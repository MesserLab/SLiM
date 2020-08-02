//
//  eidos_test_operators_comparison.cpp
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
	
	EidosAssertScriptSuccess("NAN > NAN;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("NAN > 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 > NAN;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(5.0, 6.0, NAN) > c(5.0, 5.0, 5.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, false}));
	EidosAssertScriptSuccess("c(5.0, 6.0, 8.0) > c(5.0, 5.0, NAN);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, false}));
	
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
	
	EidosAssertScriptSuccess("NAN < NAN;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("NAN < 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 < NAN;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(5.0, 6.0, NAN) < c(5.0, 5.0, 5.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false}));
	EidosAssertScriptSuccess("c(5.0, 6.0, 8.0) < c(5.0, 5.0, NAN);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false}));
	
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
	
	EidosAssertScriptSuccess("NAN >= NAN;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("NAN >= 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 >= NAN;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(5.0, 6.0, NAN) >= c(5.0, 5.0, 5.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, false}));
	EidosAssertScriptSuccess("c(5.0, 6.0, 8.0) >= c(5.0, 5.0, NAN);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, false}));
	
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
	
	EidosAssertScriptSuccess("NAN <= NAN;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("NAN <= 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 <= NAN;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(5.0, 6.0, NAN) <= c(5.0, 5.0, 5.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false}));
	EidosAssertScriptSuccess("c(5.0, 6.0, 8.0) <= c(5.0, 5.0, NAN);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false}));
	
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
	
	EidosAssertScriptSuccess("NAN == NAN;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("NAN == 5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0 == NAN;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(5.0, 6.0, NAN) == c(5.0, 5.0, 5.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false}));
	EidosAssertScriptSuccess("c(5.0, 6.0, 8.0) == c(5.0, 5.0, NAN);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false}));
	
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
	
	EidosAssertScriptSuccess("NAN != NAN;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("NAN != 5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0 != NAN;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("c(5.0, 6.0, NAN) != c(5.0, 5.0, 5.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, true}));
	EidosAssertScriptSuccess("c(5.0, 6.0, 8.0) != c(5.0, 5.0, NAN);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true, true}));
	
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





































