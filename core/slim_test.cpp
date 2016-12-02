//
//  slim_test.cpp
//  SLiM
//
//  Created by Ben Haller on 8/14/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

#include "slim_test.h"
#include "slim_sim.h"
#include "eidos_test.h"

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>


using std::string;
using std::endl;


// Helper functions for testing
void SLiMAssertScriptSuccess(const string &p_script_string, int lineNumber = -1);
void SLiMAssertScriptRaise(const string &p_script_string, const int p_bad_line, const int p_bad_position, const std::string &p_reason_snip, int lineNumber = -1);
void SLiMAssertScriptStop(const string &p_script_string, int lineNumber = -1);

// Keeping records of test success / failure
static int gSLiMTestSuccessCount = 0;
static int gSLiMTestFailureCount = 0;


// Instantiates and runs the script, and prints an error if the result does not match expectations
void SLiMAssertScriptSuccess(const string &p_script_string, int lineNumber)
{
	gSLiMTestFailureCount++;	// assume failure; we will fix this at the end if we succeed
	
	SLiMSim *sim = nullptr;
	std::istringstream infile(p_script_string);
	
	try {
		sim = new SLiMSim(infile);
		sim->InitializeRNGFromSeed(nullptr);
	}
	catch (std::runtime_error err)
	{
		if (lineNumber != -1)
			std::cerr << "[" << lineNumber << "] ";
		
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during new SLiMSim(): " << EidosGetTrimmedRaiseMessage() << endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		return;
	}
	
	try {
		while (sim->_RunOneGeneration());
	}
	catch (std::runtime_error err)
	{
		delete sim;
		
		if (lineNumber != -1)
			std::cerr << "[" << lineNumber << "] ";
		
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during RunOneGeneration(): " << EidosGetTrimmedRaiseMessage() << endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		return;
	}
	
	delete sim;
	
	gSLiMTestFailureCount--;	// correct for our assumption of failure above
	gSLiMTestSuccessCount++;
	
	//std::cerr << p_script_string << " : " << EIDOS_OUTPUT_SUCCESS_TAG << endl;
	
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

void SLiMAssertScriptRaise(const string &p_script_string, const int p_bad_line, const int p_bad_position, const std::string &p_reason_snip, int lineNumber)
{
	SLiMSim *sim = nullptr;
	
	try {
		std::istringstream infile(p_script_string);
		
		sim = new SLiMSim(infile);
		sim->InitializeRNGFromSeed(nullptr);
		
		while (sim->_RunOneGeneration());
		
		gSLiMTestFailureCount++;
		
		if (lineNumber != -1)
			std::cerr << "[" << lineNumber << "] ";
		
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : no raise during SLiM execution (expected \"" << p_reason_snip << "\")." << endl;
	}
	catch (std::runtime_error err)
	{
		// We need to call EidosGetTrimmedRaiseMessage() here to empty the error stringstream, even if we don't log the error
		std::string raise_message = EidosGetTrimmedRaiseMessage();
		
		if (raise_message.find("stop() called") == string::npos)
		{
			if (raise_message.find(p_reason_snip) != string::npos)
			{
				if ((gEidosCharacterStartOfError == -1) || (gEidosCharacterEndOfError == -1) || !gEidosCurrentScript)
				{
					if ((p_bad_line == -1) && (p_bad_position == -1))
					{
						gSLiMTestSuccessCount++;
						
						//std::cerr << p_script_string << " == (expected raise) : " << EIDOS_OUTPUT_SUCCESS_TAG << "\n   " << raise_message << endl;
					}
					else
					{
						gSLiMTestFailureCount++;
						
						if (lineNumber != -1)
							std::cerr << "[" << lineNumber << "] ";
						
						std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise expected, but no error info set" << endl;
						std::cerr << "   raise message: " << raise_message << endl;
						std::cerr << "--------------------" << std::endl << std::endl;
					}
				}
				else
				{
					eidos_script_error_position(gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript);
					
					if ((gEidosErrorLine != p_bad_line) || (gEidosErrorLineCharacter != p_bad_position))
					{
						gSLiMTestFailureCount++;
						
						if (lineNumber != -1)
							std::cerr << "[" << lineNumber << "] ";
						
						std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise expected, but error position unexpected" << endl;
						std::cerr << "   raise message: " << raise_message << endl;
						eidos_log_script_error(std::cerr, gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript, gEidosExecutingRuntimeScript);
						std::cerr << "--------------------" << std::endl << std::endl;
					}
					else
					{
						gSLiMTestSuccessCount++;
						
						//std::cerr << p_script_string << " == (expected raise) : " << EIDOS_OUTPUT_SUCCESS_TAG << "\n   " << raise_message << endl;
					}
				}
			}
			else
			{
				gSLiMTestFailureCount++;
				
				if (lineNumber != -1)
					std::cerr << "[" << lineNumber << "] ";
				
				std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise message mismatch (expected \"" << p_reason_snip << "\")." << endl;
				std::cerr << "   raise message: " << raise_message << endl;
				std::cerr << "--------------------" << std::endl << std::endl;
			}
		}
		else
		{
			gSLiMTestFailureCount++;
			
			if (lineNumber != -1)
				std::cerr << "[" << lineNumber << "] ";
			
			std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : stop() reached (expected \"" << p_reason_snip << "\")." << endl;
			std::cerr << "--------------------" << std::endl << std::endl;
		}
	}
	
	delete sim;
	
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

void SLiMAssertScriptStop(const string &p_script_string, int lineNumber)
{
	SLiMSim *sim = nullptr;
	
	try {
		std::istringstream infile(p_script_string);
		
		sim = new SLiMSim(infile);
		sim->InitializeRNGFromSeed(nullptr);
		
		while (sim->_RunOneGeneration());
		
		gSLiMTestFailureCount++;
		
		if (lineNumber != -1)
			std::cerr << "[" << lineNumber << "] ";
		
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : no raise during SLiM execution." << endl;
	}
	catch (std::runtime_error err)
	{
		// We need to call EidosGetTrimmedRaiseMessage() here to empty the error stringstream, even if we don't log the error
		std::string raise_message = EidosGetTrimmedRaiseMessage();
		
		if (raise_message.find("stop() called") == string::npos)
		{
			gSLiMTestFailureCount++;
			
			if (lineNumber != -1)
				std::cerr << "[" << lineNumber << "] ";
			
			std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : stop() not reached" << endl;
			std::cerr << "   raise message: " << raise_message << endl;
			
			if ((gEidosCharacterStartOfError != -1) && (gEidosCharacterEndOfError != -1) && gEidosCurrentScript)
			{
				eidos_script_error_position(gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript);
				eidos_log_script_error(std::cerr, gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript, gEidosExecutingRuntimeScript);
			}
			
			std::cerr << "--------------------" << std::endl << std::endl;
		}
		else
		{
			gSLiMTestSuccessCount++;
			
			//std::cerr << p_script_string << " == (expected raise) " << raise_message << " : " << EIDOS_OUTPUT_SUCCESS_TAG << endl;
		}
	}
	
	delete sim;
	
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}


// Test subfunction prototypes
static void _RunBasicTests(void);
static void _RunInitTests(void);
static void _RunSLiMSimTests(void);
static void _RunMutationTypeTests(void);
static void _RunGenomicElementTypeTests(void);
static void _RunGenomicElementTests(void);
static void _RunChromosomeTests(void);
static void _RunMutationTests(void);
static void _RunGenomeTests(void);
static void _RunSubpopulationTests(void);
static void _RunIndividualTests(void);
static void _RunSubstitutionTests(void);
static void _RunSLiMEidosBlockTests(void);


// Test function shared strings
static std::string gen1_setup("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } ");
static std::string gen1_setup_sex("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeSex('X'); } ");
static std::string gen2_stop(" 2 { stop(); } ");
static std::string gen1_setup_highmut_p1("initialize() { initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } ");
static std::string gen1_setup_p1(gen1_setup + "1 { sim.addSubpop('p1', 10); } ");
static std::string gen1_setup_sex_p1(gen1_setup_sex + "1 { sim.addSubpop('p1', 10); } ");
static std::string gen1_setup_p1p2p3(gen1_setup + "1 { sim.addSubpop('p1', 10); sim.addSubpop('p2', 10); sim.addSubpop('p3', 10); } ");


void RunSLiMTests(void)
{
	// Test SLiM.  The goal here is not really to test that the core code of SLiM is working properly – that simulations
	// work as they are intended to.  Such testing is beyond the scope of what we can do here.  Instead, the goal here
	// is to test all of the Eidos-related APIs in SLiM – to make sure that all properties, methods, and functions in
	// SLiM's Eidos interface work properly.  SLiM itself will get a little incidental testing along the way.
	
	// Reset error counts
	gSLiMTestSuccessCount = 0;
	gSLiMTestFailureCount = 0;
	
	// Run tests
	_RunBasicTests();
	_RunInitTests();
	_RunSLiMSimTests();
	_RunMutationTypeTests();
	_RunGenomicElementTypeTests();
	_RunGenomicElementTests();
	_RunChromosomeTests();
	_RunMutationTests();
	_RunGenomeTests();
	_RunSubpopulationTests();
	_RunIndividualTests();
	_RunSubstitutionTests();
	_RunSLiMEidosBlockTests();
	
	// ************************************************************************************
	//
	//	Print a summary of test results
	//
	std::cerr << endl;
	if (gSLiMTestFailureCount)
		std::cerr << "" << EIDOS_OUTPUT_FAILURE_TAG << " count: " << gSLiMTestFailureCount << endl;
	std::cerr << EIDOS_OUTPUT_SUCCESS_TAG << " count: " << gSLiMTestSuccessCount << endl;
	std::cerr.flush();
	
	// Clear out the SLiM output stream post-test
	gSLiMOut.clear();
	gSLiMOut.str("");
}

#pragma mark basic tests
void _RunBasicTests(void)
{
	// Note that the code here uses C++11 raw string literals, which Xcode's prettyprinting and autoindent code does not
	// presently understand.  The line/character positions for SLiMAssertScriptRaise() depend upon the indenting that
	// Xcode has chosen to give the Eidos scripts below.  Be careful, therefore, not to re-indent this code!
	
	// Test that a basic script works
	std::string basic_script(R"V0G0N(
							 
							 initialize() {
								 initializeMutationRate(1e-7);
								 initializeMutationType('m1', 0.5, 'f', 0.0);
								 initializeGenomicElementType('g1', m1, 1.0);
								 initializeGenomicElement(g1, 0, 99999);
								 initializeRecombinationRate(1e-8);
							 }
							 1 { sim.addSubpop('p1', 500); }
							 5 late() { sim.outputFull(); }
							 
							 )V0G0N");
	
	SLiMAssertScriptSuccess(basic_script);
	
	
	// Test that stop() raises as it is supposed to
	std::string stop_test(R"V0G0N(
						  
						  initialize() {
							  initializeMutationRate(1e-7);
							  initializeMutationType('m1', 0.5, 'f', 0.0);
							  initializeGenomicElementType('g1', m1, 1.0);
							  initializeGenomicElement(g1, 0, 99999);
							  initializeRecombinationRate(1e-8);
						  }
						  1 { sim.addSubpop('p1', 500); }
						  3 { stop('fail!'); }
						  5 late() { sim.outputFull(); }
						  
						  )V0G0N");
	
	SLiMAssertScriptStop(stop_test);
	
	// Test script registration
	SLiMAssertScriptStop("initialize() { stop(); } s1 {}", __LINE__);
	SLiMAssertScriptRaise("initialize() { stop(); } s1 {} s1 {}", 1, 31, "already defined", __LINE__);
	SLiMAssertScriptStop("initialize() { stop(); } 1: {}", __LINE__);
	SLiMAssertScriptStop("initialize() { stop(); } :1 {}", __LINE__);
	SLiMAssertScriptStop("initialize() { stop(); } 1:10 {}", __LINE__);
	SLiMAssertScriptRaise("initialize() { stop(); } : {}", 1, 27, "unexpected token", __LINE__);
}

#pragma mark initialize() tests
void _RunInitTests(void)
{	
	// ************************************************************************************
	//
	//	Initialization function tests
	//
	
	// Test (void)initializeGeneConversion(numeric$ conversionFraction, numeric$ meanLength)
	SLiMAssertScriptStop("initialize() { initializeGeneConversion(0.5, 10000000000000); stop(); }", __LINE__);										// legal; no max for meanLength
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(-0.001, 10000000000000); stop(); }", 1, 15, "must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(1.001, 10000000000000); stop(); }", 1, 15, "must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 0.0); stop(); }", 1, 15, "must be greater than 0.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5); stop(); }", 1, 15, "missing required argument", __LINE__);
	
	// Test (object<MutationType>$)initializeMutationType(is$ id, numeric$ dominanceCoeff, string$ distributionType, ...)
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeMutationType(1, 0.5, 'f', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType(-1, 0.5, 'f', 0.0); stop(); }", 1, 15, "identifier value is out of range", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('p2', 0.5, 'f', 0.0); stop(); }", 1, 15, "identifier prefix \"m\" was expected", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('mm1', 0.5, 'f', 0.0); stop(); }", 1, 15, "must be a simple integer", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f'); stop(); }", 1, 15, "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0, 0.0); stop(); }", 1, 15, "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0); stop(); }", 1, 15, "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', 0.0, 0.0); stop(); }", 1, 15, "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 0.0); stop(); }", 1, 15, "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0); stop(); }", 1, 15, "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 'foo'); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 'foo', 0.0); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0, 'foo'); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', 'foo'); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 'foo', 0.0); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 0.0, 'foo'); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 'foo', 0.0); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0, 'foo'); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', '1'); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', '1', 0.0); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0, '1'); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', '1'); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', '1', 0.0); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 0.0, '1'); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', '1', 0.0); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0, '1'); stop(); }", 1, 15, "requires that DFE parameters be numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'x', 0.0); stop(); }", 1, 15, "must be \"f\", \"g\", \"e\", \"n\", \"w\", or \"s\"", __LINE__);
	SLiMAssertScriptStop("initialize() { x = initializeMutationType('m7', 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { x = initializeMutationType(7, 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { m7 = 15; initializeMutationType(7, 0.5, 'f', 0.0); stop(); }", 1, 24, "already defined", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); initializeMutationType('m1', 0.5, 'f', 0.0); stop(); }", 1, 60, "already defined", __LINE__);
	
	// Test (object<GenomicElementType>$)initializeGenomicElementType(is$ id, io<MutationType> mutationTypes, numeric proportions)
	std::string define_m12(" initializeMutationType('m1', 0.5, 'f', 0.0); initializeMutationType('m2', 0.5, 'f', 0.5); ");
	
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', object(), integer(0)); stop(); }", __LINE__);			// legal: genomic element with no mutations
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', integer(0), float(0)); stop(); }", __LINE__);			// legal: genomic element with no mutations
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), c(0,0)); stop(); }", __LINE__);				// legal: genomic element with all zero proportions (must be fixed later...)
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1:2); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType(1, c(m1,m2), 1:2); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', 1:2, 1:2); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2)); stop(); }", 1, 105, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1); stop(); }", 1, 105, "requires the sizes", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), c(-1,2)); stop(); }", 1, 105, "must be greater than or equal to zero", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', 2:3, 1:2); stop(); }", 1, 105, "not defined", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(2,2), 1:2); stop(); }", 1, 105, "used more than once", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_m12 + "x = initializeGenomicElementType('g7', c(m1,m2), 1:2); if (x == g7) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_m12 + "x = initializeGenomicElementType(7, c(m1,m2), 1:2); if (x == g7) stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "g7 = 17; initializeGenomicElementType(7, c(m1,m2), 1:2); stop(); }", 1, 114, "already defined", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1:2); initializeGenomicElementType('g1', c(m1,m2), c(0,0)); stop(); }", 1, 156, "already defined", __LINE__);
	
	// Test (void)initializeGenomicElement(io<GenomicElementType>$ genomicElementType, integer$ start, integer$ end)
	std::string define_g1(define_m12 + " initializeGenomicElementType('g1', c(m1,m2), 1:2); ");
	
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000000); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeGenomicElement(1, 0, 1000000000); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0); stop(); }", 1, 157, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(2, 0, 1000000000); stop(); }", 1, 157, "not defined", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, -1, 1000000000); stop(); }", 1, 157, "out of range", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000001); stop(); }", 1, 157, "out of range", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 100, 99); stop(); }", 1, 157, "is less than start position", __LINE__);
	
	// Test (void)initializeMutationRate(numeric$ rate)
	SLiMAssertScriptStop("initialize() { initializeMutationRate(0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(); stop(); }", 1, 15, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(-0.0000001); stop(); }", 1, 15, "requires rate >= 0", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeMutationRate(10000000); stop(); }", __LINE__);														// legal; no maximum rate
	
	// Test (void)initializeRecombinationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(); stop(); }", 1, 15, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(-0.00001); stop(); }", 1, 15, "requires rates to be >= 0", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(10000); stop(); }", __LINE__);													// legal; no maximum rate
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000)); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 15, "requires rates to be a singleton if", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(integer(0), integer(0)); stop(); }", 1, 15, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), 1000); stop(); }", 1, 15, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), 1:3); stop(); }", 1, 15, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000)); stop(); }", 1, 15, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000)); stop(); }", 1, 15, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 15, "requires rates to be >= 0", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(); stop(); }", 1, 35, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(-0.00001); stop(); }", 1, 35, "requires rates to be >= 0", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(10000); stop(); }", __LINE__);													// legal; no maximum rate
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000)); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 35, "requires rates to be a singleton if", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(integer(0), integer(0)); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1000); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1:3); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000)); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000)); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 35, "requires rates to be >= 0", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(integer(0), integer(0), '*'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1000, '*'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1:3, '*'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000), '*'); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000), '*'); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 35, "requires rates to be >= 0", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(integer(0), integer(0), 'M'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1000, 'M'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1:3, 'M'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000), 'M'); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000), 'M'); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", 1, 35, "requires rates to be >= 0", __LINE__);
	
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, 'F'); stop(); } 1 {}", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 3000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, 'F'); } 1 {}", -1, -1, "do not cover all genomic elements", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 1000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, 'F'); } 1 { stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 1999, 'F'); } 1 {}", -1, -1, "do not cover all genomic elements", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2001, 'F'); } 1 { stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, '*'); } 1 {}", 1, 307, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), '*'); initializeRecombinationRate(0.0, 2000, 'F'); } 1 {}", 1, 307, "single map versus separate maps", __LINE__);
	
	// Test (void)initializeSex(string$ chromosomeType, [numeric$ xDominanceCoeff])
	SLiMAssertScriptStop("initialize() { initializeSex('A'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('X'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('Y'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('Z'); stop(); }", 1, 15, "requires a chromosomeType of", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex(); stop(); }", 1, 15, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A', 0.0); stop(); }", 1, 15, "may be supplied only for", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('X', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('Y', 0.0); stop(); }", 1, 15, "may be supplied only for", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('Z', 0.0); stop(); }", 1, 15, "requires a chromosomeType of", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('X', -10000); stop(); }", __LINE__);															// legal: no minimum value for dominance coeff
	SLiMAssertScriptStop("initialize() { initializeSex('X', 10000); stop(); }", __LINE__);															// legal: no maximum value for dominance coeff
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeSex('A'); stop(); }", 1, 35, "may be called only once", __LINE__);
	
	// Test (void)initializeSLiMOptions([logical$ keepPedigrees = F])
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(F); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(T); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(keepPedigrees=NULL); stop(); }", 1, 15, "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(); initializeSLiMOptions(); stop(); }", 1, 40, "may be called only once", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(0.0); initializeSLiMOptions(); stop(); }", 1, 44, "must be called before all other initialization functions", __LINE__);
}

