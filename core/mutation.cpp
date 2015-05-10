//
//  mutation.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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


#ifdef SLIMGUI
// A global counter used to assign all Mutation objects a unique ID in SLiMgui
uint64_t g_next_mutation_id = 0;
#endif

#ifdef SLIMGUI
// In SLiMgui, the mutation_id_ gets initialized here, from the global counter g_next_mutation_id
Mutation::Mutation(SLIMCONST MutationType *p_mutation_type_ptr, int p_position, double p_selection_coeff, int p_subpop_index, int p_generation) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(static_cast<float>(p_selection_coeff)), subpop_index_(p_subpop_index), generation_(p_generation), mutation_id_(g_next_mutation_id++)
#else
Mutation::Mutation(SLIMCONST MutationType *p_mutation_type_ptr, int p_position, double p_selection_coeff, int p_subpop_index, int p_generation) :
	mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(static_cast<float>(p_selection_coeff)), subpop_index_(p_subpop_index), generation_(p_generation)
#endif
{
#if DEBUG_MUTATIONS
	SLIM_OUTSTREAM << "Mutation constructed: " << this << std::endl;
#endif
}

#if DEBUG_MUTATIONS
Mutation::~Mutation()
{
	SLIM_OUTSTREAM << "Mutation destructed: " << this << std::endl;
}
#endif

std::ostream &operator<<(std::ostream &p_outstream, const Mutation &p_mutation)
{
	p_outstream << "Mutation{mutation_type_ " << p_mutation.mutation_type_ptr_->mutation_type_id_ << ", position_ " << p_mutation.position_ << ", selection_coeff_ " << p_mutation.selection_coeff_ << ", subpop_index_ " << p_mutation.subpop_index_ << ", generation_ " << p_mutation.generation_;
	
	return p_outstream;
}



































































