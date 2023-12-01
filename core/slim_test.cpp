//
//  slim_test.cpp
//  SLiM
//
//  Created by Ben Haller on 8/14/15.
//  Copyright (c) 2015-2023 Philipp Messer.  All rights reserved.
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


#include "slim_test.h"
#include "community.h"
#include "species.h"
#include "eidos_test.h"
#include "individual.h"
#include "mutation_run.h"
#include "interaction_type.h"

#include <stdlib.h>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <unordered_map>
#include <map>
#include <utility>
#include <ctime>


// Keeping records of test success / failure
static int gSLiMTestSuccessCount = 0;
static int gSLiMTestFailureCount = 0;


// Instantiates and runs the script, and prints an error if the result does not match expectations
void SLiMAssertScriptSuccess(const std::string &p_script_string, int p_lineNumber)
{
	{
	gSLiMTestFailureCount++;	// assume failure; we will fix this at the end if we succeed
	
	Community *community = nullptr;
	std::istringstream infile(p_script_string);
	
	try {
		community = new Community();
		community->InitializeFromFile(infile);
		community->InitializeRNGFromSeed(nullptr);
	}
	catch (...)
	{
		if (p_lineNumber != -1)
			std::cerr << "[" << p_lineNumber << "] ";
		
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during new Community(): " << Eidos_GetTrimmedRaiseMessage() << std::endl;
		
		gEidosErrorContext.currentScript = nullptr;
		gEidosErrorContext.executingRuntimeScript = false;
		return;
	}
	
	try {
		while (community->_RunOneTick());
	}
	catch (...)
	{
		if (community)
			for (Species *species : community->AllSpecies())
				species->DeleteAllMutationRuns();
		
		delete community;
		InteractionType::DeleteSparseVectorFreeList();
		
		if (p_lineNumber != -1)
			std::cerr << "[" << p_lineNumber << "] ";
		
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during RunOneTick(): " << Eidos_GetTrimmedRaiseMessage() << std::endl;
		
		gEidosErrorContext.currentScript = nullptr;
		gEidosErrorContext.executingRuntimeScript = false;
		return;
	}
	
	if (community)
		for (Species *species : community->AllSpecies())
			species->DeleteAllMutationRuns();
	
	delete community;
	InteractionType::DeleteSparseVectorFreeList();
	
	gSLiMTestFailureCount--;	// correct for our assumption of failure above
	gSLiMTestSuccessCount++;
	
	//std::cerr << p_script_string << " : " << EIDOS_OUTPUT_SUCCESS_TAG << endl;
	
	gEidosErrorContext.currentScript = nullptr;
	gEidosErrorContext.executingRuntimeScript = false;
	
	if (gEidos_DictionaryNonRetainReleaseReferenceCounter > 0)
		std::cerr << "WARNING (SLiMAssertScriptSuccess): gEidos_DictionaryNonRetainReleaseReferenceCounter == " << gEidos_DictionaryNonRetainReleaseReferenceCounter << " at end of test!" << std::endl;
	}
	
	gEidos_DictionaryNonRetainReleaseReferenceCounter = 0;
}

