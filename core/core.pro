#-------------------------------------------------
#
# Project created by QtCreator 2019-07-10T21:52:57
#
#-------------------------------------------------

QT       -= core gui

TARGET = core
TEMPLATE = lib

DEFINES += CORE_LIBRARY

CONFIG += c++11
QMAKE_CFLAGS_DEBUG += -g -Og -DDEBUG=1
QMAKE_CFLAGS_RELEASE += -O3
QMAKE_CXXFLAGS_DEBUG += -g -Og -DDEBUG=1
QMAKE_CXXFLAGS_RELEASE += -O3

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

