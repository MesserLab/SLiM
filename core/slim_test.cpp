//
//  slim_test.cpp
//  SLiM
//
//  Created by Ben Haller on 8/14/15.
//  Copyright (c) 2015-2019 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

#include "slim_test.h"
#include "slim_sim.h"
#include "eidos_test.h"

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <unordered_map>
#include <map>
#include <utility>


// Helper functions for testing
void SLiMAssertScriptSuccess(const std::string &p_script_string, int p_lineNumber = -1);
void SLiMAssertScriptRaise(const std::string &p_script_string, const int p_bad_line, const int p_bad_position, const std::string &p_reason_snip, int p_lineNumber = -1);
void SLiMAssertScriptStop(const std::string &p_script_string, int p_lineNumber = -1);

// Keeping records of test success / failure
static int gSLiMTestSuccessCount = 0;
static int gSLiMTestFailureCount = 0;


// Instantiates and runs the script, and prints an error if the result does not match expectations
void SLiMAssertScriptSuccess(const std::string &p_script_string, int p_lineNumber)
{
	gSLiMTestFailureCount++;	// assume failure; we will fix this at the end if we succeed
	
	SLiMSim *sim = nullptr;
	std::istringstream infile(p_script_string);
	
	try {
		sim = new SLiMSim(infile);
		sim->InitializeRNGFromSeed(nullptr);
	}
	catch (...)
	{
		if (p_lineNumber != -1)
			std::cerr << "[" << p_lineNumber << "] ";
		
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during new SLiMSim(): " << Eidos_GetTrimmedRaiseMessage() << std::endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		return;
	}
	
	try {
		while (sim->_RunOneGeneration());
	}
	catch (...)
	{
		delete sim;
		MutationRun::DeleteMutationRunFreeList();
		
		if (p_lineNumber != -1)
			std::cerr << "[" << p_lineNumber << "] ";
		
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during RunOneGeneration(): " << Eidos_GetTrimmedRaiseMessage() << std::endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		return;
	}
	
	delete sim;
	MutationRun::DeleteMutationRunFreeList();
	
	gSLiMTestFailureCount--;	// correct for our assumption of failure above
	gSLiMTestSuccessCount++;
	
	//std::cerr << p_script_string << " : " << EIDOS_OUTPUT_SUCCESS_TAG << endl;
	
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

void SLiMAssertScriptRaise(const std::string &p_script_string, const int p_bad_line, const int p_bad_position, const std::string &p_reason_snip, int p_lineNumber)
{
	SLiMSim *sim = nullptr;
	
	try {
		std::istringstream infile(p_script_string);
		
		sim = new SLiMSim(infile);
		sim->InitializeRNGFromSeed(nullptr);
		
		while (sim->_RunOneGeneration());
		
		gSLiMTestFailureCount++;
		
		if (p_lineNumber != -1)
			std::cerr << "[" << p_lineNumber << "] ";
		
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : no raise during SLiM execution (expected \"" << p_reason_snip << "\")." << std::endl;
	}
	catch (...)
	{
		// We need to call Eidos_GetTrimmedRaiseMessage() here to empty the error stringstream, even if we don't log the error
		std::string raise_message = Eidos_GetTrimmedRaiseMessage();
		
		if (raise_message.find("stop() called") == std::string::npos)
		{
			if (raise_message.find(p_reason_snip) != std::string::npos)
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
						
						if (p_lineNumber != -1)
							std::cerr << "[" << p_lineNumber << "] ";
						
						std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise expected, but no error info set" << std::endl;
						std::cerr << "   raise message: " << raise_message << std::endl;
						std::cerr << "--------------------" << std::endl << std::endl;
					}
				}
				else
				{
					Eidos_ScriptErrorPosition(gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript);
					
					if ((gEidosErrorLine != p_bad_line) || (gEidosErrorLineCharacter != p_bad_position))
					{
						gSLiMTestFailureCount++;
						
						if (p_lineNumber != -1)
							std::cerr << "[" << p_lineNumber << "] ";
						
						std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise expected, but error position unexpected" << std::endl;
						std::cerr << "   raise message: " << raise_message << std::endl;
						Eidos_LogScriptError(std::cerr, gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript, gEidosExecutingRuntimeScript);
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
				
				if (p_lineNumber != -1)
					std::cerr << "[" << p_lineNumber << "] ";
				
				std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise message mismatch (expected \"" << p_reason_snip << "\")." << std::endl;
				std::cerr << "   raise message: " << raise_message << std::endl;
				std::cerr << "--------------------" << std::endl << std::endl;
			}
		}
		else
		{
			gSLiMTestFailureCount++;
			
			if (p_lineNumber != -1)
				std::cerr << "[" << p_lineNumber << "] ";
			
			std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : stop() reached (expected \"" << p_reason_snip << "\")." << std::endl;
			std::cerr << "--------------------" << std::endl << std::endl;
		}
		
		// Error messages that say (internal error) should not be possible to trigger in script
		if (raise_message.find("(internal error)") != std::string::npos)
		{
			std::cerr << p_script_string << " : error message contains (internal error) erroneously" << std::endl;
			std::cerr << "   raise message: " << raise_message << std::endl;
		}
	}
	
	delete sim;
	MutationRun::DeleteMutationRunFreeList();
	
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

void SLiMAssertScriptStop(const std::string &p_script_string, int p_lineNumber)
{
	SLiMSim *sim = nullptr;
	
	try {
		std::istringstream infile(p_script_string);
		
		sim = new SLiMSim(infile);
		sim->InitializeRNGFromSeed(nullptr);
		
		while (sim->_RunOneGeneration());
		
		gSLiMTestFailureCount++;
		
		if (p_lineNumber != -1)
			std::cerr << "[" << p_lineNumber << "] ";
		
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : no raise during SLiM execution." << std::endl;
	}
	catch (...)
	{
		// We need to call Eidos_GetTrimmedRaiseMessage() here to empty the error stringstream, even if we don't log the error
		std::string raise_message = Eidos_GetTrimmedRaiseMessage();
		
		if (raise_message.find("stop() called") == std::string::npos)
		{
			gSLiMTestFailureCount++;
			
			if (p_lineNumber != -1)
				std::cerr << "[" << p_lineNumber << "] ";
			
			std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : stop() not reached" << std::endl;
			std::cerr << "   raise message: " << raise_message << std::endl;
			
			if ((gEidosCharacterStartOfError != -1) && (gEidosCharacterEndOfError != -1) && gEidosCurrentScript)
			{
				Eidos_ScriptErrorPosition(gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript);
				Eidos_LogScriptError(std::cerr, gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript, gEidosExecutingRuntimeScript);
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
	MutationRun::DeleteMutationRunFreeList();
	
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
static void _RunInteractionTypeTests(void);
static void _RunSubstitutionTests(void);
static void _RunSLiMEidosBlockTests(void);
static void _RunContinuousSpaceTests(void);
static void _RunNonWFTests(void);
static void _RunTreeSeqTests(void);
static void _RunNucleotideFunctionTests(void);
static void _RunNucleotideMethodTests(void);
static void _RunSLiMTimingTests(void);


// Test function shared strings
static std::string gen1_setup("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } ");
static std::string gen1_setup_sex("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeSex('X'); } ");
static std::string gen2_stop(" 2 { stop(); } ");
static std::string gen1_setup_highmut_p1("initialize() { initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } ");
static std::string gen1_setup_fixmut_p1("initialize() { initializeMutationRate(1e-4); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } 10 { sim.mutations[0].setSelectionCoeff(500.0); sim.recalculateFitness(); } ");
static std::string gen1_setup_i1("initialize() { initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', ''); } 1 { sim.addSubpop('p1', 10); } 1:10 late() { i1.evaluate(); i1.strength(p1.individuals[0]); } ");
static std::string gen1_setup_i1x("initialize() { initializeSLiMOptions(dimensionality='x'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'x'); } 1 { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); i1.evaluate(); i1.strength(p1.individuals[0]); } ");
static std::string gen1_setup_i1xPx("initialize() { initializeSLiMOptions(dimensionality='x', periodicity='x'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'x'); } 1 { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); i1.evaluate(); i1.strength(p1.individuals[0]); } ");
static std::string gen1_setup_i1xyz("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xyz'); } 1 { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); p1.individuals.y = runif(10); p1.individuals.z = runif(10); i1.evaluate(); i1.strength(p1.individuals[0]); } ");
static std::string gen1_setup_i1xyzPxz("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='xz'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xyz'); } 1 { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); p1.individuals.y = runif(10); p1.individuals.z = runif(10); i1.evaluate(); i1.strength(p1.individuals[0]); } ");
static std::string gen1_setup_p1(gen1_setup + "1 { sim.addSubpop('p1', 10); } ");
static std::string gen1_setup_sex_p1(gen1_setup_sex + "1 { sim.addSubpop('p1', 10); } ");
static std::string gen1_setup_p1p2p3(gen1_setup + "1 { sim.addSubpop('p1', 10); sim.addSubpop('p2', 10); sim.addSubpop('p3', 10); } ");

static std::string WF_prefix("initialize() { initializeSLiMModelType('WF'); } ");
static std::string nonWF_prefix("initialize() { initializeSLiMModelType('nonWF'); } ");


int RunSLiMTests(void)
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
	_RunContinuousSpaceTests();
	_RunNonWFTests();
	_RunTreeSeqTests();
	_RunNucleotideFunctionTests();
	_RunNucleotideMethodTests();
	_RunSLiMTimingTests();
	
	_RunInteractionTypeTests();		// many tests, time-consuming, so do this last
	
	// ************************************************************************************
	//
	//	Print a summary of test results
	//
	std::cerr << std::endl;
	if (gSLiMTestFailureCount)
		std::cerr << "" << EIDOS_OUTPUT_FAILURE_TAG << " count: " << gSLiMTestFailureCount << std::endl;
	std::cerr << EIDOS_OUTPUT_SUCCESS_TAG << " count: " << gSLiMTestSuccessCount << std::endl;
	std::cerr.flush();
	
	// Clear out the SLiM output stream post-test
	gSLiMOut.clear();
	gSLiMOut.str("");
	
	// return a standard Unix result code indicating success (0) or failure (1);
	return (gSLiMTestFailureCount > 0) ? 1 : 0;
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
						  3 { stop(); }
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
	SLiMAssertScriptStop("initialize() { initializeGeneConversion(0.5, 10000000000000, 0.0); stop(); }", __LINE__);										// legal; no max for meanLength
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(-0.001, 10000000000000, 0.0); stop(); }", 1, 15, "nonCrossoverFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(1.001, 10000000000000, 0.0); stop(); }", 1, 15, "nonCrossoverFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, -0.01, 0.0); stop(); }", 1, 15, "meanLength must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, -0.001); stop(); }", 1, 15, "simpleConversionFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, 1.001); stop(); }", 1, 15, "simpleConversionFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, 0.0, -1.001); stop(); }", 1, 15, "bias must be between -1.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, 0.0, 1.001); stop(); }", 1, 15, "bias must be between -1.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 1000, 0.0, 0.1); stop(); }", 1, 15, "must be 0.0 in non-nucleotide-based models", __LINE__);
	
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
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 'foo'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 'foo', 0.0); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0, 'foo'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', 'foo'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 'foo', 0.0); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 0.0, 'foo'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 'foo', 0.0); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0, 'foo'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', '1'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', '1', 0.0); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0, '1'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', '1'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', '1', 0.0); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 0.0, '1'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', '1', 0.0); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0, '1'); stop(); }", 1, 15, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'x', 0.0); stop(); }", 1, 15, "must be \"f\", \"g\", \"e\", \"n\", \"w\", or \"s\"", __LINE__);
	SLiMAssertScriptStop("initialize() { x = initializeMutationType('m7', 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { x = initializeMutationType(7, 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { m7 = 15; initializeMutationType(7, 0.5, 'f', 0.0); stop(); }", 1, 24, "already defined", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); initializeMutationType('m1', 0.5, 'f', 0.0); stop(); }", 1, 60, "already defined", __LINE__);
	
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 3.1, 0.0); stop(); }", 1, 15, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 3.1, -1.0); stop(); }", 1, 15, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'n', 3.1, -1.0); stop(); }", 1, 15, "must have a standard deviation parameter >= 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 0.0, 7.5); stop(); }", 1, 15, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', -1.0, 7.5); stop(); }", 1, 15, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 3.1, 0.0); stop(); }", 1, 15, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'w', 3.1, -7.5); stop(); }", 1, 15, "must have a shape parameter > 0", __LINE__);
	
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
	//SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000001); stop(); }", 1, 157, "out of range", __LINE__);		// now legal!
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000000000000); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000000000001); stop(); }", 1, 157, "out of range", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 100, 99); stop(); }", 1, 157, "is less than start position", __LINE__);
	
	// Test (void)initializeMutationRate(numeric$ rate)
	SLiMAssertScriptStop("initialize() { initializeMutationRate(0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(); stop(); }", 1, 15, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(-0.0000001); stop(); }", 1, 15, "requires rates to be >= 0", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeMutationRate(10000000); stop(); }", __LINE__);														// legal; no maximum rate
	
	// Test (void)initializeRecombinationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(); stop(); }", 1, 15, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(-0.00001); stop(); }", 1, 15, "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(0.5); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(0.6); stop(); }", 1, 15, "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(10000); stop(); }", 1, 15, "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000)); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 15, "requires rates to be a singleton if", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(integer(0), integer(0)); stop(); }", 1, 15, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), 1000); stop(); }", 1, 15, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), 1:3); stop(); }", 1, 15, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000)); stop(); }", 1, 15, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000)); stop(); }", 1, 15, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 15, "requires rates to be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(); stop(); }", 1, 35, "missing required argument", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(-0.00001); stop(); }", 1, 35, "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(0.5); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(0.6); stop(); }", 1, 35, "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(10000); stop(); }", 1, 35, "requires rates to be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000)); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 35, "requires rates to be a singleton if", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(integer(0), integer(0)); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1000); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1:3); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000)); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000)); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 35, "requires rates to be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(integer(0), integer(0), '*'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1000, '*'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1:3, '*'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000), '*'); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000), '*'); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 35, "requires rates to be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(integer(0), integer(0), 'M'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1000, 'M'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), 1:3, 'M'); stop(); }", 1, 35, "ends and rates to be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000), 'M'); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000), 'M'); stop(); }", 1, 35, "ascending order", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", 1, 35, "requires rates to be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, 'F'); stop(); } 1 {}", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 3000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, 'F'); } 1 {}", -1, -1, "do not cover the full chromosome", __LINE__);
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 1000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2000, 'F'); } 1 { stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 1999, 'F'); } 1 {}", -1, -1, "do not cover the full chromosome", __LINE__);
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeMutationRate(0.0); initializeGenomicElement(g1, 0, 2000); initializeSex('A'); initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000), 'M'); initializeRecombinationRate(0.0, 2001, 'F'); } 1 { stop(); }", -1, -1, "do not cover the full chromosome", __LINE__);
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
	
	// Test (void)initializeSLiMModelType(string$ modelType)
	SLiMAssertScriptRaise("initialize() { initializeSLiMModelType(); stop(); }", 1, 15, "missing required argument modelType", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMModelType('WF'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMModelType('nonWF'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMModelType('foo'); stop(); }", 1, 15, "legal values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(); initializeSLiMModelType('WF'); stop(); }", 1, 40, "must be called before", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(0.0); initializeSLiMModelType('WF'); stop(); }", 1, 44, "must be called before", __LINE__);
	
	// Test (void)initializeSLiMOptions([logical$ keepPedigrees = F], [string$ dimensionality = ""], [string$ periodicity = ""], [integer$ mutationRuns = 0], [logical$ preventIncidentalSelfing = F])
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(F); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(T); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(F, ''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(T, ''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(F, 'xyz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(T, 'xyz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(keepPedigrees=T); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality=''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xy'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='', periodicity=''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x', periodicity=''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x', periodicity='x'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xy', periodicity=''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xy', periodicity='x'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xy', periodicity='y'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xy', periodicity='xy'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity=''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='x'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='y'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='z'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='xy'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='xz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='yz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='xyz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(mutationRuns=0); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(mutationRuns=1); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(mutationRuns=100); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(preventIncidentalSelfing=F); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(preventIncidentalSelfing=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(keepPedigrees=NULL); stop(); }", 1, 15, "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality=NULL); stop(); }", 1, 15, "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(mutationRuns=NULL); stop(); }", 1, 15, "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(preventIncidentalSelfing=NULL); stop(); }", 1, 15, "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='foo'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='y'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='z'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xz'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='yz'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='zyx'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='', periodicity='x'); stop(); }", 1, 15, "may not be set in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x', periodicity='y'); stop(); }", 1, 15, "cannot utilize spatial dimensions beyond", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x', periodicity='z'); stop(); }", 1, 15, "cannot utilize spatial dimensions beyond", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xy', periodicity='z'); stop(); }", 1, 15, "cannot utilize spatial dimensions beyond", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='foo'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='xzy'); stop(); }", 1, 15, "legal non-empty values", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(); initializeSLiMOptions(); stop(); }", 1, 40, "may be called only once", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(0.0); initializeSLiMOptions(); stop(); }", 1, 44, "must be called before", __LINE__);
	
	// Test (object<InteractionType>$)initializeInteractionType(is$ id, string$ spatiality, [logical$ reciprocal = F], [numeric$ maxDistance = INF], [string$ sexSegregation = "**"])
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(-1, ''); stop(); }", 1, 15, "identifier value is out of range", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeInteractionType(0, ''); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeInteractionType('i0', ''); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, 'x'); stop(); }", 1, 15, "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType('i0', 'x'); stop(); }", 1, 15, "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, 'w'); stop(); }", 1, 15, "spatiality \"w\" must be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType('i0', 'w'); stop(); }", 1, 15, "spatiality \"w\" must be", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeInteractionType(0, '', T); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, '', T, 0.1); stop(); }", 1, 15, "must be INF for non-spatial interactions", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeInteractionType(0, '', T, INF, '**'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, '', T, INF, '*M'); stop(); }", 1, 15, "unsupported in non-sexual simulation", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, '**'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, '*M'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, '*F'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'M*'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'MM'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'MF'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'F*'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'FM'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSex('A'); initializeInteractionType(0, '', T, INF, 'FF'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, '', T, INF, 'W*'); stop(); }", 1, 15, "unsupported sexSegregation value", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeInteractionType(0, '', T, INF, '*W'); stop(); }", 1, 15, "unsupported sexSegregation value", __LINE__);

	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'w'); stop(); }", 1, 58, "spatiality \"w\" must be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType('i0', 'w'); stop(); }", 1, 58, "spatiality \"w\" must be", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T, 0.1); stop(); }", 1, 58, "must be INF for non-spatial interactions", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T, INF, '**'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T, INF, '*M'); stop(); }", 1, 58, "unsupported in non-sexual simulation", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeSex('A'); initializeInteractionType(0, '', T, INF, '*M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, '', T, INF, 'W*'); stop(); }", 1, 58, "unsupported sexSegregation value", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'y'); stop(); }", 1, 58, "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', F); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', F, 0.1); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T, 0.1); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T, 0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T, -0.1); stop(); }", 1, 58, "maxDistance must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='x'); initializeInteractionType(0, 'x', T, 0.1, '*M'); stop(); }", 1, 58, "unsupported in non-sexual simulation", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='x'); initializeSex('A'); initializeInteractionType(0, 'x', T, 0.1, '*M'); stop(); }", __LINE__);
	
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'x'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'y'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'z'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'xy'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'yz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'xz'); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'xyz'); stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'w'); stop(); }", 1, 60, "spatiality \"w\" must be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'yx'); stop(); }", 1, 60, "spatiality \"yx\" must be", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeInteractionType(0, 'zyx'); stop(); }", 1, 60, "spatiality \"zyx\" must be", __LINE__);
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
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.modelType == 'WF') stop(); } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { if (sim.modelType == 'WF') stop(); } ", __LINE__);
	SLiMAssertScriptStop(WF_prefix + gen1_setup + "1 { if (sim.modelType == 'WF') stop(); } ", __LINE__);
	SLiMAssertScriptStop(WF_prefix + gen1_setup_sex + "1 { if (sim.modelType == 'WF') stop(); } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.modelType = 'foo'; } ", 1, 230, "read-only property", __LINE__);
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
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.tag; } ", 1, 220, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { c(sim,sim).tag; } ", 1, 227, "before being set", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.tag = -17; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.tag = -17; } 2 { if (sim.tag == -17) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.dimensionality == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { if (sim.dimensionality == '') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.dimensionality = 'x'; }", 1, 366, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (sim.dimensionality == 'x') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (size(sim.interactionTypes) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { if (sim.interactionTypes == i1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.interactionTypes = i1; }", 1, 368, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (sim.interactionTypes == i1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.periodicity == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { if (sim.periodicity == '') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.periodicity = 'x'; }", 1, 363, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (sim.periodicity == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz + "1 { if (sim.periodicity == 'xz') stop(); }", __LINE__);
	
#ifdef SLIMGUI
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.inSLiMgui == T) stop(); } ", __LINE__);
#else
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.inSLiMgui == F) stop(); } ", __LINE__);
#endif
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.inSLiMgui = T; }", 1, 230, "read-only property", __LINE__);
	
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
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(spatialPositions=T); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(spatialPositions=F); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(ages=T); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(ages=F); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1x + "1 late() { sim.outputFull(spatialPositions=T); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1x + "1 late() { sim.outputFull(spatialPositions=F); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1x + "1 late() { sim.outputFull(ages=T); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1x + "1 late() { sim.outputFull(ages=F); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 late() { sim.outputFull(NULL, T); }", 1, 308, "cannot output in binary format", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull('/tmp/slimOutputFullTest.txt'); }", __LINE__);								// legal, output to file path; this test might work only on Un*x systems
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 late() { sim.outputFull('/tmp/slimOutputFullTest.slimbinary', T); }", __LINE__);						// legal, output to file path; this test might work only on Un*x systems
	SLiMAssertScriptSuccess(gen1_setup_i1x + "1 late() { p1.individuals.x = runif(10); sim.outputFull('/tmp/slimOutputFullTest_POSITIONS.txt'); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1x + "1 late() { p1.individuals.x = runif(10); sim.outputFull('/tmp/slimOutputFullTest_POSITIONS.slimbinary', T); }", __LINE__);
	
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
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.readFromPopulationFile('/tmp/slimOutputFullTest_POSITIONS.txt'); }", 1, 220, "spatial dimension or age information", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.readFromPopulationFile('/tmp/slimOutputFullTest_POSITIONS.slimbinary'); }", 1, 220, "output spatial dimensionality does not match", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1x + "1 { sim.readFromPopulationFile('/tmp/slimOutputFullTest_POSITIONS.txt'); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_i1x + "1 { sim.readFromPopulationFile('/tmp/slimOutputFullTest_POSITIONS.slimbinary'); }", __LINE__);
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
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEarlyEvent(1, '{ $; }', 2, 2); }", 1, 2, "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerLateEvent(Nis$ id, string$ source, [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerLateEvent(NULL, '{ stop(); }', 2, 2); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent('s1', '{ stop(); }', 2, 2); } s1 { }", 1, 251, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerLateEvent('s1', '{ stop(); }', 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerLateEvent(1, '{ stop(); }', 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ stop(); }', 2, 2); sim.registerLateEvent(1, '{ stop(); }', 2, 2); }", 1, 298, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ stop(); }', 3, 2); }", 1, 251, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ stop(); }', -1, -1); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ stop(); }', 0, 0); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerLateEvent(1, '{ $; }', 2, 2); }", 1, 2, "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerFitnessCallback(Nis$ id, string$ source, Nio<MutationType>$ mutType, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', 1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', m1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', NULL, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', 1, 1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', m1, p1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', NULL, p1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', 1); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', m1); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', NULL); } 10 { ; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }'); }", 1, 251, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback('s1', '{ stop(); }', m1, NULL, 2, 2); } s1 { }", 1, 251, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { s1 = 7; sim.registerFitnessCallback('s1', '{ stop(); }', m1, NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { s1 = 7; sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); }", 1, 259, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); }", 1, 314, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 3, 2); }", 1, 251, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, -1, -1); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 0, 0); }", 1, 251, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ $; }', m1, NULL, 2, 2); }", 1, 2, "unexpected token '$'", __LINE__);
	
	// Test sim - (object<SLiMEidosBlock>)registerInteractionCallback(Nis$ id, string$ source, io<InteractionType>$ intType, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }', 1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }', i1, NULL, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }', 1, 1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }', i1, p1, 5, 10); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }', 1); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }', i1); } 10 { ; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback(NULL, '{ stop(); }'); }", 1, 351, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback('s1', '{ stop(); }', i1, NULL, 2, 2); } s1 { }", 1, 351, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { s1 = 7; sim.registerInteractionCallback('s1', '{ stop(); }', i1, NULL, 2, 2); }", 1, 359, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { s1 = 7; sim.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 2, 2); }", 1, 359, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 2, 2); sim.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 2, 2); }", 1, 418, "already defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 3, 2); }", 1, 351, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback(1, '{ stop(); }', i1, NULL, -1, -1); }", 1, 351, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback(1, '{ stop(); }', i1, NULL, 0, 0); }", 1, 351, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1 + "1 { sim.registerInteractionCallback(1, '{ $; }', i1, NULL, 2, 2); }", 1, 2, "unexpected token '$'", __LINE__);
	
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
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ $; }', NULL, 2, 2); }", 1, 2, "unexpected token '$'", __LINE__);
	
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
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ $; }', NULL, 2, 2); }", 1, 2, "unexpected token '$'", __LINE__);
	
	// Test sim – (object<SLiMEidosBlock>)rescheduleScriptBlock(object<SLiMEidosBlock>$ block, [Ni$ start = NULL], [Ni$ end = NULL], [Ni generations = NULL])
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=10, end=9); stop(); } s1 10 { }", 1, 255, "requires start <= end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, generations=integer(0)); stop(); } s1 10 { }", 1, 255, "requires at least one generation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, generations=c(25, 25)); stop(); } s1 10 { }", 1, 255, "same generation cannot be used twice", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=25, end=25, generations=25); stop(); } s1 10 { }", 1, 255, "either start/end or generations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=25, end=NULL, generations=25); stop(); } s1 10 { }", 1, 255, "either start/end or generations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=NULL, end=25, generations=25); stop(); } s1 10 { }", 1, 255, "either start/end or generations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1); stop(); } s1 10 { }", 1, 255, "either start/end or generations", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=25, end=25); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25)) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=25, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25:29)) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=NULL, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 { }", 1, 255, "for the currently executing", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 { }", 1, 255, "for the currently executing", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { b = sim.rescheduleScriptBlock(s1, start=NULL, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 { }", 1, 255, "scheduled for a past generation", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { b = sim.rescheduleScriptBlock(s1, end=29); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 1:29)) stop(); } s1 10 { }", 1, 255, "scheduled for a past generation", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=25, end=NULL); if (b.start == 25 & b.end == 1000000001) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, start=25); if (b.start == 25 & b.end == 1000000001) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, generations=25); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25)) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, generations=25:28); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, 25:28)) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, generations=c(25:28, 35)); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, c(25:28, 35))) stop(); } s1 10 { }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { b = sim.rescheduleScriptBlock(s1, generations=c(13, 25:28)); r = sapply(b, 'applyValue.start:applyValue.end;'); if (identical(r, c(13, 25:28))) stop(); } s1 10 { }", __LINE__);
	
	// Test sim - (void)simulationFinished(void)
	SLiMAssertScriptStop(gen1_setup_p1 + "11 { stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1 + "10 { sim.simulationFinished(); } 11 { stop(); }", __LINE__);
	
	// Test sim SLiMEidosDictionary functionality: - (+)getValue(string$ key) and - (void)setValue(string$ key, + value)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.setValue('foo', 7:9); sim.setValue('bar', 'baz'); } 10 { if (identical(sim.getValue('foo'), 7:9) & identical(sim.getValue('bar'), 'baz')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.setValue('foo', 3:5); sim.setValue('foo', 'foobar'); } 10 { if (identical(sim.getValue('foo'), 'foobar')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.setValue('foo', 3:5); sim.setValue('foo', NULL); } 10 { if (isNULL(sim.getValue('foo'))) stop(); }", __LINE__);
}

