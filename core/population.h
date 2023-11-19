//
//  population.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2023 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.

/*
 
 The class Population represents the entire simulated population as a map of one or more subpopulations.  This class is where much
 of the simulation logic resides; the population is called to put events into effect, to evolve, and so forth.
 
 */

#ifndef __SLiM__population__
#define __SLiM__population__


#include <vector>
#include <map>
#include <string>
#include <unordered_map>

#include "slim_globals.h"
#include "substitution.h"
#include "chromosome.h"
#include "slim_eidos_block.h"
#include "mutation_run.h"


class Community;
class Species;
class Subpopulation;
class Individual;
class Genome;


#pragma mark -
#pragma mark Deferred reproduction (mainly for multithreading)
#pragma mark -

typedef enum class SLiM_DeferredReproductionType : uint8_t {
	kCrossoverMutation = 0,
	kClonal,
	kSelfed,
	kRecombinant
} SLiM_DeferredReproductionType;

class SLiM_DeferredReproduction_NonRecombinant {
public:
	SLiM_DeferredReproductionType type_;
	Individual *parent1_;
	Individual *parent2_;
	Genome *child_genome_1_;
	Genome *child_genome_2_;
	IndividualSex child_sex_;
	
	SLiM_DeferredReproduction_NonRecombinant(SLiM_DeferredReproductionType p_type,
							  Individual *p_parent1,
							  Individual *p_parent2,
							  Genome *p_child_genome_1,
							  Genome *p_child_genome_2,
							  IndividualSex p_child_sex) :
		type_(p_type), parent1_(p_parent1), parent2_(p_parent2), child_genome_1_(p_child_genome_1), child_genome_2_(p_child_genome_2), child_sex_(p_child_sex)
	{};
};

class SLiM_DeferredReproduction_Recombinant {
public:
	SLiM_DeferredReproductionType type_;
	Subpopulation *mutorigin_subpop_;
	Genome *child_genome_;
	Genome *strand1_;
	Genome *strand2_;
	std::vector<slim_position_t> break_vec_;
	IndividualSex sex_;
	
	SLiM_DeferredReproduction_Recombinant(SLiM_DeferredReproductionType p_type,
							  Subpopulation *p_mutorigin_subpop,
							  Genome *p_strand1,
							  Genome *p_strand2,
							  std::vector<slim_position_t> &p_break_vec,
							  Genome *p_child_genome,
							  IndividualSex p_sex) :
		type_(p_type), mutorigin_subpop_(p_mutorigin_subpop), child_genome_(p_child_genome), strand1_(p_strand1), strand2_(p_strand2), sex_(p_sex)
	{
		std::swap(break_vec_, p_break_vec);		// take ownership of the passed vector with std::swap(), to avoid copying
	};
};


#ifdef SLIMGUI
// This struct is used to hold fitness values observed during a run, for display by GraphView_FitnessOverTime
// The Population keeps the fitness histories for all the subpopulations, because subpops can come and go, but
// we want to remember their histories and display them even after they're gone.
typedef struct FitnessHistory {
	double *history_ = nullptr;						// mean fitness, recorded per tick; tick 1 goes at index 0
	slim_tick_t history_length_ = 0;				// the number of entries in the history_ buffer
} FitnessHistory;

// This struct similarly holds observed subpopulation sizes observed during a run, for QtSLiMGraphView_PopSizeOverTime
typedef struct SubpopSizeHistory {
    slim_popsize_t *history_ = nullptr;             // subpop size, recorded per tick; tick 1 goes at index 0
	slim_tick_t history_length_ = 0;				// the number of entries in the history_ buffer
} SubpopSizeHistory;
#endif


class Population
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

	MutationRun mutation_registry_;							// OWNED POINTERS: a registry of all mutations that have been added to this population
	bool registry_needs_consistency_check_ = false;			// set this to run CheckMutationRegistry() at the end of the cycle
	
	// Cache info for TallyMutationReferences...(); see those functions
	std::vector<Subpopulation*> last_tallied_subpops_;		// NOT OWNED POINTERS
	slim_refcount_t cached_tally_genome_count_ = 0;			// a value of 0 indicates that the cache is invalid
	
