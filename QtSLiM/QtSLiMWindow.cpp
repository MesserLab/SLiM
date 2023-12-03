//
//  QtSLiMWindow.cpp
//  SLiM
//
//  Created by Ben Haller on 7/11/2019.
//  Copyright (c) 2019-2023 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.


#include "QtSLiMWindow.h"
#include "ui_QtSLiMWindow.h"
#include "QtSLiMAppDelegate.h"
#include "QtSLiMPreferences.h"
#include "QtSLiMFindPanel.h"
#include "QtSLiMHelpWindow.h"
#include "QtSLiMEidosConsole.h"
#include "QtSLiMVariableBrowser.h"
#include "QtSLiMDebugOutputWindow.h"
#include "QtSLiMTablesDrawer.h"
#include "QtSLiMScriptTextEdit.h"
#include "QtSLiM_SLiMgui.h"

#include "QtSLiMGraphView.h"
#include "QtSLiMGraphView_1DPopulationSFS.h"
#include "QtSLiMGraphView_1DSampleSFS.h"
#include "QtSLiMGraphView_2DPopulationSFS.h"
#include "QtSLiMGraphView_2DSampleSFS.h"
#include "QtSLiMGraphView_LossTimeHistogram.h"
#include "QtSLiMGraphView_FixationTimeHistogram.h"
#include "QtSLiMGraphView_AgeDistribution.h"
#include "QtSLiMGraphView_LifetimeReproduction.h"
#include "QtSLiMGraphView_PopulationVisualization.h"
#include "QtSLiMGraphView_FitnessOverTime.h"
#include "QtSLiMGraphView_PopSizeOverTime.h"
#include "QtSLiMGraphView_FrequencyTrajectory.h"
#include "QtSLiMGraphView_PopFitnessDist.h"
#include "QtSLiMGraphView_SubpopFitnessDists.h"
#include "QtSLiMGraphView_MultispeciesPopSizeOverTime.h"
#include "QtSLiMHaplotypeManager.h"

#include <QCoreApplication>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QtDebug>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QCursor>
#include <QPalette>
#include <QFileDialog>
#include <QSettings>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QStandardPaths>
#include <QToolTip>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QFileSystemWatcher>
#include <QClipboard>
#include <QProcess>
#include <QDesktopServices>
#include <QScreen>
#include <QMetaMethod>
#include <QLabel>

#include <unistd.h>
#include <sys/stat.h>

#include <utility>
#include <map>
#include <string>
#include <algorithm>
#include <vector>

#include "individual.h"
#include "eidos_test.h"
#include "slim_test.h"
#include "log_file.h"

#ifdef _OPENMP
#error Building SLiMgui to run in parallel is not currently supported.
#endif


// This allows us to use Qt::QueuedConnection with EidosErrorContext
Q_DECLARE_METATYPE(EidosErrorContext)
static int EidosErrorContext_metatype_id = qRegisterMetaType<EidosErrorContext>();


static std::string defaultWFScriptString(void)
{
    return std::string(
                "// set up a simple neutral simulation\n"
                "initialize() {\n"
                "	initializeMutationRate(1e-7);\n"
                "	\n"
                "	// m1 mutation type: neutral\n"
                "	initializeMutationType(\"m1\", 0.5, \"f\", 0.0);\n"
                "	\n"
                "	// g1 genomic element type: uses m1 for all mutations\n"
                "	initializeGenomicElementType(\"g1\", m1, 1.0);\n"
                "	\n"
                "	// uniform chromosome of length 100 kb with uniform recombination\n"
                "	initializeGenomicElement(g1, 0, 99999);\n"
                "	initializeRecombinationRate(1e-8);\n"
                "}\n"
                "\n"
                "// create a population of 500 individuals\n"
                "1 early() {\n"
                "	sim.addSubpop(\"p1\", 500);\n"
                "}\n"
                "\n"
                "// output samples of 10 genomes periodically, all fixed mutations at end\n"
                "1000 late() { p1.outputSample(10); }\n"
                "2000 late() { p1.outputSample(10); }\n"
                "2000 late() { sim.outputFixedMutations(); }\n");
}

static std::string defaultWFScriptString_NC(void)
{
    return std::string(
        "initialize() {\n"
        "	initializeMutationRate(1e-7);\n"
        "	initializeMutationType(\"m1\", 0.5, \"f\", 0.0);\n"
        "	initializeGenomicElementType(\"g1\", m1, 1.0);\n"
        "	initializeGenomicElement(g1, 0, 99999);\n"
        "	initializeRecombinationRate(1e-8);\n"
        "}\n"
        "\n"
        "1 early() {\n"
        "	sim.addSubpop(\"p1\", 500);\n"
        "}\n"
        "\n"
        "2000 late() { sim.outputFixedMutations(); }\n");
}

static std::string defaultNonWFScriptString(void)
{
    return std::string(
                "// set up a simple neutral nonWF simulation\n"
                "initialize() {\n"
                "	initializeSLiMModelType(\"nonWF\");\n"
                "	defineConstant(\"K\", 500);	// carrying capacity\n"
                "	\n"
                "	// neutral mutations, which are allowed to fix\n"
                "	initializeMutationType(\"m1\", 0.5, \"f\", 0.0);\n"
                "	m1.convertToSubstitution = T;\n"
                "	\n"
                "	initializeGenomicElementType(\"g1\", m1, 1.0);\n"
                "	initializeGenomicElement(g1, 0, 99999);\n"
                "	initializeMutationRate(1e-7);\n"
                "	initializeRecombinationRate(1e-8);\n"
                "}\n"
                "\n"
                "// each individual reproduces itself once\n"
                "reproduction() {\n"
                "	subpop.addCrossed(individual, subpop.sampleIndividuals(1));\n"
                "}\n"
                "\n"
                "// create an initial population of 10 individuals\n"
                "1 early() {\n"
                "	sim.addSubpop(\"p1\", 10);\n"
                "}\n"
                "\n"
                "// provide density-dependent selection\n"
                "early() {\n"
                "	p1.fitnessScaling = K / p1.individualCount;\n"
                "}\n"
                "\n"
                "// output all fixed mutations at end\n"
                "2000 late() { sim.outputFixedMutations(); }\n");
}

static std::string defaultNonWFScriptString_NC(void)
{
    return std::string(
        "initialize() {\n"
        "	initializeSLiMModelType(\"nonWF\");\n"
        "	defineConstant(\"K\", 500);\n"
        "	\n"
        "	initializeMutationType(\"m1\", 0.5, \"f\", 0.0);\n"
        "	m1.convertToSubstitution = T;\n"
        "	\n"
        "	initializeGenomicElementType(\"g1\", m1, 1.0);\n"
        "	initializeGenomicElement(g1, 0, 99999);\n"
        "	initializeMutationRate(1e-7);\n"
        "	initializeRecombinationRate(1e-8);\n"
        "}\n"
        "\n"
        "reproduction() {\n"
        "	subpop.addCrossed(individual, subpop.sampleIndividuals(1));\n"
        "}\n"
        "\n"
        "1 early() {\n"
        "	sim.addSubpop(\"p1\", 10);\n"
        "}\n"
        "\n"
        "early() {\n"
        "	p1.fitnessScaling = K / p1.individualCount;\n"
        "}\n"
        "\n"
        "2000 late() { sim.outputFixedMutations(); }\n");
}


QtSLiMWindow::QtSLiMWindow(QtSLiMWindow::ModelType modelType, bool includeComments) : QMainWindow(nullptr), ui(new Ui::QtSLiMWindow)
{
    init();
    setCurrentFile(QString());
    
    // set up the initial script
    std::string untitledScriptString;
    if (includeComments)
        untitledScriptString = (modelType == QtSLiMWindow::ModelType::WF) ? defaultWFScriptString() : defaultNonWFScriptString();
    else
        untitledScriptString = (modelType == QtSLiMWindow::ModelType::WF) ? defaultWFScriptString_NC() : defaultNonWFScriptString_NC();
    
    lastSavedString = QString::fromStdString(untitledScriptString);
    scriptChangeObserved = false;
    
    ui->scriptTextEdit->setPlainText(lastSavedString);
    
    if (consoleController)
        consoleController->invalidateSymbolTableAndFunctionMap();
    
    setScriptStringAndInitializeSimulation(untitledScriptString);
    
    if (consoleController)
        consoleController->validateSymbolTableAndFunctionMap();
    
    // Update all our UI to reflect the current state of the simulation
    updateAfterTickFull(true);
    resetSLiMChangeCount();     // no recycle change count; the current model is correct
    setWindowModified(false);    // untitled windows consider themselves unmodified
}

QtSLiMWindow::QtSLiMWindow(const QString &fileName) : QMainWindow(nullptr), ui(new Ui::QtSLiMWindow)
{
    init();
    loadFile(fileName);
}

QtSLiMWindow::QtSLiMWindow(const QString &recipeName, const QString &recipeScript) : QMainWindow(nullptr), ui(new Ui::QtSLiMWindow)
{
    init();
    setCurrentFile(QString());
    setWindowFilePath(recipeName);
    isRecipe = true;
    isTransient = false;
    
    // set up the initial script
    lastSavedString = recipeScript;
    scriptChangeObserved = false;
    
    ui->scriptTextEdit->setPlainText(recipeScript);
    setScriptStringAndInitializeSimulation(recipeScript.toUtf8().constData());
    
    // Update all our UI to reflect the current state of the simulation
    updateAfterTickFull(true);
    resetSLiMChangeCount();     // no recycle change count; the current model is correct
    setWindowModified(false);    // untitled windows consider themselves unmodified
}

void QtSLiMWindow::init(void)
{
    // On macOS, we turn off the automatic quit on last window close, for Qt 5.15.2.
    // However, Qt's treatment of the menu bar seems to be a bit buggy unless a main window exists.
    // That main window can be hidden; it just needs to exist.  So here we just allow our main
    // window(s) to leak,so that Qt is happy.  This sucks, obviously, but really it seems unlikely
    // to matter.  The window will notice its zombified state when it is closed, and will free
    // resources and mark itself as a zombie so it doesn't get included in the Window menu, etc.
    // Builds against older Qt versions will just quit on the last window close, because
    // QTBUG-86874 and QTBUG-86875 prevent this from working.
#ifdef __APPLE__
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 2))
    // no set of the attribute on Qt 5.15.2; we will *not* delete on close
#else
    setAttribute(Qt::WA_DeleteOnClose);
#endif
#else
    setAttribute(Qt::WA_DeleteOnClose);
#endif
    isUntitled = true;
    isRecipe = false;
    
    // create the window UI
    ui->setupUi(this);
    
    // hide the species bar initially so it doesn't interfere with the sizing done by interpolateSplitters()
    ui->speciesBarWidget->setHidden(true);
    
    ui->speciesBar->setAcceptDrops(false);
    ui->speciesBar->setDocumentMode(false);
    ui->speciesBar->setDrawBase(false);
    ui->speciesBar->setExpanding(false);
    ui->speciesBar->setMovable(false);
    ui->speciesBar->setShape(QTabBar::RoundedNorth);
    ui->speciesBar->setTabsClosable(false);
    ui->speciesBar->setUsesScrollButtons(false);
    
    connect(ui->speciesBar, &QTabBar::currentChanged, this, &QtSLiMWindow::selectedSpeciesChanged);
    
    // add splitters with the species bar hidden; this sets correct heights on things
    interpolateSplitters();
    initializeUI();
    
    // with everything built, mark ourselves as transient (recipes and files will mark this false after us)
    isTransient = true;
    
    // wire up our continuous play and tick play timers
    connect(&continuousPlayInvocationTimer_, &QTimer::timeout, this, &QtSLiMWindow::_continuousPlay);
    connect(&continuousProfileInvocationTimer_, &QTimer::timeout, this, &QtSLiMWindow::_continuousProfile);
    connect(&playOneStepInvocationTimer_, &QTimer::timeout, this, &QtSLiMWindow::_playOneStep);
    
    // wire up deferred display of script errors and termination messages
    connect(this, &QtSLiMWindow::terminationWithMessage, this, &QtSLiMWindow::showTerminationMessage, Qt::QueuedConnection);
    
    // forward option-clicks in our views to the help window
    ui->scriptTextEdit->setOptionClickEnabled(true);
    ui->outputTextEdit->setOptionClickEnabled(false);
    
    // the script textview completes, the output textview does not
    ui->scriptTextEdit->setCodeCompletionEnabled(true);
    ui->outputTextEdit->setCodeCompletionEnabled(false);
    
    // We set the working directory for new windows to ~/Desktop/, since it makes no sense for them to use the location of the app.
    // Each running simulation will track its own working directory, and the user can set it with a button in the SLiMgui window.
    // BCH 4/2/2020: Per request from PLR, we will now use the Desktop as the default directory only if we were launched by Finder
    // or equivalent; if we were launched by a shell, we will use the working directory given us by that shell.  See issue #76
    if (qtSLiMAppDelegate->launchedFromShell())
        sim_working_dir = qtSLiMAppDelegate->QtSLiMCurrentWorkingDirectory();
    else
    #ifdef _WIN32
        sim_working_dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation).toStdString();
    #else
        sim_working_dir = Eidos_ResolvedPath("~/Desktop");
    #endif
    
    // Check that our chosen working directory actually exists; if not, use ~
    struct stat buffer;
    
    if (stat(sim_working_dir.c_str(), &buffer) != 0)
    #ifdef _WIN32
        sim_working_dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation).toStdString();
    #else
        sim_working_dir = Eidos_ResolvedPath("~");
    #endif
    
    sim_requested_working_dir = sim_working_dir;	// return to the working dir on recycle unless the user overrides it
    
    // Wire up things that set the window to be modified.
    connect(ui->scriptTextEdit, &QPlainTextEdit::textChanged, this, &QtSLiMWindow::documentWasModified);
    connect(ui->scriptTextEdit, &QPlainTextEdit::textChanged, this, &QtSLiMWindow::scriptTexteditChanged);
    
    // Watch for changes to the selection in the population tableview
    connect(ui->subpopTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &QtSLiMWindow::subpopSelectionDidChange);
    
    // Watch for changes to our change count, for the recycle button color
    connect(this, &QtSLiMWindow::controllerChangeCountChanged, this, [this]() { updateRecycleButtonIcon(false); });
    
    // Ensure that the tick lineedit does not have the initial keyboard focus and has no selection; hard to do!
    // BCH 5 August 2022: this code is no longer working, the tick lineedit still has initial focus, sigh; I added
    // the call to ui->scriptTextEdit->setFocus() and that seems to do it, not sure why I didn't do that before;
    // but since this seems to be fragile, I'm going to leave *both* approaches in the code here, maybe which
    // approach works depends on the Qt version or the platform or something.  Forward in all directions!
    ui->tickLineEdit->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    QTimer::singleShot(0, this, [this]() { ui->tickLineEdit->setFocusPolicy(Qt::FocusPolicy::StrongFocus); });
    ui->scriptTextEdit->setFocus();
    
    // watch for a change to light mode / dark mode, to customize display of the play speed slider for example
    connect(qtSLiMAppDelegate, &QtSLiMAppDelegate::applicationPaletteChanged, this, &QtSLiMWindow::applicationPaletteChanged);
    applicationPaletteChanged();
    
    // Instantiate the help panel up front so that it responds instantly; slows down our launch, but it seems better to me...
    QtSLiMHelpWindow::instance();
    
    // Create our console window; we want one all the time, so that it keeps live symbols for code completion for us
    if (!consoleController)
    {
        consoleController = new QtSLiMEidosConsole(this);
        if (consoleController)
        {
            // wire ourselves up to monitor the console for closing, to fix our button state
            connect(consoleController, &QtSLiMEidosConsole::willClose, this, [this]() {
                ui->consoleButton->setChecked(false);
                showConsoleReleased();
            });
        }
        else
        {
            qDebug() << "Could not create console controller";
        }
    }
    
    // Create our debug output window; we want one all the time, so we can log to it
    debugOutputWindow_ = new QtSLiMDebugOutputWindow(this);
    
    connect(&debugButtonFlashTimer_, &QTimer::timeout, this, &QtSLiMWindow::handleDebugButtonFlash);
    
    // We need to update our button/menu enable state whenever the focus or the active window changes
    connect(qApp, &QApplication::focusChanged, this, &QtSLiMWindow::updateUIEnabling);
    connect(qtSLiMAppDelegate, &QtSLiMAppDelegate::activeWindowListChanged, this, &QtSLiMWindow::updateUIEnabling);
    
    // We also do it specifically when the Edit menu is about to show, to correctly validate undo/redo in all cases
    // Note that it is not simple to do this revalidation when a keyboard shortcut is pressed, but happily (?), Qt
    // ignores the action validation state in that case anyway; undo/redo is delivered even if the action is disabled
    connect(ui->menuEdit, &QMenu::aboutToShow, this, &QtSLiMWindow::updateUIEnabling);
    
    // And also when about to show the Script menu, because the Show/Hide menu items might not be accurately named
    connect(ui->menuScript, &QMenu::aboutToShow, this, &QtSLiMWindow::updateUIEnabling);
    
    // The app delegate wants to know our play state so it can change the app icon
    connect(this, &QtSLiMWindow::playStateChanged, qtSLiMAppDelegate, &QtSLiMAppDelegate::playStateChanged);
    
    // Set the window icon, overriding the app icon
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    setWindowIcon(qtSLiMAppDelegate->slimDocumentIcon());
#endif
    
    // Run self-tests if modifiers are down, if we are the first window opened
    // Note that this alters the state of the app: mutation ids have been used, the RNG has been used,
    // lots of objects have been leaked due to raises, etc.  So this should be hidden/optional/undocumented.
    static bool beenHere = false;
    
    if (!beenHere)
    {
        bool optionPressed = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier);
        bool shiftPressed = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier);
        
        if (optionPressed && shiftPressed)
        {
            willExecuteScript();
            
            std::cerr << "Running Eidos self-test..." << std::endl;
            RunEidosTests();
            std::cerr << std::endl << std::endl;
            std::cerr << "Running SLiM self-test..." << std::endl;
            RunSLiMTests();
            
            didExecuteScript();
        }
        
        beenHere = true;
    }
}

void QtSLiMWindow::interpolateVerticalSplitter(void)
{
    const int splitterMargin = 8;
    QLayout *parentLayout = ui->centralWidget->layout();
    QVBoxLayout *firstSubLayout = ui->overallTopLayout;
    QHBoxLayout *secondSubLayout = ui->overallBottomLayout;
    
    // force geometry calculation, which is lazy
    setAttribute(Qt::WA_DontShowOnScreen, true);
    show();
    hide();
    setAttribute(Qt::WA_DontShowOnScreen, false);
    
    // get the geometry we need
    QSize firstSubSize = firstSubLayout->sizeHint();
    QMargins marginsP = QMargins(8, 8, 8, 8); //parentLayout->contentsMargins();
    QMargins marginsS1 = firstSubLayout->contentsMargins();
    QMargins marginsS2 = secondSubLayout->contentsMargins();
    
    // change fixed-size views to be flexible, so they cooperate with the splitters
    firstSubLayout->setStretch(0, 1);
    ui->subpopTableView->setMaximumHeight(QWIDGETSIZE_MAX);
    ui->individualsWidget->setMaximumHeight(QWIDGETSIZE_MAX);
    ui->topRightLayout->setStretch(4, 1);
#if !defined(__APPLE__)
    ui->topRightLayout->setSpacing(3);  // a platform-dependent value that prevents a couple of pixels of "play" above the play speed slider, for reasons I don't understand
#else
    ui->topRightLayout->setSpacing(4);
#endif
    ui->playSpeedSlider->setFixedHeight(ui->playSpeedSlider->sizeHint().height());
    
    // empty out parentLayout
    QLayoutItem *child;
    while ((child = parentLayout->takeAt(0)) != nullptr);
    
    ui->topBottomDividerLine->setParent(nullptr);
    ui->topBottomDividerLine = nullptr;
    
    // make the new top-level widgets and transfer in their contents
    overallTopWidget = new QWidget(nullptr);
    overallTopWidget->setLayout(firstSubLayout);
    overallTopWidget->setMinimumHeight(firstSubSize.height() + (splitterMargin - 5));   // there is already 5 pixels of margin at the bottom of overallTopWidget due to layout details
    firstSubLayout->setContentsMargins(QMargins(marginsS1.left() + marginsP.left(), marginsS1.top() + marginsP.top(), marginsS1.right() + marginsP.right(), marginsS1.bottom() + (splitterMargin - 5)));
    
    overallBottomWidget = new QWidget(nullptr);
    overallBottomWidget->setLayout(secondSubLayout);
    secondSubLayout->setContentsMargins(QMargins(marginsS2.left() + marginsP.left(), marginsS2.top() + splitterMargin, marginsS2.right() + marginsP.right(), marginsS2.bottom() + marginsP.bottom()));
    
    // make the QSplitter between the top and bottom and add the top-level widgets to it
    overallSplitter = new QtSLiMSplitter(Qt::Vertical, this);
    
    overallSplitter->setChildrenCollapsible(true);
    overallSplitter->addWidget(overallTopWidget);
    overallSplitter->addWidget(overallBottomWidget);
    overallSplitter->setHandleWidth(std::max(9, overallSplitter->handleWidth() + 3));   // ends up 9 on Ubuntu, 10 on macOS
    overallSplitter->setStretchFactor(0, 1);
    overallSplitter->setStretchFactor(1, 100);    // initially, give all height to the bottom widget
    
    // and finally, add the splitter to the parent layout
    parentLayout->addWidget(overallSplitter);
    parentLayout->setContentsMargins(0, 0, 0, 0);
}

void QtSLiMWindow::interpolateHorizontalSplitter(void)
{
    const int splitterMargin = 8;
    QLayout *parentLayout = overallBottomWidget->layout();
    QVBoxLayout *firstSubLayout = ui->scriptLayout;
    QVBoxLayout *secondSubLayout = ui->outputLayout;
    
    // force geometry calculation, which is lazy
    setAttribute(Qt::WA_DontShowOnScreen, true);
    show();
    hide();
    setAttribute(Qt::WA_DontShowOnScreen, false);
    
    // get the geometry we need
    QMargins marginsP = parentLayout->contentsMargins();
    QMargins marginsS1 = firstSubLayout->contentsMargins();
    QMargins marginsS2 = secondSubLayout->contentsMargins();
    
    // empty out parentLayout
    QLayoutItem *child;
    while ((child = parentLayout->takeAt(0)) != nullptr);
    
    // make the new top-level widgets and transfer in their contents
    scriptWidget = new QWidget(nullptr);
    scriptWidget->setLayout(firstSubLayout);
    firstSubLayout->setContentsMargins(QMargins(marginsS1.left() + marginsP.left(), marginsS1.top() + marginsP.top(), marginsS1.right() + splitterMargin, marginsS1.bottom() + marginsP.bottom()));
    
    outputWidget = new QWidget(nullptr);
    outputWidget->setLayout(secondSubLayout);
    secondSubLayout->setContentsMargins(QMargins(marginsS2.left() + splitterMargin, marginsS2.top() + marginsP.top(), marginsS2.right() + marginsP.right(), marginsS2.bottom() + marginsP.bottom()));
    
    // make the QSplitter between the left and right and add the subsidiary widgets to it
    bottomSplitter = new QtSLiMSplitter(Qt::Horizontal, this);
    
    bottomSplitter->setChildrenCollapsible(true);
    bottomSplitter->addWidget(scriptWidget);
    bottomSplitter->addWidget(outputWidget);
    bottomSplitter->setHandleWidth(std::max(9, bottomSplitter->handleWidth() + 3));   // ends up 9 on Ubuntu, 10 on macOS
    bottomSplitter->setStretchFactor(0, 2);
    bottomSplitter->setStretchFactor(1, 1);    // initially, give 2/3 of the width to the script widget
    
    // and finally, add the splitter to the parent layout
    parentLayout->addWidget(bottomSplitter);
    parentLayout->setContentsMargins(0, 0, 0, 0);
}

void QtSLiMWindow::interpolateSplitters(void)
{
#if 1
    // This case is hit if splitters are enabled; it adds a top-level vertical splitter and a subsidiary horizontal splitter
    // We do this at runtime, rather than in QtSLiMWindow.ui, to preserve the non-splitter option, and because the required
    // alterations are complex and depend upon the (platform-dependent) initial calculated sizes of the various elements
    interpolateVerticalSplitter();
    interpolateHorizontalSplitter();
#else
    // This case is hit if splitters are disabled; it does a little cleanup of elements that exist to support the splitters
    ui->topRightLayout->removeItem(ui->playControlsSpacerExpanding);
    delete ui->playControlsSpacerExpanding;
    ui->playControlsSpacerExpanding = nullptr;
#endif
}

void QtSLiMWindow::addChromosomeWidgets(QVBoxLayout *chromosomeLayout, QtSLiMChromosomeWidget *overviewWidget, QtSLiMChromosomeWidget *zoomedWidget)
{
    overviewWidget->setController(this);
	overviewWidget->setReferenceChromosomeView(nullptr);
	overviewWidget->setSelectable(true);
	
    zoomedWidget->setController(this);
	zoomedWidget->setReferenceChromosomeView(overviewWidget);
	zoomedWidget->setSelectable(false);
    
    // Forward notification of changes to the selection in the chromosome view
    connect(overviewWidget, &QtSLiMChromosomeWidget::selectedRangeChanged, this, [this]() { emit controllerChromosomeSelectionChanged(); });

    // Add these widgets to our vectors of chromosome widgets
    chromosomeWidgetLayouts.push_back(chromosomeLayout);
    chromosomeOverviewWidgets.push_back(overviewWidget);
    chromosomeZoomedWidgets.push_back(zoomedWidget);
}

void QtSLiMWindow::initializeUI(void)
{
    glueUI();
    
    // fix the layout of the window
    ui->scriptHeaderLayout->setSpacing(4);
    ui->scriptHeaderLayout->setMargin(0);
    ui->scriptHeaderLabel->setContentsMargins(8, 0, 15, 0);

    ui->outputHeaderLayout->setSpacing(4);
    ui->outputHeaderLayout->setMargin(0);
    ui->outputHeaderLabel->setContentsMargins(8, 0, 15, 0);

    ui->playControlsLayout->setSpacing(8);
    ui->playControlsLayout->setMargin(0);
    
    // substitute a custom layout subclass for playControlsLayout to lay out the profile button specially
    {
        int indexOfPlayControlsLayout = -1;
        
        // QLayout::indexOf(QLayoutItem *layoutItem) wasn't added until 5.12, oddly
        for (int i = 0; i < ui->topRightLayout->count(); ++i)
            if (ui->topRightLayout->itemAt(i) == ui->playControlsLayout)
                indexOfPlayControlsLayout = i;
        
        if (indexOfPlayControlsLayout >= 0)
        {
            QtSLiMPlayControlsLayout *newPlayControlsLayout = new QtSLiMPlayControlsLayout();
            ui->topRightLayout->insertItem(indexOfPlayControlsLayout, newPlayControlsLayout);
            newPlayControlsLayout->setParent(ui->topRightLayout);   // surprising that insertItem() doesn't do this...; but this sets our parentWidget also, correctly
            
            // Transfer over the contents of the old layout
            while (ui->playControlsLayout->count())
            {
                QLayoutItem *layoutItem = ui->playControlsLayout->takeAt(0);
                newPlayControlsLayout->addItem(layoutItem);
            }
            
            // Transfer properties of the old layout
            newPlayControlsLayout->setSpacing(ui->playControlsLayout->spacing());
            newPlayControlsLayout->setMargin(ui->playControlsLayout->margin());
            
            // Get rid of the old layout
            ui->topRightLayout->removeItem(ui->playControlsLayout);
            ui->playControlsLayout = nullptr;
            
            // Remember the new layout
            ui->playControlsLayout = newPlayControlsLayout;
        }
        else
        {
            qDebug() << "Couldn't find playControlsLayout!";
        }
    }
    
    // set the script types and syntax highlighting appropriately
    ui->scriptTextEdit->setScriptType(QtSLiMTextEdit::SLiMScriptType);
    ui->scriptTextEdit->setSyntaxHighlightType(QtSLiMTextEdit::ScriptHighlighting);
    
    ui->outputTextEdit->setScriptType(QtSLiMTextEdit::NoScriptType);
    ui->outputTextEdit->setSyntaxHighlightType(QtSLiMTextEdit::OutputHighlighting);
    
    // set button states
    ui->toggleDrawerButton->setChecked(false);
    
    // Set up the population table view
    populationTableModel_ = new QtSLiMPopulationTableModel(this);
    ui->subpopTableView->setModel(populationTableModel_);
    ui->subpopTableView->setHorizontalHeader(new QtSLiMPopulationTableHeaderView(Qt::Orientation::Horizontal, this));
    
    QHeaderView *popTableHHeader = ui->subpopTableView->horizontalHeader();
    QHeaderView *popTableVHeader = ui->subpopTableView->verticalHeader();
    
    popTableHHeader->setMinimumSectionSize(1);
    popTableVHeader->setMinimumSectionSize(1);
    
    popTableHHeader->resizeSection(0, 65);
    //popTableHHeader->resizeSection(1, 60);
    popTableHHeader->resizeSection(2, 40);
    popTableHHeader->resizeSection(3, 40);
    popTableHHeader->resizeSection(4, 40);
    popTableHHeader->resizeSection(5, 40);
    popTableHHeader->setSectionsClickable(false);
    popTableHHeader->setSectionsMovable(false);
    popTableHHeader->setSectionResizeMode(0, QHeaderView::Fixed);
    popTableHHeader->setSectionResizeMode(1, QHeaderView::Stretch);
    popTableHHeader->setSectionResizeMode(2, QHeaderView::Fixed);
    popTableHHeader->setSectionResizeMode(3, QHeaderView::Fixed);
    popTableHHeader->setSectionResizeMode(4, QHeaderView::Fixed);
    popTableHHeader->setSectionResizeMode(5, QHeaderView::Fixed);
    
    QFont headerFont = popTableHHeader->font();
    QFont cellFont = ui->subpopTableView->font();
#ifdef __linux__
    headerFont.setPointSize(8);
    cellFont.setPointSize(8);
#else
    headerFont.setPointSize(11);
    cellFont.setPointSize(11);
#endif
    popTableHHeader->setFont(headerFont);
    ui->subpopTableView->setFont(cellFont);
    
    popTableVHeader->setSectionResizeMode(QHeaderView::Fixed);
    popTableVHeader->setDefaultSectionSize(18);
    
    // Set up our built-in chromosome widgets; this should be the only place these ui outlets are used!
    addChromosomeWidgets(ui->chromosomeWidgetLayout, ui->chromosomeOverview, ui->chromosomeZoomed);
    
    // Restore the saved window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMMainWindow");
    resize(settings.value("size", QSize(950, 700)).toSize());
    move(settings.value("pos", QPoint(100, 100)).toPoint());
    settings.endGroup();
    
    // Ask the app delegate to handle the recipes menu for us
    qtSLiMAppDelegate->setUpRecipesMenu(ui->menuOpenRecipe, ui->actionFindRecipe);
    
    // Likewise for the recent documents menu
    QMenu *recentMenu = new QMenu("Open Recent", this);
    ui->actionOpenRecent->setMenu(recentMenu);
    
    qtSLiMAppDelegate->setUpRecentsMenu(recentMenu);
    
    // Set up the Window menu, which updates on demand
    connect(ui->menuWindow, &QMenu::aboutToShow, this, &QtSLiMWindow::updateWindowMenu);
}

