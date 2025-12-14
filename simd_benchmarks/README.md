# SIMD Benchmarks

This directory contains benchmark scripts used during the development of SIMD optimizations for SLiM. These files are provided for internal development use and are **not used in the build of SLiM**.

Also contained here are helper scripts and documentation for generating the SLEEF inline headers used for SIMD math functions.

## Contents

- **`run_benchmarks.sh`** - Shell script that builds SLiM with and without SIMD, runs both benchmark scripts, and reports speedup comparisons.

- **`simd_benchmark.eidos`** - Eidos script that benchmarks SIMD-optimized math functions (`sqrt`, `abs`, `floor`, `ceil`, `round`, `trunc`, `sum`, `product`) on large arrays.

- **`slim_benchmark.slim`** - SLiM simulation benchmark (N=5000, 1Mb chromosome, 5000 generations with selection) for measuring overall simulation performance.

- **`generate_avx2_sleef.sh`** - Script to generate the SLEEF AVX2 inline header on x86_64 machines.

- **`generate_arm_sleef.sh`** - Script to generate the SLEEF ARM NEON inline header on ARM64 machines.

- **`SIMD_BUILD_FLAGS.md`** - Documentation on how SIMD and SLEEF build flags are set and interact.

- **`SLEEF_HEADER_GENERATION.md`** - Documentation on how to generate and patch the SLEEF inline headers.

## Author

These benchmarks were developed by Andrew Kern as part of SIMD optimization work for SLiM.

## Usage

These files are not part of the SLiM build system. To run the benchmarks:

```bash
cd simd_benchmarks
./run_benchmarks.sh [num_runs]
```

This will build both SIMD-enabled and scalar versions of SLiM, run the benchmarks, and report the speedup.

## Results

Benchmark results look like the following (example output):

```
$ simd_benchmarks/run_benchmarks.sh 
============================================
SIMD Benchmark Runner
============================================
SLiM root: /home/adkern/SLiM
Runs per benchmark: 3

Building with SIMD enabled...
  Done.
Building with SIMD disabled...
  Done.

============================================
Eidos Math Function Benchmarks
============================================

SIMD Build:
  Running Eidos benchmark (SIMD)...
    sqrt():    0.105 sec
    abs():     0.171 sec
    floor():   0.164 sec
    ceil():    0.166 sec
    round():   0.164 sec
    trunc():   0.165 sec
    sum():     0.032 sec
    product(): 0.003 sec (1000 elements, 10000 iters)

Scalar Build:
  Running Eidos benchmark (Scalar)...
    sqrt():    0.108 sec
    abs():     0.166 sec
    floor():   0.231 sec
    ceil():    0.246 sec
    round():   0.473 sec
    trunc():   0.246 sec
    sum():     0.166 sec
    product(): 0.017 sec (1000 elements, 10000 iters)

============================================
SLiM Simulation Benchmark
(N=5000, 5000 generations, selection)
============================================

Running 3 iterations each...

SIMD Build:   12.756s (avg)
Scalar Build: 12.316s (avg)

Speedup: .96x

============================================
Benchmark complete
============================================
```
so the takeaway is that SIMD provided significant speedups for eidos math functions, while the overall SLiM simulation speedup was minimal in this specific benchmark scenario.