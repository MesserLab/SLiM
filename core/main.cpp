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
#include "eidos_test.h"
#include "slim_test.h"


void PrintUsageAndDie();

void PrintUsageAndDie()
{
	SLIM_OUTSTREAM << "usage: slim -version | -usage | -testEidos | -testSLiM | [-seed <seed>] [-time] [-mem] [-Memhist] <script file>" << std::endl;
	exit(0);
}

int main(int argc, char *argv[])
{
	// parse command-line arguments
	unsigned long int override_seed = 0;					// this is the type defined for seeds by gsl_rng_set()
	unsigned long int *override_seed_ptr = nullptr;			// by default, a seed is generated or supplied in the input file
	const char *input_file = nullptr;
	bool keep_time = false, keep_mem = false, keep_mem_hist = false;
	
	// command-line SLiM generally terminates rather than throwing
	gEidosTerminateThrows = false;
	
	for (int arg_index = 1; arg_index < argc; ++arg_index)
	{
		const char *arg = argv[arg_index];
		
		// -seed <x> or -s <x>: override the default seed with the supplied seed value
		if (strcmp(arg, "-seed") == 0 || strcmp(arg, "-s") == 0)
		{
			if (++arg_index == argc)
				PrintUsageAndDie();
			
			override_seed = strtol(argv[arg_index], NULL, 10);
			override_seed_ptr = &override_seed;
			
			continue;
		}
		
		// -time or -t: take a time measurement and output it at the end of execution
		if (strcmp(arg, "-time") == 0 || strcmp(arg, "-t") == 0)
		{
			keep_time = true;
			
			continue;
		}
		
		// -mem or -m: take a peak memory usage measurement and output it at the end of execution
		if (strcmp(arg, "-mem") == 0 || strcmp(arg, "-m") == 0)
		{
			keep_mem = true;
			
			continue;
		}
		
		// -mem or -m: take a peak memory usage measurement and output it at the end of execution
		if (strcmp(arg, "-Memhist") == 0 || strcmp(arg, "-M") == 0)
		{
			keep_mem = true;		// implied by this
			keep_mem_hist = true;
			
			continue;
		}
		
		// -version or -v: print version information
		if (strcmp(arg, "-version") == 0 || strcmp(arg, "-v") == 0)
		{
			SLIM_OUTSTREAM << "SLiM version 2.0a3, built " << __DATE__ << " " __TIME__ << std::endl;
			exit(0);
		}
		
		// -testEidos or -te: run Eidos tests and quit
		if (strcmp(arg, "-testEidos") == 0 || strcmp(arg, "-te") == 0)
		{
			gEidosTerminateThrows = true;
			Eidos_WarmUp();
			RunEidosTests();
			exit(0);
		}
		
		// -testSLiM or -ts: run SLiM tests and quit
		if (strcmp(arg, "-testSLiM") == 0 || strcmp(arg, "-ts") == 0)
		{
			gEidosTerminateThrows = true;
			Eidos_WarmUp();
			SLiM_WarmUp();
			RunSLiMTests();
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
	SLIM_ERRSTREAM << "// ********** DEBUG defined – you are not using a release build of SLiM" << std::endl << std::endl;
#endif
	
	// keep time (we do this whether or not the -time flag was passed)
	clock_t begin = clock();
	
	// keep memory usage information, if asked to
	size_t *mem_record = nullptr;
	int mem_record_index = 0;
	int mem_record_capacity = 0;
	size_t initial_mem_usage = 0;
	size_t peak_mem_usage = 0;
	
	if (keep_mem_hist)
	{
		mem_record_capacity = 16384;
		mem_record = (size_t *)malloc(mem_record_capacity * sizeof(size_t));
	}
	
	if (keep_mem)
	{
		// note we subtract the size of our memory-tracking buffer, here and below
		initial_mem_usage = EidosGetCurrentRSS() - mem_record_capacity * sizeof(size_t);
	}
	
	// run the simulation
	Eidos_WarmUp();
	SLiM_WarmUp();
	
	SLiMSim *sim = new SLiMSim(input_file);
	sim->InitializeRNGFromSeed(override_seed_ptr);
	
	if (keep_mem_hist)
		mem_record[mem_record_index++] = EidosGetCurrentRSS() - mem_record_capacity * sizeof(size_t);
	
	if (sim)
	{
		// Run the simulation to its natural end
		while (sim->RunOneGeneration())
		{
			if (keep_mem_hist)
			{
				if (mem_record_index == mem_record_capacity)
				{
					mem_record_capacity <<= 1;
					mem_record = (size_t *)realloc(mem_record, mem_record_capacity * sizeof(size_t));
				}
				
				mem_record[mem_record_index++] = EidosGetCurrentRSS() - mem_record_capacity * sizeof(size_t);
			}
		}
		
		// clean up; but this is an unnecessary waste of time in the command-line context
		//delete sim;
		//gsl_rng_free(gEidos_rng);
	}
	
	// end timing and print elapsed time
	clock_t end = clock();
	double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
	
	if (keep_time)
		SLIM_ERRSTREAM << "// ********** CPU time used: " << time_spent << std::endl;
	
	// print memory usage stats
	if (keep_mem)
	{
		peak_mem_usage = EidosGetPeakRSS();
		
		SLIM_ERRSTREAM << "// ********** Initial memory usage: " << initial_mem_usage << " bytes (" << initial_mem_usage / 1024.0 << "K, " << initial_mem_usage / (1024.0 * 1024) << "MB)" << std::endl;
		SLIM_ERRSTREAM << "// ********** Peak memory usage: " << peak_mem_usage << " bytes (" << peak_mem_usage / 1024.0 << "K, " << peak_mem_usage / (1024.0 * 1024) << "MB)" << std::endl;
	}
	
	if (keep_mem_hist)
	{
		SLIM_ERRSTREAM << "// ********** Memory usage history (execute in R for a plot): " << std::endl;
		SLIM_ERRSTREAM << "memhist <- c(" << std::endl;
		for (int hist_index = 0; hist_index < mem_record_index; ++hist_index)
		{
			SLIM_ERRSTREAM << "   " << mem_record[hist_index];
			if (hist_index < mem_record_index - 1)
				SLIM_ERRSTREAM << ",";
			SLIM_ERRSTREAM << std::endl;
		}
		SLIM_ERRSTREAM << ")" << std::endl;
		SLIM_ERRSTREAM << "initial_mem <- " << initial_mem_usage << std::endl;
		SLIM_ERRSTREAM << "peak_mem <- " << peak_mem_usage << std::endl;
		SLIM_ERRSTREAM << "#scale <- 1; scale_tag <- \"bytes\"" << std::endl;
		SLIM_ERRSTREAM << "#scale <- 1024; scale_tag <- \"K\"" << std::endl;
		SLIM_ERRSTREAM << "scale <- 1024 * 1024; scale_tag <- \"MB\"" << std::endl;
		SLIM_ERRSTREAM << "#scale <- 1024 * 1024 * 1024; scale_tag <- \"GB\"" << std::endl;
		SLIM_ERRSTREAM << "plot(memhist / scale, type=\"l\", ylab=paste0(\"Memory usage (\", scale_tag, \")\"), xlab=\"Generation (start)\", ylim=c(0,peak_mem/scale), lwd=4)" << std::endl;
		SLIM_ERRSTREAM << "abline(h=peak_mem/scale, col=\"red\")" << std::endl;
		SLIM_ERRSTREAM << "abline(h=initial_mem/scale, col=\"blue\")" << std::endl;
		
		free(mem_record);
	}
	
	return EXIT_SUCCESS;
}




































































