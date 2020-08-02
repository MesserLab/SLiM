//
//  eidos_test_operators_other.cpp
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


#pragma mark operator []
void _RunOperatorSubsetTests(void)
{
	// operator []
	EidosAssertScriptSuccess("x = 1:5; x[NULL];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; NULL[x];", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x = 1:5; NULL[NULL];", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x = 1:5; x[];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[integer(0)];", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = 1:5; x[2];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("x = 1:5; x[2:3];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4}));
	EidosAssertScriptSuccess("x = 1:5; x[c(0, 2, 4)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[0:4];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[float(0)];", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess("x = 1:5; x[2.0];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("x = 1:5; x[2.0:3];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4}));
	EidosAssertScriptSuccess("x = 1:5; x[c(0.0, 2, 4)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[0.0:4];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptRaise("x = 1:5; x[c(7,8)];", 10, "out-of-range index");
	EidosAssertScriptRaise("x = 1:5; x[logical(0)];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[T];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[c(T, T)];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[c(T, F, T)];", 10, "operator requires that the size()");
	EidosAssertScriptRaise("x = 1:5; x[NAN];", 10, "cannot be converted");
	EidosAssertScriptRaise("x = 1:5; x[c(0.0, 2, NAN)];", 10, "cannot be converted");
	EidosAssertScriptSuccess("x = 1:5; x[c(T, F, T, F, T)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[c(T, T, T, T, T)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[c(F, F, F, F, F)];", gStaticEidosValue_Integer_ZeroVec);

	EidosAssertScriptSuccess("x = c(T,T,F,T,F); x[c(T, F, T, F, T)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false}));
	EidosAssertScriptSuccess("x = 1.0:5; x[c(T, F, T, F, T)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 3.0, 5.0}));
	EidosAssertScriptSuccess("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(T, F, T, F, T)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "foobaz", "xyzzy"}));
	
	EidosAssertScriptSuccess("x = c(T,T,F,T,F); x[c(2,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptRaise("x = c(T,T,F,T,F); x[c(2,3,7)];", 19, "out-of-range index");
	EidosAssertScriptSuccess("x = c(T,T,F,T,F); x[c(2.0,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, true}));
	EidosAssertScriptRaise("x = c(T,T,F,T,F); x[c(2.0,3,7)];", 19, "out-of-range index");
	
	EidosAssertScriptSuccess("x = 1:5; x[c(2,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4}));
	EidosAssertScriptRaise("x = 1:5; x[c(2,3,7)];", 10, "out-of-range index");
	EidosAssertScriptSuccess("x = 1:5; x[c(2.0,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4}));
	EidosAssertScriptRaise("x = 1:5; x[c(2.0,3,7)];", 10, "out-of-range index");
	
	EidosAssertScriptSuccess("x = 1.0:5; x[c(2,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.0, 4.0}));
	EidosAssertScriptRaise("x = 1.0:5; x[c(2,3,7)];", 12, "out-of-range index");
	EidosAssertScriptSuccess("x = 1.0:5; x[c(2.0,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3.0, 4.0}));
	EidosAssertScriptRaise("x = 1.0:5; x[c(2.0,3,7)];", 12, "out-of-range index");
	
	EidosAssertScriptSuccess("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(2,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foobaz", "baz"}));
	EidosAssertScriptRaise("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(2,3,7)];", 48, "out-of-range index");
	EidosAssertScriptSuccess("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(2.0,3)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foobaz", "baz"}));
	EidosAssertScriptRaise("x = c('foo', 'bar', 'foobaz', 'baz', 'xyzzy'); x[c(2.0,3,7)];", 48, "out-of-range index");
	
	EidosAssertScriptSuccess("x = c(_Test(1), _Test(2), _Test(3), _Test(4), _Test(5)); x = x[c(2,3)]; x._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4}));
	EidosAssertScriptRaise("x = c(_Test(1), _Test(2), _Test(3), _Test(4), _Test(5)); x = x[c(2,3,7)]; x._yolk;", 62, "out-of-range index");
	EidosAssertScriptSuccess("x = c(_Test(1), _Test(2), _Test(3), _Test(4), _Test(5)); x = x[c(2.0,3)]; x._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 4}));
	EidosAssertScriptRaise("x = c(_Test(1), _Test(2), _Test(3), _Test(4), _Test(5)); x = x[c(2.0,3,7)]; x._yolk;", 62, "out-of-range index");
	
	EidosAssertScriptSuccess("x = 5; x[T];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("x = 5; x[F];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{}));
	EidosAssertScriptRaise("x = 5; x[logical(0)];", 8, "must match the size()");
	EidosAssertScriptSuccess("x = 5; x[0];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptRaise("x = 5; x[1];", 8, "out of range");
	EidosAssertScriptRaise("x = 5; x[-1];", 8, "out of range");
	EidosAssertScriptSuccess("x = 5; x[integer(0)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{}));
	EidosAssertScriptSuccess("x = 5; x[0.0];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptRaise("x = 5; x[1.0];", 8, "out of range");
	EidosAssertScriptRaise("x = 5; x[-1.0];", 8, "out of range");
	EidosAssertScriptSuccess("x = 5; x[float(0)];", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{}));
	
	EidosAssertScriptRaise("x = 5:9; x[matrix(0)];", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(0:2)];", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(T)];", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(c(T,T,F,T,F))];", 10, "matrix or array index operand is not supported");
	
	// matrix/array subsets, without dropping
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[], 1:6);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,], matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[NULL,NULL], matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[0,], matrix(c(1,3,5), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[1,], matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[1,NULL], matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[0:1,], matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[NULL,], matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,0], matrix(1:2, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,1], matrix(3:4, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,2], matrix(5:6, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,0:1], matrix(1:4, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,1:2], matrix(3:6, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,c(0,2)], matrix(c(1,2,5,6), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[NULL,c(0,2)], matrix(c(1,2,5,6), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[0,1], matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[1,2], matrix(6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[0,c(T,F,T)], matrix(c(1,5), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[c(F,T),c(F,F,T)], matrix(6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[c(F,F),c(F,F,F)], integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[c(F,F),c(F,T,T)], integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[c(T,T),c(T,T,F)], matrix(1:4, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[c(0,0,1,0),], matrix(c(1,3,5,1,3,5,2,4,6,1,3,5), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[c(0,0,1,0),c(1,2,1)], matrix(c(3,5,3,3,5,3,4,6,4,3,5,3), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); identical(x[,c(1,2,1)], matrix(c(3,4,5,6,3,4), nrow=2));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T),c(T,T,F)];", 26, "match the corresponding dimension");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T,T,T),c(T,T,F)];", 26, "match the corresponding dimension");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T,T),c(T,T)];", 26, "match the corresponding dimension");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T,T),c(T,T,F,T)];", 26, "match the corresponding dimension");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[-1,];", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[2,];", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[,-1];", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[,3];", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0,0,0];", 26, "too many subset arguments");
	
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[], 1:12);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[,,], array(1:12, c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[NULL,NULL,NULL], array(1:12, c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[0,,], array(c(1,3,5,7,9,11), c(1,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[1,,], array(c(2,4,6,8,10,12), c(1,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[1,NULL,NULL], array(c(2,4,6,8,10,12), c(1,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[0:1,,], array(1:12, c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[NULL,,], array(1:12, c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[,0,], array(c(1,2,7,8), c(2,1,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[,1,], array(c(3,4,9,10), c(2,1,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[,2,], array(c(5,6,11,12), c(2,1,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[,c(0,2),], array(c(1,2,5,6,7,8,11,12), c(2,2,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[,,0], array(1:6, c(2,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[NULL,NULL,1], array(7:12, c(2,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[1,2,0], array(6, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[0,1,1], array(9, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[1,1:2,], array(c(4,6,10,12), c(1,2,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[0,c(T,F,T),], array(c(1,5,7,11), c(1,2,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(T,F),c(T,F,T),], array(c(1,5,7,11), c(1,2,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(T,F),c(T,F,T),c(F,T)], array(c(7,11), c(1,2,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(T,F),c(F,F,T),c(F,T)], array(11, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(F,F),c(F,F,F),c(F,T)], integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(F,F),c(T,F,T),c(F,T)], integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(0,0,1,0),,], array(c(1,1,2,1,3,3,4,3,5,5,6,5,7,7,8,7,9,9,10,9,11,11,12,11), c(4,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); identical(x[c(0,0,1,0),c(2,1),0], array(c(5,5,6,5,3,3,4,3), c(4,2,1)));", gStaticEidosValue_LogicalT);
	
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
	EidosAssertScriptSuccess("x = 5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("x = 1:5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 2, 10, 4, 10}));
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1][1:2] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 10, 4, 10}));
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 2, 10, 4, 10}));
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2][0:1] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 2, 10, 4, 5}));
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1] = 11:13; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{11, 2, 12, 4, 13}));
	EidosAssertScriptSuccess("x = 1:5; x[x % 2 == 1][1:2] = 11:12; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 11, 4, 12}));
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2] = 11:13; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{11, 2, 12, 4, 13}));
	EidosAssertScriptSuccess("x = 1:5; x[1:3*2 - 2][0:1] = 11:12; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{11, 2, 12, 4, 5}));
	EidosAssertScriptRaise("x = 1:5; x[1:3*2 - 2][0:1] = 11:13; x;", 27, "assignment to a subscript requires");
	EidosAssertScriptRaise("x = 1:5; x[NULL] = NULL; x;", 17, "assignment to a subscript requires an rvalue that is");
	EidosAssertScriptSuccess("x = 1:5; x[NULL] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 10, 10, 10, 10}));	// assigns 10 to all indices, legal in Eidos 1.6 and later
	EidosAssertScriptRaise("x = 1:5; x[3] = NULL; x;", 14, "assignment to a subscript requires");
	EidosAssertScriptRaise("x = 1:5; x[integer(0)] = NULL; x;", 23, "type mismatch");
	EidosAssertScriptSuccess("x = 1:5; x[integer(0)] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5})); // assigns 10 to no indices, perfectly legal
	EidosAssertScriptRaise("x = 1:5; x[3] = integer(0); x;", 14, "assignment to a subscript requires");
	EidosAssertScriptSuccess("x = 1.0:5; x[3] = 1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 2, 3, 1, 5}));
	EidosAssertScriptSuccess("x = c('a', 'b', 'c'); x[1] = 1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"a", "1", "c"}));
	EidosAssertScriptRaise("x = 1:5; x[3] = 1.5; x;", 14, "type mismatch");
	EidosAssertScriptRaise("x = 1:5; x[3] = 'foo'; x;", 14, "type mismatch");
	EidosAssertScriptSuccess("x = 5; x[0] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x = 5.0; x[0] = 10.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(10)));
	EidosAssertScriptRaise("x = 5; x[0] = 10.0; x;", 12, "type mismatch");
	EidosAssertScriptSuccess("x = 5.0; x[0] = 10; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(10)));
	EidosAssertScriptSuccess("x = T; x[0] = F; x;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("x = 'foo'; x[0] = 'bar'; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("bar")));
	EidosAssertScriptSuccess("x = 1:5; x[c(T,T,F,T,F)] = 7:9; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{7, 8, 3, 9, 5}));
	EidosAssertScriptRaise("x = 1:5; x[c(T,T,F,T,F,T)] = 7:9; x;", 10, "must match the size()");
	EidosAssertScriptSuccess("x = 1:5; x[c(2,3)] = c(9, 5); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 9, 5, 5}));
	EidosAssertScriptRaise("x = 1:5; x[c(7,8)] = 7; x;", 10, "out-of-range index");
	EidosAssertScriptSuccess("x = 1:5; x[c(2.0,3)] = c(9, 5); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 9, 5, 5}));
	EidosAssertScriptRaise("x = 1:5; x[c(7.0,8)] = 7; x;", 10, "out-of-range index");
	EidosAssertScriptRaise("x = 1:5; x[NAN] = 3;", 10, "cannot be converted");
	EidosAssertScriptRaise("x = 1:5; x[c(0.0, 2, NAN)] = c(5, 7, 3);", 10, "cannot be converted");
	
	EidosAssertScriptRaise("x = 5:9; x[matrix(0)] = 3;", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(0:2)] = 3;", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(T)] = 3;", 10, "matrix or array index operand is not supported");
	EidosAssertScriptRaise("x = 5:9; x[matrix(c(T,T,F,T,F))] = 3;", 10, "matrix or array index operand is not supported");
	EidosAssertScriptSuccess("x = 1; x[] = 2; identical(x, 2);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = 1; x[NULL] = 2; identical(x, 2);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = 1:5; x[] = 2; identical(x, rep(2,5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = 1:5; x[NULL] = 2; identical(x, rep(2,5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5); x[] = 3; identical(x, matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5); x[NULL] = 3; identical(x, matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5); x[0] = 3; identical(x, matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5:9); x[] = 3; identical(x, matrix(c(3,3,3,3,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5:9); x[NULL] = 3; identical(x, matrix(c(3,3,3,3,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5:9); x[0] = 3; identical(x, matrix(c(3,6,7,8,9)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5); x[T] = 3; identical(x, matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(5:9); x[c(T,F,T,T,F)] = 3; identical(x, matrix(c(3,6,3,3,9)));", gStaticEidosValue_LogicalT);
	
	// operator = (especially in conjunction with matrix/array-style subsetting with operator [])
	EidosAssertScriptSuccess("NULL[logical(0)] = NULL;", gStaticEidosValueVOID);			// technically legal, as no assignment is done
	EidosAssertScriptRaise("NULL[logical(0),] = NULL;", 4, "too many subset arguments");
	EidosAssertScriptRaise("NULL[logical(0),logical(0)] = NULL;", 4, "too many subset arguments");
	EidosAssertScriptRaise("NULL[,] = NULL;", 4, "too many subset arguments");
	EidosAssertScriptSuccess("x = NULL; x[logical(0)] = NULL;", gStaticEidosValueVOID);	// technically legal, as no assignment is done
	EidosAssertScriptRaise("x = NULL; x[logical(0),] = NULL;", 11, "too many subset arguments");
	EidosAssertScriptRaise("x = NULL; x[logical(0),logical(0)] = NULL;", 11, "too many subset arguments");
	EidosAssertScriptRaise("x = NULL; x[,] = NULL;", 11, "too many subset arguments");
	EidosAssertScriptRaise("x = 1; x[,] = 2; x;", 8, "too many subset arguments");
	EidosAssertScriptRaise("x = 1; x[0,0] = 2; x;", 8, "too many subset arguments");
	EidosAssertScriptRaise("x = 1:5; x[,] = 2; x;", 10, "too many subset arguments");
	EidosAssertScriptRaise("x = 1:5; x[0,0] = 2; x;", 10, "too many subset arguments");
	EidosAssertScriptSuccess("x = matrix(1:5); x[,] = 2; identical(x, matrix(rep(2,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[NULL,NULL] = 2; identical(x, matrix(rep(2,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[0,0] = 2; identical(x, matrix(c(2,2,3,4,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[3,0] = 2; identical(x, matrix(c(1,2,3,2,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[1:3,0] = 7; identical(x, matrix(c(1,7,7,7,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[c(1,3),0] = 7; identical(x, matrix(c(1,7,3,7,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[c(1,3),0] = 6:7; identical(x, matrix(c(1,6,3,7,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[c(T,F,F,T,F),0] = 7; identical(x, matrix(c(7,2,3,7,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:5); x[c(T,F,F,T,F),0] = 6:7; identical(x, matrix(c(6,2,3,7,5)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("x = matrix(1:5); x[-1,0] = 2;", 18, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:5); x[5,0] = 2;", 18, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:5); x[0,-1] = 2;", 18, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:5); x[0,1] = 2;", 18, "out-of-range index");
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[1,1] = 2; identical(x, matrix(c(1,2,3,2,5,6), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[0:1,1] = 7; identical(x, matrix(c(1,2,7,7,5,6), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[1, c(T,F,T)] = 7; identical(x, matrix(c(1,7,3,4,5,7), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[0:1, c(T,F,T)] = 7; identical(x, matrix(c(7,7,3,4,7,7), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[c(T,T), c(T,F,T)] = 6:9; identical(x, matrix(c(6,7,3,4,8,9), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[-1,0] = 2;", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[2,0] = 2;", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0,-1] = 2;", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0,3] = 2;", 26, "out-of-range index");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[c(T,F,T),0] = 2;", 26, "size() of a logical");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[T,0] = 2;", 26, "size() of a logical");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0:4][,0] = 2;", 31, "chaining of matrix/array-style subsets");
	EidosAssertScriptRaise("x = matrix(1:6, nrow=2); x[0,1:2][,0] = 2;", 33, "chaining of matrix/array-style subsets");
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[0,1:2][1] = 2; identical(x, c(1,2,3,4,2,6));", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("x = matrix(1:6, nrow=2); x[0,1:2][1] = 2; identical(x, matrix(c(1,2,3,4,2,6), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=matrix(c(x,y,x,y), nrow=2); z._yolk[,1]=6.5;", 61, "subset of a property");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=matrix(c(x,y,x,y), nrow=2); z[,1]._yolk[1]=6.5;", 68, "subset of a property");
	EidosAssertScriptSuccess("x=_Test(9); z=matrix(x); identical(z._yolk, matrix(9));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=_Test(9); z=array(x, c(1,1,1,1)); identical(z._yolk, array(9, c(1,1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=_Test(9); z=matrix(x); z[0]._yolk = 6; identical(z._yolk, matrix(6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=_Test(9); z=array(x, c(1,1,1,1)); z[0]._yolk = 6; identical(z._yolk, array(6, c(1,1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=_Test(9); z=matrix(x); z[0,0]._yolk = 6; identical(z._yolk, matrix(6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=_Test(9); z=array(x, c(1,1,1,1)); z[0,0,0,0]._yolk = 6; identical(z._yolk, array(6, c(1,1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=matrix(c(x,y,x,y), nrow=2); z[,1]._yolk=6; identical(z._yolk, matrix(c(6,6,6,6), nrow=2));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[,,] = 2; identical(x, array(rep(2,12), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[1,0,1] = -1; identical(x, array(c(1,2,3,4,5,6,7,-1,9,10,11,12), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[1,c(T,F,T),1] = 7; identical(x, array(c(1,2,3,4,5,6,7,7,9,10,11,7), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[1,c(T,F,T),1] = -1:-2; identical(x, array(c(1,2,3,4,5,6,7,-1,9,10,11,-2), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[0:1,c(T,F,T),1] = 15; identical(x, array(c(1,2,3,4,5,6,15,15,9,10,15,15), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[0:1,c(T,F,T),1] = 15:18; identical(x, array(c(1,2,3,4,5,6,15,16,9,10,17,18), c(2,3,2)));", gStaticEidosValue_LogicalT);
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
	EidosAssertScriptSuccess("x = array(1:12, c(2,3,2)); x[0,1:2,][1:2] = 2; identical(x, array(c(1,2,3,4,2,6,7,8,2,10,11,12), c(2,3,2)));", gStaticEidosValue_LogicalT);
	
	// operator = (especially in conjunction with operator .)
	EidosAssertScriptSuccess("x=_Test(9); x._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(9)));
	EidosAssertScriptRaise("x=_Test(NULL);", 2, "cannot be type NULL");
	EidosAssertScriptRaise("x=_Test(9); x._yolk = NULL;", 20, "value cannot be type");
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{9, 7, 9, 7}));
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=2; z._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{9, 2, 9, 2}));
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=2; z._yolk;", 48, "not an lvalue");				// used to be legal, now a policy error
	EidosAssertScriptSuccess("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[c(1,0)]._yolk=c(2, 5); z._yolk;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{5, 2, 5, 2}));
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[c(1,0)]=c(3, 6); z._yolk;", 53, "not an lvalue");	// used to be legal, now a policy error
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[3]._yolk=6.5; z._yolk;", 48, "value cannot be type");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[3]=6.5; z._yolk;", 48, "not an lvalue");			// used to be a type error, now a policy error
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[2:3]._yolk=6.5; z._yolk;", 50, "value cannot be type");
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z._yolk[2:3]=6.5; z._yolk;", 50, "not an lvalue");			// used to be a type error, now a policy error
	EidosAssertScriptRaise("x=_Test(9); y=_Test(7); z=c(x,y,x,y); z[2]=6.5; z._yolk;", 42, "type mismatch");
	EidosAssertScriptRaise("x = 1:5; x.foo[5] = 7;", 10, "operand type integer is not supported");
	
	// operator = (with compound-operator optimizations)
	EidosAssertScriptSuccess("x = 5; x = x + 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(8)));
	EidosAssertScriptSuccess("x = 5:6; x = x + 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{8, 9}));
	EidosAssertScriptSuccess("x = 5:6; x = x + 3:4; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{8, 10}));
	EidosAssertScriptSuccess("x = 5; x = x + 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(8.5)));
	EidosAssertScriptSuccess("x = 5:6; x = x + 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{8.5, 9.5}));
	EidosAssertScriptSuccess("x = 5:6; x = x + 3.5:4.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{8.5, 10.5}));
	EidosAssertScriptRaise("x = 5:7; x = x + 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x + 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess("x = 5.5; x = x + 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(9)));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x + 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{9, 10}));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x + 3.5:4.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{9, 11}));
	EidosAssertScriptSuccess("x = 5.5; x = x + 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(8.5)));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x + 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{8.5, 9.5}));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x + 3:4; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{8.5, 10.5}));
	EidosAssertScriptRaise("x = 5.5:7.5; x = x + 3.5:4.5; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.5:6.5; x = x + 3.5:5.5; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess("x = 5; x = x - 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("x = 5:6; x = x - 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("x = 5:6; x = x - 3:4; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 2}));
	EidosAssertScriptSuccess("x = 5; x = x - 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1.5)));
	EidosAssertScriptSuccess("x = 5:6; x = x - 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.5, 2.5}));
	EidosAssertScriptSuccess("x = 5:6; x = x - 3.5:4.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.5, 1.5}));
	EidosAssertScriptRaise("x = 5:7; x = x - 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x - 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess("x = 5.5; x = x - 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2)));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x - 3.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2, 3}));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x - 3.5:4.5; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2, 2}));
	EidosAssertScriptSuccess("x = 5.5; x = x - 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.5)));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x - 3; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 3.5}));
	EidosAssertScriptSuccess("x = 5.5:6.5; x = x - 3:4; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 2.5}));
	EidosAssertScriptRaise("x = 5.5:7.5; x = x - 3.5:4.5; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.5:6.5; x = x - 3.5:5.5; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess("x = 5; x = x / 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.5)));
	EidosAssertScriptSuccess("x = 5:6; x = x / 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 3.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x / c(2,4); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 1.5}));
	EidosAssertScriptSuccess("x = 5; x = x / 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.5)));
	EidosAssertScriptSuccess("x = 5:6; x = x / 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 3.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x / c(2.0,4.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 1.5}));
	EidosAssertScriptRaise("x = 5:7; x = x / 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x / 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess("x = 5.0; x = x / 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.5)));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x / 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 3.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x / c(2.0,4.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 1.5}));
	EidosAssertScriptSuccess("x = 5.0; x = x / 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(2.5)));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x / 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 3.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x / c(2,4); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{2.5, 1.5}));
	EidosAssertScriptRaise("x = 5.0:7.0; x = x / 3.0:4.0; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.0:6.0; x = x / 3.0:5.0; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess("x = 5; x = x % 2; x;", gStaticEidosValue_Float1);
	EidosAssertScriptSuccess("x = 5:6; x = x % 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 0.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x % c(2,4); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 2.0}));
	EidosAssertScriptSuccess("x = 5; x = x % 2.0; x;", gStaticEidosValue_Float1);
	EidosAssertScriptSuccess("x = 5:6; x = x % 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 0.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x % c(2.0,4.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 2.0}));
	EidosAssertScriptRaise("x = 5:7; x = x % 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x % 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess("x = 5.0; x = x % 2.0; x;", gStaticEidosValue_Float1);
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x % 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 0.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x % c(2.0,4.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 2.0}));
	EidosAssertScriptSuccess("x = 5.0; x = x % 2; x;", gStaticEidosValue_Float1);
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x % 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 0.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x % c(2,4); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1.0, 2.0}));
	EidosAssertScriptRaise("x = 5.0:7.0; x = x % 3.0:4.0; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.0:6.0; x = x % 3.0:5.0; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess("x = 5; x = x * 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x = 5:6; x = x * 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 12}));
	EidosAssertScriptSuccess("x = 5:6; x = x * c(2,4); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{10, 24}));
	EidosAssertScriptSuccess("x = 5; x = x * 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(10.0)));
	EidosAssertScriptSuccess("x = 5:6; x = x * 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.0, 12.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x * c(2.0,4.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.0, 24.0}));
	EidosAssertScriptRaise("x = 5:7; x = x * 3:4; x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x * 3:5; x;", 15, "operator requires that either");
	EidosAssertScriptSuccess("x = 5.0; x = x * 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(10.0)));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x * 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.0, 12.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x * c(2.0,4.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.0, 24.0}));
	EidosAssertScriptSuccess("x = 5.0; x = x * 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(10.0)));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x * 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.0, 12.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x * c(2,4); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{10.0, 24.0}));
	EidosAssertScriptRaise("x = 5.0:7.0; x = x * 3.0:4.0; x;", 19, "operator requires that either");
	EidosAssertScriptRaise("x = 5.0:6.0; x = x * 3.0:5.0; x;", 19, "operator requires that either");
	
	EidosAssertScriptSuccess("x = 5; x = x ^ 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(25.0)));
	EidosAssertScriptSuccess("x = 5:6; x = x ^ 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 36.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x ^ c(2,3); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 216.0}));
	EidosAssertScriptSuccess("x = 5; x = x ^ 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(25.0)));
	EidosAssertScriptSuccess("x = 5:6; x = x ^ 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 36.0}));
	EidosAssertScriptSuccess("x = 5:6; x = x ^ c(2.0,3.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 216.0}));
	EidosAssertScriptRaise("x = 5:7; x = x ^ (3:4); x;", 15, "operator requires that either");
	EidosAssertScriptRaise("x = 5:6; x = x ^ (3:5); x;", 15, "operator requires that either");
	EidosAssertScriptSuccess("x = 5.0; x = x ^ 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(25.0)));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x ^ 2.0; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 36.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x ^ c(2.0,3.0); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 216.0}));
	EidosAssertScriptSuccess("x = 5.0; x = x ^ 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(25.0)));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x ^ 2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 36.0}));
	EidosAssertScriptSuccess("x = 5.0:6.0; x = x ^ c(2,3); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{25.0, 216.0}));
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
	EidosAssertScriptSuccess("T&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&T&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&F&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) & F;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) & T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("F & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("T & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) & c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) & c(F,F,T,T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false}));
	EidosAssertScriptSuccess("c(T,T,F,F) & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, false}));
	EidosAssertScriptSuccess("c(F,F,T,T) & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false}));
	EidosAssertScriptRaise("c(T,F,T,F) & c(F,F);", 11, "not compatible in size()");
	EidosAssertScriptRaise("c(T,T) & c(T,F,T,F);", 7, "not compatible in size()");
	EidosAssertScriptRaise("c(T,F,T,F) & _Test(3);", 11, "is not supported by");
	EidosAssertScriptRaise("_Test(3) & c(T,F,T,F);", 9, "is not supported by");
	EidosAssertScriptSuccess("5&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&5&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&5;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("0&T&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&0&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&0&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) & 0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("c(7,0,5,0) & T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("F & c(5,0,7,0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("9 & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c(7,0,5,0) & c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) & c(0,0,5,7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false}));
	EidosAssertScriptSuccess("5.0&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&5.0&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&5.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("5.0&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("0.0&T&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&0.0;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&0.0&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&0.0&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) & 0.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) & T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("F & c(5.0,0.0,7.0,0.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("9.0 & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) & c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) & c(0.0,0.0,5.0,7.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false}));
	EidosAssertScriptSuccess("INF&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&INF&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("T&NAN&F;", 1, "cannot be converted");
	EidosAssertScriptRaise("NAN&T&T;", 3, "cannot be converted");
	EidosAssertScriptRaise("c(7.0,0.0,5.0,0.0) & c(T,T,NAN,F);", 19, "cannot be converted");
	EidosAssertScriptSuccess("'foo'&T&T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T&'foo'&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T&F&'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("'foo'&F&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("''&T&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&T&'';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&''&T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F&''&F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) & '';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("c('foo','','foo','') & T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("F & c('foo','','foo','');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, false, false}));
	EidosAssertScriptSuccess("'foo' & c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c('foo','','foo','') & c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, false, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) & c('','','foo','foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{false, false, true, false}));
	
	// operator &: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with logical should suffice
	EidosAssertScriptSuccess("identical(T & T, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & F, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & matrix(T), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & F & matrix(T), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & matrix(T) & F, matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & matrix(T) & matrix(T) & T, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & matrix(T) & matrix(F) & T, matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & matrix(T) & matrix(F) & c(T,F,T), c(F,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T & matrix(T) & matrix(T) & c(T,F,T), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(T,F,T) & T & matrix(T) & matrix(F), c(F,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(T,F,T) & T & matrix(T) & matrix(T), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(c(T,F,T) & T & matrix(c(T,T,F)) & matrix(F), c(T,F,F));", 19, "non-conformable");
	EidosAssertScriptSuccess("identical(c(T,F,T) & T & matrix(c(T,T,F)) & matrix(c(T,F,T)), matrix(c(T,F,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & T, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & T & F, matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & matrix(T) & T & T, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & matrix(F) & T & T, matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(T) & matrix(c(T,F)) & T & T, matrix(F));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(c(T,F)) & matrix(F) & T & T, matrix(F));", 25, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(c(T,T)) & matrix(c(T,T)) & T & T, matrix(c(T,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(T,T)) & matrix(c(T,F)) & T & T, matrix(c(T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(T,T,T)) & matrix(c(T,F,F)) & c(T,F,T) & T, matrix(c(T,F,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,T)) & matrix(c(T,T,F)) & c(F,T,T) & T, matrix(c(F,T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & T & matrix(F) & c(T,F,T), c(F,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & T & matrix(T) & c(T,F,T), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & c(T,F,T) & T & matrix(F), c(F,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & c(T,F,T) & T & matrix(T), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) & matrix(T), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) & matrix(F), matrix(F));", gStaticEidosValue_LogicalT);
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
	EidosAssertScriptSuccess("T|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|T|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|F|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|F|F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) | F;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) | T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("F | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("T | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("c(T,F,T,F) | c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) | c(F,F,T,T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, true}));
	EidosAssertScriptSuccess("c(T,T,F,F) | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, false}));
	EidosAssertScriptSuccess("c(F,F,T,T) | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, true}));
	EidosAssertScriptRaise("c(T,F,T,F) | c(F,F);", 11, "not compatible in size()");
	EidosAssertScriptRaise("c(T,T) | c(T,F,T,F);", 7, "not compatible in size()");
	EidosAssertScriptRaise("c(T,F,T,F) | _Test(3);", 11, "is not supported by");
	EidosAssertScriptRaise("_Test(3) | c(T,F,T,F);", 9, "is not supported by");
	EidosAssertScriptSuccess("5|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|5|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|5;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5|F|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("0|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|0|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|0|F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) | 0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c(7,0,5,0) | T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("F | c(5,0,7,0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("9 | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("c(7,0,5,0) | c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) | c(0,0,5,7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, true}));
	EidosAssertScriptSuccess("5.0|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|5.0|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|5.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("5.0|F|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("0.0|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|0.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|0.0|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|0.0|F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) | 0.0;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) | T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("F | c(5.0,0.0,7.0,0.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("9.0 | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("c(7.0,0.0,5.0,0.0) | c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) | c(0.0,0.0,5.0,7.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, true}));
	EidosAssertScriptSuccess("INF|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|INF|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("T|NAN|F;", 1, "cannot be converted");
	EidosAssertScriptRaise("NAN|T|T;", 3, "cannot be converted");
	EidosAssertScriptRaise("c(7.0,0.0,5.0,0.0) | c(T,T,NAN,F);", 19, "cannot be converted");
	EidosAssertScriptSuccess("'foo'|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|'foo'|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("T|F|'foo';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("'foo'|F|F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("''|T|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|T|'';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|''|T;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("F|''|F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("c(T,F,T,F) | '';", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("c('foo','','foo','') | T;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("F | c('foo','','foo','');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("'foo' | c(T,F,T,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, true}));
	EidosAssertScriptSuccess("c('foo','','foo','') | c(T,T,F,F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true, false}));
	EidosAssertScriptSuccess("c(T,F,T,F) | c('','','foo','foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, true}));
	
	// operator |: test with mixed singletons, vectors, matrices, and arrays; the dimensionality code is shared across all operand types, so testing it with logical should suffice
	EidosAssertScriptSuccess("identical(T | F, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | F, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(T | matrix(F), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | F | matrix(T), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | matrix(F) | F, matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | matrix(F) | matrix(T) | F, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | matrix(F) | matrix(F) | T, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | matrix(T) | matrix(F) | c(T,F,T), c(T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(F | matrix(F) | matrix(F) | c(T,F,T), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(T,F,T) | T | matrix(F) | matrix(F), c(T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(c(T,F,T) | F | matrix(T) | matrix(F), c(T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(c(T,F,T) | F | matrix(c(T,T,F)) | matrix(F), c(T,T,F));", 19, "non-conformable");
	EidosAssertScriptSuccess("identical(c(T,F,T) | F | matrix(c(T,F,F)) | matrix(c(T,F,T)), matrix(c(T,F,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) | F, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) | F | F, matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) | matrix(F) | T | F, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) | matrix(F) | F | F, matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(matrix(T) | matrix(c(T,F)) | T | T, matrix(F));", 20, "non-conformable");
	EidosAssertScriptRaise("identical(matrix(c(T,F)) | matrix(F) | T | T, matrix(F));", 25, "non-conformable");
	EidosAssertScriptSuccess("identical(matrix(c(T,F)) | matrix(c(F,F)) | F | F, matrix(c(T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T)) | matrix(c(F,F)) | F | T, matrix(c(T,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,F)) | matrix(c(T,F,F)) | c(F,F,F) | F, matrix(c(T,T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(c(F,T,T)) | matrix(c(F,T,F)) | c(F,F,F) | T, matrix(c(T,T,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) | F | matrix(F) | c(T,F,T), c(T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) | F | matrix(F) | c(T,F,T), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) | c(T,F,T) | T | matrix(F), c(T,T,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) | c(T,F,F) | F | matrix(F), c(T,F,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(T) | matrix(T), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(F) | matrix(F), matrix(F));", gStaticEidosValue_LogicalT);
}

#pragma mark operator !
void _RunOperatorLogicalNotTests(void)
{
	// operator !
	EidosAssertScriptRaise("!NULL;", 0, "is not supported by");
	EidosAssertScriptSuccess("!T;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("!F;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("!7;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("!0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("!7.1;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("!0.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("!INF;", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("!NAN;", 0, "cannot be converted");
	EidosAssertScriptSuccess("!'foo';", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("!'';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("!logical(0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{}));
	EidosAssertScriptSuccess("!integer(0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{}));
	EidosAssertScriptSuccess("!float(0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{}));
	EidosAssertScriptSuccess("!string(0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{}));
	EidosAssertScriptRaise("!object();", 0, "is not supported by");
	EidosAssertScriptSuccess("!c(F,T,F,T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("!c(0,5,0,1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("!c(0,5.0,0,1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptRaise("!c(0,NAN,0,1.0);", 0, "cannot be converted");
	EidosAssertScriptSuccess("!c(0,INF,0,1.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptSuccess("!c('','foo','','bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false}));
	EidosAssertScriptRaise("!_Test(5);", 0, "is not supported by");
	
	// operator |: test with matrices and arrays; the dimensionality code is shared across all operand types, so testing it with logical should suffice
	EidosAssertScriptSuccess("identical(!T, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!F, T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!c(T,F,T), c(F,T,F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!c(F,T,F), c(T,F,T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!matrix(T), matrix(F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!matrix(F), matrix(T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!matrix(c(T,F,T)), matrix(c(F,T,F)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!matrix(c(F,T,F)), matrix(c(T,F,T)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!array(T, c(1,1,1)), array(F, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!array(F, c(1,1,1)), array(T, c(1,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!array(c(T,F,T), c(3,1,1)), array(c(F,T,F), c(3,1,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!array(c(F,T,F), c(1,3,1)), array(c(T,F,T), c(1,3,1)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(!array(c(T,F,T), c(1,1,3)), array(c(F,T,F), c(1,1,3)));", gStaticEidosValue_LogicalT);
}

#pragma mark operator ?
void _RunOperatorTernaryConditionalTests(void)
{
	// operator ?-else
	EidosAssertScriptSuccess("T ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("F ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("9 ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("0 ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("6 > 5 ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("6 < 5 ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptRaise("6 == 6:9 ? 23 else 42;", 9, "condition for ternary conditional has size()");
	EidosAssertScriptSuccess("(6 == (6:9))[0] ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("(6 == (6:9))[1] ? 23 else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptRaise("NAN ? 23 else 42;", 4, "cannot be converted");
	EidosAssertScriptRaise("_Test(6) ? 23 else 42;", 9, "cannot be converted");
	EidosAssertScriptRaise("NULL ? 23 else 42;", 5, "condition for ternary conditional has size()");
	EidosAssertScriptRaise("T ? 23; else 42;", 6, "expected 'else'");
	EidosAssertScriptRaise("T ? 23; x = 10;", 6, "expected 'else'");
	EidosAssertScriptRaise("(T ? x else y) = 10;", 15, "lvalue required");
	EidosAssertScriptSuccess("x = T ? 23 else 42; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("x = F ? 23 else 42; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	
	// test right-associativity; this produces 2 if ? else is left-associative since the left half would then evaluate to 1, which is T
	EidosAssertScriptSuccess("a = 0; a == 0 ? 1 else a == 1 ? 2 else 4;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(1)));
}
	
	// ************************************************************************************
	//
	//	Keyword tests
	//
	
#pragma mark if
void _RunKeywordIfTests(void)
{
	// if
	EidosAssertScriptSuccess("if (T) 23;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (F) 23;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (9) 23;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (0) 23;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (6 > 5) 23;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (6 < 5) 23;", gStaticEidosValueVOID);
	EidosAssertScriptRaise("if (6 == (6:9)) 23;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess("if ((6 == (6:9))[0]) 23;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if ((6 == (6:9))[1]) 23;", gStaticEidosValueVOID);
	EidosAssertScriptRaise("if (NAN) 23;", 0, "cannot be converted");
	EidosAssertScriptRaise("if (_Test(6)) 23;", 0, "cannot be converted");
	EidosAssertScriptRaise("if (NULL) 23;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess("if (matrix(1)) 23;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (matrix(0)) 23;", gStaticEidosValueVOID);
	EidosAssertScriptRaise("if (matrix(1:3)) 23;", 0, "condition for if statement has size()");
	
	// if-else
	EidosAssertScriptSuccess("if (T) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (F) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (9) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (0) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (6 > 5) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (6 < 5) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptRaise("if (6 == (6:9)) 23; else 42;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess("if ((6 == (6:9))[0]) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if ((6 == (6:9))[1]) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptRaise("if (NAN) 23; else 42;", 0, "cannot be converted");
	EidosAssertScriptRaise("if (_Test(6)) 23; else 42;", 0, "cannot be converted");
	EidosAssertScriptRaise("if (NULL) 23; else 42;", 0, "condition for if statement has size()");
	EidosAssertScriptSuccess("if (matrix(1)) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (matrix(0)) 23; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptRaise("if (matrix(1:3)) 23; else 42;", 0, "condition for if statement has size()");
}

#pragma mark do
void _RunKeywordDoTests(void)
{
	// do
	EidosAssertScriptSuccess("x=1; do x=x*2; while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(128)));
	EidosAssertScriptSuccess("x=200; do x=x*2; while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(400)));
	EidosAssertScriptSuccess("x=1; do { x=x*2; x=x+1; } while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(127)));
	EidosAssertScriptSuccess("x=200; do { x=x*2; x=x+1; } while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(401)));
	EidosAssertScriptRaise("x=1; do x=x*2; while (x < 100:102); x;", 5, "condition for do-while loop has size()");
	EidosAssertScriptRaise("x=200; do x=x*2; while (x < 100:102); x;", 7, "condition for do-while loop has size()");
	EidosAssertScriptSuccess("x=1; do x=x*2; while ((x < 100:102)[0]); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(128)));
	EidosAssertScriptSuccess("x=200; do x=x*2; while ((x < 100:102)[0]); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(400)));
	EidosAssertScriptRaise("x=200; do x=x*2; while (NAN); x;", 7, "cannot be converted");
	EidosAssertScriptRaise("x=200; do x=x*2; while (_Test(6)); x;", 7, "cannot be converted");
	EidosAssertScriptRaise("x=200; do x=x*2; while (NULL); x;", 7, "condition for do-while loop has size()");
	EidosAssertScriptSuccess("x=10; do x=x-1; while (x); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0)));
}

#pragma mark while
void _RunKeywordWhileTests(void)
{
	// while
	EidosAssertScriptSuccess("x=1; while (x<100) x=x*2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(128)));
	EidosAssertScriptSuccess("x=200; while (x<100) x=x*2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(200)));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; x=x+1; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(127)));
	EidosAssertScriptSuccess("x=200; while (x<100) { x=x*2; x=x+1; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(200)));
	EidosAssertScriptRaise("x=1; while (x < 100:102) x=x*2; x;", 5, "condition for while loop has size()");
	EidosAssertScriptRaise("x=200; while (x < 100:102) x=x*2; x;", 7, "condition for while loop has size()");
	EidosAssertScriptSuccess("x=1; while ((x < 100:102)[0]) x=x*2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(128)));
	EidosAssertScriptSuccess("x=200; while ((x < 100:102)[0]) x=x*2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(200)));
	EidosAssertScriptRaise("x=200; while (NAN) x=x*2; x;", 7, "cannot be converted");
	EidosAssertScriptRaise("x=200; while (_Test(6)) x=x*2; x;", 7, "cannot be converted");
	EidosAssertScriptRaise("x=200; while (NULL) x=x*2; x;", 7, "condition for while loop has size()");
	EidosAssertScriptSuccess("x=10; while (x) x=x-1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0)));
}

#pragma mark for / in
void _RunKeywordForInTests(void)
{
	// for and in
	EidosAssertScriptSuccess("x=0; for (y in integer(0)) x=x+1; x;", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("x=0; for (y in float(0)) x=x+1; x;", gStaticEidosValue_Integer0);
	EidosAssertScriptSuccess("x=0; for (y in 33) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(33)));
	EidosAssertScriptSuccess("x=0; for (y in 33) x=x+1; x;", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("x=0; for (y in 1:10) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(55)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) x=x+1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { x=x+y; y = 7; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(55)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { x=x+1; y = 7; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x=0; for (y in 10:1) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(55)));
	EidosAssertScriptSuccess("x=0; for (y in 10:1) x=x+1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x=0; for (y in 1.0:10) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(55.0)));
	EidosAssertScriptSuccess("x=0; for (y in 1.0:10) x=x+1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10.0) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(55.0)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10.0) x=x+1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x=0; for (y in c('foo', 'bar')) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("0foobar")));
	EidosAssertScriptSuccess("x=0; for (y in c(T,T,F,F,T,F)) x=x+asInteger(y); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptSuccess("x=0; for (y in _Test(7)) x=x+y._yolk; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptSuccess("x=0; for (y in rep(_Test(7),3)) x=x+y._yolk; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(21)));
	EidosAssertScriptRaise("x=0; y=0:2; for (y[0] in 2:4) x=x+sum(y); x;", 18, "unexpected token");	// lvalue must be an identifier, at present
	EidosAssertScriptRaise("x=0; for (y in NULL) x;", 5, "does not allow NULL");
	EidosAssertScriptSuccess("x=0; q=11:20; for (y in seqAlong(q)) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(45)));
	EidosAssertScriptSuccess("x=0; q=11:20; for (y in seqAlong(q)) x=x+1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptRaise("x=0; q=11:20; for (y in seqAlong(q, 5)) x=x+y; x;", 24, "too many arguments supplied");
	EidosAssertScriptRaise("x=0; q=11:20; for (y in seqAlong()) x=x+y; x;", 24, "missing required");
	EidosAssertScriptSuccess("x=0; for (y in seq(1,10)) x=x+y; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(55)));
	EidosAssertScriptSuccess("x=0; for (y in seq(1,10)) x=x+1; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("x=0; for (y in seqLen(5)) x=x+y+2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(20)));
	EidosAssertScriptSuccess("x=0; for (y in seqLen(1)) x=x+y+2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	EidosAssertScriptSuccess("x=0; for (y in seqLen(0)) x=x+y+2; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0)));
	EidosAssertScriptRaise("x=0; for (y in seqLen(-1)) x=x+y+2; x;", 15, "requires length to be");
	EidosAssertScriptRaise("x=0; for (y in seqLen(5:6)) x=x+y+2; x;", 15, "must be a singleton");
	EidosAssertScriptRaise("x=0; for (y in seqLen('f')) x=x+y+2; x;", 15, "cannot be type");
	
	// additional tests for zero-length ranges; seqAlong() is treated separately in the for() code, so it is tested separately here
	EidosAssertScriptSuccess("i=10; for (i in integer(0)) ; i;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("i=10; for (i in seqAlong(integer(0))) ; i;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("i=10; b=13; for (i in integer(0)) b=i; i;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("i=10; b=13; for (i in seqAlong(integer(0))) b=i; i;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptSuccess("i=10; b=13; for (i in integer(0)) b=i; b;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(13)));
	EidosAssertScriptSuccess("i=10; b=13; for (i in seqAlong(integer(0))) b=i; b;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(13)));
	
	EidosAssertScriptRaise("for (i in matrix(5):9) i;", 19, "must not be matrices or arrays");
	EidosAssertScriptRaise("for (i in 1:matrix(5)) i;", 11, "must not be matrices or arrays");
	EidosAssertScriptRaise("for (i in matrix(3):matrix(5)) i;", 19, "must not be matrices or arrays");
	EidosAssertScriptRaise("for (i in matrix(5:8):9) i;", 21, "must have size() == 1");
	EidosAssertScriptRaise("for (i in 1:matrix(5:8)) i;", 11, "must have size() == 1");
	EidosAssertScriptRaise("for (i in matrix(1:3):matrix(5:7)) i;", 21, "must have size() == 1");
	EidosAssertScriptSuccess("x = 0; for (i in seqAlong(matrix(1))) x=x+i; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0)));
	EidosAssertScriptSuccess("x = 0; for (i in seqAlong(matrix(1:3))) x=x+i; x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
}

#pragma mark next
void _RunKeywordNextTests(void)
{
	// next
	EidosAssertScriptRaise("next;", 0, "encountered with no enclosing loop");
	EidosAssertScriptRaise("if (T) next;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("if (F) next;", gStaticEidosValueVOID);
	EidosAssertScriptRaise("if (T) next; else 42;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("if (F) next; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (T) 23; else next;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptRaise("if (F) 23; else next;", 16, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) next; x=x+1; } while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(124)));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) next; x=x+1; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(124)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) next; x=x+y; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(50)));
}

#pragma mark break
void _RunKeywordBreakTests(void)
{
	// break
	EidosAssertScriptRaise("break;", 0, "encountered with no enclosing loop");
	EidosAssertScriptRaise("if (T) break;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("if (F) break;", gStaticEidosValueVOID);
	EidosAssertScriptRaise("if (T) break; else 42;", 7, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("if (F) break; else 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (T) 23; else break;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptRaise("if (F) 23; else break;", 16, "encountered with no enclosing loop");
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) break; x=x+1; } while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(62)));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) break; x=x+1; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(62)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) break; x=x+y; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
}

#pragma mark return
void _RunKeywordReturnTests(void)
{
	// return
	EidosAssertScriptSuccess("return;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("return NULL;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("return -13;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-13)));
	EidosAssertScriptSuccess("if (T) return;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (T) return NULL;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (T) return -13;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-13)));
	EidosAssertScriptSuccess("if (F) return;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (F) return NULL;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (F) return -13;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (T) return; else return 42;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (T) return NULL; else return 42;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (T) return -13; else return 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-13)));
	EidosAssertScriptSuccess("if (F) return; else return 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (F) return -13; else return 42;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(42)));
	EidosAssertScriptSuccess("if (T) return 23; else return;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (T) return 23; else return -13;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(23)));
	EidosAssertScriptSuccess("if (F) return 23; else return;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (F) return 23; else return NULL;", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("if (F) return 23; else return -13;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-13)));
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) return; x=x+1; } while (x<100); x;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("x=1; do { x=x*2; if (x>50) return x-5; x=x+1; } while (x<100); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(57)));
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) return; x=x+1; } x;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("x=1; while (x<100) { x=x*2; if (x>50) return x-5; x=x+1; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(57)));
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) return; x=x+y; } x;", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("x=0; for (y in 1:10) { if (y==5) return x-5; x=x+y; } x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
}






































