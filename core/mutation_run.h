//
//  mutation_run.h
//  SLiM
//
//  Created by Ben Haller on 11/29/16.
//  Copyright (c) 2016-2020 Philipp Messer.  All rights reserved.
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
#include "slim_globals.h"
#include "eidos_intrusive_ptr.h"
#include "eidos_object_pool.h"

#include <string.h>
#include <assert.h>


class MutationRun;

typedef Eidos_intrusive_ptr<MutationRun>	MutationRun_SP;

// MutationRun has an internal buffer that it can use to hold mutation pointers.  This makes every MutationRun object a bit bigger;
// with 64-bit pointers, a buffer big enough to hold four pointers is 32 bytes, ouch.  But avoiding the malloc overhead is
// worth it, for simulations with few mutations; and for simulations with many mutations, the 32-byte overhead is background noise.
#define SLIM_MUTRUN_BUFFER_SIZE		4


// If defined, runtime checks are conducted to ensure that MutationRun objects are not modified once they have been referenced by
// more than one Genome.  This allows multiple Genome objects to refer to the same underlying MutationRun securely.  Genome and other
// clients are responsible for making a copy of a MutationRun before modification when they do not know they are the sole owner.
#ifdef DEBUG
#define SLIM_MUTRUN_CHECK_LOCKING
#endif


// If defined as 1, MutationRun will keep a side cache of the non-neutral mutations occuring inside it.  This can greatly accelerate
// fitness calculations, but does consume additional memory, and is not always advantageous.  Define to 0 to disable this feature.
// I'm not sure how long I will maintain the ability to disable these caches; the overhead is quite small, so I think it would be OK
// to just make this always be on.  At present this flag is mostly useful for testing purposes.
#define SLIM_USE_NONNEUTRAL_CACHES	1


// MutationRun has a marking mechanism to let us loop through all genomes and perform an operation on each MutationRun once.
// This counter is used to do that; a client wishing to perform such an operation should increment the counter and then use it
// in conjuction with operation_id_ below.
extern int64_t gSLiM_MutationRun_OperationID;


class MutationRun
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:

	mutable uint32_t intrusive_ref_count_;						// used by Eidos_intrusive_ptr

private:
	
	int32_t mutation_count_ = 0;								// the number of entries presently in mutations_
	int32_t mutation_capacity_ = SLIM_MUTRUN_BUFFER_SIZE;		// the capacity of mutations_; we start by using our internal buffer
	MutationIndex mutations_buffer_[SLIM_MUTRUN_BUFFER_SIZE];	// a built-in buffer to prevent the need for malloc with few mutations
	MutationIndex *mutations_ = mutations_buffer_;				// OWNED POINTER: a pointer to an array of MutationIndex
	
