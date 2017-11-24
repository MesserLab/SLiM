//
//  subpopulation.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2016 Philipp Messer.  All rights reserved.
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
 A subpopulations also knows its size, its selfing fraction, and what fraction it receives as migrants from other subpopulations.
 
 */

#ifndef __SLiM__subpopulation__
#define __SLiM__subpopulation__


#include "slim_global.h"
#include "eidos_rng.h"
#include "genome.h"
#include "chromosome.h"
#include "eidos_value.h"
#include "slim_eidos_block.h"
#include "individual.h"
#include "slim_eidos_dictionary.h"

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
	
	double ValueAtPoint(double *p_point);
	void ColorForValue(double p_value, double *p_rgb_ptr);
	void ColorForValue(double p_value, float *p_rgb_ptr);
};
typedef struct _SpatialMap SpatialMap;

typedef std::pair<std::string, SpatialMap *> SpatialMapPair;
typedef std::map<std::string, SpatialMap *> SpatialMapMap;


#pragma mark -
#pragma mark Subpopulation
#pragma mark -

class Subpopulation : public SLiMEidosDictionary
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	
	gsl_ran_discrete_t *lookup_parent_ = nullptr;			// OWNED POINTER: lookup table for drawing a parent based upon fitness
	gsl_ran_discrete_t *lookup_female_parent_ = nullptr;	// OWNED POINTER: lookup table for drawing a female parent based upon fitness, SEX ONLY
	gsl_ran_discrete_t *lookup_male_parent_ = nullptr;		// OWNED POINTER: lookup table for drawing a male parent based upon fitness, SEX ONLY
	
	EidosSymbolTableEntry self_symbol_;						// for fast setup of the symbol table
	
public:
	
	Population &population_;						// we need to know our Population so we can remove ourselves, etc.
	
	slim_objectid_t subpopulation_id_;				// the id by which this subpopulation is indexed in the Population
	EidosValue_SP cached_value_subpop_id_;			// a cached value for subpopulation_id_; reset() if changed
	
	double selfing_fraction_ = 0.0;					// ASEX ONLY: selfing fraction, the fraction of offspring generated by self-fertilization
	double female_clone_fraction_ = 0.0;			// both asex and sex; in the asex case, male_clone_fraction_ == female_clone_fraction_ and is simply the clonal fraction
	double male_clone_fraction_ = 0.0;				//		these represent the fraction of offspring generated by asexual clonal reproduction
	
	std::map<slim_objectid_t,double> migrant_fractions_;		// m[i]: fraction made up of migrants from subpopulation i per generation
	bool child_generation_valid_ = false;			// this keeps track of whether children have been generated by EvolveSubpopulation() yet, or whether the parents are still in charge
	
	std::vector<Genome> parent_genomes_;			// all genomes in the parental generation; each individual gets two genomes, males are XY (not YX)
	EidosValue_SP cached_parent_genomes_value_;		// cached for the genomes property; reset() if changed
	slim_popsize_t parent_subpop_size_;				// parental subpopulation size
	double parent_sex_ratio_ = 0.0;					// what sex ratio the parental genomes approximate
	slim_popsize_t parent_first_male_index_ = INT_MAX;	// the index of the first male in the parental Genome vector (NOT premultiplied by 2!); equal to the number of females
	std::vector<Individual> parent_individuals_;	// objects representing simulated individuals, each of which has two genomes
	EidosValue_SP cached_parent_individuals_value_;	// cached for the individuals property; self-maintains
	
	std::vector<Genome> child_genomes_;				// all genomes in the child generation; each individual gets two genomes, males are XY (not YX)
	EidosValue_SP cached_child_genomes_value_;		// cached for the genomes property; reset() if changed
	slim_popsize_t child_subpop_size_;				// child subpopulation size
	double child_sex_ratio_ = 0.0;					// what sex ratio the child genomes approximate
	slim_popsize_t child_first_male_index_ = INT_MAX;	// the index of the first male in the child Genome vector (NOT premultiplied by 2!); equal to the number of females
	std::vector<Individual> child_individuals_;		// objects representing simulated individuals, each of which has two genomes
	EidosValue_SP cached_child_individuals_value_;	// cached for the individuals property; self-maintains
	
	std::vector<SLiMEidosBlock*> registered_mate_choice_callbacks_;	// NOT OWNED: valid only during EvolveSubpopulation; callbacks used when this subpop is parental
	std::vector<SLiMEidosBlock*> registered_modify_child_callbacks_;	// NOT OWNED: valid only during EvolveSubpopulation; callbacks used when this subpop is parental
	std::vector<SLiMEidosBlock*> registered_recombination_callbacks_;	// NOT OWNED: valid only during EvolveSubpopulation; callbacks used when this subpop is parental
	
	double *cached_parental_fitness_ = nullptr;		// OWNED POINTER: cached in UpdateFitness(), used by SLiMgui and by the fitness() methods of Subpopulation
	double *cached_male_fitness_ = nullptr;			// OWNED POINTER: SEX ONLY: same as cached_parental_fitness_ but with 0 for all females
	slim_popsize_t cached_fitness_size_ = 0;		// the size (number of entries used) of cached_parental_fitness_ and cached_male_fitness_
	slim_popsize_t cached_fitness_capacity_ = 0;	// the capacity of the malloced buffers cached_parental_fitness_ and cached_male_fitness_
	
	// SEX ONLY; the default values here are for the non-sex case
	bool sex_enabled_ = false;										// the subpopulation needs to have easy reference to whether its individuals are sexual or not...
	GenomeType modeled_chromosome_type_ = GenomeType::kAutosome;	// ...and needs to know what type of chromosomes its individuals are modeling; this should match SLiMSim
	double x_chromosome_dominance_coeff_ = 1.0;						// the dominance coefficient for heterozygosity at the X locus (i.e. males); this is global
	
	// continuous-space info
	double bounds_x0_ = 0.0, bounds_x1_ = 1.0;
	double bounds_y0_ = 0.0, bounds_y1_ = 1.0;
	double bounds_z0_ = 0.0, bounds_z1_ = 1.0;
	SpatialMapMap spatial_maps_;
	
	slim_usertag_t tag_value_;										// a user-defined tag value
	
