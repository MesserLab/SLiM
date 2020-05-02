#-------------------------------------------------
#
# Project created by QtCreator 2019-07-10T22:41:14
#
#-------------------------------------------------

QT       -= core gui

TEMPLATE = lib
CONFIG += staticlib


# Uncomment the lines below to enable ASAN (Address Sanitizer), for debugging of memory issues, in every
# .pro file project-wide.  See https://clang.llvm.org/docs/AddressSanitizer.html for discussion of ASAN
# CONFIG += sanitizer sanitize_address


CONFIG -= qt
QMAKE_CFLAGS_DEBUG += -g -Og -DDEBUG=1
QMAKE_CFLAGS_RELEASE += -O3

# get rid of spurious errors on Ubuntu, for now
linux-*: {
    QMAKE_CFLAGS += -Wno-unknown-pragmas -Wno-attributes -Wno-unused-parameter -Wno-unused-but-set-parameter
}

INCLUDEPATH = . .. kastore


# prevent link dependency cycles
QMAKE_LFLAGS += $$QMAKE_LFLAGS_NOUNDEF


SOURCES += \
    convert.c \
    core.c \
    genotypes.c \
    stats.c \
    tables.c \
    text_input.c \
    trees.c \
    kastore/kastore.c

HEADERS += \
    convert.h \
    core.h \
    genotypes.h \
    stats.h \
    tables.h \
    text_input.h \
    trees.h \
    kastore/kastore.h

