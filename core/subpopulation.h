//
//  subpopulation.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2020 Philipp Messer.  All rights reserved.
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
 are kept by the subpop, and which one is active switches back and forth in each generation, governed by child_generation_valid_.
 In nonWF models, the child generation variables are all unused; we have a #ifdef scheme with SLIM_WF_ONLY and SLIM_NONWF_ONLY to
 test for correct isolation of WF and nonWF code (see slim_global.h).  In nonWF models, the parental generation is always active.
 New offspring instead get put into the nonWF_offspring_ ivars, which get moved into the parental generation ivars at the end of
 offspring generation.  So in some ways these schemes are similar, but they use a completely non-intersecting set of ivars.  The
 parental ivars are used by both WF and nonWF models, though.
 
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
#include "slim_sim.h"

#include <vector>
#include <map>
#include "limits.h"


class Population;


extern EidosObjectClass *gSLiM_Subpopulation_Class;


#pragma mark -
#pragma mark _SpatialMap
#pragma mark -

// This is a structure for keeping a "spatial map", a 1D, 2D, or 3D grid of values for points in space that can be defined
// and then queried by the user.  These are used only in spatial simulations.  Besides being a sort of N-dimensional data
// structure to fill the void made by the lack of an array type in Eidos, it also manages interpolation, rescaling to fit
// the spatial bounds of the subpopulation, color mapping, and other miscellaneous issues.
struct _SpatialMap
{
	std::string spatiality_string_;		// "x", "y", "z", "xy", "xz", "yz", or "xyz": the spatial dimensions for the map
	int spatiality_;					// 1, 2, or 3 for 1D, 2D, or 3D: the number of spatial dimensions
	int64_t grid_size_[3];				// the number of points in the first, second, and third spatial dimensions
	double *values_;					// OWNED POINTER: the values for the grid points
	bool interpolate_;					// if true, the map will interpolate values; otherwise, nearest-neighbor
	
	double min_value_, max_value_;		// min/max values permitted; used for color mapping, not for bounds-checking values
	
	int n_colors_;						// the number of color values given to map across the min/max value range
	float *red_components_;				// OWNED POINTER: red components, n_colors_ in size, from min to max value
	float *green_components_;			// OWNED POINTER: green components, n_colors_ in size, from min to max value
	float *blue_components_;			// OWNED POINTER: blue components, n_colors_ in size, from min to max value
	
	uint8_t *display_buffer_;			// OWNED POINTER: used by SLiMgui, contains RGB values for pixels in the PopulationView
	int buffer_width_, buffer_height_;	// the size of the buffer, in pixels, each of which is 3 x sizeof(uint8_t)
	
	_SpatialMap(std::string p_spatiality_string, int p_spatiality, int64_t *p_grid_sizes, bool p_interpolate, double p_min_value, double p_max_value, int p_num_colors);
	~_SpatialMap(void);
	
	double ValueAtPoint_S1(double *p_point);
	double ValueAtPoint_S2(double *p_point);
	double ValueAtPoint_S3(double *p_point);
	void ColorForValue(double p_value, double *p_rgb_ptr);
	void ColorForValue(double p_value, float *p_rgb_ptr);
};
typedef struct _SpatialMap SpatialMap;

typedef std::pair<std::string, SpatialMap *> SpatialMapPair;
typedef std::map<std::string, SpatialMap *> SpatialMapMap;


#pragma mark -
#pragma mark Subpopulation
#pragma mark -

class Subpopulation : public EidosDictionary
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	
#ifdef SLIM_WF_ONLY
	gsl_ran_discrete_t *lookup_parent_ = nullptr;			// OWNED POINTER: lookup table for drawing a parent based upon fitness
	gsl_ran_discrete_t *lookup_female_parent_ = nullptr;	// OWNED POINTER: lookup table for drawing a female parent based upon fitness, SEX ONLY
	gsl_ran_discrete_t *lookup_male_parent_ = nullptr;		// OWNED POINTER: lookup table for drawing a male parent based upon fitness, SEX ONLY
