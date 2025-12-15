# CMake toolchain file for cross-compiling SLiM to Windows using MinGW-w64
#
# NOTE: This file is only needed for cross-compiling to Windows from Linux.
# It is NOT used for normal builds on Linux, macOS, or native Windows (MSYS2).
#
# Usage:
#   mkdir build_win64 && cd build_win64
#   cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain-mingw64.cmake
#   make -j10 slim eidos
#
# Prerequisites:
#   sudo apt-get install mingw-w64
#
# Added by Andrew Kern on 2025-12-14
# 

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the cross compilers
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Where to find the target environment
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Search for programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