#pragma mark MutationType tests
void _RunMutationTypeTests(void)
{	
	// ************************************************************************************
	//
	//	Gen 1+ tests: MutationType
	//
	
	// Test MutationType properties
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.color == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.convertToSubstitution == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.mutationStackGroup == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.mutationStackPolicy == 's') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.distributionParams == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.distributionType == 'f') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.dominanceCoeff == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.color = ''; } 2 { if (m1.color == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.color = 'red'; } 2 { if (m1.color == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.color = '#FF0000'; } 2 { if (m1.color == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.colorSubstitution = ''; } 2 { if (m1.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.colorSubstitution = 'red'; } 2 { if (m1.colorSubstitution == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.colorSubstitution = '#FF0000'; } 2 { if (m1.colorSubstitution == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.tag; }", 1, 219, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { c(m1,m1).tag; }", 1, 225, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.tag = 17; } 2 { if (m1.tag == 17) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.convertToSubstitution = F; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.mutationStackGroup = -17; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.mutationStackPolicy = 's'; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.mutationStackPolicy = 'f'; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.mutationStackPolicy = 'l'; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.mutationStackPolicy = 'z'; }", 1, 239, "property mutationStackPolicy must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.distributionParams = 0.1; }", 1, 238, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.distributionType = 'g'; }", 1, 236, "read-only property", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.dominanceCoeff = 0.3; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.id = 2; }", 1, 222, "read-only property", __LINE__);

	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); c(m1,m2).mutationStackGroup = 3; c(m1,m2).mutationStackPolicy = 'f'; } 1 { stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); c(m1,m2).mutationStackGroup = 3; m1.mutationStackPolicy = 'f'; m2.mutationStackPolicy = 'l'; } 1 { stop(); }", -1, -1, "inconsistent mutationStackPolicy", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); c(m1,m2).mutationStackGroup = 3; c(m1,m2).mutationStackPolicy = 'f'; } 1 { m2.mutationStackPolicy = 'l'; }", -1, -1, "inconsistent mutationStackPolicy", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); m1.mutationStackPolicy = 'f'; m2.mutationStackPolicy = 'l'; } 1 { stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); m1.mutationStackPolicy = 'f'; m2.mutationStackPolicy = 'l'; } 1 { c(m1,m2).mutationStackGroup = 3; }", -1, -1, "inconsistent mutationStackPolicy", __LINE__);
	
	// Test MutationType - (void)setDistribution(string$ distributionType, ...)
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('f', 2.2); if (m1.distributionType == 'f' & m1.distributionParams == 2.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('g', 3.1, 7.5); if (m1.distributionType == 'g' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('e', -3); if (m1.distributionType == 'e' & m1.distributionParams == -3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('n', 3.1, 7.5); if (m1.distributionType == 'n' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('w', 3.1, 7.5); if (m1.distributionType == 'w' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('s', 'return 1;'); if (m1.distributionType == 's' & identical(m1.distributionParams, 'return 1;')) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('x', 1.5); stop(); }", 1, 219, "must be \"f\", \"g\", \"e\", \"n\", \"w\", or \"s\"", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', 'foo'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 'foo', 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, 'foo'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', 'foo'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 'foo', 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 3.1, 'foo'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 'foo', 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 3.1, 'foo'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('s', 3); stop(); }", 1, 219, "must be of type string", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', '1'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', '1', 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, '1'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', '1'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', '1', 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 3.1, '1'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', '1', 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 3.1, '1'); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('s', 3.1); stop(); }", 1, 219, "must be of type string", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', T); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', T, 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, T); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', T); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', T, 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 3.1, T); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', T, 7.5); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 3.1, T); stop(); }", 1, 219, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('s', T); stop(); }", 1, 219, "must be of type string", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, 0.0); }", 1, 219, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, -1.0); }", 1, 219, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('n', 3.1, -1.0); }", 1, 219, "must have a standard deviation parameter >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 0.0, 7.5); }", 1, 219, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', -1.0, 7.5); }", 1, 219, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 3.1, 0.0); }", 1, 219, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('w', 3.1, -7.5); }", 1, 219, "must have a shape parameter > 0", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { m1.setDistribution('s', 'return foo;'); } 100 { stop(); }", -1, -1, "undefined identifier foo", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { m1.setDistribution('s', 'x >< 5;'); } 100 { stop(); }", -1, -1, "tokenize/parse error in type 's' DFE callback script", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { m1.setDistribution('s', 'x $ 5;'); } 100 { stop(); }", -1, -1, "tokenize/parse error in type 's' DFE callback script", __LINE__);
	
	// Test MutationType - (float)drawSelectionCoefficient([integer$ n = 1])
	// the parameters here are chosen so that these tests should fail extremely rarely
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('f', 2.2); if (m1.drawSelectionCoefficient() == 2.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('f', 2.2); if (identical(m1.drawSelectionCoefficient(10), rep(2.2, 10))) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.setDistribution('g', 3.1, 7.5); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('g', 3.1, 7.5); if (abs(mean(m1.drawSelectionCoefficient(5000)) - 3.1) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.setDistribution('e', -3.0); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('e', -3.0); if (abs(mean(m1.drawSelectionCoefficient(30000)) + 3.0) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.setDistribution('n', 3.1, 0.5); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('n', 3.1, 0.5); if (abs(mean(m1.drawSelectionCoefficient(2000)) - 3.1) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.setDistribution('w', 3.1, 7.5); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('w', 3.1, 7.5); if (abs(mean(m1.drawSelectionCoefficient(2000)) - 2.910106) < 0.1) stop(); }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.setDistribution('s', 'rbinom(1, 4, 0.5);'); m1.drawSelectionCoefficient(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('s', 'rbinom(1, 4, 0.5);'); if (abs(mean(m1.drawSelectionCoefficient(5000)) - 2.0) < 0.1) stop(); }", __LINE__);
}

#pragma mark GenomicElementType tests
void _RunGenomicElementTypeTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: GenomicElementType
	//
	
	// Test GenomicElementType properties
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.color == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.id = 2; }", 1, 222, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.mutationFractions == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.mutationTypes == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.color = ''; } 2 { if (g1.color == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.color = 'red'; } 2 { if (g1.color == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.color = '#FF0000'; } 2 { if (g1.color == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.tag; }", 1, 219, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { c(g1,g1).tag; }", 1, 225, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { g1.tag = 17; } 2 { if (g1.tag == 17) stop(); }", __LINE__);
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
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.tag; }", 1, 300, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.tag = -12; if (ge.tag == -12) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.endPosition = 999; stop(); }", 1, 312, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.startPosition = 0; stop(); }", 1, 314, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.genomicElementType = g1; stop(); }", 1, 319, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.endPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.startPosition == 1000) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.tag; }", 1, 300, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.tag = -17; if (ge.tag == -17) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.endPosition = 99999; stop(); }", 1, 312, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.startPosition = 1000; stop(); }", 1, 314, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.genomicElementType = g1; stop(); }", 1, 319, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements; ge.tag; }", 1, 297, "before being set", __LINE__);
	
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
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.colorSubstitution == '#3333FF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionEnabled == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionGCBias == 0.0) stop(); }", 1, 244, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionNonCrossoverFraction == 0.0) stop(); }", 1, 244, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionMeanLength == 0.0) stop(); }", 1, 244, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionSimpleConversionFraction == 0.0) stop(); }", 1, 244, "not defined since the DSB", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.genomicElements[0].genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.lastPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.overallRecombinationRate == 1e-8 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRateM)) stop(); }", 1, 251, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRateF)) stop(); }", 1, 251, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.recombinationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositionsM)) stop(); }", 1, 251, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositionsF)) stop(); }", 1, 251, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.recombinationRates == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRatesM)) stop(); }", 1, 251, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRatesF)) stop(); }", 1, 251, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.overallMutationRate == 1e-7 * 100000) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.overallMutationRateM)) stop(); }", 1, 251, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.overallMutationRateF)) stop(); }", 1, 251, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.mutationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.mutationEndPositionsM)) stop(); }", 1, 251, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.mutationEndPositionsF)) stop(); }", 1, 251, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.mutationRates == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.mutationRatesM)) stop(); }", 1, 251, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; if (isNULL(ch.mutationRatesF)) stop(); }", 1, 251, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.colorSubstitution = ''; if (ch.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.colorSubstitution = 'red'; if (ch.colorSubstitution == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.colorSubstitution = '#FF0000'; if (ch.colorSubstitution == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.tag; }", 1, 240, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; c(ch,ch).tag; }", 1, 246, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.tag = 3294; if (ch.tag == 3294) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.genomicElements = ch.genomicElements; stop(); }", 1, 256, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.lastPosition = 99999; stop(); }", 1, 253, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallRecombinationRate = 1e-2; stop(); }", 1, 265, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallRecombinationRateM = 1e-2; stop(); }", 1, 266, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallRecombinationRateF = 1e-2; stop(); }", 1, 266, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationEndPositions = 99999; stop(); }", 1, 266, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationEndPositionsM = 99999; stop(); }", 1, 267, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationEndPositionsF = 99999; stop(); }", 1, 267, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationRates = 1e-8; stop(); }", 1, 259, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationRatesM = 1e-8; stop(); }", 1, 260, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationRatesF = 1e-8; stop(); }", 1, 260, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallMutationRate = 1e-2; stop(); }", 1, 260, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallMutationRateM = 1e-2; stop(); }", 1, 261, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallMutationRateF = 1e-2; stop(); }", 1, 261, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.mutationEndPositions = 99999; stop(); }", 1, 261, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.mutationEndPositionsM = 99999; stop(); }", 1, 262, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.mutationEndPositionsF = 99999; stop(); }", 1, 262, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.mutationRates = 1e-8; stop(); }", 1, 254, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.mutationRatesM = 1e-8; stop(); }", 1, 255, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.mutationRatesF = 1e-8; stop(); }", 1, 255, "read-only property", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.colorSubstitution == '#3333FF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.geneConversionEnabled == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.geneConversionGCBias == 0.0) stop(); }", 1, 264, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.geneConversionNonCrossoverFraction == 0.0) stop(); }", 1, 264, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.geneConversionMeanLength == 0.0) stop(); }", 1, 264, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.geneConversionSimpleConversionFraction == 0.0) stop(); }", 1, 264, "not defined since the DSB", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.genomicElements[0].genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.lastPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.overallRecombinationRate == 1e-8 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRateM)) stop(); }", 1, 271, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRateF)) stop(); }", 1, 271, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.recombinationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositionsM)) stop(); }", 1, 271, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositionsF)) stop(); }", 1, 271, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.recombinationRates == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRatesM)) stop(); }", 1, 271, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRatesF)) stop(); }", 1, 271, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.overallMutationRate == 1e-7 * 100000) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.overallMutationRateM)) stop(); }", 1, 271, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.overallMutationRateF)) stop(); }", 1, 271, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.mutationEndPositions == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.mutationEndPositionsM)) stop(); }", 1, 271, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.mutationEndPositionsF)) stop(); }", 1, 271, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; if (ch.mutationRates == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.mutationRatesM)) stop(); }", 1, 271, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; if (isNULL(ch.mutationRatesF)) stop(); }", 1, 271, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.colorSubstitution = ''; if (ch.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.colorSubstitution = 'red'; if (ch.colorSubstitution == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.colorSubstitution = '#FF0000'; if (ch.colorSubstitution == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.tag; }", 1, 260, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; c(ch,ch).tag; }", 1, 266, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.tag = 3294; if (ch.tag == 3294) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.genomicElements = ch.genomicElements; stop(); }", 1, 276, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.lastPosition = 99999; stop(); }", 1, 273, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallRecombinationRate = 1e-2; stop(); }", 1, 285, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallRecombinationRateM = 1e-2; stop(); }", 1, 286, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallRecombinationRateF = 1e-2; stop(); }", 1, 286, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationEndPositions = 99999; stop(); }", 1, 286, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationEndPositionsM = 99999; stop(); }", 1, 287, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationEndPositionsF = 99999; stop(); }", 1, 287, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationRates = 1e-8; stop(); }", 1, 279, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationRatesM = 1e-8; stop(); }", 1, 280, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.recombinationRatesF = 1e-8; stop(); }", 1, 280, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallMutationRate = 1e-2; stop(); }", 1, 280, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallMutationRateM = 1e-2; stop(); }", 1, 281, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.overallMutationRateF = 1e-2; stop(); }", 1, 281, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationEndPositions = 99999; stop(); }", 1, 281, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationEndPositionsM = 99999; stop(); }", 1, 282, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationEndPositionsF = 99999; stop(); }", 1, 282, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationRates = 1e-8; stop(); }", 1, 274, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationRatesM = 1e-8; stop(); }", 1, 275, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.mutationRatesF = 1e-8; stop(); }", 1, 275, "read-only property", __LINE__);
	
	std::string gen1_setup_sex_2rates("initialize() { initializeSex('X'); initializeMutationRate(1e-7, sex='M'); initializeMutationRate(1e-8, sex='F'); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8, 99999, 'M'); initializeRecombinationRate(1e-7, 99999, 'F'); } ");
	
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.colorSubstitution == '#3333FF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.geneConversionEnabled == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.geneConversionGCBias == 0.0) stop(); }", 1, 371, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.geneConversionNonCrossoverFraction == 0.0) stop(); }", 1, 371, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.geneConversionMeanLength == 0.0) stop(); }", 1, 371, "not defined since the DSB", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.geneConversionSimpleConversionFraction == 0.0) stop(); }", 1, 371, "not defined since the DSB", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.genomicElements[0].genomicElementType == g1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.lastPosition == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.overallRecombinationRate)) stop(); }", 1, 378, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.overallRecombinationRateM == 1e-8 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.overallRecombinationRateF == 1e-7 * 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationEndPositions)) stop(); }", 1, 378, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.recombinationEndPositionsM == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.recombinationEndPositionsF == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.recombinationRates)) stop(); }", 1, 378, "sex-specific recombination rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.recombinationRatesM == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.recombinationRatesF == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.overallMutationRate)) stop(); }", 1, 378, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.overallMutationRateM == 1e-7 * 100000) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.overallMutationRateF == 1e-8 * 100000) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.mutationEndPositions)) stop(); }", 1, 378, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.mutationEndPositionsM == 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.mutationEndPositionsF == 99999) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (isNULL(ch.mutationRates)) stop(); }", 1, 378, "sex-specific mutation rate maps", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.mutationRatesM == 1e-7) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; if (ch.mutationRatesF == 1e-8) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.colorSubstitution = ''; if (ch.colorSubstitution == '') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.colorSubstitution = 'red'; if (ch.colorSubstitution == 'red') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.colorSubstitution = '#FF0000'; if (ch.colorSubstitution == '#FF0000') stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.tag; }", 1, 367, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; c(ch,ch).tag; }", 1, 373, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.tag = 3294; if (ch.tag == 3294) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.genomicElements = ch.genomicElements; stop(); }", 1, 383, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.lastPosition = 99999; stop(); }", 1, 380, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallRecombinationRate = 1e-2; stop(); }", 1, 392, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallRecombinationRateM = 1e-2; stop(); }", 1, 393, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallRecombinationRateF = 1e-2; stop(); }", 1, 393, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationEndPositions = 99999; stop(); }", 1, 393, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationEndPositionsM = 99999; stop(); }", 1, 394, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationEndPositionsF = 99999; stop(); }", 1, 394, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationRates = 1e-8; stop(); }", 1, 386, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationRatesM = 1e-8; stop(); }", 1, 387, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.recombinationRatesF = 1e-8; stop(); }", 1, 387, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallMutationRate = 1e-2; stop(); }", 1, 387, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallMutationRateM = 1e-2; stop(); }", 1, 388, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.overallMutationRateF = 1e-2; stop(); }", 1, 388, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationEndPositions = 99999; stop(); }", 1, 388, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationEndPositionsM = 99999; stop(); }", 1, 389, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationEndPositionsF = 99999; stop(); }", 1, 389, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationRates = 1e-8; stop(); }", 1, 381, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationRatesM = 1e-8; stop(); }", 1, 382, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.mutationRatesF = 1e-8; stop(); }", 1, 382, "read-only property", __LINE__);
	
	// Test Chromosome - (void)setMutationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(); stop(); }", 1, 240, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(-0.00001); stop(); }", 1, 240, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(10000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1)); stop(); }", 1, 240, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0)); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 240, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 240, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 240, "must be >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0), '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 240, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 240, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 240, "must be >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(); stop(); }", 1, 260, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(-0.00001); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(10000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1)); stop(); }", 1, 260, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0)); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 260, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 260, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 260, "must be >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0), '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 260, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 260, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 260, "must be >= 0", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(0.0); stop(); }", 1, 367, "single map versus separate maps", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(); stop(); }", 1, 367, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(-0.00001); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(10000); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.001), c(1000, 99999), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(integer(0), integer(0), 'M'); stop(); }", 1, 367, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", 1, 367, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", 1, 367, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", 1, 367, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", 1, 367, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", 1, 367, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", 1, 367, "must be >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setMutationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", 1, 367, "must be >= 0", __LINE__);
	
	// Test Chromosome - (void)setRecombinationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(); stop(); }", 1, 240, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(-0.00001); stop(); }", 1, 240, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.5); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.6); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(10000); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 240, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 240, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 240, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 240, "rates must be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.0); stop(); }", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(); stop(); }", 1, 260, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(-0.00001); stop(); }", 1, 260, "out of range", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.5); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.6); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(10000); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 260, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 260, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 260, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 260, "rates must be in [0.0, 0.5]", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", 1, 260, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.0); stop(); }", 1, 367, "single map versus separate maps", __LINE__);														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(); stop(); }", 1, 367, "missing required argument", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(-0.00001); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(10000); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), '*'); stop(); }", 1, 367, "single map versus separate maps", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.001), c(1000, 99999), 'M'); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0), 'M'); stop(); }", 1, 367, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999, 'M'); stop(); }", 1, 367, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999, 'M'); stop(); }", 1, 367, "to be of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000), 'M'); stop(); }", 1, 367, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999), 'M'); stop(); }", 1, 367, "ascending order", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999), 'M'); stop(); }", 1, 367, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000), 'M'); stop(); }", 1, 367, "rates must be in [0.0, 0.5]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_2rates + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000), 'M'); stop(); }", 1, 367, "rates must be in [0.0, 0.5]", __LINE__);
	
	// initializeGeneConversion() tests
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 { if (sim.chromosome.geneConversionEnabled == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 { if (sim.chromosome.geneConversionNonCrossoverFraction == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 { if (sim.chromosome.geneConversionMeanLength == 1234.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 { if (sim.chromosome.geneConversionSimpleConversionFraction == 0.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75); } 1 { if (sim.chromosome.geneConversionGCBias == 0.0) stop(); }", __LINE__);
	
	// setGeneConversion() tests
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosome.geneConversionEnabled == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosome.geneConversionNonCrossoverFraction == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosome.geneConversionMeanLength == 1234.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosome.geneConversionSimpleConversionFraction == 0.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75); if (sim.chromosome.geneConversionGCBias == 0.0) stop(); }", __LINE__);
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
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.tag; }", 1, 276, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; c(mut,mut).tag; }", 1, 283, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.tag = 278; if (mut.tag == 278) stop(); }", __LINE__);
	
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
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.tag; }", 1, 272, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; c(gen,gen).tag; }", 1, 279, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.tag = 278; if (gen.tag == 278) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.genomeType = 'A'; stop(); }", 1, 283, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.isNullGenome = F; stop(); }", 1, 285, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.mutations[0].mutationType = m1; stop(); }", 1, 299, "read-only property", __LINE__);
	
	// Test Genome + (void)addMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.addMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.addMutations(gen.mutations[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.addMutations(p1.genomes[1].mutations[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = p1.genomes[1].mutations[0]; gen.addMutations(rep(mut, 10)); if (sum(gen.mutations == mut) == 1) stop(); }", __LINE__);
	
	// Test Genome + (object<Mutation>)addNewDrawnMutation(io<MutationType> mutationType, integer position, [Ni originGeneration], [Nio<Subpopulation> originSubpop])
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
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL, NULL); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000:5003, 10, p1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000:5003, 10:13, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000:5003, 10, 0:3); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000:5003); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(7, 5000, NULL, 1); stop(); }", 1, 278, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, 0, 1); stop(); }", 1, 278, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, -1, NULL, 1); stop(); }", 1, 278, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 100000, NULL, 1); stop(); }", 1, 278, "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL, 237); stop(); }", __LINE__);											// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 5000, NULL, -1); stop(); }", 1, 278, "out of range", __LINE__);					// however, such tags must be within range
	
	// Test Genome + (object<Mutation>)addNewMutation(io<MutationType> mutationType, numeric selectionCoeff, integer position, [Ni originGeneration], [Nio<Subpopulation> originSubpop])
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
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, NULL, NULL); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000:5003, 10, p1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000:5003, 10:13, 1); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000:5003, 10, 0:3); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000:5003); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, (0:3)/10, 5000:5003); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, (0:3)/10, 5000:5003, 10, 0:3); p1.genomes.addMutations(mut); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(7, 0.1, 5000, NULL, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, 0, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, -1, NULL, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 100000, NULL, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, NULL, 237); p1.genomes.addMutations(mut); stop(); }", __LINE__);							// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0.1, 5000, NULL, -1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "out of range", __LINE__);	// however, such tags must be within range
	
	// Test Genome + (object<Mutation>)addNewDrawnMutation(io<MutationType> mutationType, integer position, [Ni originGeneration], [io<Subpopulation> originSubpop]) with new class method non-multiplex behavior
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
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, NULL, NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000:5003, 10, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000:5003, 10:13, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000:5003, 10, 0:3); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(m1, 5000:5003); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(7, 5000, NULL, 1); stop(); }", 1, 258, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, 0, 1); stop(); }", 1, 258, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, -1, NULL, 1); stop(); }", 1, 258, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 100000, NULL, 1); stop(); }", 1, 258, "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, NULL, 237); stop(); }", __LINE__);											// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewDrawnMutation(1, 5000, NULL, -1); stop(); }", 1, 258, "out of range", __LINE__);					// however, such tags must be within range
	
	// Test Genome + (object<Mutation>)addNewMutation(io<MutationType> mutationType, numeric selectionCoeff, integer position, [Ni originGeneration], [io<Subpopulation> originSubpop]) with new class method non-multiplex behavior
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
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, NULL, NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000:5003, 10, p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000:5003, 10:13, 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000:5003, 10, 0:3); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, 0.1, 5000:5003); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, (0:3)/10, 5000:5003); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(m1, (0:3)/10, 5000:5003, 10, 0:3); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(7, 0.1, 5000, NULL, 1); stop(); }", 1, 258, "not defined", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, 0, 1); stop(); }", 1, 258, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, -1, NULL, 1); stop(); }", 1, 258, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 100000, NULL, 1); stop(); }", 1, 258, "past the end", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, NULL, 237); stop(); }", __LINE__);							// bad subpop, but this is legal to allow "tagging" of mutations
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes.addNewMutation(1, 0.1, 5000, NULL, -1); stop(); }", 1, 258, "out of range", __LINE__);	// however, such tags must be within range
	
	// Test Genome - (logical$)containsMarkerMutation(io<MutationType>$ mutType, integer$ position, [logical$ returnMutation = F])
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, 1000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(1, 1000); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0:1].containsMarkerMutation(1, 1000); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, -1); stop(); }", 1, 262, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, 1000000); stop(); }", 1, 262, "past the end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(10, 1000); stop(); }", 1, 262, "mutation type m10 not defined", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, 1000, returnMutation=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(1, 1000, returnMutation=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0:1].containsMarkerMutation(1, 1000, returnMutation=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, -1, returnMutation=T); stop(); }", 1, 262, "out of range", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(m1, 1000000, returnMutation=T); stop(); }", 1, 262, "past the end", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "10 { p1.genomes[0].containsMarkerMutation(10, 1000, returnMutation=T); stop(); }", 1, 262, "mutation type m10 not defined", __LINE__);
	
	// Test Genome - (logical)containsMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].containsMutations(sim.mutations); stop(); }", __LINE__);
	
	// Test Genome - (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].countOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].countOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0:1].countOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Genome - (integer$)positionsOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].positionsOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].positionsOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0:1].positionsOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Genome - (float$)sumOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].sumOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0].sumOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { p1.genomes[0:1].sumOfMutationsOfType(1); stop(); }", __LINE__);
	
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
	
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(); gen.removeMutations(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.removeMutations(NULL); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.1, 5000); gen.removeMutations(NULL, T); }", 1, 313, "substitute may not be T if", __LINE__);
	
	// Test Genome + (void)outputMS([Ns$ filePath], [logical$ append = F], [logical$ filterMonomorphic = F])
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
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.tag; }", 1, 250, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { c(p1,p1).tag; }", 1, 256, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.tag = 135; if (p1.tag == 135) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.fitnessScaling = 135.0; if (p1.fitnessScaling == 135.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.fitnessScaling = 0.0; if (p1.fitnessScaling == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.fitnessScaling = -0.01; }", 1, 265, "must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.fitnessScaling = NAN; }", 1, 265, "must be >= 0.0", __LINE__);

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
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.tag; }", 1, 270, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { c(p1,p1).tag; }", 1, 276, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.tag = 135; if (p1.tag == 135) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.fitnessScaling = 135.0; if (p1.fitnessScaling == 135.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.fitnessScaling = 0.0; if (p1.fitnessScaling == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.fitnessScaling = -0.01; }", 1, 285, "must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.fitnessScaling = NAN; }", 1, 285, "must be >= 0.0", __LINE__);
	
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
	
	// Test Subpopulation - (float)cachedFitness(Ni indices)
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
	
	// Test Subpopulation – (object<Individual>)sampleIndividuals(integer$ size, [logical$ replace = F], [No<Individual>$ exclude = NULL], [Ns$ sex = NULL], [Ni$ tag = NULL], [Ni$ minAge = NULL], [Ni$ maxAge = NULL], [Nl$ migrant = NULL])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(0)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(2)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(4)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2])) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2])) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2])) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2])) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2])) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2])) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2])) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2])) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.sampleIndividuals(-1); }", 1, 250, "requires a sample size", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.sampleIndividuals(15, replace=F); if (size(i) == 10) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.sampleIndividuals(1, sex='M'); }", 1, 250, "in non-sexual models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.sampleIndividuals(1, sex='W'); }", 1, 270, "unrecognized value for sex", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (p1.sampleIndividuals(3, replace=T, exclude=p1.individuals[5], sex='M', tag=1).size() == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (p1.sampleIndividuals(3, replace=F, exclude=p1.individuals[5], sex='M', tag=1).size() == 2) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(0, tag=1).tag, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(1, tag=1).tag, c(1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(2, tag=1).tag, c(1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(4, tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(0, exclude=p1.individuals[2], tag=1).tag, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(1, exclude=p1.individuals[2], tag=1).tag, c(1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(2, exclude=p1.individuals[2], tag=1).tag, c(1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(4, exclude=p1.individuals[2], tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(0, replace=T, tag=1).tag, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(1, replace=T, tag=1).tag, c(1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(2, replace=T, tag=1).tag, c(1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(4, replace=T, tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2], tag=1).tag, integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2], tag=1).tag, c(1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2], tag=1).tag, c(1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2], tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2])) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2])) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2])) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2])) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2])) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2])) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2])) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2])) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, sex='M')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, sex='M')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, sex='M')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, sex='M')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2], sex='M')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2], sex='M')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2], sex='M')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2], sex='M')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, sex='M')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, sex='M')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, sex='M')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, sex='M')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2], sex='M')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2], sex='M')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2], sex='M')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2], sex='M')) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, sex='F')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, sex='F')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, sex='F')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, sex='F')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2], sex='F')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2], sex='F')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2], sex='F')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2], sex='F')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, sex='F')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, sex='F')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, sex='F')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, sex='F')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2], sex='F')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2], sex='F')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2], sex='F')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2], sex='F')) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, migrant=F)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, migrant=F)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, migrant=F)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, migrant=F)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, exclude=p1.individuals[2], migrant=F)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2], migrant=F)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, exclude=p1.individuals[2], migrant=F)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, exclude=p1.individuals[2], migrant=F)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, migrant=F)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, migrant=F)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, migrant=F)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, migrant=F)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, replace=T, exclude=p1.individuals[2], migrant=F)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, replace=T, exclude=p1.individuals[2], migrant=F)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(2, replace=T, exclude=p1.individuals[2], migrant=F)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(4, replace=T, exclude=p1.individuals[2], migrant=F)) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(0, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.sampleIndividuals(1, exclude=p1.individuals[2], migrant=T)) == 0) stop(); }", __LINE__);
	
	// Test Subpopulation – (object<Individual>)subsetIndividuals([No<Individual>$ exclude = NULL], [Ns$ sex = NULL], [Ni$ tag = NULL], [Ni$ minAge = NULL], [Ni$ maxAge = NULL], [Nl$ migrant = NULL])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.subsetIndividuals()) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.subsetIndividuals(exclude=p1.individuals[2])) == 9) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.subsetIndividuals(sex='M'); }", 1, 250, "in non-sexual models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.subsetIndividuals(sex='W'); }", 1, 270, "unrecognized value for sex", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.subsetIndividuals(tag=1).tag, c(1,1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (identical(p1.subsetIndividuals(exclude=p1.individuals[3], tag=1).tag, c(1,1,1,1))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals()) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(exclude=p1.individuals[2])) == 9) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(sex='M')) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(exclude=p1.individuals[2], sex='M')) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(sex='F')) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(exclude=p1.individuals[2], sex='F')) == 4) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(migrant=F)) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(exclude=p1.individuals[2], migrant=F)) == 9) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.subsetIndividuals(exclude=p1.individuals[2], migrant=T)) == 0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(sex='F', tag=1)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], sex='F', tag=1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(sex='F', tag=0)) == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], sex='F', tag=0)) == 3) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(tag=1, migrant=F)) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], tag=1, migrant=F)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(tag=0, migrant=F)) == 5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], tag=0, migrant=F)) == 5) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(tag=1, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], tag=1, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(tag=0, migrant=T)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.individuals.tag = rep(c(0,1),5); if (size(p1.subsetIndividuals(exclude=p1.individuals[3], tag=0, migrant=T)) == 0) stop(); }", __LINE__);
	
	// Test Subpopulation - (void)outputMSSample(integer$ sampleSize, [logical$ replace], [string$ requestedSex], [Ns$ filePath = NULL], [logical$ append = F], [logical$ filterMonomorphic = F])
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
	
	// Test Subpopulation SLiMEidosDictionary functionality: - (+)getValue(string$ key) and - (void)setValue(string$ key, + value)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setValue('foo', 7:9); p1.setValue('bar', 'baz'); } 10 { if (identical(p1.getValue('foo'), 7:9) & identical(p1.getValue('bar'), 'baz')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setValue('foo', 3:5); p1.setValue('foo', 'foobar'); } 10 { if (identical(p1.getValue('foo'), 'foobar')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setValue('foo', 3:5); p1.setValue('foo', NULL); } 10 { if (isNULL(p1.getValue('foo'))) stop(); }", __LINE__);
	
	// Test spatial stuff including spatialBounds, setSpatialBounds(), pointInBounds(), pointPeriodic(), pointReflected(), pointStopped(), pointUniform()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.spatialBounds, float(0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.spatialBounds = 0.0; stop(); }", 1, 264, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSpatialBounds(-2.0); stop(); }", 1, 250, "setSpatialBounds() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.pointInBounds(-2.0); stop(); }", 1, 250, "pointInBounds() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.pointPeriodic(-2.0); stop(); }", 1, 250, "pointPeriodic() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.pointReflected(-2.0); stop(); }", 1, 250, "pointReflected() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.pointStopped(-2.0); stop(); }", 1, 250, "pointStopped() cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.pointUniform(); stop(); }", 1, 250, "pointUniform() cannot be called in non-spatial simulations", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointInBounds(float(0)), logical(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointReflected(float(0)), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointStopped(float(0)), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 7.5)); if (identical(p1.pointPeriodic(float(0)), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointUniform(0), float(0))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (identical(p1.spatialBounds, c(0.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.spatialBounds, c(-2.0, 7.5))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.setSpatialBounds(-2.0); stop(); }", 1, 424, "requires twice as many coordinates", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(0.0, 0.0, 1.0, 1.0)); stop(); }", 1, 424, "requires twice as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(-2.1) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(-2.0) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(0.0) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(7.5) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(7.6) == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (p1.pointInBounds(11.0, 0.0) == F) stop(); }", 1, 463, "too many arguments supplied", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-2.0, 7.5)); if (identical(p1.pointInBounds(c(11.0, 0.0)), c(F,T))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(-15.5) == -0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(-5.5) == -4.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(-5.0) == -5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(2.0) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(2.5) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(3.5) == 1.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(11.0) == -4.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointReflected(11.0, 0.0) == -4.0) stop(); }", 1, 463, "too many arguments supplied", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (identical(p1.pointReflected(c(-15.5, -5.5, 2.0, 3.5)), c(-0.5, -4.5, 2.0, 1.5))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(-15.5) == -5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(-5.5) == -5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(-5.0) == -5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(2.0) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(2.5) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(3.5) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(11.0) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointStopped(11.0, 0.0) == -4.0) stop(); }", 1, 463, "too many arguments supplied", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (identical(p1.pointStopped(c(-15.5, -5.5, 2.0, 3.5)), c(-5.0, -5.0, 2.0, 2.5))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (size(p1.pointUniform()) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (size(p1.pointUniform(1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (size(p1.pointUniform(5)) == 5) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointPeriodic(-15.5) == -0.5) stop(); }", 1, 463, "no periodic spatial dimension", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(-5.0, 2.5)); if (p1.pointPeriodic(-15.5) == -0.5) stop(); }", 1, 441, "requires min coordinates to be 0.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(-0.5) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(-5.5) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(2.0) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(2.5) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(3.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(11.0) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (p1.pointPeriodic(11.0, 0.0) == -4.0) stop(); }", 1, 479, "too many arguments supplied", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xPx + "1 { p1.setSpatialBounds(c(0.0, 2.5)); if (identical(p1.pointPeriodic(c(-0.5, -5.5, 0.0, 2.5, 3.5)), c(2.0, 2.0, 0.0, 2.5, 1.0))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { if (identical(p1.spatialBounds, c(0.0, 0.0, 0.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.setSpatialBounds(c(-2.0, -100, 10.0, 7.5, -99.5, 12.0)); if (identical(p1.spatialBounds, c(-2.0, -100, 10.0, 7.5, -99.5, 12.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.setSpatialBounds(-2.0); stop(); }", 1, 488, "requires twice as many coordinates", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.setSpatialBounds(c(0.0, 0.0, 1.0, 1.0)); stop(); }", 1, 488, "requires twice as many coordinates", __LINE__);
	
	std::string gen1_setup_i1xyz_bounds(gen1_setup_i1xyz + "1 { p1.setSpatialBounds(c(-10.0, 0.0, 10.0,    -9.0, 2.0, 13.0)); ");
	std::string gen1_setup_i1xyzPxz_bounds(gen1_setup_i1xyzPxz + "1 { p1.setSpatialBounds(c(0.0, 0.0, 0.0,    9.0, 2.0, 13.0)); ");
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-10.1, 1.0, 11.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 11.0)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-8.0, 1.0, 11.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, -1.0, 11.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 11.0)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 3.0, 11.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 9.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 11.0)) == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(-9.5, 1.0, 14.0)) == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(11.0, 0.0) == F) stop(); }", 1, 554, "too many arguments supplied", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointInBounds(c(11.0, 0.0)) == F) stop(); }", 1, 554, "requires the length of point", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointInBounds(c(-10.1, 1.0, 11.0, -9.5, 1.0, 11.0)), c(F, T))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-10.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-8.0, 1.0, 11.0)), c(-10.0, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, -1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, 2.5, 11.0)), c(-9.5, 1.5, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, 1.0, 4.5)), c(-9.5, 1.0, 10.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-9.5, 1.0, 14.5)), c(-9.5, 1.0, 11.5))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointReflected(11.0, 0.0) == -4.0) stop(); }", 1, 554, "too many arguments supplied", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointReflected(c(11.0, 0.0)) == -4.0) stop(); }", 1, 554, "requires the length of point", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointReflected(c(-10.5, -1.0, 4.5, -8.0, 2.5, 14.5)), c(-9.5, 1.0, 10.5, -10.0, 1.5, 11.5))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-10.5, 1.0, 11.0)), c(-10.0, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-8.0, 1.0, 11.0)), c(-9.0, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, -1.0, 11.0)), c(-9.5, 0.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, 2.5, 11.0)), c(-9.5, 2.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, 1.0, 4.5)), c(-9.5, 1.0, 10.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, 1.0, 11.0)), c(-9.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-9.5, 1.0, 14.5)), c(-9.5, 1.0, 13.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointStopped(11.0, 0.0) == -4.0) stop(); }", 1, 554, "too many arguments supplied", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_bounds + "if (p1.pointStopped(c(11.0, 0.0)) == -4.0) stop(); }", 1, 554, "requires the length of point", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (identical(p1.pointStopped(c(-10.5, -1.0, 4.5, -8.0, 2.5, 14.5)), c(-10.0, 0.0, 10.0, -9.0, 2.0, 13.0))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (size(p1.pointUniform()) == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (size(p1.pointUniform(1)) == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_bounds + "if (size(p1.pointUniform(5)) == 15) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-10.5, 1.0, 11.0)), c(7.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, 1.0, 11.0)), c(8.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-8.0, 1.0, 11.0)), c(1.0, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, -1.0, 11.0)), c(8.5, -1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, 1.0, 11.0)), c(8.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, 2.5, 11.0)), c(8.5, 2.5, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, 1.0, 4.5)), c(8.5, 1.0, 4.5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, 1.0, 11.0)), c(8.5, 1.0, 11.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-9.5, 1.0, 14.5)), c(8.5, 1.0, 1.5))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyzPxz_bounds + "if (p1.pointPeriodic(11.0, 0.0) == -4.0) stop(); }", 1, 568, "too many arguments supplied", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyzPxz_bounds + "if (p1.pointPeriodic(c(11.0, 0.0)) == -4.0) stop(); }", 1, 568, "requires the length of point", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyzPxz_bounds + "if (identical(p1.pointPeriodic(c(-10.5, -1.0, 4.5, -8.0, 2.5, 14.5)), c(7.5, -1.0, 4.5, 1.0, 2.5, 1.5))) stop(); }", __LINE__);
	
	// Test spatial stuff including defineSpatialMap(), spatialMapColor(), and spatialMapValue()
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.defineSpatialMap('map', '', integer(0), float(0)); stop(); }", 1, 250, "spatiality \"\" must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.defineSpatialMap('map', 'x', 2, c(0.0, 1.0)); stop(); }", 1, 250, "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.spatialMapColor('m', 0.5); stop(); }", 1, 250, "could not find map", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.spatialMapValue('m', float(0)); stop(); }", 1, 250, "could not find map", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.spatialMapValue('m', 0.0); stop(); }", 1, 250, "could not find map", __LINE__);
	
	// a few tests supplying a matrix/array spatial map instead of a vector; no need to test spatialMapValue() etc. with these,
	// since it all funnels into the same map definition code anyway, so we just need to be sure the pre-funnel code is good...
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(2,2), matrix(1.0:4, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', NULL, matrix(1.0:4, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(3,3), matrix(1.0:9, nrow=3)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', NULL, matrix(1.0:9, nrow=3)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(2,3), matrix(1.0:6, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', NULL, matrix(1.0:6, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(6), matrix(1.0:6, nrow=2)); stop(); }", 1, 488, "gridSize must match the spatiality", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(3,2), matrix(1.0:6, nrow=2)); stop(); }", 1, 488, "gridSize does not match dimensions", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(2,2), matrix(1.0:6, nrow=2)); stop(); }", 1, 488, "gridSize does not match dimensions", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xy', c(3,3), matrix(1.0:6, nrow=2)); stop(); }", 1, 488, "gridSize does not match dimensions", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', c(2,2), matrix(1.0:4, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', NULL, matrix(1.0:4, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', c(3,3), matrix(1.0:9, nrow=3)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', NULL, matrix(1.0:9, nrow=3)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', c(2,3), matrix(1.0:6, nrow=2)); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', NULL, matrix(1.0:6, nrow=2)); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', c(2,2,2), array(1.0:8, c(2,2,2))); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', NULL, array(1.0:8, c(2,2,2))); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', c(3,3,3), array(1.0:27, c(3,3,3))); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', NULL, array(1.0:27, c(3,3,3))); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', c(2,3,2), array(1.0:12, c(2,3,2))); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', NULL, array(1.0:12, c(2,3,2))); stop(); }", __LINE__);
	
	// 1D sim with 1D x map
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.defineSpatialMap('map', '', integer(0), float(0)); stop(); }", 1, 424, "spatiality \"\" must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.defineSpatialMap('map', 'xy', 2, c(0.0, 1.0)); stop(); }", 1, 424, "spatial dimensions beyond those set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.defineSpatialMap('map', 'x', 1, 0.0); stop(); }", 1, 424, "elements of gridSize must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { p1.defineSpatialMap('map', 'x', 2, 0.0); stop(); }", 1, 424, "does not match the product of the sizes", __LINE__);
	
	std::string gen1_setup_i1x_mapNI(gen1_setup_i1x + "1 { p1.defineSpatialMap('map', 'x', 3, c(0.0, 1.0, 3.0), interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', -9.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 0.2) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 0.3) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 0.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 0.7) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 0.8) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 1.0) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapValue('map', 9.0) == 3.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapColor('map', -5.0) == '#000000') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapColor('map', -2.5) == '#404040') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapColor('map', 0.0001) == '#808080') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapColor('map', 2.5) == '#BFBFBF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapNI + "if (p1.spatialMapColor('map', 5.0) == '#FFFFFF') stop(); }", __LINE__);
	
	std::string gen1_setup_i1x_mapI(gen1_setup_i1x + "1 { p1.defineSpatialMap('map', 'x', 3, c(0.0, 1.0, 3.0), interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', -9.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', 0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', 0.25) == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', 0.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', 0.75) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', 1.0) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapValue('map', 9.0) == 3.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapColor('map', -5.0) == '#FF003F') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapColor('map', -2.5) == '#804020') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapColor('map', 0.0001) == '#007F00') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapColor('map', 2.5) == '#00BF80') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x_mapI + "if (p1.spatialMapColor('map', 5.0) == '#00FFFF') stop(); }", __LINE__);
	
	// 3D sim with 1D x map
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', '', integer(0), float(0)); stop(); }", 1, 488, "spatiality \"\" must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'x', 1, 0.0); stop(); }", 1, 488, "elements of gridSize must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'x', 2, 0.0); stop(); }", 1, 488, "does not match the product of the sizes", __LINE__);
	
	std::string gen1_setup_i1xyz_mapNIx(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'x', 3, c(0.0, 1.0, 3.0), interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', -9.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 0.2) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 0.3) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 0.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 0.7) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 0.8) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 1.0) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapValue('map', 9.0) == 3.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapColor('map', -5.0) == '#000000') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapColor('map', -2.5) == '#404040') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapColor('map', 0.0001) == '#808080') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapColor('map', 2.5) == '#BFBFBF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIx + "if (p1.spatialMapColor('map', 5.0) == '#FFFFFF') stop(); }", __LINE__);
	
	std::string gen1_setup_i1xyz_mapIx(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'x', 3, c(0.0, 1.0, 3.0), interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', -9.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', 0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', 0.25) == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', 0.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', 0.75) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', 1.0) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapValue('map', 9.0) == 3.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapColor('map', -5.0) == '#FF003F') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapColor('map', -2.5) == '#804020') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapColor('map', 0.0001) == '#007F00') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapColor('map', 2.5) == '#00BF80') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIx + "if (p1.spatialMapColor('map', 5.0) == '#00FFFF') stop(); }", __LINE__);
	
	// 3D sim with 1D z map
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', '', integer(0), float(0)); stop(); }", 1, 488, "spatiality \"\" must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'z', 1, 0.0); stop(); }", 1, 488, "elements of gridSize must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'z', 2, 0.0); stop(); }", 1, 488, "does not match the product of the sizes", __LINE__);
	
	std::string gen1_setup_i1xyz_mapNIz(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'z', 3, c(0.0, 1.0, 3.0), interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', -9.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 0.2) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 0.3) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 0.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 0.7) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 0.8) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 1.0) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapValue('map', 9.0) == 3.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapColor('map', -5.0) == '#000000') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapColor('map', -2.5) == '#404040') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapColor('map', 0.0001) == '#808080') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapColor('map', 2.5) == '#BFBFBF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIz + "if (p1.spatialMapColor('map', 5.0) == '#FFFFFF') stop(); }", __LINE__);
	
	std::string gen1_setup_i1xyz_mapIz(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'z', 3, c(0.0, 1.0, 3.0), interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', -9.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', 0.0) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', 0.25) == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', 0.5) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', 0.75) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', 1.0) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapValue('map', 9.0) == 3.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapColor('map', -5.0) == '#FF003F') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapColor('map', -2.5) == '#804020') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapColor('map', 0.0001) == '#007F00') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapColor('map', 2.5) == '#00BF80') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIz + "if (p1.spatialMapColor('map', 5.0) == '#00FFFF') stop(); }", __LINE__);
	
	// 3D sim with 2D xz map
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', '', integer(0), float(0)); stop(); }", 1, 488, "spatiality \"\" must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', 1, 0.0); stop(); }", 1, 488, "gridSize must match the spatiality", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', c(2,2), 0.0); stop(); }", 1, 488, "does not match the product of the sizes", __LINE__);
	
	std::string gen1_setup_i1xyz_mapNIxz(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', c(3,2), c(0.0, 1, 3, 5, 5, 5), interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_mapNIxz + "p1.spatialMapValue('map', 0.0); stop(); }", 1, 621, "must match spatiality of map", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(-9.0, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.0, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.2, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.3, 0.0)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.5, 0.0)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.7, 0.0)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.8, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(1.0, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(9.0, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(-9.0, 0.2)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.0, 0.2)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.2, 0.2)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.3, 0.2)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.5, 0.2)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.7, 0.2)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.8, 0.2)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(1.0, 0.2)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(9.0, 0.2)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(-9.0, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.0, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.2, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.3, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.5, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.7, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.8, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(1.0, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(9.0, 0.8)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(-9.0, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.0, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.2, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.3, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.5, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.7, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(0.8, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(1.0, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapValue('map', c(9.0, 1.0)) == 5.0) stop(); }", __LINE__);

	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapColor('map', -5.0) == '#000000') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapColor('map', -2.5) == '#404040') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapColor('map', 0.0001) == '#808080') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapColor('map', 2.5) == '#BFBFBF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxz + "if (p1.spatialMapColor('map', 5.0) == '#FFFFFF') stop(); }", __LINE__);
	
	std::string gen1_setup_i1xyz_mapIxz(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xz', c(3,2), c(0.0, 1, 3, 5, 5, 5), interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_mapIxz + "p1.spatialMapValue('map', 0.0); stop(); }", 1, 636, "must match spatiality of map", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(-9.0, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.0, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.25, 0.0)) == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.5, 0.0)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.75, 0.0)) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(1.0, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(9.0, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(-9.0, 0.5)) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.0, 0.5)) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.25, 0.5)) == 2.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.5, 0.5)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.75, 0.5)) == 3.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(1.0, 0.5)) == 4.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(9.0, 0.5)) == 4.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(-9.0, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.0, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.25, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.5, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(0.75, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(1.0, 1.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapValue('map', c(9.0, 1.0)) == 5.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapColor('map', -5.0) == '#FF003F') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapColor('map', -2.5) == '#804020') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapColor('map', 0.0001) == '#007F00') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapColor('map', 2.5) == '#00BF80') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxz + "if (p1.spatialMapColor('map', 5.0) == '#00FFFF') stop(); }", __LINE__);
	
	// 3D sim with 3D xyz map
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', '', integer(0), float(0)); stop(); }", 1, 488, "spatiality \"\" must be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', 1, 0.0); stop(); }", 1, 488, "gridSize must match the spatiality", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', c(2,2,2), 0.0); stop(); }", 1, 488, "does not match the product of the sizes", __LINE__);
	
	std::string gen1_setup_i1xyz_mapNIxyz(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', c(3,2,2), 0.0:11.0, interpolate=F, valueRange=c(-5.0, 5.0), colors=c('black', 'white')); ");
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_mapNIxyz + "p1.spatialMapValue('map', 0.0); stop(); }", 1, 611, "must match spatiality of map", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.0, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.0, 0.0)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.0, 0.0)) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.8, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.8, 0.0)) == 4.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.8, 0.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 1.0, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 1.0, 0.0)) == 4.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 1.0, 0.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.0, 0.6)) == 6.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.0, 0.6)) == 7.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.0, 0.6)) == 8.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.2, 0.6)) == 6.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.2, 0.6)) == 7.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.2, 0.6)) == 8.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 1.0, 0.6)) == 9.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 1.0, 0.6)) == 10.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 1.0, 0.6)) == 11.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.0, 1.0)) == 6.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.0, 1.0)) == 7.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.0, 1.0)) == 8.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.2, 1.0)) == 6.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.2, 1.0)) == 7.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.2, 1.0)) == 8.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.0, 1.0, 1.0)) == 9.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(0.5, 1.0, 1.0)) == 10.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapValue('map', c(1.0, 1.0, 1.0)) == 11.0) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapColor('map', -5.0) == '#000000') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapColor('map', -2.5) == '#404040') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapColor('map', 0.0001) == '#808080') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapColor('map', 2.5) == '#BFBFBF') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapNIxyz + "if (p1.spatialMapColor('map', 5.0) == '#FFFFFF') stop(); }", __LINE__);
	
	std::string gen1_setup_i1xyz_mapIxyz(gen1_setup_i1xyz + "1 { p1.defineSpatialMap('map', 'xyz', c(3,2,2), 0.0:11.0, interpolate=T, valueRange=c(-5.0, 5.0), colors=c('#FF003F', '#007F00', '#00FFFF')); ");
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_mapIxyz + "p1.spatialMapValue('map', 0.0); stop(); }", 1, 626, "must match spatiality of map", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.0, 0.0)) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.0, 0.0)) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.0, 0.0)) == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.5, 0.0)) == 1.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.5, 0.0)) == 2.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.5, 0.0)) == 3.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 1.0, 0.0)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 1.0, 0.0)) == 4.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 1.0, 0.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.0, 0.5)) == 3.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.0, 0.5)) == 4.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.0, 0.5)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.5, 0.5)) == 4.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.5, 0.5)) == 5.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.5, 0.5)) == 6.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 1.0, 0.5)) == 6.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 1.0, 0.5)) == 7.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 1.0, 0.5)) == 8.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.0, 1.0)) == 6.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.0, 1.0)) == 7.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.0, 1.0)) == 8.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 0.5, 1.0)) == 7.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 0.5, 1.0)) == 8.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 0.5, 1.0)) == 9.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.0, 1.0, 1.0)) == 9.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(0.5, 1.0, 1.0)) == 10.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapValue('map', c(1.0, 1.0, 1.0)) == 11.0) stop(); }", __LINE__);

	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapColor('map', -5.0) == '#FF003F') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapColor('map', -2.5) == '#804020') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapColor('map', 0.0001) == '#007F00') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapColor('map', 2.5) == '#00BF80') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_mapIxyz + "if (p1.spatialMapColor('map', 5.0) == '#00FFFF') stop(); }", __LINE__);
}

