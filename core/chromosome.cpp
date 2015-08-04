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

#include "eidos_rng.h"
#include "slim_global.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"


Chromosome::Chromosome(void) : lookup_mutation(nullptr), lookup_recombination(nullptr), exp_neg_element_mutation_rate_(0.0), exp_neg_overall_recombination_rate_(0.0), probability_both_0(0.0), probability_both_0_OR_mut_0_break_non0(0.0), probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0(0.0), last_position_(0), overall_mutation_rate_(0.0), element_mutation_rate_(0.0), overall_recombination_rate_(0.0), gene_conversion_fraction_(0.0), gene_conversion_avg_length_(0.0)
{
}

Chromosome::~Chromosome(void)
{
	//EIDOS_ERRSTREAM << "Chromosome::~Chromosome" << std::endl;
	
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
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): empty chromosome" << eidos_terminate();
	if (recombination_rates_.size() == 0)
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): recombination rate not specified" << eidos_terminate();
	if (!(overall_mutation_rate_ >= 0))
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): invalid mutation rate" << eidos_terminate();
	
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
	element_mutation_rate_ = overall_mutation_rate_ * static_cast<double>(l);
	
	// patch the recombination interval end vector if it is empty; see setRecombinationRate() and initializeRecombinationRate()
	// basically, the length of the chromosome might not have been known yet when the user set the rate
	if (recombination_end_positions_.size() == 0)
	{
		// patching can only be done when a single uniform rate is specified
		if (recombination_rates_.size() != 1)
			EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): recombination endpoints not specified" << eidos_terminate();
		
		recombination_end_positions_.push_back(last_position_);
	}
	
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
	
	if (recombination_end_positions_[recombination_rates_.size() - 1] < last_position_)
		EIDOS_TERMINATION << "ERROR (Chromosome::InitializeDraws): recombination endpoints do not cover all genomic elements" << eidos_terminate();
	
	// EIDOS_ERRSTREAM << "overall recombination rate: " << overall_recombination_rate_ << std::endl;
	
	if (lookup_recombination)
		gsl_ran_discrete_free(lookup_recombination);
	
	lookup_recombination = gsl_ran_discrete_preproc(recombination_rates_.size(), B);
	
	// precalculate probabilities for Poisson draws of mutation count and breakpoint count
	double prob_mutation_0 = exp(-element_mutation_rate_);
	double prob_breakpoint_0 = exp(-overall_recombination_rate_);
	double prob_mutation_not_0 = 1.0 - prob_mutation_0;
	double prob_breakpoint_not_0 = 1.0 - prob_breakpoint_0;
	double prob_both_0 = prob_mutation_0 * prob_breakpoint_0;
	double prob_mutation_0_breakpoint_not_0 = prob_mutation_0 * prob_breakpoint_not_0;
	double prob_mutation_not_0_breakpoint_0 = prob_mutation_not_0 * prob_breakpoint_0;
	
//	EIDOS_OUTSTREAM << "element_mutation_rate_ == " << element_mutation_rate_ << std::endl;
//	EIDOS_OUTSTREAM << "prob_mutation_0 == " << prob_mutation_0 << std::endl;
//	EIDOS_OUTSTREAM << "prob_breakpoint_0 == " << prob_breakpoint_0 << std::endl;
//	EIDOS_OUTSTREAM << "prob_mutation_not_0 == " << prob_mutation_not_0 << std::endl;
//	EIDOS_OUTSTREAM << "prob_breakpoint_not_0 == " << prob_breakpoint_not_0 << std::endl;
//	EIDOS_OUTSTREAM << "prob_both_0 == " << prob_both_0 << std::endl;
//	EIDOS_OUTSTREAM << "prob_mutation_0_breakpoint_not_0 == " << prob_mutation_0_breakpoint_not_0 << std::endl;
//	EIDOS_OUTSTREAM << "prob_mutation_not_0_breakpoint_0 == " << prob_mutation_not_0_breakpoint_0 << std::endl;
	
	exp_neg_element_mutation_rate_ = prob_mutation_0;
	exp_neg_overall_recombination_rate_ = prob_breakpoint_0;
	
	probability_both_0 = prob_both_0;
	probability_both_0_OR_mut_0_break_non0 = prob_both_0 + prob_mutation_0_breakpoint_not_0;
	probability_both_0_OR_mut_0_break_non0_OR_mut_non0_break_0 = prob_both_0 + (prob_mutation_0_breakpoint_not_0 + prob_mutation_not_0_breakpoint_0);
}

