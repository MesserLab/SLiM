//
//  chromosome.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2017 Philipp Messer.  All rights reserved.
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


#include "chromosome.h"
#include "eidos_rng.h"
#include "slim_global.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "slim_sim.h"					// for SLIM_MUTRUN_MAXIMUM_COUNT

#include <iostream>
#include <algorithm>
#include <string>
#include <cmath>


// This struct is used to represent a constant-mutation-rate subrange of a genomic element; it is used internally by Chromosome.
// It is private to Chromosome, and is not exposed in Eidos in any way; it just assists with the drawing of new mutations.
struct GESubrange
{
public:
	GenomicElement *genomic_element_ptr_;			// pointer to the genomic element that contains this subrange
	slim_position_t start_position_;				// the start position of the subrange
	slim_position_t end_position_;					// the end position of the subrange
	
	GESubrange(GenomicElement *p_genomic_element_ptr, slim_position_t p_start_position, slim_position_t p_end_position);
};

inline __attribute__((always_inline)) GESubrange::GESubrange(GenomicElement *p_genomic_element_ptr, slim_position_t p_start_position, slim_position_t p_end_position) :
	genomic_element_ptr_(p_genomic_element_ptr), start_position_(p_start_position), end_position_(p_end_position)
{
}


#pragma mark -
#pragma mark Chromosome
#pragma mark -

Chromosome::Chromosome(void) :

	single_mutation_map_(true),
	lookup_mutation_H_(nullptr), lookup_mutation_M_(nullptr), lookup_mutation_F_(nullptr), 
	overall_mutation_rate_H_(0.0), overall_mutation_rate_M_(0.0), overall_mutation_rate_F_(0.0),
	exp_neg_overall_mutation_rate_H_(0.0), exp_neg_overall_mutation_rate_M_(0.0), exp_neg_overall_mutation_rate_F_(0.0),
	
	single_recombination_map_(true), 
	lookup_recombination_H_(nullptr), lookup_recombination_M_(nullptr), lookup_recombination_F_(nullptr),
	overall_recombination_rate_H_(0.0), overall_recombination_rate_M_(0.0), overall_recombination_rate_F_(0.0),
	exp_neg_overall_recombination_rate_H_(0.0), exp_neg_overall_recombination_rate_M_(0.0), exp_neg_overall_recombination_rate_F_(0.0), 
	
