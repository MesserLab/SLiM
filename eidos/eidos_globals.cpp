//
//  eidos_globals.cpp
//  Eidos
//
//  Created by Ben Haller on 6/28/15.
//  Copyright (c) 2015-2023 Philipp Messer.  All rights reserved.
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


#include "eidos_globals.h"
#include "eidos_rng.h"
#include "eidos_script.h"
#include "eidos_value.h"
#include "eidos_interpreter.h"
#include "eidos_object_pool.h"
#include "eidos_ast_node.h"
#include "eidos_class_Object.h"
#include "eidos_class_Dictionary.h"
#include "eidos_class_DataFrame.h"
#include "eidos_class_Image.h"
#include "eidos_class_TestElement.h"

#include <stdlib.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <ctype.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#ifndef _WIN32
#include <pwd.h> // used only by Eidos_ResolvedPath(), which is not used on Windows
#endif
#include <unistd.h>
#include <algorithm>
#include "string.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <memory>
#include <limits>
#include <cmath>
#include <utility>
#include <iomanip>
#include <sys/param.h>
#include <regex>
#include <signal.h>

// added for Eidos_mkstemps() and Eidos_TemporaryDirectoryExists()
#include <sys/stat.h>
#include <fstream>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#ifdef _WIN32
#include <fileapi.h>
#endif

// for Eidos_WelchTTest()
#include "gsl_cdf.h"

// for Eidos_calc_sha_256()
#include <stdint.h>

// for _Eidos_FlushZipBuffer()
#include "../eidos_zlib/zlib.h"

// for Eidos_ColorPaletteLookup()
#include "eidos_tinycolormap.h"

//Replace some functions with their gnulib equivalents on Windows
#ifdef _WIN32
#define mkdir gnulib::mkdir
#define gettimeofday gnulib::gettimeofday
#endif

#ifdef _OPENMP
#include <stdlib.h>
#endif


// declared in eidos_openmp.h, set in Eidos_WarmUpOpenMP() when parallel
int gEidosMaxThreads = 1;
int gEidosNumThreads = 1;
bool gEidosNumThreadsOverride = false;


// Require 64-bit; apparently there are some issues on 32-bit, and nobody should be doing that anyway
static_assert(sizeof(char *) == 8, "SLiM must be built for 64-bit, not 32-bit.");


bool eidos_do_memory_checks = true;

EidosSymbolTable *gEidosConstantsSymbolTable = nullptr;

int gEidosFloatOutputPrecision = 6;

#if DEBUG_POINTS_ENABLED
int gEidosDebugIndent = 0;
#endif


#pragma mark -
#pragma mark Profiling support
#pragma mark -

// Base support used by EidosBenchmark as well as profiling
#if defined(MACH_PROFILING)

double Eidos_ElapsedProfileTime(eidos_profile_t p_elapsed_profile_time)
{
	// Eidos_ProfileTime() calls out to mach_absolute_time() at present.  It returns uint64_t, and the client would
	// then collect a start and end clock, subtract (end - start), and pass the result to this function to convert to
	// seconds as a double.  Interestingly, mach_absolute_time() uses CPU-specific units; we are close to the metal
	// here, which is why it is about twice as fast as other clock functions.  To convert a duration from CPU units,
	// we have to jump through a few hoops; see https://developer.apple.com/library/content/qa/qa1398/_index.html
	
	// If this is the first time we've run, get the timebase.  We can use denom == 0 to indicate that sTimebaseInfo
	// is uninitialised because it makes no sense to have a zero denominator in a fraction.
	static mach_timebase_info_data_t    sTimebaseInfo;
	static double timebaseRatio;
	
	if (sTimebaseInfo.denom == 0)
	{
		(void)mach_timebase_info(&sTimebaseInfo);
		
		// this ratio will convert from CPU time units to nanoseconds, AND from nanoseconds to seconds
		timebaseRatio = (sTimebaseInfo.numer / (double)sTimebaseInfo.denom) / 1000000000.0;
	}
	
	return p_elapsed_profile_time * timebaseRatio;
}

#elif defined(CHRONO_PROFILING)

double Eidos_ElapsedProfileTime(eidos_profile_t p_elapsed_profile_time)
{
	// Eidos_ProfileTime() provides time points in nanoseconds since epoch, and thus a duration is a duration in
	// nanoseconds.  We just need to convert from nanoseconds to seconds.
	return p_elapsed_profile_time / 1000000000.0;
}

#endif


// EidosBenchmark support

EidosBenchmarkType gEidosBenchmarkType = EidosBenchmarkType::kNone;
eidos_profile_t gEidosBenchmarkAccumulator = 0;


#if (SLIMPROFILING == 1)
// PROFILING

int gEidosProfilingClientCount = 0;

uint64_t gEidos_ProfileCounter;
double gEidos_ProfileOverheadTicks;
double gEidos_ProfileOverheadSeconds;
double gEidos_ProfileLagTicks;
double gEidos_ProfileLagSeconds;

static eidos_profile_t gEidos_ProfilePrep_Ticks;

void Eidos_PrepareForProfiling(void)
{
	// Prepare for profiling by measuring the overhead due to a profiling block itself
	// We will subtract out this overhead each time we use a profiling block, to compensate
	gEidos_ProfilePrep_Ticks = 0;
	gEidosProfilingClientCount++;
	
	gEidos_ProfileOverheadTicks = 0;
	gEidos_ProfileOverheadSeconds = 0;
	
	gEidos_ProfileLagTicks = 0;
	gEidos_ProfileLagSeconds = 0;
	
	eidos_profile_t clock1 = Eidos_ProfileTime();
	
	for (int i = 0; i < 1000000; ++i)
	{
		// Each iteration of this loop is meant to represent the overhead for one profiling block
		// Profiling blocks should all follow this structure for accuracy, even when it is overkill
		SLIM_PROFILE_BLOCK_START();
		
		;		// a null statement, so the measured execution time of this block should be zero
		
		SLIM_PROFILE_BLOCK_END(gEidos_ProfilePrep_Ticks);		// we use a global because real profile blocks will use a global
	}
	
	eidos_profile_t clock2 = Eidos_ProfileTime();
	
	gEidosProfilingClientCount--;
	
	eidos_profile_t profile_overhead_ticks = clock2 - clock1;
	
	gEidos_ProfileOverheadTicks = profile_overhead_ticks / 2000000.0;	// two increments of the profile counter per block
	gEidos_ProfileOverheadSeconds = Eidos_ElapsedProfileTime(profile_overhead_ticks) / 2000000.0;
	
	gEidos_ProfileLagTicks = gEidos_ProfilePrep_Ticks / 1000000.0;
	gEidos_ProfileLagSeconds = Eidos_ElapsedProfileTime(gEidos_ProfilePrep_Ticks) / 1000000.0;
	
	//std::cout << "Profile overhead external to block: " << gEidos_ProfileOverhead_double << " ticks, " << gEidos_ProfileOverheadSeconds << " seconds" << std::endl;
	//std::cout << "Profile lag internal to block: " << gEidos_ProfileLag_double << " ticks, " << gEidos_ProfileLagSeconds << " seconds" << std::endl;
}

#endif	// (SLIMPROFILING == 1)


#pragma mark -
#pragma mark Warm-up and command line processing
#pragma mark -

bool Eidos_GoodSymbolForDefine(std::string &p_symbol_name);
EidosValue_SP Eidos_ValueForCommandLineExpression(std::string &p_value_expression);


#ifdef _OPENMP

// Declarations for the number of threads we prefer to use for each parallel loop.
// These default values are all EIDOS_OMP_MAX_THREADS, to use the maximum number
// of threads in all cases.  This is primarily useful for benchmarking; normally
// these default values get overwritten by _Eidos_SetOpenMPThreadCounts().
int gEidos_OMP_threads_ABS_FLOAT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_CEIL = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_EXP_FLOAT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_FLOOR = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_LOG_FLOAT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_LOG10_FLOAT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_LOG2_FLOAT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_ROUND = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SQRT_FLOAT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SUM_INTEGER = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SUM_FLOAT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SUM_LOGICAL = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_TRUNC = EIDOS_OMP_MAX_THREADS;

int gEidos_OMP_threads_MAX_INT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_MAX_FLOAT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_MIN_INT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_MIN_FLOAT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_PMAX_INT_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_PMAX_INT_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_PMAX_FLOAT_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_PMAX_FLOAT_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_PMIN_INT_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_PMIN_INT_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_PMIN_FLOAT_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_PMIN_FLOAT_2 = EIDOS_OMP_MAX_THREADS;

int gEidos_OMP_threads_MATCH_INT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_MATCH_FLOAT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_MATCH_STRING = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_MATCH_OBJECT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SAMPLE_INDEX = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SAMPLE_R_INT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SAMPLE_R_FLOAT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SAMPLE_R_OBJECT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SAMPLE_WR_INT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SAMPLE_WR_FLOAT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SAMPLE_WR_OBJECT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_TABULATE_MAXBIN = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_TABULATE = EIDOS_OMP_MAX_THREADS;

int gEidos_OMP_threads_CONTAINS_MARKER_MUT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_I_COUNT_OF_MUTS_OF_TYPE = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_G_COUNT_OF_MUTS_OF_TYPE = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_INDS_W_PEDIGREE_IDS = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RELATEDNESS = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SAMPLE_INDIVIDUALS_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SAMPLE_INDIVIDUALS_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SET_FITNESS_SCALE_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SET_FITNESS_SCALE_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SUM_OF_MUTS_OF_TYPE = EIDOS_OMP_MAX_THREADS;

int gEidos_OMP_threads_DNORM_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_DNORM_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RBINOM_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RBINOM_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RBINOM_3 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RDUNIF_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RDUNIF_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RDUNIF_3 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_REXP_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_REXP_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RNORM_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RNORM_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RNORM_3 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RPOIS_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RPOIS_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RUNIF_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RUNIF_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_RUNIF_3 = EIDOS_OMP_MAX_THREADS;

int gEidos_OMP_threads_SORT_INT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SORT_FLOAT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SORT_STRING = EIDOS_OMP_MAX_THREADS;

int gEidos_OMP_threads_POINT_IN_BOUNDS_1D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_IN_BOUNDS_2D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_IN_BOUNDS_3D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_PERIODIC_1D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_PERIODIC_2D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_PERIODIC_3D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_REFLECTED_1D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_REFLECTED_2D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_REFLECTED_3D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_STOPPED_1D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_STOPPED_2D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_STOPPED_3D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_UNIFORM_1D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_UNIFORM_2D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_POINT_UNIFORM_3D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SET_SPATIAL_POS_1_1D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SET_SPATIAL_POS_1_2D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SET_SPATIAL_POS_1_3D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SET_SPATIAL_POS_2_1D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SET_SPATIAL_POS_2_2D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SET_SPATIAL_POS_2_3D = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SPATIAL_MAP_VALUE = EIDOS_OMP_MAX_THREADS;

int gEidos_OMP_threads_CLIPPEDINTEGRAL_1S = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_CLIPPEDINTEGRAL_2S = EIDOS_OMP_MAX_THREADS;
//int gEidos_OMP_threads_CLIPPEDINTEGRAL_3S = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_DRAWBYSTRENGTH = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_INTNEIGHCOUNT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_LOCALPOPDENSITY = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_NEARESTINTNEIGH = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_NEARESTNEIGH = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_NEIGHCOUNT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_TOTNEIGHSTRENGTH = EIDOS_OMP_MAX_THREADS;

int gEidos_OMP_threads_AGE_INCR = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_DEFERRED_REPRO = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_WF_REPRO = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_FITNESS_ASEX_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_FITNESS_ASEX_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_FITNESS_ASEX_3 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_FITNESS_SEX_1 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_FITNESS_SEX_2 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_FITNESS_SEX_3 = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_MIGRANT_CLEAR = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SIMPLIFY_SORT_PRE = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SIMPLIFY_SORT = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SIMPLIFY_SORT_POST = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_PARENTS_CLEAR = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_UNIQUE_MUTRUNS = EIDOS_OMP_MAX_THREADS;
int gEidos_OMP_threads_SURVIVAL = EIDOS_OMP_MAX_THREADS;

EidosPerTaskThreadCounts gEidosDefaultPerTaskThreadCounts = EidosPerTaskThreadCounts::kDefault;
std::string gEidosPerTaskThreadCountsSetName = "DEFAULT";	// should get overwritten
int gEidosPerTaskOriginalMaxThreadCount = EIDOS_OMP_MAX_THREADS;
int gEidosPerTaskClippedMaxThreadCount = EIDOS_OMP_MAX_THREADS;

void _Eidos_SetOpenMPThreadCounts(EidosPerTaskThreadCounts per_task_thread_counts)
{
	// This switches to a set of per-task thread counts.  Ideally, these are determined using the
	// SLiM-Benchmarks repo on GitHub, on the actual machine where production runs will be done.
	// Where the scaling curve tops out for a given test, that determines the default number of
	// threads that should be used (since performance degrades beyond that point).  The values
	// here come from tests on specific hardware that I use; they may or may not correspond to
	// what provides good performance on the end user's hardware!
	
	// One question is what to put in when a task scales all the way up to the maximum number of
	// threads that was tested.  For example, if tests went to 16 threads and it scaled to 16,
	// do you put 16, or do you put EIDOS_OMP_MAX_THREADS figuring that if someone uses those
	// per-task maximum thread counts on a similar machine with even more cores, the task might
	// well continue to scale?  This is a guess; it's extrapolating beyond the data we have.
	// But I have chosen, for that example, to use 16, not EIDOS_OMP_MAX_THREADS.  The user can
	// always fix this if they want to; better to err on the side of caution and not scale up
	// to levels where performance might become atrocious.
	
	if (per_task_thread_counts == EidosPerTaskThreadCounts::kMaxThreads)
	{
		// These are all EIDOS_OMP_MAX_THREADS, as a template for modification
		gEidosPerTaskThreadCountsSetName = "maxThreads";
		gEidosPerTaskOriginalMaxThreadCount = EIDOS_OMP_MAX_THREADS;
		gEidosPerTaskClippedMaxThreadCount = EIDOS_OMP_MAX_THREADS;
		
		gEidos_OMP_threads_ABS_FLOAT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_CEIL = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_EXP_FLOAT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_FLOOR = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_LOG_FLOAT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_LOG10_FLOAT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_LOG2_FLOAT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_ROUND = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SQRT_FLOAT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SUM_INTEGER = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SUM_FLOAT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SUM_LOGICAL = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_TRUNC = EIDOS_OMP_MAX_THREADS;
		
		gEidos_OMP_threads_MAX_INT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_MAX_FLOAT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_MIN_INT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_MIN_FLOAT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_PMAX_INT_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_PMAX_INT_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_PMAX_FLOAT_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_PMAX_FLOAT_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_PMIN_INT_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_PMIN_INT_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_PMIN_FLOAT_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_PMIN_FLOAT_2 = EIDOS_OMP_MAX_THREADS;
		
		gEidos_OMP_threads_MATCH_INT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_MATCH_FLOAT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_MATCH_STRING = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_MATCH_OBJECT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SAMPLE_INDEX = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SAMPLE_R_INT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SAMPLE_R_FLOAT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SAMPLE_R_OBJECT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SAMPLE_WR_INT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SAMPLE_WR_FLOAT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SAMPLE_WR_OBJECT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_TABULATE_MAXBIN = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_TABULATE = EIDOS_OMP_MAX_THREADS;
		
		gEidos_OMP_threads_CONTAINS_MARKER_MUT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_I_COUNT_OF_MUTS_OF_TYPE = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_G_COUNT_OF_MUTS_OF_TYPE = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_INDS_W_PEDIGREE_IDS = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RELATEDNESS = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SAMPLE_INDIVIDUALS_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SAMPLE_INDIVIDUALS_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SET_FITNESS_SCALE_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SET_FITNESS_SCALE_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SUM_OF_MUTS_OF_TYPE = EIDOS_OMP_MAX_THREADS;
		
		gEidos_OMP_threads_DNORM_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_DNORM_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RBINOM_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RBINOM_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RBINOM_3 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RDUNIF_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RDUNIF_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RDUNIF_3 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_REXP_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_REXP_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RNORM_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RNORM_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RNORM_3 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RPOIS_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RPOIS_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RUNIF_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RUNIF_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_RUNIF_3 = EIDOS_OMP_MAX_THREADS;
		
		gEidos_OMP_threads_SORT_INT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SORT_FLOAT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SORT_STRING = EIDOS_OMP_MAX_THREADS;
		
		gEidos_OMP_threads_POINT_IN_BOUNDS_1D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_IN_BOUNDS_2D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_IN_BOUNDS_3D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_PERIODIC_1D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_PERIODIC_2D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_PERIODIC_3D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_REFLECTED_1D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_REFLECTED_2D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_REFLECTED_3D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_STOPPED_1D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_STOPPED_2D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_STOPPED_3D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_UNIFORM_1D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_UNIFORM_2D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_POINT_UNIFORM_3D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SET_SPATIAL_POS_1_1D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SET_SPATIAL_POS_1_2D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SET_SPATIAL_POS_1_3D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SET_SPATIAL_POS_2_1D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SET_SPATIAL_POS_2_2D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SET_SPATIAL_POS_2_3D = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SPATIAL_MAP_VALUE = EIDOS_OMP_MAX_THREADS;
		
		gEidos_OMP_threads_CLIPPEDINTEGRAL_1S = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_CLIPPEDINTEGRAL_2S = EIDOS_OMP_MAX_THREADS;
		//gEidos_OMP_threads_CLIPPEDINTEGRAL_3S = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_DRAWBYSTRENGTH = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_INTNEIGHCOUNT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_LOCALPOPDENSITY = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_NEARESTINTNEIGH = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_NEARESTNEIGH = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_NEIGHCOUNT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_TOTNEIGHSTRENGTH = EIDOS_OMP_MAX_THREADS;
		
		gEidos_OMP_threads_AGE_INCR = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_DEFERRED_REPRO = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_WF_REPRO = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_FITNESS_ASEX_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_FITNESS_ASEX_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_FITNESS_ASEX_3 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_FITNESS_SEX_1 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_FITNESS_SEX_2 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_FITNESS_SEX_3 = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_MIGRANT_CLEAR = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SIMPLIFY_SORT_PRE = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SIMPLIFY_SORT = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SIMPLIFY_SORT_POST = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_PARENTS_CLEAR = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_UNIQUE_MUTRUNS = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_SURVIVAL = EIDOS_OMP_MAX_THREADS;
	}
	else if (per_task_thread_counts == EidosPerTaskThreadCounts::kMacStudio2022_16)
	{
		// These counts are from a Mac Studio 2022 (Mac13,2), 20-core M1 Ultra, 128 GB
		// It has 20 cores: 16 performance cores and 4 efficiency cores
		// An effort was made with OMP_PLACES and OMP_PROC_BIND to stay on the perf cores,
		// but I don't know how to tell whether that effort was successful or not, so.
		// The raw data for these choices is presently in benchmarking/STUDIO 2023-08-07
		gEidosPerTaskThreadCountsSetName = "MacStudio2022_16";
		gEidosPerTaskOriginalMaxThreadCount = 16;
		gEidosPerTaskClippedMaxThreadCount = 16;
		
		gEidos_OMP_threads_ABS_FLOAT = 8;
		gEidos_OMP_threads_CEIL = 8;
		gEidos_OMP_threads_EXP_FLOAT = 16;
		gEidos_OMP_threads_FLOOR = 8;
		gEidos_OMP_threads_LOG_FLOAT = 16;
		gEidos_OMP_threads_LOG10_FLOAT = 16;
		gEidos_OMP_threads_LOG2_FLOAT = 16;
		gEidos_OMP_threads_ROUND = 8;
		gEidos_OMP_threads_SQRT_FLOAT = 8;
		gEidos_OMP_threads_SUM_INTEGER = 8;
		gEidos_OMP_threads_SUM_FLOAT = 8;
		gEidos_OMP_threads_SUM_LOGICAL = 8;
		gEidos_OMP_threads_TRUNC = 8;
		
		gEidos_OMP_threads_MAX_INT = 8;
		gEidos_OMP_threads_MAX_FLOAT = 16;
		gEidos_OMP_threads_MIN_INT = 8;
		gEidos_OMP_threads_MIN_FLOAT = 16;
		gEidos_OMP_threads_PMAX_INT_1 = 8;
		gEidos_OMP_threads_PMAX_INT_2 = 8;
		gEidos_OMP_threads_PMAX_FLOAT_1 = 16;
		gEidos_OMP_threads_PMAX_FLOAT_2 = 16;
		gEidos_OMP_threads_PMIN_INT_1 = 8;
		gEidos_OMP_threads_PMIN_INT_2 = 8;
		gEidos_OMP_threads_PMIN_FLOAT_1 = 16;
		gEidos_OMP_threads_PMIN_FLOAT_2 = 16;
		
		gEidos_OMP_threads_MATCH_INT = 16;
		gEidos_OMP_threads_MATCH_FLOAT = 16;
		gEidos_OMP_threads_MATCH_STRING = 16;
		gEidos_OMP_threads_MATCH_OBJECT = 16;
		gEidos_OMP_threads_SAMPLE_INDEX = 12;
		gEidos_OMP_threads_SAMPLE_R_INT = 16;
		gEidos_OMP_threads_SAMPLE_R_FLOAT = 16;
		gEidos_OMP_threads_SAMPLE_R_OBJECT = 16;
		gEidos_OMP_threads_SAMPLE_WR_INT = 12;
		gEidos_OMP_threads_SAMPLE_WR_FLOAT = 8;
		gEidos_OMP_threads_SAMPLE_WR_OBJECT = 16;
		gEidos_OMP_threads_TABULATE_MAXBIN = 8;
		gEidos_OMP_threads_TABULATE = 16;
		
		gEidos_OMP_threads_CONTAINS_MARKER_MUT = 16;
		gEidos_OMP_threads_I_COUNT_OF_MUTS_OF_TYPE = 16;
		gEidos_OMP_threads_G_COUNT_OF_MUTS_OF_TYPE = 16;
		gEidos_OMP_threads_INDS_W_PEDIGREE_IDS = 8;
		gEidos_OMP_threads_RELATEDNESS = 16;
		gEidos_OMP_threads_SAMPLE_INDIVIDUALS_1 = 12;
		gEidos_OMP_threads_SAMPLE_INDIVIDUALS_2 = 12;
		gEidos_OMP_threads_SET_FITNESS_SCALE_1 = 8;
		gEidos_OMP_threads_SET_FITNESS_SCALE_2 = 8;
		gEidos_OMP_threads_SUM_OF_MUTS_OF_TYPE = 16;
		
		gEidos_OMP_threads_DNORM_1 = 16;
		gEidos_OMP_threads_DNORM_2 = 16;
		gEidos_OMP_threads_RBINOM_1 = 16;
		gEidos_OMP_threads_RBINOM_2 = 16;
		gEidos_OMP_threads_RBINOM_3 = 16;
		gEidos_OMP_threads_RDUNIF_1 = 16;
		gEidos_OMP_threads_RDUNIF_2 = 16;
		gEidos_OMP_threads_RDUNIF_3 = 16;
		gEidos_OMP_threads_REXP_1 = 16;
		gEidos_OMP_threads_REXP_2 = 16;
		gEidos_OMP_threads_RNORM_1 = 16;
		gEidos_OMP_threads_RNORM_2 = 16;
		gEidos_OMP_threads_RNORM_3 = 16;
		gEidos_OMP_threads_RPOIS_1 = 16;
		gEidos_OMP_threads_RPOIS_2 = 16;
		gEidos_OMP_threads_RUNIF_1 = 16;
		gEidos_OMP_threads_RUNIF_2 = 16;
		gEidos_OMP_threads_RUNIF_3 = 16;
		
		gEidos_OMP_threads_SORT_INT = 16;
		gEidos_OMP_threads_SORT_FLOAT = 4;
		gEidos_OMP_threads_SORT_STRING = 16;
		
		gEidos_OMP_threads_POINT_IN_BOUNDS_1D = 12;
		gEidos_OMP_threads_POINT_IN_BOUNDS_2D = 12;
		gEidos_OMP_threads_POINT_IN_BOUNDS_3D = 16;
		gEidos_OMP_threads_POINT_PERIODIC_1D = 16;
		gEidos_OMP_threads_POINT_PERIODIC_2D = 16;
		gEidos_OMP_threads_POINT_PERIODIC_3D = 16;
		gEidos_OMP_threads_POINT_REFLECTED_1D = 16;
		gEidos_OMP_threads_POINT_REFLECTED_2D = 16;
		gEidos_OMP_threads_POINT_REFLECTED_3D = 16;
		gEidos_OMP_threads_POINT_STOPPED_1D = 16;
		gEidos_OMP_threads_POINT_STOPPED_2D = 8;
		gEidos_OMP_threads_POINT_STOPPED_3D = 8;
		gEidos_OMP_threads_POINT_UNIFORM_1D = 16;
		gEidos_OMP_threads_POINT_UNIFORM_2D = 16;
		gEidos_OMP_threads_POINT_UNIFORM_3D = 16;
		gEidos_OMP_threads_SET_SPATIAL_POS_1_1D = 4;
		gEidos_OMP_threads_SET_SPATIAL_POS_1_2D = 4;
		gEidos_OMP_threads_SET_SPATIAL_POS_1_3D = 4;
		gEidos_OMP_threads_SET_SPATIAL_POS_2_1D = 4;
		gEidos_OMP_threads_SET_SPATIAL_POS_2_2D = 4;
		gEidos_OMP_threads_SET_SPATIAL_POS_2_3D = 4;
		gEidos_OMP_threads_SPATIAL_MAP_VALUE = 16;
		
		gEidos_OMP_threads_CLIPPEDINTEGRAL_1S = 16;
		gEidos_OMP_threads_CLIPPEDINTEGRAL_2S = 16;
		//gEidos_OMP_threads_CLIPPEDINTEGRAL_3S = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_DRAWBYSTRENGTH = 16;
		gEidos_OMP_threads_INTNEIGHCOUNT = 16;
		gEidos_OMP_threads_LOCALPOPDENSITY = 16;
		gEidos_OMP_threads_NEARESTINTNEIGH = 16;
		gEidos_OMP_threads_NEARESTNEIGH = 16;
		gEidos_OMP_threads_NEIGHCOUNT = 16;
		gEidos_OMP_threads_TOTNEIGHSTRENGTH = 16;
		
		gEidos_OMP_threads_AGE_INCR = 4;
		gEidos_OMP_threads_DEFERRED_REPRO = 4;
		gEidos_OMP_threads_WF_REPRO = 4;
		gEidos_OMP_threads_FITNESS_ASEX_1 = 8;
		gEidos_OMP_threads_FITNESS_ASEX_2 = 8;
		gEidos_OMP_threads_FITNESS_ASEX_3 = 2;
		gEidos_OMP_threads_FITNESS_SEX_1 = 8;
		gEidos_OMP_threads_FITNESS_SEX_2 = 8;
		gEidos_OMP_threads_FITNESS_SEX_3 = 2;
		gEidos_OMP_threads_MIGRANT_CLEAR = 4;
		gEidos_OMP_threads_SIMPLIFY_SORT_PRE = 8;
		gEidos_OMP_threads_SIMPLIFY_SORT = 16;
		gEidos_OMP_threads_SIMPLIFY_SORT_POST = 6;
		gEidos_OMP_threads_PARENTS_CLEAR = 16;
		gEidos_OMP_threads_UNIQUE_MUTRUNS = 16;
		gEidos_OMP_threads_SURVIVAL = 16;
	}
	else if (per_task_thread_counts == EidosPerTaskThreadCounts::kXeonGold2_40)
	{
		// These counts are from cbsulm21, a node in Cornell's BioHPC cluster
		// It has two 20-core (40-hyperthreaded) Intel Xeon Gold 6148 2.4GHz
		// That makes a total of 40 physical cores, 80 virtual cores
		// These tests went up to 40 cores, avoiding hyperthreading
		// The raw data for these choices is presently in benchmarking/BHPC 2023-08-07
		// These should be the defaults for production builds, on the
		// assumption that users will be on similar big HPC nodes
		gEidosPerTaskThreadCountsSetName = "XeonGold2_40";
		gEidosPerTaskOriginalMaxThreadCount = 40;
		gEidosPerTaskClippedMaxThreadCount = 40;
		
		gEidos_OMP_threads_ABS_FLOAT = 40;
		gEidos_OMP_threads_CEIL = 40;
		gEidos_OMP_threads_EXP_FLOAT = 40;
		gEidos_OMP_threads_FLOOR = 40;
		gEidos_OMP_threads_LOG_FLOAT = 40;
		gEidos_OMP_threads_LOG10_FLOAT = 40;
		gEidos_OMP_threads_LOG2_FLOAT = 40;
		gEidos_OMP_threads_ROUND = 40;
		gEidos_OMP_threads_SQRT_FLOAT = 40;
		gEidos_OMP_threads_SUM_INTEGER = 40;
		gEidos_OMP_threads_SUM_FLOAT = 40;
		gEidos_OMP_threads_SUM_LOGICAL = 40;
		gEidos_OMP_threads_TRUNC = 40;
		
		gEidos_OMP_threads_MAX_INT = 40;
		gEidos_OMP_threads_MAX_FLOAT = 40;
		gEidos_OMP_threads_MIN_INT = 40;
		gEidos_OMP_threads_MIN_FLOAT = 40;
		gEidos_OMP_threads_PMAX_INT_1 = 40;
		gEidos_OMP_threads_PMAX_INT_2 = 40;
		gEidos_OMP_threads_PMAX_FLOAT_1 = 40;
		gEidos_OMP_threads_PMAX_FLOAT_2 = 40;
		gEidos_OMP_threads_PMIN_INT_1 = 40;
		gEidos_OMP_threads_PMIN_INT_2 = 40;
		gEidos_OMP_threads_PMIN_FLOAT_1 = 40;
		gEidos_OMP_threads_PMIN_FLOAT_2 = 40;
		
		gEidos_OMP_threads_MATCH_INT = 40;
		gEidos_OMP_threads_MATCH_FLOAT = 40;
		gEidos_OMP_threads_MATCH_STRING = 40;
		gEidos_OMP_threads_MATCH_OBJECT = 40;
		gEidos_OMP_threads_SAMPLE_INDEX = 40;
		gEidos_OMP_threads_SAMPLE_R_INT = 40;
		gEidos_OMP_threads_SAMPLE_R_FLOAT = 40;
		gEidos_OMP_threads_SAMPLE_R_OBJECT = 40;
		gEidos_OMP_threads_SAMPLE_WR_INT = 40;
		gEidos_OMP_threads_SAMPLE_WR_FLOAT = 40;
		gEidos_OMP_threads_SAMPLE_WR_OBJECT = 40;
		gEidos_OMP_threads_TABULATE_MAXBIN = 40;
		gEidos_OMP_threads_TABULATE = 20;
		
		gEidos_OMP_threads_CONTAINS_MARKER_MUT = 40;
		gEidos_OMP_threads_I_COUNT_OF_MUTS_OF_TYPE = 40;
		gEidos_OMP_threads_G_COUNT_OF_MUTS_OF_TYPE = 40;
		gEidos_OMP_threads_INDS_W_PEDIGREE_IDS = 5;
		gEidos_OMP_threads_RELATEDNESS = 40;
		gEidos_OMP_threads_SAMPLE_INDIVIDUALS_1 = 40;
		gEidos_OMP_threads_SAMPLE_INDIVIDUALS_2 = 40;
		gEidos_OMP_threads_SET_FITNESS_SCALE_1 = 40;
		gEidos_OMP_threads_SET_FITNESS_SCALE_2 = 40;
		gEidos_OMP_threads_SUM_OF_MUTS_OF_TYPE = 40;
		
		gEidos_OMP_threads_DNORM_1 = 40;
		gEidos_OMP_threads_DNORM_2 = 40;
		gEidos_OMP_threads_RBINOM_1 = 10;
		gEidos_OMP_threads_RBINOM_2 = 40;
		gEidos_OMP_threads_RBINOM_3 = 40;
		gEidos_OMP_threads_RDUNIF_1 = 10;
		gEidos_OMP_threads_RDUNIF_2 = 10;
		gEidos_OMP_threads_RDUNIF_3 = 20;
		gEidos_OMP_threads_REXP_1 = 40;
		gEidos_OMP_threads_REXP_2 = 40;
		gEidos_OMP_threads_RNORM_1 = 40;
		gEidos_OMP_threads_RNORM_2 = 40;
		gEidos_OMP_threads_RNORM_3 = 40;
		gEidos_OMP_threads_RPOIS_1 = 40;
		gEidos_OMP_threads_RPOIS_2 = 40;
		gEidos_OMP_threads_RUNIF_1 = 40;
		gEidos_OMP_threads_RUNIF_2 = 40;
		gEidos_OMP_threads_RUNIF_3 = 40;
		
		gEidos_OMP_threads_SORT_INT = 10;
		gEidos_OMP_threads_SORT_FLOAT = 10;
		gEidos_OMP_threads_SORT_STRING = 10;
		
		gEidos_OMP_threads_POINT_IN_BOUNDS_1D = 40;
		gEidos_OMP_threads_POINT_IN_BOUNDS_2D = 40;
		gEidos_OMP_threads_POINT_IN_BOUNDS_3D = 40;
		gEidos_OMP_threads_POINT_PERIODIC_1D = 40;
		gEidos_OMP_threads_POINT_PERIODIC_2D = 40;
		gEidos_OMP_threads_POINT_PERIODIC_3D = 40;
		gEidos_OMP_threads_POINT_REFLECTED_1D = 40;
		gEidos_OMP_threads_POINT_REFLECTED_2D = 40;
		gEidos_OMP_threads_POINT_REFLECTED_3D = 40;
		gEidos_OMP_threads_POINT_STOPPED_1D = 40;
		gEidos_OMP_threads_POINT_STOPPED_2D = 40;
		gEidos_OMP_threads_POINT_STOPPED_3D = 40;
		gEidos_OMP_threads_POINT_UNIFORM_1D = 40;
		gEidos_OMP_threads_POINT_UNIFORM_2D = 40;
		gEidos_OMP_threads_POINT_UNIFORM_3D = 40;
		gEidos_OMP_threads_SET_SPATIAL_POS_1_1D = 5;
		gEidos_OMP_threads_SET_SPATIAL_POS_1_2D = 20;
		gEidos_OMP_threads_SET_SPATIAL_POS_1_3D = 20;
		gEidos_OMP_threads_SET_SPATIAL_POS_2_1D = 10;
		gEidos_OMP_threads_SET_SPATIAL_POS_2_2D = 20;
		gEidos_OMP_threads_SET_SPATIAL_POS_2_3D = 20;
		gEidos_OMP_threads_SPATIAL_MAP_VALUE = 40;
		
		gEidos_OMP_threads_CLIPPEDINTEGRAL_1S = 40;
		gEidos_OMP_threads_CLIPPEDINTEGRAL_2S = 40;
		//gEidos_OMP_threads_CLIPPEDINTEGRAL_3S = EIDOS_OMP_MAX_THREADS;
		gEidos_OMP_threads_DRAWBYSTRENGTH = 40;
		gEidos_OMP_threads_INTNEIGHCOUNT = 40;
		gEidos_OMP_threads_LOCALPOPDENSITY = 40;
		gEidos_OMP_threads_NEARESTINTNEIGH = 10;
		gEidos_OMP_threads_NEARESTNEIGH = 10;
		gEidos_OMP_threads_NEIGHCOUNT = 40;
		gEidos_OMP_threads_TOTNEIGHSTRENGTH = 40;
		
		gEidos_OMP_threads_AGE_INCR = 10;
		gEidos_OMP_threads_DEFERRED_REPRO = 5;
		gEidos_OMP_threads_WF_REPRO = 5;
		gEidos_OMP_threads_FITNESS_ASEX_1 = 40;
		gEidos_OMP_threads_FITNESS_ASEX_2 = 40;
		gEidos_OMP_threads_FITNESS_ASEX_3 = 5;
		gEidos_OMP_threads_FITNESS_SEX_1 = 40;
		gEidos_OMP_threads_FITNESS_SEX_2 = 40;
		gEidos_OMP_threads_FITNESS_SEX_3 = 5;
		gEidos_OMP_threads_MIGRANT_CLEAR = 20;
		gEidos_OMP_threads_SIMPLIFY_SORT_PRE = 20;
		gEidos_OMP_threads_SIMPLIFY_SORT = 40;
		gEidos_OMP_threads_SIMPLIFY_SORT_POST = 40;
		gEidos_OMP_threads_PARENTS_CLEAR = 40;
		gEidos_OMP_threads_UNIQUE_MUTRUNS = 40;
		gEidos_OMP_threads_SURVIVAL = 40;
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (_Eidos_SetOpenMPThreadCounts): (internal error) unrecognized EidosPerTaskThreadCounts value." << EidosTerminate(nullptr);
	}
	
	// Always clip the above counts to gEidosMaxThreads
	_Eidos_ClipOpenMPThreadCounts();
}

