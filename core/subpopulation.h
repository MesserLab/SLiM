//
//  subpopulation.h
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
 
 The class Subpopulation represents one simulated subpopulation, defined primarily by the genomes of the individuals it contains.
 Since one Genome object represents the mutations along one chromosome, and since SLiM presently simulates diploid individuals,
 each individual is represented by two genomes in the genome vector: individual i is represented by genomes 2*i and 2*i+1.
 A subpopulation also knows its size, its selfing fraction, and what fraction it receives as migrants from other subpopulations.
 
 The way that Subpopulation handles its Genome objects is quite complex, and is at the heart of the operation of the SLiM core,
 so a lengthy comment on it is merited.  Genomes contain MutationRuns, and they can allocate a buffer in which they keep pointers
 to those runs.  Allocating and deallocating those buffers takes time, and so is to be avoided; for this reason, Subpopulation
 re-uses Genome objects.  When a Genome is done being used, it goes into a "junkyard" vector (which one depends on whether it is
 a null genome or not); FreeSubpopGenome() handles that.  When a new genome is needed, it is preferentially obtained from the
 appropriate junkyard, and is re-purposed to its new use; NewSubpopGenome() handles that.  Those methods should be called to
 handle most create/dispose requests for Genomes, so that those junkyard vectors get used.  When NewSubpopGenome() finds the
 junkyard to be empty, it needs to actually allocate a new Genome object.  This is also complicated.  Genomes are allocated out
 of a memory pool, genome_pool_, that is specific to the subpopulation.  This helps with memory locality; it keeps all of the
 Genome objects used by a given subpop grouped closely together in memory.  (We do the same with Individual objects, with another
 pool, for the same reason).  So allocations of Genomes come out of that pool, and deallocations go back into the pool.  Do not
 use new or delete with Genome objects.  There is one more complication.  In WF models, separate "parent" and "child" generations
 are kept by the subpop, and which one is active switches back and forth in each cycle, governed by child_generation_valid_.
 In nonWF models, the child generation variables are all unused; in nonWF models, the parental generation is always active.
 New offspring instead get put into the nonWF_offspring_ ivars, which get moved into the parental generation ivars at the end of
 offspring generation.  So in some ways these schemes are similar, but they use a completely non-intersecting set of ivars.  The
 parental ivars are used by both WF and nonWF models, though.
 
 BCH 23 May 2021: The above description is now somewhat out of date; adding a new comment rather than just revising in order to
 keep a record of what has happened.  The separate genome/individual pools for each subpopulation proved to be a maintenance
 nightmare, because of migrants; a migrating individual and its genome could not simply be moved to the new subpop, because that
 subpop used different object pools, so the object states instead had to be re-created completely in new objects allocated out
 of the new subpop's object pools.  Since the pointer referring to those objects was then actually changing, all references to
 the objects needed to be patched with the new pointers, including inside Eidos objects.  This was slow, and very prone to
 difficult-to-find bugs.  On balance, it was a bad idea, and is being ripped out now.  Instead, Population will keep the object
 pools for the whole species.  For both efficiency and ease of implementation, Subpopulation will keep its own pointers to the
 object pools and junkyard vectors, but these really belong to Species and are shared now.
 
 */

#ifndef __SLiM__subpopulation__
#define __SLiM__subpopulation__


#include "slim_globals.h"
#include "eidos_rng.h"
#include "genome.h"
#include "chromosome.h"
#include "eidos_value.h"
#include "slim_eidos_block.h"
#include "individual.h"
#include "population.h"
#include "species.h"
#include "spatial_map.h"

#include <vector>
#include <map>
#include <limits.h>


class Population;


extern EidosClass *gSLiM_Subpopulation_Class;

typedef std::pair<std::string, SpatialMap *> SpatialMapPair;
typedef std::map<std::string, SpatialMap *> SpatialMapMap;