// draw a new mutation, based on the genomic element types present and their mutational proclivities
Mutation *Chromosome::DrawNewMutation(int p_subpop_index, int p_generation) const
{
	int genomic_element = static_cast<int>(gsl_ran_discrete(gEidos_rng, lookup_mutation));
	const GenomicElement &source_element = (*this)[genomic_element];
	const GenomicElementType &genomic_element_type = *source_element.genomic_element_type_ptr_;
	MutationType *mutation_type_ptr = genomic_element_type.DrawMutationType();
	
	int position = source_element.start_position_ + static_cast<int>(gsl_rng_uniform_int(gEidos_rng, source_element.end_position_ - source_element.start_position_ + 1));  
	
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
		int recombination_interval = static_cast<int>(gsl_ran_discrete(gEidos_rng, lookup_recombination));
		
		// choose a breakpoint anywhere in the chosen recombination interval with equal probability
		if (recombination_interval == 0)
			breakpoint = static_cast<int>(gsl_rng_uniform_int(gEidos_rng, recombination_end_positions_[recombination_interval]));
		else
			breakpoint = recombination_end_positions_[recombination_interval - 1] + static_cast<int>(gsl_rng_uniform_int(gEidos_rng, recombination_end_positions_[recombination_interval] - recombination_end_positions_[recombination_interval - 1]));
		
		breakpoints.push_back(breakpoint);
		
		// recombination can result in gene conversion, with probability gene_conversion_fraction_
		if (gene_conversion_fraction_ > 0.0)
		{
			if ((gene_conversion_fraction_ < 1.0) && (gsl_rng_uniform(gEidos_rng) < gene_conversion_fraction_))
			{
				// for gene conversion, choose a second breakpoint that is relatively likely to be near to the first
				int breakpoint2 = breakpoint + gsl_ran_geometric(gEidos_rng, 1.0 / gene_conversion_avg_length_);
				
				breakpoints.push_back(breakpoint2);
			}
		}
	}
	
	return breakpoints;
}


//
// Eidos support
//
#pragma mark Eidos support

const std::string *Chromosome::ElementType(void) const
{
	return &gStr_Chromosome;
}

const std::vector<const EidosPropertySignature *> *Chromosome::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectElement::Properties());
		properties->push_back(SignatureForProperty(gID_genomicElements));
		properties->push_back(SignatureForProperty(gID_lastPosition));
		properties->push_back(SignatureForProperty(gID_overallRecombinationRate));
		properties->push_back(SignatureForProperty(gID_recombinationEndPositions));
		properties->push_back(SignatureForProperty(gID_recombinationRates));
		properties->push_back(SignatureForProperty(gID_geneConversionFraction));
		properties->push_back(SignatureForProperty(gID_geneConversionMeanLength));
		properties->push_back(SignatureForProperty(gID_overallMutationRate));
		properties->push_back(SignatureForProperty(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Chromosome::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *genomicElementsSig = nullptr;
	static EidosPropertySignature *lastPositionSig = nullptr;
	static EidosPropertySignature *overallRecombinationRateSig = nullptr;
	static EidosPropertySignature *recombinationEndPositionsSig = nullptr;
	static EidosPropertySignature *recombinationRatesSig = nullptr;
	static EidosPropertySignature *geneConversionFractionSig = nullptr;
	static EidosPropertySignature *geneConversionMeanLengthSig = nullptr;
	static EidosPropertySignature *overallMutationRateSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!genomicElementsSig)
	{
		genomicElementsSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_genomicElements,				gID_genomicElements,			true,	kEidosValueMaskObject, &gStr_GenomicElement));
		lastPositionSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_lastPosition,				gID_lastPosition,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		overallRecombinationRateSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_overallRecombinationRate,	gID_overallRecombinationRate,	true,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		recombinationEndPositionsSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationEndPositions,	gID_recombinationEndPositions,	true,	kEidosValueMaskInt));
		recombinationRatesSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_recombinationRates,			gID_recombinationRates,			true,	kEidosValueMaskFloat));
		geneConversionFractionSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionFraction,		gID_geneConversionFraction,		false,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		geneConversionMeanLengthSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_geneConversionMeanLength,	gID_geneConversionMeanLength,	false,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		overallMutationRateSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_overallMutationRate,			gID_overallMutationRate,		false,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		tagSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,							gID_tag,						false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_genomicElements:			return genomicElementsSig;
		case gID_lastPosition:				return lastPositionSig;
		case gID_overallRecombinationRate:	return overallRecombinationRateSig;
		case gID_recombinationEndPositions:	return recombinationEndPositionsSig;
		case gID_recombinationRates:		return recombinationRatesSig;
		case gID_geneConversionFraction:	return geneConversionFractionSig;
		case gID_geneConversionMeanLength:	return geneConversionMeanLengthSig;
		case gID_overallMutationRate:		return overallMutationRateSig;
		case gID_tag:						return tagSig;
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SignatureForProperty(p_property_id);
	}
}

