//
//  subpopulation.h
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

#ifndef __SLiM__subpopulation__
#define __SLiM__subpopulation__


#include <vector>

#include "g_rng.h"
#include "genome.h"
#include "chromosome.h"


class subpopulation
{
	// a subpopulation is described by the vector G of 2N genomes
	// individual i is constituted by the two genomes 2*i and 2*i+1
	
private:
	
	gsl_ran_discrete_t* LT;   
	
public:
	
	int    N; // population size  
	double S; // selfing fraction
	
	std::vector<genome> G_parent;
	std::vector<genome> G_child;
	
	std::map<int,double> m; // m[i]: fraction made up of migrants from subpopulation i per generation
 
	
	subpopulation(int n);
	
	int draw_individual();
	
	void update_fitness(chromosome& chr);
	
	double W(int i, int j, chromosome& chr);
	
	void swap();
};


#endif /* defined(__SLiM__subpopulation__) */
