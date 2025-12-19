# SIMD Benchmarks

This directory contains benchmark scripts used during the development of SIMD optimizations for SLiM. These files are provided for internal development use and are **not used in the build of SLiM**.

## Contents

- **`run_benchmarks.sh`** - Shell script that builds SLiM with and without SIMD, runs both benchmark scripts, and reports speedup comparisons.

- **`simd_benchmark.eidos`** - Eidos script that benchmarks SIMD-optimized math functions (`sqrt`, `abs`, `floor`, `ceil`, `round`, `trunc`, `sum`, `product`) on large arrays.

- **`slim_benchmark.slim`** - SLiM simulation benchmark (N=5000, 1Mb chromosome, 5000 generations with selection) for measuring overall simulation performance.

- **`dnorm_benchmark.eidos`** - Eidos script that benchmarks the SIMD-optimized `dnorm()` function with singleton and vector mu/sigma parameters.

- **`benchmark_all_kernels.slim`** - SLiM script that benchmarks all 6 SIMD-optimized spatial interaction kernel types (Fixed, Linear, Exponential, Normal, Cauchy, Student's T).

- **`SIMD_BUILD_FLAGS.md`** - Documentation on how SIMD and SLEEF build flags are set and interact.

For SLEEF header generation scripts and documentation, see `eidos/sleef/`.

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

## dnorm() Benchmark Results

The `dnorm()` function was optimized to batch `exp()` calls using SLEEF SIMD vectorization. Results on x86_64 with AVX2 (N=1,000,000 elements, 10 iterations):

| Case | SIMD (M elem/s) | Scalar (M elem/s) | Speedup |
|------|-----------------|-------------------|---------|
| `dnorm(x, scalar, scalar)` | 119.9 | 43.7 | **2.74x** |
| `dnorm(x, scalar, vector)` | 52.5 | 33.5 | **1.57x** |
| `dnorm(x, vector, scalar)` | 56.8 | 34.3 | **1.66x** |
| `dnorm(x, vector, vector)` | 41.7 | 28.2 | **1.48x** |

The single mu/sigma case shows the best speedup at 2.74x. Variable parameter cases have additional overhead from per-element lookups but still benefit from batched SIMD exp().

To run this benchmark:
```bash
# Build with SIMD
mkdir build && cd build && cmake .. && make eidos
./eidos ../simd_benchmarks/dnorm_benchmark.eidos

# Build without SIMD for comparison
mkdir build_nosimd && cd build_nosimd && cmake .. -DUSE_SIMD=OFF && make eidos
./eidos ../simd_benchmarks/dnorm_benchmark.eidos
```

## Spatial Interaction Kernel Benchmark Results

The `benchmark_all_kernels.slim` script tests SIMD-optimized spatial interaction kernels. All kernel types except Fixed use a two-pass approach (build distances, then batch transform) enabling SIMD vectorization via SLEEF.

Results on x86_64 with AVX2 (50,000 individuals, ~2,262 neighbors per individual, 30 generations):

| Kernel | Original | SIMD | Speedup |
|--------|----------|------|---------|
| Fixed | 31.97s | 31.36s | 1.02x |
| Linear | 37.26s | 32.95s | **1.13x** |
| Exponential | 59.58s | 34.88s | **1.71x** |
| Normal | 56.37s | 35.15s | **1.60x** |
| Cauchy | 40.04s | 33.00s | **1.21x** |
| Student's T | 130.10s | 49.76s | **2.61x** |
| **Total** | **356.04s** | **217.80s** | **1.64x** |

The biggest gains come from kernels using transcendental functions:
- **Student's T** (2.61x): Uses `pow()` via SLEEF
- **Exponential/Normal** (1.6-1.7x): Use `exp()` via SLEEF
- **Linear/Cauchy** (~1.1-1.2x): Simple arithmetic, modest gains from explicit SIMD
- **Fixed** (1.02x): Uses special-case single-pass path (no benefit from two-pass)

To run this benchmark:
```bash
# Build with SIMD
mkdir build && cd build && cmake .. && make slim
./slim -s 42 ../simd_benchmarks/benchmark_all_kernels.slim

# Build without SIMD for comparison
mkdir build_nosimd && cd build_nosimd && cmake .. -DUSE_SIMD=OFF && make slim
./slim -s 42 ../simd_benchmarks/benchmark_all_kernels.slim
```

Adjust `W` in the script to change neighbor density (W=25 for ~2200 neighbors, W=266 for ~20 neighbors).

## SpatialMap smooth() vs smooth_fast() Benchmark Results

The `benchmark_smooth.slim` script compares the original `smooth()` method with the SIMD-optimized `smooth_fast()` method for SpatialMap convolution operations.

### SIMD Build (AVX2+FMA)

| Test | smooth() | smooth_fast() | Speedup |
|------|----------|---------------|---------|
| 1D 100 pts | 0.006ms | 0.002ms | **2.96x** |
| 1D 1000 pts | 0.131ms | 0.031ms | **4.22x** |
| 2D 50x50 | 0.349ms | 0.144ms | **2.43x** |
| 2D 100x100 | 5.615ms | 1.202ms | **4.67x** |
| 2D 200x200 | 88.485ms | 15.035ms | **5.89x** |
| 3D 20x20x20 | 3.571ms | 1.589ms | **2.25x** |
| 3D 30x30x30 | 12.394ms | 5.800ms | **2.14x** |

### NO_SIMD Build (Scalar)

| Test | smooth() | smooth_fast() | Speedup |
|------|----------|---------------|---------|
| 1D 100 pts | 0.006ms | 0.003ms | **2.24x** |
| 1D 1000 pts | 0.097ms | 0.084ms | **1.16x** |
| 2D 50x50 | 0.243ms | 0.167ms | **1.46x** |
| 2D 100x100 | 3.595ms | 2.336ms | **1.54x** |
| 2D 200x200 | 53.854ms | 39.838ms | **1.35x** |
| 3D 20x20x20 | 4.130ms | 1.458ms | **2.83x** |
| 3D 30x30x30 | 14.161ms | 5.283ms | **2.68x** |


To run this benchmark:
```bash
# Build with SIMD
mkdir build && cd build && cmake .. && make slim
./slim ../simd_benchmarks/benchmark_smooth.slim

# Build without SIMD for comparison
mkdir build_nosimd && cd build_nosimd && cmake .. -DUSE_SIMD=OFF && make slim
./slim ../simd_benchmarks/benchmark_smooth.slim
```