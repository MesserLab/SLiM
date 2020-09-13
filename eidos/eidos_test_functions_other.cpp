//
//  eidos_test_functions_other.cpp
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

#include <string>
#include <vector>


#pragma mark matrix and array
void _RunFunctionMatrixArrayTests(void)
{
	// array()
	EidosAssertScriptRaise("array(5, integer(0));", 0, "at least a matrix");
	EidosAssertScriptRaise("array(5, 1);", 0, "at least a matrix");
	EidosAssertScriptRaise("array(5, c(1,2));", 0, "product of the proposed dimensions");
	EidosAssertScriptSuccess("identical(array(5, c(1,1)), matrix(5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6, c(2,3)), matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(array(1:6, c(3,2)), matrix(1:6, nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("size(array(1:12, c(3,2,2))) == 12;", gStaticEidosValue_LogicalT);		// FIXME not sure how to test higher-dimensional arrays right now...
	
	// cbind()
	EidosAssertScriptRaise("cbind(5, 5.5);", 0, "be the same type");
	EidosAssertScriptRaise("cbind(5, array(5, c(1,1,1)));", 0, "all arguments be vectors or matrices");
	EidosAssertScriptRaise("cbind(matrix(1:4, nrow=2), matrix(1:4, nrow=4));", 0, "number of row");
	EidosAssertScriptSuccess("identical(cbind(5), matrix(5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cbind(1:5), matrix(1:5, ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cbind(1:5, 6:10), matrix(1:10, ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cbind(1:5, 6:10, NULL, integer(0), 11:15), matrix(1:15, ncol=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cbind(matrix(1:6, nrow=2), matrix(7:12, nrow=2)), matrix(1:12, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cbind(matrix(1:6, ncol=2), matrix(7:12, ncol=2)), matrix(1:12, nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(cbind(matrix(1:6, nrow=1), matrix(7:12, nrow=1)), matrix(1:12, nrow=1));", gStaticEidosValue_LogicalT);
	
	// dim()
	EidosAssertScriptSuccess("dim(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(1);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(1.5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim('foo');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(c(T, F));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(c(1, 2));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(c(1.5, 2.0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(c('foo', 'bar'));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("dim(matrix(3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 1}));
	EidosAssertScriptSuccess("dim(matrix(1:6, nrow=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("dim(matrix(1:6, nrow=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("dim(matrix(1:6, ncol=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 2}));
	EidosAssertScriptSuccess("dim(matrix(1:6, ncol=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 2}));
	EidosAssertScriptSuccess("dim(array(1:24, c(2,3,4)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 4}));
	EidosAssertScriptSuccess("dim(array(1:48, c(2,3,4,2)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 4, 2}));
	EidosAssertScriptSuccess("dim(matrix(3.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 1}));
	EidosAssertScriptSuccess("dim(matrix(1.0:6, nrow=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("dim(matrix(1.0:6, nrow=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3}));
	EidosAssertScriptSuccess("dim(matrix(1.0:6, ncol=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 2}));
	EidosAssertScriptSuccess("dim(matrix(1.0:6, ncol=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3, 2}));
	EidosAssertScriptSuccess("dim(array(1.0:24, c(2,3,4)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 4}));
	EidosAssertScriptSuccess("dim(array(1.0:48, c(2,3,4,2)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2, 3, 4, 2}));
	
	// drop()
	EidosAssertScriptSuccess("drop(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("identical(drop(integer(0)), integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(5), 5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(5:9), 5:9);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(matrix(5)), 5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(matrix(5:9)), 5:9);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(matrix(1:6, ncol=1)), 1:6);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(matrix(1:6, nrow=1)), 1:6);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(matrix(1:6, nrow=2)), matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(5, c(1,1,1))), 5);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:6, c(6,1,1))), 1:6);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:6, c(1,6,1))), 1:6);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:6, c(1,1,6))), 1:6);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:6, c(2,3,1))), matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:6, c(1,2,3))), matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:6, c(2,1,3))), matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:12, c(12,1,1))), 1:12);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(drop(array(1:12, c(2,3,2))), array(1:12, c(2,3,2)));", gStaticEidosValue_LogicalT);
	
	// matrix()
	EidosAssertScriptSuccess("matrix(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("matrix(3, nrow=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("matrix(3, ncol=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("matrix(3, nrow=1, ncol=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("matrix(1:6, nrow=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("matrix(1:6, ncol=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("matrix(1:6, ncol=2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("matrix(1:6, ncol=2, byrow=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 5, 2, 4, 6}));
	EidosAssertScriptSuccess("matrix(1:6, ncol=3, byrow=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 4, 2, 5, 3, 6}));
	EidosAssertScriptRaise("matrix(1:5, ncol=2);", 0, "not a multiple of the supplied column count");
	EidosAssertScriptRaise("matrix(1:5, nrow=2);", 0, "not a multiple of the supplied row count");
	EidosAssertScriptRaise("matrix(1:5, nrow=2, ncol=2);", 0, "length equal to the product");
	EidosAssertScriptSuccess("identical(matrix(1:6, ncol=2), matrix(c(1, 4, 2, 5, 3, 6), ncol=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1:6, ncol=3), matrix(c(1, 3, 5, 2, 4, 6), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("matrix(3.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3}));
	EidosAssertScriptSuccess("matrix(3.0, nrow=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3}));
	EidosAssertScriptSuccess("matrix(3.0, ncol=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3}));
	EidosAssertScriptSuccess("matrix(3.0, nrow=1, ncol=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{3}));
	EidosAssertScriptSuccess("matrix(1.0:6, nrow=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("matrix(1.0:6, ncol=1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("matrix(1.0:6, ncol=2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 2, 3, 4, 5, 6}));
	EidosAssertScriptSuccess("matrix(1.0:6, ncol=2, byrow=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 3, 5, 2, 4, 6}));
	EidosAssertScriptSuccess("matrix(1.0:6, ncol=3, byrow=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 4, 2, 5, 3, 6}));
	EidosAssertScriptRaise("matrix(1.0:5, ncol=2);", 0, "not a multiple of the supplied column count");
	EidosAssertScriptRaise("matrix(1.0:5, nrow=2);", 0, "not a multiple of the supplied row count");
	EidosAssertScriptRaise("matrix(1.0:5, nrow=2, ncol=2);", 0, "length equal to the product");
	EidosAssertScriptSuccess("identical(matrix(1.0:6, ncol=2), matrix(c(1.0, 4, 2, 5, 3, 6), ncol=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(matrix(1.0:6, ncol=3), matrix(c(1.0, 3, 5, 2, 4, 6), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	
	// matrixMult()
	EidosAssertScriptRaise("matrixMult(matrix(5), 5);", 0, "is not a matrix");
	EidosAssertScriptRaise("matrixMult(5, matrix(5));", 0, "is not a matrix");
	EidosAssertScriptRaise("matrixMult(matrix(5), matrix(5.5));", 0, "are the same type");
	EidosAssertScriptRaise("matrixMult(matrix(1:5), matrix(1:5));", 0, "not conformable");
	EidosAssertScriptSuccess("A = matrix(2); B = matrix(5); identical(matrixMult(A, B), matrix(10));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(2); B = matrix(1:5, nrow=1); identical(matrixMult(A, B), matrix(c(2,4,6,8,10), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1:5, ncol=1); B = matrix(2); identical(matrixMult(A, B), matrix(c(2,4,6,8,10), ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1:5, ncol=1); B = matrix(1:5, nrow=1); identical(matrixMult(A, B), matrix(c(1:5, (1:5)*2, (1:5)*3, (1:5)*4, (1:5)*5), ncol=5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1:5, nrow=1); B = matrix(1:5, ncol=1); identical(matrixMult(A, B), matrix(55));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1:6, nrow=2); B = matrix(1:6, ncol=2); identical(matrixMult(A, B), matrix(c(22, 28, 49, 64), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1:6, ncol=2); B = matrix(1:6, nrow=2); identical(matrixMult(A, B), matrix(c(9, 12, 15, 19, 26, 33, 29, 40, 51), nrow=3));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptRaise("matrixMult(matrix(5.0), 5.0);", 0, "is not a matrix");
	EidosAssertScriptRaise("matrixMult(5.0, matrix(5.0));", 0, "is not a matrix");
	EidosAssertScriptRaise("matrixMult(matrix(5.0), matrix(5));", 0, "are the same type");
	EidosAssertScriptRaise("matrixMult(matrix(1.0:5.0), matrix(1.0:5.0));", 0, "not conformable");
	EidosAssertScriptSuccess("A = matrix(2.0); B = matrix(5.0); identical(matrixMult(A, B), matrix(10.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(2.0); B = matrix(1.0:5.0, nrow=1); identical(matrixMult(A, B), matrix(c(2.0,4.0,6.0,8.0,10.0), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1.0:5.0, ncol=1); B = matrix(2.0); identical(matrixMult(A, B), matrix(c(2.0,4.0,6.0,8.0,10.0), ncol=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1.0:5.0, ncol=1); B = matrix(1.0:5.0, nrow=1); identical(matrixMult(A, B), matrix(c(1.0:5.0, (1.0:5.0)*2, (1.0:5.0)*3, (1.0:5.0)*4, (1.0:5.0)*5), ncol=5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1.0:5.0, nrow=1); B = matrix(1.0:5.0, ncol=1); identical(matrixMult(A, B), matrix(55.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1.0:6.0, nrow=2); B = matrix(1.0:6.0, ncol=2); identical(matrixMult(A, B), matrix(c(22.0, 28.0, 49.0, 64.0), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("A = matrix(1.0:6.0, ncol=2); B = matrix(1.0:6.0, nrow=2); identical(matrixMult(A, B), matrix(c(9.0, 12.0, 15.0, 19.0, 26.0, 33.0, 29.0, 40.0, 51.0), nrow=3));", gStaticEidosValue_LogicalT);
	
	// ncol()
	EidosAssertScriptSuccess("ncol(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(1);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(1.5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol('foo');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(c(T, F));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(c(1, 2));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(c(1.5, 2.0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(c('foo', 'bar'));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("ncol(matrix(3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1}));
	EidosAssertScriptSuccess("ncol(matrix(1:6, nrow=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(matrix(1:6, nrow=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(matrix(1:6, ncol=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("ncol(matrix(1:6, ncol=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("ncol(array(1:24, c(2,3,4)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(array(1:48, c(2,3,4,2)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(matrix(3.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1}));
	EidosAssertScriptSuccess("ncol(matrix(1.0:6, nrow=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(matrix(1.0:6, nrow=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(matrix(1.0:6, ncol=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("ncol(matrix(1.0:6, ncol=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("ncol(array(1.0:24, c(2,3,4)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("ncol(array(1.0:48, c(2,3,4,2)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	
	// nrow()
	EidosAssertScriptSuccess("nrow(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(T);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(1);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(1.5);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow('foo');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(c(T, F));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(c(1, 2));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(c(1.5, 2.0));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(c('foo', 'bar'));", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("nrow(matrix(3));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1}));
	EidosAssertScriptSuccess("nrow(matrix(1:6, nrow=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(matrix(1:6, nrow=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(matrix(1:6, ncol=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("nrow(matrix(1:6, ncol=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("nrow(array(1:24, c(2,3,4)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(array(1:48, c(2,3,4,2)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(matrix(3.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1}));
	EidosAssertScriptSuccess("nrow(matrix(1.0:6, nrow=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(matrix(1.0:6, nrow=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(matrix(1.0:6, ncol=2));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("nrow(matrix(1.0:6, ncol=2, byrow=T));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{3}));
	EidosAssertScriptSuccess("nrow(array(1.0:24, c(2,3,4)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	EidosAssertScriptSuccess("nrow(array(1.0:48, c(2,3,4,2)));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2}));
	
	// rbind()
	EidosAssertScriptRaise("rbind(5, 5.5);", 0, "be the same type");
	EidosAssertScriptRaise("rbind(5, array(5, c(1,1,1)));", 0, "all arguments be vectors or matrices");
	EidosAssertScriptRaise("rbind(matrix(1:4, nrow=2), matrix(1:4, nrow=4));", 0, "number of columns");
	EidosAssertScriptSuccess("identical(rbind(5), matrix(5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rbind(1:5), matrix(1:5, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rbind(1:5, 6:10), matrix(1:10, nrow=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rbind(1:5, 6:10, NULL, integer(0), 11:15), matrix(1:15, nrow=3, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rbind(matrix(1:6, nrow=2), matrix(7:12, nrow=2)), matrix(c(1,2,7,8,3,4,9,10,5,6,11,12), nrow=4));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rbind(matrix(1:6, ncol=2), matrix(7:12, ncol=2)), matrix(c(1,2,3,7,8,9,4,5,6,10,11,12), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rbind(matrix(1:6, ncol=1), matrix(7:12, ncol=1)), matrix(1:12, ncol=1));", gStaticEidosValue_LogicalT);
	
	// t()
	EidosAssertScriptRaise("t(NULL);", 0, "is not a matrix");
	EidosAssertScriptRaise("t(T);", 0, "is not a matrix");
	EidosAssertScriptRaise("t(1);", 0, "is not a matrix");
	EidosAssertScriptRaise("t(1.5);", 0, "is not a matrix");
	EidosAssertScriptRaise("t('foo');", 0, "is not a matrix");
	EidosAssertScriptSuccess("identical(t(matrix(3)), matrix(3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1:6, nrow=2)), matrix(1:6, ncol=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1:6, nrow=2, byrow=T)), matrix(1:6, ncol=2, byrow=F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1:6, ncol=2)), matrix(1:6, nrow=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1:6, ncol=2, byrow=T)), matrix(1:6, nrow=2, byrow=F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(3.0)), matrix(3.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1.0:6, nrow=2)), matrix(1.0:6, ncol=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1.0:6, nrow=2, byrow=T)), matrix(1.0:6, ncol=2, byrow=F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1.0:6, ncol=2)), matrix(1.0:6, nrow=2, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(t(matrix(1.0:6, ncol=2, byrow=T)), matrix(1.0:6, nrow=2, byrow=F));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("t(array(1:24, c(2,3,4)));", 0, "is not a matrix");
	EidosAssertScriptRaise("t(array(1:48, c(2,3,4,2)));", 0, "is not a matrix");
}

#pragma mark filesystem access
void _RunFunctionFilesystemTests(std::string temp_path)
{
	if (!Eidos_SlashTmpExists())
		return;
	
	// filesAtPath() – hard to know how to test this!  These tests should be true on Un*x machines, anyway – but might be disallowed by file permissions.
	EidosAssertScriptSuccess("type(filesAtPath('/tmp')) == 'string';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("type(filesAtPath('/tmp/')) == 'string';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(filesAtPath('/') == 'bin');", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("sum(filesAtPath('/', T) == '/bin');", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("filesAtPath('foo_is_a_bad_path');", gStaticEidosValueNULL);
	
	// writeFile()
	EidosAssertScriptSuccess("writeFile('" + temp_path + "/EidosTest.txt', c(paste(0:4), paste(5:9)));", gStaticEidosValue_LogicalT);
	
	// readFile() – note that the readFile() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess("readFile('" + temp_path + "/EidosTest.txt') == c(paste(0:4), paste(5:9));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true}));
	EidosAssertScriptSuccess("all(asInteger(strsplit(paste(readFile('" + temp_path + "/EidosTest.txt')))) == 0:9);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("readFile('foo_is_a_bad_path.txt');", gStaticEidosValueNULL);
	
	// writeFile() with append
	EidosAssertScriptSuccess("writeFile('" + temp_path + "/EidosTest.txt', 'foo', T);", gStaticEidosValue_LogicalT);
	
	// readFile() – note that the readFile() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess("readFile('" + temp_path + "/EidosTest.txt') == c(paste(0:4), paste(5:9), 'foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, true, true}));
	
	// fileExists() – note that the fileExists() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess("fileExists('" + temp_path + "/EidosTest.txt');", gStaticEidosValue_LogicalT);
	
	// deleteFile() – note that the deleteFile() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess("deleteFile('" + temp_path + "/EidosTest.txt');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("deleteFile('" + temp_path + "/EidosTest.txt');", gStaticEidosValue_LogicalF);
	
	// fileExists() – note that the fileExists() tests depend on the previous writeFile() and deleteFile() tests
	EidosAssertScriptSuccess("fileExists('" + temp_path + "/EidosTest.txt');", gStaticEidosValue_LogicalF);
	
	// writeTempFile()
	EidosAssertScriptRaise("file = writeTempFile('eidos_test_~', '.txt', '');", 7, "may not contain");
	EidosAssertScriptRaise("file = writeTempFile('eidos_test_/', '.txt', '');", 7, "may not contain");
	EidosAssertScriptRaise("file = writeTempFile('eidos_test_', 'foo~.txt', '');", 7, "may not contain");
	EidosAssertScriptRaise("file = writeTempFile('eidos_test_', 'foo/.txt', '');", 7, "may not contain");
	EidosAssertScriptSuccess("file = writeTempFile('eidos_test_', '.txt', ''); identical(readFile(file), string(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("file = writeTempFile('eidos_test_', '.txt', 'foo'); identical(readFile(file), 'foo');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("file = writeTempFile('eidos_test_', '.txt', c(paste(0:4), paste(5:9))); identical(readFile(file), c('0 1 2 3 4', '5 6 7 8 9'));", gStaticEidosValue_LogicalT);
	
	// writeFile() and writeTempFile() with compression – we don't decompress to verify, but we check for success and file existence
	EidosAssertScriptSuccess("writeFile('" + temp_path + "/EidosTest.txt', c(paste(0:4), paste(5:9)), compress=T);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("fileExists('" + temp_path + "/EidosTest.txt.gz');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("file = writeTempFile('eidos_test_', '.txt', 'foo'); fileExists(file);", gStaticEidosValue_LogicalT);
	
	// createDirectory() – we rely on writeTempFile() to give us a file path that isn't in use, from which we derive a directory path that also shouldn't be in use
	EidosAssertScriptSuccess("file = writeTempFile('eidos_test_dir', '.txt', ''); dir = substr(file, 0, nchar(file) - 5); createDirectory(dir);", gStaticEidosValue_LogicalT);
	
	// getwd() / setwd()
	EidosAssertScriptSuccess("path1 = getwd(); path2 = setwd(path1); path1 == path2;", gStaticEidosValue_LogicalT);
}

#pragma mark color manipulation
void _RunColorManipulationTests(void)
{
	// cmColors()
	EidosAssertScriptRaise("cmColors(-1);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptRaise("cmColors(10000000);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptSuccess("cmColors(0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("cmColors(1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF"}));
	EidosAssertScriptSuccess("cmColors(2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF", "#FF80FF"}));
	EidosAssertScriptSuccess("cmColors(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF", "#FFFFFF", "#FF80FF"}));
	EidosAssertScriptSuccess("cmColors(4);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF", "#D4FFFF", "#FFD5FF", "#FF80FF"}));
	EidosAssertScriptSuccess("cmColors(7);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF", "#AAFFFF", "#D4FFFF", "#FFFFFF", "#FFD5FF", "#FFAAFF", "#FF80FF"}));
	
	// colors() (we test only palettes 'cm', 'heat', and 'terrain' here)
	EidosAssertScriptRaise("colors(-1, 'cm');", 0, "requires 0 <= x <= 100000");
	EidosAssertScriptRaise("colors(10000000, 'cm');", 0, "requires 0 <= x <= 100000");
	EidosAssertScriptRaise("colors(5, 'foo');", 0, "unrecognized color palette name");
	EidosAssertScriptSuccess("colors(0, 'cm');", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("colors(1, 'cm');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF"}));
	EidosAssertScriptSuccess("colors(2, 'cm');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF", "#FF80FF"}));
	EidosAssertScriptSuccess("colors(3, 'cm');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF", "#FFFFFF", "#FF80FF"}));
	EidosAssertScriptSuccess("colors(4, 'cm');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF", "#D4FFFF", "#FFD5FF", "#FF80FF"}));
	EidosAssertScriptSuccess("colors(7, 'cm');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF", "#AAFFFF", "#D4FFFF", "#FFFFFF", "#FFD5FF", "#FFAAFF", "#FF80FF"}));
	EidosAssertScriptSuccess("colors(0.0, 'cm');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF"}));
	EidosAssertScriptSuccess("colors(-100.0, 'cm');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF"}));
	EidosAssertScriptSuccess("colors(1.0, 'cm');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF80FF"}));
	EidosAssertScriptSuccess("colors(100.0, 'cm');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF80FF"}));
	EidosAssertScriptSuccess("colors(c(0.0,0.5,1.0), 'cm');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#80FFFF", "#FFFFFF", "#FF80FF"}));
	EidosAssertScriptSuccess("colors(c(0.5,1.0,0.0), 'cm');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FFFFFF", "#FF80FF", "#80FFFF"}));
	EidosAssertScriptSuccess("colors(1, 'heat');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000"}));
	EidosAssertScriptSuccess("colors(5, 'heat');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#FF5500", "#FFAA00", "#FFFF00", "#FFFFFF"}));
	EidosAssertScriptSuccess("colors(1, 'terrain');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#00A600"}));
	EidosAssertScriptSuccess("colors(5, 'terrain');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#00A600", "#63C600", "#E6E600", "#ECB176", "#F2F2F2"}));
	
	// heatColors()
	EidosAssertScriptRaise("heatColors(-1);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptRaise("heatColors(10000000);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptSuccess("heatColors(0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("heatColors(1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000"}));
	EidosAssertScriptSuccess("heatColors(2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#FFFFFF"}));
	EidosAssertScriptSuccess("heatColors(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#FF5500", "#FFAA00", "#FFFF00", "#FFFFFF"}));
	
	// terrainColors()
	EidosAssertScriptRaise("terrainColors(-1);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptRaise("terrainColors(10000000);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptSuccess("terrainColors(0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("terrainColors(1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#00A600"}));
	EidosAssertScriptSuccess("terrainColors(2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#00A600", "#F2F2F2"}));
	EidosAssertScriptSuccess("terrainColors(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#00A600", "#63C600", "#E6E600", "#ECB176", "#F2F2F2"}));
	
	// rainbow()
	EidosAssertScriptRaise("rainbow(-1);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptRaise("rainbow(10000000);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptSuccess("rainbow(0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess("rainbow(1);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000"}));
	EidosAssertScriptSuccess("rainbow(2);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#00FFFF"}));
	EidosAssertScriptSuccess("rainbow(3);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#00FF00", "#0000FF"}));
	EidosAssertScriptSuccess("rainbow(4);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#80FF00", "#00FFFF", "#8000FF"}));
	EidosAssertScriptSuccess("rainbow(12);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF0000", "#FF8000", "#FFFF00", "#80FF00", "#00FF00", "#00FF80", "#00FFFF", "#0080FF", "#0000FF", "#8000FF", "#FF00FF", "#FF0080"}));
	EidosAssertScriptSuccess("rainbow(6, s=0.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FF8080", "#FFFF80", "#80FF80", "#80FFFF", "#8080FF", "#FF80FF"}));
	EidosAssertScriptSuccess("rainbow(6, v=0.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#800000", "#808000", "#008000", "#008080", "#000080", "#800080"}));
	EidosAssertScriptSuccess("rainbow(6, s=0.5, v=0.5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#804040", "#808040", "#408040", "#408080", "#404080", "#804080"}));
	EidosAssertScriptSuccess("rainbow(4, start=1.0/6, end=4.0/6, ccw=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FFFF00", "#00FF00", "#00FFFF", "#0000FF"}));
	EidosAssertScriptSuccess("rainbow(4, start=1.0/6, end=4.0/6, ccw=F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#FFFF00", "#FF0000", "#FF00FF", "#0000FF"}));
	EidosAssertScriptSuccess("rainbow(4, start=4.0/6, end=1.0/6, ccw=T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#0000FF", "#FF00FF", "#FF0000", "#FFFF00"}));
	EidosAssertScriptSuccess("rainbow(4, start=4.0/6, end=1.0/6, ccw=F);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"#0000FF", "#00FFFF", "#00FF00", "#FFFF00"}));
	EidosAssertScriptRaise("rainbow(4, start=NAN, end=1.0/6, ccw=F);", 0, "color component with value NAN");
	EidosAssertScriptRaise("rainbow(4, start=4.0/6, end=NAN, ccw=F);", 0, "color component with value NAN");
	
	// hsv2rgb()
	EidosAssertScriptRaise("hsv2rgb(c(0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("hsv2rgb(c(0.0, 0.0, 0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("hsv2rgb(c(NAN, 0.0, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("hsv2rgb(c(0.0, NAN, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("hsv2rgb(c(0.0, 0.0, NAN));", 0, "color component with value NAN");
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.0, -0.5)), c(0.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.0, 0.0)), c(0.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.0, 0.5)), c(0.5, 0.5, 0.5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.0, 1.0)), c(1.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.0, 1.5)), c(1.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, -0.5, 1.0)), c(1.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.25, 1.0)), c(1.0, 0.75, 0.75));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.5, 1.0)), c(1.0, 0.5, 0.5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 0.75, 1.0)), c(1.0, 0.25, 0.25));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 1.0, 1.0)), c(1.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(0.0, 1.5, 1.0)), c(1.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(-0.5, 1.0, 1.0)), c(1.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(1/6, 1.0, 1.0)), c(1.0, 1.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(2/6, 1.0, 1.0)), c(0.0, 1.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(3/6, 1.0, 1.0)), c(0.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(4/6, 1.0, 1.0)), c(0.0, 0.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(5/6, 1.0, 1.0)), c(1.0, 0.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(6/6, 1.0, 1.0)), c(1.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(c(7/6, 1.0, 1.0)), c(1.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(hsv2rgb(matrix(c(1/6, 1.0, 1.0, 0.0, 0.25, 1.0), ncol=3, byrow=T)), matrix(c(1.0, 1.0, 0.0, 1.0, 0.75, 0.75), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	
	// rgb2hsv()
	EidosAssertScriptRaise("rgb2hsv(c(0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2hsv(c(0.0, 0.0, 0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2hsv(c(NAN, 0.0, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("rgb2hsv(c(0.0, NAN, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("rgb2hsv(c(0.0, 0.0, NAN));", 0, "color component with value NAN");
	EidosAssertScriptSuccess("identical(rgb2hsv(c(-1.0, 0.0, 0.0)), c(0.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, -1.0, 0.0)), c(0.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, 0.0, -1.0)), c(0.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, 0.0, 0.0)), c(0.0, 0.0, 0.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.5, 0.5, 0.5)), c(0.0, 0.0, 0.5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 1.0, 1.0)), c(0.0, 0.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.5, 1.0, 1.0)), c(0.0, 0.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 1.5, 1.0)), c(0.0, 0.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 1.0, 1.5)), c(0.0, 0.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 0.75, 0.75)), c(0.0, 0.25, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 0.5, 0.5)), c(0.0, 0.5, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 0.25, 0.25)), c(0.0, 0.75, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 0.0, 0.0)), c(0.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.0, 1.0, 0.0)), c(1/6, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, 1.0, 0.0)), c(2/6, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, 1.0, 1.0)), c(3/6, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, 0.0, 1.0)), c(4/6, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(rgb2hsv(c(1.0, 0.0, 1.0)) - c(5/6, 1.0, 1.0))) < 1e-7;", gStaticEidosValue_LogicalT);	// roundoff with 5/6
	EidosAssertScriptSuccess("identical(rgb2hsv(c(1.5, -0.5, 0.0)), c(0.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(0.0, 1.5, -0.5)), c(2/6, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(c(-0.5, 0.0, 1.5)), c(4/6, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2hsv(matrix(c(1.0, 1.0, 0.0, 1.0, 0.75, 0.75), ncol=3, byrow=T)), matrix(c(1/6, 1.0, 1.0, 0.0, 0.25, 1.0), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	
	// rgb2color()
	EidosAssertScriptRaise("rgb2color(c(0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2color(c(0.0, 0.0, 0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2color(c(NAN, 0.0, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("rgb2color(c(0.0, NAN, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("rgb2color(c(0.0, 0.0, NAN));", 0, "color component with value NAN");
	EidosAssertScriptSuccess("rgb2color(c(-0.5, -0.5, -0.5)) == '#000000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.0, 0.0)) == '#000000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(1.0, 1.0, 1.0)) == '#FFFFFF';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(1.5, 1.5, 1.5)) == '#FFFFFF';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(1.0, 0.0, 0.0)) == '#FF0000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 1.0, 0.0)) == '#00FF00';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.0, 1.0)) == '#0000FF';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.25, 0.0, 0.0)) == '#400000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.25, 0.0)) == '#004000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.0, 0.25)) == '#000040';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.5, 0.0, 0.0)) == '#800000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.5, 0.0)) == '#008000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.0, 0.5)) == '#000080';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.75, 0.0, 0.0)) == '#BF0000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.75, 0.0)) == '#00BF00';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.0, 0.75)) == '#0000BF';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(1.0, 0.0, 0.0)) == '#FF0000';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 1.0, 0.0)) == '#00FF00';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("rgb2color(c(0.0, 0.0, 1.0)) == '#0000FF';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(rgb2color(matrix(c(0.25, 0.0, 0.0, 0.0, 0.75, 0.0, 0.0, 0.0, 1.0), ncol=3, byrow=T)), c('#400000', '#00BF00', '#0000FF'));", gStaticEidosValue_LogicalT);
	
	// color2rgb()
	EidosAssertScriptRaise("identical(color2rgb('foo'), c(0.0, 0.0, 0.0));", 10, "could not be found");
	EidosAssertScriptRaise("identical(color2rgb('#00000'), c(0.0, 0.0, 0.0));", 10, "could not be found");
	EidosAssertScriptRaise("identical(color2rgb('#0000000'), c(0.0, 0.0, 0.0));", 10, "could not be found");
	EidosAssertScriptRaise("identical(color2rgb('#0000g0'), c(0.0, 0.0, 0.0));", 10, "is malformed");
	EidosAssertScriptSuccess("identical(color2rgb('white'), c(1.0, 1.0, 1.0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(color2rgb(c('#000000', 'red', 'green', 'blue', '#FFFFFF')), matrix(c(0.0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1), ncol=3, byrow=T));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('chocolate1') - c(1.0, 127/255, 36/255))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#000000') - c(0.0, 0.0, 0.0))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#7F0000') - c(127/255, 0.0, 0.0))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#FF0000') - c(1.0, 0.0, 0.0))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#007F00') - c(0.0, 127/255, 0.0))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#00FF00') - c(0.0, 1.0, 0.0))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#00007F') - c(0.0, 0.0, 127/255))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#0000FF') - c(0.0, 0.0, 1.0))) < 1e-7;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("sum(abs(color2rgb('#0000ff') - c(0.0, 0.0, 1.0))) < 1e-7;", gStaticEidosValue_LogicalT);
}

#pragma mark miscellaneous
void _RunFunctionMiscTests_apply_sapply(void)
{
	// apply()
	EidosAssertScriptRaise("x=integer(0); apply(x, 0, 'applyValue^2;');", 14, "matrix or array");
	EidosAssertScriptRaise("x=5; apply(x, 0, 'applyValue^2;');", 5, "matrix or array");
	EidosAssertScriptRaise("x=5:9; apply(x, 0, 'applyValue^2;');", 7, "matrix or array");
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, -1, 'applyValue^2;');", 23, "out of range");
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, 2, 'applyValue^2;');", 23, "out of range");
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, c(0,0), 'applyValue^2;');", 23, "already specified");
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, integer(0), 'applyValue^2;');", 23, "requires that margins be specified");
	
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, 0, 'setSeed(5);');", 23, "must return a non-void value");
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, 0, 'semanticError;');", 23, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=matrix(1:6, nrow=2); apply(x, 0, 'syntax Error;');", 23, "unexpected token '@Error'");
	
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'sum(applyValue);'), c(9,12));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'sum(applyValue);'), c(3,7,11));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'sum(applyValue);'), matrix(1:6, nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'sum(applyValue);'), t(matrix(1:6, nrow=2)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'applyValue^2;'), matrix(c(1.0,9,25,4,16,36), nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'applyValue^2;'), matrix(c(1.0,4,9,16,25,36), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'applyValue^2;'), matrix(c(1.0,4,9,16,25,36), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'applyValue^2;'), t(matrix(c(1.0,4,9,16,25,36), nrow=2)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'c(applyValue, applyValue^2);'), matrix(c(1.0,3,5,1,9,25,2,4,6,4,16,36), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'c(applyValue, applyValue^2);'), matrix(c(1.0,2,1,4,3,4,9,16,5,6,25,36), ncol=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'c(applyValue, applyValue^2);'), array(c(1.0,1,2,4,3,9,4,16,5,25,6,36), c(2,2,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'c(applyValue, applyValue^2);'), array(c(1.0,1,3,9,5,25,2,4,4,16,6,36), c(2,3,2)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'if (applyValue[0] % 2) sum(applyValue); else NULL;'), 9);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'if (applyValue[0] % 3) sum(applyValue); else NULL;'), c(3,11));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'if (applyValue[0] % 2) sum(applyValue); else NULL;'), c(1,3,5));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'if (applyValue[0] % 2) sum(applyValue); else NULL;'), c(1,3,5));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'if (applyValue[0] % 2) applyValue^2; else NULL;'), c(1.0,9,25));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'if (applyValue[0] % 3) applyValue^2; else NULL;'), c(1.0,4,25,36));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'if (applyValue[0] % 2) applyValue^2; else NULL;'), c(1.0,9,25));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'if (applyValue[0] % 2) applyValue^2; else NULL;'), c(1.0,9,25));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'if (applyValue[0] % 2) c(applyValue, applyValue^2); else NULL;'), c(1.0,3,5,1,9,25));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'if (applyValue[0] % 3) c(applyValue, applyValue^2); else NULL;'), c(1.0,2,1,4,5,6,25,36));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'if (applyValue[0] % 2) c(applyValue, applyValue^2); else NULL;'), c(1.0,1,3,9,5,25));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'if (applyValue[0] % 2) c(applyValue, applyValue^2); else NULL;'), c(1.0,1,3,9,5,25));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, 0, 'sum(applyValue);'), c(36,42));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, 1, 'sum(applyValue);'), c(18,26,34));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, 2, 'sum(applyValue);'), c(21,57));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,1), 'sum(applyValue);'), matrix(c(8,10,12,14,16,18), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(1,2), 'sum(applyValue);'), matrix(c(3,7,11,15,19,23), nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,2), 'sum(applyValue);'), matrix(c(9,12,27,30), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,1,2), 'sum(applyValue);'), array(1:12, c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(2,1,0), 'sum(applyValue);'), array(c(1,7,3,9,5,11,2,8,4,10,6,12), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(2,0,1), 'sum(applyValue);'), array(c(1,7,2,8,3,9,4,10,5,11,6,12), c(2,2,3)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, 0, 'applyValue^2;'), matrix(c(1.0,9,25,49,81,121,4,16,36,64,100,144), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, 1, 'applyValue^2;'), matrix(c(1.0,4,49,64,9,16,81,100,25,36,121,144), ncol=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, 2, 'applyValue^2;'), matrix(c(1.0,4,9,16,25,36,49,64,81,100,121,144), ncol=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,1), 'applyValue^2;'), array(c(1.0,49,4,64,9,81,16,100,25,121,36,144), c(2,2,3)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(1,2), 'applyValue^2;'), array(c(1.0,4,9,16,25,36,49,64,81,100,121,144), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,2), 'applyValue^2;'), array(c(1.0,9,25,4,16,36,49,81,121,64,100,144), c(3,2,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,1,2), 'applyValue^2;'), array((1.0:12)^2, c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(2,1,0), 'applyValue^2;'), array(c(1.0,49,9,81,25,121,4,64,16,100,36,144), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("y = array(1:12, c(2,3,2)); identical(apply(y, c(2,0,1), 'applyValue^2;'), array(c(1.0,49,4,64,9,81,16,100,25,121,36,144), c(2,2,3)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, 0, 'sum(applyValue);'), c(144,156));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, 1, 'sum(applyValue);'), c(84,100,116));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, 2, 'sum(applyValue);'), c(114,186));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, 3, 'sum(applyValue);'), c(78,222));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(0,1), 'sum(applyValue);'), matrix(c(40,44,48,52,56,60), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(0,2), 'sum(applyValue);'), matrix(c(54,60,90,96), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(0,3), 'sum(applyValue);'), matrix(c(36,42,108,114), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(1,0), 'sum(applyValue);'), matrix(c(40,48,56,44,52,60), nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(1,2), 'sum(applyValue);'), matrix(c(30,38,46,54,62,70), nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(1,3), 'sum(applyValue);'), matrix(c(18,26,34,66,74,82), nrow=3));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(2,0), 'sum(applyValue);'), matrix(c(54,90,60,96), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(2,1), 'sum(applyValue);'), matrix(c(30,54,38,62,46,70), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(2,3), 'sum(applyValue);'), matrix(c(21,57,93,129), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(3,0), 'sum(applyValue);'), matrix(c(36,108,42,114), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(3,1), 'sum(applyValue);'), matrix(c(18,66,26,74,34,82), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(3,2), 'sum(applyValue);'), matrix(c(21,93,57,129), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(0,1,2), 'sum(applyValue);'), array(c(14,16,18,20,22,24,26,28,30,32,34,36), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(3,1,0), 'sum(applyValue);'), array(c(8,32,12,36,16,40,10,34,14,38,18,42), c(2,3,2)));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(2,3,0,1), 'sum(applyValue);'), array(c(1,7,13,19,2,8,14,20,3,9,15,21,4,10,16,22,5,11,17,23,6,12,18,24), c(2,2,2,3)));", gStaticEidosValue_LogicalT);
	
	// sapply()
	EidosAssertScriptSuccess("x=integer(0); sapply(x, 'applyValue^2;');", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("x=1:5; sapply(x, 'applyValue^2;');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{1, 4, 9, 16, 25}));
	EidosAssertScriptSuccess("x=1:5; sapply(x, 'product(1:applyValue);');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 6, 24, 120}));
	EidosAssertScriptSuccess("x=1:3; sapply(x, \"rep(''+applyValue, applyValue);\");", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"1", "2", "2", "3", "3", "3"}));
	EidosAssertScriptSuccess("x=1:5; sapply(x, \"paste(rep(''+applyValue, applyValue), sep='');\");", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"1", "22", "333", "4444", "55555"}));
	EidosAssertScriptSuccess("x=1:10; sapply(x, 'if (applyValue % 2) applyValue; else NULL;');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 3, 5, 7, 9}));
	EidosAssertScriptSuccess("x=1:5; sapply(x, 'y=applyValue; NULL;'); y;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("x=1:5; sapply(x, 'y=applyValue; y;');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1, 2, 3, 4, 5}));
	EidosAssertScriptSuccess("x=2; for (i in 1:2) x=sapply(x, 'applyValue^2;'); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16.0)));
	EidosAssertScriptRaise("x=2; sapply(x, 'semanticError;');", 5, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; sapply(x, y);", 25, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; sapply(x, y[T]);", 25, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; sapply(x, 'syntax Error;');", 5, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; sapply(x, y);", 24, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; sapply(x, y[T]);", 24, "unexpected token '@Error'");
	EidosAssertScriptSuccess("x=2; y='x;'; sapply(x, y[T]);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2)));
	
	EidosAssertScriptSuccess("identical(sapply(1:6, 'integer(0);'), integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'integer(0);', simplify='vector'), integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'integer(0);', simplify='matrix'), integer(0));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(1:6, 'integer(0);', simplify='match'), 2:7);", 10, "not all singletons");
	EidosAssertScriptRaise("identical(sapply(1:6, 'integer(0);', simplify='foo'), integer(0));", 10, "unrecognized simplify option");
	EidosAssertScriptRaise("identical(sapply(1:6, 'setSeed(5);'), integer(0));", 10, "must return a non-void value");
	
	EidosAssertScriptSuccess("identical(sapply(1:6, 'applyValue+1;'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'applyValue+1;', simplify='vector'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'applyValue+1;', simplify='match'), 2:7);", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'applyValue+1;'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'applyValue+1;', simplify='vector'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'applyValue+1;', simplify='match'), matrix(2:7, nrow=1));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'applyValue+1;'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'applyValue+1;', simplify='vector'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'applyValue+1;', simplify='match'), matrix(2:7, ncol=1));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'applyValue+1;'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'applyValue+1;', simplify='vector'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'applyValue+1;', simplify='match'), matrix(2:7, ncol=2));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'c(applyValue, applyValue+1);'), c(1,2,2,3,3,4,4,5,5,6,6,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'c(applyValue, applyValue+1);', simplify='vector'), c(1,2,2,3,3,4,4,5,5,6,6,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'c(applyValue, applyValue+1);', simplify='matrix'), matrix(c(1,2,2,3,3,4,4,5,5,6,6,7), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, ncol=2), 'c(applyValue, applyValue+1);', simplify='match'), c(1,2,2,3,3,4,4,5,5,6,6,7));", 10, "not all singletons");
	
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'applyValue+1;'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'applyValue+1;', simplify='vector'), 2:7);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'applyValue+1;', simplify='match'), array(2:7, c(2,1,3)));", gStaticEidosValue_LogicalT);
	
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'c(applyValue, applyValue+1);'), c(1,2,2,3,3,4,4,5,5,6,6,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'c(applyValue, applyValue+1);', simplify='vector'), c(1,2,2,3,3,4,4,5,5,6,6,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'c(applyValue, applyValue+1);', simplify='matrix'), matrix(c(1,2,2,3,3,4,4,5,5,6,6,7), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'c(applyValue, applyValue+1);', simplify='match'), c(1,2,2,3,3,4,4,5,5,6,6,7));", 10, "not all singletons");
	
	EidosAssertScriptSuccess("identical(sapply(1:6, 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(1:6, 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(1:6, 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), c(2,4,6));", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, nrow=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, nrow=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), matrix(c(2,4,6), nrow=1));", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, ncol=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), matrix(c(2,4,6), ncol=1));", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), matrix(c(2,4,6), ncol=2));", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;'), c(1,3,3,5,5,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='vector'), c(1,3,3,5,5,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='matrix'), matrix(c(1,3,3,5,5,7), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='match'), c(1,3,3,5,5,7));", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), array(c(2,4,6), c(2,1,3)));", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;'), c(1,3,3,5,5,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='vector'), c(1,3,3,5,5,7));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='matrix'), matrix(c(1,3,3,5,5,7), nrow=2));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='match'), c(1,3,3,5,5,7));", 10, "included NULL");
	
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else applyValue;'), c(1,3,2,3,5,4,5,7,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else applyValue;', simplify='vector'), c(1,3,2,3,5,4,5,7,6));", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else applyValue;', simplify='matrix'), matrix(c(1,3,2,3,5,4,5,7,6), nrow=2));", 10, "not of a consistent length");
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else applyValue;', simplify='match'), c(1,3,2,3,5,4,5,7,6));", 10, "not all singletons");
}