void _Eidos_ChooseDefaultOpenMPThreadCounts()
{
#if USE_OMP_LIMITS
	
	// If we are supposed to use our built-in default OMP limits, set them for our task thread counts
	// Note that the default behavior here is nothing but a wild shot in the dark!
#ifdef __APPLE__
	// On macOS, we use the results from my Mac Studio 2022 by default; note it maxes out at 16 threads
	gEidosDefaultPerTaskThreadCounts = EidosPerTaskThreadCounts::kMacStudio2022_16;
#else
	// On other systems, we use the results from the Cornell BioHPC cluster machine I test on, with a max of 40 threads
	gEidosDefaultPerTaskThreadCounts = EidosPerTaskThreadCounts::kXeonGold2_40;
#endif
	
#else
	
	// Enforce gEidosMaxThreads for the thread count ivars that govern how many threads various loops will use
	gEidosDefaultPerTaskThreadCounts = EidosPerTaskThreadCounts::kMaxThreads;
	
#endif
	
	_Eidos_SetOpenMPThreadCounts(gEidosDefaultPerTaskThreadCounts);
}

void _Eidos_ClipOpenMPThreadCounts(void)
{
	// This clips all thread-count ivars to gEidosMaxThreads, so they can be used at runtime without checking
	gEidosPerTaskClippedMaxThreadCount = std::min(gEidosMaxThreads, gEidosPerTaskOriginalMaxThreadCount);
	
	gEidos_OMP_threads_ABS_FLOAT = std::min(gEidosMaxThreads, gEidos_OMP_threads_ABS_FLOAT);
	gEidos_OMP_threads_CEIL = std::min(gEidosMaxThreads, gEidos_OMP_threads_CEIL);
	gEidos_OMP_threads_EXP_FLOAT = std::min(gEidosMaxThreads, gEidos_OMP_threads_EXP_FLOAT);
	gEidos_OMP_threads_FLOOR = std::min(gEidosMaxThreads, gEidos_OMP_threads_FLOOR);
	gEidos_OMP_threads_LOG_FLOAT = std::min(gEidosMaxThreads, gEidos_OMP_threads_LOG_FLOAT);
	gEidos_OMP_threads_LOG10_FLOAT = std::min(gEidosMaxThreads, gEidos_OMP_threads_LOG10_FLOAT);
	gEidos_OMP_threads_LOG2_FLOAT = std::min(gEidosMaxThreads, gEidos_OMP_threads_LOG2_FLOAT);
	gEidos_OMP_threads_ROUND = std::min(gEidosMaxThreads, gEidos_OMP_threads_ROUND);
	gEidos_OMP_threads_SQRT_FLOAT = std::min(gEidosMaxThreads, gEidos_OMP_threads_SQRT_FLOAT);
	gEidos_OMP_threads_SUM_INTEGER = std::min(gEidosMaxThreads, gEidos_OMP_threads_SUM_INTEGER);
	gEidos_OMP_threads_SUM_FLOAT = std::min(gEidosMaxThreads, gEidos_OMP_threads_SUM_FLOAT);
	gEidos_OMP_threads_SUM_LOGICAL = std::min(gEidosMaxThreads, gEidos_OMP_threads_SUM_LOGICAL);
	gEidos_OMP_threads_TRUNC = std::min(gEidosMaxThreads, gEidos_OMP_threads_TRUNC);

	gEidos_OMP_threads_MAX_INT = std::min(gEidosMaxThreads, gEidos_OMP_threads_MAX_INT);
	gEidos_OMP_threads_MAX_FLOAT = std::min(gEidosMaxThreads, gEidos_OMP_threads_MAX_FLOAT);
	gEidos_OMP_threads_MIN_INT = std::min(gEidosMaxThreads, gEidos_OMP_threads_MIN_INT);
	gEidos_OMP_threads_MIN_FLOAT = std::min(gEidosMaxThreads, gEidos_OMP_threads_MIN_FLOAT);
	gEidos_OMP_threads_PMAX_INT_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_PMAX_INT_1);
	gEidos_OMP_threads_PMAX_INT_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_PMAX_INT_2);
	gEidos_OMP_threads_PMAX_FLOAT_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_PMAX_FLOAT_1);
	gEidos_OMP_threads_PMAX_FLOAT_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_PMAX_FLOAT_2);
	gEidos_OMP_threads_PMIN_INT_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_PMIN_INT_1);
	gEidos_OMP_threads_PMIN_INT_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_PMIN_INT_2);
	gEidos_OMP_threads_PMIN_FLOAT_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_PMIN_FLOAT_1);
	gEidos_OMP_threads_PMIN_FLOAT_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_PMIN_FLOAT_2);

	gEidos_OMP_threads_MATCH_INT = std::min(gEidosMaxThreads, gEidos_OMP_threads_MATCH_INT);
	gEidos_OMP_threads_MATCH_FLOAT = std::min(gEidosMaxThreads, gEidos_OMP_threads_MATCH_FLOAT);
	gEidos_OMP_threads_MATCH_STRING = std::min(gEidosMaxThreads, gEidos_OMP_threads_MATCH_STRING);
	gEidos_OMP_threads_MATCH_OBJECT = std::min(gEidosMaxThreads, gEidos_OMP_threads_MATCH_OBJECT);
	gEidos_OMP_threads_SAMPLE_INDEX = std::min(gEidosMaxThreads, gEidos_OMP_threads_SAMPLE_INDEX);
	gEidos_OMP_threads_SAMPLE_R_INT = std::min(gEidosMaxThreads, gEidos_OMP_threads_SAMPLE_R_INT);
	gEidos_OMP_threads_SAMPLE_R_FLOAT = std::min(gEidosMaxThreads, gEidos_OMP_threads_SAMPLE_R_FLOAT);
	gEidos_OMP_threads_SAMPLE_R_OBJECT = std::min(gEidosMaxThreads, gEidos_OMP_threads_SAMPLE_R_OBJECT);
	gEidos_OMP_threads_SAMPLE_WR_INT = std::min(gEidosMaxThreads, gEidos_OMP_threads_SAMPLE_WR_INT);
	gEidos_OMP_threads_SAMPLE_WR_FLOAT = std::min(gEidosMaxThreads, gEidos_OMP_threads_SAMPLE_WR_FLOAT);
	gEidos_OMP_threads_SAMPLE_WR_OBJECT = std::min(gEidosMaxThreads, gEidos_OMP_threads_SAMPLE_WR_OBJECT);
	gEidos_OMP_threads_TABULATE_MAXBIN = std::min(gEidosMaxThreads, gEidos_OMP_threads_TABULATE_MAXBIN);
	gEidos_OMP_threads_TABULATE = std::min(gEidosMaxThreads, gEidos_OMP_threads_TABULATE);

	gEidos_OMP_threads_CONTAINS_MARKER_MUT = std::min(gEidosMaxThreads, gEidos_OMP_threads_CONTAINS_MARKER_MUT);
	gEidos_OMP_threads_I_COUNT_OF_MUTS_OF_TYPE = std::min(gEidosMaxThreads, gEidos_OMP_threads_I_COUNT_OF_MUTS_OF_TYPE);
	gEidos_OMP_threads_G_COUNT_OF_MUTS_OF_TYPE = std::min(gEidosMaxThreads, gEidos_OMP_threads_G_COUNT_OF_MUTS_OF_TYPE);
	gEidos_OMP_threads_INDS_W_PEDIGREE_IDS = std::min(gEidosMaxThreads, gEidos_OMP_threads_INDS_W_PEDIGREE_IDS);
	gEidos_OMP_threads_RELATEDNESS = std::min(gEidosMaxThreads, gEidos_OMP_threads_RELATEDNESS);
	gEidos_OMP_threads_SAMPLE_INDIVIDUALS_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_SAMPLE_INDIVIDUALS_1);
	gEidos_OMP_threads_SAMPLE_INDIVIDUALS_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_SAMPLE_INDIVIDUALS_2);
	gEidos_OMP_threads_SET_FITNESS_SCALE_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_SET_FITNESS_SCALE_1);
	gEidos_OMP_threads_SET_FITNESS_SCALE_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_SET_FITNESS_SCALE_2);
	gEidos_OMP_threads_SUM_OF_MUTS_OF_TYPE = std::min(gEidosMaxThreads, gEidos_OMP_threads_SUM_OF_MUTS_OF_TYPE);

	gEidos_OMP_threads_DNORM_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_DNORM_1);
	gEidos_OMP_threads_DNORM_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_DNORM_2);
	gEidos_OMP_threads_RBINOM_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RBINOM_1);
	gEidos_OMP_threads_RBINOM_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RBINOM_2);
	gEidos_OMP_threads_RBINOM_3 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RBINOM_3);
	gEidos_OMP_threads_RDUNIF_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RDUNIF_1);
	gEidos_OMP_threads_RDUNIF_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RDUNIF_2);
	gEidos_OMP_threads_RDUNIF_3 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RDUNIF_3);
	gEidos_OMP_threads_REXP_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_REXP_1);
	gEidos_OMP_threads_REXP_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_REXP_2);
	gEidos_OMP_threads_RNORM_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RNORM_1);
	gEidos_OMP_threads_RNORM_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RNORM_2);
	gEidos_OMP_threads_RNORM_3 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RNORM_3);
	gEidos_OMP_threads_RPOIS_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RPOIS_1);
	gEidos_OMP_threads_RPOIS_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RPOIS_2);
	gEidos_OMP_threads_RUNIF_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RUNIF_1);
	gEidos_OMP_threads_RUNIF_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RUNIF_2);
	gEidos_OMP_threads_RUNIF_3 = std::min(gEidosMaxThreads, gEidos_OMP_threads_RUNIF_3);

	gEidos_OMP_threads_SORT_INT = std::min(gEidosMaxThreads, gEidos_OMP_threads_SORT_INT);
	gEidos_OMP_threads_SORT_FLOAT = std::min(gEidosMaxThreads, gEidos_OMP_threads_SORT_FLOAT);
	gEidos_OMP_threads_SORT_STRING = std::min(gEidosMaxThreads, gEidos_OMP_threads_SORT_STRING);
	
	gEidos_OMP_threads_POINT_IN_BOUNDS_1D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_IN_BOUNDS_1D);
	gEidos_OMP_threads_POINT_IN_BOUNDS_2D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_IN_BOUNDS_2D);
	gEidos_OMP_threads_POINT_IN_BOUNDS_3D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_IN_BOUNDS_3D);
	gEidos_OMP_threads_POINT_PERIODIC_1D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_PERIODIC_1D);
	gEidos_OMP_threads_POINT_PERIODIC_2D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_PERIODIC_2D);
	gEidos_OMP_threads_POINT_PERIODIC_3D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_PERIODIC_3D);
	gEidos_OMP_threads_POINT_REFLECTED_1D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_REFLECTED_1D);
	gEidos_OMP_threads_POINT_REFLECTED_2D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_REFLECTED_2D);
	gEidos_OMP_threads_POINT_REFLECTED_3D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_REFLECTED_3D);
	gEidos_OMP_threads_POINT_STOPPED_1D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_STOPPED_1D);
	gEidos_OMP_threads_POINT_STOPPED_2D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_STOPPED_2D);
	gEidos_OMP_threads_POINT_STOPPED_3D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_STOPPED_3D);
	gEidos_OMP_threads_POINT_UNIFORM_1D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_UNIFORM_1D);
	gEidos_OMP_threads_POINT_UNIFORM_2D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_UNIFORM_2D);
	gEidos_OMP_threads_POINT_UNIFORM_3D = std::min(gEidosMaxThreads, gEidos_OMP_threads_POINT_UNIFORM_3D);
	gEidos_OMP_threads_SET_SPATIAL_POS_1_1D = std::min(gEidosMaxThreads, gEidos_OMP_threads_SET_SPATIAL_POS_1_1D);
	gEidos_OMP_threads_SET_SPATIAL_POS_1_2D = std::min(gEidosMaxThreads, gEidos_OMP_threads_SET_SPATIAL_POS_1_2D);
	gEidos_OMP_threads_SET_SPATIAL_POS_1_3D = std::min(gEidosMaxThreads, gEidos_OMP_threads_SET_SPATIAL_POS_1_3D);
	gEidos_OMP_threads_SET_SPATIAL_POS_2_1D = std::min(gEidosMaxThreads, gEidos_OMP_threads_SET_SPATIAL_POS_2_1D);
	gEidos_OMP_threads_SET_SPATIAL_POS_2_2D = std::min(gEidosMaxThreads, gEidos_OMP_threads_SET_SPATIAL_POS_2_2D);
	gEidos_OMP_threads_SET_SPATIAL_POS_2_3D = std::min(gEidosMaxThreads, gEidos_OMP_threads_SET_SPATIAL_POS_2_3D);
	gEidos_OMP_threads_SPATIAL_MAP_VALUE = std::min(gEidosMaxThreads, gEidos_OMP_threads_SPATIAL_MAP_VALUE);

	gEidos_OMP_threads_CLIPPEDINTEGRAL_1S = std::min(gEidosMaxThreads, gEidos_OMP_threads_CLIPPEDINTEGRAL_1S);
	gEidos_OMP_threads_CLIPPEDINTEGRAL_2S = std::min(gEidosMaxThreads, gEidos_OMP_threads_CLIPPEDINTEGRAL_2S);
	//gEidos_OMP_threads_CLIPPEDINTEGRAL_3S = std::min(gEidosMaxThreads, gEidos_OMP_threads_CLIPPEDINTEGRAL_3S);
	gEidos_OMP_threads_DRAWBYSTRENGTH = std::min(gEidosMaxThreads, gEidos_OMP_threads_DRAWBYSTRENGTH);
	gEidos_OMP_threads_INTNEIGHCOUNT = std::min(gEidosMaxThreads, gEidos_OMP_threads_INTNEIGHCOUNT);
	gEidos_OMP_threads_LOCALPOPDENSITY = std::min(gEidosMaxThreads, gEidos_OMP_threads_LOCALPOPDENSITY);
	gEidos_OMP_threads_NEARESTINTNEIGH = std::min(gEidosMaxThreads, gEidos_OMP_threads_NEARESTINTNEIGH);
	gEidos_OMP_threads_NEARESTNEIGH = std::min(gEidosMaxThreads, gEidos_OMP_threads_NEARESTNEIGH);
	gEidos_OMP_threads_NEIGHCOUNT = std::min(gEidosMaxThreads, gEidos_OMP_threads_NEIGHCOUNT);
	gEidos_OMP_threads_TOTNEIGHSTRENGTH = std::min(gEidosMaxThreads, gEidos_OMP_threads_TOTNEIGHSTRENGTH);

	gEidos_OMP_threads_AGE_INCR = std::min(gEidosMaxThreads, gEidos_OMP_threads_AGE_INCR);
	gEidos_OMP_threads_DEFERRED_REPRO = std::min(gEidosMaxThreads, gEidos_OMP_threads_DEFERRED_REPRO);
	gEidos_OMP_threads_WF_REPRO = std::min(gEidosMaxThreads, gEidos_OMP_threads_WF_REPRO);
	gEidos_OMP_threads_FITNESS_ASEX_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_FITNESS_ASEX_1);
	gEidos_OMP_threads_FITNESS_ASEX_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_FITNESS_ASEX_2);
	gEidos_OMP_threads_FITNESS_ASEX_3 = std::min(gEidosMaxThreads, gEidos_OMP_threads_FITNESS_ASEX_3);
	gEidos_OMP_threads_FITNESS_SEX_1 = std::min(gEidosMaxThreads, gEidos_OMP_threads_FITNESS_SEX_1);
	gEidos_OMP_threads_FITNESS_SEX_2 = std::min(gEidosMaxThreads, gEidos_OMP_threads_FITNESS_SEX_2);
	gEidos_OMP_threads_FITNESS_SEX_3 = std::min(gEidosMaxThreads, gEidos_OMP_threads_FITNESS_SEX_3);
	gEidos_OMP_threads_MIGRANT_CLEAR = std::min(gEidosMaxThreads, gEidos_OMP_threads_MIGRANT_CLEAR);
	gEidos_OMP_threads_SIMPLIFY_SORT_PRE = std::min(gEidosMaxThreads, gEidos_OMP_threads_SIMPLIFY_SORT_PRE);
	gEidos_OMP_threads_SIMPLIFY_SORT = std::min(gEidosMaxThreads, gEidos_OMP_threads_SIMPLIFY_SORT);
	gEidos_OMP_threads_SIMPLIFY_SORT_POST = std::min(gEidosMaxThreads, gEidos_OMP_threads_SIMPLIFY_SORT_POST);
	gEidos_OMP_threads_PARENTS_CLEAR = std::min(gEidosMaxThreads, gEidos_OMP_threads_PARENTS_CLEAR);
	gEidos_OMP_threads_UNIQUE_MUTRUNS = std::min(gEidosMaxThreads, gEidos_OMP_threads_UNIQUE_MUTRUNS);
	gEidos_OMP_threads_SURVIVAL = std::min(gEidosMaxThreads, gEidos_OMP_threads_SURVIVAL);
}