#endif	// SLIM_WF_ONLY
	
	EidosSymbolTableEntry self_symbol_;						// for fast setup of the symbol table
	
public:
	
	Population &population_;						// we need to know our Population so we can remove ourselves, etc.
	
	slim_objectid_t subpopulation_id_;				// the id by which this subpopulation is indexed in the Population
	EidosValue_SP cached_value_subpop_id_;			// a cached value for subpopulation_id_; reset() if changed
	
#ifdef SLIM_WF_ONLY
	double selfing_fraction_ = 0.0;					// ASEX ONLY: selfing fraction, the fraction of offspring generated by self-fertilization
	double female_clone_fraction_ = 0.0;			// both asex and sex; in the asex case, male_clone_fraction_ == female_clone_fraction_ and is simply the clonal fraction
	double male_clone_fraction_ = 0.0;				//		these represent the fraction of offspring generated by asexual clonal reproduction
	std::map<slim_objectid_t,double> migrant_fractions_;		// m[i]: fraction made up of migrants from subpopulation i per generation
#endif	// SLIM_WF_ONLY
	
	EidosObjectPool *genome_pool_ = nullptr;		// a pool out of which genomes are allocated, for locality of memory usage across genomes
	EidosObjectPool *individual_pool_ = nullptr;	// a pool out of which individuals are allocated, for locality of memory usage across individuals
	
	std::vector<Genome *> genome_junkyard_nonnull;	// non-null genomes get put here when we're done with them, so we can reuse them without dealloc/realloc of their mutrun buffers
	std::vector<Genome *> genome_junkyard_null;		// null genomes get put here when we're done with them, so we can reuse them without dealloc/realloc of their mutrun buffers
	
	std::vector<Genome *> parent_genomes_;			// OWNED: all genomes in the parental generation; each individual gets two genomes, males are XY (not YX)
	EidosValue_SP cached_parent_genomes_value_;		// cached for the genomes property; reset() if changed
	slim_popsize_t parent_subpop_size_;				// parental subpopulation size
	slim_popsize_t parent_first_male_index_ = INT_MAX;	// the index of the first male in the parental Genome vector (NOT premultiplied by 2!); equal to the number of females
	std::vector<Individual *> parent_individuals_;	// OWNED: objects representing simulated individuals, each of which has two genomes
	EidosValue_SP cached_parent_individuals_value_;	// cached for the individuals property; self-maintains
#ifdef SLIM_WF_ONLY
	double parent_sex_ratio_ = 0.0;					// what sex ratio the parental genomes approximate (M:M+F)
#endif	// SLIM_WF_ONLY
	
#ifdef SLIM_WF_ONLY
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
#endif	// SLIM_WF_ONLY
	
#ifdef SLIM_NONWF_ONLY
	// In nonWF models, we place generated offspring into a temporary holding pen, but it is never made the "active generation"
	// the way it is in WF models.  At soon as offspring generation is finished, these individuals get merged back in.  The
	// individuals here are kept in the order in which they were generated, not in order by sex or anything else.
	std::vector<Genome *> nonWF_offspring_genomes_;
	std::vector<Individual *> nonWF_offspring_individuals_;
#endif  // SLIM_NONWF_ONLY
	
#ifdef SLIM_WF_ONLY
	std::vector<SLiMEidosBlock*> registered_mate_choice_callbacks_;		// NOT OWNED: valid only during EvolveSubpopulation; callbacks used when this subpop is parental
#endif	// SLIM_WF_ONLY
	std::vector<SLiMEidosBlock*> registered_modify_child_callbacks_;	// NOT OWNED: valid only during EvolveSubpopulation; callbacks used when this subpop is parental
	std::vector<SLiMEidosBlock*> registered_recombination_callbacks_;	// NOT OWNED: valid only during EvolveSubpopulation; callbacks used when this subpop is parental
	std::vector<SLiMEidosBlock*> registered_mutation_callbacks_;		// NOT OWNED: valid only during EvolveSubpopulation; callbacks used when this subpop is parental