#pragma mark SLiMSim tests
void _RunSLiMSimTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: SLiMSim
	//
	
	// Test sim properties
	SLiMAssertScriptStop(gen1_setup + "1 { } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome; } " + gen2_stop, __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.chromosome = sim.chromosome; } " + gen2_stop, 1, 231, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.chromosomeType == 'A') stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.chromosomeType = 'A'; } " + gen2_stop, 1, 235, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { if (sim.chromosomeType == 'X') stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { sim.chromosomeType = 'X'; } " + gen2_stop, 1, 255, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.dominanceCoeffX; } " + gen2_stop);															// legal: the property is meaningless but may be accessed
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.dominanceCoeffX = 0.2; } ", 1, 236, "when not simulating an X chromosome", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.dominanceCoeffX; } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.dominanceCoeffX = 0.2; } " + gen2_stop, __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.generation; } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.generation = 7; } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.genomicElementTypes == g1) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.genomicElementTypes = g1; } ", 1, 240, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.mutationTypes == m1) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.mutationTypes = m1; } ", 1, 234, "read-only property", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.mutations; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.mutations = _Test(7); } ", 1, 230, "cannot be object element type", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.scriptBlocks; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.scriptBlocks = sim.scriptBlocks[0]; } ", 1, 233, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.sexEnabled == F) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { if (sim.sexEnabled == T) stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (size(sim.subpopulations) == 0) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.subpopulations = _Test(7); } ", 1, 235, "cannot be object element type", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (size(sim.substitutions) == 0) stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.substitutions = _Test(7); } ", 1, 234, "cannot be object element type", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.tag; } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.tag = -17; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.tag = -17; } 2 { if (sim.tag == -17) stop(); }", __LINE__);
	
	// Test sim - (object<Subpopulation>)addSubpop(is$ subpopID, integer$ size, [float$ sexRatio])
	SLiMAssertScriptStop(gen1_setup + "1 { sim.addSubpop('p1', 10); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.addSubpop(1, 10); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.addSubpop('p1', 10, 0.5); } " + gen2_stop, __LINE__);	// default value
	SLiMAssertScriptStop(gen1_setup + "1 { sim.addSubpop(1, 10, 0.5); } " + gen2_stop, __LINE__);	// default value
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.addSubpop('p1', 10, 0.4); } " + gen2_stop, 1, 220, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.addSubpop(1, 10, 0.4); } " + gen2_stop, 1, 220, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.addSubpop('p1', 10, 0.5); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.addSubpop(1, 10, 0.5); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { x = sim.addSubpop('p7', 10); if (x == p7) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { x = sim.addSubpop(7, 10); if (x == p7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { p7 = 17; sim.addSubpop('p7', 10); stop(); }", 1, 229, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.addSubpop('p7', 10); sim.addSubpop(7, 10); stop(); }", 1, 245, "already exists", __LINE__);
	
	// Test sim - (object<Subpopulation>)addSubpopSplit(is$ subpopID, integer$ size, io<Subpopulation>$ sourceSubpop, [float$ sexRatio])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit('p2', 10, p1); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit('p2', 10, 1); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, p1); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, 1); } " + gen2_stop, __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, 7); } " + gen2_stop, 1, 251, "not defined", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit('p2', 10, p1, 0.5); } " + gen2_stop, __LINE__);	// default value
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, p1, 0.5); } " + gen2_stop, __LINE__);	// default value
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit('p2', 10, p1, 0.4); } " + gen2_stop, 1, 251, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, p1, 0.4); } " + gen2_stop, 1, 251, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { sim.addSubpopSplit('p2', 10, p1, 0.5); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { sim.addSubpopSplit(2, 10, p1, 0.5); } " + gen2_stop, __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { x = sim.addSubpopSplit('p7', 10, p1); if (x == p7) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { x = sim.addSubpopSplit(7, 10, p1); if (x == p7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p7 = 17; sim.addSubpopSplit('p7', 10, p1); stop(); }", 1, 260, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit('p7', 10, p1); sim.addSubpopSplit(7, 10, p1); stop(); }", 1, 285, "already exists", __LINE__);
	
	// Test sim - (void)deregisterScriptBlock(io<SLiMEidosBlock> scriptBlocks)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(s1); } s1 2 { stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(1); } s1 2 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(object()); } s1 2 { stop(); }", __LINE__);									// legal: deregister nothing
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(s1, s1)); } s1 2 { stop(); }", 1, 251, "same script block", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(1, 1)); } s1 2 { stop(); }", 1, 251, "same script block", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(s1); sim.deregisterScriptBlock(s1); } s1 2 { stop(); }", 1, 282, "same script block", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(1); sim.deregisterScriptBlock(1); } s1 2 { stop(); }", 1, 281, "same script block", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(s1, s2)); } s1 2 { stop(); } s2 3 { stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(1, 2)); } s1 2 { stop(); } s2 3 { stop(); }", __LINE__);
	
	// Test sim - (float)mutationFrequencies(No<Subpopulation> subpops, [object<Mutation> mutations])
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(p1); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(c(p1, p2)); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(NULL); }", __LINE__);													// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(sim.subpopulations); }", __LINE__);										// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(object()); }", __LINE__);												// legal to specify an empty object vector
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(1); }", 1, 301, "cannot be type integer", __LINE__);						// this is one API where integer identifiers can't be used
	
	// Test sim - (integer)mutationCounts(No<Subpopulation> subpops, [object<Mutation> mutations])
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationCounts(p1); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationCounts(c(p1, p2)); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationCounts(NULL); }", __LINE__);													// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationCounts(sim.subpopulations); }", __LINE__);										// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationCounts(object()); }", __LINE__);												// legal to specify an empty object vector
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { sim.mutationCounts(1); }", 1, 301, "cannot be type integer", __LINE__);						// this is one API where integer identifiers can't be used
	
	// Test sim - (object<Mutation>)mutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { sim.mutationsOfType(m1); } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { sim.mutationsOfType(1); } ", __LINE__);
	
	// Test sim - (object<Mutation>)countOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { sim.countOfMutationsOfType(m1); } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { sim.countOfMutationsOfType(1); } ", __LINE__);
	
	// Test sim - (void)outputFixedMutations(void)
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFixedMutations(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFixedMutations(NULL); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFixedMutations('/tmp/slimOutputFixedTest.txt'); }", __LINE__);
	
	// Test sim - (void)outputFull([string$ filePath])
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(NULL); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(NULL, T); }", 1, 308, "cannot output in binary format", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull('/tmp/slimOutputFullTest.txt'); }", __LINE__);									// legal, output to file path; this test might work only on Un*x systems
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull('/tmp/slimOutputFullTest.slimbinary', T); }", __LINE__);						// legal, output to file path; this test might work only on Un*x systems
	
	// Test sim - (void)outputMutations(object<Mutation> mutations)
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(sim.mutations); }", __LINE__);											// legal; should have some mutations by gen 5
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(sim.mutations[0]); }", __LINE__);										// legal; output just one mutation
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(sim.mutations[integer(0)]); }", __LINE__);								// legal to specify an empty object vector
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(object()); }", __LINE__);												// legal to specify an empty object vector
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(NULL); }", 1, 258, "cannot be type NULL", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(sim.mutations, NULL); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 late() { sim.outputMutations(sim.mutations, '/tmp/slimOutputMutationsTest.txt'); }", __LINE__);
	
	// Test - (void)readFromPopulationFile(string$ filePath)
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.readFromPopulationFile('/tmp/slimOutputFullTest.txt'); }", __LINE__);												// legal, read from file path; depends on the outputFull() test above
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.readFromPopulationFile('/tmp/slimOutputFullTest.slimbinary'); }", __LINE__);										// legal, read from file path; depends on the outputFull() test above
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.readFromPopulationFile('/tmp/notAFile.foo'); }", 1, 220, "does not exist or is empty", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.readFromPopulationFile('/tmp/slimOutputFullTest.txt'); if (size(sim.subpopulations) != 3) stop(); }", __LINE__);			// legal; should wipe previous state
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.readFromPopulationFile('/tmp/slimOutputFullTest.slimbinary'); if (size(sim.subpopulations) != 3) stop(); }", __LINE__);	// legal; should wipe previous state
	
	// Test sim - (object<SLiMEidosBlock>)registerEarlyEvent(Nis$ id, string$ source, [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerEarlyEvent(NULL, '{ stop(); }', 2, 2); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEarlyEvent('s1', '{ stop(); }', 2, 2); } s1 { }", 1, 251, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerEarlyEvent('s1', '{ stop(); }', 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerEarlyEvent(1, '{ stop(); }', 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEarlyEvent(1, '{ stop(); }', 2, 2); sim.registerEarlyEvent(1, '{ stop(); }', 2, 2); }", 1, 299, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEarlyEvent(1, '{ stop(); }', 3, 2); }", 1, 251, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEarlyEvent(1, '{ stop(); }', -1, -1); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEarlyEvent(1, '{ stop(); }', 0, 0); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEarlyEvent(1, '{ $; }', 2, 2); }", 1, 2, "unrecognized token", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerLateEvent(Nis$ id, string$ source, [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerLateEvent(NULL, '{ stop(); }', 2, 2); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent('s1', '{ stop(); }', 2, 2); } s1 { }", 1, 251, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerLateEvent('s1', '{ stop(); }', 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerLateEvent(1, '{ stop(); }', 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ stop(); }', 2, 2); sim.registerLateEvent(1, '{ stop(); }', 2, 2); }", 1, 298, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ stop(); }', 3, 2); }", 1, 251, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ stop(); }', -1, -1); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ stop(); }', 0, 0); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ $; }', 2, 2); }", 1, 2, "unrecognized token", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerFitnessCallback(Nis$ id, string$ source, io<MutationType>$ mutType, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', 1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', m1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', 1, 1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', m1, p1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', 1); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', m1); } 10 { ; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }'); }", 1, 251, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback('s1', '{ stop(); }', m1, NULL, 2, 2); } s1 { }", 1, 251, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { s1 = 7; sim.registerFitnessCallback('s1', '{ stop(); }', m1, NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { s1 = 7; sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); }", 1, 314, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 3, 2); }", 1, 251, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, -1, -1); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 0, 0); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ $; }', m1, NULL, 2, 2); }", 1, 2, "unrecognized token", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerMateChoiceCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }', NULL, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }', NULL, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }', 1, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }', p1, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }'); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }'); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL); }", 1, 251, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback('s1', '{ stop(); }', NULL, 2, 2); } s1 { }", 1, 251, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerMateChoiceCallback('s1', '{ stop(); }', NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 313, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 3, 2); }", 1, 251, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, -1, -1); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 0, 0); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ $; }', NULL, 2, 2); }", 1, 2, "unrecognized token", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerModifyChildCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }', NULL, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }', NULL, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }', 1, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }', p1, 2, 2); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }'); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }'); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL); }", 1, 251, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback('s1', '{ stop(); }', NULL, 2, 2); } s1 { }", 1, 251, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerModifyChildCallback('s1', '{ stop(); }', NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 314, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 3, 2); }", 1, 251, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, -1, -1); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 0, 0); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ $; }', NULL, 2, 2); }", 1, 2, "unrecognized token", __LINE__);
	
	// Test sim - (void)simulationFinished(void)
	SLiMAssertScriptStop(gen1_setup_p1 + "11 { stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { sim.simulationFinished(); } 11 { stop(); }", __LINE__);
}

