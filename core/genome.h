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


class Genome : public std::vector<Mutation>
{
};

// return a merged genome consisting only of the mutations that are present in both p_genome1 and p_genome2
Genome GenomeWithFixedMutations(const Genome &p_genome1, const Genome &p_genome2);

// return a merged genome consisting only of the mutations in p_genome1 that are not in p_genome2
Genome GenomeWithPolymorphicMutations(const Genome &p_genome1, const Genome &p_genome2);


#endif /* defined(__SLiM__genome__) */




































































