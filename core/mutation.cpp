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
#include "eidos_property_signature.h"


// All Mutation objects get allocated out of a single shared pool, for speed; see SLiM_WarmUp()
EidosObjectPool *gSLiM_Mutation_Pool = nullptr;


#ifdef SLIMGUI
// A global counter used to assign all Mutation objects a unique ID in SLiMgui
uint64_t g_next_mutation_id = 0;
#endif

#ifdef SLIMGUI
// In SLiMgui, the mutation_id_ gets initialized here, from the global counter g_next_mutation_id
Mutation::Mutation(MutationType *p_mutation_type_ptr, slim_position_t p_position, double p_selection_coeff, slim_objectid_t p_subpop_index, slim_generation_t p_generation) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(static_cast<slim_selcoeff_t>(p_selection_coeff)), subpop_index_(p_subpop_index), generation_(p_generation), mutation_id_(g_next_mutation_id++)
#else
Mutation::Mutation(MutationType *p_mutation_type_ptr, slim_position_t p_position, double p_selection_coeff, slim_objectid_t p_subpop_index, slim_generation_t p_generation) :
	mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(static_cast<slim_selcoeff_t>(p_selection_coeff)), subpop_index_(p_subpop_index), generation_(p_generation)
#endif
{
#if DEBUG_MUTATIONS
	EIDOS_OUTSTREAM << "Mutation constructed: " << this << std::endl;
#endif
}

#if DEBUG_MUTATIONS
Mutation::~Mutation(void)
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
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support

const EidosObjectClass *Mutation::Class(void) const
{
	return gSLiM_Mutation_Class;
}

void Mutation::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType() << "<" << selection_coeff_ << ">";
}

EidosValue_SP Mutation::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_mutationType:
			return mutation_type_ptr_->CachedSymbolTableEntry()->second;
		case gID_originGeneration:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(generation_));
		case gID_position:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(position_));
		case gID_selectionCoeff:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(selection_coeff_));
		case gID_subpopID:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(subpop_index_));
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

void Mutation::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
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
			
		default:
		{
			return EidosObjectElement::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue_SP Mutation::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0].get() : nullptr);
	
	//
	//	*********************	- (void)setSelectionCoeff(float$ selectionCoeff)
	//
#pragma mark -setSelectionCoeff()
	
	if (p_method_id == gID_setSelectionCoeff)
	{
		double value = arg0_value->FloatAtIndex(0, nullptr);
		
		selection_coeff_ = static_cast<slim_selcoeff_t>(value);
		// intentionally no lower or upper bound; -1.0 is lethal, but DFEs may generate smaller values, and we don't want to prevent or bowdlerize that
		// also, the dominance coefficient modifies the selection coefficient, so values < -1 are in fact meaningfully different
		
		return gStaticEidosValueNULLInvisible;
	}
	
	
	// all others, including gID_none
	else
		return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}


//
//	Mutation_Class
//
#pragma mark -
#pragma mark Mutation_Class

class Mutation_Class : public EidosObjectClass
{
public:
	Mutation_Class(const Mutation_Class &p_original) = delete;	// no copy-construct
	Mutation_Class& operator=(const Mutation_Class&) = delete;	// no copying
	
	Mutation_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_Mutation_Class = new Mutation_Class();


Mutation_Class::Mutation_Class(void)
{
}

const std::string &Mutation_Class::ElementType(void) const
{
	return gStr_Mutation;
}

const std::vector<const EidosPropertySignature *> *Mutation_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->push_back(SignatureForPropertyOrRaise(gID_mutationType));
		properties->push_back(SignatureForPropertyOrRaise(gID_originGeneration));
		properties->push_back(SignatureForPropertyOrRaise(gID_position));
		properties->push_back(SignatureForPropertyOrRaise(gID_selectionCoeff));
		properties->push_back(SignatureForPropertyOrRaise(gID_subpopID));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Mutation_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *mutationTypeSig = nullptr;
	static EidosPropertySignature *originGenerationSig = nullptr;
	static EidosPropertySignature *positionSig = nullptr;
	static EidosPropertySignature *selectionCoeffSig = nullptr;
	static EidosPropertySignature *subpopIDSig = nullptr;
	
	if (!mutationTypeSig)
	{
		mutationTypeSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationType,		gID_mutationType,		true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_MutationType_Class));
		originGenerationSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_originGeneration,	gID_originGeneration,	true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		positionSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_position,			gID_position,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton));
		selectionCoeffSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_selectionCoeff,		gID_selectionCoeff,		true,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		subpopIDSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopID,			gID_subpopID,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton));
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_mutationType:		return mutationTypeSig;
		case gID_originGeneration:	return originGenerationSig;
		case gID_position:			return positionSig;
		case gID_selectionCoeff:	return selectionCoeffSig;
		case gID_subpopID:			return subpopIDSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *Mutation_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->push_back(SignatureForMethodOrRaise(gID_setSelectionCoeff));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Mutation_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *setSelectionCoeffSig = nullptr;
	
	if (!setSelectionCoeffSig)
	{
		setSelectionCoeffSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSelectionCoeff, kEidosValueMaskNULL))->AddFloat_S("selectionCoeff");
	}
	
	if (p_method_id == gID_setSelectionCoeff)
		return setSelectionCoeffSig;
	else
		return EidosObjectClass::SignatureForMethod(p_method_id);
}

EidosValue_SP Mutation_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
}

































































