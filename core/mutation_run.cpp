//
//  mutation_run.cpp
//  SLiM
//
//  Created by Ben Haller on 11/29/16.
//  Copyright (c) 2016-2025 Benjamin C. Haller.  All rights reserved.
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


#include "mutation_run.h"

#include <vector>


// For doing bulk operations across all MutationRun objects; see header
int64_t MutationRun::sOperationID = 0;


MutationRun::MutationRun(void)
#ifdef DEBUG_LOCKS_ENABLED
	: mutrun_use_count_LOCK("mutrun_use_count_LOCK")
#endif
{
	// give it some initial capacity
	mutation_capacity_ = SLIM_MUTRUN_INITIAL_CAPACITY;
	mutations_ = (MutationIndex *)malloc(mutation_capacity_ * sizeof(MutationIndex));
	if (!mutations_)
		EIDOS_TERMINATION << "ERROR (MutationRun::MutationRun): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
}

MutationRun::~MutationRun(void)
{
	free(mutations_);
	
#if SLIM_USE_NONNEUTRAL_CACHES
	if (nonneutral_mutations_)
		free(nonneutral_mutations_);
#endif
}

#if 0
// linear search
bool MutationRun::contains_mutation(const Mutation *p_mut) const
{
	MutationIndex mutation_index = p_mut->BlockIndex();
	const MutationIndex *position = begin_pointer_const();
	const MutationIndex *end_position = end_pointer_const();
	
	for (; position != end_position; ++position)
		if (*position == mutation_index)
			return true;
	
	return false;
}
#else
// binary search
bool MutationRun::contains_mutation(const Mutation *p_mut) const
{
	MutationIndex mutation_index = p_mut->BlockIndex();
	slim_position_t position = p_mut->position_;
	int mut_count = size();
	const MutationIndex *mut_ptr = begin_pointer_const();
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	int mut_index;
	
	{
		// Find the requested position by binary search
		slim_position_t mut_pos;
		
		{
			int L = 0, R = mut_count - 1;
			
			do
			{
				if (L > R)
					return false;
				
				mut_index = (L + R) >> 1;	// overflow-safe because base positions have a max of 1000000000L
				mut_pos = (mut_block_ptr + mut_ptr[mut_index])->position_;
				
				if (mut_pos < position)
				{
					L = mut_index + 1;
					continue;
				}
				if (mut_pos > position)
				{
					R = mut_index - 1;
					continue;
				}
				
				// mut_pos == p_position
				break;
			}
			while (true);
		}
		
		// The mutation at mut_index is at p_position, but it may not be the only such
		// We check it first, then we check before it scanning backwards, and check after it scanning forwards
		if (mut_ptr[mut_index] == mutation_index)
			return true;
	}
	
	// backward & forward scan are shared by both code paths
	{
		slim_position_t back_scan = mut_index;
		
		while (back_scan > 0)
		{
			const MutationIndex scan_mut_index = mut_ptr[--back_scan];
			
			if ((mut_block_ptr + scan_mut_index)->position_ != position)
				break;
			if (scan_mut_index == mutation_index)
				return true;
		}
	}
	
	{
		slim_position_t forward_scan = mut_index;
		
		while (forward_scan < mut_count - 1)
		{
			const MutationIndex scan_mut_index = mut_ptr[++forward_scan];
			
			if ((mut_block_ptr + scan_mut_index)->position_ != position)
				break;
			if (scan_mut_index == mutation_index)
				return true;
		}
	}
	
	return false;
}
#endif