#pragma mark Individual tests
void _RunIndividualTests(void)
{	
	// ************************************************************************************
	//
	//	Gen 1+ tests: Individual
	//
	
	// Test Individual properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (all(i.color == '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (size(i.genome1) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (size(i.genome2) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (size(i.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (identical(i.genome1, i.genomes[0:9 * 2])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (identical(i.genome2, i.genomes[0:9 * 2 + 1])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (all(i.index == (0:9))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (all(i.subpopulation == rep(p1, 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (all(i.sex == rep('H', 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.color = 'red'; if (all(i.color == 'red')) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i[0].tag; }", 1, 272, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.tag; }", 1, 269, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tag = 135; if (all(i.tag == 135)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i[0].tagF; }", 1, 272, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.tagF; }", 1, 269, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tagF = 135.0; if (all(i.tagF == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (size(i.migrant) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; if (all(i.migrant == F)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = 135.0; if (all(i.fitnessScaling == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = 0.0; if (all(i.fitnessScaling == 0.0)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = -0.01; }", 1, 284, "must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = NAN; }", 1, 284, "must be >= 0.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.x = 135.0; if (all(i.x == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.y = 135.0; if (all(i.y == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.z = 135.0; if (all(i.z == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.uniqueMutations; stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.genome1 = i[0].genomes[0]; stop(); }", 1, 277, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.genome2 = i[0].genomes[0]; stop(); }", 1, 277, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.genomes = i[0].genomes[0]; stop(); }", 1, 277, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.index = i[0].index; stop(); }", 1, 275, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.subpopulation = i[0].subpopulation; stop(); }", 1, 283, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.sex = i[0].sex; stop(); }", 1, 273, "read-only property", __LINE__);
	//SLiMAssertScriptRaise(gen1_setup_p1 + "10 { i = p1.individuals; i.uniqueMutations = sim.mutations[0]; stop(); }", 1, 287, "read-only property", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (all(i.color == '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (size(i.genome1) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (size(i.genome2) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (size(i.genomes) == 20) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (identical(i.genome1, i.genomes[0:9 * 2])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (identical(i.genome2, i.genomes[0:9 * 2 + 1])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (all(i.index == (0:9))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (all(i.subpopulation == rep(p1, 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (all(i.sex == repEach(c('F','M'), 5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.color = 'red'; if (all(i.color == 'red')) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i[0].tag; }", 1, 292, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.tag; }", 1, 289, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.tag = 135; if (all(i.tag == 135)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i[0].tagF; }", 1, 292, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.tagF; }", 1, 289, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.tagF = 135.0; if (all(i.tagF == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (size(i.migrant) == 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; if (all(i.migrant == F)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.fitnessScaling = 135.0; if (all(i.fitnessScaling == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.fitnessScaling = 0.0; if (all(i.fitnessScaling == 0.0)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.fitnessScaling = -0.01; }", 1, 304, "must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.fitnessScaling = NAN; }", 1, 304, "must be >= 0.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.x = 135.0; if (all(i.x == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.y = 135.0; if (all(i.y == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.z = 135.0; if (all(i.z == 135.0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "10 { i = p1.individuals; i.uniqueMutations; stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.genome1 = i[0].genomes[0]; stop(); }", 1, 297, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.genome2 = i[0].genomes[0]; stop(); }", 1, 297, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.genomes = i[0].genomes[0]; stop(); }", 1, 297, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.index = i[0].index; stop(); }", 1, 295, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.subpopulation = i[0].subpopulation; stop(); }", 1, 303, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { i = p1.individuals; i.sex = i[0].sex; stop(); }", 1, 293, "read-only property", __LINE__);
	//SLiMAssertScriptRaise(gen1_setup_sex_p1 + "10 { i = p1.individuals; i.uniqueMutations = sim.mutations[0]; stop(); }", 1, 307, "read-only property", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.x = 0.5; if (identical(i.spatialPosition, rep(0.5, 10))) stop(); }", 1, 294, "position cannot be accessed", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { i = p1.individuals; i.x = 0.5; if (identical(i.spatialPosition, rep(0.5, 10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { i = p1.individuals; i.x = 0.5; i.y = 0.6; i.z = 0.7; if (identical(i.spatialPosition, rep(c(0.5, 0.6, 0.7), 10))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.spatialPosition = 0.5; stop(); }", 1, 285, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i = p1.individuals; i.spatialPosition = 0.5; stop(); }", 1, 459, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { i = p1.individuals; i.spatialPosition = 0.5; stop(); }", 1, 523, "read-only property", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { i = p1.individuals; i.setSpatialPosition(0.5); stop(); }", 1, 269, "cannot be called in non-spatial simulations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i = p1.individuals; i[0].setSpatialPosition(float(0)); }", 1, 446, "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { i = p1.individuals; i[0].setSpatialPosition(0.5); if (identical(i[0].spatialPosition, 0.5)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i = p1.individuals; i[0].setSpatialPosition(c(0.5, 0.6)); }", 1, 446, "position parameter to contain", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i = p1.individuals; i.setSpatialPosition(float(0)); }", 1, 443, "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { i = p1.individuals; i.setSpatialPosition(0.5); if (identical(i.spatialPosition, rep(0.5, 10))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i = p1.individuals; i.setSpatialPosition(c(0.5, 0.6)); }", 1, 443, "position parameter to contain", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { i = p1.individuals; i.setSpatialPosition((1:10) / 10.0); if (identical(i.spatialPosition, (1:10) / 10.0)) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { i = p1.individuals; i[0].setSpatialPosition(0.5); }", 1, 510, "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { i = p1.individuals; i[0].setSpatialPosition(c(0.5, 0.6, 0.7)); if (identical(i[0].spatialPosition, c(0.5, 0.6, 0.7))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { i = p1.individuals; i[0].setSpatialPosition(c(0.5, 0.6, 0.7, 0.8)); }", 1, 510, "position parameter to contain", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { i = p1.individuals; i.setSpatialPosition(0.5); }", 1, 507, "requires at least as many coordinates", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { i = p1.individuals; i.setSpatialPosition(c(0.5, 0.6, 0.7)); if (identical(i.spatialPosition, rep(c(0.5, 0.6, 0.7), 10))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz + "1 { i = p1.individuals; i.setSpatialPosition(c(0.5, 0.6, 0.7, 0.8)); }", 1, 507, "position parameter to contain", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { i = p1.individuals; i.setSpatialPosition(1.0:30); if (identical(i.spatialPosition, 1.0:30)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz + "1 { i = p1.individuals; i.setSpatialPosition(1.0:30); if (identical(i.z, (1.0:10)*3)) stop(); }", __LINE__);
	
	// Some specific testing for setting of accelerated properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tag = (seqAlong(i) % 2 == 0); if (all(i.tag == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tag = seqAlong(i); if (all(i.tag == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tagF = (seqAlong(i) % 2 == 0); if (all(i.tagF == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tagF = seqAlong(i); if (all(i.tagF == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.tagF = asFloat(seqAlong(i)); if (all(i.tagF == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = (seqAlong(i) % 2 == 0); if (all(i.fitnessScaling == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = seqAlong(i); if (all(i.fitnessScaling == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.fitnessScaling = asFloat(seqAlong(i)); if (all(i.fitnessScaling == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.x = (seqAlong(i) % 2 == 0); if (all(i.x == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.x = seqAlong(i); if (all(i.x == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.x = asFloat(seqAlong(i)); if (all(i.x == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.y = (seqAlong(i) % 2 == 0); if (all(i.y == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.y = seqAlong(i); if (all(i.y == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.y = asFloat(seqAlong(i)); if (all(i.y == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.z = (seqAlong(i) % 2 == 0); if (all(i.z == (seqAlong(i) % 2 == 0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.z = seqAlong(i); if (all(i.z == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.z = asFloat(seqAlong(i)); if (all(i.z == seqAlong(i))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals; i.color = format('#%.6X', seqAlong(i)); if (all(i.color == format('#%.6X', seqAlong(i)))) stop(); }", __LINE__);
	
	// Test Individual - (logical)containsMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.containsMutations(object()); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.containsMutations(sim.mutations); stop(); }", __LINE__);
	
	// Test Individual - (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.countOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.countOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i[0:1].countOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Individual - (float$)sumOfMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.sumOfMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.sumOfMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i[0:1].sumOfMutationsOfType(1); stop(); }", __LINE__);
	
	// Test Individual - (object<Mutation>)uniqueMutationsOfType(io<MutationType>$ mutType)
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.uniqueMutationsOfType(m1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i.uniqueMutationsOfType(1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "10 { i = p1.individuals; i[0:1].uniqueMutationsOfType(1); stop(); }", __LINE__);
	
	// Test optional pedigree stuff
	std::string gen1_setup_norel("initialize() { initializeSLiMOptions(F); initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } ");
	std::string gen1_setup_rel("initialize() { initializeSLiMOptions(T); initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } ");
	
	SLiMAssertScriptRaise(gen1_setup_norel + "5 { if (all(p1.individuals.pedigreeID == -1)) stop(); }", 1, 296, "has not been enabled", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_norel + "5 { if (all(p1.individuals.pedigreeParentIDs == -1)) stop(); }", 1, 296, "has not been enabled", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_norel + "5 { if (all(p1.individuals.pedigreeGrandparentIDs == -1)) stop(); }", 1, 296, "has not been enabled", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_norel + "5 { if (all(p1.individuals.genomes.genomePedigreeID == -1)) stop(); }", 1, 304, "has not been enabled", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 { if (p1.individuals[0].relatedness(p1.individuals[0]) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 { if (p1.individuals[0].relatedness(p1.individuals[1]) == 0.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_norel + "5 { if (all(p1.individuals[0].relatedness(p1.individuals[1:9]) == 0.0)) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals.pedigreeID != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals.pedigreeParentIDs != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals.pedigreeGrandparentIDs != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals.genomes.genomePedigreeID != -1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (p1.individuals[0].relatedness(p1.individuals[0]) == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (p1.individuals[0].relatedness(p1.individuals[1]) <= 0.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_rel + "5 { if (all(p1.individuals[0].relatedness(p1.individuals[1:9]) <= 0.5)) stop(); }", __LINE__);
	
	// Test Individual SLiMEidosDictionary functionality: - (+)getValue(string$ key) and - (void)setValue(string$ key, + value)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals[0]; i.setValue('foo', 7:9); i.setValue('bar', 'baz'); if (identical(i.getValue('foo'), 7:9) & identical(i.getValue('bar'), 'baz')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals[0]; i.setValue('foo', 3:5); i.setValue('foo', 'foobar'); if (identical(i.getValue('foo'), 'foobar')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { i = p1.individuals[0]; i.setValue('foo', 3:5); i.setValue('foo', NULL); if (isNULL(i.getValue('foo'))) stop(); }", __LINE__);
}

#pragma mark InteractionType tests
static void _RunInteractionTypeTests_Nonspatial(bool p_reciprocal, bool p_immediate, bool p_sex_enabled, std::string p_sex_segregation);
static void _RunInteractionTypeTests_Spatial(std::string p_max_distance, bool p_reciprocal, bool p_immediate, bool p_sex_enabled, std::string p_sex_segregation);

void _RunInteractionTypeTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: InteractionType
	//
	
	// The goal here is to get good code coverage in interaction_type.cpp; with code of this complexity it's extremely difficult
	// to comprehensively test the actual functionality across all cases and code paths, but at least we can try to execute all
	// the major code paths and make sure we don't crash or anything.
	
	// Test InteractionType properties
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (i1.id == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (isInfinite(i1.maxDistance)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (i1.reciprocal == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (i1.sexSegregation == '**') stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { if (i1.spatiality == 'x') stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i1.id = 2; }", 1, 427, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { i1.maxDistance = 0.5; if (i1.maxDistance == 0.5) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i1.reciprocal = F; }", 1, 435, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i1.sexSegregation = '**'; }", 1, 439, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i1.spatiality = 'x'; }", 1, 435, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { i1.tag; }", 1, 424, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1x + "1 { c(i1,i1).tag; }", 1, 430, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1x + "1 { i1.tag = 17; } 2 { if (i1.tag == 17) stop(); }", __LINE__);
	
	// Run tests in a variety of combinations
	_RunInteractionTypeTests_Nonspatial(false, false, false, "**");
	_RunInteractionTypeTests_Nonspatial(true, false, false, "**");
	_RunInteractionTypeTests_Nonspatial(false, true, false, "**");
	_RunInteractionTypeTests_Nonspatial(true, true, false, "**");
	
	_RunInteractionTypeTests_Spatial(" INF ", false, false, false, "**");
	_RunInteractionTypeTests_Spatial("999.0", false, false, false, "**");
	_RunInteractionTypeTests_Spatial(" INF ", true, false, false, "**");
	_RunInteractionTypeTests_Spatial("999.0", true, false, false, "**");
	_RunInteractionTypeTests_Spatial(" INF ", false, true, false, "**");
	_RunInteractionTypeTests_Spatial("999.0", false, true, false, "**");
	_RunInteractionTypeTests_Spatial(" INF ", true, true, false, "**");
	_RunInteractionTypeTests_Spatial("999.0", true, true, false, "**");
	
	for (int sex_seg_index = 0; sex_seg_index <= 8; ++sex_seg_index)
	{
		// For a full test, change the condition to <= 8; that makes for a long test runtime, but it works.
		// Note that the tests are throttled down when sexSegregation != "**" anyway, because the results
		// will vary, and it's too much work to figure out the right answer for every test in every
		// combination; we just test for a crash or error.
		std::string seg_str;
		
		switch (sex_seg_index)
		{
			case 0: seg_str = "**"; break;
			case 1: seg_str = "*M"; break;
			case 2: seg_str = "*F"; break;
			case 3: seg_str = "M*"; break;
			case 4: seg_str = "MM"; break;
			case 5: seg_str = "MF"; break;
			case 6: seg_str = "F*"; break;
			case 7: seg_str = "FM"; break;
			case 8: seg_str = "FF"; break;
		}
		
		_RunInteractionTypeTests_Nonspatial(false, false, true, seg_str);
		_RunInteractionTypeTests_Nonspatial(true, false, true, seg_str);
		_RunInteractionTypeTests_Nonspatial(false, true, true, seg_str);
		_RunInteractionTypeTests_Nonspatial(true, true, true, seg_str);
		
		_RunInteractionTypeTests_Spatial(" INF ", false, false, true, seg_str);
		_RunInteractionTypeTests_Spatial("999.0", false, false, true, seg_str);
		_RunInteractionTypeTests_Spatial(" INF ", true, false, true, seg_str);
		_RunInteractionTypeTests_Spatial("999.0", true, false, true, seg_str);
		_RunInteractionTypeTests_Spatial(" INF ", false, true, true, seg_str);
		_RunInteractionTypeTests_Spatial("999.0", false, true, true, seg_str);
		_RunInteractionTypeTests_Spatial(" INF ", true, true, true, seg_str);
		_RunInteractionTypeTests_Spatial("999.0", true, true, true, seg_str);
	}
}

void _RunInteractionTypeTests_Nonspatial(bool p_reciprocal, bool p_immediate, bool p_sex_enabled, std::string p_sex_segregation)
{
	std::string reciprocal_string = p_reciprocal ? "reciprocal=T" : "reciprocal=F";
	std::string immediate_string = p_immediate ? "immediate=T" : "immediate=F";
	std::string sex_string = p_sex_enabled ? "initializeSex('A'); " : "                    ";
	bool sex_seg_on = (p_sex_segregation != "**");
	
	std::string gen1_setup_i1_pop("initialize() { initializeMutationRate(1e-5); " + sex_string + "initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', '', " + reciprocal_string + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); } 1:10 late() { i1.evaluate(" + immediate_string + "); i1.strength(p1.individuals[0]); } 1 late() { ind = p1.individuals; ");
	
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.unevaluate(); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.distance(ind[0], ind[2]); stop(); }", 1, 445, "interaction be spatial", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.distanceToPoint(ind[0], 1.0); stop(); }", 1, 445, "interaction be spatial", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.nearestNeighbors(ind[8], 1); stop(); }", 1, 445, "interaction be spatial", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.nearestNeighborsOfPoint(p1, 19.0, 1); stop(); }", 1, 445, "interaction be spatial", __LINE__);
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (i1.strength(ind[0], ind[2]) == 1.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (identical(i1.strength(ind[5]), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (identical(i1.strength(ind[5], NULL), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	}
	SLiMAssertScriptRaise(gen1_setup_i1_pop + "i1.totalOfNeighborStrengths(ind[0]); stop(); }", 1, 445, "interaction be spatial", __LINE__);
}

void _RunInteractionTypeTests_Spatial(std::string p_max_distance, bool p_reciprocal, bool p_immediate, bool p_sex_enabled, std::string p_sex_segregation)
{
	std::string reciprocal_string = p_reciprocal ? "reciprocal=T" : "reciprocal=F";
	std::string immediate_string = p_immediate ? "immediate=T" : "immediate=F";
	std::string sex_string = p_sex_enabled ? "initializeSex('A'); " : "                    ";
	bool sex_seg_on = (p_sex_segregation != "**");
	bool max_dist_on = (p_max_distance != "INF");
	
	// *** 1D
	for (int i = 0; i < 3; ++i)
	{
		std::string gen1_setup_i1x_pop;
		
		if (i == 0)
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'x', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = runif(10); p1.individuals.z = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else if (i == 1)
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'y', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.y = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = runif(10); p1.individuals.z = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else // if (i == 2)
			gen1_setup_i1x_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'z', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.z = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = runif(10); p1.individuals.y = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		
		// Test InteractionType – (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.distance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[0:1], ind[2]), c(11.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (i1.distance(ind[0:1], ind[2:3]) == 11.0) stop(); }", 1, 571, "either individuals1 or individuals2 be", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[integer(0)], ind[8]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[5]), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distance(ind[5], NULL), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
		
		// Test InteractionType – (float)distanceToPoint(object<Individual> individuals1, float point)
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.distanceToPoint(ind[0], 1.0) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distanceToPoint(ind[0:1], 1.0), c(11.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (i1.distanceToPoint(ind[0:1], 1.0:2.0) == 11.0) stop(); }", 1, 571, "point is of length equal to", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distanceToPoint(ind[c(0, 5, 9, 8, 1)], 5.0), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.distanceToPoint(ind[integer(0)], 8.0), float(0))) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 50); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], -1); stop(); }", 1, 567, "requires count >= 0", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return 2.0; }", 1, 567, "requires count >= 0", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return strength * 2.0; }", 1, 567, "requires count >= 0", __LINE__);
		
		// Test InteractionType – (void)evaluate([No<Subpopulation> subpops = NULL], [logical$ immediate = F])
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(); i1.evaluate(); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(NULL); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(immediate=T); i1.evaluate(); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(p1, immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.evaluate(NULL, immediate=T); stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", 1, 581, "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighbors(ind[8], 1), ind[9])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(sortBy(i1.nearestNeighbors(ind[8], 3), 'index'), ind[c(6,7,9)])) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestNeighborsOfPoint(object<Subpopulation>$ subpop, float point, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, 5.0, -1), ind[integer(0)])) stop(); }", 1, 581, "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, 5.0, 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, 19.0, 1), ind[8])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(sortBy(i1.nearestNeighborsOfPoint(p1, 19.0, 3), 'index'), ind[c(7,8,9)])) stop(); }", __LINE__);
		
		// Test InteractionType – (void)setInteractionFunction(string$ functionType, ...)
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.setInteractionFunction('q', 10.0); i1.evaluate(immediate=T); stop(); }", 1, 567, "while the interaction is being evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('q', 10.0); i1.evaluate(immediate=T); stop(); }", 1, 584, "functionType \"q\" must be", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('f'); i1.evaluate(immediate=T); stop(); }", 1, 584, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0, 2.0); i1.evaluate(immediate=T); stop(); }", 1, 584, "requires exactly", __LINE__);
		
		if (!max_dist_on)
			SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 584, "finite maximum interaction distance", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l'); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0, 2.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 604, "requires exactly", __LINE__);
		
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, -1.0); stop(); }", 1, 604, "must have a standard deviation parameter >= 0", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 0.0); stop(); }", 1, 604, "must have a scale parameter > 0", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, -1.0); stop(); }", 1, 604, "must have a scale parameter > 0", __LINE__);
		
		// Test InteractionType – (float)strength(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
		if (!sex_seg_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.strength(ind[0], ind[2]) == 1.0) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(1.0, 1.0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(1.0, 0.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5]), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5], NULL), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(2.0, 0.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(2.0, 0.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		}
		
		// Test InteractionType – (float)totalOfNeighborStrengths(object<Individual> individuals)
		if (!sex_seg_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 9.0)) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 9.0)) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 9.0)) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(9.0, 9.0, 9.0))) stop(); }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(18.0, 18.0, 18.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1x_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(18.0, 18.0, 18.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		}
		
		// Test InteractionType – (void)unevaluate(void)
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.evaluate(); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1x_pop + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.distance(ind[0], ind[2]); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.distanceToPoint(ind[0], 1.0); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.drawByStrength(ind[0]); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.nearestNeighbors(ind[8], 1); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.nearestNeighborsOfPoint(p1, 19.0, 1); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.strength(ind[0], ind[2]); stop(); }", 1, 584, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1x_pop + "i1.unevaluate(); i1.totalOfNeighborStrengths(ind[0]); stop(); }", 1, 584, "has been evaluated", __LINE__);
	}
	
	// *** 2D
	for (int i = 0; i < 6; ++i)
	{
		std::string gen1_setup_i1xy_pop;
		bool use_first_coordinate = (i < 3);
		
		if (i == 0)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xy', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = 0; p1.individuals.z = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else if (i == 1)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.z = 0; p1.individuals.y = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else if (i == 2)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'yz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.y = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.z = 0; p1.individuals.x = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else if (i == 3)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xy', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.y = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = 0; p1.individuals.z = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else if (i == 4)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.z = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.x = 0; p1.individuals.y = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		else // if (i == 5)
			gen1_setup_i1xy_pop = "initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'yz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.z = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = 0; p1.individuals.x = runif(10); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ";
		
		// Test InteractionType – (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.distance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[0:1], ind[2]), c(11.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (i1.distance(ind[0:1], ind[2:3]) == 11.0) stop(); }", 1, 564, "either individuals1 or individuals2 be", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[integer(0)], ind[8]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[5]), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distance(ind[5], NULL), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
		
		// Test InteractionType – (float)distanceToPoint(object<Individual> individuals1, float point)
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.distanceToPoint(ind[0], c(" + (use_first_coordinate ? "1.0, 0.0" : "0.0, 1.0") + ")) == 11.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distanceToPoint(ind[0:1], c(" + (use_first_coordinate ? "1.0, 0.0" : "0.0, 1.0") + ")), c(11.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (i1.distanceToPoint(ind[0:1], 1.0) == 11.0) stop(); }", 1, 564, "point is of length equal to", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distanceToPoint(ind[c(0, 5, 9, 8, 1)], c(" + (use_first_coordinate ? "5.0, 0.0" : "0.0, 5.0") + ")), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.distanceToPoint(ind[integer(0)], c(" + (use_first_coordinate ? "8.0, 0.0" : "0.0, 8.0") + ")), float(0))) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 50); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], -1); stop(); }", 1, 560, "requires count >= 0", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return 2.0; }", 1, 560, "requires count >= 0", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return strength * 2.0; }", 1, 560, "requires count >= 0", __LINE__);
		
		// Test InteractionType – (void)evaluate([No<Subpopulation> subpops = NULL], [logical$ immediate = F])
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(); i1.evaluate(); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(p1); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(NULL); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(immediate=T); i1.evaluate(); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(p1, immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.evaluate(NULL, immediate=T); stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", 1, 574, "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighbors(ind[8], 1), ind[9])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(sortBy(i1.nearestNeighbors(ind[8], 3), 'index'), ind[c(6,7,9)])) stop(); }", __LINE__);
		
		// Test InteractionType – (object<Individual>)nearestNeighborsOfPoint(object<Subpopulation>$ subpop, float point, [integer$ count = 1])
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, c(5.0, 0.0), -1), ind[integer(0)])) stop(); }", 1, 574, "requires count >= 0", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, c(5.0, 0.0), 0), ind[integer(0)])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, c(" + (use_first_coordinate ? "19.0, 0.0" : "0.0, 19.0") + "), 1), ind[8])) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(sortBy(i1.nearestNeighborsOfPoint(p1, c(" + (use_first_coordinate ? "19.0, 0.0" : "0.0, 19.0") + "), 3), 'index'), ind[c(7,8,9)])) stop(); }", __LINE__);
		
		// Test InteractionType – (void)setInteractionFunction(string$ functionType, ...)
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.setInteractionFunction('q', 10.0); i1.evaluate(immediate=T); stop(); }", 1, 560, "while the interaction is being evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('q', 10.0); i1.evaluate(immediate=T); stop(); }", 1, 577, "functionType \"q\" must be", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('f'); i1.evaluate(immediate=T); stop(); }", 1, 577, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0, 2.0); i1.evaluate(immediate=T); stop(); }", 1, 577, "requires exactly", __LINE__);
		
		if (!max_dist_on)
			SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 577, "finite maximum interaction distance", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l'); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0, 2.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 597, "requires exactly", __LINE__);
		
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, -1.0); stop(); }", 1, 597, "must have a standard deviation parameter >= 0", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 0.0); stop(); }", 1, 597, "must have a scale parameter > 0", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, -1.0); stop(); }", 1, 597, "must have a scale parameter > 0", __LINE__);
		
		// Test InteractionType – (float)strength(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
		if (!sex_seg_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.strength(ind[0], ind[2]) == 1.0) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(1.0, 1.0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(1.0, 0.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5]), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5], NULL), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(2.0, 0.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(2.0, 0.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		}
		
		// Test InteractionType – (float)totalOfNeighborStrengths(object<Individual> individuals)
		if (!sex_seg_on)
		{
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 9.0)) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 9.0)) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 9.0)) stop(); }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(9.0, 9.0, 9.0))) stop(); }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(18.0, 18.0, 18.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
			
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
			SLiMAssertScriptStop(gen1_setup_i1xy_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(18.0, 18.0, 18.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		}
		
		// Test InteractionType – (void)unevaluate(void)
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.evaluate(); stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.distance(ind[0], ind[2]); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.distanceToPoint(ind[0], c(1.0, 0.0)); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.drawByStrength(ind[0]); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.nearestNeighbors(ind[8], 1); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.nearestNeighborsOfPoint(p1, 19.0, 1); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.strength(ind[0], ind[2]); stop(); }", 1, 577, "has been evaluated", __LINE__);
		SLiMAssertScriptRaise(gen1_setup_i1xy_pop + "i1.unevaluate(); i1.totalOfNeighborStrengths(ind[0]); stop(); }", 1, 577, "has been evaluated", __LINE__);
	}
	
	// *** 3D with y and z zero
	std::string gen1_setup_i1xyz_pop("initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xyz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = 0; p1.individuals.z = 0; i1.evaluate(" + immediate_string + "); ind = p1.individuals; ");
	
	// Test InteractionType – (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.distance(ind[0], ind[2]) == 11.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[0:1], ind[2]), c(11.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[0], ind[2:3]), c(11.0, 12.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (i1.distance(ind[0:1], ind[2:3]) == 11.0) stop(); }", 1, 557, "either individuals1 or individuals2 be", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[5], ind[c(0, 5, 9, 8, 1)]), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[integer(0)], ind[8]), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[5]), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distance(ind[5], NULL), c(15.0, 5, 4, 3, 2, 0, 2, 3, 15, 20))) stop(); }", __LINE__);
	
	// Test InteractionType – (float)distanceToPoint(object<Individual> individuals1, float point)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.distanceToPoint(ind[0], c(1.0, 0.0, 0.0)) == 11.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distanceToPoint(ind[0:1], c(1.0, 0.0, 0.0)), c(11.0, 1.0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (i1.distanceToPoint(ind[0:1], 1.0) == 11.0) stop(); }", 1, 557, "point is of length equal to", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distanceToPoint(ind[c(0, 5, 9, 8, 1)], c(5.0, 0.0, 0.0)), c(15.0, 0, 20, 15, 5))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.distanceToPoint(ind[integer(0)], c(8.0, 0.0, 0.0)), float(0))) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 50); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], -1); stop(); }", 1, 553, "requires count >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return 2.0; }", 1, 553, "requires count >= 0", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.drawByStrength(ind[0], 0), ind[integer(0)])) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.drawByStrength(ind[0], -1); stop(); } interaction(i1) { return strength * 2.0; }", 1, 553, "requires count >= 0", __LINE__);
	
	// Test InteractionType – (void)evaluate([No<Subpopulation> subpops = NULL], [logical$ immediate = F])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(immediate=T); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(p1, immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.evaluate(NULL, immediate=T); stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighbors(ind[8], -1), ind[integer(0)])) stop(); }", 1, 567, "requires count >= 0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighbors(ind[8], 0), ind[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighbors(ind[8], 1), ind[9])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(sortBy(i1.nearestNeighbors(ind[8], 3), 'index'), ind[c(6,7,9)])) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestNeighborsOfPoint(object<Subpopulation>$ subpop, float point, [integer$ count = 1])
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, c(5.0, 0.0, 0.0), -1), ind[integer(0)])) stop(); }", 1, 567, "requires count >= 0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, c(5.0, 0.0, 0.0), 0), ind[integer(0)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.nearestNeighborsOfPoint(p1, c(19.0, 0.0, 0.0), 1), ind[8])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(sortBy(i1.nearestNeighborsOfPoint(p1, c(19.0, 0.0, 0.0), 3), 'index'), ind[c(7,8,9)])) stop(); }", __LINE__);
	
	// Test InteractionType – (void)setInteractionFunction(string$ functionType, ...)
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.setInteractionFunction('q', 10.0); i1.evaluate(immediate=T); stop(); }", 1, 553, "while the interaction is being evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('q', 10.0); i1.evaluate(immediate=T); stop(); }", 1, 570, "functionType \"q\" must be", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('f'); i1.evaluate(immediate=T); stop(); }", 1, 570, "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0, 2.0); i1.evaluate(immediate=T); stop(); }", 1, 570, "requires exactly", __LINE__);
	
	if (!max_dist_on)
		SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 570, "finite maximum interaction distance", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l'); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0, 2.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 2.0, 1.0); i1.evaluate(immediate=T); stop(); }", 1, 590, "requires exactly", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, -1.0); stop(); }", 1, 590, "must have a standard deviation parameter >= 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, 0.0); stop(); }", 1, 590, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('c', 5.0, -1.0); stop(); }", 1, 590, "must have a scale parameter > 0", __LINE__);
	
	// Test InteractionType – (float)strength(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.strength(ind[0], ind[2]) == 1.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(1.0, 0.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5]), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5], NULL), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);

		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(2.0, 0.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);

		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5], ind[c(0, 5, 9, 8, 1)]), c(2.0, 0.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[1], ind[integer(0)]), float(0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	}
	
	// Test InteractionType – (float)totalOfNeighborStrengths(object<Individual> individuals)
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 9.0)) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 9.0)) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 9.0)) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(9.0, 9.0, 9.0))) stop(); }", __LINE__);

		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(18.0, 18.0, 18.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);

		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[integer(0)]), float(0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[5]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[9]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "if (identical(i1.totalOfNeighborStrengths(ind[c(0, 5, 9)]), c(18.0, 18.0, 18.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	}
	
	// Test InteractionType – (void)unevaluate(void)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.distance(ind[0], ind[2]); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.distanceToPoint(ind[0], c(1.0, 0.0, 0.0)); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.drawByStrength(ind[0]); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.nearestNeighbors(ind[8], 1); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.nearestNeighborsOfPoint(p1, 19.0, 1); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.strength(ind[0], ind[2]); stop(); }", 1, 570, "has been evaluated", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_i1xyz_pop + "i1.unevaluate(); i1.totalOfNeighborStrengths(ind[0]); stop(); }", 1, 570, "has been evaluated", __LINE__);
	
	// *** 3D with full 3D coordinates; we skip the error-testing here since it's the same as before
	std::string gen1_setup_i1xyz_pop_full("initialize() { initializeSLiMOptions(dimensionality='xyz'); " + sex_string + "initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xyz', " + reciprocal_string + ", maxDistance=" + p_max_distance + ", sexSegregation='" + p_sex_segregation + "'); } 1 { sim.addSubpop('p1', 10); p1.individuals.x = c(-10.0, 0, 1, 2, 3, 5, 7, 8, 20, 25); p1.individuals.y = c(12.0, 3, -2, 10, 8, 72, 0, -5, -13, 7); p1.individuals.z = c(0.0, 5, 9, -6, 6, -16, 2, 1, -1, 8); i1.evaluate(" + immediate_string + "); ind = p1.individuals; ");
	
	// Test InteractionType – (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.distance(ind[0], ind[2]) == sqrt(11^2 + 14^2 + 9^2)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.distance(ind[0:1], ind[2]), c(sqrt(11^2 + 14^2 + 9^2), sqrt(1^2 + 5^2 + 4^2)))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.distance(ind[0], ind[2:3]), c(sqrt(11^2 + 14^2 + 9^2), sqrt(12^2 + 2^2 + 6^2)))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (all(i1.distance(ind[5]) - c(63.882705, 72.2979, 78.2112, 62.8728, 67.7052,  0.0, 74.2428, 78.9113, 87.6070, 72.1179) < 0.001)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (all(i1.distance(ind[5], NULL) - c(63.882705, 72.2979, 78.2112, 62.8728, 67.7052,  0.0, 74.2428, 78.9113, 87.6070, 72.1179) < 0.001)) stop(); }", __LINE__);
	
	// Test InteractionType – (float)distanceToPoint(object<Individual> individuals1, float point)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.distanceToPoint(ind[0], c(-7.0, 12.0, 4.0)) == 5.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.distanceToPoint(ind[0:1], c(-7.0, 12.0, 4.0)), c(5.0, sqrt(7^2 + 9^2 + 1^2)))) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0]); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 50); stop(); }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return 2.0; }", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0]); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 1); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.drawByStrength(ind[0], 50); stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	
	// Test InteractionType – (void)evaluate([No<Subpopulation> subpops = NULL], [logical$ immediate = F])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(p1); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(NULL); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(immediate=T); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(p1, immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.evaluate(NULL, immediate=T); stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestNeighbors(ind[8], 1), ind[7])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(sortBy(i1.nearestNeighbors(ind[8], 3), 'index'), ind[c(6,7,9)])) stop(); }", __LINE__);
	
	// Test InteractionType – (object<Individual>)nearestNeighborsOfPoint(object<Subpopulation>$ subpop, float point, [integer$ count = 1])
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestNeighborsOfPoint(p1, c(-7.0, 12.0, 4.0), 1), ind[0])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.nearestNeighborsOfPoint(p1, c(7.0, 3.0, 12.0), 1), ind[2])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(sortBy(i1.nearestNeighborsOfPoint(p1, c(19.0, -4.0, -2.0), 3), 'index'), ind[c(6,7,8)])) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(sortBy(i1.nearestNeighborsOfPoint(p1, c(7.0, 3.0, 12.0), 3), 'index'), ind[c(1,2,4)])) stop(); }", __LINE__);
	
	// Test InteractionType – (void)setInteractionFunction(string$ functionType, ...)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.setInteractionFunction('f', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('l', 5.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('e', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.maxDistance=1.0; i1.setInteractionFunction('n', 5.0, 1.0); i1.evaluate(immediate=T); stop(); }", __LINE__);
	
	// Test InteractionType – (float)strength(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.strength(ind[0], ind[2]) == 1.0) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[0], ind[2:3]), c(1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[5]), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[5], NULL), c(1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0))) stop(); }", __LINE__);

		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return 2.0; }", __LINE__);

		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (i1.strength(ind[0], ind[2]) == 2.0) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[0], ind[2:3]), c(2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[5]), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.strength(ind[5], NULL), c(2.0, 2.0, 2.0, 2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 2.0))) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	}
	
	// Test InteractionType – (float)totalOfNeighborStrengths(object<Individual> individuals)
	if (!sex_seg_on)
	{
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 9.0)) stop(); }", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return 2.0; }", __LINE__);
		
		SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "if (identical(i1.totalOfNeighborStrengths(ind[0]), 18.0)) stop(); } interaction(i1) { return strength * 2.0; }", __LINE__);
	}
	
	// Test InteractionType – (void)unevaluate(void)
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.evaluate(); stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_i1xyz_pop_full + "i1.unevaluate(); i1.unevaluate(); stop(); }", __LINE__);
}

