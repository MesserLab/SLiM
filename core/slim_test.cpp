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
void SLiMAssertScriptStop(const string &p_script_string);

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
		
		if (raise_message.find("stop() called") == string::npos)
		{
			if ((gEidosCharacterStartOfError == -1) || (gEidosCharacterEndOfError == -1) || !gEidosCurrentScript)
			{
				gSLiMTestFailureCount++;
				
				std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise expected, but no error info set" << endl;
				std::cerr << "   raise message: " << raise_message << endl;
				std::cerr << "--------------------" << std::endl << std::endl;
			}
			else
			{
				eidos_script_error_position(gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript);
				
				if ((gEidosErrorLine != p_bad_line) || (gEidosErrorLineCharacter != p_bad_position))
				{
					gSLiMTestFailureCount++;
					
					std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise expected, but error position unexpected" << endl;
					std::cerr << "   raise message: " << raise_message << endl;
					eidos_log_script_error(std::cerr, gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript, gEidosExecutingRuntimeScript);
					std::cerr << "--------------------" << std::endl << std::endl;
				}
				else
				{
					gSLiMTestSuccessCount++;
					
					std::cerr << p_script_string << " == (expected raise) : \e[32mSUCCESS\e[0m\n   " << raise_message << endl;
				}
			}
		}
		else
		{
			gSLiMTestFailureCount++;
			
			std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : stop() reached" << endl;
			std::cerr << "--------------------" << std::endl << std::endl;
		}
	}
}

void SLiMAssertScriptStop(const string &p_script_string)
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
		
		if (raise_message.find("stop() called") == string::npos)
		{
			gSLiMTestFailureCount++;
			
			std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : stop() not reached" << endl;
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
			
			//std::cerr << p_script_string << " == (expected raise) " << raise_message << " : \e[32mSUCCESS\e[0m" << endl;
		}
	}
}

