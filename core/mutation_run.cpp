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
