EidosValue *Chromosome::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_genomicElements:
		{
			EidosValue_Object_vector *vec = new EidosValue_Object_vector();
			
			for (auto genomic_element_iter = this->begin(); genomic_element_iter != this->end(); genomic_element_iter++)
				vec->PushElement(&(*genomic_element_iter));		// operator * can be overloaded by the iterator
			
			return vec;
		}
		case gID_lastPosition:
		{
			// Note that this cache cannot be invalidated, because we are guaranteeing that this object will
			// live for at least as long as the symbol table it may be placed into!
			if (!cached_value_lastpos_)
				cached_value_lastpos_ = (new EidosValue_Int_singleton_const(last_position_))->SetExternalPermanent();
			return cached_value_lastpos_;
		}
		case gID_overallRecombinationRate:
			return new EidosValue_Float_singleton_const(overall_recombination_rate_);
		case gID_recombinationEndPositions:
			return new EidosValue_Int_vector(recombination_end_positions_);
		case gID_recombinationRates:
			return new EidosValue_Float_vector(recombination_rates_);
			
			// variables
		case gID_geneConversionFraction:
			return new EidosValue_Float_singleton_const(gene_conversion_fraction_);
		case gID_geneConversionMeanLength:
			return new EidosValue_Float_singleton_const(gene_conversion_avg_length_);
		case gID_overallMutationRate:
			return new EidosValue_Float_singleton_const(overall_mutation_rate_);
		case gID_tag:
			return new EidosValue_Int_singleton_const(tag_value_);
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void Chromosome::SetProperty(EidosGlobalStringID p_property_id, EidosValue *p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_geneConversionFraction:
		{
			double value = p_value->FloatAtIndex(0);
			
			if ((value < 0.0) || (value > 1.0))
				EIDOS_TERMINATION << "ERROR (Chromosome::SetProperty): new value for property " << StringForEidosGlobalStringID(p_property_id) << " is out of range." << eidos_terminate();
			
			gene_conversion_fraction_ = value;
			return;
		}
		case gID_geneConversionMeanLength:
		{
			double value = p_value->FloatAtIndex(0);
			
			if (value < 0.0)
				EIDOS_TERMINATION << "ERROR (Chromosome::SetProperty): new value for property " << StringForEidosGlobalStringID(p_property_id) << " is out of range." << eidos_terminate();
			
			gene_conversion_avg_length_ = value;
			return;
		}
		case gID_overallMutationRate:
		{
			double value = p_value->FloatAtIndex(0);
			
			if ((value < 0.0) || (value > 1.0))
				EIDOS_TERMINATION << "ERROR (Chromosome::SetProperty): new value for property " << StringForEidosGlobalStringID(p_property_id) << " is out of range." << eidos_terminate();
			
			overall_mutation_rate_ = value;
			InitializeDraws();
			return;
		}
		case gID_tag:
		{
			int64_t value = p_value->IntAtIndex(0);
			
			tag_value_ = value;
			return;
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SetProperty(p_property_id, p_value);
	}
}

