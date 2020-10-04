//
//  chromosome.cpp
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


#include "chromosome.h"
#include "eidos_rng.h"
#include "slim_globals.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "slim_sim.h"					// for SLIM_MUTRUN_MAXIMUM_COUNT
#include "individual.h"
#include "subpopulation.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <cmath>
#include <utility>
#include <tuple>


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

Chromosome::Chromosome(SLiMSim *p_sim) :

	sim_(p_sim),
	single_recombination_map_(true), 
	single_mutation_map_(true),
	lookup_mutation_H_(nullptr), lookup_mutation_M_(nullptr), lookup_mutation_F_(nullptr), 
	lookup_recombination_H_(nullptr), lookup_recombination_M_(nullptr), lookup_recombination_F_(nullptr),
	exp_neg_overall_mutation_rate_H_(0.0), exp_neg_overall_mutation_rate_M_(0.0), exp_neg_overall_mutation_rate_F_(0.0),
	exp_neg_overall_recombination_rate_H_(0.0), exp_neg_overall_recombination_rate_M_(0.0), exp_neg_overall_recombination_rate_F_(0.0), 
	
#ifndef USE_GSL_POISSON
	probability_both_0_H_(0.0), probability_both_0_OR_mut_0_break_non0_H_(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_H_(0.0),
	probability_both_0_M_(0.0), probability_both_0_OR_mut_0_break_non0_M_(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_(0.0),
	probability_both_0_F_(0.0), probability_both_0_OR_mut_0_break_non0_F_(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_(0.0), 
#endif
	
	last_position_(0),
	overall_mutation_rate_H_(0.0), overall_mutation_rate_M_(0.0), overall_mutation_rate_F_(0.0),
	overall_mutation_rate_H_userlevel_(0.0), overall_mutation_rate_M_userlevel_(0.0), overall_mutation_rate_F_userlevel_(0.0),
	overall_recombination_rate_H_(0.0), overall_recombination_rate_M_(0.0), overall_recombination_rate_F_(0.0),
	overall_recombination_rate_H_userlevel_(0.0), overall_recombination_rate_M_userlevel_(0.0), overall_recombination_rate_F_userlevel_(0.0),
	using_DSB_model_(false), non_crossover_fraction_(0.0), gene_conversion_avg_length_(0.0), gene_conversion_inv_half_length_(0.0), simple_conversion_fraction_(0.0), mismatch_repair_bias_(0.0),
	last_position_mutrun_(0)
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
	
	// Dispose of any nucleotide sequence
	delete ancestral_seq_buffer_;
	ancestral_seq_buffer_ = nullptr;
	
	// Dispose of all genomic elements, which we own
	for (GenomicElement *element : genomic_elements_)
		delete element;
}

// initialize the random lookup tables used by Chromosome to draw mutation and recombination events
void Chromosome::InitializeDraws(void)
{
	if (genomic_elements_.size() == 0)
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
	
	for (unsigned int i = 0; i < genomic_elements_.size(); i++) 
	{ 
		if (genomic_elements_[i]->end_position_ > last_position_)
			last_position_ = genomic_elements_[i]->end_position_;
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
	
	// Patch the hotspot end vector if it is empty; see setHotspotMap() and initializeHotspotMap().
	// Basically, the length of the chromosome might not have been known yet when the user set the map.
	// This is done for the mutation rate maps in _InitializeOneMutationMap(); we do it here for the hotspot map.
	// Here we also fill in a rate of 1.0 if no rate was supplied
	if ((hotspot_multipliers_H_.size() == 0) && (hotspot_multipliers_M_.size() == 0) && (hotspot_multipliers_F_.size() == 0))
		hotspot_multipliers_H_.emplace_back(1.0);
	if ((hotspot_end_positions_H_.size() == 0) && (hotspot_multipliers_H_.size() == 1))
		hotspot_end_positions_H_.emplace_back(last_position_);
	if ((hotspot_end_positions_M_.size() == 0) && (hotspot_multipliers_M_.size() == 1))
		hotspot_end_positions_M_.emplace_back(last_position_);
	if ((hotspot_end_positions_F_.size() == 0) && (hotspot_multipliers_F_.size() == 1))
		hotspot_end_positions_F_.emplace_back(last_position_);
	
	// Now remake our mutation map info, which we delegate to _InitializeOneMutationMap()
	if (single_mutation_map_)
	{
		_InitializeOneMutationMap(lookup_mutation_H_, mutation_end_positions_H_, mutation_rates_H_, overall_mutation_rate_H_userlevel_, overall_mutation_rate_H_, exp_neg_overall_mutation_rate_H_, mutation_subranges_H_);
		
		// Copy the H rates into the M and F ivars, so that they can be used by DrawMutationAndBreakpointCounts() if needed
		overall_mutation_rate_M_userlevel_ = overall_mutation_rate_F_userlevel_ = overall_mutation_rate_H_userlevel_;
		overall_mutation_rate_M_ = overall_mutation_rate_F_ = overall_mutation_rate_H_;
		exp_neg_overall_mutation_rate_M_ = exp_neg_overall_mutation_rate_F_ = exp_neg_overall_mutation_rate_H_;
	}
	else
	{
		_InitializeOneMutationMap(lookup_mutation_M_, mutation_end_positions_M_, mutation_rates_M_, overall_mutation_rate_M_userlevel_, overall_mutation_rate_M_, exp_neg_overall_mutation_rate_M_, mutation_subranges_M_);
		_InitializeOneMutationMap(lookup_mutation_F_, mutation_end_positions_F_, mutation_rates_F_, overall_mutation_rate_F_userlevel_, overall_mutation_rate_F_, exp_neg_overall_mutation_rate_F_, mutation_subranges_F_);
	}
	
	// Now remake our recombination map info, which we delegate to _InitializeOneRecombinationMap()
	any_recombination_rates_05_ = false;
	
	if (single_recombination_map_)
	{
		_InitializeOneRecombinationMap(lookup_recombination_H_, recombination_end_positions_H_, recombination_rates_H_, overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_, overall_recombination_rate_H_userlevel_);
		
		// Copy the H rates into the M and F ivars, so that they can be used by DrawMutationAndBreakpointCounts() if needed
		overall_recombination_rate_M_ = overall_recombination_rate_F_ = overall_recombination_rate_H_;
		exp_neg_overall_recombination_rate_M_ = exp_neg_overall_recombination_rate_F_ = exp_neg_overall_recombination_rate_H_;
		overall_recombination_rate_M_userlevel_ = overall_recombination_rate_F_userlevel_ = overall_recombination_rate_H_userlevel_;
	}
	else
	{
		_InitializeOneRecombinationMap(lookup_recombination_M_, recombination_end_positions_M_, recombination_rates_M_, overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_, overall_recombination_rate_M_userlevel_);
		_InitializeOneRecombinationMap(lookup_recombination_F_, recombination_end_positions_F_, recombination_rates_F_, overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_, overall_recombination_rate_F_userlevel_);
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
		mutrun_length_ = (slim_position_t)ceil((last_position_ + 1) / (double)mutrun_count_);
		
		if (SLiM_verbosity_level >= 2)
			SLIM_OUTSTREAM << std::endl << "// Override mutation run count = " << mutrun_count_ << ", run length = " << mutrun_length_ << std::endl;
	}
	else
	{
		// The user has not supplied a count, so we will conduct experiments to find the best count;
		// for simplicity we will just always start with a single run, since that is often best anyway
		mutrun_count_ = 1;
		mutrun_length_ = (slim_position_t)ceil((last_position_ + 1) / (double)mutrun_count_);
		
		// When we are running experiments, the mutation run length needs to be a power of two so that it can be divided evenly,
		// potentially a fairly large number of times.  We impose a maximum mutrun count of SLIM_MUTRUN_MAXIMUM_COUNT, so
		// actually it needs to just be an even multiple of SLIM_MUTRUN_MAXIMUM_COUNT, not an exact power of two.
		mutrun_length_ = (slim_position_t)round(ceil(mutrun_length_ / (double)SLIM_MUTRUN_MAXIMUM_COUNT) * SLIM_MUTRUN_MAXIMUM_COUNT);
		
		if (SLiM_verbosity_level >= 2)
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
void Chromosome::_InitializeOneRecombinationMap(gsl_ran_discrete_t *&p_lookup, std::vector<slim_position_t> &p_end_positions, std::vector<double> &p_rates, double &p_overall_rate, double &p_exp_neg_overall_rate, double &p_overall_rate_userlevel)
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
	
	// BCH 11 April 2018: Fixing this code to use Peter Ralph's reparameterization that corrects for the way we use these rates
	// downstream.  So the rates we are passed in p_rates are from the user, and they represent "ancestry breakpoint" rates: the
	// desired probability of a crossover from one base to the next.  Downstream, when we actually generate breakpoints, we will
	// draw the number of breakpoints from a Poisson distribution with the overall rate of p_overall_rate that we sum up here,
	// then we will draw the location of each breakpoint from the p_lookup table we build here, and then we will sort and unique
	// those breakpoints so that we only ever have one breakpoint, at most, between any two bases.  That uniquing is the key step
	// that drives this reparameterization.  We need to choose a reparameterized lambda value for the Poisson draw such that the
	// probability P[Poisson(lambda) >= 1] == rate.  Simply using lambda=rate is a good approximation when rate is small, but when
	// rate is larger than 0.01 it starts to break down, and when rate is 0.5 it is quite inaccurate; P[Poisson(0.5) >= 1] ~= 0.39.
	// So we reparameterize according to the formula:
	//
	//		lambda = -log(1 - p)
	//
	// where p is the probability requested by the user and lambda is the expected mean used for the Poisson draw to ensure that
	// P[Poisson(lambda) >= 1] == p.  Note that this formula blows up for p >= 1, and gets numerically unstable as it approaches
	// that limit; the lambda values chosen will become very large.  Since values above 0.5 are unbiological anyway, we do not
	// allow them; all recombination rates must be <= 0.5 in SLiM, as of this change.  0.5 represents complete independence;
	// we do not allow "anti-linkage" at present since it seems to be unbiological.  If somebody argues with us on that, we could
	// extend the limit up to, say, 0.9 without things blowing up badly, but 1.0 is out of the question.
	//
	// Here we also calculate the overall recombination rate in reparameterized units, which is what we actually use to draw
	// breakpoints, *and* the overall recombination rate in non-reparameterized units (as in SLiM 2.x and before), which is what
	// we report to the user if they ask for it (representing the expected number of crossovers along the chromosome).  The
	// reparameterized overall recombination rate is the expected number of breakpoints we will generate *before* uniquing,
	// which has no meaning for the user.
	std::vector<double> reparameterized_rates;
	
	reparameterized_rates.reserve(p_rates.size());
	
	for (double user_rate : p_rates)
	{
		// this bounds check should have been done already externally to us, but no harm in making sure...
		if ((user_rate < 0.0) || (user_rate > 0.5))
			EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): all recombination rates in SLiM must be in [0.0, 0.5]." << EidosTerminate();
		
#if 1
		// This is the new recombination-rate paradigm that compensates for uniquing of breakpoints, etc.
		
		// keep track of whether any recombination rates in the model are 0.5; if so, gene conversion needs to exclude those positions
		if (user_rate == 0.5)
			any_recombination_rates_05_ = true;
		
		reparameterized_rates.emplace_back(-log(1.0 - user_rate));
		
		/*
		 A good model to test that the recombination rate observed is actually the crossover rate requested:
		
		initialize() {
		   defineConstant("L", 10);		// 10 breakpoint positions, so 11 bases
		   defineConstant("N", 1000);		// 1000 diploids
		
		   initializeMutationRate(0);
		   initializeMutationType("m1", 0.5, "f", 0.0);
		   initializeGenomicElementType("g1", m1, 1.0);
		   initializeGenomicElement(g1, 0, L);
		   initializeRecombinationRate(0.5);
		}
		1 {
		   sim.addSubpop("p1", N);
		   sim.tag = 0;
		}
		recombination() {
		   sim.tag = sim.tag + size(unique(breakpoints));
		   return F;
		}
		10000 late() {
		   print(sim.tag / (10000 * N * 2 * L));
		}

		 */
#else
		// This is the old recombination-rate paradigm, which should be used only for testing output compatibility in tests
#warning old recombination-rate paradigm enabled!
		
		reparameterized_rates.emplace_back(user_rate);
#endif
	}
	
	// Calculate the overall recombination rate and the lookup table for breakpoints
	std::vector<double> B(reparameterized_rates.size());
	std::vector<double> B_userlevel(reparameterized_rates.size());
	
	B_userlevel[0] = p_rates[0] * static_cast<double>(p_end_positions[0]);
	B[0] = reparameterized_rates[0] * static_cast<double>(p_end_positions[0]);	// No +1 here, because the position to the left of the first base is not a valid recombination position.
																	// So a 1-base model (position 0 to 0) has an end of 0, and thus an overall rate of 0.  This means that
																	// gsl_ran_discrete_preproc() is given an interval with rate 0, but that seems OK.  BCH 4 April 2016
	for (unsigned int i = 1; i < reparameterized_rates.size(); i++)
	{
		double length = static_cast<double>(p_end_positions[i] - p_end_positions[i - 1]);
		
		B_userlevel[i] = p_rates[i] * length;
		B[i] = reparameterized_rates[i] * length;
	}
	
	p_overall_rate_userlevel = Eidos_ExactSum(B_userlevel.data(), p_rates.size());
	p_overall_rate = Eidos_ExactSum(B.data(), reparameterized_rates.size());
	
	// All the recombination machinery below uses the reparameterized rates
#ifndef USE_GSL_POISSON
	p_exp_neg_overall_rate = Eidos_FastRandomPoisson_PRECALCULATE(p_overall_rate);				// exp(-mu); can be 0 due to underflow
#endif
	
	if (p_lookup)
		gsl_ran_discrete_free(p_lookup);
	
	p_lookup = gsl_ran_discrete_preproc(reparameterized_rates.size(), B.data());
}

// initialize one mutation map, used internally by InitializeDraws() to avoid code duplication
void Chromosome::_InitializeOneMutationMap(gsl_ran_discrete_t *&p_lookup, std::vector<slim_position_t> &p_end_positions, std::vector<double> &p_rates, double &p_requested_overall_rate, double &p_overall_rate, double &p_exp_neg_overall_rate, std::vector<GESubrange> &p_subranges)
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
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): mutation rate endpoints do not cover the full chromosome." << EidosTerminate();
	
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
	
	// We need to work with a *sorted* genomic elements vector here.  We sort it internally here, rather than in
	// genomic_elements_, so that genomic_elements_ reflects exactly what they user gave us (mostly for backward
	// compatibility).
	std::vector<GenomicElement *> sorted_ge_vec = genomic_elements_;
	
	std::sort(sorted_ge_vec.begin(), sorted_ge_vec.end(), [](GenomicElement *ge1, GenomicElement *ge2) {return ge1->start_position_ < ge2->start_position_;});
	
	// This code deals in two different currencies: *requested* mutation rates and *adjusted* mutation rates.
	// This stems from the fact that, beginning in SLiM 3.5, we unique mutation positions as we're generating
	// gametes.  This uniquing means that some drawn positions may be filtered out, resulting in a realized
	// mutation rate that is lower than the requested rate (see section 22.2.3 in the manual for discussion).
	// We want to compensate for this effect, so that the realized rate is equal to the requested rate.  To do
	// that, we calculate adjusted rates that, after uniquing, will result in the requested rate.  However, we
	// want to hide this implementation detail from the user; the rate they see in Eidos from Chromosome should
	// be the same as the rate they requested originally, not the adjusted rate.  So we do calculations with
	// both, so that we can satisfy that requirement.  The adjustment is done piecewise, so that the adjusted
	// rate for each GESubrange is correct.  If we have a nucleotide-based matation matrix that leads to some
	// rejection sampling, that will be based on the ratios of the original rates, but it will be done *after*
	// positions have been drawn and uniqued, so that should also be correct, I think.  BCH 5 Sep. 2020
	std::vector<double> A;	// a vector of requested subrange weights
	std::vector<double> B;	// a vector of adjusted subrange weights
	unsigned int mutrange_index = 0;
	slim_position_t end_of_previous_mutrange = -1;
	
	for (unsigned int ge_index = 0; ge_index < sorted_ge_vec.size(); ge_index++) 
	{
		GenomicElement &ge = *sorted_ge_vec[ge_index];
		
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
				double requested_rate = p_rates[mutrange_index];
				
				if (requested_rate >= 1.0)
					EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): requested mutation rate is >= 1.0." << EidosTerminate();
				
				double adjusted_rate = -log1p(-requested_rate);
				double requested_subrange_weight = requested_rate * subrange_length;
				double adjusted_subrange_weight = adjusted_rate * subrange_length;
				
				A.emplace_back(requested_subrange_weight);
				B.emplace_back(adjusted_subrange_weight);
				p_subranges.emplace_back(&ge, subrange_start, subrange_end);
				
				// Now we need to decide whether to advance ge_index or not; advancing mutrange_index here is not needed
				if (end_of_mutrange >= ge.end_position_)
					break;
			}
			
			end_of_previous_mutrange = end_of_mutrange;
		}
	}
	
	p_requested_overall_rate = Eidos_ExactSum(A.data(), A.size());
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

