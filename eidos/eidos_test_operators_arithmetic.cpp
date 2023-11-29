//
//  eidos_test_operators_arithmetic.cpp
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


#pragma mark operator +
void _RunOperatorPlusTests1(void)
{
	// operator +
	EidosAssertScriptRaise("NULL+T;", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+0;", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+0.5;", 4, "combination of operand types");
	EidosAssertScriptSuccess_S("NULL+'foo';", "NULLfoo");
	EidosAssertScriptRaise("NULL+_Test(7);", 4, "combination of operand types");
	EidosAssertScriptRaise("NULL+(0:2);", 4, "combination of operand types");
	EidosAssertScriptRaise("T+NULL;", 1, "combination of operand types");
	EidosAssertScriptRaise("0+NULL;", 1, "combination of operand types");
	EidosAssertScriptRaise("0.5+NULL;", 3, "combination of operand types");
	EidosAssertScriptSuccess_S("'foo'+NULL;", "fooNULL");
	EidosAssertScriptRaise("_Test(7)+NULL;", 8, "combination of operand types");
	EidosAssertScriptRaise("(0:2)+NULL;", 5, "combination of operand types");
	EidosAssertScriptRaise("+NULL;", 0, "operand type NULL is not supported");
	EidosAssertScriptSuccess_I("1+1;", 2);
	EidosAssertScriptSuccess("1+-1;", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess_IV("(0:2)+10;", {10, 11, 12});
	EidosAssertScriptSuccess_IV("10+(0:2);", {10, 11, 12});
	EidosAssertScriptSuccess_IV("(15:13)+(0:2);", {15, 15, 15});
	EidosAssertScriptRaise("(15:12)+(0:2);", 7, "operator requires that either");
	EidosAssertScriptSuccess_F("1+1.0;", 2);
	EidosAssertScriptSuccess_F("1.0+1;", 2);
	EidosAssertScriptSuccess_F("1.0+-1.0;", 0);
	EidosAssertScriptSuccess_FV("(0:2.0)+10;", {10, 11, 12});
	EidosAssertScriptSuccess_FV("10.0+(0:2);", {10, 11, 12});
	EidosAssertScriptSuccess_FV("10+(0.0:2);", {10, 11, 12});
	EidosAssertScriptSuccess_FV("(15.0:13)+(0:2.0);", {15, 15, 15});
	EidosAssertScriptRaise("(15:12.0)+(0:2);", 9, "operator requires that either");
	EidosAssertScriptSuccess_S("'foo'+5;", "foo5");
	EidosAssertScriptSuccess_S("'foo'+5.0;", "foo5.0");
	EidosAssertScriptSuccess_S("'foo'+5.1;", "foo5.1");
	EidosAssertScriptSuccess_S("5+'foo';", "5foo");
	EidosAssertScriptSuccess_S("5.0+'foo';", "5.0foo");
	EidosAssertScriptSuccess_S("5.1+'foo';", "5.1foo");
	EidosAssertScriptSuccess_SV("'foo'+1:3;", {"foo1", "foo2", "foo3"});
	EidosAssertScriptSuccess_SV("1:3+'foo';", {"1foo", "2foo", "3foo"});
	EidosAssertScriptSuccess_S("'foo'+'bar';", "foobar");
	EidosAssertScriptSuccess_SV("'foo'+c('bar', 'baz');", {"foobar", "foobaz"});
	EidosAssertScriptSuccess_SV("c('bar', 'baz')+'foo';", {"barfoo", "bazfoo"});
	EidosAssertScriptSuccess_SV("c('bar', 'baz')+c('foo', 'biz');", {"barfoo", "bazbiz"});
	EidosAssertScriptRaise("c('bar', 'baz')+c('foo', 'biz', 'boz');", 15, "operator requires that either");
	EidosAssertScriptSuccess_SV("c('bar', 'baz')+T;", {"barT", "bazT"});
	EidosAssertScriptSuccess_SV("F+c('bar', 'baz');", {"Fbar", "Fbaz"});
	EidosAssertScriptRaise("T+F;", 1, "combination of operand types");
	EidosAssertScriptRaise("T+T;", 1, "combination of operand types");
	EidosAssertScriptRaise("F+F;", 1, "combination of operand types");
	EidosAssertScriptSuccess_I("+5;", 5);
	EidosAssertScriptSuccess_F("+5.0;", 5);
	EidosAssertScriptRaise("+'foo';", 0, "is not supported by");
	EidosAssertScriptRaise("+T;", 0, "is not supported by");
	EidosAssertScriptSuccess_I("3+4+5;", 12);
	EidosAssertScriptSuccess("3.2+NAN+4.5;", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess_FV("3.5+c(5.5,NAN,2.5);", {9.0, std::numeric_limits<double>::quiet_NaN(), 6.0});
	EidosAssertScriptSuccess_FV("c(5.5,NAN,2.5)+3.5;", {9.0, std::numeric_limits<double>::quiet_NaN(), 6.0});
	EidosAssertScriptSuccess_FV("c(5.5,NAN,2.5)+c(5.5,3.5,NAN);", {11.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
	
	// operator +: raise on integer addition overflow for all code paths
	EidosAssertScriptSuccess_I("5e18;", 5000000000000000000LL);
	EidosAssertScriptRaise("1e19;", 0, "could not be represented");
#if EIDOS_HAS_OVERFLOW_BUILTINS
	EidosAssertScriptRaise("5e18 + 5e18;", 5, "overflow with the binary");
	EidosAssertScriptRaise("5e18 + c(0, 0, 5e18, 0);", 5, "overflow with the binary");
	EidosAssertScriptRaise("c(0, 0, 5e18, 0) + 5e18;", 17, "overflow with the binary");
	EidosAssertScriptRaise("c(0, 0, 5e18, 0) + c(0, 0, 5e18, 0);", 17, "overflow with the binary");
#endif
	
	// operator +: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	// this is the only place where we test the binary operators with matrices and arrays so comprehensively; the same machinery is used for all, so it should suffice
	EidosAssertScriptSuccess_L("identical(1 + integer(0), integer(0));", true);
	EidosAssertScriptSuccess_L("identical(1 + 2, 3);", true);
	EidosAssertScriptSuccess_L("identical(1 + 1:3, 2:4);", true);
	EidosAssertScriptSuccess_L("identical(1 + matrix(2), matrix(3));", true);
	EidosAssertScriptSuccess_L("identical(1 + array(2,c(1,1,1)), array(3, c(1,1,1)));", true);
	EidosAssertScriptSuccess_L("identical(1 + matrix(1:3,nrow=1), matrix(2:4, nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(1 + matrix(1:3,ncol=1), matrix(2:4, ncol=1));", true);
	EidosAssertScriptSuccess_L("identical(1 + matrix(1:6,ncol=2), matrix(2:7, ncol=2));", true);
	EidosAssertScriptSuccess_L("identical(1 + array(1:3,c(3,1,1)), array(2:4, c(3,1,1)));", true);
	EidosAssertScriptSuccess_L("identical(1 + array(1:3,c(1,3,1)), array(2:4, c(1,3,1)));", true);
	EidosAssertScriptSuccess_L("identical(1 + array(1:3,c(1,1,3)), array(2:4, c(1,1,3)));", true);
	EidosAssertScriptSuccess_L("identical(1 + array(1:6,c(3,2,1)), array(2:7, c(3,2,1)));", true);
	EidosAssertScriptSuccess_L("identical(1 + array(1:6,c(3,1,2)), array(2:7, c(3,1,2)));", true);
	EidosAssertScriptSuccess_L("identical(1 + array(1:6,c(2,3,1)), array(2:7, c(2,3,1)));", true);
	EidosAssertScriptSuccess_L("identical(1 + array(1:6,c(1,3,2)), array(2:7, c(1,3,2)));", true);
	EidosAssertScriptSuccess_L("identical(1 + array(1:6,c(2,1,3)), array(2:7, c(2,1,3)));", true);
	EidosAssertScriptSuccess_L("identical(1 + array(1:6,c(1,2,3)), array(2:7, c(1,2,3)));", true);
	
	EidosAssertScriptRaise("identical(1:3 + integer(0), integer(0));", 14, "requires that either");
	EidosAssertScriptSuccess_L("identical(1:3 + 2, 3:5);", true);
	EidosAssertScriptSuccess_L("identical(1:3 + 1:3, (1:3)*2);", true);
	EidosAssertScriptSuccess_L("identical(1:3 + matrix(2), 3:5);", true);
	EidosAssertScriptSuccess_L("identical(1:3 + array(2,c(1,1,1)), 3:5);", true);
	EidosAssertScriptSuccess_L("identical(1:3 + matrix(1:3,nrow=1), matrix((1:3)*2, nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(1:3 + matrix(1:3,ncol=1), matrix((1:3)*2, ncol=1));", true);
	EidosAssertScriptSuccess_L("identical(1:6 + matrix(1:6,ncol=2), matrix((1:6)*2, ncol=2));", true);
	EidosAssertScriptSuccess_L("identical(1:3 + array(1:3,c(3,1,1)), array((1:3)*2, c(3,1,1)));", true);
	EidosAssertScriptSuccess_L("identical(1:3 + array(1:3,c(1,3,1)), array((1:3)*2, c(1,3,1)));", true);
	EidosAssertScriptSuccess_L("identical(1:3 + array(1:3,c(1,1,3)), array((1:3)*2, c(1,1,3)));", true);
	EidosAssertScriptSuccess_L("identical(1:6 + array(1:6,c(3,2,1)), array((1:6)*2, c(3,2,1)));", true);
	EidosAssertScriptSuccess_L("identical(1:6 + array(1:6,c(3,1,2)), array((1:6)*2, c(3,1,2)));", true);
	EidosAssertScriptSuccess_L("identical(1:6 + array(1:6,c(2,3,1)), array((1:6)*2, c(2,3,1)));", true);
	EidosAssertScriptSuccess_L("identical(1:6 + array(1:6,c(1,3,2)), array((1:6)*2, c(1,3,2)));", true);
	EidosAssertScriptSuccess_L("identical(1:6 + array(1:6,c(2,1,3)), array((1:6)*2, c(2,1,3)));", true);
	EidosAssertScriptSuccess_L("identical(1:6 + array(1:6,c(1,2,3)), array((1:6)*2, c(1,2,3)));", true);
	
	EidosAssertScriptSuccess_L("identical(matrix(1) + integer(0), integer(0));", true);
	EidosAssertScriptSuccess_L("identical(matrix(1) + 2, matrix(3));", true);
	EidosAssertScriptSuccess_L("identical(matrix(1) + 1:3, 2:4);", true);
	EidosAssertScriptSuccess_L("identical(matrix(1) + matrix(2), matrix(3));", true);
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
	
	EidosAssertScriptSuccess_L("identical(array(1,c(1,1,1)) + integer(0), integer(0));", true);
	EidosAssertScriptSuccess_L("identical(array(1,c(1,1,1)) + 2, array(3, c(1,1,1)));", true);
	EidosAssertScriptSuccess_L("identical(array(1,c(1,1,1)) + 1:3, 2:4);", true);
	EidosAssertScriptRaise("identical(array(1,c(1,1,1)) + matrix(2), matrix(3));", 28, "non-conformable");
	EidosAssertScriptSuccess_L("identical(array(1,c(1,1,1)) + array(2,c(1,1,1)), array(3, c(1,1,1)));", true);
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
	EidosAssertScriptSuccess_L("identical(matrix(1:3,nrow=1) + 2, matrix(3:5, nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(matrix(1:3,nrow=1) + 1:3, matrix((1:3)*2, nrow=1));", true);
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + matrix(2), matrix(3));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + array(2,c(1,1,1)), array(3, c(1,1,1)));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(1:3,nrow=1) + matrix(1:3,nrow=1), matrix((1:3)*2, nrow=1));", true);
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
	EidosAssertScriptSuccess_L("identical(matrix(1:3,ncol=1) + 2, matrix(3:5, ncol=1));", true);
	EidosAssertScriptSuccess_L("identical(matrix(1:3,ncol=1) + 1:3, matrix((1:3)*2, ncol=1));", true);
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + matrix(2), matrix(3));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + array(2,c(1,1,1)), array(3, c(1,1,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,ncol=1) + matrix(1:3,nrow=1), matrix(2:4, nrow=1));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(1:3,ncol=1) + matrix(1:3,ncol=1), matrix((1:3)*2, ncol=1));", true);
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
	EidosAssertScriptSuccess_L("identical(matrix(1:6,ncol=2) + 2, matrix(3:8, ncol=2));", true);
	EidosAssertScriptSuccess_L("identical(matrix(1:6,ncol=2) + 1:6, matrix((1:6)*2, ncol=2));", true);
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + matrix(2), matrix(3));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(2,c(1,1,1)), array(3, c(1,1,1)));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + matrix(1:6,nrow=1), matrix(2:4, nrow=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + matrix(1:6,ncol=1), matrix(2:4, ncol=1));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(1:6,ncol=2) + matrix(1:6,ncol=2), matrix((1:6)*2, ncol=2));", true);
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
	EidosAssertScriptSuccess_L("identical(array(1:6,c(3,2,1)) + 2, array(3:8, c(3,2,1)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6,c(3,2,1)) + 1:6, array((1:6)*2, c(3,2,1)));", true);
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + matrix(2), matrix(3));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(2,c(1,1,1)), array(3, c(1,1,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + matrix(1:6,nrow=1), matrix(2:4, nrow=1));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + matrix(1:6,ncol=1), matrix(2:4, ncol=1));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + matrix(1:6,ncol=2), matrix((1:6)*2, ncol=2));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(1:3,c(3,1,1)), array(2:4, c(3,1,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(1:3,c(1,3,1)), array(2:4, c(1,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(3,2,1)) + array(1:3,c(1,1,3)), array(2:4, c(1,1,3)));", 30, "non-conformable");
	EidosAssertScriptSuccess_L("identical(array(1:6,c(3,2,1)) + array(1:6,c(3,2,1)), array((1:6)*2, c(3,2,1)));", true);
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
	EidosAssertScriptSuccess_L("identical(integer(0) + 1, integer(0));", true);
	EidosAssertScriptSuccess_L("identical(2 + 1, 3);", true);
	EidosAssertScriptSuccess_L("identical(1:3 + 1, 2:4);", true);
	EidosAssertScriptSuccess_L("identical(matrix(2) + 1, matrix(3));", true);
	EidosAssertScriptSuccess_L("identical(array(2,c(1,1,1)) + 1, array(3, c(1,1,1)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(1:3,nrow=1) + 1, matrix(2:4, nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(matrix(1:3,ncol=1) + 1, matrix(2:4, ncol=1));", true);
	EidosAssertScriptSuccess_L("identical(matrix(1:6,ncol=2) + 1, matrix(2:7, ncol=2));", true);
	EidosAssertScriptSuccess_L("identical(array(1:3,c(3,1,1)) + 1, array(2:4, c(3,1,1)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:3,c(1,3,1)) + 1, array(2:4, c(1,3,1)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:3,c(1,1,3)) + 1, array(2:4, c(1,1,3)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6,c(3,2,1)) + 1, array(2:7, c(3,2,1)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6,c(3,1,2)) + 1, array(2:7, c(3,1,2)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6,c(2,3,1)) + 1, array(2:7, c(2,3,1)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6,c(1,3,2)) + 1, array(2:7, c(1,3,2)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6,c(2,1,3)) + 1, array(2:7, c(2,1,3)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6,c(1,2,3)) + 1, array(2:7, c(1,2,3)));", true);
	
	EidosAssertScriptRaise("identical(integer(0) + 1:3, integer(0));", 21, "requires that either");
	EidosAssertScriptSuccess_L("identical(2 + 1:3, 3:5);", true);
	EidosAssertScriptSuccess_L("identical(1:3 + 1:3, (1:3)*2);", true);
	EidosAssertScriptSuccess_L("identical(matrix(2) + 1:3, 3:5);", true);
	EidosAssertScriptSuccess_L("identical(array(2,c(1,1,1)) + 1:3, 3:5);", true);
	EidosAssertScriptSuccess_L("identical(matrix(1:3,nrow=1) + 1:3, matrix((1:3)*2, nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(matrix(1:3,ncol=1) + 1:3, matrix((1:3)*2, ncol=1));", true);
	EidosAssertScriptSuccess_L("identical(matrix(1:6,ncol=2) + 1:6, matrix((1:6)*2, ncol=2));", true);
	EidosAssertScriptSuccess_L("identical(array(1:3,c(3,1,1)) + 1:3, array((1:3)*2, c(3,1,1)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:3,c(1,3,1)) + 1:3, array((1:3)*2, c(1,3,1)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:3,c(1,1,3)) + 1:3, array((1:3)*2, c(1,1,3)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6,c(3,2,1)) + 1:6, array((1:6)*2, c(3,2,1)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6,c(3,1,2)) + 1:6, array((1:6)*2, c(3,1,2)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6,c(2,3,1)) + 1:6, array((1:6)*2, c(2,3,1)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6,c(1,3,2)) + 1:6, array((1:6)*2, c(1,3,2)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6,c(2,1,3)) + 1:6, array((1:6)*2, c(2,1,3)));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6,c(1,2,3)) + 1:6, array((1:6)*2, c(1,2,3)));", true);
	
	EidosAssertScriptSuccess_L("identical(integer(0) + matrix(1), integer(0));", true);
	EidosAssertScriptSuccess_L("identical(2 + matrix(1), matrix(3));", true);
	EidosAssertScriptSuccess_L("identical(1:3 + matrix(1), 2:4);", true);
	EidosAssertScriptSuccess_L("identical(matrix(2) + matrix(1), matrix(3));", true);
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
	
	EidosAssertScriptSuccess_L("identical(integer(0) + array(1,c(1,1,1)), integer(0));", true);
	EidosAssertScriptSuccess_L("identical(2 + array(1,c(1,1,1)), array(3, c(1,1,1)));", true);
	EidosAssertScriptSuccess_L("identical(1:3 + array(1,c(1,1,1)), 2:4);", true);
	EidosAssertScriptRaise("identical(matrix(2) + array(1,c(1,1,1)), matrix(3));", 20, "non-conformable");
	EidosAssertScriptSuccess_L("identical(array(2,c(1,1,1)) + array(1,c(1,1,1)), array(3, c(1,1,1)));", true);
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
	EidosAssertScriptSuccess_L("identical(2 + matrix(1:3,nrow=1), matrix(3:5, nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(1:3 + matrix(1:3,nrow=1), matrix((1:3)*2, nrow=1));", true);
	EidosAssertScriptRaise("identical(matrix(2) + matrix(1:3,nrow=1), matrix(3));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(array(2,c(1,1,1)) + matrix(1:3,nrow=1), array(3, c(1,1,1)));", 28, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(1:3,nrow=1) + matrix(1:3,nrow=1), matrix((1:3)*2, nrow=1));", true);
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
	EidosAssertScriptSuccess_L("identical(2 + matrix(1:3,ncol=1), matrix(3:5, ncol=1));", true);
	EidosAssertScriptSuccess_L("identical(1:3 + matrix(1:3,ncol=1), matrix((1:3)*2, ncol=1));", true);
	EidosAssertScriptRaise("identical(matrix(2) + matrix(1:3,ncol=1), matrix(3));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(array(2,c(1,1,1)) + matrix(1:3,ncol=1), array(3, c(1,1,1)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) + matrix(1:3,ncol=1), matrix(2:4, nrow=1));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(1:3,ncol=1) + matrix(1:3,ncol=1), matrix((1:3)*2, ncol=1));", true);
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
	EidosAssertScriptSuccess_L("identical(2 + matrix(1:6,ncol=2), matrix(3:8, ncol=2));", true);
	EidosAssertScriptSuccess_L("identical(1:6 + matrix(1:6,ncol=2), matrix((1:6)*2, ncol=2));", true);
	EidosAssertScriptRaise("identical(matrix(2) + matrix(1:6,ncol=2), matrix(3));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(array(2,c(1,1,1)) + matrix(1:6,ncol=2), array(3, c(1,1,1)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,nrow=1) + matrix(1:6,ncol=2), matrix(2:4, nrow=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=1) + matrix(1:6,ncol=2), matrix(2:4, ncol=1));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(1:6,ncol=2) + matrix(1:6,ncol=2), matrix((1:6)*2, ncol=2));", true);
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
	EidosAssertScriptSuccess_L("identical(2 + array(1:6,c(3,2,1)), array(3:8, c(3,2,1)));", true);
	EidosAssertScriptSuccess_L("identical(1:6 + array(1:6,c(3,2,1)), array((1:6)*2, c(3,2,1)));", true);
	EidosAssertScriptRaise("identical(matrix(2) + array(1:6,c(3,2,1)), matrix(3));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(array(2,c(1,1,1)) + array(1:6,c(3,2,1)), array(3, c(1,1,1)));", 28, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,nrow=1) + array(1:6,c(3,2,1)), matrix(2:4, nrow=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=1) + array(1:6,c(3,2,1)), matrix(2:4, ncol=1));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:6,ncol=2) + array(1:6,c(3,2,1)), matrix((1:6)*2, ncol=2));", 29, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(3,1,1)) + array(1:6,c(3,2,1)), array(2:4, c(3,1,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,3,1)) + array(1:6,c(3,2,1)), array(2:4, c(1,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:3,c(1,1,3)) + array(1:6,c(3,2,1)), array(2:4, c(1,1,3)));", 30, "non-conformable");
	EidosAssertScriptSuccess_L("identical(array(1:6,c(3,2,1)) + array(1:6,c(3,2,1)), array((1:6)*2, c(3,2,1)));", true);
	EidosAssertScriptRaise("identical(array(1:6,c(3,1,2)) + array(1:6,c(3,2,1)), array(2:7, c(3,1,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,3,1)) + array(1:6,c(3,2,1)), array(2:7, c(2,3,1)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,3,2)) + array(1:6,c(3,2,1)), array(2:7, c(1,3,2)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(2,1,3)) + array(1:6,c(3,2,1)), array(2:7, c(2,1,3)));", 30, "non-conformable");
	EidosAssertScriptRaise("identical(array(1:6,c(1,2,3)) + array(1:6,c(3,2,1)), array(2:7, c(1,2,3)));", 30, "non-conformable");
}

