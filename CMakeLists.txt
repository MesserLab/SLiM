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
# to the number of cores available on your machine (typically less than or equal to the number of cores).
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
# To build the Qt5-based GUI, make sure you have the Qt5 Widgets, Core, and Gui libraries installed,
# then configure with:
#   cd build
#   cmake -D BUILD_SLIMGUI=ON ../SLiM
#   make -j10
#
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


cmake_minimum_required (VERSION 2.8.12 FATAL_ERROR)


# clang-tidy support (for internal development); must come before the project() line
# note that the hard-coded paths below will need to be fixed for other platforms
option(TIDY "Run clang-tidy on SLiM (for development)" OFF)

if(TIDY)
	cmake_minimum_required(VERSION 3.6 FATAL_ERROR)
	message(STATUS "TIDY is ${TIDY}; building with clang-tidy (for development)")
	set(CMAKE_C_COMPILER "/opt/local/libexec/llvm-17/bin/clang")
	set(CMAKE_CXX_COMPILER "/opt/local/libexec/llvm-17/bin/clang++")
	find_program(CLANG_TIDY_EXE NAMES "clang-tidy" PATHS "/opt/local/libexec/llvm-17/bin/" NO_DEFAULT_PATH REQUIRED)
	set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}" "-checks=-*,modernize-*,-modernize-redundant-void-arg,-modernize-use-trailing-return-type,-modernize-use-auto,-modernize-avoid-c-arrays,-modernize-use-equals-default,-modernize-deprecated-headers,-modernize-use-nullptr,-modernize-return-braced-init-list,-modernize-use-using,bugprone-*,-bugprone-narrowing-conversions,-bugprone-easily-swappable-parameters,-bugprone-reserved-identifier,-bugprone-suspicious-include,performance-*,-performance-avoid-endl,-performance-inefficient-string-concatenation")
	message(STATUS "+++ clang-tidy is at ${CLANG_TIDY_EXE}")
	message(STATUS "+++ CLANG_TIDY_COMMAND is ${CLANG_TIDY_COMMAND}")
endif()


project(SLiM)
if(WIN32)
    include(ExternalProject)
endif()


#
#	BUILD OPTIONS
#

# Make a Release build by default
if(NOT CMAKE_BUILD_TYPE) 
    set(CMAKE_BUILD_TYPE Release)
endif(NOT CMAKE_BUILD_TYPE)

# Add "-D BUILD_SLIMGUI=ON" to the CMake command to build SLiMgui
# This requires that Qt 5, a widget framework, is installed
option(BUILD_SLIMGUI "Build the Qt5-based GUI for SLiM" OFF)

# Add "-D PARALLEL=ON" to the CMake command to make a parallel (multi-threaded) build
# This is supported only for the command-line tools, not for SLiMgui; do not set BUILD_SLIMGUI
option(PARALLEL "Build parallel versions of eidos and slim" OFF)

if(PARALLEL)
	# BCH 12/4/2023: Parallel SLiM is presently unreleased and unsupported.  Caveat lector.
	message(FATAL_ERROR "Multithreaded SLiM is not released, not thoroughly tested, and generally not yet ready for prime time.  It is not recommended for end-user use, especially not for "production" runs, and the documentation for it is not yet public.  Please do not ask for any kind of support for this feature if you choose to experiment with it.")
endif(PARALLEL)

# Add "-D PROFILE=ON" to the CMake command to make a build of command-line slim with runtime profiling
# This makes SLiM slower, so it is not for production use.  It may be set ON only for Release builds.
option(PROFILE "Build slim for runtime profiling" OFF)

# Add "-D BUILD_NATIVE=ON" to build natively for the build machine's architecture
# this can result in better optimization, but the executable will only run on the build machine,
# so it should only be enabled when you are building on the same machine you will do your runs on
option(BUILD_NATIVE "Build native for the build machine" OFF)

# Add "-D BUILD_LTO=ON" to enable link-time optimization; this may improve performance slightly,
# but fails (see issue #33) on some machines with incompatible toolchains (so then don't enable it)
option(BUILD_LTO "Build with link-time optimization" OFF)