int Chromosome::DrawSortedUniquedMutationPositions(int p_count, IndividualSex p_sex, std::vector<std::pair<slim_position_t, GenomicElement *>> &p_positions)
{
	// BCH 1 September 2020: This method generates a vector of positions, sorted and uniqued for the caller.  This avoid various issues
	// with two mutations occurring at the same position in the same gamete.  For example, nucleotide states in the tree-seq tables could
	// end up in a state that would normally be illegal, such as an A->G mutation followed by a G->G mutation, because SLiM's code wouldn't
	// understand that the second mutation occurred on the background of the first; it would think that both mutations were A->G, erroneously.
	// This also messed up uniquing of mutations with a mutation() callback in SLiM 3.5; you might want all mutations representing a given
	// nucleotide at a given position to share the same mutation object (IBS instead of IBD), but if two mutations occurred at the same
	// position in the same gamete, the first might be accepted, and then second – which should unique down to the first – would also be
	// accepted.  The right fix seems to be to unique the drawn mutation positions before creating mutations, avoiding all these issues;
	// multiple mutations at the same position in the same gamete seems unbiological anyway.
	gsl_ran_discrete_t *lookup;
	const std::vector<GESubrange> *subranges;
	
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
	
	// draw all the positions, and keep track of the genomic element type for each
	for (int i = 0; i < p_count; ++i)
	{
		int mut_subrange_index = static_cast<int>(gsl_ran_discrete(EIDOS_GSL_RNG, lookup));
		const GESubrange &subrange = (*subranges)[mut_subrange_index];
		GenomicElement *source_element = subrange.genomic_element_ptr_;
		
		// Draw the position along the chromosome for the mutation, within the genomic element
		slim_position_t position = subrange.start_position_ + static_cast<slim_position_t>(Eidos_rng_uniform_int_MT64(subrange.end_position_ - subrange.start_position_ + 1));
		// old 32-bit position not MT64 code:
		//slim_position_t position = subrange.start_position_ + static_cast<slim_position_t>(Eidos_rng_uniform_int(EIDOS_GSL_RNG, (uint32_t)(subrange.end_position_ - subrange.start_position_ + 1)));
		
		p_positions.emplace_back(std::pair<slim_position_t, GenomicElement *>(position, source_element));
	}
	
	// sort and unique by position; 1 and 2 mutations are particularly common, so try to speed those up
	if (p_count > 1)
	{
		if (p_count == 2)
		{
			if (p_positions[0].first > p_positions[1].first)
				std::swap(p_positions[0], p_positions[1]);
			else if (p_positions[0].first == p_positions[1].first)
				p_positions.resize(1);
		}
		else
		{
			std::sort(p_positions.begin(), p_positions.end(), [](const std::pair<slim_position_t, GenomicElement *> &p1, const std::pair<slim_position_t, GenomicElement *> &p2) { return p1.first < p2.first; });
			auto unique_iter = std::unique(p_positions.begin(), p_positions.end(), [](const std::pair<slim_position_t, GenomicElement *> &p1, const std::pair<slim_position_t, GenomicElement *> &p2) { return p1.first == p2.first; });
			p_positions.resize(std::distance(p_positions.begin(), unique_iter));
		}
	}
	
	return (int)p_positions.size();
}

