//
//  mutation_block.cpp
//  SLiM
//
//  Created by Ben Haller on 10/14/25.
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


#include "mutation_block.h"
#include "mutation_run.h"


#define SLIM_MUTATION_BLOCK_INITIAL_SIZE	16384		// makes for about a 1 MB block; not unreasonable		// NOLINT(*-macro-to-enum) : this is fine


MutationBlock::MutationBlock(Species &p_species, slim_trait_index_t p_trait_count) : species_(p_species), trait_count_(p_trait_count)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("SLiM_CreateMutationBlock(): mutation_buffer_ address change");
	
	// first allocate our buffers; no need to zero the memory
	capacity_ = SLIM_MUTATION_BLOCK_INITIAL_SIZE;
	mutation_buffer_ = (Mutation *)malloc(capacity_ * sizeof(Mutation));
	refcount_buffer_ = (slim_refcount_t *)malloc(capacity_ * sizeof(slim_refcount_t));
	trait_info_buffer_ = (MutationTraitInfo *)malloc(capacity_ * (sizeof(MutationTraitInfo) * trait_count_));
	
	if (!mutation_buffer_ || !refcount_buffer_ || !trait_info_buffer_)
		EIDOS_TERMINATION << "ERROR (SLiM_CreateMutationBlock): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	//std::cout << "Allocating initial mutation block, " << SLIM_MUTATION_BLOCK_INITIAL_SIZE * sizeof(Mutation) << " bytes (sizeof(Mutation) == " << sizeof(Mutation) << ")" << std::endl;
	
	// now we need to set up our free list inside the block; initially all blocks are free
	for (MutationIndex i = 0; i < capacity_ - 1; ++i)
		*(MutationIndex *)(mutation_buffer_ + i) = i + 1;
	
	*(MutationIndex *)(mutation_buffer_ + capacity_ - 1) = -1;
	
	// now that the block is set up, we can start the free list
	free_index_ = 0;
}

MutationBlock::~MutationBlock(void)
{
	free(mutation_buffer_);
	mutation_buffer_ = nullptr;
	
	free(refcount_buffer_);
	refcount_buffer_ = nullptr;
	
	free(trait_info_buffer_);
	trait_info_buffer_ = nullptr;
	
	capacity_ = 0;
	free_index_ = -1;
	last_used_index_ = -1;
	trait_count_ = 0;
}

