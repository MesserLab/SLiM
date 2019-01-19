//
//  main.cpp
//  eidos
//
//  Created by Ben Haller on 9/15/15.
//  Copyright (c) 2015-2019 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

#include <iostream>
#include <fstream>
#include <string.h>
#include <string>

#include "time.h"

#include "eidos_global.h"
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
	if (!input_file)
		PrintUsageAndDie();
	
	// announce if we are running a debug build
#ifdef DEBUG
	std::cout << "// ********** DEBUG defined – you are not using a release build of Eidos" << std::endl << std::endl;
#endif
	
	// keep time (we do this whether or not the -time flag was passed)
	clock_t begin = clock();
	
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
	
	std::ifstream infile(input_file);
	
	if (!infile.is_open())
		EIDOS_TERMINATION << "ERROR (eidos): could not open input file: " << input_file << "." << EidosTerminate();
	
	infile.seekg(0, std::fstream::beg);
	std::stringstream buffer;
	
	buffer << infile.rdbuf();
	
	EidosScript script(buffer.str());
	
	// set up top-level error-reporting info
	gEidosCurrentScript = &script;
	gEidosExecutingRuntimeScript = false;
	
	script.Tokenize();
	script.ParseInterpreterBlockToAST(true);
	
	// reset error position indicators used by SLiMgui
	EidosScript::ClearErrorPosition();

	EidosSymbolTable *variable_symbols = new EidosSymbolTable(EidosSymbolTableType::kVariablesTable, gEidosConstantsSymbolTable);
	EidosFunctionMap function_map(*EidosInterpreter::BuiltInFunctionMap());
	EidosInterpreter interpreter(script, *variable_symbols, function_map, nullptr);
	
	EidosValue_SP result = interpreter.EvaluateInterpreterBlock(true, true);	// print output, return the last statement value (result not used)
	std::string output = interpreter.ExecutionOutput();
	
	std::cout << output << std::endl;
	
	// end timing and print elapsed time
	clock_t end = clock();
	double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
	
	if (keep_time)
		std::cout << "// ********** CPU time used: " << time_spent << std::endl;
	
	// print memory usage stats
	if (keep_mem)
	{
		peak_mem_usage = Eidos_GetPeakRSS();
		
		std::cout << "// ********** Initial memory usage: " << initial_mem_usage << " bytes (" << initial_mem_usage / 1024.0 << "K, " << initial_mem_usage / (1024.0 * 1024) << "MB)" << std::endl;
		std::cout << "// ********** Peak memory usage: " << peak_mem_usage << " bytes (" << peak_mem_usage / 1024.0 << "K, " << peak_mem_usage / (1024.0 * 1024) << "MB)" << std::endl;
	}
	
	return EXIT_SUCCESS;
}


























































