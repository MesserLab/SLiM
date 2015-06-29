//
//  chromosome.cpp
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


#include "chromosome.h"

#include <iostream>
#include "math.h"

#include "g_rng.h"
#include "slim_global.h"
#include "script_functionsignature.h"


Chromosome::Chromosome(void) : lookup_mutation(nullptr), lookup_recombination(nullptr), exp_neg_overall_mutation_rate_(0.0), exp_neg_overall_recombination_rate_(0.0), probability_both_0(0.0), probability_both_0_OR_mut_0_break_non0(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0(0.0), last_position_(0), overall_mutation_rate_(0.0), overall_recombination_rate_(0.0), gene_conversion_fraction_(0.0), gene_conversion_avg_length_(0.0)
{
}

Chromosome::~Chromosome(void)
{
	//SLIM_ERRSTREAM << "Chromosome::~Chromosome" << std::endl;
	
	if (lookup_mutation)
		gsl_ran_discrete_free(lookup_mutation);
	
	if (lookup_recombination)
		gsl_ran_discrete_free(lookup_recombination);
	
	if (cached_value_lastpos_)
		delete cached_value_lastpos_;
}

// initialize the random lookup tables used by Chromosome to draw mutation and recombination events
void Chromosome::InitializeDraws(void)
{
	if (size() == 0)
		SLIM_TERMINATION << "ERROR (Initialize): empty chromosome" << slim_terminate();
	if (recombination_rates_.size() == 0)
		SLIM_TERMINATION << "ERROR (Initialize): recombination rate not specified" << slim_terminate();
	if (!(overall_mutation_rate_ >= 0))
		SLIM_TERMINATION << "ERROR (Initialize): invalid mutation rate" << slim_terminate();
	
	// calculate the overall mutation rate and the lookup table for mutation locations
	if (cached_value_lastpos_)
	{
		delete cached_value_lastpos_;
		cached_value_lastpos_ = nullptr;
	}
	last_position_ = 0;
	
	double A[size()];
	int l = 0;
	
	for (int i = 0; i < size(); i++) 
	{ 
		if ((*this)[i].end_position_ > last_position_)
			last_position_ = (*this)[i].end_position_;
		
		int l_i = (*this)[i].end_position_ - (*this)[i].start_position_ + 1;
		
		A[i] = static_cast<double>(l_i);
		l += l_i;
	}
	
	if (lookup_mutation)
		gsl_ran_discrete_free(lookup_mutation);
	
	lookup_mutation = gsl_ran_discrete_preproc(size(), A);
	overall_mutation_rate_ = overall_mutation_rate_ * static_cast<double>(l);
	
	// calculate the overall recombination rate and the lookup table for breakpoints
	double B[recombination_rates_.size()];
	
	overall_recombination_rate_ = 0.0;
	
	B[0] = recombination_rates_[0] * static_cast<double>(recombination_end_positions_[0]);
	overall_recombination_rate_ += B[0];
	
	for (int i = 1; i < recombination_rates_.size(); i++) 
	{ 
		B[i] = recombination_rates_[i] * static_cast<double>(recombination_end_positions_[i] - recombination_end_positions_[i - 1]);
		overall_recombination_rate_+= B[i];
		
		if (recombination_end_positions_[i] > last_position_)
			last_position_ = recombination_end_positions_[i];
	}
	
	// SLIM_ERRSTREAM << "overall recombination rate: " << overall_recombination_rate_ << std::endl;
	
	if (lookup_recombination)
		gsl_ran_discrete_free(lookup_recombination);
	
	lookup_recombination = gsl_ran_discrete_preproc(recombination_rates_.size(), B);
	
	// precalculate probabilities for Poisson draws of mutation count and breakpoint count
	double prob_mutation_0 = exp(-overall_mutation_rate_);
	double prob_breakpoint_0 = exp(-overall_recombination_rate_);
	double prob_mutation_not_0 = 1.0 - prob_mutation_0;
	double prob_breakpoint_not_0 = 1.0 - prob_breakpoint_0;
	double prob_both_0 = prob_mutation_0 * prob_breakpoint_0;
	double prob_mutation_0_breakpoint_not_0 = prob_mutation_0 * prob_breakpoint_not_0;
	double prob_mutation_not_0_breakpoint_0 = prob_mutation_not_0 * prob_breakpoint_0;
	
//	SLIM_OUTSTREAM << "overall_mutation_rate_ == " << overall_mutation_rate_ << std::endl;
//	SLIM_OUTSTREAM << "prob_mutation_0 == " << prob_mutation_0 << std::endl;
//	SLIM_OUTSTREAM << "prob_breakpoint_0 == " << prob_breakpoint_0 << std::endl;
//	SLIM_OUTSTREAM << "prob_mutation_not_0 == " << prob_mutation_not_0 << std::endl;
//	SLIM_OUTSTREAM << "prob_breakpoint_not_0 == " << prob_breakpoint_not_0 << std::endl;
//	SLIM_OUTSTREAM << "prob_both_0 == " << prob_both_0 << std::endl;
//	SLIM_OUTSTREAM << "prob_mutation_0_breakpoint_not_0 == " << prob_mutation_0_breakpoint_not_0 << std::endl;
//	SLIM_OUTSTREAM << "prob_mutation_not_0_breakpoint_0 == " << prob_mutation_not_0_breakpoint_0 << std::endl;
	
	exp_neg_overall_mutation_rate_ = prob_mutation_0;
	exp_neg_overall_recombination_rate_ = prob_breakpoint_0;
	
	probability_both_0 = prob_both_0;
	probability_both_0_OR_mut_0_break_non0 = prob_both_0 + prob_mutation_0_breakpoint_not_0;
	probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0 = prob_both_0 + (prob_mutation_0_breakpoint_not_0 + prob_mutation_not_0_breakpoint_0);
}

// draw a new mutation, based on the genomic element types present and their mutational proclivities
Mutation *Chromosome::DrawNewMutation(int p_subpop_index, int p_generation) const
{
	int genomic_element = static_cast<int>(gsl_ran_discrete(g_rng, lookup_mutation));
	const GenomicElement &source_element = (*this)[genomic_element];
	const GenomicElementType &genomic_element_type = *source_element.genomic_element_type_ptr_;
	MutationType *mutation_type_ptr = genomic_element_type.DrawMutationType();
	
	int position = source_element.start_position_ + static_cast<int>(gsl_rng_uniform_int(g_rng, source_element.end_position_ - source_element.start_position_ + 1));  
	
	double selection_coeff = mutation_type_ptr->DrawSelectionCoefficient();
	
	return new Mutation(mutation_type_ptr, position, selection_coeff, p_subpop_index, p_generation);
}

// choose a set of recombination breakpoints, based on recombination intervals, overall recombination rate, and gene conversion probability
std::vector<int> Chromosome::DrawBreakpoints(const int p_num_breakpoints) const
{
	vector<int> breakpoints;
	
	// draw recombination breakpoints
	for (int i = 0; i < p_num_breakpoints; i++)
	{
		int breakpoint = 0;
		int recombination_interval = static_cast<int>(gsl_ran_discrete(g_rng, lookup_recombination));
		
		// choose a breakpoint anywhere in the chosen recombination interval with equal probability
		if (recombination_interval == 0)
			breakpoint = static_cast<int>(gsl_rng_uniform_int(g_rng, recombination_end_positions_[recombination_interval]));
		else
			breakpoint = recombination_end_positions_[recombination_interval - 1] + static_cast<int>(gsl_rng_uniform_int(g_rng, recombination_end_positions_[recombination_interval] - recombination_end_positions_[recombination_interval - 1]));
		
		breakpoints.push_back(breakpoint);
		
		// recombination can result in gene conversion, with probability gene_conversion_fraction_
		if (gene_conversion_fraction_ > 0.0)
		{
			if ((gene_conversion_fraction_ < 1.0) && (gsl_rng_uniform(g_rng) < gene_conversion_fraction_))
			{
				// for gene conversion, choose a second breakpoint that is relatively likely to be near to the first
				int breakpoint2 = breakpoint + gsl_ran_geometric(g_rng, 1.0 / gene_conversion_avg_length_);
				
				breakpoints.push_back(breakpoint2);
			}
		}
	}
	
	return breakpoints;
}


//
// SLiMscript support
//
#pragma mark SLiMscript support

const std::string *Chromosome::ElementType(void) const
{
	return &gStr_Chromosome;
}

std::vector<std::string> Chromosome::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back(gStr_genomicElements);					// this
	constants.push_back(gStr_lastPosition);					// last_position_
	constants.push_back(gStr_overallRecombinationRate);		// overall_recombination_rate_
	constants.push_back(gStr_recombinationEndPositions);		// recombination_end_positions_
	constants.push_back(gStr_recombinationRates);				// recombination_rates_
	
	return constants;
}

