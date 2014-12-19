//
//  substitution.h
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
 
 The class Substitution represents a mutation that has fixed in the population. Fixed mutations are converted to substitutions for
 efficiency, since such mutations no longer need to be tracked in each generation.  This class is not a subclass of Mutation, to
 avoid any possibility of instances of this class getting confused with mutation instances in the code.  It also adds one new
 piece of information, the time to fixation.
 
 */

#ifndef __SLiM__substitution__
#define __SLiM__substitution__


#import "mutation.h"
#import "chromosome.h"


class Substitution
{
public:
	
	int   mutation_type_;		// mutation type
	int   position_;			// position
	float selection_coeff_;		// selection coefficient
	int   subpop_index_;		// subpopulation in which mutation arose
	int   generation_;			// generation in which mutation arose  
	int   fixation_time_;		// fixation time
	
	Substitution(Mutation p_mutation, int p_fixation_time);
	
	void print(const Chromosome &p_chromosome) const;
};


#endif /* defined(__SLiM__substitution__) */




































































