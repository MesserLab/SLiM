//
//  eidos_test_functions_vector.cpp
//  Eidos
//
//  Created by Ben Haller on 7/11/20.
//  Copyright (c) 2020-2021 Philipp Messer.  All rights reserved.
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

#include "eidos_class_TestElement.h"

#include <limits>


#pragma mark vector construction
void _RunFunctionVectorConstructionTests_a_through_r(void)
{
	// c()
	EidosAssertScriptSuccess("c();", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("c(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess_L("c(T);", true);
	EidosAssertScriptSuccess_I("c(3);", 3);
	EidosAssertScriptSuccess_F("c(3.1);", 3.1);
	EidosAssertScriptSuccess_S("c('foo');", "foo");
	EidosAssertScriptSuccess_I("c(_Test(7))._yolk;", 7);
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
	EidosAssertScriptSuccess_L("c(T, NULL);", true);
	EidosAssertScriptSuccess_I("c(3, NULL);", 3);
	EidosAssertScriptSuccess_F("c(3.1, NULL);", 3.1);
	EidosAssertScriptSuccess_S("c('foo', NULL);", "foo");
	EidosAssertScriptSuccess_I("c(_Test(7), NULL)._yolk;", 7);
	EidosAssertScriptSuccess_L("c(NULL, T);", true);
	EidosAssertScriptSuccess_I("c(NULL, 3);", 3);
	EidosAssertScriptSuccess_F("c(NULL, 3.1);", 3.1);
	EidosAssertScriptSuccess_S("c(NULL, 'foo');", "foo");
	EidosAssertScriptSuccess_I("c(NULL, _Test(7))._yolk;", 7);
	EidosAssertScriptRaise("c(T, _Test(7));", 0, "cannot be mixed");
	EidosAssertScriptRaise("c(3, _Test(7));", 0, "cannot be mixed");
	EidosAssertScriptRaise("c(3.1, _Test(7));", 0, "cannot be mixed");
	EidosAssertScriptRaise("c('foo', _Test(7));", 0, "cannot be mixed");
	EidosAssertScriptSuccess_I("c(object(), _Test(7))._yolk;", 7);
	EidosAssertScriptSuccess_I("c(_Test(7), object())._yolk;", 7);
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
	EidosAssertScriptSuccess_L("logical(1);", false);
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
}

void _RunFunctionVectorConstructionTests_s_through_z(void)
{
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
	EidosAssertScriptSuccess_I("sample(5, 1, T);", 5);
	EidosAssertScriptSuccess_I("sample(5, 1, F);", 5);
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
	EidosAssertScriptSuccess("setSeed(0); sample(1:5, 5, F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 5, 2, 4, 1}));
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
	EidosAssertScriptSuccess_I("seq(2, 3, length=1);", 2);
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
	EidosAssertScriptSuccess_F("seq(2., 3, length=1);", 2);
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
	EidosAssertScriptSuccess_I("seqLen(1);", 0);
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
	EidosAssertScriptSuccess_L("all(logical(0));", true);
	EidosAssertScriptSuccess_L("all(T);", true);
	EidosAssertScriptSuccess_L("all(F);", false);
	EidosAssertScriptSuccess_L("all(c(T,T,T,T,T,T,T,T,T,T));", true);
	EidosAssertScriptSuccess_L("all(c(T,T,T,T,T,T,T,F,T,T));", false);
	EidosAssertScriptSuccess_L("all(c(F,F,F,F,F,F,F,F,F,F));", false);
	
	EidosAssertScriptRaise("all(T, NULL);", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("all(T, 0);", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("all(T, 0.5);", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("all(T, 'foo');", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("all(T, _Test(7));", 0, "all arguments be of type logical");
	EidosAssertScriptSuccess_L("all(T, logical(0));", true);
	EidosAssertScriptSuccess_L("all(T, T);", true);
	EidosAssertScriptSuccess_L("all(T, F);", false);
	EidosAssertScriptSuccess_L("all(T,T,T,T,T,T,T,T,T,T);", true);
	EidosAssertScriptSuccess_L("all(T,T,T,T,T,T,T,F,T,T);", false);
	EidosAssertScriptSuccess_L("all(F,F,F,F,F,F,F,F,F,F);", false);
	EidosAssertScriptSuccess_L("all(T,T,c(T,T,T,T),c(T,T,T,T));", true);
	EidosAssertScriptSuccess_L("all(T,T,c(T,T,T,T),c(T,F,T,T));", false);
	EidosAssertScriptSuccess_L("all(F,F,c(F,F,F,F),c(F,F,F,F));", false);
	
	// any()
	EidosAssertScriptRaise("any(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("any(0);", 0, "cannot be type");
	EidosAssertScriptRaise("any(0.5);", 0, "cannot be type");
	EidosAssertScriptRaise("any('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("any(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess_L("any(logical(0));", false);
	EidosAssertScriptSuccess_L("any(T);", true);
	EidosAssertScriptSuccess_L("any(F);", false);
	EidosAssertScriptSuccess_L("any(c(T,T,T,T,T,T,T,T,T,T));", true);
	EidosAssertScriptSuccess_L("any(c(T,T,T,T,T,T,T,F,T,T));", true);
	EidosAssertScriptSuccess_L("any(c(F,F,F,F,F,F,F,F,F,F));", false);
	
	EidosAssertScriptRaise("any(F, NULL);", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("any(F, 0);", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("any(F, 0.5);", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("any(F, 'foo');", 0, "all arguments be of type logical");
	EidosAssertScriptRaise("any(F, _Test(7));", 0, "all arguments be of type logical");
	EidosAssertScriptSuccess_L("any(F, logical(0));", false);
	EidosAssertScriptSuccess_L("any(F, T);", true);
	EidosAssertScriptSuccess_L("any(F, F);", false);
	EidosAssertScriptSuccess_L("any(T,T,T,T,T,T,T,T,T,T);", true);
	EidosAssertScriptSuccess_L("any(T,T,T,T,T,T,T,F,T,T);", true);
	EidosAssertScriptSuccess_L("any(F,F,F,F,F,F,F,F,F,F);", false);
	EidosAssertScriptSuccess_L("any(T,T,c(T,T,T,T),c(T,F,T,T));", true);
	EidosAssertScriptSuccess_L("any(F,F,c(F,F,F,F),c(F,T,F,F));", true);
	EidosAssertScriptSuccess_L("any(F,F,c(F,F,F,F),c(F,F,F,F));", false);
	
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
	EidosAssertScriptSuccess_S("format('%d', 0);", "0");
	EidosAssertScriptSuccess_S("format('%f', 0.5);", "0.500000");
	EidosAssertScriptRaise("format('%d', 'foo');", 0, "cannot be type");
	EidosAssertScriptRaise("format('%d', _Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("format('%d', 0.5);", 0, "requires an argument of type integer");
	EidosAssertScriptRaise("format('%f', 5);", 0, "requires an argument of type float");
	EidosAssertScriptSuccess_S("format('foo == %d', 0);", "foo == 0");
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
	EidosAssertScriptSuccess_L("identical(NULL, NULL);", true);
	EidosAssertScriptSuccess_L("identical(NULL, F);", false);
	EidosAssertScriptSuccess_L("identical(NULL, 0);", false);
	EidosAssertScriptSuccess_L("identical(NULL, 0.0);", false);
	EidosAssertScriptSuccess_L("identical(NULL, '');", false);
	EidosAssertScriptSuccess_L("identical(NULL, _Test(0));", false);
	EidosAssertScriptSuccess_L("identical(F, NULL);", false);
	EidosAssertScriptSuccess_L("identical(F, F);", true);
	EidosAssertScriptSuccess_L("identical(F, T);", false);
	EidosAssertScriptSuccess_L("identical(F, 0);", false);
	EidosAssertScriptSuccess_L("identical(F, 0.0);", false);
	EidosAssertScriptSuccess_L("identical(F, '');", false);
	EidosAssertScriptSuccess_L("identical(F, _Test(0));", false);
	EidosAssertScriptSuccess_L("identical(0, NULL);", false);
	EidosAssertScriptSuccess_L("identical(0, F);", false);
	EidosAssertScriptSuccess_L("identical(0, 0);", true);
	EidosAssertScriptSuccess_L("identical(0, 1);", false);
	EidosAssertScriptSuccess_L("identical(0, 0.0);", false);
	EidosAssertScriptSuccess_L("identical(0, '');", false);
	EidosAssertScriptSuccess_L("identical(0, _Test(0));", false);
	EidosAssertScriptSuccess_L("identical(0.0, NULL);", false);
	EidosAssertScriptSuccess_L("identical(0.0, F);", false);
	EidosAssertScriptSuccess_L("identical(0.0, 0);", false);
	EidosAssertScriptSuccess_L("identical(0.0, 0.0);", true);
	EidosAssertScriptSuccess_L("identical(0.0, 0.1);", false);
	EidosAssertScriptSuccess_L("identical(0.1, NAN);", false);
	EidosAssertScriptSuccess_L("identical(NAN, 0.1);", false);
	EidosAssertScriptSuccess_L("identical(NAN, NAN);", true);
	EidosAssertScriptSuccess_L("identical(0.0, '');", false);
	EidosAssertScriptSuccess_L("identical(0.0, _Test(0));", false);
	EidosAssertScriptSuccess_L("identical('', NULL);", false);
	EidosAssertScriptSuccess_L("identical('', F);", false);
	EidosAssertScriptSuccess_L("identical('', 0);", false);
	EidosAssertScriptSuccess_L("identical('', 0.0);", false);
	EidosAssertScriptSuccess_L("identical('', '');", true);
	EidosAssertScriptSuccess_L("identical('', 'x');", false);
	EidosAssertScriptSuccess_L("identical('', _Test(0));", false);
	EidosAssertScriptSuccess_L("identical(_Test(0), NULL);", false);
	EidosAssertScriptSuccess_L("identical(_Test(0), F);", false);
	EidosAssertScriptSuccess_L("identical(_Test(0), 0);", false);
	EidosAssertScriptSuccess_L("identical(_Test(0), 0.0);", false);
	EidosAssertScriptSuccess_L("identical(_Test(0), '');", false);
	EidosAssertScriptSuccess_L("identical(_Test(0), _Test(0));", false);	// object elements not equal
	EidosAssertScriptSuccess_L("identical(F, c(F,F));", false);
	EidosAssertScriptSuccess_L("identical(c(F,F), F);", false);
	EidosAssertScriptSuccess_L("identical(c(F,F), c(F,F));", true);
	EidosAssertScriptSuccess_L("identical(c(F,T,F,T,T), c(F,T,F,T,T));", true);
	EidosAssertScriptSuccess_L("identical(c(F,T,T,T,T), c(F,T,F,T,T));", false);
	EidosAssertScriptSuccess_L("identical(3, c(3,3));", false);
	EidosAssertScriptSuccess_L("identical(c(3,3), 3);", false);
	EidosAssertScriptSuccess_L("identical(c(3,3), c(3,3));", true);
	EidosAssertScriptSuccess_L("identical(c(3,7,3,7,7), c(3,7,3,7,7));", true);
	EidosAssertScriptSuccess_L("identical(c(3,7,7,7,7), c(3,7,3,7,7));", false);
	EidosAssertScriptSuccess_L("identical(3.1, c(3.1,3.1));", false);
	EidosAssertScriptSuccess_L("identical(c(3.1,3.1), 3.1);", false);
	EidosAssertScriptSuccess_L("identical(c(3.1,3.1), c(3.1,3.1));", true);
	EidosAssertScriptSuccess_L("identical(c(3.1,NAN), c(3.1,3.1));", false);
	EidosAssertScriptSuccess_L("identical(c(3.1,3.1), c(3.1,NAN));", false);
	EidosAssertScriptSuccess_L("identical(c(3.1,NAN), c(3.1,NAN));", true);
	EidosAssertScriptSuccess_L("identical(c(3.1,7.1,3.1,7.1,7.1), c(3.1,7.1,3.1,7.1,7.1));", true);
	EidosAssertScriptSuccess_L("identical(c(3.1,7.1,7.1,7.1,7.1), c(3.1,7.1,3.1,7.1,7.1));", false);
	EidosAssertScriptSuccess_L("identical('bar', c('bar','bar'));", false);
	EidosAssertScriptSuccess_L("identical(c('bar','bar'), 'bar');", false);
	EidosAssertScriptSuccess_L("identical(c('bar','bar'), c('bar','bar'));", true);
	EidosAssertScriptSuccess_L("identical(c('bar','baz','bar','baz','baz'), c('bar','baz','bar','baz','baz'));", true);
	EidosAssertScriptSuccess_L("identical(c('bar','baz','baz','baz','baz'), c('bar','baz','bar','baz','baz'));", false);
	EidosAssertScriptSuccess_L("identical(_Test(3), c(_Test(3),_Test(3)));", false);
	EidosAssertScriptSuccess_L("identical(c(_Test(3),_Test(3)), _Test(3));", false);
	EidosAssertScriptSuccess_L("identical(c(_Test(3),_Test(3)), c(_Test(3),_Test(3)));", false);	// object elements not equal
	EidosAssertScriptSuccess_L("x = c(_Test(3),_Test(3)); y = x; identical(x, y);", true);
	EidosAssertScriptSuccess_L("x = _Test(3); y = _Test(7); identical(c(x, y, x, x), c(x, y, x, x));", true);
	EidosAssertScriptSuccess_L("x = _Test(3); y = _Test(7); identical(c(x, y, x, x), c(x, y, y, x));", false);
	
	EidosAssertScriptSuccess_L("identical(matrix(F), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(matrix(F), matrix(F, byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F)), matrix(c(F,T,F,F)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F)), matrix(c(F,T,F,F), byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), byrow=T), matrix(c(F,T,F,F)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), byrow=T), matrix(c(F,T,F,F), byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), nrow=1), matrix(c(F,T,F,F), nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), ncol=1), matrix(c(F,T,F,F), ncol=1));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), nrow=2), matrix(c(F,T,F,F), nrow=2));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), ncol=2), matrix(c(F,T,F,F), ncol=2));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), ncol=2), matrix(c(F,T,F,F), nrow=2));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), nrow=2), matrix(c(F,T,F,F), ncol=2));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), nrow=2, byrow=T), matrix(c(F,T,F,F), nrow=2));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), nrow=2), matrix(c(F,T,F,F), nrow=2, byrow=T));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), nrow=2, byrow=T), matrix(c(F,T,F,F), nrow=2, byrow=T));", true);
 	EidosAssertScriptSuccess_L("identical(F, matrix(F));", false);
	EidosAssertScriptSuccess_L("identical(F, matrix(F, byrow=T));", false);
	EidosAssertScriptSuccess_L("identical(matrix(F), F);", false);
	EidosAssertScriptSuccess_L("identical(matrix(F, byrow=T), F);", false);
	EidosAssertScriptSuccess_L("identical(c(F,T,F,F), matrix(c(F,T,F,F)));", false);
	EidosAssertScriptSuccess_L("identical(c(F,T,F,F), matrix(c(F,T,F,F), nrow=1));", false);
	EidosAssertScriptSuccess_L("identical(c(F,T,F,F), matrix(c(F,T,F,F), ncol=1));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F)), c(F,T,F,F));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), nrow=1), c(F,T,F,F));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), ncol=1), c(F,T,F,F));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), nrow=1), matrix(c(F,T,F,F), ncol=1));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F,F), ncol=1), matrix(c(F,T,F,F), nrow=1));", false);
	EidosAssertScriptSuccess_L("identical(matrix(6), matrix(6));", true);
	EidosAssertScriptSuccess_L("identical(matrix(6), matrix(6, byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6)), matrix(c(6,8,6,6)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6)), matrix(c(6,8,6,6), byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), byrow=T), matrix(c(6,8,6,6)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), byrow=T), matrix(c(6,8,6,6), byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), nrow=1), matrix(c(6,8,6,6), nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), ncol=1), matrix(c(6,8,6,6), ncol=1));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), nrow=2), matrix(c(6,8,6,6), nrow=2));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), ncol=2), matrix(c(6,8,6,6), ncol=2));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), ncol=2), matrix(c(6,8,6,6), nrow=2));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), nrow=2), matrix(c(6,8,6,6), ncol=2));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), nrow=2, byrow=T), matrix(c(6,8,6,6), nrow=2));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), nrow=2), matrix(c(6,8,6,6), nrow=2, byrow=T));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), nrow=2, byrow=T), matrix(c(6,8,6,6), nrow=2, byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(6, matrix(6));", false);
	EidosAssertScriptSuccess_L("identical(6, matrix(6, byrow=T));", false);
	EidosAssertScriptSuccess_L("identical(matrix(6), 6);", false);
	EidosAssertScriptSuccess_L("identical(matrix(6, byrow=T), 6);", false);
	EidosAssertScriptSuccess_L("identical(c(6,8,6,6), matrix(c(6,8,6,6)));", false);
	EidosAssertScriptSuccess_L("identical(c(6,8,6,6), matrix(c(6,8,6,6), nrow=1));", false);
	EidosAssertScriptSuccess_L("identical(c(6,8,6,6), matrix(c(6,8,6,6), ncol=1));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6)), c(6,8,6,6));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), nrow=1), c(6,8,6,6));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), ncol=1), c(6,8,6,6));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), nrow=1), matrix(c(6,8,6,6), ncol=1));", false);
	EidosAssertScriptSuccess_L("identical(matrix(c(6,8,6,6), ncol=1), matrix(c(6,8,6,6), nrow=1));", false);
	
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
	EidosAssertScriptSuccess_L("ifelse(T, T, F);", true);
	EidosAssertScriptSuccess_L("ifelse(F, T, F);", false);
	EidosAssertScriptSuccess_L("ifelse(T, F, T);", false);
	EidosAssertScriptSuccess_L("ifelse(F, F, T);", true);
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
	EidosAssertScriptSuccess_I("ifelse(T, 5, 2);", 5);
	EidosAssertScriptSuccess_I("ifelse(F, 5, 2);", 2);
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
	EidosAssertScriptSuccess_F("ifelse(T, 5.3, 2.1);", 5.3);
	EidosAssertScriptSuccess_F("ifelse(F, 5.3, 2.1);", 2.1);
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
	EidosAssertScriptSuccess_S("ifelse(T, 'foo', 'bar');", "foo");
	EidosAssertScriptSuccess_S("ifelse(F, 'foo', 'bar');", "bar");
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
	EidosAssertScriptSuccess_I("ifelse(T, _Test(5), _Test(2))._yolk;", 5);
	EidosAssertScriptSuccess_I("ifelse(F, _Test(5), _Test(2))._yolk;", 2);
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
	
	EidosAssertScriptSuccess_L("identical(ifelse(matrix(T), 5, 2), matrix(5));", true);
	EidosAssertScriptSuccess_L("identical(ifelse(matrix(F), 5, 2), matrix(2));", true);
	EidosAssertScriptSuccess_L("identical(ifelse(matrix(c(T,T), nrow=1), 5, 2), matrix(c(5,5), nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(ifelse(matrix(c(F,F), ncol=1), 5, 2), matrix(c(2,2), ncol=1));", true);
	EidosAssertScriptSuccess_L("identical(ifelse(array(c(T,F), c(1,2,1)), 5, 2), array(c(5,2), c(1,2,1)));", true);
	EidosAssertScriptSuccess_L("identical(ifelse(matrix(c(T,T), nrow=1), 5:6, 2), matrix(c(5,6), nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(ifelse(matrix(c(T,T), ncol=1), 5, 2:3), matrix(c(5,5), ncol=1));", true);
	EidosAssertScriptSuccess_L("identical(ifelse(array(c(F,F), c(2,1,1)), 5:6, 2), array(c(2,2), c(2,1,1)));", true);
	EidosAssertScriptSuccess_L("identical(ifelse(array(c(F,F), c(1,1,2)), 5, 2:3), array(c(2,3), c(1,1,2)));", true);
	EidosAssertScriptSuccess_L("identical(ifelse(matrix(c(T,T), nrow=1), 5:6, 2:3), matrix(c(5,6), nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(ifelse(matrix(c(F,F), ncol=1), 5:6, 2:3), matrix(c(2,3), ncol=1));", true);
	EidosAssertScriptSuccess_L("identical(ifelse(array(c(T,F), c(1,2,1)), 5:6, 2:3), array(c(5,3), c(1,2,1)));", true);
	EidosAssertScriptSuccess_L("identical(ifelse(matrix(c(T,F,F,T,F,T), nrow=2), 1:6, -6:-1), matrix(c(1,-5,-4,4,-2,6), nrow=2));", true);
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
	EidosAssertScriptSuccess_I("match(9, c(5,1,9));", 2);
	EidosAssertScriptSuccess_I("match(9., c(5,1,9.));", 2);
	EidosAssertScriptSuccess_I("match('baz', c('foo','bar','baz'));", 2);
	EidosAssertScriptSuccess("match(_Test(0), c(_Test(0), _Test(1)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));	// different elements
	EidosAssertScriptSuccess_I("x1 = _Test(1); x2 = _Test(2); x9 = _Test(9); x5 = _Test(5); match(c(x9), c(x5,x1,x9));", 2);
	
	EidosAssertScriptSuccess("match(F, c(T,T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("match(7, c(5,1,9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("match(7., c(5,1,9.));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("match('zip', c('foo','bar','baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("match(_Test(7), c(_Test(0), _Test(1)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));	// different elements
	EidosAssertScriptSuccess("x1 = _Test(1); x2 = _Test(2); x9 = _Test(9); x5 = _Test(5); match(c(x2), c(x5,x1,x9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	
	EidosAssertScriptSuccess("match(c(F,T,F,F,T,T), c(T,T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1, 0, -1, -1, 0, 0}));
	EidosAssertScriptSuccess("match(c(1,2,2,9,5,1), c(5,1,9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -1, -1, 2, 0, 1}));
	EidosAssertScriptSuccess("match(c(1,2,2,9,5,1.), c(5,1,9.));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -1, -1, 2, 0, 1}));
	EidosAssertScriptSuccess("match(c(1,2,NAN,9,5,1.), c(5,1,9.));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -1, -1, 2, 0, 1}));
	EidosAssertScriptSuccess("match(c(1,2,2,9,5,1.), c(5,1,NAN));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -1, -1, -1, 0, 1}));
	EidosAssertScriptSuccess("match(c(1,2,2,NAN,5,1.), c(5,1,NAN));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -1, -1, 2, 0, 1}));
	EidosAssertScriptSuccess("match(c('bar','q','f','baz','foo','bar'), c('foo','bar','baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -1, -1, 2, 0, 1}));
	EidosAssertScriptSuccess("match(c(_Test(0), _Test(1)), c(_Test(0), _Test(1)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1, -1}));	// different elements
	EidosAssertScriptSuccess("x1 = _Test(1); x2 = _Test(2); x9 = _Test(9); x5 = _Test(5); match(c(x1,x2,x2,x9,x5,x1), c(x5,x1,x9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, -1, -1, 2, 0, 1}));
	
	// check the hash-table-based versions of match(), based on the fact that the crossover between algorithms is x_count >= 500
	EidosAssertScriptSuccess_L("x = rdunif(499, 0, 1000); t = rdunif(500, 0, 1000); m1 = match(x, t); m2 = match(c(x, 2000), t); identical(c(m1, -1), m2);", true);
	EidosAssertScriptSuccess_L("x = asFloat(rdunif(499, 0, 1000)); t = asFloat(rdunif(500, 0, 1000)); m1 = match(x, t); m2 = match(c(x, 2000), t); identical(c(m1, -1), m2);", true);
	EidosAssertScriptSuccess_L("x = asString(rdunif(499, 0, 1000)); t = asString(rdunif(500, 0, 1000)); m1 = match(x, t); m2 = match(c(x, 2000), t); identical(c(m1, -1), m2);", true);
	EidosAssertScriptSuccess_L("o = sapply(0:1001, '_Test(applyValue);'); x = o[rdunif(499, 0, 1000)]; t = o[rdunif(500, 0, 1000)]; m1 = match(x, t); m2 = match(c(x, o[1001]), t); identical(c(m1, -1), m2);", true);
	
	// order()
	EidosAssertScriptSuccess("order(integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("order(integer(0), T);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("order(integer(0), F);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess_I("order(3);", 0);
	EidosAssertScriptSuccess_I("order(3, T);", 0);
	EidosAssertScriptSuccess_I("order(3, F);", 0);
	EidosAssertScriptSuccess("order(c(6, 19, -3, 5, 2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 4, 3, 0, 1}));
	EidosAssertScriptSuccess("order(c(6, 19, -3, 5, 2), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 4, 3, 0, 1}));
	EidosAssertScriptSuccess("order(c(2, 5, -3, 19, 6), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 0, 1, 4, 3}));
	EidosAssertScriptSuccess("order(c(6, 19, -3, 5, 2), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 0, 3, 4, 2}));
	EidosAssertScriptSuccess("order(c(2, 5, -3, 19, 6), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4, 1, 0, 2}));
	EidosAssertScriptSuccess("order(c(T, F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 0}));
	EidosAssertScriptSuccess("order(c(6.1, 19.3, -3.7, 5.2, 2.3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 4, 3, 0, 1}));
	EidosAssertScriptSuccess("order(c(6.1, 19.3, -3.7, 5.2, 2.3), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 4, 3, 0, 1}));
	EidosAssertScriptSuccess("order(c(6.1, 19.3, -3.7, 5.2, 2.3), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 0, 3, 4, 2}));
	EidosAssertScriptSuccess("order(c('a', 'q', 'm', 'f', 'w'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 3, 2, 1, 4}));
	EidosAssertScriptSuccess("order(c('a', 'q', 'm', 'f', 'w'), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 3, 2, 1, 4}));
	EidosAssertScriptSuccess("order(c('a', 'q', 'm', 'f', 'w'), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{4, 1, 2, 3, 0}));
	EidosAssertScriptRaise("order(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess_L("x = c(5, 0, NAN, 17, NAN, -17); o = order(x); identical(o, c(5, 1, 0, 3, 2, 4)) | identical(o, c(5, 1, 0, 3, 4, 2));", true);
	EidosAssertScriptSuccess_L("x = c(5, 0, NAN, 17, NAN, -17); o = order(x, ascending=T); identical(o, c(5, 1, 0, 3, 2, 4)) | identical(o, c(5, 1, 0, 3, 4, 2));", true);
	EidosAssertScriptSuccess_L("x = c(5, 0, NAN, 17, NAN, -17); o = order(x, ascending=F); identical(o, c(3, 0, 1, 5, 2, 4)) | identical(o, c(3, 0, 1, 5, 4, 2));", true);
	
	// paste()
	EidosAssertScriptSuccess("paste(NULL);", gStaticEidosValue_StringEmpty);
	EidosAssertScriptSuccess_S("paste(T);", "T");
	EidosAssertScriptSuccess_S("paste(5);", "5");
	EidosAssertScriptSuccess_S("paste(5.5);", "5.5");
	EidosAssertScriptSuccess_S("paste('foo');", "foo");
	EidosAssertScriptSuccess_S("paste(_Test(7));", "_TestElement");
	EidosAssertScriptSuccess("paste(NULL, sep='$$');", gStaticEidosValue_StringEmpty);
	EidosAssertScriptSuccess_S("paste(T, sep='$$');", "T");
	EidosAssertScriptSuccess_S("paste(5, sep='$$');", "5");
	EidosAssertScriptSuccess_S("paste(5.5, sep='$$');", "5.5");
	EidosAssertScriptSuccess_S("paste('foo', sep='$$');", "foo");
	EidosAssertScriptSuccess_S("paste(_Test(7), sep='$$');", "_TestElement");
	EidosAssertScriptSuccess_S("paste(c(T,T,F,T), sep='$$');", "T$$T$$F$$T");
	EidosAssertScriptSuccess_S("paste(5:9, sep='$$');", "5$$6$$7$$8$$9");
	EidosAssertScriptSuccess_S("paste(5.5:8.9, sep='$$');", "5.5$$6.5$$7.5$$8.5");
	EidosAssertScriptSuccess_S("paste(c('foo', 'bar', 'baz'), sep='$$');", "foo$$bar$$baz");
	EidosAssertScriptSuccess_S("paste(c(_Test(7), _Test(7), _Test(7)), sep='$$');", "_TestElement$$_TestElement$$_TestElement");
	EidosAssertScriptSuccess_S("paste(c(T,T,F), 4, NULL, 'foo', 5.5:7.9, 'bar', c(_Test(7), _Test(7)), 5:8);", "T T F 4 foo 5.5 6.5 7.5 bar _TestElement _TestElement 5 6 7 8");
	EidosAssertScriptSuccess_S("paste(c(T,T,F), 4, NULL, 'foo', 5.5:7.9, 'bar', c(_Test(7), _Test(7)), 5:8, sep='$');", "T$T$F$4$foo$5.5$6.5$7.5$bar$_TestElement$_TestElement$5$6$7$8");
	
	// paste0()
	EidosAssertScriptSuccess("paste0(NULL);", gStaticEidosValue_StringEmpty);
	EidosAssertScriptSuccess_S("paste0(T);", "T");
	EidosAssertScriptSuccess_S("paste0(5);", "5");
	EidosAssertScriptSuccess_S("paste0(5.5);", "5.5");
	EidosAssertScriptSuccess_S("paste0('foo');", "foo");
	EidosAssertScriptSuccess_S("paste0(_Test(7));", "_TestElement");
	EidosAssertScriptSuccess("paste0(NULL);", gStaticEidosValue_StringEmpty);
	EidosAssertScriptSuccess_S("paste0(T);", "T");
	EidosAssertScriptSuccess_S("paste0(5);", "5");
	EidosAssertScriptSuccess_S("paste0(5.5);", "5.5");
	EidosAssertScriptSuccess_S("paste0('foo');", "foo");
	EidosAssertScriptSuccess_S("paste0(_Test(7));", "_TestElement");
	EidosAssertScriptSuccess_S("paste0(c(T,T,F,T));", "TTFT");
	EidosAssertScriptSuccess_S("paste0(5:9);", "56789");
	EidosAssertScriptSuccess_S("paste0(5.5:8.9);", "5.56.57.58.5");
	EidosAssertScriptSuccess_S("paste0(c('foo', 'bar', 'baz'));", "foobarbaz");
	EidosAssertScriptSuccess_S("paste0(c(_Test(7), _Test(7), _Test(7)));", "_TestElement_TestElement_TestElement");
	EidosAssertScriptSuccess_S("paste0(c(T,T,F), 4, NULL, 'foo', 5.5:7.9, 'bar', c(_Test(7), _Test(7)), 5:8);", "TTF4foo5.56.57.5bar_TestElement_TestElement5678");
	
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
	EidosAssertScriptSuccess_F("rev(1.0);", 1);
	EidosAssertScriptSuccess_S("rev('foo');", "foo");
	EidosAssertScriptSuccess("rev(6.0:10);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10,9,8,7,6}));
	EidosAssertScriptSuccess("rev(c(T,T,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, true, true}));
}

void _RunFunctionValueInspectionManipulationTests_s_through_z(void)
{
	// size() / length()
	EidosAssertScriptSuccess("size(NULL);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("size(logical(0));", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("size(5);", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess_I("size(c(5.5, 8.7));", 2);
	EidosAssertScriptSuccess_I("size(c('foo', 'bar', 'baz'));", 3);
	EidosAssertScriptSuccess_I("size(rep(_Test(7), 4));", 4);
	
	EidosAssertScriptSuccess("length(NULL);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("length(logical(0));", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("length(5);", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess_I("length(c(5.5, 8.7));", 2);
	EidosAssertScriptSuccess_I("length(c('foo', 'bar', 'baz'));", 3);
	EidosAssertScriptSuccess_I("length(rep(_Test(7), 4));", 4);
	
	// sort()
	EidosAssertScriptSuccess("sort(integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("sort(integer(0), T);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("sort(integer(0), F);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess_I("sort(3);", 3);
	EidosAssertScriptSuccess_I("sort(3, T);", 3);
	EidosAssertScriptSuccess_I("sort(3, F);", 3);
	EidosAssertScriptSuccess("sort(c(6, 19, -3, 5, 2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-3, 2, 5, 6, 19}));
	EidosAssertScriptSuccess("sort(c(6, 19, -3, 5, 2), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-3, 2, 5, 6, 19}));
	EidosAssertScriptSuccess("sort(c(6, 19, -3, 5, 2), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{19, 6, 5, 2, -3}));
	EidosAssertScriptSuccess("sort(c(T, F, T, T, F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, true, true}));
	EidosAssertScriptSuccess("sort(c(6.1, 19.3, -3.7, 5.2, 2.3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-3.7, 2.3, 5.2, 6.1, 19.3}));
	EidosAssertScriptSuccess("sort(c(6.1, 19.3, -3.7, 5.2, 2.3), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-3.7, 2.3, 5.2, 6.1, 19.3}));
	EidosAssertScriptSuccess("sort(c(6.1, 19.3, -3.7, 5.2, 2.3), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{19.3, 6.1, 5.2, 2.3, -3.7}));
	EidosAssertScriptSuccess("sort(c('a', 'q', 'm', 'f', 'w'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"a", "f", "m", "q", "w"}));
	EidosAssertScriptSuccess("sort(c('a', 'q', 'm', 'f', 'w'), T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"a", "f", "m", "q", "w"}));
	EidosAssertScriptSuccess("sort(c('a', 'q', 'm', 'f', 'w'), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"w", "q", "m", "f", "a"}));
	EidosAssertScriptRaise("sort(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("x = c(5, 0, NAN, 17, NAN, -17); sort(x);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-17, 0, 5, 17, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("x = c(5, 0, NAN, 17, NAN, -17); sort(x, ascending=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-17, 0, 5, 17, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("x = c(5, 0, NAN, 17, NAN, -17); sort(x, ascending=F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{17, 5, 0, -17, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()}));
	
	// sortBy()
	EidosAssertScriptRaise("sortBy(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("sortBy(T);", 0, "cannot be type");
	EidosAssertScriptRaise("sortBy(5);", 0, "cannot be type");
	EidosAssertScriptRaise("sortBy(9.1);", 0, "cannot be type");
	EidosAssertScriptRaise("sortBy('foo');", 0, "cannot be type");
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
	
	// tabulate()
	EidosAssertScriptSuccess_I("tabulate(integer(0));", 0);
	EidosAssertScriptSuccess_I("tabulate(integer(0), 0);", 0);
	EidosAssertScriptSuccess("tabulate(integer(0), 4);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0, 0, 0}));
	EidosAssertScriptSuccess("tabulate(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0, 1}));
	EidosAssertScriptSuccess("tabulate(3, 4);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0, 1, 0}));
	EidosAssertScriptSuccess("tabulate(3, 2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 0, 0}));
	EidosAssertScriptSuccess("tabulate(c(0, -1, 0, -5, 5, 3, 3, 3, 0, 3, 4, 5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 0, 0, 4, 1, 2}));
	EidosAssertScriptSuccess("tabulate(c(0, -1, 0, -5, 5, 3, 3, 3, 0, 3, 4, 5), 8);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 0, 0, 4, 1, 2, 0, 0, 0}));
	EidosAssertScriptSuccess("tabulate(c(0, -1, 0, -5, 5, 3, 3, 3, 0, 3, 4, 5), 3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 0, 0, 4}));
	EidosAssertScriptSuccess_I("sum(tabulate(rdunif(100, 5, 15)));", 100);
	EidosAssertScriptSuccess_I("sum(tabulate(rdunif(100, 5, 15), 25));", 100);
	EidosAssertScriptRaise("tabulate(c(0, -1, 0, -5, 5, 3, 3, 3, 0, 3, 4, 5), -1);", 0, "to be greater than or equal to 0");
	
	// unique()
	EidosAssertScriptSuccess("unique(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("unique(logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("unique(integer(0));", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("unique(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("unique(string(0));", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("unique(object());", gStaticEidosValue_Object_ZeroVec);
	EidosAssertScriptSuccess_L("unique(T);", true);
	EidosAssertScriptSuccess_I("unique(5);", 5);
	EidosAssertScriptSuccess_F("unique(3.5);", 3.5);
	EidosAssertScriptSuccess_S("unique('foo');", "foo");
	EidosAssertScriptSuccess_I("unique(_Test(7))._yolk;", 7);
	EidosAssertScriptSuccess("unique(c(T,T,T,T,F,T,T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("unique(c(3,5,3,9,2,3,3,7,5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 5, 9, 2, 7}));
	EidosAssertScriptSuccess("unique(c(3.5,1.2,9.3,-1.0,1.2,-1.0,1.2,7.6,3.5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.5, 1.2, 9.3, -1, 7.6}));
	EidosAssertScriptSuccess("unique(c(3.5,1.2,9.3,-1.0,NAN,1.2,-1.0,1.2,7.6,3.5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.5, 1.2, 9.3, -1, std::numeric_limits<double>::quiet_NaN(), 7.6}));
	EidosAssertScriptSuccess("unique(c(3.5,1.2,9.3,-1.0,NAN,1.2,-1.0,1.2,NAN, 7.6,3.5));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.5, 1.2, 9.3, -1, std::numeric_limits<double>::quiet_NaN(), 7.6}));
	EidosAssertScriptSuccess("unique(c('foo', 'bar', 'foo', 'baz', 'baz', 'bar', 'foo'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar", "baz"}));
	EidosAssertScriptSuccess("unique(c(_Test(7), _Test(7), _Test(2), _Test(7), _Test(2)))._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 7, 2, 7, 2}));
	
	EidosAssertScriptSuccess("unique(NULL, F);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("unique(logical(0), F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("unique(integer(0), F);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("unique(float(0), F);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("unique(string(0), F);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("unique(object(), F);", gStaticEidosValue_Object_ZeroVec);
	EidosAssertScriptSuccess_L("unique(T, F);", true);
	EidosAssertScriptSuccess_I("unique(5, F);", 5);
	EidosAssertScriptSuccess_F("unique(3.5, F);", 3.5);
	EidosAssertScriptSuccess_S("unique('foo', F);", "foo");
	EidosAssertScriptSuccess_I("unique(_Test(7), F)._yolk;", 7);
	EidosAssertScriptSuccess("unique(c(T,T,T,T,F,T,T), F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false}));
	EidosAssertScriptSuccess("sort(unique(c(3,5,3,9,2,3,3,7,5), F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 5, 7, 9}));
	EidosAssertScriptSuccess("sort(unique(c(3.5,1.2,9.3,-1.0,1.2,-1.0,1.2,7.6,3.5), F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-1, 1.2, 3.5, 7.6, 9.3}));
	EidosAssertScriptSuccess("sort(unique(c(3.5,1.2,9.3,-1.0,NAN,1.2,-1.0,1.2,7.6,3.5), F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-1, 1.2, 3.5, 7.6, 9.3, std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("sort(unique(c(3.5,1.2,9.3,-1.0,NAN,1.2,-1.0,1.2,NAN,7.6,3.5), F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{-1, 1.2, 3.5, 7.6, 9.3, std::numeric_limits<double>::quiet_NaN()}));
	EidosAssertScriptSuccess("sort(unique(c('foo', 'bar', 'foo', 'baz', 'baz', 'bar', 'foo'), F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"bar", "baz", "foo"}));
	EidosAssertScriptSuccess("sort(unique(c(_Test(7), _Test(7), _Test(2), _Test(7), _Test(2)), F)._yolk);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 2, 7, 7, 7}));
	
	EidosAssertScriptSuccess_L("x = asInteger(runif(10000, 0, 10000)); size(unique(x)) == size(unique(x, F));", true);
	EidosAssertScriptSuccess_L("x = runif(10000, 0, 1); size(unique(x)) == size(unique(x, F));", true);
	
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
	EidosAssertScriptSuccess_I("whichMax(c(F, F, T, F, T));", 2);
	EidosAssertScriptSuccess_I("whichMax(c(3, 7, 19, -5, 9));", 2);
	EidosAssertScriptSuccess_I("whichMax(c(3.3, 7.7, 19.1, -5.8, 9.0));", 2);
	EidosAssertScriptSuccess_I("whichMax(c(3.3, 7.7, 19.1, NAN, -5.8, 9.0));", 2);
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
	EidosAssertScriptSuccess_I("whichMin(c(3, 7, 19, -5, 9));", 3);
	EidosAssertScriptSuccess_I("whichMin(c(3.3, 7.7, 19.1, -5.8, 9.0));", 3);
	EidosAssertScriptSuccess_I("whichMin(c(3.3, 7.7, 19.1, NAN, -5.8, 9.0));", 4);
	EidosAssertScriptSuccess("whichMin(c('foo', 'bar', 'baz'));", gStaticEidosValue_Integer1);
	EidosAssertScriptRaise("whichMin(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("whichMin(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(logical(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(integer(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(float(0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("whichMin(string(0));", gStaticEidosValueNULL);
}

#pragma mark value type testing / coercion
void _RunStringManipulationTests(void)
{
	// nchar()
	EidosAssertScriptRaise("nchar(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("nchar(T);", 0, "cannot be type");
	EidosAssertScriptRaise("nchar(5);", 0, "cannot be type");
	EidosAssertScriptRaise("nchar(5.5);", 0, "cannot be type");
	EidosAssertScriptRaise("nchar(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess("nchar('');", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("nchar(' ');", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess_I("nchar('abcde');", 5);
	EidosAssertScriptSuccess_I("nchar('abc\tde');", 6);
	EidosAssertScriptSuccess("nchar(c('', 'abcde', '', 'wumpus'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 5, 0, 6}));
	
	EidosAssertScriptSuccess_L("identical(nchar(matrix('abc\tde')), matrix(nchar('abc\tde')));", true);
	EidosAssertScriptSuccess_L("identical(nchar(matrix(c('', 'abcde', '', 'wumpus'), nrow=2)), matrix(nchar(c('', 'abcde', '', 'wumpus')), nrow=2));", true);
	
	// strcontains()
	EidosAssertScriptRaise("strcontains('foobarbazxyzzy', '');", 0, "to be of length >= 1");
	EidosAssertScriptRaise("strcontains('foobarbazxyzzy', 'foo', pos=-1);", 0, "to be >= 0");
	EidosAssertScriptSuccess_L("strcontains('', 'a');", false);
	EidosAssertScriptSuccess_L("strcontains('', 'a', pos=1000000);", false);
	EidosAssertScriptSuccess_L("strcontains('foobarbazxyzzy', 'foo', pos=0);", true);
	EidosAssertScriptSuccess_L("strcontains('foobarbazxyzzy', 'foo', pos=1);", false);
	EidosAssertScriptSuccess_L("strcontains('foobarbazxyzzy', 'ba', pos=0);", true);
	EidosAssertScriptSuccess_L("strcontains('foobarbazxyzzy', 'ba', pos=1);", true);
	EidosAssertScriptSuccess_L("strcontains('foobarbazxyzzy', 'ba', pos=3);", true);
	EidosAssertScriptSuccess_L("strcontains('foobarbazxyzzy', 'ba', pos=4);", true);
	EidosAssertScriptSuccess_L("strcontains('foobarbazxyzzy', 'ba', pos=6);", true);
	EidosAssertScriptSuccess_L("strcontains('foobarbazxyzzy', 'ba', pos=7);", false);
	EidosAssertScriptSuccess_L("strcontains('foobarbazxyzzy', 'wumpus');", false);
	EidosAssertScriptSuccess("strcontains(c('foobarbazxyzzy', 'barfoo', 'xyzzyfoobaz', 'xyzzy'), 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, false}));
	EidosAssertScriptSuccess("strcontains(c('foobarbazxyzzy', 'barfoo', 'xyzzyfoobaz', 'xyzzy'), 'foo', pos=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, true, false}));
	
	// strfind()
	EidosAssertScriptRaise("strfind('foobarbazxyzzy', '');", 0, "to be of length >= 1");
	EidosAssertScriptRaise("strfind('foobarbazxyzzy', 'foo', pos=-1);", 0, "to be >= 0");
	EidosAssertScriptSuccess("strfind('', 'a');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("strfind('', 'a', pos=1000000);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("strfind('foobarbazxyzzy', 'foo', pos=0);", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("strfind('foobarbazxyzzy', 'foo', pos=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("strfind('foobarbazxyzzy', 'ba', pos=0);", gStaticEidosValue_Integer3);
	EidosAssertScriptSuccess("strfind('foobarbazxyzzy', 'ba', pos=1);", gStaticEidosValue_Integer3);
	EidosAssertScriptSuccess("strfind('foobarbazxyzzy', 'ba', pos=3);", gStaticEidosValue_Integer3);
	EidosAssertScriptSuccess_I("strfind('foobarbazxyzzy', 'ba', pos=4);", 6);
	EidosAssertScriptSuccess_I("strfind('foobarbazxyzzy', 'ba', pos=6);", 6);
	EidosAssertScriptSuccess("strfind('foobarbazxyzzy', 'ba', pos=7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("strfind('foobarbazxyzzy', 'wumpus');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-1)));
	EidosAssertScriptSuccess("strfind(c('foobarbazxyzzy', 'barfoo', 'xyzzyfoobaz', 'xyzzy'), 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{0, 3, 5, -1}));
	EidosAssertScriptSuccess("strfind(c('foobarbazxyzzy', 'barfoo', 'xyzzyfoobaz', 'xyzzy'), 'foo', pos=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1, 3, 5, -1}));
	
	// strprefix()
	EidosAssertScriptRaise("strprefix('foobarbazxyzzy', '');", 0, "to be of length >= 1");
	EidosAssertScriptSuccess_L("strprefix('', 'a');", false);
	EidosAssertScriptSuccess_L("strprefix('foobarbazxyzzy', 'foo');", true);
	EidosAssertScriptSuccess_L("strprefix('foobarbazxyzzy', 'oob');", false);
	EidosAssertScriptSuccess_L("strprefix('foobarbazxyzzy', 'f');", true);
	EidosAssertScriptSuccess_L("strprefix('foobarbazxyzzy', 'o');", false);
	EidosAssertScriptSuccess("strprefix(c('foobarbazxyzzy', 'barfoo', 'xyzzyfoobaz', 'xyzzy'), 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, false}));
	EidosAssertScriptSuccess("strprefix(c('foobarbazxyzzy', 'barfoo', 'xyzzyfoobaz', 'xyzzy'), 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, false, false}));
	EidosAssertScriptSuccess("strprefix(c('foobarbazxyzzy', 'barfoo', 'xyzzyfoobaz', 'xyzzy'), 'x');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, true}));
	EidosAssertScriptSuccess("strprefix(c('foobarbazxyzzy', 'barfoo', 'xyzzyfoobaz', 'xyzzy'), 'xyzzyf');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false}));
	
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
	EidosAssertScriptSuccess_S("strsplit('This is a test.', '$$');", "This is a test.");
	EidosAssertScriptSuccess("strsplit('This is a test.', 'i');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"Th", "s ", "s a test."}));
	EidosAssertScriptSuccess("strsplit('This is a test.', 's');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"Thi", " i", " a te", "t."}));
	EidosAssertScriptSuccess("strsplit('This is a test.', '');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"T", "h", "i", "s", " ", "i", "s", " ", "a", " ", "t", "e", "s", "t", "."}));
	
	// strsuffix()
	EidosAssertScriptRaise("strsuffix('foobarbazxyzzy', '');", 0, "to be of length >= 1");
	EidosAssertScriptSuccess_L("strsuffix('', 'a');", false);
	EidosAssertScriptSuccess_L("strsuffix('foobarbazxyzzy', 'zzy');", true);
	EidosAssertScriptSuccess_L("strsuffix('foobarbazxyzzy', 'yzz');", false);
	EidosAssertScriptSuccess_L("strsuffix('foobarbazxyzzy', 'y');", true);
	EidosAssertScriptSuccess_L("strsuffix('foobarbazxyzzy', 'z');", false);
	EidosAssertScriptSuccess("strsuffix(c('foobarbazxyzzy', 'barfoo', 'xyzzyfoobaz', 'xyzzy'), 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, false, false}));
	EidosAssertScriptSuccess("strsuffix(c('foobarbazxyzzy', 'barfoo', 'xyzzyfoobaz', 'xyzzy'), 'baz');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false}));
	EidosAssertScriptSuccess("strsuffix(c('foobarbazxyzzy', 'barfoo', 'xyzzyfoobaz', 'xyzzy'), 'y');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, true}));
	EidosAssertScriptSuccess("strsuffix(c('foobarbazxyzzy', 'barfoo', 'xyzzyfoobaz', 'xyzzy'), 'zxyzzy');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, false}));
	
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
	EidosAssertScriptSuccess_L("identical(asFloat(matrix(c('1','2','3'))), matrix(1.0:3.0));", true);
	
	// asInteger()
	EidosAssertScriptSuccess("asInteger(-1:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1,0,1,2,3}));
	EidosAssertScriptSuccess("asInteger(-1.0:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{-1,0,1,2,3}));
	EidosAssertScriptSuccess("asInteger(c(T,F,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1,0,1,0}));
	EidosAssertScriptSuccess("asInteger(c('1','2','3'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1,2,3}));
	EidosAssertScriptRaise("asInteger('foo');", 0, "could not be represented");
	EidosAssertScriptRaise("asInteger(NAN);", 0, "cannot be converted");
	EidosAssertScriptSuccess_L("identical(asInteger(matrix(c('1','2','3'))), matrix(1:3));", true);
	
	// asInteger() overflow tests; these may be somewhat platform-dependent but I doubt it will bite us
	EidosAssertScriptRaise("asInteger(asFloat(9223372036854775807));", 0, "too large to be converted");																// the double representation is larger than INT64_MAX
	EidosAssertScriptRaise("asInteger(asFloat(9223372036854775807-511));", 0, "too large to be converted");															// the same double representation as previous
	EidosAssertScriptSuccess_I("asInteger(asFloat(9223372036854775807-512));", 9223372036854774784);	// 9223372036854774784 == 9223372036854775807-1023, the closest value to INT64_MAX that double can represent
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
	EidosAssertScriptSuccess_L("asLogical(1);", true);
	EidosAssertScriptSuccess_L("asLogical(0);", false);
	EidosAssertScriptSuccess("asLogical(-1:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true,false,true,true,true}));
	EidosAssertScriptSuccess("asLogical(-1.0:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true,false,true,true,true}));
	EidosAssertScriptRaise("asLogical(NAN);", 0, "cannot be converted");
	EidosAssertScriptSuccess("asLogical(c(T,F,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true,false,true,false}));
	EidosAssertScriptSuccess("asLogical(c('foo','bar',''));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true,true,false}));
	EidosAssertScriptSuccess_L("identical(asLogical(matrix(-1:3)), matrix(c(T,F,T,T,T)));", true);
	
	// asString()
	EidosAssertScriptSuccess("asString(NULL);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"NULL"}));
	EidosAssertScriptSuccess("asString(-1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"-1"}));
	EidosAssertScriptSuccess("asString(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"3"}));
	EidosAssertScriptSuccess("asString(-1:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"-1","0","1","2","3"}));
	EidosAssertScriptSuccess("asString(-1.0:3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"-1","0","1","2","3"}));
	EidosAssertScriptSuccess("asString(c(1.0, NAN, -2.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"1","NAN","-2"}));
	EidosAssertScriptSuccess("asString(c(T,F,T,F));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"T","F","T","F"}));
	EidosAssertScriptSuccess("asString(c('1','2','3'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"1","2","3"}));
	EidosAssertScriptSuccess_L("identical(asString(matrix(-1:3)), matrix(c('-1','0','1','2','3')));", true);
	
	// elementType()
	EidosAssertScriptSuccess_S("elementType(NULL);", "NULL");
	EidosAssertScriptSuccess_S("elementType(T);", "logical");
	EidosAssertScriptSuccess_S("elementType(3);", "integer");
	EidosAssertScriptSuccess_S("elementType(3.5);", "float");
	EidosAssertScriptSuccess_S("elementType('foo');", "string");
	EidosAssertScriptSuccess_S("elementType(_Test(7));", "_TestElement");
	EidosAssertScriptSuccess_S("elementType(object());", "Object");
	EidosAssertScriptSuccess_S("elementType(c(object(), object()));", "Object");
	EidosAssertScriptSuccess_S("elementType(c(_Test(7), object()));", "_TestElement");
	EidosAssertScriptSuccess_S("elementType(c(object(), _Test(7)));", "_TestElement");
	EidosAssertScriptSuccess_S("elementType(_Test(7)[F]);", "_TestElement");
	
	// isFloat()
	EidosAssertScriptSuccess_L("isFloat(NULL);", false);
	EidosAssertScriptSuccess_L("isFloat(T);", false);
	EidosAssertScriptSuccess_L("isFloat(3);", false);
	EidosAssertScriptSuccess_L("isFloat(3.5);", true);
	EidosAssertScriptSuccess_L("isFloat('foo');", false);
	EidosAssertScriptSuccess_L("isFloat(_Test(7));", false);
	EidosAssertScriptSuccess_L("isFloat(object());", false);
	
	// isInteger()
	EidosAssertScriptSuccess_L("isInteger(NULL);", false);
	EidosAssertScriptSuccess_L("isInteger(T);", false);
	EidosAssertScriptSuccess_L("isInteger(3);", true);
	EidosAssertScriptSuccess_L("isInteger(3.5);", false);
	EidosAssertScriptSuccess_L("isInteger('foo');", false);
	EidosAssertScriptSuccess_L("isInteger(_Test(7));", false);
	EidosAssertScriptSuccess_L("isInteger(object());", false);
	
	// isLogical()
	EidosAssertScriptSuccess_L("isLogical(NULL);", false);
	EidosAssertScriptSuccess_L("isLogical(T);", true);
	EidosAssertScriptSuccess_L("isLogical(3);", false);
	EidosAssertScriptSuccess_L("isLogical(3.5);", false);
	EidosAssertScriptSuccess_L("isLogical('foo');", false);
	EidosAssertScriptSuccess_L("isLogical(_Test(7));", false);
	EidosAssertScriptSuccess_L("isLogical(object());", false);
	
	// isNULL()
	EidosAssertScriptSuccess_L("isNULL(NULL);", true);
	EidosAssertScriptSuccess_L("isNULL(T);", false);
	EidosAssertScriptSuccess_L("isNULL(3);", false);
	EidosAssertScriptSuccess_L("isNULL(3.5);", false);
	EidosAssertScriptSuccess_L("isNULL('foo');", false);
	EidosAssertScriptSuccess_L("isNULL(_Test(7));", false);
	EidosAssertScriptSuccess_L("isNULL(object());", false);
	
	// isObject()
	EidosAssertScriptSuccess_L("isObject(NULL);", false);
	EidosAssertScriptSuccess_L("isObject(T);", false);
	EidosAssertScriptSuccess_L("isObject(3);", false);
	EidosAssertScriptSuccess_L("isObject(3.5);", false);
	EidosAssertScriptSuccess_L("isObject('foo');", false);
	EidosAssertScriptSuccess_L("isObject(_Test(7));", true);
	EidosAssertScriptSuccess_L("isObject(object());", true);
	
	// isString()
	EidosAssertScriptSuccess_L("isString(NULL);", false);
	EidosAssertScriptSuccess_L("isString(T);", false);
	EidosAssertScriptSuccess_L("isString(3);", false);
	EidosAssertScriptSuccess_L("isString(3.5);", false);
	EidosAssertScriptSuccess_L("isString('foo');", true);
	EidosAssertScriptSuccess_L("isString(_Test(7));", false);
	EidosAssertScriptSuccess_L("isString(object());", false);
	
	// type()
	EidosAssertScriptSuccess_S("type(NULL);", "NULL");
	EidosAssertScriptSuccess_S("type(T);", "logical");
	EidosAssertScriptSuccess_S("type(3);", "integer");
	EidosAssertScriptSuccess_S("type(3.5);", "float");
	EidosAssertScriptSuccess_S("type('foo');", "string");
	EidosAssertScriptSuccess_S("type(_Test(7));", "object");
	EidosAssertScriptSuccess_S("type(object());", "object");
}
































