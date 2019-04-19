/* blas/blas.c
 * 
 * Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2009 Gerard Jungman & Brian 
 * Gough
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* GSL implementation of BLAS operations for vectors and dense
 * matrices.  Note that GSL native storage is row-major.  */

#include "config.h"
//#include <gsl/gsl_math.h>
//#include <gsl/gsl_errno.h>
//#include <gsl/gsl_cblas.h>
//#include <gsl/gsl_cblas.h>
//#include <gsl/gsl_blas_types.h>
#include "gsl_blas.h"

/* ========================================================================
 * Level 1
 * ========================================================================
 */

/* CBLAS defines vector sizes in terms of int. GSL defines sizes in
 terms of size_t, so we need to convert these into integers.  There
 is the possibility of overflow here. FIXME: Maybe this could be
 caught */

#define INT(X) ((int)(X))


int
gsl_blas_ddot (const gsl_vector * X, const gsl_vector * Y, double *result)
{
	if (X->size == Y->size)
	{
		*result =
		cblas_ddot (INT (X->size), X->data, INT (X->stride), Y->data,
					INT (Y->stride));
		return GSL_SUCCESS;
	}
	else
	{
		GSL_ERROR ("invalid length", GSL_EBADLEN);
	}
}

int
gsl_blas_dgemv (CBLAS_TRANSPOSE_t TransA, double alpha, const gsl_matrix * A,
				const gsl_vector * X, double beta, gsl_vector * Y)
{
	const size_t M = A->size1;
	const size_t N = A->size2;
	
	if ((TransA == CblasNoTrans && N == X->size && M == Y->size)
		|| (TransA == CblasTrans && M == X->size && N == Y->size))
	{
		cblas_dgemv (CblasRowMajor, TransA, INT (M), INT (N), alpha, A->data,
					 INT (A->tda), X->data, INT (X->stride), beta, Y->data,
					 INT (Y->stride));
		return GSL_SUCCESS;
	}
	else
	{
		GSL_ERROR ("invalid length", GSL_EBADLEN);
	}
}

int
gsl_blas_dtrmv (CBLAS_UPLO_t Uplo, CBLAS_TRANSPOSE_t TransA,
                CBLAS_DIAG_t Diag, const gsl_matrix * A, gsl_vector * X)
{
  const size_t M = A->size1;
  const size_t N = A->size2;

  if (M != N)
    {
      GSL_ERROR ("matrix must be square", GSL_ENOTSQR);
    }
  else if (N != X->size)
    {
      GSL_ERROR ("invalid length", GSL_EBADLEN);
    }

  cblas_dtrmv (CblasRowMajor, Uplo, TransA, Diag, INT (N), A->data,
               INT (A->tda), X->data, INT (X->stride));
  return GSL_SUCCESS;
}

int
gsl_blas_dtrsv (CBLAS_UPLO_t Uplo, CBLAS_TRANSPOSE_t TransA,
				CBLAS_DIAG_t Diag, const gsl_matrix * A, gsl_vector * X)
{
	const size_t M = A->size1;
	const size_t N = A->size2;
	
	if (M != N)
	{
		GSL_ERROR ("matrix must be square", GSL_ENOTSQR);
	}
	else if (N != X->size)
	{
		GSL_ERROR ("invalid length", GSL_EBADLEN);
	}
	
	cblas_dtrsv (CblasRowMajor, Uplo, TransA, Diag, INT (N), A->data,
				 INT (A->tda), X->data, INT (X->stride));
	return GSL_SUCCESS;
}



