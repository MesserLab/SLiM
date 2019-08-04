#-------------------------------------------------
#
# Project created by QtCreator 2019-07-10T21:56:47
#
#-------------------------------------------------

QT       += core gui opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QtSLiM
TEMPLATE = app

# Warn and error on usage of deprecated Qt APIs
DEFINES += QT_DEPRECATED_WARNINGS
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Set up to build QtSLiM; note that these settings are set in eidos.pro, core.pro, and QtSLiM.pro
DEFINES += EIDOS_GUI
DEFINES += SLIMGUI=1
DEFINES += SLIMPROFILING=0

CONFIG += c++11
QMAKE_CFLAGS_DEBUG += -g -Og -DDEBUG=1
QMAKE_CFLAGS_RELEASE += -O3
QMAKE_CXXFLAGS_DEBUG += -g -Og -DDEBUG=1
QMAKE_CXXFLAGS_RELEASE += -O3

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

# core library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../core/release/ -lcore
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../core/debug/ -lcore
else:unix: LIBS += -L$$OUT_PWD/../core/ -lcore
INCLUDEPATH += $$PWD/../core

# tskit library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../treerec/tskit/release/ -ltskit
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../treerec/tskit/debug/ -ltskit
else:unix: LIBS += -L$$OUT_PWD/../treerec/tskit/ -ltskit
INCLUDEPATH += $$PWD/../treerec/tskit $$PWD/../treerec $$PWD/../treerec/tskit/kastore

SOURCES += \
    main.cpp \
    QtSLiMWindow.cpp \
    QtSLiMAppDelegate.cpp \
    QtSLiMWindow_glue.cpp \
    QtSLiMChromosomeWidget.cpp \
    QtSLiMExtras.cpp \
    QtSLiMPopulationTable.cpp \
    QtSLiMIndividualsWidget.cpp \
    QtSLiMEidosPrettyprinter.cpp \
    QtSLiMAbout.cpp \
    QtSLiMPreferences.cpp \
    QtSLiMSyntaxHighlighting.cpp

HEADERS += \
    QtSLiMWindow.h \
    QtSLiMAppDelegate.h \
    QtSLiMChromosomeWidget.h \
    QtSLiMExtras.h \
    QtSLiMPopulationTable.h \
    QtSLiMIndividualsWidget.h \
    QtSLiMEidosPrettyprinter.h \
    QtSLiMAbout.h \
    QtSLiMPreferences.h \
    QtSLiMSyntaxHighlighting.h

FORMS += \
    QtSLiMWindow.ui \
    QtSLiMAbout.ui \
    QtSLiMPreferences.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    buttons.qrc \
    icons.qrc \
    recipes.qrc