#ifndef USE_GSL_POISSON
	probability_both_0_H_(0.0), probability_both_0_OR_mut_0_break_non0_H_(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_H_(0.0),
	probability_both_0_M_(0.0), probability_both_0_OR_mut_0_break_non0_M_(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_(0.0),
	probability_both_0_F_(0.0), probability_both_0_OR_mut_0_break_non0_F_(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_(0.0), 
#endif

	last_position_(0), gene_conversion_fraction_(0.0), gene_conversion_avg_length_(0.0), last_position_mutrun_(0)
{
	// Set up the default color for fixed mutations in SLiMgui
	color_sub_ = "#3333FF";
	if (!color_sub_.empty())
		Eidos_GetColorComponents(color_sub_, &color_sub_red_, &color_sub_green_, &color_sub_blue_);
}

Chromosome::~Chromosome(void)
{
	//EIDOS_ERRSTREAM << "Chromosome::~Chromosome" << std::endl;
	
	if (lookup_mutation_H_)
		gsl_ran_discrete_free(lookup_mutation_H_);
	
	if (lookup_mutation_M_)
		gsl_ran_discrete_free(lookup_mutation_M_);
	
	if (lookup_mutation_F_)
		gsl_ran_discrete_free(lookup_mutation_F_);
	
	if (lookup_recombination_H_)
		gsl_ran_discrete_free(lookup_recombination_H_);
	
	if (lookup_recombination_M_)
		gsl_ran_discrete_free(lookup_recombination_M_);
	
	if (lookup_recombination_F_)
		gsl_ran_discrete_free(lookup_recombination_F_);
}

// initialize the random lookup tables used by Chromosome to draw mutation and recombination events
void Chromosome::InitializeDraws(void)
{
	if (size() == 0)
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): empty chromosome." << EidosTerminate();
	
	// determine which case we are working with: separate recombination maps for the sexes, or one map
	auto rec_rates_H_size = recombination_rates_H_.size();
	auto rec_rates_M_size = recombination_rates_M_.size();
	auto rec_rates_F_size = recombination_rates_F_.size();
	
	if ((rec_rates_H_size > 0) && (rec_rates_M_size == 0) && (rec_rates_F_size == 0))
		single_recombination_map_ = true;
	else if ((rec_rates_H_size == 0) && (rec_rates_M_size > 0) && (rec_rates_F_size > 0))
		single_recombination_map_ = false;
	else
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): (internal error) unclear whether we have separate recombination maps or not, or recombination rate not specified." << EidosTerminate();
	
	// determine which case we are working with: separate mutation maps for the sexes, or one map
	auto mut_rates_H_size = mutation_rates_H_.size();
	auto mut_rates_M_size = mutation_rates_M_.size();
	auto mut_rates_F_size = mutation_rates_F_.size();
	
	if ((mut_rates_H_size > 0) && (mut_rates_M_size == 0) && (mut_rates_F_size == 0))
		single_mutation_map_ = true;
	else if ((mut_rates_H_size == 0) && (mut_rates_M_size > 0) && (mut_rates_F_size > 0))
		single_mutation_map_ = false;
	else
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): (internal error) unclear whether we have separate mutation maps or not, or mutation rate not specified." << EidosTerminate();
	
	// Calculate the last chromosome base position from our genomic elements, mutation and recombination maps
	// the end of the last genomic element may be before the end of the chromosome; the end of mutation and
	// recombination maps all need to agree, though, if they have been supplied.  Checks that the maps do
	// not end before the end of the chromosome will be done in _InitializeOne...Map().
	cached_value_lastpos_.reset();
	last_position_ = 0;
	
	for (unsigned int i = 0; i < size(); i++) 
	{ 
		if ((*this)[i].end_position_ > last_position_)
			last_position_ = (*this)[i].end_position_;
	}
	
	if (single_mutation_map_)
	{
		if (mutation_end_positions_H_.size())
			last_position_ = std::max(last_position_, *(std::max_element(mutation_end_positions_H_.begin(), mutation_end_positions_H_.end())));
	}
	else
	{
		if (mutation_end_positions_M_.size())
			last_position_ = std::max(last_position_, *(std::max_element(mutation_end_positions_M_.begin(), mutation_end_positions_M_.end())));
		if (mutation_end_positions_F_.size())
			last_position_ = std::max(last_position_, *(std::max_element(mutation_end_positions_F_.begin(), mutation_end_positions_F_.end())));
	}
	
	if (single_recombination_map_)
	{
		if (recombination_end_positions_H_.size())
			last_position_ = std::max(last_position_, *(std::max_element(recombination_end_positions_H_.begin(), recombination_end_positions_H_.end())));
	}
	else
	{
		if (recombination_end_positions_M_.size())
			last_position_ = std::max(last_position_, *(std::max_element(recombination_end_positions_M_.begin(), recombination_end_positions_M_.end())));
		if (recombination_end_positions_F_.size())
			last_position_ = std::max(last_position_, *(std::max_element(recombination_end_positions_F_.begin(), recombination_end_positions_F_.end())));
	}
	
	// Now remake our mutation map info, which we delegate to _InitializeOneMutationMap()
	if (single_mutation_map_)
	{
		_InitializeOneMutationMap(lookup_mutation_H_, mutation_end_positions_H_, mutation_rates_H_, overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_, mutation_subranges_H_);
		
		// Copy the H rates into the M and F ivars, so that they can be used by DrawMutationAndBreakpointCounts() if needed
		overall_mutation_rate_M_ = overall_mutation_rate_F_ = overall_mutation_rate_H_;
		exp_neg_overall_mutation_rate_M_ = exp_neg_overall_mutation_rate_F_ = exp_neg_overall_mutation_rate_H_;
	}
	else
	{
		_InitializeOneMutationMap(lookup_mutation_M_, mutation_end_positions_M_, mutation_rates_M_, overall_mutation_rate_M_, exp_neg_overall_mutation_rate_M_, mutation_subranges_M_);
		_InitializeOneMutationMap(lookup_mutation_F_, mutation_end_positions_F_, mutation_rates_F_, overall_mutation_rate_F_, exp_neg_overall_mutation_rate_F_, mutation_subranges_F_);
	}
	
	// Now remake our recombination map info, which we delegate to _InitializeOneRecombinationMap()
	if (single_recombination_map_)
	{
		_InitializeOneRecombinationMap(lookup_recombination_H_, recombination_end_positions_H_, recombination_rates_H_, overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_);
		
		// Copy the H rates into the M and F ivars, so that they can be used by DrawMutationAndBreakpointCounts() if needed
		overall_recombination_rate_M_ = overall_recombination_rate_F_ = overall_recombination_rate_H_;
		exp_neg_overall_recombination_rate_M_ = exp_neg_overall_recombination_rate_F_ = exp_neg_overall_recombination_rate_H_;
	}
	else
	{
		_InitializeOneRecombinationMap(lookup_recombination_M_, recombination_end_positions_M_, recombination_rates_M_, overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_);
		_InitializeOneRecombinationMap(lookup_recombination_F_, recombination_end_positions_F_, recombination_rates_F_, overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_);
	}
	
#ifndef USE_GSL_POISSON
	// Calculate joint mutation/recombination probabilities for the H/M/F cases
	if (single_mutation_map_ && single_recombination_map_)
	{
		_InitializeJointProbabilities(overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_,
									  overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_,
									  probability_both_0_H_, probability_both_0_OR_mut_0_break_non0_H_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_H_);
	}
	else if (single_mutation_map_ && !single_recombination_map_)
	{
		_InitializeJointProbabilities(overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_,
									  overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_,
									  probability_both_0_M_, probability_both_0_OR_mut_0_break_non0_M_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_);
		
		_InitializeJointProbabilities(overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_,
									  overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_,
									  probability_both_0_F_, probability_both_0_OR_mut_0_break_non0_F_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_);
	}
	else if (!single_mutation_map_ && single_recombination_map_)
	{
		_InitializeJointProbabilities(overall_mutation_rate_M_, exp_neg_overall_mutation_rate_M_,
									  overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_,
									  probability_both_0_M_, probability_both_0_OR_mut_0_break_non0_M_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_);
		
		_InitializeJointProbabilities(overall_mutation_rate_F_, exp_neg_overall_mutation_rate_F_,
									  overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_,
									  probability_both_0_F_, probability_both_0_OR_mut_0_break_non0_F_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_);
	}
	else if (!single_mutation_map_ && !single_recombination_map_)
	{
		_InitializeJointProbabilities(overall_mutation_rate_M_, exp_neg_overall_mutation_rate_M_,
									  overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_,
									  probability_both_0_M_, probability_both_0_OR_mut_0_break_non0_M_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_);
		
		_InitializeJointProbabilities(overall_mutation_rate_F_, exp_neg_overall_mutation_rate_F_,
									  overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_,
									  probability_both_0_F_, probability_both_0_OR_mut_0_break_non0_F_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_);
	}
#endif
}

