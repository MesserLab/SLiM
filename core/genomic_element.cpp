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
		EIDOS_ERRSTREAM << "********* GenomicElement::GenomicElement(GenomicElement&) called!" << std::endl;
		eidos_print_stacktrace(stderr);
		EIDOS_ERRSTREAM << "************************************************" << std::endl;
	}
	
	genomic_element_type_ptr_ = p_original.genomic_element_type_ptr_;
	start_position_ = p_original.start_position_;
	end_position_ = p_original.end_position_;
}

GenomicElement& GenomicElement::operator= (const GenomicElement& p_original)
{
	if (s_log_copy_and_assign_)
	{
		EIDOS_ERRSTREAM << "********* GenomicElement::operator=(GenomicElement&) called!" << std::endl;
		eidos_print_stacktrace(stderr);
		EIDOS_ERRSTREAM << "************************************************" << std::endl;
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

std::vector<std::string> GenomicElement::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = EidosObjectElement::ReadOnlyMembers();
	
	constants.push_back(gStr_genomicElementType);		// genomic_element_type_ptr_
	constants.push_back(gStr_startPosition);			// start_position_
	constants.push_back(gStr_endPosition);				// end_position_
	
	return constants;
}

std::vector<std::string> GenomicElement::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = EidosObjectElement::ReadWriteMembers();
	
	variables.push_back(gStr_tag);						// tag_value_
	
	return variables;
}

bool GenomicElement::MemberIsReadOnly(EidosGlobalStringID p_member_id) const
{
	switch (p_member_id)
	{
			// constants
		case gID_genomicElementType:
		case gID_startPosition:
		case gID_endPosition:
			return true;
			
			// variables
		case gID_tag:
			return false;
			
			// all others, including gID_none
		default:
			return EidosObjectElement::MemberIsReadOnly(p_member_id);
	}
}

EidosValue *GenomicElement::GetValueForMember(EidosGlobalStringID p_member_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_member_id)
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
			return EidosObjectElement::GetValueForMember(p_member_id);
	}
}

void GenomicElement::SetValueForMember(EidosGlobalStringID p_member_id, EidosValue *p_value)
{
	switch (p_member_id)
	{
		case gID_tag:
		{
			TypeCheckValue(__func__, p_member_id, p_value, kValueMaskInt);
			
			int64_t value = p_value->IntAtIndex(0);
			
			tag_value_ = value;
			return;
		}
			
		default:
		{
			return EidosObjectElement::SetValueForMember(p_member_id, p_value);
		}
	}
}

std::vector<std::string> GenomicElement::Methods(void) const
{
	std::vector<std::string> methods = EidosObjectElement::Methods();
	
	methods.push_back(gStr_setGenomicElementType);
	
	return methods;
}

const EidosMethodSignature *GenomicElement::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *setGenomicElementTypeSig = nullptr;
	
	if (!setGenomicElementTypeSig)
	{
		setGenomicElementTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setGenomicElementType, kValueMaskNULL))->AddObject_S("genomicElementType");
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
		GenomicElementType *getype = (GenomicElementType *)(arg0_value->ElementAtIndex(0));
		
		genomic_element_type_ptr_ = getype;
		
		return gStaticEidosValueNULLInvisible;
	}
	
	
	// all others, including gID_none
	else
		return EidosObjectElement::ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}



































































