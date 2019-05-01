//
//  substitution.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2019 Philipp Messer.  All rights reserved.
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
#include "slim_global.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#include <iostream>
#include <algorithm>
#include <string>
#include <vector>


#pragma mark -
#pragma mark Substitution
#pragma mark -

Substitution::Substitution(Mutation &p_mutation, slim_generation_t p_fixation_generation) :
mutation_type_ptr_(p_mutation.mutation_type_ptr_), position_(p_mutation.position_), selection_coeff_(p_mutation.selection_coeff_), subpop_index_(p_mutation.subpop_index_), origin_generation_(p_mutation.origin_generation_), fixation_generation_(p_fixation_generation), nucleotide_(p_mutation.nucleotide_), mutation_id_(p_mutation.mutation_id_), tag_value_(p_mutation.tag_value_),
	SLiMEidosDictionary(p_mutation)
{
}

Substitution::Substitution(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_position_t p_position, double p_selection_coeff, slim_objectid_t p_subpop_index, slim_generation_t p_generation, slim_generation_t p_fixation_generation, int8_t p_nucleotide) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(static_cast<slim_selcoeff_t>(p_selection_coeff)), subpop_index_(p_subpop_index), origin_generation_(p_generation), fixation_generation_(p_fixation_generation), nucleotide_(p_nucleotide), mutation_id_(p_mutation_id), tag_value_(SLIM_TAG_UNSET_VALUE)
{
}

void Substitution::PrintForSLiMOutput(std::ostream &p_out) const
{ 
	p_out << mutation_id_ << " m" << mutation_type_ptr_->mutation_type_id_ << " " << position_ << " " << selection_coeff_ << " " << mutation_type_ptr_->dominance_coeff_ << " p" << subpop_index_ << " " << origin_generation_ << " "<< fixation_generation_;
	
	// output a nucleotide if available
	static const char nuc_chars[4] = {'A', 'C', 'G', 'T'};
	
	if (mutation_type_ptr_->nucleotide_based_)
		p_out << " " << nuc_chars[nucleotide_];
	
	p_out << std::endl;
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosObjectClass *Substitution::Class(void) const
{
	return gSLiM_Substitution_Class;
}

void Substitution::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType() << "<" << mutation_id_ << ":" << selection_coeff_ << ">";
}

EidosValue_SP Substitution::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_id:					// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(mutation_id_));
		case gID_mutationType:			// ACCELERATED
			return mutation_type_ptr_->SymbolTableEntry().second;
		case gID_position:				// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(position_));
		case gID_selectionCoeff:		// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(selection_coeff_));
		case gID_originGeneration:		// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(origin_generation_));
		case gID_fixationGeneration:	// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(fixation_generation_));
			
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
			}
		}
		case gID_subpopID:				// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(subpop_index_));
		case gID_tag:					// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Substitution::GetProperty): property tag accessed on substitution before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

EidosValue *Substitution::GetProperty_Accelerated_id(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->mutation_id_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_nucleotide(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_String_vector *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector())->Reserve((int)p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		int8_t nucleotide = value->nucleotide_;
		
		if (nucleotide == -1)
			EIDOS_TERMINATION << "ERROR (Substitution::GetProperty_Accelerated_nucleotideValue): property nucleotide is only defined for nucleotide-based mutations." << EidosTerminate();
		
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

EidosValue *Substitution::GetProperty_Accelerated_nucleotideValue(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
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

EidosValue *Substitution::GetProperty_Accelerated_originGeneration(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->origin_generation_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_fixationGeneration(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->fixation_generation_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_position(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->position_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_subpopID(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->subpop_index_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (Substitution::GetProperty): property tag accessed on substitution before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_selectionCoeff(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->selection_coeff_, value_index);
	}
	
	return float_result;
}

EidosValue *Substitution::GetProperty_Accelerated_mutationType(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_MutationType_Class))->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		object_result->set_object_element_no_check(value->mutation_type_ptr_, value_index);
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
			std::string nucleotide = p_value.StringAtIndex(0, nullptr);
			
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
			int64_t nucleotide = p_value.IntAtIndex(0, nullptr);
			
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Substitution::SetProperty): property nucleotideValue is only defined for nucleotide-based substitutions." << EidosTerminate();
			if ((nucleotide < 0) || (nucleotide > 3))
				EIDOS_TERMINATION << "ERROR (Substitution::SetProperty): property nucleotideValue may only be set to 0 (A), 1 (C), 2 (G), or 3 (T)." << EidosTerminate();
			
			nucleotide_ = (int8_t)nucleotide;
			return;
		}
		case gID_subpopID:
		{
			slim_objectid_t value = SLiMCastToObjectidTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			subpop_index_ = value;
			return;
		}
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
		
		default:
		{
			return EidosObjectElement::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue_SP Substitution::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		default:					return SLiMEidosDictionary::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}


//
//	Substitution_Class
//
#pragma mark -
#pragma mark Substitution_Class
#pragma mark -

class Substitution_Class : public SLiMEidosDictionary_Class
{
public:
	Substitution_Class(const Substitution_Class &p_original) = delete;	// no copy-construct
	Substitution_Class& operator=(const Substitution_Class&) = delete;	// no copying
	inline Substitution_Class(void) { }
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
};

EidosObjectClass *gSLiM_Substitution_Class = new Substitution_Class();


const std::string &Substitution_Class::ElementType(void) const
{
	return gStr_Substitution;
}

const std::vector<const EidosPropertySignature *> *Substitution_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*SLiMEidosDictionary_Class::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,					true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_id));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationType,		true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_MutationType_Class))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_mutationType));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_position,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_position));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_selectionCoeff,		true,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_selectionCoeff));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopID,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_subpopID));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotide,			false,	kEidosValueMaskString | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_nucleotide));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotideValue,	false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_nucleotideValue));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_originGeneration,	true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_originGeneration));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_fixationGeneration,	true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_fixationGeneration));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_tag));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<const EidosMethodSignature *> *Substitution_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*SLiMEidosDictionary_Class::Methods());
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}







































































