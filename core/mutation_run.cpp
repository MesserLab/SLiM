//
//  mutation_run.cpp
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


#include "mutation_run.h"

#include <vector>


// For doing bulk operations across all MutationRun objects; see header
int64_t gSLiM_MutationRun_OperationID = 0;


// Shared pool support; see header for comments
MutationRun *MutationRun::NewMutationRun(void)
{
	if (s_freed_mutation_runs_.size())
	{
		MutationRun *back = s_freed_mutation_runs_.back();
		
		s_freed_mutation_runs_.pop_back();
		return back;
	}
	
	return new MutationRun();
}

void MutationRun::FreeMutationRun(MutationRun *p_run)
{
	p_run->mutation_count_ = 0;
	
	s_freed_mutation_runs_.emplace_back(p_run);
}

std::vector<MutationRun *> MutationRun::s_freed_mutation_runs_;


MutationRun::MutationRun() : intrusive_ref_count(0)
{
}

MutationRun::~MutationRun(void)
{
	// mutations_buffer_ is not malloced and cannot be freed; free only if we have an external buffer
	if (mutations_ != mutations_buffer_)
		free(mutations_);
}

#ifdef SLIM_MUTRUN_CHECK_LOCKING
void MutationRun::LockingViolation(void) const
{
	EIDOS_TERMINATION << "ERROR (MutationRun::LockingViolation): (internal error) a locked MutationRun was modified." << eidos_terminate();
}
#endif

void MutationRun::RemoveFixedMutations(slim_refcount_t p_fixed_count, int64_t p_operation_id)
{
	if (operation_id_ != p_operation_id)
	{
		operation_id_ = p_operation_id;
		
		// We don't use begin_pointer() / end_pointer() here, because we actually want to modify the MutationRun even
		// though it is shared by multiple Genomes; this is an exceptional case, so we go around our safeguards.
		Mutation **genome_iter = mutations_;
		Mutation **genome_backfill_iter = mutations_;
		Mutation **genome_max = mutations_ + mutation_count_;
		
		// genome_iter advances through the mutation list; for each entry it hits, the entry is either fixed (skip it) or not fixed (copy it backward to the backfill pointer)
		while (genome_iter != genome_max)
		{
			Mutation *mutation_ptr = *genome_iter;
			
			if ((mutation_ptr->reference_count_ == p_fixed_count) && (mutation_ptr->mutation_type_ptr_->convert_to_substitution_))
			{
				// Fixed mutation; we want to omit it, so we just advance our pointer
				++genome_iter;
			}
			else
			{
				// Unfixed mutation; we want to keep it, so we copy it backward and advance our backfill pointer as well as genome_iter
				if (genome_backfill_iter != genome_iter)
					*genome_backfill_iter = mutation_ptr;
				
				++genome_backfill_iter;
				++genome_iter;
			}
		}
		
		// excess mutations at the end have been copied back already; we just adjust mutation_count_ and forget about them
		mutation_count_ -= (genome_iter - genome_backfill_iter);
	}
}

bool MutationRun::_enforce_stack_policy_for_addition(slim_position_t p_position, MutationType *p_mut_type_ptr, MutationStackPolicy p_policy)
{
	Mutation **begin_ptr = begin_pointer();
	Mutation **end_ptr = end_pointer();
	
	if (p_policy == MutationStackPolicy::kKeepFirst)
	{
		// If the first mutation occurring at a site is kept, then we need to check for an existing mutation of this type
		// We scan in reverse order, because usually we're adding mutations on the end with emplace_back()
		for (Mutation **mut_ptr = end_ptr - 1; mut_ptr >= begin_ptr; --mut_ptr)
		{
			Mutation *mut = *mut_ptr;
			slim_position_t mut_position = mut->position_;
			
			if ((mut_position == p_position) && (mut->mutation_type_ptr_ == p_mut_type_ptr))
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
		Mutation **first_match_ptr = nullptr;
		
		for (Mutation **mut_ptr = end_ptr - 1; mut_ptr >= begin_ptr; --mut_ptr)
		{
			Mutation *mut = *mut_ptr;
			slim_position_t mut_position = mut->position_;
			
			if ((mut_position == p_position) && (mut->mutation_type_ptr_ == p_mut_type_ptr))
				first_match_ptr = mut_ptr;	// set repeatedly as we scan backwards, until we exit
			else if (mut_position < p_position)
				break;
		}
		
		// If we found any, we now scan forward and remove them, in anticipation of the new mutation being added
		if (first_match_ptr)
		{
			Mutation **replace_ptr = first_match_ptr;	// replace at the first match position
			Mutation **mut_ptr = first_match_ptr + 1;	// we know the initial position needs removal, so start at the next
			
			for ( ; mut_ptr < end_ptr; ++mut_ptr)
			{
				Mutation *mut = *mut_ptr;
				slim_position_t mut_position = mut->position_;
				
				if ((mut_position == p_position) && (mut->mutation_type_ptr_ == p_mut_type_ptr))
				{
					// The current scan position is a mutation that needs to be removed, so scan forward to skip copying it backward
					continue;
				}
				else
				{
					// The current scan position is a valid mutation, so we copy it backwards
					*(replace_ptr++) = mut;
				}
			}
			
			// excess mutations at the end have been copied back already; we just adjust mutation_count_ and forget about them
			set_size(size() - (int)(mut_ptr - replace_ptr));
		}
		
		return true;
	}
	else
		EIDOS_TERMINATION << "ERROR (Genome::_enforce_stack_policy_for_addition): (internal error) invalid policy." << eidos_terminate();
}