#ifdef SLIM_NONWF_ONLY
	std::vector<SLiMEidosBlock*> registered_reproduction_callbacks_;	// NOT OWNED: valid only during EvolveSubpopulation; callbacks used when this subpop is parental
#endif  // SLIM_NONWF_ONLY
	
#ifdef SLIM_WF_ONLY
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
#endif	// SLIM_WF_ONLY
	
#if (!defined(SLIMGUI) && defined(SLIM_WF_ONLY))
	// Optimized fitness caching at the Individual level.  Individual has an ivar named cached_fitness_UNSAFE_ that keeps a cached fitness value for each individual.
	// When a model is neutral or nearly neutral, every individual may have the same fitness value, and we may know that.  In such cases, we want to avoid setting
	// up the cached fitness value in each individual; instead, we set a flag here that overrides cached_fitness_UNSAFE_ and says it has the same value for all.
	// This optimization only happens when we're not in SLiMgui, for simplicitly, since SLiMgui uses the Individual cached fitness values in various places.  And it
	// happens only for the parental generation's cached fitness values (cached fitness values for the child generation should never be accessed by anyone anyway
	// since they are not valid).  And it happens only in WF models, since in nonWF models the generational overlap makes this scheme impossible.  A somewhat special
	// case, then, but it seems worthwhile since the penalty for seting every cached fitness value, and then gathering them back up again in UpdateWFFitnessBuffers(),
	// is large – almost 20% of total runtime for the full Gravel model, for example.  Neutral WF models are common and worth special-casing.
	bool individual_cached_fitness_OVERRIDE_ = false;
	double individual_cached_fitness_OVERRIDE_value_;
#endif
	
	// SEX ONLY; the default values here are for the non-sex case
	bool sex_enabled_ = false;										// the subpopulation needs to have easy reference to whether its individuals are sexual or not...
	GenomeType modeled_chromosome_type_ = GenomeType::kAutosome;	// ...and needs to know what type of chromosomes its individuals are modeling; this should match SLiMSim
	double x_chromosome_dominance_coeff_ = 1.0;						// the dominance coefficient for heterozygosity at the X locus (i.e. males); this is global
	
	// continuous-space info
	double bounds_x0_ = 0.0, bounds_x1_ = 1.0;
	double bounds_y0_ = 0.0, bounds_y1_ = 1.0;
	double bounds_z0_ = 0.0, bounds_z1_ = 1.0;
	SpatialMapMap spatial_maps_;
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;	// a user-defined tag value
	
	double fitness_scaling_ = 1.0;						// the fitnessScaling property value
#ifdef SLIMGUI
	double last_fitness_scaling_ = 1.0;					// the value for fitnessScaling before it was reset; used by SLiMgui
#endif
	
#ifdef SLIMGUI
	bool gui_selected_ = false;							// keeps track of whether we are selected in SLiMgui's table of subpopulations; note Population::gui_all_selected_ must be kept in synch!
	double parental_total_fitness_ = 0.0;				// updated in SurveyPopulation() when running under SLiMgui
	
	bool gui_center_from_user_ = false;					// if this flag is true, the center below comes from the user and should not be modified
	double gui_center_x_, gui_center_y_;				// the center used by GraphView_PopulationVisualization
	
	bool gui_radius_scaling_from_user_ = false;			// if this flag is true, the radius scaling below comes from the user and should not be modified
	double gui_radius_scaling_;							// set/used only when gui_display_from_user_ is true; a scaling factor for the circle size
	double gui_radius_;									// the radius, post-scaling, used by GraphView_PopulationVisualization
	
	bool gui_color_from_user_ = false;					// if this flag is true, the color below comes from the user and should not be modified
	float gui_color_red_, gui_color_green_, gui_color_blue_;	// the color components, used by GraphView_PopulationVisualization
#endif
	
