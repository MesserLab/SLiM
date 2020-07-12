//
//  main.cpp
//  SLiM
//
//  Created by Ben Haller on 12/12/14.
//  Copyright (c) 2014-2020 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
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
#include <fstream>
#include <string>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <ctime>
#include <chrono>
#include <sys/stat.h>

#include "slim_sim.h"
#include "slim_globals.h"
#include "eidos_test.h"
#include "slim_test.h"
#include "eidos_test_element.h"
#include "eidos_symbol_table.h"


static void PrintUsageAndDie(bool p_print_header, bool p_print_full_usage)
{
	if (p_print_header)
	{
		SLIM_OUTSTREAM << "SLiM version " << SLIM_VERSION_STRING << ", built " << __DATE__ << " " __TIME__ << "." << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "SLiM is a product of the Messer Lab, http://messerlab.org/" << std::endl;
		SLIM_OUTSTREAM << "Copyright 2013-2020 Philipp Messer.  All rights reserved." << std::endl << std::endl;
		SLIM_OUTSTREAM << "By Benjamin C. Haller, http://benhaller.com/, and Philipp Messer." << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "---------------------------------------------------------------------------------" << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "To cite SLiM in publications please use:" << std::endl << std::endl;
		SLIM_OUTSTREAM << "Haller, B.C., and Messer, P.W. (2019). SLiM 3: Forward genetic simulations" << std::endl;
		SLIM_OUTSTREAM << "beyond the Wright–Fisher model. Molecular Biology and Evolution 36(3), 632-637." << std::endl;
		SLIM_OUTSTREAM << "DOI: https://doi.org/10.1093/molbev/msy228" << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "For papers using tree-sequence recording, please cite:" << std::endl << std::endl;
		SLIM_OUTSTREAM << "Haller, B.C., Galloway, J., Kelleher, J., Messer, P.W., & Ralph, P.L. (2019)." << std::endl;
		SLIM_OUTSTREAM << "Tree‐sequence recording in SLiM opens new horizons for forward‐time simulation" << std::endl;
		SLIM_OUTSTREAM << "of whole genomes. Molecular Ecology Resources 19(2), 552-566." << std::endl;
		SLIM_OUTSTREAM << "DOI: https://doi.org/10.1111/1755-0998.12968" << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "---------------------------------------------------------------------------------" << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "SLiM home page: http://messerlab.org/slim/" << std::endl;
		SLIM_OUTSTREAM << "slim-announce mailing list: https://groups.google.com/d/forum/slim-announce" << std::endl;
		SLIM_OUTSTREAM << "slim-discuss mailing list: https://groups.google.com/d/forum/slim-discuss" << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "---------------------------------------------------------------------------------" << std::endl << std::endl;

		SLIM_OUTSTREAM << "SLiM is free software: you can redistribute it and/or modify it under the terms" << std::endl;
		SLIM_OUTSTREAM << "of the GNU General Public License as published by the Free Software Foundation," << std::endl;
		SLIM_OUTSTREAM << "either version 3 of the License, or (at your option) any later version." << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;" << std::endl;
		SLIM_OUTSTREAM << "without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR" << std::endl;
		SLIM_OUTSTREAM << "PURPOSE.  See the GNU General Public License for more details." << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "You should have received a copy of the GNU General Public License along with" << std::endl;
		SLIM_OUTSTREAM << "SLiM.  If not, see <http://www.gnu.org/licenses/>." << std::endl << std::endl;
		
		SLIM_OUTSTREAM << "---------------------------------------------------------------------------------" << std::endl << std::endl;
	}
	
	SLIM_OUTSTREAM << "usage: slim -v[ersion] | -u[sage] | -testEidos | -testSLiM |" << std::endl;
	SLIM_OUTSTREAM << "   [-l[ong] [<l>]] [-s[eed] <seed>] [-t[ime]] [-m[em]] [-M[emhist]] [-x]" << std::endl;
	SLIM_OUTSTREAM << "   [-d[efine] <def>] [<script file>]" << std::endl;
	
	if (p_print_full_usage)
	{
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "   -v[ersion]       : print SLiM's version information" << std::endl;
		SLIM_OUTSTREAM << "   -u[sage]         : print command-line usage help" << std::endl;
		SLIM_OUTSTREAM << "   -testEidos | -te : run built-in self-diagnostic tests of Eidos" << std::endl;
		SLIM_OUTSTREAM << "   -testSLiM | -ts  : run built-in self-diagnostic tests of SLiM" << std::endl;
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "   -l[ong] [<l>]    : long (i.e., verbose) output of level <l> (default 2)" << std::endl;
		SLIM_OUTSTREAM << "   -s[eed] <seed>   : supply an initial random number seed for SLiM" << std::endl;
		SLIM_OUTSTREAM << "   -t[ime]          : print SLiM's total execution time (in user clock time)" << std::endl;
		SLIM_OUTSTREAM << "   -m[em]           : print SLiM's peak memory usage" << std::endl;
		SLIM_OUTSTREAM << "   -M[emhist]       : print a histogram of SLiM's memory usage" << std::endl;
		SLIM_OUTSTREAM << "   -x               : disable SLiM's runtime safety/consistency checks" << std::endl;
		SLIM_OUTSTREAM << "   -d[efine] <def>  : define an Eidos constant, such as \"mu=1e-7\"" << std::endl;
		SLIM_OUTSTREAM << "   <script file>    : the input script file (stdin may be used instead)" << std::endl;
	}
	
	if (p_print_header || p_print_full_usage)
		SLIM_OUTSTREAM << std::endl;
	
	exit(0);
}

