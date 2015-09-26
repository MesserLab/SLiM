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
void SLiMAssertScriptRaise(const string &p_script_string, const int p_bad_line, const int p_bad_position, const std::string &p_reason_snip);
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
		
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise during RunOneGeneration(): " << EidosGetTrimmedRaiseMessage() << endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		return;
	}
	
	delete sim;
	
	gSLiMTestFailureCount--;	// correct for our assumption of failure above
	gSLiMTestSuccessCount++;
	
	//std::cerr << p_script_string << " : \e[32mSUCCESS\e[0m" << endl;
	
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

void SLiMAssertScriptRaise(const string &p_script_string, const int p_bad_line, const int p_bad_position, const std::string &p_reason_snip)
{
	SLiMSim *sim = nullptr;
	
	try {
		std::istringstream infile(p_script_string);
		
		sim = new SLiMSim(infile, nullptr);
		
		while (sim->_RunOneGeneration());
		
		gSLiMTestFailureCount++;
		
		std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : no raise during SLiM execution (expected \"" << p_reason_snip << "\")." << endl;
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
						
						//std::cerr << p_script_string << " == (expected raise) : \e[32mSUCCESS\e[0m\n   " << raise_message << endl;
					}
					else
					{
						gSLiMTestFailureCount++;
						
						std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise expected, but no error info set" << endl;
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
						
						std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise expected, but error position unexpected" << endl;
						std::cerr << "   raise message: " << raise_message << endl;
						eidos_log_script_error(std::cerr, gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript, gEidosExecutingRuntimeScript);
						std::cerr << "--------------------" << std::endl << std::endl;
					}
					else
					{
						gSLiMTestSuccessCount++;
						
						//std::cerr << p_script_string << " == (expected raise) : \e[32mSUCCESS\e[0m\n   " << raise_message << endl;
					}
				}
			}
			else
			{
				gSLiMTestFailureCount++;
				
				std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : raise message mismatch (expected \"" << p_reason_snip << "\")." << endl;
				std::cerr << "   raise message: " << raise_message << endl;
				std::cerr << "--------------------" << std::endl << std::endl;
			}
		}
		else
		{
			gSLiMTestFailureCount++;
			
			std::cerr << p_script_string << " : \e[31mFAILURE\e[0m : stop() reached (expected \"" << p_reason_snip << "\")." << endl;
			std::cerr << "--------------------" << std::endl << std::endl;
		}
	}
	
	delete sim;
	
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