void QtSLiMWindow::applicationPaletteChanged(void)
{
    bool inDarkMode = QtSLiMInDarkMode();
    
    // Custom colors for the play slider; note that this completely overrides the style sheet in the .ui file!
    if (inDarkMode)
    {
        ui->playSpeedSlider->setStyleSheet(
                    R"V0G0N(
                    QSlider::groove:horizontal {
                        border: 1px solid #606060;
                        border-radius: 1px;
                        height: 2px; /* the groove expands to the size of the slider by default. by giving it a height, it has a fixed size */
                        background: #808080;
                        margin: 2px 0;
                    }
                    QSlider::groove:horizontal:disabled {
                        border: 1px solid #505050;
                        border-radius: 1px;
                        height: 2px; /* the groove expands to the size of the slider by default. by giving it a height, it has a fixed size */
                        background: #606060;
                        margin: 2px 0;
                    }
                    
                    QSlider::handle:horizontal {
                        background: #f0f0f0;
                        border: 1px solid #b0b0b0;
                        width: 8px;
                        margin: -4px 0;
                        border-radius: 4px;
                    }
                    QSlider::handle:horizontal:disabled {
                        background: #606060;
                        border: 1px solid #505050;
                        width: 8px;
                        margin: -4px 0;
                        border-radius: 4px;
                    })V0G0N");
    }
    else
    {
        ui->playSpeedSlider->setStyleSheet(
                    R"V0G0N(
                    QSlider::groove:horizontal {
                        border: 1px solid #888888;
                        border-radius: 1px;
                        height: 2px; /* the groove expands to the size of the slider by default. by giving it a height, it has a fixed size */
                        background: #a0a0a0;
                        margin: 2px 0;
                    }
                    QSlider::groove:horizontal:disabled {
                        border: 1px solid #cccccc;
                        border-radius: 1px;
                        height: 2px; /* the groove expands to the size of the slider by default. by giving it a height, it has a fixed size */
                        background: #e0e0e0;
                        margin: 2px 0;
                    }
                    
                    QSlider::handle:horizontal {
                        background: #ffffff;
                        border: 1px solid #909090;
                        width: 8px;
                        margin: -4px 0;
                        border-radius: 4px;
                    }
                    QSlider::handle:horizontal:disabled {
                        background: #ffffff;
                        border: 1px solid #d0d0d0;
                        width: 8px;
                        margin: -4px 0;
                        border-radius: 4px;
                    })V0G0N");
    }
}

void QtSLiMWindow::displayStartupMessage(void)
{
    // Set the initial status bar message; called by QtSLiMAppDelegate::appDidFinishLaunching()
    bool inDarkMode = QtSLiMInDarkMode();
    QString message(inDarkMode ? "<font color='#AAAAAA' style='font-size: 11px;'>SLiM %1, %2 build.</font>"
                               : "<font color='#555555' style='font-size: 11px;'>SLiM %1, %2 build.</font>");
    
    ui->statusBar->showMessage(message.arg(QString(SLIM_VERSION_STRING)).arg(
#if DEBUG
                                   "debug"
#else
                                   "release"
#endif
                                   ));    
}

QtSLiMScriptTextEdit *QtSLiMWindow::scriptTextEdit(void)
{
    return ui->scriptTextEdit;
}

QtSLiMTextEdit *QtSLiMWindow::outputTextEdit(void)
{
    return ui->outputTextEdit;
}

QtSLiMWindow::~QtSLiMWindow()
{
    // Do this first, in case it uses any ivars that will be freed
    setInvalidSimulation(true);
    
    // Disconnect our connections having to do with focus changes, since they can fire
    // during our destruction while we are in an invalid state
    disconnect(qApp, QMetaMethod(), this, QMetaMethod());
    disconnect(qtSLiMAppDelegate, QMetaMethod(), this, QMetaMethod());
    
    // Then tear down the UI
    delete ui;

    // Disconnect delegate relationships
    if (consoleController)
        consoleController->parentSLiMWindow = nullptr;
    
    // Free resources
    if (community)
    {
        delete community;
        community = nullptr;
        focalSpecies = nullptr;
    }
    if (slimgui)
	{
		delete slimgui;
		slimgui = nullptr;
	}
	
	if (sim_RNG_initialized)
	{
    	_Eidos_FreeOneRNG(sim_RNG);
    	sim_RNG_initialized = false;
	}
	
    // The console is owned by us, and it owns the variable browser.  Since the parent
    // relationships are set up, they should be released by Qt automatically.
    if (consoleController)
    {
        //if (consoleController->browserController)
        //  consoleController->browserController->hide();
        consoleController->hide();
    }
}

void QtSLiMWindow::invalidateUI(void)
{
    // This is called only on macOS, when a window closes.  We can't be deleted, because
    // that screws up the global menu bar.  Instead, we need to go into a zombie state,
    // by freeing up our graph windows, console, etc., but remain allocated (but hidden).
    // The main goal is erasing all traces of us in the user interface; freeing the
    // maximal amount of memory is less of a concern, since we're not talking about
    // that much memory anyway.
    
    // First set a flag indicating that we're going into zombie mode
    isZombieWindow_ = true;
    
    // Set some other state to prevent ourselves from being reused in any way
    isUntitled = false;
    isTransient = false;
    currentFile = "ZOMBIE ZOMBIE ZOMBIE ZOMBIE ZOMBIE";
    
    // Stop all timers, so we don't try to play in the background
    continuousPlayElapsedTimer_.invalidate();
    continuousPlayInvocationTimer_.stop();
    continuousProfileInvocationTimer_.stop();
    playOneStepInvocationTimer_.stop();
    
    continuousPlayOn_ = false;
    profilePlayOn_ = false;
    nonProfilePlayOn_ = false;
    tickPlayOn_ = false;
    
    // Recycle to throw away any bulky simulation state; set the default script first to avoid errors
    // Note that this creates a species named "sim" even if the window being closed was multispecies!
    ui->scriptTextEdit->setPlainText(QString::fromStdString(defaultWFScriptString()));
    recycleClicked();
    
    // Close the variable browser and Eidos console
    if (consoleController)
    {
        QtSLiMVariableBrowser *browser = consoleController->variableBrowser();
        
        if (browser)
            browser->close();
        
        consoleController->close();
    }
    
    // Close the tables drawer
    if (tablesDrawerController)
        tablesDrawerController->close();
    
    // Close all other subsidiary windows
    const QObjectList &child_objects = children();
    
    for (QObject *child_object : child_objects)
    {
        QWidget *child_widget = qobject_cast<QWidget *>(child_object);
        
        if (child_widget && child_widget->isVisible() && (child_widget->windowFlags() & Qt::Window))
            child_widget->close();
    }
}

const QColor &QtSLiMWindow::blackContrastingColorForIndex(int index)
{
    static std::vector<QColor> colorArray;
	
	if (colorArray.size() == 0)
	{
        colorArray.emplace_back(QtSLiMColorWithHSV(0.65, 0.65, 1.00, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.55, 1.00, 1.00, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.40, 1.00, 0.90, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.16, 1.00, 1.00, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.08, 0.65, 1.00, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.00, 0.65, 1.00, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.80, 0.65, 1.00, 1.0));
        colorArray.emplace_back(QtSLiMColorWithHSV(0.00, 0.00, 0.80, 1.0));
	}
	
    return ((index >= 0) && (index <= 6)) ? colorArray[static_cast<size_t>(index)] : colorArray[7];
}

const QColor &QtSLiMWindow::whiteContrastingColorForIndex(int index)
{
    static std::vector<QColor> colorArray;
    
    if (colorArray.size() == 0)
    {
        colorArray.emplace_back(QtSLiMColorWithHSV(0.65, 0.75, 1.00, 1.0));
		colorArray.emplace_back(QtSLiMColorWithHSV(0.55, 1.00, 1.00, 1.0));
		colorArray.emplace_back(QtSLiMColorWithHSV(0.40, 1.00, 0.80, 1.0));
		colorArray.emplace_back(QtSLiMColorWithHSV(0.08, 0.75, 1.00, 1.0));
		colorArray.emplace_back(QtSLiMColorWithHSV(0.00, 0.85, 1.00, 1.0));
		colorArray.emplace_back(QtSLiMColorWithHSV(0.80, 0.85, 1.00, 1.0));
		colorArray.emplace_back(QtSLiMColorWithHSV(0.00, 0.00, 0.50, 1.0));
    }
    
    return ((index >= 0) && (index <= 5)) ? colorArray[static_cast<size_t>(index)] : colorArray[6];
}

void QtSLiMWindow::colorForGenomicElementType(GenomicElementType *elementType, slim_objectid_t elementTypeID, float *p_red, float *p_green, float *p_blue, float *p_alpha)
{
	if (elementType && !elementType->color_.empty())
	{
        *p_red = elementType->color_red_;
        *p_green = elementType->color_green_;
        *p_blue = elementType->color_blue_;
        *p_alpha = 1.0f;
	}
	else
	{
        auto elementColorIter = genomicElementColorRegistry.find(elementTypeID);
		const QColor *elementColor = nullptr;
        
		if (elementColorIter == genomicElementColorRegistry.end())
		{
			elementColor = &QtSLiMWindow::blackContrastingColorForIndex(static_cast<int>(genomicElementColorRegistry.size()));
            
            genomicElementColorRegistry.emplace(elementTypeID, *elementColor);
		}
        else
        {
            elementColor = &elementColorIter->second;
        }
		
        *p_red = static_cast<float>(elementColor->redF());
        *p_green = static_cast<float>(elementColor->greenF());
        *p_blue = static_cast<float>(elementColor->blueF());
        *p_alpha = static_cast<float>(elementColor->alphaF());
	}
}

QColor QtSLiMWindow::qcolorForSpecies(Species *species)
{
    if (species->color_.length() > 0)
        return QtSLiMColorWithRGB(species->color_red_, species->color_green_, species->color_blue_, 1.0);
    
    return whiteContrastingColorForIndex(species->species_id_);
}

void QtSLiMWindow::colorForSpecies(Species *species, float *p_red, float *p_green, float *p_blue, float *p_alpha)
{
    if (species->color_.length() > 0)
    {
        *p_red = species->color_red_;
        *p_green = species->color_green_;
        *p_blue = species->color_blue_;
        *p_alpha = 1.0;
        return;
    }
    
    const QColor &speciesColor = whiteContrastingColorForIndex(species->species_id_);
    
    *p_red = static_cast<float>(speciesColor.redF());
    *p_green = static_cast<float>(speciesColor.greenF());
    *p_blue = static_cast<float>(speciesColor.blueF());
    *p_alpha = static_cast<float>(speciesColor.alphaF());
}


//
//  Document support
//

void QtSLiMWindow::closeEvent(QCloseEvent *p_event)
{
    if (maybeSave())
    {
        // We used to save the window size/position here, but now that is done in moveEvent() / resizeEvent()
        p_event->accept();
        
        // On macOS, we turn off the automatic quit on last window close, for Qt 5.15.2.
        // In that case, we no longer get freed when we close, because we need to stick around
        // to make the global menubar work; see QtSLiMWindow::init().  So when we're closing,
        // we now free up the resources we hold and mark ourselves as a zombie window.
        // Builds against older Qt versions will just quit on the last window close, because
        // QTBUG-86874 and QTBUG-86875 prevent this from working.
#ifdef __APPLE__
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 2))
        invalidateUI();
#endif
#endif
    }
    else
    {
        p_event->ignore();
        qtSLiMAppDelegate->closeRejected();
    }
}

void QtSLiMWindow::moveEvent(QMoveEvent *p_event)
{
    if (donePositioning_)
    {
        // Save the window position; see https://doc.qt.io/qt-5/qsettings.html#details
        QSettings settings;
        
        settings.beginGroup("QtSLiMMainWindow");
        settings.setValue("size", size());
        settings.setValue("pos", pos());
        settings.endGroup();
        
        //qDebug() << "moveEvent() after done positioning";
    }
    
    QWidget::moveEvent(p_event);
}

void QtSLiMWindow::resizeEvent(QResizeEvent *p_event)
{
    if (donePositioning_)
    {
        // Save the window position; see https://doc.qt.io/qt-5/qsettings.html#details
        QSettings settings;
        
        settings.beginGroup("QtSLiMMainWindow");
        settings.setValue("size", size());
        settings.setValue("pos", pos());
        settings.endGroup();
        
        //qDebug() << "resizeEvent() after done positioning";
    }
    
    QWidget::resizeEvent(p_event);
}

void QtSLiMWindow::showEvent(QShowEvent *p_event)
{
    QWidget::showEvent(p_event);
    
    if (!testAttribute(Qt::WA_DontShowOnScreen))
    {
        //qDebug() << "showEvent() : done positioning";
        donePositioning_ = true;
    }
}

bool QtSLiMWindow::isScriptModified(void)
{
    // We used to use Qt's isWindowModified() change-tracking system.  Unfortunately, apparently that is broken on Debian;
    // see https://github.com/MesserLab/SLiM/issues/370.  It looks like Qt internally calls textChanged() and modifies the
    // document when it shouldn't, resulting in untitled documents being marked dirty.  So now we check whether the
    // script string has been changed from was was last saved to disk, or from their initial state if they are not
    // based on a disk file.  Once a change has been observed, the document stays dirty; it doesn't revert to clean if
    // the script string goes back to its original state (although smart, that would be non-standard).  BCH 10/24/2023
    if (scriptChangeObserved)
        return true;
    
    QString currentScript = ui->scriptTextEdit->toPlainText();
    
    if (lastSavedString != currentScript)
    {
        scriptChangeObserved = true;    // sticky until saved
        return true;
    }
    
    return false;
}

bool QtSLiMWindow::windowIsReuseable(void)
{
    return (isUntitled && !isRecipe && isTransient && (slimChangeCount == 0) && !isScriptModified());
}

bool QtSLiMWindow::save()
{
    return isUntitled ? saveAs() : saveFile(currentFile);
}

bool QtSLiMWindow::saveAs()
{
    QString fileName;
    
    if (isUntitled)
    {
        QSettings settings;
        QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        QString directory = settings.value("QtSLiMDefaultSaveDirectory", QVariant(desktopPath)).toString();
        QFileInfo fileInfo(QDir(directory), "Untitled.slim");
        QString path = fileInfo.absoluteFilePath();
        
        fileName = QFileDialog::getSaveFileName(this, "Save As", path);
        
        if (!fileName.isEmpty())
            settings.setValue("QtSLiMDefaultSaveDirectory", QVariant(QFileInfo(fileName).path()));
    }
    else
    {
        // propose saving to the existing filename in the existing directory
        fileName = QFileDialog::getSaveFileName(this, "Save As", currentFile);
    }
    
    if (fileName.isEmpty())
        return false;

    return saveFile(fileName);
}

void QtSLiMWindow::revert()
{
    if (isUntitled)
    {
        qApp->beep();
    }
    else
    {
        const QMessageBox::StandardButton ret = QMessageBox::warning(this, "SLiMgui", "Are you sure you want to revert?  All changes will be lost.", QMessageBox::Yes | QMessageBox::Cancel);
        
        switch (ret) {
        case QMessageBox::Yes:
            loadFile(currentFile);
            break;
        case QMessageBox::Cancel:
            break;
        default:
            break;
        }
    }
}

bool QtSLiMWindow::maybeSave()
{
    // the recycle button change state is irrelevant; the document change state is what matters
    if (!isScriptModified())
        return true;
    
    const QMessageBox::StandardButton ret = QMessageBox::warning(this, "SLiMgui", "The document has been modified.\nDo you want to save your changes?", QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    
    switch (ret) {
    case QMessageBox::Save:
        return save();
    case QMessageBox::Cancel:
        return false;
    default:
        break;
    }
    return true;
}

void QtSLiMWindow::loadFile(const QString &fileName)
{
    QFile file(fileName);
    
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, "SLiMgui", QString("Cannot read file %1:\n%2.").arg(QDir::toNativeSeparators(fileName), file.errorString()));
        return;
    }
    
    QTextStream in(&file);
    QString contents = in.readAll();
    
    lastSavedString = contents;
    scriptChangeObserved = false;
    
    ui->scriptTextEdit->setPlainText(contents);
    
    if (consoleController)
        consoleController->invalidateSymbolTableAndFunctionMap();
    
    clearOutputClicked();
    setScriptStringAndInitializeSimulation(contents.toUtf8().constData());
    
    if (consoleController)
        consoleController->validateSymbolTableAndFunctionMap();
    
    setCurrentFile(fileName);
    
    // Update all our UI to reflect the current state of the simulation
    updateAfterTickFull(true);
    resetSLiMChangeCount();     // no recycle change count; the current model is correct
    setWindowModified(false);   // loaded windows start unmodified
}

void QtSLiMWindow::loadRecipe(const QString &recipeName, const QString &recipeScript)
{
    if (consoleController)
        consoleController->invalidateSymbolTableAndFunctionMap();
    
    clearOutputClicked();
    
    lastSavedString = recipeScript;
    scriptChangeObserved = false;
    
    ui->scriptTextEdit->setPlainText(recipeScript);
    setScriptStringAndInitializeSimulation(recipeScript.toUtf8().constData());
    
    if (consoleController)
        consoleController->validateSymbolTableAndFunctionMap();
    
    setWindowFilePath(recipeName);
    isRecipe = true;
    isTransient = false;
    
    // Update all our UI to reflect the current state of the simulation
    updateAfterTickFull(true);
    resetSLiMChangeCount();     // no recycle change count; the current model is correct
    setWindowModified(false);   // loaded windows start unmodified
}

bool QtSLiMWindow::saveFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, "SLiMgui", QString("Cannot write file %1:\n%2.").arg(QDir::toNativeSeparators(fileName), file.errorString()));
        return false;
    }
    
    lastSavedString = ui->scriptTextEdit->toPlainText();
    scriptChangeObserved = false;

    QTextStream out(&file);
    out << lastSavedString;

    setCurrentFile(fileName);
    return true;
}

void QtSLiMWindow::setCurrentFile(const QString &fileName)
{
    static int sequenceNumber = 1;

    isUntitled = fileName.isEmpty();
    
    if (isUntitled) {
        if (sequenceNumber == 1)
            currentFile = QString("Untitled");
        else
            currentFile = QString("Untitled %1").arg(sequenceNumber);
        sequenceNumber++;
    } else {
        currentFile = QFileInfo(fileName).canonicalFilePath();
    }

    ui->scriptTextEdit->document()->setModified(false);
    setWindowModified(false);
    if (!isUntitled)
        isTransient = false;
    
    if (!isUntitled)
        qtSLiMAppDelegate->prependToRecentFiles(currentFile);
    
    setWindowFilePath(currentFile);
}

void QtSLiMWindow::documentWasModified()
{
    // This method should be called whenever anything happens that makes us want to mark a window as "dirty" – confirm before closing.
    // This is not quite the same as scriptTexteditChanged(), which is called whenever anything happens that makes the recycle
    // button go green; recycling resets the recycle button to gray, whereas saving resets the document state to unmodified.
    // We could be called for things that are saveable but do not trigger a need for recycling.
    
    // Things are a little more complicated now, because of a Qt bug on Debian that calls us even though the document has not,
    // in fact, been modified.  So we now determine the window modified state by comparing the script string to the last
    // saved / original script string.  See isScriptModified().  BCH 10/24/2023
    
    //setWindowModified(true);              // the old way that produces buggy behavior
    setWindowModified(isScriptModified());  // the new way that checks whether the script has actually changed
}

void QtSLiMWindow::tile(const QMainWindow *previous)
{
    if (!previous)
        return;
    int topFrameWidth = previous->geometry().top() - previous->pos().y();
    if (!topFrameWidth)
        topFrameWidth = 40;
    const QPoint position = previous->pos() + 2 * QPoint(topFrameWidth, topFrameWidth);
    if (QApplication::desktop()->availableGeometry(this).contains(rect().bottomRight() + position))
        move(position);
}


//
//  Simulation state
//

std::vector<Subpopulation *> QtSLiMWindow::listedSubpopulations(void)
{
    // This funnel method provides the vector of subpopulations that we are displaying in the population table
    // It handles the multispecies case and the "all" species tab for us
    std::vector<Subpopulation *> listedSubpops;
    Species *displaySpecies = focalDisplaySpecies();
    
    if (displaySpecies)
    {
        // If we have a displaySpecies, we just show all of the subpopulations in the species
        for (auto &iter : displaySpecies->population_.subpops_)
            listedSubpops.push_back(iter.second);
    }
    else if (!invalidSimulation() && community && community->simulation_valid_)
    {
        // If we don't, then we show all subpopulations of all species; this is the "all" tab
        for (Species *species : community->AllSpecies())
            for (auto &iter : species->population_.subpops_)
                listedSubpops.push_back(iter.second);
        
        // Sort by id, not by species
        std::sort(listedSubpops.begin(), listedSubpops.end(), [](Subpopulation *l, Subpopulation *r) { return l->subpopulation_id_ < r->subpopulation_id_; });
    }
    
    return listedSubpops;     // note these are sorted by id, not by species, unlike selectedSubpopulations()
}

std::vector<Subpopulation*> QtSLiMWindow::selectedSubpopulations(void)
{
    Species *displaySpecies = focalDisplaySpecies();
    std::vector<Subpopulation*> selectedSubpops;
	
    if (community && community->simulation_valid_)
    {
        for (Species *species : community->all_species_)
        {
            if (!displaySpecies || (displaySpecies == species))
            {
                Population &population = species->population_;
                
                for (auto popIter : population.subpops_)
                {
                    Subpopulation *subpop = popIter.second;
                    
                    if (subpop->gui_selected_)
                        selectedSubpops.emplace_back(subpop);
                }
            }
        }
    }
    
	return selectedSubpops;     // note these are sorted by species, not by id, unlike listedSubpopulations()
}

void QtSLiMWindow::chromosomeSelection(Species *species, bool *p_hasSelection, slim_position_t *p_selectionFirstBase, slim_position_t *p_selectionLastBase)
{
    // First we need to look up the chromosome view for the requested species
    for (QtSLiMChromosomeWidget *chromosomeWidget : chromosomeOverviewWidgets)
    {
        Species *widgetSpecies = chromosomeWidget->focalDisplaySpecies();
        
        if (widgetSpecies == species)
        {
            if (p_hasSelection)
                *p_hasSelection = chromosomeWidget->hasSelection();
            
            QtSLiMRange selRange = chromosomeWidget->getSelectedRange(species);
            
            if (p_selectionFirstBase)
                *p_selectionFirstBase = selRange.location;
            if (p_selectionLastBase)
                *p_selectionLastBase = selRange.location + selRange.length - 1;
            
            return;
        }
    }
    
    // We drop through to here if the species can't be found, which should not happen
    if (p_hasSelection)
        *p_hasSelection = false;
    if (p_selectionFirstBase)
        *p_selectionFirstBase = 0;
    if (p_selectionLastBase)
        *p_selectionLastBase = species->TheChromosome().last_position_;
}

const std::vector<slim_objectid_t> &QtSLiMWindow::chromosomeDisplayMuttypes(void)
{
    return chromosome_display_muttypes_;
}

void QtSLiMWindow::setInvalidSimulation(bool p_invalid)
{
    if (invalidSimulation_ != p_invalid)
    {
        invalidSimulation_ = p_invalid;
        updateUIEnabling();
    }
}

void QtSLiMWindow::setReachedSimulationEnd(bool p_reachedEnd)
{
    if (reachedSimulationEnd_ != p_reachedEnd)
    {
        reachedSimulationEnd_ = p_reachedEnd;
        updateUIEnabling();
    }
}

void QtSLiMWindow::setContinuousPlayOn(bool p_flag)
{
    if (continuousPlayOn_ != p_flag)
    {
        continuousPlayOn_ = p_flag;
        updateUIEnabling();
        emit playStateChanged();
    }
}

void QtSLiMWindow::setTickPlayOn(bool p_flag)
{
    if (tickPlayOn_ != p_flag)
    {
        tickPlayOn_ = p_flag;
        updateUIEnabling();
    }
}

void QtSLiMWindow::setProfilePlayOn(bool p_flag)
{
    if (profilePlayOn_ != p_flag)
    {
        profilePlayOn_ = p_flag;
        updateUIEnabling();
    }
}

void QtSLiMWindow::setNonProfilePlayOn(bool p_flag)
{
    if (nonProfilePlayOn_ != p_flag)
    {
        nonProfilePlayOn_ = p_flag;
        updateUIEnabling();
    }
}

bool QtSLiMWindow::offerAndExecuteAutofix(QTextCursor target, QString replacement, QString explanation, QString terminationMessage)
{
    QString informativeText = "SLiMgui has found an issue with your script that it knows how to fix:\n\n";
    informativeText.append(explanation);
    informativeText.append("\n\nWould you like SLiMgui to automatically fix it, and then recycle?\n");
    
    QMessageBox messageBox(this);
    messageBox.setText("Autofixable Error");
    messageBox.setInformativeText(informativeText);
    messageBox.setDetailedText(terminationMessage.trimmed());
    messageBox.setIcon(QMessageBox::Warning);
    messageBox.setWindowModality(Qt::WindowModal);
    messageBox.setFixedWidth(700);      // seems to be ignored
    messageBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    
    int button = messageBox.exec();
    
    if (button == QMessageBox::Yes)
    {
        target.insertText(replacement);
        recycleClicked();
        return true;
    }
    
    return false;
}

