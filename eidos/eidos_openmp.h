//
//  eidos_openmp.h
//  SLiM_OpenMP
//
//  Created by Benjamin C. Haller on 8/4/20.
//  Copyright (c) 2014-2023 Philipp Messer.  All rights reserved.
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
 
 This header should be included instead of omp.h.  It will include omp.h only if we are doing a parallel build of SLiM;
 otherwise, it will provide inline stub implementations of the OpenMP API.  We require OpenMP 4.5.
 
 Some basic instructions for building and using parallel SLiM (spelled out more in the manual):
 
	 on macOS, Apple has disabled OpenMP support in the version of clang they ship, and they do not include the OpenMP library or headers (WTF), so this needs to be fixed to build
		 first of all, the R folks have figured this out; see https://mac.r-project.org/openmp/ for information and downloads
		 do "About Xcode" to find out the Xcode version you're using, and download the corresponding openmp ("Release", probably) from that web page
		 install it as shown there ("sudo tar fvxz <file>.tar.gz -C /"); you may see "tar: Error exit delayed from previous errors" which seems to be fine
		 the rest of that web page is R-specific and can be ignored
		 note that we used to use MacPorts or Homebrew to install a separate version of clang; it seems better to use the native clang and patch it up as we do now, but this no longer links in libomp automatically
	 on other platforms, you may need to get a newer/different compiler to get one with OpenMP support; we have targeted OpenMP 4.5 as a minimum for now
		 if you need to use a different compiler, you may need to switch CMake over to using that compiler, etc.
		 you may also need to install the OpenMP library if it doesn't come installed for your toolchain; and the paths for the includes and libomp might be different than in CMakeLists.txt
		 if you figure how to make this work for a particular platform, please send us step-by-step instructions that we can share with other users!
	 if building at the command line with CMake, set -DPARALLEL=ON, and do not build SLiMgui (it will error out)
	 if building in Xcode, use the provided separate version of the project, SLiM_OpenMP.xcodeproj, and the separate targets eidos_multi and slim_multi
	 with -DPARALLEL=ON, the built executables will be slim_multi and eidos_multi, to make it easier to distinguish them; but of course you may rename them as you see fit
	 on macOS, you may (several times!) get a system alert that libomp was blocked for security; after that, go to System Preferences, Security & Privacy, tab General, click "Allow Anyway", and then click "Open" back in the system panel
	 use the -maxThreads <x> command-line option to change the maximum number of threads from OpenMP's default

 We allocate per-thread storage (for gEidosMaxThreads threads) at the global/species level for these facilities:
 
	SparseVector pools; see s_freed_sparse_vectors_PERTHREAD vs. s_freed_sparse_vectors_SINGLE
	random number generators; see gEidos_RNG_PERTHREAD vs. gEidos_RNG_SINGLE
	MutationRunContexts; see mutation_run_context_PERTHREAD vs. mutation_run_SINGLE
 
 We use #pragma omp critical to protect some places in the code, with specified names
 
 Places in the code that cannot be encountered when parallel use THREAD_SAFETY macros, defined below,
 as a runtime guard; but be aware that it only checks for Debug builds.  Release builds may just produce
 race conditions or incorrect results with no warning or error.  Always check with a Debug build.
 
 */

#ifndef eidos_openmp_h
#define eidos_openmp_h

#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <iostream>


/*
 *	For simplicity, ongoing work related to the parallelization of SLiM with OpenMP now resides in the master branch.
 *	However, multithreaded SLiM is not released, not thoroughly tested, and generally not yet ready for prime time.
 *	It is not recommended for end-user use, especially not for "production" runs, and the documentation for it is
 *	not yet public.  Please do not ask for any kind of support for this feature if you choose to experiment with it.
 *
 *		- BCH 12/4/2023
 */
#ifdef _OPENMP
#error Building multithreaded SLiM is presently disabled and unsupported.  This feature is still under development.
#endif


// This is the largest number of threads we allow the user to set.  There is no hard limit in the code;
// this is primarily just to prevent people from doing anything stupid.
#define EIDOS_OMP_MAX_THREADS	1024

// This is a cached result from omp_get_max_threads() after warmup, providing the final number of threads that we will
// be using (maximum) in parallel regions.  This can be used to preallocate per-thread data structures.
extern int gEidosMaxThreads;

// This is the number of threads that will be used in the next parallel region to execute, as set by the Eidos
// function parallelSetNumThreads().  This will generally be equal to omp_get_max_threads().  It will be clamped
// to the interval [1, gEidosMaxThreads].  If it has been set explicitly, gEidosNumThreadsOverride is set to true;
// if not, gEidosNumThreadsOverride is false.  This allows Eidos to distinguish between gEidosNumThreads == gEidosMaxThreads
// simply because it hasn't been set (gEidosNumThreadsOverride == false), indicating a desire to receive the default
// number of threads, versus having been explicitly set to gEidosMaxThreads (gEidosNumThreadsOverride == true),
// indicating a desire to force the maximum number of threads to be used even if it normally wouldn't.
extern int gEidosNumThreads;
extern bool gEidosNumThreadsOverride;


// We want to use SIGTRAP to catch problems in the debugger in a few key spots, but it doesn't exist on Windows.
// So we will just define SIGTRAP to be SIGABRT instead; SIGABRT is supported on Windows.
#ifdef _WIN32
#define SIGTRAP SIGABRT
#endif


// Thread safety checking: places in the code that have identified thread safety concerns should use this macro.  It will
// produce a runtime error for DEBUG builds if it is hit while parallel.  Put it in places that are not currently thread-safe.
// For example, object pools and other such global state are not thread-safe right now, so they should use this.
// Many of these places might be made safe with a simple locking protocol, but that has not yet been done, so beware.
// This tagging of unsafe spots is undoubtedly not comprehensive; I'm just trying to catch the most obvious problems!
// Note that this macro uses a GCC built-in; it is supported by Clang as well, but may need a tweak for other platforms.
// This macro checks only for DEBUG builds!  When working on parallel code, be sure to check correctness with DEBUG!
//
// BCH 5/14/2023: We now have two versions of this, because there are two checks one might want to do:
//
//		THREAD_SAFETY_IN_ANY_PARALLEL() - errors if inside a parallel region, even if inactive (i.e., running with
//			only one thread).  This is particularly relevant for code that is known to raise without protection,
//			and particularly, for code that executes Eidos lambda/callback code without protection.  It is also
//			useful for code that, semantically, should just never be inside a parallel region at all.  Some
//			parts of the Eidos interpreter fall into this category.
//
//		THREAD_SAFETY_IN_ACTIVE_PARALLEL() - errors if inside an active (i.e., multithreaded) parallel region.
//			This is particularly relevant for code that is not thread-safe due to use of statics, presence of races,
//			etc.; the code is fine run-single-threaded, even in an inactive parallel region.  Some parts of the
//			Eidos interpreter fall into this category, because we sometimes want to use the same code path for
//			both single-threaded and multi-threaded execution when running under OpenMP, and that can mean that
//			Eidos code ends up running inside an inactive parallel region.  Any code that does this must be prepared
//			to catch throws coming out of the Eidos interpreter, however, since they must be caught inside the
//			parallel region!