void Eidos_WarmUpOpenMP(std::ostream *outstream, bool changed_max_thread_count, int new_max_thread_count, bool active_threads, std::string thread_count_set_name)
{
	// When running under OpenMP, print a log, and also set values for the OpenMP ICV's that we want to guarantee
	// See http://www.archer.ac.uk/training/course-material/2018/09/openmp-imp/Slides/L10-TipsTricksGotchas.pdf
	// We set these with overwrite=0 so the user can override them with custom values from the environment
	// FIXME: This should all be documented somewhere...
	
	// "active" encourages idle threads to spin rather than sleep; "active" seems to be much faster, maybe lower lag?
	// In SLiMgui and EidosScribe, we don't want to use "active", though, as it will pin the CPU usage even when not running a parallel section.
	const char *wait_policy = active_threads ? "ACTIVE" : "PASSIVE";
	setenv("OMP_WAIT_POLICY", wait_policy, 0);
	
	// "true" prevents threads migrating between cores; this generally improves performance, especially with per-thread memory usage
	const char *bind_policy = "true";
	setenv("OMP_PROC_BIND", bind_policy, 0);
	
	// We do not support dynamic adjustment of the number of threads; if we ask for N threads, we expect N threads
	// It is important not to change that, or a variety of things will no longer work correctly
	omp_set_dynamic(false);
	
	// We do not support nested parallelism; we set the relevant ICVs here to make sure it is off, overriding defaults/environment
	omp_set_max_active_levels(1);
	//omp_set_nested(false);		// deprecated in favor of omp_set_max_active_levels()
	
	// Set the maximum number of threads to the user's request, but never higher than the intrinsic max thread count
	if (changed_max_thread_count)
	{
		int thread_limit = omp_get_thread_limit();
		
		if (new_max_thread_count > thread_limit)
			new_max_thread_count = thread_limit;
		
		omp_set_num_threads(new_max_thread_count);		// confusingly, sets the *max* threads as returned by omp_get_max_threads()
	}
	
	// Get the maximum number of threads in effect, which might be different from the number requested
	gEidosMaxThreads = omp_get_max_threads();
	gEidosNumThreads = gEidosMaxThreads;
	gEidosNumThreadsOverride = false;
	
	// Set up per-task thread counts according to thread_count_set_name.  If it is empty, we choose a
	// default set heuristically, based upon the hardware platform.  Otherwise, we look for a name we
	// recognize, or error out.  There are very few sets here now, so this is not terribly useful;
	// but it does allow the benchmarking suite to turn off per-task limits with "maxThreads".
	if (thread_count_set_name.length() == 0)
		_Eidos_ChooseDefaultOpenMPThreadCounts();
	else if (thread_count_set_name == "maxThreads")
		_Eidos_SetOpenMPThreadCounts(EidosPerTaskThreadCounts::kMaxThreads);
	else if (thread_count_set_name == "MacStudio2022_16")
		_Eidos_SetOpenMPThreadCounts(EidosPerTaskThreadCounts::kMacStudio2022_16);
	else if (thread_count_set_name == "XeonGold2_40")
		_Eidos_SetOpenMPThreadCounts(EidosPerTaskThreadCounts::kXeonGold2_40);
	else
		EIDOS_TERMINATION << "ERROR (Eidos_WarmUpOpenMP): (internal error) unrecognized EidosPerTaskThreadCounts value." << EidosTerminate(nullptr);
	
	// Write some diagnostic output about our configuration.  If the verbosity level is 0, outstream will be nullptr.
	if (outstream)
	{
		(*outstream) << "// ********** Running multithreaded with OpenMP (maxThreads == " << gEidosMaxThreads << ")" << std::endl;
		(*outstream) << "// ********** OMP_WAIT_POLICY == " << getenv("OMP_WAIT_POLICY") << ", OMP_PROC_BIND == " << getenv("OMP_PROC_BIND") << std::endl;
		
#if 1
		(*outstream) << "// ********** Per-task thread counts: '" << gEidosPerTaskThreadCountsSetName << "', max " << gEidosPerTaskOriginalMaxThreadCount;
		if (gEidosPerTaskClippedMaxThreadCount < gEidosPerTaskOriginalMaxThreadCount)
			(*outstream) << " (clipped to " << gEidosPerTaskClippedMaxThreadCount << ")";
		(*outstream) << std::endl;
#endif
		
#if 0
		// BCH 5/19/2023: #if 0 for now, because this gives an error on some platforms; we don't support offloading anyway.
		// Look for devices (GPUs, accelerators) that we are able to offload to.
		// Note that OpenMP offloading to the GPUs on Apple Silicon is not currently supported by any compiler.
		// Other devices may not be visible unless you build slim_multi with a special build of your compiler;
		// see https://stackoverflow.com/a/66337011/2752221 for some details.
		int num_devices = omp_get_num_devices();
		int default_device = omp_get_default_device();
		
		if (num_devices > 0)
		{
			(*outstream) << "// ********** OpenMP target device count (GPUs, accelerators): " << num_devices << std::endl;
			(*outstream) << "// ********** Default target device for OpenMP offloading: " << default_device << std::endl;
		}
#endif
	}
	
#ifdef EIDOS_GUI
	// The GUI apps don't work well multithreaded.  They have to allow threads to sleep (otherwise they peg the
	// CPU the whole time they're running), and that is so inefficient that it makes the apps actually run much
	// slower than if they were just single-threaded, as far as I can tell.  I think the threads fall asleep
	// whenever they get suspended at all, and then waking them up again is heavyweight.  So running them
	// multithreaded is really just for my own development/testing work; end users should not do so.
	if (outstream)
		(*outstream) << "// ********** RUNNING SLIMGUI / EIDOSSCRIBE WITH OPENMP IS NOT RECOMMENDED!" << std::endl;
#endif
	
	if (outstream)
		(*outstream) << std::endl;
}
#endif

void Eidos_WarmUp(void)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Eidos_WarmUp(): illegal when parallel");
	
	static bool been_here = false;
	
	if (!been_here)
	{
		been_here = true;
		
		// Initialize the random number generator with a random-ish seed.  This seed may be overridden by the Context downstream.
		Eidos_InitializeRNG();
		Eidos_SetRNGSeed(Eidos_GenerateRNGSeed());
		
		// Set up the vector of Eidos constant names
		gEidosConstantNames.emplace_back(gEidosStr_T);
		gEidosConstantNames.emplace_back(gEidosStr_F);
		gEidosConstantNames.emplace_back(gEidosStr_NULL);
		gEidosConstantNames.emplace_back(gEidosStr_PI);
		gEidosConstantNames.emplace_back(gEidosStr_E);
		gEidosConstantNames.emplace_back(gEidosStr_INF);
		gEidosConstantNames.emplace_back(gEidosStr_NAN);
		
		// Make the shared EidosValue pool
		size_t maxEidosValueSize = sizeof(EidosValue_NULL);
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Logical));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Logical_const));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_String));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_String_vector));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_String_singleton));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Int));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Int_vector));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Int_singleton));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Float));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Float_vector));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Float_singleton));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Object));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Object_vector));
		maxEidosValueSize = std::max(maxEidosValueSize, sizeof(EidosValue_Object_singleton));
		
//		std::cout << "sizeof(EidosValue) ==                  " << sizeof(EidosValue) << std::endl;
//		std::cout << "sizeof(EidosValue_NULL) ==             " << sizeof(EidosValue_NULL) << std::endl;
//		std::cout << "sizeof(EidosValue_Logical) ==          " << sizeof(EidosValue_Logical) << std::endl;
//		std::cout << "sizeof(EidosValue_Logical_const) ==    " << sizeof(EidosValue_Logical_const) << std::endl;
//		std::cout << "sizeof(EidosValue_String) ==           " << sizeof(EidosValue_String) << std::endl;
//		std::cout << "sizeof(EidosValue_String_vector) ==    " << sizeof(EidosValue_String_vector) << std::endl;
//		std::cout << "sizeof(EidosValue_String_singleton) == " << sizeof(EidosValue_String_singleton) << std::endl;
//		std::cout << "sizeof(EidosValue_Int) ==              " << sizeof(EidosValue_Int) << std::endl;
//		std::cout << "sizeof(EidosValue_Int_vector) ==       " << sizeof(EidosValue_Int_vector) << std::endl;
//		std::cout << "sizeof(EidosValue_Int_singleton) ==    " << sizeof(EidosValue_Int_singleton) << std::endl;
//		std::cout << "sizeof(EidosValue_Float) ==            " << sizeof(EidosValue_Float) << std::endl;
//		std::cout << "sizeof(EidosValue_Float_vector) ==     " << sizeof(EidosValue_Float_vector) << std::endl;
//		std::cout << "sizeof(EidosValue_Float_singleton) ==  " << sizeof(EidosValue_Float_singleton) << std::endl;
//		std::cout << "sizeof(EidosValue_Object) ==           " << sizeof(EidosValue_Object) << std::endl;
//		std::cout << "sizeof(EidosValue_Object_vector) ==    " << sizeof(EidosValue_Object_vector) << std::endl;
//		std::cout << "sizeof(EidosValue_Object_singleton) == " << sizeof(EidosValue_Object_singleton) << std::endl;
//		std::cout << "maxEidosValueSize ==                   " << maxEidosValueSize << std::endl;
		
		gEidosValuePool = new EidosObjectPool("EidosObjectPool(EidosValue)", maxEidosValueSize);
		
		// Make the shared EidosASTNode pool
		gEidosASTNodePool = new EidosObjectPool("EidosObjectPool(EidosASTNode)", sizeof(EidosASTNode));
		
		// Allocate global permanents
		gStaticEidosValueVOID = EidosValue_VOID::Static_EidosValue_VOID();
		
		gStaticEidosValueNULL = EidosValue_NULL::Static_EidosValue_NULL();
		gStaticEidosValueNULLInvisible = EidosValue_NULL::Static_EidosValue_NULL_Invisible();
		
		gStaticEidosValue_Logical_ZeroVec = EidosValue_Logical_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Logical());
		gStaticEidosValue_Integer_ZeroVec = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector());
		gStaticEidosValue_Float_ZeroVec = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector());
		gStaticEidosValue_String_ZeroVec = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_vector());
		
		gStaticEidosValue_LogicalT = EidosValue_Logical_const::Static_EidosValue_Logical_T();
		gStaticEidosValue_LogicalF = EidosValue_Logical_const::Static_EidosValue_Logical_F();
		
		gStaticEidosValue_Integer0 = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(0));
		gStaticEidosValue_Integer1 = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(1));
		gStaticEidosValue_Integer2 = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(2));
		gStaticEidosValue_Integer3 = EidosValue_Int_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(3));
		
		gStaticEidosValue_Float0 = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0.0));
		gStaticEidosValue_Float0Point5 = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(0.5));
		gStaticEidosValue_Float1 = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(1.0));
		gStaticEidosValue_Float10 = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(10.0));
		gStaticEidosValue_FloatINF = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(std::numeric_limits<double>::infinity()));
		gStaticEidosValue_FloatNAN = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(std::numeric_limits<double>::quiet_NaN()));
		gStaticEidosValue_FloatE = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(M_E));
		gStaticEidosValue_FloatPI = EidosValue_Float_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(M_PI));
		
		gStaticEidosValue_StringEmpty = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(""));
		gStaticEidosValue_StringSpace = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(" "));
		gStaticEidosValue_StringAsterisk = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("*"));
		gStaticEidosValue_StringDoubleAsterisk = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("**"));
		gStaticEidosValue_StringComma = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(","));
		gStaticEidosValue_StringPeriod = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("."));
		gStaticEidosValue_StringDoubleQuote = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("\""));
		gStaticEidosValue_String_ECMAScript = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("ECMAScript"));
		gStaticEidosValue_String_indices = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("indices"));
		gStaticEidosValue_String_average = EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("average"));
		
		// Create the global class objects for all Eidos classes, from superclass to subclass
		// This breaks encapsulation, kind of, but it needs to be done here, in order, so that superclass objects exist,
		// and so that the global string names for the classes have already been set up by C++'s static initialization
		gEidosObject_Class =				new EidosClass(							gEidosStr_Object,			nullptr);
		gEidosDictionaryUnretained_Class =	new EidosDictionaryUnretained_Class(	gEidosStr_DictionaryBase,	gEidosObject_Class);
		gEidosDictionaryRetained_Class =	new EidosDictionaryRetained_Class(		gEidosStr_Dictionary,		gEidosDictionaryUnretained_Class);
		gEidosDataFrame_Class =				new EidosDataFrame_Class(				gEidosStr_DataFrame,		gEidosDictionaryRetained_Class);
		gEidosImage_Class =					new EidosImage_Class(					gEidosStr_Image,			gEidosDictionaryRetained_Class);
		gEidosTestElement_Class =			new EidosTestElement_Class(				gEidosStr__TestElement,		gEidosDictionaryRetained_Class);
		gEidosTestElementNRR_Class =		new EidosTestElementNRR_Class(			gEidosStr__TestElementNRR,	gEidosObject_Class);
		
		// This has to be allocated after gEidosObject_Class has been initialized above; the other global permanents must be initialized
		// before that point, however, since properties and method signatures may use some of those global permanent values
		gStaticEidosValue_Object_ZeroVec = EidosValue_Object_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gEidosObject_Class));
		
		// Set up the built-in function map, which is immutable
		EidosInterpreter::CacheBuiltInFunctionMap();
		
		// Set up the symbol table for Eidos constants
		gEidosConstantsSymbolTable = new EidosSymbolTable(EidosSymbolTableType::kEidosIntrinsicConstantsTable, nullptr);
		
		// Tell all registered classes to initialize their dispatch tables; doing this here saves a flag check later
		// Note that this can't be done in the EidosClass constructor because the vtable is not set up for the subclass yet
		for (EidosClass *eidos_class : EidosClass::RegisteredClasses(true, true))
			eidos_class->CacheDispatchTables();
		
		// Check classes for mismatched duplicate interfaces
		EidosClass::CheckForDuplicateMethodsOrProperties();
		
		// Check that class names are pointers to the original global strings, which is required
		if (&gEidosImage_Class->ClassName() != &gEidosStr_Image)
		{
			std::cerr << "***** Class name mismatch in Eidos_WarmUp()!";
			exit(EXIT_FAILURE);
		}
		
		// Check that EidosDictionaryState_StringKeys and EidosDictionaryState_IntegerKeys have matching layouts
		// as far as keys_are_integers_ is concerned, so that that flag can distinguish between them
		// BCH 3/27/2023: we have to actually allocate objects here to avoid getting flagged by UBSan...
		{
			EidosDictionaryState_StringKeys *dict_state_ptr_string = new EidosDictionaryState_StringKeys;
			EidosDictionaryState_IntegerKeys *dict_state_ptr_integer = new EidosDictionaryState_IntegerKeys;
			
			uint8_t *flag_addr_string_keys = &((dict_state_ptr_string)->keys_are_integers_);
			uint8_t *flag_addr_integer_keys = &((dict_state_ptr_integer)->keys_are_integers_);
			uint8_t *flag_addr_string_contains = &((dict_state_ptr_string)->contains_non_retain_release_objects_);
			uint8_t *flag_addr_integer_contains = &((dict_state_ptr_integer)->contains_non_retain_release_objects_);
			
			size_t string_keys_offset = flag_addr_string_keys - (uint8_t *)dict_state_ptr_string;
			size_t integer_keys_offset = flag_addr_integer_keys - (uint8_t *)dict_state_ptr_integer;
			size_t string_contains_offset = flag_addr_string_contains - (uint8_t *)dict_state_ptr_string;
			size_t integer_contains_offset = flag_addr_integer_contains - (uint8_t *)dict_state_ptr_integer;
			
			if ((string_keys_offset != integer_keys_offset) || (string_contains_offset != integer_contains_offset))
			{
				std::cerr << "***** EidosDictionaryState layout mismatch in Eidos_WarmUp()!";
				exit(EXIT_FAILURE);
			}
			
			delete dict_state_ptr_string;
			delete dict_state_ptr_integer;
		}
		
#if (defined(_MSC_VER) && _MSC_VER <= 1900) || (defined(__MINGW32__) && !defined(_UCRT))
		// Work around non-conformance of Microsoft's printf %e format specifier,
		// which uses 3 digits for the exponent instead of 2.
		// Until Visual Studio 2015, the _set_output_format() function can be used to obtain standards compliant behaviour.
		// More recent Visual Studio compilers are standards compliant, and Mingw provides _set_output_format() since 2008.
		_set_output_format(_TWO_DIGIT_EXPONENT);
#endif
	}
}

bool Eidos_GoodSymbolForDefine(std::string &p_symbol_name)
{
	bool good_symbol = true;
	
	// Eidos constants are reserved
	if (std::find(gEidosConstantNames.begin(), gEidosConstantNames.end(), p_symbol_name) != gEidosConstantNames.end())
		good_symbol = false;
	
	// Eidos keywords are reserved (probably won't reach here anyway)
	if ((p_symbol_name == "if") || (p_symbol_name == "else") || (p_symbol_name == "do") || (p_symbol_name == "while") || (p_symbol_name == "for") || (p_symbol_name == "in") || (p_symbol_name == "next") || (p_symbol_name == "break") || (p_symbol_name == "return") || (p_symbol_name == "function"))
		good_symbol = false;
	
	// SLiM constants are reserved too; this code belongs in SLiM, but only
	// SLiM uses this facility right now anyway, so I'm not going to sweat it...
	if ((p_symbol_name == "community") || (p_symbol_name == "sim") || (p_symbol_name == "slimgui"))
		good_symbol = false;
	
	int len = (int)p_symbol_name.length();
	
	if (len >= 2)
	{
		char first_ch = p_symbol_name[0];
		
		if ((first_ch == 'p') || (first_ch == 'g') || (first_ch == 'm') || (first_ch == 's') || (first_ch == 'i'))
		{
			int ch_index;
			
			for (ch_index = 1; ch_index < len; ++ch_index)
			{
				char idx_ch = p_symbol_name[ch_index];
				
				if ((idx_ch < '0') || (idx_ch > '9'))
					break;
			}
			
			if (ch_index == len)
				good_symbol = false;
		}
	}
	
	return good_symbol;
}

EidosValue_SP Eidos_ValueForCommandLineExpression(std::string &p_value_expression)
{
	EidosValue_SP value;
	EidosScript script(p_value_expression, -1);
	
	// Note this can raise; the caller should be prepared for that
	script.SetFinalSemicolonOptional(true);
	script.Tokenize();
	script.ParseInterpreterBlockToAST(false);
	
	EidosSymbolTable symbol_table(EidosSymbolTableType::kLocalVariablesTable, gEidosConstantsSymbolTable);
	EidosFunctionMap function_map(*EidosInterpreter::BuiltInFunctionMap());
	EidosInterpreter interpreter(script, symbol_table, function_map, nullptr, std::cout, std::cerr);	// we're at the command line, so we assume we're using stdout/stderr
	
	value = interpreter.EvaluateInterpreterBlock(false, true);	// do not print output, return the last statement value
	
	return value;
}

