//
//  population.h
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

#ifndef __SLiM__population__
#define __SLiM__population__


#include <vector>
#include <map>
#include <string>

#include "subpopulation.h"
#include "substitution.h"
#include "chromosome.h"
#include "polymorphism.h"
#include "event.h"
#include "introduced_mutation.h"
#include "partial_sweep.h"


class population : public std::map<int,subpopulation>
{
	// the population is a map of subpopulations
	
public: 
	
	std::vector<substitution> Substitutions;
	
	std::map<int,subpopulation>::iterator it;
	
	std::vector<std::string> parameters;
	
	void add_subpopulation(int i, unsigned int N);
	
	void add_subpopulation(int i, int j, unsigned int N);
	
	void set_size(int i, unsigned int N);
	
	void set_selfing(int i, double s);
	
	void set_migration(int i, int j, double m);
	
	void execute_event(event& E, int g, chromosome& chr, std::vector<int>& FM);
	
	void introduce_mutation(introduced_mutation M, chromosome& chr);
	
	void track_mutations(int g, std::vector<int>& TM, std::vector<partial_sweep>& PS, chromosome& chr);
	
	void evolve_subpopulation(int i, chromosome& chr, int g);
	
	void crossover_mutation(int i, int c, int j, int P1, int P2, chromosome& chr, int g);
	
	void swap_generations(int g, chromosome& chr);
	
	void remove_fixed(int g);
	
	void print_all(chromosome& chr);
	
	void print_all(std::ofstream& outfile, chromosome& chr);
	
	void print_sample(int i, int n, chromosome& chr);
	
	void print_sample_ms(int i, int n, chromosome& chr);
	
	int find_mut(std::multimap<int,polymorphism>& P, mutation m);
	
	void add_mut(std::multimap<int,polymorphism>& P, mutation m);
};


#endif /* defined(__SLiM__population__) */
