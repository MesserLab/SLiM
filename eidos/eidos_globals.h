//
//  eidos_globals.h
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


#ifndef __Eidos__eidos_globals__
#define __Eidos__eidos_globals__

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <float.h>
#include <string.h>
#include <numeric>
#include <algorithm>
#include <unordered_map>
#include <cstdint>

#if defined(__APPLE__) && defined(__MACH__)
// On macOS we use mach_absolute_time() for profiling and benchmarking
#include <mach/mach_time.h>
#define MACH_PROFILING
#else
// On other platforms we use std::chrono::steady_clock
#include <chrono>
#define CHRONO_PROFILING
#endif

#include "eidos_openmp.h"
#include "eidos_intrusive_ptr.h"

class EidosScript;
class EidosToken;


// Eidos version: See also Info.plist
#define EIDOS_VERSION_STRING	("3.1")
#define EIDOS_VERSION_FLOAT		(3.1)


#ifdef _OPENMP
typedef enum {
	kDefault = 0,				// indicates that one of the other values should be chosen heuristically
	kMaxThreads,				// use EIDOS_OMP_MAX_THREADS for everything
	kMacStudio2022_16,			// Mac Studio 2022 (Mac13,2), 20-core M1 Ultra (16 performance cores)
	kXeonGold2_40,				// two 20-core (40-hyperthreaded) Intel Xeon Gold 6148 2.4GHz (40 physical cores)
} EidosPerTaskThreadCounts;

// Some state variables for user output regarding the OpenMP configuration
extern EidosPerTaskThreadCounts gEidosDefaultPerTaskThreadCounts;	// the default set on the command line, or kDefault
extern std::string gEidosPerTaskThreadCountsSetName;
extern int gEidosPerTaskOriginalMaxThreadCount, gEidosPerTaskClippedMaxThreadCount;

// Eidos_WarmUpOpenMP() should be called once at startup to give Eidos an opportunity to initialize static state
void _Eidos_SetOpenMPThreadCounts(EidosPerTaskThreadCounts per_task_thread_counts);
void _Eidos_ChooseDefaultOpenMPThreadCounts(void);
void _Eidos_ClipOpenMPThreadCounts(void);
void Eidos_WarmUpOpenMP(std::ostream *outstream, bool changed_max_thread_count, int new_max_thread_count, bool active_threads, std::string thread_count_set_name);
#endif

void Eidos_WarmUp(void);

// This can be called at startup, after Eidos_WarmUp(), to define global constants from the command line
void Eidos_DefineConstantsFromCommandLine(const std::vector<std::string> &p_constants);


// This governs whether "Robin Hood Hashing" is used instead of std::unordered_map in key spots, for speed
// Robin Hood Hashing is in robin_hood.h, and is by Martin Ankerl (https://github.com/martinus/robin-hood-hashing)
// Change this define to 1 to enable Robin Hood Hashing, or change it to 0 to disable it
// Note that you have a choice of robin_hood::unordered_node_map or robin_hood::unordered_flat_map
// With robin_hood::unordered_flat_map, references to elements are not stable, but it is probably a bit faster
// STD_UNORDERED_MAP_HASHING is the reverse flag; this just makes it easy to get an error message if this header
// is not included, following a standard usage pattern, since then neither of these defines will exist.
#define EIDOS_ROBIN_HOOD_HASHING	1

#if EIDOS_ROBIN_HOOD_HASHING
#define STD_UNORDERED_MAP_HASHING	0
#else
#define STD_UNORDERED_MAP_HASHING	1
#endif


// *******************************************************************************************************************
//
//	Context customization
//
#pragma mark -
#pragma mark Context customization
#pragma mark -

// Information on the Context within which Eidos is running (if any).  This is basically a way to let the Context
// customize the version and license and citation information printed by Eidos.

extern double gEidosContextVersion;
extern std::string gEidosContextVersionString;
extern std::string gEidosContextLicense;
extern std::string gEidosContextCitation;


// *******************************************************************************************************************
//
//	Error tracking
//
#pragma mark -
#pragma mark Error tracking
#pragma mark -

// The part of the input file that caused an error; used to highlight the token or text that caused the error.
// Eidos now also supports reporting of errors with quoted script lines, using the EidosScript* here.  The
// error tracking and reporting stuff is unfortunately very fragile, because it is based on global variables
// that get magically set up in various places and then get used in various completely different places.  This
// is a big reason why Eidos is not thread-safe at present, and it's one of the trickiest parts of the code,
// for no very good reason except that I haven't yet figured out the right way to fix it.  FIXME

// a small struct used for saving and restoring the error position in a stack-like manner
typedef struct
{
	int characterStartOfError;
	int characterEndOfError;
	int characterStartOfErrorUTF16;
	int characterEndOfErrorUTF16;
} EidosErrorPosition;

// a bigger struct used when saving and restoring not only the error position but also the script context
typedef struct {
	EidosErrorPosition errorPosition;
	EidosScript *currentScript;
	bool executingRuntimeScript;
} EidosErrorContext;

// this global struct contains the current error context
extern EidosErrorContext gEidosErrorContext;

// declared in eidos_token.h due to EidosToken dependency:
// inline __attribute__((always_inline)) EidosErrorPosition PushErrorPositionFromToken(const EidosToken *p_naughty_token_)

inline __attribute__((always_inline)) void RestoreErrorPosition(EidosErrorPosition &p_saved_position)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("RestoreErrorPosition(): gEidosErrorContext change");
	
	gEidosErrorContext.errorPosition = p_saved_position;
}

inline __attribute__((always_inline)) void ClearErrorPosition(void)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("ClearErrorPosition(): gEidosErrorContext change");
	
	gEidosErrorContext.errorPosition = EidosErrorPosition{-1, -1, -1, -1};
}

extern int gEidosErrorLine, gEidosErrorLineCharacter;	// set up by EidosTerminate()

// Warnings: consult this flag before emitting a warning
extern bool gEidosSuppressWarnings;


// *******************************************************************************************************************
//
//	Debugging support
//
#pragma mark -
#pragma mark Debugging support
#pragma mark -

// Debugging #defines that can be turned on
#define EIDOS_DEBUG_COMPLETION	0	// turn on to log information about symbol types whenever doing code completion

// Flags for various runtime checks that can be turned on or off; in SLiM, -x turns these off.
extern bool eidos_do_memory_checks;

// To leak-check slim, a few steps are recommended (BCH 5/1/2019):
//
//	- turn on Malloc Scribble so spurious pointers left over in deallocated blocks are not taken to be live references
//	- turn on Malloc Logging so you get backtraces from every leaked allocation
//	- use a DEBUG build of slim so the backtraces are accurate and not obfuscated by optimization
//	- set this #define to 1 so slim cleans up a bit and then sleeps before exit, waiting for its leaks to be assessed
//	- run "leaks slim" in Terminal; the leaks tool in Instruments seems to be very confused and reports tons of false positives
//
// To run slim under Valgrind, setting this flag to 1 is also recommended as it will enable some thunks that will
#define SLIM_LEAK_CHECKING	0
// keep Valgrind from getting confused.  Use a DEBUG build so it is symbolicated (-g) and minimally optimized (-Og),
// or add those flags to CMAKE_C_FLAGS_RELEASE and CMAKE_CXX_FLAGS_RELEASE in CMakeLists.txt.

#if SLIM_LEAK_CHECKING
#warning SLIM_LEAK_CHECKING enabled!
#endif