#ifndef USE_GSL_POISSON
void Chromosome::_InitializeJointProbabilities(double p_overall_mutation_rate, double p_exp_neg_overall_mutation_rate,
											   double p_overall_recombination_rate, double p_exp_neg_overall_recombination_rate,
											   double &p_both_0, double &p_both_0_OR_mut_0_break_non0, double &p_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0)
{
	// precalculate probabilities for Poisson draws of mutation count and breakpoint count
	double prob_mutation_0 = Eidos_FastRandomPoisson_PRECALCULATE(p_overall_mutation_rate);			// exp(-mu); can be 0 due to underflow
	double prob_breakpoint_0 = Eidos_FastRandomPoisson_PRECALCULATE(p_overall_recombination_rate);	// exp(-mu); can be 0 due to underflow
	
	// this is a validity check on the previous calculation of the exp_neg rates
	if ((p_exp_neg_overall_mutation_rate != prob_mutation_0) || (p_exp_neg_overall_recombination_rate != prob_breakpoint_0))
		EIDOS_TERMINATION << "ERROR (Chromosome::_InitializeJointProbabilities): zero-probability does not match previous calculation." << EidosTerminate();
	
	double prob_mutation_not_0 = 1.0 - prob_mutation_0;
	double prob_breakpoint_not_0 = 1.0 - prob_breakpoint_0;
	double prob_both_0 = prob_mutation_0 * prob_breakpoint_0;
	double prob_mutation_0_breakpoint_not_0 = prob_mutation_0 * prob_breakpoint_not_0;
	double prob_mutation_not_0_breakpoint_0 = prob_mutation_not_0 * prob_breakpoint_0;
	
	//	std::cout << "element_mutation_rate_ == " << element_mutation_rate_ << std::endl;
	//	std::cout << "prob_mutation_0 == " << prob_mutation_0 << std::endl;
	//	std::cout << "prob_breakpoint_0 == " << prob_breakpoint_0 << std::endl;
	//	std::cout << "prob_mutation_not_0 == " << prob_mutation_not_0 << std::endl;
	//	std::cout << "prob_breakpoint_not_0 == " << prob_breakpoint_not_0 << std::endl;
	//	std::cout << "prob_both_0 == " << prob_both_0 << std::endl;
	//	std::cout << "prob_mutation_0_breakpoint_not_0 == " << prob_mutation_0_breakpoint_not_0 << std::endl;
	//	std::cout << "prob_mutation_not_0_breakpoint_0 == " << prob_mutation_not_0_breakpoint_0 << std::endl;
	
	p_both_0 = prob_both_0;
	p_both_0_OR_mut_0_break_non0 = prob_both_0 + prob_mutation_0_breakpoint_not_0;
	p_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0 = prob_both_0 + (prob_mutation_0_breakpoint_not_0 + prob_mutation_not_0_breakpoint_0);
}
#endif

void Chromosome::ChooseMutationRunLayout(int p_preferred_count)
{
	// We now have a final last position, so we can calculate our mutation run layout
	
	if (p_preferred_count != 0)
	{
		// The user has given us a mutation run count, so use that count and divide the chromosome evenly
		if (p_preferred_count < 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::ChooseMutationRunLayout): there must be at least one mutation run per genome." << EidosTerminate();
		
		mutrun_count_ = p_preferred_count;
		mutrun_length_ = (int)ceil((last_position_ + 1) / (double)mutrun_count_);
		
		if (SLiM_verbose_output)
			SLIM_OUTSTREAM << std::endl << "// Override mutation run count = " << mutrun_count_ << ", run length = " << mutrun_length_ << std::endl;
	}
	else
	{
		// The user has not supplied a count, so we will conduct experiments to find the best count;
		// for simplicity we will just always start with a single run, since that is often best anyway
		mutrun_count_ = 1;
		mutrun_length_ = (int)ceil((last_position_ + 1) / (double)mutrun_count_);
		
		// When we are running experiments, the mutation run length needs to be a power of two so that it can be divided evenly,
		// potentially a fairly large number of times.  We impose a maximum mutrun count of SLIM_MUTRUN_MAXIMUM_COUNT, so
		// actually it needs to just be an even multiple of SLIM_MUTRUN_MAXIMUM_COUNT, not an exact power of two.
		mutrun_length_ = (int)round(ceil(mutrun_length_ / (double)SLIM_MUTRUN_MAXIMUM_COUNT) * SLIM_MUTRUN_MAXIMUM_COUNT);
		
		if (SLiM_verbose_output)
			SLIM_OUTSTREAM << std::endl << "// Initial mutation run count = " << mutrun_count_ << ", run length = " << mutrun_length_ << std::endl;
	}
	
	last_position_mutrun_ = mutrun_count_ * mutrun_length_ - 1;
	
	// Consistency check
	if ((mutrun_length_ < 1) || (mutrun_count_ * mutrun_length_ <= last_position_))
		EIDOS_TERMINATION << "ERROR (Chromosome::ChooseMutationRunLayout): (internal error) math error in mutation run calculations." << EidosTerminate();
	if (last_position_mutrun_ < last_position_)
		EIDOS_TERMINATION << "ERROR (Chromosome::ChooseMutationRunLayout): (internal error) math error in mutation run calculations." << EidosTerminate();
}

// initialize one recombination map, used internally by InitializeDraws() to avoid code duplication
void Chromosome::_InitializeOneRecombinationMap(gsl_ran_discrete_t *&p_lookup, vector<slim_position_t> &p_end_positions, vector<double> &p_rates, double &p_overall_rate, double &p_exp_neg_overall_rate)
{
	// Patch the recombination interval end vector if it is empty; see setRecombinationRate() and initializeRecombinationRate().
	// Basically, the length of the chromosome might not have been known yet when the user set the rate.
	if (p_end_positions.size() == 0)
	{
		// patching can only be done when a single uniform rate is specified
		if (p_rates.size() != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): recombination endpoints not specified." << EidosTerminate();
		
		p_end_positions.emplace_back(last_position_);
	}
	
	if (p_end_positions[p_rates.size() - 1] < last_position_)
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): recombination endpoints do not cover the full chromosome." << EidosTerminate();
	
	// Calculate the overall recombination rate and the lookup table for breakpoints
	std::vector<double> B(p_rates.size());
	
	B[0] = p_rates[0] * static_cast<double>(p_end_positions[0]);	// No +1 here, because the position to the left of the first base is not a valid recombination position.
																	// So a 1-base model (position 0 to 0) has an end of 0, and thus an overall rate of 0.  This means that
																	// gsl_ran_discrete_preproc() is given an interval with rate 0, but that seems OK.  BCH 4 April 2016
	for (unsigned int i = 1; i < p_rates.size(); i++)
		B[i] = p_rates[i] * static_cast<double>(p_end_positions[i] - p_end_positions[i - 1]);
	
	p_overall_rate = Eidos_ExactSum(B.data(), p_rates.size());
	
