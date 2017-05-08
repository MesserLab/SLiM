//
//  chromosome.cpp
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


#include "chromosome.h"
#include "eidos_rng.h"
#include "slim_global.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#include <iostream>
#include <algorithm>
#include <string>
#include <cmath>


Chromosome::Chromosome(void) : lookup_mutation_(nullptr), single_recombination_map_(true), lookup_recombination_H_(nullptr), lookup_recombination_M_(nullptr), lookup_recombination_F_(nullptr), exp_neg_element_mutation_rate_(0.0),
	exp_neg_overall_recombination_rate_H_(0.0), probability_both_0_H_(0.0), probability_both_0_OR_mut_0_break_non0_H_(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_H_(0.0), overall_recombination_rate_H_(0.0),
	exp_neg_overall_recombination_rate_M_(0.0), probability_both_0_M_(0.0), probability_both_0_OR_mut_0_break_non0_M_(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_(0.0), overall_recombination_rate_M_(0.0),
	exp_neg_overall_recombination_rate_F_(0.0), probability_both_0_F_(0.0), probability_both_0_OR_mut_0_break_non0_F_(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_(0.0), overall_recombination_rate_F_(0.0),
	last_position_(0), overall_mutation_rate_(0.0), element_mutation_rate_(0.0), gene_conversion_fraction_(0.0), gene_conversion_avg_length_(0.0)
{
	// Set up the default color for fixed mutations in SLiMgui
	color_sub_ = "#3333FF";
	if (!color_sub_.empty())
		EidosGetColorComponents(color_sub_, &color_sub_red_, &color_sub_green_, &color_sub_blue_);
}

Chromosome::~Chromosome(void)
{
	//EIDOS_ERRSTREAM << "Chromosome::~Chromosome" << std::endl;
	
	if (lookup_mutation_)
		gsl_ran_discrete_free(lookup_mutation_);
	
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
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): empty chromosome." << eidos_terminate();
	if (!(overall_mutation_rate_ >= 0))
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): invalid mutation rate " << overall_mutation_rate_ << "." << eidos_terminate();
	
	// determine which case we are working with: separate recombination maps for the sexes, or one map
	auto rates_H_size = recombination_rates_H_.size();
	auto rates_M_size = recombination_rates_M_.size();
	auto rates_F_size = recombination_rates_F_.size();
	
	if ((rates_H_size > 0) && (rates_M_size == 0) && (rates_F_size == 0))
		single_recombination_map_ = true;
	else if ((rates_H_size == 0) && (rates_M_size > 0) && (rates_F_size > 0))
		single_recombination_map_ = false;
	else
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): (internal error) unclear whether we have separate recombination maps or not, or recombination rate not specified." << eidos_terminate();
	
	// calculate the overall mutation rate and the lookup table for mutation locations
	cached_value_lastpos_.reset();
	last_position_ = 0;
	
	if (lookup_mutation_)
	{
		gsl_ran_discrete_free(lookup_mutation_);
		lookup_mutation_ = nullptr;
	}
	
	std::vector<double> A(size());
	int l = 0;
	
	for (unsigned int i = 0; i < size(); i++) 
	{ 
		if ((*this)[i].end_position_ > last_position_)
			last_position_ = (*this)[i].end_position_;
		
		slim_position_t l_i = (*this)[i].end_position_ - (*this)[i].start_position_ + 1;
		
		A[i] = static_cast<double>(l_i);
		l += l_i;
	}
	
	//if (size() > 1)		// this did not produce a performance win, surprisingly
		lookup_mutation_ = gsl_ran_discrete_preproc(size(), A.data());
	
	element_mutation_rate_ = overall_mutation_rate_ * static_cast<double>(l);
	
	// Now remake our recombination map info, which we delegate to _InitializeOneRecombinationMap()
	if (single_recombination_map_)
	{
		_InitializeOneRecombinationMap(lookup_recombination_H_, recombination_end_positions_H_, recombination_rates_H_, overall_recombination_rate_H_, exp_neg_overall_recombination_rate_H_, probability_both_0_H_, probability_both_0_OR_mut_0_break_non0_H_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_H_);
	}
	else
	{
		_InitializeOneRecombinationMap(lookup_recombination_M_, recombination_end_positions_M_, recombination_rates_M_, overall_recombination_rate_M_, exp_neg_overall_recombination_rate_M_, probability_both_0_M_, probability_both_0_OR_mut_0_break_non0_M_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_M_);
		_InitializeOneRecombinationMap(lookup_recombination_F_, recombination_end_positions_F_, recombination_rates_F_, overall_recombination_rate_F_, exp_neg_overall_recombination_rate_F_, probability_both_0_F_, probability_both_0_OR_mut_0_break_non0_F_, probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_F_);
	}
}