public:
	
	std::map<slim_objectid_t,Subpopulation*> subpops_;		// OWNED POINTERS
	
	SLiMModelType model_type_;
	Community &community_;
	Species &species_;
	
	// Object pools for individuals and genomes, kept population-wide
	EidosObjectPool species_genome_pool_;					// a pool out of which genomes are allocated, for within-species locality of memory usage across genomes
	EidosObjectPool species_individual_pool_;				// a pool out of which individuals are allocated, for within-species locality of memory usage across individuals
	std::vector<Genome *> species_genome_junkyard_nonnull;	// non-null genomes get put here when we're done with them, so we can reuse them without dealloc/realloc of their mutrun buffers
	std::vector<Genome *> species_genome_junkyard_null;		// null genomes get put here when we're done with them, so we can reuse them without dealloc/realloc of their mutrun buffers
	
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
	bool keeping_muttype_registries_ = false;				// if true, at least one MutationType is also keeping its own registry
	bool any_muttype_call_count_used_ = false;				// if true, a muttype's muttype_registry_call_count_ has been incremented
#endif
	
	slim_refcount_t total_genome_count_ = 0;				// the number of non-null genomes in the population; a fixed mutation has this count
#ifdef SLIMGUI
	slim_refcount_t gui_total_genome_count_ = 0;			// the number of non-null genomes in the selected subpopulations in SLiMgui
#endif
	
	std::vector<SLiM_DeferredReproduction_NonRecombinant> deferred_reproduction_nonrecombinant_;
	std::vector<SLiM_DeferredReproduction_Recombinant> deferred_reproduction_recombinant_;
	
	std::vector<Substitution*> substitutions_;				// OWNED POINTERS: Substitution objects for all fixed mutations
	std::unordered_multimap<slim_position_t, Substitution*> treeseq_substitutions_map_;	// TREE SEQUENCE RECORDING; keeps all fixed mutations, hashed by position

	bool child_generation_valid_ = false;					// WF only: this keeps track of whether children have been generated by EvolveSubpopulation() yet, or whether the parents are still in charge
	
	std::vector<Subpopulation*> removed_subpops_;			// OWNED POINTERS: Subpops which are set to size 0 (and thus removed) are kept here until the end of the cycle
	
#ifdef SLIMGUI
	// information-gathering for various graphs in SLiMgui
	slim_tick_t *mutation_loss_times_ = nullptr;			// histogram bins: {1 bin per mutation-type} for 10 ticks, realloced outward to add new tick bins as needed
	uint32_t mutation_loss_tick_slots_ = 0;					// the number of tick-sized slots (with bins per mutation-type) presently allocated
	
	slim_tick_t *mutation_fixation_times_ = nullptr;		// histogram bins: {1 bin per mutation-type} for 10 ticks, realloced outward to add new tick bins as needed
	uint32_t mutation_fixation_tick_slots_ = 0;				// the number of tick-sized slots (with bins per mutation-type) presently allocated
	
	std::map<slim_objectid_t,FitnessHistory> fitness_histories_;	// fitness histories indexed by subpopulation id (or by -1, for the Population history)
    std::map<slim_objectid_t,SubpopSizeHistory> subpop_size_histories_;	// size histories indexed by subpopulation id (or by -1, for the Population history)