#ifndef USE_GSL_POISSON
	p_exp_neg_overall_rate = Eidos_FastRandomPoisson_PRECALCULATE(p_overall_rate);				// exp(-mu); can be 0 due to underflow
#endif
	
	if (p_lookup)
		gsl_ran_discrete_free(p_lookup);
	
	p_lookup = gsl_ran_discrete_preproc(p_rates.size(), B.data());
}

// initialize one mutation map, used internally by InitializeDraws() to avoid code duplication
void Chromosome::_InitializeOneMutationMap(gsl_ran_discrete_t *&p_lookup, vector<slim_position_t> &p_end_positions, vector<double> &p_rates, double &p_overall_rate, double &p_exp_neg_overall_rate, vector<GESubrange> &p_subranges)
{
	// Patch the mutation interval end vector if it is empty; see setMutationRate() and initializeMutationRate().
	// Basically, the length of the chromosome might not have been known yet when the user set the rate.
	if (p_end_positions.size() == 0)
	{
		// patching can only be done when a single uniform rate is specified
		if (p_rates.size() != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): mutation rate endpoints not specified." << EidosTerminate();
		
		p_end_positions.emplace_back(last_position_);
	}
	
	if (p_end_positions[p_rates.size() - 1] < last_position_)
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): recombination endpoints do not cover the full chromosome." << EidosTerminate();
	
	// Calculate the overall mutation rate and the lookup table for mutation events.  This is more complicated than
	// for recombination maps, because the mutation rate map needs to be intersected with the genomic element map
	// such that all areas outside of any genomic element have a rate of 0.  In the end we need (i) a vector of
	// constant-rate subregions that do not span genomic element boundaries, (ii) a lookup table to go from the
	// index of a constant-rate subregion to the index of the containing genomic element, and (iii) a lookup table
	// to go from the index of a constant-rate subregion to the first base position and length of the subregion.
	// In effect, each of these subregions is sort of the new equivalent of a genomic element; we intersect the
	// mutation rate map with the genomic element map to create a list of new genomic elements of constant mut rate.
	// The class we use to represent these constant-rate subregions is GESubrange, declared in chromosome.h.
	p_subranges.clear();
	
	std::vector<double> B;
	unsigned int mutrange_index = 0;
	slim_position_t end_of_previous_mutrange = -1;
	
	for (unsigned int ge_index = 0; ge_index < size(); ge_index++) 
	{
		GenomicElement &ge = (*this)[ge_index];
		
		for ( ; mutrange_index < p_rates.size(); mutrange_index++)
		{
			slim_position_t end_of_mutrange = p_end_positions[mutrange_index];
			
			if (end_of_mutrange < ge.start_position_)
			{
				// This mutrange falls entirely before the current genomic element, so it is discarded by falling through
			}
			else if (end_of_previous_mutrange + 1 > ge.end_position_)
			{
				// This mutrange falls entirely after the current genomic element, so we need to move to the next genomic element
				break;
			}
			else
			{
				// This mutrange at least partially intersects the current genomic element, so we need to make a new GESubrange
				slim_position_t subrange_start = std::max(end_of_previous_mutrange + 1, ge.start_position_);
				slim_position_t subrange_end = std::min(end_of_mutrange, ge.end_position_);
				slim_position_t subrange_length = subrange_end - subrange_start + 1;
				double subrange_weight = p_rates[mutrange_index] * subrange_length;
				
				B.emplace_back(subrange_weight);
				p_subranges.emplace_back(&ge, subrange_start, subrange_end);
				
				// Now we need to decide whether to advance ge_index or not; advancing mutrange_index here is not needed
				if (end_of_mutrange >= ge.end_position_)
					break;
			}
			
			end_of_previous_mutrange = end_of_mutrange;
		}
	}
	
	p_overall_rate = Eidos_ExactSum(B.data(), B.size());
	
#ifndef USE_GSL_POISSON
	p_exp_neg_overall_rate = Eidos_FastRandomPoisson_PRECALCULATE(p_overall_rate);				// exp(-mu); can be 0 due to underflow
#endif
	
	if (p_lookup)
		gsl_ran_discrete_free(p_lookup);
	
	p_lookup = gsl_ran_discrete_preproc(B.size(), B.data());
}

// prints an error message and exits
void Chromosome::MutationMapConfigError(void) const
{
	EIDOS_TERMINATION << "ERROR (Chromosome::MutationMapConfigError): (internal error) an error occurred in the configuration of mutation maps." << EidosTerminate();
}
void Chromosome::RecombinationMapConfigError(void) const
{
	EIDOS_TERMINATION << "ERROR (Chromosome::RecombinationMapConfigError): (internal error) an error occurred in the configuration of recombination maps." << EidosTerminate();
}