Mutation *MutationRun::mutation_with_type_and_position(MutationType *p_mut_type, slim_position_t p_position, slim_position_t p_last_position) const
{
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	int mut_count = size();
	const MutationIndex *mut_ptr = begin_pointer_const();
	int mut_index;
	
	if (p_position == 0)
	{
		// The marker is supposed to be at position 0.  This is a very common case, so we special-case it
		// to avoid an inefficient binary search.  Instead, we just look at the beginning.
		if (mut_count == 0)
			return nullptr;
		
		if ((mut_block_ptr + mut_ptr[0])->position_ > 0)
			return nullptr;
		
		if ((mut_block_ptr + mut_ptr[0])->mutation_type_ptr_ == p_mut_type)
			return (mut_block_ptr + mut_ptr[0]);
		
		mut_index = 0;	// drop through to forward scan
	}
	else if (p_position == p_last_position)
	{
		// The marker is supposed to be at the very end of the chromosome.  This is also a common case,
		// so we special-case it by starting at the last mutation in the haplosome.
		if (mut_count == 0)
			return nullptr;
		
		mut_index = mut_count - 1;
		
		if ((mut_block_ptr + mut_ptr[mut_index])->position_ < p_last_position)
			return nullptr;
		
		if ((mut_block_ptr + mut_ptr[mut_index])->mutation_type_ptr_ == p_mut_type)
			return (mut_block_ptr + mut_ptr[mut_index]);
		
		// drop through to backward scan
	}
	else
	{
		// Find the requested position by binary search
		slim_position_t mut_pos;
		
		{
			int L = 0, R = mut_count - 1;
			
			do
			{
				if (L > R)
					return nullptr;
				
				mut_index = (L + R) >> 1;	// overflow-safe because base positions have a max of 1000000000L
				mut_pos = (mut_block_ptr + mut_ptr[mut_index])->position_;
				
				if (mut_pos < p_position)
				{
					L = mut_index + 1;
					continue;
				}
				if (mut_pos > p_position)
				{
					R = mut_index - 1;
					continue;
				}
				
				// mut_pos == p_position
				break;
			}
			while (true);
		}
		
		// The mutation at mut_index is at p_position, but it may not be the only such
		// We check it first, then we check before it scanning backwards, and check after it scanning forwards
		if ((mut_block_ptr + mut_ptr[mut_index])->mutation_type_ptr_ == p_mut_type)
			return (mut_block_ptr + mut_ptr[mut_index]);
	}
	
	// backward & forward scan are shared by both code paths
	{
		slim_position_t back_scan = mut_index;
		
		while (back_scan > 0)
		{
			const MutationIndex scan_mut_index = mut_ptr[--back_scan];
			
			if ((mut_block_ptr + scan_mut_index)->position_ != p_position)
				break;
			if ((mut_block_ptr + scan_mut_index)->mutation_type_ptr_ == p_mut_type)
				return (mut_block_ptr + scan_mut_index);
		}
	}
	
	{
		slim_position_t forward_scan = mut_index;
		
		while (forward_scan < mut_count - 1)
		{
			const MutationIndex scan_mut_index = mut_ptr[++forward_scan];
			
			if ((mut_block_ptr + scan_mut_index)->position_ != p_position)
				break;
			if ((mut_block_ptr + scan_mut_index)->mutation_type_ptr_ == p_mut_type)
				return (mut_block_ptr + scan_mut_index);
		}
	}
	
	return nullptr;
}

const std::vector<Mutation *> *MutationRun::derived_mutation_ids_at_position(slim_position_t p_position) const
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("MutationRun::derived_mutation_ids_at_position(): usage of statics");
	
	static std::vector<Mutation *> return_vec;
	
	// First clear out whatever might be left over from last time
	return_vec.resize(0);
	
	// Then fill in all the mutation IDs at the given position.  We search backward from the end since usually we are called
	// when a new mutation has just been added to the end; this will be slow for addNew[Drawn]Mutation() and removeMutations(),
	// but fast for the other cases, such as new SLiM-generated mutations, which are much more common.
	const MutationIndex *begin_ptr = begin_pointer_const();
	const MutationIndex *end_ptr = end_pointer_const();
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	for (const MutationIndex *mut_ptr = end_ptr - 1; mut_ptr >= begin_ptr; --mut_ptr)
	{
		Mutation *mut = mut_block_ptr + *mut_ptr;
		slim_position_t mut_position = mut->position_;
		
		if (mut_position == p_position)
			return_vec.emplace_back(mut);
		else if (mut_position < p_position)
			break;
	}
	
	return &return_vec;
}