bool QtSLiMWindow::checkTerminationForAutofix(QString terminationMessage)
{
    QTextCursor selection = ui->scriptTextEdit->textCursor();
    QString selectionString = selection.selectedText();
    
    // get the four characters prior to the selected error range, to recognize if the error is preceded by "sim."; note this is a heuristic, not precise
    QTextCursor beforeSelection4 = selection;
    beforeSelection4.setPosition(beforeSelection4.selectionStart(), QTextCursor::MoveAnchor);
    beforeSelection4.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 4);
    beforeSelection4.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 4);
    QString beforeSelection4String = beforeSelection4.selectedText();
    
    // early() events are no longer default
    if (terminationMessage.contains("unexpected token {") &&
            terminationMessage.contains("expected an event declaration") &&
            terminationMessage.contains("early() is no longer a default script block type") &&
            (selectionString == "{"))
        return offerAndExecuteAutofix(selection, "early() {", "Script blocks no longer default to `early()`; `early()` must be explicitly specified.", terminationMessage);
    
    // sim to community changes
    if ((beforeSelection4String == "sim.") && terminationMessage.contains("method createLogFile() is not defined on object element type Species"))
        return offerAndExecuteAutofix(beforeSelection4, "community.", "The `createLogFile()` method has been moved to the Community class.", terminationMessage);
    
    if ((beforeSelection4String == "sim.") && terminationMessage.contains("method deregisterScriptBlock() is not defined on object element type Species"))
        return offerAndExecuteAutofix(beforeSelection4, "community.", "The `deregisterScriptBlock()` method has been moved to the Community class.", terminationMessage);
    
    if ((beforeSelection4String == "sim.") && terminationMessage.contains("method registerFirstEvent() is not defined on object element type Species"))
        return offerAndExecuteAutofix(beforeSelection4, "community.", "The `registerFirstEvent()` method has been moved to the Community class.", terminationMessage);
    
    if ((beforeSelection4String == "sim.") && terminationMessage.contains("method registerEarlyEvent() is not defined on object element type Species"))
        return offerAndExecuteAutofix(beforeSelection4, "community.", "The `registerEarlyEvent()` method has been moved to the Community class.", terminationMessage);
    
    if ((beforeSelection4String == "sim.") && terminationMessage.contains("method registerLateEvent() is not defined on object element type Species"))
        return offerAndExecuteAutofix(beforeSelection4, "community.", "The `registerLateEvent()` method has been moved to the Community class.", terminationMessage);
    
    if ((beforeSelection4String == "sim.") && terminationMessage.contains("method rescheduleScriptBlock() is not defined on object element type Species"))
        return offerAndExecuteAutofix(beforeSelection4, "community.", "The `rescheduleScriptBlock()` method has been moved to the Community class.", terminationMessage);
    
    if ((beforeSelection4String == "sim.") && terminationMessage.contains("method simulationFinished() is not defined on object element type Species"))
        return offerAndExecuteAutofix(beforeSelection4, "community.", "The `simulationFinished()` method has been moved to the Community class.", terminationMessage);
    
    if ((beforeSelection4String == "sim.") && terminationMessage.contains("method outputUsage() is not defined on object element type Species"))
        return offerAndExecuteAutofix(beforeSelection4, "community.", "The `outputUsage()` method has been moved to the Community class.", terminationMessage);
    
    if ((beforeSelection4String == "sim.") && terminationMessage.contains("property logFiles is not defined for object element type Species"))
        return offerAndExecuteAutofix(beforeSelection4, "community.", "The `logFiles` property has been moved to the Community class.", terminationMessage);
    
    if ((beforeSelection4String == "sim.") && terminationMessage.contains("property generationStage is not defined for object element type Species"))
        return offerAndExecuteAutofix(beforeSelection4, "community.", "The `generationStage` property has been moved to the Community class.", terminationMessage);
    
    if ((beforeSelection4String == "sim.") && terminationMessage.contains("property modelType is not defined for object element type Species"))
        return offerAndExecuteAutofix(beforeSelection4, "community.", "The `modelType` property has been moved to the Community class.", terminationMessage);
    
    if ((beforeSelection4String == "sim.") && terminationMessage.contains("property verbosity is not defined for object element type Species"))
        return offerAndExecuteAutofix(beforeSelection4, "community.", "The `verbosity` property has been moved to the Community class.", terminationMessage);
    
    // generation to tick changes
    if (terminationMessage.contains("property originGeneration is not defined for object element type Mutation"))
        return offerAndExecuteAutofix(selection, "originTick", "The `originGeneration` property has been removed from Mutation; in its place is `originTick` (which measures in ticks, not generations).", terminationMessage);

    if (terminationMessage.contains("property originGeneration is not defined for object element type Substitution"))
        return offerAndExecuteAutofix(selection, "originTick", "The `originGeneration` property has been removed from Substitution; in its place is `originTick` (which measures in ticks, not generations).", terminationMessage);

    if (terminationMessage.contains("property fixationGeneration is not defined for object element type Substitution"))
        return offerAndExecuteAutofix(selection, "fixationTick", "The `fixationGeneration` property has been removed from Substitution; in its place is `fixationTick` (which measures in ticks, not generations).", terminationMessage);
    
    // generation to cycle changes
    if (terminationMessage.contains("property generation is not defined for object element type Species"))
        return offerAndExecuteAutofix(selection, "cycle", "The `generation` property of Species has been renamed to `cycle`.", terminationMessage);
    
    if (terminationMessage.contains("property generationStage is not defined for object element type Community"))
        return offerAndExecuteAutofix(selection, "cycleStage", "The `generationStage` property of Community has been renamed to `cycleStage`.", terminationMessage);
    
    if (terminationMessage.contains("method addGeneration() is not defined on object element type LogFile"))
        return offerAndExecuteAutofix(selection, "addCycle", "The `addGeneration()` method of Community has been renamed to `addCycle()`.", terminationMessage);
    
    if (terminationMessage.contains("method addGenerationStage() is not defined on object element type LogFile"))
        return offerAndExecuteAutofix(selection, "addCycleStage", "The `addGenerationStage()` method of Community has been renamed to `addCycleStage()`.", terminationMessage);
    
    // removal of various callback pseudo-parameters
    if (terminationMessage.contains("undefined identifier genome1"))
        return offerAndExecuteAutofix(selection, "individual.genome1", "The `genome1` pseudo-parameter has been removed; it is now accessed as `individual.genome1`.", terminationMessage);

    if (terminationMessage.contains("undefined identifier genome2"))
        return offerAndExecuteAutofix(selection, "individual.genome2", "The `genome2` pseudo-parameter has been removed; it is now accessed as `individual.genome2`.", terminationMessage);

    if (terminationMessage.contains("undefined identifier childGenome1"))
        return offerAndExecuteAutofix(selection, "child.genome1", "The `childGenome1` pseudo-parameter has been removed; it is now accessed as `child.genome1`.", terminationMessage);

    if (terminationMessage.contains("undefined identifier childGenome2"))
        return offerAndExecuteAutofix(selection, "child.genome2", "The `childGenome2` pseudo-parameter has been removed; it is now accessed as `child.genome2`.", terminationMessage);

    if (terminationMessage.contains("undefined identifier parent1Genome1"))
        return offerAndExecuteAutofix(selection, "parent1.genome1", "The `parent1Genome1` pseudo-parameter has been removed; it is now accessed as `parent1.genome1`.", terminationMessage);

    if (terminationMessage.contains("undefined identifier parent1Genome2"))
        return offerAndExecuteAutofix(selection, "parent1.genome2", "The `parent1Genome2` pseudo-parameter has been removed; it is now accessed as `parent1.genome2`.", terminationMessage);

    if (terminationMessage.contains("undefined identifier parent2Genome1"))
        return offerAndExecuteAutofix(selection, "parent2.genome1", "The `parent2Genome1` pseudo-parameter has been removed; it is now accessed as `parent2.genome1`.", terminationMessage);

    if (terminationMessage.contains("undefined identifier parent2Genome2"))
        return offerAndExecuteAutofix(selection, "parent2.genome2", "The `parent2Genome2` pseudo-parameter has been removed; it is now accessed as `parent2.genome2`.", terminationMessage);

    if (terminationMessage.contains("undefined identifier childIsFemale"))
        return offerAndExecuteAutofix(selection, "(child.sex == \"F\")", "The `childIsFemale` pseudo-parameter has been removed; it is now accessed as `child.sex == \"F\"`.", terminationMessage);
    
    // changes to InteractionType -evaluate()
    if (terminationMessage.contains("missing required argument subpops") && (selectionString == "evaluate"))
    {
        QTextCursor entireCall = selection;
        entireCall.setPosition(entireCall.selectionStart(), QTextCursor::MoveAnchor);
        entireCall.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 11);
        QString entireCallString = entireCall.selectedText();
        
        if (entireCallString == "evaluate();")
            return offerAndExecuteAutofix(entireCall, "evaluate(sim.subpopulations);", "The evaluate() method now requires a vector of subpopulations to evaluate.", terminationMessage);
    }
    
    if (terminationMessage.contains("named argument immediate skipped over required argument subpops") && (selectionString == "evaluate"))
    {
        QTextCursor entireCall = selection;
        entireCall.setPosition(entireCall.selectionStart(), QTextCursor::MoveAnchor);
        entireCall.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 22);
        QString entireCallString = entireCall.selectedText();
        
        if ((entireCallString == "evaluate(immediate=T);") || (entireCallString == "evaluate(immediate=F);"))
            return offerAndExecuteAutofix(entireCall, "evaluate(sim.subpopulations);", "The evaluate() method no longer supports immediate evaluation, and the `immediate` parameter has been removed.", terminationMessage);
    }
    
    if (terminationMessage.contains("unrecognized named argument immediate") && (selectionString == "evaluate"))
    {
        {
            QTextCursor callEnd = selection;
            callEnd.setPosition(callEnd.selectionStart(), QTextCursor::MoveAnchor);
            callEnd.movePosition(QTextCursor::EndOfLine, QTextCursor::MoveAnchor, 1);
            callEnd.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 15);
            QString callEndString = callEnd.selectedText();
            
            if ((callEndString == ", immediate=T);") || (callEndString == ", immediate=F);"))
                return offerAndExecuteAutofix(callEnd, ");", "The evaluate() method no longer supports immediate evaluation, and the `immediate` parameter has been removed.", terminationMessage);
        }
        
        {
            QTextCursor callEnd = selection;
            callEnd.setPosition(callEnd.selectionStart(), QTextCursor::MoveAnchor);
            callEnd.movePosition(QTextCursor::EndOfLine, QTextCursor::MoveAnchor, 1);
            callEnd.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 14);
            QString callEndString = callEnd.selectedText();
            
            if ((callEndString == ",immediate=T);") || (callEndString == ",immediate=F);"))
                return offerAndExecuteAutofix(callEnd, ");", "The evaluate() method no longer supports immediate evaluation, and the `immediate` parameter has been removed.", terminationMessage);
        }
        
        {
            QTextCursor callEnd = selection;
            callEnd.setPosition(callEnd.selectionStart(), QTextCursor::MoveAnchor);
            callEnd.movePosition(QTextCursor::EndOfLine, QTextCursor::MoveAnchor, 1);
            callEnd.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 17);
            QString callEndString = callEnd.selectedText();
            
            if ((callEndString == ", immediate = T);") || (callEndString == ", immediate = F);"))
                return offerAndExecuteAutofix(callEnd, ");", "The evaluate() method no longer supports immediate evaluation, and the `immediate` parameter has been removed.", terminationMessage);
        }
        
        {
            QTextCursor callEnd = selection;
            callEnd.setPosition(callEnd.selectionStart(), QTextCursor::MoveAnchor);
            callEnd.movePosition(QTextCursor::EndOfLine, QTextCursor::MoveAnchor, 1);
            callEnd.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 16);
            QString callEndString = callEnd.selectedText();
            
            if ((callEndString == ",immediate = T);") || (callEndString == ",immediate = F);"))
                return offerAndExecuteAutofix(callEnd, ");", "The evaluate() method no longer supports immediate evaluation, and the `immediate` parameter has been removed.", terminationMessage);
        }
    }
    
    // API changes in anticipation of multi-phenotype
    if (terminationMessage.contains("unexpected identifier @fitness; expected an event declaration"))
    {
        {
            QTextCursor callbackDecl = selection;
            callbackDecl.setPosition(callbackDecl.selectionStart(), QTextCursor::MoveAnchor);
            callbackDecl.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 14);
            QString callbackDeclString = callbackDecl.selectedText();
            
            if (callbackDeclString == "fitness(NULL, ")
                return offerAndExecuteAutofix(callbackDecl, "fitnessEffect(", "The fitness(NULL) callback type is now called a fitnessEffect() callback.", terminationMessage);
        }
        
        {
            QTextCursor callbackDecl = selection;
            callbackDecl.setPosition(callbackDecl.selectionStart(), QTextCursor::MoveAnchor);
            callbackDecl.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 13);
            QString callbackDeclString = callbackDecl.selectedText();
            
            if (callbackDeclString == "fitness(NULL,")
                return offerAndExecuteAutofix(callbackDecl, "fitnessEffect(", "The fitness(NULL) callback type is now called a fitnessEffect() callback.", terminationMessage);
            if (callbackDeclString == "fitness(NULL)")
                return offerAndExecuteAutofix(callbackDecl, "fitnessEffect()", "The fitness(NULL) callback type is now called a fitnessEffect() callback.", terminationMessage);
        }
        
        {
            QTextCursor callbackDecl = selection;
            callbackDecl.setPosition(callbackDecl.selectionStart(), QTextCursor::MoveAnchor);
            callbackDecl.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 9);
            QString callbackDeclString = callbackDecl.selectedText();
            
            if (callbackDeclString == "fitness(m")
                return offerAndExecuteAutofix(callbackDecl, "mutationEffect(m", "The fitness() callback type is now called a mutationEffect() callback.", terminationMessage);
        }
    }
    
    if (terminationMessage.contains("undefined identifier relFitness"))
        return offerAndExecuteAutofix(selection, "effect", "The `relFitness` pseudo-parameter has been renamed to `effect`.", terminationMessage);
    
    // other deprecated APIs, unrelated to multispecies and multi-phenotype
    if ((beforeSelection4String == "sim.") && terminationMessage.contains("property inSLiMgui is not defined for object element type Species"))
    {
        QTextCursor simAndSelection = beforeSelection4;
        simAndSelection.setPosition(selection.selectionEnd(), QTextCursor::KeepAnchor);
        
        return offerAndExecuteAutofix(simAndSelection, "exists(\"slimgui\")", "The `inSLiMgui` property has been removed; now use `exists(\"slimgui\")`.", terminationMessage);
    }
    
    return false;
}

void QtSLiMWindow::showTerminationMessage(QString terminationMessage, EidosErrorContext errorContext)
{
    //qDebug() << terminationMessage;
    
    // Depending on the circumstances of the error, we might be able to select a range in our input file to show what caused the error
	if (!changedSinceRecycle())
    {
		ui->scriptTextEdit->selectErrorRange(errorContext);
        
        // check to see if this is an error we can assist the user in fixing; if they choose to autofix, we are done
        if (checkTerminationForAutofix(terminationMessage))
            return;
    }
    
    // Show an error sheet/panel
    QString fullMessage(terminationMessage);
    
    fullMessage.append("\nThis error has invalidated the simulation; it cannot be run further.  Once the script is fixed, you can recycle the simulation and try again.");
    
    QMessageBox messageBox(this);
    messageBox.setText("Simulation Runtime Error");
    messageBox.setInformativeText(fullMessage);
    messageBox.setIcon(QMessageBox::Warning);
    messageBox.setWindowModality(Qt::WindowModal);
    messageBox.setFixedWidth(700);      // seems to be ignored
    messageBox.exec();
    
    // Show the error in the status bar also
    statusBar()->showMessage("<font color='#cc0000' style='font-size: 11px;'>" + terminationMessage.trimmed().toHtmlEscaped() + "</font>");
}

void QtSLiMWindow::checkForSimulationTermination(void)
{
    std::string &&terminationMessage = gEidosTermination.str();

    if (!terminationMessage.empty())
    {
        // Get the termination message and clear the global
        QString message = QString::fromStdString(terminationMessage);

        gEidosTermination.clear();
        gEidosTermination.str("");

        // Get the error position and clear the global
        EidosErrorContext errorContext = gEidosErrorContext;
        
        gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, nullptr, false};
        
        // Send the signal, which connects up to QtSLiMWindow::showTerminationMessage() through a Qt::QueuedConnection
        emit terminationWithMessage(message, errorContext);
        
        // Now we need to clean up so we are in a displayable state.  Note that we don't even attempt to dispose
        // of the old simulation object; who knows what state it is in, touching it might crash.
        community = nullptr;
        focalSpecies = nullptr;
        slimgui = nullptr;

		if (sim_RNG_initialized)
		{
        	_Eidos_FreeOneRNG(sim_RNG);
        	sim_RNG_initialized = false;
		}
		
        setReachedSimulationEnd(true);
        setInvalidSimulation(true);
    }
}

void QtSLiMWindow::startNewSimulationFromScript(void)
{
    if (community)
    {
        delete community;
        community = nullptr;
        focalSpecies = nullptr;
    }
    if (slimgui)
    {
        delete slimgui;
        slimgui = nullptr;
    }
    
    // forget any script block coloring
    ui->scriptTextEdit->clearScriptBlockColoring();

	// Free the old simulation RNG and make a new one, to have clean state
	if (sim_RNG_initialized)
	{
		_Eidos_FreeOneRNG(sim_RNG);
		sim_RNG_initialized = false;
	}
	
	_Eidos_InitializeOneRNG(sim_RNG);
	sim_RNG_initialized = true;
	
	// The Eidos RNG may be set up already; if so, get rid of it.  When we are not running, we keep the
	// Eidos RNG in an initialized state, to catch errors with the swapping of RNG state.  Nobody should
	// use it when we have not swapped in our own RNG.
	if (gEidos_RNG_Initialized)
	{
		_Eidos_FreeOneRNG(gEidos_RNG_SINGLE);
		gEidos_RNG_Initialized = false;
	}
	
	// Swap in our RNG
	std::swap(sim_RNG, gEidos_RNG_SINGLE);
	std::swap(sim_RNG_initialized, gEidos_RNG_Initialized);

    std::istringstream infile(scriptString);

    try
    {
        community = new Community();
        community->InitializeFromFile(infile);
        community->InitializeRNGFromSeed(nullptr);
        community->SetDebugPoints(&ui->scriptTextEdit->debuggingPoints());

		// Swap out our RNG
		std::swap(sim_RNG, gEidos_RNG_SINGLE);
		std::swap(sim_RNG_initialized, gEidos_RNG_Initialized);

        // We also reset various Eidos/SLiM instance state; each SLiMgui window is independent
        sim_next_pedigree_id = 0;
        sim_next_mutation_id = 0;
        sim_suppress_warnings = false;

        // The current working directory was set up in -init to be ~/Desktop, and should not be reset here; if the
        // user has changed it, that change ought to stick across recycles.  So this bounces us back to the last dir chosen.
        sim_working_dir = sim_requested_working_dir;

        setReachedSimulationEnd(false);
        setInvalidSimulation(false);
        hasImported_ = false;
    }
    catch (...)
    {
        // BCH 12/25/2022: adding this to swap out our RNG after a raise, seems better...
		std::swap(sim_RNG, gEidos_RNG_SINGLE);
		std::swap(sim_RNG_initialized, gEidos_RNG_Initialized);
        
        if (community)
            community->simulation_valid_ = false;
        setReachedSimulationEnd(true);
        checkForSimulationTermination();
    }

    if (community)
    {
        // make a new SLiMgui instance to represent SLiMgui in Eidos
        slimgui = new SLiMgui(*community, this);

        // set up the "slimgui" symbol for it immediately
        community->simulation_constants_->InitializeConstantSymbolEntry(slimgui->SymbolTableEntry());
    }
    
    if (community && community->simulation_valid_ && (community->all_species_.size() > 1))
    {
        // set up script block coloring
        std::vector<SLiMEidosBlock*> &blocks = community->AllScriptBlocks();
        
        for (SLiMEidosBlock *block : blocks)
        {
            Species *species = (block->species_spec_ ? block->species_spec_ : (block->ticks_spec_ ? block->ticks_spec_ : nullptr));
            
            if (species && (block->user_script_line_offset_ != -1) && block->root_node_ && block->root_node_->token_)
            {
                EidosToken *block_root_token = block->root_node_->token_;
                int startPos = block_root_token->token_UTF16_start_;
                int endPos = block_root_token->token_UTF16_end_;
                
                ui->scriptTextEdit->addScriptBlockColoring(startPos, endPos, species);
            }
        }
    }
}

void QtSLiMWindow::setScriptStringAndInitializeSimulation(std::string string)
{
    scriptString = string;
    startNewSimulationFromScript();
}

Species *QtSLiMWindow::focalDisplaySpecies(void)
{
    // SLiMgui focuses on one species at a time in its main window display; this method should be called to obtain that species.
	// This funnel method checks for various invalid states and returns nil; callers should check for a nil return as needed.
	if (!invalidSimulation_ && community && community->simulation_valid_)
	{
        // If we have a focal species set already, it must be valid (the community still exists), so return it
        if (focalSpecies)
            return focalSpecies;
        
        // If "all" is chosen, we return nullptr, which represents that state
        if (focalSpeciesName.compare("all") == 0)
            return nullptr;
        
        // If not, we'll choose a species from the species list if there are any
		const std::vector<Species *> &all_species = community->AllSpecies();
		
		if (all_species.size() >= 1)
        {
            // If we have a species name remembered, try to choose that species again
            if (focalSpeciesName.length())
            {
                for (Species *species : all_species)
                {
                    if (species->name_.compare(focalSpeciesName) == 0)
                    {
                        focalSpecies = species;
                        return focalSpecies;
                    }
                }
            }
            
            // Failing that, choose the first declared species and remember its name
            focalSpecies = all_species[0];
            focalSpeciesName = focalSpecies->name_;
        }
	}
	
	return nullptr;
}

void QtSLiMWindow::selectedSpeciesChanged(void)
{
    // We don't want to react to automatic tab changes as we are adding or removing tabs from the species bar
    if (reloadingSpeciesBar)
        return;
    
    int speciesIndex = ui->speciesBar->currentIndex();
    const std::vector<Species *> &allSpecies = community->AllSpecies();
    
    if (speciesIndex == (int)allSpecies.size())
    {
        // this is the "all" tab
        focalSpecies = nullptr;
        focalSpeciesName = "all";
    }
    else
    {
        if ((speciesIndex < 0) || (speciesIndex >= (int)allSpecies.size()))
        {
            qDebug() << "selectedSpeciesChanged() index" << speciesIndex << "out of range";
            return;
        }
        
        focalSpecies = allSpecies[speciesIndex];
        focalSpeciesName = focalSpecies->name_;
    }
    
    //qDebug() << "selectedSpeciesChanged(): changed to species name" << QString::fromStdString(focalSpeciesName);
    
    // do a full update to show the state for the new species
    updateAfterTickFull(true);
}

QtSLiMGraphView *QtSLiMWindow::graphViewForGraphWindow(QWidget *p_window)
{
    if (p_window)
    {
        QLayout *window_layout = p_window->layout();
        
        if (window_layout && (window_layout->count() > 0))
        {
            QLayoutItem *item = window_layout->itemAt(0);
            
            if (item)
                return qobject_cast<QtSLiMGraphView *>(item->widget());
        }
    }
    return nullptr;
}

void QtSLiMWindow::updateOutputViews(void)
{
    QtSLiMDebugOutputWindow *debugWindow = debugOutputWindow();
    std::string &&newOutput = gSLiMOut.str();
	
	if (!newOutput.empty())
	{
        QString str = QString::fromStdString(newOutput);
		
		// So, ideally we would stay pinned at the bottom if the user had scrolled to the bottom, but would stay
		// at the user's chosen scroll position above the bottom if they chose such a position.  Unfortunately,
		// this doesn't seem to work.  I'm not quite sure why.  Particularly when large amounts of output get
		// added quickly, the scroller doesn't seem to catch up, and then it reads here as not being at the
		// bottom, and so we become unpinned even though we used to be pinned.  I'm going to just give up, for
		// now, and always scroll to the bottom when new output comes out.  That's what many other such apps
		// do anyway; it's a little annoying if you're trying to read old output, but so it goes.
		
		//NSScrollView *enclosingScrollView = [outputTextView enclosingScrollView];
		//BOOL scrolledToBottom = YES; //(![enclosingScrollView hasVerticalScroller] || [[enclosingScrollView verticalScroller] doubleValue] == 1.0);
		
        // ui->outputTextEdit->append(str) would seem the obvious thing to do, but that adds an extra newline (!),
        // so it can't be used.  WTF.  The solution here does not preserve the user's scroll position; see discussion at
        // https://stackoverflow.com/questions/13559990/how-to-append-text-to-qplaintextedit-without-adding-newline-and-keep-scroll-at
        // which has a complex solution involving subclassing QPlainTextEdit... sigh...
        ui->outputTextEdit->moveCursor(QTextCursor::End);
        ui->outputTextEdit->insertPlainText(str);
        ui->outputTextEdit->moveCursor(QTextCursor::End);
        
		//if ([[NSUserDefaults standardUserDefaults] boolForKey:defaultsSyntaxHighlightOutputKey])
		//	[outputTextView recolorAfterChanges];
		
		// if the user was scrolled to the bottom, we keep them there; otherwise, we let them stay where they were
		//if (scrolledToBottom)
		//	[outputTextView scrollRangeToVisible:NSMakeRange([[outputTextView string] length], 0)];
		
        // We add run output to the appropriate subview of the output viewer, too; it shows up in both places
        if (debugWindow)
            debugWindow->takeRunOutput(str);
        
		// clear any error flags set on the stream and empty out its string so it is ready to receive new output
		gSLiMOut.clear();
		gSLiMOut.str("");
	}
    
    // BCH 2/9/2021: We now handle the error output here too, since we want to be in charge of how the
    // debug window shows itself, etc.  We follow the same strategy as above; comments have been removed.
    std::string &&newErrors = gSLiMError.str();
    
    if (!newErrors.empty())
    {
        QString str = QString::fromStdString(newErrors);
        
        if (debugWindow)
        {
            debugWindow->takeDebugOutput(str);
            
            // Flash the debugging output button to alert the user to new output
            flashDebugButton();
        }
        
        gSLiMError.clear();
        gSLiMError.str("");
    }
    
    // BCH 5/15/2022: And now scheduling stream output happens here too, following the pattern above.
    std::string &&newSchedulingOutput = gSLiMScheduling.str();
    
    if (!newSchedulingOutput.empty())
    {
        QString str = QString::fromStdString(newSchedulingOutput);
        
        if (debugWindow)
            debugWindow->takeSchedulingOutput(str);
        
        gSLiMScheduling.clear();
        gSLiMScheduling.str("");
    }
    
    // Scan through LogFile instances kept by the sim and flush them to the debug window
    if (debugWindow && !invalidSimulation_ && community)
    {
        for (LogFile *logfile : community->log_file_registry_)
        {
            for (auto &lineElements : logfile->emitted_lines_)
            {
                // This call takes a vector of string elements comprising one logfile output line
                debugWindow->takeLogFileOutput(lineElements, logfile->user_file_path_);
            }
            
            logfile->emitted_lines_.clear();
        }
    }
    
    // Scan through file output kept by the sim and flush it to the debug window
    if (debugWindow && !invalidSimulation_ && community)
    {
        for (size_t index = 0; index < community->file_write_paths_.size(); ++index)
        {
            // This call takes a vector of lines comprising all the output for one file
            debugWindow->takeFileOutput(community->file_write_buffers_[index], community->file_write_appends_[index], community->file_write_paths_[index]);
        }
        
        community->file_write_paths_.clear();
        community->file_write_buffers_.clear();
        community->file_write_appends_.clear();
    }
}

void QtSLiMWindow::flashDebugButton(void)
{
    // every 40 is one cycle up and down, to red and back; so 200 gives five cycles,
    // which seems good for catching the user's attention effectively; maybe excessive,
    // but that's better than being missed...
    if (debugButtonFlashCount_ == 0)
        debugButtonFlashCount_ = 200;
    else if (debugButtonFlashCount_ < 200)
        debugButtonFlashCount_ += 40;       // new output adds one cycle, up to the max of five
    
    debugButtonFlashTimer_.start(0);
}

void QtSLiMWindow::stopDebugButtonFlash(void)
{
    // called when the button gets clicked, pressed, etc.
    debugButtonFlashCount_ = 0;
    ui->debugOutputButton->setTemporaryIconOpacity(0.0);
    debugButtonFlashTimer_.stop();
}

void QtSLiMWindow::handleDebugButtonFlash(void)
{
    // decrement with each tick
    --debugButtonFlashCount_;
    if (debugButtonFlashCount_ < 0)
        debugButtonFlashCount_ = 0;
    
    // set opacity of the red overlay based on the counter, and reschedule ourselves as needed
    if (debugButtonFlashCount_ == 0)
    {
        stopDebugButtonFlash();
    }
    else
    {
        int opacity_int = debugButtonFlashCount_ % 40;
        const double PI = 3.141592653589793;    // not in the C++ standard until C++20!
        //double opacity_float = std::max(0.0, std::sin(PI * opacity_int / 40.0));                              // dwell on red; not as nice, I decided
        double opacity_float = std::max(0.0, 1.0 - (std::cos(2 * PI * opacity_int / 40.0) * 0.5 + 0.5));        // equal time red and non-red
        
        //qDebug() << "debugButtonFlashCount_" << debugButtonFlashCount_ << ", opacity_int" << opacity_int << ", opacity_float" << opacity_float;
        
        ui->debugOutputButton->setTemporaryIconOpacity(opacity_float);
        
        if (debugButtonFlashTimer_.interval() != 17)   // about 60 Hz
            debugButtonFlashTimer_.start(17);
    }
}

void QtSLiMWindow::updateTickCounter(void)
{
    Species *displaySpecies = focalDisplaySpecies();
    
    if (!displaySpecies)
        ui->cycleLineEdit->setText("");
    else if (community->Tick() == 0)
        ui->cycleLineEdit->setText("initialize()");
    else
         ui->cycleLineEdit->setText(QString::number(displaySpecies->Cycle()));
    
    if (!community)
    {
        ui->tickLineEdit->setText("");
        ui->tickLineEdit->setProgress(0.0);
    }
    else if (community->Tick() == 0)
    {
        ui->tickLineEdit->setText("initialize()");
        ui->tickLineEdit->setProgress(0.0);
    }
    else
    {
        slim_tick_t tick = community->Tick();
        slim_tick_t lastTick = community->EstimatedLastTick();
        
        double progress = (lastTick > 0) ? (tick / (double)lastTick) : 0.0;
        
        ui->tickLineEdit->setText(QString::number(tick));
        ui->tickLineEdit->setProgress(progress);
    }
}

