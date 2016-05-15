//
//  polymorphism.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
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


#include "polymorphism.h"

#include <fstream>
#include <map>
#include <utility>


using std::multimap;


Polymorphism::Polymorphism(int p_mutation_id, const Mutation *p_mutation_ptr, slim_refcount_t p_prevalence) :
	mutation_id_(p_mutation_id), mutation_ptr_(p_mutation_ptr), prevalence_(p_prevalence)
{
}

void Polymorphism::print(std::ostream &p_out) const
{
	p_out << mutation_id_ << " " << "m" << mutation_ptr_->mutation_type_ptr_->mutation_type_id_ << " " << mutation_ptr_->position_ << " " << mutation_ptr_->selection_coeff_ << " " << mutation_ptr_->mutation_type_ptr_->dominance_coeff_ << " p" << mutation_ptr_->subpop_index_ << " " << mutation_ptr_->generation_ << " " << prevalence_ << std::endl;
}

void Polymorphism::print_no_id(std::ostream &p_out) const
{
	p_out << "m" << mutation_ptr_->mutation_type_ptr_->mutation_type_id_ << " " << mutation_ptr_->position_ << " " << mutation_ptr_->selection_coeff_ << " " << mutation_ptr_->mutation_type_ptr_->dominance_coeff_ << " p" << mutation_ptr_->subpop_index_ << " " << mutation_ptr_->generation_ << " " << prevalence_ << std::endl;
}

// find p_mutation in p_polymorphisms and return its id
int FindMutationInPolymorphismMap(const multimap<const slim_position_t,Polymorphism> &p_polymorphisms, const Mutation *p_mutation)
{
	// iterate through all mutations with same position
	std::pair<multimap<const slim_position_t,Polymorphism>::const_iterator,multimap<const slim_position_t,Polymorphism>::const_iterator> range = p_polymorphisms.equal_range(p_mutation->position_);
	multimap<const slim_position_t,Polymorphism>::const_iterator polymorphisms_iter;
	
	for (polymorphisms_iter = range.first; polymorphisms_iter != range.second; polymorphisms_iter++)
		if (polymorphisms_iter->second.mutation_ptr_ == p_mutation) 
			return polymorphisms_iter->second.mutation_id_;
	
	return 0;
}

// if mutation p_mutation is present in p_polymorphisms increase its prevalence, otherwise add it
void AddMutationToPolymorphismMap(multimap<const slim_position_t,Polymorphism> *p_polymorphisms, const Mutation *p_mutation)
{
	// iterate through all mutations with same position
	std::pair<multimap<const slim_position_t,Polymorphism>::iterator,multimap<const slim_position_t,Polymorphism>::iterator> range = p_polymorphisms->equal_range(p_mutation->position_);
	multimap<const slim_position_t,Polymorphism>::iterator polymorphisms_iter;
	
	for (polymorphisms_iter = range.first; polymorphisms_iter != range.second; polymorphisms_iter++)
		if (polymorphisms_iter->second.mutation_ptr_ == p_mutation) 
		{ 
			polymorphisms_iter->second.prevalence_++;
			return;
		}
	
	// the mutation was not found, so add it to p_polymorphisms
	int mutation_id = static_cast<int>(p_polymorphisms->size());
	Polymorphism new_polymorphism = Polymorphism(mutation_id, p_mutation, 1);
	
	p_polymorphisms->insert(std::pair<const slim_position_t,Polymorphism>(p_mutation->position_, new_polymorphism));
}



































