void SLiMAssertScriptRaise(const std::string &p_script_string, const std::string &p_reason_snip, int p_lineNumber, bool p_expect_error_position)
{
	{
	Community *community = nullptr;
	
	try {
		std::istringstream infile(p_script_string);
		
		community = new Community();
		community->InitializeFromFile(infile);
		community->InitializeRNGFromSeed(nullptr);
		
		while (community->_RunOneTick());
		
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
				if ((gEidosErrorContext.errorPosition.characterStartOfError == -1) ||
					(gEidosErrorContext.errorPosition.characterEndOfError == -1) ||
					!gEidosErrorContext.currentScript)
				{
					if (!p_expect_error_position)
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
					if (!p_expect_error_position)
						std::cerr << p_script_string << " : " << EIDOS_OUTPUT_SUCCESS_TAG << " : raise expected, and error info is set; but error info was not expected!" << std::endl;
					
					Eidos_ScriptErrorPosition(gEidosErrorContext);
					
					gSLiMTestSuccessCount++;
					
					//std::cerr << p_script_string << " == (expected raise) : " << EIDOS_OUTPUT_SUCCESS_TAG << "\n   " << raise_message << endl;
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
	
	if (community)
		for (Species *species : community->AllSpecies())
			species->DeleteAllMutationRuns();
	
	delete community;
	InteractionType::DeleteSparseVectorFreeList();
	
	gEidosErrorContext.currentScript = nullptr;
	gEidosErrorContext.executingRuntimeScript = false;
	
	if (gEidos_DictionaryNonRetainReleaseReferenceCounter > 0)
		std::cerr << "WARNING (SLiMAssertScriptRaise): gEidos_DictionaryNonRetainReleaseReferenceCounter == " << gEidos_DictionaryNonRetainReleaseReferenceCounter << " at end of test!" << std::endl;
	}
	
	gEidos_DictionaryNonRetainReleaseReferenceCounter = 0;
}

void SLiMAssertScriptStop(const std::string &p_script_string, int p_lineNumber)
{
	{
	Community *community = nullptr;
	
	try {
		std::istringstream infile(p_script_string);
		
		community = new Community();
		community->InitializeFromFile(infile);
		community->InitializeRNGFromSeed(nullptr);
		
		while (community->_RunOneTick());
		
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
			
			if ((gEidosErrorContext.errorPosition.characterStartOfError != -1) &&
				(gEidosErrorContext.errorPosition.characterEndOfError != -1) &&
				gEidosErrorContext.currentScript)
			{
				Eidos_ScriptErrorPosition(gEidosErrorContext);
				Eidos_LogScriptError(std::cerr, gEidosErrorContext);
			}
			
			std::cerr << "--------------------" << std::endl << std::endl;
		}
		else
		{
			gSLiMTestSuccessCount++;
			
			//std::cerr << p_script_string << " == (expected raise) " << raise_message << " : " << EIDOS_OUTPUT_SUCCESS_TAG << endl;
		}
	}
	
	if (community)
		for (Species *species : community->AllSpecies())
			species->DeleteAllMutationRuns();
	
	delete community;
	InteractionType::DeleteSparseVectorFreeList();
	
	gEidosErrorContext.currentScript = nullptr;
	gEidosErrorContext.executingRuntimeScript = false;
	
	if (gEidos_DictionaryNonRetainReleaseReferenceCounter > 0)
		std::cerr << "WARNING (SLiMAssertScriptStop): gEidos_DictionaryNonRetainReleaseReferenceCounter == " << gEidos_DictionaryNonRetainReleaseReferenceCounter << " at end of test!" << std::endl;
	}
	
	gEidos_DictionaryNonRetainReleaseReferenceCounter = 0;
}


// Test subfunction prototypes
static void _RunBasicTests(void);
static void _RunSLiMTimingTests(void);


// Test function shared strings
std::string gen1_setup("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } ");
std::string gen1_setup_sex("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeSex('X'); } ");
std::string gen2_stop(" 2 early() { stop(); } ");
std::string gen1_setup_highmut_p1("initialize() { initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 early() { sim.addSubpop('p1', 10); } ");
std::string gen1_setup_fixmut_p1("initialize() { initializeMutationRate(1e-4); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 early() { sim.addSubpop('p1', 10); } 10 early() { sim.mutations[0].setSelectionCoeff(500.0); sim.recalculateFitness(); } ");
std::string gen1_setup_i1("initialize() { initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', ''); } 1 early() { sim.addSubpop('p1', 10); } 1:10 late() { } ");
std::string gen1_setup_i1x("initialize() { initializeSLiMOptions(dimensionality='x'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'x'); } 1 early() { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); } ");
std::string gen1_setup_i1xPx("initialize() { initializeSLiMOptions(dimensionality='x', periodicity='x'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'x'); } 1 early() { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); } ");
std::string gen1_setup_i1xy("initialize() { initializeSLiMOptions(dimensionality='xy'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xy'); } 1 early() { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); p1.individuals.y = runif(10); } ");
std::string gen1_setup_i1xyPxy("initialize() { initializeSLiMOptions(dimensionality='xy', periodicity='xy'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xy'); } 1 early() { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); p1.individuals.y = runif(10); } ");
std::string gen1_setup_i1xyz("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xyz'); } 1 early() { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); p1.individuals.y = runif(10); p1.individuals.z = runif(10); } ");
std::string gen1_setup_i1xyzPxz("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='xz'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xyz'); } 1 early() { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); p1.individuals.y = runif(10); p1.individuals.z = runif(10); } ");
std::string gen1_setup_p1(gen1_setup + "1 early() { sim.addSubpop('p1', 10); } ");
std::string gen1_setup_p1_100(gen1_setup + "1 early() { sim.addSubpop('p1', 100); } ");
std::string gen1_setup_sex_p1(gen1_setup_sex + "1 early() { sim.addSubpop('p1', 10); } ");
std::string gen1_setup_sex_p1_100(gen1_setup_sex + "1 early() { sim.addSubpop('p1', 100); } ");
std::string gen1_setup_p1p2p3(gen1_setup + "1 early() { sim.addSubpop('p1', 10); sim.addSubpop('p2', 10); sim.addSubpop('p3', 10); } ");
std::string gen1_setup_p1p2p3_100(gen1_setup + "1 early() { sim.addSubpop('p1', 100); sim.addSubpop('p2', 100); sim.addSubpop('p3', 100); } ");

std::string WF_prefix("initialize() { initializeSLiMModelType('WF'); } ");
std::string nonWF_prefix("initialize() { initializeSLiMModelType('nonWF'); } ");

std::string pedigrees_prefix("initialize() { initializeSLiMOptions(keepPedigrees=T); } ");


int RunSLiMTests(void)
{
	// This function should never be called when parallel, but individual tests are allowed to go parallel internally
	THREAD_SAFETY_IN_ANY_PARALLEL("RunSLiMTests(): illegal when parallel");
	
	// Test SLiM.  The goal here is not really to test that the core code of SLiM is working properly – that simulations
	// work as they are intended to.  Such testing is beyond the scope of what we can do here.  Instead, the goal here
	// is to test all of the Eidos-related APIs in SLiM – to make sure that all properties, methods, and functions in
	// SLiM's Eidos interface work properly.  SLiM itself will get a little incidental testing along the way.
	
	if (!Eidos_TemporaryDirectoryExists())
		std::cout << "WARNING: This system does not appear to have a writeable temporary directory.  Filesystem tests are disabled, and functions such as writeTempFile() and system() that depend upon the existence of the temporary directory will raise an exception if called (and are therefore also not tested).  Other self-tests that rely on writing temporary files, such as of readCSV() and Image, will also be disabled.  If this is surprising, contact the system administrator for details." << std::endl;
	
	// We want to run the self-test inside a new temporary directory, to prevent collisions with other self-test runs
	std::string prefix = Eidos_TemporaryDirectory() + "slimTest_";
	std::string temp_path_template = prefix + "XXXXXX";
	char *temp_path_cstr = strdup(temp_path_template.c_str());
	
	if (Eidos_mkstemps_directory(temp_path_cstr, 0) == 0)
	{
		//std::cout << "Running SLiM self-tests in " << temp_path_cstr << " ..." << std::endl;
	}
	else
	{
		std::cout << "A folder within the temporary directory could not be created; there may be a permissions problem with the temporary directory.  The self-test could not be run." << std::endl;
		return 1;
	}
	
	std::string temp_path(temp_path_cstr);	// the final random path generated by Eidos_mkstemps_directory
	free(temp_path_cstr);
	
	// Reset error counts
	gSLiMTestSuccessCount = 0;
	gSLiMTestFailureCount = 0;
	
	// Run tests
	_RunBasicTests();
	_RunRelatednessTests();
	_RunInitTests();
	_RunCommunityTests();
	_RunSpeciesTests(temp_path);
	_RunMutationTypeTests();
	_RunGenomicElementTypeTests();
	_RunGenomicElementTests();
	_RunChromosomeTests();
	_RunMutationTests();
	_RunGenomeTests(temp_path);
	_RunSubpopulationTests();
	_RunIndividualTests();
	_RunSubstitutionTests();
	_RunSLiMEidosBlockTests();
	_RunContinuousSpaceTests();
	_RunSpatialMapTests();
	_RunNonWFTests();
	_RunTreeSeqTests(temp_path);
	_RunNucleotideFunctionTests();
	_RunNucleotideMethodTests();
	_RunSLiMTimingTests();
	
#ifdef _OPENMP
	_RunParallelSLiMTests();
#endif
	
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
	
	// Clear out the SLiM output streams post-test
	gSLiMOut.clear();
	gSLiMOut.str("");
	
	gSLiMError.clear();
	gSLiMError.str("");
	
#ifdef SLIMGUI
	gSLiMScheduling.clear();
	gSLiMScheduling.str("");
#endif
	
	// return a standard Unix result code indicating success (0) or failure (1);
	return (gSLiMTestFailureCount > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
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
							 1 early() { sim.addSubpop('p1', 500); }
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
						  1 early() { sim.addSubpop('p1', 500); }
						  3 early() { stop(); }
						  5 late() { sim.outputFull(); }
						  
						  )V0G0N");
	
	SLiMAssertScriptStop(stop_test);
	
	// Test script registration
	SLiMAssertScriptStop("initialize() { stop(); } s1 early() {}", __LINE__);
	SLiMAssertScriptRaise("initialize() { stop(); } s1 early() {} s1 early() {}", "already defined", __LINE__);
	SLiMAssertScriptStop("initialize() { stop(); } 1: early() {}", __LINE__);
	SLiMAssertScriptStop("initialize() { stop(); } :1 early() {}", __LINE__);
	SLiMAssertScriptStop("initialize() { stop(); } 1:10 early() {}", __LINE__);
	SLiMAssertScriptRaise("initialize() { stop(); } : early() {}", "unexpected token", __LINE__);
	
	// Test top-level structure parsing; this was adding for SLiM 4 due to the multispecies revisions
	// If these tests do not stop for other reasons, they produce a raise due to incorrect initialization, "At least one mutation rate interval must be defined..."
	SLiMAssertScriptStop("initialize() {} 1 early() { stop(); }", __LINE__);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(0.0); } 1 early() {}", "mutation rate interval", __LINE__, false);
	SLiMAssertScriptRaise("initialize() { initializeRecombinationRate(0.0); } initialize() {} 1 early() {}", "mutation rate interval", __LINE__, false);
	SLiMAssertScriptRaise("initialize() {} initialize() {} ticks fox 1 early() {}", "undeclared species", __LINE__);
	SLiMAssertScriptRaise("initialize() {} initialize() {} species fox 1 early() {}", "preceded by a species", __LINE__);
	SLiMAssertScriptRaise("initialize() {} initialize() {} ticks fox mutationEffect(m1) {}", "preceded by a ticks", __LINE__);
	SLiMAssertScriptRaise("initialize() {} initialize() {} species fox mutationEffect(m1) {}", "undeclared species", __LINE__);
	SLiMAssertScriptRaise("species all initialize() {} species fox initialize() { initializeRecombinationRate(0.0); } ticks all 1 early() {}", "mutation rate interval", __LINE__, false);
	SLiMAssertScriptRaise("species all initialize() {} species fox initialize() {} 1 early() {}", "preceded by a ticks", __LINE__);
	SLiMAssertScriptRaise("species all initialize() {} ticks all 1 early() { stop(); }", "no species-specific initialize() callback found", __LINE__, false);
	SLiMAssertScriptRaise("species fox initialize() {} initialize() {} 1 early() {}", "species specifiers are required", __LINE__);
	SLiMAssertScriptRaise("initialize() {} species fox initialize() {} 1 early() {}", "species specifiers are illegal", __LINE__);
	SLiMAssertScriptRaise("species fox initialize() {} 1 early() { stop(); }", "must be preceded by a ticks specifier", __LINE__);
	SLiMAssertScriptRaise("initialize() {} ticks all 1 early() { stop(); }", "ticks specifiers should not be used", __LINE__);
	SLiMAssertScriptRaise("initialize() {} species all 1 early() { stop(); }", "may not be preceded by a species specifier", __LINE__);
	SLiMAssertScriptStop("species mouse initialize() {} species fox initialize() {} ticks all 1 early() { stop(); }", __LINE__);
	SLiMAssertScriptRaise("species mouse initialize() {} species fox initialize() { initializeRecombinationRate(0.0); } ticks all 1 early() {}", "mutation rate interval", __LINE__, false);
	SLiMAssertScriptRaise("species mouse initialize() {} species fox initialize() {} ticks bear 1 early() {}", "undeclared species", __LINE__);
	SLiMAssertScriptRaise("species mouse initialize() {} species fox initialize() { initializeRecombinationRate(0.0); } ticks fox 1 early() {}", "mutation rate interval", __LINE__, false);
	SLiMAssertScriptRaise("species mouse initialize() {} species fox initialize() {} species all mutationEffect(m1) {}", "mutationEffect() callbacks may not be declared with 'species all'", __LINE__);
	SLiMAssertScriptRaise("species mouse initialize() {} species fox initialize() {} species bear mutationEffect(m1) {}", "undeclared species", __LINE__);
	SLiMAssertScriptRaise("species mouse initialize() {} species fox initialize() { initializeRecombinationRate(0.0); } species fox mutationEffect(m1) {}", "mutation rate interval", __LINE__, false);
	SLiMAssertScriptRaise("species mouse initialize() {} species fox initialize() {} mutationEffect(m1) {}", "must be preceded", __LINE__);
	SLiMAssertScriptRaise("species mouse species mouse", "must be followed by a callback", __LINE__);
	SLiMAssertScriptRaise("ticks mouse ticks mouse", "must be followed by an event", __LINE__);
	SLiMAssertScriptRaise("foo", "unexpected identifier", __LINE__);
	SLiMAssertScriptRaise("species mouse foo", "unexpected identifier", __LINE__);
	SLiMAssertScriptRaise("ticks mouse foo", "unexpected identifier", __LINE__);
	SLiMAssertScriptRaise("species fox function (void)foo(void) {}", "may not be preceded", __LINE__);
	SLiMAssertScriptRaise("ticks fox function (void)foo(void) {}", "may not be preceded", __LINE__);
	SLiMAssertScriptRaise("species fox 1 early() {}", "may not be preceded", __LINE__);
	SLiMAssertScriptRaise("ticks fox 1 early() {}", "no initialize() callback", __LINE__, false);
	SLiMAssertScriptRaise("species fox mutationEffect(m1) {}", "no initialize() callback", __LINE__, false);
	SLiMAssertScriptRaise("ticks fox mutationEffect(m1) {}", "may not be preceded", __LINE__);
	
	// Test no-genetics scripts; we just want to confirm that they can run without any null genome errors, consistency check errors, etc., especially in DEBUG
	SLiMAssertScriptRaise("initialize() {initializeTreeSeq();} 1 early() {sim.addSubpop('p1', 10);} 5 late() {stop();}", "cannot use tree-sequence recording", __LINE__, false);
	SLiMAssertScriptRaise("initialize() {initializeSLiMOptions(nucleotideBased=T);} 1 early() {sim.addSubpop('p1', 10);} 5 late() {stop();}", "cannot be nucleotide-based", __LINE__, false);
	SLiMAssertScriptRaise("initialize() {initializeSLiMOptions(mutationRuns=5);} 1 early() {sim.addSubpop('p1', 10);} 5 late() {stop();}", "cannot have a specified mutation run count", __LINE__, false);
	SLiMAssertScriptStop("initialize() {} 1 early() {sim.addSubpop('p1', 10);} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {} 1 early() {sim.addSubpop('p1', 10); p1.setCloningRate(0.5);} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {} 1 early() {sim.addSubpop('p1', 10); p1.setSelfingRate(0.5);} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSex('A');} 1 early() {sim.addSubpop('p1', 10);} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSex('A');} 1 early() {sim.addSubpop('p1', 10); p1.setCloningRate(0.5);} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSex('X');} 1 early() {sim.addSubpop('p1', 10);} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSex('X');} 1 early() {sim.addSubpop('p1', 10); p1.setCloningRate(0.5);} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSex('Y');} 1 early() {sim.addSubpop('p1', 10);} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSex('Y');} 1 early() {sim.addSubpop('p1', 10); p1.setCloningRate(0.5);} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSLiMModelType('nonWF');} reproduction() {subpop.addCrossed(individual, subpop.sampleIndividuals(1));} 1 early() {sim.addSubpop('p1', 20);} early() {p1.fitnessScaling = 20 / p1.individualCount;} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSLiMModelType('nonWF');} reproduction() {subpop.addCloned(individual);} 1 early() {sim.addSubpop('p1', 20);} early() {p1.fitnessScaling = 20 / p1.individualCount;} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSLiMModelType('nonWF');} reproduction() {subpop.addSelfed(individual);} 1 early() {sim.addSubpop('p1', 20);} early() {p1.fitnessScaling = 20 / p1.individualCount;} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSLiMModelType('nonWF');} reproduction() {subpop.addEmpty();} 1 early() {sim.addSubpop('p1', 20);} early() {p1.fitnessScaling = 20 / p1.individualCount;} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSLiMModelType('nonWF');initializeSex('A');} reproduction(NULL,'F') {subpop.addCrossed(individual, subpop.sampleIndividuals(1,sex='M'));} 1 early() {sim.addSubpop('p1', 20);} early() {p1.fitnessScaling = 20 / p1.individualCount;} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSLiMModelType('nonWF');initializeSex('A');} reproduction() {subpop.addCloned(individual);} 1 early() {sim.addSubpop('p1', 20);} early() {p1.fitnessScaling = 20 / p1.individualCount;} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSLiMModelType('nonWF');initializeSex('A');} reproduction() {subpop.addEmpty();} 1 early() {sim.addSubpop('p1', 20);} early() {p1.fitnessScaling = 20 / p1.individualCount;} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSLiMModelType('nonWF');initializeSex('X');} reproduction(NULL,'F') {subpop.addCrossed(individual, subpop.sampleIndividuals(1,sex='M'));} 1 early() {sim.addSubpop('p1', 20);} early() {p1.fitnessScaling = 20 / p1.individualCount;} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSLiMModelType('nonWF');initializeSex('X');} reproduction() {subpop.addCloned(individual);} 1 early() {sim.addSubpop('p1', 20);} early() {p1.fitnessScaling = 20 / p1.individualCount;} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSLiMModelType('nonWF');initializeSex('X');} reproduction() {subpop.addEmpty();} 1 early() {sim.addSubpop('p1', 20);} early() {p1.fitnessScaling = 20 / p1.individualCount;} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSLiMModelType('nonWF');initializeSex('Y');} reproduction(NULL,'F') {subpop.addCrossed(individual, subpop.sampleIndividuals(1,sex='M'));} 1 early() {sim.addSubpop('p1', 20);} early() {p1.fitnessScaling = 20 / p1.individualCount;} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSLiMModelType('nonWF');initializeSex('Y');} reproduction() {subpop.addCloned(individual);} 1 early() {sim.addSubpop('p1', 20);} early() {p1.fitnessScaling = 20 / p1.individualCount;} 5 late() {stop();}", __LINE__);
	SLiMAssertScriptStop("initialize() {initializeSLiMModelType('nonWF');initializeSex('Y');} reproduction() {subpop.addEmpty();} 1 early() {sim.addSubpop('p1', 20);} early() {p1.fitnessScaling = 20 / p1.individualCount;} 5 late() {stop();}", __LINE__);
	
	// Test "long-term boundary" checks; note that Eidos has more complete checks in eidos_test_functions_other.cpp,
	// so here we are only testing specifically that SLiM catches long-term boundary violations correctly
	std::string ltb1_script(R"V0G0N(
							
							initialize() {
							initializeMutationRate(1e-7);
							initializeMutationType('m1', 0.5, 'f', 0.0);
							initializeGenomicElementType('g1', m1, 1.0);
							initializeGenomicElement(g1, 0, 99999);
							initializeRecombinationRate(1e-8);
							}
							1 early() { sim.addSubpop('p1', 500); x = Dictionary('a', p1); }
							5 late() { sim.outputFull(); }
							
							)V0G0N");
	
	SLiMAssertScriptSuccess(ltb1_script);	// x is scoped, so there is no long-term reference
	
	std::string ltb2_script(R"V0G0N(
							
							initialize() {
							initializeMutationRate(1e-7);
							initializeMutationType('m1', 0.5, 'f', 0.0);
							initializeGenomicElementType('g1', m1, 1.0);
							initializeGenomicElement(g1, 0, 99999);
							initializeRecombinationRate(1e-8);
							}
							1 early() { sim.addSubpop('p1', 500); defineGlobal('x', Dictionary('a', p1)); }
							5 late() { sim.outputFull(); }
							
							)V0G0N");
	
	SLiMAssertScriptRaise(ltb2_script, "long-term reference has been kept", 0, false);	// x is not scoped, so there is a long-term reference
	
	std::string ltb3_script(R"V0G0N(
							
							initialize() {
							initializeMutationRate(1e-7);
							initializeMutationType('m1', 0.5, 'f', 0.0);
							initializeGenomicElementType('g1', m1, 1.0);
							initializeGenomicElement(g1, 0, 99999);
							initializeRecombinationRate(1e-8);
							}
							1 early() { sim.addSubpop('p1', 500); sim.setValue('x', Dictionary('a', p1)); }
							5 late() { sim.outputFull(); }
							
							)V0G0N");
	
	SLiMAssertScriptRaise(ltb3_script, "long-term reference has been kept", 0, false);	// sim.getValue('x') is not scoped, so there is a long-term reference
}

