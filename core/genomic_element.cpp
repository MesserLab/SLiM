//
//  genomic_element.cpp
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


#include "genomic_element.h"
#include "slim_global.h"
#include "slim_sim.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#include <algorithm>
#include <string>
#include <vector>


#pragma mark -
#pragma mark GenomicElement
#pragma mark -

bool GenomicElement::s_log_copy_and_assign_ = true;


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
//	Methods to enforce limited copying
//

GenomicElement::GenomicElement(const GenomicElement& p_original)
{
	if (s_log_copy_and_assign_)
	{
		SLIM_ERRSTREAM << "********* GenomicElement::GenomicElement(GenomicElement&) called!" << std::endl;
		Eidos_PrintStacktrace(stderr);
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
		Eidos_PrintStacktrace(stderr);
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
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosObjectClass *GenomicElement::Class(void) const
{
	return gSLiM_GenomicElement_Class;
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
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(start_position_));
		case gID_endPosition:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(end_position_));
			
			// variables
		case gID_tag:					// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value_));
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

int64_t GenomicElement::GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_startPosition:			return start_position_;
		case gID_endPosition:			return end_position_;
		case gID_tag:					return tag_value_;
			
		default:						return EidosObjectElement::GetProperty_Accelerated_Int(p_property_id);
	}
}

EidosObjectElement *GenomicElement::GetProperty_Accelerated_ObjectElement(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_genomicElementType:	return genomic_element_type_ptr_;
			
		default:						return EidosObjectElement::GetProperty_Accelerated_ObjectElement(p_property_id);
	}
}

void GenomicElement::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	switch (p_property_id)
	{
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

EidosValue_SP GenomicElement::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_setGenomicElementType: return ExecuteMethod_setGenomicElementType(p_method_id, p_arguments, p_argument_count, p_interpreter);
		default:						return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}

//	*********************	- (void)setGenomicElementType(io<GenomicElementType>$ genomicElementType)
//
EidosValue_SP GenomicElement::ExecuteMethod_setGenomicElementType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *genomicElementType_value = p_arguments[0].get();
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	GenomicElementType *getype_ptr = SLiM_ExtractGenomicElementTypeFromEidosValue_io(genomicElementType_value, 0, sim, "setGenomicElementType()");
	
	genomic_element_type_ptr_ = getype_ptr;
	
	return gStaticEidosValueNULLInvisible;
}


//
//	GenomicElement_Class
//
#pragma mark -
#pragma mark GenomicElement_Class
#pragma mark -

class GenomicElement_Class : public EidosObjectClass
{
public:
	GenomicElement_Class(const GenomicElement_Class &p_original) = delete;	// no copy-construct
	GenomicElement_Class& operator=(const GenomicElement_Class&) = delete;	// no copying
	
	GenomicElement_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_GenomicElement_Class = new GenomicElement_Class();


GenomicElement_Class::GenomicElement_Class(void)
{
}

const std::string &GenomicElement_Class::ElementType(void) const
{
	return gStr_GenomicElement;
}

const std::vector<const EidosPropertySignature *> *GenomicElement_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->emplace_back(SignatureForPropertyOrRaise(gID_genomicElementType));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_startPosition));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_endPosition));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *GenomicElement_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *genomicElementTypeSig = nullptr;
	static EidosPropertySignature *startPositionSig = nullptr;
	static EidosPropertySignature *endPositionSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!genomicElementTypeSig)
	{
		genomicElementTypeSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_genomicElementType,	gID_genomicElementType,		true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_GenomicElementType_Class))->DeclareAcceleratedGet();
		startPositionSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_startPosition,		gID_startPosition,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		endPositionSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_endPosition,			gID_endPosition,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		tagSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					gID_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
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
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *GenomicElement_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gID_setGenomicElementType));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *GenomicElement_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *setGenomicElementTypeSig = nullptr;
	
	if (!setGenomicElementTypeSig)
	{
		setGenomicElementTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setGenomicElementType, kEidosValueMaskNULL))->AddIntObject_S("genomicElementType", gSLiM_GenomicElementType_Class);
	}
	
	if (p_method_id == gID_setGenomicElementType)
		return setGenomicElementTypeSig;
	else
		return EidosObjectClass::SignatureForMethod(p_method_id);
}

EidosValue_SP GenomicElement_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
}



































