// draw a new mutation, based on the genomic element types present and their mutational proclivities
MutationIndex Chromosome::DrawNewMutation(IndividualSex p_sex, slim_objectid_t p_subpop_index, slim_generation_t p_generation) const
{
	gsl_ran_discrete_t *lookup;
	const vector<GESubrange> *subranges;
	
	if (single_mutation_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		lookup = lookup_mutation_H_;
		subranges = &mutation_subranges_H_;
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			lookup = lookup_mutation_M_;
			subranges = &mutation_subranges_M_;
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			lookup = lookup_mutation_F_;
			subranges = &mutation_subranges_F_;
		}
		else
		{
			MutationMapConfigError();
		}
	}
	
	int mut_subrange_index = static_cast<int>(gsl_ran_discrete(gEidos_rng, lookup));
	const GESubrange &subrange = (*subranges)[mut_subrange_index];
	const GenomicElement &source_element = *(subrange.genomic_element_ptr_);
	const GenomicElementType &genomic_element_type = *source_element.genomic_element_type_ptr_;
	MutationType *mutation_type_ptr = genomic_element_type.DrawMutationType();
	
	slim_position_t position = subrange.start_position_ + static_cast<slim_position_t>(Eidos_rng_uniform_int(gEidos_rng, subrange.end_position_ - subrange.start_position_ + 1));  
	
	double selection_coeff = mutation_type_ptr->DrawSelectionCoefficient();
	
	// NOTE THAT THE STACKING POLICY IS NOT ENFORCED HERE, SINCE WE DO NOT KNOW WHAT GENOME WE WILL BE INSERTED INTO!  THIS IS THE CALLER'S RESPONSIBILITY!
	MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
	
	new (gSLiM_Mutation_Block + new_mut_index) Mutation(mutation_type_ptr, position, selection_coeff, p_subpop_index, p_generation);
	
	return new_mut_index;
}

// choose a set of recombination breakpoints, based on recombination intervals, overall recombination rate, and gene conversion probability
// BEWARE!  Chromosome::DrawBreakpoints_Detailed() below must be altered in parallel with this method!
void Chromosome::DrawBreakpoints(IndividualSex p_sex, const int p_num_breakpoints, std::vector<slim_position_t> &p_crossovers) const
{
	gsl_ran_discrete_t *lookup;
	const vector<slim_position_t> *end_positions;
	
	if (single_recombination_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		lookup = lookup_recombination_H_;
		end_positions = &recombination_end_positions_H_;
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			lookup = lookup_recombination_M_;
			end_positions = &recombination_end_positions_M_;
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			lookup = lookup_recombination_F_;
			end_positions = &recombination_end_positions_F_;
		}
		else
		{
			RecombinationMapConfigError();
		}
	}
	
	// draw recombination breakpoints
	for (int i = 0; i < p_num_breakpoints; i++)
	{
		slim_position_t breakpoint = 0;
		int recombination_interval = static_cast<int>(gsl_ran_discrete(gEidos_rng, lookup));
		
		// choose a breakpoint anywhere in the chosen recombination interval with equal probability
		
		// BCH 4 April 2016: Added +1 to positions in the first interval.  We do not want to generate a recombination breakpoint
		// to the left of the 0th base, and the code in InitializeDraws() above explicitly omits that position from its calculation
		// of the overall recombination rate.  Using recombination_end_positions_[recombination_interval] here for the first
		// interval means that we use one less breakpoint position than usual; conceptually, the previous breakpoint ended at -1,
		// so it ought to be recombination_end_positions_[recombination_interval]+1, but we do not add one there, in order to
		// use one fewer positions.  We then shift all the positions to the right one, with the +1 that is added here, thereby
		// making the position that was omitted be the position to the left of the 0th base.
		//
		// I also added +1 in the formula for regions after the 0th.  In general, we want a recombination interval to own all the
		// positions to the left of its enclosed bases, up to and including the position to the left of the final base given as the
		// end position of the interval.  The next interval's first owned recombination position is therefore to the left of the
		// base that is one position to the right of the end of the preceding interval.  So we have to add one to the position
		// given by recombination_end_positions_[recombination_interval - 1], at minimum.  Since Eidos_rng_uniform_int() returns
		// a zero-based random number, that means we need a +1 here as well.
		//
		// The key fact here is that a recombination breakpoint position of 1 means "break to the left of the base at position 1" –
		// the breakpoint falls between bases, to the left of the bse at the specified number.  This is a consequence of the logic
		// is the crossover-mutation code, which copies mutations as long as their position is *less than* the position of the next
		// breakpoint.  When their position is *equal*, the breakpoint gets serviced by switching strands.  That logic causes the
		// breakpoints to fall to the left of their designated base.
		//
		// Note that Eidos_rng_uniform_int() crashes (well, aborts fatally) if passed 0 for n.  We need to guarantee that that doesn't
		// happen, and we don't want to waste time checking for that condition here.  For a 1-base model, we are guaranteed that
		// the overall recombination rate will be zero, by the logic in InitializeDraws(), and so we should not be called in the
		// first place.  For longer chromosomes that start with a 1-base recombination interval, the rate calculated by
		// InitializeDraws() for the first interval should be 0, so gsl_ran_discrete() should never return the first interval to
		// us here.  For all other recombination intervals, the math of pos[x]-pos[x-1] should always result in a value >0,
		// since we guarantee that recombination end positions are in strictly ascending order.  So we should never crash.  :->
		
		if (recombination_interval == 0)
			breakpoint = static_cast<slim_position_t>(Eidos_rng_uniform_int(gEidos_rng, (*end_positions)[recombination_interval]) + 1);
		else
			breakpoint = (*end_positions)[recombination_interval - 1] + 1 + static_cast<slim_position_t>(Eidos_rng_uniform_int(gEidos_rng, (*end_positions)[recombination_interval] - (*end_positions)[recombination_interval - 1]));
		
		p_crossovers.emplace_back(breakpoint);
		
		// recombination can result in gene conversion, with probability gene_conversion_fraction_
		if (gene_conversion_fraction_ > 0.0)
		{
			if ((gene_conversion_fraction_ < 1.0) && (Eidos_rng_uniform(gEidos_rng) < gene_conversion_fraction_))
			{
				// for gene conversion, choose a second breakpoint that is relatively likely to be near to the first
				// note that this second breakpoint does not count toward the total number of breakpoints we need to
				// generate; this means that when gene conversion occurs, we return more breakpoints than requested!
				slim_position_t breakpoint2 = SLiMClampToPositionType(breakpoint + gsl_ran_geometric(gEidos_rng, 1.0 / gene_conversion_avg_length_));
				
				if (breakpoint2 <= last_position_)	// used to always add; added this 17 August 2015 BCH, but shouldn't really matter
					p_crossovers.emplace_back(breakpoint2);
			}
		}
	}
}