void Chromosome::ChooseMutationRunLayout(int p_preferred_count)
{
	// We now have a final last position, which should not change henceforth.  We can therefore calculate how many mutation runs
	// we will use, and how long they will be.  Note that this decision may depend also upon the overall mutation rate and the
	// overall recombination rate in future.
	
	// Choose a mutation-run length for which the expected number of events per generation is perhaps 0.5?
	// So use 0.15 = run_length * rate.  FIXME this could doubtless be improved...
	double chromosome_length = last_position_ + 1;
	double mu = overall_mutation_rate_;
	double r = (single_recombination_map_ ? overall_recombination_rate_H_ : (overall_recombination_rate_M_ + overall_recombination_rate_F_) / 2) / last_position_;
	double target_length = 0.15 / (mu + r);
	
	if (target_length < 50000)				// runs that are too short cause a lot of overhead
		target_length = 50000;
	
	double target_count = chromosome_length / target_length;
	
	mutrun_count_ = std::max((int)round(target_count), 1);
	
	if (mutrun_count_ > 100)				// too many runs is rarely beneficial, and can be quite harmful
		mutrun_count_ = 100;
	
	if (SLiM_verbose_output)
	{
		// Write out some information about how the mutrun count was arrived at
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "// Default mutation run count: " << std::endl;
		SLIM_OUTSTREAM << "//    chromosome_length = " << chromosome_length << std::endl;
		SLIM_OUTSTREAM << "//    mu = " << mu << std::endl;
		SLIM_OUTSTREAM << "//    r = " << r << std::endl;
		SLIM_OUTSTREAM << "//    target_length = " << target_length << std::endl;
		SLIM_OUTSTREAM << "//    target_count = " << target_count << std::endl;
		SLIM_OUTSTREAM << "//    mutrun_count_ = " << mutrun_count_ << std::endl;
	}
	
	// If the user specified a preferred mutation run length, use that
	if (p_preferred_count != 0)
	{
		if (p_preferred_count < 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::ChooseMutationRunLayout): there must be at least one mutation run per genome." << eidos_terminate();
		
		mutrun_count_ = p_preferred_count;
		
		if (SLiM_verbose_output)
			SLIM_OUTSTREAM << std::endl << "// Override mutation run count: " << mutrun_count_ << std::endl;
	}
	
	// Calculate the length of mutation runs needed given the mutation run count and chromosome length
	mutrun_length_ = (int)ceil((last_position_ + 1) / (double)mutrun_count_);
	
	// Consistency check
	if ((mutrun_length_ < 1) || (mutrun_count_ * mutrun_length_ <= last_position_))
		EIDOS_TERMINATION << "ERROR (Chromosome::ChooseMutationRunLayout): (internal error) math error in mutation run calculations." << eidos_terminate();
}