void QtSLiMWindow::updateSpeciesBar(void)
{
    // Update the species bar as needed; we do this only after initialization, to avoid a hide/show on recycle of multispecies models
    if (!invalidSimulation_ && community && community->simulation_valid_ && (community->Tick() >= 1))
    {
        bool speciesBarVisibleNow = !ui->speciesBarWidget->isHidden();
        bool speciesBarShouldBeVisible = (community->all_species_.size() > 1);
        
        if (speciesBarVisibleNow && !speciesBarShouldBeVisible)
        {
            ui->speciesBar->setEnabled(false);
            ui->speciesBarWidget->setHidden(true);
            
            reloadingSpeciesBar = true;
            
            while (ui->speciesBar->count())
                ui->speciesBar->removeTab(0);
            
            reloadingSpeciesBar = false;
        }
        else if (!speciesBarVisibleNow && speciesBarShouldBeVisible)
        {
            ui->speciesBar->setEnabled(true);
            ui->speciesBarWidget->setHidden(false);
            
            if ((ui->speciesBar->count() == 0) && (community->all_species_.size() > 0))
            {
                // add tabs for species when shown
                int selectedSpeciesIndex = 0;
                bool avatarsOnly = (community->all_species_.size() > 2);
                
                reloadingSpeciesBar = true;
                
                for (Species *species : community->all_species_)
                {
                    QString tabLabel = QString::fromStdString(species->avatar_);
                    
                    if (!avatarsOnly)
                    {
                        tabLabel.append(" ");
                        tabLabel.append(QString::fromStdString(species->name_));
                    }
                    
                    int newTabIndex = ui->speciesBar->addTab(tabLabel);
                    
                    ui->speciesBar->setTabToolTip(newTabIndex, QString::fromStdString(species->name_).prepend("Species "));
                    
                    if (focalSpeciesName.length() && (species->name_.compare(focalSpeciesName) == 0))
                        selectedSpeciesIndex = newTabIndex;
                }
                
                {
                    // add the "all" tab
                    QString allLabel = QString::fromUtf8("\xF0\x9F\x94\x85");   // "low brightness symbol", https://www.compart.com/en/unicode/U+1F505
                
                    if (!avatarsOnly)
                        allLabel.append(" all");
                    
                    int newTabIndex = ui->speciesBar->addTab(allLabel);
                    
                    ui->speciesBar->setTabToolTip(newTabIndex, "Show all species together");
                    
                    if (focalSpeciesName.length() && (focalSpeciesName.compare("all") == 0))
                        selectedSpeciesIndex = newTabIndex;
                }
                
                reloadingSpeciesBar = false;
                
                //qDebug() << "selecting index" << selectedSpeciesIndex << "for name" << QString::fromStdString(focalSpeciesName);
                ui->speciesBar->setCurrentIndex(selectedSpeciesIndex);
            }
        }
    }
    else
    {
        // Whenever we're invalid or uninitialized, we hide the species bar and disable and remove all the tabs
        ui->speciesBar->setEnabled(false);
        ui->speciesBarWidget->setHidden(true);
        
        reloadingSpeciesBar = true;
        
        while (ui->speciesBar->count())
            ui->speciesBar->removeTab(0);
        
        reloadingSpeciesBar = false;
    }
}

void QtSLiMWindow::removeExtraChromosomeViews(void)
{
    while (chromosomeOverviewWidgets.size() > 1)
    {
        QVBoxLayout *widgetLayout = chromosomeWidgetLayouts.back();
        
        ui->chromosomeLayout->removeItem(widgetLayout);
        
        // remove all items under widgetLayout
        QLayoutItem *child;
        
        while ((child = widgetLayout->takeAt(0)) != nullptr)
        {
            delete child->widget(); // delete the widget
            delete child;   // delete the layout item
        }
        
        delete widgetLayout;
        
        ui->chromosomeLayout->update();
        
        chromosomeWidgetLayouts.pop_back();
        chromosomeOverviewWidgets.pop_back();
        chromosomeZoomedWidgets.pop_back();
    }
    
    // Sometimes the call above to "delete child->widget()" hangs for up to a second.  This appears to be due to
    // disposing of the OpenGL context used for the widget, and might be an AMD Radeon issue.  Here's a backtrace
    // I managed to get from sample.  The only thing I can think of to do about this would be to keep the view
    // around and reuse it, to avoid having to dispose of its context.  But this may be specific to my hardware;
    // probably not worth jumping through hoops to address.  BCH 5/9/2022
    //
    // 845 QtSLiMChromosomeWidget::~QtSLiMChromosomeWidget()  (in SLiMgui) + 188  [0x1033354ac]  QtSLiMChromosomeWidget.cpp:140
    //   845 QOpenGLWidget::~QOpenGLWidget()  (in QtWidgets) + 39  [0x104811887]  qopenglwidget.cpp:1020
    //     844 QOpenGLWidgetPrivate::reset()  (in QtWidgets) + 226  [0x104810582]  qopenglwidget.cpp:719
    //       844 QOpenGLContext::~QOpenGLContext()  (in QtGui) + 24  [0x104e58f68]  qopenglcontext.cpp:690
    //         844 QOpenGLContext::destroy()  (in QtGui) + 200  [0x104e58818]  qopenglcontext.cpp:653
    //           844 QCocoaGLContext::~QCocoaGLContext()  (in libqcocoa.dylib) + 14  [0x105c648ce]  qcocoaglcontext.mm:354
    //             844 QCocoaGLContext::~QCocoaGLContext()  (in libqcocoa.dylib) + 51  [0x105c64753]  qcocoaglcontext.mm:355
    //               844 -[NSOpenGLContext dealloc]  (in AppKit) + 62  [0x7fff34349987]
    //                 844 CGLReleaseContext  (in OpenGL) + 178  [0x7fff4166942a]
    //                   843 gliDestroyContext  (in GLEngine) + 127  [0x7fff4168f9d1]
    //                     843 gldDestroyContext  (in libGPUSupportMercury.dylib) + 114  [0x7fff57fe6745]
    //                       842 glrTerminateContext  (in AMDRadeonX6000GLDriver) + 42  [0x11f390257]
}

void QtSLiMWindow::updateChromosomeViewSetup(void)
{
    Species *displaySpecies = focalDisplaySpecies();
    
    QtSLiMChromosomeWidget *overviewWidget = chromosomeOverviewWidgets[0];
    QtSLiMChromosomeWidget *zoomedWidget = chromosomeZoomedWidgets[0];
    
    if (invalidSimulation_ || !community || !community->simulation_valid_ || (community->Tick() == 0))
    {
        // We are in an invalid state of some kind, so we want one chromosome view that is displaying the empty state
        overviewWidget->setFocalDisplaySpecies(nullptr);
        zoomedWidget->setFocalDisplaySpecies(nullptr);
        
        removeExtraChromosomeViews();
    }
    else if (displaySpecies)
    {
        // We have a focal display species, so we want just one chromosome view, displaying that species
        overviewWidget->setFocalDisplaySpecies(displaySpecies);
        zoomedWidget->setFocalDisplaySpecies(displaySpecies);
        
        removeExtraChromosomeViews();
    }
    else if (chromosomeOverviewWidgets.size() != community->all_species_.size())
    {
        // We are on the "all" species tab in a multispecies model; create a chromosome view for each species
        // We should always arrive at this state through the "invalid state" case above as an intermediate
        removeExtraChromosomeViews();
        
        for (int index = 0; index < (int)community->all_species_.size(); ++index)
        {
            displaySpecies = community->all_species_[index];
            
            if (index == 0)
            {
                // overviewWidget and zoomedWidget were set above and are used for index == 0
            }
            else
            {
                // Beyond the built-in chromosome view, we create the rest dynamically
                // This code is based directly on the MOC code for the built-in views
                QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
                sizePolicy1.setHorizontalStretch(0);
                sizePolicy1.setVerticalStretch(0);
                
                QVBoxLayout *chromosomeWidgetLayout = new QVBoxLayout();
                chromosomeWidgetLayout->setSpacing(15);
                
                overviewWidget = new QtSLiMChromosomeWidget(ui->centralWidget);
                sizePolicy1.setHeightForWidth(overviewWidget->sizePolicy().hasHeightForWidth());
                overviewWidget->setSizePolicy(sizePolicy1);
                overviewWidget->setMinimumSize(QSize(0, 30));
                overviewWidget->setMaximumSize(QSize(16777215, 30));
                chromosomeWidgetLayout->addWidget(overviewWidget);
                
                zoomedWidget = new QtSLiMChromosomeWidget(ui->centralWidget);
                sizePolicy1.setHeightForWidth(zoomedWidget->sizePolicy().hasHeightForWidth());
                zoomedWidget->setSizePolicy(sizePolicy1);
                zoomedWidget->setMinimumSize(QSize(0, 65));
                zoomedWidget->setMaximumSize(QSize(16777215, 65));
                chromosomeWidgetLayout->addWidget(zoomedWidget);
                
                ui->chromosomeLayout->insertLayout(1, chromosomeWidgetLayout);
                
                addChromosomeWidgets(chromosomeWidgetLayout, overviewWidget, zoomedWidget);
            }
            
            overviewWidget->setFocalDisplaySpecies(displaySpecies);
            zoomedWidget->setFocalDisplaySpecies(displaySpecies);
        }
    }
}

