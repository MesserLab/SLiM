//
//  sleef_config.h
//  Eidos
//
//  Created by Andrew Kern on 12/14/2025.
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

 SLEEF (SIMD Library for Evaluating Elementary Functions) configuration header.

 This header provides architecture-specific macros for using SLEEF inline headers
 to vectorize transcendental math functions (exp, log, log10, log2).

 SLEEF is used under the Boost Software License - see sleef/LICENSE.

 Architecture support:
   - AVX2+FMA (Intel Haswell+, AMD Zen+): 4-wide double vectorization
   - ARM NEON (Apple Silicon, ARM64 Linux): 2-wide double vectorization
   - SSE4.2-only / No SIMD: Falls back to scalar std:: functions

 */

#ifndef sleef_config_h
#define sleef_config_h

// ================================
// AVX2+FMA Configuration (x86_64)
// ================================
#if defined(EIDOS_HAS_AVX2) && defined(EIDOS_HAS_FMA)
    #include <immintrin.h>
    #include "sleefinline_avx2.h"

    #define EIDOS_SLEEF_AVAILABLE 1
    #define EIDOS_SLEEF_VEC_SIZE 4

    // Type definitions
    #define EIDOS_SLEEF_TYPE_D __m256d

    // Load/Store operations
    #define EIDOS_SLEEF_LOAD_D(ptr) _mm256_loadu_pd(ptr)
    #define EIDOS_SLEEF_STORE_D(ptr, v) _mm256_storeu_pd(ptr, v)

    // Transcendental functions (u10 = 1.0 ULP accuracy)
    #define EIDOS_SLEEF_EXP_D(v) Sleef_expd4_u10avx2(v)
    #define EIDOS_SLEEF_LOG_D(v) Sleef_logd4_u10avx2(v)
    #define EIDOS_SLEEF_LOG10_D(v) Sleef_log10d4_u10avx2(v)
    #define EIDOS_SLEEF_LOG2_D(v) Sleef_log2d4_u10avx2(v)

// ================================
// ARM NEON Configuration (ARM64)
// ================================
#elif defined(EIDOS_HAS_NEON)
    #include <arm_neon.h>
    // Note: sleefinline_advsimd.h needs to be generated on ARM and patched
    // #include "sleefinline_advsimd.h"

    // For now, fall back to scalar until ARM header is available
    #define EIDOS_SLEEF_AVAILABLE 0
    #define EIDOS_SLEEF_VEC_SIZE 1

    // Placeholder for when ARM header is added:
    // #define EIDOS_SLEEF_AVAILABLE 1
    // #define EIDOS_SLEEF_VEC_SIZE 2
    // #define EIDOS_SLEEF_TYPE_D float64x2_t
    // #define EIDOS_SLEEF_LOAD_D(ptr) vld1q_f64(ptr)
    // #define EIDOS_SLEEF_STORE_D(ptr, v) vst1q_f64(ptr, v)
    // #define EIDOS_SLEEF_EXP_D(v) Sleef_expd2_u10advsimd(v)
    // #define EIDOS_SLEEF_LOG_D(v) Sleef_logd2_u10advsimd(v)
    // #define EIDOS_SLEEF_LOG10_D(v) Sleef_log10d2_u10advsimd(v)
    // #define EIDOS_SLEEF_LOG2_D(v) Sleef_log2d2_u10advsimd(v)

// ================================
// Scalar Fallback (SSE4.2-only or no SIMD)
// ================================
#else
    #define EIDOS_SLEEF_AVAILABLE 0
    #define EIDOS_SLEEF_VEC_SIZE 1
#endif

#endif /* sleef_config_h */