// draw a new mutation, based on the genomic element types present and their mutational proclivities
MutationIndex Chromosome::DrawNewMutation(std::pair<slim_position_t, GenomicElement *> &p_position, slim_objectid_t p_subpop_index, slim_generation_t p_generation) const
{
	const GenomicElement &source_element = *(p_position.second);
	const GenomicElementType &genomic_element_type = *(source_element.genomic_element_type_ptr_);
	MutationType *mutation_type_ptr = genomic_element_type.DrawMutationType();
	
	double selection_coeff = mutation_type_ptr->DrawSelectionCoefficient();
	
	// NOTE THAT THE STACKING POLICY IS NOT ENFORCED HERE, SINCE WE DO NOT KNOW WHAT GENOME WE WILL BE INSERTED INTO!  THIS IS THE CALLER'S RESPONSIBILITY!
	MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
	
	// A nucleotide value of -1 is always used here; in nucleotide-based models this gets patched later, but that is sequence-dependent and background-dependent
	Mutation *mutation = gSLiM_Mutation_Block + new_mut_index;
	
	new (mutation) Mutation(mutation_type_ptr, p_position.first, selection_coeff, p_subpop_index, p_generation, -1);
	
	// addition to the main registry and the muttype registries will happen if the new mutation clears the stacking policy
	
	return new_mut_index;
}

// apply mutation() to a generated mutation; we might return nullptr (proposed mutation rejected), the original proposed mutation (it was accepted), or a replacement Mutation *
Mutation *Chromosome::ApplyMutationCallbacks(Mutation *p_mut, Genome *p_genome, GenomicElement *p_genomic_element, int8_t p_original_nucleotide, std::vector<SLiMEidosBlock*> &p_mutation_callbacks) const
{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	slim_objectid_t mutation_type_id = p_mut->mutation_type_ptr_->mutation_type_id_;
	
	// note the focal child during the callback, so we can prevent illegal operations during the callback
	SLiMEidosBlockType old_executing_block_type = sim_->executing_block_type_;
	sim_->executing_block_type_ = SLiMEidosBlockType::SLiMEidosMutationCallback;
	
	bool mutation_accepted = true, mutation_replaced = false;
	
	for (SLiMEidosBlock *mutation_callback : p_mutation_callbacks)
	{
		if (mutation_callback->active_)
		{
			// The callback is active and matches the mutation type id of the mutation, so we need to execute it
			// This code is similar to Population::ExecuteScript, but we set up an additional symbol table, and we use the return value
			slim_objectid_t callback_mutation_type_id = mutation_callback->mutation_type_id_;
			
			if ((callback_mutation_type_id == -1) || (callback_mutation_type_id == mutation_type_id))
			{
				EidosValue_Object_singleton local_mut(p_mut, gSLiM_Mutation_Class);
				EidosValue_Int_singleton local_originalNuc(p_original_nucleotide);
				
				// We need to actually execute the script; we start a block here to manage the lifetime of the symbol table
				{
					EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &sim_->SymbolTable());
					EidosSymbolTable client_symbols(EidosSymbolTableType::kVariablesTable, &callback_symbols);
					EidosFunctionMap &function_map = sim_->FunctionMap();
					EidosInterpreter interpreter(mutation_callback->compound_statement_node_, client_symbols, function_map, sim_);
					
					if (mutation_callback->contains_self_)
						callback_symbols.InitializeConstantSymbolEntry(mutation_callback->SelfSymbolTableEntry());		// define "self"
					
					// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
					// We can use that method because we know the lifetime of the symbol table is shorter than that of
					// the value objects, and we know that the values we are setting here will not change (the objects
					// referred to by the values may change, but the values themselves will not change).
					if (mutation_callback->contains_mut_)
					{
						local_mut.StackAllocated();			// prevent Eidos_intrusive_ptr from trying to delete this
						callback_symbols.InitializeConstantSymbolEntry(gID_mut, EidosValue_SP(&local_mut));
					}
					if (mutation_callback->contains_parent_)
						callback_symbols.InitializeConstantSymbolEntry(gID_parent, p_genome->OwningIndividual()->CachedEidosValue());
					if (mutation_callback->contains_genome_)
						callback_symbols.InitializeConstantSymbolEntry(gID_genome, p_genome->CachedEidosValue());
					if (mutation_callback->contains_element_)
						callback_symbols.InitializeConstantSymbolEntry(gID_element, p_genomic_element->CachedEidosValue());
					if (mutation_callback->contains_subpop_)
						callback_symbols.InitializeConstantSymbolEntry(gID_subpop, p_genome->OwningSubpopulation()->SymbolTableEntry().second);
					if (mutation_callback->contains_originalNuc_)
					{
						local_originalNuc.StackAllocated();		// prevent Eidos_intrusive_ptr from trying to delete this
						callback_symbols.InitializeConstantSymbolEntry(gID_originalNuc, EidosValue_SP(&local_originalNuc));
					}
					
					try
					{
						// Interpret the script; the result from the interpretation must be a singleton logical, T if the mutation is accepted, F if it is rejected, or a Mutation object
						EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(mutation_callback->script_);
						EidosValue *result = result_SP.get();
						EidosValueType resultType = result->Type();
						int resultCount = result->Count();
						
						if ((resultType == EidosValueType::kValueLogical) && (resultCount == 1))
						{
							mutation_accepted = result->LogicalAtIndex(0, nullptr);
						}
						else if ((resultType == EidosValueType::kValueObject) && (((EidosValue_Object *)result)->Class() == gSLiM_Mutation_Class) && (resultCount == 1))
						{
							Mutation *replacementMutation = (Mutation *)result->ObjectElementAtIndex(0, mutation_callback->identifier_token_);
							
							if (replacementMutation == p_mut)
							{
								// returning the proposed mutation is the same as accepting it
								mutation_accepted = true;
							}
							else
							{
								// First, check that the mutation fits the requirements: position, mutation type, nucleotide; we are already set up for the mutation type and can't change in
								// mid-stream (would we switch to running mutation() callbacks for the new type??), and we have already chosen the nucleotide for consistency with the background
								// BCH 3 September 2020: A thread on slim-discuss (https://groups.google.com/forum/#!topic/slim-discuss/ALnZrzIroKY) has convinced me that it would be useful
								// to be able to return a replacement mutation of a different mutation type, so that you can generate mutations of a single type up front and multifurcate that
								// generation process into multiple mutation types on the other side (perhaps with different dominance coefficients, for example).  So I have relaxed the
								// mutation type requirement here.  If the mutation type is changed in the replacement, we switch to running callbacks for the new mutation type, but we do not
								// go back to the beginning; we're running callbacks in the order they were declared, and just switching which type we're running, mid-stream.
								// BCH 4 September 2020: Peter has persuaded me to relax the nucleotide restriction as well.  If the user changes the nucleotide, it may produce surprising
								// results (a mutation that doesn't change the nucleotide, like G->G, or a deviation from requested mutation-matrix probabilities), but that's the user's
								// responsibility to deal with; it can be a useful thing to do, Peter thinks.
								if (replacementMutation->position_ != p_mut->position_)
									EIDOS_TERMINATION << "ERROR (Chromosome::ApplyMutationCallbacks): a replacement mutation from a mutation() callback must match the position of the proposed mutation." << EidosTerminate(mutation_callback->identifier_token_);
								if ((replacementMutation->state_ == MutationState::kRemovedWithSubstitution) || (replacementMutation->state_ == MutationState::kFixedAndSubstituted))
									EIDOS_TERMINATION << "ERROR (Chromosome::ApplyMutationCallbacks): a replacement mutation from a mutation() callback cannot be fixed/substituted." << EidosTerminate(mutation_callback->identifier_token_);
								if (replacementMutation->mutation_type_ptr_ != p_mut->mutation_type_ptr_)
								{
									//EIDOS_TERMINATION << "ERROR (Chromosome::ApplyMutationCallbacks): a replacement mutation from a mutation() callback must match the mutation type of the proposed mutation." << EidosTerminate(mutation_callback->identifier_token_);
									mutation_type_id = p_mut->mutation_type_ptr_->mutation_type_id_;
								}
								//if (replacementMutation->nucleotide_ != p_mut->nucleotide_)
								//	EIDOS_TERMINATION << "ERROR (Chromosome::ApplyMutationCallbacks): a replacement mutation from a mutation() callback must match the nucleotide of the proposed mutation." << EidosTerminate(mutation_callback->identifier_token_);
								
								// It fits the bill, so replace the original proposed mutation with this new mutation; note we don't fix local_mut, it's about to go out of scope anyway
								p_mut = replacementMutation;
								mutation_replaced = true;
								mutation_accepted = true;
							}
						}
						else
							EIDOS_TERMINATION << "ERROR (Chromosome::ApplyMutationCallbacks): mutation() callbacks must provide a return value that is either a singleton logical or a singleton Mutation object." << EidosTerminate(mutation_callback->identifier_token_);
						
						// Output generated by the interpreter goes to our output stream
						interpreter.FlushExecutionOutputToStream(SLIM_OUTSTREAM);
					}
					catch (...)
					{
						// Emit final output even on a throw, so that stop() messages and such get printed
						interpreter.FlushExecutionOutputToStream(SLIM_OUTSTREAM);
						
						throw;
					}
				}
				
				if (!mutation_accepted)
					break;
			}
		}
	}
	
	// If a replacement mutation has been accepted at this point, we now check that it is not already present in the background genome; if it is present, the mutation is a no-op (implemented as a rejection)
	if (mutation_replaced && mutation_accepted)
	{
		if (p_genome->contains_mutation(p_mut->BlockIndex()))
			mutation_accepted = false;
	}
	
	sim_->executing_block_type_ = old_executing_block_type;
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(sim_->profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMutationCallback)]);
#endif
	
	return (mutation_accepted ? p_mut : nullptr);
}