#ifdef _OPENMP

#if DEBUG
#define THREAD_SAFETY_IN_ACTIVE_PARALLEL(s) if (omp_in_parallel()) { std::cerr << "THREAD_SAFETY_IN_ACTIVE_PARALLEL error in " << s; raise(SIGTRAP); }
#define THREAD_SAFETY_IN_ANY_PARALLEL(s) if (omp_get_level() > 0) { std::cerr << "THREAD_SAFETY_IN_ANY_PARALLEL error in " << s; raise(SIGTRAP); }
#else
#define THREAD_SAFETY_IN_ACTIVE_PARALLEL(s)	{ ; }
#define THREAD_SAFETY_IN_ANY_PARALLEL(s)	{ ; }
#endif

#else

#define THREAD_SAFETY_IN_ACTIVE_PARALLEL(s)	{ ; }
#define THREAD_SAFETY_IN_ANY_PARALLEL(s)	{ ; }

#endif


// This macro is for calculating the correct number of threads to use for a given loop; it uses a thread count set with
// parallelSetNumThreads() if it exists, otherwise uses the thread count provided by (x), expected to be <= gEidosMaxThreads.
#ifdef _OPENMP
#define EIDOS_THREAD_COUNT(x) int thread_count = (gEidosNumThreadsOverride ? gEidosNumThreads : (x))
#else
#define EIDOS_THREAD_COUNT(x)
#endif


#ifdef _OPENMP

// Check that the OpenMP version supported by the compiler suffices.  Note that _OPENMP is formatted as a "YYYYMM" date of
// release.  See https://github.com/Kitware/CMake/blob/v3.16.3/Modules/FindOpenMP.cmake#L384 for dates of release.  For
// quick reference, "200805=3.0", "201107=3.1", "201307=4.0", "201511=4.5", "201811=5.0".  Right now we require 4.5.
#if (_OPENMP < 201511)
#error OpenMP version 4.5 or later is required.
#endif

// We're building SLiM for running in parallel, and OpenMP is present; include the header.
#include "omp.h"

// C++ wrappers for OpenMP's lock types.  For now we define these only in the OpenMP case, not with the stubs below for
// the non-OpenMP case, to encourage this to be used only when we are multithreading, to avoid single-threaded overhead.
// The big benefit to using these classes is that you get automatic RAII construction/destruction of the lock.
class OMPLock
{
public:
	OMPLock() { omp_init_lock(&lock_); }
	~OMPLock() { omp_destroy_lock(&lock_); }
	
	void set() { omp_set_lock(&lock_); }
	void unset() { omp_unset_lock(&lock_); }
	int test() { return omp_test_lock(&lock_); }		// true (non-zero) if the lock was obtained
	
private:
	omp_lock_t lock_;
};

class OMPNestLock
{
public:
	OMPNestLock() { omp_init_nest_lock(&lock_); }
	~OMPNestLock() { omp_destroy_nest_lock(&lock_); }
	
	void set() { omp_set_nest_lock(&lock_); }
	void unset() { omp_unset_nest_lock(&lock_); }
	int test() { return omp_test_nest_lock(&lock_); }	// true (non-zero) if the lock was obtained
	
private:
	omp_nest_lock_t lock_;
};

// This is a lock-based class used in DEBUG builds to test for race conditions involving code that is not locked
// or otherwise arbitrated.  The expectation is that only one thread at a time will be executing in regions
// governed by a debug lock, but that is not enforced; it is supposed to be a consequence of the design of the code
// itself.  This class makes it easy to check for race conditions involving such regions.
#if (defined(_OPENMP) && DEBUG)
#define DEBUG_LOCKS_ENABLED

class EidosDebugLock
{
public:
	EidosDebugLock() = delete;
	EidosDebugLock(const char *p_name) : lock_name_(p_name) { omp_init_nest_lock(&lock_); }
	~EidosDebugLock() { omp_destroy_nest_lock(&lock_); }
	
	void start_critical(int p_owner) {
		int result = omp_test_nest_lock(&lock_);
		
		if (!result)
		{
			// We did not get the lock; somebody else is using the same resource.  This is a fatal error.
			std::cerr << "race with " << lock_name_ << ": EidosDebugLock owner == " << owner_ << ", racing owner == " << p_owner << std::endl;
			THREAD_SAFETY_IN_ACTIVE_PARALLEL("EidosDebugLock::start_critical(): race condition hit!");
		}
		else
		{
			// We got the lock; mark our turf so if somebody races with us they know who got the lock first
#pragma omp atomic write
			owner_ = p_owner;
			current_nest_ = result;
		}
	}
	void end_critical() {
		if (current_nest_ == 1)
		{
#pragma omp atomic write
			owner_ = -1;
		}
		--current_nest_;
		omp_unset_nest_lock(&lock_);
	}
	
private:
	omp_nest_lock_t lock_;
	std::string lock_name_;			// a client-defined string name for the lock, to identify it in debug output
	int owner_ = -1;				// a client-defined integer value identifying which code region has taken the lock
	int current_nest_ = 0;			// the current nesting count, so we know if we're about to unlock
};
#endif


// Define minimum counts for all the parallel loops we use.  Some of these loops are in SLiM, so we violate encapsulation
// here a bit; a slim_openmp.h header could be created to alleviate that if it's a problem, but it seems harmless for now.
// These counts are collected in one place to make it easier to optimize their values in a pre-build optimization pass.

// Set this flag to 0 to switch to running parallel loops with the maximum number of threads, regardless of the per-task
// default thread counts or the task size thresholds (but not regardless of the user's parallelSetNumThreads() setting).
#define USE_OMP_LIMITS	1

#if USE_OMP_LIMITS
// This set of minimum counts is for production code

