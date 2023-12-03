//
//  QtSLiMWindow_glue.cpp
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

#include <QCoreApplication>
#include <QKeyEvent>
#include <QDebug>

#include "QtSLiMScriptTextEdit.h"
#include "QtSLiMAppDelegate.h"


void QtSLiMWindow::glueUI(void)
{
    // connect all QtSLiMWindow slots
    //connect(ui->playOneStepButton, &QPushButton::clicked, this, &QtSLiMWindow::playOneStepClicked);   // done in playOneStepPressed() now!
    connect(ui->playButton, &QPushButton::clicked, this, [this]() { playOrProfile(tickPlayOn_ ? PlayType::kTickPlay : PlayType::kNormalPlay); });
    connect(ui->profileButton, &QPushButton::clicked, this, [this]() { playOrProfile(PlayType::kProfilePlay); });
    connect(ui->tickLineEdit, &QLineEdit::returnPressed, this, &QtSLiMWindow::tickChanged);
    //connect(ui->cycleLineEdit, &QLineEdit::returnPressed, this, &QtSLiMWindow::cycleChanged);   // not editable at the moment
    connect(ui->recycleButton, &QPushButton::clicked, this, &QtSLiMWindow::recycleClicked);
    connect(ui->playSpeedSlider, &QSlider::valueChanged, this, &QtSLiMWindow::playSpeedChanged);

    connect(ui->toggleDrawerButton, &QPushButton::clicked, this, &QtSLiMWindow::showDrawerClicked);
    //connect(ui->chromosomeActionButton, &QPushButton::clicked, this, &QtSLiMWindow::chromosomeActionClicked); // this button runs when it is pressed

    connect(ui->clearDebugButton, &QPushButton::clicked, ui->scriptTextEdit, &QtSLiMScriptTextEdit::clearDebugPoints);
    connect(ui->checkScriptButton, &QPushButton::clicked, ui->scriptTextEdit, &QtSLiMTextEdit::checkScript);
    connect(ui->prettyprintButton, &QPushButton::clicked, ui->scriptTextEdit, &QtSLiMTextEdit::prettyprintClicked);
    connect(ui->scriptHelpButton, &QPushButton::clicked, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_help);
    connect(ui->consoleButton, &QPushButton::clicked, this, &QtSLiMWindow::showConsoleClicked);
    connect(ui->browserButton, &QPushButton::clicked, this, &QtSLiMWindow::showBrowserClicked);
    //connect(ui->jumpToPopupButton, &QPushButton::clicked, this, &QtSLiMWindow::jumpToPopupButtonClicked); // this button runs when it is pressed

    connect(ui->clearOutputButton, &QPushButton::clicked, this, &QtSLiMWindow::clearOutputClicked);
    connect(ui->dumpPopulationButton, &QPushButton::clicked, this, &QtSLiMWindow::dumpPopulationClicked);
    connect(ui->debugOutputButton, &QPushButton::clicked, this, &QtSLiMWindow::debugOutputClicked);
    //connect(ui->graphPopupButton, &QPushButton::clicked, this, &QtSLiMWindow::graphPopupButtonClicked); // this button runs when it is pressed
    connect(ui->changeDirectoryButton, &QPushButton::clicked, this, &QtSLiMWindow::changeDirectoryClicked);

    // set up QtSLiMPushButton "base names" for all buttons
    ui->playOneStepButton->qtslimSetBaseName("play_step");
    ui->playButton->qtslimSetBaseName("play");
    ui->profileButton->qtslimSetBaseName("profile");
    ui->recycleButton->qtslimSetBaseName("recycle");
    ui->toggleDrawerButton->qtslimSetBaseName("open_type_drawer");
    ui->chromosomeActionButton->qtslimSetBaseName("action");
    ui->clearDebugButton->qtslimSetBaseName("clear_debug");
    ui->checkScriptButton->qtslimSetBaseName("check");
    ui->prettyprintButton->qtslimSetBaseName("prettyprint");
    ui->scriptHelpButton->qtslimSetBaseName("syntax_help");
    ui->consoleButton->qtslimSetBaseName("show_console");
    ui->browserButton->qtslimSetBaseName("show_browser");
    ui->jumpToPopupButton->qtslimSetBaseName("jump_to");
    ui->clearOutputButton->qtslimSetBaseName("delete");
    ui->dumpPopulationButton->qtslimSetBaseName("dump_output");
    ui->debugOutputButton->qtslimSetBaseName("debug");
    ui->graphPopupButton->qtslimSetBaseName("graph_submenu");
    ui->changeDirectoryButton->qtslimSetBaseName("change_folder");
    
    // set up the "temporary icon" on the debugging button, to support pulsing
    static QIcon *debug_RED = nullptr;
    if (!debug_RED)
        debug_RED = new QIcon(":buttons/debug_RED.png");
    ui->debugOutputButton->setTemporaryIcon(*debug_RED);
    
    // set up all icon-based QPushButtons to change their icon as they track
    connect(ui->playOneStepButton, &QPushButton::pressed, this, &QtSLiMWindow::playOneStepPressed);
    connect(ui->playOneStepButton, &QPushButton::released, this, &QtSLiMWindow::playOneStepReleased);
    connect(ui->playButton, &QPushButton::pressed, this, &QtSLiMWindow::playPressed);
    connect(ui->playButton, &QPushButton::released, this, &QtSLiMWindow::playReleased);
    connect(ui->profileButton, &QPushButton::pressed, this, &QtSLiMWindow::profilePressed);
    connect(ui->profileButton, &QPushButton::released, this, &QtSLiMWindow::profileReleased);
    connect(ui->recycleButton, &QPushButton::pressed, this, &QtSLiMWindow::recyclePressed);
    connect(ui->recycleButton, &QPushButton::released, this, &QtSLiMWindow::recycleReleased);
    connect(ui->toggleDrawerButton, &QPushButton::pressed, this, &QtSLiMWindow::toggleDrawerPressed);
    connect(ui->toggleDrawerButton, &QPushButton::released, this, &QtSLiMWindow::toggleDrawerReleased);
    connect(ui->chromosomeActionButton, &QPushButton::pressed, this, &QtSLiMWindow::chromosomeActionPressed);
    connect(ui->chromosomeActionButton, &QPushButton::released, this, &QtSLiMWindow::chromosomeActionReleased);
    connect(ui->clearDebugButton, &QPushButton::pressed, this, &QtSLiMWindow::clearDebugPressed);
    connect(ui->clearDebugButton, &QPushButton::released, this, &QtSLiMWindow::clearDebugReleased);
    connect(ui->checkScriptButton, &QPushButton::pressed, this, &QtSLiMWindow::checkScriptPressed);
    connect(ui->checkScriptButton, &QPushButton::released, this, &QtSLiMWindow::checkScriptReleased);
    connect(ui->prettyprintButton, &QPushButton::pressed, this, &QtSLiMWindow::prettyprintPressed);
    connect(ui->prettyprintButton, &QPushButton::released, this, &QtSLiMWindow::prettyprintReleased);
    connect(ui->scriptHelpButton, &QPushButton::pressed, this, &QtSLiMWindow::scriptHelpPressed);
    connect(ui->scriptHelpButton, &QPushButton::released, this, &QtSLiMWindow::scriptHelpReleased);
    connect(ui->consoleButton, &QPushButton::pressed, this, &QtSLiMWindow::showConsolePressed);
    connect(ui->consoleButton, &QPushButton::released, this, &QtSLiMWindow::showConsoleReleased);
    connect(ui->browserButton, &QPushButton::pressed, this, &QtSLiMWindow::showBrowserPressed);
    connect(ui->browserButton, &QPushButton::released, this, &QtSLiMWindow::showBrowserReleased);
    connect(ui->jumpToPopupButton, &QPushButton::pressed, this, &QtSLiMWindow::jumpToPopupButtonPressed);
    connect(ui->jumpToPopupButton, &QPushButton::released, this, &QtSLiMWindow::jumpToPopupButtonReleased);
    connect(ui->clearOutputButton, &QPushButton::pressed, this, &QtSLiMWindow::clearOutputPressed);
    connect(ui->clearOutputButton, &QPushButton::released, this, &QtSLiMWindow::clearOutputReleased);
    connect(ui->dumpPopulationButton, &QPushButton::pressed, this, &QtSLiMWindow::dumpPopulationPressed);
    connect(ui->dumpPopulationButton, &QPushButton::released, this, &QtSLiMWindow::dumpPopulationReleased);
    connect(ui->debugOutputButton, &QPushButton::pressed, this, &QtSLiMWindow::debugOutputPressed);
    connect(ui->debugOutputButton, &QPushButton::released, this, &QtSLiMWindow::debugOutputReleased);
    connect(ui->graphPopupButton, &QPushButton::pressed, this, &QtSLiMWindow::graphPopupButtonPressed);
    connect(ui->graphPopupButton, &QPushButton::released, this, &QtSLiMWindow::graphPopupButtonReleased);
    connect(ui->changeDirectoryButton, &QPushButton::pressed, this, &QtSLiMWindow::changeDirectoryPressed);
    connect(ui->changeDirectoryButton, &QPushButton::released, this, &QtSLiMWindow::changeDirectoryReleased);
    
    // this action seems to need to be added to the main window in order to function reliably;
    // I'm not sure why, maybe it is because it is connected to an object that is not a widget?
    // adding it as an action here seems to have no visible effect except that the shortcut now works
    addAction(ui->actionFindRecipe);
    
    // menu items that are not visible, for hidden shortcuts
    QAction *actionNewWF_commentless = new QAction("New WF (Commentless)", this);
    actionNewWF_commentless->setShortcut(Qt::CTRL + Qt::AltModifier + Qt::Key_N);
    connect(actionNewWF_commentless, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_newWF_commentless);
    addAction(actionNewWF_commentless);
    
    QAction *actionNewNonWF_commentless = new QAction("New nonWF (Commentless)", this);
    actionNewNonWF_commentless->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::AltModifier + Qt::Key_N);
    connect(actionNewNonWF_commentless, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_newNonWF_commentless);
    addAction(actionNewNonWF_commentless);
    
    // connect all menu items with existing slots
    connect(ui->actionPreferences, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_preferences);
    connect(ui->actionAboutQtSLiM, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_about);
    connect(ui->actionShowCycle_WF, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_showCycle_WF);
    connect(ui->actionShowCycle_nonWF, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_showCycle_nonWF);
    connect(ui->actionQtSLiMHelp, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_help);
    connect(ui->actionQuitQtSLiM, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_quit);
    connect(ui->actionNew, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_newWF);
    connect(ui->actionNew_nonWF, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_newNonWF);
    connect(ui->actionOpen, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_open);
    connect(ui->actionClose, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_close);
    connect(ui->actionSave, &QAction::triggered, this, &QtSLiMWindow::save);
    connect(ui->actionSaveAs, &QAction::triggered, this, &QtSLiMWindow::saveAs);
    connect(ui->actionRevertToSaved, &QAction::triggered, this, &QtSLiMWindow::revert);
    connect(ui->actionStep, &QAction::triggered, this, &QtSLiMWindow::playOneStepClicked);
    connect(ui->actionPlay, &QAction::triggered, this, [this]() { playOrProfile(tickPlayOn_ ? PlayType::kTickPlay : PlayType::kNormalPlay); });
    connect(ui->actionProfile, &QAction::triggered, this, [this]() { playOrProfile(PlayType::kProfilePlay); });
    connect(ui->actionRecycle, &QAction::triggered, this, &QtSLiMWindow::recycleClicked);
    connect(ui->actionChangeWorkingDirectory, &QAction::triggered, this, &QtSLiMWindow::changeDirectoryClicked);
    connect(ui->actionDumpPopulationState, &QAction::triggered, this, &QtSLiMWindow::dumpPopulationClicked);
    connect(ui->actionMinimize, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_minimize);
    connect(ui->actionZoom, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_zoom);
    
    connect(ui->actionGraph_1D_Population_SFS, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
	connect(ui->actionGraph_1D_Sample_SFS, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
	connect(ui->actionGraph_2D_Population_SFS, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
	connect(ui->actionGraph_2D_Sample_SFS, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
	connect(ui->actionGraph_Mutation_Frequency_Trajectories, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
	connect(ui->actionGraph_Mutation_Loss_Time_Histogram, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
	connect(ui->actionGraph_Mutation_Fixation_Time_Histogram, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
	connect(ui->actionGraph_Population_Fitness_Distribution, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
	connect(ui->actionGraph_Subpopulation_Fitness_Distributions, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
	connect(ui->actionGraph_Fitness_Time, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
	connect(ui->actionGraph_Age_Distribution, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
	connect(ui->actionGraph_Lifetime_Reproduce_Output, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
	connect(ui->actionGraph_Population_Size_Time, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
    connect(ui->actionGraph_Population_Visualization, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
    connect(ui->actionGraph_Multispecies_Population_Size_Time, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
	connect(ui->actionCreate_Haplotype_Plot, &QAction::triggered, this, &QtSLiMWindow::displayGraphClicked);
    
    // connect menu items that can go to either a QtSLiMWindow or a QtSLiMEidosConsole
    connect(ui->actionFocusOnScript, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_focusOnScript);
    connect(ui->actionFocusOnConsole, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_focusOnConsole);
    connect(ui->actionCheckScript, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_checkScript);
    connect(ui->actionPrettyprintScript, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_prettyprintScript);
    connect(ui->actionReformatScript, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_reformatScript);
    connect(ui->actionShowScriptHelp, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_help);
    connect(ui->actionBiggerFont, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_biggerFont);
    connect(ui->actionSmallerFont, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_smallerFont);
    connect(ui->actionShowEidosConsole, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_showEidosConsole);
    connect(ui->actionShowVariableBrowser, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_showVariableBrowser);
    connect(ui->actionClearOutput, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_clearOutput);
    connect(ui->actionClearDebug, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_clearDebugPoints);
    connect(ui->actionShowDebuggingOutput, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_showDebuggingOutput);
    
    // connect menu items that open a URL
    connect(ui->actionSLiMWorkshops, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_helpWorkshops);
    connect(ui->actionSendFeedback, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_helpFeedback);
    connect(ui->actionMailingList_slimdiscuss, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_helpSLiMDiscuss);
    connect(ui->actionMailingList_slimannounce, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_helpSLiMAnnounce);
    connect(ui->actionSLiMHomePage, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_helpSLiMHome);
    connect(ui->actionSLiMExtras, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_helpSLiMExtras);
    connect(ui->actionAboutMesserLab, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_helpMesserLab);
    connect(ui->actionAboutBenHaller, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_helpBenHaller);
    connect(ui->actionAboutStickSoftware, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_helpStickSoftware);
    
    // connect custom menu items
    connect(ui->actionShiftLeft, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_shiftLeft);
    connect(ui->actionShiftRight, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_shiftRight);
    connect(ui->actionCommentUncomment, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_commentUncomment);
    connect(ui->actionExecuteSelection, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_executeSelection);
    connect(ui->actionExecuteAll, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_executeAll);
    
    // standard actions that need to be dispatched (I haven't found a better way to do this;
    // this is basically implementing the first responder / event dispatch mechanism)
    connect(ui->actionUndo, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_undo);
    connect(ui->actionRedo, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_redo);
    connect(ui->actionCut, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_cut);
    connect(ui->actionCopy, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_copy);
    connect(ui->actionPaste, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_paste);
    connect(ui->actionDelete, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_delete);
    connect(ui->actionSelectAll, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_selectAll);
    
    // Find panel actions; these just get forwarded to QtSLiMFindPanel
    connect(ui->actionFindShow, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_findShow);
    connect(ui->actionFindNext, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_findNext);
    connect(ui->actionFindPrevious, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_findPrevious);
    connect(ui->actionReplaceAndFind, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_replaceAndFind);
    connect(ui->actionUseSelectionForFind, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_useSelectionForFind);
    connect(ui->actionUseSelectionForReplace, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_useSelectionForReplace);
    connect(ui->actionJumpToSelection, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_jumpToSelection);
    connect(ui->actionJumpToLine, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_jumpToLine);
}


//
//  private slots
//

void QtSLiMWindow::playPressed(void)
{
    updatePlayButtonIcon(true);
}
void QtSLiMWindow::playReleased(void)
{
    updatePlayButtonIcon(false);
}
void QtSLiMWindow::profilePressed(void)
{
    updateProfileButtonIcon(true);
}
void QtSLiMWindow::profileReleased(void)
{
    updateProfileButtonIcon(false);
}
void QtSLiMWindow::recyclePressed(void)
{
    updateRecycleButtonIcon(true);
}
void QtSLiMWindow::recycleReleased(void)
{
    updateRecycleButtonIcon(false);
}
void QtSLiMWindow::toggleDrawerPressed(void)
{
    ui->toggleDrawerButton->qtslimSetHighlight(true);
}
void QtSLiMWindow::toggleDrawerReleased(void)
{
    ui->toggleDrawerButton->qtslimSetHighlight(false);
}
void QtSLiMWindow::chromosomeActionPressed(void)
{
    ui->chromosomeActionButton->qtslimSetHighlight(true);
    chromosomeActionRunMenu();  // this button runs its menu when it is pressed, so make that call here
}
void QtSLiMWindow::chromosomeActionReleased(void)
{
    ui->chromosomeActionButton->qtslimSetHighlight(false);
}
void QtSLiMWindow::clearDebugPressed(void)
{
    ui->clearDebugButton->qtslimSetHighlight(true);
}
void QtSLiMWindow::clearDebugReleased(void)
{
    ui->clearDebugButton->qtslimSetHighlight(false);
}
void QtSLiMWindow::checkScriptPressed(void)
{
    ui->checkScriptButton->qtslimSetHighlight(true);
}
void QtSLiMWindow::checkScriptReleased(void)
{
    ui->checkScriptButton->qtslimSetHighlight(false);
}
void QtSLiMWindow::prettyprintPressed(void)
{
    ui->prettyprintButton->qtslimSetHighlight(true);
}
void QtSLiMWindow::prettyprintReleased(void)
{
    ui->prettyprintButton->qtslimSetHighlight(false);
}
void QtSLiMWindow::scriptHelpPressed(void)
{
    ui->scriptHelpButton->qtslimSetHighlight(true);
}
void QtSLiMWindow::scriptHelpReleased(void)
{
    ui->scriptHelpButton->qtslimSetHighlight(false);
}
void QtSLiMWindow::showConsolePressed(void)
{
    ui->consoleButton->qtslimSetHighlight(true);
}
void QtSLiMWindow::showConsoleReleased(void)
{
    ui->consoleButton->qtslimSetHighlight(false);
}
void QtSLiMWindow::showBrowserPressed(void)
{
    ui->browserButton->qtslimSetHighlight(true);
}
void QtSLiMWindow::showBrowserReleased(void)
{
    ui->browserButton->qtslimSetHighlight(false);
}
void QtSLiMWindow::jumpToPopupButtonPressed(void)
{
    ui->jumpToPopupButton->qtslimSetHighlight(true);
    jumpToPopupButtonRunMenu();  // this button runs its menu when it is pressed, so make that call here
}
void QtSLiMWindow::jumpToPopupButtonReleased(void)
{
    ui->jumpToPopupButton->qtslimSetHighlight(false);
}
void QtSLiMWindow::clearOutputPressed(void)
{
    ui->clearOutputButton->qtslimSetHighlight(true);
}
void QtSLiMWindow::clearOutputReleased(void)
{
    ui->clearOutputButton->qtslimSetHighlight(false);
}
void QtSLiMWindow::dumpPopulationPressed(void)
{
    ui->dumpPopulationButton->qtslimSetHighlight(true);
}
void QtSLiMWindow::dumpPopulationReleased(void)
{
    ui->dumpPopulationButton->qtslimSetHighlight(false);
}
void QtSLiMWindow::debugOutputPressed(void)
{
    ui->debugOutputButton->qtslimSetHighlight(true);
    stopDebugButtonFlash();
}
void QtSLiMWindow::debugOutputReleased(void)
{
    ui->debugOutputButton->qtslimSetHighlight(false);
    stopDebugButtonFlash();
}
void QtSLiMWindow::graphPopupButtonPressed(void)
{
    ui->graphPopupButton->qtslimSetHighlight(true);
    graphPopupButtonRunMenu();  // this button runs its menu when it is pressed, so make that call here
}
void QtSLiMWindow::graphPopupButtonReleased(void)
{
    ui->graphPopupButton->qtslimSetHighlight(false);
}
void QtSLiMWindow::changeDirectoryPressed(void)
{
    ui->changeDirectoryButton->qtslimSetHighlight(true);
}
void QtSLiMWindow::changeDirectoryReleased(void)
{
    ui->changeDirectoryButton->qtslimSetHighlight(false);
}
