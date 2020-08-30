# To use CMake to build SLiM, create a new subdirectory alongside your source directory (assumed here
# to be named SLiM), e.g., "build", then run the following commands:
#
#   cd build
#   cmake ../SLiM
#   make -j10
#
# This will make a Release build, with optimization and without debugging symbols, by default.
# The built executables will be placed in the build directory upon successful completion. The optional
# -j argument specifies the number of threads to use for the build, which should be a value related 
# to the number of cores available on your machine. Typically this value does not exceed the number of cores.

# 
# You can also explicitly make a Release build; this is typically done in a directory named "Release"
# instead of "build":
#
#   mkdir Release
#   cd Release
#   cmake -D CMAKE_BUILD_TYPE=Release ../SLiM
#   make 
#
# Or you can make a Debug build (without optimization, with debugging symbols):
#
#   mkdir Debug
#   cd Debug
#   cmake -D CMAKE_BUILD_TYPE=Debug ../SLiM
#   make 
#

# To build the Qt5-based GUI, make sure you have the Qt5 Widgets, Core, and Gui libraries installed, then 
# configure with:
#   cd build
#   cmake -D BUILD_SLIMGUI=ON ../SLiM
#   make -j10


# In all cases the concept is the same: make a build directory of some name, cd into it, run cmake
# to set up the build (with a CMAKE_BUILD_TYPE flag if desired, otherwise Release will be used by
# default), then run make to actually do the build.  This setup (1) keeps all build products out of
# your source tree, which is generally a good idea, and (2) allows you to have both Release and
# Debug builds going simultaneously.
#
# You can do "make VERBOSE=1" instead of just "make" to see the full command lines used.  There are
# also various targets defined by cmake for make, such as "slim", "eidos", "clean", "all", etc.  To
# rebuild all of cmake's internal caches etc. (which is generally a good idea after a "git pull",
# for example, or after the addition or removal of source files), the simplest thing is generally
# to touch the CMakeLists.txt file in the source tree top-level directory:
#
#   touch ../SLiM/CMakeLists.txt
#
# Then you can just do "make"; cmake will automatically be re-run by make since the CMakeLists.txt
# file has changed.


cmake_minimum_required (VERSION 2.8 FATAL_ERROR)

project(SLiM)


# Make a Release build by default
if(NOT CMAKE_BUILD_TYPE) 
    set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)

# User-defined CMAKE command-line options
option(BUILD_SLIMGUI "Build the Qt5-based GUI for SLiM" OFF)


# Use the flags below for [all / Debug / Release] builds; these flags are built in to cmake
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wno-attributes -Wunused-label -Wimplicit -Wunused-variable -Wunused-value -Wno-pragmas -Wempty-body -Wshadow -Wparentheses -Wmissing-prototypes -Wswitch -Wpointer-sign -Wsign-compare -Wstrict-prototypes -Wno-sign-conversion -Wuninitialized")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-attributes -Wunused-label -Wunused-variable -Wunused-value -Wno-pragmas -Wempty-body -Wshadow -Wparentheses -Wswitch -Wsign-compare -Wno-sign-conversion -Wuninitialized")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -Og -DDEBUG=1 -DSLIMPROFILING=0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Og -DDEBUG=1 -DSLIMPROFILING=0")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -DSLIMPROFILING=1")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DSLIMPROFILING=1")

# Report the build type and SLiMgui build status
message("CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")

if(BUILD_SLIMGUI)
	message("BUILD_SLIMGUI is ${BUILD_SLIMGUI}")
else()
	message("BUILD_SLIMGUI is ${BUILD_SLIMGUI}; use -DBUILD_SLIMGUI=ON to enable building SLiMgui")
endif()

# Test for -flto support
# BCH 4/4/2019: I am disabling this LTO stuff for now.  It made only a very small performance
# difference, and multiple users reported build problems associated with it (see Issue #33).
#include(CheckCXXCompilerFlag)
#include(CheckCCompilerFlag)
#CHECK_CXX_COMPILER_FLAG(-flto CXX_SUPPORTS_FLTO)
#CHECK_C_COMPILER_FLAG(-flto C_SUPPORTS_FLTO)
#if(CXX_SUPPORTS_FLTO AND C_SUPPORTS_FLTO)
#    message(STATUS "Compiling with FLTO support")
#    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
#endif()