// initialize one recombination map, used internally by InitializeDraws() to avoid code duplication
void Chromosome::_InitializeOneRecombinationMap(gsl_ran_discrete_t *&p_lookup, vector<slim_position_t> &p_end_positions, vector<double> &p_rates, double &p_overall_rate, double &p_exp_neg_overall_rate, double &p_both_0, double &p_both_0_OR_mut_0_break_non0_, double &p_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_)
{
	// patch the recombination interval end vector if it is empty; see setRecombinationRate() and initializeRecombinationRate()
	// basically, the length of the chromosome might not have been known yet when the user set the rate
	if (p_end_positions.size() == 0)
	{
		// patching can only be done when a single uniform rate is specified
		if (p_rates.size() != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): recombination endpoints not specified." << eidos_terminate();
		
		p_end_positions.emplace_back(last_position_);
	}
	
	// calculate the overall recombination rate and the lookup table for breakpoints
	std::vector<double> B(p_rates.size());
	
	p_overall_rate = 0.0;
	
	B[0] = p_rates[0] * static_cast<double>(p_end_positions[0]);	// No +1 here, because the position to the left of the first base is not a valid recombination position.
	// So a 1-base model (position 0 to 0) has a recombination end of 0, and thus an overall rate of 0.
	// This means that gsl_ran_discrete_preproc() is given an interval with rate 0, but it does not
	// seem to mind that.  BCH 4 April 2016
	p_overall_rate += B[0];
	
	for (unsigned int i = 1; i < p_rates.size(); i++) 
	{ 
		B[i] = p_rates[i] * static_cast<double>(p_end_positions[i] - p_end_positions[i - 1]);
		p_overall_rate += B[i];
		
		if (p_end_positions[i] > last_position_)
			last_position_ = p_end_positions[i];
	}
	
	if (p_end_positions[p_rates.size() - 1] < last_position_)
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): recombination endpoints do not cover all genomic elements." << eidos_terminate();
	
	// EIDOS_ERRSTREAM << "overall recombination rate: " << p_overall_rate << std::endl;
	
	if (p_lookup)
	{
		gsl_ran_discrete_free(p_lookup);
		p_lookup = nullptr;
	}
	
	//if (p_rates.size() > 1)		// this did not produce a performance win, surprisingly
		p_lookup = gsl_ran_discrete_preproc(p_rates.size(), B.data());
	
	// precalculate probabilities for Poisson draws of mutation count and breakpoint count
	double prob_mutation_0 = eidos_fast_ran_poisson_PRECALCULATE(element_mutation_rate_);		// exp(-mu); can be 0 due to underflow
	double prob_breakpoint_0 = eidos_fast_ran_poisson_PRECALCULATE(p_overall_rate);				// exp(-mu); can be 0 due to underflow
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
	
	exp_neg_element_mutation_rate_ = prob_mutation_0;
	p_exp_neg_overall_rate = prob_breakpoint_0;
	
	p_both_0 = prob_both_0;
	p_both_0_OR_mut_0_break_non0_ = prob_both_0 + prob_mutation_0_breakpoint_not_0;
	p_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0_ = prob_both_0 + (prob_mutation_0_breakpoint_not_0 + prob_mutation_not_0_breakpoint_0);
}

// prints an error message and exits
void Chromosome::RecombinationMapConfigError(void) const
{
	EIDOS_TERMINATION << "ERROR (Chromosome::RecombinationMapConfigError): (internal error) an error occurred in the configuration of recombination maps." << eidos_terminate();
}