#pragma mark MutationType tests
void _RunMutationTypeTests(void)
{	
	// ************************************************************************************
	//
	//	Gen 1+ tests: MutationType
	//
	
	// Test MutationType properties
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.convertToSubstitution == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.mutationStackPolicy == 's') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.distributionParams == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.distributionType == 'f') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.dominanceCoeff == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.tag = 17; } 2 { if (m1.tag == 17) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.convertToSubstitution = F; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.mutationStackPolicy = 's'; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.mutationStackPolicy = 'f'; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.mutationStackPolicy = 'l'; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.mutationStackPolicy = 'z'; }", 1, 239, "property mutationStackPolicy must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.distributionParams = 0.1; }", 1, 238, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.distributionType = 'g'; }", 1, 236, "read-only property", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.dominanceCoeff = 0.3; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.id = 2; }", 1, 222, "read-only property", __LINE__);
	
	// Test MutationType - (void)setDistribution(string$ distributionType, ...)
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('f', 2.2); if (m1.distributionType == 'f' & m1.distributionParams == 2.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('g', 3.1, 7.5); if (m1.distributionType == 'g' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('e', -3); if (m1.distributionType == 'e' & m1.distributionParams == -3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('n', 3.1, 7.5); if (m1.distributionType == 'n' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('w', 3.1, 7.5); if (m1.distributionType == 'w' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('s', 'return 1;'); if (m1.distributionType == 's' & identical(m1.distributionParams, 'return 1;')) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('x', 1.5); stop(); }", 1, 219, "must be \"f\", \"g\", \"e\", \"n\", \"w\", or \"s\"", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', 'foo'); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 'foo', 7.5); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, 'foo'); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', 'foo'); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 'foo', 7.5); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 3.1, 'foo'); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 'foo', 7.5); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 3.1, 'foo'); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('s', 3); stop(); }", 1, 219, "requires that the parameters for this DFE be of type string", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', '1'); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', '1', 7.5); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, '1'); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', '1'); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', '1', 7.5); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 3.1, '1'); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', '1', 7.5); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 3.1, '1'); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('s', 3.1); stop(); }", 1, 219, "requires that the parameters for this DFE be of type string", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', T); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', T, 7.5); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, T); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', T); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', T, 7.5); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 3.1, T); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', T, 7.5); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 3.1, T); stop(); }", 1, 219, "requires that the parameters for this DFE be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('s', T); stop(); }", 1, 219, "requires that the parameters for this DFE be of type string", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { m1.setDistribution('s', 'return foo;'); } 100 { stop(); }", -1, -1, "undefined identifier foo", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { m1.setDistribution('s', 'x >< 5;'); } 100 { stop(); }", -1, -1, "tokenize/parse error in type 's' DFE callback script", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { m1.setDistribution('s', 'x $ 5;'); } 100 { stop(); }", -1, -1, "tokenize/parse error in type 's' DFE callback script", __LINE__);
}