#if SLIM_USE_NONNEUTRAL_CACHES
	
	// Non-neutral mutation caching.  This is a somewhat complex scheme designed to speed up fitness calculations.
	// The idea is that the mutation run can cache, once, a list of all of the non-neutral mutations it contains,
	// and then the fitness code can refer to that cached list from then on, saving a huge amount of looping over
	// neutral mutations in many simulations.  This simple idea is complicated by a few factors.  First of all, if
	// the mutation run changes, the cache needs to be invalidated.  Second, if the external information that the
	// cache relies upon changes, the cache needs to be invalidated.  That external information consists of (a) the
	// selection coefficients of mutations, and (b) the existence and state of fitness() callbacks.  There are
	// three separate regimes in which these caches are used:
	//
	//	1. No fitness callbacks defined.  Here caches depend solely upon mutation selection coefficients, and can
	//		be carried forward through generations with impunity.  If any mutation's selcoeff is changed between
	//		zero and non-zero, a global flag in SLiMSim (nonneutral_change_counter_) marks all caches as invalid.
	//
	//	2. Only constant-effect neutral callbacks are defined: "return 0.0;".  RecalculateFitness() runs through
	//		mutation types and callbacks, and figures this state out and sets a flag in each mutation type as to
	//		whether it is effectively neutral, after considering these constant-effect callbacks, or not.  This
	//		state changes in every generation, so caches cannot be carried forward from generation to generation
	//		in this regime unless the state of the callbacks, with respect to making mutation types neutral, is
	//		unchanged.  If RecalculateFitness() detects a callback change, it sets the global all-invalid flag.
	//
	//	3. At least one non-constant callback is defined.  RecalculateFitness() figures this out, and if this is
	//		the case, the non-neutral cache must include all mutations for which their muttype has a callback
	//		defined at all, whether constant or not, neutral or not, active or not, because the callback regime
	//		itself could change unpredictably.  These caches cannot be carried forward unless the state of the
	//		callbacks, with respect to which mutation types are influenced by them, is unchanged.  If a callback
	//		change is detected by RecalculateFitness(), it sets the global all-invalid flag.
	//
	//	(FIXME) One could imagine inserting a regime between 2 and 3 that would allow a mix of constant and
	//		non-constant callbacks, as long as the non-constant callbacks were "well-behaved" – no use of the
	//		active property, no executeLambda, etc. – so that SLiM could know that the constant callbacks would
	//		apply if they were active.  This could be pretty useful for models that have a mix of QTLs (using
	//		a constant neutral callbacks) and other loci that are governed by fitness callbacks.  This strikes
	//		me as a pretty edge case, though; mostly models are either QTL models or non-QTL models, I think.
	//
	// When models switch between one regime and another, they generally need to recache, since the criteria
	// for inclusion in the cache differs from regime to regime.  This is handled by RecalculateFitness().
	// The last regime used (for the previous generation) is remembered in sim.last_nonneutral_regime_.
	//
	// Mutation runs are considered to be immutable in SLiM if they are referred to by more than one genome.
	// If they are referred to only once, however, they can be changed.  What that occurs, their nonneutral
	// cache must be invalidated.  This means that any code that calls UseCount() on a mutrun, and modifies it
	// if the count is 1, must also invalidate the nonneutral cache.  This is done automatically by the existing
	// methods – in particular, MutationRun::will_modify_run(), which should be a funnel for all such code.
	// Newly created mutation runs are also routinely modified on the (valid) assumption that they are referred
	// to by only one genome (or no genomes at all, more likely); this is fine since they don't have a nonneutral
	// cache yet anyway.
	//
	// These caches are only used for mutation runs that are accessed by the FitnessOfParentWithGenomeIndices...()
	// suite of methods; pure neutral models and non-chromosome-dependent models will never touch these caches
	// and the buffer will never be allocated.
	
	int32_t nonneutral_mutation_capacity_ = 0;					// the capacity of nonneutral_mutations_
	int32_t nonneutral_mutations_count_ = -1;					// the number of entries currently used; -1 indicates an invalid cache
	MutationIndex *nonneutral_mutations_ = nullptr;				// OWNED POINTER: a pointer to MutationIndex for non-neutral mutations
	
	int32_t nonneutral_change_validation_ = 0;					// compared to sim.nonneutral_change_counter_ to detect changes

#if defined(SLIMGUI) && (SLIMPROFILING == 1)
// PROFILING
	
	bool recached_run_ = false;
	
#endif	// defined(SLIMGUI) && (SLIMPROFILING == 1)
	
#endif	// SLIM_USE_NONNEUTRAL_CACHES
	
