//
//  substitution.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2025 Benjamin C. Haller.  All rights reserved.
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


#include "substitution.h"
#include "slim_globals.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "species.h"

#include <iostream>
#include <algorithm>
#include <string>
#include <vector>


#pragma mark -
#pragma mark Substitution
#pragma mark -

Substitution::Substitution(Mutation &p_mutation, slim_tick_t p_fixation_tick) :
	EidosDictionaryRetained(), mutation_type_ptr_(p_mutation.mutation_type_ptr_), position_(p_mutation.position_), selection_coeff_(p_mutation.selection_coeff_), subpop_index_(p_mutation.subpop_index_), origin_tick_(p_mutation.origin_tick_), fixation_tick_(p_fixation_tick), chromosome_index_(p_mutation.chromosome_index_), nucleotide_(p_mutation.nucleotide_), mutation_id_(p_mutation.mutation_id_), tag_value_(p_mutation.tag_value_)
	
{
	AddKeysAndValuesFrom(&p_mutation);
	// No call to ContentsChanged() here; we know we use Dictionary not DataFrame, and Mutation already vetted the dictionary
}

Substitution::Substitution(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, double p_selection_coeff, slim_objectid_t p_subpop_index, slim_tick_t p_tick, slim_tick_t p_fixation_tick, int8_t p_nucleotide) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(static_cast<slim_selcoeff_t>(p_selection_coeff)), subpop_index_(p_subpop_index), origin_tick_(p_tick), fixation_tick_(p_fixation_tick), chromosome_index_(p_chromosome_index), nucleotide_(p_nucleotide), mutation_id_(p_mutation_id), tag_value_(SLIM_TAG_UNSET_VALUE)
{
}

void Substitution::PrintForSLiMOutput(std::ostream &p_out) const
{ 
	p_out << mutation_id_ << " m" << mutation_type_ptr_->mutation_type_id_ << " " << position_;
	
	// BCH 2/2/2025: Note that in multi-chrom models, this method now prints the chromosome symbol after the position
	// For brevity and backward compatibility, the chromosome symbol is not printed in single-chromosome models
	Species &species = mutation_type_ptr_->species_;
	const std::vector<Chromosome *> &chromosomes = species.Chromosomes();
	
	if (chromosomes.size() > 1)
	{
		Chromosome *chromosome = chromosomes[chromosome_index_];
		
		p_out << " \"" << chromosome->Symbol() << "\"";
	}
	
	// and then the remainder of the output line
	p_out << " " << selection_coeff_ << " " << mutation_type_ptr_->dominance_coeff_ << " p" << subpop_index_ << " " << origin_tick_ << " "<< fixation_tick_;
	
	// output a nucleotide if available
	if (mutation_type_ptr_->nucleotide_based_)
		p_out << " " << gSLiM_Nucleotides[nucleotide_];
	
	p_out << std::endl;
}

void Substitution::PrintForSLiMOutput_Tag(std::ostream &p_out) const
{ 
	// BCH 4/7/2025: This is a copy of PrintForSLiMOutput() with output of tag_value_ added at the end
	
	p_out << mutation_id_ << " m" << mutation_type_ptr_->mutation_type_id_ << " " << position_;
	
	// BCH 2/2/2025: Note that in multi-chrom models, this method now prints the chromosome symbol after the position
	// For brevity and backward compatibility, the chromosome symbol is not printed in single-chromosome models
	Species &species = mutation_type_ptr_->species_;
	const std::vector<Chromosome *> &chromosomes = species.Chromosomes();
	
	if (chromosomes.size() > 1)
	{
		Chromosome *chromosome = chromosomes[chromosome_index_];
		
		p_out << " \"" << chromosome->Symbol() << "\"";
	}
	
	// and then the remainder of the output line
	p_out << " " << selection_coeff_ << " " << mutation_type_ptr_->dominance_coeff_ << " p" << subpop_index_ << " " << origin_tick_ << " "<< fixation_tick_;
	
	// output a nucleotide if available
	if (mutation_type_ptr_->nucleotide_based_)
		p_out << " " << gSLiM_Nucleotides[nucleotide_];
	
	// output the tag value, or '?' or the tag is not defined
	if (tag_value_ == SLIM_TAG_UNSET_VALUE)
		p_out << ' ' << '?';
	else
		p_out << ' ' << tag_value_;
	
	p_out << std::endl;
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosClass *Substitution::Class(void) const
{
	return gSLiM_Substitution_Class;
}

void Substitution::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassNameForDisplay() << "<" << mutation_id_ << ":" << selection_coeff_ << ">";
}