# obtain the Git commit SHA-1; see ./cmake/_README.txt and https://stackoverflow.com/a/4318642/2752221
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
#message(STATUS "GIT_SHA1 is ${GIT_SHA1}")


# Use the flags below for [all / Debug / Release] builds; these flags are built in to cmake
# Note that -fno-math-errno is deliberately set for C++ (for eidos and slim) but not for C (for gsl, eidos_zlib, kastore, tables)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wno-attributes -Wunused-label -Wimplicit -Wunused-variable -Wunused-value -Wno-pragmas -Wempty-body -Wshadow -Wparentheses -Wmissing-prototypes -Wswitch -Wpointer-sign -Wsign-compare -Wstrict-prototypes -Wno-sign-conversion -Wuninitialized")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-attributes -Wunused-label -Wunused-variable -Wunused-value -Wno-pragmas -Wempty-body -Wshadow -Wparentheses -Wswitch -Wsign-compare -Wno-sign-conversion -Wuninitialized -fno-math-errno")

# Add -march=native if requested
if(BUILD_NATIVE)
	message(STATUS "BUILD_NATIVE is ${BUILD_NATIVE}; building native (for this machine only)")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()

# Windows specific flags and variables
if(WIN32)
    set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ -lwsock32 -lws2_32 ${CMAKE_CXX_STANDARD_LIBRARIES}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
    set(GNULIB_NAMESPACE_SOURCES "${PROJECT_SOURCE_DIR}/core/chromosome.cpp" 
        "${PROJECT_SOURCE_DIR}/core/genome.cpp" 
        "${PROJECT_SOURCE_DIR}/core/population.cpp"
        "${PROJECT_SOURCE_DIR}/core/slim_globals.cpp"
        "${PROJECT_SOURCE_DIR}/core/slim_sim_eidos.cpp"
        "${PROJECT_SOURCE_DIR}/core/slim_sim.cpp"
        "${PROJECT_SOURCE_DIR}/core/species.cpp"
        "${PROJECT_SOURCE_DIR}/core/species_eidos.cpp"
        "${PROJECT_SOURCE_DIR}/core/subpopulation.cpp"
        "${PROJECT_SOURCE_DIR}/eidos/eidos_functions_files.cpp"
        "${PROJECT_SOURCE_DIR}/eidos/eidos_functions_other.cpp"
        "${PROJECT_SOURCE_DIR}/eidos/eidos_globals.cpp"
        "${PROJECT_SOURCE_DIR}/eidos/eidos_class_DataFrame.cpp"
        "${PROJECT_SOURCE_DIR}/QtSLiM/QtSLiMAbout.cpp"
        "${PROJECT_SOURCE_DIR}/QtSLiM/QtSLiMAppDelegate.cpp"
        "${PROJECT_SOURCE_DIR}/QtSLiM/QtSLiMFindRecipe.cpp"
        "${PROJECT_SOURCE_DIR}/QtSLiM/QtSLiMGraphView.cpp"
        "${PROJECT_SOURCE_DIR}/QtSLiM/QtSLiMHaplotypeOptions.cpp"
        "${PROJECT_SOURCE_DIR}/QtSLiM/QtSLiMHelpWindow.cpp"
        "${PROJECT_SOURCE_DIR}/QtSLiM/QtSLiMScriptTextEdit.cpp"
        "${PROJECT_SOURCE_DIR}/QtSLiM/QtSLiMVariableBrowser.cpp"
        "${PROJECT_SOURCE_DIR}/QtSLiM/QtSLiMWindow.cpp")
endif()

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -Og -DDEBUG=1")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Og -DDEBUG=1")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Report whether the build is parallel or not
if(PARALLEL)
	# -Xclang is also needed on macOS; it is not set here since macOS builds are done in Xcode
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
	link_directories(/usr/local/lib/)
	message(STATUS "PARALLEL is ${PARALLEL}")
