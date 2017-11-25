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

struct GESubrange;


extern EidosObjectClass *gSLiM_Chromosome_Class;


class Chromosome : private std::vector<GenomicElement>, public EidosObjectElement
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

#ifdef SLIMGUI
public:
#else
private:
#endif
	
	// We now allow different recombination maps for males and females, optionally.  Unfortunately, this means we have a bit of an
	// explosion of state involved with recombination.  We now have _H, _M, and _F versions of many ivars.  The _M and _F versions
	// are used if sex is enabled and different recombination maps have been specified for males versus females.  The _H version
	// is used in all other cases – when sex is not enabled (i.e., hermaphrodites), and when separate maps have not been specified.
	// This flag indicates which option has been chosen; after initialize() time this cannot be changed.
	bool single_recombination_map_ = true;
	
	// The same is now true for mutation rate maps as well; we now have _H, _M, and _F versions of those, just as with recombination
	// maps.  This flag indicates which option has been chosen; after initialize() time this cannot be changed.
	bool single_mutation_map_ = true;
	
	gsl_ran_discrete_t *lookup_mutation_H_ = nullptr;		// OWNED POINTER: lookup table for drawing mutations
	gsl_ran_discrete_t *lookup_mutation_M_ = nullptr;
	gsl_ran_discrete_t *lookup_mutation_F_ = nullptr;
	
	gsl_ran_discrete_t *lookup_recombination_H_ = nullptr;	// OWNED POINTER: lookup table for drawing recombination breakpoints
	gsl_ran_discrete_t *lookup_recombination_M_ = nullptr;
	gsl_ran_discrete_t *lookup_recombination_F_ = nullptr;
	
	// caches to speed up Poisson draws in CrossoverMutation()
	double exp_neg_overall_mutation_rate_H_;			
	double exp_neg_overall_mutation_rate_M_;
	double exp_neg_overall_mutation_rate_F_;
	
	double exp_neg_overall_recombination_rate_H_;			
	double exp_neg_overall_recombination_rate_M_;
	double exp_neg_overall_recombination_rate_F_;
	
#ifndef USE_GSL_POISSON
	// joint probabilities, used to accelerate drawing a mutation count and breakpoint count jointly
	double probability_both_0_H_;
	double probability_both_0_OR_mut_0_break_non0_H_;
	double probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_H_;
	
	double probability_both_0_M_;
	double probability_both_0_OR_mut_0_break_non0_M_;
	double probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_;
	
	double probability_both_0_F_;
	double probability_both_0_OR_mut_0_break_non0_F_;
	double probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_;
#endif
	
	// GESubrange vectors used to facilitate new mutation generation – draw a subrange, then draw a position inside the subrange
	vector<GESubrange> mutation_subranges_H_;
	vector<GESubrange> mutation_subranges_M_;
	vector<GESubrange> mutation_subranges_F_;
	