#pragma mark Substitution tests
void _RunSubstitutionTests(void)
{
	// ************************************************************************************
	//
	//	Gen 1+ tests: Substitution
	//
	
	// Test Substitution properties
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { if (size(sim.substitutions) > 0) stop(); }", __LINE__);										// check that our script generates substitutions fast enough
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.fixationGeneration > 0 & sub.fixationGeneration <= 30) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.mutationType == m1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.originGeneration > 0 & sub.originGeneration <= 10) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.position >= 0 & sub.position <= 99999) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { if (sum(sim.substitutions.selectionCoeff == 500.0) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.subpopID == 1) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.fixationGeneration = 10; stop(); }", 1, 375, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.mutationType = m1; stop(); }", 1, 369, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.originGeneration = 10; stop(); }", 1, 373, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.position = 99999; stop(); }", 1, 365, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.selectionCoeff = 50.0; stop(); }", 1, 371, "read-only property", __LINE__);
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.subpopID = 237; if (sub.subpopID == 237) stop(); }", __LINE__);						// legal; this field may be used as a user tag
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
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.type == 'early') stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.type == 'early') stop(); } s1 2:4 early() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.type == 'late') stop(); } s1 2:4 late() { sim = 10; } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { s1.active = 198; if (s1.active == 198) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.end = 4; stop(); } s1 2:4 { sim = 10; } ", 1, 254, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.id = 1; stop(); } s1 2:4 { sim = 10; } ", 1, 253, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.source = '{ sim = 10; }'; stop(); } s1 2:4 { sim = 10; } ", 1, 257, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.start = 2; stop(); } s1 2:4 { sim = 10; } ", 1, 256, "read-only property", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.tag; } s1 2:4 { sim = 10; } ", 1, 250, "before being set", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { c(s1,s1).tag; } s1 2:4 { sim = 10; } ", 1, 256, "before being set", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { s1.tag = 219; if (s1.tag == 219) stop(); } s1 2:4 { sim = 10; } ", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.type = 'event'; stop(); } s1 2:4 { sim = 10; } ", 1, 255, "read-only property", __LINE__);
	
	// No methods on SLiMEidosBlock
	
	// Test user-defined functions in SLiM; there is a huge amount more that could be tested, but these get tested by EidosScribe too,
	// so mostly we just need to make sure here that they get declared and defined properly in SLiM, and are callable.
	SLiMAssertScriptStop(gen1_setup_p1 + "function (i)A(i x) {return x*2;} 1 { if (A(2) == 4) stop(); } 10 {  } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "function (i)A(i x) {return B(x)+1;} function (i)B(i x) {return x*2;} 1 { if (A(2) == 5) stop(); } 10 {  } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "function (i)fac([i b=10]) { if (b <= 1) return 1; else return b*fac(b-1); } 1 { if (fac(5) == 120) stop(); } 10 {  } ", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "function (i)spsize(o<Subpopulation>$ sp) { return sp.individualCount; } 2 { if (spsize(p1) == 10) stop(); } 10 {  } ", __LINE__);
	
	// Test callbacks; we don't attempt to test their functionality here, just their declaration and the fact that they get called
	// Their actual functionality gets tested by the R test suite and the recipes; we want to probe error cases here, more
	// Things to be careful of: declaration syntax, return value types, special optimized cases, pseudo-parameter definitions
	
	// fitness() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(m1) { return relFitness; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(m1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(NULL) { return relFitness; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(NULL) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(m1, p1) { return relFitness; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(m1, p1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(NULL, p1) { return relFitness; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(NULL, p1) { stop(); } 100 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "fitness(m2) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "fitness(m2, p1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "fitness(m1, p4) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "fitness(NULL, p4) { stop(); } 100 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 fitness(m1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 fitness(m1, p1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 fitness(NULL, p1) { stop(); } 100 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness() { stop(); } 100 { ; }", 1, 301, "mutation type id is required", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1, p1, p2) { stop(); } 100 { ; }", 1, 307, "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1, m1) { stop(); } 100 { ; }", 1, 305, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(p1) { stop(); } 100 { ; }", 1, 301, "identifier prefix \"m\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1, NULL) { stop(); } 100 { ; }", 1, 305, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(NULL, m1) { stop(); } 100 { ; }", 1, 307, "identifier prefix \"p\" was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { ; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { return NULL; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { return F; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { return T; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { return 1; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { return 'a'; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { return mut; } 100 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; ; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; return NULL; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; return F; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; return T; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; return 1; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; return 'a'; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "fitness(m1) { mut; return mut; } 100 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "fitness(m1) { mut; homozygous; individual; genome1; genome2; subpop; return relFitness; } 100 { stop(); }", __LINE__);
	
	// mateChoice() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice() { return weights; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice() { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice(p1) { return weights; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mateChoice(p4) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mateChoice(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(m1) { stop(); } 10 { ; }", 1, 304, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1, p1) { stop(); } 10 { ; }", 1, 306, "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(NULL) { stop(); } 10 { ; }", 1, 304, "identifier prefix \"p\" was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { ; } 10 { ; }", 1, 293, "must explicitly return a value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return F; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return T; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return 1; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return 1.0; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return 'a'; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { return genome1; } 10 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; ; } 10 { ; }", 1, 293, "must explicitly return a value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return F; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return T; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return 1; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return 1.0; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return 'a'; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mateChoice(p1) { subpop; return genome1; } 10 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mateChoice(p1) { individual; genome1; genome2; subpop; sourceSubpop; return weights; } 10 { stop(); }", __LINE__);
	
	// modifyChild() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild() { return T; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild() { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild(p1) { return T; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "modifyChild(p4) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 modifyChild(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(m1) { stop(); } 10 { ; }", 1, 305, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1, p1) { stop(); } 10 { ; }", 1, 307, "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(NULL) { stop(); } 10 { ; }", 1, 305, "identifier prefix \"p\" was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { ; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return NULL; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return 1; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return 1.0; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return 'a'; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { return child; } 10 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; ; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return NULL; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return 1; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return 1.0; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return 'a'; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "modifyChild(p1) { subpop; return child; } 10 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "modifyChild(p1) { child; childGenome1; childGenome2; parent1; parent1Genome1; parent1Genome2; isCloning; isSelfing; parent2; parent2Genome1; parent2Genome2; subpop; sourceSubpop; return T; } 10 { stop(); }", __LINE__);
	
	// recombination() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination() { return F; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination() { return T; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination() { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination(p1) { return F; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination(p1) { return T; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "recombination(p4) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 recombination(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(m1) { stop(); } 10 { ; }", 1, 307, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1, p1) { stop(); } 10 { ; }", 1, 309, "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(NULL) { stop(); } 10 { ; }", 1, 307, "identifier prefix \"p\" was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { ; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return NULL; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return 1; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return 1.0; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return 'a'; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { return subpop; } 10 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; ; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return NULL; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return 1; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return 1.0; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return 'a'; } 10 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "recombination(p1) { subpop; return subpop; } 10 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "recombination(p1) { individual; genome1; genome2; subpop; breakpoints; return T; } 10 { stop(); }", __LINE__);
	
	// interaction() callbacks
	static std::string gen1_setup_p1p2p3_i1(gen1_setup_p1p2p3 + "initialize() { initializeInteractionType('i1', ''); } { i1.evaluate(immediate=T); i1.strength(p1.individuals[0]); } ");
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1) { return 1.0; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1) { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1, p1) { return 1.0; } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1, p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_i1 + "interaction(i2) { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_i1 + "interaction(i2, p1) { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_i1 + "interaction(i1, p4) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess("early() { s1.active = 0; } " + gen1_setup_p1p2p3_i1 + "s1 interaction(i1) { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptSuccess("early() { s1.active = 0; } " + gen1_setup_p1p2p3_i1 + "s1 interaction(i1, p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction() { stop(); } 10 { ; }", 1, 421, "interaction type id is required", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1, p1, p2) { stop(); } 10 { ; }", 1, 427, "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1, i1) { stop(); } 10 { ; }", 1, 425, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(p1) { stop(); } 10 { ; }", 1, 421, "identifier prefix \"i\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1, NULL) { stop(); } 10 { ; }", 1, 425, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(NULL, i1) { stop(); } 10 { ; }", 1, 421, "identifier prefix \"i\" was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { ; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return NULL; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return F; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return T; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return 1; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return 'a'; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { return subpop; } 10 { ; }", 1, 409, "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { subpop; ; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { subpop; return F; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { subpop; return T; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { subpop; return 1; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { subpop; return 'a'; } 10 { ; }", 1, 409, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_i1 + "interaction(i1) { subpop; return subpop; } 10 { ; }", 1, 409, "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_i1 + "interaction(i1) { distance; strength; receiver; exerter; subpop; return 1.0; } 10 { stop(); }", __LINE__);
	
	// reproduction() callbacks
	static std::string gen1_setup_p1p2p3_nonWF(nonWF_prefix + gen1_setup_sex_p1 + "1 { sim.addSubpop('p2', 10); sim.addSubpop('p3', 10); } " + "late() { sim.subpopulations.individuals.fitnessScaling = 0.0; } ");
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction() { subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction() { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1, 'F') { subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1, 'F') { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, 'F') { subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, 'F') { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1, NULL) { subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1, NULL) { stop(); } 10 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, NULL) { subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, NULL) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_nonWF + "reproduction(p4) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3_nonWF + "reproduction() { s1.active = 0; } s1 reproduction(p1) { stop(); } 10 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(m1) { stop(); } 10 { ; }", 1, 447, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1, p1) { stop(); } 10 { ; }", 1, 434, "needs a value for sex", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(NULL, '*') { stop(); } 10 { ; }", 1, 434, "needs a value for sex", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return NULL; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return F; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return T; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return 1; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return 1.0; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return 'a'; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { return subpop; } 10 { ; }", 1, 434, "must return void", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return NULL; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return F; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return T; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return 1; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return 1.0; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return 'a'; } 10 { ; }", 1, 434, "must return void", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { subpop; return subpop; } 10 { ; }", 1, 434, "must return void", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3_nonWF + "reproduction(p1) { individual; genome1; genome2; subpop; subpop.addCloned(individual); } 10 { stop(); }", __LINE__);
	
	// mutation() callbacks
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1) { return T; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation() { return T; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation() { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL) { return T; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1, p1) { return T; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1, p1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL, p1) { return T; } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(NULL, p1) { stop(); } 100 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutation(m2) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutation(m2, p1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutation(m1, p4) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "mutation(NULL, p4) { stop(); } 100 { ; }", __LINE__);
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mutation(m1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mutation(m1, p1) { stop(); } 100 { ; }", __LINE__);
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "early() { s1.active = 0; } s1 mutation(NULL, p1) { stop(); } 100 { ; }", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1, p1, p2) { stop(); } 100 { ; }", 1, 308, "unexpected token", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1, m1) { stop(); } 100 { ; }", 1, 306, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(p1) { stop(); } 100 { ; }", 1, 302, "identifier prefix \"m\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1, NULL) { stop(); } 100 { ; }", 1, 306, "identifier prefix \"p\" was expected", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(NULL, m1) { stop(); } 100 { ; }", 1, 308, "identifier prefix \"p\" was expected", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { ; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { return NULL; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { return 1; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { return 1.0; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { return 'a'; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { return mut; } 100 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; ; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; return NULL; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; return 1; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; return 1.0; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; return 'a'; } 100 { ; }", 1, 293, "return value", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "mutation(m1) { mut; return mut; } 100 { ; }", 1, 293, "return value", __LINE__);
	
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "mutation(m1) { mut; genome; element; originalNuc; parent; subpop; return T; } 100 { stop(); }", __LINE__);
}

#pragma mark Continuous space tests
void _RunContinuousSpaceTests(void)
{
	// Since these tests are so different from others – spatiality has to be enabled, interactions have to be set up,
	// etc. – I decided to put them in their own test function, rather than wedging them into the class tests above.
	// Tests of the basic functionality of properties and methods remain in the class tests, however.
}

#pragma mark nonWF model tests
void _RunNonWFTests(void)
{
	// Test properties and methods that should be disabled in nonWF mode
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.setSubpopulationSize(500); } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.cloningRate; } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.setCloningRate(0.5); } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.selfingRate; } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.setSelfingRate(0.5); } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_sex_p1 + "1 { p1.sexRatio; } ", 1, 321, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.5); } ", 1, 321, "not available in nonWF models", __LINE__);
	
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 100, p1); } ", 1, 302, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.immigrantSubpopFractions; } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.immigrantSubpopIDs; } ", 1, 301, "not available in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.setMigrationRates(2, 0.1); } ", 1, 301, "not available in nonWF models", __LINE__);
	
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 mateChoice() { return T; } ", 1, 296, "may not be defined in nonWF models", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ return T; } '); } ", 1, 302, "not available in nonWF models", __LINE__);
	
	// Test properties and methods that should be disabled in WF mode
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.individuals.age; } ", 1, 310, "not available in WF models", __LINE__);
	
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.removeSubpopulation(); stop(); }", 1, 298, "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.takeMigrants(p1.individuals); stop(); }", 1, 298, "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.addCloned(p1.individuals[0]); stop(); }", 1, 298, "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.addCrossed(p1.individuals[0], p1.individuals[1]); stop(); }", 1, 298, "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.addEmpty(); stop(); }", 1, 298, "not available in WF models", __LINE__);
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 { p1.addSelfed(p1.individuals[0]); stop(); }", 1, 298, "not available in WF models", __LINE__);
	
	SLiMAssertScriptRaise(WF_prefix + gen1_setup_p1 + "1 reproduction() { return; } ", 1, 293, "may not be defined in WF models", __LINE__);
	
	// SLiMSim.modelType
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup + "1 { if (sim.modelType == 'nonWF') stop(); } ", __LINE__);
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_sex + "1 { if (sim.modelType == 'nonWF') stop(); } ", __LINE__);
	
	// Individual.age
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1 + "1 { p1.individuals.age; stop(); } ", __LINE__);
	
	// Subpopulation - (void)removeSubpopulation()
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1 + "1 { p1.removeSubpopulation(); stop(); }", __LINE__);
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.removeSubpopulation(); if (p1.individualCount == 10) stop(); }", 1, 328, "undefined identifier", __LINE__);		// the symbol is undefined immediately
	SLiMAssertScriptStop(nonWF_prefix + gen1_setup_p1 + "1 { px=p1; p1.removeSubpopulation(); if (px.individualCount == 10) stop(); }", __LINE__);									// does not take visible effect until child generation
	SLiMAssertScriptRaise(nonWF_prefix + gen1_setup_p1 + "1 { p1.removeSubpopulation(); } 2 { if (p1.individualCount == 0) stop(); }", 1, 334, "undefined identifier", __LINE__);
}