void Eidos_DefineConstantsFromCommandLine(const std::vector<std::string> &p_constants)
{
	// We want to throw exceptions, even in SLiM, so that we can catch them here
	bool save_throws = gEidosTerminateThrows;
	
	gEidosTerminateThrows = true;
	
	for (const std::string &constant : p_constants)
	{
		// Each constant must be in the form x=y, where x is a valid identifier and y is a valid Eidos expression.
		// We parse the assignment using EidosScript, and work with the resulting AST, for generality.
		EidosScript script(constant, -1);
		bool malformed = false;
		
		try
		{
			script.SetFinalSemicolonOptional(true);
			script.Tokenize();
			script.ParseInterpreterBlockToAST(false);
		}
		catch (...)
		{
			malformed = true;
		}
		
		if (!malformed)
		{
			//script.PrintTokens(std::cout);
			//script.PrintAST(std::cout);
			
			const EidosASTNode *AST = script.AST();
			
			if (AST && (AST->token_->token_type_ == EidosTokenType::kTokenInterpreterBlock) && (AST->children_.size() == 1))
			{
				const EidosASTNode *top_node = AST->children_[0];
				
				if (top_node && (top_node->token_->token_type_ == EidosTokenType::kTokenAssign) && (top_node->children_.size() == 2))
				{
					const EidosASTNode *left_node = top_node->children_[0];
					
					if (left_node && (left_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (left_node->children_.size() == 0))
					{
						std::string symbol_name = left_node->token_->token_string_;
						
						// OK, if the symbol name is acceptable, keep digging
						if (Eidos_GoodSymbolForDefine(symbol_name))
						{
							const EidosASTNode *right_node = top_node->children_[1];
							
							if (right_node)
							{
								// Rather than try to make a new script with right_node as its root, we simply take the substring
								// to the right of the = operator and make a new script object from that, and evaluate that.
								// Note that the expression also parsed in the context of "value = <expr>", so this limits the
								// syntax allowed; the value cannot be a compound statement, for example.
								int32_t assign_end = top_node->token_->token_end_;
								std::string value_expression = constant.substr(assign_end + 1);
								EidosValue_SP x_value_sp;
								
								try {
									x_value_sp = Eidos_ValueForCommandLineExpression(value_expression);
								} catch (...) {
									// Syntactic errors should have already been caught, but semantic errors can raise here, and we re-raise
									// with a generic "could not be evaluated" message to lead the user toward the commend-line def as the problem
									gEidosTerminateThrows = save_throws;
									std::string terminationMessage = gEidosTermination.str();

									EIDOS_TERMINATION << "ERROR (Eidos_DefineConstantsFromCommandLine): command-line expression could not be evaluated: " << constant << std::endl;
									EIDOS_TERMINATION << "original error: " << terminationMessage;
									EIDOS_TERMINATION << EidosTerminate(nullptr);
								}
								
								if (x_value_sp)
								{
									//std::cout << "define " << symbol_name << " = " << value_expression << std::endl;
									
									// Permanently alter the global Eidos symbol table; don't do this at home!
									EidosGlobalStringID symbol_id = EidosStringRegistry::GlobalStringIDForString(symbol_name);
									EidosSymbolTableEntry table_entry(symbol_id, x_value_sp);
									
									gEidosConstantsSymbolTable->InitializeConstantSymbolEntry(table_entry);
									
									continue;
								}
							}
						}
						else
						{
							gEidosTerminateThrows = save_throws;
							
							EIDOS_TERMINATION << "ERROR (Eidos_DefineConstantsFromCommandLine): illegal defined constant name '" << symbol_name << "'." << EidosTerminate(nullptr);
						}
					}
				}
			}
		}
		
		gEidosTerminateThrows = save_throws;
		
		// Terminate without putting out a script line/character diagnostic; that looks weird
		EIDOS_TERMINATION << "ERROR (Eidos_DefineConstantsFromCommandLine): malformed command-line constant definition: " << constant;
		
		if (gEidosTerminateThrows)
		{
			EIDOS_TERMINATION << EidosTerminate(nullptr);
		}
		else
		{
			// This is from operator<<(std::ostream& p_out, const EidosTerminate &p_terminator)
			EIDOS_TERMINATION << std::endl;
			EIDOS_TERMINATION.flush();
			exit(EXIT_FAILURE);
		}
	}
	
	gEidosTerminateThrows = save_throws;
}


// Information on the Context within which Eidos is running (if any).
double gEidosContextVersion = 0.0;
std::string gEidosContextVersionString;
std::string gEidosContextLicense;
std::string gEidosContextCitation;


#pragma mark -
#pragma mark Termination handling
#pragma mark -

// the part of the input file that caused an error; used to highlight the token or text that caused the error
EidosErrorContext gEidosErrorContext = {{-1, -1, -1, -1}, nullptr, false};

int gEidosErrorLine = -1, gEidosErrorLineCharacter = -1;

// Warnings
bool gEidosSuppressWarnings = false;


// define string stream used for output when gEidosTerminateThrows == 1; otherwise, terminates call exit()
bool gEidosTerminateThrows = true;
std::ostringstream gEidosTermination;
bool gEidosTerminated;


// Print a demangled stack backtrace of the caller function to FILE* out.
// Note that in Cocoa this works better: NSLog(@"%@", NSThread.callStackSymbols);
// For a shortened backtrace: NSLog(@"%@", [NSThread.callStackSymbols subarrayWithRange:NSMakeRange(0, MIN(5UL, NSThread.callStackSymbols.count))]);
void Eidos_PrintStacktrace(FILE *p_out, unsigned int p_max_frames)
{
	fprintf(p_out, "stack trace:\n");
	
	// storage array for stack trace address data
	void* addrlist[p_max_frames+1];
	
	// retrieve current stack addresses
	int addrlen = backtrace(addrlist, static_cast<int>(sizeof(addrlist) / sizeof(void*)));
	
	if (addrlen == 0)
	{
		fprintf(p_out, "  <empty, possibly corrupt>\n");
		return;
	}
	
	// resolve addresses into strings containing "filename(function+address)",
	// this array must be free()-ed
	char** symbollist = backtrace_symbols(addrlist, addrlen);
	
	// allocate string which will be filled with the demangled function name
	size_t funcnamesize = 256;
	char* funcname = (char*)malloc(funcnamesize);
	
	// iterate over the returned symbol lines. skip the first, it is the
	// address of this function.
	for (int i = 1; i < addrlen; i++)
	{
		char *begin_name = 0, *end_name = 0, *begin_offset = 0, *end_offset = 0;
		
		// find parentheses and +address offset surrounding the mangled name:
		// ./module(function+0x15c) [0x8048a6d]
		for (char *p = symbollist[i]; *p; ++p)
		{
			if (*p == '(')
				begin_name = p;
			else if (*p == '+')
				begin_offset = p;
			else if (*p == ')' && begin_offset)
			{
				end_offset = p;
				break;
			}
		}
		
		// BCH 24 Dec 2014: backtrace_symbols() on OS X seems to return strings in a different, non-standard format.
		// Added this code in an attempt to parse that format.  No doubt it could be done more cleanly.  :->
		if (!(begin_name && begin_offset && end_offset
			  && begin_name < begin_offset))
		{
			enum class ParseState {
				kInWhitespace1 = 1,
				kInLineNumber,
				kInWhitespace2,
				kInPackageName,
				kInWhitespace3,
				kInAddress,
				kInWhitespace4,
				kInFunction,
				kInWhitespace5,
				kInPlus,
				kInWhitespace6,
				kInOffset,
				kInOverrun
			};
			ParseState parse_state = ParseState::kInWhitespace1;
			char *p;
			
			for (p = symbollist[i]; *p; ++p)
			{
				switch (parse_state)
				{
					case ParseState::kInWhitespace1:	if (!isspace(*p)) parse_state = ParseState::kInLineNumber;	break;
					case ParseState::kInLineNumber:		if (isspace(*p)) parse_state = ParseState::kInWhitespace2;	break;
					case ParseState::kInWhitespace2:	if (!isspace(*p)) parse_state = ParseState::kInPackageName;	break;
					case ParseState::kInPackageName:	if (isspace(*p)) parse_state = ParseState::kInWhitespace3;	break;
					case ParseState::kInWhitespace3:	if (!isspace(*p)) parse_state = ParseState::kInAddress;		break;
					case ParseState::kInAddress:		if (isspace(*p)) parse_state = ParseState::kInWhitespace4;	break;
					case ParseState::kInWhitespace4:
						if (!isspace(*p))
						{
							parse_state = ParseState::kInFunction;
							begin_name = p - 1;
						}
						break;
					case ParseState::kInFunction:
						if (isspace(*p))
						{
							parse_state = ParseState::kInWhitespace5;
							end_name = p;
						}
						break;
					case ParseState::kInWhitespace5:	if (!isspace(*p)) parse_state = ParseState::kInPlus;		break;
					case ParseState::kInPlus:			if (isspace(*p)) parse_state = ParseState::kInWhitespace6;	break;
					case ParseState::kInWhitespace6:
						if (!isspace(*p))
						{
							parse_state = ParseState::kInOffset;
							begin_offset = p - 1;
						}
						break;
					case ParseState::kInOffset:
						if (isspace(*p))
						{
							parse_state = ParseState::kInOverrun;
							end_offset = p;
						}
						break;
					case ParseState::kInOverrun:
						break;
				}
			}
			
			if (parse_state == ParseState::kInOffset && !end_offset)
				end_offset = p;
		}
		
		if (begin_name && begin_offset && end_offset
			&& begin_name < begin_offset)
		{
			*begin_name++ = '\0';
			if (end_name)
				*end_name = '\0';
			*begin_offset++ = '\0';
			*end_offset = '\0';
			
			// mangled name is now in [begin_name, begin_offset) and caller
			// offset in [begin_offset, end_offset). now apply __cxa_demangle():
			
			int status;
			char *ret = abi::__cxa_demangle(begin_name, funcname, &funcnamesize, &status);
			
			if (status == 0)
			{
				funcname = ret; // use possibly realloc()-ed string; static analyzer doesn't like this but it is OK I think
				fprintf(p_out, "  %s : %s + %s\n", symbollist[i], funcname, begin_offset);
			}
			else
			{
				// demangling failed. Output function name as a C function with
				// no arguments.
				fprintf(p_out, "  %s : %s() + %s\n", symbollist[i], begin_name, begin_offset);
			}
		}
		else
		{
			// couldn't parse the line? print the whole line.
			fprintf(p_out, "URF:  %s\n", symbollist[i]);
		}
	}
	
	free(funcname);
	free(symbollist);
	
	fflush(p_out);
}

void Eidos_ScriptErrorPosition(const EidosErrorContext &p_error_context)
{
	EidosScript *currentScript = p_error_context.currentScript;
	int errorStart = p_error_context.errorPosition.characterStartOfError;
	int errorEnd = p_error_context.errorPosition.characterEndOfError;
	
	gEidosErrorLine = -1;
	gEidosErrorLineCharacter = -1;
	
	if (currentScript && (errorStart >= 0) && (errorEnd >= errorStart))
	{
		// figure out the script line and position
		const std::string &script_string = currentScript->String();
		int length = (int)script_string.length();
		
		if ((length >= errorStart) && (length >= errorEnd))	// == is the EOF position, which we want to allow but have to treat carefully
		{
			int lineStart = (errorStart < length) ? errorStart : length - 1;
			int lineEnd = (errorEnd < length) ? errorEnd : length - 1;
			int lineNumber;
			
			for (; lineStart > 0; --lineStart)
				if ((script_string[lineStart - 1] == '\n') || (script_string[lineStart - 1] == '\r'))
					break;
			for (; lineEnd < length - 1; ++lineEnd)
				if ((script_string[lineEnd + 1] == '\n') || (script_string[lineEnd + 1] == '\r'))
					break;
			
			// Figure out the line number in the script where the error starts
			lineNumber = 1;
			
			for (int i = 0; i < lineStart; ++i)
				if (script_string[i] == '\n')
					lineNumber++;
			
			gEidosErrorLine = lineNumber;
			gEidosErrorLineCharacter = errorStart - lineStart;
		}
	}
}

void Eidos_LogScriptError(std::ostream& p_out, const EidosErrorContext &p_error_context)
{
	EidosScript *currentScript = p_error_context.currentScript;
	int errorStart = p_error_context.errorPosition.characterStartOfError;
	int errorEnd = p_error_context.errorPosition.characterEndOfError;
	
	if (currentScript && (errorStart >= 0) && (errorEnd >= errorStart))
	{
		// figure out the script line, print it, show the error position
		const std::string &script_string = currentScript->String();
		int length = (int)script_string.length();
		
		if ((length >= errorStart) && (length >= errorEnd))	// == is the EOF position, which we want to allow but have to treat carefully
		{
			int lineStart = (errorStart < length) ? errorStart : length - 1;
			int lineEnd = (errorEnd < length) ? errorEnd : length - 1;
			int lineNumber;
			
			for (; lineStart > 0; --lineStart)
				if ((script_string[lineStart - 1] == '\n') || (script_string[lineStart - 1] == '\r'))
					break;
			for (; lineEnd < length - 1; ++lineEnd)
				if ((script_string[lineEnd + 1] == '\n') || (script_string[lineEnd + 1] == '\r'))
					break;
			
			// Figure out the line number in the script where the error starts
			lineNumber = 1;
			
			for (int i = 0; i < lineStart; ++i)
				if (script_string[i] == '\n')
					lineNumber++;
			
			gEidosErrorLine = lineNumber;
			gEidosErrorLineCharacter = errorStart - lineStart;
			
			p_out << std::endl << "Error on script line " << gEidosErrorLine << ", character " << gEidosErrorLineCharacter;
			
			if (p_error_context.executingRuntimeScript)
				p_out << " (inside runtime script block)";
			
			p_out << ":" << std::endl << std::endl;
			
			// Emit the script line, converting tabs to three spaces
			for (int i = lineStart; i <= lineEnd; ++i)
			{
				char script_char = script_string[i];
				
				if (script_char == '\t')
					p_out << "   ";
				else if ((script_char == '\n') || (script_char == '\r'))	// don't show more than one line
					break;
				else
					p_out << script_char;
			}
			
			p_out << std::endl;
			
			// Emit the error indicator line, again emitting three spaces where the script had a tab
			for (int i = lineStart; i < errorStart; ++i)
			{
				char script_char = script_string[i];
				
				if (script_char == '\t')
					p_out << "   ";
				else if ((script_char == '\n') || (script_char == '\r'))	// don't show more than one line
					break;
				else
					p_out << ' ';
			}
			
			// Emit the error indicator
			for (int i = 0; i < errorEnd - errorStart + 1; ++i)
				p_out << "^";
			
			p_out << std::endl;
		}
	}
}

EidosTerminate::EidosTerminate(const EidosToken *p_error_token)
{
	// This is the end of the line, so we don't need to treat the error position as a stack
	if (p_error_token)
		PushErrorPositionFromToken(p_error_token);
}

EidosTerminate::EidosTerminate(bool p_print_backtrace) : print_backtrace_(p_print_backtrace)
{
}

EidosTerminate::EidosTerminate(const EidosToken *p_error_token, bool p_print_backtrace) : print_backtrace_(p_print_backtrace)
{
	// This is the end of the line, so we don't need to treat the error position as a stack
	if (p_error_token)
		PushErrorPositionFromToken(p_error_token);
}

void operator<<(std::ostream& p_out, const EidosTerminate &p_terminator)
{
	p_out << std::endl;
	
	p_out.flush();
	
	if (p_terminator.print_backtrace_)
		Eidos_PrintStacktrace(stderr);
	
	if (gEidosTerminateThrows)
	{
		// BCH 5/14/2023: I used to have a check here for (omp_get_level() > 0), and would do raise(SIGTRAP) in that situation
		// to get a trap in the debugger for the point when an exception was raised inside a parallel region.  However, we now
		// have some places in the code where such raises are guarded by try/catch, so they are no longer unambiguously wrong.
		// So I've deleted that check here.  The throw below will happen, and if no try/catch is in place and we're inside a
		// parallel region, we will end up with an uncaught C++ exception error.
		
		// In this case, EidosTerminate() throws an exception that gets caught by the Context.  That invalidates the simulation object, and
		// causes the Context to display an error message and ends the simulation run, but it does not terminate the app.
		throw std::runtime_error("A runtime error occurred in Eidos");
	}
	else
	{
		// In this case, EidosTerminate() does in fact terminate; this is appropriate when errors are simply fatal and there is no UI.
		// In this case, we want to emit a diagnostic showing the line of script where the error occurred, if we can.
		// This facility uses only the non-UTF16 positions, since it is based on std::string, so those positions can be ignored.
		Eidos_LogScriptError(p_out, gEidosErrorContext);
		
		// Try to flush any outstanding file buffers
		Eidos_FlushFiles();
		
		exit(EXIT_FAILURE);
	}
}

std::string Eidos_GetTrimmedRaiseMessage(void)
{
	if (gEidosTerminateThrows)
	{
		std::string terminationMessage = gEidosTermination.str();
		
		gEidosTermination.clear();
		gEidosTermination.str(gEidosStr_empty_string);
		
		// trim off newlines at the end of the raise string
		size_t endpos = terminationMessage.find_last_not_of("\n\r");
		if (std::string::npos != endpos)
			terminationMessage = terminationMessage.substr(0, endpos + 1);
		
		return terminationMessage;
	}
	else
	{
		return gEidosStr_empty_string;
	}
}

std::string Eidos_GetUntrimmedRaiseMessage(void)
{
	if (gEidosTerminateThrows)
	{
		std::string terminationMessage = gEidosTermination.str();
		
		gEidosTermination.clear();
		gEidosTermination.str(gEidosStr_empty_string);
		
		return terminationMessage;
	}
	else
	{
		return gEidosStr_empty_string;
	}
}


#pragma mark -
#pragma mark Debugging support
#pragma mark -

void CheckLongTermBoundary()
{
	THREAD_SAFETY_IN_ANY_PARALLEL("CheckLongTermBoundary(): illegal when parallel");
	
	// Right now, EidosDictionary is the only part of Eidos that is smart about long-term
	// boundaries, so we just need to check its state.  But in future, we could allow the
	// user to call defineGlobal() with a non-retain-release object as long as they fix
	// the reference by the next long-term boundary.
	bool violation = false;
	
	if (gEidos_DictionaryNonRetainReleaseReferenceCounter != 0)
		violation = true;
	
	if (violation)
		EIDOS_TERMINATION << "ERROR (CheckLongTermBoundary): A long-term reference has been kept to an Eidos object that is not under retain-release memory management.  For example, a SLiM Individual or Subpopulation may have been placed in a global dictionary.  This is illegal; only objects that are under retain-release memory management can be kept long-term." << EidosTerminate(nullptr);
}


#pragma mark -
#pragma mark Memory usage monitoring
#pragma mark -

//
//	The code below was obtained from http://nadeausoftware.com/articles/2012/07/c_c_tip_how_get_process_resident_set_size_physical_memory_use.  It may or may not work.
//	On Windows, it requires linking with Microsoft's psapi.lib.  That is left as an exercise for the reader.  Nadeau says "On other OSes, the default libraries are sufficient."
//

/*
 * Author:  David Robert Nadeau
 * Site:    http://NadeauSoftware.com/
 * License: Creative Commons Attribution 3.0 Unported License
 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
 */

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#include <sys/resource.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/mach.h>

#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
#include <fcntl.h>
#include <procfs.h>

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
#include <stdio.h>

#endif

#else
#error "Cannot define getPeakRSS( ) or getCurrentRSS( ) for an unknown OS."
#endif

/**
 * Returns the peak (maximum so far) resident set size (physical
 * memory use) measured in bytes, or zero if the value cannot be
 * determined on this OS.
 */
size_t Eidos_GetPeakRSS(void)
{
#if defined(_WIN32)
	/* Windows -------------------------------------------------- */
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo( GetCurrentProcess( ), &info, sizeof(info) );
	return (size_t)info.PeakWorkingSetSize;
	
#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
	/* AIX and Solaris ------------------------------------------ */
	struct psinfo psinfo;
	int fd = -1;
	if ( (fd = open( "/proc/self/psinfo", O_RDONLY )) == -1 )
		return (size_t)0L;		/* Can't open? */
	if ( read( fd, &psinfo, sizeof(psinfo) ) != sizeof(psinfo) )
	{
		close( fd );
		return (size_t)0L;		/* Can't read? */
	}
	close( fd );
	return (size_t)(psinfo.pr_rssize * 1024L);
	
#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
	/* BSD, Linux, and OSX -------------------------------------- */
	struct rusage rusage;
	getrusage( RUSAGE_SELF, &rusage );
#if defined(__APPLE__) && defined(__MACH__)
	return (size_t)rusage.ru_maxrss;
#else
	return (size_t)(rusage.ru_maxrss * 1024L);
#endif
	
#else
	/* Unknown OS ----------------------------------------------- */
	return (size_t)0L;			/* Unsupported. */
#endif
}


/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
size_t Eidos_GetCurrentRSS(void)
{
#if defined(_WIN32)
	/* Windows -------------------------------------------------- */
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo( GetCurrentProcess( ), &info, sizeof(info) );
	return (size_t)info.WorkingSetSize;
	
#elif defined(__APPLE__) && defined(__MACH__)
	/* OSX ------------------------------------------------------ */
	struct mach_task_basic_info info;
	mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
	if ( task_info( mach_task_self( ), MACH_TASK_BASIC_INFO,
				   (task_info_t)&info, &infoCount ) != KERN_SUCCESS )
		return (size_t)0L;		/* Can't access? */
	return (size_t)info.resident_size;
	
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
	/* Linux ---------------------------------------------------- */
	long rss = 0L;
	FILE* fp = NULL;
	if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL )
		return (size_t)0L;		/* Can't open? */
	if ( fscanf( fp, "%*s%ld", &rss ) != 1 )
	{
		fclose( fp );
		return (size_t)0L;		/* Can't read? */
	}
	fclose( fp );
	return (size_t)rss * (size_t)sysconf( _SC_PAGESIZE);
	
#else
	/* AIX, BSD, Solaris, and Unknown OS ------------------------ */
	return (size_t)0L;			/* Unsupported. */
#endif
}

/**
 *This is my own code, patterned after Nadeau's code above
 *
 * Returns the current virtual memory use measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
size_t Eidos_GetVMUsage(void)
{
#if defined(_WIN32)
	/* Windows -------------------------------------------------- */
	// see https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo( GetCurrentProcess( ), &info, sizeof(info) );
	return (size_t)info.PagefileUsage;
	
#elif defined(__APPLE__) && defined(__MACH__)
	/* OSX ------------------------------------------------------ */
	struct mach_task_basic_info info;
	mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
	if ( task_info( mach_task_self( ), MACH_TASK_BASIC_INFO,
				   (task_info_t)&info, &infoCount ) != KERN_SUCCESS )
		return (size_t)0L;		/* Can't access? */
	return (size_t)info.virtual_size;
	
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
	/* Linux ---------------------------------------------------- */
	// see https://man7.org/linux/man-pages/man5/proc.5.html
	long vmsize = 0L;
	FILE* fp = NULL;
	if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL )
		return (size_t)0L;		/* Can't open? */
	if ( fscanf( fp, "%ld", &vmsize ) != 1 )
	{
		fclose( fp );
		return (size_t)0L;		/* Can't read? */
	}
	fclose( fp );
	return (size_t)vmsize * (size_t)sysconf( _SC_PAGESIZE);
	
#else
	/* AIX, BSD, Solaris, and Unknown OS ------------------------ */
	return (size_t)0L;			/* Unsupported. */
#endif
}

size_t Eidos_GetMaxRSS(void)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Eidos_GetMaxRSS(): usage of statics");
	
	static bool beenHere = false;
	static size_t max_rss = 0;
	
	if (!beenHere)
	{

#if defined(_WIN32)
	// Assume unlimited on Windows with warning
	std::cerr << "WARNING: Eidos_GetMaxRSS() does not work properly in Windows, so return assumes no limit, which may be incorrect.";
	max_rss = 0;

#else
#if 0
		// Find our RSS limit by launching a subshell to run "ulimit -m"
		std::string limit_string = Eidos_Exec("ulimit -m");
		
		std::string unlimited("unlimited");
		
		if (std::mismatch(unlimited.begin(), unlimited.end(), limit_string.begin()).first == unlimited.end())
		{
			// "unlimited" is a prefix of foobar, so use 0 to represent that
			max_rss = 0;
		}
		else
		{
			errno = 0;
			
			const char *c_str = limit_string.c_str();
			char *last_used_char = nullptr;
			
			max_rss = strtoll(c_str, &last_used_char, 10);
			
			if (errno || (last_used_char == c_str))
			{
				// If an error occurs, assume we are unlimited
				max_rss = 0;
			}
			else
			{
				// This value is in 1024-byte units, so multiply to get a limit in bytes
				max_rss *= 1024L;
			}
		}
#else
		// Find our RSS limit using getrlimit() – easier and safer
		struct rlimit rlim;
		
		if (getrlimit(RLIMIT_RSS, &rlim) == 0)
		{
			// This value is in bytes, no scaling needed
			max_rss = (uint64_t)rlim.rlim_max;
			
			// If the claim is that we have more than 1024 TB at our disposal, then we will consider ourselves unlimited :->
			if (max_rss > 1024LL * 1024L * 1024L * 1024L * 1024L)
				max_rss = 0;
		}
		else
		{
			// If an error occurs, assume we are unlimited
			max_rss = 0;
		}
#endif
#endif
		
		beenHere = true;
	}

	return max_rss;
}

void Eidos_CheckRSSAgainstMax(const std::string &p_message1, const std::string &p_message2)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Eidos_CheckRSSAgainstMax():  usage of statics");
	
	static bool beenHere = false;
	static size_t max_rss = 0;
	
	if (!beenHere)
	{
		// The first time we are called, we get the memory limit and sanity-check it
		max_rss = Eidos_GetMaxRSS();
		
#if 0
		// Impose a 20 MB limit, for testing
		max_rss = 20*1024*1024;
#warning Turn this off!
#endif
		
		if (max_rss != 0)
		{
			size_t current_rss = Eidos_GetCurrentRSS();
			
			// If we are already within 10 MB of overrunning our supposed limit, disable checking; assume that
			// either Eidos_GetMaxRSS() or Eidos_GetCurrentRSS() is not telling us the truth.
			if (current_rss + 10L*1024L*1024L > max_rss)
				max_rss = 0;
		}
		
		// Switch off our memory check flag if we are not going to enforce a limit anyway;
		// this allows the caller to skip calling us when possible, for speed
		if (max_rss == 0)
			eidos_do_memory_checks = false;
		
		beenHere = true;
	}
	
	if (eidos_do_memory_checks && (max_rss != 0))
	{
		size_t current_rss = Eidos_GetCurrentRSS();
		
		// If we are within 10 MB of overrunning our limit, then terminate with a message before
		// the system does it for us.  10 MB gives us a little headroom, so that we detect this
		// condition before the system does.
		if (current_rss + 10L*1024L*1024L > max_rss)
		{
			// We output our warning to std::cerr, because we may get killed by the OS for exceeding our memory limit before other streams would get flushed
			// Note this warning is not suppressed by gEidosSuppressWarnings; that is deliberate
			std::cerr << "WARNING (" << p_message1 << "): memory usage of " << (current_rss / (1024.0 * 1024.0)) << " MB is dangerously close to the limit of " << (max_rss / (1024.0 * 1024.0)) << " MB reported by the operating system.  This SLiM process may soon be killed by the operating system for exceeding the memory limit.  You might raise the per-process memory limit, or modify your model to decrease memory usage.  You can turn off this memory check with the '-x' command-line option.  " << p_message2 << std::endl;
			std::cerr.flush();
			
			// We want to issue only one warning, so turn off warnings now
			eidos_do_memory_checks = false;
		}
	}
}


#pragma mark -
#pragma mark File I/O
#pragma mark -

// resolve a leading ~ in a filesystem path to the user's home directory
std::string Eidos_ResolvedPath(const std::string &p_path)
{
	std::string path = p_path;
	
	// if there is a leading '~', replace it with the user's home directory; not sure if this works on Windows... It doesn't..
	#ifndef _WIN32
	if ((path.length() > 0) && (path[0] == '~'))
	{
		long bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
		
		if (bufsize == -1)
		{
			// non-reentrant code when we can't get a buffer size
			const char *homedir = getenv("HOME");
			
			if (homedir == NULL)
				homedir = getpwuid(getuid())->pw_dir;
			
			if (strlen(homedir))
				path.replace(0, 1, homedir);
		}
		else
		{
			// reentrant version using getpwuid_r
			char buffer[bufsize];
			struct passwd pwd, *result = NULL;
			int retval = getpwuid_r(getuid(), &pwd, buffer, bufsize, &result);
			
			if (retval || !result)
			{
				std::cerr << "Eidos_ResolvedPath(): Could not resolve ~ in path due to failure of getpwuid_r";
			}
			else
			{
				const char *homedir = pwd.pw_dir;
				
				if (strlen(homedir))
					path.replace(0, 1, homedir);
			}
		}
	}
	#else
	if ((path.length() > 0) && (path[0] == '~'))
	{
		EIDOS_TERMINATION << "ERROR (Eidos_ResolvedPath): Could not resolve ~ in path because it is not supported on Windows." << EidosTerminate();
	}
	#endif	
	
	return path;
}

// Get the filename (or a trailing directory name) from a path
std::string Eidos_LastPathComponent(const std::string &p_path)
{
	std::string path = Eidos_StripTrailingSlash(p_path);
	
	auto components = Eidos_string_split(path, "/");
	
	if (components.size() == 0)
		return "";
	
	return components.back();
}

// Get the current working directory; oddly, C++ has no API for this
std::string Eidos_CurrentDirectory(void)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Eidos_CurrentDirectory(): usage of statics");
	
	// buffer of size MAXPATHLEN * 8 to accommodate relatively long paths
	static char *path_buffer = nullptr;
	
	if (!path_buffer)
		path_buffer = (char *)malloc(MAXPATHLEN * 8 * sizeof(char));
	
	errno = 0;
	char *buf = getcwd(path_buffer, MAXPATHLEN * 8 * sizeof(char));
	
	if (!buf)
	{
		std::cout << "Eidos_CurrentDirectory(): Unable to get the current working directory (error " << errno << ")" << std::endl;
		return "ERROR";
	}
	
	return buf;
}

// Remove a trailing slash in a path like ~/foo/bar/
std::string Eidos_StripTrailingSlash(const std::string &p_path)
{
	int path_length = (int)p_path.length();
	bool path_ends_in_slash = (path_length > 0) && (p_path[path_length-1] == '/');
	
	if (path_ends_in_slash)
	{
		std::string path = p_path;
		
		path.pop_back();		// remove the trailing slash, which just confuses stat()
		return path;
	}
	
	return p_path;
}

// Create a directory at the given path if it does not already exist; returns false if an error occurred (which emits a warning)
bool Eidos_CreateDirectory(const std::string &p_path, std::string *p_error_string)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Eidos_CreateDirectory():  filesystem write");
	
	std::string path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(p_path));
	
	errno = 0;
	
	struct stat file_info;
	bool path_exists = (stat(path.c_str(), &file_info) == 0);
	
	if (path_exists)
	{
		bool is_directory = !!(file_info.st_mode & S_IFDIR);
		
		if (is_directory)
		{
			p_error_string->assign("#WARNING (Eidos_ExecuteFunction_createDirectory): function createDirectory() could not create a directory because a directory at the specified path already exists.");
			return true;
		}
		else
		{
			p_error_string->assign("#WARNING (Eidos_ExecuteFunction_createDirectory): function createDirectory() could not create a directory because a file at the specified path already exists.");
			return false;
		}
	}
	else if (errno == ENOENT)
	{
		// The path does not exist, so let's try to create it
		errno = 0;
		
		if (mkdir(path.c_str(), 0777) == 0)
		{
			// success
			p_error_string->assign("");
			return true;
		}
		else
		{
			p_error_string->assign("#WARNING (Eidos_ExecuteFunction_createDirectory): function createDirectory() could not create a directory because of an unspecified filesystem error.");
			return false;
		}
	}
	else
	{
		// The stat() call failed for an unknown reason
		p_error_string->assign("#WARNING (Eidos_ExecuteFunction_createDirectory): function createDirectory() could not create a directory because of an unspecified filesystem error.");
		return false;
	}
}