// Eidos: math functions
#define EIDOS_OMPMIN_ABS_FLOAT				2000
#define EIDOS_OMPMIN_CEIL					2000
#define EIDOS_OMPMIN_EXP_FLOAT				2000
#define EIDOS_OMPMIN_FLOOR					2000
#define EIDOS_OMPMIN_LOG_FLOAT				2000
#define EIDOS_OMPMIN_LOG10_FLOAT			2000
#define EIDOS_OMPMIN_LOG2_FLOAT				2000
#define EIDOS_OMPMIN_ROUND					2000
#define EIDOS_OMPMIN_SQRT_FLOAT				2000
#define EIDOS_OMPMIN_SUM_INTEGER			2000
#define EIDOS_OMPMIN_SUM_FLOAT				2000
#define EIDOS_OMPMIN_SUM_LOGICAL			6000
#define EIDOS_OMPMIN_TRUNC					2000

// Eidos: max(), min(), pmax(), pmin()
#define EIDOS_OMPMIN_MAX_INT				2000
#define EIDOS_OMPMIN_MAX_FLOAT				2000
#define EIDOS_OMPMIN_MIN_INT				2000
#define EIDOS_OMPMIN_MIN_FLOAT				2000
#define EIDOS_OMPMIN_PMAX_INT_1				2000
#define EIDOS_OMPMIN_PMAX_INT_2				2000
#define EIDOS_OMPMIN_PMAX_FLOAT_1			2000
#define EIDOS_OMPMIN_PMAX_FLOAT_2			2000
#define EIDOS_OMPMIN_PMIN_INT_1				2000
#define EIDOS_OMPMIN_PMIN_INT_2				2000
#define EIDOS_OMPMIN_PMIN_FLOAT_1			2000
#define EIDOS_OMPMIN_PMIN_FLOAT_2			2000

// Eidos: match(), sample(), tabulate()
#define EIDOS_OMPMIN_MATCH_INT				2000
#define EIDOS_OMPMIN_MATCH_FLOAT			2000
#define EIDOS_OMPMIN_MATCH_STRING			2000
#define EIDOS_OMPMIN_MATCH_OBJECT			2000
#define EIDOS_OMPMIN_SAMPLE_INDEX			2000
#define EIDOS_OMPMIN_SAMPLE_R_INT			2000
#define EIDOS_OMPMIN_SAMPLE_R_FLOAT			2000
#define EIDOS_OMPMIN_SAMPLE_R_OBJECT		2000
#define EIDOS_OMPMIN_SAMPLE_WR_INT			2000
#define EIDOS_OMPMIN_SAMPLE_WR_FLOAT		2000
#define EIDOS_OMPMIN_SAMPLE_WR_OBJECT		2000
#define EIDOS_OMPMIN_TABULATE_MAXBIN		2000
#define EIDOS_OMPMIN_TABULATE				2000

// SLiM methods/properties
#define EIDOS_OMPMIN_CONTAINS_MARKER_MUT	900
#define EIDOS_OMPMIN_I_COUNT_OF_MUTS_OF_TYPE	2
#define EIDOS_OMPMIN_G_COUNT_OF_MUTS_OF_TYPE	2
#define EIDOS_OMPMIN_INDS_W_PEDIGREE_IDS	2000
#define EIDOS_OMPMIN_RELATEDNESS			2000
#define EIDOS_OMPMIN_SAMPLE_INDIVIDUALS_1	2000
#define EIDOS_OMPMIN_SAMPLE_INDIVIDUALS_2	2000
#define EIDOS_OMPMIN_SET_FITNESS_SCALE_1	900
#define EIDOS_OMPMIN_SET_FITNESS_SCALE_2	1500
#define EIDOS_OMPMIN_SUM_OF_MUTS_OF_TYPE	2

// Distribution draws and related
#define EIDOS_OMPMIN_DNORM_1				10000
#define EIDOS_OMPMIN_DNORM_2				10000
#define EIDOS_OMPMIN_RBINOM_1				10000
#define EIDOS_OMPMIN_RBINOM_2				10000
#define EIDOS_OMPMIN_RBINOM_3				10000
#define EIDOS_OMPMIN_RDUNIF_1				10000
#define EIDOS_OMPMIN_RDUNIF_2				10000
#define EIDOS_OMPMIN_RDUNIF_3				10000
#define EIDOS_OMPMIN_REXP_1					10000
#define EIDOS_OMPMIN_REXP_2					10000
#define EIDOS_OMPMIN_RNORM_1				10000
#define EIDOS_OMPMIN_RNORM_2				10000
#define EIDOS_OMPMIN_RNORM_3				10000
#define EIDOS_OMPMIN_RPOIS_1				10000
#define EIDOS_OMPMIN_RPOIS_2				10000
#define EIDOS_OMPMIN_RUNIF_1				10000
#define EIDOS_OMPMIN_RUNIF_2				10000
#define EIDOS_OMPMIN_RUNIF_3				10000

// Sorting & ordering
#define EIDOS_OMPMIN_SORT_INT				4000
#define EIDOS_OMPMIN_SORT_FLOAT				4000
#define EIDOS_OMPMIN_SORT_STRING			4000

// Spatial point/map manipulation
#define EIDOS_OMPMIN_POINT_IN_BOUNDS_1D		2000
#define EIDOS_OMPMIN_POINT_IN_BOUNDS_2D		2000
#define EIDOS_OMPMIN_POINT_IN_BOUNDS_3D		2000
#define EIDOS_OMPMIN_POINT_PERIODIC_1D		2000
#define EIDOS_OMPMIN_POINT_PERIODIC_2D		2000
#define EIDOS_OMPMIN_POINT_PERIODIC_3D		2000
#define EIDOS_OMPMIN_POINT_REFLECTED_1D		2000
#define EIDOS_OMPMIN_POINT_REFLECTED_2D		2000
#define EIDOS_OMPMIN_POINT_REFLECTED_3D		2000
#define EIDOS_OMPMIN_POINT_STOPPED_1D		2000
#define EIDOS_OMPMIN_POINT_STOPPED_2D		2000
#define EIDOS_OMPMIN_POINT_STOPPED_3D		2000
#define EIDOS_OMPMIN_POINT_UNIFORM_1D		2000
#define EIDOS_OMPMIN_POINT_UNIFORM_2D		2000
#define EIDOS_OMPMIN_POINT_UNIFORM_3D		2000
#define EIDOS_OMPMIN_SET_SPATIAL_POS_1_1D	10000
#define EIDOS_OMPMIN_SET_SPATIAL_POS_1_2D	10000
#define EIDOS_OMPMIN_SET_SPATIAL_POS_1_3D	10000
#define EIDOS_OMPMIN_SET_SPATIAL_POS_2_1D	10000
#define EIDOS_OMPMIN_SET_SPATIAL_POS_2_2D	10000
#define EIDOS_OMPMIN_SET_SPATIAL_POS_2_3D	10000
#define EIDOS_OMPMIN_SPATIAL_MAP_VALUE		2000

