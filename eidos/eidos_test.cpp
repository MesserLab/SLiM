//
//  eidos_test.cpp
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of Eidos.
//
//	Eidos is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	Eidos is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with Eidos.  If not, see <http://www.gnu.org/licenses/>.


#include "eidos_test.h"
#include "eidos_script.h"
#include "eidos_value.h"
#include "eidos_interpreter.h"
#include "eidos_globals.h"
#include "eidos_rng.h"
#include "eidos_test_element.h"

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <limits>
#include <random>
#include <ctime>

#if 0
#if ((defined(SLIMGUI) && (SLIMPROFILING == 1)) || defined(EIDOS_GUI))
// includes for the timing code in RunEidosTests(), which is normally #if 0
#include "sys/time.h"	// for gettimeofday()
#include <chrono>
#include <mach/mach_time.h>
#endif
#endif


// Keeping records of test success / failure
static int gEidosTestSuccessCount = 0;
static int gEidosTestFailureCount = 0;


// Instantiates and runs the script, and prints an error if the result does not match expectations
void EidosAssertScriptSuccess(const std::string &p_script_string, EidosValue_SP p_correct_result)
{
	EidosScript script(p_script_string);
	EidosValue_SP result;
	EidosSymbolTable symbol_table(EidosSymbolTableType::kVariablesTable, gEidosConstantsSymbolTable);
	
	gEidosCurrentScript = &script;
	
	gEidosTestFailureCount++;	// assume failure; we will fix this at the end if we succeed
	
	try {
		script.Tokenize();
	}
	catch (...)
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during Tokenize(): " << Eidos_GetTrimmedRaiseMessage() << std::endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		return;
	}
	
	try {
		script.ParseInterpreterBlockToAST(true);
	}
	catch (...)
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during ParseToAST(): " << Eidos_GetTrimmedRaiseMessage() << std::endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		return;
	}
	
	try {
		EidosFunctionMap function_map(*EidosInterpreter::BuiltInFunctionMap());
		EidosInterpreter interpreter(script, symbol_table, function_map, nullptr);
		
		result = interpreter.EvaluateInterpreterBlock(true, true);		// print output, return the last statement value
	}
	catch (...)
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during EvaluateInterpreterBlock(): " << Eidos_GetTrimmedRaiseMessage() << std::endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		
		return;
	}
	
	// Check that the result is actually what we want it to be
	if (!result)
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : return value is nullptr" << std::endl;
	}
	else if (result->Type() != p_correct_result->Type())
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : unexpected return type (" << result->Type() << ", expected " << p_correct_result->Type() << ")" << std::endl;
	}
	else if (result->ElementType() != p_correct_result->ElementType())
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : unexpected return element type (" << result->ElementType() << ", expected " << p_correct_result->ElementType() << ")" << std::endl;
	}
	else if (result->Count() != p_correct_result->Count())
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : unexpected return length (" << result->Count() << ", expected " << p_correct_result->Count() << ")" << std::endl;
	}
	//else if (result->DimensionCount() != 1)
	//{
	//	std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : matrix or array returned, which is not tested; use identical()" << std::endl;
	//}
	else
	{
		if (!IdenticalEidosValues(result.get(), p_correct_result.get(), false))		// don't compare dimensions; we have no easy way to specify matrix/array results
		{
			std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : mismatched values (" << *result << "), expected (" << *p_correct_result << ")" << std::endl;
			return;
		}
		
		gEidosTestFailureCount--;	// correct for our assumption of failure above
		gEidosTestSuccessCount++;
		
		//std::cerr << p_script_string << " == " << p_correct_result->Type() << "(" << *p_correct_result << ") : " << EIDOS_OUTPUT_SUCCESS_TAG << endl;
	}
	
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

