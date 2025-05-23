#-------------------------------------------------
#
# Project created by QtCreator 2019-07-10T21:56:47
#
#-------------------------------------------------

QT       += core gui opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# OpenGLWidget moved to openglwidgets in Qt6
greaterThan(QT_MAJOR_VERSION, 5): QT += openglwidgets

# Note that the target is SLiMgui now, but the old QtSLiM name lives on throughout the project
TARGET = SLiMgui
TEMPLATE = app

# qmake defines to set up a macOS bundle application build
CONFIG += app_bundle
QMAKE_INFO_PLIST = QtSLiM_Info.plist
ICON = QtSLiM_AppIcon.icns
QMAKE_TARGET_BUNDLE_PREFIX = "org.messerlab"
QMAKE_BUNDLE = "SLiMgui"		# This governs the location of our prefs, which we keep under org.messerlab.SLiMgui
VERSION = 5.0

docIconFiles.files = $$PWD/QtSLiM_DocIcon.icns
docIconFiles.path = Contents/Resources
QMAKE_BUNDLE_DATA += docIconFiles

# Uncomment this line for a production build, to build for both Intel and Apple Silicon.  This only works with Qt6;
# Qt5 for macOS is built for Intel only.  Uncomment this for all components or you will get link errors.
#QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64


# Uncomment the lines below to enable ASAN (Address Sanitizer), for debugging of memory issues, in every
# .pro file project-wide.  See https://clang.llvm.org/docs/AddressSanitizer.html for discussion of ASAN
# Also set the ASAN_OPTIONS env. variable, in the Run Settings section of the Project tab in Qt Creator, to
# strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1
# This also enables undefined behavior sanitizing, in conjunction with ASAN, because why not.
#CONFIG += sanitizer sanitize_address sanitize_undefined


# Get the current Git SHA-1 and put it into a define; see https://stackoverflow.com/questions/27041573/print-git-hash-in-qt-as-macro-created-at-compile-time
# Note that this only runs when qmake runs!  It would be nice to have a solution more like the one we use under CMake.
GIT_HASH="\\\"$$system(git -C \""$$_PRO_FILE_PWD_"\" rev-parse HEAD)\\\""
DEFINES += GIT_SHA1=$$GIT_HASH
#message(Git SHA-1 == $$GIT_HASH)


# Warn and error on usage of deprecated Qt APIs; see also -Wno-deprecated-declarations below
# These flags are for development; we don't want production builds warning or erroring due to deprecation
# DEFINES += QT_DEPRECATED_WARNINGS					# uncomment this to get warnings about deprecated APIs
# DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x050F00    # disables all the APIs deprecated before Qt 5.15.0
# DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060500    # disables all the APIs deprecated before Qt 6.5.0


# Bring in flag settings from the environment; see https://stackoverflow.com/a/17578151/2752221
# Right now I do this only in QtSLiM.pro, to bring a CXXFLAGS setting in from GitHub Actions, but
# these lines could be added to the other .pro files too if that proves useful.
QMAKE_CXXFLAGS += $$(CXXFLAGS)
QMAKE_CFLAGS += $$(CFLAGS)


# Set up to build QtSLiM; note that these settings are set in eidos.pro, core.pro, and QtSLiM.pro
DEFINES += EIDOS_GUI
DEFINES += SLIMGUI=1


# Uncomment this define to disable the use of OpenGL in SLiMgui completely.  This, plus removing the
# link dependency on openglwidgets, should allow you to build SLiMgui without linking OpenGL at all.
# I don't expect end users to need to do this; it is for testing purposes, to ensure than the code
# path used when OpenGL is disabled in Preferences does not inadvertently make any OpenGL calls.
#DEFINES += SLIM_NO_OPENGL


greaterThan(QT_MAJOR_VERSION, 5) {
	# For Qt6 we require C++17 (because Qt6 requires it), but don't use it ourselves
	CONFIG += c++17
	CONFIG += c17
	QMAKE_CFLAGS += -std=c17
} else {
	# For Qt5 we require just C++11
	CONFIG += c++11
	CONFIG += c11
	QMAKE_CFLAGS += -std=c11
}

QMAKE_CFLAGS_DEBUG += -g -Og -DDEBUG=1 -DSLIMPROFILING=0
QMAKE_CFLAGS_RELEASE += -O3 -DSLIMPROFILING=1
QMAKE_CXXFLAGS_DEBUG += -g -Og -DDEBUG=1 -DSLIMPROFILING=0
QMAKE_CXXFLAGS_RELEASE += -O3 -DSLIMPROFILING=1

# get rid of warnings for deprecated declarations (not sure why commenting out QT_DEPRECATED_WARNINGS above is insufficient...)
QMAKE_CXXFLAGS += -Wno-deprecated-declarations

# other warnings we want
QMAKE_CXXFLAGS += -Wshadow