// This is /tmp/ (with trailing slash!) on macOS and Linux, but will be elsewhere on Windows.  Should be used instead of /tmp/ everywhere.
std::string Eidos_TemporaryDirectory(void)
{
	#ifdef _WIN32
	std::string temp_path;
	char charPath[MAX_PATH];
	if (GetTempPathA(MAX_PATH, charPath))
  		temp_path = charPath;
	// GetTempPathA gives a Windows path with Windows backslashes in it.
	// This breaks some other code because Eidos treats backslashes
	// as escape characters. So we replace them with forward slashes
	// which is understood by both linux and Windows.
	std::replace(temp_path.begin(), temp_path.end(), '\\', '/'); 
	return temp_path;
	#else
	return "/tmp/";
	#endif
}

bool Eidos_TemporaryDirectoryExists(void)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Eidos_TemporaryDirectoryExists(): usage of statics");
	
	// we cache the result for speed, making the assumption that the temporary directory will not change underneath us
	static bool been_here = false;
	static bool exists = false;
	
	if (!been_here)
	{
		std::string path = Eidos_TemporaryDirectory();
		struct stat file_info;
		bool path_exists = (stat(path.c_str(), &file_info) == 0);
		
		// test that Eidos_TemporaryDirectory() itself exists and is a directory
		if (path_exists)
		{
			bool is_directory = !!(file_info.st_mode & S_IFDIR);
			
			if (is_directory)
			{
				// test that it is writeable, in practice, by creating a temp file
				std::string prefix = Eidos_TemporaryDirectory() + "eidos_tmp_test";
				std::string suffix = ".txt";
				std::string file_path_template = prefix + "XXXXXX" + suffix;
				char *file_path_cstr = strdup(file_path_template.c_str());
				int fd = Eidos_mkstemps(file_path_cstr, 4);
				
				if (fd != -1)
				{
					std::string file_path(file_path_cstr);
					std::ofstream file_stream(file_path.c_str(), std::ios_base::out);
					close(fd);	// opened by Eidos_mkstemps()
					
					if (file_stream.is_open())
					{
						file_stream << "Eidos test of the temporary directory" << std::endl;
						
						if (!file_stream.bad())
						{
							file_stream.close();
							
							// test that we can delete the temp file
							if (remove(file_path.c_str()) == 0)
							{
								// passed all tests, so we consider that Eidos_TemporaryDirectory() exists
								exists = true;
							}
						}
					}
				}
				
				free(file_path_cstr);
				file_path_cstr = nullptr;
			}
		}
		
		been_here = true;
	}
	
	return exists;
}

// Create a temporary file based upon a template filename; note that pattern is modified!

// There is a function called mkstemps() on OS X, and on many Linux systems, but it is not
// standard and so we can't rely on it being present.  It is also not clear that it has
// exactly the same behavior on all systems where it is present.  So we use our own version
// of the function, taken indirectly from the GNU C library.

// This is based upon code from https://github.com/HSAFoundation/HSA-Debugger-GDB-Source-AMD/blob/master/gdb-7.8/libiberty/mkstemps.c
// That code has the following license notice:

/* Copyright (C) 1991, 1992, 1996, 1998, 2004 Free Software Foundation, Inc.
 This file is derived from mkstemp.c from the GNU C Library.
 The GNU C Library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public License as
 published by the Free Software Foundation; either version 2 of the
 License, or (at your option) any later version.
 The GNU C Library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 You should have received a copy of the GNU Library General Public
 License along with the GNU C Library; see the file COPYING.LIB.  If not,
 write to the Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,
 Boston, MA 02110-1301, USA.  */

// Since we are GPL anyway, there should be no problem with the inclusion of this code.

/*
 @deftypefn Replacement int mkstemps (char *@var{pattern}, int @var{suffix_len})
 Generate a unique temporary file name from @var{pattern}.
 @var{pattern} has the form:
 @example
 @var{path}/ccXXXXXX@var{suffix}
 @end example
 @var{suffix_len} tells us how long @var{suffix} is (it can be zero
 length).  The last six characters of @var{pattern} before @var{suffix}
 must be @samp{XXXXXX}; they are replaced with a string that makes the
 filename unique.  Returns a file descriptor open on the file for
 reading and writing.
 @end deftypefn
 */

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef TMP_MAX_EIDOS
#define TMP_MAX_EIDOS 16384
#endif

int Eidos_mkstemps(char *p_pattern, int p_suffix_len)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Eidos_mkstemps():  filesystem write");
	
	static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	static uint64_t value;
	size_t len = strlen(p_pattern);
	
	if (((int)len < 6 + p_suffix_len) || (strncmp(&p_pattern[len - 6 - p_suffix_len], "XXXXXX", 6) != 0))
		return -1;
	
	char *XXXXXX = &p_pattern[len - 6 - p_suffix_len];
	
	// Get some more or less random data
	struct timeval tv;
	
	gettimeofday(&tv, NULL);
	value += ((uint64_t) tv.tv_usec << 16) ^ tv.tv_sec ^ getpid();
	
	for (int count = 0; count < TMP_MAX_EIDOS; ++count)
	{
		uint64_t v = value;
		
		// Fill in the random bits
		XXXXXX[0] = letters[v % 62];
		v /= 62;
		XXXXXX[1] = letters[v % 62];
		v /= 62;
		XXXXXX[2] = letters[v % 62];
		v /= 62;
		XXXXXX[3] = letters[v % 62];
		v /= 62;
		XXXXXX[4] = letters[v % 62];
		v /= 62;
		XXXXXX[5] = letters[v % 62];
		
		int fd = open(p_pattern, O_BINARY | O_RDWR | O_CREAT | O_EXCL, 0600);
		
		if (fd >= 0)
			return fd;									// The file did not already exist; we have created it
		
		if ((errno != EEXIST) && (errno != EISDIR))		// Fatal error (EPERM, ENOSPC etc); doesn't make sense to loop
			break;
		
		// This is a random value.  It is only necessary that the next TMP_MAX_EIDOS
		// values generated by adding 7777 to value are different with (modulo 2^32).
		value += 7777;
	}
	
	// We return the null string if we can't find a unique file name
	p_pattern[0] = '\0';
	return -1;
}

int Eidos_mkstemps_directory(char *p_pattern, int p_suffix_len)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Eidos_mkstemps_directory():  filesystem write");
	
	static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	static uint64_t value;
	size_t len = strlen(p_pattern);
	
	if (((int)len < 6 + p_suffix_len) || (strncmp(&p_pattern[len - 6 - p_suffix_len], "XXXXXX", 6) != 0))
		return -1;
	
	char *XXXXXX = &p_pattern[len - 6 - p_suffix_len];
	
	// Get some more or less random data
	struct timeval tv;
	
	gettimeofday(&tv, NULL);
	value += ((uint64_t) tv.tv_usec << 16) ^ tv.tv_sec ^ getpid();
	
	for (int count = 0; count < TMP_MAX_EIDOS; ++count)
	{
		uint64_t v = value;
		
		// Fill in the random bits
		XXXXXX[0] = letters[v % 62];
		v /= 62;
		XXXXXX[1] = letters[v % 62];
		v /= 62;
		XXXXXX[2] = letters[v % 62];
		v /= 62;
		XXXXXX[3] = letters[v % 62];
		v /= 62;
		XXXXXX[4] = letters[v % 62];
		v /= 62;
		XXXXXX[5] = letters[v % 62];
		
		errno = 0;
		int ret = mkdir(p_pattern, 0777);
		
		if (ret == 0)
			return 0;				// The directory did not already exist; we have created it
		
		if (errno != EEXIST)		// Fatal error (EPERM, ENOSPC etc); doesn't make sense to loop
			break;
		
		// This is a random value.  It is only necessary that the next TMP_MAX_EIDOS
		// values generated by adding 7777 to value are different with (modulo 2^32).
		value += 7777;
	}
	
	// We return the null string if we can't find a unique file name
	p_pattern[0] = '\0';
	return -1;
}

#if EIDOS_BUFFER_ZIP_APPENDS
// This contains all unflushed append data for zip files written by writeFile(); see Eidos_FlushFiles() below
std::unordered_map<std::string, std::string> gEidosBufferedZipAppendData;

// This flushes the bytes in outstring to the file at file_path, with gzip append
bool _Eidos_FlushZipBuffer(const std::string &file_path, const std::string &outstring)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("_Eidos_FlushZipBuffer():  filesystem write");
	
	//std::cout << "_Eidos_FlushZipBuffer() called for " << file_path << std::endl;
	
	gzFile gzf = gzopen(file_path.c_str(), "ab");
	
	if (!gzf)
		return false;
	
	const char *outcstr = outstring.c_str();
	size_t outcstr_length = outstring.length();
	
	// do the writing with zlib
	bool success = false;
	int retval = gzbuffer(gzf, 128*1024L);	// bigger buffer for greater speed
	
	if (retval != -1)
	{
		retval = gzwrite(gzf, outcstr, (unsigned)outcstr_length);
		
		if ((retval != 0) || (outcstr_length == 0))	// writing 0 bytes returns 0, which is supposed to be an error code
		{
			retval = gzclose_w(gzf);
			
			if (retval == Z_OK)
				success = true;
		}
	}
	
	return success;
}
#endif

// This flushes a given file, if it is buffering zip output
void Eidos_FlushFile(const std::string &p_file_path)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Eidos_FlushFile():  filesystem write");
	
#if EIDOS_BUFFER_ZIP_APPENDS
	auto buffer_iter = gEidosBufferedZipAppendData.find(p_file_path);
	
	if (buffer_iter != gEidosBufferedZipAppendData.end())
	{
		bool result = _Eidos_FlushZipBuffer(buffer_iter->first, buffer_iter->second);
		
		if (!result)
			EIDOS_TERMINATION << "ERROR (Eidos_FlushFile): Flush of gzip data to file " << buffer_iter->first << " failed!" << EidosTerminate(nullptr);
		
		gEidosBufferedZipAppendData.erase(buffer_iter);
	}
#endif
}

// This flushes all outstanding buffered zip data to the appropriate files
void Eidos_FlushFiles(void)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Eidos_FlushFiles():  filesystem write");
	
#if EIDOS_BUFFER_ZIP_APPENDS
	// Write out buffered data in gEidosBufferedZipAppendData to the appropriate files, using zlib's gzip append mode
	for (auto &buffer_pair : gEidosBufferedZipAppendData)
	{
		bool result = _Eidos_FlushZipBuffer(buffer_pair.first, buffer_pair.second);
		
		if (!result)
		{
			// Note that we do this without a raise, because we often want to flush when we're already handling a raise; simpler to just log, the user will figure it out...
			std::cerr << std::endl << "ERROR (Eidos_FlushFiles): Flush of gzip data to file " << buffer_pair.first << " failed!" << std::endl;
		}
	}
	
	gEidosBufferedZipAppendData.clear();
#endif
}

void Eidos_WriteToFile(const std::string &p_file_path, const std::vector<const std::string *> &p_contents, bool p_append, bool p_compress, EidosFileFlush p_flush_option)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Eidos_WriteToFile():  filesystem write");
	
	// note that we add a newline after the last line in all cases, so that appending new content to a file produces correct line breaks
	
	if (p_compress)
	{
		// compression using zlib; very different from the no-compression case, unfortunately, because here we use C-based APIs
		#if EIDOS_BUFFER_ZIP_APPENDS
		if (p_append)
		{
			// the append case gets handled by _Eidos_FlushZipBuffer() if EIDOS_BUFFER_ZIP_APPENDS is true
			auto buffer_iter = gEidosBufferedZipAppendData.find(p_file_path);
			
			if (buffer_iter == gEidosBufferedZipAppendData.end())
				buffer_iter = gEidosBufferedZipAppendData.emplace(p_file_path, "").first;
			
			std::string &buffer = buffer_iter->second;
			
			// append lines to the buffer; this copies bytes, which is a bit inefficient but shouldn't matter in the big picture
			for (const std::string *content_line : p_contents)
			{
				buffer.append(*content_line);
				buffer.append(1, '\n');
			}
			
			// if the buffer data exceeds a (somewhat arbitrary) 128K buffer maximum, write it out and remove the buffer entry
			if ((p_flush_option == EidosFileFlush::kForceFlush) ||
				((p_flush_option == EidosFileFlush::kDefaultFlush) && (buffer.length() > 1024L * 128L)))
			{
				bool result = _Eidos_FlushZipBuffer(p_file_path, buffer);
				gEidosBufferedZipAppendData.erase(buffer_iter);
				
				if (!result)
					EIDOS_TERMINATION << "#ERROR (Eidos_WriteToFile): could not flush zip buffer to file at path " << p_file_path << "." << EidosTerminate(nullptr);
			}
		}
		else
		#endif
		{
			// this code can handle both the append and the non-append case, but the append case may generate very low-quality
			// compression (potentially even worse than the uncompressed data) due to having an excess of gzip headers
			gzFile gzf = gzopen(p_file_path.c_str(), p_append ? "ab" : "wb");
			
			if (!gzf)
				EIDOS_TERMINATION << "#ERROR (Eidos_WriteToFile): could not write to file at path " << p_file_path << "." << EidosTerminate(nullptr);
			
			std::ostringstream outstream;
			
			for (const std::string *content_line : p_contents)
				outstream << *content_line << std::endl;
			
			std::string outstring = outstream.str();
			const char *outcstr = outstring.c_str();
			size_t outcstr_length = strlen(outcstr);
			
			// do the writing with zlib
			bool failed = true;
			int retval = gzbuffer(gzf, 128*1024L);	// bigger buffer for greater speed
			
			if (retval != -1)
			{
				retval = gzwrite(gzf, outcstr, (unsigned)outcstr_length);
				
				if ((retval != 0) || (outcstr_length == 0))	// writing 0 bytes returns 0, which is supposed to be an error code
				{
					retval = gzclose_w(gzf);
					
					if (retval == Z_OK)
						failed = false;
				}
			}
			
			if (failed)
				EIDOS_TERMINATION << "#ERROR (Eidos_WriteToFile): encountered zlib errors while writing to file at path " << p_file_path << "." << EidosTerminate(nullptr);
		}
	}
	else
	{
		// no compression
		std::ofstream file_stream(p_file_path.c_str(), p_append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		
		if (!file_stream.is_open())
			EIDOS_TERMINATION << "#ERROR (Eidos_WriteToFile): could not write to file at path " << p_file_path << "." << EidosTerminate(nullptr);
		
		for (const std::string *content_line : p_contents)
			file_stream << *content_line << std::endl;
		
		if (file_stream.bad())
			EIDOS_TERMINATION << "#ERROR (Eidos_WriteToFile): encountered stream errors while writing to file at path " << p_file_path << "." << EidosTerminate(nullptr);
	}
}


#pragma mark -
#pragma mark Utility functions
#pragma mark -

// Welch's t-test.  This function returns the p-value for a two-sided unpaired Welch's
// t-test between two samples.  The null hypothesis is that the means of the two samples
// are not different.  If p < alpha, this null hypothesis is rejected, supporting the
// alternative hypothesis that the two samples are drawn from different distributions.
// As I understand it, that this code uses biased estimators of the variance and std.
// deviation, presumably for simplicity and speed, so the results will be somewhat
// inexact for small sample sizes.

// This code is modified from WiggleTools ( https://github.com/Ensembl/WiggleTools ),
// from https://github.com/Ensembl/WiggleTools/blob/master/src/setComparisons.c .
// Thanks to EMBL-European Bioinformatics Institute for making this code available.

// WiggleTools is licensed under the Apache 2.0 license
// That license is compatible with the GPL 3.0 that we are licensed under, according
// to https://www.apache.org/licenses/GPL-compatibility.html )
// The original notice from WiggleTools follows:

// Copyright [1999-2017] EMBL-European Bioinformatics Institute
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
// http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

double Eidos_TTest_TwoSampleWelch(const double *p_set1, int p_count1, const double *p_set2, int p_count2, double *p_set_mean1, double *p_set_mean2)
{
	if ((p_count1 <= 1) || (p_count2 <= 1))
	{
		std::cout << "Eidos_TTest_TwoSampleWelch requires enough elements to compute variance" << std::endl;
		return NAN;
	}
	
	// Compute measurements
	double sum1 = 0, sum2 = 0, sumSq1 = 0, sumSq2 = 0;
	int index;
	
	for (index = 0; index < p_count1; index++) {
		double value = p_set1[index];
		
		sum1 += value;
		sumSq1 += value * value;
	}
	
	for (index = 0; index < p_count2; index++) {
		double value = p_set2[index];
		
		sum2 += value;
		sumSq2 += value * value;
	}
	
	double mean1 = sum1 / p_count1;
	double mean2 = sum2 / p_count2;
	double meanSq1 = sumSq1 / p_count1;
	double meanSq2 = sumSq2 / p_count2;
	double var1 = meanSq1 - mean1 * mean1;
	double var2 = meanSq2 - mean2 * mean2;
	
	if (p_set_mean1)
		*p_set_mean1 = mean1;
	if (p_set_mean2)
		*p_set_mean2 = mean2;
	
	// To avoid divisions by 0:
	if (var1 + var2 == 0)
		return NAN;
	
	// two-sample test
	double t = (mean1 - mean2) / sqrt(var1 / p_count1 + var2 / p_count2);
	
	if (t < 0)
		t = -t;
	
	double nu = (var1 / p_count1 + var2 / p_count2) * (var1 / p_count1 + var2 / p_count2) / ((var1 * var1) / (p_count1 * p_count1 * (p_count1 - 1)) + (var2 * var2) / (p_count2 * p_count2 * (p_count2 - 1)));
	
	// return the P-value
	return (std::isnan(t) ? t : 2 * gsl_cdf_tdist_Q(t, nu));
}

// This function returns a one-sample t-test, testing the null hypothesis that
// the mean of the sample is equal to mu.  This code is obviously derived from
// the code above, but was written by me in consultation with Wikipedia.

double Eidos_TTest_OneSample(const double *p_set1, int p_count1, double p_mu, double *p_set_mean1)
{
	if (p_count1 <= 1)
	{
		std::cout << "Eidos_TTest_OneSample requires enough elements to compute variance" << std::endl;
		return NAN;
	}
	
	// Compute measurements
	double sum1 = 0, sumSq1 = 0;
	int index;
	
	for (index = 0; index < p_count1; index++) {
		double value = p_set1[index];
		
		sum1 += value;
		sumSq1 += value * value;
	}
	
	double mean1 = sum1 / p_count1;
	double meanSq1 = sumSq1 / p_count1;
	double var1 = meanSq1 - mean1 * mean1;
	
	if (p_set_mean1)
		*p_set_mean1 = mean1;
	
	// To avoid divisions by 0:
	if (var1 == 0)
		return NAN;
	
	// one-sample test
	double t = (mean1 - p_mu) / (sqrt(var1) / sqrt(p_count1));
	
	if (t < 0)
		t = -t;
	
	double nu = p_count1 - 1;
	
	// return the P-value
	return (std::isnan(t) ? t : 2 * gsl_cdf_tdist_Q(t, nu));
}

// This function uses an algorithm by Shewchuk (http://www.cs.berkeley.edu/~jrs/papers/robustr.pdf) to provide
// an exact sum (within the precision limits of the double type) for a vector of double values.  This code is
// adapted from source code in Python for its fsum() function, as implemented in the file mathmodule.c in the
// math_fsum() C function, from Python version 3.6.2, downloaded from https://www.python.org/getit/source/.
// The authors of that code appear to be Raymond Hettinger and Mark Dickinson; a big thank-you to them.
// The PSF open-source license for Python 3.6.2, which the PSF states is GSL-compatible, may be found on their
// website at https://docs.python.org/3.6/license.html.  Their license follows:

/*

1. This LICENSE AGREEMENT is between the Python Software Foundation ("PSF"), and
the Individual or Organization ("Licensee") accessing and otherwise using Python
3.6.2 software in source or binary form and its associated documentation.

2. Subject to the terms and conditions of this License Agreement, PSF hereby
grants Licensee a nonexclusive, royalty-free, world-wide license to reproduce,
analyze, test, perform and/or display publicly, prepare derivative works,
distribute, and otherwise use Python 3.6.2 alone or in any derivative
version, provided, however, that PSF's License Agreement and PSF's notice of
copyright, i.e., "Copyright © 2001-2017 Python Software Foundation; All Rights
Reserved" are retained in Python 3.6.2 alone or in any derivative version
prepared by Licensee.

3. In the event Licensee prepares a derivative work that is based on or
incorporates Python 3.6.2 or any part thereof, and wants to make the
derivative work available to others as provided herein, then Licensee hereby
agrees to include in any such work a brief summary of the changes made to Python
3.6.2.

4. PSF is making Python 3.6.2 available to Licensee on an "AS IS" basis.
PSF MAKES NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  BY WAY OF
EXAMPLE, BUT NOT LIMITATION, PSF MAKES NO AND DISCLAIMS ANY REPRESENTATION OR
WARRANTY OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE
USE OF PYTHON 3.6.2 WILL NOT INFRINGE ANY THIRD PARTY RIGHTS.

5. PSF SHALL NOT BE LIABLE TO LICENSEE OR ANY OTHER USERS OF PYTHON 3.6.2
FOR ANY INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES OR LOSS AS A RESULT OF
MODIFYING, DISTRIBUTING, OR OTHERWISE USING PYTHON 3.6.2, OR ANY DERIVATIVE
THEREOF, EVEN IF ADVISED OF THE POSSIBILITY THEREOF.

6. This License Agreement will automatically terminate upon a material breach of
its terms and conditions.

7. Nothing in this License Agreement shall be deemed to create any relationship
of agency, partnership, or joint venture between PSF and Licensee.  This License
Agreement does not grant permission to use PSF trademarks or trade name in a
trademark sense to endorse or promote products or services of Licensee, or any
third party.

8. By copying, installing or otherwise using Python 3.6.2, Licensee agrees
to be bound by the terms and conditions of this License Agreement.

*/

// As to the "brief summary of the changes made" requested by their license, I have de-Pythoned their
// code to make it work directly with a vector of doubles and return a double value; and I have changed
// the way that the partials array is kept, now using a permanently allocated buffer grown with realloc();
// and I have renamed the function; and I have removed some asserts and error checks; otherwise I have
// tried to preserve their algorithm. The comments below are from the Python source.  BCH 3 August 2017.

/* Precision summation function as msum() by Raymond Hettinger in
 <http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/393090>,
 enhanced with the exact partials sum and roundoff from Mark
 Dickinson's post at <http://bugs.python.org/file10357/msum4.py>.
 See those links for more details, proofs and other references.
 
 Note 1: IEEE 754R floating point semantics are assumed,
 but the current implementation does not re-establish special
 value semantics across iterations (i.e. handling -Inf + Inf).
 
 Note 2:  No provision is made for intermediate overflow handling;
 therefore, sum([1e+308, 1e-308, 1e+308]) returns 1e+308 while
 sum([1e+308, 1e+308, 1e-308]) raises an OverflowError due to the
 overflow of the first partial sum.
 
 Note 3: The intermediate values lo, yr, and hi are declared volatile so
 aggressive compilers won't algebraically reduce lo to always be exactly 0.0.
 Also, the volatile declaration forces the values to be stored in memory as
 regular doubles instead of extended long precision (80-bit) values.  This
 prevents double rounding because any addition or subtraction of two doubles
 can be resolved exactly into double-sized hi and lo values.  As long as the
 hi value gets forced into a double before yr and lo are computed, the extra
 bits in downstream extended precision operations (x87 for example) will be
 exactly zero and therefore can be losslessly stored back into a double,
 thereby preventing double rounding.
 
 Note 4: A similar implementation is in Modules/cmathmodule.c.
 Be sure to update both when making changes.
 
 Note 5: The signature of math.fsum() differs from builtins.sum()
 because the start argument doesn't make sense in the context of
 accurate summation.  Since the partials table is collapsed before
 returning a result, sum(seq2, start=sum(seq1)) may not equal the
 accurate result returned by sum(itertools.chain(seq1, seq2)).
 */

/* Full precision summation of a sequence of floats.
 
	def msum(iterable):
		partials = []  # sorted, non-overlapping partial sums
		for x in iterable:
			i = 0
			for y in partials:
				if abs(x) < abs(y):
					x, y = y, x
				hi = x + y
				lo = y - (hi - x)
				if lo:
					partials[i] = lo
					i += 1
				x = hi
			partials[i:] = [x]
		return sum_exact(partials)
 
 Rounded x+y stored in hi with the roundoff stored in lo.  Together hi+lo
 are exactly equal to x+y.  The inner loop applies hi/lo summation to each
 partial so that the list of partial sums remains exact.
 
 Sum_exact() adds the partial sums exactly and correctly rounds the final
 result (using the round-half-to-even rule).  The items in partials remain
 non-zero, non-special, non-overlapping and strictly increasing in
 magnitude, but possibly not all having the same sign.
 
 Depends on IEEE 754 arithmetic guarantees and half-even rounding.
 */

