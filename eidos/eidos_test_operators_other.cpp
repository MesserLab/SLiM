//
//  eidos_test_operators_other.cpp
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


#pragma mark operator []
void _RunOperatorSubsetTests(void)
{
	// operator []
	EidosAssertScriptSuccess_IV("x = 1:5; x[NULL];", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess_NULL("x = 1:5; NULL[x];");
	EidosAssertScriptSuccess_NULL("x = 1:5; NULL[NULL];");
	EidosAssertScriptSuccess_IV("x = 1:5; x[];", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess("x = 1:5; x[integer(0)];", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess_I("x = 1:5; x[2];", 3);
	EidosAssertScriptSuccess_IV("x = 1:5; x[2:3];", {3, 4});
	EidosAssertScriptSuccess_IV("x = 1:5; x[c(0, 2, 4)];", {1, 3, 5});
	EidosAssertScriptSuccess_IV("x = 1:5; x[0:4];", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess("x = 1:5; x[float(0)];", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess_I("x = 1:5; x[2.0];", 3);
	EidosAssertScriptSuccess_IV("x = 1:5; x[2.0:3];", {3, 4});
	EidosAssertScriptSuccess_IV("x = 1:5; x[c(0.0, 2, 4)];", {1, 3, 5});
	EidosAssertScriptSuccess_IV("x = 1:5; x[0.0:4];", {1, 2, 3, 4, 5});
	EidosAssertScriptRaise("x = 1:5; x[c(7,8)];", 10, "out-of-range index");
	EidosAssertScriptRaise("x = 1:5; x[logical(0)];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[T];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[c(T, T)];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[c(T, F, T)];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[NAN];", 10, "cannot be converted");
	EidosAssertScriptRaise("x = 1:5; x[c(0.0, 2, NAN)];", 10, "cannot be converted");
	EidosAssertScriptSuccess_IV("x = 1:5; x[c(T, F, T, F, T)];", {1, 3, 5});
	EidosAssertScriptSuccess_IV("x = 1:5; x[c(T, T, T, T, T)];", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess("x = 1:5; x[c(F, F, F, F, F)];", gStaticEidosValue_Integer_ZeroVec);

	EidosAssertScriptSuccess_LV("x = c(T,T,F,T,F); x[c(T, F, T, F, T)];", {true, false, false});
	EidosAssertScriptSuccess_FV("x = 1.0:5; x[c(T, F, T, F, T)];", {1.0, 3.0, 5.0});
	EidosAssertScriptSuccess_SV("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(T, F, T, F, T)];", {"foo", "foobaz", "xyzzy"});
	
	EidosAssertScriptSuccess_LV("x = c(T,T,F,T,F); x[c(2,3)];", {false, true});
	EidosAssertScriptRaise("x = c(T,T,F,T,F); x[c(2,3,7)];", 19, "out-of-range index");
	EidosAssertScriptSuccess_LV("x = c(T,T,F,T,F); x[c(2.0,3)];", {false, true});
	EidosAssertScriptRaise("x = c(T,T,F,T,F); x[c(2.0,3,7)];", 19, "out-of-range index");
	
	EidosAssertScriptSuccess_IV("x = 1:5; x[c(2,3)];", {3, 4});
	EidosAssertScriptRaise("x = 1:5; x[c(2,3,7)];", 10, "out-of-range index");
	EidosAssertScriptSuccess_IV("x = 1:5; x[c(2.0,3)];", {3, 4});
	EidosAssertScriptRaise("x = 1:5; x[c(2.0,3,7)];", 10, "out-of-range index");
	
	EidosAssertScriptSuccess_FV("x = 1.0:5; x[c(2,3)];", {3.0, 4.0});
	EidosAssertScriptRaise("x = 1.0:5; x[c(2,3,7)];", 12, "out-of-range index");
	EidosAssertScriptSuccess_FV("x = 1.0:5; x[c(2.0,3)];", {3.0, 4.0});
	EidosAssertScriptRaise("x = 1.0:5; x[c(2.0,3,7)];", 12, "out-of-range index");
	
	EidosAssertScriptSuccess_SV("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(2,3)];", {"foobaz", "baz"});
	EidosAssertScriptRaise("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(2,3,7)];", 48, "out-of-range index");
	EidosAssertScriptSuccess_SV("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(2.0,3)];", {"foobaz", "baz"});
	EidosAssertScriptRaise("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(2.0,3,7)];", 48, "out-of-range index");
	
	EidosAssertScriptSuccess_IV("x = c(_Test(1), _Test(2), _Test(3), _Test(4), _Test(5)); x = x[c(2,3)]; x._yolk;", {3, 4});
	EidosAssertScriptRaise("x = c(_Test(1), _Test(2), _Test(3), _Test(4), _Test(5)); x = x[c(2,3,7)]; x._yolk;", 62, "out-of-range index");
	EidosAssertScriptSuccess_IV("x = c(_Test(1), _Test(2), _Test(3), _Test(4), _Test(5)); x = x[c(2.0,3)]; x._yolk;", {3, 4});
	EidosAssertScriptRaise("x = c(_Test(1), _Test(2), _Test(3), _Test(4), _Test(5)); x = x[c(2.0,3,7)]; x._yolk;", 62, "out-of-range index");
	
	EidosAssertScriptSuccess_I("x = 5; x[T];", 5);
	EidosAssertScriptSuccess_IV("x = 5; x[F];", {});
	EidosAssertScriptRaise("x = 5; x[logical(0)];", 8, "must match the size()");
	EidosAssertScriptSuccess_I("x = 5; x[0];", 5);
	EidosAssertScriptRaise("x = 5; x[1];", 8, "out of range");
	EidosAssertScriptRaise("x = 5; x[-1];", 8, "out of range");
	EidosAssertScriptSuccess_IV("x = 5; x[integer(0)];", {});
	EidosAssertScriptSuccess_I("x = 5; x[0.0];", 5);
	EidosAssertScriptRaise("x = 5; x[1.0];", 8, "out-of-range");
	EidosAssertScriptRaise("x = 5; x[-1.0];", 8, "out-of-range");
	EidosAssertScriptSuccess_IV("x = 5; x[float(0)];", {});
	
	EidosAssertScriptRaise("x = 5:9; x[matrix(0)];", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(0:2)];", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(T)];", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(c(T,T,F,T,F))];", 10, "matrix or array index operand is not supported");
	
	// matrix/array subsets, without dropping
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[], 1:6);", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[,], matrix(1:6, nrow=2));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[NULL,NULL], matrix(1:6, nrow=2));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[0,], matrix(c(1,3,5), nrow=1));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[1,], matrix(c(2,4,6), nrow=1));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[1,NULL], matrix(c(2,4,6), nrow=1));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[0:1,], matrix(1:6, nrow=2));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[NULL,], matrix(1:6, nrow=2));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[,0], matrix(1:2, ncol=1));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[,1], matrix(3:4, ncol=1));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[,2], matrix(5:6, ncol=1));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[,0:1], matrix(1:4, ncol=2));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[,1:2], matrix(3:6, ncol=2));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[,c(0,2)], matrix(c(1,2,5,6), ncol=2));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[NULL,c(0,2)], matrix(c(1,2,5,6), ncol=2));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[0,1], matrix(3));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[1,2], matrix(6));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[0,c(T,F,T)], matrix(c(1,5), nrow=1));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[c(F,T),c(F,F,T)], matrix(6));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[c(F,F),c(F,F,F)], integer(0));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[c(F,F),c(F,T,T)], integer(0));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[c(T,T),c(T,T,F)], matrix(1:4, ncol=2));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[c(0,0,1,0),], matrix(c(1,3,5,1,3,5,2,4,6,1,3,5), ncol=3, byrow=T));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[c(0,0,1,0),c(1,2,1)], matrix(c(3,5,3,3,5,3,4,6,4,3,5,3), ncol=3, byrow=T));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); identical(x[,c(1,2,1)], matrix(c(3,4,5,6,3,4), nrow=2));", true);
	
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T),c(T,T,F)];", 26, "match the corresponding dimension");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T,T,T),c(T,T,F)];", 26, "match the corresponding dimension");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T,T),c(T,T)];", 26, "match the corresponding dimension");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T,T),c(T,T,F,T)];", 26, "match the corresponding dimension");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[-1,];", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[2,];", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[,-1];", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[,3];", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0,0,0];", 26, "too many subset arguments");
	
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[], 1:12);", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[,,], array(1:12, c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[NULL,NULL,NULL], array(1:12, c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[0,,], array(c(1,3,5,7,9,11), c(1,3,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[1,,], array(c(2,4,6,8,10,12), c(1,3,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[1,NULL,NULL], array(c(2,4,6,8,10,12), c(1,3,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[0:1,,], array(1:12, c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[NULL,,], array(1:12, c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[,0,], array(c(1,2,7,8), c(2,1,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[,1,], array(c(3,4,9,10), c(2,1,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[,2,], array(c(5,6,11,12), c(2,1,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[,c(0,2),], array(c(1,2,5,6,7,8,11,12), c(2,2,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[,,0], array(1:6, c(2,3,1)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[NULL,NULL,1], array(7:12, c(2,3,1)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[1,2,0], array(6, c(1,1,1)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[0,1,1], array(9, c(1,1,1)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[1,1:2,], array(c(4,6,10,12), c(1,2,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[0,c(T,F,T),], array(c(1,5,7,11), c(1,2,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[c(T,F),c(T,F,T),], array(c(1,5,7,11), c(1,2,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[c(T,F),c(T,F,T),c(F,T)], array(c(7,11), c(1,2,1)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[c(T,F),c(F,F,T),c(F,T)], array(11, c(1,1,1)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[c(F,F),c(F,F,F),c(F,T)], integer(0));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[c(F,F),c(T,F,T),c(F,T)], integer(0));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[c(0,0,1,0),,], array(c(1,1,2,1,3,3,4,3,5,5,6,5,7,7,8,7,9,9,10,9,11,11,12,11), c(4,3,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); identical(x[c(0,0,1,0),c(2,1),0], array(c(5,5,6,5,3,3,4,3), c(4,2,1)));", true);
	
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T), c(T,T,T), c(T,T)];", 28, "match the corresponding dimension");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T,T,T), c(T,T,T), c(T,T)];", 28, "match the corresponding dimension");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T,T), c(T,T), c(T,T)];", 28, "match the corresponding dimension");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T,T), c(T,T,T,T), c(T,T)];", 28, "match the corresponding dimension");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T,T), c(T,T,T), c(T)];", 28, "match the corresponding dimension");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T,T), c(T,T,T), c(T,T,T)];", 28, "match the corresponding dimension");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[-1, 0, 0];", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[2, 0, 0];", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0, -1, 0];", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0, 3, 0];", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0, 0, -1];", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0, 0, 2];", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0, 0];", 28, "too few subset arguments");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0, 0, 0, 0];", 28, "too many subset arguments");
}