std::vector<std::string> Chromosome::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	variables.push_back(gStr_geneConversionFraction);			// gene_conversion_fraction_
	variables.push_back(gStr_geneConversionMeanLength);		// gene_conversion_avg_length_
	variables.push_back(gStr_overallMutationRate);				// overall_mutation_rate_
	
	return variables;
}

bool Chromosome::MemberIsReadOnly(GlobalStringID p_member_id) const
{
	switch (p_member_id)
	{
			// constants
		case gID_genomicElements:
		case gID_lastPosition:
		case gID_overallRecombinationRate:
		case gID_recombinationEndPositions:
		case gID_recombinationRates:
			return true;
			
			// variables
		case gID_geneConversionFraction:
		case gID_geneConversionMeanLength:
		case gID_overallMutationRate:
			return false;
			
			// all others, including gID_none
		default:
			return ScriptObjectElement::MemberIsReadOnly(p_member_id);
	}
}

ScriptValue *Chromosome::GetValueForMember(GlobalStringID p_member_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_member_id)
	{
			// constants
		case gID_genomicElements:
		{
			ScriptValue_Object_vector *vec = new ScriptValue_Object_vector();
			
			for (auto genomic_element_iter = this->begin(); genomic_element_iter != this->end(); genomic_element_iter++)
				vec->PushElement(&(*genomic_element_iter));		// operator * can be overloaded by the iterator
			
			return vec;
		}
		case gID_lastPosition:
		{
			// Note that this cache cannot be invalidated, because we are guaranteeing that this object will
			// live for at least as long as the symbol table it may be placed into!
			if (!cached_value_lastpos_)
				cached_value_lastpos_ = (new ScriptValue_Int_singleton_const(last_position_))->SetExternalPermanent();
			return cached_value_lastpos_;
		}
		case gID_overallRecombinationRate:
			return new ScriptValue_Float_singleton_const(overall_recombination_rate_);
		case gID_recombinationEndPositions:
			return new ScriptValue_Int_vector(recombination_end_positions_);
		case gID_recombinationRates:
			return new ScriptValue_Float_vector(recombination_rates_);
			
			// variables
		case gID_geneConversionFraction:
			return new ScriptValue_Float_singleton_const(gene_conversion_fraction_);
		case gID_geneConversionMeanLength:
			return new ScriptValue_Float_singleton_const(gene_conversion_avg_length_);
		case gID_overallMutationRate:
			return new ScriptValue_Float_singleton_const(overall_mutation_rate_);
			
			// all others, including gID_none
		default:
			return ScriptObjectElement::GetValueForMember(p_member_id);
	}
}

