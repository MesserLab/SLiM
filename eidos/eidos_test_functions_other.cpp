//
//  eidos_test_functions_other.cpp
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

#include <string>
#include <vector>


#pragma mark matrix and array
void _RunFunctionMatrixArrayTests(void)
{
	// array()
	EidosAssertScriptRaise("array(5, integer(0));", 0, "at least a matrix");
	EidosAssertScriptRaise("array(5, 1);", 0, "at least a matrix");
	EidosAssertScriptRaise("array(5, c(1,2));", 0, "product of the proposed dimensions");
	EidosAssertScriptSuccess_L("identical(array(5, c(1,1)), matrix(5));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6, c(2,3)), matrix(1:6, nrow=2));", true);
	EidosAssertScriptSuccess_L("identical(array(1:6, c(3,2)), matrix(1:6, nrow=3));", true);
	EidosAssertScriptSuccess_L("size(array(1:12, c(3,2,2))) == 12;", true);		// FIXME not sure how to test higher-dimensional arrays right now...
	
	// cbind()
	EidosAssertScriptRaise("cbind(5, 5.5);", 0, "be the same type");
	EidosAssertScriptRaise("cbind(5, array(5, c(1,1,1)));", 0, "all arguments be vectors or matrices");
	EidosAssertScriptRaise("cbind(matrix(1:4, nrow=2), matrix(1:4, nrow=4));", 0, "number of row");
	EidosAssertScriptSuccess_L("identical(cbind(5), matrix(5));", true);
	EidosAssertScriptSuccess_L("identical(cbind(1:5), matrix(1:5, ncol=1));", true);
	EidosAssertScriptSuccess_L("identical(cbind(1:5, 6:10), matrix(1:10, ncol=2));", true);
	EidosAssertScriptSuccess_L("identical(cbind(1:5, 6:10, NULL, integer(0), 11:15), matrix(1:15, ncol=3));", true);
	EidosAssertScriptSuccess_L("identical(cbind(matrix(1:6, nrow=2), matrix(7:12, nrow=2)), matrix(1:12, nrow=2));", true);
	EidosAssertScriptSuccess_L("identical(cbind(matrix(1:6, ncol=2), matrix(7:12, ncol=2)), matrix(1:12, nrow=3));", true);
	EidosAssertScriptSuccess_L("identical(cbind(matrix(1:6, nrow=1), matrix(7:12, nrow=1)), matrix(1:12, nrow=1));", true);
	
	// diag()
	EidosAssertScriptRaise("diag(array(5, c(1, 1, 1)));", 0, "a vector or a matrix");
	EidosAssertScriptRaise("diag(matrix(5), nrow=1);", 0, "must be NULL");
	EidosAssertScriptRaise("diag(matrix(5), ncol=1);", 0, "must be NULL");
	EidosAssertScriptSuccess_IV("diag(matrix(5));", {5});
	EidosAssertScriptSuccess_IV("diag(matrix(1:10, ncol=5));", {1, 4});
	EidosAssertScriptSuccess_IV("diag(t(matrix(1:10, ncol=5)));", {1, 4});
	EidosAssertScriptSuccess_IV("diag(matrix(1:16, ncol=4));", {1, 6, 11, 16});
	EidosAssertScriptSuccess_IV("diag(t(matrix(1:16, ncol=4)));", {1, 6, 11, 16});
	
	EidosAssertScriptRaise("diag(ncol=3);", 0, "one of four specific");
	EidosAssertScriptRaise("diag(nrow=0);", 0, "matrix must be >= 1");
	EidosAssertScriptRaise("diag(nrow=1, ncol=0);", 0, "matrix must be >= 1");
	EidosAssertScriptSuccess_L("d = diag(nrow=1); identical(d, matrix(1));", true);
	EidosAssertScriptSuccess_L("d = diag(nrow=3); identical(d, matrix(c(1, 0, 0, 0, 1, 0, 0, 0, 1), nrow=3, ncol=3));", true);
	EidosAssertScriptSuccess_L("d = diag(nrow=3, ncol=2); identical(d, matrix(c(1, 0, 0, 0, 1, 0), nrow=3, ncol=2));", true);
	EidosAssertScriptSuccess_L("d = diag(nrow=2, ncol=3); identical(d, matrix(c(1, 0, 0, 1, 0, 0), nrow=2, ncol=3));", true);
	
	EidosAssertScriptRaise("diag(T);", 0, "one of four specific");
	EidosAssertScriptRaise("diag(F);", 0, "one of four specific");
	EidosAssertScriptRaise("diag(1.5);", 0, "one of four specific");
	EidosAssertScriptRaise("diag('foo');", 0, "one of four specific");
	EidosAssertScriptRaise("diag(0);", 0, "size must be >= 1");
	EidosAssertScriptSuccess_L("d = diag(1); identical(d, matrix(1));", true);
	EidosAssertScriptSuccess_L("d = diag(3); identical(d, matrix(c(1, 0, 0, 0, 1, 0, 0, 0, 1), nrow=3, ncol=3));", true);
	
	EidosAssertScriptSuccess_L("d = diag(c(1,4)); identical(d, matrix(c(1, 0, 0, 4), nrow=2));", true);
	EidosAssertScriptSuccess_L("d = diag(c(1,4), ncol=3); identical(d, matrix(c(1, 0, 0, 4, 0, 0), nrow=2));", true);
	EidosAssertScriptRaise("diag(c(1,4), nrow=3);", 0, "truncated or recycled");
	EidosAssertScriptSuccess_L("d = diag(c(1,4), nrow=3, ncol=2); identical(d, matrix(c(1, 0, 0, 0, 4, 0), nrow=3));", true);
	EidosAssertScriptSuccess_L("d = diag(c(1,4), nrow=2, ncol=3); identical(d, matrix(c(1, 0, 0, 4, 0, 0), nrow=2));", true);
	EidosAssertScriptRaise("diag(c(1,4), nrow=3, ncol=3);", 0, "truncated or recycled");
	EidosAssertScriptRaise("diag(c(1,4), nrow=1);", 0, "truncated or recycled");
	EidosAssertScriptRaise("diag(c(1,4), ncol=1);", 0, "truncated or recycled");
	
	EidosAssertScriptSuccess_L("d = diag(c(1.0,4)); identical(d, matrix(c(1.0, 0, 0, 4), nrow=2));", true);
	EidosAssertScriptSuccess_L("d = diag(c(1.0,4), ncol=3); identical(d, matrix(c(1.0, 0, 0, 4, 0, 0), nrow=2));", true);
	EidosAssertScriptRaise("diag(c(1.0,4), nrow=3);", 0, "truncated or recycled");
	EidosAssertScriptSuccess_L("d = diag(c(1.0,4), nrow=3, ncol=2); identical(d, matrix(c(1.0, 0, 0, 0, 4, 0), nrow=3));", true);
	EidosAssertScriptSuccess_L("d = diag(c(1.0,4), nrow=2, ncol=3); identical(d, matrix(c(1.0, 0, 0, 4, 0, 0), nrow=2));", true);
	EidosAssertScriptRaise("diag(c(1.0,4), nrow=3, ncol=3);", 0, "truncated or recycled");
	EidosAssertScriptRaise("diag(c(1.0,4), nrow=1);", 0, "truncated or recycled");
	EidosAssertScriptRaise("diag(c(1.0,4), ncol=1);", 0, "truncated or recycled");
	
	// dim()
	EidosAssertScriptSuccess_NULL("dim(NULL);");
	EidosAssertScriptSuccess_NULL("dim(T);");
	EidosAssertScriptSuccess_NULL("dim(1);");
	EidosAssertScriptSuccess_NULL("dim(1.5);");
	EidosAssertScriptSuccess_NULL("dim('foo');");
	EidosAssertScriptSuccess_NULL("dim(c(T, F));");
	EidosAssertScriptSuccess_NULL("dim(c(1, 2));");
	EidosAssertScriptSuccess_NULL("dim(c(1.5, 2.0));");
	EidosAssertScriptSuccess_NULL("dim(c('foo', 'bar'));");
	EidosAssertScriptSuccess_IV("dim(matrix(3));", {1, 1});
	EidosAssertScriptSuccess_IV("dim(matrix(1:6, nrow=2));", {2, 3});
	EidosAssertScriptSuccess_IV("dim(matrix(1:6, nrow=2, byrow=T));", {2, 3});
	EidosAssertScriptSuccess_IV("dim(matrix(1:6, ncol=2));", {3, 2});
	EidosAssertScriptSuccess_IV("dim(matrix(1:6, ncol=2, byrow=T));", {3, 2});
	EidosAssertScriptSuccess_IV("dim(array(1:24, c(2,3,4)));", {2, 3, 4});
	EidosAssertScriptSuccess_IV("dim(array(1:48, c(2,3,4,2)));", {2, 3, 4, 2});
	EidosAssertScriptSuccess_IV("dim(matrix(3.0));", {1, 1});
	EidosAssertScriptSuccess_IV("dim(matrix(1.0:6, nrow=2));", {2, 3});
	EidosAssertScriptSuccess_IV("dim(matrix(1.0:6, nrow=2, byrow=T));", {2, 3});
	EidosAssertScriptSuccess_IV("dim(matrix(1.0:6, ncol=2));", {3, 2});
	EidosAssertScriptSuccess_IV("dim(matrix(1.0:6, ncol=2, byrow=T));", {3, 2});
	EidosAssertScriptSuccess_IV("dim(array(1.0:24, c(2,3,4)));", {2, 3, 4});
	EidosAssertScriptSuccess_IV("dim(array(1.0:48, c(2,3,4,2)));", {2, 3, 4, 2});
	
	// drop()
	EidosAssertScriptSuccess_NULL("drop(NULL);");
	EidosAssertScriptSuccess_L("identical(drop(integer(0)), integer(0));", true);
	EidosAssertScriptSuccess_L("identical(drop(5), 5);", true);
	EidosAssertScriptSuccess_L("identical(drop(5:9), 5:9);", true);
	EidosAssertScriptSuccess_L("identical(drop(matrix(5)), 5);", true);
	EidosAssertScriptSuccess_L("identical(drop(matrix(5:9)), 5:9);", true);
	EidosAssertScriptSuccess_L("identical(drop(matrix(1:6, ncol=1)), 1:6);", true);
	EidosAssertScriptSuccess_L("identical(drop(matrix(1:6, nrow=1)), 1:6);", true);
	EidosAssertScriptSuccess_L("identical(drop(matrix(1:6, nrow=2)), matrix(1:6, nrow=2));", true);
	EidosAssertScriptSuccess_L("identical(drop(array(5, c(1,1,1))), 5);", true);
	EidosAssertScriptSuccess_L("identical(drop(array(1:6, c(6,1,1))), 1:6);", true);
	EidosAssertScriptSuccess_L("identical(drop(array(1:6, c(1,6,1))), 1:6);", true);
	EidosAssertScriptSuccess_L("identical(drop(array(1:6, c(1,1,6))), 1:6);", true);
	EidosAssertScriptSuccess_L("identical(drop(array(1:6, c(2,3,1))), matrix(1:6, nrow=2));", true);
	EidosAssertScriptSuccess_L("identical(drop(array(1:6, c(1,2,3))), matrix(1:6, nrow=2));", true);
	EidosAssertScriptSuccess_L("identical(drop(array(1:6, c(2,1,3))), matrix(1:6, nrow=2));", true);
	EidosAssertScriptSuccess_L("identical(drop(array(1:12, c(12,1,1))), 1:12);", true);
	EidosAssertScriptSuccess_L("identical(drop(array(1:12, c(2,3,2))), array(1:12, c(2,3,2)));", true);
	
	// lowerTri()
	EidosAssertScriptRaise("ut = lowerTri(0);", 5, "is not a matrix");
	EidosAssertScriptSuccess_L("ut = lowerTri(matrix(5)); identical(ut, matrix(F));", true);
	EidosAssertScriptSuccess_L("ut = lowerTri(matrix(5), T); identical(ut, matrix(T));", true);
	EidosAssertScriptSuccess_IV("x = matrix(1:16, 4); ut = lowerTri(x); x[c(ut)];", {2, 3, 4, 7, 8, 12});
	EidosAssertScriptSuccess_IV("x = matrix(1:16, 4); ut = lowerTri(x, T); x[c(ut)];", {1, 2, 3, 4, 6, 7, 8, 11, 12, 16});
	EidosAssertScriptSuccess_IV("x = matrix(1:12, 3); ut = lowerTri(x); x[c(ut)];", {2, 3, 6});
	EidosAssertScriptSuccess_IV("x = matrix(1:12, 3); ut = lowerTri(x, T); x[c(ut)];", {1, 2, 3, 5, 6, 9});
	EidosAssertScriptSuccess_IV("x = matrix(1:12, 4); ut = lowerTri(x); x[c(ut)];", {2, 3, 4, 7, 8, 12});
	EidosAssertScriptSuccess_IV("x = matrix(1:12, 4); ut = lowerTri(x, T); x[c(ut)];", {1, 2, 3, 4, 6, 7, 8, 11, 12});
	
	// matrix()
	EidosAssertScriptSuccess_IV("matrix(3);", {3});
	EidosAssertScriptSuccess_IV("matrix(3, nrow=1);", {3});
	EidosAssertScriptSuccess_IV("matrix(3, ncol=1);", {3});
	EidosAssertScriptSuccess_IV("matrix(3, nrow=1, ncol=1);", {3});
	EidosAssertScriptSuccess_IV("matrix(1:6, nrow=1);", {1, 2, 3, 4, 5, 6});
	EidosAssertScriptSuccess_IV("matrix(1:6, ncol=1);", {1, 2, 3, 4, 5, 6});
	EidosAssertScriptSuccess_IV("matrix(1:6, ncol=2);", {1, 2, 3, 4, 5, 6});
	EidosAssertScriptSuccess_IV("matrix(1:6, ncol=2, byrow=T);", {1, 3, 5, 2, 4, 6});
	EidosAssertScriptSuccess_IV("matrix(1:6, ncol=3, byrow=T);", {1, 4, 2, 5, 3, 6});
	EidosAssertScriptRaise("matrix(1:5, ncol=2);", 0, "not a multiple of the supplied column count");
	EidosAssertScriptRaise("matrix(1:5, nrow=2);", 0, "not a multiple of the supplied row count");
	EidosAssertScriptRaise("matrix(1:5, nrow=2, ncol=2);", 0, "length equal to the product");
	EidosAssertScriptSuccess_L("identical(matrix(1:6, ncol=2), matrix(c(1, 4, 2, 5, 3, 6), ncol=2, byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(1:6, ncol=3), matrix(c(1, 3, 5, 2, 4, 6), ncol=3, byrow=T));", true);
	EidosAssertScriptSuccess_FV("matrix(3.0);", {3});
	EidosAssertScriptSuccess_FV("matrix(3.0, nrow=1);", {3});
	EidosAssertScriptSuccess_FV("matrix(3.0, ncol=1);", {3});
	EidosAssertScriptSuccess_FV("matrix(3.0, nrow=1, ncol=1);", {3});
	EidosAssertScriptSuccess_FV("matrix(1.0:6, nrow=1);", {1, 2, 3, 4, 5, 6});
	EidosAssertScriptSuccess_FV("matrix(1.0:6, ncol=1);", {1, 2, 3, 4, 5, 6});
	EidosAssertScriptSuccess_FV("matrix(1.0:6, ncol=2);", {1, 2, 3, 4, 5, 6});
	EidosAssertScriptSuccess_FV("matrix(1.0:6, ncol=2, byrow=T);", {1, 3, 5, 2, 4, 6});
	EidosAssertScriptSuccess_FV("matrix(1.0:6, ncol=3, byrow=T);", {1, 4, 2, 5, 3, 6});
	EidosAssertScriptRaise("matrix(1.0:5, ncol=2);", 0, "not a multiple of the supplied column count");
	EidosAssertScriptRaise("matrix(1.0:5, nrow=2);", 0, "not a multiple of the supplied row count");
	EidosAssertScriptRaise("matrix(1.0:5, nrow=2, ncol=2);", 0, "length equal to the product");
	EidosAssertScriptSuccess_L("identical(matrix(1.0:6, ncol=2), matrix(c(1.0, 4, 2, 5, 3, 6), ncol=2, byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(matrix(1.0:6, ncol=3), matrix(c(1.0, 3, 5, 2, 4, 6), ncol=3, byrow=T));", true);
	EidosAssertScriptRaise("matrix(integer(0), nrow=0);", 0, "dimension <= 0");
	EidosAssertScriptRaise("matrix(integer(0), ncol=0);", 0, "dimension <= 0");
	EidosAssertScriptRaise("matrix(integer(0));", 0, "matrix with zero elements");
	
	// matrixMult()
	EidosAssertScriptRaise("matrixMult(matrix(5), 5);", 0, "is not a matrix");
	EidosAssertScriptRaise("matrixMult(5, matrix(5));", 0, "is not a matrix");
	EidosAssertScriptRaise("matrixMult(matrix(5), matrix(5.5));", 0, "are the same type");
	EidosAssertScriptRaise("matrixMult(matrix(1:5), matrix(1:5));", 0, "not conformable");
	EidosAssertScriptSuccess_L("A = matrix(2); B = matrix(5); identical(matrixMult(A, B), matrix(10));", true);
	EidosAssertScriptSuccess_L("A = matrix(2); B = matrix(1:5, nrow=1); identical(matrixMult(A, B), matrix(c(2,4,6,8,10), nrow=1));", true);
	EidosAssertScriptSuccess_L("A = matrix(1:5, ncol=1); B = matrix(2); identical(matrixMult(A, B), matrix(c(2,4,6,8,10), ncol=1));", true);
	EidosAssertScriptSuccess_L("A = matrix(1:5, ncol=1); B = matrix(1:5, nrow=1); identical(matrixMult(A, B), matrix(c(1:5, (1:5)*2, (1:5)*3, (1:5)*4, (1:5)*5), ncol=5));", true);
	EidosAssertScriptSuccess_L("A = matrix(1:5, nrow=1); B = matrix(1:5, ncol=1); identical(matrixMult(A, B), matrix(55));", true);
	EidosAssertScriptSuccess_L("A = matrix(1:6, nrow=2); B = matrix(1:6, ncol=2); identical(matrixMult(A, B), matrix(c(22, 28, 49, 64), nrow=2));", true);
	EidosAssertScriptSuccess_L("A = matrix(1:6, ncol=2); B = matrix(1:6, nrow=2); identical(matrixMult(A, B), matrix(c(9, 12, 15, 19, 26, 33, 29, 40, 51), nrow=3));", true);
	
	EidosAssertScriptRaise("matrixMult(matrix(5.0), 5.0);", 0, "is not a matrix");
	EidosAssertScriptRaise("matrixMult(5.0, matrix(5.0));", 0, "is not a matrix");
	EidosAssertScriptRaise("matrixMult(matrix(5.0), matrix(5));", 0, "are the same type");
	EidosAssertScriptRaise("matrixMult(matrix(1.0:5.0), matrix(1.0:5.0));", 0, "not conformable");
	EidosAssertScriptSuccess_L("A = matrix(2.0); B = matrix(5.0); identical(matrixMult(A, B), matrix(10.0));", true);
	EidosAssertScriptSuccess_L("A = matrix(2.0); B = matrix(1.0:5.0, nrow=1); identical(matrixMult(A, B), matrix(c(2.0,4.0,6.0,8.0,10.0), nrow=1));", true);
	EidosAssertScriptSuccess_L("A = matrix(1.0:5.0, ncol=1); B = matrix(2.0); identical(matrixMult(A, B), matrix(c(2.0,4.0,6.0,8.0,10.0), ncol=1));", true);
	EidosAssertScriptSuccess_L("A = matrix(1.0:5.0, ncol=1); B = matrix(1.0:5.0, nrow=1); identical(matrixMult(A, B), matrix(c(1.0:5.0, (1.0:5.0)*2, (1.0:5.0)*3, (1.0:5.0)*4, (1.0:5.0)*5), ncol=5));", true);
	EidosAssertScriptSuccess_L("A = matrix(1.0:5.0, nrow=1); B = matrix(1.0:5.0, ncol=1); identical(matrixMult(A, B), matrix(55.0));", true);
	EidosAssertScriptSuccess_L("A = matrix(1.0:6.0, nrow=2); B = matrix(1.0:6.0, ncol=2); identical(matrixMult(A, B), matrix(c(22.0, 28.0, 49.0, 64.0), nrow=2));", true);
	EidosAssertScriptSuccess_L("A = matrix(1.0:6.0, ncol=2); B = matrix(1.0:6.0, nrow=2); identical(matrixMult(A, B), matrix(c(9.0, 12.0, 15.0, 19.0, 26.0, 33.0, 29.0, 40.0, 51.0), nrow=3));", true);
	
	// ncol()
	EidosAssertScriptSuccess_NULL("ncol(NULL);");
	EidosAssertScriptSuccess_NULL("ncol(T);");
	EidosAssertScriptSuccess_NULL("ncol(1);");
	EidosAssertScriptSuccess_NULL("ncol(1.5);");
	EidosAssertScriptSuccess_NULL("ncol('foo');");
	EidosAssertScriptSuccess_NULL("ncol(c(T, F));");
	EidosAssertScriptSuccess_NULL("ncol(c(1, 2));");
	EidosAssertScriptSuccess_NULL("ncol(c(1.5, 2.0));");
	EidosAssertScriptSuccess_NULL("ncol(c('foo', 'bar'));");
	EidosAssertScriptSuccess_IV("ncol(matrix(3));", {1});
	EidosAssertScriptSuccess_IV("ncol(matrix(1:6, nrow=2));", {3});
	EidosAssertScriptSuccess_IV("ncol(matrix(1:6, nrow=2, byrow=T));", {3});
	EidosAssertScriptSuccess_IV("ncol(matrix(1:6, ncol=2));", {2});
	EidosAssertScriptSuccess_IV("ncol(matrix(1:6, ncol=2, byrow=T));", {2});
	EidosAssertScriptSuccess_IV("ncol(array(1:24, c(2,3,4)));", {3});
	EidosAssertScriptSuccess_IV("ncol(array(1:48, c(2,3,4,2)));", {3});
	EidosAssertScriptSuccess_IV("ncol(matrix(3.0));", {1});
	EidosAssertScriptSuccess_IV("ncol(matrix(1.0:6, nrow=2));", {3});
	EidosAssertScriptSuccess_IV("ncol(matrix(1.0:6, nrow=2, byrow=T));", {3});
	EidosAssertScriptSuccess_IV("ncol(matrix(1.0:6, ncol=2));", {2});
	EidosAssertScriptSuccess_IV("ncol(matrix(1.0:6, ncol=2, byrow=T));", {2});
	EidosAssertScriptSuccess_IV("ncol(array(1.0:24, c(2,3,4)));", {3});
	EidosAssertScriptSuccess_IV("ncol(array(1.0:48, c(2,3,4,2)));", {3});
	
	// nrow()
	EidosAssertScriptSuccess_NULL("nrow(NULL);");
	EidosAssertScriptSuccess_NULL("nrow(T);");
	EidosAssertScriptSuccess_NULL("nrow(1);");
	EidosAssertScriptSuccess_NULL("nrow(1.5);");
	EidosAssertScriptSuccess_NULL("nrow('foo');");
	EidosAssertScriptSuccess_NULL("nrow(c(T, F));");
	EidosAssertScriptSuccess_NULL("nrow(c(1, 2));");
	EidosAssertScriptSuccess_NULL("nrow(c(1.5, 2.0));");
	EidosAssertScriptSuccess_NULL("nrow(c('foo', 'bar'));");
	EidosAssertScriptSuccess_IV("nrow(matrix(3));", {1});
	EidosAssertScriptSuccess_IV("nrow(matrix(1:6, nrow=2));", {2});
	EidosAssertScriptSuccess_IV("nrow(matrix(1:6, nrow=2, byrow=T));", {2});
	EidosAssertScriptSuccess_IV("nrow(matrix(1:6, ncol=2));", {3});
	EidosAssertScriptSuccess_IV("nrow(matrix(1:6, ncol=2, byrow=T));", {3});
	EidosAssertScriptSuccess_IV("nrow(array(1:24, c(2,3,4)));", {2});
	EidosAssertScriptSuccess_IV("nrow(array(1:48, c(2,3,4,2)));", {2});
	EidosAssertScriptSuccess_IV("nrow(matrix(3.0));", {1});
	EidosAssertScriptSuccess_IV("nrow(matrix(1.0:6, nrow=2));", {2});
	EidosAssertScriptSuccess_IV("nrow(matrix(1.0:6, nrow=2, byrow=T));", {2});
	EidosAssertScriptSuccess_IV("nrow(matrix(1.0:6, ncol=2));", {3});
	EidosAssertScriptSuccess_IV("nrow(matrix(1.0:6, ncol=2, byrow=T));", {3});
	EidosAssertScriptSuccess_IV("nrow(array(1.0:24, c(2,3,4)));", {2});
	EidosAssertScriptSuccess_IV("nrow(array(1.0:48, c(2,3,4,2)));", {2});
	
	// rbind()
	EidosAssertScriptRaise("rbind(5, 5.5);", 0, "be the same type");
	EidosAssertScriptRaise("rbind(5, array(5, c(1,1,1)));", 0, "all arguments be vectors or matrices");
	EidosAssertScriptRaise("rbind(matrix(1:4, nrow=2), matrix(1:4, nrow=4));", 0, "number of columns");
	EidosAssertScriptSuccess_L("identical(rbind(5), matrix(5));", true);
	EidosAssertScriptSuccess_L("identical(rbind(1:5), matrix(1:5, nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(rbind(1:5, 6:10), matrix(1:10, nrow=2, byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(rbind(1:5, 6:10, NULL, integer(0), 11:15), matrix(1:15, nrow=3, byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(rbind(matrix(1:6, nrow=2), matrix(7:12, nrow=2)), matrix(c(1,2,7,8,3,4,9,10,5,6,11,12), nrow=4));", true);
	EidosAssertScriptSuccess_L("identical(rbind(matrix(1:6, ncol=2), matrix(7:12, ncol=2)), matrix(c(1,2,3,7,8,9,4,5,6,10,11,12), ncol=2));", true);
	EidosAssertScriptSuccess_L("identical(rbind(matrix(1:6, ncol=1), matrix(7:12, ncol=1)), matrix(1:12, ncol=1));", true);
	
	// t()
	EidosAssertScriptRaise("t(NULL);", 0, "is not a matrix");
	EidosAssertScriptRaise("t(T);", 0, "is not a matrix");
	EidosAssertScriptRaise("t(1);", 0, "is not a matrix");
	EidosAssertScriptRaise("t(1.5);", 0, "is not a matrix");
	EidosAssertScriptRaise("t('foo');", 0, "is not a matrix");
	EidosAssertScriptSuccess_L("identical(t(matrix(3)), matrix(3));", true);
	EidosAssertScriptSuccess_L("identical(t(matrix(1:6, nrow=2)), matrix(1:6, ncol=2, byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(t(matrix(1:6, nrow=2, byrow=T)), matrix(1:6, ncol=2, byrow=F));", true);
	EidosAssertScriptSuccess_L("identical(t(matrix(1:6, ncol=2)), matrix(1:6, nrow=2, byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(t(matrix(1:6, ncol=2, byrow=T)), matrix(1:6, nrow=2, byrow=F));", true);
	EidosAssertScriptSuccess_L("identical(t(matrix(3.0)), matrix(3.0));", true);
	EidosAssertScriptSuccess_L("identical(t(matrix(1.0:6, nrow=2)), matrix(1.0:6, ncol=2, byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(t(matrix(1.0:6, nrow=2, byrow=T)), matrix(1.0:6, ncol=2, byrow=F));", true);
	EidosAssertScriptSuccess_L("identical(t(matrix(1.0:6, ncol=2)), matrix(1.0:6, nrow=2, byrow=T));", true);
	EidosAssertScriptSuccess_L("identical(t(matrix(1.0:6, ncol=2, byrow=T)), matrix(1.0:6, nrow=2, byrow=F));", true);
	EidosAssertScriptRaise("t(array(1:24, c(2,3,4)));", 0, "is not a matrix");
	EidosAssertScriptRaise("t(array(1:48, c(2,3,4,2)));", 0, "is not a matrix");
	
	// upperTri()
	EidosAssertScriptRaise("ut = upperTri(0);", 5, "is not a matrix");
	EidosAssertScriptSuccess_L("ut = upperTri(matrix(5)); identical(ut, matrix(F));", true);
	EidosAssertScriptSuccess_L("ut = upperTri(matrix(5), T); identical(ut, matrix(T));", true);
	EidosAssertScriptSuccess_IV("x = matrix(1:16, 4); ut = upperTri(x); x[c(ut)];", {5, 9, 10, 13, 14, 15});
	EidosAssertScriptSuccess_IV("x = matrix(1:16, 4); ut = upperTri(x, T); x[c(ut)];", {1, 5, 6, 9, 10, 11, 13, 14, 15, 16});
	EidosAssertScriptSuccess_IV("x = matrix(1:12, 3); ut = upperTri(x); x[c(ut)];", {4, 7, 8, 10, 11, 12});
	EidosAssertScriptSuccess_IV("x = matrix(1:12, 3); ut = upperTri(x, T); x[c(ut)];", {1, 4, 5, 7, 8, 9, 10, 11, 12});
	EidosAssertScriptSuccess_IV("x = matrix(1:12, 4); ut = upperTri(x); x[c(ut)];", {5, 9, 10});
	EidosAssertScriptSuccess_IV("x = matrix(1:12, 4); ut = upperTri(x, T); x[c(ut)];", {1, 5, 6, 9, 10, 11});
}

