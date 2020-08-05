//
//  eidos_openmp.h
//  SLiM_OpenMP
//
//  Created by Benjamin C. Haller on 8/4/20.
//  Copyright (c) 2014-2020 Philipp Messer.  All rights reserved.
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

/*
 
 This header should be included instead of omp.h.  It will include omp.h only if we are doing a parallel build of SLiM.
 
 */

#ifndef eidos_openmp_h
#define eidos_openmp_h


#ifdef _OPENMP

// We're building SLiM for running in parallel, and OpenMP is present; include the header.
#include "omp.h"

// Define minimum counts for all the parallel loops we use.  Some of these loops are in SLiM, so we violate encapsulation
// here a bit; a slim_openmp.h header could be created to alleviate that if it's a problem, but it seems harmless.
// Constants that are commented out here are not used in the code; the corresponding loops have no minimum count, because
// we expect that they are always worth running in parallel.  These counts are collected in one place to make it easier
// to optimize their values in a pre-build optimization pass.
#define EIDOS_OMPMIN_SUM_INTEGER			2000
#define EIDOS_OMPMIN_SUM_FLOAT				2000
#define EIDOS_OMPMIN_SUM_LOGICAL			6000
#define EIDOS_OMPMIN_SET_FITNESS_S1			900
#define EIDOS_OMPMIN_SET_FITNESS_S2			1500
#define EIDOS_OMPMIN_SUM_OF_MUTS_OF_TYPE	2
//#define EIDOS_OMPMIN_CALC_ALL_DIST_1D		1
//#define EIDOS_OMPMIN_CALC_ALL_DIST_2D		1
//#define EIDOS_OMPMIN_CALC_ALL_DIST_3D		1
//#define EIDOS_OMPMIN_CALC_ALL_STR_FIXED	1
//#define EIDOS_OMPMIN_CALC_ALL_STR_LINEAR	1
//#define EIDOS_OMPMIN_CALC_ALL_STR_EXP		1
//#define EIDOS_OMPMIN_CALC_ALL_STR_NORMAL	1
//#define EIDOS_OMPMIN_CALC_ALL_STR_CAUCHY	1

#else /* ifdef _OPENMP */

// No OpenMP.  We define only placeholders that we need for the build to succeed.

#endif /* ifdef _OPENMP */


#endif /* eidos_openmp_h */


























