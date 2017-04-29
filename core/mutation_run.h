//
//  mutation_run.h
//  SLiM
//
//  Created by Ben Haller on 11/29/16.
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

/*
 
 The class MutationRun represents a run of mutations inside a genome.  It is used internally by Genome; it is not visible to Eidos
 code in SLiM, since the Genome class hides it behind a simplified API.  Most clients of Genome should strive to use the Genome
 APIs directly; it would be nice if MutationRun could be kept as a private implementation detail in most (all?) cases.
 
 */

#ifndef __SLiM__mutation_run__
#define __SLiM__mutation_run__


#include "mutation.h"
#include "slim_global.h"
#include "eidos_intrusive_ptr.h"
#include "eidos_object_pool.h"

#include <string.h>


class MutationRun;

typedef Eidos_intrusive_ptr<MutationRun>	MutationRun_SP;

// MutationRun has an internal buffer that it can use to hold mutation pointers.  This makes every Genome object a bit bigger;
// with 64-bit pointers, a buffer big enough to hold four pointers is 32 bytes, ouch.  But avoiding the malloc overhead is
// worth it, for simulations with few mutations; and for simulations with many mutations, the 32-byte overhead is background noise.
#define SLIM_MUTRUN_BUFFER_SIZE		4


// If defined, runtime checks are conducted to ensure that MutationRun objects are not modified once they have been referenced by
// more than one Genome.  This allows multiple Genome objects to refer to the same underlying MutationRun securely.  Genome and other
// clients are responsible for making a copy of a MutationRun before modification when they do not know they are the sole owner.
// FIXME should probably be turned off when DEBUG is not set, once things have shaken out...
#define SLIM_MUTRUN_CHECK_LOCKING


// MutationRun has a marking mechanism to let us loop through all genomes and perform an operation on each MutationRun once.
// This counter is used to do that; a client wishing to perform such an operation should increment the counter and then use it
// in conjuction with operation_id_ below.
extern int64_t gSLiM_MutationRun_OperationID;


class MutationRun
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
protected:

	mutable uint32_t intrusive_ref_count;						// used by Eidos_intrusive_ptr

protected:
	
	int mutation_count_ = 0;									// the number of entries presently in mutations_
	int mutation_capacity_ = SLIM_MUTRUN_BUFFER_SIZE;			// the capacity of mutations_; we start by using our internal buffer
	Mutation *(mutations_buffer_[SLIM_MUTRUN_BUFFER_SIZE]);		// a built-in buffer to prevent the need for malloc with few mutations
	Mutation **mutations_ = mutations_buffer_;					// OWNED POINTER: a pointer to an array of pointers to const Mutation objects
	
public:
	
	int64_t operation_id_ = 0;		// used to mark the MutationRun objects that have been handled by a global operation
	
	// Allocation and disposal of MutationRun objects should go through these funnels.  The point of this architecture
	// is to re-use the instances completely.  We don't use EidosObjectPool here because it would construct/destruct the
	// objects, and we actually don't want that; we want the buffers in used MutationRun objects to stay allocated, for
	// greater speed.  We are constantly creating new runs, adding mutations in to them, and then throwing them away; once
	// the pool of freed runs settles into a steady state, that process can go on with no memory allocs or reallocs at all.
	static MutationRun *NewMutationRun(void);
	static void FreeMutationRun(MutationRun *p_run);
	static std::vector<MutationRun *> s_freed_mutation_runs_;
	
	MutationRun(const MutationRun&) = delete;					// no copying
	MutationRun& operator=(const MutationRun&) = delete;		// no copying
	MutationRun(void);											// constructed empty
	~MutationRun(void);
	
#ifdef SLIM_MUTRUN_CHECK_LOCKING
	void LockingViolation(void) const __attribute__((__noreturn__)) __attribute__((cold));
