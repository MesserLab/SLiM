//
//  genomic_element.cpp
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


#include "genomic_element.h"
#include "slim_global.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"


bool GenomicElement::s_log_copy_and_assign_ = true;


GenomicElement::GenomicElement(GenomicElementType *p_genomic_element_type_ptr, int p_start_position, int p_end_position) :
	genomic_element_type_ptr_(p_genomic_element_type_ptr), start_position_(p_start_position), end_position_(p_end_position)
{
}

std::ostream &operator<<(std::ostream &p_outstream, const GenomicElement &p_genomic_element)
{
	p_outstream << "GenomicElement{genomic_element_type_ g" << p_genomic_element.genomic_element_type_ptr_->genomic_element_type_id_ << ", start_position_ " << p_genomic_element.start_position_ << ", end_position_ " << p_genomic_element.end_position_ << "}";
	
	return p_outstream;
}


//
//	Methods to enforce limited copying
//

GenomicElement::GenomicElement(const GenomicElement& p_original)
{
	if (s_log_copy_and_assign_)
	{
		SLIM_ERRSTREAM << "********* GenomicElement::GenomicElement(GenomicElement&) called!" << std::endl;
		eidos_print_stacktrace(stderr);
		SLIM_ERRSTREAM << "************************************************" << std::endl;
	}
	
	genomic_element_type_ptr_ = p_original.genomic_element_type_ptr_;
	start_position_ = p_original.start_position_;
	end_position_ = p_original.end_position_;
}

GenomicElement& GenomicElement::operator= (const GenomicElement& p_original)
{
	if (s_log_copy_and_assign_)
	{
		SLIM_ERRSTREAM << "********* GenomicElement::operator=(GenomicElement&) called!" << std::endl;
		eidos_print_stacktrace(stderr);
		SLIM_ERRSTREAM << "************************************************" << std::endl;
	}
	
	genomic_element_type_ptr_ = p_original.genomic_element_type_ptr_;
	start_position_ = p_original.start_position_;
	end_position_ = p_original.end_position_;
	
	return *this;
}

bool GenomicElement::LogGenomicElementCopyAndAssign(bool p_log)
{
	bool old_value = s_log_copy_and_assign_;
	
	s_log_copy_and_assign_ = p_log;
	
	return old_value;
}


//
// Eidos support
//
const std::string *GenomicElement::ElementType(void) const
{
	return &gStr_GenomicElement;
}

const std::vector<const EidosPropertySignature *> *GenomicElement::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectElement::Properties());
		properties->push_back(SignatureForProperty(gID_genomicElementType));
		properties->push_back(SignatureForProperty(gID_startPosition));
		properties->push_back(SignatureForProperty(gID_endPosition));
		properties->push_back(SignatureForProperty(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *GenomicElement::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *genomicElementTypeSig = nullptr;
	static EidosPropertySignature *startPositionSig = nullptr;
	static EidosPropertySignature *endPositionSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!genomicElementTypeSig)
	{
		genomicElementTypeSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_genomicElementType,	gID_genomicElementType,		true,	kEidosValueMaskObject | kEidosValueMaskSingleton, &gStr_GenomicElementType));
		startPositionSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_startPosition,		gID_startPosition,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		endPositionSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_endPosition,			gID_endPosition,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		tagSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					gID_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_genomicElementType:	return genomicElementTypeSig;
		case gID_startPosition:			return startPositionSig;
		case gID_endPosition:			return endPositionSig;
		case gID_tag:					return tagSig;
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SignatureForProperty(p_property_id);
	}
}

EidosValue *GenomicElement::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_genomicElementType:
			return genomic_element_type_ptr_->CachedSymbolTableEntry()->second;
		case gID_startPosition:
			return new EidosValue_Int_singleton_const(start_position_);
		case gID_endPosition:
			return new EidosValue_Int_singleton_const(end_position_);
			
			// variables
		case gID_tag:
			return new EidosValue_Int_singleton_const(tag_value_);
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void GenomicElement::SetProperty(EidosGlobalStringID p_property_id, EidosValue *p_value)
{
	switch (p_property_id)
	{
		case gID_tag:
		{
			int64_t value = p_value->IntAtIndex(0);
			
			tag_value_ = value;
			return;
		}
			
		default:
		{
			return EidosObjectElement::SetProperty(p_property_id, p_value);
		}
	}
}

const std::vector<const EidosMethodSignature *> *GenomicElement::Methods(void) const
{
	std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectElement::Methods());
		methods->push_back(SignatureForMethod(gID_setGenomicElementType));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *GenomicElement::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *setGenomicElementTypeSig = nullptr;
	
	if (!setGenomicElementTypeSig)
	{
		setGenomicElementTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setGenomicElementType, kEidosValueMaskNULL))->AddObject_S("genomicElementType", &gStr_GenomicElementType);
	}
	
	if (p_method_id == gID_setGenomicElementType)
		return setGenomicElementTypeSig;
	else
		return EidosObjectElement::SignatureForMethod(p_method_id);
}

EidosValue *GenomicElement::ExecuteMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0] : nullptr);
	
	//
	//	*********************	- (void)setGenomicElementType(object$ genomicElementType)
	//
#pragma mark -setGenomicElementType()
	
	if (p_method_id == gID_setGenomicElementType)
	{
		GenomicElementType *getype = (GenomicElementType *)(arg0_value->ObjectElementAtIndex(0));
		
		genomic_element_type_ptr_ = getype;
		
		return gStaticEidosValueNULLInvisible;
	}
	
	
	// all others, including gID_none
	else
		return EidosObjectElement::ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}



































