#pragma mark treeseq tests
void _RunTreeSeqTests(void)
{
	// initializeTreeSeq()
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=10.0, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=10.0, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=INF, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=INF, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=0.0, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=0.0, checkCoalescence=F, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=10.0, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=10.0, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=INF, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=INF, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=0.0, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=0.0, checkCoalescence=T, runCrosschecks=F); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=10.0, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=10.0, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=INF, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=INF, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=0.0, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=0.0, checkCoalescence=F, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=10.0, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=10.0, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=INF, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=INF, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=F, simplificationRatio=0.0, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(recordMutations=T, simplificationRatio=0.0, checkCoalescence=T, runCrosschecks=T); } " + gen1_setup_p1 + "100 { stop(); }", __LINE__);
	
	// treeSeqCoalesced()
	SLiMAssertScriptRaise("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "1: { sim.treeSeqCoalesced(); } 100 { stop(); }", 1, 290, "coalescence checking is enabled", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(checkCoalescence=T); } " + gen1_setup_p1 + "1: { sim.treeSeqCoalesced(); } 100 { stop(); }", __LINE__);
	
	// treeSeqSimplify()
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "50 { sim.treeSeqSimplify(); } 100 { stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "1: { sim.treeSeqSimplify(); } 100 { stop(); }", __LINE__);
	
	// treeSeqRememberIndividuals()
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "50 { sim.treeSeqRememberIndividuals(p1.individuals); } 100 { sim.treeSeqSimplify(); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "1: { sim.treeSeqRememberIndividuals(p1.individuals); } 100 { sim.treeSeqSimplify(); stop(); }", __LINE__);
	
	// treeSeqOutput()
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { sim.treeSeqOutput('/tmp/SLiM_treeSeq_1.trees', simplify=F, _binary=F); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { sim.treeSeqOutput('/tmp/SLiM_treeSeq_2.trees', simplify=T, _binary=F); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { sim.treeSeqOutput('/tmp/SLiM_treeSeq_3.trees', simplify=F, _binary=T); stop(); }", __LINE__);
	SLiMAssertScriptStop("initialize() { initializeTreeSeq(); } " + gen1_setup_p1 + "100 { sim.treeSeqOutput('/tmp/SLiM_treeSeq_4.trees', simplify=T, _binary=T); stop(); }", __LINE__);
}

#pragma mark Nucleotide API tests
void _RunNucleotideFunctionTests(void)
{
	// nucleotidesToCodons()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotidesToCodons(string(0)), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotidesToCodons(integer(0)), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons('A'); }", 1, 247, "multiple of three in length", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(0); }", 1, 247, "multiple of three in length", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons('AA'); }", 1, 247, "multiple of three in length", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c(0,0)); }", 1, 247, "multiple of three in length", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('AAA') == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('A','A','A')) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(0,0,0)) == 0) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('AAC') == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('A','A','C')) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(0,0,1)) == 1) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('AAG') == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('A','A','G')) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(0,0,2)) == 2) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('AAT') == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('A','A','T')) == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(0,0,3)) == 3) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('ACA') == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('A','C','A')) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(0,1,0)) == 4) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('CAA') == 16) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('C','A','A')) == 16) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(1,0,0)) == 16) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons('TTT') == 63) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c('T','T','T')) == 63) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(3,3,3)) == 63) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { if (nucleotidesToCodons('AAAA') == 0) stop(); }", 1, 251, "multiple of three in length", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { if (nucleotidesToCodons(c(0,0,0,0)) == 0) stop(); }", 1, 251, "multiple of three in length", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotidesToCodons('AAAAACAAGAATTTT'), c(0,1,2,3,63))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotidesToCodons(c('A','A','A','A','A','C','A','A','G','A','A','T','T','T','T')), c(0,1,2,3,63))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotidesToCodons(c(0,0,0,0,0,1,0,0,2,0,0,3,3,3,3)), c(0,1,2,3,63))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons('ADA'); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c('A','D','A')); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c(0,-1,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c(0,4,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons('AAAADA'); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c('A','A','A','A','D','A')); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c(0,0,0,0,-1,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotidesToCodons(c(0,0,0,0,4,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	
	// codonsToAminoAcids()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(integer(0), long=F, paste=T), '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(integer(0), long=T, paste=T), '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(integer(0), long=F, paste=F), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(integer(0), long=T, paste=F), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(0, long=F, paste=T), 'K')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(0, long=T, paste=T), 'Lys')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(0, long=F, paste=F), 'K')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(0, long=T, paste=F), 'Lys')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(c(0,1,63), long=F, paste=T), 'KNF')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(c(0,1,63), long=T, paste=T), 'Lys-Asn-Phe')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(c(0,1,63), long=F, paste=F), c('K','N','F'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToAminoAcids(c(0,1,63), long=T, paste=F), c('Lys', 'Asn', 'Phe'))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(-1, long=F, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(-1, long=T, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(-1, long=F, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(-1, long=T, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(64, long=F, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(64, long=T, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(64, long=F, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(64, long=T, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,-1), long=F, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,-1), long=T, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,-1), long=F, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,-1), long=T, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,64), long=F, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,64), long=T, paste=T); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,64), long=F, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToAminoAcids(c(0,64), long=T, paste=F); }", 1, 247, "requires codons to be", __LINE__);
	
	// mm16To256()
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mm16To256(rep(0.0,15)); }", 1, 247, "to be of length 16", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mm16To256(rep(0.0,16)); }", 1, 247, "to be a 4x4 matrix", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(mm16To256(matrix(rep(0.0,16), ncol=4)), matrix(rep(0.0,256),ncol=4))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(mm16To256(matrix(rep(0.25,16), ncol=4)), matrix(rep(0.25,256),ncol=4))) stop(); }", __LINE__);
	
	// mmJukesCantor()
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mmJukesCantor(-0.1); }", 1, 247, "requires alpha >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mmJukesCantor(0.35); }", 1, 247, "requires 3 * alpha <= 1.0", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(mmJukesCantor(0.0), matrix(rep(0.0,16),ncol=4))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(mmJukesCantor(0.25), matrix(c(0.0, 0.25, 0.25, 0.25, 0.25, 0.0, 0.25, 0.25, 0.25, 0.25, 0.0, 0.25, 0.25, 0.25, 0.25, 0.0),ncol=4))) stop(); }", __LINE__);
	
	// mmKimura()
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mmKimura(-0.1, 0.5); }", 1, 247, "requires alpha to be in", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mmKimura(1.1, 0.5); }", 1, 247, "requires alpha to be in", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mmKimura(0.5, -0.1); }", 1, 247, "requires beta to be in", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { mmKimura(0.5, 1.1); }", 1, 247, "requires beta to be in", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(mmKimura(0.0, 0.0), matrix(rep(0.0,16),ncol=4))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(mmKimura(0.5, 0.25), matrix(c(0.0, 0.25, 0.5, 0.25, 0.25, 0.0, 0.25, 0.5, 0.5, 0.25, 0.0, 0.25, 0.25, 0.5, 0.25, 0.0),ncol=4))) stop(); }", __LINE__);
	
	// nucleotideCounts()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(string(0)), c(0,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(integer(0)), c(0,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts('A'), c(1,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts('C'), c(0,1,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts('G'), c(0,0,1,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts('T'), c(0,0,0,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(0), c(1,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(1), c(0,1,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(2), c(0,0,1,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(3), c(0,0,0,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts('ACGT'), c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(c('A','C','G','T')), c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(c(0,1,2,3)), c(1,1,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts('AACACGATCG'), c(4,3,2,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(c('A','A','C','A','C','G','A','T','C','G')), c(4,3,2,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideCounts(c(0,0,1,0,1,2,0,3,1,2)), c(4,3,2,1))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts('ADA'); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts(c('A','D','A')); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts(c(0,-1,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts(c(0,4,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts('AAAADA'); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts(c('A','A','A','A','D','A')); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts(c(0,0,0,0,-1,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideCounts(c(0,0,0,0,4,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	
	// nucleotideFrequencies()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (all(isNAN(nucleotideFrequencies(string(0))))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (all(isNAN(nucleotideFrequencies(integer(0))))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies('A'), c(1.0,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies('C'), c(0,1.0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies('G'), c(0,0,1.0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies('T'), c(0,0,0,1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(0), c(1.0,0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(1), c(0,1.0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(2), c(0,0,1.0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(3), c(0,0,0,1.0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies('ACGT'), c(0.25,0.25,0.25,0.25))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(c('A','C','G','T')), c(0.25,0.25,0.25,0.25))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(c(0,1,2,3)), c(0.25,0.25,0.25,0.25))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies('AACACGATCG'), c(0.4,0.3,0.2,0.1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(c('A','A','C','A','C','G','A','T','C','G')), c(0.4,0.3,0.2,0.1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(nucleotideFrequencies(c(0,0,1,0,1,2,0,3,1,2)), c(0.4,0.3,0.2,0.1))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies('ADA'); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies(c('A','D','A')); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies(c(0,-1,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies(c(0,4,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies('AAAADA'); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies(c('A','A','A','A','D','A')); }", 1, 247, "requires string sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies(c(0,0,0,0,-1,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { nucleotideFrequencies(c(0,0,0,0,4,0)); }", 1, 247, "requires integer sequence values", __LINE__);
	
	// randomNucleotides()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(randomNucleotides(0, format='string'), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(randomNucleotides(0, format='char'), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(randomNucleotides(0, format='integer'), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, format='string'), 'A')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(1); if (identical(randomNucleotides(1, format='char'), 'T')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(2); if (identical(randomNucleotides(1, format='integer'), 2)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(3); if (identical(randomNucleotides(10, format='string'), 'ACACATATGA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(4); if (identical(randomNucleotides(10, format='char'), c('A','G','C','A','C','T','C','G','C','T'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(5); if (identical(randomNucleotides(10, format='integer'), c(2,2,0,1,2,2,0,2,1,3))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(1.0,0,0,0), format='string'), 'A')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(1.0,0,0,0), format='char'), 'A')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(1.0,0,0,0), format='integer'), 0)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,1.0,0,0), format='string'), 'C')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,1.0,0,0), format='char'), 'C')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,1.0,0,0), format='integer'), 1)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,1.0,0), format='string'), 'G')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,1.0,0), format='char'), 'G')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,1.0,0), format='integer'), 2)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,0,1.0), format='string'), 'T')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,0,1.0), format='char'), 'T')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(1, basis=c(0,0,0,1.0), format='integer'), 3)) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(1.0,0,0,0), format='string'), 'AAAAAAAAAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(1.0,0,0,0), format='char'), rep('A',10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(1.0,0,0,0), format='integer'), rep(0,10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,1.0,0,0), format='string'), 'CCCCCCCCCC')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,1.0,0,0), format='char'), rep('C',10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,1.0,0,0), format='integer'), rep(1,10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,1.0,0), format='string'), 'GGGGGGGGGG')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,1.0,0), format='char'), rep('G',10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,1.0,0), format='integer'), rep(2,10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,0,1.0), format='string'), 'TTTTTTTTTT')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,0,1.0), format='char'), rep('T',10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(10, basis=c(0,0,0,1.0), format='integer'), rep(3,10))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(100, basis=c(10.0,1.0,2.0,3.0), format='string'), 'ATAAAAAAAGAAATAAACTATGAATATCATAAAATACAAAATAAAATAATTTGTAAGAGTAAATTATTAGTATGAATCTAACATAATAAAAAATAATATA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(100, basis=c(10.0,1.0,2.0,3.0), format='char'), c('A','T','A','A','A','A','A','A','A','G','A','A','A','T','A','A','A','C','T','A','T','G','A','A','T','A','T','C','A','T','A','A','A','A','T','A','C','A','A','A','A','T','A','A','A','A','T','A','A','T','T','T','G','T','A','A','G','A','G','T','A','A','A','T','T','A','T','T','A','G','T','A','T','G','A','A','T','C','T','A','A','C','A','T','A','A','T','A','A','A','A','A','A','T','A','A','T','A','T','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { setSeed(0); if (identical(randomNucleotides(100, basis=c(10.0,1.0,2.0,3.0), format='integer'), c(0,3,0,0,0,0,0,0,0,2,0,0,0,3,0,0,0,1,3,0,3,2,0,0,3,0,3,1,0,3,0,0,0,0,3,0,1,0,0,0,0,3,0,0,0,0,3,0,0,3,3,3,2,3,0,0,2,0,2,3,0,0,0,3,3,0,3,3,0,2,3,0,3,2,0,0,3,1,3,0,0,1,0,3,0,0,3,0,0,0,0,0,0,3,0,0,3,0,3,0))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(-1); }", 1, 247, "requires length to be in [0, 2e9]", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(0, basis=3.0); }", 1, 247, "requires basis to be either", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(0, basis=c(0.0,0.0,0.0,0.0)); }", 1, 247, "requires at least one basis value to be > 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(0, basis=c(0.0,0.0,0.2,-0.1)); }", 1, 247, "requires basis values to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(0, basis=c(0.0,0.0,0.2,INF)); }", 1, 247, "requires basis values to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(0, basis=c(0.0,0.0,0.2,NAN)); }", 1, 247, "requires basis values to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { randomNucleotides(0, basis=c(0.0,0.0,0.2,0.0), format='foo'); }", 1, 247, "requires a format of", __LINE__);
	
	// codonsToNucleotides()
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(integer(0), format='string'), '')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(integer(0), format='char'), string(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(integer(0), format='integer'), integer(0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(0, format='string'), 'AAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(1, format='string'), 'AAC')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(2, format='string'), 'AAG')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(3, format='string'), 'AAT')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(4, format='string'), 'ACA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(8, format='string'), 'AGA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(12, format='string'), 'ATA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(16, format='string'), 'CAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(32, format='string'), 'GAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(48, format='string'), 'TAA')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(63, format='string'), 'TTT')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(0, format='char'), c('A','A','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(1, format='char'), c('A','A','C'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(2, format='char'), c('A','A','G'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(3, format='char'), c('A','A','T'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(4, format='char'), c('A','C','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(8, format='char'), c('A','G','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(12, format='char'), c('A','T','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(16, format='char'), c('C','A','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(32, format='char'), c('G','A','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(48, format='char'), c('T','A','A'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(63, format='char'), c('T','T','T'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(0, format='integer'), c(0,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(1, format='integer'), c(0,0,1))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(2, format='integer'), c(0,0,2))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(3, format='integer'), c(0,0,3))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(4, format='integer'), c(0,1,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(8, format='integer'), c(0,2,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(12, format='integer'), c(0,3,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(16, format='integer'), c(1,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(32, format='integer'), c(2,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(48, format='integer'), c(3,0,0))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(63, format='integer'), c(3,3,3))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(0:5, format='string'), 'AAAAACAAGAATACAACC')) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(0:5, format='char'), c('A','A','A','A','A','C','A','A','G','A','A','T','A','C','A','A','C','C'))) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(codonsToNucleotides(0:5, format='integer'), c(0,0,0,0,0,1,0,0,2,0,0,3,0,1,0,0,1,1))) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(-1, format='string'); }", 1, 247, "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(-1, format='char'); }", 1, 247, "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(-1, format='integer'); }", 1, 247, "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(64, format='string'); }", 1, 247, "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(64, format='char'); }", 1, 247, "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(64, format='integer'); }", 1, 247, "requires codon values to be", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { codonsToNucleotides(0, format='foo'); }", 1, 247, "requires a format of", __LINE__);
}