const std::vector<const EidosMethodSignature *> *Chromosome::Methods(void) const
{
	std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectElement::Methods());
		methods->push_back(SignatureForMethod(gID_setRecombinationRate));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Chromosome::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *setRecombinationRateSig = nullptr;
	
	if (!setRecombinationRateSig)
	{
		setRecombinationRateSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setRecombinationRate, kEidosValueMaskNULL))->AddNumeric("rates")->AddInt_O("ends");
	}
	
	if (p_method_id == gID_setRecombinationRate)
		return setRecombinationRateSig;
	else
		return EidosObjectElement::SignatureForMethod(p_method_id);
}

EidosValue *Chromosome::ExecuteMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0] : nullptr);
	EidosValue *arg1_value = ((p_argument_count >= 2) ? p_arguments[1] : nullptr);
	
	//
	//	*********************	- (void)setRecombinationRate(numeric rates, [integer ends])
	//
#pragma mark -setRecombinationRate()
	
	if (p_method_id == gID_setRecombinationRate)
	{
		int rate_count = arg0_value->Count();
		
		if (p_argument_count == 1)
		{
			if (rate_count != 1)
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod): setRecombinationRate() requires rates to be a singleton if ends is not supplied." << eidos_terminate();
			
			double recombination_rate = arg0_value->FloatAtIndex(0);
			
			// check values
			if (recombination_rate < 0.0)
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod): setRecombinationRate() requires rates to be >= 0." << eidos_terminate();
			
			// then adopt them
			recombination_rates_.clear();
			recombination_end_positions_.clear();
			
			recombination_rates_.push_back(recombination_rate);
			//recombination_end_positions_.push_back(?);	// deferred; patched in Chromosome::InitializeDraws().
		}
		else if (p_argument_count == 2)
		{
			int end_count = arg1_value->Count();
			
			if ((end_count != rate_count) || (end_count == 0))
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod): setRecombinationRate() requires ends and rates to be of equal and nonzero size." << eidos_terminate();
			
			// check values
			for (int value_index = 0; value_index < end_count; ++value_index)
			{
				double recombination_rate = arg0_value->FloatAtIndex(value_index);
				int64_t recombination_end_position = arg1_value->IntAtIndex(value_index);
				
				if (value_index > 0)
					if (recombination_end_position <= arg1_value->IntAtIndex(value_index - 1))
						EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod): setRecombinationRate() requires ends to be in ascending order." << eidos_terminate();
				
				if (recombination_rate < 0.0)
					EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod): setRecombinationRate() requires rates to be >= 0." << eidos_terminate();
			}
			
			// The stake here is that the last position in the chromosome is not allowed to change after the chromosome is
			// constructed.  When we call InitializeDraws() below, we recalculate the last position – and we must come up
			// with the same answer that we got before, otherwise our last_position_ cache is invalid.
			if (arg1_value->IntAtIndex(end_count - 1) != last_position_)
				EIDOS_TERMINATION << "ERROR (Chromosome::ExecuteMethod): setRecombinationRate() requires the last interval to end at the last position of the chromosome." << eidos_terminate();
			
			// then adopt them
			recombination_rates_.clear();
			recombination_end_positions_.clear();
			
			for (int interval_index = 0; interval_index < end_count; ++interval_index)
			{
				double recombination_rate = arg0_value->FloatAtIndex(interval_index);
				int64_t recombination_end_position = arg1_value->IntAtIndex(interval_index);	// used to have a -1; switched to zero-based
				
				recombination_rates_.push_back(recombination_rate);
				recombination_end_positions_.push_back((int)recombination_end_position);
			}
		}
		
		InitializeDraws();
		
		return gStaticEidosValueNULLInvisible;
	}
	
	
	// all others, including gID_none
	else
		return EidosObjectElement::ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}





































