// The same logic as Chromosome::DrawBreakpoints() above, but breaks results down into crossovers versus
// gene conversion stand/end points.  See Chromosome::DrawBreakpoints for comments on the logic.
void Chromosome::DrawBreakpoints_Detailed(IndividualSex p_sex, const int p_num_breakpoints, vector<slim_position_t> &p_crossovers, vector<slim_position_t> &p_gcstarts, vector<slim_position_t> &p_gcends) const
{
	gsl_ran_discrete_t *lookup;
	const vector<slim_position_t> *end_positions;
	
	if (single_recombination_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		lookup = lookup_recombination_H_;
		end_positions = &recombination_end_positions_H_;
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_sex == IndividualSex::kMale)
		{
			lookup = lookup_recombination_M_;
			end_positions = &recombination_end_positions_M_;
		}
		else if (p_sex == IndividualSex::kFemale)
		{
			lookup = lookup_recombination_F_;
			end_positions = &recombination_end_positions_F_;
		}
		else
		{
			RecombinationMapConfigError();
		}
	}
	
	// draw recombination breakpoints
	for (int i = 0; i < p_num_breakpoints; i++)
	{
		slim_position_t breakpoint = 0;
		int recombination_interval = static_cast<int>(gsl_ran_discrete(gEidos_rng, lookup));
		
		// choose a breakpoint anywhere in the chosen recombination interval with equal probability
		if (recombination_interval == 0)
			breakpoint = static_cast<slim_position_t>(Eidos_rng_uniform_int(gEidos_rng, (*end_positions)[recombination_interval]) + 1);
		else
			breakpoint = (*end_positions)[recombination_interval - 1] + 1 + static_cast<slim_position_t>(Eidos_rng_uniform_int(gEidos_rng, (*end_positions)[recombination_interval] - (*end_positions)[recombination_interval - 1]));
		
		// recombination can result in gene conversion, with probability gene_conversion_fraction_
		if (gene_conversion_fraction_ > 0.0)
		{
			if ((gene_conversion_fraction_ < 1.0) && (Eidos_rng_uniform(gEidos_rng) < gene_conversion_fraction_))
			{
				p_gcstarts.emplace_back(breakpoint);
				
				// for gene conversion, choose a second breakpoint that is relatively likely to be near to the first
				// note that this second breakpoint does not count toward the total number of breakpoints we need to
				// generate; this means that when gene conversion occurs, we return more breakpoints than requested!
				slim_position_t breakpoint2 = SLiMClampToPositionType(breakpoint + gsl_ran_geometric(gEidos_rng, 1.0 / gene_conversion_avg_length_));
				
				if (breakpoint2 <= last_position_)	// used to always add; added this 17 August 2015 BCH, but shouldn't really matter
					p_gcends.emplace_back(breakpoint2);
				else
					p_gcends.emplace_back(last_position_ + 1);	// so every start has an end; harmless and will be removed by unique()
				
				continue;
			}
		}
		
		p_crossovers.emplace_back(breakpoint);
	}
}


//
// Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosObjectClass *Chromosome::Class(void) const
{
	return gSLiM_Chromosome_Class;
}

EidosValue_SP Chromosome::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_genomicElements:
		{
			EidosValue_Object_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_GenomicElement_Class);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto genomic_element_iter = this->begin(); genomic_element_iter != this->end(); genomic_element_iter++)
				vec->push_object_element(&(*genomic_element_iter));		// operator * can be overloaded by the iterator
			
			return result_SP;
		}
		case gID_lastPosition:
		{
			if (!cached_value_lastpos_)
				cached_value_lastpos_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(last_position_));
			return cached_value_lastpos_;
		}
			
		case gID_mutationEndPositions:
		{
			if (!single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositions is not defined since sex-specific mutation rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(mutation_end_positions_H_));
		}
		case gID_mutationEndPositionsM:
		{
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositionsM is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(mutation_end_positions_M_));
		}
		case gID_mutationEndPositionsF:
		{
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositionsF is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(mutation_end_positions_F_));
		}
			
		case gID_mutationRates:
		{
			if (!single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRates is not defined since sex-specific mutation rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(mutation_rates_H_));
		}
		case gID_mutationRatesM:
		{
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRatesM is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(mutation_rates_M_));
		}
		case gID_mutationRatesF:
		{
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRatesF is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(mutation_rates_F_));
		}
			
		case gID_overallMutationRate:
		{
			if (!single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRate is not defined since sex-specific mutation rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_mutation_rate_H_));
		}
		case gID_overallMutationRateM:
		{
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRateM is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_mutation_rate_M_));
		}
		case gID_overallMutationRateF:
		{
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRateF is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_mutation_rate_F_));
		}
			
		case gID_overallRecombinationRate:
		{
			if (!single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallRecombinationRate is not defined since sex-specific recombination rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_recombination_rate_H_));
		}
		case gID_overallRecombinationRateM:
		{
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallRecombinationRateM is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_recombination_rate_M_));
		}
		case gID_overallRecombinationRateF:
		{
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallRecombinationRateF is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_recombination_rate_F_));
		}
			
		case gID_recombinationEndPositions:
		{
			if (!single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property recombinationEndPositions is not defined since sex-specific recombination rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(recombination_end_positions_H_));
		}
		case gID_recombinationEndPositionsM:
		{
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property recombinationEndPositionsM is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(recombination_end_positions_M_));
		}
		case gID_recombinationEndPositionsF:
		{
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property recombinationEndPositionsF is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(recombination_end_positions_F_));
		}
			
		case gID_recombinationRates:
		{
			if (!single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property recombinationRates is not defined since sex-specific recombination rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(recombination_rates_H_));
		}
		case gID_recombinationRatesM:
		{
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property recombinationRatesM is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(recombination_rates_M_));
		}
		case gID_recombinationRatesF:
		{
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property recombinationRatesF is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(recombination_rates_F_));
		}
			
			// variables
		case gID_colorSubstitution:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(color_sub_));
		case gID_geneConversionFraction:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gene_conversion_fraction_));
		case gID_geneConversionMeanLength:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gene_conversion_avg_length_));
		case gID_tag:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value_));
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void Chromosome::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_colorSubstitution:
		{
			color_sub_ = p_value.StringAtIndex(0, nullptr);
			if (!color_sub_.empty())
				Eidos_GetColorComponents(color_sub_, &color_sub_red_, &color_sub_green_, &color_sub_blue_);
			return;
		}
		case gID_geneConversionFraction:
		{
			double value = p_value.FloatAtIndex(0, nullptr);
			
			if ((value < 0.0) || (value > 1.0))
				EIDOS_TERMINATION << "ERROR (Chromosome::SetProperty): new value " << value << " for property " << Eidos_StringForGlobalStringID(p_property_id) << " is out of range." << EidosTerminate();
			
			gene_conversion_fraction_ = value;
			return;
		}
		case gID_geneConversionMeanLength:
		{
			double value = p_value.FloatAtIndex(0, nullptr);
			
			if (value <= 0.0)		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (Chromosome::SetProperty): new value " << value << " for property " << Eidos_StringForGlobalStringID(p_property_id) << " is out of range." << EidosTerminate();
			
			gene_conversion_avg_length_ = value;
			return;
		}
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SetProperty(p_property_id, p_value);
	}
}

