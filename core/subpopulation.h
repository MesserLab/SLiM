//
//  subpopulation.h
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
 
 The class Subpopulation represents one simulated subpopulation, defined primarily by the individuals it contains and their haplosomes.
 A subpopulation also knows its size, its selfing fraction, and what fraction it receives as migrants from other subpopulations.
 
 The way that Subpopulation handles its Haplosome objects is quite complex, and is at the heart of the operation of the SLiM core,
 so a lengthy comment on it is merited.  Haplosomes contain MutationRuns, and they can allocate a buffer in which they keep pointers
 to those runs.  Allocating and deallocating those buffers takes time, and so is to be avoided; for this reason, Subpopulation
 re-uses Haplosome objects.  When a Haplosome is done being used, it goes into a "junkyard" vector (which one depends on whether it is
 a null haplosome or not); FreeHaplosome() handles that.  When a new haplosome is needed, it is preferentially obtained from the
 appropriate junkyard, and is re-purposed to its new use; NewHaplosome() handles that.  Those methods should be called to
 handle most create/dispose requests for Haplosomes, so that those junkyard vectors get used.  When NewHaplosome() finds the
 junkyard to be empty, it needs to actually allocate a new Haplosome object.  This is also complicated.  Haplosomes are allocated out
 of a memory pool, haplosome_pool_, that belongs to the species.  This helps with memory locality; it keeps all of the Haplosome
 objects used by a given species grouped closely together in memory.  (We do the same with Individual objects, with another
 pool, for the same reason).  So allocations of Haplosomes come out of that pool, and deallocations go back into the pool.  Do not
 use new or delete with Haplosome objects.  There is one more complication.  In WF models, separate "parent" and "child" generations
 are kept by the subpop, and which one is active switches back and forth in each cycle, governed by child_generation_valid_.
 In nonWF models, the child generation variables are all unused; in nonWF models, the parental generation is always active.
 New offspring instead get put into the nonWF_offspring_ ivars, which get moved into the parental generation ivars at the end of
 offspring generation.  So in some ways these schemes are similar, but they use a completely non-intersecting set of ivars.  The
 parental ivars are used by both WF and nonWF models, though.
 
 BCH 10/19/2024: Note that haplosomes are now associated with a particular Chromosome object; each chromosome might use a different
 number of mutation runs, so for efficiency we want to keep the haplosome junkyards for different chromosomes separate.  Each
 chromosome therefore now has its own haplosome junkyard, and NewHaplosome_NULL(), NewHaplosome_NONNULL(), and FreeHaplosome()
 are now Chromosome methods that work with the target chromosome's junkyard.
 
 */

#ifndef __SLiM__subpopulation__
#define __SLiM__subpopulation__


#include "slim_globals.h"
#include "eidos_rng.h"
#include "haplosome.h"
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
	
	EidosObjectPool &individual_pool_;				// NOT OWNED: a pool out of which individuals are allocated, for within-species locality of memory usage across individuals
	std::vector<Individual *> &individuals_junkyard_;	// NOT OWNED: individuals get put here when we're done with them, so we can reuse them quickly
	
	int haplosome_count_per_individual_;					// inherits its value from species_.haplosome_count_per_individual_
	bool has_null_haplosomes_ = false;					// inherits its value from species_.chromosomes_use_null_haplosomes_ but can additionally be false; use CouldContainNullHaplosomes() to check this flag
	
	slim_popsize_t parent_subpop_size_;				// parental subpopulation size
	slim_popsize_t parent_first_male_index_ = INT_MAX;	// the index of the first male in the parental Haplosome vector (NOT premultiplied by 2!); equal to the number of females
	std::vector<Individual *> parent_individuals_;	// OWNED: objects representing simulated individuals, each of which has two haplosomes
	EidosValue_SP cached_parent_individuals_value_;	// cached for the individuals property; self-maintains
	double parent_sex_ratio_ = 0.0;					// WF only: what sex ratio the parent individuals approximate (M:M+F)
	
	// WF only:
	// In WF models, we actually switch to a "child" generation just after offspring generation; this is then the active generation.
	// Then, with SwapChildAndParentHaplosomes(), the child generation gets swapped into the parent generation, which becomes active.
	// I'm not sure this is a great design really, but it's pretty entrenched at this point, and pretty harmless...
	slim_popsize_t child_subpop_size_;				// child subpopulation size
	slim_popsize_t child_first_male_index_ = INT_MAX;	// the index of the first male in the child Haplosome vector (NOT premultiplied by 2!); equal to the number of females
	std::vector<Individual *> child_individuals_;	// OWNED: objects representing simulated individuals, each of which has two haplosomes
	double child_sex_ratio_ = 0.0;					// what sex ratio the child individuals approximate (M:M+F)
	
	// nonWF only:
	// In nonWF models, we place generated offspring into a temporary holding pen, but it is never made the "active generation"
	// the way it is in WF models.  At soon as offspring generation is finished, these individuals get merged back in.  The
	// individuals here are kept in the order in which they were generated, not in order by sex or anything else.
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
	bool sex_enabled_ = false;										// the subpopulation needs to have easy reference to whether its individuals are sexual or not
	
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
				  double p_sex_ratio, bool p_haploid);		// SEX ONLY: construct with a sex ratio (fraction male)
	~Subpopulation(void);																			// destructor
	
	void SetName(const std::string &p_name);												// change the name property of the subpopulation, handling the uniqueness logic
	
	inline __attribute__((always_inline)) int HaplosomeCountPerIndividual(void) { return haplosome_count_per_individual_; }
	slim_refcount_t NullHaplosomeCount(void);
	inline bool CouldContainNullHaplosomes(void) {
#if DEBUG
		// in DEBUG, check that has_null_haplosomes_ is not false when null haplosomes in fact exist (we allow the opposite case)
		if (!has_null_haplosomes_ && (NullHaplosomeCount() > 0))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CouldContainNullHaplosomes): (internal error) has_null_haplosomes_ is not correct." << EidosTerminate();
