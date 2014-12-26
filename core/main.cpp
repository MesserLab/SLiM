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


void RunSLiM(char *p_input_file, int *p_override_seed);
void PrintUsageAndDie();


// Run a simulation using the given input file and the supplied seed (if it is not NULL)
void RunSLiM(char *p_input_file, int *p_override_seed)
{
	// check the input file for syntactic correctness
	CheckInputFile(p_input_file);
	
	// initialize simulation parameters
	int time_start;
	int time_duration;
	Chromosome chromosome;
	Population population;
	
	population.parameters_.push_back("#INPUT PARAMETER FILE");
	population.parameters_.push_back(p_input_file);
	
	// demographic and structure events
	multimap<const int,Event> events; 
	multimap<const int,Event>::iterator eventsIterator;
	
	// output events (time, output)
	multimap<const int,Event> outputs; 
	multimap<const int,Event>::iterator outputsIterator;
	
	// user-defined mutations that will be introduced (time, mutation)
	multimap<const int,IntroducedMutation> introduced_mutations; 
	multimap<const int,IntroducedMutation>::iterator introduced_mutations_iter;
	
	// tracked mutation-types
	std::vector<int> tracked_mutations; 
	
	// mutations undergoing partial sweeps
	std::vector<PartialSweep> partial_sweeps;
	
	// read all configuration information from the input file
	Initialize(population, p_input_file, chromosome, time_start, time_duration, events, outputs, introduced_mutations, partial_sweeps, population.parameters_, p_override_seed);
	
	// evolve over t generations
	std::cout << time_start << " " << time_duration << std::endl;
	
	for (int generation = time_start; generation < (time_start + time_duration); generation++)
	{ 
		// execute demographic and substructure events in this generation 
		std::pair<multimap<const int,Event>::iterator,multimap<const int,Event>::iterator> event_range = events.equal_range(generation);
		
		for (eventsIterator = event_range.first; eventsIterator != event_range.second; eventsIterator++)
			population.ExecuteEvent(eventsIterator->second, generation, chromosome, tracked_mutations);
		
		// evolve all subpopulations
		for (const std::pair<const int,Subpopulation> &subpop_pair : population)
			population.EvolveSubpopulation(subpop_pair.first, chromosome, generation);
		
		// introduce user-defined mutations
		std::pair<multimap<const int,IntroducedMutation>::iterator,multimap<const int,IntroducedMutation>::iterator> introd_mut_range = introduced_mutations.equal_range(generation);
		
		for (introduced_mutations_iter = introd_mut_range.first; introduced_mutations_iter != introd_mut_range.second; introduced_mutations_iter++)
			population.IntroduceMutation(introduced_mutations_iter->second);
		
		// execute output events
		std::pair<multimap<const int,Event>::iterator,multimap<const int,Event>::iterator> output_event_range = outputs.equal_range(generation);
		for (outputsIterator = output_event_range.first; outputsIterator != output_event_range.second; outputsIterator++)
			population.ExecuteEvent(outputsIterator->second, generation, chromosome, tracked_mutations);
		
		// track particular mutation-types and set s=0 for partial sweeps when completed
		if (tracked_mutations.size() > 0 || partial_sweeps.size() > 0)
			population.TrackMutations(generation, tracked_mutations, partial_sweeps);
		
		// swap generations
		population.SwapGenerations(generation);   
	}
}

void PrintUsageAndDie()
{
	std::cerr << "usage: slim [-seed <seed>] <parameter file>" << std::endl;
	exit(1);
}

int main(int argc, char *argv[])
{
	// parse command-line arguments
	int override_seed = 0;
	int *override_seed_ptr = NULL;			// by default, a seed is generated or supplied in the input file
	char *input_file = NULL;
	bool keep_time = false;
	
	for (int arg_index = 1; arg_index < argc; ++arg_index)
	{
		char *arg = argv[arg_index];
		
		// -seed <x>: override the default seed with the supplied seed value
		if (strcmp(arg, "-seed") == 0)
		{
			if (++arg_index == argc)
				PrintUsageAndDie();
			
			override_seed = atoi(argv[arg_index]);
			override_seed_ptr = &override_seed;
			
			continue;
		}
		
		// -time: take a time measurement and output it at the end of execution
		if (strcmp(arg, "-time") == 0)
		{
			keep_time = true;
			
			continue;
		}
		
		// this is the fall-through, which should be the input file, and should be the last argument given
		if (arg_index + 1 != argc)
			PrintUsageAndDie();
		
		input_file = argv[arg_index];
	}
	
	// check that we got what we need
	if (!input_file)
		PrintUsageAndDie();
	
	// run the simulation
	clock_t begin, end;
	double time_spent;
	
	begin = clock();
	
	RunSLiM(input_file, override_seed_ptr);
	
	end = clock();
	time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
	
	if (keep_time)
		std::cerr << "CPU time used: " << time_spent << std::endl;
	
	return 0;
}




































































