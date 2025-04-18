#-------------------------------------------------
#
# Project created by QtCreator 2019-07-10T21:52:57
#
#-------------------------------------------------

QT       -= core gui

TEMPLATE = lib
CONFIG += staticlib


# Uncomment this line for a production build, to build for both Intel and Apple Silicon.  This only works with Qt6;
# Qt5 for macOS is built for Intel only.  Uncomment this for all components or you will get link errors.
#QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64


# Uncomment the lines below to enable ASAN (Address Sanitizer), for debugging of memory issues, in every
# .pro file project-wide.  See https://clang.llvm.org/docs/AddressSanitizer.html for discussion of ASAN
# Also set the ASAN_OPTIONS env. variable, in the Run Settings section of the Project tab in Qt Creator, to
# strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1
# This also enables undefined behavior sanitizing, in conjunction with ASAN, because why not.
#CONFIG += sanitizer sanitize_address sanitize_undefined


# Set up to build QtSLiM; note that these settings are set in eidos.pro, core.pro, and QtSLiM.pro
DEFINES += EIDOS_GUI
DEFINES += SLIMGUI=1

CONFIG -= qt
CONFIG += c++11
CONFIG += c11
QMAKE_CFLAGS += -std=c11
QMAKE_CFLAGS_DEBUG += -g -Og -DDEBUG=1 -DSLIMPROFILING=0
QMAKE_CFLAGS_RELEASE += -O3 -DSLIMPROFILING=1
QMAKE_CXXFLAGS_DEBUG += -g -Og -DDEBUG=1 -DSLIMPROFILING=0
QMAKE_CXXFLAGS_RELEASE += -O3 -DSLIMPROFILING=1

# get rid of spurious errors on Ubuntu, for now
linux-*: {
    QMAKE_CXXFLAGS += -Wno-unknown-pragmas -Wno-attributes -Wno-unused-parameter
}


# prevent link dependency cycles
QMAKE_LFLAGS += $$QMAKE_LFLAGS_NOUNDEF

# eidos library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../eidos/release/ -leidos
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../eidos/debug/ -leidos
else:unix: LIBS += -L$$OUT_PWD/../eidos/ -leidos
INCLUDEPATH += $$PWD/../eidos
DEPENDPATH += $$PWD/../eidos
win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos/release/libeidos.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos/debug/libeidos.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos/release/eidos.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos/debug/eidos.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../eidos/libeidos.a

# gsl library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../gsl/release/ -lgsl
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../gsl/debug/ -lgsl
else:unix: LIBS += -L$$OUT_PWD/../gsl/ -lgsl
INCLUDEPATH += $$PWD/../gsl $$PWD/../gsl/blas $$PWD/../gsl/block $$PWD/../gsl/cblas $$PWD/../gsl/cdf
INCLUDEPATH += $$PWD/../gsl/complex $$PWD/../gsl/err $$PWD/../gsl/interpolation $$PWD/../gsl/linalg $$PWD/../gsl/matrix
INCLUDEPATH += $$PWD/../gsl/randist $$PWD/../gsl/rng $$PWD/../gsl/specfunc $$PWD/../gsl/sys $$PWD/../gsl/vector
DEPENDPATH += $$PWD/../gsl
win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gsl/release/libgsl.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gsl/debug/libgsl.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gsl/release/gsl.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gsl/debug/gsl.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../gsl/libgsl.a

# tskit library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../treerec/tskit/release/ -ltskit
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../treerec/tskit/debug/ -ltskit
else:unix: LIBS += -L$$OUT_PWD/../treerec/tskit/ -ltskit
INCLUDEPATH += $$PWD/../treerec/tskit $$PWD/../treerec $$PWD/../treerec/tskit/kastore
DEPENDPATH += $$PWD/../treerec/tskit
win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../treerec/tskit/release/libtskit.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../treerec/tskit/debug/libtskit.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../treerec/tskit/release/tskit.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../treerec/tskit/debug/tskit.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../treerec/tskit/libtskit.a


SOURCES += \
    chromosome.cpp \
    community.cpp \
    community_eidos.cpp \
    genomic_element_type.cpp \
    genomic_element.cpp \
    haplosome.cpp \
    individual.cpp \
    interaction_type.cpp \
    log_file.cpp \
    mutation_run.cpp \
    mutation_type.cpp \
    mutation.cpp \
    polymorphism.cpp \
    population.cpp \
    slim_eidos_block.cpp \
    slim_functions.cpp \
    slim_globals.cpp \
    slim_test.cpp \
    slim_test_core.cpp \
    slim_test_genetics.cpp \
    slim_test_other.cpp \
    sparse_vector.cpp \
    spatial_kernel.cpp \
    spatial_map.cpp \
    species.cpp \
    species_eidos.cpp \
    subpopulation.cpp \
    substitution.cpp

HEADERS += \
    chromosome.h \
    community.h \
    genomic_element_type.h \
    genomic_element.h \
    haplosome.h \
    individual.h \
    interaction_type.h \
    log_file.h \
    mutation_run.h \
    mutation_type.h \
    mutation.h \
    polymorphism.h \
    population.h \
    slim_eidos_block.h \
    slim_functions.h \
    slim_globals.h \
    slim_test.h \
    sparse_vector.h \
    spatial_kernel.h \
    spatial_map.h \
    species.h \
    subpopulation.h \
    substitution.h