void _RunNucleotideMethodTests(void)
{
	// Test that various nucleotide-based APIs behave as they ought to when used in a non-nucleotide model
	SLiMAssertScriptRaise("initialize() { initializeAncestralNucleotides('ACGT'); } ", 1, 15, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeHotspotMap(1.0); } ", 1, 15, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationTypeNuc(1, 0.5, 'f', 0.0); } ", 1, 15, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mutationMatrix=mmJukesCantor(1e-7)); } ", 1, 60, "to be NULL in non-nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.hotspotEndPositions; }", 1, 262, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.hotspotEndPositionsM; }", 1, 262, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.hotspotEndPositionsF; }", 1, 262, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.hotspotMultipliers; }", 1, 262, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.hotspotMultipliersM; }", 1, 262, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.hotspotMultipliersF; }", 1, 262, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.ancestralNucleotides(); }", 1, 262, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.chromosome.setHotspotMap(1.0); }", 1, 262, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes[0].nucleotides(); }", 1, 261, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { g1.mutationMatrix; }", 1, 250, "only defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { g1.setMutationMatrix(mmJukesCantor(1e-7)); }", 1, 250, "only be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.nucleotide; }", 1, 276, "only defined for nucleotide-based mutations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.nucleotideValue; }", 1, 276, "only defined for nucleotide-based mutations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.nucleotide; }", 1, 356, "only defined for nucleotide-based mutations", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.nucleotideValue; }", 1, 356, "only defined for nucleotide-based mutations", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (sim.nucleotideBased == F) stop(); }", __LINE__);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (m1.nucleotideBased == F) stop(); }", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 5000, nucleotide='A'); stop(); }", 1, 278, "NULL in non-nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.0, 5000, nucleotide='A'); stop(); }", 1, 278, "NULL in non-nucleotide-based models", __LINE__);
	
	// Test that some APIs are correctly disabled in nucleotide-based models
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(nucleotideBased=T); initializeMutationRate(1e-7); } ", 1, 57, "may not be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeSLiMOptions(nucleotideBased=T); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); }", 1, 102, "non-NULL in nucleotide-based models", __LINE__);
	
	std::string nuc_model_start("initialize() { initializeSLiMOptions(nucleotideBased=T); ");
	std::string nuc_model_init(nuc_model_start + "initializeAncestralNucleotides(randomNucleotides(1e2)); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-7)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } ");
	
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.mutationEndPositions; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.mutationEndPositionsF; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.mutationEndPositionsM; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.mutationRates; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.mutationRatesF; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.mutationRatesM; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.overallMutationRate; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.overallMutationRateF; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.overallMutationRateM; }", 1, 320, "not defined in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setMutationRate(1e-7); }", 1, 320, "may not be called in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop('p1', 10); gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 50); }", 1, 361, "requires nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop('p1', 10); gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 0.0, 50); }", 1, 361, "requires nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { m1.mutationStackGroup = 2; }", 1, 327, "for nucleotide-based mutation types", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { m1.mutationStackPolicy = 'f'; }", 1, 328, "for nucleotide-based mutation types", __LINE__);
	
	// initializeAncestralNucleotides()
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(integer(0)); } ", 1, 57, "requires a sequence of length >= 1", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(-1); } ", 1, 57, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(4); } ", 1, 57, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides('AACAGTACGTTACAGGTACAD'); } ", 1, 57, "could not be opened or does not exist", __LINE__);	// file path!
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(c(0,-1,2)); } ", 1, 57, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(c(0,4,2)); } ", 1, 57, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeAncestralNucleotides(c('A','D','T')); } ", 1, 57, "string nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "if (initializeAncestralNucleotides('A') == 1) stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "if (initializeAncestralNucleotides(0) == 1) stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "if (initializeAncestralNucleotides('ACGTACGT') == 8) stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "if (initializeAncestralNucleotides(c(0,1,2,3,0,1,2,3)) == 8) stop(); } ", __LINE__);
	
	// initializeHotspotMap()
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(float(0)); } ", 1, 57, "to be a singleton", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(1.0, integer(0)); } ", 1, 57, "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(float(0), 1e2-1); } ", 1, 57, "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(float(0), integer(0)); } ", 1, 57, "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(1.0, sex='A'); } ", 1, 57, "requested sex \"A\" unsupported", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(1.0, sex='M'); } ", 1, 57, "supplied in non-sexual simulation", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeSex('A'); initializeHotspotMap(1.0, sex='A'); } ", 1, 77, "requested sex \"A\" unsupported", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeSex('A'); initializeHotspotMap(1.0, sex='M'); initializeHotspotMap(1.0, sex='F'); initializeHotspotMap(1.0, sex='M'); } ", 1, 149, "may be called only once", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(1.0); initializeHotspotMap(1.0); } ", 1, 84, "may be called only once", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(c(1.0, 1.2)); } ", 1, 57, "multipliers to be a singleton", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(-0.1); } ", 1, 57, "multipliers to be >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(0.1, c(10, 20)); } ", 1, 57, "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(c(1.0, 1.2), 10); } ", 1, 57, "of equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(c(1.0, 1.2), c(20, 10)); } ", 1, 57, "in strictly ascending order", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeHotspotMap(c(1.0, -1.2), c(10, 20)); } ", 1, 57, "multipliers to be >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 20)); } 1 {}", -1, -1, "do not cover the full chromosome", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "initializeHotspotMap(2.0); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); } 1 { stop(); } ", __LINE__);
	
	// initializeMutationTypeNuc() (copied from initializeMutationType())
	SLiMAssertScriptStop(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "initializeMutationTypeNuc(1, 0.5, 'f', 0.0); stop(); }", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc(-1, 0.5, 'f', 0.0); stop(); }", 1, 57, "identifier value is out of range", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('p2', 0.5, 'f', 0.0); stop(); }", 1, 57, "identifier prefix \"m\" was expected", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('mm1', 0.5, 'f', 0.0); stop(); }", 1, 57, "must be a simple integer", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f'); stop(); }", 1, 57, "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', 0.0, 0.0); stop(); }", 1, 57, "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 0.0); stop(); }", 1, 57, "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'e', 0.0, 0.0); stop(); }", 1, 57, "requires exactly 1 DFE parameter", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 0.0); stop(); }", 1, 57, "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 0.0); stop(); }", 1, 57, "requires exactly 2 DFE parameters", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', 'foo'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 'foo', 0.0); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 0.0, 'foo'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'e', 'foo'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 'foo', 0.0); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 0.0, 'foo'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 'foo', 0.0); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 0.0, 'foo'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', '1'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', '1', 0.0); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 0.0, '1'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'e', '1'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', '1', 0.0); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 0.0, '1'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', '1', 0.0); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 0.0, '1'); stop(); }", 1, 57, "must be of type numeric", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'x', 0.0); stop(); }", 1, 57, "must be \"f\", \"g\", \"e\", \"n\", \"w\", or \"s\"", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "x = initializeMutationTypeNuc('m7', 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_start + "x = initializeMutationTypeNuc(7, 0.5, 'f', 0.0); if (x == m7) stop(); }", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "m7 = 15; initializeMutationTypeNuc(7, 0.5, 'f', 0.0); stop(); }", 1, 66, "already defined", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); stop(); }", 1, 105, "already defined", __LINE__);
	
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 3.1, 0.0); stop(); }", 1, 57, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'g', 3.1, -1.0); stop(); }", 1, 57, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'n', 3.1, -1.0); stop(); }", 1, 57, "must have a standard deviation parameter >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 0.0, 7.5); stop(); }", 1, 57, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', -1.0, 7.5); stop(); }", 1, 57, "must have a scale parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 3.1, 0.0); stop(); }", 1, 57, "must have a shape parameter > 0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationTypeNuc('m1', 0.5, 'w', 3.1, -7.5); stop(); }", 1, 57, "must have a shape parameter > 0", __LINE__);
	
	// initializeGenomicElementType()
	SLiMAssertScriptRaise(nuc_model_start + "initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mutationMatrix=mmJukesCantor(1e-7)); } ", 1, 102, "requires all mutation types for", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0); } ", 1, 316, "non-NULL in nucleotide-based models", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, float(0)); } ", 1, 316, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, rep(1.0, 16)); } ", 1, 316, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, rep(1.0, 256)); } ", 1, 316, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, matrix(rep(1.0, 16), ncol=2)); } ", 1, 316, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, matrix(rep(1.0, 256), ncol=2)); } ", 1, 316, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, matrix(rep(1.0, 16), ncol=4)); } ", 1, 316, "must contain 0.0 for all", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, matrix(rep(1.0, 256), ncol=4)); } ", 1, 316, "must contain 0.0 for all", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, mmJukesCantor(0.25)*2); } ", 1, 316, "requires the sum of each mutation matrix row", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, mm16To256(mmJukesCantor(0.25))*2); } ", 1, 316, "requires the sum of each mutation matrix row", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { mm = mmJukesCantor(0.25); mm[0,1] = -0.1; initializeGenomicElementType('g2', m1, 1.0, mm); } ", 1, 358, "to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "initialize() { mm = mm16To256(mmJukesCantor(0.25)); mm[0,1] = -0.1; initializeGenomicElementType('g2', m1, 1.0, mm); } ", 1, 369, "to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, mmJukesCantor(0.25)); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGenomicElementType('g2', m1, 1.0, mm16To256(mmJukesCantor(0.25))); stop(); } ", __LINE__);
	
	// hotspotEndPositions, hotspotEndPositionsM, hotspotEndPositionsF
	SLiMAssertScriptStop(nuc_model_init + "1 { if (sim.chromosome.hotspotEndPositions == 1e2-1) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(2.0); } 1 { if (sim.chromosome.hotspotEndPositions == 1e2-1) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); } 1 { if (identical(sim.chromosome.hotspotEndPositions, c(10, 1e2-1))) stop(); }", __LINE__);
	
	// hotspotMultipliers, hotspotMultipliersM, hotspotMultipliersF
	SLiMAssertScriptStop(nuc_model_init + "1 { if (sim.chromosome.hotspotMultipliers == 1.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(2.0); } 1 { if (sim.chromosome.hotspotMultipliers == 2.0) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); } 1 { if (identical(sim.chromosome.hotspotMultipliers, c(1.0, 1.2))) stop(); }", __LINE__);
	
	// ancestralNucleotides()
	std::string ances_setup_string = "initialize() { initializeSLiMOptions(nucleotideBased=T); defineConstant('AS', randomNucleotides(1e2, format='string')); initializeAncestralNucleotides(AS); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-7)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } ";
	std::string ances_setup_char = "initialize() { initializeSLiMOptions(nucleotideBased=T); defineConstant('AS', randomNucleotides(1e2, format='char')); initializeAncestralNucleotides(AS); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-7)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } ";
	std::string ances_setup_integer = "initialize() { initializeSLiMOptions(nucleotideBased=T); defineConstant('AS', randomNucleotides(1e2, format='integer')); initializeAncestralNucleotides(AS); initializeMutationTypeNuc(1, 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-7)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } ";
	
	SLiMAssertScriptStop(ances_setup_string + "1 { if (identical(sim.chromosome.ancestralNucleotides(format='string'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { if (identical(sim.chromosome.ancestralNucleotides(end=49, format='string'), substr(AS, 0, 49))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=50, format='string'), substr(AS, 50, 99))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=25, end=69, format='string'), substr(AS, 25, 69))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(substr(AS, 10, 39)))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(ances_setup_char + "1 { if (identical(sim.chromosome.ancestralNucleotides(format='char'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { if (identical(sim.chromosome.ancestralNucleotides(end=49, format='char'), AS[0:49])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=50, format='char'), AS[50:99])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=25, end=69, format='char'), AS[25:69])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(AS[10:39]))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(ances_setup_integer + "1 { if (identical(sim.chromosome.ancestralNucleotides(format='integer'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { if (identical(sim.chromosome.ancestralNucleotides(end=49, format='integer'), AS[0:49])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=50, format='integer'), AS[50:99])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=25, end=69, format='integer'), AS[25:69])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { if (identical(sim.chromosome.ancestralNucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(AS[10:39]))) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.chromosome.ancestralNucleotides(start=-1, end=50, format='integer'); }", 1, 364, "within the chromosome's extent", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.chromosome.ancestralNucleotides(start=50, end=100, format='integer'); }", 1, 364, "within the chromosome's extent", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.chromosome.ancestralNucleotides(start=75, end=25, format='integer'); }", 1, 364, "start must be <= end", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.chromosome.ancestralNucleotides(format='foo'); }", 1, 364, "format must be either", __LINE__);
	
	// setHotspotMap()
	std::string nuc_w_hotspot = nuc_model_init + "initialize() { initializeHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); } ";
	
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(float(0)); }", 1, 386, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(1.0, integer(0)); }", 1, 386, "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(float(0), 1e2-1); }", 1, 386, "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(float(0), integer(0)); }", 1, 386, "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(1.0, sex='A'); }", 1, 386, "sex \"A\" unsupported", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(1.0, sex='M'); }", 1, 386, "original configuration", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(c(1.0, 1.2)); }", 1, 386, "to be a singleton if", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(-0.1); }", 1, 386, "multipliers must be >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(0.1, c(10, 20)); }", 1, 386, "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(c(1.0, 1.2), 10); }", 1, 386, "equal and nonzero size", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(c(1.0, 1.2), c(20, 10)); }", 1, 386, "strictly ascending order", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(c(1.0, -1.2), c(10, 20)); }", 1, 386, "multipliers must be >= 0", __LINE__);
	SLiMAssertScriptRaise(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(c(1.0, 1.2), c(10, 20)); }", 1, 386, "must end at the last position", __LINE__);
	SLiMAssertScriptStop(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(1.2); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_w_hotspot + "1 { sim.chromosome.setHotspotMap(c(1.0, 1.2), c(10, 1e2-1)); stop(); }", __LINE__);
	
	// nucleotides()
	SLiMAssertScriptStop(ances_setup_string + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(format='string'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(end=49, format='string'), substr(AS, 0, 49))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=50, format='string'), substr(AS, 50, 99))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=25, end=69, format='string'), substr(AS, 25, 69))) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_string + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(substr(AS, 10, 39)))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(ances_setup_char + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(format='char'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(end=49, format='char'), AS[0:49])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=50, format='char'), AS[50:99])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=25, end=69, format='char'), AS[25:69])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_char + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(AS[10:39]))) stop(); }", __LINE__);
	
	SLiMAssertScriptStop(ances_setup_integer + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(format='integer'), AS)) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(end=49, format='integer'), AS[0:49])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=50, format='integer'), AS[50:99])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=25, end=69, format='integer'), AS[25:69])) stop(); }", __LINE__);
	SLiMAssertScriptStop(ances_setup_integer + "1 { sim.addSubpop(1, 10); if (identical(p1.genomes[0].nucleotides(start=10, end=39, format='codon'), nucleotidesToCodons(AS[10:39]))) stop(); }", __LINE__);
	
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.addSubpop(1, 10); p1.genomes[0].nucleotides(start=-1, end=50, format='integer'); }", 1, 385, "within the chromosome's extent", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.addSubpop(1, 10); p1.genomes[0].nucleotides(start=50, end=100, format='integer'); }", 1, 385, "within the chromosome's extent", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.addSubpop(1, 10); p1.genomes[0].nucleotides(start=75, end=25, format='integer'); }", 1, 385, "start must be <= end", __LINE__);
	SLiMAssertScriptRaise(ances_setup_integer + "1 { sim.addSubpop(1, 10); p1.genomes[0].nucleotides(format='foo'); }", 1, 385, "format must be either", __LINE__);
	
	// mutationMatrix()
	SLiMAssertScriptStop(nuc_model_init + "1 { if (identical(g1.mutationMatrix, mmJukesCantor(1e-7))) stop(); }", __LINE__);
	
	// setMutationMatrix()
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(NULL); } ", 1, 308, "cannot be type NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(float(0)); } ", 1, 308, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(rep(1.0, 16)); } ", 1, 308, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(rep(1.0, 256)); } ", 1, 308, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(matrix(rep(1.0, 16), ncol=2)); } ", 1, 308, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(matrix(rep(1.0, 256), ncol=2)); } ", 1, 308, "a 4x4 or 64x4 matrix", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(matrix(rep(1.0, 16), ncol=4)); } ", 1, 308, "must contain 0.0 for all", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(matrix(rep(1.0, 256), ncol=4)); } ", 1, 308, "must contain 0.0 for all", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(mmJukesCantor(0.25)*2); } ", 1, 308, "requires the sum of each mutation matrix row", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { g1.setMutationMatrix(mm16To256(mmJukesCantor(0.25))*2); } ", 1, 308, "requires the sum of each mutation matrix row", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { mm = mmJukesCantor(0.25); mm[0,1] = -0.1; g1.setMutationMatrix(mm); } ", 1, 350, "to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { mm = mm16To256(mmJukesCantor(0.25)); mm[0,1] = -0.1; g1.setMutationMatrix(mm); } ", 1, 361, "to be finite and >= 0.0", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { g1.setMutationMatrix(mmJukesCantor(0.25)); stop(); } ", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { g1.setMutationMatrix(mm16To256(mmJukesCantor(0.25))); stop(); } ", __LINE__);
	
	// nucleotide & nucleotideValue
	std::string nuc_highmut("initialize() { initializeSLiMOptions(nucleotideBased=T); initializeAncestralNucleotides(randomNucleotides(1e2)); initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-2)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } ");
	std::string nuc_fixmut("initialize() { initializeSLiMOptions(nucleotideBased=T); initializeAncestralNucleotides(randomNucleotides(1e2)); initializeMutationTypeNuc('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0, mmJukesCantor(1e-2)); initializeGenomicElement(g1, 0, 1e2-1); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } 10 { sim.mutations[0].setSelectionCoeff(500.0); sim.recalculateFitness(); } ");
	
	SLiMAssertScriptStop(nuc_highmut + "10 { mut = sim.mutations[0]; mut.nucleotide; stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_highmut + "10 { mut = sim.mutations[0]; mut.nucleotideValue; stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_fixmut + "30 { sub = sim.substitutions[0]; sub.nucleotide; stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_fixmut + "30 { sub = sim.substitutions[0]; sub.nucleotideValue; stop(); }", __LINE__);
	
	// addNewDrawnMutation()
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10); }", 1, 341, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide=NULL); }", 1, 341, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide='D'); }", 1, 341, "string nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide=-1); }", 1, 341, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide=4); }", 1, 341, "integer nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide='A'); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewDrawnMutation(m1, 10, nucleotide=0); stop(); }", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10); }", 1, 343, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=NULL); }", 1, 343, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=c('A','D','G','C')); }", 1, 343, "string nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=c(0,-1,2,3)); }", 1, 343, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=c(0,4,2,3)); }", 1, 343, "integer nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=c('A','C','G','T')); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewDrawnMutation(m1, 10, nucleotide=0:3); stop(); }", __LINE__);
	
	// addNewMutation()
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10); }", 1, 341, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide=NULL); }", 1, 341, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide='D'); }", 1, 341, "string nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide=-1); }", 1, 341, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide=4); }", 1, 341, "integer nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide='A'); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0].addNewMutation(m1, 0.5, 10, nucleotide=0); stop(); }", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10); }", 1, 343, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=NULL); }", 1, 343, "nucleotide to be non-NULL", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=c('A','D','G','C')); }", 1, 343, "string nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=c(0,-1,2,3)); }", 1, 343, "integer nucleotide values", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=c(0,4,2,3)); }", 1, 343, "integer nucleotide values", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=c('A','C','G','T')); stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.addSubpop(1, 10); p1.genomes[0:3].addNewMutation(m1, 0.5, 10, nucleotide=0:3); stop(); }", __LINE__);
	
	// SLiMSim.nucleotideBased
	SLiMAssertScriptStop(nuc_model_init + "1 { if (sim.nucleotideBased == T) stop(); }", __LINE__);
	
	// MutationType.nucleotideBased
	SLiMAssertScriptStop(nuc_model_init + "1 { if (m1.nucleotideBased == T) stop(); }", __LINE__);
	
	// initializeGeneConversion() tests using GC bias != 0
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 { if (sim.chromosome.geneConversionEnabled == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 { if (sim.chromosome.geneConversionNonCrossoverFraction == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 { if (sim.chromosome.geneConversionMeanLength == 1234.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 { if (sim.chromosome.geneConversionSimpleConversionFraction == 0.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "initialize() { initializeGeneConversion(0.2, 1234.5, 0.75, -0.01); } 1 { if (sim.chromosome.geneConversionGCBias == -0.01) stop(); }", __LINE__);
	
	// Chromosome.setGeneConversion() tests using GC bias != 0
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionEnabled == T) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionNonCrossoverFraction == 0.2) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionMeanLength == 1234.5) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionSimpleConversionFraction == 0.75) stop(); }", __LINE__);
	SLiMAssertScriptStop(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.2, 1234.5, 0.75, -0.01); if (sim.chromosome.geneConversionGCBias == -0.01) stop(); }", __LINE__);
	
	// Chromosome.setGeneConversion() bounds tests
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(-0.001, 10000000000000, 0.0); stop(); }", 1, 320, "nonCrossoverFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(1.001, 10000000000000, 0.0); stop(); }", 1, 320, "nonCrossoverFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.5, -0.01, 0.0); stop(); }", 1, 320, "meanLength must be >= 0.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.5, 1000, -0.001); stop(); }", 1, 320, "simpleConversionFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.5, 1000, 1.001); stop(); }", 1, 320, "simpleConversionFraction must be between 0.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.5, 1000, 0.0, -1.001); stop(); }", 1, 320, "bias must be between -1.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(nuc_model_init + "1 { sim.chromosome.setGeneConversion(0.5, 1000, 0.0, 1.001); stop(); }", 1, 320, "bias must be between -1.0 and 1.0", __LINE__);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.chromosome.setGeneConversion(0.5, 1000, 0.0, 0.1); stop(); }", 1, 231, "must be 0.0 in non-nucleotide-based models", __LINE__);
}