double Eidos_ExactSum(const double *p_double_vec, int64_t p_vec_length)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Eidos_ExactSum(): usage of statics");
	
	// We allocate the partials using malloc() rather than initially using the stack,
	// and keep the allocated block around forever; simpler if a bit less efficient.
	static double *p = nullptr;		// partials array
	static int m = 0;				// size of partials array
	
	if (m == 0)
	{
		m = 32;			// this is NUM_PARTIALS in the Python code
		p = (double *)malloc(m * sizeof(double));
		if (!p)
			EIDOS_TERMINATION << "ERROR (Eidos_ExactSum): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	}
	
	int i, j, n = 0;
	double x, y;
	double xsave, special_sum = 0.0, inf_sum = 0.0;
	volatile double hi, yr, lo = 0.0;		// see comment above re: volatile; added initializer to get rid of a false warning
	
	for (int64_t vec_index = 0; vec_index < p_vec_length; ++vec_index)
	{
		//assert(0 <= n && n <= m);
		
		x = p_double_vec[vec_index];
		
		xsave = x;
		for (i = j = 0; j < n; j++) {       /* for y in partials */
			y = p[j];
			if (fabs(x) < fabs(y))
				std::swap(x, y);
			hi = x + y;
			yr = hi - x;
			lo = y - yr;
			if (lo != 0.0)
				p[i++] = lo;
			x = hi;
		}
		
		n = i;                              /* ps[i:] = [x] */
		if (x != 0.0) {
			if (!std::isfinite(x)) {
				/* a nonfinite x could arise either as
				 a result of intermediate overflow, or
				 as a result of a nan or inf in the
				 summands */
				if (std::isfinite(xsave))
				{
					// Python returns some sort of error object in this case; we throw
					EIDOS_TERMINATION << "ERROR (Eidos_ExactSum): intermediate overflow in Eidos_ExactSum()." << EidosTerminate(nullptr);
				}
				
				if (std::isinf(xsave))
					inf_sum += xsave;
				special_sum += xsave;
				/* reset partials */
				n = 0;
			}
			else
			{
				if (n >= m)
				{
					m = m * 2;
					p = (double *)realloc(p, m * sizeof(double));	// NOLINT(*-realloc-usage) : realloc failure is a fatal error anyway
					if (!p)
						EIDOS_TERMINATION << "ERROR (Eidos_ExactSum): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
				}
				
				p[n++] = x;
			}
		}
	}
	
	if (special_sum != 0.0) {
		if (std::isnan(inf_sum))
		{
			// Python returns some sort of error object in this case; we throw
			EIDOS_TERMINATION << "ERROR (Eidos_ExactSum): -inf + inf in Eidos_ExactSum()." << EidosTerminate(nullptr);
		}
		else
		{
			return special_sum;
		}
	}
	
	hi = 0.0;
	if (n > 0) {
		hi = p[--n];
		/* sum_exact(ps, hi) from the top, stop when the sum becomes
		 inexact. */
		while (n > 0) {
			x = hi;
			y = p[--n];
			//assert(fabs(y) < fabs(x));
			hi = x + y;
			yr = hi - x;
			lo = y - yr;
			if (lo != 0.0)
				break;
		}
		/* Make half-even rounding work across multiple partials.
		 Needed so that sum([1e-16, 1, 1e16]) will round-up the last
		 digit to two instead of down to zero (the 1e-16 makes the 1
		 slightly closer to two).  With a potential 1 ULP rounding
		 error fixed-up, math.fsum() can guarantee commutativity. */
		if (n > 0 && ((lo < 0.0 && p[n-1] < 0.0) ||
					  (lo > 0.0 && p[n-1] > 0.0))) {
			y = lo * 2.0;
			x = hi + y;
			yr = x - hi;
			if (y == yr)
				hi = x;
		}
	}
	return hi;
}

bool Eidos_ApproximatelyEqual(double a, double b)
{
	// different signs is a mismatch
	if (std::signbit(a) != std::signbit(b))
		return false;
	
	// both zero is not a mismatch (getting rid of this case for div-by-zero safety
	if ((a == 0) && (b == 0))
		return true;
	
	// one zero (and one not) is a mismatch
	if ((a == 0) || (b == 0))
		return false;
	
	// one significantly bigger is a mismatch
	if (a / b > 1.0001)
		return false;
	
	// the other significantly bigger is a mismatch
	if (b / a > 1.0001)
		return false;
	
	return true;
}

std::vector<std::string> Eidos_string_split(const std::string &joined_string, const std::string &separator)
{
	std::vector<std::string> tokens;
	std::string::size_type start_idx = 0, sep_idx;
	
	if (separator.length() == 0)
	{
		// special-case a zero-length separator
		for (const char &ch : joined_string)
			tokens.emplace_back(&ch, 1);
	}
	else
	{
		// non-zero-length separator
		while (true)
		{
			sep_idx = joined_string.find(separator, start_idx);
			
			if (sep_idx == std::string::npos)
			{
				tokens.emplace_back(joined_string.substr(start_idx));
				break;
			}
			else
			{
				tokens.emplace_back(joined_string.substr(start_idx, sep_idx - start_idx));
				start_idx = sep_idx + separator.size();
			}
		}
	}
	
	return tokens;
}

std::string Eidos_string_join(const std::vector<std::string> &p_vec, const std::string &p_delim)
{
	std::string result;
	size_t vec_size = p_vec.size();
	
	for (size_t i = 0; i < vec_size; ++i)
	{
		if (i > 0)
			result.append(p_delim);
		result.append(p_vec[i]);
	}
	
	return result;
}

// thanks to https://stackoverflow.com/a/874160/2752221
bool Eidos_string_hasPrefix(std::string const &fullString, std::string const &prefix)
{
	if (fullString.length() >= prefix.length()) {
		return (0 == fullString.compare(0, prefix.length(), prefix));
	} else {
		return false;
	}
}

// thanks to https://stackoverflow.com/a/874160/2752221
bool Eidos_string_hasSuffix(std::string const &fullString, std::string const &suffix)
{
	if (fullString.length() >= suffix.length()) {
		return (0 == fullString.compare(fullString.length() - suffix.length(), suffix.length(), suffix));
	} else {
		return false;
	}
}

// case-insensitive string find and string equality; see https://stackoverflow.com/a/19839371/2752221
bool Eidos_string_containsCaseInsensitive(const std::string &strHaystack, const std::string &strNeedle)
{
	auto it = std::search(
						  strHaystack.begin(), strHaystack.end(),
						  strNeedle.begin(),   strNeedle.end(),
						  [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
						  );
	return (it != strHaystack.end() );
}

bool Eidos_string_equalsCaseInsensitive(const std::string &s1, const std::string &s2)
{
	if (s1.length() != s2.length())
		return false;
	
	return Eidos_string_containsCaseInsensitive(s1, s2);
}

// quotes and adds backslash escapes
std::string Eidos_string_escaped(const std::string &unescapedString, EidosStringQuoting quoting)
{
	bool use_single_quote = false, use_double_quote = false;
	
	if (quoting == EidosStringQuoting::kDoubleQuotes)
		use_double_quote = true;
	else if (quoting == EidosStringQuoting::kSingleQuotes)
		use_single_quote = true;
	else if (quoting == EidosStringQuoting::kChooseQuotes)
	{
		if (unescapedString.find('"') != std::string::npos)
			use_single_quote = true;
		else
			use_double_quote = true;
	}
	
	std::string escapedString;
	
	// add the opening quote
	if (use_single_quote)
		escapedString = '\'';
	else if (use_double_quote)
		escapedString = '"';
	
	// add characters from unquotedString one by one, escaping them if necessary; we do not do arbitrary unicode or control-character escapes
	for (char ch : unescapedString)
	{
		if (ch == '\r')
			escapedString += "\\r";
		else if (ch == '\n')
			escapedString += "\\n";
		else if (ch == '\t')
			escapedString += "\\t";
		else if (ch == '\\')
			escapedString += "\\\\";
		else if ((ch == '\"') && use_double_quote)		// only escape double quotes if the exterior quotes are double
			escapedString += "\\\"";
		else if ((ch == '\'') && use_single_quote)		// only escape single quotes if the exterior quotes are single
			escapedString += "\\\'";
		else
			escapedString += ch;
	}
	
	// add the closing quote
	if (use_single_quote)
		escapedString += '\'';
	else if (use_double_quote)
		escapedString += '"';
	
	return escapedString;
}

// quotes and ""-escapes quotes
std::string Eidos_string_escaped_CSV(const std::string &unescapedString)
{
	std::string escapedString;
	
	escapedString = '"';
	
	// add characters from unquotedString one by one, escaping them if necessary; for CSV we only escape double quotes
	for (char ch : unescapedString)
	{
		if (ch == '\"')
			escapedString += "\"\"";	// a single " turns into "", in the CSV quoting style
		else
			escapedString += ch;
	}
	
	// add the closing quote
	escapedString += '"';
	
	return escapedString;
}

// run a Un*x command; thanks to http://stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c-using-posix
/*std::string Eidos_Exec(const char *p_cmd)
{
	char buffer[128];
	std::string result = "";
	std::shared_ptr<FILE> command_pipe(popen(p_cmd, "r"), pclose);
	if (!command_pipe) throw std::runtime_error("popen() failed!");
	while (!feof(command_pipe.get())) {
		if (fgets(buffer, 128, command_pipe.get()) != NULL)
			result += buffer;
	}
	return result;
}*/

std::string EidosStringForFloat(double p_value)
{
	// Customize our output a bit to look like Eidos, not C++
	if (std::isinf(p_value))
	{
		if (std::signbit(p_value))
			return gEidosStr_MINUS_INF;
		else
			return gEidosStr_INF;
	}
	else if (std::isnan(p_value))
		return gEidosStr_NAN;
	else
	{
		// could probably use std::to_string() instead, but need to think about precision etc.
		std::ostringstream ss;
		ss << std::setprecision(gEidosFloatOutputPrecision);
		ss << p_value;
		std::string string_value = ss.str();
		
		// BCH 10/13/2021: I'd like float values to always print with a decimal point.  This is a change in
		// behavior in Eidos 2.7 (SLiM 3.7), but it seems unlikely to bite anybody – the opposite, really,
		// since it increases clarity and consistency.  So now we look for a decimal point in the float,
		// and if there is none, we append ".0".  We also need to worry about scientific notation; if there
		// is an "e" or "E", we insert the ".0" just before that to produce 1.0e30 rather than 1e30.
		if (string_value.find('.') == std::string::npos)
		{
			size_t e_pos = string_value.find_first_of("eE");
			
			if (e_pos == std::string::npos)
				string_value.append(".0");
			else
				string_value.insert(e_pos, ".0");
		}
		
		return string_value;
	}
}

int DisplayDigitsForIntegerPart(double x)
{
	// This function just uses log10 to give the number of digits needed to display the integer part of a double.
	// The reason it's split out into a function is that the result, for x==0, is -inf, and we want to return 1.
	double digits = ceil(log10(floor(x)));
	
	if (std::isfinite(digits))
		return (int)digits;
	return 1;
}

bool Eidos_RegexWorks(void)
{
	// check whether <regex> works, because on some platforms it doesn't (!); test just once and cache the result
	static bool beenHere = false;
	static bool regex_works = false;
	
#pragma omp critical (Eidos_RegexWorks)
	{
		if (!beenHere)
		{
			std::regex pattern_regex("cd", std::regex_constants::ECMAScript);
			std::string x_element = "bcd";
			std::smatch match_info;
			bool is_match = std::regex_search(x_element, match_info, pattern_regex);
			
			regex_works = is_match;
			beenHere = true;
		}
	}
	
	return regex_works;
}


// *******************************************************************************************************************
//
//	SHA-256
//

// This code is from https://github.com/amosnier/sha-2 which is under the public-domain "unlicense".
// Thanks to Alain Mosnier for this code.  License follows:

/*

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org>

*/

#pragma mark -
#pragma mark SHA-256
#pragma mark -

#define CHUNK_SIZE 64
#define TOTAL_LEN_LEN 8

/*
 * ABOUT bool: this file does not use bool in order to be as pre-C99 compatible as possible.
 */

/*
 * Comments from pseudo-code at https://en.wikipedia.org/wiki/SHA-2 are reproduced here.
 * When useful for clarification, portions of the pseudo-code are reproduced here too.
 */

/*
 * Initialize array of round constants:
 * (first 32 bits of the fractional parts of the cube roots of the first 64 primes 2..311):
 */
static const uint32_t SHA_k[] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

struct SHA_buffer_state {
	const uint8_t * p;
	size_t len;
	size_t total_len;
	int single_one_delivered; /* bool */
	int total_len_delivered; /* bool */
};

static inline uint32_t right_rot(uint32_t value, unsigned int count)
{
	/*
	 * Defined behaviour in standard C for all count where 0 < count < 32,
	 * which is what we need here.
	 */
	return value >> count | value << (32 - count);
}

static void init_buf_state(struct SHA_buffer_state * state, const void * input, size_t len)
{
	state->p = static_cast<const uint8_t *>(input);
	state->len = len;
	state->total_len = len;
	state->single_one_delivered = 0;
	state->total_len_delivered = 0;
}

/* Return value: bool */
static int calc_chunk(uint8_t chunk[CHUNK_SIZE], struct SHA_buffer_state * state)
{
	size_t space_in_chunk;

	if (state->total_len_delivered) {
		return 0;
	}

	if (state->len >= CHUNK_SIZE) {
		memcpy(chunk, state->p, CHUNK_SIZE);
		state->p += CHUNK_SIZE;
		state->len -= CHUNK_SIZE;
		return 1;
	}

	memcpy(chunk, state->p, state->len);
	chunk += state->len;
	space_in_chunk = CHUNK_SIZE - state->len;
	state->p += state->len;
	state->len = 0;

	/* If we are here, space_in_chunk is one at minimum. */
	if (!state->single_one_delivered) {
		*chunk++ = 0x80;
		space_in_chunk -= 1;
		state->single_one_delivered = 1;
	}

	/*
	 * Now:
	 * - either there is enough space left for the total length, and we can conclude,
	 * - or there is too little space left, and we have to pad the rest of this chunk with zeroes.
	 * In the latter case, we will conclude at the next invokation of this function.
	 */
	if (space_in_chunk >= TOTAL_LEN_LEN) {
		const size_t left = space_in_chunk - TOTAL_LEN_LEN;
		size_t len = state->total_len;
		int i;
		memset(chunk, 0x00, left);
		chunk += left;

		/* Storing of len * 8 as a big endian 64-bit without overflow. */
		chunk[7] = (uint8_t) (len << 3);
		len >>= 5;
		for (i = 6; i >= 0; i--) {
			chunk[i] = (uint8_t) len;
			len >>= 8;
		}
		state->total_len_delivered = 1;
	} else {
		memset(chunk, 0x00, space_in_chunk);
	}

	return 1;
}

/*
 * Limitations:
 * - Since input is a pointer in RAM, the data to hash should be in RAM, which could be a problem
 *   for large data sizes.
 * - SHA algorithms theoretically operate on bit strings. However, this implementation has no support
 *   for bit string lengths that are not multiples of eight, and it really operates on arrays of bytes.
 *   In particular, the len parameter is a number of bytes.
 */
void Eidos_calc_sha_256(uint8_t hash[32], const void *input, size_t len)
{
	/*
	 * Note 1: All integers (expect indexes) are 32-bit unsigned integers and addition is calculated modulo 2^32.
	 * Note 2: For each round, there is one round constant k[i] and one entry in the message schedule array w[i], 0 = i = 63
	 * Note 3: The compression function uses 8 working variables, a through h
	 * Note 4: Big-endian convention is used when expressing the constants in this pseudocode,
	 *     and when parsing message block data from bytes to words, for example,
	 *     the first word of the input message "abc" after padding is 0x61626380
	 */

	/*
	 * Initialize hash values:
	 * (first 32 bits of the fractional parts of the square roots of the first 8 primes 2..19):
	 */
	uint32_t h[] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
	unsigned i, j;

	/* 512-bit chunks is what we will operate on. */
	uint8_t chunk[64];

	struct SHA_buffer_state state;

	init_buf_state(&state, input, len);

	while (calc_chunk(chunk, &state)) {
		uint32_t ah[8];

		const uint8_t *p = chunk;

		/* Initialize working variables to current hash value: */
		for (i = 0; i < 8; i++)
			ah[i] = h[i];

		/* Compression function main loop: */
		for (i = 0; i < 4; i++) {
			/*
			 * The w-array is really w[64], but since we only need
			 * 16 of them at a time, we save stack by calculating
			 * 16 at a time.
			 *
			 * This optimization was not there initially and the
			 * rest of the comments about w[64] are kept in their
			 * initial state.
			 */

			/*
			 * create a 64-entry message schedule array w[0..63] of 32-bit words
			 * (The initial values in w[0..63] don't matter, so many implementations zero them here)
			 * copy chunk into first 16 words w[0..15] of the message schedule array
			 */
			uint32_t w[16];

			for (j = 0; j < 16; j++) {
				if (i == 0) {
					w[j] = (uint32_t) p[0] << 24 | (uint32_t) p[1] << 16 |
						(uint32_t) p[2] << 8 | (uint32_t) p[3];
					p += 4;
				} else {
					/* Extend the first 16 words into the remaining 48 words w[16..63] of the message schedule array: */
					const uint32_t s0 = right_rot(w[(j + 1) & 0xf], 7) ^ right_rot(w[(j + 1) & 0xf], 18) ^ (w[(j + 1) & 0xf] >> 3);
					const uint32_t s1 = right_rot(w[(j + 14) & 0xf], 17) ^ right_rot(w[(j + 14) & 0xf], 19) ^ (w[(j + 14) & 0xf] >> 10);
					w[j] = w[j] + s0 + w[(j + 9) & 0xf] + s1;
				}
				const uint32_t s1 = right_rot(ah[4], 6) ^ right_rot(ah[4], 11) ^ right_rot(ah[4], 25);
				const uint32_t ch = (ah[4] & ah[5]) ^ (~ah[4] & ah[6]);
				const uint32_t temp1 = ah[7] + s1 + ch + SHA_k[i << 4 | j] + w[j];
				const uint32_t s0 = right_rot(ah[0], 2) ^ right_rot(ah[0], 13) ^ right_rot(ah[0], 22);
				const uint32_t maj = (ah[0] & ah[1]) ^ (ah[0] & ah[2]) ^ (ah[1] & ah[2]);
				const uint32_t temp2 = s0 + maj;

				ah[7] = ah[6];
				ah[6] = ah[5];
				ah[5] = ah[4];
				ah[4] = ah[3] + temp1;
				ah[3] = ah[2];
				ah[2] = ah[1];
				ah[1] = ah[0];
				ah[0] = temp1 + temp2;
			}
		}

		/* Add the compressed chunk to the current hash value: */
		for (i = 0; i < 8; i++)
			h[i] += ah[i];
	}

	/* Produce the final hash value (big-endian): */
	for (i = 0, j = 0; i < 8; i++)
	{
		hash[j++] = (uint8_t) (h[i] >> 24);
		hash[j++] = (uint8_t) (h[i] >> 16);
		hash[j++] = (uint8_t) (h[i] >> 8);
		hash[j++] = (uint8_t) h[i];
	}
}

#undef CHUNK_SIZE
#undef TOTAL_LEN_LEN

void Eidos_hash_to_string(char string[65], const uint8_t hash[32])
{
	size_t i;
	for (i = 0; i < 32; i++) {
		string += snprintf(string, 65, "%02x", hash[i]);
	}
}	


#pragma mark -
#pragma mark Global strings & IDs
#pragma mark -

//	Global std::string objects.
const std::string &gEidosStr_empty_string = EidosRegisteredString("", gEidosID_empty_string);
const std::string &gEidosStr_space_string = EidosRegisteredString(" ", gEidosID_space_string);

// mostly function names used in multiple places
const std::string &gEidosStr_apply = EidosRegisteredString("apply", gEidosID_apply);
const std::string &gEidosStr_sapply = EidosRegisteredString("sapply", gEidosID_sapply);
const std::string &gEidosStr_doCall = EidosRegisteredString("doCall", gEidosID_doCall);
const std::string &gEidosStr_executeLambda = EidosRegisteredString("executeLambda", gEidosID_executeLambda);
const std::string &gEidosStr__executeLambda_OUTER = EidosRegisteredString("_executeLambda_OUTER", gEidosID__executeLambda_OUTER);
const std::string &gEidosStr_ls = EidosRegisteredString("ls", gEidosID_ls);
const std::string &gEidosStr_rm = EidosRegisteredString("rm", gEidosID_rm);
const std::string &gEidosStr_usage = EidosRegisteredString("usage", gEidosID_usage);

// mostly language keywords
const std::string &gEidosStr_if = EidosRegisteredString("if", gEidosID_if);
const std::string &gEidosStr_else = EidosRegisteredString("else", gEidosID_else);
const std::string &gEidosStr_do = EidosRegisteredString("do", gEidosID_do);
const std::string &gEidosStr_while = EidosRegisteredString("while", gEidosID_while);
const std::string &gEidosStr_for = EidosRegisteredString("for", gEidosID_for);
const std::string &gEidosStr_in = EidosRegisteredString("in", gEidosID_in);
const std::string &gEidosStr_next = EidosRegisteredString("next", gEidosID_next);
const std::string &gEidosStr_break = EidosRegisteredString("break", gEidosID_break);
const std::string &gEidosStr_return = EidosRegisteredString("return", gEidosID_return);
const std::string &gEidosStr_function = EidosRegisteredString("function", gEidosID_function);

// mostly Eidos global constants
const std::string &gEidosStr_T = EidosRegisteredString("T", gEidosID_T);
const std::string &gEidosStr_F = EidosRegisteredString("F", gEidosID_F);
const std::string &gEidosStr_NULL = EidosRegisteredString("NULL", gEidosID_NULL);
const std::string &gEidosStr_PI = EidosRegisteredString("PI", gEidosID_PI);
const std::string &gEidosStr_E = EidosRegisteredString("E", gEidosID_E);
const std::string &gEidosStr_INF = EidosRegisteredString("INF", gEidosID_INF);
const std::string &gEidosStr_MINUS_INF = EidosRegisteredString("-INF", gEidosID_MINUS_INF);
const std::string &gEidosStr_NAN = EidosRegisteredString("NAN", gEidosID_NAN);

// mostly Eidos type names
const std::string &gEidosStr_void = EidosRegisteredString("void", gEidosID_void);
const std::string &gEidosStr_logical = EidosRegisteredString("logical", gEidosID_logical);
const std::string &gEidosStr_string = EidosRegisteredString("string", gEidosID_string);
const std::string &gEidosStr_integer = EidosRegisteredString("integer", gEidosID_integer);
const std::string &gEidosStr_float = EidosRegisteredString("float", gEidosID_float);
const std::string &gEidosStr_object = EidosRegisteredString("object", gEidosID_object);
const std::string &gEidosStr_numeric = EidosRegisteredString("numeric", gEidosID_numeric);

// other miscellaneous strings
const std::string &gEidosStr_ELLIPSIS = EidosRegisteredString("...", gEidosID_ELLIPSIS);
const std::string &gEidosStr_type = EidosRegisteredString("type", gEidosID_type);
const std::string &gEidosStr_source = EidosRegisteredString("source", gEidosID_source);
const std::string &gEidosStr_GetPropertyOfElements = EidosRegisteredString("GetPropertyOfElements", gEidosID_GetPropertyOfElements);
const std::string &gEidosStr_ExecuteInstanceMethod = EidosRegisteredString("ExecuteInstanceMethod", gEidosID_ExecuteInstanceMethod);
const std::string &gEidosStr_undefined = EidosRegisteredString("undefined", gEidosID_undefined);
const std::string &gEidosStr_applyValue = EidosRegisteredString("applyValue", gEidosID_applyValue);

// strings for EidosObject
const std::string &gEidosStr_Object = EidosRegisteredString("Object", gEidosID_Object);
const std::string &gEidosStr_size = EidosRegisteredString("size", gEidosID_size);
const std::string &gEidosStr_length = EidosRegisteredString("length", gEidosID_length);
const std::string &gEidosStr_methodSignature = EidosRegisteredString("methodSignature", gEidosID_methodSignature);
const std::string &gEidosStr_propertySignature = EidosRegisteredString("propertySignature", gEidosID_propertySignature);
const std::string &gEidosStr_str = EidosRegisteredString("str", gEidosID_str);
const std::string &gEidosStr_stringRepresentation = EidosRegisteredString("stringRepresentation", gEidosID_stringRepresentation);

// strings for EidosTestElement
const std::string &gEidosStr__TestElement = EidosRegisteredString("_TestElement", gEidosID__TestElement);
const std::string &gEidosStr__TestElementNRR = EidosRegisteredString("_TestElementNRR", gEidosID__TestElementNRR);
const std::string &gEidosStr__yolk = EidosRegisteredString("_yolk", gEidosID__yolk);
const std::string &gEidosStr__increment = EidosRegisteredString("_increment", gEidosID__increment);
const std::string &gEidosStr__cubicYolk = EidosRegisteredString("_cubicYolk", gEidosID__cubicYolk);
const std::string &gEidosStr__squareTest = EidosRegisteredString("_squareTest", gEidosID__squareTest);

// strings for Dictionary (i.e., for EidosDictionaryUnretained, but also inherited by EidosDictionaryRetained)
const std::string &gEidosStr_DictionaryBase = EidosRegisteredString("DictionaryBase", gEidosID_DictionaryBase);
const std::string &gEidosStr_allKeys = EidosRegisteredString("allKeys", gEidosID_allKeys);
const std::string &gEidosStr_addKeysAndValuesFrom = EidosRegisteredString("addKeysAndValuesFrom", gEidosID_addKeysAndValuesFrom);
const std::string &gEidosStr_appendKeysAndValuesFrom = EidosRegisteredString("appendKeysAndValuesFrom", gEidosID_appendKeysAndValuesFrom);
const std::string &gEidosStr_clearKeysAndValues = EidosRegisteredString("clearKeysAndValues", gEidosID_clearKeysAndValues);
const std::string &gEidosStr_compactIndices = EidosRegisteredString("compactIndices", gEidosID_compactIndices);
const std::string &gEidosStr_getRowValues = EidosRegisteredString("getRowValues", gEidosID_getRowValues);
const std::string &gEidosStr_getValue = EidosRegisteredString("getValue", gEidosID_getValue);
const std::string &gEidosStr_identicalContents = EidosRegisteredString("identicalContents", gEidosID_identicalContents);
const std::string &gEidosStr_serialize = EidosRegisteredString("serialize", gEidosID_serialize);
const std::string &gEidosStr_setValue = EidosRegisteredString("setValue", gEidosID_setValue);

// strings for Dictionary (i.e., for EidosDictionaryRetained, which is the publicly visible class called "Dictionary" in Eidos)
const std::string &gEidosStr_Dictionary = EidosRegisteredString("Dictionary", gEidosID_Dictionary);

// strings for DataFrame
const std::string &gEidosStr_DataFrame = EidosRegisteredString("DataFrame", gEidosID_DataFrame);
const std::string &gEidosStr_colNames = EidosRegisteredString("colNames", gEidosID_colNames);
const std::string &gEidosStr_dim = EidosRegisteredString("dim", gEidosID_dim);
const std::string &gEidosStr_ncol = EidosRegisteredString("ncol", gEidosID_ncol);
const std::string &gEidosStr_nrow = EidosRegisteredString("nrow", gEidosID_nrow);
const std::string &gEidosStr_asMatrix = EidosRegisteredString("asMatrix", gEidosID_asMatrix);
const std::string &gEidosStr_cbind = EidosRegisteredString("cbind", gEidosID_cbind);
const std::string &gEidosStr_rbind = EidosRegisteredString("rbind", gEidosID_rbind);
const std::string &gEidosStr_subset = EidosRegisteredString("subset", gEidosID_subset);
const std::string &gEidosStr_subsetColumns = EidosRegisteredString("subsetColumns", gEidosID_subsetColumns);
const std::string &gEidosStr_subsetRows = EidosRegisteredString("subsetRows", gEidosID_subsetRows);

// strings for EidosImage
const std::string &gEidosStr_Image = EidosRegisteredString("Image", gEidosID_Image);
const std::string &gEidosStr_width = EidosRegisteredString("width", gEidosID_width);
const std::string &gEidosStr_height = EidosRegisteredString("height", gEidosID_height);
const std::string &gEidosStr_bitsPerChannel = EidosRegisteredString("bitsPerChannel", gEidosID_bitsPerChannel);
const std::string &gEidosStr_isGrayscale = EidosRegisteredString("isGrayscale", gEidosID_isGrayscale);
const std::string &gEidosStr_integerR = EidosRegisteredString("integerR", gEidosID_integerR);
const std::string &gEidosStr_integerG = EidosRegisteredString("integerG", gEidosID_integerG);
const std::string &gEidosStr_integerB = EidosRegisteredString("integerB", gEidosID_integerB);
const std::string &gEidosStr_integerK = EidosRegisteredString("integerK", gEidosID_integerK);
const std::string &gEidosStr_floatR = EidosRegisteredString("floatR", gEidosID_floatR);
const std::string &gEidosStr_floatG = EidosRegisteredString("floatG", gEidosID_floatG);
const std::string &gEidosStr_floatB = EidosRegisteredString("floatB", gEidosID_floatB);
const std::string &gEidosStr_floatK = EidosRegisteredString("floatK", gEidosID_floatK);
const std::string &gEidosStr_write = EidosRegisteredString("write", gEidosID_write);

// strings for parameters, function names, etc., that are needed as explicit registrations in a Context and thus have to be
// explicitly registered by Eidos; see the comment in EidosStringRegistry::RegisterStringForGlobalID() below
const std::string &gEidosStr_start = EidosRegisteredString("start", gEidosID_start);
const std::string &gEidosStr_end = EidosRegisteredString("end", gEidosID_end);
const std::string &gEidosStr_weights = EidosRegisteredString("weights", gEidosID_weights);
const std::string &gEidosStr_range = EidosRegisteredString("range", gEidosID_range);
const std::string &gEidosStr_c = EidosRegisteredString("c", gEidosID_c);
const std::string &gEidosStr_t = EidosRegisteredString("t", gEidosID_t);
const std::string &gEidosStr_n = EidosRegisteredString("n", gEidosID_n);
const std::string &gEidosStr_s = EidosRegisteredString("s", gEidosID_s);
const std::string &gEidosStr_x = EidosRegisteredString("x", gEidosID_x);
const std::string &gEidosStr_y = EidosRegisteredString("y", gEidosID_y);
const std::string &gEidosStr_z = EidosRegisteredString("z", gEidosID_z);
const std::string &gEidosStr_xy = EidosRegisteredString("xy", gEidosID_xy);
const std::string &gEidosStr_xz = EidosRegisteredString("xz", gEidosID_xz);
const std::string &gEidosStr_yz = EidosRegisteredString("yz", gEidosID_yz);
const std::string &gEidosStr_xyz = EidosRegisteredString("xyz", gEidosID_xyz);
const std::string &gEidosStr_color = EidosRegisteredString("color", gEidosID_color);
const std::string &gEidosStr_filePath = EidosRegisteredString("filePath", gEidosID_filePath);

const std::string &gEidosStr_Mutation = EidosRegisteredString("Mutation", gEidosID_Mutation);		// in Eidos for hack reasons; see EidosValue_Object::EidosValue_Object()
const std::string &gEidosStr_Genome = EidosRegisteredString("Genome", gEidosID_Genome);			// in Eidos for hack reasons; see EidosValue_Object::EidosValue_Object()
const std::string &gEidosStr_Individual = EidosRegisteredString("Individual", gEidosID_Individual);	// in Eidos for hack reasons; see EidosValue_Object::EidosValue_Object()

std::vector<std::string> gEidosConstantNames;


EidosStringRegistry::EidosStringRegistry(void) : gNextUnusedID(gEidosID_LastContextEntry)
{
}

EidosStringRegistry::~EidosStringRegistry(void)
{
	// The gIDToString map will not be safe to use, since we will have freed strings out from under it
	gIDToString.clear();
	
	// Now we free all of the registration objects that we made above in EidosRegisteredString().
	// The point of this thunk vector is to prevent Valgrind from being confused and thinking we
	// have leaked the global strings that we constructed; apparently unordered_map keeps them in
	// a way (unaligned?) that Valgrind does not recognize as a reference to the copies, so it
	// reports them as leaked even though they're not.
	for (auto gstr_iter : gIDToString_Thunk)
		delete (gstr_iter);
	
	gIDToString_Thunk.clear();

	// We also free all the std::strings we allocated in _GlobalStringIDForString() to avoid
	// them being reported as leaks.
	for (auto g_string : globalString_Thunk)
		delete (g_string);
	
	globalString_Thunk.clear();
}

void EidosStringRegistry::_RegisterStringForGlobalID(const std::string &p_string, EidosGlobalStringID p_string_id)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("EidosStringRegistry::_RegisterStringForGlobalID(): string registry change");
	
	// BCH 13 September 2016: So, this is a tricky issue without a good resolution at the moment.  Eidos explicitly registers
	// a few strings, using this method, using the function EidosRegisteredString().  And SLiM explicitly registers
	// a bunch more strings, in SLiM_RegisterGlobalStringsAndIDs().  So far so good.  But Eidos also registers a bunch of
	// strings "in passing", as a side effect of calling GlobalStringIDForString(), because it doesn't care what the
	// IDs of those strings are, it just wants them to be registered for later matching.  This happens to function names and
	// parameter names, in particular.  This is good; we don't want to have to explicitly enumerate and register all of those
	// strings, that would be a tremendous pain.  The problem is that these "in passing" registrations can conflict with
	// registrations done in the Context, unpredictably.  A new parameter named "weights" is added to a new Eidos function,
	// and suddenly the explicit registration of "weights" in SLiM has broken and needs to be removed.  At least you know
	// that that has happened, because you end up here.  When you end up here, don't just comment out the registration call
	// in the Context; you also need to add an explicit registration call in Eidos, and remove the string and ID definitions
	// in the Context, and so forth.  Migrate the whole explicit registration from the Context back into Eidos.  Unfortunate,
	// but I don't see any good solution.  Sure is nice how uniquing of selectors just happens automatically in Obj-C!  That
	// is basically what we're trying to duplicate here, without language support.
	if (gStringToID.find(p_string) != gStringToID.end())
		EIDOS_TERMINATION << "ERROR (EidosStringRegistry::_RegisterStringForGlobalID): string " << p_string << " has already been registered." << EidosTerminate(nullptr);
	
	if (gIDToString.find(p_string_id) != gIDToString.end())
		EIDOS_TERMINATION << "ERROR (EidosStringRegistry::_RegisterStringForGlobalID): id " << p_string_id << " has already been registered." << EidosTerminate(nullptr);
	
	if (p_string_id >= gEidosID_LastContextEntry)
		EIDOS_TERMINATION << "ERROR (EidosStringRegistry::_RegisterStringForGlobalID): id " << p_string_id << " is out of the legal range for preregistered strings." << EidosTerminate(nullptr);
	
	gStringToID[p_string] = p_string_id;
	gIDToString[p_string_id] = &p_string;
	
	//std::cout << "_RegisterStringForGlobalID(): added string " << p_string << ", id " << p_string_id << std::endl;
}