#pragma mark filesystem access
void _RunFunctionFilesystemTests(const std::string &temp_path)
{
	if (!Eidos_TemporaryDirectoryExists())
		return;
	
	// filesAtPath() – hard to know how to test this!  These tests should be true on Un*x machines, anyway – but might be disallowed by file permissions.
	EidosAssertScriptSuccess_L("type(filesAtPath(tempdir())) == 'string';", true);
	// these always fail on Windows and I can't think of any good easy replacement
	#ifndef _WIN32
	EidosAssertScriptSuccess_L("type(filesAtPath('/tmp/')) == 'string';", true);
	EidosAssertScriptSuccess("sum(filesAtPath('/') == 'bin');", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess("sum(filesAtPath('/', T) == '/bin');", gStaticEidosValue_Integer1);
	#endif
	EidosAssertScriptSuccess_NULL("filesAtPath('foo_is_a_bad_path');");
	
	// writeFile()
	EidosAssertScriptSuccess_L("writeFile('" + temp_path + "/EidosTest.txt', c(paste(0:4), paste(5:9)));", true);
	
	// readFile() – note that the readFile() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess_LV("readFile('" + temp_path + "/EidosTest.txt') == c(paste(0:4), paste(5:9));", {true, true});
	EidosAssertScriptSuccess_L("all(asInteger(strsplit(paste(readFile('" + temp_path + "/EidosTest.txt')))) == 0:9);", true);
	EidosAssertScriptSuccess_NULL("readFile('foo_is_a_bad_path.txt');");
	
	// writeFile() with append
	EidosAssertScriptSuccess_L("writeFile('" + temp_path + "/EidosTest.txt', 'foo', T);", true);
	
	// readFile() – note that the readFile() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess_LV("readFile('" + temp_path + "/EidosTest.txt') == c(paste(0:4), paste(5:9), 'foo');", {true, true, true});
	
	// fileExists() – note that the fileExists() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess_L("fileExists('" + temp_path + "/EidosTest.txt');", true);
	
	// deleteFile() – note that the deleteFile() tests depend on the previous writeFile() test
	EidosAssertScriptSuccess_L("deleteFile('" + temp_path + "/EidosTest.txt');", true);
	EidosAssertScriptSuccess_L("deleteFile('" + temp_path + "/EidosTest.txt');", false);
	
	// fileExists() – note that the fileExists() tests depend on the previous writeFile() and deleteFile() tests
	EidosAssertScriptSuccess_L("fileExists('" + temp_path + "/EidosTest.txt');", false);
	
	// tempdir() - we don't try to write to it, we just call it
	EidosAssertScriptSuccess_L("d = tempdir(); length(d) > 0;", true);
	
	// writeTempFile()
	EidosAssertScriptRaise("file = writeTempFile('eidos_test_~', '.txt', '');", 7, "may not contain");
	EidosAssertScriptRaise("file = writeTempFile('eidos_test_/', '.txt', '');", 7, "may not contain");
	EidosAssertScriptRaise("file = writeTempFile('eidos_test_', 'foo~.txt', '');", 7, "may not contain");
	EidosAssertScriptRaise("file = writeTempFile('eidos_test_', 'foo/.txt', '');", 7, "may not contain");
	EidosAssertScriptSuccess_L("file = writeTempFile('eidos_test_', '.txt', ''); identical(readFile(file), string(0));", true);
	EidosAssertScriptSuccess_L("file = writeTempFile('eidos_test_', '.txt', 'foo'); identical(readFile(file), 'foo');", true);
	EidosAssertScriptSuccess_L("file = writeTempFile('eidos_test_', '.txt', c(paste(0:4), paste(5:9))); identical(readFile(file), c('0 1 2 3 4', '5 6 7 8 9'));", true);
	
	// writeFile() and writeTempFile() with compression – we don't decompress to verify, but we check for success and file existence
	EidosAssertScriptSuccess_L("writeFile('" + temp_path + "/EidosTest.txt', c(paste(0:4), paste(5:9)), compress=T);", true);
	EidosAssertScriptSuccess_L("fileExists('" + temp_path + "/EidosTest.txt.gz');", true);
	EidosAssertScriptSuccess_L("file = writeTempFile('eidos_test_', '.txt', 'foo'); fileExists(file);", true);
	
	// createDirectory() – we rely on writeTempFile() to give us a file path that isn't in use, from which we derive a directory path that also shouldn't be in use
	EidosAssertScriptSuccess_L("file = writeTempFile('eidos_test_dir', '.txt', ''); dir = substr(file, 0, nchar(file) - 5); createDirectory(dir);", true);
	
	// getwd() / setwd()
	EidosAssertScriptSuccess_L("path1 = getwd(); path2 = setwd(path1); path1 == path2;", true);
}

#pragma mark color manipulation
void _RunColorManipulationTests(void)
{
	// cmColors()
	EidosAssertScriptRaise("cmColors(-1);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptRaise("cmColors(10000000);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptSuccess("cmColors(0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess_SV("cmColors(1);", {"#80FFFF"});
	EidosAssertScriptSuccess_SV("cmColors(2);", {"#80FFFF", "#FF80FF"});
	EidosAssertScriptSuccess_SV("cmColors(3);", {"#80FFFF", "#FFFFFF", "#FF80FF"});
	EidosAssertScriptSuccess_SV("cmColors(4);", {"#80FFFF", "#D4FFFF", "#FFD5FF", "#FF80FF"});
	EidosAssertScriptSuccess_SV("cmColors(7);", {"#80FFFF", "#AAFFFF", "#D4FFFF", "#FFFFFF", "#FFD5FF", "#FFAAFF", "#FF80FF"});
	
	// colors() (only palettes 'cm', 'heat', and 'terrain' get checked for their specific values)
	EidosAssertScriptRaise("colors(-1, 'cm');", 0, "requires 0 <= x <= 100000");
	EidosAssertScriptRaise("colors(10000000, 'cm');", 0, "requires 0 <= x <= 100000");
	EidosAssertScriptRaise("colors(5, 'foo');", 0, "unrecognized color palette name");
	EidosAssertScriptRaise("colors(c(0, 1), 'cm');", 0, "to be singleton");
	EidosAssertScriptSuccess("colors(0, 'cm');", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess_SV("colors(1, 'cm');", {"#80FFFF"});
	EidosAssertScriptSuccess_SV("colors(2, 'cm');", {"#80FFFF", "#FF80FF"});
	EidosAssertScriptSuccess_SV("colors(3, 'cm');", {"#80FFFF", "#FFFFFF", "#FF80FF"});
	EidosAssertScriptSuccess_SV("colors(4, 'cm');", {"#80FFFF", "#D4FFFF", "#FFD5FF", "#FF80FF"});
	EidosAssertScriptSuccess_SV("colors(7, 'cm');", {"#80FFFF", "#AAFFFF", "#D4FFFF", "#FFFFFF", "#FFD5FF", "#FFAAFF", "#FF80FF"});
	EidosAssertScriptSuccess_SV("colors(0.0, 'cm');", {"#80FFFF"});
	EidosAssertScriptSuccess_SV("colors(-100.0, 'cm');", {"#80FFFF"});
	EidosAssertScriptSuccess_SV("colors(1.0, 'cm');", {"#FF80FF"});
	EidosAssertScriptSuccess_SV("colors(100.0, 'cm');", {"#FF80FF"});
	EidosAssertScriptSuccess_SV("colors(c(0.0,0.5,1.0), 'cm');", {"#80FFFF", "#FFFFFF", "#FF80FF"});
	EidosAssertScriptSuccess_SV("colors(c(0.5,1.0,0.0), 'cm');", {"#FFFFFF", "#FF80FF", "#80FFFF"});
	EidosAssertScriptSuccess_SV("colors(1, 'heat');", {"#FF0000"});
	EidosAssertScriptSuccess_SV("colors(5, 'heat');", {"#FF0000", "#FF5500", "#FFAA00", "#FFFF00", "#FFFFFF"});
	EidosAssertScriptSuccess_SV("colors(1, 'terrain');", {"#00A600"});
	EidosAssertScriptSuccess_SV("colors(5, 'terrain');", {"#00A600", "#63C600", "#E6E600", "#ECB176", "#F2F2F2"});
	EidosAssertScriptSuccess_L("colors(5, 'parula'); T;", true);
	EidosAssertScriptSuccess_L("colors(5, 'hot'); T;", true);
	EidosAssertScriptSuccess_L("colors(5, 'jet'); T;", true);
	EidosAssertScriptSuccess_L("colors(5, 'turbo'); T;", true);
	EidosAssertScriptSuccess_L("colors(5, 'gray'); T;", true);
	EidosAssertScriptSuccess_L("colors(5, 'magma'); T;", true);
	EidosAssertScriptSuccess_L("colors(5, 'inferno'); T;", true);
	EidosAssertScriptSuccess_L("colors(5, 'plasma'); T;", true);
	EidosAssertScriptSuccess_L("colors(5, 'viridis'); T;", true);
	EidosAssertScriptSuccess_L("colors(5, 'cividis'); T;", true);
	
	// heatColors()
	EidosAssertScriptRaise("heatColors(-1);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptRaise("heatColors(10000000);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptSuccess("heatColors(0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess_SV("heatColors(1);", {"#FF0000"});
	EidosAssertScriptSuccess_SV("heatColors(2);", {"#FF0000", "#FFFFFF"});
	EidosAssertScriptSuccess_SV("heatColors(5);", {"#FF0000", "#FF5500", "#FFAA00", "#FFFF00", "#FFFFFF"});
	
	// terrainColors()
	EidosAssertScriptRaise("terrainColors(-1);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptRaise("terrainColors(10000000);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptSuccess("terrainColors(0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess_SV("terrainColors(1);", {"#00A600"});
	EidosAssertScriptSuccess_SV("terrainColors(2);", {"#00A600", "#F2F2F2"});
	EidosAssertScriptSuccess_SV("terrainColors(5);", {"#00A600", "#63C600", "#E6E600", "#ECB176", "#F2F2F2"});
	
	// rainbow()
	EidosAssertScriptRaise("rainbow(-1);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptRaise("rainbow(10000000);", 0, "requires 0 <= n <= 100000");
	EidosAssertScriptSuccess("rainbow(0);", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess_SV("rainbow(1);", {"#FF0000"});
	EidosAssertScriptSuccess_SV("rainbow(2);", {"#FF0000", "#00FFFF"});
	EidosAssertScriptSuccess_SV("rainbow(3);", {"#FF0000", "#00FF00", "#0000FF"});
	EidosAssertScriptSuccess_SV("rainbow(4);", {"#FF0000", "#80FF00", "#00FFFF", "#8000FF"});
	EidosAssertScriptSuccess_SV("rainbow(12);", {"#FF0000", "#FF8000", "#FFFF00", "#80FF00", "#00FF00", "#00FF80", "#00FFFF", "#0080FF", "#0000FF", "#8000FF", "#FF00FF", "#FF0080"});
	EidosAssertScriptSuccess_SV("rainbow(6, s=0.5);", {"#FF8080", "#FFFF80", "#80FF80", "#80FFFF", "#8080FF", "#FF80FF"});
	EidosAssertScriptSuccess_SV("rainbow(6, v=0.5);", {"#800000", "#808000", "#008000", "#008080", "#000080", "#800080"});
	EidosAssertScriptSuccess_SV("rainbow(6, s=0.5, v=0.5);", {"#804040", "#808040", "#408040", "#408080", "#404080", "#804080"});
	EidosAssertScriptSuccess_SV("rainbow(4, start=1.0/6, end=4.0/6, ccw=T);", {"#FFFF00", "#00FF00", "#00FFFF", "#0000FF"});
	EidosAssertScriptSuccess_SV("rainbow(4, start=1.0/6, end=4.0/6, ccw=F);", {"#FFFF00", "#FF0000", "#FF00FF", "#0000FF"});
	EidosAssertScriptSuccess_SV("rainbow(4, start=4.0/6, end=1.0/6, ccw=T);", {"#0000FF", "#FF00FF", "#FF0000", "#FFFF00"});
	EidosAssertScriptSuccess_SV("rainbow(4, start=4.0/6, end=1.0/6, ccw=F);", {"#0000FF", "#00FFFF", "#00FF00", "#FFFF00"});
	EidosAssertScriptRaise("rainbow(4, start=NAN, end=1.0/6, ccw=F);", 0, "color component with value NAN");
	EidosAssertScriptRaise("rainbow(4, start=4.0/6, end=NAN, ccw=F);", 0, "color component with value NAN");
	
	// hsv2rgb()
	EidosAssertScriptRaise("hsv2rgb(c(0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("hsv2rgb(c(0.0, 0.0, 0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("hsv2rgb(c(NAN, 0.0, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("hsv2rgb(c(0.0, NAN, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("hsv2rgb(c(0.0, 0.0, NAN));", 0, "color component with value NAN");
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(0.0, 0.0, -0.5)), c(0.0, 0.0, 0.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(0.0, 0.0, 0.0)), c(0.0, 0.0, 0.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(0.0, 0.0, 0.5)), c(0.5, 0.5, 0.5));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(0.0, 0.0, 1.0)), c(1.0, 1.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(0.0, 0.0, 1.5)), c(1.0, 1.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(0.0, -0.5, 1.0)), c(1.0, 1.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(0.0, 0.25, 1.0)), c(1.0, 0.75, 0.75));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(0.0, 0.5, 1.0)), c(1.0, 0.5, 0.5));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(0.0, 0.75, 1.0)), c(1.0, 0.25, 0.25));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(0.0, 1.0, 1.0)), c(1.0, 0.0, 0.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(0.0, 1.5, 1.0)), c(1.0, 0.0, 0.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(-0.5, 1.0, 1.0)), c(1.0, 0.0, 0.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(1/6, 1.0, 1.0)), c(1.0, 1.0, 0.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(2/6, 1.0, 1.0)), c(0.0, 1.0, 0.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(3/6, 1.0, 1.0)), c(0.0, 1.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(4/6, 1.0, 1.0)), c(0.0, 0.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(5/6, 1.0, 1.0)), c(1.0, 0.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(6/6, 1.0, 1.0)), c(1.0, 0.0, 0.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(c(7/6, 1.0, 1.0)), c(1.0, 0.0, 0.0));", true);
	EidosAssertScriptSuccess_L("identical(hsv2rgb(matrix(c(1/6, 1.0, 1.0, 0.0, 0.25, 1.0), ncol=3, byrow=T)), matrix(c(1.0, 1.0, 0.0, 1.0, 0.75, 0.75), ncol=3, byrow=T));", true);
	
	// rgb2hsv()
	EidosAssertScriptRaise("rgb2hsv(c(0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2hsv(c(0.0, 0.0, 0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2hsv(c(NAN, 0.0, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("rgb2hsv(c(0.0, NAN, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("rgb2hsv(c(0.0, 0.0, NAN));", 0, "color component with value NAN");
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(-1.0, 0.0, 0.0)), c(0.0, 0.0, 0.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(0.0, -1.0, 0.0)), c(0.0, 0.0, 0.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(0.0, 0.0, -1.0)), c(0.0, 0.0, 0.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(0.0, 0.0, 0.0)), c(0.0, 0.0, 0.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(0.5, 0.5, 0.5)), c(0.0, 0.0, 0.5));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(1.0, 1.0, 1.0)), c(0.0, 0.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(1.5, 1.0, 1.0)), c(0.0, 0.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(1.0, 1.5, 1.0)), c(0.0, 0.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(1.0, 1.0, 1.5)), c(0.0, 0.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(1.0, 0.75, 0.75)), c(0.0, 0.25, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(1.0, 0.5, 0.5)), c(0.0, 0.5, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(1.0, 0.25, 0.25)), c(0.0, 0.75, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(1.0, 0.0, 0.0)), c(0.0, 1.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(1.0, 1.0, 0.0)), c(1/6, 1.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(0.0, 1.0, 0.0)), c(2/6, 1.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(0.0, 1.0, 1.0)), c(3/6, 1.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(0.0, 0.0, 1.0)), c(4/6, 1.0, 1.0));", true);
	EidosAssertScriptSuccess_L("sum(abs(rgb2hsv(c(1.0, 0.0, 1.0)) - c(5/6, 1.0, 1.0))) < 1e-7;", true);	// roundoff with 5/6
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(1.5, -0.5, 0.0)), c(0.0, 1.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(0.0, 1.5, -0.5)), c(2/6, 1.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(c(-0.5, 0.0, 1.5)), c(4/6, 1.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(rgb2hsv(matrix(c(1.0, 1.0, 0.0, 1.0, 0.75, 0.75), ncol=3, byrow=T)), matrix(c(1/6, 1.0, 1.0, 0.0, 0.25, 1.0), ncol=3, byrow=T));", true);
	
	// rgb2color()
	EidosAssertScriptRaise("rgb2color(c(0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2color(c(0.0, 0.0, 0.0, 0.0));", 0, "must contain exactly three");
	EidosAssertScriptRaise("rgb2color(c(NAN, 0.0, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("rgb2color(c(0.0, NAN, 0.0));", 0, "color component with value NAN");
	EidosAssertScriptRaise("rgb2color(c(0.0, 0.0, NAN));", 0, "color component with value NAN");
	EidosAssertScriptSuccess_L("rgb2color(c(-0.5, -0.5, -0.5)) == '#000000';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.0, 0.0, 0.0)) == '#000000';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(1.0, 1.0, 1.0)) == '#FFFFFF';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(1.5, 1.5, 1.5)) == '#FFFFFF';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(1.0, 0.0, 0.0)) == '#FF0000';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.0, 1.0, 0.0)) == '#00FF00';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.0, 0.0, 1.0)) == '#0000FF';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.25, 0.0, 0.0)) == '#400000';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.0, 0.25, 0.0)) == '#004000';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.0, 0.0, 0.25)) == '#000040';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.5, 0.0, 0.0)) == '#800000';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.0, 0.5, 0.0)) == '#008000';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.0, 0.0, 0.5)) == '#000080';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.75, 0.0, 0.0)) == '#BF0000';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.0, 0.75, 0.0)) == '#00BF00';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.0, 0.0, 0.75)) == '#0000BF';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(1.0, 0.0, 0.0)) == '#FF0000';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.0, 1.0, 0.0)) == '#00FF00';", true);
	EidosAssertScriptSuccess_L("rgb2color(c(0.0, 0.0, 1.0)) == '#0000FF';", true);
	EidosAssertScriptSuccess_L("identical(rgb2color(matrix(c(0.25, 0.0, 0.0, 0.0, 0.75, 0.0, 0.0, 0.0, 1.0), ncol=3, byrow=T)), c('#400000', '#00BF00', '#0000FF'));", true);
	
	// color2rgb()
	EidosAssertScriptRaise("identical(color2rgb('foo'), c(0.0, 0.0, 0.0));", 10, "could not be found");
	EidosAssertScriptRaise("identical(color2rgb('#00000'), c(0.0, 0.0, 0.0));", 10, "could not be found");
	EidosAssertScriptRaise("identical(color2rgb('#0000000'), c(0.0, 0.0, 0.0));", 10, "could not be found");
	EidosAssertScriptRaise("identical(color2rgb('#0000g0'), c(0.0, 0.0, 0.0));", 10, "is malformed");
	EidosAssertScriptSuccess_L("identical(color2rgb('white'), c(1.0, 1.0, 1.0));", true);
	EidosAssertScriptSuccess_L("identical(color2rgb(c('#000000', 'red', 'green', 'blue', '#FFFFFF')), matrix(c(0.0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1), ncol=3, byrow=T));", true);
	EidosAssertScriptSuccess_L("sum(abs(color2rgb('chocolate1') - c(1.0, 127/255, 36/255))) < 1e-7;", true);
	EidosAssertScriptSuccess_L("sum(abs(color2rgb('#000000') - c(0.0, 0.0, 0.0))) < 1e-7;", true);
	EidosAssertScriptSuccess_L("sum(abs(color2rgb('#7F0000') - c(127/255, 0.0, 0.0))) < 1e-7;", true);
	EidosAssertScriptSuccess_L("sum(abs(color2rgb('#FF0000') - c(1.0, 0.0, 0.0))) < 1e-7;", true);
	EidosAssertScriptSuccess_L("sum(abs(color2rgb('#007F00') - c(0.0, 127/255, 0.0))) < 1e-7;", true);
	EidosAssertScriptSuccess_L("sum(abs(color2rgb('#00FF00') - c(0.0, 1.0, 0.0))) < 1e-7;", true);
	EidosAssertScriptSuccess_L("sum(abs(color2rgb('#00007F') - c(0.0, 0.0, 127/255))) < 1e-7;", true);
	EidosAssertScriptSuccess_L("sum(abs(color2rgb('#0000FF') - c(0.0, 0.0, 1.0))) < 1e-7;", true);
	EidosAssertScriptSuccess_L("sum(abs(color2rgb('#0000ff') - c(0.0, 0.0, 1.0))) < 1e-7;", true);
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
	
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'sum(applyValue);'), c(9,12));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'sum(applyValue);'), c(3,7,11));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'sum(applyValue);'), matrix(1:6, nrow=2));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'sum(applyValue);'), t(matrix(1:6, nrow=2)));", true);
	
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'applyValue^2;'), matrix(c(1.0,9,25,4,16,36), nrow=3));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'applyValue^2;'), matrix(c(1.0,4,9,16,25,36), nrow=2));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'applyValue^2;'), matrix(c(1.0,4,9,16,25,36), nrow=2));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'applyValue^2;'), t(matrix(c(1.0,4,9,16,25,36), nrow=2)));", true);
	
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'c(applyValue, applyValue^2);'), matrix(c(1.0,3,5,1,9,25,2,4,6,4,16,36), ncol=2));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'c(applyValue, applyValue^2);'), matrix(c(1.0,2,1,4,3,4,9,16,5,6,25,36), ncol=3));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'c(applyValue, applyValue^2);'), array(c(1.0,1,2,4,3,9,4,16,5,25,6,36), c(2,2,3)));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'c(applyValue, applyValue^2);'), array(c(1.0,1,3,9,5,25,2,4,4,16,6,36), c(2,3,2)));", true);
	
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'if (applyValue[0] % 2) sum(applyValue); else NULL;'), 9);", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'if (applyValue[0] % 3) sum(applyValue); else NULL;'), c(3,11));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'if (applyValue[0] % 2) sum(applyValue); else NULL;'), c(1,3,5));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'if (applyValue[0] % 2) sum(applyValue); else NULL;'), c(1,3,5));", true);
	
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'if (applyValue[0] % 2) applyValue^2; else NULL;'), c(1.0,9,25));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'if (applyValue[0] % 3) applyValue^2; else NULL;'), c(1.0,4,25,36));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'if (applyValue[0] % 2) applyValue^2; else NULL;'), c(1.0,9,25));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'if (applyValue[0] % 2) applyValue^2; else NULL;'), c(1.0,9,25));", true);
	
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, 0, 'if (applyValue[0] % 2) c(applyValue, applyValue^2); else NULL;'), c(1.0,3,5,1,9,25));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, 1, 'if (applyValue[0] % 3) c(applyValue, applyValue^2); else NULL;'), c(1.0,2,1,4,5,6,25,36));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, c(0,1), 'if (applyValue[0] % 2) c(applyValue, applyValue^2); else NULL;'), c(1.0,1,3,9,5,25));", true);
	EidosAssertScriptSuccess_L("x=matrix(1:6, nrow=2); identical(apply(x, c(1,0), 'if (applyValue[0] % 2) c(applyValue, applyValue^2); else NULL;'), c(1.0,1,3,9,5,25));", true);
	
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, 0, 'sum(applyValue);'), c(36,42));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, 1, 'sum(applyValue);'), c(18,26,34));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, 2, 'sum(applyValue);'), c(21,57));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,1), 'sum(applyValue);'), matrix(c(8,10,12,14,16,18), nrow=2));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, c(1,2), 'sum(applyValue);'), matrix(c(3,7,11,15,19,23), nrow=3));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,2), 'sum(applyValue);'), matrix(c(9,12,27,30), nrow=2));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,1,2), 'sum(applyValue);'), array(1:12, c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, c(2,1,0), 'sum(applyValue);'), array(c(1,7,3,9,5,11,2,8,4,10,6,12), c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, c(2,0,1), 'sum(applyValue);'), array(c(1,7,2,8,3,9,4,10,5,11,6,12), c(2,2,3)));", true);
	
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, 0, 'applyValue^2;'), matrix(c(1.0,9,25,49,81,121,4,16,36,64,100,144), ncol=2));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, 1, 'applyValue^2;'), matrix(c(1.0,4,49,64,9,16,81,100,25,36,121,144), ncol=3));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, 2, 'applyValue^2;'), matrix(c(1.0,4,9,16,25,36,49,64,81,100,121,144), ncol=2));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,1), 'applyValue^2;'), array(c(1.0,49,4,64,9,81,16,100,25,121,36,144), c(2,2,3)));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, c(1,2), 'applyValue^2;'), array(c(1.0,4,9,16,25,36,49,64,81,100,121,144), c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,2), 'applyValue^2;'), array(c(1.0,9,25,4,16,36,49,81,121,64,100,144), c(3,2,2)));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, c(0,1,2), 'applyValue^2;'), array((1.0:12)^2, c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, c(2,1,0), 'applyValue^2;'), array(c(1.0,49,9,81,25,121,4,64,16,100,36,144), c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("y = array(1:12, c(2,3,2)); identical(apply(y, c(2,0,1), 'applyValue^2;'), array(c(1.0,49,4,64,9,81,16,100,25,121,36,144), c(2,2,3)));", true);
	
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, 0, 'sum(applyValue);'), c(144,156));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, 1, 'sum(applyValue);'), c(84,100,116));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, 2, 'sum(applyValue);'), c(114,186));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, 3, 'sum(applyValue);'), c(78,222));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(0,1), 'sum(applyValue);'), matrix(c(40,44,48,52,56,60), nrow=2));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(0,2), 'sum(applyValue);'), matrix(c(54,60,90,96), nrow=2));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(0,3), 'sum(applyValue);'), matrix(c(36,42,108,114), nrow=2));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(1,0), 'sum(applyValue);'), matrix(c(40,48,56,44,52,60), nrow=3));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(1,2), 'sum(applyValue);'), matrix(c(30,38,46,54,62,70), nrow=3));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(1,3), 'sum(applyValue);'), matrix(c(18,26,34,66,74,82), nrow=3));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(2,0), 'sum(applyValue);'), matrix(c(54,90,60,96), nrow=2));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(2,1), 'sum(applyValue);'), matrix(c(30,54,38,62,46,70), nrow=2));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(2,3), 'sum(applyValue);'), matrix(c(21,57,93,129), nrow=2));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(3,0), 'sum(applyValue);'), matrix(c(36,108,42,114), nrow=2));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(3,1), 'sum(applyValue);'), matrix(c(18,66,26,74,34,82), nrow=2));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(3,2), 'sum(applyValue);'), matrix(c(21,93,57,129), nrow=2));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(0,1,2), 'sum(applyValue);'), array(c(14,16,18,20,22,24,26,28,30,32,34,36), c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(3,1,0), 'sum(applyValue);'), array(c(8,32,12,36,16,40,10,34,14,38,18,42), c(2,3,2)));", true);
	EidosAssertScriptSuccess_L("z = array(1:24, c(2,3,2,2)); identical(apply(z, c(2,3,0,1), 'sum(applyValue);'), array(c(1,7,13,19,2,8,14,20,3,9,15,21,4,10,16,22,5,11,17,23,6,12,18,24), c(2,2,2,3)));", true);
	
	// sapply()
	EidosAssertScriptSuccess_NULL("x=integer(0); sapply(x, 'applyValue^2;');");
	EidosAssertScriptSuccess_FV("x=1:5; sapply(x, 'applyValue^2;');", {1, 4, 9, 16, 25});
	EidosAssertScriptSuccess_IV("x=1:5; sapply(x, 'product(1:applyValue);');", {1, 2, 6, 24, 120});
	EidosAssertScriptSuccess_SV("x=1:3; sapply(x, \"rep(''+applyValue, applyValue);\");", {"1", "2", "2", "3", "3", "3"});
	EidosAssertScriptSuccess_SV("x=1:5; sapply(x, \"paste(rep(''+applyValue, applyValue), sep='');\");", {"1", "22", "333", "4444", "55555"});
	EidosAssertScriptSuccess_IV("x=1:10; sapply(x, 'if (applyValue % 2) applyValue; else NULL;');", {1, 3, 5, 7, 9});
	EidosAssertScriptSuccess_I("x=1:5; sapply(x, 'y=applyValue; NULL;'); y;", 5);
	EidosAssertScriptSuccess_IV("x=1:5; sapply(x, 'y=applyValue; y;');", {1, 2, 3, 4, 5});
	EidosAssertScriptSuccess_F("x=2; for (i in 1:2) x=sapply(x, 'applyValue^2;'); x;", 16.0);
	EidosAssertScriptRaise("x=2; sapply(x, 'semanticError;');", 5, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; sapply(x, y);", 25, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; y='semanticError;'; sapply(x, y[T]);", 25, "undefined identifier semanticError");
	EidosAssertScriptRaise("x=2; sapply(x, 'syntax Error;');", 5, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; sapply(x, y);", 24, "unexpected token '@Error'");
	EidosAssertScriptRaise("x=2; y='syntax Error;'; sapply(x, y[T]);", 24, "unexpected token '@Error'");
	EidosAssertScriptSuccess_I("x=2; y='x;'; sapply(x, y[T]);", 2);
	
	EidosAssertScriptSuccess_L("identical(sapply(1:6, 'integer(0);'), integer(0));", true);
	EidosAssertScriptSuccess_L("identical(sapply(1:6, 'integer(0);', simplify='vector'), integer(0));", true);
	EidosAssertScriptSuccess_L("identical(sapply(1:6, 'integer(0);', simplify='matrix'), integer(0));", true);
	EidosAssertScriptRaise("identical(sapply(1:6, 'integer(0);', simplify='match'), 2:7);", 10, "not all singletons");
	EidosAssertScriptRaise("identical(sapply(1:6, 'integer(0);', simplify='foo'), integer(0));", 10, "unrecognized simplify option");
	EidosAssertScriptRaise("identical(sapply(1:6, 'setSeed(5);'), integer(0));", 10, "must return a non-void value");
	
	EidosAssertScriptSuccess_L("identical(sapply(1:6, 'applyValue+1;'), 2:7);", true);
	EidosAssertScriptSuccess_L("identical(sapply(1:6, 'applyValue+1;', simplify='vector'), 2:7);", true);
	EidosAssertScriptSuccess_L("identical(sapply(1:6, 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(sapply(1:6, 'applyValue+1;', simplify='match'), 2:7);", true);
	
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, nrow=1), 'applyValue+1;'), 2:7);", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, nrow=1), 'applyValue+1;', simplify='vector'), 2:7);", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, nrow=1), 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, nrow=1), 'applyValue+1;', simplify='match'), matrix(2:7, nrow=1));", true);
	
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=1), 'applyValue+1;'), 2:7);", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=1), 'applyValue+1;', simplify='vector'), 2:7);", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=1), 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=1), 'applyValue+1;', simplify='match'), matrix(2:7, ncol=1));", true);
	
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=2), 'applyValue+1;'), 2:7);", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=2), 'applyValue+1;', simplify='vector'), 2:7);", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=2), 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=2), 'applyValue+1;', simplify='match'), matrix(2:7, ncol=2));", true);
	
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=2), 'c(applyValue, applyValue+1);'), c(1,2,2,3,3,4,4,5,5,6,6,7));", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=2), 'c(applyValue, applyValue+1);', simplify='vector'), c(1,2,2,3,3,4,4,5,5,6,6,7));", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=2), 'c(applyValue, applyValue+1);', simplify='matrix'), matrix(c(1,2,2,3,3,4,4,5,5,6,6,7), nrow=2));", true);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, ncol=2), 'c(applyValue, applyValue+1);', simplify='match'), c(1,2,2,3,3,4,4,5,5,6,6,7));", 10, "not all singletons");
	
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'applyValue+1;'), 2:7);", true);
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'applyValue+1;', simplify='vector'), 2:7);", true);
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'applyValue+1;', simplify='matrix'), matrix(2:7, nrow=1));", true);
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'applyValue+1;', simplify='match'), array(2:7, c(2,1,3)));", true);
	
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'c(applyValue, applyValue+1);'), c(1,2,2,3,3,4,4,5,5,6,6,7));", true);
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'c(applyValue, applyValue+1);', simplify='vector'), c(1,2,2,3,3,4,4,5,5,6,6,7));", true);
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'c(applyValue, applyValue+1);', simplify='matrix'), matrix(c(1,2,2,3,3,4,4,5,5,6,6,7), nrow=2));", true);
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'c(applyValue, applyValue+1);', simplify='match'), c(1,2,2,3,3,4,4,5,5,6,6,7));", 10, "not all singletons");
	
	EidosAssertScriptSuccess_L("identical(sapply(1:6, 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", true);
	EidosAssertScriptSuccess_L("identical(sapply(1:6, 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", true);
	EidosAssertScriptSuccess_L("identical(sapply(1:6, 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", true);
	EidosAssertScriptRaise("identical(sapply(1:6, 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), c(2,4,6));", 10, "included NULL");
	
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, nrow=1), 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, nrow=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, nrow=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", true);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, nrow=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), matrix(c(2,4,6), nrow=1));", 10, "included NULL");
	
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=1), 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", true);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, ncol=1), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), matrix(c(2,4,6), ncol=1));", 10, "included NULL");
	
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", true);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), matrix(c(2,4,6), ncol=2));", 10, "included NULL");
	
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;'), c(1,3,3,5,5,7));", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='vector'), c(1,3,3,5,5,7));", true);
	EidosAssertScriptSuccess_L("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='matrix'), matrix(c(1,3,3,5,5,7), nrow=2));", true);
	EidosAssertScriptRaise("identical(sapply(matrix(1:6, ncol=2), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='match'), c(1,3,3,5,5,7));", 10, "included NULL");
	
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) applyValue+1; else NULL;'), c(2,4,6));", true);
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='vector'), c(2,4,6));", true);
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='matrix'), matrix(c(2,4,6), nrow=1));", true);
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) applyValue+1; else NULL;', simplify='match'), array(c(2,4,6), c(2,1,3)));", 10, "included NULL");
	
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;'), c(1,3,3,5,5,7));", true);
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='vector'), c(1,3,3,5,5,7));", true);
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='matrix'), matrix(c(1,3,3,5,5,7), nrow=2));", true);
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else NULL;', simplify='match'), c(1,3,3,5,5,7));", 10, "included NULL");
	
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else applyValue;'), c(1,3,2,3,5,4,5,7,6));", true);
	EidosAssertScriptSuccess_L("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else applyValue;', simplify='vector'), c(1,3,2,3,5,4,5,7,6));", true);
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else applyValue;', simplify='matrix'), matrix(c(1,3,2,3,5,4,5,7,6), nrow=2));", 10, "not of a consistent length");
	EidosAssertScriptRaise("identical(sapply(array(1:6, c(2,1,3)), 'if (applyValue % 2) c(applyValue, applyValue+2); else applyValue;', simplify='match'), c(1,3,2,3,5,4,5,7,6));", 10, "not all singletons");
}

