//
//  subpopulation.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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


#include <vector>

#include "slim_global.h"
#include "g_rng.h"
#include "genome.h"
#include "chromosome.h"


class Subpopulation
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	
	gsl_ran_discrete_t *lookup_parent_ = nullptr;			// OWNED POINTER: lookup table for drawing a parent based upon fitness
	gsl_ran_discrete_t *lookup_female_parent_ = nullptr;	// OWNED POINTER: lookup table for drawing a female parent based upon fitness, SEX ONLY
	gsl_ran_discrete_t *lookup_male_parent_ = nullptr;		// OWNED POINTER: lookup table for drawing a male parent based upon fitness, SEX ONLY
	
public:
	
	double selfing_fraction_ = 0.0;					// selfing fraction
	std::map<int,double> migrant_fractions_;		// m[i]: fraction made up of migrants from subpopulation i per generation
	
	std::vector<Genome> parent_genomes_;			// all genomes in the parental generation; each individual gets two genomes, males are XY (not YX)
	int parent_subpop_size_;						// parental subpopulation size
	double parent_sex_ratio_ = 0.0;					// what sex ratio the parental genomes approximate
	int parent_first_male_index_ = INT_MAX;			// the index of the first male in the parental Genome vector (NOT premultiplied by 2!); equal to the number of females
	
	std::vector<Genome> child_genomes_;				// all genomes in the child generation; each individual gets two genomes, males are XY (not YX)
	int child_subpop_size_;							// child subpopulation size
	double child_sex_ratio_ = 0.0;					// what sex ratio the child genomes approximate
	int child_first_male_index_ = INT_MAX;			// the index of the first male in the child Genome vector (NOT premultiplied by 2!); equal to the number of females
	
	// SEX ONLY; the default values here are for the non-sex case
	bool sex_enabled_ = false;										// the subpopulation needs to have easy reference to whether its individuals are sexual or not...
	GenomeType modeled_chromosome_type_ = GenomeType::kAutosome;	// ...and needs to know what type of chromosomes its individuals are modeling; this should match SLiMSim
	double x_chromosome_dominance_coeff_ = 1.0;						// the dominance coefficient for heterozygosity at the X locus (i.e. males); this is global
	
	Subpopulation(const Subpopulation&) = delete;													// no copying
	Subpopulation& operator=(const Subpopulation&) = delete;										// no copying
	explicit Subpopulation(int p_subpop_size);														// construct with a population size
	Subpopulation(int p_subpop_size, double p_sex_ratio,
				  GenomeType p_modeled_chromosome_type, double p_x_chromosome_dominance_coeff);		// SEX ONLY: construct with a sex ratio (fraction male), chromosome type (AXY), and X dominance coeff
	~Subpopulation(void);																			// destructor
	
	inline int DrawParentUsingFitness(void) const;													// draw an individual from the subpopulation based upon fitness
	inline int DrawParentEqualProbability(void) const;												// draw an individual from the subpopulation with equal probabilities
	inline int DrawFemaleParentUsingFitness(void) const;											// draw a female from the subpopulation based upon fitness; SEX ONLY
	inline int DrawFemaleParentEqualProbability(void) const;										// draw a female from the subpopulation  with equal probabilities; SEX ONLY
	inline int DrawMaleParentUsingFitness(void) const;												// draw a male from the subpopulation based upon fitness; SEX ONLY
	inline int DrawMaleParentEqualProbability(void) const;											// draw a male from the subpopulation  with equal probabilities; SEX ONLY
	
	void GenerateChildrenToFit(const bool p_parents_also);											// given the subpop size and sex ratio currently set for the child generation, make new genomes to fit
	inline IndividualSex SexOfChild(int p_child_index);													// return the sex of the child at the given index
	void UpdateFitness(void);																		// update the fitness lookup table based upon current mutations
	double FitnessOfParentWithGenomeIndices(int p_genome_index1, int p_genome_index2) const;	// calculate the fitness of a given individual; the x dominance coeff is used only if the X is modeled
	void SwapChildAndParentGenomes(void);															// switch to the next generation by swapping; the children become the parents
};


inline __attribute__((always_inline)) int Subpopulation::DrawParentUsingFitness(void) const
{
#if DEBUG
	if (sex_enabled_)
		std::cerr << "Subpopulation::DrawIndividual() called on a population for which sex is enabled" << std::endl << slim_terminate();
#endif
	
	return static_cast<int>(gsl_ran_discrete(g_rng, lookup_parent_));
}

inline __attribute__((always_inline)) int Subpopulation::DrawParentEqualProbability(void) const
{
#if DEBUG
	if (sex_enabled_)
		std::cerr << "Subpopulation::DrawIndividualEqualProbability() called on a population for which sex is enabled" << std::endl << slim_terminate();
#endif
	
	return static_cast<int>(gsl_rng_uniform_int(g_rng, parent_subpop_size_));
}

// SEX ONLY
inline __attribute__((always_inline)) int Subpopulation::DrawFemaleParentUsingFitness(void) const
{
#if DEBUG
	if (!sex_enabled_)
		std::cerr << "Subpopulation::DrawFemale() called on a population for which sex is not enabled" << std::endl << slim_terminate();
#endif
	
	return static_cast<int>(gsl_ran_discrete(g_rng, lookup_female_parent_));
}

// SEX ONLY
inline __attribute__((always_inline)) int Subpopulation::DrawFemaleParentEqualProbability(void) const
{
#if DEBUG
	if (!sex_enabled_)
		std::cerr << "Subpopulation::DrawFemaleEqualProbability() called on a population for which sex is not enabled" << std::endl << slim_terminate();
#endif
	
	return static_cast<int>(gsl_rng_uniform_int(g_rng, parent_first_male_index_));
}

// SEX ONLY
inline __attribute__((always_inline)) int Subpopulation::DrawMaleParentUsingFitness(void) const
{
#if DEBUG
	if (!sex_enabled_)
		std::cerr << "Subpopulation::DrawMale() called on a population for which sex is not enabled" << std::endl << slim_terminate();
#endif
	
	return static_cast<int>(gsl_ran_discrete(g_rng, lookup_male_parent_)) + parent_first_male_index_;
}

// SEX ONLY
inline __attribute__((always_inline)) int Subpopulation::DrawMaleParentEqualProbability(void) const
{
#if DEBUG
	if (!sex_enabled_)
		std::cerr << "Subpopulation::DrawMaleEqualProbability() called on a population for which sex is not enabled" << std::endl << slim_terminate();
#endif
	
	return static_cast<int>(gsl_rng_uniform_int(g_rng, parent_subpop_size_ - parent_first_male_index_) + parent_first_male_index_);
}

inline IndividualSex Subpopulation::SexOfChild(int p_child_index)
{
	if (!sex_enabled_)
		return IndividualSex::kHermaphrodite;
	else if (p_child_index < child_first_male_index_)
		return IndividualSex::kFemale;
	else
		return IndividualSex::kMale;
}


#endif /* defined(__SLiM__subpopulation__) */




































