#pragma mark operator = with []
void _RunOperatorAssignTests(void)
{
	// operator =
	EidosAssertScriptRaise("E = 7;", 2, "cannot be redefined because it is a constant");
	EidosAssertScriptRaise("E = E + 7;", 2, "cannot be redefined because it is a constant");
	
	// operator = (especially in conjunction with operator [])
	EidosAssertScriptSuccess_I("x = 5; x;", 5);
	EidosAssertScriptSuccess_IV("x = 1:5; x;", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess_IV("x = 1:5; x[x % 2 == 1] = 10; x;", {10, 2, 10, 4, 10});
	EidosAssertScriptSuccess_IV("x = 1:5; x[x % 2 == 1][1:2] = 10; x;", {1, 2, 10, 4, 10});
	EidosAssertScriptSuccess_IV("x = 1:5; x[1:3*2 - 2] = 10; x;", {10, 2, 10, 4, 10});
	EidosAssertScriptSuccess_IV("x = 1:5; x[1:3*2 - 2][0:1] = 10; x;", {10, 2, 10, 4, 5});
	EidosAssertScriptSuccess_IV("x = 1:5; x[x % 2 == 1] = 11:13; x;", {11, 2, 12, 4, 13});
	EidosAssertScriptSuccess_IV("x = 1:5; x[x % 2 == 1][1:2] = 11:12; x;", {1, 2, 11, 4, 12});
	EidosAssertScriptSuccess_IV("x = 1:5; x[1:3*2 - 2] = 11:13; x;", {11, 2, 12, 4, 13});
	EidosAssertScriptSuccess_IV("x = 1:5; x[1:3*2 - 2][0:1] = 11:12; x;", {11, 2, 12, 4, 5});
	EidosAssertScriptRaise("x = 1:5; x[1:3*2 - 2][0:1] = 11:13; x;", 27, "assignment to a subscript requires");
	EidosAssertScriptRaise("x = 1:5; x[NULL] = NULL; x;", 17, "assignment to a subscript requires an rvalue that is");
	EidosAssertScriptSuccess_IV("x = 1:5; x[NULL] = 10; x;", {10, 10, 10, 10, 10});	// assigns 10 to all indices, legal in Eidos 1.6 and later
	EidosAssertScriptRaise("x = 1:5; x[3] = NULL; x;", 14, "assignment to a subscript requires");
	EidosAssertScriptRaise("x = 1:5; x[integer(0)] = NULL; x;", 23, "type mismatch");
	EidosAssertScriptSuccess_IV("x = 1:5; x[integer(0)] = 10; x;", {1, 2, 3, 4, 5}); // assigns 10 to no indices, perfectly legal
	EidosAssertScriptRaise("x = 1:5; x[3] = integer(0); x;", 14, "assignment to a subscript requires");
	EidosAssertScriptSuccess_FV("x = 1.0:5; x[3] = 1; x;", {1, 2, 3, 1, 5});
	EidosAssertScriptSuccess_SV("x = c('a', 'b', 'c'); x[1] = 1; x;", {"a", "1", "c"});
	EidosAssertScriptRaise("x = 1:5; x[3] = 1.5; x;", 14, "type mismatch");
	EidosAssertScriptRaise("x = 1:5; x[3] = 'foo'; x;", 14, "type mismatch");
	EidosAssertScriptSuccess_I("x = 5; x[0] = 10; x;", 10);
	EidosAssertScriptSuccess_F("x = 5.0; x[0] = 10.0; x;", 10);
	EidosAssertScriptRaise("x = 5; x[0] = 10.0; x;", 12, "type mismatch");
	EidosAssertScriptSuccess_F("x = 5.0; x[0] = 10; x;", 10);
	EidosAssertScriptSuccess_L("x = T; x[0] = F; x;", false);
	EidosAssertScriptSuccess_S("x = 'foo'; x[0] = 'bar'; x;", "bar");
	EidosAssertScriptSuccess_IV("x = 1:5; x[c(T,T,F,T,F)] = 7:9; x;", {7, 8, 3, 9, 5});
	EidosAssertScriptRaise("x = 1:5; x[c(T,T,F,T,F,T)] = 7:9; x;", 10, "must match the size()");
	EidosAssertScriptSuccess_IV("x = 1:5; x[c(2,3)] = c(9, 5); x;", {1, 2, 9, 5, 5});
	EidosAssertScriptRaise("x = 1:5; x[c(7,8)] = 7; x;", 10, "out-of-range index");
	EidosAssertScriptSuccess_IV("x = 1:5; x[c(2.0,3)] = c(9, 5); x;", {1, 2, 9, 5, 5});
	EidosAssertScriptRaise("x = 1:5; x[c(7.0,8)] = 7; x;", 10, "out-of-range index");
	EidosAssertScriptRaise("x = 1:5; x[NAN] = 3;", 10, "cannot be converted");
	EidosAssertScriptRaise("x = 1:5; x[c(0.0, 2, NAN)] = c(5, 7, 3);", 10, "cannot be converted");
	
	EidosAssertScriptRaise("x = 5:9; x[matrix(0)] = 3;", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(0:2)] = 3;", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(T)] = 3;", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(c(T,T,F,T,F))] = 3;", 10, "matrix or array index operand is not supported");
	EidosAssertScriptSuccess_L("x = 1; x[] = 2; identical(x, 2);", true);
	EidosAssertScriptSuccess_L("x = 1; x[NULL] = 2; identical(x, 2);", true);
	EidosAssertScriptSuccess_L("x = 1:5; x[] = 2; identical(x, rep(2,5));", true);
	EidosAssertScriptSuccess_L("x = 1:5; x[NULL] = 2; identical(x, rep(2,5));", true);
	EidosAssertScriptSuccess_L("x = matrix(5); x[] = 3; identical(x, matrix(3));", true);
	EidosAssertScriptSuccess_L("x = matrix(5); x[NULL] = 3; identical(x, matrix(3));", true);
	EidosAssertScriptSuccess_L("x = matrix(5); x[0] = 3; identical(x, matrix(3));", true);
	EidosAssertScriptSuccess_L("x = matrix(5:9); x[] = 3; identical(x, matrix(c(3,3,3,3,3)));", true);
	EidosAssertScriptSuccess_L("x = matrix(5:9); x[NULL] = 3; identical(x, matrix(c(3,3,3,3,3)));", true);
	EidosAssertScriptSuccess_L("x = matrix(5:9); x[0] = 3; identical(x, matrix(c(3,6,7,8,9)));", true);
	EidosAssertScriptSuccess_L("x = matrix(5); x[T] = 3; identical(x, matrix(3));", true);
	EidosAssertScriptSuccess_L("x = matrix(5:9); x[c(T,F,T,T,F)] = 3; identical(x, matrix(c(3,6,3,3,9)));", true);
	
	// operator = (especially in conjunction with matrix/array-style subsetting with operator [])
	EidosAssertScriptSuccess_VOID("NULL[logical(0)] = NULL;");			// technically legal, as no assignment is done
	EidosAssertScriptRaise("NULL[logical(0),] = NULL;", 4, "too many subset arguments");
	EidosAssertScriptRaise("NULL[logical(0),logical(0)] = NULL;", 4, "too many subset arguments");
	EidosAssertScriptRaise("NULL[,] = NULL;", 4, "too many subset arguments");
	EidosAssertScriptSuccess_VOID("x = NULL; x[logical(0)] = NULL;");	// technically legal, as no assignment is done
	EidosAssertScriptRaise("x = NULL; x[logical(0),] = NULL;", 11, "too many subset arguments");
	EidosAssertScriptRaise("x = NULL; x[logical(0),logical(0)] = NULL;", 11, "too many subset arguments");
	EidosAssertScriptRaise("x = NULL; x[,] = NULL;", 11, "too many subset arguments");
	EidosAssertScriptRaise("x = 1; x[,] = 2; x;", 8, "too many subset arguments");
	EidosAssertScriptRaise("x = 1; x[0,0] = 2; x;", 8, "too many subset arguments");
	EidosAssertScriptRaise("x = 1:5; x[,] = 2; x;", 10, "too many subset arguments");
	EidosAssertScriptRaise("x = 1:5; x[0,0] = 2; x;", 10, "too many subset arguments");
	EidosAssertScriptSuccess_L("x = matrix(1:5); x[,] = 2; identical(x, matrix(rep(2,5)));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:5); x[NULL,NULL] = 2; identical(x, matrix(rep(2,5)));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:5); x[0,0] = 2; identical(x, matrix(c(2,2,3,4,5)));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:5); x[3,0] = 2; identical(x, matrix(c(1,2,3,2,5)));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:5); x[1:3,0] = 7; identical(x, matrix(c(1,7,7,7,5)));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:5); x[c(1,3),0] = 7; identical(x, matrix(c(1,7,3,7,5)));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:5); x[c(1,3),0] = 6:7; identical(x, matrix(c(1,6,3,7,5)));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:5); x[c(T,F,F,T,F),0] = 7; identical(x, matrix(c(7,2,3,7,5)));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:5); x[c(T,F,F,T,F),0] = 6:7; identical(x, matrix(c(6,2,3,7,5)));", true);
	EidosAssertScriptRaise("x = matrix(1:5); x[-1,0] = 2;", 18, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:5); x[5,0] = 2;", 18, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:5); x[0,-1] = 2;", 18, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:5); x[0,1] = 2;", 18, "out-of-range index");
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); x[1,1] = 2; identical(x, matrix(c(1,2,3,2,5,6), nrow=2));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); x[0:1,1] = 7; identical(x, matrix(c(1,2,7,7,5,6), nrow=2));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); x[1, c(T,F,T)] = 7; identical(x, matrix(c(1,7,3,4,5,7), nrow=2));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); x[0:1, c(T,F,T)] = 7; identical(x, matrix(c(7,7,3,4,7,7), nrow=2));", true);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); x[c(T,T), c(T,F,T)] = 6:9; identical(x, matrix(c(6,7,3,4,8,9), nrow=2));", true);
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[-1,0] = 2;", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[2,0] = 2;", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0,-1] = 2;", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0,3] = 2;", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T,F,T),0] = 2;", 26, "size() of a logical");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[T,0] = 2;", 26, "size() of a logical");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0:4][,0] = 2;", 31, "chaining of matrix/array-style subsets");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0,1:2][,0] = 2;", 33, "chaining of matrix/array-style subsets");
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); x[0,1:2][1] = 2; identical(x, c(1,2,3,4,2,6));", false);
	EidosAssertScriptSuccess_L("x = matrix(1:6, nrow=2); x[0,1:2][1] = 2; identical(x, matrix(c(1,2,3,4,2,6), nrow=2));", true);
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=matrix(c(x,y,x,y), nrow=2); z._yolk[,1]=6.5;", 61, "subset of a property");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=matrix(c(x,y,x,y), nrow=2); z[,1]._yolk[1]=6.5;", 68, "subset of a property");
	EidosAssertScriptSuccess_L("x=_Test(9); z=matrix(x); identical(z._yolk, matrix(9));", true);
	EidosAssertScriptSuccess_L("x=_Test(9); z=array(x, c(1,1,1,1)); identical(z._yolk, array(9, c(1,1,1,1)));", true);
	EidosAssertScriptSuccess_L("x=_Test(9); z=matrix(x); z[0]._yolk = 6; identical(z._yolk, matrix(6));", true);
	EidosAssertScriptSuccess_L("x=_Test(9); z=array(x, c(1,1,1,1)); z[0]._yolk = 6; identical(z._yolk, array(6, c(1,1,1,1)));", true);
	EidosAssertScriptSuccess_L("x=_Test(9); z=matrix(x); z[0,0]._yolk = 6; identical(z._yolk, matrix(6));", true);
	EidosAssertScriptSuccess_L("x=_Test(9); z=array(x, c(1,1,1,1)); z[0,0,0,0]._yolk = 6; identical(z._yolk, array(6, c(1,1,1,1)));", true);
	EidosAssertScriptSuccess_L("x=_Test(9); y=_Test(7); z=matrix(c(x,y,x,y), nrow=2); z[,1]._yolk=6; identical(z._yolk, matrix(c(6,6,6,6), nrow=2));", true);
	
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); x[,,] = 2; identical(x, array(rep(2,12), c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); x[1,0,1] = -1; identical(x, array(c(1,2,3,4,5,6,7,-1,9,10,11,12), c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); x[1,c(T,F,T),1] = 7; identical(x, array(c(1,2,3,4,5,6,7,7,9,10,11,7), c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); x[1,c(T,F,T),1] = -1:-2; identical(x, array(c(1,2,3,4,5,6,7,-1,9,10,11,-2), c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); x[0:1,c(T,F,T),1] = 15; identical(x, array(c(1,2,3,4,5,6,15,15,9,10,15,15), c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); x[0:1,c(T,F,T),1] = 15:18; identical(x, array(c(1,2,3,4,5,6,15,16,9,10,17,18), c(2,3,2)));", true);
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0:1,c(T,F,T),1] = 15:17; identical(x, array(c(1,2,3,4,5,6,15,16,9,10,17,18), c(2,3,2)));", 45, ".size() matching the .size");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0:1,c(T,F,T),1] = 15:19; identical(x, array(c(1,2,3,4,5,6,15,16,9,10,17,18), c(2,3,2)));", 45, ".size() matching the .size");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[-1,0,0] = 2;", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[2,0,0] = 2;", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0,-1,0] = 2;", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0,3,0] = 2;", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0,0,-1] = 2;", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0,0,2] = 2;", 28, "out-of-range index");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[c(T,F,T),0,0] = 2;", 28, "size() of a logical");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[T,0,0] = 2;", 28, "size() of a logical");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0:4][,0,] = 2;", 33, "chaining of matrix/array-style subsets");
	EidosAssertScriptRaise("x = array(1:12, c(2,3,2)); x[0,1:2,][,0,] = 2;", 36, "chaining of matrix/array-style subsets");
	EidosAssertScriptSuccess_L("x = array(1:12, c(2,3,2)); x[0,1:2,][1:2] = 2; identical(x, array(c(1,2,3,4,2,6,7,8,2,10,11,12), c(2,3,2)));", true);
	
	// operator = (especially in conjunction with operator .)
	EidosAssertScriptSuccess_I("x=_Test(9); x._yolk;", 9);
	EidosAssertScriptRaise("x=_Test(NULL);", 2, "cannot be type NULL");
	EidosAssertScriptRaise("x=_Test(9); x._yolk = NULL;", 20, "value cannot be type");
	EidosAssertScriptSuccess_IV("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk;", {9, 7, 9, 7});
	EidosAssertScriptSuccess_IV("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=2; z._yolk;", {9, 2, 9, 2});
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=2; z._yolk;", 48, "not an lvalue");				// used to be legal, now a policy error
	EidosAssertScriptSuccess_IV("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[c(1,0)]._yolk=c(2, 5); z._yolk;", {5, 2, 5, 2});
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[c(1,0)]=c(3, 6); z._yolk;", 53, "not an lvalue");	// used to be legal, now a policy error
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=6.5; z._yolk;", 48, "value cannot be type");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=6.5; z._yolk;", 48, "not an lvalue");			// used to be a type error, now a policy error
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[2:3]._yolk=6.5; z._yolk;", 50, "value cannot be type");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[2:3]=6.5; z._yolk;", 50, "not an lvalue");			// used to be a type error, now a policy error
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[2]=6.5; z._yolk;", 42, "type mismatch");
	EidosAssertScriptRaise("x = 1:5; x.foo[5] = 7;", 10, "operand type integer is not supported");
	
	// operator = (with compound-operator optimizations)
	EidosAssertScriptSuccess_I("x = 5; x = x + 3; x;", 8);
	EidosAssertScriptSuccess_IV("x = 5:6; x = x + 3; x;", {8, 9});
	EidosAssertScriptSuccess_IV("x = 5:6; x = x + 3:4; x;", {8, 10});
	EidosAssertScriptSuccess_F("x = 5; x = x + 3.5; x;", 8.5);
	EidosAssertScriptSuccess_FV("x = 5:6; x = x + 3.5; x;", {8.5, 9.5});
	EidosAssertScriptSuccess_FV("x = 5:6; x = x + 3.5:4.5; x;", {8.5, 10.5});
	EidosAssertScriptRaise("x = 5:7; x = x + 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x + 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess_F("x = 5.5; x = x + 3.5; x;", 9);
	EidosAssertScriptSuccess_FV("x = 5.5:6.5; x = x + 3.5; x;", {9, 10});
	EidosAssertScriptSuccess_FV("x = 5.5:6.5; x = x + 3.5:4.5; x;", {9, 11});
	EidosAssertScriptSuccess_F("x = 5.5; x = x + 3; x;", 8.5);
	EidosAssertScriptSuccess_FV("x = 5.5:6.5; x = x + 3; x;", {8.5, 9.5});
	EidosAssertScriptSuccess_FV("x = 5.5:6.5; x = x + 3:4; x;", {8.5, 10.5});
	EidosAssertScriptRaise("x = 5.5:7.5; x = x + 3.5:4.5; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.5:6.5; x = x + 3.5:5.5; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess_I("x = 5; x = x - 3; x;", 2);
	EidosAssertScriptSuccess_IV("x = 5:6; x = x - 3; x;", {2, 3});
	EidosAssertScriptSuccess_IV("x = 5:6; x = x - 3:4; x;", {2, 2});
	EidosAssertScriptSuccess_F("x = 5; x = x - 3.5; x;", 1.5);
	EidosAssertScriptSuccess_FV("x = 5:6; x = x - 3.5; x;", {1.5, 2.5});
	EidosAssertScriptSuccess_FV("x = 5:6; x = x - 3.5:4.5; x;", {1.5, 1.5});
	EidosAssertScriptRaise("x = 5:7; x = x - 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x - 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess_F("x = 5.5; x = x - 3.5; x;", 2);
	EidosAssertScriptSuccess_FV("x = 5.5:6.5; x = x - 3.5; x;", {2, 3});
	EidosAssertScriptSuccess_FV("x = 5.5:6.5; x = x - 3.5:4.5; x;", {2, 2});
	EidosAssertScriptSuccess_F("x = 5.5; x = x - 3; x;", 2.5);
	EidosAssertScriptSuccess_FV("x = 5.5:6.5; x = x - 3; x;", {2.5, 3.5});
	EidosAssertScriptSuccess_FV("x = 5.5:6.5; x = x - 3:4; x;", {2.5, 2.5});
	EidosAssertScriptRaise("x = 5.5:7.5; x = x - 3.5:4.5; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.5:6.5; x = x - 3.5:5.5; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess_F("x = 5; x = x / 2; x;", 2.5);
	EidosAssertScriptSuccess_FV("x = 5:6; x = x / 2; x;", {2.5, 3.0});
	EidosAssertScriptSuccess_FV("x = 5:6; x = x / c(2,4); x;", {2.5, 1.5});
	EidosAssertScriptSuccess_F("x = 5; x = x / 2.0; x;", 2.5);
	EidosAssertScriptSuccess_FV("x = 5:6; x = x / 2.0; x;", {2.5, 3.0});
	EidosAssertScriptSuccess_FV("x = 5:6; x = x / c(2.0,4.0); x;", {2.5, 1.5});
	EidosAssertScriptRaise("x = 5:7; x = x / 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x / 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess_F("x = 5.0; x = x / 2.0; x;", 2.5);
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x / 2.0; x;", {2.5, 3.0});
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x / c(2.0,4.0); x;", {2.5, 1.5});
	EidosAssertScriptSuccess_F("x = 5.0; x = x / 2; x;", 2.5);
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x / 2; x;", {2.5, 3.0});
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x / c(2,4); x;", {2.5, 1.5});
	EidosAssertScriptRaise("x = 5.0:7.0; x = x / 3.0:4.0; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.0:6.0; x = x / 3.0:5.0; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess("x = 5; x = x % 2; x;", gStaticEidosValue_Float1);
	EidosAssertScriptSuccess_FV("x = 5:6; x = x % 2; x;", {1.0, 0.0});
	EidosAssertScriptSuccess_FV("x = 5:6; x = x % c(2,4); x;", {1.0, 2.0});
	EidosAssertScriptSuccess("x = 5; x = x % 2.0; x;", gStaticEidosValue_Float1);
	EidosAssertScriptSuccess_FV("x = 5:6; x = x % 2.0; x;", {1.0, 0.0});
	EidosAssertScriptSuccess_FV("x = 5:6; x = x % c(2.0,4.0); x;", {1.0, 2.0});
	EidosAssertScriptRaise("x = 5:7; x = x % 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x % 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess("x = 5.0; x = x % 2.0; x;", gStaticEidosValue_Float1);
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x % 2.0; x;", {1.0, 0.0});
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x % c(2.0,4.0); x;", {1.0, 2.0});
	EidosAssertScriptSuccess("x = 5.0; x = x % 2; x;", gStaticEidosValue_Float1);
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x % 2; x;", {1.0, 0.0});
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x % c(2,4); x;", {1.0, 2.0});
	EidosAssertScriptRaise("x = 5.0:7.0; x = x % 3.0:4.0; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.0:6.0; x = x % 3.0:5.0; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess_I("x = 5; x = x * 2; x;", 10);
	EidosAssertScriptSuccess_IV("x = 5:6; x = x * 2; x;", {10, 12});
	EidosAssertScriptSuccess_IV("x = 5:6; x = x * c(2,4); x;", {10, 24});
	EidosAssertScriptSuccess_F("x = 5; x = x * 2.0; x;", 10.0);
	EidosAssertScriptSuccess_FV("x = 5:6; x = x * 2.0; x;", {10.0, 12.0});
	EidosAssertScriptSuccess_FV("x = 5:6; x = x * c(2.0,4.0); x;", {10.0, 24.0});
	EidosAssertScriptRaise("x = 5:7; x = x * 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x * 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess_F("x = 5.0; x = x * 2.0; x;", 10.0);
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x * 2.0; x;", {10.0, 12.0});
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x * c(2.0,4.0); x;", {10.0, 24.0});
	EidosAssertScriptSuccess_F("x = 5.0; x = x * 2; x;", 10.0);
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x * 2; x;", {10.0, 12.0});
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x * c(2,4); x;", {10.0, 24.0});
	EidosAssertScriptRaise("x = 5.0:7.0; x = x * 3.0:4.0; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.0:6.0; x = x * 3.0:5.0; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess_F("x = 5; x = x ^ 2; x;", 25.0);
	EidosAssertScriptSuccess_FV("x = 5:6; x = x ^ 2; x;", {25.0, 36.0});
	EidosAssertScriptSuccess_FV("x = 5:6; x = x ^ c(2,3); x;", {25.0, 216.0});
	EidosAssertScriptSuccess_F("x = 5; x = x ^ 2.0; x;", 25.0);
	EidosAssertScriptSuccess_FV("x = 5:6; x = x ^ 2.0; x;", {25.0, 36.0});
	EidosAssertScriptSuccess_FV("x = 5:6; x = x ^ c(2.0,3.0); x;", {25.0, 216.0});
	EidosAssertScriptRaise("x = 5:7; x = x ^ (3:4); x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x ^ (3:5); x;", 15, "operator requires that either");
	EidosAssertScriptSuccess_F("x = 5.0; x = x ^ 2.0; x;", 25.0);
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x ^ 2.0; x;", {25.0, 36.0});
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x ^ c(2.0,3.0); x;", {25.0, 216.0});
	EidosAssertScriptSuccess_F("x = 5.0; x = x ^ 2; x;", 25.0);
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x ^ 2; x;", {25.0, 36.0});
	EidosAssertScriptSuccess_FV("x = 5.0:6.0; x = x ^ c(2,3); x;", {25.0, 216.0});
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
	EidosAssertScriptSuccess_L("T&T&T;", true);
	EidosAssertScriptSuccess_L("T&T&F;", false);
	EidosAssertScriptSuccess_L("T&F&T;", false);
	EidosAssertScriptSuccess_L("T&F&F;", false);
	EidosAssertScriptSuccess_L("F&T&T;", false);
	EidosAssertScriptSuccess_L("F&T&F;", false);
	EidosAssertScriptSuccess_L("F&F&T;", false);
	EidosAssertScriptSuccess_L("F&F&F;", false);
	EidosAssertScriptSuccess_LV("c(T,F,T,F) & F;", {false, false, false, false});
	EidosAssertScriptSuccess_LV("c(T,F,T,F) & T;", {true, false, true, false});
	EidosAssertScriptSuccess_LV("F & c(T,F,T,F);", {false, false, false, false});
	EidosAssertScriptSuccess_LV("T & c(T,F,T,F);", {true, false, true, false});
	EidosAssertScriptSuccess_LV("c(T,F,T,F) & c(T,T,F,F);", {true, false, false, false});
	EidosAssertScriptSuccess_LV("c(T,F,T,F) & c(F,F,T,T);", {false, false, true, false});
	EidosAssertScriptSuccess_LV("c(T,T,F,F) & c(T,F,T,F);", {true, false, false, false});
	EidosAssertScriptSuccess_LV("c(F,F,T,T) & c(T,F,T,F);", {false, false, true, false});
	EidosAssertScriptRaise("c(T,F,T,F) & c(F,F);", 11, "not compatible in size()");
	EidosAssertScriptRaise("c(T,T) & c(T,F,T,F);", 7, "not compatible in size()");
	EidosAssertScriptRaise("c(T,F,T,F) & _Test(3);", 11, "is not supported by");
	EidosAssertScriptRaise("_Test(3) & c(T,F,T,F);", 9, "is not supported by");
	EidosAssertScriptSuccess_L("5&T&T;", true);
	EidosAssertScriptSuccess_L("T&5&F;", false);
	EidosAssertScriptSuccess_L("T&F&5;", false);
	EidosAssertScriptSuccess_L("5&F&F;", false);
	EidosAssertScriptSuccess_L("0&T&T;", false);
	EidosAssertScriptSuccess_L("F&T&0;", false);
	EidosAssertScriptSuccess_L("F&0&T;", false);
	EidosAssertScriptSuccess_L("F&0&F;", false);
	EidosAssertScriptSuccess_LV("c(T,F,T,F) & 0;", {false, false, false, false});
	EidosAssertScriptSuccess_LV("c(7,0,5,0) & T;", {true, false, true, false});
	EidosAssertScriptSuccess_LV("F & c(5,0,7,0);", {false, false, false, false});
	EidosAssertScriptSuccess_LV("9 & c(T,F,T,F);", {true, false, true, false});
	EidosAssertScriptSuccess_LV("c(7,0,5,0) & c(T,T,F,F);", {true, false, false, false});
	EidosAssertScriptSuccess_LV("c(T,F,T,F) & c(0,0,5,7);", {false, false, true, false});
	EidosAssertScriptSuccess_L("5.0&T&T;", true);
	EidosAssertScriptSuccess_L("T&5.0&F;", false);
	EidosAssertScriptSuccess_L("T&F&5.0;", false);
	EidosAssertScriptSuccess_L("5.0&F&F;", false);
	EidosAssertScriptSuccess_L("0.0&T&T;", false);
	EidosAssertScriptSuccess_L("F&T&0.0;", false);
	EidosAssertScriptSuccess_L("F&0.0&T;", false);
	EidosAssertScriptSuccess_L("F&0.0&F;", false);
	EidosAssertScriptSuccess_LV("c(T,F,T,F) & 0.0;", {false, false, false, false});
	EidosAssertScriptSuccess_LV("c(7.0,0.0,5.0,0.0) & T;", {true, false, true, false});
	EidosAssertScriptSuccess_LV("F & c(5.0,0.0,7.0,0.0);", {false, false, false, false});
	EidosAssertScriptSuccess_LV("9.0 & c(T,F,T,F);", {true, false, true, false});
	EidosAssertScriptSuccess_LV("c(7.0,0.0,5.0,0.0) & c(T,T,F,F);", {true, false, false, false});
	EidosAssertScriptSuccess_LV("c(T,F,T,F) & c(0.0,0.0,5.0,7.0);", {false, false, true, false});
	EidosAssertScriptSuccess_L("INF&T&T;", true);
	EidosAssertScriptSuccess_L("T&INF&F;", false);
	EidosAssertScriptRaise("T&NAN&F;", 1, "cannot be converted");
	EidosAssertScriptRaise("NAN&T&T;", 3, "cannot be converted");
	EidosAssertScriptRaise("c(7.0,0.0,5.0,0.0) & c(T,T,NAN,F);", 19, "cannot be converted");
	EidosAssertScriptSuccess_L("'foo'&T&T;", true);
	EidosAssertScriptSuccess_L("T&'foo'&F;", false);
	EidosAssertScriptSuccess_L("T&F&'foo';", false);
	EidosAssertScriptSuccess_L("'foo'&F&F;", false);
	EidosAssertScriptSuccess_L("''&T&T;", false);
	EidosAssertScriptSuccess_L("F&T&'';", false);
	EidosAssertScriptSuccess_L("F&''&T;", false);
	EidosAssertScriptSuccess_L("F&''&F;", false);
	EidosAssertScriptSuccess_LV("c(T,F,T,F) & '';", {false, false, false, false});
	EidosAssertScriptSuccess_LV("c('foo','','foo','') & T;", {true, false, true, false});
	EidosAssertScriptSuccess_LV("F & c('foo','','foo','');", {false, false, false, false});
	EidosAssertScriptSuccess_LV("'foo' & c(T,F,T,F);", {true, false, true, false});
	EidosAssertScriptSuccess_LV("c('foo','','foo','') & c(T,T,F,F);", {true, false, false, false});
	EidosAssertScriptSuccess_LV("c(T,F,T,F) & c('','','foo','foo');", {false, false, true, false});
	
	// operator &: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with logical should suffice
	EidosAssertScriptSuccess_L("identical(T & T, T);", true);
	EidosAssertScriptSuccess_L("identical(T & F, F);", true);
	EidosAssertScriptSuccess_L("identical(T & matrix(T), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(T & F & matrix(T), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(T & matrix(T) & F, matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(T & matrix(T) & matrix(T) & T, matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(T & matrix(T) & matrix(F) & T, matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(T & matrix(T) & matrix(F) & c(T,F,T), c(F,F,F));", true);
	EidosAssertScriptSuccess_L("identical(T & matrix(T) & matrix(T) & c(T,F,T), c(T,F,T));", true);
	EidosAssertScriptSuccess_L("identical(c(T,F,T) & T & matrix(T) & matrix(F), c(F,F,F));", true);
	EidosAssertScriptSuccess_L("identical(c(T,F,T) & T & matrix(T) & matrix(T), c(T,F,T));", true);
	EidosAssertScriptRaise("identical(c(T,F,T) & T & matrix(c(T,T,F)) & matrix(F), c(T,F,F));", 19, "non-conformable");
	EidosAssertScriptSuccess_L("identical(c(T,F,T) & T & matrix(c(T,T,F)) & matrix(c(T,F,T)), matrix(c(T,F,F)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(T) & T, matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(T) & T & F, matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(matrix(T) & matrix(T) & T & T, matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(T) & matrix(F) & T & T, matrix(F));", true);
	EidosAssertScriptRaise("identical(matrix(T) & matrix(c(T,F)) & T & T, matrix(F));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(c(T,F)) & matrix(F) & T & T, matrix(F));", 25, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(c(T,T)) & matrix(c(T,T)) & T & T, matrix(c(T,T)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(T,T)) & matrix(c(T,F)) & T & T, matrix(c(T,F)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(T,T,T)) & matrix(c(T,F,F)) & c(T,F,T) & T, matrix(c(T,F,F)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,T)) & matrix(c(T,T,F)) & c(F,T,T) & T, matrix(c(F,T,F)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(T) & T & matrix(F) & c(T,F,T), c(F,F,F));", true);
	EidosAssertScriptSuccess_L("identical(matrix(T) & T & matrix(T) & c(T,F,T), c(T,F,T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(T) & c(T,F,T) & T & matrix(F), c(F,F,F));", true);
	EidosAssertScriptSuccess_L("identical(matrix(T) & c(T,F,T) & T & matrix(T), c(T,F,T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(T) & matrix(T), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(F) & matrix(F), matrix(F));", true);
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
	EidosAssertScriptSuccess_L("T|T|T;", true);
	EidosAssertScriptSuccess_L("T|T|F;", true);
	EidosAssertScriptSuccess_L("T|F|T;", true);
	EidosAssertScriptSuccess_L("T|F|F;", true);
	EidosAssertScriptSuccess_L("F|T|T;", true);
	EidosAssertScriptSuccess_L("F|T|F;", true);
	EidosAssertScriptSuccess_L("F|F|T;", true);
	EidosAssertScriptSuccess_L("F|F|F;", false);
	EidosAssertScriptSuccess_LV("c(T,F,T,F) | F;", {true, false, true, false});
	EidosAssertScriptSuccess_LV("c(T,F,T,F) | T;", {true, true, true, true});
	EidosAssertScriptSuccess_LV("F | c(T,F,T,F);", {true, false, true, false});
	EidosAssertScriptSuccess_LV("T | c(T,F,T,F);", {true, true, true, true});
	EidosAssertScriptSuccess_LV("c(T,F,T,F) | c(T,T,F,F);", {true, true, true, false});
	EidosAssertScriptSuccess_LV("c(T,F,T,F) | c(F,F,T,T);", {true, false, true, true});
	EidosAssertScriptSuccess_LV("c(T,T,F,F) | c(T,F,T,F);", {true, true, true, false});
	EidosAssertScriptSuccess_LV("c(F,F,T,T) | c(T,F,T,F);", {true, false, true, true});
	EidosAssertScriptRaise("c(T,F,T,F) | c(F,F);", 11, "not compatible in size()");
	EidosAssertScriptRaise("c(T,T) | c(T,F,T,F);", 7, "not compatible in size()");
	EidosAssertScriptRaise("c(T,F,T,F) | _Test(3);", 11, "is not supported by");
	EidosAssertScriptRaise("_Test(3) | c(T,F,T,F);", 9, "is not supported by");
	EidosAssertScriptSuccess_L("5|T|T;", true);
	EidosAssertScriptSuccess_L("T|5|F;", true);
	EidosAssertScriptSuccess_L("T|F|5;", true);
	EidosAssertScriptSuccess_L("5|F|F;", true);
	EidosAssertScriptSuccess_L("0|T|T;", true);
	EidosAssertScriptSuccess_L("F|T|0;", true);
	EidosAssertScriptSuccess_L("F|0|T;", true);
	EidosAssertScriptSuccess_L("F|0|F;", false);
	EidosAssertScriptSuccess_LV("c(T,F,T,F) | 0;", {true, false, true, false});
	EidosAssertScriptSuccess_LV("c(7,0,5,0) | T;", {true, true, true, true});
	EidosAssertScriptSuccess_LV("F | c(5,0,7,0);", {true, false, true, false});
	EidosAssertScriptSuccess_LV("9 | c(T,F,T,F);", {true, true, true, true});
	EidosAssertScriptSuccess_LV("c(7,0,5,0) | c(T,T,F,F);", {true, true, true, false});
	EidosAssertScriptSuccess_LV("c(T,F,T,F) | c(0,0,5,7);", {true, false, true, true});
	EidosAssertScriptSuccess_L("5.0|T|T;", true);
	EidosAssertScriptSuccess_L("T|5.0|F;", true);
	EidosAssertScriptSuccess_L("T|F|5.0;", true);
	EidosAssertScriptSuccess_L("5.0|F|F;", true);
	EidosAssertScriptSuccess_L("0.0|T|T;", true);
	EidosAssertScriptSuccess_L("F|T|0.0;", true);
	EidosAssertScriptSuccess_L("F|0.0|T;", true);
	EidosAssertScriptSuccess_L("F|0.0|F;", false);
	EidosAssertScriptSuccess_LV("c(T,F,T,F) | 0.0;", {true, false, true, false});
	EidosAssertScriptSuccess_LV("c(7.0,0.0,5.0,0.0) | T;", {true, true, true, true});
	EidosAssertScriptSuccess_LV("F | c(5.0,0.0,7.0,0.0);", {true, false, true, false});
	EidosAssertScriptSuccess_LV("9.0 | c(T,F,T,F);", {true, true, true, true});
	EidosAssertScriptSuccess_LV("c(7.0,0.0,5.0,0.0) | c(T,T,F,F);", {true, true, true, false});
	EidosAssertScriptSuccess_LV("c(T,F,T,F) | c(0.0,0.0,5.0,7.0);", {true, false, true, true});
	EidosAssertScriptSuccess_L("INF|T|T;", true);
	EidosAssertScriptSuccess_L("T|INF|F;", true);
	EidosAssertScriptRaise("T|NAN|F;", 1, "cannot be converted");
	EidosAssertScriptRaise("NAN|T|T;", 3, "cannot be converted");
	EidosAssertScriptRaise("c(7.0,0.0,5.0,0.0) | c(T,T,NAN,F);", 19, "cannot be converted");
	EidosAssertScriptSuccess_L("'foo'|T|T;", true);
	EidosAssertScriptSuccess_L("T|'foo'|F;", true);
	EidosAssertScriptSuccess_L("T|F|'foo';", true);
	EidosAssertScriptSuccess_L("'foo'|F|F;", true);
	EidosAssertScriptSuccess_L("''|T|T;", true);
	EidosAssertScriptSuccess_L("F|T|'';", true);
	EidosAssertScriptSuccess_L("F|''|T;", true);
	EidosAssertScriptSuccess_L("F|''|F;", false);
	EidosAssertScriptSuccess_LV("c(T,F,T,F) | '';", {true, false, true, false});
	EidosAssertScriptSuccess_LV("c('foo','','foo','') | T;", {true, true, true, true});
	EidosAssertScriptSuccess_LV("F | c('foo','','foo','');", {true, false, true, false});
	EidosAssertScriptSuccess_LV("'foo' | c(T,F,T,F);", {true, true, true, true});
	EidosAssertScriptSuccess_LV("c('foo','','foo','') | c(T,T,F,F);", {true, true, true, false});
	EidosAssertScriptSuccess_LV("c(T,F,T,F) | c('','','foo','foo');", {true, false, true, true});
	
	// operator |: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with logical should suffice
	EidosAssertScriptSuccess_L("identical(T | F, T);", true);
	EidosAssertScriptSuccess_L("identical(F | F, F);", true);
	EidosAssertScriptSuccess_L("identical(T | matrix(F), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(F | F | matrix(T), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(F | matrix(F) | F, matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(F | matrix(F) | matrix(T) | F, matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(F | matrix(F) | matrix(F) | T, matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(F | matrix(T) | matrix(F) | c(T,F,T), c(T,T,T));", true);
	EidosAssertScriptSuccess_L("identical(F | matrix(F) | matrix(F) | c(T,F,T), c(T,F,T));", true);
	EidosAssertScriptSuccess_L("identical(c(T,F,T) | T | matrix(F) | matrix(F), c(T,T,T));", true);
	EidosAssertScriptSuccess_L("identical(c(T,F,T) | F | matrix(T) | matrix(F), c(T,T,T));", true);
	EidosAssertScriptRaise("identical(c(T,F,T) | F | matrix(c(T,T,F)) | matrix(F), c(T,T,F));", 19, "non-conformable");
	EidosAssertScriptSuccess_L("identical(c(T,F,T) | F | matrix(c(T,F,F)) | matrix(c(T,F,T)), matrix(c(T,F,T)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(T) | F, matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(F) | F | F, matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(matrix(F) | matrix(F) | T | F, matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(T) | matrix(F) | F | F, matrix(T));", true);
	EidosAssertScriptRaise("identical(matrix(T) | matrix(c(T,F)) | T | T, matrix(F));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(c(T,F)) | matrix(F) | T | T, matrix(F));", 25, "non-conformable");
	EidosAssertScriptSuccess_L("identical(matrix(c(T,F)) | matrix(c(F,F)) | F | F, matrix(c(T,F)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T)) | matrix(c(F,F)) | F | T, matrix(c(T,T)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,F)) | matrix(c(T,F,F)) | c(F,F,F) | F, matrix(c(T,T,F)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(c(F,T,T)) | matrix(c(F,T,F)) | c(F,F,F) | T, matrix(c(T,T,T)));", true);
	EidosAssertScriptSuccess_L("identical(matrix(T) | F | matrix(F) | c(T,F,T), c(T,T,T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(F) | F | matrix(F) | c(T,F,T), c(T,F,T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(F) | c(T,F,T) | T | matrix(F), c(T,T,T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(F) | c(T,F,F) | F | matrix(F), c(T,F,F));", true);
	EidosAssertScriptSuccess_L("identical(matrix(T) | matrix(T), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(F) | matrix(F), matrix(F));", true);
}

#pragma mark operator !
void _RunOperatorLogicalNotTests(void)
{
	// operator !
	EidosAssertScriptRaise("!NULL;", 0, "is not supported by");
	EidosAssertScriptSuccess_L("!T;", false);
	EidosAssertScriptSuccess_L("!F;", true);
	EidosAssertScriptSuccess_L("!7;", false);
	EidosAssertScriptSuccess_L("!0;", true);
	EidosAssertScriptSuccess_L("!7.1;", false);
	EidosAssertScriptSuccess_L("!0.0;", true);
	EidosAssertScriptSuccess_L("!INF;", false);
	EidosAssertScriptRaise("!NAN;", 0, "cannot be converted");
	EidosAssertScriptSuccess_L("!'foo';", false);
	EidosAssertScriptSuccess_L("!'';", true);
	EidosAssertScriptSuccess_LV("!logical(0);", {});
	EidosAssertScriptSuccess_LV("!integer(0);", {});
	EidosAssertScriptSuccess_LV("!float(0);", {});
	EidosAssertScriptSuccess_LV("!string(0);", {});
	EidosAssertScriptRaise("!object();", 0, "is not supported by");
	EidosAssertScriptSuccess_LV("!c(F,T,F,T);", {true, false, true, false});
	EidosAssertScriptSuccess_LV("!c(0,5,0,1);", {true, false, true, false});
	EidosAssertScriptSuccess_LV("!c(0,5.0,0,1.0);", {true, false, true, false});
	EidosAssertScriptRaise("!c(0,NAN,0,1.0);", 0, "cannot be converted");
	EidosAssertScriptSuccess_LV("!c(0,INF,0,1.0);", {true, false, true, false});
	EidosAssertScriptSuccess_LV("!c('','foo','','bar');", {true, false, true, false});
	EidosAssertScriptRaise("!_Test(5);", 0, "is not supported by");
	
	// operator |: test with matrices and arrays; the dimensionality code is shared across all operand types, so testing it with logical should suffice
	EidosAssertScriptSuccess_L("identical(!T, F);", true);
	EidosAssertScriptSuccess_L("identical(!F, T);", true);
	EidosAssertScriptSuccess_L("identical(!c(T,F,T), c(F,T,F));", true);
	EidosAssertScriptSuccess_L("identical(!c(F,T,F), c(T,F,T));", true);
	EidosAssertScriptSuccess_L("identical(!matrix(T), matrix(F));", true);
	EidosAssertScriptSuccess_L("identical(!matrix(F), matrix(T));", true);
	EidosAssertScriptSuccess_L("identical(!matrix(c(T,F,T)), matrix(c(F,T,F)));", true);
	EidosAssertScriptSuccess_L("identical(!matrix(c(F,T,F)), matrix(c(T,F,T)));", true);
	EidosAssertScriptSuccess_L("identical(!array(T, c(1,1,1)), array(F, c(1,1,1)));", true);
	EidosAssertScriptSuccess_L("identical(!array(F, c(1,1,1)), array(T, c(1,1,1)));", true);
	EidosAssertScriptSuccess_L("identical(!array(c(T,F,T), c(3,1,1)), array(c(F,T,F), c(3,1,1)));", true);
	EidosAssertScriptSuccess_L("identical(!array(c(F,T,F), c(1,3,1)), array(c(T,F,T), c(1,3,1)));", true);
	EidosAssertScriptSuccess_L("identical(!array(c(T,F,T), c(1,1,3)), array(c(F,T,F), c(1,1,3)));", true);
}

#pragma mark operator ?
void _RunOperatorTernaryConditionalTests(void)
{
	// operator ?-else
	EidosAssertScriptSuccess_I("T ? 23 else 42;", 23);
	EidosAssertScriptSuccess_I("F ? 23 else 42;", 42);
	EidosAssertScriptSuccess_I("9 ? 23 else 42;", 23);
	EidosAssertScriptSuccess_I("0 ? 23 else 42;", 42);
	EidosAssertScriptSuccess_I("6 > 5 ? 23 else 42;", 23);
	EidosAssertScriptSuccess_I("6 < 5 ? 23 else 42;", 42);
	EidosAssertScriptRaise("6 == 6:9 ? 23 else 42;", 9, "condition for ternary conditional has size()");
	EidosAssertScriptSuccess_I("(6 == (6:9))[0] ? 23 else 42;", 23);
	EidosAssertScriptSuccess_I("(6 == (6:9))[1] ? 23 else 42;", 42);
	EidosAssertScriptRaise("NAN ? 23 else 42;", 4, "cannot be converted");
	EidosAssertScriptRaise("_Test(6) ? 23 else 42;", 9, "cannot be converted");
	EidosAssertScriptRaise("NULL ? 23 else 42;", 5, "condition for ternary conditional has size()");
	EidosAssertScriptRaise("T ? 23; else 42;", 6, "expected 'else'");
	EidosAssertScriptRaise("T ? 23; x = 10;", 6, "expected 'else'");
	EidosAssertScriptRaise("(T ? x else y) = 10;", 15, "lvalue required");
	EidosAssertScriptSuccess_I("x = T ? 23 else 42; x;", 23);
	EidosAssertScriptSuccess_I("x = F ? 23 else 42; x;", 42);
	
	// test right-associativity; this produces 2 if ? else is left-associative since the left half would then evaluate to 1, which is T
	EidosAssertScriptSuccess_I("a = 0; a == 0 ? 1 else a == 1 ? 2 else 4;", 1);
}
	
	// ************************************************************************************
	//
	//	Keyword tests
	//
	
#pragma mark if
void _RunKeywordIfTests(void)
{
	// if
	EidosAssertScriptSuccess_I("if (T) 23;", 23);
	EidosAssertScriptSuccess_VOID("if (F) 23;");
	EidosAssertScriptSuccess_I("if (9) 23;", 23);
	EidosAssertScriptSuccess_VOID("if (0) 23;");
	EidosAssertScriptSuccess_I("if (6 > 5) 23;", 23);
	EidosAssertScriptSuccess_VOID("if (6 < 5) 23;");
	EidosAssertScriptRaise("if (6 == (6:9)) 23;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess_I("if ((6 == (6:9))[0]) 23;", 23);
	EidosAssertScriptSuccess_VOID("if ((6 == (6:9))[1]) 23;");
	EidosAssertScriptRaise("if (NAN) 23;", 0, "cannot be converted");
	EidosAssertScriptRaise("if (_Test(6)) 23;", 0, "cannot be converted");
	EidosAssertScriptRaise("if (NULL) 23;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess_I("if (matrix(1)) 23;", 23);
	EidosAssertScriptSuccess_VOID("if (matrix(0)) 23;");
	EidosAssertScriptRaise("if (matrix(1:3)) 23;", 0, "condition for if statement has size()");
	
	// if-else
	EidosAssertScriptSuccess_I("if (T) 23; else 42;", 23);
	EidosAssertScriptSuccess_I("if (F) 23; else 42;", 42);
	EidosAssertScriptSuccess_I("if (9) 23; else 42;", 23);
	EidosAssertScriptSuccess_I("if (0) 23; else 42;", 42);
	EidosAssertScriptSuccess_I("if (6 > 5) 23; else 42;", 23);
	EidosAssertScriptSuccess_I("if (6 < 5) 23; else 42;", 42);
	EidosAssertScriptRaise("if (6 == (6:9)) 23; else 42;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess_I("if ((6 == (6:9))[0]) 23; else 42;", 23);
	EidosAssertScriptSuccess_I("if ((6 == (6:9))[1]) 23; else 42;", 42);
	EidosAssertScriptRaise("if (NAN) 23; else 42;", 0, "cannot be converted");
	EidosAssertScriptRaise("if (_Test(6)) 23; else 42;", 0, "cannot be converted");
	EidosAssertScriptRaise("if (NULL) 23; else 42;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess_I("if (matrix(1)) 23; else 42;", 23);
	EidosAssertScriptSuccess_I("if (matrix(0)) 23; else 42;", 42);
	EidosAssertScriptRaise("if (matrix(1:3)) 23; else 42;", 0, "condition for if statement has size()");
}

#pragma mark do
void _RunKeywordDoTests(void)
{
	// do
	EidosAssertScriptSuccess_I("x=1; do x=x*2; while (x<100); x;", 128);
	EidosAssertScriptSuccess_I("x=200; do x=x*2; while (x<100); x;", 400);
	EidosAssertScriptSuccess_I("x=1; do { x=x*2; x=x+1; } while (x<100); x;", 127);
	EidosAssertScriptSuccess_I("x=200; do { x=x*2; x=x+1; } while (x<100); x;", 401);
	EidosAssertScriptRaise("x=1; do x=x*2; while (x < 100:102); x;", 5, "condition for do-while loop has size()");
	EidosAssertScriptRaise("x=200; do x=x*2; while (x < 100:102); x;", 7, "condition for do-while loop has size()");
	EidosAssertScriptSuccess_I("x=1; do x=x*2; while ((x < 100:102)[0]); x;", 128);
	EidosAssertScriptSuccess_I("x=200; do x=x*2; while ((x < 100:102)[0]); x;", 400);
	EidosAssertScriptRaise("x=200; do x=x*2; while (NAN); x;", 7, "cannot be converted");
	EidosAssertScriptRaise("x=200; do x=x*2; while (_Test(6)); x;", 7, "cannot be converted");
	EidosAssertScriptRaise("x=200; do x=x*2; while (NULL); x;", 7, "condition for do-while loop has size()");
	EidosAssertScriptSuccess_I("x=10; do x=x-1; while (x); x;", 0);
}

#pragma mark while
void _RunKeywordWhileTests(void)
{
	// while
	EidosAssertScriptSuccess_I("x=1; while (x<100) x=x*2; x;", 128);
	EidosAssertScriptSuccess_I("x=200; while (x<100) x=x*2; x;", 200);
	EidosAssertScriptSuccess_I("x=1; while (x<100) { x=x*2; x=x+1; } x;", 127);
	EidosAssertScriptSuccess_I("x=200; while (x<100) { x=x*2; x=x+1; } x;", 200);
	EidosAssertScriptRaise("x=1; while (x < 100:102) x=x*2; x;", 5, "condition for while loop has size()");
	EidosAssertScriptRaise("x=200; while (x < 100:102) x=x*2; x;", 7, "condition for while loop has size()");
	EidosAssertScriptSuccess_I("x=1; while ((x < 100:102)[0]) x=x*2; x;", 128);
	EidosAssertScriptSuccess_I("x=200; while ((x < 100:102)[0]) x=x*2; x;", 200);
	EidosAssertScriptRaise("x=200; while (NAN) x=x*2; x;", 7, "cannot be converted");
	EidosAssertScriptRaise("x=200; while (_Test(6)) x=x*2; x;", 7, "cannot be converted");
	EidosAssertScriptRaise("x=200; while (NULL) x=x*2; x;", 7, "condition for while loop has size()");
	EidosAssertScriptSuccess_I("x=10; while (x) x=x-1; x;", 0);
}

#pragma mark for / in
void _RunKeywordForInTests(void)
{
	// for and in
	EidosAssertScriptSuccess("x=0; for (y in integer(0)) x=x+1; x;", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("x=0; for (y in float(0)) x=x+1; x;", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess_I("x=0; for (y in 33) x=x+y; x;", 33);
	EidosAssertScriptSuccess("x=0; for (y in 33) x=x+1; x;", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess_I("x=0; for (y in 1:10) x=x+y; x;", 55);
	EidosAssertScriptSuccess_I("x=0; for (y in 1:10) x=x+1; x;", 10);
	EidosAssertScriptSuccess_I("x=0; for (y in 1:10) { x=x+y; y = 7; } x;", 55);
	EidosAssertScriptSuccess_I("x=0; for (y in 1:10) { x=x+1; y = 7; } x;", 10);
	EidosAssertScriptSuccess_I("x=0; for (y in 10:1) x=x+y; x;", 55);
	EidosAssertScriptSuccess_I("x=0; for (y in 10:1) x=x+1; x;", 10);
	EidosAssertScriptSuccess_F("x=0; for (y in 1.0:10) x=x+y; x;", 55.0);
	EidosAssertScriptSuccess_I("x=0; for (y in 1.0:10) x=x+1; x;", 10);
	EidosAssertScriptSuccess_F("x=0; for (y in 1:10.0) x=x+y; x;", 55.0);
	EidosAssertScriptSuccess_I("x=0; for (y in 1:10.0) x=x+1; x;", 10);
	EidosAssertScriptSuccess_S("x=0; for (y in c('foo', 'bar')) x=x+y; x;", "0foobar");
	EidosAssertScriptSuccess_I("x=0; for (y in c(T,T,F,F,T,F)) x=x+asInteger(y); x;", 3);
	EidosAssertScriptSuccess_I("x=0; for (y in _Test(7)) x=x+y._yolk; x;", 7);
	EidosAssertScriptSuccess_I("x=0; for (y in rep(_Test(7),3)) x=x+y._yolk; x;", 21);
	EidosAssertScriptRaise("x=0; y=0:2; for (y[0] in 2:4) x=x+sum(y); x;", 18, "unexpected token");	// lvalue must be an identifier, at present
	EidosAssertScriptRaise("x=0; y=0:2; for (y.z in 2:4) x=x+sum(y); x;", 18, "unexpected token");	// lvalue must be an identifier, at present
	EidosAssertScriptRaise("x=0; for (y in NULL) x;", 5, "does not allow NULL");
	EidosAssertScriptSuccess_I("x=0; q=11:20; for (y in seqAlong(q)) x=x+y; x;", 45);
	EidosAssertScriptSuccess_I("x=0; q=11:20; for (y in seqAlong(q)) x=x+1; x;", 10);
	EidosAssertScriptRaise("x=0; q=11:20; for (y in seqAlong(q, 5)) x=x+y; x;", 24, "too many arguments supplied");
	EidosAssertScriptRaise("x=0; q=11:20; for (y in seqAlong()) x=x+y; x;", 24, "missing required");
	EidosAssertScriptSuccess_I("x=0; for (y in seq(1,10)) x=x+y; x;", 55);
	EidosAssertScriptSuccess_I("x=0; for (y in seq(1,10)) x=x+1; x;", 10);
	EidosAssertScriptSuccess_I("x=0; for (y in seqLen(5)) x=x+y+2; x;", 20);
	EidosAssertScriptSuccess_I("x=0; for (y in seqLen(1)) x=x+y+2; x;", 2);
	EidosAssertScriptSuccess_I("x=0; for (y in seqLen(0)) x=x+y+2; x;", 0);
	EidosAssertScriptRaise("x=0; for (y in seqLen(-1)) x=x+y+2; x;", 15, "requires length to be");
	EidosAssertScriptRaise("x=0; for (y in seqLen(5:6)) x=x+y+2; x;", 15, "must be a singleton");
	EidosAssertScriptRaise("x=0; for (y in seqLen('f')) x=x+y+2; x;", 15, "cannot be type");
	
	// additional tests for zero-length ranges; seqAlong() is treated separately in the for() code, so it is tested separately here
	EidosAssertScriptSuccess_I("i=10; for (i in integer(0)) ; i;", 10);
	EidosAssertScriptSuccess_I("i=10; for (i in seqAlong(integer(0))) ; i;", 10);
	EidosAssertScriptSuccess_I("i=10; b=13; for (i in integer(0)) b=i; i;", 10);
	EidosAssertScriptSuccess_I("i=10; b=13; for (i in seqAlong(integer(0))) b=i; i;", 10);
	EidosAssertScriptSuccess_I("i=10; b=13; for (i in integer(0)) b=i; b;", 13);
	EidosAssertScriptSuccess_I("i=10; b=13; for (i in seqAlong(integer(0))) b=i; b;", 13);
	
	EidosAssertScriptRaise("for (i in matrix(5):9) i;", 19, "must not be matrices or arrays");
	EidosAssertScriptRaise("for (i in 1:matrix(5)) i;", 11, "must not be matrices or arrays");
	EidosAssertScriptRaise("for (i in matrix(3):matrix(5)) i;", 19, "must not be matrices or arrays");
	EidosAssertScriptRaise("for (i in matrix(5:8):9) i;", 21, "must have size() == 1");
	EidosAssertScriptRaise("for (i in 1:matrix(5:8)) i;", 11, "must have size() == 1");
	EidosAssertScriptRaise("for (i in matrix(1:3):matrix(5:7)) i;", 21, "must have size() == 1");
	EidosAssertScriptSuccess_I("x = 0; for (i in seqAlong(matrix(1))) x=x+i; x;", 0);
	EidosAssertScriptSuccess_I("x = 0; for (i in seqAlong(matrix(1:3))) x=x+i; x;", 3);
}

#pragma mark next
void _RunKeywordNextTests(void)
{
	// next
	EidosAssertScriptRaise("next;", 0, "encountered with no enclosing loop");
	EidosAssertScriptRaise("if (T) next;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess_VOID("if (F) next;");
	EidosAssertScriptRaise("if (T) next; else 42;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess_I("if (F) next; else 42;", 42);
	EidosAssertScriptSuccess_I("if (T) 23; else next;", 23);
	EidosAssertScriptRaise("if (F) 23; else next;", 16, "encountered with no enclosing loop");
	EidosAssertScriptSuccess_I("x=1; do { x=x*2; if (x>50) next; x=x+1; } while (x<100); x;", 124);
	EidosAssertScriptSuccess_I("x=1; while (x<100) { x=x*2; if (x>50) next; x=x+1; } x;", 124);
	EidosAssertScriptSuccess_I("x=0; for (y in 1:10) { if (y==5) next; x=x+y; } x;", 50);
}

#pragma mark break
void _RunKeywordBreakTests(void)
{
	// break
	EidosAssertScriptRaise("break;", 0, "encountered with no enclosing loop");
	EidosAssertScriptRaise("if (T) break;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess_VOID("if (F) break;");
	EidosAssertScriptRaise("if (T) break; else 42;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess_I("if (F) break; else 42;", 42);
	EidosAssertScriptSuccess_I("if (T) 23; else break;", 23);
	EidosAssertScriptRaise("if (F) 23; else break;", 16, "encountered with no enclosing loop");
	EidosAssertScriptSuccess_I("x=1; do { x=x*2; if (x>50) break; x=x+1; } while (x<100); x;", 62);
	EidosAssertScriptSuccess_I("x=1; while (x<100) { x=x*2; if (x>50) break; x=x+1; } x;", 62);
	EidosAssertScriptSuccess_I("x=0; for (y in 1:10) { if (y==5) break; x=x+y; } x;", 10);
}

#pragma mark return
void _RunKeywordReturnTests(void)
{
	// return
	EidosAssertScriptSuccess_VOID("return;");
	EidosAssertScriptSuccess_NULL("return NULL;");
	EidosAssertScriptSuccess_I("return -13;", -13);
	EidosAssertScriptSuccess_VOID("if (T) return;");
	EidosAssertScriptSuccess_NULL("if (T) return NULL;");
	EidosAssertScriptSuccess_I("if (T) return -13;", -13);
	EidosAssertScriptSuccess_VOID("if (F) return;");
	EidosAssertScriptSuccess_VOID("if (F) return NULL;");
	EidosAssertScriptSuccess_VOID("if (F) return -13;");
	EidosAssertScriptSuccess_VOID("if (T) return; else return 42;");
	EidosAssertScriptSuccess_NULL("if (T) return NULL; else return 42;");
	EidosAssertScriptSuccess_I("if (T) return -13; else return 42;", -13);
	EidosAssertScriptSuccess_I("if (F) return; else return 42;", 42);
	EidosAssertScriptSuccess_I("if (F) return -13; else return 42;", 42);
	EidosAssertScriptSuccess_I("if (T) return 23; else return;", 23);
	EidosAssertScriptSuccess_I("if (T) return 23; else return -13;", 23);
	EidosAssertScriptSuccess_VOID("if (F) return 23; else return;");
	EidosAssertScriptSuccess_NULL("if (F) return 23; else return NULL;");
	EidosAssertScriptSuccess_I("if (F) return 23; else return -13;", -13);
	EidosAssertScriptSuccess_VOID("x=1; do { x=x*2; if (x>50) return; x=x+1; } while (x<100); x;");
	EidosAssertScriptSuccess_I("x=1; do { x=x*2; if (x>50) return x-5; x=x+1; } while (x<100); x;", 57);
	EidosAssertScriptSuccess_VOID("x=1; while (x<100) { x=x*2; if (x>50) return; x=x+1; } x;");
	EidosAssertScriptSuccess_I("x=1; while (x<100) { x=x*2; if (x>50) return x-5; x=x+1; } x;", 57);
	EidosAssertScriptSuccess_VOID("x=0; for (y in 1:10) { if (y==5) return; x=x+y; } x;");
	EidosAssertScriptSuccess_I("x=0; for (y in 1:10) { if (y==5) return x-5; x=x+y; } x;", 5);
}






































