# To use CMAKE, create a new subdirectory in your source directory, e.g., build, then run 
# the following commands: 
#   cd build
#   cmake ..
#   make
# Example session can be seen in example_cmake_output.txt

cmake_minimum_required (VERSION 2.6)

# Use the flags below for all builds and report the build type
set(CMAKE_C_FLAGS "-g -std=c11 -O3 -Wno-deprecated-register -Wno-attributes")
set(CMAKE_CXX_FLAGS "-g -std=c++11 -O3 -Wno-deprecated-register -Wno-attributes") 
message("CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")

# Place slim and eidos in the bin subdirectory of the source directory
set( EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin )

find_package(HDF5 REQUIRED)
#include_directories(${HDF5_INCLUDE_DIR})

set(TARGET_NAME tables)
file(GLOB_RECURSE TABLE_SOURCES ${PROJECT_SOURCE_DIR}/tables-api-files/*.c)
add_library(${TARGET_NAME} STATIC ${TABLE_SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC ${HDF5_INCLUDE_DIR})

# GSL 
set(TARGET_NAME gsl)
file(GLOB_RECURSE GSL_SOURCES ${PROJECT_SOURCE_DIR}/gsl/*.c ${PROJECT_SOURCE_DIR}/gsl/*/*.c)
set(GSL_INCLUDES ${PROJECT_SOURCE_DIR}/gsl ${PROJECT_SOURCE_DIR}/gsl/specfunc ${PROJECT_SOURCE_DIR}/gsl/blas ${PROJECT_SOURCE_DIR}/gsl/rng ${PROJECT_SOURCE_DIR}/gsl/cdf ${PROJECT_SOURCE_DIR}/gsl/vector ${PROJECT_SOURCE_DIR}/gsl/err ${PROJECT_SOURCE_DIR}/gsl/sys ${PROJECT_SOURCE_DIR}/gsl/randist ${PROJECT_SOURCE_DIR}/gsl/matrix ${PROJECT_SOURCE_DIR}/gsl/cblas ${PROJECT_SOURCE_DIR}/gsl/complex ${PROJECT_SOURCE_DIR}/gsl/block ${PROJECT_SOURCE_DIR}/gsl/linalg)
add_library(${TARGET_NAME} STATIC ${GSL_SOURCES})
target_include_directories(${TARGET_NAME} PUBLIC ${GSL_INCLUDES})


set(TARGET_NAME slim)
file(GLOB_RECURSE SLIM_SOURCES ${PROJECT_SOURCE_DIR}/core/*.cpp ${PROJECT_SOURCE_DIR}/eidos/*.cpp)
add_executable(${TARGET_NAME} ${SLIM_SOURCES})
target_include_directories(${TARGET_NAME} PRIVATE ${GSL_INCLUDES} "${PROJECT_SOURCE_DIR}/core" "${PROJECT_SOURCE_DIR}/eidos")
target_link_libraries(${TARGET_NAME} PUBLIC gsl)
target_link_libraries(${TARGET_NAME} PUBLIC tables)

set(TARGET_NAME eidos)
file(GLOB_RECURSE EIDOS_SOURCES  ${PROJECT_SOURCE_DIR}/eidos/*.cpp  ${PROJECT_SOURCE_DIR}/eidostool/*.cpp)
add_executable(${TARGET_NAME} ${EIDOS_SOURCES})
target_include_directories(${TARGET_NAME} PRIVATE ${GSL_INCLUDES} "${PROJECT_SOURCE_DIR}/eidos")
target_link_libraries(${TARGET_NAME} PUBLIC gsl)
target_link_libraries(${TARGET_NAME} PUBLIC tables)