// draw a new mutation, based on the genomic element types present and their mutational proclivities
MutationIndex Chromosome::DrawNewMutation(slim_objectid_t p_subpop_index, slim_generation_t p_generation) const
{
	int genomic_element_index = static_cast<int>(gsl_ran_discrete(gEidos_rng, lookup_mutation_));
	const GenomicElement &source_element = (*this)[genomic_element_index];
	const GenomicElementType &genomic_element_type = *source_element.genomic_element_type_ptr_;
	MutationType *mutation_type_ptr = genomic_element_type.DrawMutationType();
	
	slim_position_t position = source_element.start_position_ + static_cast<slim_position_t>(gsl_rng_uniform_int(gEidos_rng, source_element.end_position_ - source_element.start_position_ + 1));  
	
	double selection_coeff = mutation_type_ptr->DrawSelectionCoefficient();
	
	// NOTE THAT THE STACKING POLICY IS NOT ENFORCED HERE, SINCE WE DO NOT KNOW WHAT GENOME WE WILL BE INSERTED INTO!  THIS IS THE CALLER'S RESPONSIBILITY!
	MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
	
	new (gSLiM_Mutation_Block + new_mut_index) Mutation(new_mut_index,mutation_type_ptr, position, selection_coeff, p_subpop_index, p_generation);
	
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
		// given by recombination_end_positions_[recombination_interval - 1], at minimum.  Since gsl_rng_uniform_int() returns
		// a zero-based random number, that means we need a +1 here as well.
		//
		// The key fact here is that a recombination breakpoint position of 1 means "break to the left of the base at position 1" –
		// the breakpoint falls between bases, to the left of the bse at the specified number.  This is a consequence of the logic
		// is the crossover-mutation code, which copies mutations as long as their position is *less than* the position of the next
		// breakpoint.  When their position is *equal*, the breakpoint gets serviced by switching strands.  That logic causes the
		// breakpoints to fall to the left of their designated base.
		//
		// Note that gsl_rng_uniform_int() crashes (well, aborts fatally) if passed 0 for n.  We need to guarantee that that doesn't
		// happen, and we don't want to waste time checking for that condition here.  For a 1-base model, we are guaranteed that
		// the overall recombination rate will be zero, by the logic in InitializeDraws(), and so we should not be called in the
		// first place.  For longer chromosomes that start with a 1-base recombination interval, the rate calculated by
		// InitializeDraws() for the first interval should be 0, so gsl_ran_discrete() should never return the first interval to
		// us here.  For all other recombination intervals, the math of pos[x]-pos[x-1] should always result in a value >0,
		// since we guarantee that recombination end positions are in strictly ascending order.  So we should never crash.  :->
		
		if (recombination_interval == 0)
			breakpoint = static_cast<slim_position_t>(gsl_rng_uniform_int(gEidos_rng, (*end_positions)[recombination_interval]) + 1);
		else
			breakpoint = (*end_positions)[recombination_interval - 1] + 1 + static_cast<slim_position_t>(gsl_rng_uniform_int(gEidos_rng, (*end_positions)[recombination_interval] - (*end_positions)[recombination_interval - 1]));
		
		p_crossovers.emplace_back(breakpoint);
		
		// recombination can result in gene conversion, with probability gene_conversion_fraction_
		if (gene_conversion_fraction_ > 0.0)
		{
			if ((gene_conversion_fraction_ < 1.0) && (gsl_rng_uniform(gEidos_rng) < gene_conversion_fraction_))
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
			breakpoint = static_cast<slim_position_t>(gsl_rng_uniform_int(gEidos_rng, (*end_positions)[recombination_interval]) + 1);
		else
			breakpoint = (*end_positions)[recombination_interval - 1] + 1 + static_cast<slim_position_t>(gsl_rng_uniform_int(gEidos_rng, (*end_positions)[recombination_interval] - (*end_positions)[recombination_interval - 1]));
		
		// recombination can result in gene conversion, with probability gene_conversion_fraction_
		if (gene_conversion_fraction_ > 0.0)
		{
			if ((gene_conversion_fraction_ < 1.0) && (gsl_rng_uniform(gEidos_rng) < gene_conversion_fraction_))
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
				vec->PushObjectElement(&(*genomic_element_iter));		// operator * can be overloaded by the iterator
			
			return result_SP;
		}
		case gID_lastPosition:
		{
			if (!cached_value_lastpos_)
				cached_value_lastpos_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(last_position_));
			return cached_value_lastpos_;
		}
			
		case gID_overallRecombinationRate:
			if (!single_recombination_map_) return gStaticEidosValueNULL;
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_recombination_rate_H_));
		case gID_overallRecombinationRateM:
			if (single_recombination_map_) return gStaticEidosValueNULL;
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_recombination_rate_M_));
		case gID_overallRecombinationRateF:
			if (single_recombination_map_) return gStaticEidosValueNULL;
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_recombination_rate_F_));
		
		case gID_recombinationEndPositions:
			if (!single_recombination_map_) return gStaticEidosValueNULL;
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(recombination_end_positions_H_));
		case gID_recombinationEndPositionsM:
			if (single_recombination_map_) return gStaticEidosValueNULL;
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(recombination_end_positions_M_));
		case gID_recombinationEndPositionsF:
			if (single_recombination_map_) return gStaticEidosValueNULL;
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector(recombination_end_positions_F_));
		
		case gID_recombinationRates:
			if (!single_recombination_map_) return gStaticEidosValueNULL;
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(recombination_rates_H_));
		case gID_recombinationRatesM:
			if (single_recombination_map_) return gStaticEidosValueNULL;
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(recombination_rates_M_));
		case gID_recombinationRatesF:
			if (single_recombination_map_) return gStaticEidosValueNULL;
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector(recombination_rates_F_));
			
			// variables
		case gID_colorSubstitution:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(color_sub_));
		case gID_geneConversionFraction:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gene_conversion_fraction_));
		case gID_geneConversionMeanLength:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(gene_conversion_avg_length_));
		case gID_mutationRate:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(overall_mutation_rate_));
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
				EidosGetColorComponents(color_sub_, &color_sub_red_, &color_sub_green_, &color_sub_blue_);
			return;
		}
		case gID_geneConversionFraction:
		{
			double value = p_value.FloatAtIndex(0, nullptr);
			
			if ((value < 0.0) || (value > 1.0))
				EIDOS_TERMINATION << "ERROR (Chromosome::SetProperty): new value " << value << " for property " << StringForEidosGlobalStringID(p_property_id) << " is out of range." << eidos_terminate();
			
			gene_conversion_fraction_ = value;
			return;
		}
		case gID_geneConversionMeanLength:
		{
			double value = p_value.FloatAtIndex(0, nullptr);
			
			if (value <= 0.0)		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (Chromosome::SetProperty): new value " << value << " for property " << StringForEidosGlobalStringID(p_property_id) << " is out of range." << eidos_terminate();
			
			gene_conversion_avg_length_ = value;
			return;
		}
		case gID_mutationRate:
		{
			double value = p_value.FloatAtIndex(0, nullptr);
			
			if (value < 0.0)	// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (Chromosome::SetProperty): new value " << value << " for property " << StringForEidosGlobalStringID(p_property_id) << " is out of range." << eidos_terminate();
			
			overall_mutation_rate_ = value;
			InitializeDraws();
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
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0].get() : nullptr);
	EidosValue *arg1_value = ((p_argument_count >= 2) ? p_arguments[1].get() : nullptr);
	EidosValue *arg2_value = ((p_argument_count >= 3) ? p_arguments[2].get() : nullptr);
	
	//
	//	*********************	– (void)setRecombinationRate(numeric rates, [Ni ends = NULL], [string$ sex = "*"])
	//