// Spatial queries
#define EIDOS_OMPMIN_CLIPPEDINTEGRAL_1S		10000
#define EIDOS_OMPMIN_CLIPPEDINTEGRAL_2S		10000
//#define EIDOS_OMPMIN_CLIPPEDINTEGRAL_3S		10000
#define EIDOS_OMPMIN_DRAWBYSTRENGTH			10
#define EIDOS_OMPMIN_INTNEIGHCOUNT			10
#define EIDOS_OMPMIN_LOCALPOPDENSITY		10
#define EIDOS_OMPMIN_NEARESTINTNEIGH		10
#define EIDOS_OMPMIN_NEARESTNEIGH			10
#define EIDOS_OMPMIN_NEIGHCOUNT				10
#define EIDOS_OMPMIN_TOTNEIGHSTRENGTH		10

// SLiM core
#define EIDOS_OMPMIN_AGE_INCR				10000
#define EIDOS_OMPMIN_DEFERRED_REPRO			100
#define EIDOS_OMPMIN_WF_REPRO				100
#define EIDOS_OMPMIN_FITNESS_ASEX_1			10000
#define EIDOS_OMPMIN_FITNESS_ASEX_2			10000
#define EIDOS_OMPMIN_FITNESS_ASEX_3			10000
#define EIDOS_OMPMIN_FITNESS_SEX_1			10000
#define EIDOS_OMPMIN_FITNESS_SEX_2			10000
#define EIDOS_OMPMIN_FITNESS_SEX_3			10000
#define EIDOS_OMPMIN_MIGRANT_CLEAR			10000
#define EIDOS_OMPMIN_SIMPLIFY_SORT_PRE		4000
#define EIDOS_OMPMIN_SIMPLIFY_SORT			4000
#define EIDOS_OMPMIN_SIMPLIFY_SORT_POST		4000
#define EIDOS_OMPMIN_SURVIVAL				10000

#else
// This set of minimum counts is for debugging; we want to run all self-tests in parallel, so that
// bugs don't get masked just by virtue of the bug-inducing task being too small to run parallel
#warning switch back to production minimum counts!

// Eidos: math functions
#define EIDOS_OMPMIN_ABS_FLOAT				0
#define EIDOS_OMPMIN_CEIL					0
#define EIDOS_OMPMIN_EXP_FLOAT				0
#define EIDOS_OMPMIN_FLOOR					0
#define EIDOS_OMPMIN_LOG_FLOAT				0
#define EIDOS_OMPMIN_LOG10_FLOAT			0
#define EIDOS_OMPMIN_LOG2_FLOAT				0
#define EIDOS_OMPMIN_ROUND					0
#define EIDOS_OMPMIN_SQRT_FLOAT				0
#define EIDOS_OMPMIN_SUM_INTEGER			0
#define EIDOS_OMPMIN_SUM_FLOAT				0
#define EIDOS_OMPMIN_SUM_LOGICAL			0
#define EIDOS_OMPMIN_TRUNC					0

// Eidos: max(), min(), pmax(), pmin()
#define EIDOS_OMPMIN_MAX_INT				0
#define EIDOS_OMPMIN_MAX_FLOAT				0
#define EIDOS_OMPMIN_MIN_INT				0
#define EIDOS_OMPMIN_MIN_FLOAT				0
#define EIDOS_OMPMIN_PMAX_INT_1				0
#define EIDOS_OMPMIN_PMAX_INT_2				0
#define EIDOS_OMPMIN_PMAX_FLOAT_1			0
#define EIDOS_OMPMIN_PMAX_FLOAT_2			0
#define EIDOS_OMPMIN_PMIN_INT_1				0
#define EIDOS_OMPMIN_PMIN_INT_2				0
#define EIDOS_OMPMIN_PMIN_FLOAT_1			0
#define EIDOS_OMPMIN_PMIN_FLOAT_2			0

// Eidos: match(), sample(), tabulate()
#define EIDOS_OMPMIN_MATCH_INT				0
#define EIDOS_OMPMIN_MATCH_FLOAT			0
#define EIDOS_OMPMIN_MATCH_STRING			0
#define EIDOS_OMPMIN_MATCH_OBJECT			0
#define EIDOS_OMPMIN_SAMPLE_INDEX			0
#define EIDOS_OMPMIN_SAMPLE_R_INT			0
#define EIDOS_OMPMIN_SAMPLE_R_FLOAT			0
#define EIDOS_OMPMIN_SAMPLE_R_OBJECT		0
#define EIDOS_OMPMIN_SAMPLE_WR_INT			0
#define EIDOS_OMPMIN_SAMPLE_WR_FLOAT		0
#define EIDOS_OMPMIN_SAMPLE_WR_OBJECT		0
#define EIDOS_OMPMIN_TABULATE_MAXBIN		0
#define EIDOS_OMPMIN_TABULATE				0

// SLiM methods/properties
#define EIDOS_OMPMIN_CONTAINS_MARKER_MUT	0
#define EIDOS_OMPMIN_I_COUNT_OF_MUTS_OF_TYPE	0
#define EIDOS_OMPMIN_G_COUNT_OF_MUTS_OF_TYPE	0
#define EIDOS_OMPMIN_INDS_W_PEDIGREE_IDS	0
#define EIDOS_OMPMIN_RELATEDNESS			0
#define EIDOS_OMPMIN_SAMPLE_INDIVIDUALS_1	0
#define EIDOS_OMPMIN_SAMPLE_INDIVIDUALS_2	0
#define EIDOS_OMPMIN_SET_FITNESS_SCALE_1	0
#define EIDOS_OMPMIN_SET_FITNESS_SCALE_2	0
#define EIDOS_OMPMIN_SUM_OF_MUTS_OF_TYPE	0

