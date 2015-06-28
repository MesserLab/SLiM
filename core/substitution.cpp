//
//  substitution.cpp
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


#include "substitution.h"

#include <iostream>


#ifdef SLIMGUI
// In SLiMgui, the mutation_id_ gets initialized here, from the mutation
Substitution::Substitution(Mutation &p_mutation, int p_fixation_time) :
mutation_type_ptr_(p_mutation.mutation_type_ptr_), position_(p_mutation.position_), selection_coeff_(p_mutation.selection_coeff_), subpop_index_(p_mutation.subpop_index_), generation_(p_mutation.generation_), fixation_time_(p_fixation_time), mutation_id_(p_mutation.mutation_id_)
#else
Substitution::Substitution(Mutation &p_mutation, int p_fixation_time) :
	mutation_type_ptr_(p_mutation.mutation_type_ptr_), position_(p_mutation.position_), selection_coeff_(p_mutation.selection_coeff_), subpop_index_(p_mutation.subpop_index_), generation_(p_mutation.generation_), fixation_time_(p_fixation_time)
#endif
{
}

void Substitution::print(std::ostream &p_out) const
{ 
	p_out << " m" << mutation_type_ptr_->mutation_type_id_ << " " << position_ << " " << selection_coeff_ << " " << mutation_type_ptr_->dominance_coeff_ << " p" << subpop_index_ << " " << generation_ << " "<< fixation_time_ << std::endl;		// used to have a +1 on position_; switched to zero-based
}

//
// SLiMscript support
//
const std::string *Substitution::ElementType(void) const
{
	return &gStr_Substitution;
}

void Substitution::Print(std::ostream &p_ostream) const
{
	p_ostream << *ElementType() << "<" << selection_coeff_ << ">";
}

std::vector<std::string> Substitution::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back(gStr_mutationType);		// mutation_type_ptr_
	constants.push_back(gStr_position);			// position_
	constants.push_back(gStr_selectionCoeff);		// selection_coeff_
	constants.push_back(gStr_subpopID);			// subpop_index_
	constants.push_back(gStr_originGeneration);	// generation_
	constants.push_back(gStr_fixationTime);		// fixation_time_
	
	return constants;
}

std::vector<std::string> Substitution::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	return variables;
}

bool Substitution::MemberIsReadOnly(GlobalStringID p_member_id) const
{
	switch (p_member_id)
	{
			// constants
		case gID_mutationType:
		case gID_position:
		case gID_selectionCoeff:
		case gID_subpopID:
		case gID_originGeneration:
		case gID_fixationTime:
			return true;
			
			// all others, including gID_none
		default:
			return ScriptObjectElement::MemberIsReadOnly(p_member_id);
	}
}

ScriptValue *Substitution::GetValueForMember(GlobalStringID p_member_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_member_id)
	{
			// constants
		case gID_mutationType:
			return mutation_type_ptr_->CachedSymbolTableEntry()->second;
		case gID_position:
			return new ScriptValue_Int_singleton_const(position_);
		case gID_selectionCoeff:
			return new ScriptValue_Float_singleton_const(selection_coeff_);
		case gID_subpopID:
			return new ScriptValue_Int_singleton_const(subpop_index_);
		case gID_originGeneration:
			return new ScriptValue_Int_singleton_const(generation_);
		case gID_fixationTime:
			return new ScriptValue_Int_singleton_const(fixation_time_);
			
			// all others, including gID_none
		default:
			return ScriptObjectElement::GetValueForMember(p_member_id);
	}
}

void Substitution::SetValueForMember(GlobalStringID p_member_id, ScriptValue *p_value)
{
	return ScriptObjectElement::SetValueForMember(p_member_id, p_value);
}

std::vector<std::string> Substitution::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	return methods;
}

const FunctionSignature *Substitution::SignatureForMethod(GlobalStringID p_method_id) const
{
	return ScriptObjectElement::SignatureForMethod(p_method_id);
}

ScriptValue *Substitution::ExecuteMethod(GlobalStringID p_method_id, ScriptValue *const *const p_arguments, int p_argument_count, ScriptInterpreter &p_interpreter)
{
	return ScriptObjectElement::ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}






































































