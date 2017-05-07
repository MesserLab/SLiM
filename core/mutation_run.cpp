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

void MutationRun::_RemoveFixedMutations(void)
{
	// Mutations that have fixed, and are thus targeted for removal, have already had their refcount set to -1.
	// That is done only when convertToSubstitution == T, so we don't need to check that flag here.
	
	// We don't use begin_pointer() / end_pointer() here, because we actually want to modify the MutationRun even
	// though it is shared by multiple Genomes; this is an exceptional case, so we go around our safeguards.
	MutationIndex *genome_iter = mutations_;
	MutationIndex *genome_backfill_iter = nullptr;
	MutationIndex *genome_max = mutations_ + mutation_count_;
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	// genome_iter advances through the mutation list; for each entry it hits, the entry is either fixed (skip it) or not fixed
	// (copy it backward to the backfill pointer).  We do this with two successive loops; the first knows that no mutation has
	// yet been skipped, whereas the second knows that at least one mutation has been.
	while (genome_iter != genome_max)
	{
		if ((mut_block_ptr + (*genome_iter++))->reference_count_ != -1)
			continue;
		
		// Fixed mutation; we want to omit it, so we skip it in genome_backfill_iter and transition to the second loop
		genome_backfill_iter = genome_iter - 1;
		break;
	}
	
	while (genome_iter != genome_max)
	{
		MutationIndex mutation_index = *genome_iter;
		Mutation *mutation_ptr = mut_block_ptr + mutation_index;
		
		if (mutation_ptr->reference_count_ != -1)
		{
			// Unfixed mutation; we want to keep it, so we copy it backward and advance our backfill pointer as well as genome_iter
			*genome_backfill_iter = mutation_index;
			
			++genome_backfill_iter;
			++genome_iter;
		}
		else
		{
			// Fixed mutation; we want to omit it, so we just advance our pointer
			++genome_iter;
		}
	}
	
	// excess mutations at the end have been copied back already; we just adjust mutation_count_ and forget about them
	if (genome_backfill_iter != nullptr)
		mutation_count_ -= (genome_iter - genome_backfill_iter);
}

bool MutationRun::_enforce_stack_policy_for_addition(slim_position_t p_position, MutationType *p_mut_type_ptr, MutationStackPolicy p_policy)
{
	MutationIndex *begin_ptr = begin_pointer();
	MutationIndex *end_ptr = end_pointer();
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	if (p_policy == MutationStackPolicy::kKeepFirst)
	{
		// If the first mutation occurring at a site is kept, then we need to check for an existing mutation of this type
		// We scan in reverse order, because usually we're adding mutations on the end with emplace_back()
		for (MutationIndex *mut_ptr = end_ptr - 1; mut_ptr >= begin_ptr; --mut_ptr)
		{
			Mutation *mut = mut_block_ptr + *mut_ptr;
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
		MutationIndex *first_match_ptr = nullptr;
		
		for (MutationIndex *mut_ptr = end_ptr - 1; mut_ptr >= begin_ptr; --mut_ptr)
		{
			Mutation *mut = mut_block_ptr + *mut_ptr;
			slim_position_t mut_position = mut->position_;
			
			if ((mut_position == p_position) && (mut->mutation_type_ptr_ == p_mut_type_ptr))
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
				
				if ((mut_position == p_position) && (mut->mutation_type_ptr_ == p_mut_type_ptr))
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
		EIDOS_TERMINATION << "ERROR (Genome::_enforce_stack_policy_for_addition): (internal error) invalid policy." << eidos_terminate();
}























