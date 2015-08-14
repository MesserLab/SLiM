//
//  polymorphism.h
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

/*
 
 The class Polymorphism represents a polymorphism within a population.  It is not used in the simulation dynamics; it is only used
 for the purpose of collating statistics about a simulation for output.
 
 */

#ifndef __SLiM__polymorphism__
#define __SLiM__polymorphism__


#include <iostream>

#include "chromosome.h"


class Polymorphism
{
	// This class allows copying by design
	
public:
	
	int mutation_id_;							// mutation id
	const MutationType *mutation_type_ptr_;		// mutation type identifier
	slim_selcoeff_t selection_coeff_;			// selection coefficient
	slim_objectid_t subpop_index_;				// subpopulation in which mutation arose
	slim_generation_t generation_;				// generation in which mutation arose  
	slim_refcount_t prevalence_;				// prevalence
	
	Polymorphism(void) = delete;				// no null construction
	Polymorphism(int p_mutation_id, const MutationType *p_mutation_type_ptr, double p_selection_coeff, slim_objectid_t p_subpop_index, slim_generation_t p_generation, slim_refcount_t p_prevalence);
	
	void print(std::ostream &p_out, slim_position_t p_index, bool include_id = true) const;
};


// find p_mutation in p_polymorphisms and return its id
int FindMutationInPolymorphismMap(const std::multimap<const slim_position_t,Polymorphism> &p_polymorphisms, const Mutation &p_mutation);

// if mutation p_mutation is present in p_polymorphisms increase its prevalence, otherwise add it
void AddMutationToPolymorphismMap(std::multimap<const slim_position_t,Polymorphism> *p_polymorphisms, const Mutation &p_mutation);


#endif /* defined(__SLiM__polymorphism__) */




































