# get rid of spurious errors on Ubuntu, for now
linux-*: {
    QMAKE_CXXFLAGS += -Wno-unknown-pragmas -Wno-attributes -Wno-unused-parameter
}


# prevent link dependency cycles
QMAKE_LFLAGS += $$QMAKE_LFLAGS_NOUNDEF

# core library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../core/release/ -lcore
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../core/debug/ -lcore
else:unix: LIBS += -L$$OUT_PWD/../core/ -lcore
INCLUDEPATH += $$PWD/../core
DEPENDPATH += $$PWD/../core
win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../core/release/libcore.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../core/debug/libcore.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../core/release/core.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../core/debug/core.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../core/libcore.a

# eidos library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../eidos/release/ -leidos
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../eidos/debug/ -leidos
else:unix: LIBS += -L$$OUT_PWD/../eidos/ -leidos
INCLUDEPATH += $$PWD/../eidos
DEPENDPATH += $$PWD/../eidos
win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos/release/libeidos.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos/debug/libeidos.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos/release/eidos.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos/debug/eidos.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../eidos/libeidos.a

# tskit library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../treerec/tskit/release/ -ltskit
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../treerec/tskit/debug/ -ltskit
else:unix: LIBS += -L$$OUT_PWD/../treerec/tskit/ -ltskit
INCLUDEPATH += $$PWD/../treerec/tskit $$PWD/../treerec $$PWD/../treerec/tskit/kastore
DEPENDPATH += $$PWD/../treerec/tskit
win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../treerec/tskit/release/libtskit.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../treerec/tskit/debug/libtskit.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../treerec/tskit/release/tskit.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../treerec/tskit/debug/tskit.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../treerec/tskit/libtskit.a

# gsl library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../gsl/release/ -lgsl
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../gsl/debug/ -lgsl
else:unix: LIBS += -L$$OUT_PWD/../gsl/ -lgsl
INCLUDEPATH += $$PWD/../gsl $$PWD/../gsl/blas $$PWD/../gsl/block $$PWD/../gsl/cblas $$PWD/../gsl/cdf
INCLUDEPATH += $$PWD/../gsl/complex $$PWD/../gsl/err $$PWD/../gsl/interpolation $$PWD/../gsl/linalg $$PWD/../gsl/matrix
INCLUDEPATH += $$PWD/../gsl/randist $$PWD/../gsl/rng $$PWD/../gsl/specfunc $$PWD/../gsl/sys $$PWD/../gsl/vector
DEPENDPATH += $$PWD/../gsl
win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gsl/release/libgsl.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gsl/debug/libgsl.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gsl/release/gsl.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../gsl/debug/gsl.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../gsl/libgsl.a

# eidos_zlib library dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../eidos_zlib/release/ -leidos_zlib
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../eidos_zlib/debug/ -leidos_zlib
else:unix: LIBS += -L$$OUT_PWD/../eidos_zlib/ -leidos_zlib
INCLUDEPATH += $$PWD/../eidos_zlib $$PWD/../eidos_zlib/blas $$PWD/../eidos_zlib/block $$PWD/../eidos_zlib/cblas $$PWD/../eidos_zlib/cdf
INCLUDEPATH += $$PWD/../eidos_zlib/complex $$PWD/../eidos_zlib/err $$PWD/../eidos_zlib/linalg $$PWD/../eidos_zlib/matrix
INCLUDEPATH += $$PWD/../eidos_zlib/randist $$PWD/../eidos_zlib/rng $$PWD/../eidos_zlib/specfunc $$PWD/../eidos_zlib/sys $$PWD/../eidos_zlib/vector
DEPENDPATH += $$PWD/../eidos_zlib
win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos_zlib/release/libeidos_zlib.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos_zlib/debug/libeidos_zlib.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos_zlib/release/eidos_zlib.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$OUT_PWD/../eidos_zlib/debug/eidos_zlib.lib
else:unix: PRE_TARGETDEPS += $$OUT_PWD/../eidos_zlib/libeidos_zlib.a