// Distribution draws and related
#define EIDOS_OMPMIN_DNORM_1				0
#define EIDOS_OMPMIN_DNORM_2				0
#define EIDOS_OMPMIN_RBINOM_1				0
#define EIDOS_OMPMIN_RBINOM_2				0
#define EIDOS_OMPMIN_RBINOM_3				0
#define EIDOS_OMPMIN_RDUNIF_1				0
#define EIDOS_OMPMIN_RDUNIF_2				0
#define EIDOS_OMPMIN_RDUNIF_3				0
#define EIDOS_OMPMIN_REXP_1					0
#define EIDOS_OMPMIN_REXP_2					0
#define EIDOS_OMPMIN_RNORM_1				0
#define EIDOS_OMPMIN_RNORM_2				0
#define EIDOS_OMPMIN_RNORM_3				0
#define EIDOS_OMPMIN_RPOIS_1				0
#define EIDOS_OMPMIN_RPOIS_2				0
#define EIDOS_OMPMIN_RUNIF_1				0
#define EIDOS_OMPMIN_RUNIF_2				0
#define EIDOS_OMPMIN_RUNIF_3				0

// Sorting & ordering
#define EIDOS_OMPMIN_SORT_INT				0
#define EIDOS_OMPMIN_SORT_FLOAT				0
#define EIDOS_OMPMIN_SORT_STRING			0

// Spatial point/map manipulation
#define EIDOS_OMPMIN_POINT_IN_BOUNDS_1D		0
#define EIDOS_OMPMIN_POINT_IN_BOUNDS_2D		0
#define EIDOS_OMPMIN_POINT_IN_BOUNDS_3D		0
#define EIDOS_OMPMIN_POINT_PERIODIC_1D		0
#define EIDOS_OMPMIN_POINT_PERIODIC_2D		0
#define EIDOS_OMPMIN_POINT_PERIODIC_3D		0
#define EIDOS_OMPMIN_POINT_REFLECTED_1D		0
#define EIDOS_OMPMIN_POINT_REFLECTED_2D		0
#define EIDOS_OMPMIN_POINT_REFLECTED_3D		0
#define EIDOS_OMPMIN_POINT_STOPPED_1D		0
#define EIDOS_OMPMIN_POINT_STOPPED_2D		0
#define EIDOS_OMPMIN_POINT_STOPPED_3D		0
#define EIDOS_OMPMIN_POINT_UNIFORM_1D		0
#define EIDOS_OMPMIN_POINT_UNIFORM_2D		0
#define EIDOS_OMPMIN_POINT_UNIFORM_3D		0
#define EIDOS_OMPMIN_SET_SPATIAL_POS_1_1D	0
#define EIDOS_OMPMIN_SET_SPATIAL_POS_1_2D	0
#define EIDOS_OMPMIN_SET_SPATIAL_POS_1_3D	0
#define EIDOS_OMPMIN_SET_SPATIAL_POS_2_1D	0
#define EIDOS_OMPMIN_SET_SPATIAL_POS_2_2D	0
#define EIDOS_OMPMIN_SET_SPATIAL_POS_2_3D	0
#define EIDOS_OMPMIN_SPATIAL_MAP_VALUE		0

// Spatial queries
#define EIDOS_OMPMIN_CLIPPEDINTEGRAL_1S		0
#define EIDOS_OMPMIN_CLIPPEDINTEGRAL_2S		0
//#define EIDOS_OMPMIN_CLIPPEDINTEGRAL_3S		0
#define EIDOS_OMPMIN_DRAWBYSTRENGTH			0
#define EIDOS_OMPMIN_INTNEIGHCOUNT			0
#define EIDOS_OMPMIN_LOCALPOPDENSITY		0
#define EIDOS_OMPMIN_NEARESTINTNEIGH		0
#define EIDOS_OMPMIN_NEARESTNEIGH			0
#define EIDOS_OMPMIN_NEIGHCOUNT				0
#define EIDOS_OMPMIN_TOTNEIGHSTRENGTH		0

// SLiM core
#define EIDOS_OMPMIN_AGE_INCR				0
#define EIDOS_OMPMIN_DEFERRED_REPRO			0
#define EIDOS_OMPMIN_WF_REPRO				0
#define EIDOS_OMPMIN_FITNESS_ASEX_1			0
#define EIDOS_OMPMIN_FITNESS_ASEX_2			0
#define EIDOS_OMPMIN_FITNESS_ASEX_3			0
#define EIDOS_OMPMIN_FITNESS_SEX_1			0
#define EIDOS_OMPMIN_FITNESS_SEX_2			0
#define EIDOS_OMPMIN_FITNESS_SEX_3			0
#define EIDOS_OMPMIN_MIGRANT_CLEAR			0
#define EIDOS_OMPMIN_SIMPLIFY_SORT_PRE		0
#define EIDOS_OMPMIN_SIMPLIFY_SORT			0
#define EIDOS_OMPMIN_SIMPLIFY_SORT_POST		0
#define EIDOS_OMPMIN_SURVIVAL				0

#endif


// Here we declare variables that hold the number of threads we prefer to use for each parallel loop.
// These have default values, which can be overridden with parallelSetTaskThreadCounts().

// Eidos: math functions; benchmark section E
extern int gEidos_OMP_threads_ABS_FLOAT;
extern int gEidos_OMP_threads_CEIL;
extern int gEidos_OMP_threads_EXP_FLOAT;
extern int gEidos_OMP_threads_FLOOR;
extern int gEidos_OMP_threads_LOG_FLOAT;
extern int gEidos_OMP_threads_LOG10_FLOAT;
extern int gEidos_OMP_threads_LOG2_FLOAT;
extern int gEidos_OMP_threads_ROUND;
extern int gEidos_OMP_threads_SQRT_FLOAT;
extern int gEidos_OMP_threads_SUM_INTEGER;
extern int gEidos_OMP_threads_SUM_FLOAT;
extern int gEidos_OMP_threads_SUM_LOGICAL;
extern int gEidos_OMP_threads_TRUNC;

// Eidos: max(), min(), pmax(), pmin(); benchmark section X
extern int gEidos_OMP_threads_MAX_INT;
extern int gEidos_OMP_threads_MAX_FLOAT;
extern int gEidos_OMP_threads_MIN_INT;
extern int gEidos_OMP_threads_MIN_FLOAT;
extern int gEidos_OMP_threads_PMAX_INT_1;
extern int gEidos_OMP_threads_PMAX_INT_2;
extern int gEidos_OMP_threads_PMAX_FLOAT_1;
extern int gEidos_OMP_threads_PMAX_FLOAT_2;
extern int gEidos_OMP_threads_PMIN_INT_1;
extern int gEidos_OMP_threads_PMIN_INT_2;
extern int gEidos_OMP_threads_PMIN_FLOAT_1;
extern int gEidos_OMP_threads_PMIN_FLOAT_2;