#pragma mark -
#pragma mark Subpopulation
#pragma mark -

class Subpopulation : public EidosDictionaryUnretained
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	typedef EidosDictionaryUnretained super;

private:
	
	// WF only:
	gsl_ran_discrete_t *lookup_parent_ = nullptr;			// OWNED POINTER: lookup table for drawing a parent based upon fitness
	gsl_ran_discrete_t *lookup_female_parent_ = nullptr;	// OWNED POINTER: lookup table for drawing a female parent based upon fitness, SEX ONLY
	gsl_ran_discrete_t *lookup_male_parent_ = nullptr;		// OWNED POINTER: lookup table for drawing a male parent based upon fitness, SEX ONLY
	
	EidosSymbolTableEntry self_symbol_;						// for fast setup of the symbol table
	
public:
	
	Community &community_;
	Species &species_;
	Population &population_;
	SLiMModelType model_type_;
	
	slim_objectid_t subpopulation_id_;				// the id by which this subpopulation is indexed in the Population
	EidosValue_SP cached_value_subpop_id_;			// a cached value for subpopulation_id_; reset() if changed
	bool has_been_removed_ = false;					// a flag set to true when the subpop is removed and should no longer be used
	
	std::string name_;								// the `name` property; "p1", "p2", etc. by default, following the Eidos self-symbol
	std::string description_;						// the `description` property; the empty string by default
	
	// WF only:
	double selfing_fraction_ = 0.0;					// ASEX ONLY: selfing fraction, the fraction of offspring generated by self-fertilization
	double female_clone_fraction_ = 0.0;			// both asex and sex; in the asex case, male_clone_fraction_ == female_clone_fraction_ and is simply the clonal fraction
	double male_clone_fraction_ = 0.0;				//		these represent the fraction of offspring generated by asexual clonal reproduction
	std::map<slim_objectid_t,double> migrant_fractions_;		// m[i]: fraction made up of migrants from subpopulation i per cycle
	
	// These object pools are owned by Population, and are used by all of its Subpopulations; we have references to them just for efficiency
	EidosObjectPool &genome_pool_;					// NOT OWNED: a pool out of which genomes are allocated, for within-species locality of memory usage across genomes
	EidosObjectPool &individual_pool_;				// NOT OWNED: a pool out of which individuals are allocated, for within-species locality of memory usage across individuals
	std::vector<Genome *> &genome_junkyard_nonnull;	// NOT OWNED: non-null genomes get put here when we're done with them, so we can reuse them without dealloc/realloc of their mutrun buffers
	std::vector<Genome *> &genome_junkyard_null;	// NOT OWNED: null genomes get put here when we're done with them, so we can reuse them without dealloc/realloc of their mutrun buffers
	bool has_null_genomes_ = false;					// false until addRecombinant() etc. generates a null genome and sets it to true; NOT set by null genomes for sex chromosome sims; used for optimizations
	
	std::vector<Genome *> parent_genomes_;			// OWNED: all genomes in the parental generation; each individual gets two genomes, males are XY (not YX)
	EidosValue_SP cached_parent_genomes_value_;		// cached for the genomes property; reset() if changed
	slim_popsize_t parent_subpop_size_;				// parental subpopulation size
	slim_popsize_t parent_first_male_index_ = INT_MAX;	// the index of the first male in the parental Genome vector (NOT premultiplied by 2!); equal to the number of females
	std::vector<Individual *> parent_individuals_;	// OWNED: objects representing simulated individuals, each of which has two genomes
	EidosValue_SP cached_parent_individuals_value_;	// cached for the individuals property; self-maintains
	double parent_sex_ratio_ = 0.0;					// WF only: what sex ratio the parental genomes approximate (M:M+F)
	
	// WF only:
	// In WF models, we actually switch to a "child" generation just after offspring generation; this is then the active generation.
	// Then, with SwapChildAndParentGenomes(), the child generation gets swapped into the parent generation, which becomes active.
	// I'm not sure this is a great design really, but it's pretty entrenched at this point, and pretty harmless...
	bool child_generation_valid_ = false;			// this keeps track of whether children have been generated by EvolveSubpopulation() yet, or whether the parents are still in charge
	std::vector<Genome *> child_genomes_;			// OWNED: all genomes in the child generation; each individual gets two genomes, males are XY (not YX)
	EidosValue_SP cached_child_genomes_value_;		// cached for the genomes property; reset() if changed
	slim_popsize_t child_subpop_size_;				// child subpopulation size
	slim_popsize_t child_first_male_index_ = INT_MAX;	// the index of the first male in the child Genome vector (NOT premultiplied by 2!); equal to the number of females
	std::vector<Individual *> child_individuals_;	// OWNED: objects representing simulated individuals, each of which has two genomes
	EidosValue_SP cached_child_individuals_value_;	// cached for the individuals property; self-maintains
	double child_sex_ratio_ = 0.0;					// what sex ratio the child genomes approximate (M:M+F)
	
	// nonWF only:
	// In nonWF models, we place generated offspring into a temporary holding pen, but it is never made the "active generation"
	// the way it is in WF models.  At soon as offspring generation is finished, these individuals get merged back in.  The
	// individuals here are kept in the order in which they were generated, not in order by sex or anything else.
	std::vector<Genome *> nonWF_offspring_genomes_;
	std::vector<Individual *> nonWF_offspring_individuals_;
	
	// nonWF only:
	// In nonWF models, survival() callbacks can move individuals to new subpopulations.  These individuals are kept in a
	// temporary holding pen, similar to how newly generated individuals are kept.  They are moved after all survival()
	// callbacks have finished.  The difference, compared to reproduction, is that the individuals in this holding pen
	// already belong to a subpopulation; they are not removed until they get moved to their new destination.
	std::vector<Individual *> nonWF_survival_moved_individuals_;
	
	// the lifetime reproductive output of all individuals that died in the last mortality event; cleared each cycle
	std::vector<int32_t> lifetime_reproductive_output_MH_;		// males / hermaphrodites
	std::vector<int32_t> lifetime_reproductive_output_F_;		// females
	
	std::vector<SLiMEidosBlock*> registered_mate_choice_callbacks_;		// WF only; NOT OWNED: valid only during EvolveSubpopulation; callbacks used when this subpop is parental
	std::vector<SLiMEidosBlock*> registered_modify_child_callbacks_;	// NOT OWNED: valid only during EvolveSubpopulation; callbacks used when this subpop is parental
	std::vector<SLiMEidosBlock*> registered_recombination_callbacks_;	// NOT OWNED: valid only during EvolveSubpopulation; callbacks used when this subpop is parental
	std::vector<SLiMEidosBlock*> registered_mutation_callbacks_;		// NOT OWNED: valid only during EvolveSubpopulation; callbacks used when this subpop is parental
	std::vector<SLiMEidosBlock*> registered_reproduction_callbacks_;	// nonWF only; NOT OWNED: valid only during EvolveSubpopulation; callbacks used when this subpop is parental
	
	// WF only:
	// Fitness caching.  Every individual now caches its fitness internally, and that is what is used by SLiMgui and by the cachedFitness() method of Subpopulation.
	// These fitness cache buffers are additional to that, used only in WF models.  They are used for two things.  First, as the data source for setting up our lookup
	// objects for drawing mates by fitness; the GSL wants that data to be in the form of a single buffer.  And second, by mateChoice() callbacks, which throw around
	// vectors of weights, and want to have default weight vectors for the non-sex and sex cases.  In nonWF models these buffers are not used, and not even set up.
	// In WF models we could continue to use these buffers for all uses (i.e., SLiMgui and cachedFitness()), but to keep the code simple it seems better to use the
	// caches in Individual for both WF and nonWF models where possible.
	double *cached_parental_fitness_ = nullptr;		// OWNED POINTER: cached in UpdateFitness()
	double *cached_male_fitness_ = nullptr;			// OWNED POINTER: SEX ONLY: same as cached_parental_fitness_ but with 0 for all females
	slim_popsize_t cached_fitness_size_ = 0;		// the size (number of entries used) of cached_parental_fitness_ and cached_male_fitness_
	slim_popsize_t cached_fitness_capacity_ = 0;	// the capacity of the malloced buffers cached_parental_fitness_ and cached_male_fitness_
	
	// WF only:
	// Optimized fitness caching at the Individual level.  Individual has an ivar named cached_fitness_UNSAFE_ that keeps a cached fitness value for each individual.
	// When a model is neutral or nearly neutral, every individual may have the same fitness value, and we may know that.  In such cases, we want to avoid setting
	// up the cached fitness value in each individual; instead, we set a flag here that overrides cached_fitness_UNSAFE_ and says it has the same value for all.
	// This happens only for the parental generation's cached fitness values (cached fitness values for the child generation should never be accessed by anyone
	// since they are not valid).  And it happens only in WF models, since in nonWF models the generational overlap makes this scheme impossible.  A somewhat special
	// case, then, but it seems worthwhile since the penalty for seting every cached fitness value, and then gathering them back up again in UpdateWFFitnessBuffers(),
	// is large – almost 20% of total runtime for the full Gravel model, for example.  Neutral WF models are common and worth special-casing.
	bool individual_cached_fitness_OVERRIDE_ = false;
	double individual_cached_fitness_OVERRIDE_value_;
	
	// SEX ONLY; the default values here are for the non-sex case
	bool sex_enabled_ = false;										// the subpopulation needs to have easy reference to whether its individuals are sexual or not...
	GenomeType modeled_chromosome_type_ = GenomeType::kAutosome;	// ...and needs to know what type of chromosomes its individuals are modeling; this should match Species
	
	// continuous-space info
	double bounds_x0_ = 0.0, bounds_x1_ = 1.0;
	double bounds_y0_ = 0.0, bounds_y1_ = 1.0;
	double bounds_z0_ = 0.0, bounds_z1_ = 1.0;
	SpatialMapMap spatial_maps_;						// the SpatialMap objects in this are retained!
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;	// a user-defined tag value
	
	double subpop_fitness_scaling_ = 1.0;				// the fitnessScaling property value
	
