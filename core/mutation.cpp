//
//  mutation.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2025 Benjamin C. Haller.  All rights reserved.
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
#include "species.h"

#include <algorithm>
#include <string>
#include <vector>
#include <cstdint>
#include <csignal>


// All Mutation objects get allocated out of a single shared block, for speed; see SLiM_WarmUp()
// Note this is shared by all species; the mutations for every species come out of the same shared block.
Mutation *gSLiM_Mutation_Block = nullptr;
MutationIndex gSLiM_Mutation_Block_Capacity = 0;
MutationIndex gSLiM_Mutation_FreeIndex = -1;
MutationIndex gSLiM_Mutation_Block_LastUsedIndex = -1;

#ifdef DEBUG_LOCKS_ENABLED
EidosDebugLock gSLiM_Mutation_LOCK("gSLiM_Mutation_LOCK");
#endif

slim_refcount_t *gSLiM_Mutation_Refcounts = nullptr;

#define SLIM_MUTATION_BLOCK_INITIAL_SIZE	16384		// makes for about a 1 MB block; not unreasonable		// NOLINT(*-macro-to-enum) : this is fine

extern std::vector<EidosValue_Object *> gEidosValue_Object_Mutation_Registry;	// this is in Eidos; see SLiM_IncreaseMutationBlockCapacity()

void SLiM_CreateMutationBlock(void)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("SLiM_CreateMutationBlock(): gSLiM_Mutation_Block address change");
	
	// first allocate the block; no need to zero the memory
	gSLiM_Mutation_Block_Capacity = SLIM_MUTATION_BLOCK_INITIAL_SIZE;
	gSLiM_Mutation_Block = (Mutation *)malloc(gSLiM_Mutation_Block_Capacity * sizeof(Mutation));
	gSLiM_Mutation_Refcounts = (slim_refcount_t *)malloc(gSLiM_Mutation_Block_Capacity * sizeof(slim_refcount_t));
	
	if (!gSLiM_Mutation_Block || !gSLiM_Mutation_Refcounts)
		EIDOS_TERMINATION << "ERROR (SLiM_CreateMutationBlock): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
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
	// We do not use a THREAD_SAFETY macro here because this needs to be checked in release builds also;
	// we are not able to completely protect against this occurring at runtime, and it corrupts the run.
	// It's OK for this to be called when we're inside an inactive parallel region; there is then no
	// race condition.  When a parallel region is active, even inside a critical region, reallocating
	// the mutation block has the potential for a race with other threads.
	if (omp_in_parallel())
	{
		std::cerr << "ERROR (SLiM_IncreaseMutationBlockCapacity): (internal error) SLiM_IncreaseMutationBlockCapacity() was called to reallocate gSLiM_Mutation_Block inside a parallel section.  If you see this message, you need to increase the pre-allocation margin for your simulation, because it is generating such an unexpectedly large number of new mutations.  Please contact the SLiM developers for guidance on how to do this." << std::endl;
		raise(SIGTRAP);
	}
	
#ifdef DEBUG_LOCKS_ENABLED
	gSLiM_Mutation_LOCK.start_critical(1);
