//
//  QtSLiMWindow.cpp
//  SLiM
//
//  Created by Ben Haller on 7/11/2019.
//  Copyright (c) 2019-2020 Philipp Messer.  All rights reserved.
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
#include "QtSLiMEidosPrettyprinter.h"
#include "QtSLiMPreferences.h"
#include "QtSLiMFindPanel.h"
#include "QtSLiMHelpWindow.h"
#include "QtSLiMEidosConsole.h"
#include "QtSLiMVariableBrowser.h"
#include "QtSLiMTablesDrawer.h"
#include "QtSLiMSyntaxHighlighting.h"
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
#include "QtSLiMGraphView_PopulationVisualization.h"
#include "QtSLiMGraphView_FitnessOverTime.h"
#include "QtSLiMGraphView_PopSizeOverTime.h"
#include "QtSLiMGraphView_FrequencyTrajectory.h"
#include "QtSLiMGraphView_PopFitnessDist.h"
#include "QtSLiMGraphView_SubpopFitnessDists.h"
#include "QtSLiMHaplotypeManager.h"

#include <QCoreApplication>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QtDebug>
#include <QMessageBox>
#include <QTextEdit>
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

#include <unistd.h>
#include <sys/stat.h>

#include "individual.h"
#include "eidos_test.h"
#include "slim_test.h"


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
                "1 {\n"
                "	sim.addSubpop(\"p1\", 500);\n"
                "}\n"
                "\n"
                "// output samples of 10 genomes periodically, all fixed mutations at end\n"
                "1000 late() { p1.outputSample(10); }\n"
                "2000 late() { p1.outputSample(10); }\n"
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


QtSLiMWindow::QtSLiMWindow(QtSLiMWindow::ModelType modelType) : QMainWindow(nullptr), ui(new Ui::QtSLiMWindow)
{
    init();
    setCurrentFile(QString());
    
    // set up the initial script
    std::string untitledScriptString = (modelType == QtSLiMWindow::ModelType::WF) ? defaultWFScriptString() : defaultNonWFScriptString();
    ui->scriptTextEdit->setPlainText(QString::fromStdString(untitledScriptString));
    
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
    ui->scriptTextEdit->setPlainText(recipeScript);
    setScriptStringAndInitializeSimulation(recipeScript.toUtf8().constData());
    
    // Update all our UI to reflect the current state of the simulation
    updateAfterTickFull(true);
    resetSLiMChangeCount();     // no recycle change count; the current model is correct
    setWindowModified(false);    // untitled windows consider themselves unmodified
}

void QtSLiMWindow::init(void)
{
#ifdef __APPLE__
    // On macOS we want to stay running when the last main window closes, following platform UI guidelines.
    // However, Qt's treatment of the menu bar seems to be a bit buggy unless a main window exists.  That
    // main window can be hidden; it just needs to exist.  So here we just allow our main window(s) to leak,
    // so that Qt is happy.  This sucks, obviously, but really it seems unlikely to matter.  The window will
    // notice its zombified state when it is closed, and will free resources and mark itself as a zombie so
    // it doesn't get included in the Window menu, etc.
    // BCH 9/23/2020: I am forced not to do this by a crash on quit, so we continue to delete on close for
    // now (and we continue to quit when the last window closes).  See QTBUG-86874 and QTBUG-86875.  If a
    // fix or workaround for either of those issues is found, the code is otherwise ready to transition to
    // having QtSLiM stay open after the last window closes, on macOS.  Search for those bug numbers to find
    // the other spots in the code related to this mess.
    // BCH 9/24/2020: Note that QTBUG-86875 is fixed in 5.15.1, but we don't want to require that.
    setAttribute(Qt::WA_DeleteOnClose);
#else
    setAttribute(Qt::WA_DeleteOnClose);
#endif
    isUntitled = true;
    isRecipe = false;
    
    // create the window UI
    ui->setupUi(this);
    interpolateSplitters();
    initializeUI();
    
    // with everything built, mark ourselves as transient (recipes and files will mark this false after us)
    isTransient = true;
    
    // wire up our continuous play and generation play timers
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
        sim_working_dir = Eidos_ResolvedPath("~/Desktop");
    
    // Check that our chosen working directory actually exists; if not, use ~
    struct stat buffer;
    
    if (stat(sim_working_dir.c_str(), &buffer) != 0)
        sim_working_dir = Eidos_ResolvedPath("~");
    
    sim_requested_working_dir = sim_working_dir;	// return to the working dir on recycle unless the user overrides it
    
    // Wire up things that set the window to be modified.
    connect(ui->scriptTextEdit, &QTextEdit::textChanged, this, &QtSLiMWindow::documentWasModified);
    connect(ui->scriptTextEdit, &QTextEdit::textChanged, this, &QtSLiMWindow::scriptTexteditChanged);
    
    // Watch for changes to the selection in the population tableview
    connect(ui->subpopTableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &QtSLiMWindow::subpopSelectionDidChange);
    
    // Watch for changes to the selection in the chromosome view
    connect(ui->chromosomeOverview, &QtSLiMChromosomeWidget::selectedRangeChanged, this, [this]() { emit controllerSelectionChanged(); });
    
    // Ensure that the generation lineedit does not have the initial keyboard focus and has no selection; hard to do!
    ui->generationLineEdit->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    QTimer::singleShot(0, [this]() { ui->generationLineEdit->setFocusPolicy(Qt::FocusPolicy::StrongFocus); });
    
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
    
    ui->line->setParent(nullptr);
    ui->line = nullptr;
    
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
        QtSLiMPlayControlsLayout *newPlayControlsLayout = new QtSLiMPlayControlsLayout();
        int indexOfPlayControlsLayout = -1;
        
        // QLayout::indexOf(QLayoutItem *layoutItem) wasn't added until 5.12, oddly
        for (int i = 0; i < ui->topRightLayout->count(); ++i)
            if (ui->topRightLayout->itemAt(i) == ui->playControlsLayout)
                indexOfPlayControlsLayout = i;
        
        if (indexOfPlayControlsLayout >= 0)
        {
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
    ui->showChromosomeMapsButton->setChecked(zoomedChromosomeShowsRateMaps);
    ui->showGenomicElementsButton->setChecked(zoomedChromosomeShowsGenomicElements);
    ui->showMutationsButton->setChecked(zoomedChromosomeShowsMutations);
    ui->showFixedSubstitutionsButton->setChecked(zoomedChromosomeShowsFixedSubstitutions);
    
    showChromosomeMapsToggled();
    showGenomicElementsToggled();
    showMutationsToggled();
    showFixedSubstitutionsToggled();
    
    // Set up the population table view
    populationTableModel_ = new QtSLiMPopulationTableModel(this);
    ui->subpopTableView->setModel(populationTableModel_);
    ui->subpopTableView->setHorizontalHeader(new QtSLiMPopulationTableHeaderView(Qt::Orientation::Horizontal, this));
    
    QHeaderView *popTableHHeader = ui->subpopTableView->horizontalHeader();
    QHeaderView *popTableVHeader = ui->subpopTableView->verticalHeader();
    
    popTableHHeader->setMinimumSectionSize(1);
    popTableVHeader->setMinimumSectionSize(1);
    
    popTableHHeader->resizeSection(0, 35);
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
#ifdef __APPLE__
    headerFont.setPointSize(11);
    cellFont.setPointSize(11);
#else
    headerFont.setPointSize(8);
    cellFont.setPointSize(8);
#endif
    popTableHHeader->setFont(headerFont);
    ui->subpopTableView->setFont(cellFont);
    
    popTableVHeader->setSectionResizeMode(QHeaderView::Fixed);
    popTableVHeader->setDefaultSectionSize(18);
    
    // Set up our chromosome views to show the proper stuff
	ui->chromosomeOverview->setReferenceChromosomeView(nullptr);
	ui->chromosomeOverview->setSelectable(true);
	ui->chromosomeOverview->setShouldDrawGenomicElements(true);
	ui->chromosomeOverview->setShouldDrawMutations(false);
	ui->chromosomeOverview->setShouldDrawFixedSubstitutions(false);
	ui->chromosomeOverview->setShouldDrawRateMaps(false);
	
	ui->chromosomeZoomed->setReferenceChromosomeView(ui->chromosomeOverview);
	ui->chromosomeZoomed->setSelectable(false);
	ui->chromosomeZoomed->setShouldDrawGenomicElements(ui->showGenomicElementsButton->isChecked());
	ui->chromosomeZoomed->setShouldDrawMutations(ui->showMutationsButton->isChecked());
	ui->chromosomeZoomed->setShouldDrawFixedSubstitutions(ui->showFixedSubstitutionsButton->isChecked());
	ui->chromosomeZoomed->setShouldDrawRateMaps(ui->showChromosomeMapsButton->isChecked());
    
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

void QtSLiMWindow::displayStartupMessage(void)
{
    // Set the initial status bar message; called by QtSLiMAppDelegate::appDidFinishLaunching()
    QString message("<font color='#555555' style='font-size: 11px;'>SLiM %1, %2 build.</font>");
    
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
    if (sim)
    {
        delete sim;
        sim = nullptr;
    }
    if (slimgui)
	{
		delete slimgui;
		slimgui = nullptr;
	}

    Eidos_FreeRNG(sim_RNG);
    
    // The console is owned by us, and it owns the variable browser.  Since the parent
    // relationships are set up, they should be released by Qt automatically.
    if (consoleController)
    {
        //if (consoleController->browserController)
        //  consoleController->browserController->hide();
        consoleController->hide();
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
            
            genomicElementColorRegistry.insert(std::pair<slim_objectid_t, QColor>(elementTypeID, *elementColor));
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

//
//  Document support
//

void QtSLiMWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave())
    {
        // We used to save the window size/position here, but now that is done in moveEvent() / resizeEvent()
        event->accept();
        
        // We no longer get freed when we close, because we need to stick around to make the global menubar
        // work; see QtSLiMWindow::init().  So when we're closing, we now free up the resources we hold.
        // This should close and free all auxiliary windows, while leaving us alive.
        // BCH 9/23/2020: I am forced not to do this by a crash on quit, so we continue to delete on close for
        // now (and we continue to quit when the last window closes).  See QTBUG-86874 and QTBUG-86875.  If a
        // fix or workaround for either of those issues is found, the code is otherwise ready to transition to
        // having QtSLiM stay open after the last window closes, on macOS.  Search for those bug numbers to find
        // the other spots in the code related to this mess.
        // BCH 9/24/2020: Note that QTBUG-86875 is fixed in 5.15.1, but we don't want to require that.
        
        // This function was still under development, and has been removed; the idea was to tear down unneeded
        // UI on the window being permanently hidden, like graph views, Eidos console, object tables window,
        // variable browser, haplotype plots... but there wasn't really any useful, debugged code yet.
        //invalidate();
    }
    else
    {
        event->ignore();
    }
}

void QtSLiMWindow::moveEvent(QMoveEvent *event)
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
    
    QWidget::moveEvent(event);
}

void QtSLiMWindow::resizeEvent(QResizeEvent *event)
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
    
    QWidget::resizeEvent(event);
}

void QtSLiMWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    
    if (!testAttribute(Qt::WA_DontShowOnScreen))
    {
        //qDebug() << "showEvent() : done positioning";
        donePositioning_ = true;
    }
}

bool QtSLiMWindow::windowIsReuseable(void)
{
    return (isUntitled && !isRecipe && isTransient && (slimChangeCount == 0) && !isWindowModified());
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
    if (!isWindowModified())
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

    QTextStream out(&file);
    out << ui->scriptTextEdit->toPlainText();

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
    setWindowModified(true);
}