#pragma mark operator -
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
	EidosAssertScriptSuccess_I("1--1;", 2);
	EidosAssertScriptSuccess_IV("(0:2)-10;", {-10, -9, -8});
	EidosAssertScriptSuccess_IV("10-(0:2);", {10, 9, 8});
	EidosAssertScriptSuccess_IV("(15:13)-(0:2);", {15, 13, 11});
	EidosAssertScriptRaise("(15:12)-(0:2);", 7, "operator requires that either");
	EidosAssertScriptSuccess_F("1-1.0;", 0);
	EidosAssertScriptSuccess_F("1.0-1;", 0);
	EidosAssertScriptSuccess_F("1.0--1.0;", 2);
	EidosAssertScriptSuccess_FV("(0:2.0)-10;", {-10, -9, -8});
	EidosAssertScriptSuccess_FV("10.0-(0:2);", {10, 9, 8});
	EidosAssertScriptSuccess_FV("10-(0.0:2);", {10, 9, 8});
	EidosAssertScriptSuccess_FV("(15.0:13)-(0:2.0);", {15, 13, 11});
	EidosAssertScriptRaise("(15:12.0)-(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("'foo'-1;", 5, "is not supported by");
	EidosAssertScriptRaise("T-F;", 1, "is not supported by");
	EidosAssertScriptRaise("T-T;", 1, "is not supported by");
	EidosAssertScriptRaise("F-F;", 1, "is not supported by");
	EidosAssertScriptSuccess_I("-5;", -5);
	EidosAssertScriptSuccess_F("-5.0;", -5);
	EidosAssertScriptSuccess_IV("-c(5, -6);", {-5, 6});
	EidosAssertScriptSuccess_FV("-c(5.0, -6.0);", {-5, 6});
	EidosAssertScriptRaise("-'foo';", 0, "is not supported by");
	EidosAssertScriptRaise("-T;", 0, "is not supported by");
	EidosAssertScriptSuccess_I("3-4-5;", -6);
	EidosAssertScriptSuccess("3.2-NAN-4.5;", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess_FV("3.5-c(5.5,NAN,2.5);", {-2.0, std::numeric_limits<double>::quiet_NaN(), 1.0});
	EidosAssertScriptSuccess_FV("c(5.5,NAN,2.5)-3.5;", {2.0, std::numeric_limits<double>::quiet_NaN(), -1.0});
	EidosAssertScriptSuccess_FV("c(5.5,NAN,2.5)-c(5.5,3.5,NAN);", {0.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
	
	// operator -: raise on integer subtraction overflow for all code paths
	EidosAssertScriptSuccess_I("9223372036854775807;", INT64_MAX);
	EidosAssertScriptSuccess_I("-9223372036854775807 - 1;", INT64_MIN);
	EidosAssertScriptSuccess_I("-5e18;", -5000000000000000000LL);
#if EIDOS_HAS_OVERFLOW_BUILTINS
	EidosAssertScriptRaise("-(-9223372036854775807 - 1);", 0, "overflow with the unary");
	EidosAssertScriptRaise("-c(-9223372036854775807 - 1, 10);", 0, "overflow with the unary");
	EidosAssertScriptRaise("-5e18 - 5e18;", 6, "overflow with the binary");
	EidosAssertScriptRaise("-5e18 - c(0, 0, 5e18, 0);", 6, "overflow with the binary");
	EidosAssertScriptRaise("c(0, 0, -5e18, 0) - 5e18;", 18, "overflow with the binary");
	EidosAssertScriptRaise("c(0, 0, -5e18, 0) - c(0, 0, 5e18, 0);", 18, "overflow with the binary");
#endif
	
	// operator -: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess_L("identical(-matrix(2), matrix(-2));", true);
	EidosAssertScriptSuccess_L("identical(-matrix(1:3), matrix(-1:-3));", true);
	EidosAssertScriptSuccess_L("identical(-array(2, c(1,1,1)), array(-2, c(1,1,1)));", true);
	EidosAssertScriptSuccess_L("identical(-array(1:6, c(3,1,2)), array(-1:-6, c(3,1,2)));", true);
	
	EidosAssertScriptSuccess_L("identical(1-matrix(2), matrix(-1));", true);
	EidosAssertScriptSuccess_L("identical(1-matrix(1:3), matrix(0:-2));", true);
	EidosAssertScriptSuccess_L("identical(1:3-matrix(2), -1:1);", true);
	EidosAssertScriptSuccess_L("identical(4:6-matrix(1:3), matrix(c(3,3,3)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(5)-matrix(2), matrix(3));", true);
	EidosAssertScriptRaise("identical(matrix(1:3)-matrix(2), matrix(3));", 21, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1)-matrix(1:3,ncol=1), matrix(3));", 28, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(7:9)-matrix(1:3), matrix(c(6,6,6)));", true);
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
    EidosAssertScriptSuccess_I("1*-1;", -1);
	EidosAssertScriptSuccess_IV("(0:2)*10;", {0, 10, 20});
	EidosAssertScriptSuccess_IV("10*(0:2);", {0, 10, 20});
	EidosAssertScriptSuccess_IV("(15:13)*(0:2);", {0, 14, 26});
	EidosAssertScriptRaise("(15:12)*(0:2);", 7, "operator requires that either");
    EidosAssertScriptSuccess_F("1*1.0;", 1);
    EidosAssertScriptSuccess_F("1.0*1;", 1);
    EidosAssertScriptSuccess_F("1.0*-1.0;", -1);
	EidosAssertScriptSuccess_FV("(0:2.0)*10;", {0, 10, 20});
	EidosAssertScriptSuccess_FV("10.0*(0:2);", {0, 10, 20});
	EidosAssertScriptSuccess_FV("(15.0:13)*(0:2.0);", {0, 14, 26});
	EidosAssertScriptRaise("(15:12.0)*(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("'foo'*5;", 5, "is not supported by");
	EidosAssertScriptRaise("T*F;", 1, "is not supported by");
	EidosAssertScriptRaise("T*T;", 1, "is not supported by");
	EidosAssertScriptRaise("F*F;", 1, "is not supported by");
	EidosAssertScriptRaise("*5;", 0, "unexpected token");
	EidosAssertScriptRaise("*5.0;", 0, "unexpected token");
	EidosAssertScriptRaise("*'foo';", 0, "unexpected token");
	EidosAssertScriptRaise("*T;", 0, "unexpected token");
    EidosAssertScriptSuccess_I("3*4*5;", 60);
	EidosAssertScriptSuccess("3.0*NAN*4.5;", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess_FV("3.0*c(5.5,NAN,2.5);", {16.5, std::numeric_limits<double>::quiet_NaN(), 7.5});
	EidosAssertScriptSuccess_FV("c(5.5,NAN,2.5)*3.0;", {16.5, std::numeric_limits<double>::quiet_NaN(), 7.5});
	EidosAssertScriptSuccess_FV("c(5.5,NAN,2.5)*c(5.0,3.5,NAN);", {27.5, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
	
	// operator *: raise on integer multiplication overflow for all code paths
	EidosAssertScriptSuccess_I("5e18;", 5000000000000000000LL);
	EidosAssertScriptRaise("1e19;", 0, "could not be represented");
#if EIDOS_HAS_OVERFLOW_BUILTINS
	EidosAssertScriptRaise("5e18 * 2;", 5, "multiplication overflow");
	EidosAssertScriptRaise("5e18 * c(0, 0, 2, 0);", 5, "multiplication overflow");
	EidosAssertScriptRaise("c(0, 0, 2, 0) * 5e18;", 14, "multiplication overflow");
	EidosAssertScriptRaise("c(0, 0, 2, 0) * c(0, 0, 5e18, 0);", 14, "multiplication overflow");
	EidosAssertScriptRaise("c(0, 0, 5e18, 0) * c(0, 0, 2, 0);", 17, "multiplication overflow");
#endif
	
	// operator *: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess_L("identical(5 * matrix(2), matrix(10));", true);
	EidosAssertScriptSuccess_L("identical(5 * matrix(1:3), matrix(c(5,10,15)));", true);
	EidosAssertScriptSuccess_L("identical(1:3 * matrix(2), c(2,4,6));", true);
	EidosAssertScriptSuccess_L("identical(4:6 * matrix(1:3), matrix(c(4,10,18)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(5) * matrix(2), matrix(10));", true);
	EidosAssertScriptRaise("identical(matrix(1:3) * matrix(2), matrix(c(2,4,6)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(4:6,nrow=1) * matrix(1:3,ncol=1), matrix(c(4,10,18)));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(6:8) * matrix(1:3), matrix(c(6,14,24)));", true);
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
    EidosAssertScriptSuccess_F("1/1;", 1);
    EidosAssertScriptSuccess_F("1/-1;", -1);
	EidosAssertScriptSuccess_FV("(0:2)/10;", {0, 0.1, 0.2});
	EidosAssertScriptRaise("(15:12)/(0:2);", 7, "operator requires that either");
    EidosAssertScriptSuccess_F("1/1.0;", 1);
    EidosAssertScriptSuccess_F("1.0/1;", 1);
    EidosAssertScriptSuccess_F("1.0/-1.0;", -1);
	EidosAssertScriptSuccess_FV("(0:2.0)/10;", {0, 0.1, 0.2});
	EidosAssertScriptSuccess_FV("10.0/(0:2);", {std::numeric_limits<double>::infinity(), 10, 5});
	EidosAssertScriptSuccess_FV("10/(0.0:2);", {std::numeric_limits<double>::infinity(), 10, 5});
	EidosAssertScriptSuccess_FV("(15.0:13)/(0:2.0);", {std::numeric_limits<double>::infinity(), 14, 6.5});
	EidosAssertScriptSuccess_F("1.0/0.0;", std::numeric_limits<double>::infinity());
	EidosAssertScriptSuccess_F("1.0/-0.0;", -std::numeric_limits<double>::infinity());	// signed zeros as per IEEE 754
	EidosAssertScriptSuccess_F("0.0/0.0;", std::numeric_limits<double>::quiet_NaN());
	EidosAssertScriptSuccess_F("INF/INF;", std::numeric_limits<double>::quiet_NaN());
	EidosAssertScriptRaise("(15:12.0)/(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("'foo'/5;", 5, "is not supported by");
	EidosAssertScriptRaise("T/F;", 1, "is not supported by");
	EidosAssertScriptRaise("T/T;", 1, "is not supported by");
	EidosAssertScriptRaise("F/F;", 1, "is not supported by");
	EidosAssertScriptRaise("/5;", 0, "unexpected token");
	EidosAssertScriptRaise("/5.0;", 0, "unexpected token");
	EidosAssertScriptRaise("/'foo';", 0, "unexpected token");
	EidosAssertScriptRaise("/T;", 0, "unexpected token");
    EidosAssertScriptSuccess_F("3/4/5;", 0.15);
	EidosAssertScriptSuccess("6/0;", gStaticEidosValue_FloatINF);
	EidosAssertScriptSuccess("3.0/NAN/4.5;", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess_FV("2.0/c(5.0,NAN,2.5);", {0.4, std::numeric_limits<double>::quiet_NaN(), 0.8});
	EidosAssertScriptSuccess_FV("c(5.0,NAN,2.5)/2.0;", {2.5, std::numeric_limits<double>::quiet_NaN(), 1.25});
	EidosAssertScriptSuccess_FV("c(5.0,NAN,2.5)/c(5.0,3.5,NAN);", {1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
	
	// operator /: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess_L("identical(5 / matrix(2), matrix(2.5));", true);
	EidosAssertScriptSuccess_L("identical(12 / matrix(1:3), matrix(c(12.0,6,4)));", true);
	EidosAssertScriptSuccess_L("identical(1:3 / matrix(2), c(0.5,1,1.5));", true);
	EidosAssertScriptSuccess_L("identical(4:6 / matrix(1:3), matrix(c(4,2.5,2)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(5) / matrix(2), matrix(2.5));", true);
	EidosAssertScriptRaise("identical(matrix(1:3) / matrix(2), matrix(c(0.5,1,1.5)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(4:6,nrow=1) / matrix(1:3,ncol=1), matrix(c(4,2.5,2)));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(7:9) / matrix(1:3), matrix(c(7.0,4,3)));", true);
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
    EidosAssertScriptSuccess_F("1%1;", 0);
    EidosAssertScriptSuccess_F("1%-1;", 0);
	EidosAssertScriptSuccess_FV("(0:2)%10;", {0, 1, 2});
	EidosAssertScriptRaise("(15:12)%(0:2);", 7, "operator requires that either");
    EidosAssertScriptSuccess_F("1%1.0;", 0);
    EidosAssertScriptSuccess_F("1.0%1;", 0);
    EidosAssertScriptSuccess_F("1.0%-1.0;", 0);
	EidosAssertScriptSuccess_FV("(0:2.0)%10;", {0, 1, 2});
	EidosAssertScriptSuccess_FV("10.0%(0:4);", {std::numeric_limits<double>::quiet_NaN(), 0, 0, 1, 2});
	EidosAssertScriptSuccess_FV("10%(0.0:4);", {std::numeric_limits<double>::quiet_NaN(), 0, 0, 1, 2});
	EidosAssertScriptSuccess_FV("(15.0:13)%(0:2.0);", {std::numeric_limits<double>::quiet_NaN(), 0, 1});
	EidosAssertScriptRaise("(15:12.0)%(0:2);", 9, "operator requires that either");
	EidosAssertScriptRaise("'foo'%5;", 5, "is not supported by");
	EidosAssertScriptRaise("T%F;", 1, "is not supported by");
	EidosAssertScriptRaise("T%T;", 1, "is not supported by");
	EidosAssertScriptRaise("F%F;", 1, "is not supported by");
	EidosAssertScriptRaise("%5;", 0, "unexpected token");
	EidosAssertScriptRaise("%5.0;", 0, "unexpected token");
	EidosAssertScriptRaise("%'foo';", 0, "unexpected token");
	EidosAssertScriptRaise("%T;", 0, "unexpected token");
    EidosAssertScriptSuccess_F("3%4%5;", 3);
	EidosAssertScriptSuccess("3.0%NAN%4.5;", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess_FV("2.0%c(5.0,NAN,2.5);", {2.0, std::numeric_limits<double>::quiet_NaN(), 2.0});
	EidosAssertScriptSuccess_FV("c(5.0,NAN,2.5)%2.0;", {1.0, std::numeric_limits<double>::quiet_NaN(), 0.5});
	EidosAssertScriptSuccess_FV("c(6.0,NAN,2.5)%c(5.0,3.5,NAN);", {1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
	
	// operator %: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess_L("identical(5 % matrix(2), matrix(1.0));", true);
	EidosAssertScriptSuccess_L("identical(5 % matrix(1:3), matrix(c(0.0,1,2)));", true);
	EidosAssertScriptSuccess_L("identical(1:3 % matrix(2), c(1.0,0,1));", true);
	EidosAssertScriptSuccess_L("identical(4:6 % matrix(1:3), matrix(c(0.0,1,0)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(5) % matrix(2), matrix(1.0));", true);
	EidosAssertScriptRaise("identical(matrix(1:3) % matrix(2), matrix(c(1.0,0,1)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(4:6,nrow=1) % matrix(1:3,ncol=1), matrix(c(0.0,1,0)));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(6:8) % matrix(1:3), matrix(c(0.0,1,2)));", true);
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
	EidosAssertScriptSuccess_IV("1:5;", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess_IV("5:1;", {5, 4, 3, 2, 1});
	EidosAssertScriptSuccess_IV("-2:1;", {-2, -1, 0, 1});
	EidosAssertScriptSuccess_IV("1:-2;", {1, 0, -1, -2});
	EidosAssertScriptSuccess("1:1;", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess_FV("1.0:5;", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess_FV("5.0:1;", {5, 4, 3, 2, 1});
	EidosAssertScriptSuccess_FV("-2.0:1;", {-2, -1, 0, 1});
	EidosAssertScriptSuccess_FV("1.0:-2;", {1, 0, -1, -2});
	EidosAssertScriptSuccess_F("1.0:1;", 1);
	EidosAssertScriptSuccess_FV("1:5.0;", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess_FV("5:1.0;", {5, 4, 3, 2, 1});
	EidosAssertScriptSuccess_FV("-2:1.0;", {-2, -1, 0, 1});
	EidosAssertScriptSuccess_FV("1:-2.0;", {1, 0, -1, -2});
	EidosAssertScriptSuccess_F("1:1.0;", 1);
	EidosAssertScriptRaise("1:F;", 1, "is not supported by");
	EidosAssertScriptRaise("F:1;", 1, "is not supported by");
	EidosAssertScriptRaise("T:F;", 1, "is not supported by");
	EidosAssertScriptRaise("'a':'z';", 3, "is not supported by");
	EidosAssertScriptRaise("1:(2:3);", 1, "operator must have size()");
	EidosAssertScriptRaise("(1:2):3;", 5, "operator must have size()");
	EidosAssertScriptSuccess_FV("1.5:4.7;", {1.5, 2.5, 3.5, 4.5});
	EidosAssertScriptSuccess_FV("1.5:-2.7;", {1.5, 0.5, -0.5, -1.5, -2.5});
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
	EidosAssertScriptSuccess_F("1^1;", 1);
	EidosAssertScriptSuccess_F("1^-1;", 1);
	EidosAssertScriptSuccess_FV("(0:2)^10;", {0, 1, 1024});
	EidosAssertScriptSuccess_FV("10^(0:2);", {1, 10, 100});
	EidosAssertScriptSuccess_FV("(15:13)^(0:2);", {1, 14, 169});
	EidosAssertScriptRaise("(15:12)^(0:2);", 7, "operator requires that either");
	EidosAssertScriptRaise("NULL^(0:2);", 4, "is not supported by");
	EidosAssertScriptSuccess_F("1^1.0;", 1);
	EidosAssertScriptSuccess_F("1.0^1;", 1);
	EidosAssertScriptSuccess_F("1.0^-1.0;", 1);
	EidosAssertScriptSuccess_FV("(0:2.0)^10;", {0, 1, 1024});
	EidosAssertScriptSuccess_FV("10.0^(0:2);", {1, 10, 100});
	EidosAssertScriptSuccess_FV("10^(0.0:2);", {1, 10, 100});
	EidosAssertScriptSuccess_FV("(15.0:13)^(0:2.0);", {1, 14, 169});
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
	EidosAssertScriptSuccess_F("4^(3^2);", 262144);		// right-associative!
	EidosAssertScriptSuccess_F("4^3^2;", 262144);		// right-associative!
	EidosAssertScriptSuccess("3.0^NAN^4.5;", gStaticEidosValue_FloatNAN);
	EidosAssertScriptSuccess_FV("4.0^c(5.0,NAN,2.5);", {1024.0, std::numeric_limits<double>::quiet_NaN(), 32.0});
	EidosAssertScriptSuccess_FV("c(5.0,NAN,2.5)^2.0;", {25.0, std::numeric_limits<double>::quiet_NaN(), 6.25});
	EidosAssertScriptSuccess_FV("c(6.0,NAN,2.5)^c(5.0,3.5,NAN);", {7776.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()});
	
	// operator ^: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess_L("identical(5 ^ matrix(2), matrix(25.0));", true);
	EidosAssertScriptSuccess_L("identical(2 ^ matrix(1:3), matrix(c(2.0,4,8)));", true);
	EidosAssertScriptSuccess_L("identical((1:3) ^ matrix(2), c(1.0,4,9));", true);
	EidosAssertScriptSuccess_L("identical((2:4) ^ matrix(1:3), matrix(c(2.0,9,64)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(5) ^ matrix(2), matrix(25.0));", true);
	EidosAssertScriptRaise("identical(matrix(1:3) ^ matrix(2), matrix(c(1.0,4,9)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(2:4,nrow=1) ^ matrix(1:3,ncol=1), matrix(c(2.0,9,64)));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(2:4) ^ matrix(1:3), matrix(c(2.0,9,64)));", true);
	
	// operator ^ precedence and associativity tests
	EidosAssertScriptSuccess_F("-2^2;", -4);
	EidosAssertScriptSuccess_FV("x=1:3; y=1:3; -x^y;", {-1, -4, -27});
	EidosAssertScriptSuccess_F("-2.0^2;", -4);
	EidosAssertScriptSuccess_F("-2^2.0;", -4);
	EidosAssertScriptSuccess_F("-2.0^2.0;", -4);
	EidosAssertScriptSuccess_FV("x=1.0:3; y=1:3; -x^y;", {-1, -4, -27});
	EidosAssertScriptSuccess_FV("x=1:3; y=1.0:3; -x^y;", {-1, -4, -27});
	EidosAssertScriptSuccess_FV("x=1.0:3; y=1.0:3; -x^y;", {-1, -4, -27});
	EidosAssertScriptSuccess_F("2^2^4;", 65536);
	EidosAssertScriptSuccess_F("1/(2^-2^4);", 65536);
}













