void MutationRun::_RemoveFixedMutations(void)
{
	// Mutations that have fixed, and are thus targeted for removal, have had their state_ set to kFixedAndSubstituted.
	// That is done only when convertToSubstitution == T, so we don't need to check that flag here.
	
	// We don't use begin_pointer() / end_pointer() here, because we actually want to modify the MutationRun even
	// though it is shared by multiple Haplosomes; this is an exceptional case, so we go around our safeguards.
	MutationIndex *haplosome_iter = mutations_;
	MutationIndex *haplosome_backfill_iter = nullptr;
	MutationIndex *haplosome_max = mutations_ + mutation_count_;
	Mutation *mutation_block_ptr = gSLiM_Mutation_Block;
	
	// haplosome_iter advances through the mutation list; for each entry it hits, the entry is either fixed (skip it) or not fixed
	// (copy it backward to the backfill pointer).  We do this with two successive loops; the first knows that no mutation has
	// yet been skipped, whereas the second knows that at least one mutation has been.
	while (haplosome_iter != haplosome_max)
	{
		if ((mutation_block_ptr + (*haplosome_iter++))->state_ != MutationState::kFixedAndSubstituted)
			continue;
		
		// Fixed mutation; we want to omit it, so we skip it in haplosome_backfill_iter and transition to the second loop
		haplosome_backfill_iter = haplosome_iter - 1;
		break;
	}
	
#ifdef __clang_analyzer__
	// the static analyzer doesn't understand the way the loop above drops through to the loop below
	// this assert is not always true, but it is true whenever (haplosome_iter != haplosome_max) at this point
	assert(haplosome_backfill_iter);
#endif
	
	while (haplosome_iter != haplosome_max)
	{
		MutationIndex mutation_index = *haplosome_iter;
		
		if ((mutation_block_ptr + mutation_index)->state_ != MutationState::kFixedAndSubstituted)
		{
			// Unfixed mutation; we want to keep it, so we copy it backward and advance our backfill pointer as well as haplosome_iter
			*haplosome_backfill_iter = mutation_index;
			
			++haplosome_backfill_iter;
			++haplosome_iter;
		}
		else
		{
			// Fixed mutation; we want to omit it, so we just advance our pointer
			++haplosome_iter;
		}
	}
	
	// excess mutations at the end have been copied back already; we just adjust mutation_count_ and forget about them
	if (haplosome_backfill_iter != nullptr)
	{
		mutation_count_ -= (haplosome_iter - haplosome_backfill_iter);
		
#if SLIM_USE_NONNEUTRAL_CACHES
		// invalidate the nonneutral mutation cache
		nonneutral_mutations_count_ = -1;
#endif
	}
}

