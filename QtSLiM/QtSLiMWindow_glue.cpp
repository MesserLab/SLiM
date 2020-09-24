//
//  QtSLiMWindow_glue.cpp
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

#include <QCoreApplication>
#include <QKeyEvent>

#include "QtSLiMScriptTextEdit.h"
#include "QtSLiMEidosConsole.h"
#include "QtSLiMVariableBrowser.h"
#include "QtSLiMConsoleTextEdit.h"
#include "QtSLiMAppDelegate.h"
#include "QtSLiMFindPanel.h"


void QtSLiMWindow::glueUI(void)
{
    // connect all QtSLiMWindow slots
    //connect(ui->playOneStepButton, &QPushButton::clicked, this, &QtSLiMWindow::playOneStepClicked);   // done in playOneStepPressed() now!
    connect(ui->playButton, &QPushButton::clicked, this, [this]() { playOrProfile(generationPlayOn_ ? PlayType::kGenerationPlay : PlayType::kNormalPlay); });
    connect(ui->profileButton, &QPushButton::clicked, this, [this]() { playOrProfile(PlayType::kProfilePlay); });
    connect(ui->generationLineEdit, &QLineEdit::returnPressed, this, &QtSLiMWindow::generationChanged);
    connect(ui->recycleButton, &QPushButton::clicked, this, &QtSLiMWindow::recycleClicked);
    connect(ui->playSpeedSlider, &QSlider::valueChanged, this, &QtSLiMWindow::playSpeedChanged);

    connect(ui->toggleDrawerButton, &QPushButton::clicked, this, &QtSLiMWindow::toggleDrawerToggled);
    connect(ui->showMutationsButton, &QPushButton::clicked, this, &QtSLiMWindow::showMutationsToggled);
    connect(ui->showFixedSubstitutionsButton, &QPushButton::clicked, this, &QtSLiMWindow::showFixedSubstitutionsToggled);
    connect(ui->showChromosomeMapsButton, &QPushButton::clicked, this, &QtSLiMWindow::showChromosomeMapsToggled);
    connect(ui->showGenomicElementsButton, &QPushButton::clicked, this, &QtSLiMWindow::showGenomicElementsToggled);

    connect(ui->checkScriptButton, &QPushButton::clicked, ui->scriptTextEdit, &QtSLiMTextEdit::checkScript);
    connect(ui->prettyprintButton, &QPushButton::clicked, ui->scriptTextEdit, &QtSLiMTextEdit::prettyprintClicked);
    connect(ui->scriptHelpButton, &QPushButton::clicked, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_help);
    connect(ui->consoleButton, &QPushButton::clicked, this, &QtSLiMWindow::showConsoleClicked);
    connect(ui->browserButton, &QPushButton::clicked, this, &QtSLiMWindow::showBrowserClicked);
    //connect(ui->jumpToPopupButton, &QPushButton::clicked, this, &QtSLiMWindow::jumpToPopupButtonClicked); // this button runs when it is pressed

    connect(ui->clearOutputButton, &QPushButton::clicked, this, &QtSLiMWindow::clearOutputClicked);
    connect(ui->dumpPopulationButton, &QPushButton::clicked, this, &QtSLiMWindow::dumpPopulationClicked);
    //connect(ui->graphPopupButton, &QPushButton::clicked, this, &QtSLiMWindow::graphPopupButtonClicked); // this button runs when it is pressed
    connect(ui->changeDirectoryButton, &QPushButton::clicked, this, &QtSLiMWindow::changeDirectoryClicked);

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
    connect(ui->showMutationsButton, &QPushButton::pressed, this, &QtSLiMWindow::showMutationsPressed);
    connect(ui->showMutationsButton, &QPushButton::released, this, &QtSLiMWindow::showMutationsReleased);
    connect(ui->showFixedSubstitutionsButton, &QPushButton::pressed, this, &QtSLiMWindow::showFixedSubstitutionsPressed);
    connect(ui->showFixedSubstitutionsButton, &QPushButton::released, this, &QtSLiMWindow::showFixedSubstitutionsReleased);
    connect(ui->showChromosomeMapsButton, &QPushButton::pressed, this, &QtSLiMWindow::showChromosomeMapsPressed);
    connect(ui->showChromosomeMapsButton, &QPushButton::released, this, &QtSLiMWindow::showChromosomeMapsReleased);
    connect(ui->showGenomicElementsButton, &QPushButton::pressed, this, &QtSLiMWindow::showGenomicElementsPressed);
    connect(ui->showGenomicElementsButton, &QPushButton::released, this, &QtSLiMWindow::showGenomicElementsReleased);
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
    connect(ui->graphPopupButton, &QPushButton::pressed, this, &QtSLiMWindow::graphPopupButtonPressed);
    connect(ui->graphPopupButton, &QPushButton::released, this, &QtSLiMWindow::graphPopupButtonReleased);
    connect(ui->changeDirectoryButton, &QPushButton::pressed, this, &QtSLiMWindow::changeDirectoryPressed);
    connect(ui->changeDirectoryButton, &QPushButton::released, this, &QtSLiMWindow::changeDirectoryReleased);
    
    // this action seems to need to be added to the main window in order to function reliably;
    // I'm not sure why, maybe it is because it is connected to an object that is not a widget?
    // adding it as an action here seems to have no visible effect except that the shortcut now works
    addAction(ui->actionFindRecipe);
    
    // connect all menu items with existing slots
    connect(ui->actionPreferences, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_preferences);
    connect(ui->actionAboutQtSLiM, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_about);
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
    connect(ui->actionPlay, &QAction::triggered, this, [this]() { playOrProfile(generationPlayOn_ ? PlayType::kGenerationPlay : PlayType::kNormalPlay); });
    connect(ui->actionProfile, &QAction::triggered, this, [this]() { playOrProfile(PlayType::kProfilePlay); });
    connect(ui->actionRecycle, &QAction::triggered, this, &QtSLiMWindow::recycleClicked);
    connect(ui->actionChangeWorkingDirectory, &QAction::triggered, this, &QtSLiMWindow::changeDirectoryClicked);
    connect(ui->actionDumpPopulationState, &QAction::triggered, this, &QtSLiMWindow::dumpPopulationClicked);
    connect(ui->actionMinimize, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_minimize);
    connect(ui->actionZoom, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_zoom);
    
    // connect menu items that can go to either a QtSLiMWindow or a QtSLiMEidosConsole
    connect(ui->actionCheckScript, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_checkScript);
    connect(ui->actionPrettyprintScript, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_prettyprintScript);
    connect(ui->actionReformatScript, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_reformatScript);
    connect(ui->actionShowScriptHelp, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_help);
    connect(ui->actionShowEidosConsole, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_showEidosConsole);
    connect(ui->actionShowVariableBrowser, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_showVariableBrowser);
    connect(ui->actionClearOutput, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_clearOutput);
    
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
    ui->toggleDrawerButton->setIcon(QIcon(ui->toggleDrawerButton->isChecked() ? ":/buttons/open_type_drawer.png" : ":/buttons/open_type_drawer_H.png"));
}
void QtSLiMWindow::toggleDrawerReleased(void)
{
    ui->toggleDrawerButton->setIcon(QIcon(ui->toggleDrawerButton->isChecked() ? ":/buttons/open_type_drawer_H.png" : ":/buttons/open_type_drawer.png"));
}
void QtSLiMWindow::showMutationsPressed(void)
{
    ui->showMutationsButton->setIcon(QIcon(ui->showMutationsButton->isChecked() ? ":/buttons/show_mutations.png" : ":/buttons/show_mutations_H.png"));
}
void QtSLiMWindow::showMutationsReleased(void)
{
    ui->showMutationsButton->setIcon(QIcon(ui->showMutationsButton->isChecked() ? ":/buttons/show_mutations_H.png" : ":/buttons/show_mutations.png"));
}
void QtSLiMWindow::showFixedSubstitutionsPressed(void)
{
    ui->showFixedSubstitutionsButton->setIcon(QIcon(ui->showFixedSubstitutionsButton->isChecked() ? ":/buttons/show_fixed.png" : ":/buttons/show_fixed_H.png"));
}
void QtSLiMWindow::showFixedSubstitutionsReleased(void)
{
    ui->showFixedSubstitutionsButton->setIcon(QIcon(ui->showFixedSubstitutionsButton->isChecked() ? ":/buttons/show_fixed_H.png" : ":/buttons/show_fixed.png"));
}
void QtSLiMWindow::showChromosomeMapsPressed(void)
{
    ui->showChromosomeMapsButton->setIcon(QIcon(ui->showChromosomeMapsButton->isChecked() ? ":/buttons/show_recombination.png" : ":/buttons/show_recombination_H.png"));
}
void QtSLiMWindow::showChromosomeMapsReleased(void)
{
    ui->showChromosomeMapsButton->setIcon(QIcon(ui->showChromosomeMapsButton->isChecked() ? ":/buttons/show_recombination_H.png" : ":/buttons/show_recombination.png"));
}
void QtSLiMWindow::showGenomicElementsPressed(void)
{
    ui->showGenomicElementsButton->setIcon(QIcon(ui->showGenomicElementsButton->isChecked() ? ":/buttons/show_genomicelements.png" : ":/buttons/show_genomicelements_H.png"));
}
void QtSLiMWindow::showGenomicElementsReleased(void)
{
    ui->showGenomicElementsButton->setIcon(QIcon(ui->showGenomicElementsButton->isChecked() ? ":/buttons/show_genomicelements_H.png" : ":/buttons/show_genomicelements.png"));
}
void QtSLiMWindow::checkScriptPressed(void)
{
    ui->checkScriptButton->setIcon(QIcon(":/buttons/check_H.png"));
}
void QtSLiMWindow::checkScriptReleased(void)
{
    ui->checkScriptButton->setIcon(QIcon(":/buttons/check.png"));
}
void QtSLiMWindow::prettyprintPressed(void)
{
    ui->prettyprintButton->setIcon(QIcon(":/buttons/prettyprint_H.png"));
}
void QtSLiMWindow::prettyprintReleased(void)
{
    ui->prettyprintButton->setIcon(QIcon(":/buttons/prettyprint.png"));
}
void QtSLiMWindow::scriptHelpPressed(void)
{
    ui->scriptHelpButton->setIcon(QIcon(":/buttons/syntax_help_H.png"));
}
void QtSLiMWindow::scriptHelpReleased(void)
{
    ui->scriptHelpButton->setIcon(QIcon(":/buttons/syntax_help.png"));
}
void QtSLiMWindow::showConsolePressed(void)
{
    ui->consoleButton->setIcon(QIcon(ui->consoleButton->isChecked() ? ":/buttons/show_console.png" : ":/buttons/show_console_H.png"));
}
void QtSLiMWindow::showConsoleReleased(void)
{
    ui->consoleButton->setIcon(QIcon(ui->consoleButton->isChecked() ? ":/buttons/show_console_H.png" : ":/buttons/show_console.png"));
}
void QtSLiMWindow::showBrowserPressed(void)
{
    ui->browserButton->setIcon(QIcon(ui->browserButton->isChecked() ? ":/buttons/show_browser.png" : ":/buttons/show_browser_H.png"));
}
void QtSLiMWindow::showBrowserReleased(void)
{
    ui->browserButton->setIcon(QIcon(ui->browserButton->isChecked() ? ":/buttons/show_browser_H.png" : ":/buttons/show_browser.png"));
}
void QtSLiMWindow::jumpToPopupButtonPressed(void)
{
    ui->jumpToPopupButton->setIcon(QIcon(":/buttons/jump_to_H.png"));
    jumpToPopupButtonRunMenu();  // this button runs its menu when it is pressed, so make that call here
}
void QtSLiMWindow::jumpToPopupButtonReleased(void)
{
    ui->jumpToPopupButton->setIcon(QIcon(":/buttons/jump_to.png"));
}
void QtSLiMWindow::clearOutputPressed(void)
{
    ui->clearOutputButton->setIcon(QIcon(":/buttons/delete_H.png"));
}
void QtSLiMWindow::clearOutputReleased(void)
{
    ui->clearOutputButton->setIcon(QIcon(":/buttons/delete.png"));
}
void QtSLiMWindow::dumpPopulationPressed(void)
{
    ui->dumpPopulationButton->setIcon(QIcon(":/buttons/dump_output_H.png"));
}
void QtSLiMWindow::dumpPopulationReleased(void)
{
    ui->dumpPopulationButton->setIcon(QIcon(":/buttons/dump_output.png"));
}
void QtSLiMWindow::graphPopupButtonPressed(void)
{
    ui->graphPopupButton->setIcon(QIcon(":/buttons/graph_submenu_H.png"));
    graphPopupButtonRunMenu();  // this button runs its menu when it is pressed, so make that call here
}
void QtSLiMWindow::graphPopupButtonReleased(void)
{
    ui->graphPopupButton->setIcon(QIcon(":/buttons/graph_submenu.png"));
}
void QtSLiMWindow::changeDirectoryPressed(void)
{
    ui->changeDirectoryButton->setIcon(QIcon(":/buttons/change_folder_H.png"));
}
void QtSLiMWindow::changeDirectoryReleased(void)
{
    ui->changeDirectoryButton->setIcon(QIcon(":/buttons/change_folder.png"));
}
