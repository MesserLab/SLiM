//
//  eidos_test.cpp
//  Eidos
//
//  Created by Ben Haller on 4/7/15.
//  Copyright (c) 2015-2025 Benjamin C. Haller.  All rights reserved.
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
#include "eidos_sorting.h"

#include <stdlib.h>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <limits>
#include <random>
#include <ctime>
#include <algorithm>

#if 0
// includes for the timing code in RunEidosTests(), which is normally #if 0
#include "sys/time.h"	// for gettimeofday()
#include <chrono>
#if defined(__APPLE__) && defined(__MACH__)
#include <mach/mach_time.h>
#endif
#endif


// Keeping records of test success / failure
static int gEidosTestSuccessCount = 0;
static int gEidosTestFailureCount = 0;


// Instantiates and runs the script, and prints an error if the result does not match expectations
void EidosAssertScriptSuccess(const std::string &p_script_string, const EidosValue_SP &p_correct_result)
{
	{
	EidosScript script(p_script_string);
	EidosValue_SP result;
	EidosSymbolTable symbol_table(EidosSymbolTableType::kGlobalVariablesTable, gEidosConstantsSymbolTable);
	
	gEidosErrorContext.currentScript = &script;
	
	gEidosTestFailureCount++;	// assume failure; we will fix this at the end if we succeed
	
	try {
		script.Tokenize();
	}
	catch (...)
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during Tokenize(): " << Eidos_GetTrimmedRaiseMessage() << std::endl;
		
		ClearErrorContext();
		return;
	}
	
	try {
		script.ParseInterpreterBlockToAST(true);
	}
	catch (...)
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during ParseToAST(): " << Eidos_GetTrimmedRaiseMessage() << std::endl;
		
		ClearErrorContext();
		return;
	}
	
	try {
		EidosFunctionMap function_map(*EidosInterpreter::BuiltInFunctionMap());
		std::ostringstream black_hole;
		EidosInterpreter interpreter(script, symbol_table, function_map, nullptr, black_hole, black_hole);
		
		result = interpreter.EvaluateInterpreterBlock(true, true);		// print output, return the last statement value
	}
	catch (...)
	{
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during EvaluateInterpreterBlock(): " << Eidos_GetTrimmedRaiseMessage() << std::endl;
		
		ClearErrorContext();
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
	
	ClearErrorContext();
	
	if (gEidos_DictionaryNonRetainReleaseReferenceCounter > 0)
		std::cerr << "WARNING (EidosAssertScriptSuccess): gEidos_DictionaryNonRetainReleaseReferenceCounter == " << gEidos_DictionaryNonRetainReleaseReferenceCounter << " at end of test!" << std::endl;
	}
	
	gEidos_DictionaryNonRetainReleaseReferenceCounter = 0;
}

void EidosAssertScriptSuccess_L(const std::string &p_script_string, eidos_logical_t p_logical)
{
	EidosAssertScriptSuccess(p_script_string, p_logical == true ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
}

void EidosAssertScriptSuccess_VOID(const std::string &p_script_string)
{
	EidosAssertScriptSuccess(p_script_string, gStaticEidosValueVOID);
}

void EidosAssertScriptSuccess_NULL(const std::string &p_script_string)
{
	EidosAssertScriptSuccess(p_script_string, gStaticEidosValueNULL);
}

void EidosAssertScriptSuccess_LV(const std::string &p_script_string, std::initializer_list<eidos_logical_t> p_logical_vec)
{
	EidosAssertScriptSuccess(p_script_string, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical(p_logical_vec)));
}

void EidosAssertScriptSuccess_I(const std::string &p_script_string, int64_t p_integer)
{
	EidosAssertScriptSuccess(p_script_string, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(p_integer)));
}

void EidosAssertScriptSuccess_IV(const std::string &p_script_string, std::initializer_list<int64_t> p_integer_vec)
{
	EidosAssertScriptSuccess(p_script_string, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(p_integer_vec)));
}

void EidosAssertScriptSuccess_F(const std::string &p_script_string, double p_float)
{
	EidosAssertScriptSuccess(p_script_string, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(p_float)));
}

void EidosAssertScriptSuccess_FV(const std::string &p_script_string, std::initializer_list<double> p_float_vec)
{
	EidosAssertScriptSuccess(p_script_string, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(p_float_vec)));
}

void EidosAssertScriptSuccess_S(const std::string &p_script_string, const char *p_string)
{
	EidosAssertScriptSuccess(p_script_string, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(p_string)));
}

void EidosAssertScriptSuccess_SV(const std::string &p_script_string, std::initializer_list<const char *> p_string_vec)
{
	EidosAssertScriptSuccess(p_script_string, EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(p_string_vec)));
}

