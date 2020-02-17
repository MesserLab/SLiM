#-------------------------------------------------
#
# Project created by QtCreator 2019-07-10T21:52:05
#
#-------------------------------------------------

QT       -= core gui

TARGET = eidos
TEMPLATE = lib

DEFINES += EIDOS_LIBRARY

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

SOURCES += \
    eidos_ast_node.cpp \
    eidos_beep.cpp \
    eidos_call_signature.cpp \
    eidos_functions.cpp \
    eidos_globals.cpp \
    eidos_interpreter.cpp \
    eidos_property_signature.cpp \
    eidos_rng.cpp \
    eidos_script.cpp \
    eidos_symbol_table.cpp \
    eidos_test_element.cpp \
    eidos_test.cpp \
    eidos_token.cpp \
    eidos_type_interpreter.cpp \
    eidos_type_table.cpp \
    eidos_value.cpp

HEADERS += \
    eidos_global.h \
    eidos_ast_node.h \
    eidos_beep.h \
    eidos_call_signature.h \
    eidos_functions.h \
    eidos_globals.h \
    eidos_interpreter.h \
    eidos_intrusive_ptr.h \
    eidos_object_pool.h \
    eidos_property_signature.h \
    eidos_rng.h \
    eidos_script.h \
    eidos_symbol_table.h \
    eidos_test_builtins.h \
    eidos_test_element.h \
    eidos_test.h \
    eidos_token.h \
    eidos_type_interpreter.h \
    eidos_type_table.h \
    eidos_value.h