else()
	# At least for now, I don't want to advertise the existence of this flag
	#message(STATUS "PARALLEL is ${PARALLEL}; use -DPARALLEL=ON to enable OpenMP parallelization")
endif(PARALLEL)

# Report whether the build has runtime profiling support or not
if(PROFILE)
	# compile flags for this get set at the end of this script
	message(STATUS "PROFILE is ${PROFILE}")
else()
	# At least for now, I don't want to advertise the existence of this flag
	#message(STATUS "PROFILE is ${PROFILE}; use -PROFILE=ON to enable runtime profiling")
endif(PROFILE)

# Report the build type and SLiMgui build status
message(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")

if(BUILD_SLIMGUI)
	message(STATUS "BUILD_SLIMGUI is ${BUILD_SLIMGUI}")
else()
	message(STATUS "BUILD_SLIMGUI is ${BUILD_SLIMGUI}; use -DBUILD_SLIMGUI=ON to enable building SLiMgui")
endif()

# Windows compat
if(WIN32)
    set(GNU_DIR ${CMAKE_CURRENT_BINARY_DIR}/libgnu-prefix/src/libgnu-build)
    set(GNU_STATIC_LIB ${GNU_DIR}/gllib/libgnu.a)
    set(GNU_INCLUDES ${GNU_DIR}/gllib)

    file(MAKE_DIRECTORY ${GNU_INCLUDES})

    ExternalProject_Add(libgnu
        SOURCE_DIR ${PROJECT_SOURCE_DIR}/windows_compat/gnulib
        CONFIGURE_COMMAND ${PROJECT_SOURCE_DIR}/windows_compat/gnulib/configure
        BUILD_COMMAND ${MAKE}
        BUILD_BYPRODUCTS ${GNU_STATIC_LIB})

    add_library(gnu STATIC IMPORTED GLOBAL)
    add_dependencies(gnu libgnu)
    set_target_properties(gnu PROPERTIES IMPORTED_LOCATION ${GNU_STATIC_LIB})
    set_target_properties(gnu PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${GNU_INCLUDES})
    set_target_properties(gnu PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${GNU_INCLUDES})
endif()

# Do link-time optimization with -flto if requested and supported
if(BUILD_LTO)
    include(CheckCXXCompilerFlag)
    include(CheckCCompilerFlag)
    CHECK_CXX_COMPILER_FLAG(-flto CXX_SUPPORTS_FLTO)
    CHECK_C_COMPILER_FLAG(-flto C_SUPPORTS_FLTO)
    if(CXX_SUPPORTS_FLTO AND C_SUPPORTS_FLTO)
        message(STATUS "BUILD_LTO is ${BUILD_LTO}; building with link-time optimization")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
    else()
        message(AUTHOR_WARNING "BUILD_LTO is ${BUILD_LTO} but -flto is not supported by the compiler")
    endif()
endif()

# GSL - adding /usr/local/include so all targets that use GSL_INCLUDES get omp.h
set(TARGET_NAME gsl)
file(GLOB_RECURSE GSL_SOURCES ${PROJECT_SOURCE_DIR}/gsl/*.c ${PROJECT_SOURCE_DIR}/gsl/*/*.c)
set(GSL_INCLUDES ${PROJECT_SOURCE_DIR}/gsl ${PROJECT_SOURCE_DIR}/gsl/specfunc ${PROJECT_SOURCE_DIR}/gsl/blas ${PROJECT_SOURCE_DIR}/gsl/rng ${PROJECT_SOURCE_DIR}/gsl/cdf ${PROJECT_SOURCE_DIR}/gsl/vector ${PROJECT_SOURCE_DIR}/gsl/err ${PROJECT_SOURCE_DIR}/gsl/sys ${PROJECT_SOURCE_DIR}/gsl/randist ${PROJECT_SOURCE_DIR}/gsl/matrix ${PROJECT_SOURCE_DIR}/gsl/cblas ${PROJECT_SOURCE_DIR}/gsl/complex ${PROJECT_SOURCE_DIR}/gsl/block ${PROJECT_SOURCE_DIR}/gsl/interpolation ${PROJECT_SOURCE_DIR}/gsl/linalg /usr/local/include)
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
if(WIN32)
    target_link_libraries(${TARGET_NAME} PUBLIC gnu)
endif()

# SLIM
if(PARALLEL)
	set(TARGET_NAME slim_multi)
else()
	set(TARGET_NAME slim)
endif(PARALLEL)
file(GLOB_RECURSE SLIM_SOURCES ${PROJECT_SOURCE_DIR}/core/*.cpp ${PROJECT_SOURCE_DIR}/eidos/*.cpp)

# use the Git commit SHA-1 obtained above
configure_file("${PROJECT_SOURCE_DIR}/cmake/GitSHA1.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/GitSHA1.cpp" @ONLY)
list(APPEND SLIM_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/GitSHA1.cpp" ${PROJECT_SOURCE_DIR}/cmake/GitSHA1.h)

add_executable(${TARGET_NAME} ${SLIM_SOURCES})
target_include_directories(${TARGET_NAME} PRIVATE ${GSL_INCLUDES} "${PROJECT_SOURCE_DIR}/core" "${PROJECT_SOURCE_DIR}/eidos")
target_link_libraries(${TARGET_NAME} PUBLIC gsl eidos_zlib tables)
if(PARALLEL)
	# linking in the OpenMP library is maybe automatic with gcc?
	#target_link_libraries(${TARGET_NAME} PUBLIC omp)
endif()
if(WIN32)
    set_source_files_properties(${SLIM_SOURCES} PROPERTIES COMPILE_FLAGS "-include config.h")
    set_source_files_properties(${GNULIB_NAMESPACE_SOURCES} TARGET_DIRECTORY slim PROPERTIES COMPILE_FLAGS "-include config.h -DGNULIB_NAMESPACE=gnulib")
    target_include_directories(${TARGET_NAME} BEFORE PUBLIC ${GNU_DIR})
    target_link_libraries(${TARGET_NAME} PUBLIC gnu)
endif()

# EIDOS
if(PARALLEL)
	set(TARGET_NAME eidos_multi)
else()
	set(TARGET_NAME eidos)
endif(PARALLEL)

file(GLOB_RECURSE EIDOS_SOURCES  ${PROJECT_SOURCE_DIR}/eidos/*.cpp  ${PROJECT_SOURCE_DIR}/eidostool/*.cpp)
add_executable(${TARGET_NAME} ${EIDOS_SOURCES})
target_include_directories(${TARGET_NAME} PRIVATE ${GSL_INCLUDES} "${PROJECT_SOURCE_DIR}/eidos")
target_link_libraries(${TARGET_NAME} PUBLIC gsl eidos_zlib tables)
if(PARALLEL)
	# linking in the OpenMP library is maybe automatic with gcc?
	#target_link_libraries(${TARGET_NAME} PUBLIC omp)
endif()
if(WIN32)
    set_source_files_properties(${EIDOS_SOURCES} PROPERTIES COMPILE_FLAGS "-include config.h")
    set_source_files_properties(${GNULIB_NAMESPACE_SOURCES} TARGET_DIRECTORY slim eidos PROPERTIES COMPILE_FLAGS "-include config.h -DGNULIB_NAMESPACE=gnulib")
    target_include_directories(${TARGET_NAME} BEFORE PUBLIC ${GNU_DIR})
    target_link_libraries(${TARGET_NAME} PUBLIC gnu)
endif()

if(PARALLEL)
	install(TARGETS slim_multi eidos_multi DESTINATION bin)
else()
	install(TARGETS slim eidos DESTINATION bin)
endif(PARALLEL)

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
if(WIN32)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_autogen/mocs_compilation.cpp" PROPERTIES COMPILE_FLAGS "-include config.h -DGNULIB_NAMESPACE=gnulib")
endif()
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
	target_link_libraries( ${TARGET_NAME} PUBLIC Qt5::Widgets Qt5::Core Qt5::Gui OpenGL::GL gsl tables eidos_zlib )
else()
    if(WIN32)
        set_source_files_properties(${QTSLIM_SOURCES} PROPERTIES COMPILE_FLAGS "-include config.h")
        set_source_files_properties(${GNULIB_NAMESPACE_SOURCES} TARGET_DIRECTORY slim eidos SLiMgui PROPERTIES COMPILE_FLAGS "-include config.h -DGNULIB_NAMESPACE=gnulib")
        target_include_directories(${TARGET_NAME} BEFORE PUBLIC ${GNU_DIR})
        target_link_libraries(${TARGET_NAME} PUBLIC Qt5::Widgets Qt5::Core Qt5::Gui OpenGL::GL gsl tables eidos_zlib gnu )
    else()
	    target_link_libraries( ${TARGET_NAME} PUBLIC Qt5::Widgets Qt5::Core Qt5::Gui OpenGL::GL gsl tables eidos_zlib )
        # Install icons and desktop files to the data root directory (usually /usr/local/share, or /usr/share).
	    install(DIRECTORY data/ TYPE DATA)
    endif() 
endif()
install(TARGETS ${TARGET_NAME} DESTINATION bin)
endif(BUILD_SLIMGUI)


# Deal with the PROFILE and PARALLEL flags, which interact and are handled in a complex way.
#
# For SLiMgui, profiling is always on for Release builds, always off for Debug builds; PROFILE does not affect it
# For slim, profiling follows the PROFILE setting for Release builds, but in Debug builds it is an error for PROFILE to be on
# For eidos, profiling is always off; the eidos command-line tool does not support profiling on its own, only in SLiM
#
# Note that SLiMgui cannot be built parallel; if you want to build parallel, do not set BUILD_SLIMGUI

if(PROFILE)
	if(CMAKE_BUILD_TYPE STREQUAL Debug)
		message(FATAL_ERROR "PROFILE is not allowed for Debug builds")
	endif()

	if(PARALLEL)
		target_compile_definitions( eidos_multi PRIVATE SLIMPROFILING=0)
		target_compile_definitions( slim_multi PRIVATE SLIMPROFILING=1)
	else()
		target_compile_definitions( eidos PRIVATE SLIMPROFILING=0)
		target_compile_definitions( slim PRIVATE SLIMPROFILING=1)
	endif(PARALLEL)
else()
	if(PARALLEL)
		target_compile_definitions( eidos_multi PRIVATE SLIMPROFILING=0)
		target_compile_definitions( slim_multi PRIVATE SLIMPROFILING=0)
	else()
		target_compile_definitions( eidos PRIVATE SLIMPROFILING=0)
		target_compile_definitions( slim PRIVATE SLIMPROFILING=0)
	endif(PARALLEL)
endif(PROFILE)

if(BUILD_SLIMGUI)
	if(CMAKE_BUILD_TYPE STREQUAL Release)
		target_compile_definitions( SLiMgui PRIVATE SLIMPROFILING=1)
	else()
		target_compile_definitions( SLiMgui PRIVATE SLIMPROFILING=0)
	endif(CMAKE_BUILD_TYPE STREQUAL Release)

	if(PARALLEL)
		message(FATAL_ERROR "PARALLEL is not allowed for SLiMgui; running SLiMgui multi-threaded is not supported.  If you wish to build SLiM parallel, do not set BUILD_SLIMGUI for that build.")
	endif()
endif(BUILD_SLIMGUI)

# implement clang-tidy for all end-user targets (not for gsl, zlib, kastore, tskit)
if(TIDY)
	if(PARALLEL)
		set_target_properties(eidos_multi PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
		set_target_properties(slim_multi PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
	else()
		set_target_properties(eidos PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
		set_target_properties(slim PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
	endif(PARALLEL)
	if(BUILD_SLIMGUI)
		set_target_properties(SLiMgui PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
	endif(BUILD_SLIMGUI)
endif()