#ifdef SLIMGUI
	bool gui_selected_ = false;							// keeps track of whether we are selected in SLiMgui's table of subpopulations; note Population::gui_all_selected_ must be kept in synch!
	double parental_total_fitness_ = 0.0;				// updated in UpdateFitness() when running under SLiMgui
	double gui_center_x_, gui_center_y_, gui_radius_;	// used as scratch space by GraphView_PopulationVisualization
#endif
	
	Subpopulation(const Subpopulation&) = delete;													// no copying
	Subpopulation& operator=(const Subpopulation&) = delete;										// no copying
	Subpopulation(void) = delete;																	// no null construction
	Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size);	// construct with a population size
	Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size, double p_sex_ratio,
				  GenomeType p_modeled_chromosome_type, double p_x_chromosome_dominance_coeff);		// SEX ONLY: construct with a sex ratio (fraction male), chromosome type (AXY), and X dominance coeff
	~Subpopulation(void);																			// destructor
	
	inline slim_popsize_t DrawParentUsingFitness(void) const;										// draw an individual from the subpopulation based upon fitness
	inline slim_popsize_t DrawParentEqualProbability(void) const;									// draw an individual from the subpopulation with equal probabilities
	inline slim_popsize_t DrawFemaleParentUsingFitness(void) const;									// draw a female from the subpopulation based upon fitness; SEX ONLY
	inline slim_popsize_t DrawFemaleParentEqualProbability(void) const;								// draw a female from the subpopulation  with equal probabilities; SEX ONLY
	inline slim_popsize_t DrawMaleParentUsingFitness(void) const;									// draw a male from the subpopulation based upon fitness; SEX ONLY
	inline slim_popsize_t DrawMaleParentEqualProbability(void) const;								// draw a male from the subpopulation  with equal probabilities; SEX ONLY
	
	void GenerateChildrenToFit(const bool p_parents_also);											// given the subpop size and sex ratio currently set for the child generation, make new genomes to fit
	inline IndividualSex SexOfIndividual(slim_popsize_t p_individual_index);						// return the sex of the individual at the given index; uses child_generation_valid
	void UpdateFitness(std::vector<SLiMEidosBlock*> &p_fitness_callbacks, std::vector<SLiMEidosBlock*> &p_global_fitness_callbacks);							// update the fitness lookup table based upon current mutations
	
	// calculate the fitness of a given individual; the x dominance coeff is used only if the X is modeled
	double FitnessOfParentWithGenomeIndices_NoCallbacks(slim_popsize_t p_individual_index);
	double FitnessOfParentWithGenomeIndices_Callbacks(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_fitness_callbacks);
	double FitnessOfParentWithGenomeIndices_SingleCallback(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_fitness_callbacks, MutationType *p_single_callback_mut_type);
	
	double ApplyFitnessCallbacks(MutationIndex p_mutation, int p_homozygous, double p_computed_fitness, std::vector<SLiMEidosBlock*> &p_fitness_callbacks, Individual *p_individual, Genome *p_genome1, Genome *p_genome2);
	double ApplyGlobalFitnessCallbacks(std::vector<SLiMEidosBlock*> &p_fitness_callbacks, slim_popsize_t p_individual_index);
	
	void SwapChildAndParentGenomes(void);															// switch to the next generation by swapping; the children become the parents
	
	//
	// Eidos support
	//
	EidosSymbolTableEntry &SymbolTableEntry(void) { return self_symbol_; };
	
	virtual const EidosObjectClass *Class(void) const;
	virtual void Print(std::ostream &p_ostream) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setMigrationRates(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointInBounds(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointReflected(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointStopped(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointPeriodic(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_pointUniform(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setCloningRate(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setSelfingRate(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setSexRatio(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setSpatialBounds(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setSubpopulationSize(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_cachedFitness(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_defineSpatialMap(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_spatialMapColor(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_spatialMapValue(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_outputXSample(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	
	// Accelerated property access; see class EidosObjectElement for comments on this mechanism
	virtual int64_t GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id);
	virtual double GetProperty_Accelerated_Float(EidosGlobalStringID p_property_id);
};


inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawParentUsingFitness(void) const
{
#if DEBUG
	if (sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawParentUsingFitness): called on a population for which sex is enabled." << EidosTerminate();
#endif
	
	if (lookup_parent_)
		return static_cast<slim_popsize_t>(gsl_ran_discrete(gEidos_rng, lookup_parent_));
	else
		return static_cast<slim_popsize_t>(Eidos_RandomInt(gEidos_rng, parent_subpop_size_));
}

inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawParentEqualProbability(void) const
{
#if DEBUG
	if (sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawParentEqualProbability): called on a population for which sex is enabled." << EidosTerminate();
#endif
	
	return static_cast<slim_popsize_t>(Eidos_RandomInt(gEidos_rng, parent_subpop_size_));
}

// SEX ONLY
inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawFemaleParentUsingFitness(void) const
{
#if DEBUG
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawFemaleParentUsingFitness): called on a population for which sex is not enabled." << EidosTerminate();
#endif
	
	if (lookup_female_parent_)
		return static_cast<slim_popsize_t>(gsl_ran_discrete(gEidos_rng, lookup_female_parent_));
	else
		return static_cast<slim_popsize_t>(Eidos_RandomInt(gEidos_rng, parent_first_male_index_));
}

// SEX ONLY
inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawFemaleParentEqualProbability(void) const
{
#if DEBUG
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawFemaleParentEqualProbability): called on a population for which sex is not enabled." << EidosTerminate();
#endif
	
	return static_cast<slim_popsize_t>(Eidos_RandomInt(gEidos_rng, parent_first_male_index_));
}

// SEX ONLY
inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawMaleParentUsingFitness(void) const
{
#if DEBUG
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawMaleParentUsingFitness): called on a population for which sex is not enabled." << EidosTerminate();
#endif
	
	if (lookup_male_parent_)
		return static_cast<slim_popsize_t>(gsl_ran_discrete(gEidos_rng, lookup_male_parent_)) + parent_first_male_index_;
	else
		return static_cast<slim_popsize_t>(Eidos_RandomInt(gEidos_rng, parent_subpop_size_ - parent_first_male_index_) + parent_first_male_index_);
}

// SEX ONLY
inline __attribute__((always_inline)) slim_popsize_t Subpopulation::DrawMaleParentEqualProbability(void) const
{
#if DEBUG
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::DrawMaleParentEqualProbability): called on a population for which sex is not enabled." << EidosTerminate();
#endif
	
	return static_cast<slim_popsize_t>(Eidos_RandomInt(gEidos_rng, parent_subpop_size_ - parent_first_male_index_) + parent_first_male_index_);
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


#endif /* defined(__SLiM__subpopulation__) */




































