bool MutationRun::_EnforceStackPolicyForAddition(slim_position_t p_position, MutationStackPolicy p_policy, int64_t p_stack_group)
{
	MutationIndex *begin_ptr = begin_pointer();
	MutationIndex *end_ptr = end_pointer();
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	if (p_policy == MutationStackPolicy::kKeepFirst)
	{
		// If the first mutation occurring at a site is kept, then we need to check for an existing mutation of this stacking group
		// We scan in reverse order, because usually we're adding mutations on the end with emplace_back()
		for (MutationIndex *mut_ptr = end_ptr - 1; mut_ptr >= begin_ptr; --mut_ptr)
		{
			Mutation *mut = mut_block_ptr + *mut_ptr;
			slim_position_t mut_position = mut->position_;
			
			if ((mut_position == p_position) && (mut->mutation_type_ptr_->stack_group_ == p_stack_group))
				return false;
			else if (mut_position < p_position)
				return true;
		}
		
		return true;
	}
	else if (p_policy == MutationStackPolicy::kKeepLast)
	{
		// If the last mutation occurring at a site is kept, then we need to check for existing mutations of this type
		// We scan in reverse order, because usually we're adding mutations on the end with emplace_back()
		MutationIndex *first_match_ptr = nullptr;
		
		for (MutationIndex *mut_ptr = end_ptr - 1; mut_ptr >= begin_ptr; --mut_ptr)
		{
			Mutation *mut = mut_block_ptr + *mut_ptr;
			slim_position_t mut_position = mut->position_;
			
			if ((mut_position == p_position) && (mut->mutation_type_ptr_->stack_group_ == p_stack_group))
				first_match_ptr = mut_ptr;	// set repeatedly as we scan backwards, until we exit
			else if (mut_position < p_position)
				break;
		}
		
		// If we found any, we now scan forward and remove them, in anticipation of the new mutation being added
		if (first_match_ptr)
		{
			MutationIndex *replace_ptr = first_match_ptr;	// replace at the first match position
			MutationIndex *mut_ptr = first_match_ptr + 1;	// we know the initial position needs removal, so start at the next
			
			for ( ; mut_ptr < end_ptr; ++mut_ptr)
			{
				MutationIndex mut_index = *mut_ptr;
				Mutation *mut = mut_block_ptr + mut_index;
				slim_position_t mut_position = mut->position_;
				
				if ((mut_position == p_position) && (mut->mutation_type_ptr_->stack_group_ == p_stack_group))
				{
					// The current scan position is a mutation that needs to be removed, so scan forward to skip copying it backward
					continue;
				}
				else
				{
					// The current scan position is a valid mutation, so we copy it backwards
					*(replace_ptr++) = mut_index;
				}
			}
			
			// excess mutations at the end have been copied back already; we just adjust mutation_count_ and forget about them
			set_size(size() - (int)(mut_ptr - replace_ptr));
		}
		
		return true;
	}
	else
		EIDOS_TERMINATION << "ERROR (MutationRun::_EnforceStackPolicyForAddition): (internal error) invalid policy." << EidosTerminate();
}

void MutationRun::split_run(MutationRun **p_first_half, MutationRun **p_second_half, slim_position_t p_split_first_position, MutationRunContext &p_mutrun_context) const
{
	MutationRun *first_half = NewMutationRun(p_mutrun_context);
	MutationRun *second_half = NewMutationRun(p_mutrun_context);
	int32_t second_half_start;
	
	for (second_half_start = 0; second_half_start < mutation_count_; ++second_half_start)
		if ((gSLiM_Mutation_Block + mutations_[second_half_start])->position_ >= p_split_first_position)
			break;
	
	if (second_half_start > 0)
		first_half->emplace_back_bulk(mutations_, second_half_start);
	
	if (second_half_start < mutation_count_)
		second_half->emplace_back_bulk(mutations_ + second_half_start, mutation_count_ - second_half_start);
	
	*p_first_half = first_half;
	*p_second_half = second_half;
}


#if SLIM_USE_NONNEUTRAL_CACHES

void MutationRun::cache_nonneutral_mutations_REGIME_1() const
{
	//
	//	Regime 1 means there are no mutationEffect() callbacks at all, so neutrality can be assessed
	//	simply by looking at selection_coeff_ != 0.0.  The mutation type is irrelevant.
	//
	zero_out_nonneutral_buffer();
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	// loop through mutations and copy the non-neutral ones into our buffer, resizing as needed
	for (int32_t bufindex = 0; bufindex < mutation_count_; ++bufindex)
	{
		MutationIndex mutindex = mutations_[bufindex];
		
		if ((mut_block_ptr + mutindex)->selection_coeff_ != 0.0)
			add_to_nonneutral_buffer(mutindex);
	}
}

