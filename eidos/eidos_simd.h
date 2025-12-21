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

// ---------------------
// Sine: sin(x)
// ---------------------
inline void sin_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_AVAILABLE
    for (; i + EIDOS_SLEEF_VEC_SIZE <= count; i += EIDOS_SLEEF_VEC_SIZE)
    {
        EIDOS_SLEEF_TYPE_D v = EIDOS_SLEEF_LOAD_D(&input[i]);
        EIDOS_SLEEF_TYPE_D r = EIDOS_SLEEF_SIN_D(v);
        EIDOS_SLEEF_STORE_D(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::sin(input[i]);
}

// ---------------------
// Cosine: cos(x)
// ---------------------
inline void cos_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_AVAILABLE
    for (; i + EIDOS_SLEEF_VEC_SIZE <= count; i += EIDOS_SLEEF_VEC_SIZE)
    {
        EIDOS_SLEEF_TYPE_D v = EIDOS_SLEEF_LOAD_D(&input[i]);
        EIDOS_SLEEF_TYPE_D r = EIDOS_SLEEF_COS_D(v);
        EIDOS_SLEEF_STORE_D(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::cos(input[i]);
}

// ---------------------
// Tangent: tan(x)
// ---------------------
inline void tan_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_AVAILABLE
    for (; i + EIDOS_SLEEF_VEC_SIZE <= count; i += EIDOS_SLEEF_VEC_SIZE)
    {
        EIDOS_SLEEF_TYPE_D v = EIDOS_SLEEF_LOAD_D(&input[i]);
        EIDOS_SLEEF_TYPE_D r = EIDOS_SLEEF_TAN_D(v);
        EIDOS_SLEEF_STORE_D(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::tan(input[i]);
}

// ---------------------
// Arc Sine: asin(x)
// ---------------------
inline void asin_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_AVAILABLE
    for (; i + EIDOS_SLEEF_VEC_SIZE <= count; i += EIDOS_SLEEF_VEC_SIZE)
    {
        EIDOS_SLEEF_TYPE_D v = EIDOS_SLEEF_LOAD_D(&input[i]);
        EIDOS_SLEEF_TYPE_D r = EIDOS_SLEEF_ASIN_D(v);
        EIDOS_SLEEF_STORE_D(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::asin(input[i]);
}

// ---------------------
// Arc Cosine: acos(x)
// ---------------------
inline void acos_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_AVAILABLE
    for (; i + EIDOS_SLEEF_VEC_SIZE <= count; i += EIDOS_SLEEF_VEC_SIZE)
    {
        EIDOS_SLEEF_TYPE_D v = EIDOS_SLEEF_LOAD_D(&input[i]);
        EIDOS_SLEEF_TYPE_D r = EIDOS_SLEEF_ACOS_D(v);
        EIDOS_SLEEF_STORE_D(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::acos(input[i]);
}

// ---------------------
// Arc Tangent: atan(x)
// ---------------------
inline void atan_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_AVAILABLE
    for (; i + EIDOS_SLEEF_VEC_SIZE <= count; i += EIDOS_SLEEF_VEC_SIZE)
    {
        EIDOS_SLEEF_TYPE_D v = EIDOS_SLEEF_LOAD_D(&input[i]);
        EIDOS_SLEEF_TYPE_D r = EIDOS_SLEEF_ATAN_D(v);
        EIDOS_SLEEF_STORE_D(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::atan(input[i]);
}

// ---------------------
// Arc Tangent 2: atan2(y, x)
// ---------------------
inline void atan2_float64(const double *y, const double *x, double *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_AVAILABLE
    for (; i + EIDOS_SLEEF_VEC_SIZE <= count; i += EIDOS_SLEEF_VEC_SIZE)
    {
        EIDOS_SLEEF_TYPE_D vy = EIDOS_SLEEF_LOAD_D(&y[i]);
        EIDOS_SLEEF_TYPE_D vx = EIDOS_SLEEF_LOAD_D(&x[i]);
        EIDOS_SLEEF_TYPE_D r = EIDOS_SLEEF_ATAN2_D(vy, vx);
        EIDOS_SLEEF_STORE_D(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::atan2(y[i], x[i]);
}

// ---------------------
// Power: pow(x, y) = x^y
// ---------------------
inline void pow_float64(const double *base, const double *exp, double *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_AVAILABLE
    for (; i + EIDOS_SLEEF_VEC_SIZE <= count; i += EIDOS_SLEEF_VEC_SIZE)
    {
        EIDOS_SLEEF_TYPE_D vb = EIDOS_SLEEF_LOAD_D(&base[i]);
        EIDOS_SLEEF_TYPE_D ve = EIDOS_SLEEF_LOAD_D(&exp[i]);
        EIDOS_SLEEF_TYPE_D r = EIDOS_SLEEF_POW_D(vb, ve);
        EIDOS_SLEEF_STORE_D(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::pow(base[i], exp[i]);
}

// Broadcast version: all elements raised to same power (base_array ^ scalar_exp)
inline void pow_float64_scalar_exp(const double *base, double exp_scalar, double *output, int64_t count)
{
    int64_t i = 0;

#if defined(EIDOS_HAS_AVX2) && defined(EIDOS_HAS_FMA)
    __m256d ve_broadcast = _mm256_set1_pd(exp_scalar);
    for (; i + 4 <= count; i += 4)
    {
        __m256d vb = _mm256_loadu_pd(&base[i]);
        __m256d r = Sleef_powd4_u10avx2(vb, ve_broadcast);
        _mm256_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_NEON)
    float64x2_t ve_broadcast = vdupq_n_f64(exp_scalar);
    for (; i + 2 <= count; i += 2)
    {
        float64x2_t vb = vld1q_f64(&base[i]);
        float64x2_t r = Sleef_powd2_u10advsimd(vb, ve_broadcast);
        vst1q_f64(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::pow(base[i], exp_scalar);
}

// Broadcast version: scalar base raised to array of powers (scalar_base ^ exp_array)
inline void pow_float64_scalar_base(double base_scalar, const double *exp, double *output, int64_t count)
{
    int64_t i = 0;

#if defined(EIDOS_HAS_AVX2) && defined(EIDOS_HAS_FMA)
    __m256d vb_broadcast = _mm256_set1_pd(base_scalar);
    for (; i + 4 <= count; i += 4)
    {
        __m256d ve = _mm256_loadu_pd(&exp[i]);
        __m256d r = Sleef_powd4_u10avx2(vb_broadcast, ve);
        _mm256_storeu_pd(&output[i], r);
    }
#elif defined(EIDOS_HAS_NEON)
    float64x2_t vb_broadcast = vdupq_n_f64(base_scalar);
    for (; i + 2 <= count; i += 2)
    {
        float64x2_t ve = vld1q_f64(&exp[i]);
        float64x2_t r = Sleef_powd2_u10advsimd(vb_broadcast, ve);
        vst1q_f64(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::pow(base_scalar, exp[i]);
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

// ================================
// Float (Single-Precision) SIMD Operations
// ================================
// These functions operate on arrays of floats, used by spatial interaction kernels.

// ---------------------
// Exponential: exp(x) for floats
// ---------------------
inline void exp_float32(const float *input, float *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_FLOAT_AVAILABLE
    for (; i + EIDOS_SLEEF_VEC_SIZE_F <= count; i += EIDOS_SLEEF_VEC_SIZE_F)
    {
        EIDOS_SLEEF_TYPE_F v = EIDOS_SLEEF_LOAD_F(&input[i]);
        EIDOS_SLEEF_TYPE_F r = EIDOS_SLEEF_EXP_F(v);
        EIDOS_SLEEF_STORE_F(&output[i], r);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        output[i] = std::exp(input[i]);
}

// ---------------------
// Exponential Kernel: strength = fmax * exp(-lambda * distance)
// ---------------------
// Operates in-place on a distance array, transforming distances to strengths.
// This is optimized for the spatial interaction kernel calculation.
inline void exp_kernel_float32(float *distances, int64_t count, float fmax, float lambda)
{
    int64_t i = 0;

#if EIDOS_SLEEF_FLOAT_AVAILABLE
    // We need to compute: fmax * exp(-lambda * distance)
    // First, compute -lambda * distance for all values, then exp, then multiply by fmax
    EIDOS_SLEEF_TYPE_F v_fmax =
#if defined(EIDOS_HAS_AVX2)
        _mm256_set1_ps(fmax);
    EIDOS_SLEEF_TYPE_F v_neg_lambda = _mm256_set1_ps(-lambda);
#elif defined(EIDOS_HAS_NEON)
        vdupq_n_f32(fmax);
    EIDOS_SLEEF_TYPE_F v_neg_lambda = vdupq_n_f32(-lambda);
#endif

    for (; i + EIDOS_SLEEF_VEC_SIZE_F <= count; i += EIDOS_SLEEF_VEC_SIZE_F)
    {
        EIDOS_SLEEF_TYPE_F v_dist = EIDOS_SLEEF_LOAD_F(&distances[i]);

        // Compute -lambda * distance
#if defined(EIDOS_HAS_AVX2)
        EIDOS_SLEEF_TYPE_F v_arg = _mm256_mul_ps(v_neg_lambda, v_dist);
#elif defined(EIDOS_HAS_NEON)
        EIDOS_SLEEF_TYPE_F v_arg = vmulq_f32(v_neg_lambda, v_dist);
#endif

        // Compute exp(-lambda * distance)
        EIDOS_SLEEF_TYPE_F v_exp = EIDOS_SLEEF_EXP_F(v_arg);

        // Compute fmax * exp(...)
#if defined(EIDOS_HAS_AVX2)
        EIDOS_SLEEF_TYPE_F v_result = _mm256_mul_ps(v_fmax, v_exp);
#elif defined(EIDOS_HAS_NEON)
        EIDOS_SLEEF_TYPE_F v_result = vmulq_f32(v_fmax, v_exp);
#endif

        EIDOS_SLEEF_STORE_F(&distances[i], v_result);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
        distances[i] = fmax * std::exp(-lambda * distances[i]);
}

// ---------------------
// Normal (Gaussian) Kernel: strength = fmax * exp(-distance^2 / 2sigma^2)
// ---------------------
// Operates in-place on a distance array, transforming distances to strengths.
// The two_sigma_sq parameter is pre-computed as 2 * sigma^2 for efficiency.
inline void normal_kernel_float32(float *distances, int64_t count, float fmax, float two_sigma_sq)
{
    int64_t i = 0;

#if EIDOS_SLEEF_FLOAT_AVAILABLE
    // We need to compute: fmax * exp(-distance^2 / two_sigma_sq)
    EIDOS_SLEEF_TYPE_F v_fmax =
#if defined(EIDOS_HAS_AVX2)
        _mm256_set1_ps(fmax);
    EIDOS_SLEEF_TYPE_F v_neg_inv_2sigsq = _mm256_set1_ps(-1.0f / two_sigma_sq);
#elif defined(EIDOS_HAS_NEON)
        vdupq_n_f32(fmax);
    EIDOS_SLEEF_TYPE_F v_neg_inv_2sigsq = vdupq_n_f32(-1.0f / two_sigma_sq);
#endif

    for (; i + EIDOS_SLEEF_VEC_SIZE_F <= count; i += EIDOS_SLEEF_VEC_SIZE_F)
    {
        EIDOS_SLEEF_TYPE_F v_dist = EIDOS_SLEEF_LOAD_F(&distances[i]);

        // Compute distance^2
#if defined(EIDOS_HAS_AVX2)
        EIDOS_SLEEF_TYPE_F v_dist_sq = _mm256_mul_ps(v_dist, v_dist);
        // Compute -distance^2 / 2sigma^2
        EIDOS_SLEEF_TYPE_F v_arg = _mm256_mul_ps(v_dist_sq, v_neg_inv_2sigsq);
#elif defined(EIDOS_HAS_NEON)
        EIDOS_SLEEF_TYPE_F v_dist_sq = vmulq_f32(v_dist, v_dist);
        // Compute -distance^2 / 2sigma^2
        EIDOS_SLEEF_TYPE_F v_arg = vmulq_f32(v_dist_sq, v_neg_inv_2sigsq);
#endif

        // Compute exp(-distance^2 / 2sigma^2)
        EIDOS_SLEEF_TYPE_F v_exp = EIDOS_SLEEF_EXP_F(v_arg);

        // Compute fmax * exp(...)
#if defined(EIDOS_HAS_AVX2)
        EIDOS_SLEEF_TYPE_F v_result = _mm256_mul_ps(v_fmax, v_exp);
#elif defined(EIDOS_HAS_NEON)
        EIDOS_SLEEF_TYPE_F v_result = vmulq_f32(v_fmax, v_exp);
#endif

        EIDOS_SLEEF_STORE_F(&distances[i], v_result);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
    {
        float d = distances[i];
        distances[i] = fmax * std::exp(-(d * d) / two_sigma_sq);
    }
}

// ---------------------
// Student's T Kernel: strength = fmax / pow(1 + (d/tau)^2 / nu, (nu+1)/2)
// ---------------------
// Operates in-place on a distance array, transforming distances to strengths.
// Parameters: fmax = maximum strength, nu = degrees of freedom, tau = scale
inline void tdist_kernel_float32(float *distances, int64_t count, float fmax, float nu, float tau)
{
    int64_t i = 0;

    // Pre-compute constants
    float inv_tau = 1.0f / tau;
    float inv_nu = 1.0f / nu;
    float exponent = (nu + 1.0f) / 2.0f;

#if EIDOS_SLEEF_FLOAT_AVAILABLE
    EIDOS_SLEEF_TYPE_F v_fmax, v_inv_tau, v_inv_nu, v_exponent, v_one;
#if defined(EIDOS_HAS_AVX2)
    v_fmax = _mm256_set1_ps(fmax);
    v_inv_tau = _mm256_set1_ps(inv_tau);
    v_inv_nu = _mm256_set1_ps(inv_nu);
    v_exponent = _mm256_set1_ps(-exponent);
    v_one = _mm256_set1_ps(1.0f);
#elif defined(EIDOS_HAS_NEON)
    v_fmax = vdupq_n_f32(fmax);
    v_inv_tau = vdupq_n_f32(inv_tau);
    v_inv_nu = vdupq_n_f32(inv_nu);
    v_exponent = vdupq_n_f32(-exponent);
    v_one = vdupq_n_f32(1.0f);
#endif

    for (; i + EIDOS_SLEEF_VEC_SIZE_F <= count; i += EIDOS_SLEEF_VEC_SIZE_F)
    {
        EIDOS_SLEEF_TYPE_F v_dist = EIDOS_SLEEF_LOAD_F(&distances[i]);

        // Compute (d / tau)
#if defined(EIDOS_HAS_AVX2)
        EIDOS_SLEEF_TYPE_F v_d_over_tau = _mm256_mul_ps(v_dist, v_inv_tau);
        // Compute (d/tau)^2
        EIDOS_SLEEF_TYPE_F v_d_over_tau_sq = _mm256_mul_ps(v_d_over_tau, v_d_over_tau);
        // Compute (d/tau)^2 / nu
        EIDOS_SLEEF_TYPE_F v_term = _mm256_mul_ps(v_d_over_tau_sq, v_inv_nu);
        // Compute 1 + (d/tau)^2 / nu
        EIDOS_SLEEF_TYPE_F v_base = _mm256_add_ps(v_one, v_term);
#elif defined(EIDOS_HAS_NEON)
        EIDOS_SLEEF_TYPE_F v_d_over_tau = vmulq_f32(v_dist, v_inv_tau);
        EIDOS_SLEEF_TYPE_F v_d_over_tau_sq = vmulq_f32(v_d_over_tau, v_d_over_tau);
        EIDOS_SLEEF_TYPE_F v_term = vmulq_f32(v_d_over_tau_sq, v_inv_nu);
        EIDOS_SLEEF_TYPE_F v_base = vaddq_f32(v_one, v_term);
#endif

        // Compute pow(base, -exponent) = 1 / pow(base, exponent)
        EIDOS_SLEEF_TYPE_F v_pow = EIDOS_SLEEF_POW_F(v_base, v_exponent);

        // Compute fmax * pow(...)
#if defined(EIDOS_HAS_AVX2)
        EIDOS_SLEEF_TYPE_F v_result = _mm256_mul_ps(v_fmax, v_pow);
#elif defined(EIDOS_HAS_NEON)
        EIDOS_SLEEF_TYPE_F v_result = vmulq_f32(v_fmax, v_pow);
#endif

        EIDOS_SLEEF_STORE_F(&distances[i], v_result);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
    {
        float d = distances[i];
        float d_over_tau = d * inv_tau;
        distances[i] = fmax * std::pow(1.0f + d_over_tau * d_over_tau * inv_nu, -exponent);
    }
}

// ---------------------
// Cauchy Kernel: strength = fmax / (1 + (d/lambda)^2)
// ---------------------
// Operates in-place on a distance array, transforming distances to strengths.
// Parameters: fmax = maximum strength, lambda = scale parameter
inline void cauchy_kernel_float32(float *distances, int64_t count, float fmax, float lambda)
{
    int64_t i = 0;
    float inv_lambda = 1.0f / lambda;

#if defined(EIDOS_HAS_AVX2)
    __m256 v_fmax = _mm256_set1_ps(fmax);
    __m256 v_inv_lambda = _mm256_set1_ps(inv_lambda);
    __m256 v_one = _mm256_set1_ps(1.0f);

    for (; i + 8 <= count; i += 8)
    {
        __m256 v_dist = _mm256_loadu_ps(&distances[i]);
        __m256 v_temp = _mm256_mul_ps(v_dist, v_inv_lambda);      // d/lambda
        __m256 v_temp_sq = _mm256_mul_ps(v_temp, v_temp);         // (d/lambda)^2
        __m256 v_denom = _mm256_add_ps(v_one, v_temp_sq);         // 1 + (d/lambda)^2
        __m256 v_result = _mm256_div_ps(v_fmax, v_denom);         // fmax / denom
        _mm256_storeu_ps(&distances[i], v_result);
    }
#elif defined(EIDOS_HAS_NEON)
    float32x4_t v_fmax = vdupq_n_f32(fmax);
    float32x4_t v_inv_lambda = vdupq_n_f32(inv_lambda);
    float32x4_t v_one = vdupq_n_f32(1.0f);

    for (; i + 4 <= count; i += 4)
    {
        float32x4_t v_dist = vld1q_f32(&distances[i]);
        float32x4_t v_temp = vmulq_f32(v_dist, v_inv_lambda);
        float32x4_t v_temp_sq = vmulq_f32(v_temp, v_temp);
        float32x4_t v_denom = vaddq_f32(v_one, v_temp_sq);
        float32x4_t v_result = vdivq_f32(v_fmax, v_denom);
        vst1q_f32(&distances[i], v_result);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
    {
        float d = distances[i];
        float temp = d * inv_lambda;
        distances[i] = fmax / (1.0f + temp * temp);
    }
}

// ---------------------
// Linear Kernel: strength = fmax * (1 - d / max_distance)
// ---------------------
// Operates in-place on a distance array, transforming distances to strengths.
// Parameters: fmax = maximum strength, max_distance = interaction max distance
inline void linear_kernel_float32(float *distances, int64_t count, float fmax, float max_distance)
{
    int64_t i = 0;
    float fmax_over_maxdist = fmax / max_distance;

#if defined(EIDOS_HAS_AVX2)
    __m256 v_fmax = _mm256_set1_ps(fmax);
    __m256 v_fmax_over_maxdist = _mm256_set1_ps(fmax_over_maxdist);

    for (; i + 8 <= count; i += 8)
    {
        __m256 v_dist = _mm256_loadu_ps(&distances[i]);
        // fmax - d * (fmax / max_distance) = fmax * (1 - d/max_distance)
        __m256 v_term = _mm256_mul_ps(v_dist, v_fmax_over_maxdist);
        __m256 v_result = _mm256_sub_ps(v_fmax, v_term);
        _mm256_storeu_ps(&distances[i], v_result);
    }
#elif defined(EIDOS_HAS_NEON)
    float32x4_t v_fmax = vdupq_n_f32(fmax);
    float32x4_t v_fmax_over_maxdist = vdupq_n_f32(fmax_over_maxdist);

    for (; i + 4 <= count; i += 4)
    {
        float32x4_t v_dist = vld1q_f32(&distances[i]);
        float32x4_t v_term = vmulq_f32(v_dist, v_fmax_over_maxdist);
        float32x4_t v_result = vsubq_f32(v_fmax, v_term);
        vst1q_f32(&distances[i], v_result);
    }
#endif

    // Scalar remainder
    for (; i < count; i++)
    {
        distances[i] = fmax - distances[i] * fmax_over_maxdist;
    }
}

// ================================
// Convolution Helpers for SpatialMap::smooth()
// ================================
// These functions compute vectorized dot products for convolution operations.
// They accumulate both kernel_sum and conv_sum (weighted sum) in a single pass.

// ---------------------
// Convolution dot product: accumulates kernel_sum and kernel*values sum
// ---------------------
// Computes: kernel_sum += sum(kernel), conv_sum += sum(kernel * values)
// Used by SpatialMap convolution when all values are valid (no edge handling needed)
inline void convolve_dot_product_float64(
    const double *kernel_ptr, const double *pixel_ptr,
    int64_t count, double &kernel_sum, double &conv_sum)
{
    int64_t i = 0;
    double local_ksum = 0.0;
    double local_csum = 0.0;

#if defined(EIDOS_HAS_AVX2) && defined(EIDOS_HAS_FMA)
    __m256d v_ksum = _mm256_setzero_pd();
    __m256d v_csum = _mm256_setzero_pd();

    for (; i + 4 <= count; i += 4)
    {
        __m256d v_kernel = _mm256_loadu_pd(&kernel_ptr[i]);
        __m256d v_pixel = _mm256_loadu_pd(&pixel_ptr[i]);

        v_ksum = _mm256_add_pd(v_ksum, v_kernel);
        v_csum = _mm256_fmadd_pd(v_kernel, v_pixel, v_csum);
    }

    // Horizontal sum
    __m128d ksum_low = _mm256_castpd256_pd128(v_ksum);
    __m128d ksum_high = _mm256_extractf128_pd(v_ksum, 1);
    ksum_low = _mm_add_pd(ksum_low, ksum_high);
    __m128d ksum_shuf = _mm_shuffle_pd(ksum_low, ksum_low, 1);
    ksum_low = _mm_add_sd(ksum_low, ksum_shuf);
    local_ksum = _mm_cvtsd_f64(ksum_low);

    __m128d csum_low = _mm256_castpd256_pd128(v_csum);
    __m128d csum_high = _mm256_extractf128_pd(v_csum, 1);
    csum_low = _mm_add_pd(csum_low, csum_high);
    __m128d csum_shuf = _mm_shuffle_pd(csum_low, csum_low, 1);
    csum_low = _mm_add_sd(csum_low, csum_shuf);
    local_csum = _mm_cvtsd_f64(csum_low);

#elif defined(EIDOS_HAS_SSE42)
    __m128d v_ksum = _mm_setzero_pd();
    __m128d v_csum = _mm_setzero_pd();

    for (; i + 2 <= count; i += 2)
    {
        __m128d v_kernel = _mm_loadu_pd(&kernel_ptr[i]);
        __m128d v_pixel = _mm_loadu_pd(&pixel_ptr[i]);

        v_ksum = _mm_add_pd(v_ksum, v_kernel);
        __m128d v_prod = _mm_mul_pd(v_kernel, v_pixel);
        v_csum = _mm_add_pd(v_csum, v_prod);
    }

    // Horizontal sum
    __m128d ksum_shuf = _mm_shuffle_pd(v_ksum, v_ksum, 1);
    v_ksum = _mm_add_sd(v_ksum, ksum_shuf);
    local_ksum = _mm_cvtsd_f64(v_ksum);

    __m128d csum_shuf = _mm_shuffle_pd(v_csum, v_csum, 1);
    v_csum = _mm_add_sd(v_csum, csum_shuf);
    local_csum = _mm_cvtsd_f64(v_csum);

#elif defined(EIDOS_HAS_NEON)
    float64x2_t v_ksum = vdupq_n_f64(0.0);
    float64x2_t v_csum = vdupq_n_f64(0.0);

    for (; i + 2 <= count; i += 2)
    {
        float64x2_t v_kernel = vld1q_f64(&kernel_ptr[i]);
        float64x2_t v_pixel = vld1q_f64(&pixel_ptr[i]);

        v_ksum = vaddq_f64(v_ksum, v_kernel);
        v_csum = vfmaq_f64(v_csum, v_kernel, v_pixel);
    }

    local_ksum = vaddvq_f64(v_ksum);
    local_csum = vaddvq_f64(v_csum);
#endif

    // Scalar remainder
    for (; i < count; i++)
    {
        local_ksum += kernel_ptr[i];
        local_csum += kernel_ptr[i] * pixel_ptr[i];
    }

    kernel_sum += local_ksum;
    conv_sum += local_csum;
}

// ---------------------
// Scaled convolution dot product for edge handling
// ---------------------
// Like convolve_dot_product_float64 but scales kernel values by coverage factor.
// Used when handling edges where some kernel positions are out of bounds.
inline void convolve_dot_product_scaled_float64(
    const double *kernel_ptr, const double *pixel_ptr,
    int64_t count, double coverage,
    double &kernel_sum, double &conv_sum)
{
    int64_t i = 0;
    double local_ksum = 0.0;
    double local_csum = 0.0;

#if defined(EIDOS_HAS_AVX2) && defined(EIDOS_HAS_FMA)
    __m256d v_coverage = _mm256_set1_pd(coverage);
    __m256d v_ksum = _mm256_setzero_pd();
    __m256d v_csum = _mm256_setzero_pd();

    for (; i + 4 <= count; i += 4)
    {
        __m256d v_kernel = _mm256_loadu_pd(&kernel_ptr[i]);
        __m256d v_pixel = _mm256_loadu_pd(&pixel_ptr[i]);
        __m256d v_scaled_kernel = _mm256_mul_pd(v_kernel, v_coverage);

        v_ksum = _mm256_add_pd(v_ksum, v_scaled_kernel);
        v_csum = _mm256_fmadd_pd(v_scaled_kernel, v_pixel, v_csum);
    }

    // Horizontal sum
    __m128d ksum_low = _mm256_castpd256_pd128(v_ksum);
    __m128d ksum_high = _mm256_extractf128_pd(v_ksum, 1);
    ksum_low = _mm_add_pd(ksum_low, ksum_high);
    __m128d ksum_shuf = _mm_shuffle_pd(ksum_low, ksum_low, 1);
    ksum_low = _mm_add_sd(ksum_low, ksum_shuf);
    local_ksum = _mm_cvtsd_f64(ksum_low);

    __m128d csum_low = _mm256_castpd256_pd128(v_csum);
    __m128d csum_high = _mm256_extractf128_pd(v_csum, 1);
    csum_low = _mm_add_pd(csum_low, csum_high);
    __m128d csum_shuf = _mm_shuffle_pd(csum_low, csum_low, 1);
    csum_low = _mm_add_sd(csum_low, csum_shuf);
    local_csum = _mm_cvtsd_f64(csum_low);

#elif defined(EIDOS_HAS_SSE42)
    __m128d v_coverage = _mm_set1_pd(coverage);
    __m128d v_ksum = _mm_setzero_pd();
    __m128d v_csum = _mm_setzero_pd();

    for (; i + 2 <= count; i += 2)
    {
        __m128d v_kernel = _mm_loadu_pd(&kernel_ptr[i]);
        __m128d v_pixel = _mm_loadu_pd(&pixel_ptr[i]);
        __m128d v_scaled_kernel = _mm_mul_pd(v_kernel, v_coverage);

        v_ksum = _mm_add_pd(v_ksum, v_scaled_kernel);
        __m128d v_prod = _mm_mul_pd(v_scaled_kernel, v_pixel);
        v_csum = _mm_add_pd(v_csum, v_prod);
    }

    // Horizontal sum
    __m128d ksum_shuf = _mm_shuffle_pd(v_ksum, v_ksum, 1);
    v_ksum = _mm_add_sd(v_ksum, ksum_shuf);
    local_ksum = _mm_cvtsd_f64(v_ksum);

    __m128d csum_shuf = _mm_shuffle_pd(v_csum, v_csum, 1);
    v_csum = _mm_add_sd(v_csum, csum_shuf);
    local_csum = _mm_cvtsd_f64(v_csum);

#elif defined(EIDOS_HAS_NEON)
    float64x2_t v_coverage = vdupq_n_f64(coverage);
    float64x2_t v_ksum = vdupq_n_f64(0.0);
    float64x2_t v_csum = vdupq_n_f64(0.0);

    for (; i + 2 <= count; i += 2)
    {
        float64x2_t v_kernel = vld1q_f64(&kernel_ptr[i]);
        float64x2_t v_pixel = vld1q_f64(&pixel_ptr[i]);
        float64x2_t v_scaled_kernel = vmulq_f64(v_kernel, v_coverage);

        v_ksum = vaddq_f64(v_ksum, v_scaled_kernel);
        v_csum = vfmaq_f64(v_csum, v_scaled_kernel, v_pixel);
    }

    local_ksum = vaddvq_f64(v_ksum);
    local_csum = vaddvq_f64(v_csum);
#endif

    // Scalar remainder with scaling
    for (; i < count; i++)
    {
        double scaled_k = kernel_ptr[i] * coverage;
        local_ksum += scaled_k;
        local_csum += scaled_k * pixel_ptr[i];
    }

    kernel_sum += local_ksum;
    conv_sum += local_csum;
}

} // namespace Eidos_SIMD

#endif /* eidos_simd_h */
