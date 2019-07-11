#-------------------------------------------------
#
# Project created by QtCreator 2019-07-10T22:41:14
#
#-------------------------------------------------

QT       -= core gui

TARGET = tskit
TEMPLATE = lib

DEFINES += TSKIT_LIBRARY

CONFIG -= qt
QMAKE_CFLAGS_DEBUG += -g -Og -DDEBUG=1
QMAKE_CFLAGS_RELEASE += -O3

INCLUDEPATH = . .. kastore

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
    tskit_global.h \
    convert.h \
    core.h \
    genotypes.h \
    stats.h \
    tables.h \
    text_input.h \
    trees.h \
    kastore/kastore.h