public:
	
	// We use private inheritance from std::vector<GenomicElement> to avoid issues with Chromosome being treated polymorphically
	// as a std::vector, and forward only the minimal set of std::vector methods that users of Chromosome actually want.
	// See http://stackoverflow.com/questions/4353203/thou-shalt-not-inherit-from-stdvector for discussion.
	using std::vector<GenomicElement>::emplace_back;
	using std::vector<GenomicElement>::begin;
	using std::vector<GenomicElement>::end;
	
	vector<slim_position_t> mutation_end_positions_H_;		// end positions of each defined mutation region (BEFORE intersection with GEs)
	vector<slim_position_t> mutation_end_positions_M_;
	vector<slim_position_t> mutation_end_positions_F_;
	
	vector<double> mutation_rates_H_;						// mutation rates, in events per base pair (BEFORE intersection with GEs)
	vector<double> mutation_rates_M_;
	vector<double> mutation_rates_F_;
	
	vector<slim_position_t> recombination_end_positions_H_;	// end positions of each defined recombination region
	vector<slim_position_t> recombination_end_positions_M_;
	vector<slim_position_t> recombination_end_positions_F_;
	
	vector<double> recombination_rates_H_;					// recombination rates, in events per base pair
	vector<double> recombination_rates_M_;
	vector<double> recombination_rates_F_;
	
	slim_position_t last_position_;							// last position; used to be called length_ but it is (length - 1) really
	EidosValue_SP cached_value_lastpos_;					// a cached value for last_position_; reset() if that changes
	
	double overall_mutation_rate_H_;						// overall mutation rate (AFTER intersection with GEs)
	double overall_mutation_rate_M_;						// overall mutation rate
	double overall_mutation_rate_F_;						// overall mutation rate
	
	double overall_recombination_rate_H_;					// overall recombination rate
	double overall_recombination_rate_M_;					// overall recombination rate
	double overall_recombination_rate_F_;					// overall recombination rate
	
	double gene_conversion_fraction_;						// gene conversion fraction
	double gene_conversion_avg_length_;						// average gene conversion stretch length
	
	int32_t mutrun_count_;									// number of mutation runs being used for all genomes
	int32_t mutrun_length_;									// the length, in base pairs, of each mutation run; the last run may not use its full length
	slim_position_t last_position_mutrun_;					// (mutrun_count_ * mutrun_length_ - 1), for complete coverage in crossover-mutation
	
	std::string color_sub_;										// color to use for substitutions by default (in SLiMgui)
	float color_sub_red_, color_sub_green_, color_sub_blue_;	// cached color components from color_sub_; should always be in sync
	
	slim_usertag_t tag_value_;								// a user-defined tag value
	
	Chromosome(const Chromosome&) = delete;									// no copying
	Chromosome& operator=(const Chromosome&) = delete;						// no copying
	Chromosome(void);														// supplied null constructor
	~Chromosome(void);														// destructor
	
	// initialize the random lookup tables used by Chromosome to draw mutation and recombination events
	void InitializeDraws(void);
	void _InitializeOneRecombinationMap(gsl_ran_discrete_t *&p_lookup, vector<slim_position_t> &p_end_positions, vector<double> &p_rates, double &p_overall_rate, double &p_exp_neg_overall_rate);
	void _InitializeOneMutationMap(gsl_ran_discrete_t *&p_lookup, vector<slim_position_t> &p_end_positions, vector<double> &p_rates, double &p_overall_rate, double &p_exp_neg_overall_rate, vector<GESubrange> &p_subranges);
	void ChooseMutationRunLayout(int p_preferred_count);
	
	// draw the number of mutations that occur, based on the overall mutation rate
	int DrawMutationCount(IndividualSex p_sex) const;
	
	// draw a new mutation, based on the genomic element types present and their mutational proclivities
	MutationIndex DrawNewMutation(IndividualSex p_sex, slim_objectid_t p_subpop_index, slim_generation_t p_generation) const;
	
	// draw the number of breakpoints that occur, based on the overall recombination rate
	int DrawBreakpointCount(IndividualSex p_sex) const;
	
	// choose a set of recombination breakpoints, based on recomb. intervals, overall recomb. rate, and gene conversion probability
	void DrawBreakpoints(IndividualSex p_sex, const int p_num_breakpoints, std::vector<slim_position_t> &p_crossovers) const;
	void DrawBreakpoints_Detailed(IndividualSex p_sex, const int p_num_breakpoints, vector<slim_position_t> &p_crossovers, vector<slim_position_t> &p_gcstarts, vector<slim_position_t> &p_gcends) const;
	
#ifndef USE_GSL_POISSON
	// draw both the mutation count and breakpoint count, using a single Poisson draw for speed
	void DrawMutationAndBreakpointCounts(IndividualSex p_sex, int *p_mut_count, int *p_break_count) const;
	
	// initialize the joint probabilities used by DrawMutationAndBreakpointCounts()
	void _InitializeJointProbabilities(double p_overall_mutation_rate, double p_exp_neg_overall_mutation_rate,
												   double p_overall_recombination_rate, double p_exp_neg_overall_recombination_rate,
												   double &p_both_0, double &p_both_0_OR_mut_0_break_non0, double &p_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0);
#endif
	
	// internal methods for throwing errors from inline functions when assumptions about the configuration of maps are violated
	void MutationMapConfigError(void) const __attribute__((__noreturn__)) __attribute__((cold)) __attribute__((analyzer_noreturn));
	void RecombinationMapConfigError(void) const __attribute__((__noreturn__)) __attribute__((cold)) __attribute__((analyzer_noreturn));
	
	
	//
	// Eidos support
	//
	virtual const EidosObjectClass *Class(void) const;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id);
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value);
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setMutationRate(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setRecombinationRate(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter);
};

// draw the number of mutations that occur, based on the overall mutation rate
inline __attribute__((always_inline)) int Chromosome::DrawMutationCount(IndividualSex p_sex) const
{
#ifdef USE_GSL_POISSON
	if (single_mutation_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		return gsl_ran_poisson(gEidos_rng, overall_mutation_rate_H_);
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			return gsl_ran_poisson(gEidos_rng, overall_mutation_rate_M_);
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			return gsl_ran_poisson(gEidos_rng, overall_mutation_rate_F_);
		}
		else
		{
			MutationMapConfigError();
		}
	}