#if SLIM_LEAK_CHECKING
static void clean_up_leak_false_positives(void)
{
	// This does a little cleanup that helps Valgrind to understand that some things have not been leaked.
	// I think perhaps unordered_map keeps values in an unaligned manner that Valgrind doesn't see as pointers.
	Eidos_FreeGlobalStrings();
	EidosTestElement::FreeThunks();
	MutationRun::DeleteMutationRunFreeList();
	FreeSymbolTablePool();
	Eidos_FreeRNG(gEidos_RNG);
}
#endif

static void test_exit(int test_result)
{
#if SLIM_LEAK_CHECKING
	clean_up_leak_false_positives();
	
	// sleep() to give time to assess leaks at the command line
	std::cout << "\nSLEEPING" << std::endl;
	sleep(100000);
#endif
	
	exit(test_result);
}

int main(int argc, char *argv[])
{
	// parse command-line arguments
	unsigned long int override_seed = 0;					// this is the type used for seeds in the GSL
	unsigned long int *override_seed_ptr = nullptr;			// by default, a seed is generated or supplied in the input file
	const char *input_file = nullptr;
	bool keep_time = false, keep_mem = false, keep_mem_hist = false, skip_checks = false, tree_seq_checks = false;
	std::vector<std::string> defined_constants;
	
	// command-line SLiM generally terminates rather than throwing
	gEidosTerminateThrows = false;
	
	// "slim" with no arguments prints usage, *unless* stdin is not a tty, in which case we're running the stdin script
	if ((argc == 1) && isatty(fileno(stdin)))
		PrintUsageAndDie(true, true);
	
	for (int arg_index = 1; arg_index < argc; ++arg_index)
	{
		const char *arg = argv[arg_index];
		
		// -long or -l [<l>]: switches to long (i.e. verbose) output, with an optional integer level specifier
		if (strcmp(arg, "-long") == 0 || strcmp(arg, "-l") == 0)
		{
			if (arg_index + 1 == argc)
			{
				// -l[ong] is the last command-line argument, so there is no level; default to 2
				SLiM_verbosity_level = 2;
			}
			else
			{
				// there is another argument following; if it is an integer, we eat it
				const char *s = argv[arg_index + 1];
				bool is_digits = true;
				
				while (*s) {
					if (isdigit(*s++) == 0)
					{
						is_digits = false;
						break;
					}
				}
				
				if (!is_digits)
				{
					// the argument contains non-digit characters, so assume it is not intended for us
					SLiM_verbosity_level = 2;
				}
				else
				{
					errno = 0;
					long verbosity = strtol(argv[arg_index + 1], NULL, 10);
					
					if (errno)
					{
						// the argument did not parse as an integer, so assume it is not intended for us
						SLiM_verbosity_level = 2;
					}
					else
					{
						// the argument parsed as an integer, so it is used to set the verbosity level
						if ((verbosity < 0) || (verbosity > 2))
						{
							SLIM_ERRSTREAM << "Verbosity level supplied to -l[ong] must be 0, 1, or 2." << std::endl;
							exit(0);
						}
						
						SLiM_verbosity_level = verbosity;
						arg_index++;
					}
				}
			}
			
			// SLIM_OUTSTREAM << "// ********** Verbosity level set to " << SLiM_verbosity_level << std::endl << std::endl;
			
			continue;
		}
		
		// -seed <x> or -s <x>: override the default seed with the supplied seed value
		if (strcmp(arg, "-seed") == 0 || strcmp(arg, "-s") == 0)
		{
			if (++arg_index == argc)
				PrintUsageAndDie(false, true);
			
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
		
		// -x: skip runtime checks for greater speed, or to avoid them if they are causing problems
		if (strcmp(arg, "-x") == 0)
		{
			skip_checks = true;
			eidos_do_memory_checks = false;
			
			continue;
		}
		
		// -version or -v: print version information
		if (strcmp(arg, "-version") == 0 || strcmp(arg, "-v") == 0)
		{
			SLIM_OUTSTREAM << "SLiM version " << SLIM_VERSION_STRING << ", built " << __DATE__ << " " __TIME__ << std::endl;
			exit(0);
		}
		
		// -testEidos or -te: run Eidos tests and quit
		if (strcmp(arg, "-testEidos") == 0 || strcmp(arg, "-te") == 0)
		{
			gEidosTerminateThrows = true;
			Eidos_WarmUp();
			Eidos_FinishWarmUp();
			
			int test_result = RunEidosTests();
			
			Eidos_FlushFiles();
			test_exit(test_result);
		}
		
		// -testSLiM or -ts: run SLiM tests and quit
		if (strcmp(arg, "-testSLiM") == 0 || strcmp(arg, "-ts") == 0)
		{
			gEidosTerminateThrows = true;
			Eidos_WarmUp();
			SLiM_WarmUp();
			Eidos_FinishWarmUp();
			
			int test_result = RunSLiMTests();
			
			Eidos_FlushFiles();
			test_exit(test_result);
		}
		
		// -usage or -u: print usage information
		if (strcmp(arg, "-usage") == 0 || strcmp(arg, "-u") == 0 || strcmp(arg, "-?") == 0)
			PrintUsageAndDie(false, true);
		
		// -define or -d: define Eidos constants
		if (strcmp(arg, "-define") == 0 || strcmp(arg, "-d") == 0)
		{
			if (++arg_index == argc)
				PrintUsageAndDie(false, true);
			
			defined_constants.push_back(argv[arg_index]);
			
			continue;
		}
		
		// -TSXC is an undocumented command-line flag that turns on tree-sequence recording and runtime crosschecks
		if (strcmp(arg, "-TSXC") == 0)
		{
			tree_seq_checks = true;
			continue;
		}
		
		// this is the fall-through, which should be the input file, and should be the last argument given
		if (arg_index + 1 != argc)
		{
			SLIM_ERRSTREAM << "Unrecognized command-line argument: " << arg << std::endl << std::endl;
			
			PrintUsageAndDie(false, true);
		}
		
		input_file = argv[arg_index];
	}
	
	// check that we got what we need; if no file was supplied, then stdin must not be a tty (i.e., must be a pipe, etc.)
	if (!input_file && isatty(fileno(stdin)))
		PrintUsageAndDie(false, true);
	
	// announce if we are running a debug build or are skipping runtime checks
#ifdef DEBUG
	SLIM_ERRSTREAM << "// ********** DEBUG defined – you are not using a release build of SLiM" << std::endl << std::endl;
#endif
	if (SLiM_verbosity_level >= 2)
		SLIM_ERRSTREAM << "// ********** The -l[ong] command-line option has enabled verbose output (level " << SLiM_verbosity_level << ")" << std::endl << std::endl;
	if (skip_checks)
		SLIM_ERRSTREAM << "// ********** The -x command-line option has disabled some runtime checks" << std::endl << std::endl;
	
	// emit defined constants in verbose mode
	if (defined_constants.size() && (SLiM_verbosity_level >= 2))
	{
		for (std::string &constant : defined_constants)
			std::cout << "-d[efine]: " << constant << std::endl;
		std::cout << std::endl;
	}
	
	// keep time (we do this whether or not the -time flag was passed)
	std::clock_t begin_cpu = std::clock();
	std::chrono::steady_clock::time_point begin_wall = std::chrono::steady_clock::now();
	
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
		initial_mem_usage = Eidos_GetCurrentRSS() - mem_record_capacity * sizeof(size_t);
	}
	
	// run the simulation
	Eidos_WarmUp();
	SLiM_WarmUp();
	Eidos_FinishWarmUp();
	
	SLiMSim *sim = nullptr;
	
	if (!input_file)
	{
		// no input file supplied; either the user forgot (if stdin is a tty) or they're piping a script into stdin
		// we checked for the tty case above, so here we assume stdin will supply the script
		sim = new SLiMSim(std::cin);
	}
	else
	{
		// BCH 1/18/2020: check that input_file is a valid path to a file that we can access before opening it
		{
			FILE *fp = fopen(input_file, "r");
			
			if (!fp)
				EIDOS_TERMINATION << std::endl << "ERROR (main): could not open input file: " << input_file << "." << EidosTerminate();
			
			struct stat fileInfo;
			int retval = fstat(fileno(fp), &fileInfo);
			
			if (retval != 0)
				EIDOS_TERMINATION << std::endl << "ERROR (main): could not access input file: " << input_file << "." << EidosTerminate();
			
			// BCH 30 March 2020: adding S_ISFIFO() as a permitted file type here, to re-enable redirection of input
			if (!S_ISREG(fileInfo.st_mode) && !S_ISFIFO(fileInfo.st_mode))
			{
				fclose(fp);
				EIDOS_TERMINATION << std::endl << "ERROR (main): input file " << input_file << " is not a regular file (it might be a directory or other special file)." << EidosTerminate();
			}
			fclose(fp);
		}
		
		// input file supplied; open it and use it
		std::ifstream infile(input_file);
		
		if (!infile.is_open())
			EIDOS_TERMINATION << std::endl << "ERROR (main): could not open input file: " << input_file << "." << EidosTerminate();
		
		sim = new SLiMSim(infile);
	}
	
	if (keep_mem_hist)
		mem_record[mem_record_index++] = Eidos_GetCurrentRSS() - mem_record_capacity * sizeof(size_t);
	
	if (sim)
	{
		sim->InitializeRNGFromSeed(override_seed_ptr);
		
		Eidos_DefineConstantsFromCommandLine(defined_constants);	// do this after the RNG has been set up
		
		for (int arg_index = 0; arg_index < argc; ++arg_index)
			sim->cli_params_.push_back(argv[arg_index]);
		
		if (tree_seq_checks)
			sim->TSXC_Enable();
		
#if DO_MEMORY_CHECKS
		// We check memory usage at the end of every 10 generations, to be able to provide the user with a decent error message
		// if the maximum memory limit is exceeded.  Every 10 generations is a compromise; these checks do take a little time.
		// Even with a model that runs through generations very quickly, though, checking every 10 makes little difference.
		// Models in which the generations take longer will see no measurable difference in runtime at all.  Note that these
		// checks can be disabled with the -x command-line option.
		int mem_check_counter = 0, mem_check_mod = 10;
#endif
		
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
				
				mem_record[mem_record_index++] = Eidos_GetCurrentRSS() - mem_record_capacity * sizeof(size_t);
			}
			
#if DO_MEMORY_CHECKS
			if (eidos_do_memory_checks)
			{
				mem_check_counter++;
				
				if (mem_check_counter % mem_check_mod == 0)
				{
					// Check memory usage at the end of the generation, so we can print a decent error message
					std::ostringstream message;
					
					message << "(Limit exceeded at end of generation " << sim->Generation() << ".)" << std::endl;
					
					Eidos_CheckRSSAgainstMax("main()", message.str());
				}
			}
#endif
		}
		
		// clean up; but most of this is an unnecessary waste of time in the command-line context
		Eidos_FlushFiles();
		
#if SLIM_LEAK_CHECKING
		delete sim;
		sim = nullptr;
		clean_up_leak_false_positives();
		
		// sleep() to give time to assess leaks at the command line
		std::cout << "\nSLEEPING" << std::endl;
		sleep(100000);
#endif
	}
	
	// end timing and print elapsed time
	std::clock_t end_cpu = std::clock();
	std::chrono::steady_clock::time_point end_wall = std::chrono::steady_clock::now();
	double cpu_time_secs = static_cast<double>(end_cpu - begin_cpu) / CLOCKS_PER_SEC;
	double wall_time_secs = std::chrono::duration<double>(end_wall - begin_wall).count();
	
	if (keep_time)
	{
		SLIM_ERRSTREAM << "// ********** CPU time used: " << cpu_time_secs << std::endl;
		SLIM_ERRSTREAM << "// ********** Wall time used: " << wall_time_secs << std::endl;
	}
	
	// print memory usage stats
	if (keep_mem)
	{
		peak_mem_usage = Eidos_GetPeakRSS();
		
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




































