void _RunFunctionMiscTests(const std::string &temp_path)
{
	// assert()
	EidosAssertScriptRaise("assert();", 0, "missing required argument assertions");
	EidosAssertScriptSuccess_VOID("assert(T);");
	EidosAssertScriptRaise("assert(F);", 0, "assertion failed");
	EidosAssertScriptSuccess_VOID("assert(c(T, T, T, T, T));");
	EidosAssertScriptRaise("assert(c(F, F, F, T, F));", 0, "assertion failed");
	EidosAssertScriptRaise("assert(c(F, F, F, F, F));", 0, "assertion failed");
	EidosAssertScriptSuccess_VOID("assert(T, 'foo bar!');");
	EidosAssertScriptRaise("assert(F, 'foo bar!');", 0, "foo bar!");

	// beep() – this is commented out by default since it would confuse people if the Eidos self-test beeped...
	//EidosAssertScriptSuccess_NULL("beep();");
	//EidosAssertScriptSuccess_NULL("beep('Submarine');");
	
	// citation()
	EidosAssertScriptSuccess_VOID("citation();");
	EidosAssertScriptRaise("citation(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("citation(_Test(7));", 0, "too many arguments supplied");
	
	// clock()
	EidosAssertScriptSuccess_L("c = clock(); isFloat(c);", true);
	EidosAssertScriptSuccess_L("c = clock('cpu'); isFloat(c);", true);
	EidosAssertScriptSuccess_L("c = clock('mono'); isFloat(c);", true);
	EidosAssertScriptRaise("clock('foo');", 0, "unrecognized clock type");
	
	// date()
	EidosAssertScriptSuccess_I("size(strsplit(date(), '-'));", 3);
	EidosAssertScriptRaise("date(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("date(_Test(7));", 0, "too many arguments supplied");
	
	// defineConstant()
	EidosAssertScriptSuccess_I("defineConstant('foo', 5:10); sum(foo);", 45);
	EidosAssertScriptRaise("defineConstant('T', 5:10);", 0, "is already defined");
	EidosAssertScriptRaise("defineConstant('foo', 5:10); defineConstant('foo', 5:10); sum(foo);", 29, "is already defined");
	EidosAssertScriptRaise("foo = 5:10; defineConstant('foo', 5:10); sum(foo);", 12, "is already defined");
	EidosAssertScriptRaise("defineConstant('foo', 5:10); rm('foo');", 29, "cannot be removed");
	EidosAssertScriptSuccess_I("defineConstant('foo', _Test(5)); foo._yolk;", 5);
	EidosAssertScriptRaise("defineConstant('foo', _TestNRR(5)); foo._yolk;", 0, "retain/release");	// leaks due to _TestNRR, not a bug
	
	// defineGlobal()
	EidosAssertScriptSuccess_I("defineGlobal('foo', 5:10); sum(foo);", 45);
	EidosAssertScriptRaise("defineGlobal('T', 5:10);", 0, "cannot be redefined");
	EidosAssertScriptSuccess_I("defineGlobal('foo', 5:11); defineGlobal('foo', 5:10); sum(foo);", 45);
	EidosAssertScriptSuccess_I("foo = 5:11; defineGlobal('foo', 5:10); sum(foo);", 45);		// we're in the global namespace anyway
	EidosAssertScriptRaise("defineGlobal('foo', 5:10); rm('foo'); sum(foo);", 42, "undefined identifier");
	EidosAssertScriptSuccess_I("defineGlobal('foo', _Test(5)); foo._yolk;", 5);
	EidosAssertScriptRaise("defineGlobal('foo', _TestNRR(5)); foo._yolk;", 0, "retain/release");	// leaks due to _TestNRR, not a bug
	
	// doCall()
	EidosAssertScriptSuccess_L("abs(doCall('sin', 0.0) - 0) < 0.000001;", true);
	EidosAssertScriptSuccess_L("abs(doCall('sin', PI/2) - 1) < 0.000001;", true);
	EidosAssertScriptRaise("doCall('sin');", 0, "requires 1 argument(s), but 0 are supplied");
	EidosAssertScriptRaise("doCall('sin', 'bar');", 0, "cannot be type string");
	EidosAssertScriptRaise("doCall('sin', 0, 1);", 0, "requires at most 1 argument");
	EidosAssertScriptRaise("doCall('si', 0, 1);", 0, "unrecognized function name");
	
	// executeLambda()
	EidosAssertScriptSuccess_F("x=7; executeLambda('x^2;');", 49);
	EidosAssertScriptRaise("x=7; executeLambda('x^2');", 5, "unexpected token");
	EidosAssertScriptRaise("x=7; executeLambda(c('x^2;', '5;'));", 5, "must be a singleton");
	EidosAssertScriptRaise("x=7; executeLambda(string(0));", 5, "must be a singleton");
	EidosAssertScriptSuccess_F("x=7; executeLambda('x=x^2+4;'); x;", 53);
	EidosAssertScriptSuccess_F("x=7; executeLambda('x=x^2+4;', timed=T); x;", 53);
	EidosAssertScriptSuccess_F("x=7; executeLambda('x=x^2+4;', timed='cpu'); x;", 53);
	EidosAssertScriptSuccess_F("x=7; executeLambda('x=x^2+4;', timed='mono'); x;", 53);
	//EidosAssertScriptRaise("x=7; executeLambda('x=x^2+4;', timed='foo'); x;", 5, "clock type");	// FIXME raise doesn't come through correctly!
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
	EidosAssertScriptSuccess_F("x=2; for (i in 1:2) executeLambda('x=x^2;'); x;", 16);
	EidosAssertScriptSuccess_F("x=2; y='x=x^2;'; for (i in 1:2) executeLambda(y); x;", 16);
	EidosAssertScriptSuccess_F("x=2; y='x=x^2;'; for (i in 1:2) executeLambda(y[T]); x;", 16);
	
	EidosAssertScriptSuccess_F("x=7; executeLambda('x^2;', T);", 49);
	EidosAssertScriptRaise("x=7; executeLambda('x^2', T);", 5, "unexpected token");
	EidosAssertScriptRaise("x=7; executeLambda(c('x^2;', '5;'), T);", 5, "must be a singleton");
	EidosAssertScriptRaise("x=7; executeLambda(string(0), T);", 5, "must be a singleton");
	EidosAssertScriptSuccess_F("x=7; executeLambda('x=x^2+4;', T); x;", 53);
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
	EidosAssertScriptSuccess_F("x=2; for (i in 1:2) executeLambda('x=x^2;', T); x;", 16);
	EidosAssertScriptSuccess_F("x=2; y='x=x^2;'; for (i in 1:2) executeLambda(y, T); x;", 16);
	EidosAssertScriptSuccess_F("x=2; y='x=x^2;'; for (i in 1:2) executeLambda(y[T], T); x;", 16);

	// exists()
	EidosAssertScriptSuccess_L("exists('T');", true);
	EidosAssertScriptSuccess_L("exists('foo');", false);
	EidosAssertScriptSuccess_L("foo = 5:10; exists('foo');", true);
	EidosAssertScriptSuccess_L("foo = 5:10; rm('foo'); exists('foo');", false);
	EidosAssertScriptSuccess_L("defineConstant('foo', 5:10); exists('foo');", true);
	EidosAssertScriptSuccess_LV("a=5; c=7.0; g='foo'; exists(c('a', 'b', 'c', 'd', 'e', 'f', 'g'));", {true, false, true, false, false, false, true});
	EidosAssertScriptSuccess_LV("exists(c('T', 'Q', 'F', 'PW', 'PI', 'D', 'E'));", {true, false, true, false, true, false, true});
	
	// functionSignature()
	EidosAssertScriptSuccess_VOID("functionSignature();");
	EidosAssertScriptSuccess_VOID("functionSignature('functionSignature');");
	EidosAssertScriptSuccess_VOID("functionSignature('foo');");	// does not throw at present
	EidosAssertScriptRaise("functionSignature(string(0));", 0, "must be a singleton");
	EidosAssertScriptSuccess_VOID("functionSignature(NULL);");		// same as omitting the parameter
	EidosAssertScriptRaise("functionSignature(T);", 0, "cannot be type");
	EidosAssertScriptRaise("functionSignature(3);", 0, "cannot be type");
	EidosAssertScriptRaise("functionSignature(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("functionSignature(_Test(7));", 0, "cannot be type");
	
	// functionSource()
	EidosAssertScriptSuccess_VOID("functionSource('foo');");	 // does not throw at present
	EidosAssertScriptSuccess_VOID("functionSource('mean');");
	EidosAssertScriptSuccess_VOID("functionSource('source');");
	
	// ls()
	EidosAssertScriptSuccess_VOID("ls();");
	EidosAssertScriptSuccess_VOID("ls(F);");
	EidosAssertScriptSuccess_VOID("ls(T);");
	
	// license()
	EidosAssertScriptSuccess_VOID("license();");
	EidosAssertScriptRaise("license(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("license(_Test(7));", 0, "too many arguments supplied");
	
	// rm()
	EidosAssertScriptSuccess_VOID("rm();");
	EidosAssertScriptRaise("x=37; rm('x'); x;", 15, "undefined identifier");
	EidosAssertScriptSuccess_I("x=37; rm('y'); x;", 37);
	EidosAssertScriptRaise("x=37; rm(); x;", 12, "undefined identifier");
	EidosAssertScriptRaise("rm(3);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(_Test(7));", 0, "cannot be type");
	EidosAssertScriptRaise("rm(T);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(F);", 0, "cannot be type");
	EidosAssertScriptSuccess_VOID("rm(NULL);");		// same as omitting the parameter
	EidosAssertScriptRaise("rm(INF);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(NAN);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(E);", 0, "cannot be type");
	EidosAssertScriptRaise("rm(PI);", 0, "cannot be type");
	EidosAssertScriptRaise("rm('PI');", 0, "intrinsic Eidos constant");
	EidosAssertScriptRaise("defineConstant('foo', 1:10); rm('foo'); foo;", 29, "is a constant");
	
	// setSeed()
	EidosAssertScriptSuccess_L("setSeed(5); x=runif(10); setSeed(5); y=runif(10); all(x==y);", true);
	EidosAssertScriptSuccess_L("setSeed(5); x=runif(10); setSeed(6); y=runif(10); all(x==y);", false);
	EidosAssertScriptRaise("setSeed(NULL);", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed(T);", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed('foo');", 0, "cannot be type");
	EidosAssertScriptRaise("setSeed(_Test(7));", 0, "cannot be type");
	
	// getSeed()
	EidosAssertScriptSuccess_I("setSeed(13); getSeed();", 13);
	EidosAssertScriptSuccess_I("setSeed(13); setSeed(7); getSeed();", 7);
	EidosAssertScriptRaise("getSeed(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("getSeed(_Test(7));", 0, "too many arguments supplied");
	
	// source()
	if (Eidos_TemporaryDirectoryExists())
	{
		EidosAssertScriptSuccess_I("path = '" + temp_path + "'; file = path + '/EidosSourceTest.txt'; writeFile(file, 'x=9*9;'); source(file); x;", 81);														// finds the file and executes it correctly
		EidosAssertScriptSuccess_L("path = '" + temp_path + "'; file = path + '/EidosSourceTest2.txt'; writeFile(file, 'x = getwd();'); d = getwd(); source(file, chdir=F); x == d;", true);					// doesn't change the wd with chdir=F
		EidosAssertScriptSuccess_L("path = '" + temp_path + "'; file = path + '/EidosSourceTest3.txt'; writeFile(file, 'x = getwd();'); d = getwd(); source(file, chdir=T); d == getwd();", true);				// any change is temporary with chdir=T
		EidosAssertScriptSuccess_L("path = '" + temp_path + "'; file = path + '/EidosSourceTest3.txt'; writeFile(file, 'x = getwd();'); source(file, chdir=T); setwd(path); d = getwd(); x == d;", true);		// change is correct with chdir=T; might not match temp_path due to symlinks
	}
	EidosAssertScriptRaise("source('/this/path/presumably/does/not/exist/foo_bar_baz_12345.eidos');", 0, "file not found at path");
	
	// stop()
	EidosAssertScriptRaise("stop();", 0, "stop() called");
	EidosAssertScriptRaise("stop('Error');", 0, "stop() called with error message:");
	EidosAssertScriptRaise("stop(NULL);", 0, "stop() called");		// same as omitting the parameter
	EidosAssertScriptRaise("stop(T);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(3);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(3.5);", 0, "cannot be type");
	EidosAssertScriptRaise("stop(_Test(7));", 0, "cannot be type");
	
	// suppressWarnings()
	EidosAssertScriptSuccess_L("suppressWarnings(F);", false);
	EidosAssertScriptSuccess_L("suppressWarnings(T);", false);
	EidosAssertScriptSuccess_L("suppressWarnings(T); suppressWarnings(F);", true);
	
	// sysinfo()
	EidosAssertScriptSuccess_L("x = sysinfo('os'); length(x) > 0;", true);
	EidosAssertScriptSuccess_L("x = sysinfo('sysname'); length(x) > 0;", true);
	EidosAssertScriptSuccess_L("x = sysinfo('release'); length(x) > 0;", true);
	EidosAssertScriptSuccess_L("x = sysinfo('version'); length(x) > 0;", true);
	EidosAssertScriptSuccess_L("x = sysinfo('nodename'); length(x) > 0;", true);
	EidosAssertScriptSuccess_L("x = sysinfo('machine'); length(x) > 0;", true);
	// These two keys are not yet supported due to problems on Windows and Ubuntu 18.04
	//EidosAssertScriptSuccess_L("x = sysinfo('login'); length(x) > 0;", true);
	//EidosAssertScriptSuccess_L("x = sysinfo('user'); length(x) > 0;", true);
	EidosAssertScriptSuccess_L("x = sysinfo('foo'); x == 'unknown';", true);
	
	// system()
	if (Eidos_TemporaryDirectoryExists())
	{
		EidosAssertScriptRaise("system('');", 0, "non-empty command string");
		// sadly none of the original tests work in Windows, including the echo one, 
		// because Windows does not understand ;
		// here I just make Windows versions of each original test (see the #else below)
		#ifdef _WIN32
		EidosAssertScriptSuccess_S("system('set /a 5 + 5');", "10");
		EidosAssertScriptSuccess_S("system('set', args=c('/a', '5', '+', '5'));", "10");
		EidosAssertScriptSuccess_S("system('set /a 5 / 0', stderr=T);", "Divide by zero error.");
		EidosAssertScriptSuccess_S("system('echo foo');", "foo");
		// input doesn't currently work because ofstream() fails
		EidosAssertScriptSuccess_SV("system('echo foo&echo bar&echo baz');", {"foo", "bar", "baz"});
		#else
		EidosAssertScriptSuccess_S("system('expr 5 + 5');", "10");
		EidosAssertScriptSuccess_S("system('expr', args=c('5', '+', '5'));", "10");
		EidosAssertScriptSuccess_L("err = system('expr 5 / 0', stderr=T); (err == 'expr: division by zero') | (err == 'expr: división por cero') | (err == 'expr: division par zéro') | (substr(err, 0, 5) == 'expr: ');", true);	// unfortunately system localization makes the message returned vary
		EidosAssertScriptSuccess_S("system('printf foo');", "foo");
		EidosAssertScriptSuccess_S("system(\"printf 'foo bar baz' | wc -m | sed 's/ //g'\");", "11");
		EidosAssertScriptSuccess_S("system(\"(wc -l | sed 's/ //g')\", input='foo\\nbar\\nbaz\\n');", "3");
		EidosAssertScriptSuccess_S("system(\"(wc -l | sed 's/ //g')\", input=c('foo', 'bar', 'baz'));", "3");
		EidosAssertScriptSuccess_SV("system(\"echo foo; echo bar; echo baz;\");", {"foo", "bar", "baz"});
		#endif
	}
	
	// time()
	EidosAssertScriptSuccess_I("size(strsplit(time(), ':'));", 3);
	EidosAssertScriptRaise("time(NULL);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time(T);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time(3);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time(3.5);", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time('foo');", 0, "too many arguments supplied");
	EidosAssertScriptRaise("time(_Test(7));", 0, "too many arguments supplied");
	
	// usage(); allow zero since this call returns zero on some less-supported platforms
	EidosAssertScriptSuccess_L("usage() >= 0.0;", true);
	EidosAssertScriptSuccess_L("usage(F) >= 0.0;", true);
	EidosAssertScriptSuccess_L("usage(T) >= 0.0;", true);
	EidosAssertScriptSuccess_L("usage('rss') >= 0.0;", true);
	EidosAssertScriptSuccess_L("usage('rss_peak') >= 0.0;", true);
	EidosAssertScriptSuccess_L("usage('vm') >= 0.0;", true);
	EidosAssertScriptRaise("usage('foo') >= 0.0;", 0, "type should be");
	
	// version()
	EidosAssertScriptSuccess_L("type(version(T)) == 'float';", true);
	EidosAssertScriptSuccess_L("type(version(F)) == 'float';", true);
	EidosAssertScriptRaise("version(NULL);", 0, "cannot be type NULL");
	EidosAssertScriptRaise("version(3);", 0, "cannot be type integer");
	EidosAssertScriptRaise("version(3.5);", 0, "cannot be type float");
	EidosAssertScriptRaise("version('foo');", 0, "cannot be type string");
	EidosAssertScriptRaise("version(_Test(7));", 0, "cannot be type object");
}

#pragma mark classes
void _RunClassTests(const std::string &temp_path)
{
	// Test EidosObject methods, using EidosTestElement since EidosObject is an abstract base class
	
	// methodSignature()
	EidosAssertScriptSuccess_VOID("_Test(7).methodSignature();");
	EidosAssertScriptSuccess_VOID("_Test(7).methodSignature('methodSignature');");
	EidosAssertScriptSuccess_VOID("matrix(_Test(7)).methodSignature('methodSignature');");
	
	// propertySignature()
	EidosAssertScriptSuccess_VOID("_Test(7).propertySignature();");
	EidosAssertScriptSuccess_VOID("_Test(7).propertySignature('_yolk');");
	EidosAssertScriptSuccess_VOID("matrix(_Test(7)).propertySignature('_yolk');");
	
	// size() / length()
	EidosAssertScriptSuccess("_Test(7).size();", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess_I("rep(_Test(7), 5).size();", 5);
	EidosAssertScriptSuccess_I("matrix(rep(_Test(7), 5)).size();", 5);
	
	EidosAssertScriptSuccess("_Test(7).length();", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess_I("rep(_Test(7), 5).length();", 5);
	EidosAssertScriptSuccess_I("matrix(rep(_Test(7), 5)).length();", 5);
	
	// str()
	EidosAssertScriptSuccess_VOID("_Test(7).str();");
	EidosAssertScriptSuccess_VOID("c(_Test(7), _Test(8), _Test(9)).str();");
	EidosAssertScriptSuccess_VOID("matrix(_Test(7)).str();");
	EidosAssertScriptSuccess_VOID("matrix(c(_Test(7), _Test(8), _Test(9))).str();");
	
	// stringRepresentation()
	EidosAssertScriptSuccess_SV("matrix(rep(_Test(7), 3)).stringRepresentation();", {"_TestElement", "_TestElement", "_TestElement"});
	EidosAssertScriptSuccess_S("Dictionary('a', 1:3, 'b', 5:6).stringRepresentation();", R"V0G0N({"a"=1 2 3;"b"=5 6;})V0G0N");
	EidosAssertScriptSuccess_S("Dictionary('b', 5:6, 'a', 1:3).stringRepresentation();", R"V0G0N({"a"=1 2 3;"b"=5 6;})V0G0N");
	EidosAssertScriptSuccess_S("Dictionary(10, 1:3, 15, 5:6).stringRepresentation();", "{10=1 2 3;15=5 6;}");
	EidosAssertScriptSuccess_S("Dictionary(15, 5:6, 10, 1:3).stringRepresentation();", "{10=1 2 3;15=5 6;}");
	
	// Test EidosDictionaryUnretained properties and methods, using EidosDictionaryRetained
	// since there's no way to instantiate an EidosDictionaryUnretained directly
	
	// setValue() / getValue()
	EidosAssertScriptSuccess_NULL("x = Dictionary(); x.getValue('a');");
	EidosAssertScriptSuccess_LV("x = Dictionary(); x.setValue('a', c(T,F,T)); x.getValue('a');", {true,false,true});
	EidosAssertScriptSuccess_IV("x = Dictionary(); x.setValue('a', 7:9); x.getValue('a');", {7,8,9});
	EidosAssertScriptSuccess_FV("x = Dictionary(); x.setValue('a', 7.0:9); x.getValue('a');", {7,8,9});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('baz', c('foo', 'bar')); x.getValue('baz');", {"foo", "bar"});
	EidosAssertScriptSuccess_S("x = Dictionary(); y = Dictionary(); y.setValue('foo', 'bar'); x.setValue('a', y); x.getValue('a').getValue('foo');", "bar");
	EidosAssertScriptSuccess_NULL("x = Dictionary(); x.setValue('a', 7:9); x.setValue('a', NULL); x.getValue('a');");
	EidosAssertScriptSuccess_NULL("x = Dictionary(); y = Dictionary(); y.setValue('foo', 'bar'); x.setValue('a', y); x.getValue('a').setValue('foo', NULL); x.getValue('a').getValue('foo');");
	EidosAssertScriptRaise("x = Dictionary(); x.setValue('a', 7:9); x.setValue(5, 5:8);", 42, "integer key");
	EidosAssertScriptRaise("x = Dictionary(); x.setValue('a', 7:9); x.getValue(5);", 42, "integer key");
	
	EidosAssertScriptSuccess_NULL("x = Dictionary(); x.getValue(5);");
	EidosAssertScriptSuccess_LV("x = Dictionary(); x.setValue(5, c(T,F,T)); x.getValue(5);", {true,false,true});
	EidosAssertScriptSuccess_IV("x = Dictionary(); x.setValue(5, 7:9); x.getValue(5);", {7,8,9});
	EidosAssertScriptSuccess_FV("x = Dictionary(); x.setValue(5, 7.0:9); x.getValue(5);", {7,8,9});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue(5, c('foo', 'bar')); x.getValue(5);", {"foo", "bar"});
	EidosAssertScriptSuccess_S("x = Dictionary(); y = Dictionary(); y.setValue(5, 'bar'); x.setValue(7, y); x.getValue(7).getValue(5);", "bar");
	EidosAssertScriptSuccess_NULL("x = Dictionary(); x.setValue(5, 7:9); x.setValue(5, NULL); x.getValue(5);");
	EidosAssertScriptSuccess_NULL("x = Dictionary(); y = Dictionary(); y.setValue(5, 'bar'); x.setValue(7, y); x.getValue(7).setValue(5, NULL); x.getValue(7).getValue(5);");
	EidosAssertScriptRaise("x = Dictionary(); x.setValue(5, 7:9); x.setValue('a', 5:8);", 40, "string key");
	EidosAssertScriptRaise("x = Dictionary(); x.setValue(5, 7:9); x.getValue('a');", 40, "string key");
	
	// allKeys
	EidosAssertScriptSuccess("x = Dictionary(); x.allKeys;", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('bar', c(T,F,T)); x.allKeys;", "bar");
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('bar', c(T,F,T)); x.setValue('foo', 7:9); x.setValue('baz', 7.0:9); x.allKeys;", {"bar", "baz", "foo"});
	
	EidosAssertScriptSuccess_I("x = Dictionary(); x.setValue(5, c(T,F,T)); x.allKeys;", 5);
	EidosAssertScriptSuccess_IV("x = Dictionary(); x.setValue(1, c(T,F,T)); x.setValue(5, 7:9); x.setValue(3, 7.0:9); x.allKeys;", {1, 3, 5});
	
	// addKeysAndValuesFrom()
	EidosAssertScriptSuccess("x = Dictionary(); y = x; x.setValue('bar', 2); y.getValue('bar');", gStaticEidosValue_Integer2);
	EidosAssertScriptSuccess_NULL("x = Dictionary(); y = Dictionary(); y.addKeysAndValuesFrom(x); x.setValue('bar', 2); y.getValue('bar');");
	EidosAssertScriptSuccess("x = Dictionary(); x.setValue('bar', 2); y = Dictionary(); y.addKeysAndValuesFrom(x); x.setValue('bar', 1); y.getValue('bar');", gStaticEidosValue_Integer2);
	EidosAssertScriptSuccess("x = Dictionary(); x.setValue('bar', 2); y = Dictionary(); y.addKeysAndValuesFrom(x); x.setValue('bar', 1); x.getValue('bar');", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('bar', 2); x.setValue('baz', 'foo'); y = Dictionary(); y.addKeysAndValuesFrom(x); y.setValue('xyzzy', 17); sort(y.allKeys);", {"bar", "baz", "xyzzy"});
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('bar', 2); x.setValue('baz', 'foo'); y = Dictionary(); y.addKeysAndValuesFrom(x); y.setValue('baz', NULL); y.allKeys;", "bar");
	
	EidosAssertScriptSuccess("x = Dictionary(); y = x; x.setValue(5, 2); y.getValue(5);", gStaticEidosValue_Integer2);
	EidosAssertScriptSuccess_NULL("x = Dictionary(); y = Dictionary(); y.addKeysAndValuesFrom(x); x.setValue(5, 2); y.getValue(5);");
	EidosAssertScriptSuccess("x = Dictionary(); x.setValue(5, 2); y = Dictionary(); y.addKeysAndValuesFrom(x); x.setValue(5, 1); y.getValue(5);", gStaticEidosValue_Integer2);
	EidosAssertScriptSuccess("x = Dictionary(); x.setValue(5, 2); y = Dictionary(); y.addKeysAndValuesFrom(x); x.setValue(5, 1); x.getValue(5);", gStaticEidosValue_Integer1);
	EidosAssertScriptSuccess_IV("x = Dictionary(); x.setValue(5, 2); x.setValue(7, 'foo'); y = Dictionary(); y.addKeysAndValuesFrom(x); y.setValue(9, 17); sort(y.allKeys);", {5, 7, 9});
	EidosAssertScriptSuccess_I("x = Dictionary(); x.setValue(5, 2); x.setValue(7, 'foo'); y = Dictionary(); y.addKeysAndValuesFrom(x); y.setValue(7, NULL); y.allKeys;", 5);
	
	EidosAssertScriptRaise("x = Dictionary(); x.setValue(5, 2); y = Dictionary(); y.setValue('a', 'foo'); y.addKeysAndValuesFrom(x);", 80, "integer key");
	
	// Dictionary(...)
	// identicalContents()
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3)); y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); x.identicalContents(y);", true);
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3)); y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'e', c(1.1, 2.2, 3.3)); x.identicalContents(y);", false);
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3)); y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.15, 2.2, 3.3)); x.identicalContents(y);", false);
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3, 4.4)); y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); x.identicalContents(y);", false);
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3)); y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3, 4.4)); x.identicalContents(y);", false);
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3)); y = Dictionary(x); x.identicalContents(y);", true);
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3)); y = Dictionary(x); y.identicalContents(x);", true);
	EidosAssertScriptRaise("Dictionary(5);", 0, "be a singleton Dictionary");
	EidosAssertScriptRaise("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3, 4.4)); Dictionary(c(y,y));", 100, "be a singleton");
	EidosAssertScriptRaise("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3, 4.4)); Dictionary(y, y);", 100, "keys be of type string or integer");
	
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue(5, 0:2); x.setValue(7, c('foo', 'bar', 'baz')); x.setValue(9, c(T, F, T)); x.setValue(11, c(1.1, 2.2, 3.3)); y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); x.identicalContents(y);", true);
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue(5, 0:2); x.setValue(7, c('foo', 'bar', 'baz')); x.setValue(9, c(T, F, T)); x.setValue(11, c(1.1, 2.2, 3.3)); y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 13, c(1.1, 2.2, 3.3)); x.identicalContents(y);", false);
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue(5, 0:2); x.setValue(7, c('foo', 'bar', 'baz')); x.setValue(9, c(T, F, T)); x.setValue(11, c(1.1, 2.2, 3.3)); y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.15, 2.2, 3.3)); x.identicalContents(y);", false);
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue(5, 0:2); x.setValue(7, c('foo', 'bar', 'baz')); x.setValue(9, c(T, F, T)); x.setValue(11, c(1.1, 2.2, 3.3, 4.4)); y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); x.identicalContents(y);", false);
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue(5, 0:2); x.setValue(7, c('foo', 'bar', 'baz')); x.setValue(9, c(T, F, T)); x.setValue(11, c(1.1, 2.2, 3.3)); y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3, 4.4)); x.identicalContents(y);", false);
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue(5, 0:2); x.setValue(7, c('foo', 'bar', 'baz')); x.setValue(9, c(T, F, T)); x.setValue(11, c(1.1, 2.2, 3.3)); y = Dictionary(x); x.identicalContents(y);", true);
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue(5, 0:2); x.setValue(7, c('foo', 'bar', 'baz')); x.setValue(9, c(T, F, T)); x.setValue(11, c(1.1, 2.2, 3.3)); y = Dictionary(x); y.identicalContents(x);", true);
	EidosAssertScriptRaise("Dictionary(5);", 0, "be a singleton Dictionary");
	EidosAssertScriptRaise("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3, 4.4)); Dictionary(c(y,y));", 93, "be a singleton");
	EidosAssertScriptRaise("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3, 4.4)); Dictionary(y, y);", 93, "keys be of type string or integer");
	
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue(5, 2); y = Dictionary(); y.setValue('a', 'foo'); x.identicalContents(y);", false);
	EidosAssertScriptRaise("x = Dictionary(5, 1:10, 'a', 1:10);", 4, "string key");
	EidosAssertScriptRaise("x = Dictionary('a', 1:10, 5, 1:10);", 4, "integer key");
	
	// appendKeysAndValuesFrom()
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = Dictionary('a', 4, 'b', 9:10); x.appendKeysAndValuesFrom(y); x.identicalContents(Dictionary('a', 0:4, 'b', 2:10));", true);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = Dictionary('a', 4, 'c', 9:10); x.appendKeysAndValuesFrom(y); x.identicalContents(Dictionary('a', 0:4, 'b', 2:8, 'c', 9:10));", true);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = Dictionary('a', 4, 'b', 9.0:10); x.appendKeysAndValuesFrom(y); x.identicalContents(Dictionary('a', 0:4, 'b', 2:10));", false);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = Dictionary('a', 4, 'b', 9.0:10); x.appendKeysAndValuesFrom(y); x.identicalContents(Dictionary('a', 0:4, 'b', 2.0:10));", true);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = Dictionary('a', 4, 'b', 9:10); x.appendKeysAndValuesFrom(c(y, y)); x.identicalContents(Dictionary('a', c(0:4, 4), 'b', c(2:10, 9:10)));", true);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = Dictionary('a', 4, 'c', 9:10); x.appendKeysAndValuesFrom(c(y, y)); x.identicalContents(Dictionary('a', c(0:4, 4), 'b', 2:8, 'c', c(9:10, 9:10)));", true);
	EidosAssertScriptRaise("x = Dictionary('a', 0:3, 'b', 2:8); y = Dictionary('a', 4, 'c', 9:10); x.appendKeysAndValuesFrom(x);", 73, "cannot append a Dictionary to itself");
	
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = Dictionary(5, 4, 7, 9:10); x.appendKeysAndValuesFrom(y); x.identicalContents(Dictionary(5, 0:4, 7, 2:10));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = Dictionary(5, 4, 9, 9:10); x.appendKeysAndValuesFrom(y); x.identicalContents(Dictionary(5, 0:4, 7, 2:8, 9, 9:10));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = Dictionary(5, 4, 7, 9.0:10); x.appendKeysAndValuesFrom(y); x.identicalContents(Dictionary(5, 0:4, 7, 2:10));", false);
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = Dictionary(5, 4, 7, 9.0:10); x.appendKeysAndValuesFrom(y); x.identicalContents(Dictionary(5, 0:4, 7, 2.0:10));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = Dictionary(5, 4, 7, 9:10); x.appendKeysAndValuesFrom(c(y, y)); x.identicalContents(Dictionary(5, c(0:4, 4), 7, c(2:10, 9:10)));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = Dictionary(5, 4, 9, 9:10); x.appendKeysAndValuesFrom(c(y, y)); x.identicalContents(Dictionary(5, c(0:4, 4), 7, 2:8, 9, c(9:10, 9:10)));", true);
	EidosAssertScriptRaise("x = Dictionary(5, 0:3, 7, 2:8); y = Dictionary(5, 4, 9, 9:10); x.appendKeysAndValuesFrom(x);", 65, "cannot append a Dictionary to itself");
	
	EidosAssertScriptRaise("x = Dictionary(5, 0:3, 7, 2:8); y = Dictionary('a', 4, 'b', 9:10); x.appendKeysAndValuesFrom(y);", 69, "string key");
	
	// getRowValues()
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = x.getRowValues(0); y.identicalContents(Dictionary('a', 0, 'b', 2));", true);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = x.getRowValues(1); y.identicalContents(Dictionary('a', 1, 'b', 3));", true);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = x.getRowValues(4); y.identicalContents(Dictionary('a', integer(0), 'b', 6));", true);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = x.getRowValues(4, drop=T); y.identicalContents(Dictionary('b', 6));", true);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = x.getRowValues(c(T, F, T, T)); y.identicalContents(Dictionary('a', c(0, 2, 3), 'b', c(2, 4, 5)));", true);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = x.getRowValues(F); y.identicalContents(Dictionary('a', integer(0), 'b', integer(0)));", true);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = x.getRowValues(c(F, F)); y.identicalContents(Dictionary('a', integer(0), 'b', integer(0)));", true);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = x.getRowValues(F, drop=T); y.identicalContents(Dictionary());", true);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 0:3, 'b', 2:8); y = x.getRowValues(c(F, F), drop=T); y.identicalContents(Dictionary());", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(0).identicalContents(Dictionary('a', 0, 'b', 'foo', 'c', T, 'd', 1.1));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(1).identicalContents(Dictionary('a', 1, 'b', 'bar', 'c', F, 'd', 2.2));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(2).identicalContents(Dictionary('a', 2, 'b', 'baz', 'c', T, 'd', 3.3));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(3).identicalContents(Dictionary('a', integer(0), 'b', string(0), 'c', logical(0), 'd', float(0)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(3, drop=T).identicalContents(Dictionary());", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(-1).identicalContents(Dictionary('a', integer(0), 'b', string(0), 'c', logical(0), 'd', float(0)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(-1, drop=T).identicalContents(Dictionary());", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(0:1).identicalContents(Dictionary('a', 0:1, 'b', c('foo', 'bar'), 'c', c(T, F), 'd', c(1.1, 2.2)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(1:0).identicalContents(Dictionary('a', 1:0, 'b', c('bar', 'foo'), 'c', c(F, T), 'd', c(2.2, 1.1)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(c(F, F, F)).identicalContents(Dictionary('a', integer(0), 'b', string(0), 'c', logical(0), 'd', float(0)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(c(T, F, F)).identicalContents(Dictionary('a', 0, 'b', 'foo', 'c', T, 'd', 1.1));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(c(F, T, F)).identicalContents(Dictionary('a', 1, 'b', 'bar', 'c', F, 'd', 2.2));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(c(F, F, T)).identicalContents(Dictionary('a', 2, 'b', 'baz', 'c', T, 'd', 3.3));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(c(T, T, F)).identicalContents(Dictionary('a', 0:1, 'b', c('foo', 'bar'), 'c', c(T, F), 'd', c(1.1, 2.2)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(c(T, T, F, T)).identicalContents(Dictionary('a', 0:1, 'b', c('foo', 'bar'), 'c', c(T, F), 'd', c(1.1, 2.2)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(c(T, T)).identicalContents(Dictionary('a', 0:1, 'b', c('foo', 'bar'), 'c', c(T, F), 'd', c(1.1, 2.2)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(c(F, F)).identicalContents(Dictionary('a', integer(0), 'b', string(0), 'c', logical(0), 'd', float(0)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); y.getRowValues(c(F, F), drop=T).identicalContents(Dictionary());", true);
	
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = x.getRowValues(0); y.identicalContents(Dictionary(5, 0, 7, 2));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = x.getRowValues(1); y.identicalContents(Dictionary(5, 1, 7, 3));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = x.getRowValues(4); y.identicalContents(Dictionary(5, integer(0), 7, 6));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = x.getRowValues(4, drop=T); y.identicalContents(Dictionary(7, 6));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = x.getRowValues(c(T, F, T, T)); y.identicalContents(Dictionary(5, c(0, 2, 3), 7, c(2, 4, 5)));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = x.getRowValues(F); y.identicalContents(Dictionary(5, integer(0), 7, integer(0)));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = x.getRowValues(c(F, F)); y.identicalContents(Dictionary(5, integer(0), 7, integer(0)));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = x.getRowValues(F, drop=T); y.identicalContents(Dictionary());", true);
	EidosAssertScriptSuccess_L("x = Dictionary(5, 0:3, 7, 2:8); y = x.getRowValues(c(F, F), drop=T); y.identicalContents(Dictionary());", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(0).identicalContents(Dictionary(5, 0, 7, 'foo', 9, T, 11, 1.1));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(1).identicalContents(Dictionary(5, 1, 7, 'bar', 9, F, 11, 2.2));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(2).identicalContents(Dictionary(5, 2, 7, 'baz', 9, T, 11, 3.3));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(3).identicalContents(Dictionary(5, integer(0), 7, string(0), 9, logical(0), 11, float(0)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(3, drop=T).identicalContents(Dictionary());", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(-1).identicalContents(Dictionary(5, integer(0), 7, string(0), 9, logical(0), 11, float(0)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(-1, drop=T).identicalContents(Dictionary());", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(0:1).identicalContents(Dictionary(5, 0:1, 7, c('foo', 'bar'), 9, c(T, F), 11, c(1.1, 2.2)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(1:0).identicalContents(Dictionary(5, 1:0, 7, c('bar', 'foo'), 9, c(F, T), 11, c(2.2, 1.1)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(c(F, F, F)).identicalContents(Dictionary(5, integer(0), 7, string(0), 9, logical(0), 11, float(0)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(c(T, F, F)).identicalContents(Dictionary(5, 0, 7, 'foo', 9, T, 11, 1.1));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(c(F, T, F)).identicalContents(Dictionary(5, 1, 7, 'bar', 9, F, 11, 2.2));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(c(F, F, T)).identicalContents(Dictionary(5, 2, 7, 'baz', 9, T, 11, 3.3));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(c(T, T, F)).identicalContents(Dictionary(5, 0:1, 7, c('foo', 'bar'), 9, c(T, F), 11, c(1.1, 2.2)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(c(T, T, F, T)).identicalContents(Dictionary(5, 0:1, 7, c('foo', 'bar'), 9, c(T, F), 11, c(1.1, 2.2)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(c(T, T)).identicalContents(Dictionary(5, 0:1, 7, c('foo', 'bar'), 9, c(T, F), 11, c(1.1, 2.2)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(c(F, F)).identicalContents(Dictionary(5, integer(0), 7, string(0), 9, logical(0), 11, float(0)));", true);
	EidosAssertScriptSuccess_L("y = Dictionary(5, 0:2, 7, c('foo', 'bar', 'baz'), 9, c(T, F, T), 11, c(1.1, 2.2, 3.3)); y.getRowValues(c(F, F), drop=T).identicalContents(Dictionary());", true);
	
	// clearKeysAndValues()
	EidosAssertScriptSuccess("x = Dictionary(); x.setValue('bar', 2); x.setValue('baz', 'foo'); x.clearKeysAndValues(); x.allKeys;", gStaticEidosValue_String_ZeroVec);
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('bar', 2); x.setValue('baz', 'foo'); x.clearKeysAndValues(); x.setValue('foo', 'baz'); x.allKeys;", "foo");
	
	EidosAssertScriptSuccess("x = Dictionary(); x.setValue(5, 2); x.setValue(7, 'foo'); x.clearKeysAndValues(); x.allKeys;", gStaticEidosValue_Integer_ZeroVec);
	EidosAssertScriptSuccess_I("x = Dictionary(); x.setValue(5, 2); x.setValue(7, 'foo'); x.clearKeysAndValues(); x.setValue(9, 'baz'); x.allKeys;", 9);
	
	EidosAssertScriptRaise("x = Dictionary(); x.setValue(5, 2); x.setValue(7, 'foo'); x.clearKeysAndValues(); x.setValue('foo', 'baz'); x.allKeys;", 84, "string key");
	
	// compactIndices()
	EidosAssertScriptSuccess_L("x = Dictionary(); x.compactIndices(); x.identicalContents(Dictionary());", true);
	EidosAssertScriptRaise("x = Dictionary(); x.setValue('foo', 5:7); x.compactIndices();", 44, "integer keys");
	EidosAssertScriptSuccess_L("x = Dictionary(53,'c', 17,'b', 80,'d', 5,'a', 85,'e'); x.compactIndices(preserveOrder=F); values=sapply(x.allKeys, 'x.getValue(applyValue);'); identical(x.allKeys, 0:4) & identical(sort(values), c('a','b','c','d','e'));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(53,'c', 17,'b', 80,'d', 5,'a', 85,'e'); x.compactIndices(preserveOrder=T); x.identicalContents(Dictionary(0,'a', 1,'b', 2,'c', 3,'d', 4,'e'));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(53,'c', 7,integer(0), 17,'b', 80,'d', 83,string(0), 5,'a', 35,object(), 85,'e'); x.compactIndices(preserveOrder=F); values=sapply(x.allKeys, 'x.getValue(applyValue);'); identical(x.allKeys, 0:4) & identical(sort(values), c('a','b','c','d','e'));", true);
	EidosAssertScriptSuccess_L("x = Dictionary(53,'c', 7,integer(0), 17,'b', 80,'d', 83,string(0), 5,'a', 35,object(), 85,'e'); x.compactIndices(preserveOrder=T); x.identicalContents(Dictionary(0,'a', 1,'b', 2,'c', 3,'d', 4,'e'));", true);
	
	// serialize()
	EidosAssertScriptSuccess_S("x = Dictionary(); x.serialize();", "");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('foo', 1:3); x.serialize();", "\"foo\"=1 2 3;");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('foo', 1:3); x.setValue('bar', 'baz'); x.serialize();", R"V0G0N("bar"="baz";"foo"=1 2 3;)V0G0N");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('foo', 1:3); y = Dictionary(); y.setValue('a', 1.5); y.setValue('b', T); x.setValue('xyzzy', y); x.serialize();", R"V0G0N("foo"=1 2 3;"xyzzy"={"a"=1.5;"b"=T;};)V0G0N");
	
	EidosAssertScriptSuccess_S("x = Dictionary(); x.serialize();", "");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue(5, 1:3); x.serialize();", "5=1 2 3;");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue(5, 1:3); x.setValue(3, 'baz'); x.serialize();", "3=\"baz\";5=1 2 3;");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue(5, 1:3); y = Dictionary(); y.setValue(20, 1.5); y.setValue(30, T); x.setValue(11, y); x.serialize();", "5=1 2 3;11={20=1.5;30=T;};");
	
	EidosAssertScriptSuccess_S("x = Dictionary(); x.serialize('slim');", "");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('foo', 1:3); x.serialize('slim');", "\"foo\"=1 2 3;");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('foo', 1:3); x.setValue('bar', 'baz'); x.serialize('slim');", R"V0G0N("bar"="baz";"foo"=1 2 3;)V0G0N");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('foo', 1:3); y = Dictionary(); y.setValue('a', 1.5); y.setValue('b', T); x.setValue('xyzzy', y); x.serialize('slim');", R"V0G0N("foo"=1 2 3;"xyzzy"={"a"=1.5;"b"=T;};)V0G0N");
	
	EidosAssertScriptSuccess_S("x = Dictionary(); x.serialize('slim');", "");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue(5, 1:3); x.serialize('slim');", "5=1 2 3;");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue(5, 1:3); x.setValue(3, 'baz'); x.serialize('slim');", "3=\"baz\";5=1 2 3;");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue(5, 1:3); y = Dictionary(); y.setValue(20, 1.5); y.setValue(30, T); x.setValue(11, y); x.serialize('slim');", "5=1 2 3;11={20=1.5;30=T;};");
	
	EidosAssertScriptSuccess_S("x = Dictionary(); x.serialize('json');", "{}");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('foo', 1:3); x.serialize('json');", "{\"foo\":[1,2,3]}");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('foo', 1:3); x.setValue('bar', 'baz'); x.serialize('json');", R"V0G0N({"bar":["baz"],"foo":[1,2,3]})V0G0N");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('foo', 1:3); y = Dictionary(); y.setValue('a', 1.5); y.setValue('b', T); x.setValue('xyzzy', y); x.serialize('json');", R"V0G0N({"foo":[1,2,3],"xyzzy":[{"a":[1.5],"b":[true]}]})V0G0N");
	
	EidosAssertScriptRaise("x = Dictionary(); x.setValue(5, 1:3); x.serialize('json');", 40, "integer keys");
	
	EidosAssertScriptSuccess_S("x = Dictionary(); x.serialize('csv');", "");
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('foo', 1:3); x.serialize('csv');", {"\"foo\"", "1", "2", "3"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('foo', 1:3); x.setValue('bar', 'baz'); x.serialize('csv');", {R"V0G0N("bar","foo")V0G0N", "\"baz\",1", ",2", ",3"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('bar', 1:3); x.setValue('foo', 'baz'); x.serialize('csv');", {R"V0G0N("bar","foo")V0G0N", "1,\"baz\"", "2,", "3,"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('bar', 1:3); x.setValue('foo', c(T,F)); x.serialize('csv');", {R"V0G0N("bar","foo")V0G0N", "1,TRUE", "2,FALSE", "3,"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('bar', 1:3); x.setValue('foo', c(1.0, 2.1, 3.2)); x.serialize('csv');", {R"V0G0N("bar","foo")V0G0N", "1,1.0", "2,2.1", "3,3.2"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('foo', c(INF, -INF, NAN)); x.serialize('csv');", {"\"foo\"", "INF", "-INF", "NAN"});
	EidosAssertScriptRaise("x = Dictionary(); x.setValue('foo', 1:3); y = Dictionary(); x.setValue('xyzzy', y); x.serialize('csv');", 86, "object to CSV/TSV");
	
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue(5, 1:3); x.serialize('csv');", {"5", "1", "2", "3"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue(5, 1:3); x.setValue(3, 'baz'); x.serialize('csv');", {"3,5", "\"baz\",1", ",2", ",3"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue(3, 1:3); x.setValue(5, 'baz'); x.serialize('csv');", {"3,5", "1,\"baz\"", "2,", "3,"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue(3, 1:3); x.setValue(5, c(T,F)); x.serialize('csv');", {"3,5", "1,TRUE", "2,FALSE", "3,"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue(3, 1:3); x.setValue(5, c(1.0, 2.1, 3.2)); x.serialize('csv');", {"3,5", "1,1.0", "2,2.1", "3,3.2"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue(5, c(INF, -INF, NAN)); x.serialize('csv');", {"5", "INF", "-INF", "NAN"});
	EidosAssertScriptRaise("x = Dictionary(); x.setValue(5, 1:3); y = Dictionary(); x.setValue(11, y); x.serialize('csv');", 77, "object to CSV/TSV");
	
	EidosAssertScriptSuccess_S("x = Dictionary(); x.serialize('tsv');", "");
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('foo', 1:3); x.serialize('tsv');", {"\"foo\"", "1", "2", "3"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('foo', 1:3); x.setValue('bar', 'baz'); x.serialize('tsv');", {"\"bar\"\t\"foo\"", "\"baz\"\t1", "\t2", "\t3"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('bar', 1:3); x.setValue('foo', 'baz'); x.serialize('tsv');", {"\"bar\"\t\"foo\"", "1\t\"baz\"", "2\t", "3\t"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('bar', 1:3); x.setValue('foo', c(T,F)); x.serialize('tsv');", {"\"bar\"\t\"foo\"", "1\tTRUE", "2\tFALSE", "3\t"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('bar', 1:3); x.setValue('foo', c(1.0, 2.1, 3.2)); x.serialize('tsv');", {"\"bar\"\t\"foo\"", "1\t1.0", "2\t2.1", "3\t3.2"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue('foo', c(INF, -INF, NAN)); x.serialize('tsv');", {"\"foo\"", "INF", "-INF", "NAN"});
	EidosAssertScriptRaise("x = Dictionary(); x.setValue('foo', 1:3); y = Dictionary(); x.setValue('xyzzy', y); x.serialize('tsv');", 86, "object to CSV/TSV");
	
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue(5, 1:3); x.serialize('tsv');", {"5", "1", "2", "3"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue(5, 1:3); x.setValue(3, 'baz'); x.serialize('tsv');", {"3\t5", "\"baz\"\t1", "\t2", "\t3"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue(3, 1:3); x.setValue(5, 'baz'); x.serialize('tsv');", {"3\t5", "1\t\"baz\"", "2\t", "3\t"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue(3, 1:3); x.setValue(5, c(T,F)); x.serialize('tsv');", {"3\t5", "1\tTRUE", "2\tFALSE", "3\t"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue(3, 1:3); x.setValue(5, c(1.0, 2.1, 3.2)); x.serialize('tsv');", {"3\t5", "1\t1.0", "2\t2.1", "3\t3.2"});
	EidosAssertScriptSuccess_SV("x = Dictionary(); x.setValue(5, c(INF, -INF, NAN)); x.serialize('tsv');", {"5", "INF", "-INF", "NAN"});
	EidosAssertScriptRaise("x = Dictionary(); x.setValue(5, 1:3); y = Dictionary(); x.setValue(11, y); x.serialize('tsv');", 77, "object to CSV/TSV");
	
	EidosAssertScriptRaise("x = Dictionary(); x.serialize('foo');", 20, "does not recognize the format");
	
	// serialize(format='json') exact tests
	EidosAssertScriptSuccess_S("x = Dictionary(); x.serialize('json');", "{}");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', logical(0)); x.serialize('json');", "{\"b\":[]}");	// indistinguishable
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', T); x.serialize('json');", "{\"b\":[true]}");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', F); x.serialize('json');", "{\"b\":[false]}");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', c(T,F,T,F)); x.serialize('json');", "{\"b\":[true,false,true,false]}");
	
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', integer(0)); x.serialize('json');", "{\"b\":[]}");	// indistinguishable
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', -5); x.serialize('json');", "{\"b\":[-5]}");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', 5); x.serialize('json');", "{\"b\":[5]}");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', c(-5,5,10,-172)); x.serialize('json');", "{\"b\":[-5,5,10,-172]}");
	
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', float(0)); x.serialize('json');", "{\"b\":[]}");	// indistinguishable
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', -5.0); x.serialize('json');", "{\"b\":[-5.0]}");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', 5.7); x.serialize('json');", "{\"b\":[5.7]}");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', c(-5.0,5.7,10,-172)); x.serialize('json');", "{\"b\":[-5.0,5.7,10.0,-172.0]}");
	
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', string(0)); x.serialize('json');", "{\"b\":[]}");	// indistinguishable
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', \"foo\"); x.serialize('json');", R"V0G0N({"b":["foo"]})V0G0N");
	EidosAssertScriptSuccess_S(R"V0G0N(x = Dictionary(); x.setValue('b', "foo'\"bar"); x.serialize('json');)V0G0N", R"V0G0N({"b":["foo'\"bar"]})V0G0N");
	EidosAssertScriptSuccess_S(R"V0G0N(x = Dictionary(); x.setValue('b', c('foo','bar',"foo'\"bar")); x.serialize('json');)V0G0N", R"V0G0N({"b":["foo","bar","foo'\"bar"]})V0G0N");
	
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', Dictionary()); x.serialize('json');", "{\"b\":[{}]}");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', c(Dictionary(),Dictionary())); x.serialize('json');", "{\"b\":[{},{}]}");
	EidosAssertScriptSuccess_S("x = Dictionary(); x.setValue('b', c(Dictionary('x',1:3),Dictionary(),Dictionary('y','foo','z',1.73))); x.serialize('json');", R"V0G0N({"b":[{"x":[1,2,3]},{},{"y":["foo"],"z":[1.73]}]})V0G0N");
	
	// Dictionary(x="JSON_string")
	EidosAssertScriptRaise("Dictionary('invalid');", 0, "valid JSON string");
	EidosAssertScriptRaise("Dictionary('{invalid}');", 0, "valid JSON string");
	EidosAssertScriptSuccess_S("a = Dictionary('{\"a\": null}'); a.serialize('json');", "{\"a\":[{}]}");
	EidosAssertScriptSuccess_S("a = Dictionary('{\"a\": {}}'); a.serialize('json');", "{\"a\":[{}]}");
	EidosAssertScriptSuccess_S("a = Dictionary('{\"a\": true}'); a.serialize('json');", "{\"a\":[true]}");
	EidosAssertScriptSuccess_S("a = Dictionary('{\"a\": [true]}'); a.serialize('json');", "{\"a\":[true]}");
	EidosAssertScriptSuccess_S("a = Dictionary('{\"a\": false}'); a.serialize('json');", "{\"a\":[false]}");
	EidosAssertScriptSuccess_S("a = Dictionary('{\"a\": [false]}'); a.serialize('json');", "{\"a\":[false]}");
	EidosAssertScriptSuccess_S("a = Dictionary('{\"a\": 5}'); a.serialize('json');", "{\"a\":[5]}");
	EidosAssertScriptSuccess_S("a = Dictionary('{\"a\": [5]}'); a.serialize('json');", "{\"a\":[5]}");
	EidosAssertScriptSuccess_S("a = Dictionary('{\"a\": 5.5}'); a.serialize('json');", "{\"a\":[5.5]}");
	EidosAssertScriptSuccess_S("a = Dictionary('{\"a\": [5.5]}'); a.serialize('json');", "{\"a\":[5.5]}");
	EidosAssertScriptSuccess_S(R"V0G0N(a = Dictionary('{"a": "b"}'); a.serialize('json');)V0G0N", R"V0G0N({"a":["b"]})V0G0N");
	EidosAssertScriptSuccess_S(R"V0G0N(a = Dictionary('{"a": ["b"]}'); a.serialize('json');)V0G0N", R"V0G0N({"a":["b"]})V0G0N");
	EidosAssertScriptSuccess_L("a = Dictionary(); a.setValue('logical_empty', logical(0)); a.setValue('logical_T', T); a.setValue('logical_F', F); a.setValue('logical_vector', c(T, F, T, F)); a.setValue('int_empty', integer(0)); a.setValue('int_singleton', 1); a.setValue('int_vector', 1:3); a.setValue('float_empty', float(0)); a.setValue('float_singleton', 1.0); a.setValue('float_vector', 1.0:3); a.setValue('string_empty', string(0)); a.setValue('string_singleton', 'foo'); a.setValue('string_vector', c('foo', 'bar', 'baz')); sa_json = a.serialize('json'); b = Dictionary(sa_json); sb_json = b.serialize('json'); identical(sa_json,sb_json);", true);
	EidosAssertScriptSuccess_L("x = Dictionary('a', 5:7, 'b', 'foo'); x.setValue('c', Dictionary('d', 18)); y = x.serialize('json'); z = Dictionary(y); z = z.serialize('json'); identical(y, z);", true);
	
	// DataFrame(...)
	// identicalContents()
	EidosAssertScriptSuccess_L("x = DataFrame(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3)); y = DataFrame('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); x.identicalContents(y);", true);
	EidosAssertScriptSuccess_L("x = DataFrame(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3)); y = DataFrame('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'e', c(1.1, 2.2, 3.3)); x.identicalContents(y);", false);
	EidosAssertScriptSuccess_L("x = DataFrame(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3)); y = DataFrame('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.15, 2.2, 3.3)); x.identicalContents(y);", false);
	EidosAssertScriptRaise("x = DataFrame(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3, 4.4));", 111, "inconsistent column sizes");
	EidosAssertScriptRaise("y = DataFrame('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3, 4.4));", 4, "inconsistent column sizes");
	EidosAssertScriptSuccess_L("x = DataFrame(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3)); y = DataFrame(x); x.identicalContents(y);", true);
	EidosAssertScriptSuccess_L("x = DataFrame(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3)); y = DataFrame(x); y.identicalContents(x);", true);
	EidosAssertScriptRaise("DataFrame(5);", 0, "be a singleton Dictionary");
	EidosAssertScriptRaise("y = DataFrame('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); DataFrame(c(y,y));", 94, "be a singleton");
	EidosAssertScriptRaise("y = DataFrame('a', 0:2, 'b', c('foo', 'bar', 'baz'), 'c', c(T, F, T), 'd', c(1.1, 2.2, 3.3)); DataFrame(y, y);", 94, "keys be of type string or integer");
	EidosAssertScriptRaise("x = DataFrame(5, 1:10, 'a', 1:10);", 4, "always uses string keys");
	EidosAssertScriptRaise("x = DataFrame('a', 1:10, 5, 1:10);", 4, "always uses string keys");
	EidosAssertScriptSuccess_L("x = Dictionary('a', 1:10); y = DataFrame(x); z = DataFrame('a', 1:10); y.identicalContents(z);", true);
	EidosAssertScriptRaise("x = Dictionary(5, 1:10); y = DataFrame(x);", 29, "always uses string keys");
	
	EidosAssertScriptSuccess_L("x = Dictionary(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3)); y = DataFrame(x); y.identicalContents(x);", true);
	EidosAssertScriptSuccess_L("x = DataFrame(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3)); y = Dictionary(x); y.identicalContents(x);", true);
	EidosAssertScriptRaise("x = Dictionary(); x.setValue('a', 0:2); x.setValue('b', c('foo', 'bar', 'baz')); x.setValue('c', c(T, F, T)); x.setValue('d', c(1.1, 2.2, 3.3, 4.4)); y = DataFrame(x); y.identicalContents(x);", 154, "inconsistent column sizes");
	
	// DataFrame test column length check after Dictionary operations
	EidosAssertScriptRaise("x = DataFrame(); x.setValue('bar', 2); x.setValue('foo', 2:3);", 41, "inconsistent column sizes");
	EidosAssertScriptRaise("x = DataFrame('a', 2:4, 'b', 3:5); y = Dictionary('c', 4:7); x.appendKeysAndValuesFrom(y);", 63, "inconsistent column sizes");
	EidosAssertScriptSuccess_L("x = DataFrame('a', 2:4, 'b', 2:4); y = Dictionary('a', 5, 'b', 5, 'c', 4:7); x.appendKeysAndValuesFrom(y); x.identicalContents(DataFrame('a', 2:5, 'b', 2:5, 'c', 4:7));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('a', 2:4, 'b', 2:4); y = Dictionary('b', 5, 'a', 5, 'c', 4:7); x.appendKeysAndValuesFrom(y); x.identicalContents(DataFrame('a', 2:5, 'b', 2:5, 'c', 4:7));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 2:4, 'a', 2:4); y = Dictionary('a', 5, 'b', 5, 'c', 4:7); x.appendKeysAndValuesFrom(y); x.identicalContents(DataFrame('b', 2:5, 'a', 2:5, 'c', 4:7));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('a', 2:4, 'b', 2:4); y = Dictionary('a', 5, 'b', 5, 'c', 4:7); x.appendKeysAndValuesFrom(y); x.identicalContents(DataFrame('b', 2:5, 'a', 2:5, 'c', 4:7));", false);
	
	// DataFrame properties: colnames, dim, ncol, nrow
	EidosAssertScriptSuccess_L("x = DataFrame(); identical(x.colNames, string(0));", true);
	EidosAssertScriptSuccess_L("x = DataFrame(); identical(x.dim, c(0, 0));", true);
	EidosAssertScriptSuccess_L("x = DataFrame(); identical(x.ncol, 0);", true);
	EidosAssertScriptSuccess_L("x = DataFrame(); identical(x.nrow, 0);", true);

	EidosAssertScriptSuccess_L("x = DataFrame('a', integer(0), 'b', logical(0)); identical(x.colNames, c('a', 'b'));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('a', integer(0), 'b', logical(0)); identical(x.colNames, c('b', 'a'));", false);
	EidosAssertScriptSuccess_L("x = DataFrame('b', integer(0), 'a', logical(0)); identical(x.colNames, c('b', 'a'));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('a', integer(0), 'b', logical(0)); identical(x.dim, c(0, 2));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('a', integer(0), 'b', logical(0)); identical(x.ncol, 2);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('a', integer(0), 'b', logical(0)); identical(x.nrow, 0);", true);

	EidosAssertScriptSuccess_L("x = DataFrame('a', 1:3, 'b', c(T,F,T)); identical(x.colNames, c('a', 'b'));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('a', 1:3, 'b', c(T,F,T)); identical(x.colNames, c('b', 'a'));", false);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); identical(x.colNames, c('b', 'a'));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); identical(x.dim, c(3, 2));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); identical(x.ncol, 2);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); identical(x.nrow, 3);", true);
	
	// DataFrame asMatrix()
	EidosAssertScriptRaise("x = DataFrame('a', 1:3, 'b', c(T,F,T)); x.asMatrix();", 42, "is the same type (logical != integer)");
	EidosAssertScriptRaise("x = DataFrame('a', DataFrame(), 'b', Dictionary()); x.asMatrix();", 54, "is the same class (Dictionary != DataFrame)");
	EidosAssertScriptSuccess_L("x = DataFrame('a', 1:5, 'b', 11:15); m1 = x.asMatrix(); m2 = matrix(c(1:5, 11:15), ncol=2, byrow=F); identical(m1, m2);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:5, 'a', 11:15); m1 = x.asMatrix(); m2 = matrix(c(1:5, 11:15), ncol=2, byrow=F); identical(m1, m2);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 11:15, 'a', 1:5); m1 = x.asMatrix(); m2 = matrix(c(11:15, 1:5), ncol=2, byrow=F); identical(m1, m2);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('a', 11:15, 'b', 1:5); m1 = x.asMatrix(); m2 = matrix(c(11:15, 1:5), ncol=2, byrow=F); identical(m1, m2);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 11.0:15, 'a', 1.0:5); m1 = x.asMatrix(); m2 = matrix(c(11.0:15, 1.0:5), ncol=2, byrow=F); identical(m1, m2);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', c('foo','bar'), 'a', c('baz','barbaz')); m1 = x.asMatrix(); m2 = matrix(c('foo','bar','baz','barbaz'), ncol=2, byrow=F); identical(m1, m2);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', c(T,T,F), 'a', c(F,T,F)); m1 = x.asMatrix(); m2 = matrix(c(T,T,F,F,T,F), ncol=2, byrow=F); identical(m1, m2);", true);
	EidosAssertScriptSuccess_L("d1 = Dictionary('foo', 1:8); d2 = Dictionary('baz', 11:18); x = DataFrame('b', d1, 'a', d2); m1 = x.asMatrix(); m2 = matrix(c(d1, d2), ncol=2, byrow=F); identical(m1, m2);", true);
	
	// DataFrame cbind()
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); y = DataFrame(); y.cbind(x); y.identicalContents(x);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); y = DataFrame('c', 2.0:4); y.cbind(x); DataFrame('c', 2.0:4, 'b', 1:3, 'a', c(T,F,T)).identicalContents(y);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); y = DataFrame('c', 2.0:4); x.cbind(y); DataFrame('b', 1:3, 'a', c(T,F,T), 'c', 2.0:4).identicalContents(x);", true);
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); y = DataFrame('c', 2.0:5); x.cbind(y);", 69, "inconsistent column sizes");
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.cbind(x);", 42, "already exists");
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); y = DataFrame('a', 2.0:4); x.cbind(y);", 69, "already exists");
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3); y = DataFrame('c', 2.0:4); z = DataFrame('a', c(T,F,T)); x.cbind(y, z); DataFrame('b', 1:3, 'c', 2.0:4, 'a', c(T,F,T)).identicalContents(x);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3); y = DataFrame('c', 2.0:4); z = DataFrame('a', c(T,F,T)); x.cbind(c(y, z)); DataFrame('b', 1:3, 'c', 2.0:4, 'a', c(T,F,T)).identicalContents(x);", true);
	
	// DataFrame rbind()
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); y = DataFrame(); y.rbind(x); y.identicalContents(x);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); y = DataFrame('b', 4:5, 'a', c(T,F)); x.rbind(y); DataFrame('b', 1:5, 'a', c(T,F,T,T,F)).identicalContents(x);", true);
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); y = Dictionary('b', 4:5, 'a', c(T,F)); x.rbind(y);", 81, "do not match the columns");
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); y = DataFrame('a', 4:5, 'b', c(T,F)); x.rbind(y);", 80, "do not match the columns");
	EidosAssertScriptSuccess_L("x = DataFrame('a', 1:3, 'b', c(T,F,T)); y = Dictionary('a', 4:5, 'b', c(T,F)); x.rbind(y); DataFrame('a', 1:5, 'b', c(T,F,T,T,F)).identicalContents(x);", true);
	EidosAssertScriptRaise("x = DataFrame('a', 1:3, 'b', c(T,F,T)); y = Dictionary('a', 4:5, 'b', F); x.rbind(y);", 76, "inconsistent column sizes");
	EidosAssertScriptRaise("x = DataFrame('a', 1:3, 'b', c(T,F,T)); x.rbind(x);", 42, "to itself");
	EidosAssertScriptSuccess_L("x = DataFrame('a', 1:3, 'b', c(T,F,T)); y = DataFrame(x); x.rbind(y); DataFrame('a', c(1:3,1:3), 'b', c(T,F,T,T,F,T)).identicalContents(x);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('a', 1:3, 'b', c(T,F,T)); y = DataFrame(x); x.rbind(c(y, y), y); DataFrame('a', c(1:3,1:3,1:3,1:3), 'b', c(T,F,T,T,F,T,T,F,T,T,F,T)).identicalContents(x);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); y = DataFrame('b', 4.0:5, 'a', c(T,F)); x.rbind(y); DataFrame('b', 1.0:5, 'a', c(T,F,T,T,F)).identicalContents(x);", true);
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); y = DataFrame('b', Dictionary(), 'a', T); x.rbind(y);", 84, "cannot be mixed");
	
	// DataFrame subset()
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset().identicalContents(x);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(rows=0).identicalContents(DataFrame('b', 1, 'a', T));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(rows=1).identicalContents(DataFrame('b', 2, 'a', F));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(rows=2).identicalContents(DataFrame('b', 3, 'a', T));", true);
	EidosAssertScriptSuccess_IV("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(cols=0);", {1, 2, 3});
	EidosAssertScriptSuccess_LV("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(cols=1);", {true, false, true});
	
	EidosAssertScriptSuccess_I("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(0, 0);", 1);
	EidosAssertScriptSuccess_I("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(1, 0);", 2);
	EidosAssertScriptSuccess_I("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(0, 'b');", 1);
	EidosAssertScriptSuccess_I("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(1, 'b');", 2);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(1, 1);", false);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(2, 1);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(1, 'a');", false);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(2, 'a');", true);
	EidosAssertScriptSuccess_IV("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(1:2, 0);", {2, 3});
	EidosAssertScriptSuccess_IV("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(2:1, 0);", {3, 2});
	EidosAssertScriptSuccess_IV("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(c(F, T, T), 0);", {2, 3});
	EidosAssertScriptSuccess_IV("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(c(T, T, T), 0);", {1, 2, 3});
	EidosAssertScriptSuccess_IV("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(c(F, F, F), 0);", {});
	EidosAssertScriptSuccess_IV("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(integer(0), 0);", {});
	EidosAssertScriptSuccess_IV("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(1:2, c(T, F));", {2, 3});
	EidosAssertScriptSuccess_LV("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(1:2, c(F, T));", {false, true});
	EidosAssertScriptSuccess_LV("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(2:1, c(F, T));", {true, false});
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(2:1, integer(0)).identicalContents(DataFrame());", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(1, 0:1).identicalContents(DataFrame('b', 2, 'a', F));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(1, 1:0).identicalContents(DataFrame('a', F, 'b', 2));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(1, c(T, T)).identicalContents(DataFrame('b', 2, 'a', F));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(1, c('b', 'a')).identicalContents(DataFrame('b', 2, 'a', F));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(1, c('a', 'b')).identicalContents(DataFrame('a', F, 'b', 2));", true);
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(4, 0);", 42, "out-of-range index");
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(0, 4);", 42, "out of range");
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(T, 0);", 42, "logical index operand must match");
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(0, T);", 42, "logical index vector length does not match");
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subset(0, 'c');", 42, "key c is not defined");
	
	// DataFrame subsetColumns()
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(integer(0)).identicalContents(DataFrame());", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(c(F,F)).identicalContents(DataFrame());", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(string(0)).identicalContents(DataFrame());", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(0).identicalContents(DataFrame('b', 1:3));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(1).identicalContents(DataFrame('a', c(T,F,T)));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(c(T,F)).identicalContents(DataFrame('b', 1:3));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(c(F,T)).identicalContents(DataFrame('a', c(T,F,T)));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns('b').identicalContents(DataFrame('b', 1:3));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns('a').identicalContents(DataFrame('a', c(T,F,T)));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(0:1).identicalContents(x);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(c(T,T)).identicalContents(x);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(c('b','a')).identicalContents(x);", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(1:0).identicalContents(DataFrame('a', c(T,F,T), 'b', 1:3));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(c('a','b')).identicalContents(DataFrame('a', c(T,F,T), 'b', 1:3));", true);
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(4);", 42, "out of range");
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns(T);", 42, "logical index vector length does not match");
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetColumns('c');", 42, "key c is not defined");
	
	// DataFrame subsetRows()
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(integer(0)).identicalContents(DataFrame('b', integer(0), 'a', logical(0)));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(c(F,F,F)).identicalContents(DataFrame('b', integer(0), 'a', logical(0)));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(0).identicalContents(DataFrame('b', 1, 'a', T));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(1).identicalContents(DataFrame('b', 2, 'a', F));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(2).identicalContents(DataFrame('b', 3, 'a', T));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(c(T,F,F)).identicalContents(DataFrame('b', 1, 'a', T));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(c(F,T,F)).identicalContents(DataFrame('b', 2, 'a', F));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(c(F,F,T)).identicalContents(DataFrame('b', 3, 'a', T));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(0:1).identicalContents(DataFrame('b', 1:2, 'a', c(T,F)));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(c(T,T,F)).identicalContents(DataFrame('b', 1:2, 'a', c(T,F)));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(1:0).identicalContents(DataFrame('b', 2:1, 'a', c(F,T)));", true);
	EidosAssertScriptSuccess_L("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(0:2).identicalContents(x);", true);
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(4);", 42, "out-of-range");
	EidosAssertScriptRaise("x = DataFrame('b', 1:3, 'a', c(T,F,T)); x.subsetRows(T);", 42, "logical index operand must match");
	
	if (Eidos_TemporaryDirectoryExists())
	{
		// DataFrame serialize and readCSV() round-trip; tests that specify column types explicitly work without <regex>, the rest don't run if it is broken
		EidosAssertScriptRaise("x = Dictionary('a', c(T, T, F), 'b', 3:4); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file, colTypes='li');", 112, "could not be represented");
		
		EidosAssertScriptSuccess_L("x = DataFrame('a', 3:5); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file, colTypes='i'); DataFrame('a', 3:5).identicalContents(y);", true);
		EidosAssertScriptSuccess_L("x = DataFrame('a', 3:5); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file, colTypes='f'); DataFrame('a', 3.0:5).identicalContents(y);", true);
		EidosAssertScriptSuccess_L("x = DataFrame('a', 3:5); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file, colTypes='s'); DataFrame('a', asString(3:5)).identicalContents(y);", true);
		EidosAssertScriptSuccess_L("x = Dictionary('a', 3:5, 'b', 3:4); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file, colTypes='i_'); Dictionary('a', 3:5).identicalContents(y);", true);
		EidosAssertScriptSuccess_L("x = Dictionary('a', 3:5, 'b', 3:4); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file, colTypes='i-'); Dictionary('a', 3:5).identicalContents(y);", true);
		
		EidosAssertScriptRaise("x = Dictionary('a', c(T, T, F), 'b', 3:4); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, colTypes='li', sep='\t');", 112, "could not be represented");
		
		EidosAssertScriptSuccess_L("x = DataFrame('a', 3:5); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, colTypes='i', sep='\t'); DataFrame('a', 3:5).identicalContents(y);", true);
		EidosAssertScriptSuccess_L("x = DataFrame('a', 3:5); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, colTypes='f', sep='\t'); DataFrame('a', 3.0:5).identicalContents(y);", true);
		EidosAssertScriptSuccess_L("x = DataFrame('a', 3:5); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, colTypes='s', sep='\t'); DataFrame('a', asString(3:5)).identicalContents(y);", true);
		EidosAssertScriptSuccess_L("x = Dictionary('a', 3:5, 'b', 3:4); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, colTypes='i_', sep='\t'); Dictionary('a', 3:5).identicalContents(y);", true);
		EidosAssertScriptSuccess_L("x = Dictionary('a', 3:5, 'b', 3:4); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, colTypes='i-', sep='\t'); Dictionary('a', 3:5).identicalContents(y);", true);

		if (!Eidos_RegexWorks())
		{
			// already warned about this in _RunStringManipulationTests()
			//std::cout << "WARNING: This build of Eidos does not have a working <regex> library, due to a bug in the underlying C++ standard library provided by the system.  This may cause problems with the Eidos functions grep() and readCSV(); if you do not use those functions, it should not affect you.  If a case where a problem does occur is encountered, an error will result.  This problem might be resolved by updating your compiler or toolchain, or by upgrading to a more recent version of your operating system." << std::endl;
		}
		else
		{
			EidosAssertScriptSuccess_L("x = DataFrame('a', 1:3); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', 1.0:3); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', c('foo', 'bar', 'baz')); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', c(T, T, F)); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', c(T, T, F), 'b', 3:5); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('b', c(T, T, F), 'a', 3:5); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = Dictionary('a', c(T, T, F), 'b', 3:5); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = Dictionary('a', c(T, T, F), 'b', 3:4); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file); x.identicalContents(y);", false);
			EidosAssertScriptSuccess_L("x = Dictionary('a', c(T, T, F), 'b', 3:4); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file); Dictionary('a', c(T, T, F), 'b', c('3','4','')).identicalContents(y);", true);
			
			EidosAssertScriptSuccess_L("x = DataFrame('a', c('foo', 'bar')); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file, colNames=F); DataFrame('X1', c('a', 'foo', 'bar')).identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', c('foo', 'bar')); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file, colNames='b'); DataFrame('b', c('a', 'foo', 'bar')).identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', 3:5); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file); DataFrame('a', 3:5).identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', 3:5); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file, colTypes='?'); DataFrame('a', 3:5).identicalContents(y);", true);
			
			EidosAssertScriptSuccess_L("x = DataFrame('a', 1:3); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, sep='\t'); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', 1.0:3); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, sep='\t'); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', c('foo', 'bar', 'baz')); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, sep='\t'); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', c(T, T, F)); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, sep='\t'); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', c(T, T, F), 'b', 3:5); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, sep='\t'); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('b', c(T, T, F), 'a', 3:5); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, sep='\t'); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = Dictionary('a', c(T, T, F), 'b', 3:5); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, sep='\t'); x.identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = Dictionary('a', c(T, T, F), 'b', 3:4); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, sep='\t'); x.identicalContents(y);", false);
			EidosAssertScriptSuccess_L("x = Dictionary('a', c(T, T, F), 'b', 3:4); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, sep='\t'); Dictionary('a', c(T, T, F), 'b', c('3','4','')).identicalContents(y);", true);
			
			EidosAssertScriptSuccess_L("x = DataFrame('a', c('foo', 'bar')); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, colNames=F, sep='\t'); DataFrame('X1', c('a', 'foo', 'bar')).identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', c('foo', 'bar')); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, colNames='b', sep='\t'); DataFrame('b', c('a', 'foo', 'bar')).identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', 3:5); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, sep='\t'); DataFrame('a', 3:5).identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = DataFrame('a', 3:5); file = writeTempFile('eidos_test_', '.tsv', x.serialize('tsv')); y = readCSV(file, colTypes='?', sep='\t'); DataFrame('a', 3:5).identicalContents(y);", true);
			
			EidosAssertScriptSuccess_L(R"V0G0N(x = Dictionary('a', 3:6, 'b', c(121,131,141,141141)); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file, quote='1'); Dictionary('"a"', 3:6, '"b"', c(2:4, 414)).identicalContents(y);)V0G0N", true);
			EidosAssertScriptSuccess_L("x = Dictionary('b', c('10$25', '10$0', '10$')); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file, dec='$'); Dictionary('b', c(10.25, 10, 10)).identicalContents(y);", true);
			EidosAssertScriptSuccess_L("x = Dictionary('a', c('foo', 'bar'), 'b', c(10.5, 10.25)); file = writeTempFile('eidos_test_', '.csv', x.serialize('csv')); y = readCSV(file, dec='$', comment='.'); Dictionary('a', c('foo', 'bar'), 'b', c(10, 10)).identicalContents(y);", true);
			
			// test sep="" whitespace separator)
			EidosAssertScriptSuccess_L("file = writeTempFile('eidos_test_', '.csv', c('  a   b   c   d   e', '   1   2   3   4   5   ', ' 10  20  30  40  50', '100 200 300 400 500')); y = readCSV(file, sep=''); Dictionary('a', c(1,10,100), 'b', c(2,20,200), 'c', c(3,30,300), 'd', c(4,40,400), 'e', c(5,50,500)).identicalContents(y);", true);
		}
	}
	
	// Test EidosDictionary's interaction with retain-released and non-retain-released objects using EidosTestElement and EidosTestElementNRR
	// Note that these tests will leak instances of EidosTestElementNRR; since it is not under retain-release there is no way to know when to release it!
	// They will also cause warning to be emitted to the console, so they are disabled by default; but they worked last time I checked