#endif
	
	if (!gSLiM_Mutation_Block)
		EIDOS_TERMINATION << "ERROR (SLiM_IncreaseMutationBlockCapacity): (internal error) called before SLiM_CreateMutationBlock()." << EidosTerminate();
	
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
	// BCH 11 May 2020: the realloc of gSLiM_Mutation_Block is technically problematic, because
	// Mutation is non-trivially copyable according to C++.  But it is safe, so I cast to void*
	// in the hopes that that will make the warning go away.
	std::uintptr_t old_mutation_block = reinterpret_cast<std::uintptr_t>(gSLiM_Mutation_Block);
	MutationIndex old_block_capacity = gSLiM_Mutation_Block_Capacity;
	
	//std::cout << "old capacity: " << old_block_capacity << std::endl;
	
	// BCH 25 July 2023: check for increasing our block beyond the maximum size of 2^31 mutations.
	// See https://github.com/MesserLab/SLiM/issues/361.  Note that the initial size should be
	// a power of 2, so that we actually reach the maximum; see SLIM_MUTATION_BLOCK_INITIAL_SIZE.
	// In other words, we expect to be at exactly 0x0000000040000000UL here, and thus to double
	// to 0x0000000080000000UL, which is a capacity of 2^31, which is the limit of int32_t.
	if ((size_t)old_block_capacity > 0x0000000040000000UL)	// >2^30 means >2^31 when doubled
		EIDOS_TERMINATION << "ERROR (SLiM_IncreaseMutationBlockCapacity): too many mutations; there is a limit of 2^31 (2147483648) segregating mutations in SLiM." << EidosTerminate(nullptr);
	
	gSLiM_Mutation_Block_Capacity *= 2;
	gSLiM_Mutation_Block = (Mutation *)realloc((void*)gSLiM_Mutation_Block, gSLiM_Mutation_Block_Capacity * sizeof(Mutation));						// NOLINT(*-realloc-usage) : realloc failure is a fatal error anyway
	gSLiM_Mutation_Refcounts = (slim_refcount_t *)realloc(gSLiM_Mutation_Refcounts, gSLiM_Mutation_Block_Capacity * sizeof(slim_refcount_t));		// NOLINT(*-realloc-usage) : realloc failure is a fatal error anyway
	
	if (!gSLiM_Mutation_Block || !gSLiM_Mutation_Refcounts)
		EIDOS_TERMINATION << "ERROR (SLiM_IncreaseMutationBlockCapacity): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	//std::cout << "new capacity: " << gSLiM_Mutation_Block_Capacity << std::endl;
	
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
	
#ifdef DEBUG_LOCKS_ENABLED
	gSLiM_Mutation_LOCK.end_critical();
#endif
}

void SLiM_ZeroRefcountBlock(MutationRun &p_mutation_registry, bool p_registry_only)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("SLiM_ZeroRefcountBlock(): gSLiM_Mutation_Block change");
	
#ifdef SLIMGUI
	// BCH 11/25/2017: This code path needs to be used in SLiMgui to avoid modifying the refcounts
	// for mutations in other simulations sharing the mutation block.
	p_registry_only = true;
#endif
	
	if (p_registry_only)
	{
		// This code path zeros out refcounts just for the mutations currently in use in the registry.
		// It is thus minimal, but probably quite a bit slower than just zeroing out the whole thing.
		// BCH 6/8/2023: This is necessary in SLiMgui, as noted above, but also in multispecies sims
		// so that one species does not step on the toes of another species.
		slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
		const MutationIndex *registry_iter = p_mutation_registry.begin_pointer_const();
		const MutationIndex *registry_iter_end = p_mutation_registry.end_pointer_const();
		
		while (registry_iter != registry_iter_end)
			*(refcount_block_ptr + (*registry_iter++)) = 0;
	}
	else
	{
		// Zero out the whole thing with EIDOS_BZERO(), without worrying about which bits are in use.
		// This hits more memory, but avoids having to read the registry, and should write whole cache lines.
		EIDOS_BZERO(gSLiM_Mutation_Refcounts, (gSLiM_Mutation_Block_LastUsedIndex + 1) * sizeof(slim_refcount_t));
	}
}

size_t SLiMMemoryUsageForMutationBlock(void)
{
	return gSLiM_Mutation_Block_Capacity * sizeof(Mutation);
}

size_t SLiMMemoryUsageForFreeMutations(void)
{
	size_t mut_count = 0;
	MutationIndex nextFreeBlock = gSLiM_Mutation_FreeIndex;
	
	while (nextFreeBlock != -1)
	{
		mut_count++;
		nextFreeBlock = *(MutationIndex *)(gSLiM_Mutation_Block + nextFreeBlock);
	}
	
	return mut_count * sizeof(Mutation);
}

size_t SLiMMemoryUsageForMutationRefcounts(void)
{
	return gSLiM_Mutation_Block_Capacity * sizeof(slim_refcount_t);
}


#pragma mark -
#pragma mark Mutation
#pragma mark -

// A global counter used to assign all Mutation objects a unique ID
slim_mutationid_t gSLiM_next_mutation_id = 0;