void MutationBlock::IncreaseMutationBlockCapacity(void)
{
	// We do not use a THREAD_SAFETY macro here because this needs to be checked in release builds also;
	// we are not able to completely protect against this occurring at runtime, and it corrupts the run.
	// It's OK for this to be called when we're inside an inactive parallel region; there is then no
	// race condition.  When a parallel region is active, even inside a critical region, reallocating
	// the mutation block has the potential for a race with other threads.
	if (omp_in_parallel())
	{
		std::cerr << "ERROR (MutationBlock::IncreaseMutationBlockCapacity): (internal error) IncreaseMutationBlockCapacity() was called to reallocate mutation_buffer_ inside a parallel section.  If you see this message, you need to increase the pre-allocation margin for your simulation, because it is generating such an unexpectedly large number of new mutations.  Please contact the SLiM developers for guidance on how to do this." << std::endl;
		raise(SIGTRAP);
	}
	
#ifdef DEBUG_LOCKS_ENABLED
	mutation_block_LOCK.start_critical(1);
#endif
	
	if (!mutation_buffer_)
		EIDOS_TERMINATION << "ERROR (MutationBlock::IncreaseMutationBlockCapacity): (internal error) mutation buffer not allocated!" << EidosTerminate();
	
	// We need to expand the size of our Mutation block.  This has the consequence of invalidating
	// every Mutation * in the program.  In general that is fine; we are careful to only keep
	// pointers to Mutation temporarily, and for long-term reference we use MutationIndex.  The
	// exception to this is EidosValue_Object; the user can put references to mutations into
	// variables that need to remain valid across reallocs like this.  We therefore have to hunt
	// down every EidosValue_Object that contains Mutations, and fix the pointer inside each of
	// them.  Yes, this is very gross.  This is why pointers are evil.  :->
	
	// First we need to get a vector containing the memory location of every pointer-to-Mutation*
	// in every EidosValue_Object in the whole runtime.  This is provided to us by EidosValue_Object,
	// which keeps that registry for us.  We cache the locations of the pointers to mutations that
	// belong to our species.
	std::vector<EidosValue_Object *> &mutation_object_registry = EidosValue_Object::static_EidosValue_Object_Mutation_Registry;
	std::vector<std::uintptr_t> locations_to_patch;
	
	for (EidosValue_Object *mutation_value : mutation_object_registry)
	{
		EidosObject * const *object_buffer = mutation_value->data();
		Mutation * const *mutation_buffer = (Mutation * const *)object_buffer;
		int mutation_count = mutation_value->Count();
		
		for (int index = 0; index < mutation_count; ++index)
		{
			Mutation *mutation = mutation_buffer[index];
			MutationType *muttype = mutation->mutation_type_ptr_;
			Species *species = &muttype->species_;
			
			if (species == &species_)
			{
				// This mutation belongs to our species; so we're about to move it in memory.  We need to
				// keep a pointer to the memory location where this EidosValue_Object is keeping a pointer
				// to it, so that we can patch this pointer after the realloc.
				locations_to_patch.push_back(reinterpret_cast<std::uintptr_t>(mutation_buffer + index));
			}
		}
	}
	
	// Next we do our realloc.  We just need to note the change in value for the pointer.
	// For now we will just double in size; we don't want to waste too much memory, but we
	// don't want to have to realloc too often, either.
	// BCH 11 May 2020: the realloc of mutation_buffer_ is technically problematic, because
	// Mutation is non-trivially copyable according to C++.  But it is safe, so I cast to
	// std::uintptr_t to make the warning go away.
	std::uintptr_t old_mutation_block = reinterpret_cast<std::uintptr_t>(mutation_buffer_);
	MutationIndex old_block_capacity = capacity_;
	
	//std::cout << "old capacity: " << old_block_capacity << std::endl;
	
	// BCH 25 July 2023: check for increasing our block beyond the maximum size of 2^31 mutations.
	// See https://github.com/MesserLab/SLiM/issues/361.  Note that the initial size should be
	// a power of 2, so that we actually reach the maximum; see SLIM_MUTATION_BLOCK_INITIAL_SIZE.
	// In other words, we expect to be at exactly 0x0000000040000000UL here, and thus to double
	// to 0x0000000080000000UL, which is a capacity of 2^31, which is the limit of int32_t.
	if ((size_t)old_block_capacity > 0x0000000040000000UL)	// >2^30 means >2^31 when doubled
		EIDOS_TERMINATION << "ERROR (MutationBlock::IncreaseMutationBlockCapacity): too many mutations; there is a limit of 2^31 (2147483648) segregating mutations in SLiM." << EidosTerminate(nullptr);
	
	capacity_ *= 2;
	mutation_buffer_ = (Mutation *)realloc((void*)mutation_buffer_, capacity_ * sizeof(Mutation));									// NOLINT(*-realloc-usage) : realloc failure is a fatal error anyway
	refcount_buffer_ = (slim_refcount_t *)realloc(refcount_buffer_, capacity_ * sizeof(slim_refcount_t));							// NOLINT(*-realloc-usage) : realloc failure is a fatal error anyway
	trait_info_buffer_ = (MutationTraitInfo *)realloc(trait_info_buffer_, capacity_ * (sizeof(MutationTraitInfo) * trait_count_));	// NOLINT(*-realloc-usage) : realloc failure is a fatal error anyway
	
	if (!mutation_buffer_ || !refcount_buffer_ || !trait_info_buffer_)
		EIDOS_TERMINATION << "ERROR (MutationBlock::IncreaseMutationBlockCapacity): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	//std::cout << "new capacity: " << capacity_ << std::endl;
	
	std::uintptr_t new_mutation_block = reinterpret_cast<std::uintptr_t>(mutation_buffer_);
	
	// Set up the free list to extend into the new portion of the buffer.  If we are called when
	// free_index_ != -1, the free list will start with the new region.
	for (MutationIndex i = old_block_capacity; i < capacity_ - 1; ++i)
		*(MutationIndex *)(mutation_buffer_ + i) = i + 1;
	
	*(MutationIndex *)(mutation_buffer_ + capacity_ - 1) = free_index_;
	
	free_index_ = old_block_capacity;
	
	// Now we go out and fix Mutation * references in EidosValue_Object in all symbol tables
	if (new_mutation_block != old_mutation_block)
	{
		// This may be excessively cautious, but I want to avoid subtracting these uintptr_t values
		// to produce a negative number; that seems unwise and possibly platform-dependent.
		if (old_mutation_block > new_mutation_block)
		{
			std::uintptr_t ptr_diff = old_mutation_block - new_mutation_block;
			
			for (std::uintptr_t location_to_patch : locations_to_patch)
			{
				std::uintptr_t *pointer_to_location = reinterpret_cast<std::uintptr_t *>(location_to_patch);
				std::uintptr_t old_element_ptr = *pointer_to_location;
				std::uintptr_t new_element_ptr = old_element_ptr - ptr_diff;
				
				*pointer_to_location = new_element_ptr;
			}
		}
		else
		{
			std::uintptr_t ptr_diff = new_mutation_block - old_mutation_block;
			
			for (std::uintptr_t location_to_patch : locations_to_patch)
			{
				std::uintptr_t *pointer_to_location = reinterpret_cast<std::uintptr_t *>(location_to_patch);
				std::uintptr_t old_element_ptr = *pointer_to_location;
				std::uintptr_t new_element_ptr = old_element_ptr + ptr_diff;
				
				*pointer_to_location = new_element_ptr;
			}
		}
	}
	
#ifdef DEBUG_LOCKS_ENABLED
	mutation_block_LOCK.end_critical();
#endif
}