#if 0
	EidosAssertScriptSuccess_L("_Test(5000); T;", true);
	EidosAssertScriptSuccess_L("_TestNRR(5001); T;", true);
	EidosAssertScriptSuccess_L("x = _Test(5002); T;", true);
	EidosAssertScriptSuccess_L("x = _TestNRR(5003); T;", true);
	EidosAssertScriptSuccess_L("x = _Test(5004); x = 5; T;", true);
	EidosAssertScriptSuccess_L("x = _TestNRR(5005); x = 5; T;", true);
	EidosAssertScriptSuccess_L("x = _Test(5006); y = Dictionary('a', x); T;", true);
	EidosAssertScriptSuccess_L("x = _TestNRR(5007); y = Dictionary('a', x); T;", true);											// logs - y references x
	EidosAssertScriptSuccess_L("x = _Test(5008); y = Dictionary('a', x); y = 5; T;", true);
	EidosAssertScriptSuccess_L("x = _TestNRR(5009); y = Dictionary('a', x); y = 5; T;", true);
	EidosAssertScriptSuccess_L("x = _Test(5010); y = Dictionary('a', x); z = Dictionary(y); y = 5; T;", true);
	EidosAssertScriptSuccess_L("x = _TestNRR(5011); y = Dictionary('a', x); z = Dictionary(y); y = 5; T;", true);				// logs - z references x (copied from y)
	EidosAssertScriptSuccess_L("x = _Test(5011); y = Dictionary('a', x); z = Dictionary(y); y = 5; z = 5; T;", true);
	EidosAssertScriptSuccess_L("x = _TestNRR(5012); y = Dictionary('a', x); z = Dictionary(y); y = 5; z = 5; T;", true);
	EidosAssertScriptSuccess_L("x = _Test(5013); y = Dictionary('a', x); z = Dictionary('b', y); y = 5; T;", true);
	EidosAssertScriptSuccess_L("x = _TestNRR(5014); y = Dictionary('a', x); z = Dictionary('b', y); y = 5; T;", true);			// logs - z retains y, which references x
	EidosAssertScriptSuccess_L("x = _Test(5015); y = Dictionary('a', x); z = Dictionary('b', y); y = 5; z = 5; T;", true);
	EidosAssertScriptSuccess_L("x = _TestNRR(5016); y = Dictionary('a', x); z = Dictionary('b', y); y = 5; z = 5; T;", true);
