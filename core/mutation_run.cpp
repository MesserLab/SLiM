//
//  mutation_run.cpp
//  SLiM
//
//  Created by Ben Haller on 11/29/16.
//  Copyright (c) 2016-2026 Benjamin C. Haller.  All rights reserved.
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
#include "species.h"
#include "mutation_block.h"

#include <vector>


// For doing bulk operations across all MutationRun objects; see header
slim_operation_id_t MutationRun::sOperationID = 0;


std::ostream& operator<<(std::ostream& p_out, TraitCalculationRegime p_regime)
{
	switch (p_regime)
	{
		case TraitCalculationRegime::kUndefined:							p_out << "kUndefined";							break;
		case TraitCalculationRegime::kPureNeutral:							p_out << "kPureNeutral";						break;
		case TraitCalculationRegime::kNoActiveCallbacks:					p_out << "kNoActiveCallbacks";					break;
		case TraitCalculationRegime::kAllGlobalNeutralCallbacks:			p_out << "kAllGlobalNeutralCallbacks";			break;
		case TraitCalculationRegime::kNonNeutralCallbacks:					p_out << "kNonNeutralCallbacks";				break;
		case TraitCalculationRegime::kAllNonNeutralNoIndDomCaches:			p_out << "kAllNonNeutralNoIndDomCaches";		break;
		case TraitCalculationRegime::kAllNonNeutralWithIndDomCaches:		p_out << "kAllNonNeutralWithIndDomCaches";		break;
		case TraitCalculationRegime::kHaploidAllNonNeutralNoCallbacks:		p_out << "kHaploidAllNonNeutralNoCallbacks";	break;
		case TraitCalculationRegime::kHaploidNoCallbacks:					p_out << "kHaploidNoCallbacks";					break;
		case TraitCalculationRegime::kHaploidAllNonNeutralWithCallbacks:	p_out << "kHaploidAllNonNeutralWithCallbacks";	break;
		case TraitCalculationRegime::kHaploidWithCallbacks:					p_out << "kHaploidWithCallbacks";				break;
	}
	
	return p_out;
}

std::string RegimeDescription(TraitCalculationRegime p_regime)
{
	switch (p_regime)
	{
		case TraitCalculationRegime::kUndefined:							return "*** undefined ***";
		case TraitCalculationRegime::kPureNeutral:							return "all mutations effectively neutral";
		case TraitCalculationRegime::kNoActiveCallbacks:					return "no mutationEffect() callbacks";
		case TraitCalculationRegime::kAllGlobalNeutralCallbacks:			return "constant neutral mutationEffect() callbacks only";
		case TraitCalculationRegime::kNonNeutralCallbacks:					return "unpredictable mutationEffect() callbacks present";
		case TraitCalculationRegime::kAllNonNeutralNoIndDomCaches:			return "most/all mutations nonneutral; no independent dominance";
		case TraitCalculationRegime::kAllNonNeutralWithIndDomCaches:		return "most/all mutations nonneutral; independent dominance";
		case TraitCalculationRegime::kHaploidAllNonNeutralNoCallbacks:		return "haploid, all nonneutral mutations, no callbacks";
		case TraitCalculationRegime::kHaploidNoCallbacks:					return "haploid, some neutral mutations, no callbacks";
		case TraitCalculationRegime::kHaploidAllNonNeutralWithCallbacks:	return "haploid, all nonneutral mutations, some callbacks";
		case TraitCalculationRegime::kHaploidWithCallbacks:					return "haploid, some neutral mutations, some callbacks";
	}
}


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
	
#if SLIM_USE_NONNEUTRAL_CACHES()
	if (nonneutral_cache_)
	{
		free(nonneutral_cache_);
		
#if DEBUG
		nonneutral_cache_ = nullptr;
		internal_cache_count_DEBUG_ = static_cast<MutRunInternalCacheIndex>(-1);
#endif
	}
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
	MutationBlock *mutation_block = p_mut->mutation_type_ptr_->mutation_block_;
	MutationIndex mutation_index = mutation_block->IndexInBlock(p_mut);
	slim_position_t position = p_mut->position_;
	int mut_count = size();
	const MutationIndex *mut_ptr = begin_pointer_const();
	Mutation *mut_block_ptr = mutation_block->mutation_buffer_;
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
	Mutation *mut_block_ptr = p_mut_type->mutation_block_->mutation_buffer_;
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

const std::vector<Mutation *> *MutationRun::derived_mutation_ids_at_position(Mutation *p_mut_block_ptr, slim_position_t p_position) const
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
	
	for (const MutationIndex *mut_ptr = end_ptr - 1; mut_ptr >= begin_ptr; --mut_ptr)
	{
		Mutation *mut = p_mut_block_ptr + *mut_ptr;
		slim_position_t mut_position = mut->position_;
		
		if (mut_position == p_position)
			return_vec.emplace_back(mut);
		else if (mut_position < p_position)
			break;
	}
	
	return &return_vec;
}

void MutationRun::_RemoveFixedMutations(Mutation *p_mut_block_ptr)
{
	// Mutations that have fixed, and are thus targeted for removal, have had their state_ set to kFixedAndSubstituted.
	// That is done only when convertToSubstitution == T, so we don't need to check that flag here.
	
	// We don't use begin_pointer() / end_pointer() here, because we actually want to modify the MutationRun even
	// though it is shared by multiple Haplosomes; this is an exceptional case, so we go around our safeguards.
	MutationIndex *haplosome_iter = mutations_;
	MutationIndex *haplosome_backfill_iter = nullptr;
	MutationIndex *haplosome_max = mutations_ + mutation_count_;
	
	// haplosome_iter advances through the mutation list; for each entry it hits, the entry is either fixed (skip it) or not fixed
	// (copy it backward to the backfill pointer).  We do this with two successive loops; the first knows that no mutation has
	// yet been skipped, whereas the second knows that at least one mutation has been.
	while (haplosome_iter != haplosome_max)
	{
		if ((p_mut_block_ptr + (*haplosome_iter++))->state_ != MutationState::kFixedAndSubstituted)
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
		
		if ((p_mut_block_ptr + mutation_index)->state_ != MutationState::kFixedAndSubstituted)
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
		
#if SLIM_USE_NONNEUTRAL_CACHES()
		// invalidate the nonneutral mutation cache
		if (nonneutral_cache_)
			nonneutral_cache_->nonneutral_count_ = -1;
#endif
	}
}