#if (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
	// these counters track generated offspring of different types, in nonWF models
	int64_t gui_offspring_cloned_M_ = 0;
	int64_t gui_offspring_cloned_F_ = 0;
	int64_t gui_offspring_selfed_ = 0;
	int64_t gui_offspring_crossed_ = 0;
	int64_t gui_offspring_empty_ = 0;
	
	// these track migrants out to us from other subpopulations, in nonWF models
	double gui_premigration_size_ = 0;					// the size of this subpop without migration
	std::map<slim_objectid_t,double> gui_migrants_;		// m[i]: count of migrants from subpopulation i in this generation
#endif	// (defined(SLIM_NONWF_ONLY) && defined(SLIMGUI))
	
	Subpopulation(const Subpopulation&) = delete;													// no copying
	Subpopulation& operator=(const Subpopulation&) = delete;										// no copying
	Subpopulation(void) = delete;																	// no null construction
	Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size, bool p_record_in_treeseq);
	Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size, bool p_record_in_treeseq,
				  double p_sex_ratio, GenomeType p_modeled_chromosome_type, double p_x_chromosome_dominance_coeff);		// SEX ONLY: construct with a sex ratio (fraction male), chromosome type (AXY), and X dominance coeff
	~Subpopulation(void);																			// destructor
	
#ifdef SLIM_WF_ONLY
	slim_popsize_t DrawParentUsingFitness(void) const;										// draw an individual from the subpopulation based upon fitness
	slim_popsize_t DrawFemaleParentUsingFitness(void) const;								// draw a female from the subpopulation based upon fitness; SEX ONLY
	slim_popsize_t DrawMaleParentUsingFitness(void) const;									// draw a male from the subpopulation based upon fitness; SEX ONLY
#endif	// SLIM_WF_ONLY
	slim_popsize_t DrawParentEqualProbability(void) const;									// draw an individual from the subpopulation with equal probabilities
	slim_popsize_t DrawFemaleParentEqualProbability(void) const;							// draw a female from the subpopulation  with equal probabilities; SEX ONLY
	slim_popsize_t DrawMaleParentEqualProbability(void) const;								// draw a male from the subpopulation  with equal probabilities; SEX ONLY
	
	void MakeMemoryPools(size_t p_individual_capacity);
	
	// Returns a new genome object that is cleared to nullptr; call clear_to_empty() afterwards if you need empty mutruns
	Genome *_NewSubpopGenome(int p_mutrun_count, slim_position_t p_mutrun_length, GenomeType p_genome_type, bool p_is_null);	// internal use only
	inline __attribute__((always_inline)) Genome *NewSubpopGenome(int p_mutrun_count, slim_position_t p_mutrun_length, GenomeType p_genome_type, bool p_is_null)
	{
		if (p_is_null)
		{
			if (genome_junkyard_null.size())
			{
				Genome *back = genome_junkyard_null.back();
				genome_junkyard_null.pop_back();
				
				back->genome_type_ = p_genome_type;
				return back;
			}
		}
		else
		{
			if (genome_junkyard_nonnull.size())
			{
				Genome *back = genome_junkyard_nonnull.back();
				genome_junkyard_nonnull.pop_back();
				
				// Conceptually, we want to call ReinitializeGenomeNoClear() to set the genome up with the
				// current type, mutrun setup, etc.  But we know that the genome is nonnull, and that we
				// want it to be nonnull, and we know that the genome has already been cleared to nullptr,
				// so in practice we can do less work than ReinitializeGenomeNoClear(), inline.
				back->genome_type_ = p_genome_type;
				
				if (back->mutrun_count_ != p_mutrun_count)
				{
					// the number of mutruns has changed; need to reallocate
					if (back->mutruns_ != back->run_buffer_)
						delete[] back->mutruns_;
					
					back->mutrun_count_ = p_mutrun_count;
					back->mutrun_length_ = p_mutrun_length;
					
					if (p_mutrun_count <= SLIM_GENOME_MUTRUN_BUFSIZE)
						back->mutruns_ = back->run_buffer_;
					else
						back->mutruns_ = new MutationRun_SP[p_mutrun_count];
				}
				return back;
			}
		}
		
		return _NewSubpopGenome(p_mutrun_count, p_mutrun_length, p_genome_type, p_is_null);
	}
	
	// Frees a genome object (puts it in one of the junkyards), clearing it to nullptr to keep our bookkeeping straight
	inline __attribute__((always_inline)) void FreeSubpopGenome(Genome *p_genome)
	{
		if (p_genome->IsNull())
			genome_junkyard_null.emplace_back(p_genome);
		else
		{
			p_genome->clear_to_nullptr();
			genome_junkyard_nonnull.emplace_back(p_genome);
		}
	}
	
