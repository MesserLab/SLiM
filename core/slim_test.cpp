//
//  slim_test.cpp
//  SLiM
//
//  Created by Ben Haller on 8/14/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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
#include "slim_sim.h"
#include "eidos_test.h"

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
static void _RunSLiMTimingTests(void);


// Test function shared strings
std::string gen1_setup("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } ");
std::string gen1_setup_sex("initialize() { initializeMutationRate(1e-7); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeSex('X'); } ");
std::string gen2_stop(" 2 { stop(); } ");
std::string gen1_setup_highmut_p1("initialize() { initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } ");
std::string gen1_setup_fixmut_p1("initialize() { initializeMutationRate(1e-4); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); } 1 { sim.addSubpop('p1', 10); } 10 { sim.mutations[0].setSelectionCoeff(500.0); sim.recalculateFitness(); } ");
std::string gen1_setup_i1("initialize() { initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', ''); } 1 { sim.addSubpop('p1', 10); } 1:10 late() { i1.evaluate(); i1.strength(p1.individuals[0]); } ");
std::string gen1_setup_i1x("initialize() { initializeSLiMOptions(dimensionality='x'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'x'); } 1 { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); i1.evaluate(); i1.strength(p1.individuals[0]); } ");
std::string gen1_setup_i1xPx("initialize() { initializeSLiMOptions(dimensionality='x', periodicity='x'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'x'); } 1 { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); i1.evaluate(); i1.strength(p1.individuals[0]); } ");
std::string gen1_setup_i1xyz("initialize() { initializeSLiMOptions(dimensionality='xyz'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xyz'); } 1 { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); p1.individuals.y = runif(10); p1.individuals.z = runif(10); i1.evaluate(); i1.strength(p1.individuals[0]); } ");
std::string gen1_setup_i1xyzPxz("initialize() { initializeSLiMOptions(dimensionality='xyz', periodicity='xz'); initializeMutationRate(1e-5); initializeMutationType('m1', 0.5, 'f', 0.0); initializeGenomicElementType('g1', m1, 1.0); initializeGenomicElement(g1, 0, 99999); initializeRecombinationRate(1e-8); initializeInteractionType('i1', 'xyz'); } 1 { sim.addSubpop('p1', 10); } 1:10 late() { p1.individuals.x = runif(10); p1.individuals.y = runif(10); p1.individuals.z = runif(10); i1.evaluate(); i1.strength(p1.individuals[0]); } ");
std::string gen1_setup_p1(gen1_setup + "1 { sim.addSubpop('p1', 10); } ");
std::string gen1_setup_sex_p1(gen1_setup_sex + "1 { sim.addSubpop('p1', 10); } ");
std::string gen1_setup_p1p2p3(gen1_setup + "1 { sim.addSubpop('p1', 10); sim.addSubpop('p2', 10); sim.addSubpop('p3', 10); } ");

std::string WF_prefix("initialize() { initializeSLiMModelType('WF'); } ");
std::string nonWF_prefix("initialize() { initializeSLiMModelType('nonWF'); } ");


int RunSLiMTests(void)
{
	// Test SLiM.  The goal here is not really to test that the core code of SLiM is working properly – that simulations
	// work as they are intended to.  Such testing is beyond the scope of what we can do here.  Instead, the goal here
	// is to test all of the Eidos-related APIs in SLiM – to make sure that all properties, methods, and functions in
	// SLiM's Eidos interface work properly.  SLiM itself will get a little incidental testing along the way.
	
	if (!Eidos_SlashTmpExists())
		std::cout << "WARNING: This system does not appear to have a writeable /tmp directory.  Filesystem tests are disabled, and functions such as writeTempFile() and system() that depend upon the existence of /tmp will raise an exception if called (and are therefore also not tested).  If this is surprising, contact the system administrator for details." << std::endl;
	
	// We want to run the self-test inside a new temporary directory, to prevent collisions with other self-test runs
	std::string prefix = "/tmp/slimTest_";
	std::string temp_path_template = prefix + "XXXXXX";
	char *temp_path_cstr = strdup(temp_path_template.c_str());
	
	if (Eidos_mkstemps_directory(temp_path_cstr, 0) == 0)
	{
		//std::cout << "Running SLiM self-tests in " << temp_path_cstr << " ..." << std::endl;
	}
	else
	{
		std::cout << "A temporary folder within /tmp could not be created; there may be a permissions problem with /tmp.  The self-test could not be run." << std::endl;
		return 1;
	}
	
	std::string temp_path(temp_path_cstr);	// the final random path generated by Eidos_mkstemps_directory
	
	// Reset error counts
	gSLiMTestSuccessCount = 0;
	gSLiMTestFailureCount = 0;
	
	// Run tests
	_RunBasicTests();
	_RunInitTests();
	_RunSLiMSimTests(temp_path);
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
	_RunNonWFTests();
	_RunTreeSeqTests(temp_path);
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
			individual_map.emplace(std::pair<EidosGlobalStringID, const EidosPropertySignature *>(individual_properties[i], gSLiM_Individual_Class->_SignatureForProperty(individual_properties[i])));
		
		for (int i = 0; i < 17; ++i)
			individual_unordered_map.emplace(std::pair<EidosGlobalStringID, const EidosPropertySignature *>(individual_properties[i], gSLiM_Individual_Class->_SignatureForProperty(individual_properties[i])));
		
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































































