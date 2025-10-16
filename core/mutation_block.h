//
//  mutation_block.h
//  SLiM
//
//  Created by Ben Haller on 10/14/25.
//  Copyright (c) 2025 Benjamin C. Haller.  All rights reserved.
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

/*
 
 The class MutationBlock represents an allocation zone for Mutation objects and associated data.  Each allocated mutation
 is referenced by its uint32_t index into the block.  Several malloced buffers are maintained by MutationBlock in parallel.
 One holds the Mutation objects themselves.  Another holds refcounts for the mutations, which are best kept separately for
 greater memory locality during tasks that are centered on refcounts.  A third holds per-trait data for each mutation;
 since the number of traits is determined at runtime, the size of each record in that buffer is determined at runtime, and
 so that data cannot be kept within the Mutation objects themselves.  MutationBlock keeps all this in sync, reallocs all
 the blocks as needed, etc.
 
 */

#ifndef __SLiM__mutation_block__
#define __SLiM__mutation_block__


#include "mutation.h"

class Mutation;
class MutationRun;


class MutationBlock
{
public:
	Species &species_;
	
	Mutation *mutation_buffer_ = nullptr;
	slim_refcount_t *refcount_buffer_ = nullptr;
	MutationTraitInfo *trait_info_buffer_ = nullptr;
	
	MutationIndex capacity_ = 0;
	MutationIndex free_index_ = -1;
	MutationIndex last_used_index_ = -1;
	
	int trait_count_;	// the number of MutationTraitInfo records kept in trait_info_buffer_ for each mutation
	
#ifdef DEBUG_LOCKS_ENABLED
	// We do not arbitrate access to the mutation block with a lock; instead, we expect that clients
	// will manage their own multithreading issues.  In DEBUG mode we check for incorrect uses (races).
	// We use this lock to check.  Any failure to acquire the lock indicates a race.
	EidosDebugLock mutation_block_LOCK("mutation_block_LOCK");
#endif
	
	explicit MutationBlock(Species &p_species, int p_trait_count);
	~MutationBlock(void);
	
	void IncreaseMutationBlockCapacity(void);
	void ZeroRefcountBlock(MutationRun &p_mutation_registry);
	
	inline __attribute__((always_inline)) Mutation *MutationForIndex(MutationIndex p_index) const { return mutation_buffer_ + p_index; }
	inline __attribute__((always_inline)) slim_refcount_t RefcountForIndex(MutationIndex p_index) const { return refcount_buffer_[p_index]; }
	inline __attribute__((always_inline)) MutationTraitInfo *TraitInfoIndex(MutationIndex p_index) const { return trait_info_buffer_ + (p_index * trait_count_); }
	
	inline __attribute__((always_inline)) MutationIndex IndexInBlock(const Mutation *p_mutation) const
	{
		return (MutationIndex)(p_mutation - mutation_buffer_);
	}
	
	size_t MemoryUsageForMutationBlock(void) const;
	size_t MemoryUsageForFreeMutations(void) const;
	size_t MemoryUsageForMutationRefcounts(void) const;
	size_t MemoryUsageForTraitInfo(void) const;
	
	inline __attribute__((always_inline)) MutationIndex NewMutationFromBlock(void)
	{
	#ifdef DEBUG_LOCKS_ENABLED
		mutation_block_LOCK.start_critical(0);
	#endif
		
		if (free_index_ == -1)
			IncreaseMutationBlockCapacity();
		
		MutationIndex result = free_index_;
		
		free_index_ = *(MutationIndex *)(mutation_buffer_ + result);
		
		if (last_used_index_ < result)
			last_used_index_ = result;
		
	#ifdef DEBUG_LOCKS_ENABLED
		mutation_block_LOCK.end_critical();
	#endif
		
		return result;	// no need to zero out the memory, we are just an allocater, not a constructor
	}

	inline __attribute__((always_inline)) void DisposeMutationToBlock(MutationIndex p_mutation_index)
	{
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("SLiM_DisposeMutationToBlock(): gSLiM_Mutation_Block change");
		
		void *mut_ptr = mutation_buffer_ + p_mutation_index;
		
		*(MutationIndex *)mut_ptr = free_index_;
		free_index_ = p_mutation_index;
	}
};

#endif /* defined(__SLiM__mutation_block__) */