#pragma mark -setRecombinationRate()
	
	if (p_method_id == gID_setRecombinationRate)
	{
		int rate_count = arg0_value->Count();
		
		// Figure out what sex we are being given a map for
		IndividualSex requested_sex = IndividualSex::kUnspecified;
		
		std::string sex_string = arg2_value->StringAtIndex(0, nullptr);
		
		if (sex_string.compare("M") == 0)
			requested_sex = IndividualSex::kMale;
		else if (sex_string.compare("F") == 0)
			requested_sex = IndividualSex::kFemale;
		else if (sex_string.compare("*") == 0)
			requested_sex = IndividualSex::kUnspecified;
		else
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteInstanceMethod): setRecombinationRate() requested sex \"" << sex_string << "\" unsupported." << eidos_terminate();
		
		// Make sure specifying a map for that sex is legal, given our current state
		if (((requested_sex == IndividualSex::kUnspecified) && !single_recombination_map_) ||
			((requested_sex != IndividualSex::kUnspecified) && single_recombination_map_))
			EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteInstanceMethod): setRecombinationRate() cannot change the chromosome between using a single map versus separate maps for the sexes; the original configuration must be preserved." << eidos_terminate();
		
		// Set up to replace the requested map
		vector<slim_position_t> &positions = ((requested_sex == IndividualSex::kUnspecified) ? recombination_end_positions_H_ : 
											  ((requested_sex == IndividualSex::kMale) ? recombination_end_positions_M_ : recombination_end_positions_F_));
		vector<double> &rates = ((requested_sex == IndividualSex::kUnspecified) ? recombination_rates_H_ : 
								 ((requested_sex == IndividualSex::kMale) ? recombination_rates_M_ : recombination_rates_F_));
		
		if (arg1_value->Type() == EidosValueType::kValueNULL)
		{
			// ends is missing/NULL
			if (rate_count != 1)
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteInstanceMethod): setRecombinationRate() requires rates to be a singleton if ends is not supplied." << eidos_terminate();
			
			double recombination_rate = arg0_value->FloatAtIndex(0, nullptr);
			
			// check values
			if (recombination_rate < 0.0)		// intentionally no upper bound
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteInstanceMethod): setRecombinationRate() rate " << recombination_rate << " out of range; rates must be >= 0." << eidos_terminate();
			
			// then adopt them
			rates.clear();
			positions.clear();
			
			rates.emplace_back(recombination_rate);
			//positions.emplace_back(?);	// deferred; patched in Chromosome::InitializeDraws().
		}
		else
		{
			// ends is supplied
			int end_count = arg1_value->Count();
			
			if ((end_count != rate_count) || (end_count == 0))
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteInstanceMethod): setRecombinationRate() requires ends and rates to be of equal and nonzero size." << eidos_terminate();
			
			// check values
			for (int value_index = 0; value_index < end_count; ++value_index)
			{
				double recombination_rate = arg0_value->FloatAtIndex(value_index, nullptr);
				slim_position_t recombination_end_position = SLiMCastToPositionTypeOrRaise(arg1_value->IntAtIndex(value_index, nullptr));
				
				if (value_index > 0)
					if (recombination_end_position <= arg1_value->IntAtIndex(value_index - 1, nullptr))
						EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteInstanceMethod): setRecombinationRate() requires ends to be in strictly ascending order." << eidos_terminate();
				
				if (recombination_rate < 0.0)		// intentionally no upper bound
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteInstanceMethod): setRecombinationRate() rate " << recombination_rate << " out of range; rates must be >= 0." << eidos_terminate();
			}
			
			// The stake here is that the last position in the chromosome is not allowed to change after the chromosome is
			// constructed.  When we call InitializeDraws() below, we recalculate the last position – and we must come up
			// with the same answer that we got before, otherwise our last_position_ cache is invalid.
			int64_t new_last_position = arg1_value->IntAtIndex(end_count - 1, nullptr);
			
			if (new_last_position != last_position_)
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteInstanceMethod): setRecombinationRate() rate " << new_last_position << " noncompliant; the last interval must end at the last position of the chromosome (" << last_position_ << ")." << eidos_terminate();
			
			// then adopt them
			rates.clear();
			positions.clear();
			
			for (int interval_index = 0; interval_index < end_count; ++interval_index)
			{
				double recombination_rate = arg0_value->FloatAtIndex(interval_index, nullptr);
				slim_position_t recombination_end_position = SLiMCastToPositionTypeOrRaise(arg1_value->IntAtIndex(interval_index, nullptr));
				
				rates.emplace_back(recombination_rate);
				positions.emplace_back(recombination_end_position);
			}
		}
		
		InitializeDraws();
		
		return gStaticEidosValueNULLInvisible;
	}
	
	
	// all others, including gID_none
	else
		return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}


