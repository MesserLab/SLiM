//
//  chromosome.h
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
class Genome;
class SLiMSim;


extern EidosObjectClass *gSLiM_Chromosome_Class;


class Chromosome : public EidosObjectElement
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

#ifdef SLIMGUI
public:
#else
private:
#endif
	
	std::vector<GenomicElement *> genomic_elements_;		// OWNED POINTERS: genomic elements belong to the chromosome
	SLiMSim *sim_;
	
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
	std::vector<GESubrange> mutation_subranges_H_;
	std::vector<GESubrange> mutation_subranges_M_;
	std::vector<GESubrange> mutation_subranges_F_;
	
public:
	
	std::vector<slim_position_t> mutation_end_positions_H_;		// end positions of each defined mutation region (BEFORE intersection with GEs)
	std::vector<slim_position_t> mutation_end_positions_M_;
	std::vector<slim_position_t> mutation_end_positions_F_;
	
	std::vector<double> mutation_rates_H_;						// mutation rates, in events per base pair (BEFORE intersection with GEs)
	std::vector<double> mutation_rates_M_;
	std::vector<double> mutation_rates_F_;
	
	std::vector<slim_position_t> recombination_end_positions_H_;	// end positions of each defined recombination region
	std::vector<slim_position_t> recombination_end_positions_M_;
	std::vector<slim_position_t> recombination_end_positions_F_;
	
	std::vector<double> recombination_rates_H_;					// recombination rates, in probability of crossover per base pair (user-specified)
	std::vector<double> recombination_rates_M_;
	std::vector<double> recombination_rates_F_;
	
	bool any_recombination_rates_05_ = false;				// set to T if any recombination rate is 0.5; those are excluded from gene conversion
	
	slim_position_t last_position_;							// last position; used to be called length_ but it is (length - 1) really
	EidosValue_SP cached_value_lastpos_;					// a cached value for last_position_; reset() if that changes
	
	double overall_mutation_rate_H_;						// overall mutation rate (AFTER intersection with GEs)
	double overall_mutation_rate_M_;						// overall mutation rate
	double overall_mutation_rate_F_;						// overall mutation rate
	
	double overall_mutation_rate_H_userlevel_;				// requested (un-adjusted) overall mutation rate (AFTER intersection with GEs)
	double overall_mutation_rate_M_userlevel_;				// requested (un-adjusted) overall mutation rate
	double overall_mutation_rate_F_userlevel_;				// requested (un-adjusted) overall mutation rate
	
	double overall_recombination_rate_H_;					// overall recombination rate (reparameterized; see _InitializeOneRecombinationMap)
	double overall_recombination_rate_M_;					// overall recombination rate (reparameterized; see _InitializeOneRecombinationMap)
	double overall_recombination_rate_F_;					// overall recombination rate (reparameterized; see _InitializeOneRecombinationMap)
	
	double overall_recombination_rate_H_userlevel_;			// overall recombination rate (unreparameterized; see _InitializeOneRecombinationMap)
	double overall_recombination_rate_M_userlevel_;			// overall recombination rate (unreparameterized; see _InitializeOneRecombinationMap)
	double overall_recombination_rate_F_userlevel_;			// overall recombination rate (unreparameterized; see _InitializeOneRecombinationMap)
	
	bool using_DSB_model_;									// if true, we are using the DSB recombination model, involving the variables below
	double non_crossover_fraction_;							// fraction of DSBs that do not result in crossover
	double gene_conversion_avg_length_;						// average gene conversion stretch length
	double gene_conversion_inv_half_length_;				// 1.0 / (gene_conversion_avg_length_ / 2.0), used for geometric draws
	double simple_conversion_fraction_;						// fraction of gene conversion tracts that are "simple" (no heteroduplex mismatche repair)
	double mismatch_repair_bias_;							// GC bias in heteroduplex mismatch repair in complex gene conversion tracts
	
	int32_t mutrun_count_;									// number of mutation runs being used for all genomes
	slim_position_t mutrun_length_;							// the length, in base pairs, of each mutation run; the last run may not use its full length
	slim_position_t last_position_mutrun_;					// (mutrun_count_ * mutrun_length_ - 1), for complete coverage in crossover-mutation
	
	std::string color_sub_;										// color to use for substitutions by default (in SLiMgui)
	float color_sub_red_, color_sub_green_, color_sub_blue_;	// cached color components from color_sub_; should always be in sync
	
	// nucleotide-based models
	NucleotideArray *ancestral_seq_buffer_ = nullptr;
	std::vector<slim_position_t> hotspot_end_positions_H_;		// end positions of each defined hotspot region (BEFORE intersection with GEs)
	std::vector<slim_position_t> hotspot_end_positions_M_;
	std::vector<slim_position_t> hotspot_end_positions_F_;
	std::vector<double> hotspot_multipliers_H_;					// hotspot multipliers (BEFORE intersection with GEs)
	std::vector<double> hotspot_multipliers_M_;
	std::vector<double> hotspot_multipliers_F_;
	
	slim_usertag_t tag_value_ = SLIM_TAG_UNSET_VALUE;			// a user-defined tag value
	
	Chromosome(const Chromosome&) = delete;									// no copying
	Chromosome& operator=(const Chromosome&) = delete;						// no copying
	Chromosome(void) = delete;												// no null constructor
	explicit Chromosome(SLiMSim *p_sim);									// construct with a simulation object
	~Chromosome(void);														// destructor
	
	inline __attribute__((always_inline)) std::vector<GenomicElement *> &GenomicElements(void)			{ return genomic_elements_; }
	inline __attribute__((always_inline)) NucleotideArray *AncestralSequence(void)						{ return ancestral_seq_buffer_; }
	
	// initialize the random lookup tables used by Chromosome to draw mutation and recombination events
	void InitializeDraws(void);
	void _InitializeOneRecombinationMap(gsl_ran_discrete_t *&p_lookup, std::vector<slim_position_t> &p_end_positions, std::vector<double> &p_rates, double &p_overall_rate, double &p_exp_neg_overall_rate, double &p_overall_rate_userlevel);
	void _InitializeOneMutationMap(gsl_ran_discrete_t *&p_lookup, std::vector<slim_position_t> &p_end_positions, std::vector<double> &p_rates, double &p_requested_overall_rate, double &p_overall_rate, double &p_exp_neg_overall_rate, std::vector<GESubrange> &p_subranges);
	void ChooseMutationRunLayout(int p_preferred_count);
	
	inline bool UsingSingleRecombinationMap(void) const { return single_recombination_map_; }
	inline bool UsingSingleMutationMap(void) const { return single_mutation_map_; }
	inline size_t GenomicElementCount(void) const { return genomic_elements_.size(); }
	
	// draw the number of mutations that occur, based on the overall mutation rate
	int DrawMutationCount(IndividualSex p_sex) const;
	
	// draw a vector of mutation positions (and the corresponding GenomicElementType objects), which is sorted and uniqued for the caller
	int DrawSortedUniquedMutationPositions(int p_count, IndividualSex p_sex, std::vector<std::pair<slim_position_t, GenomicElement *>> &p_positions);
	
	// draw a new mutation, based on the genomic element types present and their mutational proclivities
	MutationIndex DrawNewMutation(std::pair<slim_position_t, GenomicElement *> &p_position, slim_objectid_t p_subpop_index, slim_generation_t p_generation) const;
	
	// draw a new mutation with reference to the genomic background upon which it is occurring, for nucleotide-based models and/or mutation() callbacks
	Mutation *ApplyMutationCallbacks(Mutation *p_mut, Genome *p_genome, GenomicElement *p_genomic_element, int8_t p_original_nucleotide, std::vector<SLiMEidosBlock*> &p_mutation_callbacks) const;
	MutationIndex DrawNewMutationExtended(std::pair<slim_position_t, GenomicElement *> &p_position, slim_objectid_t p_subpop_index, slim_generation_t p_generation, Genome *parent_genome_1, Genome *parent_genome_2, std::vector<slim_position_t> *all_breakpoints, std::vector<SLiMEidosBlock*> *p_mutation_callbacks) const;
	
	// draw the number of breakpoints that occur, based on the overall recombination rate
	int DrawBreakpointCount(IndividualSex p_sex) const;
	
	// choose a set of recombination breakpoints, based on recomb. intervals, overall recomb. rate, and gene conversion parameters
	void DrawCrossoverBreakpoints(IndividualSex p_parent_sex, const int p_num_breakpoints, std::vector<slim_position_t> &p_crossovers) const;
	void DrawDSBBreakpoints(IndividualSex p_parent_sex, const int p_num_breakpoints, std::vector<slim_position_t> &p_crossovers, std::vector<slim_position_t> &p_heteroduplex) const;
	
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
	
	// Memory usage tallying, for outputUsage()
	size_t MemoryUsageForMutationMaps(void);
	size_t MemoryUsageForRecombinationMaps(void);
	size_t MemoryUsageForAncestralSequence(void);
	
	
	//
	// Eidos support
	//
	virtual const EidosObjectClass *Class(void) const override;
	
	virtual EidosValue_SP GetProperty(EidosGlobalStringID p_property_id) override;
	virtual void SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value) override;
	
	virtual EidosValue_SP ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) override;
	EidosValue_SP ExecuteMethod_ancestralNucleotides(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setAncestralNucleotides(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setGeneConversion(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setHotspotMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setMutationRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_setRecombinationRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
	EidosValue_SP ExecuteMethod_drawBreakpoints(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter);
};

// draw the number of mutations that occur, based on the overall mutation rate
inline __attribute__((always_inline)) int Chromosome::DrawMutationCount(IndividualSex p_sex) const
{
#ifdef USE_GSL_POISSON
	if (single_mutation_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		return gsl_ran_poisson(EIDOS_GSL_RNG, overall_mutation_rate_H_);
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			return gsl_ran_poisson(EIDOS_GSL_RNG, overall_mutation_rate_M_);
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			return gsl_ran_poisson(EIDOS_GSL_RNG, overall_mutation_rate_F_);
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
		return gsl_ran_poisson(EIDOS_GSL_RNG, overall_recombination_rate_H_);
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			return gsl_ran_poisson(EIDOS_GSL_RNG, overall_recombination_rate_M_);
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			return gsl_ran_poisson(EIDOS_GSL_RNG, overall_recombination_rate_F_);
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
	double u = Eidos_rng_uniform(EIDOS_GSL_RNG);
	
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




































































