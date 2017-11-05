//
//  mutation.cpp
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


#include "mutation.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "slim_sim.h"	// we need to tell the simulation if a selection coefficient is set to non-neutral...

#include <algorithm>
#include <string>
#include <vector>
#include <cstdint>


// All Mutation objects get allocated out of a single shared block, for speed; see SLiM_WarmUp()
Mutation *gSLiM_Mutation_Block = nullptr;
MutationIndex gSLiM_Mutation_Block_Capacity = 0;
MutationIndex gSLiM_Mutation_FreeIndex = -1;
MutationIndex gSLiM_Mutation_Block_LastUsedIndex = -1;

slim_refcount_t *gSLiM_Mutation_Refcounts = nullptr;

#define SLIM_MUTATION_BLOCK_INITIAL_SIZE	16384		// makes for about a 1 MB block; not unreasonable

extern std::vector<EidosValue_Object *> gEidosValue_Object_Mutation_Registry;	// this is in Eidos; see SLiM_IncreaseMutationBlockCapacity()

void SLiM_CreateMutationBlock(void)
{
	// first allocate the block; no need to zero the memory
	gSLiM_Mutation_Block_Capacity = SLIM_MUTATION_BLOCK_INITIAL_SIZE;
	gSLiM_Mutation_Block = (Mutation *)malloc(gSLiM_Mutation_Block_Capacity * sizeof(Mutation));
	gSLiM_Mutation_Refcounts = (slim_refcount_t *)malloc(gSLiM_Mutation_Block_Capacity * sizeof(slim_refcount_t));
	
	//std::cout << "Allocating initial mutation block, " << SLIM_MUTATION_BLOCK_INITIAL_SIZE * sizeof(Mutation) << " bytes (sizeof(Mutation) == " << sizeof(Mutation) << ")" << std::endl;
	
	// now we need to set up our free list inside the block; initially all blocks are free
	for (MutationIndex i = 0; i < gSLiM_Mutation_Block_Capacity - 1; ++i)
		*(MutationIndex *)(gSLiM_Mutation_Block + i) = i + 1;
	
	*(MutationIndex *)(gSLiM_Mutation_Block + gSLiM_Mutation_Block_Capacity - 1) = -1;
	
	// now that the block is set up, we can start the free list
	gSLiM_Mutation_FreeIndex = 0;
}

void SLiM_IncreaseMutationBlockCapacity(void)
{
	if (!gSLiM_Mutation_Block)
		EIDOS_TERMINATION << "ERROR (SLiM_NewMutationFromBlock_realloc): (internal error) called before SLiM_CreateMutationBlock()." << EidosTerminate();
	
	// We need to expand the size of our Mutation block.  This has the consequence of invalidating
	// every Mutation * in the program.  In general that is fine; we are careful to only keep
	// pointers to Mutation temporarily, and for long-term reference we use MutationIndex.  The
	// exception to this is EidosValue_Object; the user can put references to mutations into
	// variables that need to remain valid across reallocs like this.  We therefore have to hunt
	// down every EidosValue_Object that contains Mutations, and fix the pointer inside each of
	// them.  Because in SLiMgui all of the running simulations share a single Mutation block at
	// the moment, in SLiMgui this patching has to occur across all of the simulations, not just
	// the one that made this call.  Yes, this is very gross.  This is why pointers are evil.  :->
	
	// First let's do our realloc.  We just need to note the change in value for the pointer.
	// For now we will just double in size; we don't want to waste too much memory, but we
	// don't want to have to realloc too often, either.
	std::uintptr_t old_mutation_block = reinterpret_cast<std::uintptr_t>(gSLiM_Mutation_Block);
	MutationIndex old_block_capacity = gSLiM_Mutation_Block_Capacity;
	
	gSLiM_Mutation_Block_Capacity *= 2;
	gSLiM_Mutation_Block = (Mutation *)realloc(gSLiM_Mutation_Block, gSLiM_Mutation_Block_Capacity * sizeof(Mutation));
	gSLiM_Mutation_Refcounts = (slim_refcount_t *)realloc(gSLiM_Mutation_Refcounts, gSLiM_Mutation_Block_Capacity * sizeof(slim_refcount_t));
	
	std::uintptr_t new_mutation_block = reinterpret_cast<std::uintptr_t>(gSLiM_Mutation_Block);
	
	// Set up the free list to extend into the new portion of the buffer.  If we are called when
	// gSLiM_Mutation_FreeIndex != -1, the free list will start with the new region.
	for (MutationIndex i = old_block_capacity; i < gSLiM_Mutation_Block_Capacity - 1; ++i)
		*(MutationIndex *)(gSLiM_Mutation_Block + i) = i + 1;
	
	*(MutationIndex *)(gSLiM_Mutation_Block + gSLiM_Mutation_Block_Capacity - 1) = gSLiM_Mutation_FreeIndex;
	
	gSLiM_Mutation_FreeIndex = old_block_capacity;
	
	// Now we go out and fix Mutation * references in EidosValue_Object in all symbol tables
	if (new_mutation_block != old_mutation_block)
	{
		// This may be excessively cautious, but I want to avoid subtracting these uintptr_t values
		// to produce a negative number; that seems unwise and possibly platform-dependent.
		if (old_mutation_block > new_mutation_block)
		{
			std::uintptr_t ptr_diff = old_mutation_block - new_mutation_block;
			
			for (EidosValue_Object *mutation_value : gEidosValue_Object_Mutation_Registry)
				mutation_value->PatchPointersBySubtracting(ptr_diff);
		}
		else
		{
			std::uintptr_t ptr_diff = new_mutation_block - old_mutation_block;
			
			for (EidosValue_Object *mutation_value : gEidosValue_Object_Mutation_Registry)
				mutation_value->PatchPointersByAdding(ptr_diff);
		}
	}
}