// Eidos: match(), sample(), tabulate(); benchmark section V
extern int gEidos_OMP_threads_MATCH_INT;
extern int gEidos_OMP_threads_MATCH_FLOAT;
extern int gEidos_OMP_threads_MATCH_STRING;
extern int gEidos_OMP_threads_MATCH_OBJECT;
extern int gEidos_OMP_threads_SAMPLE_INDEX;
extern int gEidos_OMP_threads_SAMPLE_R_INT;
extern int gEidos_OMP_threads_SAMPLE_R_FLOAT;
extern int gEidos_OMP_threads_SAMPLE_R_OBJECT;
extern int gEidos_OMP_threads_SAMPLE_WR_INT;
extern int gEidos_OMP_threads_SAMPLE_WR_FLOAT;
extern int gEidos_OMP_threads_SAMPLE_WR_OBJECT;
extern int gEidos_OMP_threads_TABULATE_MAXBIN;
extern int gEidos_OMP_threads_TABULATE;

// SLiM methods/properties; benchmark section C
extern int gEidos_OMP_threads_CONTAINS_MARKER_MUT;
extern int gEidos_OMP_threads_I_COUNT_OF_MUTS_OF_TYPE;
extern int gEidos_OMP_threads_G_COUNT_OF_MUTS_OF_TYPE;
extern int gEidos_OMP_threads_INDS_W_PEDIGREE_IDS;
extern int gEidos_OMP_threads_RELATEDNESS;
extern int gEidos_OMP_threads_SAMPLE_INDIVIDUALS_1;
extern int gEidos_OMP_threads_SAMPLE_INDIVIDUALS_2;
extern int gEidos_OMP_threads_SET_FITNESS_SCALE_1;
extern int gEidos_OMP_threads_SET_FITNESS_SCALE_2;
extern int gEidos_OMP_threads_SUM_OF_MUTS_OF_TYPE;

// Distribution draws and related; benchmark section R
extern int gEidos_OMP_threads_DNORM_1;
extern int gEidos_OMP_threads_DNORM_2;
extern int gEidos_OMP_threads_RBINOM_1;
extern int gEidos_OMP_threads_RBINOM_2;
extern int gEidos_OMP_threads_RBINOM_3;
extern int gEidos_OMP_threads_RDUNIF_1;
extern int gEidos_OMP_threads_RDUNIF_2;
extern int gEidos_OMP_threads_RDUNIF_3;
extern int gEidos_OMP_threads_REXP_1;
extern int gEidos_OMP_threads_REXP_2;
extern int gEidos_OMP_threads_RNORM_1;
extern int gEidos_OMP_threads_RNORM_2;
extern int gEidos_OMP_threads_RNORM_3;
extern int gEidos_OMP_threads_RPOIS_1;
extern int gEidos_OMP_threads_RPOIS_2;
extern int gEidos_OMP_threads_RUNIF_1;
extern int gEidos_OMP_threads_RUNIF_2;
extern int gEidos_OMP_threads_RUNIF_3;

// Sorting & ordering
extern int gEidos_OMP_threads_SORT_INT;
extern int gEidos_OMP_threads_SORT_FLOAT;
extern int gEidos_OMP_threads_SORT_STRING;

// Spatial point/map manipulation; benchmark section P
extern int gEidos_OMP_threads_POINT_IN_BOUNDS_1D;
extern int gEidos_OMP_threads_POINT_IN_BOUNDS_2D;
extern int gEidos_OMP_threads_POINT_IN_BOUNDS_3D;
extern int gEidos_OMP_threads_POINT_PERIODIC_1D;
extern int gEidos_OMP_threads_POINT_PERIODIC_2D;
extern int gEidos_OMP_threads_POINT_PERIODIC_3D;
extern int gEidos_OMP_threads_POINT_REFLECTED_1D;
extern int gEidos_OMP_threads_POINT_REFLECTED_2D;
extern int gEidos_OMP_threads_POINT_REFLECTED_3D;
extern int gEidos_OMP_threads_POINT_STOPPED_1D;
extern int gEidos_OMP_threads_POINT_STOPPED_2D;
extern int gEidos_OMP_threads_POINT_STOPPED_3D;
extern int gEidos_OMP_threads_POINT_UNIFORM_1D;
extern int gEidos_OMP_threads_POINT_UNIFORM_2D;
extern int gEidos_OMP_threads_POINT_UNIFORM_3D;
extern int gEidos_OMP_threads_SET_SPATIAL_POS_1_1D;
extern int gEidos_OMP_threads_SET_SPATIAL_POS_1_2D;
extern int gEidos_OMP_threads_SET_SPATIAL_POS_1_3D;
extern int gEidos_OMP_threads_SET_SPATIAL_POS_2_1D;
extern int gEidos_OMP_threads_SET_SPATIAL_POS_2_2D;
extern int gEidos_OMP_threads_SET_SPATIAL_POS_2_3D;
extern int gEidos_OMP_threads_SPATIAL_MAP_VALUE;

// Spatial queries; benchmark sections D and S
extern int gEidos_OMP_threads_CLIPPEDINTEGRAL_1S;
extern int gEidos_OMP_threads_CLIPPEDINTEGRAL_2S;
//extern int gEidos_OMP_threads_CLIPPEDINTEGRAL_3S;
extern int gEidos_OMP_threads_DRAWBYSTRENGTH;
extern int gEidos_OMP_threads_INTNEIGHCOUNT;
extern int gEidos_OMP_threads_LOCALPOPDENSITY;
extern int gEidos_OMP_threads_NEARESTINTNEIGH;
extern int gEidos_OMP_threads_NEARESTNEIGH;
extern int gEidos_OMP_threads_NEIGHCOUNT;
extern int gEidos_OMP_threads_TOTNEIGHSTRENGTH;