#endif
		return has_null_haplosomes_;
	}
	
	slim_popsize_t DrawParentUsingFitness(gsl_rng *rng) const;								// WF only: draw an individual from the subpopulation based upon fitness
	slim_popsize_t DrawFemaleParentUsingFitness(gsl_rng *rng) const;						// WF only: draw a female from the subpopulation based upon fitness; SEX ONLY
	slim_popsize_t DrawMaleParentUsingFitness(gsl_rng *rng) const;							// WF only: draw a male from the subpopulation based upon fitness; SEX ONLY

	slim_popsize_t DrawParentEqualProbability(gsl_rng *rng) const;							// draw an individual from the subpopulation with equal probabilities
	slim_popsize_t DrawFemaleParentEqualProbability(gsl_rng *rng) const;					// draw a female from the subpopulation  with equal probabilities; SEX ONLY
	slim_popsize_t DrawMaleParentEqualProbability(gsl_rng *rng) const;						// draw a male from the subpopulation  with equal probabilities; SEX ONLY
	
	inline __attribute__((always_inline)) Individual *NewSubpopIndividual(slim_popsize_t p_individual_index, IndividualSex p_sex, slim_age_t p_age, double p_fitness, float p_mean_parent_age)
	{
		if (individuals_junkyard_.size())
		{
			Individual *back = individuals_junkyard_.back();
			individuals_junkyard_.pop_back();
			
#if DEBUG
			// check all ivars that should be guaranteed by the junkyard
			if ((back->KeyCount() != 0) || (back->tag_value_ != SLIM_TAG_UNSET_VALUE) || (back->tagF_value_ != SLIM_TAGF_UNSET_VALUE) || (back->tagL0_set_ != false) || (back->tagL1_set_ != false) || (back->tagL2_set_ != false) || (back->tagL3_set_ != false) || (back->tagL4_set_ != false) || (back->reproductive_output_ != 0)
#ifdef SLIMGUI
				// BCH 3/23/2025: color variables now only exist in SLiMgui, to save on memory footprint
				|| (back->color_set_ != false)
#endif				
				)
				EIDOS_TERMINATION << "ERROR (Subpopulation::NewSubpopIndividual): (internal error) junkyard individual incorrectly configured." << EidosTerminate();
#endif
			
#if DEBUG
			// set up our haplosomes vector with nullptr values initially, when in DEBUG
			EIDOS_BZERO(back->haplosomes_, haplosome_count_per_individual_ * sizeof(Haplosome *));
#endif
			
			// set all ivars that are not guaranteed by the junkyard; see FreeSubpopIndividual()
			// note that we do not reset the pedigree ivars anywhere; they should be overwritten if
			// pedigree tracking is enabled, otherwise they will be left as -1 since they are unused
			back->mean_parent_age_ = p_mean_parent_age;
			back->sex_ = p_sex;
			back->migrant_ = false;
			back->killed_ = false;
			back->fitness_scaling_ = 1.0;
			back->cached_fitness_UNSAFE_ = p_fitness;
#ifdef SLIMGUI
			back->cached_unscaled_fitness_ = p_fitness;
#endif
			back->age_ = p_age;
			back->index_ = p_individual_index;
			back->subpopulation_ = this;
			return back;
		}
		
		return  new (individual_pool_.AllocateChunk()) Individual(this, p_individual_index, p_sex, p_age, p_fitness, p_mean_parent_age);
	}
	
	inline __attribute__((always_inline)) void FreeSubpopIndividual(Individual *p_individual)
	{
		// The individuals junkyard guarantees certain things, since it can sometimes do so efficiently.
		// This is based on what can be reset efficiently in WF models in Subpopulation::SwapChildAndParentHaplosomes(),
		// which performs these resets itself as needed (and GenerateChildrenToFitWF() then returns individuals to the
		// junkyard directly with _FreeSubpopIndividual(), avoiding these state resets).
		p_individual->RemoveAllKeys();	// no call to ContentsChanged() here, for speed; we know individual is a Dictionary not a DataFrame
		p_individual->ClearColor();
		p_individual->tag_value_ = SLIM_TAG_UNSET_VALUE;
		p_individual->tagF_value_ = SLIM_TAGF_UNSET_VALUE;
		p_individual->tagL0_set_ = false;
		p_individual->tagL1_set_ = false;
		p_individual->tagL2_set_ = false;
		p_individual->tagL3_set_ = false;
		p_individual->tagL4_set_ = false;
		p_individual->reproductive_output_ = 0;
		
		_FreeSubpopIndividual(p_individual);
	}
	inline __attribute__((always_inline)) void _FreeSubpopIndividual(Individual *p_individual)
	{
#if 0
		// To avoid using the junkvard and debug problems with it, just enable this block.
		individual->~Individual();
		population_.species_individual_pool_.DisposeChunk(const_cast<Individual *>(individual));
		return;
#endif
		
		// This returns an individual to the junkyard directly; the caller is responsible for
		// resetting all of the state the junkyard guarantees; see FreeSubpopIndividual().
		// The only thing we reset here is haplosomes_, since we want to free up those
		// resources immediately in all code paths.
		const std::vector<Chromosome *> &chromosome_for_haplosome_index = species_.ChromosomesForHaplosomeIndices();
		int haplosome_count = haplosome_count_per_individual_;
		Haplosome **haplosomes = p_individual->haplosomes_;
		
		for (int haplosome_index = 0; haplosome_index < haplosome_count; haplosome_index++)
		{
			Haplosome *haplosome = haplosomes[haplosome_index];
			Chromosome *chromosome = chromosome_for_haplosome_index[haplosome_index];
			
			chromosome->FreeHaplosome(haplosome);
		}
		
		// Clear our haplosomes vector with nullptr values.  This is perhaps not strictly necessary,
		// since the nullptr for subpopulation_ set below would already indicate that the individual
		// is no longer valid and its haplosomes have been freed; but it seems wise to clean up
		// here to prevent the possibility of access to stale haplosomes.  This is one cleanup that
		// should not be done only in DEBUG, I think; the risk of logic errors with this is too high.
		EIDOS_BZERO(p_individual->haplosomes_, haplosome_count_per_individual_ * sizeof(Haplosome *));
		
		// The individual needs to be detached from its subpopulation, since the subpopulation
		// might get freed while the individual is still in the junkyard.  This means it
		// will not be able to access things through the subpopulation any more; but since
		// its haplosomes have already been freed, above, that ought to be OK.
		p_individual->subpopulation_ = nullptr;
		
		individuals_junkyard_.emplace_back(p_individual);
	}
	
	void GenerateParentsToFit(slim_age_t p_initial_age, double p_sex_ratio, bool p_allow_zero_size, bool p_require_both_sexes, bool p_record_in_treeseq, bool p_haploid, float p_mean_parent_age);	// given the set subpop size and requested sex ratio, make new haplosomes and individuals to fit
	void CheckIndividualIntegrity(void);
	