#endif
	
	// Test EidosImage properties and methods – but how?  If it were possible to construct an Image from a matrix, that would provide an avenue for testing...
	// That is what we do here now, but we can only test grayscale images since we can only generate grayscale images, at present... FIXME
	EidosAssertScriptSuccess_L("m = matrix(0:14, nrow=3, ncol=5); i = Image(m); i.bitsPerChannel == 8;", true);
	EidosAssertScriptSuccess_L("m = matrix(0:14, nrow=3, ncol=5); i = Image(m); i.height == 3;", true);
	EidosAssertScriptSuccess_L("m = matrix(0:14, nrow=3, ncol=5); i = Image(m); i.width == 5;", true);
	EidosAssertScriptSuccess_L("m = matrix(0:14, nrow=3, ncol=5); i = Image(m); i.isGrayscale == T;", true);
	EidosAssertScriptSuccess_L("m = matrix(0:14, nrow=3, ncol=5); i = Image(m); identical(i.integerK, m);", true);
	EidosAssertScriptRaise("m = matrix(0:14, nrow=3, ncol=5); i = Image(m); i.integerR;", 50, "from a grayscale");
	EidosAssertScriptRaise("m = matrix(0:14, nrow=3, ncol=5); i = Image(m); i.integerG;", 50, "from a grayscale");
	EidosAssertScriptRaise("m = matrix(0:14, nrow=3, ncol=5); i = Image(m); i.integerB;", 50, "from a grayscale");
	EidosAssertScriptSuccess_L("m = matrix(0:14, nrow=3, ncol=5); i = Image(m); identical(i.floatK, m/255);", true);
	EidosAssertScriptRaise("m = matrix(0:14, nrow=3, ncol=5); i = Image(m); i.floatR;", 50, "from a grayscale");
	EidosAssertScriptRaise("m = matrix(0:14, nrow=3, ncol=5); i = Image(m); i.floatG;", 50, "from a grayscale");
	EidosAssertScriptRaise("m = matrix(0:14, nrow=3, ncol=5); i = Image(m); i.floatB;", 50, "from a grayscale");
	
	if (Eidos_TemporaryDirectoryExists())
	{
		EidosAssertScriptSuccess_L("m = matrix(0:14, nrow=3, ncol=5); i = Image(m); path = '" + temp_path + "/image_write.png'; i.write(path); i2 = Image(path); identical(m, i2.integerK);", true);
	}
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
	EidosAssertScriptSuccess_I("function (i)plus(i x) { return x + 1; } plus(5);", 6);
	EidosAssertScriptSuccess_F("function (f)plus(f x) { return x + 1; } plus(5.0);", 6.0);
	EidosAssertScriptSuccess_I("function (fi)plus(fi x) { return x + 1; } plus(5);", 6);
	EidosAssertScriptSuccess_F("function (fi)plus(fi x) { return x + 1; } plus(5.0);", 6.0);
	EidosAssertScriptSuccess_IV("function (fi)plus(fi x) { return x + 1; } plus(c(5, 6, 7));", {6, 7, 8});
	EidosAssertScriptSuccess_FV("function (fi)plus(fi x) { return x + 1; } plus(c(5.0, 6.0, 7.0));", {6.0, 7.0, 8.0});
	
	EidosAssertScriptSuccess_L("function (l$)nor(l$ x, l$ y) { return !(x | y); } nor(F, F);", true);
	EidosAssertScriptSuccess_L("function (l$)nor(l$ x, l$ y) { return !(x | y); } nor(T, F);", false);
	EidosAssertScriptSuccess_L("function (l$)nor(l$ x, l$ y) { return !(x | y); } nor(F, T);", false);
	EidosAssertScriptSuccess_L("function (l$)nor(l$ x, l$ y) { return !(x | y); } nor(T, T);", false);
	
	EidosAssertScriptSuccess_S("function (s)append(s x, s y) { return x + ',' + y; } append('foo', 'bar');", "foo,bar");
	EidosAssertScriptSuccess_SV("function (s)append(s x, s y) { return x + ',' + y; } append('foo', c('bar','baz'));", {"foo,bar", "foo,baz"});
	
	// Default arguments
	EidosAssertScriptSuccess_IV("function (fi)plus([fi x = 2]) { return x + 1; } plus(c(5, 6, 7));", {6, 7, 8});
	EidosAssertScriptSuccess_I("function (fi)plus([fi x = 2]) { return x + 1; } plus();", 3);
	EidosAssertScriptSuccess_IV("function (fi)plus([fi x = -2]) { return x + 1; } plus(c(5, 6, 7));", {6, 7, 8});
	EidosAssertScriptSuccess_I("function (fi)plus([fi x = -2]) { return x + 1; } plus();", -1);
	
	EidosAssertScriptSuccess_FV("function (fi)plus([fi x = 2.0]) { return x + 1; } plus(c(5.0, 6.0, 7.0));", {6.0, 7.0, 8.0});
	EidosAssertScriptSuccess_F("function (fi)plus([fi x = 2.0]) { return x + 1; } plus();", 3.0);
	EidosAssertScriptSuccess_FV("function (fi)plus([fi x = -2.0]) { return x + 1; } plus(c(5.0, 6.0, 7.0));", {6.0, 7.0, 8.0});
	EidosAssertScriptSuccess_F("function (fi)plus([fi x = -2.0]) { return x + 1; } plus();", -1.0);
	
	EidosAssertScriptSuccess_SV("function (s)append(s x, [s y = 'foo']) { return x + ',' + y; } append('foo', c('bar','baz'));", {"foo,bar", "foo,baz"});
	EidosAssertScriptSuccess_SV("function (s)append(s x, [s y = 'foo']) { return x + ',' + y; } append('bar');", {"bar,foo"});
	
	EidosAssertScriptSuccess_LV("function (l)or(l x, [l y = T]) { return x | y; } or(c(T, F, T, F), T);", {true, true, true, true});
	EidosAssertScriptSuccess_LV("function (l)or(l x, [l y = T]) { return x | y; } or(c(T, F, T, F), F);", {true, false, true, false});
	EidosAssertScriptSuccess_LV("function (l)or(l x, [l y = T]) { return x | y; } or(c(T, F, T, F));", {true, true, true, true});
	EidosAssertScriptSuccess_LV("function (l)or(l x, [l y = F]) { return x | y; } or(c(T, F, T, F), T);", {true, true, true, true});
	EidosAssertScriptSuccess_LV("function (l)or(l x, [l y = F]) { return x | y; } or(c(T, F, T, F), F);", {true, false, true, false});
	EidosAssertScriptSuccess_LV("function (l)or(l x, [l y = F]) { return x | y; } or(c(T, F, T, F));", {true, false, true, false});
	
	EidosAssertScriptRaise("function (fi)plus([fi x = FOO]) { return x + 1; } plus();", 26, "default value must be");
	EidosAssertScriptRaise("function (fi)plus([fi x = 9223372036854775808]) { return x + 1; } plus();", 26, "could not be represented");
	EidosAssertScriptRaise("function (fi)plus([fi x = -FOO]) { return x + 1; } plus();", 27, "unexpected token");
	
	// Recursion
	EidosAssertScriptSuccess_I("function (i)fac([i b=10]) { if (b <= 1) return 1; else return b*fac(b-1); } fac(3); ", 6);
	EidosAssertScriptSuccess_I("function (i)fac([i b=10]) { if (b <= 1) return 1; else return b*fac(b-1); } fac(5); ", 120);
	EidosAssertScriptSuccess_I("function (i)fac([i b=10]) { if (b <= 1) return 1; else return b*fac(b-1); } fac(); ", 3628800);
	
	EidosAssertScriptSuccess_S("function (s)star(i x) { if (x <= 0) return ''; else return '*' + star(x - 1); } star(5); ", "*****");
	EidosAssertScriptSuccess_S("function (s)star(i x) { if (x <= 0) return ''; else return '*' + star(x - 1); } star(10); ", "**********");
	EidosAssertScriptSuccess_S("function (s)star(i x) { if (x <= 0) return ''; else return '*' + star(x - 1); } star(0); ", "");
	
	EidosAssertScriptSuccess_I("function (i)fib(i x) { if (x <= 1) return x; else return fib(x - 1) + fib(x - 2); } fib(10);", 55);
	
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
	EidosAssertScriptSuccess_I("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return foo(x - 1); } foo(5); ", 16);
	EidosAssertScriptSuccess_I("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return foo(x - 1); } foo(10); ", 56);
	EidosAssertScriptSuccess_I("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return foo(x - 1); } foo(-10); ", -9);
	
	EidosAssertScriptSuccess_I("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return baz(x - 1); } function (i)baz(i x) { return x * foo(x); } foo(5); ", 153);
	EidosAssertScriptSuccess_I("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return baz(x - 1); } function (i)baz(i x) { return x * foo(x); } foo(10); ", 2335699);
	EidosAssertScriptSuccess_I("function (i)foo(i x) { return x + bar(x); } function (i)bar(i x) { if (x <= 1) return 1; else return baz(x - 1); } function (i)baz(i x) { return x * foo(x); } foo(-10); ", -9);
	
	// Scoping, defineConstant(), and defineGlobal()
	EidosAssertScriptRaise("defineConstant('x', 10); function (i)plus(i x) { return x + 1; } plus(5);", 65, "cannot be redefined because it is a constant");
	EidosAssertScriptRaise("defineConstant('x', 10); function (i)plus(i y) { x = y + 1; return x; } plus(5);", 72, "cannot be redefined because it is a constant");
	EidosAssertScriptSuccess_I("defineConstant('x', 10); function (i)plus(i y) { return x + y; } plus(5);", 15);
	EidosAssertScriptSuccess_I("x = 10; function (i)plus(i y) { return x + y; } plus(5);", 15);
	EidosAssertScriptSuccess_I("defineConstant('x', 10); y = 1; function (i)plus(i y) { return x + y; } plus(5);", 15);
	EidosAssertScriptSuccess_I("defineConstant('x', 10); y = 1; function (i)plus(i y) { return x + y; } plus(5); y; ", 1);
	EidosAssertScriptSuccess_I("defineConstant('x', 10); y = 1; function (i)plus(i y) { y = y + 1; return x + y; } plus(5); ", 16);
	EidosAssertScriptSuccess_I("defineConstant('x', 10); y = 1; function (i)plus(i y) { y = y + 1; return x + y; } plus(5); y; ", 1);
	EidosAssertScriptSuccess_I("function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); ", 6);
	EidosAssertScriptSuccess_I("function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); x; ", 10);
	EidosAssertScriptRaise("function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); y; ", 81, "undefined identifier y");
	EidosAssertScriptRaise("function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); plus(5); ", 81, "identifier 'x' is already defined");
	EidosAssertScriptRaise("x = 3; function (i)plus(i y) { defineConstant('x', 10); y = y + 1; return y; } plus(5); x; ", 79, "identifier 'x' is already defined");
	EidosAssertScriptSuccess_I("function (i)plus(i y) { foo(); y = y + 1; return y; } function (void)foo(void) { defineConstant('x', 10); } plus(5); x; ", 10);
	EidosAssertScriptRaise("function (i)plus(i x) { foo(); x = x + 1; return x; } function (void)foo(void) { defineConstant('x', 10); } plus(5); x; ", 108, "identifier 'x' is already defined");
	EidosAssertScriptRaise("x = 3; function (i)plus(i y) { foo(); y = y + 1; return y; } function (void)foo(void) { defineConstant('x', 10); } plus(5); x; ", 115, "identifier 'x' is already defined");
	EidosAssertScriptSuccess_I("function (i)plus(i y) { foo(y); y = y + 1; return y; } function (void)foo(i y) { y = 12; } plus(5); ", 6);
	EidosAssertScriptRaise("function (i)plus(i y) { foo(y); y = y + 1; return y; } function (void)foo(i y) { y = 12; } plus(5); y; ", 100, "undefined identifier y");
	EidosAssertScriptSuccess_I("function (i)plus(i y) { foo(y); y = y + 1; return y; } function (void)foo(i x) { y = 12; } plus(5); ", 6);
	EidosAssertScriptRaise("function (i)plus(i y) { foo(y); y = y + 1; return y; } function (void)foo(i x) { y = 12; } plus(5); y; ", 100, "undefined identifier y");
	
	EidosAssertScriptSuccess_I("x = 15; x;", 15);
	EidosAssertScriptSuccess_I("defineGlobal('x', 15); x;", 15);
	EidosAssertScriptSuccess_I("x = 5; defineGlobal('x', 15); x;", 15);
	EidosAssertScriptSuccess_I("defineGlobal('x', 15); x = 5; x;", 5);
	EidosAssertScriptSuccess_I("x = 5; defineGlobal('x', 15); defineGlobal('x', 25); x;", 25);
	EidosAssertScriptSuccess_I("x = 5; defineGlobal('x', 15); x = 3; defineGlobal('x', 25); x;", 25);
	EidosAssertScriptSuccess_I("x = 15; function (i)foo(void) { return x; } foo();", 15);
	EidosAssertScriptSuccess_I("x = 15; function (i)foo(void) { x = 5; return x; } foo();", 5);
	EidosAssertScriptSuccess_I("x = 15; function (i)foo(void) { x = 5; return x; } foo(); x;", 15);
	EidosAssertScriptSuccess_I("x = 15; function (i)foo(void) { defineGlobal('x', 5); return 25; } foo();", 25);
	EidosAssertScriptSuccess_I("x = 15; function (i)foo(void) { defineGlobal('x', 5); return x; } foo();", 5);
	EidosAssertScriptSuccess_I("x = 15; function (i)foo(void) { defineGlobal('x', 5); return 25; } foo(); x;", 5);
	EidosAssertScriptSuccess_I("x = 15; function (i)foo(void) { y = x; defineGlobal('x', 5); return y; } foo();", 15);
	EidosAssertScriptSuccess_I("x = 15; function (i)foo(void) { y = x; defineGlobal('y', 25); return y; } foo();", 15);
	EidosAssertScriptSuccess_I("x = 15; function (i)foo(void) { y = x; defineGlobal('y', 25); return y; } foo(); y;", 25);
	EidosAssertScriptRaise("x = 15; function (i)foo(void) { y = x; return y; } foo(); y;", 58, "undefined identifier y");
	
	EidosAssertScriptRaise("x = 5; defineConstant('x', 10);", 7, "already defined");
	EidosAssertScriptRaise("defineConstant('x', 10); x = 5;", 27, "is a constant");
	EidosAssertScriptRaise("defineConstant('x', 10); defineConstant('x', 5);", 25, "already defined");
	EidosAssertScriptRaise("x = 5; function(void)foo(void) { defineConstant('x', 10); } foo();", 60, "already defined");
	EidosAssertScriptRaise("defineConstant('x', 10); function(void)foo(void) { x = 5; } foo();", 60, "is a constant");
	EidosAssertScriptRaise("defineConstant('x', 10); function(void)foo(void) { defineConstant('x', 5); } foo();", 77, "already defined");
	EidosAssertScriptRaise("function(void)foo(void) { defineConstant('x', 10); } foo(); x = 5;", 62, "is a constant");
	EidosAssertScriptRaise("function(void)foo(void) { x = 5; } foo(); defineConstant('x', 10); foo();", 67, "is a constant");
	EidosAssertScriptRaise("function(void)foo(void) { defineConstant('x', 5); } foo(); defineConstant('x', 10);", 59, "already defined");
	
	EidosAssertScriptRaise("defineGlobal('x', 5); defineConstant('x', 10);", 22, "already defined");
	EidosAssertScriptRaise("defineConstant('x', 10); defineGlobal('x', 5);", 25, "is a constant");
	EidosAssertScriptRaise("defineGlobal('x', 5); function(void)foo(void) { defineConstant('x', 10); } foo();", 75, "already defined");
	EidosAssertScriptRaise("defineConstant('x', 10); function(void)foo(void) { defineGlobal('x', 5); } foo();", 75, "is a constant");
	EidosAssertScriptRaise("function(void)foo(void) { defineConstant('x', 10); } foo(); defineGlobal('x', 5);", 60, "is a constant");
	EidosAssertScriptRaise("function(void)foo(void) { defineGlobal('x', 5); } foo(); defineConstant('x', 10); foo();", 57, "already defined");
	
	// Mutual recursion with lambdas
	
	
	// Tests mimicking built-in Eidos functions; these are good for testing user-defined functions, but also good for testing our built-ins!
	const std::string &builtins_test_string =