void QtSLiMWindow::updateAfterTickFull(bool fullUpdate)
{
    // fullUpdate is used to suppress some expensive updating to every third update
	if (!fullUpdate)
	{
		if (++partialUpdateCount_ >= 3)
		{
			partialUpdateCount_ = 0;
			fullUpdate = true;
		}
	}
    
    // Update the species bar and then fetch the focal species after that update, which might change it
    updateSpeciesBar();
	
    // Create or destroy chromosome views for each species, and set the species for each chromosome view
    updateChromosomeViewSetup();
    
    // Flush any buffered output to files every full update, so that the user sees changes to the files without too much delay
	if (fullUpdate)
		Eidos_FlushFiles();
	
	// Check whether the simulation has terminated due to an error; if so, show an error message with a delayed perform
	checkForSimulationTermination();
	
	// The rest of the code here needs to be careful about the invalid state; we do want to update our controls when invalid, but sim is nil.
    bool inInvalidState = (!community || !community->simulation_valid_ || invalidSimulation());
    
	if (fullUpdate)
	{
		// FIXME it would be good for this updating to be minimal; reloading the tableview every time, etc., is quite wasteful...
		updateOutputViews();
		
		// Reloading the subpop tableview is tricky, because we need to preserve the selection across the reload, while also noting that the selection is forced
		// to change when a subpop goes extinct.  The current selection is noted in the gui_selected_ ivar of each subpop.  So what we do here is reload the tableview
		// while suppressing our usual update of our selection state, and then we try to re-impose our selection state on the new tableview content.  If a subpop
		// went extinct, we will fail to notice the selection change; but that is OK, since we force an update of populationView and chromosomeZoomed below anyway.
		reloadingSubpopTableview = true;
        populationTableModel_->reloadTable();
		
        int subpopCount = populationTableModel_->rowCount();
        
		if (subpopCount > 0)
		{
            ui->subpopTableView->selectionModel()->reset();
            
			for (int i = 0; i < subpopCount; ++i)
			{
                Subpopulation *subpop = populationTableModel_->subpopAtIndex(i);
                
				if (subpop->gui_selected_)
                {
                    QModelIndex modelIndex = ui->subpopTableView->model()->index(i, 0);
                    
                    ui->subpopTableView->selectionModel()->select(modelIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                }
			}
		}
        else
        {
            ui->subpopTableView->selectionModel()->clear();
        }
		
		reloadingSubpopTableview = false;
        
        // We don't want to allow an empty selection, maybe; if we are now in that state, and there are subpops to select, select them all
        // See also subpopSelectionDidChange() which also needs to do this
        if ((ui->subpopTableView->selectionModel()->selectedRows().size() == 0) && subpopCount)
            ui->subpopTableView->selectAll();
	}
	
	// Now update our other UI, some of which depends upon the state of subpopTableView
    ui->individualsWidget->update();
    
    for (QtSLiMChromosomeWidget *zoomedWidget : chromosomeZoomedWidgets)
        zoomedWidget->stateChanged();
    
    if (fullUpdate)
        updateTickCounter();
    
    if (fullUpdate)
    {
        double elapsedTimeInSLiM = elapsedCPUClock_ / static_cast<double>(CLOCKS_PER_SEC);
        
        if (elapsedTimeInSLiM == 0.0)
            ui->statusBar->clearMessage();
        else
        {
            bool inDarkMode = QtSLiMInDarkMode();
            QString message(inDarkMode ? "<font color='#AAAAAA' style='font-size: 11px;'><tt>%1</tt> CPU seconds elapsed inside SLiM; <tt>%2</tt> MB memory usage in SLiM; <tt>%3</tt> mutations segregating, <tt>%4</tt> substitutions.</font>"
                                       : "<font color='#555555' style='font-size: 11px;'><tt>%1</tt> CPU seconds elapsed inside SLiM; <tt>%2</tt> MB memory usage in SLiM; <tt>%3</tt> mutations segregating, <tt>%4</tt> substitutions.</font>");
            
            if (!inInvalidState)
            {
                int totalRegistrySize = 0;
                
                for (Species *species : community->AllSpecies())
                {
                    int registry_size;
                    
                    species->population_.MutationRegistry(&registry_size);
                    totalRegistrySize += registry_size;
                }
                
                // Tally up usage across the simulation
                SLiMMemoryUsage_Community usage_community;
                SLiMMemoryUsage_Species usage_all_species;
                
                EIDOS_BZERO(&usage_all_species, sizeof(SLiMMemoryUsage_Species));
                
                community->TabulateSLiMMemoryUsage_Community(&usage_community, nullptr);
                
                for (Species *species : community->AllSpecies())
                {
                    SLiMMemoryUsage_Species usage_one_species;
                    
                    species->TabulateSLiMMemoryUsage_Species(&usage_one_species);
                    AccumulateMemoryUsageIntoTotal_Species(usage_one_species, usage_all_species);
                }
                
                double current_memory_MB = (usage_community.totalMemoryUsage + usage_all_species.totalMemoryUsage) / (1024.0 * 1024.0);
                
                // Tally up substitutions across the simulation
                int totalSubstitutions = 0;
                
                for (Species *species : community->AllSpecies())
                    totalSubstitutions += species->population_.substitutions_.size();
                
                ui->statusBar->showMessage(message.arg(elapsedTimeInSLiM, 0, 'f', 6)
                                           .arg(current_memory_MB, 0, 'f', 1)
                                           .arg(totalRegistrySize)
                                           .arg(totalSubstitutions));
            }
            else
                ui->statusBar->showMessage(message.arg(elapsedTimeInSLiM, 0, 'f', 6));
        }
	}
    
	// Update stuff that only needs updating when the script is re-parsed, not after every tick
	if (inInvalidState || community->mutation_types_changed_)
	{
        if (tablesDrawerController && tablesDrawerController->mutTypeTableModel_)
            tablesDrawerController->mutTypeTableModel_->reloadTable();
		
		if (community)
			community->mutation_types_changed_ = false;
	}
	
	if (inInvalidState || community->genomic_element_types_changed_)
	{
        if (tablesDrawerController && tablesDrawerController->geTypeTableModel_)
            tablesDrawerController->geTypeTableModel_->reloadTable();
		
		if (community)
			community->genomic_element_types_changed_ = false;
	}
	
	if (inInvalidState || community->interaction_types_changed_)
	{
        if (tablesDrawerController && tablesDrawerController->interactionTypeTableModel_)
            tablesDrawerController->interactionTypeTableModel_->reloadTable();
		
		if (community)
			community->interaction_types_changed_ = false;
	}
	
	if (inInvalidState || community->scripts_changed_)
	{
        if (tablesDrawerController && tablesDrawerController->eidosBlockTableModel_)
            tablesDrawerController->eidosBlockTableModel_->reloadTable();
		
		if (community)
			community->scripts_changed_ = false;
	}
	
	if (inInvalidState || community->chromosome_changed_)
	{
        for (QtSLiMChromosomeWidget *overviewWidget : chromosomeOverviewWidgets)
        {
            overviewWidget->restoreLastSelection();
            overviewWidget->update();
        }
        
		if (community)
			community->chromosome_changed_ = false;
	}
	
	// Update graph windows as well; this will usually trigger an update() but may do other updating work as well
	if (fullUpdate)
        emit controllerUpdatedAfterTick();
}

void QtSLiMWindow::updatePlayButtonIcon(bool pressed)
{
    bool highlighted = ui->playButton->isChecked() ^ pressed;
    
    ui->playButton->qtslimSetHighlight(highlighted);
}

void QtSLiMWindow::updateProfileButtonIcon(bool pressed)
{
    bool highlighted = ui->profileButton->isChecked() ^ pressed;
    
    if (profilePlayOn_)
        ui->profileButton->qtslimSetIcon("profile_R", !highlighted);    // flipped intentionally
    else
        ui->profileButton->qtslimSetIcon("profile", highlighted);
}

void QtSLiMWindow::updateRecycleButtonIcon(bool pressed)
{
    if (slimChangeCount)
        ui->recycleButton->qtslimSetIcon("recycle_G", pressed);
    else
        ui->recycleButton->qtslimSetIcon("recycle", pressed);
}

void QtSLiMWindow::updateUIEnabling(void)
{
    // First we update all the UI that belongs exclusively to ourselves: buttons, labels, etc.
    ui->playOneStepButton->setEnabled(!reachedSimulationEnd_ && !continuousPlayOn_);
    ui->playButton->setEnabled(!reachedSimulationEnd_ && !profilePlayOn_);
    ui->profileButton->setEnabled(!reachedSimulationEnd_ && !nonProfilePlayOn_ && !tickPlayOn_);
    ui->recycleButton->setEnabled(!continuousPlayOn_);
    
    ui->playSpeedSlider->setEnabled(!invalidSimulation_);
    
    if (invalidSimulation_)
    {
        // when an error occurs, we want these textfields to have a dimmed/disabled appearance
        ui->tickLineEdit->setAppearance(/* enabled */ false, /* dimmed */ true);
        ui->cycleLineEdit->setAppearance(/* enabled */ false, /* dimmed */ true);
    }
    else
    {
        // otherwise, we want an enabled _appearance_ at all times, but we have to disable them
        // to prevent editing during play; so we set the text color to prevent it from dimming
        // note that the cycle lineedit is always disabled, but follows the appearance of the tick lineedit;
        // the "editable but dimmed" visual appearance is actually a little different so hopefully this is clear
        bool editingAllowed = (!reachedSimulationEnd_ && !continuousPlayOn_);
        
        ui->tickLineEdit->setAppearance(/* enabled */ editingAllowed, /* dimmed */ false);
        ui->cycleLineEdit->setAppearance(/* enabled */ false, /* dimmed */ false);
    }
    
    ui->toggleDrawerButton->setEnabled(true);
    
    ui->clearDebugButton->setEnabled(true);
    ui->checkScriptButton->setEnabled(!continuousPlayOn_);
    ui->prettyprintButton->setEnabled(!continuousPlayOn_);
    ui->scriptHelpButton->setEnabled(true);
    ui->consoleButton->setEnabled(true);
    ui->browserButton->setEnabled(true);
    ui->jumpToPopupButton->setEnabled(true);
    
    ui->chromosomeActionButton->setEnabled(!invalidSimulation_);
    ui->clearOutputButton->setEnabled(!invalidSimulation_);
    ui->dumpPopulationButton->setEnabled(!invalidSimulation_);
    ui->debugOutputButton->setEnabled(true);
    ui->graphPopupButton->setEnabled(!invalidSimulation_);
    ui->changeDirectoryButton->setEnabled(!continuousPlayOn_);
    
    ui->scriptTextEdit->setReadOnly(continuousPlayOn_);
    ui->outputTextEdit->setReadOnly(true);
    
    ui->tickLabel->setEnabled(!invalidSimulation_);
    ui->cycleLabel->setEnabled(!invalidSimulation_);
    ui->outputHeaderLabel->setEnabled(!invalidSimulation_);
    
    // Tell the console controller to enable/disable its buttons
    if (consoleController)
        consoleController->setInterfaceEnabled(!continuousPlayOn_);
    
    // Then, if we are the focused or active window, we update the menus to reflect our state
    // If there's a focused/active window but it isn't us, we reflect that situation with a different method
    // Keep in mind that in Qt each QMainWindow has its own menu bar, its own actions, etc.; this is not global state!
    // This means we spend a little time updating menu enable states that are not visible anyway, but it's fast
    QWidget *currentFocusWidget = qApp->focusWidget();
    QWidget *focusWindow = (currentFocusWidget ? currentFocusWidget->window() : qtSLiMAppDelegate->activeWindow());
    
    if (focusWindow == this) {
        //qDebug() << "updateMenuEnablingACTIVE()";
        updateMenuEnablingACTIVE(currentFocusWidget);
    } else {
        //qDebug() << "updateMenuEnablingINACTIVE()";
        updateMenuEnablingINACTIVE(currentFocusWidget, focusWindow);
    }
}

void QtSLiMWindow::updateMenuEnablingACTIVE(QWidget *p_focusWidget)
{
    // Enable/disable actions (i.e., menu items) when our window is active.  Note that this
    // does not enable/disable buttons; that is done in QtSLiMWindow::updateUIEnabling().
    
    ui->actionClose->setEnabled(true);
    ui->actionSave->setEnabled(true);
    ui->actionSaveAs->setEnabled(true);
    ui->actionRevertToSaved->setEnabled(!isUntitled);
    
    //ui->menuSimulation->setEnabled(true);     // commented out these menu-level enable/disables; they flash weirdly and are distracting
    ui->actionStep->setEnabled(!reachedSimulationEnd_ && !continuousPlayOn_);
    ui->actionPlay->setEnabled(!reachedSimulationEnd_ && !profilePlayOn_);
    ui->actionPlay->setText(nonProfilePlayOn_ ? "Stop" : "Play");
    ui->actionProfile->setEnabled(!reachedSimulationEnd_ && !nonProfilePlayOn_ && !tickPlayOn_);
    ui->actionProfile->setText(profilePlayOn_ ? "Stop" : "Profile");
    ui->actionRecycle->setEnabled(!continuousPlayOn_);
    
    //ui->menuScript->setEnabled(true);
    ui->actionClearDebug->setEnabled(true);
    ui->actionCheckScript->setEnabled(!continuousPlayOn_);
    ui->actionPrettyprintScript->setEnabled(!continuousPlayOn_);
    ui->actionReformatScript->setEnabled(!continuousPlayOn_);
    ui->actionShowScriptHelp->setEnabled(true);
    ui->actionBiggerFont->setEnabled(true);
    ui->actionSmallerFont->setEnabled(true);
    ui->actionShowEidosConsole->setEnabled(true);
    ui->actionShowVariableBrowser->setEnabled(true);
    ui->actionShowDebuggingOutput->setEnabled(true);
    
    ui->actionClearOutput->setEnabled(!invalidSimulation_);
    ui->actionExecuteAll->setEnabled(false);
    ui->actionExecuteSelection->setEnabled(false);
    ui->actionDumpPopulationState->setEnabled(!invalidSimulation_);
    ui->actionChangeWorkingDirectory->setEnabled(!continuousPlayOn_);
    
    // see QtSLiMWindow::graphPopupButtonRunMenu() for parallel code involving the graph popup button
    Species *displaySpecies = focalDisplaySpecies();
    bool graphItemsEnabled = displaySpecies && !invalidSimulation_;
    bool haplotypePlotEnabled = displaySpecies && !continuousPlayOn_ && displaySpecies->population_.subpops_.size();
    
    //ui->menuGraph->setEnabled(graphItemsEnabled);
    ui->actionGraph_1D_Population_SFS->setEnabled(graphItemsEnabled);
	ui->actionGraph_1D_Sample_SFS->setEnabled(graphItemsEnabled);
	ui->actionGraph_2D_Population_SFS->setEnabled(graphItemsEnabled);
	ui->actionGraph_2D_Sample_SFS->setEnabled(graphItemsEnabled);
	ui->actionGraph_Mutation_Frequency_Trajectories->setEnabled(graphItemsEnabled);
	ui->actionGraph_Mutation_Loss_Time_Histogram->setEnabled(graphItemsEnabled);
	ui->actionGraph_Mutation_Fixation_Time_Histogram->setEnabled(graphItemsEnabled);
	ui->actionGraph_Population_Fitness_Distribution->setEnabled(graphItemsEnabled);
	ui->actionGraph_Subpopulation_Fitness_Distributions->setEnabled(graphItemsEnabled);
	ui->actionGraph_Fitness_Time->setEnabled(graphItemsEnabled);
	ui->actionGraph_Age_Distribution->setEnabled(graphItemsEnabled);
	ui->actionGraph_Lifetime_Reproduce_Output->setEnabled(graphItemsEnabled);
	ui->actionGraph_Population_Size_Time->setEnabled(graphItemsEnabled);
	ui->actionGraph_Population_Visualization->setEnabled(graphItemsEnabled);
    ui->actionGraph_Multispecies_Population_Size_Time->setEnabled(!invalidSimulation_);     // displaySpecies not required
	ui->actionCreate_Haplotype_Plot->setEnabled(haplotypePlotEnabled);
    
    updateMenuEnablingSHARED(p_focusWidget);
}

void QtSLiMWindow::updateMenuEnablingINACTIVE(QWidget *p_focusWidget, QWidget *focusWindow)
{
    // Enable/disable actions (i.e., menu items) when our window is inactive.  Note that this
    // does not enable/disable buttons; that is done in QtSLiMWindow::updateUIEnabling().
    
    QWidget *currentActiveWindow = QApplication::activeWindow();
    ui->actionClose->setEnabled(currentActiveWindow ? true : false);
    
    ui->actionSave->setEnabled(false);
    ui->actionSaveAs->setEnabled(false);
    ui->actionRevertToSaved->setEnabled(false);
    
    //ui->menuSimulation->setEnabled(false);
    ui->actionStep->setEnabled(false);
    ui->actionPlay->setEnabled(false);
    ui->actionPlay->setText("Play");
    ui->actionProfile->setEnabled(false);
    ui->actionProfile->setText("Profile");
    ui->actionRecycle->setEnabled(false);
    
    // The script menu state, if we are inactive, is mostly either (a) governed by the front console
    // controller, or (b) is disabled, if a console controller is not active
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    bool consoleFocused = (eidosConsole != nullptr);
    bool consoleFocusedAndEditable = ((eidosConsole != nullptr) && !continuousPlayOn_);
    
    //ui->menuScript->setEnabled(consoleFocused);
    ui->actionCheckScript->setEnabled(consoleFocusedAndEditable);
    ui->actionPrettyprintScript->setEnabled(consoleFocusedAndEditable);
    ui->actionReformatScript->setEnabled(consoleFocusedAndEditable);
    ui->actionClearOutput->setEnabled(consoleFocused);
    ui->actionExecuteAll->setEnabled(consoleFocusedAndEditable);
    ui->actionExecuteSelection->setEnabled(consoleFocusedAndEditable);
    
    // but these menu items apply only to QtSLiMWindow, not to the Eidos console
    ui->actionClearDebug->setEnabled(false);
    ui->actionDumpPopulationState->setEnabled(false);
    ui->actionChangeWorkingDirectory->setEnabled(false);
    
    //ui->menuGraph->setEnabled(false);
    ui->actionGraph_1D_Population_SFS->setEnabled(false);
	ui->actionGraph_1D_Sample_SFS->setEnabled(false);
	ui->actionGraph_2D_Population_SFS->setEnabled(false);
	ui->actionGraph_2D_Sample_SFS->setEnabled(false);
	ui->actionGraph_Mutation_Frequency_Trajectories->setEnabled(false);
	ui->actionGraph_Mutation_Loss_Time_Histogram->setEnabled(false);
	ui->actionGraph_Mutation_Fixation_Time_Histogram->setEnabled(false);
	ui->actionGraph_Population_Fitness_Distribution->setEnabled(false);
	ui->actionGraph_Subpopulation_Fitness_Distributions->setEnabled(false);
	ui->actionGraph_Fitness_Time->setEnabled(false);
	ui->actionGraph_Age_Distribution->setEnabled(false);
	ui->actionGraph_Lifetime_Reproduce_Output->setEnabled(false);
	ui->actionGraph_Population_Size_Time->setEnabled(false);
	ui->actionGraph_Population_Visualization->setEnabled(false);
	ui->actionCreate_Haplotype_Plot->setEnabled(false);
    
    // we can show our various windows as long as we can reach the controller window
    QtSLiMWindow *slimWindow = qtSLiMAppDelegate->dispatchQtSLiMWindowFromSecondaries();
    bool canReachSLiMWindow = !!slimWindow;
    
    ui->actionShowScriptHelp->setEnabled(canReachSLiMWindow);
    ui->actionShowEidosConsole->setEnabled(canReachSLiMWindow);
    ui->actionShowVariableBrowser->setEnabled(canReachSLiMWindow);
    ui->actionShowDebuggingOutput->setEnabled(canReachSLiMWindow);
    
    updateMenuEnablingSHARED(p_focusWidget);
}

void QtSLiMWindow::updateMenuEnablingSHARED(QWidget *p_focusWidget)
{
    // Here we update the enable state for menu items, such as cut/copy/paste, that go to
    // the p_focusWidget whatever window it might be in; "first responder" in Cocoa parlance
    QLineEdit *lE = dynamic_cast<QLineEdit*>(p_focusWidget);
    QTextEdit *tE = dynamic_cast<QTextEdit*>(p_focusWidget);
    QPlainTextEdit *ptE = dynamic_cast<QPlainTextEdit*>(p_focusWidget);
    QtSLiMTextEdit *stE = dynamic_cast<QtSLiMTextEdit *>(tE);
    bool hasEnabledDestination = (lE && lE->isEnabled()) || (tE && tE->isEnabled()) || (ptE && ptE->isEnabled());
    bool hasEnabledModifiableDestination = (lE && lE->isEnabled() && !lE->isReadOnly()) ||
            (tE && tE->isEnabled() && !tE->isReadOnly()) || (ptE && ptE->isEnabled() && !ptE->isReadOnly());
    bool hasUndoableDestination = (lE && lE->isEnabled() && !lE->isReadOnly() && lE->isUndoAvailable()) ||
            (tE && tE->isEnabled() && !tE->isReadOnly() && tE->isUndoRedoEnabled()) ||
            (ptE && ptE->isEnabled() && !ptE->isReadOnly() && ptE->isUndoRedoEnabled());
    bool hasRedoableDestination = (lE && lE->isEnabled() && !lE->isReadOnly() && lE->isRedoAvailable()) ||
            (tE && tE->isEnabled() && !tE->isReadOnly() && tE->isUndoRedoEnabled()) ||
            (ptE && ptE->isEnabled() && !ptE->isReadOnly() && ptE->isUndoRedoEnabled());
    bool hasCopyableDestination = (lE && lE->isEnabled() && lE->selectedText().length()) ||
            (tE && tE->isEnabled()) || (ptE && ptE->isEnabled());
    
    if (stE)
    {
        // refine our assessment of undo/redo/copy capability if possible
        hasUndoableDestination = hasUndoableDestination && stE->qtslimIsUndoAvailable();
        hasRedoableDestination = hasRedoableDestination && stE->qtslimIsRedoAvailable();
        hasCopyableDestination = hasCopyableDestination && stE->qtslimIsCopyAvailable();
    }
    
    ui->actionUndo->setEnabled(hasUndoableDestination);
    ui->actionRedo->setEnabled(hasRedoableDestination);
    ui->actionCut->setEnabled(hasEnabledModifiableDestination);
    ui->actionCopy->setEnabled(hasCopyableDestination);
    ui->actionPaste->setEnabled(hasEnabledModifiableDestination);
    ui->actionDelete->setEnabled(hasEnabledModifiableDestination);
    ui->actionSelectAll->setEnabled(hasEnabledDestination);
    
    ui->actionBiggerFont->setEnabled(true);
    ui->actionSmallerFont->setEnabled(true);
    
    // actions handled by QtSLiMScriptTextEdit only
    QtSLiMScriptTextEdit *scriptEdit = dynamic_cast<QtSLiMScriptTextEdit*>(p_focusWidget);
    bool isModifiableScriptTextEdit = (scriptEdit && !scriptEdit->isReadOnly());
    
    ui->actionShiftLeft->setEnabled(isModifiableScriptTextEdit);
    ui->actionShiftRight->setEnabled(isModifiableScriptTextEdit);
    ui->actionCommentUncomment->setEnabled(isModifiableScriptTextEdit);
    
    // actions handled by the Find panel only
    QtSLiMFindPanel &findPanelInstance = QtSLiMFindPanel::instance();
    bool hasFindTarget = (findPanelInstance.targetTextEditRequireModifiable(false) != nullptr);
    bool hasModifiableFindTarget = (findPanelInstance.targetTextEditRequireModifiable(true) != nullptr);
    
    ui->actionFindShow->setEnabled(true);
    ui->actionFindNext->setEnabled(hasFindTarget);
    ui->actionFindPrevious->setEnabled(hasFindTarget);
    ui->actionReplaceAndFind->setEnabled(hasModifiableFindTarget);
    ui->actionUseSelectionForFind->setEnabled(hasFindTarget);
    ui->actionUseSelectionForReplace->setEnabled(hasFindTarget);
    ui->actionJumpToSelection->setEnabled(hasFindTarget);
    ui->actionJumpToLine->setEnabled(hasFindTarget);
    
    findPanelInstance.fixEnableState();   // give it a chance to update its buttons whenever we update
}

void QtSLiMWindow::updateWindowMenu(void)
{
    // Clear out old actions, up to the separator
    const QList<QAction *> actions = ui->menuWindow->actions();
    
    do
    {
        QAction *lastAction = actions.last();
        
        if (!lastAction)
            break;
        if ((lastAction->objectName().length() == 0) || (lastAction->objectName() == "action"))
            break;
        
        ui->menuWindow->removeAction(lastAction);
    }
    while (true);
    
    // Get the main windows, in sorted order
    const QList<QWidget *> allWidgets = QApplication::allWidgets();
    std::vector<std::pair<std::string, QtSLiMWindow *>> windows;
    
    for (QWidget *widget : allWidgets)
    {
        QtSLiMWindow *mainWin = qobject_cast<QtSLiMWindow *>(widget);
        
        if (mainWin && !mainWin->isZombieWindow_)
        {
            QString title = mainWin->windowTitle();
            
            if (title.endsWith("[*]"))
                title.chop(3);
            
            windows.emplace_back(title.toStdString(), mainWin);
        }
    }
    
    std::sort(windows.begin(), windows.end(), [](const std::pair<std::string, QtSLiMWindow *> &l, const std::pair<std::string, QtSLiMWindow *> &r) { return l.first < r.first; });
    
    // Make new actions
    QWidget *activeWindow = qtSLiMAppDelegate->activeWindow();
    
    for (const auto &pair : windows)
    {
        QString title = QString::fromStdString(pair.first);
        QtSLiMWindow *mainWin = pair.second;
        
        if (mainWin)
        {
            QAction *action = ui->menuWindow->addAction(title, mainWin, [mainWin]() { mainWin->raise(); mainWin->activateWindow(); });
            action->setCheckable(mainWin == activeWindow);  // only set checkable if checked, to avoid the empty checkbox on Ubuntu
            action->setChecked(mainWin == activeWindow);
            action->setObjectName("__QtSLiM_window__");
            
            // Get the subwindows, in sorted order
            std::vector<std::pair<std::string, QWidget *>> subwindows;
            
            for (QWidget *widget : allWidgets)
            {
                QWidget *finalParent = widget->parentWidget();
                
                while (finalParent && (finalParent != mainWin))
                    finalParent = finalParent->parentWidget();
                
                if ((qobject_cast<QtSLiMWindow *>(widget) == nullptr) &&
                        (finalParent == mainWin) &&
                        (widget->isVisible()) &&
                        (((widget->windowFlags() & Qt::Window) == Qt::Window) || ((widget->windowFlags() & Qt::Tool) == Qt::Tool)))
                {
                    QString subwindowTitle = widget->windowTitle();
                    
                    if (subwindowTitle.length())
                    {
                        if (graphViewForGraphWindow(widget))
                            subwindowTitle.prepend("Graph: ");
                        subwindows.emplace_back(subwindowTitle.toStdString(), widget);
                    }
                }
            }
            
            std::sort(subwindows.begin(), subwindows.end(), [](const std::pair<std::string, QWidget *> &l, const std::pair<std::string, QWidget *> &r) { return l.first < r.first; });
            
            // Add indented subitems for windows owned by this main window
            for (const auto &subpair : subwindows)
            {
                QString subwindowTitle = QString::fromStdString(subpair.first);
                QWidget *subwindow = subpair.second;
                
                QAction *subwindowAction = ui->menuWindow->addAction(subwindowTitle.prepend("    "), subwindow, [subwindow]() { subwindow->raise(); subwindow->activateWindow(); });
                subwindowAction->setCheckable(subwindow == activeWindow);  // only set checkable if checked, to avoid the empty checkbox on Ubuntu
                subwindowAction->setChecked(subwindow == activeWindow);
                subwindowAction->setObjectName("__QtSLiM_subwindow__");
            }
        }
    }
}


//
//  profiling
//

#if (SLIMPROFILING == 1)

void QtSLiMWindow::colorScriptWithProfileCountsFromNode(const EidosASTNode *node, double elapsedTime, int32_t baseIndex, QTextDocument *doc, QTextCharFormat &baseFormat)
{
    // First color the range for this node
	eidos_profile_t count = node->profile_total_;
	
	if (count > 0)
	{
		int32_t start = 0, end = 0;
		
		node->FullUTF16Range(&start, &end);
		
		start -= baseIndex;
		end -= baseIndex;
		
		QTextCursor colorCursor(doc);
        colorCursor.setPosition(start);
        colorCursor.setPosition(end + 1, QTextCursor::KeepAnchor);
        
        QColor backgroundColor = slimColorForFraction(Eidos_ElapsedProfileTime(count) / elapsedTime);
		QTextCharFormat colorFormat = baseFormat;
        
        colorFormat.setBackground(backgroundColor);
        colorCursor.setCharFormat(colorFormat);
	}
	
	// Then let child nodes color
	for (const EidosASTNode *child : node->children_)
        colorScriptWithProfileCountsFromNode(child, elapsedTime, baseIndex, doc, baseFormat);
}

void QtSLiMWindow::displayProfileResults(void)
{
    // Make a new window to show the profile results
    QWidget *profile_window = new QWidget(this, Qt::Window);    // the profile window has us as a parent, but is still a standalone window
    QString title = profile_window->windowTitle();
    
    if (title.length() == 0)
        title = "Untitled";
    
    profile_window->setWindowTitle("Profile Report for " + title);
    profile_window->setMinimumSize(500, 200);
    profile_window->resize(500, 600);
    profile_window->move(50, 50);
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    profile_window->setWindowIcon(QIcon());
#endif
    
    // make window actions for all global menu items
    qtSLiMAppDelegate->addActionsForGlobalMenuItems(profile_window);
    
    // Make a QPlainTextEdit to hold the results
    QHBoxLayout *window_layout = new QHBoxLayout;
    QPlainTextEdit *textEdit = new QPlainTextEdit();
    
    profile_window->setLayout(window_layout);
    
    window_layout->setMargin(0);
    window_layout->setSpacing(0);
    window_layout->addWidget(textEdit);
    
    textEdit->setFrameStyle(QFrame::NoFrame);
    textEdit->setReadOnly(true);
    
    // Change the background color for the palette to white (rather than letting it be black when in dark mode)
    QPalette p = textEdit->palette();
    p.setColor(QPalette::Active, QPalette::Base, Qt::white);
    textEdit->setPalette(p);
    textEdit->setBackgroundVisible(false);
    
    // Make the text document that will hold the profile results
    QTextDocument *doc = textEdit->document();
    QTextCursor tc = textEdit->textCursor();
    
    doc->setDocumentMargin(10);
    
    // Choose our fonts; the variable names here are historical, they are not necessarily menlo/optima...
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    QFont menlo11(prefs.displayFontPref());
    qreal displayFontSize = menlo11.pointSizeF();
    qreal scaleFactor = displayFontSize / 11.0;     // The unscaled sizes are geared toward Optima on the Mac
    
#if defined(__linux__)
    // On Linux font sizes seem to run large, who knows why, so reduce the scale factor somewhat to compensate
    scaleFactor *= 0.75;
#endif

    QFont optimaFont;
    {
        // We want a body font of Optima on the Mac; on non-Mac platforms we'll just use the default system font for now
        QFontDatabase fontdb;
        QStringList families = fontdb.families();
        
        // Use filter() to look for matches, since the foundry can be appended after the name (why isn't this easier??)
        if (families.filter("Optima").size() > 0)              // good on Mac
            optimaFont = QFont("Optima", 13);
    }
    
    QFont optima18b(optimaFont);
    QFont optima14b(optimaFont);
    QFont optima13(optimaFont);
    QFont optima13i(optimaFont);
    QFont optima8(optimaFont);
    QFont optima3(optimaFont);
    
    optima18b.setPointSizeF(scaleFactor * 18);
    optima18b.setWeight(QFont::Bold);
    optima14b.setPointSizeF(scaleFactor * 14);
    optima14b.setWeight(QFont::Bold);
    optima13.setPointSizeF(scaleFactor * 13);
    optima13i.setPointSizeF(scaleFactor * 13);
    optima13i.setItalic(true);
    optima8.setPointSizeF(scaleFactor * 8);
    optima3.setPointSizeF(scaleFactor * 3);
    
    // Make the QTextCharFormat objects we will use.  Note that we override the usual foreground/background colors
    // that come from light/dark mode; because we change the background color of text, we want to use a balck-on-white
    // base palette whether we are in light or dark mode, otherwise things get complicated, especially since the user
    // might switch between light/dark after the profile is displayed
    QTextCharFormat optima18b_d, optima14b_d, optima13_d, optima13i_d, optima8_d, optima3_d, menlo11_d;
    
    optima18b_d.setFont(optima18b);
    optima14b_d.setFont(optima14b);
    optima13_d.setFont(optima13);
    optima13i_d.setFont(optima13i);
    optima8_d.setFont(optima8);
    optima3_d.setFont(optima3);
    menlo11_d.setFont(menlo11);
    
    optima18b_d.setBackground(Qt::white);
    optima14b_d.setBackground(Qt::white);
    optima13_d.setBackground(Qt::white);
    optima13i_d.setBackground(Qt::white);
    optima8_d.setBackground(Qt::white);
    optima3_d.setBackground(Qt::white);
    menlo11_d.setBackground(Qt::white);
    
    optima18b_d.setForeground(Qt::black);
    optima14b_d.setForeground(Qt::black);
    optima13_d.setForeground(Qt::black);
    optima13i_d.setForeground(Qt::black);
    optima8_d.setForeground(Qt::black);
    optima3_d.setForeground(Qt::black);
    menlo11_d.setForeground(Qt::black);
    
    // Adjust the tab width to the monospace font we have chosen
    double tabWidth = 0;
    QFontMetricsF fm(menlo11);
    
#if (QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
    tabWidth = fm.width("   ");                // deprecated in 5.11
#else
    tabWidth = fm.horizontalAdvance("   ");    // added in Qt 5.11
#endif
    
#if (QT_VERSION < QT_VERSION_CHECK(5, 10, 0))
    textEdit->setTabStopWidth((int)floor(tabWidth));      // deprecated in 5.10
#else
    textEdit->setTabStopDistance(tabWidth);               // added in 5.10
#endif
    
    // Build the report attributed string
    QDateTime profileStartDate = QDateTime::fromSecsSinceEpoch(community->profile_start_date);
	QDateTime profileEndDate = QDateTime::fromSecsSinceEpoch(community->profile_end_date);
	
    QString startDateString = profileStartDate.toString("M/d/yy, h:mm:ss AP");
    QString endDateString = profileEndDate.toString("M/d/yy, h:mm:ss AP");
	double elapsedWallClockTime = (std::chrono::duration_cast<std::chrono::microseconds>(community->profile_end_clock - community->profile_start_clock).count()) / (double)1000000L;
    double elapsedCPUTimeInSLiM = community->profile_elapsed_CPU_clock / static_cast<double>(CLOCKS_PER_SEC);
	double elapsedWallClockTimeInSLiM = Eidos_ElapsedProfileTime(community->profile_elapsed_wall_clock);
	slim_tick_t elapsedSLiMTicks = community->profile_end_tick - community->profile_start_tick;
    
    tc.insertText("Profile Report\n", optima18b_d);
    tc.insertText(" \n", optima3_d);
    
    tc.insertText("Model: " + title + "\n", optima13_d);
    tc.insertText(" \n", optima8_d);
    
    tc.insertText("Run start: " + startDateString + "\n", optima13_d);
    tc.insertText("Run end: " + endDateString + "\n", optima13_d);
    tc.insertText(" \n", optima8_d);
    
#ifdef _OPENMP
    tc.insertText(QString("Maximum parallel threads: %1\n").arg(gEidosMaxThreads), optima13_d);
    tc.insertText(" \n", optima8_d);
#endif
    
    tc.insertText(QString("Elapsed wall clock time: %1 s\n").arg(elapsedWallClockTime, 0, 'f', 2), optima13_d);
    tc.insertText(QString("Elapsed wall clock time inside SLiM core (corrected): %1 s\n").arg(elapsedWallClockTimeInSLiM, 0, 'f', 2), optima13_d);
    tc.insertText(QString("Elapsed CPU time inside SLiM core (uncorrected): %1 s\n").arg(elapsedCPUTimeInSLiM, 0, 'f', 2), optima13_d);
    tc.insertText(QString("Elapsed ticks: %1%2\n").arg(elapsedSLiMTicks).arg((community->profile_start_tick == 0) ? " (including initialize)" : ""), optima13_d);
    tc.insertText(" \n", optima8_d);
    
    tc.insertText(QString("Profile block external overhead: %1 ticks (%2 s)\n").arg(gEidos_ProfileOverheadTicks, 0, 'f', 2).arg(gEidos_ProfileOverheadSeconds, 0, 'g', 4), optima13_d);
    tc.insertText(QString("Profile block internal lag: %1 ticks (%2 s)\n").arg(gEidos_ProfileLagTicks, 0, 'f', 2).arg(gEidos_ProfileLagSeconds, 0, 'g', 4), optima13_d);
    tc.insertText(" \n", optima8_d);
    
    size_t total_usage = community->profile_total_memory_usage_Community.totalMemoryUsage + community->profile_total_memory_usage_AllSpecies.totalMemoryUsage;
	size_t average_usage = total_usage / community->total_memory_tallies_;
	size_t last_usage = community->profile_last_memory_usage_Community.totalMemoryUsage + community->profile_last_memory_usage_AllSpecies.totalMemoryUsage;
	
    tc.insertText(QString("Average tick SLiM memory use: %1\n").arg(stringForByteCount(average_usage)), optima13_d);
    tc.insertText(QString("Final tick SLiM memory use: %1\n").arg(stringForByteCount(last_usage)), optima13_d);
    
	//
	//	Cycle stage breakdown
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		bool isWF = (community->ModelType() == SLiMModelType::kModelTypeWF);
		double elapsedStage0Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[0]);
		double elapsedStage1Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[1]);
		double elapsedStage2Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[2]);
		double elapsedStage3Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[3]);
		double elapsedStage4Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[4]);
		double elapsedStage5Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[5]);
		double elapsedStage6Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[6]);
        double elapsedStage7Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[7]);
        double elapsedStage8Time = Eidos_ElapsedProfileTime(community->profile_stage_totals_[8]);
		double percentStage0 = (elapsedStage0Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage1 = (elapsedStage1Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage2 = (elapsedStage2Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage3 = (elapsedStage3Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage4 = (elapsedStage4Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage5 = (elapsedStage5Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage6 = (elapsedStage6Time / elapsedWallClockTimeInSLiM) * 100.0;
        double percentStage7 = (elapsedStage7Time / elapsedWallClockTimeInSLiM) * 100.0;
        double percentStage8 = (elapsedStage8Time / elapsedWallClockTimeInSLiM) * 100.0;
		int fw = 4;
		
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage0Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage1Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage2Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage3Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage4Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage5Time));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage6Time));
        fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage7Time));
        fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedStage8Time));
		
		tc.insertText(" \n", optima13_d);
		tc.insertText("Cycle stage breakdown\n", optima14b_d);
		tc.insertText(" \n", optima3_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage0Time, fw, 'f', 2).arg(percentStage0, 5, 'f', 2), menlo11_d);
		tc.insertText(" : initialize() callback execution\n", optima13_d);
		
        tc.insertText(QString("%1 s (%2%)").arg(elapsedStage1Time, fw, 'f', 2).arg(percentStage1, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 0 – first() event execution\n" : " : stage 0 – first() event execution\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage2Time, fw, 'f', 2).arg(percentStage2, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 1 – early() event execution\n" : " : stage 1 – offspring generation\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage3Time, fw, 'f', 2).arg(percentStage3, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 2 – offspring generation\n" : " : stage 2 – early() event execution\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage4Time, fw, 'f', 2).arg(percentStage4, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 3 – bookkeeping (fixed mutation removal, etc.)\n" : " : stage 3 – fitness calculation\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage5Time, fw, 'f', 2).arg(percentStage5, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 4 – generation swap\n" : " : stage 4 – viability/survival selection\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage6Time, fw, 'f', 2).arg(percentStage6, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 5 – late() event execution\n" : " : stage 5 – bookkeeping (fixed mutation removal, etc.)\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage7Time, fw, 'f', 2).arg(percentStage7, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 6 – fitness calculation\n" : " : stage 6 – late() event execution\n"), optima13_d);
        
        tc.insertText(QString("%1 s (%2%)").arg(elapsedStage8Time, fw, 'f', 2).arg(percentStage8, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 7 – tree sequence auto-simplification\n" : " : stage 7 – tree sequence auto-simplification\n"), optima13_d);
	}
	
	//
	//	Callback type breakdown
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
        double elapsedTime_first = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosEventFirst]);
		double elapsedTime_early = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosEventEarly]);
		double elapsedTime_late = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosEventLate]);
		double elapsedTime_initialize = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosInitializeCallback]);
		double elapsedTime_mutationEffect = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosMutationEffectCallback]);
		double elapsedTime_fitnessEffect = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosFitnessEffectCallback]);
		double elapsedTime_interaction = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosInteractionCallback]);
		double elapsedTime_matechoice = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosMateChoiceCallback]);
		double elapsedTime_modifychild = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosModifyChildCallback]);
		double elapsedTime_recombination = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosRecombinationCallback]);
		double elapsedTime_mutation = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosMutationCallback]);
		double elapsedTime_reproduction = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosReproductionCallback]);
        double elapsedTime_survival = Eidos_ElapsedProfileTime(community->profile_callback_totals_[(int)SLiMEidosBlockType::SLiMEidosSurvivalCallback]);
		double percent_first = (elapsedTime_first / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_early = (elapsedTime_early / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_late = (elapsedTime_late / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_initialize = (elapsedTime_initialize / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_fitness = (elapsedTime_mutationEffect / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_fitnessglobal = (elapsedTime_fitnessEffect / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_interaction = (elapsedTime_interaction / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_matechoice = (elapsedTime_matechoice / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_modifychild = (elapsedTime_modifychild / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_recombination = (elapsedTime_recombination / elapsedWallClockTimeInSLiM) * 100.0;
		double percent_mutation = (elapsedTime_mutation / elapsedWallClockTimeInSLiM) * 100.0;
        double percent_reproduction = (elapsedTime_reproduction / elapsedWallClockTimeInSLiM) * 100.0;
        double percent_survival = (elapsedTime_survival / elapsedWallClockTimeInSLiM) * 100.0;
		int fw = 4, fw2 = 4;
		
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_first));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_early));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_late));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_initialize));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_mutationEffect));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_fitnessEffect));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_interaction));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_matechoice));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_modifychild));
		fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_recombination));
        fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_mutation));
        fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_reproduction));
        fw = std::max(fw, 3 + DisplayDigitsForIntegerPart(elapsedTime_survival));
		
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_first));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_early));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_late));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_initialize));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_fitness));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_fitnessglobal));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_interaction));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_matechoice));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_modifychild));
		fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_recombination));
        fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_mutation));
        fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_reproduction));
        fw2 = std::max(fw2, 3 + DisplayDigitsForIntegerPart(percent_survival));
		
		tc.insertText(" \n", optima13_d);
		tc.insertText("Callback type breakdown\n", optima14b_d);
		tc.insertText(" \n", optima3_d);
		
		// Note these are out of numeric order, but in cycle stage order
		if (community->ModelType() == SLiMModelType::kModelTypeWF)
		{
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_initialize, fw, 'f', 2).arg(percent_initialize, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : initialize() callbacks\n", optima13_d);
			
            tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_first, fw, 'f', 2).arg(percent_first, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : first() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_early, fw, 'f', 2).arg(percent_early, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : early() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_matechoice, fw, 'f', 2).arg(percent_matechoice, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : mateChoice() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_recombination, fw, 'f', 2).arg(percent_recombination, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : recombination() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_mutation, fw, 'f', 2).arg(percent_mutation, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : mutation() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_modifychild, fw, 'f', 2).arg(percent_modifychild, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : modifyChild() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_late, fw, 'f', 2).arg(percent_late, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : late() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_mutationEffect, fw, 'f', 2).arg(percent_fitness, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : mutationEffect() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_fitnessEffect, fw, 'f', 2).arg(percent_fitnessglobal, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : fitnessEffect() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_interaction, fw, 'f', 2).arg(percent_interaction, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : interaction() callbacks\n", optima13_d);
		}
		else
		{
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_initialize, fw, 'f', 2).arg(percent_initialize, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : initialize() callbacks\n", optima13_d);
			
            tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_first, fw, 'f', 2).arg(percent_first, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : first() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_reproduction, fw, 'f', 2).arg(percent_reproduction, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : reproduction() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_recombination, fw, 'f', 2).arg(percent_recombination, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : recombination() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_mutation, fw, 'f', 2).arg(percent_mutation, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : mutation() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_modifychild, fw, 'f', 2).arg(percent_modifychild, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : modifyChild() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_early, fw, 'f', 2).arg(percent_early, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : early() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_mutationEffect, fw, 'f', 2).arg(percent_fitness, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : mutationEffect() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_fitnessEffect, fw, 'f', 2).arg(percent_fitnessglobal, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : fitnessEffect() callbacks\n", optima13_d);
			
            tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_survival, fw, 'f', 2).arg(percent_survival, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : survival() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_late, fw, 'f', 2).arg(percent_late, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : late() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedTime_interaction, fw, 'f', 2).arg(percent_interaction, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : interaction() callbacks\n", optima13_d);
		}
	}
	
	//
	//	Script block profiles
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		{
			std::vector<SLiMEidosBlock*> &script_blocks = community->AllScriptBlocks();
			
			// Convert the profile counts in all script blocks into self counts (excluding the counts of nodes below them)
			for (SLiMEidosBlock *script_block : script_blocks)
				if (script_block->type_ != SLiMEidosBlockType::SLiMEidosUserDefinedFunction)		// exclude function blocks; not user-visible
					script_block->root_node_->ConvertProfileTotalsToSelfCounts();
		}
		{
			tc.insertText(" \n", optima13_d);
			tc.insertText("Script block profiles (as a fraction of corrected wall clock time)\n", optima14b_d);
			tc.insertText(" \n", optima3_d);
			
			std::vector<SLiMEidosBlock*> &script_blocks = community->AllScriptBlocks();
			bool firstBlock = true, hiddenInconsequentialBlocks = false;
			
			for (SLiMEidosBlock *script_block : script_blocks)
			{
				if (script_block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
					continue;
				
				const EidosASTNode *profile_root = script_block->root_node_;
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					if (!firstBlock)
						tc.insertText(" \n \n", menlo11_d);
					firstBlock = false;
					
					const std::string &script_std_string = profile_root->token_->token_string_;
					QString script_string = QString::fromStdString(script_std_string);
					
					tc.insertText(QString("%1 s (%2%):\n").arg(total_block_time, 0, 'f', 2).arg(percent_block_time, 0, 'f', 2), menlo11_d);
					tc.insertText(" \n", optima3_d);
					
                    int colorBase = tc.position();
                    tc.insertText(script_string, menlo11_d);
                    colorScriptWithProfileCountsFromNode(profile_root, elapsedWallClockTimeInSLiM, profile_root->token_->token_UTF16_start_ - colorBase, doc, menlo11_d);
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
			{
				tc.insertText(" \n", menlo11_d);
				tc.insertText(" \n", optima3_d);
				tc.insertText("(blocks using < 0.01 s and < 0.01% of total wall clock time are not shown)", optima13i_d);
			}
		}
		{
			tc.insertText(" \n", menlo11_d);
			tc.insertText(" \n", optima13_d);
			tc.insertText("Script block profiles (as a fraction of within-block wall clock time)\n", optima14b_d);
			tc.insertText(" \n", optima3_d);
			
			std::vector<SLiMEidosBlock*> &script_blocks = community->AllScriptBlocks();
			bool firstBlock = true, hiddenInconsequentialBlocks = false;
			
			for (SLiMEidosBlock *script_block : script_blocks)
			{
				if (script_block->type_ == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
					continue;
				
				const EidosASTNode *profile_root = script_block->root_node_;
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					if (!firstBlock)
						tc.insertText(" \n \n", menlo11_d);
					firstBlock = false;
					
					const std::string &script_std_string = profile_root->token_->token_string_;
                    QString script_string = QString::fromStdString(script_std_string);
					
					tc.insertText(QString("%1 s (%2%):\n").arg(total_block_time, 0, 'f', 2).arg(percent_block_time, 0, 'f', 2), menlo11_d);
					tc.insertText(" \n", optima3_d);
					
                    int colorBase = tc.position();
                    tc.insertText(script_string, menlo11_d);
                    if (total_block_time > 0.0)
                        colorScriptWithProfileCountsFromNode(profile_root, total_block_time, profile_root->token_->token_UTF16_start_ - colorBase, doc, menlo11_d);
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
			{
				tc.insertText(" \n", menlo11_d);
				tc.insertText(" \n", optima3_d);
				tc.insertText("(blocks using < 0.01 s and < 0.01% of total wall clock time are not shown)", optima13i_d);
			}
		}
	}
	
	//
	//	User-defined functions (if any)
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		EidosFunctionMap &function_map = community->FunctionMap();
		std::vector<const EidosFunctionSignature *> userDefinedFunctions;
		
		for (const auto &functionPairIter : function_map)
		{
			const EidosFunctionSignature *signature = functionPairIter.second.get();
			
			if (signature->body_script_ && signature->user_defined_)
			{
				signature->body_script_->AST()->ConvertProfileTotalsToSelfCounts();
				userDefinedFunctions.emplace_back(signature);
			}
		}
		
		if (userDefinedFunctions.size())
		{
			tc.insertText(" \n", menlo11_d);
			tc.insertText(" \n", optima13_d);
			tc.insertText("User-defined functions (as a fraction of corrected wall clock time)\n", optima14b_d);
			tc.insertText(" \n", optima3_d);
			
			bool firstBlock = true, hiddenInconsequentialBlocks = false;
			
			for (const EidosFunctionSignature *signature : userDefinedFunctions)
			{
				const EidosASTNode *profile_root = signature->body_script_->AST();
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					if (!firstBlock)
						tc.insertText(" \n \n", menlo11_d);
					firstBlock = false;
					
					const std::string &script_std_string = profile_root->token_->token_string_;
					QString script_string = QString::fromStdString(script_std_string);
					const std::string &&signature_string = signature->SignatureString();
					QString signatureString = QString::fromStdString(signature_string);
					
					tc.insertText(QString("%1 s (%2%):\n").arg(total_block_time, 0, 'f', 2).arg(percent_block_time, 0, 'f', 2), menlo11_d);
					tc.insertText(" \n", optima3_d);
					tc.insertText(signatureString + "\n", menlo11_d);
					
                    int colorBase = tc.position();
                    tc.insertText(script_string, menlo11_d);
                    colorScriptWithProfileCountsFromNode(profile_root, elapsedWallClockTimeInSLiM, profile_root->token_->token_UTF16_start_ - colorBase, doc, menlo11_d);
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
			{
				tc.insertText(" \n", menlo11_d);
				tc.insertText(" \n", optima3_d);
				tc.insertText("(functions using < 0.01 s and < 0.01% of total wall clock time are not shown)", optima13i_d);
			}
		}
		if (userDefinedFunctions.size())
		{
			tc.insertText(" \n", menlo11_d);
			tc.insertText(" \n", optima13_d);
			tc.insertText("User-defined functions (as a fraction of within-block wall clock time)\n", optima14b_d);
			tc.insertText(" \n", optima3_d);
			
			bool firstBlock = true, hiddenInconsequentialBlocks = false;
			
			for (const EidosFunctionSignature *signature : userDefinedFunctions)
			{
				const EidosASTNode *profile_root = signature->body_script_->AST();
				double total_block_time = Eidos_ElapsedProfileTime(profile_root->TotalOfSelfCounts());	// relies on ConvertProfileTotalsToSelfCounts() being called above!
				double percent_block_time = (total_block_time / elapsedWallClockTimeInSLiM) * 100.0;
				
				if ((total_block_time >= 0.01) || (percent_block_time >= 0.01))
				{
					if (!firstBlock)
						tc.insertText(" \n \n", menlo11_d);
					firstBlock = false;
					
					const std::string &script_std_string = profile_root->token_->token_string_;
					QString script_string = QString::fromStdString(script_std_string);
					const std::string &&signature_string = signature->SignatureString();
					QString signatureString = QString::fromStdString(signature_string);
					
					tc.insertText(QString("%1 s (%2%):\n").arg(total_block_time, 0, 'f', 2).arg(percent_block_time, 0, 'f', 2), menlo11_d);
					tc.insertText(" \n", optima3_d);
					tc.insertText(signatureString + "\n", menlo11_d);
					
                    int colorBase = tc.position();
                    tc.insertText(script_string, menlo11_d);
                    if (total_block_time > 0.0)
                        colorScriptWithProfileCountsFromNode(profile_root, total_block_time, profile_root->token_->token_UTF16_start_ - colorBase, doc, menlo11_d);
				}
				else
					hiddenInconsequentialBlocks = true;
			}
			
			if (hiddenInconsequentialBlocks)
			{
				tc.insertText(" \n", menlo11_d);
				tc.insertText(" \n", optima3_d);
				tc.insertText("(functions using < 0.01 s and < 0.01% of total wall clock time are not shown)", optima13i_d);
			}
		}
	}
	
#if SLIM_USE_NONNEUTRAL_CACHES
	//
	//	MutationRun metrics, presented per Species
	//
    for (Species *focal_species : community->all_species_)
	{
        tc.insertText(" \n", menlo11_d);
		tc.insertText(" \n", optima13_d);
		tc.insertText("MutationRun usage", optima14b_d);
        if (community->all_species_.size() > 1)
        {
            tc.insertText(" (", optima14b_d);
            tc.insertText(QString::fromStdString(focal_species->avatar_), optima14b_d);
            tc.insertText(" ", optima14b_d);
            tc.insertText(QString::fromStdString(focal_species->name_), optima14b_d);
            tc.insertText(")", optima14b_d);
        }
        tc.insertText("\n", optima14b_d);
		tc.insertText(" \n", optima3_d);
		
        if (!focal_species->HasGenetics())
        {
            tc.insertText("(omitted for no-genetics species)", optima13i_d);
            continue;
        }
        
		int64_t power_tallies[20];	// we only go up to 1024 mutruns right now, but this gives us some headroom
		int64_t power_tallies_total = static_cast<int>(focal_species->profile_mutcount_history_.size());
		
		for (int power = 0; power < 20; ++power)
			power_tallies[power] = 0;
		
		for (int32_t count : focal_species->profile_mutcount_history_)
		{
			int power = static_cast<int>(round(log2(count)));
			
			power_tallies[power]++;
		}
		
		for (int power = 0; power < 20; ++power)
		{
			if (power_tallies[power] > 0)
			{
				tc.insertText(QString("%1%").arg((power_tallies[power] / static_cast<double>(power_tallies_total)) * 100.0, 6, 'f', 2), menlo11_d);
				tc.insertText(QString(" of ticks : %1 mutation runs per genome\n").arg(static_cast<int>(round(pow(2.0, power)))), optima13_d);
			}
		}
		
		
		int64_t regime_tallies[3];
		int64_t regime_tallies_total = static_cast<int>(focal_species->profile_nonneutral_regime_history_.size());
		
		for (int regime = 0; regime < 3; ++regime)
			regime_tallies[regime] = 0;
		
		for (int32_t regime : focal_species->profile_nonneutral_regime_history_)
			if ((regime >= 1) && (regime <= 3))
				regime_tallies[regime - 1]++;
			else
				regime_tallies_total--;
		
		tc.insertText(" \n", optima13_d);
		
		for (int regime = 0; regime < 3; ++regime)
		{
			tc.insertText(QString("%1%").arg((regime_tallies[regime] / static_cast<double>(regime_tallies_total)) * 100.0, 6, 'f', 2), menlo11_d);
			tc.insertText(QString(" of ticks : regime %1 (%2)\n").arg(regime + 1).arg(regime == 0 ? "no mutationEffect() callbacks" : (regime == 1 ? "constant neutral mutationEffect() callbacks only" : "unpredictable mutationEffect() callbacks present")), optima13_d);
		}
		
		
		tc.insertText(" \n", optima13_d);
		
		tc.insertText(QString("%1").arg(focal_species->profile_mutation_total_usage_), menlo11_d);
		tc.insertText(" mutations referenced, summed across all ticks\n", optima13_d);
		
		tc.insertText(QString("%1").arg(focal_species->profile_nonneutral_mutation_total_), menlo11_d);
		tc.insertText(" mutations considered potentially nonneutral\n", optima13_d);
		
		tc.insertText(QString("%1%").arg(((focal_species->profile_mutation_total_usage_ - focal_species->profile_nonneutral_mutation_total_) / static_cast<double>(focal_species->profile_mutation_total_usage_)) * 100.0, 0, 'f', 2), menlo11_d);
		tc.insertText(" of mutations excluded from fitness calculations\n", optima13_d);
		
		tc.insertText(QString("%1").arg(focal_species->profile_max_mutation_index_), menlo11_d);
		tc.insertText(" maximum simultaneous mutations\n", optima13_d);
		
		
		tc.insertText(" \n", optima13_d);
		
		tc.insertText(QString("%1").arg(focal_species->profile_mutrun_total_usage_), menlo11_d);
		tc.insertText(" mutation runs referenced, summed across all ticks\n", optima13_d);
		
		tc.insertText(QString("%1").arg(focal_species->profile_unique_mutrun_total_), menlo11_d);
		tc.insertText(" unique mutation runs maintained among those\n", optima13_d);
		
		tc.insertText(QString("%1%").arg((focal_species->profile_mutrun_nonneutral_recache_total_ / static_cast<double>(focal_species->profile_unique_mutrun_total_)) * 100.0, 6, 'f', 2), menlo11_d);
		tc.insertText(" of mutation run nonneutral caches rebuilt per tick\n", optima13_d);
		
		tc.insertText(QString("%1%").arg(((focal_species->profile_mutrun_total_usage_ - focal_species->profile_unique_mutrun_total_) / static_cast<double>(focal_species->profile_mutrun_total_usage_)) * 100.0, 6, 'f', 2), menlo11_d);
		tc.insertText(" of mutation runs shared among genomes", optima13_d);
	}
#endif
	
	{
		//
		//	Memory usage metrics
		//
        SLiMMemoryUsage_Community &mem_tot_C = community->profile_total_memory_usage_Community;
		SLiMMemoryUsage_Species &mem_tot_S = community->profile_total_memory_usage_AllSpecies;
		SLiMMemoryUsage_Community &mem_last_C = community->profile_last_memory_usage_Community;
		SLiMMemoryUsage_Species &mem_last_S = community->profile_last_memory_usage_AllSpecies;
		uint64_t div = static_cast<uint64_t>(community->total_memory_tallies_);
		double ddiv = community->total_memory_tallies_;
        double average_total = (mem_tot_C.totalMemoryUsage + mem_tot_S.totalMemoryUsage) / ddiv;
		double final_total = mem_last_C.totalMemoryUsage + mem_last_S.totalMemoryUsage;
		
		tc.insertText(" \n", menlo11_d);
		tc.insertText(" \n", optima13_d);
		tc.insertText("SLiM memory usage (average / final tick)\n", optima14b_d);
		tc.insertText(" \n", optima3_d);
		
        QTextCharFormat colored_menlo = menlo11_d;
        
		// Chromosome
		tc.insertText(attributedStringForByteCount(mem_tot_S.chromosomeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.chromosomeObjects, final_total, colored_menlo), colored_menlo);
        tc.insertText(QString(" : Chromosome objects (%1 / %2)\n").arg(mem_tot_S.chromosomeObjects_count / ddiv, 0, 'f', 2).arg(mem_last_S.chromosomeObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.chromosomeMutationRateMaps / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.chromosomeMutationRateMaps, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : mutation rate maps\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.chromosomeRecombinationRateMaps / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.chromosomeRecombinationRateMaps, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : recombination rate maps\n", optima13_d);

		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.chromosomeAncestralSequence / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.chromosomeAncestralSequence, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : ancestral nucleotides\n", optima13_d);
		
        // Community
        tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot_C.communityObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_C.communityObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : Community object\n", optima13_d);
		
		// Genome
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.genomeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.genomeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Genome objects (%1 / %2)\n").arg(mem_tot_S.genomeObjects_count / ddiv, 0, 'f', 2).arg(mem_last_S.genomeObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.genomeExternalBuffers / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.genomeExternalBuffers, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : external MutationRun* buffers\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.genomeUnusedPoolSpace / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.genomeUnusedPoolSpace, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool space\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.genomeUnusedPoolBuffers / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.genomeUnusedPoolBuffers, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool buffers\n", optima13_d);
		
		// GenomicElement
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.genomicElementObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.genomicElementObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : GenomicElement objects (%1 / %2)\n").arg(mem_tot_S.genomicElementObjects_count / ddiv, 0, 'f', 2).arg(mem_last_S.genomicElementObjects_count), optima13_d);
		
		// GenomicElementType
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.genomicElementTypeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.genomicElementTypeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : GenomicElementType objects (%1 / %2)\n").arg(mem_tot_S.genomicElementTypeObjects_count / ddiv, 0, 'f', 2).arg(mem_last_S.genomicElementTypeObjects_count), optima13_d);
		
		// Individual
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.individualObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.individualObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Individual objects (%1 / %2)\n").arg(mem_tot_S.individualObjects_count / ddiv, 0, 'f', 2).arg(mem_last_S.individualObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.individualUnusedPoolSpace / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.individualUnusedPoolSpace, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool space\n", optima13_d);
		
		// InteractionType
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot_C.interactionTypeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_C.interactionTypeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : InteractionType objects (%1 / %2)\n").arg(mem_tot_C.interactionTypeObjects_count / ddiv, 0, 'f', 2).arg(mem_last_C.interactionTypeObjects_count), optima13_d);
		
		if (mem_tot_C.interactionTypeObjects_count || mem_last_C.interactionTypeObjects_count)
		{
			tc.insertText("   ", menlo11_d);
			tc.insertText(attributedStringForByteCount(mem_tot_C.interactionTypeKDTrees / div, average_total, colored_menlo), colored_menlo);
			tc.insertText(" / ", optima13_d);
			tc.insertText(attributedStringForByteCount(mem_last_C.interactionTypeKDTrees, final_total, colored_menlo), colored_menlo);
			tc.insertText(" : k-d trees\n", optima13_d);
			
			tc.insertText("   ", menlo11_d);
			tc.insertText(attributedStringForByteCount(mem_tot_C.interactionTypePositionCaches / div, average_total, colored_menlo), colored_menlo);
			tc.insertText(" / ", optima13_d);
			tc.insertText(attributedStringForByteCount(mem_last_C.interactionTypePositionCaches, final_total, colored_menlo), colored_menlo);
			tc.insertText(" : position caches\n", optima13_d);
			
			tc.insertText("   ", menlo11_d);
			tc.insertText(attributedStringForByteCount(mem_tot_C.interactionTypeSparseVectorPool / div, average_total, colored_menlo), colored_menlo);
			tc.insertText(" / ", optima13_d);
			tc.insertText(attributedStringForByteCount(mem_last_C.interactionTypeSparseVectorPool, final_total, colored_menlo), colored_menlo);
			tc.insertText(" : sparse arrays\n", optima13_d);
		}
		
		// Mutation
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.mutationObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.mutationObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Mutation objects (%1 / %2)\n").arg(mem_tot_S.mutationObjects_count / ddiv, 0, 'f', 2).arg(mem_last_S.mutationObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_C.mutationRefcountBuffer / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_C.mutationRefcountBuffer, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : refcount buffer\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_C.mutationUnusedPoolSpace / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_C.mutationUnusedPoolSpace, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool space\n", optima13_d);
		
		// MutationRun
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.mutationRunObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.mutationRunObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : MutationRun objects (%1 / %2)\n").arg(mem_tot_S.mutationRunObjects_count / ddiv, 0, 'f', 2).arg(mem_last_S.mutationRunObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.mutationRunExternalBuffers / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.mutationRunExternalBuffers, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : external MutationIndex buffers\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.mutationRunNonneutralCaches / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.mutationRunNonneutralCaches, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : nonneutral mutation caches\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.mutationRunUnusedPoolSpace / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.mutationRunUnusedPoolSpace, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool space\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.mutationRunUnusedPoolBuffers / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.mutationRunUnusedPoolBuffers, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool buffers\n", optima13_d);
		
		// MutationType
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.mutationTypeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.mutationTypeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : MutationType objects (%1 / %2)\n").arg(mem_tot_S.mutationTypeObjects_count / ddiv, 0, 'f', 2).arg(mem_last_S.mutationTypeObjects_count), optima13_d);
		
		// Species
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.speciesObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.speciesObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : Species objects\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.speciesTreeSeqTables / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.speciesTreeSeqTables, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : tree-sequence tables\n", optima13_d);
		
		// Subpopulation
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.subpopulationObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.subpopulationObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Subpopulation objects (%1 / %2)\n").arg(mem_tot_S.subpopulationObjects_count / ddiv, 0, 'f', 2).arg(mem_last_S.subpopulationObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.subpopulationFitnessCaches / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.subpopulationFitnessCaches, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : fitness caches\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.subpopulationParentTables / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.subpopulationParentTables, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : parent tables\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.subpopulationSpatialMaps / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.subpopulationSpatialMaps, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : spatial maps\n", optima13_d);
		
		if (mem_tot_S.subpopulationSpatialMapsDisplay || mem_last_S.subpopulationSpatialMapsDisplay)
		{
			tc.insertText("   ", menlo11_d);
			tc.insertText(attributedStringForByteCount(mem_tot_S.subpopulationSpatialMapsDisplay / div, average_total, colored_menlo), colored_menlo);
			tc.insertText(" / ", optima13_d);
			tc.insertText(attributedStringForByteCount(mem_last_S.subpopulationSpatialMapsDisplay, final_total, colored_menlo), colored_menlo);
			tc.insertText(" : spatial map display (SLiMgui only)\n", optima13_d);
		}
		
		// Substitution
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot_S.substitutionObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_S.substitutionObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Substitution objects (%1 / %2)\n").arg(mem_tot_S.substitutionObjects_count / ddiv, 0, 'f', 2).arg(mem_last_S.substitutionObjects_count), optima13_d);
		
		// Eidos
		tc.insertText(" \n", optima8_d);
		tc.insertText("Eidos:\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_C.eidosASTNodePool / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_C.eidosASTNodePool, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : EidosASTNode pool\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_C.eidosSymbolTablePool / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_C.eidosSymbolTablePool, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : EidosSymbolTable pool\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot_C.eidosValuePool / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last_C.eidosValuePool, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : EidosValue pool\n", optima13_d);
        
        tc.insertText("   ", menlo11_d);
        tc.insertText(attributedStringForByteCount(mem_tot_C.fileBuffers / div, average_total, colored_menlo), colored_menlo);
        tc.insertText(" / ", optima13_d);
        tc.insertText(attributedStringForByteCount(mem_last_C.fileBuffers, final_total, colored_menlo), colored_menlo);
        tc.insertText(" : File buffers", optima13_d);
    }
    
    // Done, show the window
    tc.setPosition(0);
    textEdit->setTextCursor(tc);
    profile_window->show();    
}