void MutationRun::cache_nonneutral_mutations_REGIME_2() const
{
	//
	//	Regime 2 means the only mutationEffect() callbacks are (a) constant-effect, (b) neutral (i.e.,
	//	make their mutation type become neutral), and (c) global (i.e. apply to all subpopulations).
	//	Here neutrality is assessed by first consulting the set_neutral_by_global_active_callback
	//	flag of MutationType, which is set up by RecalculateFitness() for us.  If that is true,
	//	the mutation is neutral; if false, selection_coeff_ is reliable.  Note the code below uses
	//	the exact way that the C operator && works to implement this order of checks.
	//
	zero_out_nonneutral_buffer();
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	// loop through mutations and copy the non-neutral ones into our buffer, resizing as needed
	for (int32_t bufindex = 0; bufindex < mutation_count_; ++bufindex)
	{
		MutationIndex mutindex = mutations_[bufindex];
		Mutation *mutptr = mut_block_ptr + mutindex;
		
		// The result of && is not order-dependent, but the first condition is checked first.
		// I expect many mutations would fail the first test (thus short-circuiting), whereas
		// few would fail the second test (i.e. actually be 0.0) in a QTL model.
		if ((!mutptr->mutation_type_ptr_->set_neutral_by_global_active_callback_) && (mutptr->selection_coeff_ != 0.0))
			add_to_nonneutral_buffer(mutindex);
	}
}

void MutationRun::cache_nonneutral_mutations_REGIME_3() const
{
	//
	//	Regime 3 means that there are mutationEffect() callbacks beyond the constant neutral global
	//	callbacks of regime 2, so if a mutation's muttype is subject to any mutationEffect() callbacks
	//	at all, whether active or not, that mutation must be considered to be non-neutral (because
	//	a rogue callback could enable/disable other callbacks).  This is determined by consulting
	//	the subject_to_mutationEffect_callback flag of MutationType, set up by RecalculateFitness()
	//	for us.  If that flag is not set, then the selection_coeff_ is reliable as usual.
	//
	zero_out_nonneutral_buffer();
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	// loop through mutations and copy the non-neutral ones into our buffer, resizing as needed
	for (int32_t bufindex = 0; bufindex < mutation_count_; ++bufindex)
	{
		MutationIndex mutindex = mutations_[bufindex];
		Mutation *mutptr = mut_block_ptr + mutindex;
		
		// The result of || is not order-dependent, but the first condition is checked first.
		// I have reordered this to put the fast test first; or I'm guessing it's the fast test.
		if ((mutptr->selection_coeff_ != 0.0) || (mutptr->mutation_type_ptr_->subject_to_mutationEffect_callback_))
			add_to_nonneutral_buffer(mutindex);
	}
}

void MutationRun::check_nonneutral_mutation_cache() const
{
	if (!nonneutral_mutations_)
		EIDOS_TERMINATION << "ERROR (MutationRun::check_nonneutral_mutation_cache): (internal error) cache not allocated." << EidosTerminate();
	if (nonneutral_mutations_count_ == -1)
		EIDOS_TERMINATION << "ERROR (MutationRun::check_nonneutral_mutation_cache): (internal error) unvalidated cache." << EidosTerminate();
	if (nonneutral_mutations_count_ > nonneutral_mutation_capacity_)
		EIDOS_TERMINATION << "ERROR (MutationRun::check_nonneutral_mutation_cache): (internal error) cache size exceeds cache capacity." << EidosTerminate();
	
	// Check for correctness in regime 1.  Now that we have three regimes, this isn't really worth maintaining;
	// it really just replicates the above logic exactly, so it is not a very effective cross-check.
	
	/*
	int32_t cache_index = 0;
	
	for (int32_t bufindex = 0; bufindex < mutation_count_; ++bufindex)
	{
		MutationIndex mutindex = mutations_[bufindex];
		Mutation *mutptr = gSLiM_Mutation_Block + mutindex;
		
		if (mutptr->selection_coeff_ != 0.0)
			if (*(nonneutral_mutations_ + cache_index++) != mutindex)
				EIDOS_TERMINATION << "ERROR (MutationRun::check_nonneutral_mutation_cache_REGIME_1): (internal error) unsynchronized cache." << EidosTerminate();
	}
	 */
}

#endif