void Chromosome::SetValueForMember(GlobalStringID p_member_id, ScriptValue *p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_member_id)
	{
		case gID_geneConversionFraction:
		{
			TypeCheckValue(__func__, p_member_id, p_value, kScriptValueMaskInt | kScriptValueMaskFloat);
			
			double value = p_value->FloatAtIndex(0);
			RangeCheckValue(__func__, p_member_id, (value >= 0.0) && (value <= 1.0));
			
			gene_conversion_fraction_ = value;
			return;
		}
		case gID_geneConversionMeanLength:
		{
			TypeCheckValue(__func__, p_member_id, p_value, kScriptValueMaskInt | kScriptValueMaskFloat);
			
			double value = p_value->FloatAtIndex(0);
			RangeCheckValue(__func__, p_member_id, value >= 0);
			
			gene_conversion_avg_length_ = value;
			return;
		}
		case gID_overallMutationRate:
		{
			TypeCheckValue(__func__, p_member_id, p_value, kScriptValueMaskFloat);
			
			double value = p_value->FloatAtIndex(0);
			RangeCheckValue(__func__, p_member_id, (value >= 0.0) && (value <= 1.0));
			
			overall_mutation_rate_ = value;
			return;
		}
			
			// all others, including gID_none
		default:
			return ScriptObjectElement::SetValueForMember(p_member_id, p_value);
	}
}