EidosGlobalStringID EidosStringRegistry::_GlobalStringIDForString(const std::string &p_string)
{
	//std::cerr << "EidosStringRegistry::_GlobalStringIDForString: " << p_string << std::endl;
	auto found_iter = gStringToID.find(p_string);
	
	if (found_iter == gStringToID.end())
	{
		// If the hash table does not already contain this key, then we add it to the table as a side effect.
		// We have to copy the string, because we have no idea what the caller might do with the string they passed us.
		// Since the hash table makes its own copy of the key, this copy is used only for the value in the hash tables.
		uint32_t string_id = gNextUnusedID++;
		
#if DEBUG
		// Check that this string ID has not already been used; this should never happen
		auto found_ID_iter = gIDToString.find(string_id);
		
		if (found_ID_iter != gIDToString.end())
			EIDOS_TERMINATION << "ERROR (EidosStringRegistry::_GlobalStringIDForString): id " << string_id << " was already in use; collision during in-passing registration of global string '" << p_string << "'." << EidosTerminate(nullptr);
#endif
		
		const std::string *copied_string = new const std::string(p_string);
		
		gStringToID[*copied_string] = string_id;	// makes another copy for the key
		gIDToString[string_id] = copied_string;		// uses the copy we made above
		
#if SLIM_LEAK_CHECKING
		// We add the string copies to a thunk object for later freeing, if we're leak-checking.
		// Normally all these copied strings live for the lifespan of the process.
		globalString_Thunk.emplace_back(copied_string);
#endif
		
		//std::cout << "_GlobalStringIDForString(): added string " << p_string << ", id " << string_id << std::endl;
		
		return string_id;
	}
	else
		return found_iter->second;
}

const std::string &EidosStringRegistry::_StringForGlobalStringID(EidosGlobalStringID p_string_id)
{
	//std::cerr << "EidosStringRegistry::_StringForGlobalStringID: " << p_string_id << std::endl;
	auto found_iter = gIDToString.find(p_string_id);
	
	if (found_iter == gIDToString.end())
		return gEidosStr_undefined;
	else
		return *(found_iter->second);
}

const std::string &EidosRegisteredString(const char *p_cstr, EidosGlobalStringID p_id)
{
	_EidosRegisteredString *registration_object = new _EidosRegisteredString(p_cstr, p_id);
	
#if SLIM_LEAK_CHECKING
	// We add registration objects to a thunk vector so we can free them at the end to un-confuse Valgrind;
	// see ~EidosStringRegistry().  Note that this thunk vector is not used by Eidos or SLiM, but the
	// registration objects are; they hold onto the std::string objects used by _RegisterStringForGlobalID().
	EidosStringRegistry::ThunkRegistration(registration_object);
#endif
	
	return registration_object->string_;
}


#pragma mark -
#pragma mark Named/specified color support
#pragma mark -

// *******************************************************************************************************************
//
//	Support for named / specified colors in Eidos
//

// Named colors.  This list is taken directly from R, used under their GPL-3.

