//
//  introduced_mutation.cpp
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


#include "introduced_mutation.h"


IntroducedMutation::IntroducedMutation(const MutationType *p_mutation_type_ptr, int p_position, int p_subpop_index, int p_generation, int p_num_homozygotes, int p_num_heterozygotes) :
	Mutation(p_mutation_type_ptr, p_position, 0.0, p_subpop_index, p_generation), num_homozygotes_(p_num_homozygotes), num_heterozygotes_(p_num_heterozygotes)
{
}

std::ostream &operator<<(std::ostream &p_outstream, const IntroducedMutation &p_introduced_mutation)
{
	p_outstream << "IntroducedMutation{mutation_type_ " << p_introduced_mutation.mutation_type_ptr_->mutation_type_id_ << ", position_ " << p_introduced_mutation.position_ << ", subpop_index_ " << p_introduced_mutation.subpop_index_ << ", generation_ " << p_introduced_mutation.generation_ << ", num_homozygotes_ " << p_introduced_mutation.num_homozygotes_ << ", num_heterozygotes_ " << p_introduced_mutation.num_heterozygotes_ << "}";
	
	return p_outstream;
}





































