void SLiM_ZeroRefcountBlock(__attribute__((unused)) MutationRun &p_mutation_registry)
{
#if 0
	// This version zeros out refcounts just for the mutations currently in use in the registry.
	// It is thus minimal, but probably quite a bit slower than just zeroing out the whole thing.
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
	const MutationIndex *registry_iter = p_mutation_registry.begin_pointer_const();
	const MutationIndex *registry_iter_end = p_mutation_registry.end_pointer_const();
	
	// Do 16 reps
	while (registry_iter + 16 <= registry_iter_end)
	{
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
		*(refcount_block_ptr + (*registry_iter++)) = 0;
	}
	
	// Finish off
	while (registry_iter != registry_iter_end)
		*(refcount_block_ptr + (*registry_iter++)) = 0;
#else
	// Zero out the whole thing with bzero(), without worrying about which bits are in use.
	// This hits more memory, but avoids having to read the registry, and should write whole cache lines.
	bzero(gSLiM_Mutation_Refcounts, (gSLiM_Mutation_Block_LastUsedIndex + 1) * sizeof(slim_refcount_t));
#endif
}


// A global counter used to assign all Mutation objects a unique ID
slim_mutationid_t gSLiM_next_mutation_id = 0;

Mutation::Mutation(MutationType *p_mutation_type_ptr, slim_position_t p_position, double p_selection_coeff, slim_objectid_t p_subpop_index, slim_generation_t p_generation) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(static_cast<slim_selcoeff_t>(p_selection_coeff)), subpop_index_(p_subpop_index), generation_(p_generation), mutation_id_(gSLiM_next_mutation_id++)
{
	// cache values used by the fitness calculation code for speed; see header
	cached_one_plus_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + selection_coeff_);
	cached_one_plus_dom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + mutation_type_ptr_->dominance_coeff_ * selection_coeff_);
	
	// zero out our refcount, which is now kept in a separate buffer
	gSLiM_Mutation_Refcounts[BlockIndex()] = 0;
	
#if DEBUG_MUTATIONS
	SLIM_OUTSTREAM << "Mutation constructed: " << this << std::endl;
#endif
}

Mutation::Mutation(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_position_t p_position, double p_selection_coeff, slim_objectid_t p_subpop_index, slim_generation_t p_generation) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(static_cast<slim_selcoeff_t>(p_selection_coeff)), subpop_index_(p_subpop_index), generation_(p_generation), mutation_id_(p_mutation_id)
{
	// cache values used by the fitness calculation code for speed; see header
	cached_one_plus_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + selection_coeff_);
	cached_one_plus_dom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + mutation_type_ptr_->dominance_coeff_ * selection_coeff_);

	// zero out our refcount, which is now kept in a separate buffer
	gSLiM_Mutation_Refcounts[BlockIndex()] = 0;
	