#pragma mark Individual relatedness tests
void _RunRelatednessTests(void)
{
	// This function tests the relatedness() function of Individual.  This can't be done easily in script, since setting up an exact pedigree
	// in a SLiM model is kind of a pain, and we want to test a bunch of different pedigrees here; it would just be too complex.  So we test
	// the internal API of the Individual class here instead, and verify that it produces the correct values; relatedness() uses that.
	
	typedef struct pedigree_test_info_ {
		slim_pedigreeid_t A;
		slim_pedigreeid_t A_P1;
		slim_pedigreeid_t A_P2;
		slim_pedigreeid_t A_G1;
		slim_pedigreeid_t A_G2;
		slim_pedigreeid_t A_G3;
		slim_pedigreeid_t A_G4;
		slim_pedigreeid_t B;
		slim_pedigreeid_t B_P1;
		slim_pedigreeid_t B_P2;
		slim_pedigreeid_t B_G1;
		slim_pedigreeid_t B_G2;
		slim_pedigreeid_t B_G3;
		slim_pedigreeid_t B_G4;
		IndividualSex A_sex;
		IndividualSex B_sex;
		GenomeType type;
		double expectedRelatedness;
	} pedigree_test_info;
	
	pedigree_test_info test_pedigrees[] = {
		// two individuals that are completely unrelated
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 12, 13, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 12, 13, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 12, 13, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 12, 13, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 12, 13, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kAutosome, /*expected */ 0.0},
		
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 12, 13, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 12, 13, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 12, 13, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kXChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 12, 13, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kXChromosome, /*expected */ 0.0},

		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 12, 13, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kYChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 12, 13, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kYChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 12, 13, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kYChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 12, 13, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kYChromosome, /*expected */ 0.0},
		
		// completely unrelated individuals with missing information
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ -1, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ -1, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ -1, -1, -1, -1, -1, -1, -1, /* B */ -1, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.0},

		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 8, 9, -1, -1, -1, -1, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 8, 9, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 8, 9, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 8, 9, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 8, 9, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kAutosome, /*expected */ 0.0},
		
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 8, 9, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 8, 9, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 8, 9, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kXChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 8, 9, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kXChromosome, /*expected */ 0.0},

		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 8, 9, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kYChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 8, 9, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kYChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 8, 9, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kYChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 8, 9, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kYChromosome, /*expected */ 0.0},

		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kAutosome, /*expected */ 0.0},
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kAutosome, /*expected */ 0.0},
		
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 0.0},
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 0.0},
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kXChromosome, /*expected */ 0.0},
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kXChromosome, /*expected */ 0.0},

		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kYChromosome, /*expected */ 0.0},
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kYChromosome, /*expected */ 0.0},
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kYChromosome, /*expected */ 0.0},
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, -1, -1, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kYChromosome, /*expected */ 0.0},
		
		// the exact same individual
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 0, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 1.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 0, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kAutosome, /*expected */ 1.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 0, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kAutosome, /*expected */ 1.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 0, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 1.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 0, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kXChromosome, /*expected */ 1.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 0, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kYChromosome, /*expected */ 1.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 0, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kYChromosome, /*expected */ 1.0},
		
		// products of cloning/selfing
		{/* A */ 0, 1, 1, 3, 4, 3, 4, /* B */ 0, 1, 1, 3, 4, 3, 4, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 1.0},
		{/* A */ 0, 1, 1, 3, 4, 3, 4, /* B */ 7, 1, 1, 3, 4, 3, 4, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 1.0},
		{/* A */ 0, 1, 1, 2, 2, 2, 2, /* B */ 0, 1, 1, 2, 2, 2, 2, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 1.0},
		{/* A */ 0, 1, 1, 2, 2, 2, 2, /* B */ 7, 1, 1, 2, 2, 2, 2, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 1.0},
		{/* A */ 0, 1, 1, 2, 2, 2, 2, /* B */ 1, 2, 2, 3, 3, 3, 3, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 1.0},
		{/* A */ 0, 1, 1, 2, 2, 2, 2, /* B */ 2, 3, 3, 4, 4, 4, 4, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 1.0},
		
		// siblings
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kAutosome, /*expected */ 0.5},
		
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 1.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kXChromosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kXChromosome, /*expected */ 0.5},
		
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kYChromosome, /*expected */ 1.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kYChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kYChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 2, 3, 4, 5, 6, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kYChromosome, /*expected */ 0.0},
		
		// siblings with missing information
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 1, 2, -1, -1, -1, -1, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 1, 2, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 1, 2, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 1, 2, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 1, 2, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kAutosome, /*expected */ 0.5},
		
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 1, 2, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 1.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 1, 2, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 1, 2, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kXChromosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 1, 2, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kXChromosome, /*expected */ 0.5},
		
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 1, 2, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kYChromosome, /*expected */ 1.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 1, 2, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kYChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 1, 2, -1, -1, -1, -1, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kYChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, -1, -1, -1, -1, /* B */ 7, 1, 2, -1, -1, -1, -1, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kYChromosome, /*expected */ 0.0},
		
		// parent-child (hermaphroditic)
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 0, 8, 1, 2, 9, 10, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, 0, 8, -1, -1, -1, -1, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.5},
		
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 0, 9, 10, 1, 2, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, -1, -1, -1, -1, -1, -1, /* B */ 7, 8, 0, -1, -1, -1, -1, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.5},
		
		// mother-daughter
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 0, 8, 1, 2, 9, 10, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 0, 8, 1, 2, 9, 10, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kXChromosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 0, 8, 1, 2, 9, 10, /* other */ IndividualSex::kFemale, IndividualSex::kFemale, GenomeType::kYChromosome, /*expected */ 0.0},
		
		// mother-son
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 0, 8, 1, 2, 9, 10, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 0, 8, 1, 2, 9, 10, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 1.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 0, 8, 1, 2, 9, 10, /* other */ IndividualSex::kFemale, IndividualSex::kMale, GenomeType::kYChromosome, /*expected */ 0.0},
		
		// father-daughter
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 0, 9, 10, 1, 2, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 0, 9, 10, 1, 2, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kXChromosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 0, 9, 10, 1, 2, /* other */ IndividualSex::kMale, IndividualSex::kFemale, GenomeType::kYChromosome, /*expected */ 0.0},
		
		// father-son
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 0, 9, 10, 1, 2, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kAutosome, /*expected */ 0.5},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 0, 9, 10, 1, 2, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 0.0},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 0, 9, 10, 1, 2, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kYChromosome, /*expected */ 1.0},
		
		// half-siblings (hermaphroditic)
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 1, 8, 3, 4, 9, 10, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.25},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 2, 8, 5, 6, 9, 10, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.25},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 1, 9, 10, 3, 4, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.25},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 2, 9, 10, 5, 6, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.25},
		
		// cousins (hermaphroditic)
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 3, 4, 10, 11, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.125},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 3, 4, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.125},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 5, 6, 10, 11, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.125},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 10, 11, 5, 6, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.125},
		
		// grandchild-grandparent (hermaphroditic)
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 3, 7, 8, 9, 10, 11, 12, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.25},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 4, 7, 8, 9, 10, 11, 12, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.25},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 5, 7, 8, 9, 10, 11, 12, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.25},
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 6, 7, 8, 9, 10, 11, 12, /* other */ IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, /*expected */ 0.25},
		
		// random spot-checks; obviously there is a huge variety of set-ups we could check...
		// male cousins sharing both of their maternal grandparents, modeling the X:
		{/* A */ 0, 1, 2, 3, 4, 5, 6, /* B */ 7, 8, 9, 3, 4, 10, 11, /* other */ IndividualSex::kMale, IndividualSex::kMale, GenomeType::kXChromosome, /*expected */ 0.5},
		
		/* end-of-array marker entry : DO NOT TOUCH */ {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, IndividualSex::kHermaphrodite, IndividualSex::kHermaphrodite, GenomeType::kAutosome, -1.0}
	};
	
	{
		pedigree_test_info *p = test_pedigrees;
		
		for ( ; p->expectedRelatedness > -1.0; ++p)
		{
			try {
				double rel = Individual::_Relatedness(p->A, p->A_P1, p->A_P2, p->A_G1, p->A_G2, p->A_G3, p->A_G4, p->B, p->B_P1, p->B_P2, p->B_G1, p->B_G2, p->B_G3, p->B_G4, p->A_sex, p->B_sex, p->type);
				double expected = p->expectedRelatedness;
				
				if (rel == expected)
				{
					gSLiMTestSuccessCount++;
					
					//std::cerr << "relatedness test " << EIDOS_OUTPUT_SUCCESS_TAG << ": test index " << (p - test_pedigrees) << " produced a relatedness of " << rel << " (as expected)" << std::endl;
				}
				else
				{
					gSLiMTestFailureCount++;
					
					std::cerr << "relatedness test " << EIDOS_OUTPUT_FAILURE_TAG << ": test index " << (p - test_pedigrees) << " produced a relatedness of " << rel << " (" << expected << " expected)" << std::endl;
				}
			}
			catch (...)
			{
				std::cerr << "relatedness test " << EIDOS_OUTPUT_FAILURE_TAG << ": test index " << (p - test_pedigrees) << " raised an exception: " << Eidos_GetTrimmedRaiseMessage() << std::endl;
			}
		}
	}
	
	// this output is useful for figuring out which entry is causing a problem, when it is located near the end
	//std::cerr << "end-of-array index == " << (p - test_pedigrees) << std::endl;
	
	// Run tests of SharedParentCount() as well, which is much simpler
	typedef struct sharedparent_test_info_ {
		slim_pedigreeid_t X_P1;
		slim_pedigreeid_t X_P2;
		slim_pedigreeid_t Y_P1;
		slim_pedigreeid_t Y_P2;
		int expectedCount;
	} sharedparent_test_info;
	
	sharedparent_test_info test_sharedparent[] = {
		{/* X */ -1, -1, /* Y */ -1, -1, /*expected */ 0},		// missing information
		{/* X */ 0, 1, /* Y */ 2, 3, /*expected */ 0},
		{/* X */ 0, 1, /* Y */ 2, 2, /*expected */ 0},
		{/* X */ 0, 1, /* Y */ 0, 2, /*expected */ 1},
		{/* X */ 0, 1, /* Y */ 0, 0, /*expected */ 1},
		{/* X */ 0, 0, /* Y */ 0, 1, /*expected */ 1},
		{/* X */ 0, 1, /* Y */ 0, 1, /*expected */ 2},
		{/* X */ 0, 1, /* Y */ 1, 0, /*expected */ 2},
		{/* X */ 0, 0, /* Y */ 0, 0, /*expected */ 2},
		/* end-of-array marker entry : DO NOT TOUCH */ {-1, -1, -1, -1, -1}
	};
	
	{
		sharedparent_test_info *p = test_sharedparent;
		
		for ( ; p->expectedCount > -1; ++p)
		{
			try {
				int count = Individual::_SharedParentCount(p->X_P1, p->X_P2, p->Y_P1, p->Y_P2);
				int expected = p->expectedCount;
				
				if (count == expected)
				{
					gSLiMTestSuccessCount++;
					
					//std::cerr << "sharedParentCount test " << EIDOS_OUTPUT_SUCCESS_TAG << ": test index " << (p - test_sharedparent) << " produced a count of " << count << " (as expected)" << std::endl;
				}
				else
				{
					gSLiMTestFailureCount++;
					
					std::cerr << "sharedParentCount test " << EIDOS_OUTPUT_FAILURE_TAG << ": test index " << (p - test_sharedparent) << " produced a count of " << count << " (" << expected << " expected)" << std::endl;
				}
			}
			catch (...)
			{
				std::cerr << "sharedParentCount test " << EIDOS_OUTPUT_FAILURE_TAG << ": test index " << (p - test_sharedparent) << " raised an exception: " << Eidos_GetTrimmedRaiseMessage() << std::endl;
			}
		}
	}
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
			genome_map.emplace(genome_properties[i], gSLiM_Genome_Class->_SignatureForProperty(genome_properties[i]));
		
		for (int i = 0; i < 4; ++i)
			genome_unordered_map.emplace(genome_properties[i], gSLiM_Genome_Class->_SignatureForProperty(genome_properties[i]));
		
		{
			std::clock_t begin = std::clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 100000000; i++)
			{
				EidosGlobalStringID property_id = genome_properties[Eidos_rng_uniform_int(EIDOS_GSL_RNG, 4)];
				
				total += (int64_t)(gSLiM_Genome_Class->_SignatureForProperty(property_id));
			}
			
			std::clock_t end = std::clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for Genome_Class::_SignatureForProperty() calls: " << time_spent << std::endl;
		}
		{
			std::clock_t begin = std::clock();
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
			
			std::clock_t end = std::clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for genome_map calls: " << time_spent << std::endl;
		}
		{
			std::clock_t begin = std::clock();
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
			
			std::clock_t end = std::clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for genome_unordered_map calls: " << time_spent << std::endl << std::endl;
		}
	}
	
	{
		EidosGlobalStringID individual_properties[17] = {gID_subpopulation, gID_index, gID_genomes, gID_sex, gID_tag, gID_tagF, gID_fitnessScaling, gEidosID_x, gEidosID_y, gEidosID_z, gID_age, gID_pedigreeID, gID_pedigreeParentIDs, gID_pedigreeGrandparentIDs, gID_spatialPosition, gID_uniqueMutations, gEidosID_color};
		std::map<EidosGlobalStringID, const EidosPropertySignature *> individual_map;
		std::unordered_map<EidosGlobalStringID, const EidosPropertySignature *> individual_unordered_map;
		
		for (int i = 0; i < 17; ++i)
			individual_map.emplace(individual_properties[i], gSLiM_Individual_Class->_SignatureForProperty(individual_properties[i]));
		
		for (int i = 0; i < 17; ++i)
			individual_unordered_map.emplace(individual_properties[i], gSLiM_Individual_Class->_SignatureForProperty(individual_properties[i]));
		
		{
			std::clock_t begin = std::clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 100000000; i++)
			{
				EidosGlobalStringID property_id = individual_properties[Eidos_rng_uniform_int(EIDOS_GSL_RNG, 17)];
				
				total += (int64_t)(gSLiM_Individual_Class->_SignatureForProperty(property_id));
			}
			
			std::clock_t end = std::clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for Individual_Class::_SignatureForProperty() calls: " << time_spent << std::endl;
		}
		{
			std::clock_t begin = std::clock();
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
			
			std::clock_t end = std::clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for individual_map calls: " << time_spent << std::endl;
		}
		{
			std::clock_t begin = std::clock();
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
			
			std::clock_t end = std::clock();
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
			std::clock_t begin = std::clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 1000000000; i++)
			{
				bool contains_symbol = internalTable.ContainsSymbol(gEidosID_F);
				
				total += (contains_symbol ? 1 : 2);
			}
			
			std::clock_t end = std::clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for internalTable (F) checks: " << time_spent << std::endl;
		}
		{
			std::clock_t begin = std::clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 1000000000; i++)
			{
				bool contains_symbol = externalTable.ContainsSymbol(gEidosID_F);
				
				total += (contains_symbol ? 1 : 2);
			}
			
			std::clock_t end = std::clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for externalTable (F) checks: " << time_spent << std::endl << std::endl;
		}
		
		{
			std::clock_t begin = std::clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 1000000000; i++)
			{
				EidosGlobalStringID variable_id = symbol_ids[Eidos_rng_uniform_int(EIDOS_GSL_RNG, 7)];
				
				bool contains_symbol = internalTable.ContainsSymbol(variable_id);
				
				total += (contains_symbol ? 1 : 2);
			}
			
			std::clock_t end = std::clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for internalTable successful checks: " << time_spent << std::endl;
		}
		{
			std::clock_t begin = std::clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 1000000000; i++)
			{
				EidosGlobalStringID variable_id = symbol_ids[Eidos_rng_uniform_int(EIDOS_GSL_RNG, 7)];
				
				bool contains_symbol = externalTable.ContainsSymbol(variable_id);
				
				total += (contains_symbol ? 1 : 2);
			}
			
			std::clock_t end = std::clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for externalTable successful checks: " << time_spent << std::endl << std::endl;
		}
		
		{
			std::clock_t begin = std::clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 1000000000; i++)
			{
				bool contains_symbol = internalTable.ContainsSymbol(gEidosID_applyValue);
				
				total += (contains_symbol ? 1 : 2);
			}
			
			std::clock_t end = std::clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for internalTable failed checks: " << time_spent << std::endl;
		}
		{
			std::clock_t begin = std::clock();
			int64_t total = 0;
			
			for (int64_t i = 0; i < 1000000000; i++)
			{
				bool contains_symbol = externalTable.ContainsSymbol(gEidosID_applyValue);
				
				total += (contains_symbol ? 1 : 2);
			}
			
			std::clock_t end = std::clock();
			double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
			
			std::cout << "Time for externalTable failed checks: " << time_spent << std::endl << std::endl;
		}
	}
