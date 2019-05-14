//
//  eidos_test.cpp
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
//  Copyright (c) 2015-2019 Philipp Messer.  All rights reserved.
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
#include "eidos_global.h"
#include "eidos_rng.h"
#include "eidos_test_element.h"

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <limits>
#include <random>

#if ((defined(SLIMGUI) && (SLIMPROFILING == 1)) || defined(EIDOS_GUI))
// includes for the timing code in RunEidosTests(), which is normally #if 0
#include "sys/time.h"	// for gettimeofday()
#include <chrono>
#include <mach/mach_time.h>
#endif


// Helper functions for testing
void EidosAssertScriptSuccess(const std::string &p_script_string, EidosValue_SP p_correct_result);
void EidosAssertScriptRaise(const std::string &p_script_string, const int p_bad_position, const std::string &p_reason_snip);

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
		for (int value_index = 0; value_index < result->Count(); ++value_index)
		{
			if (CompareEidosValues(*result, value_index, *p_correct_result, value_index, nullptr) != 0)
			{
				std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : mismatched values (" << *result << "), expected (" << *p_correct_result << ")" << std::endl;
				
				return;
			}
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


// Test subfunction prototypes
static void _RunLiteralsIdentifiersAndTokenizationTests(void);
static void _RunSymbolsAndVariablesTests(void);
static void _RunParsingTests(void);
static void _RunFunctionDispatchTests(void);
static void _RunRuntimeErrorTests(void);
static void _RunVectorsAndSingletonsTests(void);
static void _RunOperatorPlusTests1(void);
static void _RunOperatorPlusTests2(void);
static void _RunOperatorMinusTests(void);
static void _RunOperatorMultTests(void);
static void _RunOperatorDivTests(void);
static void _RunOperatorModTests(void);
static void _RunOperatorSubsetTests(void);
static void _RunOperatorAssignTests(void);
static void _RunOperatorGtTests(void);
static void _RunOperatorLtTests(void);
static void _RunOperatorGtEqTests(void);
static void _RunOperatorLtEqTests(void);
static void _RunOperatorEqTests(void);
static void _RunOperatorNotEqTests(void);
static void _RunOperatorRangeTests(void);
static void _RunOperatorExpTests(void);
static void _RunOperatorLogicalAndTests(void);
static void _RunOperatorLogicalOrTests(void);
static void _RunOperatorLogicalNotTests(void);
static void _RunOperatorTernaryConditionalTests(void);
static void _RunKeywordIfTests(void);
static void _RunKeywordDoTests(void);
static void _RunKeywordWhileTests(void);
static void _RunKeywordForInTests(void);
static void _RunKeywordNextTests(void);
static void _RunKeywordBreakTests(void);
static void _RunKeywordReturnTests(void);
static void _RunFunctionMathTests_a_through_f(void);
static void _RunFunctionMathTests_g_through_r(void);
static void _RunFunctionMathTests_setUnionIntersection(void);
static void _RunFunctionMathTests_setDifferenceSymmetricDifference(void);
static void _RunFunctionMathTests_s_through_z(void);
static void _RunFunctionMatrixArrayTests(void);
static void _RunFunctionStatisticsTests(void);
static void _RunFunctionDistributionTests(void);
static void _RunFunctionVectorConstructionTests(void);
static void _RunFunctionValueInspectionManipulationTests_a_through_f(void);
static void _RunFunctionValueInspectionManipulationTests_g_through_l(void);
static void _RunFunctionValueInspectionManipulationTests_m_through_r(void);
static void _RunFunctionValueInspectionManipulationTests_s_through_z(void);
static void _RunFunctionValueTestingCoercionTests(void);
static void _RunFunctionFilesystemTests(void);
static void _RunColorManipulationTests(void);
static void _RunFunctionMiscTests_apply_sapply(void);
static void _RunFunctionMiscTests(void);
static void _RunMethodTests(void);
static void _RunCodeExampleTests(void);
static void _RunUserDefinedFunctionTests(void);
static void _RunVoidEidosValueTests(void);


int RunEidosTests(void)
{
	// Reset error counts
	gEidosTestSuccessCount = 0;
	gEidosTestFailureCount = 0;
	
#if (!EIDOS_HAS_OVERFLOW_BUILTINS)
	std::cout << "WARNING: This build of Eidos does not detect integer arithmetic overflows.  Compiling Eidos with GCC version 5.0 or later, or Clang version 3.9 or later, is required for this feature.  This means that integer addition, subtraction, or multiplication that overflows the 64-bit range of Eidos (" << INT64_MIN << " to " << INT64_MAX << ") will not be detected." << std::endl;
#endif
	
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
	_RunFunctionFilesystemTests();
	_RunColorManipulationTests();
	_RunFunctionMiscTests_apply_sapply();
	_RunFunctionMiscTests();
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
			double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
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
			
			double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
			
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
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		int64_t total = 0;
		
		for (int64_t iteration = 0; iteration < 1000000000; ++iteration)
			total += gsl_rng_uniform_int(EIDOS_GSL_RNG, 500);
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << std::endl << "gsl_rng_uniform_int(): time == " << (end_time - start_time) << ", total == " << total << std::endl;
	}
	
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		int64_t total = 0;
		
		for (int64_t iteration = 0; iteration < 1000000000; ++iteration)
			total += Eidos_rng_uniform_int(EIDOS_GSL_RNG, 500);
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "Eidos_rng_uniform_int(): time == " << (end_time - start_time) << ", total == " << total << std::endl;
	}
	
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		int64_t total = 0;
		init_genrand64(0);
		
		for (int64_t iteration = 0; iteration < 1000000000; ++iteration)
			total += Eidos_rng_uniform_int_MT64(500);
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "Eidos_rng_uniform_int_MT64(): time == " << (end_time - start_time) << ", total == " << total << std::endl;
	}
	
	{
		std::random_device rd;
		std::mt19937_64 e2(rd());
		std::uniform_int_distribution<long long int> dist(0, 499);
		int64_t total = 0;
		
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		for (int64_t iteration = 0; iteration < 1000000000; ++iteration)
			total += dist(e2);
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "std::mt19937_64: time == " << (end_time - start_time) << ", total == " << total << std::endl;
	}
#endif
	
#if 0
	// Speed tests of gsl_rng_uniform() versus Eidos_rng_uniform()
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		for (int64_t iteration = 0; iteration < 100000000; ++iteration)
			gsl_rng_uniform(EIDOS_GSL_RNG);
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << std::endl << "gsl_rng_uniform(): time == " << (end_time - start_time) << std::endl;
	}
	
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		for (int64_t iteration = 0; iteration < 100000000; ++iteration)
			Eidos_rng_uniform(EIDOS_GSL_RNG);
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "Eidos_rng_uniform(): time == " << (end_time - start_time) << std::endl;
	}
#endif
	
#if 0
	// Speed tests of gsl_rng_uniform_pos() versus Eidos_rng_uniform_pos()
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		for (int64_t iteration = 0; iteration < 100000000; ++iteration)
			gsl_rng_uniform_pos(EIDOS_GSL_RNG);
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << std::endl << "gsl_rng_uniform_pos(): time == " << (end_time - start_time) << std::endl;
	}
	
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		for (int64_t iteration = 0; iteration < 100000000; ++iteration)
			Eidos_rng_uniform_pos(EIDOS_GSL_RNG);
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
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
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
			total_time += clock();
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock(): time == " << (end_time - start_time) << ", total_time == " << (total_time / CLOCKS_PER_SEC) << std::endl;
	}
	
	// gettimeofday()
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		struct timeval timer;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			gettimeofday(&timer, NULL);
			
			total_time += ((int64_t)timer.tv_sec) * 1000000 + timer.tv_usec;
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to gettimeofday(): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000.0) << std::endl;
	}
	
	// clock_gettime(CLOCK_REALTIME, ...)
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		struct timespec timer;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			clock_gettime(CLOCK_REALTIME, &timer);
			
			total_time += ((int64_t)timer.tv_sec) * 1000000000 + timer.tv_nsec;
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime(CLOCK_REALTIME, ...): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime(CLOCK_MONOTONIC_RAW, ...)
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		struct timespec timer;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			clock_gettime(CLOCK_MONOTONIC_RAW, &timer);
			
			total_time += ((int64_t)timer.tv_sec) * 1000000000 + timer.tv_nsec;
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime(CLOCK_MONOTONIC_RAW, ...): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime(CLOCK_UPTIME_RAW, ...)
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		struct timespec timer;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			clock_gettime(CLOCK_UPTIME_RAW, &timer);
			
			total_time += ((int64_t)timer.tv_sec) * 1000000000 + timer.tv_nsec;
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime(CLOCK_UPTIME_RAW, ...): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime(CLOCK_PROCESS_CPUTIME_ID, ...)
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		struct timespec timer;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &timer);
			
			total_time += ((int64_t)timer.tv_sec) * 1000000000 + timer.tv_nsec;
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime(CLOCK_PROCESS_CPUTIME_ID, ...): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime(CLOCK_THREAD_CPUTIME_ID, ...)
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		struct timespec timer;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &timer);
			
			total_time += ((int64_t)timer.tv_sec) * 1000000000 + timer.tv_nsec;
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime(CLOCK_THREAD_CPUTIME_ID, ...): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// std::chrono::high_resolution_clock::now()
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		int64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			__attribute__((unused)) auto timer = std::chrono::high_resolution_clock::now();
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to std::chrono::high_resolution_clock::now(): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime_nsec_np(CLOCK_REALTIME)
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += clock_gettime_nsec_np(CLOCK_REALTIME);
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime_nsec_np(CLOCK_REALTIME): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime_nsec_np(CLOCK_UPTIME_RAW)
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime_nsec_np(CLOCK_UPTIME_RAW): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime_nsec_np(CLOCK_PROCESS_CPUTIME_ID)
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += clock_gettime_nsec_np(CLOCK_PROCESS_CPUTIME_ID);
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime_nsec_np(CLOCK_PROCESS_CPUTIME_ID): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// clock_gettime_nsec_np(CLOCK_THREAD_CPUTIME_ID)
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += clock_gettime_nsec_np(CLOCK_THREAD_CPUTIME_ID);
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to clock_gettime_nsec_np(CLOCK_THREAD_CPUTIME_ID): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
#if ((defined(SLIMGUI) && (SLIMPROFILING == 1)) || defined(EIDOS_GUI))
	
	// mach_absolute_time()
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += mach_absolute_time();
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to mach_absolute_time(): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// mach_continuous_time()
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += mach_continuous_time();
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
		std::cout << "10000000 calls to mach_continuous_time(): time == " << (end_time - start_time) << ", total_time == " << (total_time / 1000000000.0) << std::endl;
	}
	
	// Eidos_ProfileTime()
	{
		double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		uint64_t total_time = 0;
		
		for (int i = 0; i < 10000000; ++i)
		{
			total_time += Eidos_ProfileTime();
		}
		
		double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
		
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
	 defined Eidos_ProfileTime() as using mach_absolute_time() in eidos_global.h.
	 
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
				clock_t begin = clock();
				int64_t total = 0;
				
				for (int64_t i = 0; i < 20000000; i++)
					total += gsl_ran_binomial(EIDOS_GSL_RNG, p, n);
				
				double time_spent = static_cast<double>(clock() - begin) / CLOCKS_PER_SEC;
				std::cout << ((pref == 1) ? "***" : "   ");
				std::cout << "time for 20000000 calls to gsl_ran_binomial(): " << time_spent << " (total == " << total << ")" << std::endl;
			}
			if (poisson_allowed)
			{
				clock_t begin = clock();
				int64_t total = 0;
				
				for (int64_t i = 0; i < 20000000; i++)
					total += gsl_ran_poisson(EIDOS_GSL_RNG, p * n);
				
				double time_spent = static_cast<double>(clock() - begin) / CLOCKS_PER_SEC;
				std::cout << ((pref == 2) ? "***" : "   ");
				std::cout << "time for 20000000 calls to gsl_ran_poisson(): " << time_spent << " (total == " << total << ")" << std::endl;
			}
			if (gaussian_allowed)
			{
				clock_t begin = clock();
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
	EidosAssertScriptRaise("c(x=2);", 0, "named argument x in ellipsis argument section");
	EidosAssertScriptRaise("c(x=1, 2, 3);", 0, "named argument x in ellipsis argument section");
	EidosAssertScriptRaise("c(1, x=2, 3);", 0, "named argument x in ellipsis argument section");
	EidosAssertScriptRaise("c(1, 2, x=3);", 0, "named argument x in ellipsis argument section");
	
	EidosAssertScriptSuccess("doCall('abs', -10);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("doCall(functionName='abs', -10);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptRaise("doCall(x='abs', -10);", 0, "skipped over required argument");
	EidosAssertScriptRaise("doCall('abs', x=-10);", 0, "named argument x in ellipsis argument section");
	EidosAssertScriptRaise("doCall('abs', functionName=-10);", 0, "named argument functionName in ellipsis argument section");
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

	// ************************************************************************************
	//
	//	Operator tests
	//
	
	#pragma mark -
	#pragma mark Operators
	#pragma mark -
	
#pragma mark operator +
void _RunOperatorPlusTests1(void)
{
	// operator +
	EidosAssertScriptRaise("NULL+T;", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+0;", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+0.5;", 4, "combination of operand types");
	EidosAssertScriptSuccess("NULL+'foo';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("NULLfoo")));
	EidosAssertScriptRaise("NULL+_Test(7);", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+(0:2);", 4, "combination of operand types");
	EidosAssertScriptRaise("T+NULL;", 1, "combination of operand types");
	EidosAssertScriptRaise("0+NULL;", 1, "combination of operand types");
	EidosAssertScriptRaise("0.5+NULL;", 3, "combination of operand types");
	EidosAssertScriptSuccess("'foo'+NULL;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("fooNULL")));
	EidosAssertScriptRaise("_Test(7)+NULL;", 8, "combination of operand types");
	EidosAssertScriptRaise("(0:2)+NULL;", 5, "combination of operand types");
	EidosAssertScriptRaise("+NULL;", 0, "operand type NULL is not supported");
	EidosAssertScriptSuccess("1+1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("1+-1;", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("(0:2)+10;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 11, 12}));
	EidosAssertScriptSuccess("10+(0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 11, 12}));
	EidosAssertScriptSuccess("(15:13)+(0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{15, 15, 15}));
	EidosAssertScriptRaise("(15:12)+(0:2);", 7, "operator requires that either");
	EidosAssertScriptSuccess("1+1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2)));
	EidosAssertScriptSuccess("1.0+1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2)));
	EidosAssertScriptSuccess("1.0+-1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0)));
	EidosAssertScriptSuccess("(0:2.0)+10;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10, 11, 12}));
	EidosAssertScriptSuccess("10.0+(0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10, 11, 12}));
	EidosAssertScriptSuccess("10+(0.0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10, 11, 12}));
	EidosAssertScriptSuccess("(15.0:13)+(0:2.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{15, 15, 15}));
	EidosAssertScriptRaise("(15:12.0)+(0:2);", 9, "operator requires that either");
	EidosAssertScriptSuccess("'foo'+5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo5")));
	EidosAssertScriptSuccess("'foo'+5.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo5")));
	EidosAssertScriptSuccess("'foo'+5.1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo5.1")));
	EidosAssertScriptSuccess("5+'foo';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5foo")));
	EidosAssertScriptSuccess("5.0+'foo';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5foo")));
	EidosAssertScriptSuccess("5.1+'foo';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5.1foo")));
	EidosAssertScriptSuccess("'foo'+1:3;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo1", "foo2", "foo3"}));
	EidosAssertScriptSuccess("1:3+'foo';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"1foo", "2foo", "3foo"}));
	EidosAssertScriptSuccess("'foo'+'bar';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foobar")));
	EidosAssertScriptSuccess("'foo'+c('bar', 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foobar", "foobaz"}));
	EidosAssertScriptSuccess("c('bar', 'baz')+'foo';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"barfoo", "bazfoo"}));
	EidosAssertScriptSuccess("c('bar', 'baz')+c('foo', 'biz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"barfoo", "bazbiz"}));
	EidosAssertScriptSuccess("c('bar', 'baz')+T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"barT", "bazT"}));
	EidosAssertScriptSuccess("F+c('bar', 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"Fbar", "Fbaz"}));
	EidosAssertScriptRaise("T+F;", 1, "combination of operand types");
	EidosAssertScriptRaise("T+T;", 1, "combination of operand types");
	EidosAssertScriptRaise("F+F;", 1, "combination of operand types");
	EidosAssertScriptSuccess("+5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("+5.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5)));
	EidosAssertScriptRaise("+'foo';", 0, "is not supported by");
	EidosAssertScriptRaise("+T;", 0, "is not supported by");
	EidosAssertScriptSuccess("3+4+5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(12)));
	
	// operator +: raise on integer addition overflow for all code paths
	EidosAssertScriptSuccess("5e18;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5000000000000000000LL)));
	EidosAssertScriptRaise("1e19;", 0, "could not be represented");
#if EIDOS_HAS_OVERFLOW_BUILTINS
	EidosAssertScriptRaise("5e18 + 5e18;", 5, "overflow with the binary");
	EidosAssertScriptRaise("5e18 + c(0, 0, 5e18, 0);", 5, "overflow with the binary");
	EidosAssertScriptRaise("c(0, 0, 5e18, 0) + 5e18;", 17, "overflow with the binary");
	EidosAssertScriptRaise("c(0, 0, 5e18, 0) + c(0, 0, 5e18, 0);", 17, "overflow with the binary");
#endif
	
	// operator +: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	// this is the only place where we test the binary operators with matrices and arrays so comprehensively; the same machinery is used for all, so it should suffice
	EidosAssertScriptSuccess("identical(1 + integer(0), integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + 2, 3);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + 1:3, 2:4);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + matrix(2), matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + array(2,c(1,1,1)), array(3, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + matrix(1:3,nrow=1), matrix(2:4, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + matrix(1:3,ncol=1), matrix(2:4, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + matrix(1:6,ncol=2), matrix(2:7, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + array(1:3,c(3,1,1)), array(2:4, c(3,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + array(1:3,c(1,3,1)), array(2:4, c(1,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + array(1:3,c(1,1,3)), array(2:4, c(1,1,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + array(1:6,c(3,2,1)), array(2:7, c(3,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + array(1:6,c(3,1,2)), array(2:7, c(3,1,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + array(1:6,c(2,3,1)), array(2:7, c(2,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + array(1:6,c(1,3,2)), array(2:7, c(1,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + array(1:6,c(2,1,3)), array(2:7, c(2,1,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1 + array(1:6,c(1,2,3)), array(2:7, c(1,2,3)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptRaise("identical(1:3 + integer(0), integer(0));", 14, "requires that either");
	EidosAssertScriptSuccess("identical(1:3 + 2, 3:5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + 1:3, (1:3)*2);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + matrix(2), 3:5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + array(2,c(1,1,1)), 3:5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + matrix(1:3,nrow=1), matrix((1:3)*2, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + matrix(1:3,ncol=1), matrix((1:3)*2, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:6 + matrix(1:6,ncol=2), matrix((1:6)*2, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + array(1:3,c(3,1,1)), array((1:3)*2, c(3,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + array(1:3,c(1,3,1)), array((1:3)*2, c(1,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + array(1:3,c(1,1,3)), array((1:3)*2, c(1,1,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:6 + array(1:6,c(3,2,1)), array((1:6)*2, c(3,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:6 + array(1:6,c(3,1,2)), array((1:6)*2, c(3,1,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:6 + array(1:6,c(2,3,1)), array((1:6)*2, c(2,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:6 + array(1:6,c(1,3,2)), array((1:6)*2, c(1,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:6 + array(1:6,c(2,1,3)), array((1:6)*2, c(2,1,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:6 + array(1:6,c(1,2,3)), array((1:6)*2, c(1,2,3)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("identical(matrix(1) + integer(0), integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1) + 2, matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1) + 1:3, 2:4);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1) + matrix(2), matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1) + array(2,c(1,1,1)), array(3, c(1,1,1)));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1) + matrix(1:3,nrow=1), matrix(2:4, nrow=1));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1) + matrix(1:3,ncol=1), matrix(2:4, ncol=1));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1) + matrix(1:6,ncol=2), matrix(2:7, ncol=2));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1) + array(1:3,c(3,1,1)), array(2:4, c(3,1,1)));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1) + array(1:3,c(1,3,1)), array(2:4, c(1,3,1)));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1) + array(1:3,c(1,1,3)), array(2:4, c(1,1,3)));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1) + array(1:6,c(3,2,1)), array(2:7, c(3,2,1)));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1) + array(1:6,c(3,1,2)), array(2:7, c(3,1,2)));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1) + array(1:6,c(2,3,1)), array(2:7, c(2,3,1)));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1) + array(1:6,c(1,3,2)), array(2:7, c(1,3,2)));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1) + array(1:6,c(2,1,3)), array(2:7, c(2,1,3)));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1) + array(1:6,c(1,2,3)), array(2:7, c(1,2,3)));", 20, "non-conformable");
	
	EidosAssertScriptSuccess("identical(array(1,c(1,1,1)) + integer(0), integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1,c(1,1,1)) + 2, array(3, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1,c(1,1,1)) + 1:3, 2:4);", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + matrix(2), matrix(3));", 28, "non-conformable");
	EidosAssertScriptSuccess("identical(array(1,c(1,1,1)) + array(2,c(1,1,1)), array(3, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + matrix(1:3,nrow=1), matrix(2:4, nrow=1));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + matrix(1:3,ncol=1), matrix(2:4, ncol=1));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + matrix(1:6,ncol=2), matrix(2:7, ncol=2));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + array(1:3,c(3,1,1)), array(2:4, c(3,1,1)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + array(1:3,c(1,3,1)), array(2:4, c(1,3,1)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + array(1:3,c(1,1,3)), array(2:4, c(1,1,3)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + array(1:6,c(3,2,1)), array(2:7, c(3,2,1)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + array(1:6,c(3,1,2)), array(2:7, c(3,1,2)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + array(1:6,c(2,3,1)), array(2:7, c(2,3,1)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + array(1:6,c(1,3,2)), array(2:7, c(1,3,2)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + array(1:6,c(2,1,3)), array(2:7, c(2,1,3)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + array(1:6,c(1,2,3)), array(2:7, c(1,2,3)));", 28, "non-conformable");
	
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + integer(0), integer(0));", 29, "requires that either");
	EidosAssertScriptSuccess("identical(matrix(1:3,nrow=1) + 2, matrix(3:5, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1:3,nrow=1) + 1:3, matrix((1:3)*2, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + matrix(2), matrix(3));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + array(2,c(1,1,1)), array(3, c(1,1,1)));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(1:3,nrow=1) + matrix(1:3,nrow=1), matrix((1:3)*2, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + matrix(1:3,ncol=1), matrix(2:4, ncol=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + matrix(1:6,ncol=2), matrix(2:7, ncol=2));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + array(1:3,c(3,1,1)), array(2:4, c(3,1,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + array(1:3,c(1,3,1)), array(2:4, c(1,3,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + array(1:3,c(1,1,3)), array(2:4, c(1,1,3)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + array(1:6,c(3,2,1)), array(2:7, c(3,2,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + array(1:6,c(3,1,2)), array(2:7, c(3,1,2)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + array(1:6,c(2,3,1)), array(2:7, c(2,3,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + array(1:6,c(1,3,2)), array(2:7, c(1,3,2)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + array(1:6,c(2,1,3)), array(2:7, c(2,1,3)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + array(1:6,c(1,2,3)), array(2:7, c(1,2,3)));", 29, "non-conformable");
	
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + integer(0), integer(0));", 29, "requires that either");
	EidosAssertScriptSuccess("identical(matrix(1:3,ncol=1) + 2, matrix(3:5, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1:3,ncol=1) + 1:3, matrix((1:3)*2, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + matrix(2), matrix(3));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + array(2,c(1,1,1)), array(3, c(1,1,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + matrix(1:3,nrow=1), matrix(2:4, nrow=1));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(1:3,ncol=1) + matrix(1:3,ncol=1), matrix((1:3)*2, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + matrix(1:6,ncol=2), matrix(2:7, ncol=2));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + array(1:3,c(3,1,1)), array(2:4, c(3,1,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + array(1:3,c(1,3,1)), array(2:4, c(1,3,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + array(1:3,c(1,1,3)), array(2:4, c(1,1,3)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + array(1:6,c(3,2,1)), array(2:7, c(3,2,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + array(1:6,c(3,1,2)), array(2:7, c(3,1,2)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + array(1:6,c(2,3,1)), array(2:7, c(2,3,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + array(1:6,c(1,3,2)), array(2:7, c(1,3,2)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + array(1:6,c(2,1,3)), array(2:7, c(2,1,3)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + array(1:6,c(1,2,3)), array(2:7, c(1,2,3)));", 29, "non-conformable");
	
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + integer(0), integer(0));", 29, "requires that either");
	EidosAssertScriptSuccess("identical(matrix(1:6,ncol=2) + 2, matrix(3:8, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1:6,ncol=2) + 1:6, matrix((1:6)*2, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + matrix(2), matrix(3));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(2,c(1,1,1)), array(3, c(1,1,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + matrix(1:6,nrow=1), matrix(2:4, nrow=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + matrix(1:6,ncol=1), matrix(2:4, ncol=1));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(1:6,ncol=2) + matrix(1:6,ncol=2), matrix((1:6)*2, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(1:3,c(3,1,1)), array(2:4, c(3,1,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(1:3,c(1,3,1)), array(2:4, c(1,3,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(1:3,c(1,1,3)), array(2:4, c(1,1,3)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(1:6,c(3,2,1)), array(2:7, c(3,2,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(1:6,c(3,1,2)), array(2:7, c(3,1,2)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(1:6,c(2,3,1)), array(2:7, c(2,3,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(1:6,c(1,3,2)), array(2:7, c(1,3,2)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(1:6,c(2,1,3)), array(2:7, c(2,1,3)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(1:6,c(1,2,3)), array(2:7, c(1,2,3)));", 29, "non-conformable");
	
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + integer(0), integer(0));", 30, "requires that either");
	EidosAssertScriptSuccess("identical(array(1:6,c(3,2,1)) + 2, array(3:8, c(3,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6,c(3,2,1)) + 1:6, array((1:6)*2, c(3,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + matrix(2), matrix(3));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(2,c(1,1,1)), array(3, c(1,1,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + matrix(1:6,nrow=1), matrix(2:4, nrow=1));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + matrix(1:6,ncol=1), matrix(2:4, ncol=1));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + matrix(1:6,ncol=2), matrix((1:6)*2, ncol=2));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(1:3,c(3,1,1)), array(2:4, c(3,1,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(1:3,c(1,3,1)), array(2:4, c(1,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(1:3,c(1,1,3)), array(2:4, c(1,1,3)));", 30, "non-conformable");
	EidosAssertScriptSuccess("identical(array(1:6,c(3,2,1)) + array(1:6,c(3,2,1)), array((1:6)*2, c(3,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(1:6,c(3,1,2)), array(2:7, c(3,1,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(1:6,c(2,3,1)), array(2:7, c(2,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(1:6,c(1,3,2)), array(2:7, c(1,3,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(1:6,c(2,1,3)), array(2:7, c(2,1,3)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(1:6,c(1,2,3)), array(2:7, c(1,2,3)));", 30, "non-conformable");
}

void _RunOperatorPlusTests2(void)
{
	// operator +: identical to the previous tests, but with the order of the operands switched; should behave identically,
	// except that the error positions change, unfortunately.  Xcode search-replace to generate this from the above:
	// identical\(([A-Za-z0-9:(),=]+) \+ ([A-Za-z0-9:(),=]+), 
	// identical\($2 + $1, 
	EidosAssertScriptSuccess("identical(integer(0) + 1, integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(2 + 1, 3);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + 1, 2:4);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(2) + 1, matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(2,c(1,1,1)) + 1, array(3, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1:3,nrow=1) + 1, matrix(2:4, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1:3,ncol=1) + 1, matrix(2:4, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1:6,ncol=2) + 1, matrix(2:7, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:3,c(3,1,1)) + 1, array(2:4, c(3,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:3,c(1,3,1)) + 1, array(2:4, c(1,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:3,c(1,1,3)) + 1, array(2:4, c(1,1,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6,c(3,2,1)) + 1, array(2:7, c(3,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6,c(3,1,2)) + 1, array(2:7, c(3,1,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6,c(2,3,1)) + 1, array(2:7, c(2,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6,c(1,3,2)) + 1, array(2:7, c(1,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6,c(2,1,3)) + 1, array(2:7, c(2,1,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6,c(1,2,3)) + 1, array(2:7, c(1,2,3)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptRaise("identical(integer(0) + 1:3, integer(0));", 21, "requires that either");
	EidosAssertScriptSuccess("identical(2 + 1:3, 3:5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + 1:3, (1:3)*2);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(2) + 1:3, 3:5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(2,c(1,1,1)) + 1:3, 3:5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1:3,nrow=1) + 1:3, matrix((1:3)*2, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1:3,ncol=1) + 1:3, matrix((1:3)*2, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1:6,ncol=2) + 1:6, matrix((1:6)*2, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:3,c(3,1,1)) + 1:3, array((1:3)*2, c(3,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:3,c(1,3,1)) + 1:3, array((1:3)*2, c(1,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:3,c(1,1,3)) + 1:3, array((1:3)*2, c(1,1,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6,c(3,2,1)) + 1:6, array((1:6)*2, c(3,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6,c(3,1,2)) + 1:6, array((1:6)*2, c(3,1,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6,c(2,3,1)) + 1:6, array((1:6)*2, c(2,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6,c(1,3,2)) + 1:6, array((1:6)*2, c(1,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6,c(2,1,3)) + 1:6, array((1:6)*2, c(2,1,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6,c(1,2,3)) + 1:6, array((1:6)*2, c(1,2,3)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("identical(integer(0) + matrix(1), integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(2 + matrix(1), matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + matrix(1), 2:4);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(2) + matrix(1), matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(array(2,c(1,1,1)) + matrix(1), array(3, c(1,1,1)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + matrix(1), matrix(2:4, nrow=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + matrix(1), matrix(2:4, ncol=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + matrix(1), matrix(2:7, ncol=2));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(3,1,1)) + matrix(1), array(2:4, c(3,1,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,3,1)) + matrix(1), array(2:4, c(1,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,1,3)) + matrix(1), array(2:4, c(1,1,3)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + matrix(1), array(2:7, c(3,2,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,1,2)) + matrix(1), array(2:7, c(3,1,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,3,1)) + matrix(1), array(2:7, c(2,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,3,2)) + matrix(1), array(2:7, c(1,3,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,1,3)) + matrix(1), array(2:7, c(2,1,3)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,2,3)) + matrix(1), array(2:7, c(1,2,3)));", 30, "non-conformable");
	
	EidosAssertScriptSuccess("identical(integer(0) + array(1,c(1,1,1)), integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(2 + array(1,c(1,1,1)), array(3, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + array(1,c(1,1,1)), 2:4);", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(2) + array(1,c(1,1,1)), matrix(3));", 20, "non-conformable");
	EidosAssertScriptSuccess("identical(array(2,c(1,1,1)) + array(1,c(1,1,1)), array(3, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + array(1,c(1,1,1)), matrix(2:4, nrow=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + array(1,c(1,1,1)), matrix(2:4, ncol=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(1,c(1,1,1)), matrix(2:7, ncol=2));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(3,1,1)) + array(1,c(1,1,1)), array(2:4, c(3,1,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,3,1)) + array(1,c(1,1,1)), array(2:4, c(1,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,1,3)) + array(1,c(1,1,1)), array(2:4, c(1,1,3)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(1,c(1,1,1)), array(2:7, c(3,2,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,1,2)) + array(1,c(1,1,1)), array(2:7, c(3,1,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,3,1)) + array(1,c(1,1,1)), array(2:7, c(2,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,3,2)) + array(1,c(1,1,1)), array(2:7, c(1,3,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,1,3)) + array(1,c(1,1,1)), array(2:7, c(2,1,3)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,2,3)) + array(1,c(1,1,1)), array(2:7, c(1,2,3)));", 30, "non-conformable");
	
	EidosAssertScriptRaise("identical(integer(0) + matrix(1:3,nrow=1), integer(0));", 21, "requires that either");
	EidosAssertScriptSuccess("identical(2 + matrix(1:3,nrow=1), matrix(3:5, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + matrix(1:3,nrow=1), matrix((1:3)*2, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(2) + matrix(1:3,nrow=1), matrix(3));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(array(2,c(1,1,1)) + matrix(1:3,nrow=1), array(3, c(1,1,1)));", 28, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(1:3,nrow=1) + matrix(1:3,nrow=1), matrix((1:3)*2, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + matrix(1:3,nrow=1), matrix(2:4, ncol=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + matrix(1:3,nrow=1), matrix(2:7, ncol=2));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(3,1,1)) + matrix(1:3,nrow=1), array(2:4, c(3,1,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,3,1)) + matrix(1:3,nrow=1), array(2:4, c(1,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,1,3)) + matrix(1:3,nrow=1), array(2:4, c(1,1,3)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + matrix(1:3,nrow=1), array(2:7, c(3,2,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,1,2)) + matrix(1:3,nrow=1), array(2:7, c(3,1,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,3,1)) + matrix(1:3,nrow=1), array(2:7, c(2,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,3,2)) + matrix(1:3,nrow=1), array(2:7, c(1,3,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,1,3)) + matrix(1:3,nrow=1), array(2:7, c(2,1,3)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,2,3)) + matrix(1:3,nrow=1), array(2:7, c(1,2,3)));", 30, "non-conformable");
	
	EidosAssertScriptRaise("identical(integer(0) + matrix(1:3,ncol=1), integer(0));", 21, "requires that either");
	EidosAssertScriptSuccess("identical(2 + matrix(1:3,ncol=1), matrix(3:5, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 + matrix(1:3,ncol=1), matrix((1:3)*2, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(2) + matrix(1:3,ncol=1), matrix(3));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(array(2,c(1,1,1)) + matrix(1:3,ncol=1), array(3, c(1,1,1)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + matrix(1:3,ncol=1), matrix(2:4, nrow=1));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(1:3,ncol=1) + matrix(1:3,ncol=1), matrix((1:3)*2, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + matrix(1:3,ncol=1), matrix(2:7, ncol=2));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(3,1,1)) + matrix(1:3,ncol=1), array(2:4, c(3,1,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,3,1)) + matrix(1:3,ncol=1), array(2:4, c(1,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,1,3)) + matrix(1:3,ncol=1), array(2:4, c(1,1,3)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + matrix(1:3,ncol=1), array(2:7, c(3,2,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,1,2)) + matrix(1:3,ncol=1), array(2:7, c(3,1,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,3,1)) + matrix(1:3,ncol=1), array(2:7, c(2,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,3,2)) + matrix(1:3,ncol=1), array(2:7, c(1,3,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,1,3)) + matrix(1:3,ncol=1), array(2:7, c(2,1,3)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,2,3)) + matrix(1:3,ncol=1), array(2:7, c(1,2,3)));", 30, "non-conformable");
	
	EidosAssertScriptRaise("identical(integer(0) + matrix(1:6,ncol=2), integer(0));", 21, "requires that either");
	EidosAssertScriptSuccess("identical(2 + matrix(1:6,ncol=2), matrix(3:8, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:6 + matrix(1:6,ncol=2), matrix((1:6)*2, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(2) + matrix(1:6,ncol=2), matrix(3));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(array(2,c(1,1,1)) + matrix(1:6,ncol=2), array(3, c(1,1,1)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,nrow=1) + matrix(1:6,ncol=2), matrix(2:4, nrow=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=1) + matrix(1:6,ncol=2), matrix(2:4, ncol=1));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(1:6,ncol=2) + matrix(1:6,ncol=2), matrix((1:6)*2, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(array(1:3,c(3,1,1)) + matrix(1:6,ncol=2), array(2:4, c(3,1,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,3,1)) + matrix(1:6,ncol=2), array(2:4, c(1,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,1,3)) + matrix(1:6,ncol=2), array(2:4, c(1,1,3)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + matrix(1:6,ncol=2), array(2:7, c(3,2,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,1,2)) + matrix(1:6,ncol=2), array(2:7, c(3,1,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,3,1)) + matrix(1:6,ncol=2), array(2:7, c(2,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,3,2)) + matrix(1:6,ncol=2), array(2:7, c(1,3,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,1,3)) + matrix(1:6,ncol=2), array(2:7, c(2,1,3)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,2,3)) + matrix(1:6,ncol=2), array(2:7, c(1,2,3)));", 30, "non-conformable");
	
	EidosAssertScriptRaise("identical(integer(0) + array(1:6,c(3,2,1)), integer(0));", 21, "requires that either");
	EidosAssertScriptSuccess("identical(2 + array(1:6,c(3,2,1)), array(3:8, c(3,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:6 + array(1:6,c(3,2,1)), array((1:6)*2, c(3,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(2) + array(1:6,c(3,2,1)), matrix(3));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(array(2,c(1,1,1)) + array(1:6,c(3,2,1)), array(3, c(1,1,1)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,nrow=1) + array(1:6,c(3,2,1)), matrix(2:4, nrow=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=1) + array(1:6,c(3,2,1)), matrix(2:4, ncol=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(1:6,c(3,2,1)), matrix((1:6)*2, ncol=2));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(3,1,1)) + array(1:6,c(3,2,1)), array(2:4, c(3,1,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,3,1)) + array(1:6,c(3,2,1)), array(2:4, c(1,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,1,3)) + array(1:6,c(3,2,1)), array(2:4, c(1,1,3)));", 30, "non-conformable");
	EidosAssertScriptSuccess("identical(array(1:6,c(3,2,1)) + array(1:6,c(3,2,1)), array((1:6)*2, c(3,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(array(1:6,c(3,1,2)) + array(1:6,c(3,2,1)), array(2:7, c(3,1,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,3,1)) + array(1:6,c(3,2,1)), array(2:7, c(2,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,3,2)) + array(1:6,c(3,2,1)), array(2:7, c(1,3,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,1,3)) + array(1:6,c(3,2,1)), array(2:7, c(2,1,3)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,2,3)) + array(1:6,c(3,2,1)), array(2:7, c(1,2,3)));", 30, "non-conformable");
}

#pragma mark operator −
void _RunOperatorMinusTests(void)
{
	// operator -
	EidosAssertScriptRaise("NULL-T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL-0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL-0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL-'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL-_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL-(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T-NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0-NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5-NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'-NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)-NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)-NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("-NULL;", 0, "is not supported by");
	EidosAssertScriptSuccess("1-1;", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("1--1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("(0:2)-10;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-10, -9, -8}));
	EidosAssertScriptSuccess("10-(0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 9, 8}));
	EidosAssertScriptSuccess("(15:13)-(0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{15, 13, 11}));
	EidosAssertScriptRaise("(15:12)-(0:2);", 7, "operator requires that either");
	EidosAssertScriptSuccess("1-1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0)));
	EidosAssertScriptSuccess("1.0-1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0)));
	EidosAssertScriptSuccess("1.0--1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2)));
	EidosAssertScriptSuccess("(0:2.0)-10;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-10, -9, -8}));
	EidosAssertScriptSuccess("10.0-(0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10, 9, 8}));
	EidosAssertScriptSuccess("10-(0.0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10, 9, 8}));
	EidosAssertScriptSuccess("(15.0:13)-(0:2.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{15, 13, 11}));
	EidosAssertScriptRaise("(15:12.0)-(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("'foo'-1;", 5, "is not supported by");
	EidosAssertScriptRaise("T-F;", 1, "is not supported by");
	EidosAssertScriptRaise("T-T;", 1, "is not supported by");
	EidosAssertScriptRaise("F-F;", 1, "is not supported by");
	EidosAssertScriptSuccess("-5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-5)));
	EidosAssertScriptSuccess("-5.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-5)));
	EidosAssertScriptSuccess("-c(5, -6);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-5, 6}));
	EidosAssertScriptSuccess("-c(5.0, -6.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-5, 6}));
	EidosAssertScriptRaise("-'foo';", 0, "is not supported by");
	EidosAssertScriptRaise("-T;", 0, "is not supported by");
	EidosAssertScriptSuccess("3-4-5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-6)));
	
	// operator -: raise on integer subtraction overflow for all code paths
	EidosAssertScriptSuccess("9223372036854775807;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(INT64_MAX)));
	EidosAssertScriptSuccess("-9223372036854775807 - 1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(INT64_MIN)));
	EidosAssertScriptSuccess("-5e18;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-5000000000000000000LL)));
#if EIDOS_HAS_OVERFLOW_BUILTINS
	EidosAssertScriptRaise("-(-9223372036854775807 - 1);", 0, "overflow with the unary");
	EidosAssertScriptRaise("-c(-9223372036854775807 - 1, 10);", 0, "overflow with the unary");
	EidosAssertScriptRaise("-5e18 - 5e18;", 6, "overflow with the binary");
	EidosAssertScriptRaise("-5e18 - c(0, 0, 5e18, 0);", 6, "overflow with the binary");
	EidosAssertScriptRaise("c(0, 0, -5e18, 0) - 5e18;", 18, "overflow with the binary");
	EidosAssertScriptRaise("c(0, 0, -5e18, 0) - c(0, 0, 5e18, 0);", 18, "overflow with the binary");
#endif
	
	// operator -: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess("identical(-matrix(2), matrix(-2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(-matrix(1:3), matrix(-1:-3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(-array(2, c(1,1,1)), array(-2, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(-array(1:6, c(3,1,2)), array(-1:-6, c(3,1,2)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("identical(1-matrix(2), matrix(-1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1-matrix(1:3), matrix(0:-2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3-matrix(2), -1:1);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(4:6-matrix(1:3), matrix(c(3,3,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(5)-matrix(2), matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3)-matrix(2), matrix(3));", 21, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1)-matrix(1:3,ncol=1), matrix(3));", 28, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(7:9)-matrix(1:3), matrix(c(6,6,6)));", gStaticEidosValue_LogicalT);
}

#pragma mark operator *
void _RunOperatorMultTests(void)
{
    // operator *
	EidosAssertScriptRaise("NULL*T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL*0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL*0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL*'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL*_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL*(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T*NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0*NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5*NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'*NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)*NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)*NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("*NULL;", 0, "unexpected token");
    EidosAssertScriptSuccess("1*1;", gStaticEidosValue_Integer1);
    EidosAssertScriptSuccess("1*-1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("(0:2)*10;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 10, 20}));
	EidosAssertScriptSuccess("10*(0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 10, 20}));
	EidosAssertScriptSuccess("(15:13)*(0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 14, 26}));
	EidosAssertScriptRaise("(15:12)*(0:2);", 7, "operator requires that either");
    EidosAssertScriptSuccess("1*1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
    EidosAssertScriptSuccess("1.0*1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
    EidosAssertScriptSuccess("1.0*-1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-1)));
	EidosAssertScriptSuccess("(0:2.0)*10;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, 10, 20}));
	EidosAssertScriptSuccess("10.0*(0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, 10, 20}));
	EidosAssertScriptSuccess("(15.0:13)*(0:2.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, 14, 26}));
	EidosAssertScriptRaise("(15:12.0)*(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("'foo'*5;", 5, "is not supported by");
	EidosAssertScriptRaise("T*F;", 1, "is not supported by");
	EidosAssertScriptRaise("T*T;", 1, "is not supported by");
	EidosAssertScriptRaise("F*F;", 1, "is not supported by");
	EidosAssertScriptRaise("*5;", 0, "unexpected token");
	EidosAssertScriptRaise("*5.0;", 0, "unexpected token");
	EidosAssertScriptRaise("*'foo';", 0, "unexpected token");
	EidosAssertScriptRaise("*T;", 0, "unexpected token");
    EidosAssertScriptSuccess("3*4*5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(60)));
	
	// operator *: raise on integer multiplication overflow for all code paths
	EidosAssertScriptSuccess("5e18;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5000000000000000000LL)));
	EidosAssertScriptRaise("1e19;", 0, "could not be represented");
#if EIDOS_HAS_OVERFLOW_BUILTINS
	EidosAssertScriptRaise("5e18 * 2;", 5, "multiplication overflow");
	EidosAssertScriptRaise("5e18 * c(0, 0, 2, 0);", 5, "multiplication overflow");
	EidosAssertScriptRaise("c(0, 0, 2, 0) * 5e18;", 14, "multiplication overflow");
	EidosAssertScriptRaise("c(0, 0, 2, 0) * c(0, 0, 5e18, 0);", 14, "multiplication overflow");
	EidosAssertScriptRaise("c(0, 0, 5e18, 0) * c(0, 0, 2, 0);", 17, "multiplication overflow");
#endif
	
	// operator *: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess("identical(5 * matrix(2), matrix(10));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 * matrix(1:3), matrix(c(5,10,15)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 * matrix(2), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(4:6 * matrix(1:3), matrix(c(4,10,18)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(5) * matrix(2), matrix(10));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3) * matrix(2), matrix(c(2,4,6)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(4:6,nrow=1) * matrix(1:3,ncol=1), matrix(c(4,10,18)));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(6:8) * matrix(1:3), matrix(c(6,14,24)));", gStaticEidosValue_LogicalT);
}

#pragma mark operator /
void _RunOperatorDivTests(void)
{
    // operator /
	EidosAssertScriptRaise("NULL/T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL/0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL/0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL/'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL/_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL/(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T/NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0/NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5/NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'/NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)/NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)/NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("/NULL;", 0, "unexpected token");
    EidosAssertScriptSuccess("1/1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
    EidosAssertScriptSuccess("1/-1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-1)));
	EidosAssertScriptSuccess("(0:2)/10;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, 0.1, 0.2}));
	EidosAssertScriptRaise("(15:12)/(0:2);", 7, "operator requires that either");
    EidosAssertScriptSuccess("1/1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
    EidosAssertScriptSuccess("1.0/1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
    EidosAssertScriptSuccess("1.0/-1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-1)));
	EidosAssertScriptSuccess("(0:2.0)/10;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, 0.1, 0.2}));
	EidosAssertScriptSuccess("10.0/(0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::infinity(), 10, 5}));
	EidosAssertScriptSuccess("10/(0.0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::infinity(), 10, 5}));
	EidosAssertScriptSuccess("(15.0:13)/(0:2.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::infinity(), 14, 6.5}));
	EidosAssertScriptRaise("(15:12.0)/(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("'foo'/5;", 5, "is not supported by");
	EidosAssertScriptRaise("T/F;", 1, "is not supported by");
	EidosAssertScriptRaise("T/T;", 1, "is not supported by");
	EidosAssertScriptRaise("F/F;", 1, "is not supported by");
	EidosAssertScriptRaise("/5;", 0, "unexpected token");
	EidosAssertScriptRaise("/5.0;", 0, "unexpected token");
	EidosAssertScriptRaise("/'foo';", 0, "unexpected token");
	EidosAssertScriptRaise("/T;", 0, "unexpected token");
    EidosAssertScriptSuccess("3/4/5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0.15)));
	EidosAssertScriptSuccess("6/0;", gStaticEidosValue_FloatINF);
	
	// operator /: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess("identical(5 / matrix(2), matrix(2.5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(12 / matrix(1:3), matrix(c(12.0,6,4)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 / matrix(2), c(0.5,1,1.5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(4:6 / matrix(1:3), matrix(c(4,2.5,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(5) / matrix(2), matrix(2.5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3) / matrix(2), matrix(c(0.5,1,1.5)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(4:6,nrow=1) / matrix(1:3,ncol=1), matrix(c(4,2.5,2)));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(7:9) / matrix(1:3), matrix(c(7.0,4,3)));", gStaticEidosValue_LogicalT);
}

#pragma mark operator %
void _RunOperatorModTests(void)
{
    // operator %
	EidosAssertScriptRaise("NULL%T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL%0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL%0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL%'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL%_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL%(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T%NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0%NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5%NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'%NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)%NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)%NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("%NULL;", 0, "unexpected token");
    EidosAssertScriptSuccess("1%1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0)));
    EidosAssertScriptSuccess("1%-1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0)));
	EidosAssertScriptSuccess("(0:2)%10;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, 1, 2}));
	EidosAssertScriptRaise("(15:12)%(0:2);", 7, "operator requires that either");
    EidosAssertScriptSuccess("1%1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0)));
    EidosAssertScriptSuccess("1.0%1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0)));
    EidosAssertScriptSuccess("1.0%-1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0)));
	EidosAssertScriptSuccess("(0:2.0)%10;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, 1, 2}));
	EidosAssertScriptSuccess("10.0%(0:4);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN(), 0, 0, 1, 2}));
	EidosAssertScriptSuccess("10%(0.0:4);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN(), 0, 0, 1, 2}));
	EidosAssertScriptSuccess("(15.0:13)%(0:2.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN(), 0, 1}));
	EidosAssertScriptRaise("(15:12.0)%(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("'foo'%5;", 5, "is not supported by");
	EidosAssertScriptRaise("T%F;", 1, "is not supported by");
	EidosAssertScriptRaise("T%T;", 1, "is not supported by");
	EidosAssertScriptRaise("F%F;", 1, "is not supported by");
	EidosAssertScriptRaise("%5;", 0, "unexpected token");
	EidosAssertScriptRaise("%5.0;", 0, "unexpected token");
	EidosAssertScriptRaise("%'foo';", 0, "unexpected token");
	EidosAssertScriptRaise("%T;", 0, "unexpected token");
    EidosAssertScriptSuccess("3%4%5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3)));
	
	// operator %: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess("identical(5 % matrix(2), matrix(1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 % matrix(1:3), matrix(c(0.0,1,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(1:3 % matrix(2), c(1.0,0,1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(4:6 % matrix(1:3), matrix(c(0.0,1,0)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(5) % matrix(2), matrix(1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3) % matrix(2), matrix(c(1.0,0,1)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(4:6,nrow=1) % matrix(1:3,ncol=1), matrix(c(0.0,1,0)));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(6:8) % matrix(1:3), matrix(c(0.0,1,2)));", gStaticEidosValue_LogicalT);
}

#pragma mark operator []
void _RunOperatorSubsetTests(void)
{
	// operator []
	EidosAssertScriptSuccess("x = 1:5; x[NULL];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; NULL[x];", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x = 1:5; NULL[NULL];", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x = 1:5; x[];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[integer(0)];", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = 1:5; x[2];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("x = 1:5; x[2:3];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4}));
	EidosAssertScriptSuccess("x = 1:5; x[c(0, 2, 4)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[0:4];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[float(0)];", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = 1:5; x[2.0];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("x = 1:5; x[2.0:3];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4}));
	EidosAssertScriptSuccess("x = 1:5; x[c(0.0, 2, 4)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[0.0:4];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptRaise("x = 1:5; x[c(7,8)];", 10, "out-of-range index");
	EidosAssertScriptRaise("x = 1:5; x[logical(0)];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[T];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[c(T, T)];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[c(T, F, T)];", 10, "operator requires that the size()");
	EidosAssertScriptSuccess("x = 1:5; x[c(T, F, T, F, T)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[c(T, T, T, T, T)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[c(F, F, F, F, F)];", gStaticEidosValue_Integer_ZeroVec);

	EidosAssertScriptSuccess("x = c(T,T,F,T,F); x[c(T, F, T, F, T)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false}));
	EidosAssertScriptSuccess("x = 1.0:5; x[c(T, F, T, F, T)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 3.0, 5.0}));
	EidosAssertScriptSuccess("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(T, F, T, F, T)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "foobaz", "xyzzy"}));
	
	EidosAssertScriptSuccess("x = c(T,T,F,T,F); x[c(2,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptRaise("x = c(T,T,F,T,F); x[c(2,3,7)];", 19, "out-of-range index");
	EidosAssertScriptSuccess("x = c(T,T,F,T,F); x[c(2.0,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptRaise("x = c(T,T,F,T,F); x[c(2.0,3,7)];", 19, "out-of-range index");
	
	EidosAssertScriptSuccess("x = 1:5; x[c(2,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4}));
	EidosAssertScriptRaise("x = 1:5; x[c(2,3,7)];", 10, "out-of-range index");
	EidosAssertScriptSuccess("x = 1:5; x[c(2.0,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4}));
	EidosAssertScriptRaise("x = 1:5; x[c(2.0,3,7)];", 10, "out-of-range index");
	
	EidosAssertScriptSuccess("x = 1.0:5; x[c(2,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.0, 4.0}));
	EidosAssertScriptRaise("x = 1.0:5; x[c(2,3,7)];", 12, "out-of-range index");
	EidosAssertScriptSuccess("x = 1.0:5; x[c(2.0,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.0, 4.0}));
	EidosAssertScriptRaise("x = 1.0:5; x[c(2.0,3,7)];", 12, "out-of-range index");
	
	EidosAssertScriptSuccess("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(2,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foobaz", "baz"}));
	EidosAssertScriptRaise("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(2,3,7)];", 48, "out-of-range index");
	EidosAssertScriptSuccess("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(2.0,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foobaz", "baz"}));
	EidosAssertScriptRaise("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(2.0,3,7)];", 48, "out-of-range index");
	
	EidosAssertScriptSuccess("x = c(_Test(1), _Test(2), _Test(3), _Test(4), _Test(5)); x = x[c(2,3)]; x._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4}));
	EidosAssertScriptRaise("x = c(_Test(1), _Test(2), _Test(3), _Test(4), _Test(5)); x = x[c(2,3,7)]; x._yolk;", 62, "out-of-range index");
	EidosAssertScriptSuccess("x = c(_Test(1), _Test(2), _Test(3), _Test(4), _Test(5)); x = x[c(2.0,3)]; x._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4}));
	EidosAssertScriptRaise("x = c(_Test(1), _Test(2), _Test(3), _Test(4), _Test(5)); x = x[c(2.0,3,7)]; x._yolk;", 62, "out-of-range index");
	
	EidosAssertScriptSuccess("x = 5; x[T];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("x = 5; x[F];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{}));
	EidosAssertScriptRaise("x = 5; x[logical(0)];", 8, "must match the size()");
	EidosAssertScriptSuccess("x = 5; x[0];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptRaise("x = 5; x[1];", 8, "out of range");
	EidosAssertScriptRaise("x = 5; x[-1];", 8, "out of range");
	EidosAssertScriptSuccess("x = 5; x[integer(0)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{}));
	EidosAssertScriptSuccess("x = 5; x[0.0];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptRaise("x = 5; x[1.0];", 8, "out of range");
	EidosAssertScriptRaise("x = 5; x[-1.0];", 8, "out of range");
	EidosAssertScriptSuccess("x = 5; x[float(0)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{}));
	
	EidosAssertScriptRaise("x = 5:9; x[matrix(0)];", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(0:2)];", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(T)];", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(c(T,T,F,T,F))];", 10, "matrix or array index operand is not supported");
	
	// matrix/array subsets, without dropping
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[], 1:6);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,], matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[NULL,NULL], matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[0,], matrix(c(1,3,5), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[1,], matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[1,NULL], matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[0:1,], matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[NULL,], matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,0], matrix(1:2, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,1], matrix(3:4, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,2], matrix(5:6, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,0:1], matrix(1:4, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,1:2], matrix(3:6, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,c(0,2)], matrix(c(1,2,5,6), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[NULL,c(0,2)], matrix(c(1,2,5,6), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[0,1], matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[1,2], matrix(6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[0,c(T,F,T)], matrix(c(1,5), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[c(F,T),c(F,F,T)], matrix(6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[c(F,F),c(F,F,F)], integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[c(F,F),c(F,T,T)], integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[c(T,T),c(T,T,F)], matrix(1:4, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[c(0,0,1,0),], matrix(c(1,3,5,1,3,5,2,4,6,1,3,5), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[c(0,0,1,0),c(1,2,1)], matrix(c(3,5,3,3,5,3,4,6,4,3,5,3), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,c(1,2,1)], matrix(c(3,4,5,6,3,4), nrow=2));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T),c(T,T,F)];", 26, "match the corresponding dimension");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T,T,T),c(T,T,F)];", 26, "match the corresponding dimension");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T,T),c(T,T)];", 26, "match the corresponding dimension");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T,T),c(T,T,F,T)];", 26, "match the corresponding dimension");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[-1,];", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[2,];", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[,-1];", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[,3];", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0,0,0];", 26, "too many subset arguments");
	
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[], 1:12);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[,,], array(1:12, c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[NULL,NULL,NULL], array(1:12, c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[0,,], array(c(1,3,5,7,9,11), c(1,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[1,,], array(c(2,4,6,8,10,12), c(1,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[1,NULL,NULL], array(c(2,4,6,8,10,12), c(1,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[0:1,,], array(1:12, c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[NULL,,], array(1:12, c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[,0,], array(c(1,2,7,8), c(2,1,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[,1,], array(c(3,4,9,10), c(2,1,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[,2,], array(c(5,6,11,12), c(2,1,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[,c(0,2),], array(c(1,2,5,6,7,8,11,12), c(2,2,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[,,0], array(1:6, c(2,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[NULL,NULL,1], array(7:12, c(2,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[1,2,0], array(6, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[0,1,1], array(9, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[1,1:2,], array(c(4,6,10,12), c(1,2,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[0,c(T,F,T),], array(c(1,5,7,11), c(1,2,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(T,F),c(T,F,T),], array(c(1,5,7,11), c(1,2,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(T,F),c(T,F,T),c(F,T)], array(c(7,11), c(1,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(T,F),c(F,F,T),c(F,T)], array(11, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(F,F),c(F,F,F),c(F,T)], integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(F,F),c(T,F,T),c(F,T)], integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(0,0,1,0),,], array(c(1,1,2,1,3,3,4,3,5,5,6,5,7,7,8,7,9,9,10,9,11,11,12,11), c(4,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(0,0,1,0),c(2,1),0], array(c(5,5,6,5,3,3,4,3), c(4,2,1)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T), c(T,T,T), c(T,T)];", 28, "match the corresponding dimension");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T,T,T), c(T,T,T), c(T,T)];", 28, "match the corresponding dimension");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T,T), c(T,T), c(T,T)];", 28, "match the corresponding dimension");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T,T), c(T,T,T,T), c(T,T)];", 28, "match the corresponding dimension");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T,T), c(T,T,T), c(T)];", 28, "match the corresponding dimension");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T,T), c(T,T,T), c(T,T,T)];", 28, "match the corresponding dimension");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[-1, 0, 0];", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[2, 0, 0];", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0, -1, 0];", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0, 3, 0];", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0, 0, -1];", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0, 0, 2];", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0, 0];", 28, "too few subset arguments");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0, 0, 0, 0];", 28, "too many subset arguments");
}

#pragma mark operator = with []
void _RunOperatorAssignTests(void)
{
	// operator =
	EidosAssertScriptRaise("E = 7;", 2, "cannot be redefined because it is a constant");
	EidosAssertScriptRaise("E = E + 7;", 2, "cannot be redefined because it is a constant");
	
	// operator = (especially in conjunction with operator [])
	EidosAssertScriptSuccess("x = 5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("x = 1:5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 2, 10, 4, 10}));
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1][1:2] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 10, 4, 10}));
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 2, 10, 4, 10}));
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2][0:1] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 2, 10, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1] = 11:13; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{11, 2, 12, 4, 13}));
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1][1:2] = 11:12; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 11, 4, 12}));
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2] = 11:13; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{11, 2, 12, 4, 13}));
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2][0:1] = 11:12; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{11, 2, 12, 4, 5}));
	EidosAssertScriptRaise("x = 1:5; x[1:3*2 - 2][0:1] = 11:13; x;", 27, "assignment to a subscript requires");
	EidosAssertScriptRaise("x = 1:5; x[NULL] = NULL; x;", 17, "assignment to a subscript requires an rvalue that is");
	EidosAssertScriptSuccess("x = 1:5; x[NULL] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 10, 10, 10, 10}));	// assigns 10 to all indices, legal in Eidos 1.6 and later
	EidosAssertScriptRaise("x = 1:5; x[3] = NULL; x;", 14, "assignment to a subscript requires");
	EidosAssertScriptRaise("x = 1:5; x[integer(0)] = NULL; x;", 23, "type mismatch");
	EidosAssertScriptSuccess("x = 1:5; x[integer(0)] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5})); // assigns 10 to no indices, perfectly legal
	EidosAssertScriptRaise("x = 1:5; x[3] = integer(0); x;", 14, "assignment to a subscript requires");
	EidosAssertScriptSuccess("x = 1.0:5; x[3] = 1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 2, 3, 1, 5}));
	EidosAssertScriptSuccess("x = c('a', 'b', 'c'); x[1] = 1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"a", "1", "c"}));
	EidosAssertScriptRaise("x = 1:5; x[3] = 1.5; x;", 14, "type mismatch");
	EidosAssertScriptRaise("x = 1:5; x[3] = 'foo'; x;", 14, "type mismatch");
	EidosAssertScriptSuccess("x = 5; x[0] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x = 5.0; x[0] = 10.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(10)));
	EidosAssertScriptRaise("x = 5; x[0] = 10.0; x;", 12, "type mismatch");
	EidosAssertScriptSuccess("x = 5.0; x[0] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(10)));
	EidosAssertScriptSuccess("x = T; x[0] = F; x;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("x = 'foo'; x[0] = 'bar'; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptSuccess("x = 1:5; x[c(T,T,F,T,F)] = 7:9; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8, 3, 9, 5}));
	EidosAssertScriptRaise("x = 1:5; x[c(T,T,F,T,F,T)] = 7:9; x;", 10, "must match the size()");
	EidosAssertScriptSuccess("x = 1:5; x[c(2,3)] = c(9, 5); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 9, 5, 5}));
	EidosAssertScriptRaise("x = 1:5; x[c(7,8)] = 7; x;", 10, "out-of-range index");
	EidosAssertScriptSuccess("x = 1:5; x[c(2.0,3)] = c(9, 5); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 9, 5, 5}));
	EidosAssertScriptRaise("x = 1:5; x[c(7.0,8)] = 7; x;", 10, "out-of-range index");
	
	EidosAssertScriptRaise("x = 5:9; x[matrix(0)] = 3;", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(0:2)] = 3;", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(T)] = 3;", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(c(T,T,F,T,F))] = 3;", 10, "matrix or array index operand is not supported");
	EidosAssertScriptSuccess("x = 1; x[] = 2; identical(x, 2);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = 1; x[NULL] = 2; identical(x, 2);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = 1:5; x[] = 2; identical(x, rep(2,5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = 1:5; x[NULL] = 2; identical(x, rep(2,5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5); x[] = 3; identical(x, matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5); x[NULL] = 3; identical(x, matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5); x[0] = 3; identical(x, matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5:9); x[] = 3; identical(x, matrix(c(3,3,3,3,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5:9); x[NULL] = 3; identical(x, matrix(c(3,3,3,3,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5:9); x[0] = 3; identical(x, matrix(c(3,6,7,8,9)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5); x[T] = 3; identical(x, matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5:9); x[c(T,F,T,T,F)] = 3; identical(x, matrix(c(3,6,3,3,9)));", gStaticEidosValue_LogicalT);
	
	// operator = (especially in conjunction with matrix/array-style subsetting with operator [])
	EidosAssertScriptSuccess("NULL[logical(0)] = NULL;", gStaticEidosValueVOID);			// technically legal, as no assignment is done
	EidosAssertScriptRaise("NULL[logical(0),] = NULL;", 4, "too many subset arguments");
	EidosAssertScriptRaise("NULL[logical(0),logical(0)] = NULL;", 4, "too many subset arguments");
	EidosAssertScriptRaise("NULL[,] = NULL;", 4, "too many subset arguments");
	EidosAssertScriptSuccess("x = NULL; x[logical(0)] = NULL;", gStaticEidosValueVOID);	// technically legal, as no assignment is done
	EidosAssertScriptRaise("x = NULL; x[logical(0),] = NULL;", 11, "too many subset arguments");
	EidosAssertScriptRaise("x = NULL; x[logical(0),logical(0)] = NULL;", 11, "too many subset arguments");
	EidosAssertScriptRaise("x = NULL; x[,] = NULL;", 11, "too many subset arguments");
	EidosAssertScriptRaise("x = 1; x[,] = 2; x;", 8, "too many subset arguments");
	EidosAssertScriptRaise("x = 1; x[0,0] = 2; x;", 8, "too many subset arguments");
	EidosAssertScriptRaise("x = 1:5; x[,] = 2; x;", 10, "too many subset arguments");
	EidosAssertScriptRaise("x = 1:5; x[0,0] = 2; x;", 10, "too many subset arguments");
	EidosAssertScriptSuccess("x = matrix(1:5); x[,] = 2; identical(x, matrix(rep(2,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[NULL,NULL] = 2; identical(x, matrix(rep(2,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[0,0] = 2; identical(x, matrix(c(2,2,3,4,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[3,0] = 2; identical(x, matrix(c(1,2,3,2,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[1:3,0] = 7; identical(x, matrix(c(1,7,7,7,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[c(1,3),0] = 7; identical(x, matrix(c(1,7,3,7,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[c(1,3),0] = 6:7; identical(x, matrix(c(1,6,3,7,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[c(T,F,F,T,F),0] = 7; identical(x, matrix(c(7,2,3,7,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[c(T,F,F,T,F),0] = 6:7; identical(x, matrix(c(6,2,3,7,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("x = matrix(1:5); x[-1,0] = 2;", 18, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:5); x[5,0] = 2;", 18, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:5); x[0,-1] = 2;", 18, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:5); x[0,1] = 2;", 18, "out-of-range index");
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[1,1] = 2; identical(x, matrix(c(1,2,3,2,5,6), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[0:1,1] = 7; identical(x, matrix(c(1,2,7,7,5,6), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[1, c(T,F,T)] = 7; identical(x, matrix(c(1,7,3,4,5,7), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[0:1, c(T,F,T)] = 7; identical(x, matrix(c(7,7,3,4,7,7), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[c(T,T), c(T,F,T)] = 6:9; identical(x, matrix(c(6,7,3,4,8,9), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[-1,0] = 2;", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[2,0] = 2;", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0,-1] = 2;", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0,3] = 2;", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T,F,T),0] = 2;", 26, "size() of a logical");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[T,0] = 2;", 26, "size() of a logical");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0:4][,0] = 2;", 31, "chaining of matrix/array-style subsets");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0,1:2][,0] = 2;", 33, "chaining of matrix/array-style subsets");
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[0,1:2][1] = 2; identical(x, c(1,2,3,4,2,6));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[0,1:2][1] = 2; identical(x, matrix(c(1,2,3,4,2,6), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=matrix(c(x,y,x,y), nrow=2); z._yolk[,1]=6.5;", 61, "subset of a property");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=matrix(c(x,y,x,y), nrow=2); z[,1]._yolk[1]=6.5;", 68, "subset of a property");
	EidosAssertScriptSuccess("x=_Test(9); z=matrix(x); identical(z._yolk, matrix(9));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=_Test(9); z=array(x, c(1,1,1,1)); identical(z._yolk, array(9, c(1,1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=_Test(9); z=matrix(x); z[0]._yolk = 6; identical(z._yolk, matrix(6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=_Test(9); z=array(x, c(1,1,1,1)); z[0]._yolk = 6; identical(z._yolk, array(6, c(1,1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=_Test(9); z=matrix(x); z[0,0]._yolk = 6; identical(z._yolk, matrix(6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=_Test(9); z=array(x, c(1,1,1,1)); z[0,0,0,0]._yolk = 6; identical(z._yolk, array(6, c(1,1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=matrix(c(x,y,x,y), nrow=2); z[,1]._yolk=6; identical(z._yolk, matrix(c(6,6,6,6), nrow=2));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[,,] = 2; identical(x, array(rep(2,12), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[1,0,1] = -1; identical(x, array(c(1,2,3,4,5,6,7,-1,9,10,11,12), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[1,c(T,F,T),1] = 7; identical(x, array(c(1,2,3,4,5,6,7,7,9,10,11,7), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[1,c(T,F,T),1] = -1:-2; identical(x, array(c(1,2,3,4,5,6,7,-1,9,10,11,-2), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[0:1,c(T,F,T),1] = 15; identical(x, array(c(1,2,3,4,5,6,15,15,9,10,15,15), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[0:1,c(T,F,T),1] = 15:18; identical(x, array(c(1,2,3,4,5,6,15,16,9,10,17,18), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0:1,c(T,F,T),1] = 15:17; identical(x, array(c(1,2,3,4,5,6,15,16,9,10,17,18), c(2,3,2)));", 45, ".size() matching the .size");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0:1,c(T,F,T),1] = 15:19; identical(x, array(c(1,2,3,4,5,6,15,16,9,10,17,18), c(2,3,2)));", 45, ".size() matching the .size");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[-1,0,0] = 2;", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[2,0,0] = 2;", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0,-1,0] = 2;", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0,3,0] = 2;", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0,0,-1] = 2;", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0,0,2] = 2;", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T,F,T),0,0] = 2;", 28, "size() of a logical");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[T,0,0] = 2;", 28, "size() of a logical");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0:4][,0,] = 2;", 33, "chaining of matrix/array-style subsets");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0,1:2,][,0,] = 2;", 36, "chaining of matrix/array-style subsets");
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[0,1:2,][1:2] = 2; identical(x, array(c(1,2,3,4,2,6,7,8,2,10,11,12), c(2,3,2)));", gStaticEidosValue_LogicalT);
	
	// operator = (especially in conjunction with operator .)
	EidosAssertScriptSuccess("x=_Test(9); x._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(9)));
	EidosAssertScriptRaise("x=_Test(NULL);", 2, "cannot be type NULL");
	EidosAssertScriptRaise("x=_Test(9); x._yolk = NULL;", 20, "value cannot be type");
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{9, 7, 9, 7}));
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=2; z._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{9, 2, 9, 2}));
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=2; z._yolk;", 48, "not an lvalue");				// used to be legal, now a policy error
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[c(1,0)]._yolk=c(2, 5); z._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 2, 5, 2}));
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[c(1,0)]=c(3, 6); z._yolk;", 53, "not an lvalue");	// used to be legal, now a policy error
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=6.5; z._yolk;", 48, "value cannot be type");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=6.5; z._yolk;", 48, "not an lvalue");			// used to be a type error, now a policy error
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[2:3]._yolk=6.5; z._yolk;", 50, "value cannot be type");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[2:3]=6.5; z._yolk;", 50, "not an lvalue");			// used to be a type error, now a policy error
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[2]=6.5; z._yolk;", 42, "type mismatch");
	EidosAssertScriptRaise("x = 1:5; x.foo[5] = 7;", 10, "operand type integer is not supported");
	
	// operator = (with compound-operator optimizations)
	EidosAssertScriptSuccess("x = 5; x = x + 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(8)));
	EidosAssertScriptSuccess("x = 5:6; x = x + 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{8, 9}));
	EidosAssertScriptSuccess("x = 5:6; x = x + 3:4; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{8, 10}));
	EidosAssertScriptSuccess("x = 5; x = x + 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(8.5)));
	EidosAssertScriptSuccess("x = 5:6; x = x + 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{8.5, 9.5}));
	EidosAssertScriptSuccess("x = 5:6; x = x + 3.5:4.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{8.5, 10.5}));
	EidosAssertScriptRaise("x = 5:7; x = x + 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x + 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess("x = 5.5; x = x + 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(9)));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x + 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{9, 10}));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x + 3.5:4.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{9, 11}));
	EidosAssertScriptSuccess("x = 5.5; x = x + 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(8.5)));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x + 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{8.5, 9.5}));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x + 3:4; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{8.5, 10.5}));
	EidosAssertScriptRaise("x = 5.5:7.5; x = x + 3.5:4.5; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.5:6.5; x = x + 3.5:5.5; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess("x = 5; x = x - 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("x = 5:6; x = x - 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("x = 5:6; x = x - 3:4; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 2}));
	EidosAssertScriptSuccess("x = 5; x = x - 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1.5)));
	EidosAssertScriptSuccess("x = 5:6; x = x - 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.5, 2.5}));
	EidosAssertScriptSuccess("x = 5:6; x = x - 3.5:4.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.5, 1.5}));
	EidosAssertScriptRaise("x = 5:7; x = x - 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x - 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess("x = 5.5; x = x - 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2)));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x - 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2, 3}));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x - 3.5:4.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2, 2}));
	EidosAssertScriptSuccess("x = 5.5; x = x - 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.5)));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x - 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 3.5}));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x - 3:4; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 2.5}));
	EidosAssertScriptRaise("x = 5.5:7.5; x = x - 3.5:4.5; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.5:6.5; x = x - 3.5:5.5; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess("x = 5; x = x / 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.5)));
	EidosAssertScriptSuccess("x = 5:6; x = x / 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 3.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x / c(2,4); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 1.5}));
	EidosAssertScriptSuccess("x = 5; x = x / 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.5)));
	EidosAssertScriptSuccess("x = 5:6; x = x / 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 3.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x / c(2.0,4.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 1.5}));
	EidosAssertScriptRaise("x = 5:7; x = x / 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x / 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess("x = 5.0; x = x / 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.5)));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x / 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 3.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x / c(2.0,4.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 1.5}));
	EidosAssertScriptSuccess("x = 5.0; x = x / 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.5)));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x / 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 3.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x / c(2,4); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 1.5}));
	EidosAssertScriptRaise("x = 5.0:7.0; x = x / 3.0:4.0; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.0:6.0; x = x / 3.0:5.0; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess("x = 5; x = x % 2; x;", gStaticEidosValue_Float1);
	EidosAssertScriptSuccess("x = 5:6; x = x % 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 0.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x % c(2,4); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 2.0}));
	EidosAssertScriptSuccess("x = 5; x = x % 2.0; x;", gStaticEidosValue_Float1);
	EidosAssertScriptSuccess("x = 5:6; x = x % 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 0.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x % c(2.0,4.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 2.0}));
	EidosAssertScriptRaise("x = 5:7; x = x % 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x % 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess("x = 5.0; x = x % 2.0; x;", gStaticEidosValue_Float1);
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x % 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 0.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x % c(2.0,4.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 2.0}));
	EidosAssertScriptSuccess("x = 5.0; x = x % 2; x;", gStaticEidosValue_Float1);
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x % 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 0.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x % c(2,4); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 2.0}));
	EidosAssertScriptRaise("x = 5.0:7.0; x = x % 3.0:4.0; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.0:6.0; x = x % 3.0:5.0; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess("x = 5; x = x * 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x = 5:6; x = x * 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 12}));
	EidosAssertScriptSuccess("x = 5:6; x = x * c(2,4); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 24}));
	EidosAssertScriptSuccess("x = 5; x = x * 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(10.0)));
	EidosAssertScriptSuccess("x = 5:6; x = x * 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.0, 12.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x * c(2.0,4.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.0, 24.0}));
	EidosAssertScriptRaise("x = 5:7; x = x * 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x * 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess("x = 5.0; x = x * 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(10.0)));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x * 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.0, 12.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x * c(2.0,4.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.0, 24.0}));
	EidosAssertScriptSuccess("x = 5.0; x = x * 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(10.0)));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x * 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.0, 12.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x * c(2,4); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.0, 24.0}));
	EidosAssertScriptRaise("x = 5.0:7.0; x = x * 3.0:4.0; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.0:6.0; x = x * 3.0:5.0; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess("x = 5; x = x ^ 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(25.0)));
	EidosAssertScriptSuccess("x = 5:6; x = x ^ 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 36.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x ^ c(2,3); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 216.0}));
	EidosAssertScriptSuccess("x = 5; x = x ^ 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(25.0)));
	EidosAssertScriptSuccess("x = 5:6; x = x ^ 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 36.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x ^ c(2.0,3.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 216.0}));
	EidosAssertScriptRaise("x = 5:7; x = x ^ (3:4); x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x ^ (3:5); x;", 15, "operator requires that either");
	EidosAssertScriptSuccess("x = 5.0; x = x ^ 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(25.0)));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x ^ 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 36.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x ^ c(2.0,3.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 216.0}));
	EidosAssertScriptSuccess("x = 5.0; x = x ^ 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(25.0)));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x ^ 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 36.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x ^ c(2,3); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 216.0}));
	EidosAssertScriptRaise("x = 5.0:7.0; x = x ^ (3.0:4.0); x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.0:6.0; x = x ^ (3.0:5.0); x;", 19, "operator requires that either");
	
#if EIDOS_HAS_OVERFLOW_BUILTINS
	EidosAssertScriptRaise("x = 5e18; x = x + 5e18;", 16, "overflow with the binary");
	EidosAssertScriptRaise("x = c(5e18, 0); x = x + 5e18;", 22, "overflow with the binary");
	EidosAssertScriptRaise("x = -5e18; x = x - 5e18;", 17, "overflow with the binary");
	EidosAssertScriptRaise("x = c(-5e18, 0); x = x - 5e18;", 23, "overflow with the binary");
	EidosAssertScriptRaise("x = 5e18; x = x * 2;", 16, "multiplication overflow");
	EidosAssertScriptRaise("x = c(5e18, 0); x = x * 2;", 22, "multiplication overflow");
#endif
}

#pragma mark operator >
void _RunOperatorGtTests(void)
{
	// operator >
	EidosAssertScriptRaise("NULL>T;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>0;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>0.5;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>'foo';", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>_Test(7);", 4, "cannot be used with type");
	EidosAssertScriptRaise("NULL>(0:2);", 4, "testing NULL with");
	EidosAssertScriptRaise("T>NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0>NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0.5>NULL;", 3, "testing NULL with");
	EidosAssertScriptRaise("'foo'>NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("_Test(7)>NULL;", 8, "cannot be used with type");
	EidosAssertScriptRaise("(0:2)>NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise(">NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T > F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T > T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F > T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F > F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T > 0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T > 1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F > 0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F > 1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T > -5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-5 > T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T > 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 > T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T > -5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-5.0 > T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T > 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 > T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T > 'FOO';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'FOO' > T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T > 'XYZZY';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'XYZZY' > T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 > -10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10 > 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 > -10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10 > 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 > -10.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10.0 > 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo' > 'bar';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'bar' > 'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("120 > '10';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("10 > '120';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("120 > '15';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("15 > '120';", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("_Test(9) > 5;", 9, "cannot be used with type");
	EidosAssertScriptRaise("5 > _Test(9);", 2, "cannot be used with type");
	EidosAssertScriptSuccess("5 > 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10.0 > -10.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 > 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 > 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 > '5';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'5' > 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo' > 'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("_Test(9) > _Test(9);", 9, "cannot be used with type");
	
	EidosAssertScriptSuccess("T > c(T, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("5 > c(5, 6);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	EidosAssertScriptSuccess("5.0 > c(5.0, 6.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	EidosAssertScriptSuccess("'foo' > c('foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	
	EidosAssertScriptSuccess("c(T, F) > T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	EidosAssertScriptSuccess("c(5, 6) > 5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("c(5.0, 6.0) > 5.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("c('foo', 'bar') > 'foo';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	
	EidosAssertScriptSuccess("c(T, F) > c(T, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	EidosAssertScriptSuccess("c(5, 6) > c(5, 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	EidosAssertScriptSuccess("c(5.0, 6.0) > c(5.0, 8.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	EidosAssertScriptSuccess("c('foo', 'bar') > c('foo', 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	
	EidosAssertScriptRaise("c(5,6) > c(5,6,7);", 7, "operator requires that either");
	
	// operator >: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess("identical(4 > 5, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 > 5, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(6 > 5, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(4 > matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 > matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(6 > matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(2 > matrix(1:3), matrix(c(T,F,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((1:3) > matrix(2), c(F,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((1:3) > matrix(3:1), matrix(c(F,F,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(4) > matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(5) > matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(6) > matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3) > matrix(2), matrix(c(F,F,T)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) > matrix(3:1,ncol=1), matrix(c(F,F,T)));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(1:3) > matrix(3:1), matrix(c(F,F,T)));", gStaticEidosValue_LogicalT);
}

#pragma mark operator <
void _RunOperatorLtTests(void)
{
	// operator <
	EidosAssertScriptRaise("NULL<T;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<0;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<0.5;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<'foo';", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<_Test(7);", 4, "cannot be used with type");
	EidosAssertScriptRaise("NULL<(0:2);", 4, "testing NULL with");
	EidosAssertScriptRaise("T<NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0<NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0.5<NULL;", 3, "testing NULL with");
	EidosAssertScriptRaise("'foo'<NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("_Test(7)<NULL;", 8, "cannot be used with type");
	EidosAssertScriptRaise("(0:2)<NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("<NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T < F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T < T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F < T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F < F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T < 0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T < 1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F < 0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F < 1;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T < -5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-5 < T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T < 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 < T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T < -5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-5.0 < T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T < 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 < T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T < 'FOO';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'FOO' < T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T < 'XYZZY';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'XYZZY' < T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 < -10;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10 < 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 < -10;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10 < 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 < -10.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10.0 < 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo' < 'bar';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'bar' < 'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("120 < '10';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("10 < '120';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("120 < '15';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("15 < '120';", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("_Test(9) < 5;", 9, "cannot be used with type");
	EidosAssertScriptRaise("5 < _Test(9);", 2, "cannot be used with type");
	EidosAssertScriptSuccess("5 < 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10.0 < -10.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 < 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 < 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 < '5';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'5' < 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo' < 'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("_Test(9) < _Test(9);", 9, "cannot be used with type");
	
	EidosAssertScriptSuccess("T < c(T, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	EidosAssertScriptSuccess("5 < c(5, 6);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("5.0 < c(5.0, 6.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("'foo' < c('foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	
	EidosAssertScriptSuccess("c(T, F) < T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("c(5, 6) < 5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	EidosAssertScriptSuccess("c(5.0, 6.0) < 5.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	EidosAssertScriptSuccess("c('foo', 'bar') < 'foo';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	
	EidosAssertScriptSuccess("c(T, F) < c(T, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("c(5, 6) < c(5, 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("c(5.0, 6.0) < c(5.0, 8.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("c('foo', 'bar') < c('foo', 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	
	EidosAssertScriptRaise("c(5,6) < c(5,6,7);", 7, "operator requires that either");
	
	// operator <: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess("identical(4 < 5, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 < 5, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(6 < 5, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(4 < matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 < matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(6 < matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(2 < matrix(1:3), matrix(c(F,F,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((1:3) < matrix(2), c(T,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((1:3) < matrix(3:1), matrix(c(T,F,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(4) < matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(5) < matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(6) < matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3) < matrix(2), matrix(c(T,F,F)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) < matrix(3:1,ncol=1), matrix(c(T,F,F)));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(1:3) < matrix(3:1), matrix(c(T,F,F)));", gStaticEidosValue_LogicalT);
}

#pragma mark operator >=
void _RunOperatorGtEqTests(void)
{
	// operator >=
	EidosAssertScriptRaise("NULL>=T;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>=0;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>=0.5;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>='foo';", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL>=_Test(7);", 4, "cannot be used with type");
	EidosAssertScriptRaise("NULL>=(0:2);", 4, "testing NULL with");
	EidosAssertScriptRaise("T>=NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0>=NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0.5>=NULL;", 3, "testing NULL with");
	EidosAssertScriptRaise("'foo'>=NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("_Test(7)>=NULL;", 8, "cannot be used with type");
	EidosAssertScriptRaise("(0:2)>=NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise(">=NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T >= F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T >= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F >= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F >= F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T >= 0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T >= 1;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F >= 0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F >= 1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T >= -5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-5 >= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T >= 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 >= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T >= -5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-5.0 >= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T >= 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 >= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T >= 'FOO';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'FOO' >= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T >= 'XYZZY';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'XYZZY' >= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 >= -10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10 >= 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 >= -10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10 >= 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 >= -10.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10.0 >= 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo' >= 'bar';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'bar' >= 'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("120 >= '10';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("10 >= '120';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("120 >= '15';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("15 >= '120';", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("_Test(9) >= 5;", 9, "cannot be used with type");
	EidosAssertScriptRaise("5 >= _Test(9);", 2, "cannot be used with type");
	EidosAssertScriptSuccess("5 >= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10.0 >= -10.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 >= 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 >= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 >= '5';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'5' >= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo' >= 'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("_Test(9) >= _Test(9);", 9, "cannot be used with type");
	
	EidosAssertScriptSuccess("T >= c(T, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("5 >= c(5, 6);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("5.0 >= c(5.0, 6.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("'foo' >= c('foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	
	EidosAssertScriptSuccess("c(T, F) >= T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("c(5, 6) >= 5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("c(5.0, 6.0) >= 5.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("c('foo', 'bar') >= 'foo';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	
	EidosAssertScriptSuccess("c(T, F) >= c(T, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("c(5, 6) >= c(5, 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("c(5.0, 6.0) >= c(5.0, 8.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("c('foo', 'bar') >= c('foo', 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	
	EidosAssertScriptRaise("c(5,6) >= c(5,6,7);", 7, "operator requires that either");
	
	// operator >=: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess("identical(4 >= 5, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 >= 5, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(6 >= 5, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(4 >= matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 >= matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(6 >= matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(2 >= matrix(1:3), matrix(c(T,T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((1:3) >= matrix(2), c(F,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((1:3) >= matrix(3:1), matrix(c(F,T,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(4) >= matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(5) >= matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(6) >= matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3) >= matrix(2), matrix(c(F,T,T)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) >= matrix(3:1,ncol=1), matrix(c(F,T,T)));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(1:3) >= matrix(3:1), matrix(c(F,T,T)));", gStaticEidosValue_LogicalT);
}

#pragma mark operator <=
void _RunOperatorLtEqTests(void)
{
	// operator <=
	EidosAssertScriptRaise("NULL<=T;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<=0;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<=0.5;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<='foo';", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL<=_Test(7);", 4, "cannot be used with type");
	EidosAssertScriptRaise("NULL<=(0:2);", 4, "testing NULL with");
	EidosAssertScriptRaise("T<=NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0<=NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0.5<=NULL;", 3, "testing NULL with");
	EidosAssertScriptRaise("'foo'<=NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("_Test(7)<=NULL;", 8, "cannot be used with type");
	EidosAssertScriptRaise("(0:2)<=NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("<=NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T <= F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T <= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F <= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F <= F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T <= 0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T <= 1;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F <= 0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F <= 1;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T <= -5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-5 <= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T <= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 <= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T <= -5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-5.0 <= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T <= 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 <= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T <= 'FOO';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'FOO' <= T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T <= 'XYZZY';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'XYZZY' <= T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 <= -10;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10 <= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 <= -10;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10 <= 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 <= -10.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10.0 <= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo' <= 'bar';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'bar' <= 'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("120 <= '10';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("10 <= '120';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("120 <= '15';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("15 <= '120';", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("_Test(9) <= 5;", 9, "cannot be used with type");
	EidosAssertScriptRaise("5 <= _Test(9);", 2, "cannot be used with type");
	EidosAssertScriptSuccess("5 <= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10.0 <= -10.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 <= 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 <= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 <= '5';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'5' <= 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo' <= 'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("_Test(9) <= _Test(9);", 9, "cannot be used with type");
	
	EidosAssertScriptSuccess("T <= c(T, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("5 <= c(5, 6);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("5.0 <= c(5.0, 6.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("'foo' <= c('foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	
	EidosAssertScriptSuccess("c(T, F) <= T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("c(5, 6) <= 5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("c(5.0, 6.0) <= 5.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("c('foo', 'bar') <= 'foo';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	
	EidosAssertScriptSuccess("c(T, F) <= c(T, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("c(5, 6) <= c(5, 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("c(5.0, 6.0) <= c(5.0, 8.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("c('foo', 'bar') <= c('foo', 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	
	EidosAssertScriptRaise("c(5,6) <= c(5,6,7);", 7, "operator requires that either");
	
	// operator <=: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess("identical(4 <= 5, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 <= 5, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(6 <= 5, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(4 <= matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 <= matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(6 <= matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(2 <= matrix(1:3), matrix(c(F,T,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((1:3) <= matrix(2), c(T,T,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((1:3) <= matrix(3:1), matrix(c(T,T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(4) <= matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(5) <= matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(6) <= matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3) <= matrix(2), matrix(c(T,T,F)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) <= matrix(3:1,ncol=1), matrix(c(T,T,F)));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(1:3) <= matrix(3:1), matrix(c(T,T,F)));", gStaticEidosValue_LogicalT);
}

#pragma mark operator ==
void _RunOperatorEqTests(void)
{
	// operator ==
	EidosAssertScriptRaise("NULL==T;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL==0;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL==0.5;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL=='foo';", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL==_Test(7);", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL==(0:2);", 4, "testing NULL with");
	EidosAssertScriptRaise("T==NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0==NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0.5==NULL;", 3, "testing NULL with");
	EidosAssertScriptRaise("'foo'==NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("_Test(7)==NULL;", 8, "testing NULL with");
	EidosAssertScriptRaise("(0:2)==NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("==NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T == F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F == F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T == 0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == 1;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F == 0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F == 1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == -5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-5 == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == -5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-5.0 == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == 'FOO';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'FOO' == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T == 'XYZZY';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'XYZZY' == T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 == -10;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10 == 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 == -10;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10 == 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 == -10.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10.0 == 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo' == 'bar';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'bar' == 'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("120 == '10';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("10 == '120';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("120 == '15';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("15 == '120';", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("_Test(9) == 5;", 9, "cannot be converted to");
	EidosAssertScriptRaise("5 == _Test(9);", 2, "cannot be converted to");
	EidosAssertScriptSuccess("5 == 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10.0 == -10.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 == 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 == 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 == '5';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'5' == 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo' == 'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("_Test(9) == _Test(9);", gStaticEidosValue_LogicalF);	// not the same object
	
	EidosAssertScriptSuccess("T == c(T, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("5 == c(5, 6);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("5.0 == c(5.0, 6.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("'foo' == c('foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("x = _Test(9); x == c(x, _Test(9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	
	EidosAssertScriptSuccess("c(T, F) == T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("c(5, 6) == 5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("c(5.0, 6.0) == 5.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("c('foo', 'bar') == 'foo';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("x = _Test(9); c(x, _Test(9)) == x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	
	EidosAssertScriptSuccess("c(T, F) == c(T, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("c(5, 6) == c(5, 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("c(5.0, 6.0) == c(5.0, 8.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("c('foo', 'bar') == c('foo', 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("x = _Test(9); c(x, _Test(9)) == c(x, x);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	
	EidosAssertScriptRaise("c(5,6) == c(5,6,7);", 7, "operator requires that either");
	
	// operator ==: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess("identical(5 == 5, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 == matrix(2), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 == matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(2 == matrix(1:3), matrix(c(F,T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((1:3) == matrix(2), c(F,T,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((1:3) == matrix(3:1), matrix(c(F,T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(5) == matrix(2), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(5) == matrix(5), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3) == matrix(2), matrix(c(1.0,4,9)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(2:4,nrow=1) == matrix(1:3,ncol=1), matrix(c(2.0,9,64)));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(1:3) == matrix(3:1), matrix(c(F,T,F)));", gStaticEidosValue_LogicalT);
}

#pragma mark operator !=
void _RunOperatorNotEqTests(void)
{
	// operator !=
	EidosAssertScriptRaise("NULL!=T;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL!=0;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL!=0.5;", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL!='foo';", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL!=_Test(7);", 4, "testing NULL with");
	EidosAssertScriptRaise("NULL!=(0:2);", 4, "testing NULL with");
	EidosAssertScriptRaise("T!=NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0!=NULL;", 1, "testing NULL with");
	EidosAssertScriptRaise("0.5!=NULL;", 3, "testing NULL with");
	EidosAssertScriptRaise("'foo'!=NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("_Test(7)!=NULL;", 8, "testing NULL with");
	EidosAssertScriptRaise("(0:2)!=NULL;", 5, "testing NULL with");
	EidosAssertScriptRaise("!=NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T != F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F != F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T != 0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != 1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F != 0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F != 1;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != -5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-5 != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != -5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-5.0 != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != 'FOO';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'FOO' != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T != 'XYZZY';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'XYZZY' != T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 != -10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10 != 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 != -10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10 != 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5 != -10.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("-10.0 != 5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo' != 'bar';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'bar' != 'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("120 != '10';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("10 != '120';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("120 != '15';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("15 != '120';", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("_Test(9) != 5;", 9, "cannot be converted to");
	EidosAssertScriptRaise("5 != _Test(9);", 2, "cannot be converted to");
	EidosAssertScriptSuccess("5 != 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("-10.0 != -10.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 != 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 != 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5 != '5';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'5' != 5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo' != 'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("_Test(9) != _Test(9);", gStaticEidosValue_LogicalT);	// not the same object
	
	EidosAssertScriptSuccess("T != c(T, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("5 != c(5, 6);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("5.0 != c(5.0, 6.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("'foo' != c('foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("x = _Test(9); x != c(x, _Test(9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	
	EidosAssertScriptSuccess("c(T, F) != T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("c(5, 6) != 5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("c(5.0, 6.0) != 5.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("c('foo', 'bar') != 'foo';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("x = _Test(9); c(x, _Test(9)) != x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	
	EidosAssertScriptSuccess("c(T, F) != c(T, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("c(5, 6) != c(5, 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("c(5.0, 6.0) != c(5.0, 8.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("c('foo', 'bar') != c('foo', 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("x = _Test(9); c(x, _Test(9)) != c(x, x);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	
	EidosAssertScriptRaise("c(5,6) != c(5,6,7);", 7, "operator requires that either");
	
	// operator !=: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess("identical(5 != 5, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 != matrix(2), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(5 != matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(2 != matrix(1:3), matrix(c(T,F,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((1:3) != matrix(2), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((1:3) != matrix(3:1), matrix(c(T,F,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(5) != matrix(2), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(5) != matrix(5), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3) != matrix(2), matrix(c(1.0,4,9)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(2:4,nrow=1) != matrix(1:3,ncol=1), matrix(c(2.0,9,64)));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(1:3) != matrix(3:1), matrix(c(T,F,T)));", gStaticEidosValue_LogicalT);
}

#pragma mark operator :
void _RunOperatorRangeTests(void)
{
	// operator :
	EidosAssertScriptRaise("NULL:T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL:0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL:0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL:'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL:_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL:(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T:NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0:NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5:NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo':NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7):NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2):NULL;", 5, "is not supported by");
	EidosAssertScriptRaise(":NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("1:5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("5:1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 4, 3, 2, 1}));
	EidosAssertScriptSuccess("-2:1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-2, -1, 0, 1}));
	EidosAssertScriptSuccess("1:-2;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 0, -1, -2}));
	EidosAssertScriptSuccess("1:1;", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("1.0:5;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("5.0:1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5, 4, 3, 2, 1}));
	EidosAssertScriptSuccess("-2.0:1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-2, -1, 0, 1}));
	EidosAssertScriptSuccess("1.0:-2;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 0, -1, -2}));
	EidosAssertScriptSuccess("1.0:1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
	EidosAssertScriptSuccess("1:5.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("5:1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5, 4, 3, 2, 1}));
	EidosAssertScriptSuccess("-2:1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-2, -1, 0, 1}));
	EidosAssertScriptSuccess("1:-2.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 0, -1, -2}));
	EidosAssertScriptSuccess("1:1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
	EidosAssertScriptRaise("1:F;", 1, "is not supported by");
	EidosAssertScriptRaise("F:1;", 1, "is not supported by");
	EidosAssertScriptRaise("T:F;", 1, "is not supported by");
	EidosAssertScriptRaise("'a':'z';", 3, "is not supported by");
	EidosAssertScriptRaise("1:(2:3);", 1, "operator must have size()");
	EidosAssertScriptRaise("(1:2):3;", 5, "operator must have size()");
	EidosAssertScriptSuccess("1.5:4.7;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.5, 2.5, 3.5, 4.5}));
	EidosAssertScriptSuccess("1.5:-2.7;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.5, 0.5, -0.5, -1.5, -2.5}));
	EidosAssertScriptRaise("1.5:INF;", 3, "range with more than");
	EidosAssertScriptRaise("1.5:NAN;", 3, "must not be NAN");
	EidosAssertScriptRaise("INF:1.5;", 3, "range with more than");
	EidosAssertScriptRaise("NAN:1.5;", 3, "must not be NAN");
	EidosAssertScriptRaise("1:100000010;", 1, "more than 100000000 entries");
	EidosAssertScriptRaise("100000010:1;", 9, "more than 100000000 entries");
	
	EidosAssertScriptRaise("matrix(5):9;", 9, "must not be matrices or arrays");
	EidosAssertScriptRaise("1:matrix(5);", 1, "must not be matrices or arrays");
	EidosAssertScriptRaise("matrix(3):matrix(5);", 9, "must not be matrices or arrays");
	EidosAssertScriptRaise("matrix(5:8):9;", 11, "must have size() == 1");
	EidosAssertScriptRaise("1:matrix(5:8);", 1, "must have size() == 1");
	EidosAssertScriptRaise("matrix(1:3):matrix(5:7);", 11, "must have size() == 1");
}

#pragma mark operator ^
void _RunOperatorExpTests(void)
{
	// operator ^
	EidosAssertScriptRaise("NULL^T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL^0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL^0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL^'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL^_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL^(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T^NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0^NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5^NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'^NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)^NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)^NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("^NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("1^1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
	EidosAssertScriptSuccess("1^-1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
	EidosAssertScriptSuccess("(0:2)^10;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, 1, 1024}));
	EidosAssertScriptSuccess("10^(0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 10, 100}));
	EidosAssertScriptSuccess("(15:13)^(0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 14, 169}));
	EidosAssertScriptRaise("(15:12)^(0:2);", 7, "operator requires that either");
	EidosAssertScriptRaise("NULL^(0:2);", 4, "is not supported by");
	EidosAssertScriptSuccess("1^1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
	EidosAssertScriptSuccess("1.0^1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
	EidosAssertScriptSuccess("1.0^-1.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
	EidosAssertScriptSuccess("(0:2.0)^10;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, 1, 1024}));
	EidosAssertScriptSuccess("10.0^(0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 10, 100}));
	EidosAssertScriptSuccess("10^(0.0:2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 10, 100}));
	EidosAssertScriptSuccess("(15.0:13)^(0:2.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 14, 169}));
	EidosAssertScriptRaise("(15:12.0)^(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("NULL^(0:2.0);", 4, "is not supported by");
	EidosAssertScriptRaise("'foo'^5;", 5, "is not supported by");
	EidosAssertScriptRaise("T^F;", 1, "is not supported by");
	EidosAssertScriptRaise("T^T;", 1, "is not supported by");
	EidosAssertScriptRaise("F^F;", 1, "is not supported by");
	EidosAssertScriptRaise("^5;", 0, "unexpected token");
	EidosAssertScriptRaise("^5.0;", 0, "unexpected token");
	EidosAssertScriptRaise("^'foo';", 0, "unexpected token");
	EidosAssertScriptRaise("^T;", 0, "unexpected token");
	EidosAssertScriptSuccess("4^(3^2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(262144)));		// right-associative!
	EidosAssertScriptSuccess("4^3^2;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(262144)));		// right-associative!
	
	// operator ^: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess("identical(5 ^ matrix(2), matrix(25.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(2 ^ matrix(1:3), matrix(c(2.0,4,8)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((1:3) ^ matrix(2), c(1.0,4,9));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical((2:4) ^ matrix(1:3), matrix(c(2.0,9,64)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(5) ^ matrix(2), matrix(25.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(1:3) ^ matrix(2), matrix(c(1.0,4,9)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(2:4,nrow=1) ^ matrix(1:3,ncol=1), matrix(c(2.0,9,64)));", 29, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(2:4) ^ matrix(1:3), matrix(c(2.0,9,64)));", gStaticEidosValue_LogicalT);
}

#pragma mark operator &
void _RunOperatorLogicalAndTests(void)
{
	// operator &
	EidosAssertScriptRaise("NULL&T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL&0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL&0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL&'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL&_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL&(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T&NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0&NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5&NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'&NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)&NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)&NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("&NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&T&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&F&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) & F;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) & T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("F & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("T & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) & c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) & c(F,F,T,T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false}));
	EidosAssertScriptSuccess("c(T,T,F,F) & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, false}));
	EidosAssertScriptSuccess("c(F,F,T,T) & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false}));
	EidosAssertScriptRaise("c(T,F,T,F) & c(F,F);", 11, "not compatible in size()");
	EidosAssertScriptRaise("c(T,T) & c(T,F,T,F);", 7, "not compatible in size()");
	EidosAssertScriptRaise("c(T,F,T,F) & _Test(3);", 11, "is not supported by");
	EidosAssertScriptRaise("_Test(3) & c(T,F,T,F);", 9, "is not supported by");
	EidosAssertScriptSuccess("5&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&5&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("0&T&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&0&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&0&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) & 0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("c(7,0,5,0) & T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("F & c(5,0,7,0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("9 & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c(7,0,5,0) & c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) & c(0,0,5,7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false}));
	EidosAssertScriptSuccess("5.0&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&5.0&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("0.0&T&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&0.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&0.0&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&0.0&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) & 0.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) & T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("F & c(5.0,0.0,7.0,0.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("9.0 & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) & c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) & c(0.0,0.0,5.0,7.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false}));
	EidosAssertScriptSuccess("INF&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&INF&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("T&NAN&F;", 1, "cannot be converted");
	EidosAssertScriptRaise("NAN&T&T;", 3, "cannot be converted");
	EidosAssertScriptSuccess("'foo'&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&'foo'&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo'&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("''&T&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&'';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&''&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&''&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) & '';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("c('foo','','foo','') & T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("F & c('foo','','foo','');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("'foo' & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c('foo','','foo','') & c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) & c('','','foo','foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false}));
	
	// operator &: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with logical should suffice
	EidosAssertScriptSuccess("identical(T & T, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & F, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & matrix(T), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & F & matrix(T), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & matrix(T) & F, matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & matrix(T) & matrix(T) & T, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & matrix(T) & matrix(F) & T, matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & matrix(T) & matrix(F) & c(T,F,T), c(F,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & matrix(T) & matrix(T) & c(T,F,T), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(T,F,T) & T & matrix(T) & matrix(F), c(F,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(T,F,T) & T & matrix(T) & matrix(T), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(c(T,F,T) & T & matrix(c(T,T,F)) & matrix(F), c(T,F,F));", 19, "non-conformable");
	EidosAssertScriptSuccess("identical(c(T,F,T) & T & matrix(c(T,T,F)) & matrix(c(T,F,T)), matrix(c(T,F,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & T, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & T & F, matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & matrix(T) & T & T, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & matrix(F) & T & T, matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(T) & matrix(c(T,F)) & T & T, matrix(F));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(c(T,F)) & matrix(F) & T & T, matrix(F));", 25, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(c(T,T)) & matrix(c(T,T)) & T & T, matrix(c(T,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(T,T)) & matrix(c(T,F)) & T & T, matrix(c(T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(T,T,T)) & matrix(c(T,F,F)) & c(T,F,T) & T, matrix(c(T,F,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,T)) & matrix(c(T,T,F)) & c(F,T,T) & T, matrix(c(F,T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & T & matrix(F) & c(T,F,T), c(F,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & T & matrix(T) & c(T,F,T), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & c(T,F,T) & T & matrix(F), c(F,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & c(T,F,T) & T & matrix(T), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & matrix(T), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) & matrix(F), matrix(F));", gStaticEidosValue_LogicalT);
}

#pragma mark operator |
void _RunOperatorLogicalOrTests(void)
{
	// operator |
	EidosAssertScriptRaise("NULL|T;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL|0;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL|0.5;", 4, "is not supported by");
	EidosAssertScriptRaise("NULL|'foo';", 4, "is not supported by");
	EidosAssertScriptRaise("NULL|_Test(7);", 4, "is not supported by");
	EidosAssertScriptRaise("NULL|(0:2);", 4, "is not supported by");
	EidosAssertScriptRaise("T|NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0|NULL;", 1, "is not supported by");
	EidosAssertScriptRaise("0.5|NULL;", 3, "is not supported by");
	EidosAssertScriptRaise("'foo'|NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("_Test(7)|NULL;", 8, "is not supported by");
	EidosAssertScriptRaise("(0:2)|NULL;", 5, "is not supported by");
	EidosAssertScriptRaise("|NULL;", 0, "unexpected token");
	EidosAssertScriptSuccess("T|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|T|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|F|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|F|F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) | F;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) | T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("F | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("T | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("c(T,F,T,F) | c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) | c(F,F,T,T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, true}));
	EidosAssertScriptSuccess("c(T,T,F,F) | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, false}));
	EidosAssertScriptSuccess("c(F,F,T,T) | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, true}));
	EidosAssertScriptRaise("c(T,F,T,F) | c(F,F);", 11, "not compatible in size()");
	EidosAssertScriptRaise("c(T,T) | c(T,F,T,F);", 7, "not compatible in size()");
	EidosAssertScriptRaise("c(T,F,T,F) | _Test(3);", 11, "is not supported by");
	EidosAssertScriptRaise("_Test(3) | c(T,F,T,F);", 9, "is not supported by");
	EidosAssertScriptSuccess("5|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|5|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5|F|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("0|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|0|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|0|F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) | 0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c(7,0,5,0) | T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("F | c(5,0,7,0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("9 | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("c(7,0,5,0) | c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) | c(0,0,5,7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, true}));
	EidosAssertScriptSuccess("5.0|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|5.0|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0|F|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("0.0|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|0.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|0.0|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|0.0|F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) | 0.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) | T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("F | c(5.0,0.0,7.0,0.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("9.0 | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) | c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) | c(0.0,0.0,5.0,7.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, true}));
	EidosAssertScriptSuccess("INF|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|INF|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("T|NAN|F;", 1, "cannot be converted");
	EidosAssertScriptRaise("NAN|T|T;", 3, "cannot be converted");
	EidosAssertScriptSuccess("'foo'|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|'foo'|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo'|F|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("''|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|'';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|''|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|''|F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) | '';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c('foo','','foo','') | T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("F | c('foo','','foo','');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("'foo' | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("c('foo','','foo','') | c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) | c('','','foo','foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, true}));
	
	// operator |: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with logical should suffice
	EidosAssertScriptSuccess("identical(T | F, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | F, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T | matrix(F), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | F | matrix(T), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | matrix(F) | F, matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | matrix(F) | matrix(T) | F, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | matrix(F) | matrix(F) | T, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | matrix(T) | matrix(F) | c(T,F,T), c(T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | matrix(F) | matrix(F) | c(T,F,T), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(T,F,T) | T | matrix(F) | matrix(F), c(T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(T,F,T) | F | matrix(T) | matrix(F), c(T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(c(T,F,T) | F | matrix(c(T,T,F)) | matrix(F), c(T,T,F));", 19, "non-conformable");
	EidosAssertScriptSuccess("identical(c(T,F,T) | F | matrix(c(T,F,F)) | matrix(c(T,F,T)), matrix(c(T,F,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) | F, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) | F | F, matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) | matrix(F) | T | F, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) | matrix(F) | F | F, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(T) | matrix(c(T,F)) | T | T, matrix(F));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(c(T,F)) | matrix(F) | T | T, matrix(F));", 25, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(c(T,F)) | matrix(c(F,F)) | F | F, matrix(c(T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T)) | matrix(c(F,F)) | F | T, matrix(c(T,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F)) | matrix(c(T,F,F)) | c(F,F,F) | F, matrix(c(T,T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,T)) | matrix(c(F,T,F)) | c(F,F,F) | T, matrix(c(T,T,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) | F | matrix(F) | c(T,F,T), c(T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) | F | matrix(F) | c(T,F,T), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) | c(T,F,T) | T | matrix(F), c(T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) | c(T,F,F) | F | matrix(F), c(T,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) | matrix(T), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) | matrix(F), matrix(F));", gStaticEidosValue_LogicalT);
}

#pragma mark operator !
void _RunOperatorLogicalNotTests(void)
{
	// operator !
	EidosAssertScriptRaise("!NULL;", 0, "is not supported by");
	EidosAssertScriptSuccess("!T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("!F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("!7;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("!0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("!7.1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("!0.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("!INF;", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("!NAN;", 0, "cannot be converted");
	EidosAssertScriptSuccess("!'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("!'';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("!logical(0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{}));
	EidosAssertScriptSuccess("!integer(0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{}));
	EidosAssertScriptSuccess("!float(0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{}));
	EidosAssertScriptSuccess("!string(0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{}));
	EidosAssertScriptRaise("!object();", 0, "is not supported by");
	EidosAssertScriptSuccess("!c(F,T,F,T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("!c(0,5,0,1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("!c(0,5.0,0,1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptRaise("!c(0,NAN,0,1.0);", 0, "cannot be converted");
	EidosAssertScriptSuccess("!c(0,INF,0,1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("!c('','foo','','bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptRaise("!_Test(5);", 0, "is not supported by");
	
	// operator |: test with matrices and arrays; the dimensionality code is shared across all operand types, so testing it with logical should suffice
	EidosAssertScriptSuccess("identical(!T, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!F, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!c(T,F,T), c(F,T,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!c(F,T,F), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!matrix(T), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!matrix(F), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!matrix(c(T,F,T)), matrix(c(F,T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!matrix(c(F,T,F)), matrix(c(T,F,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!array(T, c(1,1,1)), array(F, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!array(F, c(1,1,1)), array(T, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!array(c(T,F,T), c(3,1,1)), array(c(F,T,F), c(3,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!array(c(F,T,F), c(1,3,1)), array(c(T,F,T), c(1,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!array(c(T,F,T), c(1,1,3)), array(c(F,T,F), c(1,1,3)));", gStaticEidosValue_LogicalT);
}

#pragma mark operator ?
void _RunOperatorTernaryConditionalTests(void)
{
	// operator ?-else
	EidosAssertScriptSuccess("T ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("F ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("9 ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("0 ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("6 > 5 ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("6 < 5 ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptRaise("6 == 6:9 ? 23 else 42;", 9, "condition for ternary conditional has size()");
	EidosAssertScriptSuccess("(6 == (6:9))[0] ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("(6 == (6:9))[1] ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptRaise("_Test(6) ? 23 else 42;", 9, "cannot be converted");
	EidosAssertScriptRaise("NULL ? 23 else 42;", 5, "condition for ternary conditional has size()");
	EidosAssertScriptRaise("T ? 23; else 42;", 6, "expected 'else'");
	EidosAssertScriptRaise("T ? 23; x = 10;", 6, "expected 'else'");
	EidosAssertScriptRaise("(T ? x else y) = 10;", 15, "lvalue required");
	EidosAssertScriptSuccess("x = T ? 23 else 42; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("x = F ? 23 else 42; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	
	// test right-associativity; this produces 2 if ? else is left-associative since the left half would then evaluate to 1, which is T
	EidosAssertScriptSuccess("a = 0; a == 0 ? 1 else a == 1 ? 2 else 4;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(1)));
}
	
	// ************************************************************************************
	//
	//	Keyword tests
	//
	
#pragma mark if
void _RunKeywordIfTests(void)
{
	// if
	EidosAssertScriptSuccess("if (T) 23;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (F) 23;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (9) 23;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (0) 23;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (6 > 5) 23;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (6 < 5) 23;", gStaticEidosValueVOID);
	EidosAssertScriptRaise("if (6 == (6:9)) 23;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess("if ((6 == (6:9))[0]) 23;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if ((6 == (6:9))[1]) 23;", gStaticEidosValueVOID);
	EidosAssertScriptRaise("if (_Test(6)) 23;", 0, "cannot be converted");
	EidosAssertScriptRaise("if (NULL) 23;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess("if (matrix(1)) 23;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (matrix(0)) 23;", gStaticEidosValueVOID);
	EidosAssertScriptRaise("if (matrix(1:3)) 23;", 0, "condition for if statement has size()");
	
	// if-else
	EidosAssertScriptSuccess("if (T) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (F) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (9) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (0) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (6 > 5) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (6 < 5) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptRaise("if (6 == (6:9)) 23; else 42;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess("if ((6 == (6:9))[0]) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if ((6 == (6:9))[1]) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptRaise("if (_Test(6)) 23; else 42;", 0, "cannot be converted");
	EidosAssertScriptRaise("if (NULL) 23; else 42;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess("if (matrix(1)) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (matrix(0)) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptRaise("if (matrix(1:3)) 23; else 42;", 0, "condition for if statement has size()");
}

#pragma mark do
void _RunKeywordDoTests(void)
{
	// do
	EidosAssertScriptSuccess("x=1; do x=x*2; while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(128)));
	EidosAssertScriptSuccess("x=200; do x=x*2; while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(400)));
	EidosAssertScriptSuccess("x=1; do { x=x*2; x=x+1; } while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(127)));
	EidosAssertScriptSuccess("x=200; do { x=x*2; x=x+1; } while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(401)));
	EidosAssertScriptRaise("x=1; do x=x*2; while (x < 100:102); x;", 5, "condition for do-while loop has size()");
	EidosAssertScriptRaise("x=200; do x=x*2; while (x < 100:102); x;", 7, "condition for do-while loop has size()");
	EidosAssertScriptSuccess("x=1; do x=x*2; while ((x < 100:102)[0]); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(128)));
	EidosAssertScriptSuccess("x=200; do x=x*2; while ((x < 100:102)[0]); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(400)));
	EidosAssertScriptRaise("x=200; do x=x*2; while (_Test(6)); x;", 7, "cannot be converted");
	EidosAssertScriptRaise("x=200; do x=x*2; while (NULL); x;", 7, "condition for do-while loop has size()");
	EidosAssertScriptSuccess("x=10; do x=x-1; while (x); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0)));
}

#pragma mark while
void _RunKeywordWhileTests(void)
{
	// while
	EidosAssertScriptSuccess("x=1; while (x<100) x=x*2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(128)));
	EidosAssertScriptSuccess("x=200; while (x<100) x=x*2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(200)));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; x=x+1; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(127)));
	EidosAssertScriptSuccess("x=200; while (x<100) { x=x*2; x=x+1; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(200)));
	EidosAssertScriptRaise("x=1; while (x < 100:102) x=x*2; x;", 5, "condition for while loop has size()");
	EidosAssertScriptRaise("x=200; while (x < 100:102) x=x*2; x;", 7, "condition for while loop has size()");
	EidosAssertScriptSuccess("x=1; while ((x < 100:102)[0]) x=x*2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(128)));
	EidosAssertScriptSuccess("x=200; while ((x < 100:102)[0]) x=x*2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(200)));
	EidosAssertScriptRaise("x=200; while (_Test(6)) x=x*2; x;", 7, "cannot be converted");
	EidosAssertScriptRaise("x=200; while (NULL) x=x*2; x;", 7, "condition for while loop has size()");
	EidosAssertScriptSuccess("x=10; while (x) x=x-1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0)));
}

#pragma mark for / in
void _RunKeywordForInTests(void)
{
	// for and in
	EidosAssertScriptSuccess("x=0; for (y in integer(0)) x=x+1; x;", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("x=0; for (y in float(0)) x=x+1; x;", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("x=0; for (y in 33) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(33)));
	EidosAssertScriptSuccess("x=0; for (y in 33) x=x+1; x;", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("x=0; for (y in 1:10) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(55)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) x=x+1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { x=x+y; y = 7; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(55)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { x=x+1; y = 7; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x=0; for (y in 10:1) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(55)));
	EidosAssertScriptSuccess("x=0; for (y in 10:1) x=x+1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x=0; for (y in 1.0:10) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(55.0)));
	EidosAssertScriptSuccess("x=0; for (y in 1.0:10) x=x+1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10.0) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(55.0)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10.0) x=x+1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x=0; for (y in c('foo', 'bar')) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("0foobar")));
	EidosAssertScriptSuccess("x=0; for (y in c(T,T,F,F,T,F)) x=x+asInteger(y); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("x=0; for (y in _Test(7)) x=x+y._yolk; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x=0; for (y in rep(_Test(7),3)) x=x+y._yolk; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(21)));
	EidosAssertScriptRaise("x=0; y=0:2; for (y[0] in 2:4) x=x+sum(y); x;", 18, "unexpected token");	// lvalue must be an identifier, at present
	EidosAssertScriptRaise("x=0; for (y in NULL) x;", 5, "does not allow NULL");
	EidosAssertScriptSuccess("x=0; q=11:20; for (y in seqAlong(q)) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(45)));
	EidosAssertScriptSuccess("x=0; q=11:20; for (y in seqAlong(q)) x=x+1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptRaise("x=0; q=11:20; for (y in seqAlong(q, 5)) x=x+y; x;", 24, "too many arguments supplied");
	EidosAssertScriptRaise("x=0; q=11:20; for (y in seqAlong()) x=x+y; x;", 24, "missing required");
	EidosAssertScriptSuccess("x=0; for (y in seq(1,10)) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(55)));
	EidosAssertScriptSuccess("x=0; for (y in seq(1,10)) x=x+1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x=0; for (y in seqLen(5)) x=x+y+2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(20)));
	EidosAssertScriptSuccess("x=0; for (y in seqLen(1)) x=x+y+2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("x=0; for (y in seqLen(0)) x=x+y+2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0)));
	EidosAssertScriptRaise("x=0; for (y in seqLen(-1)) x=x+y+2; x;", 15, "requires length to be");
	EidosAssertScriptRaise("x=0; for (y in seqLen(5:6)) x=x+y+2; x;", 15, "must be a singleton");
	EidosAssertScriptRaise("x=0; for (y in seqLen('f')) x=x+y+2; x;", 15, "cannot be type");
	
	// additional tests for zero-length ranges; seqAlong() is treated separately in the for() code, so it is tested separately here
	EidosAssertScriptSuccess("i=10; for (i in integer(0)) ; i;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("i=10; for (i in seqAlong(integer(0))) ; i;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("i=10; b=13; for (i in integer(0)) b=i; i;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("i=10; b=13; for (i in seqAlong(integer(0))) b=i; i;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("i=10; b=13; for (i in integer(0)) b=i; b;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(13)));
	EidosAssertScriptSuccess("i=10; b=13; for (i in seqAlong(integer(0))) b=i; b;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(13)));
	
	EidosAssertScriptRaise("for (i in matrix(5):9) i;", 19, "must not be matrices or arrays");
	EidosAssertScriptRaise("for (i in 1:matrix(5)) i;", 11, "must not be matrices or arrays");
	EidosAssertScriptRaise("for (i in matrix(3):matrix(5)) i;", 19, "must not be matrices or arrays");
	EidosAssertScriptRaise("for (i in matrix(5:8):9) i;", 21, "must have size() == 1");
	EidosAssertScriptRaise("for (i in 1:matrix(5:8)) i;", 11, "must have size() == 1");
	EidosAssertScriptRaise("for (i in matrix(1:3):matrix(5:7)) i;", 21, "must have size() == 1");
	EidosAssertScriptSuccess("x = 0; for (i in seqAlong(matrix(1))) x=x+i; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0)));
	EidosAssertScriptSuccess("x = 0; for (i in seqAlong(matrix(1:3))) x=x+i; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
}

#pragma mark next
void _RunKeywordNextTests(void)
{
	// next
	EidosAssertScriptRaise("next;", 0, "encountered with no enclosing loop");
	EidosAssertScriptRaise("if (T) next;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("if (F) next;", gStaticEidosValueVOID);
	EidosAssertScriptRaise("if (T) next; else 42;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("if (F) next; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (T) 23; else next;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptRaise("if (F) 23; else next;", 16, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) next; x=x+1; } while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(124)));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) next; x=x+1; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(124)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) next; x=x+y; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(50)));
}

#pragma mark break
void _RunKeywordBreakTests(void)
{
	// break
	EidosAssertScriptRaise("break;", 0, "encountered with no enclosing loop");
	EidosAssertScriptRaise("if (T) break;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("if (F) break;", gStaticEidosValueVOID);
	EidosAssertScriptRaise("if (T) break; else 42;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("if (F) break; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (T) 23; else break;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptRaise("if (F) 23; else break;", 16, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) break; x=x+1; } while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(62)));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) break; x=x+1; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(62)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) break; x=x+y; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
}

#pragma mark return
void _RunKeywordReturnTests(void)
{
	// return
	EidosAssertScriptSuccess("return;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("return NULL;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("return -13;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-13)));
	EidosAssertScriptSuccess("if (T) return;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (T) return NULL;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (T) return -13;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-13)));
	EidosAssertScriptSuccess("if (F) return;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (F) return NULL;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (F) return -13;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (T) return; else return 42;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (T) return NULL; else return 42;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (T) return -13; else return 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-13)));
	EidosAssertScriptSuccess("if (F) return; else return 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (F) return -13; else return 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (T) return 23; else return;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (T) return 23; else return -13;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (F) return 23; else return;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (F) return 23; else return NULL;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (F) return 23; else return -13;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-13)));
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) return; x=x+1; } while (x<100); x;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) return x-5; x=x+1; } while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(57)));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) return; x=x+1; } x;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) return x-5; x=x+1; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(57)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) return; x=x+y; } x;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) return x-5; x=x+y; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
}
	
	// ************************************************************************************
	//
	//	Function tests
	//
	#pragma mark -
	#pragma mark Functions
	#pragma mark -
	
#pragma mark math
void _RunFunctionMathTests_a_through_f(void)
{
	// abs()
	EidosAssertScriptSuccess("abs(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("abs(-5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("abs(c(-2, 7, -18, 12));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 7, 18, 12}));
	EidosAssertScriptSuccess("abs(5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5.5)));
	EidosAssertScriptSuccess("abs(-5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5.5)));
	EidosAssertScriptSuccess("abs(c(-2.0, 7.0, -18.0, 12.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2, 7, 18, 12}));
	EidosAssertScriptRaise("abs(T);", 0, "cannot be type");
	EidosAssertScriptRaise("abs('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("abs(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("abs(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("abs(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("abs(integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("abs(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("abs(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("-9223372036854775807 - 1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(INT64_MIN)));
	EidosAssertScriptRaise("abs(-9223372036854775807 - 1);", 0, "most negative integer");
	EidosAssertScriptRaise("abs(c(17, -9223372036854775807 - 1));", 0, "most negative integer");
	EidosAssertScriptSuccess("abs(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(abs(matrix(5)), matrix(5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(abs(matrix(-5)), matrix(5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(abs(matrix(5:7)), matrix(5:7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(abs(matrix(-5:-7)), matrix(5:7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(abs(array(5, c(1,1,1))), array(5, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(abs(array(-5, c(1,1,1))), array(5, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(abs(array(5:7, c(3,1,1))), array(5:7, c(3,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(abs(array(-5:-7, c(1,3,1))), array(5:7, c(1,3,1)));", gStaticEidosValue_LogicalT);
	
	// acos()
	EidosAssertScriptSuccess("abs(acos(0) - PI/2) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(acos(1) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(acos(c(0, 1, -1)) - c(PI/2, 0, PI))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(acos(0.0) - PI/2) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(acos(1.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(acos(c(0.0, 1.0, -1.0)) - c(PI/2, 0, PI))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("acos(T);", 0, "cannot be type");
	EidosAssertScriptRaise("acos('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("acos(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("acos(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("acos(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("acos(integer(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("acos(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("acos(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("acos(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(acos(matrix(0.5)), matrix(acos(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(acos(matrix(c(0.1, 0.2, 0.3))), matrix(acos(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// asin()
	EidosAssertScriptSuccess("abs(asin(0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(asin(1) - PI/2) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(asin(c(0, 1, -1)) - c(0, PI/2, -PI/2))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(asin(0.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(asin(1.0) - PI/2) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(asin(c(0.0, 1.0, -1.0)) - c(0, PI/2, -PI/2))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("asin(T);", 0, "cannot be type");
	EidosAssertScriptRaise("asin('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("asin(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("asin(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("asin(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("asin(integer(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("asin(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("asin(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("asin(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(asin(matrix(0.5)), matrix(asin(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(asin(matrix(c(0.1, 0.2, 0.3))), matrix(asin(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// atan()
	EidosAssertScriptSuccess("abs(atan(0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan(1) - PI/4) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(atan(c(0, 1, -1)) - c(0, PI/4, -PI/4))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan(0.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan(1.0) - PI/4) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(atan(c(0.0, 1.0, -1.0)) - c(0, PI/4, -PI/4))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("atan(T);", 0, "cannot be type");
	EidosAssertScriptRaise("atan('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("atan(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("atan(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("atan(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("atan(integer(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("atan(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("atan(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("atan(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(atan(matrix(0.5)), matrix(atan(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(atan(matrix(c(0.1, 0.2, 0.3))), matrix(atan(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// atan2()
	EidosAssertScriptSuccess("abs(atan2(0, 1) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan2(0, -1) - PI) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(atan2(c(0, 0, -1), c(1, -1, 0)) - c(0, PI, -PI/2))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan2(0.0, 1.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan2(0.0, -1.0) - PI) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(atan2(c(0.0, 0.0, -1.0), c(1.0, -1.0, 0.0)) - c(0, PI, -PI/2))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("atan2(T);", 0, "missing required argument");
	EidosAssertScriptRaise("atan2('foo');", 0, "missing required argument");
	EidosAssertScriptRaise("atan2(_Test(7));", 0, "missing required argument");
	EidosAssertScriptRaise("atan2(NULL);", 0, "missing required argument");
	EidosAssertScriptRaise("atan2(0, T);", 0, "cannot be type");
	EidosAssertScriptRaise("atan2(0, 'foo');", 0, "cannot be type");
	EidosAssertScriptRaise("atan2(0, _Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("atan2(0, NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("atan2(logical(0), logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("atan2(integer(0), integer(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("atan2(float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("atan2(string(0), string(0));", 0, "cannot be type");
	EidosAssertScriptRaise("atan2(0.0, c(0.0, 1.0));", 0, "requires arguments of equal length");		// argument count mismatch
	EidosAssertScriptSuccess("atan2(0.5, NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("atan2(NAN, 0.5);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(atan2(matrix(0.5), matrix(0.25)), matrix(atan2(0.5, 0.25)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(atan2(matrix(0.5), 0.25), matrix(atan2(0.5, 0.25)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(atan2(0.5, matrix(0.25)), matrix(atan2(0.5, 0.25)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(atan2(matrix(c(0.1, 0.2, 0.3)), matrix(c(0.3, 0.2, 0.1))), matrix(atan2(c(0.1, 0.2, 0.3), c(0.3, 0.2, 0.1))));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(atan2(matrix(c(0.1, 0.2, 0.3)), c(0.3, 0.2, 0.1)), matrix(atan2(c(0.1, 0.2, 0.3), c(0.3, 0.2, 0.1))));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(atan2(c(0.1, 0.2, 0.3), matrix(c(0.3, 0.2, 0.1))), matrix(atan2(c(0.1, 0.2, 0.3), c(0.3, 0.2, 0.1))));", gStaticEidosValue_LogicalT);
	
	// ceil()
	EidosAssertScriptSuccess("ceil(5.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(6.0)));
	EidosAssertScriptSuccess("ceil(-5.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-5.0)));
	EidosAssertScriptSuccess("ceil(c(-2.1, 7.1, -18.8, 12.8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-2.0, 8, -18, 13}));
	EidosAssertScriptRaise("ceil(T);", 0, "cannot be type");
	EidosAssertScriptRaise("ceil(5);", 0, "cannot be type");
	EidosAssertScriptRaise("ceil('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("ceil(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("ceil(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("ceil(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("ceil(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("ceil(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("ceil(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("ceil(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(ceil(matrix(0.3)), matrix(ceil(0.3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ceil(matrix(0.6)), matrix(ceil(0.6)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ceil(matrix(-0.3)), matrix(ceil(-0.3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ceil(matrix(-0.6)), matrix(ceil(-0.6)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ceil(matrix(c(0.1, 5.7, -0.3))), matrix(ceil(c(0.1, 5.7, -0.3))));", gStaticEidosValue_LogicalT);
	
	// cos()
	EidosAssertScriptSuccess("abs(cos(0) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cos(0.0) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cos(PI/2) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(cos(c(0, PI/2, PI)) - c(1, 0, -1))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("cos(T);", 0, "cannot be type");
	EidosAssertScriptRaise("cos('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("cos(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("cos(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("cos(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("cos(integer(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("cos(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("cos(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("cos(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(cos(matrix(0.5)), matrix(cos(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cos(matrix(c(0.1, 0.2, 0.3))), matrix(cos(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// cumProduct()
	EidosAssertScriptSuccess("cumProduct(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("cumProduct(-5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-5)));
	EidosAssertScriptSuccess("cumProduct(c(-2, 7, -18, 12));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-2, -14, 252, 3024}));
	EidosAssertScriptSuccess("cumProduct(5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5.5)));
	EidosAssertScriptSuccess("cumProduct(-5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-5.5)));
	EidosAssertScriptSuccess("cumProduct(c(-2.0, 7.0, -18.0, 12.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-2.0, -14.0, 252.0, 3024.0}));
	EidosAssertScriptRaise("cumProduct(T);", 0, "cannot be type");
	EidosAssertScriptRaise("cumProduct('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("cumProduct(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("cumProduct(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("cumProduct(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("cumProduct(integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("cumProduct(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("cumProduct(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("-9223372036854775807 - 1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(INT64_MIN)));
#if EIDOS_HAS_OVERFLOW_BUILTINS
	EidosAssertScriptRaise("-9223372036854775807 - 2;", 21, "subtraction overflow");
	EidosAssertScriptRaise("cumProduct(c(-922337203685477581, 10));", 0, "multiplication overflow");
	EidosAssertScriptRaise("cumProduct(c(922337203685477581, 10));", 0, "multiplication overflow");
#endif
	EidosAssertScriptSuccess("cumProduct(c(5, 5, 3.0, NAN, 2.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5.0, 25.0, 75.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()}));
	
	EidosAssertScriptSuccess("identical(cumProduct(matrix(0.5)), matrix(cumProduct(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cumProduct(matrix(c(0.1, 0.2, 0.3))), matrix(cumProduct(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// cumSum()
	EidosAssertScriptSuccess("cumSum(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("cumSum(-5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-5)));
	EidosAssertScriptSuccess("cumSum(c(-2, 7, -18, 12));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-2, 5, -13, -1}));
	EidosAssertScriptSuccess("cumSum(5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5.5)));
	EidosAssertScriptSuccess("cumSum(-5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-5.5)));
	EidosAssertScriptSuccess("cumSum(c(-2.0, 7.0, -18.0, 12.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-2.0, 5.0, -13.0, -1.0}));
	EidosAssertScriptRaise("cumSum(T);", 0, "cannot be type");
	EidosAssertScriptRaise("cumSum('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("cumSum(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("cumSum(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("cumSum(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("cumSum(integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("cumSum(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("cumSum(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("-9223372036854775807 - 1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(INT64_MIN)));
#if EIDOS_HAS_OVERFLOW_BUILTINS
	EidosAssertScriptRaise("-9223372036854775807 - 2;", 21, "subtraction overflow");
	EidosAssertScriptRaise("cumSum(c(-9223372036854775807, -1, -1));", 0, "addition overflow");
	EidosAssertScriptRaise("cumSum(c(9223372036854775807, 1, 1));", 0, "addition overflow");
#endif
	EidosAssertScriptSuccess("cumSum(c(5, 5, 3.0, NAN, 2.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5.0, 10.0, 13.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()}));
	
	EidosAssertScriptSuccess("identical(cumSum(matrix(0.5)), matrix(cumSum(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cumSum(matrix(c(0.1, 0.2, 0.3))), matrix(cumSum(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// exp()
	EidosAssertScriptSuccess("abs(exp(0) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(exp(0.0) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(exp(1.0) - E) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(exp(c(0, 1.0, -1)) - c(1, E, 0.3678794))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("exp(T);", 0, "cannot be type");
	EidosAssertScriptRaise("exp('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("exp(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("exp(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("exp(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("exp(integer(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("exp(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("exp(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("exp(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(exp(matrix(0.5)), matrix(exp(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(exp(matrix(c(0.1, 0.2, 0.3))), matrix(exp(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// floor()
	EidosAssertScriptSuccess("floor(5.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5.0)));
	EidosAssertScriptSuccess("floor(-5.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-6.0)));
	EidosAssertScriptSuccess("floor(c(-2.1, 7.1, -18.8, 12.8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-3.0, 7, -19, 12}));
	EidosAssertScriptRaise("floor(T);", 0, "cannot be type");
	EidosAssertScriptRaise("floor(5);", 0, "cannot be type");
	EidosAssertScriptRaise("floor('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("floor(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("floor(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("floor(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("floor(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("floor(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("floor(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("floor(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(floor(matrix(0.3)), matrix(floor(0.3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(floor(matrix(0.6)), matrix(floor(0.6)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(floor(matrix(-0.3)), matrix(floor(-0.3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(floor(matrix(-0.6)), matrix(floor(-0.6)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(floor(matrix(c(0.1, 5.7, -0.3))), matrix(floor(c(0.1, 5.7, -0.3))));", gStaticEidosValue_LogicalT);
}

void _RunFunctionMathTests_g_through_r(void)
{
	// integerDiv()
	EidosAssertScriptSuccess("integerDiv(6, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("integerDiv(7, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("integerDiv(8, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("integerDiv(9, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("integerDiv(6:9, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 2, 2, 3}));
	EidosAssertScriptSuccess("integerDiv(6:9, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 3, 4, 4}));
	EidosAssertScriptSuccess("integerDiv(-6:-9, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-2, -2, -2, -3}));
	EidosAssertScriptSuccess("integerDiv(-6:-9, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-3, -3, -4, -4}));
	EidosAssertScriptSuccess("integerDiv(6, 2:6);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 2, 1, 1, 1}));
	EidosAssertScriptSuccess("integerDiv(8:12, 2:6);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 3, 2, 2, 2}));
	EidosAssertScriptSuccess("integerDiv(-6, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-2)));
	EidosAssertScriptSuccess("integerDiv(-7, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-2)));
	EidosAssertScriptSuccess("integerDiv(-8, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-2)));
	EidosAssertScriptSuccess("integerDiv(-9, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-3)));
	EidosAssertScriptSuccess("integerDiv(6, -3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-2)));
	EidosAssertScriptSuccess("integerDiv(7, -3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-2)));
	EidosAssertScriptSuccess("integerDiv(8, -3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-2)));
	EidosAssertScriptSuccess("integerDiv(9, -3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-3)));
	EidosAssertScriptSuccess("integerDiv(-6, -3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("integerDiv(-7, -3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("integerDiv(-8, -3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("integerDiv(-9, -3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptRaise("integerDiv(10, 0);", 0, "division by 0");
	EidosAssertScriptRaise("integerDiv(9:10, 0:1);", 0, "division by 0");
	EidosAssertScriptRaise("integerDiv(9, 0:1);", 0, "division by 0");
	EidosAssertScriptRaise("integerDiv(9:10, 0);", 0, "division by 0");
	EidosAssertScriptRaise("integerDiv(9:10, 1:3);", 0, "requires that either");
	
	EidosAssertScriptSuccess("identical(integerDiv(5, matrix(2)), matrix(2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(integerDiv(12, matrix(1:3)), matrix(c(12,6,4)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(integerDiv(1:3, matrix(2)), c(0,1,1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(integerDiv(4:6, matrix(1:3)), matrix(c(4,2,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(integerDiv(matrix(5), matrix(2)), matrix(2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(integerDiv(matrix(1:3), matrix(2)), matrix(c(0,1,1)));", 10, "non-conformable");
	EidosAssertScriptRaise("identical(integerDiv(matrix(4:6,nrow=1), matrix(1:3,ncol=1)), matrix(c(4,2,2)));", 10, "non-conformable");
	EidosAssertScriptSuccess("identical(integerDiv(matrix(7:9), matrix(1:3)), matrix(c(7,4,3)));", gStaticEidosValue_LogicalT);
	
	// integerMod()
	EidosAssertScriptSuccess("integerMod(6, 3);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("integerMod(7, 3);", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("integerMod(8, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("integerMod(9, 3);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("integerMod(6:9, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 1, 2, 0}));
	EidosAssertScriptSuccess("integerMod(6:9, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 1, 0, 1}));
	EidosAssertScriptSuccess("integerMod(-6:-9, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, -1, -2, 0}));
	EidosAssertScriptSuccess("integerMod(-6:-9, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, -1, 0, -1}));
	EidosAssertScriptSuccess("integerMod(6, 2:6);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 2, 1, 0}));
	EidosAssertScriptSuccess("integerMod(8:12, 2:6);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 2, 1, 0}));
	EidosAssertScriptSuccess("integerMod(-6, 3);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("integerMod(-7, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("integerMod(-8, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-2)));
	EidosAssertScriptSuccess("integerMod(-9, 3);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("integerMod(6, -3);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("integerMod(7, -3);", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("integerMod(8, -3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("integerMod(9, -3);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("integerMod(-6, -3);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("integerMod(-7, -3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("integerMod(-8, -3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-2)));
	EidosAssertScriptSuccess("integerMod(-9, -3);", gStaticEidosValue_Integer0);
	EidosAssertScriptRaise("integerMod(10, 0);", 0, "modulo by 0");
	EidosAssertScriptRaise("integerMod(9:10, 0:1);", 0, "modulo by 0");
	EidosAssertScriptRaise("integerMod(9, 0:1);", 0, "modulo by 0");
	EidosAssertScriptRaise("integerMod(9:10, 0);", 0, "modulo by 0");
	EidosAssertScriptRaise("integerMod(9:10, 1:3);", 0, "requires that either");
	
	EidosAssertScriptSuccess("identical(integerMod(5, matrix(2)), matrix(1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(integerMod(5, matrix(1:3)), matrix(c(0,1,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(integerMod(1:3, matrix(2)), c(1,0,1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(integerMod(4:6, matrix(1:3)), matrix(c(0,1,0)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(integerMod(matrix(5), matrix(2)), matrix(1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(integerMod(matrix(1:3), matrix(2)), matrix(c(1,0,1)));", 10, "non-conformable");
	EidosAssertScriptRaise("identical(integerMod(matrix(4:6,nrow=1), matrix(1:3,ncol=1)), matrix(c(0,1,0)));", 10, "non-conformable");
	EidosAssertScriptSuccess("identical(integerMod(matrix(6:8), matrix(1:3)), matrix(c(0,1,2)));", gStaticEidosValue_LogicalT);
	
	// isFinite()
	EidosAssertScriptSuccess("isFinite(0.0);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isFinite(0.05);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isFinite(INF);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFinite(NAN);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFinite(c(5/0, 0/0, 17.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true}));	// INF, NAN, normal
	EidosAssertScriptRaise("isFinite(1);", 0, "cannot be type");
	EidosAssertScriptRaise("isFinite(T);", 0, "cannot be type");
	EidosAssertScriptRaise("isFinite('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("isFinite(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("isFinite(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("isFinite(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("isFinite(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("isFinite(float(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptRaise("isFinite(string(0));", 0, "cannot be type");
	
	EidosAssertScriptSuccess("identical(isFinite(5.0), T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(isFinite(c(5.0, INF, NAN)), c(T,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(isFinite(matrix(5.0)), matrix(T));",gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(isFinite(matrix(c(5.0, INF, NAN))), matrix(c(T,F,F)));", gStaticEidosValue_LogicalT);
	
	// isInfinite()
	EidosAssertScriptSuccess("isInfinite(0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInfinite(0.05);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInfinite(INF);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isInfinite(NAN);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInfinite(c(5/0, 0/0, 17.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false}));	// INF, NAN, normal
	EidosAssertScriptRaise("isInfinite(1);", 0, "cannot be type");
	EidosAssertScriptRaise("isInfinite(T);", 0, "cannot be type");
	EidosAssertScriptRaise("isInfinite('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("isInfinite(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("isInfinite(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("isInfinite(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("isInfinite(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("isInfinite(float(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptRaise("isInfinite(string(0));", 0, "cannot be type");
	
	EidosAssertScriptSuccess("identical(isInfinite(5.0), F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(isInfinite(c(5.0, INF, NAN)), c(F,T,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(isInfinite(matrix(5.0)), matrix(F));",gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(isInfinite(matrix(c(5.0, INF, NAN))), matrix(c(F,T,F)));", gStaticEidosValue_LogicalT);
	
	// isNAN()
	EidosAssertScriptSuccess("isNAN(0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNAN(0.05);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNAN(INF);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNAN(NAN);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isNAN(c(5/0, 0/0, 17.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, false}));	// INF, NAN, normal
	EidosAssertScriptRaise("isNAN(1);", 0, "cannot be type");
	EidosAssertScriptRaise("isNAN(T);", 0, "cannot be type");
	EidosAssertScriptRaise("isNAN('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("isNAN(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("isNAN(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("isNAN(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("isNAN(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("isNAN(float(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptRaise("isNAN(string(0));", 0, "cannot be type");
	
	EidosAssertScriptSuccess("identical(isNAN(5.0), F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(isNAN(c(5.0, INF, NAN)), c(F,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(isNAN(matrix(5.0)), matrix(F));",gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(isNAN(matrix(c(5.0, INF, NAN))), matrix(c(F,F,T)));", gStaticEidosValue_LogicalT);
	
	// log()
	EidosAssertScriptSuccess("abs(log(1) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(log(E) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(log(E^3.5) - 3.5) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(log(c(1, E, E^3.5)) - c(0, 1, 3.5))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("log(T);", 0, "cannot be type");
	EidosAssertScriptRaise("log('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("log(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("log(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("log(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("log(integer(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("log(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("log(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("log(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(log(matrix(0.5)), matrix(log(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(log(matrix(c(0.1, 0.2, 0.3))), matrix(log(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// log10()
	EidosAssertScriptSuccess("abs(log10(1) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(log10(10) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(log10(0.001) - -3) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(log10(c(1, 10, 0.001)) - c(0, 1, -3))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("log10(T);", 0, "cannot be type");
	EidosAssertScriptRaise("log10('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("log10(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("log10(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("log10(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("log10(integer(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("log10(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("log10(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("log10(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(log10(matrix(0.5)), matrix(log10(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(log10(matrix(c(0.1, 0.2, 0.3))), matrix(log10(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// log2()
	EidosAssertScriptSuccess("abs(log2(1) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(log2(2) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(log2(0.125) - -3) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(log2(c(1, 2, 0.125)) - c(0, 1, -3))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("log2(T);", 0, "cannot be type");
	EidosAssertScriptRaise("log2('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("log2(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("log2(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("log2(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("log2(integer(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("log2(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("log2(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("log2(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(log2(matrix(0.5)), matrix(log2(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(log2(matrix(c(0.1, 0.2, 0.3))), matrix(log2(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// product()
	EidosAssertScriptSuccess("product(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("product(-5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-5)));
	EidosAssertScriptSuccess("product(c(-2, 7, -18, 12));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3024)));
#if EIDOS_HAS_OVERFLOW_BUILTINS
	EidosAssertScriptSuccess("product(c(200000000, 3000000000000, 1000));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(6e23)));
#endif
	EidosAssertScriptSuccess("product(5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5.5)));
	EidosAssertScriptSuccess("product(-5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-5.5)));
	EidosAssertScriptSuccess("product(c(-2.5, 7.5, -18.5, 12.5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-2.5*7.5*-18.5*12.5)));
	EidosAssertScriptRaise("product(T);", 0, "cannot be type");
	EidosAssertScriptRaise("product('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("product(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("product(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("product(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("product(integer(0));", gStaticEidosValue_Integer1);	// product of no elements is 1 (as in R)
	EidosAssertScriptSuccess("product(float(0));", gStaticEidosValue_Float1);
	EidosAssertScriptRaise("product(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("product(c(5.0, 2.0, NAN, 2.0));", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("product(matrix(5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("product(matrix(c(5, -5)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-25)));
	EidosAssertScriptSuccess("product(array(c(5, -5, 3), c(1,3,1)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-75)));
	
	// round()
	EidosAssertScriptSuccess("round(5.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5.0)));
	EidosAssertScriptSuccess("round(-5.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-5.0)));
	EidosAssertScriptSuccess("round(c(-2.1, 7.1, -18.8, 12.8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-2.0, 7, -19, 13}));
	EidosAssertScriptRaise("round(T);", 0, "cannot be type");
	EidosAssertScriptRaise("round(5);", 0, "cannot be type");
	EidosAssertScriptRaise("round('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("round(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("round(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("round(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("round(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("round(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("round(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("round(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(round(matrix(0.3)), matrix(round(0.3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(round(matrix(0.6)), matrix(round(0.6)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(round(matrix(-0.3)), matrix(round(-0.3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(round(matrix(-0.6)), matrix(round(-0.6)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(round(matrix(c(0.1, 5.7, -0.3))), matrix(round(c(0.1, 5.7, -0.3))));", gStaticEidosValue_LogicalT);
}

void _RunFunctionMathTests_setUnionIntersection(void)
{
	// setUnion()
	EidosAssertScriptSuccess("setUnion(NULL, NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("setUnion(logical(0), logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setUnion(integer(0), integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setUnion(float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setUnion(string(0), string(0));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setUnion(object(), object());", gStaticEidosValue_Object_ZeroVec);
	EidosAssertScriptSuccess("size(setUnion(_Test(7)[F], object()));", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("size(setUnion(object(), _Test(7)[F]));", gStaticEidosValue_Integer0);
	
	EidosAssertScriptRaise("setUnion(NULL, logical(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setUnion(logical(0), integer(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setUnion(integer(0), float(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setUnion(float(0), string(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setUnion(string(0), object());", 0, "requires that both operands");
	EidosAssertScriptRaise("setUnion(object(), NULL);", 0, "requires that both operands");
	
	EidosAssertScriptSuccess("setUnion(T, logical(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setUnion(logical(0), T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setUnion(F, logical(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setUnion(logical(0), F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setUnion(7, integer(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setUnion(integer(0), 7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setUnion(3.2, float(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setUnion(float(0), 3.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setUnion('foo', string(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setUnion(string(0), 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setUnion(_Test(7), object())._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setUnion(object(), _Test(7))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	
	EidosAssertScriptSuccess("setUnion(c(T, T, T), logical(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setUnion(logical(0), c(F, F, F));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setUnion(c(F, F, T), logical(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setUnion(logical(0), c(F, F, T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setUnion(c(7, 7, 7), integer(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setUnion(integer(0), c(7, 7, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setUnion(c(7, 8, 7), integer(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8}));
	EidosAssertScriptSuccess("setUnion(integer(0), c(7, 7, 8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8}));
	EidosAssertScriptSuccess("setUnion(c(3.2, 3.2, 3.2), float(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setUnion(float(0), c(3.2, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setUnion(c(4.2, 3.2, 3.2), float(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{4.2, 3.2}));
	EidosAssertScriptSuccess("setUnion(float(0), c(3.2, 4.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 4.2}));
	EidosAssertScriptSuccess("setUnion(c('foo', 'foo', 'foo'), string(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setUnion(string(0), c('foo', 'foo', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setUnion(c('foo', 'bar', 'foo'), string(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar"}));
	EidosAssertScriptSuccess("setUnion(string(0), c('foo', 'foo', 'bar'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar"}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setUnion(c(x, x, x), object())._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setUnion(object(), c(x, x, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setUnion(c(y, x, x), object())._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{9, 7}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setUnion(object(), c(x, x, y))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9}));
	
	EidosAssertScriptSuccess("setUnion(T, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setUnion(F, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setUnion(F, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setUnion(T, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setUnion(7, 7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setUnion(8, 7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{8, 7}));
	EidosAssertScriptSuccess("setUnion(3.2, 3.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setUnion(2.3, 3.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.3, 3.2}));
	EidosAssertScriptSuccess("setUnion('foo', 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setUnion('bar', 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"bar", "foo"}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setUnion(x, x)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setUnion(x, y)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9}));
	
	EidosAssertScriptSuccess("setUnion(T, c(T, T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setUnion(F, c(T, T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setUnion(F, c(F, T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setUnion(T, c(F, F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setUnion(7, c(7, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setUnion(8, c(7, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8}));
	EidosAssertScriptSuccess("setUnion(8, c(7, 8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8}));
	EidosAssertScriptSuccess("setUnion(8, c(7, 9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9, 8}));
	EidosAssertScriptSuccess("setUnion(3.2, c(3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setUnion(2.3, c(3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 2.3}));
	EidosAssertScriptSuccess("setUnion(2.3, c(3.2, 2.3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 2.3}));
	EidosAssertScriptSuccess("setUnion(2.3, c(3.2, 7.6));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 7.6, 2.3}));
	EidosAssertScriptSuccess("setUnion('foo', c('foo', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setUnion('bar', c('foo', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar"}));
	EidosAssertScriptSuccess("setUnion('bar', c('foo', 'bar'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar"}));
	EidosAssertScriptSuccess("setUnion('bar', c('foo', 'baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "baz", "bar"}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setUnion(x, c(x, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setUnion(y, c(x, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setUnion(y, c(x, y))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); z = _Test(-5); setUnion(y, c(x, z))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, -5, 9}));
	
	EidosAssertScriptSuccess("setUnion(c(T, T), T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setUnion(c(T, T), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setUnion(c(F, T), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setUnion(c(F, F), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setUnion(c(7, 7), 7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setUnion(c(7, 7), 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8}));
	EidosAssertScriptSuccess("setUnion(c(7, 8), 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8}));
	EidosAssertScriptSuccess("setUnion(c(7, 9), 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9, 8}));
	EidosAssertScriptSuccess("setUnion(c(3.2, 3.2), 3.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setUnion(c(3.2, 3.2), 2.3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 2.3}));
	EidosAssertScriptSuccess("setUnion(c(3.2, 2.3), 2.3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 2.3}));
	EidosAssertScriptSuccess("setUnion(c(3.2, 7.6), 2.3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 7.6, 2.3}));
	EidosAssertScriptSuccess("setUnion(c('foo', 'foo'), 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setUnion(c('foo', 'foo'), 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar"}));
	EidosAssertScriptSuccess("setUnion(c('foo', 'bar'), 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar"}));
	EidosAssertScriptSuccess("setUnion(c('foo', 'baz'), 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "baz", "bar"}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setUnion(c(x, x), x)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setUnion(c(x, x), y)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setUnion(c(x, y), y)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); z = _Test(-5); setUnion(c(x, z), y)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, -5, 9}));
	
	EidosAssertScriptSuccess("setUnion(c(T, T, T, T), c(T, T, T, T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setUnion(c(T, T, T, T), c(T, T, T, F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setUnion(c(7, 7, 7, 7), c(7, 7, 7, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setUnion(c(7, 10, 7, 8), c(7, 9, 7, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 10, 8, 9}));
	EidosAssertScriptSuccess("setUnion(c(3.2, 3.2, 3.2, 3.2), c(3.2, 3.2, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setUnion(c(3.2, 6.0, 7.9, 3.2), c(5.5, 6.0, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 6.0, 7.9, 5.5}));
	EidosAssertScriptSuccess("setUnion(c('foo', 'foo', 'foo', 'foo'), c('foo', 'foo', 'foo', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setUnion(c('foo', 'bar', 'foo', 'foobaz'), c('foo', 'foo', 'baz', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar", "foobaz", "baz"}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setUnion(c(x, x, x, x), c(x, x, x, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); z = _Test(-5); q = _Test(26); setUnion(c(x, y, x, q), c(x, x, z, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9, 26, -5}));
	
	// setIntersection()
	EidosAssertScriptSuccess("setIntersection(NULL, NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("setIntersection(logical(0), logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(integer(0), integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(string(0), string(0));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(object(), object());", gStaticEidosValue_Object_ZeroVec);
	
	EidosAssertScriptRaise("setIntersection(NULL, logical(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setIntersection(logical(0), integer(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setIntersection(integer(0), float(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setIntersection(float(0), string(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setIntersection(string(0), object());", 0, "requires that both operands");
	EidosAssertScriptRaise("setIntersection(object(), NULL);", 0, "requires that both operands");
	
	EidosAssertScriptSuccess("setIntersection(T, logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(logical(0), T);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(F, logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(logical(0), F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(7, integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(integer(0), 7);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(3.2, float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(float(0), 3.2);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setIntersection('foo', string(0));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(string(0), 'foo');", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(_Test(7), object())._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(object(), _Test(7))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	
	EidosAssertScriptSuccess("setIntersection(c(T, T, T), logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(logical(0), c(F, F, F));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c(F, F, T), logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(logical(0), c(F, F, T));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c(7, 7, 7), integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(integer(0), c(7, 7, 7));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c(7, 8, 7), integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(integer(0), c(7, 7, 8));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c(3.2, 3.2, 3.2), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(float(0), c(3.2, 3.2, 3.2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c(4.2, 3.2, 3.2), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(float(0), c(3.2, 4.2, 3.2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c('foo', 'foo', 'foo'), string(0));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(string(0), c('foo', 'foo', 'foo'));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c('foo', 'bar', 'foo'), string(0));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(string(0), c('foo', 'foo', 'bar'));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setIntersection(c(x, x, x), object())._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setIntersection(object(), c(x, x, x))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setIntersection(c(y, x, x), object())._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setIntersection(object(), c(x, x, y))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	
	EidosAssertScriptSuccess("setIntersection(T, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setIntersection(F, T);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(F, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setIntersection(T, F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(7, 7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setIntersection(8, 7);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(3.2, 3.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setIntersection(2.3, 3.2);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setIntersection('foo', 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setIntersection('bar', 'foo');", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setIntersection(x, x)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setIntersection(x, y)._yolk;", gStaticEidosValue_Integer_ZeroVec);
	
	EidosAssertScriptSuccess("setIntersection(T, c(T, T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setIntersection(F, c(T, T));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(F, c(F, T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setIntersection(T, c(F, F));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(7, c(7, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setIntersection(8, c(7, 7));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(8, c(7, 8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(8)));
	EidosAssertScriptSuccess("setIntersection(8, c(7, 9));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(3.2, c(3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setIntersection(2.3, c(3.2, 3.2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(2.3, c(3.2, 2.3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.3)));
	EidosAssertScriptSuccess("setIntersection(2.3, c(3.2, 7.6));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setIntersection('foo', c('foo', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setIntersection('bar', c('foo', 'foo'));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setIntersection('bar', c('foo', 'bar'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptSuccess("setIntersection('bar', c('foo', 'baz'));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setIntersection(x, c(x, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setIntersection(y, c(x, x))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setIntersection(y, c(x, y))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(9)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); z = _Test(-5); setIntersection(y, c(x, z))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	
	EidosAssertScriptSuccess("setIntersection(c(T, T), T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setIntersection(c(T, T), F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c(F, T), F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setIntersection(c(F, F), T);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c(7, 7), 7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setIntersection(c(7, 7), 8);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c(7, 8), 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(8)));
	EidosAssertScriptSuccess("setIntersection(c(7, 9), 8);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c(3.2, 3.2), 3.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setIntersection(c(3.2, 3.2), 2.3);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c(3.2, 2.3), 2.3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.3)));
	EidosAssertScriptSuccess("setIntersection(c(3.2, 7.6), 2.3);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c('foo', 'foo'), 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setIntersection(c('foo', 'foo'), 'bar');", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setIntersection(c('foo', 'bar'), 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptSuccess("setIntersection(c('foo', 'baz'), 'bar');", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setIntersection(c(x, x), x)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setIntersection(c(x, x), y)._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setIntersection(c(x, y), y)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(9)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); z = _Test(-5); setIntersection(c(x, z), y)._yolk;", gStaticEidosValue_Integer_ZeroVec);
	
	EidosAssertScriptSuccess("setIntersection(c(T, T, T, T), c(T, T, T, T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setIntersection(c(T, T, T, T), c(T, T, T, F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setIntersection(c(T, T, F, T), c(T, T, T, F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setIntersection(c(7, 7, 7, 7), c(7, 7, 7, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setIntersection(c(7, 10, 7, 8), c(7, 9, 7, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setIntersection(c(7, 10, 7, 8), c(7, 9, 8, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8}));
	EidosAssertScriptSuccess("setIntersection(c(3.2, 3.2, 3.2, 3.2), c(3.2, 3.2, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setIntersection(c(3.2, 6.0, 7.9, 3.2), c(5.5, 1.3, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setIntersection(c(3.2, 6.0, 7.9, 3.2), c(5.5, 6.0, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 6.0}));
	EidosAssertScriptSuccess("setIntersection(c('foo', 'foo', 'foo', 'foo'), c('foo', 'foo', 'foo', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setIntersection(c('foo', 'bar', 'foo', 'foobaz'), c('foo', 'foo', 'baz', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setIntersection(c('foo', 'bar', 'foo', 'foobaz'), c('bar', 'foo', 'baz', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar"}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setIntersection(c(x, x, x, x), c(x, x, x, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); z = _Test(-5); q = _Test(26); setIntersection(c(x, y, x, q), c(x, x, z, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); z = _Test(-5); q = _Test(26); setIntersection(c(x, y, x, q), c(y, x, z, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9}));
}

void _RunFunctionMathTests_setDifferenceSymmetricDifference(void)
{
	// setDifference()
	EidosAssertScriptSuccess("setDifference(NULL, NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("setDifference(logical(0), logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setDifference(integer(0), integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setDifference(float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setDifference(string(0), string(0));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setDifference(object(), object());", gStaticEidosValue_Object_ZeroVec);
	
	EidosAssertScriptRaise("setDifference(NULL, logical(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setDifference(logical(0), integer(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setDifference(integer(0), float(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setDifference(float(0), string(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setDifference(string(0), object());", 0, "requires that both operands");
	EidosAssertScriptRaise("setDifference(object(), NULL);", 0, "requires that both operands");
	
	EidosAssertScriptSuccess("setDifference(T, logical(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setDifference(logical(0), T);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setDifference(F, logical(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setDifference(logical(0), F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setDifference(7, integer(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setDifference(integer(0), 7);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setDifference(3.2, float(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setDifference(float(0), 3.2);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setDifference('foo', string(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setDifference(string(0), 'foo');", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setDifference(_Test(7), object())._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setDifference(object(), _Test(7))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	
	EidosAssertScriptSuccess("setDifference(c(T, T, T), logical(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setDifference(logical(0), c(F, F, F));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(F, F, T), logical(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setDifference(logical(0), c(F, F, T));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(7, 7, 7), integer(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setDifference(integer(0), c(7, 7, 7));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(7, 8, 7), integer(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8}));
	EidosAssertScriptSuccess("setDifference(integer(0), c(7, 7, 8));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(3.2, 3.2, 3.2), float(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setDifference(float(0), c(3.2, 3.2, 3.2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(4.2, 3.2, 3.2), float(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{4.2, 3.2}));
	EidosAssertScriptSuccess("setDifference(float(0), c(3.2, 4.2, 3.2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c('foo', 'foo', 'foo'), string(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setDifference(string(0), c('foo', 'foo', 'foo'));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c('foo', 'bar', 'foo'), string(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar"}));
	EidosAssertScriptSuccess("setDifference(string(0), c('foo', 'foo', 'bar'));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setDifference(c(x, x, x), object())._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setDifference(object(), c(x, x, x))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setDifference(c(y, x, x), object())._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{9, 7}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setDifference(object(), c(x, x, y))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	
	EidosAssertScriptSuccess("setDifference(T, T);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setDifference(F, T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setDifference(F, F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setDifference(T, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setDifference(7, 7);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setDifference(8, 7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(8)));
	EidosAssertScriptSuccess("setDifference(3.2, 3.2);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setDifference(2.3, 3.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.3)));
	EidosAssertScriptSuccess("setDifference('foo', 'foo');", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setDifference('bar', 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setDifference(x, x)._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setDifference(x, y)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	
	EidosAssertScriptSuccess("setDifference(T, c(T, T));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setDifference(F, c(T, T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setDifference(F, c(F, T));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setDifference(T, c(F, F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setDifference(7, c(7, 7));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setDifference(8, c(7, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(8)));
	EidosAssertScriptSuccess("setDifference(8, c(7, 8));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setDifference(8, c(7, 9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(8)));
	EidosAssertScriptSuccess("setDifference(3.2, c(3.2, 3.2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setDifference(2.3, c(3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.3)));
	EidosAssertScriptSuccess("setDifference(2.3, c(3.2, 2.3));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setDifference(2.3, c(3.2, 7.6));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.3)));
	EidosAssertScriptSuccess("setDifference('foo', c('foo', 'foo'));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setDifference('bar', c('foo', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptSuccess("setDifference('bar', c('foo', 'bar'));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setDifference('bar', c('foo', 'baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setDifference(x, c(x, x))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setDifference(y, c(x, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(9)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setDifference(y, c(x, y))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); z = _Test(-5); setDifference(y, c(x, z))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(9)));
	
	EidosAssertScriptSuccess("setDifference(c(T, T), T);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(T, T), F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setDifference(c(F, T), F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setDifference(c(F, F), T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setDifference(c(7, 7), 7);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(7, 7), 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setDifference(c(7, 8), 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setDifference(c(7, 9), 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9}));
	EidosAssertScriptSuccess("setDifference(c(3.2, 3.2), 3.2);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(3.2, 3.2), 2.3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setDifference(c(3.2, 2.3), 2.3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setDifference(c(3.2, 7.6), 2.3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 7.6}));
	EidosAssertScriptSuccess("setDifference(c('foo', 'foo'), 'foo');", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c('foo', 'foo'), 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setDifference(c('foo', 'bar'), 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setDifference(c('foo', 'baz'), 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "baz"}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setDifference(c(x, x), x)._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setDifference(c(x, x), y)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setDifference(c(x, y), y)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); z = _Test(-5); setDifference(c(x, z), y)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, -5}));
	
	EidosAssertScriptSuccess("setDifference(c(T, T, T, T), c(T, T, T, T));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(T, T, T, T), c(T, T, T, F));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(T, T, F, F), c(T, T, T, F));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(7, 7, 7, 7), c(7, 7, 7, 7));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(7, 10, 7, 10, 8), c(7, 9, 7, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 8}));
	EidosAssertScriptSuccess("setDifference(c(3.2, 3.2, 3.2, 3.2), c(3.2, 3.2, 3.2, 3.2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(3.2, 6.0, 7.9, 3.2, 7.9), c(5.5, 6.0, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(7.9)));
	EidosAssertScriptSuccess("setDifference(c('foo', 'foo', 'foo', 'foo'), c('foo', 'foo', 'foo', 'foo'));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c('foo', 'bar', 'foobaz', 'foo', 'foobaz'), c('foo', 'foo', 'baz', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"bar", "foobaz"}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setDifference(c(x, x, x, x), c(x, x, x, x))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); z = _Test(-5); q = _Test(26); setDifference(c(x, y, q, x, q), c(x, x, z, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{9, 26}));
	
	// setSymmetricDifference()
	EidosAssertScriptSuccess("setSymmetricDifference(NULL, NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("setSymmetricDifference(logical(0), logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(integer(0), integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(string(0), string(0));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(object(), object());", gStaticEidosValue_Object_ZeroVec);
	EidosAssertScriptSuccess("size(setSymmetricDifference(_Test(7)[F], object()));", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("size(setSymmetricDifference(object(), _Test(7)[F]));", gStaticEidosValue_Integer0);
	
	EidosAssertScriptRaise("setSymmetricDifference(NULL, logical(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setSymmetricDifference(logical(0), integer(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setSymmetricDifference(integer(0), float(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setSymmetricDifference(float(0), string(0));", 0, "requires that both operands");
	EidosAssertScriptRaise("setSymmetricDifference(string(0), object());", 0, "requires that both operands");
	EidosAssertScriptRaise("setSymmetricDifference(object(), NULL);", 0, "requires that both operands");
	
	EidosAssertScriptSuccess("setSymmetricDifference(T, logical(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSymmetricDifference(logical(0), T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSymmetricDifference(F, logical(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setSymmetricDifference(logical(0), F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setSymmetricDifference(7, integer(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setSymmetricDifference(integer(0), 7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setSymmetricDifference(3.2, float(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setSymmetricDifference(float(0), 3.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setSymmetricDifference('foo', string(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setSymmetricDifference(string(0), 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setSymmetricDifference(_Test(7), object())._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setSymmetricDifference(object(), _Test(7))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	
	EidosAssertScriptSuccess("setSymmetricDifference(c(T, T, T), logical(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSymmetricDifference(logical(0), c(F, F, F));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setSymmetricDifference(c(F, F, T), logical(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setSymmetricDifference(logical(0), c(F, F, T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setSymmetricDifference(c(7, 7, 7), integer(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setSymmetricDifference(integer(0), c(7, 7, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setSymmetricDifference(c(7, 8, 7), integer(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8}));
	EidosAssertScriptSuccess("setSymmetricDifference(integer(0), c(7, 7, 8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8}));
	EidosAssertScriptSuccess("setSymmetricDifference(c(3.2, 3.2, 3.2), float(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setSymmetricDifference(float(0), c(3.2, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setSymmetricDifference(c(4.2, 3.2, 3.2), float(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{4.2, 3.2}));
	EidosAssertScriptSuccess("setSymmetricDifference(float(0), c(3.2, 4.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 4.2}));
	EidosAssertScriptSuccess("setSymmetricDifference(c('foo', 'foo', 'foo'), string(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setSymmetricDifference(string(0), c('foo', 'foo', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setSymmetricDifference(c('foo', 'bar', 'foo'), string(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar"}));
	EidosAssertScriptSuccess("setSymmetricDifference(string(0), c('foo', 'foo', 'bar'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar"}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setSymmetricDifference(c(x, x, x), object())._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setSymmetricDifference(object(), c(x, x, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setSymmetricDifference(c(y, x, x), object())._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{9, 7}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setSymmetricDifference(object(), c(x, x, y))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9}));
	
	EidosAssertScriptSuccess("setSymmetricDifference(T, T);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(F, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setSymmetricDifference(F, F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(T, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setSymmetricDifference(7, 7);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(8, 7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{8, 7}));
	EidosAssertScriptSuccess("setSymmetricDifference(3.2, 3.2);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(2.3, 3.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.3, 3.2}));
	EidosAssertScriptSuccess("setSymmetricDifference('foo', 'foo');", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference('bar', 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"bar", "foo"}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setSymmetricDifference(x, x)._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setSymmetricDifference(x, y)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9}));
	
	EidosAssertScriptSuccess("setSymmetricDifference(T, c(T, T));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(F, c(T, T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setSymmetricDifference(F, c(F, T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSymmetricDifference(T, c(F, F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setSymmetricDifference(7, c(7, 7));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(8, c(7, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8}));
	EidosAssertScriptSuccess("setSymmetricDifference(8, c(7, 8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setSymmetricDifference(8, c(7, 9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9, 8}));
	EidosAssertScriptSuccess("setSymmetricDifference(3.2, c(3.2, 3.2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(2.3, c(3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 2.3}));
	EidosAssertScriptSuccess("setSymmetricDifference(2.3, c(3.2, 2.3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setSymmetricDifference(2.3, c(3.2, 7.6));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 7.6, 2.3}));
	EidosAssertScriptSuccess("setSymmetricDifference('foo', c('foo', 'foo'));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference('bar', c('foo', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar"}));
	EidosAssertScriptSuccess("setSymmetricDifference('bar', c('foo', 'bar'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setSymmetricDifference('bar', c('foo', 'baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "baz", "bar"}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setSymmetricDifference(x, c(x, x))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setSymmetricDifference(y, c(x, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setSymmetricDifference(y, c(x, y))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); z = _Test(-5); setSymmetricDifference(y, c(x, z))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, -5, 9}));
	
	EidosAssertScriptSuccess("setSymmetricDifference(c(T, T), T);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(c(T, T), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setSymmetricDifference(c(F, T), F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSymmetricDifference(c(F, F), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("setSymmetricDifference(c(7, 7), 7);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(c(7, 7), 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8}));
	EidosAssertScriptSuccess("setSymmetricDifference(c(7, 8), 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("setSymmetricDifference(c(7, 9), 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9, 8}));
	EidosAssertScriptSuccess("setSymmetricDifference(c(3.2, 3.2), 3.2);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(c(3.2, 3.2), 2.3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 2.3}));
	EidosAssertScriptSuccess("setSymmetricDifference(c(3.2, 2.3), 2.3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setSymmetricDifference(c(3.2, 7.6), 2.3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 7.6, 2.3}));
	EidosAssertScriptSuccess("setSymmetricDifference(c('foo', 'foo'), 'foo');", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(c('foo', 'foo'), 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar"}));
	EidosAssertScriptSuccess("setSymmetricDifference(c('foo', 'bar'), 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("setSymmetricDifference(c('foo', 'baz'), 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "baz", "bar"}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setSymmetricDifference(c(x, x), x)._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setSymmetricDifference(c(x, x), y)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 9}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setSymmetricDifference(c(x, y), y)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); z = _Test(-5); setSymmetricDifference(c(x, z), y)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, -5, 9}));
	
	EidosAssertScriptSuccess("setSymmetricDifference(c(T, T, T, T), c(T, T, T, T));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(c(T, T, T, T), c(T, T, T, F));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("setSymmetricDifference(c(T, T, F, T), c(T, T, T, F));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(c(7, 7, 7, 7), c(7, 7, 7, 7));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(c(7, 10, 7, 10, 8), c(7, 9, 7, 9, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 8, 9}));
	EidosAssertScriptSuccess("setSymmetricDifference(c(3.2, 3.2, 3.2, 3.2), c(3.2, 3.2, 3.2, 3.2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(c(7.3, 10.5, 7.3, 10.5, 8.9), c(7.3, 9.7, 7.3, 9.7, 7.3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.5, 8.9, 9.7}));
	EidosAssertScriptSuccess("setSymmetricDifference(c('foo', 'foo', 'foo', 'foo'), c('foo', 'foo', 'foo', 'foo'));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(c('foo', 'bar', 'foo', 'bar', 'foobaz'), c('foo', 'baz', 'foo', 'baz', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"bar", "foobaz", "baz"}));
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); setSymmetricDifference(c(x, x, x, x), c(x, x, x, x))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = _Test(7); y = _Test(9); z = _Test(-5); q = _Test(26); setSymmetricDifference(c(x, y, x, y, z), c(x, q, x, q, x))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{9, -5, 26}));
}

void _RunFunctionMathTests_s_through_z(void)
{
	// sin()
	EidosAssertScriptSuccess("abs(sin(0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(sin(0.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(sin(PI/2) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(sin(c(0, PI/2, PI)) - c(0, 1, 0))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("sin(T);", 0, "cannot be type");
	EidosAssertScriptRaise("sin('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sin(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("sin(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("sin(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("sin(integer(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("sin(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("sin(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("sin(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(sin(matrix(0.5)), matrix(sin(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sin(matrix(c(0.1, 0.2, 0.3))), matrix(sin(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// sqrt()
	EidosAssertScriptSuccess("sqrt(64);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(8)));
	EidosAssertScriptSuccess("isNAN(sqrt(-64));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sqrt(c(4, -16, 9, 1024));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2, NAN, 3, 32}));
	EidosAssertScriptSuccess("sqrt(64.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(8)));
	EidosAssertScriptSuccess("isNAN(sqrt(-64.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sqrt(c(4.0, -16.0, 9.0, 1024.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2, NAN, 3, 32}));
	EidosAssertScriptRaise("sqrt(T);", 0, "cannot be type");
	EidosAssertScriptRaise("sqrt('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sqrt(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("sqrt(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("sqrt(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("sqrt(integer(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("sqrt(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("sqrt(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("sqrt(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(sqrt(matrix(0.5)), matrix(sqrt(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sqrt(matrix(c(0.1, 0.2, 0.3))), matrix(sqrt(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// sum()
	EidosAssertScriptSuccess("sum(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("sum(-5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-5)));
	EidosAssertScriptSuccess("sum(c(-2, 7, -18, 12));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("sum(c(200000000, 3000000000000));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3000200000000)));
#if EIDOS_HAS_OVERFLOW_BUILTINS
	EidosAssertScriptSuccess("sum(rep(3000000000000000000, 100));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3e20)));
#endif
	EidosAssertScriptSuccess("sum(5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5.5)));
	EidosAssertScriptSuccess("sum(-5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-5.5)));
	EidosAssertScriptSuccess("sum(c(-2.5, 7.5, -18.5, 12.5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-1)));
	EidosAssertScriptSuccess("sum(T);", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("sum(c(T,F,T,F,T,T,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptRaise("sum('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sum(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("sum(NULL);", 0, "cannot be type");
	EidosAssertScriptSuccess("sum(logical(0));", gStaticEidosValue_Integer0);	// sum of no elements is 0 (as in R)
	EidosAssertScriptSuccess("sum(integer(0));", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("sum(float(0));", gStaticEidosValue_Float0);
	EidosAssertScriptRaise("sum(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("sum(c(5.0, 2.0, NAN, 2.0));", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("sum(matrix(5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("sum(matrix(c(5, -5)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0)));
	EidosAssertScriptSuccess("sum(array(c(5, -5, 3), c(1,3,1)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	
	// sumExact()
	EidosAssertScriptSuccess("sumExact(5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5.5)));
	EidosAssertScriptSuccess("sumExact(-5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-5.5)));
	EidosAssertScriptSuccess("sumExact(c(-2.5, 7.5, -18.5, 12.5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-1)));
	EidosAssertScriptRaise("sumExact(T);", 0, "cannot be type");
	EidosAssertScriptRaise("sumExact(1);", 0, "cannot be type");
	EidosAssertScriptRaise("sumExact('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sumExact(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("sumExact(NULL);", 0, "cannot be type");
	EidosAssertScriptSuccess("sumExact(float(0));", gStaticEidosValue_Float0);
	EidosAssertScriptSuccess("v = c(1, 1.0e100, 1, -1.0e100); v = rep(v, 10000); sumExact(v);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(20000)));
	EidosAssertScriptSuccess("v = c(-1, 1.0e100, -1, -1.0e100); v = rep(v, 10000); sumExact(v);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-20000)));
	EidosAssertScriptSuccess("v = c(-1, 1.0e100, 1, -1.0e100); v = rep(v, 10000); sumExact(v);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0)));
	EidosAssertScriptSuccess("sumExact(c(5.0, 2.0, NAN, 2.0));", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("sumExact(matrix(5.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5.0)));
	EidosAssertScriptSuccess("sumExact(matrix(c(5.0, -5)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0.0)));
	EidosAssertScriptSuccess("sumExact(array(c(5.0, -5, 3), c(1,3,1)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.0)));
	
	// tan()
	EidosAssertScriptSuccess("abs(tan(0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(tan(0.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(tan(PI/4) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(tan(c(0, PI/4, -PI/4)) - c(0, 1, -1))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("tan(T);", 0, "cannot be type");
	EidosAssertScriptRaise("tan('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("tan(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("tan(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("tan(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("tan(integer(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("tan(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("tan(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("tan(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(tan(matrix(0.5)), matrix(tan(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(tan(matrix(c(0.1, 0.2, 0.3))), matrix(tan(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// trunc()
	EidosAssertScriptSuccess("trunc(5.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5.0)));
	EidosAssertScriptSuccess("trunc(-5.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-5.0)));
	EidosAssertScriptSuccess("trunc(c(-2.1, 7.1, -18.8, 12.8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-2.0, 7, -18, 12}));
	EidosAssertScriptRaise("trunc(T);", 0, "cannot be type");
	EidosAssertScriptRaise("trunc(5);", 0, "cannot be type");
	EidosAssertScriptRaise("trunc('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("trunc(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("trunc(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("trunc(logical(0));", 0, "cannot be type");
	EidosAssertScriptRaise("trunc(integer(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("trunc(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("trunc(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("trunc(NAN);", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("identical(trunc(matrix(0.3)), matrix(trunc(0.3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(trunc(matrix(0.6)), matrix(trunc(0.6)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(trunc(matrix(-0.3)), matrix(trunc(-0.3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(trunc(matrix(-0.6)), matrix(trunc(-0.6)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(trunc(matrix(c(0.1, 5.7, -0.3))), matrix(trunc(c(0.1, 5.7, -0.3))));", gStaticEidosValue_LogicalT);
}

#pragma mark statistics
void _RunFunctionStatisticsTests(void)
{
	// cor()
	EidosAssertScriptRaise("cor(T, T);", 0, "cannot be type");
	EidosAssertScriptSuccess("cor(3, 3);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cor(3.5, 3.5);", gStaticEidosValueNULL);
	EidosAssertScriptRaise("cor('foo', 'foo');", 0, "cannot be type");
	EidosAssertScriptRaise("cor(c(F, F, T, F, T), c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess("abs(cor(1:5, 1:5) - 1) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("cor(1:5, 1:4);", 0, "the same size");
	EidosAssertScriptSuccess("abs(cor(1:11, 1:11) - 1) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cor(1:5, 5:1) - -1) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cor(1:11, 11:1) - -1) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cor(1.0:5, 1:5) - 1) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cor(1:11, 1.0:11) - 1) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cor(1.0:5, 5.0:1) - -1) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cor(1.0:11, 11.0:1) - -1) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("cor(c(1.0, 2.0, NAN), c(8.0, 9.0, NAN));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptRaise("cor(c('foo', 'bar', 'baz'), c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("cor(_Test(7), _Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("cor(NULL, NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("cor(logical(0), logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("cor(integer(0), integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cor(float(0), float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("cor(string(0), string(0));", 0, "cannot be type");
	
	// cov()
	EidosAssertScriptRaise("cov(T, T);", 0, "cannot be type");
	EidosAssertScriptSuccess("cov(3, 3);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cov(3.5, 3.5);", gStaticEidosValueNULL);
	EidosAssertScriptRaise("cov('foo', 'foo');", 0, "cannot be type");
	EidosAssertScriptRaise("cov(c(F, F, T, F, T), c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess("abs(cov(1:5, 1:5) - 2.5) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("cov(1:5, 1:4);", 0, "the same size");
	EidosAssertScriptSuccess("abs(cov(1:11, 1:11) - 11) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cov(1:5, 5:1) - -2.5) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cov(1:11, 11:1) - -11) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cov(1.0:5, 1:5) - 2.5) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cov(1:11, 1.0:11) - 11) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cov(1.0:5, 5.0:1) - -2.5) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(cov(1.0:11, 11.0:1) - -11) < 1e-10;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("cov(c(1.0, 2.0, NAN), c(8.0, 9.0, NAN));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptRaise("cov(c('foo', 'bar', 'baz'), c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("cov(_Test(7), _Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("cov(NULL, NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("cov(logical(0), logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("cov(integer(0), integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cov(float(0), float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("cov(string(0), string(0));", 0, "cannot be type");
	
	// max()
	EidosAssertScriptSuccess("max(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("max(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("max(3.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.5)));
	EidosAssertScriptSuccess("max('foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("max(c(F, F, F, F, F));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("max(c(F, F, T, F, T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("max(c(3, 7, 19, -5, 9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(19)));
	EidosAssertScriptSuccess("max(c(3.3, 7.7, 19.1, -5.8, 9.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(19.1)));
	EidosAssertScriptSuccess("max(c('bar', 'foo', 'baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptRaise("max(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("max(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("max(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("max(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("max(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("max(string(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("max(c(1.0, 5.0, NAN, 2.0));", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("max(F, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("max(T, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("max(F, c(F,F), logical(0), c(F,F,F,F,F));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("max(F, c(F,F), logical(0), c(T,F,F,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("max(1, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("max(2, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("max(integer(0), c(3,7,-8,0), 0, c(-10,10));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("max(1.0, 2.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.0)));
	EidosAssertScriptSuccess("max(2.0, 1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.0)));
	EidosAssertScriptSuccess("max(c(3.,7.,-8.,0.), 0., c(-10.,0.), float(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(7)));
	EidosAssertScriptRaise("max(c(3,7,-8,0), c(-10.,10.));", 0, "the same type");
	EidosAssertScriptSuccess("max('foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("max('bar', 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("max('foo', string(0), c('baz','bar'), 'xyzzy', c('foobar', 'barbaz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("xyzzy")));
	
	// mean()
	EidosAssertScriptSuccess("mean(T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
	EidosAssertScriptSuccess("mean(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3)));
	EidosAssertScriptSuccess("mean(3.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.5)));
	EidosAssertScriptRaise("mean('foo');", 0, "cannot be type");
	EidosAssertScriptSuccess("mean(c(F, F, T, F, T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0.4)));
	EidosAssertScriptSuccess("mean(c(3, 7, 19, -5, 16));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(8)));
	EidosAssertScriptSuccess("mean(c(3.3, 7.2, 19.1, -5.6, 16.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(8.0)));
	EidosAssertScriptRaise("mean(c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("mean(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("mean(NULL);", 0, "cannot be type");
	EidosAssertScriptSuccess("mean(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("mean(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("mean(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("mean(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("mean(rep(1e18, 9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1e18)));	// stays in integer internally
	EidosAssertScriptSuccess("mean(rep(1e18, 10));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1e18)));	// overflows to float internally
	EidosAssertScriptSuccess("mean(c(1.0, 5.0, NAN, 2.0));", gStaticEidosValue_FloatNAN);
	
	// min()
	EidosAssertScriptSuccess("min(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("min(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("min(3.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.5)));
	EidosAssertScriptSuccess("min('foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("min(c(T, F, T, F, T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("min(c(3, 7, 19, -5, 9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-5)));
	EidosAssertScriptSuccess("min(c(3.3, 7.7, 19.1, -5.8, 9.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-5.8)));
	EidosAssertScriptSuccess("min(c('foo', 'bar', 'baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptRaise("min(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("min(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("min(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("min(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("min(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("min(string(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("min(c(1.0, 5.0, NAN, 2.0));", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess("min(T, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("min(F, T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("min(T, c(T,T), logical(0), c(T,T,T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("min(F, c(T,T), logical(0), c(T,T,T,T,T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("min(1, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(1)));
	EidosAssertScriptSuccess("min(2, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(1)));
	EidosAssertScriptSuccess("min(integer(0), c(3,7,-8,0), 0, c(-10,10));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-10)));
	EidosAssertScriptSuccess("min(1.0, 2.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1.0)));
	EidosAssertScriptSuccess("min(2.0, 1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1.0)));
	EidosAssertScriptSuccess("min(c(3.,7.,-8.,0.), 0., c(0.,10.), float(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-8)));
	EidosAssertScriptRaise("min(c(3,7,-8,0), c(-10.,10.));", 0, "the same type");
	EidosAssertScriptSuccess("min('foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptSuccess("min('bar', 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptSuccess("min('foo', string(0), c('baz','bar'), 'xyzzy', c('foobar', 'barbaz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	
	// pmax()
	EidosAssertScriptRaise("pmax(c(T,T), logical(0));", 0, "of equal length");
	EidosAssertScriptRaise("pmax(logical(0), c(F,F));", 0, "of equal length");
	EidosAssertScriptSuccess("pmax(T, logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("pmax(logical(0), F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptRaise("pmax(T, 1);", 0, "to be the same type");
	EidosAssertScriptRaise("pmax(0, F);", 0, "to be the same type");
	EidosAssertScriptSuccess("pmax(NULL, NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("pmax(T, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("pmax(F, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("pmax(T, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("pmax(F, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("pmax(c(T,F,T,F), c(T,T,F,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, false}));
	EidosAssertScriptSuccess("pmax(1, 5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("pmax(-8, 6);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(6)));
	EidosAssertScriptSuccess("pmax(7, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("pmax(8, -8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(8)));
	EidosAssertScriptSuccess("pmax(c(1,-8,7,8), c(5,6,1,-8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 6, 7, 8}));
	EidosAssertScriptSuccess("pmax(1., 5.);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5)));
	EidosAssertScriptSuccess("pmax(-INF, 6.);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(6)));
	EidosAssertScriptSuccess("pmax(7., 1.);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(7)));
	EidosAssertScriptSuccess("pmax(INF, -8.);", gStaticEidosValue_FloatINF);
	EidosAssertScriptSuccess("pmax(NAN, -8.);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmax(NAN, INF);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmax(c(1.,-INF,7.,INF, NAN, NAN), c(5.,6.,1.,-8.,-8.,INF));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5, 6, 7, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("pmax('foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("pmax('bar', 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("baz")));
	EidosAssertScriptSuccess("pmax('xyzzy', 'xyzzy');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("xyzzy")));
	EidosAssertScriptSuccess("pmax('', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptSuccess("pmax(c('foo','bar','xyzzy',''), c('bar','baz','xyzzy','bar'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "baz", "xyzzy", "bar"}));
	
	EidosAssertScriptSuccess("pmax(F, c(T,T,F,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, false, false}));
	EidosAssertScriptSuccess("pmax(c(T,F,T,F), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("pmax(4, c(5,6,1,-8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 6, 4, 4}));
	EidosAssertScriptSuccess("pmax(c(1,-8,7,8), -2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -2, 7, 8}));
	EidosAssertScriptSuccess("pmax(4., c(5.,6.,1.,-8.,-8.,INF));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5, 6, 4, 4, 4, std::numeric_limits<double>::infinity()}));
	EidosAssertScriptSuccess("pmax(c(1.,-INF,7.,INF, NAN, NAN), 5.);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5, 5, 7, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("pmax('baz', c('bar','baz','xyzzy','bar'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"baz", "baz", "xyzzy", "baz"}));
	EidosAssertScriptSuccess("pmax(c('foo','bar','xyzzy',''), 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "baz", "xyzzy", "baz"}));
	
	EidosAssertScriptSuccess("identical(pmax(5, 3:7), c(5,5,5,6,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(pmax(3:7, 5), c(5,5,5,6,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(pmax(matrix(5), 3:7), c(5,5,5,6,7));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(3:7, matrix(5)), c(5,5,5,6,7));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(array(5, c(1,1,1)), 3:7), c(5,5,5,6,7));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(3:7, array(5, c(1,1,1))), c(5,5,5,6,7));", 10, "the singleton is a vector");
	EidosAssertScriptSuccess("identical(pmax(5, matrix(3:7)), matrix(c(5,5,5,6,7)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(pmax(matrix(3:7), 5), matrix(c(5,5,5,6,7)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(pmax(5, array(3:7, c(1,5,1))), array(c(5,5,5,6,7), c(1,5,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(pmax(array(3:7, c(1,5,1)), 5), array(c(5,5,5,6,7), c(1,5,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(pmax(1:5, matrix(3:7)), matrix(c(5,5,5,6,7)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmax(matrix(3:7), 1:5), matrix(c(5,5,5,6,7)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmax(1:5, array(3:7, c(1,5,1))), array(c(5,5,5,6,7), c(1,5,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmax(array(3:7, c(1,5,1)), 1:5), array(c(5,5,5,6,7), c(1,5,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmax(matrix(5), matrix(3:7)), matrix(c(5,5,5,6,7)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(matrix(3:7), matrix(5)), matrix(c(5,5,5,6,7)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(matrix(5), array(3:7, c(1,5,1))), array(c(5,5,5,6,7), c(1,5,1)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(array(3:7, c(1,5,1)), matrix(5)), array(c(5,5,5,6,7), c(1,5,1)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(matrix(5:1, nrow=1), matrix(1:5, ncol=1)), matrix(c(5,4,3,4,5)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptSuccess("identical(pmax(matrix(5:1, nrow=1), matrix(1:5, nrow=1)), matrix(c(5,4,3,4,5), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(pmax(matrix(1:5), array(3:7, c(1,5,1))), array(c(5,5,5,6,7), c(1,5,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptSuccess("identical(pmax(array(5:1, c(1,5,1)), array(1:5, c(1,5,1))), array(c(5,4,3,4,5), c(1,5,1)));", gStaticEidosValue_LogicalT);
	
	// pmin()
	EidosAssertScriptRaise("pmin(c(T,T), logical(0));", 0, "of equal length");
	EidosAssertScriptRaise("pmin(logical(0), c(F,F));", 0, "of equal length");
	EidosAssertScriptSuccess("pmin(T, logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("pmin(logical(0), F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptRaise("pmin(T, 1);", 0, "to be the same type");
	EidosAssertScriptRaise("pmin(0, F);", 0, "to be the same type");
	EidosAssertScriptSuccess("pmin(NULL, NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("pmin(T, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("pmin(F, T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("pmin(T, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("pmin(F, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("pmin(c(T,F,T,F), c(T,T,F,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, false}));
	EidosAssertScriptSuccess("pmin(1, 5);", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("pmin(-8, 6);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-8)));
	EidosAssertScriptSuccess("pmin(7, 1);", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("pmin(8, -8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-8)));
	EidosAssertScriptSuccess("pmin(c(1,-8,7,8), c(5,6,1,-8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -8, 1, -8}));
	EidosAssertScriptSuccess("pmin(1., 5.);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
	EidosAssertScriptSuccess("pmin(-INF, 6.);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-std::numeric_limits<double>::infinity())));
	EidosAssertScriptSuccess("pmin(7., 1.);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
	EidosAssertScriptSuccess("pmin(INF, -8.);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-8)));
	EidosAssertScriptSuccess("pmin(NAN, -8.);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmin(NAN, INF);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmin(c(1.,-INF,7.,INF, NAN, NAN), c(5.,6.,1.,-8.,-8.,INF));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, -std::numeric_limits<double>::infinity(), 1, -8, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("pmin('foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptSuccess("pmin('bar', 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptSuccess("pmin('xyzzy', 'xyzzy');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("xyzzy")));
	EidosAssertScriptSuccess("pmin('', 'bar');", gStaticEidosValue_StringEmpty);
	EidosAssertScriptSuccess("pmin(c('foo','bar','xyzzy',''), c('bar','baz','xyzzy','bar'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"bar", "bar", "xyzzy", ""}));
	
	EidosAssertScriptSuccess("pmin(F, c(T,T,F,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("pmin(c(T,F,T,F), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("pmin(4, c(5,6,1,-8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 4, 1, -8}));
	EidosAssertScriptSuccess("pmin(c(1,-8,7,8), -2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-2, -8, -2, -2}));
	EidosAssertScriptSuccess("pmin(4., c(5.,6.,1.,-8.,-8.,INF));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{4, 4, 1, -8, -8, 4}));
	EidosAssertScriptSuccess("pmin(c(1.,-INF,7.,INF, NAN, NAN), 5.);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, -std::numeric_limits<double>::infinity(), 5, 5, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("pmin('baz', c('bar','baz','xyzzy','bar'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"bar", "baz", "baz", "bar"}));
	EidosAssertScriptSuccess("pmin(c('foo','bar','xyzzy',''), 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"baz", "bar", "baz", ""}));
	
	EidosAssertScriptSuccess("identical(pmin(5, 3:7), c(3,4,5,5,5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(pmin(3:7, 5), c(3,4,5,5,5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(pmin(matrix(5), 3:7), c(3,4,5,5,5));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(3:7, matrix(5)), c(3,4,5,5,5));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(array(5, c(1,1,1)), 3:7), c(3,4,5,5,5));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(3:7, array(5, c(1,1,1))), c(3,4,5,5,5));", 10, "the singleton is a vector");
	EidosAssertScriptSuccess("identical(pmin(5, matrix(3:7)), matrix(c(3,4,5,5,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(pmin(matrix(3:7), 5), matrix(c(3,4,5,5,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(pmin(5, array(3:7, c(1,5,1))), array(c(3,4,5,5,5), c(1,5,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(pmin(array(3:7, c(1,5,1)), 5), array(c(3,4,5,5,5), c(1,5,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(pmin(1:5, matrix(3:7)), matrix(c(3,4,5,5,5)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmin(matrix(3:7), 1:5), matrix(c(3,4,5,5,5)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmin(1:5, array(3:7, c(1,5,1))), array(c(3,4,5,5,5), c(1,5,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmin(array(3:7, c(1,5,1)), 1:5), array(c(3,4,5,5,5), c(1,5,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmin(matrix(5), matrix(3:7)), matrix(c(3,4,5,5,5)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(matrix(3:7), matrix(5)), matrix(c(3,4,5,5,5)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(matrix(5), array(3:7, c(1,5,1))), array(c(3,4,5,5,5), c(1,5,1)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(array(3:7, c(1,5,1)), matrix(5)), array(c(3,4,5,5,5), c(1,5,1)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(matrix(5:1, nrow=1), matrix(1:5, ncol=1)), matrix(c(1,2,3,2,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptSuccess("identical(pmin(matrix(5:1, nrow=1), matrix(1:5, nrow=1)), matrix(c(1,2,3,2,1), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(pmin(matrix(1:5), array(3:7, c(1,5,1))), array(c(3,4,5,5,5), c(1,5,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptSuccess("identical(pmin(array(5:1, c(1,5,1)), array(1:5, c(1,5,1))), array(c(1,2,3,2,1), c(1,5,1)));", gStaticEidosValue_LogicalT);
	
	// range()
	EidosAssertScriptRaise("range(T);", 0, "cannot be type");
	EidosAssertScriptSuccess("range(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 3}));
	EidosAssertScriptSuccess("range(3.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.5, 3.5}));
	EidosAssertScriptRaise("range('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("range(c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess("range(c(3, 7, 19, -5, 9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-5, 19}));
	EidosAssertScriptSuccess("range(c(3.3, 7.7, 19.1, -5.8, 9.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-5.8, 19.1}));
	EidosAssertScriptRaise("range(c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("range(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("range(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("range(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("range(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("range(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("range(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("range(c(1.0, 5.0, NAN, 2.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()}));
	
	EidosAssertScriptSuccess("range(integer(0), c(3,7,-8,0), 0, c(-10,10));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-10,10}));
	EidosAssertScriptSuccess("range(c(3.,7.,-8.,0.), 0., c(0.,10.), float(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-8,10}));
	EidosAssertScriptRaise("range(c(3,7,-8,0), c(-10.,10.));", 0, "the same type");
	
	// sd()
	EidosAssertScriptRaise("sd(T);", 0, "cannot be type");
	EidosAssertScriptSuccess("sd(3);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("sd(3.5);", gStaticEidosValueNULL);
	EidosAssertScriptRaise("sd('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sd(c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess("sd(c(2, 3, 2, 8, 0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3)));
	EidosAssertScriptSuccess("sd(c(9.1, 5.1, 5.1, 4.1, 7.1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.0)));
	EidosAssertScriptSuccess("sd(c(9.1, 5.1, 5.1, NAN, 7.1));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptRaise("sd(c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("sd(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("sd(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("sd(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("sd(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("sd(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("sd(string(0));", 0, "cannot be type");
	
	// ttest()
	EidosAssertScriptRaise("ttest(1:5.0);", 0, "either y or mu to be non-NULL");
	EidosAssertScriptRaise("ttest(1:5.0, 1:5.0, 5.0);", 0, "either y or mu to be NULL");
	EidosAssertScriptRaise("ttest(5.0, 1:5.0);", 0, "enough elements in x");
	EidosAssertScriptRaise("ttest(1:5.0, 5.0);", 0, "enough elements in y");
	EidosAssertScriptRaise("ttest(5.0, mu=6.0);", 0, "enough elements in x");
	EidosAssertScriptSuccess("abs(ttest(1:50.0, 1:50.0) - 1.0) < 0.001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(ttest(1:50.0, 1:60.0) - 0.101496) < 0.001;", gStaticEidosValue_LogicalT);			// R gives 0.1046, not sure why but I suspect corrected vs. uncorrected standard deviations
	EidosAssertScriptSuccess("abs(ttest(1:50.0, 10.0:60.0) - 0.00145575) < 0.001;", gStaticEidosValue_LogicalT);	// R gives 0.001615
	EidosAssertScriptSuccess("abs(ttest(1:50.0, mu=25.0) - 0.807481) < 0.001;", gStaticEidosValue_LogicalT);		// R gives 0.8094
	EidosAssertScriptSuccess("abs(ttest(1:50.0, mu=30.0) - 0.0321796) < 0.001;", gStaticEidosValue_LogicalT);		// R gives 0.03387
	EidosAssertScriptSuccess("ttest(c(1.0, 2.0, NAN), mu=25.0);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("ttest(c(1.0, 2.0, NAN), c(8.0, 9.0, NAN));", gStaticEidosValue_FloatNAN);
	
	// var()
	EidosAssertScriptRaise("var(T);", 0, "cannot be type");
	EidosAssertScriptSuccess("var(3);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("var(3.5);", gStaticEidosValueNULL);
	EidosAssertScriptRaise("var('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("var(c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess("var(c(2, 3, 2, 8, 0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(9)));
	EidosAssertScriptSuccess("var(c(9.1, 5.1, 5.1, 4.1, 7.1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(4.0)));
	EidosAssertScriptSuccess("var(c(9.1, 5.1, 5.1, NAN, 7.1));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptRaise("var(c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("var(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("var(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("var(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("var(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("var(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("var(string(0));", 0, "cannot be type");
}

#pragma mark distributions
void _RunFunctionDistributionTests(void)
{
	// dmvnorm()
	EidosAssertScriptRaise("dmvnorm(array(c(1.0,2,3,2,1), c(1,5,1)), c(0.0, 2.0), matrix(c(10,3,3,2), nrow=2));", 0, "requires x to be");
	EidosAssertScriptSuccess("dmvnorm(float(0), c(0.0, 2.0), matrix(c(10,3,3,2), nrow=2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("dmvnorm(3.0, c(0.0, 2.0), matrix(c(10,3,3,2), nrow=2));", 0, "dimensionality of >= 2");
	EidosAssertScriptRaise("dmvnorm(1.0:3.0, c(0.0, 2.0), matrix(c(10,3,3,2), nrow=2));", 0, "matching the dimensionality");
	EidosAssertScriptRaise("dmvnorm(c(0.0, 2.0), c(0.0, 2.0), c(10,3,3,2));", 0, "sigma to be a matrix");
	EidosAssertScriptRaise("dmvnorm(c(0.0, 2.0), c(0.0, 2.0, 3.0), matrix(c(10,3,3,2), nrow=2));", 0, "matching the dimensionality");
	EidosAssertScriptRaise("dmvnorm(c(0.0, 2.0), c(0.0, 2.0), matrix(c(10,3,3,2,4,8), nrow=3));", 0, "matching the dimensionality");
	EidosAssertScriptRaise("abs(dmvnorm(c(0.0, 2.0), c(0.0, 2.0), matrix(c(0,0,0,0), nrow=2)) - 0.047987) < 0.00001;", 4, "positive-definite");
	EidosAssertScriptSuccess("abs(dmvnorm(c(0.0, 2.0), c(0.0, 2.0), matrix(c(10,3,3,2), nrow=2)) - 0.047987) < 0.00001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("dmvnorm(c(NAN, 2.0), c(0.0, 2.0), matrix(c(10,3,3,2), nrow=2));", gStaticEidosValue_FloatNAN);
	
	// dnorm()
	EidosAssertScriptSuccess("dnorm(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("dnorm(float(0), float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("dnorm(0.0, 0, 1) - 0.3989423 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true}));
	EidosAssertScriptSuccess("dnorm(1.0, 1.0, 1.0) - 0.3989423 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true}));
	EidosAssertScriptSuccess("dnorm(c(0.0,0.0), c(0,0), 1) - 0.3989423 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("dnorm(c(0.0,1.0), c(0.0,1.0), 1.0) - 0.3989423 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("dnorm(c(0.0,0.0), 0.0, c(1.0,1.0)) - 0.3989423 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("dnorm(c(-1.0,0.0,1.0)) - c(0.2419707,0.3989423,0.2419707) < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptRaise("dnorm(1.0, 0, 0);", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("dnorm(1.0, 0.0, -1.0);", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("dnorm(c(0.5, 1.0), 0.0, c(5, -1.0));", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("dnorm(1.0, c(-10, 10, 1), 100.0);", 0, "requires mean to be");
	EidosAssertScriptRaise("dnorm(1.0, 10.0, c(0.1, 10, 1));", 0, "requires sd to be");
	EidosAssertScriptSuccess("dnorm(NAN);", gStaticEidosValue_FloatNAN);
	
	// pnorm()
	EidosAssertScriptSuccess("pnorm(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("pnorm(float(0), float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("pnorm(0.0, 0, 1) - 0.5 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true}));
	EidosAssertScriptSuccess("pnorm(1.0, 1.0, 1.0) - 0.5 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true}));
	EidosAssertScriptSuccess("pnorm(c(0.0,0.0), c(0,0), 1) - 0.5 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("pnorm(c(0.0,1.0), c(0.0,1.0), 1.0) - 0.5 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("pnorm(c(0.0,0.0), 0.0, c(1.0,1.0)) - 0.5 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("pnorm(c(-1.0,0.0,1.0)) - c(0.1586553,0.5,0.8413447) < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("pnorm(c(-1.0,0.0,1.0), mean=0.5, sd=10) - c(0.4403823,0.4800612,0.5199388) < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptRaise("pnorm(1.0, 0, 0);", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("pnorm(1.0, 0.0, -1.0);", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("pnorm(c(0.5, 1.0), 0.0, c(5, -1.0));", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("pnorm(1.0, c(-10, 10, 1), 100.0);", 0, "requires mean to be");
	EidosAssertScriptRaise("pnorm(1.0, 10.0, c(0.1, 10, 1));", 0, "requires sd to be");
	EidosAssertScriptSuccess("pnorm(NAN);", gStaticEidosValue_FloatNAN);
	
	// rbeta()
	EidosAssertScriptSuccess("rbeta(0, 1, 1000);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rbeta(0, float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setSeed(0); abs(rbeta(1, 1, 5) - c(0.115981)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true}));
	EidosAssertScriptSuccess("setSeed(0); abs(rbeta(3, 1, 5) - c(0.115981, 0.0763773, 0.05032)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptRaise("rbeta(-1, 1, 1000);", 0, "requires n to be");
	EidosAssertScriptRaise("rbeta(2, 0, 1);", 0, "requires alpha > 0.0");
	EidosAssertScriptRaise("rbeta(2, c(1,0), 1);", 0, "requires alpha > 0.0");
	EidosAssertScriptRaise("rbeta(2, 1, 0);", 0, "requires beta > 0.0");
	EidosAssertScriptRaise("rbeta(2, 1, c(1,0));", 0, "requires beta > 0.0");
	EidosAssertScriptRaise("rbeta(2, c(0.1, 10, 1), 10.0);", 0, "requires alpha to be of length");
	EidosAssertScriptRaise("rbeta(2, 10.0, c(0.1, 10, 1));", 0, "requires beta to be of length");
	EidosAssertScriptSuccess("rbeta(1, NAN, 1);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("rbeta(1, 1, NAN);", gStaticEidosValue_FloatNAN);
	
	// rbinom()
	EidosAssertScriptSuccess("rbinom(0, 10, 0.5);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("rbinom(1, 10, 0.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0}));
	EidosAssertScriptSuccess("rbinom(3, 10, 0.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0}));
	EidosAssertScriptSuccess("rbinom(3, 10, 1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 10, 10}));
	EidosAssertScriptSuccess("rbinom(3, 0, 0.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0}));
	EidosAssertScriptSuccess("rbinom(3, 0, 1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0}));
	EidosAssertScriptSuccess("setSeed(0); rbinom(10, 1, 0.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 1, 1, 1, 1, 1, 0, 0, 0, 0}));
	EidosAssertScriptSuccess("setSeed(0); rbinom(10, 1, 0.5000001);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 0, 0, 1, 1, 0, 1, 0, 1, 0}));
	EidosAssertScriptSuccess("setSeed(0); rbinom(5, 10, 0.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 8, 5, 3, 4}));
	EidosAssertScriptSuccess("setSeed(1); rbinom(5, 10, 0.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 6, 3, 6, 3}));
	EidosAssertScriptSuccess("setSeed(2); rbinom(5, 1000, 0.01);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{11, 16, 10, 14, 10}));
	EidosAssertScriptSuccess("setSeed(3); rbinom(5, 1000, 0.99);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{992, 990, 995, 991, 995}));
	EidosAssertScriptSuccess("setSeed(4); rbinom(3, 100, c(0.1, 0.5, 0.9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 50, 87}));
	EidosAssertScriptSuccess("setSeed(5); rbinom(3, c(10, 30, 50), 0.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{6, 12, 26}));
	EidosAssertScriptRaise("rbinom(-1, 10, 0.5);", 0, "requires n to be");
	EidosAssertScriptRaise("rbinom(3, -1, 0.5);", 0, "requires size >= 0");
	EidosAssertScriptRaise("rbinom(3, 10, -0.1);", 0, "in [0.0, 1.0]");
	EidosAssertScriptRaise("rbinom(3, 10, 1.1);", 0, "in [0.0, 1.0]");
	EidosAssertScriptRaise("rbinom(3, 10, c(0.1, 0.2));", 0, "to be of length 1 or n");
	EidosAssertScriptRaise("rbinom(3, c(10, 12), 0.5);", 0, "to be of length 1 or n");
	EidosAssertScriptRaise("rbinom(2, -1, c(0.5,0.5));", 0, "requires size >= 0");
	EidosAssertScriptRaise("rbinom(2, c(10,10), -0.1);", 0, "in [0.0, 1.0]");
	EidosAssertScriptRaise("rbinom(2, 10, NAN);", 0, "in [0.0, 1.0]");
	
	// rcauchy()
	EidosAssertScriptSuccess("rcauchy(0);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rcauchy(0, float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setSeed(0); (rcauchy(2) - c(0.665522, -0.155038)) < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("setSeed(0); (rcauchy(2, 10.0) - c(10.6655, 9.84496)) < 0.001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("setSeed(2); (rcauchy(2, 10.0, 100.0) - c(-255.486, -4.66262)) < 0.001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("setSeed(3); (rcauchy(2, c(-10, 10), 100.0) - c(89.8355, 1331.82)) < 0.01;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("setSeed(4); (rcauchy(2, 10.0, c(0.1, 10)) - c(10.05, -4.51227)) < 0.001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptRaise("rcauchy(-1);", 0, "requires n to be");
	EidosAssertScriptRaise("rcauchy(1, 0, 0);", 0, "requires scale > 0.0");
	EidosAssertScriptRaise("rcauchy(2, c(0,0), -1);", 0, "requires scale > 0.0");
	EidosAssertScriptRaise("rcauchy(2, c(-10, 10, 1), 100.0);", 0, "requires location to be");
	EidosAssertScriptRaise("rcauchy(2, 10.0, c(0.1, 10, 1));", 0, "requires scale to be");
	EidosAssertScriptSuccess("rcauchy(1, NAN, 100.0);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("rcauchy(1, 10.0, NAN);", gStaticEidosValue_FloatNAN);
	
	// rdunif()
	EidosAssertScriptSuccess("rdunif(0);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("rdunif(0, integer(0), integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("rdunif(1, 0, 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0}));
	EidosAssertScriptSuccess("rdunif(3, 0, 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0}));
	EidosAssertScriptSuccess("rdunif(1, 1, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1}));
	EidosAssertScriptSuccess("rdunif(3, 1, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 1, 1}));
	EidosAssertScriptSuccess("setSeed(0); identical(rdunif(1), 0);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSeed(0); identical(rdunif(10), c(0,1,1,1,1,1,0,0,0,0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSeed(0); identical(rdunif(10, 10, 11), c(10,11,11,11,11,11,10,10,10,10));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSeed(0); identical(rdunif(10, 10, 15), c(10, 15, 11, 10, 14, 12, 11, 10, 12, 15));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSeed(0); identical(rdunif(10, -10, 15), c(-6, 9, 13, 8, -10, -2, 1, -2, 4, -9));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSeed(0); identical(rdunif(5, 1000000, 2000000), c(1834587, 1900900, 1272746, 1916963, 1786506));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSeed(0); identical(rdunif(5, 1000000000, 2000000000), c(1824498419, 1696516320, 1276316141, 1114192161, 1469447550));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSeed(0); identical(rdunif(5, 10000000000, 20000000000), c(18477398967, 14168180191, 12933243864, 17033840166, 15472500391));", gStaticEidosValue_LogicalT);	// 64-bit range
	EidosAssertScriptRaise("rdunif(-1);", 0, "requires n to be");
	EidosAssertScriptRaise("rdunif(1, 0, -1);", 0, "requires min <= max");
	EidosAssertScriptRaise("rdunif(2, 0, c(7, -1));", 0, "requires min <= max");
	EidosAssertScriptRaise("rdunif(2, c(7, -1), 0);", 0, "requires min <= max");
	EidosAssertScriptRaise("rdunif(2, c(-10, 10, 1), 100);", 0, "requires min");
	EidosAssertScriptRaise("rdunif(2, -10, c(1, 10, 1));", 0, "requires max");
	
	// rexp()
	EidosAssertScriptSuccess("rexp(0);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rexp(0, float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setSeed(0); abs(rexp(1) - c(0.206919)) < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true}));
	EidosAssertScriptSuccess("setSeed(0); abs(rexp(3) - c(0.206919, 3.01675, 0.788416)) < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("setSeed(1); abs(rexp(3, 10) - c(20.7, 12.2, 0.9)) < 0.1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("setSeed(2); abs(rexp(3, 100000) - c(95364.3, 307170.0, 74334.9)) < 0.1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("setSeed(3); abs(rexp(3, c(10, 100, 1000)) - c(2.8, 64.6, 58.8)) < 0.1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptRaise("rexp(-1);", 0, "requires n to be");
	EidosAssertScriptRaise("rexp(3, c(10, 5));", 0, "requires mu to be");
	EidosAssertScriptSuccess("rexp(1, NAN);", gStaticEidosValue_FloatNAN);
	
	// rgamma()
	EidosAssertScriptSuccess("rgamma(0, 0, 1000);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rgamma(0, float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rgamma(3, 0, 1000);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0.0, 0.0, 0.0}));
	EidosAssertScriptSuccess("setSeed(0); abs(rgamma(1, 1, 100) - c(1.02069)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true}));
	EidosAssertScriptSuccess("setSeed(0); abs(rgamma(3, 1, 100) - c(1.02069, 1.0825, 0.951862)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("setSeed(0); abs(rgamma(3, -1, 100) - c(-1.02069, -1.0825, -0.951862)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("setSeed(0); abs(rgamma(3, c(-1,-1,-1), 100) - c(-1.02069, -1.0825, -0.951862)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("setSeed(0); abs(rgamma(3, -1, c(100,100,100)) - c(-1.02069, -1.0825, -0.951862)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptRaise("rgamma(-1, 0, 1000);", 0, "requires n to be");
	EidosAssertScriptRaise("rgamma(2, 0, 0);", 0, "requires shape > 0.0");
	EidosAssertScriptRaise("rgamma(2, c(0,0), 0);", 0, "requires shape > 0.0");
	EidosAssertScriptRaise("rgamma(2, c(0.1, 10, 1), 10.0);", 0, "requires mean to be of length");
	EidosAssertScriptRaise("rgamma(2, 10.0, c(0.1, 10, 1));", 0, "requires shape to be of length");
	EidosAssertScriptSuccess("rgamma(1, NAN, 100);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("rgamma(1, 1, NAN);", gStaticEidosValue_FloatNAN);
	
	// rgeom()
	EidosAssertScriptSuccess("rgeom(0, 1.0);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("rgeom(1, 1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0}));
	EidosAssertScriptSuccess("rgeom(5, 1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0, 0, 0}));
	EidosAssertScriptSuccess("setSeed(1); rgeom(5, 0.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 1, 10, 1, 10}));
	EidosAssertScriptSuccess("setSeed(1); rgeom(5, 0.4);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 4, 0, 4}));
	EidosAssertScriptSuccess("setSeed(5); rgeom(5, 0.01);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{31, 31, 299, 129, 58}));
	EidosAssertScriptSuccess("setSeed(2); rgeom(1, 0.0001);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4866}));
	EidosAssertScriptSuccess("setSeed(3); rgeom(6, c(1, 0.1, 0.01, 0.001, 0.0001, 0.00001));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 13, 73, 2860, 8316, 282489}));
	EidosAssertScriptRaise("rgeom(-1, 1.0);", 0, "requires n to be");
	EidosAssertScriptRaise("rgeom(0, 0.0);", 0, "requires 0.0 < p <= 1.0");
	EidosAssertScriptRaise("rgeom(0, 1.1);", 0, "requires 0.0 < p <= 1.0");
	EidosAssertScriptRaise("rgeom(2, c(0.1, 0.1, 0.1));", 0, "requires p to be of length 1 or n");
	EidosAssertScriptRaise("rgeom(2, c(0.0, 0.0));", 0, "requires 0.0 < p <= 1.0");
	EidosAssertScriptRaise("rgeom(2, c(0.5, 1.1));", 0, "requires 0.0 < p <= 1.0");
	EidosAssertScriptRaise("rgeom(2, NAN);", 0, "requires 0.0 < p <= 1.0");
	
	// rlnorm()
	EidosAssertScriptSuccess("rlnorm(0);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rlnorm(0, float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rlnorm(1, 0, 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0}));
	EidosAssertScriptSuccess("rlnorm(3, 0, 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 1.0, 1.0}));
	EidosAssertScriptSuccess("abs(rlnorm(3, 1, 0) - E) < 0.000001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("abs(rlnorm(3, c(1,1,1), 0) - E) < 0.000001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("abs(rlnorm(3, 1, c(0,0,0)) - E) < 0.000001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptRaise("rlnorm(-1);", 0, "requires n to be");
	EidosAssertScriptRaise("rlnorm(2, c(-10, 10, 1), 100.0);", 0, "requires meanlog to be");
	EidosAssertScriptRaise("rlnorm(2, 10.0, c(0.1, 10, 1));", 0, "requires sdlog to be");
	EidosAssertScriptSuccess("rlnorm(1, NAN, 100);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("rlnorm(1, 1, NAN);", gStaticEidosValue_FloatNAN);
	
	// rmvnorm()
	EidosAssertScriptRaise("rmvnorm(0, c(0,2), matrix(c(10,3), nrow=2));", 0, "requires n to be");
	EidosAssertScriptRaise("rmvnorm(5, matrix(c(0,0)), matrix(c(10,3,3,2), nrow=2));", 0, "plain vector of length k");
	EidosAssertScriptRaise("rmvnorm(5, c(0,0), c(10,3,3,2));", 0, "sigma to be a matrix");
	EidosAssertScriptRaise("rmvnorm(5, 0, matrix(c(10,3,3,2), nrow=2));", 0, "k must be >= 2");
	EidosAssertScriptRaise("rmvnorm(5, c(0,2), matrix(c(10,3), nrow=2));", 0, "sigma to be a k x k matrix");
	EidosAssertScriptRaise("rmvnorm(5, c(0,2), matrix(c(10,3,3,2), nrow=1)); NULL;", 0, "sigma to be a k x k matrix");
	EidosAssertScriptRaise("rmvnorm(5, c(0,2), matrix(c(0,0,0,0), nrow=2));", 0, "positive-definite");
	EidosAssertScriptSuccess("x = rmvnorm(5, c(0,2), matrix(c(10,3,3,2), nrow=2)); identical(dim(x), c(5,2));", gStaticEidosValue_LogicalT);
	
	// rnorm()
	EidosAssertScriptSuccess("rnorm(0);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rnorm(0, float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rnorm(1, 0, 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0.0}));
	EidosAssertScriptSuccess("rnorm(3, 0, 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0.0, 0.0, 0.0}));
	EidosAssertScriptSuccess("rnorm(1, 1, 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0}));
	EidosAssertScriptSuccess("rnorm(3, 1, 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 1.0, 1.0}));
	EidosAssertScriptSuccess("setSeed(0); (rnorm(2) - c(-0.785386, 0.132009)) < 0.000001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("setSeed(1); (rnorm(2, 10.0) - c(10.38, 10.26)) < 0.01;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("setSeed(2); (rnorm(2, 10.0, 100.0) - c(59.92, 95.35)) < 0.01;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("setSeed(3); (rnorm(2, c(-10, 10), 100.0) - c(59.92, 95.35)) < 0.01;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("setSeed(4); (rnorm(2, 10.0, c(0.1, 10)) - c(59.92, 95.35)) < 0.01;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptRaise("rnorm(-1);", 0, "requires n to be");
	EidosAssertScriptRaise("rnorm(1, 0, -1);", 0, "requires sd >= 0.0");
	EidosAssertScriptRaise("rnorm(2, c(0,0), -1);", 0, "requires sd >= 0.0");
	EidosAssertScriptRaise("rnorm(2, c(-10, 10, 1), 100.0);", 0, "requires mean to be");
	EidosAssertScriptRaise("rnorm(2, 10.0, c(0.1, 10, 1));", 0, "requires sd to be");
	EidosAssertScriptSuccess("rnorm(1, 1, NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("rnorm(1, NAN, 1);", gStaticEidosValue_FloatNAN);
	
	// rpois()
	EidosAssertScriptSuccess("rpois(0, 1.0);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setSeed(0); rpois(5, 1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 2, 0, 1, 1}));
	EidosAssertScriptSuccess("setSeed(1); rpois(5, 0.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 0, 0, 0, 0}));
	EidosAssertScriptSuccess("setSeed(2); rpois(5, 10000);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10205, 10177, 10094, 10227, 9875}));
	EidosAssertScriptSuccess("setSeed(2); rpois(1, 10000);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10205}));
	EidosAssertScriptSuccess("setSeed(3); rpois(5, c(1, 10, 100, 1000, 10000));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 8, 97, 994, 9911}));
	EidosAssertScriptRaise("rpois(-1, 1.0);", 0, "requires n to be");
	EidosAssertScriptRaise("rpois(0, 0.0);", 0, "requires lambda > 0.0");
	EidosAssertScriptRaise("rpois(0, NAN);", 0, "requires lambda > 0.0");
	EidosAssertScriptRaise("rpois(2, c(0.0, 0.0));", 0, "requires lambda > 0.0");
	EidosAssertScriptRaise("rpois(2, c(1.5, NAN));", 0, "requires lambda > 0.0");
	EidosAssertScriptRaise("setSeed(4); rpois(5, c(1, 10, 100, 1000));", 12, "requires lambda");
	
	// runif()
	EidosAssertScriptSuccess("runif(0);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("runif(0, float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("runif(1, 0, 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0.0}));
	EidosAssertScriptSuccess("runif(3, 0, 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0.0, 0.0, 0.0}));
	EidosAssertScriptSuccess("runif(1, 1, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0}));
	EidosAssertScriptSuccess("runif(3, 1, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 1.0, 1.0}));
	EidosAssertScriptSuccess("setSeed(0); abs(runif(1) - c(0.186915)) < 0.000001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true}));
	EidosAssertScriptSuccess("setSeed(0); abs(runif(2) - c(0.186915, 0.951040)) < 0.000001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("setSeed(1); abs(runif(2, 0.5) - c(0.93, 0.85)) < 0.01;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("setSeed(2); abs(runif(2, 10.0, 100.0) - c(65.31, 95.82)) < 0.01;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("setSeed(3); abs(runif(2, c(-100, 1), 10.0) - c(-72.52, 5.28)) < 0.01;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("setSeed(4); abs(runif(2, -10.0, c(1, 1000)) - c(-8.37, 688.97)) < 0.01;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptRaise("runif(-1);", 0, "requires n to be");
	EidosAssertScriptRaise("runif(1, 0, -1);", 0, "requires min < max");
	EidosAssertScriptRaise("runif(2, 0, c(7,-1));", 0, "requires min < max");
	EidosAssertScriptRaise("runif(2, c(-10, 10, 1), 100.0);", 0, "requires min");
	EidosAssertScriptRaise("runif(2, -10.0, c(0.1, 10, 1));", 0, "requires max");
	EidosAssertScriptSuccess("runif(1, 1, NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("runif(1, NAN, 1);", gStaticEidosValue_FloatNAN);
	
	// rweibull()
	EidosAssertScriptSuccess("rweibull(0, 1, 1);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rweibull(0, float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setSeed(0); abs(rweibull(1, 1, 1) - c(1.6771)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true}));
	EidosAssertScriptSuccess("setSeed(0); abs(rweibull(3, 1, 1) - c(1.6771, 0.0501994, 0.60617)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptRaise("rweibull(1, 0, 1);", 0, "requires lambda > 0.0");
	EidosAssertScriptRaise("rweibull(1, 1, 0);", 0, "requires k > 0.0");
	EidosAssertScriptRaise("rweibull(3, c(1,1,0), 1);", 0, "requires lambda > 0.0");
	EidosAssertScriptRaise("rweibull(3, 1, c(1,1,0));", 0, "requires k > 0.0");
	EidosAssertScriptRaise("rweibull(-1, 1, 1);", 0, "requires n to be");
	EidosAssertScriptRaise("rweibull(2, c(10, 0, 1), 100.0);", 0, "requires lambda to be");
	EidosAssertScriptRaise("rweibull(2, 10.0, c(0.1, 0, 1));", 0, "requires k to be");
	EidosAssertScriptSuccess("rweibull(1, 1, NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("rweibull(1, NAN, 1);", gStaticEidosValue_FloatNAN);
}

#pragma mark vector construction
void _RunFunctionVectorConstructionTests(void)
{
	// c()
	EidosAssertScriptSuccess("c();", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("c(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("c(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("c(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("c(3.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.1)));
	EidosAssertScriptSuccess("c('foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("c(_Test(7))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("c(NULL, NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("c(T, F, T, T, T, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, true, true, false}));
	EidosAssertScriptSuccess("c(3, 7, 19, -5, 9);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 7, 19, -5, 9}));
	EidosAssertScriptSuccess("c(3.3, 7.7, 19.1, -5.8, 9.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.3, 7.7, 19.1, -5.8, 9.0}));
	EidosAssertScriptSuccess("c('foo', 'bar', 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar", "baz"}));
	EidosAssertScriptSuccess("c(_Test(7), _Test(3), _Test(-9))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 3, -9}));
	EidosAssertScriptSuccess("c(T, c(T, F, F), T, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, false, false, true, false}));
	EidosAssertScriptSuccess("c(3, 7, c(17, -2), -5, 9);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 7, 17, -2, -5, 9}));
	EidosAssertScriptSuccess("c(3.3, 7.7, c(17.1, -2.9), -5.8, 9.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.3, 7.7, 17.1, -2.9, -5.8, 9.0}));
	EidosAssertScriptSuccess("c('foo', c('bar', 'bar2', 'bar3'), 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar", "bar2", "bar3", "baz"}));
	EidosAssertScriptSuccess("c(T, 3, c(F, T), 7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 0, 1, 7}));
	EidosAssertScriptSuccess("c(T, 3, c(F, T), 7.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 3, 0, 1, 7.1}));
	EidosAssertScriptSuccess("c(T, c(3, 6), 'bar', 7.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"T", "3", "6", "bar", "7.1"}));
	EidosAssertScriptSuccess("c(T, NULL);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("c(3, NULL);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("c(3.1, NULL);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.1)));
	EidosAssertScriptSuccess("c('foo', NULL);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("c(_Test(7), NULL)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("c(NULL, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("c(NULL, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("c(NULL, 3.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.1)));
	EidosAssertScriptSuccess("c(NULL, 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("c(NULL, _Test(7))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptRaise("c(T, _Test(7));", 0, "cannot be mixed");
	EidosAssertScriptRaise("c(3, _Test(7));", 0, "cannot be mixed");
	EidosAssertScriptRaise("c(3.1, _Test(7));", 0, "cannot be mixed");
	EidosAssertScriptRaise("c('foo', _Test(7));", 0, "cannot be mixed");
	EidosAssertScriptSuccess("c(object(), _Test(7))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("c(_Test(7), object())._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("c(object(), object());", gStaticEidosValue_Object_ZeroVec);
	//EidosAssertScriptSuccess("c(object(), object());", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidosTestElement_Class)));		// should fail
	EidosAssertScriptSuccess("c(object(), _Test(7)[F]);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidosTestElement_Class)));
	EidosAssertScriptSuccess("c(_Test(7)[F], object());", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidosTestElement_Class)));
	
	// float()
	EidosAssertScriptSuccess("float(0);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("float(1);", gStaticEidosValue_Float0);
	EidosAssertScriptSuccess("float(2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0.0, 0.0}));
	EidosAssertScriptSuccess("float(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0.0, 0.0, 0.0, 0.0, 0.0}));
	EidosAssertScriptRaise("float(-1);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("float(-10000);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("float(NULL);", 0, "cannot be type NULL");
	EidosAssertScriptRaise("float(integer(0));", 0, "must be a singleton");
	
	// integer()
	EidosAssertScriptSuccess("integer(0);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("integer(1);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("integer(2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0}));
	EidosAssertScriptSuccess("integer(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0, 0, 0}));
	EidosAssertScriptRaise("integer(-1);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("integer(-10000);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("integer(NULL);", 0, "cannot be type NULL");
	EidosAssertScriptRaise("integer(integer(0));", 0, "must be a singleton");
	
	EidosAssertScriptSuccess("integer(10, 0, 1, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0, 1, 0, 0, 0, 0, 0, 0}));
	EidosAssertScriptSuccess("integer(10, 1, 0, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 1, 1, 0, 1, 1, 1, 1, 1, 1}));
	EidosAssertScriptSuccess("integer(10, 8, -3, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{8, 8, 8, -3, 8, 8, 8, 8, 8, 8}));
	EidosAssertScriptSuccess("integer(10, 0, 1, c(3, 7, 1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 1, 0, 1, 0, 0, 0, 1, 0, 0}));
	EidosAssertScriptSuccess("integer(10, 1, 0, c(3, 7, 1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 0, 1, 0, 1, 1, 1, 0, 1, 1}));
	EidosAssertScriptSuccess("integer(10, 8, -3, c(3, 7, 1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{8, -3, 8, -3, 8, 8, 8, -3, 8, 8}));
	EidosAssertScriptSuccess("integer(10, 8, -3, integer(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{8, 8, 8, 8, 8, 8, 8, 8, 8, 8}));
	EidosAssertScriptSuccess("integer(10, 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{8, 8, 8, 8, 8, 8, 8, 8, 8, 8}));
	EidosAssertScriptRaise("integer(10, 0, 1, -7);", 0, "requires positions in fill2Indices to be");
	EidosAssertScriptRaise("integer(10, 0, 1, c(1, 2, -7, 9));", 0, "requires positions in fill2Indices to be");
	
	// logical()
	EidosAssertScriptSuccess("logical(0);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("logical(1);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("logical(2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	EidosAssertScriptSuccess("logical(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false, false}));
	EidosAssertScriptRaise("logical(-1);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("logical(-10000);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("logical(NULL);", 0, "cannot be type NULL");
	EidosAssertScriptRaise("logical(integer(0));", 0, "must be a singleton");
	
	// object()
	EidosAssertScriptSuccess("object();", gStaticEidosValue_Object_ZeroVec);
	EidosAssertScriptRaise("object(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("object(integer(0));", 0, "too many arguments supplied");
	
	// rep()
	EidosAssertScriptRaise("rep(NULL, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("rep(T, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("rep(3, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("rep(3.5, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("rep('foo', -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("rep(_Test(7), -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptSuccess("rep(NULL, 0);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("rep(T, 0);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("rep(3, 0);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("rep(3.5, 0);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rep('foo', 0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("rep(_Test(7), 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidosTestElement_Class)));
	EidosAssertScriptSuccess("rep(NULL, 2);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("rep(T, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("rep(3, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 3}));
	EidosAssertScriptSuccess("rep(3.5, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.5, 3.5}));
	EidosAssertScriptSuccess("rep('foo', 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "foo"}));
	EidosAssertScriptSuccess("rep(_Test(7), 2)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 7}));
	EidosAssertScriptSuccess("rep(c(T, F), 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("rep(c(3, 7), 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 7, 3, 7}));
	EidosAssertScriptSuccess("rep(c(3.5, 9.1), 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.5, 9.1, 3.5, 9.1}));
	EidosAssertScriptSuccess("rep(c('foo', 'bar'), 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar", "foo", "bar"}));
	EidosAssertScriptSuccess("rep(c(_Test(7), _Test(2)), 2)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 2, 7, 2}));
	EidosAssertScriptSuccess("rep(logical(0), 5);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("rep(integer(0), 5);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("rep(float(0), 5);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rep(string(0), 5);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("rep(object(), 5);", gStaticEidosValue_Object_ZeroVec);
	EidosAssertScriptRaise("rep(object(), c(5, 3));", 0, "must be a singleton");
	EidosAssertScriptRaise("rep(object(), integer(0));", 0, "must be a singleton");
	
	// repEach()
	EidosAssertScriptRaise("repEach(NULL, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("repEach(T, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("repEach(3, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("repEach(3.5, -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("repEach('foo', -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptRaise("repEach(_Test(7), -1);", 0, "count to be greater than or equal to 0");
	EidosAssertScriptSuccess("repEach(NULL, 0);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("repEach(T, 0);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("repEach(3, 0);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("repEach(3.5, 0);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("repEach('foo', 0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("repEach(_Test(7), 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidosTestElement_Class)));
	EidosAssertScriptSuccess("repEach(NULL, 2);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("repEach(T, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("repEach(3, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 3}));
	EidosAssertScriptSuccess("repEach(3.5, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.5, 3.5}));
	EidosAssertScriptSuccess("repEach('foo', 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "foo"}));
	EidosAssertScriptSuccess("repEach(_Test(7), 2)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 7}));
	EidosAssertScriptSuccess("repEach(c(T, F), 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, false, false}));
	EidosAssertScriptSuccess("repEach(c(3, 7), 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 3, 7, 7}));
	EidosAssertScriptSuccess("repEach(c(3.5, 9.1), 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.5, 3.5, 9.1, 9.1}));
	EidosAssertScriptSuccess("repEach(c('foo', 'bar'), 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "foo", "bar", "bar"}));
	EidosAssertScriptSuccess("repEach(c(_Test(7), _Test(2)), 2)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 7, 2, 2}));
	EidosAssertScriptRaise("repEach(NULL, c(2,3));", 0, "requires that parameter");
	EidosAssertScriptSuccess("repEach(c(T, F), c(2,3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, false, false, false}));
	EidosAssertScriptSuccess("repEach(c(3, 7), c(2,3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 3, 7, 7, 7}));
	EidosAssertScriptSuccess("repEach(c(3.5, 9.1), c(2,3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.5, 3.5, 9.1, 9.1, 9.1}));
	EidosAssertScriptSuccess("repEach(c('foo', 'bar'), c(2,3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "foo", "bar", "bar", "bar"}));
	EidosAssertScriptSuccess("repEach(c(_Test(7), _Test(2)), c(2,3))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 7, 2, 2, 2}));
	EidosAssertScriptRaise("repEach(NULL, c(2,-1));", 0, "requires that parameter");
	EidosAssertScriptRaise("repEach(c(T, F), c(2,-1));", 0, "requires all elements of");
	EidosAssertScriptRaise("repEach(c(3, 7), c(2,-1));", 0, "requires all elements of");
	EidosAssertScriptRaise("repEach(c(3.5, 9.1), c(2,-1));", 0, "requires all elements of");
	EidosAssertScriptRaise("repEach(c('foo', 'bar'), c(2,-1));", 0, "requires all elements of");
	EidosAssertScriptRaise("repEach(c(_Test(7), _Test(2)), c(2,-1))._yolk;", 0, "requires all elements of");
	EidosAssertScriptRaise("repEach(NULL, c(2,3,1));", 0, "requires that parameter");
	EidosAssertScriptRaise("repEach(c(T, F), c(2,3,1));", 0, "requires that parameter");
	EidosAssertScriptRaise("repEach(c(3, 7), c(2,3,1));", 0, "requires that parameter");
	EidosAssertScriptRaise("repEach(c(3.5, 9.1), c(2,3,1));", 0, "requires that parameter");
	EidosAssertScriptRaise("repEach(c('foo', 'bar'), c(2,3,1));", 0, "requires that parameter");
	EidosAssertScriptRaise("repEach(c(_Test(7), _Test(2)), c(2,3,1))._yolk;", 0, "requires that parameter");
	EidosAssertScriptSuccess("repEach(logical(0), 5);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("repEach(integer(0), 5);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("repEach(float(0), 5);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("repEach(string(0), 5);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("repEach(object(), 5);", gStaticEidosValue_Object_ZeroVec);
	EidosAssertScriptRaise("repEach(object(), c(5, 3));", 0, "requires that parameter");
	EidosAssertScriptSuccess("repEach(object(), integer(0));", gStaticEidosValue_Object_ZeroVec);
	
	// sample() – since this function treats parameter x type-agnostically, we'll test integers only (and NULL a little bit)
	EidosAssertScriptSuccess("sample(NULL, 0, T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("sample(NULL, 0, F);", gStaticEidosValueNULL);
	EidosAssertScriptRaise("sample(NULL, 1, T);", 0, "insufficient elements");
	EidosAssertScriptRaise("sample(NULL, 1, F);", 0, "insufficient elements");
	EidosAssertScriptRaise("sample(1:5, -1, T);", 0, "requires a sample size");
	EidosAssertScriptRaise("sample(1:5, -1, F);", 0, "requires a sample size");
	EidosAssertScriptSuccess("sample(integer(0), 0, T);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("sample(integer(0), 0, F);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptRaise("sample(integer(0), 1, T);", 0, "insufficient elements");
	EidosAssertScriptRaise("sample(integer(0), 1, F);", 0, "insufficient elements");
	EidosAssertScriptSuccess("sample(5, 1, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("sample(5, 1, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("sample(5, 2, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 5}));
	EidosAssertScriptSuccess("sample(5, 2, T, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 5}));
	EidosAssertScriptRaise("sample(5, 2, T, -1);", 0, "requires all weights to be");
	EidosAssertScriptRaise("sample(1:5, 2, T, c(1,2,-1,4,5));", 0, "requires all weights to be");
	EidosAssertScriptRaise("sample(5, 2, T, 0);", 0, "summing to <= 0");
	EidosAssertScriptRaise("sample(1:5, 2, T, c(0,0,0,0,0));", 0, "summing to <= 0");
	EidosAssertScriptSuccess("sample(5, 2, T, 1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 5}));
	EidosAssertScriptRaise("sample(5, 2, T, -1.0);", 0, "requires all weights to be");
	EidosAssertScriptRaise("sample(1:5, 2, T, c(1,2,-1.0,4,5));", 0, "requires all weights to be");
	EidosAssertScriptRaise("sample(5, 2, T, 0.0);", 0, "summing to <= 0");
	EidosAssertScriptRaise("sample(1:5, 2, T, c(0.0,0,0,0,0));", 0, "summing to <= 0");
	EidosAssertScriptRaise("sample(5, 2, T, NAN);", 0, "requires all weights to be");
	EidosAssertScriptRaise("sample(1:5, 2, T, c(1,2,NAN,4,5));", 0, "requires all weights to be");
	EidosAssertScriptRaise("sample(5, 2, F);", 0, "insufficient elements");
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 5, 3, 1, 2}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 5, 3, 2, 4}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 6, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 5, 3, 1, 2, 3}));
	EidosAssertScriptRaise("setSeed(0); sample(1:5, 6, F);", 12, "insufficient elements");
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 1, T, (1:5)*(1:5)*(1:5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 1, T, (1.0:5.0)^3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 1, F, (1:5)*(1:5)*(1:5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 1, F, (1.0:5.0)^3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 1, T, (0:4)*(0:4)*(0:4));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 1, T, (0.0:4.0)^3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 1, T, c(0,0,1,0,0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 1, T, c(0,0,1.0,0,0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 1, F, c(0,0,1,0,0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 1, F, c(0,0,1.0,0,0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("setSeed(0); sum(sample(1:5, 1, T, c(1,0,100,0,0)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("setSeed(0); sum(sample(1:5, 1, T, c(1.0,0,100.0,0,0)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("setSeed(0); sum(sample(1:5, 1, F, c(1,0,100,0,0)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("setSeed(0); sum(sample(1:5, 1, F, c(1.0,0,100.0,0,0)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("setSeed(0); sum(sample(1:5, 2, T, c(1,0,100,0,0)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{6}));
	EidosAssertScriptSuccess("setSeed(0); sum(sample(1:5, 2, T, c(1.0,0,100.0,0,0)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{6}));
	EidosAssertScriptSuccess("setSeed(0); sum(sample(1:5, 2, F, c(1,0,100,0,0)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4}));
	EidosAssertScriptSuccess("setSeed(0); sum(sample(1:5, 2, F, c(1.0,0,100.0,0,0)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4}));
	EidosAssertScriptSuccess("setSeed(0); sum(sample(1:5, 100, T, c(1,0,100,0,0)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{298}));
	EidosAssertScriptSuccess("setSeed(0); sum(sample(1:5, 100, T, c(1.0,0,100.0,0,0)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{298}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, T, (1:5)*(1:5)*(1:5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 5, 5, 3, 4}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, T, (1.0:5.0)^3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 5, 5, 3, 4}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, F, (1:5)*(1:5)*(1:5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 5, 3, 1, 2}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, F, (1.0:5.0)^3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 5, 3, 1, 2}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, T, (0:4)*(0:4)*(0:4));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 5, 5, 3, 4}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, T, (0.0:4.0)^3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 5, 5, 3, 4}));
	EidosAssertScriptRaise("setSeed(1); sample(1:3, 3, F, c(2.0, 3.0, NAN));", 12, "requires all weights to be");
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, F, (0:4)^3);", 12, "weights summing to");
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, F, asInteger((0:4)^3));", 12, "weights summing to");
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, T, -1:3);", 12, "requires all weights to be");
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, T, 1:6);", 12, "to be the same length");
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, T, 1);", 12, "to be the same length");
	
	// seq()
	EidosAssertScriptSuccess("seq(1, 5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("seq(5, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 4, 3, 2, 1}));
	EidosAssertScriptRaise("seq(5, 1, 0);", 0, "requires by != 0");
	EidosAssertScriptSuccess("seq(1.1, 5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.1, 2.1, 3.1, 4.1}));
	EidosAssertScriptSuccess("seq(1, 5.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("seq(5.5, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5.5, 4.5, 3.5, 2.5, 1.5}));
	EidosAssertScriptRaise("seq(5.1, 1, 0);", 0, "requires by != 0");
	EidosAssertScriptSuccess("seq(1, 10, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 5, 7, 9}));
	EidosAssertScriptRaise("seq(1, 10, -2);", 0, "has incorrect sign");
	EidosAssertScriptSuccess("seq(10, 1, -2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 8, 6, 4, 2}));
	EidosAssertScriptSuccess("(seq(1, 2, 0.2) - c(1, 1.2, 1.4, 1.6, 1.8, 2.0)) < 0.000000001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true, true, true}));
	EidosAssertScriptRaise("seq(1, 2, -0.2);", 0, "has incorrect sign");
	EidosAssertScriptSuccess("(seq(2, 1, -0.2) - c(2.0, 1.8, 1.6, 1.4, 1.2, 1)) < 0.000000001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true, true, true}));
	EidosAssertScriptRaise("seq('foo', 2, 1);", 0, "cannot be type");
	EidosAssertScriptRaise("seq(1, 'foo', 2);", 0, "cannot be type");
	EidosAssertScriptRaise("seq(2, 1, 'foo');", 0, "cannot be type");
	EidosAssertScriptRaise("seq(T, 2, 1);", 0, "cannot be type");
	EidosAssertScriptRaise("seq(1, T, 2);", 0, "cannot be type");
	EidosAssertScriptRaise("seq(2, 1, T);", 0, "cannot be type");
	EidosAssertScriptRaise("seq(NULL, 2, 1);", 0, "cannot be type");
	EidosAssertScriptRaise("seq(1, NULL, 2);", 0, "cannot be type");
	EidosAssertScriptSuccess("seq(2, 1, NULL);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 1})); // NULL uses the default by
	
	EidosAssertScriptRaise("seq(2, 3, 1, 2);", 0, "may be supplied with either");
	EidosAssertScriptRaise("seq(2, 3, by=1, length=2);", 0, "may be supplied with either");
	EidosAssertScriptRaise("seq(2, 3, length=-2);", 0, "must be > 0");
	EidosAssertScriptRaise("seq(2, 3, length=0);", 0, "must be > 0");
	EidosAssertScriptSuccess("seq(2, 3, length=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("seq(2, 3, length=2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("seq(2, 2, length=5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 2, 2, 2, 2}));
	EidosAssertScriptSuccess("seq(2, 10, length=5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 4, 6, 8, 10}));
	EidosAssertScriptSuccess("seq(2, 4, length=5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.0, 2.5, 3.0, 3.5, 4.0}));
	EidosAssertScriptSuccess("seq(3, 2, length=2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 2}));
	EidosAssertScriptSuccess("seq(10, 2, length=5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 8, 6, 4, 2}));
	EidosAssertScriptSuccess("seq(4, 2, length=5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{4.0, 3.5, 3.0, 2.5, 2.0}));
	
	EidosAssertScriptRaise("seq(2., 3, 1, 2);", 0, "may be supplied with either");
	EidosAssertScriptRaise("seq(2., 3, by=1, length=2);", 0, "may be supplied with either");
	EidosAssertScriptRaise("seq(2., 3, length=-2);", 0, "must be > 0");
	EidosAssertScriptRaise("seq(2., 3, length=0);", 0, "must be > 0");
	EidosAssertScriptSuccess("seq(2., 3, length=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2)));
	EidosAssertScriptSuccess("seq(2., 3, length=2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2, 3}));
	EidosAssertScriptSuccess("seq(2., 2, length=5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2, 2, 2, 2, 2}));
	EidosAssertScriptSuccess("seq(2., 10, length=5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2, 4, 6, 8, 10}));
	EidosAssertScriptSuccess("seq(2., 4, length=5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.0, 2.5, 3.0, 3.5, 4.0}));
	EidosAssertScriptSuccess("seq(3., 2, length=2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3, 2}));
	EidosAssertScriptSuccess("seq(10., 2, length=5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10, 8, 6, 4, 2}));
	EidosAssertScriptSuccess("seq(4., 2, length=5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{4.0, 3.5, 3.0, 2.5, 2.0}));
	
	EidosAssertScriptRaise("seq(NAN, 3.0, by=1.0);", 0, "requires a finite value");
	EidosAssertScriptRaise("seq(NAN, 3.0, length=2);", 0, "requires a finite value");
	EidosAssertScriptRaise("seq(2.0, NAN, by=1.0);", 0, "requires a finite value");
	EidosAssertScriptRaise("seq(2.0, NAN, length=2);", 0, "requires a finite value");
	EidosAssertScriptRaise("seq(2, 3, by=NAN);", 0, "requires a finite value");
	EidosAssertScriptRaise("seq(2.0, 3.0, by=NAN);", 0, "requires a finite value");
	EidosAssertScriptRaise("seq(2.0, 3.0, length=10000001);", 0, "more than 10000000 entries");
	
	// seqAlong()
	EidosAssertScriptSuccess("seqAlong(NULL);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("seqAlong(logical(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("seqAlong(object());", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("seqAlong(5);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("seqAlong(5.1);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("seqAlong('foo');", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("seqAlong(5:9);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 1, 2, 3, 4}));
	EidosAssertScriptSuccess("seqAlong(5.1:9.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 1, 2, 3, 4}));
	EidosAssertScriptSuccess("seqAlong(c('foo', 'bar', 'baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 1, 2}));
	EidosAssertScriptSuccess("seqAlong(matrix(5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0}));
	EidosAssertScriptSuccess("seqAlong(matrix(5:9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 1, 2, 3, 4}));
	
	// seqLen()
	EidosAssertScriptSuccess("seqLen(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 1, 2, 3, 4}));
	EidosAssertScriptSuccess("seqLen(1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0)));
	EidosAssertScriptSuccess("seqLen(0);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptRaise("seqLen(-1);", 0, "requires length to be");
	EidosAssertScriptRaise("seqLen(5:6);", 0, "must be a singleton");
	EidosAssertScriptRaise("seqLen('f');", 0, "cannot be type");
	
	// string()
	EidosAssertScriptSuccess("string(0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("string(1);", gStaticEidosValue_StringEmpty);
	EidosAssertScriptSuccess("string(2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"", ""}));
	EidosAssertScriptSuccess("string(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"", "", "", "", ""}));
	EidosAssertScriptRaise("string(-1);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("string(-10000);", 0, "to be greater than or equal to");
	EidosAssertScriptRaise("string(NULL);", 0, "cannot be type NULL");
	EidosAssertScriptRaise("string(integer(0));", 0, "must be a singleton");
}

#pragma mark value inspection / manipulation
void _RunFunctionValueInspectionManipulationTests_a_through_f(void)
{
	// all()
	EidosAssertScriptRaise("all(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("all(0);", 0, "cannot be type");
	EidosAssertScriptRaise("all(0.5);", 0, "cannot be type");
	EidosAssertScriptRaise("all('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("all(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("all(logical(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("all(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("all(F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("all(c(T,T,T,T,T,T,T,T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("all(c(T,T,T,T,T,T,T,F,T,T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("all(c(F,F,F,F,F,F,F,F,F,F));", gStaticEidosValue_LogicalF);
	
	EidosAssertScriptRaise("all(T, NULL);", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("all(T, 0);", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("all(T, 0.5);", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("all(T, 'foo');", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("all(T, _Test(7));", 0, "all arguments be of type logical");
	EidosAssertScriptSuccess("all(T, logical(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("all(T, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("all(T, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("all(T,T,T,T,T,T,T,T,T,T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("all(T,T,T,T,T,T,T,F,T,T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("all(F,F,F,F,F,F,F,F,F,F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("all(T,T,c(T,T,T,T),c(T,T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("all(T,T,c(T,T,T,T),c(T,F,T,T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("all(F,F,c(F,F,F,F),c(F,F,F,F));", gStaticEidosValue_LogicalF);
	
	// any()
	EidosAssertScriptRaise("any(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("any(0);", 0, "cannot be type");
	EidosAssertScriptRaise("any(0.5);", 0, "cannot be type");
	EidosAssertScriptRaise("any('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("any(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("any(logical(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("any(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("any(F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("any(c(T,T,T,T,T,T,T,T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("any(c(T,T,T,T,T,T,T,F,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("any(c(F,F,F,F,F,F,F,F,F,F));", gStaticEidosValue_LogicalF);
	
	EidosAssertScriptRaise("any(F, NULL);", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("any(F, 0);", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("any(F, 0.5);", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("any(F, 'foo');", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("any(F, _Test(7));", 0, "all arguments be of type logical");
	EidosAssertScriptSuccess("any(F, logical(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("any(F, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("any(F, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("any(T,T,T,T,T,T,T,T,T,T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("any(T,T,T,T,T,T,T,F,T,T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("any(F,F,F,F,F,F,F,F,F,F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("any(T,T,c(T,T,T,T),c(T,F,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("any(F,F,c(F,F,F,F),c(F,T,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("any(F,F,c(F,F,F,F),c(F,F,F,F));", gStaticEidosValue_LogicalF);
	
	// cat() – can't test the actual output, but we can make sure it executes...
	EidosAssertScriptRaise("cat();", 0, "missing required argument x");
	EidosAssertScriptSuccess("cat(NULL);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(T);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(5);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(5.5);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat('foo');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(_Test(7));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(NULL, '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(T, '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(5, '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(5.5, '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat('foo', '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(_Test(7), '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(c(T,T,F,T), '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(5:9, '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(5.5:8.9, '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(c('foo', 'bar', 'baz'), '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("cat(c(_Test(7), _Test(7), _Test(7)), '$$');", gStaticEidosValueVOID);
	
	// catn() – can't test the actual output, but we can make sure it executes...
	EidosAssertScriptSuccess("catn();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(NULL);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(T);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(5);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(5.5);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn('foo');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(_Test(7));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(NULL, '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(T, '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(5, '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(5.5, '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn('foo', '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(_Test(7), '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(c(T,T,F,T), '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(5:9, '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(5.5:8.9, '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(c('foo', 'bar', 'baz'), '$$');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("catn(c(_Test(7), _Test(7), _Test(7)), '$$');", gStaticEidosValueVOID);
	
	// format()
	EidosAssertScriptRaise("format('%d', NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("format('%d', T);", 0, "cannot be type");
	EidosAssertScriptSuccess("format('%d', 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("0")));
	EidosAssertScriptSuccess("format('%f', 0.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("0.500000")));
	EidosAssertScriptRaise("format('%d', 'foo');", 0, "cannot be type");
	EidosAssertScriptRaise("format('%d', _Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("format('%d', 0.5);", 0, "requires an argument of type integer");
	EidosAssertScriptRaise("format('%f', 5);", 0, "requires an argument of type float");
	EidosAssertScriptSuccess("format('foo == %d', 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo == 0")));
	EidosAssertScriptRaise("format('%++d', 8:12);", 0, "flag '+' specified");
	EidosAssertScriptRaise("format('%--d', 8:12);", 0, "flag '-' specified");
	EidosAssertScriptRaise("format('%  d', 8:12);", 0, "flag ' ' specified");
	EidosAssertScriptRaise("format('%00d', 8:12);", 0, "flag '0' specified");
	EidosAssertScriptRaise("format('%##d', 8:12);", 0, "flag '#' specified");
	EidosAssertScriptSuccess("format('%d', 8:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"8","9","10","11","12"}));
	EidosAssertScriptSuccess("format('%3d', 8:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"  8","  9"," 10"," 11"," 12"}));
	EidosAssertScriptSuccess("format('%10d', 8:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"         8","         9","        10","        11","        12"}));
	EidosAssertScriptSuccess("format('%-3d', 8:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"8  ","9  ","10 ","11 ","12 "}));
	EidosAssertScriptSuccess("format('%- 3d', 8:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{" 8 "," 9 "," 10"," 11"," 12"}));
	EidosAssertScriptSuccess("format('%+3d', 8:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{" +8"," +9","+10","+11","+12"}));
	EidosAssertScriptSuccess("format('%+-3d', 8:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"+8 ","+9 ","+10","+11","+12"}));
	EidosAssertScriptSuccess("format('%+03d', 8:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"+08","+09","+10","+11","+12"}));
	EidosAssertScriptSuccess("format('%i', 8:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"8","9","10","11","12"}));
	EidosAssertScriptSuccess("format('%o', 8:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"10","11","12","13","14"}));
	EidosAssertScriptSuccess("format('%x', 8:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"8","9","a","b","c"}));
	EidosAssertScriptSuccess("format('%X', 8:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"8","9","A","B","C"}));
	EidosAssertScriptRaise("format('%#d', 8:12);", 0, "the flag '#' may not be used with");
	EidosAssertScriptRaise("format('%n', 8:12);", 0, "conversion specifier 'n' not supported");
	EidosAssertScriptRaise("format('%', 8:12);", 0, "missing conversion specifier after '%'");
	EidosAssertScriptRaise("format('%d%d', 8:12);", 0, "only one % escape is allowed");
	EidosAssertScriptRaise("format('%d%', 8:12);", 0, "only one % escape is allowed");
	EidosAssertScriptSuccess("format('%%%d%%', 8:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"%8%","%9%","%10%","%11%","%12%"}));
	EidosAssertScriptSuccess("format('%f', 8.0:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"8.000000","9.000000","10.000000","11.000000","12.000000"}));
	EidosAssertScriptSuccess("format('%.2f', 8.0:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"8.00","9.00","10.00","11.00","12.00"}));
	EidosAssertScriptSuccess("format('%8.2f', 8.0:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"    8.00","    9.00","   10.00","   11.00","   12.00"}));
	EidosAssertScriptSuccess("format('%+8.2f', 8.0:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"   +8.00","   +9.00","  +10.00","  +11.00","  +12.00"}));
	EidosAssertScriptSuccess("format('%+08.2f', 8.0:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"+0008.00","+0009.00","+0010.00","+0011.00","+0012.00"}));
	EidosAssertScriptSuccess("format('%-8.2f', 8.0:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"8.00    ","9.00    ","10.00   ","11.00   ","12.00   "}));
	EidosAssertScriptSuccess("format('%- 8.2f', 8.0:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{" 8.00   "," 9.00   "," 10.00  "," 11.00  "," 12.00  "}));
	EidosAssertScriptSuccess("format('%8.2F', 8.0:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"    8.00","    9.00","   10.00","   11.00","   12.00"}));
	EidosAssertScriptSuccess("format('%8.2e', 8.0:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"8.00e+00", "9.00e+00", "1.00e+01", "1.10e+01", "1.20e+01"}));
	EidosAssertScriptSuccess("format('%8.2E', 8.0:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"8.00E+00", "9.00E+00", "1.00E+01", "1.10E+01", "1.20E+01"}));
	EidosAssertScriptSuccess("format('%8.2g', 8.0:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"       8","       9","      10","      11","      12"}));
	EidosAssertScriptSuccess("format('%#8.2g', 8.0:12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"     8.0","     9.0","     10.","     11.","     12."}));
}

void _RunFunctionValueInspectionManipulationTests_g_through_l(void)
{
	// identical()
	EidosAssertScriptSuccess("identical(NULL, NULL);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(NULL, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(NULL, 0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(NULL, 0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(NULL, '');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(NULL, _Test(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(F, NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(F, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F, T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(F, 0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(F, 0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(F, '');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(F, _Test(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0, NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0, 0);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(0, 1);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0, 0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0, '');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0, _Test(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0.0, NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0.0, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0.0, 0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0.0, 0.0);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(0.0, 0.1);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0.0, '');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(0.0, _Test(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical('', NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical('', F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical('', 0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical('', 0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical('', '');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical('', 'x');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical('', _Test(0));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(0), NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(0), F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(0), 0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(0), 0.0);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(0), '');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(0), _Test(0));", gStaticEidosValue_LogicalF);	// object elements not equal
	EidosAssertScriptSuccess("identical(F, c(F,F));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(F,F), F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(F,F), c(F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(F,T,F,T,T), c(F,T,F,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(F,T,T,T,T), c(F,T,F,T,T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(3, c(3,3));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(3,3), 3);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(3,3), c(3,3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(3,7,3,7,7), c(3,7,3,7,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(3,7,7,7,7), c(3,7,3,7,7));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(3.1, c(3.1,3.1));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(3.1,3.1), 3.1);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(3.1,3.1), c(3.1,3.1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(3.1,7.1,3.1,7.1,7.1), c(3.1,7.1,3.1,7.1,7.1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(3.1,7.1,7.1,7.1,7.1), c(3.1,7.1,3.1,7.1,7.1));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical('bar', c('bar','bar'));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c('bar','bar'), 'bar');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c('bar','bar'), c('bar','bar'));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c('bar','baz','bar','baz','baz'), c('bar','baz','bar','baz','baz'));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c('bar','baz','baz','baz','baz'), c('bar','baz','bar','baz','baz'));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(_Test(3), c(_Test(3),_Test(3)));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(_Test(3),_Test(3)), _Test(3));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(_Test(3),_Test(3)), c(_Test(3),_Test(3)));", gStaticEidosValue_LogicalF);	// object elements not equal
	EidosAssertScriptSuccess("x = c(_Test(3),_Test(3)); y = x; identical(x, y);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = _Test(3); y = _Test(7); identical(c(x, y, x, x), c(x, y, x, x));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = _Test(3); y = _Test(7); identical(c(x, y, x, x), c(x, y, y, x));", gStaticEidosValue_LogicalF);
	
	EidosAssertScriptSuccess("identical(matrix(F), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F), matrix(F, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F)), matrix(c(F,T,F,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F)), matrix(c(F,T,F,F), byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), byrow=T), matrix(c(F,T,F,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), byrow=T), matrix(c(F,T,F,F), byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), nrow=1), matrix(c(F,T,F,F), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), ncol=1), matrix(c(F,T,F,F), ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), nrow=2), matrix(c(F,T,F,F), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), ncol=2), matrix(c(F,T,F,F), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), ncol=2), matrix(c(F,T,F,F), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), nrow=2), matrix(c(F,T,F,F), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), nrow=2, byrow=T), matrix(c(F,T,F,F), nrow=2));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), nrow=2), matrix(c(F,T,F,F), nrow=2, byrow=T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), nrow=2, byrow=T), matrix(c(F,T,F,F), nrow=2, byrow=T));", gStaticEidosValue_LogicalT);
 	EidosAssertScriptSuccess("identical(F, matrix(F));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(F, matrix(F, byrow=T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(F), F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(F, byrow=T), F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(F,T,F,F), matrix(c(F,T,F,F)));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(F,T,F,F), matrix(c(F,T,F,F), nrow=1));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(F,T,F,F), matrix(c(F,T,F,F), ncol=1));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F)), c(F,T,F,F));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), nrow=1), c(F,T,F,F));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), ncol=1), c(F,T,F,F));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), nrow=1), matrix(c(F,T,F,F), ncol=1));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F,F), ncol=1), matrix(c(F,T,F,F), nrow=1));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(6), matrix(6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(6), matrix(6, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6)), matrix(c(6,8,6,6)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6)), matrix(c(6,8,6,6), byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), byrow=T), matrix(c(6,8,6,6)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), byrow=T), matrix(c(6,8,6,6), byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), nrow=1), matrix(c(6,8,6,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), ncol=1), matrix(c(6,8,6,6), ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), nrow=2), matrix(c(6,8,6,6), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), ncol=2), matrix(c(6,8,6,6), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), ncol=2), matrix(c(6,8,6,6), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), nrow=2), matrix(c(6,8,6,6), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), nrow=2, byrow=T), matrix(c(6,8,6,6), nrow=2));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), nrow=2), matrix(c(6,8,6,6), nrow=2, byrow=T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), nrow=2, byrow=T), matrix(c(6,8,6,6), nrow=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(6, matrix(6));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(6, matrix(6, byrow=T));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(6), 6);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(6, byrow=T), 6);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(6,8,6,6), matrix(c(6,8,6,6)));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(6,8,6,6), matrix(c(6,8,6,6), nrow=1));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(c(6,8,6,6), matrix(c(6,8,6,6), ncol=1));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6)), c(6,8,6,6));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), nrow=1), c(6,8,6,6));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), ncol=1), c(6,8,6,6));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), nrow=1), matrix(c(6,8,6,6), ncol=1));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("identical(matrix(c(6,8,6,6), ncol=1), matrix(c(6,8,6,6), nrow=1));", gStaticEidosValue_LogicalF);
	
	// ifelse()
	EidosAssertScriptRaise("ifelse(NULL, integer(0), integer(0));", 0, "cannot be type");
	EidosAssertScriptRaise("ifelse(logical(0), NULL, integer(0));", 0, "to be the same type");
	EidosAssertScriptRaise("ifelse(logical(0), integer(0), NULL);", 0, "to be the same type");
	EidosAssertScriptSuccess("ifelse(logical(0), logical(0), logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("ifelse(logical(0), integer(0), integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("ifelse(logical(0), float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("ifelse(logical(0), string(0), string(0));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("ifelse(logical(0), object(), object());", gStaticEidosValue_Object_ZeroVec);
	EidosAssertScriptSuccess("ifelse(logical(0), T, F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("ifelse(logical(0), 0, 1);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("ifelse(logical(0), 0.0, 1.0);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("ifelse(logical(0), 'foo', 'bar');", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("ifelse(logical(0), _Test(0), _Test(1))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptRaise("ifelse(logical(0), 5:6, 2);", 0, "trueValues and falseValues each be either");
	EidosAssertScriptRaise("ifelse(logical(0), 5, 2:3);", 0, "trueValues and falseValues each be either");
	EidosAssertScriptRaise("ifelse(T, integer(0), integer(0));", 0, "trueValues and falseValues each be either");
	EidosAssertScriptRaise("ifelse(T, 5, 2:3);", 0, "trueValues and falseValues each be either");
	EidosAssertScriptRaise("ifelse(T, 5:6, 2);", 0, "trueValues and falseValues each be either");
	EidosAssertScriptRaise("ifelse(c(T,T), 5:7, 2);", 0, "trueValues and falseValues each be either");
	EidosAssertScriptRaise("ifelse(c(T,T), 5, 2:4);", 0, "trueValues and falseValues each be either");
	EidosAssertScriptRaise("ifelse(c(T,T), 5:7, 2:4);", 0, "trueValues and falseValues each be either");
	
	EidosAssertScriptSuccess("ifelse(logical(0), T, F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("ifelse(T, T, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("ifelse(F, T, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("ifelse(T, F, T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("ifelse(F, F, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("ifelse(c(T,T), T, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("ifelse(c(F,F), T, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	EidosAssertScriptSuccess("ifelse(c(T,F), F, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("ifelse(c(F,T), F, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("ifelse(c(T,T), c(T,F), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("ifelse(c(T,T), F, c(T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	EidosAssertScriptSuccess("ifelse(c(F,F), c(T,F), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("ifelse(c(F,F), T, c(T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("ifelse(c(T,T), c(T,F), c(F,T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("ifelse(c(F,F), c(T,F), c(F,T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptSuccess("ifelse(c(T,F), c(T,F), c(F,T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("ifelse(c(F,T), c(T,F), c(F,T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false}));
	EidosAssertScriptSuccess("ifelse(c(T,F,F,T,F,T), rep(T,6), rep(F,6));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, true, false, true}));
	EidosAssertScriptSuccess("ifelse(c(T,F,F,T,F,T), rep(F,6), rep(T,6));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, true, false, true, false}));
	
	EidosAssertScriptSuccess("ifelse(logical(0), 5, 2);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("ifelse(T, 5, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("ifelse(F, 5, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("ifelse(c(T,T), 5, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 5}));
	EidosAssertScriptSuccess("ifelse(c(F,F), 5, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 2}));
	EidosAssertScriptSuccess("ifelse(c(T,F), 5, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 2}));
	EidosAssertScriptSuccess("ifelse(c(T,T), 5:6, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 6}));
	EidosAssertScriptSuccess("ifelse(c(T,T), 5, 2:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 5}));
	EidosAssertScriptSuccess("ifelse(c(F,F), 5:6, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 2}));
	EidosAssertScriptSuccess("ifelse(c(F,F), 5, 2:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("ifelse(c(T,T), 5:6, 2:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 6}));
	EidosAssertScriptSuccess("ifelse(c(F,F), 5:6, 2:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("ifelse(c(T,F), 5:6, 2:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 3}));
	EidosAssertScriptSuccess("ifelse(c(T,F,F,T,F,T), 1:6, -6:-1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -5, -4, 4, -2, 6}));
	
	EidosAssertScriptSuccess("ifelse(logical(0), 5.3, 2.1);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("ifelse(T, 5.3, 2.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(5.3)));
	EidosAssertScriptSuccess("ifelse(F, 5.3, 2.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.1)));
	EidosAssertScriptSuccess("ifelse(c(T,T), 5.3, 2.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5.3, 5.3}));
	EidosAssertScriptSuccess("ifelse(c(F,F), 5.3, 2.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.1, 2.1}));
	EidosAssertScriptSuccess("ifelse(c(T,F), 5.3, 2.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5.3, 2.1}));
	EidosAssertScriptSuccess("ifelse(c(T,T), c(5.3, 6.3), 2.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5.3, 6.3}));
	EidosAssertScriptSuccess("ifelse(c(T,T), 5.3, c(2.1, 3.1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5.3, 5.3}));
	EidosAssertScriptSuccess("ifelse(c(F,F), c(5.3, 6.3), 2.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.1, 2.1}));
	EidosAssertScriptSuccess("ifelse(c(F,F), 5.3, c(2.1, 3.1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.1, 3.1}));
	EidosAssertScriptSuccess("ifelse(c(T,T), c(5.3, 6.3), c(2.1, 3.1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5.3, 6.3}));
	EidosAssertScriptSuccess("ifelse(c(F,F), c(5.3, 6.3), c(2.1, 3.1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.1, 3.1}));
	EidosAssertScriptSuccess("ifelse(c(T,F), c(5.3, 6.3), c(2.1, 3.1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5.3, 3.1}));
	EidosAssertScriptSuccess("ifelse(c(T,F,F,T,F,T), 1.0:6.0, -6.0:-1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, -5.0, -4.0, 4.0, -2.0, 6.0}));
	
	EidosAssertScriptSuccess("ifelse(logical(0), 'foo', 'bar');", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("ifelse(T, 'foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("ifelse(F, 'foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptSuccess("ifelse(c(T,T), 'foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "foo"}));
	EidosAssertScriptSuccess("ifelse(c(F,F), 'foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"bar", "bar"}));
	EidosAssertScriptSuccess("ifelse(c(T,F), 'foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar"}));
	EidosAssertScriptSuccess("ifelse(c(T,T), c('foo', 'baz'), 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "baz"}));
	EidosAssertScriptSuccess("ifelse(c(T,T), 'foo', c('bar', 'xyzzy'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "foo"}));
	EidosAssertScriptSuccess("ifelse(c(F,F), c('foo', 'baz'), 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"bar", "bar"}));
	EidosAssertScriptSuccess("ifelse(c(F,F), 'foo', c('bar', 'xyzzy'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"bar", "xyzzy"}));
	EidosAssertScriptSuccess("ifelse(c(T,T), c('foo', 'baz'), c('bar', 'xyzzy'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "baz"}));
	EidosAssertScriptSuccess("ifelse(c(F,F), c('foo', 'baz'), c('bar', 'xyzzy'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"bar", "xyzzy"}));
	EidosAssertScriptSuccess("ifelse(c(T,F), c('foo', 'baz'), c('bar', 'xyzzy'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "xyzzy"}));
	EidosAssertScriptSuccess("ifelse(c(T,F,F,T,F,T), c('a','b','c','d','e','f'), c('A','B','C','D','E','F'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"a", "B", "C", "d", "E", "f"}));
	
	EidosAssertScriptSuccess("ifelse(logical(0), _Test(5), _Test(2))._yolk;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("ifelse(T, _Test(5), _Test(2))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("ifelse(F, _Test(5), _Test(2))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("ifelse(c(T,T), _Test(5), _Test(2))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 5}));
	EidosAssertScriptSuccess("ifelse(c(F,F), _Test(5), _Test(2))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 2}));
	EidosAssertScriptSuccess("ifelse(c(T,F), _Test(5), _Test(2))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 2}));
	EidosAssertScriptSuccess("ifelse(c(T,T), c(_Test(5),_Test(6)), _Test(2))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 6}));
	EidosAssertScriptSuccess("ifelse(c(T,T), _Test(5), c(_Test(2),_Test(3)))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 5}));
	EidosAssertScriptSuccess("ifelse(c(F,F), c(_Test(5),_Test(6)), _Test(2))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 2}));
	EidosAssertScriptSuccess("ifelse(c(F,F), _Test(5), c(_Test(2),_Test(3)))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("ifelse(c(T,T), c(_Test(5),_Test(6)), c(_Test(2),_Test(3)))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 6}));
	EidosAssertScriptSuccess("ifelse(c(F,F), c(_Test(5),_Test(6)), c(_Test(2),_Test(3)))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("ifelse(c(T,F), c(_Test(5),_Test(6)), c(_Test(2),_Test(3)))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 3}));
	EidosAssertScriptSuccess("ifelse(c(T,F,F,T,F,T), c(_Test(1), _Test(2), _Test(3), _Test(4), _Test(5), _Test(6)), c(_Test(-6), _Test(-5), _Test(-4), _Test(-3), _Test(-2), _Test(-1)))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -5, -4, 4, -2, 6}));
	
	EidosAssertScriptSuccess("identical(ifelse(matrix(T), 5, 2), matrix(5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ifelse(matrix(F), 5, 2), matrix(2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ifelse(matrix(c(T,T), nrow=1), 5, 2), matrix(c(5,5), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ifelse(matrix(c(F,F), ncol=1), 5, 2), matrix(c(2,2), ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ifelse(array(c(T,F), c(1,2,1)), 5, 2), array(c(5,2), c(1,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ifelse(matrix(c(T,T), nrow=1), 5:6, 2), matrix(c(5,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ifelse(matrix(c(T,T), ncol=1), 5, 2:3), matrix(c(5,5), ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ifelse(array(c(F,F), c(2,1,1)), 5:6, 2), array(c(2,2), c(2,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ifelse(array(c(F,F), c(1,1,2)), 5, 2:3), array(c(2,3), c(1,1,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ifelse(matrix(c(T,T), nrow=1), 5:6, 2:3), matrix(c(5,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ifelse(matrix(c(F,F), ncol=1), 5:6, 2:3), matrix(c(2,3), ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ifelse(array(c(T,F), c(1,2,1)), 5:6, 2:3), array(c(5,3), c(1,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(ifelse(matrix(c(T,F,F,T,F,T), nrow=2), 1:6, -6:-1), matrix(c(1,-5,-4,4,-2,6), nrow=2));", gStaticEidosValue_LogicalT);
}

void _RunFunctionValueInspectionManipulationTests_m_through_r(void)
{
	// match()
	EidosAssertScriptSuccess("match(NULL, NULL);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptRaise("match(NULL, F);", 0, "to be the same type");
	EidosAssertScriptRaise("match(NULL, 0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(NULL, 0.0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(NULL, '');", 0, "to be the same type");
	EidosAssertScriptRaise("match(NULL, _Test(0));", 0, "to be the same type");
	EidosAssertScriptRaise("match(F, NULL);", 0, "to be the same type");
	EidosAssertScriptSuccess("match(F, F);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("match(F, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptRaise("match(F, 0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(F, 0.0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(F, '');", 0, "to be the same type");
	EidosAssertScriptRaise("match(F, _Test(0));", 0, "to be the same type");
	EidosAssertScriptRaise("match(0, NULL);", 0, "to be the same type");
	EidosAssertScriptRaise("match(0, F);", 0, "to be the same type");
	EidosAssertScriptSuccess("match(0, 0);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("match(0, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptRaise("match(0, 0.0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(0, '');", 0, "to be the same type");
	EidosAssertScriptRaise("match(0, _Test(0));", 0, "to be the same type");
	EidosAssertScriptRaise("match(0.0, NULL);", 0, "to be the same type");
	EidosAssertScriptRaise("match(0.0, F);", 0, "to be the same type");
	EidosAssertScriptRaise("match(0.0, 0);", 0, "to be the same type");
	EidosAssertScriptSuccess("match(0.0, 0.0);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("match(0.0, 0.1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptRaise("match(0.0, '');", 0, "to be the same type");
	EidosAssertScriptRaise("match(0.0, _Test(0));", 0, "to be the same type");
	EidosAssertScriptRaise("match('', NULL);", 0, "to be the same type");
	EidosAssertScriptRaise("match('', F);", 0, "to be the same type");
	EidosAssertScriptRaise("match('', 0);", 0, "to be the same type");
	EidosAssertScriptRaise("match('', 0.0);", 0, "to be the same type");
	EidosAssertScriptSuccess("match('', '');", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("match('', 'f');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptRaise("match('', _Test(0));", 0, "to be the same type");
	EidosAssertScriptRaise("match(_Test(0), NULL);", 0, "to be the same type");
	EidosAssertScriptRaise("match(_Test(0), F);", 0, "to be the same type");
	EidosAssertScriptRaise("match(_Test(0), 0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(_Test(0), 0.0);", 0, "to be the same type");
	EidosAssertScriptRaise("match(_Test(0), '');", 0, "to be the same type");
	EidosAssertScriptSuccess("match(_Test(0), _Test(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));							// different elements
	EidosAssertScriptSuccess("x = _Test(0); match(x, x);", gStaticEidosValue_Integer0);
	
	EidosAssertScriptSuccess("match(c(F,T,F,F,T,T), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1, 0, -1, -1, 0, 0}));
	EidosAssertScriptSuccess("match(c(1,2,2,9,5,1), 5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1, -1, -1, -1, 0, -1}));
	EidosAssertScriptSuccess("match(c(1,2,2,9,5,1.), 5.);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1, -1, -1, -1, 0, -1}));
	EidosAssertScriptSuccess("match(c('bar','q','f','baz','foo','bar'), 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1, -1, -1, -1, 0, -1}));
	EidosAssertScriptSuccess("match(c(_Test(0), _Test(1)), _Test(0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1, -1}));				// different elements
	EidosAssertScriptSuccess("x1 = _Test(1); x2 = _Test(2); x9 = _Test(9); x5 = _Test(5); match(c(x1,x2,x2,x9,x5,x1), x5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1, -1, -1, -1, 0, -1}));
	
	EidosAssertScriptSuccess("match(F, c(T,F));", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("match(9, c(5,1,9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("match(9., c(5,1,9.));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("match('baz', c('foo','bar','baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("match(_Test(0), c(_Test(0), _Test(1)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));	// different elements
	EidosAssertScriptSuccess("x1 = _Test(1); x2 = _Test(2); x9 = _Test(9); x5 = _Test(5); match(c(x9), c(x5,x1,x9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	
	EidosAssertScriptSuccess("match(F, c(T,T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("match(7, c(5,1,9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("match(7., c(5,1,9.));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("match('zip', c('foo','bar','baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("match(_Test(7), c(_Test(0), _Test(1)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));	// different elements
	EidosAssertScriptSuccess("x1 = _Test(1); x2 = _Test(2); x9 = _Test(9); x5 = _Test(5); match(c(x2), c(x5,x1,x9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	
	EidosAssertScriptSuccess("match(c(F,T,F,F,T,T), c(T,T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1, 0, -1, -1, 0, 0}));
	EidosAssertScriptSuccess("match(c(1,2,2,9,5,1), c(5,1,9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -1, -1, 2, 0, 1}));
	EidosAssertScriptSuccess("match(c(1,2,2,9,5,1.), c(5,1,9.));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -1, -1, 2, 0, 1}));
	EidosAssertScriptSuccess("match(c('bar','q','f','baz','foo','bar'), c('foo','bar','baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -1, -1, 2, 0, 1}));
	EidosAssertScriptSuccess("match(c(_Test(0), _Test(1)), c(_Test(0), _Test(1)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1, -1}));	// different elements
	EidosAssertScriptSuccess("x1 = _Test(1); x2 = _Test(2); x9 = _Test(9); x5 = _Test(5); match(c(x1,x2,x2,x9,x5,x1), c(x5,x1,x9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -1, -1, 2, 0, 1}));
	
	// nchar()
	EidosAssertScriptRaise("nchar(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("nchar(T);", 0, "cannot be type");
	EidosAssertScriptRaise("nchar(5);", 0, "cannot be type");
	EidosAssertScriptRaise("nchar(5.5);", 0, "cannot be type");
	EidosAssertScriptRaise("nchar(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("nchar('');", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("nchar(' ');", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("nchar('abcde');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("nchar('abc\tde');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(6)));
	EidosAssertScriptSuccess("nchar(c('', 'abcde', '', 'wumpus'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 5, 0, 6}));
	
	EidosAssertScriptSuccess("identical(nchar(matrix('abc\tde')), matrix(nchar('abc\tde')));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(nchar(matrix(c('', 'abcde', '', 'wumpus'), nrow=2)), matrix(nchar(c('', 'abcde', '', 'wumpus')), nrow=2));", gStaticEidosValue_LogicalT);
	
	// order()
	EidosAssertScriptSuccess("order(integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("order(integer(0), T);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("order(integer(0), F);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("order(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0)));
	EidosAssertScriptSuccess("order(3, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0)));
	EidosAssertScriptSuccess("order(3, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0)));
	EidosAssertScriptSuccess("order(c(6, 19, -3, 5, 2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 4, 3, 0, 1}));
	EidosAssertScriptSuccess("order(c(6, 19, -3, 5, 2), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 4, 3, 0, 1}));
	EidosAssertScriptSuccess("order(c(2, 5, -3, 19, 6), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 0, 1, 4, 3}));
	EidosAssertScriptSuccess("order(c(6, 19, -3, 5, 2), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 0, 3, 4, 2}));
	EidosAssertScriptSuccess("order(c(2, 5, -3, 19, 6), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4, 1, 0, 2}));
	EidosAssertScriptSuccess("order(c(T, F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 0}));
	EidosAssertScriptSuccess("order(c(6.1, 19.3, -3.7, 5.2, 2.3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 4, 3, 0, 1}));
	EidosAssertScriptSuccess("order(c('a', 'q', 'm', 'f', 'w'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 3, 2, 1, 4}));
	EidosAssertScriptRaise("order(_Test(7));", 0, "cannot be type");
	
	// paste()
	EidosAssertScriptSuccess("paste(NULL);", gStaticEidosValue_StringEmpty);
	EidosAssertScriptSuccess("paste(T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("T")));
	EidosAssertScriptSuccess("paste(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5")));
	EidosAssertScriptSuccess("paste(5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5.5")));
	EidosAssertScriptSuccess("paste('foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("paste(_Test(7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("_TestElement")));
	EidosAssertScriptSuccess("paste(NULL, '$$');", gStaticEidosValue_StringEmpty);
	EidosAssertScriptSuccess("paste(T, '$$');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("T")));
	EidosAssertScriptSuccess("paste(5, '$$');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5")));
	EidosAssertScriptSuccess("paste(5.5, '$$');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5.5")));
	EidosAssertScriptSuccess("paste('foo', '$$');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("paste(_Test(7), '$$');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("_TestElement")));
	EidosAssertScriptSuccess("paste(c(T,T,F,T), '$$');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("T$$T$$F$$T")));
	EidosAssertScriptSuccess("paste(5:9, '$$');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5$$6$$7$$8$$9")));
	EidosAssertScriptSuccess("paste(5.5:8.9, '$$');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5.5$$6.5$$7.5$$8.5")));
	EidosAssertScriptSuccess("paste(c('foo', 'bar', 'baz'), '$$');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo$$bar$$baz")));
	EidosAssertScriptSuccess("paste(c(_Test(7), _Test(7), _Test(7)), '$$');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("_TestElement$$_TestElement$$_TestElement")));
	
	// paste0()
	EidosAssertScriptSuccess("paste0(NULL);", gStaticEidosValue_StringEmpty);
	EidosAssertScriptSuccess("paste0(T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("T")));
	EidosAssertScriptSuccess("paste0(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5")));
	EidosAssertScriptSuccess("paste0(5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5.5")));
	EidosAssertScriptSuccess("paste0('foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("paste0(_Test(7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("_TestElement")));
	EidosAssertScriptSuccess("paste0(NULL);", gStaticEidosValue_StringEmpty);
	EidosAssertScriptSuccess("paste0(T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("T")));
	EidosAssertScriptSuccess("paste0(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5")));
	EidosAssertScriptSuccess("paste0(5.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5.5")));
	EidosAssertScriptSuccess("paste0('foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("paste0(_Test(7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("_TestElement")));
	EidosAssertScriptSuccess("paste0(c(T,T,F,T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("TTFT")));
	EidosAssertScriptSuccess("paste0(5:9);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("56789")));
	EidosAssertScriptSuccess("paste0(5.5:8.9);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("5.56.57.58.5")));
	EidosAssertScriptSuccess("paste0(c('foo', 'bar', 'baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foobarbaz")));
	EidosAssertScriptSuccess("paste0(c(_Test(7), _Test(7), _Test(7)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("_TestElement_TestElement_TestElement")));
	
	// print()
	EidosAssertScriptSuccess("print(NULL);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("print(T);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("print(5);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("print(5.5);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("print('foo');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("print(_Test(7));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("print(c(T,T,F,T));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("print(5:9);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("print(5.5:8.9);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("print(c('foo', 'bar', 'baz'));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("print(c(_Test(7), _Test(7), _Test(7)));", gStaticEidosValueVOID);
	
	// rev()
	EidosAssertScriptSuccess("rev(6:10);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10,9,8,7,6}));
	EidosAssertScriptSuccess("rev(-(6:10));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-10,-9,-8,-7,-6}));
	EidosAssertScriptSuccess("rev(c('foo','bar','baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"baz","bar","foo"}));
	EidosAssertScriptSuccess("rev(-1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("rev(1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
	EidosAssertScriptSuccess("rev('foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("rev(6.0:10);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10,9,8,7,6}));
	EidosAssertScriptSuccess("rev(c(T,T,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, true, true}));
}

void _RunFunctionValueInspectionManipulationTests_s_through_z(void)
{
	// size() / length()
	EidosAssertScriptSuccess("size(NULL);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("size(logical(0));", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("size(5);", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("size(c(5.5, 8.7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("size(c('foo', 'bar', 'baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("size(rep(_Test(7), 4));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(4)));
	
	EidosAssertScriptSuccess("length(NULL);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("length(logical(0));", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("length(5);", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("length(c(5.5, 8.7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("length(c('foo', 'bar', 'baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("length(rep(_Test(7), 4));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(4)));
	
	// sort()
	EidosAssertScriptSuccess("sort(integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("sort(integer(0), T);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("sort(integer(0), F);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("sort(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("sort(3, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("sort(3, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("sort(c(6, 19, -3, 5, 2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-3, 2, 5, 6, 19}));
	EidosAssertScriptSuccess("sort(c(6, 19, -3, 5, 2), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-3, 2, 5, 6, 19}));
	EidosAssertScriptSuccess("sort(c(6, 19, -3, 5, 2), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{19, 6, 5, 2, -3}));
	EidosAssertScriptSuccess("sort(c(T, F, T, T, F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, true, true}));
	EidosAssertScriptSuccess("sort(c(6.1, 19.3, -3.7, 5.2, 2.3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-3.7, 2.3, 5.2, 6.1, 19.3}));
	EidosAssertScriptSuccess("sort(c('a', 'q', 'm', 'f', 'w'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"a", "f", "m", "q", "w"}));
	EidosAssertScriptRaise("sort(_Test(7));", 0, "cannot be type");
	
	// sortBy()
	EidosAssertScriptRaise("sortBy(NULL);", 0, "missing required argument");
	EidosAssertScriptRaise("sortBy(T);", 0, "missing required argument");
	EidosAssertScriptRaise("sortBy(5);", 0, "missing required argument");
	EidosAssertScriptRaise("sortBy(9.1);", 0, "missing required argument");
	EidosAssertScriptRaise("sortBy('foo');", 0, "missing required argument");
	EidosAssertScriptRaise("sortBy(NULL, 'foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sortBy(T, 'foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sortBy(5, 'foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sortBy(9.1, 'foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sortBy('foo', 'foo');", 0, "cannot be type");
	EidosAssertScriptSuccess("sortBy(object(), 'foo');", gStaticEidosValue_Object_ZeroVec);
	EidosAssertScriptSuccess("sortBy(c(_Test(7), _Test(2), _Test(-8), _Test(3), _Test(75)), '_yolk')._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-8, 2, 3, 7, 75}));
	EidosAssertScriptSuccess("sortBy(c(_Test(7), _Test(2), _Test(-8), _Test(3), _Test(75)), '_yolk', T)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-8, 2, 3, 7, 75}));
	EidosAssertScriptSuccess("sortBy(c(_Test(7), _Test(2), _Test(-8), _Test(3), _Test(75)), '_yolk', F)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{75, 7, 3, 2, -8}));
	EidosAssertScriptRaise("sortBy(c(_Test(7), _Test(2), _Test(-8), _Test(3), _Test(75)), '_foo')._yolk;", 0, "attempt to get a value");
	
	// str() – can't test the actual output, but we can make sure it executes...
	EidosAssertScriptSuccess("str(NULL);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(logical(0));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(T);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(c(T,F,F,T));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(matrix(T));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(matrix(c(T,F,F,T)));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(integer(0));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(5);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(5:8);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(matrix(5));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(matrix(5:8));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(float(0));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(5.9);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(5.9:8);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(matrix(5.9));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(matrix(5.9:8));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(string(0));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str('foo');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(c('foo', 'bar', 'baz'));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(matrix('foo'));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(matrix(c('foo', 'bar', 'baz')));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(object());", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(_Test(7));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(c(_Test(7), _Test(8), _Test(9)));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(matrix(_Test(7)));", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("str(matrix(c(_Test(7), _Test(8), _Test(9))));", gStaticEidosValueVOID);
	
	// strsplit()
	EidosAssertScriptRaise("strsplit(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("strsplit(T);", 0, "cannot be type");
	EidosAssertScriptRaise("strsplit(5);", 0, "cannot be type");
	EidosAssertScriptRaise("strsplit(5.6);", 0, "cannot be type");
	EidosAssertScriptRaise("strsplit(string(0));", 0, "must be a singleton");
	EidosAssertScriptRaise("strsplit(string(0), '$$');", 0, "must be a singleton");
	EidosAssertScriptRaise("strsplit(c('foo', 'bar'));", 0, "must be a singleton");
	EidosAssertScriptRaise("strsplit(c('foo', 'bar'), '$$');", 0, "must be a singleton");
	EidosAssertScriptSuccess("strsplit('');", gStaticEidosValue_StringEmpty);
	EidosAssertScriptSuccess("strsplit('', '$$');", gStaticEidosValue_StringEmpty);
	EidosAssertScriptSuccess("strsplit(' ');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"", ""}));
	EidosAssertScriptSuccess("strsplit('$$', '$$');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"", ""}));
	EidosAssertScriptSuccess("strsplit('  ');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"", "", ""}));
	EidosAssertScriptSuccess("strsplit('$$$$', '$$');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"", "", ""}));
	EidosAssertScriptSuccess("strsplit('$$$$', '');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"$", "$", "$", "$"}));
	EidosAssertScriptSuccess("strsplit('This is a test.');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"This", "is", "a", "test."}));
	EidosAssertScriptSuccess("strsplit('This is a test.', '$$');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("This is a test.")));
	EidosAssertScriptSuccess("strsplit('This is a test.', 'i');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"Th", "s ", "s a test."}));
	EidosAssertScriptSuccess("strsplit('This is a test.', 's');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"Thi", " i", " a te", "t."}));
	EidosAssertScriptSuccess("strsplit('This is a test.', '');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"T", "h", "i", "s", " ", "i", "s", " ", "a", " ", "t", "e", "s", "t", "."}));
	
	// substr()
	EidosAssertScriptSuccess("substr(string(0), 1);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("substr(string(0), 1, 2);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("x=c('foo'); substr(x, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"oo"}));
	EidosAssertScriptSuccess("x=c('foo'); substr(x, 1, 10000);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"oo"}));
	EidosAssertScriptSuccess("x=c('foo'); substr(x, 1, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"o"}));
	EidosAssertScriptSuccess("x=c('foo'); substr(x, 1, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"oo"}));
	EidosAssertScriptSuccess("x=c('foo'); substr(x, 1, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"oo"}));
	EidosAssertScriptSuccess("x=c('foo'); substr(x, 1, 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{""}));
	EidosAssertScriptSuccess("x=c('foo'); substr(x, 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{""}));
	EidosAssertScriptSuccess("x=c('foo'); substr(x, -100);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo"}));
	EidosAssertScriptSuccess("x=c('foo'); substr(x, -100, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"fo"}));
	EidosAssertScriptRaise("x=c('foo'); substr(x, 1, c(2, 4));", 12, "requires the size of");
	EidosAssertScriptRaise("x=c('foo'); substr(x, c(1, 2), 4);", 12, "requires the size of");
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"oo", "ar", "oobaz"}));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 10000);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"oo", "ar", "oobaz"}));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"o", "a", "o"}));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"oo", "ar", "oo"}));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"oo", "ar", "oob"}));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, c(1, 2, 3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"oo", "r", "baz"}));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, c(1, 2, 3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"o", "ar", "oob"}));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, c(1, 2, 3), c(1, 2, 3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"o", "r", "b"}));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, c(1, 2, 3), c(2, 4, 6));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"oo", "r", "baz"}));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 1, 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"", "", ""}));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"", "", ""}));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, -100);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar", "foobaz"}));
	EidosAssertScriptSuccess("x=c('foo','bar','foobaz'); substr(x, -100, 1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"fo", "ba", "fo"}));
	EidosAssertScriptRaise("x=c('foo','bar','foobaz'); substr(x, 1, c(2, 4));", 27, "requires the size of");
	EidosAssertScriptRaise("x=c('foo','bar','foobaz'); substr(x, c(1, 2), 4);", 27, "requires the size of");
	
	// unique()
	EidosAssertScriptSuccess("unique(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("unique(logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("unique(integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("unique(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("unique(string(0));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("unique(object());", gStaticEidosValue_Object_ZeroVec);
	EidosAssertScriptSuccess("unique(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("unique(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("unique(3.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.5)));
	EidosAssertScriptSuccess("unique('foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("unique(_Test(7))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("unique(c(T,T,T,T,F,T,T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("unique(c(3,5,3,9,2,3,3,7,5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 5, 9, 2, 7}));
	EidosAssertScriptSuccess("unique(c(3.5,1.2,9.3,-1.0,1.2,-1.0,1.2,7.6,3.5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.5, 1.2, 9.3, -1, 7.6}));
	EidosAssertScriptSuccess("unique(c('foo', 'bar', 'foo', 'baz', 'baz', 'bar', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar", "baz"}));
	EidosAssertScriptSuccess("unique(c(_Test(7), _Test(7), _Test(2), _Test(7), _Test(2)))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 7, 2, 7, 2}));
	
	EidosAssertScriptSuccess("unique(NULL, F);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("unique(logical(0), F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("unique(integer(0), F);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("unique(float(0), F);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("unique(string(0), F);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("unique(object(), F);", gStaticEidosValue_Object_ZeroVec);
	EidosAssertScriptSuccess("unique(T, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("unique(5, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("unique(3.5, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.5)));
	EidosAssertScriptSuccess("unique('foo', F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("unique(_Test(7), F)._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("unique(c(T,T,T,T,F,T,T), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("sort(unique(c(3,5,3,9,2,3,3,7,5), F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 5, 7, 9}));
	EidosAssertScriptSuccess("sort(unique(c(3.5,1.2,9.3,-1.0,1.2,-1.0,1.2,7.6,3.5), F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-1, 1.2, 3.5, 7.6, 9.3}));
	EidosAssertScriptSuccess("sort(unique(c('foo', 'bar', 'foo', 'baz', 'baz', 'bar', 'foo'), F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"bar", "baz", "foo"}));
	EidosAssertScriptSuccess("sort(unique(c(_Test(7), _Test(7), _Test(2), _Test(7), _Test(2)), F)._yolk);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 2, 7, 7, 7}));
	
	EidosAssertScriptSuccess("x = asInteger(runif(10000, 0, 10000)); size(unique(x)) == size(unique(x, F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = runif(10000, 0, 1); size(unique(x)) == size(unique(x, F));", gStaticEidosValue_LogicalT);
	
	// which()
	EidosAssertScriptRaise("which(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("which(5);", 0, "cannot be type");
	EidosAssertScriptRaise("which(5.7);", 0, "cannot be type");
	EidosAssertScriptRaise("which('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("which(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("which(logical(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("which(F);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("which(T);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("which(c(T,F,F,T,F,T,F,F,T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 3, 5, 8}));
	
	// whichMax()
	EidosAssertScriptSuccess("whichMax(T);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("whichMax(3);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("whichMax(3.5);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("whichMax('foo');", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("whichMax(c(F, F, T, F, T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("whichMax(c(3, 7, 19, -5, 9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("whichMax(c(3.3, 7.7, 19.1, -5.8, 9.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("whichMax(c('bar', 'foo', 'baz'));", gStaticEidosValue_Integer1);
	EidosAssertScriptRaise("whichMax(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("whichMax(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMax(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMax(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMax(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMax(string(0));", gStaticEidosValueNULL);
	
	// whichMin()
	EidosAssertScriptSuccess("whichMin(T);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("whichMin(3);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("whichMin(3.5);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("whichMin('foo');", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("whichMin(c(T, F, T, F, T));", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("whichMin(c(3, 7, 19, -5, 9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("whichMin(c(3.3, 7.7, 19.1, -5.8, 9.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("whichMin(c('foo', 'bar', 'baz'));", gStaticEidosValue_Integer1);
	EidosAssertScriptRaise("whichMin(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("whichMin(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(string(0));", gStaticEidosValueNULL);
}

#pragma mark value type testing / coercion
void _RunFunctionValueTestingCoercionTests(void)
{
	// asFloat()
	EidosAssertScriptSuccess("asFloat(-1:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-1,0,1,2,3}));
	EidosAssertScriptSuccess("asFloat(-1.0:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-1,0,1,2,3}));
	EidosAssertScriptSuccess("asFloat(c(T,F,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1,0,1,0}));
	EidosAssertScriptSuccess("asFloat(c('1','2','3'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1,2,3}));
	EidosAssertScriptRaise("asFloat('foo');", 0, "could not be represented");
	EidosAssertScriptSuccess("identical(asFloat(matrix(c('1','2','3'))), matrix(1.0:3.0));", gStaticEidosValue_LogicalT);
	
	// asInteger()
	EidosAssertScriptSuccess("asInteger(-1:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1,0,1,2,3}));
	EidosAssertScriptSuccess("asInteger(-1.0:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1,0,1,2,3}));
	EidosAssertScriptSuccess("asInteger(c(T,F,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1,0,1,0}));
	EidosAssertScriptSuccess("asInteger(c('1','2','3'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1,2,3}));
	EidosAssertScriptRaise("asInteger('foo');", 0, "could not be represented");
	EidosAssertScriptRaise("asInteger(NAN);", 0, "cannot be converted");
	EidosAssertScriptSuccess("identical(asInteger(matrix(c('1','2','3'))), matrix(1:3));", gStaticEidosValue_LogicalT);
	
	// asInteger() overflow tests; these may be somewhat platform-dependent but I doubt it will bite us
	EidosAssertScriptRaise("asInteger(asFloat(9223372036854775807));", 0, "too large to be converted");																// the double representation is larger than INT64_MAX
	EidosAssertScriptRaise("asInteger(asFloat(9223372036854775807-511));", 0, "too large to be converted");															// the same double representation as previous
	EidosAssertScriptSuccess("asInteger(asFloat(9223372036854775807-512));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(9223372036854774784)));	// 9223372036854774784 == 9223372036854775807-1023, the closest value to INT64_MAX that double can represent
	EidosAssertScriptSuccess("asInteger(asFloat(-9223372036854775807 - 1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(INT64_MIN)));			// the double representation is exact
	EidosAssertScriptSuccess("asInteger(asFloat(-9223372036854775807 - 1) - 1024);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(INT64_MIN)));	// the same double representation as previous; the closest value to INT64_MIN that double can represent
	EidosAssertScriptRaise("asInteger(asFloat(-9223372036854775807 - 1) - 1025);", 0, "too large to be converted");													// overflow on cast
	EidosAssertScriptRaise("asInteger(asFloat(c(9223372036854775807, 0)));", 0, "too large to be converted");																// the double representation is larger than INT64_MAX
	EidosAssertScriptRaise("asInteger(asFloat(c(9223372036854775807, 0)-511));", 0, "too large to be converted");															// the same double representation as previous
	EidosAssertScriptSuccess("asInteger(asFloat(c(9223372036854775807, 0)-512));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{9223372036854774784, -512}));	// 9223372036854774784 == 9223372036854775807-1023, the closest value to INT64_MAX that double can represent
	EidosAssertScriptSuccess("asInteger(asFloat(c(-9223372036854775807, 0) - 1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{INT64_MIN, -1}));			// the double representation is exact
	EidosAssertScriptSuccess("asInteger(asFloat(c(-9223372036854775807, 0) - 1) - 1024);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{INT64_MIN, -1025}));	// the same double representation as previous; the closest value to INT64_MIN that double can represent
	EidosAssertScriptRaise("asInteger(asFloat(c(-9223372036854775807, 0) - 1) - 1025);", 0, "too large to be converted");													// overflow on cast
	
	// asLogical()
	EidosAssertScriptSuccess("asLogical(1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true}));
	EidosAssertScriptSuccess("asLogical(0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false}));
	EidosAssertScriptSuccess("asLogical(-1:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true,false,true,true,true}));
	EidosAssertScriptSuccess("asLogical(-1.0:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true,false,true,true,true}));
	EidosAssertScriptRaise("asLogical(NAN);", 0, "cannot be converted");
	EidosAssertScriptSuccess("asLogical(c(T,F,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true,false,true,false}));
	EidosAssertScriptSuccess("asLogical(c('foo','bar',''));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true,true,false}));
	EidosAssertScriptSuccess("identical(asLogical(matrix(-1:3)), matrix(c(T,F,T,T,T)));", gStaticEidosValue_LogicalT);
	
	// asString()
	EidosAssertScriptSuccess("asString(NULL);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"NULL"}));
	EidosAssertScriptSuccess("asString(-1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"-1"}));
	EidosAssertScriptSuccess("asString(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"3"}));
	EidosAssertScriptSuccess("asString(-1:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"-1","0","1","2","3"}));
	EidosAssertScriptSuccess("asString(-1.0:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"-1","0","1","2","3"}));
	EidosAssertScriptSuccess("asString(c(1.0, NAN, -2.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"1","NAN","-2"}));
	EidosAssertScriptSuccess("asString(c(T,F,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"T","F","T","F"}));
	EidosAssertScriptSuccess("asString(c('1','2','3'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"1","2","3"}));
	EidosAssertScriptSuccess("identical(asString(matrix(-1:3)), matrix(c('-1','0','1','2','3')));", gStaticEidosValue_LogicalT);
	
	// elementType()
	EidosAssertScriptSuccess("elementType(NULL);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("NULL")));
	EidosAssertScriptSuccess("elementType(T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("logical")));
	EidosAssertScriptSuccess("elementType(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("integer")));
	EidosAssertScriptSuccess("elementType(3.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("float")));
	EidosAssertScriptSuccess("elementType('foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("string")));
	EidosAssertScriptSuccess("elementType(_Test(7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("_TestElement")));
	EidosAssertScriptSuccess("elementType(object());", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("undefined")));
	EidosAssertScriptSuccess("elementType(c(object(), object()));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("undefined")));
	EidosAssertScriptSuccess("elementType(c(_Test(7), object()));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("_TestElement")));
	EidosAssertScriptSuccess("elementType(c(object(), _Test(7)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("_TestElement")));
	EidosAssertScriptSuccess("elementType(_Test(7)[F]);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("_TestElement")));
	
	// isFloat()
	EidosAssertScriptSuccess("isFloat(NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFloat(T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFloat(3);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFloat(3.5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isFloat('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFloat(_Test(7));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isFloat(object());", gStaticEidosValue_LogicalF);
	
	// isInteger()
	EidosAssertScriptSuccess("isInteger(NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInteger(T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInteger(3);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isInteger(3.5);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInteger('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInteger(_Test(7));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isInteger(object());", gStaticEidosValue_LogicalF);
	
	// isLogical()
	EidosAssertScriptSuccess("isLogical(NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isLogical(T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isLogical(3);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isLogical(3.5);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isLogical('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isLogical(_Test(7));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isLogical(object());", gStaticEidosValue_LogicalF);
	
	// isNULL()
	EidosAssertScriptSuccess("isNULL(NULL);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isNULL(T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNULL(3);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNULL(3.5);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNULL('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNULL(_Test(7));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isNULL(object());", gStaticEidosValue_LogicalF);
	
	// isObject()
	EidosAssertScriptSuccess("isObject(NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isObject(T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isObject(3);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isObject(3.5);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isObject('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isObject(_Test(7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isObject(object());", gStaticEidosValue_LogicalT);
	
	// isString()
	EidosAssertScriptSuccess("isString(NULL);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isString(T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isString(3);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isString(3.5);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isString('foo');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("isString(_Test(7));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("isString(object());", gStaticEidosValue_LogicalF);
	
	// type()
	EidosAssertScriptSuccess("type(NULL);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("NULL")));
	EidosAssertScriptSuccess("type(T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("logical")));
	EidosAssertScriptSuccess("type(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("integer")));
	EidosAssertScriptSuccess("type(3.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("float")));
	EidosAssertScriptSuccess("type('foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("string")));
	EidosAssertScriptSuccess("type(_Test(7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("object")));
	EidosAssertScriptSuccess("type(object());", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("object")));
}

#pragma mark matrix and array
void _RunFunctionMatrixArrayTests(void)
{
	// array()
	EidosAssertScriptRaise("array(5, integer(0));", 0, "at least a matrix");
	EidosAssertScriptRaise("array(5, 1);", 0, "at least a matrix");
	EidosAssertScriptRaise("array(5, c(1,2));", 0, "product of the proposed dimensions");
	EidosAssertScriptSuccess("identical(array(5, c(1,1)), matrix(5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6, c(2,3)), matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6, c(3,2)), matrix(1:6, nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("size(array(1:12, c(3,2,2))) == 12;", gStaticEidosValue_LogicalT);		// FIXME not sure how to test higher-dimensional arrays right now...
	
	// cbind()
	EidosAssertScriptRaise("cbind(5, 5.5);", 0, "be the same type");
	EidosAssertScriptRaise("cbind(5, array(5, c(1,1,1)));", 0, "all arguments be vectors or matrices");
	EidosAssertScriptRaise("cbind(matrix(1:4, nrow=2), matrix(1:4, nrow=4));", 0, "number of row");
	EidosAssertScriptSuccess("identical(cbind(5), matrix(5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cbind(1:5), matrix(1:5, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cbind(1:5, 6:10), matrix(1:10, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cbind(1:5, 6:10, NULL, integer(0), 11:15), matrix(1:15, ncol=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cbind(matrix(1:6, nrow=2), matrix(7:12, nrow=2)), matrix(1:12, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cbind(matrix(1:6, ncol=2), matrix(7:12, ncol=2)), matrix(1:12, nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cbind(matrix(1:6, nrow=1), matrix(7:12, nrow=1)), matrix(1:12, nrow=1));", gStaticEidosValue_LogicalT);
	
	// dim()
	EidosAssertScriptSuccess("dim(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(1);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(1.5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim('foo');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(c(T, F));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(c(1, 2));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(c(1.5, 2.0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(c('foo', 'bar'));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(matrix(3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 1}));
	EidosAssertScriptSuccess("dim(matrix(1:6, nrow=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("dim(matrix(1:6, nrow=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("dim(matrix(1:6, ncol=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 2}));
	EidosAssertScriptSuccess("dim(matrix(1:6, ncol=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 2}));
	EidosAssertScriptSuccess("dim(array(1:24, c(2,3,4)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 4}));
	EidosAssertScriptSuccess("dim(array(1:48, c(2,3,4,2)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 4, 2}));
	EidosAssertScriptSuccess("dim(matrix(3.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 1}));
	EidosAssertScriptSuccess("dim(matrix(1.0:6, nrow=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("dim(matrix(1.0:6, nrow=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("dim(matrix(1.0:6, ncol=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 2}));
	EidosAssertScriptSuccess("dim(matrix(1.0:6, ncol=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 2}));
	EidosAssertScriptSuccess("dim(array(1.0:24, c(2,3,4)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 4}));
	EidosAssertScriptSuccess("dim(array(1.0:48, c(2,3,4,2)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 4, 2}));
	
	// drop()
	EidosAssertScriptSuccess("drop(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("identical(drop(integer(0)), integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(5), 5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(5:9), 5:9);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(matrix(5)), 5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(matrix(5:9)), 5:9);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(matrix(1:6, ncol=1)), 1:6);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(matrix(1:6, nrow=1)), 1:6);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(matrix(1:6, nrow=2)), matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(5, c(1,1,1))), 5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:6, c(6,1,1))), 1:6);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:6, c(1,6,1))), 1:6);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:6, c(1,1,6))), 1:6);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:6, c(2,3,1))), matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:6, c(1,2,3))), matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:6, c(2,1,3))), matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:12, c(12,1,1))), 1:12);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:12, c(2,3,2))), array(1:12, c(2,3,2)));", gStaticEidosValue_LogicalT);
	
	// matrix()
	EidosAssertScriptSuccess("matrix(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("matrix(3, nrow=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("matrix(3, ncol=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("matrix(3, nrow=1, ncol=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("matrix(1:6, nrow=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("matrix(1:6, ncol=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("matrix(1:6, ncol=2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("matrix(1:6, ncol=2, byrow=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 5, 2, 4, 6}));
	EidosAssertScriptSuccess("matrix(1:6, ncol=3, byrow=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 4, 2, 5, 3, 6}));
	EidosAssertScriptRaise("matrix(1:5, ncol=2);", 0, "not a multiple of the supplied column count");
	EidosAssertScriptRaise("matrix(1:5, nrow=2);", 0, "not a multiple of the supplied row count");
	EidosAssertScriptRaise("matrix(1:5, nrow=2, ncol=2);", 0, "length equal to the product");
	EidosAssertScriptSuccess("identical(matrix(1:6, ncol=2), matrix(c(1, 4, 2, 5, 3, 6), ncol=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1:6, ncol=3), matrix(c(1, 3, 5, 2, 4, 6), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("matrix(3.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3}));
	EidosAssertScriptSuccess("matrix(3.0, nrow=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3}));
	EidosAssertScriptSuccess("matrix(3.0, ncol=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3}));
	EidosAssertScriptSuccess("matrix(3.0, nrow=1, ncol=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3}));
	EidosAssertScriptSuccess("matrix(1.0:6, nrow=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("matrix(1.0:6, ncol=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("matrix(1.0:6, ncol=2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("matrix(1.0:6, ncol=2, byrow=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 3, 5, 2, 4, 6}));
	EidosAssertScriptSuccess("matrix(1.0:6, ncol=3, byrow=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 4, 2, 5, 3, 6}));
	EidosAssertScriptRaise("matrix(1.0:5, ncol=2);", 0, "not a multiple of the supplied column count");
	EidosAssertScriptRaise("matrix(1.0:5, nrow=2);", 0, "not a multiple of the supplied row count");
	EidosAssertScriptRaise("matrix(1.0:5, nrow=2, ncol=2);", 0, "length equal to the product");
	EidosAssertScriptSuccess("identical(matrix(1.0:6, ncol=2), matrix(c(1.0, 4, 2, 5, 3, 6), ncol=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1.0:6, ncol=3), matrix(c(1.0, 3, 5, 2, 4, 6), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	
	// matrixMult()
	EidosAssertScriptRaise("matrixMult(matrix(5), 5);", 0, "is not a matrix");
	EidosAssertScriptRaise("matrixMult(5, matrix(5));", 0, "is not a matrix");
	EidosAssertScriptRaise("matrixMult(matrix(5), matrix(5.5));", 0, "are the same type");
	EidosAssertScriptRaise("matrixMult(matrix(1:5), matrix(1:5));", 0, "not conformable");
	EidosAssertScriptSuccess("A = matrix(2); B = matrix(5); identical(matrixMult(A, B), matrix(10));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(2); B = matrix(1:5, nrow=1); identical(matrixMult(A, B), matrix(c(2,4,6,8,10), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1:5, ncol=1); B = matrix(2); identical(matrixMult(A, B), matrix(c(2,4,6,8,10), ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1:5, ncol=1); B = matrix(1:5, nrow=1); identical(matrixMult(A, B), matrix(c(1:5, (1:5)*2, (1:5)*3, (1:5)*4, (1:5)*5), ncol=5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1:5, nrow=1); B = matrix(1:5, ncol=1); identical(matrixMult(A, B), matrix(55));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1:6, nrow=2); B = matrix(1:6, ncol=2); identical(matrixMult(A, B), matrix(c(22, 28, 49, 64), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1:6, ncol=2); B = matrix(1:6, nrow=2); identical(matrixMult(A, B), matrix(c(9, 12, 15, 19, 26, 33, 29, 40, 51), nrow=3));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptRaise("matrixMult(matrix(5.0), 5.0);", 0, "is not a matrix");
	EidosAssertScriptRaise("matrixMult(5.0, matrix(5.0));", 0, "is not a matrix");
	EidosAssertScriptRaise("matrixMult(matrix(5.0), matrix(5));", 0, "are the same type");
	EidosAssertScriptRaise("matrixMult(matrix(1.0:5.0), matrix(1.0:5.0));", 0, "not conformable");
	EidosAssertScriptSuccess("A = matrix(2.0); B = matrix(5.0); identical(matrixMult(A, B), matrix(10.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(2.0); B = matrix(1.0:5.0, nrow=1); identical(matrixMult(A, B), matrix(c(2.0,4.0,6.0,8.0,10.0), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1.0:5.0, ncol=1); B = matrix(2.0); identical(matrixMult(A, B), matrix(c(2.0,4.0,6.0,8.0,10.0), ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1.0:5.0, ncol=1); B = matrix(1.0:5.0, nrow=1); identical(matrixMult(A, B), matrix(c(1.0:5.0, (1.0:5.0)*2, (1.0:5.0)*3, (1.0:5.0)*4, (1.0:5.0)*5), ncol=5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1.0:5.0, nrow=1); B = matrix(1.0:5.0, ncol=1); identical(matrixMult(A, B), matrix(55.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1.0:6.0, nrow=2); B = matrix(1.0:6.0, ncol=2); identical(matrixMult(A, B), matrix(c(22.0, 28.0, 49.0, 64.0), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1.0:6.0, ncol=2); B = matrix(1.0:6.0, nrow=2); identical(matrixMult(A, B), matrix(c(9.0, 12.0, 15.0, 19.0, 26.0, 33.0, 29.0, 40.0, 51.0), nrow=3));", gStaticEidosValue_LogicalT);
	
	// ncol()
	EidosAssertScriptSuccess("ncol(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(1);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(1.5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol('foo');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(c(T, F));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(c(1, 2));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(c(1.5, 2.0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(c('foo', 'bar'));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(matrix(3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1}));
	EidosAssertScriptSuccess("ncol(matrix(1:6, nrow=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(matrix(1:6, nrow=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(matrix(1:6, ncol=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("ncol(matrix(1:6, ncol=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("ncol(array(1:24, c(2,3,4)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(array(1:48, c(2,3,4,2)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(matrix(3.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1}));
	EidosAssertScriptSuccess("ncol(matrix(1.0:6, nrow=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(matrix(1.0:6, nrow=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(matrix(1.0:6, ncol=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("ncol(matrix(1.0:6, ncol=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("ncol(array(1.0:24, c(2,3,4)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(array(1.0:48, c(2,3,4,2)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	
	// nrow()
	EidosAssertScriptSuccess("nrow(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(1);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(1.5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow('foo');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(c(T, F));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(c(1, 2));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(c(1.5, 2.0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(c('foo', 'bar'));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(matrix(3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1}));
	EidosAssertScriptSuccess("nrow(matrix(1:6, nrow=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(matrix(1:6, nrow=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(matrix(1:6, ncol=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("nrow(matrix(1:6, ncol=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("nrow(array(1:24, c(2,3,4)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(array(1:48, c(2,3,4,2)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(matrix(3.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1}));
	EidosAssertScriptSuccess("nrow(matrix(1.0:6, nrow=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(matrix(1.0:6, nrow=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(matrix(1.0:6, ncol=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("nrow(matrix(1.0:6, ncol=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("nrow(array(1.0:24, c(2,3,4)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(array(1.0:48, c(2,3,4,2)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	
	// rbind()
	EidosAssertScriptRaise("rbind(5, 5.5);", 0, "be the same type");
	EidosAssertScriptRaise("rbind(5, array(5, c(1,1,1)));", 0, "all arguments be vectors or matrices");
	EidosAssertScriptRaise("rbind(matrix(1:4, nrow=2), matrix(1:4, nrow=4));", 0, "number of columns");
	EidosAssertScriptSuccess("identical(rbind(5), matrix(5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rbind(1:5), matrix(1:5, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rbind(1:5, 6:10), matrix(1:10, nrow=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rbind(1:5, 6:10, NULL, integer(0), 11:15), matrix(1:15, nrow=3, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rbind(matrix(1:6, nrow=2), matrix(7:12, nrow=2)), matrix(c(1,2,7,8,3,4,9,10,5,6,11,12), nrow=4));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rbind(matrix(1:6, ncol=2), matrix(7:12, ncol=2)), matrix(c(1,2,3,7,8,9,4,5,6,10,11,12), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rbind(matrix(1:6, ncol=1), matrix(7:12, ncol=1)), matrix(1:12, ncol=1));", gStaticEidosValue_LogicalT);
	
	// t()
	EidosAssertScriptRaise("t(NULL);", 0, "is not a matrix");
	EidosAssertScriptRaise("t(T);", 0, "is not a matrix");
	EidosAssertScriptRaise("t(1);", 0, "is not a matrix");
	EidosAssertScriptRaise("t(1.5);", 0, "is not a matrix");
	EidosAssertScriptRaise("t('foo');", 0, "is not a matrix");
	EidosAssertScriptSuccess("identical(t(matrix(3)), matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1:6, nrow=2)), matrix(1:6, ncol=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1:6, nrow=2, byrow=T)), matrix(1:6, ncol=2, byrow=F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1:6, ncol=2)), matrix(1:6, nrow=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1:6, ncol=2, byrow=T)), matrix(1:6, nrow=2, byrow=F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(3.0)), matrix(3.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1.0:6, nrow=2)), matrix(1.0:6, ncol=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1.0:6, nrow=2, byrow=T)), matrix(1.0:6, ncol=2, byrow=F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1.0:6, ncol=2)), matrix(1.0:6, nrow=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1.0:6, ncol=2, byrow=T)), matrix(1.0:6, nrow=2, byrow=F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("t(array(1:24, c(2,3,4)));", 0, "is not a matrix");
	EidosAssertScriptRaise("t(array(1:48, c(2,3,4,2)));", 0, "is not a matrix");
}

#pragma mark filesystem access
void _RunFunctionFilesystemTests(void)
{
	// filesAtPath() – hard to know how to test this!  These tests should be true on Un*x machines, anyway – but might be disallowed by file permissions.
	EidosAssertScriptSuccess("type(filesAtPath('/tmp')) == 'string';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("type(filesAtPath('/tmp/')) == 'string';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(filesAtPath('/') == 'bin');", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("sum(filesAtPath('/', T) == '/bin');", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("filesAtPath('foo_is_a_bad_path');", gStaticEidosValueNULL);
	
	// writeFile()
	EidosAssertScriptSuccess("writeFile('/tmp/EidosTest.txt', c(paste(0:4), paste(5:9)));", gStaticEidosValue_LogicalT);
	
	// readFile() – note that the readFile() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess("readFile('/tmp/EidosTest.txt') == c(paste(0:4), paste(5:9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("all(asInteger(strsplit(paste(readFile('/tmp/EidosTest.txt')))) == 0:9);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("readFile('foo_is_a_bad_path.txt');", gStaticEidosValueNULL);
	
	// writeFile() with append
	EidosAssertScriptSuccess("writeFile('/tmp/EidosTest.txt', 'foo', T);", gStaticEidosValue_LogicalT);
	
	// readFile() – note that the readFile() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess("readFile('/tmp/EidosTest.txt') == c(paste(0:4), paste(5:9), 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	
	// fileExists() – note that the fileExists() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess("fileExists('/tmp/EidosTest.txt');", gStaticEidosValue_LogicalT);
	
	// deleteFile() – note that the deleteFile() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess("deleteFile('/tmp/EidosTest.txt');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("deleteFile('/tmp/EidosTest.txt');", gStaticEidosValue_LogicalF);
	
	// fileExists() – note that the fileExists() tests depend on the previous writeFile() and deleteFile() tests
	EidosAssertScriptSuccess("fileExists('/tmp/EidosTest.txt');", gStaticEidosValue_LogicalF);
	
	// writeTempFile()
	EidosAssertScriptRaise("file = writeTempFile('eidos_test_~', '.txt', '');", 7, "may not contain");
	EidosAssertScriptRaise("file = writeTempFile('eidos_test_/', '.txt', '');", 7, "may not contain");
	EidosAssertScriptRaise("file = writeTempFile('eidos_test_', 'foo~.txt', '');", 7, "may not contain");
	EidosAssertScriptRaise("file = writeTempFile('eidos_test_', 'foo/.txt', '');", 7, "may not contain");
	EidosAssertScriptSuccess("file = writeTempFile('eidos_test_', '.txt', ''); identical(readFile(file), string(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("file = writeTempFile('eidos_test_', '.txt', 'foo'); identical(readFile(file), 'foo');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("file = writeTempFile('eidos_test_', '.txt', c(paste(0:4), paste(5:9))); identical(readFile(file), c('0 1 2 3 4', '5 6 7 8 9'));", gStaticEidosValue_LogicalT);
	
	// createDirectory() – we rely on writeTempFile() to give us a file path that isn't in use, from which we derive a directory path that also shouldn't be in use
	EidosAssertScriptSuccess("file = writeTempFile('eidos_test_dir', '.txt', ''); dir = substr(file, 0, nchar(file) - 5); createDirectory(dir);", gStaticEidosValue_LogicalT);
	
	// getwd() / setwd()
	EidosAssertScriptSuccess("path1 = getwd(); path2 = setwd(path1); path1 == path2;", gStaticEidosValue_LogicalT);
}

#pragma mark color manipulation
void _RunColorManipulationTests(void)
{
	// cmColors()
	EidosAssertScriptRaise("cmColors(-1);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptRaise("cmColors(10000000);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptSuccess("cmColors(0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("cmColors(1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF"}));
	EidosAssertScriptSuccess("cmColors(2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF", "#FF80FF"}));
	EidosAssertScriptSuccess("cmColors(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF", "#FFFFFF", "#FF80FF"}));
	EidosAssertScriptSuccess("cmColors(4);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF", "#D4FFFF", "#FFD5FF", "#FF80FF"}));
	EidosAssertScriptSuccess("cmColors(7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF", "#AAFFFF", "#D4FFFF", "#FFFFFF", "#FFD5FF", "#FFAAFF", "#FF80FF"}));
	
	// heatColors()
	EidosAssertScriptRaise("heatColors(-1);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptRaise("heatColors(10000000);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptSuccess("heatColors(0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("heatColors(1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000"}));
	EidosAssertScriptSuccess("heatColors(2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#FFFF00"}));
	EidosAssertScriptSuccess("heatColors(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#FF8000", "#FFFF00"}));
	EidosAssertScriptSuccess("heatColors(4);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#FF8000", "#FFFF00", "#FFFF80"}));
	EidosAssertScriptSuccess("heatColors(8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#FF3300", "#FF6600", "#FF9900", "#FFCC00", "#FFFF00", "#FFFF40", "#FFFFBF"}));
	
	// terrainColors()
	EidosAssertScriptRaise("terrainColors(-1);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptRaise("terrainColors(10000000);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptSuccess("terrainColors(0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("terrainColors(1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#F2F2F2"}));
	EidosAssertScriptSuccess("terrainColors(2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#00A600", "#F2F2F2"}));
	EidosAssertScriptSuccess("terrainColors(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#00A600", "#ECB176", "#F2F2F2"}));
	EidosAssertScriptSuccess("terrainColors(4);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#00A600", "#E6E600", "#ECB176", "#F2F2F2"}));
	EidosAssertScriptSuccess("terrainColors(8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#00A600", "#3EBB00", "#8BD000", "#E6E600", "#E9BD3A", "#ECB176", "#EFC2B3", "#F2F2F2"}));
	
	// rainbow()
	EidosAssertScriptRaise("rainbow(-1);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptRaise("rainbow(10000000);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptSuccess("rainbow(0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("rainbow(1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000"}));
	EidosAssertScriptSuccess("rainbow(2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#00FFFF"}));
	EidosAssertScriptSuccess("rainbow(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#00FF00", "#0000FF"}));
	EidosAssertScriptSuccess("rainbow(4);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#80FF00", "#00FFFF", "#8000FF"}));
	EidosAssertScriptSuccess("rainbow(12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#FF8000", "#FFFF00", "#80FF00", "#00FF00", "#00FF80", "#00FFFF", "#0080FF", "#0000FF", "#8000FF", "#FF00FF", "#FF0080"}));
	EidosAssertScriptSuccess("rainbow(6, s=0.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF8080", "#FFFF80", "#80FF80", "#80FFFF", "#8080FF", "#FF80FF"}));
	EidosAssertScriptSuccess("rainbow(6, v=0.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#800000", "#808000", "#008000", "#008080", "#000080", "#800080"}));
	EidosAssertScriptSuccess("rainbow(6, s=0.5, v=0.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#804040", "#808040", "#408040", "#408080", "#404080", "#804080"}));
	EidosAssertScriptSuccess("rainbow(4, start=1.0/6, end=4.0/6, ccw=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FFFF00", "#00FF00", "#00FFFF", "#0000FF"}));
	EidosAssertScriptSuccess("rainbow(4, start=1.0/6, end=4.0/6, ccw=F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FFFF00", "#FF0000", "#FF00FF", "#0000FF"}));
	EidosAssertScriptSuccess("rainbow(4, start=4.0/6, end=1.0/6, ccw=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#0000FF", "#FF00FF", "#FF0000", "#FFFF00"}));
	EidosAssertScriptSuccess("rainbow(4, start=4.0/6, end=1.0/6, ccw=F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#0000FF", "#00FFFF", "#00FF00", "#FFFF00"}));
	
	// hsv2rgb()
	EidosAssertScriptRaise("hsv2rgb(c(0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("hsv2rgb(c(0.0, 0.0, 0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("hsv2rgb(c(NAN, 0.0, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("hsv2rgb(c(0.0, NAN, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("hsv2rgb(c(0.0, 0.0, NAN));", 0, "color component with value NAN");
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.0, -0.5)), c(0.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.0, 0.0)), c(0.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.0, 0.5)), c(0.5, 0.5, 0.5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.0, 1.0)), c(1.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.0, 1.5)), c(1.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, -0.5, 1.0)), c(1.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.25, 1.0)), c(1.0, 0.75, 0.75));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.5, 1.0)), c(1.0, 0.5, 0.5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.75, 1.0)), c(1.0, 0.25, 0.25));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 1.0, 1.0)), c(1.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 1.5, 1.0)), c(1.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(-0.5, 1.0, 1.0)), c(1.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(1/6, 1.0, 1.0)), c(1.0, 1.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(2/6, 1.0, 1.0)), c(0.0, 1.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(3/6, 1.0, 1.0)), c(0.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(4/6, 1.0, 1.0)), c(0.0, 0.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(5/6, 1.0, 1.0)), c(1.0, 0.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(6/6, 1.0, 1.0)), c(1.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(7/6, 1.0, 1.0)), c(1.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(matrix(c(1/6, 1.0, 1.0, 0.0, 0.25, 1.0), ncol=3, byrow=T)), matrix(c(1.0, 1.0, 0.0, 1.0, 0.75, 0.75), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	
	// rgb2hsv()
	EidosAssertScriptRaise("rgb2hsv(c(0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2hsv(c(0.0, 0.0, 0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2hsv(c(NAN, 0.0, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("rgb2hsv(c(0.0, NAN, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("rgb2hsv(c(0.0, 0.0, NAN));", 0, "color component with value NAN");
	EidosAssertScriptSuccess("identical(rgb2hsv(c(-1.0, 0.0, 0.0)), c(0.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, -1.0, 0.0)), c(0.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, 0.0, -1.0)), c(0.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, 0.0, 0.0)), c(0.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.5, 0.5, 0.5)), c(0.0, 0.0, 0.5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 1.0, 1.0)), c(0.0, 0.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.5, 1.0, 1.0)), c(0.0, 0.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 1.5, 1.0)), c(0.0, 0.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 1.0, 1.5)), c(0.0, 0.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 0.75, 0.75)), c(0.0, 0.25, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 0.5, 0.5)), c(0.0, 0.5, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 0.25, 0.25)), c(0.0, 0.75, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 0.0, 0.0)), c(0.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 1.0, 0.0)), c(1/6, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, 1.0, 0.0)), c(2/6, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, 1.0, 1.0)), c(3/6, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, 0.0, 1.0)), c(4/6, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(rgb2hsv(c(1.0, 0.0, 1.0)) - c(5/6, 1.0, 1.0))) < 1e-7;", gStaticEidosValue_LogicalT);	// roundoff with 5/6
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.5, -0.5, 0.0)), c(0.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, 1.5, -0.5)), c(2/6, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(-0.5, 0.0, 1.5)), c(4/6, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(matrix(c(1.0, 1.0, 0.0, 1.0, 0.75, 0.75), ncol=3, byrow=T)), matrix(c(1/6, 1.0, 1.0, 0.0, 0.25, 1.0), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	
	// rgb2color()
	EidosAssertScriptRaise("rgb2color(c(0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2color(c(0.0, 0.0, 0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2color(c(NAN, 0.0, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("rgb2color(c(0.0, NAN, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("rgb2color(c(0.0, 0.0, NAN));", 0, "color component with value NAN");
	EidosAssertScriptSuccess("rgb2color(c(-0.5, -0.5, -0.5)) == '#000000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.0, 0.0)) == '#000000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(1.0, 1.0, 1.0)) == '#FFFFFF';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(1.5, 1.5, 1.5)) == '#FFFFFF';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(1.0, 0.0, 0.0)) == '#FF0000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 1.0, 0.0)) == '#00FF00';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.0, 1.0)) == '#0000FF';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.25, 0.0, 0.0)) == '#400000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.25, 0.0)) == '#004000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.0, 0.25)) == '#000040';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.5, 0.0, 0.0)) == '#800000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.5, 0.0)) == '#008000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.0, 0.5)) == '#000080';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.75, 0.0, 0.0)) == '#BF0000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.75, 0.0)) == '#00BF00';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.0, 0.75)) == '#0000BF';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(1.0, 0.0, 0.0)) == '#FF0000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 1.0, 0.0)) == '#00FF00';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.0, 1.0)) == '#0000FF';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2color(matrix(c(0.25, 0.0, 0.0, 0.0, 0.75, 0.0, 0.0, 0.0, 1.0), ncol=3, byrow=T)), c('#400000', '#00BF00', '#0000FF'));", gStaticEidosValue_LogicalT);
	
	// color2rgb()
	EidosAssertScriptRaise("identical(color2rgb('foo'), c(0.0, 0.0, 0.0));", 10, "could not be found");
	EidosAssertScriptRaise("identical(color2rgb('#00000'), c(0.0, 0.0, 0.0));", 10, "could not be found");
	EidosAssertScriptRaise("identical(color2rgb('#0000000'), c(0.0, 0.0, 0.0));", 10, "could not be found");
	EidosAssertScriptRaise("identical(color2rgb('#0000g0'), c(0.0, 0.0, 0.0));", 10, "is malformed");
	EidosAssertScriptSuccess("identical(color2rgb('white'), c(1.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(color2rgb(c('#000000', 'red', 'green', 'blue', '#FFFFFF')), matrix(c(0.0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('chocolate1') - c(1.0, 127/255, 36/255))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#000000') - c(0.0, 0.0, 0.0))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#7F0000') - c(127/255, 0.0, 0.0))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#FF0000') - c(1.0, 0.0, 0.0))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#007F00') - c(0.0, 127/255, 0.0))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#00FF00') - c(0.0, 1.0, 0.0))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#00007F') - c(0.0, 0.0, 127/255))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#0000FF') - c(0.0, 0.0, 1.0))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#0000ff') - c(0.0, 0.0, 1.0))) < 1e-7;", gStaticEidosValue_LogicalT);
}

#pragma mark miscellaneous
void _RunFunctionMiscTests_apply_sapply(void)
{
	// apply()
	EidosAssertScriptRaise("x=integer(0); apply(x, 0, 'applyValue^2;');", 14, "matrix or array");
	EidosAssertScriptRaise("x=5; apply(x, 0, 'applyValue^2;');", 5, "matrix or array");
	EidosAssertScriptRaise("x=5:9; apply(x, 0, 'applyValue^2;');", 7, "matrix or array");
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, -1, 'applyValue^2;');", 23, "out of range");
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, 2, 'applyValue^2;');", 23, "out of range");
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, c(0,0), 'applyValue^2;');", 23, "already specified");
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, integer(0), 'applyValue^2;');", 23, "requires that margins be specified");
	
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, 0, 'setSeed(5);');", 23, "must return a non-void value");
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, 0, 'semanticError;');", 23, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, 0, 'syntax Error;');", 23, "unexpected token '@Error'");
	
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'sum(applyValue);'), c(9,12));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'sum(applyValue);'), c(3,7,11));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'sum(applyValue);'), matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'sum(applyValue);'), t(matrix(1:6, nrow=2)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'applyValue^2;'), matrix(c(1.0,9,25,4,16,36), nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'applyValue^2;'), matrix(c(1.0,4,9,16,25,36), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'applyValue^2;'), matrix(c(1.0,4,9,16,25,36), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'applyValue^2;'), t(matrix(c(1.0,4,9,16,25,36), nrow=2)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'c(applyValue, applyValue^2);'), matrix(c(1.0,3,5,1,9,25,2,4,6,4,16,36), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'c(applyValue, applyValue^2);'), matrix(c(1.0,2,1,4,3,4,9,16,5,6,25,36), ncol=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'c(applyValue, applyValue^2);'), array(c(1.0,1,2,4,3,9,4,16,5,25,6,36), c(2,2,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'c(applyValue, applyValue^2);'), array(c(1.0,1,3,9,5,25,2,4,4,16,6,36), c(2,3,2)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'if (applyValue[0] % 2) sum(applyValue); else NULL;'), 9);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'if (applyValue[0] % 3) sum(applyValue); else NULL;'), c(3,11));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'if (applyValue[0] % 2) sum(applyValue); else NULL;'), c(1,3,5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'if (applyValue[0] % 2) sum(applyValue); else NULL;'), c(1,3,5));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'if (applyValue[0] % 2) applyValue^2; else NULL;'), c(1.0,9,25));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'if (applyValue[0] % 3) applyValue^2; else NULL;'), c(1.0,4,25,36));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'if (applyValue[0] % 2) applyValue^2; else NULL;'), c(1.0,9,25));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'if (applyValue[0] % 2) applyValue^2; else NULL;'), c(1.0,9,25));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'if (applyValue[0] % 2) c(applyValue, applyValue^2); else NULL;'), c(1.0,3,5,1,9,25));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'if (applyValue[0] % 3) c(applyValue, applyValue^2); else NULL;'), c(1.0,2,1,4,5,6,25,36));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'if (applyValue[0] % 2) c(applyValue, applyValue^2); else NULL;'), c(1.0,1,3,9,5,25));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'if (applyValue[0] % 2) c(applyValue, applyValue^2); else NULL;'), c(1.0,1,3,9,5,25));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, 0, 'sum(applyValue);'), c(36,42));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, 1, 'sum(applyValue);'), c(18,26,34));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, 2, 'sum(applyValue);'), c(21,57));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,1), 'sum(applyValue);'), matrix(c(8,10,12,14,16,18), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(1,2), 'sum(applyValue);'), matrix(c(3,7,11,15,19,23), nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,2), 'sum(applyValue);'), matrix(c(9,12,27,30), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,1,2), 'sum(applyValue);'), array(1:12, c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(2,1,0), 'sum(applyValue);'), array(c(1,7,3,9,5,11,2,8,4,10,6,12), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(2,0,1), 'sum(applyValue);'), array(c(1,7,2,8,3,9,4,10,5,11,6,12), c(2,2,3)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, 0, 'applyValue^2;'), matrix(c(1.0,9,25,49,81,121,4,16,36,64,100,144), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, 1, 'applyValue^2;'), matrix(c(1.0,4,49,64,9,16,81,100,25,36,121,144), ncol=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, 2, 'applyValue^2;'), matrix(c(1.0,4,9,16,25,36,49,64,81,100,121,144), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,1), 'applyValue^2;'), array(c(1.0,49,4,64,9,81,16,100,25,121,36,144), c(2,2,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(1,2), 'applyValue^2;'), array(c(1.0,4,9,16,25,36,49,64,81,100,121,144), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,2), 'applyValue^2;'), array(c(1.0,9,25,4,16,36,49,81,121,64,100,144), c(3,2,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,1,2), 'applyValue^2;'), array((1.0:12)^2, c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(2,1,0), 'applyValue^2;'), array(c(1.0,49,9,81,25,121,4,64,16,100,36,144), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(2,0,1), 'applyValue^2;'), array(c(1.0,49,4,64,9,81,16,100,25,121,36,144), c(2,2,3)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, 0, 'sum(applyValue);'), c(144,156));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, 1, 'sum(applyValue);'), c(84,100,116));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, 2, 'sum(applyValue);'), c(114,186));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, 3, 'sum(applyValue);'), c(78,222));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(0,1), 'sum(applyValue);'), matrix(c(40,44,48,52,56,60), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(0,2), 'sum(applyValue);'), matrix(c(54,60,90,96), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(0,3), 'sum(applyValue);'), matrix(c(36,42,108,114), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(1,0), 'sum(applyValue);'), matrix(c(40,48,56,44,52,60), nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(1,2), 'sum(applyValue);'), matrix(c(30,38,46,54,62,70), nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(1,3), 'sum(applyValue);'), matrix(c(18,26,34,66,74,82), nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(2,0), 'sum(applyValue);'), matrix(c(54,90,60,96), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(2,1), 'sum(applyValue);'), matrix(c(30,54,38,62,46,70), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(2,3), 'sum(applyValue);'), matrix(c(21,57,93,129), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(3,0), 'sum(applyValue);'), matrix(c(36,108,42,114), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(3,1), 'sum(applyValue);'), matrix(c(18,66,26,74,34,82), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(3,2), 'sum(applyValue);'), matrix(c(21,93,57,129), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(0,1,2), 'sum(applyValue);'), array(c(14,16,18,20,22,24,26,28,30,32,34,36), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(3,1,0), 'sum(applyValue);'), array(c(8,32,12,36,16,40,10,34,14,38,18,42), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(2,3,0,1), 'sum(applyValue);'), array(c(1,7,13,19,2,8,14,20,3,9,15,21,4,10,16,22,5,11,17,23,6,12,18,24), c(2,2,2,3)));", gStaticEidosValue_LogicalT);
	
	// sapply()
	EidosAssertScriptSuccess("x=integer(0); sapply(x, 'applyValue^2;');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x=1:5; sapply(x, 'applyValue^2;');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 4, 9, 16, 25}));
	EidosAssertScriptSuccess("x=1:5; sapply(x, 'product(1:applyValue);');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 6, 24, 120}));
	EidosAssertScriptSuccess("x=1:3; sapply(x, \"rep(''+applyValue, applyValue);\");", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"1", "2", "2", "3", "3", "3"}));
	EidosAssertScriptSuccess("x=1:5; sapply(x, \"paste(rep(''+applyValue, applyValue), '');\");", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"1", "22", "333", "4444", "55555"}));
	EidosAssertScriptSuccess("x=1:10; sapply(x, 'if (applyValue % 2) applyValue; else NULL;');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 5, 7, 9}));
	EidosAssertScriptSuccess("x=1:5; sapply(x, 'y=applyValue; NULL;'); y;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("x=1:5; sapply(x, 'y=applyValue; y;');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x=2; for (i in 1:2) x=sapply(x, 'applyValue^2;'); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16.0)));
	EidosAssertScriptRaise("x=2; sapply(x, 'semanticError;');", 5, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; sapply(x, y);", 25, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; sapply(x, y[T]);", 25, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; sapply(x, 'syntax Error;');", 5, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; sapply(x, y);", 24, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; sapply(x, y[T]);", 24, "unexpected token '@Error'");
	EidosAssertScriptSuccess("x=2; y='x;'; sapply(x, y[T]);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	
	EidosAssertScriptSuccess("identical(sapply(1:6, 'integer(0);'), integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'integer(0);', simplify='vector'), integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'integer(0);', simplify='matrix'), integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(1:6, 'integer(0);', simplify='match'), 2:7);", 10, "not all singletons");
	EidosAssertScriptRaise("identical(sapply(1:6, 'integer(0);', simplify='foo'), integer(0));", 10, "unrecognized simplify option");
	EidosAssertScriptRaise("identical(sapply(1:6, 'setSeed(5);'), integer(0));", 10, "must return a non-void value");
	
	EidosAssertScriptSuccess("identical(sapply(1:6, 'applyValue+1;'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'applyValue+1;', simplify='vector'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'applyValue+1;', simplify='match'), 2:7);", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'applyValue+1;'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'applyValue+1;', simplify='vector'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'applyValue+1;', simplify='match'), matrix(2:7, nrow=1));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'applyValue+1;'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'applyValue+1;', simplify='vector'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'applyValue+1;', simplify='match'), matrix(2:7, ncol=1));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'applyValue+1;'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'applyValue+1;', simplify='vector'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'applyValue+1;', simplify='match'), matrix(2:7, ncol=2));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'c(applyValue, applyValue+1);'), c(1,2,2,3,3,4,4,5,5,6,6,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'c(applyValue, applyValue+1);', simplify='vector'), c(1,2,2,3,3,4,4,5,5,6,6,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'c(applyValue, applyValue+1);', simplify='matrix'), matrix(c(1,2,2,3,3,4,4,5,5,6,6,7), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, ncol=2), 'c(applyValue, applyValue+1);', simplify='match'), c(1,2,2,3,3,4,4,5,5,6,6,7));", 10, "not all singletons");
	
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'applyValue+1;'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'applyValue+1;', simplify='vector'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'applyValue+1;', simplify='match'), array(2:7, c(2,1,3)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'c(applyValue, applyValue+1);'), c(1,2,2,3,3,4,4,5,5,6,6,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'c(applyValue, applyValue+1);', simplify='vector'), c(1,2,2,3,3,4,4,5,5,6,6,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'c(applyValue, applyValue+1);', simplify='matrix'), matrix(c(1,2,2,3,3,4,4,5,5,6,6,7), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'c(applyValue, applyValue+1);', simplify='match'), c(1,2,2,3,3,4,4,5,5,6,6,7));", 10, "not all singletons");
	
	EidosAssertScriptSuccess("identical(sapply(1:6, 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(1:6, 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), 2,4,6);", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, nrow=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), matrix(c(2,4,6), nrow=1));", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, ncol=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), matrix(c(2,4,6), ncol=1));", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), matrix(c(2,4,6), ncol=2));", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;'), c(1,3,3,5,5,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='vector'), c(1,3,3,5,5,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='matrix'), matrix(c(1,3,3,5,5,7), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='match'), c(1,3,3,5,5,7));", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), array(c(2,4,6), c(2,1,3)));", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;'), c(1,3,3,5,5,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='vector'), c(1,3,3,5,5,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='matrix'), matrix(c(1,3,3,5,5,7), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='match'), c(1,3,3,5,5,7));", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else applyValue;'), c(1,3,2,3,5,4,5,7,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else applyValue;', simplify='vector'), c(1,3,2,3,5,4,5,7,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else applyValue;', simplify='matrix'), matrix(c(1,3,2,3,5,4,5,7,6), nrow=2));", 10, "not of a consistent length");
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else applyValue;', simplify='match'), c(1,3,2,3,5,4,5,7,6));", 10, "not all singletons");
}

void _RunFunctionMiscTests(void)
{
	// beep() – this is commented out by default since it would confuse people if the Eidos self-test beeped...
	//EidosAssertScriptSuccess("beep();", gStaticEidosValueNULL);
	//EidosAssertScriptSuccess("beep('Submarine');", gStaticEidosValueNULL);
	
	// citation()
	EidosAssertScriptSuccess("citation();", gStaticEidosValueVOID);
	EidosAssertScriptRaise("citation(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation(_Test(7));", 0, "too many arguments supplied");
	
	// clock()
	EidosAssertScriptSuccess("c = clock(); isFloat(c);", gStaticEidosValue_LogicalT);
	
	// date()
	EidosAssertScriptSuccess("size(strsplit(date(), '-'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptRaise("date(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date(_Test(7));", 0, "too many arguments supplied");
	
	// defineConstant()
	EidosAssertScriptSuccess("defineConstant('foo', 5:10); sum(foo);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(45)));
	EidosAssertScriptRaise("defineConstant('T', 5:10);", 0, "is already defined");
	EidosAssertScriptRaise("defineConstant('foo', 5:10); defineConstant('foo', 5:10); sum(foo);", 29, "is already defined");
	EidosAssertScriptRaise("foo = 5:10; defineConstant('foo', 5:10); sum(foo);", 12, "is already defined");
	EidosAssertScriptRaise("defineConstant('foo', 5:10); rm('foo');", 29, "cannot be removed");
	
	// doCall()
	EidosAssertScriptSuccess("abs(doCall('sin', 0.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(doCall('sin', PI/2) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("doCall('sin');", 0, "requires 1 argument(s), but 0 are supplied");
	EidosAssertScriptRaise("doCall('sin', 'bar');", 0, "cannot be type string");
	EidosAssertScriptRaise("doCall('sin', 0, 1);", 0, "requires at most 1 argument");
	EidosAssertScriptRaise("doCall('si', 0, 1);", 0, "unrecognized function name");
	
	// executeLambda()
	EidosAssertScriptSuccess("x=7; executeLambda('x^2;');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(49)));
	EidosAssertScriptRaise("x=7; executeLambda('x^2');", 5, "unexpected token");
	EidosAssertScriptRaise("x=7; executeLambda(c('x^2;', '5;'));", 5, "must be a singleton");
	EidosAssertScriptRaise("x=7; executeLambda(string(0));", 5, "must be a singleton");
	EidosAssertScriptSuccess("x=7; executeLambda('x=x^2+4;'); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(53)));
	EidosAssertScriptRaise("executeLambda(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(T);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(3);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("x=2; for (i in 1:2) executeLambda('semanticError;'); x;", 20, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; for (i in 1:2) executeLambda(y); x;", 40, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; for (i in 1:2) executeLambda(y[T]); x;", 40, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; for (i in 1:2) executeLambda('syntax Error;'); x;", 20, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; for (i in 1:2) executeLambda(y); x;", 39, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; for (i in 1:2) executeLambda(y[T]); x;", 39, "unexpected token '@Error'");
	EidosAssertScriptSuccess("x=2; for (i in 1:2) executeLambda('x=x^2;'); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16)));
	EidosAssertScriptSuccess("x=2; y='x=x^2;'; for (i in 1:2) executeLambda(y); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16)));
	EidosAssertScriptSuccess("x=2; y='x=x^2;'; for (i in 1:2) executeLambda(y[T]); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16)));
	
	EidosAssertScriptSuccess("x=7; executeLambda('x^2;', T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(49)));
	EidosAssertScriptRaise("x=7; executeLambda('x^2', T);", 5, "unexpected token");
	EidosAssertScriptRaise("x=7; executeLambda(c('x^2;', '5;'), T);", 5, "must be a singleton");
	EidosAssertScriptRaise("x=7; executeLambda(string(0), T);", 5, "must be a singleton");
	EidosAssertScriptSuccess("x=7; executeLambda('x=x^2+4;', T); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(53)));
	EidosAssertScriptRaise("executeLambda(NULL, T);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(T, T);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(3, T);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(3.5, T);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(_Test(7), T);", 0, "cannot be type");
	EidosAssertScriptRaise("x=2; for (i in 1:2) executeLambda('semanticError;', T); x;", 20, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; for (i in 1:2) executeLambda(y, T); x;", 40, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; for (i in 1:2) executeLambda(y[T], T); x;", 40, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; for (i in 1:2) executeLambda('syntax Error;', T); x;", 20, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; for (i in 1:2) executeLambda(y, T); x;", 39, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; for (i in 1:2) executeLambda(y[T], T); x;", 39, "unexpected token '@Error'");
	EidosAssertScriptSuccess("x=2; for (i in 1:2) executeLambda('x=x^2;', T); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16)));
	EidosAssertScriptSuccess("x=2; y='x=x^2;'; for (i in 1:2) executeLambda(y, T); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16)));
	EidosAssertScriptSuccess("x=2; y='x=x^2;'; for (i in 1:2) executeLambda(y[T], T); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16)));

	// exists()
	EidosAssertScriptSuccess("exists('T');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("exists('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("foo = 5:10; exists('foo');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("foo = 5:10; rm('foo'); exists('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("defineConstant('foo', 5:10); exists('foo');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("a=5; c=7.0; g='foo'; exists(c('a', 'b', 'c', 'd', 'e', 'f', 'g'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false, false, false, true}));
	EidosAssertScriptSuccess("exists(c('T', 'Q', 'F', 'PW', 'PI', 'D', 'E'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false, true, false, true}));
	
	// functionSignature()
	EidosAssertScriptSuccess("functionSignature();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("functionSignature('functionSignature');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("functionSignature('foo');", gStaticEidosValueVOID);	// does not throw at present
	EidosAssertScriptRaise("functionSignature(string(0));", 0, "must be a singleton");
	EidosAssertScriptSuccess("functionSignature(NULL);", gStaticEidosValueVOID);		// same as omitting the parameter
	EidosAssertScriptRaise("functionSignature(T);", 0, "cannot be type");
	EidosAssertScriptRaise("functionSignature(3);", 0, "cannot be type");
	EidosAssertScriptRaise("functionSignature(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("functionSignature(_Test(7));", 0, "cannot be type");
	
	// ls()
	EidosAssertScriptSuccess("ls();", gStaticEidosValueVOID);
	EidosAssertScriptRaise("ls(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls(_Test(7));", 0, "too many arguments supplied");
	
	// license()
	EidosAssertScriptSuccess("license();", gStaticEidosValueVOID);
	EidosAssertScriptRaise("license(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(_Test(7));", 0, "too many arguments supplied");
	
	// rm()
	EidosAssertScriptSuccess("rm();", gStaticEidosValueVOID);
	EidosAssertScriptRaise("x=37; rm('x'); x;", 15, "undefined identifier");
	EidosAssertScriptSuccess("x=37; rm('y'); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(37)));
	EidosAssertScriptRaise("x=37; rm(); x;", 12, "undefined identifier");
	EidosAssertScriptRaise("rm(3);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("rm(T);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(F);", 0, "cannot be type");
	EidosAssertScriptSuccess("rm(NULL);", gStaticEidosValueVOID);		// same as omitting the parameter
	EidosAssertScriptRaise("rm(INF);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(NAN);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(E);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(PI);", 0, "cannot be type");
	EidosAssertScriptRaise("rm('PI');", 0, "intrinsic Eidos constant");
	EidosAssertScriptRaise("rm('PI', T);", 0, "intrinsic Eidos constant");
	EidosAssertScriptRaise("defineConstant('foo', 1:10); rm('foo'); foo;", 29, "is a constant");
	EidosAssertScriptRaise("defineConstant('foo', 1:10); rm('foo', T); foo;", 43, "undefined identifier");
	
	// setSeed()
	EidosAssertScriptSuccess("setSeed(5); x=runif(10); setSeed(5); y=runif(10); all(x==y);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSeed(5); x=runif(10); setSeed(6); y=runif(10); all(x==y);", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("setSeed(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed(T);", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed(_Test(7));", 0, "cannot be type");
	
	// getSeed()
	EidosAssertScriptSuccess("setSeed(13); getSeed();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(13)));
	EidosAssertScriptSuccess("setSeed(13); setSeed(7); getSeed();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptRaise("getSeed(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed(_Test(7));", 0, "too many arguments supplied");
	
	// source()
	EidosAssertScriptSuccess("path = '/tmp/EidosSourceTest.txt'; writeFile(path, 'x=9*9;'); source(path); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(81)));
	
	// stop()
	EidosAssertScriptRaise("stop();", 0, "stop() called");
	EidosAssertScriptRaise("stop('Error');", 0, "stop(\"Error\") called");
	EidosAssertScriptRaise("stop(NULL);", 0, "stop() called");		// same as omitting the parameter
	EidosAssertScriptRaise("stop(T);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(3);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(_Test(7));", 0, "cannot be type");
	
	// suppressWarnings()
	EidosAssertScriptSuccess("suppressWarnings(F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("suppressWarnings(T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("suppressWarnings(T); suppressWarnings(F);", gStaticEidosValue_LogicalT);
	
	// system()
	EidosAssertScriptRaise("system('');", 0, "non-empty command string");
	EidosAssertScriptSuccess("system('expr 5 + 5');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("10")));
	EidosAssertScriptSuccess("system('expr', args=c('5', '+', '5'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("10")));
	EidosAssertScriptSuccess("err = system('expr 5 / 0', stderr=T); (err == 'expr: division by zero') | (err == 'expr: división por cero') | (err == 'expr: division par zéro') | (substr(err, 0, 5) == 'expr: ');", gStaticEidosValue_LogicalT);	// unfortunately system localization makes the message returned vary
	EidosAssertScriptSuccess("system('printf foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("system(\"printf 'foo bar baz' | wc -m | sed 's/ //g'\");", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("11")));
	EidosAssertScriptSuccess("system(\"(wc -l | sed 's/ //g')\", input='foo\\nbar\\nbaz\\n');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("3")));
	EidosAssertScriptSuccess("system(\"(wc -l | sed 's/ //g')\", input=c('foo', 'bar', 'baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("3")));
	EidosAssertScriptSuccess("system(\"echo foo; echo bar; echo baz;\");", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar", "baz"}));
	
	// time()
	EidosAssertScriptSuccess("size(strsplit(time(), ':'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptRaise("time(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time(_Test(7));", 0, "too many arguments supplied");
	
	// usage(); allow zero since this call returns zero on some less-supported platforms
	EidosAssertScriptSuccess("usage() >= 0.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("usage(F) >= 0.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("usage(T) >= 0.0;", gStaticEidosValue_LogicalT);
	
	// version()
	EidosAssertScriptSuccess("type(version(T)) == 'float';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("type(version(F)) == 'float';", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("version(NULL);", 0, "cannot be type NULL");
	EidosAssertScriptRaise("version(3);", 0, "cannot be type integer");
	EidosAssertScriptRaise("version(3.5);", 0, "cannot be type float");
	EidosAssertScriptRaise("version('foo');", 0, "cannot be type string");
	EidosAssertScriptRaise("version(_Test(7));", 0, "cannot be type object");
}

#pragma mark methods
void _RunMethodTests(void)
{
	// methodSignature()
	EidosAssertScriptSuccess("_Test(7).methodSignature();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("_Test(7).methodSignature('methodSignature');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("matrix(_Test(7)).methodSignature('methodSignature');", gStaticEidosValueVOID);
	
	// propertySignature()
	EidosAssertScriptSuccess("_Test(7).propertySignature();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("_Test(7).propertySignature('_yolk');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("matrix(_Test(7)).propertySignature('_yolk');", gStaticEidosValueVOID);
	
	// size() / length()
	EidosAssertScriptSuccess("_Test(7).size();", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("rep(_Test(7), 5).size();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("matrix(rep(_Test(7), 5)).size();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	
	EidosAssertScriptSuccess("_Test(7).length();", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("rep(_Test(7), 5).length();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("matrix(rep(_Test(7), 5)).length();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	
	// str()
	EidosAssertScriptSuccess("_Test(7).str();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("c(_Test(7), _Test(8), _Test(9)).str();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("matrix(_Test(7)).str();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("matrix(c(_Test(7), _Test(8), _Test(9))).str();", gStaticEidosValueVOID);
}

#pragma mark code examples
void _RunCodeExampleTests(void)
{
	// Fibonacci sequence; see Eidos manual section 2.6.1-ish
	EidosAssertScriptSuccess(	"fib = c(1, 1);												\
								while (size(fib) < 20)										\
								{															\
									next_fib = fib[size(fib) - 1] + fib[size(fib) - 2];		\
									fib = c(fib, next_fib);									\
								}															\
								fib;",
							 EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1,1,2,3,5,8,13,21,34,55,89,144,233,377,610,987,1597,2584,4181,6765}));
	
	EidosAssertScriptSuccess(	"counter = 12;							\
								factorial = 1;							\
								do										\
								{										\
									factorial = factorial * counter;	\
									counter = counter - 1;				\
								}										\
								while (counter > 0);					\
								factorial;",
							 EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(479001600)));
	
	EidosAssertScriptSuccess(	"last = 200;				\
								p = integer(0);				\
								x = 2:last;					\
								lim = last^0.5;				\
								do {						\
									v = x[0];				\
									if (v > lim)			\
										break;				\
									p = c(p, v);			\
									x = x[x % v != 0];		\
								} while (T);				\
								c(p, x);",
							 EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199}));
}

#pragma mark user-defined functions
void _RunUserDefinedFunctionTests(void)
{
	// Basic functionality
	EidosAssertScriptSuccess("function (i)plus(i x) { return x + 1; } plus(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(6)));
	EidosAssertScriptSuccess("function (f)plus(f x) { return x + 1; } plus(5.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(6.0)));
	EidosAssertScriptSuccess("function (fi)plus(fi x) { return x + 1; } plus(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(6)));
	EidosAssertScriptSuccess("function (fi)plus(fi x) { return x + 1; } plus(5.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(6.0)));
	EidosAssertScriptSuccess("function (fi)plus(fi x) { return x + 1; } plus(c(5, 6, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{6, 7, 8}));
	EidosAssertScriptSuccess("function (fi)plus(fi x) { return x + 1; } plus(c(5.0, 6.0, 7.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{6.0, 7.0, 8.0}));
	
	EidosAssertScriptSuccess("function (l$)nor(l$ x, l$ y) { return !(x | y); } nor(F, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("function (l$)nor(l$ x, l$ y) { return !(x | y); } nor(T, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("function (l$)nor(l$ x, l$ y) { return !(x | y); } nor(F, T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("function (l$)nor(l$ x, l$ y) { return !(x | y); } nor(T, T);", gStaticEidosValue_LogicalF);
	
	EidosAssertScriptSuccess("function (s)append(s x, s y) { return x + ',' + y; } append('foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo,bar")));
	EidosAssertScriptSuccess("function (s)append(s x, s y) { return x + ',' + y; } append('foo', c('bar','baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo,bar", "foo,baz"}));
	
	// Recursion
	EidosAssertScriptSuccess("function (i)fac([i b=10]) { if (b <= 1) return 1; else return b*fac(b-1); } fac(3); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(6)));
	EidosAssertScriptSuccess("function (i)fac([i b=10]) { if (b <= 1) return 1; else return b*fac(b-1); } fac(5); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(120)));
	EidosAssertScriptSuccess("function (i)fac([i b=10]) { if (b <= 1) return 1; else return b*fac(b-1); } fac(); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3628800)));
	
	EidosAssertScriptSuccess("function (s)star(i x) { if (x <= 0) return ''; else return '*' + star(x - 1); } star(5); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("*****")));
	EidosAssertScriptSuccess("function (s)star(i x) { if (x <= 0) return ''; else return '*' + star(x - 1); } star(10); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("**********")));
	EidosAssertScriptSuccess("function (s)star(i x) { if (x <= 0) return ''; else return '*' + star(x - 1); } star(0); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("")));
	
	// Type-checking
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(NULL);", 35, "argument 1 (x) cannot be type NULL");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(T);", 35, "argument 1 (x) cannot be type logical");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(5);", 35, "return value cannot be type integer");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(5.0);", 35, "argument 1 (x) cannot be type float");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo('foo');", 35, "argument 1 (x) cannot be type string");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(_Test(7));", 35, "argument 1 (x) cannot be type object");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo();", 35, "missing required argument x");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(5, 6);", 35, "too many arguments supplied");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(x=5);", 35, "return value cannot be type integer");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(y=5);", 35, "named argument y skipped over required argument x");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(x=5, y=5);", 35, "unrecognized named argument y");
	
	// Mutual recursion
	EidosAssertScriptSuccess("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return foo(x - 1); } foo(5); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(16)));
	EidosAssertScriptSuccess("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return foo(x - 1); } foo(10); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(56)));
	EidosAssertScriptSuccess("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return foo(x - 1); } foo(-10); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-9)));
	
	EidosAssertScriptSuccess("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return baz(x - 1); } function (i)baz(i x) { return x * foo(x); } foo(5); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(153)));
	EidosAssertScriptSuccess("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return baz(x - 1); } function (i)baz(i x) { return x * foo(x); } foo(10); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2335699)));
	EidosAssertScriptSuccess("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return baz(x - 1); } function (i)baz(i x) { return x * foo(x); } foo(-10); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-9)));
	
	// Scoping
	EidosAssertScriptRaise("defineConstant('x', 10); function (i)plus(i x) { return x + 1; } plus(5);", 65, "cannot be redefined because it is a constant");
	EidosAssertScriptRaise("defineConstant('x', 10); function (i)plus(i y) { x = y + 1; return x; } plus(5);", 72, "cannot be redefined because it is a constant");
	EidosAssertScriptSuccess("defineConstant('x', 10); function (i)plus(i y) { return x + y; } plus(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(15)));
	EidosAssertScriptRaise("x = 10; function (i)plus(i y) { return x + y; } plus(5);", 48, "undefined identifier x");
	EidosAssertScriptSuccess("defineConstant('x', 10); y = 1; function (i)plus(i y) { return x + y; } plus(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(15)));
	EidosAssertScriptSuccess("defineConstant('x', 10); y = 1; function (i)plus(i y) { return x + y; } plus(5); y; ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(1)));
	EidosAssertScriptSuccess("defineConstant('x', 10); y = 1; function (i)plus(i y) { y = y + 1; return x + y; } plus(5); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(16)));
	EidosAssertScriptSuccess("defineConstant('x', 10); y = 1; function (i)plus(i y) { y = y + 1; return x + y; } plus(5); y; ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(1)));
	EidosAssertScriptSuccess("function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(6)));
	EidosAssertScriptSuccess("function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); x; ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptRaise("function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); y; ", 81, "undefined identifier y");
	EidosAssertScriptRaise("function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); plus(5); ", 81, "identifier 'x' is already defined");
	EidosAssertScriptRaise("x = 3; function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); x; ", 79, "identifier 'x' is already defined");
	EidosAssertScriptSuccess("function (i)plus(i y) { foo(); y = y + 1; return y; } function (void)foo(void) { defineConstant('x', 10); } plus(5); x; ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptRaise("function (i)plus(i x) { foo(); x = x + 1; return x; } function (void)foo(void) { defineConstant('x', 10); } plus(5); x; ", 108, "identifier 'x' is already defined");
	EidosAssertScriptRaise("x = 3; function (i)plus(i y) { foo(); y = y + 1; return y; } function (void)foo(void) { defineConstant('x', 10); } plus(5); x; ", 115, "identifier 'x' is already defined");
	
	// Mutual recursion with lambdas
	
	
	// Tests mimicking built-in Eidos functions; these are good for testing user-defined functions, but also good for testing our built-ins!
	const std::string &builtins_test_string =
#include "eidos_test_builtins.h"
	;
	std::vector<std::string> test_strings = Eidos_string_split(builtins_test_string, "// ***********************************************************************************************");
	
	//for (int testidx = 0; testidx < 100; testidx++)	// uncomment this for a more thorough stress test
	{
		for (std::string &test_string : test_strings)
		{
			std::string test_string_fixed = test_string + "\nreturn T;\n";
			
			EidosAssertScriptSuccess(test_string_fixed, gStaticEidosValue_LogicalT);
		}
	}
}

#pragma mark void EidosValue
void _RunVoidEidosValueTests(void)
{
	// void$ or NULL$ as a type-specifier is not legal, semantically; likewise with similar locutions
	EidosAssertScriptRaise("function (void$)foo(void) { return; } foo();", 14, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (void)foo(void$) { return; } foo();", 23, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (NULL$)foo(void) { return NULL; } foo();", 14, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (void)foo(NULL$) { return; } foo(NULL);", 23, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (v$)foo(void) { return NULL; } foo();", 11, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (void)foo(v$) { return; } foo(NULL);", 20, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (N$)foo(void) { return NULL; } foo();", 11, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (void)foo(N$) { return; } foo(NULL);", 20, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (vN$)foo(void) { return NULL; } foo();", 12, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (void)foo(vN$) { return; } foo(NULL);", 21, "may not be declared to be singleton");
	
	// functions declared to return void must return void
	EidosAssertScriptSuccess("function (void)foo(void) { 5; } foo();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("function (void)foo(void) { 5; return; } foo();", gStaticEidosValueVOID);
	EidosAssertScriptRaise("function (void)foo(void) { return 5; } foo();", 39, "return value must be void");
	EidosAssertScriptRaise("function (void)foo(void) { return NULL; } foo();", 42, "return value must be void");
	
	// functions declared to return NULL must return NULL
	EidosAssertScriptRaise("function (NULL)foo(void) { 5; } foo();", 32, "return value cannot be void");
	EidosAssertScriptRaise("function (NULL)foo(void) { 5; return; } foo();", 40, "return value cannot be void");
	EidosAssertScriptRaise("function (NULL)foo(void) { return 5; } foo();", 39, "return value cannot be type integer");
	EidosAssertScriptSuccess("function (NULL)foo(void) { return NULL; } foo();", gStaticEidosValueNULL);
	
	// functions declared to return * may return anything but void
	EidosAssertScriptRaise("function (*)foo(void) { 5; } foo();", 29, "return value cannot be void");
	EidosAssertScriptRaise("function (*)foo(void) { 5; return; } foo();", 37, "return value cannot be void");
	EidosAssertScriptSuccess("function (*)foo(void) { return 5; } foo();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("function (*)foo(void) { return NULL; } foo();", gStaticEidosValueNULL);
	
	// functions declared to return vNlifso may return anything at all
	EidosAssertScriptSuccess("function (vNlifso)foo(void) { 5; } foo();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("function (vNlifso)foo(void) { 5; return; } foo();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("function (vNlifso)foo(void) { return 5; } foo();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("function (vNlifso)foo(void) { return NULL; } foo();", gStaticEidosValueNULL);
	
	// functions may not be declared as taking a parameter of type void
	EidosAssertScriptRaise("function (void)foo(void x) { return; } foo();", 19, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(void x) { return; } foo(citation());", 19, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo([void x]) { return; } foo(citation());", 20, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(vNlifso x) { return; } foo();", 19, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(vNlifso x) { return; } foo(citation());", 19, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo([vNlifso x = 5]) { return; } foo(citation());", 20, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(integer x, void y) { return; } foo(5);", 30, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(integer x, void y) { return; } foo(5, citation());", 30, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(integer x, [void y]) { return; } foo(5, citation());", 31, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(integer x, vNlifso y) { return; } foo(5);", 30, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(integer x, vNlifso y) { return; } foo(5, citation());", 30, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(integer x, [vNlifso y = 5]) { return; } foo(5, citation());", 31, "void is not allowed");
	
	// functions *may* be declared as taking a parameter of type NULL, or returning NULL; this is new, with the new void support
	// not sure why anybody would want to do this, of course, but hey, ours not to reason why...
	EidosAssertScriptSuccess("function (void)foo(NULL x) { return; } foo(NULL);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("function (void)bar([NULL x = NULL]) { return; } bar(NULL);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("function (void)bar([NULL x = NULL]) { return; } bar();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("function (NULL)foo(NULL x) { return x; } foo(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("function (NULL)bar([NULL x = NULL]) { return x; } bar(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("function (NULL)bar([NULL x = NULL]) { return x; } bar();", gStaticEidosValueNULL);
	
	// functions may not be passed void arguments
	EidosAssertScriptRaise("function (void)foo(void) { return; } foo(citation());", 37, "too many arguments");
	EidosAssertScriptRaise("function (void)foo(* x) { return; } foo();", 36, "missing required argument");
	EidosAssertScriptRaise("function (void)foo(* x) { return; } foo(citation());", 36, "cannot be type void");
	EidosAssertScriptRaise("function (void)foo(* x) { return; } foo(x = citation());", 36, "cannot be type void");
	EidosAssertScriptRaise("function (void)foo([* x = 5]) { return; } foo(x = citation());", 42, "cannot be type void");
	EidosAssertScriptRaise("function (void)foo([* x = 5]) { return; } foo(citation());", 42, "cannot be type void");
	
	// same again, with isNULL(* x)
	EidosAssertScriptRaise("isNULL();", 0, "missing required argument");
	EidosAssertScriptRaise("isNULL(citation());", 0, "cannot be type void");
	
	// same again, with c(...)
	EidosAssertScriptRaise("c(citation());", 0, "cannot be type void");
	EidosAssertScriptRaise("c(5, citation(), 10);", 0, "cannot be type void");
	
	// void may not participate in any operator: [], (), ., + (unary), - (unary), !, ^, :, *, /, %, +, -, <, >, <=, >=, ==, !=, &, |, ?else, =
	// we do not comprehensively test all operand types here, but I think the interpreter code is written such that these tests should suffice
	EidosAssertScriptRaise("citation()[0];", 10, "type void is not supported");
	EidosAssertScriptRaise("citation()[logical(0)];", 10, "type void is not supported");
	EidosAssertScriptRaise("(1:5)[citation()];", 5, "type void is not supported");
	
	EidosAssertScriptRaise("citation()();", 8, "illegal operand for a function call");
	EidosAssertScriptRaise("(citation())();", 9, "illegal operand for a function call");
	EidosAssertScriptSuccess("(citation());", gStaticEidosValueVOID);				// about the only thing that is legal with void!
	
	EidosAssertScriptRaise("citation().test();", 10, "type void is not supported");
	EidosAssertScriptRaise("citation().test = 5;", 16, "type void is not supported");
	
	EidosAssertScriptRaise("+citation();", 0, "type void is not supported");
	
	EidosAssertScriptRaise("-citation();", 0, "type void is not supported");
	
	EidosAssertScriptRaise("!citation();", 0, "type void is not supported");
	
	EidosAssertScriptRaise("citation()^5;", 10, "type void is not supported");
	EidosAssertScriptRaise("5^citation();", 1, "type void is not supported");
	EidosAssertScriptRaise("citation()^citation();", 10, "type void is not supported");
	
	EidosAssertScriptRaise("citation():5;", 10, "type void is not supported");
	EidosAssertScriptRaise("5:citation();", 1, "type void is not supported");
	EidosAssertScriptRaise("citation():citation();", 10, "type void is not supported");
	
	EidosAssertScriptRaise("citation()*5;", 10, "type void is not supported");
	EidosAssertScriptRaise("5*citation();", 1, "type void is not supported");
	EidosAssertScriptRaise("citation()*citation();", 10, "type void is not supported");
	
	EidosAssertScriptRaise("citation()/5;", 10, "type void is not supported");
	EidosAssertScriptRaise("5/citation();", 1, "type void is not supported");
	EidosAssertScriptRaise("citation()/citation();", 10, "type void is not supported");
	
	EidosAssertScriptRaise("citation()%5;", 10, "type void is not supported");
	EidosAssertScriptRaise("5%citation();", 1, "type void is not supported");
	EidosAssertScriptRaise("citation()%citation();", 10, "type void is not supported");
	
	EidosAssertScriptRaise("5 + citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() + 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() + citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 - citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() - 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() - citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 < citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() < 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() < citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 > citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() > 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() > citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 <= citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() <= 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() <= citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 >= citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() >= 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() >= citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 == citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() == 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() == citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 != citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() != 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() != citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("T & citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() & T;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() & citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("T | citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() | T;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() | citation();", 11, "type void is not supported");
	
	EidosAssertScriptSuccess("T ? citation() else F;", gStaticEidosValueVOID);		// also legal with void, as long as you don't try to use the result...
	EidosAssertScriptSuccess("F ? citation() else F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T ? F else citation();", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F ? F else citation();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("T ? citation() else citation();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("F ? citation() else citation();", gStaticEidosValueVOID);
	EidosAssertScriptRaise("citation() ? T else F;", 11, "size() != 1");
	
	EidosAssertScriptRaise("x = citation();", 2, "void may never be assigned");
	
	// void may not be used in while, do-while, for, etc.
	EidosAssertScriptRaise("if (citation()) T;", 0, "size() != 1");
	EidosAssertScriptRaise("if (citation()) T; else F;", 0, "size() != 1");
	EidosAssertScriptSuccess("if (T) citation(); else citation();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (F) citation(); else citation();", gStaticEidosValueVOID);
	
	EidosAssertScriptRaise("while (citation()) F;", 0, "size() != 1");
	
	EidosAssertScriptRaise("do F; while (citation());", 0, "size() != 1");
	
	EidosAssertScriptRaise("for (x in citation()) T;", 0, "does not allow void");
}






























