#pragma mark SLiM timing tests
void _RunSLiMTimingTests(void)
{
#if 0
	// Speed comparison of different signature lookup methods
	// the conclusion is that std::unordered_map is faster than std::map or the old switch()-based scheme
	{
		EidosGlobalStringID genome_properties[4] = {gID_genomeType, gID_isNullGenome, gID_mutations, gID_tag};
		std::map<EidosGlobalStringID, const EidosPropertySignature *> genome_map;
		std::unordered_map<EidosGlobalStringID, const EidosPropertySignature *> genome_unordered_map;
		
		for (int i = 0; i < 4; ++i)
			genome_map.emplace(std::pair<EidosGlobalStringID, const EidosPropertySignature *>(genome_properties[i], gSLiM_Genome_Class->_SignatureForProperty(genome_properties[i])));
		
		for (int i = 0; i < 4; ++i)
			genome_unordered_map.emplace(std::pair<EidosGlobalStringID, const EidosPropertySignature *>(genome_properties[i], gSLiM_Genome_Class->_SignatureForProperty(genome_properties[i])));
		
		{
			clock_t begin = clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 100000000; i++)
			{
				EidosGlobalStringID property_id = genome_properties[Eidos_rng_uniform_int(EIDOS_GSL_RNG, 4)];
				
				total += (int64_t)(gSLiM_Genome_Class->_SignatureForProperty(property_id));
			}
			
			clock_t end = clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for Genome_Class::_SignatureForProperty() calls: " << time_spent << std::endl;
		}
		{
			clock_t begin = clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 100000000; i++)
			{
				EidosGlobalStringID property_id = genome_properties[Eidos_rng_uniform_int(EIDOS_GSL_RNG, 4)];
				
				auto found_iter = genome_map.find(property_id);
				if (found_iter != genome_map.end())
				{
					const EidosPropertySignature *sig = found_iter->second;
					total += (int64_t)sig;
				}
			}
			
			clock_t end = clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for genome_map calls: " << time_spent << std::endl;
		}
		{
			clock_t begin = clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 100000000; i++)
			{
				EidosGlobalStringID property_id = genome_properties[Eidos_rng_uniform_int(EIDOS_GSL_RNG, 4)];
				
				auto found_iter = genome_unordered_map.find(property_id);
				if (found_iter != genome_unordered_map.end())
				{
					const EidosPropertySignature *sig = found_iter->second;
					total += (int64_t)sig;
				}
			}
			
			clock_t end = clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for genome_unordered_map calls: " << time_spent << std::endl << std::endl;
		}
	}
	
	{
		EidosGlobalStringID individual_properties[17] = {gID_subpopulation, gID_index, gID_genomes, gID_sex, gID_tag, gID_tagF, gID_fitnessScaling, gEidosID_x, gEidosID_y, gEidosID_z, gID_age, gID_pedigreeID, gID_pedigreeParentIDs, gID_pedigreeGrandparentIDs, gID_spatialPosition, gID_uniqueMutations, gEidosID_color};
		std::map<EidosGlobalStringID, const EidosPropertySignature *> individual_map;
		std::unordered_map<EidosGlobalStringID, const EidosPropertySignature *> individual_unordered_map;
		
		for (int i = 0; i < 17; ++i)
			individual_map.emplace(std::pair<EidosGlobalStringID, const EidosPropertySignature *>(individual_properties[i], gSLiM_Individual_Class->_SignatureForProperty(individual_properties[i])));
		
		for (int i = 0; i < 17; ++i)
			individual_unordered_map.emplace(std::pair<EidosGlobalStringID, const EidosPropertySignature *>(individual_properties[i], gSLiM_Individual_Class->_SignatureForProperty(individual_properties[i])));
		
		{
			clock_t begin = clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 100000000; i++)
			{
				EidosGlobalStringID property_id = individual_properties[Eidos_rng_uniform_int(EIDOS_GSL_RNG, 17)];
				
				total += (int64_t)(gSLiM_Individual_Class->_SignatureForProperty(property_id));
			}
			
			clock_t end = clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for Individual_Class::_SignatureForProperty() calls: " << time_spent << std::endl;
		}
		{
			clock_t begin = clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 100000000; i++)
			{
				EidosGlobalStringID property_id = individual_properties[Eidos_rng_uniform_int(EIDOS_GSL_RNG, 17)];
				
				auto found_iter = individual_map.find(property_id);
				if (found_iter != individual_map.end())
				{
					const EidosPropertySignature *sig = found_iter->second;
					total += (int64_t)sig;
				}
			}
			
			clock_t end = clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for individual_map calls: " << time_spent << std::endl;
		}
		{
			clock_t begin = clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 100000000; i++)
			{
				EidosGlobalStringID property_id = individual_properties[Eidos_rng_uniform_int(EIDOS_GSL_RNG, 17)];
				
				auto found_iter = individual_unordered_map.find(property_id);
				if (found_iter != individual_unordered_map.end())
				{
					const EidosPropertySignature *sig = found_iter->second;
					total += (int64_t)sig;
				}
			}
			
			clock_t end = clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for individual_unordered_map calls: " << time_spent << std::endl << std::endl;
		}
	}
