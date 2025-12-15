#!/bin/bash
# generate_avx2_sleef.sh - Run this on an x86_64 machine to generate SLEEF AVX2 header
# Usage: ./eidos/sleef/generate_avx2_sleef.sh > eidos/sleef/sleefinline_avx2.h

set -e

TMPDIR=$(mktemp -d)
cd "$TMPDIR"

# Clone and build SLEEF with inline headers
git clone --depth 1 https://github.com/shibatch/sleef.git sleef >&2
cd sleef
mkdir build && cd build
cmake .. \
    -DSLEEF_BUILD_INLINE_HEADERS=TRUE \
    -DSLEEF_BUILD_TESTS=OFF \
    -DSLEEF_BUILD_LIBM=OFF \
    -DSLEEF_BUILD_DFT=OFF \
    -DSLEEF_BUILD_QUAD=OFF \
    -DCMAKE_INSTALL_PREFIX="$TMPDIR/install" >&2
make inline_headers >&2 || make >&2 || true

# Check if file exists
if [ ! -f include/sleefinline_avx2.h ]; then
    echo "ERROR: sleefinline_avx2.h not found. Contents of include/:" >&2
    ls -la include/ >&2
    exit 1
fi

# Patch hex float literals for C++11 compatibility and output to stdout
# Also fix __float128 compatibility (not supported by Clang/AppleClang)
sed \
    -e 's/0x1\.2ced32p+126/9.99999968028569247e+37/g' \
    -e 's/0x1\.62e42fefa39efp+9/7.09782712893383973e+02/g' \
    -e 's/0x1\.c7b1f3cac7433p+1019/9.99999999999999986e+306/g' \
    -e 's/0x1p-1022/2.22507385850720138e-308/g' \
    -e 's/0x1p-126/1.17549435082228751e-38/g' \
    -e 's/^#define SLEEF_FLOAT128_IS_IEEEQP$/\/\/ Only define SLEEF_FLOAT128_IS_IEEEQP on compilers that support __float128\n#if defined(__GNUC__) \&\& !defined(__clang__) \&\& defined(__SIZEOF_FLOAT128__)\n#define SLEEF_FLOAT128_IS_IEEEQP\n#endif/' \
    include/sleefinline_avx2.h

# Cleanup
cd /
rm -rf "$TMPDIR"
