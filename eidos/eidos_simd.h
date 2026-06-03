//
//  eidos_simd.h
//  Eidos
//
//  Created by Andrew Kern on 11/26/2025.
//  Copyright (c) 2024-2025 Benjamin C. Haller.  All rights reserved.
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

 SIMD acceleration for Eidos math operations, with runtime CPU dispatch.

 This header is the public interface to a set of vectorized array-math kernels
 (sqrt, exp, the spatial-interaction kernels, etc.).  Each kernel is exposed as
 a function pointer in namespace Eidos_SIMD; callers simply write, e.g.,
 Eidos_SIMD::sqrt_float64(in, out, n) exactly as if it were a function.

 How the dispatch works, and why
 -------------------------------
 The kernels are compiled once per instruction-set "tier":

   - scalar       portable C++ that runs on any CPU
   - SSE4.2       x86_64-v2, present on essentially every x86_64 CPU (~2009+)
   - AVX2 + FMA   x86_64-v3, Haswell / Excavator and later (~2013+)
   - NEON         baseline on every ARM64 CPU

 Each tier lives in its own translation unit (eidos_simd_scalar.cpp,
 eidos_simd_sse42.cpp, eidos_simd_avx2.cpp, eidos_simd_neon.cpp); only those
 translation units are built with instruction-set-specific compiler flags.
 Every other translation unit in SLiM, and the dispatcher itself, is compiled
 at the plain x86_64 (SSE2) baseline.  At startup Eidos_SIMD_Init() probes the
 CPU with __builtin_cpu_supports() and points the Eidos_SIMD function pointers
 at the fastest tier the hardware actually supports.

 The point of this design is that a single binary is correct everywhere: the
 bulk of the executable contains only baseline instructions, so it cannot fault
 on an old CPU, while the AVX2/SSE4.2 code is reached only after the CPU has
 been confirmed to support it.  Previously SLiM was built with -mavx2 applied
 globally, which let the compiler emit AVX2 instructions throughout the whole
 binary and caused SIGILL crashes on pre-AVX2 hardware (issue #628).

 Building with -D USE_SIMD=OFF defines EIDOS_SUPPRESS_SIMD_DISPATCH, which
 forces the scalar tier and omits all instruction-set-specific code.

 */

#ifndef eidos_simd_h
#define eidos_simd_h

#include <cstdint>

// EIDOS_SIMD_DISPATCH_X86 / _ARM are defined when this build includes a
// hardware-specific tier beyond scalar.  Runtime dispatch needs both a
// supported compiler (for __builtin_cpu_supports and target attributes /
// per-file ISA flags) and a known architecture.  When USE_SIMD=OFF, neither
// is defined and only the scalar tier exists.
#if !defined(EIDOS_SUPPRESS_SIMD_DISPATCH) && (defined(__GNUC__) || defined(__clang__))
	#if defined(__x86_64__) || defined(__i386__)
		#define EIDOS_SIMD_DISPATCH_X86 1
	#elif defined(__aarch64__) || defined(__arm64__)
		#define EIDOS_SIMD_DISPATCH_ARM 1
	#endif
#endif

// The complete set of dispatched SIMD kernels, as X-macro entries of the form
// X(return_type, name, (parameter_types)).  This single list drives the
// function-pointer declarations, their definitions, and the per-tier dispatch
// tables, so a kernel is added or removed in exactly one place.
#define EIDOS_SIMD_FUNCTION_TABLE \
	X(void,   sqrt_float64,                  (const double *, double *, int64_t)) \
	X(void,   abs_float64,                   (const double *, double *, int64_t)) \
	X(void,   floor_float64,                 (const double *, double *, int64_t)) \
	X(void,   ceil_float64,                  (const double *, double *, int64_t)) \
	X(void,   trunc_float64,                 (const double *, double *, int64_t)) \
	X(void,   round_float64,                 (const double *, double *, int64_t)) \
	X(void,   exp_float64,                   (const double *, double *, int64_t)) \
	X(void,   log_float64,                   (const double *, double *, int64_t)) \
	X(void,   log10_float64,                 (const double *, double *, int64_t)) \
	X(void,   log2_float64,                  (const double *, double *, int64_t)) \
	X(void,   sin_float64,                   (const double *, double *, int64_t)) \
	X(void,   cos_float64,                   (const double *, double *, int64_t)) \
	X(void,   tan_float64,                   (const double *, double *, int64_t)) \
	X(void,   asin_float64,                  (const double *, double *, int64_t)) \
	X(void,   acos_float64,                  (const double *, double *, int64_t)) \
	X(void,   atan_float64,                  (const double *, double *, int64_t)) \
	X(void,   atan2_float64,                 (const double *, const double *, double *, int64_t)) \
	X(void,   pow_float64,                   (const double *, const double *, double *, int64_t)) \
	X(void,   pow_float64_scalar_exp,        (const double *, double, double *, int64_t)) \
	X(void,   pow_float64_scalar_base,       (double, const double *, double *, int64_t)) \
	X(double, sum_float64,                   (const double *, int64_t)) \
	X(double, product_float64,               (const double *, int64_t)) \
	X(void,   exp_float32,                   (const float *, float *, int64_t)) \
	X(void,   exp_kernel_float32,            (float *, int64_t, float, float)) \
	X(void,   normal_kernel_float32,         (float *, int64_t, float, float)) \
	X(void,   tdist_kernel_float32,          (float *, int64_t, float, float, float)) \
	X(void,   cauchy_kernel_float32,         (float *, int64_t, float, float)) \
	X(void,   linear_kernel_float32,         (float *, int64_t, float, float)) \
	X(void,   convolve_dot_product_float64,        (const double *, const double *, int64_t, double &, double &)) \
	X(void,   convolve_dot_product_scaled_float64, (const double *, const double *, int64_t, double, double &, double &))

// The public kernels: function pointers set by Eidos_SIMD_Init() to point at
// the tier selected for the running CPU.  Call them as Eidos_SIMD::name(...).
namespace Eidos_SIMD {
#define X(ret, name, params) extern ret (*name) params;
EIDOS_SIMD_FUNCTION_TABLE
#undef X
}

// Declarations of the scalar-tier kernels.  These are the universal fallback;
// the function pointers above are statically initialized to point here, so a
// call is always valid even before Eidos_SIMD_Init() has run.  The scalar tier
// is defined in eidos_simd_scalar.cpp and built on every platform.
namespace Eidos_SIMD_scalar {
#define X(ret, name, params) ret name params;
EIDOS_SIMD_FUNCTION_TABLE
#undef X
}

// Each tier's translation unit defines a fill function that points the
// Eidos_SIMD pointers at that tier's kernels.  Eidos_SIMD_Init() calls the
// appropriate one based on runtime CPU detection.
void Eidos_SIMD_Fill_scalar(void);
#if EIDOS_SIMD_DISPATCH_X86
void Eidos_SIMD_Fill_sse42(void);
void Eidos_SIMD_Fill_avx2(void);
#endif
#if EIDOS_SIMD_DISPATCH_ARM
void Eidos_SIMD_Fill_neon(void);
#endif

// Detect the CPU and select the fastest available SIMD tier.  This must be
// called once, early in startup, before any Eidos_SIMD kernel is used;
// Eidos_WarmUp() does this.  It is idempotent.
void Eidos_SIMD_Init(void);

// Force a specific SIMD tier by name ("scalar", "SSE4.2", "AVX2+FMA", "NEON")
// instead of the auto-selected best tier.  Returns false, leaving the active
// tier unchanged, if the named tier is unavailable on this CPU or in this
// build.  Used by the SIMD self-tests to exercise every tier; calling
// Eidos_SIMD_Init() afterward restores the best tier for the CPU.
bool Eidos_SIMD_SelectTier(const char *tier_name);

// The name of the SIMD tier selected at startup: "scalar", "SSE4.2",
// "AVX2+FMA", or "NEON".  Valid only after Eidos_SIMD_Init() has run.
const char *Eidos_SIMD_ActiveTierName(void);

// True if the active tier uses SLEEF for vectorized transcendental functions
// (exp, log, sin, ...).  SLEEF is wired up for the AVX2+FMA and NEON tiers;
// the scalar and SSE4.2 tiers use std:: transcendentals.
bool Eidos_SIMD_SLEEFActive(void);

#endif /* eidos_simd_h */
