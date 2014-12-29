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
#include "mutation_type.h"
#include "population.h"
#include "chromosome.h"
#include "event.h"


class SLiMSim
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.
	
private:
	
	int time_start_, time_duration_, generation_;									// the start and duration for which the simulation will run, in generations, and the current generation
	std::vector<std::string> input_parameters_;										// invocation parameters from input file
	Chromosome chromosome_;															// the chromosome, which defines genomic elements
	Population population_;															// the population, which contains sub-populations
	std::map<int,MutationType*> mutation_types_;									// OWNED POINTERS: this map is the owner of all allocated MutationType objects
	std::map<int,GenomicElementType*> genomic_element_types_;						// OWNED POINTERS: this map is the owner of all allocated MutationType objects
	std::multimap<const int,Event*> events_;										// OWNED POINTERS: demographic and structure events
	std::multimap<const int,Event*> outputs_;										// OWNED POINTERS: output events (time, output)
	std::multimap<const int,const IntroducedMutation*> introduced_mutations_;		// OWNED POINTERS: user-defined mutations that will be introduced (time, mutation)
	std::vector<const PartialSweep*> partial_sweeps_;								// OWNED POINTERS: mutations undergoing partial sweeps
	std::vector<MutationType*> tracked_mutations_;									// tracked mutation-types; these pointers are not owned (they are owned by mutation_types_, above)
	int rng_seed_;																	// random number generator seed
	bool rng_seed_supplied_to_constructor_;											// true if the RNG seed was supplied, which means it overrides other RNG seed sources
	
	// private initialization methods
	void CheckInputFile(const char *p_input_file);									// check an input file for correctness and exit with a good error message if there is a problem
	void InitializePopulationFromFile(const char *p_file);							// initialize the population from the information in the file given
	void InitializeFromFile(const char *p_input_file);								// parse a (previously checked) input file and set up the simulation state from its contents
	
public:
	
	SLiMSim(const SLiMSim&) = delete;												// no copying
	SLiMSim& operator=(const SLiMSim&) = delete;									// no copying
	SLiMSim(const char *p_input_file, int *p_override_seed_ptr = nullptr);			// construct a SLiMSim from an input file, with an optional RNG seed value
	~SLiMSim(void);																	// destructor
	
	void RunOneGeneration(void);													// run a single simulation generation and advance the generation counter
	void RunToEnd(void);															// run the simulation to the end
	
	// accessors
	inline const std::vector<std::string> &InputParameters(void) const				{ return input_parameters_; }
	inline const std::map<int,MutationType*> &MutationTypes(void) const				{ return mutation_types_; }
	
};


#endif /* defined(__SLiM__slim_sim__) */












































































