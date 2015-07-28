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


#include <iostream>

#include "slim_sim.h"
#include "slim_global.h"


void PrintUsageAndDie();

void PrintUsageAndDie()
{
	EIDOS_TERMINATION << "usage: slim -version | -usage | [-seed <seed>] [-time] <script file>" << eidos_terminate();
}

int main(int argc, char *argv[])
{
	// parse command-line arguments
	int override_seed = 0;
	int *override_seed_ptr = nullptr;			// by default, a seed is generated or supplied in the input file
	char *input_file = nullptr;
	bool keep_time = false;
	
	for (int arg_index = 1; arg_index < argc; ++arg_index)
	{
		char *arg = argv[arg_index];
		
		// -seed <x> or -s <x>: override the default seed with the supplied seed value
		if (strcmp(arg, "-seed") == 0 || strcmp(arg, "-s") == 0)
		{
			if (++arg_index == argc)
				PrintUsageAndDie();
			
			override_seed = atoi(argv[arg_index]);
			override_seed_ptr = &override_seed;
			
			continue;
		}
		
		// -time or -t: take a time measurement and output it at the end of execution
		if (strcmp(arg, "-time") == 0 || strcmp(arg, "-t") == 0)
		{
			keep_time = true;
			
			continue;
		}
		
		// -version or -v: print version information
		if (strcmp(arg, "-version") == 0 || strcmp(arg, "-v") == 0)
		{
			EIDOS_ERRSTREAM << "SLiM version 2.0a3, built " << __DATE__ << " " __TIME__ << std::endl;
			exit(0);
		}
		
		// -usage or -u: print usage information
		if (strcmp(arg, "-usage") == 0 || strcmp(arg, "-u") == 0 || strcmp(arg, "-?") == 0)
		{
			PrintUsageAndDie();
		}
		
		// this is the fall-through, which should be the input file, and should be the last argument given
		if (arg_index + 1 != argc)
			PrintUsageAndDie();
		
		input_file = argv[arg_index];
	}
	
	// check that we got what we need
	if (!input_file)
		PrintUsageAndDie();
	
	// announce if we are running a debug build
#ifdef DEBUG
	EIDOS_ERRSTREAM << "// ********** DEBUG defined – you are not using a release build of SLiM" << std::endl << std::endl;
#endif
	
	// keep time (we do this whether or not the -time flag was passed)
	clock_t begin = clock();
	
	// run the simulation
	SLiMSim *sim = new SLiMSim(input_file, override_seed_ptr);
	
	if (sim)
	{
		sim->RunToEnd();
		
		// clean up; but this is an unnecessary waste of time in the command-line context
		//delete sim;
		//gsl_rng_free(gEidos_rng);
	}
	
	// end timing and print elapsed time
	clock_t end = clock();
	double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
	
	if (keep_time)
		EIDOS_ERRSTREAM << "// ********** CPU time used: " << time_spent << std::endl;
	
	return EXIT_SUCCESS;
}




































































