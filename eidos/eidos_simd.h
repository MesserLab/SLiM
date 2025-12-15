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

 SIMD acceleration for Eidos math operations, independent of OpenMP.

 This header provides vectorized implementations of common math operations
 using platform-specific SIMD intrinsics when available:
   - x86_64: SSE4.2 or AVX2 via <immintrin.h>
   - ARM64: NEON via <arm_neon.h>
 Falls back to scalar code when no SIMD is available.

 */

#ifndef eidos_simd_h
#define eidos_simd_h

#include <cstdint>
#include <cmath>

// Determine SIMD capability level
#if defined(EIDOS_HAS_AVX2)
    #include <immintrin.h>
    #define EIDOS_SIMD_WIDTH 4          // 4 doubles per AVX register
    #define EIDOS_SIMD_FLOAT_WIDTH 8    // 8 floats per AVX register
#elif defined(EIDOS_HAS_SSE42)
    #include <emmintrin.h>
    #include <smmintrin.h>
    #define EIDOS_SIMD_WIDTH 2          // 2 doubles per SSE register
    #define EIDOS_SIMD_FLOAT_WIDTH 4    // 4 floats per SSE register
#elif defined(EIDOS_HAS_NEON)
    #include <arm_neon.h>
    #define EIDOS_SIMD_WIDTH 2          // 2 doubles per NEON register
    #define EIDOS_SIMD_FLOAT_WIDTH 4    // 4 floats per NEON register
#else
    #define EIDOS_SIMD_WIDTH 1          // Scalar fallback
    #define EIDOS_SIMD_FLOAT_WIDTH 1
#endif

// Include SLEEF for vectorized transcendental functions (exp, log, log10, log2)
// SLEEF is only used when AVX2+FMA or NEON is available
#if defined(EIDOS_HAS_AVX2) || defined(EIDOS_HAS_NEON)
#include "sleef/sleef_config.h"
#endif

// ================================
// SIMD Vector Math Operations
// ================================
// These functions apply an operation to arrays of doubles.
// They handle the loop, SIMD processing, and scalar remainder.

namespace Eidos_SIMD {

// ---------------------
// Square Root: sqrt(x)
// ---------------------
inline void sqrt_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if defined(EIDOS_HAS_AVX2)
    // Process 4 doubles at a time
    for (; i + 4 <= count; i += 4)
    {
        __m256d v = _mm256_loadu_pd(&input[i]);
        __m256d r = _mm256_sqrt_pd(v);
        _mm256_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_SSE42)
    // Process 2 doubles at a time
    for (; i + 2 <= count; i += 2)
    {
        __m128d v = _mm_loadu_pd(&input[i]);
        __m128d r = _mm_sqrt_pd(v);
        _mm_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_NEON)
    // Process 2 doubles at a time
    for (; i + 2 <= count; i += 2)
    {
        float64x2_t v = vld1q_f64(&input[i]);
        float64x2_t r = vsqrtq_f64(v);
        vst1q_f64(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::sqrt(input[i]);
}

// ---------------------
// Absolute Value: abs(x)
// ---------------------
inline void abs_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if defined(EIDOS_HAS_AVX2)
    // Create sign mask (all bits except sign bit)
    __m256d sign_mask = _mm256_set1_pd(-0.0);
    for (; i + 4 <= count; i += 4)
    {
        __m256d v = _mm256_loadu_pd(&input[i]);
        __m256d r = _mm256_andnot_pd(sign_mask, v);  // Clear sign bit
        _mm256_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_SSE42)
    __m128d sign_mask = _mm_set1_pd(-0.0);
    for (; i + 2 <= count; i += 2)
    {
        __m128d v = _mm_loadu_pd(&input[i]);
        __m128d r = _mm_andnot_pd(sign_mask, v);
        _mm_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_NEON)
    for (; i + 2 <= count; i += 2)
    {
        float64x2_t v = vld1q_f64(&input[i]);
        float64x2_t r = vabsq_f64(v);
        vst1q_f64(&output[i], r);
    }
#endif

    for (; i < count; i++)
        output[i] = std::fabs(input[i]);
}

// ---------------------
// Floor: floor(x)
// ---------------------
inline void floor_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if defined(EIDOS_HAS_AVX2)
    for (; i + 4 <= count; i += 4)
    {
        __m256d v = _mm256_loadu_pd(&input[i]);
        __m256d r = _mm256_floor_pd(v);
        _mm256_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_SSE42)
    for (; i + 2 <= count; i += 2)
    {
        __m128d v = _mm_loadu_pd(&input[i]);
        __m128d r = _mm_floor_pd(v);
        _mm_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_NEON)
    for (; i + 2 <= count; i += 2)
    {
        float64x2_t v = vld1q_f64(&input[i]);
        float64x2_t r = vrndmq_f64(v);  // Round toward minus infinity (floor)
        vst1q_f64(&output[i], r);
    }
#endif

