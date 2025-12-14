# SLEEF Header Generation and Patching

This document explains how to regenerate the SLEEF inline headers if needed.

## Background

[SLEEF](https://github.com/shibatch/sleef) (SIMD Library for Evaluating Elementary Functions) provides vectorized implementations of transcendental math functions (exp, log, sin, cos, etc.) with guaranteed accuracy.

We use SLEEF's **inline headers** which are self-contained header files that can be vendored directly into a project without linking to an external library. This avoids runtime dependencies and allows the compiler to inline the SIMD code.

## Current Headers

| File | Architecture | Vector Width | Source |
|------|--------------|--------------|--------|
| `eidos/sleef/sleefinline_avx2.h` | x86_64 AVX2+FMA | 4 doubles | Generated on x86_64 Linux |
| `eidos/sleef/sleefinline_advsimd.h` | ARM64 NEON | 2 doubles | Generated on ARM64 macOS |

Both headers are from **SLEEF 4.0.0** (generated December 2024).

## Why Patching is Required

SLEEF's generated headers use **C99 hexadecimal floating-point literals** like:

```c
0x1p-1022      // 2.22507385850720138e-308 (DBL_MIN)
0x1p-126       // 1.17549435082228751e-38  (FLT_MIN)
0x1.62e42fefa39efp+9  // 709.78... (log overflow threshold)
```

These are valid in C99/C11 and C++17, but **not in C++11/C++14**. Since SLiM uses C++11 (C++17 only when building with Qt6), we must convert these to decimal equivalents.

## Generating the AVX2 Header (x86_64)

Run on any x86_64 Linux or macOS machine:

```bash
#!/bin/bash
# generate_avx2_sleef.sh

set -e
TMPDIR=$(mktemp -d)
cd "$TMPDIR"

# Clone and build SLEEF
git clone --depth 1 https://github.com/shibatch/sleef.git
cd sleef && mkdir build && cd build

cmake .. \
    -DSLEEF_BUILD_INLINE_HEADERS=TRUE \
    -DSLEEF_BUILD_TESTS=OFF \
    -DSLEEF_BUILD_LIBM=OFF \
    -DSLEEF_BUILD_DFT=OFF \
    -DSLEEF_BUILD_QUAD=OFF \
    -DCMAKE_INSTALL_PREFIX="$TMPDIR/install"

make inline_headers

# Patch hex floats and output
sed \
    -e 's/0x1\.2ced32p+126/9.99999968028569247e+37/g' \
    -e 's/0x1\.62e42fefa39efp+9/7.09782712893383973e+02/g' \
    -e 's/0x1\.c7b1f3cac7433p+1019/9.99999999999999986e+306/g' \
    -e 's/0x1p-1022/2.22507385850720138e-308/g' \
    -e 's/0x1p-126/1.17549435082228751e-38/g' \
    include/sleefinline_avx2.h

# Cleanup
cd / && rm -rf "$TMPDIR"
```

## Generating the ARM NEON Header (ARM64)

Must be run on an ARM64 machine (Apple Silicon Mac or ARM64 Linux). There's a helper script in the repo:

```bash
# From an x86_64 machine, run on a remote ARM64 machine:
ssh arm-machine "bash -s" < generate_arm_sleef.sh > eidos/sleef/sleefinline_advsimd.h

# Or run directly on an ARM64 machine:
./generate_arm_sleef.sh > eidos/sleef/sleefinline_advsimd.h
```

The script (`generate_arm_sleef.sh` in repo root) handles:
1. Cloning SLEEF
2. Building with `-DCMAKE_OSX_ARCHITECTURES=arm64` (for Apple Silicon)
3. Patching hex float literals
4. Outputting the patched header to stdout

## Hex Float Patch Reference

These are the known hex float literals that need patching:

| Hex Literal | Decimal Equivalent | Meaning |
|-------------|-------------------|---------|
| `0x1p-1022` | `2.22507385850720138e-308` | DBL_MIN (smallest normal double) |
| `0x1p-126` | `1.17549435082228751e-38` | FLT_MIN (smallest normal float) |
| `0x1.62e42fefa39efp+9` | `7.09782712893383973e+02` | ln(DBL_MAX), exp overflow threshold |
| `0x1.2ced32p+126` | `9.99999968028569247e+37` | Float overflow constant |
| `0x1.c7b1f3cac7433p+1019` | `9.99999999999999986e+306` | Double overflow constant |

To find any new hex floats in a generated header:

```bash
grep -oE '0x[0-9a-fA-F.]+p[+-]?[0-9]+' sleefinline_avx2.h | sort -u
```

To convert a hex float to decimal (Python):

```python
>>> float.fromhex('0x1p-1022')
2.2250738585072014e-308
>>> f'{float.fromhex("0x1.62e42fefa39efp+9"):.17g}'
'709.78271289338397'
```

## Verifying the Patched Headers

After patching, verify the headers compile with C++11:

```bash
# Test AVX2 header
echo '#include "eidos/sleef/sleefinline_avx2.h"' | \
    g++ -std=c++11 -mavx2 -mfma -x c++ -c - -o /dev/null

# Test ARM NEON header (on ARM64)
echo '#include "eidos/sleef/sleefinline_advsimd.h"' | \
    g++ -std=c++11 -x c++ -c - -o /dev/null
```

## SLEEF License

SLEEF is distributed under the **Boost Software License 1.0**, which permits:
- Commercial and non-commercial use
- Modification and redistribution
- No attribution required in binary distributions

The license file is included at `eidos/sleef/LICENSE`.

## Updating to a New SLEEF Version

1. Generate new headers using the scripts above
2. Check for new hex float literals: `grep -oE '0x[0-9a-fA-F.]+p[+-]?[0-9]+' header.h`
3. Add any new literals to the sed patch commands
4. Verify compilation with C++11
5. Run tests: `./slim -testEidos && ./slim -testSLiM`
6. Benchmark to verify performance: `./slim simd_benchmarks/sleef_benchmark.slim`

## Files

| File | Purpose |
|------|---------|
| `eidos/sleef/sleefinline_avx2.h` | Patched SLEEF AVX2 header (~6700 lines) |
| `eidos/sleef/sleefinline_advsimd.h` | Patched SLEEF ARM NEON header (~6900 lines) |
| `eidos/sleef/sleef_config.h` | Architecture selection macros |
| `eidos/sleef/LICENSE` | Boost Software License |
| `generate_arm_sleef.sh` | Script to generate ARM header on remote machine |