#pragma mark GenomicElementType tests
void _RunGenomicElementTypeTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: GenomicElementType
	//
	
	// Test GenomicElementType properties
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.id = 2; }", 1, 222, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.mutationFractions == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.mutationTypes == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.tag = 17; } 2 { if (m1.tag == 17) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.mutationFractions = 1.0; }", 1, 237, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.mutationTypes = m1; }", 1, 233, "read-only property", __LINE__);
	
	// Test GenomicElementType - (void)setMutationFractions(io<MutationType> mutationTypes, numeric proportions)
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(object(), integer(0)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(m1, 0.0); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(1, 0.0); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(m1, 0.3); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(1, 0.3); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2), c(0.3, 0.7)); if (identical(g1.mutationTypes, c(m1,m2)) & identical(g1.mutationFractions, c(0.3,0.7))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(1,2), c(0.3, 0.7)); if (identical(g1.mutationTypes, c(m1,m2)) & identical(g1.mutationFractions, c(0.3,0.7))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2)); stop(); }", 1, 281, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2), 0.3); stop(); }", 1, 281, "requires the sizes", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2), c(-1, 2)); stop(); }", 1, 281, "must be greater than or equal to zero", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(2,3), c(1, 2)); stop(); }", 1, 281, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m2,m2), c(1, 2)); stop(); }", 1, 281, "used more than once", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(2,2), c(1, 2)); stop(); }", 1, 281, "used more than once", __LINE__);
}