#endif	// (SLIMPROFILING == 1)


//
//  simulation play mechanics
//

void QtSLiMWindow::willExecuteScript(void)
{
    // Whenever we are about to execute script, we swap in our random number generator; at other times, gEidos_rng is NULL.
    // The goal here is to keep each SLiM window independent in its random number sequence.
    if (gEidos_RNG_Initialized)
        qDebug() << "eidosConsoleWindowControllerWillExecuteScript: gEidos_rng already set up!";

	std::swap(sim_RNG, gEidos_RNG_SINGLE);
	std::swap(sim_RNG_initialized, gEidos_RNG_Initialized);

    // We also swap in the pedigree id and mutation id counters; each SLiMgui window is independent
    gSLiM_next_pedigree_id = sim_next_pedigree_id;
    gSLiM_next_mutation_id = sim_next_mutation_id;
    gEidosSuppressWarnings = sim_suppress_warnings;

    // Set the current directory to its value for this window
    errno = 0;
    int retval = chdir(sim_working_dir.c_str());

    if (retval == -1)
        qDebug() << "willExecuteScript: Unable to set the working directory to " << sim_working_dir.c_str() << " (error " << errno << ")";
}

void QtSLiMWindow::didExecuteScript(void)
{
    // Swap our random number generator back out again; see -eidosConsoleWindowControllerWillExecuteScript
	std::swap(sim_RNG, gEidos_RNG_SINGLE);
	std::swap(sim_RNG_initialized, gEidos_RNG_Initialized);

    // Swap out our pedigree id and mutation id counters; see -eidosConsoleWindowControllerWillExecuteScript
    // Setting to -100000 here is not necessary, but will maybe help find bugs...
    sim_next_pedigree_id = gSLiM_next_pedigree_id;
    gSLiM_next_pedigree_id = -100000;

    sim_next_mutation_id = gSLiM_next_mutation_id;
    gSLiM_next_mutation_id = -100000;

    sim_suppress_warnings = gEidosSuppressWarnings;
    gEidosSuppressWarnings = false;

    // Get the current working directory; each SLiM window has its own cwd, which may have been changed in script since ...WillExecuteScript:
    sim_working_dir = Eidos_CurrentDirectory();

    // Return to the app's working directory when not running SLiM/Eidos code
    std::string &app_cwd = qtSLiMAppDelegate->QtSLiMCurrentWorkingDirectory();
    errno = 0;
    int retval = chdir(app_cwd.c_str());

    if (retval == -1)
        qDebug() << "didExecuteScript: Unable to set the working directory to " << app_cwd.c_str() << " (error " << errno << ")";
}

bool QtSLiMWindow::runSimOneTick(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    // This method should always be used when calling out to run the simulation, because it swaps the correct random number
    // generator stuff in and out bracketing the call to RunOneTick().  This bracketing would need to be done around
    // any other call out to the simulation that caused it to use random numbers, too, such as subsample output.
    bool stillRunning = true;

    willExecuteScript();

    // We always take a start clock measurement, to tally elapsed time spent running the model
    clock_t startCPUClock = clock();
    
#if (SLIMPROFILING == 1)
	if (profilePlayOn_)
	{
		// We put the wall clock measurements on the inside since we want those to be maximally accurate,
		// as profile report percentages are fractions of the total elapsed wall clock time.
		SLIM_PROFILE_BLOCK_START();

		stillRunning = community->RunOneTick();

		SLIM_PROFILE_BLOCK_END(community->profile_elapsed_wall_clock);
	}
    else
#endif
    {
        stillRunning = community->RunOneTick();
    }
    
    // Take an end clock time to tally elapsed time spent running the model
    clock_t endCPUClock = clock();
    
    elapsedCPUClock_ += (endCPUClock - startCPUClock);
    
#if (SLIMPROFILING == 1)
	if (profilePlayOn_)
        community->profile_elapsed_CPU_clock += (endCPUClock - startCPUClock);
#endif
    
    didExecuteScript();

    // We also want to let graphViews know when each tick has finished, in case they need to pull data from the sim.  Note this
    // happens after every tick, not just when we are updating the UI, so drawing and setNeedsDisplay: should not happen here.
    emit controllerTickFinished();

    return stillRunning;
}