    for (; i < count; i++)
        output[i] = std::floor(input[i]);
}

// ---------------------
// Ceil: ceil(x)
// ---------------------
inline void ceil_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if defined(EIDOS_HAS_AVX2)
    for (; i + 4 <= count; i += 4)
    {
        __m256d v = _mm256_loadu_pd(&input[i]);
        __m256d r = _mm256_ceil_pd(v);
        _mm256_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_SSE42)
    for (; i + 2 <= count; i += 2)
    {
        __m128d v = _mm_loadu_pd(&input[i]);
        __m128d r = _mm_ceil_pd(v);
        _mm_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_NEON)
    for (; i + 2 <= count; i += 2)
    {
        float64x2_t v = vld1q_f64(&input[i]);
        float64x2_t r = vrndpq_f64(v);  // Round toward plus infinity (ceil)
        vst1q_f64(&output[i], r);
    }
#endif

    for (; i < count; i++)
        output[i] = std::ceil(input[i]);
}

// ---------------------
// Truncate: trunc(x)
// ---------------------
inline void trunc_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if defined(EIDOS_HAS_AVX2)
    for (; i + 4 <= count; i += 4)
    {
        __m256d v = _mm256_loadu_pd(&input[i]);
        __m256d r = _mm256_round_pd(v, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
        _mm256_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_SSE42)
    for (; i + 2 <= count; i += 2)
    {
        __m128d v = _mm_loadu_pd(&input[i]);
        __m128d r = _mm_round_pd(v, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
        _mm_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_NEON)
    for (; i + 2 <= count; i += 2)
    {
        float64x2_t v = vld1q_f64(&input[i]);
        float64x2_t r = vrndq_f64(v);  // Round toward zero (truncate)
        vst1q_f64(&output[i], r);
    }
#endif

    for (; i < count; i++)
        output[i] = std::trunc(input[i]);
}

// ---------------------
// Round: round(x)
// ---------------------
inline void round_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if defined(EIDOS_HAS_AVX2)
    for (; i + 4 <= count; i += 4)
    {
        __m256d v = _mm256_loadu_pd(&input[i]);
        __m256d r = _mm256_round_pd(v, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
        _mm256_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_SSE42)
    for (; i + 2 <= count; i += 2)
    {
        __m128d v = _mm_loadu_pd(&input[i]);
        __m128d r = _mm_round_pd(v, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
        _mm_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_NEON)
    for (; i + 2 <= count; i += 2)
    {
        float64x2_t v = vld1q_f64(&input[i]);
        float64x2_t r = vrndaq_f64(v);  // Round to nearest, ties away from zero
        vst1q_f64(&output[i], r);
    }
#endif

    for (; i < count; i++)
        output[i] = std::round(input[i]);
}

// ---------------------
// Exponential: exp(x)
// ---------------------
inline void exp_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_AVAILABLE
    for (; i + EIDOS_SLEEF_VEC_SIZE <= count; i += EIDOS_SLEEF_VEC_SIZE)
    {
        EIDOS_SLEEF_TYPE_D v = EIDOS_SLEEF_LOAD_D(&input[i]);
        EIDOS_SLEEF_TYPE_D r = EIDOS_SLEEF_EXP_D(v);
        EIDOS_SLEEF_STORE_D(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::exp(input[i]);
}

// ---------------------
// Natural Log: log(x)
// ---------------------
inline void log_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_AVAILABLE
    for (; i + EIDOS_SLEEF_VEC_SIZE <= count; i += EIDOS_SLEEF_VEC_SIZE)
    {
        EIDOS_SLEEF_TYPE_D v = EIDOS_SLEEF_LOAD_D(&input[i]);
        EIDOS_SLEEF_TYPE_D r = EIDOS_SLEEF_LOG_D(v);
        EIDOS_SLEEF_STORE_D(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::log(input[i]);
}

// ---------------------
// Log base 10: log10(x)
// ---------------------
inline void log10_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_AVAILABLE
    for (; i + EIDOS_SLEEF_VEC_SIZE <= count; i += EIDOS_SLEEF_VEC_SIZE)
    {
        EIDOS_SLEEF_TYPE_D v = EIDOS_SLEEF_LOAD_D(&input[i]);
        EIDOS_SLEEF_TYPE_D r = EIDOS_SLEEF_LOG10_D(v);
        EIDOS_SLEEF_STORE_D(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::log10(input[i]);
}

// ---------------------
// Log base 2: log2(x)
// ---------------------
inline void log2_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_AVAILABLE
    for (; i + EIDOS_SLEEF_VEC_SIZE <= count; i += EIDOS_SLEEF_VEC_SIZE)
    {
        EIDOS_SLEEF_TYPE_D v = EIDOS_SLEEF_LOAD_D(&input[i]);
        EIDOS_SLEEF_TYPE_D r = EIDOS_SLEEF_LOG2_D(v);
        EIDOS_SLEEF_STORE_D(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::log2(input[i]);
}

// ================================
// Reductions
// ================================

// ---------------------
// Sum: sum(x)
// ---------------------
inline double sum_float64(const double *input, int64_t count)
{
    double sum = 0.0;
    int64_t i = 0;

#if defined(EIDOS_HAS_AVX2)
    __m256d vsum = _mm256_setzero_pd();
    for (; i + 4 <= count; i += 4)
    {
        __m256d v = _mm256_loadu_pd(&input[i]);
        vsum = _mm256_add_pd(vsum, v);
    }
    // Horizontal sum of 4 doubles
    __m128d vlow  = _mm256_castpd256_pd128(vsum);
    __m128d vhigh = _mm256_extractf128_pd(vsum, 1);
    vlow  = _mm_add_pd(vlow, vhigh);     // 2 doubles
    __m128d shuf = _mm_shuffle_pd(vlow, vlow, 1);
    vlow = _mm_add_sd(vlow, shuf);       // 1 double
    sum = _mm_cvtsd_f64(vlow);
#elif defined(EIDOS_HAS_SSE42)
    __m128d vsum = _mm_setzero_pd();
    for (; i + 2 <= count; i += 2)
    {
        __m128d v = _mm_loadu_pd(&input[i]);
        vsum = _mm_add_pd(vsum, v);
    }
    // Horizontal sum of 2 doubles
    __m128d shuf = _mm_shuffle_pd(vsum, vsum, 1);
    vsum = _mm_add_sd(vsum, shuf);
    sum = _mm_cvtsd_f64(vsum);
#elif defined(EIDOS_HAS_NEON)
    float64x2_t vsum = vdupq_n_f64(0.0);
    for (; i + 2 <= count; i += 2)
    {
        float64x2_t v = vld1q_f64(&input[i]);
        vsum = vaddq_f64(vsum, v);
    }
    // Horizontal sum of 2 doubles
    sum = vaddvq_f64(vsum);
#endif

    // Scalar remainder
    for (; i < count; i++)
        sum += input[i];

    return sum;
}

// ---------------------
// Product: product(x)
// ---------------------
inline double product_float64(const double *input, int64_t count)
{
    double prod = 1.0;
    int64_t i = 0;

#if defined(EIDOS_HAS_AVX2)
    __m256d vprod = _mm256_set1_pd(1.0);
    for (; i + 4 <= count; i += 4)
    {
        __m256d v = _mm256_loadu_pd(&input[i]);
        vprod = _mm256_mul_pd(vprod, v);
    }
    // Horizontal product of 4 doubles
    __m128d vlow  = _mm256_castpd256_pd128(vprod);
    __m128d vhigh = _mm256_extractf128_pd(vprod, 1);
    vlow  = _mm_mul_pd(vlow, vhigh);
    __m128d shuf = _mm_shuffle_pd(vlow, vlow, 1);
    vlow = _mm_mul_sd(vlow, shuf);
    prod = _mm_cvtsd_f64(vlow);
#elif defined(EIDOS_HAS_SSE42)
    __m128d vprod = _mm_set1_pd(1.0);
    for (; i + 2 <= count; i += 2)
    {
        __m128d v = _mm_loadu_pd(&input[i]);
        vprod = _mm_mul_pd(vprod, v);
    }
    __m128d shuf = _mm_shuffle_pd(vprod, vprod, 1);
    vprod = _mm_mul_sd(vprod, shuf);
    prod = _mm_cvtsd_f64(vprod);
#elif defined(EIDOS_HAS_NEON)
    float64x2_t vprod = vdupq_n_f64(1.0);
    for (; i + 2 <= count; i += 2)
    {
        float64x2_t v = vld1q_f64(&input[i]);
        vprod = vmulq_f64(vprod, v);
    }
    // Horizontal product of 2 doubles
    prod = vgetq_lane_f64(vprod, 0) * vgetq_lane_f64(vprod, 1);
#endif

    for (; i < count; i++)
        prod *= input[i];

    return prod;
}

} // namespace Eidos_SIMD

#endif /* eidos_simd_h */
