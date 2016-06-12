//
//  substitution.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2016 Philipp Messer.  All rights reserved.
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


#include "substitution.h"
#include "slim_global.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#include <iostream>
#include <algorithm>
#include <string>
#include <vector>


Substitution::Substitution(Mutation &p_mutation, slim_generation_t p_fixation_generation) :
mutation_type_ptr_(p_mutation.mutation_type_ptr_), position_(p_mutation.position_), selection_coeff_(p_mutation.selection_coeff_), subpop_index_(p_mutation.subpop_index_), generation_(p_mutation.generation_), fixation_generation_(p_fixation_generation), mutation_id_(p_mutation.mutation_id_), tag_value_(p_mutation.tag_value_)
{
}

void Substitution::print(std::ostream &p_out) const
{ 
	p_out << " m" << mutation_type_ptr_->mutation_type_id_ << " " << position_ << " " << selection_coeff_ << " " << mutation_type_ptr_->dominance_coeff_ << " p" << subpop_index_ << " " << generation_ << " "<< fixation_generation_ << std::endl;
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support

const EidosObjectClass *Substitution::Class(void) const
{
	return gSLiM_Substitution_Class;
}

void Substitution::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType() << "<" << mutation_id_ << ":" << selection_coeff_ << ">";
}

EidosValue_SP Substitution::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_id:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(mutation_id_));
		case gID_mutationType:
			return mutation_type_ptr_->SymbolTableEntry().second;
		case gID_position:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(position_));
		case gID_selectionCoeff:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(selection_coeff_));
		case gID_subpopID:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(subpop_index_));
		case gID_originGeneration:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(generation_));
		case gID_fixationGeneration:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(fixation_generation_));
			
			// variables
		case gID_tag:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value_));
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void Substitution::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_subpopID:
		{
			slim_objectid_t value = SLiMCastToObjectidTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			subpop_index_ = value;
			return;
		}
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

EidosValue_SP Substitution::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}


//
//	Substitution_Class
//
#pragma mark -
#pragma mark Substitution_Class

class Substitution_Class : public EidosObjectClass
{
public:
	Substitution_Class(const Substitution_Class &p_original) = delete;	// no copy-construct
	Substitution_Class& operator=(const Substitution_Class&) = delete;	// no copying
	
	Substitution_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_Substitution_Class = new Substitution_Class();


Substitution_Class::Substitution_Class(void)
{
}

const std::string &Substitution_Class::ElementType(void) const
{
	return gStr_Substitution;
}

const std::vector<const EidosPropertySignature *> *Substitution_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->emplace_back(SignatureForPropertyOrRaise(gID_id));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_mutationType));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_position));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_selectionCoeff));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_subpopID));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_originGeneration));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_fixationGeneration));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Substitution_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *idSig = nullptr;
	static EidosPropertySignature *mutationTypeSig = nullptr;
	static EidosPropertySignature *positionSig = nullptr;
	static EidosPropertySignature *selectionCoeffSig = nullptr;
	static EidosPropertySignature *subpopIDSig = nullptr;
	static EidosPropertySignature *originGenerationSig = nullptr;
	static EidosPropertySignature *fixationGenerationSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!idSig)
	{
		idSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_id,					gID_id,					true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		mutationTypeSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationType,		gID_mutationType,		true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_MutationType_Class));
		positionSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_position,			gID_position,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		selectionCoeffSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_selectionCoeff,		gID_selectionCoeff,		true,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		subpopIDSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopID,			gID_subpopID,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		originGenerationSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_originGeneration,	gID_originGeneration,	true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		fixationGenerationSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_fixationGeneration,	gID_fixationGeneration,	true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		tagSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					gID_tag,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_id:					return idSig;
		case gID_mutationType:			return mutationTypeSig;
		case gID_position:				return positionSig;
		case gID_selectionCoeff:		return selectionCoeffSig;
		case gID_subpopID:				return subpopIDSig;
		case gID_originGeneration:		return originGenerationSig;
		case gID_fixationGeneration:	return fixationGenerationSig;
		case gID_tag:					return tagSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *Substitution_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Substitution_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	return EidosObjectClass::SignatureForMethod(p_method_id);
}

EidosValue_SP Substitution_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
}






































