// Instantiates and runs the script, and prints an error if the script does not cause an exception to be raised
void EidosAssertScriptRaise(const std::string &p_script_string, const int p_bad_position, const std::string &p_reason_snip)
{
	EidosScript script(p_script_string);
	EidosSymbolTable symbol_table(EidosSymbolTableType::kVariablesTable, gEidosConstantsSymbolTable);
	EidosFunctionMap function_map(*EidosInterpreter::BuiltInFunctionMap());
	
	gEidosCurrentScript = &script;
	
	try {
		script.Tokenize();
		script.ParseInterpreterBlockToAST(true);
		
		EidosInterpreter interpreter(script, symbol_table, function_map, nullptr);
		
		EidosValue_SP result = interpreter.EvaluateInterpreterBlock(true, true);		// print output, return the last statement value
		
		gEidosTestFailureCount++;
		
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : no raise during EvaluateInterpreterBlock()." << std::endl;
	}
	catch (...)
	{
		// We need to call Eidos_GetTrimmedRaiseMessage() here to empty the error stringstream, even if we don't log the error
		std::string raise_message = Eidos_GetTrimmedRaiseMessage();
		
		if (raise_message.find(p_reason_snip) != std::string::npos)
		{
			if ((gEidosCharacterStartOfError == -1) || (gEidosCharacterEndOfError == -1) || !gEidosCurrentScript)
			{
				gEidosTestFailureCount++;
				
				std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise expected, but no error info set" << std::endl;
				std::cerr << p_script_string << "   raise message: " << raise_message << std::endl;
				std::cerr << "--------------------" << std::endl << std::endl;
			}
			else if (gEidosCharacterStartOfError != p_bad_position)
			{
				gEidosTestFailureCount++;
				
				std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise expected, but error position unexpected" << std::endl;
				std::cerr << p_script_string << "   raise message: " << raise_message << std::endl;
				Eidos_LogScriptError(std::cerr, gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript, gEidosExecutingRuntimeScript);
				std::cerr << "--------------------" << std::endl << std::endl;
			}
			else
			{
				gEidosTestSuccessCount++;
				
				//std::cerr << p_script_string << " == (expected raise) " << raise_message << " : " << EIDOS_OUTPUT_SUCCESS_TAG << endl;
			}
		}
		else
		{
			gEidosTestFailureCount++;
			
			std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise message mismatch (expected \"" << p_reason_snip << "\")." << std::endl;
			std::cerr << "   raise message: " << raise_message << std::endl;
			std::cerr << "--------------------" << std::endl << std::endl;
		}
		
		// Error messages that say (internal error) should not be possible to trigger in script
		if (raise_message.find("(internal error)") != std::string::npos)
		{
			std::cerr << p_script_string << " : error message contains (internal error) erroneously" << std::endl;
			std::cerr << "   raise message: " << raise_message << std::endl;
		}
	}
	
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

int RunEidosTests(void)
{
	// Reset error counts
	gEidosTestSuccessCount = 0;
	gEidosTestFailureCount = 0;
	
#if (!EIDOS_HAS_OVERFLOW_BUILTINS)
	std::cout << "WARNING: This build of Eidos does not detect integer arithmetic overflows.  Compiling Eidos with GCC version 5.0 or later, or Clang version 3.9 or later, is required for this feature.  This means that integer addition, subtraction, or multiplication that overflows the 64-bit range of Eidos (" << INT64_MIN << " to " << INT64_MAX << ") will not be detected." << std::endl;
#endif
	
	if (!Eidos_SlashTmpExists())
		std::cout << "WARNING: This system does not appear to have a writeable /tmp directory.  Filesystem tests are disabled, and functions such as writeTempFile() and system() that depend upon the existence of /tmp will raise an exception if called (and are therefore also not tested).  If this is surprising, contact the system administrator for details." << std::endl;
	
	// We want to run the self-test inside a new temporary directory, to prevent collisions with other self-test runs
	std::string prefix = "/tmp/eidosTest_";
	std::string temp_path_template = prefix + "XXXXXX";
	char *temp_path_cstr = strdup(temp_path_template.c_str());
	
	if (Eidos_mkstemps_directory(temp_path_cstr, 0) == 0)
	{
		//std::cout << "Running Eidos self-tests in " << temp_path_cstr << " ..." << std::endl;
	}
	else
	{
		std::cout << "A temporary folder within /tmp could not be created; there may be a permissions problem with /tmp.  The self-test could not be run." << std::endl;
		return 1;
	}
	
	std::string temp_path(temp_path_cstr);	// the final random path generated by Eidos_mkstemps_directory
	
	// Run tests
	_RunLiteralsIdentifiersAndTokenizationTests();
	_RunSymbolsAndVariablesTests();
	_RunParsingTests();
	_RunFunctionDispatchTests();
	_RunRuntimeErrorTests();
	_RunVectorsAndSingletonsTests();
	_RunOperatorPlusTests1();
	_RunOperatorPlusTests2();
	_RunOperatorMinusTests();
	_RunOperatorMultTests();
	_RunOperatorDivTests();
	_RunOperatorModTests();
	_RunOperatorSubsetTests();
	_RunOperatorAssignTests();
	_RunOperatorGtTests();
	_RunOperatorLtTests();
	_RunOperatorGtEqTests();
	_RunOperatorLtEqTests();
	_RunOperatorEqTests();
	_RunOperatorNotEqTests();
	_RunOperatorRangeTests();
	_RunOperatorExpTests();
	_RunOperatorLogicalAndTests();
	_RunOperatorLogicalOrTests();
	_RunOperatorLogicalNotTests();
	_RunOperatorTernaryConditionalTests();
	_RunKeywordIfTests();
	_RunKeywordDoTests();
	_RunKeywordWhileTests();
	_RunKeywordForInTests();
	_RunKeywordNextTests();
	_RunKeywordBreakTests();
	_RunKeywordReturnTests();
	_RunFunctionMathTests_a_through_f();
	_RunFunctionMathTests_g_through_r();
	_RunFunctionMathTests_setUnionIntersection();
	_RunFunctionMathTests_setDifferenceSymmetricDifference();
	_RunFunctionMathTests_s_through_z();
	_RunFunctionMatrixArrayTests();
	_RunFunctionStatisticsTests();
	_RunFunctionDistributionTests();
	_RunFunctionVectorConstructionTests();
	_RunFunctionValueInspectionManipulationTests_a_through_f();
	_RunFunctionValueInspectionManipulationTests_g_through_l();
	_RunFunctionValueInspectionManipulationTests_m_through_r();
	_RunFunctionValueInspectionManipulationTests_s_through_z();
	_RunFunctionValueTestingCoercionTests();
	_RunFunctionFilesystemTests(temp_path);
	_RunColorManipulationTests();
	_RunFunctionMiscTests_apply_sapply();
	_RunFunctionMiscTests(temp_path);
	_RunMethodTests();
	_RunCodeExampleTests();
	_RunUserDefinedFunctionTests();
	_RunVoidEidosValueTests();
	
	// ************************************************************************************
	//
	//	Print a summary of test results
	//
	std::cerr << std::endl;
	if (gEidosTestFailureCount)
		std::cerr << "" << EIDOS_OUTPUT_FAILURE_TAG << " count: " << gEidosTestFailureCount << std::endl;
	std::cerr << EIDOS_OUTPUT_SUCCESS_TAG << " count: " << gEidosTestSuccessCount << std::endl;
	
	// If we are tracking allocations, print a count
#ifdef EIDOS_TRACK_VALUE_ALLOCATION
	std::cerr << "EidosValue allocation count: " << EidosValue::valueTrackingCount << std::endl;
	for (EidosValue *value : EidosValue::valueTrackingVector)
		std::cerr << *value << endl;
#endif
	
	// Do some tests of our custom math functions
#if 0
	Eidos_InitializeRNG();
	Eidos_SetRNGSeed(Eidos_GenerateSeedFromPIDAndTime());
	
	int64_t totals[17];		// note 17 is prime
	
	for (int i = 0; i < 17; ++i)
		totals[i] = 0;
	
	for (int i = 0; i < 100000000; ++i)
		totals[Eidos_rng_uniform_int(EIDOS_GSL_RNG, 17)]++;
	
	for (int i = 0; i < 17; ++i)
		std::cout << "totals[" << i << "] == " << totals[i] << std::endl;
#endif
	
#if 0
	//#ifndef USE_GSL_POISSON
	Eidos_InitializeRNG();
	Eidos_SetRNGSeed(Eidos_GenerateSeedFromPIDAndTime());
	
	double total;
	int i;
	
	std::cout << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += Eidos_FastRandomPoisson(1.0);
	
	std::cout << "Eidos_FastRandomPoisson(1.0): mean = " << (total / 1000000) << ", expected 1.0" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += gsl_ran_poisson(EIDOS_GSL_RNG, 1.0);
	
	std::cout << "gsl_ran_poisson(1.0): mean = " << (total / 1000000) << ", expected 1.0" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += Eidos_FastRandomPoisson(0.001);
	
	std::cout << "Eidos_FastRandomPoisson(0.001): mean = " << (total / 1000000) << ", expected 0.001" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += gsl_ran_poisson(EIDOS_GSL_RNG, 0.001);
	
	std::cout << "gsl_ran_poisson(0.001): mean = " << (total / 1000000) << ", expected 0.001" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += Eidos_FastRandomPoisson(0.00001);
	
	std::cout << "Eidos_FastRandomPoisson(0.00001): mean = " << (total / 1000000) << ", expected 0.00001" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += gsl_ran_poisson(EIDOS_GSL_RNG, 0.00001);
	
	std::cout << "gsl_ran_poisson(0.00001): mean = " << (total / 1000000) << ", expected 0.00001" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 100000; i++)
		total += Eidos_FastRandomPoisson(100);
	
	std::cout << "Eidos_FastRandomPoisson(100): mean = " << (total / 100000) << ", expected 100" << std::endl;
	
	for (total = 0.0, i = 0; i < 100000; i++)
		total += gsl_ran_poisson(EIDOS_GSL_RNG, 100);
	
	std::cout << "gsl_ran_poisson(100): mean = " << (total / 100000) << ", expected 100" << std::endl << std::endl;
	
	
	std::cout << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += Eidos_FastRandomPoisson(1.0, exp(-1.0));
	
	std::cout << "Eidos_FastRandomPoisson(1.0): mean = " << (total / 1000000) << ", expected 1.0" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += gsl_ran_poisson(EIDOS_GSL_RNG, 1.0);
	
	std::cout << "gsl_ran_poisson(1.0): mean = " << (total / 1000000) << ", expected 1.0" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += Eidos_FastRandomPoisson(0.001, exp(-0.001));
	
	std::cout << "Eidos_FastRandomPoisson(0.001): mean = " << (total / 1000000) << ", expected 0.001" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += gsl_ran_poisson(EIDOS_GSL_RNG, 0.001);
	
	std::cout << "gsl_ran_poisson(0.001): mean = " << (total / 1000000) << ", expected 0.001" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += Eidos_FastRandomPoisson(0.00001, exp(-0.00001));
	
	std::cout << "Eidos_FastRandomPoisson(0.00001): mean = " << (total / 1000000) << ", expected 0.00001" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += gsl_ran_poisson(EIDOS_GSL_RNG, 0.00001);
	
	std::cout << "gsl_ran_poisson(0.00001): mean = " << (total / 1000000) << ", expected 0.00001" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 100000; i++)
		total += Eidos_FastRandomPoisson(100, exp(-100));
	
	std::cout << "Eidos_FastRandomPoisson(100): mean = " << (total / 100000) << ", expected 100" << std::endl;
	
	for (total = 0.0, i = 0; i < 100000; i++)
		total += gsl_ran_poisson(EIDOS_GSL_RNG, 100);
	
	std::cout << "gsl_ran_poisson(100): mean = " << (total / 100000) << ", expected 100" << std::endl << std::endl;
	
	
	std::cout << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += Eidos_FastRandomPoisson_NONZERO(1.0, exp(-1.0));
	
	std::cout << "Eidos_FastRandomPoisson(1.0): mean = " << (total / 1000000) << ", expected ~1.58" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
	{
		unsigned int x;
		
		do {
			x = gsl_ran_poisson(EIDOS_GSL_RNG, 1.0);
		} while (x == 0);
		
		total += x;
	}
	
	std::cout << "gsl_ran_poisson(1.0): mean = " << (total / 1000000) << ", expected ~1.58" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += Eidos_FastRandomPoisson_NONZERO(0.001, exp(-0.001));
	
	std::cout << "Eidos_FastRandomPoisson(0.001): mean = " << (total / 1000000) << ", expected ~1.0005" << std::endl;
	
	//	for (total = 0.0, i = 0; i < 1000000; i++)
	//	{
	//		unsigned int x;
	//		
	//		do {
	//			x = gsl_ran_poisson(EIDOS_GSL_RNG, 0.001);
	//		} while (x == 0);
	//		
	//		total += x;
	//	}
	//	
	//	std::cout << "gsl_ran_poisson(0.001): mean = " << (total / 1000000) << ", expected ~1.0005" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += Eidos_FastRandomPoisson_NONZERO(0.00001, exp(-0.00001));
	
	std::cout << std::endl << "Eidos_FastRandomPoisson(0.00001): mean = " << (total / 1000000) << ", expected ~1.00001" << std::endl;
	
	//	for (total = 0.0, i = 0; i < 1000000; i++)
	//	{
	//		unsigned int x;
	//		
	//		do {
	//			x = gsl_ran_poisson(EIDOS_GSL_RNG, 0.00001);
	//		} while (x == 0);
	//		
	//		total += x;
	//	}
	//	
	//	std::cout << "gsl_ran_poisson(0.00001): mean = " << (total / 1000000) << ", expected ~1.00001" << std::endl;
	
	for (total = 0.0, i = 0; i < 100000; i++)
		total += Eidos_FastRandomPoisson_NONZERO(100, exp(-100));
	
	std::cout << std::endl << "Eidos_FastRandomPoisson(100): mean = " << (total / 100000) << ", expected ~100" << std::endl;
	
	for (total = 0.0, i = 0; i < 100000; i++)
	{
		unsigned int x;
		
		do {
			x = gsl_ran_poisson(EIDOS_GSL_RNG, 100);
		} while (x == 0);
		
		total += x;
	}
	
	std::cout << "gsl_ran_poisson(100): mean = " << (total / 100000) << ", expected ~100" << std::endl << std::endl;
	
#endif
	
#if 0
	// Speed tests of gsl_ran_poisson() vs. Eidos_FastRandomPoisson()
	// When built with optimization, this indicates that (on my present machine) the GSL's method
	// starts to be faster at about mu > 250, so we will cross over at that point.
	for (int iteration = 0; iteration <= 19; ++iteration)
	{
		double mu = 0, exp_neg_mu;
		
		if (iteration == 0) mu = 0.1;
		if (iteration == 1) mu = 0.4;
		if (iteration == 2) mu = 1.0;
		if (iteration == 3) mu = 4.0;
		if (iteration == 4) mu = 10.0;
		if (iteration == 5) mu = 40.0;
		if (iteration == 6) mu = 100.0;
		if (iteration == 7) mu = 200.0;
		if (iteration == 8) mu = 220.0;
		if (iteration == 9) mu = 240.0;
		if (iteration == 10) mu = 260.0;
		if (iteration == 11) mu = 280.0;
		if (iteration == 12) mu = 300.0;
		if (iteration == 13) mu = 400.0;
		if (iteration == 14) mu = 500.0;
		if (iteration == 15) mu = 600.0;
		if (iteration == 16) mu = 700.0;
		if (iteration == 17) mu = 1000.0;
		if (iteration == 18) mu = 10000.0;
		if (iteration == 19) mu = 100000.0;
		
		exp_neg_mu = Eidos_FastRandomPoisson_PRECALCULATE(mu);
		
		for (int type = 0; type <= 2; ++type)
		{
			double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
			double total = 0.0;
			
			if (type == 0)
			{
				for (int i = 0; i < 1000000; i++)
					total += Eidos_FastRandomPoisson(mu);
			}
			else if (type == 1)
			{
				for (int i = 0; i < 1000000; i++)
					total += Eidos_FastRandomPoisson(mu, exp_neg_mu);
			}
			else if (type == 2)
			{
				for (int i = 0; i < 1000000; i++)
					total += gsl_ran_poisson(EIDOS_GSL_RNG, mu);
			}
			
			double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
			
			std::cout << "mu " << mu << " T " << type << ": total = " << total << ", time == " << (end_time - start_time) << std::endl;
		}
	}
#endif
	
#if 0
	// Speed tests of gsl_rng_uniform_int() / Eidos_rng_uniform_int() / Eidos_rng_uniform_int_MT64 / C++'s MT64
	// The use of total here is a thunk to force the compiler not to optimize away any work
	// Results (12 May 2018, Release build):
	//
	//		gsl_rng_uniform_int(): time == 18.5712, total == 249506669929
	//		Eidos_rng_uniform_int(): time == 2.83272, total == 249504929021
	//		Eidos_rng_uniform_int_MT64(): time == 3.28454, total == 249512506686
	//		std::mt19937_64: time == 16.9182, total == 249496346730
	//
	// So Eidos_rng_uniform_int_MT64() is only marginally slower than Eidos_rng_uniform_int(), whereas the
	// straight GSL and C++ RNGs are much slower.  The straight GSL is not inlined; I'm not sure why the C++
	// MT64 is so slow.
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		int64_t total = 0;
		
		for (int64_t iteration = 0; iteration < 1000000000; ++iteration)
			total += gsl_rng_uniform_int(EIDOS_GSL_RNG, 500);
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << std::endl << "gsl_rng_uniform_int(): time == " << (end_time - start_time) << ", total == " << total << std::endl;
	}
	
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		int64_t total = 0;
		
		for (int64_t iteration = 0; iteration < 1000000000; ++iteration)
			total += Eidos_rng_uniform_int(EIDOS_GSL_RNG, 500);
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "Eidos_rng_uniform_int(): time == " << (end_time - start_time) << ", total == " << total << std::endl;
	}
	
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		int64_t total = 0;
		init_genrand64(0);
		
		for (int64_t iteration = 0; iteration < 1000000000; ++iteration)
			total += Eidos_rng_uniform_int_MT64(500);
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "Eidos_rng_uniform_int_MT64(): time == " << (end_time - start_time) << ", total == " << total << std::endl;
	}
	
	{
		std::random_device rd;
		std::mt19937_64 e2(rd());
		std::uniform_int_distribution<long long int> dist(0, 499);
		int64_t total = 0;
		
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		for (int64_t iteration = 0; iteration < 1000000000; ++iteration)
			total += dist(e2);
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "std::mt19937_64: time == " << (end_time - start_time) << ", total == " << total << std::endl;
	}
