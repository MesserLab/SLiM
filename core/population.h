//
//  population.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2025 Benjamin C. Haller.  All rights reserved.
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
class Haplosome;


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
	Haplosome *child_haplosome_1_;
	Haplosome *child_haplosome_2_;
	IndividualSex child_sex_;
	
	SLiM_DeferredReproduction_NonRecombinant(SLiM_DeferredReproductionType p_type,
							  Individual *p_parent1,
							  Individual *p_parent2,
							  Haplosome *p_child_haplosome_1,
							  Haplosome *p_child_haplosome_2,
							  IndividualSex p_child_sex) :
		type_(p_type), parent1_(p_parent1), parent2_(p_parent2), child_haplosome_1_(p_child_haplosome_1), child_haplosome_2_(p_child_haplosome_2), child_sex_(p_child_sex)
	{};
};

class SLiM_DeferredReproduction_Recombinant {
public:
	SLiM_DeferredReproductionType type_;
	Subpopulation *mutorigin_subpop_;
	Haplosome *child_haplosome_;
	Haplosome *strand1_;
	Haplosome *strand2_;
	std::vector<slim_position_t> break_vec_;
	
	SLiM_DeferredReproduction_Recombinant(SLiM_DeferredReproductionType p_type,
							  Subpopulation *p_mutorigin_subpop,
							  Haplosome *p_strand1,
							  Haplosome *p_strand2,
							  std::vector<slim_position_t> &p_break_vec,
							  Haplosome *p_child_haplosome) :
		type_(p_type), mutorigin_subpop_(p_mutorigin_subpop), child_haplosome_(p_child_haplosome), strand1_(p_strand1), strand2_(p_strand2)
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
	
	// Cache info for TallyMutationReferences...(), along with Chromosome::cached_tally_haplosome_count_; see those functions
	bool cached_tallies_valid_ = false;
	std::vector<Subpopulation*> last_tallied_subpops_;		// NOT OWNED POINTERS
	
public:
	
	std::map<slim_objectid_t,Subpopulation*> subpops_;		// OWNED POINTERS
	
	SLiMModelType model_type_;
	Community &community_;
	Species &species_;
	
	// Object pools for individuals and haplosomes, kept species-wide
	EidosObjectPool &species_haplosome_pool_;					// NOT OWNED; a pool out of which haplosomes are allocated, for within-species locality
	EidosObjectPool &species_individual_pool_;					// NOT OWNED; a pool out of which individuals are allocated, for within-species locality
	std::vector<Individual *> species_individuals_junkyard_;	// individuals get put here when we're done with them, for fast reuse
	
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
	bool keeping_muttype_registries_ = false;				// if true, at least one MutationType is also keeping its own registry
	bool any_muttype_call_count_used_ = false;				// if true, a muttype's muttype_registry_call_count_ has been incremented
#endif
	
#if DEFER_BROKEN
	// The "defer" flag is simply disregarded at the moment; its design has rotted away,
	// and needs to be remade anew once things have settled down.
	std::vector<SLiM_DeferredReproduction_NonRecombinant> deferred_reproduction_nonrecombinant_;
	std::vector<SLiM_DeferredReproduction_Recombinant> deferred_reproduction_recombinant_;
#endif
	
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
	// note that in the general case (e.g., addRecombinant()), the haplosomes might not belong to the parent
	bool ApplyRecombinationCallbacks(Individual *p_parent, Haplosome *p_haplosome1, Haplosome *p_haplosome2, std::vector<slim_position_t> &p_crossovers, std::vector<SLiMEidosBlock*> &p_recombination_callbacks);
	
