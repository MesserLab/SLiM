//
//  polymorphism.h
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2019 Philipp Messer.  All rights reserved.
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

/*
 
 The class Polymorphism represents a polymorphism within a population.  It is not used in the simulation dynamics; it is only used
 for the purpose of collating statistics about a simulation for output.
 
 */

#ifndef __SLiM__polymorphism__
#define __SLiM__polymorphism__


#include <iostream>

#include "chromosome.h"


class Polymorphism;

// This used to be a multimap that indexed by position, allowing collisions.  Now it is a std::map that indexes by mutation_id_,
// which avoids any possibility of collisions, making the code simpler and faster.  BCH 11 June 2016
typedef std::map<const slim_mutationid_t,Polymorphism> PolymorphismMap;
typedef std::pair<const slim_mutationid_t,Polymorphism> PolymorphismPair;


class Polymorphism
{
	// This class allows copying by design
	
public:
	
	slim_polymorphismid_t polymorphism_id_;		// a unique identifier for the polymorphism, starting at 0; this is used instead of the
												// mutation's mutation_id_ because it compresses the range, allowing smaller output files
	const Mutation *mutation_ptr_;				// the mutation represented
	slim_refcount_t prevalence_;				// prevalence
	
	Polymorphism(void) = delete;				// no null construction
	Polymorphism(slim_polymorphismid_t p_polymorphism_id, const Mutation *p_mutation_ptr, slim_refcount_t p_prevalence);
	
	void Print_ID(std::ostream &p_out) const;			// includes polymorphism_id_ at the beginning
	void Print_NoID(std::ostream &p_out) const;	// does not include polymorphism_id_
	
	friend bool operator<(const Polymorphism& p_l, const Polymorphism& p_r)
	{
		return p_l.mutation_ptr_->position_ < p_r.mutation_ptr_->position_; // keep the same order
	}
};


// find p_mutation in p_polymorphisms and return its polymorphism_id_
slim_polymorphismid_t FindMutationInPolymorphismMap(const PolymorphismMap &p_polymorphisms, const Mutation *p_mutation);

// if mutation p_mutation is present in p_polymorphisms increase its prevalence, otherwise add it
void AddMutationToPolymorphismMap(PolymorphismMap *p_polymorphisms, const Mutation *p_mutation);


#endif /* defined(__SLiM__polymorphism__) */




































