#include "eidos_test_builtins.h"
	;
	{
		std::vector<std::string> test_strings = Eidos_string_split(builtins_test_string, "// ***********************************************************************************************");
		
		//for (int testidx = 0; testidx < 100; testidx++)	// uncomment this for a more thorough stress test
		{
			for (std::string &test_string : test_strings)
			{
				std::string test_string_fixed = test_string + "\nreturn T;\n";
				
				EidosAssertScriptSuccess_L(test_string_fixed, true);
			}
		}
	}
	
	// Tests of parallelization of Eidos functions; this is here just because the above test is here
#ifdef _OPENMP
	const std::string &parallelization_test_string =
#include "eidos_test_parallel.h"
	;
	{
		std::vector<std::string> test_strings = Eidos_string_split(parallelization_test_string, "// ***********************************************************************************************");
		
		//for (int testidx = 0; testidx < 100; testidx++)	// uncomment this for a more thorough stress test
		{
			for (std::string &test_string : test_strings)
			{
				std::string test_string_fixed = test_string + "\nreturn T;\n";
				
				// Note that we ensure that we are using the maximum number of threads at start & end
				gEidosNumThreads = gEidosMaxThreads;
				gEidosNumThreadsOverride = false;
				omp_set_num_threads(gEidosMaxThreads);
				
				EidosAssertScriptSuccess_L(test_string_fixed, true);
				
				gEidosNumThreads = gEidosMaxThreads;
				gEidosNumThreadsOverride = false;
				omp_set_num_threads(gEidosMaxThreads);
			}
		}
	}