# GSL 
set(TARGET_NAME gsl)
file(GLOB_RECURSE GSL_SOURCES ${PROJECT_SOURCE_DIR}/gsl/*.c ${PROJECT_SOURCE_DIR}/gsl/*/*.c)
set(GSL_INCLUDES ${PROJECT_SOURCE_DIR}/gsl ${PROJECT_SOURCE_DIR}/gsl/specfunc ${PROJECT_SOURCE_DIR}/gsl/blas ${PROJECT_SOURCE_DIR}/gsl/rng ${PROJECT_SOURCE_DIR}/gsl/cdf ${PROJECT_SOURCE_DIR}/gsl/vector ${PROJECT_SOURCE_DIR}/gsl/err ${PROJECT_SOURCE_DIR}/gsl/sys ${PROJECT_SOURCE_DIR}/gsl/randist ${PROJECT_SOURCE_DIR}/gsl/matrix ${PROJECT_SOURCE_DIR}/gsl/cblas ${PROJECT_SOURCE_DIR}/gsl/complex ${PROJECT_SOURCE_DIR}/gsl/block ${PROJECT_SOURCE_DIR}/gsl/linalg)
add_library(${TARGET_NAME} STATIC ${GSL_SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC ${GSL_INCLUDES})

# ZLIB
set(TARGET_NAME eidos_zlib)
file(GLOB_RECURSE ZLIB_SOURCES ${PROJECT_SOURCE_DIR}/eidos_zlib/*.c)
set(ZLIB_INCLUDES ${PROJECT_SOURCE_DIR}/eidos_zlib)
add_library(${TARGET_NAME} STATIC ${ZLIB_SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC)

# KASTORE
set(TARGET_NAME kastore)
file(GLOB_RECURSE KASTORE_SOURCES ${PROJECT_SOURCE_DIR}/treerec/tskit/kastore/*.c)
set(KASTORE_INCLUDES ${PROJECT_SOURCE_DIR}/treerec/tskit/kastore)
add_library(${TARGET_NAME} STATIC ${KASTORE_SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC)

# TSKIT
set(TARGET_NAME tables)
file(GLOB_RECURSE TABLE_SOURCES ${PROJECT_SOURCE_DIR}/treerec/tskit/*.c)
set(TSKIT_INCLUDES ${PROJECT_SOURCE_DIR}/treerec)
add_library(${TARGET_NAME} STATIC ${TABLE_SOURCES})
target_include_directories(${TARGET_NAME} PRIVATE ${GSL_INCLUDES} ${KASTORE_INCLUDES})
target_include_directories(${TARGET_NAME} PUBLIC ${KASTORE_INCLUDES} ${TSKIT_INCLUDES})

set(TARGET_NAME slim)
file(GLOB_RECURSE SLIM_SOURCES ${PROJECT_SOURCE_DIR}/core/*.cpp ${PROJECT_SOURCE_DIR}/eidos/*.cpp)
add_executable(${TARGET_NAME} ${SLIM_SOURCES})
target_include_directories(${TARGET_NAME} PRIVATE ${GSL_INCLUDES} "${PROJECT_SOURCE_DIR}/core" "${PROJECT_SOURCE_DIR}/eidos")
target_link_libraries(${TARGET_NAME} PUBLIC gsl eidos_zlib tables)

set(TARGET_NAME eidos)
file(GLOB_RECURSE EIDOS_SOURCES  ${PROJECT_SOURCE_DIR}/eidos/*.cpp  ${PROJECT_SOURCE_DIR}/eidostool/*.cpp)
add_executable(${TARGET_NAME} ${EIDOS_SOURCES})
target_include_directories(${TARGET_NAME} PRIVATE ${GSL_INCLUDES} "${PROJECT_SOURCE_DIR}/eidos")
target_link_libraries(${TARGET_NAME} PUBLIC gsl eidos_zlib tables)

install(TARGETS slim eidos DESTINATION bin)

# SLiMgui -- this can be enabled with the -DBUILD_SLIMGUI=ON option to cmake
if(BUILD_SLIMGUI)
cmake_minimum_required (VERSION 3.1.0 FATAL_ERROR)
set(TARGET_NAME SLiMgui)
find_package(OpenGL REQUIRED)
find_package(Qt5 REQUIRED
    Core
    Gui
    Widgets
)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
list(REMOVE_ITEM SLIM_SOURCES ${PROJECT_SOURCE_DIR}/core/main.cpp)
file(GLOB_RECURSE QTSLIM_SOURCES ${PROJECT_SOURCE_DIR}/QtSLiM/*.cpp ${PROJECT_SOURCE_DIR}/QtSLiM/*.qrc ${PROJECT_SOURCE_DIR}/eidos/*.cpp)
add_executable(${TARGET_NAME} "${QTSLIM_SOURCES}" "${SLIM_SOURCES}")
set_target_properties( ${TARGET_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions( ${TARGET_NAME} PRIVATE EIDOSGUI=1 SLIMGUI=1)
target_include_directories(${TARGET_NAME} PUBLIC ${GSL_INCLUDES} "${PROJECT_SOURCE_DIR}/QtSLiM" "${PROJECT_SOURCE_DIR}/eidos" "${PROJECT_SOURCE_DIR}/core" "${PROJECT_SOURCE_DIR}/treerec" "${PROJECT_SOURCE_DIR}/treerec/tskit/kastore")
if(APPLE)
	target_link_libraries( ${TARGET_NAME} PUBLIC Qt5::Widgets Qt5::Core Qt5::Gui OpenGL::GL gsl tables eidos_zlib /usr/lib/libobjc.A.dylib )
else()
	target_link_libraries( ${TARGET_NAME} PUBLIC Qt5::Widgets Qt5::Core Qt5::Gui OpenGL::GL gsl tables eidos_zlib )
endif()
install(TARGETS ${TARGET_NAME} DESTINATION bin)
endif(BUILD_SLIMGUI)

