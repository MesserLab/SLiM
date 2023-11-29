//
//  eidos_test_functions_statistics.cpp
//  Eidos
//
//  Created by Ben Haller on 7/11/20.
//  Copyright (c) 2020-2023 Philipp Messer.  All rights reserved.
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
void _RunFunctionStatisticsTests_a_through_p(void)
{
	// cor()
	EidosAssertScriptRaise("cor(T, T);", 0, "cannot be type");
	EidosAssertScriptSuccess_NULL("cor(3, 3);");
	EidosAssertScriptSuccess_NULL("cor(3.5, 3.5);");
	EidosAssertScriptRaise("cor('foo', 'foo');", 0, "cannot be type");
	EidosAssertScriptRaise("cor(c(F, F, T, F, T), c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess_L("abs(cor(1:5, 1:5) - 1) < 1e-10;", true);
	EidosAssertScriptRaise("cor(1:5, 1:4);", 0, "the same size");
	EidosAssertScriptSuccess_L("abs(cor(1:11, 1:11) - 1) < 1e-10;", true);
	EidosAssertScriptSuccess_L("abs(cor(1:5, 5:1) - -1) < 1e-10;", true);
	EidosAssertScriptSuccess_L("abs(cor(1:11, 11:1) - -1) < 1e-10;", true);
	EidosAssertScriptSuccess_L("abs(cor(1.0:5, 1:5) - 1) < 1e-10;", true);
	EidosAssertScriptSuccess_L("abs(cor(1:11, 1.0:11) - 1) < 1e-10;", true);
	EidosAssertScriptSuccess_L("abs(cor(1.0:5, 5.0:1) - -1) < 1e-10;", true);
	EidosAssertScriptSuccess_L("abs(cor(1.0:11, 11.0:1) - -1) < 1e-10;", true);
	EidosAssertScriptSuccess("cor(c(1.0, 2.0, NAN), c(8.0, 9.0, 10.0));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("cor(c(1.0, 2.0, 3.0), c(8.0, 9.0, NAN));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("cor(c(1.0, 2.0, NAN), c(8.0, 9.0, NAN));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptRaise("cor(c('foo', 'bar', 'baz'), c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("cor(_Test(7), _Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("cor(NULL, NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("cor(logical(0), logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess_NULL("cor(integer(0), integer(0));");
	EidosAssertScriptSuccess_NULL("cor(float(0), float(0));");
	EidosAssertScriptRaise("cor(string(0), string(0));", 0, "cannot be type");
	
	// cov()
	EidosAssertScriptRaise("cov(T, T);", 0, "cannot be type");
	EidosAssertScriptSuccess_NULL("cov(3, 3);");
	EidosAssertScriptSuccess_NULL("cov(3.5, 3.5);");
	EidosAssertScriptRaise("cov('foo', 'foo');", 0, "cannot be type");
	EidosAssertScriptRaise("cov(c(F, F, T, F, T), c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess_L("abs(cov(1:5, 1:5) - 2.5) < 1e-10;", true);
	EidosAssertScriptRaise("cov(1:5, 1:4);", 0, "the same size");
	EidosAssertScriptSuccess_L("abs(cov(1:11, 1:11) - 11) < 1e-10;", true);
	EidosAssertScriptSuccess_L("abs(cov(1:5, 5:1) - -2.5) < 1e-10;", true);
	EidosAssertScriptSuccess_L("abs(cov(1:11, 11:1) - -11) < 1e-10;", true);
	EidosAssertScriptSuccess_L("abs(cov(1.0:5, 1:5) - 2.5) < 1e-10;", true);
	EidosAssertScriptSuccess_L("abs(cov(1:11, 1.0:11) - 11) < 1e-10;", true);
	EidosAssertScriptSuccess_L("abs(cov(1.0:5, 5.0:1) - -2.5) < 1e-10;", true);
	EidosAssertScriptSuccess_L("abs(cov(1.0:11, 11.0:1) - -11) < 1e-10;", true);
	EidosAssertScriptSuccess("cov(c(1.0, 2.0, NAN), c(8.0, 9.0, 10.0));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("cov(c(1.0, 2.0, 3.0), c(8.0, 9.0, NAN));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("cov(c(1.0, 2.0, NAN), c(8.0, 9.0, NAN));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptRaise("cov(c('foo', 'bar', 'baz'), c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("cov(_Test(7), _Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("cov(NULL, NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("cov(logical(0), logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess_NULL("cov(integer(0), integer(0));");
	EidosAssertScriptSuccess_NULL("cov(float(0), float(0));");
	EidosAssertScriptRaise("cov(string(0), string(0));", 0, "cannot be type");
	
	// max()
	EidosAssertScriptSuccess_L("max(T);", true);
	EidosAssertScriptSuccess_I("max(3);", 3);
	EidosAssertScriptSuccess_F("max(3.5);", 3.5);
	EidosAssertScriptSuccess("max(NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess_S("max('foo');", "foo");
	EidosAssertScriptSuccess_L("max(c(F, F, F, F, F));", false);
	EidosAssertScriptSuccess_L("max(c(F, F, T, F, T));", true);
	EidosAssertScriptSuccess_I("max(c(3, 7, 19, -5, 9));", 19);
	EidosAssertScriptSuccess_F("max(c(3.3, 7.7, 19.1, -5.8, 9.0));", 19.1);
	EidosAssertScriptSuccess_S("max(c('bar', 'foo', 'baz'));", "foo");
	EidosAssertScriptRaise("max(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess_NULL("max(NULL);");
	EidosAssertScriptSuccess_NULL("max(logical(0));");
	EidosAssertScriptSuccess_NULL("max(integer(0));");
	EidosAssertScriptSuccess_NULL("max(float(0));");
	EidosAssertScriptSuccess_NULL("max(string(0));");
	EidosAssertScriptSuccess("max(c(1.0, 5.0, NAN, 2.0));", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess_L("max(F, T);", true);
	EidosAssertScriptSuccess_L("max(T, F);", true);
	EidosAssertScriptSuccess_L("max(F, c(F,F), logical(0), c(F,F,F,F,F));", false);
	EidosAssertScriptSuccess_L("max(F, c(F,F), logical(0), c(T,F,F,F,F));", true);
	EidosAssertScriptSuccess_I("max(1, 2);", 2);
	EidosAssertScriptSuccess_I("max(2, 1);", 2);
	EidosAssertScriptSuccess_I("max(integer(0), c(3,7,-8,0), 0, c(-10,10));", 10);
	EidosAssertScriptSuccess_F("max(1.0, 2.0);", 2.0);
	EidosAssertScriptSuccess_F("max(2.0, 1.0);", 2.0);
	EidosAssertScriptSuccess_F("max(c(3.,7.,-8.,0.), 0., c(-10.,0.), float(0));", 7);
	EidosAssertScriptRaise("max(c(3,7,-8,0), c(-10.,10.));", 0, "the same type");
	EidosAssertScriptSuccess_S("max('foo', 'bar');", "foo");
	EidosAssertScriptSuccess_S("max('bar', 'foo');", "foo");
	EidosAssertScriptSuccess_S("max('foo', string(0), c('baz','bar'), 'xyzzy', c('foobar', 'barbaz'));", "xyzzy");
	
	// mean()
	EidosAssertScriptSuccess_F("mean(T);", 1);
	EidosAssertScriptSuccess_F("mean(3);", 3);
	EidosAssertScriptSuccess_F("mean(3.5);", 3.5);
	EidosAssertScriptRaise("mean('foo');", 0, "cannot be type");
	EidosAssertScriptSuccess_F("mean(c(F, F, T, F, T));", 0.4);
	EidosAssertScriptSuccess_F("mean(c(3, 7, 19, -5, 16));", 8);
	EidosAssertScriptSuccess_F("mean(c(3.5, 7.25, 19.125, -5.5, 18.125));", 8.5);
	EidosAssertScriptRaise("mean(c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("mean(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("mean(NULL);", 0, "cannot be type");
	EidosAssertScriptSuccess_NULL("mean(logical(0));");
	EidosAssertScriptSuccess_NULL("mean(integer(0));");
	EidosAssertScriptSuccess_NULL("mean(float(0));");
	EidosAssertScriptRaise("mean(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess_F("mean(rep(1e18, 9));", 1e18);	// stays in integer internally
#if EIDOS_HAS_OVERFLOW_BUILTINS
	EidosAssertScriptSuccess_F("mean(rep(1e18, 10));", 1e18);	// overflows to float internally
#endif
	EidosAssertScriptSuccess("mean(c(1.0, 5.0, NAN, 2.0));", gStaticEidosValue_FloatNAN);
	
	// min()
	EidosAssertScriptSuccess_L("min(T);", true);
	EidosAssertScriptSuccess_I("min(3);", 3);
	EidosAssertScriptSuccess_F("min(3.5);", 3.5);
	EidosAssertScriptSuccess("min(NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess_S("min('foo');", "foo");
	EidosAssertScriptSuccess_L("min(c(T, F, T, F, T));", false);
	EidosAssertScriptSuccess_I("min(c(3, 7, 19, -5, 9));", -5);
	EidosAssertScriptSuccess_F("min(c(3.3, 7.7, 19.1, -5.8, 9.0));", -5.8);
	EidosAssertScriptSuccess_S("min(c('foo', 'bar', 'baz'));", "bar");
	EidosAssertScriptRaise("min(_Test(7));", 0, "cannot be type");
	EidosAssertScriptSuccess_NULL("min(NULL);");
	EidosAssertScriptSuccess_NULL("min(logical(0));");
	EidosAssertScriptSuccess_NULL("min(integer(0));");
	EidosAssertScriptSuccess_NULL("min(float(0));");
	EidosAssertScriptSuccess_NULL("min(string(0));");
	EidosAssertScriptSuccess("min(c(1.0, 5.0, NAN, 2.0));", gStaticEidosValue_FloatNAN);
	
	EidosAssertScriptSuccess_L("min(T, F);", false);
	EidosAssertScriptSuccess_L("min(F, T);", false);
	EidosAssertScriptSuccess_L("min(T, c(T,T), logical(0), c(T,T,T,T,T));", true);
	EidosAssertScriptSuccess_L("min(F, c(T,T), logical(0), c(T,T,T,T,T));", false);
	EidosAssertScriptSuccess_I("min(1, 2);", 1);
	EidosAssertScriptSuccess_I("min(2, 1);", 1);
	EidosAssertScriptSuccess_I("min(integer(0), c(3,7,-8,0), 0, c(-10,10));", -10);
	EidosAssertScriptSuccess_F("min(1.0, 2.0);", 1.0);
	EidosAssertScriptSuccess_F("min(2.0, 1.0);", 1.0);
	EidosAssertScriptSuccess_F("min(c(3.,7.,-8.,0.), 0., c(0.,10.), float(0));", -8);
	EidosAssertScriptRaise("min(c(3,7,-8,0), c(-10.,10.));", 0, "the same type");
	EidosAssertScriptSuccess_S("min('foo', 'bar');", "bar");
	EidosAssertScriptSuccess_S("min('bar', 'foo');", "bar");
	EidosAssertScriptSuccess_S("min('foo', string(0), c('baz','bar'), 'xyzzy', c('foobar', 'barbaz'));", "bar");
	
	// pmax()
	EidosAssertScriptRaise("pmax(c(T,T), logical(0));", 0, "of equal length");
	EidosAssertScriptRaise("pmax(logical(0), c(F,F));", 0, "of equal length");
	EidosAssertScriptSuccess("pmax(T, logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("pmax(logical(0), F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptRaise("pmax(T, 1);", 0, "to be the same type");
	EidosAssertScriptRaise("pmax(0, F);", 0, "to be the same type");
	EidosAssertScriptSuccess_NULL("pmax(NULL, NULL);");
	EidosAssertScriptSuccess_L("pmax(T, T);", true);
	EidosAssertScriptSuccess_L("pmax(F, T);", true);
	EidosAssertScriptSuccess_L("pmax(T, F);", true);
	EidosAssertScriptSuccess_L("pmax(F, F);", false);
	EidosAssertScriptSuccess_LV("pmax(c(T,F,T,F), c(T,T,F,F));", {true, true, true, false});
	EidosAssertScriptSuccess_I("pmax(1, 5);", 5);
	EidosAssertScriptSuccess_I("pmax(-8, 6);", 6);
	EidosAssertScriptSuccess_I("pmax(7, 1);", 7);
	EidosAssertScriptSuccess_I("pmax(8, -8);", 8);
	EidosAssertScriptSuccess_IV("pmax(c(1,-8,7,8), c(5,6,1,-8));", {5, 6, 7, 8});
	EidosAssertScriptSuccess_F("pmax(1., 5.);", 5);
	EidosAssertScriptSuccess_F("pmax(-INF, 6.);", 6);
	EidosAssertScriptSuccess_F("pmax(7., 1.);", 7);
	EidosAssertScriptSuccess("pmax(INF, -8.);", gStaticEidosValue_FloatINF);
	EidosAssertScriptSuccess("pmax(NAN, -8.);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmax(-8., NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmax(NAN, INF);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmax(INF, NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess_FV("pmax(c(1.,-INF,7.,INF,NAN,-8.,NAN), c(5.,6.,1.,-8.,-8.,NAN,INF));", {5, 6, 7, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
	EidosAssertScriptSuccess_S("pmax('foo', 'bar');", "foo");
	EidosAssertScriptSuccess_S("pmax('bar', 'baz');", "baz");
	EidosAssertScriptSuccess_S("pmax('xyzzy', 'xyzzy');", "xyzzy");
	EidosAssertScriptSuccess_S("pmax('', 'bar');", "bar");
	EidosAssertScriptSuccess_SV("pmax(c('foo','bar','xyzzy',''), c('bar','baz','xyzzy','bar'));", {"foo", "baz", "xyzzy", "bar"});
	
	EidosAssertScriptSuccess_LV("pmax(F, c(T,T,F,F));", {true, true, false, false});
	EidosAssertScriptSuccess_LV("pmax(c(T,F,T,F), T);", {true, true, true, true});
	EidosAssertScriptSuccess_IV("pmax(4, c(5,6,1,-8));", {5, 6, 4, 4});
	EidosAssertScriptSuccess_IV("pmax(c(1,-8,7,8), -2);", {1, -2, 7, 8});
	EidosAssertScriptSuccess_FV("pmax(4., c(5.,6.,1.,-8.,-8.,INF));", {5, 6, 4, 4, 4, std::numeric_limits<double>::infinity()});
	EidosAssertScriptSuccess_FV("pmax(c(1.,-INF,7.,INF, NAN, NAN), 5.);", {5, 5, 7, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
	EidosAssertScriptSuccess_SV("pmax('baz', c('bar','baz','xyzzy','bar'));", {"baz", "baz", "xyzzy", "baz"});
	EidosAssertScriptSuccess_SV("pmax(c('foo','bar','xyzzy',''), 'baz');", {"foo", "baz", "xyzzy", "baz"});
	
	EidosAssertScriptSuccess_L("identical(pmax(5, 3:7), c(5,5,5,6,7));", true);
	EidosAssertScriptSuccess_L("identical(pmax(3:7, 5), c(5,5,5,6,7));", true);
	EidosAssertScriptRaise("identical(pmax(matrix(5), 3:7), c(5,5,5,6,7));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(3:7, matrix(5)), c(5,5,5,6,7));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(array(5, c(1,1,1)), 3:7), c(5,5,5,6,7));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(3:7, array(5, c(1,1,1))), c(5,5,5,6,7));", 10, "the singleton is a vector");
	EidosAssertScriptSuccess_L("identical(pmax(5, matrix(3:7)), matrix(c(5,5,5,6,7)));", true);
	EidosAssertScriptSuccess_L("identical(pmax(matrix(3:7), 5), matrix(c(5,5,5,6,7)));", true);
	EidosAssertScriptSuccess_L("identical(pmax(5, array(3:7, c(1,5,1))), array(c(5,5,5,6,7), c(1,5,1)));", true);
	EidosAssertScriptSuccess_L("identical(pmax(array(3:7, c(1,5,1)), 5), array(c(5,5,5,6,7), c(1,5,1)));", true);
	EidosAssertScriptRaise("identical(pmax(1:5, matrix(3:7)), matrix(c(5,5,5,6,7)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmax(matrix(3:7), 1:5), matrix(c(5,5,5,6,7)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmax(1:5, array(3:7, c(1,5,1))), array(c(5,5,5,6,7), c(1,5,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmax(array(3:7, c(1,5,1)), 1:5), array(c(5,5,5,6,7), c(1,5,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmax(matrix(5), matrix(3:7)), matrix(c(5,5,5,6,7)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(matrix(3:7), matrix(5)), matrix(c(5,5,5,6,7)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(matrix(5), array(3:7, c(1,5,1))), array(c(5,5,5,6,7), c(1,5,1)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(array(3:7, c(1,5,1)), matrix(5)), array(c(5,5,5,6,7), c(1,5,1)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmax(matrix(5:1, nrow=1), matrix(1:5, ncol=1)), matrix(c(5,4,3,4,5)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptSuccess_L("identical(pmax(matrix(5:1, nrow=1), matrix(1:5, nrow=1)), matrix(c(5,4,3,4,5), nrow=1));", true);
	EidosAssertScriptRaise("identical(pmax(matrix(1:5), array(3:7, c(1,5,1))), array(c(5,5,5,6,7), c(1,5,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptSuccess_L("identical(pmax(array(5:1, c(1,5,1)), array(1:5, c(1,5,1))), array(c(5,4,3,4,5), c(1,5,1)));", true);
	
	// pmin()
	EidosAssertScriptRaise("pmin(c(T,T), logical(0));", 0, "of equal length");
	EidosAssertScriptRaise("pmin(logical(0), c(F,F));", 0, "of equal length");
	EidosAssertScriptSuccess("pmin(T, logical(0));", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptSuccess("pmin(logical(0), F);", gStaticEidosValue_Logical_ZeroVec);
	EidosAssertScriptRaise("pmin(T, 1);", 0, "to be the same type");
	EidosAssertScriptRaise("pmin(0, F);", 0, "to be the same type");
	EidosAssertScriptSuccess_NULL("pmin(NULL, NULL);");
	EidosAssertScriptSuccess_L("pmin(T, T);", true);
	EidosAssertScriptSuccess_L("pmin(F, T);", false);
	EidosAssertScriptSuccess_L("pmin(T, F);", false);
	EidosAssertScriptSuccess_L("pmin(F, F);", false);
	EidosAssertScriptSuccess_LV("pmin(c(T,F,T,F), c(T,T,F,F));", {true, false, false, false});
	EidosAssertScriptSuccess("pmin(1, 5);", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess_I("pmin(-8, 6);", -8);
	EidosAssertScriptSuccess("pmin(7, 1);", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess_I("pmin(8, -8);", -8);
	EidosAssertScriptSuccess_IV("pmin(c(1,-8,7,8), c(5,6,1,-8));", {1, -8, 1, -8});
	EidosAssertScriptSuccess_F("pmin(1., 5.);", 1);
	EidosAssertScriptSuccess_F("pmin(-INF, 6.);", (-std::numeric_limits<double>::infinity()));
	EidosAssertScriptSuccess_F("pmin(7., 1.);", 1);
	EidosAssertScriptSuccess_F("pmin(INF, -8.);", -8);
	EidosAssertScriptSuccess("pmin(NAN, -8.);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmin(-8., NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmin(NAN, INF);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("pmin(INF, NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess_FV("pmin(c(1.,-INF,7.,INF,NAN,-8.,NAN), c(5.,6.,1.,-8.,-8.,NAN,INF));", {1, -std::numeric_limits<double>::infinity(), 1, -8, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
	EidosAssertScriptSuccess_S("pmin('foo', 'bar');", "bar");
	EidosAssertScriptSuccess_S("pmin('bar', 'baz');", "bar");
	EidosAssertScriptSuccess_S("pmin('xyzzy', 'xyzzy');", "xyzzy");
	EidosAssertScriptSuccess("pmin('', 'bar');", gStaticEidosValue_StringEmpty);
	EidosAssertScriptSuccess_SV("pmin(c('foo','bar','xyzzy',''), c('bar','baz','xyzzy','bar'));", {"bar", "bar", "xyzzy", ""});
	
	EidosAssertScriptSuccess_LV("pmin(F, c(T,T,F,F));", {false, false, false, false});
	EidosAssertScriptSuccess_LV("pmin(c(T,F,T,F), T);", {true, false, true, false});
	EidosAssertScriptSuccess_IV("pmin(4, c(5,6,1,-8));", {4, 4, 1, -8});
	EidosAssertScriptSuccess_IV("pmin(c(1,-8,7,8), -2);", {-2, -8, -2, -2});
	EidosAssertScriptSuccess_FV("pmin(4., c(5.,6.,1.,-8.,-8.,INF));", {4, 4, 1, -8, -8, 4});
	EidosAssertScriptSuccess_FV("pmin(c(1.,-INF,7.,INF, NAN, NAN), 5.);", {1, -std::numeric_limits<double>::infinity(), 5, 5, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
	EidosAssertScriptSuccess_SV("pmin('baz', c('bar','baz','xyzzy','bar'));", {"bar", "baz", "baz", "bar"});
	EidosAssertScriptSuccess_SV("pmin(c('foo','bar','xyzzy',''), 'baz');", {"baz", "bar", "baz", ""});
	
	EidosAssertScriptSuccess_L("identical(pmin(5, 3:7), c(3,4,5,5,5));", true);
	EidosAssertScriptSuccess_L("identical(pmin(3:7, 5), c(3,4,5,5,5));", true);
	EidosAssertScriptRaise("identical(pmin(matrix(5), 3:7), c(3,4,5,5,5));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(3:7, matrix(5)), c(3,4,5,5,5));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(array(5, c(1,1,1)), 3:7), c(3,4,5,5,5));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(3:7, array(5, c(1,1,1))), c(3,4,5,5,5));", 10, "the singleton is a vector");
	EidosAssertScriptSuccess_L("identical(pmin(5, matrix(3:7)), matrix(c(3,4,5,5,5)));", true);
	EidosAssertScriptSuccess_L("identical(pmin(matrix(3:7), 5), matrix(c(3,4,5,5,5)));", true);
	EidosAssertScriptSuccess_L("identical(pmin(5, array(3:7, c(1,5,1))), array(c(3,4,5,5,5), c(1,5,1)));", true);
	EidosAssertScriptSuccess_L("identical(pmin(array(3:7, c(1,5,1)), 5), array(c(3,4,5,5,5), c(1,5,1)));", true);
	EidosAssertScriptRaise("identical(pmin(1:5, matrix(3:7)), matrix(c(3,4,5,5,5)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmin(matrix(3:7), 1:5), matrix(c(3,4,5,5,5)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmin(1:5, array(3:7, c(1,5,1))), array(c(3,4,5,5,5), c(1,5,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmin(array(3:7, c(1,5,1)), 1:5), array(c(3,4,5,5,5), c(1,5,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptRaise("identical(pmin(matrix(5), matrix(3:7)), matrix(c(3,4,5,5,5)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(matrix(3:7), matrix(5)), matrix(c(3,4,5,5,5)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(matrix(5), array(3:7, c(1,5,1))), array(c(3,4,5,5,5), c(1,5,1)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(array(3:7, c(1,5,1)), matrix(5)), array(c(3,4,5,5,5), c(1,5,1)));", 10, "the singleton is a vector");
	EidosAssertScriptRaise("identical(pmin(matrix(5:1, nrow=1), matrix(1:5, ncol=1)), matrix(c(1,2,3,2,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptSuccess_L("identical(pmin(matrix(5:1, nrow=1), matrix(1:5, nrow=1)), matrix(c(1,2,3,2,1), nrow=1));", true);
	EidosAssertScriptRaise("identical(pmin(matrix(1:5), array(3:7, c(1,5,1))), array(c(3,4,5,5,5), c(1,5,1)));", 10, "same vector/matrix/array dimensions");
	EidosAssertScriptSuccess_L("identical(pmin(array(5:1, c(1,5,1)), array(1:5, c(1,5,1))), array(c(1,2,3,2,1), c(1,5,1)));", true);
}

void _RunFunctionStatisticsTests_q_through_z(void)
{
	// quantile()
	EidosAssertScriptRaise("quantile(integer(0));", 0, "x to have length greater than 0");
	EidosAssertScriptRaise("quantile(float(0));", 0, "x to have length greater than 0");
	EidosAssertScriptSuccess_F("quantile(INF, 0.5);", (std::numeric_limits<double>::infinity()));
	EidosAssertScriptSuccess_F("quantile(-INF, 0.5);", (-std::numeric_limits<double>::infinity()));
	EidosAssertScriptSuccess_FV("quantile(0);", {0.0, 0.0, 0.0, 0.0, 0.0});
	EidosAssertScriptSuccess_FV("quantile(1);", {1.0, 1.0, 1.0, 1.0, 1.0});
	EidosAssertScriptRaise("quantile(integer(0), float(0));", 0, "x to have length greater than 0");
	EidosAssertScriptSuccess("quantile(0, float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("quantile(1, float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("quantile(1, -0.0000001);", 0, "requires probabilities to be in [0, 1]");
	EidosAssertScriptRaise("quantile(1, 1.0000001);", 0, "requires probabilities to be in [0, 1]");
	EidosAssertScriptRaise("quantile(NAN);", 0, "quantiles of NAN are undefined");
	EidosAssertScriptRaise("quantile(c(-5, 7, 2, NAN, 9));", 0, "quantiles of NAN are undefined");
	EidosAssertScriptRaise("quantile(c(-5, 7, 2, 8, 9), -0.0000001);", 0, "requires probabilities to be in [0, 1]");
	EidosAssertScriptRaise("quantile(c(-5, 7, 2, 8, 9), 1.0000001);", 0, "requires probabilities to be in [0, 1]");
	EidosAssertScriptSuccess_FV("quantile(0:100);", {0, 25, 50, 75, 100});
	EidosAssertScriptSuccess_F("quantile(0:100, 0.27);", 27);
	EidosAssertScriptSuccess_FV("quantile(0:100, c(0.8, 0.3, 0.72, 0.0, 0.67));", {80, 30, 72, 0, 67});
	EidosAssertScriptSuccess_FV("quantile(0:10, c(0.15, 0.25, 0.5, 0.82));", {1.5, 2.5, 5.0, 8.2});
	EidosAssertScriptSuccess_FV("quantile(10:0, c(0.15, 0.25, 0.5, 0.82));", {1.5, 2.5, 5.0, 8.2});
	EidosAssertScriptSuccess_FV("quantile(c(17, 12, 4, 87, 3, 1081, 273));", {3, 8, 17, 180, 1081});
	EidosAssertScriptSuccess_FV("quantile(0.0:100);", {0, 25, 50, 75, 100});
	EidosAssertScriptSuccess_F("quantile(0.0:100, 0.27);", 27);
	
	// range()
	EidosAssertScriptRaise("range(T);", 0, "cannot be type");
	EidosAssertScriptSuccess_IV("range(3);", {3, 3});
	EidosAssertScriptSuccess_FV("range(3.5);", {3.5, 3.5});
	EidosAssertScriptRaise("range('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("range(c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess_IV("range(c(3, 7, 19, -5, 9));", {-5, 19});
	EidosAssertScriptSuccess_FV("range(c(3.3, 7.7, 19.1, -5.8, 9.0));", {-5.8, 19.1});
	EidosAssertScriptRaise("range(c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("range(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("range(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("range(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess_NULL("range(integer(0));");
	EidosAssertScriptSuccess_NULL("range(float(0));");
	EidosAssertScriptRaise("range(string(0));", 0, "cannot be type");
	EidosAssertScriptSuccess_FV("range(NAN);", {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
	EidosAssertScriptSuccess_FV("range(c(1.0, 5.0, NAN, 2.0));", {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
	
	EidosAssertScriptSuccess_IV("range(integer(0), c(3,7,-8,0), 0, c(-10,10));", {-10,10});
	EidosAssertScriptSuccess_FV("range(c(3.,7.,-8.,0.), 0., c(0.,10.), float(0));", {-8,10});
	EidosAssertScriptRaise("range(c(3,7,-8,0), c(-10.,10.));", 0, "the same type");
	
	// rank()
	EidosAssertScriptSuccess("rank(integer(0));", gStaticEidosValue_Float_ZeroVec);		// 'average' is the default
	EidosAssertScriptSuccess("rank(integer(0), 'average');", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rank(integer(0), 'first');", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("rank(integer(0), 'last');", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptRaise("rank(integer(0), 'random');", 0, "not currently supported");
	EidosAssertScriptSuccess("rank(integer(0), 'max');", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("rank(integer(0), 'min');", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptRaise("rank(integer(0), 'invalid');", 0, "requires tiesMethod to be");
	
	EidosAssertScriptSuccess("rank(float(0));", gStaticEidosValue_Float_ZeroVec);		// 'average' is the default
	EidosAssertScriptSuccess("rank(float(0), 'average');", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rank(float(0), 'first');", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("rank(float(0), 'last');", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptRaise("rank(float(0), 'random');", 0, "not currently supported");
	EidosAssertScriptSuccess("rank(float(0), 'max');", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("rank(float(0), 'min');", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptRaise("rank(float(0), 'invalid');", 0, "requires tiesMethod to be");
	
	EidosAssertScriptSuccess_F("rank(3);", 1.0);
	EidosAssertScriptSuccess_F("rank(3, 'average');", 1.0);
	EidosAssertScriptSuccess_I("rank(3, 'first');", 1);
	EidosAssertScriptSuccess_I("rank(3, 'last');", 1);
	EidosAssertScriptRaise("rank(3, 'random');", 0, "not currently supported");
	EidosAssertScriptSuccess_I("rank(3, 'max');", 1);
	EidosAssertScriptSuccess_I("rank(3, 'min');", 1);
	
	EidosAssertScriptSuccess_F("rank(3.5);", 1.0);
	EidosAssertScriptSuccess_F("rank(3.5, 'average');", 1.0);
	EidosAssertScriptSuccess_I("rank(3.5, 'first');", 1);
	EidosAssertScriptSuccess_I("rank(3.5, 'last');", 1);
	EidosAssertScriptRaise("rank(3.5, 'random');", 0, "not currently supported");
	EidosAssertScriptSuccess_I("rank(3.5, 'max');", 1);
	EidosAssertScriptSuccess_I("rank(3.5, 'min');", 1);
	
	EidosAssertScriptSuccess_FV("rank(c(0, 20, 10, 15));", {1.0, 4.0, 2.0, 3.0});
	EidosAssertScriptSuccess_FV("rank(c(0, 20, 10, 15), 'average');", {1.0, 4.0, 2.0, 3.0});
	EidosAssertScriptSuccess_IV("rank(c(0, 20, 10, 15), 'first');", {1, 4, 2, 3});
	EidosAssertScriptSuccess_IV("rank(c(0, 20, 10, 15), 'last');", {1, 4, 2, 3});
	EidosAssertScriptRaise("rank(c(0, 20, 10, 15), 'random');", 0, "not currently supported");
	EidosAssertScriptSuccess_IV("rank(c(0, 20, 10, 15), 'max');", {1, 4, 2, 3});
	EidosAssertScriptSuccess_IV("rank(c(0, 20, 10, 15), 'min');", {1, 4, 2, 3});
	
	EidosAssertScriptSuccess_FV("rank(c(0.5, 20.5, 10.5, 15.5));", {1.0, 4.0, 2.0, 3.0});
	EidosAssertScriptSuccess_FV("rank(c(0.5, 20.5, 10.5, 15.5), 'average');", {1.0, 4.0, 2.0, 3.0});
	EidosAssertScriptSuccess_IV("rank(c(0.5, 20.5, 10.5, 15.5), 'first');", {1, 4, 2, 3});
	EidosAssertScriptSuccess_IV("rank(c(0.5, 20.5, 10.5, 15.5), 'last');", {1, 4, 2, 3});
	EidosAssertScriptRaise("rank(c(0.5, 20.5, 10.5, 15.5), 'random');", 0, "not currently supported");
	EidosAssertScriptSuccess_IV("rank(c(0.5, 20.5, 10.5, 15.5), 'max');", {1, 4, 2, 3});
	EidosAssertScriptSuccess_IV("rank(c(0.5, 20.5, 10.5, 15.5), 'min');", {1, 4, 2, 3});
	
	EidosAssertScriptSuccess_FV("rank(c(10, 12, 15, 12, 10, 25, 12));", {1.5, 4.0, 6.0, 4.0, 1.5, 7.0, 4.0});
	EidosAssertScriptSuccess_FV("rank(c(10, 12, 15, 12, 10, 25, 12), 'average');", {1.5, 4.0, 6.0, 4.0, 1.5, 7.0, 4.0});
	EidosAssertScriptSuccess_IV("rank(c(10, 12, 15, 12, 10, 25, 12), 'first');", {1, 3, 6, 4, 2, 7, 5});
	EidosAssertScriptSuccess_IV("rank(c(10, 12, 15, 12, 10, 25, 12), 'last');", {2, 5, 6, 4, 1, 7, 3});
	EidosAssertScriptRaise("rank(c(10, 12, 15, 12, 10, 25, 12), 'random');", 0, "not currently supported");
	EidosAssertScriptSuccess_IV("rank(c(10, 12, 15, 12, 10, 25, 12), 'max');", {2, 5, 6, 5, 2, 7, 5});
	EidosAssertScriptSuccess_IV("rank(c(10, 12, 15, 12, 10, 25, 12), 'min');", {1, 3, 6, 3, 1, 7, 3});
	
	EidosAssertScriptSuccess_FV("rank(c(10.5, 12.5, 15.5, 12.5, 10.5, 25.5, 12.5));", {1.5, 4.0, 6.0, 4.0, 1.5, 7.0, 4.0});
	EidosAssertScriptSuccess_FV("rank(c(10.5, 12.5, 15.5, 12.5, 10.5, 25.5, 12.5), 'average');", {1.5, 4.0, 6.0, 4.0, 1.5, 7.0, 4.0});
	EidosAssertScriptSuccess_IV("rank(c(10.5, 12.5, 15.5, 12.5, 10.5, 25.5, 12.5), 'first');", {1, 3, 6, 4, 2, 7, 5});
	EidosAssertScriptSuccess_IV("rank(c(10.5, 12.5, 15.5, 12.5, 10.5, 25.5, 12.5), 'last');", {2, 5, 6, 4, 1, 7, 3});
	EidosAssertScriptRaise("rank(c(10.5, 12.5, 15.5, 12.5, 10.5, 25.5, 12.5), 'random');", 0, "not currently supported");
	EidosAssertScriptSuccess_IV("rank(c(10.5, 12.5, 15.5, 12.5, 10.5, 25.5, 12.5), 'max');", {2, 5, 6, 5, 2, 7, 5});
	EidosAssertScriptSuccess_IV("rank(c(10.5, 12.5, 15.5, 12.5, 10.5, 25.5, 12.5), 'min');", {1, 3, 6, 3, 1, 7, 3});
	
	EidosAssertScriptSuccess_FV("rank(c(4, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3));", {14.0, 5.0, 14.0, 9.5, 19.0, 9.5, 9.5, 9.5, 2.5, 9.5, 17.5, 5.0, 16.0, 5.0, 2.5, 17.5, 14.0, 1.0, 20.0, 9.5});
	EidosAssertScriptSuccess_FV("rank(c(4, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3), 'average');", {14.0, 5.0, 14.0, 9.5, 19.0, 9.5, 9.5, 9.5, 2.5, 9.5, 17.5, 5.0, 16.0, 5.0, 2.5, 17.5, 14.0, 1.0, 20.0, 9.5});
	EidosAssertScriptSuccess_IV("rank(c(4, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3), 'first');", {13, 4, 14, 7, 19, 8, 9, 10, 2, 11, 17, 5, 16, 6, 3, 18, 15, 1, 20, 12});
	EidosAssertScriptSuccess_IV("rank(c(4, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3), 'last');", {15, 6, 14, 12, 19, 11, 10, 9, 3, 8, 18, 5, 16, 4, 2, 17, 13, 1, 20, 7});
	EidosAssertScriptRaise("rank(c(4, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3), 'random');", 0, "not currently supported");
	EidosAssertScriptSuccess_IV("rank(c(4, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3), 'max');", {15, 6, 15, 12, 19, 12, 12, 12, 3, 12, 18, 6, 16, 6, 3, 18, 15, 1, 20, 12});
	EidosAssertScriptSuccess_IV("rank(c(4, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3), 'min');", {13, 4, 13, 7, 19, 7, 7, 7, 2, 7, 17, 4, 16, 4, 2, 17, 13, 1, 20, 7});
	
	EidosAssertScriptSuccess_FV("rank(c(4.0, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3));", {14.0, 5.0, 14.0, 9.5, 19.0, 9.5, 9.5, 9.5, 2.5, 9.5, 17.5, 5.0, 16.0, 5.0, 2.5, 17.5, 14.0, 1.0, 20.0, 9.5});
	EidosAssertScriptSuccess_FV("rank(c(4.0, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3), 'average');", {14.0, 5.0, 14.0, 9.5, 19.0, 9.5, 9.5, 9.5, 2.5, 9.5, 17.5, 5.0, 16.0, 5.0, 2.5, 17.5, 14.0, 1.0, 20.0, 9.5});
	EidosAssertScriptSuccess_IV("rank(c(4.0, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3), 'first');", {13, 4, 14, 7, 19, 8, 9, 10, 2, 11, 17, 5, 16, 6, 3, 18, 15, 1, 20, 12});
	EidosAssertScriptSuccess_IV("rank(c(4.0, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3), 'last');", {15, 6, 14, 12, 19, 11, 10, 9, 3, 8, 18, 5, 16, 4, 2, 17, 13, 1, 20, 7});
	EidosAssertScriptRaise("rank(c(4.0, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3), 'random');", 0, "not currently supported");
	EidosAssertScriptSuccess_IV("rank(c(4.0, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3), 'max');", {15, 6, 15, 12, 19, 12, 12, 12, 3, 12, 18, 6, 16, 6, 3, 18, 15, 1, 20, 12});
	EidosAssertScriptSuccess_IV("rank(c(4.0, 2, 4, 3, 7, 3, 3, 3, 1, 3, 6, 2, 5, 2, 1, 6, 4, 0, 9, 3), 'min');", {13, 4, 13, 7, 19, 7, 7, 7, 2, 7, 17, 4, 16, 4, 2, 17, 13, 1, 20, 7});
	
	EidosAssertScriptRaise("rank(c(T, F));", 0, "cannot be type logical");						// logical not supported, unlike R
	EidosAssertScriptRaise("rank(c('a', 'q', 'm', 'f', 'w'));", 0, "cannot be type string");	// string not supported, unlike R
	
	/*EidosAssertScriptSuccess_L("x = c(5, 0, NAN, 17, NAN, -17); o = rank(x); identical(o, c(5, 1, 0, 3, 2, 4)) | identical(o, c(5, 1, 0, 3, 4, 2));", true);
	EidosAssertScriptSuccess_L("x = c(5, 0, NAN, 17, NAN, -17); o = rank(x, ascending=T); identical(o, c(5, 1, 0, 3, 2, 4)) | identical(o, c(5, 1, 0, 3, 4, 2));", true);
	EidosAssertScriptSuccess_L("x = c(5, 0, NAN, 17, NAN, -17); o = rank(x, ascending=F); identical(o, c(3, 0, 1, 5, 2, 4)) | identical(o, c(3, 0, 1, 5, 4, 2));", true);*/
	
	// sd()
	EidosAssertScriptRaise("sd(T);", 0, "cannot be type");
	EidosAssertScriptSuccess_NULL("sd(3);");
	EidosAssertScriptSuccess_NULL("sd(3.5);");
	EidosAssertScriptRaise("sd('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("sd(c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess_F("sd(c(2, 3, 2, 8, 0));", 3);
	EidosAssertScriptSuccess_F("sd(c(9.1, 5.1, 5.1, 4.1, 7.1));", 2.0);
	EidosAssertScriptSuccess("sd(c(9.1, 5.1, 5.1, NAN, 7.1));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptRaise("sd(c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("sd(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("sd(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("sd(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess_NULL("sd(integer(0));");
	EidosAssertScriptSuccess_NULL("sd(float(0));");
	EidosAssertScriptRaise("sd(string(0));", 0, "cannot be type");
	
	// ttest()
	EidosAssertScriptRaise("ttest(1:5.0);", 0, "either y or mu to be non-NULL");
	EidosAssertScriptRaise("ttest(1:5.0, 1:5.0, 5.0);", 0, "either y or mu to be NULL");
	EidosAssertScriptRaise("ttest(5.0, 1:5.0);", 0, "enough elements in x");
	EidosAssertScriptRaise("ttest(1:5.0, 5.0);", 0, "enough elements in y");
	EidosAssertScriptRaise("ttest(5.0, mu=6.0);", 0, "enough elements in x");
	EidosAssertScriptSuccess_L("abs(ttest(1:50.0, 1:50.0) - 1.0) < 0.001;", true);
	EidosAssertScriptSuccess_L("abs(ttest(1:50.0, 1:60.0) - 0.101496) < 0.001;", true);			// R gives 0.1046, not sure why but I suspect corrected vs. uncorrected standard deviations
	EidosAssertScriptSuccess_L("abs(ttest(1:50.0, 10.0:60.0) - 0.00145575) < 0.001;", true);	// R gives 0.001615
	EidosAssertScriptSuccess_L("abs(ttest(1:50.0, mu=25.0) - 0.807481) < 0.001;", true);		// R gives 0.8094
	EidosAssertScriptSuccess_L("abs(ttest(1:50.0, mu=30.0) - 0.0321796) < 0.001;", true);		// R gives 0.03387
	EidosAssertScriptSuccess("ttest(c(1.0, 2.0, NAN), mu=25.0);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("ttest(c(1.0, 2.0, NAN), c(8.0, 9.0, 10.0));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("ttest(c(1.0, 2.0, 3,0), c(8.0, 9.0, NAN));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("ttest(c(1.0, 2.0, NAN), c(8.0, 9.0, NAN));", gStaticEidosValue_FloatNAN);
	
	// var()
	EidosAssertScriptRaise("var(T);", 0, "cannot be type");
	EidosAssertScriptSuccess_NULL("var(3);");
	EidosAssertScriptSuccess_NULL("var(3.5);");
	EidosAssertScriptRaise("var('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("var(c(F, F, T, F, T));", 0, "cannot be type");
	EidosAssertScriptSuccess_F("var(c(2, 3, 2, 8, 0));", 9);
	EidosAssertScriptSuccess_F("var(c(9.1, 5.1, 5.1, 4.1, 7.1));", 4.0);
	EidosAssertScriptSuccess("var(c(9.1, 5.1, 5.1, NAN, 7.1));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptRaise("var(c('foo', 'bar', 'baz'));", 0, "cannot be type");
	EidosAssertScriptRaise("var(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("var(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("var(logical(0));", 0, "cannot be type");
	EidosAssertScriptSuccess_NULL("var(integer(0));");
	EidosAssertScriptSuccess_NULL("var(float(0));");
	EidosAssertScriptRaise("var(string(0));", 0, "cannot be type");
}

#pragma mark distributions
void _RunFunctionDistributionTests(void)
{
	// findInterval() - note results are 1 less than in R, due to zero-basing vs. 1-basing of indices
	EidosAssertScriptRaise("findInterval(c(-1,0,1,9,10,11), integer(0));", 0, "vec to be of length > 0");
	EidosAssertScriptRaise("findInterval(c(-1,0,1,9,10,11), float(0));", 0, "vec to be of length > 0");
	EidosAssertScriptRaise("findInterval(c(-1,0,1,9,10,11), c(0:10,9));", 0, "non-decreasing order");
	EidosAssertScriptRaise("findInterval(c(-1,0,1,9,10,11), c(1,0:10));", 0, "non-decreasing order");
	EidosAssertScriptRaise("findInterval(c(-1,0,1,9,10,11), c(0:10.0,9));", 0, "non-decreasing order");
	EidosAssertScriptRaise("findInterval(c(-1,0,1,9,10,11), c(1.0,0:10));", 0, "non-decreasing order");
	
	EidosAssertScriptSuccess_I("findInterval(3, 3);", 0);
	EidosAssertScriptSuccess_I("findInterval(3, 3, rightmostClosed=T);", -1);
	EidosAssertScriptSuccess_I("findInterval(3, 3, allInside=T);", -1);
	EidosAssertScriptSuccess_I("findInterval(3, 3, rightmostClosed=T, allInside=T);", -1);
	EidosAssertScriptSuccess_IV("findInterval(0:5, 3);", {-1, -1, -1, 0, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(0:5, 3, rightmostClosed=T);", {-1, -1, -1, -1, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(0:5, 3, allInside=T);", {0, 0, 0, -1, -1, -1});
	EidosAssertScriptSuccess_IV("findInterval(0:5, 3, rightmostClosed=T, allInside=T);", {0, 0, 0, -1, -1, -1});
	
	EidosAssertScriptSuccess_I("findInterval(3.0, 3);", 0);
	EidosAssertScriptSuccess_I("findInterval(3.0, 3, rightmostClosed=T);", -1);
	EidosAssertScriptSuccess_I("findInterval(3.0, 3, allInside=T);", -1);
	EidosAssertScriptSuccess_I("findInterval(3.0, 3, rightmostClosed=T, allInside=T);", -1);
	EidosAssertScriptSuccess_IV("findInterval(0.0:5, 3);", {-1, -1, -1, 0, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(0.0:5, 3, rightmostClosed=T);", {-1, -1, -1, -1, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(0.0:5, 3, allInside=T);", {0, 0, 0, -1, -1, -1});
	EidosAssertScriptSuccess_IV("findInterval(0.0:5, 3, rightmostClosed=T, allInside=T);", {0, 0, 0, -1, -1, -1});
	
	EidosAssertScriptSuccess_I("findInterval(3, 3.0);", 0);
	EidosAssertScriptSuccess_I("findInterval(3, 3.0, rightmostClosed=T);", -1);
	EidosAssertScriptSuccess_I("findInterval(3, 3.0, allInside=T);", -1);
	EidosAssertScriptSuccess_I("findInterval(3, 3.0, rightmostClosed=T, allInside=T);", -1);
	EidosAssertScriptSuccess_IV("findInterval(0:5, 3.0);", {-1, -1, -1, 0, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(0:5, 3.0, rightmostClosed=T);", {-1, -1, -1, -1, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(0:5, 3.0, allInside=T);", {0, 0, 0, -1, -1, -1});
	EidosAssertScriptSuccess_IV("findInterval(0:5, 3.0, rightmostClosed=T, allInside=T);", {0, 0, 0, -1, -1, -1});
	
	EidosAssertScriptSuccess_I("findInterval(3.0, 3.0);", 0);
	EidosAssertScriptSuccess_I("findInterval(3.0, 3.0, rightmostClosed=T);", -1);
	EidosAssertScriptSuccess_I("findInterval(3.0, 3.0, allInside=T);", -1);
	EidosAssertScriptSuccess_I("findInterval(3.0, 3.0, rightmostClosed=T, allInside=T);", -1);
	EidosAssertScriptSuccess_IV("findInterval(0.0:5, 3.0);", {-1, -1, -1, 0, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(0.0:5, 3.0, rightmostClosed=T);", {-1, -1, -1, -1, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(0.0:5, 3.0, allInside=T);", {0, 0, 0, -1, -1, -1});
	EidosAssertScriptSuccess_IV("findInterval(0.0:5, 3.0, rightmostClosed=T, allInside=T);", {0, 0, 0, -1, -1, -1});
	
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11), 0:10);", {-1, 0, 1, 9, 10, 10});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11), 0:10, rightmostClosed=T);", {-1, 0, 1, 9, 9, 10});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11), 0:10, allInside=T);", {0, 0, 1, 9, 9, 9});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11), 0:10, rightmostClosed=T, allInside=T);", {0, 0, 1, 9, 9, 9});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11), repEach(0:10, 2));", {-1, 1, 3, 19, 21, 21});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1), 0:10);", {10, 10, 9, 1, 0, -1});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1), 0:10, rightmostClosed=T);", {10, 9, 9, 1, 0, -1});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1), 0:10, allInside=T);", {9, 9, 9, 1, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1), 0:10, rightmostClosed=T, allInside=T);", {9, 9, 9, 1, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1), repEach(0:10, 2));", {21, 21, 19, 3, 1, -1});
	
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11.0), 0:10.0);", {-1, 0, 1, 9, 10, 10});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11.0), 0:10.0, rightmostClosed=T);", {-1, 0, 1, 9, 9, 10});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11.0), 0:10.0, allInside=T);", {0, 0, 1, 9, 9, 9});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11.0), 0:10.0, rightmostClosed=T, allInside=T);", {0, 0, 1, 9, 9, 9});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11.0), repEach(0:10.0, 2));", {-1, 1, 3, 19, 21, 21});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1.0), 0:10.0);", {10, 10, 9, 1, 0, -1});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1.0), 0:10.0, rightmostClosed=T);", {10, 9, 9, 1, 0, -1});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1.0), 0:10.0, allInside=T);", {9, 9, 9, 1, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1.0), 0:10.0, rightmostClosed=T, allInside=T);", {9, 9, 9, 1, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1.0), repEach(0:10.0, 2));", {21, 21, 19, 3, 1, -1});
	
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11.0), 0:10);", {-1, 0, 1, 9, 10, 10});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11.0), 0:10, rightmostClosed=T);", {-1, 0, 1, 9, 9, 10});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11.0), 0:10, allInside=T);", {0, 0, 1, 9, 9, 9});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11.0), 0:10, rightmostClosed=T, allInside=T);", {0, 0, 1, 9, 9, 9});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11.0), repEach(0:10, 2));", {-1, 1, 3, 19, 21, 21});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1.0), 0:10);", {10, 10, 9, 1, 0, -1});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1.0), 0:10, rightmostClosed=T);", {10, 9, 9, 1, 0, -1});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1.0), 0:10, allInside=T);", {9, 9, 9, 1, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1.0), 0:10, rightmostClosed=T, allInside=T);", {9, 9, 9, 1, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1.0), repEach(0:10, 2));", {21, 21, 19, 3, 1, -1});
	
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11), 0:10.0);", {-1, 0, 1, 9, 10, 10});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11), 0:10.0, rightmostClosed=T);", {-1, 0, 1, 9, 9, 10});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11), 0:10.0, allInside=T);", {0, 0, 1, 9, 9, 9});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11), 0:10.0, rightmostClosed=T, allInside=T);", {0, 0, 1, 9, 9, 9});
	EidosAssertScriptSuccess_IV("findInterval(c(-1,0,1,9,10,11), repEach(0:10.0, 2));", {-1, 1, 3, 19, 21, 21});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1), 0:10.0);", {10, 10, 9, 1, 0, -1});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1), 0:10.0, rightmostClosed=T);", {10, 9, 9, 1, 0, -1});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1), 0:10.0, allInside=T);", {9, 9, 9, 1, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1), 0:10.0, rightmostClosed=T, allInside=T);", {9, 9, 9, 1, 0, 0});
	EidosAssertScriptSuccess_IV("findInterval(c(11,10,9,1,0,-1), repEach(0:10.0, 2));", {21, 21, 19, 3, 1, -1});
	
	// dmvnorm()
	EidosAssertScriptRaise("dmvnorm(array(c(1.0,2,3,2,1), c(1,5,1)), c(0.0, 2.0), matrix(c(10,3,3,2), nrow=2));", 0, "requires x to be");
	EidosAssertScriptSuccess("dmvnorm(float(0), c(0.0, 2.0), matrix(c(10,3,3,2), nrow=2));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptRaise("dmvnorm(3.0, c(0.0, 2.0), matrix(c(10,3,3,2), nrow=2));", 0, "dimensionality of >= 2");
	EidosAssertScriptRaise("dmvnorm(1.0:3.0, c(0.0, 2.0), matrix(c(10,3,3,2), nrow=2));", 0, "matching the dimensionality");
	EidosAssertScriptRaise("dmvnorm(c(0.0, 2.0), c(0.0, 2.0), c(10,3,3,2));", 0, "sigma to be a matrix");
	EidosAssertScriptRaise("dmvnorm(c(0.0, 2.0), c(0.0, 2.0, 3.0), matrix(c(10,3,3,2), nrow=2));", 0, "matching the dimensionality");
	EidosAssertScriptRaise("dmvnorm(c(0.0, 2.0), c(0.0, 2.0), matrix(c(10,3,3,2,4,8), nrow=3));", 0, "matching the dimensionality");
	EidosAssertScriptRaise("abs(dmvnorm(c(0.0, 2.0), c(0.0, 2.0), matrix(c(0,0,0,0), nrow=2)) - 0.047987) < 0.00001;", 4, "positive-definite");
	EidosAssertScriptSuccess_L("abs(dmvnorm(c(0.0, 2.0), c(0.0, 2.0), matrix(c(10,3,3,2), nrow=2)) - 0.047987) < 0.00001;", true);
	EidosAssertScriptSuccess("dmvnorm(c(NAN, 2.0), c(0.0, 2.0), matrix(c(10,3,3,2), nrow=2));", gStaticEidosValue_FloatNAN);
	EidosAssertScriptRaise("dmvnorm(c(0.0, 2.0), c(0.0, 2.0), matrix(c(10,3,NAN,2), nrow=2));", 0, "to contain NANs");
	
	// dnorm()
	EidosAssertScriptSuccess("dnorm(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("dnorm(float(0), float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess_L("dnorm(0.0, 0, 1) - 0.3989423 < 0.00001;", true);
	EidosAssertScriptSuccess_L("dnorm(1.0, 1.0, 1.0) - 0.3989423 < 0.00001;", true);
	EidosAssertScriptSuccess_LV("dnorm(c(0.0,0.0), c(0,0), 1) - 0.3989423 < 0.00001;", {true, true});
	EidosAssertScriptSuccess_LV("dnorm(c(0.0,1.0), c(0.0,1.0), 1.0) - 0.3989423 < 0.00001;", {true, true});
	EidosAssertScriptSuccess_LV("dnorm(c(0.0,0.0), 0.0, c(1.0,1.0)) - 0.3989423 < 0.00001;", {true, true});
	EidosAssertScriptSuccess_LV("dnorm(c(-1.0,0.0,1.0)) - c(0.2419707,0.3989423,0.2419707) < 0.00001;", {true, true, true});
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
	EidosAssertScriptSuccess_L("qnorm(0.05) + 1.644854 < 0.00001 ;", true);
	EidosAssertScriptSuccess_L("qnorm(0.95) - 1.644854 < 0.00001 ;", true);
	EidosAssertScriptSuccess_L("qnorm(0.05, 0, 1) + 1.644854 < 0.00001;", true);
	EidosAssertScriptSuccess_L("qnorm(0.05, 5.5, 3.4) + 0.09250233 < 0.00001;", true);
	EidosAssertScriptSuccess_L("qnorm(0.05, 0, 1.0) + 1.644854 < 0.00001;", true);
	EidosAssertScriptSuccess_LV("qnorm(c(0.05,0.05), c(0, 0), 1) + 1.644854 < 0.00001;", {true, true});
	EidosAssertScriptSuccess_LV("c(2, 1)*qnorm(c(0.05, 0.05), 0., c(1, 2)) + 3.289707 < 0.00001;", {true, true});
	EidosAssertScriptSuccess_LV("qnorm(c(0.25, 0.5, 0.75)) - c(-0.6744898, 0.0000000, 0.6744898) < 0.00001;", {true, true, true});
	EidosAssertScriptRaise("qnorm(0.5, 0, 0);", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("qnorm(-0.1);", 0, "requires 0.0 <= p <= 1.0");
	EidosAssertScriptRaise("qnorm(1.1);", 0, "requires 0.0 <= p <= 1.0");
	EidosAssertScriptRaise("qnorm(c(0.05, 1.1));", 0, "requires 0.0 <= p <= 1.0");
	EidosAssertScriptRaise("qnorm(c(0.05, 1.1), c(0.0, 0.1));", 0, "requires 0.0 <= p <= 1.0");
	EidosAssertScriptRaise("qnorm(c(0.05, 1.1), c(0.0, 0.1), c(0.1, 0.5));", 0, "requires 0.0 <= p <= 1.0");
	EidosAssertScriptRaise("qnorm(c(0.05, 0.95), 0.0, c(5, -1.0));", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("qnorm(0.1, c(-10, 10, 1), 100.0);", 0, "requires mean to be");
	EidosAssertScriptRaise("qnorm(0.1, 10.0, c(0.1, 10, 1));", 0, "requires sd to be");
	EidosAssertScriptSuccess("qnorm(NAN);", gStaticEidosValue_FloatNAN);

	// pnorm()
	EidosAssertScriptSuccess("pnorm(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("pnorm(float(0), float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess_L("pnorm(0.0, 0, 1) - 0.5 < 0.00001;", true);
	EidosAssertScriptSuccess_L("pnorm(1.0, 1.0, 1.0) - 0.5 < 0.00001;", true);
	EidosAssertScriptSuccess_LV("pnorm(c(0.0,0.0), c(0,0), 1) - 0.5 < 0.00001;", {true, true});
	EidosAssertScriptSuccess_LV("pnorm(c(0.0,1.0), c(0.0,1.0), 1.0) - 0.5 < 0.00001;", {true, true});
	EidosAssertScriptSuccess_LV("pnorm(c(0.0,0.0), 0.0, c(1.0,1.0)) - 0.5 < 0.00001;", {true, true});
	EidosAssertScriptSuccess_LV("pnorm(c(-1.0,0.0,1.0)) - c(0.1586553,0.5,0.8413447) < 0.00001;", {true, true, true});
	EidosAssertScriptSuccess_LV("pnorm(c(-1.0,0.0,1.0), mean=0.5, sd=10) - c(0.4403823,0.4800612,0.5199388) < 0.00001;", {true, true, true});
	EidosAssertScriptRaise("pnorm(1.0, 0, 0);", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("pnorm(1.0, 0.0, -1.0);", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("pnorm(c(0.5, 1.0), 0.0, c(5, -1.0));", 0, "requires sd > 0.0");
	EidosAssertScriptRaise("pnorm(1.0, c(-10, 10, 1), 100.0);", 0, "requires mean to be");
	EidosAssertScriptRaise("pnorm(1.0, 10.0, c(0.1, 10, 1));", 0, "requires sd to be");
	EidosAssertScriptSuccess("pnorm(NAN);", gStaticEidosValue_FloatNAN);
	
	// dbeta()
	EidosAssertScriptSuccess("dbeta(float(0), 1, 1000);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("dbeta(float(0), float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess_L("abs(dbeta(0.0, 1, 5) - c(5)) < 0.0001;", true);
	EidosAssertScriptSuccess_L("abs(dbeta(0.5, 1, 5) - c(0.3125)) < 0.0001;", true);
	EidosAssertScriptSuccess_L("abs(dbeta(1.0, 1, 5) - c(0)) < 0.0001;", true);
	EidosAssertScriptSuccess_LV("abs(dbeta(c(0, 0.5, 1), 1, 5) - c(5, 0.3125, 0)) < 0.0001;", {true, true, true});
	EidosAssertScriptSuccess_LV("abs(dbeta(c(0, 0.5, 1), 1, c(10, 4, 1)) - c(10, 0.5, 1)) < 0.0001;", {true, true, true});
	EidosAssertScriptSuccess_LV("abs(dbeta(c(0, 0.5, 1), c(1, 2, 3), c(10, 4, 1)) - c(10, 1.25, 3)) < 0.0001;", {true, true, true});
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
	EidosAssertScriptSuccess_L("setSeed(0); abs(rbeta(1, 1, 5) - c(0.115981)) < 0.0001;", true);
	EidosAssertScriptSuccess_LV("setSeed(0); abs(rbeta(3, 1, 5) - c(0.115981, 0.0763773, 0.05032)) < 0.0001;", {true, true, true});
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
	EidosAssertScriptSuccess_IV("rbinom(1, 10, 0.0);", {0});
	EidosAssertScriptSuccess_IV("rbinom(3, 10, 0.0);", {0, 0, 0});
	EidosAssertScriptSuccess_IV("rbinom(3, 10, 1.0);", {10, 10, 10});
	EidosAssertScriptSuccess_IV("rbinom(3, 0, 0.0);", {0, 0, 0});
	EidosAssertScriptSuccess_IV("rbinom(3, 0, 1.0);", {0, 0, 0});
	EidosAssertScriptSuccess_IV("setSeed(0); rbinom(10, 1, 0.5);", {0, 1, 1, 1, 1, 1, 0, 0, 0, 0});
	EidosAssertScriptSuccess_IV("setSeed(0); rbinom(10, 1, 0.5000001);", {1, 0, 0, 1, 1, 0, 1, 0, 1, 0});
	EidosAssertScriptSuccess_IV("setSeed(0); rbinom(5, 10, 0.5);", {4, 8, 5, 3, 4});
	EidosAssertScriptSuccess_IV("setSeed(1); rbinom(5, 10, 0.5);", {7, 6, 3, 6, 3});
	EidosAssertScriptSuccess_IV("setSeed(2); rbinom(5, 1000, 0.01);", {11, 16, 10, 14, 10});
	EidosAssertScriptSuccess_IV("setSeed(3); rbinom(5, 1000, 0.99);", {992, 990, 995, 991, 995});
	EidosAssertScriptSuccess_IV("setSeed(4); rbinom(3, 100, c(0.1, 0.5, 0.9));", {7, 50, 87});
	EidosAssertScriptSuccess_IV("setSeed(5); rbinom(3, c(10, 30, 50), 0.5);", {6, 12, 26});
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
	EidosAssertScriptSuccess_LV("setSeed(0); (rcauchy(2) - c(0.665522, -0.155038)) < 0.00001;", {true, true});
	EidosAssertScriptSuccess_LV("setSeed(0); (rcauchy(2, 10.0) - c(10.6655, 9.84496)) < 0.001;", {true, true});
	EidosAssertScriptSuccess_LV("setSeed(2); (rcauchy(2, 10.0, 100.0) - c(-255.486, -4.66262)) < 0.001;", {true, true});
	EidosAssertScriptSuccess_LV("setSeed(3); (rcauchy(2, c(-10, 10), 100.0) - c(89.8355, 1331.82)) < 0.01;", {true, true});
	EidosAssertScriptSuccess_LV("setSeed(4); (rcauchy(2, 10.0, c(0.1, 10)) - c(10.05, -4.51227)) < 0.001;", {true, true});
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
	EidosAssertScriptSuccess_IV("rdunif(1, 0, 0);", {0});
	EidosAssertScriptSuccess_IV("rdunif(3, 0, 0);", {0, 0, 0});
	EidosAssertScriptSuccess_IV("rdunif(1, 1, 1);", {1});
	EidosAssertScriptSuccess_IV("rdunif(3, 1, 1);", {1, 1, 1});
	EidosAssertScriptSuccess_L("setSeed(0); identical(rdunif(1), 0);", true);
	EidosAssertScriptSuccess_L("setSeed(0); identical(rdunif(10), c(0,1,1,1,1,1,0,0,0,0));", true);
	EidosAssertScriptSuccess_L("setSeed(0); identical(rdunif(10, 10, 11), c(10,11,11,11,11,11,10,10,10,10));", true);
	EidosAssertScriptSuccess_L("setSeed(0); identical(rdunif(10, 10, 15), c(10, 15, 11, 10, 14, 12, 11, 10, 12, 15));", true);
	EidosAssertScriptSuccess_L("setSeed(0); identical(rdunif(10, -10, 15), c(-6, 9, 13, 8, -10, -2, 1, -2, 4, -9));", true);
	EidosAssertScriptSuccess_L("setSeed(0); identical(rdunif(5, 1000000, 2000000), c(1834587, 1900900, 1272746, 1916963, 1786506));", true);
	EidosAssertScriptSuccess_L("setSeed(0); identical(rdunif(5, 1000000000, 2000000000), c(1824498419, 1696516320, 1276316141, 1114192161, 1469447550));", true);
	EidosAssertScriptSuccess_L("setSeed(0); identical(rdunif(5, 10000000000, 20000000000), c(18477398967, 14168180191, 12933243864, 17033840166, 15472500391));", true);	// 64-bit range
	EidosAssertScriptRaise("rdunif(-1);", 0, "requires n to be");
	EidosAssertScriptRaise("rdunif(1, 0, -1);", 0, "requires min <= max");
	EidosAssertScriptRaise("rdunif(2, 0, c(7, -1));", 0, "requires min <= max");
	EidosAssertScriptRaise("rdunif(2, c(7, -1), 0);", 0, "requires min <= max");
	EidosAssertScriptRaise("rdunif(2, c(-10, 10, 1), 100);", 0, "requires min");
	EidosAssertScriptRaise("rdunif(2, -10, c(1, 10, 1));", 0, "requires max");
	
	// dexp()
	EidosAssertScriptSuccess("dexp(float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("dexp(float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess_L("abs(dexp(1.0) - 0.3678794) < 0.00001;", true);
	EidosAssertScriptSuccess_L("abs(dexp(0.01) - 0.9900498) < 0.00001;", true);
	EidosAssertScriptSuccess_L("all(abs(dexp(c(1.0, 0.01)) - c(0.3678794, 0.9900498)) < 0.00001);", true);
	EidosAssertScriptSuccess_L("abs(dexp(0.01, 0.1) - 9.048374) < 0.00001;", true);
	EidosAssertScriptSuccess_L("abs(dexp(0.01, 0.01) - 36.78794) < 0.0001;", true);
	EidosAssertScriptSuccess_LV("abs(dexp(c(0.01, 0.01, 0.01), c(1, 0.1, 0.01)) - c(0.9900498, 9.048374, 36.78794)) < 0.0001;", {true, true, true});
	EidosAssertScriptRaise("dexp(3.0, c(10, 5));", 0, "requires mu to be");
	EidosAssertScriptSuccess("dexp(NAN, 0.1);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("dexp(0.01, NAN);", gStaticEidosValue_FloatNAN);
	
	// rexp()
	EidosAssertScriptSuccess("rexp(0);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rexp(0, float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess_L("setSeed(0); abs(rexp(1) - c(0.206919)) < 0.00001;", true);
	EidosAssertScriptSuccess_LV("setSeed(0); abs(rexp(3) - c(0.206919, 3.01675, 0.788416)) < 0.00001;", {true, true, true});
	EidosAssertScriptSuccess_LV("setSeed(1); abs(rexp(3, 10) - c(20.7, 12.2, 0.9)) < 0.1;", {true, true, true});
	EidosAssertScriptSuccess_LV("setSeed(2); abs(rexp(3, 100000) - c(95364.3, 307170.0, 74334.9)) < 0.1;", {true, true, true});
	EidosAssertScriptSuccess_LV("setSeed(3); abs(rexp(3, c(10, 100, 1000)) - c(2.8, 64.6, 58.8)) < 0.1;", {true, true, true});
	EidosAssertScriptRaise("rexp(-1);", 0, "requires n to be");
	EidosAssertScriptRaise("rexp(3, c(10, 5));", 0, "requires mu to be");
	EidosAssertScriptSuccess("rexp(1, NAN);", gStaticEidosValue_FloatNAN);
	
	// dgamma()
	EidosAssertScriptSuccess("dgamma(float(0), 0, 1000);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("dgamma(float(0), float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("dgamma(3.0, 0, 1000);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess_L("abs(dgamma(0.1, 1/100, 1) - 0.004539993) < 0.0001;", true);
	EidosAssertScriptSuccess_L("abs(dgamma(0.01, 1/100, 1) - 36.78794) < 0.0001;", true);
	EidosAssertScriptSuccess_L("abs(dgamma(0.001, 1/100, 1) - 90.48374) < 0.0001;", true);
	EidosAssertScriptSuccess_LV("abs(dgamma(c(0.1, 0.01, 0.001), 1/100, 1) - c(0.004539993, 36.78794, 90.48374)) < 0.0001;", {true, true, true});
	EidosAssertScriptRaise("dgamma(2.0, 0, 0);", 0, "requires shape > 0.0");
	EidosAssertScriptRaise("dgamma(c(1.0, 2.0), 0, c(1.0, 0));", 0, "requires shape > 0.0");
	EidosAssertScriptRaise("dgamma(2.0, c(0.1, 10, 1), 10.0);", 0, "requires mean to be of length");
	EidosAssertScriptRaise("dgamma(2.0, 10.0, c(0.1, 10, 1));", 0, "requires shape to be of length");
	EidosAssertScriptSuccess("dgamma(NAN, 1/100, 1);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("dgamma(0.1, NAN, 1);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptRaise("dgamma(0.1, 1/100, NAN);", 0, "requires shape > 0.0");
	
	// rf()
	EidosAssertScriptSuccess("rf(0, 10, 15);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rf(0, float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess_L("setSeed(0); abs(rf(1, 2, 3) - c(0.568968)) < 0.0001;", true);
	EidosAssertScriptSuccess_LV("setSeed(0); abs(rf(3, 2, 3) - c(0.568968, 0.533479, 0.316429)) < 0.0001;", {true, true, true});
	EidosAssertScriptSuccess_LV("setSeed(0); abs(rf(3, 2, 4) - c(0.588202, 0.486162, 0.295787)) < 0.0001;", {true, true, true});
	EidosAssertScriptSuccess_LV("setSeed(0); abs(rf(3, c(2,2,2), 4) - c(0.588202, 0.486162, 0.295787)) < 0.0001;", {true, true, true});
	EidosAssertScriptSuccess_LV("setSeed(0); abs(rf(3, 2, c(4,4,4)) - c(0.588202, 0.486162, 0.295787)) < 0.0001;", {true, true, true});
	EidosAssertScriptRaise("rf(-1, 10, 15);", 0, "requires n to be");
	EidosAssertScriptRaise("rf(2, 0, 15);", 0, "requires d1 > 0.0");
	EidosAssertScriptRaise("rf(2, 10, 0);", 0, "requires d2 > 0.0");
	EidosAssertScriptRaise("rf(2, c(10,0), 15);", 0, "requires d1 > 0.0");
	EidosAssertScriptRaise("rf(2, 10, c(15,0));", 0, "requires d2 > 0.0");
	EidosAssertScriptRaise("rf(2, c(0.1, 10, 1), 10.0);", 0, "requires d1 to be of length");
	EidosAssertScriptRaise("rf(2, 10.0, c(0.1, 10, 1));", 0, "requires d2 to be of length");
	EidosAssertScriptSuccess("rf(1, NAN, 15);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("rf(1, 10, NAN);", gStaticEidosValue_FloatNAN);
	
	// rgamma()
	EidosAssertScriptSuccess("rgamma(0, 0, 1000);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rgamma(0, float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess_FV("rgamma(3, 0, 1000);", {0.0, 0.0, 0.0});
	EidosAssertScriptSuccess_L("setSeed(0); abs(rgamma(1, 1, 100) - c(1.02069)) < 0.0001;", true);
	EidosAssertScriptSuccess_LV("setSeed(0); abs(rgamma(3, 1, 100) - c(1.02069, 1.0825, 0.951862)) < 0.0001;", {true, true, true});
	EidosAssertScriptSuccess_LV("setSeed(0); abs(rgamma(3, -1, 100) - c(-1.02069, -1.0825, -0.951862)) < 0.0001;", {true, true, true});
	EidosAssertScriptSuccess_LV("setSeed(0); abs(rgamma(3, c(-1,-1,-1), 100) - c(-1.02069, -1.0825, -0.951862)) < 0.0001;", {true, true, true});
	EidosAssertScriptSuccess_LV("setSeed(0); abs(rgamma(3, -1, c(100,100,100)) - c(-1.02069, -1.0825, -0.951862)) < 0.0001;", {true, true, true});
	EidosAssertScriptRaise("rgamma(-1, 0, 1000);", 0, "requires n to be");
	EidosAssertScriptRaise("rgamma(2, 0, 0);", 0, "requires shape > 0.0");
	EidosAssertScriptRaise("rgamma(2, c(0,0), 0);", 0, "requires shape > 0.0");
	EidosAssertScriptRaise("rgamma(2, c(0.1, 10, 1), 10.0);", 0, "requires mean to be of length");
	EidosAssertScriptRaise("rgamma(2, 10.0, c(0.1, 10, 1));", 0, "requires shape to be of length");
	EidosAssertScriptSuccess("rgamma(1, NAN, 100);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("rgamma(1, 1, NAN);", gStaticEidosValue_FloatNAN);
	
	// rgeom()
	EidosAssertScriptSuccess("rgeom(0, 1.0);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess_IV("rgeom(1, 1.0);", {0});
	EidosAssertScriptSuccess_IV("rgeom(5, 1.0);", {0, 0, 0, 0, 0});
	EidosAssertScriptSuccess_IV("setSeed(1); rgeom(5, 0.2);", {0, 1, 10, 1, 10});
	EidosAssertScriptSuccess_IV("setSeed(1); rgeom(5, 0.4);", {0, 0, 4, 0, 4});
	EidosAssertScriptSuccess_IV("setSeed(5); rgeom(5, 0.01);", {31, 31, 299, 129, 58});
	EidosAssertScriptSuccess_IV("setSeed(2); rgeom(1, 0.0001);", {4866});
	EidosAssertScriptSuccess_IV("setSeed(3); rgeom(6, c(1, 0.1, 0.01, 0.001, 0.0001, 0.00001));", {0, 13, 73, 2860, 8316, 282489});
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
	EidosAssertScriptSuccess_FV("rlnorm(1, 0, 0);", {1.0});
	EidosAssertScriptSuccess_FV("rlnorm(3, 0, 0);", {1.0, 1.0, 1.0});
	EidosAssertScriptSuccess_LV("abs(rlnorm(3, 1, 0) - E) < 0.000001;", {true, true, true});
	EidosAssertScriptSuccess_LV("abs(rlnorm(3, c(1,1,1), 0) - E) < 0.000001;", {true, true, true});
	EidosAssertScriptSuccess_LV("abs(rlnorm(3, 1, c(0,0,0)) - E) < 0.000001;", {true, true, true});
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
	EidosAssertScriptSuccess_L("x = rmvnorm(5, c(0,2), matrix(c(10,3,3,2), nrow=2)); identical(dim(x), c(5,2));", true);
	EidosAssertScriptSuccess_L("x = rmvnorm(5, c(0,NAN), matrix(c(10,3,3,2), nrow=2)); all(!isNAN(x[,0])) & all(isNAN(x[,1]));", true);
	EidosAssertScriptRaise("rmvnorm(5, c(0,2), matrix(c(10,3,NAN,2), nrow=2));", 0, "to contain NANs");
	
	// rnbinom()
	EidosAssertScriptSuccess("rnbinom(0, 10, 0.5);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess_IV("rnbinom(1, 10, 1.0);", {0});
	EidosAssertScriptSuccess_IV("rnbinom(1, 10.0, 1.0);", {0});
	EidosAssertScriptSuccess_IV("rnbinom(3, 10, 1.0);", {0, 0, 0});
	EidosAssertScriptSuccess_IV("rnbinom(3, 10.0, 1.0);", {0, 0, 0});
	EidosAssertScriptRaise("rnbinom(3, 0, 0.0);", 0, "probability in (0.0, 1.0]");
	EidosAssertScriptSuccess_IV("rnbinom(3, 0, 1.0);", {0, 0, 0});
	EidosAssertScriptSuccess_IV("setSeed(0); rnbinom(10, 1, 0.5);", {1, 0, 0, 0, 0, 1, 0, 0, 2, 2});
	EidosAssertScriptSuccess_IV("setSeed(0); rnbinom(10, 1, 0.5000001);", {1, 0, 0, 0, 0, 1, 0, 0, 2, 2});
	EidosAssertScriptSuccess_IV("setSeed(0); rnbinom(5, 10, 0.5);", {6, 13, 1, 6, 5});
	EidosAssertScriptSuccess_IV("setSeed(1); rnbinom(5, 10, 0.5);", {2, 6, 9, 10, 7});
	EidosAssertScriptSuccess_IV("setSeed(2); rnbinom(5, 1000, 0.01);", {103776, 97182, 94313, 95927, 92216});
	EidosAssertScriptSuccess_IV("setSeed(3); rnbinom(5, 1000, 0.99);", {6, 6, 8, 5, 15});
	EidosAssertScriptSuccess_IV("setSeed(4); rnbinom(3, 100, c(0.1, 0.5, 0.9));", {842, 125, 11});
	EidosAssertScriptSuccess_IV("setSeed(5); rnbinom(3, c(10, 30, 50), 0.5);", {16, 26, 45});
	EidosAssertScriptRaise("rnbinom(-1, 10, 0.5);", 0, "requires n to be");
	EidosAssertScriptRaise("rnbinom(3, -1, 0.5);", 0, "requires size >= 0");
	EidosAssertScriptRaise("rnbinom(3, 10, -0.1);", 0, "in (0.0, 1.0]");
	EidosAssertScriptRaise("rnbinom(3, 10, 1.1);", 0, "in (0.0, 1.0]");
	EidosAssertScriptRaise("rnbinom(3, 10, c(0.1, 0.2));", 0, "to be of length 1 or n");
	EidosAssertScriptRaise("rnbinom(3, c(10, 12), 0.5);", 0, "to be of length 1 or n");
	EidosAssertScriptRaise("rnbinom(2, -1, c(0.5,0.5));", 0, "requires size >= 0");
	EidosAssertScriptRaise("rnbinom(2, c(10,10), -0.1);", 0, "in (0.0, 1.0]");
	EidosAssertScriptRaise("rnbinom(2, 10, NAN);", 0, "in (0.0, 1.0]");
	EidosAssertScriptRaise("rnbinom(2, NAN, 0.5);", 0, "requires size >= 0");
	
	// rnorm()
	EidosAssertScriptSuccess("rnorm(0);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("rnorm(0, float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess_FV("rnorm(1, 0, 0);", {0.0});
	EidosAssertScriptSuccess_FV("rnorm(3, 0, 0);", {0.0, 0.0, 0.0});
	EidosAssertScriptSuccess_FV("rnorm(1, 1, 0);", {1.0});
	EidosAssertScriptSuccess_FV("rnorm(3, 1, 0);", {1.0, 1.0, 1.0});
	EidosAssertScriptSuccess_LV("setSeed(0); (rnorm(2) - c(-0.785386, 0.132009)) < 0.000001;", {true, true});
	EidosAssertScriptSuccess_LV("setSeed(1); (rnorm(2, 10.0) - c(10.38, 10.26)) < 0.01;", {true, true});
	EidosAssertScriptSuccess_LV("setSeed(2); (rnorm(2, 10.0, 100.0) - c(59.92, 95.35)) < 0.01;", {true, true});
	EidosAssertScriptSuccess_LV("setSeed(3); (rnorm(2, c(-10, 10), 100.0) - c(59.92, 95.35)) < 0.01;", {true, true});
	EidosAssertScriptSuccess_LV("setSeed(4); (rnorm(2, 10.0, c(0.1, 10)) - c(59.92, 95.35)) < 0.01;", {true, true});
	EidosAssertScriptRaise("rnorm(-1);", 0, "requires n to be");
	EidosAssertScriptRaise("rnorm(1, 0, -1);", 0, "requires sd >= 0.0");
	EidosAssertScriptRaise("rnorm(2, c(0,0), -1);", 0, "requires sd >= 0.0");
	EidosAssertScriptRaise("rnorm(2, 0, c(-1, -1));", 0, "requires sd >= 0.0");
	EidosAssertScriptRaise("rnorm(2, c(0,0), c(-1, -1));", 0, "requires sd >= 0.0");
	EidosAssertScriptRaise("rnorm(2, c(-10, 10, 1), 100.0);", 0, "requires mean to be");
	EidosAssertScriptRaise("rnorm(2, 10.0, c(0.1, 10, 1));", 0, "requires sd to be");
	EidosAssertScriptSuccess("rnorm(1, 1, NAN);", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess("rnorm(1, NAN, 1);", gStaticEidosValue_FloatNAN);
	
	// rpois()
	EidosAssertScriptSuccess("rpois(0, 1.0);", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess_IV("setSeed(0); rpois(5, 1.0);", {0, 2, 0, 1, 1});
	EidosAssertScriptSuccess_IV("setSeed(1); rpois(5, 0.2);", {1, 0, 0, 0, 0});
	EidosAssertScriptSuccess_IV("setSeed(2); rpois(5, 10000);", {10205, 10177, 10094, 10227, 9875});
	EidosAssertScriptSuccess_IV("setSeed(2); rpois(1, 10000);", {10205});
	EidosAssertScriptSuccess_IV("setSeed(3); rpois(5, c(1, 10, 100, 1000, 10000));", {0, 8, 97, 994, 9911});
	EidosAssertScriptRaise("rpois(-1, 1.0);", 0, "requires n to be");
	EidosAssertScriptRaise("rpois(0, 0.0);", 0, "requires lambda > 0.0");
	EidosAssertScriptRaise("rpois(0, NAN);", 0, "requires lambda > 0.0");
	EidosAssertScriptRaise("rpois(2, c(0.0, 0.0));", 0, "requires lambda > 0.0");
	EidosAssertScriptRaise("rpois(2, c(1.5, NAN));", 0, "requires lambda > 0.0");
	EidosAssertScriptRaise("setSeed(4); rpois(5, c(1, 10, 100, 1000));", 12, "requires lambda");
	
	// runif()
	EidosAssertScriptSuccess("runif(0);", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess("runif(0, float(0), float(0));", gStaticEidosValue_Float_ZeroVec);
	EidosAssertScriptSuccess_FV("runif(1, 0, 0);", {0.0});
	EidosAssertScriptSuccess_FV("runif(3, 0, 0);", {0.0, 0.0, 0.0});
	EidosAssertScriptSuccess_FV("runif(1, 1, 1);", {1.0});
	EidosAssertScriptSuccess_FV("runif(3, 1, 1);", {1.0, 1.0, 1.0});
	EidosAssertScriptSuccess_L("setSeed(0); abs(runif(1) - c(0.186915)) < 0.000001;", true);
	EidosAssertScriptSuccess_LV("setSeed(0); abs(runif(2) - c(0.186915, 0.951040)) < 0.000001;", {true, true});
	EidosAssertScriptSuccess_LV("setSeed(1); abs(runif(2, 0.5) - c(0.93, 0.85)) < 0.01;", {true, true});
	EidosAssertScriptSuccess_LV("setSeed(2); abs(runif(2, 10.0, 100.0) - c(65.31, 95.82)) < 0.01;", {true, true});
	EidosAssertScriptSuccess_LV("setSeed(3); abs(runif(2, c(-100, 1), 10.0) - c(-72.52, 5.28)) < 0.01;", {true, true});
	EidosAssertScriptSuccess_LV("setSeed(4); abs(runif(2, -10.0, c(1, 1000)) - c(-8.37, 688.97)) < 0.01;", {true, true});
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
	EidosAssertScriptSuccess_L("setSeed(0); abs(rweibull(1, 1, 1) - c(1.6771)) < 0.0001;", true);
	EidosAssertScriptSuccess_LV("setSeed(0); abs(rweibull(3, 1, 1) - c(1.6771, 0.0501994, 0.60617)) < 0.0001;", {true, true, true});
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













































