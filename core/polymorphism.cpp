//
//  polymorphism.cpp
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


#include "polymorphism.h"

#include <fstream>


using std::multimap;


Polymorphism::Polymorphism(int p_mutation_id, const MutationType *p_mutation_type_ptr, double p_selection_coeff, int p_subpop_index, int p_generation, int p_prevalence) :
	mutation_id_(p_mutation_id), mutation_type_ptr_(p_mutation_type_ptr), selection_coeff_(static_cast<typeof(selection_coeff_)>(p_selection_coeff)), subpop_index_(p_subpop_index), generation_(p_generation), prevalence_(p_prevalence)
{
}

void Polymorphism::print(std::ostream &p_out, int p_index, bool include_id /* = true*/) const
{
	if (include_id)
		p_out << mutation_id_ << " ";
	
	p_out << "m" << mutation_type_ptr_->mutation_type_id_ << " " << p_index << " " << selection_coeff_ << " " << mutation_type_ptr_->dominance_coeff_ << " p" << subpop_index_ << " " << generation_ << " " << prevalence_ << std::endl;		// used to have a +1 on p_index, which is the position; switched to zero-based
}

// find p_mutation in p_polymorphisms and return its id
int FindMutationInPolymorphismMap(const multimap<const int,Polymorphism> &p_polymorphisms, const Mutation &p_mutation)
{
	// iterate through all mutations with same position
	std::pair<multimap<const int,Polymorphism>::const_iterator,multimap<const int,Polymorphism>::const_iterator> range = p_polymorphisms.equal_range(p_mutation.position_);
	multimap<const int,Polymorphism>::const_iterator polymorphisms_iter;
	
	for (polymorphisms_iter = range.first; polymorphisms_iter != range.second; polymorphisms_iter++)
		if (polymorphisms_iter->second.mutation_type_ptr_ == p_mutation.mutation_type_ptr_ && polymorphisms_iter->second.selection_coeff_ == p_mutation.selection_coeff_) 
			return polymorphisms_iter->second.mutation_id_;
	
	return 0;
}

// if mutation p_mutation is present in p_polymorphisms increase its prevalence, otherwise add it
void AddMutationToPolymorphismMap(multimap<const int,Polymorphism> *p_polymorphisms, const Mutation &p_mutation)
{
	// iterate through all mutations with same position
	std::pair<multimap<const int,Polymorphism>::iterator,multimap<const int,Polymorphism>::iterator> range = p_polymorphisms->equal_range(p_mutation.position_);
	multimap<const int,Polymorphism>::iterator polymorphisms_iter;
	
	for (polymorphisms_iter = range.first; polymorphisms_iter != range.second; polymorphisms_iter++)
		if (polymorphisms_iter->second.mutation_type_ptr_ == p_mutation.mutation_type_ptr_ && polymorphisms_iter->second.selection_coeff_ == p_mutation.selection_coeff_) 
		{ 
			polymorphisms_iter->second.prevalence_++;
			return;
		}
	
	// the mutation was not found, so add it to p_polymorphisms
	int mutation_id = static_cast<int>(p_polymorphisms->size());	// used to have a +1 here; switched to zero-based
	Polymorphism new_polymorphism = Polymorphism(mutation_id, p_mutation.mutation_type_ptr_, p_mutation.selection_coeff_, p_mutation.subpop_index_, p_mutation.generation_, 1);
	
	p_polymorphisms->insert(std::pair<const int,Polymorphism>(p_mutation.position_, new_polymorphism));
}



































































