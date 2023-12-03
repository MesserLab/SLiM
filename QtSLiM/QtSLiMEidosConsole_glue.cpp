//
//  QtSLiMEidosConsole_glue.cpp
//  SLiM
//
//  Created by Ben Haller on 12/6/2019.
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


#include "QtSLiMEidosConsole.h"
#include "ui_QtSLiMEidosConsole.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QDesktopServices>

#include "QtSLiMAppDelegate.h"


void QtSLiMEidosConsole::glueUI(void)
{
    connect(ui->consoleTextEdit, &QtSLiMConsoleTextEdit::executeScript, this, &QtSLiMEidosConsole::executePromptScript);
    
    // connect all QtSLiMEidosConsole slots
    connect(ui->checkScriptButton, &QPushButton::clicked, ui->scriptTextEdit, &QtSLiMTextEdit::checkScript);
    connect(ui->prettyprintButton, &QPushButton::clicked, ui->scriptTextEdit, &QtSLiMTextEdit::prettyprintClicked);
    connect(ui->scriptHelpButton, &QPushButton::clicked, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_help);
    connect(ui->browserButton, &QPushButton::clicked, this, &QtSLiMEidosConsole::showBrowserClicked);
    
    connect(ui->executeSelectionButton, &QPushButton::clicked, this, &QtSLiMEidosConsole::executeSelectionClicked);
    connect(ui->executeAllButton, &QPushButton::clicked, this, &QtSLiMEidosConsole::executeAllClicked);

    connect(ui->clearOutputButton, &QPushButton::clicked, ui->consoleTextEdit, &QtSLiMConsoleTextEdit::clearToPrompt);
    
    // set up QtSLiMPushButton "base names" for all buttons
    ui->checkScriptButton->qtslimSetBaseName("check");
    ui->prettyprintButton->qtslimSetBaseName("prettyprint");
    ui->scriptHelpButton->qtslimSetBaseName("syntax_help");
    ui->browserButton->qtslimSetBaseName("show_browser");
    ui->executeSelectionButton->qtslimSetBaseName("execute_selection");
    ui->executeAllButton->qtslimSetBaseName("execute_script");
    ui->clearOutputButton->qtslimSetBaseName("delete");
    
    // set up all icon-based QPushButtons to change their icon as they track
    connect(ui->checkScriptButton, &QPushButton::pressed, this, &QtSLiMEidosConsole::checkScriptPressed);
    connect(ui->checkScriptButton, &QPushButton::released, this, &QtSLiMEidosConsole::checkScriptReleased);
    connect(ui->prettyprintButton, &QPushButton::pressed, this, &QtSLiMEidosConsole::prettyprintPressed);
    connect(ui->prettyprintButton, &QPushButton::released, this, &QtSLiMEidosConsole::prettyprintReleased);
    connect(ui->scriptHelpButton, &QPushButton::pressed, this, &QtSLiMEidosConsole::scriptHelpPressed);
    connect(ui->scriptHelpButton, &QPushButton::released, this, &QtSLiMEidosConsole::scriptHelpReleased);
    connect(ui->browserButton, &QPushButton::pressed, this, &QtSLiMEidosConsole::showBrowserPressed);
    connect(ui->browserButton, &QPushButton::released, this, &QtSLiMEidosConsole::showBrowserReleased);
    
    connect(ui->executeSelectionButton, &QPushButton::pressed, this, &QtSLiMEidosConsole::executeSelectionPressed);
    connect(ui->executeSelectionButton, &QPushButton::released, this, &QtSLiMEidosConsole::executeSelectionReleased);
    connect(ui->executeAllButton, &QPushButton::pressed, this, &QtSLiMEidosConsole::executeAllPressed);
    connect(ui->executeAllButton, &QPushButton::released, this, &QtSLiMEidosConsole::executeAllReleased);
    
    connect(ui->clearOutputButton, &QPushButton::pressed, this, &QtSLiMEidosConsole::clearOutputPressed);
    connect(ui->clearOutputButton, &QPushButton::released, this, &QtSLiMEidosConsole::clearOutputReleased);
    
    // make window actions for all global menu items
    qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
}


//
//  private slots
//

void QtSLiMEidosConsole::checkScriptPressed(void)
{
    ui->checkScriptButton->qtslimSetHighlight(true);
}
void QtSLiMEidosConsole::checkScriptReleased(void)
{
    ui->checkScriptButton->qtslimSetHighlight(false);
}
void QtSLiMEidosConsole::prettyprintPressed(void)
{
    ui->prettyprintButton->qtslimSetHighlight(true);
}
void QtSLiMEidosConsole::prettyprintReleased(void)
{
    ui->prettyprintButton->qtslimSetHighlight(false);
}
void QtSLiMEidosConsole::scriptHelpPressed(void)
{
    ui->scriptHelpButton->qtslimSetHighlight(true);
}
void QtSLiMEidosConsole::scriptHelpReleased(void)
{
    ui->scriptHelpButton->qtslimSetHighlight(false);
}
void QtSLiMEidosConsole::showBrowserPressed(void)
{
    ui->browserButton->qtslimSetHighlight(true);
}
void QtSLiMEidosConsole::showBrowserReleased(void)
{
    ui->browserButton->qtslimSetHighlight(false);
}
void QtSLiMEidosConsole::executeSelectionPressed(void)
{
    ui->executeSelectionButton->qtslimSetHighlight(true);
}
void QtSLiMEidosConsole::executeSelectionReleased(void)
{
    ui->executeSelectionButton->qtslimSetHighlight(false);
}
void QtSLiMEidosConsole::executeAllPressed(void)
{
    ui->executeAllButton->qtslimSetHighlight(true);
}
void QtSLiMEidosConsole::executeAllReleased(void)
{
    ui->executeAllButton->qtslimSetHighlight(false);
}
void QtSLiMEidosConsole::clearOutputPressed(void)
{
    ui->clearOutputButton->qtslimSetHighlight(true);
}
void QtSLiMEidosConsole::clearOutputReleased(void)
{
    ui->clearOutputButton->qtslimSetHighlight(false);
}


