bool MutationRun::_EnforceStackPolicyForAddition(Mutation *p_mut_block_ptr, slim_position_t p_position, MutationStackPolicy p_policy, int64_t p_stack_group)
{
	MutationIndex *begin_ptr = begin_pointer();
	MutationIndex *end_ptr = end_pointer();
	
	if (p_policy == MutationStackPolicy::kKeepFirst)
	{
		// If the first mutation occurring at a site is kept, then we need to check for an existing mutation of this stacking group
		// We scan in reverse order, because usually we're adding mutations on the end with emplace_back()
		for (MutationIndex *mut_ptr = end_ptr - 1; mut_ptr >= begin_ptr; --mut_ptr)
		{
			Mutation *mut = p_mut_block_ptr + *mut_ptr;
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
			Mutation *mut = p_mut_block_ptr + *mut_ptr;
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
				Mutation *mut = p_mut_block_ptr + mut_index;
				slim_position_t mut_position = mut->position_;
				
				if ((mut_position == p_position) && (mut->mutation_type_ptr_->stack_group_ == p_stack_group))
				{
					// The current scan position is a mutation that needs to be removed, so scan forward to skip copying it backward
					// BCH 2/14/2026: this mutation would still be present in the tree sequence, so it needs to be retained by the species
					mut->mutation_type_ptr_->species_.NotifyMutationRemoved(mut);
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

void MutationRun::split_run(Mutation *p_mut_block_ptr, MutationRun **p_first_half, MutationRun **p_second_half, slim_position_t p_split_first_position, MutationRunContext &p_mutrun_context) const
{
	MutationRun *first_half = NewMutationRun(p_mutrun_context);
	MutationRun *second_half = NewMutationRun(p_mutrun_context);
	int32_t second_half_start;
	
	for (second_half_start = 0; second_half_start < mutation_count_; ++second_half_start)
		if ((p_mut_block_ptr + mutations_[second_half_start])->position_ >= p_split_first_position)
			break;
	
	if (second_half_start > 0)
		first_half->emplace_back_bulk(mutations_, second_half_start);
	
	if (second_half_start < mutation_count_)
		second_half->emplace_back_bulk(mutations_ + second_half_start, mutation_count_ - second_half_start);
	
	*p_first_half = first_half;
	*p_second_half = second_half;
}


#if SLIM_USE_NONNEUTRAL_CACHES()

void MutationRun::cache_nonneutral_mutations_REGIME_kPureNeutral(
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
													  NonNeutralValidationMetrics *metrics
#endif
													  ) const
{
	//
	//	kPureNeutral means there are no genetic effects at all, even considering callbacks, so we can just empty
	//	the non-neutral cache; this is often called "pure-neutral" in SLiM's code.  Note that this means that
	//	the genetics (including callbacks) are neutral for the trait; it does NOT mean that the trait is neutral
	//	for the individual, particularly since baseline and individual offsets are still included in the trait
	//	calculations.  In this regime, if the nonneutral cache is unallocated, we leave it unallocated; there is
	//	no need for it.  If it is allocated, we empty it but leave it allocated; this state means that the model
	//	has been non-pure-neutral in the past, and so it might return to being non-pure-neutral in the future,
	//	so we want to avoid alloc/free thrash.  This will not always be the right choice; we could decrease our
	//	memory footprint by freeing the buffers here, which would be a win if we stay pure-neutral for a long
	//	time.  But I think having somewhat higher memory usage is preferable to the possibility of massive alloc
	//	thrash in models that, for example, have mostly empty genomes with occasional sweeps -- perhaps a common
	//	mode for tree-sequence recording models!
	//
	zero_out_nonneutral_cache_NOALLOC();
	
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
	metrics->mutations_omitted_ += mutation_count_;
#endif
}

void MutationRun::cache_nonneutral_mutations_REGIME_kNoActiveCallbacks(Mutation *p_mut_block_ptr, MutRunInternalCacheIndex inddom_cache_count
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
													  , NonNeutralValidationMetrics *metrics
#endif
													  ) const
{
#if DEBUG
	if ((internal_cache_count_DEBUG_ != static_cast<MutRunInternalCacheIndex>(-1)) && (inddom_cache_count != internal_cache_count_DEBUG_))
		EIDOS_TERMINATION << "ERROR (MutationRun::cache_nonneutral_mutations_REGIME_kNoActiveCallbacks): (internal error) inddom_cache_count != internal_cache_count_DEBUG_." << EidosTerminate(nullptr);
#endif
	
	//
	//	kNoActiveCallbacks means there are no active mutationEffect() callbacks at all, so neutrality can be
	//	assessed simply by looking at whether the mutation itself is neutral.  The mutation type is irrelevant.
	//
	zero_out_nonneutral_cache(inddom_cache_count);
	
	// loop through mutations and copy the non-neutral ones into our buffer, resizing as needed
	// because access to the nonneutral mutation buffer is complex and slow, we manage it internally here
	MutationIndex *mutation_buffer = nonneutral_mutation_buffer(inddom_cache_count);
	int32_t buffer_capacity = nonneutral_cache_->nonneutral_capacity_;
	int32_t buffer_count = nonneutral_cache_->nonneutral_count_;
	
	for (int32_t bufindex = 0; bufindex < mutation_count_; ++bufindex)
	{
		MutationIndex mutindex = mutations_[bufindex];
		Mutation *mutptr = p_mut_block_ptr + mutindex;
		
		if (!mutptr->is_neutral_for_all_traits_)
		{
			if (buffer_count == buffer_capacity)
			{
				// expand the buffer and re-fetch our local information about it
				expand_nonneutral_buffer(inddom_cache_count);
				mutation_buffer = nonneutral_mutation_buffer(inddom_cache_count);
				buffer_capacity = nonneutral_cache_->nonneutral_capacity_;
			}
			
			*(mutation_buffer + buffer_count) = mutindex;
			++buffer_count;
			
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
			metrics->mutations_cached_++;
#endif
		}
		else
		{
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
			metrics->mutations_omitted_++;
#endif
		}
	}
	
	nonneutral_cache_->nonneutral_count_ = buffer_count;
}

void MutationRun::cache_nonneutral_mutations_REGIME_kAllGlobalNeutralCallbacks(Mutation *p_mut_block_ptr, MutRunInternalCacheIndex inddom_cache_count
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
													  , NonNeutralValidationMetrics *metrics
#endif
													  ) const
{
#if DEBUG
	if ((internal_cache_count_DEBUG_ != static_cast<MutRunInternalCacheIndex>(-1)) && (inddom_cache_count != internal_cache_count_DEBUG_))
		EIDOS_TERMINATION << "ERROR (MutationRun::cache_nonneutral_mutations_REGIME_kAllGlobalNeutralCallbacks): (internal error) inddom_cache_count != internal_cache_count_DEBUG_." << EidosTerminate(nullptr);
#endif
	
	//
	//	kAllGlobalNeutralCallbacks means the only mutationEffect() callbacks are (a) constant-effect,
	//	(b) neutral (i.e., make their mutation type become neutral), and (c) global (i.e. apply to all
	//	subpops).  Here neutrality is assessed by first consulting the subject_to_mutationEffect_callback_
	//	flag of MutationType; if it is set, the mutation is neutral because the callback is known to be
	//	global-neutral.  Otherwise, the mutation's neutral flag is reliable.
	//
	zero_out_nonneutral_cache(inddom_cache_count);
	
	// loop through mutations and copy the non-neutral ones into our buffer, resizing as needed
	// because access to the nonneutral mutation buffer is complex and slow, we manage it internally here
	MutationIndex *mutation_buffer = nonneutral_mutation_buffer(inddom_cache_count);
	int32_t buffer_capacity = nonneutral_cache_->nonneutral_capacity_;
	int32_t buffer_count = nonneutral_cache_->nonneutral_count_;
	
	for (int32_t bufindex = 0; bufindex < mutation_count_; ++bufindex)
	{
		MutationIndex mutindex = mutations_[bufindex];
		Mutation *mutptr = p_mut_block_ptr + mutindex;
		
		// The result of && is not order-dependent, but the first condition is checked first.
		// I expect many mutations would fail the first test (thus short-circuiting), whereas
		// few would fail the second test (i.e. actually be neutral) in a model in this regime.
		if ((!mutptr->mutation_type_ptr_->subject_to_mutationEffect_callback_) && !mutptr->is_neutral_for_all_traits_)
		{
			if (buffer_count == buffer_capacity)
			{
				// expand the buffer and re-fetch our local information about it
				expand_nonneutral_buffer(inddom_cache_count);
				mutation_buffer = nonneutral_mutation_buffer(inddom_cache_count);
				buffer_capacity = nonneutral_cache_->nonneutral_capacity_;
			}
			
			*(mutation_buffer + buffer_count) = mutindex;
			++buffer_count;
			
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
			metrics->mutations_cached_++;
#endif
		}
		else
		{
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
			metrics->mutations_omitted_++;
#endif
		}
	}
	
	nonneutral_cache_->nonneutral_count_ = buffer_count;
}

void MutationRun::cache_nonneutral_mutations_REGIME_kNonNeutralCallbacks(Mutation *p_mut_block_ptr, MutRunInternalCacheIndex inddom_cache_count
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
													  , NonNeutralValidationMetrics *metrics
#endif
													  ) const
{
#if DEBUG
	if ((internal_cache_count_DEBUG_ != static_cast<MutRunInternalCacheIndex>(-1)) && (inddom_cache_count != internal_cache_count_DEBUG_))
		EIDOS_TERMINATION << "ERROR (MutationRun::cache_nonneutral_mutations_REGIME_kNonNeutralCallbacks): (internal error) inddom_cache_count != internal_cache_count_DEBUG_." << EidosTerminate(nullptr);
#endif
	
	//
	//	kNonNeutralCallbacks means that there are active mutationEffect() callbacks beyond the constant neutral
	//	global callbacks of regime 2 -- but those non-global-neutral callbacks might not affect the mutation.
	//	So we first consult subject_to_mutationEffect_callback_ to find out if a callback of any kind affects
	//	the mutation.  If that is true, then we consult subject_to_non_global_neutral_callback_; if that is
	//	true, the mutation is affected by a nonneutral callback, which must be called even if it is overridden
	//	by a global-neutral callback (it might have side effects).  If subject_to_non_global_neutral_callback_
	//	is false, we know the mutation is rendered neutral by a global-neutral callback.  And if the test
	//	of subject_to_mutationEffect_callback_ was false, the mutation's neutral flag is reliable.
	//
	zero_out_nonneutral_cache(inddom_cache_count);
	
	// loop through mutations and copy the non-neutral ones into our buffer, resizing as needed
	// because access to the nonneutral mutation buffer is complex and slow, we manage it internally here
	MutationIndex *mutation_buffer = nonneutral_mutation_buffer(inddom_cache_count);
	int32_t buffer_capacity = nonneutral_cache_->nonneutral_capacity_;
	int32_t buffer_count = nonneutral_cache_->nonneutral_count_;
	
	for (int32_t bufindex = 0; bufindex < mutation_count_; ++bufindex)
	{
		MutationIndex mutindex = mutations_[bufindex];
		Mutation *mutptr = p_mut_block_ptr + mutindex;
		MutationType *muttypeptr = mutptr->mutation_type_ptr_;
		
		if (muttypeptr->subject_to_mutationEffect_callback_)
		{
			if (muttypeptr->subject_to_non_global_neutral_callback_)
			{
				if (buffer_count == buffer_capacity)
				{
					// expand the buffer and re-fetch our local information about it
					expand_nonneutral_buffer(inddom_cache_count);
					mutation_buffer = nonneutral_mutation_buffer(inddom_cache_count);
					buffer_capacity = nonneutral_cache_->nonneutral_capacity_;
				}
				
				*(mutation_buffer + buffer_count) = mutindex;
				++buffer_count;
				
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
				metrics->mutations_cached_++;
#endif
			}
			else
			{
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
				metrics->mutations_omitted_++;
#endif
			}
		}
		else
		{
			if (!mutptr->is_neutral_for_all_traits_)
			{
				if (buffer_count == buffer_capacity)
				{
					// expand the buffer and re-fetch our local information about it
					expand_nonneutral_buffer(inddom_cache_count);
					mutation_buffer = nonneutral_mutation_buffer(inddom_cache_count);
					buffer_capacity = nonneutral_cache_->nonneutral_capacity_;
				}
				
				*(mutation_buffer + buffer_count) = mutindex;
				++buffer_count;
				
	#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
				metrics->mutations_cached_++;
	#endif
			}
			else
			{
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
				metrics->mutations_omitted_++;
#endif
			}
		}
	}
	
	nonneutral_cache_->nonneutral_count_ = buffer_count;
}

void MutationRun::cache_nonneutral_mutations_REGIME_kAllNonNeutralNoIndDomCaches(
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
													  NonNeutralValidationMetrics *metrics
#endif
													  ) const
{
	//
	//	kAllNonNeutralNoIndDomCaches means that all mutations are deemed nonneutral, and so we will not set up
	//	nonneutral mutation buffers at all, since they'd just be a copy of the main mutation buffers; instead,
	//	we will just use the main mutation buffers for trait calculations.  In addition, in regime 4 there is no
	//	need for independent-dominance caches, so we actually don't need nonneutral caches at all.  If they are
	//	allocated we'll leave them allocated, to avoid thrash, but if they're not, we won't create them.  This
	//	is actually the same as regime 0, as far as the empty nonneutral mutation buffers generated, but is
	//	distinguished from it for conceptual clarity.
	//
	zero_out_nonneutral_cache_NOALLOC();
	
	// mark the nonneutral cache with a special value that indicates this state.  It has to be a special count
	// value; the capacity can't be messed with, since it might be a leftover from a previous use of the cache.
	// The marker value used is intended to be likely to produce a crash if it is ever used as a count.
	if (nonneutral_cache_)
		nonneutral_cache_->nonneutral_count_ = SLIM_MUTRUN_USE_MAIN_BUFFER;
	
	// we consider the mutations in the main buffer to be "cached" in this case
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
	metrics->mutations_cached_ += mutation_count_;
#endif
}

#if SLIM_USE_INDEPENDENT_DOMINANCE_CACHES()
void MutationRun::cache_nonneutral_mutations_REGIME_kAllNonNeutralWithIndDomCaches(MutRunInternalCacheIndex inddom_cache_count
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
													  , NonNeutralValidationMetrics *metrics
#endif
													  ) const
{
#if DEBUG
	if ((internal_cache_count_DEBUG_ != static_cast<MutRunInternalCacheIndex>(-1)) && (inddom_cache_count != internal_cache_count_DEBUG_))
		EIDOS_TERMINATION << "ERROR (MutationRun::cache_nonneutral_mutations_REGIME_kAllNonNeutralWithIndDomCaches): (internal error) inddom_cache_count != internal_cache_count_DEBUG_." << EidosTerminate(nullptr);
#endif
	
	//
	//	kAllNonNeutralWithIndDomCaches means that all mutations are deemed nonneutral, and so we will not set
	//	up nonneutral mutation buffers, since they'd just be a copy of the main mutation buffers; instead, we
	//	will simply use the main mutation buffers for trait calculations.  In regime 5 there is a need for the
	//	independent-dominance caches, however, so we will allocate nonneutral caches, but we won't allocate any
	//	space at all for the nonneutral mutation buffers (unlike other regimes, which provide a minimum block
	//	size as a starter).
	//
	zero_out_nonneutral_cache_ZEROSIZE(inddom_cache_count);
	
	// mark the nonneutral cache with a special value that indicates this state.  It has to be a special count
	// value; the capacity can't be messed with, since it might be a leftover from a previous use of the cache.
	// The marker value used is intended to be likely to produce a crash if it is ever used as a count.
	if (nonneutral_cache_)
		nonneutral_cache_->nonneutral_count_ = SLIM_MUTRUN_USE_MAIN_BUFFER;
	
	// we consider the mutations in the main buffer to be "cached" in this case
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
	metrics->mutations_cached_ += mutation_count_;
#endif
}
#endif

#if SLIM_USE_HAPLOID_CACHES()
void MutationRun::cache_nonneutral_mutations_REGIME_kHaploidAllNonNeutralNoCallbacks(MutationBlock *mutation_block, MutRunInternalCacheIndex inddom_cache_count, const std::vector<Trait *> traits
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
													  , NonNeutralValidationMetrics *metrics
#endif
													  ) const
{
#if DEBUG
	if ((internal_cache_count_DEBUG_ != static_cast<MutRunInternalCacheIndex>(-1)) && (inddom_cache_count != internal_cache_count_DEBUG_))
		EIDOS_TERMINATION << "ERROR (MutationRun::cache_nonneutral_mutations_REGIME_kHaploidAllNonNeutralNoCallbacks): (internal error) inddom_cache_count != internal_cache_count_DEBUG_." << EidosTerminate(nullptr);
#endif
	
	//
	//	kHaploidAllNonNeutralNoCallbacks is a haploid-only regime in which there are NO mutationEffect()
	//	callbacks.  All mutations are nonneutral, so we do not need to test the mutations for neutrality.
	//
	//	In this regime, we can cache every effect for every mutation, and leave the nonneutral
	//	buffer completely empty; it is as if every mutation effect is independent-dominance for
	//	every trait, because the chromosome is haploid.
	//
	zero_out_nonneutral_cache_ZEROSIZE(inddom_cache_count);
	
	slim_trait_index_t trait_count = (slim_trait_index_t)inddom_cache_count;
	
	// loop through traits and then mutations, and cache all effects for all mutations
	// we do traits at the top level so we have just one effect accumulator at a time
	for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
	{
		Trait *trait = traits[trait_index];
		double effect_accumulator;
		
		if (trait->Type() == TraitType::kAdditive)
		{
			effect_accumulator = 0.0;
			
			for (int32_t bufindex = 0; bufindex < mutation_count_; ++bufindex)
			{
				MutationIndex mutindex = mutations_[bufindex];
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForIndex(mutindex) + trait_index;
				slim_effect_t homozygous_effect = mut_trait_info->homozygous_effect_;
				
				effect_accumulator += (double)homozygous_effect;
			}
		}
		else // (trait->Type() == TraitType::kMultiplicative)
		{
			effect_accumulator = 1.0;
			
			for (int32_t bufindex = 0; bufindex < mutation_count_; ++bufindex)
			{
				MutationIndex mutindex = mutations_[bufindex];
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForIndex(mutindex) + trait_index;
				slim_effect_t homozygous_effect = mut_trait_info->homozygous_effect_;
				
				effect_accumulator *= (double)homozygous_effect;
			}
		}
		
		nonneutral_cache_->internal_cache_[trait_index] = (slim_effect_t)effect_accumulator;
	}
	
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
	metrics->mutations_summarized_ += mutation_count_;
#endif
}

void MutationRun::cache_nonneutral_mutations_REGIME_kHaploidNoCallbacks(MutationBlock *mutation_block, MutRunInternalCacheIndex inddom_cache_count, const std::vector<Trait *> traits
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
													  , NonNeutralValidationMetrics *metrics
#endif
													  ) const
{
#if DEBUG
	if ((internal_cache_count_DEBUG_ != static_cast<MutRunInternalCacheIndex>(-1)) && (inddom_cache_count != internal_cache_count_DEBUG_))
		EIDOS_TERMINATION << "ERROR (MutationRun::cache_nonneutral_mutations_REGIME_kHaploidNoCallbacks): (internal error) inddom_cache_count != internal_cache_count_DEBUG_." << EidosTerminate(nullptr);
#endif
	
	//
	//	kHaploidNoCallbacks is a haploid-only regime in which there are NO mutationEffect() callbacks.
	//	Some mutations are neutral, so we test the mutations for neutrality to skip work.
	//
	//	In this regime, we can cache every effect for every mutation, and leave the nonneutral
	//	buffer completely empty; it is as if every mutation effect is independent-dominance for
	//	every trait, because the chromosome is haploid.
	//
	zero_out_nonneutral_cache_ZEROSIZE(inddom_cache_count);
	
	slim_trait_index_t trait_count = (slim_trait_index_t)inddom_cache_count;
	
	// first reset all effect accumulators to neutral values for their traits
	for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
	{
		Trait *trait = traits[trait_index];
		
		if (trait->Type() == TraitType::kAdditive)
			nonneutral_cache_->internal_cache_[trait_index] = (slim_effect_t)0.0;
		else // (trait->Type() == TraitType::kMultiplicative)
			nonneutral_cache_->internal_cache_[trait_index] = (slim_effect_t)1.0;
	}
	
	// loop through mutations and then traits, and cache all effects for all mutations
	// we do mutations at the top level because we have to choose how to handle each mutation
	Mutation *mut_block_ptr = mutation_block->mutation_buffer_;
	
	for (int32_t bufindex = 0; bufindex < mutation_count_; ++bufindex)
	{
		MutationIndex mutindex = mutations_[bufindex];
		Mutation *mutptr = mut_block_ptr + mutindex;
		
		if (!mutptr->is_neutral_for_all_traits_)
		{
			MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForIndex(mutindex);
			
			for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
			{
				MutationTraitInfo *trait_info = mut_trait_info + trait_index;
				
				if (traits[trait_index]->Type() == TraitType::kAdditive)
					nonneutral_cache_->internal_cache_[trait_index] += trait_info->homozygous_effect_;
				else // (trait->Type() == TraitType::kMultiplicative)
					nonneutral_cache_->internal_cache_[trait_index] *= trait_info->homozygous_effect_;
			}
			
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
			metrics->mutations_summarized_++;
#endif
		}
		else
		{
			
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
			metrics->mutations_omitted_++;
#endif
		}
	}
}

void MutationRun::cache_nonneutral_mutations_REGIME_kHaploidAllNonNeutralWithCallbacks(MutationBlock *mutation_block, MutRunInternalCacheIndex inddom_cache_count, const std::vector<Trait *> traits
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
													  , NonNeutralValidationMetrics *metrics
#endif
													  ) const
{
#if DEBUG
	if ((internal_cache_count_DEBUG_ != static_cast<MutRunInternalCacheIndex>(-1)) && (inddom_cache_count != internal_cache_count_DEBUG_))
		EIDOS_TERMINATION << "ERROR (MutationRun::cache_nonneutral_mutations_REGIME_kHaploidAllNonNeutralWithCallbacks): (internal error) inddom_cache_count != internal_cache_count_DEBUG_." << EidosTerminate(nullptr);
#endif
	
	//
	//	kHaploidAllNonNeutralWithCallbacks is a haploid-only regime in which there are some active
	//	mutationEffect() callbacks.  In this regime, we loop through mutations and check whether their
	//	mutation type is subject to a callback.  If so, we put it into the non-neutral mutation buffer;
	//	we do this even if the callback has a constant effect, since figuring things out gets very
	//	complicated with multiple traits, multiple callbacks, trait-specific callbacks, etc.  If not,
	//	in this regime we know that all mutations are nonneutral, so we just incorporate the effects
	//	of the mutation into the haploid effect caches and leave it out of the non-neutral mutation
	//	buffer.
	//
	//	As in all the haploid regimes, it is as if every mutation effect is independent-dominance
	//	for every trait, because the chromosome is haploid.  The only difference is that some mutation
	//	types are subject to callbacks here, which we have to test for.
	//
	zero_out_nonneutral_cache(inddom_cache_count);
	
	slim_trait_index_t trait_count = (slim_trait_index_t)inddom_cache_count;
	
	// first reset all effect accumulators to neutral values for their traits
	for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
	{
		Trait *trait = traits[trait_index];
		
		if (trait->Type() == TraitType::kAdditive)
			nonneutral_cache_->internal_cache_[trait_index] = (slim_effect_t)0.0;
		else // (trait->Type() == TraitType::kMultiplicative)
			nonneutral_cache_->internal_cache_[trait_index] = (slim_effect_t)1.0;
	}
	
	// loop through mutations and then traits, and cache all effects for all mutations
	// we do mutations at the top level because we have to choose how to handle each mutation
	Mutation *mut_block_ptr = mutation_block->mutation_buffer_;
	MutationIndex *mutation_buffer = nonneutral_mutation_buffer(inddom_cache_count);
	int32_t buffer_capacity = nonneutral_cache_->nonneutral_capacity_;
	int32_t buffer_count = nonneutral_cache_->nonneutral_count_;
	
	for (int32_t bufindex = 0; bufindex < mutation_count_; ++bufindex)
	{
		MutationIndex mutindex = mutations_[bufindex];
		Mutation *mutptr = mut_block_ptr + mutindex;
		MutationType *muttypeptr = mutptr->mutation_type_ptr_;
		
		if (muttypeptr->subject_to_mutationEffect_callback_)
		{
			// If the only callback the mutation type is subject to is a global neutral callback,
			// the mutation is effectively neutral and doesn't need to be evaluated at all; it
			// has been made neutral.  But if other callbacks are present, we need to handle it.
			if (muttypeptr->subject_to_non_global_neutral_callback_)
			{
				if (buffer_count == buffer_capacity)
				{
					// expand the buffer and re-fetch our local information about it
					expand_nonneutral_buffer(inddom_cache_count);
					mutation_buffer = nonneutral_mutation_buffer(inddom_cache_count);
					buffer_capacity = nonneutral_cache_->nonneutral_capacity_;
				}
				
				*(mutation_buffer + buffer_count) = mutindex;
				++buffer_count;
				
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
				metrics->mutations_cached_++;
#endif
			}
			else
			{
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
				metrics->mutations_omitted_++;
#endif
			}
		}
		else
		{
			// The mutation isn't subject to any callbacks at all, so we can accumulate all its effects
			MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForIndex(mutindex);
			
			for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
			{
				MutationTraitInfo *trait_info = mut_trait_info + trait_index;
				
				if (traits[trait_index]->Type() == TraitType::kAdditive)
					nonneutral_cache_->internal_cache_[trait_index] += trait_info->homozygous_effect_;
				else // (trait->Type() == TraitType::kMultiplicative)
					nonneutral_cache_->internal_cache_[trait_index] *= trait_info->homozygous_effect_;
			}
			
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
			metrics->mutations_summarized_++;
#endif
		}
	}
	
	nonneutral_cache_->nonneutral_count_ = buffer_count;
}

void MutationRun::cache_nonneutral_mutations_REGIME_kHaploidWithCallbacks(MutationBlock *mutation_block, MutRunInternalCacheIndex inddom_cache_count, const std::vector<Trait *> traits
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
													  , NonNeutralValidationMetrics *metrics
#endif
													  ) const
{
#if DEBUG
	if ((internal_cache_count_DEBUG_ != static_cast<MutRunInternalCacheIndex>(-1)) && (inddom_cache_count != internal_cache_count_DEBUG_))
		EIDOS_TERMINATION << "ERROR (MutationRun::cache_nonneutral_mutations_REGIME_kHaploidWithCallbacks): (internal error) inddom_cache_count != internal_cache_count_DEBUG_." << EidosTerminate(nullptr);
#endif
	
	//
	//	kHaploidWithCallbacks is a haploid-only regime in which there are some active mutationEffect()
	//	callbacks.  In this regime, we loop through mutations and check whether their mutation type is
	//	subject to a callback.  If so, we put it into the non-neutral mutation buffer to be handled
	//	downstream; we do this even if the callback has a constant effect, since figuring things out
	//	gets very complicated with multiple traits, multiple callbacks, trait-specific callbacks, etc.
	//	If not, in this regime we know that some mutations are neutral, so we test whether the mutation
	//	is neutral for all traits.  If so, we can just skip it entirely.  If not, we incorporate the
	//	effects of the mutation into the haploid effect caches and leave it out of the non-neutral
	//	mutation buffer.
	//
	//	As in all the haploid regimes, it is as if every mutation effect is independent-dominance
	//	for every trait, because the chromosome is haploid.  The only difference is that some mutation
	//	types are subject to callbacks here, which we have to test for.
	//
	zero_out_nonneutral_cache(inddom_cache_count);
	
	slim_trait_index_t trait_count = (slim_trait_index_t)inddom_cache_count;
	
	// first reset all effect accumulators to neutral values for their traits
	for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
	{
		Trait *trait = traits[trait_index];
		
		if (trait->Type() == TraitType::kAdditive)
			nonneutral_cache_->internal_cache_[trait_index] = (slim_effect_t)0.0;
		else // (trait->Type() == TraitType::kMultiplicative)
			nonneutral_cache_->internal_cache_[trait_index] = (slim_effect_t)1.0;
	}
	
	// loop through mutations and then traits, and cache all effects for all mutations
	// we do mutations at the top level because we have to choose how to handle each mutation
	Mutation *mut_block_ptr = mutation_block->mutation_buffer_;
	MutationIndex *mutation_buffer = nonneutral_mutation_buffer(inddom_cache_count);
	int32_t buffer_capacity = nonneutral_cache_->nonneutral_capacity_;
	int32_t buffer_count = nonneutral_cache_->nonneutral_count_;
	
	for (int32_t bufindex = 0; bufindex < mutation_count_; ++bufindex)
	{
		MutationIndex mutindex = mutations_[bufindex];
		Mutation *mutptr = mut_block_ptr + mutindex;
		MutationType *muttypeptr = mutptr->mutation_type_ptr_;
		
		if (muttypeptr->subject_to_mutationEffect_callback_)
		{
			// If the only callback the mutation type is subject to is a global neutral callback,
			// the mutation is effectively neutral and doesn't need to be evaluated at all; it
			// has been made neutral.  But if other callbacks are present, we need to handle it.
			if (muttypeptr->subject_to_non_global_neutral_callback_)
			{
				if (buffer_count == buffer_capacity)
				{
					// expand the buffer and re-fetch our local information about it
					expand_nonneutral_buffer(inddom_cache_count);
					mutation_buffer = nonneutral_mutation_buffer(inddom_cache_count);
					buffer_capacity = nonneutral_cache_->nonneutral_capacity_;
				}
				
				*(mutation_buffer + buffer_count) = mutindex;
				++buffer_count;
				
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
				metrics->mutations_cached_++;
#endif
			}
			else
			{
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
				metrics->mutations_omitted_++;
#endif
			}
		}
		else
		{
			// The mutation isn't subject to any callbacks at all, so we can accumulate all its effects
			if (!mutptr->is_neutral_for_all_traits_)
			{
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForIndex(mutindex);
				
				for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
				{
					MutationTraitInfo *trait_info = mut_trait_info + trait_index;
					
					if (traits[trait_index]->Type() == TraitType::kAdditive)
						nonneutral_cache_->internal_cache_[trait_index] += trait_info->homozygous_effect_;
					else // (trait->Type() == TraitType::kMultiplicative)
						nonneutral_cache_->internal_cache_[trait_index] *= trait_info->homozygous_effect_;
				}
			}
			
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
			metrics->mutations_summarized_++;
#endif
		}
	}
	
	nonneutral_cache_->nonneutral_count_ = buffer_count;
}
#endif

void MutationRun::check_nonneutral_mutation_cache() const
{
	// This is called by beginend_nonneutral_pointers() to verify that the nonneutral cache is in a valid state
	// to be accessed.  There are valid cache states with nullptr for nonneutral_cache_, but those states should
	// never be accessed by beginend_nonneutral_pointers(), so they are considered an error here.
	if (!nonneutral_cache_)
		EIDOS_TERMINATION << "ERROR (MutationRun::check_nonneutral_mutation_cache): (internal error) cache not allocated." << EidosTerminate();
	
	if (nonneutral_cache_->nonneutral_count_ == -1)
		EIDOS_TERMINATION << "ERROR (MutationRun::check_nonneutral_mutation_cache): (internal error) unvalidated cache." << EidosTerminate();
	if (nonneutral_cache_->nonneutral_count_ < 0)
		EIDOS_TERMINATION << "ERROR (MutationRun::check_nonneutral_mutation_cache): (internal error) invalid nonneutral_count_ " << nonneutral_cache_->nonneutral_count_ << "." << EidosTerminate();
	if (nonneutral_cache_->nonneutral_count_ > nonneutral_cache_->nonneutral_capacity_)
		EIDOS_TERMINATION << "ERROR (MutationRun::check_nonneutral_mutation_cache): (internal error) cache size exceeds cache capacity." << EidosTerminate();
}

void MutationRun::check_nonneutral_mutation_cache_MAIN() const
{
	// This alternate version of check_nonneutral_mutation_cache() is called when in trait calculation regime
	// kAllNonNeutralNoIndDomCaches or kAllNonNeutralWithIndDomCaches, which do not construct the nonneutral
	// mutation buffers.  We check for validity of the nonneutral cache given that we are in that state.
	if (nonneutral_cache_)
	{
		if (nonneutral_cache_->nonneutral_count_ == -1)
			EIDOS_TERMINATION << "ERROR (MutationRun::check_nonneutral_mutation_cache_MAIN): (internal error) unvalidated cache." << EidosTerminate();
		if (nonneutral_cache_->nonneutral_count_ != SLIM_MUTRUN_USE_MAIN_BUFFER)
			EIDOS_TERMINATION << "ERROR (MutationRun::check_nonneutral_mutation_cache_MAIN): (internal error) invalid nonneutral_count_ " << nonneutral_cache_->nonneutral_count_ << " (should be SLIM_MUTRUN_USE_MAIN_BUFFER)." << EidosTerminate();
		if (mutation_count_ > mutation_capacity_)
			EIDOS_TERMINATION << "ERROR (MutationRun::check_nonneutral_mutation_cache_MAIN): (internal error) cache size exceeds cache capacity." << EidosTerminate();
	}
}

#if SLIM_USE_INDEPENDENT_DOMINANCE_CACHES()

template <const bool f_is_additive_trait, const bool f_haploid_chromosome>
void MutationRun::validate_independent_dominance_cache_for_trait(slim_trait_index_t trait_index, MutRunInternalCacheIndex inddom_cache_index, MutRunInternalCacheIndex inddom_cache_count, MutationBlock *mutation_block
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
																 , NonNeutralValidationMetrics *metrics
#endif
																 ) const
{
#if DEBUG
	if (inddom_cache_index >= internal_cache_count_DEBUG_)
		EIDOS_TERMINATION << "ERROR (MutationRun::validate_independent_dominance_cache_for_trait): (internal error) inddom_cache_index >= internal_cache_count_DEBUG_." << EidosTerminate(nullptr);
	if (f_haploid_chromosome)
		EIDOS_TERMINATION << "ERROR (MutationRun::validate_independent_dominance_cache_for_trait): (internal error) this method should no longer be called for haploid chromosomes, I think." << EidosTerminate(nullptr);
	if (!nonneutral_cache_)
		EIDOS_TERMINATION << "ERROR (MutationRun::validate_independent_dominance_cache_for_trait): (internal error) nonneutral_cache_ should always be allocated, since we need independent-dominance caches." << EidosTerminate(nullptr);
#endif
	
	// get the nonneutral mutation buffer, or use the main buffer if we are in that mode
	const MutationIndex *haplosome_iter, *haplosome_max;
	
	if (nonneutral_cache_->nonneutral_count_ != SLIM_MUTRUN_USE_MAIN_BUFFER)
		beginend_nonneutral_pointers(&haplosome_iter, &haplosome_max, inddom_cache_count);
	else
		beginend_nonneutral_pointers_MAIN(&haplosome_iter, &haplosome_max);
	
	// do internal math using double to avoid numerical error
	double effect_accumulator = (f_is_additive_trait ? 0.0 : 1.0);	// start with neutrality
	
	while (haplosome_iter != haplosome_max)
	{
		MutationIndex mutindex = *haplosome_iter++;
		slim_effect_t independent_dominance_effect;
		
		if (f_haploid_chromosome)
			independent_dominance_effect = mutation_block->TraitInfoForIndex(mutindex)[trait_index].homozygous_effect_;
		else
			independent_dominance_effect = mutation_block->TraitInfoForIndex(mutindex)[trait_index].heterozygous_effect_;
		
		if (f_is_additive_trait)
			effect_accumulator += (double)independent_dominance_effect;
		else
			effect_accumulator *= (double)independent_dominance_effect;
		
#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
		metrics->mutations_summarized_++;
#endif
	}
	
	nonneutral_cache_->internal_cache_[static_cast<slim_trait_index_t>(inddom_cache_index)] = (slim_effect_t)effect_accumulator;
}

#if SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()
template void MutationRun::validate_independent_dominance_cache_for_trait<false, false>(slim_trait_index_t, MutRunInternalCacheIndex, MutRunInternalCacheIndex, MutationBlock *, NonNeutralValidationMetrics *) const;
template void MutationRun::validate_independent_dominance_cache_for_trait<false, true>(slim_trait_index_t, MutRunInternalCacheIndex, MutRunInternalCacheIndex, MutationBlock *, NonNeutralValidationMetrics *) const;
template void MutationRun::validate_independent_dominance_cache_for_trait<true, false>(slim_trait_index_t, MutRunInternalCacheIndex, MutRunInternalCacheIndex, MutationBlock *, NonNeutralValidationMetrics *) const;
template void MutationRun::validate_independent_dominance_cache_for_trait<true, true>(slim_trait_index_t, MutRunInternalCacheIndex, MutRunInternalCacheIndex, MutationBlock *, NonNeutralValidationMetrics *) const;
#else	// !(SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND())
template void MutationRun::validate_independent_dominance_cache_for_trait<false, false>(slim_trait_index_t, MutRunInternalCacheIndex, MutRunInternalCacheIndex, MutationBlock *) const;
template void MutationRun::validate_independent_dominance_cache_for_trait<false, true>(slim_trait_index_t, MutRunInternalCacheIndex, MutRunInternalCacheIndex, MutationBlock *) const;
template void MutationRun::validate_independent_dominance_cache_for_trait<true, false>(slim_trait_index_t, MutRunInternalCacheIndex, MutRunInternalCacheIndex, MutationBlock *) const;
template void MutationRun::validate_independent_dominance_cache_for_trait<true, true>(slim_trait_index_t, MutRunInternalCacheIndex, MutRunInternalCacheIndex, MutationBlock *) const;
#endif	// SLIM_PROFILE_NONNEUTRAL_CACHES() || DEBUG_TRAIT_DEMAND()

#endif	// SLIM_USE_INDEPENDENT_DOMINANCE_CACHES()
#endif	// SLIM_USE_NONNEUTRAL_CACHES()


// Shorthand for clear(), then copy_from_run(p_mutations_to_set), then insert_sorted_mutation() on every
// mutation in p_mutations_to_add, with checks with enforce_stack_policy_for_addition().  The point of
// this is speed: like HaplosomeCloned(), we can merge the new mutations in much faster if we do it in
// bulk.  Note that p_mutations_to_set and p_mutations_to_add must both be sorted by position.
void MutationRun::clear_set_and_merge(Mutation *p_mut_block_ptr, const MutationRun &p_mutations_to_set, std::vector<MutationIndex> &p_mutations_to_add)
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
	const MutationIndex *mutation_iter		= p_mutations_to_add.data();
	const MutationIndex *mutation_iter_max	= mutation_iter + p_mutations_to_add.size();
	MutationIndex mutation_iter_mutation_index = *mutation_iter;
	slim_position_t mutation_iter_pos = (p_mut_block_ptr + mutation_iter_mutation_index)->position_;
	
	const MutationIndex *parent_iter		= p_mutations_to_set.begin_pointer_const();
	const MutationIndex *parent_iter_max	= p_mutations_to_set.end_pointer_const();
	MutationIndex parent_iter_mutation_index = *parent_iter;
	slim_position_t parent_iter_pos = (p_mut_block_ptr + parent_iter_mutation_index)->position_;
	
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
			parent_iter_pos = (p_mut_block_ptr + parent_iter_mutation_index)->position_;
		}
		else
		{
			// we have a new mutation to add, which we know is not already present; check the stacking policy
			if (enforce_stack_policy_for_addition(p_mut_block_ptr, mutation_iter_pos, (p_mut_block_ptr + mutation_iter_mutation_index)->mutation_type_ptr_))
				emplace_back(mutation_iter_mutation_index);
			
			mutation_iter++;
			if (mutation_iter == mutation_iter_max)
				break;
			
			mutation_iter_mutation_index = *mutation_iter;
			mutation_iter_pos = (p_mut_block_ptr + mutation_iter_mutation_index)->position_;
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
		mutation_iter_pos = (p_mut_block_ptr + mutation_iter_mutation_index)->position_;
		
		if (enforce_stack_policy_for_addition(p_mut_block_ptr, mutation_iter_pos, (p_mut_block_ptr + mutation_iter_mutation_index)->mutation_type_ptr_))
			emplace_back(mutation_iter_mutation_index);
		
		mutation_iter++;
	}
}

size_t MutationRun::MemoryUsageForMutationIndexBuffers(void) const
{
	return mutation_capacity_ * sizeof(MutationIndex);
}

size_t MutationRun::MemoryUsageForNonneutralCaches(slim_trait_index_t trait_count) const
{
#if SLIM_USE_NONNEUTRAL_CACHES()
	if (nonneutral_cache_)
		return nonneutral_cache_->nonneutral_capacity_ * sizeof(MutationIndex) + sizeof(NonNeutralCache) + trait_count * sizeof(slim_effect_t);
#endif
	
	return 0;
}















































