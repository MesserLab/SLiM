//
//  introduced_mutation.h
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
 
 The class IntroducedMutation represents a prearranged mutation defined in the input file.  In addition to the usual characteristics of
 a mutation, the initial number of homozygotes and heterozygotes to be set up in the affected subpopulation is defined.
 
 */

#ifndef __SLiM__introduced_mutation__
#define __SLiM__introduced_mutation__


#import "mutation.h"

#include <iostream>


class IntroducedMutation : public Mutation
{
public:
	
	int num_homozygotes_;
	int num_heterozygotes_;
	
	IntroducedMutation(int p_mutation_type, int p_position, int p_subpop_index, int p_generation, int p_num_homozygotes, int p_num_heterozygotes);
};

// support stream output of IntroducedMutation, for debugging
std::ostream &operator<<(std::ostream &p_outstream, const IntroducedMutation &p_introduced_mutation);


#endif /* defined(__SLiM__introduced_mutation__) */




































































