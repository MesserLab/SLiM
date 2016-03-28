//
//  chromosome.h
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
 
 The class Chromosome represents an entire chromosome.  Only the portions of the chromosome that are relevant to the simulation are
 explicitly modeled, so in practice, a chromosome is a vector of genomic elements defined by the input file.  A chromosome also has
 a length, an overall mutation rate, an overall recombination rate, and parameters related to gene conversion.
 
 */

#ifndef __SLiM__chromosome__
#define __SLiM__chromosome__


#include <vector>
#include <map>

#include "mutation.h"
#include "mutation_type.h"
#include "genomic_element.h"
#include "genomic_element_type.h"
#include "eidos_rng.h"
#include "eidos_value.h"


extern EidosObjectClass *gSLiM_Chromosome_Class;


class Chromosome : private std::vector<GenomicElement>, public EidosObjectElement
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

private:
	
	gsl_ran_discrete_t *lookup_mutation_ = nullptr;			// OWNED POINTER: lookup table for drawing mutations
	gsl_ran_discrete_t *lookup_recombination_ = nullptr;	// OWNED POINTER: lookup table for drawing recombination breakpoints
	
	// caches to speed up Poisson draws in CrossoverMutation()
	double exp_neg_element_mutation_rate_;
	double exp_neg_overall_recombination_rate_;
	
	double probability_both_0_;
	double probability_both_0_OR_mut_0_break_non0_;
	double probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_;
	
public:
	
	// We use private inheritance from std::vector<GenomicElement> to avoid issues with Chromosome being treated polymorphically
	// as a std::vector, and forward only the minimal set of std::vector methods that users of Chromosome actually want.
	// See http://stackoverflow.com/questions/4353203/thou-shalt-not-inherit-from-stdvector for discussion.
	using std::vector<GenomicElement>::emplace_back;
	using std::vector<GenomicElement>::begin;
	using std::vector<GenomicElement>::end;
	
	vector<slim_position_t> recombination_end_positions_;	// end positions of each defined recombination region
	vector<double> recombination_rates_;					// recombination rates, in events per base pair
	
	slim_position_t last_position_;							// last position; used to be called length_ but it is (length - 1) really
	EidosValue_SP cached_value_lastpos_;					// a cached value for last_position_; reset() if that changes
	
	double overall_mutation_rate_;							// overall mutation rate, as specified by initializeMutationRate()
	double element_mutation_rate_;							// overall rate * number of nucleotides in elements; the practical mutation rate for SLiM
	double overall_recombination_rate_;						// overall recombination rate
	double gene_conversion_fraction_;						// gene conversion fraction
	double gene_conversion_avg_length_;						// average gene conversion stretch length
	
	slim_usertag_t tag_value_;								// a user-defined tag value
	
	Chromosome(const Chromosome&) = delete;									// no copying
	Chromosome& operator=(const Chromosome&) = delete;						// no copying
	Chromosome(void);														// supplied null constructor
	~Chromosome(void);														// destructor
	
	void InitializeDraws(void);												// initialize the random lookup tables used by Chromosome to draw mutation and recombination events
	int DrawMutationCount(void) const;										// draw the number of mutations that occur, based on the overall mutation rate
	Mutation *DrawNewMutation(slim_objectid_t p_subpop_index, slim_generation_t p_generation) const;	// draw a new mutation, based on the genomic element types present and their mutational proclivities
	int DrawBreakpointCount(void) const;									// draw the number of breakpoints that occur, based on the overall recombination rate
	vector<slim_position_t> DrawBreakpoints(const int p_num_breakpoints) const;	// choose a set of recombination breakpoints, based on recomb. intervals, overall recomb. rate, and gene conversion probability
	void DrawMutationAndBreakpointCounts(int *p_mut_count, int *p_break_count) const;
	
	//
	// Eidos support
	//
	virtual const EidosObjectClass *Class(void) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};

// draw the number of mutations that occur, based on the overall mutation rate
inline __attribute__((always_inline)) int Chromosome::DrawMutationCount(void) const
{
#ifdef USE_GSL_POISSON
	return gsl_ran_poisson(gEidos_rng, element_mutation_rate_);
#else
	return eidos_fast_ran_poisson(element_mutation_rate_, exp_neg_element_mutation_rate_);
#endif
}

// draw the number of breakpoints that occur, based on the overall recombination rate
inline __attribute__((always_inline)) int Chromosome::DrawBreakpointCount(void) const
{
#ifdef USE_GSL_POISSON
	return gsl_ran_poisson(gEidos_rng, overall_recombination_rate_);
#else
	return eidos_fast_ran_poisson(overall_recombination_rate_, exp_neg_overall_recombination_rate_);
#endif
}

// determine both the mutation count and the breakpoint count with (usually) a single RNG draw
inline __attribute__((always_inline)) void Chromosome::DrawMutationAndBreakpointCounts(int *p_mut_count, int *p_break_count) const
{
	double u = gsl_rng_uniform(gEidos_rng);
	
	if (u <= probability_both_0_)
	{
		*p_mut_count = 0;
		*p_break_count = 0;
	}
	else if (u <= probability_both_0_OR_mut_0_break_non0_)
	{
		*p_mut_count = 0;
		
#ifdef USE_GSL_POISSON
		*p_break_count = gsl_ran_poisson(gEidos_rng, overall_recombination_rate_);
#else
		*p_break_count = eidos_fast_ran_poisson_nonzero(overall_recombination_rate_, exp_neg_overall_recombination_rate_);
#endif
	}
	else if (u <= probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_)
	{
#ifdef USE_GSL_POISSON
		*p_mut_count = gsl_ran_poisson(gEidos_rng, element_mutation_rate_);
#else
		*p_mut_count = eidos_fast_ran_poisson_nonzero(element_mutation_rate_, exp_neg_element_mutation_rate_);
#endif
		
		*p_break_count = 0;
	}
	else
	{
#ifdef USE_GSL_POISSON
		*p_mut_count = gsl_ran_poisson(gEidos_rng, element_mutation_rate_);
		*p_break_count = gsl_ran_poisson(gEidos_rng, overall_recombination_rate_);
#else
		*p_mut_count = eidos_fast_ran_poisson_nonzero(element_mutation_rate_, exp_neg_element_mutation_rate_);
		*p_break_count = eidos_fast_ran_poisson_nonzero(overall_recombination_rate_, exp_neg_overall_recombination_rate_);
#endif
	}
}


#endif /* defined(__SLiM__chromosome__) */




































