void _RunFunctionMiscTests(std::string temp_path)
{
	// beep() – this is commented out by default since it would confuse people if the Eidos self-test beeped...
	//EidosAssertScriptSuccess("beep();", gStaticEidosValueNULL);
	//EidosAssertScriptSuccess("beep('Submarine');", gStaticEidosValueNULL);
	
	// citation()
	EidosAssertScriptSuccess("citation();", gStaticEidosValueVOID);
	EidosAssertScriptRaise("citation(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation(_Test(7));", 0, "too many arguments supplied");
	
	// clock()
	EidosAssertScriptSuccess("c = clock(); isFloat(c);", gStaticEidosValue_LogicalT);
	
	// date()
	EidosAssertScriptSuccess("size(strsplit(date(), '-'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptRaise("date(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date(_Test(7));", 0, "too many arguments supplied");
	
	// defineConstant()
	EidosAssertScriptSuccess("defineConstant('foo', 5:10); sum(foo);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(45)));
	EidosAssertScriptRaise("defineConstant('T', 5:10);", 0, "is already defined");
	EidosAssertScriptRaise("defineConstant('foo', 5:10); defineConstant('foo', 5:10); sum(foo);", 29, "is already defined");
	EidosAssertScriptRaise("foo = 5:10; defineConstant('foo', 5:10); sum(foo);", 12, "is already defined");
	EidosAssertScriptRaise("defineConstant('foo', 5:10); rm('foo');", 29, "cannot be removed");
	
	// doCall()
	EidosAssertScriptSuccess("abs(doCall('sin', 0.0) - 0) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("abs(doCall('sin', PI/2) - 1) < 0.000001;", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("doCall('sin');", 0, "requires 1 argument(s), but 0 are supplied");
	EidosAssertScriptRaise("doCall('sin', 'bar');", 0, "cannot be type string");
	EidosAssertScriptRaise("doCall('sin', 0, 1);", 0, "requires at most 1 argument");
	EidosAssertScriptRaise("doCall('si', 0, 1);", 0, "unrecognized function name");
	
	// executeLambda()
	EidosAssertScriptSuccess("x=7; executeLambda('x^2;');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(49)));
	EidosAssertScriptRaise("x=7; executeLambda('x^2');", 5, "unexpected token");
	EidosAssertScriptRaise("x=7; executeLambda(c('x^2;', '5;'));", 5, "must be a singleton");
	EidosAssertScriptRaise("x=7; executeLambda(string(0));", 5, "must be a singleton");
	EidosAssertScriptSuccess("x=7; executeLambda('x=x^2+4;'); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(53)));
	EidosAssertScriptRaise("executeLambda(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(T);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(3);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("x=2; for (i in 1:2) executeLambda('semanticError;'); x;", 20, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; for (i in 1:2) executeLambda(y); x;", 40, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; for (i in 1:2) executeLambda(y[T]); x;", 40, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; for (i in 1:2) executeLambda('syntax Error;'); x;", 20, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; for (i in 1:2) executeLambda(y); x;", 39, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; for (i in 1:2) executeLambda(y[T]); x;", 39, "unexpected token '@Error'");
	EidosAssertScriptSuccess("x=2; for (i in 1:2) executeLambda('x=x^2;'); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16)));
	EidosAssertScriptSuccess("x=2; y='x=x^2;'; for (i in 1:2) executeLambda(y); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16)));
	EidosAssertScriptSuccess("x=2; y='x=x^2;'; for (i in 1:2) executeLambda(y[T]); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16)));
	
	EidosAssertScriptSuccess("x=7; executeLambda('x^2;', T);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(49)));
	EidosAssertScriptRaise("x=7; executeLambda('x^2', T);", 5, "unexpected token");
	EidosAssertScriptRaise("x=7; executeLambda(c('x^2;', '5;'), T);", 5, "must be a singleton");
	EidosAssertScriptRaise("x=7; executeLambda(string(0), T);", 5, "must be a singleton");
	EidosAssertScriptSuccess("x=7; executeLambda('x=x^2+4;', T); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(53)));
	EidosAssertScriptRaise("executeLambda(NULL, T);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(T, T);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(3, T);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(3.5, T);", 0, "cannot be type");
	EidosAssertScriptRaise("executeLambda(_Test(7), T);", 0, "cannot be type");
	EidosAssertScriptRaise("x=2; for (i in 1:2) executeLambda('semanticError;', T); x;", 20, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; for (i in 1:2) executeLambda(y, T); x;", 40, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; for (i in 1:2) executeLambda(y[T], T); x;", 40, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; for (i in 1:2) executeLambda('syntax Error;', T); x;", 20, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; for (i in 1:2) executeLambda(y, T); x;", 39, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; for (i in 1:2) executeLambda(y[T], T); x;", 39, "unexpected token '@Error'");
	EidosAssertScriptSuccess("x=2; for (i in 1:2) executeLambda('x=x^2;', T); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16)));
	EidosAssertScriptSuccess("x=2; y='x=x^2;'; for (i in 1:2) executeLambda(y, T); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16)));
	EidosAssertScriptSuccess("x=2; y='x=x^2;'; for (i in 1:2) executeLambda(y[T], T); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(16)));

	// exists()
	EidosAssertScriptSuccess("exists('T');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("exists('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("foo = 5:10; exists('foo');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("foo = 5:10; rm('foo'); exists('foo');", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("defineConstant('foo', 5:10); exists('foo');", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("a=5; c=7.0; g='foo'; exists(c('a', 'b', 'c', 'd', 'e', 'f', 'g'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false, false, false, true}));
	EidosAssertScriptSuccess("exists(c('T', 'Q', 'F', 'PW', 'PI', 'D', 'E'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical{true, false, true, false, true, false, true}));
	
	// functionSignature()
	EidosAssertScriptSuccess("functionSignature();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("functionSignature('functionSignature');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("functionSignature('foo');", gStaticEidosValueVOID);	// does not throw at present
	EidosAssertScriptRaise("functionSignature(string(0));", 0, "must be a singleton");
	EidosAssertScriptSuccess("functionSignature(NULL);", gStaticEidosValueVOID);		// same as omitting the parameter
	EidosAssertScriptRaise("functionSignature(T);", 0, "cannot be type");
	EidosAssertScriptRaise("functionSignature(3);", 0, "cannot be type");
	EidosAssertScriptRaise("functionSignature(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("functionSignature(_Test(7));", 0, "cannot be type");
	
	// ls()
	EidosAssertScriptSuccess("ls();", gStaticEidosValueVOID);
	EidosAssertScriptRaise("ls(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("ls(_Test(7));", 0, "too many arguments supplied");
	
	// license()
	EidosAssertScriptSuccess("license();", gStaticEidosValueVOID);
	EidosAssertScriptRaise("license(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(_Test(7));", 0, "too many arguments supplied");
	
	// rm()
	EidosAssertScriptSuccess("rm();", gStaticEidosValueVOID);
	EidosAssertScriptRaise("x=37; rm('x'); x;", 15, "undefined identifier");
	EidosAssertScriptSuccess("x=37; rm('y'); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(37)));
	EidosAssertScriptRaise("x=37; rm(); x;", 12, "undefined identifier");
	EidosAssertScriptRaise("rm(3);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("rm(T);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(F);", 0, "cannot be type");
	EidosAssertScriptSuccess("rm(NULL);", gStaticEidosValueVOID);		// same as omitting the parameter
	EidosAssertScriptRaise("rm(INF);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(NAN);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(E);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(PI);", 0, "cannot be type");
	EidosAssertScriptRaise("rm('PI');", 0, "intrinsic Eidos constant");
	EidosAssertScriptRaise("rm('PI', T);", 0, "intrinsic Eidos constant");
	EidosAssertScriptRaise("defineConstant('foo', 1:10); rm('foo'); foo;", 29, "is a constant");
	EidosAssertScriptRaise("defineConstant('foo', 1:10); rm('foo', T); foo;", 43, "undefined identifier");
	
	// setSeed()
	EidosAssertScriptSuccess("setSeed(5); x=runif(10); setSeed(5); y=runif(10); all(x==y);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("setSeed(5); x=runif(10); setSeed(6); y=runif(10); all(x==y);", gStaticEidosValue_LogicalF);
	EidosAssertScriptRaise("setSeed(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed(T);", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed(_Test(7));", 0, "cannot be type");
	
	// getSeed()
	EidosAssertScriptSuccess("setSeed(13); getSeed();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(13)));
	EidosAssertScriptSuccess("setSeed(13); setSeed(7); getSeed();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(7)));
	EidosAssertScriptRaise("getSeed(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed(_Test(7));", 0, "too many arguments supplied");
	
	// source()
	if (Eidos_SlashTmpExists())
		EidosAssertScriptSuccess("path = '" + temp_path + "/EidosSourceTest.txt'; writeFile(path, 'x=9*9;'); source(path); x;", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(81)));
	
	// stop()
	EidosAssertScriptRaise("stop();", 0, "stop() called");
	EidosAssertScriptRaise("stop('Error');", 0, "stop(\"Error\") called");
	EidosAssertScriptRaise("stop(NULL);", 0, "stop() called");		// same as omitting the parameter
	EidosAssertScriptRaise("stop(T);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(3);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(_Test(7));", 0, "cannot be type");
	
	// suppressWarnings()
	EidosAssertScriptSuccess("suppressWarnings(F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("suppressWarnings(T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("suppressWarnings(T); suppressWarnings(F);", gStaticEidosValue_LogicalT);
	
	// system()
	if (Eidos_SlashTmpExists())
	{
		EidosAssertScriptRaise("system('');", 0, "non-empty command string");
		EidosAssertScriptSuccess("system('expr 5 + 5');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("10")));
		EidosAssertScriptSuccess("system('expr', args=c('5', '+', '5'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("10")));
		EidosAssertScriptSuccess("err = system('expr 5 / 0', stderr=T); (err == 'expr: division by zero') | (err == 'expr: división por cero') | (err == 'expr: division par zéro') | (substr(err, 0, 5) == 'expr: ');", gStaticEidosValue_LogicalT);	// unfortunately system localization makes the message returned vary
		EidosAssertScriptSuccess("system('printf foo');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo")));
		EidosAssertScriptSuccess("system(\"printf 'foo bar baz' | wc -m | sed 's/ //g'\");", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("11")));
		EidosAssertScriptSuccess("system(\"(wc -l | sed 's/ //g')\", input='foo\\nbar\\nbaz\\n');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("3")));
		EidosAssertScriptSuccess("system(\"(wc -l | sed 's/ //g')\", input=c('foo', 'bar', 'baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("3")));
		EidosAssertScriptSuccess("system(\"echo foo; echo bar; echo baz;\");", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo", "bar", "baz"}));
	}
	
	// time()
	EidosAssertScriptSuccess("size(strsplit(time(), ':'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3)));
	EidosAssertScriptRaise("time(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time(_Test(7));", 0, "too many arguments supplied");
	
	// usage(); allow zero since this call returns zero on some less-supported platforms
	EidosAssertScriptSuccess("usage() >= 0.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("usage(F) >= 0.0;", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("usage(T) >= 0.0;", gStaticEidosValue_LogicalT);
	
	// version()
	EidosAssertScriptSuccess("type(version(T)) == 'float';", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("type(version(F)) == 'float';", gStaticEidosValue_LogicalT);
	EidosAssertScriptRaise("version(NULL);", 0, "cannot be type NULL");
	EidosAssertScriptRaise("version(3);", 0, "cannot be type integer");
	EidosAssertScriptRaise("version(3.5);", 0, "cannot be type float");
	EidosAssertScriptRaise("version('foo');", 0, "cannot be type string");
	EidosAssertScriptRaise("version(_Test(7));", 0, "cannot be type object");
}

#pragma mark methods
void _RunMethodTests(void)
{
	// methodSignature()
	EidosAssertScriptSuccess("_Test(7).methodSignature();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("_Test(7).methodSignature('methodSignature');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("matrix(_Test(7)).methodSignature('methodSignature');", gStaticEidosValueVOID);
	
	// propertySignature()
	EidosAssertScriptSuccess("_Test(7).propertySignature();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("_Test(7).propertySignature('_yolk');", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("matrix(_Test(7)).propertySignature('_yolk');", gStaticEidosValueVOID);
	
	// size() / length()
	EidosAssertScriptSuccess("_Test(7).size();", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("rep(_Test(7), 5).size();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("matrix(rep(_Test(7), 5)).size();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	
	EidosAssertScriptSuccess("_Test(7).length();", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("rep(_Test(7), 5).length();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("matrix(rep(_Test(7), 5)).length();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	
	// str()
	EidosAssertScriptSuccess("_Test(7).str();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("c(_Test(7), _Test(8), _Test(9)).str();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("matrix(_Test(7)).str();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("matrix(c(_Test(7), _Test(8), _Test(9))).str();", gStaticEidosValueVOID);
}

#pragma mark code examples
void _RunCodeExampleTests(void)
{
	// Fibonacci sequence; see Eidos manual section 2.6.1-ish
	EidosAssertScriptSuccess(	"fib = c(1, 1);												\
								while (size(fib) < 20)										\
								{															\
									next_fib = fib[size(fib) - 1] + fib[size(fib) - 2];		\
									fib = c(fib, next_fib);									\
								}															\
								fib;",
							 EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{1,1,2,3,5,8,13,21,34,55,89,144,233,377,610,987,1597,2584,4181,6765}));
	
	EidosAssertScriptSuccess(	"counter = 12;							\
								factorial = 1;							\
								do										\
								{										\
									factorial = factorial * counter;	\
									counter = counter - 1;				\
								}										\
								while (counter > 0);					\
								factorial;",
							 EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(479001600)));
	
	EidosAssertScriptSuccess(	"last = 200;				\
								p = integer(0);				\
								x = 2:last;					\
								lim = last^0.5;				\
								do {						\
									v = x[0];				\
									if (v > lim)			\
										break;				\
									p = c(p, v);			\
									x = x[x % v != 0];		\
								} while (T);				\
								c(p, x);",
							 EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199}));
}

#pragma mark user-defined functions
void _RunUserDefinedFunctionTests(void)
{
	// Basic functionality
	EidosAssertScriptSuccess("function (i)plus(i x) { return x + 1; } plus(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(6)));
	EidosAssertScriptSuccess("function (f)plus(f x) { return x + 1; } plus(5.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(6.0)));
	EidosAssertScriptSuccess("function (fi)plus(fi x) { return x + 1; } plus(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(6)));
	EidosAssertScriptSuccess("function (fi)plus(fi x) { return x + 1; } plus(5.0);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(6.0)));
	EidosAssertScriptSuccess("function (fi)plus(fi x) { return x + 1; } plus(c(5, 6, 7));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector{6, 7, 8}));
	EidosAssertScriptSuccess("function (fi)plus(fi x) { return x + 1; } plus(c(5.0, 6.0, 7.0));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{6.0, 7.0, 8.0}));
	
	EidosAssertScriptSuccess("function (l$)nor(l$ x, l$ y) { return !(x | y); } nor(F, F);", gStaticEidosValue_LogicalT);
	EidosAssertScriptSuccess("function (l$)nor(l$ x, l$ y) { return !(x | y); } nor(T, F);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("function (l$)nor(l$ x, l$ y) { return !(x | y); } nor(F, T);", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("function (l$)nor(l$ x, l$ y) { return !(x | y); } nor(T, T);", gStaticEidosValue_LogicalF);
	
	EidosAssertScriptSuccess("function (s)append(s x, s y) { return x + ',' + y; } append('foo', 'bar');", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("foo,bar")));
	EidosAssertScriptSuccess("function (s)append(s x, s y) { return x + ',' + y; } append('foo', c('bar','baz'));", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector{"foo,bar", "foo,baz"}));
	
	// Recursion
	EidosAssertScriptSuccess("function (i)fac([i b=10]) { if (b <= 1) return 1; else return b*fac(b-1); } fac(3); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(6)));
	EidosAssertScriptSuccess("function (i)fac([i b=10]) { if (b <= 1) return 1; else return b*fac(b-1); } fac(5); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(120)));
	EidosAssertScriptSuccess("function (i)fac([i b=10]) { if (b <= 1) return 1; else return b*fac(b-1); } fac(); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3628800)));
	
	EidosAssertScriptSuccess("function (s)star(i x) { if (x <= 0) return ''; else return '*' + star(x - 1); } star(5); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("*****")));
	EidosAssertScriptSuccess("function (s)star(i x) { if (x <= 0) return ''; else return '*' + star(x - 1); } star(10); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("**********")));
	EidosAssertScriptSuccess("function (s)star(i x) { if (x <= 0) return ''; else return '*' + star(x - 1); } star(0); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("")));
	
	// Type-checking
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(NULL);", 35, "argument 1 (x) cannot be type NULL");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(T);", 35, "argument 1 (x) cannot be type logical");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(5);", 35, "return value cannot be type integer");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(5.0);", 35, "argument 1 (x) cannot be type float");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo('foo');", 35, "argument 1 (x) cannot be type string");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(_Test(7));", 35, "argument 1 (x) cannot be type object");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo();", 35, "missing required argument x");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(5, 6);", 35, "too many arguments supplied");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(x=5);", 35, "return value cannot be type integer");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(y=5);", 35, "named argument y skipped over required argument x");
	EidosAssertScriptRaise("function (s)foo(i x) { return x; } foo(x=5, y=5);", 35, "unrecognized named argument y");
	
	// Mutual recursion
	EidosAssertScriptSuccess("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return foo(x - 1); } foo(5); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(16)));
	EidosAssertScriptSuccess("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return foo(x - 1); } foo(10); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(56)));
	EidosAssertScriptSuccess("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return foo(x - 1); } foo(-10); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-9)));
	
	EidosAssertScriptSuccess("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return baz(x - 1); } function (i)baz(i x) { return x * foo(x); } foo(5); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(153)));
	EidosAssertScriptSuccess("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return baz(x - 1); } function (i)baz(i x) { return x * foo(x); } foo(10); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2335699)));
	EidosAssertScriptSuccess("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return baz(x - 1); } function (i)baz(i x) { return x * foo(x); } foo(-10); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(-9)));
	
	// Scoping
	EidosAssertScriptRaise("defineConstant('x', 10); function (i)plus(i x) { return x + 1; } plus(5);", 65, "cannot be redefined because it is a constant");
	EidosAssertScriptRaise("defineConstant('x', 10); function (i)plus(i y) { x = y + 1; return x; } plus(5);", 72, "cannot be redefined because it is a constant");
	EidosAssertScriptSuccess("defineConstant('x', 10); function (i)plus(i y) { return x + y; } plus(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(15)));
	EidosAssertScriptRaise("x = 10; function (i)plus(i y) { return x + y; } plus(5);", 48, "undefined identifier x");
	EidosAssertScriptSuccess("defineConstant('x', 10); y = 1; function (i)plus(i y) { return x + y; } plus(5);", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(15)));
	EidosAssertScriptSuccess("defineConstant('x', 10); y = 1; function (i)plus(i y) { return x + y; } plus(5); y; ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(1)));
	EidosAssertScriptSuccess("defineConstant('x', 10); y = 1; function (i)plus(i y) { y = y + 1; return x + y; } plus(5); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(16)));
	EidosAssertScriptSuccess("defineConstant('x', 10); y = 1; function (i)plus(i y) { y = y + 1; return x + y; } plus(5); y; ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(1)));
	EidosAssertScriptSuccess("function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(6)));
	EidosAssertScriptSuccess("function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); x; ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptRaise("function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); y; ", 81, "undefined identifier y");
	EidosAssertScriptRaise("function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); plus(5); ", 81, "identifier 'x' is already defined");
	EidosAssertScriptRaise("x = 3; function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); x; ", 79, "identifier 'x' is already defined");
	EidosAssertScriptSuccess("function (i)plus(i y) { foo(); y = y + 1; return y; } function (void)foo(void) { defineConstant('x', 10); } plus(5); x; ", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(10)));
	EidosAssertScriptRaise("function (i)plus(i x) { foo(); x = x + 1; return x; } function (void)foo(void) { defineConstant('x', 10); } plus(5); x; ", 108, "identifier 'x' is already defined");
	EidosAssertScriptRaise("x = 3; function (i)plus(i y) { foo(); y = y + 1; return y; } function (void)foo(void) { defineConstant('x', 10); } plus(5); x; ", 115, "identifier 'x' is already defined");
	
	// Mutual recursion with lambdas
	
	
	// Tests mimicking built-in Eidos functions; these are good for testing user-defined functions, but also good for testing our built-ins!
	const std::string &builtins_test_string =
#include "eidos_test_builtins.h"
	;
	std::vector<std::string> test_strings = Eidos_string_split(builtins_test_string, "// ***********************************************************************************************");
	
	//for (int testidx = 0; testidx < 100; testidx++)	// uncomment this for a more thorough stress test
	{
		for (std::string &test_string : test_strings)
		{
			std::string test_string_fixed = test_string + "\nreturn T;\n";
			
			EidosAssertScriptSuccess(test_string_fixed, gStaticEidosValue_LogicalT);
		}
	}
}

#pragma mark void EidosValue
void _RunVoidEidosValueTests(void)
{
	// void$ or NULL$ as a type-specifier is not legal, semantically; likewise with similar locutions
	EidosAssertScriptRaise("function (void$)foo(void) { return; } foo();", 14, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (void)foo(void$) { return; } foo();", 23, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (NULL$)foo(void) { return NULL; } foo();", 14, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (void)foo(NULL$) { return; } foo(NULL);", 23, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (v$)foo(void) { return NULL; } foo();", 11, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (void)foo(v$) { return; } foo(NULL);", 20, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (N$)foo(void) { return NULL; } foo();", 11, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (void)foo(N$) { return; } foo(NULL);", 20, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (vN$)foo(void) { return NULL; } foo();", 12, "may not be declared to be singleton");
	EidosAssertScriptRaise("function (void)foo(vN$) { return; } foo(NULL);", 21, "may not be declared to be singleton");
	
	// functions declared to return void must return void
	EidosAssertScriptSuccess("function (void)foo(void) { 5; } foo();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("function (void)foo(void) { 5; return; } foo();", gStaticEidosValueVOID);
	EidosAssertScriptRaise("function (void)foo(void) { return 5; } foo();", 39, "return value must be void");
	EidosAssertScriptRaise("function (void)foo(void) { return NULL; } foo();", 42, "return value must be void");
	
	// functions declared to return NULL must return NULL
	EidosAssertScriptRaise("function (NULL)foo(void) { 5; } foo();", 32, "return value cannot be void");
	EidosAssertScriptRaise("function (NULL)foo(void) { 5; return; } foo();", 40, "return value cannot be void");
	EidosAssertScriptRaise("function (NULL)foo(void) { return 5; } foo();", 39, "return value cannot be type integer");
	EidosAssertScriptSuccess("function (NULL)foo(void) { return NULL; } foo();", gStaticEidosValueNULL);
	
	// functions declared to return * may return anything but void
	EidosAssertScriptRaise("function (*)foo(void) { 5; } foo();", 29, "return value cannot be void");
	EidosAssertScriptRaise("function (*)foo(void) { 5; return; } foo();", 37, "return value cannot be void");
	EidosAssertScriptSuccess("function (*)foo(void) { return 5; } foo();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("function (*)foo(void) { return NULL; } foo();", gStaticEidosValueNULL);
	
	// functions declared to return vNlifso may return anything at all
	EidosAssertScriptSuccess("function (vNlifso)foo(void) { 5; } foo();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("function (vNlifso)foo(void) { 5; return; } foo();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("function (vNlifso)foo(void) { return 5; } foo();", EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(5)));
	EidosAssertScriptSuccess("function (vNlifso)foo(void) { return NULL; } foo();", gStaticEidosValueNULL);
	
	// functions may not be declared as taking a parameter of type void
	EidosAssertScriptRaise("function (void)foo(void x) { return; } foo();", 19, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(void x) { return; } foo(citation());", 19, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo([void x]) { return; } foo(citation());", 20, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(vNlifso x) { return; } foo();", 19, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(vNlifso x) { return; } foo(citation());", 19, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo([vNlifso x = 5]) { return; } foo(citation());", 20, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(integer x, void y) { return; } foo(5);", 30, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(integer x, void y) { return; } foo(5, citation());", 30, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(integer x, [void y]) { return; } foo(5, citation());", 31, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(integer x, vNlifso y) { return; } foo(5);", 30, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(integer x, vNlifso y) { return; } foo(5, citation());", 30, "void is not allowed");
	EidosAssertScriptRaise("function (void)foo(integer x, [vNlifso y = 5]) { return; } foo(5, citation());", 31, "void is not allowed");
	
	// functions *may* be declared as taking a parameter of type NULL, or returning NULL; this is new, with the new void support
	// not sure why anybody would want to do this, of course, but hey, ours not to reason why...
	EidosAssertScriptSuccess("function (void)foo(NULL x) { return; } foo(NULL);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("function (void)bar([NULL x = NULL]) { return; } bar(NULL);", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("function (void)bar([NULL x = NULL]) { return; } bar();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("function (NULL)foo(NULL x) { return x; } foo(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("function (NULL)bar([NULL x = NULL]) { return x; } bar(NULL);", gStaticEidosValueNULL);
	EidosAssertScriptSuccess("function (NULL)bar([NULL x = NULL]) { return x; } bar();", gStaticEidosValueNULL);
	
	// functions may not be passed void arguments
	EidosAssertScriptRaise("function (void)foo(void) { return; } foo(citation());", 37, "too many arguments");
	EidosAssertScriptRaise("function (void)foo(* x) { return; } foo();", 36, "missing required argument");
	EidosAssertScriptRaise("function (void)foo(* x) { return; } foo(citation());", 36, "cannot be type void");
	EidosAssertScriptRaise("function (void)foo(* x) { return; } foo(x = citation());", 36, "cannot be type void");
	EidosAssertScriptRaise("function (void)foo([* x = 5]) { return; } foo(x = citation());", 42, "cannot be type void");
	EidosAssertScriptRaise("function (void)foo([* x = 5]) { return; } foo(citation());", 42, "cannot be type void");
	
	// same again, with isNULL(* x)
	EidosAssertScriptRaise("isNULL();", 0, "missing required argument");
	EidosAssertScriptRaise("isNULL(citation());", 0, "cannot be type void");
	
	// same again, with c(...)
	EidosAssertScriptRaise("c(citation());", 0, "cannot be type void");
	EidosAssertScriptRaise("c(5, citation(), 10);", 0, "cannot be type void");
	
	// void may not participate in any operator: [], (), ., + (unary), - (unary), !, ^, :, *, /, %, +, -, <, >, <=, >=, ==, !=, &, |, ?else, =
	// we do not comprehensively test all operand types here, but I think the interpreter code is written such that these tests should suffice
	EidosAssertScriptRaise("citation()[0];", 10, "type void is not supported");
	EidosAssertScriptRaise("citation()[logical(0)];", 10, "type void is not supported");
	EidosAssertScriptRaise("(1:5)[citation()];", 5, "type void is not supported");
	
	EidosAssertScriptRaise("citation()();", 8, "illegal operand for a function call");
	EidosAssertScriptRaise("(citation())();", 9, "illegal operand for a function call");
	EidosAssertScriptSuccess("(citation());", gStaticEidosValueVOID);				// about the only thing that is legal with void!
	
	EidosAssertScriptRaise("citation().test();", 10, "type void is not supported");
	EidosAssertScriptRaise("citation().test = 5;", 16, "type void is not supported");
	
	EidosAssertScriptRaise("+citation();", 0, "type void is not supported");
	
	EidosAssertScriptRaise("-citation();", 0, "type void is not supported");
	
	EidosAssertScriptRaise("!citation();", 0, "type void is not supported");
	
	EidosAssertScriptRaise("citation()^5;", 10, "type void is not supported");
	EidosAssertScriptRaise("5^citation();", 1, "type void is not supported");
	EidosAssertScriptRaise("citation()^citation();", 10, "type void is not supported");
	
	EidosAssertScriptRaise("citation():5;", 10, "type void is not supported");
	EidosAssertScriptRaise("5:citation();", 1, "type void is not supported");
	EidosAssertScriptRaise("citation():citation();", 10, "type void is not supported");
	
	EidosAssertScriptRaise("citation()*5;", 10, "type void is not supported");
	EidosAssertScriptRaise("5*citation();", 1, "type void is not supported");
	EidosAssertScriptRaise("citation()*citation();", 10, "type void is not supported");
	
	EidosAssertScriptRaise("citation()/5;", 10, "type void is not supported");
	EidosAssertScriptRaise("5/citation();", 1, "type void is not supported");
	EidosAssertScriptRaise("citation()/citation();", 10, "type void is not supported");
	
	EidosAssertScriptRaise("citation()%5;", 10, "type void is not supported");
	EidosAssertScriptRaise("5%citation();", 1, "type void is not supported");
	EidosAssertScriptRaise("citation()%citation();", 10, "type void is not supported");
	
	EidosAssertScriptRaise("5 + citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() + 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() + citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 - citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() - 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() - citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 < citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() < 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() < citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 > citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() > 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() > citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 <= citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() <= 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() <= citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 >= citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() >= 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() >= citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 == citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() == 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() == citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("5 != citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() != 5;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() != citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("T & citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() & T;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() & citation();", 11, "type void is not supported");
	
	EidosAssertScriptRaise("T | citation();", 2, "type void is not supported");
	EidosAssertScriptRaise("citation() | T;", 11, "type void is not supported");
	EidosAssertScriptRaise("citation() | citation();", 11, "type void is not supported");
	
	EidosAssertScriptSuccess("T ? citation() else F;", gStaticEidosValueVOID);		// also legal with void, as long as you don't try to use the result...
	EidosAssertScriptSuccess("F ? citation() else F;", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("T ? F else citation();", gStaticEidosValue_LogicalF);
	EidosAssertScriptSuccess("F ? F else citation();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("T ? citation() else citation();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("F ? citation() else citation();", gStaticEidosValueVOID);
	EidosAssertScriptRaise("citation() ? T else F;", 11, "size() != 1");
	
	EidosAssertScriptRaise("x = citation();", 2, "void may never be assigned");
	
	// void may not be used in while, do-while, for, etc.
	EidosAssertScriptRaise("if (citation()) T;", 0, "size() != 1");
	EidosAssertScriptRaise("if (citation()) T; else F;", 0, "size() != 1");
	EidosAssertScriptSuccess("if (T) citation(); else citation();", gStaticEidosValueVOID);
	EidosAssertScriptSuccess("if (F) citation(); else citation();", gStaticEidosValueVOID);
	
	EidosAssertScriptRaise("while (citation()) F;", 0, "size() != 1");
	
	EidosAssertScriptRaise("do F; while (citation());", 0, "size() != 1");
	
	EidosAssertScriptRaise("for (x in citation()) T;", 0, "does not allow void");
}






























