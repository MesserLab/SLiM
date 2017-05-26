//
//  eidos_test.cpp
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
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


using std::string;
using std::vector;
using std::map;
using std::endl;
using std::istringstream;
using std::istream;
using std::ostream;


#if (!EIDOS_HAS_OVERFLOW_BUILTINS)
std::cout << "WARNING: This build of Eidos does not detect integer arithmetic overflows.  Compiling Eidos with GCC version 5.0 or later, or Clang version 3.9 or later, is required for this feature.  This means that integer addition, subtraction, or multiplication that overflows the 64-bit range of Eidos (" << INT64_MIN << " to " << INT64_MAX << ") will not be detected." << std::endl;
#endif


// Helper functions for testing
void EidosAssertScriptSuccess(const string &p_script_string, EidosValue_SP p_correct_result);
void EidosAssertScriptRaise(const string &p_script_string, const int p_bad_position, const std::string &p_reason_snip);

// Keeping records of test success / failure
static int gEidosTestSuccessCount = 0;
static int gEidosTestFailureCount = 0;


// Instantiates and runs the script, and prints an error if the result does not match expectations
void EidosAssertScriptSuccess(const string &p_script_string, EidosValue_SP p_correct_result)
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
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during Tokenize(): " << EidosGetTrimmedRaiseMessage() << endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		return;
	}
	
	try {
		script.ParseInterpreterBlockToAST();
	}
	catch (...)
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during ParseToAST(): " << EidosGetTrimmedRaiseMessage() << endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		return;
	}
	
	try {
		EidosInterpreter interpreter(script, symbol_table, *EidosInterpreter::BuiltInFunctionMap(), nullptr);
		
		result = interpreter.EvaluateInterpreterBlock(true);
	}
	catch (...)
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during EvaluateInterpreterBlock(): " << EidosGetTrimmedRaiseMessage() << endl;
		
		gEidosCurrentScript = nullptr;
		gEidosExecutingRuntimeScript = false;
		
		return;
	}
	
	// Check that the result is actually what we want it to be
	if (!result)
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : return value is nullptr" << endl;
	}
	else if (result->Type() != p_correct_result->Type())
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : unexpected return type (" << result->Type() << ", expected " << p_correct_result->Type() << ")" << endl;
	}
	else if (result->ElementType() != p_correct_result->ElementType())
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : unexpected return element type (" << result->ElementType() << ", expected " << p_correct_result->ElementType() << ")" << endl;
	}
	else if (result->Count() != p_correct_result->Count())
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : unexpected return length (" << result->Count() << ", expected " << p_correct_result->Count() << ")" << endl;
	}
	else
	{
		for (int value_index = 0; value_index < result->Count(); ++value_index)
		{
			if (CompareEidosValues(*result, value_index, *p_correct_result, value_index, nullptr) != 0)
			{
				std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : mismatched values (" << *result << "), expected (" << *p_correct_result << ")" << endl;
				
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
void EidosAssertScriptRaise(const string &p_script_string, const int p_bad_position, const std::string &p_reason_snip)
{
	EidosScript script(p_script_string);
	EidosSymbolTable symbol_table(EidosSymbolTableType::kVariablesTable, gEidosConstantsSymbolTable);
	
	gEidosCurrentScript = &script;
	
	try {
		script.Tokenize();
		script.ParseInterpreterBlockToAST();
		
		EidosInterpreter interpreter(script, symbol_table, *EidosInterpreter::BuiltInFunctionMap(), nullptr);
		
		EidosValue_SP result = interpreter.EvaluateInterpreterBlock(true);
		
		gEidosTestFailureCount++;
		
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : no raise during EvaluateInterpreterBlock()." << endl;
	}
	catch (...)
	{
		// We need to call EidosGetTrimmedRaiseMessage() here to empty the error stringstream, even if we don't log the error
		std::string raise_message = EidosGetTrimmedRaiseMessage();
		
		if (raise_message.find(p_reason_snip) != string::npos)
		{
			if ((gEidosCharacterStartOfError == -1) || (gEidosCharacterEndOfError == -1) || !gEidosCurrentScript)
			{
				gEidosTestFailureCount++;
				
				std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise expected, but no error info set" << endl;
				std::cerr << p_script_string << "   raise message: " << raise_message << endl;
				std::cerr << "--------------------" << std::endl << std::endl;
			}
			else if (gEidosCharacterStartOfError != p_bad_position)
			{
				gEidosTestFailureCount++;
				
				std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise expected, but error position unexpected" << endl;
				std::cerr << p_script_string << "   raise message: " << raise_message << endl;
				eidos_log_script_error(std::cerr, gEidosCharacterStartOfError, gEidosCharacterEndOfError, gEidosCurrentScript, gEidosExecutingRuntimeScript);
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
			
			std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise message mismatch (expected \"" << p_reason_snip << "\")." << endl;
			std::cerr << "   raise message: " << raise_message << endl;
			std::cerr << "--------------------" << std::endl << std::endl;
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
static void _RunOperatorPlusTests(void);
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
static void _RunKeywordIfTests(void);
static void _RunKeywordDoTests(void);
static void _RunKeywordWhileTests(void);
static void _RunKeywordForInTests(void);
static void _RunKeywordNextTests(void);
static void _RunKeywordBreakTests(void);
static void _RunKeywordReturnTests(void);
static void _RunFunctionMathTests(void);
static void _RunFunctionSummaryStatsTests(void);
static void _RunFunctionVectorConstructionTests(void);
static void _RunFunctionValueInspectionManipulationTests(void);
static void _RunFunctionValueTestingCoercionTests(void);
static void _RunFunctionFilesystemTests(void);
static void _RunColorManipulationTests(void);
static void _RunFunctionMiscTests(void);
static void _RunMethodTests(void);
static void _RunCodeExampleTests(void);


void RunEidosTests(void)
{
	// Reset error counts
	gEidosTestSuccessCount = 0;
	gEidosTestFailureCount = 0;
	
	// Run tests
	_RunLiteralsIdentifiersAndTokenizationTests();
	_RunSymbolsAndVariablesTests();
	_RunParsingTests();
	_RunFunctionDispatchTests();
	_RunRuntimeErrorTests();
	_RunVectorsAndSingletonsTests();
	_RunOperatorPlusTests();
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
	_RunKeywordIfTests();
	_RunKeywordDoTests();
	_RunKeywordWhileTests();
	_RunKeywordForInTests();
	_RunKeywordNextTests();
	_RunKeywordBreakTests();
	_RunKeywordReturnTests();
	_RunFunctionMathTests();
	_RunFunctionSummaryStatsTests();
	_RunFunctionVectorConstructionTests();
	_RunFunctionValueInspectionManipulationTests();
	_RunFunctionValueTestingCoercionTests();
	_RunFunctionFilesystemTests();
	_RunColorManipulationTests();
	_RunFunctionMiscTests();
	_RunMethodTests();
	_RunCodeExampleTests();
	
	// ************************************************************************************
	//
	//	Print a summary of test results
	//
	std::cerr << endl;
	if (gEidosTestFailureCount)
		std::cerr << "" << EIDOS_OUTPUT_FAILURE_TAG << " count: " << gEidosTestFailureCount << endl;
	std::cerr << EIDOS_OUTPUT_SUCCESS_TAG << " count: " << gEidosTestSuccessCount << endl;
	
	// If we are tracking allocations, print a count
#ifdef EIDOS_TRACK_VALUE_ALLOCATION
	std::cerr << "EidosValue allocation count: " << EidosValue::valueTrackingCount << endl;
	for (EidosValue *value : EidosValue::valueTrackingVector)
		std::cerr << *value << endl;
#endif
	
	// Do some tests of our custom math functions
#if 0
	//#ifndef USE_GSL_POISSON
	EidosInitializeRNGFromSeed(EidosGenerateSeedFromPIDAndTime());
	
	double total;
	int i;
	
	std::cout << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += eidos_fast_ran_poisson(1.0);
	
	std::cout << "eidos_fast_ran_poisson(1.0): mean = " << (total / 1000000) << ", expected 1.0" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += gsl_ran_poisson(gEidos_rng, 1.0);
	
	std::cout << "gsl_ran_poisson(1.0): mean = " << (total / 1000000) << ", expected 1.0" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += eidos_fast_ran_poisson(0.001);
	
	std::cout << "eidos_fast_ran_poisson(0.001): mean = " << (total / 1000000) << ", expected 0.001" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += gsl_ran_poisson(gEidos_rng, 0.001);
	
	std::cout << "gsl_ran_poisson(0.001): mean = " << (total / 1000000) << ", expected 0.001" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += eidos_fast_ran_poisson(0.00001);
	
	std::cout << "eidos_fast_ran_poisson(0.00001): mean = " << (total / 1000000) << ", expected 0.00001" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += gsl_ran_poisson(gEidos_rng, 0.00001);
	
	std::cout << "gsl_ran_poisson(0.00001): mean = " << (total / 1000000) << ", expected 0.00001" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 100000; i++)
		total += eidos_fast_ran_poisson(100);
	
	std::cout << "eidos_fast_ran_poisson(100): mean = " << (total / 100000) << ", expected 100" << std::endl;
	
	for (total = 0.0, i = 0; i < 100000; i++)
		total += gsl_ran_poisson(gEidos_rng, 100);
	
	std::cout << "gsl_ran_poisson(100): mean = " << (total / 100000) << ", expected 100" << std::endl << std::endl;
	
	
	std::cout << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += eidos_fast_ran_poisson(1.0, exp(-1.0));
	
	std::cout << "eidos_fast_ran_poisson(1.0): mean = " << (total / 1000000) << ", expected 1.0" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += gsl_ran_poisson(gEidos_rng, 1.0);
	
	std::cout << "gsl_ran_poisson(1.0): mean = " << (total / 1000000) << ", expected 1.0" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += eidos_fast_ran_poisson(0.001, exp(-0.001));
	
	std::cout << "eidos_fast_ran_poisson(0.001): mean = " << (total / 1000000) << ", expected 0.001" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += gsl_ran_poisson(gEidos_rng, 0.001);
	
	std::cout << "gsl_ran_poisson(0.001): mean = " << (total / 1000000) << ", expected 0.001" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += eidos_fast_ran_poisson(0.00001, exp(-0.00001));
	
	std::cout << "eidos_fast_ran_poisson(0.00001): mean = " << (total / 1000000) << ", expected 0.00001" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += gsl_ran_poisson(gEidos_rng, 0.00001);
	
	std::cout << "gsl_ran_poisson(0.00001): mean = " << (total / 1000000) << ", expected 0.00001" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 100000; i++)
		total += eidos_fast_ran_poisson(100, exp(-100));
	
	std::cout << "eidos_fast_ran_poisson(100): mean = " << (total / 100000) << ", expected 100" << std::endl;
	
	for (total = 0.0, i = 0; i < 100000; i++)
		total += gsl_ran_poisson(gEidos_rng, 100);
	
	std::cout << "gsl_ran_poisson(100): mean = " << (total / 100000) << ", expected 100" << std::endl << std::endl;
	
	
	std::cout << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += eidos_fast_ran_poisson_nonzero(1.0, exp(-1.0));
	
	std::cout << "eidos_fast_ran_poisson(1.0): mean = " << (total / 1000000) << ", expected ~1.58" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
	{
		unsigned int x;
		
		do {
			x = gsl_ran_poisson(gEidos_rng, 1.0);
		} while (x == 0);
		
		total += x;
	}
	
	std::cout << "gsl_ran_poisson(1.0): mean = " << (total / 1000000) << ", expected ~1.58" << std::endl << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += eidos_fast_ran_poisson_nonzero(0.001, exp(-0.001));
	
	std::cout << "eidos_fast_ran_poisson(0.001): mean = " << (total / 1000000) << ", expected ~1.0005" << std::endl;
	
	//	for (total = 0.0, i = 0; i < 1000000; i++)
	//	{
	//		unsigned int x;
	//		
	//		do {
	//			x = gsl_ran_poisson(gEidos_rng, 0.001);
	//		} while (x == 0);
	//		
	//		total += x;
	//	}
	//	
	//	std::cout << "gsl_ran_poisson(0.001): mean = " << (total / 1000000) << ", expected ~1.0005" << std::endl;
	
	for (total = 0.0, i = 0; i < 1000000; i++)
		total += eidos_fast_ran_poisson_nonzero(0.00001, exp(-0.00001));
	
	std::cout << std::endl << "eidos_fast_ran_poisson(0.00001): mean = " << (total / 1000000) << ", expected ~1.00001" << std::endl;
	
	//	for (total = 0.0, i = 0; i < 1000000; i++)
	//	{
	//		unsigned int x;
	//		
	//		do {
	//			x = gsl_ran_poisson(gEidos_rng, 0.00001);
	//		} while (x == 0);
	//		
	//		total += x;
	//	}
	//	
	//	std::cout << "gsl_ran_poisson(0.00001): mean = " << (total / 1000000) << ", expected ~1.00001" << std::endl;
	
	for (total = 0.0, i = 0; i < 100000; i++)
		total += eidos_fast_ran_poisson_nonzero(100, exp(-100));
	
	std::cout << std::endl << "eidos_fast_ran_poisson(100): mean = " << (total / 100000) << ", expected ~100" << std::endl;
	
	for (total = 0.0, i = 0; i < 100000; i++)
	{
		unsigned int x;
		
		do {
			x = gsl_ran_poisson(gEidos_rng, 100);
		} while (x == 0);
		
		total += x;
	}
	
	std::cout << "gsl_ran_poisson(100): mean = " << (total / 100000) << ", expected ~100" << std::endl << std::endl;
	
#endif
	
#if 0
	// Speed tests of gsl_ran_poisson() vs. eidos_fast_ran_poisson()
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
		
		exp_neg_mu = eidos_fast_ran_poisson_PRECALCULATE(mu);
		
		for (int type = 0; type <= 2; ++type)
		{
			double start_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
			double total = 0.0;
			
			if (type == 0)
			{
				for (int i = 0; i < 1000000; i++)
					total += eidos_fast_ran_poisson(mu);
			}
			else if (type == 1)
			{
				for (int i = 0; i < 1000000; i++)
					total += eidos_fast_ran_poisson(mu, exp_neg_mu);
			}
			else if (type == 2)
			{
				for (int i = 0; i < 1000000; i++)
					total += gsl_ran_poisson(gEidos_rng, mu);
			}
			
			double end_time = static_cast<double>(clock()) / CLOCKS_PER_SEC;
			
			std::cout << "mu " << mu << " T " << type << ": total = " << total << ", time == " << (end_time - start_time) << std::endl;
		}
	}
#endif
	
	// If we ran tests, the random number seed has been set; let's set it back to a good seed value
	EidosInitializeRNGFromSeed(EidosGenerateSeedFromPIDAndTime());
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
	EidosAssertScriptRaise("foo$foo;", 3, "unrecognized token");
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
	EidosAssertScriptRaise("abs(x=-10, x=-10);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("abs(x=-10, y=-10);", 0, "too many arguments supplied");
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
	EidosAssertScriptRaise("seq(1, 3, by=1, by=1);", 0, "too many arguments supplied");
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
	EidosAssertScriptSuccess("doCall(function='abs', -10);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptRaise("doCall(x='abs', -10);", 0, "skipped over required argument");
	EidosAssertScriptRaise("doCall('abs', x=-10);", 0, "named argument x in ellipsis argument section");
	EidosAssertScriptRaise("doCall('abs', function=-10);", 0, "named argument function in ellipsis argument section");
	EidosAssertScriptRaise("doCall(x='abs');", 0, "skipped over required argument");
	EidosAssertScriptRaise("doCall(function='abs');", 0, "requires 1 argument(s), but 0 are supplied");
	
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
void _RunOperatorPlusTests(void)
{
	// operator +
	EidosAssertScriptRaise("NULL+T;", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+0;", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+0.5;", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+'foo';", 4, "does not support operands of type NULL");
	EidosAssertScriptRaise("NULL+_Test(7);", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+(0:2);", 4, "combination of operand types");
	EidosAssertScriptRaise("T+NULL;", 1, "combination of operand types");
	EidosAssertScriptRaise("0+NULL;", 1, "combination of operand types");
	EidosAssertScriptRaise("0.5+NULL;", 3, "combination of operand types");
	EidosAssertScriptRaise("'foo'+NULL;", 5, "does not support operands of type NULL");
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
}

#pragma mark operator []
void _RunOperatorSubsetTests(void)
{
	// operator []
	EidosAssertScriptRaise("x = 1:5; x[NULL];", 10, "is not supported by");
	EidosAssertScriptSuccess("x = 1:5; NULL[x];", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x = 1:5; NULL[NULL];", gStaticEidosValueNULL);
	EidosAssertScriptRaise("x = 1:5; x[];", 11, "unexpected token");
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
}

#pragma mark operator = with []
void _RunOperatorAssignTests(void)
{
	// operator =
	EidosAssertScriptRaise("E = 7;", 2, "cannot be redefined because it is a constant");
	EidosAssertScriptRaise("E = E + 7;", 2, "cannot assign into a constant");
	
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
	EidosAssertScriptRaise("x = 1:5; x[NULL] = NULL; x;", 10, "is not supported by");
	EidosAssertScriptRaise("x = 1:5; x[NULL] = 10; x;", 10, "is not supported by");
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
	
	// operator = (especially in conjunction with operator .)
	EidosAssertScriptSuccess("x=_Test(9); x._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(9)));
	EidosAssertScriptRaise("x=_Test(NULL);", 2, "cannot be type NULL");
	EidosAssertScriptRaise("x=_Test(9); x._yolk = NULL;", 20, "value cannot be type");
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{9, 7, 9, 7}));
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=2; z._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{9, 2, 9, 2}));
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=2; z._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{9, 2, 9, 2}));
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[c(1,0)]._yolk=c(2, 5); z._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 2, 5, 2}));
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[c(1,0)]=c(3, 6); z._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{6, 3, 6, 3}));
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=6.5; z._yolk;", 48, "value cannot be type");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=6.5; z._yolk;", 48, "value cannot be type");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[2:3]._yolk=6.5; z._yolk;", 50, "value cannot be type");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[2:3]=6.5; z._yolk;", 50, "value cannot be type");
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
	EidosAssertScriptRaise("1:10000010;", 1, "more than 10000000 entries");
	EidosAssertScriptRaise("10000010:1;", 8, "more than 10000000 entries");
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
	EidosAssertScriptSuccess("if (F) 23;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (9) 23;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (0) 23;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (6 > 5) 23;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (6 < 5) 23;", gStaticEidosValueNULL);
	EidosAssertScriptRaise("if (6 == (6:9)) 23;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess("if ((6 == (6:9))[0]) 23;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if ((6 == (6:9))[1]) 23;", gStaticEidosValueNULL);
	EidosAssertScriptRaise("if (_Test(6)) 23;", 0, "cannot be converted");
	EidosAssertScriptRaise("if (NULL) 23;", 0, "condition for if statement has size()");
	
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
}

