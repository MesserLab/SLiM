/* randist/mvgauss.c
 * 
 * Copyright (C) 2016 Timothée Flutre, Patrick Alken
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

#include "config.h"
#include <math.h>
#include "gsl_math.h"
#include "gsl_rng.h"
#include "gsl_randist.h"
#include "gsl_vector.h"
#include "gsl_matrix.h"
#include "gsl_blas.h"
//#include "gsl_linalg.h"
//#include "gsl_statistics.h"

/* Generate a random vector from a multivariate Gaussian distribution using
 * the Cholesky decomposition of the variance-covariance matrix, following
 * "Computational Statistics" from Gentle (2009), section 7.4.
 *
 * mu      mean vector (dimension d)
 * L       matrix resulting from the Cholesky decomposition of
 *         variance-covariance matrix Sigma = L L^T (dimension d x d)
 * result  output vector (dimension d)
 */
int
gsl_ran_multivariate_gaussian (const gsl_rng * r,
                               const gsl_vector * mu,
                               const gsl_matrix * L,
                               gsl_vector * result)
{
  const size_t M = L->size1;
  const size_t N = L->size2;

  if (M != N)
    {
      GSL_ERROR("requires square matrix", GSL_ENOTSQR);
    }
  else if (mu->size != M)
    {
      GSL_ERROR("incompatible dimension of mean vector with variance-covariance matrix", GSL_EBADLEN);
    }
  else if (result->size != M)
    {
      GSL_ERROR("incompatible dimension of result vector", GSL_EBADLEN);
    }
  else
    {
      size_t i;

      for (i = 0; i < M; ++i)
        gsl_vector_set(result, i, gsl_ran_ugaussian(r));

      gsl_blas_dtrmv(CblasLower, CblasNoTrans, CblasNonUnit, L, result);
      gsl_vector_add(result, mu);

      return GSL_SUCCESS;
    }
}