#endif
	
#if 0
	// Speed tests of gsl_rng_uniform() versus Eidos_rng_uniform()
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		for (int64_t iteration = 0; iteration < 100000000; ++iteration)
			gsl_rng_uniform(EIDOS_GSL_RNG);
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << std::endl << "gsl_rng_uniform(): time == " << (end_time - start_time) << std::endl;
	}
	
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		for (int64_t iteration = 0; iteration < 100000000; ++iteration)
			Eidos_rng_uniform(EIDOS_GSL_RNG);
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "Eidos_rng_uniform(): time == " << (end_time - start_time) << std::endl;
	}
#endif
	
#if 0
	// Speed tests of gsl_rng_uniform_pos() versus Eidos_rng_uniform_pos()
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		for (int64_t iteration = 0; iteration < 100000000; ++iteration)
			gsl_rng_uniform_pos(EIDOS_GSL_RNG);
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << std::endl << "gsl_rng_uniform_pos(): time == " << (end_time - start_time) << std::endl;
	}
	
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		for (int64_t iteration = 0; iteration < 100000000; ++iteration)
			Eidos_rng_uniform_pos(EIDOS_GSL_RNG);
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "Eidos_rng_uniform_pos(): time == " << (end_time - start_time) << std::endl;
	}
#endif
	