#pragma mark GenomicElement tests
void _RunGenomicElementTests(void)
{	
	// ************************************************************************************
	//
	//	Gen 1+ tests: GenomicElement
	//
	
	std::string gen1_setup_2ge("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 999); initializeGenomicElement(g1, 1000, 99999); initializeRecombinationRate(1e-8); } ");
	
	// Test GenomicElement properties
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; if (ge.endPosition == 999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; if (ge.startPosition == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; if (ge.genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.tag = -12; if (ge.tag == -12) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.endPosition = 999; stop(); }", 1, 312, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.startPosition = 0; stop(); }", 1, 314, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.genomicElementType = g1; stop(); }", 1, 319, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.endPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.startPosition == 1000) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.tag = -17; if (ge.tag == -17) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.endPosition = 99999; stop(); }", 1, 312, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.startPosition = 1000; stop(); }", 1, 314, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.genomicElementType = g1; stop(); }", 1, 319, "read-only property", __LINE__);
	
	// Test GenomicElement - (void)setGenomicElementType(io<GenomicElementType>$ genomicElementType)
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(g1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(1); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(); stop(); }", 1, 300, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(object()); stop(); }", 1, 300, "must be a singleton", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(2); stop(); }", 1, 300, "not defined", __LINE__);
	
	// Test GenomicElement position testing
	SLiMAssertScriptStop(gen1_setup_2ge + "initialize() { initializeGenomicElement(g1, 100000, 100000); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "initialize() { initializeGenomicElement(g1, 99999, 100000); stop(); }", 1, 268, "overlaps existing genomic element", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "initialize() { initializeGenomicElement(g1, -2, -1); stop(); }", 1, 268, "chromosome position or length is out of range", __LINE__);
}

#pragma mark Chromosome tests
void _RunChromosomeTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Chromosome
	//
	
	// Test Chromosome properties
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionFraction == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionMeanLength == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.genomicElements[0].genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.lastPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.mutationRate == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.overallRecombinationRate == 1e-8 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRateM)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRateF)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.recombinationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositionsM)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositionsF)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.recombinationRates == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRatesM)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRatesF)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.tag = 3294; if (ch.tag == 3294) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionFraction = 0.1; if (ch.geneConversionFraction == 0.1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionFraction = -0.001; stop(); }", 1, 263, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionFraction = 1.001; stop(); }", 1, 263, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 0.2; if (ch.geneConversionMeanLength == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 0.0; stop(); }", 1, 265, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 1e10; stop(); }", __LINE__);												// legal; no upper bound
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.genomicElements = ch.genomicElements; stop(); }", 1, 256, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.lastPosition = 99999; stop(); }", 1, 253, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.mutationRate = 1e-6; if (ch.mutationRate == 1e-6) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.mutationRate = -1e-6; stop(); }", 1, 253, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.mutationRate = 1e6; stop(); }", __LINE__);														// legal; no upper bound
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallRecombinationRate = 1e-2; stop(); }", 1, 265, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallRecombinationRateM = 1e-2; stop(); }", 1, 266, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallRecombinationRateF = 1e-2; stop(); }", 1, 266, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationEndPositions = 99999; stop(); }", 1, 266, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationEndPositionsM = 99999; stop(); }", 1, 267, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationEndPositionsF = 99999; stop(); }", 1, 267, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationRates = 1e-8; stop(); }", 1, 259, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationRatesM = 1e-8; stop(); }", 1, 260, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationRatesF = 1e-8; stop(); }", 1, 260, "read-only property", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.geneConversionFraction == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.geneConversionMeanLength == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.genomicElements[0].genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.lastPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.mutationRate == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.overallRecombinationRate == 1e-8 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRateM)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRateF)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.recombinationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositionsM)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositionsF)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.recombinationRates == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRatesM)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRatesF)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.tag = 3294; if (ch.tag == 3294) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.geneConversionFraction = 0.1; if (ch.geneConversionFraction == 0.1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.geneConversionFraction = -0.001; stop(); }", 1, 283, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.geneConversionFraction = 1.001; stop(); }", 1, 283, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 0.2; if (ch.geneConversionMeanLength == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 0.0; stop(); }", 1, 285, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 1e10; stop(); }", __LINE__);												// legal; no upper bound
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.genomicElements = ch.genomicElements; stop(); }", 1, 276, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.lastPosition = 99999; stop(); }", 1, 273, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationRate = 1e-6; if (ch.mutationRate == 1e-6) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationRate = -1e-6; stop(); }", 1, 273, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationRate = 1e6; stop(); }", __LINE__);														// legal; no upper bound
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallRecombinationRate = 1e-2; stop(); }", 1, 285, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallRecombinationRateM = 1e-2; stop(); }", 1, 286, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallRecombinationRateF = 1e-2; stop(); }", 1, 286, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationEndPositions = 99999; stop(); }", 1, 286, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationEndPositionsM = 99999; stop(); }", 1, 287, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationEndPositionsF = 99999; stop(); }", 1, 287, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationRates = 1e-8; stop(); }", 1, 279, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationRatesM = 1e-8; stop(); }", 1, 280, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationRatesF = 1e-8; stop(); }", 1, 280, "read-only property", __LINE__);
	
	std::string gen1_setup_sex_2rates("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeSex('X'); initializeRecombinationRate(1e-8, 99999, 'M'); initializeRecombinationRate(1e-7, 99999, 'F'); } ");
	
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.geneConversionFraction == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.geneConversionMeanLength == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.genomicElements[0].genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.lastPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.mutationRate == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRate)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.overallRecombinationRateM == 1e-8 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.overallRecombinationRateF == 1e-7 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositions)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.recombinationEndPositionsM == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.recombinationEndPositionsF == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRates)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.recombinationRatesM == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.recombinationRatesF == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.tag = 3294; if (ch.tag == 3294) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.geneConversionFraction = 0.1; if (ch.geneConversionFraction == 0.1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.geneConversionFraction = -0.001; stop(); }", 1, 342, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.geneConversionFraction = 1.001; stop(); }", 1, 342, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 0.2; if (ch.geneConversionMeanLength == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 0.0; stop(); }", 1, 344, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 1e10; stop(); }", __LINE__);												// legal; no upper bound
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.genomicElements = ch.genomicElements; stop(); }", 1, 335, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.lastPosition = 99999; stop(); }", 1, 332, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationRate = 1e-6; if (ch.mutationRate == 1e-6) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationRate = -1e-6; stop(); }", 1, 332, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationRate = 1e6; stop(); }", __LINE__);														// legal; no upper bound
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallRecombinationRate = 1e-2; stop(); }", 1, 344, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallRecombinationRateM = 1e-2; stop(); }", 1, 345, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallRecombinationRateF = 1e-2; stop(); }", 1, 345, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationEndPositions = 99999; stop(); }", 1, 345, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationEndPositionsM = 99999; stop(); }", 1, 346, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationEndPositionsF = 99999; stop(); }", 1, 346, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationRates = 1e-8; stop(); }", 1, 338, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationRatesM = 1e-8; stop(); }", 1, 339, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationRatesF = 1e-8; stop(); }", 1, 339, "read-only property", __LINE__);
	
	// Test Chromosome - (void)setRecombinationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(); stop(); }", 1, 240, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(-0.00001); stop(); }", 1, 240, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(10000); stop(); }", __LINE__);													// legal; no maximum rate
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 240, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 240, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 240, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 240, "must be >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 240, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 240, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 240, "must be >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(); stop(); }", 1, 260, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(-0.00001); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(10000); stop(); }", __LINE__);													// legal; no maximum rate
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 260, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 260, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 260, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 260, "must be >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 260, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 260, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 260, "must be >= 0", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.0); stop(); }", 1, 319, "single map versus separate maps", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(); stop(); }", 1, 319, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(-0.00001); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(10000); stop(); }", 1, 319, "single map versus separate maps", __LINE__);													// legal; no maximum rate
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), '*'); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 319, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), 'M'); stop(); }", 1, 319, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", 1, 319, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", 1, 319, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", 1, 319, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", 1, 319, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", 1, 319, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", 1, 319, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", 1, 319, "must be >= 0", __LINE__);
}

