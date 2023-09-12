/* linalg/gsl_linalg.h
 * 
 * Copyright (C) 1996, 1997, 1998, 1999, 2000, 2006, 2007 Gerard Jungman, Brian Gough, Patrick Alken
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

#ifndef __GSL_LINALG_H__
#define __GSL_LINALG_H__

#include <stdlib.h>
//#include "gsl_mode.h"
//#include "gsl_permutation.h"
#include "gsl_vector.h"
#include "gsl_matrix.h"
#include "gsl_math.h"
#include "gsl_inline.h"
#include "gsl_blas.h"

#undef __BEGIN_DECLS
#undef __END_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS }
#else
#define __BEGIN_DECLS           /* empty */
#define __END_DECLS             /* empty */
#endif

__BEGIN_DECLS

/* Cholesky Decomposition */

int gsl_linalg_cholesky_decomp (gsl_matrix * A);
int gsl_linalg_cholesky_decomp1 (gsl_matrix * A);

/* Linear solve for a symmetric tridiagonal system.

 * The input vectors represent the NxN matrix as follows:
 *
 *     diag[0]  offdiag[0]             0    ...
 *  offdiag[0]     diag[1]    offdiag[1]    ...
 *           0  offdiag[1]       diag[2]    ...
 *           0           0    offdiag[2]    ...
 *         ...         ...           ...    ...
 */
int gsl_linalg_solve_symm_tridiag (const gsl_vector * diag,
								   const gsl_vector * offdiag,
								   const gsl_vector * b,
								   gsl_vector * x);

/* Linear solve for a nonsymmetric tridiagonal system.

 * The input vectors represent the NxN matrix as follows:
 *
 *       diag[0]  abovediag[0]              0    ...
 *  belowdiag[0]       diag[1]   abovediag[1]    ...
 *             0  belowdiag[1]        diag[2]    ...
 *             0             0   belowdiag[2]    ...
 *           ...           ...            ...    ...
 */
int gsl_linalg_solve_tridiag (const gsl_vector * diag,
								   const gsl_vector * abovediag,
								   const gsl_vector * belowdiag,
								   const gsl_vector * b,
								   gsl_vector * x);


/* Linear solve for a symmetric cyclic tridiagonal system.

 * The input vectors represent the NxN matrix as follows:
 *
 *      diag[0]  offdiag[0]             0   .....  offdiag[N-1]
 *   offdiag[0]     diag[1]    offdiag[1]   .....
 *            0  offdiag[1]       diag[2]   .....
 *            0           0    offdiag[2]   .....
 *          ...         ...
 * offdiag[N-1]         ...
 */
int gsl_linalg_solve_symm_cyc_tridiag (const gsl_vector * diag,
									   const gsl_vector * offdiag,
									   const gsl_vector * b,
									   gsl_vector * x);

/* Linear solve for a nonsymmetric cyclic tridiagonal system.

 * The input vectors represent the NxN matrix as follows:
 *
 *        diag[0]  abovediag[0]             0   .....  belowdiag[N-1]
 *   belowdiag[0]       diag[1]  abovediag[1]   .....
 *              0  belowdiag[1]       diag[2]
 *              0             0  belowdiag[2]   .....
 *            ...           ...
 * abovediag[N-1]           ...
 */
int gsl_linalg_solve_cyc_tridiag (const gsl_vector * diag,
								  const gsl_vector * abovediag,
								  const gsl_vector * belowdiag,
								  const gsl_vector * b,
								  gsl_vector * x);

__END_DECLS

#endif /* __GSL_LINALG_H__ */