EidosNamedColor gEidosNamedColors[] = {
	{"white", 255, 255, 255},
	{"aliceblue", 240, 248, 255},
	{"antiquewhite", 250, 235, 215},
	{"antiquewhite1", 255, 239, 219},
	{"antiquewhite2", 238, 223, 204},
	{"antiquewhite3", 205, 192, 176},
	{"antiquewhite4", 139, 131, 120},
	{"aquamarine", 127, 255, 212},
	{"aquamarine1", 127, 255, 212},
	{"aquamarine2", 118, 238, 198},
	{"aquamarine3", 102, 205, 170},
	{"aquamarine4", 69, 139, 116},
	{"azure", 240, 255, 255},
	{"azure1", 240, 255, 255},
	{"azure2", 224, 238, 238},
	{"azure3", 193, 205, 205},
	{"azure4", 131, 139, 139},
	{"beige", 245, 245, 220},
	{"bisque", 255, 228, 196},
	{"bisque1", 255, 228, 196},
	{"bisque2", 238, 213, 183},
	{"bisque3", 205, 183, 158},
	{"bisque4", 139, 125, 107},
	{"black", 0, 0, 0},
	{"blanchedalmond", 255, 235, 205},
	{"blue", 0, 0, 255},
	{"blue1", 0, 0, 255},
	{"blue2", 0, 0, 238},
	{"blue3", 0, 0, 205},
	{"blue4", 0, 0, 139},
	{"blueviolet", 138, 43, 226},
	{"brown", 165, 42, 42},
	{"brown1", 255, 64, 64},
	{"brown2", 238, 59, 59},
	{"brown3", 205, 51, 51},
	{"brown4", 139, 35, 35},
	{"burlywood", 222, 184, 135},
	{"burlywood1", 255, 211, 155},
	{"burlywood2", 238, 197, 145},
	{"burlywood3", 205, 170, 125},
	{"burlywood4", 139, 115, 85},
	{"cadetblue", 95, 158, 160},
	{"cadetblue1", 152, 245, 255},
	{"cadetblue2", 142, 229, 238},
	{"cadetblue3", 122, 197, 205},
	{"cadetblue4", 83, 134, 139},
	{"chartreuse", 127, 255, 0},
	{"chartreuse1", 127, 255, 0},
	{"chartreuse2", 118, 238, 0},
	{"chartreuse3", 102, 205, 0},
	{"chartreuse4", 69, 139, 0},
	{"chocolate", 210, 105, 30},
	{"chocolate1", 255, 127, 36},
	{"chocolate2", 238, 118, 33},
	{"chocolate3", 205, 102, 29},
	{"chocolate4", 139, 69, 19},
	{"coral", 255, 127, 80},
	{"coral1", 255, 114, 86},
	{"coral2", 238, 106, 80},
	{"coral3", 205, 91, 69},
	{"coral4", 139, 62, 47},
	{"cornflowerblue", 100, 149, 237},
	{"cornsilk", 255, 248, 220},
	{"cornsilk1", 255, 248, 220},
	{"cornsilk2", 238, 232, 205},
	{"cornsilk3", 205, 200, 177},
	{"cornsilk4", 139, 136, 120},
	{"cyan", 0, 255, 255},
	{"cyan1", 0, 255, 255},
	{"cyan2", 0, 238, 238},
	{"cyan3", 0, 205, 205},
	{"cyan4", 0, 139, 139},
	{"darkblue", 0, 0, 139},
	{"darkcyan", 0, 139, 139},
	{"darkgoldenrod", 184, 134, 11},
	{"darkgoldenrod1", 255, 185, 15},
	{"darkgoldenrod2", 238, 173, 14},
	{"darkgoldenrod3", 205, 149, 12},
	{"darkgoldenrod4", 139, 101, 8},
	{"darkgray", 169, 169, 169},
	{"darkgreen", 0, 100, 0},
	{"darkgrey", 169, 169, 169},
	{"darkkhaki", 189, 183, 107},
	{"darkmagenta", 139, 0, 139},
	{"darkolivegreen", 85, 107, 47},
	{"darkolivegreen1", 202, 255, 112},
	{"darkolivegreen2", 188, 238, 104},
	{"darkolivegreen3", 162, 205, 90},
	{"darkolivegreen4", 110, 139, 61},
	{"darkorange", 255, 140, 0},
	{"darkorange1", 255, 127, 0},
	{"darkorange2", 238, 118, 0},
	{"darkorange3", 205, 102, 0},
	{"darkorange4", 139, 69, 0},
	{"darkorchid", 153, 50, 204},
	{"darkorchid1", 191, 62, 255},
	{"darkorchid2", 178, 58, 238},
	{"darkorchid3", 154, 50, 205},
	{"darkorchid4", 104, 34, 139},
	{"darkred", 139, 0, 0},
	{"darksalmon", 233, 150, 122},
	{"darkseagreen", 143, 188, 143},
	{"darkseagreen1", 193, 255, 193},
	{"darkseagreen2", 180, 238, 180},
	{"darkseagreen3", 155, 205, 155},
	{"darkseagreen4", 105, 139, 105},
	{"darkslateblue", 72, 61, 139},
	{"darkslategray", 47, 79, 79},
	{"darkslategray1", 151, 255, 255},
	{"darkslategray2", 141, 238, 238},
	{"darkslategray3", 121, 205, 205},
	{"darkslategray4", 82, 139, 139},
	{"darkslategrey", 47, 79, 79},
	{"darkturquoise", 0, 206, 209},
	{"darkviolet", 148, 0, 211},
	{"deeppink", 255, 20, 147},
	{"deeppink1", 255, 20, 147},
	{"deeppink2", 238, 18, 137},
	{"deeppink3", 205, 16, 118},
	{"deeppink4", 139, 10, 80},
	{"deepskyblue", 0, 191, 255},
	{"deepskyblue1", 0, 191, 255},
	{"deepskyblue2", 0, 178, 238},
	{"deepskyblue3", 0, 154, 205},
	{"deepskyblue4", 0, 104, 139},
	{"dimgray", 105, 105, 105},
	{"dimgrey", 105, 105, 105},
	{"dodgerblue", 30, 144, 255},
	{"dodgerblue1", 30, 144, 255},
	{"dodgerblue2", 28, 134, 238},
	{"dodgerblue3", 24, 116, 205},
	{"dodgerblue4", 16, 78, 139},
	{"firebrick", 178, 34, 34},
	{"firebrick1", 255, 48, 48},
	{"firebrick2", 238, 44, 44},
	{"firebrick3", 205, 38, 38},
	{"firebrick4", 139, 26, 26},
	{"floralwhite", 255, 250, 240},
	{"forestgreen", 34, 139, 34},
	{"gainsboro", 220, 220, 220},
	{"ghostwhite", 248, 248, 255},
	{"gold", 255, 215, 0},
	{"gold1", 255, 215, 0},
	{"gold2", 238, 201, 0},
	{"gold3", 205, 173, 0},
	{"gold4", 139, 117, 0},
	{"goldenrod", 218, 165, 32},
	{"goldenrod1", 255, 193, 37},
	{"goldenrod2", 238, 180, 34},
	{"goldenrod3", 205, 155, 29},
	{"goldenrod4", 139, 105, 20},
	{"gray", 190, 190, 190},
	{"gray0", 0, 0, 0},
	{"gray1", 3, 3, 3},
	{"gray2", 5, 5, 5},
	{"gray3", 8, 8, 8},
	{"gray4", 10, 10, 10},
	{"gray5", 13, 13, 13},
	{"gray6", 15, 15, 15},
	{"gray7", 18, 18, 18},
	{"gray8", 20, 20, 20},
	{"gray9", 23, 23, 23},
	{"gray10", 26, 26, 26},
	{"gray11", 28, 28, 28},
	{"gray12", 31, 31, 31},
	{"gray13", 33, 33, 33},
	{"gray14", 36, 36, 36},
	{"gray15", 38, 38, 38},
	{"gray16", 41, 41, 41},
	{"gray17", 43, 43, 43},
	{"gray18", 46, 46, 46},
	{"gray19", 48, 48, 48},
	{"gray20", 51, 51, 51},
	{"gray21", 54, 54, 54},
	{"gray22", 56, 56, 56},
	{"gray23", 59, 59, 59},
	{"gray24", 61, 61, 61},
	{"gray25", 64, 64, 64},
	{"gray26", 66, 66, 66},
	{"gray27", 69, 69, 69},
	{"gray28", 71, 71, 71},
	{"gray29", 74, 74, 74},
	{"gray30", 77, 77, 77},
	{"gray31", 79, 79, 79},
	{"gray32", 82, 82, 82},
	{"gray33", 84, 84, 84},
	{"gray34", 87, 87, 87},
	{"gray35", 89, 89, 89},
	{"gray36", 92, 92, 92},
	{"gray37", 94, 94, 94},
	{"gray38", 97, 97, 97},
	{"gray39", 99, 99, 99},
	{"gray40", 102, 102, 102},
	{"gray41", 105, 105, 105},
	{"gray42", 107, 107, 107},
	{"gray43", 110, 110, 110},
	{"gray44", 112, 112, 112},
	{"gray45", 115, 115, 115},
	{"gray46", 117, 117, 117},
	{"gray47", 120, 120, 120},
	{"gray48", 122, 122, 122},
	{"gray49", 125, 125, 125},
	{"gray50", 127, 127, 127},
	{"gray51", 130, 130, 130},
	{"gray52", 133, 133, 133},
	{"gray53", 135, 135, 135},
	{"gray54", 138, 138, 138},
	{"gray55", 140, 140, 140},
	{"gray56", 143, 143, 143},
	{"gray57", 145, 145, 145},
	{"gray58", 148, 148, 148},
	{"gray59", 150, 150, 150},
	{"gray60", 153, 153, 153},
	{"gray61", 156, 156, 156},
	{"gray62", 158, 158, 158},
	{"gray63", 161, 161, 161},
	{"gray64", 163, 163, 163},
	{"gray65", 166, 166, 166},
	{"gray66", 168, 168, 168},
	{"gray67", 171, 171, 171},
	{"gray68", 173, 173, 173},
	{"gray69", 176, 176, 176},
	{"gray70", 179, 179, 179},
	{"gray71", 181, 181, 181},
	{"gray72", 184, 184, 184},
	{"gray73", 186, 186, 186},
	{"gray74", 189, 189, 189},
	{"gray75", 191, 191, 191},
	{"gray76", 194, 194, 194},
	{"gray77", 196, 196, 196},
	{"gray78", 199, 199, 199},
	{"gray79", 201, 201, 201},
	{"gray80", 204, 204, 204},
	{"gray81", 207, 207, 207},
	{"gray82", 209, 209, 209},
	{"gray83", 212, 212, 212},
	{"gray84", 214, 214, 214},
	{"gray85", 217, 217, 217},
	{"gray86", 219, 219, 219},
	{"gray87", 222, 222, 222},
	{"gray88", 224, 224, 224},
	{"gray89", 227, 227, 227},
	{"gray90", 229, 229, 229},
	{"gray91", 232, 232, 232},
	{"gray92", 235, 235, 235},
	{"gray93", 237, 237, 237},
	{"gray94", 240, 240, 240},
	{"gray95", 242, 242, 242},
	{"gray96", 245, 245, 245},
	{"gray97", 247, 247, 247},
	{"gray98", 250, 250, 250},
	{"gray99", 252, 252, 252},
	{"gray100", 255, 255, 255},
	{"green", 0, 255, 0},
	{"green1", 0, 255, 0},
	{"green2", 0, 238, 0},
	{"green3", 0, 205, 0},
	{"green4", 0, 139, 0},
	{"greenyellow", 173, 255, 47},
	{"grey", 190, 190, 190},
	{"grey0", 0, 0, 0},
	{"grey1", 3, 3, 3},
	{"grey2", 5, 5, 5},
	{"grey3", 8, 8, 8},
	{"grey4", 10, 10, 10},
	{"grey5", 13, 13, 13},
	{"grey6", 15, 15, 15},
	{"grey7", 18, 18, 18},
	{"grey8", 20, 20, 20},
	{"grey9", 23, 23, 23},
	{"grey10", 26, 26, 26},
	{"grey11", 28, 28, 28},
	{"grey12", 31, 31, 31},
	{"grey13", 33, 33, 33},
	{"grey14", 36, 36, 36},
	{"grey15", 38, 38, 38},
	{"grey16", 41, 41, 41},
	{"grey17", 43, 43, 43},
	{"grey18", 46, 46, 46},
	{"grey19", 48, 48, 48},
	{"grey20", 51, 51, 51},
	{"grey21", 54, 54, 54},
	{"grey22", 56, 56, 56},
	{"grey23", 59, 59, 59},
	{"grey24", 61, 61, 61},
	{"grey25", 64, 64, 64},
	{"grey26", 66, 66, 66},
	{"grey27", 69, 69, 69},
	{"grey28", 71, 71, 71},
	{"grey29", 74, 74, 74},
	{"grey30", 77, 77, 77},
	{"grey31", 79, 79, 79},
	{"grey32", 82, 82, 82},
	{"grey33", 84, 84, 84},
	{"grey34", 87, 87, 87},
	{"grey35", 89, 89, 89},
	{"grey36", 92, 92, 92},
	{"grey37", 94, 94, 94},
	{"grey38", 97, 97, 97},
	{"grey39", 99, 99, 99},
	{"grey40", 102, 102, 102},
	{"grey41", 105, 105, 105},
	{"grey42", 107, 107, 107},
	{"grey43", 110, 110, 110},
	{"grey44", 112, 112, 112},
	{"grey45", 115, 115, 115},
	{"grey46", 117, 117, 117},
	{"grey47", 120, 120, 120},
	{"grey48", 122, 122, 122},
	{"grey49", 125, 125, 125},
	{"grey50", 127, 127, 127},
	{"grey51", 130, 130, 130},
	{"grey52", 133, 133, 133},
	{"grey53", 135, 135, 135},
	{"grey54", 138, 138, 138},
	{"grey55", 140, 140, 140},
	{"grey56", 143, 143, 143},
	{"grey57", 145, 145, 145},
	{"grey58", 148, 148, 148},
	{"grey59", 150, 150, 150},
	{"grey60", 153, 153, 153},
	{"grey61", 156, 156, 156},
	{"grey62", 158, 158, 158},
	{"grey63", 161, 161, 161},
	{"grey64", 163, 163, 163},
	{"grey65", 166, 166, 166},
	{"grey66", 168, 168, 168},
	{"grey67", 171, 171, 171},
	{"grey68", 173, 173, 173},
	{"grey69", 176, 176, 176},
	{"grey70", 179, 179, 179},
	{"grey71", 181, 181, 181},
	{"grey72", 184, 184, 184},
	{"grey73", 186, 186, 186},
	{"grey74", 189, 189, 189},
	{"grey75", 191, 191, 191},
	{"grey76", 194, 194, 194},
	{"grey77", 196, 196, 196},
	{"grey78", 199, 199, 199},
	{"grey79", 201, 201, 201},
	{"grey80", 204, 204, 204},
	{"grey81", 207, 207, 207},
	{"grey82", 209, 209, 209},
	{"grey83", 212, 212, 212},
	{"grey84", 214, 214, 214},
	{"grey85", 217, 217, 217},
	{"grey86", 219, 219, 219},
	{"grey87", 222, 222, 222},
	{"grey88", 224, 224, 224},
	{"grey89", 227, 227, 227},
	{"grey90", 229, 229, 229},
	{"grey91", 232, 232, 232},
	{"grey92", 235, 235, 235},
	{"grey93", 237, 237, 237},
	{"grey94", 240, 240, 240},
	{"grey95", 242, 242, 242},
	{"grey96", 245, 245, 245},
	{"grey97", 247, 247, 247},
	{"grey98", 250, 250, 250},
	{"grey99", 252, 252, 252},
	{"grey100", 255, 255, 255},
	{"honeydew", 240, 255, 240},
	{"honeydew1", 240, 255, 240},
	{"honeydew2", 224, 238, 224},
	{"honeydew3", 193, 205, 193},
	{"honeydew4", 131, 139, 131},
	{"hotpink", 255, 105, 180},
	{"hotpink1", 255, 110, 180},
	{"hotpink2", 238, 106, 167},
	{"hotpink3", 205, 96, 144},
	{"hotpink4", 139, 58, 98},
	{"indianred", 205, 92, 92},
	{"indianred1", 255, 106, 106},
	{"indianred2", 238, 99, 99},
	{"indianred3", 205, 85, 85},
	{"indianred4", 139, 58, 58},
	{"ivory", 255, 255, 240},
	{"ivory1", 255, 255, 240},
	{"ivory2", 238, 238, 224},
	{"ivory3", 205, 205, 193},
	{"ivory4", 139, 139, 131},
	{"khaki", 240, 230, 140},
	{"khaki1", 255, 246, 143},
	{"khaki2", 238, 230, 133},
	{"khaki3", 205, 198, 115},
	{"khaki4", 139, 134, 78},
	{"lavender", 230, 230, 250},
	{"lavenderblush", 255, 240, 245},
	{"lavenderblush1", 255, 240, 245},
	{"lavenderblush2", 238, 224, 229},
	{"lavenderblush3", 205, 193, 197},
	{"lavenderblush4", 139, 131, 134},
	{"lawngreen", 124, 252, 0},
	{"lemonchiffon", 255, 250, 205},
	{"lemonchiffon1", 255, 250, 205},
	{"lemonchiffon2", 238, 233, 191},
	{"lemonchiffon3", 205, 201, 165},
	{"lemonchiffon4", 139, 137, 112},
	{"lightblue", 173, 216, 230},
	{"lightblue1", 191, 239, 255},
	{"lightblue2", 178, 223, 238},
	{"lightblue3", 154, 192, 205},
	{"lightblue4", 104, 131, 139},
	{"lightcoral", 240, 128, 128},
	{"lightcyan", 224, 255, 255},
	{"lightcyan1", 224, 255, 255},
	{"lightcyan2", 209, 238, 238},
	{"lightcyan3", 180, 205, 205},
	{"lightcyan4", 122, 139, 139},
	{"lightgoldenrod", 238, 221, 130},
	{"lightgoldenrod1", 255, 236, 139},
	{"lightgoldenrod2", 238, 220, 130},
	{"lightgoldenrod3", 205, 190, 112},
	{"lightgoldenrod4", 139, 129, 76},
	{"lightgoldenrodyellow", 250, 250, 210},
	{"lightgray", 211, 211, 211},
	{"lightgreen", 144, 238, 144},
	{"lightgrey", 211, 211, 211},
	{"lightpink", 255, 182, 193},
	{"lightpink1", 255, 174, 185},
	{"lightpink2", 238, 162, 173},
	{"lightpink3", 205, 140, 149},
	{"lightpink4", 139, 95, 101},
	{"lightsalmon", 255, 160, 122},
	{"lightsalmon1", 255, 160, 122},
	{"lightsalmon2", 238, 149, 114},
	{"lightsalmon3", 205, 129, 98},
	{"lightsalmon4", 139, 87, 66},
	{"lightseagreen", 32, 178, 170},
	{"lightskyblue", 135, 206, 250},
	{"lightskyblue1", 176, 226, 255},
	{"lightskyblue2", 164, 211, 238},
	{"lightskyblue3", 141, 182, 205},
	{"lightskyblue4", 96, 123, 139},
	{"lightslateblue", 132, 112, 255},
	{"lightslategray", 119, 136, 153},
	{"lightslategrey", 119, 136, 153},
	{"lightsteelblue", 176, 196, 222},
	{"lightsteelblue1", 202, 225, 255},
	{"lightsteelblue2", 188, 210, 238},
	{"lightsteelblue3", 162, 181, 205},
	{"lightsteelblue4", 110, 123, 139},
	{"lightyellow", 255, 255, 224},
	{"lightyellow1", 255, 255, 224},
	{"lightyellow2", 238, 238, 209},
	{"lightyellow3", 205, 205, 180},
	{"lightyellow4", 139, 139, 122},
	{"limegreen", 50, 205, 50},
	{"linen", 250, 240, 230},
	{"magenta", 255, 0, 255},
	{"magenta1", 255, 0, 255},
	{"magenta2", 238, 0, 238},
	{"magenta3", 205, 0, 205},
	{"magenta4", 139, 0, 139},
	{"maroon", 176, 48, 96},
	{"maroon1", 255, 52, 179},
	{"maroon2", 238, 48, 167},
	{"maroon3", 205, 41, 144},
	{"maroon4", 139, 28, 98},
	{"mediumaquamarine", 102, 205, 170},
	{"mediumblue", 0, 0, 205},
	{"mediumorchid", 186, 85, 211},
	{"mediumorchid1", 224, 102, 255},
	{"mediumorchid2", 209, 95, 238},
	{"mediumorchid3", 180, 82, 205},
	{"mediumorchid4", 122, 55, 139},
	{"mediumpurple", 147, 112, 219},
	{"mediumpurple1", 171, 130, 255},
	{"mediumpurple2", 159, 121, 238},
	{"mediumpurple3", 137, 104, 205},
	{"mediumpurple4", 93, 71, 139},
	{"mediumseagreen", 60, 179, 113},
	{"mediumslateblue", 123, 104, 238},
	{"mediumspringgreen", 0, 250, 154},
	{"mediumturquoise", 72, 209, 204},
	{"mediumvioletred", 199, 21, 133},
	{"midnightblue", 25, 25, 112},
	{"mintcream", 245, 255, 250},
	{"mistyrose", 255, 228, 225},
	{"mistyrose1", 255, 228, 225},
	{"mistyrose2", 238, 213, 210},
	{"mistyrose3", 205, 183, 181},
	{"mistyrose4", 139, 125, 123},
	{"moccasin", 255, 228, 181},
	{"navajowhite", 255, 222, 173},
	{"navajowhite1", 255, 222, 173},
	{"navajowhite2", 238, 207, 161},
	{"navajowhite3", 205, 179, 139},
	{"navajowhite4", 139, 121, 94},
	{"navy", 0, 0, 128},
	{"navyblue", 0, 0, 128},
	{"oldlace", 253, 245, 230},
	{"olivedrab", 107, 142, 35},
	{"olivedrab1", 192, 255, 62},
	{"olivedrab2", 179, 238, 58},
	{"olivedrab3", 154, 205, 50},
	{"olivedrab4", 105, 139, 34},
	{"orange", 255, 165, 0},
	{"orange1", 255, 165, 0},
	{"orange2", 238, 154, 0},
	{"orange3", 205, 133, 0},
	{"orange4", 139, 90, 0},
	{"orangered", 255, 69, 0},
	{"orangered1", 255, 69, 0},
	{"orangered2", 238, 64, 0},
	{"orangered3", 205, 55, 0},
	{"orangered4", 139, 37, 0},
	{"orchid", 218, 112, 214},
	{"orchid1", 255, 131, 250},
	{"orchid2", 238, 122, 233},
	{"orchid3", 205, 105, 201},
	{"orchid4", 139, 71, 137},
	{"palegoldenrod", 238, 232, 170},
	{"palegreen", 152, 251, 152},
	{"palegreen1", 154, 255, 154},
	{"palegreen2", 144, 238, 144},
	{"palegreen3", 124, 205, 124},
	{"palegreen4", 84, 139, 84},
	{"paleturquoise", 175, 238, 238},
	{"paleturquoise1", 187, 255, 255},
	{"paleturquoise2", 174, 238, 238},
	{"paleturquoise3", 150, 205, 205},
	{"paleturquoise4", 102, 139, 139},
	{"palevioletred", 219, 112, 147},
	{"palevioletred1", 255, 130, 171},
	{"palevioletred2", 238, 121, 159},
	{"palevioletred3", 205, 104, 137},
	{"palevioletred4", 139, 71, 93},
	{"papayawhip", 255, 239, 213},
	{"peachpuff", 255, 218, 185},
	{"peachpuff1", 255, 218, 185},
	{"peachpuff2", 238, 203, 173},
	{"peachpuff3", 205, 175, 149},
	{"peachpuff4", 139, 119, 101},
	{"peru", 205, 133, 63},
	{"pink", 255, 192, 203},
	{"pink1", 255, 181, 197},
	{"pink2", 238, 169, 184},
	{"pink3", 205, 145, 158},
	{"pink4", 139, 99, 108},
	{"plum", 221, 160, 221},
	{"plum1", 255, 187, 255},
	{"plum2", 238, 174, 238},
	{"plum3", 205, 150, 205},
	{"plum4", 139, 102, 139},
	{"powderblue", 176, 224, 230},
	{"purple", 160, 32, 240},
	{"purple1", 155, 48, 255},
	{"purple2", 145, 44, 238},
	{"purple3", 125, 38, 205},
	{"purple4", 85, 26, 139},
	{"red", 255, 0, 0},
	{"red1", 255, 0, 0},
	{"red2", 238, 0, 0},
	{"red3", 205, 0, 0},
	{"red4", 139, 0, 0},
	{"rosybrown", 188, 143, 143},
	{"rosybrown1", 255, 193, 193},
	{"rosybrown2", 238, 180, 180},
	{"rosybrown3", 205, 155, 155},
	{"rosybrown4", 139, 105, 105},
	{"royalblue", 65, 105, 225},
	{"royalblue1", 72, 118, 255},
	{"royalblue2", 67, 110, 238},
	{"royalblue3", 58, 95, 205},
	{"royalblue4", 39, 64, 139},
	{"saddlebrown", 139, 69, 19},
	{"salmon", 250, 128, 114},
	{"salmon1", 255, 140, 105},
	{"salmon2", 238, 130, 98},
	{"salmon3", 205, 112, 84},
	{"salmon4", 139, 76, 57},
	{"sandybrown", 244, 164, 96},
	{"seagreen", 46, 139, 87},
	{"seagreen1", 84, 255, 159},
	{"seagreen2", 78, 238, 148},
	{"seagreen3", 67, 205, 128},
	{"seagreen4", 46, 139, 87},
	{"seashell", 255, 245, 238},
	{"seashell1", 255, 245, 238},
	{"seashell2", 238, 229, 222},
	{"seashell3", 205, 197, 191},
	{"seashell4", 139, 134, 130},
	{"sienna", 160, 82, 45},
	{"sienna1", 255, 130, 71},
	{"sienna2", 238, 121, 66},
	{"sienna3", 205, 104, 57},
	{"sienna4", 139, 71, 38},
	{"skyblue", 135, 206, 235},
	{"skyblue1", 135, 206, 255},
	{"skyblue2", 126, 192, 238},
	{"skyblue3", 108, 166, 205},
	{"skyblue4", 74, 112, 139},
	{"slateblue", 106, 90, 205},
	{"slateblue1", 131, 111, 255},
	{"slateblue2", 122, 103, 238},
	{"slateblue3", 105, 89, 205},
	{"slateblue4", 71, 60, 139},
	{"slategray", 112, 128, 144},
	{"slategray1", 198, 226, 255},
	{"slategray2", 185, 211, 238},
	{"slategray3", 159, 182, 205},
	{"slategray4", 108, 123, 139},
	{"slategrey", 112, 128, 144},
	{"snow", 255, 250, 250},
	{"snow1", 255, 250, 250},
	{"snow2", 238, 233, 233},
	{"snow3", 205, 201, 201},
	{"snow4", 139, 137, 137},
	{"springgreen", 0, 255, 127},
	{"springgreen1", 0, 255, 127},
	{"springgreen2", 0, 238, 118},
	{"springgreen3", 0, 205, 102},
	{"springgreen4", 0, 139, 69},
	{"steelblue", 70, 130, 180},
	{"steelblue1", 99, 184, 255},
	{"steelblue2", 92, 172, 238},
	{"steelblue3", 79, 148, 205},
	{"steelblue4", 54, 100, 139},
	{"tan", 210, 180, 140},
	{"tan1", 255, 165, 79},
	{"tan2", 238, 154, 73},
	{"tan3", 205, 133, 63},
	{"tan4", 139, 90, 43},
	{"thistle", 216, 191, 216},
	{"thistle1", 255, 225, 255},
	{"thistle2", 238, 210, 238},
	{"thistle3", 205, 181, 205},
	{"thistle4", 139, 123, 139},
	{"tomato", 255, 99, 71},
	{"tomato1", 255, 99, 71},
	{"tomato2", 238, 92, 66},
	{"tomato3", 205, 79, 57},
	{"tomato4", 139, 54, 38},
	{"turquoise", 64, 224, 208},
	{"turquoise1", 0, 245, 255},
	{"turquoise2", 0, 229, 238},
	{"turquoise3", 0, 197, 205},
	{"turquoise4", 0, 134, 139},
	{"violet", 238, 130, 238},
	{"violetred", 208, 32, 144},
	{"violetred1", 255, 62, 150},
	{"violetred2", 238, 58, 140},
	{"violetred3", 205, 50, 120},
	{"violetred4", 139, 34, 82},
	{"wheat", 245, 222, 179},
	{"wheat1", 255, 231, 186},
	{"wheat2", 238, 216, 174},
	{"wheat3", 205, 186, 150},
	{"wheat4", 139, 126, 102},
	{"whitesmoke", 245, 245, 245},
	{"yellow", 255, 255, 0},
	{"yellow1", 255, 255, 0},
	{"yellow2", 238, 238, 0},
	{"yellow3", 205, 205, 0},
	{"yellow4", 139, 139, 0},
	{"yellowgreen", 154, 205, 50},
	{nullptr, 0, 0, 0}
};

void Eidos_GetColorComponents(const std::string &p_color_name, float *p_red_component, float *p_green_component, float *p_blue_component)
{
	// Colors can be specified either in hex as "#RRGGBB" or as a named color from the list above
	if ((p_color_name.length() == 7) && (p_color_name[0] == '#'))
	{
		try
		{
			unsigned long r = stoul(p_color_name.substr(1, 2), nullptr, 16);
			unsigned long g = stoul(p_color_name.substr(3, 2), nullptr, 16);
			unsigned long b = stoul(p_color_name.substr(5, 2), nullptr, 16);
			
			*p_red_component = r / 255.0F;
			*p_green_component = g / 255.0F;
			*p_blue_component = b / 255.0F;
			return;
		}
		catch (...)
		{
			EIDOS_TERMINATION << "ERROR (Eidos_GetColorComponents): color specification '" << p_color_name << "' is malformed." << EidosTerminate();
		}
	}
	else
	{
		for (EidosNamedColor *color_table = gEidosNamedColors; color_table->name; ++color_table)
		{
			if (p_color_name == color_table->name)
			{
				*p_red_component = color_table->red / 255.0F;
				*p_green_component = color_table->green / 255.0F;
				*p_blue_component = color_table->blue / 255.0F;
				return;
			}
		}
	}
	
	EIDOS_TERMINATION << "ERROR (Eidos_GetColorComponents): color named '" << p_color_name << "' could not be found." << EidosTerminate();
}

void Eidos_GetColorComponents(const std::string &p_color_name, uint8_t *p_red_component, uint8_t *p_green_component, uint8_t *p_blue_component)
{
	// Colors can be specified either in hex as "#RRGGBB" or as a named color from the list above
	if ((p_color_name.length() == 7) && (p_color_name[0] == '#'))
	{
		try
		{
			unsigned long r = stoul(p_color_name.substr(1, 2), nullptr, 16);
			unsigned long g = stoul(p_color_name.substr(3, 2), nullptr, 16);
			unsigned long b = stoul(p_color_name.substr(5, 2), nullptr, 16);
			
			*p_red_component = (uint8_t)r;
			*p_green_component = (uint8_t)g;
			*p_blue_component = (uint8_t)b;
			return;
		}
		catch (...)
		{
			EIDOS_TERMINATION << "ERROR (Eidos_GetColorComponents): color specification '" << p_color_name << "' is malformed." << EidosTerminate();
		}
	}
	else
	{
		for (EidosNamedColor *color_table = gEidosNamedColors; color_table->name; ++color_table)
		{
			if (p_color_name == color_table->name)
			{
				*p_red_component = color_table->red;
				*p_green_component = color_table->green;
				*p_blue_component = color_table->blue;
				return;
			}
		}
	}
	
	EIDOS_TERMINATION << "ERROR (Eidos_GetColorComponents): color named '" << p_color_name << "' could not be found." << EidosTerminate();
}

void Eidos_GetColorString(double p_red, double p_green, double p_blue, char *p_string_buffer)
{
	if (std::isnan(p_red) || std::isnan(p_green) || std::isnan(p_blue))
		EIDOS_TERMINATION << "ERROR (Eidos_GetColorString): color component with value NAN is not legal." << EidosTerminate();
	
	if (p_red < 0.0) p_red = 0.0;
	if (p_red > 1.0) p_red = 1.0;
	if (p_green < 0.0) p_green = 0.0;
	if (p_green > 1.0) p_green = 1.0;
	if (p_blue < 0.0) p_blue = 0.0;
	if (p_blue > 1.0) p_blue = 1.0;
	
	int r_i = (int)round(p_red * 255.0);
	int g_i = (int)round(p_green * 255.0);
	int b_i = (int)round(p_blue * 255.0);
	
	static char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
	
	p_string_buffer[0] = '#';
	p_string_buffer[1] = hex[r_i / 16];
	p_string_buffer[2] = hex[r_i % 16];
	p_string_buffer[3] = hex[g_i / 16];
	p_string_buffer[4] = hex[g_i % 16];
	p_string_buffer[5] = hex[b_i / 16];
	p_string_buffer[6] = hex[b_i % 16];
	p_string_buffer[7] = 0;
}

void Eidos_GetColorString(uint8_t p_red, uint8_t p_green, uint8_t p_blue, char *p_string_buffer)
{
	int r_i = (int)p_red;
	int g_i = (int)p_green;
	int b_i = (int)p_blue;
	
	static char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
	
	p_string_buffer[0] = '#';
	p_string_buffer[1] = hex[r_i / 16];
	p_string_buffer[2] = hex[r_i % 16];
	p_string_buffer[3] = hex[g_i / 16];
	p_string_buffer[4] = hex[g_i % 16];
	p_string_buffer[5] = hex[b_i / 16];
	p_string_buffer[6] = hex[b_i % 16];
	p_string_buffer[7] = 0;
}

void Eidos_HSV2RGB(double h, double s, double v, double *p_r, double *p_g, double *p_b)
{
	if (std::isnan(h) || std::isnan(s) || std::isnan(v))
		EIDOS_TERMINATION << "ERROR (Eidos_HSV2RGB): color component with value NAN is not legal." << EidosTerminate();
	
	if (h < 0.0) h = 0.0;
	if (h > 1.0) h = 1.0;
	if (s < 0.0) s = 0.0;
	if (s > 1.0) s = 1.0;
	if (v < 0.0) v = 0.0;
	if (v > 1.0) v = 1.0;
	
	double c = v * s;
	double x = c * (1.0 - fabs(fmod(h * 6.0, 2.0) - 1.0));
	double m = v - c;
	double r, g, b;
	
	if (h < 1.0 / 6.0)			{	r = c;	g = x;	b = 0;	}
	else if (h < 2.0 / 6.0)		{	r = x;	g = c;	b = 0;	}
	else if (h < 3.0 / 6.0)		{	r = 0;	g = c;	b = x;	}
	else if (h < 4.0 / 6.0)		{	r = 0;	g = x;	b = c;	}
	else if (h < 5.0 / 6.0)		{	r = x;	g = 0;	b = c;	}
	else						{	r = c;	g = 0;	b = x;	}
	
	*p_r = r + m;
	*p_g = g + m;
	*p_b = b + m;
}

void Eidos_RGB2HSV(double r, double g, double b, double *p_h, double *p_s, double *p_v)
{
	if (std::isnan(r) || std::isnan(g) || std::isnan(b))
		EIDOS_TERMINATION << "ERROR (Eidos_RGB2HSV): color component with value NAN is not legal." << EidosTerminate();
	
	if (r < 0.0) r = 0.0;
	if (r > 1.0) r = 1.0;
	if (g < 0.0) g = 0.0;
	if (g > 1.0) g = 1.0;
	if (b < 0.0) b = 0.0;
	if (b > 1.0) b = 1.0;
	
	double c_max = std::max(r, std::max(g, b));
	double c_min = std::min(r, std::min(g, b));
	double delta = c_max - c_min;
	double h, s, v;
	
	if (delta == 0)
		h = 0.0;
	else if (c_max == r)
		h = (1.0/6.0) * fmod(((g - b) / delta) + 6.0, 6.0);
	else if (c_max == g)
		h = (1.0/6.0) * (((b - r) / delta) + 2.0);
	else // if (c_max == b)
		h = (1.0/6.0) * (((r - g) / delta) + 4.0);
	
	if (c_max == 0.0)
		s = 0.0;
	else
		s = delta / c_max;
	
	v = c_max;
	
	*p_h = h;
	*p_s = s;
	*p_v = v;
}

void Eidos_ColorPaletteLookup(double fraction, EidosColorPalette palette, double &r, double &g, double &b)
{
	if (fraction < 0.0) fraction = 0.0;
	if (fraction > 1.0) fraction = 1.0;
	
	switch (palette)
	{
		case EidosColorPalette::kPalette_cm:
		{
			// Note that for even n, this generates somewhat different values than R does, but I think
			// that is a bug in their code; the space between the two central values is doubled.
			r = (fraction >= 0.5) ? 1.0 : (fraction + 0.5);
			g = (fraction <= 0.5) ? 1.0 : (1.5 - fraction);
			b = 1.0;
			break;
		}
		case EidosColorPalette::kPalette_heat:
		{
			// Note the behavior of this palette was changed slightly in Eidos 1.5, to be more consistent
			if (fraction < 0.75)
			{
				r = 1.0;
				g = fraction / 0.75;
				b = 0.0;
			}
			else
			{
				r = 1.0;
				g = 1.0;
				b = (fraction - 0.75) / 0.25;
			}
			break;
		}
		case EidosColorPalette::kPalette_terrain:
		{
			// Note the behavior of this palette was changed slightly in Eidos 1.5, to be more consistent
			if (fraction < 0.5)
			{
				double w = fraction / 0.5;
				double h = 4/12.0 + (2/12.0 - 4/12.0) * w;
				double s = 1.0 + (1.0 - 1.0) * w;
				double v = 0.65 + (0.9 - 0.65) * w;
				
				Eidos_HSV2RGB(h, s, v, &r, &g, &b);
			}
			else
			{
				double w = (fraction - 0.5) / 0.5;
				double h = 2/12.0 + (0/12.0 - 2/12.0) * w;
				double s = 1.0 + (0.0 - 1.0) * w;
				double v = 0.9 + (0.95 - 0.9) * w;
				
				Eidos_HSV2RGB(h, s, v, &r, &g, &b);
			}
			break;
		}
		case EidosColorPalette::kPalette_parula:
		{
			tinycolormap::Color color = tinycolormap::GetParulaColor(fraction);
			r = color.r(); g = color.g(); b = color.b();
			break;
		}
		case EidosColorPalette::kPalette_hot:
		{
			tinycolormap::Color color = tinycolormap::GetHotColor(fraction);
			r = color.r(); g = color.g(); b = color.b();
			break;
		}
		case EidosColorPalette::kPalette_jet:
		{
			tinycolormap::Color color = tinycolormap::GetJetColor(fraction);
			r = color.r(); g = color.g(); b = color.b();
			break;
		}
		case EidosColorPalette::kPalette_turbo:
		{
			tinycolormap::Color color = tinycolormap::GetTurboColor(fraction);
			r = color.r(); g = color.g(); b = color.b();
			break;
		}
		case EidosColorPalette::kPalette_gray:
		{
			tinycolormap::Color color = tinycolormap::GetGrayColor(fraction);
			r = color.r(); g = color.g(); b = color.b();
			break;
		}
		case EidosColorPalette::kPalette_magma:
		{
			tinycolormap::Color color = tinycolormap::GetMagmaColor(fraction);
			r = color.r(); g = color.g(); b = color.b();
			break;
		}
		case EidosColorPalette::kPalette_inferno:
		{
			tinycolormap::Color color = tinycolormap::GetInfernoColor(fraction);
			r = color.r(); g = color.g(); b = color.b();
			break;
		}
		case EidosColorPalette::kPalette_plasma:
		{
			tinycolormap::Color color = tinycolormap::GetPlasmaColor(fraction);
			r = color.r(); g = color.g(); b = color.b();
			break;
		}
		case EidosColorPalette::kPalette_viridis:
		{
			tinycolormap::Color color = tinycolormap::GetViridisColor(fraction);
			r = color.r(); g = color.g(); b = color.b();
			break;
		}
		case EidosColorPalette::kPalette_cividis:
		{
			tinycolormap::Color color = tinycolormap::GetCividisColor(fraction);
			r = color.r(); g = color.g(); b = color.b();
			break;
		}
	}
}
















































