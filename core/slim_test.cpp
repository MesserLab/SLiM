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
void SLiMAssertScriptRaise(const string &p_script_string, const int p_bad_line, const int p_bad_position);

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
		while (sim->_RunOneGeneration());
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

void SLiMAssertScriptRaise(const string &p_script_string, const int p_bad_line, const int p_bad_position)
{
	try {
		std::istringstream infile(p_script_string);
		SLiMSim *sim = new SLiMSim(infile, nullptr);
		while (sim->_RunOneGeneration());
		
		gSLiMTestFailureCount++;
		
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : no raise during SLiM execution." << endl;
	}
	catch (std::runtime_error err)
	{
		// We need to call EidosGetTrimmedRaiseMessage() here to empty the error stringstream, even if we don't log the error
		std::string raise_message = EidosGetTrimmedRaiseMessage();
		
		if ((gEidosCharacterStartOfError == -1) || (gEidosCharacterEndOfError == -1) || !gEidosCurrentScript)
		{
			gSLiMTestFailureCount++;
			
			std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise expected, but no error info set" << endl;
			std::cerr << p_script_string << "   raise message: " << raise_message << endl;
			std::cerr << "--------------------" << std::endl << std::endl;
		}
		else
		{
			eidos_script_error_position(gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript);
			
			if ((gEidosErrorLine != p_bad_line) || (gEidosErrorLineCharacter != p_bad_position))
			{
				gSLiMTestFailureCount++;
				
				std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise expected, but error position unexpected" << endl;
				std::cerr << p_script_string << "   raise message: " << raise_message << endl;
				eidos_log_script_error(std::cerr, gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript, gEidosExecutingRuntimeScript);
				std::cerr << "--------------------" << std::endl << std::endl;
			}
			else
			{
				gSLiMTestSuccessCount++;
				
				//std::cerr << p_script_string << " == (expected raise) " << raise_message << " : \e[32mSUCCESS\e[0m" << endl;
			}
		}
	}
}

void RunSLiMTests(void)
{
	// Reset error counts
	gSLiMTestSuccessCount = 0;
	gSLiMTestFailureCount = 0;
	
	// Test that a basic script works
	SLiMAssertScriptSuccess("initialize() {\n"
							"   initializeMutationRate(1e-7);\n"
							"   initializeMutationType(\"m1\", 0.5, \"f\", 0.0);\n"
							"   initializeGenomicElementType(\"g1\", m1, 1.0);\n"
							"   initializeGenomicElement(g1, 0, 99999);\n"
							"   initializeRecombinationRate(1e-8);\n"
							"}\n"
							"1 { sim.addSubpop(\"p1\", 500); }\n"
							"5 { sim.outputFull(); }\n"
							);
	
	// Test that stop() raises as it is supposed to
	SLiMAssertScriptRaise("initialize() {\n"
						  "   initializeMutationRate(1e-7);\n"
						  "   initializeMutationType(\"m1\", 0.5, \"f\", 0.0);\n"
						  "   initializeGenomicElementType(\"g1\", m1, 1.0);\n"
						  "   initializeGenomicElement(g1, 0, 99999);\n"
						  "   initializeRecombinationRate(1e-8);\n"
						  "}\n"
						  "1 { sim.addSubpop(\"p1\", 500); }\n"
						  "3 { stop(\"fail!\"); }\n"
						  "5 { sim.outputFull(); }\n"
						  , 9, 4);
	
	// ************************************************************************************
	//
	//	Print a summary of test results
	//
	std::cerr << endl;
	if (gSLiMTestFailureCount)
		std::cerr << "\e[31mFAILURE\e[0m count: " << gSLiMTestFailureCount << endl;
	std::cerr << "\e[32mSUCCESS\e[0m count: " << gSLiMTestSuccessCount << endl;
	
	// Clear out the SLiM output stream post-test
	gSLiMOut.clear();
	gSLiMOut.str("");
}































































