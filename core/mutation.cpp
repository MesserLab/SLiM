//
//  mutation.cpp
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


#include "mutation.h"


#ifdef SLIMGUI
// A global counter used to assign all Mutation objects a unique ID in SLiMgui
uint64_t g_next_mutation_id = 0;
#endif

#ifdef SLIMGUI
// In SLiMgui, the mutation_id_ gets initialized here, from the global counter g_next_mutation_id
Mutation::Mutation(SLIMCONST MutationType *p_mutation_type_ptr, int p_position, double p_selection_coeff, int p_subpop_index, int p_generation) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(static_cast<typeof(selection_coeff_)>(p_selection_coeff)), subpop_index_(p_subpop_index), generation_(p_generation), mutation_id_(g_next_mutation_id++)
#else
Mutation::Mutation(SLIMCONST MutationType *p_mutation_type_ptr, int p_position, double p_selection_coeff, int p_subpop_index, int p_generation) :
	mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(static_cast<typeof(selection_coeff_)>(p_selection_coeff)), subpop_index_(p_subpop_index), generation_(p_generation)
#endif
{
#if DEBUG_MUTATIONS
	SLIM_OUTSTREAM << "Mutation constructed: " << this << std::endl;
#endif
}

#if DEBUG_MUTATIONS
Mutation::~Mutation()
{
	SLIM_OUTSTREAM << "Mutation destructed: " << this << std::endl;
}
#endif

std::ostream &operator<<(std::ostream &p_outstream, const Mutation &p_mutation)
{
	p_outstream << "Mutation{mutation_type_ " << p_mutation.mutation_type_ptr_->mutation_type_id_ << ", position_ " << p_mutation.position_ << ", selection_coeff_ " << p_mutation.selection_coeff_ << ", subpop_index_ " << p_mutation.subpop_index_ << ", generation_ " << p_mutation.generation_;
	
	return p_outstream;
}

#ifndef SLIMCORE
//
// SLiMscript support
//
std::string Mutation::ElementType(void) const
{
	return "Mutation";
}

std::vector<std::string> Mutation::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back("mutationType");		// mutation_type_ptr_
	constants.push_back("position");			// position_
	constants.push_back("subpopID");			// subpop_index_
	constants.push_back("originGeneration");	// generation_
	
	return constants;
}

std::vector<std::string> Mutation::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	variables.push_back("selectionCoeff");		// selection_coeff_
	
	return variables;
}

ScriptValue *Mutation::GetValueForMember(const std::string &p_member_name)
{
	// constants
	if (p_member_name.compare("mutationType") == 0)
		return new ScriptValue_Object(mutation_type_ptr_);
	if (p_member_name.compare("position") == 0)
		return new ScriptValue_Int(position_);
	if (p_member_name.compare("subpopID") == 0)
		return new ScriptValue_Int(subpop_index_);
	if (p_member_name.compare("originGeneration") == 0)
		return new ScriptValue_Int(generation_);
	
	// variables
	if (p_member_name.compare("selectionCoeff") == 0)
		return new ScriptValue_Float(selection_coeff_);
	
	return ScriptObjectElement::GetValueForMember(p_member_name);
}

void Mutation::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	if (p_member_name.compare("selectionCoeff") == 0)
	{
		TypeCheckValue(__func__, p_member_name, p_value, kScriptValueMaskInt | kScriptValueMaskFloat);
		
		double value = p_value->FloatAtIndex(0);
		
		selection_coeff_ = static_cast<typeof(selection_coeff_)>(value);	// float, at present, but I don't want to hard-code that
		return;
	}
	
	return ScriptObjectElement::SetValueForMember(p_member_name, p_value);
}

std::vector<std::string> Mutation::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	return methods;
}

const FunctionSignature *Mutation::SignatureForMethod(std::string const &p_method_name) const
{
	return ScriptObjectElement::SignatureForMethod(p_method_name);
}

ScriptValue *Mutation::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	return ScriptObjectElement::ExecuteMethod(p_method_name, p_arguments, p_output_stream, p_interpreter);
}

#endif	// #ifndef SLIMCORE


































































