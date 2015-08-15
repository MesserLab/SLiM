//
//  slim_test.cpp
//  SLiM
//
//  Created by Ben Haller on 8/14/15.
//  Copyright (c) 2015 Messer Lab, http://messerlab.org/software/. All rights reserved.
//

#include "slim_test.h"
#include "slim_sim.h"

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>


using std::string;
using std::endl;


// Helper functions for testing
void SLiMAssertScriptSuccess(const string &p_script_string);
void SLiMAssertScriptRaise(const string &p_script_string);

// Keeping records of test success / failure
static int gSLiMTestSuccessCount = 0;
static int gSLiMTestFailureCount = 0;


// Instantiates and runs the script, and prints an error if the result does not match expectations
void SLiMAssertScriptSuccess(const string &p_script_string)
{
	gSLiMTestFailureCount++;	// assume failure; we will fix this at the end if we succeed
	
	SLiMSim *sim = nullptr;
	std::istringstream infile(p_script_string);
	
	try {
		sim = new SLiMSim(infile, nullptr);
	}
	catch (std::runtime_error err)
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise during new SLiMSim(): " << EidosGetTrimmedRaiseMessage() << endl;
		return;
	}
	
	try {
		while (sim->RunOneGeneration());
	}
	catch (std::runtime_error err)
	{
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise during RunOneGeneration(): " << EidosGetTrimmedRaiseMessage() << endl;
		return;
	}
	
	gSLiMTestFailureCount--;	// correct for our assumption of failure above
	gSLiMTestSuccessCount++;
	
	//std::cerr << p_script_string << " : \e[32mSUCCESS\e[0m" << endl;
}

void SLiMAssertScriptRaise(const string &p_script_string)
{
	try {
		std::istringstream infile(p_script_string);
		SLiMSim *sim = new SLiMSim(infile, nullptr);
		while (sim->RunOneGeneration());
		
		gSLiMTestFailureCount++;
		
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : no raise during SLiM execution." << endl;
	}
	catch (std::runtime_error err)
	{
		gSLiMTestSuccessCount++;
		
		// We need to call EidosGetTrimmedRaiseMessage() here to empty the error stringstream, even if we don't log the error
		std::string raise_message = EidosGetTrimmedRaiseMessage();
		
		//std::cerr << p_script_string << " == (expected raise) " << raise_message << " : \e[32mSUCCESS\e[0m" << endl;
		return;
	}
}

void RunSLiMTests(void)
{
	// Reset error counts
	gSLiMTestSuccessCount = 0;
	gSLiMTestFailureCount = 0;
	
	// Test that a basic script works
	SLiMAssertScriptSuccess("initialize() {"
							"   initializeMutationRate(1e-7);"
							"   initializeMutationType(\"m1\", 0.5, \"f\", 0.0);"
							"   initializeGenomicElementType(\"g1\", m1, 1.0);"
							"   initializeGenomicElement(g1, 0, 99999);"
							"   initializeRecombinationRate(1e-8);"
							"}"
							"1 { sim.addSubpop(\"p1\", 500); }"
							"5 { sim.outputFull(); }"
							);
	
	// Test that stop() raises as it is supposed to
	SLiMAssertScriptRaise("initialize() {"
						  "   initializeMutationRate(1e-7);"
						  "   initializeMutationType(\"m1\", 0.5, \"f\", 0.0);"
						  "   initializeGenomicElementType(\"g1\", m1, 1.0);"
						  "   initializeGenomicElement(g1, 0, 99999);"
						  "   initializeRecombinationRate(1e-8);"
						  "}"
						  "1 { sim.addSubpop(\"p1\", 500); }"
						  "3 { stop(\"fail!\"); }"
						  "5 { sim.outputFull(); }"
						  );
	
	// ************************************************************************************
	//
	//	Print a summary of test results
	//
	std::cerr << endl;
	if (gSLiMTestFailureCount)
		std::cerr << "\e[31mFAILURE\e[0m count: " << gSLiMTestFailureCount << endl;
	std::cerr << "\e[32mSUCCESS\e[0m count: " << gSLiMTestSuccessCount << endl;
}































