#define SLIM_MUTRUN_LOCK_CHECK()	if (intrusive_ref_count > 1) LockingViolation();
#else
#define SLIM_MUTRUN_LOCK_CHECK()	;
#endif
	
	inline Mutation *const & operator[] (int p_index) const {	// [] returns a reference to a pointer to Mutation; this is the const-pointer variant
		return mutations_[p_index];
	}
	
	inline Mutation *& operator[] (int p_index) {				// [] returns a reference to a pointer to Mutation; this is the non-const-pointer variant
		return mutations_[p_index];
	}
	
	inline int size(void) const {
		return mutation_count_;
	}
	
	inline void set_size(int p_size) {
		SLIM_MUTRUN_LOCK_CHECK();
		
		mutation_count_ = p_size;
	}
	
	inline void clear(void)
	{
		SLIM_MUTRUN_LOCK_CHECK();
		
		mutation_count_ = 0;
	}
	
	inline bool contains_mutation(const Mutation *p_mutation) {
		// This function does not assume that mutations are in sorted order, because we want to be able to use it with the mutation registry
		Mutation *const *position = begin_pointer_const();
		Mutation *const *end_position = end_pointer_const();
		
		for (; position != end_position; ++position)
			if (*position == p_mutation)
				return true;
		
		return false;
	}
	
	inline void pop_back(void)
	{
		SLIM_MUTRUN_LOCK_CHECK();
		
		if (mutation_count_ > 0)	// the standard says that popping an empty vector results in undefined behavior; this seems reasonable
			--mutation_count_;
	}
	
	inline void emplace_back(Mutation *p_mutation)
	{
		SLIM_MUTRUN_LOCK_CHECK();
		
		if (mutation_count_ == mutation_capacity_)
		{
			if (mutations_ == mutations_buffer_)
			{
				// We're allocating a malloced buffer for the first time, so we outgrew our internal buffer.  We might try jumping by
				// more than a factor of two, to avoid repeated reallocs; in practice, that is not a win.  The large majority of SLiM's
				// memory usage in typical simulations comes from these arrays of pointers kept by Genome, so making them larger
				// than necessary can massively balloon SLiM's memory usage for very little gain.  The realloc() calls are very fast;
				// avoiding it is not a major concern.  In fact, using *8 here instead of *2 actually slows down a test simulation,
				// perhaps because it causes a true realloc rather than just a size increment of the existing malloc block.  Who knows.
				mutation_capacity_ = SLIM_MUTRUN_BUFFER_SIZE * 2;
				mutations_ = (Mutation **)malloc(mutation_capacity_ * sizeof(Mutation*));
				
				memcpy(mutations_, mutations_buffer_, mutation_count_ * sizeof(Mutation*));
			}
			else
			{
				// Up to a point, we want to double our capacity each time we have to realloc.  Beyond a certain point, that starts to
				// use a whole lot of memory, so we start expanding at a linear rate instead of a geometric rate.  This policy is based
				// on guesswork; the optimal policy would depend strongly on the particular details of the simulation being run.  The
				// goal, though, is twofold: (1) to avoid excessive reallocations early on, and (2) to avoid the peak memory usage,
				// when all genomes have grown to their stable equilibrium size, being drastically higher than necessary.  The policy
				// chosen here is intended to try to achieve both of those goals.  The size sequence we follow now is:
				//
				//	4 (using our built-in pointer buffer)
				//	8 (2x)
				//	16 (2x)
				//	32 (2x)
				//	48 (+16)
				//	64 (+16)
				//	80 (+16)
				//	...
				
				if (mutation_capacity_ < 32)
					mutation_capacity_ <<= 1;		// double the number of pointers we can hold
				else
					mutation_capacity_ += 16;
				
				mutations_ = (Mutation **)realloc(mutations_, mutation_capacity_ * sizeof(Mutation*));
			}
		}
		
		// Now we are guaranteed to have enough memory, so copy the pointer in
		// (unless malloc/realloc failed, which we're not going to worry about!)
		*(mutations_ + mutation_count_) = p_mutation;
		++mutation_count_;
	}
	
	inline void emplace_back_bulk(Mutation **p_mutation_ptr, long p_copy_count)
	{
		SLIM_MUTRUN_LOCK_CHECK();
		
		if (mutation_count_ + p_copy_count > mutation_capacity_)
		{
			// See emplace_back for comments on our capacity policy
			if (mutations_ == mutations_buffer_)
			{
				// We're allocating a malloced buffer for the first time, so we outgrew our internal buffer.  We might try jumping by
				// more than a factor of two, to avoid repeated reallocs; in practice, that is not a win.  The large majority of SLiM's
				// memory usage in typical simulations comes from these arrays of pointers kept by Genome, so making them larger
				// than necessary can massively balloon SLiM's memory usage for very little gain.  The realloc() calls are very fast;
				// avoiding it is not a major concern.  In fact, using *8 here instead of *2 actually slows down a test simulation,
				// perhaps because it causes a true realloc rather than just a size increment of the existing malloc block.  Who knows.
				mutation_capacity_ = SLIM_MUTRUN_BUFFER_SIZE * 2;
				
				while (mutation_count_ + p_copy_count > mutation_capacity_)
				{
					if (mutation_capacity_ < 32)
						mutation_capacity_ <<= 1;		// double the number of pointers we can hold
					else
						mutation_capacity_ += 16;
				}
				
				mutations_ = (Mutation **)malloc(mutation_capacity_ * sizeof(Mutation*));
				
				memcpy(mutations_, mutations_buffer_, mutation_count_ * sizeof(Mutation*));
			}
			else
			{
				do
				{
					if (mutation_capacity_ < 32)
						mutation_capacity_ <<= 1;		// double the number of pointers we can hold
					else
						mutation_capacity_ += 16;
				}
				while (mutation_count_ + p_copy_count > mutation_capacity_);
				
				mutations_ = (Mutation **)realloc(mutations_, mutation_capacity_ * sizeof(Mutation*));
			}
		}
		
		// Now we are guaranteed to have enough memory, so copy the pointers in
		// (unless malloc/realloc failed, which we're not going to worry about!)
		memcpy(mutations_ + mutation_count_, p_mutation_ptr, p_copy_count * sizeof(Mutation*));
		mutation_count_ += p_copy_count;
	}
	
	inline void insert_sorted_mutation(Mutation *p_mutation)
	{
		// first push it back on the end, which deals with capacity/locking issues
		emplace_back(p_mutation);
		
		// if it was our first element, then we're done; this would work anyway, but since it is extremely common let's short-circuit it
		if (mutation_count_ == 1)
			return;
		
		// then find the proper position for it
		Mutation **sort_position = begin_pointer();
		Mutation *const *end_position = end_pointer_const() - 1;		// the position of the newly added element
		
		for ( ; sort_position != end_position; ++sort_position)
			if (CompareMutations(p_mutation, *sort_position))	// if (p_mutation->position_ < (*sort_position)->position_)
				break;
		
		// if we got all the way to the end, then the mutation belongs at the end, so we're done
		if (sort_position == end_position)
			return;
		
		// the new mutation has a position less than that at sort_position, so we need to move everybody upward
		memmove(sort_position + 1, sort_position, (char *)end_position - (char *)sort_position);
		
		// finally, put the mutation where it belongs
		*sort_position = p_mutation;
	}
	
	inline void insert_sorted_mutation_if_unique(Mutation *p_mutation)
	{
		// first push it back on the end, which deals with capacity/locking issues
		emplace_back(p_mutation);
		
		// if it was our first element, then we're done; this would work anyway, but since it is extremely common let's short-circuit it
		if (mutation_count_ == 1)
			return;
		
		// then find the proper position for it
		Mutation **sort_position = begin_pointer();
		Mutation *const *end_position = end_pointer_const() - 1;		// the position of the newly added element
		
		for ( ; sort_position != end_position; ++sort_position)
		{
			if (CompareMutations(p_mutation, *sort_position))	// if (p_mutation->position_ < (*sort_position)->position_)
			{
				break;
			}
			else if (p_mutation == *sort_position)
			{
				// We are only supposed to insert the mutation if it is unique, and apparently it is not; discard it off the end
				--mutation_count_;
				return;
			}
		}
		
		// if we got all the way to the end, then the mutation belongs at the end, so we're done
		if (sort_position == end_position)
			return;
		
		// the new mutation has a position less than that at sort_position, so we need to move everybody upward
		memmove(sort_position + 1, sort_position, (char *)end_position - (char *)sort_position);
		
		// finally, put the mutation where it belongs
		*sort_position = p_mutation;
	}
	
	bool _enforce_stack_policy_for_addition(slim_position_t p_position, MutationType *p_mut_type_ptr, MutationStackPolicy p_policy);
	
	inline bool enforce_stack_policy_for_addition(slim_position_t p_position, MutationType *p_mut_type_ptr)
	{
		MutationStackPolicy policy = p_mut_type_ptr->stack_policy_;
		
		if (policy == MutationStackPolicy::kStack)
		{
			// If mutations are allowed to stack (the default), then we have no work to do and the new mutation is always added
			return true;
		}
		else
		{
			// Otherwise, a relatively complicated check is needed, so we call out to a non-inline function
			return _enforce_stack_policy_for_addition(p_position, p_mut_type_ptr, policy);
		}
	}
	
	inline void copy_from_run(const MutationRun &p_source_run)
	{
		SLIM_MUTRUN_LOCK_CHECK();
		
		int source_mutation_count = p_source_run.mutation_count_;
		
		// first we need to ensure that we have sufficient capacity
		if (source_mutation_count > mutation_capacity_)
		{
			mutation_capacity_ = p_source_run.mutation_capacity_;		// just use the same capacity as the source
			
			// mutations_buffer_ is not malloced and cannot be realloced, so forget that we were using it
			if (mutations_ == mutations_buffer_)
				mutations_ = nullptr;
			
			mutations_ = (Mutation **)realloc(mutations_, mutation_capacity_ * sizeof(Mutation*));
		}
		
		// then copy all pointers from the source to ourselves
		memcpy(mutations_, p_source_run.mutations_, source_mutation_count * sizeof(Mutation*));
		mutation_count_ = source_mutation_count;
	}
	
	inline Mutation *const *begin_pointer_const(void) const
	{
		return mutations_;
	}
	
	inline Mutation *const *end_pointer_const(void) const
	{
		return mutations_ + mutation_count_;
	}
	
	inline Mutation **begin_pointer(void)
	{
		SLIM_MUTRUN_LOCK_CHECK();
		
		return mutations_;
	}
	
	inline Mutation **end_pointer(void)
	{
		SLIM_MUTRUN_LOCK_CHECK();
		
		return mutations_ + mutation_count_;
	}
	
	void RemoveFixedMutations(slim_refcount_t p_fixed_count, int64_t p_operation_id);
	
	// Eidos_intrusive_ptr support
	inline __attribute__((always_inline)) uint32_t use_count() const { return intrusive_ref_count; }
	inline __attribute__((always_inline)) bool unique() const { return intrusive_ref_count == 1; }
	
	friend void Eidos_intrusive_ptr_add_ref(const MutationRun *p_value);
	friend void Eidos_intrusive_ptr_release(const MutationRun *p_value);
};

// Eidos_intrusive_ptr support
inline __attribute__((always_inline)) void Eidos_intrusive_ptr_add_ref(const MutationRun *p_value)
{
	++(p_value->intrusive_ref_count);
}

inline __attribute__((always_inline)) void Eidos_intrusive_ptr_release(const MutationRun *p_value)
{
	if ((--(p_value->intrusive_ref_count)) == 0)
	{
		// Do not delete; all MutationRun objects under Eidos_intrusive_ptr should have been allocated
		// by MutationRun::NewMutationRun(), so we return them to that pool here without destructing
		MutationRun::FreeMutationRun(const_cast<MutationRun *>(p_value));
	}
}


#endif /* __SLiM__mutation_run__ */
