void RunSLiMTests(void)
{
	// Test SLiM.  The goal here is not really to test that the core code of SLiM is working properly – that simulations
	// work as they are intended to.  Such testing is beyond the scope of what we can do here.  Instead, the goal here
	// is to test all of the Eidos-related APIs in SLiM – to make sure that all properties, methods, and functions in
	// SLiM's Eidos interface work properly.  SLiM itself will get a little incidental testing along the way.
	
	// Note that the code here uses C++11 raw string literals, which Xcode's prettyprinting and autoindent code does not
	// presently understand.  The line/character positions for SLiMAssertScriptRaise() depend upon the indenting that
	// Xcode has chosen to give the Eidos scripts below.  Be careful, therefore, not to re-indent this code!
	
	
	// Reset error counts
	gSLiMTestSuccessCount = 0;
	gSLiMTestFailureCount = 0;
	
	
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
							 5 { sim.outputFull(); }
							 
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
						  5 { sim.outputFull(); }
						  
						  )V0G0N");
	
	SLiMAssertScriptStop(stop_test);
	
	
	// ************************************************************************************
	//
	//	Initialization function tests
	//
	
	// Test (void)initializeGeneConversion(numeric$ conversionFraction, numeric$ meanLength)
	SLiMAssertScriptStop("initialize() { initializeGeneConversion(0.5, 10000000000000); stop(); }");			// legal; no max for meanLength (FIXME should this be illegal?)
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(-0.001, 10000000000000); stop(); }", 1, 15);	// conversionFraction out of range
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(1.001, 10000000000000); stop(); }", 1, 15);	// conversionFraction out of range
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 0.0); stop(); }", 1, 15);				// meanLength out of range
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5); stop(); }", 1, 15);					// missing param
	
	// Test (object<MutationType>$)initializeMutationType(is$ id, numeric$ dominanceCoeff, string$ distributionType, ...)
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); stop(); }");					// legal
	SLiMAssertScriptStop("initialize() { initializeMutationType(1, 0.5, 'f', 0.0); stop(); }");						// legal
	SLiMAssertScriptRaise("initialize() { initializeMutationType(-1, 0.5, 'f', 0.0); stop(); }", 1, 15);			// id invalid
	SLiMAssertScriptRaise("initialize() { initializeMutationType('p2', 0.5, 'f', 0.0); stop(); }", 1, 15);			// id invalid
	SLiMAssertScriptRaise("initialize() { initializeMutationType('mm1', 0.5, 'f', 0.0); stop(); }", 1, 15);			// id invalid
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f'); stop(); }", 1, 15);				// missing param
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0, 0.0); stop(); }", 1, 15);		// DFE param count wrong
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0); stop(); }", 1, 15);			// DFE param count wrong
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', 0.0, 0.0); stop(); }", 1, 15);		// DFE param count wrong
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'x', 0.0); stop(); }", 1, 15);			// DFE type undefined
	SLiMAssertScriptStop("initialize() { x = initializeMutationType('m7', 0.5, 'f', 0.0); if (x == m7) stop(); }");	// check that symbol is defined
	SLiMAssertScriptStop("initialize() { x = initializeMutationType(7, 0.5, 'f', 0.0); if (x == m7) stop(); }");	// check that symbol is defined
	SLiMAssertScriptRaise("initialize() { m7 = 15; initializeMutationType(7, 0.5, 'f', 0.0); stop(); }", 1, 24);	// symbol collision
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); initializeMutationType('m1', 0.5, 'f', 0.0); stop(); }", 1, 60);	// same id defined twice
	
	// Test (object<GenomicElementType>$)initializeGenomicElementType(is$ id, io<MutationType> mutationTypes, numeric proportions)
	std::string define_m12(" initializeMutationType('m1', 0.5, 'f', 0.0); initializeMutationType('m2', 0.5, 'f', 0.5); ");
	
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1:2); stop(); }");					// legal
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType(1, c(m1,m2), 1:2); stop(); }");						// legal
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', 1:2, 1:2); stop(); }");						// legal
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2)); stop(); }", 1, 105);				// missing param
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1); stop(); }", 1, 105);			// proportions count wrong
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), c(0,0)); stop(); }", 1, 105);		// proportions all 0
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), c(0,1)); stop(); }", 1, 105);		// proportion is 0 (FIXME should this be legal?)
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), c(-1,2)); stop(); }", 1, 105);		// proportion is negative
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', 2:3, 1:2); stop(); }", 1, 105);				// reference to undefined mutation type
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(1,1,2), 1:3); stop(); }");					// same mut-type used more than once (FIXME should this be illegal?)
	SLiMAssertScriptStop("initialize() {" + define_m12 + "x = initializeGenomicElementType('g7', c(m1,m2), 1:2); if (x == g7) stop(); }");	// check that symbol is defined
	SLiMAssertScriptStop("initialize() {" + define_m12 + "x = initializeGenomicElementType(7, c(m1,m2), 1:2); if (x == g7) stop(); }");		// check that symbol is defined
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "g7 = 17; initializeGenomicElementType(7, c(m1,m2), 1:2); stop(); }", 1, 114);	// symbol collision
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1:2); initializeGenomicElementType('g1', c(m1,m2), c(0,0)); stop(); }", 1, 156);	// same id defined twice
	
	// Test (void)initializeGenomicElement(io<GenomicElementType>$ genomicElementType, integer$ start, integer$ end)
	std::string define_g1(define_m12 + " initializeGenomicElementType('g1', c(m1,m2), 1:2); ");
	
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000000); stop(); }");				// legal
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeGenomicElement(1, 0, 1000000000); stop(); }");				// legal
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0); stop(); }", 1, 157);					// missing param
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(2, 0, 1000000000); stop(); }", 1, 157);		// reference to undefined genomic element type
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, -1, 1000000000); stop(); }", 1, 157);	// start out of range
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000001); stop(); }", 1, 157);		// end out of range
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 100, 99); stop(); }", 1, 157);			// end before start
	
	// Test (void)initializeMutationRate(numeric$ rate)
	SLiMAssertScriptStop("initialize() { initializeMutationRate(0.0); stop(); }");					// legal
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(); stop(); }", 1, 15);				// missing param
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(-0.0000001); stop(); }", 1, 15);	// rate out of range
	SLiMAssertScriptStop("initialize() { initializeMutationRate(10000000); stop(); }");				// legal; no maximum rate (FIXME should this be illegal?)
	
	// Test (void)initializeRecombinationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(0.0); stop(); }");										// legal: singleton rate, no end
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(); stop(); }", 1, 15);								// missing param
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(-0.00001); stop(); }", 1, 15);						// rate out of range
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(10000); stop(); }");									// legal; no maximum rate (FIXME should there be a max?)
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000)); stop(); }");				// legal: multiple rates, matching ends
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 15);						// missing param
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(integer(0), integer(0)); stop(); }", 1, 15);			// length zero params
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), 1000); stop(); }", 1, 15);				// length mismatch
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), 1:3); stop(); }", 1, 15);				// length mismatch
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000)); stop(); }", 1, 15);		// ends out of order
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000)); stop(); }", 1, 15);		// identical ends
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 15);	// rate out of range
	
	// Test (void)initializeSex(string$ chromosomeType, [numeric$ xDominanceCoeff])
	SLiMAssertScriptStop("initialize() { initializeSex('A'); stop(); }");										// legal: autosome
	SLiMAssertScriptStop("initialize() { initializeSex('X'); stop(); }");										// legal: X chromosome
	SLiMAssertScriptStop("initialize() { initializeSex('Y'); stop(); }");										// legal: Y chromosome
	SLiMAssertScriptRaise("initialize() { initializeSex('Z'); stop(); }", 1, 15);								// unknown chromosome type
	SLiMAssertScriptRaise("initialize() { initializeSex(); stop(); }", 1, 15);									// missing param
	SLiMAssertScriptRaise("initialize() { initializeSex('A', 0.0); stop(); }", 1, 15);							// disallowed dominance coeff
	SLiMAssertScriptStop("initialize() { initializeSex('X', 0.0); stop(); }");									// legal: X chromosome with dominance coeff
	SLiMAssertScriptRaise("initialize() { initializeSex('Y', 0.0); stop(); }", 1, 15);							// disallowed dominance coeff
	SLiMAssertScriptRaise("initialize() { initializeSex('Z', 0.0); stop(); }", 1, 15);							// unknown chromosome type
	SLiMAssertScriptStop("initialize() { initializeSex('X', -10000); stop(); }");								// legal: no minimum value for dominance coeff (FIXME should there be?)
	SLiMAssertScriptStop("initialize() { initializeSex('X', 10000); stop(); }");								// legal: no maximum value for dominance coeff (FIXME should there be?)
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeSex('A'); stop(); }", 1, 35);			// two sex declarations
	
	
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































