void QtSLiMWindow::tile(const QMainWindow *previous)
{
    if (!previous)
        return;
    int topFrameWidth = previous->geometry().top() - previous->pos().y();
    if (!topFrameWidth)
        topFrameWidth = 40;
    const QPoint pos = previous->pos() + 2 * QPoint(topFrameWidth, topFrameWidth);
    if (QApplication::desktop()->availableGeometry(this).contains(rect().bottomRight() + pos))
        move(pos);
}


//
//  Simulation state
//

std::vector<Subpopulation*> QtSLiMWindow::selectedSubpopulations(void)
{
    std::vector<Subpopulation*> selectedSubpops;
	
	if (!invalidSimulation() && sim)
	{
		Population &population = sim->population_;
        
        for (auto popIter : population.subpops_)
		{
			Subpopulation *subpop = popIter.second;
			
			if (subpop->gui_selected_)
				selectedSubpops.emplace_back(subpop);
		}
	}
	
	return selectedSubpops;
}

void QtSLiMWindow::chromosomeSelection(bool *p_hasSelection, slim_position_t *p_selectionFirstBase, slim_position_t *p_selectionLastBase)
{
    QtSLiMChromosomeWidget *chromosomeOverview = ui->chromosomeOverview;
    
    if (p_hasSelection)
        *p_hasSelection = chromosomeOverview->hasSelection();
    
    QtSLiMRange selRange = chromosomeOverview->getSelectedRange();
    
    if (p_selectionFirstBase)
        *p_selectionFirstBase = selRange.location;
    if (p_selectionLastBase)
        *p_selectionLastBase = selRange.location + selRange.length - 1;
}

