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
#include "script_functionsignature.h"


#ifdef SLIMGUI
// A global counter used to assign all Mutation objects a unique ID in SLiMgui
uint64_t g_next_mutation_id = 0;
#endif

#ifdef SLIMGUI
// In SLiMgui, the mutation_id_ gets initialized here, from the global counter g_next_mutation_id
Mutation::Mutation(MutationType *p_mutation_type_ptr, int p_position, double p_selection_coeff, int p_subpop_index, int p_generation) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(static_cast<typeof(selection_coeff_)>(p_selection_coeff)), subpop_index_(p_subpop_index), generation_(p_generation), mutation_id_(g_next_mutation_id++)
#else
Mutation::Mutation(MutationType *p_mutation_type_ptr, int p_position, double p_selection_coeff, int p_subpop_index, int p_generation) :
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

//
// SLiMscript support
//
std::string Mutation::ElementType(void) const
{
	return gStr_Mutation;
}

void Mutation::Print(std::ostream &p_ostream) const
{
	p_ostream << ElementType() << "<" << selection_coeff_ << ">";
}

std::vector<std::string> Mutation::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back(gStr_mutationType);		// mutation_type_ptr_
	constants.push_back(gStr_originGeneration);	// generation_
	constants.push_back(gStr_position);			// position_
	constants.push_back(gStr_selectionCoeff);		// selection_coeff_
	constants.push_back(gStr_subpopID);			// subpop_index_
	
	return constants;
}

std::vector<std::string> Mutation::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	return variables;
}

ScriptValue *Mutation::GetValueForMember(const std::string &p_member_name)
{
	// constants
	if (p_member_name.compare(gStr_mutationType) == 0)
		return mutation_type_ptr_->CachedSymbolTableEntry()->second;
	if (p_member_name.compare(gStr_originGeneration) == 0)
		return new ScriptValue_Int(generation_);
	if (p_member_name.compare(gStr_position) == 0)
		return new ScriptValue_Int(position_);
	if (p_member_name.compare(gStr_selectionCoeff) == 0)
		return new ScriptValue_Float(selection_coeff_);
	if (p_member_name.compare(gStr_subpopID) == 0)
		return new ScriptValue_Int(subpop_index_);
	
	// variables
	
	return ScriptObjectElement::GetValueForMember(p_member_name);
}

void Mutation::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	return ScriptObjectElement::SetValueForMember(p_member_name, p_value);
}

std::vector<std::string> Mutation::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	methods.push_back(gStr_setSelectionCoeff);
	
	return methods;
}

const FunctionSignature *Mutation::SignatureForMethod(std::string const &p_method_name) const
{
	static FunctionSignature *setSelectionCoeffSig = nullptr;
	
	if (!setSelectionCoeffSig)
	{
		setSelectionCoeffSig = (new FunctionSignature(gStr_setSelectionCoeff, FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddFloat_S();
	}
	
	if (p_method_name.compare(gStr_setSelectionCoeff) == 0)
		return setSelectionCoeffSig;
	else
		return ScriptObjectElement::SignatureForMethod(p_method_name);
}

ScriptValue *Mutation::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, ScriptInterpreter &p_interpreter)
{
	int num_arguments = (int)p_arguments.size();
	ScriptValue *arg0_value = ((num_arguments >= 1) ? p_arguments[0] : nullptr);
	
	//
	//	*********************	- (void)setSelectionCoeff(float$ selectionCoeff)
	//
#pragma mark -setSelectionCoeff()
	
	if (p_method_name.compare(gStr_setSelectionCoeff) == 0)
	{
		double value = arg0_value->FloatAtIndex(0);
		
		selection_coeff_ = static_cast<typeof(selection_coeff_)>(value);	// float, at present, but I don't want to hard-code that
		
		return gStaticScriptValueNULLInvisible;
	}
	
	
	else
		return ScriptObjectElement::ExecuteMethod(p_method_name, p_arguments, p_interpreter);
}



































