SOURCES += \
    ../cmake/GitSHA1_qmake.cpp \
    QtSLiMChromosomeWidget_GL.cpp \
    QtSLiMChromosomeWidget_QT.cpp \
    QtSLiMDebugOutputWindow.cpp \
    QtSLiMGraphView_1DPopulationSFS.cpp \
    QtSLiMGraphView_1DSampleSFS.cpp \
    QtSLiMGraphView_2DPopulationSFS.cpp \
    QtSLiMGraphView_2DSampleSFS.cpp \
    QtSLiMGraphView_AgeDistribution.cpp \
    QtSLiMGraphView_CustomPlot.cpp \
    QtSLiMGraphView_LifetimeReproduction.cpp \
    QtSLiMGraphView_MultispeciesPopSizeOverTime.cpp \
    QtSLiMGraphView_PopFitnessDist.cpp \
    QtSLiMGraphView_PopSizeOverTime.cpp \
    QtSLiMGraphView_SubpopFitnessDists.cpp \
    QtSLiMHaplotypeManager_GL.cpp \
    QtSLiMHaplotypeManager_QT.cpp \
    QtSLiMIndividualsWidget_GL.cpp \
    QtSLiMIndividualsWidget_QT.cpp \
    QtSLiMOpenGL.cpp \
    QtSLiM_Plot.cpp \
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
    QtSLiMSyntaxHighlighting.cpp \
    QtSLiMFindRecipe.cpp \
    QtSLiMHelpWindow.cpp \
    QtSLiMScriptTextEdit.cpp \
    QtSLiMEidosConsole.cpp \
    QtSLiMEidosConsole_glue.cpp \
    QtSLiMConsoleTextEdit.cpp \
    QtSLiM_SLiMgui.cpp \
    QtSLiMTablesDrawer.cpp \
    QtSLiMFindPanel.cpp \
    QtSLiMGraphView.cpp \
    QtSLiMGraphView_FixationTimeHistogram.cpp \
    QtSLiMGraphView_LossTimeHistogram.cpp \
    QtSLiMGraphView_PopulationVisualization.cpp \
    QtSLiMGraphView_FitnessOverTime.cpp \
    QtSLiMGraphView_FrequencyTrajectory.cpp \
    QtSLiMHaplotypeManager.cpp \
    QtSLiMHaplotypeOptions.cpp \
    QtSLiMHaplotypeProgress.cpp \
    QtSLiMVariableBrowser.cpp

HEADERS += \
    QtSLiMDebugOutputWindow.h \
    QtSLiMGraphView_1DPopulationSFS.h \
    QtSLiMGraphView_1DSampleSFS.h \
    QtSLiMGraphView_2DPopulationSFS.h \
    QtSLiMGraphView_2DSampleSFS.h \
    QtSLiMGraphView_AgeDistribution.h \
    QtSLiMGraphView_CustomPlot.h \
    QtSLiMGraphView_LifetimeReproduction.h \
    QtSLiMGraphView_MultispeciesPopSizeOverTime.h \
    QtSLiMGraphView_PopFitnessDist.h \
    QtSLiMGraphView_PopSizeOverTime.h \
    QtSLiMGraphView_SubpopFitnessDists.h \
    QtSLiMOpenGL.h \
    QtSLiMOpenGL_Emulation.h \
    QtSLiMWindow.h \
    QtSLiMAppDelegate.h \
    QtSLiMChromosomeWidget.h \
    QtSLiMExtras.h \
    QtSLiMPopulationTable.h \
    QtSLiMIndividualsWidget.h \
    QtSLiMEidosPrettyprinter.h \
    QtSLiMAbout.h \
    QtSLiMPreferences.h \
    QtSLiMSyntaxHighlighting.h \
    QtSLiMFindRecipe.h \
    QtSLiMHelpWindow.h \
    QtSLiMScriptTextEdit.h \
    QtSLiMEidosConsole.h \
    QtSLiMConsoleTextEdit.h \
    QtSLiM_Plot.h \
    QtSLiM_SLiMgui.h \
    QtSLiMTablesDrawer.h \
    QtSLiMFindPanel.h \
    QtSLiMGraphView.h \
    QtSLiMGraphView_FixationTimeHistogram.h \
    QtSLiMGraphView_LossTimeHistogram.h \
    QtSLiMGraphView_PopulationVisualization.h \
    QtSLiMGraphView_FitnessOverTime.h \
    QtSLiMGraphView_FrequencyTrajectory.h \
    QtSLiMHaplotypeManager.h \
    QtSLiMHaplotypeOptions.h \
    QtSLiMHaplotypeProgress.h \
    QtSLiMVariableBrowser.h

FORMS += \
    QtSLiMDebugOutputWindow.ui \
    QtSLiMWindow.ui \
    QtSLiMAbout.ui \
    QtSLiMPreferences.ui \
    QtSLiMFindRecipe.ui \
    QtSLiMHelpWindow.ui \
    QtSLiMEidosConsole.ui \
    QtSLiMTablesDrawer.ui \
    QtSLiMFindPanel.ui \
    QtSLiMHaplotypeOptions.ui \
    QtSLiMHaplotypeProgress.ui \
    QtSLiMVariableBrowser.ui

# Deploy to /Applications on macOS, /usr/local/bin on Linux/Un*x
macx: target.path = /Applications
else: unix:!android: target.path = /usr/local/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    buttons.qrc \
    buttons_DARK.qrc \
    icons.qrc \
    recipes.qrc \
    help.qrc

DISTFILES += \
	QtSLiM_AppIcon.icns \
	QtSLiM_DocIcon.icns

