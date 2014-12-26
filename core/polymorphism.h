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
public:
	
	int   mutation_id_; // mutation id
	const MutationType *mutation_type_ptr_;		// mutation type identifier
	float selection_coeff_;  // selection coefficient
	int   subpop_index_;  // subpopulation in which mutation arose
	int   generation_;  // generation in which mutation arose  
	int   prevalence_;  // prevalence
	
	Polymorphism(int p_mutation_id, const MutationType *p_mutation_type_ptr, double p_selection_coeff, int p_subpop_index, int p_generation, int p_prevalence);
	
	void print(int p_index) const;
	
	void print(std::ofstream &p_outfile, int p_index) const;
	
	void print_no_id(int p_index) const;
};


#endif /* defined(__SLiM__polymorphism__) */




































