#endif
	
#if 0
	// Speed comparison between internal and internal symbols for EidosSymbolTable, for ContainsSymbol() checks
	// the conclusion is that an external table is significantly faster for failed checks (as when we have to
	// check the intrinsic symbols table for a name conflict before defining a new symbols), slightly faster for
	// successful checks (as when a random intrinsic symbol is used), but significantly slower for successful
	// checks against early symbols in the table (T and F, which are very commonly used).  We cache EidosValues
	// for the intrinsic constants in the tree during optimization, so the successful-lookup cases should not be
	// important; the failed-check case should be what matters.  Nevertheless, puzzingly, testing indicates that
	// switching the intrinsic constants table to use the external hash table hurts performance.  I'm not sure
	// why that is, but I have decided not to do that since it does not seem to be a win despite the results here.
	{
		EidosGlobalStringID symbol_ids[7] = {gEidosID_T, gEidosID_F, gEidosID_NULL, gEidosID_PI, gEidosID_E, gEidosID_INF, gEidosID_NAN};
		EidosSymbolTable internalTable(EidosSymbolTableType::kEidosIntrinsicConstantsTable, nullptr, false);
		EidosSymbolTable externalTable(EidosSymbolTableType::kEidosIntrinsicConstantsTable, nullptr, true);
		
		{
			clock_t begin = clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 1000000000; i++)
			{
				bool contains_symbol = internalTable.ContainsSymbol(gEidosID_F);
				
				total += (contains_symbol ? 1 : 2);
			}
			
			clock_t end = clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for internalTable (F) checks: " << time_spent << std::endl;
		}
		{
			clock_t begin = clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 1000000000; i++)
			{
				bool contains_symbol = externalTable.ContainsSymbol(gEidosID_F);
				
				total += (contains_symbol ? 1 : 2);
			}
			
			clock_t end = clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for externalTable (F) checks: " << time_spent << std::endl << std::endl;
		}
		
		{
			clock_t begin = clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 1000000000; i++)
			{
				EidosGlobalStringID variable_id = symbol_ids[Eidos_rng_uniform_int(EIDOS_GSL_RNG, 7)];
				
				bool contains_symbol = internalTable.ContainsSymbol(variable_id);
				
				total += (contains_symbol ? 1 : 2);
			}
			
			clock_t end = clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for internalTable successful checks: " << time_spent << std::endl;
		}
		{
			clock_t begin = clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 1000000000; i++)
			{
				EidosGlobalStringID variable_id = symbol_ids[Eidos_rng_uniform_int(EIDOS_GSL_RNG, 7)];
				
				bool contains_symbol = externalTable.ContainsSymbol(variable_id);
				
				total += (contains_symbol ? 1 : 2);
			}
			
			clock_t end = clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for externalTable successful checks: " << time_spent << std::endl << std::endl;
		}
		
		{
			clock_t begin = clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 1000000000; i++)
			{
				bool contains_symbol = internalTable.ContainsSymbol(gEidosID_applyValue);
				
				total += (contains_symbol ? 1 : 2);
			}
			
			clock_t end = clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for internalTable failed checks: " << time_spent << std::endl;
		}
		{
			clock_t begin = clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 1000000000; i++)
			{
				bool contains_symbol = externalTable.ContainsSymbol(gEidosID_applyValue);
				
				total += (contains_symbol ? 1 : 2);
			}
			
			clock_t end = clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for externalTable failed checks: " << time_spent << std::endl << std::endl;
		}
	}
#endif
}































