Mutation::Mutation(MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, double p_selection_coeff, slim_objectid_t p_subpop_index, slim_tick_t p_tick, int8_t p_nucleotide) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(static_cast<slim_selcoeff_t>(p_selection_coeff)), subpop_index_(p_subpop_index), origin_tick_(p_tick), chromosome_index_(p_chromosome_index), state_(MutationState::kNewMutation), nucleotide_(p_nucleotide), mutation_id_(gSLiM_next_mutation_id++)
{
#ifdef DEBUG_LOCKS_ENABLED
	gSLiM_Mutation_LOCK.start_critical(2);
#endif
	
	// initialize the tag to the "unset" value
	tag_value_ = SLIM_TAG_UNSET_VALUE;
	
	// cache values used by the fitness calculation code for speed; see header
	cached_one_plus_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + selection_coeff_);
	cached_one_plus_dom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + mutation_type_ptr_->dominance_coeff_ * selection_coeff_);
	cached_one_plus_hemizygousdom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + mutation_type_ptr_->hemizygous_dominance_coeff_ * selection_coeff_);
	
	// zero out our refcount, which is now kept in a separate buffer
	gSLiM_Mutation_Refcounts[BlockIndex()] = 0;
	
#if DEBUG_MUTATIONS
	std::cout << "Mutation constructed: " << this << std::endl;
#endif
	
#ifdef DEBUG_LOCKS_ENABLED
	gSLiM_Mutation_LOCK.end_critical();
#endif
	
#if 0
	// Dump the memory layout of a Mutation object.  Note this code needs to be synced tightly with the header, since C++ has no real introspection.
	static bool been_here = false;
	
#pragma omp critical (Mutation_layout_dump)
	{
		if (!been_here)
		{
			char *ptr_base = (char *)this;
			char *ptr_mutation_type_ptr_ = (char *)&(this->mutation_type_ptr_);
			char *ptr_position_ = (char *)&(this->position_);
			char *ptr_selection_coeff_ = (char *)&(this->selection_coeff_);
			char *ptr_subpop_index_ = (char *)&(this->subpop_index_);
			char *ptr_origin_tick_ = (char *)&(this->origin_tick_);
			char *ptr_state_ = (char *)&(this->state_);
			char *ptr_nucleotide_ = (char *)&(this->nucleotide_);
			char *ptr_scratch_ = (char *)&(this->scratch_);
			char *ptr_mutation_id_ = (char *)&(this->mutation_id_);
			char *ptr_tag_value_ = (char *)&(this->tag_value_);
			char *ptr_cached_one_plus_sel_ = (char *)&(this->cached_one_plus_sel_);
			char *ptr_cached_one_plus_dom_sel_ = (char *)&(this->cached_one_plus_dom_sel_);
			char *ptr_cached_one_plus_haploiddom_sel_ = (char *)&(this->cached_one_plus_haploiddom_sel_);
			
			std::cout << "Class Mutation memory layout (sizeof(Mutation) == " << sizeof(Mutation) << ") :" << std::endl << std::endl;
			std::cout << "   " << (ptr_mutation_type_ptr_ - ptr_base) << " (" << sizeof(MutationType *) << " bytes): MutationType *mutation_type_ptr_" << std::endl;
			std::cout << "   " << (ptr_position_ - ptr_base) << " (" << sizeof(slim_position_t) << " bytes): const slim_position_t position_" << std::endl;
			std::cout << "   " << (ptr_selection_coeff_ - ptr_base) << " (" << sizeof(slim_selcoeff_t) << " bytes): slim_selcoeff_t selection_coeff_" << std::endl;
			std::cout << "   " << (ptr_subpop_index_ - ptr_base) << " (" << sizeof(slim_objectid_t) << " bytes): slim_objectid_t subpop_index_" << std::endl;
			std::cout << "   " << (ptr_origin_tick_ - ptr_base) << " (" << sizeof(slim_tick_t) << " bytes): const slim_tick_t origin_tick_" << std::endl;
			std::cout << "   " << (ptr_state_ - ptr_base) << " (" << sizeof(int8_t) << " bytes): const int8_t state_" << std::endl;
			std::cout << "   " << (ptr_nucleotide_ - ptr_base) << " (" << sizeof(int8_t) << " bytes): const int8_t nucleotide_" << std::endl;
			std::cout << "   " << (ptr_scratch_ - ptr_base) << " (" << sizeof(int8_t) << " bytes): const int8_t scratch_" << std::endl;
			std::cout << "   " << (ptr_mutation_id_ - ptr_base) << " (" << sizeof(slim_mutationid_t) << " bytes): const slim_mutationid_t mutation_id_" << std::endl;
			std::cout << "   " << (ptr_tag_value_ - ptr_base) << " (" << sizeof(slim_usertag_t) << " bytes): slim_usertag_t tag_value_" << std::endl;
			std::cout << "   " << (ptr_cached_one_plus_sel_ - ptr_base) << " (" << sizeof(slim_selcoeff_t) << " bytes): slim_selcoeff_t cached_one_plus_sel_" << std::endl;
			std::cout << "   " << (ptr_cached_one_plus_dom_sel_ - ptr_base) << " (" << sizeof(slim_selcoeff_t) << " bytes): slim_selcoeff_t cached_one_plus_dom_sel_" << std::endl;
			std::cout << "   " << (ptr_cached_one_plus_haploiddom_sel_ - ptr_base) << " (" << sizeof(slim_selcoeff_t) << " bytes): slim_selcoeff_t cached_one_plus_haploiddom_sel_" << std::endl;
			std::cout << std::endl;
			
			been_here = true;
		}
	}
