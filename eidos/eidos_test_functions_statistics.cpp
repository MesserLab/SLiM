//
//  eidos_test_functions_statistics.cpp
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

#include <limits>


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
	EidosAssertScriptSuccess("cor(c(1.0, 2.0, NAN), c(8.0, 9.0, 10.0));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("cor(c(1.0, 2.0, 3.0), c(8.0, 9.0, NAN));", gStaticEidosValue_FloatNAN);
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
	EidosAssertScriptSuccess("cov(c(1.0, 2.0, NAN), c(8.0, 9.0, 10.0));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("cov(c(1.0, 2.0, 3.0), c(8.0, 9.0, NAN));", gStaticEidosValue_FloatNAN);
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
	EidosAssertScriptSuccess("pmax(-8., NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmax(NAN, INF);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmax(INF, NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmax(c(1.,-INF,7.,INF,NAN,-8.,NAN), c(5.,6.,1.,-8.,-8.,NAN,INF));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{5, 6, 7, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()}));
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
	EidosAssertScriptSuccess("pmin(-8., NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmin(NAN, INF);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmin(INF, NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmin(c(1.,-INF,7.,INF,NAN,-8.,NAN), c(5.,6.,1.,-8.,-8.,NAN,INF));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, -std::numeric_limits<double>::infinity(), 1, -8, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()}));
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
	
	// quantile()
	EidosAssertScriptRaise("quantile(integer(0));", 0, "x to have length greater than 0");
	EidosAssertScriptRaise("quantile(float(0));", 0, "x to have length greater than 0");
	EidosAssertScriptSuccess("quantile(INF, 0.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(std::numeric_limits<double>::infinity())));
	EidosAssertScriptSuccess("quantile(-INF, 0.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(-std::numeric_limits<double>::infinity())));
	EidosAssertScriptSuccess("quantile(0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0.0, 0.0, 0.0, 0.0, 0.0}));
	EidosAssertScriptSuccess("quantile(1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 1.0, 1.0, 1.0, 1.0}));
	EidosAssertScriptRaise("quantile(integer(0), float(0));", 0, "x to have length greater than 0");
	EidosAssertScriptSuccess("quantile(0, float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("quantile(1, float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("quantile(1, -0.0000001);", 0, "requires probabilities to be in [0, 1]");
	EidosAssertScriptRaise("quantile(1, 1.0000001);", 0, "requires probabilities to be in [0, 1]");
	EidosAssertScriptRaise("quantile(NAN);", 0, "quantiles of NAN are undefined");
	EidosAssertScriptRaise("quantile(c(-5, 7, 2, NAN, 9));", 0, "quantiles of NAN are undefined");
	EidosAssertScriptRaise("quantile(c(-5, 7, 2, 8, 9), -0.0000001);", 0, "requires probabilities to be in [0, 1]");
	EidosAssertScriptRaise("quantile(c(-5, 7, 2, 8, 9), 1.0000001);", 0, "requires probabilities to be in [0, 1]");
	EidosAssertScriptSuccess("quantile(0:100);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0, 25, 50, 75, 100}));
	EidosAssertScriptSuccess("quantile(0:100, 0.27);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(27)));
	EidosAssertScriptSuccess("quantile(0:100, c(0.8, 0.3, 0.72, 0.0, 0.67));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{80, 30, 72, 0, 67}));
	EidosAssertScriptSuccess("quantile(0:10, c(0.15, 0.25, 0.5, 0.82));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.5, 2.5, 5.0, 8.2}));
	EidosAssertScriptSuccess("quantile(10:0, c(0.15, 0.25, 0.5, 0.82));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.5, 2.5, 5.0, 8.2}));
	EidosAssertScriptSuccess("quantile(c(17, 12, 4, 87, 3, 1081, 273));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3, 8, 17, 180, 1081}));
	
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
	EidosAssertScriptSuccess("ttest(c(1.0, 2.0, NAN), c(8.0, 9.0, 10.0));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("ttest(c(1.0, 2.0, 3,0), c(8.0, 9.0, NAN));", gStaticEidosValue_FloatNAN);
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
	EidosAssertScriptRaise("dmvnorm(c(0.0, 2.0), c(0.0, 2.0), matrix(c(10,3,NAN,2), nrow=2));", 0, "to contain NANs");
	
	// dnorm()
	EidosAssertScriptSuccess("dnorm(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("dnorm(float(0), float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("dnorm(0.0, 0, 1) - 0.3989423 < 0.00001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("dnorm(1.0, 1.0, 1.0) - 0.3989423 < 0.00001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("dnorm(c(0.0,0.0), c(0,0), 1) - 0.3989423 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("dnorm(c(0.0,1.0), c(0.0,1.0), 1.0) - 0.3989423 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("dnorm(c(0.0,0.0), 0.0, c(1.0,1.0)) - 0.3989423 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("dnorm(c(-1.0,0.0,1.0)) - c(0.2419707,0.3989423,0.2419707) < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptRaise("dnorm(1.0, 0, 0);", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("dnorm(1.0, 0.0, -1.0);", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("dnorm(c(0.5, 1.0), 0.0, c(5, -1.0));", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("dnorm(1.0, c(-10, 10, 1), 100.0);", 0, "requires mean to be");
	EidosAssertScriptRaise("dnorm(1.0, 10.0, c(0.1, 10, 1));", 0, "requires sd to be");
	EidosAssertScriptSuccess("dnorm(NAN, 0, 1);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("dnorm(1.0, NAN, 1);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("dnorm(1.0, 0, NAN);", gStaticEidosValue_FloatNAN);
		
	// qnorm()
	EidosAssertScriptSuccess("-qnorm(0.0);", gStaticEidosValue_FloatINF);
	EidosAssertScriptSuccess("qnorm(1.0);", gStaticEidosValue_FloatINF);
	EidosAssertScriptSuccess("qnorm(0.05) + 1.644854 < 0.00001 ;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("qnorm(0.95) - 1.644854 < 0.00001 ;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("qnorm(0.05, 0, 1) + 1.644854 < 0.00001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("qnorm(0.05, 5.5, 3.4) + 0.09250233 < 0.00001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("qnorm(0.05, 0, 1.0) + 1.644854 < 0.00001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("qnorm(c(0.05,0.05), c(0, 0), 1) + 1.644854 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("c(2, 1)*qnorm(c(0.05, 0.05), 0., c(1, 2)) + 3.289707 < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("qnorm(c(0.25, 0.5, 0.75)) - c(-0.6744898, 0.0000000, 0.6744898) < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptRaise("qnorm(0.5, 0, 0);", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("qnorm(-0.1);", 0, "requires 0.0 <= p <= 1.0");
	EidosAssertScriptRaise("qnorm(1.1);", 0, "requires 0.0 <= p <= 1.0");
	EidosAssertScriptRaise("qnorm(c(0.05, 1.1));", 0, "requires 0.0 <= p <= 1.0");
	EidosAssertScriptRaise("qnorm(c(0.05, 0.95), 0.0, c(5, -1.0));", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("qnorm(0.1, c(-10, 10, 1), 100.0);", 0, "requires mean to be");
	EidosAssertScriptRaise("qnorm(0.1, 10.0, c(0.1, 10, 1));", 0, "requires sd to be");
	EidosAssertScriptSuccess("qnorm(NAN);", gStaticEidosValue_FloatNAN);

	// pnorm()
	EidosAssertScriptSuccess("pnorm(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("pnorm(float(0), float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("pnorm(0.0, 0, 1) - 0.5 < 0.00001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("pnorm(1.0, 1.0, 1.0) - 0.5 < 0.00001;", gStaticEidosValue_LogicalT);
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
	
	// dbeta()
	EidosAssertScriptSuccess("dbeta(float(0), 1, 1000);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("dbeta(float(0), float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("abs(dbeta(0.0, 1, 5) - c(5)) < 0.0001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(dbeta(0.5, 1, 5) - c(0.3125)) < 0.0001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(dbeta(1.0, 1, 5) - c(0)) < 0.0001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(dbeta(c(0, 0.5, 1), 1, 5) - c(5, 0.3125, 0)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("abs(dbeta(c(0, 0.5, 1), 1, c(10, 4, 1)) - c(10, 0.5, 1)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("abs(dbeta(c(0, 0.5, 1), c(1, 2, 3), c(10, 4, 1)) - c(10, 1.25, 3)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptRaise("dbeta(c(0.0, 0), 0, 1);", 0, "requires alpha > 0.0");
	EidosAssertScriptRaise("dbeta(c(0.0, 0), c(1,0), 1);", 0, "requires alpha > 0.0");
	EidosAssertScriptRaise("dbeta(c(0.0, 0), 1, 0);", 0, "requires beta > 0.0");
	EidosAssertScriptRaise("dbeta(c(0.0, 0), 1, c(1,0));", 0, "requires beta > 0.0");
	EidosAssertScriptRaise("dbeta(c(0.0, 0), c(0.1, 10, 1), 10.0);", 0, "requires alpha to be of length");
	EidosAssertScriptRaise("dbeta(c(0.0, 0), 10.0, c(0.1, 10, 1));", 0, "requires beta to be of length");
	EidosAssertScriptSuccess("dbeta(NAN, 1, 5);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptRaise("dbeta(0.5, NAN, 5);", 0, "requires alpha > 0.0");
	EidosAssertScriptRaise("dbeta(0.5, 1, NAN);", 0, "requires beta > 0.0");
	
	// rbeta()
	EidosAssertScriptSuccess("rbeta(0, 1, 1000);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rbeta(0, float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setSeed(0); abs(rbeta(1, 1, 5) - c(0.115981)) < 0.0001;", gStaticEidosValue_LogicalT);
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
	
	// dexp()
	EidosAssertScriptSuccess("dexp(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("dexp(float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("abs(dexp(1.0) - 0.3678794) < 0.00001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(dexp(0.01) - 0.9900498) < 0.00001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(dexp(0.01, 0.1) - 9.048374) < 0.00001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(dexp(0.01, 0.01) - 36.78794) < 0.0001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(dexp(c(0.01, 0.01, 0.01), c(1, 0.1, 0.01)) - c(0.9900498, 9.048374, 36.78794)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptRaise("dexp(3.0, c(10, 5));", 0, "requires mu to be");
	EidosAssertScriptSuccess("dexp(NAN, 0.1);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("dexp(0.01, NAN);", gStaticEidosValue_FloatNAN);
	
	// rexp()
	EidosAssertScriptSuccess("rexp(0);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rexp(0, float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("setSeed(0); abs(rexp(1) - c(0.206919)) < 0.00001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSeed(0); abs(rexp(3) - c(0.206919, 3.01675, 0.788416)) < 0.00001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("setSeed(1); abs(rexp(3, 10) - c(20.7, 12.2, 0.9)) < 0.1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("setSeed(2); abs(rexp(3, 100000) - c(95364.3, 307170.0, 74334.9)) < 0.1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptSuccess("setSeed(3); abs(rexp(3, c(10, 100, 1000)) - c(2.8, 64.6, 58.8)) < 0.1;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptRaise("rexp(-1);", 0, "requires n to be");
	EidosAssertScriptRaise("rexp(3, c(10, 5));", 0, "requires mu to be");
	EidosAssertScriptSuccess("rexp(1, NAN);", gStaticEidosValue_FloatNAN);
	
	// dgamma()
	EidosAssertScriptSuccess("dgamma(float(0), 0, 1000);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("dgamma(float(0), float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("dgamma(3.0, 0, 1000);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("abs(dgamma(0.1, 1/100, 1) - 0.004539993) < 0.0001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(dgamma(0.01, 1/100, 1) - 36.78794) < 0.0001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(dgamma(0.001, 1/100, 1) - 90.48374) < 0.0001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(dgamma(c(0.1, 0.01, 0.001), 1/100, 1) - c(0.004539993, 36.78794, 90.48374)) < 0.0001;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	EidosAssertScriptRaise("dgamma(2.0, 0, 0);", 0, "requires shape > 0.0");
	EidosAssertScriptRaise("dgamma(c(1.0, 2.0), 0, c(1.0, 0));", 0, "requires shape > 0.0");
	EidosAssertScriptRaise("dgamma(2.0, c(0.1, 10, 1), 10.0);", 0, "requires mean to be of length");
	EidosAssertScriptRaise("dgamma(2.0, 10.0, c(0.1, 10, 1));", 0, "requires shape to be of length");
	EidosAssertScriptSuccess("dgamma(NAN, 1/100, 1);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("dgamma(0.1, NAN, 1);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptRaise("dgamma(0.1, 1/100, NAN);", 0, "requires shape > 0.0");
	
	// rgamma()
	EidosAssertScriptSuccess("rgamma(0, 0, 1000);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rgamma(0, float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rgamma(3, 0, 1000);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{0.0, 0.0, 0.0}));
	EidosAssertScriptSuccess("setSeed(0); abs(rgamma(1, 1, 100) - c(1.02069)) < 0.0001;", gStaticEidosValue_LogicalT);
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
	EidosAssertScriptSuccess("x = rmvnorm(5, c(0,NAN), matrix(c(10,3,3,2), nrow=2)); all(!isNAN(x[,0])) & all(isNAN(x[,1]));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("rmvnorm(5, c(0,2), matrix(c(10,3,NAN,2), nrow=2));", 0, "to contain NANs");
	
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
	EidosAssertScriptSuccess("setSeed(0); abs(runif(1) - c(0.186915)) < 0.000001;", gStaticEidosValue_LogicalT);
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
	EidosAssertScriptSuccess("setSeed(0); abs(rweibull(1, 1, 1) - c(1.6771)) < 0.0001;", gStaticEidosValue_LogicalT);
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













