#if DEBUG_MUTATIONS
	SLIM_OUTSTREAM << "Mutation constructed: " << this << std::endl;
#endif
	
	// Since a mutation id was supplied by the caller, we need to ensure that subsequent mutation ids generated do not collide
	if (gSLiM_next_mutation_id <= mutation_id_)
		gSLiM_next_mutation_id = mutation_id_ + 1;
}

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
	p_ostream << Class()->ElementType() << "<" << mutation_id_ << ":" << selection_coeff_ << ">";
}

EidosValue_SP Mutation::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_id:				// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(mutation_id_));
		case gID_mutationType:		// ACCELERATED
			return mutation_type_ptr_->SymbolTableEntry().second;
		case gID_originGeneration:	// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(generation_));
		case gID_position:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(position_));
		case gID_selectionCoeff:	// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(selection_coeff_));
		case gID_subpopID:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(subpop_index_));
			
			// variables
		case gID_tag:				// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value_));
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

int64_t Mutation::GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_id:				return mutation_id_;
		case gID_originGeneration:	return generation_;
		case gID_position:			return position_;
		case gID_subpopID:			return subpop_index_;
		case gID_tag:				return tag_value_;
			
		default:					return EidosObjectElement::GetProperty_Accelerated_Int(p_property_id);
	}
}

double Mutation::GetProperty_Accelerated_Float(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_selectionCoeff:	return selection_coeff_;
			
		default:					return EidosObjectElement::GetProperty_Accelerated_Float(p_property_id);
	}
}

EidosObjectElement *Mutation::GetProperty_Accelerated_ObjectElement(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_mutationType:		return mutation_type_ptr_;
			
		default:					return EidosObjectElement::GetProperty_Accelerated_ObjectElement(p_property_id);
	}
}

void Mutation::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_subpopID:			// ACCELERATED
		{
			slim_objectid_t value = SLiMCastToObjectidTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			subpop_index_ = value;
			return;
		}
		case gID_tag:				// ACCELERATED
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

void Mutation::SetProperty_Accelerated_Int(EidosGlobalStringID p_property_id, int64_t p_value)
{
	switch (p_property_id)
	{
		case gID_subpopID:		subpop_index_ = SLiMCastToObjectidTypeOrRaise(p_value); return;
			
		case gID_tag:			tag_value_ = p_value; return;		// SLiMCastToUsertagTypeOrRaise() is a no-op at present
			
		default:				return EidosObjectElement::SetProperty_Accelerated_Int(p_property_id, p_value);
	}
}

EidosValue_SP Mutation::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_setSelectionCoeff:	return ExecuteMethod_setSelectionCoeff(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_setMutationType:	return ExecuteMethod_setMutationType(p_method_id, p_arguments, p_argument_count, p_interpreter);
		default:					return SLiMEidosDictionary::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}

//	*********************	- (void)setSelectionCoeff(float$ selectionCoeff)
//
EidosValue_SP Mutation::ExecuteMethod_setSelectionCoeff(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	
	double value = arg0_value->FloatAtIndex(0, nullptr);
	slim_selcoeff_t old_coeff = selection_coeff_;
	
	selection_coeff_ = static_cast<slim_selcoeff_t>(value);
	// intentionally no lower or upper bound; -1.0 is lethal, but DFEs may generate smaller values, and we don't want to prevent or bowdlerize that
	// also, the dominance coefficient modifies the selection coefficient, so values < -1 are in fact meaningfully different
	
	// since this selection coefficient came from the user, check and set pure_neutral_ and all_pure_neutral_DFE_
	if (selection_coeff_ != 0.0)
	{
		SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
		
		sim.pure_neutral_ = false;							// let the sim know that it is no longer a pure-neutral simulation
		mutation_type_ptr_->all_pure_neutral_DFE_ = false;	// let the mutation type for this mutation know that it is no longer pure neutral
		
		// If a selection coefficient has changed from zero to non-zero, or vice versa, MutationRun's nonneutral mutation caches need revalidation
		if (old_coeff == 0.0)
		{
			sim.nonneutral_change_counter_++;
		}
	}
	else if (old_coeff != 0.0)	// && (selection_coeff_ == 0.0) implied by the "else"
	{
		SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
		
		// If a selection coefficient has changed from zero to non-zero, or vice versa, MutationRun's nonneutral mutation caches need revalidation
		sim.nonneutral_change_counter_++;
	}
	
	// cache values used by the fitness calculation code for speed; see header
	cached_one_plus_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + selection_coeff_);
	cached_one_plus_dom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + mutation_type_ptr_->dominance_coeff_ * selection_coeff_);
	
	return gStaticEidosValueNULLInvisible;
}

