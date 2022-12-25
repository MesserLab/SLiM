//
//  eidos_test_operators_comparison.cpp
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
	EidosAssertScriptSuccess_L("T > F;", true);
	EidosAssertScriptSuccess_L("T > T;", false);
	EidosAssertScriptSuccess_L("F > T;", false);
	EidosAssertScriptSuccess_L("F > F;", false);
	EidosAssertScriptSuccess_L("T > 0;", true);
	EidosAssertScriptSuccess_L("T > 1;", false);
	EidosAssertScriptSuccess_L("F > 0;", false);
	EidosAssertScriptSuccess_L("F > 1;", false);
	EidosAssertScriptSuccess_L("T > -5;", true);
	EidosAssertScriptSuccess_L("-5 > T;", false);
	EidosAssertScriptSuccess_L("T > 5;", false);
	EidosAssertScriptSuccess_L("5 > T;", true);
	EidosAssertScriptSuccess_L("T > -5.0;", true);
	EidosAssertScriptSuccess_L("-5.0 > T;", false);
	EidosAssertScriptSuccess_L("T > 5.0;", false);
	EidosAssertScriptSuccess_L("5.0 > T;", true);
	EidosAssertScriptSuccess_L("T > 'FOO';", true);
	EidosAssertScriptSuccess_L("'FOO' > T;", false);
	EidosAssertScriptSuccess_L("T > 'XYZZY';", false);
	EidosAssertScriptSuccess_L("'XYZZY' > T;", true);
	EidosAssertScriptSuccess_L("5 > -10;", true);
	EidosAssertScriptSuccess_L("-10 > 5;", false);
	EidosAssertScriptSuccess_L("5.0 > -10;", true);
	EidosAssertScriptSuccess_L("-10 > 5.0;", false);
	EidosAssertScriptSuccess_L("5 > -10.0;", true);
	EidosAssertScriptSuccess_L("-10.0 > 5;", false);
	EidosAssertScriptSuccess_L("'foo' > 'bar';", true);
	EidosAssertScriptSuccess_L("'bar' > 'foo';", false);
	EidosAssertScriptSuccess_L("120 > '10';", true);
	EidosAssertScriptSuccess_L("10 > '120';", false);
	EidosAssertScriptSuccess_L("120 > '15';", false);
	EidosAssertScriptSuccess_L("15 > '120';", true);
	EidosAssertScriptRaise("_Test(9) > 5;", 9, "cannot be used with type");
	EidosAssertScriptRaise("5 > _Test(9);", 2, "cannot be used with type");
	EidosAssertScriptSuccess_L("5 > 5;", false);
	EidosAssertScriptSuccess_L("-10.0 > -10.0;", false);
	EidosAssertScriptSuccess_L("5 > 5.0;", false);
	EidosAssertScriptSuccess_L("5.0 > 5;", false);
	EidosAssertScriptSuccess_L("5 > '5';", false);
	EidosAssertScriptSuccess_L("'5' > 5;", false);
	EidosAssertScriptSuccess_L("'foo' > 'foo';", false);
	EidosAssertScriptRaise("_Test(9) > _Test(9);", 9, "cannot be used with type");
	
	EidosAssertScriptSuccess_LV("T > c(T, F);", {false, true});
	EidosAssertScriptSuccess_LV("5 > c(5, 6);", {false, false});
	EidosAssertScriptSuccess_LV("5.0 > c(5.0, 6.0);", {false, false});
	EidosAssertScriptSuccess_LV("'foo' > c('foo', 'bar');", {false, true});
	
	EidosAssertScriptSuccess_LV("c(T, F) > T;", {false, false});
	EidosAssertScriptSuccess_LV("c(5, 6) > 5;", {false, true});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0) > 5.0;", {false, true});
	EidosAssertScriptSuccess_LV("c('foo', 'bar') > 'foo';", {false, false});
	
	EidosAssertScriptSuccess_LV("c(T, F) > c(T, T);", {false, false});
	EidosAssertScriptSuccess_LV("c(5, 6) > c(5, 8);", {false, false});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0) > c(5.0, 8.0);", {false, false});
	EidosAssertScriptSuccess_LV("c('foo', 'bar') > c('foo', 'baz');", {false, false});
	
	EidosAssertScriptSuccess_L("NAN > NAN;", false);
	EidosAssertScriptSuccess_L("NAN > 5.0;", false);
	EidosAssertScriptSuccess_L("5.0 > NAN;", false);
	EidosAssertScriptSuccess_LV("c(5.0, 6.0, NAN) > c(5.0, 5.0, 5.0);", {false, true, false});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0, 8.0) > c(5.0, 5.0, NAN);", {false, true, false});
	
	EidosAssertScriptRaise("c(5,6) > c(5,6,7);", 7, "operator requires that either");
	
	// operator >: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess_L("identical(4 > 5, F);", true);
	EidosAssertScriptSuccess_L("identical(5 > 5, F);", true);
	EidosAssertScriptSuccess_L("identical(6 > 5, T);", true);
	EidosAssertScriptSuccess_L("identical(4 > matrix(5), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(5 > matrix(5), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(6 > matrix(5), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(2 > matrix(1:3), matrix(c(T,F,F)));", true);
	EidosAssertScriptSuccess_L("identical((1:3) > matrix(2), c(F,F,T));", true);
	EidosAssertScriptSuccess_L("identical((1:3) > matrix(3:1), matrix(c(F,F,T)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(4) > matrix(5), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(matrix(5) > matrix(5), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(matrix(6) > matrix(5), matrix(T));", true);
	EidosAssertScriptRaise("identical(matrix(1:3) > matrix(2), matrix(c(F,F,T)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) > matrix(3:1,ncol=1), matrix(c(F,F,T)));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(1:3) > matrix(3:1), matrix(c(F,F,T)));", true);
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
	EidosAssertScriptSuccess_L("T < F;", false);
	EidosAssertScriptSuccess_L("T < T;", false);
	EidosAssertScriptSuccess_L("F < T;", true);
	EidosAssertScriptSuccess_L("F < F;", false);
	EidosAssertScriptSuccess_L("T < 0;", false);
	EidosAssertScriptSuccess_L("T < 1;", false);
	EidosAssertScriptSuccess_L("F < 0;", false);
	EidosAssertScriptSuccess_L("F < 1;", true);
	EidosAssertScriptSuccess_L("T < -5;", false);
	EidosAssertScriptSuccess_L("-5 < T;", true);
	EidosAssertScriptSuccess_L("T < 5;", true);
	EidosAssertScriptSuccess_L("5 < T;", false);
	EidosAssertScriptSuccess_L("T < -5.0;", false);
	EidosAssertScriptSuccess_L("-5.0 < T;", true);
	EidosAssertScriptSuccess_L("T < 5.0;", true);
	EidosAssertScriptSuccess_L("5.0 < T;", false);
	EidosAssertScriptSuccess_L("T < 'FOO';", false);
	EidosAssertScriptSuccess_L("'FOO' < T;", true);
	EidosAssertScriptSuccess_L("T < 'XYZZY';", true);
	EidosAssertScriptSuccess_L("'XYZZY' < T;", false);
	EidosAssertScriptSuccess_L("5 < -10;", false);
	EidosAssertScriptSuccess_L("-10 < 5;", true);
	EidosAssertScriptSuccess_L("5.0 < -10;", false);
	EidosAssertScriptSuccess_L("-10 < 5.0;", true);
	EidosAssertScriptSuccess_L("5 < -10.0;", false);
	EidosAssertScriptSuccess_L("-10.0 < 5;", true);
	EidosAssertScriptSuccess_L("'foo' < 'bar';", false);
	EidosAssertScriptSuccess_L("'bar' < 'foo';", true);
	EidosAssertScriptSuccess_L("120 < '10';", false);
	EidosAssertScriptSuccess_L("10 < '120';", true);
	EidosAssertScriptSuccess_L("120 < '15';", true);
	EidosAssertScriptSuccess_L("15 < '120';", false);
	EidosAssertScriptRaise("_Test(9) < 5;", 9, "cannot be used with type");
	EidosAssertScriptRaise("5 < _Test(9);", 2, "cannot be used with type");
	EidosAssertScriptSuccess_L("5 < 5;", false);
	EidosAssertScriptSuccess_L("-10.0 < -10.0;", false);
	EidosAssertScriptSuccess_L("5 < 5.0;", false);
	EidosAssertScriptSuccess_L("5.0 < 5;", false);
	EidosAssertScriptSuccess_L("5 < '5';", false);
	EidosAssertScriptSuccess_L("'5' < 5;", false);
	EidosAssertScriptSuccess_L("'foo' < 'foo';", false);
	EidosAssertScriptRaise("_Test(9) < _Test(9);", 9, "cannot be used with type");
	
	EidosAssertScriptSuccess_LV("T < c(T, F);", {false, false});
	EidosAssertScriptSuccess_LV("5 < c(5, 6);", {false, true});
	EidosAssertScriptSuccess_LV("5.0 < c(5.0, 6.0);", {false, true});
	EidosAssertScriptSuccess_LV("'foo' < c('foo', 'bar');", {false, false});
	
	EidosAssertScriptSuccess_LV("c(T, F) < T;", {false, true});
	EidosAssertScriptSuccess_LV("c(5, 6) < 5;", {false, false});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0) < 5.0;", {false, false});
	EidosAssertScriptSuccess_LV("c('foo', 'bar') < 'foo';", {false, true});
	
	EidosAssertScriptSuccess_LV("c(T, F) < c(T, T);", {false, true});
	EidosAssertScriptSuccess_LV("c(5, 6) < c(5, 8);", {false, true});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0) < c(5.0, 8.0);", {false, true});
	EidosAssertScriptSuccess_LV("c('foo', 'bar') < c('foo', 'baz');", {false, true});
	
	EidosAssertScriptSuccess_L("NAN < NAN;", false);
	EidosAssertScriptSuccess_L("NAN < 5.0;", false);
	EidosAssertScriptSuccess_L("5.0 < NAN;", false);
	EidosAssertScriptSuccess_LV("c(5.0, 6.0, NAN) < c(5.0, 5.0, 5.0);", {false, false, false});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0, 8.0) < c(5.0, 5.0, NAN);", {false, false, false});
	
	EidosAssertScriptRaise("c(5,6) < c(5,6,7);", 7, "operator requires that either");
	
	// operator <: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess_L("identical(4 < 5, T);", true);
	EidosAssertScriptSuccess_L("identical(5 < 5, F);", true);
	EidosAssertScriptSuccess_L("identical(6 < 5, F);", true);
	EidosAssertScriptSuccess_L("identical(4 < matrix(5), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(5 < matrix(5), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(6 < matrix(5), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(2 < matrix(1:3), matrix(c(F,F,T)));", true);
	EidosAssertScriptSuccess_L("identical((1:3) < matrix(2), c(T,F,F));", true);
	EidosAssertScriptSuccess_L("identical((1:3) < matrix(3:1), matrix(c(T,F,F)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(4) < matrix(5), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(5) < matrix(5), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(matrix(6) < matrix(5), matrix(F));", true);
	EidosAssertScriptRaise("identical(matrix(1:3) < matrix(2), matrix(c(T,F,F)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) < matrix(3:1,ncol=1), matrix(c(T,F,F)));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(1:3) < matrix(3:1), matrix(c(T,F,F)));", true);
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
	EidosAssertScriptSuccess_L("T >= F;", true);
	EidosAssertScriptSuccess_L("T >= T;", true);
	EidosAssertScriptSuccess_L("F >= T;", false);
	EidosAssertScriptSuccess_L("F >= F;", true);
	EidosAssertScriptSuccess_L("T >= 0;", true);
	EidosAssertScriptSuccess_L("T >= 1;", true);
	EidosAssertScriptSuccess_L("F >= 0;", true);
	EidosAssertScriptSuccess_L("F >= 1;", false);
	EidosAssertScriptSuccess_L("T >= -5;", true);
	EidosAssertScriptSuccess_L("-5 >= T;", false);
	EidosAssertScriptSuccess_L("T >= 5;", false);
	EidosAssertScriptSuccess_L("5 >= T;", true);
	EidosAssertScriptSuccess_L("T >= -5.0;", true);
	EidosAssertScriptSuccess_L("-5.0 >= T;", false);
	EidosAssertScriptSuccess_L("T >= 5.0;", false);
	EidosAssertScriptSuccess_L("5.0 >= T;", true);
	EidosAssertScriptSuccess_L("T >= 'FOO';", true);
	EidosAssertScriptSuccess_L("'FOO' >= T;", false);
	EidosAssertScriptSuccess_L("T >= 'XYZZY';", false);
	EidosAssertScriptSuccess_L("'XYZZY' >= T;", true);
	EidosAssertScriptSuccess_L("5 >= -10;", true);
	EidosAssertScriptSuccess_L("-10 >= 5;", false);
	EidosAssertScriptSuccess_L("5.0 >= -10;", true);
	EidosAssertScriptSuccess_L("-10 >= 5.0;", false);
	EidosAssertScriptSuccess_L("5 >= -10.0;", true);
	EidosAssertScriptSuccess_L("-10.0 >= 5;", false);
	EidosAssertScriptSuccess_L("'foo' >= 'bar';", true);
	EidosAssertScriptSuccess_L("'bar' >= 'foo';", false);
	EidosAssertScriptSuccess_L("120 >= '10';", true);
	EidosAssertScriptSuccess_L("10 >= '120';", false);
	EidosAssertScriptSuccess_L("120 >= '15';", false);
	EidosAssertScriptSuccess_L("15 >= '120';", true);
	EidosAssertScriptRaise("_Test(9) >= 5;", 9, "cannot be used with type");
	EidosAssertScriptRaise("5 >= _Test(9);", 2, "cannot be used with type");
	EidosAssertScriptSuccess_L("5 >= 5;", true);
	EidosAssertScriptSuccess_L("-10.0 >= -10.0;", true);
	EidosAssertScriptSuccess_L("5 >= 5.0;", true);
	EidosAssertScriptSuccess_L("5.0 >= 5;", true);
	EidosAssertScriptSuccess_L("5 >= '5';", true);
	EidosAssertScriptSuccess_L("'5' >= 5;", true);
	EidosAssertScriptSuccess_L("'foo' >= 'foo';", true);
	EidosAssertScriptRaise("_Test(9) >= _Test(9);", 9, "cannot be used with type");
	
	EidosAssertScriptSuccess_LV("T >= c(T, F);", {true, true});
	EidosAssertScriptSuccess_LV("5 >= c(5, 6);", {true, false});
	EidosAssertScriptSuccess_LV("5.0 >= c(5.0, 6.0);", {true, false});
	EidosAssertScriptSuccess_LV("'foo' >= c('foo', 'bar');", {true, true});
	
	EidosAssertScriptSuccess_LV("c(T, F) >= T;", {true, false});
	EidosAssertScriptSuccess_LV("c(5, 6) >= 5;", {true, true});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0) >= 5.0;", {true, true});
	EidosAssertScriptSuccess_LV("c('foo', 'bar') >= 'foo';", {true, false});
	
	EidosAssertScriptSuccess_LV("c(T, F) >= c(T, T);", {true, false});
	EidosAssertScriptSuccess_LV("c(5, 6) >= c(5, 8);", {true, false});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0) >= c(5.0, 8.0);", {true, false});
	EidosAssertScriptSuccess_LV("c('foo', 'bar') >= c('foo', 'baz');", {true, false});
	
	EidosAssertScriptSuccess_L("NAN >= NAN;", false);
	EidosAssertScriptSuccess_L("NAN >= 5.0;", false);
	EidosAssertScriptSuccess_L("5.0 >= NAN;", false);
	EidosAssertScriptSuccess_LV("c(5.0, 6.0, NAN) >= c(5.0, 5.0, 5.0);", {true, true, false});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0, 8.0) >= c(5.0, 5.0, NAN);", {true, true, false});
	
	EidosAssertScriptRaise("c(5,6) >= c(5,6,7);", 7, "operator requires that either");
	
	// operator >=: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess_L("identical(4 >= 5, F);", true);
	EidosAssertScriptSuccess_L("identical(5 >= 5, T);", true);
	EidosAssertScriptSuccess_L("identical(6 >= 5, T);", true);
	EidosAssertScriptSuccess_L("identical(4 >= matrix(5), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(5 >= matrix(5), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(6 >= matrix(5), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(2 >= matrix(1:3), matrix(c(T,T,F)));", true);
	EidosAssertScriptSuccess_L("identical((1:3) >= matrix(2), c(F,T,T));", true);
	EidosAssertScriptSuccess_L("identical((1:3) >= matrix(3:1), matrix(c(F,T,T)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(4) >= matrix(5), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(matrix(5) >= matrix(5), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(6) >= matrix(5), matrix(T));", true);
	EidosAssertScriptRaise("identical(matrix(1:3) >= matrix(2), matrix(c(F,T,T)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) >= matrix(3:1,ncol=1), matrix(c(F,T,T)));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(1:3) >= matrix(3:1), matrix(c(F,T,T)));", true);
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
	EidosAssertScriptSuccess_L("T <= F;", false);
	EidosAssertScriptSuccess_L("T <= T;", true);
	EidosAssertScriptSuccess_L("F <= T;", true);
	EidosAssertScriptSuccess_L("F <= F;", true);
	EidosAssertScriptSuccess_L("T <= 0;", false);
	EidosAssertScriptSuccess_L("T <= 1;", true);
	EidosAssertScriptSuccess_L("F <= 0;", true);
	EidosAssertScriptSuccess_L("F <= 1;", true);
	EidosAssertScriptSuccess_L("T <= -5;", false);
	EidosAssertScriptSuccess_L("-5 <= T;", true);
	EidosAssertScriptSuccess_L("T <= 5;", true);
	EidosAssertScriptSuccess_L("5 <= T;", false);
	EidosAssertScriptSuccess_L("T <= -5.0;", false);
	EidosAssertScriptSuccess_L("-5.0 <= T;", true);
	EidosAssertScriptSuccess_L("T <= 5.0;", true);
	EidosAssertScriptSuccess_L("5.0 <= T;", false);
	EidosAssertScriptSuccess_L("T <= 'FOO';", false);
	EidosAssertScriptSuccess_L("'FOO' <= T;", true);
	EidosAssertScriptSuccess_L("T <= 'XYZZY';", true);
	EidosAssertScriptSuccess_L("'XYZZY' <= T;", false);
	EidosAssertScriptSuccess_L("5 <= -10;", false);
	EidosAssertScriptSuccess_L("-10 <= 5;", true);
	EidosAssertScriptSuccess_L("5.0 <= -10;", false);
	EidosAssertScriptSuccess_L("-10 <= 5.0;", true);
	EidosAssertScriptSuccess_L("5 <= -10.0;", false);
	EidosAssertScriptSuccess_L("-10.0 <= 5;", true);
	EidosAssertScriptSuccess_L("'foo' <= 'bar';", false);
	EidosAssertScriptSuccess_L("'bar' <= 'foo';", true);
	EidosAssertScriptSuccess_L("120 <= '10';", false);
	EidosAssertScriptSuccess_L("10 <= '120';", true);
	EidosAssertScriptSuccess_L("120 <= '15';", true);
	EidosAssertScriptSuccess_L("15 <= '120';", false);
	EidosAssertScriptRaise("_Test(9) <= 5;", 9, "cannot be used with type");
	EidosAssertScriptRaise("5 <= _Test(9);", 2, "cannot be used with type");
	EidosAssertScriptSuccess_L("5 <= 5;", true);
	EidosAssertScriptSuccess_L("-10.0 <= -10.0;", true);
	EidosAssertScriptSuccess_L("5 <= 5.0;", true);
	EidosAssertScriptSuccess_L("5.0 <= 5;", true);
	EidosAssertScriptSuccess_L("5 <= '5';", true);
	EidosAssertScriptSuccess_L("'5' <= 5;", true);
	EidosAssertScriptSuccess_L("'foo' <= 'foo';", true);
	EidosAssertScriptRaise("_Test(9) <= _Test(9);", 9, "cannot be used with type");
	
	EidosAssertScriptSuccess_LV("T <= c(T, F);", {true, false});
	EidosAssertScriptSuccess_LV("5 <= c(5, 6);", {true, true});
	EidosAssertScriptSuccess_LV("5.0 <= c(5.0, 6.0);", {true, true});
	EidosAssertScriptSuccess_LV("'foo' <= c('foo', 'bar');", {true, false});
	
	EidosAssertScriptSuccess_LV("c(T, F) <= T;", {true, true});
	EidosAssertScriptSuccess_LV("c(5, 6) <= 5;", {true, false});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0) <= 5.0;", {true, false});
	EidosAssertScriptSuccess_LV("c('foo', 'bar') <= 'foo';", {true, true});
	
	EidosAssertScriptSuccess_LV("c(T, F) <= c(T, T);", {true, true});
	EidosAssertScriptSuccess_LV("c(5, 6) <= c(5, 8);", {true, true});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0) <= c(5.0, 8.0);", {true, true});
	EidosAssertScriptSuccess_LV("c('foo', 'bar') <= c('foo', 'baz');", {true, true});
	
	EidosAssertScriptSuccess_L("NAN <= NAN;", false);
	EidosAssertScriptSuccess_L("NAN <= 5.0;", false);
	EidosAssertScriptSuccess_L("5.0 <= NAN;", false);
	EidosAssertScriptSuccess_LV("c(5.0, 6.0, NAN) <= c(5.0, 5.0, 5.0);", {true, false, false});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0, 8.0) <= c(5.0, 5.0, NAN);", {true, false, false});
	
	EidosAssertScriptRaise("c(5,6) <= c(5,6,7);", 7, "operator requires that either");
	
	// operator <=: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess_L("identical(4 <= 5, T);", true);
	EidosAssertScriptSuccess_L("identical(5 <= 5, T);", true);
	EidosAssertScriptSuccess_L("identical(6 <= 5, F);", true);
	EidosAssertScriptSuccess_L("identical(4 <= matrix(5), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(5 <= matrix(5), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(6 <= matrix(5), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(2 <= matrix(1:3), matrix(c(F,T,T)));", true);
	EidosAssertScriptSuccess_L("identical((1:3) <= matrix(2), c(T,T,F));", true);
	EidosAssertScriptSuccess_L("identical((1:3) <= matrix(3:1), matrix(c(T,T,F)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(4) <= matrix(5), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(5) <= matrix(5), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(6) <= matrix(5), matrix(F));", true);
	EidosAssertScriptRaise("identical(matrix(1:3) <= matrix(2), matrix(c(T,T,F)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(1:3,nrow=1) <= matrix(3:1,ncol=1), matrix(c(T,T,F)));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(1:3) <= matrix(3:1), matrix(c(T,T,F)));", true);
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
	EidosAssertScriptSuccess_L("T == F;", false);
	EidosAssertScriptSuccess_L("T == T;", true);
	EidosAssertScriptSuccess_L("F == T;", false);
	EidosAssertScriptSuccess_L("F == F;", true);
	EidosAssertScriptSuccess_L("T == 0;", false);
	EidosAssertScriptSuccess_L("T == 1;", true);
	EidosAssertScriptSuccess_L("F == 0;", true);
	EidosAssertScriptSuccess_L("F == 1;", false);
	EidosAssertScriptSuccess_L("T == -5;", false);
	EidosAssertScriptSuccess_L("-5 == T;", false);
	EidosAssertScriptSuccess_L("T == 5;", false);
	EidosAssertScriptSuccess_L("5 == T;", false);
	EidosAssertScriptSuccess_L("T == -5.0;", false);
	EidosAssertScriptSuccess_L("-5.0 == T;", false);
	EidosAssertScriptSuccess_L("T == 5.0;", false);
	EidosAssertScriptSuccess_L("5.0 == T;", false);
	EidosAssertScriptSuccess_L("T == 'FOO';", false);
	EidosAssertScriptSuccess_L("'FOO' == T;", false);
	EidosAssertScriptSuccess_L("T == 'XYZZY';", false);
	EidosAssertScriptSuccess_L("'XYZZY' == T;", false);
	EidosAssertScriptSuccess_L("5 == -10;", false);
	EidosAssertScriptSuccess_L("-10 == 5;", false);
	EidosAssertScriptSuccess_L("5.0 == -10;", false);
	EidosAssertScriptSuccess_L("-10 == 5.0;", false);
	EidosAssertScriptSuccess_L("5 == -10.0;", false);
	EidosAssertScriptSuccess_L("-10.0 == 5;", false);
	EidosAssertScriptSuccess_L("'foo' == 'bar';", false);
	EidosAssertScriptSuccess_L("'bar' == 'foo';", false);
	EidosAssertScriptSuccess_L("120 == '10';", false);
	EidosAssertScriptSuccess_L("10 == '120';", false);
	EidosAssertScriptSuccess_L("120 == '15';", false);
	EidosAssertScriptSuccess_L("15 == '120';", false);
	EidosAssertScriptRaise("_Test(9) == 5;", 9, "cannot be converted to");
	EidosAssertScriptRaise("5 == _Test(9);", 2, "cannot be converted to");
	EidosAssertScriptSuccess_L("5 == 5;", true);
	EidosAssertScriptSuccess_L("-10.0 == -10.0;", true);
	EidosAssertScriptSuccess_L("5 == 5.0;", true);
	EidosAssertScriptSuccess_L("5.0 == 5;", true);
	EidosAssertScriptSuccess_L("5 == '5';", true);
	EidosAssertScriptSuccess_L("'5' == 5;", true);
	EidosAssertScriptSuccess_L("'foo' == 'foo';", true);
	EidosAssertScriptSuccess_L("_Test(9) == _Test(9);", false);	// not the same object
	
	EidosAssertScriptSuccess_LV("T == c(T, F);", {true, false});
	EidosAssertScriptSuccess_LV("5 == c(5, 6);", {true, false});
	EidosAssertScriptSuccess_LV("5.0 == c(5.0, 6.0);", {true, false});
	EidosAssertScriptSuccess_LV("'foo' == c('foo', 'bar');", {true, false});
	EidosAssertScriptSuccess_LV("x = _Test(9); x == c(x, _Test(9));", {true, false});
	
	EidosAssertScriptSuccess_LV("c(T, F) == T;", {true, false});
	EidosAssertScriptSuccess_LV("c(5, 6) == 5;", {true, false});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0) == 5.0;", {true, false});
	EidosAssertScriptSuccess_LV("c('foo', 'bar') == 'foo';", {true, false});
	EidosAssertScriptSuccess_LV("x = _Test(9); c(x, _Test(9)) == x;", {true, false});
	
	EidosAssertScriptSuccess_LV("c(T, F) == c(T, T);", {true, false});
	EidosAssertScriptSuccess_LV("c(5, 6) == c(5, 8);", {true, false});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0) == c(5.0, 8.0);", {true, false});
	EidosAssertScriptSuccess_LV("c('foo', 'bar') == c('foo', 'baz');", {true, false});
	EidosAssertScriptSuccess_LV("x = _Test(9); c(x, _Test(9)) == c(x, x);", {true, false});
	
	EidosAssertScriptSuccess_L("NAN == NAN;", false);
	EidosAssertScriptSuccess_L("NAN == 5.0;", false);
	EidosAssertScriptSuccess_L("5.0 == NAN;", false);
	EidosAssertScriptSuccess_LV("c(5.0, 6.0, NAN) == c(5.0, 5.0, 5.0);", {true, false, false});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0, 8.0) == c(5.0, 5.0, NAN);", {true, false, false});
	
	EidosAssertScriptRaise("c(5,6) == c(5,6,7);", 7, "operator requires that either");
	
	// operator ==: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess_L("identical(5 == 5, T);", true);
	EidosAssertScriptSuccess_L("identical(5 == matrix(2), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(5 == matrix(5), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(2 == matrix(1:3), matrix(c(F,T,F)));", true);
	EidosAssertScriptSuccess_L("identical((1:3) == matrix(2), c(F,T,F));", true);
	EidosAssertScriptSuccess_L("identical((1:3) == matrix(3:1), matrix(c(F,T,F)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(5) == matrix(2), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(matrix(5) == matrix(5), matrix(T));", true);
	EidosAssertScriptRaise("identical(matrix(1:3) == matrix(2), matrix(c(1.0,4,9)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(2:4,nrow=1) == matrix(1:3,ncol=1), matrix(c(2.0,9,64)));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(1:3) == matrix(3:1), matrix(c(F,T,F)));", true);
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
	EidosAssertScriptSuccess_L("T != F;", true);
	EidosAssertScriptSuccess_L("T != T;", false);
	EidosAssertScriptSuccess_L("F != T;", true);
	EidosAssertScriptSuccess_L("F != F;", false);
	EidosAssertScriptSuccess_L("T != 0;", true);
	EidosAssertScriptSuccess_L("T != 1;", false);
	EidosAssertScriptSuccess_L("F != 0;", false);
	EidosAssertScriptSuccess_L("F != 1;", true);
	EidosAssertScriptSuccess_L("T != -5;", true);
	EidosAssertScriptSuccess_L("-5 != T;", true);
	EidosAssertScriptSuccess_L("T != 5;", true);
	EidosAssertScriptSuccess_L("5 != T;", true);
	EidosAssertScriptSuccess_L("T != -5.0;", true);
	EidosAssertScriptSuccess_L("-5.0 != T;", true);
	EidosAssertScriptSuccess_L("T != 5.0;", true);
	EidosAssertScriptSuccess_L("5.0 != T;", true);
	EidosAssertScriptSuccess_L("T != 'FOO';", true);
	EidosAssertScriptSuccess_L("'FOO' != T;", true);
	EidosAssertScriptSuccess_L("T != 'XYZZY';", true);
	EidosAssertScriptSuccess_L("'XYZZY' != T;", true);
	EidosAssertScriptSuccess_L("5 != -10;", true);
	EidosAssertScriptSuccess_L("-10 != 5;", true);
	EidosAssertScriptSuccess_L("5.0 != -10;", true);
	EidosAssertScriptSuccess_L("-10 != 5.0;", true);
	EidosAssertScriptSuccess_L("5 != -10.0;", true);
	EidosAssertScriptSuccess_L("-10.0 != 5;", true);
	EidosAssertScriptSuccess_L("'foo' != 'bar';", true);
	EidosAssertScriptSuccess_L("'bar' != 'foo';", true);
	EidosAssertScriptSuccess_L("120 != '10';", true);
	EidosAssertScriptSuccess_L("10 != '120';", true);
	EidosAssertScriptSuccess_L("120 != '15';", true);
	EidosAssertScriptSuccess_L("15 != '120';", true);
	EidosAssertScriptRaise("_Test(9) != 5;", 9, "cannot be converted to");
	EidosAssertScriptRaise("5 != _Test(9);", 2, "cannot be converted to");
	EidosAssertScriptSuccess_L("5 != 5;", false);
	EidosAssertScriptSuccess_L("-10.0 != -10.0;", false);
	EidosAssertScriptSuccess_L("5 != 5.0;", false);
	EidosAssertScriptSuccess_L("5.0 != 5;", false);
	EidosAssertScriptSuccess_L("5 != '5';", false);
	EidosAssertScriptSuccess_L("'5' != 5;", false);
	EidosAssertScriptSuccess_L("'foo' != 'foo';", false);
	EidosAssertScriptSuccess_L("_Test(9) != _Test(9);", true);	// not the same object
	
	EidosAssertScriptSuccess_LV("T != c(T, F);", {false, true});
	EidosAssertScriptSuccess_LV("5 != c(5, 6);", {false, true});
	EidosAssertScriptSuccess_LV("5.0 != c(5.0, 6.0);", {false, true});
	EidosAssertScriptSuccess_LV("'foo' != c('foo', 'bar');", {false, true});
	EidosAssertScriptSuccess_LV("x = _Test(9); x != c(x, _Test(9));", {false, true});
	
	EidosAssertScriptSuccess_LV("c(T, F) != T;", {false, true});
	EidosAssertScriptSuccess_LV("c(5, 6) != 5;", {false, true});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0) != 5.0;", {false, true});
	EidosAssertScriptSuccess_LV("c('foo', 'bar') != 'foo';", {false, true});
	EidosAssertScriptSuccess_LV("x = _Test(9); c(x, _Test(9)) != x;", {false, true});
	
	EidosAssertScriptSuccess_LV("c(T, F) != c(T, T);", {false, true});
	EidosAssertScriptSuccess_LV("c(5, 6) != c(5, 8);", {false, true});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0) != c(5.0, 8.0);", {false, true});
	EidosAssertScriptSuccess_LV("c('foo', 'bar') != c('foo', 'baz');", {false, true});
	EidosAssertScriptSuccess_LV("x = _Test(9); c(x, _Test(9)) != c(x, x);", {false, true});
	
	EidosAssertScriptSuccess_L("NAN != NAN;", true);
	EidosAssertScriptSuccess_L("NAN != 5.0;", true);
	EidosAssertScriptSuccess_L("5.0 != NAN;", true);
	EidosAssertScriptSuccess_LV("c(5.0, 6.0, NAN) != c(5.0, 5.0, 5.0);", {false, true, true});
	EidosAssertScriptSuccess_LV("c(5.0, 6.0, 8.0) != c(5.0, 5.0, NAN);", {false, true, true});
	
	EidosAssertScriptRaise("c(5,6) != c(5,6,7);", 7, "operator requires that either");
	
	// operator !=: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with integer should suffice
	EidosAssertScriptSuccess_L("identical(5 != 5, F);", true);
	EidosAssertScriptSuccess_L("identical(5 != matrix(2), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(5 != matrix(5), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(2 != matrix(1:3), matrix(c(T,F,T)));", true);
	EidosAssertScriptSuccess_L("identical((1:3) != matrix(2), c(T,F,T));", true);
	EidosAssertScriptSuccess_L("identical((1:3) != matrix(3:1), matrix(c(T,F,T)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(5) != matrix(2), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(5) != matrix(5), matrix(F));", true);
	EidosAssertScriptRaise("identical(matrix(1:3) != matrix(2), matrix(c(1.0,4,9)));", 22, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(2:4,nrow=1) != matrix(1:3,ncol=1), matrix(c(2.0,9,64)));", 29, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(1:3) != matrix(3:1), matrix(c(T,F,T)));", true);
}





