#pragma mark Mutation tests
void _RunMutationTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Mutation
	//
	
	// Test Mutation properties
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if (mut.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if ((mut.originGeneration >= 1) & (mut.originGeneration < 10)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if ((mut.position >= 0) & (mut.position < 100000)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if (mut.selectionCoeff == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if (mut.subpopID == 1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.mutationType = m1; stop(); }", 1, 289, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.originGeneration = 1; stop(); }", 1, 293, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.position = 0; stop(); }", 1, 285, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.selectionCoeff = 0.1; stop(); }", 1, 291, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.subpopID = 237; if (mut.subpopID == 237) stop(); }", __LINE__);						// legal; this field may be used as a user tag
	
	// Test Mutation - (void)setMutationType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setMutationType(m1); if (mut.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setMutationType(m1); if (mut.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setMutationType(2); if (mut.mutationType == m1) stop(); }", 1, 276, "mutation type m2 not defined", __LINE__);
	
	// Test Mutation - (void)setSelectionCoeff(float$ selectionCoeff)
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(0.5); if (mut.selectionCoeff == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(1); if (mut.selectionCoeff == 1) stop(); }", 1, 276, "cannot be type integer", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(-500.0); if (mut.selectionCoeff == -500.0) stop(); }", __LINE__);	// legal; no lower bound
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(500.0); if (mut.selectionCoeff == 500.0) stop(); }", __LINE__);		// legal; no upper bound
}