void QtSLiMWindow::_continuousPlay(void)
{
    // NOTE this code is parallel to the code in _continuousProfile()
	if (!invalidSimulation_)
	{
        QElapsedTimer playStartTimer;
        playStartTimer.start();
        
		double speedSliderValue = ui->playSpeedSlider->value() / 100.0;     // scale is 0 to 100, since only integer values are allowed by QSlider
		double intervalSinceStarting = continuousPlayElapsedTimer_.nsecsElapsed() / 1000000000.0;
		
		// Calculate frames per second; this equation must match the equation in playSpeedChanged:
		double maxTicksPerSecond = 1000000000.0;	// bounded, to allow -eidos_pauseExecution to interrupt us
		
		if (speedSliderValue < 0.99999)
			maxTicksPerSecond = (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * 839;
		
		//qDebug() << "speedSliderValue == " << speedSliderValue << ", maxTicksPerSecond == " << maxTicksPerSecond;
		
		// We keep a local version of reachedSimulationEnd, because calling setReachedSimulationEnd: every tick
		// can actually be a large drag for simulations that run extremely quickly – it can actually exceed the time
		// spent running the simulation itself!  Moral of the story, KVO is wicked slow.
		bool reachedEnd = reachedSimulationEnd_;
		
		do
		{
			if (continuousPlayTicksCompleted_ / intervalSinceStarting >= maxTicksPerSecond)
				break;
			
            if (tickPlayOn_ && (community->Tick() >= targetTick_))
                break;
            
            reachedEnd = !runSimOneTick();
			
			continuousPlayTicksCompleted_++;
		}
		while (!reachedEnd && (playStartTimer.nsecsElapsed() / 1000000000.0) < 0.02);
		
		setReachedSimulationEnd(reachedEnd);
		
		if (!reachedSimulationEnd_ && (!tickPlayOn_ || !(community->Tick() >= targetTick_)))
		{
            updateAfterTickFull((playStartTimer.nsecsElapsed() / 1000000000.0) > 0.04);
			continuousPlayInvocationTimer_.start(0);
		}
		else
		{
			// stop playing
			updateAfterTickFull(true);
            
            if (nonProfilePlayOn_)
                playOrProfile(PlayType::kNormalPlay);       // click the Play button
            else if (tickPlayOn_)
                playOrProfile(PlayType::kTickPlay);   // click the Play button
			
			// bounce our icon; if we are not the active app, to signal that the run is done
			//[NSApp requestUserAttention:NSInformationalRequest];
		}
	}
}

void QtSLiMWindow::_continuousProfile(void)
{
	// NOTE this code is parallel to the code in _continuousPlay()
	if (!invalidSimulation_)
	{
        QElapsedTimer playStartTimer;
        playStartTimer.start();
		
		// We keep a local version of reachedSimulationEnd, because calling setReachedSimulationEnd: every tick
		// can actually be a large drag for simulations that run extremely quickly – it can actually exceed the time
		// spent running the simulation itself!  Moral of the story, KVO is wicked slow.
		bool reachedEnd = reachedSimulationEnd_;
		
		if (!reachedEnd)
		{
			do
			{
                reachedEnd = !runSimOneTick();
				
				continuousPlayTicksCompleted_++;
			}
            while (!reachedEnd && (playStartTimer.nsecsElapsed() / 1000000000.0) < 0.02);
			
            setReachedSimulationEnd(reachedEnd);
		}
		
		if (!reachedSimulationEnd_)
		{
            updateAfterTickFull((playStartTimer.nsecsElapsed() / 1000000000.0) > 0.04);
            continuousProfileInvocationTimer_.start(0);
		}
		else
		{
			// stop profiling
            updateAfterTickFull(true);
			playOrProfile(PlayType::kProfilePlay);   // click the Profile button
			
			// bounce our icon; if we are not the active app, to signal that the run is done
			//[NSApp requestUserAttention:NSInformationalRequest];
		}
	}
}

void QtSLiMWindow::playOrProfile(PlayType playType)
{
#if DEBUG
	if (playType == PlayType::kProfilePlay)
	{
        ui->profileButton->setChecked(false);
        updateProfileButtonIcon(false);
		
        QMessageBox messageBox(this);
        messageBox.setText("Release build required");
        messageBox.setInformativeText("In order to obtain accurate timing information that is relevant to the actual runtime of a model, profiling requires that you are running a Release build of SLiMgui.");
        messageBox.setIcon(QMessageBox::Warning);
        messageBox.setWindowModality(Qt::WindowModal);
        messageBox.exec();
		
		return;
	}
#endif
    
#if (SLIMPROFILING != 1)
	if (playType == PlayType::kProfilePlay)
	{
        ui->profileButton->setChecked(false);
        updateProfileButtonIcon(false);
		
        QMessageBox messageBox(this);
        messageBox.setText("Profiling disabled");
        messageBox.setInformativeText("Profiling has been disabled in this build of SLiMgui.  Please change the definition of SLIMPROFILING to 1 in the project's .pro files.");
        messageBox.setIcon(QMessageBox::Warning);
        messageBox.setWindowModality(Qt::WindowModal);
        messageBox.exec();
		
		return;
	}
#endif
    
    if (!continuousPlayOn_)
	{
        // log information needed to track our play speed
        continuousPlayElapsedTimer_.restart();
		continuousPlayTicksCompleted_ = 0;
        
		setContinuousPlayOn(true);
		if (playType == PlayType::kProfilePlay)
            setProfilePlayOn(true);
        else if (playType == PlayType::kNormalPlay)
            setNonProfilePlayOn(true);
        else if (playType == PlayType::kTickPlay)
            setTickPlayOn(true);
		
		// keep the button on; this works for the button itself automatically, but when the menu item is chosen this is needed
		if (playType == PlayType::kProfilePlay)
		{
            ui->profileButton->setChecked(true);
            updateProfileButtonIcon(false);
		}
		else    // kNormalPlay and kTickPlay
		{
            ui->playButton->setChecked(true);
            updatePlayButtonIcon(false);
			//[self placeSubview:playButton aboveSubview:profileButton];
		}
		
		// invalidate the console symbols, and don't validate them until we are done
		if (consoleController)
            consoleController->invalidateSymbolTableAndFunctionMap();
		
#if (SLIMPROFILING == 1)
		// prepare profiling information if necessary
		if (playType == PlayType::kProfilePlay)
			community->StartProfiling();
#endif
		
		// start playing/profiling
		if (playType == PlayType::kProfilePlay)
            continuousProfileInvocationTimer_.start(0);
        else    // kNormalPlay and kTickPlay
            continuousPlayInvocationTimer_.start(0);
	}
	else
	{
#if (SLIMPROFILING == 1)
		// close out profiling information if necessary
		if ((playType == PlayType::kProfilePlay) && community && !invalidSimulation_)
			community->StopProfiling();
#endif
		
        // stop our recurring perform request
		if (playType == PlayType::kProfilePlay)
            continuousProfileInvocationTimer_.stop();
        else
            continuousPlayInvocationTimer_.stop();
		
        setContinuousPlayOn(false);
        if (playType == PlayType::kProfilePlay)
            setProfilePlayOn(false);
        else if (playType == PlayType::kNormalPlay)
            setNonProfilePlayOn(false);
        else if (playType == PlayType::kTickPlay)
            setTickPlayOn(false);
		
		// keep the button off; this works for the button itself automatically, but when the menu item is chosen this is needed
		if (playType == PlayType::kProfilePlay)
		{
            ui->profileButton->setChecked(false);
            updateProfileButtonIcon(false);
		}
		else    // kNormalPlay and kTickPlay
		{
            ui->playButton->setChecked(false);
            updatePlayButtonIcon(false);
			//[self placeSubview:profileButton aboveSubview:playButton];
		}
		
        // clean up and update UI
		if (consoleController)
            consoleController->validateSymbolTableAndFunctionMap();
        
		updateAfterTickFull(true);
		
#if (SLIMPROFILING == 1)
		// If we just finished profiling, display a report
		if ((playType == PlayType::kProfilePlay) && community && !invalidSimulation_)
			displayProfileResults();
#endif
	}
}

//
//	Eidos SLiMgui method forwards
//

void QtSLiMWindow::finish_eidos_pauseExecution(void)
{
	// this gets called by performSelectorOnMainThread: after _continuousPlay: has broken out of its loop
	// if the simulation has already ended, or is invalid, or is not in continuous play, it does nothing
	if (!invalidSimulation_ && !reachedSimulationEnd_ && continuousPlayOn_ && nonProfilePlayOn_ && !profilePlayOn_ && !tickPlayOn_)
	{
		playOrProfile(PlayType::kNormalPlay);	// this will simulate a press of the play button to stop continuous play
		
		// bounce our icon; if we are not the active app, to signal that the run is done
		//[NSApp requestUserAttention:NSInformationalRequest];
	}
}

void QtSLiMWindow::eidos_openDocument(QString path)
{
    if (path.endsWith(".pdf", Qt::CaseInsensitive))
    {
        // Block opening PDFs; SLiMgui supported PDF but QtSLiM doesn't, so we should explicitly intercept and error out, otherwise we'll try to open the PDF as a SLiM model
		// FIXME: This shouldn't be using EIDOS_TERMINATION!
        EIDOS_TERMINATION << "ERROR (QtSLiMWindow::eidos_openDocument): opening PDF files is not supported in SLiMgui; using PNG instead is suggested." << EidosTerminate(nullptr);
    }
    
    qtSLiMAppDelegate->openFile(path, this);
}

void QtSLiMWindow::eidos_pauseExecution(void)
{
    if (!invalidSimulation_ && !reachedSimulationEnd_ && continuousPlayOn_ && nonProfilePlayOn_ && !profilePlayOn_ && !tickPlayOn_)
	{
		continuousPlayTicksCompleted_ = UINT64_MAX - 1;			// this will break us out of the loop in _continuousPlay: at the end of this tick
        
        QMetaObject::invokeMethod(this, "finish_eidos_pauseExecution", Qt::QueuedConnection);   // this will actually stop continuous play
	}
}


//
//  change tracking and the recycle button
//

// Do our own tracking of the change count.  We do this so that we know whether the script is in
// the same state it was in when we last recycled, or has been changed.  If it has been changed,
// we add a highlight under the recycle button to suggest to the user that they might want to
// recycle to bring their changes into force.
void QtSLiMWindow::updateChangeCount(void) //:(NSDocumentChangeType)change
{
	//[super updateChangeCount:change];
	
	// Mask off flags in the high bits.  Apple is not explicit about this, but NSChangeDiscardable
	// is 256, and acts as a flag bit, so it seems reasonable to assume this for future compatibility.
//	NSDocumentChangeType maskedChange = (NSDocumentChangeType)(change & 0x00FF);
	
//	if ((maskedChange == NSChangeDone) || (maskedChange == NSChangeRedone))
		slimChangeCount++;
//	else if (maskedChange == NSChangeUndone)
//		slimChangeCount--;
	
    emit controllerChangeCountChanged(slimChangeCount);
}

bool QtSLiMWindow::changedSinceRecycle(void)
{
	return !(slimChangeCount == 0);
}

void QtSLiMWindow::resetSLiMChangeCount(void)
{
    slimChangeCount = 0;
    emit controllerChangeCountChanged(slimChangeCount);
}

// slot receiving the signal QPlainTextEdit::textChanged() from the script textedit
void QtSLiMWindow::scriptTexteditChanged(void)
{
    // Poke the change count.  In SLiMgui we get separate notification types for changes vs. undo/redo,
    // allowing us to know when the document has returned to a checkpoint state due to undo/redo, but
    // there seems to be no way to do that with Qt, so once we register a change, only recycling will
    // bring us back to the unchanged state.
    updateChangeCount();
}


//
//  public slots
//

void QtSLiMWindow::playOneStepClicked(void)
{
    if (!invalidSimulation_)
    {
        if (consoleController)
            consoleController->invalidateSymbolTableAndFunctionMap();
        
        setReachedSimulationEnd(!runSimOneTick());
        
        // BCH 5/7/2021: moved these two lines up here, above validateSymbolTableAndFunctionMap(), so that
        // updateAfterTickFull() calls checkForSimulationTermination() for us before we re-validate the
        // symbol table; this way if the simulation has hit an error the symbol table no longer contains
        // SLiM stuff in it.  I *think* this mirrors what happens when play, rather than step, is used.
        // Nevertheless, it might be a fragile change, so I'm leaving this comment to document the change.
        ui->tickLineEdit->clearFocus();
        updateAfterTickFull(true);
        
        if (consoleController)
            consoleController->validateSymbolTableAndFunctionMap();
    }
}

void QtSLiMWindow::_playOneStep(void)
{
    playOneStepClicked();
    
    if (!reachedSimulationEnd_)
    {
        playOneStepInvocationTimer_.start(350); // milliseconds
    }
    else
    {
        // stop playing
        playOneStepReleased();
    }
}

void QtSLiMWindow::playOneStepPressed(void)
{
    ui->playOneStepButton->qtslimSetHighlight(true);
    _playOneStep();
}

void QtSLiMWindow::playOneStepReleased(void)
{
    ui->playOneStepButton->qtslimSetHighlight(false);
    playOneStepInvocationTimer_.stop();
}

void QtSLiMWindow::tickChanged(void)
{
	if (!tickPlayOn_)
	{
		QString tickString = ui->tickLineEdit->text();
		
		// Special-case initialize(); we can never advance to it, since it is first, so we just validate it
		if (tickString == "initialize()")
		{
			if (community->Tick() != 0)
			{
				qApp->beep();
				updateTickCounter();
                ui->tickLineEdit->selectAll();
			}
			
			return;
		}
		
		// Get the integer value from the textfield, since it is not "initialize()"
		targetTick_ = SLiMClampToTickType(static_cast<int64_t>(tickString.toLongLong()));
		
		// make sure the requested tick is in range
		if (community->Tick() >= targetTick_)
		{
			if (community->Tick() > targetTick_)
            {
                qApp->beep();
                updateTickCounter();
                ui->tickLineEdit->selectAll();
			}
            
			return;
		}
		
		// get the first responder out of the tick textfield
        ui->tickLineEdit->clearFocus();
		
		// start playing
        playOrProfile(PlayType::kTickPlay);
	}
	else
	{
		// stop our recurring perform request; I don't think this is hit any more
        playOrProfile(PlayType::kTickPlay);
	}
}

void QtSLiMWindow::recycleClicked(void)
{
    // If the user has requested autosaves, act on that; these calls run modal, blocking panels
    if (!isZombieWindow_)
    {
        QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
        
        if (prefsNotifier.autosaveOnRecyclePref())
        {
            if (!isUntitled)
                saveFile(currentFile);
            else if (prefsNotifier.showSaveIfUntitledPref())
                saveAs();
            //else
            //    qApp->beep();
        }
    }
    
    // Now do the recycle
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    // Converting a QString to a std::string is surprisingly tricky: https://stackoverflow.com/a/4644922/2752221
    std::string utf8_script_string = ui->scriptTextEdit->toPlainText().toUtf8().constData();
    
    if (consoleController)
        consoleController->invalidateSymbolTableAndFunctionMap();
    
    clearOutputClicked();
    if (debugOutputWindow_)
        debugOutputWindow_->clearAllOutput();
    
    setScriptStringAndInitializeSimulation(utf8_script_string);
    
    if (consoleController)
        consoleController->validateSymbolTableAndFunctionMap();
    
    ui->tickLineEdit->clearFocus();
    elapsedCPUClock_ = 0;
    
    updateAfterTickFull(true);
    
    ui->scriptTextEdit->setPalette(ui->scriptTextEdit->qtslimStandardPalette());     // clear any error highlighting
    
    // A bit of playing with undo.  We want to break undo coalescing at the point of recycling, so that undo and redo stop
    // at the moment that we recycled.  Then we reset a change counter that we use to know if we have changed relative to
    // the recycle point, so we can highlight the recycle button to show that the executing script is out of date.
    //[scriptTextView breakUndoCoalescing];
    resetSLiMChangeCount();
    
    emit controllerRecycled();
}

void QtSLiMWindow::playSpeedChanged(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
	// We want our speed to be from the point when the slider changed, not from when play started
    continuousPlayElapsedTimer_.restart();
	continuousPlayTicksCompleted_ = 1;		// this prevents a new tick from executing every time the slider moves a pixel
	
	// This method is called whenever playSpeedSlider changes, continuously; we want to show the chosen speed in a tooltip-ish window
    double speedSliderValue = ui->playSpeedSlider->value() / 100.0;     // scale is 0 to 100, since only integer values are allowed by QSlider
	
	// Calculate frames per second; this equation must match the equation in _continuousPlay:
	double maxTicksPerSecond = static_cast<double>(INFINITY);
	
	if (speedSliderValue < 0.99999)
		maxTicksPerSecond = (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * 839;
	
	// Make a tooltip label string
	QString fpsString("∞ fps");
	
	if (!std::isinf(maxTicksPerSecond))
	{
		if (maxTicksPerSecond < 1.0)
			fpsString = QString::asprintf("%.2f fps", maxTicksPerSecond);
		else if (maxTicksPerSecond < 10.0)
			fpsString = QString::asprintf("%.1f fps", maxTicksPerSecond);
		else
			fpsString = QString::asprintf("%.0f fps", maxTicksPerSecond);
		
		//qDebug() << "fps string: " << fpsString;
	}
    
    // Show the tooltip; wow, that was easy...
    QPoint widgetOrigin = ui->playSpeedSlider->mapToGlobal(QPoint());
    QPoint cursorPosition = QCursor::pos();
    QPoint tooltipPosition = QPoint(cursorPosition.x() - 2, widgetOrigin.y() - ui->playSpeedSlider->rect().height() - 8);
    QToolTip::showText(tooltipPosition, fpsString, ui->playSpeedSlider, QRect(), 1000000);  // 1000 seconds; taken down on mouseup automatically
}

void QtSLiMWindow::showDrawerClicked(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    if (!tablesDrawerController)
        tablesDrawerController = new QtSLiMTablesDrawer(this);
    
    // position it to the right of the main window, with the same height
    QRect windowRect = geometry();
    windowRect.setLeft(windowRect.left() + windowRect.width() + 9);
    windowRect.setRight(windowRect.left() + 200);   // the minimum in the nib is larger
    
    tablesDrawerController->setGeometry(windowRect);
    
    tablesDrawerController->show();
    tablesDrawerController->raise();
    tablesDrawerController->activateWindow();
}

void QtSLiMWindow::showConsoleClicked(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    if (!consoleController)
    {
        qApp->beep();
        return;
    }
    
    consoleController->show();
    consoleController->raise();
    consoleController->activateWindow();
}

void QtSLiMWindow::showBrowserClicked(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    if (!consoleController)
    {
        qApp->beep();
        return;
    }
    
    consoleController->showBrowserClicked();
}

void QtSLiMWindow::debugOutputClicked(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    if (!debugOutputWindow_)
    {
        qApp->beep();
        return;
    }
    
    stopDebugButtonFlash();
    
    debugOutputWindow_->show();
    debugOutputWindow_->raise();
    debugOutputWindow_->activateWindow();
}

void QtSLiMWindow::runChromosomeContextMenuAtPoint(QPoint p_globalPoint)
{
    if (!invalidSimulation() && community && community->simulation_valid_)
	{
        QMenu contextMenu("chromosome_menu", this);
        
        QAction *displayMutations = contextMenu.addAction("Display Mutations");
        displayMutations->setCheckable(true);
        displayMutations->setChecked(chromosome_shouldDrawMutations_);
        
        QAction *displaySubstitutions = contextMenu.addAction("Display Substitutions");
        displaySubstitutions->setCheckable(true);
        displaySubstitutions->setChecked(chromosome_shouldDrawFixedSubstitutions_);
        
        QAction *displayGenomicElements = contextMenu.addAction("Display Genomic Elements");
        displayGenomicElements->setCheckable(true);
        displayGenomicElements->setChecked(chromosome_shouldDrawGenomicElements_);
        
        QAction *displayRateMaps = contextMenu.addAction("Display Rate Maps");
        displayRateMaps->setCheckable(true);
        displayRateMaps->setChecked(chromosome_shouldDrawRateMaps_);
        
        contextMenu.addSeparator();
        
        QAction *displayFrequencies = contextMenu.addAction("Display Frequencies");
        displayFrequencies->setCheckable(true);
        displayFrequencies->setChecked(!chromosome_display_haplotypes_);
        
        QAction *displayHaplotypes = contextMenu.addAction("Display Haplotypes");
        displayHaplotypes->setCheckable(true);
        displayHaplotypes->setChecked(chromosome_display_haplotypes_);
        
        QActionGroup *displayGroup = new QActionGroup(this);    // On Linux this provides a radio-button-group appearance
        displayGroup->addAction(displayFrequencies);
        displayGroup->addAction(displayHaplotypes);
        
        QAction *displayAllMutations = nullptr;
        QAction *selectNonneutralMutations = nullptr;
        
		// mutation type checkmark items
        {
            const std::map<slim_objectid_t,MutationType*> &muttypes = community->AllMutationTypes();
			
			if (muttypes.size() > 0)
			{
                contextMenu.addSeparator();
                
                displayAllMutations = contextMenu.addAction("Display All Mutations");
                displayAllMutations->setCheckable(true);
                displayAllMutations->setChecked(chromosome_display_muttypes_.size() == 0);
                
                // Make a sorted list of all mutation types we know – those that exist, and those that used to exist that we are displaying
				std::vector<slim_objectid_t> all_muttypes;
				
				for (auto muttype_iter : muttypes)
				{
					MutationType *muttype = muttype_iter.second;
					slim_objectid_t muttype_id = muttype->mutation_type_id_;
					
					all_muttypes.emplace_back(muttype_id);
				}
				
				all_muttypes.insert(all_muttypes.end(), chromosome_display_muttypes_.begin(), chromosome_display_muttypes_.end());
                
                // Avoid building a huge menu, which will hang the app
				if (all_muttypes.size() <= 500)
				{
					std::sort(all_muttypes.begin(), all_muttypes.end());
					all_muttypes.resize(static_cast<size_t>(std::distance(all_muttypes.begin(), std::unique(all_muttypes.begin(), all_muttypes.end()))));
					
					// Then add menu items for each of those muttypes
					for (slim_objectid_t muttype_id : all_muttypes)
					{
                        QString menuItemTitle = QString("Display m%1").arg(muttype_id);
                        MutationType *muttype = community->MutationTypeWithID(muttype_id);  // try to look up the mutation type; can fail if it doesn't exists now
                        
                        if (muttype && (community->all_species_.size() > 1))
                            menuItemTitle.append(" ").append(QString::fromStdString(muttype->species_.avatar_));
                        
                        QAction *mutationAction = contextMenu.addAction(menuItemTitle);
                        
                        mutationAction->setData(muttype_id);
                        mutationAction->setCheckable(true);
                        
						if (std::find(chromosome_display_muttypes_.begin(), chromosome_display_muttypes_.end(), muttype_id) != chromosome_display_muttypes_.end())
							mutationAction->setChecked(true);
					}
				}
                
                contextMenu.addSeparator();
                
                selectNonneutralMutations = contextMenu.addAction("Select Non-Neutral MutationTypes");
            }
        }
        
        // Run the context menu synchronously
        QAction *action = contextMenu.exec(p_globalPoint);
        
        // Act upon the chosen action; we just do it right here instead of dealing with slots
        if (action)
        {
            if (action == displayMutations)
                chromosome_shouldDrawMutations_ = !chromosome_shouldDrawMutations_;
            else if (action == displaySubstitutions)
                chromosome_shouldDrawFixedSubstitutions_ = !chromosome_shouldDrawFixedSubstitutions_;
            else if (action == displayGenomicElements)
                chromosome_shouldDrawGenomicElements_ = !chromosome_shouldDrawGenomicElements_;
            else if (action == displayRateMaps)
                chromosome_shouldDrawRateMaps_ = !chromosome_shouldDrawRateMaps_;
            else if (action == displayFrequencies)
                chromosome_display_haplotypes_ = false;
            else if (action == displayHaplotypes)
                chromosome_display_haplotypes_ = true;
            else
            {
                const std::map<slim_objectid_t,MutationType*> &muttypes = community->AllMutationTypes();
                
                if (action == displayAllMutations)
                    chromosome_display_muttypes_.clear();
                else if (action == selectNonneutralMutations)
                {
                    // - (IBAction)filterNonNeutral:(id)sender
                    chromosome_display_muttypes_.clear();
                    
                    for (auto muttype_iter : muttypes)
                    {
                        MutationType *muttype = muttype_iter.second;
                        slim_objectid_t muttype_id = muttype->mutation_type_id_;
                        
                        if ((muttype->dfe_type_ != DFEType::kFixed) || (muttype->dfe_parameters_[0] != 0.0))
                            chromosome_display_muttypes_.emplace_back(muttype_id);
                    }
                }
                else
                {
                    // - (IBAction)filterMutations:(id)sender
                    slim_objectid_t muttype_id = action->data().toInt();
                    auto present_iter = std::find(chromosome_display_muttypes_.begin(), chromosome_display_muttypes_.end(), muttype_id);
                    
                    if (present_iter == chromosome_display_muttypes_.end())
                    {
                        // this mut-type is not being displayed, so add it to our display list
                        chromosome_display_muttypes_.emplace_back(muttype_id);
                    }
                    else
                    {
                        // this mut-type is being displayed, so remove it from our display list
                        chromosome_display_muttypes_.erase(present_iter);
                    }
                }
            }
            
            for (auto *widget : chromosomeZoomedWidgets)
                widget->update();
        }
    }
}

void QtSLiMWindow::chromosomeActionRunMenu(void)
{
    QPoint mousePos = QCursor::pos();
    
    runChromosomeContextMenuAtPoint(mousePos);
    
    // This is not called by Qt, for some reason (nested tracking loops?), so we call it explicitly
    chromosomeActionReleased();
}

void QtSLiMWindow::jumpToPopupButtonRunMenu(void)
{
    QPlainTextEdit *scriptTE = ui->scriptTextEdit;
    QString currentScriptString = scriptTE->toPlainText();
    QByteArray utf8bytes = currentScriptString.toUtf8();
	const char *cstr = utf8bytes.constData();
    bool failedParse = true;
    
    // Collect actions, with associated script positions
    std::vector<std::pair<int32_t, QAction *>> jumpActions;
    
    // First we scan for comments of the form /** comment */ or /// comment, which are taken to be section headers
    // BCH 10/13/2020: Now we exclude comments of the form /*** or ////, since they are not of the expected form, but are instead just somebody's fancy comment block
    // BCH 11/15/2021: Whoops, the previous change broke /***/ as a separator item; added back in as a special case
    if (cstr)
    {
        SLiMEidosScript script(cstr);
        
        script.Tokenize(true, true);            // make bad tokens as needed, keep nonsignificant tokens
        
        const std::vector<EidosToken> &tokens = script.Tokens();
        size_t token_count = tokens.size();
        QString comment;
        
        for (size_t token_index = 0; token_index < token_count; ++token_index)
        {
            const EidosToken &token = tokens[token_index];
            
            if ((token.token_type_ == EidosTokenType::kTokenCommentLong) && (token.token_string_ == "/***/"))
            {
                comment = QString();
            }
            else if ((token.token_type_ == EidosTokenType::kTokenComment) && (token.token_string_.rfind("///", 0) == 0) && (token.token_string_.rfind("////", 0) != 0))
            {
                comment = QString::fromStdString(token.token_string_);
                comment = comment.mid(3);
            }
            else if (token.token_type_ == EidosTokenType::kTokenCommentLong && (token.token_string_.rfind("/**", 0) == 0) && (token.token_string_.rfind("/***", 0) != 0))
            {
                comment = QString::fromStdString(token.token_string_);
                comment = comment.mid(3, comment.length() - 5);
            }
            else
                continue;
            
            // Exclude comments that contain newlines and similar characters
            if ((comment.indexOf(QChar::LineFeed) != -1) ||
                    (comment.indexOf(0x0C) != -1) ||
                    (comment.indexOf(QChar::CarriageReturn) != -1) ||
                    (comment.indexOf(QChar::ParagraphSeparator) != -1) ||
                    (comment.indexOf(QChar::LineSeparator) != -1))
                continue;
            
            comment = comment.trimmed();
            comment = comment.replace("&", "&&");   // quote ampersands since Qt uses them as keyboard shortcut escapes
            
            int32_t comment_start = token.token_UTF16_start_;
            int32_t comment_end = token.token_UTF16_end_ + 1;
            QAction *jumpAction;
            
            if (comment.length() == 0)
            {
                jumpAction = new QAction("", scriptTE);
                jumpAction->setSeparator(true);
            }
            else
            {
                // we cannot handle within-text formatting, since Qt doesn't support it; just an overall style
                // this is supported only on these section header items; we can't do the formatting on script block items
                
                // handle # H1 to ###### H6 headers, used to set the font size; these cannot be nested
                int headerLevel = 3;    // 1/2 are bigger; 3 is "default" and has no effect; 4/5/6 are progressively smaller
                
                if (comment.startsWith("# "))
                {
                    headerLevel = 1;
                    comment = comment.mid(2);
                }
                else if (comment.startsWith("## "))
                {
                    headerLevel = 2;
                    comment = comment.mid(3);
                }
                else if (comment.startsWith("### "))
                {
                    headerLevel = 3;
                    comment = comment.mid(4);
                }
                else if (comment.startsWith("#### "))
                {
                    headerLevel = 4;
                    comment = comment.mid(5);
                }
                else if (comment.startsWith("##### "))
                {
                    headerLevel = 5;
                    comment = comment.mid(6);
                }
                else if (comment.startsWith("###### "))
                {
                    headerLevel = 6;
                    comment = comment.mid(7);
                }
                
                // handle **bold** and _italic_ markdown; these can be nested and all get eaten
                bool isBold = false, isItalic = false;
                bool sawStyleChange = false;
                
                do
                {
                    // loop until this stays false, so we handle nested styles
                    sawStyleChange = false;
                    
                    if (comment.startsWith("__") && comment.endsWith("__"))
                    {
                        isBold = true;
                        sawStyleChange = true;
                        comment = comment.mid(2, comment.length() - 4);
                    }
                    if (comment.startsWith("**") && comment.endsWith("**"))
                    {
                        isBold = true;
                        sawStyleChange = true;
                        comment = comment.mid(2, comment.length() - 4);
                    }
                    if (comment.startsWith("_") && comment.endsWith("_"))
                    {
                        isItalic = true;
                        sawStyleChange = true;
                        comment = comment.mid(1, comment.length() - 2);
                    }
                    if (comment.startsWith("*") && comment.endsWith("*"))
                    {
                        isItalic = true;
                        sawStyleChange = true;
                        comment = comment.mid(1, comment.length() - 2);
                    }
                }
                while (sawStyleChange);
                
                jumpAction = new QAction(comment);
                connect(jumpAction, &QAction::triggered, scriptTE, [scriptTE, comment_start, comment_end]() {
                    QTextCursor cursor = scriptTE->textCursor();
                    cursor.setPosition(comment_start, QTextCursor::MoveAnchor);
                    cursor.setPosition(comment_end, QTextCursor::KeepAnchor);
                    scriptTE->setTextCursor(cursor);
                    scriptTE->centerCursor();
                    QtSLiMFlashHighlightInTextEdit(scriptTE);
                });
                
                QFont action_font = jumpAction->font();
                if (isBold)
                    action_font.setBold(true);
                if (isItalic)
                    action_font.setItalic(true);
                if (headerLevel == 1)
                    action_font.setPointSizeF(action_font.pointSizeF() * 1.50);
                if (headerLevel == 2)
                    action_font.setPointSizeF(action_font.pointSizeF() * 1.25);
                //if (headerLevel == 3)
                //    action_font.setPointSizeF(action_font.pointSizeF() * 1.00);
                if (headerLevel == 4)
                    action_font.setPointSizeF(action_font.pointSizeF() * 0.96);
                if (headerLevel == 5)
                    action_font.setPointSizeF(action_font.pointSizeF() * 0.85);
                if (headerLevel == 6)
                    action_font.setPointSizeF(action_font.pointSizeF() * 0.75);
                jumpAction->setFont(action_font);
            }
            
            jumpActions.emplace_back(comment_start, jumpAction);
        }
    }
    
    // Figure out whether we have multispecies avatars, and thus want to use the "low brightness symbol" emoji for "ticks all" blocks.
    // This emoji provides nicely lined up spacing in the menu, and indicates "ticks all" clearly; seems better than nothing.  It would
    // be even better, perhaps, to have a spacer of emoji width, to make things line up without having a symbol displayed; unfortunately
    // such a spacer does not seem to exist.  https://stackoverflow.com/questions/66496671/is-there-a-blank-unicode-character-matching-emoji-width
    QString ticksAllAvatar;
    
    if (community && community->is_explicit_species_ && (community->all_species_.size() > 0))
    {
        bool hasAvatars = false;
        
        for (Species *species : community->all_species_)
            if (species->avatar_.length() > 0)
                hasAvatars = true;
        
        if (hasAvatars)
            ticksAllAvatar = QString::fromUtf8("\xF0\x9F\x94\x85");     // "low brightness symbol", https://www.compart.com/en/unicode/U+1F505
    }
    
    // Next we parse and get script blocks
    if (cstr)
    {
        SLiMEidosScript script(cstr);
        
        try {
            script.Tokenize(true, false);            // make bad tokens as needed, do not keep nonsignificant tokens
            script.ParseSLiMFileToAST(true);        // make bad nodes as needed (i.e. never raise, and produce a correct tree)
            
            // Extract SLiMEidosBlocks from the parse tree
            const EidosASTNode *root_node = script.AST();
            QString specifierAvatar;
            
            for (EidosASTNode *script_block_node : root_node->children_)
            {
                // handle species/ticks specifiers, which are identifier token nodes at the top level of the AST with one child
                if ((script_block_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (script_block_node->children_.size() == 1))
                {
                    EidosASTNode *specifierChild = script_block_node->children_[0];
                    std::string specifierSpeciesName = specifierChild->token_->token_string_;
                    Species *specifierSpecies = (community ? community->SpeciesWithName(specifierSpeciesName) : nullptr);
                    
                    if (specifierSpecies && specifierSpecies->avatar_.length())
                        specifierAvatar = QString::fromStdString(specifierSpecies->avatar_);
                    else if (!specifierSpecies && (specifierSpeciesName == "all"))
                        specifierAvatar = ticksAllAvatar;
                    
                    continue;
                }
                
                // Create the block and use it to find the string from the start of its declaration to the start of its code
                SLiMEidosBlock *new_script_block = new SLiMEidosBlock(script_block_node);
                int32_t decl_start = new_script_block->root_node_->token_->token_UTF16_start_;
                int32_t code_start = new_script_block->compound_statement_node_->token_->token_UTF16_start_;
                QString decl = currentScriptString.mid(decl_start, code_start - decl_start);
                
                // Remove everything including and after the first newline
                if (decl.indexOf(QChar::LineFeed) != -1)
                    decl.truncate(decl.indexOf(QChar::LineFeed));
                if (decl.indexOf(0x0C) != -1)                       // form feed; apparently QChar::FormFeed did not exist in older Qt versions
                    decl.truncate(decl.indexOf(0x0C));
                if (decl.indexOf(QChar::CarriageReturn) != -1)
                    decl.truncate(decl.indexOf(QChar::CarriageReturn));
                if (decl.indexOf(QChar::ParagraphSeparator) != -1)
                    decl.truncate(decl.indexOf(QChar::ParagraphSeparator));
                if (decl.indexOf(QChar::LineSeparator) != -1)
                    decl.truncate(decl.indexOf(QChar::LineSeparator));
                
                // Extract a comment at the end and put it after a em-dash in the string
                int simpleCommentStart = decl.indexOf("//");
                int blockCommentStart = decl.indexOf("/*");
                QString comment;
                
                if ((simpleCommentStart != -1) && ((blockCommentStart == -1) || (simpleCommentStart < blockCommentStart)))
                {
                    // extract a simple comment
                    comment = decl.right(decl.length() - simpleCommentStart - 2);
                    decl.truncate(simpleCommentStart);
                }
                else if ((blockCommentStart != -1) && ((simpleCommentStart == -1) || (blockCommentStart < simpleCommentStart)))
                {
                    // extract a block comment
                    comment = decl.right(decl.length() - blockCommentStart - 2);
                    decl.truncate(blockCommentStart);
                    
                    int blockCommentEnd = comment.indexOf("*/");
                    
                    if (blockCommentEnd != -1)
                        comment.truncate(blockCommentEnd);
                }
                
                // Calculate the end of the declaration string; trim off whitespace at the end
                decl = decl.trimmed();
                
                int32_t decl_end = decl_start + decl.length();
                
                // Remove trailing whitespace, replace tabs with spaces, etc.
                decl = decl.simplified();
                comment = comment.trimmed();
                comment = comment.replace("&", "&&");   // quote ampersands since Qt uses them as keyboard shortcut escapes
                
                if (comment.length() > 0)
                    decl = decl + "  —  " + comment;
                
                // If a species/ticks specifier was previously seen that provides us with an avatar, prepend that
                if (specifierAvatar.length())
                {
                    decl = specifierAvatar + " " + decl;
                    specifierAvatar.clear();
                }
                
                // Make a menu item with the final string, and annotate it with the range to select
                QAction *jumpAction = new QAction(decl);
                
                connect(jumpAction, &QAction::triggered, scriptTE, [scriptTE, decl_start, decl_end]() {
                    QTextCursor cursor = scriptTE->textCursor();
                    cursor.setPosition(decl_start, QTextCursor::MoveAnchor);
                    cursor.setPosition(decl_end, QTextCursor::KeepAnchor);
                    scriptTE->setTextCursor(cursor);
                    scriptTE->centerCursor();
                    QtSLiMFlashHighlightInTextEdit(scriptTE);
                });
                
                jumpActions.emplace_back(decl_start, jumpAction);
                
                failedParse = false;
                
                delete new_script_block;
            }
        }
        catch (...)
        {
        }
    }
    
    QMenu contextMenu("jump_to_menu", this);
    
    if (failedParse || (jumpActions.size() == 0))
    {
        QAction *parseErrorItem = contextMenu.addAction("No symbols");
        parseErrorItem->setEnabled(false);
        
        // contextMenu never took ownership, so we need to dispose of allocated actions
        for (auto &jump_pair : jumpActions)
            delete jump_pair.second;
    }
    else
    {
        // sort the actions by position
        std::sort(jumpActions.begin(),
                  jumpActions.end(),
                  [](const std::pair<int32_t, QAction *> &a1, const std::pair<int32_t, QAction *> &a2) { return a1.first < a2.first; });
        
        // add them all to contextMenu, and give it ownership
        for (auto &jump_pair : jumpActions)
        {
            contextMenu.addAction(jump_pair.second);
            jump_pair.second->setParent(&contextMenu);
        }
    }
    
    // Run the context menu synchronously
    QPoint mousePos = QCursor::pos();
    contextMenu.exec(mousePos);
    
    // This is not called by Qt, for some reason (nested tracking loops?), so we call it explicitly
    jumpToPopupButtonReleased();
}

void QtSLiMWindow::clearOutputClicked(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    ui->outputTextEdit->setPlainText("");
}

void QtSLiMWindow::clearDebugPointsClicked(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    ui->scriptTextEdit->clearDebugPoints();
}

void QtSLiMWindow::dumpPopulationClicked(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    try
	{
        // BCH 3/6/2022: Note that the species cycle has been added here for SLiM 4, in keeping with SLiM's native output formats.
        Species *displaySpecies = focalDisplaySpecies();
        
        if (displaySpecies)
        {
            slim_tick_t species_cycle = displaySpecies->Cycle();
            
            // dump the population
            SLIM_OUTSTREAM << "#OUT: " << community->tick_ << " " << species_cycle << " A" << std::endl;
            displaySpecies->population_.PrintAll(SLIM_OUTSTREAM, true, true, false, false);	// output spatial positions and ages if available, but not ancestral sequence
            
            // dump fixed substitutions also; so the dump in SLiMgui is like outputFull() + outputFixedMutations()
            SLIM_OUTSTREAM << std::endl;
            SLIM_OUTSTREAM << "#OUT: " << community->tick_ << " " << species_cycle << " F " << std::endl;
            SLIM_OUTSTREAM << "Mutations:" << std::endl;
            
            for (unsigned int i = 0; i < displaySpecies->population_.substitutions_.size(); i++)
            {
                SLIM_OUTSTREAM << i << " ";
                displaySpecies->population_.substitutions_[i]->PrintForSLiMOutput(SLIM_OUTSTREAM);
            }
            
            // now send SLIM_OUTSTREAM to the output textview
            updateOutputViews();
        }
        else
        {
            // With no display species, including when on the "all" species tab, we just beep
            qApp->beep();
        }
	}
	catch (...)
	{
	}
}

void QtSLiMWindow::displayGraphClicked(void)
{
    // see QtSLiMWindow::graphPopupButtonRunMenu() for parallel code for the graph pop-up button
    QObject *object = sender();
    QAction *action = qobject_cast<QAction *>(object);
    
    if (action)
    {
        Species *displaySpecies = focalDisplaySpecies();
        
        if (action == ui->actionCreate_Haplotype_Plot)
        {
            if (displaySpecies && !continuousPlayOn_ && displaySpecies->population_.subpops_.size())
            {
                isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
                
                QtSLiMHaplotypeManager::CreateHaplotypePlot(this);
            }
            else
            {
                qApp->beep();
            }
        }
        else
        {
            QtSLiMGraphView *graphView = nullptr;
            
            if (displaySpecies)
            {
                if (action == ui->actionGraph_1D_Population_SFS)
                    graphView = new QtSLiMGraphView_1DPopulationSFS(this, this);
                else if (action == ui->actionGraph_1D_Sample_SFS)
                    graphView = new QtSLiMGraphView_1DSampleSFS(this, this);
                else if (action == ui->actionGraph_2D_Population_SFS)
                    graphView = new QtSLiMGraphView_2DPopulationSFS(this, this);
                else if (action == ui->actionGraph_2D_Sample_SFS)
                    graphView = new QtSLiMGraphView_2DSampleSFS(this, this);
                else if (action == ui->actionGraph_Mutation_Frequency_Trajectories)
                    graphView = new QtSLiMGraphView_FrequencyTrajectory(this, this);
                else if (action == ui->actionGraph_Mutation_Loss_Time_Histogram)
                    graphView = new QtSLiMGraphView_LossTimeHistogram(this, this);
                else if (action == ui->actionGraph_Mutation_Fixation_Time_Histogram)
                    graphView = new QtSLiMGraphView_FixationTimeHistogram(this, this);
                else if (action == ui->actionGraph_Population_Fitness_Distribution)
                    graphView = new QtSLiMGraphView_PopFitnessDist(this, this);
                else if (action == ui->actionGraph_Subpopulation_Fitness_Distributions)
                    graphView = new QtSLiMGraphView_SubpopFitnessDists(this, this);
                else if (action == ui->actionGraph_Fitness_Time)
                    graphView = new QtSLiMGraphView_FitnessOverTime(this, this);
                else if (action == ui->actionGraph_Age_Distribution)
                    graphView = new QtSLiMGraphView_AgeDistribution(this, this);
                else if (action == ui->actionGraph_Lifetime_Reproduce_Output)
                    graphView = new QtSLiMGraphView_LifetimeReproduction(this, this);
                else if (action == ui->actionGraph_Population_Size_Time)
                    graphView = new QtSLiMGraphView_PopSizeOverTime(this, this);
                else if (action == ui->actionGraph_Population_Visualization)
                    graphView = new QtSLiMGraphView_PopulationVisualization(this, this);
            }
            else if (action == ui->actionGraph_Multispecies_Population_Size_Time)
                graphView = new QtSLiMGraphView_MultispeciesPopSizeOverTime(this, this);
            
            if (graphView)
            {
                QWidget *graphWindow = graphWindowWithView(graphView);
                
                if (graphWindow)
                {
                    graphWindow->show();
                    graphWindow->raise();
                    graphWindow->activateWindow();
                }
            }
            else
            {
                qApp->beep();
            }
        }
    }
}

static bool rectIsOnscreen(QRect windowRect)
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    
    for (QScreen *screen : screens)
    {
        QRect screenRect = screen->availableGeometry();
        
        if (screenRect.contains(windowRect, true))
            return true;
    }
    
    return false;
}

void QtSLiMWindow::positionNewSubsidiaryWindow(QWidget *p_window)
{
    // force geometry calculation, which is lazy
    p_window->setAttribute(Qt::WA_DontShowOnScreen, true);
    p_window->show();
    p_window->hide();
    p_window->setAttribute(Qt::WA_DontShowOnScreen, false);
    
    // Now get the frame geometry; note that on X11 systems the window frame is often not included in frameGeometry(), even
    // though it's supposed to be, because it is simply not available from X.  We attempt to compensate by adding in the
    // height of the window title bar, although that entails making assumptions about the windowing system appearance.
    QRect windowFrame = p_window->frameGeometry();
    QRect mainWindowFrame = this->frameGeometry();
    bool drawerIsOpen = (!!tablesDrawerController);
    const int titleBarHeight = 30;
    QPoint unadjust;
    
    if (windowFrame == p_window->geometry())
    {
        windowFrame.adjust(0, -titleBarHeight, 0, 0);
        unadjust = QPoint(0, 30);
    }
    if (mainWindowFrame == this->geometry())
    {
        mainWindowFrame.adjust(0, -titleBarHeight, 0, 0);
    }
    
    // try along the bottom first
    {
        QRect candidateFrame = windowFrame;
        
        candidateFrame.moveLeft(mainWindowFrame.left() + openedGraphCount_bottom * (windowFrame.width() + 5));
        candidateFrame.moveTop(mainWindowFrame.bottom() + 5);
        
        if (rectIsOnscreen(candidateFrame) && (candidateFrame.right() <= mainWindowFrame.right()))          // avoid going over to the right, to leave room for the tables drawer window
        {
            p_window->move(candidateFrame.topLeft() + unadjust);
            openedGraphCount_bottom++;
            return;
        }
    }
    
    // try on the left side
    {
        QRect candidateFrame = windowFrame;
        
        candidateFrame.moveRight(mainWindowFrame.left() - 5);
        candidateFrame.moveTop(mainWindowFrame.top() + openedGraphCount_left * (windowFrame.height() + 5));
        
        if (rectIsOnscreen(candidateFrame)) // && (candidateFrame.bottom() <= mainWindowFrame.bottom()))    // doesn't overlap anybody else
        {
            p_window->move(candidateFrame.topLeft() + unadjust);
            openedGraphCount_left++;
            return;
        }
    }
    
    // try along the top
	{
		QRect candidateFrame = windowFrame;
		
		candidateFrame.moveLeft(mainWindowFrame.left() + openedGraphCount_top * (windowFrame.width() + 5));
		candidateFrame.moveBottom(mainWindowFrame.top() - 5);
		
        if (rectIsOnscreen(candidateFrame)) // && (candidateFrame.right() <= mainWindowFrame.right()))    // doesn't overlap anybody else
        {
            p_window->move(candidateFrame.topLeft() + unadjust);
            openedGraphCount_top++;
            return;
        }
	}
    
    // unless the drawer is open, let's try on the right side
	if (!drawerIsOpen)
	{
		QRect candidateFrame = windowFrame;
		
		candidateFrame.moveLeft(mainWindowFrame.right() + 5);
        candidateFrame.moveTop(mainWindowFrame.top() + openedGraphCount_right * (windowFrame.height() + 5));
		
        if (rectIsOnscreen(candidateFrame)) // && (candidateFrame.bottom() <= mainWindowFrame.bottom()))   // doesn't overlap anybody else
        {
            p_window->move(candidateFrame.topLeft() + unadjust);
            openedGraphCount_right++;
            return;
        }
	}
	
    // if the drawer is open, try to the right of it
    if (drawerIsOpen)
    {
        QRect drawerFrame = tablesDrawerController->frameGeometry();
        QRect candidateFrame = windowFrame;
		
		candidateFrame.moveLeft(drawerFrame.right() + 5);
        candidateFrame.moveTop(drawerFrame.top() + openedGraphCount_right * (windowFrame.height() + 5));
		
        if (rectIsOnscreen(candidateFrame)) // && (candidateFrame.bottom() <= drawerFrame.bottom()))    // doesn't overlap anybody else
        {
            p_window->move(candidateFrame.topLeft() + unadjust);
            openedGraphCount_right++;
            return;
        }
    }
	
	// if none of those worked, we just leave the window where it got placed out of the nib
}

QWidget *QtSLiMWindow::imageWindowWithPath(const QString &path)
{
    QImage image(path);
    QFileInfo fileInfo(path);
    
    // We have an image; note that it might be a "null image", however
    bool null = image.isNull();
    int window_width = (null ? 288 : image.width());
    int window_height = (null ? 288 : image.height());
    
    QWidget *image_window = new QWidget(this, Qt::Window | Qt::Tool);    // the image window has us as a parent, but is still a standalone window
    
    image_window->setWindowTitle(fileInfo.fileName());
    image_window->setFixedSize(window_width, window_height);
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    image_window->setWindowIcon(qtSLiMAppDelegate->genericDocumentIcon());    // doesn't seem to quite work; we get the SLiM document icon, inherited from parent presumably
#endif
    image_window->setWindowFilePath(path);
    
    // Make the image view
    QLabel *imageView = new QLabel();
    
    imageView->setStyleSheet("QLabel { background-color : white; }");
    imageView->setBackgroundRole(QPalette::Base);
    imageView->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageView->setScaledContents(true);
    imageView->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    
    if (null)
        imageView->setText("No image data");
    else
        imageView->setPixmap(QPixmap::fromImage(image));
    
    // Install imageView in the window
    QVBoxLayout *topLayout = new QVBoxLayout;
    
    image_window->setLayout(topLayout);
    topLayout->setMargin(0);
    topLayout->setSpacing(0);
    topLayout->addWidget(imageView);
    
    // Make a file system watcher to update us when the image changes
    QFileSystemWatcher *watcher = new QFileSystemWatcher(QStringList(path), image_window);
    
    connect(watcher, &QFileSystemWatcher::fileChanged, imageView, [imageView](const QString &watched_path) {
        QImage watched_image(watched_path);
        
        if (watched_image.isNull()) {
            imageView->setText("No image data");
        } else {
            imageView->setPixmap(QPixmap::fromImage(watched_image));
            imageView->window()->setFixedSize(watched_image.width(), watched_image.height());
        }
    });
    
    // Set up a context menu for copy/open
    QMenu *contextMenu = new QMenu("image_menu", imageView);
    contextMenu->addAction("Copy Image", this, [path]() {
        QImage watched_image(path);     // get the current image from the filesystem
        QClipboard *clipboard = QGuiApplication::clipboard();
        clipboard->setImage(watched_image);
    });
    contextMenu->addAction("Copy File Path", this, [path]() {
        QClipboard *clipboard = QGuiApplication::clipboard();
        clipboard->setText(path);
    });
    
    // Reveal in Finder / Show in Explorer: see https://stackoverflow.com/questions/3490336
    // Note there is no good solution on Linux, so we do "Open File" instead
    #if defined(Q_OS_MACOS)
    contextMenu->addAction("Reveal in Finder", this, [path]() {
        const QFileInfo fileInfo(path);
        QStringList scriptArgs;
        scriptArgs << QLatin1String("-e") << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"").arg(fileInfo.canonicalFilePath());
        QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
        scriptArgs.clear();
        scriptArgs << QLatin1String("-e") << QLatin1String("tell application \"Finder\" to activate");
        QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
    });
    #elif defined(Q_WS_WIN)
    contextMenu->addAction("Show in Explorer", [path]() {
        const QFileInfo fileInfo(path);
        const FileName explorer = Environment::systemEnvironment().searchInPath(QLatin1String("explorer.exe"));
        if (explorer.isEmpty())
            qApp->beep();
        QStringList param;
        if (!fileInfo.isDir())
            param += QLatin1String("/select,");
        param += QDir::toNativeSeparators(fileInfo.canonicalFilePath());
        QProcess::startDetached(explorer.toString() + " " + param);
    });
    #else
    contextMenu->addAction("Open File", [path]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    #endif
    
    imageView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(imageView, &QLabel::customContextMenuRequested, imageView, [imageView, contextMenu](const QPoint &pos) {
        // Run the context menu if we have an image (in which case the text length is zero)
        if (imageView->text().length() == 0)
            contextMenu->exec(imageView->mapToGlobal(pos));
    });
    
    // Position the window nicely
    positionNewSubsidiaryWindow(image_window);
    
    // make window actions for all global menu items
    // we do NOT need to do this, because we use Qt::Tool; Qt will use our parent window's shortcuts
    //qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
    
    image_window->setAttribute(Qt::WA_DeleteOnClose, true);
    
    return image_window;
}

QWidget *QtSLiMWindow::graphWindowWithView(QtSLiMGraphView *graphView)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    // Make a new window to show the graph
    QWidget *graph_window = new QWidget(this, Qt::Window | Qt::Tool);    // the graph window has us as a parent, but is still a standalone window
    QString title = graphView->graphTitle();
    
    graph_window->setWindowTitle(title);
    graph_window->setMinimumSize(250, 250);
    graph_window->resize(300, 300);
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    graph_window->setWindowIcon(QIcon());
#endif
    
    // Install graphView in the window
    QVBoxLayout *topLayout = new QVBoxLayout;
    
    graph_window->setLayout(topLayout);
    topLayout->setMargin(0);
    topLayout->setSpacing(0);
    topLayout->addWidget(graphView);
    
    // Add a horizontal layout at the bottom, for popup buttons and such added by the graph
    QHBoxLayout *buttonLayout = nullptr;
    
    {
        buttonLayout = new QHBoxLayout;
        
        buttonLayout->setMargin(5);
        buttonLayout->setSpacing(5);
        topLayout->addLayout(buttonLayout);
        
        QLabel *speciesLabel = new QLabel();
        speciesLabel->setText("");
        buttonLayout->addWidget(speciesLabel);
        speciesLabel->setHidden(true);
        
        QSpacerItem *rightSpacer = new QSpacerItem(16, 5, QSizePolicy::Expanding, QSizePolicy::Minimum);
        buttonLayout->addItem(rightSpacer);
        
        // this code is based on the creation of executeScriptButton in ui_QtSLiMEidosConsole.h
        QtSLiMPushButton *actionButton = new QtSLiMPushButton(graph_window);
        actionButton->setObjectName(QString::fromUtf8("actionButton"));
        actionButton->setMinimumSize(QSize(20, 20));
        actionButton->setMaximumSize(QSize(20, 20));
        actionButton->setFocusPolicy(Qt::NoFocus);
        QIcon icon4;
        icon4.addFile(QtSLiMImagePath("action", false), QSize(), QIcon::Normal, QIcon::Off);
        icon4.addFile(QtSLiMImagePath("action", true), QSize(), QIcon::Normal, QIcon::On);
        actionButton->setIcon(icon4);
        actionButton->setIconSize(QSize(20, 20));
        actionButton->qtslimSetBaseName("action");
        actionButton->setCheckable(true);
        actionButton->setFlat(true);
#if QT_CONFIG(tooltip)
        actionButton->setToolTip("<html><head/><body><p>configure graph</p></body></html>");
#endif // QT_CONFIG(tooltip)
        buttonLayout->addWidget(actionButton);
        
        connect(actionButton, &QPushButton::pressed, graphView, [actionButton, graphView]() { actionButton->qtslimSetHighlight(true); graphView->actionButtonRunMenu(actionButton); });
        connect(actionButton, &QPushButton::released, graphView, [actionButton]() { actionButton->qtslimSetHighlight(false); });
        
        actionButton->setEnabled(!invalidSimulation() && (community->Tick() > 0));
    }
    
    // Give the graph view a chance to do something with the window it's now in
    graphView->addedToWindow();
    
    // force geometry calculation, which is lazy
    graph_window->setAttribute(Qt::WA_DontShowOnScreen, true);
    graph_window->show();
    graph_window->hide();
    graph_window->setAttribute(Qt::WA_DontShowOnScreen, false);
    
    // If we added a button layout, give it room so the graph area is still square
    // Note this has to happen after forcing layout calculations
    if (buttonLayout)
    {
        QSize contentSize = graph_window->size();
        QSize minSize = graph_window->minimumSize();
        int buttonLayoutHeight = buttonLayout->geometry().height();
        
        contentSize.setHeight(contentSize.height() + buttonLayoutHeight);
        graph_window->resize(contentSize);
        
        minSize.setHeight(minSize.height() + buttonLayoutHeight);
        graph_window->setMinimumSize(minSize);
    }
    
    // Position the window nicely
    positionNewSubsidiaryWindow(graph_window);
    
    // make window actions for all global menu items
    // we do NOT need to do this, because we use Qt::Tool; Qt will use our parent window's shortcuts
    //qtSLiMAppDelegate->addActionsForGlobalMenuItems(graph_window);
    
    graph_window->setAttribute(Qt::WA_DeleteOnClose, true);
    
    return graph_window;
}

void QtSLiMWindow::graphPopupButtonRunMenu(void)
{
	bool disableAll = false;
    Species *displaySpecies = focalDisplaySpecies();
	
	// When the simulation is not valid and initialized, the context menu is disabled
	if (invalidSimulation_ || !displaySpecies)
		disableAll = true;
    
    QMenu contextMenu("graph_menu", this);
    
    QAction *graph1DFreqSpectrum = contextMenu.addAction("Graph 1D Population SFS");
    graph1DFreqSpectrum->setEnabled(!disableAll);
    
    QAction *graph1DSampleSFS = contextMenu.addAction("Graph 1D Sample SFS");
    graph1DSampleSFS->setEnabled(!disableAll);
    
    contextMenu.addSeparator();
    
    QAction *graph2DFreqSpectrum = contextMenu.addAction("Graph 2D Population SFS");
    graph2DFreqSpectrum->setEnabled(!disableAll);
    
    QAction *graph2DSampleSFS = contextMenu.addAction("Graph 2D Sample SFS");
    graph2DSampleSFS->setEnabled(!disableAll);
    
    contextMenu.addSeparator();
    
    QAction *graphMutFreqTrajectories = contextMenu.addAction("Graph Mutation Frequency Trajectories");
    graphMutFreqTrajectories->setEnabled(!disableAll);
    
    QAction *graphMutLossTimeHist = contextMenu.addAction("Graph Mutation Loss Time Histogram");
    graphMutLossTimeHist->setEnabled(!disableAll);
    
    QAction *graphMutFixTimeHist = contextMenu.addAction("Graph Mutation Fixation Time Histogram");
    graphMutFixTimeHist->setEnabled(!disableAll);
    
    contextMenu.addSeparator();
    
    QAction *graphPopFitnessDist = contextMenu.addAction("Graph Population Fitness Distribution");
    graphPopFitnessDist->setEnabled(!disableAll);
    
    QAction *graphSubpopFitnessDists = contextMenu.addAction("Graph Subpopulation Fitness Distributions");
    graphSubpopFitnessDists->setEnabled(!disableAll);
    
    QAction *graphFitnessVsTime = contextMenu.addAction("Graph Fitness ~ Time");
    graphFitnessVsTime->setEnabled(!disableAll);
    
    contextMenu.addSeparator();
    
    QAction *graphAgeDistribution = contextMenu.addAction("Graph Age Distribution");
    graphAgeDistribution->setEnabled(!disableAll);
    
    QAction *graphLifetimeReproduction = contextMenu.addAction("Graph Lifetime Reproductive Output");
    graphLifetimeReproduction->setEnabled(!disableAll);
    
    QAction *graphPopSizeVsTime = contextMenu.addAction("Graph Population Size ~ Time");
    graphPopSizeVsTime->setEnabled(!disableAll);
    
    QAction *graphPopVisualization = contextMenu.addAction("Graph Population Visualization");
    graphPopVisualization->setEnabled(!disableAll);
    
    contextMenu.addSeparator();
    
    QAction *graphMultispeciesPopSizeVsTime = contextMenu.addAction("Multispecies Population Size ~ Time");
    graphMultispeciesPopSizeVsTime->setEnabled(!invalidSimulation_);
    
    contextMenu.addSeparator();
    
    QAction *createHaplotypePlot = contextMenu.addAction("Create Haplotype Plot");
    createHaplotypePlot->setEnabled(!disableAll && !continuousPlayOn_ && displaySpecies && displaySpecies->population_.subpops_.size());
    
    // Run the context menu synchronously
    QPoint mousePos = QCursor::pos();
    QAction *action = contextMenu.exec(mousePos);
    
    if (action && !invalidSimulation_)
    {
        displaySpecies = focalDisplaySpecies();     // might change while the menu is running...
        
        if (action == createHaplotypePlot)
        {
            if (!continuousPlayOn_ && displaySpecies && displaySpecies->population_.subpops_.size())
            {
                isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
                
                QtSLiMHaplotypeManager::CreateHaplotypePlot(this);
            }
            else
            {
                qApp->beep();
            }
        }
        else
        {
            QtSLiMGraphView *graphView = nullptr;
            
            if (displaySpecies)
            {
                if (action == graph1DFreqSpectrum)
                    graphView = new QtSLiMGraphView_1DPopulationSFS(this, this);
                if (action == graph1DSampleSFS)
                    graphView = new QtSLiMGraphView_1DSampleSFS(this, this);
                if (action == graph2DFreqSpectrum)
                    graphView = new QtSLiMGraphView_2DPopulationSFS(this, this);
                if (action == graph2DSampleSFS)
                    graphView = new QtSLiMGraphView_2DSampleSFS(this, this);
                if (action == graphMutFreqTrajectories)
                    graphView = new QtSLiMGraphView_FrequencyTrajectory(this, this);
                if (action == graphMutLossTimeHist)
                    graphView = new QtSLiMGraphView_LossTimeHistogram(this, this);
                if (action == graphMutFixTimeHist)
                    graphView = new QtSLiMGraphView_FixationTimeHistogram(this, this);
                if (action == graphPopFitnessDist)
                    graphView = new QtSLiMGraphView_PopFitnessDist(this, this);
                if (action == graphSubpopFitnessDists)
                    graphView = new QtSLiMGraphView_SubpopFitnessDists(this, this);
                if (action == graphFitnessVsTime)
                    graphView = new QtSLiMGraphView_FitnessOverTime(this, this);
                if (action == graphAgeDistribution)
                    graphView = new QtSLiMGraphView_AgeDistribution(this, this);
                if (action == graphLifetimeReproduction)
                    graphView = new QtSLiMGraphView_LifetimeReproduction(this, this);
                if (action == graphPopSizeVsTime)
                    graphView = new QtSLiMGraphView_PopSizeOverTime(this, this);
                if (action == graphPopVisualization)
                    graphView = new QtSLiMGraphView_PopulationVisualization(this, this);
            }
            
            if (action == graphMultispeciesPopSizeVsTime)
                graphView = new QtSLiMGraphView_MultispeciesPopSizeOverTime(this, this);
            
            if (graphView)
            {
                QWidget *graphWindow = graphWindowWithView(graphView);
                
                if (graphWindow)
                {
                    graphWindow->show();
                    graphWindow->raise();
                    graphWindow->activateWindow();
                }
            }
            else
            {
                qApp->beep();
            }                
        }
    }
    
    // This is not called by Qt, for some reason (nested tracking loops?), so we call it explicitly
    graphPopupButtonReleased();
}

void QtSLiMWindow::changeDirectoryClicked(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    QFileDialog dialog(this);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setViewMode(QFileDialog::List);
    dialog.setDirectory(QString::fromUtf8(sim_working_dir.c_str()));
    
    // FIXME could use QFileDialog::open() to get a sheet instead of an app-model panel...
    if (dialog.exec())
    {
        QStringList fileNames = dialog.selectedFiles();
        
        if (fileNames.size() == 1)
        {
            sim_working_dir = fileNames[0].toUtf8().constData();
            sim_requested_working_dir = sim_working_dir;
        }
    }
}

void QtSLiMWindow::subpopSelectionDidChange(const QItemSelection & /* selected */, const QItemSelection & /* deselected */)
{
    if (!invalidSimulation_ && !reloadingSubpopTableview)
    {
        QItemSelectionModel *selectionModel = ui->subpopTableView->selectionModel();
        QModelIndexList selectedRows = selectionModel->selectedRows();
        std::vector<Subpopulation *> subpops = listedSubpopulations();
        size_t subpopCount = subpops.size();
        
        // first get the state of each row, for algorithmic convenience
        std::vector<bool> rowSelectedState(subpopCount, false);
        
        for (QModelIndex &modelIndex : selectedRows)
            rowSelectedState[static_cast<size_t>(modelIndex.row())] = true;
        
        // then loop through subpops and update their selected state
        auto subpopIter = subpops.begin();
        //bool all_selected = true;
        bool none_selected = true;
        
        for (size_t i = 0; i < subpopCount; ++i)
        {
            (*subpopIter)->gui_selected_ = rowSelectedState[i];
            
            if ((*subpopIter)->gui_selected_)
                none_selected = false;
            //else
            //    all_selected = false;
            
            subpopIter++;
        }
        
        // If the selection has changed, that means that the mutation tallies need to be recomputed
        for (Species *species : community->AllSpecies())
            species->population_.TallyMutationReferencesAcrossPopulation(true);
        
        // It's a bit hard to tell for sure whether we need to update or not, since a selected subpop might have been removed from the tableview;
        // selection changes should not happen often, so we can just always update, I think.
        ui->individualsWidget->update();
        
        for (QtSLiMChromosomeWidget *zoomedWidget : chromosomeZoomedWidgets)
            zoomedWidget->update();     // was setNeedsDisplayInInterior, which would be more minimal
        
        // We don't want to allow an empty selection, maybe; if we are now in that state, and there are subpops to select, select them all
        // See also updateAfterTickFull() which also needs to do this
        if (none_selected && subpops.size())
            ui->subpopTableView->selectAll();
    }
}
