void SLiMAssertScriptStop(const string &p_script_string)
{
	SLiMSim *sim = nullptr;
	
	try {
		std::istringstream infile(p_script_string);
		
		sim = new SLiMSim(infile, nullptr);
		
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
	
	delete sim;
	
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
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
	
	#pragma mark basic tests
	
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
	#pragma mark initialize() tests
	
	// Test (void)initializeGeneConversion(numeric$ conversionFraction, numeric$ meanLength)
	SLiMAssertScriptStop("initialize() { initializeGeneConversion(0.5, 10000000000000); stop(); }");										// legal; no max for meanLength
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(-0.001, 10000000000000); stop(); }", 1, 15, "must be between 0.0 and 1.0");
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(1.001, 10000000000000); stop(); }", 1, 15, "must be between 0.0 and 1.0");
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5, 0.0); stop(); }", 1, 15, "must be greater than 0.0");
	SLiMAssertScriptRaise("initialize() { initializeGeneConversion(0.5); stop(); }", 1, 15, "missing required argument");
	
	// Test (object<MutationType>$)initializeMutationType(is$ id, numeric$ dominanceCoeff, string$ distributionType, ...)
	SLiMAssertScriptStop("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); stop(); }");
	SLiMAssertScriptStop("initialize() { initializeMutationType(1, 0.5, 'f', 0.0); stop(); }");
	SLiMAssertScriptRaise("initialize() { initializeMutationType(-1, 0.5, 'f', 0.0); stop(); }", 1, 15, "identifier value is out of range");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('p2', 0.5, 'f', 0.0); stop(); }", 1, 15, "identifier prefix \"m\" was expected");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('mm1', 0.5, 'f', 0.0); stop(); }", 1, 15, "must be a simple integer");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f'); stop(); }", 1, 15, "requires exactly 1 DFE parameter");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0, 0.0); stop(); }", 1, 15, "requires exactly 1 DFE parameter");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0); stop(); }", 1, 15, "requires exactly 2 DFE parameters");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', 0.0, 0.0); stop(); }", 1, 15, "requires exactly 1 DFE parameter");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 'foo'); stop(); }", 1, 15, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 'foo', 0.0); stop(); }", 1, 15, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0, 'foo'); stop(); }", 1, 15, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', 'foo'); stop(); }", 1, 15, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', '1'); stop(); }", 1, 15, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', '1', 0.0); stop(); }", 1, 15, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0, '1'); stop(); }", 1, 15, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', '1'); stop(); }", 1, 15, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'x', 0.0); stop(); }", 1, 15, "must be \"f\", \"g\", or \"e\"");
	SLiMAssertScriptStop("initialize() { x = initializeMutationType('m7', 0.5, 'f', 0.0); if (x == m7) stop(); }");
	SLiMAssertScriptStop("initialize() { x = initializeMutationType(7, 0.5, 'f', 0.0); if (x == m7) stop(); }");
	SLiMAssertScriptRaise("initialize() { m7 = 15; initializeMutationType(7, 0.5, 'f', 0.0); stop(); }", 1, 24, "already defined");
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); initializeMutationType('m1', 0.5, 'f', 0.0); stop(); }", 1, 60, "already defined");
	
	// Test (object<GenomicElementType>$)initializeGenomicElementType(is$ id, io<MutationType> mutationTypes, numeric proportions)
	std::string define_m12(" initializeMutationType('m1', 0.5, 'f', 0.0); initializeMutationType('m2', 0.5, 'f', 0.5); ");
	
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', object(), integer(0)); stop(); }");			// legal: genomic element with no mutations
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', integer(0), float(0)); stop(); }");			// legal: genomic element with no mutations
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), c(0,0)); stop(); }");				// legal: genomic element with all zero proportions (must be fixed later...)
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1:2); stop(); }");
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType(1, c(m1,m2), 1:2); stop(); }");
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', 1:2, 1:2); stop(); }");
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2)); stop(); }", 1, 105, "missing required argument");
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1); stop(); }", 1, 105, "requires the sizes");
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), c(-1,2)); stop(); }", 1, 105, "must be greater than or equal to zero");
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', 2:3, 1:2); stop(); }", 1, 105, "not defined");
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(2,2), 1:2); stop(); }", 1, 105, "used more than once");
	SLiMAssertScriptStop("initialize() {" + define_m12 + "x = initializeGenomicElementType('g7', c(m1,m2), 1:2); if (x == g7) stop(); }");
	SLiMAssertScriptStop("initialize() {" + define_m12 + "x = initializeGenomicElementType(7, c(m1,m2), 1:2); if (x == g7) stop(); }");
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "g7 = 17; initializeGenomicElementType(7, c(m1,m2), 1:2); stop(); }", 1, 114, "already defined");
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1:2); initializeGenomicElementType('g1', c(m1,m2), c(0,0)); stop(); }", 1, 156, "already defined");
	
	// Test (void)initializeGenomicElement(io<GenomicElementType>$ genomicElementType, integer$ start, integer$ end)
	std::string define_g1(define_m12 + " initializeGenomicElementType('g1', c(m1,m2), 1:2); ");
	
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000000); stop(); }");
	SLiMAssertScriptStop("initialize() {" + define_g1 + "initializeGenomicElement(1, 0, 1000000000); stop(); }");
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0); stop(); }", 1, 157, "missing required argument");
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(2, 0, 1000000000); stop(); }", 1, 157, "not defined");
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, -1, 1000000000); stop(); }", 1, 157, "out of range");
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 0, 1000000001); stop(); }", 1, 157, "out of range");
	SLiMAssertScriptRaise("initialize() {" + define_g1 + "initializeGenomicElement(g1, 100, 99); stop(); }", 1, 157, "is less than start position");
	
	// Test (void)initializeMutationRate(numeric$ rate)
	SLiMAssertScriptStop("initialize() { initializeMutationRate(0.0); stop(); }");
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(); stop(); }", 1, 15, "missing required argument");
	SLiMAssertScriptRaise("initialize() { initializeMutationRate(-0.0000001); stop(); }", 1, 15, "requires rate >= 0");
	SLiMAssertScriptStop("initialize() { initializeMutationRate(10000000); stop(); }");														// legal; no maximum rate
	
	// Test (void)initializeRecombinationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(0.0); stop(); }");														// legal: singleton rate, no end
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(); stop(); }", 1, 15, "missing required argument");
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(-0.00001); stop(); }", 1, 15, "requires rates to be >= 0");
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(10000); stop(); }");													// legal; no maximum rate
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(1000, 2000)); stop(); }");
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 15, "requires rates to be a singleton if");
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(integer(0), integer(0)); stop(); }", 1, 15, "ends and rates to be");
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), 1000); stop(); }", 1, 15, "ends and rates to be");
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), 1:3); stop(); }", 1, 15, "ends and rates to be");
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(2000, 1000)); stop(); }", 1, 15, "ascending order");
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, 0.1), c(1000, 1000)); stop(); }", 1, 15, "ascending order");
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 15, "requires rates to be >= 0");
	
	// Test (void)initializeSex(string$ chromosomeType, [numeric$ xDominanceCoeff])
	SLiMAssertScriptStop("initialize() { initializeSex('A'); stop(); }");
	SLiMAssertScriptStop("initialize() { initializeSex('X'); stop(); }");
	SLiMAssertScriptStop("initialize() { initializeSex('Y'); stop(); }");
	SLiMAssertScriptRaise("initialize() { initializeSex('Z'); stop(); }", 1, 15, "requires a chromosomeType of");
	SLiMAssertScriptRaise("initialize() { initializeSex(); stop(); }", 1, 15, "missing required argument");
	SLiMAssertScriptRaise("initialize() { initializeSex('A', 0.0); stop(); }", 1, 15, "may be supplied only for");
	SLiMAssertScriptStop("initialize() { initializeSex('X', 0.0); stop(); }");
	SLiMAssertScriptRaise("initialize() { initializeSex('Y', 0.0); stop(); }", 1, 15, "may be supplied only for");
	SLiMAssertScriptRaise("initialize() { initializeSex('Z', 0.0); stop(); }", 1, 15, "requires a chromosomeType of");
	SLiMAssertScriptStop("initialize() { initializeSex('X', -10000); stop(); }");															// legal: no minimum value for dominance coeff
	SLiMAssertScriptStop("initialize() { initializeSex('X', 10000); stop(); }");															// legal: no maximum value for dominance coeff
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeSex('A'); stop(); }", 1, 35, "may be called only once");
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: SLiMSim
	//
	#pragma mark SLiMSim tests
	
	std::string gen1_setup("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } ");
	std::string gen1_setup_sex("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeSex('X'); } ");
	std::string gen2_stop(" 2 { stop(); } ");
	
	// Test sim properties
	SLiMAssertScriptStop(gen1_setup + "1 { } " + gen2_stop);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome; } " + gen2_stop);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.chromosome = sim.chromosome; } " + gen2_stop, 1, 231, "read-only property");
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.chromosomeType == 'A') stop(); } ");
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.chromosomeType = 'A'; } " + gen2_stop, 1, 235, "read-only property");
	SLiMAssertScriptStop(gen1_setup_sex + "1 { if (sim.chromosomeType == 'X') stop(); } ");
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { sim.chromosomeType = 'X'; } " + gen2_stop, 1, 255, "read-only property");
	SLiMAssertScriptStop(gen1_setup + "1 { sim.dominanceCoeffX; } " + gen2_stop);															// legal: the property is meaningless but may be accessed
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.dominanceCoeffX = 0.2; } ", 1, 236, "when not simulating an X chromosome");
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.dominanceCoeffX; } " + gen2_stop);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.dominanceCoeffX = 0.2; } " + gen2_stop);
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.generation; } ");
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.generation = 7; } " + gen2_stop);
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.genomicElementTypes == g1) stop(); } ");
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.genomicElementTypes = g1; } ", 1, 240, "read-only property");
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.mutationTypes == m1) stop(); } ");
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.mutationTypes = m1; } ", 1, 234, "read-only property");
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.mutations; } ");
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.mutations = _Test(7); } ", 1, 230, "cannot be object element type");
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.scriptBlocks; } ");
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.scriptBlocks = sim.scriptBlocks[0]; } ", 1, 233, "read-only property");
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.sexEnabled == F) stop(); } ");
	SLiMAssertScriptStop(gen1_setup_sex + "1 { if (sim.sexEnabled == T) stop(); } ");
	SLiMAssertScriptStop(gen1_setup + "1 { if (size(sim.subpopulations) == 0) stop(); } ");
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.subpopulations = _Test(7); } ", 1, 235, "cannot be object element type");
	SLiMAssertScriptStop(gen1_setup + "1 { if (size(sim.substitutions) == 0) stop(); } ");
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.substitutions = _Test(7); } ", 1, 234, "cannot be object element type");
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.tag; } ");
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.tag = -17; } ");
	SLiMAssertScriptStop(gen1_setup + "1 { sim.tag = -17; } 2 { if (sim.tag == -17) stop(); }");
	
	// Test sim - (object<Subpopulation>)addSubpop(is$ subpopID, integer$ size, [float$ sexRatio])
	SLiMAssertScriptStop(gen1_setup + "1 { sim.addSubpop('p1', 10); } " + gen2_stop);
	SLiMAssertScriptStop(gen1_setup + "1 { sim.addSubpop(1, 10); } " + gen2_stop);
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.addSubpop('p1', 10, 0.5); } " + gen2_stop, 1, 220, "non-sexual simulation");
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.addSubpop(1, 10, 0.5); } " + gen2_stop, 1, 220, "non-sexual simulation");
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.addSubpop('p1', 10, 0.5); } " + gen2_stop);
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.addSubpop(1, 10, 0.5); } " + gen2_stop);
	SLiMAssertScriptStop(gen1_setup + "1 { x = sim.addSubpop('p7', 10); if (x == p7) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { x = sim.addSubpop(7, 10); if (x == p7) stop(); }");
	SLiMAssertScriptRaise(gen1_setup + "1 { p7 = 17; sim.addSubpop('p7', 10); stop(); }", 1, 229, "already defined");
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.addSubpop('p7', 10); sim.addSubpop(7, 10); stop(); }", 1, 245, "already exists");
	
	// Test sim - (object<Subpopulation>)addSubpopSplit(is$ subpopID, integer$ size, io<Subpopulation>$ sourceSubpop, [float$ sexRatio])
	std::string gen1_setup_p1(gen1_setup + "1 { sim.addSubpop('p1', 10); } ");
	std::string gen1_setup_sex_p1(gen1_setup_sex + "1 { sim.addSubpop('p1', 10); } ");
	
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit('p2', 10, p1); } " + gen2_stop);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit('p2', 10, 1); } " + gen2_stop);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, p1); } " + gen2_stop);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, 1); } " + gen2_stop);
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, 7); } " + gen2_stop, 1, 251, "not defined");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit('p2', 10, p1, 0.5); } " + gen2_stop, 1, 251, "non-sexual simulation");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, p1, 0.5); } " + gen2_stop, 1, 251, "non-sexual simulation");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { sim.addSubpopSplit('p2', 10, p1, 0.5); } " + gen2_stop);
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { sim.addSubpopSplit(2, 10, p1, 0.5); } " + gen2_stop);
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { x = sim.addSubpopSplit('p7', 10, p1); if (x == p7) stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { x = sim.addSubpopSplit(7, 10, p1); if (x == p7) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p7 = 17; sim.addSubpopSplit('p7', 10, p1); stop(); }", 1, 260, "already defined");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit('p7', 10, p1); sim.addSubpopSplit(7, 10, p1); stop(); }", 1, 285, "already exists");
	
	// Test sim - (void)deregisterScriptBlock(io<SLiMEidosBlock> scriptBlocks)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(s1); } s1 2 { stop(); }");
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(1); } s1 2 { stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(object()); } s1 2 { stop(); }");									// legal: deregister nothing
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(s1, s1)); } s1 2 { stop(); }", 1, 251, "same script block");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(1, 1)); } s1 2 { stop(); }", 1, 251, "same script block");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(s1); sim.deregisterScriptBlock(s1); } s1 2 { stop(); }", 1, 282, "same script block");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(1); sim.deregisterScriptBlock(1); } s1 2 { stop(); }", 1, 281, "same script block");
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(s1, s2)); } s1 2 { stop(); } s2 3 { stop(); }");
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(1, 2)); } s1 2 { stop(); } s2 3 { stop(); }");
	
	// Test sim - (float)mutationFrequencies(No<Subpopulation> subpops, [object<Mutation> mutations])
	std::string gen1_setup_p1p2p3(gen1_setup + "1 { sim.addSubpop('p1', 10); sim.addSubpop('p2', 10); sim.addSubpop('p3', 10); } ");
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(p1); }");
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(c(p1, p2)); }");
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(NULL); }");													// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(sim.subpopulations); }");										// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(object()); }");												// legal to specify an empty object vector
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(1); }", 1, 301, "cannot be type integer");						// this is one API where integer identifiers can't be used
	
	// Test sim - (void)outputFixedMutations(void)
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.outputFixedMutations(); }");
	
	// Test sim - (void)outputFull([string$ filePath])
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.outputFull(); }");
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.outputFull('/tmp/slimOutputFullTest.txt'); }");									// legal, output to file path; this test might work only on Un*x systems
	
	// Test sim - (void)outputMutations(object<Mutation> mutations)
	std::string gen1_setup_highmut_p1("initialize() { initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } ");
	
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 { sim.outputMutations(sim.mutations); }");											// legal; should have some mutations by gen 5
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 { sim.outputMutations(sim.mutations[0]); }");										// legal; output just one mutation
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 { sim.outputMutations(sim.mutations[integer(0)]); }");								// legal to specify an empty object vector
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 { sim.outputMutations(object()); }");												// legal to specify an empty object vector
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "5 { sim.outputMutations(NULL); }", 1, 251, "cannot be type NULL");
	
	// Test - (void)readFromPopulationFile(string$ filePath)
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.readFromPopulationFile('/tmp/slimOutputFullTest.txt'); }");												// legal, read from file path; depends on the outputFull() test above
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.readFromPopulationFile('/tmp/notAFile.foo'); }", 1, 220, "could not open");
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.readFromPopulationFile('/tmp/slimOutputFullTest.txt'); if (size(sim.subpopulations) != 3) stop(); }");	// legal; should wipe previous state
	
	// Test sim - (object<SLiMEidosBlock>)registerEvent(Nis$ id, string$ source, [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerEvent(NULL, '{ stop(); }', 2, 2); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerEvent('s1', '{ stop(); }', 2, 2); }", 1, 259, "already defined");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerEvent(1, '{ stop(); }', 2, 2); }", 1, 259, "already defined");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEvent(1, '{ stop(); }', 2, 2); sim.registerEvent(1, '{ stop(); }', 2, 2); }", 1, 294, "already defined");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEvent(1, '{ stop(); }', 3, 2); }", 1, 251, "requires start <= end");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEvent(1, '{ stop(); }', -1, -1); }", 1, 251, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEvent(1, '{ stop(); }', 0, 0); }", 1, 251, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerEvent(1, '{ $; }', 2, 2); }", 1, 2, "unrecognized token");
	
	// Test sim - (object<SLiMEidosBlock>)registerFitnessCallback(Nis$ id, string$ source, io<MutationType>$ mutType, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', 1, NULL, 5, 10); }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', m1, NULL, 5, 10); }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', 1, 1, 5, 10); }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', m1, p1, 5, 10); }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', 1); } 10 { ; }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }', m1); } 10 { ; }");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(NULL, '{ stop(); }'); }", 1, 251, "missing required argument");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { s1 = 7; sim.registerFitnessCallback('s1', '{ stop(); }', m1, NULL, 2, 2); }", 1, 259, "already defined");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { s1 = 7; sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); }", 1, 259, "already defined");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); }", 1, 314, "already defined");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 3, 2); }", 1, 251, "requires start <= end");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, -1, -1); }", 1, 251, "out of range");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ stop(); }', m1, NULL, 0, 0); }", 1, 251, "out of range");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerFitnessCallback(1, '{ $; }', m1, NULL, 2, 2); }", 1, 2, "unrecognized token");
	
	// Test sim - (object<SLiMEidosBlock>)registerMateChoiceCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }', NULL, 2, 2); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }', NULL, 2, 2); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }', 1, 2, 2); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }', p1, 2, 2); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }'); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL, '{ stop(); }'); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(NULL); }", 1, 251, "missing required argument");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerMateChoiceCallback('s1', '{ stop(); }', NULL, 2, 2); }", 1, 259, "already defined");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 259, "already defined");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 313, "already defined");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 3, 2); }", 1, 251, "requires start <= end");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, -1, -1); }", 1, 251, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ stop(); }', NULL, 0, 0); }", 1, 251, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerMateChoiceCallback(1, '{ $; }', NULL, 2, 2); }", 1, 2, "unrecognized token");
	
	// Test sim - (object<SLiMEidosBlock>)registerModifyChildCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }', NULL, 2, 2); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }', NULL, 2, 2); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }', 1, 2, 2); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }', p1, 2, 2); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }'); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL, '{ stop(); }'); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(NULL); }", 1, 251, "missing required argument");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerModifyChildCallback('s1', '{ stop(); }', NULL, 2, 2); }", 1, 259, "already defined");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 259, "already defined");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 314, "already defined");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 3, 2); }", 1, 251, "requires start <= end");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, -1, -1); }", 1, 251, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ stop(); }', NULL, 0, 0); }", 1, 251, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerModifyChildCallback(1, '{ $; }', NULL, 2, 2); }", 1, 2, "unrecognized token");
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: MutationType
	//
	#pragma mark MutationType tests
	
	// Test MutationType properties
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.distributionParams == 0.0) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.distributionType == 'f') stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.dominanceCoeff == 0.5) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.id == 1) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { m1.tag = 17; } 2 { if (m1.tag == 17) stop(); }");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.distributionParams = 0.1; }", 1, 238, "read-only property");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.distributionType = 'g'; }", 1, 236, "read-only property");
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.dominanceCoeff = 0.3; }");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.id = 2; }", 1, 222, "read-only property");
	
	// Test MutationType - (void)setDistribution(string$ distributionType, ...)
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('f', 2.2); if (m1.distributionType == 'f' & m1.distributionParams == 2.2) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('g', 3.1, 7.5); if (m1.distributionType == 'g' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('e', -3); if (m1.distributionType == 'e' & m1.distributionParams == -3) stop(); }");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('x', 1.5); stop(); }", 1, 219, "must be \"f\", \"g\", or \"e\"");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', 'foo'); stop(); }", 1, 219, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 'foo', 7.5); stop(); }", 1, 219, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, 'foo'); stop(); }", 1, 219, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', 'foo'); stop(); }", 1, 219, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', '1'); stop(); }", 1, 219, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', '1', 7.5); stop(); }", 1, 219, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, '1'); stop(); }", 1, 219, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', '1'); stop(); }", 1, 219, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', T); stop(); }", 1, 219, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', T, 7.5); stop(); }", 1, 219, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, T); stop(); }", 1, 219, "requires that DFE parameters be numeric");
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', T); stop(); }", 1, 219, "requires that DFE parameters be numeric");
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: GenomicElementType
	//
	#pragma mark GenomicElementType tests
	
	// Test GenomicElementType properties
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.id == 1) stop(); }");
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.id = 2; }", 1, 222, "read-only property");
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.mutationFractions == 1.0) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.mutationTypes == m1) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { m1.tag = 17; } 2 { if (m1.tag == 17) stop(); }");
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.mutationFractions = 1.0; }", 1, 237, "read-only property");
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.mutationTypes = m1; }", 1, 233, "read-only property");
	
	// Test GenomicElementType - (void)setMutationFractions(io<MutationType> mutationTypes, numeric proportions)
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(object(), integer(0)); stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(m1, 0.0); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.0) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(1, 0.0); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.0) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(m1, 0.3); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.3) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(1, 0.3); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.3) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2), c(0.3, 0.7)); if (identical(g1.mutationTypes, c(m1,m2)) & identical(g1.mutationFractions, c(0.3,0.7))) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(1,2), c(0.3, 0.7)); if (identical(g1.mutationTypes, c(m1,m2)) & identical(g1.mutationFractions, c(0.3,0.7))) stop(); }");
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2)); stop(); }", 1, 281, "missing required argument");
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2), 0.3); stop(); }", 1, 281, "requires the sizes");
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2), c(-1, 2)); stop(); }", 1, 281, "must be greater than or equal to zero");
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(2,3), c(1, 2)); stop(); }", 1, 281, "not defined");
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m2,m2), c(1, 2)); stop(); }", 1, 281, "used more than once");
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(2,2), c(1, 2)); stop(); }", 1, 281, "used more than once");
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: GenomicElement
	//
	#pragma mark GenomicElement tests
	
	std::string gen1_setup_2ge("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 999); initializeGenomicElement(g1, 1000, 99999); initializeRecombinationRate(1e-8); } ");
	
	// Test GenomicElement properties
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; if (ge.endPosition == 999) stop(); }");
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; if (ge.startPosition == 0) stop(); }");
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; if (ge.genomicElementType == g1) stop(); }");
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.tag = -12; if (ge.tag == -12) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.endPosition = 999; stop(); }", 1, 312, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.startPosition = 0; stop(); }", 1, 314, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.genomicElementType = g1; stop(); }", 1, 319, "read-only property");
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.endPosition == 99999) stop(); }");
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.startPosition == 1000) stop(); }");
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.genomicElementType == g1) stop(); }");
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.tag = -17; if (ge.tag == -17) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.endPosition = 99999; stop(); }", 1, 312, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.startPosition = 1000; stop(); }", 1, 314, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.genomicElementType = g1; stop(); }", 1, 319, "read-only property");
	
	// Test GenomicElement - (void)setGenomicElementType(io<GenomicElementType>$ genomicElementType)
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(g1); stop(); }");
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(1); stop(); }");
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(); stop(); }", 1, 300, "missing required argument");
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(object()); stop(); }", 1, 300, "must be a singleton");
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(2); stop(); }", 1, 300, "not defined");
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: Chromosome
	//
	#pragma mark Chromosome tests
	
	// Test Chromosome properties
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionFraction == 0.0) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionMeanLength == 0.0) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.genomicElements[0].genomicElementType == g1) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.lastPosition == 99999) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.overallMutationRate == 1e-7) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.overallRecombinationRate == 1e-8 * 100000) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.recombinationEndPositions == 99999) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.recombinationRates == 1e-8) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.tag = 3294; if (ch.tag == 3294) stop(); }");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionFraction = 0.1; if (ch.geneConversionFraction == 0.1) stop(); }");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionFraction = -0.001; stop(); }", 1, 263, "out of range");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionFraction = 1.001; stop(); }", 1, 263, "out of range");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 0.2; if (ch.geneConversionMeanLength == 0.2) stop(); }");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 0.0; stop(); }", 1, 265, "out of range");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 1e10; stop(); }");												// legal; no upper bound
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.genomicElements = ch.genomicElements; stop(); }", 1, 256, "read-only property");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.lastPosition = 99999; stop(); }", 1, 253, "read-only property");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.overallMutationRate = 1e-6; if (ch.overallMutationRate == 1e-6) stop(); }");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallMutationRate = -1e-6; stop(); }", 1, 260, "out of range");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.overallMutationRate = 1e6; stop(); }");														// legal; no upper bound
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallRecombinationRate = 1e-2; stop(); }", 1, 265, "read-only property");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationEndPositions = 99999; stop(); }", 1, 266, "read-only property");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationRates = 1e-8; stop(); }", 1, 259, "read-only property");
	
	// Test Chromosome - (void)setRecombinationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.0); stop(); }");														// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(); stop(); }", 1, 240, "missing required argument");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(-0.00001); stop(); }", 1, 240, "out of range");
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(10000); stop(); }");													// legal; no maximum rate
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 240, "to be a singleton if");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", 1, 240, "to be of equal and nonzero size");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", 1, 240, "to be of equal and nonzero size");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 240, "to be of equal and nonzero size");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 240, "ascending order");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 240, "ascending order");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 240, "must be >= 0");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 2000)); stop(); }", 1, 240, "must be >= 0");
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 100000)); stop(); }", 1, 240, "must be >= 0");
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: Mutation
	//
	#pragma mark Mutation tests
	
	// Test Mutation properties
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if (mut.mutationType == m1) stop(); }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if ((mut.originGeneration >= 1) & (mut.originGeneration < 10)) stop(); }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if ((mut.position >= 0) & (mut.position < 100000)) stop(); }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if (mut.selectionCoeff == 0.0) stop(); }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if (mut.subpopID == 1) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.mutationType = m1; stop(); }", 1, 289, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.originGeneration = 1; stop(); }", 1, 293, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.position = 0; stop(); }", 1, 285, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.selectionCoeff = 0.1; stop(); }", 1, 291, "read-only property");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.subpopID = 237; if (mut.subpopID == 237) stop(); }");						// legal; this field may be used as a user tag
	
	// Test Mutation - (void)setSelectionCoeff(float$ selectionCoeff)
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(0.5); if (mut.selectionCoeff == 0.5) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(1); if (mut.selectionCoeff == 1) stop(); }", 1, 276, "cannot be type integer");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(-500.0); if (mut.selectionCoeff == -500.0) stop(); }");	// legal; no lower bound
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(500.0); if (mut.selectionCoeff == 500.0) stop(); }");		// legal; no upper bound
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: Genome
	//
	#pragma mark Genome tests
	
	// Test Genome properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; if (gen.genomeType == 'A') stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; if (gen.isNullGenome == F) stop(); }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; if (gen.mutations[0].mutationType == m1) stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.tag = 278; if (gen.tag == 278) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.genomeType = 'A'; stop(); }", 1, 283, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.isNullGenome = F; stop(); }", 1, 285, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.mutations[0].mutationType = m1; stop(); }", 1, 299, "read-only property");
	
	// Test Genome - (void)addMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.addMutations(object()); stop(); }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.addMutations(gen.mutations[0]); stop(); }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.addMutations(p1.genomes[1].mutations[0]); stop(); }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = p1.genomes[1].mutations[0]; gen.addMutations(rep(mut, 10)); if (sum(gen.mutations == mut) == 1) stop(); }");
	
	// Test Genome - (object<Mutation>)addNewDrawnMutation(io<MutationType>$ mutationType, Ni$ originGeneration, integer$ position, io<Subpopulation>$ originSubpop)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 10, 5000, p1); p1.genomes.addMutations(mut); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 10, 5000, 1); p1.genomes.addMutations(mut); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 10, 5000, p1); p1.genomes.addMutations(mut); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 10, 5000, 1); p1.genomes.addMutations(mut); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, NULL, 5000, 1); p1.genomes.addMutations(mut); stop(); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(7, NULL, 5000, 1); stop(); }", 1, 278, "not defined");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 0, 5000, 1); stop(); }", 1, 278, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, NULL, -1, 1); stop(); }", 1, 278, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, NULL, 100000, 1); stop(); }", 1, 278, "past the end");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, NULL, 5000, 237); stop(); }");											// bad subpop, but this is legal to allow "tagging" of mutations
	
	// Test Genome - (object<Mutation>)addNewMutation(io<MutationType>$ mutationType, Ni$ originGeneration, integer$ position, numeric$ selectionCoeff, io<Subpopulation>$ originSubpop)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 10, 5000, 0.1, p1); p1.genomes.addMutations(mut); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 10, 5000, 0.1, 1); p1.genomes.addMutations(mut); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 10, 5000, 0.1, p1); p1.genomes.addMutations(mut); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 10, 5000, 0.1, 1); p1.genomes.addMutations(mut); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, NULL, 5000, 0.1, 1); p1.genomes.addMutations(mut); stop(); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(7, NULL, 5000, 0.1, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "not defined");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 0, 5000, 0.1, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, NULL, -1, 0.1, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, NULL, 100000, 0.1, 1); p1.genomes.addMutations(mut); stop(); }", 1, 278, "past the end");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, NULL, 5000, 0.1, 237); p1.genomes.addMutations(mut); stop(); }");			// bad subpop, but this is legal to allow "tagging" of mutations
	
	// Test Genome - (void)removeMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 10, 5000, 0.1, p1); gen.removeMutations(mut); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 10, 5000, 0.1, p1); gen.removeMutations(mut); gen.removeMutations(mut); stop(); }");	// legal to remove a mutation that is not present
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { gen = p1.genomes[0]; gen.removeMutations(object()); stop(); }");
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.removeMutations(gen.mutations); stop(); }");
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: Subpopulation
	//
	#pragma mark Subpopulation tests
	
	// Test Subpopulation properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.cloningRate == 0.0) stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.firstMaleIndex == p1.firstMaleIndex) stop(); }");					// legal but undefined value in non-sexual sims
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (size(p1.genomes) == 20) stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.id == 1) stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.immigrantSubpopFractions, float(0))) stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.immigrantSubpopIDs, integer(0))) stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.selfingRate == 0.0) stop(); }");									// legal but always 0.0 in non-sexual sims
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.sexRatio == 0.0) stop(); }");										// legal but always 0.0 in non-sexual sims
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (p1.individualCount == 10) stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.tag = 135; if (p1.tag == 135) stop(); }");

	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.cloningRate = 0.0; stop(); }", 1, 262, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.firstMaleIndex = p1.firstMaleIndex; stop(); }", 1, 265, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.genomes = p1.genomes[0]; stop(); }", 1, 258, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.id = 1; stop(); }", 1, 253, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.immigrantSubpopFractions = 1.0; stop(); }", 1, 275, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.immigrantSubpopIDs = 1; stop(); }", 1, 269, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.selfingRate = 0.0; stop(); }", 1, 262, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.sexRatio = 0.5; stop(); }", 1, 259, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.individualCount = 10; stop(); }", 1, 266, "read-only property");

	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (identical(p1.cloningRate, c(0.0,0.0))) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.firstMaleIndex == 5) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (size(p1.genomes) == 20) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.id == 1) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (identical(p1.immigrantSubpopFractions, float(0))) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (identical(p1.immigrantSubpopIDs, integer(0))) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.selfingRate == 0.0) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.sexRatio == 0.5) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { if (p1.individualCount == 10) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.tag = 135; if (p1.tag == 135) stop(); }");
	
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.cloningRate = 0.0; stop(); }", 1, 282, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.firstMaleIndex = p1.firstMaleIndex; stop(); }", 1, 285, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.genomes = p1.genomes[0]; stop(); }", 1, 278, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.id = 1; stop(); }", 1, 273, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.immigrantSubpopFractions = 1.0; stop(); }", 1, 295, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.immigrantSubpopIDs = 1; stop(); }", 1, 289, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.selfingRate = 0.0; stop(); }", 1, 282, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.sexRatio = 0.5; stop(); }", 1, 279, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.individualCount = 10; stop(); }", 1, 286, "read-only property");
	
	// Test Subpopulation - (float)fitness(Ni indices)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.cachedFitness(NULL), rep(1.0, 10))) stop(); }");				// legal (after subpop construction)
	SLiMAssertScriptStop(gen1_setup_p1 + "2 { if (identical(p1.cachedFitness(NULL), rep(1.0, 10))) stop(); }");				// legal (after child generation)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.cachedFitness(0), 1.0)) stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (identical(p1.cachedFitness(0:3), rep(1.0, 4))) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { identical(p1.cachedFitness(-1), rep(1.0, 10)); stop(); }", 1, 260, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { identical(p1.cachedFitness(10), rep(1.0, 10)); stop(); }", 1, 260, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { identical(p1.cachedFitness(c(-1,5)), rep(1.0, 10)); stop(); }", 1, 260, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { identical(p1.cachedFitness(c(5,10)), rep(1.0, 10)); stop(); }", 1, 260, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { identical(p1.cachedFitness(-1), rep(1.0, 10)); stop(); }", 1, 260, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { identical(p1.cachedFitness(10), rep(1.0, 10)); stop(); }", 1, 260, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { identical(p1.cachedFitness(c(-1,5)), rep(1.0, 10)); stop(); }", 1, 260, "out of range");
	SLiMAssertScriptRaise(gen1_setup_p1 + "2 { identical(p1.cachedFitness(c(5,10)), rep(1.0, 10)); stop(); }", 1, 260, "out of range");
	
	// Test Subpopulation - (void)outputMSSample(integer$ sampleSize, [string$ requestedSex])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.outputMSSample(1); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.outputMSSample(5); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.outputMSSample(10); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.outputMSSample(20); stop(); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.outputMSSample(1, 'M'); stop(); }", 1, 250, "non-sexual simulation");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.outputMSSample(1, 'F'); stop(); }", 1, 250, "non-sexual simulation");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.outputMSSample(1, '*'); stop(); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.outputMSSample(1, 'Z'); stop(); }", 1, 250, "requested sex");
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputMSSample(1); stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputMSSample(5); stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputMSSample(10); stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputMSSample(20); stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputMSSample(1, 'M'); stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputMSSample(1, 'F'); stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputMSSample(1, '*'); stop(); }");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.outputMSSample(1, 'Z'); stop(); }", 1, 270, "requested sex");
	
	// Test Subpopulation - (void)outputSample(integer$ sampleSize, [string$ requestedSex])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.outputSample(1); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.outputSample(5); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.outputSample(10); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.outputSample(20); stop(); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.outputSample(1, 'M'); stop(); }", 1, 250, "non-sexual simulation");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.outputSample(1, 'F'); stop(); }", 1, 250, "non-sexual simulation");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.outputSample(1, '*'); stop(); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.outputSample(1, 'Z'); stop(); }", 1, 250, "requested sex");
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputSample(1); stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputSample(5); stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputSample(10); stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputSample(20); stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputSample(1, 'M'); stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputSample(1, 'F'); stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.outputSample(1, '*'); stop(); }");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.outputSample(1, 'Z'); stop(); }", 1, 270, "requested sex");
	
	// Test Subpopulation - (void)setCloningRate(numeric rate)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setCloningRate(0.0); } 10 { if (p1.cloningRate == 0.0) stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setCloningRate(0.5); } 10 { if (p1.cloningRate == 0.5) stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setCloningRate(1.0); } 10 { if (p1.cloningRate == 1.0) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setCloningRate(-0.001); stop(); }", 1, 250, "within [0,1]");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setCloningRate(1.001); stop(); }", 1, 250, "within [0,1]");
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(0.0); } 10 { if (identical(p1.cloningRate, c(0.0, 0.0))) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(0.5); } 10 { if (identical(p1.cloningRate, c(0.5, 0.5))) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(1.0); } 10 { if (identical(p1.cloningRate, c(1.0, 1.0))) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setCloningRate(-0.001); stop(); }", 1, 270, "within [0,1]");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setCloningRate(1.001); stop(); }", 1, 270, "within [0,1]");
	
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(0.0, 0.1)); } 10 { if (identical(p1.cloningRate, c(0.0, 0.1))) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(0.5, 0.1)); } 10 { if (identical(p1.cloningRate, c(0.5, 0.1))) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(1.0, 0.1)); } 10 { if (identical(p1.cloningRate, c(1.0, 0.1))) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(0.0, -0.001)); stop(); }", 1, 270, "within [0,1]");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setCloningRate(c(0.0, 1.001)); stop(); }", 1, 270, "within [0,1]");
	
	// Test Subpopulation - (void)setMigrationRates(io<Subpopulation> sourceSubpops, numeric rates)
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(2, 0.1); } 10 { stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(3, 0.1); } 10 { stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 3), c(0.1, 0.1)); } 10 { stop(); }");
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(1, 0.1); } 10 { stop(); }", 1, 300, "self-referential");
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(4, 0.1); } 10 { stop(); }", 1, 300, "not defined");
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 1), c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "self-referential");
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 4), c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "not defined");
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 2), c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "two rates set");
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(p2, p2), c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "two rates set");
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 3), 0.1); } 10 { stop(); }", 1, 300, "to be equal in size");
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(2, c(0.1, 0.1)); } 10 { stop(); }", 1, 300, "to be equal in size");
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(2, -0.0001); } 10 { stop(); }", 1, 300, "within [0,1]");
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(2, 1.0001); } 10 { stop(); }", 1, 300, "within [0,1]");
	SLiMAssertScriptRaise(gen1_setup_p1p2p3 + "1 { p1.setMigrationRates(c(2, 3), c(0.6, 0.6)); } 10 { stop(); }", -1, -1, "must sum to <= 1.0");	// raise is from EvolveSubpopulation(); we don't force constraints prematurely
	
	// Test Subpopulation - (void)setSelfingRate(numeric$ rate)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSelfingRate(0.0); } 10 { if (p1.selfingRate == 0.0) stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSelfingRate(0.5); } 10 { if (p1.selfingRate == 0.5) stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSelfingRate(1.0); } 10 { if (p1.selfingRate == 1.0) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSelfingRate(-0.001); }", 1, 250, "within [0,1]");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSelfingRate(1.001); }", 1, 250, "within [0,1]");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setSelfingRate(0.0); stop(); }");													// we permit this, since a rate of 0.0 makes sense even in sexual sims
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSelfingRate(0.1); stop(); }", 1, 270, "cannot be called in sexual simulations");
	
	// Test Subpopulation - (void)setSexRatio(float$ sexRatio)
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSexRatio(0.0); stop(); }", 1, 250, "cannot be called in asexual simulations");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSexRatio(0.1); stop(); }", 1, 250, "cannot be called in asexual simulations");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.0); } 10 { if (p1.sexRatio == 0.0) stop(); }", 1, 270, "produced no males");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.1); } 10 { if (p1.sexRatio == 0.1) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.5); } 10 { if (p1.sexRatio == 0.5) stop(); }");
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { p1.setSexRatio(0.9); } 10 { if (p1.sexRatio == 0.9) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSexRatio(1.0); } 10 { if (p1.sexRatio == 1.0) stop(); }", 1, 270, "produced no females");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSexRatio(-0.001); }", 1, 270, "within [0,1]");
	SLiMAssertScriptRaise(gen1_setup_sex_p1 + "1 { p1.setSexRatio(1.001); }", 1, 270, "within [0,1]");
	
	// Test Subpopulation - (void)setSubpopulationSize(integer$ size)
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSubpopulationSize(0); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSubpopulationSize(0); if (p1.individualCount == 10) stop(); }");					// does not take visible effect until child generation
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSubpopulationSize(0); } 2 { if (p1.individualCount == 0) stop(); }", 1, 285, "undefined identifier");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSubpopulationSize(20); stop(); }");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSubpopulationSize(20); if (p1.individualCount == 10) stop(); }");					// does not take visible effect until child generation
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { p1.setSubpopulationSize(20); } 2 { if (p1.individualCount == 20) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p1.setSubpopulationSize(-1); stop(); }", 1, 250, "out of range");
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: Substitution
	//
	#pragma mark Substitution tests
	
	// Test Substitution properties
	std::string gen1_setup_fixmut_p1("initialize() { initializeMutationRate(1e-4); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } 10 { sim.mutations[0].setSelectionCoeff(500.0); sim.recalculateFitness(); } ");
	
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { if (size(sim.substitutions) > 0) stop(); }");										// check that our script generates substitutions fast enough
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.fixationTime > 0 & sub.fixationTime <= 30) stop(); }");
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.mutationType == m1) stop(); }");
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.originGeneration > 0 & sub.originGeneration <= 10) stop(); }");
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.position > 0 & sub.position <= 99999) stop(); }");
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { if (sum(sim.substitutions.selectionCoeff == 500.0) == 1) stop(); }");
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; if (sub.subpopID == 1) stop(); }");
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.fixationTime = 10; stop(); }", 1, 369, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.mutationType = m1; stop(); }", 1, 369, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.originGeneration = 10; stop(); }", 1, 373, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.position = 99999; stop(); }", 1, 365, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.selectionCoeff = 50.0; stop(); }", 1, 371, "read-only property");
	SLiMAssertScriptStop(gen1_setup_fixmut_p1 + "30 { sub = sim.substitutions[0]; sub.subpopID = 237; if (sub.subpopID == 237) stop(); }");						// legal; this field may be used as a user tag
	
	// No methods on Substitution
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: SLiMEidosBlock
	//
	#pragma mark SLiMEidosBlock tests
	
	// Test SLiMEidosBlock properties
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.active == -1) stop(); } s1 2:4 { sim = 10; } ");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.end == 4) stop(); } s1 2:4 { sim = 10; } ");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.id == 1) stop(); } s1 2:4 { sim = 10; } ");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.source == '{ sim = 10; }') stop(); } s1 2:4 { sim = 10; } ");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.start == 2) stop(); } s1 2:4 { sim = 10; } ");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { s1.tag; stop(); } s1 2:4 { sim = 10; } ");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { if (s1.type == 'event') stop(); } s1 2:4 { sim = 10; } ");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { s1.active = 198; if (s1.active == 198) stop(); } s1 2:4 { sim = 10; } ");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.end = 4; stop(); } s1 2:4 { sim = 10; } ", 1, 254, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.id = 1; stop(); } s1 2:4 { sim = 10; } ", 1, 253, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.source = '{ sim = 10; }'; stop(); } s1 2:4 { sim = 10; } ", 1, 257, "read-only property");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.start = 2; stop(); } s1 2:4 { sim = 10; } ", 1, 256, "read-only property");
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { s1.tag = 219; if (s1.tag == 219) stop(); } s1 2:4 { sim = 10; } ");
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1.type = 'event'; stop(); } s1 2:4 { sim = 10; } ", 1, 255, "read-only property");
	
	// No methods on SLiMEidosBlock
	
	
	// ************************************************************************************
	//
	//	Print a summary of test results
	//
	std::cerr << endl;
	if (gSLiMTestFailureCount)
		std::cerr << "\e[31mFAILURE\e[0m count: " << gSLiMTestFailureCount << endl;
	std::cerr << "\e[32mSUCCESS\e[0m count: " << gSLiMTestSuccessCount << endl;
	std::cerr.flush();
	
	// Clear out the SLiM output stream post-test
	gSLiMOut.clear();
	gSLiMOut.str("");
}































