// SLiM internals; benchmark section I
extern int gEidos_OMP_threads_AGE_INCR;
extern int gEidos_OMP_threads_DEFERRED_REPRO;
extern int gEidos_OMP_threads_WF_REPRO;
extern int gEidos_OMP_threads_FITNESS_ASEX_1;
extern int gEidos_OMP_threads_FITNESS_ASEX_2;
extern int gEidos_OMP_threads_FITNESS_ASEX_3;
extern int gEidos_OMP_threads_FITNESS_SEX_1;
extern int gEidos_OMP_threads_FITNESS_SEX_2;
extern int gEidos_OMP_threads_FITNESS_SEX_3;
extern int gEidos_OMP_threads_MIGRANT_CLEAR;
extern int gEidos_OMP_threads_SIMPLIFY_SORT_PRE;
extern int gEidos_OMP_threads_SIMPLIFY_SORT;
extern int gEidos_OMP_threads_SIMPLIFY_SORT_POST;
extern int gEidos_OMP_threads_PARENTS_CLEAR;
extern int gEidos_OMP_threads_UNIQUE_MUTRUNS;
extern int gEidos_OMP_threads_SURVIVAL;

// benchmark section M is for "models", whole SLiM models that test overall scaling
// for different model types; they do not correspond to per-task keys


#else /* ifdef _OPENMP */

// No OpenMP.  This is the "stub header" from the OpenMP 4.5 specification.  I've added "inline" in various spots to
// make it more performant, and __attribute__((unused)) to get rid of warnings, and copied some enum and typedef
// definitions that for some reason are not included in the provided stub header.

#include <stdio.h>
#include <stdlib.h>

inline void omp_set_num_threads(__attribute__((unused)) int num_threads)
{
}

inline int omp_get_num_threads(void)
{
	return 1;
}

inline int omp_get_max_threads(void)
{
	return 1;
}

inline int omp_get_thread_num(void)
{
	return 0;
}

inline int omp_get_num_procs(void)
{
	return 1;
}

inline int omp_in_parallel(void)
{
	return 0;
}

inline void omp_set_dynamic(__attribute__((unused)) int dynamic_threads)
{
}

inline int omp_get_dynamic(void)
{
	return 0;
}

inline int omp_get_cancellation(void)
{
	return 0;
}

inline void omp_set_nested(__attribute__((unused)) int nested)
{
}

inline int omp_get_nested(void)
{
	return 0;
}

typedef enum omp_sched_t {
	omp_sched_static = 1,
	omp_sched_dynamic = 2,
	omp_sched_guided = 3,
	omp_sched_auto = 4
} omp_sched_t;

inline void omp_set_schedule(__attribute__((unused)) omp_sched_t kind, __attribute__((unused)) int modifier)
{
}

inline void omp_get_schedule(omp_sched_t *kind, int *chunk_size)
{
	*kind = omp_sched_static;
	*chunk_size = 0;
}

inline int omp_get_thread_limit(void)
{
	return 1;
}

inline void omp_set_max_active_levels(__attribute__((unused)) int max_active_levels)
{
}

inline int omp_get_max_active_levels(void)
{
	return 0;
}

inline int omp_get_level(void)
{
	return 0;
}