// Shorthand for clear(), then copy_from_run(p_mutations_to_set), then insert_sorted_mutation() on every
// mutation in p_mutations_to_add, with checks with enforce_stack_policy_for_addition().  The point of
// this is speed: like HaplosomeCloned(), we can merge the new mutations in much faster if we do it in
// bulk.  Note that p_mutations_to_set and p_mutations_to_add must both be sorted by position.
void MutationRun::clear_set_and_merge(const MutationRun &p_mutations_to_set, std::vector<MutationIndex> &p_mutations_to_add)
{
	// first, clear all mutations out of the receiver
	clear();
	
	// handle the cases with no mutations in one or the other given run, so we can assume >= 1 mutations below
	int mut_to_set_count = p_mutations_to_set.size();
	int mut_to_add_count = (int)p_mutations_to_add.size();
	
	if (mut_to_add_count == 0)
	{
		copy_from_run(p_mutations_to_set);
		return;
	}
	
	if (mut_to_set_count == 0)
	{
		copy_from_vector(p_mutations_to_add);
		return;
	}
	
	// assume that all mutations will be added, and adjust capacity accordingly
	if (mut_to_set_count + mut_to_add_count > mutation_capacity_)
	{
		// See emplace_back for comments on our capacity policy
		do
		{
			if (mutation_capacity_ < 32)
				mutation_capacity_ <<= 1;		// double the number of pointers we can hold
			else
				mutation_capacity_ += 16;
		}
		while (mut_to_set_count + mut_to_add_count > mutation_capacity_);
		
		mutations_ = (MutationIndex *)realloc(mutations_, mutation_capacity_ * sizeof(MutationIndex));
		if (!mutations_)
			EIDOS_TERMINATION << "ERROR (MutationRun::clear_set_and_merge): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	}
	
	// then interleave mutations together, effectively setting p_mutations_to_set and then adding in p_mutations_to_add
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	const MutationIndex *mutation_iter		= p_mutations_to_add.data();
	const MutationIndex *mutation_iter_max	= mutation_iter + p_mutations_to_add.size();
	MutationIndex mutation_iter_mutation_index = *mutation_iter;
	slim_position_t mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
	
	const MutationIndex *parent_iter		= p_mutations_to_set.begin_pointer_const();
	const MutationIndex *parent_iter_max	= p_mutations_to_set.end_pointer_const();
	MutationIndex parent_iter_mutation_index = *parent_iter;
	slim_position_t parent_iter_pos = (mut_block_ptr + parent_iter_mutation_index)->position_;
	
	// this loop runs while we are still interleaving mutations from both sources
	do
	{
		if (parent_iter_pos <= mutation_iter_pos)
		{
			// we have a parent mutation that comes first, so copy it
			emplace_back(*parent_iter);
			
			parent_iter++;
			if (parent_iter == parent_iter_max)
				break;
			
			parent_iter_mutation_index = *parent_iter;
			parent_iter_pos = (mut_block_ptr + parent_iter_mutation_index)->position_;
		}
		else
		{
			// we have a new mutation to add, which we know is not already present; check the stacking policy
			if (enforce_stack_policy_for_addition(mutation_iter_pos, (mut_block_ptr + mutation_iter_mutation_index)->mutation_type_ptr_))
				emplace_back(mutation_iter_mutation_index);
			
			mutation_iter++;
			if (mutation_iter == mutation_iter_max)
				break;
			
			mutation_iter_mutation_index = *mutation_iter;
			mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
		}
	}
	while (true);
	
	// one source is exhausted, but there are still mutations left in the other source
	while (parent_iter != parent_iter_max)
	{
		emplace_back(*parent_iter);
		parent_iter++;
	}
	
	while (mutation_iter != mutation_iter_max)
	{
		mutation_iter_mutation_index = *mutation_iter;
		mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
		
		if (enforce_stack_policy_for_addition(mutation_iter_pos, (mut_block_ptr + mutation_iter_mutation_index)->mutation_type_ptr_))
			emplace_back(mutation_iter_mutation_index);
		
		mutation_iter++;
	}
}

size_t MutationRun::MemoryUsageForMutationIndexBuffers(void) const
{
	return mutation_capacity_ * sizeof(MutationIndex);
}

size_t MutationRun::MemoryUsageForNonneutralCaches(void) const
{
	return nonneutral_mutation_capacity_ * sizeof(MutationIndex);
}















