const std::vector<slim_objectid_t> &QtSLiMWindow::chromosomeDisplayMuttypes(void)
{
    QtSLiMChromosomeWidget *chromosomeZoomed = ui->chromosomeZoomed;
    
    return chromosomeZoomed->displayMuttypes();
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

void QtSLiMWindow::setGenerationPlayOn(bool p_flag)
{
    if (generationPlayOn_ != p_flag)
    {
        generationPlayOn_ = p_flag;
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

void QtSLiMWindow::showTerminationMessage(QString terminationMessage)
{
    //qDebug() << terminationMessage;
    
    // Depending on the circumstances of the error, we might be able to select a range in our input file to show what caused the error
	if (!changedSinceRecycle())
		ui->scriptTextEdit->selectErrorRange();
    
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
        QString message = QString::fromStdString(terminationMessage);

        gEidosTermination.clear();
        gEidosTermination.str("");

        emit terminationWithMessage(message);
        
        // Now we need to clean up so we are in a displayable state.  Note that we don't even attempt to dispose
        // of the old simulation object; who knows what state it is in, touching it might crash.
        sim = nullptr;
        slimgui = nullptr;

        Eidos_FreeRNG(sim_RNG);

        setReachedSimulationEnd(true);
        setInvalidSimulation(true);
    }
}

void QtSLiMWindow::startNewSimulationFromScript(void)
{
    if (sim)
    {
        delete sim;
        sim = nullptr;
    }
    if (slimgui)
    {
        delete slimgui;
        slimgui = nullptr;
    }

    // Free the old simulation RNG and let SLiM make one for us
    Eidos_FreeRNG(sim_RNG);

    if (EIDOS_GSL_RNG)
        qDebug() << "gEidos_RNG already set up in startNewSimulationFromScript!";

    std::istringstream infile(scriptString);

    try
    {
        sim = new SLiMSim(infile);
        sim->InitializeRNGFromSeed(nullptr);

        // We take over the RNG instance that SLiMSim just made, since each SLiMgui window has its own RNG
        sim_RNG = gEidos_RNG;
        EIDOS_BZERO(&gEidos_RNG, sizeof(Eidos_RNG_State));

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
        if (sim)
            sim->simulation_valid_ = false;
        setReachedSimulationEnd(true);
        checkForSimulationTermination();
    }

    if (sim)
    {
        // make a new SLiMgui instance to represent SLiMgui in Eidos
        slimgui = new SLiMgui(*sim, this);

        // set up the "slimgui" symbol for it immediately
        sim->simulation_constants_->InitializeConstantSymbolEntry(slimgui->SymbolTableEntry());
    }
}

void QtSLiMWindow::setScriptStringAndInitializeSimulation(std::string string)
{
    scriptString = string;
    startNewSimulationFromScript();
}

QtSLiMGraphView *QtSLiMWindow::graphViewForGraphWindow(QWidget *window)
{
    if (window)
    {
        QLayout *layout = window->layout();
        
        if (layout && (layout->count() > 0))
        {
            QLayoutItem *item = layout->itemAt(0);
            
            if (item)
                return qobject_cast<QtSLiMGraphView *>(item->widget());
        }
    }
    return nullptr;
}

void QtSLiMWindow::updateOutputTextView(void)
{
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
        // which has a complex solution involving subclassing QTextEdit... sigh...
        ui->outputTextEdit->moveCursor(QTextCursor::End);
        ui->outputTextEdit->insertPlainText(str);
        ui->outputTextEdit->moveCursor(QTextCursor::End);
        
		//if ([[NSUserDefaults standardUserDefaults] boolForKey:defaultsSyntaxHighlightOutputKey])
		//	[outputTextView recolorAfterChanges];
		
		// if the user was scrolled to the bottom, we keep them there; otherwise, we let them stay where they were
		//if (scrolledToBottom)
		//	[outputTextView scrollRangeToVisible:NSMakeRange([[outputTextView string] length], 0)];
		
		// clear any error flags set on the stream and empty out its string so it is ready to receive new output
		gSLiMOut.clear();
		gSLiMOut.str("");
	}
}

void QtSLiMWindow::updateGenerationCounter(void)
{
    if (!invalidSimulation_)
	{
		if (sim->generation_ == 0)
            ui->generationLineEdit->setText("initialize()");
		else
            ui->generationLineEdit->setText(QString::number(sim->generation_));
	}
	else
        ui->generationLineEdit->setText("");
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
	
    // Flush any buffered output to files every full update, so that the user sees changes to the files without too much delay
	if (fullUpdate)
		Eidos_FlushFiles();
	
	// Check whether the simulation has terminated due to an error; if so, show an error message with a delayed perform
	checkForSimulationTermination();
	
	// The rest of the code here needs to be careful about the invalid state; we do want to update our controls when invalid, but sim is nil.
	bool invalid = invalidSimulation();
	
	if (fullUpdate)
	{
		// FIXME it would be good for this updating to be minimal; reloading the tableview every time, etc., is quite wasteful...
		updateOutputTextView();
		
		// Reloading the subpop tableview is tricky, because we need to preserve the selection across the reload, while also noting that the selection is forced
		// to change when a subpop goes extinct.  The current selection is noted in the gui_selected_ ivar of each subpop.  So what we do here is reload the tableview
		// while suppressing our usual update of our selection state, and then we try to re-impose our selection state on the new tableview content.  If a subpop
		// went extinct, we will fail to notice the selection change; but that is OK, since we force an update of populationView and chromosomeZoomed below anyway.
		reloadingSubpopTableview = true;
        populationTableModel_->reloadTable();
		
		if (invalid || !sim)
		{
            ui->subpopTableView->selectionModel()->clear();
		}
		else
		{
            ui->subpopTableView->selectionModel()->reset();
            
			Population &population = sim->population_;
			int subpopCount = static_cast<int>(population.subpops_.size());
			auto popIter = population.subpops_.begin();
			
			for (int i = 0; i < subpopCount; ++i)
			{
				if (popIter->second->gui_selected_)
                {
                    QModelIndex modelIndex = ui->subpopTableView->model()->index(i, 0);
                    
                    ui->subpopTableView->selectionModel()->select(modelIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
                }
				
				popIter++;
			}
		}
		
		reloadingSubpopTableview = false;
	}
	
	// Now update our other UI, some of which depends upon the state of subpopTableView
    ui->individualsWidget->update();
    ui->chromosomeZoomed->update();
	
	if (fullUpdate)
		updateGenerationCounter();
    
    if (fullUpdate)
    {
        double elapsedTimeInSLiM = elapsedCPUClock_ / static_cast<double>(CLOCKS_PER_SEC);
        
        if (elapsedTimeInSLiM == 0.0)
            ui->statusBar->clearMessage();
        else
        {
            QString message("<font color='#555555' style='font-size: 11px;'><tt>%1</tt> CPU seconds elapsed inside SLiM; <tt>%2</tt> mutations segregating, <tt>%3</tt> substitutions.</font>");
            
            if (sim)
            {
                int registry_size;
                sim->population_.MutationRegistry(&registry_size);
                
                ui->statusBar->showMessage(message.arg(elapsedTimeInSLiM, 0, 'f', 6)
                                           .arg(registry_size)
                                           .arg(sim->population_.substitutions_.size()));
            }
            else
                ui->statusBar->showMessage(message.arg(elapsedTimeInSLiM, 0, 'f', 6));
        }
	}
    
	// Update stuff that only needs updating when the script is re-parsed, not after every tick
	if (invalid || sim->mutation_types_changed_)
	{
        if (tablesDrawerController && tablesDrawerController->mutTypeTableModel_)
            tablesDrawerController->mutTypeTableModel_->reloadTable();
		
		if (sim)
			sim->mutation_types_changed_ = false;
	}
	
	if (invalid || sim->genomic_element_types_changed_)
	{
        if (tablesDrawerController && tablesDrawerController->geTypeTableModel_)
            tablesDrawerController->geTypeTableModel_->reloadTable();
		
		if (sim)
			sim->genomic_element_types_changed_ = false;
	}
	
	if (invalid || sim->interaction_types_changed_)
	{
        if (tablesDrawerController && tablesDrawerController->interactionTypeTableModel_)
            tablesDrawerController->interactionTypeTableModel_->reloadTable();
		
		if (sim)
			sim->interaction_types_changed_ = false;
	}
	
	if (invalid || sim->scripts_changed_)
	{
        if (tablesDrawerController && tablesDrawerController->eidosBlockTableModel_)
            tablesDrawerController->eidosBlockTableModel_->reloadTable();
		
		if (sim)
			sim->scripts_changed_ = false;
	}
	
	if (invalid || sim->chromosome_changed_)
	{
		ui->chromosomeOverview->restoreLastSelection();
		ui->chromosomeOverview->update();
		
		if (sim)
			sim->chromosome_changed_ = false;
	}
	
	// Update graph windows as well; this will usually trigger an update() but may do other updating work as well
	if (fullUpdate)
        emit controllerUpdatedAfterTick();
}

void QtSLiMWindow::updatePlayButtonIcon(bool pressed)
{
    bool highlighted = ui->playButton->isChecked() ^ pressed;
    
    ui->playButton->setIcon(QIcon(highlighted ? ":/buttons/play_H.png" : ":/buttons/play.png"));
}

void QtSLiMWindow::updateProfileButtonIcon(bool pressed)
{
    bool highlighted = ui->profileButton->isChecked() ^ pressed;
    
    if (profilePlayOn_)
        ui->profileButton->setIcon(QIcon(highlighted ? ":/buttons/profile_R.png" : ":/buttons/profile_RH.png"));    // flipped intentionally
    else
        ui->profileButton->setIcon(QIcon(highlighted ? ":/buttons/profile_H.png" : ":/buttons/profile.png"));
}

void QtSLiMWindow::updateRecycleButtonIcon(bool pressed)
{
    if (slimChangeCount)
        ui->recycleButton->setIcon(QIcon(pressed ? ":/buttons/recycle_GH.png" : ":/buttons/recycle_G.png"));
    else
        ui->recycleButton->setIcon(QIcon(pressed ? ":/buttons/recycle_H.png" : ":/buttons/recycle.png"));
}

void QtSLiMWindow::updateUIEnabling(void)
{
    // First we update all the UI that belongs exclusively to ourselves: buttons, labels, etc.
    ui->playOneStepButton->setEnabled(!reachedSimulationEnd_ && !continuousPlayOn_);
    ui->playButton->setEnabled(!reachedSimulationEnd_ && !profilePlayOn_);
    ui->profileButton->setEnabled(!reachedSimulationEnd_ && !nonProfilePlayOn_ && !generationPlayOn_);
    ui->recycleButton->setEnabled(!continuousPlayOn_);
    
    ui->playSpeedSlider->setEnabled(!invalidSimulation_);
    ui->generationLineEdit->setEnabled(!reachedSimulationEnd_ && !continuousPlayOn_);

    ui->toggleDrawerButton->setEnabled(true);
    ui->showMutationsButton->setEnabled(!invalidSimulation_);
    ui->showChromosomeMapsButton->setEnabled(!invalidSimulation_);
    ui->showGenomicElementsButton->setEnabled(!invalidSimulation_);
    ui->showFixedSubstitutionsButton->setEnabled(!invalidSimulation_);
    
    ui->checkScriptButton->setEnabled(!continuousPlayOn_);
    ui->prettyprintButton->setEnabled(!continuousPlayOn_);
    ui->scriptHelpButton->setEnabled(true);
    ui->consoleButton->setEnabled(true);
    ui->browserButton->setEnabled(true);
    ui->jumpToPopupButton->setEnabled(true);
    
    ui->clearOutputButton->setEnabled(!invalidSimulation_);
    ui->dumpPopulationButton->setEnabled(!invalidSimulation_);
    ui->graphPopupButton->setEnabled(!invalidSimulation_);
    ui->changeDirectoryButton->setEnabled(!invalidSimulation_ && !continuousPlayOn_);
    
    ui->scriptTextEdit->setReadOnly(continuousPlayOn_);
    ui->outputTextEdit->setReadOnly(true);
    
    ui->generationLabel->setEnabled(!invalidSimulation_);
    ui->outputHeaderLabel->setEnabled(!invalidSimulation_);
    
    // Tell the console controller to enable/disable its buttons
    if (consoleController)
        consoleController->setInterfaceEnabled(!continuousPlayOn_);
    
    // Then, if we are the active window, we update the menus to reflect our state
    // If there's an active window but it isn't us, we reflect that situation with a different method
    // Keep in mind that in Qt each QMainWindow has its own menu bar, its own actions, etc.; this is not global state!
    // This means we spend a little time updating menu enable states that are not visible anyway, but it's fast
    QWidget *focusWidget = qApp->focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : nullptr);
    
    if (focusWindow == this) {
        //qDebug() << "updateMenuEnablingACTIVE()";
        updateMenuEnablingACTIVE(focusWidget);
    } else {
        //qDebug() << "updateMenuEnablingINACTIVE()";
        updateMenuEnablingINACTIVE(focusWidget, focusWindow);
    }
}

void QtSLiMWindow::updateMenuEnablingACTIVE(QWidget *focusWidget)
{
    ui->actionSave->setEnabled(true);
    ui->actionSaveAs->setEnabled(true);
    ui->actionRevertToSaved->setEnabled(!isUntitled);
    
    //ui->menuSimulation->setEnabled(true);     // commented out these menu-level enable/disables; they flash weirdly and are distracting
    ui->actionStep->setEnabled(!reachedSimulationEnd_ && !continuousPlayOn_);
    ui->actionPlay->setEnabled(!reachedSimulationEnd_ && !profilePlayOn_);
    ui->actionPlay->setText(nonProfilePlayOn_ ? "Stop" : "Play");
    ui->actionProfile->setEnabled(!reachedSimulationEnd_ && !nonProfilePlayOn_ && !generationPlayOn_);
    ui->actionProfile->setText(profilePlayOn_ ? "Stop" : "Profile");
    ui->actionRecycle->setEnabled(!continuousPlayOn_);
    
    //ui->menuScript->setEnabled(true);
    ui->actionCheckScript->setEnabled(!continuousPlayOn_);
    ui->actionPrettyprintScript->setEnabled(!continuousPlayOn_);
    ui->actionReformatScript->setEnabled(!continuousPlayOn_);
    ui->actionShowScriptHelp->setEnabled(true);
    ui->actionShowEidosConsole->setEnabled(true);
    ui->actionShowEidosConsole->setText(!consoleController->isVisible() ? "Show Eidos Console" : "Hide Eidos Console");
    
    bool varBrowserVisible = false;
    if (consoleController->variableBrowser())
        varBrowserVisible = consoleController->variableBrowser()->isVisible();
    
    ui->actionShowVariableBrowser->setEnabled(true);
    ui->actionShowVariableBrowser->setText(!varBrowserVisible ? "Show Variable Browser" : "Hide Variable Browser");
    
    ui->actionClearOutput->setEnabled(!invalidSimulation_);
    ui->actionExecuteAll->setEnabled(false);
    ui->actionExecuteSelection->setEnabled(false);
    ui->actionDumpPopulationState->setEnabled(!invalidSimulation_);
    ui->actionChangeWorkingDirectory->setEnabled(!invalidSimulation_ && !continuousPlayOn_);
    
    updateMenuEnablingSHARED(focusWidget);
}

void QtSLiMWindow::updateMenuEnablingINACTIVE(QWidget *focusWidget, QWidget *focusWindow)
{
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
    QtSLiMVariableBrowser *varBrowser = dynamic_cast<QtSLiMVariableBrowser*>(focusWindow);
    bool varBrowserFocused = (varBrowser != nullptr);
    bool varBrowserVisible = false;
    
    if (consoleFocused)
        varBrowserVisible = (eidosConsole->variableBrowser() && eidosConsole->variableBrowser()->isVisible());
    else if (varBrowserFocused)
        varBrowserVisible = varBrowser->isVisible();
    
    //ui->menuScript->setEnabled(consoleFocused);
    ui->actionCheckScript->setEnabled(consoleFocusedAndEditable);
    ui->actionPrettyprintScript->setEnabled(consoleFocusedAndEditable);
    ui->actionReformatScript->setEnabled(consoleFocusedAndEditable);
    ui->actionShowScriptHelp->setEnabled(consoleFocused);
    ui->actionShowEidosConsole->setEnabled(consoleFocused);
    ui->actionShowEidosConsole->setText(!consoleFocused ? "Show Eidos Console" : "Hide Eidos Console");
    ui->actionShowVariableBrowser->setEnabled(consoleFocused || varBrowserFocused);
    ui->actionShowVariableBrowser->setText(!varBrowserVisible ? "Show Variable Browser" : "Hide Variable Browser");
    ui->actionClearOutput->setEnabled(consoleFocused);
    ui->actionExecuteAll->setEnabled(consoleFocusedAndEditable);
    ui->actionExecuteSelection->setEnabled(consoleFocusedAndEditable);
    
    // but these two menu items apply only to QtSLiMWindow, not to the Eidos console
    ui->actionDumpPopulationState->setEnabled(false);
    ui->actionChangeWorkingDirectory->setEnabled(false);
    
    updateMenuEnablingSHARED(focusWidget);
}

void QtSLiMWindow::updateMenuEnablingSHARED(QWidget *focusWidget)
{
    // Here we update the enable state for menu items, such as cut/copy/paste, that go to
    // the focusWidget whatever window it might be in; "first responder" in Cocoa parlance
    QLineEdit *lE = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *tE = dynamic_cast<QTextEdit*>(focusWidget);
    QtSLiMTextEdit *stE = dynamic_cast<QtSLiMTextEdit *>(tE);
    bool hasEnabledDestination = (lE && lE->isEnabled()) || (tE && tE->isEnabled());
    bool hasEnabledModifiableDestination = (lE && lE->isEnabled() && !lE->isReadOnly()) ||
            (tE && tE->isEnabled() && !tE->isReadOnly());
    bool hasUndoableDestination = (lE && lE->isEnabled() && !lE->isReadOnly() && lE->isUndoAvailable()) ||
            (tE && tE->isEnabled() && !tE->isReadOnly() && tE->isUndoRedoEnabled());
    bool hasRedoableDestination = (lE && lE->isEnabled() && !lE->isReadOnly() && lE->isRedoAvailable()) ||
            (tE && tE->isEnabled() && !tE->isReadOnly() && tE->isUndoRedoEnabled());
    bool hasCopyableDestination = (lE && lE->isEnabled() && lE->selectedText().length()) ||
            (tE && tE->isEnabled());
    
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
    
    // actions handled by QtSLiMScriptTextEdit only
    QtSLiMScriptTextEdit *scriptEdit = dynamic_cast<QtSLiMScriptTextEdit*>(focusWidget);
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

void QtSLiMWindow::updateVariableBrowserButtonState(bool visible)
{
    // Should only be called by QtSLiMEidosConsole::updateVariableBrowserButtonStates() so state is synced
    ui->browserButton->setChecked(visible);
    showBrowserReleased();
}

void QtSLiMWindow::updateWindowMenu(void)
{
    // Clear out old actions, up to the separator
    do
    {
        QAction *lastAction = ui->menuWindow->actions().last();
        
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
        
        if (mainWin)
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

#if defined(SLIMGUI) && (SLIMPROFILING == 1)

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
        colorCursor.setPosition(end, QTextCursor::KeepAnchor); // +1?
        
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
    QWidget *window = new QWidget(this, Qt::Window);    // the profile window has us as a parent, but is still a standalone window
    QString title = window->windowTitle();
    
    if (title.length() == 0)
        title = "Untitled";
    
    window->setWindowTitle("Profile Report for " + title);
    window->setMinimumSize(500, 200);
    window->resize(500, 600);
    window->move(50, 50);
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    window->setWindowIcon(QIcon());
#endif
    
    // make window actions for all global menu items
    qtSLiMAppDelegate->addActionsForGlobalMenuItems(window);
    
    // Make a QTextEdit to hold the results
    QHBoxLayout *layout = new QHBoxLayout;
    QTextEdit *textEdit = new QTextEdit();
    
    window->setLayout(layout);
    
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(textEdit);
    
    textEdit->setFrameStyle(QFrame::NoFrame);
    textEdit->setReadOnly(true);
    
    QTextDocument *doc = textEdit->document();
    QTextCursor tc = textEdit->textCursor();
    
    doc->setDocumentMargin(10);
    
    // Choose our fonts; the variable names here are historical, they are not necessarily menlo/optima...
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    QFont menlo11(prefs.displayFontPref());
    qreal displayFontSize = menlo11.pointSizeF();
    qreal scaleFactor = displayFontSize / 11.0;     // The unscaled sizes are geared toward Optima on the Mac
    
#if !defined(__APPLE__)
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
    
    // Make the QTextCharFormat objects we will use
    QTextCharFormat optima18b_d, optima14b_d, optima13_d, optima13i_d, optima8_d, optima3_d, menlo11_d;
    
    optima18b_d.setFont(optima18b);
    optima14b_d.setFont(optima14b);
    optima13_d.setFont(optima13);
    optima13i_d.setFont(optima13i);
    optima8_d.setFont(optima8);
    optima3_d.setFont(optima3);
    menlo11_d.setFont(menlo11);
    
    // Adjust the tab width to the monospace font we have chosen
    int tabWidth = 0;
    QFontMetrics fm(menlo11);
    
    //tabWidth = fm.horizontalAdvance("   ");   // added in Qt 5.11
    tabWidth = fm.width("   ");                 // deprecated (in 5.11, I assume)
    
    textEdit->setTabStopWidth(tabWidth);        // deprecated in 5.10
    
    // Build the report attributed string
    QString startDateString = profileStartDate_.toString("M/d/yy, h:mm:ss AP");
    QString endDateString = profileEndDate_.toString("M/d/yy, h:mm:ss AP");
    double elapsedWallClockTime = (profileStartDate_.msecsTo(profileEndDate_)) / 1000.0;
    double elapsedCPUTimeInSLiM = profileElapsedCPUClock / static_cast<double>(CLOCKS_PER_SEC);
	double elapsedWallClockTimeInSLiM = Eidos_ElapsedProfileTime(profileElapsedWallClock);
    
    tc.insertText("Profile Report\n", optima18b_d);
    tc.insertText(" \n", optima3_d);
    
    tc.insertText("Model: " + title + "\n", optima13_d);
    tc.insertText(" \n", optima8_d);
    
    tc.insertText("Run start: " + startDateString + "\n", optima13_d);
    tc.insertText("Run end: " + endDateString + "\n", optima13_d);
    tc.insertText(" \n", optima8_d);
    
    tc.insertText(QString("Elapsed wall clock time: %1 s\n").arg(elapsedWallClockTime, 0, 'f', 2), optima13_d);
    tc.insertText(QString("Elapsed wall clock time inside SLiM core (corrected): %1 s\n").arg(elapsedWallClockTimeInSLiM, 0, 'f', 2), optima13_d);
    tc.insertText(QString("Elapsed CPU time inside SLiM core (uncorrected): %1 s\n").arg(elapsedCPUTimeInSLiM, 0, 'f', 2), optima13_d);
    tc.insertText(QString("Elapsed generations: %1%2\n").arg(continuousPlayGenerationsCompleted_).arg((profileStartGeneration == 0) ? " (including initialize)" : ""), optima13_d);
    tc.insertText(" \n", optima8_d);
    
    tc.insertText(QString("Profile block external overhead: %1 ticks (%2 s)\n").arg(gEidos_ProfileOverheadTicks, 0, 'f', 2).arg(gEidos_ProfileOverheadSeconds, 0, 'g', 4), optima13_d);
    tc.insertText(QString("Profile block internal lag: %1 ticks (%2 s)\n").arg(gEidos_ProfileLagTicks, 0, 'f', 2).arg(gEidos_ProfileLagSeconds, 0, 'g', 4), optima13_d);
    tc.insertText(" \n", optima8_d);
    
    tc.insertText(QString("Average generation SLiM memory use: %1\n").arg(stringForByteCount(sim->profile_total_memory_usage_.totalMemoryUsage / static_cast<size_t>(sim->total_memory_tallies_))), optima13_d);
    tc.insertText(QString("Final generation SLiM memory use: %1\n").arg(stringForByteCount(sim->profile_last_memory_usage_.totalMemoryUsage)), optima13_d);
    
	//
	//	Generation stage breakdown
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		bool isWF = (sim->ModelType() == SLiMModelType::kModelTypeWF);
		double elapsedStage0Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[0]);
		double elapsedStage1Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[1]);
		double elapsedStage2Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[2]);
		double elapsedStage3Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[3]);
		double elapsedStage4Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[4]);
		double elapsedStage5Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[5]);
		double elapsedStage6Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[6]);
		double elapsedStage7Time = Eidos_ElapsedProfileTime(sim->profile_stage_totals_[7]);
		double percentStage0 = (elapsedStage0Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage1 = (elapsedStage1Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage2 = (elapsedStage2Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage3 = (elapsedStage3Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage4 = (elapsedStage4Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage5 = (elapsedStage5Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage6 = (elapsedStage6Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentStage7 = (elapsedStage7Time / elapsedWallClockTimeInSLiM) * 100.0;
		int fw = 4;
		
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage0Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage1Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage2Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage3Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage4Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage5Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage6Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedStage7Time)))));
		
		tc.insertText(" \n", optima13_d);
		tc.insertText("Generation stage breakdown\n", optima14b_d);
		tc.insertText(" \n", optima3_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage0Time, fw, 'f', 2).arg(percentStage0, 5, 'f', 2), menlo11_d);
		tc.insertText(" : initialize() callback execution\n", optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage1Time, fw, 'f', 2).arg(percentStage1, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 1 – early() event execution\n" : " : stage 1 – offspring generation\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage2Time, fw, 'f', 2).arg(percentStage2, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 2 – offspring generation\n" : " : stage 2 – early() event execution\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage3Time, fw, 'f', 2).arg(percentStage3, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 3 – bookkeeping (fixed mutation removal, etc.)\n" : " : stage 3 – fitness calculation\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage4Time, fw, 'f', 2).arg(percentStage4, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 4 – generation swap\n" : " : stage 4 – viability/survival selection\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage5Time, fw, 'f', 2).arg(percentStage5, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 5 – late() event execution\n" : " : stage 5 – bookkeeping (fixed mutation removal, etc.)\n"), optima13_d);
		
		tc.insertText(QString("%1 s (%2%)").arg(elapsedStage6Time, fw, 'f', 2).arg(percentStage6, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 6 – fitness calculation\n" : " : stage 6 – late() event execution\n"), optima13_d);
        
        tc.insertText(QString("%1 s (%2%)").arg(elapsedStage7Time, fw, 'f', 2).arg(percentStage7, 5, 'f', 2), menlo11_d);
		tc.insertText((isWF ? " : stage 7 – tree sequence auto-simplification\n" : " : stage 7 – tree sequence auto-simplification\n"), optima13_d);
	}
	
	//
	//	Callback type breakdown
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		double elapsedType0Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[0]);
		double elapsedType1Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[1]);
		double elapsedType2Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[2]);
		double elapsedType3Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[3]);
		double elapsedType4Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[4]);
		double elapsedType5Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[5]);
		double elapsedType6Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[6]);
		double elapsedType7Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[7]);
		double elapsedType8Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[8]);
		double elapsedType9Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[9]);
		double elapsedType10Time = Eidos_ElapsedProfileTime(sim->profile_callback_totals_[10]);
		double percentType0 = (elapsedType0Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType1 = (elapsedType1Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType2 = (elapsedType2Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType3 = (elapsedType3Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType4 = (elapsedType4Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType5 = (elapsedType5Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType6 = (elapsedType6Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType7 = (elapsedType7Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType8 = (elapsedType8Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType9 = (elapsedType9Time / elapsedWallClockTimeInSLiM) * 100.0;
		double percentType10 = (elapsedType10Time / elapsedWallClockTimeInSLiM) * 100.0;
		int fw = 4, fw2 = 4;
		
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType0Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType1Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType2Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType3Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType4Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType5Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType6Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType7Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType8Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType9Time)))));
		fw = std::max(fw, 3 + static_cast<int>(ceil(log10(floor(elapsedType10Time)))));
		
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType0)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType1)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType2)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType3)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType4)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType5)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType6)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType7)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType8)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType9)))));
		fw2 = std::max(fw2, 3 + static_cast<int>(ceil(log10(floor(percentType10)))));
		
		tc.insertText(" \n", optima13_d);
		tc.insertText("Callback type breakdown\n", optima14b_d);
		tc.insertText(" \n", optima3_d);
		
		// Note these are out of numeric order, but in generation-cycle order
		if (sim->ModelType() == SLiMModelType::kModelTypeWF)
		{
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType2Time, fw, 'f', 2).arg(percentType2, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : initialize() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType0Time, fw, 'f', 2).arg(percentType0, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : early() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType6Time, fw, 'f', 2).arg(percentType6, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : mateChoice() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType8Time, fw, 'f', 2).arg(percentType8, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : recombination() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType9Time, fw, 'f', 2).arg(percentType9, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : mutation() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType7Time, fw, 'f', 2).arg(percentType7, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : modifyChild() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType1Time, fw, 'f', 2).arg(percentType1, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : late() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType3Time, fw, 'f', 2).arg(percentType3, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : fitness() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType4Time, fw, 'f', 2).arg(percentType4, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : fitness() callbacks (global)\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType5Time, fw, 'f', 2).arg(percentType5, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : interaction() callbacks\n", optima13_d);
		}
		else
		{
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType2Time, fw, 'f', 2).arg(percentType2, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : initialize() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType10Time, fw, 'f', 2).arg(percentType10, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : reproduction() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType8Time, fw, 'f', 2).arg(percentType8, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : recombination() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType9Time, fw, 'f', 2).arg(percentType9, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : mutation() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType7Time, fw, 'f', 2).arg(percentType7, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : modifyChild() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType0Time, fw, 'f', 2).arg(percentType0, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : early() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType3Time, fw, 'f', 2).arg(percentType3, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : fitness() callbacks\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType4Time, fw, 'f', 2).arg(percentType4, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : fitness() callbacks (global)\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType1Time, fw, 'f', 2).arg(percentType1, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : late() events\n", optima13_d);
			
			tc.insertText(QString("%1 s (%2%)").arg(elapsedType5Time, fw, 'f', 2).arg(percentType5, fw2, 'f', 2), menlo11_d);
			tc.insertText(" : interaction() callbacks\n", optima13_d);
		}
	}
	
	//
	//	Script block profiles
	//
	if (elapsedWallClockTimeInSLiM > 0.0)
	{
		{
			std::vector<SLiMEidosBlock*> &script_blocks = sim->AllScriptBlocks();
			
			// Convert the profile counts in all script blocks into self counts (excluding the counts of nodes below them)
			for (SLiMEidosBlock *script_block : script_blocks)
				if (script_block->type_ != SLiMEidosBlockType::SLiMEidosUserDefinedFunction)		// exclude function blocks; not user-visible
					script_block->root_node_->ConvertProfileTotalsToSelfCounts();
		}
		{
			tc.insertText(" \n", optima13_d);
			tc.insertText("Script block profiles (as a fraction of corrected wall clock time)\n", optima14b_d);
			tc.insertText(" \n", optima3_d);
			
			std::vector<SLiMEidosBlock*> &script_blocks = sim->AllScriptBlocks();
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
			
			std::vector<SLiMEidosBlock*> &script_blocks = sim->AllScriptBlocks();
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
		EidosFunctionMap &function_map = sim->FunctionMap();
		std::vector<const EidosFunctionSignature *> userDefinedFunctions;
		
		for (auto functionPairIter : function_map)
		{
			const EidosFunctionSignature *signature = functionPairIter.second.get();
			
			if (signature->body_script_ && signature->user_defined_)
			{
				signature->body_script_->AST()->ConvertProfileTotalsToSelfCounts();
				userDefinedFunctions.push_back(signature);
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
	//	MutationRun metrics
	//
	{
		int64_t power_tallies[20];	// we only go up to 1024 mutruns right now, but this gives us some headroom
		int64_t power_tallies_total = static_cast<int>(sim->profile_mutcount_history_.size());
		
		for (int power = 0; power < 20; ++power)
			power_tallies[power] = 0;
		
		for (int32_t count : sim->profile_mutcount_history_)
		{
			int power = static_cast<int>(round(log2(count)));
			
			power_tallies[power]++;
		}
		
		tc.insertText(" \n", menlo11_d);
		tc.insertText(" \n", optima13_d);
		tc.insertText("MutationRun usage\n", optima14b_d);
		tc.insertText(" \n", optima3_d);
		
		for (int power = 0; power < 20; ++power)
		{
			if (power_tallies[power] > 0)
			{
				tc.insertText(QString("%1%").arg((power_tallies[power] / static_cast<double>(power_tallies_total)) * 100.0, 6, 'f', 2), menlo11_d);
				tc.insertText(QString(" of generations : %1 mutation runs per genome\n").arg(static_cast<int>(round(pow(2.0, power)))), optima13_d);
			}
		}
		
		
		int64_t regime_tallies[3];
		int64_t regime_tallies_total = static_cast<int>(sim->profile_nonneutral_regime_history_.size());
		
		for (int regime = 0; regime < 3; ++regime)
			regime_tallies[regime] = 0;
		
		for (int32_t regime : sim->profile_nonneutral_regime_history_)
			if ((regime >= 1) && (regime <= 3))
				regime_tallies[regime - 1]++;
			else
				regime_tallies_total--;
		
		tc.insertText(" \n", optima13_d);
		
		for (int regime = 0; regime < 3; ++regime)
		{
			tc.insertText(QString("%1%").arg((regime_tallies[regime] / static_cast<double>(regime_tallies_total)) * 100.0, 6, 'f', 2), menlo11_d);
			tc.insertText(QString(" of generations : regime %1 (%2)\n").arg(regime + 1).arg(regime == 0 ? "no fitness callbacks" : (regime == 1 ? "constant neutral fitness callbacks only" : "unpredictable fitness callbacks present")), optima13_d);
		}
		
		
		tc.insertText(" \n", optima13_d);
		
		tc.insertText(QString("%1").arg(sim->profile_mutation_total_usage_), menlo11_d);
		tc.insertText(" mutations referenced, summed across all generations\n", optima13_d);
		
		tc.insertText(QString("%1").arg(sim->profile_nonneutral_mutation_total_), menlo11_d);
		tc.insertText(" mutations considered potentially nonneutral\n", optima13_d);
		
		tc.insertText(QString("%1%").arg(((sim->profile_mutation_total_usage_ - sim->profile_nonneutral_mutation_total_) / static_cast<double>(sim->profile_mutation_total_usage_)) * 100.0, 0, 'f', 2), menlo11_d);
		tc.insertText(" of mutations excluded from fitness calculations\n", optima13_d);
		
		tc.insertText(QString("%1").arg(sim->profile_max_mutation_index_), menlo11_d);
		tc.insertText(" maximum simultaneous mutations\n", optima13_d);
		
		
		tc.insertText(" \n", optima13_d);
		
		tc.insertText(QString("%1").arg(sim->profile_mutrun_total_usage_), menlo11_d);
		tc.insertText(" mutation runs referenced, summed across all generations\n", optima13_d);
		
		tc.insertText(QString("%1").arg(sim->profile_unique_mutrun_total_), menlo11_d);
		tc.insertText(" unique mutation runs maintained among those\n", optima13_d);
		
		tc.insertText(QString("%1%").arg((sim->profile_mutrun_nonneutral_recache_total_ / static_cast<double>(sim->profile_unique_mutrun_total_)) * 100.0, 6, 'f', 2), menlo11_d);
		tc.insertText(" of mutation run nonneutral caches rebuilt per generation\n", optima13_d);
		
		tc.insertText(QString("%1%").arg(((sim->profile_mutrun_total_usage_ - sim->profile_unique_mutrun_total_) / static_cast<double>(sim->profile_mutrun_total_usage_)) * 100.0, 6, 'f', 2), menlo11_d);
		tc.insertText(" of mutation runs shared among genomes", optima13_d);
	}
#endif
	
	{
		//
		//	Memory usage metrics
		//
		SLiM_MemoryUsage &mem_tot = sim->profile_total_memory_usage_;
		SLiM_MemoryUsage &mem_last = sim->profile_last_memory_usage_;
		uint64_t div = static_cast<uint64_t>(sim->total_memory_tallies_);
		double ddiv = sim->total_memory_tallies_;
		double average_total = mem_tot.totalMemoryUsage / ddiv;
		double final_total = mem_last.totalMemoryUsage;
		
		tc.insertText(" \n", menlo11_d);
		tc.insertText(" \n", optima13_d);
		tc.insertText("SLiM memory usage (average / final generation)\n", optima14b_d);
		tc.insertText(" \n", optima3_d);
		
        QTextCharFormat colored_menlo = menlo11_d;
        
		// Chromosome
		tc.insertText(attributedStringForByteCount(mem_tot.chromosomeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.chromosomeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : Chromosome object\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.chromosomeMutationRateMaps / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.chromosomeMutationRateMaps, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : mutation rate maps\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.chromosomeRecombinationRateMaps / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.chromosomeRecombinationRateMaps, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : recombination rate maps\n", optima13_d);

		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.chromosomeAncestralSequence / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.chromosomeAncestralSequence, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : ancestral nucleotides\n", optima13_d);
		
		// Genome
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.genomeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.genomeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Genome objects (%1 / %2)\n").arg(mem_tot.genomeObjects_count / ddiv, 0, 'f', 2).arg(mem_last.genomeObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.genomeExternalBuffers / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.genomeExternalBuffers, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : external MutationRun* buffers\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.genomeUnusedPoolSpace / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.genomeUnusedPoolSpace, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool space\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.genomeUnusedPoolBuffers / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.genomeUnusedPoolBuffers, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool buffers\n", optima13_d);
		
		// GenomicElement
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.genomicElementObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.genomicElementObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : GenomicElement objects (%1 / %2)\n").arg(mem_tot.genomicElementObjects_count / ddiv, 0, 'f', 2).arg(mem_last.genomicElementObjects_count), optima13_d);
		
		// GenomicElementType
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.genomicElementTypeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.genomicElementTypeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : GenomicElementType objects (%1 / %2)\n").arg(mem_tot.genomicElementTypeObjects_count / ddiv, 0, 'f', 2).arg(mem_last.genomicElementTypeObjects_count), optima13_d);
		
		// Individual
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.individualObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.individualObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Individual objects (%1 / %2)\n").arg(mem_tot.individualObjects_count / ddiv, 0, 'f', 2).arg(mem_last.individualObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.individualUnusedPoolSpace / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.individualUnusedPoolSpace, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool space\n", optima13_d);
		
		// InteractionType
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.interactionTypeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.interactionTypeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : InteractionType objects (%1 / %2)\n").arg(mem_tot.interactionTypeObjects_count / ddiv, 0, 'f', 2).arg(mem_last.interactionTypeObjects_count), optima13_d);
		
		if (mem_tot.interactionTypeObjects_count || mem_last.interactionTypeObjects_count)
		{
			tc.insertText("   ", menlo11_d);
			tc.insertText(attributedStringForByteCount(mem_tot.interactionTypeKDTrees / div, average_total, colored_menlo), colored_menlo);
			tc.insertText(" / ", optima13_d);
			tc.insertText(attributedStringForByteCount(mem_last.interactionTypeKDTrees, final_total, colored_menlo), colored_menlo);
			tc.insertText(" : k-d trees\n", optima13_d);
			
			tc.insertText("   ", menlo11_d);
			tc.insertText(attributedStringForByteCount(mem_tot.interactionTypePositionCaches / div, average_total, colored_menlo), colored_menlo);
			tc.insertText(" / ", optima13_d);
			tc.insertText(attributedStringForByteCount(mem_last.interactionTypePositionCaches, final_total, colored_menlo), colored_menlo);
			tc.insertText(" : position caches\n", optima13_d);
			
			tc.insertText("   ", menlo11_d);
			tc.insertText(attributedStringForByteCount(mem_tot.interactionTypeSparseArrays / div, average_total, colored_menlo), colored_menlo);
			tc.insertText(" / ", optima13_d);
			tc.insertText(attributedStringForByteCount(mem_last.interactionTypeSparseArrays, final_total, colored_menlo), colored_menlo);
			tc.insertText(" : sparse arrays\n", optima13_d);
		}
		
		// Mutation
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Mutation objects (%1 / %2)\n").arg(mem_tot.mutationObjects_count / ddiv, 0, 'f', 2).arg(mem_last.mutationObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationRefcountBuffer / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationRefcountBuffer, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : refcount buffer\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationUnusedPoolSpace / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationUnusedPoolSpace, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool space\n", optima13_d);
		
		// MutationRun
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationRunObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationRunObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : MutationRun objects (%1 / %2)\n").arg(mem_tot.mutationRunObjects_count / ddiv, 0, 'f', 2).arg(mem_last.mutationRunObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationRunExternalBuffers / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationRunExternalBuffers, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : external MutationIndex buffers\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationRunNonneutralCaches / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationRunNonneutralCaches, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : nonneutral mutation caches\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationRunUnusedPoolSpace / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationRunUnusedPoolSpace, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool space\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationRunUnusedPoolBuffers / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationRunUnusedPoolBuffers, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : unused pool buffers\n", optima13_d);
		
		// MutationType
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.mutationTypeObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.mutationTypeObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : MutationType objects (%1 / %2)\n").arg(mem_tot.mutationTypeObjects_count / ddiv, 0, 'f', 2).arg(mem_last.mutationTypeObjects_count), optima13_d);
		
		// SLiMSim
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.slimsimObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.slimsimObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : SLiMSim object\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.slimsimTreeSeqTables / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.slimsimTreeSeqTables, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : tree-sequence tables\n", optima13_d);
		
		// Subpopulation
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.subpopulationObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.subpopulationObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Subpopulation objects (%1 / %2)\n").arg(mem_tot.subpopulationObjects_count / ddiv, 0, 'f', 2).arg(mem_last.subpopulationObjects_count), optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.subpopulationFitnessCaches / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.subpopulationFitnessCaches, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : fitness caches\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.subpopulationParentTables / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.subpopulationParentTables, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : parent tables\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.subpopulationSpatialMaps / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.subpopulationSpatialMaps, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : spatial maps\n", optima13_d);
		
		if (mem_tot.subpopulationSpatialMapsDisplay || mem_last.subpopulationSpatialMapsDisplay)
		{
			tc.insertText("   ", menlo11_d);
			tc.insertText(attributedStringForByteCount(mem_tot.subpopulationSpatialMapsDisplay / div, average_total, colored_menlo), colored_menlo);
			tc.insertText(" / ", optima13_d);
			tc.insertText(attributedStringForByteCount(mem_last.subpopulationSpatialMapsDisplay, final_total, colored_menlo), colored_menlo);
			tc.insertText(" : spatial map display (SLiMgui only)\n", optima13_d);
		}
		
		// Substitution
		tc.insertText(" \n", optima8_d);
		tc.insertText(attributedStringForByteCount(mem_tot.substitutionObjects / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.substitutionObjects, final_total, colored_menlo), colored_menlo);
		tc.insertText(QString(" : Substitution objects (%1 / %2)\n").arg(mem_tot.substitutionObjects_count / ddiv, 0, 'f', 2).arg(mem_last.substitutionObjects_count), optima13_d);
		
		// Eidos
		tc.insertText(" \n", optima8_d);
		tc.insertText("Eidos:\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.eidosASTNodePool / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.eidosASTNodePool, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : EidosASTNode pool\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.eidosSymbolTablePool / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.eidosSymbolTablePool, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : EidosSymbolTable pool\n", optima13_d);
		
		tc.insertText("   ", menlo11_d);
		tc.insertText(attributedStringForByteCount(mem_tot.eidosValuePool / div, average_total, colored_menlo), colored_menlo);
		tc.insertText(" / ", optima13_d);
		tc.insertText(attributedStringForByteCount(mem_last.eidosValuePool, final_total, colored_menlo), colored_menlo);
		tc.insertText(" : EidosValue pool", optima13_d);
	}
    
    // Done, show the window
    tc.setPosition(0);
    textEdit->setTextCursor(tc);
    window->show();    
}