inline int omp_get_ancestor_thread_num(int level)
{
	if (level == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

inline int omp_get_team_size(int level)
{
	if (level == 0)
	{
		return 1;
	}
	else
	{
		return -1;
	}
}

inline int omp_get_active_level(void)
{
	return 0;
}

inline int omp_in_final(void)
{
	return 1;
}

typedef enum omp_proc_bind_t
{
	omp_proc_bind_false = 0,
	omp_proc_bind_true = 1,
	omp_proc_bind_master = 2,
	omp_proc_bind_close = 3,
	omp_proc_bind_spread = 4
} omp_proc_bind_t;

inline omp_proc_bind_t omp_get_proc_bind(void)
{
	return omp_proc_bind_false;
}

inline int omp_get_num_places(void)
{
	return 0;
}

inline int omp_get_place_num_procs(__attribute__((unused)) int place_num)
{
	return 0;
}

inline void omp_get_place_proc_ids(__attribute__((unused)) int place_num, __attribute__((unused)) int *ids)
{
}

inline int omp_get_place_num(void)
{
	return -1;
}

inline int omp_get_partition_num_places(void)
{
	return 0;
}

inline void omp_get_partition_place_nums(__attribute__((unused)) int *place_nums)
{
}

inline void omp_set_default_device(__attribute__((unused)) int device_num)
{
}

inline int omp_get_default_device(void)
{
	return 0;
}

inline int omp_get_num_devices(void)
{
	return 0;
}

inline int omp_get_num_teams(void)
{
	return 1;
}

inline int omp_get_team_num(void)
{
	return 0;
}

inline int omp_is_initial_device(void)
{
	return 1;
}

inline int omp_get_initial_device(void)
{
	return -10;
}

inline int omp_get_max_task_priority(void)
{
	return 0;
}

struct __omp_lock
{
	int lock;
};

typedef struct __omp_lock omp_lock_t;

enum { UNLOCKED = -1, INIT, LOCKED };

inline void omp_init_lock(omp_lock_t *arg)
{
	struct __omp_lock *lock = (struct __omp_lock *)arg;
	lock->lock = UNLOCKED;
}

typedef enum omp_lock_hint_t
{
	omp_lock_hint_none = 0,
	omp_lock_hint_uncontended = 1,
	omp_lock_hint_contended = 2,
	omp_lock_hint_nonspeculative = 4,
	omp_lock_hint_speculative = 8
	/* , Add vendor specific constants for lock hints here,
	 starting from the most-significant bit. */
} omp_lock_hint_t;

inline void omp_init_lock_with_hint(omp_lock_t *arg, __attribute__((unused)) omp_lock_hint_t hint)
{
	omp_init_lock(arg);
}

inline void omp_destroy_lock(omp_lock_t *arg)
{
	struct __omp_lock *lock = (struct __omp_lock *)arg;
	lock->lock = INIT;
}

inline void omp_set_lock(omp_lock_t *arg)
{
	struct __omp_lock *lock = (struct __omp_lock *)arg;
	if (lock->lock == UNLOCKED)
	{
		lock->lock = LOCKED;
	}
	else if (lock->lock == LOCKED)
	{
		fprintf(stderr, "error: deadlock in using lock variable\n");
		exit(1);
	}
	else
	{
		fprintf(stderr, "error: lock not initialized\n");
		exit(1);
	}
}

inline void omp_unset_lock(omp_lock_t *arg)
{
	struct __omp_lock *lock = (struct __omp_lock *)arg;
	if (lock->lock == LOCKED)
	{
		lock->lock = UNLOCKED;
	}
	else if (lock->lock == UNLOCKED)
	{
		fprintf(stderr, "error: lock not set\n");
		exit(1);
	}
	else
	{
		fprintf(stderr, "error: lock not initialized\n");
		exit(1);
	}
}

inline int omp_test_lock(omp_lock_t *arg)
{
	struct __omp_lock *lock = (struct __omp_lock *)arg;
	if (lock->lock == UNLOCKED)
	{
		lock->lock = LOCKED;
		return 1;
	}
	else if (lock->lock == LOCKED)
	{
		return 0;
	}
	else
	{
		fprintf(stderr, "error: lock not initialized\n");
		exit(1);
	}
}

struct __omp_nest_lock
{
	short owner;
	short count;
};

typedef struct __omp_nest_lock omp_nest_lock_t;

enum { NOOWNER = -1, MASTER = 0 };

inline void omp_init_nest_lock(omp_nest_lock_t *arg)
{
	struct __omp_nest_lock *nlock=(struct __omp_nest_lock *)arg;
	nlock->owner = NOOWNER;
	nlock->count = 0;
}

inline void omp_init_nest_lock_with_hint(omp_nest_lock_t *arg, __attribute__((unused)) omp_lock_hint_t hint)
{
	omp_init_nest_lock(arg);
}

inline void omp_destroy_nest_lock(omp_nest_lock_t *arg)
{
	struct __omp_nest_lock *nlock=(struct __omp_nest_lock *)arg;
	nlock->owner = NOOWNER;
	nlock->count = UNLOCKED;
}

inline void omp_set_nest_lock(omp_nest_lock_t *arg)
{
	struct __omp_nest_lock *nlock=(struct __omp_nest_lock *)arg;
	if (nlock->owner == MASTER && nlock->count >= 1)
	{
		nlock->count++;
	}
	else if (nlock->owner == NOOWNER && nlock->count == 0)
	{
		nlock->owner = MASTER;
		nlock->count = 1;
	}
	else
	{
		fprintf(stderr, "error: lock corrupted or not initialized\n");
		exit(1);
	}
}

inline void omp_unset_nest_lock(omp_nest_lock_t *arg)
{
	struct __omp_nest_lock *nlock=(struct __omp_nest_lock *)arg;
	if (nlock->owner == MASTER && nlock->count >= 1)
	{
		nlock->count--;
		if (nlock->count == 0)
		{
			nlock->owner = NOOWNER;
		}
	}
	else if (nlock->owner == NOOWNER && nlock->count == 0)
	{
		fprintf(stderr, "error: lock not set\n");
		exit(1);
	}
	else
	{
		fprintf(stderr, "error: lock corrupted or not initialized\n");
		exit(1);
	}
}

inline int omp_test_nest_lock(omp_nest_lock_t *arg)
{
	struct __omp_nest_lock *nlock=(struct __omp_nest_lock *)arg;
	omp_set_nest_lock(arg);
	return nlock->count;
}

inline double omp_get_wtime(void)
{
	/* This function does not provide a working
	 * wallclock timer. Replace it with a version
	 * customized for the target machine.
	 */
	return 0.0;
}

inline double omp_get_wtick(void)
{
	/* This function does not provide a working
	 * clock tick function. Replace it with
	 * a version customized for the target machine.
	 */
	return 365. * 86400.;
}

inline void * omp_target_alloc(size_t size, int device_num)
{
	if (device_num != -10)
		return NULL;
	return malloc(size);
}

inline void omp_target_free(void *device_ptr, __attribute__((unused)) int device_num)
{
	free(device_ptr);
}

inline int omp_target_is_present(__attribute__((unused)) void *ptr, __attribute__((unused)) int device_num)
{
	return 1;
}

inline int omp_target_memcpy(void *dst, void *src, size_t length,
					  size_t dst_offset, size_t src_offset,
					  int dst_device, int src_device)
{
	// only the default device is valid in a stub
	if (dst_device != -10 || src_device != -10 || ! dst || ! src )
		return EINVAL;
	memcpy((char *)dst + dst_offset, (char *)src + src_offset, length);
	return 0;
}

inline int omp_target_memcpy_rect(
						   void *dst, void *src,
						   size_t element_size,
						   int num_dims,
						   const size_t *volume,
						   const size_t *dst_offsets,
						   const size_t *src_offsets,
						   const size_t *dst_dimensions,
						   const size_t *src_dimensions,
						   int dst_device_num, int src_device_num)
{
	int ret=0;
	// Both null, return number of dimensions supported,
	// this stub supports an arbitrary number
	if (dst == NULL && src == NULL) return INT_MAX;
	
	if (!volume || !dst_offsets || !src_offsets
		|| !dst_dimensions || !src_dimensions
		|| num_dims < 1 ) {
		ret = EINVAL;
		goto done;
	}
	if (num_dims == 1) {
		ret = omp_target_memcpy(dst, src,
								element_size * volume[0],
								dst_offsets[0] * element_size,
								src_offsets[0] * element_size,
								dst_device_num, src_device_num);
		if(ret) goto done;
	} else {
		size_t dst_slice_size = element_size;
		size_t src_slice_size = element_size;
		for (int i=1; i < num_dims; i++) {
			dst_slice_size *= dst_dimensions[i];
			src_slice_size *= src_dimensions[i];
		}
		size_t dst_off = dst_offsets[0] * dst_slice_size;
		size_t src_off = src_offsets[0] * src_slice_size;
		for (size_t i=0; i < volume[0]; i++) {
			ret = omp_target_memcpy_rect(
										 (char *)dst + dst_off + dst_slice_size*i,
										 (char *)src + src_off + src_slice_size*i,
										 element_size,
										 num_dims - 1,
										 volume + 1,
										 dst_offsets + 1,
										 src_offsets + 1,
										 dst_dimensions + 1,
										 src_dimensions + 1,
										 dst_device_num,
										 src_device_num);
			if (ret) goto done;
		}
	}
done:
	return ret;
}

inline int omp_target_associate_ptr(__attribute__((unused)) void *host_ptr, __attribute__((unused)) void *device_ptr,
									__attribute__((unused)) size_t size, __attribute__((unused)) size_t device_offset,
									__attribute__((unused)) int device_num)
{
	// No association is possible because all host pointers
	// are considered present
	return EINVAL;
}

inline int omp_target_disassociate_ptr(__attribute__((unused)) void *ptr, __attribute__((unused)) int device_num)
{
	return EINVAL;
}


#endif /* ifdef _OPENMP */


#endif /* eidos_openmp_h */

























