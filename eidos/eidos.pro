#-------------------------------------------------
#
# Project created by QtCreator 2019-07-10T21:52:05
#
#-------------------------------------------------

QT       -= core gui

TEMPLATE = lib
CONFIG += staticlib


# Uncomment the lines below to enable ASAN (Address Sanitizer), for debugging of memory issues, in every
# .pro file project-wide.  See https://clang.llvm.org/docs/AddressSanitizer.html for discussion of ASAN
# You may also want to set ASAN_OPTIONS, in the Run Settings section of the Project tab in Qt Creator, to
# strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1
# This also enables undefined behavior sanitizing, in conjunction with ASAN, because why not.
#CONFIG += sanitizer sanitize_address sanitize_undefined


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


# prevent link dependency cycles
QMAKE_LFLAGS += $$QMAKE_LFLAGS_NOUNDEF

# gsl library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../gsl/release/ -lgsl
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../gsl/debug/ -lgsl
else:unix: LIBS += -L$$OUT_PWD/../gsl/ -lgsl
INCLUDEPATH += $$PWD/../gsl $$PWD/../gsl/blas $$PWD/../gsl/block $$PWD/../gsl/cblas $$PWD/../gsl/cdf
INCLUDEPATH += $$PWD/../gsl/complex $$PWD/../gsl/err $$PWD/../gsl/linalg $$PWD/../gsl/matrix
INCLUDEPATH += $$PWD/../gsl/randist $$PWD/../gsl/rng $$PWD/../gsl/specfunc $$PWD/../gsl/sys $$PWD/../gsl/vector
DEPENDPATH += $$PWD/../gsl
win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gsl/release/libgsl.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gsl/debug/libgsl.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gsl/release/gsl.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gsl/debug/gsl.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../gsl/libgsl.a

# eidos_zlib dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../eidos_zlib/release/ -leidos_zlib
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../eidos_zlib/debug/ -leidos_zlib
else:unix: LIBS += -L$$OUT_PWD/../eidos_zlib/ -leidos_zlib
INCLUDEPATH += $$PWD/../eidos_zlib
DEPENDPATH += $$PWD/../eidos_zlib
win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos_zlib/release/libeidos_zlib.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos_zlib/debug/libeidos_zlib.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos_zlib/release/eidos_zlib.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos_zlib/debug/eidos_zlib.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../eidos_zlib/libeidos_zlib.a


SOURCES += \
    eidos_ast_node.cpp \
    eidos_beep.cpp \
    eidos_call_signature.cpp \
    eidos_dictionary.cpp \
    eidos_functions.cpp \
    eidos_globals.cpp \
    eidos_interpreter.cpp \
    eidos_property_signature.cpp \
    eidos_rng.cpp \
    eidos_script.cpp \
    eidos_symbol_table.cpp \
    eidos_test_element.cpp \
    eidos_test.cpp \
    eidos_test_functions_math.cpp \
    eidos_test_functions_other.cpp \
    eidos_test_functions_statistics.cpp \
    eidos_test_functions_vector.cpp \
    eidos_test_operators_arithmetic.cpp \
    eidos_test_operators_comparison.cpp \
    eidos_test_operators_other.cpp \
    eidos_token.cpp \
    eidos_type_interpreter.cpp \
    eidos_type_table.cpp \
    eidos_value.cpp

HEADERS += \
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
    eidos_tinycolormap.h \
    eidos_token.h \
    eidos_type_interpreter.h \
    eidos_type_table.h \
    eidos_value.h