// draw a new mutation with reference to the genomic background upon which it is occurring, for nucleotide-based models and/or mutation() callbacks
MutationIndex Chromosome::DrawNewMutationExtended(std::pair<slim_position_t, GenomicElement *> &p_position, slim_objectid_t p_subpop_index, slim_generation_t p_generation, Genome *parent_genome_1, Genome *parent_genome_2, std::vector<slim_position_t> *all_breakpoints, std::vector<SLiMEidosBlock*> *p_mutation_callbacks) const
{
	slim_position_t position = p_position.first;
	GenomicElement &source_element = *(p_position.second);
	const GenomicElementType &genomic_element_type = *(source_element.genomic_element_type_ptr_);
	
	// Determine which parental genome the mutation will be atop (so we can get the genetic context for it)
	bool on_first_genome = true;
	
	if (all_breakpoints)
	{
		for (slim_position_t breakpoint : *all_breakpoints)
		{
			if (breakpoint > position)
				break;
			
			on_first_genome = !on_first_genome;
		}
	}
	
	Genome *background_genome = (on_first_genome ? parent_genome_1 : parent_genome_2);
	
	// Determine whether the mutation will be created at all, and if it is, what nucleotide to use
	int8_t original_nucleotide = -1, nucleotide = -1;
	
	if (genomic_element_type.mutation_matrix_)
	{
		EidosValue_Float_vector *mm = genomic_element_type.mutation_matrix_.get();
		int mm_count = mm->Count();
		
		if (mm_count == 16)
		{
			// The mutation matrix only cares about the single-nucleotide context; figure it out
			GenomeWalker walker(background_genome);
			
			walker.MoveToPosition(position);
			
			while (!walker.Finished())
			{
				Mutation *mut = walker.CurrentMutation();
				slim_position_t pos = mut->position_;
				
				// pos >= position is guaranteed by MoveToPosition()
				if (pos == position)
				{
					int8_t mut_nuc = mut->nucleotide_;
					
					if (mut_nuc != -1)
						original_nucleotide = mut_nuc;
					
					walker.NextMutation();
				}
				else
				{
					break;
				}
			}
			
			// No mutation is present at the position, so the background comes from the ancestral sequence
			if (original_nucleotide == -1)
				original_nucleotide = (int8_t)ancestral_seq_buffer_->NucleotideAtIndex(position);
			
			// OK, now we know the background nucleotide; determine the mutation rates to derived nucleotides
			double *nuc_thresholds = genomic_element_type.mm_thresholds + original_nucleotide * 4;
			double draw = Eidos_rng_uniform(EIDOS_GSL_RNG);
			
			if (draw < nuc_thresholds[0])		nucleotide = 0;
			else if (draw < nuc_thresholds[1])	nucleotide = 1;
			else if (draw < nuc_thresholds[2])	nucleotide = 2;
			else if (draw < nuc_thresholds[3])	nucleotide = 3;
			else {
				// The mutation is an excess mutation, the result of an overall mutation rate on this genetic background
				// that is less than the maximum overall mutation rate for any genetic background; it should be discarded,
				// as if this mutation event never occurred at all.  We signal this by returning -1.
				return -1;
			}
		}
		else if (mm_count == 256)
		{
			// The mutation matrix cares about the trinucleotide context; figure it out
			int8_t background_nuc1 = -1, background_nuc3 = -1;
			GenomeWalker walker(background_genome);
			
			walker.MoveToPosition(position - 1);
			
			while (!walker.Finished())
			{
				Mutation *mut = walker.CurrentMutation();
				slim_position_t pos = mut->position_;
				
				// pos >= position - 1 is guaranteed by MoveToPosition()
				if (pos == position - 1)
				{
					int8_t mut_nuc = mut->nucleotide_;
					
					if (mut_nuc != -1)
						background_nuc1 = mut_nuc;
					
					walker.NextMutation();
				}
				else if (pos == position)
				{
					int8_t mut_nuc = mut->nucleotide_;
					
					if (mut_nuc != -1)
						original_nucleotide = mut_nuc;
					
					walker.NextMutation();
				}
				else if (pos == position + 1)
				{
					int8_t mut_nuc = mut->nucleotide_;
					
					if (mut_nuc != -1)
						background_nuc3 = mut_nuc;
					
					walker.NextMutation();
				}
				else
				{
					break;
				}
			}
			
			// No mutation is present at the position, so the background comes from the ancestral sequence
			// If a base in the trinucleotide is off the end of the chromosome, we assume it is an A; that is arbitrary,
			// but should avoid skewed rates associated with the dynamics of C/G in certain sequences (like CpG)
			if (background_nuc1 == -1)
				background_nuc1 = ((position == 0) ? 0 : (int8_t)ancestral_seq_buffer_->NucleotideAtIndex(position - 1));
			if (original_nucleotide == -1)
				original_nucleotide = (int8_t)ancestral_seq_buffer_->NucleotideAtIndex(position);
			if (background_nuc3 == -1)
				background_nuc3 = ((position == last_position_) ? 0 : (int8_t)ancestral_seq_buffer_->NucleotideAtIndex(position + 1));
			
			// OK, now we know the background nucleotide; determine the mutation rates to derived nucleotides
			int trinuc = ((int)background_nuc1) * 16 + ((int)original_nucleotide) * 4 + (int)background_nuc3;
			double *nuc_thresholds = genomic_element_type.mm_thresholds + trinuc * 4;
			double draw = Eidos_rng_uniform(EIDOS_GSL_RNG);
			
			if (draw < nuc_thresholds[0])		nucleotide = 0;
			else if (draw < nuc_thresholds[1])	nucleotide = 1;
			else if (draw < nuc_thresholds[2])	nucleotide = 2;
			else if (draw < nuc_thresholds[3])	nucleotide = 3;
			else {
				// The mutation is an excess mutation, the result of an overall mutation rate on this genetic background
				// that is less than the maximum overall mutation rate for any genetic background; it should be discarded,
				// as if this mutation event never occurred at all.  We signal this by returning -1.
				return -1;
			}
		}
		else
			EIDOS_TERMINATION << "ERROR (Chromosome::DrawNewMutationExtended): (internal error) unexpected mutation matrix size." << EidosTerminate();
	}
	
	// Draw mutation type and selection coefficient, and create the new mutation
	MutationType *mutation_type_ptr = genomic_element_type.DrawMutationType();
	
	double selection_coeff = mutation_type_ptr->DrawSelectionCoefficient();
	
	// NOTE THAT THE STACKING POLICY IS NOT ENFORCED HERE!  THIS IS THE CALLER'S RESPONSIBILITY!
	MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
	Mutation *mutation = gSLiM_Mutation_Block + new_mut_index;
	
	new (mutation) Mutation(mutation_type_ptr, position, selection_coeff, p_subpop_index, p_generation, nucleotide);
	
	// Call mutation() callbacks if there are any
	if (p_mutation_callbacks)
	{
		Mutation *post_callback_mut = ApplyMutationCallbacks(gSLiM_Mutation_Block + new_mut_index, background_genome, &source_element, original_nucleotide, *p_mutation_callbacks);
		
		// If the callback didn't return the proposed mutation, it will not be used; dispose of it
		if (post_callback_mut != mutation)
		{
			//std::cout << "proposed mutation not used, disposing..." << std::endl;
			mutation->Release();
		}
		
		// If it returned nullptr, the mutation event was rejected
		if (post_callback_mut == nullptr)
		{
			//std::cout << "mutation rejected..." << std::endl;
			return -1;
		}
		
		// Otherwise, we will request the addition of whatever mutation it returned (which might be the proposed mutation).
		// Note that if an existing mutation was returned, ApplyMutationCallbacks() guarantees that it is not already present in the background genome.
		MutationIndex post_callback_mut_index = post_callback_mut->BlockIndex();
		
		if (new_mut_index != post_callback_mut_index)
		{
			//std::cout << "replacing mutation!" << std::endl;
			new_mut_index = post_callback_mut_index;
		}
	}
	
	// addition to the main registry and the muttype registries will happen if the new mutation clears the stacking policy
	
	return new_mut_index;
}

// draw a set of uniqued breakpoints according to the "crossover breakpoint" model and run them through recombination() callbacks, returning the final usable set
void Chromosome::DrawCrossoverBreakpoints(IndividualSex p_parent_sex, const int p_num_breakpoints, std::vector<slim_position_t> &p_crossovers) const
{
	// BEWARE! Chromosome::DrawDSBBreakpoints() below must be altered in parallel with this method!
#if DEBUG
	if (using_DSB_model_)
		EIDOS_TERMINATION << "ERROR (Chromosome::DrawCrossoverBreakpoints): (internal error) this method should not be called when the DSB recombination model is being used." << EidosTerminate();
#endif
	
	gsl_ran_discrete_t *lookup;
	const std::vector<slim_position_t> *end_positions;
	
	if (single_recombination_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		lookup = lookup_recombination_H_;
		end_positions = &recombination_end_positions_H_;
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_parent_sex == IndividualSex::kMale)
		{
			lookup = lookup_recombination_M_;
			end_positions = &recombination_end_positions_M_;
		}
		else if (p_parent_sex == IndividualSex::kFemale)
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
		int recombination_interval = static_cast<int>(gsl_ran_discrete(EIDOS_GSL_RNG, lookup));
		
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
		// the breakpoint falls between bases, to the left of the base at the specified number.  This is a consequence of the logic
		// in the crossover-mutation code, which copies mutations as long as their position is *less than* the position of the next
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
			breakpoint = static_cast<slim_position_t>(Eidos_rng_uniform_int_MT64((*end_positions)[recombination_interval]) + 1);
		else
			breakpoint = (*end_positions)[recombination_interval - 1] + 1 + static_cast<slim_position_t>(Eidos_rng_uniform_int_MT64((*end_positions)[recombination_interval] - (*end_positions)[recombination_interval - 1]));
		
		p_crossovers.emplace_back(breakpoint);
	}
	
	// sort and unique
	if (p_num_breakpoints > 2)
	{
		std::sort(p_crossovers.begin(), p_crossovers.end());
		p_crossovers.erase(std::unique(p_crossovers.begin(), p_crossovers.end()), p_crossovers.end());
	}
	else if (p_num_breakpoints == 2)
	{
		// do our own dumb inline sort/unique if we have just two elements, to avoid the calls above
		// I didn't actually test this to confirm that it's faster, but models that generate many
		// breakpoints will generally hit the case above anyway, and models that generate few will
		// suffer only the additional (num_breakpoints == 2) test before falling through...
		slim_position_t bp1 = p_crossovers[0];
		slim_position_t bp2 = p_crossovers[1];
		
		if (bp1 > bp2)
			std::swap(p_crossovers[0], p_crossovers[1]);
		else if (bp1 == bp2)
			p_crossovers.resize(1);
	}
}