void QtSLiMWindow::startProfiling(void)
{
	// prepare for profiling by measuring profile block overhead and lag
	Eidos_PrepareForProfiling();
	
	// initialize counters
	profileElapsedCPUClock = 0;
	profileElapsedWallClock = 0;
	profileStartGeneration = sim->Generation();
	
	// call this first, which has the side effect of emptying out any pending profile counts
	sim->CollectSLiMguiMutationProfileInfo();
	
	// zero out profile counts for generation stages
    for (int i = 0; i < 8; ++i)
		sim->profile_stage_totals_[i] = 0;
	
	// zero out profile counts for callback types (note SLiMEidosUserDefinedFunction is excluded; that is not a category we profile)
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosEventEarly)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosEventLate)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosInitializeCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosFitnessCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosInteractionCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosMateChoiceCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosModifyChildCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosRecombinationCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosMutationCallback)] = 0;
	sim->profile_callback_totals_[static_cast<int>(SLiMEidosBlockType::SLiMEidosReproductionCallback)] = 0;
	
	// zero out profile counts for script blocks; dynamic scripts will be zeroed on construction
	std::vector<SLiMEidosBlock*> &script_blocks = sim->AllScriptBlocks();
	
	for (SLiMEidosBlock *script_block : script_blocks)
		if (script_block->type_ != SLiMEidosBlockType::SLiMEidosUserDefinedFunction)	// exclude user-defined functions; not user-visible as blocks
			script_block->root_node_->ZeroProfileTotals();
	
	// zero out profile counts for all user-defined functions
	EidosFunctionMap &function_map = sim->FunctionMap();
	
	for (auto functionPairIter : function_map)
	{
		const EidosFunctionSignature *signature = functionPairIter.second.get();
		
		if (signature->body_script_ && signature->user_defined_)
			signature->body_script_->AST()->ZeroProfileTotals();
	}
	
