//
//  main.cpp
//  SLiM
//
//  Created by Ben Haller on 12/12/14.
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
 
 This file defines main() and related functions that initiate and run a SLiM simulation.
 
 */


#include <map>
#include <iostream>

#include "input_parsing.h"
#include "mutation.h"
#include "population.h"
#include "chromosome.h"


using std::multimap;


int main(int argc, char *argv[])
{
	// initialize simulation parameters
	
	if (argc <= 1)
	{
		std::cerr << "usage: slim <parameter file>" << std::endl;
		exit(1);
	} 
	
	char *input_file = argv[1];
	CheckInputFile(input_file);
	
	int time_start;
	int time_duration;
	Chromosome chromosome;
	Population population;
	std::map<int,Subpopulation>::iterator subpopulation_iter;
	
	population.parameters_.push_back("#INPUT PARAMETER FILE");
	population.parameters_.push_back(input_file);
	
	// demographic and structure events
	multimap<int,Event> events; 
	multimap<int,Event>::iterator eventsIterator;
	
	// output events (time, output)
	multimap<int,Event> outputs; 
	multimap<int,Event>::iterator outputsIterator;
	
	// user-defined mutations that will be introduced (time, mutation)
	multimap<int,IntroducedMutation> introduced_mutations; 
	multimap<int,IntroducedMutation>::iterator introduced_mutations_iter;
	
	// tracked mutation-types
	std::vector<int> tracked_mutations; 
	
	// mutations undergoing partial sweeps
	std::vector<PartialSweep> partial_sweeps;
	
	// read all configuration information from the input fiel
	Initialize(population, input_file, chromosome, time_start, time_duration, events, outputs, introduced_mutations, partial_sweeps, population.parameters_);
	
	// evolve over t generations
	std::cout << time_start << " " << time_duration << std::endl;
	
	for (int generation = time_start; generation < (time_start + time_duration); generation++)
	{ 
		// execute demographic and substructure events in this generation 
		std::pair<multimap<int,Event>::iterator,multimap<int,Event>::iterator> event_range = events.equal_range(generation);
		
		for (eventsIterator = event_range.first; eventsIterator != event_range.second; eventsIterator++)
			population.ExecuteEvent(eventsIterator->second, generation, chromosome, tracked_mutations);
		
		// evolve all subpopulations
		for (subpopulation_iter = population.begin(); subpopulation_iter != population.end(); subpopulation_iter++)
			population.EvolveSubpopulation(subpopulation_iter->first, chromosome, generation);
		
		// introduce user-defined mutations
		std::pair<multimap<int,IntroducedMutation>::iterator,multimap<int,IntroducedMutation>::iterator> introd_mut_range = introduced_mutations.equal_range(generation);
		
		for (introduced_mutations_iter = introd_mut_range.first; introduced_mutations_iter != introd_mut_range.second; introduced_mutations_iter++)
			population.IntroduceMutation(introduced_mutations_iter->second, chromosome);
		
		// execute output events
		std::pair<multimap<int,Event>::iterator,multimap<int,Event>::iterator> output_event_range = outputs.equal_range(generation);
		for (outputsIterator = output_event_range.first; outputsIterator != output_event_range.second; outputsIterator++)
			population.ExecuteEvent(outputsIterator->second, generation, chromosome, tracked_mutations);
		
		// track particular mutation-types and set s=0 for partial sweeps when completed
		if (tracked_mutations.size() > 0 || partial_sweeps.size() > 0)
			population.TrackMutations(generation, tracked_mutations, partial_sweeps, chromosome);
		
		// swap generations
		population.SwapGenerations(generation, chromosome);   
	}
	
	return 0;
}




































































