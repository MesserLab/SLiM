#-------------------------------------------------
#
# Project created by QtCreator 2019-07-10T21:56:47
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QtSLiM
TEMPLATE = app

# Warn and error on usage of deprecated Qt APIs
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11
QMAKE_CFLAGS_DEBUG += -g -Og -DDEBUG=1
QMAKE_CFLAGS_RELEASE += -O3
QMAKE_CXXFLAGS_DEBUG += -g -Og -DDEBUG=1
QMAKE_CXXFLAGS_RELEASE += -O3

SOURCES += \
        main.cpp \
        QtSLiMWindow.cpp

HEADERS += \
        QtSLiMWindow.h

FORMS += \
        QtSLiMWindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