// draw a set of uniqued breakpoints according to the "double-stranded break" model and run them through recombination() callbacks, returning the final usable set
// the information returned here also includes a list of heteroduplex regions where mismatches between the two parental strands will need to be resolved
void Chromosome::DrawDSBBreakpoints(IndividualSex p_parent_sex, const int p_num_breakpoints, std::vector<slim_position_t> &p_crossovers, std::vector<slim_position_t> &p_heteroduplex) const
{
	// BEWARE! Chromosome::DrawCrossoverBreakpoints() above must be altered in parallel with this method!
#if DEBUG
	if (!using_DSB_model_)
		EIDOS_TERMINATION << "ERROR (Chromosome::DrawDSBBreakpoints): (internal error) this method should not be called when the crossover breakpoints recombination model is being used." << EidosTerminate();
#endif
	
	gsl_ran_discrete_t *lookup;
	const std::vector<slim_position_t> *end_positions;
	const std::vector<double> *rates;
	
	if (single_recombination_map_)
	{
		// With a single map, we don't care what sex we are passed; same map for all, and sex may be enabled or disabled
		lookup = lookup_recombination_H_;
		end_positions = &recombination_end_positions_H_;
		rates = &recombination_rates_H_;
	}
	else
	{
		// With sex-specific maps, we treat males and females separately, and the individual we're given better be one of the two
		if (p_parent_sex == IndividualSex::kMale)
		{
			lookup = lookup_recombination_M_;
			end_positions = &recombination_end_positions_M_;
			rates = &recombination_rates_M_;
		}
		else if (p_parent_sex == IndividualSex::kFemale)
		{
			lookup = lookup_recombination_F_;
			end_positions = &recombination_end_positions_F_;
			rates = &recombination_rates_F_;
		}
		else
		{
			RecombinationMapConfigError();
		}
	}
	
	// Draw extents and crossover/noncrossover and simple/complex decisions for p_num_breakpoints DSBs; we may not end up using all
	// of them, if the uniquing step reduces the set of DSBs, but we don't want to redraw these things if we have to loop back due
	// to a collision, because such redrawing would be liable to produce bias towards shorter extents.  (Redrawing the crossover/
	// noncrossover and simple/complex decisions would probably be harmless, but it is simpler to just make all decisions up front.)
	static std::vector<std::tuple<slim_position_t, slim_position_t, bool, bool>> dsb_infos;	// using a static prevents reallocation
	dsb_infos.clear();
	
	if (gene_conversion_avg_length_ < 2.0)
	{
		for (int i = 0; i < p_num_breakpoints; i++)
		{
			// If the gene conversion tract mean length is < 2.0, gsl_ran_geometric() will blow up, and we should treat the tract length as zero
			bool noncrossover = (Eidos_rng_uniform(EIDOS_GSL_RNG) <= non_crossover_fraction_);				// tuple position 2
			bool simple = (Eidos_rng_uniform(EIDOS_GSL_RNG) <= simple_conversion_fraction_);				// tuple position 3
			
			dsb_infos.emplace_back(std::tuple<slim_position_t, slim_position_t, bool, bool>(0, 0, noncrossover, simple));
		}
	}
	else
	{
		for (int i = 0; i < p_num_breakpoints; i++)
		{
			slim_position_t extent1 = gsl_ran_geometric(EIDOS_GSL_RNG, gene_conversion_inv_half_length_);	// tuple position 0
			slim_position_t extent2 = gsl_ran_geometric(EIDOS_GSL_RNG, gene_conversion_inv_half_length_);	// tuple position 1
			bool noncrossover = (Eidos_rng_uniform(EIDOS_GSL_RNG) <= non_crossover_fraction_);				// tuple position 2
			bool simple = (Eidos_rng_uniform(EIDOS_GSL_RNG) <= simple_conversion_fraction_);				// tuple position 3
			
			dsb_infos.emplace_back(std::tuple<slim_position_t, slim_position_t, bool, bool>(extent1, extent2, noncrossover, simple));
		}
	}
	
	int try_count = 0;
	
generateDSBs:
	
	if (++try_count > 100)
		EIDOS_TERMINATION << "ERROR (Chromosome::DrawDSBBreakpoints): non-overlapping recombination regions could not be achieved in 100 tries; terminating.  The recombination rate and/or mean gene conversion tract length may be too high." << EidosTerminate();
	
	// First draw DSB points; dsb_points contains positions and a flag for whether the breakpoint is at a rate=0.5 position
	static std::vector<std::pair<slim_position_t, bool>> dsb_points;	// using a static prevents reallocation
	dsb_points.clear();
	
	for (int i = 0; i < p_num_breakpoints; i++)
	{
		slim_position_t breakpoint = 0;
		int recombination_interval = static_cast<int>(gsl_ran_discrete(EIDOS_GSL_RNG, lookup));
		
		if (recombination_interval == 0)
			breakpoint = static_cast<slim_position_t>(Eidos_rng_uniform_int_MT64((*end_positions)[recombination_interval]) + 1);
		else
			breakpoint = (*end_positions)[recombination_interval - 1] + 1 + static_cast<slim_position_t>(Eidos_rng_uniform_int_MT64((*end_positions)[recombination_interval] - (*end_positions)[recombination_interval - 1]));
		
		if ((*rates)[recombination_interval] == 0.5)
			dsb_points.emplace_back(std::pair<slim_position_t, bool>(breakpoint, true));
		else
			dsb_points.emplace_back(std::pair<slim_position_t, bool>(breakpoint, false));
	}
	
	// Sort and unique the resulting DSB vector
	if (p_num_breakpoints > 1)
	{
		std::sort(dsb_points.begin(), dsb_points.end());		// sorts by the first element of the pair
		dsb_points.erase(std::unique(dsb_points.begin(), dsb_points.end()), dsb_points.end());
	}
	
	// Assemble lists of crossover breakpoints and heteroduplex regions, starting from a clean slate
	int final_num_breakpoints = (int)dsb_points.size();
	slim_position_t last_position_used = -1;
	
	p_crossovers.clear();
	p_heteroduplex.clear();
	
	for (int i = 0; i < final_num_breakpoints; i++)
	{
		std::pair<slim_position_t, bool> &dsb_pair = dsb_points[i];
		std::tuple<slim_position_t, slim_position_t, bool, bool> &dsb_info = dsb_infos[i];
		slim_position_t dsb_point = dsb_pair.first;
		
		if (dsb_pair.second)
		{
			// This DSB is at a rate=0.5 point, so we do not generate a gene conversion tract; it just translates directly to a crossover breakpoint
			// Note that we do NOT check non_crossover_fraction_ here; it does not apply to rate=0.5 positions, since they cannot undergo gene conversion
			if (dsb_point <= last_position_used)
				goto generateDSBs;
			
			p_crossovers.push_back(dsb_point);
			last_position_used = dsb_point;
		}
		else
		{
			// This DSB is not at a rate=0.5 point, so we generate a gene conversion tract around it
			slim_position_t tract_start = dsb_point - std::get<0>(dsb_info);
			slim_position_t tract_end = SLiMClampToPositionType(dsb_point + std::get<1>(dsb_info));
			
			// We do not want to allow GC tracts to extend all the way to the chromosome beginning or end
			// This is partly because biologically it seems weird, and partly because a breakpoint at position 0 breaks tree-seq recording
			if (tract_start <= 0)
				goto generateDSBs;
			if (tract_end > last_position_)
				goto generateDSBs;
			
			if (tract_start <= last_position_used)
				goto generateDSBs;
			
			if (tract_start == tract_end)
			{
				// gene conversion tract of zero length, so no tract after all, but we do use non_crossover here
				if (!std::get<2>(dsb_info))
					p_crossovers.push_back(tract_start);
				last_position_used = tract_start;
			}
			else
			{
				// gene conversion tract of non-zero length, so generate the tract
				p_crossovers.push_back(tract_start);
				if (std::get<2>(dsb_info))
					p_crossovers.push_back(tract_end);
				last_position_used = tract_end;
				
				// decide if it is a simple or a complex tract
				if (!std::get<3>(dsb_info))
				{
					// complex gene conversion tract; we need to save it in the list of heteroduplex regions
					p_heteroduplex.push_back(tract_start);
					p_heteroduplex.push_back(tract_end - 1);	// heteroduplex positions are base positions, so the last position is to the left of the GC tract end
				}
			}
		}
	}
}

size_t Chromosome::MemoryUsageForMutationMaps(void)
{
	size_t usage = 0;
	
	usage = (mutation_rates_H_.size() + mutation_rates_M_.size() + mutation_rates_F_.size()) * sizeof(double);
	usage += (mutation_end_positions_H_.size() + mutation_end_positions_M_.size() + mutation_end_positions_F_.size()) * sizeof(slim_position_t);
	usage += (mutation_subranges_H_.size() + mutation_subranges_M_.size() + mutation_subranges_F_.size()) * sizeof(GESubrange);
	usage += (hotspot_multipliers_H_.size() + hotspot_multipliers_M_.size() + hotspot_multipliers_F_.size()) * sizeof(double);
	usage += (hotspot_end_positions_H_.size() + hotspot_end_positions_M_.size() + hotspot_end_positions_F_.size()) * sizeof(slim_position_t);
	
	if (lookup_mutation_H_)
		usage += lookup_mutation_H_->K * (sizeof(size_t) + sizeof(double));
	
	if (lookup_mutation_M_)
		usage += lookup_mutation_M_->K * (sizeof(size_t) + sizeof(double));
	
	if (lookup_mutation_F_)
		usage += lookup_mutation_F_->K * (sizeof(size_t) + sizeof(double));
	
	return usage;
}