public:
	
	int64_t operation_id_ = 0;		// used to mark the MutationRun objects that have been handled by a global operation
	
	// Allocation and disposal of MutationRun objects should go through these funnels.  The point of this architecture
	// is to re-use the instances completely.  We don't use EidosObjectPool here because it would construct/destruct the
	// objects, and we actually don't want that; we want the buffers in used MutationRun objects to stay allocated, for
	// greater speed.  We are constantly creating new runs, adding mutations in to them, and then throwing them away; once
	// the pool of freed runs settles into a steady state, that process can go on with no memory allocs or reallocs at all.
	static inline __attribute__((always_inline)) MutationRun *NewMutationRun(void)
	{
		if (s_freed_mutation_runs_.size())
		{
			// We assume that the object in the free list is in a reuseable state; see FreeMutationRun() below.
			
			MutationRun *back = s_freed_mutation_runs_.back();
			
			s_freed_mutation_runs_.pop_back();
			return back;
		}
		
		return new MutationRun();
	}
	
	static inline __attribute__((always_inline)) void FreeMutationRun(MutationRun *p_run)
	{
		// We return mutation runs to the free list in a valid, reuseable state.  We do not free its buffers, avoiding that
		// free/alloc thrash is one of the big wins of recycling mutation run objects, in fact.
		
		p_run->mutation_count_ = 0;						// empty the mutation buffer
		
#if SLIM_USE_NONNEUTRAL_CACHES
		p_run->nonneutral_mutations_count_ = -1;		// mark the non-neutral mutation cache as invalid
#endif
		
		s_freed_mutation_runs_.emplace_back(p_run);
	}
	
	static inline void DeleteMutationRunFreeList(void)
	{
		// This is not normally used by SLiM, but it is used in the SLiM test code in order to prevent mutation runs
		// that are allocated in one test from carrying over to later tests (which makes leak debugging a pain).
		for (auto mutrun_iter : s_freed_mutation_runs_)
			delete (mutrun_iter);
		
		s_freed_mutation_runs_.clear();
	}
	
	static std::vector<MutationRun *> s_freed_mutation_runs_;
	
	MutationRun(const MutationRun&) = delete;					// no copying
	MutationRun& operator=(const MutationRun&) = delete;		// no copying
	inline MutationRun(void) : intrusive_ref_count_(0) { }		// constructed empty
	~MutationRun(void);
	
	
#ifdef SLIM_MUTRUN_CHECK_LOCKING
	
	void LockingViolation(void) const __attribute__((__noreturn__)) __attribute__((cold)) __attribute__((analyzer_noreturn));
	
#if SLIM_USE_NONNEUTRAL_CACHES
	// Added (nonneutral_mutations_count_ != -1) with the addition of the nonneutral caches; modifying a unique run should not occur after it has cached
#define SLIM_MUTRUN_LOCK_CHECK()	if ((intrusive_ref_count_ > 1) || (nonneutral_mutations_count_ != -1)) LockingViolation();
#else
#define SLIM_MUTRUN_LOCK_CHECK()	if (intrusive_ref_count_ > 1) LockingViolation();
#endif

#else
	
#define SLIM_MUTRUN_LOCK_CHECK()	;
	