#endif
}

#pragma mark SLiM timing tests
extern void _RunParallelSLiMTests()
{
	// Tests of parallelization of SLiM functions/methods/core code; see also eidos_test_parallel.h
#ifdef _OPENMP
	const std::string &parallelization_test_string =
#include "slim_test_parallel.h"
	;
	{
		std::vector<std::string> test_strings = Eidos_string_split(parallelization_test_string, "// ***********************************************************************************************");
		
		//for (int testidx = 0; testidx < 100; testidx++)	// uncomment this for a more thorough stress test
		{
			for (std::string &test_string : test_strings)
			{
				// Skip empty tests
				if (test_string.find("initialize()") == std::string::npos)
					continue;
				
				//std::chrono::steady_clock::time_point begin_ts = std::chrono::steady_clock::now();
				
				// Note that we ensure that we are using the maximum number of threads at start & end
				{
					gEidosNumThreads = gEidosMaxThreads;
					gEidosNumThreadsOverride = false;
					omp_set_num_threads(gEidosMaxThreads);
					
					SLiMAssertScriptSuccess(test_string);
					
					gEidosNumThreads = gEidosMaxThreads;
					gEidosNumThreadsOverride = false;
					omp_set_num_threads(gEidosMaxThreads);
				}
				
				//std::chrono::steady_clock::time_point end_ts = std::chrono::steady_clock::now();
				//std::cout << "parallel test took " << std::chrono::duration<double>(end_ts - begin_ts).count() << " seconds:" << std::endl << test_string << std::endl << std::endl;
			}
		}
	}
#endif
}
































