EidosValue_SP Substitution::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_chromosome:
		{
			Species &species = mutation_type_ptr_->species_;
			const std::vector<Chromosome *> &chromosomes = species.Chromosomes();
			Chromosome *chromosome = chromosomes[chromosome_index_];
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(chromosome, gSLiM_Chromosome_Class));
		}
		case gID_id:					// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(mutation_id_));
		case gID_mutationType:			// ACCELERATED
			return mutation_type_ptr_->SymbolTableEntry().second;
		case gID_position:				// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(position_));
		case gID_selectionCoeff:		// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(selection_coeff_));
		case gID_originTick:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(origin_tick_));
		case gID_fixationTick:		// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(fixation_tick_));
			
			// variables
		case gID_nucleotide:			// ACCELERATED
		{
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Substitution::GetProperty): property nucleotide is only defined for nucleotide-based mutations." << EidosTerminate();
			
			switch (nucleotide_)
			{
				case 0:	return gStaticEidosValue_StringA;
				case 1:	return gStaticEidosValue_StringC;
				case 2:	return gStaticEidosValue_StringG;
				case 3:	return gStaticEidosValue_StringT;
				default:
					EIDOS_TERMINATION << "ERROR (Substitution::GetProperty): (internal error) unrecognized value for nucleotide_." << EidosTerminate();
			}
		}
		case gID_nucleotideValue:		// ACCELERATED
		{
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Substitution::GetProperty): property nucleotideValue is only defined for nucleotide-based mutations." << EidosTerminate();
			
			switch (nucleotide_)
			{
				case 0:	return gStaticEidosValue_Integer0;
				case 1:	return gStaticEidosValue_Integer1;
				case 2:	return gStaticEidosValue_Integer2;
				case 3:	return gStaticEidosValue_Integer3;
				default:
					EIDOS_TERMINATION << "ERROR (Substitution::GetProperty): (internal error) unrecognized value for nucleotide_." << EidosTerminate();
			}
		}
		case gID_subpopID:				// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(subpop_index_));
		case gID_tag:					// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Substitution::GetProperty): property tag accessed on substitution before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(tag_value));
		}
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue *Substitution::GetProperty_Accelerated_id(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->mutation_id_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_nucleotide(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String())->Reserve((int)p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		int8_t nucleotide = value->nucleotide_;
		
		if (nucleotide == -1)
			EIDOS_TERMINATION << "ERROR (Substitution::GetProperty_Accelerated_nucleotide): property nucleotide is only defined for nucleotide-based mutations." << EidosTerminate();
		
		if (nucleotide == 0)
			string_result->PushString(gStr_A);
		else if (nucleotide == 1)
			string_result->PushString(gStr_C);
		else if (nucleotide == 2)
			string_result->PushString(gStr_G);
		else if (nucleotide == 3)
			string_result->PushString(gStr_T);
	}
	
	return string_result;
}

EidosValue *Substitution::GetProperty_Accelerated_nucleotideValue(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		int8_t nucleotide = value->nucleotide_;
		
		if (nucleotide == -1)
			EIDOS_TERMINATION << "ERROR (Substitution::GetProperty_Accelerated_nucleotideValue): property nucleotideValue is only defined for nucleotide-based mutations." << EidosTerminate();
		
		int_result->set_int_no_check(nucleotide, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_originTick(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->origin_tick_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_fixationTick(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->fixation_tick_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_position(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->position_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_subpopID(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->subpop_index_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (Substitution::GetProperty_Accelerated_tag): property tag accessed on substitution before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_selectionCoeff(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->selection_coeff_, value_index);
	}
	
	return float_result;
}

EidosValue *Substitution::GetProperty_Accelerated_mutationType(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Object *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_MutationType_Class))->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		object_result->set_object_element_no_check_NORR(value->mutation_type_ptr_, value_index);
	}
	
	return object_result;
}

void Substitution::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_nucleotide:
		{
			const std::string &nucleotide = ((EidosValue_String &)p_value).StringRefAtIndex_NOCAST(0, nullptr);
			
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Substitution::SetProperty): property nucleotide is only defined for nucleotide-based substitutions." << EidosTerminate();
			
			if (nucleotide == gStr_A)		nucleotide_ = 0;
			else if (nucleotide == gStr_C)	nucleotide_ = 1;
			else if (nucleotide == gStr_G)	nucleotide_ = 2;
			else if (nucleotide == gStr_T)	nucleotide_ = 3;
			else EIDOS_TERMINATION << "ERROR (Substitution::SetProperty): property nucleotide may only be set to 'A', 'C', 'G', or 'T'." << EidosTerminate();
			return;
		}
		case gID_nucleotideValue:
		{
			int64_t nucleotide = p_value.IntAtIndex_NOCAST(0, nullptr);
			
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Substitution::SetProperty): property nucleotideValue is only defined for nucleotide-based substitutions." << EidosTerminate();
			if ((nucleotide < 0) || (nucleotide > 3))
				EIDOS_TERMINATION << "ERROR (Substitution::SetProperty): property nucleotideValue may only be set to 0 (A), 1 (C), 2 (G), or 3 (T)." << EidosTerminate();
			
			nucleotide_ = (int8_t)nucleotide;
			return;
		}
		case gID_subpopID:
		{
			slim_objectid_t value = SLiMCastToObjectidTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			subpop_index_ = value;
			return;
		}
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			tag_value_ = value;
			return;
		}
		
		default:
		{
			return super::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue_SP Substitution::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		default:					return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}


//
//	Substitution_Class
//
#pragma mark -
#pragma mark Substitution_Class
#pragma mark -

EidosClass *gSLiM_Substitution_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *Substitution_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Substitution_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_chromosome,			true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Chromosome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,					true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_id));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationType,		true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_MutationType_Class))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_mutationType));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_position,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_position));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_selectionCoeff,		true,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_selectionCoeff));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopID,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_subpopID));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotide,			false,	kEidosValueMaskString | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_nucleotide));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotideValue,	false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_nucleotideValue));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_originTick,	true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_originTick));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_fixationTick,	true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_fixationTick));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_tag));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Substitution_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Substitution_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}







































