void MutationBlock::ZeroRefcountBlock(MutationRun &p_mutation_registry)
{
#pragma unused (p_mutation_registry)
	
	THREAD_SAFETY_IN_ANY_PARALLEL("SLiM_ZeroRefcountBlock(): mutation_buffer_ change");
	
#if 0
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
		// BCH 10/15/2025: This is no longer needed in any case, since we now keep a separate MutationBlock
		// object for each species in each simulation.  Keeping the code as a record of this policy shift.
		slim_refcount_t *refcount_block_ptr = refcount_buffer_;
		const MutationIndex *registry_iter = p_mutation_registry.begin_pointer_const();
		const MutationIndex *registry_iter_end = p_mutation_registry.end_pointer_const();
		
		while (registry_iter != registry_iter_end)
			*(refcount_block_ptr + (*registry_iter++)) = 0;
		
		return;
	}
#endif
	
	// Zero out the whole thing with EIDOS_BZERO(), without worrying about which bits are in use.
	// This hits more memory, but avoids having to read the registry, and should write whole cache lines.
	EIDOS_BZERO(refcount_buffer_, (last_used_index_ + 1) * sizeof(slim_refcount_t));
}

size_t MutationBlock::MemoryUsageForMutationBlock(void) const
{
	// includes the usage counted by MemoryUsageForFreeMutations()
	return capacity_ * sizeof(Mutation);
}

size_t MutationBlock::MemoryUsageForFreeMutations(void) const
{
	size_t mut_count = 0;
	MutationIndex nextFreeBlock = free_index_;
	
	while (nextFreeBlock != -1)
	{
		mut_count++;
		nextFreeBlock = *(MutationIndex *)(mutation_buffer_ + nextFreeBlock);
	}
	
	return mut_count * sizeof(Mutation);
}

size_t MutationBlock::MemoryUsageForMutationRefcounts(void) const
{
	return capacity_ * sizeof(slim_refcount_t);
}

size_t MutationBlock::MemoryUsageForTraitInfo(void) const
{
	return capacity_ * (sizeof(MutationTraitInfo) * trait_count_);
}







