#if 0
	{
		// Test that our inline, modified version of taus_get() is equivalent to the GSL's taus_get()
		unsigned long int *gsl_taus, *eidos_taus, *mixed_taus;
		int iter;
		
		gsl_taus = (unsigned long int *)malloc(100000 * sizeof(unsigned long int));
		eidos_taus = (unsigned long int *)malloc(100000 * sizeof(unsigned long int));
		mixed_taus = (unsigned long int *)malloc(100000 * sizeof(unsigned long int));
		
		Eidos_InitializeRNG();
		Eidos_SetRNGSeed(10);
		
		for (iter = 0; iter < 100000; ++iter)
			gsl_taus[iter] = gsl_rng_get(EIDOS_GSL_RNG);
		
		Eidos_InitializeRNG();
		Eidos_SetRNGSeed(10);
		
		for (iter = 0; iter < 100000; ++iter)
			eidos_taus[iter] = taus_get_inline(EIDOS_GSL_RNG->state);
		
		Eidos_InitializeRNG();
		Eidos_SetRNGSeed(10);
		
		for (iter = 0; iter < 50000; ++iter)
		{
			mixed_taus[iter * 2] = gsl_rng_get(EIDOS_GSL_RNG);
			mixed_taus[iter * 2 + 1] = taus_get_inline(EIDOS_GSL_RNG->state);
		}
		
		for (iter = 0; iter < 50000; ++iter)
		{
			unsigned long int a = gsl_taus[iter];
			unsigned long int b = eidos_taus[iter];
			unsigned long int c = mixed_taus[iter];
			
			if ((a != b) || (b != c))
			{
				std::cout << std::endl << "RNG mismatch: a == " << a << ", b == " << b << ", c == " << c << "." << std::endl;
				break;
			}
		}
		
		if (iter == 50000)
			std::cout << std::endl << "RNGs match." << std::endl;
		
		free(gsl_taus);
		free(eidos_taus);
		free(mixed_taus);
	}
#endif
	
#if 0
	// Speed tests of different timing methods; we need a very fast method for profiling
	// Note that the total_time variables are meaningless; they are just thunks to force the code to include the overhead of understanding the results of the calls
	
	// clock()
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
			total_time += std::clock();
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to std::clock(): time == " << (end_time - start_time) << ", total_time == " << (total_time / CLOCKS_PER_SEC) << std::endl;
	}
	
	// gettimeofday()
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		struct timeval timer;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			gettimeofday(&timer, NULL);
			
			total_time += ((int64_t)timer.tv_sec) * 1000000 + timer.tv_usec;
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to gettimeofday(): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000.0) << std::endl;
	}
	
	// clock_gettime(CLOCK_REALTIME, ...)
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		struct timespec timer;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			clock_gettime(CLOCK_REALTIME, &timer);
			
			total_time += ((int64_t)timer.tv_sec) * 1000000000 + timer.tv_nsec;
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime(CLOCK_REALTIME, ...): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime(CLOCK_MONOTONIC_RAW, ...)
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		struct timespec timer;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			clock_gettime(CLOCK_MONOTONIC_RAW, &timer);
			
			total_time += ((int64_t)timer.tv_sec) * 1000000000 + timer.tv_nsec;
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime(CLOCK_MONOTONIC_RAW, ...): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime(CLOCK_UPTIME_RAW, ...)
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		struct timespec timer;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			clock_gettime(CLOCK_UPTIME_RAW, &timer);
			
			total_time += ((int64_t)timer.tv_sec) * 1000000000 + timer.tv_nsec;
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime(CLOCK_UPTIME_RAW, ...): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime(CLOCK_PROCESS_CPUTIME_ID, ...)
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		struct timespec timer;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timer);
			
			total_time += ((int64_t)timer.tv_sec) * 1000000000 + timer.tv_nsec;
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime(CLOCK_PROCESS_CPUTIME_ID, ...): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime(CLOCK_THREAD_CPUTIME_ID, ...)
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		struct timespec timer;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &timer);
			
			total_time += ((int64_t)timer.tv_sec) * 1000000000 + timer.tv_nsec;
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime(CLOCK_THREAD_CPUTIME_ID, ...): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// std::chrono::high_resolution_clock::now()
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			__attribute__((unused)) auto timer = std::chrono::high_resolution_clock::now();
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to std::chrono::high_resolution_clock::now(): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime_nsec_np(CLOCK_REALTIME)
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += clock_gettime_nsec_np(CLOCK_REALTIME);
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime_nsec_np(CLOCK_REALTIME): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime_nsec_np(CLOCK_UPTIME_RAW)
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime_nsec_np(CLOCK_UPTIME_RAW): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime_nsec_np(CLOCK_PROCESS_CPUTIME_ID)
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += clock_gettime_nsec_np(CLOCK_PROCESS_CPUTIME_ID);
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime_nsec_np(CLOCK_PROCESS_CPUTIME_ID): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime_nsec_np(CLOCK_THREAD_CPUTIME_ID)
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += clock_gettime_nsec_np(CLOCK_THREAD_CPUTIME_ID);
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime_nsec_np(CLOCK_THREAD_CPUTIME_ID): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
#if (defined(SLIMGUI) && (SLIMPROFILING == 1))
	// Note that at present this code is in eidos_test.cpp but runs only when running in SLiMgui,
	// which never happens; this is for historical reasons, and the code can be moved if needed
	
	// mach_absolute_time()
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += mach_absolute_time();
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to mach_absolute_time(): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// mach_continuous_time()
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += mach_continuous_time();
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to mach_continuous_time(): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// Eidos_ProfileTime()
	{
		double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += Eidos_ProfileTime();
		}
		
		double end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to Eidos_ProfileTime(): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