size_t Chromosome::MemoryUsageForRecombinationMaps(void)
{
	size_t usage = 0;
	
	usage = (recombination_rates_H_.size() + recombination_rates_M_.size() + recombination_rates_F_.size()) * sizeof(double);
	usage += (recombination_end_positions_H_.size() + recombination_end_positions_M_.size() + recombination_end_positions_F_.size()) * sizeof(slim_position_t);
	
	if (lookup_recombination_H_)
		usage += lookup_recombination_H_->K * (sizeof(size_t) + sizeof(double));
	
	if (lookup_recombination_M_)
		usage += lookup_recombination_M_->K * (sizeof(size_t) + sizeof(double));
	
	if (lookup_recombination_F_)
		usage += lookup_recombination_F_->K * (sizeof(size_t) + sizeof(double));
	
	return usage;
}

size_t Chromosome::MemoryUsageForAncestralSequence(void)
{
	size_t usage = 0;
	
	if (ancestral_seq_buffer_)
	{
		std::size_t length = ancestral_seq_buffer_->size();
		
		usage += ((length + 31) / 32) * sizeof(uint64_t);
	}
	
	return usage;
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
			
			for (GenomicElement *genomic_element : genomic_elements_)
				vec->push_object_element_NORR(genomic_element);
			
			return result_SP;
		}
		case gID_lastPosition:
		{
			if (!cached_value_lastpos_)
				cached_value_lastpos_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(last_position_));
			return cached_value_lastpos_;
		}
			
		case gID_hotspotEndPositions:
		{
			if (!sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotEndPositions is only defined in nucleotide-based models." << EidosTerminate();
			if (!single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotEndPositions is not defined since sex-specific hotspot maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(hotspot_end_positions_H_));
		}
		case gID_hotspotEndPositionsM:
		{
			if (!sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotEndPositionsM is only defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotEndPositionsM is not defined since sex-specific hotspot maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(hotspot_end_positions_M_));
		}
		case gID_hotspotEndPositionsF:
		{
			if (!sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotEndPositionsF is only defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotEndPositionsF is not defined since sex-specific hotspot maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(hotspot_end_positions_F_));
		}
			
		case gID_hotspotMultipliers:
		{
			if (!sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotMultipliers is only defined in nucleotide-based models." << EidosTerminate();
			if (!single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotMultipliers is not defined since sex-specific hotspot maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(hotspot_multipliers_H_));
		}
		case gID_hotspotMultipliersM:
		{
			if (!sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotMultipliersM is only defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotMultipliersM is not defined since sex-specific hotspot maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(hotspot_multipliers_M_));
		}
		case gID_hotspotMultipliersF:
		{
			if (!sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotMultipliersF is only defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property hotspotMultipliersF is not defined since sex-specific hotspot maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(hotspot_multipliers_F_));
		}
			
		case gID_mutationEndPositions:
		{
			if (sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositions is not defined in nucleotide-based models." << EidosTerminate();
			if (!single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositions is not defined since sex-specific mutation rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(mutation_end_positions_H_));
		}
		case gID_mutationEndPositionsM:
		{
			if (sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositionsM is not defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositionsM is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(mutation_end_positions_M_));
		}
		case gID_mutationEndPositionsF:
		{
			if (sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositionsF is not defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationEndPositionsF is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(mutation_end_positions_F_));
		}
			
		case gID_mutationRates:
		{
			if (sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRates is not defined in nucleotide-based models." << EidosTerminate();
			if (!single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRates is not defined since sex-specific mutation rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(mutation_rates_H_));
		}
		case gID_mutationRatesM:
		{
			if (sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRatesM is not defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRatesM is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(mutation_rates_M_));
		}
		case gID_mutationRatesF:
		{
			if (sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRatesF is not defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property mutationRatesF is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(mutation_rates_F_));
		}
			
		case gID_overallMutationRate:
		{
			if (sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRate is not defined in nucleotide-based models." << EidosTerminate();
			if (!single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRate is not defined since sex-specific mutation rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_mutation_rate_H_userlevel_));
		}
		case gID_overallMutationRateM:
		{
			if (sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRateM is not defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRateM is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_mutation_rate_M_userlevel_));
		}
		case gID_overallMutationRateF:
		{
			if (sim_->IsNucleotideBased())
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRateF is not defined in nucleotide-based models." << EidosTerminate();
			if (single_mutation_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallMutationRateF is not defined since sex-specific mutation rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_mutation_rate_F_userlevel_));
		}
			
		case gID_overallRecombinationRate:
		{
			if (!single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallRecombinationRate is not defined since sex-specific recombination rate maps have been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_recombination_rate_H_userlevel_));
		}
		case gID_overallRecombinationRateM:
		{
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallRecombinationRateM is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_recombination_rate_M_userlevel_));
		}
		case gID_overallRecombinationRateF:
		{
			if (single_recombination_map_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property overallRecombinationRateF is not defined since sex-specific recombination rate maps have not been defined." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_recombination_rate_F_userlevel_));
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
		case gID_geneConversionEnabled:
			return using_DSB_model_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
		case gID_geneConversionGCBias:
		{
			if (!using_DSB_model_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property geneConversionGCBias is not defined since the DSB recombination model is not being used." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(mismatch_repair_bias_));
		}
		case gID_geneConversionNonCrossoverFraction:
		{
			if (!using_DSB_model_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property geneConversionNonCrossoverFraction is not defined since the DSB recombination model is not being used." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(non_crossover_fraction_));
		}
		case gID_geneConversionMeanLength:
		{
			if (!using_DSB_model_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property geneConversionMeanLength is not defined since the DSB recombination model is not being used." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gene_conversion_avg_length_));
		}
		case gID_geneConversionSimpleConversionFraction:
		{
			if (!using_DSB_model_)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property geneConversionSimpleConversionFraction is not defined since the DSB recombination model is not being used." << EidosTerminate();
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(simple_conversion_fraction_));
		}
		case gID_tag:
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Chromosome::GetProperty): property tag accessed on chromosome before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
			
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

EidosValue_SP Chromosome::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_ancestralNucleotides:		return ExecuteMethod_ancestralNucleotides(p_method_id, p_arguments, p_interpreter);
		case gID_setAncestralNucleotides:	return ExecuteMethod_setAncestralNucleotides(p_method_id, p_arguments, p_interpreter);
		case gID_setGeneConversion:			return ExecuteMethod_setGeneConversion(p_method_id, p_arguments, p_interpreter);
		case gID_setHotspotMap:				return ExecuteMethod_setHotspotMap(p_method_id, p_arguments, p_interpreter);
		case gID_setMutationRate:			return ExecuteMethod_setMutationRate(p_method_id, p_arguments, p_interpreter);
		case gID_setRecombinationRate:		return ExecuteMethod_setRecombinationRate(p_method_id, p_arguments, p_interpreter);
		case gID_drawBreakpoints:			return ExecuteMethod_drawBreakpoints(p_method_id, p_arguments, p_interpreter);
		default:							return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	– (is)ancestralNucleotides([Ni$ start = NULL], [Ni$ end = NULL], [s$ format = "string"])
//
EidosValue_SP Chromosome::ExecuteMethod_ancestralNucleotides(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// The ancestral sequence is actually kept by SLiMSim, so go get it
	if (!sim_->IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_ancestralNucleotides): ancestralNucleotides() may only be called in nucleotide-based models." << EidosTerminate();
	
	NucleotideArray *sequence = sim_->TheChromosome().AncestralSequence();
	EidosValue *start_value = p_arguments[0].get();
	EidosValue *end_value = p_arguments[1].get();
	
	int64_t start = (start_value->Type() == EidosValueType::kValueNULL) ? 0 : start_value->IntAtIndex(0, nullptr);
	int64_t end = (end_value->Type() == EidosValueType::kValueNULL) ? last_position_ : end_value->IntAtIndex(0, nullptr);
	
	if ((start < 0) || (end < 0) || (start > last_position_) || (end > last_position_) || (start > end))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_ancestralNucleotides): start and end must be within the chromosome's extent, and start must be <= end." << EidosTerminate();
	if (((std::size_t)start >= sequence->size()) || ((std::size_t)end >= sequence->size()))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_ancestralNucleotides): (internal error) start and end must be within the ancestral sequence's length." << EidosTerminate();
	
	int64_t length = end - start + 1;
	
	if (length > INT_MAX)
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_ancestralNucleotides): the returned vector would exceed the maximum vector length in Eidos." << EidosTerminate();
	
	EidosValue *format_value = p_arguments[2].get();
	std::string format = format_value->StringAtIndex(0, nullptr);
	
	if (format == "codon")
		return sequence->NucleotidesAsCodonVector(start, end, /* p_force_vector */ false);
	if (format == "string")
		return sequence->NucleotidesAsStringSingleton(start, end);
	if (format == "integer")
		return sequence->NucleotidesAsIntegerVector(start, end);
	if (format == "char")
		return sequence->NucleotidesAsStringVector(start, end);
	
	EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_ancestralNucleotides): parameter format must be either 'string', 'char', 'integer', or 'codon'." << EidosTerminate();
}