#endif
	
	
	inline __attribute__((always_inline)) void will_modify_run(void) {
		SLIM_MUTRUN_LOCK_CHECK();
		
#if SLIM_USE_NONNEUTRAL_CACHES
		nonneutral_mutations_count_ = -1;		// invalidate the nonneutral cache since the run is changing
#endif
	}
	
	inline __attribute__((always_inline)) MutationIndex const & operator[] (int p_index) const {	// [] returns a reference to a pointer to Mutation; this is the const-pointer variant
		return mutations_[p_index];
	}
	
	inline __attribute__((always_inline)) MutationIndex& operator[] (int p_index) {				// [] returns a reference to a pointer to Mutation; this is the non-const-pointer variant
		return mutations_[p_index];
	}
	
	inline __attribute__((always_inline)) int size(void) const {
		return mutation_count_;
	}
	
	inline __attribute__((always_inline)) void set_size(int p_size) {
		SLIM_MUTRUN_LOCK_CHECK();
		
		mutation_count_ = p_size;
	}
	
	inline __attribute__((always_inline)) void clear(void)
	{
		SLIM_MUTRUN_LOCK_CHECK();
		
		mutation_count_ = 0;
	}
	
	bool contains_mutation(MutationIndex p_mutation_index);
	
	Mutation *mutation_with_type_and_position(MutationType *p_mut_type, slim_position_t p_position, slim_position_t p_last_position);
	
	inline __attribute__((always_inline)) void pop_back(void)
	{
		SLIM_MUTRUN_LOCK_CHECK();
		
		if (mutation_count_ > 0)	// the standard says that popping an empty vector results in undefined behavior; this seems reasonable
			--mutation_count_;
	}
	
	inline __attribute__((always_inline)) void emplace_back(MutationIndex p_mutation_index)
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
				mutations_ = (MutationIndex *)malloc(mutation_capacity_ * sizeof(MutationIndex));
				
				memcpy(mutations_, mutations_buffer_, mutation_count_ * sizeof(MutationIndex));
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
				
				mutations_ = (MutationIndex *)realloc(mutations_, mutation_capacity_ * sizeof(MutationIndex));
			}
		}
		
		// Now we are guaranteed to have enough memory, so copy the pointer in
		// (unless malloc/realloc failed, which we're not going to worry about!)
		*(mutations_ + mutation_count_) = p_mutation_index;
		++mutation_count_;
	}
	
	inline void emplace_back_bulk(const MutationIndex *p_mutation_indices, long p_copy_count)
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
				
				mutations_ = (MutationIndex *)malloc(mutation_capacity_ * sizeof(MutationIndex));
				
				memcpy(mutations_, mutations_buffer_, mutation_count_ * sizeof(MutationIndex));
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
				
				mutations_ = (MutationIndex *)realloc(mutations_, mutation_capacity_ * sizeof(MutationIndex));
			}
		}
		
		// Now we are guaranteed to have enough memory, so copy the pointers in
		// (unless malloc/realloc failed, which we're not going to worry about!)
		memcpy(mutations_ + mutation_count_, p_mutation_indices, p_copy_count * sizeof(MutationIndex));
		mutation_count_ += p_copy_count;
	}
	
	inline void insert_sorted_mutation(MutationIndex p_mutation_index)
	{
		// first push it back on the end, which deals with capacity/locking issues
		emplace_back(p_mutation_index);
		
		// if it was our first element, then we're done; this would work anyway, but since it is extremely common let's short-circuit it
		if (mutation_count_ == 1)
			return;
		
		// then find the proper position for it
		Mutation *mut_ptr_to_insert = gSLiM_Mutation_Block + p_mutation_index;
		MutationIndex *sort_position = begin_pointer();
		const MutationIndex *end_position = end_pointer_const() - 1;		// the position of the newly added element
		
		for ( ; sort_position != end_position; ++sort_position)
			if (CompareMutations(mut_ptr_to_insert, gSLiM_Mutation_Block + *sort_position))	// if (p_mutation->position_ < (*sort_position)->position_)
				break;
		
		// if we got all the way to the end, then the mutation belongs at the end, so we're done
		if (sort_position == end_position)
			return;
		
		// the new mutation has a position less than that at sort_position, so we need to move everybody upward
		memmove(sort_position + 1, sort_position, (char *)end_position - (char *)sort_position);
		
		// finally, put the mutation where it belongs
		*sort_position = p_mutation_index;
	}

	/*
	 This version of insert_sorted_mutation() searches for the insertion point from the end
	 instead of the beginning.  I investigated that, but ultimately decided on a different
	 course of action, and this change seems unnecessary and destabilizing; I don't think
	 it would benefit any of the users of this method, on average.  Keeping the code for
	 posterity.  BCH 29 October 2017
	 
	inline void insert_sorted_mutation(MutationIndex p_mutation_index)
	{
		// first push it back on the end, which deals with capacity/locking issues
		emplace_back(p_mutation_index);
		
		// if it was our first element, then we're done; this would work anyway, but since it is extremely common let's short-circuit it
		if (mutation_count_ == 1)
			return;
		
		// then find the proper position for it; mutations are often added in ascending order, so let's search backwards
		Mutation *mut_ptr_to_insert = gSLiM_Mutation_Block + p_mutation_index;
		MutationIndex *sort_position = end_pointer() - 2;				// the position back one from the newly added element (at end-1)
		const MutationIndex *end_position = begin_pointer_const() - 1;	// the position back one from the start of the mutation run
		
		// check for the mutation actually belonging at the end, for the quick return and simple completion design
		if (!CompareMutations(mut_ptr_to_insert, gSLiM_Mutation_Block + *sort_position))	// if (p_mutation->position_ >= (*sort_position)->position_)
			return;
		
		// ok, it doesn't belong at the end; start searching at end_pointer() - 3, which might not exist (might ==end_position)
		--sort_position;
		
		for ( ; sort_position != end_position; --sort_position)
			if (!CompareMutations(mut_ptr_to_insert, gSLiM_Mutation_Block + *sort_position))	// if (p_mutation->position_ >= (*sort_position)->position_)
				break;
		
		// ok, it belongs right *after* sort_position; the mutation at sort_position should stay, so skip ahead one
		++sort_position;
		
		// the new mutation has a position less than that at sort_position, so we need to move everybody upward
		memmove(sort_position + 1, sort_position, (char *)(end_pointer_const() - 1) - (char *)sort_position);
		
		// finally, put the mutation where it belongs
		*sort_position = p_mutation_index;
	}*/
	
	inline void insert_sorted_mutation_if_unique(MutationIndex p_mutation_index)
	{
		// first push it back on the end, which deals with capacity/locking issues
		emplace_back(p_mutation_index);
		
		// if it was our first element, then we're done; this would work anyway, but since it is extremely common let's short-circuit it
		if (mutation_count_ == 1)
			return;
		
		// then find the proper position for it
		Mutation *mut_ptr_to_insert = gSLiM_Mutation_Block + p_mutation_index;
		MutationIndex *sort_position = begin_pointer();
		const MutationIndex *end_position = end_pointer_const() - 1;		// the position of the newly added element
		
		for ( ; sort_position != end_position; ++sort_position)
		{
			if (CompareMutations(mut_ptr_to_insert, gSLiM_Mutation_Block + *sort_position))	// if (p_mutation->position_ < (*sort_position)->position_)
			{
				break;
			}
			else if (p_mutation_index == *sort_position)
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
		*sort_position = p_mutation_index;
	}
	
	bool _EnforceStackPolicyForAddition(slim_position_t p_position, MutationStackPolicy p_policy, int64_t p_stack_group);
	inline __attribute__((always_inline)) bool enforce_stack_policy_for_addition(slim_position_t p_position, MutationType *p_mut_type_ptr);	// below
	
	inline __attribute__((always_inline)) void copy_from_run(const MutationRun &p_source_run)
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
			
			mutations_ = (MutationIndex *)realloc(mutations_, mutation_capacity_ * sizeof(MutationIndex));
		}
		
		// then copy all pointers from the source to ourselves
		memcpy(mutations_, p_source_run.mutations_, source_mutation_count * sizeof(MutationIndex));
		mutation_count_ = source_mutation_count;
	}
	
	// Shorthand for clear(), then copy_from_run(p_mutations_to_set), then insert_sorted_mutation() on every
	// mutation in p_mutations_to_add, with checks with enforce_stack_policy_for_addition().  The point of
	// this is speed: like DoClonalMutation(), we can merge the new mutations in much faster if we do it in
	// bulk.  Note that p_mutations_to_set and p_mutations_to_add must both be sorted by position, and it
	// must be guaranteed that none of the mutations in the two given runs are the same.
	void clear_set_and_merge(MutationRun &p_mutations_to_set, MutationRun &p_mutations_to_add);
	
	// This is used by the tree sequence recording code to get the full derived state at a given position.
	// Note that the vector returned is cached internally and reused with each call, for speed.
	const std::vector<Mutation *> *derived_mutation_ids_at_position(slim_position_t p_position) const;
	
	inline __attribute__((always_inline)) const MutationIndex *begin_pointer_const(void) const
	{
		return mutations_;
	}
	
	inline __attribute__((always_inline)) const MutationIndex *end_pointer_const(void) const
	{
		return mutations_ + mutation_count_;
	}
	
	inline __attribute__((always_inline)) MutationIndex *begin_pointer(void)
	{
		SLIM_MUTRUN_LOCK_CHECK();
		
		return mutations_;
	}
	
	inline __attribute__((always_inline)) MutationIndex *end_pointer(void)
	{
		SLIM_MUTRUN_LOCK_CHECK();
		
		return mutations_ + mutation_count_;
	}
	
	void _RemoveFixedMutations(void);
	inline __attribute__((always_inline)) void RemoveFixedMutations(int64_t p_operation_id)
	{
		if (operation_id_ != p_operation_id)
		{
			operation_id_ = p_operation_id;
			
			_RemoveFixedMutations();
		}
	}
	
	// Hash and comparison functions used by UniqueMutationRuns() to unique mutation runs
	inline __attribute__((always_inline)) int64_t Hash(void)
	{
		uint64_t hash = mutation_count_;
		
		// Hash mutation pointers together with the mutation count; we use every 4th mutation pointer for 4x speed,
		// and it doesn't seem to produce too many hash collisions, at least for the models I've tried.  Actually,
		// early on when chromosomes are nearly empty collisions are common (where using mut_index++ gives us zero
		// collisions in pretty much all cases), but at that stage Identical() is fast so it's OK.  At equilibrium
		// when chromosomes are more full collisions are much less common, so we avoid Identical() when it is slow.
		for (int mut_index = 0; mut_index < mutation_count_; mut_index += 4)
		{
			// this hash function is a stab in the dark based upon the sdbm algorithm here: http://www.cse.yorku.ca/~oz/hash.html
			hash = (uint64_t)mutations_[mut_index] + (hash << 6) + (hash << 16) - hash;
		}
		
		return hash;
	}
	
	inline __attribute__((always_inline)) bool Identical(MutationRun &p_run)
	{
		if (mutation_count_ != p_run.mutation_count_)
			return false;
		
		if (memcmp(mutations_, p_run.mutations_, mutation_count_ * sizeof(MutationIndex)) != 0)
			return false;
		
		return true;
	}
	
	// splitting mutation runs
	void split_run(MutationRun **p_first_half, MutationRun **p_second_half, slim_position_t p_split_first_position);
	
