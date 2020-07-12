//
//  main.cpp
//  eidos
//
//  Created by Ben Haller on 9/15/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

#include <iostream>
#include <fstream>
#include <string.h>
#include <string>
#include <ctime>
#include <chrono>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "eidos_globals.h"
#include "eidos_interpreter.h"
#include "eidos_test.h"


void PrintUsageAndDie();

void PrintUsageAndDie()
{
	std::cout << "usage: eidos -version | -usage | -testEidos | [-time] [-mem] <script file>" << std::endl;
	exit(0);
}

int main(int argc, const char * argv[])
{
	// command-line Eidos generally terminates rather than throwing
	gEidosTerminateThrows = false;
	
	// parse command-line arguments
	const char *input_file = nullptr;
	bool keep_time = false, keep_mem = false;
	
	// "slim" with no arguments prints usage, *unless* stdin is not a tty, in which case we're running the stdin script
	if ((argc == 1) && isatty(fileno(stdin)))
		PrintUsageAndDie();
	
	for (int arg_index = 1; arg_index < argc; ++arg_index)
	{
		const char *arg = argv[arg_index];
		
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
		
		// -version or -v: print version information
		if (strcmp(arg, "-version") == 0 || strcmp(arg, "-v") == 0)
		{
			std::cout << "Eidos version " << EIDOS_VERSION_STRING << ", built " << __DATE__ << " " __TIME__ << std::endl;
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
			exit(test_result);
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
	if (!input_file && isatty(fileno(stdin)))
		PrintUsageAndDie();
	
	// announce if we are running a debug build
#ifdef DEBUG
	std::cout << "// ********** DEBUG defined – you are not using a release build of Eidos" << std::endl << std::endl;
#endif
	
	// keep time (we do this whether or not the -time flag was passed)
	std::clock_t begin_cpu = std::clock();
	std::chrono::steady_clock::time_point begin_wall = std::chrono::steady_clock::now();
	
	// keep memory usage information, if asked to
	size_t initial_mem_usage = 0;
	size_t peak_mem_usage = 0;
	
	if (keep_mem)
	{
		// note we subtract the size of our memory-tracking buffer, here and below
		initial_mem_usage = Eidos_GetCurrentRSS();
	}
	
	// warm up and load the script
	Eidos_WarmUp();
	Eidos_FinishWarmUp();
	EidosScript::ClearErrorPosition();
	
	EidosScript *script = nullptr;
	
	if (!input_file)
	{
		// no input file supplied; either the user forgot (if stdin is a tty) or they're piping a script into stdin
		// we checked for the tty case above, so here we assume stdin will supply the script
		std::stringstream buffer;
		
		buffer << std::cin.rdbuf();
		
		script = new EidosScript(buffer.str());
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
				EIDOS_TERMINATION << std::endl << "ERROR (main): input file " << input_file << " is not a regular file or a fifo (it might be a directory or other special file)." << EidosTerminate();
			}
			fclose(fp);
		}
		
		std::ifstream infile(input_file);
		
		if (!infile.is_open())
			EIDOS_TERMINATION << "ERROR (eidos): could not open input file: " << input_file << "." << EidosTerminate();
		
		infile.seekg(0, std::fstream::beg);
		std::stringstream buffer;
		
		buffer << infile.rdbuf();
		
		script = new EidosScript(buffer.str());
	}
	
	// set up top-level error-reporting info
	gEidosCurrentScript = script;
	gEidosExecutingRuntimeScript = false;
	
	script->Tokenize();
	script->ParseInterpreterBlockToAST(true);
	
	// reset error position indicators used by SLiMgui
	EidosScript::ClearErrorPosition();

	EidosSymbolTable *variable_symbols = new EidosSymbolTable(EidosSymbolTableType::kVariablesTable, gEidosConstantsSymbolTable);
	EidosFunctionMap function_map(*EidosInterpreter::BuiltInFunctionMap());
	EidosInterpreter interpreter(*script, *variable_symbols, function_map, nullptr);
	
	EidosValue_SP result = interpreter.EvaluateInterpreterBlock(true, true);	// print output, return the last statement value (result not used)
	std::string output = interpreter.ExecutionOutput();
	
	std::cout << output << std::endl;
	
	Eidos_FlushFiles();
	
	// end timing and print elapsed time
	std::clock_t end_cpu = std::clock();
	std::chrono::steady_clock::time_point end_wall = std::chrono::steady_clock::now();
	double cpu_time_secs = static_cast<double>(end_cpu - begin_cpu) / CLOCKS_PER_SEC;
	double wall_time_secs = std::chrono::duration<double>(end_wall - begin_wall).count();
	
	if (keep_time)
	{
		std::cout << "// ********** CPU time used: " << cpu_time_secs << std::endl;
		std::cout << "// ********** Wall time used: " << wall_time_secs << std::endl;
	}
	
	// print memory usage stats
	if (keep_mem)
	{
		peak_mem_usage = Eidos_GetPeakRSS();
		
		std::cout << "// ********** Initial memory usage: " << initial_mem_usage << " bytes (" << initial_mem_usage / 1024.0 << "K, " << initial_mem_usage / (1024.0 * 1024) << "MB)" << std::endl;
		std::cout << "// ********** Peak memory usage: " << peak_mem_usage << " bytes (" << peak_mem_usage / 1024.0 << "K, " << peak_mem_usage / (1024.0 * 1024) << "MB)" << std::endl;
	}
	
	return EXIT_SUCCESS;
}


























