#endif
	
	/*
	 
	 Results:
	 
	 10000000 calls to clock(): time == 3.69977, total_time == 22539996
	 10000000 calls to gettimeofday(): time == 0.427816, total_time == 5.16001e+12
	 10000000 calls to clock_gettime(CLOCK_REALTIME, ...): time == 0.434227, total_time == -5.07085e+09
	 10000000 calls to clock_gettime(CLOCK_MONOTONIC_RAW, ...): time == 0.46535, total_time == -8.45659e+09
	 10000000 calls to clock_gettime(CLOCK_UPTIME_RAW, ...): time == 0.41903, total_time == -8.45214e+09
	 10000000 calls to clock_gettime(CLOCK_PROCESS_CPUTIME_ID, ...): time == 3.53099, total_time == 7.62471e+07
	 10000000 calls to clock_gettime(CLOCK_THREAD_CPUTIME_ID, ...): time == 1.66773, total_time == 1.01822e+08
	 10000000 calls to std::chrono::high_resolution_clock::now(): time == 0.423068, total_time == 0
	 10000000 calls to clock_gettime_nsec_np(CLOCK_REALTIME): time == 0.424018, total_time == 1.34453e+10
	 10000000 calls to clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW): time == 0.415543, total_time == 1.00593e+10
	 10000000 calls to clock_gettime_nsec_np(CLOCK_UPTIME_RAW): time == 0.357498, total_time == 1.00631e+10
	 10000000 calls to clock_gettime_nsec_np(CLOCK_PROCESS_CPUTIME_ID): time == 3.51726, total_time == 1.44264e+08
	 10000000 calls to clock_gettime_nsec_np(CLOCK_THREAD_CPUTIME_ID): time == 1.62355, total_time == 1.69689e+08
	 10000000 calls to mach_absolute_time(): time == 0.203174, total_time == 1.01174e+10
	 10000000 calls to mach_continuous_time(): time == 0.2646, total_time == 1.01197e+10
	 10000000 calls to Eidos_ProfileTime(): time == 0.20364, total_time == 1.0122e+10

	 10000000 calls to clock(): time == 3.6669, total_time == 21594595
	 10000000 calls to gettimeofday(): time == 0.420807, total_time == 5.16038e+12
	 10000000 calls to clock_gettime(CLOCK_REALTIME, ...): time == 0.4383, total_time == -4.70372e+09
	 10000000 calls to clock_gettime(CLOCK_MONOTONIC_RAW, ...): time == 0.46461, total_time == -8.08945e+09
	 10000000 calls to clock_gettime(CLOCK_UPTIME_RAW, ...): time == 0.413183, total_time == -8.08511e+09
	 10000000 calls to clock_gettime(CLOCK_PROCESS_CPUTIME_ID, ...): time == 3.49895, total_time == 7.45271e+07
	 10000000 calls to clock_gettime(CLOCK_THREAD_CPUTIME_ID, ...): time == 1.66718, total_time == 1.00431e+08
	 10000000 calls to std::chrono::high_resolution_clock::now(): time == 0.419601, total_time == 0
	 10000000 calls to clock_gettime_nsec_np(CLOCK_REALTIME): time == 0.437738, total_time == 1.3812e+10
	 10000000 calls to clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW): time == 0.416851, total_time == 1.04261e+10
	 10000000 calls to clock_gettime_nsec_np(CLOCK_UPTIME_RAW): time == 0.355363, total_time == 1.043e+10
	 10000000 calls to clock_gettime_nsec_np(CLOCK_PROCESS_CPUTIME_ID): time == 3.53823, total_time == 1.42916e+08
	 10000000 calls to clock_gettime_nsec_np(CLOCK_THREAD_CPUTIME_ID): time == 1.64254, total_time == 1.68505e+08
	 10000000 calls to mach_absolute_time(): time == 0.203185, total_time == 1.04846e+10
	 10000000 calls to mach_continuous_time(): time == 0.248343, total_time == 1.04869e+10
	 10000000 calls to Eidos_ProfileTime(): time == 0.208794, total_time == 1.04892e+10
	 
	 10000000 calls to clock(): time == 3.7723, total_time == 22027941
	 10000000 calls to gettimeofday(): time == 0.402914, total_time == 5.16072e+12
	 10000000 calls to clock_gettime(CLOCK_REALTIME, ...): time == 0.439594, total_time == -4.36005e+09
	 10000000 calls to clock_gettime(CLOCK_MONOTONIC_RAW, ...): time == 0.468783, total_time == -7.74572e+09
	 10000000 calls to clock_gettime(CLOCK_UPTIME_RAW, ...): time == 0.423101, total_time == -7.74128e+09
	 10000000 calls to clock_gettime(CLOCK_PROCESS_CPUTIME_ID, ...): time == 3.59922, total_time == 7.61285e+07
	 10000000 calls to clock_gettime(CLOCK_THREAD_CPUTIME_ID, ...): time == 1.68519, total_time == 1.02508e+08
	 10000000 calls to std::chrono::high_resolution_clock::now(): time == 0.408809, total_time == 0
	 10000000 calls to clock_gettime_nsec_np(CLOCK_REALTIME): time == 0.431629, total_time == 1.41569e+10
	 10000000 calls to clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW): time == 0.419269, total_time == 1.0771e+10
	 10000000 calls to clock_gettime_nsec_np(CLOCK_UPTIME_RAW): time == 0.357072, total_time == 1.07748e+10
	 10000000 calls to clock_gettime_nsec_np(CLOCK_PROCESS_CPUTIME_ID): time == 3.50717, total_time == 1.44766e+08
	 10000000 calls to clock_gettime_nsec_np(CLOCK_THREAD_CPUTIME_ID): time == 1.61161, total_time == 1.70191e+08
	 10000000 calls to mach_absolute_time(): time == 0.203799, total_time == 1.08289e+10
	 10000000 calls to mach_continuous_time(): time == 0.262729, total_time == 1.08312e+10
	 10000000 calls to Eidos_ProfileTime(): time == 0.201686, total_time == 1.08335e+10

	 So mach_absolute_time() is the fastest, and also gives us the information in a convenient form.  It is only
	 available on OS X, of course, but that is fine since we only offer profiling in SLiMgui.  I have therefore
	 defined Eidos_ProfileTime() as using mach_absolute_time() in eidos_globals.h.
	 
	 */
#endif
	
#if 0
	{
		// Test Eidos_ExactSum() for correctness.  Seems to work; what do I know.  There is more rigorous test
		// code at http://code.activestate.com/recipes/393090/ but it assumes the existence of a correct
		// function to compare against, which we don't have.
		double *values = (double *)malloc(10000 * sizeof(double) * 4);
		double *vptr = values;
		
		for (int64_t rep = 0; rep < 10000; rep++)
		{
			*(vptr++) = 1.0;
			*(vptr++) = 1.0e100;
			*(vptr++) = 1.0;
			*(vptr++) = -1.0e100;
		}
		
		double exact_sum = Eidos_ExactSum(values, 10000 * 4);
		double inexact_sum = 0.0;
		
		vptr = values;
		for (int64_t index = 0; index < 10000 * 4; ++index)
			inexact_sum += *(vptr++);
		
		std::cout << "Inexact sum: " << inexact_sum << std::endl;
		std::cout << "Exact sum: " << exact_sum << std::endl;
		
		free(values);
	}
#endif
	
#if 0
	// Speed comparison between Poisson and binomial draws in various parameter regimes, using the GSL's own code.
	// For doing things like drawing the number of recombination or mutation events that happen across a genome,
	// the binomial distribution is technically correct (# trials, probability per trial), but we have been using
	// the Poisson distribution because it is an extremely good approximation for the binomial in the regimes we
	// normally run in (e.g., chromosomes longer than ~100 sites, per-site probabilities less than 1e-3).  And
	// that is fine.  However, some people are using SLiM with parameters where the Poisson poorly approximates
	// the binomial (10 independent loci, i.e. 9 trials, 0.5 probability per trial), so we should switch.  The
	// question is: how big is the price of switching?  Poisson is nice because it is simple and fast; binomial
	// is complex and slow.  If the price is substantial, we might continue to use Poisson in the parameter
	// regime where it is safe, and switch to binomial only when necessary; but if the price is small, we'll just
	// switch to binomial always, for simplicity.
	
	// Regarding the Poisson approximation, https://www.itl.nist.gov/div898/handbook/pmc/section3/pmc331.htm says:
	// The sample size n should be equal to or larger than 20 and the probability of a single success, p, should
	// be smaller than or equal to 0.05. If n >= 100, the approximation is excellent if np is also <= 10.
	
	// The GSL, in binomial_tpe.c, has the following comment:
	//
	//	Note, Bruce Schmeiser (personal communication) points out that if
	//	you want very fast binomial deviates, and you are happy with
	//	approximate results, and/or n and n*p are both large, then you can
	//	just use gaussian estimates: mean=n*p, variance=n*p*(1-p).
	//
	// And it turns out that the Gaussian approxiamtion is indeed as much as twice as fast as either of the other
	// methods, and is quite accurate under Schmeiser's stated conditions.  So we may want to use it; testing it.
	// Regarding this approximation, Wikipedia recommends that it be used when n > 9*(1-p)/p and n > 9*p/(1-p).
	
	// Upshot: I have encoded our rules for which distribution to prefer in the code below; the preference is
	// shown with asterisks.  Happily, this often coincides with the fastest version.  BCH 7 April 2018.
	{
		for (int trial = 0; trial < 8*11; ++trial)
		{
			double p = 0.0;
			unsigned int n = 0;
			
			switch (trial % 8)
			{
				case 0: p = 0.5; break;
				case 1: p = 0.1; break;
				case 2: p = 0.01; break;
				case 3: p = 0.001; break;
				case 4: p = 0.0001; break;
				case 5: p = 0.000001; break;
				case 6: p = 0.00000001; break;
				case 7: p = 0.0000000001; break;
			}
			
			switch (trial / 8)
			{
				case 0: n = 10; break;
				case 1: n = 15; break;
				case 2: n = 20; break;
				case 3: n = 25; break;
				case 4: n = 50; break;
				case 5: n = 100; break;
				case 6: n = 200; break;
				case 7: n = 1000; break;
				case 8: n = 10000; break;
				case 9: n = 10000000; break;
				case 10: n = 1000000000; break;
			}
			
			std::cout << "Case #" << trial << " (" << n << " trials, " << p << " probability):" << std::endl;
			
			// decide which version will be used in SLiM
			int pref = 0;
			bool gaussian_allowed = ((n > 9 * (1-p) / p) && (n > 9 * p / (1-p)));
			bool poisson_allowed = ((p <= 0.01) && (n > 50));
			
			gaussian_allowed = false;
			
			if ((n >= 20) && gaussian_allowed)			// Gaussian; (n >= 20) is because binomial is faster before then anyway
				pref = 3;
			else if ((n * p <= 1.0) && poisson_allowed)	// Poisson; (n * p > 1.0) because binomial is faster before then anyway
				pref = 2;
			else										// binomial
				pref = 1;
			
			{
				std::clock_t begin = std::clock();
				int64_t total = 0;
				
				for (int64_t i = 0; i < 20000000; i++)
					total += gsl_ran_binomial(EIDOS_GSL_RNG, p, n);
				
				double time_spent = static_cast<double>(clock() - begin) / CLOCKS_PER_SEC;
				std::cout << ((pref == 1) ? "***" : "   ");
				std::cout << "time for 20000000 calls to gsl_ran_binomial(): " << time_spent << " (total == " << total << ")" << std::endl;
			}
			if (poisson_allowed)
			{
				std::clock_t begin = std::clock();
				int64_t total = 0;
				
				for (int64_t i = 0; i < 20000000; i++)
					total += gsl_ran_poisson(EIDOS_GSL_RNG, p * n);
				
				double time_spent = static_cast<double>(clock() - begin) / CLOCKS_PER_SEC;
				std::cout << ((pref == 2) ? "***" : "   ");
				std::cout << "time for 20000000 calls to gsl_ran_poisson(): " << time_spent << " (total == " << total << ")" << std::endl;
			}
			if (gaussian_allowed)
			{
				std::clock_t begin = std::clock();
				int64_t total = 0;
				double sd = sqrt(n * p * (1 - p));
				
				for (int64_t i = 0; i < 20000000; i++)
					total += (int)round(gsl_ran_gaussian(EIDOS_GSL_RNG, sd) + p * n);
				
				double time_spent = static_cast<double>(clock() - begin) / CLOCKS_PER_SEC;
				std::cout << ((pref == 3) ? "***" : "   ");
				std::cout << "time for 20000000 calls to gsl_ran_gaussian(): " << time_spent << " (total == " << total << ")" << std::endl;
			}
			std::cout << std::endl;
		}
	}
