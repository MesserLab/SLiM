//
//  eidos_test.h
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

/*
 
 This file contains code to test Eidos.
 
 */

#ifndef __Eidos__eidos_test__
#define __Eidos__eidos_test__

#include <string>

#include "eidos_value.h"


int RunEidosTests(void);


// Can turn on escape sequences to color test output; at present we turn these on for the command-line
// tools and off for the GUI tools, since Terminal supports these codes but Xcode does not.
#ifdef EIDOS_GUI

#define EIDOS_OUTPUT_FAILURE_TAG	"FAILURE"
#define EIDOS_OUTPUT_SUCCESS_TAG	"SUCCESS"

#else

#define EIDOS_OUTPUT_FAILURE_TAG	"\e[31mFAILURE\e[0m"
#define EIDOS_OUTPUT_SUCCESS_TAG	"\e[32mSUCCESS\e[0m"

#endif


// Conceptually, all the eidos_test_X.cpp stuff is a single source file, and all the details below are private.
// It is split into multiple files to improve compile performance; the single source file took more than a minute to compile


// Helper functions for testing
extern void EidosAssertScriptSuccess(const std::string &p_script_string, EidosValue_SP p_correct_result);
extern void EidosAssertScriptRaise(const std::string &p_script_string, const int p_bad_position, const std::string &p_reason_snip);


// Test subfunction prototypes
extern void _RunLiteralsIdentifiersAndTokenizationTests(void);
extern void _RunSymbolsAndVariablesTests(void);
extern void _RunParsingTests(void);
extern void _RunFunctionDispatchTests(void);
extern void _RunRuntimeErrorTests(void);
extern void _RunVectorsAndSingletonsTests(void);
extern void _RunOperatorPlusTests1(void);
extern void _RunOperatorPlusTests2(void);
extern void _RunOperatorMinusTests(void);
extern void _RunOperatorMultTests(void);
extern void _RunOperatorDivTests(void);
extern void _RunOperatorModTests(void);
extern void _RunOperatorSubsetTests(void);
extern void _RunOperatorAssignTests(void);
extern void _RunOperatorGtTests(void);
extern void _RunOperatorLtTests(void);
extern void _RunOperatorGtEqTests(void);
extern void _RunOperatorLtEqTests(void);
extern void _RunOperatorEqTests(void);
extern void _RunOperatorNotEqTests(void);
extern void _RunOperatorRangeTests(void);
extern void _RunOperatorExpTests(void);
extern void _RunOperatorLogicalAndTests(void);
extern void _RunOperatorLogicalOrTests(void);
extern void _RunOperatorLogicalNotTests(void);
extern void _RunOperatorTernaryConditionalTests(void);
extern void _RunKeywordIfTests(void);
extern void _RunKeywordDoTests(void);
extern void _RunKeywordWhileTests(void);
extern void _RunKeywordForInTests(void);
extern void _RunKeywordNextTests(void);
extern void _RunKeywordBreakTests(void);
extern void _RunKeywordReturnTests(void);
extern void _RunFunctionMathTests_a_through_f(void);
extern void _RunFunctionMathTests_g_through_r(void);
extern void _RunFunctionMathTests_setUnionIntersection(void);
extern void _RunFunctionMathTests_setDifferenceSymmetricDifference(void);
extern void _RunFunctionMathTests_s_through_z(void);
extern void _RunFunctionMatrixArrayTests(void);
extern void _RunFunctionStatisticsTests(void);
extern void _RunFunctionDistributionTests(void);
extern void _RunFunctionVectorConstructionTests(void);
extern void _RunFunctionValueInspectionManipulationTests_a_through_f(void);
extern void _RunFunctionValueInspectionManipulationTests_g_through_l(void);
extern void _RunFunctionValueInspectionManipulationTests_m_through_r(void);
extern void _RunFunctionValueInspectionManipulationTests_s_through_z(void);
extern void _RunFunctionValueTestingCoercionTests(void);
extern void _RunFunctionFilesystemTests(std::string temp_path);
extern void _RunColorManipulationTests(void);
extern void _RunFunctionMiscTests_apply_sapply(void);
extern void _RunFunctionMiscTests(std::string temp_path);
extern void _RunMethodTests(void);
extern void _RunCodeExampleTests(void);
extern void _RunUserDefinedFunctionTests(void);
extern void _RunVoidEidosValueTests(void);


#endif /* defined(__Eidos__eidos_test__) */































