//	*********************	– (integer)drawBreakpoints([No<Individual>$ parent = NULL], [Ni$ n = NULL])
//
EidosValue_SP Chromosome::ExecuteMethod_drawBreakpoints(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *parent_value = p_arguments[0].get();
	EidosValue *n_value = p_arguments[1].get();
	
	if (using_DSB_model_ && (simple_conversion_fraction_ != 1.0))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_drawBreakpoints): drawBreakpoints() does not allow complex gene conversion tracts to be in use, since there is no provision for handling heteroduplex regions." << EidosTerminate();
	
	// In a sexual model with sex-specific recombination maps, we need to know the parent we're
	// generating breakpoints for; in other situations it is optional, but recombination()
	// breakpoints will not be called if parent is NULL.
	Individual *parent = nullptr;
	
	if (parent_value->Type() != EidosValueType::kValueNULL)
		parent = (Individual *)parent_value->ObjectElementAtIndex(0, nullptr);
	
	if (!parent && !single_recombination_map_)
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_drawBreakpoints): drawBreakpoints() requires a non-NULL parent parameter in sexual models with sex-specific recombination maps." << EidosTerminate();
	
	IndividualSex parent_sex = IndividualSex::kUnspecified;
	std::vector<SLiMEidosBlock*> recombination_callbacks;
	Subpopulation *parent_subpop = nullptr;
	
	if (parent)
	{
		parent_sex = parent->sex_;
		parent_subpop = &parent->subpopulation_;
		recombination_callbacks = sim_->ScriptBlocksMatching(sim_->Generation(), SLiMEidosBlockType::SLiMEidosRecombinationCallback, -1, -1, parent_subpop->subpopulation_id_);
	}
	
	// Much of the breakpoint-drawing code here is taken from Population::DoCrossoverMutation().
	// We don't want to split it out into a separate function because (a) we don't want that
	// overhead for DoCrossoverMutation(), which is a hotspot, and (b) we do things slightly
	// differently here (not generating a past-the-end breakpoint, for example).
	int num_breakpoints;
	
	if (n_value->Type() == EidosValueType::kValueNULL)
		num_breakpoints = DrawBreakpointCount(parent_sex);
	else
	{
		int64_t n = n_value->IntAtIndex(0, nullptr);
		
		if ((n < 0) || (n > 1000000L))
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_drawBreakpoints): drawBreakpoints() requires 0 <= n <= 1000000." << EidosTerminate();
		
		num_breakpoints = (int)n;
	}
	
	std::vector<slim_position_t> all_breakpoints;
	std::vector<slim_position_t> heteroduplex;				// never actually used since simple_conversion_fraction_ must be 1.0
	
	// draw the breakpoints based on the recombination rate map, and sort and unique the result
	if (num_breakpoints)
	{
		if (using_DSB_model_)
			DrawDSBBreakpoints(parent_sex, num_breakpoints, all_breakpoints, heteroduplex);
		else
			DrawCrossoverBreakpoints(parent_sex, num_breakpoints, all_breakpoints);
		
		if (recombination_callbacks.size())
		{
			// a non-zero number of breakpoints, with recombination callbacks
			sim_->ThePopulation().ApplyRecombinationCallbacks(parent->index_, parent->genome1_, parent->genome2_, parent_subpop, all_breakpoints, recombination_callbacks);
			
			if (all_breakpoints.size() > 1)
			{
				std::sort(all_breakpoints.begin(), all_breakpoints.end());
				all_breakpoints.erase(unique(all_breakpoints.begin(), all_breakpoints.end()), all_breakpoints.end());
			}
		}
	}
	else if (recombination_callbacks.size())
	{
		// zero breakpoints from the SLiM core, but we have recombination() callbacks
		sim_->ThePopulation().ApplyRecombinationCallbacks(parent->index_, parent->genome1_, parent->genome2_, parent_subpop, all_breakpoints, recombination_callbacks);
		
		if (all_breakpoints.size() > 1)
		{
			std::sort(all_breakpoints.begin(), all_breakpoints.end());
			all_breakpoints.erase(unique(all_breakpoints.begin(), all_breakpoints.end()), all_breakpoints.end());
		}
	}
	else
	{
		// no breakpoints, no gene conversion, no recombination() callbacks
	}
	
	if (all_breakpoints.size() == 0)
		return gStaticEidosValue_Integer_ZeroVec;
	else
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(all_breakpoints));
}

//	*********************	(integer$)setAncestralNucleotides(is sequence)
//
EidosValue_SP Chromosome::ExecuteMethod_setAncestralNucleotides(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *sequence_value = p_arguments[0].get();
	
	if (!sim_->IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): initializeAncestralNucleotides() may be only be called in nucleotide-based models." << EidosTerminate();
	
	EidosValueType sequence_value_type = sequence_value->Type();
	int sequence_value_count = sequence_value->Count();
	
	if (sequence_value_count == 0)
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): initializeAncestralNucleotides() requires a sequence of length >= 1." << EidosTerminate();
	
	if (ancestral_seq_buffer_)
	{
		delete ancestral_seq_buffer_;
		ancestral_seq_buffer_ = nullptr;
	}
	
	if (sequence_value_type == EidosValueType::kValueInt)
	{
		// A vector of integers has been provided, where ACGT == 0123
		if (sequence_value_count == 1)
		{
			// singleton case
			int64_t int_value = sequence_value->IntAtIndex(0, nullptr);
			
			ancestral_seq_buffer_ = new NucleotideArray(1);
			ancestral_seq_buffer_->SetNucleotideAtIndex((std::size_t)0, (uint64_t)int_value);
		}
		else
		{
			// non-singleton, direct access
			const EidosValue_Int_vector *int_vec = sequence_value->IntVector();
			const int64_t *int_data = int_vec->data();
			
			try {
				ancestral_seq_buffer_ = new NucleotideArray(sequence_value_count, int_data);
			} catch (...) {
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): integer nucleotide values must be 0 (A), 1 (C), 2 (G), or 3 (T)." << EidosTerminate();
			}
		}
	}
	else if (sequence_value_type == EidosValueType::kValueString)
	{
		if (sequence_value_count != 1)
		{
			// A vector of characters has been provided, which must all be "A" / "C" / "G" / "T"
			const std::vector<std::string> *string_vec = sequence_value->StringVector();
			
			try {
				ancestral_seq_buffer_ = new NucleotideArray(sequence_value_count, *string_vec);
			} catch (...) {
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): string nucleotide values must be 'A', 'C', 'G', or 'T'." << EidosTerminate();
			}
		}
		else	// sequence_value_count == 1
		{
			const std::string &sequence_string = sequence_value->IsSingleton() ? ((EidosValue_String_singleton *)sequence_value)->StringValue() : (*sequence_value->StringVector())[0];
			bool contains_only_nuc = true;
			
			try {
				ancestral_seq_buffer_ = new NucleotideArray(sequence_string.length(), sequence_string.c_str());
			} catch (...) {
				contains_only_nuc = false;
			}
			
			if (!contains_only_nuc)
			{
				// A singleton string has been provided that contains characters other than ACGT; we will interpret it as a filesystem path for a FASTA file
				std::string file_path = Eidos_ResolvedPath(sequence_string);
				std::ifstream file_stream(file_path.c_str());
				
				if (!file_stream.is_open())
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): the file at path " << sequence_string << " could not be opened or does not exist." << EidosTerminate();
				
				bool started_sequence = false;
				std::string line, fasta_sequence;
				
				while (getline(file_stream, line))
				{
					// skippable lines are blank or start with a '>' or ';'
					// we skip over them if they're at the start of the file; once we start a sequence, they terminate the sequence
					bool skippable = ((line.length() == 0) || (line[0] == '>') || (line[0] == ';'));
					
					if (!started_sequence && skippable)
						continue;
					if (skippable)
						break;
					
					// otherwise, append the nucleotides from this line, removing a \r if one is present at the end of the line
					if (line.back() == '\r')
						line.pop_back();
					
					fasta_sequence.append(line);
					started_sequence = true;
				}
				
				if (file_stream.bad())
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): a filesystem error occurred while reading the file at path " << sequence_string << "." << EidosTerminate();
				
				if (fasta_sequence.length() == 0)
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): no FASTA sequence found in " << sequence_string << "." << EidosTerminate();
				
				try {
					ancestral_seq_buffer_ = new NucleotideArray(fasta_sequence.length(), fasta_sequence.c_str());
				} catch (...) {
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): FASTA sequence data must contain only the nucleotides ACGT." << EidosTerminate();
				}
			}
		}
	}
	
	// check that the length of the new sequence matches the chromosome length
	if (ancestral_seq_buffer_->size() != (std::size_t)(last_position_ + 1))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setAncestralNucleotides): The chromosome length (" << last_position_ + 1 << " base" << (last_position_ + 1 != 1 ? "s" : "") << ") does not match the ancestral sequence length (" << ancestral_seq_buffer_->size() << " base" << (ancestral_seq_buffer_->size() != 1 ? "s" : "") << ")." << EidosTerminate();
	
	// debugging
	//std::cout << "ancestral sequence set: " << *ancestral_seq_buffer_ << std::endl;
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(ancestral_seq_buffer_->size()));
}

