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


Mutation::Mutation(void) { ; }

Mutation::Mutation(int p_mutation_type, int p_position, double p_selection_coeff, int p_subpop_index, int p_generation) 
{ 
	mutation_type_ = p_mutation_type;
	position_ = p_position;
	selection_coeff_ = static_cast<float>(p_selection_coeff);
	subpop_index_ = p_subpop_index;
	generation_ = p_generation;
}

bool operator< (const Mutation &p_mutation1, const Mutation &p_mutation2)
{
	return (p_mutation1.position_ < p_mutation2.position_);
}

bool operator== (const Mutation &p_mutation1, const Mutation &p_mutation2)
{
	return (p_mutation1.position_ == p_mutation2.position_ &&
			p_mutation1.mutation_type_ == p_mutation2.mutation_type_ &&
			p_mutation1.selection_coeff_ == p_mutation2.selection_coeff_);
}

std::ostream &operator<<(std::ostream &p_outstream, const Mutation &p_mutation)
{
	p_outstream << "Mutation{mutation_type_ " << p_mutation.mutation_type_ << ", position_ " << p_mutation.position_ << ", selection_coeff_ " << p_mutation.selection_coeff_ << ", subpop_index_ " << p_mutation.subpop_index_ << ", generation_ " << p_mutation.generation_;
	
	return p_outstream;
}



































