// Instantiates and runs the script, and prints an error if the script does not cause an exception to be raised
void EidosAssertScriptRaise(const std::string &p_script_string, const int p_bad_position, const char *p_reason_snip)
{
	{
	std::string reason_snip(p_reason_snip);
	EidosScript script(p_script_string);
	EidosSymbolTable symbol_table(EidosSymbolTableType::kGlobalVariablesTable, gEidosConstantsSymbolTable);
	EidosFunctionMap function_map(*EidosInterpreter::BuiltInFunctionMap());
	
	gEidosErrorContext.currentScript = &script;
	
	try {
		script.Tokenize();
		script.ParseInterpreterBlockToAST(true);
		
		std::ostringstream black_hole;
		EidosInterpreter interpreter(script, symbol_table, function_map, nullptr, black_hole, black_hole);
		
		EidosValue_SP result = interpreter.EvaluateInterpreterBlock(true, true);		// print output, return the last statement value
		
		gEidosTestFailureCount++;
		
		std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : no raise during EvaluateInterpreterBlock()." << std::endl;
	}
	catch (...)
	{
		// We need to call Eidos_GetTrimmedRaiseMessage() here to empty the error stringstream, even if we don't log the error
		std::string raise_message = Eidos_GetTrimmedRaiseMessage();
		
		if (raise_message.find(reason_snip) != std::string::npos)
		{
			if ((gEidosErrorContext.errorPosition.characterStartOfError == -1) ||
				(gEidosErrorContext.errorPosition.characterEndOfError == -1))
			{
				gEidosTestFailureCount++;
				
				std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise expected, but no error info set" << std::endl;
				std::cerr << p_script_string << "   raise message: " << raise_message << std::endl;
				std::cerr << "--------------------" << std::endl << std::endl;
			}
			else if (gEidosErrorContext.errorPosition.characterStartOfError != p_bad_position)
			{
				gEidosTestFailureCount++;
				
				std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise expected, but error position unexpected" << std::endl;
				std::cerr << p_script_string << "   raise message: " << raise_message << std::endl;
				Eidos_LogScriptError(std::cerr, gEidosErrorContext);
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
			
			std::cerr << p_script_string << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise message mismatch (expected \"" << reason_snip << "\")." << std::endl;
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
	
	ClearErrorContext();
	
	if (gEidos_DictionaryNonRetainReleaseReferenceCounter > 0)
		std::cerr << "WARNING (EidosAssertScriptRaise): gEidos_DictionaryNonRetainReleaseReferenceCounter == " << gEidos_DictionaryNonRetainReleaseReferenceCounter << " at end of test!" << std::endl;
	}
	
	gEidos_DictionaryNonRetainReleaseReferenceCounter = 0;
}

int RunEidosTests(void)
{
	// This function should never be called when parallel, but individual tests are allowed to go parallel internally
	THREAD_SAFETY_IN_ANY_PARALLEL("RunEidosTests(): illegal when parallel");
	
	// Reset error counts
	gEidosTestSuccessCount = 0;
	gEidosTestFailureCount = 0;
	
#if (!EIDOS_HAS_OVERFLOW_BUILTINS)
	std::cout << "WARNING: This build of Eidos does not detect integer arithmetic overflows.  Compiling Eidos with GCC version 5.0 or later, or Clang version 3.9 or later, is required for this feature.  This means that integer addition, subtraction, or multiplication that overflows the 64-bit range of Eidos (" << INT64_MIN << " to " << INT64_MAX << ") will not be detected." << std::endl;
#endif
	
	if (!Eidos_TemporaryDirectoryExists())
		std::cout << "WARNING: This system does not appear to have a writeable temporary directory.  Filesystem tests are disabled, and functions such as writeTempFile() and system() that depend upon the existence of the temporary directory will raise an exception if called (and are therefore also not tested).  Other self-tests that rely on writing temporary files, such as of readCSV() and Image, will also be disabled.  If this is surprising, contact the system administrator for details." << std::endl;
	
	// We want to run the self-test inside a new temporary directory, to prevent collisions with other self-test runs
	std::string prefix = Eidos_TemporaryDirectory() + "eidosTest_";
	std::string temp_path_template = prefix + "XXXXXX";
	char *temp_path_cstr = strdup(temp_path_template.c_str());
	
	if (Eidos_mkstemps_directory(temp_path_cstr, 0) == 0)
	{
		//std::cout << "Running Eidos self-tests in " << temp_path_cstr << " ..." << std::endl;
	}
	else
	{
		std::cout << "A folder within the temporary directory could not be created; there may be a permissions problem with the temporary directory.  The self-test could not be run." << std::endl;
		return 1;
	}
	
	std::string temp_path(temp_path_cstr);	// the final random path generated by Eidos_mkstemps_directory
	free(temp_path_cstr);
	
	// Run tests
	_RunInternalFilesystemTests();
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
	_RunFunctionStatisticsTests_a_through_p();
	_RunFunctionStatisticsTests_q_through_z();
	_RunFunctionDistributionTests();
	_RunFunctionVectorConstructionTests_a_through_r();
	_RunFunctionVectorConstructionTests_s_through_z();
	_RunFunctionValueInspectionManipulationTests_a_through_f();
	_RunFunctionValueInspectionManipulationTests_g_through_l();
	_RunFunctionValueInspectionManipulationTests_m_through_r();
	_RunFunctionValueInspectionManipulationTests_s_through_z();
	_RunStringManipulationTests();
	_RunFunctionValueTestingCoercionTests();
	_RunFunctionFilesystemTests(temp_path);
	_RunColorManipulationTests();
	_RunFunctionMiscTests_apply_sapply();
	_RunFunctionMiscTests(temp_path);
	_RunClassTests(temp_path);
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
	Eidos_SetRNGSeed(Eidos_GenerateRNGSeed());
	
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
	Eidos_SetRNGSeed(Eidos_GenerateRNGSeed());
	
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
		
		Eidos_SetRNGSeed(10);
		
		for (iter = 0; iter < 100000; ++iter)
			gsl_taus[iter] = gsl_rng_get(EIDOS_GSL_RNG);
		
		Eidos_SetRNGSeed(10);
		
		for (iter = 0; iter < 100000; ++iter)
			eidos_taus[iter] = taus_get_inline(EIDOS_GSL_RNG->state);
		
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
	
#if defined(__APPLE__) && defined(__MACH__)
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
	
#endif
	
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
	// For doing things like drawing the number of recombination or mutation events that happen across a haplosome,
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
	
#if 0
	// Speed and correctness tests of various parallel sorting algorithms
	{
		std::cout << std::endl << "SORTING TESTS:" << std::endl;
		
		gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());		// the single-threaded RNG
		typedef std::string SORT_TYPE;
		
		{
			// use the appropriate comparator for the sort type, in the code below
			// note that Eidos_ParallelSort_Comparator() and Sriram_parallel_omp_sort() require a comparator,
			// whereas std::sort() defaults to ascending (op <) by default, and Eidos_ParallelSort() doesn't take one.
			//auto comparator_scalar = [](SORT_TYPE a, SORT_TYPE b) { return a < b; };
			auto comparator_string = [](const std::string &a, const std::string &b) { return a < b; };
			//auto comparator_double = [](const double& a, const double& b) { return std::isnan(b) || (a < b); };
			const std::size_t test_size = 10000000;
			const int reps = 5;
			double time_sum;
			std::vector<SORT_TYPE> data_original;
			data_original.resize(test_size);
			
			for (std::size_t i = 0; i < test_size; ++i)
			{
				//data_original[i] = Eidos_rng_uniform_int(rng, test_size);
				data_original[i] = std::to_string(Eidos_rng_uniform_int(rng, test_size));
			}
			
			std::vector<SORT_TYPE> data_stdsort;
			
			std::cout << "time (std::sort): ";
			time_sum = 0.0;
			for (int rep = 0; rep < reps; ++rep)
			{
				data_stdsort = data_original;
				eidos_profile_t begin = Eidos_BenchmarkTime();
				
				std::sort(data_stdsort.begin(), data_stdsort.end());
				
				eidos_profile_t end = Eidos_BenchmarkTime();
				double time_spent = Eidos_ElapsedProfileTime(end - begin);
				std::cout << time_spent << " ";
				time_sum += time_spent;
			}
			std::cout << " : mean " << time_sum / reps << std::endl;
			
#ifdef _OPENMP
			std::cout << "time (PQUICK): ";
			time_sum = 0.0;
			for (int rep = 0; rep < reps; ++rep)
			{
				std::vector<SORT_TYPE> data_PQUICK = data_original;
				eidos_profile_t begin = Eidos_BenchmarkTime();
				
				Eidos_ParallelSort(data_PQUICK.data(), data_PQUICK.size(), true);
				
				eidos_profile_t end = Eidos_BenchmarkTime();
				double time_spent = Eidos_ElapsedProfileTime(end - begin);
				bool correct = (data_PQUICK == data_stdsort);
				std::cout << time_spent << " " << (!correct ? "(INCORRECT) " : "");
				time_sum += time_spent;
			}
			std::cout << " : mean " << time_sum / reps << std::endl;
			
			std::cout << "time (PQUICKCOMP): ";
			time_sum = 0.0;
			for (int rep = 0; rep < reps; ++rep)
			{
				std::vector<SORT_TYPE> data_PQUICKCOMP = data_original;
				eidos_profile_t begin = Eidos_BenchmarkTime();
				
				Eidos_ParallelSort_Comparator(data_PQUICKCOMP.data(), data_PQUICKCOMP.size(), comparator_string);
				
				eidos_profile_t end = Eidos_BenchmarkTime();
				double time_spent = Eidos_ElapsedProfileTime(end - begin);
				bool correct = (data_PQUICKCOMP == data_stdsort);
				std::cout << time_spent << " " << (!correct ? "(INCORRECT) " : "");
				time_sum += time_spent;
			}
			std::cout << " : mean " << time_sum / reps << std::endl;
			
			std::cout << "time (PSRIRAM): ";
			time_sum = 0.0;
			for (int rep = 0; rep < reps; ++rep)
			{
				std::vector<SORT_TYPE> data_PSRIRAM = data_original;
				eidos_profile_t begin = Eidos_BenchmarkTime();
				
				Sriram_parallel_omp_sort(data_PSRIRAM, comparator_string);
				
				eidos_profile_t end = Eidos_BenchmarkTime();
				double time_spent = Eidos_ElapsedProfileTime(end - begin);
				bool correct = (data_PSRIRAM == data_stdsort);
				std::cout << time_spent << " " << (!correct ? "(INCORRECT) " : "");
				time_sum += time_spent;
			}
			std::cout << " : mean " << time_sum / reps << std::endl;
#endif
		}
		
		std::cout << std::endl << std::endl;
	}
#endif
	
	// If we ran tests, the random number seed has been set; let's set it back to a good seed value
	Eidos_SetRNGSeed(Eidos_GenerateRNGSeed());
	
	// return a standard Unix result code indicating success (0) or failure (1);
	return (gEidosTestFailureCount > 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}

#pragma mark internal filesystem tests
void _RunInternalFilesystemTests(void)
{
	// test some of the core Eidos C++ filesystem functions directly for correct behavior
	// the behaviors are quite different on Windows, so that is handled entirely separately
#ifndef _WIN32
	// Eidos_ResolvedPath(): look for replacement of a leading ~, pass-through of the rest
	try {
		std::string result = Eidos_ResolvedPath("foo/bar.baz");
		if (result == "foo/bar.baz")
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_ResolvedPath(\"foo/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_ResolvedPath(\"foo/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
	
	try {
		std::string result = Eidos_ResolvedPath("~/foo/bar.baz");
		if ((result.length() > 0) && (result[0] != '~') && Eidos_string_hasSuffix(result, "/foo/bar.baz"))
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_ResolvedPath(\"~/foo/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_ResolvedPath(\"~/foo/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
	
	// Eidos_AbsolutePath(): the current working directory should be prepended
	try {
		std::string result = Eidos_AbsolutePath("foo/bar.baz");
		if (Eidos_string_hasSuffix(result, "/foo/bar.baz"))
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_AbsolutePath(\"foo/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_AbsolutePath(\"foo/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
	
	// Eidos_StripTrailingSlash(): remove a / at the end of a path, pass everything else through
	try {
		std::string result = Eidos_StripTrailingSlash("~/foo/foobar/");
		if (result == "~/foo/foobar")
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_StripTrailingSlash(\"~/foo/foobar/\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_StripTrailingSlash(\"~/foo/foobar/\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
	
	try {
		std::string result = Eidos_StripTrailingSlash("~/foo/foobar");
		if (result == "~/foo/foobar")
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_StripTrailingSlash(\"~/foo/foobar\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_StripTrailingSlash(\"~/foo/foobar\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
	
	// Eidos_LastPathComponent(): extract the last component of the path using / as the separator
	try {
		std::string result = Eidos_LastPathComponent("foo/foobar/bar.baz");
		if (result == "bar.baz")
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_LastPathComponent(\"foo/foobar/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_LastPathComponent(\"foo/foobar/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
	
	try {
		std::string result = Eidos_LastPathComponent("foo/foobar/");
		if (result == "foobar")
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_LastPathComponent(\"foo/foobar/\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_LastPathComponent(\"foo/foobar/\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
#else
	// Eidos_ResolvedPath(): on Windows this is not supported, and should raise if a leading ~ is present
	try {
		std::string result = Eidos_ResolvedPath("foo/bar.baz");
		if (result == "foo/bar.baz")
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_ResolvedPath(\"foo/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_ResolvedPath(\"foo/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
	
	try {
		std::string result = Eidos_ResolvedPath("~/foo/bar.baz");
		gEidosTestFailureCount++;
		std::cerr << "Eidos_ResolvedPath(\"~/foo/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << " (raise expected)" << std::endl;
	} catch (...) {
		gEidosTestSuccessCount++;
	}
	
	// Eidos_AbsolutePath(): the current working directory should be prepended; it might end in \ or in /
	try {
		std::string result = Eidos_AbsolutePath("foo/bar.baz");
		if ((Eidos_string_hasSuffix(result, "/foo/bar.baz")) || (Eidos_string_hasSuffix(result, "\\foo/bar.baz")))
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_AbsolutePath(\"foo/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_AbsolutePath(\"foo/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
	
	// Eidos_StripTrailingSlash(): remove a / or \\ at the end of a path, pass everything else through
	try {
		std::string result = Eidos_StripTrailingSlash("~/foo/foobar/");
		if (result == "~/foo/foobar")
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_StripTrailingSlash(\"~/foo/foobar/\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_StripTrailingSlash(\"~/foo/foobar/\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
	
	try {
		std::string result = Eidos_StripTrailingSlash("~\\foo\\foobar\\");
		if (result == "~\\foo\\foobar")
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_StripTrailingSlash(\"~\\foo\\foobar\\\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_StripTrailingSlash(\"~\\foo\\foobar\\\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
	
	try {
		std::string result = Eidos_StripTrailingSlash("~/foo/foobar");
		if (result == "~/foo/foobar")
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_StripTrailingSlash(\"~/foo/foobar\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_StripTrailingSlash(\"~/foo/foobar\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
	
	// Eidos_LastPathComponent(): extract the last component of the path using / and \ as the separators
	try {
		std::string result = Eidos_LastPathComponent("foo/foobar/bar.baz");
		if (result == "bar.baz")
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_LastPathComponent(\"foo/foobar/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_LastPathComponent(\"foo/foobar/bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
	
	try {
		std::string result = Eidos_LastPathComponent("foo\\foobar\\bar.baz");
		if (result == "bar.baz")
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_LastPathComponent(\"foo\\foobar\\bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_LastPathComponent(\"foo\\foobar\\bar.baz\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}

	try {
		std::string result = Eidos_LastPathComponent("foo/foobar/");
		if (result == "foobar")
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_LastPathComponent(\"foo/foobar/\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_LastPathComponent(\"foo/foobar/\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}

	try {
		std::string result = Eidos_LastPathComponent("foo\\foobar\\");
		if (result == "foobar")
			gEidosTestSuccessCount++;
		else
		{
			gEidosTestFailureCount++;
			std::cerr << "Eidos_LastPathComponent(\"foo\\foobar\\\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : incorrect result " << result << std::endl;
		}
	} catch (...) {
		gEidosTestFailureCount++;
		std::cerr << "Eidos_LastPathComponent(\"foo\\foobar\\\")" << " : " << EIDOS_OUTPUT_FAILURE_TAG << " : raise during execution" << std::endl;
	}
#endif
}

#pragma mark literals & identifiers
void _RunLiteralsIdentifiersAndTokenizationTests(void)
{
	// test literals, built-in identifiers, and tokenization
	// NOLINTBEGIN(*-raw-string-literal) : these strings are fine
	EidosAssertScriptSuccess_VOID(";");
	EidosAssertScriptSuccess_I("3;", 3);
	EidosAssertScriptSuccess_I("3e2;", 300);
	EidosAssertScriptSuccess_F("3.1;", 3.1);
	EidosAssertScriptSuccess_F("3.1e2;", 3.1e2);
	EidosAssertScriptSuccess_F("3.1e-2;", 3.1e-2);
	EidosAssertScriptSuccess_F("3.1e+2;", 3.1e+2);
	EidosAssertScriptSuccess_S("'foo';", "foo");
	EidosAssertScriptSuccess_S("'foo\\tbar';", "foo\tbar");
	EidosAssertScriptSuccess_S("'\\'foo\\'\\t\\\"bar\"';", "'foo'\t\"bar\"");
	EidosAssertScriptSuccess_S("\"foo\";", "foo");
	EidosAssertScriptSuccess_S("\"foo\\tbar\";", "foo\tbar");
	EidosAssertScriptSuccess_S("\"\\'foo'\\t\\\"bar\\\"\";", "'foo'\t\"bar\"");
	EidosAssertScriptSuccess_S("<<\n'foo'\n\"bar\"\n>>;", "'foo'\n\"bar\"");
	EidosAssertScriptSuccess_S("<<---\n'foo'\n\"bar\"\n>>---;", "'foo'\n\"bar\"");
	EidosAssertScriptSuccess_S("<<<<\n'foo'\n\"bar\"\n>><<;", "'foo'\n\"bar\"");
	EidosAssertScriptSuccess_S("<<<<\n'foo'\n\"bar>><\"\n>><<;", "'foo'\n\"bar>><\"");
	EidosAssertScriptSuccess_L("T;", true);
	EidosAssertScriptSuccess_L("F;", false);
	EidosAssertScriptSuccess_NULL("NULL;");
	EidosAssertScriptSuccess("INF;", gStaticEidosValue_FloatINF);
	EidosAssertScriptSuccess_F("-INF;", (-std::numeric_limits<double>::infinity()));
	EidosAssertScriptSuccess("NAN;", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess_L("E - exp(1) < 0.0000001;", true);
	EidosAssertScriptSuccess_L("PI - asin(1)*2 < 0.0000001;", true);
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
	// NOLINTEND(*-raw-string-literal)
	
	// tests related to the R-style assignment operator, <-, which is explicitly illegal in Eidos to prevent mistakes ("a <- b;" meaning "a < -b;")
	EidosAssertScriptSuccess_L("x = -9; x < -8;", true);
	EidosAssertScriptRaise("x = -9; x <- 8;", 10, "<- is not legal");
	EidosAssertScriptRaise("x = -9; x<-8;", 9, "<- is not legal");
	
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
	EidosAssertScriptRaise("PI = PI % 2;", 3, "is a constant");
	EidosAssertScriptRaise("PI = PI % 2.0;", 3, "is a constant");
	EidosAssertScriptRaise("PI = PI ^ 2;", 3, "is a constant");
	EidosAssertScriptRaise("PI = PI ^ 2.0;", 3, "is a constant");
	EidosAssertScriptRaise("PI = c(PI, 2);", 3, "is a constant");
	EidosAssertScriptRaise("PI = c(PI, 2.0);", 3, "is a constant");
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
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = Q % 2;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = Q % 2.0;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = Q ^ 2;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = Q ^ 2.0;", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = c(Q, 2);", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); Q = c(Q, 2.0);", 26, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); for (Q in c(3, 4)) 5;", 29, "is a constant");
	EidosAssertScriptRaise("defineConstant('Q', 7); for (Q in c(3.0, 4.0)) 5;", 29, "is a constant");
}

#pragma mark symbol table
void _RunSymbolsAndVariablesTests(void)
{
	// test symbol table and variable dynamics
	EidosAssertScriptSuccess_I("x = 3; x;", 3);
	EidosAssertScriptSuccess_F("x = 3.1; x;", 3.1);
	EidosAssertScriptSuccess_S("x = 'foo'; x;", "foo");
	EidosAssertScriptSuccess_L("x = T; x;", true);
	EidosAssertScriptSuccess_NULL("x = NULL; x;");
	EidosAssertScriptSuccess_I("x = 'first'; x = 3; x;", 3);
	EidosAssertScriptSuccess_F("x = 'first'; x = 3.1; x;", 3.1);
	EidosAssertScriptSuccess_S("x = 'first'; x = 'foo'; x;", "foo");
	EidosAssertScriptSuccess_L("x = 'first'; x = T; x;", true);
	EidosAssertScriptSuccess_NULL("x = 'first'; x = NULL; x;");
	EidosAssertScriptSuccess_IV("x = 1:5; y = x + 1; x;", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess_IV("x = 1:5; y = x + 1; y;", {2, 3, 4, 5, 6});
	EidosAssertScriptSuccess_IV("x = 1:5; y = x + 1; x = x + 1; x;", {2, 3, 4, 5, 6});
	EidosAssertScriptSuccess_IV("x = 1:5; y = x + 1; x = x + 1; y;", {2, 3, 4, 5, 6});
	EidosAssertScriptSuccess_IV("x = 1:5; y = x; x = x + 1; x;", {2, 3, 4, 5, 6});
	EidosAssertScriptSuccess_IV("x = 1:5; y = x; x = x + 1; y;", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess_IV("x = 1:5; y = x; x = x + x; x;", {2, 4, 6, 8, 10});
	EidosAssertScriptSuccess_IV("x = 1:5; y = x; x = x + x; y;", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess_IV("x = 1:5; y = x; x[1] = 0; x;", {1, 0, 3, 4, 5});
	EidosAssertScriptSuccess_IV("x = 1:5; y = x; x[1] = 0; y;", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess_IV("x = 1:5; y = x; y[1] = 0; x;", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess_IV("x = 1:5; y = x; y[1] = 0; y;", {1, 0, 3, 4, 5});
	EidosAssertScriptSuccess_IV("for (i in 1:3) { x = 1:5; x[1] = x[1] + 1; } x;", {1, 3, 3, 4, 5});
	
	// some tests for Unicode in symbol names; both accented characters and emoji should be legal (and all other Unicode above 7-bit ASCII)
	// note that "\u00E9" is &eacute; and "\u1F603" is a grinning face emoji
	EidosAssertScriptSuccess_I("\u00E9 = 3; \u00E9;", 3);
	EidosAssertScriptSuccess_I("\u00E9e = 3; \u00E9e;", 3);
	EidosAssertScriptSuccess_I("e\u00E9 = 3; e\u00E9;", 3);
	EidosAssertScriptSuccess_I("e\u00E9\u00E9e = 3; e\u00E9\u00E9e;", 3);
	EidosAssertScriptSuccess_I("\u1F603 = 3; \u1F603;", 3);
	EidosAssertScriptSuccess_I("\u1F603e = 3; \u1F603e;", 3);
	EidosAssertScriptSuccess_I("e\u1F603 = 3; e\u1F603;", 3);
	EidosAssertScriptSuccess_I("e\u1F603\u1F603e = 3; e\u1F603\u1F603e;", 3);
	
	// test defineGlobal() and defineConstant() for correctly checking identifier syntax
	EidosAssertScriptSuccess_I("defineConstant('Q', 7); Q;", 7);
	EidosAssertScriptSuccess_I("defineConstant('_Qixx_14850_', 7); _Qixx_14850_;", 7);
	EidosAssertScriptRaise("defineConstant('_Qixx 14850_', 7);", 0, "valid Eidos identifier");
	EidosAssertScriptRaise("defineConstant('_Qixx.14850_', 7);", 0, "valid Eidos identifier");
	
	EidosAssertScriptSuccess_I("defineConstant('\u00E9', 3); \u00E9;", 3);
	EidosAssertScriptSuccess_I("defineConstant('\u00E9e', 3); \u00E9e;", 3);
	EidosAssertScriptSuccess_I("defineConstant('e\u00E9', 3); e\u00E9;", 3);
	EidosAssertScriptSuccess_I("defineConstant('e\u00E9\u00E9e', 3); e\u00E9\u00E9e;", 3);
	EidosAssertScriptSuccess_I("defineConstant('\u1F603', 3); \u1F603;", 3);
	EidosAssertScriptSuccess_I("defineConstant('\u1F603e', 3); \u1F603e;", 3);
	EidosAssertScriptSuccess_I("defineConstant('e\u1F603', 3); e\u1F603;", 3);
	EidosAssertScriptSuccess_I("defineConstant('e\u1F603\u1F603e', 3); e\u1F603\u1F603e;", 3);
	
	EidosAssertScriptSuccess_I("defineGlobal('Q', 7); Q;", 7);
	EidosAssertScriptSuccess_I("defineGlobal('_Qixx_14850_', 7); _Qixx_14850_;", 7);
	EidosAssertScriptRaise("defineGlobal('_Qixx 14850_', 7);", 0, "valid Eidos identifier");
	EidosAssertScriptRaise("defineGlobal('_Qixx.14850_', 7);", 0, "valid Eidos identifier");
	
	EidosAssertScriptSuccess_I("defineGlobal('\u00E9', 3); \u00E9;", 3);
	EidosAssertScriptSuccess_I("defineGlobal('\u00E9e', 3); \u00E9e;", 3);
	EidosAssertScriptSuccess_I("defineGlobal('e\u00E9', 3); e\u00E9;", 3);
	EidosAssertScriptSuccess_I("defineGlobal('e\u00E9\u00E9e', 3); e\u00E9\u00E9e;", 3);
	EidosAssertScriptSuccess_I("defineGlobal('\u1F603', 3); \u1F603;", 3);
	EidosAssertScriptSuccess_I("defineGlobal('\u1F603e', 3); \u1F603e;", 3);
	EidosAssertScriptSuccess_I("defineGlobal('e\u1F603', 3); e\u1F603;", 3);
	EidosAssertScriptSuccess_I("defineGlobal('e\u1F603\u1F603e', 3); e\u1F603\u1F603e;", 3);
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
	EidosAssertScriptSuccess_I("abs(-10);", 10);
	EidosAssertScriptRaise("abs();", 0, "missing required argument 'x'");
	EidosAssertScriptRaise("abs(-10, -10);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("abs(x=-10, -10);", 0, "too many arguments supplied");
	EidosAssertScriptSuccess_I("abs(x=-10);", 10);
	EidosAssertScriptRaise("abs(y=-10);", 0, "skipped over required argument");
	EidosAssertScriptRaise("abs(x=-10, x=-10);", 0, "supplied more than once");
	EidosAssertScriptRaise("abs(x=-10, y=-10);", 0, "unrecognized named argument 'y'");
	EidosAssertScriptRaise("abs(y=-10, x=-10);", 0, "skipped over required argument");
	
	EidosAssertScriptSuccess_I("integerDiv(6, 3);", 2);
	EidosAssertScriptRaise("integerDiv(6, 3, 3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("integerDiv(x=6, y=3, 3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("integerDiv(6);", 0, "missing required argument 'y'");
	EidosAssertScriptSuccess_I("integerDiv(x=6, y=3);", 2);
	EidosAssertScriptRaise("integerDiv(y=6, 3);", 0, "skipped over required argument");
	EidosAssertScriptRaise("integerDiv(y=6, x=3);", 0, "skipped over required argument");
	EidosAssertScriptRaise("integerDiv(x=6, 3);", 0, "unnamed argument may not follow after named arguments");
	EidosAssertScriptSuccess_I("integerDiv(6, y=3);", 2);
	
	EidosAssertScriptSuccess_IV("seq(1, 3, 1);", {1, 2, 3});
	EidosAssertScriptSuccess_IV("seq(1, 3, NULL);", {1, 2, 3});
	EidosAssertScriptSuccess_IV("seq(1, 3, by=1);", {1, 2, 3});
	EidosAssertScriptSuccess_IV("seq(1, 3, by=NULL);", {1, 2, 3});
	EidosAssertScriptRaise("seq(10, to=20, from=10);", 0, "supplied twice in the argument list");
	EidosAssertScriptRaise("seq(10, 20, foo=20);", 0, "no parameter with that name");
	EidosAssertScriptRaise("rainbow(10, v=0.5, s=0.5);", 0, "supplied out of order");
	EidosAssertScriptRaise("seq(1, 3, by=1, length=1, by=1);", 0, "supplied more than once");
	EidosAssertScriptRaise("seq(1, 3, length=1, by=1);", 0, "supplied out of order");
	EidosAssertScriptSuccess_IV("seq(1, 3);", {1, 2, 3});
	EidosAssertScriptRaise("seq(by=1, 1, 3);", 0, "named argument 'by' skipped over required argument");
	EidosAssertScriptRaise("seq(by=NULL, 1, 3);", 0, "named argument 'by' skipped over required argument");
	
	EidosAssertScriptSuccess_NULL("c();");
	EidosAssertScriptSuccess_NULL("c(NULL);");
	EidosAssertScriptSuccess_I("c(2);", 2);
	EidosAssertScriptSuccess_IV("c(1, 2, 3);", {1, 2, 3});
	EidosAssertScriptRaise("c(x=2);", 0, "unrecognized named argument 'x'");
	EidosAssertScriptRaise("c(x=1, 2, 3);", 0, "unrecognized named argument 'x'");
	EidosAssertScriptRaise("c(1, x=2, 3);", 0, "unrecognized named argument 'x'");
	EidosAssertScriptRaise("c(1, 2, x=3);", 0, "unrecognized named argument 'x'");
	
	EidosAssertScriptSuccess_I("doCall('abs', -10);", 10);
	EidosAssertScriptSuccess_I("doCall(functionName='abs', -10);", 10);
	EidosAssertScriptRaise("doCall(x='abs', -10);", 0, "skipped over required argument");
	EidosAssertScriptRaise("doCall('abs', x=-10);", 0, "unrecognized named argument 'x'");
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
	EidosAssertScriptSuccess_LV("rep(1:3, 2) == 2;", {false, true, false, false, true, false});
	EidosAssertScriptSuccess_LV("rep(1:3, 2) != 2;", {true, false, true, true, false, true});
	EidosAssertScriptSuccess_LV("rep(1:3, 2) < 2;", {true, false, false, true, false, false});
	EidosAssertScriptSuccess_LV("rep(1:3, 2) <= 2;", {true, true, false, true, true, false});
	EidosAssertScriptSuccess_LV("rep(1:3, 2) > 2;", {false, false, true, false, false, true});
	EidosAssertScriptSuccess_LV("rep(1:3, 2) >= 2;", {false, true, true, false, true, true});
	
	EidosAssertScriptSuccess_LV("2 == rep(1:3, 2);", {false, true, false, false, true, false});
	EidosAssertScriptSuccess_LV("2 != rep(1:3, 2);", {true, false, true, true, false, true});
	EidosAssertScriptSuccess_LV("2 > rep(1:3, 2);", {true, false, false, true, false, false});
	EidosAssertScriptSuccess_LV("2 >= rep(1:3, 2);", {true, true, false, true, true, false});
	EidosAssertScriptSuccess_LV("2 < rep(1:3, 2);", {false, false, true, false, false, true});
	EidosAssertScriptSuccess_LV("2 <= rep(1:3, 2);", {false, true, true, false, true, true});
	
	EidosAssertScriptSuccess_I("_Test(2)._yolk;", 2);
	EidosAssertScriptSuccess_IV("c(_Test(2),_Test(3))._yolk;", {2, 3});
	EidosAssertScriptSuccess_IV("_Test(2)[F]._yolk;", {});
	
	EidosAssertScriptSuccess_I("_Test(2)._cubicYolk();", 8);
	EidosAssertScriptSuccess_IV("c(_Test(2),_Test(3))._cubicYolk();", {8, 27});
	EidosAssertScriptSuccess_IV("_Test(2)[F]._cubicYolk();", {});
	
	EidosAssertScriptSuccess_I("_Test(2)._increment._yolk;", 3);
	EidosAssertScriptSuccess_IV("c(_Test(2),_Test(3))._increment._yolk;", {3, 4});
	EidosAssertScriptSuccess_IV("_Test(2)[F]._increment._yolk;", {});
	
	EidosAssertScriptSuccess_I("_Test(2)._increment._cubicYolk();", 27);
	EidosAssertScriptSuccess_IV("c(_Test(2),_Test(3))._increment._cubicYolk();", {27, 64});
	EidosAssertScriptSuccess_IV("_Test(2)[F]._increment._cubicYolk();", {});
	
	EidosAssertScriptSuccess_I("_Test(2)._squareTest()._yolk;", 4);
	EidosAssertScriptSuccess_IV("c(_Test(2),_Test(3))._squareTest()._yolk;", {4, 9});
	EidosAssertScriptSuccess_IV("_Test(2)[F]._squareTest()._yolk;", {});
	
	EidosAssertScriptSuccess_I("_Test(2)._squareTest()._cubicYolk();", 64);
	EidosAssertScriptSuccess_IV("c(_Test(2),_Test(3))._squareTest()._cubicYolk();", {64, 729});
	EidosAssertScriptSuccess_IV("_Test(2)[F]._squareTest()._cubicYolk();", {});
}

	






























































