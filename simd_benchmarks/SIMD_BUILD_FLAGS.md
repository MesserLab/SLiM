# SIMD and SLEEF Build Flags

This document explains how the SIMD and SLEEF build flags work and interact in the SLiM/Eidos codebase.

## Overview

There are three layers:
1. **CMake options** - set at configure time
2. **Compiler definitions** - passed via `-D` flags
3. **Header conditionals** - control which code paths are compiled

## Layer 1: CMake Configuration

In `CMakeLists.txt`:

```cmake
option(USE_SIMD "Enable SIMD optimizations" ON)  # User-controllable
```

When `USE_SIMD=ON` (default), CMake detects the platform and compiler capabilities:

| Platform | Detection | Defines Set |
|----------|-----------|-------------|
| Linux/macOS x86_64 | Tests `-mavx2`, `-mfma` flags | `EIDOS_HAS_AVX2=1`, `EIDOS_HAS_FMA=1` |
| Linux/macOS ARM64 | Always available | `EIDOS_HAS_NEON=1` |
| Windows/MinGW | Tests `-mavx2`, `-mfma` flags | `EIDOS_HAS_AVX2=1`, `EIDOS_HAS_FMA=1` |
| SSE4.2 only | Tests `-msse4.2` | `EIDOS_HAS_SSE42=1` |

## Layer 2: Header Selection

In `eidos/eidos_simd.h` (around line 57):

```cpp
// Include SLEEF config if we have AVX2 or NEON
#if defined(EIDOS_HAS_AVX2) || defined(EIDOS_HAS_NEON)
#include "sleef/sleef_config.h"
#endif
```

In `eidos/sleef/sleef_config.h`:

```cpp
#ifndef EIDOS_SLEEF_AVAILABLE  // Allow command-line override

#if defined(EIDOS_HAS_AVX2) && defined(EIDOS_HAS_FMA)
    // AVX2+FMA path (Intel Haswell+, AMD Zen+)
    #include "sleefinline_avx2.h"
    #define EIDOS_SLEEF_AVAILABLE 1
    #define EIDOS_SLEEF_VEC_SIZE 4        // 4 doubles per vector
    #define EIDOS_SLEEF_EXP_D(v) Sleef_expd4_u10avx2(v)
    // ... other function macros

#elif defined(EIDOS_HAS_NEON)
    // ARM NEON path (Apple Silicon, ARM64 Linux)
    #include "sleefinline_advsimd.h"
    #define EIDOS_SLEEF_AVAILABLE 1
    #define EIDOS_SLEEF_VEC_SIZE 2        // 2 doubles per vector
    #define EIDOS_SLEEF_EXP_D(v) Sleef_expd2_u10advsimd(v)
    // ... other function macros

#else
    // Scalar fallback (SSE4.2-only or no SIMD)
    #define EIDOS_SLEEF_AVAILABLE 0
    #define EIDOS_SLEEF_VEC_SIZE 1
#endif

#endif // EIDOS_SLEEF_AVAILABLE
```

## Layer 3: Function Implementation

In `eidos/eidos_simd.h`, the actual functions use the macros:

```cpp
inline void exp_float64(const double *input, double *output, int64_t count)
{
    int64_t i = 0;

#if EIDOS_SLEEF_AVAILABLE
    // SIMD loop - process 4 (AVX2) or 2 (NEON) doubles at a time
    for (; i + EIDOS_SLEEF_VEC_SIZE <= count; i += EIDOS_SLEEF_VEC_SIZE)
    {
        EIDOS_SLEEF_TYPE_D v = EIDOS_SLEEF_LOAD_D(&input[i]);
        EIDOS_SLEEF_TYPE_D r = EIDOS_SLEEF_EXP_D(v);
        EIDOS_SLEEF_STORE_D(&output[i], r);
    }
#endif

    // Scalar remainder (always runs for leftover elements)
    for (; i < count; i++)
        output[i] = std::exp(input[i]);
}
```

## Flag Interaction Summary

```
USE_SIMD=ON (cmake)
    │
    ├─► x86_64 + AVX2 + FMA detected
    │       │
    │       ├─► -DEIDOS_HAS_AVX2=1 -DEIDOS_HAS_FMA=1 -mavx2 -mfma
    │       │
    │       └─► sleef_config.h sets EIDOS_SLEEF_AVAILABLE=1, VEC_SIZE=4
    │               │
    │               └─► exp_float64() uses Sleef_expd4_u10avx2() (4-wide)
    │
    ├─► ARM64 detected
    │       │
    │       ├─► -DEIDOS_HAS_NEON=1
    │       │
    │       └─► sleef_config.h sets EIDOS_SLEEF_AVAILABLE=1, VEC_SIZE=2
    │               │
    │               └─► exp_float64() uses Sleef_expd2_u10advsimd() (2-wide)
    │
    └─► SSE4.2-only or unknown
            │
            ├─► -DEIDOS_HAS_SSE42=1 (or nothing)
            │
            └─► sleef_config.h sets EIDOS_SLEEF_AVAILABLE=0
                    │
                    └─► exp_float64() uses std::exp() (scalar)

USE_SIMD=OFF (cmake)
    │
    └─► No EIDOS_HAS_* defined
            │
            └─► All SIMD disabled, scalar fallback everywhere
```

## Testing/Override

You can force SLEEF off for benchmarking:

```bash
cmake .. -DCMAKE_CXX_FLAGS="-DEIDOS_SLEEF_AVAILABLE=0"
```

This works because `sleef_config.h` checks `#ifndef EIDOS_SLEEF_AVAILABLE` before defining it.

## What Uses SLEEF vs Hand-Written SIMD

| Function | Implementation | Why |
|----------|---------------|-----|
| `exp`, `log`, `log10`, `log2` | **SLEEF** | Transcendentals need complex algorithms |
| `sqrt`, `abs` | Hand-written SIMD | Hardware has direct instructions (`vsqrtpd`, bit ops) |
| `floor`, `ceil`, `round`, `trunc` | Hand-written SIMD | Hardware rounding instructions |
| `sum`, `product` | Hand-written SIMD | Simple accumulation + horizontal reduction |

## Performance Results

Benchmarks on 1M elements, 10 repetitions (x86_64 AVX2):

| Function | Without SLEEF | With SLEEF | Speedup |
|----------|---------------|------------|---------|
| exp()    | 8.30 ms       | 4.05 ms    | **2.1x** |
| log()    | 6.17 ms       | 3.37 ms    | **1.8x** |
| log10()  | 10.79 ms      | 3.66 ms    | **2.9x** |
| log2()   | 5.81 ms       | 3.99 ms    | **1.5x** |

## Key Files

- `CMakeLists.txt` - SIMD detection and flag setting (lines ~306-360)
- `eidos/eidos_simd.h` - SIMD function implementations
- `eidos/sleef/sleef_config.h` - SLEEF architecture selection
- `eidos/sleef/sleefinline_avx2.h` - SLEEF AVX2 inline functions
- `eidos/sleef/sleefinline_advsimd.h` - SLEEF ARM NEON inline functions
- `cmake/toolchain-mingw64.cmake` - Windows cross-compilation toolchain