#endif
	
	// If we ran tests, the random number seed has been set; let's set it back to a good seed value
	Eidos_InitializeRNG();
	Eidos_SetRNGSeed(Eidos_GenerateSeedFromPIDAndTime());
	
	// return a standard Unix result code indicating success (0) or failure (1);
	return (gEidosTestFailureCount > 0) ? 1 : 0;
}

#pragma mark literals & identifiers
void _RunLiteralsIdentifiersAndTokenizationTests(void)
{
	// test literals, built-in identifiers, and tokenization
	EidosAssertScriptSuccess("3;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("3e2;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(300)));
	EidosAssertScriptSuccess("3.1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.1)));
	EidosAssertScriptSuccess("3.1e2;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.1e2)));
	EidosAssertScriptSuccess("3.1e-2;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.1e-2)));
	EidosAssertScriptSuccess("3.1e+2;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.1e+2)));
	EidosAssertScriptSuccess("'foo';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("'foo\\tbar';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo\tbar")));
	EidosAssertScriptSuccess("'\\'foo\\'\\t\\\"bar\"';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("'foo'\t\"bar\"")));
	EidosAssertScriptSuccess("\"foo\";", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("\"foo\\tbar\";", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo\tbar")));
	EidosAssertScriptSuccess("\"\\'foo'\\t\\\"bar\\\"\";", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("'foo'\t\"bar\"")));
	EidosAssertScriptSuccess("<<\n'foo'\n\"bar\"\n>>;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("'foo'\n\"bar\"")));
	EidosAssertScriptSuccess("<<---\n'foo'\n\"bar\"\n>>---;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("'foo'\n\"bar\"")));
	EidosAssertScriptSuccess("<<<<\n'foo'\n\"bar\"\n>><<;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("'foo'\n\"bar\"")));
	EidosAssertScriptSuccess("<<<<\n'foo'\n\"bar>><\"\n>><<;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("'foo'\n\"bar>><\"")));
	EidosAssertScriptSuccess("T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("NULL;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("INF;", gStaticEidosValue_FloatINF);
	EidosAssertScriptSuccess("-INF;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-std::numeric_limits<double>::infinity())));
	EidosAssertScriptSuccess("NAN;", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("E - exp(1) < 0.0000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("PI - asin(1)*2 < 0.0000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("foo$foo;", 3, "unexpected token '$'");
	EidosAssertScriptRaise("foo#foo;", 3, "unrecognized token");
	EidosAssertScriptRaise("3..5;", 3, "unexpected token");		// second period is a dot operator!
	EidosAssertScriptRaise("3ee5;", 0, "unrecognized token");
	EidosAssertScriptRaise("3e-+5;", 0, "unrecognized token");
	EidosAssertScriptRaise("3e-;", 0, "unrecognized token");
	EidosAssertScriptRaise("3e;", 0, "unrecognized token");
	EidosAssertScriptRaise("'foo' + 'foo;", 8, "unexpected EOF");
	EidosAssertScriptRaise("'foo' + 'foo\\q';", 12, "illegal escape");
	EidosAssertScriptRaise("'foo' + 'foo\\", 8, "unexpected EOF");
	EidosAssertScriptRaise("'foo' + 'foo\n';", 8, "illegal newline");
	EidosAssertScriptRaise("1e100;", 0, "could not be represented");							// out of range for integer
	EidosAssertScriptRaise("1000000000000000000000000000;", 0, "could not be represented");		// out of range for integer
	EidosAssertScriptRaise("1.0e100000000000;", 0, "could not be represented");					// out of range for double
	EidosAssertScriptRaise("T = 5;", 2, "is a constant");
	EidosAssertScriptRaise("F = 5;", 2, "is a constant");
	EidosAssertScriptRaise("NULL = 5;", 5, "is a constant");
	EidosAssertScriptRaise("INF = 5;", 4, "is a constant");
	EidosAssertScriptRaise("NAN = 5;", 4, "is a constant");
	EidosAssertScriptRaise("E = 5;", 2, "is a constant");
	EidosAssertScriptRaise("PI = 5;", 3, "is a constant");
	
	// try harder to overwrite a constant
	EidosAssertScriptRaise("T = F;", 2, "is a constant");
	EidosAssertScriptRaise("T[0] = F;", 5, "is a constant");
	EidosAssertScriptRaise("T[0][0] = F;", 8, "is a constant");
	EidosAssertScriptRaise("T = !T;", 2, "is a constant");
	EidosAssertScriptRaise("for (T in c(F, F)) 5;", 5, "is a constant");
	
	EidosAssertScriptRaise("PI = 3;", 3, "is a constant");
	EidosAssertScriptRaise("PI = 3.0;", 3, "is a constant");
	EidosAssertScriptRaise("PI[0] = 3;", 6, "is a constant");
	EidosAssertScriptRaise("PI[0] = 3.0;", 6, "is a constant");
	EidosAssertScriptRaise("PI[0][0] = 3;", 9, "is a constant");
	EidosAssertScriptRaise("PI[0][0] = 3.0;", 9, "is a constant");
	EidosAssertScriptRaise("PI = PI + 1;", 3, "is a constant");
	EidosAssertScriptRaise("PI = PI + 1.0;", 3, "is a constant");
	EidosAssertScriptRaise("PI = PI - 1;", 3, "is a constant");
	EidosAssertScriptRaise("PI = PI - 1.0;", 3, "is a constant");
	EidosAssertScriptRaise("PI = PI * 2;", 3, "is a constant");
	EidosAssertScriptRaise("PI = PI * 2.0;", 3, "is a constant");
	EidosAssertScriptRaise("PI = PI / 2;", 3, "is a constant");
	EidosAssertScriptRaise("PI = PI / 2.0;", 3, "is a constant");
	EidosAssertScriptRaise("for (PI in c(3, 4)) 5;", 5, "is a constant");
	EidosAssertScriptRaise("for (PI in c(3.0, 4.0)) 5;", 5, "is a constant");
	
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = 3;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = 3.0;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q[0] = 3;", 29, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q[0] = 3.0;", 29, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q[0][0] = 3;", 32, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q[0][0] = 3.0;", 32, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = Q + 1;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = Q + 1.0;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = Q - 1;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = Q - 1.0;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = Q * 2;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = Q * 2.0;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = Q / 2;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = Q / 2.0;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); for (Q in c(3, 4)) 5;", 29, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); for (Q in c(3.0, 4.0)) 5;", 29, "is a constant");
}

#pragma mark symbol table
void _RunSymbolsAndVariablesTests(void)
{
	// test symbol table and variable dynamics
	EidosAssertScriptSuccess("x = 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("x = 3.1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.1)));
	EidosAssertScriptSuccess("x = 'foo'; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("x = T; x;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = NULL; x;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x = 'first'; x = 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("x = 'first'; x = 3.1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.1)));
	EidosAssertScriptSuccess("x = 'first'; x = 'foo'; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("x = 'first'; x = T; x;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = 'first'; x = NULL; x;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x = 1:5; y = x + 1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; y = x + 1; y;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("x = 1:5; y = x + 1; x = x + 1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("x = 1:5; y = x + 1; x = x + 1; y;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("x = 1:5; y = x; x = x + 1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("x = 1:5; y = x; x = x + 1; y;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; y = x; x = x + x; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 4, 6, 8, 10}));
	EidosAssertScriptSuccess("x = 1:5; y = x; x = x + x; y;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; y = x; x[1] = 0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 0, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; y = x; x[1] = 0; y;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; y = x; y[1] = 0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; y = x; y[1] = 0; y;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 0, 3, 4, 5}));
	EidosAssertScriptSuccess("for (i in 1:3) { x = 1:5; x[1] = x[1] + 1; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 3, 4, 5}));
}

#pragma mark parsing
void _RunParsingTests(void)
{
	// test some simple parsing errors
	EidosAssertScriptRaise("5 + 5", 5, "unexpected token");					// missing ;
	EidosAssertScriptRaise("{ 5;", 4, "unexpected token");					// missing }
	EidosAssertScriptRaise("5 };", 2, "unexpected token");					// missing {
	EidosAssertScriptRaise("(5 + 7;", 6, "unexpected token");				// missing )
	EidosAssertScriptRaise("5 + 7);", 5, "unexpected token");				// missing (
	EidosAssertScriptRaise("a[5;", 3, "unexpected token");					// missing ]
	EidosAssertScriptRaise("a 5];", 2, "unexpected token");					// missing ]
	EidosAssertScriptRaise("a(5;", 3, "unexpected token");					// missing )
	EidosAssertScriptRaise("a 5);", 2, "unexpected token");					// missing (
	EidosAssertScriptRaise("a.;", 2, "unexpected token");					// missing identifier
	EidosAssertScriptRaise("if (5 T;", 6, "unexpected token");				// missing )
	EidosAssertScriptRaise("if 5) T;", 3, "unexpected token");				// missing (
	EidosAssertScriptRaise("if (5) else 5;", 7, "unexpected token");		// missing statement
	EidosAssertScriptRaise("do ; (T);", 5, "unexpected token");				// missing while
	EidosAssertScriptRaise("do ; while T);", 11, "unexpected token");		// missing (
	EidosAssertScriptRaise("do ; while (T;", 13, "unexpected token");		// missing )
	EidosAssertScriptRaise("while T);", 6, "unexpected token");				// missing (
	EidosAssertScriptRaise("while (T;", 8, "unexpected token");				// missing )
	EidosAssertScriptRaise("for;", 3, "unexpected token");					// missing range
	EidosAssertScriptRaise("for (x);", 6, "unexpected token");				// missing in
	EidosAssertScriptRaise("for (x in);", 9, "unexpected token");			// missing range
	EidosAssertScriptRaise("for (in 3:5);", 5, "unexpected token");			// missing range variable
	EidosAssertScriptRaise("for (x in 3:5;", 13, "unexpected token");		// missing )
	EidosAssertScriptRaise("for x in 3:5) ;", 4, "unexpected token");		// missing (
	EidosAssertScriptRaise("next 5;", 5, "unexpected token");				// missing ;
	EidosAssertScriptRaise("break 5;", 6, "unexpected token");				// missing ;
}

#pragma mark dispatch
void _RunFunctionDispatchTests(void)
{
	// test function dispatch, default arguments, and named arguments
	EidosAssertScriptSuccess("abs(-10);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptRaise("abs();", 0, "missing required argument x");
	EidosAssertScriptRaise("abs(-10, -10);", 0, "too many arguments supplied");
	EidosAssertScriptSuccess("abs(x=-10);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptRaise("abs(y=-10);", 0, "skipped over required argument");
	EidosAssertScriptRaise("abs(x=-10, x=-10);", 0, "supplied more than once");
	EidosAssertScriptRaise("abs(x=-10, y=-10);", 0, "unrecognized named argument y");
	EidosAssertScriptRaise("abs(y=-10, x=-10);", 0, "skipped over required argument");
	
	EidosAssertScriptSuccess("integerDiv(6, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptRaise("integerDiv(6, 3, 3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("integerDiv(6);", 0, "missing required argument y");
	EidosAssertScriptSuccess("integerDiv(x=6, y=3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptRaise("integerDiv(y=6, 3);", 0, "skipped over required argument");
	EidosAssertScriptRaise("integerDiv(y=6, x=3);", 0, "skipped over required argument");
	EidosAssertScriptRaise("integerDiv(x=6, 3);", 0, "unnamed argument may not follow after named arguments");
	EidosAssertScriptSuccess("integerDiv(6, y=3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	
	EidosAssertScriptSuccess("seq(1, 3, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3}));
	EidosAssertScriptSuccess("seq(1, 3, NULL);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3}));
	EidosAssertScriptSuccess("seq(1, 3, by=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3}));
	EidosAssertScriptSuccess("seq(1, 3, by=NULL);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3}));
	EidosAssertScriptRaise("seq(1, 3, x=1);", 0, "ran out of optional arguments");
	EidosAssertScriptRaise("seq(1, 3, by=1, length=1, by=1);", 0, "supplied more than once");
	EidosAssertScriptRaise("seq(1, 3, length=1, by=1);", 0, "supplied out of order");
	EidosAssertScriptSuccess("seq(1, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3}));
	EidosAssertScriptRaise("seq(by=1, 1, 3);", 0, "named argument by skipped over required argument");
	EidosAssertScriptRaise("seq(by=NULL, 1, 3);", 0, "named argument by skipped over required argument");
	
	EidosAssertScriptSuccess("c();", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("c(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("c(2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("c(1, 2, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3}));
	EidosAssertScriptRaise("c(x=2);", 0, "unrecognized named argument x");
	EidosAssertScriptRaise("c(x=1, 2, 3);", 0, "unrecognized named argument x");
	EidosAssertScriptRaise("c(1, x=2, 3);", 0, "unrecognized named argument x");
	EidosAssertScriptRaise("c(1, 2, x=3);", 0, "unrecognized named argument x");
	
	EidosAssertScriptSuccess("doCall('abs', -10);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("doCall(functionName='abs', -10);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptRaise("doCall(x='abs', -10);", 0, "skipped over required argument");
	EidosAssertScriptRaise("doCall('abs', x=-10);", 0, "unrecognized named argument x");
	EidosAssertScriptRaise("doCall('abs', functionName=-10);", 0, "could not be matched");
	EidosAssertScriptRaise("doCall(x='abs');", 0, "skipped over required argument");
	EidosAssertScriptRaise("doCall(functionName='abs');", 0, "requires 1 argument(s), but 0 are supplied");
	
	EidosAssertScriptRaise("foobaz();", 0, "unrecognized function name");
	EidosAssertScriptRaise("_Test(7).foobaz();", 9, "method foobaz() is not defined");
}

#pragma mark runtime
void _RunRuntimeErrorTests(void)
{
	// test some simple runtime errors
	EidosAssertScriptRaise("x = y * 3;", 4, "undefined identifier");									// undefined variable referenced
	EidosAssertScriptRaise("print(y * 3);", 6, "undefined identifier");									// undefined variable referenced as function argument
	
	EidosAssertScriptRaise("x = T; x[1];", 8, "out of range");									// subscript out of range (singleton logical)
	EidosAssertScriptRaise("x = T; x[-1];", 8, "out of range");									// subscript out of range (singleton logical)
	EidosAssertScriptRaise("x = T; x[1] = T;", 8, "out-of-range index");								// subscript out of range in assignment (singleton logical)
	EidosAssertScriptRaise("x = T; x[-1] = T;", 8, "out-of-range index");								// subscript out of range in assignment (singleton logical)
	EidosAssertScriptRaise("x = c(T,F); x[2];", 13, "out of range");							// subscript out of range (vector logical)
	EidosAssertScriptRaise("x = c(T,F); x[-1];", 13, "out of range");							// subscript out of range (vector logical)
	EidosAssertScriptRaise("x = c(T,F); x[2] = F;", 13, "out-of-range index");						// subscript out of range in assignment (vector logical)
	EidosAssertScriptRaise("x = c(T,F); x[-1] = F;", 13, "out-of-range index");						// subscript out of range in assignment (vector logical)

	EidosAssertScriptRaise("x = 8; x[1];", 8, "out of range");									// subscript out of range (singleton int)
	EidosAssertScriptRaise("x = 8; x[-1];", 8, "out of range");									// subscript out of range (singleton int)
	EidosAssertScriptRaise("x = 8; x[1] = 7;", 8, "out-of-range index");								// subscript out of range in assignment (singleton int)
	EidosAssertScriptRaise("x = 8; x[-1] = 7;", 8, "out-of-range index");								// subscript out of range in assignment (singleton int)
	EidosAssertScriptRaise("x = 7:9; x[3];", 10, "out of range");								// subscript out of range (vector int)
	EidosAssertScriptRaise("x = 7:9; x[-1];", 10, "out of range");								// subscript out of range (vector int)
	EidosAssertScriptRaise("x = 7:9; x[3] = 12;", 10, "out-of-range index");							// subscript out of range in assignment (vector int)
	EidosAssertScriptRaise("x = 7:9; x[-1] = 12;", 10, "out-of-range index");							// subscript out of range in assignment (vector int)

	EidosAssertScriptRaise("x = 8.0; x[1];", 10, "out of range");								// subscript out of range (singleton float)
	EidosAssertScriptRaise("x = 8.0; x[-1];", 10, "out of range");								// subscript out of range (singleton float)
	EidosAssertScriptRaise("x = 8.0; x[1] = 7.0;", 10, "out-of-range index");							// subscript out of range in assignment (singleton float)
	EidosAssertScriptRaise("x = 8.0; x[-1] = 7.0;", 10, "out-of-range index");						// subscript out of range in assignment (singleton float)
	EidosAssertScriptRaise("x = 7.0:9; x[3];", 12, "out of range");								// subscript out of range (vector float)
	EidosAssertScriptRaise("x = 7.0:9; x[-1];", 12, "out of range");							// subscript out of range (vector float)
	EidosAssertScriptRaise("x = 7.0:9; x[3] = 12.0;", 12, "out-of-range index");						// subscript out of range in assignment (vector float)
	EidosAssertScriptRaise("x = 7.0:9; x[-1] = 12.0;", 12, "out-of-range index");						// subscript out of range in assignment (vector float)

	EidosAssertScriptRaise("x = 'foo'; x[1];", 12, "out of range");								// subscript out of range (singleton string)
	EidosAssertScriptRaise("x = 'foo'; x[-1];", 12, "out of range");							// subscript out of range (singleton string)
	EidosAssertScriptRaise("x = 'foo'; x[1] = _Test(6);", 12, "out-of-range index");					// subscript out of range in assignment (singleton string)
	EidosAssertScriptRaise("x = 'foo'; x[-1] = _Test(6);", 12, "out-of-range index");					// subscript out of range in assignment (singleton string)
	EidosAssertScriptRaise("x = c('foo', 'bar'); x[2];", 22, "out of range");					// subscript out of range (vector string)
	EidosAssertScriptRaise("x = c('foo', 'bar'); x[-1];", 22, "out of range");					// subscript out of range (vector string)
	EidosAssertScriptRaise("x = c('foo', 'bar'); x[2] = _Test(6);", 22, "out-of-range index");		// subscript out of range in assignment (vector string)
	EidosAssertScriptRaise("x = c('foo', 'bar'); x[-1] = _Test(6);", 22, "out-of-range index");		// subscript out of range in assignment (vector string)

	EidosAssertScriptRaise("x = _Test(8); x[1];", 15, "out of range");							// subscript out of range (singleton object)
	EidosAssertScriptRaise("x = _Test(8); x[-1];", 15, "out of range");							// subscript out of range (singleton object)
	EidosAssertScriptRaise("x = _Test(8); x[1] = _Test(6);", 15, "out-of-range index");				// subscript out of range in assignment (singleton object)
	EidosAssertScriptRaise("x = _Test(8); x[-1] = _Test(6);", 15, "out-of-range index");				// subscript out of range in assignment (singleton object)
	EidosAssertScriptRaise("x = rep(_Test(8), 2); x[2];", 23, "out of range");					// subscript out of range (vector object)
	EidosAssertScriptRaise("x = rep(_Test(8), 2); x[-1];", 23, "out of range");					// subscript out of range (vector object)
	EidosAssertScriptRaise("x = rep(_Test(8), 2); x[2] = _Test(6);", 23, "out-of-range index");		// subscript out of range in assignment (vector object)
	EidosAssertScriptRaise("x = rep(_Test(8), 2); x[-1] = _Test(6);", 23, "out-of-range index");		// subscript out of range in assignment (vector object)
}

#pragma mark vectors & singletons
void _RunVectorsAndSingletonsTests(void)
{	
	// test vector-to-singleton comparisons for integers, and multiplexing of methods and properties declared as singleton
	EidosAssertScriptSuccess("rep(1:3, 2) == 2;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, false, false, true, false}));
	EidosAssertScriptSuccess("rep(1:3, 2) != 2;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, true, false, true}));
	EidosAssertScriptSuccess("rep(1:3, 2) < 2;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, true, false, false}));
	EidosAssertScriptSuccess("rep(1:3, 2) <= 2;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, false, true, true, false}));
	EidosAssertScriptSuccess("rep(1:3, 2) > 2;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false, false, true}));
	EidosAssertScriptSuccess("rep(1:3, 2) >= 2;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, true, false, true, true}));
	
	EidosAssertScriptSuccess("2 == rep(1:3, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, false, false, true, false}));
	EidosAssertScriptSuccess("2 != rep(1:3, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, true, false, true}));
	EidosAssertScriptSuccess("2 > rep(1:3, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, true, false, false}));
	EidosAssertScriptSuccess("2 >= rep(1:3, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, false, true, true, false}));
	EidosAssertScriptSuccess("2 < rep(1:3, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false, false, true}));
	EidosAssertScriptSuccess("2 <= rep(1:3, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, true, false, true, true}));
	
	EidosAssertScriptSuccess("_Test(2)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("c(_Test(2),_Test(3))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("_Test(2)[F]._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{}));
	
	EidosAssertScriptSuccess("_Test(2)._cubicYolk();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(8)));
	EidosAssertScriptSuccess("c(_Test(2),_Test(3))._cubicYolk();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{8, 27}));
	EidosAssertScriptSuccess("_Test(2)[F]._cubicYolk();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{}));
	
	EidosAssertScriptSuccess("_Test(2)._increment._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("c(_Test(2),_Test(3))._increment._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4}));
	EidosAssertScriptSuccess("_Test(2)[F]._increment._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{}));
	
	EidosAssertScriptSuccess("_Test(2)._increment._cubicYolk();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(27)));
	EidosAssertScriptSuccess("c(_Test(2),_Test(3))._increment._cubicYolk();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{27, 64}));
	EidosAssertScriptSuccess("_Test(2)[F]._increment._cubicYolk();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{}));
	
	EidosAssertScriptSuccess("_Test(2)._squareTest()._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(4)));
	EidosAssertScriptSuccess("c(_Test(2),_Test(3))._squareTest()._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 9}));
	EidosAssertScriptSuccess("_Test(2)[F]._squareTest()._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{}));
	
	EidosAssertScriptSuccess("_Test(2)._squareTest()._cubicYolk();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(64)));
	EidosAssertScriptSuccess("c(_Test(2),_Test(3))._squareTest()._cubicYolk();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{64, 729}));
	EidosAssertScriptSuccess("_Test(2)[F]._squareTest()._cubicYolk();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{}));
}

	






























































