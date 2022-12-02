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
	 use the -maxthreads <x> command-line option to change the maximum number of threads from OpenMP's default

 At present, we have parallelized these areas (this list may not always be up to date):
 
	Eidos_ExecuteFunction_sum()
	Eidos_ExecuteFunction_rnorm()
	Individual::SetProperty_Accelerated_fitnessScaling()
	Individual::ExecuteMethod_Accelerated_sumOfMutationsOfType()
	InteractionType::ExecuteMethod_interactingNeighborCount()
	InteractionType::ExecuteMethod_totalOfNeighborStrengths()
	EidosValue_Int_vector::Sort()
 
 We allocate per-thread storage (for gEidosMaxThreads threads) at the global level for these facilities:
 
	SparseVector pools; see s_freed_sparse_vectors_PERTHREAD vs. s_freed_sparse_vectors_SINGLE
	random number generators; see gEidos_RNG_PERTHREAD vs. gEidos_RNG_SINGLE
 
 We use #pragma omp critical to protect some places in the code, with specified names
 
 Places in the code that cannot be encountered when parallel use THREAD_SAFETY_CHECK(), defined below,
 as a runtime guard; but be aware that it only checks for Debug builds.  Release builds may just produce
 race conditions are incorrect results with no warning or error.  Always check with a Debug build.
 
 */

#ifndef eidos_openmp_h
#define eidos_openmp_h

#include <signal.h>
#include <limits.h>


// This is a cached result from omp_get_max_threads() after warmup, providing the final number of threads that we will
// be using (maximum) in parallel regions.  This can be used to preallocate per-thread data structures.
extern int gEidosMaxThreads;


// THREAD_SAFETY_CHECK(): places in the code that have identified thread safety concerns should use this macro.  It will
// produce a runtime error for DEBUG builds if it is hit while parallel.  Put it in places that are not currently thread-safe.
// For example, object pools and other such global state are not thread-safe right now, so they should use this.
// Many of these places might be made safe with a simple locking protocol, but that has not yet been done, so beware.
// This tagging of unsafe spots is undoubtedly not comprehensive; I'm just trying to catch the most obvious problems!
// Note that this macro uses a GCC built-in; it is supported by Clang as well, but may need a tweak for other platforms.
// This macros checks only for DEBUG builds!  When working on parallel code, be sure to check correctness with DEBUG!
#ifdef _OPENMP

#if DEBUG
#define THREAD_SAFETY_CHECK(s) if (omp_in_parallel()) { std::cerr << "THREAD_SAFETY_CHECK error in " << s; raise(SIGTRAP); }
#else
#define THREAD_SAFETY_CHECK(s)
#endif

#else

#define THREAD_SAFETY_CHECK(s)

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

// Define minimum counts for all the parallel loops we use.  Some of these loops are in SLiM, so we violate encapsulation
// here a bit; a slim_openmp.h header could be created to alleviate that if it's a problem, but it seems harmless for now.
// These counts are collected in one place to make it easier to optimize their values in a pre-build optimization pass.
#define EIDOS_OMPMIN_SUM_INTEGER			2000
#define EIDOS_OMPMIN_SUM_FLOAT				2000
#define EIDOS_OMPMIN_SUM_LOGICAL			6000
#define EIDOS_OMPMIN_SET_FITNESS_S1			900
#define EIDOS_OMPMIN_SET_FITNESS_S2			1500
#define EIDOS_OMPMIN_SUM_OF_MUTS_OF_TYPE	2
#define EIDOS_OMPMIN_RNORM_1				10000
#define EIDOS_OMPMIN_RNORM_2				10000
#define EIDOS_OMPMIN_RNORM_3				10000
#define EIDOS_OMPMIN_INTNEIGHCOUNT			10
#define EIDOS_OMPMIN_TOTNEIGHSTRENGTH		10

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
