#else
	if (single_mutation_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		return Eidos_FastRandomPoisson(overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_);
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			return Eidos_FastRandomPoisson(overall_mutation_rate_M_, exp_neg_overall_mutation_rate_M_);
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			return Eidos_FastRandomPoisson(overall_mutation_rate_F_, exp_neg_overall_mutation_rate_F_);
		}
		else
		{
			MutationMapConfigError();
		}
	}
#endif
}

// draw the number of breakpoints that occur, based on the overall recombination rate
inline __attribute__((always_inline)) int Chromosome::DrawBreakpointCount(IndividualSex p_sex) const
{
#ifdef USE_GSL_POISSON
	if (single_recombination_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		return gsl_ran_poisson(gEidos_rng, overall_recombination_rate_H_);
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			return gsl_ran_poisson(gEidos_rng, overall_recombination_rate_M_);
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			return gsl_ran_poisson(gEidos_rng, overall_recombination_rate_F_);
		}
		else
		{
			RecombinationMapConfigError();
		}
	}
#else
	if (single_recombination_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		return Eidos_FastRandomPoisson(overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_);
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			return Eidos_FastRandomPoisson(overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_);
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			return Eidos_FastRandomPoisson(overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_);
		}
		else
		{
			RecombinationMapConfigError();
		}
	}
#endif
}

#ifndef USE_GSL_POISSON
// determine both the mutation count and the breakpoint count with (usually) a single RNG draw
// this method relies on Eidos_FastRandomPoisson_NONZERO() and cannot be called when USE_GSL_POISSON is defined
inline __attribute__((always_inline)) void Chromosome::DrawMutationAndBreakpointCounts(IndividualSex p_sex, int *p_mut_count, int *p_break_count) const
{
	double u = gsl_rng_uniform(gEidos_rng);
	
	if (single_recombination_map_ && single_mutation_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled.
		// We use the _H_ variants of all ivars in this case, which are all that is set up.
		if (u <= probability_both_0_H_)
		{
			*p_mut_count = 0;
			*p_break_count = 0;
		}
		else if (u <= probability_both_0_OR_mut_0_break_non0_H_)
		{
			*p_mut_count = 0;
			*p_break_count = Eidos_FastRandomPoisson_NONZERO(overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_);
		}
		else if (u <= probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_H_)
		{
			*p_mut_count = Eidos_FastRandomPoisson_NONZERO(overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_);
			*p_break_count = 0;
		}
		else
		{
			*p_mut_count = Eidos_FastRandomPoisson_NONZERO(overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_);
			*p_break_count = Eidos_FastRandomPoisson_NONZERO(overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_);
		}
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two.
		// We use the _M_ and _F_ variants in this case; either the mutation map or the recombination map might be non-sex-specific,
		// so the _H_ variants were originally set up, but InitializeDraws() copies them into the _M_ and _F_ variants for us
		// so that we don't have to worry about finding the correct variant for each subcase of single/double maps.
		if (p_sex == IndividualSex::kMale)
		{
			if (u <= probability_both_0_M_)
			{
				*p_mut_count = 0;
				*p_break_count = 0;
			}
			else if (u <= probability_both_0_OR_mut_0_break_non0_M_)
			{
				*p_mut_count = 0;
				*p_break_count = Eidos_FastRandomPoisson_NONZERO(overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_);
			}
			else if (u <= probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_)
			{
				*p_mut_count = Eidos_FastRandomPoisson_NONZERO(overall_mutation_rate_M_, exp_neg_overall_mutation_rate_M_);
				*p_break_count = 0;
			}
			else
			{
				*p_mut_count = Eidos_FastRandomPoisson_NONZERO(overall_mutation_rate_M_, exp_neg_overall_mutation_rate_M_);
				*p_break_count = Eidos_FastRandomPoisson_NONZERO(overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_);
			}
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			if (u <= probability_both_0_F_)
			{
				*p_mut_count = 0;
				*p_break_count = 0;
			}
			else if (u <= probability_both_0_OR_mut_0_break_non0_F_)
			{
				*p_mut_count = 0;
				*p_break_count = Eidos_FastRandomPoisson_NONZERO(overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_);
			}
			else if (u <= probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_)
			{
				*p_mut_count = Eidos_FastRandomPoisson_NONZERO(overall_mutation_rate_F_, exp_neg_overall_mutation_rate_F_);
				*p_break_count = 0;
			}
			else
			{
				*p_mut_count = Eidos_FastRandomPoisson_NONZERO(overall_mutation_rate_F_, exp_neg_overall_mutation_rate_F_);
				*p_break_count = Eidos_FastRandomPoisson_NONZERO(overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_);
			}
		}
		else
		{
			RecombinationMapConfigError();
		}
	}
}
#endif


#endif /* defined(__SLiM__chromosome__) */




































