#endif
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
	EidosAssertScriptSuccess_VOID("function (void)foo(void) { 5; } foo();");
	EidosAssertScriptSuccess_VOID("function (void)foo(void) { 5; return; } foo();");
	EidosAssertScriptRaise("function (void)foo(void) { return 5; } foo();", 39, "return value must be void");
	EidosAssertScriptRaise("function (void)foo(void) { return NULL; } foo();", 42, "return value must be void");
	
	// functions declared to return NULL must return NULL
	EidosAssertScriptRaise("function (NULL)foo(void) { 5; } foo();", 32, "return value cannot be void");
	EidosAssertScriptRaise("function (NULL)foo(void) { 5; return; } foo();", 40, "return value cannot be void");
	EidosAssertScriptRaise("function (NULL)foo(void) { return 5; } foo();", 39, "return value cannot be type integer");
	EidosAssertScriptSuccess_NULL("function (NULL)foo(void) { return NULL; } foo();");
	
	// functions declared to return * may return anything but void
	EidosAssertScriptRaise("function (*)foo(void) { 5; } foo();", 29, "return value cannot be void");
	EidosAssertScriptRaise("function (*)foo(void) { 5; return; } foo();", 37, "return value cannot be void");
	EidosAssertScriptSuccess_I("function (*)foo(void) { return 5; } foo();", 5);
	EidosAssertScriptSuccess_NULL("function (*)foo(void) { return NULL; } foo();");
	
	// functions declared to return vNlifso may return anything at all
	EidosAssertScriptSuccess_VOID("function (vNlifso)foo(void) { 5; } foo();");
	EidosAssertScriptSuccess_VOID("function (vNlifso)foo(void) { 5; return; } foo();");
	EidosAssertScriptSuccess_I("function (vNlifso)foo(void) { return 5; } foo();", 5);
	EidosAssertScriptSuccess_NULL("function (vNlifso)foo(void) { return NULL; } foo();");
	
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
	EidosAssertScriptSuccess_VOID("function (void)foo(NULL x) { return; } foo(NULL);");
	EidosAssertScriptSuccess_VOID("function (void)bar([NULL x = NULL]) { return; } bar(NULL);");
	EidosAssertScriptSuccess_VOID("function (void)bar([NULL x = NULL]) { return; } bar();");
	EidosAssertScriptSuccess_NULL("function (NULL)foo(NULL x) { return x; } foo(NULL);");
	EidosAssertScriptSuccess_NULL("function (NULL)bar([NULL x = NULL]) { return x; } bar(NULL);");
	EidosAssertScriptSuccess_NULL("function (NULL)bar([NULL x = NULL]) { return x; } bar();");
	
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
	EidosAssertScriptSuccess_VOID("(citation());");				// about the only thing that is legal with void!
	
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
	
	EidosAssertScriptSuccess_VOID("T ? citation() else F;");		// also legal with void, as long as you don't try to use the result...
	EidosAssertScriptSuccess_L("F ? citation() else F;", false);
	EidosAssertScriptSuccess_L("T ? F else citation();", false);
	EidosAssertScriptSuccess_VOID("F ? F else citation();");
	EidosAssertScriptSuccess_VOID("T ? citation() else citation();");
	EidosAssertScriptSuccess_VOID("F ? citation() else citation();");
	EidosAssertScriptRaise("citation() ? T else F;", 11, "size() != 1");
	
	EidosAssertScriptRaise("x = citation();", 2, "void may never be assigned");
	
	// void may not be used in while, do-while, for, etc.
	EidosAssertScriptRaise("if (citation()) T;", 0, "size() != 1");
	EidosAssertScriptRaise("if (citation()) T; else F;", 0, "size() != 1");
	EidosAssertScriptSuccess_VOID("if (T) citation(); else citation();");
	EidosAssertScriptSuccess_VOID("if (F) citation(); else citation();");
	
	EidosAssertScriptRaise("while (citation()) F;", 0, "size() != 1");
	
	EidosAssertScriptRaise("do F; while (citation());", 0, "size() != 1");
	
	EidosAssertScriptRaise("for (x in citation()) T;", 0, "does not allow void");
}






