//	*********************	- (void)setMutationType(io<MutationType>$ mutType)
//
EidosValue_SP Mutation::ExecuteMethod_setMutationType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *arg0_value = p_arguments[0].get();
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(arg0_value, 0, sim, "setMutationType()");
	
	// We take just the mutation type pointer; if the user wants a new selection coefficient, they can do that themselves
	mutation_type_ptr_ = mutation_type_ptr;
	
	// cache values used by the fitness calculation code for speed; see header
	cached_one_plus_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + selection_coeff_);
	cached_one_plus_dom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + mutation_type_ptr_->dominance_coeff_ * selection_coeff_);
	
	return gStaticEidosValueNULLInvisible;
}


//
//	Mutation_Class
//
#pragma mark -
#pragma mark Mutation_Class

class Mutation_Class : public SLiMEidosDictionary_Class
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
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_Mutation_Class = new Mutation_Class();


Mutation_Class::Mutation_Class(void)
{
}

const std::string &Mutation_Class::ElementType(void) const
{
	return gEidosStr_Mutation;		// in Eidos; see EidosValue_Object::EidosValue_Object()
}

const std::vector<const EidosPropertySignature *> *Mutation_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->emplace_back(SignatureForPropertyOrRaise(gID_id));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_mutationType));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_originGeneration));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_position));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_selectionCoeff));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_subpopID));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Mutation_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *idSig = nullptr;
	static EidosPropertySignature *mutationTypeSig = nullptr;
	static EidosPropertySignature *originGenerationSig = nullptr;
	static EidosPropertySignature *positionSig = nullptr;
	static EidosPropertySignature *selectionCoeffSig = nullptr;
	static EidosPropertySignature *subpopIDSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!idSig)
	{
		idSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_id,					gID_id,					true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		mutationTypeSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationType,		gID_mutationType,		true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_MutationType_Class))->DeclareAcceleratedGet();
		originGenerationSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_originGeneration,	gID_originGeneration,	true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		positionSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_position,			gID_position,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		selectionCoeffSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_selectionCoeff,		gID_selectionCoeff,		true,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		subpopIDSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopID,			gID_subpopID,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet()->DeclareAcceleratedSet();
		tagSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					gID_tag,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet()->DeclareAcceleratedSet();
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_id:				return idSig;
		case gID_mutationType:		return mutationTypeSig;
		case gID_originGeneration:	return originGenerationSig;
		case gID_position:			return positionSig;
		case gID_selectionCoeff:	return selectionCoeffSig;
		case gID_subpopID:			return subpopIDSig;
		case gID_tag:				return tagSig;
			
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
		methods = new std::vector<const EidosMethodSignature *>(*SLiMEidosDictionary_Class::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gID_setSelectionCoeff));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setMutationType));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Mutation_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *setSelectionCoeffSig = nullptr;
	static EidosInstanceMethodSignature *setMutationTypeSig = nullptr;
	
	if (!setSelectionCoeffSig)
	{
		setSelectionCoeffSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSelectionCoeff, kEidosValueMaskNULL))->AddFloat_S("selectionCoeff");
		setMutationTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMutationType, kEidosValueMaskNULL))->AddIntObject_S("mutType", gSLiM_MutationType_Class);
	}
	
	if (p_method_id == gID_setSelectionCoeff)
		return setSelectionCoeffSig;
	else if (p_method_id == gID_setMutationType)
		return setMutationTypeSig;
	else
		return SLiMEidosDictionary_Class::SignatureForMethod(p_method_id);
}

EidosValue_SP Mutation_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
}

































