#if SLIM_USE_NONNEUTRAL_CACHES
	// zero out mutation run metrics
	sim->profile_mutcount_history_.clear();
	sim->profile_nonneutral_regime_history_.clear();
	sim->profile_mutation_total_usage_ = 0;
	sim->profile_nonneutral_mutation_total_ = 0;
	sim->profile_mutrun_total_usage_ = 0;
	sim->profile_unique_mutrun_total_ = 0;
	sim->profile_mutrun_nonneutral_recache_total_ = 0;
	sim->profile_max_mutation_index_ = 0;
#endif
	
	// zero out memory usage metrics
	EIDOS_BZERO(&sim->profile_last_memory_usage_, sizeof(SLiM_MemoryUsage));
	EIDOS_BZERO(&sim->profile_total_memory_usage_, sizeof(SLiM_MemoryUsage));
	sim->total_memory_tallies_ = 0;
}

void QtSLiMWindow::endProfiling(void)
{
    profileEndDate_ = QDateTime::currentDateTime();
}

#endif	// defined(SLIMGUI) && (SLIMPROFILING == 1)


//
//  simulation play mechanics
//

void QtSLiMWindow::willExecuteScript(void)
{
    // Whenever we are about to execute script, we swap in our random number generator; at other times, gEidos_rng is NULL.
    // The goal here is to keep each SLiM window independent in its random number sequence.
    if (EIDOS_GSL_RNG)
        qDebug() << "eidosConsoleWindowControllerWillExecuteScript: gEidos_rng already set up!";

    gEidos_RNG = sim_RNG;

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
    sim_RNG = gEidos_RNG;
    EIDOS_BZERO(&gEidos_RNG, sizeof(Eidos_RNG_State));

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

bool QtSLiMWindow::runSimOneGeneration(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    // This method should always be used when calling out to run the simulation, because it swaps the correct random number
    // generator stuff in and out bracketing the call to RunOneGeneration().  This bracketing would need to be done around
    // any other call out to the simulation that caused it to use random numbers, too, such as subsample output.
    bool stillRunning = true;

    willExecuteScript();

    // We always take a start clock measurement, to tally elapsed time spent running the model
    clock_t startCPUClock = clock();
    
#if (defined(SLIMGUI) && (SLIMPROFILING == 1))
	if (profilePlayOn_)
	{
		// We put the wall clock measurements on the inside since we want those to be maximally accurate,
		// as profile report percentages are fractions of the total elapsed wall clock time.
		SLIM_PROFILE_BLOCK_START();

		stillRunning = sim->RunOneGeneration();

		SLIM_PROFILE_BLOCK_END(profileElapsedWallClock);
	}
    else
#endif
    {
        stillRunning = sim->RunOneGeneration();
    }
    
    // Take an end clock time to tally elapsed time spent running the model
    clock_t endCPUClock = clock();
    
    elapsedCPUClock_ += (endCPUClock - startCPUClock);
    
#if (defined(SLIMGUI) && (SLIMPROFILING == 1))
	if (profilePlayOn_)
        profileElapsedCPUClock += (endCPUClock - startCPUClock);
#endif
    
    didExecuteScript();

    // We also want to let graphViews know when each generation has finished, in case they need to pull data from the sim.  Note this
    // happens after every generation, not just when we are updating the UI, so drawing and setNeedsDisplay: should not happen here.
    emit controllerGenerationFinished();

    return stillRunning;
}

void QtSLiMWindow::_continuousPlay(void)
{
    // NOTE this code is parallel to the code in _continuousProfile()
	if (!invalidSimulation_)
	{
        QElapsedTimer startTimer;
        startTimer.start();
        
		double speedSliderValue = ui->playSpeedSlider->value() / 100.0;     // scale is 0 to 100, since only integer values are allowed by QSlider
		double intervalSinceStarting = continuousPlayElapsedTimer_.nsecsElapsed() / 1000000000.0;
		
		// Calculate frames per second; this equation must match the equation in playSpeedChanged:
		double maxGenerationsPerSecond = 1000000000.0;	// bounded, to allow -eidos_pauseExecution to interrupt us
		
		if (speedSliderValue < 0.99999)
			maxGenerationsPerSecond = (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * 839;
		
		//qDebug() << "speedSliderValue == " << speedSliderValue << ", maxGenerationsPerSecond == " << maxGenerationsPerSecond;
		
		// We keep a local version of reachedSimulationEnd, because calling setReachedSimulationEnd: every generation
		// can actually be a large drag for simulations that run extremely quickly – it can actually exceed the time
		// spent running the simulation itself!  Moral of the story, KVO is wicked slow.
		bool reachedEnd = reachedSimulationEnd_;
		
		do
		{
			if (continuousPlayGenerationsCompleted_ / intervalSinceStarting >= maxGenerationsPerSecond)
				break;
			
            if (generationPlayOn_ && (sim->generation_ >= targetGeneration_))
                break;
            
            reachedEnd = !runSimOneGeneration();
			
			continuousPlayGenerationsCompleted_++;
		}
		while (!reachedEnd && (startTimer.nsecsElapsed() / 1000000000.0) < 0.02);
		
		setReachedSimulationEnd(reachedEnd);
		
		if (!reachedSimulationEnd_ && (!generationPlayOn_ || !(sim->generation_ >= targetGeneration_)))
		{
            updateAfterTickFull((startTimer.nsecsElapsed() / 1000000000.0) > 0.04);
			continuousPlayInvocationTimer_.start(0);
		}
		else
		{
			// stop playing
			updateAfterTickFull(true);
            
            if (nonProfilePlayOn_)
                playOrProfile(PlayType::kNormalPlay);       // click the Play button
            else if (generationPlayOn_)
                playOrProfile(PlayType::kGenerationPlay);   // click the Play button
			
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
        QElapsedTimer startTimer;
        startTimer.start();
		
		// We keep a local version of reachedSimulationEnd, because calling setReachedSimulationEnd: every generation
		// can actually be a large drag for simulations that run extremely quickly – it can actually exceed the time
		// spent running the simulation itself!  Moral of the story, KVO is wicked slow.
		bool reachedEnd = reachedSimulationEnd_;
		
		if (!reachedEnd)
		{
			do
			{
                reachedEnd = !runSimOneGeneration();
				
				continuousPlayGenerationsCompleted_++;
			}
            while (!reachedEnd && (startTimer.nsecsElapsed() / 1000000000.0) < 0.02);
			
            setReachedSimulationEnd(reachedEnd);
		}
		
		if (!reachedSimulationEnd_)
		{
            updateAfterTickFull((startTimer.nsecsElapsed() / 1000000000.0) > 0.04);
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
#ifdef DEBUG
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
    
#if (SLIMPROFILING == 0)
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
		continuousPlayGenerationsCompleted_ = 0;
        
		setContinuousPlayOn(true);
		if (playType == PlayType::kProfilePlay)
            setProfilePlayOn(true);
        else if (playType == PlayType::kNormalPlay)
            setNonProfilePlayOn(true);
        else if (playType == PlayType::kGenerationPlay)
            setGenerationPlayOn(true);
		
		// keep the button on; this works for the button itself automatically, but when the menu item is chosen this is needed
		if (playType == PlayType::kProfilePlay)
		{
            ui->profileButton->setChecked(true);
            updateProfileButtonIcon(false);
		}
		else    // kNormalPlay and kGenerationPlay
		{
            ui->playButton->setChecked(true);
            updatePlayButtonIcon(false);
			//[self placeSubview:playButton aboveSubview:profileButton];
		}
		
		// invalidate the console symbols, and don't validate them until we are done
		if (consoleController)
            consoleController->invalidateSymbolTableAndFunctionMap();
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// prepare profiling information if necessary
		if (playType == PlayType::kProfilePlay)
		{
			gEidosProfilingClientCount++;
			startProfiling();
            profileStartDate_ = QDateTime::currentDateTime();
		}
#endif
		
		// start playing/profiling
		if (playType == PlayType::kProfilePlay)
            continuousProfileInvocationTimer_.start(0);
        else    // kNormalPlay and kGenerationPlay
            continuousPlayInvocationTimer_.start(0);
	}
	else
	{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// close out profiling information if necessary
		if ((playType == PlayType::kProfilePlay) && sim && !invalidSimulation_)
		{
			endProfiling();
			gEidosProfilingClientCount--;
		}
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
        else if (playType == PlayType::kGenerationPlay)
            setGenerationPlayOn(false);
		
		// keep the button off; this works for the button itself automatically, but when the menu item is chosen this is needed
		if (playType == PlayType::kProfilePlay)
		{
            ui->profileButton->setChecked(false);
            updateProfileButtonIcon(false);
		}
		else    // kNormalPlay and kGenerationPlay
		{
            ui->playButton->setChecked(false);
            updatePlayButtonIcon(false);
			//[self placeSubview:profileButton aboveSubview:playButton];
		}
		
        // clean up and update UI
		if (consoleController)
            consoleController->validateSymbolTableAndFunctionMap();
        
		updateAfterTickFull(true);
		
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
		// If we just finished profiling, display a report
		if ((playType == PlayType::kProfilePlay) && sim && !invalidSimulation_)
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
	if (!invalidSimulation_ && !reachedSimulationEnd_ && continuousPlayOn_ && nonProfilePlayOn_ && !profilePlayOn_ && !generationPlayOn_)
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
        EIDOS_TERMINATION << "ERROR (QtSLiMWindow::eidos_openDocument): opening PDF files is not supported in SLiMgui; using PNG instead is suggested." << EidosTerminate(nullptr);
    }
    
    qtSLiMAppDelegate->openFile(path, this);
}

void QtSLiMWindow::eidos_pauseExecution(void)
{
    if (!invalidSimulation_ && !reachedSimulationEnd_ && continuousPlayOn_ && nonProfilePlayOn_ && !profilePlayOn_ && !generationPlayOn_)
	{
		continuousPlayGenerationsCompleted_ = UINT64_MAX - 1;			// this will break us out of the loop in _continuousPlay: at the end of this generation
        
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
	
	updateRecycleButtonIcon(false);
}

bool QtSLiMWindow::changedSinceRecycle(void)
{
	return !(slimChangeCount == 0);
}

void QtSLiMWindow::resetSLiMChangeCount(void)
{
    slimChangeCount = 0;
	
	updateRecycleButtonIcon(false);
}

// slot receiving the signal QTextEdit::textChanged() from the script textedit
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
        
        setReachedSimulationEnd(!runSimOneGeneration());
        
        if (consoleController)
            consoleController->validateSymbolTableAndFunctionMap();
        
        ui->generationLineEdit->clearFocus();
        updateAfterTickFull(true);
    }
}

void QtSLiMWindow::_playOneStep(void)
{
    playOneStepClicked();
    playOneStepInvocationTimer_.start(350); // milliseconds
}

void QtSLiMWindow::playOneStepPressed(void)
{
    ui->playOneStepButton->setIcon(QIcon(":/buttons/play_step_H.png"));
    _playOneStep();
}

void QtSLiMWindow::playOneStepReleased(void)
{
    ui->playOneStepButton->setIcon(QIcon(":/buttons/play_step.png"));
    playOneStepInvocationTimer_.stop();
}

void QtSLiMWindow::generationChanged(void)
{
	if (!generationPlayOn_)
	{
		QString generationString = ui->generationLineEdit->text();
		
		// Special-case initialize(); we can never advance to it, since it is first, so we just validate it
		if (generationString == "initialize()")
		{
			if (sim->generation_ != 0)
			{
				qApp->beep();
				updateGenerationCounter();
                ui->generationLineEdit->selectAll();
			}
			
			return;
		}
		
		// Get the integer value from the textfield, since it is not "initialize()"
		targetGeneration_ = SLiMClampToGenerationType(static_cast<int64_t>(generationString.toLongLong()));
		
		// make sure the requested generation is in range
		if (sim->generation_ >= targetGeneration_)
		{
			if (sim->generation_ > targetGeneration_)
            {
                qApp->beep();
                updateGenerationCounter();
                ui->generationLineEdit->selectAll();
			}
            
			return;
		}
		
		// get the first responder out of the generation textfield
        ui->generationLineEdit->clearFocus();
		
		// start playing
        playOrProfile(PlayType::kGenerationPlay);
	}
	else
	{
		// stop our recurring perform request; I don't think this is hit any more
        playOrProfile(PlayType::kGenerationPlay);
	}
}

void QtSLiMWindow::recycleClicked(void)
{
    // If the user has requested autosaves, act on that; these calls run modal, blocking panels
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
    
    // Now do the recycle
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    // Converting a QString to a std::string is surprisingly tricky: https://stackoverflow.com/a/4644922/2752221
    std::string utf8_script_string = ui->scriptTextEdit->toPlainText().toUtf8().constData();
    
    if (consoleController)
        consoleController->invalidateSymbolTableAndFunctionMap();
    
    clearOutputClicked();
    setScriptStringAndInitializeSimulation(utf8_script_string);
    
    if (consoleController)
        consoleController->validateSymbolTableAndFunctionMap();
    
    ui->generationLineEdit->clearFocus();
    elapsedCPUClock_ = 0;
    
    updateAfterTickFull(true);
    
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
	continuousPlayGenerationsCompleted_ = 1;		// this prevents a new generation from executing every time the slider moves a pixel
	
	// This method is called whenever playSpeedSlider changes, continuously; we want to show the chosen speed in a tooltip-ish window
    double speedSliderValue = ui->playSpeedSlider->value() / 100.0;     // scale is 0 to 100, since only integer values are allowed by QSlider
	
	// Calculate frames per second; this equation must match the equation in _continuousPlay:
	double maxGenerationsPerSecond = static_cast<double>(INFINITY);
	
	if (speedSliderValue < 0.99999)
		maxGenerationsPerSecond = (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * (speedSliderValue + 0.06) * 839;
	
	// Make a tooltip label string
	QString fpsString("∞ fps");
	
	if (!std::isinf(maxGenerationsPerSecond))
	{
		if (maxGenerationsPerSecond < 1.0)
			fpsString = QString::asprintf("%.2f fps", maxGenerationsPerSecond);
		else if (maxGenerationsPerSecond < 10.0)
			fpsString = QString::asprintf("%.1f fps", maxGenerationsPerSecond);
		else
			fpsString = QString::asprintf("%.0f fps", maxGenerationsPerSecond);
		
		//qDebug() << "fps string: " << fpsString;
	}
    
    // Show the tooltip; wow, that was easy...
    QPoint widgetOrigin = ui->playSpeedSlider->mapToGlobal(QPoint());
    QPoint cursorPosition = QCursor::pos();
    QPoint tooltipPosition = QPoint(cursorPosition.x() - 2, widgetOrigin.y() - ui->playSpeedSlider->rect().height() - 8);
    QToolTip::showText(tooltipPosition, fpsString, ui->playSpeedSlider, QRect(), 1000000);  // 1000 seconds; taken down on mouseup automatically
}

void QtSLiMWindow::toggleDrawerToggled(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    bool newValue = ui->toggleDrawerButton->isChecked();
    
    ui->toggleDrawerButton->setIcon(QIcon(newValue ? ":/buttons/open_type_drawer_H.png" : ":/buttons/open_type_drawer.png"));

    if (!tablesDrawerController)
    {
        tablesDrawerController = new QtSLiMTablesDrawer(this);
        if (tablesDrawerController)
        {
            // wire ourselves up to monitor the tables drawer for closing, to fix our button state
            connect(tablesDrawerController, &QtSLiMTablesDrawer::willClose, this, [this]() {
                ui->toggleDrawerButton->setChecked(false);
                toggleDrawerReleased();
            });
        }
        else
        {
            qDebug() << "Could not create tables drawer";
            return;
        }
    }
    
    if (newValue)
    {
        // position it to the right of the main window, with the same height
        QRect windowRect = geometry();
        windowRect.setLeft(windowRect.left() + windowRect.width() + 9);
        windowRect.setRight(windowRect.left() + 200);   // the minimum in the nib is larger
        
        tablesDrawerController->setGeometry(windowRect);
        
        tablesDrawerController->show();
        tablesDrawerController->raise();
        tablesDrawerController->activateWindow();
    }
    else
    {
        tablesDrawerController->hide();
    }
}

void QtSLiMWindow::showMutationsToggled(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    bool newValue = ui->showMutationsButton->isChecked();
    
    ui->showMutationsButton->setIcon(QIcon(newValue ? ":/buttons/show_mutations_H.png" : ":/buttons/show_mutations.png"));

    if (newValue != zoomedChromosomeShowsMutations)
	{
		zoomedChromosomeShowsMutations = newValue;
		ui->chromosomeZoomed->setShouldDrawMutations(newValue);
        ui->chromosomeZoomed->update();
	}
}

void QtSLiMWindow::showFixedSubstitutionsToggled(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    bool newValue = ui->showFixedSubstitutionsButton->isChecked();
    
    ui->showFixedSubstitutionsButton->setIcon(QIcon(newValue ? ":/buttons/show_fixed_H.png" : ":/buttons/show_fixed.png"));

    if (newValue != zoomedChromosomeShowsFixedSubstitutions)
	{
		zoomedChromosomeShowsFixedSubstitutions = newValue;
        ui->chromosomeZoomed->setShouldDrawFixedSubstitutions(newValue);
        ui->chromosomeZoomed->update();
    }
}

void QtSLiMWindow::showChromosomeMapsToggled(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    bool newValue = ui->showChromosomeMapsButton->isChecked();
    
    ui->showChromosomeMapsButton->setIcon(QIcon(newValue ? ":/buttons/show_recombination_H.png" : ":/buttons/show_recombination.png"));

    if (newValue != zoomedChromosomeShowsRateMaps)
	{
		zoomedChromosomeShowsRateMaps = newValue;
		ui->chromosomeZoomed->setShouldDrawRateMaps(newValue);
        ui->chromosomeZoomed->update();
	}
}

void QtSLiMWindow::showGenomicElementsToggled(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    bool newValue = ui->showGenomicElementsButton->isChecked();
    
    ui->showGenomicElementsButton->setIcon(QIcon(newValue ? ":/buttons/show_genomicelements_H.png" : ":/buttons/show_genomicelements.png"));

    if (newValue != zoomedChromosomeShowsGenomicElements)
	{
		zoomedChromosomeShowsGenomicElements = newValue;
		ui->chromosomeZoomed->setShouldDrawGenomicElements(newValue);
        ui->chromosomeZoomed->update();
	}
}

void QtSLiMWindow::showConsoleClicked(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    if (!consoleController)
    {
        qApp->beep();
        return;
    }
    
    // we're about to toggle the visibility, so set our checked state accordingly
    ui->consoleButton->setChecked(!consoleController->isVisible());
    
    ui->consoleButton->setIcon(QIcon(ui->consoleButton->isChecked() ? ":/buttons/show_console_H.png" : ":/buttons/show_console.png"));
    
    if (ui->consoleButton->isChecked())
    {
        consoleController->show();
        consoleController->raise();
        consoleController->activateWindow();
    }
    else
    {
        consoleController->hide();
    }
}

void QtSLiMWindow::showBrowserClicked(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    if (!consoleController)
    {
        qApp->beep();
        return;
    }
    
    // we're about to toggle the visibility, so set our checked state accordingly
    if (!consoleController->variableBrowser())
        ui->browserButton->setChecked(true);
    else
        ui->browserButton->setChecked(!consoleController->variableBrowser()->isVisible());
    
    consoleController->setVariableBrowserVisibility(ui->browserButton->isChecked());
}

void QtSLiMWindow::jumpToPopupButtonRunMenu(void)
{
    QTextEdit *scriptTE = ui->scriptTextEdit;
    QString currentScriptString = scriptTE->toPlainText();
    QByteArray utf8bytes = currentScriptString.toUtf8();
	const char *cstr = utf8bytes.constData();
    bool failedParse = true;
    
    // Collect actions, with associated script positions
    std::vector<std::pair<int32_t, QAction *>> jumpActions;
    
    // First we scan for comments of the form /** comment */ or /// comment, which are taken to be section headers
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
            
            if ((token.token_type_ == EidosTokenType::kTokenComment) && (token.token_string_.rfind("///", 0) == 0))
            {
                comment = QString::fromStdString(token.token_string_);
                comment = comment.mid(3);
            }
            else if (token.token_type_ == EidosTokenType::kTokenCommentLong && (token.token_string_.rfind("/**", 0) == 0))
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
                jumpAction = new QAction(comment);
                connect(jumpAction, &QAction::triggered, scriptTE, [scriptTE, comment_start, comment_end]() {
                    QTextCursor cursor = scriptTE->textCursor();
                    cursor.setPosition(comment_start, QTextCursor::MoveAnchor);
                    cursor.setPosition(comment_end, QTextCursor::KeepAnchor);
                    scriptTE->setTextCursor(cursor);
                });
                
                QFont font = jumpAction->font();
                font.setBold(true);
                jumpAction->setFont(font);
            }
            
            jumpActions.emplace_back(comment_start, jumpAction);
        }
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
            
            for (EidosASTNode *script_block_node : root_node->children_)
            {
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
                
                if (comment.length() > 0)
                    decl = decl + "  —  " + comment;
                
                // Make a menu item with the final string, and annotate it with the range to select
                QAction *jumpAction = new QAction(decl);
                
                connect(jumpAction, &QAction::triggered, scriptTE, [scriptTE, decl_start, decl_end]() {
                    QTextCursor cursor = scriptTE->textCursor();
                    cursor.setPosition(decl_start, QTextCursor::MoveAnchor);
                    cursor.setPosition(decl_end, QTextCursor::KeepAnchor);
                    scriptTE->setTextCursor(cursor);
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

void QtSLiMWindow::dumpPopulationClicked(void)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    try
	{
		// dump the population
		SLIM_OUTSTREAM << "#OUT: " << sim->generation_ << " A" << std::endl;
		sim->population_.PrintAll(SLIM_OUTSTREAM, true, true, false, false);	// output spatial positions and ages if available, but not ancestral sequence
		
		// dump fixed substitutions also; so the dump in SLiMgui is like outputFull() + outputFixedMutations()
		SLIM_OUTSTREAM << std::endl;
		SLIM_OUTSTREAM << "#OUT: " << sim->generation_ << " F " << std::endl;
		SLIM_OUTSTREAM << "Mutations:" << std::endl;
		
		for (unsigned int i = 0; i < sim->population_.substitutions_.size(); i++)
		{
			SLIM_OUTSTREAM << i << " ";
			sim->population_.substitutions_[i]->PrintForSLiMOutput(SLIM_OUTSTREAM);
		}
		
		// now send SLIM_OUTSTREAM to the output textview
		updateOutputTextView();
	}
	catch (...)
	{
	}
}

static bool rectIsOnscreen(QRect windowRect)
{
    QList<QScreen *> screens = QGuiApplication::screens();
    
    for (QScreen *screen : screens)
    {
        QRect screenRect = screen->availableGeometry();
        
        if (screenRect.contains(windowRect, true))
            return true;
    }
    
    return false;
}

void QtSLiMWindow::positionNewSubsidiaryWindow(QWidget *window)
{
    // force geometry calculation, which is lazy
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    window->show();
    window->hide();
    window->setAttribute(Qt::WA_DontShowOnScreen, false);
    
    // Now get the frame geometry; note that on X11 systems the window frame is often not included in frameGeometry(), even
    // though it's supposed to be, because it is simply not available from X.  We attempt to compensate by adding in the
    // height of the window title bar, although that entails making assumptions about the windowing system appearance.
    QRect windowFrame = window->frameGeometry();
    QRect mainWindowFrame = this->frameGeometry();
    bool drawerIsOpen = (!!tablesDrawerController);
    const int titleBarHeight = 30;
    QPoint unadjust;
    
    if (windowFrame == window->geometry())
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
            window->move(candidateFrame.topLeft() + unadjust);
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
            window->move(candidateFrame.topLeft() + unadjust);
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
            window->move(candidateFrame.topLeft() + unadjust);
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
            window->move(candidateFrame.topLeft() + unadjust);
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
            window->move(candidateFrame.topLeft() + unadjust);
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
    int width = (null ? 288 : image.width());
    int height = (null ? 288 : image.height());
    
    QWidget *window = new QWidget(this, Qt::Window | Qt::Tool);    // the image window has us as a parent, but is still a standalone window
    
    window->setWindowTitle(fileInfo.fileName());
    window->setFixedSize(width, height);
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    window->setWindowIcon(qtSLiMAppDelegate->genericDocumentIcon());    // doesn't seem to quite work; we get the SLiM document icon, inherited from parent presumably
#endif
    window->setWindowFilePath(path);
    
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
    
    window->setLayout(topLayout);
    topLayout->setMargin(0);
    topLayout->setSpacing(0);
    topLayout->addWidget(imageView);
    
    // Make a file system watcher to update us when the image changes
    QFileSystemWatcher *watcher = new QFileSystemWatcher(QStringList(path), window);
    
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
    contextMenu->addAction("Copy Image", [path]() {
        QImage watched_image(path);     // get the current image from the filesystem
        QClipboard *clipboard = QGuiApplication::clipboard();
        clipboard->setImage(watched_image);
    });
    contextMenu->addAction("Copy File Path", [path]() {
        QClipboard *clipboard = QGuiApplication::clipboard();
        clipboard->setText(path);
    });
    
    // Reveal in Finder / Show in Explorer: see https://stackoverflow.com/questions/3490336
    // Note there is no good solution on Linux, so we do "Open File" instead
    #if defined(Q_OS_MACOS)
    contextMenu->addAction("Reveal in Finder", [path]() {
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
    positionNewSubsidiaryWindow(window);
    
    // make window actions for all global menu items
    // we do NOT need to do this, because we use Qt::Tool; Qt will use our parent window's shortcuts
    //qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
    
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    
    return window;
}

QWidget *QtSLiMWindow::graphWindowWithView(QtSLiMGraphView *graphView)
{
    isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
    
    // Make a new window to show the graph
    QWidget *window = new QWidget(this, Qt::Window | Qt::Tool);    // the graph window has us as a parent, but is still a standalone window
    QString title = graphView->graphTitle();
    
    window->setWindowTitle(title);
    window->setMinimumSize(250, 250);
    window->resize(300, 300);
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    window->setWindowIcon(QIcon());
#endif
    
    // Install graphView in the window
    QVBoxLayout *topLayout = new QVBoxLayout;
    
    window->setLayout(topLayout);
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
        
        QSpacerItem *rightSpacer = new QSpacerItem(16, 5, QSizePolicy::Expanding, QSizePolicy::Minimum);
        buttonLayout->addItem(rightSpacer);
        
        // this code is based on the creation of executeScriptButton in ui_QtSLiMEidosConsole.h
        QtSLiMPushButton *actionButton = new QtSLiMPushButton(window);
        actionButton->setObjectName(QString::fromUtf8("actionButton"));
        actionButton->setMinimumSize(QSize(20, 20));
        actionButton->setMaximumSize(QSize(20, 20));
        actionButton->setFocusPolicy(Qt::NoFocus);
        actionButton->setStyleSheet(QString::fromUtf8("QPushButton:pressed {\n"
"	background-color: #00000000;\n"
"	border: 0px;\n"
"}\n"
"QPushButton:checked {\n"
"	background-color: #00000000;\n"
"	border: 0px;\n"
"}"));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/buttons/action.png"), QSize(), QIcon::Normal, QIcon::Off);
        icon4.addFile(QString::fromUtf8(":/buttons/action_H.png"), QSize(), QIcon::Normal, QIcon::On);
        actionButton->setIcon(icon4);
        actionButton->setIconSize(QSize(20, 20));
        actionButton->setCheckable(true);
        actionButton->setFlat(true);
#if QT_CONFIG(tooltip)
        actionButton->setToolTip("<html><head/><body><p>configure graph</p></body></html>");
#endif // QT_CONFIG(tooltip)
        buttonLayout->addWidget(actionButton);
        
        connect(actionButton, &QPushButton::pressed, graphView, [actionButton, graphView]() { actionButton->setIcon(QIcon(":/buttons/action_H.png")); graphView->actionButtonRunMenu(actionButton); });
        connect(actionButton, &QPushButton::released, graphView, [actionButton]() { actionButton->setIcon(QIcon(":/buttons/action.png")); });
        
        actionButton->setEnabled(!invalidSimulation() && (sim->generation_ > 0));
    }
    
    // Give the graph view a chance to do something with the window it's now in
    graphView->addedToWindow();
    
    // force geometry calculation, which is lazy
    window->setAttribute(Qt::WA_DontShowOnScreen, true);
    window->show();
    window->hide();
    window->setAttribute(Qt::WA_DontShowOnScreen, false);
    
    // If we added a button layout, give it room so the graph area is still square
    // Note this has to happen after forcing layout calculations
    if (buttonLayout)
    {
        QSize contentSize = window->size();
        QSize minSize = window->minimumSize();
        int buttonLayoutHeight = buttonLayout->geometry().height();
        
        contentSize.setHeight(contentSize.height() + buttonLayoutHeight);
        window->resize(contentSize);
        
        minSize.setHeight(minSize.height() + buttonLayoutHeight);
        window->setMinimumSize(minSize);
    }
    
    // Position the window nicely
    positionNewSubsidiaryWindow(window);
    
    // make window actions for all global menu items
    // we do NOT need to do this, because we use Qt::Tool; Qt will use our parent window's shortcuts
    //qtSLiMAppDelegate->addActionsForGlobalMenuItems(window);
    
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    
    return window;
}

void QtSLiMWindow::graphPopupButtonRunMenu(void)
{
	bool disableAll = false;
	
	// When the simulation is not valid and initialized, the context menu is disabled
	if (invalidSimulation_) // || !sim || !sim->simulation_valid_ || (sim->generation_ < 1))
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
    
    QAction *graphPopSizeVsTime = contextMenu.addAction("Graph Population Size ~ Time");
    graphPopSizeVsTime->setEnabled(!disableAll);
    
    QAction *graphPopVisualization = contextMenu.addAction("Graph Population Visualization");
    graphPopVisualization->setEnabled(!disableAll);
    
    contextMenu.addSeparator();
    
    QAction *createHaplotypePlot = contextMenu.addAction("Create Haplotype Plot");
    createHaplotypePlot->setEnabled(!disableAll && !continuousPlayOn_ && sim && sim->simulation_valid_ && sim->population_.subpops_.size());
    
    // Run the context menu synchronously
    QPoint mousePos = QCursor::pos();
    QAction *action = contextMenu.exec(mousePos);
    
    if (action && !invalidSimulation_)
    {
        QtSLiMGraphView *graphView = nullptr;
        
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
        if (action == graphPopSizeVsTime)
            graphView = new QtSLiMGraphView_PopSizeOverTime(this, this);
        if (action == graphPopVisualization)
            graphView = new QtSLiMGraphView_PopulationVisualization(this, this);
        if (action == createHaplotypePlot)
        {
            if (!continuousPlayOn_ && sim && sim->simulation_valid_ && sim->population_.subpops_.size())
            {
                isTransient = false;    // Since the user has taken an interest in the window, clear the document's transient status
                
                QtSLiMHaplotypeManager::CreateHaplotypePlot(this);
            }
            else
            {
                qApp->beep();
            }
        }
        
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
        Population &population = sim->population_;
        std::map<slim_objectid_t,Subpopulation*> &subpops = population.subpops_;
        size_t subpopCount = subpops.size();
        
        // first get the state of each row, for algorithmic convenience
        std::vector<bool> rowSelectedState(subpopCount, false);
        
        for (QModelIndex &modelIndex : selectedRows)
            rowSelectedState[static_cast<size_t>(modelIndex.row())] = true;
        
        // then loop through subpops and update their selected state
        auto popIter = population.subpops_.begin();
        bool all_selected = true;
        
        for (size_t i = 0; i < subpopCount; ++i)
        {
            popIter->second->gui_selected_ = rowSelectedState[i];
            
            if (!popIter->second->gui_selected_)
                all_selected = false;
            
            popIter++;
        }
        
        population.gui_all_selected_ = all_selected;
        
        // If the selection has changed, that means that the mutation tallies need to be recomputed
        population.TallyMutationReferences(nullptr, true);
        
        // It's a bit hard to tell for sure whether we need to update or not, since a selected subpop might have been removed from the tableview;
        // selection changes should not happen often, so we can just always update, I think.
        ui->individualsWidget->update();
        ui->chromosomeZoomed->update();     // was setNeedsDisplayInInterior, which would be more minimal
    }
}
