EidosValue_SP Chromosome::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_setMutationRate:		return ExecuteMethod_setMutationRate(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_setRecombinationRate:	return ExecuteMethod_setRecombinationRate(p_method_id, p_arguments, p_argument_count, p_interpreter);
		default:						return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}

//	*********************	– (void)setMutationRate(numeric rates, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP Chromosome::ExecuteMethod_setMutationRate(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *rates_value = p_arguments[0].get();
	EidosValue *ends_value = p_arguments[1].get();
	EidosValue *sex_value = p_arguments[2].get();
	
	int rate_count = rates_value->Count();
	
	// Figure out what sex we are being given a map for
	IndividualSex requested_sex = IndividualSex::kUnspecified;
	
	std::string sex_string = sex_value->StringAtIndex(0, nullptr);
	
	if (sex_string.compare("M") == 0)
		requested_sex = IndividualSex::kMale;
	else if (sex_string.compare("F") == 0)
		requested_sex = IndividualSex::kFemale;
	else if (sex_string.compare("*") == 0)
		requested_sex = IndividualSex::kUnspecified;
	else
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() requested sex \"" << sex_string << "\" unsupported." << EidosTerminate();
	
	// Make sure specifying a map for that sex is legal, given our current state
	if (((requested_sex == IndividualSex::kUnspecified) && !single_mutation_map_) ||
		((requested_sex != IndividualSex::kUnspecified) && single_mutation_map_))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() cannot change the chromosome between using a single map versus separate maps for the sexes; the original configuration must be preserved." << EidosTerminate();
	
	// Set up to replace the requested map
	vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? mutation_end_positions_H_ : 
										  ((requested_sex == IndividualSex::kMale) ? mutation_end_positions_M_ : mutation_end_positions_F_));
	vector<double> &rates = ((requested_sex == IndividualSex::kUnspecified) ? mutation_rates_H_ : 
							 ((requested_sex == IndividualSex::kMale) ? mutation_rates_M_ : mutation_rates_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		// ends is missing/NULL
		if (rate_count != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() requires rates to be a singleton if ends is not supplied." << EidosTerminate();
		
		double mutation_rate = rates_value->FloatAtIndex(0, nullptr);
		
		// check values
		if (mutation_rate < 0.0)		// intentionally no upper bound
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() rate " << mutation_rate << " out of range; rates must be >= 0." << EidosTerminate();
		
		// then adopt them
		rates.clear();
		positions.clear();
		
		rates.emplace_back(mutation_rate);
		//positions.emplace_back(?);	// deferred; patched in Chromosome::InitializeDraws().
	}
	else
	{
		// ends is supplied
		int end_count = ends_value->Count();
		
		if ((end_count != rate_count) || (end_count == 0))
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() requires ends and rates to be of equal and nonzero size." << EidosTerminate();
		
		// check values
		for (int value_index = 0; value_index < end_count; ++value_index)
		{
			double mutation_rate = rates_value->FloatAtIndex(value_index, nullptr);
			slim_position_t mutation_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex(value_index, nullptr));
			
			if (value_index > 0)
				if (mutation_end_position <= ends_value->IntAtIndex(value_index - 1, nullptr))
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() requires ends to be in strictly ascending order." << EidosTerminate();
			
			if (mutation_rate < 0.0)		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() rate " << mutation_rate << " out of range; rates must be >= 0." << EidosTerminate();
		}
		
		// The stake here is that the last position in the chromosome is not allowed to change after the chromosome is
		// constructed.  When we call InitializeDraws() below, we recalculate the last position – and we must come up
		// with the same answer that we got before, otherwise our last_position_ cache is invalid.
		int64_t new_last_position = ends_value->IntAtIndex(end_count - 1, nullptr);
		
		if (new_last_position != last_position_)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() rate " << new_last_position << " noncompliant; the last interval must end at the last position of the chromosome (" << last_position_ << ")." << EidosTerminate();
		
		// then adopt them
		rates.clear();
		positions.clear();
		
		for (int interval_index = 0; interval_index < end_count; ++interval_index)
		{
			double mutation_rate = rates_value->FloatAtIndex(interval_index, nullptr);
			slim_position_t mutation_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex(interval_index, nullptr));
			
			rates.emplace_back(mutation_rate);
			positions.emplace_back(mutation_end_position);
		}
	}
	
	InitializeDraws();
	
	return gStaticEidosValueNULLInvisible;
}