#if SLIM_USE_NONNEUTRAL_CACHES
	// caching non-neutral mutations; see above for comments about the "regime" etc.
	
	inline __attribute__((always_inline)) void zero_out_nonneutral_buffer(void)
	{
		if (!nonneutral_mutations_)
		{
			// If we don't have a buffer allocated yet, follow the same rules as for the main mutation buffer,
			// but without the built-in internal buffer at the beginning
			nonneutral_mutation_capacity_ = SLIM_MUTRUN_BUFFER_SIZE * 2;
			nonneutral_mutations_ = (MutationIndex *)malloc(nonneutral_mutation_capacity_ * sizeof(MutationIndex));
		}
		
		// empty out the current buffer contents
		nonneutral_mutations_count_ = 0;
	}
	
	inline __attribute__((always_inline)) void add_to_nonneutral_buffer(MutationIndex p_mutation_index)
	{
		// This is basically the emplace_back() code, but for the nonneutral buffer
		if (nonneutral_mutations_count_ == nonneutral_mutation_capacity_)
		{
#ifdef __clang_analyzer__
			assert(nonneutral_mutation_capacity_ > 0);
#endif
			
			if (nonneutral_mutation_capacity_ < 32)
				nonneutral_mutation_capacity_ <<= 1;		// double the number of pointers we can hold
			else
				nonneutral_mutation_capacity_ += 16;
			
			nonneutral_mutations_ = (MutationIndex *)realloc(nonneutral_mutations_, nonneutral_mutation_capacity_ * sizeof(MutationIndex));
		}
		
		*(nonneutral_mutations_ + nonneutral_mutations_count_) = p_mutation_index;
		++nonneutral_mutations_count_;
	}
	
	void cache_nonneutral_mutations_REGIME_1();
	void cache_nonneutral_mutations_REGIME_2();
	void cache_nonneutral_mutations_REGIME_3();
	
	void check_nonneutral_mutation_cache();
	
	inline __attribute__((always_inline)) void beginend_nonneutral_pointers(const MutationIndex **p_mutptr_iter, const MutationIndex **p_mutptr_max, int32_t p_nonneutral_change_counter, int32_t p_nonneutral_regime)
	{
		if ((nonneutral_change_validation_ != p_nonneutral_change_counter) || (nonneutral_mutations_count_ == -1))
		{
			// If the nonneutral change counter has changed since we last validated, or our cache is invalid for other
			// reasons (most notably being a new mutation run that has not yet cached), validate it immediately
			nonneutral_change_validation_ = p_nonneutral_change_counter;
			
			switch (p_nonneutral_regime)
			{
				case 1: cache_nonneutral_mutations_REGIME_1(); break;
				case 2: cache_nonneutral_mutations_REGIME_2(); break;
				case 3: cache_nonneutral_mutations_REGIME_3(); break;
			}
			
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
			// PROFILING
			recached_run_ = true;
#endif
		}
		
#if DEBUG
		check_nonneutral_mutation_cache();
#endif
		
		// Return the requested pointers to allow the caller to iterate over the nonneutral mutation buffer
		*p_mutptr_iter = nonneutral_mutations_;
		*p_mutptr_max = nonneutral_mutations_ + nonneutral_mutations_count_;
	}
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	inline __attribute__((always_inline)) void tally_nonneutral_mutations(int64_t *p_mutation_count, int64_t *p_nonneutral_count, int64_t *p_recached_count)
	{
		*p_mutation_count += mutation_count_;
		
		if (nonneutral_mutations_count_ != -1)
			*p_nonneutral_count += nonneutral_mutations_count_;
		
		if (recached_run_)
		{
			(*p_recached_count)++;
			recached_run_ = false;
		}
	}