#pragma mark Genome tests
void _RunGenomeTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Genome
	//
	
	// Test Genome properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; if (gen.genomeType == 'A') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; if (gen.isNullGenome == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; if (gen.mutations[0].mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.tag = 278; if (gen.tag == 278) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.genomeType = 'A'; stop(); }", 1, 283, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.isNullGenome = F; stop(); }", 1, 285, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.mutations[0].mutationType = m1; stop(); }", 1, 299, "read-only property", __LINE__);
	
	// Test Genome + (void)addMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.addMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.addMutations(gen.mutations[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.addMutations(p1.genomes[1].mutations[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = p1.genomes[1].mutations[0]; gen.addMutations(rep(mut, 10)); if (sum(gen.mutations == mut) == 1) stop(); }", __LINE__);
	
	// Test Genome + (object<Mutation>)addNewDrawnMutation(io<MutationType>$ mutationType, integer$ position, [Ni$ originGeneration], [io<Subpopulation>$ originSubpop])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000, 10, p1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000, 10, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000, 10); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, 10, p1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, 10, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, 10); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(7, 5000, NULL, 1); stop(); }", 1, 278, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, 0, 1); stop(); }", 1, 278, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, -1, NULL, 1); stop(); }", 1, 278, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 100000, NULL, 1); stop(); }", 1, 278, "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL, 237); stop(); }", __LINE__);											// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL, -1); stop(); }", 1, 278, "out of range", __LINE__);					// however, such tags must be within range
	
	// Test Genome + (object<Mutation>)addNewMutation(io<MutationType>$ mutationType, numeric$ selectionCoeff, integer$ position, [Ni$ originGeneration], [io<Subpopulation>$ originSubpop])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000, 10, p1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000, 10, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000, 10); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, 10, p1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, 10, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, 10); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, NULL, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, NULL); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(7, 0.1, 5000, NULL, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, 0, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, -1, NULL, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 100000, NULL, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, NULL, 237); p1.genomes.addMutations(mut); stop(); }", __LINE__);							// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, NULL, -1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "out of range", __LINE__);	// however, such tags must be within range
	
	// Test Genome + (object<Mutation>)addNewDrawnMutation(io<MutationType>$ mutationType, integer$ position, [Ni$ originGeneration], [io<Subpopulation>$ originSubpop]) with new class method non-multiplex behavior
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000, 10, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000, 10, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000, 10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, 10, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, 10, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, 10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, NULL, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, NULL); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(7, 5000, NULL, 1); stop(); }", 1, 258, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, 0, 1); stop(); }", 1, 258, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, -1, NULL, 1); stop(); }", 1, 258, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 100000, NULL, 1); stop(); }", 1, 258, "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, NULL, 237); stop(); }", __LINE__);											// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, NULL, -1); stop(); }", 1, 258, "out of range", __LINE__);					// however, such tags must be within range
	
	// Test Genome + (object<Mutation>)addNewMutation(io<MutationType>$ mutationType, numeric$ selectionCoeff, integer$ position, [Ni$ originGeneration], [io<Subpopulation>$ originSubpop]) with new class method non-multiplex behavior
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000, 10, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000, 10, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000, 10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, 10, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, 10, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, 10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, NULL, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, NULL); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(7, 0.1, 5000, NULL, 1); stop(); }", 1, 258, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, 0, 1); stop(); }", 1, 258, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, -1, NULL, 1); stop(); }", 1, 258, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 100000, NULL, 1); stop(); }", 1, 258, "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, NULL, 237); stop(); }", __LINE__);							// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, NULL, -1); stop(); }", 1, 258, "out of range", __LINE__);	// however, such tags must be within range
	
	// Test Genome - (logical$)containsMarkerMutation(io<MutationType>$ mutType, integer$ position)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, 1000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(1, 1000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0:1].containsMarkerMutation(1, 1000); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, -1); stop(); }", 1, 262, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, 1000000); stop(); }", 1, 262, "past the end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(10, 1000); stop(); }", 1, 262, "mutation type m10 not defined", __LINE__);
	
	// Test Genome - (logical)containsMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMutations(sim.mutations); stop(); }", __LINE__);
	
	// Test Genome - (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].countOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].countOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0:1].countOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Genome - (object<Mutation>)mutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { p1.genomes[0].mutationsOfType(m1); } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { p1.genomes[0].mutationsOfType(1); } ", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { p1.genomes[0:1].mutationsOfType(1); } ", __LINE__);
	
	// Test Genome + (void)removeMutations(object<Mutation> mutations, [logical$ substitute])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(mut); gen.removeMutations(mut); stop(); }", __LINE__);	// legal to remove a mutation that is not present
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.removeMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.removeMutations(gen.mutations); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(mut, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(mut, T); gen.removeMutations(mut, T); stop(); }", __LINE__);	// legal to remove a mutation that is not present
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.removeMutations(object(), T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.removeMutations(gen.mutations, T); stop(); }", __LINE__);
	
	// Test Genome + (void)outputMS([Ns$ filePath])
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 0, T).outputMS(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 100, T).outputMS(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 0, T).outputMS(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 100, T).outputMS(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 0, T).outputMS('/tmp/slimOutputMSTest1.txt'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 100, T).outputMS('/tmp/slimOutputMSTest2.txt'); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 0, T).outputMS(NULL); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 100, T).outputMS(NULL); stop(); }", 1, 302, "cannot output null genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes[!p1.genomes.isNullGenome], 100, T).outputMS(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 0, T).outputMS('/tmp/slimOutputMSTest3.txt'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 100, T).outputMS('/tmp/slimOutputMSTest4.txt'); stop(); }", 1, 302, "cannot output null genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes[!p1.genomes.isNullGenome], 100, T).outputMS('/tmp/slimOutputMSTest5.txt'); stop(); }", __LINE__);
	
	// Test Genome + (void)output([Ns$ filePath])
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 0, T).output(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 100, T).output(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 0, T).output(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 100, T).output(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 0, T).output('/tmp/slimOutputTest1.txt'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.genomes, 100, T).output('/tmp/slimOutputTest2.txt'); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 0, T).output(NULL); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 100, T).output(NULL); stop(); }", 1, 302, "cannot output null genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes[!p1.genomes.isNullGenome], 100, T).output(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 0, T).output('/tmp/slimOutputTest3.txt'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes, 100, T).output('/tmp/slimOutputTest4.txt'); stop(); }", 1, 302, "cannot output null genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.genomes[!p1.genomes.isNullGenome], 100, T).output('/tmp/slimOutputTest5.txt'); stop(); }", __LINE__);
	
	// Test Genome + (void)outputVCF([Ns$ filePath], [logical$ outputMultiallelics])
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF('/tmp/slimOutputVCFTest1.txt'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF('/tmp/slimOutputVCFTest2.txt'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF(NULL, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF(NULL, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF('/tmp/slimOutputVCFTest3.txt', F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF('/tmp/slimOutputVCFTest4.txt', F); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF('/tmp/slimOutputVCFTest5.txt'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF('/tmp/slimOutputVCFTest6.txt'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF(NULL, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF(NULL, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 0, T).genomes.outputVCF('/tmp/slimOutputVCFTest7.txt', F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 late() { sample(p1.individuals, 100, T).genomes.outputVCF('/tmp/slimOutputVCFTest8.txt', F); stop(); }", __LINE__);
}

#pragma mark Subpopulation tests
void _RunSubpopulationTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Subpopulation
	//
	
	// Test Subpopulation properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.cloningRate == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.firstMaleIndex == p1.firstMaleIndex) stop(); }", __LINE__);					// legal but undefined value in non-sexual sims
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.individuals) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.immigrantSubpopFractions, float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.immigrantSubpopIDs, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.selfingRate == 0.0) stop(); }", __LINE__);									// legal but always 0.0 in non-sexual sims
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.sexRatio == 0.0) stop(); }", __LINE__);										// legal but always 0.0 in non-sexual sims
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.individualCount == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.tag = 135; if (p1.tag == 135) stop(); }", __LINE__);

	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.cloningRate = 0.0; stop(); }", 1, 262, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.firstMaleIndex = p1.firstMaleIndex; stop(); }", 1, 265, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes = p1.genomes[0]; stop(); }", 1, 258, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.individuals = p1.individuals[0]; stop(); }", 1, 262, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.id = 1; stop(); }", 1, 253, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.immigrantSubpopFractions = 1.0; stop(); }", 1, 275, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.immigrantSubpopIDs = 1; stop(); }", 1, 269, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.selfingRate = 0.0; stop(); }", 1, 262, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.sexRatio = 0.5; stop(); }", 1, 259, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.individualCount = 10; stop(); }", 1, 266, "read-only property", __LINE__);

	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (identical(p1.cloningRate, c(0.0,0.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.firstMaleIndex == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.individuals) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (identical(p1.immigrantSubpopFractions, float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (identical(p1.immigrantSubpopIDs, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.selfingRate == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.sexRatio == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.individualCount == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.tag = 135; if (p1.tag == 135) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.cloningRate = 0.0; stop(); }", 1, 282, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.firstMaleIndex = p1.firstMaleIndex; stop(); }", 1, 285, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.genomes = p1.genomes[0]; stop(); }", 1, 278, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.individuals = p1.individuals[0]; stop(); }", 1, 282, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.id = 1; stop(); }", 1, 273, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.immigrantSubpopFractions = 1.0; stop(); }", 1, 295, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.immigrantSubpopIDs = 1; stop(); }", 1, 289, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.selfingRate = 0.0; stop(); }", 1, 282, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.sexRatio = 0.5; stop(); }", 1, 279, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.individualCount = 10; stop(); }", 1, 286, "read-only property", __LINE__);
	
	// Test Subpopulation - (float)fitness(Ni indices)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.cachedFitness(NULL), rep(1.0, 10))) stop(); }", __LINE__);				// legal (after subpop construction)
	SLiMAssertScriptStop(gen1_setup_p1 + "2 { if (identical(p1.cachedFitness(NULL), rep(1.0, 10))) stop(); }", __LINE__);				// legal (after child generation)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.cachedFitness(0), 1.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.cachedFitness(0:3), rep(1.0, 4))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { identical(p1.cachedFitness(-1), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { identical(p1.cachedFitness(10), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { identical(p1.cachedFitness(c(-1,5)), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { identical(p1.cachedFitness(c(5,10)), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { identical(p1.cachedFitness(-1), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { identical(p1.cachedFitness(10), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { identical(p1.cachedFitness(c(-1,5)), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { identical(p1.cachedFitness(c(5,10)), rep(1.0, 10)); stop(); }", 1, 260, "out of range", __LINE__);
	
	// Test Subpopulation - (void)outputMSSample(integer$ sampleSize, [logical$ replace], [string$ requestedSex])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputMSSample(30, F); stop(); }", 1, 257, "not enough eligible genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F, 'M'); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F, 'F'); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputMSSample(1, F, 'Z'); stop(); }", 1, 257, "requested sex", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(30, F); stop(); }", 1, 277, "not enough eligible genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F, 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F, 'F'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputMSSample(1, F, 'Z'); stop(); }", 1, 277, "requested sex", __LINE__);
	
	// Test Subpopulation - (void)outputSample(integer$ sampleSize, [logical$ replace], [string$ requestedSex])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputSample(30, F); stop(); }", 1, 257, "not enough eligible genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputSample(1, F, 'M'); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputSample(1, F, 'F'); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputSample(1, F, 'Z'); stop(); }", 1, 257, "requested sex", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputSample(30, F); stop(); }", 1, 277, "not enough eligible genomes", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F, 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F, 'F'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputSample(1, F, 'Z'); stop(); }", 1, 277, "requested sex", __LINE__);
	
	// Test Subpopulation - (void)outputVCFSample(integer$ sampleSize, [logical$ replace], [string$ requestedSex], [logical$ outputMultiallelics)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(30, F); stop(); }", 1, 257, "not enough eligible individuals", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F, 'M'); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F, 'F'); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(1, F, 'Z'); stop(); }", 1, 257, "requested sex", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, 'M', F); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, 'F', F); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, '*', F); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, 'M', T); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, 'F', T); stop(); }", 1, 257, "non-sexual simulation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 late() { p1.outputVCFSample(5, F, '*', T); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(10); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(30); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(30, F); stop(); }", 1, 277, "not enough eligible individuals", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(30, T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F, 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F, 'F'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F, '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(1, F, 'Z'); stop(); }", 1, 277, "requested sex", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, 'M', F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, 'F', F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, '*', F); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, 'M', T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, 'F', T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 late() { p1.outputVCFSample(5, F, '*', T); stop(); }", __LINE__);
	
	// Test Subpopulation - (void)setCloningRate(numeric rate)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setCloningRate(0.0); } 10 { if (p1.cloningRate == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setCloningRate(0.5); } 10 { if (p1.cloningRate == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setCloningRate(1.0); } 10 { if (p1.cloningRate == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setCloningRate(-0.001); stop(); }", 1, 250, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setCloningRate(1.001); stop(); }", 1, 250, "within [0,1]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(0.0); } 10 { if (identical(p1.cloningRate, c(0.0, 0.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(0.5); } 10 { if (identical(p1.cloningRate, c(0.5, 0.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(1.0); } 10 { if (identical(p1.cloningRate, c(1.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setCloningRate(-0.001); stop(); }", 1, 270, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setCloningRate(1.001); stop(); }", 1, 270, "within [0,1]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(0.0, 0.1)); } 10 { if (identical(p1.cloningRate, c(0.0, 0.1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(0.5, 0.1)); } 10 { if (identical(p1.cloningRate, c(0.5, 0.1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(1.0, 0.1)); } 10 { if (identical(p1.cloningRate, c(1.0, 0.1))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(0.0, -0.001)); stop(); }", 1, 270, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(0.0, 1.001)); stop(); }", 1, 270, "within [0,1]", __LINE__);
	
	// Test Subpopulation - (void)setMigrationRates(io<Subpopulation> sourceSubpops, numeric rates)
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(2, 0.1); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(3, 0.1); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 3), c(0.1, 0.1)); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(1, 0.1); } 10 { stop(); }", 1, 300, "self-referential", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(4, 0.1); } 10 { stop(); }", 1, 300, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 1), c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "self-referential", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 4), c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 2), c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "two rates set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(p2, p2), c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "two rates set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 3), 0.1); } 10 { stop(); }", 1, 300, "to be equal in size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(2, c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "to be equal in size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(2, -0.0001); } 10 { stop(); }", 1, 300, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(2, 1.0001); } 10 { stop(); }", 1, 300, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 3), c(0.6, 0.6)); } 10 { stop(); }", -1, -1, "must sum to <= 1.0", __LINE__);	// raise is from EvolveSubpopulation(); we don't force constraints prematurely
	
	// Test Subpopulation - (void)setSelfingRate(numeric$ rate)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSelfingRate(0.0); } 10 { if (p1.selfingRate == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSelfingRate(0.5); } 10 { if (p1.selfingRate == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSelfingRate(1.0); } 10 { if (p1.selfingRate == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSelfingRate(-0.001); }", 1, 250, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSelfingRate(1.001); }", 1, 250, "within [0,1]", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setSelfingRate(0.0); stop(); }", __LINE__);													// we permit this, since a rate of 0.0 makes sense even in sexual sims
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSelfingRate(0.1); stop(); }", 1, 270, "cannot be called in sexual simulations", __LINE__);
	
	// Test Subpopulation - (void)setSexRatio(float$ sexRatio)
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSexRatio(0.0); stop(); }", 1, 250, "cannot be called in asexual simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSexRatio(0.1); stop(); }", 1, 250, "cannot be called in asexual simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.0); } 10 { if (p1.sexRatio == 0.0) stop(); }", 1, 270, "produced no males", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.1); } 10 { if (p1.sexRatio == 0.1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.5); } 10 { if (p1.sexRatio == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.9); } 10 { if (p1.sexRatio == 0.9) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSexRatio(1.0); } 10 { if (p1.sexRatio == 1.0) stop(); }", 1, 270, "produced no females", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSexRatio(-0.001); }", 1, 270, "within [0,1]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSexRatio(1.001); }", 1, 270, "within [0,1]", __LINE__);
	
	// Test Subpopulation - (void)setSubpopulationSize(integer$ size)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSubpopulationSize(0); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSubpopulationSize(0); if (p1.individualCount == 10) stop(); }", 1, 279, "undefined identifier", __LINE__);		// the symbol is undefined immediately
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { px=p1; p1.setSubpopulationSize(0); if (px.individualCount == 10) stop(); }", __LINE__);									// does not take visible effect until child generation
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSubpopulationSize(0); } 2 { if (p1.individualCount == 0) stop(); }", 1, 285, "undefined identifier", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSubpopulationSize(20); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSubpopulationSize(20); if (p1.individualCount == 10) stop(); }", __LINE__);					// does not take visible effect until child generation
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSubpopulationSize(20); } 2 { if (p1.individualCount == 20) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSubpopulationSize(-1); stop(); }", 1, 250, "out of range", __LINE__);
}

