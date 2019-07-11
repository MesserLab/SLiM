#-------------------------------------------------
#
# Project created by QtCreator 2019-07-10T22:40:37
#
#-------------------------------------------------

QT       -= core gui

TARGET = kastore
TEMPLATE = lib

DEFINES += KASTORE_LIBRARY

QMAKE_CFLAGS_DEBUG += -g -Og -DDEBUG=1
QMAKE_CFLAGS_RELEASE += -O3

SOURCES += \
    kastore.c

HEADERS += \
    kastore_global.h \
    kastore.h

