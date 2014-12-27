//
//  slim_sim.h
//  SLiM
//
//  Created by Ben Haller on 12/26/14.
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
 
 SLiM encapsulates a simulation run as a SLiMSim object.  This allows a simulation to be stepped and controlled by a GUI.
 
 */


#ifndef __SLiM__slim_sim__
#define __SLiM__slim_sim__


#include <stdio.h>
#include <map>
#include <vector>

#include "mutation.h"
#include "population.h"
#include "chromosome.h"
#include "event.h"


class SLiMSim
{
private:
	
	//
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	//
	SLiMSim(const SLiMSim&);						// disable copy constructor
	SLiMSim &operator = (const SLiMSim&);			// disable assignment operator
	
	
	// the start and duration for which the simulation will run, in generations, and the current generation
	int time_start_, time_duration_, generation_;
	
	// invocation parameters from input file
	std::vector<std::string> input_parameters_;
	
	// the chromosome, which defines genomic elements
	Chromosome chromosome_;
	
	// the population, which contains sub-populations
	Population population_;
	
	// demographic and structure events
	std::multimap<const int,Event*> events_; 
	
	// output events (time, output)
	std::multimap<const int,Event*> outputs_; 
	
	// user-defined mutations that will be introduced (time, mutation)
	std::multimap<const int,const IntroducedMutation*> introduced_mutations_; 
	
	// tracked mutation-types
	std::vector<int> tracked_mutations_; 
	
	// mutations undergoing partial sweeps
	std::vector<PartialSweep> partial_sweeps_;
	
	// this map is the owner of all allocated MutationType objects
	std::map<int,MutationType*>			mutation_types_;
	
	// this map is the owner of all allocated GenomicElementType objects
	std::map<int,GenomicElementType*>	genomic_element_types_;
	
	// random number generator seed info
	int rng_seed_;
	bool rng_seed_supplied_to_constructor_;
	
	
	// defined in slim_sim_input.cpp: check an input file for correctness and exit with a good error message if there is a problem
	void CheckInputFile(const char *p_input_file);
	
	// defined in slim_sim_input.cpp: initialize the population from the information in the file given
	void InitializePopulationFromFile(const char *p_file);
	
	// defined in slim_sim_input.cpp: parse a (previously checked) input file and set up the simulation state from its contents
	void InitializeFromFile(const char *p_input_file);
	
	
public:
	
	// construct a SLiMSim from an input file, with an optional RNG seed value (pass nullptr to get a generated seed)
	SLiMSim(const char *p_input_file, int *p_override_seed_ptr = nullptr);
	
	// run a single simulation generation and advance the generation counter
	void RunOneGeneration(void);
	
	// run the simulation to the end
	void RunToEnd(void);
	
	
	// accessors
	const std::vector<std::string> &InputParameters(void) const { return input_parameters_; }
	
};


#endif /* defined(__SLiM__slim_sim__) */












































































