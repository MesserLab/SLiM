//
//  genome.h
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
 
 The class Genome represents a particular genome, defined as a vector of mutations.  Each individual in the simulation has a genome,
 which determines that individual's fitness (from the fitness effects of all of the mutations possessed).
 
 */

#ifndef __SLiM__genome__
#define __SLiM__genome__


#include <vector>

#include "mutation.h"


class Genome : public std::vector<const Mutation*>
{
	// This class has a restricted copying policy; see below
	
private:
	
#ifdef DEBUG
	static bool s_log_copy_and_assign_;						// true if logging is disabled (see below)
#endif
	
public:
	
#ifdef DEBUG
	//
	//	This class should not be copied, in general, but the default copy constructor and assignment operator cannot be entirely
	//	disabled, because we want to keep instances of this class inside STL containers.  We therefore override the default copy
	//	constructor and the default assignment operator to log whenever they are called.  This is intended to reduce the risk of
	//	unintentional copying.  Logging can be disabled by bracketing with LogGenomeCopyAndAssign() when appropriate.
	//
	Genome(const Genome &p_original);
	Genome& operator= (const Genome &p_original);
	static bool LogGenomeCopyAndAssign(bool p_log);			// returns the old value; save and restore that value!
#endif
	
	Genome() = default;										// default constructor
};

// return a merged genome consisting only of the mutations that are present in both p_genome1 and p_genome2
Genome GenomeWithFixedMutations(const Genome &p_genome1, const Genome &p_genome2);

// return a merged genome consisting only of the mutations in p_genome1 that are not in p_genome2
Genome GenomeWithPolymorphicMutations(const Genome &p_genome1, const Genome &p_genome2);


#endif /* defined(__SLiM__genome__) */




































