#endif
}

Mutation::Mutation(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, double p_selection_coeff, slim_objectid_t p_subpop_index, slim_tick_t p_tick, int8_t p_nucleotide) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(static_cast<slim_selcoeff_t>(p_selection_coeff)), subpop_index_(p_subpop_index), origin_tick_(p_tick), chromosome_index_(p_chromosome_index), state_(MutationState::kNewMutation), nucleotide_(p_nucleotide), mutation_id_(p_mutation_id)
{
	// initialize the tag to the "unset" value
	tag_value_ = SLIM_TAG_UNSET_VALUE;
	
	// cache values used by the fitness calculation code for speed; see header
	cached_one_plus_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + selection_coeff_);
	cached_one_plus_dom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + mutation_type_ptr_->dominance_coeff_ * selection_coeff_);
	cached_one_plus_hemizygousdom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + mutation_type_ptr_->hemizygous_dominance_coeff_ * selection_coeff_);
	
	// zero out our refcount, which is now kept in a separate buffer
	gSLiM_Mutation_Refcounts[BlockIndex()] = 0;
	
#if DEBUG_MUTATIONS
	std::cout << "Mutation constructed: " << this << std::endl;
#endif
	
	// Since a mutation id was supplied by the caller, we need to ensure that subsequent mutation ids generated do not collide
	// This constructor (unline the other Mutation() constructor above) is presently never called multithreaded,
	// so we just enforce that here.  If that changes, it should start using the debug lock to detect races, as above.
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Mutation::Mutation(): gSLiM_next_mutation_id change");
	
	if (gSLiM_next_mutation_id <= mutation_id_)
		gSLiM_next_mutation_id = mutation_id_ + 1;
}

void Mutation::SelfDelete(void)
{
	// This is called when our retain count reaches zero
	// We destruct ourselves and return our memory to our shared pool
	MutationIndex mutation_index = BlockIndex();
	
	this->~Mutation();
	SLiM_DisposeMutationToBlock(mutation_index);
}

// This is unused except by debugging code and in the debugger itself
std::ostream &operator<<(std::ostream &p_outstream, const Mutation &p_mutation)
{
	p_outstream << "Mutation{mutation_type_ " << p_mutation.mutation_type_ptr_->mutation_type_id_ << ", position_ " << p_mutation.position_ << ", selection_coeff_ " << p_mutation.selection_coeff_ << ", subpop_index_ " << p_mutation.subpop_index_ << ", origin_tick_ " << p_mutation.origin_tick_;
	
	return p_outstream;
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosClass *Mutation::Class(void) const
{
	return gSLiM_Mutation_Class;
}

void Mutation::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassNameForDisplay() << "<" << mutation_id_ << ":" << selection_coeff_ << ">";
}

