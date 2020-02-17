#-------------------------------------------------
#
# Project created by QtCreator 2019-07-10T21:52:57
#
#-------------------------------------------------

QT       -= core gui

TARGET = core
TEMPLATE = lib

DEFINES += CORE_LIBRARY

# Set up to build QtSLiM; note that these settings are set in eidos.pro, core.pro, and QtSLiM.pro
DEFINES += EIDOS_GUI
DEFINES += SLIMGUI=1

CONFIG += c++11
CONFIG -= qt
QMAKE_CFLAGS_DEBUG += -g -Og -DDEBUG=1 -DSLIMPROFILING=0
QMAKE_CFLAGS_RELEASE += -O3 -DSLIMPROFILING=1
QMAKE_CXXFLAGS_DEBUG += -g -Og -DDEBUG=1 -DSLIMPROFILING=0
QMAKE_CXXFLAGS_RELEASE += -O3 -DSLIMPROFILING=1

# get rid of spurious errors on Ubuntu, for now
linux-*: {
    QMAKE_CXXFLAGS += -Wno-unknown-pragmas -Wno-attributes -Wno-unused-parameter
}

# gsl library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../gsl/release/ -lgsl
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../gsl/debug/ -lgsl
else:unix: LIBS += -L$$OUT_PWD/../gsl/ -lgsl
INCLUDEPATH += $$PWD/../gsl $$PWD/../gsl/blas $$PWD/../gsl/block $$PWD/../gsl/cblas $$PWD/../gsl/cdf
INCLUDEPATH += $$PWD/../gsl/complex $$PWD/../gsl/err $$PWD/../gsl/linalg $$PWD/../gsl/matrix
INCLUDEPATH += $$PWD/../gsl/randist $$PWD/../gsl/rng $$PWD/../gsl/specfunc $$PWD/../gsl/sys $$PWD/../gsl/vector

# eidos library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../eidos/release/ -leidos
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../eidos/debug/ -leidos
else:unix: LIBS += -L$$OUT_PWD/../eidos/ -leidos
INCLUDEPATH += $$PWD/../eidos

# tskit library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../treerec/tskit/release/ -ltskit
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../treerec/tskit/debug/ -ltskit
else:unix: LIBS += -L$$OUT_PWD/../treerec/tskit/ -ltskit
INCLUDEPATH += $$PWD/../treerec/tskit $$PWD/../treerec $$PWD/../treerec/tskit/kastore

SOURCES += \
    chromosome.cpp \
    genome.cpp \
    genomic_element_type.cpp \
    genomic_element.cpp \
    individual.cpp \
    interaction_type.cpp \
    mutation_run.cpp \
    mutation_type.cpp \
    mutation.cpp \
    polymorphism.cpp \
    population.cpp \
    slim_eidos_block.cpp \
    slim_eidos_dictionary.cpp \
    slim_functions.cpp \
    slim_globals.cpp \
    slim_sim.cpp \
    slim_test.cpp \
    sparse_array.cpp \
    subpopulation.cpp \
    substitution.cpp

HEADERS += \
    core_global.h \
    chromosome.h \
    genome.h \
    genomic_element_type.h \
    genomic_element.h \
    individual.h \
    interaction_type.h \
    json.hpp \
    mutation_run.h \
    mutation_type.h \
    mutation.h \
    polymorphism.h \
    population.h \
    slim_eidos_block.h \
    slim_eidos_dictionary.h \
    slim_functions.h \
    slim_globals.h \
    slim_sim.h \
    slim_test.h \
    sparse_array.h \
    subpopulation.h \
    substitution.h