#endif
	
	Population(const Population&) = delete;					// no copying
	Population& operator=(const Population&) = delete;		// no copying
	Population(void) = delete;								// no default constructor
	explicit Population(Species &p_species);			// our constructor: we must have a reference to our species, from which we get our community
	~Population(void);										// destructor
	
	// add new empty subpopulation p_subpop_id of size p_subpop_size
	Subpopulation *AddSubpopulation(slim_objectid_t p_subpop_id, slim_popsize_t p_subpop_size, double p_initial_sex_ratio, bool p_haploid);
	
	// Mutation registry access
	inline const MutationIndex *MutationRegistry(int *registry_count)
	{
		*registry_count = mutation_registry_.size();
		return mutation_registry_.begin_pointer_const();
	}
	
	inline void MutationRegistryAdd(Mutation *p_mutation)
	{
#if DEBUG
		if ((p_mutation->state_ == MutationState::kInRegistry) ||
			(p_mutation->state_ == MutationState::kRemovedWithSubstitution) ||
			(p_mutation->state_ == MutationState::kFixedAndSubstituted))
			EIDOS_TERMINATION << "ERROR (Population::MutationRegistryAdd): " << "(internal error) cannot add a mutation to the registry that is already in the registry, or has been fixed/substituted." << EidosTerminate();
#endif
		
		// We could be adding a lost mutation back into the registry (from a mutation() callback), in which case it gets a retain
		// New mutations already have a retain count of 1, which we use (i.e., we take ownership of the mutation passed in to us)
		if (p_mutation->state_ != MutationState::kNewMutation)
			p_mutation->Retain();
		
		MutationIndex new_mut_index = p_mutation->BlockIndex();
		mutation_registry_.emplace_back(new_mut_index);
		
		p_mutation->state_ = MutationState::kInRegistry;
		
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
		if (keeping_muttype_registries_)
		{
			MutationType *mutation_type_ptr = p_mutation->mutation_type_ptr_;
			
			if (mutation_type_ptr->keeping_muttype_registry_)
			{
				// This mutation type is also keeping its own private registry, so we need to add to that as well
				mutation_type_ptr->muttype_registry_.emplace_back(new_mut_index);
			}
		}
#endif
	}
	
	// apply modifyChild() callbacks to a generated child; a return of false means "do not use this child, generate a new one"
	bool ApplyModifyChildCallbacks(Individual *p_child, Individual *p_parent1, Individual *p_parent2, bool p_is_selfing, bool p_is_cloning, Subpopulation *p_target_subpop, Subpopulation *p_source_subpop, std::vector<SLiMEidosBlock*> &p_modify_child_callbacks);
	
	// apply recombination() callbacks to a generated child; a return of true means the breakpoints were changed
	bool ApplyRecombinationCallbacks(slim_popsize_t p_parent_index, Genome *p_genome1, Genome *p_genome2, Subpopulation *p_source_subpop, std::vector<slim_position_t> &p_crossovers, std::vector<SLiMEidosBlock*> &p_recombination_callbacks);
	
	// generate a child genome from parental genomes, with recombination, gene conversion, and mutation
	void DoCrossoverMutation(Subpopulation *p_source_subpop, Genome &p_child_genome, slim_popsize_t p_parent_index, IndividualSex p_child_sex, IndividualSex p_parent_sex, std::vector<SLiMEidosBlock*> *p_recombination_callbacks, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
	void DoHeteroduplexRepair(std::vector<slim_position_t> &p_heteroduplex, std::vector<slim_position_t> &p_breakpoints, Genome *p_parent_genome_1, Genome *p_parent_genome_2, Genome *p_child_genome);
	
	// generate a child genome from parental genomes, with predetermined recombination and mutation
	void DoRecombinantMutation(Subpopulation *p_mutorigin_subpop, Genome &p_child_genome, Genome *p_parent_genome_1, Genome *p_parent_genome_2, IndividualSex p_parent_sex, std::vector<slim_position_t> &p_breakpoints, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
	
	// generate a child genome from a single parental genome, without recombination or gene conversion, but with mutation
	void DoClonalMutation(Subpopulation *p_mutorigin_subpop, Genome &p_child_genome, Genome &p_parent_genome, IndividualSex p_child_sex, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
	
	// An internal method that validates cached fitness values kept by Mutation objects
	void ValidateMutationFitnessCaches(void);
	
	// Recalculate all fitness values for the parental generation, including the use of mutationEffect() callbacks
	void RecalculateFitness(slim_tick_t p_tick);
	
	// Scan through all mutation runs in the simulation and unique them
	void UniqueMutationRuns(void);
	
	// Scan through all genomes and either split or join their mutation runs, to double or halve the number of runs per genome
	void SplitMutationRuns(int32_t p_new_mutrun_count);
	void JoinMutationRuns(int32_t p_new_mutrun_count);
	
	// Tally mutations and remove fixed/lost mutations
	void MaintainMutationRegistry(void);
	
	// Tally MutationRun usage and free unused MutationRuns.  Note that all of these tallying methods tally into
	// the same use_count_ counter kept by MutationRun, so a new tally wipes the results of the previous tally.
	// Also note that the mutation tallying methods below call these methods to tally mutation runs first, so
	// the mutation run tallies will be altered as a side effect of doing a mutation tally.  The return value
	// for all of these methods is the number of non-null genomes that were tallied across.
	slim_refcount_t TallyMutationRunReferencesForPopulation(void);
	slim_refcount_t TallyMutationRunReferencesForSubpops(std::vector<Subpopulation*> *p_subpops_to_tally);
	slim_refcount_t TallyMutationRunReferencesForGenomes(std::vector<Genome*> *p_genomes_to_tally);
	void FreeUnusedMutationRuns(void);	// depends upon a previous tally by TallyMutationRunReferencesForPopulation()!
	
	// Tally Mutation usage; these count the total number of times that each Mutation in the registry is referenced
	// by a population (or a set of subpopulations, or a set of genomes), putting the usage counts into the refcount
	// block kept by Mutation.  For the whole population, and for a set of subpops, cache info is maintained so the
	// tally can be reused when possible.  For a set of genomes, the result is not cached, and so the cache is
	// always invalidated.  The maximum number of references (the total number of non-null genomes tallied) is
	// always returned.  When tallying across all subpopulations, total_genome_count_ is also set to this same
	// value, which is the maximum possible number of references (i.e. fixation), as a side effect.  The cache
	// of tallies can be invalidated by calling InvalidateMutationReferencesCache().
	inline void InvalidateMutationReferencesCache(void) { last_tallied_subpops_.clear(); cached_tally_genome_count_ = 0; }
	
	slim_refcount_t TallyMutationReferencesAcrossPopulation(bool p_force_recache);
	slim_refcount_t TallyMutationReferencesAcrossSubpopulations(std::vector<Subpopulation*> *p_subpops_to_tally, bool p_force_recache);
	slim_refcount_t TallyMutationReferencesAcrossGenomes(std::vector<Genome*> *p_genomes_to_tally);
	
	slim_refcount_t _CountNonNullGenomes(void);
#ifdef SLIMGUI
	void _CopyRefcountsToSLiMgui(void);
#endif
	void _TallyMutationReferences_FAST_FromMutationRunUsage(void);
#if DEBUG
	void _CheckMutationTallyAcrossGenomes(std::vector<Genome*> &p_genomes);
#endif
	
	// Eidos back-end code that counts up tallied mutations, to be called after TallyMutationReferences...().
	// These methods correctly handle cases where the mutations are fixed, removed, substituted, lost, etc.,
	// to return the correct frequency/count values to the user as an EidosValue_SP.
	EidosValue_SP Eidos_FrequenciesForTalliedMutations(EidosValue *mutations_value, int total_genome_count);
	EidosValue_SP Eidos_CountsForTalliedMutations(EidosValue *mutations_value, int total_genome_count);
	
	// Handle negative fixation (remove from the registry) and positive fixation (convert to Substitution).
	// This uses reference counts from TallyMutationReferencesAcrossPopulation(), which must be called before this method.
	void RemoveAllFixedMutations(void);
	
	// check the registry for any bad entries (i.e. zombies, mutations with an incorrect state_)
	void CheckMutationRegistry(bool p_check_genomes);
	inline void SetMutationRegistryNeedsCheck(void) { registry_needs_consistency_check_ = true; }
	inline bool MutationRegistryNeedsCheck(void) { return registry_needs_consistency_check_; }
	
	// assess usage patterns of mutation runs across the simulation
	void AssessMutationRuns(void);
	
	//********** WF methods
	
	// add new subpopulation p_subpop_id of size p_subpop_size individuals drawn from source subpopulation p_source_subpop_id
	Subpopulation *AddSubpopulationSplit(slim_objectid_t p_subpop_id, Subpopulation &p_source_subpop, slim_popsize_t p_subpop_size, double p_initial_sex_ratio);
	
	// set size of subpopulation p_subpop_id to p_subpop_size
	void SetSize(Subpopulation &p_subpop, slim_popsize_t p_subpop_size);
	
	// set fraction p_migrant_fraction of p_subpop_id that originates as migrants from p_source_subpop_id per cycle  
	void SetMigration(Subpopulation &p_subpop, slim_objectid_t p_source_subpop_id, double p_migrant_fraction);
	
	// apply mateChoice() callbacks to a mating event with a chosen first parent; the return is the second parent index, or -1 to force a redraw
	slim_popsize_t ApplyMateChoiceCallbacks(slim_popsize_t p_parent1_index, Subpopulation *p_subpop, Subpopulation *p_source_subpop, std::vector<SLiMEidosBlock*> &p_mate_choice_callbacks);
	
	// generate children for subpopulation p_subpop_id, drawing from all source populations, handling crossover and mutation
	void EvolveSubpopulation(Subpopulation &p_subpop, bool p_mate_choice_callbacks_present, bool p_modify_child_callbacks_present, bool p_recombination_callbacks_present, bool p_mutation_callbacks_present, bool p_type_s_dfe_present);
	
	// step forward a generation: make the children become the parents
	void SwapGenerations(void);
	
	// Clear all parental genomes to use nullptr for their mutation runs, so they are ready to reuse in the next tick
	void ClearParentalGenomes(void);
	
	//********** nonWF methods

	// remove subpopulation p_subpop_id from the model entirely
	void RemoveSubpopulation(Subpopulation &p_subpop);
	
	// move individuals as requested by survival() callbacks
	void ResolveSurvivalPhaseMovement(void);
	
	// checks for deferred genomes in queue right now; allows optimization when none are present
	inline bool HasDeferredGenomes(void) { return ((deferred_reproduction_nonrecombinant_.size() > 0) || (deferred_reproduction_recombinant_.size() > 0)); }
	void CheckForDeferralInGenomesVector(Genome **p_genomes, size_t p_elements_size, const std::string &p_caller);
	void CheckForDeferralInGenomes(EidosValue_Object *p_genomes, const std::string &p_caller);
	void CheckForDeferralInIndividualsVector(Individual **p_individuals, size_t p_elements_size, const std::string &p_caller);
	
	void DoDeferredReproduction(void);
	
	//********** methods for all models
	
	void PurgeRemovedSubpopulations(void);

	// print all mutations and all genomes to a stream
	void PrintAll(std::ostream &p_out, bool p_output_spatial_positions, bool p_output_ages, bool p_output_ancestral_nucs, bool p_output_pedigree_ids) const;
	void PrintAllBinary(std::ostream &p_out, bool p_output_spatial_positions, bool p_output_ages, bool p_output_ancestral_nucs, bool p_output_pedigree_ids) const;
	
	// print sample of p_sample_size genomes from subpopulation p_subpop_id, using SLiM's own format
	void PrintSample_SLiM(std::ostream &p_out, Subpopulation &p_subpop, slim_popsize_t p_sample_size, bool p_replace, IndividualSex p_requested_sex) const;
	
	// print sample of p_sample_size genomes from subpopulation p_subpop_id, using "ms" format
	void PrintSample_MS(std::ostream &p_out, Subpopulation &p_subpop, slim_popsize_t p_sample_size, bool p_replace, IndividualSex p_requested_sex, const Chromosome &p_chromosome, bool p_filter_monomorphic) const;
	
	// print sample of p_sample_size genomes from subpopulation p_subpop_id, using "vcf" format
	void PrintSample_VCF(std::ostream &p_out, Subpopulation &p_subpop, slim_popsize_t p_sample_size, bool p_replace, IndividualSex p_requested_sex, bool p_output_multiallelics, bool p_simplify_nucs, bool p_output_nonnucs) const;
	
	// remove subpopulations, purge all mutations and substitutions, etc.; called before InitializePopulationFrom[Text|Binary]File()
	void RemoveAllSubpopulationInfo(void);
	
	// additional methods for SLiMgui, for information-gathering support
#ifdef SLIMGUI
	void RecordFitness(slim_tick_t p_history_index, slim_objectid_t p_subpop_id, double p_fitness_value);
    void RecordSubpopSize(slim_tick_t p_history_index, slim_objectid_t p_subpop_id, slim_popsize_t p_subpop_size);
	void SurveyPopulation(void);
	void AddTallyForMutationTypeAndBinNumber(int p_mutation_type_index, int p_mutation_type_count, slim_tick_t p_bin_number, slim_tick_t **p_buffer, uint32_t *p_bufferBins);
#endif
};


#endif /* defined(__SLiM__population__) */




































