	// generate a child haplosome from parental haplosome(s), very directly -- no null haplosomes etc., just cross/clone/recombine
	// these methods are templated with variants for speed; see also MungeIndividualCrossed() etc.
	template <const bool f_treeseq, const bool f_callbacks>
	void HaplosomeCrossed(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<SLiMEidosBlock*> *p_recombination_callbacks, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
	
	template <const bool f_treeseq, const bool f_callbacks>
	void HaplosomeCloned(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
	
	template <const bool f_treeseq, const bool f_callbacks>
	void HaplosomeRecombined(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<slim_position_t> &p_breakpoints, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
	
	void (Population::*HaplosomeCrossed_TEMPLATED)(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<SLiMEidosBlock*> *p_recombination_callbacks, std::vector<SLiMEidosBlock*> *p_mutation_callbacks) = nullptr;
	void (Population::*HaplosomeCloned_TEMPLATED)(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome, std::vector<SLiMEidosBlock*> *p_mutation_callbacks) = nullptr;
	void (Population::*HaplosomeRecombined_TEMPLATED)(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<slim_position_t> &p_breakpoints, std::vector<SLiMEidosBlock*> *p_mutation_callbacks) = nullptr;
	
	void DoHeteroduplexRepair(std::vector<slim_position_t> &p_heteroduplex, slim_position_t *p_breakpoints, int p_breakpoints_count, Haplosome *p_parent_haplosome_1, Haplosome *p_parent_haplosome_2, Haplosome *p_child_haplosome);
	
	// generate offspring within a reproduction() callback using templated Subpopulation methods; these pointers get
	// set up at the beginning of each tick's reproduction() callback stage, and should not be used outside of it
	Individual *(Subpopulation::*GenerateIndividualCrossed_TEMPLATED)(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex) = nullptr;
	Individual *(Subpopulation::*GenerateIndividualSelfed_TEMPLATED)(Individual *p_parent) = nullptr;
	Individual *(Subpopulation::*GenerateIndividualCloned_TEMPLATED)(Individual *p_parent) = nullptr;

	// An internal method that validates cached fitness values kept by Mutation objects
	void ValidateMutationFitnessCaches(void);
	
	// Recalculate all fitness values for the parental generation, including the use of mutationEffect() callbacks
	void RecalculateFitness(slim_tick_t p_tick);
	
	// Scan through all mutation runs in the simulation and unique them
	void UniqueMutationRuns(void);
	
	// Scan through all haplosomes and either split or join their mutation runs, to double or halve the number of runs per haplosome
	void SplitMutationRunsForChromosome(int32_t p_new_mutrun_count, Chromosome *p_chromosome);
	void JoinMutationRunsForChromosome(int32_t p_new_mutrun_count, Chromosome *p_chromosome);
	
	// Tally mutations and remove fixed/lost mutations
	void MaintainMutationRegistry(void);
	
	// Tally MutationRun usage and free unused MutationRuns.  Note that all of these tallying methods tally into
	// the same use_count_ counter kept by MutationRun, so a new tally wipes the results of the previous tally.
	// Also note that the mutation tallying methods below call these methods to tally mutation runs first, so
	// the mutation run tallies will be altered as a side effect of doing a mutation tally.  These methods all
	// place the number of non-null haplosomes that were tallied across into the tallied_haplosome_count_ value
	// of each chromosome involved in the tally.
	void TallyMutationRunReferencesForPopulationForChromosome(Chromosome *p_chromosome);
	void TallyMutationRunReferencesForPopulation(bool p_clock_for_mutrun_experiments);
	void TallyMutationRunReferencesForSubpopsForChromosome(std::vector<Subpopulation*> *p_subpops_to_tally, Chromosome *p_chromosome);
	void TallyMutationRunReferencesForSubpops(std::vector<Subpopulation*> *p_subpops_to_tally);
	void TallyMutationRunReferencesForHaplosomes(const Haplosome * const *haplosomes_ptr, slim_popsize_t haplosomes_count);
	void FreeUnusedMutationRuns(void);	// depends upon a previous tally by TallyMutationRunReferencesForPopulation()!
	
	// Tally Mutation usage; these count the total number of times that each Mutation in the registry is referenced
	// by a population (or a set of subpopulations, or a set of haplosomes), putting the usage counts into the refcount
	// block kept by Mutation.  For the whole population, and for a set of subpops, cache info is maintained so the
	// tally can be reused when possible.  For a set of haplosomes, the result is not cached, and so the cache is
	// always invalidated.  The maximum number of references (the total number of non-null haplosomes tallied) is
	// always placed into the tallied_haplosome_count_ value for each chromosome involved in the tally.  When
	// tallying across all subpopulations, total_haplosome_count_ for each chromosome is also set to this same
	// value, which is the maximum possible number of references (i.e. fixation), as a side effect.  The cache
	// of tallies can be invalidated by calling InvalidateMutationReferencesCache().
	void InvalidateMutationReferencesCache(void);

	void TallyMutationReferencesAcrossPopulation(bool p_clock_for_mutrun_experiments);
#ifdef SLIMGUI
	void TallyMutationReferencesAcrossPopulation_SLiMgui(void);		// tallies selected subpops into SLiMgui-private counters
#endif
	void TallyMutationReferencesAcrossSubpopulations(std::vector<Subpopulation*> *p_subpops_to_tally);
	void TallyMutationReferencesAcrossHaplosomes(const Haplosome * const *haplosomes, slim_popsize_t haplosomes_count);
	
	slim_refcount_t _CountNonNullHaplosomesForChromosome(Chromosome *p_chromosome);
	void _TallyMutationReferences_FAST_FromMutationRunUsage(bool p_clock_for_mutrun_experiments);
#if DEBUG
	void _CheckMutationTallyAcrossHaplosomes(const Haplosome * const *haplosomes_ptr, slim_popsize_t haplosomes_count, std::string caller_name);
#endif
	
	// Eidos back-end code that counts up tallied mutations, to be called after TallyMutationReferences...().
	// These methods correctly handle cases where the mutations are fixed, removed, substituted, lost, etc.,
	// to return the correct frequency/count values to the user as an EidosValue_SP.
	EidosValue_SP Eidos_FrequenciesForTalliedMutations(EidosValue *mutations_value);
	EidosValue_SP Eidos_CountsForTalliedMutations(EidosValue *mutations_value);
	
	// Handle negative fixation (remove from the registry) and positive fixation (convert to Substitution).
	// This uses reference counts from TallyMutationReferencesAcrossPopulation(), which must be called before this method.
	void RemoveAllFixedMutations(void);
	
	// check the registry for any bad entries (i.e. zombies, mutations with an incorrect state_)
	void CheckMutationRegistry(bool p_check_haplosomes);
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
	
#if SLIM_CLEAR_HAPLOSOMES
	// Clear all parental haplosomes to use nullptr for their mutation runs, so they are ready to reuse in the next tick
	void ClearParentalHaplosomes(void);
#endif
	
	//********** nonWF methods

	// remove subpopulation p_subpop_id from the model entirely
	void RemoveSubpopulation(Subpopulation &p_subpop);
	
	// move individuals as requested by survival() callbacks
	void ResolveSurvivalPhaseMovement(void);
	
	// checks for deferred haplosomes in queue right now; allows optimization when none are present
#if DEFER_BROKEN
	inline bool HasDeferredHaplosomes(void) { return ((deferred_reproduction_nonrecombinant_.size() > 0) || (deferred_reproduction_recombinant_.size() > 0)); }
	void CheckForDeferralInHaplosomesVector(Haplosome **p_haplosomes, size_t p_elements_size, const std::string &p_caller);
	void CheckForDeferralInHaplosomes(EidosValue_Object *p_haplosomes, const std::string &p_caller);
	void CheckForDeferralInIndividualsVector(Individual * const *p_individuals, size_t p_elements_size, const std::string &p_caller);
#else
	inline bool HasDeferredHaplosomes(void) { return false; }
	inline void CheckForDeferralInHaplosomesVector(__attribute__ ((unused)) Haplosome **p_haplosomes, __attribute__ ((unused)) size_t p_elements_size, __attribute__ ((unused)) const std::string &p_caller) {}
	inline void CheckForDeferralInHaplosomes(__attribute__ ((unused)) EidosValue_Object *p_haplosomes, __attribute__ ((unused)) const std::string &p_caller) {}
	inline void CheckForDeferralInIndividualsVector(__attribute__ ((unused)) Individual * const *p_individuals, __attribute__ ((unused)) size_t p_elements_size, __attribute__ ((unused)) const std::string &p_caller) {}
#endif
	
#if DEFER_BROKEN
	void DoDeferredReproduction(void);
#endif
	
	//********** methods for all models
	
	void PurgeRemovedSubpopulations(void);

	// print all mutations and all haplosomes to a stream
	// note that text output is now at Individual::PrintIndividuals_SLiM()
	void PrintAllBinary(std::ostream &p_out, bool p_output_spatial_positions, bool p_output_ages, bool p_output_ancestral_nucs, bool p_output_pedigree_ids, bool p_output_object_tags, bool p_output_substitutions) const;
	
	// print sample of p_sample_size haplosomes from subpopulation p_subpop_id, using SLiM's own format
	void PrintSample_SLiM(std::ostream &p_out, Subpopulation &p_subpop, slim_popsize_t p_sample_size, bool p_replace, IndividualSex p_requested_sex, const Chromosome &p_chromosome) const;
	
	// print sample of p_sample_size haplosomes from subpopulation p_subpop_id, using "ms" format
	void PrintSample_MS(std::ostream &p_out, Subpopulation &p_subpop, slim_popsize_t p_sample_size, bool p_replace, IndividualSex p_requested_sex, const Chromosome &p_chromosome, bool p_filter_monomorphic) const;
	
	// print sample of p_sample_size haplosomes from subpopulation p_subpop_id, using "vcf" format
	void PrintSample_VCF(std::ostream &p_out, Subpopulation &p_subpop, slim_popsize_t p_sample_size, bool p_replace, IndividualSex p_requested_sex, const Chromosome &p_chromosome, bool p_output_multiallelics, bool p_simplify_nucs, bool p_output_nonnucs, bool p_group_as_individuals) const;
	
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




































