#pragma mark Individual tests
void _RunIndividualTests(void)
{	
	// ************************************************************************************
	//
	//	Gen 1+ tests: Individual
	//
	
	// Test Individual properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (size(i.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (all(i.index == (0:9))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (all(i.subpopulation == rep(p1, 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (all(i.sex == rep('H', 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tag = 135; if (all(i.tag == 135)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.uniqueMutations; stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.genomes = i[0].genomes[0]; stop(); }", 1, 277, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.index = i[0].index; stop(); }", 1, 275, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.subpopulation = i[0].subpopulation; stop(); }", 1, 283, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.sex = i[0].sex; stop(); }", 1, 273, "read-only property", __LINE__);
	//SLiMAssertScriptRaise(gen1_setup_p1 + "10 { i = p1.individuals; i.uniqueMutations = sim.mutations[0]; stop(); }", 1, 287, "read-only property", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (size(i.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (all(i.index == (0:9))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (all(i.subpopulation == rep(p1, 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (all(i.sex == repEach(c('F','M'), 5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.tag = 135; if (all(i.tag == 135)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 { i = p1.individuals; i.uniqueMutations; stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.genomes = i[0].genomes[0]; stop(); }", 1, 297, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.index = i[0].index; stop(); }", 1, 295, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.subpopulation = i[0].subpopulation; stop(); }", 1, 303, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.sex = i[0].sex; stop(); }", 1, 293, "read-only property", __LINE__);
	//SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 { i = p1.individuals; i.uniqueMutations = sim.mutations[0]; stop(); }", 1, 307, "read-only property", __LINE__);
	
	// Test Individual - (logical)containsMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.containsMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.containsMutations(sim.mutations); stop(); }", __LINE__);
	
	// Test Individual - (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.countOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.countOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i[0:1].countOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Individual - (object<Mutation>)uniqueMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.uniqueMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.uniqueMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i[0:1].uniqueMutationsOfType(1); stop(); }", __LINE__);
	
	// Test optional pedigree stuff
	std::string gen1_setup_norel("initialize() { initializeSLiMOptions(F); initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } ");
	std::string gen1_setup_rel("initialize() { initializeSLiMOptions(T); initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } ");
	
	SLiMAssertScriptStop(gen1_setup_norel + "5 { if (all(p1.individuals.pedigreeID == -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 { if (all(p1.individuals.pedigreeParentIDs == -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 { if (all(p1.individuals.pedigreeGrandparentIDs == -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 { if (p1.individuals[0].relatedness(p1.individuals[0]) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 { if (p1.individuals[0].relatedness(p1.individuals[1]) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 { if (all(p1.individuals[0].relatedness(p1.individuals[1:9]) == 0.0)) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals.pedigreeID != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals.pedigreeParentIDs != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals.pedigreeGrandparentIDs != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (p1.individuals[0].relatedness(p1.individuals[0]) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (p1.individuals[0].relatedness(p1.individuals[1]) <= 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals[0].relatedness(p1.individuals[1:9]) <= 0.5)) stop(); }", __LINE__);
}

#pragma mark Substitution tests
void _RunSubstitutionTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Substitution
	//
	
	// Test Substitution properties
	std::string gen1_setup_fixmut_p1("initialize() { initializeMutationRate(1e-4); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } 10 { sim.mutations[0].setSelectionCoeff(500.0); sim.recalculateFitness(); } ");
	
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { if (size(sim.substitutions) > 0) stop(); }", __LINE__);										// check that our script generates substitutions fast enough
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.fixationGeneration > 0 & sub.fixationGeneration <= 30) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.originGeneration > 0 & sub.originGeneration <= 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.position > 0 & sub.position <= 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { if (sum(sim.substitutions.selectionCoeff == 500.0) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.subpopID == 1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.fixationGeneration = 10; stop(); }", 1, 375, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.mutationType = m1; stop(); }", 1, 369, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.originGeneration = 10; stop(); }", 1, 373, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.position = 99999; stop(); }", 1, 365, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.selectionCoeff = 50.0; stop(); }", 1, 371, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.subpopID = 237; if (sub.subpopID == 237) stop(); }", __LINE__);						// legal; this field may be used as a user tag
	
	// No methods on Substitution
}

#pragma mark SLiMEidosBlock tests
void _RunSLiMEidosBlockTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: SLiMEidosBlock
	//
	
	// Test SLiMEidosBlock properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.active == -1) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.end == 4) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.id == 1) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.source == '{ sim = 10; }') stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.start == 2) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { s1.tag; stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.type == 'early') stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.type == 'early') stop(); } s1 2:4 early() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.type == 'late') stop(); } s1 2:4 late() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { s1.active = 198; if (s1.active == 198) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.end = 4; stop(); } s1 2:4 { sim = 10; } ", 1, 254, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.id = 1; stop(); } s1 2:4 { sim = 10; } ", 1, 253, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.source = '{ sim = 10; }'; stop(); } s1 2:4 { sim = 10; } ", 1, 257, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.start = 2; stop(); } s1 2:4 { sim = 10; } ", 1, 256, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { s1.tag = 219; if (s1.tag == 219) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.type = 'event'; stop(); } s1 2:4 { sim = 10; } ", 1, 255, "read-only property", __LINE__);
	
	// No methods on SLiMEidosBlock
}
	































