#pragma mark next
void _RunKeywordNextTests(void)
{
	// next
	EidosAssertScriptRaise("next;", 0, "encountered with no enclosing loop");
	EidosAssertScriptRaise("if (T) next;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("if (F) next;", gStaticEidosValueNULL);
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
	EidosAssertScriptSuccess("if (F) break;", gStaticEidosValueNULL);
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
	EidosAssertScriptSuccess("return;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("return -13;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-13)));
	EidosAssertScriptSuccess("if (T) return;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (T) return -13;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-13)));
	EidosAssertScriptSuccess("if (F) return;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (F) return -13;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (T) return; else 42;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (T) return -13; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-13)));
	EidosAssertScriptSuccess("if (F) return; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (F) return -13; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (T) 23; else return;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (T) 23; else return -13;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (F) 23; else return;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (F) 23; else return -13;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-13)));
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) return; x=x+1; } while (x<100); x;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) return x-5; x=x+1; } while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(57)));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) return; x=x+1; } x;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) return x-5; x=x+1; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(57)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) return; x=x+y; } x;", gStaticEidosValueNULL);
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
void _RunFunctionMathTests(void)
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
}

#pragma mark summary statistics
void _RunFunctionSummaryStatsTests(void)
{
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
	
	// mean()
	EidosAssertScriptRaise("mean(T);", 0, "cannot be type");
	EidosAssertScriptSuccess("mean(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3)));
	EidosAssertScriptSuccess("mean(3.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.5)));
	EidosAssertScriptRaise("mean('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("mean(c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess("mean(c(3, 7, 19, -5, 16));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(8)));
	EidosAssertScriptSuccess("mean(c(3.3, 7.2, 19.1, -5.6, 16.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(8.0)));
	EidosAssertScriptRaise("mean(c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("mean(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("mean(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("mean(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("mean(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("mean(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("mean(string(0));", 0, "cannot be type");
	
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
	
	// pmax()
	EidosAssertScriptRaise("pmax(T, logical(0));", 0, "of equal length");
	EidosAssertScriptRaise("pmax(logical(0), F);", 0, "of equal length");
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
	
	// pmin()
	EidosAssertScriptRaise("pmin(T, logical(0));", 0, "of equal length");
	EidosAssertScriptRaise("pmin(logical(0), F);", 0, "of equal length");
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
	
	// sd()
	EidosAssertScriptRaise("sd(T);", 0, "cannot be type");
	EidosAssertScriptSuccess("sd(3);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("sd(3.5);", gStaticEidosValueNULL);
	EidosAssertScriptRaise("sd('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sd(c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess("sd(c(2, 3, 2, 8, 0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3)));
	EidosAssertScriptSuccess("sd(c(9.1, 5.1, 5.1, 4.1, 7.1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.0)));
	EidosAssertScriptRaise("sd(c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("sd(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("sd(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("sd(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess("sd(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("sd(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptRaise("sd(string(0));", 0, "cannot be type");
	
	#pragma distributions
	
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
	
	// rbinom()
	EidosAssertScriptSuccess("rbinom(0, 10, 0.5);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("rbinom(1, 10, 0.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0}));
	EidosAssertScriptSuccess("rbinom(3, 10, 0.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0}));
	EidosAssertScriptSuccess("rbinom(3, 10, 1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 10, 10}));
	EidosAssertScriptSuccess("rbinom(3, 0, 0.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0}));
	EidosAssertScriptSuccess("rbinom(3, 0, 1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0}));
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
	
	// rpois()
	EidosAssertScriptSuccess("rpois(0, 1.0);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("setSeed(0); rpois(5, 1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 2, 0, 1, 1}));
	EidosAssertScriptSuccess("setSeed(1); rpois(5, 0.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 0, 0, 0, 0}));
	EidosAssertScriptSuccess("setSeed(2); rpois(5, 10000);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10205, 10177, 10094, 10227, 9875}));
	EidosAssertScriptSuccess("setSeed(2); rpois(1, 10000);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10205}));
	EidosAssertScriptSuccess("setSeed(3); rpois(5, c(1, 10, 100, 1000, 10000));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 8, 97, 994, 9911}));
	EidosAssertScriptRaise("rpois(-1, 1.0);", 0, "requires n to be");
	EidosAssertScriptRaise("rpois(0, 0.0);", 0, "requires lambda > 0.0");
	EidosAssertScriptRaise("rpois(2, c(0.0, 0.0));", 0, "requires lambda > 0.0");
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
	//EidosAssertScriptSuccess("c(object(), object());", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidos_TestElementClass)));		// should fail
	EidosAssertScriptSuccess("c(object(), _Test(7)[F]);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidos_TestElementClass)));
	EidosAssertScriptSuccess("c(_Test(7)[F], object());", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidos_TestElementClass)));
	
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
	EidosAssertScriptSuccess("rep(_Test(7), 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidos_TestElementClass)));
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
	EidosAssertScriptSuccess("repEach(_Test(7), 0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidos_TestElementClass)));
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
	EidosAssertScriptRaise("sample(5, 2, F);", 0, "insufficient elements");
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 5, 3, 1, 2}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 5, 3, 2, 4}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 6, T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 5, 3, 1, 2, 3}));
	EidosAssertScriptRaise("setSeed(0); sample(1:5, 6, F);", 12, "insufficient elements");
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, T, (1:5)^3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 5, 5, 3, 4}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, F, (1:5)^3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 5, 3, 1, 2}));
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, T, (0:4)^3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 5, 5, 3, 4}));
	EidosAssertScriptRaise("setSeed(1); sample(1:5, 5, F, (0:4)^3);", 12, "weights summing to");
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
void _RunFunctionValueInspectionManipulationTests(void)
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
	
	// cat() – can't test the actual output, but we can make sure it executes...
	EidosAssertScriptRaise("cat();", 0, "missing required argument x");
	EidosAssertScriptSuccess("cat(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(5.5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat('foo');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(_Test(7));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(NULL, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(T, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(5, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(5.5, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat('foo', '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(_Test(7), '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(c(T,T,F,T), '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(5:9, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(5.5:8.9, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(c('foo', 'bar', 'baz'), '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("cat(c(_Test(7), _Test(7), _Test(7)), '$$');", gStaticEidosValueNULL);
	
	// catn() – can't test the actual output, but we can make sure it executes...
	EidosAssertScriptSuccess("catn();", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(5.5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn('foo');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(_Test(7));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(NULL, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(T, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(5, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(5.5, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn('foo', '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(_Test(7), '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(c(T,T,F,T), '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(5:9, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(5.5:8.9, '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(c('foo', 'bar', 'baz'), '$$');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("catn(c(_Test(7), _Test(7), _Test(7)), '$$');", gStaticEidosValueNULL);
	
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
	EidosAssertScriptSuccess("print(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(5.5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print('foo');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(_Test(7));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(c(T,T,F,T));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(5:9);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(5.5:8.9);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(c('foo', 'bar', 'baz'));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("print(c(_Test(7), _Test(7), _Test(7)));", gStaticEidosValueNULL);
	
	// rev()
	EidosAssertScriptSuccess("rev(6:10);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10,9,8,7,6}));
	EidosAssertScriptSuccess("rev(-(6:10));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-10,-9,-8,-7,-6}));
	EidosAssertScriptSuccess("rev(c('foo','bar','baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"baz","bar","foo"}));
	EidosAssertScriptSuccess("rev(-1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("rev(1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1)));
	EidosAssertScriptSuccess("rev('foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("rev(6.0:10);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10,9,8,7,6}));
	EidosAssertScriptSuccess("rev(c(T,T,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, true, true}));
	
	// size()
	EidosAssertScriptSuccess("size(NULL);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("size(logical(0));", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("size(5);", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("size(c(5.5, 8.7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("size(c('foo', 'bar', 'baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("size(rep(_Test(7), 4));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(4)));
	
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
	EidosAssertScriptSuccess("str(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(c(5.5, 8.7));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(c('foo', 'bar', 'baz'));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("str(rep(_Test(7), 4));", gStaticEidosValueNULL);
	
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
	
	// asInteger()
	EidosAssertScriptSuccess("asInteger(-1:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1,0,1,2,3}));
	EidosAssertScriptSuccess("asInteger(-1.0:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1,0,1,2,3}));
	EidosAssertScriptSuccess("asInteger(c(T,F,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1,0,1,0}));
	EidosAssertScriptSuccess("asInteger(c('1','2','3'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1,2,3}));
	EidosAssertScriptRaise("asInteger('foo');", 0, "could not be represented");
	
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
	EidosAssertScriptSuccess("asLogical(c(T,F,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true,false,true,false}));
	EidosAssertScriptSuccess("asLogical(c('foo','bar',''));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true,true,false}));
	
	// asString()
	EidosAssertScriptSuccess("asString(-1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"-1"}));
	EidosAssertScriptSuccess("asString(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"3"}));
	EidosAssertScriptSuccess("asString(-1:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"-1","0","1","2","3"}));
	EidosAssertScriptSuccess("asString(-1.0:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"-1","0","1","2","3"}));
	EidosAssertScriptSuccess("asString(c(T,F,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"T","F","T","F"}));
	EidosAssertScriptSuccess("asString(c('1','2','3'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"1","2","3"}));
	
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
	
	// deleteFile() – note that the deleteFile() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess("deleteFile('/tmp/EidosTest.txt');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("deleteFile('/tmp/EidosTest.txt');", gStaticEidosValue_LogicalF);
	
	// createDirectory() – hard to test this, since it's hard to generate a path to create a folder at that is guaranteed not to exist, especially if this same test has run before on this system...
	
	// writeTempFile()
	EidosAssertScriptSuccess("file = writeTempFile('eidos_test_', '.txt', ''); identical(readFile(file), string(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("file = writeTempFile('eidos_test_', '.txt', 'foo'); identical(readFile(file), 'foo');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("file = writeTempFile('eidos_test_', '.txt', c(paste(0:4), paste(5:9))); identical(readFile(file), c('0 1 2 3 4', '5 6 7 8 9'));", gStaticEidosValue_LogicalT);
}

#pragma mark color manipulation
void _RunColorManipulationTests(void)
{
	// hsv2rgb()
	EidosAssertScriptRaise("hsv2rgb(c(0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("hsv2rgb(c(0.0, 0.0, 0.0, 0.0));", 0, "must contain exactly three");
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
	
	// rgb2hsv()
	EidosAssertScriptRaise("rgb2hsv(c(0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2hsv(c(0.0, 0.0, 0.0, 0.0));", 0, "must contain exactly three");
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
	
	// rgb2color()
	EidosAssertScriptRaise("rgb2color(c(0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2color(c(0.0, 0.0, 0.0, 0.0));", 0, "must contain exactly three");
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
	
	// color2rgb()
	EidosAssertScriptRaise("identical(color2rgb('foo'), c(0.0, 0.0, 0.0));", 10, "could not be found");
	EidosAssertScriptRaise("identical(color2rgb('#00000'), c(0.0, 0.0, 0.0));", 10, "could not be found");
	EidosAssertScriptRaise("identical(color2rgb('#0000000'), c(0.0, 0.0, 0.0));", 10, "could not be found");
	EidosAssertScriptRaise("identical(color2rgb('#0000g0'), c(0.0, 0.0, 0.0));", 10, "is malformed");
	EidosAssertScriptSuccess("identical(color2rgb('white'), c(1.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
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
void _RunFunctionMiscTests(void)
{
	// apply()
	EidosAssertScriptSuccess("x=integer(0); apply(x, 'applyValue^2;');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x=1:5; apply(x, 'applyValue^2;');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 4, 9, 16, 25}));
	EidosAssertScriptSuccess("x=1:5; apply(x, 'product(1:applyValue);');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 6, 24, 120}));
	EidosAssertScriptSuccess("x=1:3; apply(x, \"rep(''+applyValue, applyValue);\");", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"1", "2", "2", "3", "3", "3"}));
	EidosAssertScriptSuccess("x=1:5; apply(x, \"paste(rep(''+applyValue, applyValue), '');\");", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"1", "22", "333", "4444", "55555"}));
	EidosAssertScriptSuccess("x=1:10; apply(x, 'if (applyValue % 2) applyValue;');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 5, 7, 9}));
	EidosAssertScriptSuccess("x=1:5; apply(x, 'y=applyValue;'); y;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("x=1:5; apply(x, 'y=applyValue; y;');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x=2; for (i in 1:2) x=apply(x, 'applyValue^2;'); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16.0)));
	EidosAssertScriptRaise("x=2; apply(x, 'semanticError;');", 5, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; apply(x, y);", 25, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; apply(x, y[T]);", 25, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; apply(x, 'syntax Error;');", 5, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; apply(x, y);", 24, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; apply(x, y[T]);", 24, "unexpected token '@Error'");
	EidosAssertScriptSuccess("x=2; y='x;'; apply(x, y[T]);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	
	// beep() – this is commented out by default since it would confuse people if the Eidos self-test beeped...
	//EidosAssertScriptSuccess("beep();", gStaticEidosValueNULL);
	//EidosAssertScriptSuccess("beep('Submarine');", gStaticEidosValueNULL);
	
	// citation()
	EidosAssertScriptSuccess("citation();", gStaticEidosValueNULL);
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
	
	// function()
	EidosAssertScriptSuccess("function();", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("function('function');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("function('foo');", gStaticEidosValueNULL);	// does not throw at present
	EidosAssertScriptRaise("function(string(0));", 0, "must be a singleton");
	EidosAssertScriptSuccess("function(NULL);", gStaticEidosValueNULL);		// same as omitting the parameter
	EidosAssertScriptRaise("function(T);", 0, "cannot be type");
	EidosAssertScriptRaise("function(3);", 0, "cannot be type");
	EidosAssertScriptRaise("function(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("function(_Test(7));", 0, "cannot be type");
	
	// ls()
	EidosAssertScriptSuccess("ls();", gStaticEidosValueNULL);
	EidosAssertScriptRaise("ls(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls(_Test(7));", 0, "too many arguments supplied");
	
	// license()
	EidosAssertScriptSuccess("license();", gStaticEidosValueNULL);
	EidosAssertScriptRaise("license(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(_Test(7));", 0, "too many arguments supplied");
	
	// rm()
	EidosAssertScriptSuccess("rm();", gStaticEidosValueNULL);
	EidosAssertScriptRaise("x=37; rm('x'); x;", 15, "undefined identifier");
	EidosAssertScriptSuccess("x=37; rm('y'); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(37)));
	EidosAssertScriptRaise("x=37; rm(); x;", 12, "undefined identifier");
	EidosAssertScriptRaise("rm(3);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("rm(T);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(F);", 0, "cannot be type");
	EidosAssertScriptSuccess("rm(NULL);", gStaticEidosValueNULL);		// same as omitting the parameter
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
	
	// stop()
	EidosAssertScriptRaise("stop();", 0, "stop() called");
	EidosAssertScriptRaise("stop('Error');", 0, "stop() called");
	EidosAssertScriptRaise("stop(NULL);", 0, "stop() called");		// same as omitting the parameter
	EidosAssertScriptRaise("stop(T);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(3);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(_Test(7));", 0, "cannot be type");
	
	// system()
	EidosAssertScriptSuccess("system('expr 5 + 5');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("10")));
	EidosAssertScriptSuccess("system('expr', args=c('5', '+', '5'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("10")));
	EidosAssertScriptSuccess("system('expr 5 / 0', stderr=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("expr: division by zero")));	// is this reliable?
	EidosAssertScriptSuccess("system('printf foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
	EidosAssertScriptSuccess("system(\"printf 'foo bar baz' | wc -m | sed 's/ //g'\");", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("11")));
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
	
	// version()
	EidosAssertScriptSuccess("version();", gStaticEidosValueNULL);
	EidosAssertScriptRaise("version(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("version(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("version(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("version(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("version('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("version(_Test(7));", 0, "too many arguments supplied");
}

#pragma mark methods
void _RunMethodTests(void)
{
	// method()
	EidosAssertScriptSuccess("_Test(7).method();", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("_Test(7).method('method');", gStaticEidosValueNULL);
	
	// property()
	EidosAssertScriptSuccess("_Test(7).property();", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("_Test(7).property('yolk');", gStaticEidosValueNULL);
	
	// size()
	EidosAssertScriptSuccess("_Test(7).size();", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("rep(_Test(7), 5).size();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	
	// str()
	EidosAssertScriptSuccess("_Test(7).str();", gStaticEidosValueNULL);
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































