#if (defined(_OPENMP) && SLIM_USE_NONNEUTRAL_CACHES)
	void FixNonNeutralCaches_OMP(void);
#endif
	void UpdateFitness(std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks, std::vector<SLiMEidosBlock*> &p_fitnessEffect_callbacks);	// update fitness values based upon current mutations

	// calculate the fitness of a given individual; the x dominance coeff is used only if the X is modeled
	template <const bool f_mutrunexps, const bool f_callbacks, const bool f_singlecallback>
	double FitnessOfParent(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
	double FitnessOfParent_1CH_Diploid(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
	double FitnessOfParent_1CH_Haploid(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
	
	template <const bool f_callbacks, const bool f_singlecallback>
	double _Fitness_HaploidChromosome(Haplosome *haplosome, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
	template <const bool f_callbacks, const bool f_singlecallback>
	double _Fitness_DiploidChromosome(Haplosome *haplosome1, Haplosome *haplosome2, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
	
	double ApplyMutationEffectCallbacks(MutationIndex p_mutation, int p_homozygous, double p_computed_fitness, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks, Individual *p_individual);
	double ApplyFitnessEffectCallbacks(std::vector<SLiMEidosBlock*> &p_fitnessEffect_callbacks, slim_popsize_t p_individual_index);
	
	// generate newly allocated offspring individuals from parent individuals; these methods loop over
	// chromosomes/haplosomes, and are templated for speed, providing a set of optimized variants
	template <const bool f_mutrunexps, const bool f_pedigree_rec, const bool f_treeseq, const bool f_callbacks, const bool f_spatial>
	Individual *GenerateIndividualCrossed(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
	
	template <const bool f_mutrunexps, const bool f_pedigree_rec, const bool f_treeseq, const bool f_callbacks, const bool f_spatial>
	Individual *GenerateIndividualSelfed(Individual *p_parent);
	
	template <const bool f_mutrunexps, const bool f_pedigree_rec, const bool f_treeseq, const bool f_callbacks, const bool f_spatial>
	Individual *GenerateIndividualCloned(Individual *p_parent);
	
	Individual *GenerateIndividualEmpty(slim_popsize_t p_individual_index, IndividualSex p_child_sex, slim_age_t p_age, double p_fitness, float p_mean_parent_age, bool p_haplosome1_null, bool p_haplosome2_null, bool p_run_modify_child, bool p_record_in_treeseq);
	
	// these WF-only "munge" variants munge an existing individual into the new child, reusing the individual
	// and its haplosome objects; they are all templated for speed, providing variants for different milieux
	template <const bool f_mutrunexps, const bool f_pedigree_rec, const bool f_treeseq, const bool f_callbacks, const bool f_spatial>
	bool MungeIndividualCrossed(Individual *p_child, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
	
	template <const bool f_pedigree_rec, const bool f_treeseq, const bool f_spatial>
	bool MungeIndividualCrossed_1CH_A(Individual *p_child, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
	
	template <const bool f_pedigree_rec, const bool f_treeseq, const bool f_spatial>
	bool MungeIndividualCrossed_1CH_H(Individual *p_child, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
	
	template <const bool f_mutrunexps, const bool f_pedigree_rec, const bool f_treeseq, const bool f_callbacks, const bool f_spatial>
	bool MungeIndividualSelfed(Individual *p_child, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
	
	template <const bool f_mutrunexps, const bool f_pedigree_rec, const bool f_treeseq, const bool f_callbacks, const bool f_spatial>
	bool MungeIndividualCloned(Individual *p_child, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
	
	template <const bool f_pedigree_rec, const bool f_treeseq, const bool f_spatial>
	bool MungeIndividualCloned_1CH_A(Individual *p_child, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
	
	template <const bool f_pedigree_rec, const bool f_treeseq, const bool f_spatial>
	bool MungeIndividualCloned_1CH_H(Individual *p_child, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
	
	// WF only:
	void WipeIndividualsAndHaplosomes(std::vector<Individual *> &p_individuals, slim_popsize_t p_individual_count, slim_popsize_t p_first_male);
	void GenerateChildrenToFitWF(void);		// given the set subpop size and sex ratio, configure the child generation haplosomes and individuals to fit
	void UpdateWFFitnessBuffers(bool p_pure_neutral);																					// update the WF model fitness buffers after UpdateFitness()
	void TallyLifetimeReproductiveOutput(void);
	void SwapChildAndParentHaplosomes(void);															// switch to the next generation by swapping; the children become the parents

	// nonWF only:
	void ApplyReproductionCallbacks(std::vector<SLiMEidosBlock*> &p_reproduction_callbacks, slim_popsize_t p_individual_index);
	void ReproduceSubpopulation(void);
	void MergeReproductionOffspring(void);
	bool ApplySurvivalCallbacks(std::vector<SLiMEidosBlock*> &p_survival_callbacks, Individual *p_individual, double p_fitness, double p_draw, bool p_surviving);
	void ViabilitySurvival(std::vector<SLiMEidosBlock*> &p_survival_callbacks);
	void IncrementIndividualAges(void);
	
	static IndividualSex _ValidateHaplosomesAndChooseSex(ChromosomeType p_chromosome_type, bool p_haplosome1_null, bool p_haplosome2_null, EidosValue *p_sex_value, bool p_sex_enabled, const char *p_caller_name);
	static IndividualSex _SexForSexValue(EidosValue *p_sex_value, bool p_sex_enabled);
	
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
	EidosValue_SP ExecuteMethod_addMultiRecombinant(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addRecombinant(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addSelfed(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_removeSubpopulation(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_takeMigrants(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	EidosValue_SP ExecuteMethod_haplosomesForChromosomes(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_deviatePositions(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_deviatePositionsWithMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointDeviated(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointInBounds(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointReflected(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointStopped(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointPeriodic(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointUniform(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointUniformWithMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
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




































































