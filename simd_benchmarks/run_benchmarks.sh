#!/bin/bash
# SIMD Benchmark Runner
# Builds SLiM with and without SIMD, runs benchmarks, compares results

set -e

SLIM_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BENCHMARK_DIR="$SLIM_ROOT/simd_benchmarks"
BUILD_SIMD="$SLIM_ROOT/build_simd"
BUILD_SCALAR="$SLIM_ROOT/build_scalar"

NUM_RUNS=${1:-3}  # Default to 3 runs, or use first argument

echo "============================================"
echo "SIMD Benchmark Runner"
echo "============================================"
echo "SLiM root: $SLIM_ROOT"
echo "Runs per benchmark: $NUM_RUNS"
echo ""

# Build with SIMD
echo "Building with SIMD enabled..."
rm -rf "$BUILD_SIMD"
mkdir -p "$BUILD_SIMD"
cd "$BUILD_SIMD"
cmake -DUSE_SIMD=ON -DCMAKE_BUILD_TYPE=Release .. > /dev/null
make -j10 > /dev/null 2>&1
echo "  Done."

# Build without SIMD
echo "Building with SIMD disabled..."
rm -rf "$BUILD_SCALAR"
mkdir -p "$BUILD_SCALAR"
cd "$BUILD_SCALAR"
cmake -DUSE_SIMD=OFF -DCMAKE_BUILD_TYPE=Release .. > /dev/null
make -j10 > /dev/null 2>&1
echo "  Done."
echo ""

# Function to run eidos benchmark and extract times
run_eidos_benchmark() {
    local binary="$1"
    local label="$2"

    echo "  Running Eidos benchmark ($label)..."
    "$binary" "$BENCHMARK_DIR/simd_benchmark.eidos" 2>/dev/null | grep -E "^\w+\(\):" | while read line; do
        echo "    $line"
    done
}

# Function to run slim benchmark and get average time
run_slim_benchmark() {
    local binary="$1"
    local runs="$2"
    local total=0

    for ((i=1; i<=runs; i++)); do
        time=$("$binary" "$BENCHMARK_DIR/slim_benchmark.slim" 2>/dev/null | grep "Elapsed time" | grep -oE '[0-9]+\.[0-9]+')
        total=$(echo "$total + $time" | bc)
    done

    avg=$(echo "scale=3; $total / $runs" | bc)
    echo "$avg"
}

echo "============================================"
echo "Eidos Math Function Benchmarks"
echo "============================================"
echo ""

echo "SIMD Build:"
run_eidos_benchmark "$BUILD_SIMD/eidos" "SIMD"
echo ""

echo "Scalar Build:"
run_eidos_benchmark "$BUILD_SCALAR/eidos" "Scalar"
echo ""

echo "============================================"
echo "SLiM Simulation Benchmark"
echo "(N=5000, 5000 generations, selection)"
echo "============================================"
echo ""

echo "Running $NUM_RUNS iterations each..."
echo ""

simd_time=$(run_slim_benchmark "$BUILD_SIMD/slim" "$NUM_RUNS")
echo "SIMD Build:   ${simd_time}s (avg)"

scalar_time=$(run_slim_benchmark "$BUILD_SCALAR/slim" "$NUM_RUNS")
echo "Scalar Build: ${scalar_time}s (avg)"

if [ "$(echo "$scalar_time > 0" | bc)" -eq 1 ]; then
    speedup=$(echo "scale=2; $scalar_time / $simd_time" | bc)
    echo ""
    echo "Speedup: ${speedup}x"
fi

echo ""
echo "============================================"
echo "Benchmark complete"
echo "============================================"