EidosValue_SP Mutation::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_chromosome:
		{
			Species &species = mutation_type_ptr_->species_;
			const std::vector<Chromosome *> &chromosomes = species.Chromosomes();
			Chromosome *chromosome = chromosomes[chromosome_index_];
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(chromosome, gSLiM_Chromosome_Class));
		}
		case gID_id:				// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(mutation_id_));
		case gID_isFixed:			// ACCELERATED
			return (((state_ == MutationState::kFixedAndSubstituted) || (state_ == MutationState::kRemovedWithSubstitution)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_isSegregating:		// ACCELERATED
			return ((state_ == MutationState::kInRegistry) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_mutationType:		// ACCELERATED
			return mutation_type_ptr_->SymbolTableEntry().second;
		case gID_originTick:		// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(origin_tick_));
		case gID_position:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(position_));
		case gID_selectionCoeff:	// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(selection_coeff_));
			
			// variables
		case gID_nucleotide:		// ACCELERATED
		{
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Mutation::GetProperty): property nucleotide is only defined for nucleotide-based mutations." << EidosTerminate();
			
			switch (nucleotide_)
			{
				case 0:	return gStaticEidosValue_StringA;
				case 1:	return gStaticEidosValue_StringC;
				case 2:	return gStaticEidosValue_StringG;
				case 3:	return gStaticEidosValue_StringT;
				default:
					EIDOS_TERMINATION << "ERROR (Mutation::GetProperty): (internal error) unrecognized value for nucleotide_." << EidosTerminate();
			}
		}
		case gID_nucleotideValue:	// ACCELERATED
		{
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Mutation::GetProperty): property nucleotideValue is only defined for nucleotide-based mutations." << EidosTerminate();
			
			switch (nucleotide_)
			{
				case 0:	return gStaticEidosValue_Integer0;
				case 1:	return gStaticEidosValue_Integer1;
				case 2:	return gStaticEidosValue_Integer2;
				case 3:	return gStaticEidosValue_Integer3;
				default:
					EIDOS_TERMINATION << "ERROR (Mutation::GetProperty): (internal error) unrecognized value for nucleotide_." << EidosTerminate();
			}
		}
		case gID_subpopID:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(subpop_index_));
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Mutation::GetProperty): property tag accessed on mutation before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(tag_value));
		}
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue *Mutation::GetProperty_Accelerated_id(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->mutation_id_, value_index);
	}
	
	return int_result;
}

EidosValue *Mutation::GetProperty_Accelerated_isFixed(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		logical_result->set_logical_no_check((value->state_ == MutationState::kFixedAndSubstituted) || (value->state_ == MutationState::kRemovedWithSubstitution), value_index);
	}
	
	return logical_result;
}

EidosValue *Mutation::GetProperty_Accelerated_isSegregating(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		logical_result->set_logical_no_check(value->state_ == MutationState::kInRegistry, value_index);
	}
	
	return logical_result;
}

EidosValue *Mutation::GetProperty_Accelerated_nucleotide(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String())->Reserve((int)p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		int8_t nucleotide = value->nucleotide_;
		
		if (nucleotide == -1)
			EIDOS_TERMINATION << "ERROR (Mutation::GetProperty_Accelerated_nucleotide): property nucleotide is only defined for nucleotide-based mutations." << EidosTerminate();
		
		if (nucleotide == 0)
			string_result->PushString(gStr_A);
		else if (nucleotide == 1)
			string_result->PushString(gStr_C);
		else if (nucleotide == 2)
			string_result->PushString(gStr_G);
		else if (nucleotide == 3)
			string_result->PushString(gStr_T);
	}
	
	return string_result;
}

EidosValue *Mutation::GetProperty_Accelerated_nucleotideValue(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		int8_t nucleotide = value->nucleotide_;
		
		if (nucleotide == -1)
			EIDOS_TERMINATION << "ERROR (Mutation::GetProperty_Accelerated_nucleotideValue): property nucleotideValue is only defined for nucleotide-based mutations." << EidosTerminate();
		
		int_result->set_int_no_check(nucleotide, value_index);
	}
	
	return int_result;
}

