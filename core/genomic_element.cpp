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
#include "script_functionsignature.h"


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
		print_stacktrace(stderr);
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
		print_stacktrace(stderr);
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
// SLiMscript support
//
std::string GenomicElement::ElementType(void) const
{
	return gStr_GenomicElement;
}

std::vector<std::string> GenomicElement::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back(gStr_genomicElementType);		// genomic_element_type_ptr_
	constants.push_back(gStr_startPosition);			// start_position_
	constants.push_back(gStr_endPosition);				// end_position_
	
	return constants;
}

std::vector<std::string> GenomicElement::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	return variables;
}

ScriptValue *GenomicElement::GetValueForMember(const std::string &p_member_name)
{
	// constants
	if (p_member_name.compare(gStr_genomicElementType) == 0)
		return genomic_element_type_ptr_->CachedSymbolTableEntry()->second;
	if (p_member_name.compare(gStr_startPosition) == 0)
		return new ScriptValue_Int_singleton_const(start_position_);
	if (p_member_name.compare(gStr_endPosition) == 0)
		return new ScriptValue_Int_singleton_const(end_position_);
	
	return ScriptObjectElement::GetValueForMember(p_member_name);
}

void GenomicElement::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	return ScriptObjectElement::SetValueForMember(p_member_name, p_value);
}

std::vector<std::string> GenomicElement::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	methods.push_back(gStr_changeGenomicElementType);
	
	return methods;
}

const FunctionSignature *GenomicElement::SignatureForMethod(const std::string &p_method_name) const
{
	static FunctionSignature *changeGenomicElementTypeSig = nullptr;
	
	if (!changeGenomicElementTypeSig)
	{
		changeGenomicElementTypeSig = (new FunctionSignature(gStr_changeGenomicElementType, FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddObject_S();
	}
	
	if (p_method_name.compare(gStr_changeGenomicElementType) == 0)
		return changeGenomicElementTypeSig;
	else
		return ScriptObjectElement::SignatureForMethod(p_method_name);
}

ScriptValue *GenomicElement::ExecuteMethod(const std::string &p_method_name, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter)
{
	ScriptValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0] : nullptr);
	
	//
	//	*********************	- (void)changeGenomicElementType(object$ genomicElementType)
	//
#pragma mark -changeGenomicElementType()
	
	if (p_method_name.compare(gStr_changeGenomicElementType) == 0)
	{
		GenomicElementType *getype = (GenomicElementType *)(arg0_value->ElementAtIndex(0));
		
		genomic_element_type_ptr_ = getype;
		
		return gStaticScriptValueNULLInvisible;
	}
	
	
	else
		return ScriptObjectElement::ExecuteMethod(p_method_name, p_arguments, p_argument_count, p_interpreter);
}



































