#ifdef SLIMGUI
	bool gui_selected_ = false;							// keeps track of whether we are selected in SLiMgui's table of subpopulations
	double parental_mean_unscaled_fitness_ = 0.0;		// updated in SurveyPopulation() when running under SLiMgui
	
	bool gui_center_from_user_ = false;					// if this flag is true, the center below comes from the user and should not be modified
	double gui_center_x_, gui_center_y_;				// the center used by GraphView_PopulationVisualization
	
	bool gui_radius_scaling_from_user_ = false;			// if this flag is true, the radius scaling below comes from the user and should not be modified
	double gui_radius_scaling_;							// set/used only when gui_display_from_user_ is true; a scaling factor for the circle size
	double gui_radius_;									// the radius, post-scaling, used by GraphView_PopulationVisualization
	
	bool gui_color_from_user_ = false;					// if this flag is true, the color below comes from the user and should not be modified
	float gui_color_red_, gui_color_green_, gui_color_blue_;	// the color components, used by GraphView_PopulationVisualization
#endif
	
#if defined(SLIMGUI)
	// nonWF only:
	// these counters track generated offspring of different types, in nonWF models
	int64_t gui_offspring_cloned_M_ = 0;
	int64_t gui_offspring_cloned_F_ = 0;
	int64_t gui_offspring_selfed_ = 0;
	int64_t gui_offspring_crossed_ = 0;
	int64_t gui_offspring_empty_ = 0;
	
	// these track migrants out to us from other subpopulations, in nonWF models
	double gui_premigration_size_ = 0;					// the size of this subpop without migration
	std::map<slim_objectid_t,double> gui_migrants_;		// m[i]: count of migrants from subpopulation i in this cycle
