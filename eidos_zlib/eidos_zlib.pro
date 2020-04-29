#-------------------------------------------------
#
# Project created by QtCreator 2020-04-17T14:17:13
#
#-------------------------------------------------

QT       -= core gui

TARGET = eidos_zlib
TEMPLATE = lib
CONFIG += staticlib


# Uncomment the lines below to enable ASAN (Address Sanitizer), for debugging of memory issues, in every
# .pro file project-wide.  See https://clang.llvm.org/docs/AddressSanitizer.html for discussion of ASAN
# CONFIG += sanitizer sanitize_address


# get rid of spurious errors on Ubuntu, for now
linux-*: {
    QMAKE_CXXFLAGS += -Wno-unknown-pragmas -Wno-pragmas
}


SOURCES += \
    zutil.c \
    adler32.c \
    compress.c \
    crc32.c \
    deflate.c \
    gzlib.c \
    gzwrite.c \
    trees.c

HEADERS += \
    zutil.h \
    crc32.h \
    deflate.h \
    gzguts.h \
    trees.h \
    zconf.h \
    zlib.h

