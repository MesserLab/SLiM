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
#include "eidos_call_signature.h"


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
	EIDOS_OUTSTREAM << "Mutation constructed: " << this << std::endl;
#endif
}

#if DEBUG_MUTATIONS
Mutation::~Mutation()
{
	EIDOS_OUTSTREAM << "Mutation destructed: " << this << std::endl;
}
#endif

std::ostream &operator<<(std::ostream &p_outstream, const Mutation &p_mutation)
{
	p_outstream << "Mutation{mutation_type_ " << p_mutation.mutation_type_ptr_->mutation_type_id_ << ", position_ " << p_mutation.position_ << ", selection_coeff_ " << p_mutation.selection_coeff_ << ", subpop_index_ " << p_mutation.subpop_index_ << ", generation_ " << p_mutation.generation_;
	
	return p_outstream;
}

//
// Eidos support
//
const std::string *Mutation::ElementType(void) const
{
	return &gStr_Mutation;
}

void Mutation::Print(std::ostream &p_ostream) const
{
	p_ostream << *ElementType() << "<" << selection_coeff_ << ">";
}

std::vector<std::string> Mutation::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = EidosObjectElement::ReadOnlyMembers();
	
	constants.push_back(gStr_mutationType);		// mutation_type_ptr_
	constants.push_back(gStr_originGeneration);	// generation_
	constants.push_back(gStr_position);			// position_
	constants.push_back(gStr_selectionCoeff);		// selection_coeff_
	constants.push_back(gStr_subpopID);			// subpop_index_
	
	return constants;
}

std::vector<std::string> Mutation::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = EidosObjectElement::ReadWriteMembers();
	
	return variables;
}

bool Mutation::MemberIsReadOnly(EidosGlobalStringID p_member_id) const
{
	switch (p_member_id)
	{
			// constants
		case gID_mutationType:
		case gID_originGeneration:
		case gID_position:
		case gID_selectionCoeff:
		case gID_subpopID:
			return true;
			
			// all others, including gID_none
		default:
			return EidosObjectElement::MemberIsReadOnly(p_member_id);
	}
}

EidosValue *Mutation::GetValueForMember(EidosGlobalStringID p_member_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_member_id)
	{
			// constants
		case gID_mutationType:
			return mutation_type_ptr_->CachedSymbolTableEntry()->second;
		case gID_originGeneration:
			return new EidosValue_Int_singleton_const(generation_);
		case gID_position:
			return new EidosValue_Int_singleton_const(position_);
		case gID_selectionCoeff:
			return new EidosValue_Float_singleton_const(selection_coeff_);
		case gID_subpopID:
			return new EidosValue_Int_singleton_const(subpop_index_);
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetValueForMember(p_member_id);
	}
}

void Mutation::SetValueForMember(EidosGlobalStringID p_member_id, EidosValue *p_value)
{
	return EidosObjectElement::SetValueForMember(p_member_id, p_value);
}

std::vector<std::string> Mutation::Methods(void) const
{
	std::vector<std::string> methods = EidosObjectElement::Methods();
	
	methods.push_back(gStr_setSelectionCoeff);
	
	return methods;
}

const EidosMethodSignature *Mutation::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *setSelectionCoeffSig = nullptr;
	
	if (!setSelectionCoeffSig)
	{
		setSelectionCoeffSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSelectionCoeff, kValueMaskNULL))->AddFloat_S("selectionCoeff");
	}
	
	if (p_method_id == gID_setSelectionCoeff)
		return setSelectionCoeffSig;
	else
		return EidosObjectElement::SignatureForMethod(p_method_id);
}

EidosValue *Mutation::ExecuteMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0] : nullptr);
	
	//
	//	*********************	- (void)setSelectionCoeff(float$ selectionCoeff)
	//
#pragma mark -setSelectionCoeff()
	
	if (p_method_id == gID_setSelectionCoeff)
	{
		double value = arg0_value->FloatAtIndex(0);
		
		selection_coeff_ = static_cast<typeof(selection_coeff_)>(value);	// float, at present, but I don't want to hard-code that
		
		return gStaticEidosValueNULLInvisible;
	}
	
	
	// all others, including gID_none
	else
		return EidosObjectElement::ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}



































