#ifdef SLIM_WF_ONLY
	void WipeIndividualsAndGenomes(std::vector<Individual *> &p_individuals, std::vector<Genome *> &p_genomes, slim_popsize_t p_individual_count, slim_popsize_t p_first_male, bool p_no_clear);
	void GenerateChildrenToFitWF(void);		// given the set subpop size and sex ratio, configure the child generation genomes and individuals to fit
#endif	// SLIM_WF_ONLY
	void GenerateParentsToFit(slim_age_t p_initial_age, double p_sex_ratio, bool p_allow_zero_size, bool p_require_both_sexes, bool p_record_in_treeseq);	// given the set subpop size and requested sex ratio, make new genomes and individuals to fit
	void CheckIndividualIntegrity(void);
	
	IndividualSex SexOfIndividual(slim_popsize_t p_individual_index);						// return the sex of the individual at the given index; uses child_generation_valid
	void UpdateFitness(std::vector<SLiMEidosBlock*> &p_fitness_callbacks, std::vector<SLiMEidosBlock*> &p_global_fitness_callbacks);	// update fitness values based upon current mutations
#ifdef SLIM_WF_ONLY
	void UpdateWFFitnessBuffers(bool p_pure_neutral);																					// update the WF model fitness buffers after UpdateFitness()
#endif	// SLIM_WF_ONLY
	
	// calculate the fitness of a given individual; the x dominance coeff is used only if the X is modeled
	double FitnessOfParentWithGenomeIndices_NoCallbacks(slim_popsize_t p_individual_index);
	double FitnessOfParentWithGenomeIndices_Callbacks(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_fitness_callbacks);
	double FitnessOfParentWithGenomeIndices_SingleCallback(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_fitness_callbacks, MutationType *p_single_callback_mut_type);
	
	double ApplyFitnessCallbacks(MutationIndex p_mutation, int p_homozygous, double p_computed_fitness, std::vector<SLiMEidosBlock*> &p_fitness_callbacks, Individual *p_individual, Genome *p_genome1, Genome *p_genome2);
	double ApplyGlobalFitnessCallbacks(std::vector<SLiMEidosBlock*> &p_fitness_callbacks, slim_popsize_t p_individual_index);
	
#ifdef SLIM_WF_ONLY
	void SwapChildAndParentGenomes(void);															// switch to the next generation by swapping; the children become the parents
#endif	// SLIM_WF_ONLY
	