#endif	// defined(SLIMGUI)
	
	Subpopulation(const Subpopulation&) = delete;													// no copying
	Subpopulation& operator=(const Subpopulation&) = delete;										// no copying
	Subpopulation(void) = delete;																	// no null construction
	Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size, bool p_record_in_treeseq, bool p_haploid);
	Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size, bool p_record_in_treeseq,
				  double p_sex_ratio, GenomeType p_modeled_chromosome_type, bool p_haploid);		// SEX ONLY: construct with a sex ratio (fraction male), chromosome type (AXY), and X dominance coeff
	~Subpopulation(void);																			// destructor
	
	void SetName(const std::string &p_name);												// change the name property of the subpopulation, handling the uniqueness logic
	
	slim_popsize_t DrawParentUsingFitness(gsl_rng *rng) const;								// WF only: draw an individual from the subpopulation based upon fitness
	slim_popsize_t DrawFemaleParentUsingFitness(gsl_rng *rng) const;						// WF only: draw a female from the subpopulation based upon fitness; SEX ONLY
	slim_popsize_t DrawMaleParentUsingFitness(gsl_rng *rng) const;							// WF only: draw a male from the subpopulation based upon fitness; SEX ONLY

	slim_popsize_t DrawParentEqualProbability(gsl_rng *rng) const;							// draw an individual from the subpopulation with equal probabilities
	slim_popsize_t DrawFemaleParentEqualProbability(gsl_rng *rng) const;					// draw a female from the subpopulation  with equal probabilities; SEX ONLY
	slim_popsize_t DrawMaleParentEqualProbability(gsl_rng *rng) const;						// draw a male from the subpopulation  with equal probabilities; SEX ONLY
	
	// Returns a new genome object that is cleared to nullptr; call clear_to_empty() afterwards if you need empty mutruns
	Genome *_NewSubpopGenome_NULL(GenomeType p_genome_type);	// internal use only
	Genome *_NewSubpopGenome_NONNULL(int p_mutrun_count, slim_position_t p_mutrun_length, GenomeType p_genome_type);	// internal use only
	inline __attribute__((always_inline)) Genome *NewSubpopGenome_NULL(GenomeType p_genome_type)
	{
		if (genome_junkyard_null.size())
		{
			Genome *back = genome_junkyard_null.back();
			genome_junkyard_null.pop_back();
			
			back->genome_type_ = p_genome_type;
			return back;
		}
		
		return _NewSubpopGenome_NULL(p_genome_type);
	}
	inline __attribute__((always_inline)) Genome *NewSubpopGenome_NONNULL(int p_mutrun_count, slim_position_t p_mutrun_length, GenomeType p_genome_type)
	{
#if DEBUG
		if (p_mutrun_count == 0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::NewSubpopGenome_NONNULL): (internal error) mutrun count of zero with p_is_null == false." << EidosTerminate();
#endif
		
		if (genome_junkyard_nonnull.size())
		{
			Genome *back = genome_junkyard_nonnull.back();
			genome_junkyard_nonnull.pop_back();
			
			// Conceptually, we want to call ReinitializeGenomeNoClear() to set the genome up with the
			// current type, mutrun setup, etc.  But we know that the genome is nonnull, and that we
			// want it to be nonnull, so we can do less work than ReinitializeGenomeNoClear(), inline.
			back->genome_type_ = p_genome_type;
			
			if (back->mutrun_count_ != p_mutrun_count)
			{
				// the number of mutruns has changed; need to reallocate
				if (back->mutruns_ != back->run_buffer_)
					free(back->mutruns_);
				
				back->mutrun_count_ = p_mutrun_count;
				back->mutrun_length_ = p_mutrun_length;
				
				if (p_mutrun_count <= SLIM_GENOME_MUTRUN_BUFSIZE)
				{
					back->mutruns_ = back->run_buffer_;
					EIDOS_BZERO(back->run_buffer_, SLIM_GENOME_MUTRUN_BUFSIZE * sizeof(const MutationRun *));
				}
				else
					back->mutruns_ = (const MutationRun **)calloc(p_mutrun_count, sizeof(const MutationRun *));
			}
			else
			{
				// the number of mutruns is unchanged, but we need to zero out the reused buffer here
				if (p_mutrun_count <= SLIM_GENOME_MUTRUN_BUFSIZE)
					EIDOS_BZERO(back->run_buffer_, SLIM_GENOME_MUTRUN_BUFSIZE * sizeof(const MutationRun *));		// much faster because optimized at compile time
				else
					EIDOS_BZERO(back->mutruns_, p_mutrun_count * sizeof(const MutationRun *));
			}
			return back;
		}
		
		return _NewSubpopGenome_NONNULL(p_mutrun_count, p_mutrun_length, p_genome_type);
	}
	
	// Frees a genome object (puts it in one of the junkyards); we do not clear the mutrun buffer, so it must be cleared when reused!
	inline __attribute__((always_inline)) void FreeSubpopGenome(Genome *p_genome)
	{
		if (p_genome->IsNull())
			genome_junkyard_null.emplace_back(p_genome);
		else
			genome_junkyard_nonnull.emplace_back(p_genome);
	}
	
	void GenerateParentsToFit(slim_age_t p_initial_age, double p_sex_ratio, bool p_allow_zero_size, bool p_require_both_sexes, bool p_record_in_treeseq, bool p_haploid, float p_mean_parent_age);	// given the set subpop size and requested sex ratio, make new genomes and individuals to fit
	void CheckIndividualIntegrity(void);
	
	IndividualSex SexOfIndividual(slim_popsize_t p_individual_index);						// return the sex of the individual at the given index; uses child_generation_valid_