std::vector<std::string> Chromosome::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	methods.push_back(gStr_changeRecombinationIntervals);
	
	return methods;
}

const FunctionSignature *Chromosome::SignatureForMethod(GlobalStringID p_method_id) const
{
	static FunctionSignature *changeRecombinationIntervalsSig = nullptr;
	
	if (!changeRecombinationIntervalsSig)
	{
		changeRecombinationIntervalsSig = (new FunctionSignature(gStr_changeRecombinationIntervals, FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddInt()->AddNumeric();
	}
	
	if (p_method_id == gID_changeRecombinationIntervals)
		return changeRecombinationIntervalsSig;
	else
		return ScriptObjectElement::SignatureForMethod(p_method_id);
}

ScriptValue *Chromosome::ExecuteMethod(GlobalStringID p_method_id, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter)
{
	ScriptValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0] : nullptr);
	ScriptValue *arg1_value = ((p_argument_count >= 2) ? p_arguments[1] : nullptr);
	
	//
	//	*********************	- (void)changeRecombinationIntervals(integer ends, numeric rates)
	//
#pragma mark -changeRecombinationIntervals()
	
	if (p_method_id == gID_changeRecombinationIntervals)
	{
		int ends_count = arg0_value->Count();
		int rates_count = arg1_value->Count();
		
		// check first
		if ((ends_count == 0) || (ends_count != rates_count))
			SLIM_TERMINATION << "ERROR (Chromosome::ExecuteMethod): changeRecombinationIntervals() requires ends and rates to be equal in size, containing at least one entry." << slim_terminate();
		
		for (int value_index = 0; value_index < ends_count; ++value_index)
		{
			int64_t end = arg0_value->IntAtIndex(value_index);
			double rate = arg1_value->FloatAtIndex(value_index);
			
			if (value_index > 0)
				if (end <= arg0_value->IntAtIndex(value_index - 1))
					SLIM_TERMINATION << "ERROR (Chromosome::ExecuteMethod): changeRecombinationIntervals() requires ends to be in ascending order." << slim_terminate();
			
			if (rate < 0.0)
				SLIM_TERMINATION << "ERROR (Chromosome::ExecuteMethod): changeRecombinationIntervals() requires rates to be >= 0." << slim_terminate();
		}
		
		// The stake here is that the last position in the chromosome is not allowed to change after the chromosome is
		// constructed.  When we call InitializeDraws() below, we recalculate the last position – and we must come up
		// with the same answer that we got before, otherwise our last_position_ cache is invalid.
		if (arg0_value->IntAtIndex(ends_count - 1) != last_position_)
			SLIM_TERMINATION << "ERROR (Chromosome::ExecuteMethod): changeRecombinationIntervals() requires the last interval to end at the last position of the chromosome." << slim_terminate();
		
		// then adopt them
		recombination_end_positions_.clear();
		recombination_rates_.clear();
		
		for (int value_index = 0; value_index < ends_count; ++value_index)
		{
			int64_t end = arg0_value->IntAtIndex(value_index);
			double rate = arg1_value->FloatAtIndex(value_index);
			
			recombination_end_positions_.push_back((int)end);
			recombination_rates_.push_back(rate);
		}
		
		InitializeDraws();
		
		return gStaticScriptValueNULLInvisible;
	}
	
	
	// all others, including gID_none
	else
		return ScriptObjectElement::ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}





































