//	*********************	– (void)setRecombinationRate(numeric rates, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP Chromosome::ExecuteMethod_setRecombinationRate(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *rates_value = p_arguments[0].get();
	EidosValue *ends_value = p_arguments[1].get();
	EidosValue *sex_value = p_arguments[2].get();
	
	int rate_count = rates_value->Count();
	
	// Figure out what sex we are being given a map for
	IndividualSex requested_sex = IndividualSex::kUnspecified;
	
	std::string sex_string = sex_value->StringAtIndex(0, nullptr);
	
	if (sex_string.compare("M") == 0)
		requested_sex = IndividualSex::kMale;
	else if (sex_string.compare("F") == 0)
		requested_sex = IndividualSex::kFemale;
	else if (sex_string.compare("*") == 0)
		requested_sex = IndividualSex::kUnspecified;
	else
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() requested sex \"" << sex_string << "\" unsupported." << EidosTerminate();
	
	// Make sure specifying a map for that sex is legal, given our current state
	if (((requested_sex == IndividualSex::kUnspecified) && !single_recombination_map_) ||
		((requested_sex != IndividualSex::kUnspecified) && single_recombination_map_))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() cannot change the chromosome between using a single map versus separate maps for the sexes; the original configuration must be preserved." << EidosTerminate();
	
	// Set up to replace the requested map
	vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? recombination_end_positions_H_ : 
										  ((requested_sex == IndividualSex::kMale) ? recombination_end_positions_M_ : recombination_end_positions_F_));
	vector<double> &rates = ((requested_sex == IndividualSex::kUnspecified) ? recombination_rates_H_ : 
							 ((requested_sex == IndividualSex::kMale) ? recombination_rates_M_ : recombination_rates_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		// ends is missing/NULL
		if (rate_count != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() requires rates to be a singleton if ends is not supplied." << EidosTerminate();
		
		double recombination_rate = rates_value->FloatAtIndex(0, nullptr);
		
		// check values
		if (recombination_rate < 0.0)		// intentionally no upper bound
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() rate " << recombination_rate << " out of range; rates must be >= 0." << EidosTerminate();
		
		// then adopt them
		rates.clear();
		positions.clear();
		
		rates.emplace_back(recombination_rate);
		//positions.emplace_back(?);	// deferred; patched in Chromosome::InitializeDraws().
	}
	else
	{
		// ends is supplied
		int end_count = ends_value->Count();
		
		if ((end_count != rate_count) || (end_count == 0))
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() requires ends and rates to be of equal and nonzero size." << EidosTerminate();
		
		// check values
		for (int value_index = 0; value_index < end_count; ++value_index)
		{
			double recombination_rate = rates_value->FloatAtIndex(value_index, nullptr);
			slim_position_t recombination_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex(value_index, nullptr));
			
			if (value_index > 0)
				if (recombination_end_position <= ends_value->IntAtIndex(value_index - 1, nullptr))
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() requires ends to be in strictly ascending order." << EidosTerminate();
			
			if (recombination_rate < 0.0)		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() rate " << recombination_rate << " out of range; rates must be >= 0." << EidosTerminate();
		}
		
		// The stake here is that the last position in the chromosome is not allowed to change after the chromosome is
		// constructed.  When we call InitializeDraws() below, we recalculate the last position – and we must come up
		// with the same answer that we got before, otherwise our last_position_ cache is invalid.
		int64_t new_last_position = ends_value->IntAtIndex(end_count - 1, nullptr);
		
		if (new_last_position != last_position_)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() rate " << new_last_position << " noncompliant; the last interval must end at the last position of the chromosome (" << last_position_ << ")." << EidosTerminate();
		
		// then adopt them
		rates.clear();
		positions.clear();
		
		for (int interval_index = 0; interval_index < end_count; ++interval_index)
		{
			double recombination_rate = rates_value->FloatAtIndex(interval_index, nullptr);
			slim_position_t recombination_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex(interval_index, nullptr));
			
			rates.emplace_back(recombination_rate);
			positions.emplace_back(recombination_end_position);
		}
	}
	
	InitializeDraws();
	
	return gStaticEidosValueNULLInvisible;
}


//
//	Chromosome_Class
//
#pragma mark -
#pragma mark Chromosome_Class
#pragma mark -

class Chromosome_Class : public EidosObjectClass
{
public:
	Chromosome_Class(const Chromosome_Class &p_original) = delete;	// no copy-construct
	Chromosome_Class& operator=(const Chromosome_Class&) = delete;	// no copying
	inline Chromosome_Class(void) { }
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
};

EidosObjectClass *gSLiM_Chromosome_Class = new Chromosome_Class();


const std::string &Chromosome_Class::ElementType(void) const
{
	return gStr_Chromosome;
}

const std::vector<const EidosPropertySignature *> *Chromosome_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomicElements,			true,	kEidosValueMaskObject, gSLiM_GenomicElement_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_lastPosition,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationEndPositions,		true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationEndPositionsM,		true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationEndPositionsF,		true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationRates,				true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationRatesM,				true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationRatesF,				true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallMutationRate,		true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallMutationRateM,		true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallMutationRateF,		true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallRecombinationRate,	true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallRecombinationRateM,	true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallRecombinationRateF,	true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationEndPositions,	true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationEndPositionsM,	true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationEndPositionsF,	true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationRates,			true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationRatesM,		true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationRatesF,		true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionFraction,		false,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionMeanLength,	false,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,						false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_colorSubstitution,			false,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<const EidosMethodSignature *> *Chromosome_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMutationRate, kEidosValueMaskNULL))->AddNumeric("rates")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setRecombinationRate, kEidosValueMaskNULL))->AddNumeric("rates")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}





































































