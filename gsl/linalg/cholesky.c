/* Cholesky Decomposition
 *
 * Copyright (C) 2000 Thomas Walter
 * Copyright (C) 2000, 2001, 2002, 2003, 2005, 2007 Brian Gough, Gerard Jungman
 * Copyright (C) 2016 Patrick Alken
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 3, or (at your option) any
 * later version.
 *
 * This source is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * 03 May 2000: Modified for GSL by Brian Gough
 * 29 Jul 2005: Additions by Gerard Jungman
 * 04 Mar 2016: Change Cholesky algorithm to gaxpy version by Patrick Alken
 */

/*
 * Cholesky decomposition of a symmetrix positive definite matrix.
 * This is useful to solve the matrix arising in
 *    periodic cubic splines
 *    approximating splines
 *
 * This algorithm does:
 *   A = L * L'
 * with
 *   L  := lower left triangle matrix
 *   L' := the transposed form of L.
 *
 */

#include "config.h"

#include "gsl_math.h"
#include "gsl_errno.h"
#include "gsl_vector.h"
#include "gsl_matrix.h"
#include "gsl_blas.h"
#include "gsl_linalg.h"

/*
In GSL 2.2, we decided to modify the behavior of the Cholesky decomposition
to store the Cholesky factor in the lower triangle, and store the original
matrix in the upper triangle. Previous versions stored the Cholesky factor in
both places. The routine gsl_linalg_cholesky_decomp1 was added for the new
behavior, and gsl_linalg_cholesky_decomp is maintained for backward compatibility.
It will be removed in a future release.
*/

int
gsl_linalg_cholesky_decomp (gsl_matrix * A)
{
  int status;

  status = gsl_linalg_cholesky_decomp1(A);
  if (status == GSL_SUCCESS)
    {
      gsl_matrix_transpose_tricpy('L', 0, A, A);
    }

  return status;
}

/*
gsl_linalg_cholesky_decomp1()
  Perform Cholesky decomposition of a symmetric positive
definite matrix using lower triangle

Inputs: A - (input) symmetric, positive definite matrix
            (output) lower triangle contains Cholesky factor

Return: success/error

Notes:
1) Based on algorithm 4.2.1 (Gaxpy Cholesky) of Golub and
Van Loan, Matrix Computations (4th ed).

2) original matrix is saved in upper triangle on output
*/

int
gsl_linalg_cholesky_decomp1 (gsl_matrix * A)
{
  const size_t M = A->size1;
  const size_t N = A->size2;

  if (M != N)
    {
      GSL_ERROR("cholesky decomposition requires square matrix", GSL_ENOTSQR);
    }
  else
    {
      size_t j;

      /* save original matrix in upper triangle for later rcond calculation */
      gsl_matrix_transpose_tricpy('L', 0, A, A);

      for (j = 0; j < N; ++j)
        {
          double ajj;
          gsl_vector_view v = gsl_matrix_subcolumn(A, j, j, N - j); /* A(j:n,j) */

          if (j > 0)
            {
              gsl_vector_view w = gsl_matrix_subrow(A, j, 0, j);           /* A(j,1:j-1)^T */
              gsl_matrix_view m = gsl_matrix_submatrix(A, j, 0, N - j, j); /* A(j:n,1:j-1) */

              gsl_blas_dgemv(CblasNoTrans, -1.0, &m.matrix, &w.vector, 1.0, &v.vector);
            }

          ajj = gsl_matrix_get(A, j, j);

          if (ajj <= 0.0)
            {
              GSL_ERROR("matrix is not positive definite", GSL_EDOM);
            }

          ajj = sqrt(ajj);
          gsl_vector_scale(&v.vector, 1.0 / ajj);
        }

      return GSL_SUCCESS;
    }
}