#if (defined(_OPENMP) && SLIM_USE_NONNEUTRAL_CACHES)
	void FixNonNeutralCaches_OMP(void);
#endif
	void UpdateFitness(std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks, std::vector<SLiMEidosBlock*> &p_fitnessEffect_callbacks);	// update fitness values based upon current mutations

	// calculate the fitness of a given individual; the x dominance coeff is used only if the X is modeled
	double FitnessOfParentWithGenomeIndices_NoCallbacks(slim_popsize_t p_individual_index);
	double FitnessOfParentWithGenomeIndices_Callbacks(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
	double FitnessOfParentWithGenomeIndices_SingleCallback(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks, MutationType *p_single_callback_mut_type);
	
	double ApplyMutationEffectCallbacks(MutationIndex p_mutation, int p_homozygous, double p_computed_fitness, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks, Individual *p_individual);
	double ApplyFitnessEffectCallbacks(std::vector<SLiMEidosBlock*> &p_fitnessEffect_callbacks, slim_popsize_t p_individual_index);
	
	// WF only:
	void WipeIndividualsAndGenomes(std::vector<Individual *> &p_individuals, std::vector<Genome *> &p_genomes, slim_popsize_t p_individual_count, slim_popsize_t p_first_male);
	void GenerateChildrenToFitWF(void);		// given the set subpop size and sex ratio, configure the child generation genomes and individuals to fit
	void UpdateWFFitnessBuffers(bool p_pure_neutral);																					// update the WF model fitness buffers after UpdateFitness()
	void TallyLifetimeReproductiveOutput(void);
	void SwapChildAndParentGenomes(void);															// switch to the next generation by swapping; the children become the parents

	// nonWF only:
	void ApplyReproductionCallbacks(std::vector<SLiMEidosBlock*> &p_reproduction_callbacks, slim_popsize_t p_individual_index);
	void ReproduceSubpopulation(void);
	void MergeReproductionOffspring(void);
	bool ApplySurvivalCallbacks(std::vector<SLiMEidosBlock*> &p_survival_callbacks, Individual *p_individual, double p_fitness, double p_draw, bool p_surviving);
	void ViabilitySurvival(std::vector<SLiMEidosBlock*> &p_survival_callbacks);
	void IncrementIndividualAges(void);
	IndividualSex _GenomeConfigurationForSex(EidosValue *p_sex_value, GenomeType &p_genome1_type, GenomeType &p_genome2_type, bool &p_genome1_null, bool &p_genome2_null);
	inline __attribute__((always_inline)) void _ProcessNewOffspring(bool p_proposed_child_accepted, Individual *p_individual, Genome *p_genome1, Genome *p_genome2, EidosValue_Object_vector *p_result)
	{
		if (p_proposed_child_accepted)
		{
			// The child was accepted, so add it to our staging area and to the caller's result vector
			nonWF_offspring_genomes_.emplace_back(p_genome1);
			nonWF_offspring_genomes_.emplace_back(p_genome2);
			nonWF_offspring_individuals_.emplace_back(p_individual);
			
			p_result->push_object_element_NORR(p_individual);
		}
		else
		{
			// The child was rejected, so dispose of the genomes and individual
			FreeSubpopGenome(p_genome1);
			FreeSubpopGenome(p_genome2);
			
			p_individual->~Individual();
			individual_pool_.DisposeChunk(const_cast<Individual *>(p_individual));
			
			// TREE SEQUENCE RECORDING
			if (species_.RecordingTreeSequence())
				species_.RetractNewIndividual();
		}
	}
	
	// inline accessors that handle the parent/child distinction
	inline __attribute__((always_inline)) std::vector<Individual *> &CurrentIndividuals(void)
	{
		return (child_generation_valid_ ? child_individuals_ : parent_individuals_);
	}
	inline __attribute__((always_inline)) std::vector<Genome *> &CurrentGenomes(void)
	{
		return (child_generation_valid_ ? child_genomes_ : parent_genomes_);
	}
	inline __attribute__((always_inline)) slim_popsize_t CurrentGenomeCount(void)
	{
		return (child_generation_valid_ ? 2*child_subpop_size_ : 2*parent_subpop_size_);
	}
	inline __attribute__((always_inline)) slim_popsize_t CurrentSubpopSize(void)
	{
		return (child_generation_valid_ ? child_subpop_size_ : parent_subpop_size_);
	}
	inline __attribute__((always_inline)) slim_popsize_t CurrentFirstMaleIndex(void)
	{
		return (child_generation_valid_ ? child_first_male_index_ : parent_first_male_index_);
	}
	
	// Memory usage tallying, for outputUsage()
	size_t MemoryUsageForParentTables(void);
	
	//
	// Eidos support
	//
	inline EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; };
	
	virtual const EidosClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	
	// WF only:
	EidosValue_SP ExecuteMethod_setMigrationRates(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setCloningRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setSelfingRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setSexRatio(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setSubpopulationSize(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// nonWF only:
	EidosValue_SP ExecuteMethod_addCloned(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addCrossed(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addEmpty(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addRecombinant(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addSelfed(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_removeSubpopulation(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_takeMigrants(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	EidosValue_SP ExecuteMethod_pointDeviated(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointInBounds(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointReflected(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointStopped(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointPeriodic(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointUniform(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setSpatialBounds(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_cachedFitness(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_defineSpatialMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addSpatialMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_removeSpatialMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_spatialMapColor(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_spatialMapImage(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_spatialMapValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputXSample(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_sampleIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_subsetIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_configureDisplay(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObject for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_id(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_firstMaleIndex(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_individualCount(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_fitnessScaling(EidosObject **p_values, size_t p_values_size);
	
	static void SetProperty_Accelerated_fitnessScaling(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
};


inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawParentUsingFitness(gsl_rng *rng) const
{
#if DEBUG
	if (sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawParentUsingFitness): (internal error) called on a population for which sex is enabled." << EidosTerminate();
#endif
	
	if (lookup_parent_)
		return static_cast<slim_popsize_t>(gsl_ran_discrete(rng, lookup_parent_));
	else
		return static_cast<slim_popsize_t>(Eidos_rng_uniform_int(rng, parent_subpop_size_));
}

inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawParentEqualProbability(gsl_rng *rng) const
{
#if DEBUG
	if (sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawParentEqualProbability): (internal error) called on a population for which sex is enabled." << EidosTerminate();
#endif
	
	return static_cast<slim_popsize_t>(Eidos_rng_uniform_int(rng, parent_subpop_size_));
}

// SEX ONLY
inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawFemaleParentUsingFitness(gsl_rng *rng) const
{
#if DEBUG
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawFemaleParentUsingFitness): (internal error) called on a population for which sex is not enabled." << EidosTerminate();
#endif
	
	if (lookup_female_parent_)
		return static_cast<slim_popsize_t>(gsl_ran_discrete(rng, lookup_female_parent_));
	else
		return static_cast<slim_popsize_t>(Eidos_rng_uniform_int(rng, parent_first_male_index_));
}

// SEX ONLY
inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawFemaleParentEqualProbability(gsl_rng *rng) const
{
#if DEBUG
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawFemaleParentEqualProbability): (internal error) called on a population for which sex is not enabled." << EidosTerminate();
#endif
	
	return static_cast<slim_popsize_t>(Eidos_rng_uniform_int(rng, parent_first_male_index_));
}

// SEX ONLY
inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawMaleParentUsingFitness(gsl_rng *rng) const
{
#if DEBUG
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawMaleParentUsingFitness): (internal error) called on a population for which sex is not enabled." << EidosTerminate();
#endif
	
	if (lookup_male_parent_)
		return static_cast<slim_popsize_t>(gsl_ran_discrete(rng, lookup_male_parent_)) + parent_first_male_index_;
	else
		return static_cast<slim_popsize_t>(Eidos_rng_uniform_int(rng, parent_subpop_size_ - parent_first_male_index_) + parent_first_male_index_);
}

// SEX ONLY
inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawMaleParentEqualProbability(gsl_rng *rng) const
{
#if DEBUG
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawMaleParentEqualProbability): (internal error) called on a population for which sex is not enabled." << EidosTerminate();
#endif
	
	return static_cast<slim_popsize_t>(Eidos_rng_uniform_int(rng, parent_subpop_size_ - parent_first_male_index_) + parent_first_male_index_);
}

inline IndividualSex Subpopulation::SexOfIndividual(slim_popsize_t p_individual_index)
{
	if (!sex_enabled_)
	{
		return IndividualSex::kHermaphrodite;
	}
	else if (child_generation_valid_)
	{
		if (p_individual_index < child_first_male_index_)
			return IndividualSex::kFemale;
		else
			return IndividualSex::kMale;
	}
	else
	{
		if (p_individual_index < parent_first_male_index_)
			return IndividualSex::kFemale;
		else
			return IndividualSex::kMale;
	}
}

class Subpopulation_Class : public EidosDictionaryUnretained_Class
{
private:
	typedef EidosDictionaryUnretained_Class super;

public:
	Subpopulation_Class(const Subpopulation_Class &p_original) = delete;	// no copy-construct
	Subpopulation_Class& operator=(const Subpopulation_Class&) = delete;	// no copying
	inline Subpopulation_Class(const std::string &p_class_name, EidosClass *p_superclass) : super(p_class_name, p_superclass) { }
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};


#endif /* defined(__SLiM__subpopulation__) */




































