// Enabling "debug points" in SLiMgui.  These are only enabled under SLiMgui (i.e., QtSLiM), and should always be enabled
// in that scenario, for end users.  However, I've put a define here to control them for my own debugging purposes.
#ifdef SLIMGUI
#define DEBUG_POINTS_ENABLED	1
extern int gEidosDebugIndent;
#else
#define DEBUG_POINTS_ENABLED	0
#endif

#if DEBUG_POINTS_ENABLED
// A simple class for RAII-based debug point indentation; saves some hassle with exceptions, etc.
class EidosDebugPointIndent
{
private:
	int indent_;
public:
	EidosDebugPointIndent(void) : indent_(0) { }
	~EidosDebugPointIndent(void) { gEidosDebugIndent -= indent_; }
	inline void indent(int spaces = 4) { gEidosDebugIndent += spaces; indent_ += spaces; }
	inline void outdent(int spaces = 4) { gEidosDebugIndent -= spaces; indent_ -= spaces; }
	
	static inline const std::string Indent(void) { return std::string(gEidosDebugIndent, ' '); }
};
#endif

// Eidos defines the concept of "long-term boundaries", which are moments in time when Eidos objects
// that are not under retain-release memory management could be freed.  Keeping a reference to such
// an object between long-term boundaries is generally safe; keeping a reference to such an object
// across a long-term boundary is generally NOT safe.  This function should be called, internally by
// Eidos and externally by the Context, at such boundaries, and code should not free Eidos objects
// (except local temporaries) without calling this function first.  This allows internal bookkeeping
// to check for violations of the long-term boundary conventions.
void CheckLongTermBoundary();


// *******************************************************************************************************************
//
//	Memory usage monitoring
//
#pragma mark -
#pragma mark Memory usage monitoring
#pragma mark -

// Memory-monitoring calls.  See the .cpp for comments.  These return a size in bytes.
size_t Eidos_GetPeakRSS(void);
size_t Eidos_GetCurrentRSS(void);
size_t Eidos_GetVMUsage(void);

// Memory limits, retrieved by calling "ulimit -m"; cached internally.  Returns a size in bytes; 0 means "no limit".
size_t Eidos_GetMaxRSS(void);

// This checks whether our memory usage has gotten within 10 MB of the maximum memory usage, and terminates if so.
// p_message1 should be the name of the calling function/method; p_message2 can be any clarifying message.
// It is a good idea to check eidos_do_memory_checks before calling this, to save calling overhead.
void Eidos_CheckRSSAgainstMax(const std::string &p_message1, const std::string &p_message2);


// *******************************************************************************************************************
//
//	Profiling support
//
#pragma mark -
#pragma mark Profiling support
#pragma mark -

// BCH 1/22/2023: Note that profiling can now be enabled for both command-line and GUI builds.  It is enabled
// when SLIMPROFILING is defined to 1; if it is undefined, 0, or any other value, profiling is disabled.
// BCH 8/5/2023: Also, note that the foundational profiling code is now also used for the EidosBenchmark facility,
// even when SLiM is not built for profiling, so that foundational code is now always included in the build.

#if defined(MACH_PROFILING)

// This is the fastest clock, is available across OS X versions, and gives us nanoseconds.  The only disadvantage to
// it is that it is platform-specific, so we can only use this clock in SLiMgui and Eidos_GUI.  That is OK.  This
// returns uint64_t in CPU-specific time units; see https://developer.apple.com/library/content/qa/qa1398/_index.html
typedef uint64_t eidos_profile_t;

// Get an uncorrected profile clock measurement (for EidosBenchmark), to be used as a start or end time
inline __attribute__((always_inline)) eidos_profile_t Eidos_BenchmarkTime(void) { return mach_absolute_time(); }

#elif defined(CHRONO_PROFILING)

// For the <chrono> steady_clock time point representation, we will convert time points to nanoseconds since epoch
typedef uint64_t eidos_profile_t;