//
//	Chromosome_Class
//
#pragma mark -
#pragma mark Chromosome_Class

class Chromosome_Class : public EidosObjectClass
{
public:
	Chromosome_Class(const Chromosome_Class &p_original) = delete;	// no copy-construct
	Chromosome_Class& operator=(const Chromosome_Class&) = delete;	// no copying
	
	Chromosome_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_Chromosome_Class = new Chromosome_Class();


Chromosome_Class::Chromosome_Class(void)
{
}

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
		properties->emplace_back(SignatureForPropertyOrRaise(gID_genomicElements));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_lastPosition));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_overallRecombinationRate));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_overallRecombinationRateM));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_overallRecombinationRateF));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_recombinationEndPositions));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_recombinationEndPositionsM));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_recombinationEndPositionsF));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_recombinationRates));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_recombinationRatesM));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_recombinationRatesF));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_geneConversionFraction));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_geneConversionMeanLength));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_mutationRate));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tag));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_colorSubstitution));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Chromosome_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *genomicElementsSig = nullptr;
	static EidosPropertySignature *lastPositionSig = nullptr;
	static EidosPropertySignature *overallRecombinationRateSig = nullptr;
	static EidosPropertySignature *overallRecombinationRateMSig = nullptr;
	static EidosPropertySignature *overallRecombinationRateFSig = nullptr;
	static EidosPropertySignature *recombinationEndPositionsSig = nullptr;
	static EidosPropertySignature *recombinationEndPositionsMSig = nullptr;
	static EidosPropertySignature *recombinationEndPositionsFSig = nullptr;
	static EidosPropertySignature *recombinationRatesSig = nullptr;
	static EidosPropertySignature *recombinationRatesMSig = nullptr;
	static EidosPropertySignature *recombinationRatesFSig = nullptr;
	static EidosPropertySignature *geneConversionFractionSig = nullptr;
	static EidosPropertySignature *geneConversionMeanLengthSig = nullptr;
	static EidosPropertySignature *mutationRateSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	static EidosPropertySignature *colorSubstitutionSig = nullptr;
	
	if (!genomicElementsSig)
	{
		genomicElementsSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_genomicElements,				gID_genomicElements,			true,	kEidosValueMaskObject, gSLiM_GenomicElement_Class));
		lastPositionSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_lastPosition,				gID_lastPosition,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		overallRecombinationRateSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_overallRecombinationRate,	gID_overallRecombinationRate,	true,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		overallRecombinationRateMSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_overallRecombinationRateM,	gID_overallRecombinationRateM,	true,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		overallRecombinationRateFSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_overallRecombinationRateF,	gID_overallRecombinationRateF,	true,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		recombinationEndPositionsSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationEndPositions,	gID_recombinationEndPositions,	true,	kEidosValueMaskInt));
		recombinationEndPositionsMSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationEndPositionsM,	gID_recombinationEndPositionsM,	true,	kEidosValueMaskInt));
		recombinationEndPositionsFSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationEndPositionsF,	gID_recombinationEndPositionsF,	true,	kEidosValueMaskInt));
		recombinationRatesSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationRates,			gID_recombinationRates,			true,	kEidosValueMaskFloat));
		recombinationRatesMSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationRatesM,			gID_recombinationRatesM,		true,	kEidosValueMaskFloat));
		recombinationRatesFSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationRatesF,			gID_recombinationRatesF,		true,	kEidosValueMaskFloat));
		geneConversionFractionSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionFraction,		gID_geneConversionFraction,		false,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		geneConversionMeanLengthSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionMeanLength,	gID_geneConversionMeanLength,	false,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		mutationRateSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationRate,				gID_mutationRate,				false,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		tagSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,							gID_tag,						false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		colorSubstitutionSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_colorSubstitution,			gID_colorSubstitution,			false,	kEidosValueMaskString | kEidosValueMaskSingleton));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_genomicElements:				return genomicElementsSig;
		case gID_lastPosition:					return lastPositionSig;
		case gID_overallRecombinationRate:		return overallRecombinationRateSig;
		case gID_overallRecombinationRateM:		return overallRecombinationRateMSig;
		case gID_overallRecombinationRateF:		return overallRecombinationRateFSig;
		case gID_recombinationEndPositions:		return recombinationEndPositionsSig;
		case gID_recombinationEndPositionsM:	return recombinationEndPositionsMSig;
		case gID_recombinationEndPositionsF:	return recombinationEndPositionsFSig;
		case gID_recombinationRates:			return recombinationRatesSig;
		case gID_recombinationRatesM:			return recombinationRatesMSig;
		case gID_recombinationRatesF:			return recombinationRatesFSig;
		case gID_geneConversionFraction:		return geneConversionFractionSig;
		case gID_geneConversionMeanLength:		return geneConversionMeanLengthSig;
		case gID_mutationRate:					return mutationRateSig;
		case gID_tag:							return tagSig;
		case gID_colorSubstitution:				return colorSubstitutionSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *Chromosome_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gID_setRecombinationRate));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Chromosome_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *setRecombinationRateSig = nullptr;
	
	if (!setRecombinationRateSig)
	{
		setRecombinationRateSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setRecombinationRate, kEidosValueMaskNULL))->AddNumeric("rates")->AddInt_ON("ends", gStaticEidosValueNULL)->AddString_OS("sex", gStaticEidosValue_StringAsterisk);
	}
	
	if (p_method_id == gID_setRecombinationRate)
		return setRecombinationRateSig;
	else
		return EidosObjectClass::SignatureForMethod(p_method_id);
}

EidosValue_SP Chromosome_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
}





































































