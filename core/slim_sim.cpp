//
//  slim_sim.cpp
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


#include "slim_sim.h"


using std::multimap;


SLiMSim::SLiMSim(const char *p_input_file, int *p_override_seed_ptr)
{
	// track the random number seed given, if there is one
	if (p_override_seed_ptr)
	{
		rng_seed_ = *p_override_seed_ptr;
		rng_seed_supplied_to_constructor_ = true;
	}
	else
	{
		rng_seed_ = 0;
		rng_seed_supplied_to_constructor_ = false;
	}
	
	// check the input file for syntactic correctness
	CheckInputFile(p_input_file);
	
	// initialize simulation parameters
	input_parameters_.push_back("#INPUT PARAMETER FILE");
	input_parameters_.push_back(p_input_file);
	
	// read all configuration information from the input file
	InitializeFromFile(p_input_file);
	
	// evolve over t generations
	std::cout << time_start_ << " " << time_duration_ << std::endl;
	
	// start at the beginning
	generation_ = time_start_;
}

void SLiMSim::RunOneGeneration(void)
{
	if (generation_ < time_start_ + time_duration_)
	{
		// execute demographic and substructure events in this generation 
		std::pair<multimap<const int,Event*>::iterator,multimap<const int,Event*>::iterator> event_range = events_.equal_range(generation_);
		
		for (multimap<const int,Event*>::iterator eventsIterator = event_range.first; eventsIterator != event_range.second; eventsIterator++)
			population_.ExecuteEvent(*eventsIterator->second, generation_, chromosome_, *this, &tracked_mutations_);
		
		// evolve all subpopulations
		for (const std::pair<const int,Subpopulation*> &subpop_pair : population_)
			population_.EvolveSubpopulation(subpop_pair.first, chromosome_, generation_);
		
		// introduce user-defined mutations
		std::pair<multimap<const int,IntroducedMutation>::iterator,multimap<const int,IntroducedMutation>::iterator> introd_mut_range = introduced_mutations_.equal_range(generation_);
		
		for (multimap<const int,IntroducedMutation>::iterator introduced_mutations_iter = introd_mut_range.first; introduced_mutations_iter != introd_mut_range.second; introduced_mutations_iter++)
			population_.IntroduceMutation(introduced_mutations_iter->second);
		
		// execute output events
		std::pair<multimap<const int,Event*>::iterator,multimap<const int,Event*>::iterator> output_event_range = outputs_.equal_range(generation_);
		
		for (multimap<const int,Event*>::iterator outputsIterator = output_event_range.first; outputsIterator != output_event_range.second; outputsIterator++)
			population_.ExecuteEvent(*outputsIterator->second, generation_, chromosome_, *this, &tracked_mutations_);
		
		// track particular mutation-types and set s = 0 for partial sweeps when completed
		if (tracked_mutations_.size() > 0 || partial_sweeps_.size() > 0)
			population_.TrackMutations(generation_, tracked_mutations_, &partial_sweeps_);
		
		// swap generations
		population_.SwapGenerations(generation_);
		
		// advance the generation counter as soon as the generation is done
		generation_++;
	}
}

void SLiMSim::RunToEnd(void)
{
	while (generation_ < time_start_ + time_duration_)
		RunOneGeneration();
}





































