#ifdef SLIM_NONWF_ONLY
	void ApplyReproductionCallbacks(std::vector<SLiMEidosBlock*> &p_reproduction_callbacks, slim_popsize_t p_individual_index);
	void ReproduceSubpopulation(void);
	void MergeReproductionOffspring(void);
	void ViabilitySelection(void);
	void IncrementIndividualAges(void);
	
	IndividualSex _GenomeConfigurationForSex(EidosValue *p_sex_value, GenomeType &p_genome1_type, GenomeType &p_genome2_type, bool &p_genome1_null, bool &p_genome2_null);
	inline __attribute__((always_inline)) EidosValue_SP _ResultAfterModifyChildCallbacks(bool p_proposed_child_accepted, Individual *p_individual, Genome *p_genome1, Genome *p_genome2)
	{
		if (p_proposed_child_accepted)
		{
			// The child was accepted, so add it to our staging area and return it to the caller
			nonWF_offspring_genomes_.push_back(p_genome1);
			nonWF_offspring_genomes_.push_back(p_genome2);
			nonWF_offspring_individuals_.push_back(p_individual);
			
			// Note we use the special EidosValue_Object_singleton() constructor that sets registered_for_patching_ to false
			// and avoids registering the EidosValue in the address-patching registries.  This is safe because address patching
			// only occurs as a side effect of takeMigrants(), and takeMigrants() cannot be called within a reproduction()
			// callback, whereas we are *only* called in reproduction() callbacks; there is no way for this unregistered
			// EidosValue to slip out to a context where it would need its address to be patched.  Avoiding the registry should
			// make a speed difference; nevertheless, a gross hack.  See EidosValue_Object::EidosValue_Object() for comments.
			EidosValue_Object_singleton *individual_value = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(p_individual, gSLiM_Individual_Class, false);
			
			return EidosValue_SP(individual_value);
		}
		else
		{
			// The child was rejected, so dispose of the genomes and individual
			FreeSubpopGenome(p_genome1);
			FreeSubpopGenome(p_genome2);
			
			p_individual->~Individual();
			individual_pool_->DisposeChunk(const_cast<Individual *>(p_individual));
			
			// TREE SEQUENCE RECORDING
			SLiMSim &sim = population_.sim_;
			
			if (sim.RecordingTreeSequence())
				sim.RetractNewIndividual();
		}
		
		return gStaticEidosValueNULL;
	}
#endif  // SLIM_NONWF_ONLY
	
	// inline accessors that handle the parent/child distinction
	inline __attribute__((always_inline)) std::vector<Individual *> &CurrentIndividuals(void)
	{
#ifdef SLIM_WF_ONLY
		return (child_generation_valid_ ? child_individuals_ : parent_individuals_);
#else
		return parent_individuals_;
#endif
	}
	inline __attribute__((always_inline)) std::vector<Genome *> &CurrentGenomes(void)
	{
#ifdef SLIM_WF_ONLY
		return (child_generation_valid_ ? child_genomes_ : parent_genomes_);
#else
		return parent_genomes_;
#endif
	}
	inline __attribute__((always_inline)) slim_popsize_t CurrentGenomeCount(void)
	{
#ifdef SLIM_WF_ONLY
		return (child_generation_valid_ ? 2*child_subpop_size_ : 2*parent_subpop_size_);
#else
		return 2*parent_subpop_size_;
#endif
	}
	inline __attribute__((always_inline)) slim_popsize_t CurrentSubpopSize(void)
	{
#ifdef SLIM_WF_ONLY
		return (child_generation_valid_ ? child_subpop_size_ : parent_subpop_size_);
#else
		return parent_subpop_size_;
#endif
	}
	inline __attribute__((always_inline)) slim_popsize_t CurrentFirstMaleIndex(void)
	{
#ifdef SLIM_WF_ONLY
		return (child_generation_valid_ ? child_first_male_index_ : parent_first_male_index_);
#else
		return parent_first_male_index_;
#endif
	}
	
	// Memory usage tallying, for outputUsage()
	size_t MemoryUsageForParentTables(void);
	
	//
	// Eidos support
	//
	inline EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; };
	
	virtual const EidosObjectClass *Class(void) const override;
	virtual void Print(std::ostream &p_ostream) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	
