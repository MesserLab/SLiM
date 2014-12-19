//
//  input_parsing.h
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
 
 Input parsing functions are defined here, mostly to keep them from clogging up main.cpp.
 
 */

#ifndef __SLiM__input_parsing__
#define __SLiM__input_parsing__


#include <iostream>
#include <string>
#include <map>

#include "Population.h"


// check an input file for correctness and exit with a good error message if there is a problem
void CheckInputFile(char *p_input_file);

// parse a (previously checked) input file and set up the simulation state from its contents
void Initialize(Population &p_population,
				char *p_input_file,
				Chromosome &p_chromosome,
				int &p_time_start,
				int &p_time_duration,
				std::multimap<int,Event> &p_events,
				std::multimap<int,Event> &p_outputs,
				std::multimap<int,IntroducedMutation> &p_introduced_mutations,
				std::vector<PartialSweep> &p_partial_sweeps,
				std::vector<std::string> &p_parameters,
				int *p_override_seed);


#endif /* defined(__SLiM__input_parsing__) */




































