EidosValue *Mutation::GetProperty_Accelerated_originTick(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->origin_tick_, value_index);
	}
	
	return int_result;
}

EidosValue *Mutation::GetProperty_Accelerated_position(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->position_, value_index);
	}
	
	return int_result;
}

EidosValue *Mutation::GetProperty_Accelerated_subpopID(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->subpop_index_, value_index);
	}
	
	return int_result;
}

EidosValue *Mutation::GetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (Mutation::GetProperty_Accelerated_tag): property tag accessed on mutation before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

EidosValue *Mutation::GetProperty_Accelerated_selectionCoeff(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->selection_coeff_, value_index);
	}
	
	return float_result;
}

EidosValue *Mutation::GetProperty_Accelerated_mutationType(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Object *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_MutationType_Class))->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		object_result->set_object_element_no_check_NORR(value->mutation_type_ptr_, value_index);
	}
	
	return object_result;
}

void Mutation::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_nucleotide:
		{
			const std::string &nucleotide = ((EidosValue_String &)p_value).StringRefAtIndex_NOCAST(0, nullptr);
			
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Mutation::SetProperty): property nucleotide is only defined for nucleotide-based mutations." << EidosTerminate();
			
			if (nucleotide == gStr_A)		nucleotide_ = 0;
			else if (nucleotide == gStr_C)	nucleotide_ = 1;
			else if (nucleotide == gStr_G)	nucleotide_ = 2;
			else if (nucleotide == gStr_T)	nucleotide_ = 3;
			else EIDOS_TERMINATION << "ERROR (Mutation::SetProperty): property nucleotide may only be set to 'A', 'C', 'G', or 'T'." << EidosTerminate();
			return;
		}
		case gID_nucleotideValue:
		{
			int64_t nucleotide = p_value.IntAtIndex_NOCAST(0, nullptr);
			
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Mutation::SetProperty): property nucleotideValue is only defined for nucleotide-based mutations." << EidosTerminate();
			if ((nucleotide < 0) || (nucleotide > 3))
				EIDOS_TERMINATION << "ERROR (Mutation::SetProperty): property nucleotideValue may only be set to 0 (A), 1 (C), 2 (G), or 3 (T)." << EidosTerminate();
			
			nucleotide_ = (int8_t)nucleotide;
			return;
		}
		case gID_subpopID:			// ACCELERATED
		{
			slim_objectid_t value = SLiMCastToObjectidTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			subpop_index_ = value;
			return;
		}
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
		default:
		{
			return super::SetProperty(p_property_id, p_value);
		}
	}
}

void Mutation::SetProperty_Accelerated_subpopID(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex_NOCAST(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Mutation *)(p_values[value_index]))->subpop_index_ = SLiMCastToObjectidTypeOrRaise(source_value);
	}
	else
	{
		const int64_t *source_data = p_source.IntData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Mutation *)(p_values[value_index]))->subpop_index_ = SLiMCastToObjectidTypeOrRaise(source_data[value_index]);
	}
}

void Mutation::SetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	// SLiMCastToUsertagTypeOrRaise() is a no-op at present
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex_NOCAST(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Mutation *)(p_values[value_index]))->tag_value_ = source_value;
	}
	else
	{
		const int64_t *source_data = p_source.IntData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Mutation *)(p_values[value_index]))->tag_value_ = source_data[value_index];
	}
}

EidosValue_SP Mutation::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_setSelectionCoeff:	return ExecuteMethod_setSelectionCoeff(p_method_id, p_arguments, p_interpreter);
		case gID_setMutationType:	return ExecuteMethod_setMutationType(p_method_id, p_arguments, p_interpreter);
		default:					return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (void)setSelectionCoeff(float$ selectionCoeff)
