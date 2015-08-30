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
					
					//std::cerr << p_script_string << " == (expected raise) : \e[32mSUCCESS\e[0m\n   " << raise_message << endl;
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
	SLiMAssertScriptStop("initialize() { initializeGeneConversion(0.5, 10000000000000); stop(); }");			// legal; no max for meanLength
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
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 'foo'); stop(); }", 1, 15);		// DFE params must be numeric
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 'foo', 0.0); stop(); }", 1, 15);	// DFE param must be numeric
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0, 'foo'); stop(); }", 1, 15);	// DFE param must be numeric
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', 'foo'); stop(); }", 1, 15);		// DFE param must be numeric
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', '1'); stop(); }", 1, 15);			// DFE params must be numeric
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', '1', 0.0); stop(); }", 1, 15);		// DFE param must be numeric
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'g', 0.0, '1'); stop(); }", 1, 15);		// DFE param must be numeric
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'e', '1'); stop(); }", 1, 15);			// DFE param must be numeric
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'x', 0.0); stop(); }", 1, 15);			// DFE type undefined
	SLiMAssertScriptStop("initialize() { x = initializeMutationType('m7', 0.5, 'f', 0.0); if (x == m7) stop(); }");	// check that symbol is defined
	SLiMAssertScriptStop("initialize() { x = initializeMutationType(7, 0.5, 'f', 0.0); if (x == m7) stop(); }");	// check that symbol is defined
	SLiMAssertScriptRaise("initialize() { m7 = 15; initializeMutationType(7, 0.5, 'f', 0.0); stop(); }", 1, 24);	// symbol collision
	SLiMAssertScriptRaise("initialize() { initializeMutationType('m1', 0.5, 'f', 0.0); initializeMutationType('m1', 0.5, 'f', 0.0); stop(); }", 1, 60);	// same id defined twice
	
	// Test (object<GenomicElementType>$)initializeGenomicElementType(is$ id, io<MutationType> mutationTypes, numeric proportions)
	std::string define_m12(" initializeMutationType('m1', 0.5, 'f', 0.0); initializeMutationType('m2', 0.5, 'f', 0.5); ");
	
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', object(), integer(0)); stop(); }");			// legal: genomic element with no mutations
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', integer(0), float(0)); stop(); }");			// legal: genomic element with no mutations
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), c(0,0)); stop(); }");				// legal: genomic element with all zero proportions (must be fixed later...)
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1:2); stop(); }");					// legal
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType(1, c(m1,m2), 1:2); stop(); }");						// legal
	SLiMAssertScriptStop("initialize() {" + define_m12 + "initializeGenomicElementType('g1', 1:2, 1:2); stop(); }");						// legal
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2)); stop(); }", 1, 105);				// missing param
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), 1); stop(); }", 1, 105);			// proportions count wrong
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(m1,m2), c(-1,2)); stop(); }", 1, 105);		// proportion is negative
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', 2:3, 1:2); stop(); }", 1, 105);				// reference to undefined mutation type
	SLiMAssertScriptRaise("initialize() {" + define_m12 + "initializeGenomicElementType('g1', c(2,2), 1:2); stop(); }", 1, 105);			// reference to a mutation type more than once
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
	SLiMAssertScriptStop("initialize() { initializeMutationRate(10000000); stop(); }");				// legal; no maximum rate
	
	// Test (void)initializeRecombinationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(0.0); stop(); }");										// legal: singleton rate, no end
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(); stop(); }", 1, 15);								// missing param
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(-0.00001); stop(); }", 1, 15);						// rate out of range
	SLiMAssertScriptStop("initialize() { initializeRecombinationRate(10000); stop(); }");									// legal; no maximum rate
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
	SLiMAssertScriptStop("initialize() { initializeSex('X', -10000); stop(); }");								// legal: no minimum value for dominance coeff
	SLiMAssertScriptStop("initialize() { initializeSex('X', 10000); stop(); }");								// legal: no maximum value for dominance coeff
	SLiMAssertScriptRaise("initialize() { initializeSex('A'); initializeSex('A'); stop(); }", 1, 35);			// two sex declarations
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: SLiMSim
	//
	
	std::string gen1_setup("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } ");
	std::string gen1_setup_sex("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeSex('X'); } ");
	std::string gen2_stop(" 2 { stop(); } ");
	
	// Test sim properties
	SLiMAssertScriptStop(gen1_setup + "1 { } " + gen2_stop);															// legal; simplest simulation, no subpop
	SLiMAssertScriptStop(gen1_setup + "1 { sim.chromosome; } " + gen2_stop);											// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.chromosome = sim.chromosome; } " + gen2_stop, 1, 231);					// read-only property
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.chromosomeType == 'A') stop(); } ");									// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.chromosomeType = 'A'; } " + gen2_stop, 1, 235);							// read-only property
	SLiMAssertScriptStop(gen1_setup_sex + "1 { if (sim.chromosomeType == 'X') stop(); } ");								// legal
	SLiMAssertScriptRaise(gen1_setup_sex + "1 { sim.chromosomeType = 'X'; } " + gen2_stop, 1, 255);						// read-only property
	SLiMAssertScriptStop(gen1_setup + "1 { sim.dominanceCoeffX; } " + gen2_stop);										// legal: the property is meaningless but may be accessed
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.dominanceCoeffX = 0.2; } ", 1, 236);									// setting disallowed property
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.dominanceCoeffX; } " + gen2_stop);									// legal: property enabled
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.dominanceCoeffX = 0.2; } " + gen2_stop);								// legal: property enabled
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.generation; } ");														// legal
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.generation = 7; } " + gen2_stop);										// legal
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.genomicElementTypes == g1) stop(); } ");								// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.genomicElementTypes = g1; } ", 1, 240);									// read-only property
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.mutationTypes == m1) stop(); } ");									// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.mutationTypes = m1; } ", 1, 234);										// read-only property
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.mutations; } ");														// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.mutations = _Test(7); } ", 1, 230);										// type Mutation required
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.scriptBlocks; } ");													// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.scriptBlocks = sim.scriptBlocks[0]; } ", 1, 233);						// read-only property
	SLiMAssertScriptStop(gen1_setup + "1 { if (sim.sexEnabled == F) stop(); } ");										// legal
	SLiMAssertScriptStop(gen1_setup_sex + "1 { if (sim.sexEnabled == T) stop(); } ");			// legal
	SLiMAssertScriptStop(gen1_setup + "1 { if (size(sim.subpopulations) == 0) stop(); } ");								// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.subpopulations = _Test(7); } ", 1, 235);								// type Subpopulation required
	SLiMAssertScriptStop(gen1_setup + "1 { if (size(sim.substitutions) == 0) stop(); } ");								// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.substitutions = _Test(7); } ", 1, 234);									// type Substitution required
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.tag; } ");															// legal
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.tag = -17; } ");														// legal
	SLiMAssertScriptStop(gen1_setup + "1 { sim.tag = -17; } 2 { if (sim.tag == -17) stop(); }");						// legal
	
	// Test sim - (object<Subpopulation>)addSubpop(is$ subpopID, integer$ size, [float$ sexRatio])
	SLiMAssertScriptStop(gen1_setup + "1 { sim.addSubpop('p1', 10); } " + gen2_stop);									// legal with subpop string
	SLiMAssertScriptStop(gen1_setup + "1 { sim.addSubpop(1, 10); } " + gen2_stop);										// legal with subpop id
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.addSubpop('p1', 10, 0.5); } " + gen2_stop, 1, 220);						// sex ratio supplied in non-sexual simulation
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.addSubpop(1, 10, 0.5); } " + gen2_stop, 1, 220);						// sex ratio supplied in non-sexual simulation
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.addSubpop('p1', 10, 0.5); } " + gen2_stop);							// legal
	SLiMAssertScriptStop(gen1_setup_sex + "1 { sim.addSubpop(1, 10, 0.5); } " + gen2_stop);								// legal
	SLiMAssertScriptStop(gen1_setup + "1 { x = sim.addSubpop('p7', 10); if (x == p7) stop(); }");						// check that symbol is defined
	SLiMAssertScriptStop(gen1_setup + "1 { x = sim.addSubpop(7, 10); if (x == p7) stop(); }");							// check that symbol is defined
	SLiMAssertScriptRaise(gen1_setup + "1 { p7 = 17; sim.addSubpop('p7', 10); stop(); }", 1, 229);						// symbol collision
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.addSubpop('p7', 10); sim.addSubpop(7, 10); stop(); }", 1, 245);			// same id defined twice
	
	// Test sim - (object<Subpopulation>)addSubpopSplit(is$ subpopID, integer$ size, object<Subpopulation>$ sourceSubpop, [float$ sexRatio])
	std::string gen1_setup_p1(gen1_setup + "1 { sim.addSubpop('p1', 10); } ");
	std::string gen1_setup_sex_p1(gen1_setup_sex + "1 { sim.addSubpop('p1', 10); } ");
	
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit('p2', 10, p1); } " + gen2_stop);										// legal with subpop string
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, p1); } " + gen2_stop);											// legal with subpop id
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit('p2', 10, p1, 0.5); } " + gen2_stop, 1, 251);							// sex ratio supplied in non-sexual simulation
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit(2, 10, p1, 0.5); } " + gen2_stop, 1, 251);							// sex ratio supplied in non-sexual simulation
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { sim.addSubpopSplit('p2', 10, p1, 0.5); } " + gen2_stop);								// legal
	SLiMAssertScriptStop(gen1_setup_sex_p1 + "1 { sim.addSubpopSplit(2, 10, p1, 0.5); } " + gen2_stop);									// legal
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { x = sim.addSubpopSplit('p7', 10, p1); if (x == p7) stop(); }");							// check that symbol is defined
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { x = sim.addSubpopSplit(7, 10, p1); if (x == p7) stop(); }");								// check that symbol is defined
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { p7 = 17; sim.addSubpopSplit('p7', 10, p1); stop(); }", 1, 260);							// symbol collision
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.addSubpopSplit('p7', 10, p1); sim.addSubpopSplit(7, 10, p1); stop(); }", 1, 285);	// same id defined twice
	
	// Test sim - (void)deregisterScriptBlock(object<SLiMEidosBlock> scriptBlocks)
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(s1); } s1 2 { stop(); }");										// legal
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(object()); } s1 2 { stop(); }");									// legal: deregister nothing
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(s1, s1)); } s1 2 { stop(); }", 1, 251);							// double deregister
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(s1); sim.deregisterScriptBlock(s1); } s1 2 { stop(); }", 1, 282);	// double deregister
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.deregisterScriptBlock(c(s1, s2)); } s1 2 { stop(); } s2 3 { stop(); }");				// legal
	
	// Test sim - (float)mutationFrequencies(No<Subpopulation> subpops, [object<Mutation> mutations])
	std::string gen1_setup_p1p2p3(gen1_setup + "1 { sim.addSubpop('p1', 10); sim.addSubpop('p2', 10); sim.addSubpop('p3', 10); } ");
	
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(p1); }");					// legal
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(c(p1, p2)); }");			// legal
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(NULL); }");				// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(sim.subpopulations); }");	// legal, requests population-wide frequencies
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.mutationFrequencies(object()); }");			// legal to specify an empty object vector
	
	// Test sim - (void)outputFixedMutations(void)
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.outputFixedMutations(); }");				// legal
	
	// Test sim - (void)outputFull([string$ filePath])
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.outputFull(); }");									// legal
	SLiMAssertScriptSuccess(gen1_setup_p1p2p3 + "1 { sim.outputFull('/tmp/slimOutputFullTest.txt'); }");	// legal, output to file path; this test might work only on Un*x systems
	
	// Test sim - (void)outputMutations(object<Mutation> mutations)
	std::string gen1_setup_highmut_p1("initialize() { initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } ");
	
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 { sim.outputMutations(sim.mutations); }");						// legal; should have some mutations by gen 5
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 { sim.outputMutations(sim.mutations[0]); }");					// legal; output just one mutation
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 { sim.outputMutations(sim.mutations[integer(0)]); }");			// legal to specify an empty object vector
	SLiMAssertScriptSuccess(gen1_setup_highmut_p1 + "5 { sim.outputMutations(object()); }");							// legal to specify an empty object vector
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "5 { sim.outputMutations(NULL); }", 1, 251);							// NULL not allowed
	
	// Test - (void)readFromPopulationFile(string$ filePath)
	SLiMAssertScriptSuccess(gen1_setup + "1 { sim.readFromPopulationFile('/tmp/slimOutputFullTest.txt'); }");												// legal, read from file path; depends on the outputFull() test above
	SLiMAssertScriptRaise(gen1_setup + "1 { sim.readFromPopulationFile('/tmp/notAFile.foo'); }", 1, 220);													// no file at path
	SLiMAssertScriptSuccess(gen1_setup_p1 + "1 { sim.readFromPopulationFile('/tmp/slimOutputFullTest.txt'); if (size(sim.subpopulations) != 3) stop(); }");	// legal; should wipe previous state
	
	// Test sim - (object<SLiMEidosBlock>)registerScriptEvent(Nis$ id, string$ source, [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerScriptEvent(NULL, '{ stop(); }', 2, 2); }");														// legal
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerScriptEvent('s1', '{ stop(); }', 2, 2); }", 1, 259);										// symbol collision
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerScriptEvent(1, '{ stop(); }', 2, 2); }", 1, 259);										// symbol collision
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptEvent(1, '{ stop(); }', 2, 2); registerScriptEvent(1, '{ stop(); }', 2, 2); }", 1, 296);	// same id defined twice
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptEvent(1, '{ stop(); }', 3, 2); }", 1, 251);												// start < end
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptEvent(1, '{ stop(); }', -1, -1); }", 1, 251);												// generation -1
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptEvent(1, '{ stop(); }', 0, 0); }", 1, 251);												// generation 0
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptEvent(1, '{ $; }', 2, 2); }", 1, 2);														// syntax error inside block
	
	// Test sim - (object<SLiMEidosBlock>)registerScriptFitnessCallback(Nis$ id, string$ source, io<MutationType>$ mutType, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerScriptFitnessCallback(NULL, '{ stop(); }', 1, NULL, 5, 10); }");																				// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerScriptFitnessCallback(NULL, '{ stop(); }', m1, NULL, 5, 10); }");																				// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerScriptFitnessCallback(NULL, '{ stop(); }', 1, 1, 5, 10); }");																					// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerScriptFitnessCallback(NULL, '{ stop(); }', m1, p1, 5, 10); }");																				// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerScriptFitnessCallback(NULL, '{ stop(); }', 1); } 10 { ; }");																					// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "1 { sim.registerScriptFitnessCallback(NULL, '{ stop(); }', m1); } 10 { ; }");																					// legal
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerScriptFitnessCallback(NULL, '{ stop(); }'); }", 1, 251);																						// missing param
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { s1 = 7; sim.registerScriptFitnessCallback('s1', '{ stop(); }', m1, NULL, 2, 2); }", 1, 259);																// symbol collision
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { s1 = 7; sim.registerScriptFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); }", 1, 259);																// symbol collision
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerScriptFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); sim.registerScriptFitnessCallback(1, '{ stop(); }', m1, NULL, 2, 2); }", 1, 320);	// same id defined twice
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerScriptFitnessCallback(1, '{ stop(); }', m1, NULL, 3, 2); }", 1, 251);																		// start < end
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerScriptFitnessCallback(1, '{ stop(); }', m1, NULL, -1, -1); }", 1, 251);																		// generation -1
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerScriptFitnessCallback(1, '{ stop(); }', m1, NULL, 0, 0); }", 1, 251);																		// generation 0
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "1 { sim.registerScriptFitnessCallback(1, '{ $; }', m1, NULL, 2, 2); }", 1, 2);																				// syntax error inside block
	
	// Test sim - (object<SLiMEidosBlock>)registerScriptMateChoiceCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerScriptMateChoiceCallback(NULL, '{ stop(); }', NULL, 2, 2); }");																				// legal
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerScriptMateChoiceCallback(NULL, '{ stop(); }', NULL, 2, 2); }");																				// legal
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerScriptMateChoiceCallback(NULL, '{ stop(); }', 1, 2, 2); }");																					// legal
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerScriptMateChoiceCallback(NULL, '{ stop(); }', p1, 2, 2); }");																					// legal
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerScriptMateChoiceCallback(NULL, '{ stop(); }'); }");																							// legal
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerScriptMateChoiceCallback(NULL, '{ stop(); }'); }");																							// legal
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptMateChoiceCallback(NULL); }", 1, 251);																									// missing param
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerScriptMateChoiceCallback('s1', '{ stop(); }', NULL, 2, 2); }", 1, 259);																// symbol collision
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerScriptMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 259);																	// symbol collision
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); sim.registerScriptMateChoiceCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 319);		// same id defined twice
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptMateChoiceCallback(1, '{ stop(); }', NULL, 3, 2); }", 1, 251);																			// start < end
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptMateChoiceCallback(1, '{ stop(); }', NULL, -1, -1); }", 1, 251);																		// generation -1
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptMateChoiceCallback(1, '{ stop(); }', NULL, 0, 0); }", 1, 251);																			// generation 0
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptMateChoiceCallback(1, '{ $; }', NULL, 2, 2); }", 1, 2);																				// syntax error inside block
	
	// Test sim - (object<SLiMEidosBlock>)registerScriptModifyChildCallback(Nis$ id, string$ source, [Nio<Subpopulation>$ subpop], [integer$ start], [integer$ end])
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerScriptModifyChildCallback(NULL, '{ stop(); }', NULL, 2, 2); }");																				// legal
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerScriptModifyChildCallback(NULL, '{ stop(); }', NULL, 2, 2); }");																				// legal
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerScriptModifyChildCallback(NULL, '{ stop(); }', 1, 2, 2); }");																					// legal
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerScriptModifyChildCallback(NULL, '{ stop(); }', p1, 2, 2); }");																				// legal
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerScriptModifyChildCallback(NULL, '{ stop(); }'); }");																							// legal
	SLiMAssertScriptStop(gen1_setup_p1 + "1 { sim.registerScriptModifyChildCallback(NULL, '{ stop(); }'); }");																							// legal
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptModifyChildCallback(NULL); }", 1, 251);																								// missing param
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerScriptModifyChildCallback('s1', '{ stop(); }', NULL, 2, 2); }", 1, 259);																// symbol collision
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { s1 = 7; sim.registerScriptModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 259);																// symbol collision
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); sim.registerScriptModifyChildCallback(1, '{ stop(); }', NULL, 2, 2); }", 1, 320);	// same id defined twice
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptModifyChildCallback(1, '{ stop(); }', NULL, 3, 2); }", 1, 251);																		// start < end
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptModifyChildCallback(1, '{ stop(); }', NULL, -1, -1); }", 1, 251);																		// generation -1
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptModifyChildCallback(1, '{ stop(); }', NULL, 0, 0); }", 1, 251);																		// generation 0
	SLiMAssertScriptRaise(gen1_setup_p1 + "1 { sim.registerScriptModifyChildCallback(1, '{ $; }', NULL, 2, 2); }", 1, 2);																				// syntax error inside block
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: MutationType
	//
	
	// Test MutationType properties
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.distributionParams == 0.0) stop(); }");		// legal
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.distributionType == 'f') stop(); }");			// legal
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.dominanceCoeff == 0.5) stop(); }");			// legal
	SLiMAssertScriptStop(gen1_setup + "1 { if (m1.id == 1) stop(); }");							// legal
	SLiMAssertScriptStop(gen1_setup + "1 { m1.tag = 17; } 2 { if (m1.tag == 17) stop(); }");	// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.distributionParams = 0.1; }", 1, 238);			// setting read-only property
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.distributionType = 'g'; }", 1, 236);				// setting read-only property
	SLiMAssertScriptSuccess(gen1_setup + "1 { m1.dominanceCoeff = 0.3; }");						// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.id = 2; }", 1, 222);								// setting read-only property
	
	// Test MutationType - (void)setDistribution(string$ distributionType, ...)
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('f', 2.2); if (m1.distributionType == 'f' & m1.distributionParams == 2.2) stop(); }");						// legal
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('g', 3.1, 7.5); if (m1.distributionType == 'g' & identical(m1.distributionParams, c(3.1, 7.5))) stop(); }");	// legal
	SLiMAssertScriptStop(gen1_setup + "1 { m1.setDistribution('e', -3); if (m1.distributionType == 'e' & m1.distributionParams == -3) stop(); }");							// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('x', 1.5); stop(); }", 1, 219);																				// no such DFE
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', 'foo'); stop(); }", 1, 219);																			// DFE params must be numeric
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 'foo', 7.5); stop(); }", 1, 219);																		// DFE params must be numeric
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, 'foo'); stop(); }", 1, 219);																		// DFE params must be numeric
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', 'foo'); stop(); }", 1, 219);																			// DFE params must be numeric
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', '1'); stop(); }", 1, 219);																				// DFE params must be numeric
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', '1', 7.5); stop(); }", 1, 219);																			// DFE params must be numeric
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, '1'); stop(); }", 1, 219);																			// DFE params must be numeric
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', '1'); stop(); }", 1, 219);																				// DFE params must be numeric
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('f', T); stop(); }", 1, 219);																				// DFE params must be numeric
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', T, 7.5); stop(); }", 1, 219);																			// DFE params must be numeric
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('g', 3.1, T); stop(); }", 1, 219);																			// DFE params must be numeric
	SLiMAssertScriptRaise(gen1_setup + "1 { m1.setDistribution('e', T); stop(); }", 1, 219);																				// DFE params must be numeric
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: GenomicElementType
	//
	
	// Test GenomicElementType properties
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.id == 1) stop(); }");							// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.id = 2; }", 1, 222);								// setting read-only property
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.mutationFractions == 1.0) stop(); }");		// legal
	SLiMAssertScriptStop(gen1_setup + "1 { if (g1.mutationTypes == m1) stop(); }");				// legal
	SLiMAssertScriptStop(gen1_setup + "1 { m1.tag = 17; } 2 { if (m1.tag == 17) stop(); }");	// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.mutationFractions = 1.0; }", 1, 237);			// setting read-only property
	SLiMAssertScriptRaise(gen1_setup + "1 { g1.mutationTypes = m1; }", 1, 233);					// setting read-only property
	
	// Test GenomicElementType - (void)setMutationFractions(io<MutationType> mutationTypes, numeric proportions)
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(object(), integer(0)); stop(); }");						// legal
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(m1, 0.0); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.0) stop(); }");								// legal
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(1, 0.0); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.0) stop(); }");								// legal
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(m1, 0.3); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.3) stop(); }");								// legal
	SLiMAssertScriptStop(gen1_setup + "1 { g1.setMutationFractions(1, 0.3); if (g1.mutationTypes == m1 & g1.mutationFractions == 0.3) stop(); }");								// legal
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2), c(0.3, 0.7)); if (identical(g1.mutationTypes, c(m1,m2)) & identical(g1.mutationFractions, c(0.3,0.7))) stop(); }");						// legal
	SLiMAssertScriptStop(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(1,2), c(0.3, 0.7)); if (identical(g1.mutationTypes, c(m1,m2)) & identical(g1.mutationFractions, c(0.3,0.7))) stop(); }");						// legal
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2)); stop(); }", 1, 281);				// missing param
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2), 0.3); stop(); }", 1, 281);			// proportions count wrong
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m1,m2), c(-1, 2)); stop(); }", 1, 281);		// proportion is negative
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(2,3), c(1, 2)); stop(); }", 1, 281);		// reference to undefined mutation type
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(m2,m2), c(1, 2)); stop(); }", 1, 281);		// reference to a mutation type more than once
	SLiMAssertScriptRaise(gen1_setup + "initialize() { initializeMutationType('m2', 0.7, 'e', 0.5); } 1 { g1.setMutationFractions(c(2,2), c(1, 2)); stop(); }", 1, 281);		// reference to a mutation type more than once
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: GenomicElement
	//
	
	std::string gen1_setup_2ge("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 999); initializeGenomicElement(g1, 1000, 99999); initializeRecombinationRate(1e-8); } ");
	
	// Test GenomicElement properties
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; if (ge.endPosition == 999) stop(); }");				// legal
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; if (ge.startPosition == 0) stop(); }");				// legal
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; if (ge.genomicElementType == g1) stop(); }");		// legal
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.tag = -12; if (ge.tag == -12) stop(); }");		// legal
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.endPosition = 999; stop(); }", 1, 312);			// setting read-only property
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.startPosition = 0; stop(); }", 1, 314);			// setting read-only property
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.genomicElementType = g1; stop(); }", 1, 319);	// setting read-only property
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.endPosition == 99999) stop(); }");			// legal
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.startPosition == 1000) stop(); }");			// legal
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; if (ge.genomicElementType == g1) stop(); }");		// legal
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.tag = -17; if (ge.tag == -17) stop(); }");		// legal
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.endPosition = 99999; stop(); }", 1, 312);		// setting read-only property
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.startPosition = 1000; stop(); }", 1, 314);		// setting read-only property
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[1]; ge.genomicElementType = g1; stop(); }", 1, 319);	// setting read-only property
	
	// Test GenomicElement - (void)setGenomicElementType(io<GenomicElementType>$ genomicElementType)
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(g1); stop(); }");					// legal
	SLiMAssertScriptStop(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(1); stop(); }");					// legal
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(); stop(); }", 1, 300);			// missing parameter
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(object()); stop(); }", 1, 300);	// missing object
	SLiMAssertScriptRaise(gen1_setup_2ge + "1 { ge = sim.chromosome.genomicElements[0]; ge.setGenomicElementType(2); stop(); }", 1, 300);			// undefined genomic element
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: Chromosome
	//
	
	// Test Chromosome properties
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionFraction == 0.0) stop(); }");					// legal
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.geneConversionMeanLength == 0.0) stop(); }");				// legal
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.genomicElements[0].genomicElementType == g1) stop(); }");	// legal
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.lastPosition == 99999) stop(); }");							// legal
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.overallMutationRate == 1e-7) stop(); }");					// legal
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.overallRecombinationRate == 1e-8 * 100000) stop(); }");		// legal
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.recombinationEndPositions == 99999) stop(); }");				// legal
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; if (ch.recombinationRates == 1e-8) stop(); }");						// legal
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.tag = 3294; if (ch.tag == 3294) stop(); }");						// legal
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionFraction = 0.1; if (ch.geneConversionFraction == 0.1) stop(); }");			// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionFraction = -0.001; stop(); }", 1, 263);									// value out of range
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionFraction = 1.001; stop(); }", 1, 263);									// value out of range
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 0.2; if (ch.geneConversionMeanLength == 0.2) stop(); }");		// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 0.0; stop(); }", 1, 265);									// value out of range
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.geneConversionMeanLength = 1e10; stop(); }");											// legal; no upper bound
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.genomicElements = ch.genomicElements; stop(); }", 1, 256);								// setting read-only property
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.lastPosition = 99999; stop(); }", 1, 253);												// setting read-only property
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.overallMutationRate = 1e-6; if (ch.overallMutationRate == 1e-6) stop(); }");				// legal
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallMutationRate = -1e-6; stop(); }", 1, 260);										// value out of range
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.overallMutationRate = 1e6; stop(); }");													// legal; no upper bound
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.overallRecombinationRate = 1e-2; stop(); }", 1, 265);									// setting read-only property
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationEndPositions = 99999; stop(); }", 1, 266);									// setting read-only property
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.recombinationRates = 1e-8; stop(); }", 1, 259);											// setting read-only property
	
	// Test Chromosome - (void)setRecombinationRate(numeric rates, [integer ends])
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(0.0); stop(); }");										// legal: singleton rate, no end
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(); stop(); }", 1, 240);								// missing param
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(-0.00001); stop(); }", 1, 240);						// rate out of range
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(10000); stop(); }");									// legal; no maximum rate
	SLiMAssertScriptStop(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(1000, 99999)); stop(); }");				// legal: multiple rates, matching ends
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1)); stop(); }", 1, 240);						// missing param
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(integer(0), integer(0)); stop(); }", 1, 240);			// length zero params
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99999); stop(); }", 1, 240);				// length mismatch
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), 99997:99999); stop(); }", 1, 240);		// length mismatch
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 1000)); stop(); }", 1, 240);		// ends out of order
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, 0.1), c(99999, 99999)); stop(); }", 1, 240);	// identical ends
	SLiMAssertScriptRaise(gen1_setup + "1 { ch = sim.chromosome; ch.setRecombinationRate(c(0.0, -0.001), c(1000, 99999)); stop(); }", 1, 240);	// rate out of range
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: Mutation
	//
	
	// Test Mutation properties
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if (mut.mutationType == m1) stop(); }");											// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if ((mut.originGeneration >= 1) & (mut.originGeneration < 10)) stop(); }");		// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if ((mut.position >= 0) & (mut.position < 100000)) stop(); }");					// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if (mut.selectionCoeff == 0.0) stop(); }");										// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; if (mut.subpopID == 1) stop(); }");												// legal
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.mutationType = m1; stop(); }", 1, 289);										// setting read-only property
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.originGeneration = 1; stop(); }", 1, 293);									// setting read-only property
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.position = 0; stop(); }", 1, 285);											// setting read-only property
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.selectionCoeff = 0.1; stop(); }", 1, 291);									// setting read-only property
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.subpopID = 2; stop(); }", 1, 285);											// setting read-only property
	
	// Test Mutation - (void)setSelectionCoeff(float$ selectionCoeff)
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(0.5); if (mut.selectionCoeff == 0.5) stop(); }");			// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(-500.0); if (mut.selectionCoeff == -500.0) stop(); }");	// legal; no lower bound
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { mut = sim.mutations[0]; mut.setSelectionCoeff(500.0); if (mut.selectionCoeff == 500.0) stop(); }");		// legal; no upper bound
	
	
	// ************************************************************************************
	//
	//	Gen 1+ tests: Genome
	//
	
	//std::string gen1_setup_highmut_p1("initialize() { initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } ");
	
	// Test Genome properties
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; if (gen.genomeType == 'A') stop(); }");						// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; if (gen.isNullGenome == F) stop(); }");						// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; if (gen.mutations[0].mutationType == m1) stop(); }");		// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.tag = 278; if (gen.tag == 278) stop(); }");				// legal
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.genomeType = 'A'; stop(); }", 1, 284);					// legal
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.isNullGenome = F; stop(); }", 1, 286);					// legal
	SLiMAssertScriptRaise(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.mutations[0].mutationType = m1; stop(); }", 1, 299);	// legal
	
	// Test Genome - (void)addMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.addMutations(object()); stop(); }");																				// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.addMutations(gen.mutations[0]); stop(); }");																		// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.addMutations(p1.genomes[1].mutations[0]); stop(); }");																// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = p1.genomes[1].mutations[0]; gen.addMutations(rep(mut, 10)); if (sum(gen.mutations == mut) == 1) stop(); }");		// legal
	
	// Test Genome - (object<Mutation>)addNewDrawnMutation(io<MutationType>$ mutationType, Ni$ originGeneration, integer$ position, io<Subpopulation>$ originSubpop)
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 10, 5000, p1); p1.genomes.addMutations(mut); stop(); }");								// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(m1, 10, 5000, 1); p1.genomes.addMutations(mut); stop(); }");								// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 10, 5000, p1); p1.genomes.addMutations(mut); stop(); }");								// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, 10, 5000, 1); p1.genomes.addMutations(mut); stop(); }");								// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = gen.addNewDrawnMutation(1, NULL, 5000, 1); p1.genomes.addMutations(mut); stop(); }");								// legal
	
	// Test Genome - (object<Mutation>)addNewMutation(io<MutationType>$ mutationType, Ni$ originGeneration, integer$ position, numeric$ selectionCoeff, io<Subpopulation>$ originSubpop)
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 10, 5000, 0.1, p1); p1.genomes.addMutations(mut); stop(); }");								// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 10, 5000, 0.1, 1); p1.genomes.addMutations(mut); stop(); }");								// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 10, 5000, 0.1, p1); p1.genomes.addMutations(mut); stop(); }");								// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, 10, 5000, 0.1, 1); p1.genomes.addMutations(mut); stop(); }");								// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = gen.addNewMutation(1, NULL, 5000, 0.1, 1); p1.genomes.addMutations(mut); stop(); }");								// legal
	
	// Test Genome - (void)removeMutations(object<Mutation> mutations)
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; mut = gen.addNewMutation(m1, 10, 5000, 0.1, p1); gen.removeMutations(mut); stop(); }");									// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.removeMutations(object()); stop(); }");																				// legal
	SLiMAssertScriptStop(gen1_setup_highmut_p1 + "10 { gen = p1.genomes[0]; gen.removeMutations(gen.mutations); stop(); }");																		// legal
	
	
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































