// Get an uncorrected profile clock measurement (for EidosBenchmark), to be used as a start or end time
inline __attribute__((always_inline)) eidos_profile_t Eidos_BenchmarkTime(void) { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

#endif

#define EIDOS_BENCHMARK_START(x)	eidos_profile_t slim__benchmark_start = ((gEidosBenchmarkType == (x)) ? Eidos_BenchmarkTime() : 0);
#define EIDOS_BENCHMARK_END(x)	if (gEidosBenchmarkType == (x)) gEidosBenchmarkAccumulator += (Eidos_BenchmarkTime() - slim__benchmark_start);

// Convert an elapsed profiling time (the difference between two Eidos_ProfileTime() results) to seconds
double Eidos_ElapsedProfileTime(eidos_profile_t p_elapsed_profile_time);


// For benchmarking purposes, we now have a small timing facility that can time one selected
// piece of code, reporting the total time taken in that one piece of code as the total runtime
// of the SLiM model.  This is entirely separate from the user-level profiling feature that
// generates a profile report of a whole running model, and it is not user-visible.  The piece
// of code selected for timing is chosen with an internal Eidos function, _startBenchmark().
// You can either end benchmarking with _stopBenchmark() and get the elapsed seconds inside
// the code benchmarked, as the return value, or let a command-line run finish and get the
// total benchmarked time as console output.  Note that EidosBenchmark does not attempt to
// correct for its own overhead/lag, so it is less accurate than profiling; it should be
// used where relative times are more important than absolute times, and where the time spent
// for one execution of the benchmarked code block takes a significant amount of time (making
// overhead/lag negligible).  The enum has stuff for SLiM as well, for convenience.

typedef enum {
	kNone = 0,
	
	// Eidos internal
	k_SAMPLE_INDEX,					// making an index buffer for sample(), to use in the sampling algorithm
	k_TABULATE_MAXBIN,				// calculating the maxbin value for tabulate(), if not user-supplied
	
	// SLiM internal parallel loops
	k_AGE_INCR,						// age increment, at end of tick
	k_DEFERRED_REPRO,				// deferred reproduction (without callbacks) in nonWF models
	k_WF_REPRO,						// WF reproduction (without callbacks)
	k_FITNESS_ASEX_1,				// fitness calculation, asexual, with individual fitnessScaling values but no non-neutral mutations
	k_FITNESS_ASEX_2,				// fitness calculation, asexual, with neither individual fitnessScaling nor non-neutral mutations
	k_FITNESS_ASEX_3,				// fitness calculation, asexual, with individual fitnessScaling values and non-neutral mutations
	k_FITNESS_SEX_1,				// fitness calculation, sexual, with individual fitnessScaling values but no non-neutral mutations
	k_FITNESS_SEX_2,				// fitness calculation, sexual, with neither individual fitnessScaling nor non-neutral mutations
	k_FITNESS_SEX_3,				// fitness calculation, sexual, with individual fitnessScaling values and non-neutral mutations
	k_MIGRANT_CLEAR,				// clearing the migrant flag of individuals, at end of tick
	k_PARENTS_CLEAR,				// clearing the genomes of parents at generation switch, in WF models
	k_UNIQUE_MUTRUNS,				// uniquing mutation runs (periodic bookkeeping)
	k_SURVIVAL,						// evaluating survival in nonWF models (without callbacks)
	
	// SLiM, whole tasks (not parallel loops)
	k_MUT_TALLY,					// tally mutation reference counts, in Population::MaintainMutationRegistry()
	k_MUTRUN_FREE,					// free unused mutation runs, in Population::MaintainMutationRegistry()
	k_MUT_FREE,						// free fixed or unused mutations, in Population::MaintainMutationRegistry()
	k_SIMPLIFY_SORT_PRE,			// pre-sorting for simplification, in slim_sort_edges()
	k_SIMPLIFY_SORT,				// sorting for simplification, in slim_sort_edges()
	k_SIMPLIFY_SORT_POST,			// post-sorting for simplification, in slim_sort_edges()
	k_SIMPLIFY_CORE,				// the core simplification algorithm, in Species::SimplifyTreeSequence()
} EidosBenchmarkType;

extern EidosBenchmarkType gEidosBenchmarkType;			// which code is being benchmarked in this run; kNone by default
extern eidos_profile_t gEidosBenchmarkAccumulator;		// accumulated profile counts for the benchmarked code


#if (SLIMPROFILING == 1)
// PROFILING

extern int gEidosProfilingClientCount;	// if non-zero, profiling is happening in some context

// Profiling clocks; note that these can overflow, we don't care, only (t2-t1) ever matters and that is overflow-robust

extern uint64_t gEidos_ProfileCounter;			// incremented by Eidos_ProfileTime() every time it is called
extern double gEidos_ProfileOverheadTicks;		// the overhead for one profile call, in ticks
extern double gEidos_ProfileOverheadSeconds;	// the overhead for one profile call, in seconds
extern double gEidos_ProfileLagTicks;			// the clocked length of an empty profile block, in ticks
extern double gEidos_ProfileLagSeconds;			// the clocked length of an empty profile block, in seconds

#if defined(MACH_PROFILING)

// Get a profile clock measurement, to be used as a start or end time
inline __attribute__((always_inline)) eidos_profile_t Eidos_ProfileTime(void) { gEidos_ProfileCounter++; return mach_absolute_time(); }

#elif defined(CHRONO_PROFILING)

// Get a profile clock measurement, to be used as a start or end time
inline __attribute__((always_inline)) eidos_profile_t Eidos_ProfileTime(void) { gEidos_ProfileCounter++; return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

#endif

// This should be called immediately before profiling to measure the overhead and lag for profile blocks
void Eidos_PrepareForProfiling(void);

// Macros for profile blocks
#define	SLIM_PROFILE_BLOCK_START()																																			\
	bool slim__condition_a = (gEidosProfilingClientCount ? true : false);																									\
	eidos_profile_t slim__start_clock = (slim__condition_a ? Eidos_ProfileTime() : eidos_profile_t());																		\
	uint64_t slim__start_profile_counter = (slim__condition_a ? gEidos_ProfileCounter : 0);

#define	SLIM_PROFILE_BLOCK_START_NESTED()																																	\
	eidos_profile_t slim__start_clock2 = (slim__condition_a ? Eidos_ProfileTime() : eidos_profile_t());																		\
	uint64_t slim__start_profile_counter2 = (slim__condition_a ? gEidos_ProfileCounter : 0);

#define	SLIM_PROFILE_BLOCK_START_CONDITION(slim__condition_param)																											\
	bool slim__condition_b = gEidosProfilingClientCount && (slim__condition_param);																							\
	eidos_profile_t slim__start_clock = (slim__condition_b ? Eidos_ProfileTime() : eidos_profile_t());																		\
	uint64_t slim__start_profile_counter = (slim__condition_b ? gEidos_ProfileCounter : 0);

#define SLIM_PROFILE_BLOCK_END(slim__accumulator)																															\
	if (slim__condition_a)																																					\
	{																																										\
		uint64_t slim__end_profile_counter = gEidos_ProfileCounter;																											\
		uint64_t slim__contained_profile_calls = slim__end_profile_counter - slim__start_profile_counter;																	\
		eidos_profile_t slim__end_clock = Eidos_ProfileTime();																												\
																																											\
		uint64_t slim__uncorrected_ticks = (slim__end_clock - slim__start_clock);																							\
		uint64_t slim__correction = static_cast<eidos_profile_t>(round(gEidos_ProfileLagTicks + gEidos_ProfileOverheadTicks * slim__contained_profile_calls));				\
		uint64_t slim__corrected_ticks = ((slim__correction < slim__uncorrected_ticks) ? (slim__uncorrected_ticks - slim__correction) : 0);									\
																																											\
		(slim__accumulator) += slim__corrected_ticks;																														\
	}

#define SLIM_PROFILE_BLOCK_END_NESTED(slim__accumulator)																													\
	if (slim__condition_a)																																					\
	{																																										\
		uint64_t slim__end_profile_counter2 = gEidos_ProfileCounter;																										\
		uint64_t slim__contained_profile_calls2 = slim__end_profile_counter2 - slim__start_profile_counter2;																\
		eidos_profile_t slim__end_clock2 = Eidos_ProfileTime();																												\
																																											\
		uint64_t slim__uncorrected_ticks2 = (slim__end_clock2 - slim__start_clock2);																						\
		uint64_t slim__correction2 = (eidos_profile_t)round(gEidos_ProfileLagTicks + gEidos_ProfileOverheadTicks * slim__contained_profile_calls2);							\
		uint64_t slim__corrected_ticks2 = ((slim__correction2 < slim__uncorrected_ticks2) ? (slim__uncorrected_ticks2 - slim__correction2) : 0);							\
																																											\
		(slim__accumulator) += slim__corrected_ticks2;																														\
	}

#define SLIM_PROFILE_BLOCK_END_CONDITION(slim__accumulator)																													\
	if (slim__condition_b)																																					\
	{																																										\
		uint64_t slim__end_profile_counter = gEidos_ProfileCounter;																											\
		uint64_t slim__contained_profile_calls = slim__end_profile_counter - slim__start_profile_counter;																	\
		eidos_profile_t slim__end_clock = Eidos_ProfileTime();																												\
																																											\
		uint64_t slim__uncorrected_ticks = (slim__end_clock - slim__start_clock);																							\
		uint64_t slim__correction = (eidos_profile_t)round(gEidos_ProfileLagTicks + gEidos_ProfileOverheadTicks * slim__contained_profile_calls);							\
		uint64_t slim__corrected_ticks = ((slim__correction < slim__uncorrected_ticks) ? (slim__uncorrected_ticks - slim__correction) : 0);									\
																																											\
		(slim__accumulator) += slim__corrected_ticks;																														\
	}

#endif	// (SLIMPROFILING == 1)


// *******************************************************************************************************************
//
//	Termination handling
//
#pragma mark -
#pragma mark Termination handling
#pragma mark -

// Print a demangled stack backtrace of the caller function to FILE* out; see eidos_globals.cpp for credits and comments.
void Eidos_PrintStacktrace(FILE *p_out = stderr, unsigned int p_max_frames = 63);

// Print an offending line of script with carets indicating an error position
void Eidos_ScriptErrorPosition(const EidosErrorContext &p_error_context);
void Eidos_LogScriptError(std::ostream& p_out, const EidosErrorContext &p_error_context);

// If gEidosTerminateThrows == 0, << EidosTerminate causes a call to exit().  In that mode, output
// related to termination output goes to cerr.  The other mode has gEidosTerminateThrows == 1.  In that mode,
// we use a global ostringstream to capture all termination-related output, and whoever catches the raise handles
// the termination stream.  All other Eidos output goes to ExecutionOutputStream(), defined on EidosInterpreter.
// BCH 2/9/2021: Eidos output can now also go to ErrorOutputStream(), also defined on EidosInterpreter.
extern bool gEidosTerminateThrows;
extern std::ostringstream gEidosTermination;

#define EIDOS_TERMINATION	(gEidosTerminateThrows ? gEidosTermination : std::cerr)

// This little class is used as a stream manipulator that causes termination with EXIT_FAILURE, optionally
// with a backtrace.  This is nice since it lets us log and terminate in a single line of code.  It also allows
// a GUI to intercept the exit() call and do something more graceful with it.
class EidosTerminate
{
public:
	bool print_backtrace_ = false;
	
	EidosTerminate(void) = default;							// default constructor, no backtrace, does not change error range
	explicit EidosTerminate(const EidosToken *p_error_token);	// supply a token from which an error range is taken
	
	// These constructors request a backtrace as well
	explicit EidosTerminate(bool p_print_backtrace);
	EidosTerminate(const EidosToken *p_error_token, bool p_print_backtrace);
};

// Send an EidosTerminate object to an output stream, causing a raise or an exit() call depending on
// gEidosTerminateThrows.  Either way, this call does not return, so it is marked as noreturn as a hint
// to the compiler.  Also, since this is always an error case, it is marked as cold as a hint to branch
// prediction and optimization; all code paths that lead to this will be considered cold.
void operator<<(std::ostream& p_out, const EidosTerminate &p_terminator) __attribute__((__noreturn__)) __attribute__((cold)) __attribute__((analyzer_noreturn));

// Get the message from the last raise out of gEidosTermination, optionally with newlines trimmed from the ends
std::string Eidos_GetTrimmedRaiseMessage(void);
std::string Eidos_GetUntrimmedRaiseMessage(void);


// *******************************************************************************************************************
//
//	File I/O functions
//
#pragma mark -
#pragma mark File I/O
#pragma mark -

// Resolve a leading ~ in a filesystem path to the user's home directory
std::string Eidos_ResolvedPath(const std::string &p_path);

// Get the filename (or a trailing directory name) from a path
std::string Eidos_LastPathComponent(const std::string &p_path);

// Get the current working directory; oddly, C++ has no API for this
std::string Eidos_CurrentDirectory(void);

// Remove a trailing slash in a path like ~/foo/bar/
std::string Eidos_StripTrailingSlash(const std::string &p_path);

// Create a directory at a given filesystem path if it does not already exist (which is not an error);
// calls Eidos_ResolvedPath() on the given path, since I think we always want that anyway.  Returns false
// if the operation fails (i.e. the directory may or may not even exist).  Returns true if the directory
// exists.  A warning string can be returned through p_error_string, even if true is returned; for example,
// if the directory already exists a warning is emitted but the return value is true.
bool Eidos_CreateDirectory(const std::string &p_path, std::string *p_error_string);

// This is /tmp/ (with trailing slash!) on macOS and Linux, but will be elsewhere on Windows.  Should be used instead of /tmp/ everywhere.
std::string Eidos_TemporaryDirectory(void);

// Returns true if Eidos_TemporaryDirectory() appears to exist and be writeable, false otherwise
// Apparently on some cluster systems /tmp does not exist, weirdly... I have no idea what's up with that...
bool Eidos_TemporaryDirectoryExists(void);

// Create a temporary file based upon a template filename; note that pattern is modified!
int Eidos_mkstemps(char *p_pattern, int p_suffix_len);
int Eidos_mkstemps_directory(char *p_pattern, int p_suffix_len);

// Writing files with support for gzip compression and buffered flushing
#define EIDOS_BUFFER_ZIP_APPENDS	1

#if EIDOS_BUFFER_ZIP_APPENDS	// implementation details for Eidos_FlushFiles(); for internal use only
extern std::unordered_map<std::string, std::string> gEidosBufferedZipAppendData;	// filename -> text
bool _Eidos_FlushZipBuffer(const std::string &p_file_path, const std::string &p_outstring);
#endif

void Eidos_FlushFile(const std::string &p_file_path);
void Eidos_FlushFiles(void);			// This should be called at the end of execution, or any other appropriate time, to flush buffered file append data

enum class EidosFileFlush {
	kNoFlush = 0,		// no flush, no matter what
	kDefaultFlush,		// flush if the buffer is over a threshold number of bytes
	kForceFlush			// flush, no matter what; not recommended with compression
};

void Eidos_WriteToFile(const std::string &p_file_path, const std::vector<const std::string *> &p_contents, bool p_append, bool p_compress, EidosFileFlush p_flush_option);


// *******************************************************************************************************************
//
//	Utility functions
//
#pragma mark -
#pragma mark Utility functions
#pragma mark -

// bzero() is deprecated, but memset() is not a perfect substitute, so this is a macro to use instead
// this follows the standard bzero() declaration: void bzero(void *s, size_t n);
// see https://stackoverflow.com/a/17097978/2752221 for some justification
#define EIDOS_BZERO(s, n) memset((s), 0, (n))

// Welch's t-test functions; sample means are returned in mean1 and mean2, which may be nullptr
double Eidos_TTest_TwoSampleWelch(const double *p_set1, int p_count1, const double *p_set2, int p_count2, double *p_mean1, double *p_mean2);
double Eidos_TTest_OneSample(const double *p_set1, int p_count1, double p_mu, double *p_mean1);

// Exact summation of a floating-point vector using the Shewchuk algorithm; surprisingly, not in the GSL
double Eidos_ExactSum(const double *p_double_vec, int64_t p_vec_length);

// Approximate equality of two floating-point numbers, within a ratio tolerance of 1.0001
bool Eidos_ApproximatelyEqual(double a, double b);

// Split a std::string into a vector of substrings separated by a given delimiter
std::vector<std::string> Eidos_string_split(const std::string &p_str, const std::string &p_delim);
std::string Eidos_string_join(const std::vector<std::string> &p_vec, const std::string &p_delim);
bool Eidos_string_hasPrefix(std::string const &fullString, std::string const &prefix);
bool Eidos_string_hasSuffix(std::string const &fullString, std::string const &suffix);
bool Eidos_string_containsCaseInsensitive(const std::string &strHaystack, const std::string &strNeedle);
bool Eidos_string_equalsCaseInsensitive(const std::string &s1, const std::string &s2);

// Quote and escape a string
enum class EidosStringQuoting {
	kNoQuotes = 0,
	kSingleQuotes,
	kDoubleQuotes,
	kChooseQuotes		// chooses single or double, whichever seems better
};

std::string Eidos_string_escaped(const std::string &unescapedString, EidosStringQuoting quoting);		// quotes and adds backslash escapes
std::string Eidos_string_escaped_CSV(const std::string &unescapedString);								// quotes and ""-escapes quotes

// Run a Unix command
// BCH 13 December 2017: no longer used, commenting this out
//std::string Eidos_Exec(const char *p_cmd);

extern int gEidosFloatOutputPrecision;		// precision used for output of float values in Eidos; not user-visible at present

std::string EidosStringForFloat(double p_value);

int DisplayDigitsForIntegerPart(double x);	// number of digits needed to display the integer part of a double

// Fisher-Yates Shuffle: choose a random subset of a std::vector, without replacement.
// see https://stackoverflow.com/questions/9345087/choose-m-elements-randomly-from-a-vector-containing-n-elements
// see also https://ideone.com/3A3cv for demo code using this
// Note that this uses random(), not the GSL RNG.  This is actually desirable, because we use this
// for doing haplotype display stuff; using random() avoids altering the simulation state.  For use
// in a simulation, see the implementation of sample() for some useful approaches.
template<class BidiIter>
BidiIter Eidos_random_unique(BidiIter begin, BidiIter end, size_t num_random)
{
	size_t left = std::distance(begin, end);
	
	while (num_random--)
	{
		BidiIter r = begin;
		std::advance(r, random() % left);
		std::swap(*begin, *r);
		++begin;
		--left;
	}
	
	return begin;
}

// The <regex> library does not work on Ubuntu 18.04, annoyingly; probably a very old compiler or something.  So we have to check.
bool Eidos_RegexWorks(void);


// *******************************************************************************************************************
//
//	SHA-256
//
#pragma mark -
#pragma mark SHA-256
#pragma mark -

// from https://github.com/amosnier/sha-2 which is under the public-domain "unlicense"; see .cpp for further details
void Eidos_calc_sha_256(uint8_t hash[32], const void *input, size_t len);	// len is the number of bytes in input
void Eidos_hash_to_string(char string[65], const uint8_t hash[32]);


// *******************************************************************************************************************
//
//	Locking
//
#pragma mark -
#pragma mark Locking
#pragma mark -

// exception-safe RAII-based locking; modified from https://stackoverflow.com/a/2201076/2752221
// I didn't want to depend upon assert(), so I removed that; the user is responsible for checking the flag before locking
// The point of this is simply that the lock flag gets cleared automatically on exit by RAII, even with exceptions
class Eidos_simple_lock
{
private:
	Eidos_simple_lock(const Eidos_simple_lock&) = delete;
	Eidos_simple_lock& operator=(const Eidos_simple_lock&) = delete;
	
	bool& mLock;
	
public:
	Eidos_simple_lock(bool& pLock) : mLock(pLock)	{	mLock = true;	}
	~Eidos_simple_lock(void)						{	mLock = false;	}
};


// *******************************************************************************************************************
//
//	Overflow-detecting integer operations
//
#pragma mark -
#pragma mark Overflow-detecting integer operations
#pragma mark -

// Clang and GCC 5.0 support a suite a builtin functions that perform integer operations with detection of overflow.
//
//     http://clang.llvm.org/docs/LanguageExtensions.html#checked-arithmetic-builtins and
//     https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html
//
// We want to use those builtins when possible, so that Eidos integer arithmetic is robust against overflow bugs.
// However, since GCC pre-5.0 doesn't have these builtins, and other compilers probably don't either, we have to
// do some messy version checking and such.

// BCH 6 April 2016: switched from the type-specific variants (i.e. __builtin_saddll_overflow, etc.) to the
// type-non-specific variants (i.e. __builtin_add_overflow) since some platforms use more than 64 bits for
// "long long".  The non-specific variants should choose which type-specific variant to use at compile time
// anyway, so this should not be an issue as long as we are careful to always use int64_t operands.

#if (defined(__clang__) && defined(__has_builtin))

#if __has_builtin(__builtin_add_overflow)
// Clang with support for overflow built-ins (if one is defined we assume all of them are)
#define Eidos_add_overflow(a, b, c)	__builtin_add_overflow((a), (b), (c))
#define Eidos_sub_overflow(a, b, c)	__builtin_sub_overflow((a), (b), (c))
#define Eidos_mul_overflow(a, b, c)	__builtin_mul_overflow((a), (b), (c))
#endif

#elif defined(__GNUC__)

#if (__GNUC__ >= 5)
// GCC 5.0 or later; overflow built-ins supported
#define Eidos_add_overflow(a, b, c)	__builtin_add_overflow((a), (b), (c))
#define Eidos_sub_overflow(a, b, c)	__builtin_sub_overflow((a), (b), (c))
#define Eidos_mul_overflow(a, b, c)	__builtin_mul_overflow((a), (b), (c))
#endif

#endif

// Uncomment these to test handling of missing built-ins on a system that does have the built-ins; with these
// uncommented, the Eidos test suite should emit a warning about missing integer overflow checks, but all tests
// that are run should pass.
//#undef Eidos_add_overflow
//#undef Eidos_sub_overflow
//#undef Eidos_mul_overflow


#ifdef Eidos_add_overflow

#define EIDOS_HAS_OVERFLOW_BUILTINS		1

#else

// Define the macros to just use simple operators, in all other cases.  Note that this means overflows are not
// detected!  The Eidos test suite will emit a warning in this case, telling the user to upgrade their compiler.
#define Eidos_add_overflow(a, b, c)	(*(c)=(a)+(b), false)
#define Eidos_sub_overflow(a, b, c)	(*(c)=(a)-(b), false)
#define Eidos_mul_overflow(a, b, c)	(*(c)=(a)*(b), false)
#define EIDOS_HAS_OVERFLOW_BUILTINS		0

#endif


// *******************************************************************************************************************
//
//	Floating point representation in text
//
#pragma mark -
#pragma mark Floating point representation
#pragma mark -

// The question here is how many digits are needed to print out in text representations of floating-point numbers
// in order to preserve their exact value.  This is quite a complicated question, and the macros in float.h needed
// to resolve it are not always available.  See the answer from chux at https://stackoverflow.com/a/19897395/2752221,
// which this is based upon.  The EIDOS_DBL_DIGS and EIDOS_FLT_DIGS macros defined here can be used directly with
// a printf() format of %.*g with a double or float argument, respectively, to produce a correct representation in
// all cases, if I understand that thread correctly.

#ifdef DBL_DECIMAL_DIG
#define EIDOS_DBL_DIGS (DBL_DECIMAL_DIG)
#else  
#define EIDOS_DBL_DIGS (DBL_DIG + 3)
#endif

#ifdef FLT_DECIMAL_DIG
#define EIDOS_FLT_DIGS (FLT_DECIMAL_DIG)
#else  
#define EIDOS_FLT_DIGS (FLT_DIG + 3)
#endif


// *******************************************************************************************************************
//
//	Global strings & IDs
//
#pragma mark -
#pragma mark Global strings & IDs
#pragma mark -

//
//	Global std::string objects.  This is kind of gross, but there are several rationales for it.  First of all, it makes
//	a speed difference; converting a C string to a std::string is done every time it is hit in the code (C++ does not
//	treat that as a constant expression and cache it for you, at least with the current generation of compilers).  The
//	conversion is surprisingly slow; it has shown up repeatedly in profiles I have done.  Second, there is the issue of
//	uniqueness; many of these strings occur in multiple places in the code, and a typo in one of those multiple occurrences
//	would cause a bug that would be very difficult to find.  If multiple places in the code intend to refer to the same
//	conceptual string, and rely on those references being the same, then a shared constant should be used.  So... oh well.
//

typedef uint32_t EidosGlobalStringID;

class _EidosRegisteredString;

// The string registry itself.  This is a bit of an odd design; the class has a singleton instance, but that is hidden
// and all the public API is static methods.  This makes the syntax for using the registry smoother; you don't have
// to ask for the shared instance, that's an implementation detail.  The reason for this implementation choice is that
// it gets us past C++'s incredibly annoying "static initialization order fiasco"; since all access to the internals
// of the registry is guarded by the singleton bottleneck, our private ivars are guaranteed to get initialized on
// demand.  If we just made them static data members and got rid of the singleton pattern, there would be a global
// initialize-time order dependency, since calls to register strings get made as a side effect of global initialization;
// the registration call might occur when the statics for the registry had not been set up yet.
class EidosStringRegistry
{
private:
	// robin_hood seems unnecessary here, dynamic lookups are generally not a bottleneck because EidosGlobalStringID gets cached
	std::unordered_map<std::string, EidosGlobalStringID> gStringToID;
	std::unordered_map<EidosGlobalStringID, const std::string *> gIDToString;
	std::vector<const _EidosRegisteredString *> gIDToString_Thunk;
	std::vector<const std::string *> globalString_Thunk;
	EidosGlobalStringID gNextUnusedID;
	
	EidosStringRegistry(const EidosStringRegistry&) = delete;					// no copying
	EidosStringRegistry& operator=(const EidosStringRegistry&) = delete;		// no copying
	EidosStringRegistry(void);													// construction is private
	~EidosStringRegistry(void);
	
	// this is the singleton bottleneck, thread-safe with C++11
	static inline EidosStringRegistry& sharedRegistry(void)
	{
		static EidosStringRegistry instance;
		return instance;
	}
	
	EidosGlobalStringID _GlobalStringIDForString(const std::string &p_string);
	const std::string &_StringForGlobalStringID(EidosGlobalStringID p_string_id);
	void _RegisterStringForGlobalID(const std::string &p_string, EidosGlobalStringID p_string_id);
	
	// This registers a standard string with a given ID.  This should not be called directly, to avoid initialization order
	// problems; instead, declare a global EidosRegisteredString.  All IDs used by the Context should start at gEidosID_LastEntry.
	// This function does not make a copy of the string that it is passed, since it is intended for use with global strings.
	static void RegisterStringForGlobalID(const std::string &p_string, EidosGlobalStringID p_string_id)
	{
		return EidosStringRegistry::sharedRegistry()._RegisterStringForGlobalID(p_string, p_string_id);
	}
    
public:
	// GlobalStringIDForString() takes any string and uniques it through a hash table.  If the string does not already exist
	// in the hash table, it is copied, and the copy is registered and returned as the uniqued string.
	static inline EidosGlobalStringID GlobalStringIDForString(const std::string &p_string)
	{
		return EidosStringRegistry::sharedRegistry()._GlobalStringIDForString(p_string);
	}
	
	// StringForGlobalStringID() returns the uniqued global string for the ID through a reverse lookup.  The reference
	// returned is to the uniqued string stored internally by the registry, so it will be the same every time this is called,
	// and does not need to be copied if kept externally.
	static const std::string &StringForGlobalStringID(EidosGlobalStringID p_string_id)
	{
		return EidosStringRegistry::sharedRegistry()._StringForGlobalStringID(p_string_id);
	}
    
#if SLIM_LEAK_CHECKING
    static inline void ThunkRegistration(_EidosRegisteredString *p_registration_object)
    {
        EidosStringRegistry::sharedRegistry().gIDToString_Thunk.emplace_back(p_registration_object);
    }
#endif
	
	friend class _EidosRegisteredString;
};

// This class is used to register a string with a given id, calling RegisterStringForGlobalID() as a construction side effect.
class _EidosRegisteredString
{
private:
	const std::string string_;
	const EidosGlobalStringID string_id_;
	inline _EidosRegisteredString(const char *p_cstr, EidosGlobalStringID p_id) : string_(p_cstr), string_id_(p_id)
	{
		EidosStringRegistry::RegisterStringForGlobalID(string_, string_id_);
	}
public:
	friend const std::string &EidosRegisteredString(const char *p_cstr, EidosGlobalStringID p_id);
};

// This should be used as the "constructor" for _EidosRegisteredString, yielding a std::string& that is the uniqued string object
const std::string &EidosRegisteredString(const char *p_cstr, EidosGlobalStringID p_id);


extern const std::string &gEidosStr_empty_string;
extern const std::string &gEidosStr_space_string;

extern const std::string &gEidosStr_apply;
extern const std::string &gEidosStr_sapply;
extern const std::string &gEidosStr_doCall;
extern const std::string &gEidosStr_executeLambda;
extern const std::string &gEidosStr__executeLambda_OUTER;
extern const std::string &gEidosStr_ls;
extern const std::string &gEidosStr_rm;
extern const std::string &gEidosStr_usage;

extern const std::string &gEidosStr_if;
extern const std::string &gEidosStr_else;
extern const std::string &gEidosStr_do;
extern const std::string &gEidosStr_while;
extern const std::string &gEidosStr_for;
extern const std::string &gEidosStr_in;
extern const std::string &gEidosStr_next;
extern const std::string &gEidosStr_break;
extern const std::string &gEidosStr_return;
extern const std::string &gEidosStr_function;

extern const std::string &gEidosStr_T;
extern const std::string &gEidosStr_F;
extern const std::string &gEidosStr_NULL;
extern const std::string &gEidosStr_PI;
extern const std::string &gEidosStr_E;
extern const std::string &gEidosStr_INF;
extern const std::string &gEidosStr_MINUS_INF;	// "-INF"
extern const std::string &gEidosStr_NAN;

extern const std::string &gEidosStr_void;
extern const std::string &gEidosStr_logical;
extern const std::string &gEidosStr_string;
extern const std::string &gEidosStr_integer;
extern const std::string &gEidosStr_float;
extern const std::string &gEidosStr_object;
extern const std::string &gEidosStr_numeric;

extern const std::string &gEidosStr_ELLIPSIS;
extern const std::string &gEidosStr_type;
extern const std::string &gEidosStr_source;
extern const std::string &gEidosStr_GetPropertyOfElements;
extern const std::string &gEidosStr_ExecuteInstanceMethod;
extern const std::string &gEidosStr_undefined;
extern const std::string &gEidosStr_applyValue;

extern const std::string &gEidosStr_Object;
extern const std::string &gEidosStr_size;
extern const std::string &gEidosStr_length;
extern const std::string &gEidosStr_methodSignature;
extern const std::string &gEidosStr_propertySignature;
extern const std::string &gEidosStr_str;
extern const std::string &gEidosStr_stringRepresentation;

extern const std::string &gEidosStr__TestElement;
extern const std::string &gEidosStr__TestElementNRR;
extern const std::string &gEidosStr__yolk;
extern const std::string &gEidosStr__increment;
extern const std::string &gEidosStr__cubicYolk;
extern const std::string &gEidosStr__squareTest;

extern const std::string &gEidosStr_DictionaryBase;
extern const std::string &gEidosStr_allKeys;
extern const std::string &gEidosStr_addKeysAndValuesFrom;
extern const std::string &gEidosStr_appendKeysAndValuesFrom;
extern const std::string &gEidosStr_clearKeysAndValues;
extern const std::string &gEidosStr_compactIndices;
extern const std::string &gEidosStr_getRowValues;
extern const std::string &gEidosStr_getValue;
extern const std::string &gEidosStr_identicalContents;
extern const std::string &gEidosStr_serialize;
extern const std::string &gEidosStr_setValue;

extern const std::string &gEidosStr_Dictionary;

extern const std::string &gEidosStr_DataFrame;
extern const std::string &gEidosStr_colNames;
extern const std::string &gEidosStr_dim;
extern const std::string &gEidosStr_ncol;
extern const std::string &gEidosStr_nrow;
extern const std::string &gEidosStr_asMatrix;
extern const std::string &gEidosStr_cbind;
extern const std::string &gEidosStr_rbind;
extern const std::string &gEidosStr_subset;
extern const std::string &gEidosStr_subsetColumns;
extern const std::string &gEidosStr_subsetRows;

extern const std::string &gEidosStr_Image;
extern const std::string &gEidosStr_width;
extern const std::string &gEidosStr_height;
extern const std::string &gEidosStr_bitsPerChannel;
extern const std::string &gEidosStr_isGrayscale;
extern const std::string &gEidosStr_integerR;
extern const std::string &gEidosStr_integerG;
extern const std::string &gEidosStr_integerB;
extern const std::string &gEidosStr_integerK;
extern const std::string &gEidosStr_floatR;
extern const std::string &gEidosStr_floatG;
extern const std::string &gEidosStr_floatB;
extern const std::string &gEidosStr_floatK;
extern const std::string &gEidosStr_write;

extern const std::string &gEidosStr_start;
extern const std::string &gEidosStr_end;
extern const std::string &gEidosStr_weights;
extern const std::string &gEidosStr_range;
extern const std::string &gEidosStr_c;
extern const std::string &gEidosStr_t;
extern const std::string &gEidosStr_n;
extern const std::string &gEidosStr_s;
extern const std::string &gEidosStr_x;
extern const std::string &gEidosStr_y;
extern const std::string &gEidosStr_z;
extern const std::string &gEidosStr_xy;
extern const std::string &gEidosStr_xz;
extern const std::string &gEidosStr_yz;
extern const std::string &gEidosStr_xyz;
extern const std::string &gEidosStr_color;
extern const std::string &gEidosStr_filePath;

extern const std::string &gEidosStr_Mutation;	// in Eidos for hack reasons; see EidosValue_Object::EidosValue_Object()
extern const std::string &gEidosStr_Genome;		// in Eidos for hack reasons; see EidosValue_Object::EidosValue_Object()
extern const std::string &gEidosStr_Individual;	// in Eidos for hack reasons; see EidosValue_Object::EidosValue_Object()

// the compile-time-constant IDs associated with the above strings
enum _EidosGlobalStringID : uint32_t
{
	gEidosID_none = 0,
	
	gEidosID_empty_string,
	gEidosID_space_string,

	gEidosID_apply,
	gEidosID_sapply,
	gEidosID_doCall,
	gEidosID_executeLambda,
	gEidosID__executeLambda_OUTER,
	gEidosID_ls,
	gEidosID_rm,
	gEidosID_usage,

	gEidosID_if,
	gEidosID_else,
	gEidosID_do,
	gEidosID_while,
	gEidosID_for,
	gEidosID_in,
	gEidosID_next,
	gEidosID_break,
	gEidosID_return,
	gEidosID_function,

	gEidosID_T,
	gEidosID_F,
	gEidosID_NULL,
	gEidosID_PI,
	gEidosID_E,
	gEidosID_INF,
	gEidosID_MINUS_INF,
	gEidosID_NAN,

	gEidosID_void,
	gEidosID_logical,
	gEidosID_string,
	gEidosID_integer,
	gEidosID_float,
	gEidosID_object,
	gEidosID_numeric,

	gEidosID_ELLIPSIS,
	gEidosID_type,
	gEidosID_source,
	gEidosID_GetPropertyOfElements,
	gEidosID_ExecuteInstanceMethod,
	gEidosID_undefined,
	gEidosID_applyValue,
	
	gEidosID_Object,
	gEidosID_size,
	gEidosID_length,
	gEidosID_methodSignature,
	gEidosID_propertySignature,
	gEidosID_str,
	gEidosID_stringRepresentation,

	gEidosID__TestElement,
	gEidosID__TestElementNRR,
	gEidosID__yolk,
	gEidosID__increment,
	gEidosID__cubicYolk,
	gEidosID__squareTest,

	gEidosID_DictionaryBase,
	gEidosID_allKeys,
	gEidosID_addKeysAndValuesFrom,
	gEidosID_appendKeysAndValuesFrom,
	gEidosID_clearKeysAndValues,
	gEidosID_compactIndices,
	gEidosID_getRowValues,
	gEidosID_getValue,
	gEidosID_identicalContents,
	gEidosID_serialize,
	gEidosID_setValue,

	gEidosID_Dictionary,

	gEidosID_DataFrame,
	gEidosID_colNames,
	gEidosID_dim,
	gEidosID_ncol,
	gEidosID_nrow,
	gEidosID_asMatrix,
	gEidosID_cbind,
	gEidosID_rbind,
	gEidosID_subset,
	gEidosID_subsetColumns,
	gEidosID_subsetRows,

	gEidosID_Image,
	gEidosID_width,
	gEidosID_height,
	gEidosID_bitsPerChannel,
	gEidosID_isGrayscale,
	gEidosID_integerR,
	gEidosID_integerG,
	gEidosID_integerB,
	gEidosID_integerK,
	gEidosID_floatR,
	gEidosID_floatG,
	gEidosID_floatB,
	gEidosID_floatK,
	gEidosID_write,

	gEidosID_start,
	gEidosID_end,
	gEidosID_weights,
	gEidosID_range,
	gEidosID_c,
	gEidosID_t,
	gEidosID_n,
	gEidosID_s,
	gEidosID_x,
	gEidosID_y,
	gEidosID_z,
	gEidosID_xy,
	gEidosID_xz,
	gEidosID_yz,
	gEidosID_xyz,
	gEidosID_color,
	gEidosID_filePath,

	gEidosID_Mutation,
	gEidosID_Genome,
	gEidosID_Individual,
	
	gEidosID_LastEntry,					// IDs added by the Context should start here
	gEidosID_LastContextEntry = 490		// IDs added by the Context must end before this value; Eidos reserves the remaining values
};

extern std::vector<std::string> gEidosConstantNames;	// T, F, NULL, PI, E, INF, NAN


// *******************************************************************************************************************
//
//	Support for named / specified colors in SLiM
//
#pragma mark -
#pragma mark Named/specified color support
#pragma mark -

typedef struct {
	const char *name;
	uint8_t red, green, blue;
} EidosNamedColor;

extern EidosNamedColor gEidosNamedColors[];

void Eidos_GetColorComponents(const std::string &p_color_name, float *p_red_component, float *p_green_component, float *p_blue_component);
void Eidos_GetColorComponents(const std::string &p_color_name, uint8_t *p_red_component, uint8_t *p_green_component, uint8_t *p_blue_component);

void Eidos_GetColorString(double p_red, double p_green, double p_blue, char *p_string_buffer);		// p_string_buffer must have room for 8 chars, including the null
void Eidos_GetColorString(uint8_t p_red, uint8_t p_green, uint8_t p_blue, char *p_string_buffer);	// p_string_buffer must have room for 8 chars, including the null

void Eidos_HSV2RGB(double h, double s, double v, double *p_r, double *p_g, double *p_b);
void Eidos_RGB2HSV(double r, double g, double b, double *p_h, double *p_s, double *p_v);

enum class EidosColorPalette : int
{
	kPalette_cm = 0,
	kPalette_heat,
	kPalette_terrain,
	kPalette_parula,
	kPalette_hot,
	kPalette_jet,
	kPalette_turbo,
	kPalette_gray,
	kPalette_magma,
	kPalette_inferno,
	kPalette_plasma,
	kPalette_viridis,
	kPalette_cividis,
};

void Eidos_ColorPaletteLookup(double fraction, EidosColorPalette palette, double &r, double &g, double &b);


// *******************************************************************************************************************
//
//	Basic EidosValue stuff (not in eidos_value.h to break circular header problems)
//
#pragma mark -
#pragma mark Basic EidosValue stuff
#pragma mark -

class EidosValue;
class EidosValue_VOID;
class EidosValue_NULL;
class EidosValue_Logical;
class EidosValue_Logical_const;
class EidosValue_Int;
class EidosValue_Int_singleton;
class EidosValue_Int_vector;
class EidosValue_Float;
class EidosValue_Float_singleton;
class EidosValue_Float_vector;
class EidosValue_String;
class EidosValue_String_singleton;
class EidosValue_String_vector;
class EidosValue_Object;
class EidosValue_Object_singleton;
class EidosValue_Object_vector;

class EidosObjectPool;
class EidosPropertySignature;
class EidosMethodSignature;
class EidosInstanceMethodSignature;
class EidosInterpreter;
class EidosToken;

class EidosObject;	// the value type for EidosValue_Object
class EidosClass;		// the class definition object for EidosObject


// Type int64_t is used for Eidos "integer", type double is used for Eidos "float", type std::string is used
// for Eidos "string", and EidosObject* is used for Eidos "object".  The type used for Eidos "logical"
// is a bit less clear, and so is controlled by a typedef here.  Type bool would be the obvious choice, but
// std::vector<bool> is a special class that may not be desirable; it generally encapsulates a priority for
// small memory usage over high speed, which is not our priority.  The measured speed difference is not large,
// about 4% for fairly logical-intensive tests, but the memory penalty seems irrelevant, so why not.  If you
// feel differently, switching the typedef below should cleanly switch over to bool instead of uint8_t.
typedef uint8_t eidos_logical_t;
//typedef bool eidos_logical_t;


// We use Eidos_intrusive_ptr to refer to most EidosValue instances, unless they are used only in one place with
// a single owner.  For convenience, there is a typedef for Eidos_intrusive_ptr for each EidosValue subclass.
typedef Eidos_intrusive_ptr<EidosValue>						EidosValue_SP;
typedef Eidos_intrusive_ptr<EidosValue_VOID>				EidosValue_VOID_SP;
typedef Eidos_intrusive_ptr<EidosValue_NULL>				EidosValue_NULL_SP;
typedef Eidos_intrusive_ptr<EidosValue_Logical>				EidosValue_Logical_SP;
typedef Eidos_intrusive_ptr<EidosValue_Logical_const>		EidosValue_Logical_const_SP;
typedef Eidos_intrusive_ptr<EidosValue_Int>					EidosValue_Int_SP;
typedef Eidos_intrusive_ptr<EidosValue_Int_singleton>		EidosValue_Int_singleton_SP;
typedef Eidos_intrusive_ptr<EidosValue_Int_vector>			EidosValue_Int_vector_SP;
typedef Eidos_intrusive_ptr<EidosValue_Float>				EidosValue_Float_SP;
typedef Eidos_intrusive_ptr<EidosValue_Float_singleton>		EidosValue_Float_singleton_SP;
typedef Eidos_intrusive_ptr<EidosValue_Float_vector>		EidosValue_Float_vector_SP;
typedef Eidos_intrusive_ptr<EidosValue_String>				EidosValue_String_SP;
typedef Eidos_intrusive_ptr<EidosValue_String_singleton>	EidosValue_String_singleton_SP;
typedef Eidos_intrusive_ptr<EidosValue_String_vector>		EidosValue_String_vector_SP;
typedef Eidos_intrusive_ptr<EidosValue_Object>				EidosValue_Object_SP;
typedef Eidos_intrusive_ptr<EidosValue_Object_singleton>	EidosValue_Object_singleton_SP;
typedef Eidos_intrusive_ptr<EidosValue_Object_vector>		EidosValue_Object_vector_SP;


// EidosValueType is an enum of the possible types for EidosValue objects.  Note that all of these types are vectors of the stated
// type; all objects in Eidos are vectors.  The order of these types is in type-promotion order, from lowest to highest, except
// that NULL never gets promoted to any other type, and nothing ever gets promoted to object type.
enum class EidosValueType : uint8_t
{
	kValueVOID = 0,		// special void type; this cannot be mixed with other types or promoted to other types, cannot be passed to a function/method, cannot be assigned to a variable
	kValueNULL,			// special NULL type; this cannot be mixed with other types or promoted to other types
	
	kValueLogical,		// logicals (eidos_logical_t)
	kValueInt,			// integers (int64_t)
	kValueFloat,		// floats (double)
	kValueString,		// strings (std:string)
	
	kValueObject		// a vector of EidosObject objects: these represent built-in objects with properties and methods
};

std::string StringForEidosValueType(const EidosValueType p_type);
std::ostream &operator<<(std::ostream &p_outstream, const EidosValueType p_type);


// EidosValueMask is a uint32_t used as a bit mask to identify permitted types for EidosValue objects (arguments, returns)
// Note that these mask values must correspond to the values in EidosValueType directly; (1 << (int)type) == mask must be true.
typedef uint32_t EidosValueMask;

const EidosValueMask kEidosValueMaskNone =			0x00000000;
const EidosValueMask kEidosValueMaskVOID =			0x00000001;
const EidosValueMask kEidosValueMaskNULL =			0x00000002;
const EidosValueMask kEidosValueMaskLogical =		0x00000004;
const EidosValueMask kEidosValueMaskInt =			0x00000008;
const EidosValueMask kEidosValueMaskFloat =			0x00000010;
const EidosValueMask kEidosValueMaskString =		0x00000020;
const EidosValueMask kEidosValueMaskObject =		0x00000040;

const EidosValueMask kEidosValueMaskOptional =		0x80000000;
const EidosValueMask kEidosValueMaskSingleton =		0x40000000;
const EidosValueMask kEidosValueMaskFlagStrip =		0x3FFFFFFF;

const EidosValueMask kEidosValueMaskNumeric =		(kEidosValueMaskInt | kEidosValueMaskFloat);									// integer or float
const EidosValueMask kEidosValueMaskLogicalEquiv =	(kEidosValueMaskLogical | kEidosValueMaskInt | kEidosValueMaskFloat);			// logical, integer, or float
const EidosValueMask kEidosValueMaskAnyBase =		(kEidosValueMaskNULL | kEidosValueMaskLogicalEquiv | kEidosValueMaskString);	// any non-void type except object
const EidosValueMask kEidosValueMaskAny =			(kEidosValueMaskAnyBase | kEidosValueMaskObject);								// any non-void type including object

std::string StringForEidosValueMask(const EidosValueMask p_mask, const EidosClass *p_object_class, const std::string &p_name, EidosValue *p_default);
//std::ostream &operator<<(std::ostream &p_outstream, const EidosValueMask p_mask);	// can't do this since EidosValueMask is just uint32_t


// EidosTypeSpecifier is a struct that provides a full type specifier, including an optional object class, for a value
typedef struct {
	EidosValueMask type_mask;					// can specify kEidosValueMaskNone for no known type, or any combination of type masks
	const EidosClass *object_class;		// if kEidosValueMaskObject is included in type_mask, this can specify a class (or can be nullptr)
} EidosTypeSpecifier;





#endif /* defined(__Eidos__eidos_globals__) */



















































