//
//  mutation.h
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
 
 The class Mutation represents a single mutation, defined by its type, its position on the chromosome, and its selection coefficient.
 Mutations are also tagged with the subpopulation and generation in which they arose.
 
 */

#ifndef __SLiM__mutation__
#define __SLiM__mutation__


#include <iostream>


class Mutation
{
public:
	
	int   mutation_type_;		// mutation type identifier
	int   position_;			// position on the chromosome
	float selection_coeff_;		// selection coefficient
	int   subpop_index_;		// subpopulation in which mutation arose
	int   generation_;			// generation in which mutation arose  
	
	// null constructor
	Mutation(void);
	
	// standard constructor, supplying values for all ivars
	Mutation(int p_mutation_type, int p_position, double p_selection_coeff, int p_subpop_index, int p_generation);
};

// true if M1 has an earlier (smaller) position than M2
bool operator< (const Mutation &p_mutation1, const Mutation &p_mutation2);

// true if M1 and M2 have the same position, type, and selection coefficient
bool operator== (const Mutation &p_mutation1, const Mutation &p_mutation2);

// support stream output of Mutation, for debugging
std::ostream& operator<<(std::ostream& p_outstream, const Mutation& p_mutation);


#endif /* defined(__SLiM__mutation__) */




































