//	*********************	– (void)setGeneConversion(numeric$ nonCrossoverFraction, numeric$ meanLength, numeric$ simpleConversionFraction, [numeric$ bias = 0])
//
EidosValue_SP Chromosome::ExecuteMethod_setGeneConversion(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *nonCrossoverFraction_value = p_arguments[0].get();
	EidosValue *meanLength_value = p_arguments[1].get();
	EidosValue *simpleConversionFraction_value = p_arguments[2].get();
	EidosValue *bias_value = p_arguments[3].get();
	
	double non_crossover_fraction = nonCrossoverFraction_value->FloatAtIndex(0, nullptr);
	double gene_conversion_avg_length = meanLength_value->FloatAtIndex(0, nullptr);
	double simple_conversion_fraction = simpleConversionFraction_value->FloatAtIndex(0, nullptr);
	double bias = bias_value->FloatAtIndex(0, nullptr);
	
	if ((non_crossover_fraction < 0.0) || (non_crossover_fraction > 1.0) || std::isnan(non_crossover_fraction))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setGeneConversion): setGeneConversion() nonCrossoverFraction must be between 0.0 and 1.0 inclusive (" << EidosStringForFloat(non_crossover_fraction) << " supplied)." << EidosTerminate();
	if ((gene_conversion_avg_length < 0.0) || std::isnan(gene_conversion_avg_length))		// intentionally no upper bound
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setGeneConversion): setGeneConversion() meanLength must be >= 0.0 (" << EidosStringForFloat(gene_conversion_avg_length) << " supplied)." << EidosTerminate();
	if ((simple_conversion_fraction < 0.0) || (simple_conversion_fraction > 1.0) || std::isnan(simple_conversion_fraction))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setGeneConversion): setGeneConversion() simpleConversionFraction must be between 0.0 and 1.0 inclusive (" << EidosStringForFloat(simple_conversion_fraction) << " supplied)." << EidosTerminate();
	if ((bias < -1.0) || (bias > 1.0) || std::isnan(bias))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setGeneConversion): setGeneConversion() bias must be between -1.0 and 1.0 inclusive (" << EidosStringForFloat(bias) << " supplied)." << EidosTerminate();
	if ((bias != 0.0) && !sim_->IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setGeneConversion): setGeneConversion() bias must be 0.0 in non-nucleotide-based models." << EidosTerminate();
	
	using_DSB_model_ = true;
	non_crossover_fraction_ = non_crossover_fraction;
	gene_conversion_avg_length_ = gene_conversion_avg_length;
	gene_conversion_inv_half_length_ = 1.0 / (gene_conversion_avg_length / 2.0);
	simple_conversion_fraction_ = simple_conversion_fraction;
	mismatch_repair_bias_ = bias;
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)setHotspotMap(numeric multipliers, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP Chromosome::ExecuteMethod_setHotspotMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (!sim_->IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() may only be called in nucleotide-based models (use setMutationRate() to vary the mutation rate along the chromosome)." << EidosTerminate();
	
	EidosValue *multipliers_value = p_arguments[0].get();
	EidosValue *ends_value = p_arguments[1].get();
	EidosValue *sex_value = p_arguments[2].get();
	
	int multipliers_count = multipliers_value->Count();
	
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
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() requested sex \"" << sex_string << "\" unsupported." << EidosTerminate();
	
	// Make sure specifying a map for that sex is legal, given our current state
	if (((requested_sex == IndividualSex::kUnspecified) && !single_mutation_map_) ||
		((requested_sex != IndividualSex::kUnspecified) && single_mutation_map_))
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() cannot change the chromosome between using a single map versus separate maps for the sexes; the original configuration must be preserved." << EidosTerminate();
	
	// Set up to replace the requested map
	std::vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? hotspot_end_positions_H_ : 
										  ((requested_sex == IndividualSex::kMale) ? hotspot_end_positions_M_ : hotspot_end_positions_F_));
	std::vector<double> &multipliers = ((requested_sex == IndividualSex::kUnspecified) ? hotspot_multipliers_H_ : 
							 ((requested_sex == IndividualSex::kMale) ? hotspot_multipliers_M_ : hotspot_multipliers_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		// ends is missing/NULL
		if (multipliers_count != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() requires multipliers to be a singleton if ends is not supplied." << EidosTerminate();
		
		double multiplier = multipliers_value->FloatAtIndex(0, nullptr);
		
		// check values
		if ((multiplier < 0.0) || !std::isfinite(multiplier))		// intentionally no upper bound
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() multiplier " << EidosStringForFloat(multiplier) << " out of range; multipliers must be >= 0." << EidosTerminate();
		
		// then adopt them
		multipliers.clear();
		positions.clear();
		
		multipliers.emplace_back(multiplier);
		positions.emplace_back(last_position_);
	}
	else
	{
		// ends is supplied
		int end_count = ends_value->Count();
		
		if ((end_count != multipliers_count) || (end_count == 0))
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() requires ends and multipliers to be of equal and nonzero size." << EidosTerminate();
		
		// check values
		for (int value_index = 0; value_index < end_count; ++value_index)
		{
			double multiplier = multipliers_value->FloatAtIndex(value_index, nullptr);
			slim_position_t mutation_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex(value_index, nullptr));
			
			if (value_index > 0)
				if (mutation_end_position <= ends_value->IntAtIndex(value_index - 1, nullptr))
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() requires ends to be in strictly ascending order." << EidosTerminate();
			
			if ((multiplier < 0.0) || !std::isfinite(multiplier))		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() multiplier " << multiplier << " out of range; multipliers must be >= 0." << EidosTerminate();
		}
		
		// The stake here is that the last position in the chromosome is not allowed to change after the chromosome is
		// constructed.  When we call InitializeDraws() below, we recalculate the last position – and we must come up
		// with the same answer that we got before, otherwise our last_position_ cache is invalid.
		int64_t new_last_position = ends_value->IntAtIndex(end_count - 1, nullptr);
		
		if (new_last_position != last_position_)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setHotspotMap): setHotspotMap() end " << new_last_position << " noncompliant; the last interval must end at the last position of the chromosome (" << last_position_ << ")." << EidosTerminate();
		
		// then adopt them
		multipliers.clear();
		positions.clear();
		
		for (int interval_index = 0; interval_index < end_count; ++interval_index)
		{
			double multiplier = multipliers_value->FloatAtIndex(interval_index, nullptr);
			slim_position_t mutation_end_position = SLiMCastToPositionTypeOrRaise(ends_value->IntAtIndex(interval_index, nullptr));
			
			multipliers.emplace_back(multiplier);
			positions.emplace_back(mutation_end_position);
		}
	}
	
	sim_->CreateNucleotideMutationRateMap();
	InitializeDraws();
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)setMutationRate(numeric rates, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP Chromosome::ExecuteMethod_setMutationRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (sim_->IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() may not be called in nucleotide-based models (use setHotspotMap() to vary the mutation rate along the chromosome)." << EidosTerminate();
	
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
	std::vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? mutation_end_positions_H_ : 
										  ((requested_sex == IndividualSex::kMale) ? mutation_end_positions_M_ : mutation_end_positions_F_));
	std::vector<double> &rates = ((requested_sex == IndividualSex::kUnspecified) ? mutation_rates_H_ : 
							 ((requested_sex == IndividualSex::kMale) ? mutation_rates_M_ : mutation_rates_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		// ends is missing/NULL
		if (rate_count != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() requires rates to be a singleton if ends is not supplied." << EidosTerminate();
		
		double mutation_rate = rates_value->FloatAtIndex(0, nullptr);
		
		// check values
		if ((mutation_rate < 0.0) || !std::isfinite(mutation_rate))		// intentionally no upper bound
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() rate " << EidosStringForFloat(mutation_rate) << " out of range; rates must be >= 0." << EidosTerminate();
		
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
			
			if ((mutation_rate < 0.0) || !std::isfinite(mutation_rate))		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() rate " << EidosStringForFloat(mutation_rate) << " out of range; rates must be >= 0." << EidosTerminate();
		}
		
		// The stake here is that the last position in the chromosome is not allowed to change after the chromosome is
		// constructed.  When we call InitializeDraws() below, we recalculate the last position – and we must come up
		// with the same answer that we got before, otherwise our last_position_ cache is invalid.
		int64_t new_last_position = ends_value->IntAtIndex(end_count - 1, nullptr);
		
		if (new_last_position != last_position_)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setMutationRate): setMutationRate() end " << new_last_position << " noncompliant; the last interval must end at the last position of the chromosome (" << last_position_ << ")." << EidosTerminate();
		
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
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)setRecombinationRate(numeric rates, [Ni ends = NULL], [string$ sex = "*"])
//
EidosValue_SP Chromosome::ExecuteMethod_setRecombinationRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
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
	std::vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? recombination_end_positions_H_ : 
										  ((requested_sex == IndividualSex::kMale) ? recombination_end_positions_M_ : recombination_end_positions_F_));
	std::vector<double> &rates = ((requested_sex == IndividualSex::kUnspecified) ? recombination_rates_H_ : 
							 ((requested_sex == IndividualSex::kMale) ? recombination_rates_M_ : recombination_rates_F_));
	
	if (ends_value->Type() == EidosValueType::kValueNULL)
	{
		// ends is missing/NULL
		if (rate_count != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() requires rates to be a singleton if ends is not supplied." << EidosTerminate();
		
		double recombination_rate = rates_value->FloatAtIndex(0, nullptr);
		
		// check values
		if ((recombination_rate < 0.0) || (recombination_rate > 0.5))
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() rate " << recombination_rate << " out of range; rates must be in [0.0, 0.5]." << EidosTerminate();
		
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
			
			if ((recombination_rate < 0.0) || (recombination_rate > 0.5))
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod_setRecombinationRate): setRecombinationRate() rate " << recombination_rate << " out of range; rates must be in [0.0, 0.5]." << EidosTerminate();
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
	
	return gStaticEidosValueVOID;
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
	
	virtual const std::string &ElementType(void) const override;
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};

EidosObjectClass *gSLiM_Chromosome_Class = new Chromosome_Class();


const std::string &Chromosome_Class::ElementType(void) const
{
	return gStr_Chromosome;
}

const std::vector<EidosPropertySignature_CSP> *Chromosome_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<EidosPropertySignature_CSP>(*EidosObjectClass::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomicElements,						true,	kEidosValueMaskObject, gSLiM_GenomicElement_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_lastPosition,							true,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hotspotEndPositions,					true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hotspotEndPositionsM,					true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hotspotEndPositionsF,					true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hotspotMultipliers,						true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hotspotMultipliersM,					true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hotspotMultipliersF,					true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationEndPositions,					true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationEndPositionsM,					true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationEndPositionsF,					true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationRates,							true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationRatesM,							true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationRatesF,							true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallMutationRate,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallMutationRateM,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallMutationRateF,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallRecombinationRate,				true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallRecombinationRateM,				true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_overallRecombinationRateF,				true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationEndPositions,				true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationEndPositionsM,				true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationEndPositionsF,				true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationRates,						true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationRatesM,					true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationRatesF,					true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionEnabled,					true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionGCBias,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionNonCrossoverFraction,		true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionMeanLength,				true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionSimpleConversionFraction,	true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,									false,	kEidosValueMaskInt | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_colorSubstitution,						false,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Chromosome_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<EidosMethodSignature_CSP>(*EidosObjectClass::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_ancestralNucleotides, kEidosValueMaskInt | kEidosValueMaskString))->AddInt_OSN(gEidosStr_start, gStaticEidosValueNULL)->AddInt_OSN(gEidosStr_end, gStaticEidosValueNULL)->AddString_OS("format", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("string"))));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_drawBreakpoints, kEidosValueMaskInt))->AddObject_OSN("parent", gSLiM_Individual_Class, gStaticEidosValueNULL)->AddInt_OSN("n", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setAncestralNucleotides, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntString("sequence"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setGeneConversion, kEidosValueMaskVOID))->AddNumeric_S("nonCrossoverFraction")->AddNumeric_S("meanLength")->AddNumeric_S("simpleConversionFraction")->AddNumeric_OS("bias", gStaticEidosValue_Integer0));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setHotspotMap, kEidosValueMaskVOID))->AddNumeric("multipliers")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMutationRate, kEidosValueMaskVOID))->AddNumeric("rates")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setRecombinationRate, kEidosValueMaskVOID))->AddNumeric("rates")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}





































