#endif	// defined(SLIMGUI) && (SLIMPROFILING == 1)
	
#endif	// SLIM_USE_NONNEUTRAL_CACHES
	
	// Memory usage tallying, for outputUsage()
	size_t MemoryUsageForMutationIndexBuffers(void);
	size_t MemoryUsageForNonneutralCaches(void);
	
	// Eidos_intrusive_ptr support
	inline __attribute__((always_inline)) uint32_t UseCount() const { return intrusive_ref_count_; }
	
	friend void Eidos_intrusive_ptr_add_ref(const MutationRun *p_value);
	friend void Eidos_intrusive_ptr_release(const MutationRun *p_value);
};

// Eidos_intrusive_ptr support
inline __attribute__((always_inline)) void Eidos_intrusive_ptr_add_ref(const MutationRun *p_value)
{
	++(p_value->intrusive_ref_count_);
}

inline __attribute__((always_inline)) void Eidos_intrusive_ptr_release(const MutationRun *p_value)
{
	if ((--(p_value->intrusive_ref_count_)) == 0)
	{
		// Do not delete; all MutationRun objects under Eidos_intrusive_ptr should have been allocated
		// by MutationRun::NewMutationRun(), so we return them to that pool here without destructing
		MutationRun::FreeMutationRun(const_cast<MutationRun *>(p_value));
	}
}

// We need MutationType below, but we can't include it at top because it requires MutationRun to be defined...
#include "mutation_type.h"

inline __attribute__((always_inline)) bool MutationRun::enforce_stack_policy_for_addition(slim_position_t p_position, MutationType *p_mut_type_ptr)
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
		return _EnforceStackPolicyForAddition(p_position, policy, p_mut_type_ptr->stack_group_);
	}
}

#endif /* __SLiM__mutation_run__ */
