#ifdef SLIM_WF_ONLY
	EidosValue_SP ExecuteMethod_setMigrationRates(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setCloningRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setSelfingRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setSexRatio(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setSubpopulationSize(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
#endif	// SLIM_WF_ONLY
	
#ifdef SLIM_NONWF_ONLY
	EidosValue_SP ExecuteMethod_addCloned(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addCrossed(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addEmpty(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addRecombinant(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_addSelfed(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_removeSubpopulation(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_takeMigrants(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
#endif  // SLIM_NONWF_ONLY
	
	EidosValue_SP ExecuteMethod_pointInBounds(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointReflected(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointStopped(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointPeriodic(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointUniform(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setSpatialBounds(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_cachedFitness(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_defineSpatialMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_spatialMapColor(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_spatialMapValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputXSample(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_sampleIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_subsetIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_configureDisplay(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObjectElement for comments on this mechanism
	static EidosValue *GetProperty_Accelerated_id(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_firstMaleIndex(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_individualCount(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size);
	static EidosValue *GetProperty_Accelerated_fitnessScaling(EidosObjectElement **p_values, size_t p_values_size);
	
	static void SetProperty_Accelerated_fitnessScaling(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
	static void SetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size);
};


#ifdef SLIM_WF_ONLY
inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawParentUsingFitness(void) const
{
#if DEBUG
	if (sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawParentUsingFitness): (internal error) called on a population for which sex is enabled." << EidosTerminate();
#endif
	
	if (lookup_parent_)
		return static_cast<slim_popsize_t>(gsl_ran_discrete(EIDOS_GSL_RNG, lookup_parent_));
	else
		return static_cast<slim_popsize_t>(Eidos_rng_uniform_int(EIDOS_GSL_RNG, parent_subpop_size_));
}
#endif	// SLIM_WF_ONLY

inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawParentEqualProbability(void) const
{
#if DEBUG
	if (sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawParentEqualProbability): (internal error) called on a population for which sex is enabled." << EidosTerminate();
#endif
	
	return static_cast<slim_popsize_t>(Eidos_rng_uniform_int(EIDOS_GSL_RNG, parent_subpop_size_));
}

#ifdef SLIM_WF_ONLY
// SEX ONLY
inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawFemaleParentUsingFitness(void) const
{
#if DEBUG
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawFemaleParentUsingFitness): (internal error) called on a population for which sex is not enabled." << EidosTerminate();
#endif
	
	if (lookup_female_parent_)
		return static_cast<slim_popsize_t>(gsl_ran_discrete(EIDOS_GSL_RNG, lookup_female_parent_));
	else
		return static_cast<slim_popsize_t>(Eidos_rng_uniform_int(EIDOS_GSL_RNG, parent_first_male_index_));
}
#endif	// SLIM_WF_ONLY

// SEX ONLY
inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawFemaleParentEqualProbability(void) const
{
#if DEBUG
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawFemaleParentEqualProbability): (internal error) called on a population for which sex is not enabled." << EidosTerminate();
#endif
	
	return static_cast<slim_popsize_t>(Eidos_rng_uniform_int(EIDOS_GSL_RNG, parent_first_male_index_));
}

#ifdef SLIM_WF_ONLY
// SEX ONLY
inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawMaleParentUsingFitness(void) const
{
#if DEBUG
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawMaleParentUsingFitness): (internal error) called on a population for which sex is not enabled." << EidosTerminate();
#endif
	
	if (lookup_male_parent_)
		return static_cast<slim_popsize_t>(gsl_ran_discrete(EIDOS_GSL_RNG, lookup_male_parent_)) + parent_first_male_index_;
	else
		return static_cast<slim_popsize_t>(Eidos_rng_uniform_int(EIDOS_GSL_RNG, parent_subpop_size_ - parent_first_male_index_) + parent_first_male_index_);
}
#endif	// SLIM_WF_ONLY

// SEX ONLY
inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawMaleParentEqualProbability(void) const
{
#if DEBUG
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawMaleParentEqualProbability): (internal error) called on a population for which sex is not enabled." << EidosTerminate();
#endif
	
	return static_cast<slim_popsize_t>(Eidos_rng_uniform_int(EIDOS_GSL_RNG, parent_subpop_size_ - parent_first_male_index_) + parent_first_male_index_);
}

inline IndividualSex Subpopulation::SexOfIndividual(slim_popsize_t p_individual_index)
{
	if (!sex_enabled_)
	{
		return IndividualSex::kHermaphrodite;
	}
#ifdef SLIM_WF_ONLY
	else if (child_generation_valid_)
	{
		if (p_individual_index < child_first_male_index_)
			return IndividualSex::kFemale;
		else
			return IndividualSex::kMale;
	}
#endif	// SLIM_WF_ONLY
	else
	{
		if (p_individual_index < parent_first_male_index_)
			return IndividualSex::kFemale;
		else
			return IndividualSex::kMale;
	}
}


#endif /* defined(__SLiM__subpopulation__) */




































































