//
//  eidos_test_functions_math.cpp
//  Eidos
//
//  Created by Ben Haller on 7/11/20.
//  Copyright (c) 2020 Philipp Messer.  All rights reserved.
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

#include "math.h"

#include <limits>


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
	EidosAssertScriptSuccess("abs(c(-2.0, 7.0, -18.0, NAN, 12.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2, 7, 18, std::numeric_limits<double>::quiet_NaN(), 12}));
	
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
	EidosAssertScriptSuccess("acos(c(1, NAN, 1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, std::numeric_limits<double>::quiet_NaN(), 0}));
	
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
	EidosAssertScriptSuccess("asin(c(0, NAN, 0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, std::numeric_limits<double>::quiet_NaN(), 0}));
	
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
	EidosAssertScriptSuccess("atan(c(0, NAN, 0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, std::numeric_limits<double>::quiet_NaN(), 0}));
	
	EidosAssertScriptSuccess("identical(atan(matrix(0.5)), matrix(atan(0.5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(atan(matrix(c(0.1, 0.2, 0.3))), matrix(atan(c(0.1, 0.2, 0.3))));", gStaticEidosValue_LogicalT);
	
	// atan2()
	EidosAssertScriptSuccess("abs(atan2(0, 1) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan2(0, -1) - PI) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(atan2(c(0, 0, -1), c(1, -1, 0)) - c(0, PI, -PI/2))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan2(0.0, 1.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(atan2(0.0, -1.0) - PI) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(atan2(c(0.0, 0.0, -1.0), c(1.0, -1.0, 0.0)) - c(0, PI, -PI/2))) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("atan2(T);", 0, "cannot be type");
	EidosAssertScriptRaise("atan2('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("atan2(_Test(7));", 0, "missing required argument");
	EidosAssertScriptRaise("atan2(NULL);", 0, "cannot be type");
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
	EidosAssertScriptSuccess("atan2(c(0, NAN, 0, 0), c(1, 1, NAN, 1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), 0}));
	
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
	EidosAssertScriptSuccess("ceil(c(-2.1, 7.1, -18.8, NAN, 12.8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-2.0, 8, -18, std::numeric_limits<double>::quiet_NaN(), 13}));
	
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
	EidosAssertScriptSuccess("cos(c(0, NAN, 0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, std::numeric_limits<double>::quiet_NaN(), 1}));
	
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
	EidosAssertScriptSuccess("exp(c(0, NAN, 0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, std::numeric_limits<double>::quiet_NaN(), 1}));
	
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
	EidosAssertScriptSuccess("floor(c(-2.1, 7.1, -18.8, NAN, 12.8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-3.0, 7, -19, std::numeric_limits<double>::quiet_NaN(), 12}));
	
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
	EidosAssertScriptSuccess("log(c(1, NAN, 1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, std::numeric_limits<double>::quiet_NaN(), 0}));
	
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
	EidosAssertScriptSuccess("log10(c(1, NAN, 1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, std::numeric_limits<double>::quiet_NaN(), 0}));
	
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
	EidosAssertScriptSuccess("log2(c(1, NAN, 1));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, std::numeric_limits<double>::quiet_NaN(), 0}));
	
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
	EidosAssertScriptSuccess("round(c(-2.1, 7.1, -18.8, NAN, 12.8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-2.0, 7, -19, std::numeric_limits<double>::quiet_NaN(), 13}));
	
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
	
	EidosAssertScriptSuccess("setUnion(NAN, NAN);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setUnion(c(3.2, NAN, NAN, 3.2), NAN);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setUnion(c(3.2, NAN, NAN, 3.2), 3.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setUnion(NAN, c(3.2, NAN, NAN, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setUnion(3.2, c(3.2, NAN, NAN, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setUnion(c(3.2, 3.2, NAN, NAN, 3.2, 3.2), c(3.2, 3.2, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setUnion(c(3.2, 3.2, NAN, NAN, 3.2, 3.2), c(3.2, NAN, 3.2, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setUnion(c(3.2, 6.0, NAN, NAN, 7.9, 3.2), c(5.5, 6.0, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 6.0, std::numeric_limits<double>::quiet_NaN(), 7.9, 5.5}));
	EidosAssertScriptSuccess("setUnion(c(3.2, 6.0, NAN, NAN, 7.9, 3.2), c(5.5, NAN, 6.0, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 6.0, std::numeric_limits<double>::quiet_NaN(), 7.9, 5.5}));
	
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
	
	EidosAssertScriptSuccess("setIntersection(NAN, NAN);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setIntersection(c(3.2, NAN, NAN, 3.2), NAN);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setIntersection(c(3.2, NAN, NAN, 3.2), 3.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2}));
	EidosAssertScriptSuccess("setIntersection(NAN, c(3.2, NAN, NAN, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setIntersection(3.2, c(3.2, NAN, NAN, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2}));
	EidosAssertScriptSuccess("setIntersection(c(3.2, 3.2, 3.2, NAN, NAN, 3.2), c(3.2, 3.2, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(3.2)));
	EidosAssertScriptSuccess("setIntersection(c(3.2, 3.2, 3.2, NAN, NAN, 3.2), c(3.2, NAN, 3.2, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setIntersection(c(3.2, 6.0, 7.9, NAN, NAN, 3.2), c(5.5, 6.0, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 6.0}));
	EidosAssertScriptSuccess("setIntersection(c(3.2, 6.0, 7.9, NAN, NAN, 3.2), c(5.5, NAN, 6.0, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2, 6.0, std::numeric_limits<double>::quiet_NaN()}));
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
	
	EidosAssertScriptSuccess("setDifference(NAN, NAN);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(3.2, NAN, NAN, 3.2), NAN);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2}));
	EidosAssertScriptSuccess("setDifference(c(3.2, NAN, NAN, 3.2), 3.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setDifference(NAN, c(3.2, NAN, NAN, 3.2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setDifference(3.2, c(3.2, NAN, NAN, 3.2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(3.2, 3.2, NAN, NAN, 3.2, 3.2), c(3.2, 3.2, 3.2, 3.2));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("setDifference(c(3.2, 3.2, NAN, NAN, 3.2, 3.2), c(3.2, NAN, 3.2, 3.2, 3.2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setDifference(c(3.2, 6.0, NAN, NAN, 7.9, 3.2, 7.9), c(5.5, 6.0, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN(), 7.9}));
	EidosAssertScriptSuccess("setDifference(c(3.2, 6.0, NAN, NAN, 7.9, 3.2, 7.9), c(5.5, NAN, 6.0, 3.2, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(7.9)));
	
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
	
	EidosAssertScriptSuccess("setSymmetricDifference(NAN, NAN);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(c(3.2, NAN, NAN, 3.2), NAN);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2}));
	EidosAssertScriptSuccess("setSymmetricDifference(c(3.2, NAN, NAN, 3.2), 3.2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setSymmetricDifference(NAN, c(3.2, NAN, NAN, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.2}));
	EidosAssertScriptSuccess("setSymmetricDifference(3.2, c(3.2, NAN, NAN, 3.2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("setSymmetricDifference(c(3.2, 3.2, NAN, NAN, 3.2, 3.2), c(3.2, 3.2, 3.2, 3.2));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("setSymmetricDifference(c(3.2, 3.2, NAN, NAN, 3.2, 3.2), c(3.2, NAN, 3.2, 3.2, 3.2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setSymmetricDifference(c(3.2, 3.2, 3.2, 3.2), c(3.2, NAN, NAN, 3.2, 3.2, 3.2));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("setSymmetricDifference(c(7.3, 10.5, NAN, NAN, 7.3, 10.5, 8.9), c(7.3, 9.7, 7.3, 9.7, 7.3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.5, std::numeric_limits<double>::quiet_NaN(), 8.9, 9.7}));
	EidosAssertScriptSuccess("setSymmetricDifference(c(7.3, 10.5, NAN, NAN, 7.3, 10.5, 8.9), c(7.3, NAN, 9.7, 7.3, 9.7, 7.3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.5, 8.9, 9.7}));
	EidosAssertScriptSuccess("setSymmetricDifference(c(7.3, 10.5, 7.3, 10.5, 8.9), c(7.3, NAN, NAN, 9.7, 7.3, 9.7, 7.3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.5, 8.9, std::numeric_limits<double>::quiet_NaN(), 9.7}));
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
	EidosAssertScriptSuccess("sin(c(0, NAN, 0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, std::numeric_limits<double>::quiet_NaN(), 0}));
	
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
	EidosAssertScriptSuccess("sqrt(c(64, NAN, 9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{8, std::numeric_limits<double>::quiet_NaN(), 3}));
	
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
	EidosAssertScriptSuccess("tan(c(0, NAN, 0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, std::numeric_limits<double>::quiet_NaN(), 0}));
	
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
	EidosAssertScriptSuccess("trunc(c(-2.1, 7.1, -18.8, NAN, 12.8));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-2.0, 7, -18, std::numeric_limits<double>::quiet_NaN(), 12}));
	
	EidosAssertScriptSuccess("identical(trunc(matrix(0.3)), matrix(trunc(0.3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(trunc(matrix(0.6)), matrix(trunc(0.6)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(trunc(matrix(-0.3)), matrix(trunc(-0.3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(trunc(matrix(-0.6)), matrix(trunc(-0.6)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(trunc(matrix(c(0.1, 5.7, -0.3))), matrix(trunc(c(0.1, 5.7, -0.3))));", gStaticEidosValue_LogicalT);
}








































