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
#include "slim_global.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

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
// Eidos support
//
const std::string *Substitution::ElementType(void) const
{
	return &gStr_Substitution;
}

void Substitution::Print(std::ostream &p_ostream) const
{
	p_ostream << *ElementType() << "<" << selection_coeff_ << ">";
}

const std::vector<const EidosPropertySignature *> *Substitution::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectElement::Properties());
		properties->push_back(SignatureForProperty(gID_mutationType));
		properties->push_back(SignatureForProperty(gID_position));
		properties->push_back(SignatureForProperty(gID_selectionCoeff));
		properties->push_back(SignatureForProperty(gID_subpopID));
		properties->push_back(SignatureForProperty(gID_originGeneration));
		properties->push_back(SignatureForProperty(gID_fixationTime));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Substitution::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *mutationTypeSig = nullptr;
	static EidosPropertySignature *positionSig = nullptr;
	static EidosPropertySignature *selectionCoeffSig = nullptr;
	static EidosPropertySignature *subpopIDSig = nullptr;
	static EidosPropertySignature *originGenerationSig = nullptr;
	static EidosPropertySignature *fixationTimeSig = nullptr;
	
	if (!mutationTypeSig)
	{
		mutationTypeSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationType,		gID_mutationType,		true,	kValueMaskObject | kValueMaskSingleton, &gStr_MutationType));
		positionSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_position,			gID_position,			true,	kValueMaskInt | kValueMaskSingleton));
		selectionCoeffSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_selectionCoeff,		gID_selectionCoeff,		true,	kValueMaskFloat | kValueMaskSingleton));
		subpopIDSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopID,			gID_subpopID,			true,	kValueMaskInt | kValueMaskSingleton));
		originGenerationSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_originGeneration,	gID_originGeneration,	true,	kValueMaskInt | kValueMaskSingleton));
		fixationTimeSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_fixationTime,		gID_fixationTime,		true,	kValueMaskInt | kValueMaskSingleton));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_mutationType:		return mutationTypeSig;
		case gID_position:			return positionSig;
		case gID_selectionCoeff:	return selectionCoeffSig;
		case gID_subpopID:			return subpopIDSig;
		case gID_originGeneration:	return originGenerationSig;
		case gID_fixationTime:		return fixationTimeSig;
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SignatureForProperty(p_property_id);
	}
}

EidosValue *Substitution::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_mutationType:
			return mutation_type_ptr_->CachedSymbolTableEntry()->second;
		case gID_position:
			return new EidosValue_Int_singleton_const(position_);
		case gID_selectionCoeff:
			return new EidosValue_Float_singleton_const(selection_coeff_);
		case gID_subpopID:
			return new EidosValue_Int_singleton_const(subpop_index_);
		case gID_originGeneration:
			return new EidosValue_Int_singleton_const(generation_);
		case gID_fixationTime:
			return new EidosValue_Int_singleton_const(fixation_time_);
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void Substitution::SetProperty(EidosGlobalStringID p_property_id, EidosValue *p_value)
{
	return EidosObjectElement::SetProperty(p_property_id, p_value);
}

const std::vector<const EidosMethodSignature *> *Substitution::Methods(void) const
{
	std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectElement::Methods());
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Substitution::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	return EidosObjectElement::SignatureForMethod(p_method_id);
}

EidosValue *Substitution::ExecuteMethod(EidosGlobalStringID p_method_id, EidosValue *const *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	return EidosObjectElement::ExecuteMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}






































































