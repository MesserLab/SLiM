//
//  genomic_element.cpp
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


#include "genomic_element.h"
#include "slim_globals.h"
#include "species.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#include <algorithm>
#include <string>
#include <vector>


#pragma mark -
#pragma mark GenomicElement
#pragma mark -


GenomicElement::GenomicElement(GenomicElementType *p_genomic_element_type_ptr, slim_position_t p_start_position, slim_position_t p_end_position) :
	genomic_element_type_ptr_(p_genomic_element_type_ptr), start_position_(p_start_position), end_position_(p_end_position)
{
}

// This is unused except by debugging code and in the debugger itself
std::ostream &operator<<(std::ostream &p_outstream, const GenomicElement &p_genomic_element)
{
	p_outstream << "GenomicElement{genomic_element_type_ g" << p_genomic_element.genomic_element_type_ptr_->genomic_element_type_id_ << ", start_position_ " << p_genomic_element.start_position_ << ", end_position_ " << p_genomic_element.end_position_ << "}";
	
	return p_outstream;
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

void GenomicElement::GenerateCachedEidosValue(void)
{
	// Note that this cache cannot be invalidated as long as a symbol table might exist that this value has been placed into
	self_value_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(this, gSLiM_GenomicElement_Class));
}

const EidosClass *GenomicElement::Class(void) const
{
	return gSLiM_GenomicElement_Class;
}

void GenomicElement::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassNameForDisplay();	// standard EidosObject behavior (not Dictionary behavior)
}

EidosValue_SP GenomicElement::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_genomicElementType:	// ACCELERATED
			return genomic_element_type_ptr_->SymbolTableEntry().second;
		case gID_startPosition:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(start_position_));
		case gID_endPosition:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(end_position_));
			
			// variables
		case gID_tag:					// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (GenomicElement::GetProperty): property tag accessed on genomic element before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(tag_value));
		}
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue *GenomicElement::GetProperty_Accelerated_startPosition(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		GenomicElement *value = (GenomicElement *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->start_position_, value_index);
	}
	
	return int_result;
}

EidosValue *GenomicElement::GetProperty_Accelerated_endPosition(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		GenomicElement *value = (GenomicElement *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->end_position_, value_index);
	}
	
	return int_result;
}

EidosValue *GenomicElement::GetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		GenomicElement *value = (GenomicElement *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (GenomicElement::GetProperty_Accelerated_tag): property tag accessed on genomic element before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

EidosValue *GenomicElement::GetProperty_Accelerated_genomicElementType(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Object *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_GenomicElementType_Class))->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		GenomicElement *value = (GenomicElement *)(p_values[value_index]);
		
		object_result->set_object_element_no_check_NORR(value->genomic_element_type_ptr_, value_index);
	}
	
	return object_result;
}

void GenomicElement::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	switch (p_property_id)
	{
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

EidosValue_SP GenomicElement::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_setGenomicElementType: return ExecuteMethod_setGenomicElementType(p_method_id, p_arguments, p_interpreter);
		default:						return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (void)setGenomicElementType(io<GenomicElementType>$ genomicElementType)
//
EidosValue_SP GenomicElement::ExecuteMethod_setGenomicElementType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *genomicElementType_value = p_arguments[0].get();
	Species &species = genomic_element_type_ptr_->species_;
	GenomicElementType *getype_ptr = SLiM_ExtractGenomicElementTypeFromEidosValue_io(genomicElementType_value, 0, &species.community_, &species, "setGenomicElementType()");		// SPECIES CONSISTENCY CHECK
	
	genomic_element_type_ptr_ = getype_ptr;
	
	return gStaticEidosValueVOID;
}


//
//	GenomicElement_Class
//
#pragma mark -
#pragma mark GenomicElement_Class
#pragma mark -

EidosClass *gSLiM_GenomicElement_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *GenomicElement_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("GenomicElement_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomicElementType,	true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_GenomicElementType_Class))->DeclareAcceleratedGet(GenomicElement::GetProperty_Accelerated_genomicElementType));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_startPosition,		true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(GenomicElement::GetProperty_Accelerated_startPosition));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_endPosition,		true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(GenomicElement::GetProperty_Accelerated_endPosition));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(GenomicElement::GetProperty_Accelerated_tag));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *GenomicElement_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("GenomicElement_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setGenomicElementType, kEidosValueMaskVOID))->AddIntObject_S("genomicElementType", gSLiM_GenomicElementType_Class));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}



































































