//
//  eidos_openmp.h
//  SLiM_OpenMP
//
//  Created by Benjamin C. Haller on 8/4/20.
//  Copyright (c) 2014-2022 Philipp Messer.  All rights reserved.
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

#include <signal.h>


// This is a cached result from omp_get_max_threads() after warmup, providing the final number of threads that we will
// be using (maximum) in parallel regions.  This can be used to preallocate per-thread data structures.
extern int gEidosMaxThreads;


// THREAD_SAFETY_CHECK(): places in the code that have identified thread safety concerns should use this macro.  It will
// produce a runtime error for DEBUG builds if it is hit while parallel.  Put it in places that are not currently thread-safe.
// For example, the RNG, object pools, and other such global state are not thread-safe right now, so they should use this.
// Many of these places might be made safe with a simple locking protocol, but that has not yet been done, so beware.
// This tagging of unsafe spots is undoubtedly not comprehensive; I'm just trying to catch the most obvious problems!
// Note that this macro uses a GCC built-in; it is supported by Clang as well, but may need a tweak for other platforms.
#ifdef _OPENMP

#if DEBUG
#define THREAD_SAFETY_CHECK() if (omp_in_parallel()) raise(SIGTRAP);
#else
#define THREAD_SAFETY_CHECK()
#endif

#else

#define THREAD_SAFETY_CHECK()

#endif


#ifdef _OPENMP

// Check that the OpenMP version supported by the compiler suffices.  Note that _OPENMP is formatted as a "YYYYMM" date of
// release.  See https://github.com/Kitware/CMake/blob/v3.16.3/Modules/FindOpenMP.cmake#L384 for dates of release.  For
// quick reference, "200805=3.0", "201107=3.1", "201307=4.0", "201511=4.5", "201811=5.0".  Right now we require 3.1.
#if (_OPENMP < 201107)
#error OpenMP version 3.1 or later is required.
#endif

// We're building SLiM for running in parallel, and OpenMP is present; include the header.
#include "omp.h"

// Define minimum counts for all the parallel loops we use.  Some of these loops are in SLiM, so we violate encapsulation
// here a bit; a slim_openmp.h header could be created to alleviate that if it's a problem, but it seems harmless for now.
// These counts are collected in one place to make it easier to optimize their values in a pre-build optimization pass.
#define EIDOS_OMPMIN_SUM_INTEGER			2000
#define EIDOS_OMPMIN_SUM_FLOAT				2000
#define EIDOS_OMPMIN_SUM_LOGICAL			6000
#define EIDOS_OMPMIN_SET_FITNESS_S1			900
#define EIDOS_OMPMIN_SET_FITNESS_S2			1500
#define EIDOS_OMPMIN_SUM_OF_MUTS_OF_TYPE	2

#else /* ifdef _OPENMP */

// No OpenMP.  This is the "stub header" from the OpenMP 3.1 specification.  I've added "inline" in various spots to
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

inline void omp_get_schedule(omp_sched_t *kind, int *modifier)
{
	*kind = omp_sched_static;
	*modifier = 0;
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

#endif /* ifdef _OPENMP */


#endif /* eidos_openmp_h */

























