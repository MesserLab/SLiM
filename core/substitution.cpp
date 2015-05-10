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
Substitution::Substitution(SLIMCONST Mutation &p_mutation, int p_fixation_time) :
mutation_type_ptr_(p_mutation.mutation_type_ptr_), position_(p_mutation.position_), selection_coeff_(p_mutation.selection_coeff_), subpop_index_(p_mutation.subpop_index_), generation_(p_mutation.generation_), fixation_time_(p_fixation_time), mutation_id_(p_mutation.mutation_id_)
#else
Substitution::Substitution(SLIMCONST Mutation &p_mutation, int p_fixation_time) :
	mutation_type_ptr_(p_mutation.mutation_type_ptr_), position_(p_mutation.position_), selection_coeff_(p_mutation.selection_coeff_), subpop_index_(p_mutation.subpop_index_), generation_(p_mutation.generation_), fixation_time_(p_fixation_time)
#endif
{
}

void Substitution::print(std::ostream &p_out) const
{ 
	p_out << " m" << mutation_type_ptr_->mutation_type_id_ << " " << position_+1 << " " << selection_coeff_ << " " << mutation_type_ptr_->dominance_coeff_ << " p" << subpop_index_ << " " << generation_ << " "<< fixation_time_ << std::endl; 
}

#ifndef SLIMCORE
//
// SLiMscript support
//
std::string Substitution::ElementType(void) const
{
	return "Substitution";
}

std::vector<std::string> Substitution::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back("mutationType");		// mutation_type_ptr_
	constants.push_back("position");			// position_
	constants.push_back("selectionCoeff");		// selection_coeff_
	constants.push_back("subpopID");			// subpop_index_
	constants.push_back("originGeneration");	// generation_
	constants.push_back("fixationTime");		// fixation_time_
	
	return constants;
}

std::vector<std::string> Substitution::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	return variables;
}

ScriptValue *Substitution::GetValueForMember(const std::string &p_member_name)
{
	// constants
	if (p_member_name.compare("mutationType") == 0)
		return new ScriptValue_Object(mutation_type_ptr_);
	if (p_member_name.compare("position") == 0)
		return new ScriptValue_Int(position_);
	if (p_member_name.compare("selectionCoeff") == 0)
		return new ScriptValue_Float(selection_coeff_);
	if (p_member_name.compare("subpopID") == 0)
		return new ScriptValue_Int(subpop_index_);
	if (p_member_name.compare("originGeneration") == 0)
		return new ScriptValue_Int(generation_);
	if (p_member_name.compare("fixationTime") == 0)
		return new ScriptValue_Int(fixation_time_);
	
	return ScriptObjectElement::GetValueForMember(p_member_name);
}

void Substitution::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	return ScriptObjectElement::SetValueForMember(p_member_name, p_value);
}

std::vector<std::string> Substitution::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	return methods;
}

const FunctionSignature *Substitution::SignatureForMethod(std::string const &p_method_name) const
{
	return ScriptObjectElement::SignatureForMethod(p_method_name);
}

ScriptValue *Substitution::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	return ScriptObjectElement::ExecuteMethod(p_method_name, p_arguments, p_output_stream, p_interpreter);
}

#endif	// #ifndef SLIMCORE





































