//
EidosValue_SP Mutation::ExecuteMethod_setSelectionCoeff(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *selectionCoeff_value = p_arguments[0].get();
	
	double value = selectionCoeff_value->FloatAtIndex_NOCAST(0, nullptr);
	slim_selcoeff_t old_coeff = selection_coeff_;
	
	selection_coeff_ = static_cast<slim_selcoeff_t>(value);
	// intentionally no lower or upper bound; -1.0 is lethal, but DFEs may generate smaller values, and we don't want to prevent or bowdlerize that
	// also, the dominance coefficient modifies the selection coefficient, so values < -1 are in fact meaningfully different
	
	// since this selection coefficient came from the user, check and set pure_neutral_ and all_pure_neutral_DFE_
	if (selection_coeff_ != 0.0)
	{
		Species &species = mutation_type_ptr_->species_;
		
		species.pure_neutral_ = false;							// let the sim know that it is no longer a pure-neutral simulation
		mutation_type_ptr_->all_pure_neutral_DFE_ = false;	// let the mutation type for this mutation know that it is no longer pure neutral
		
		// If a selection coefficient has changed from zero to non-zero, or vice versa, MutationRun's nonneutral mutation caches need revalidation
		if (old_coeff == 0.0)
		{
			species.nonneutral_change_counter_++;
		}
	}
	else if (old_coeff != 0.0)	// && (selection_coeff_ == 0.0) implied by the "else"
	{
		Species &species = mutation_type_ptr_->species_;
		
		// If a selection coefficient has changed from zero to non-zero, or vice versa, MutationRun's nonneutral mutation caches need revalidation
		species.nonneutral_change_counter_++;
	}
	
	// cache values used by the fitness calculation code for speed; see header
	cached_one_plus_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + selection_coeff_);
	cached_one_plus_dom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + mutation_type_ptr_->dominance_coeff_ * selection_coeff_);
	cached_one_plus_hemizygousdom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + mutation_type_ptr_->hemizygous_dominance_coeff_ * selection_coeff_);
	
	return gStaticEidosValueVOID;
}

//	*********************	- (void)setMutationType(io<MutationType>$ mutType)
//
EidosValue_SP Mutation::ExecuteMethod_setMutationType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	Species &species = mutation_type_ptr_->species_;
	
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &species.community_, &species, "setMutationType()");		// SPECIES CONSISTENCY CHECK
	
	if (mutation_type_ptr->nucleotide_based_ != mutation_type_ptr_->nucleotide_based_)
		EIDOS_TERMINATION << "ERROR (Mutation::ExecuteMethod_setMutationType): setMutationType() does not allow a mutation to be changed from nucleotide-based to non-nucleotide-based or vice versa." << EidosTerminate();
	
	// We take just the mutation type pointer; if the user wants a new selection coefficient, they can do that themselves
	mutation_type_ptr_ = mutation_type_ptr;
	
	// If we are non-neutral, make sure the mutation type knows it is now also non-neutral
	if (selection_coeff_ != 0.0)
		mutation_type_ptr_->all_pure_neutral_DFE_ = false;
	
	// cache values used by the fitness calculation code for speed; see header
	cached_one_plus_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + selection_coeff_);
	cached_one_plus_dom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + mutation_type_ptr_->dominance_coeff_ * selection_coeff_);
	cached_one_plus_hemizygousdom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + mutation_type_ptr_->hemizygous_dominance_coeff_ * selection_coeff_);
	
	return gStaticEidosValueVOID;
}


//
//	Mutation_Class
//
#pragma mark -
#pragma mark Mutation_Class
#pragma mark -

EidosClass *gSLiM_Mutation_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *Mutation_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Mutation_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_chromosome,				true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Chromosome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,						true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_id));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_isFixed,				true,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_isFixed));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_isSegregating,			true,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_isSegregating));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationType,			true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_MutationType_Class))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_mutationType));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotide,				false,	kEidosValueMaskString | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_nucleotide));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotideValue,		false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_nucleotideValue));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_originTick,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_originTick));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_position,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_position));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_selectionCoeff,			true,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_selectionCoeff));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopID,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_subpopID)->DeclareAcceleratedSet(Mutation::SetProperty_Accelerated_subpopID));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_tag)->DeclareAcceleratedSet(Mutation::SetProperty_Accelerated_tag));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Mutation_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Mutation_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSelectionCoeff, kEidosValueMaskVOID))->AddFloat_S("selectionCoeff"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMutationType, kEidosValueMaskVOID))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

































































