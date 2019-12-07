#include "QtSLiMEidosConsole.h"
#include "ui_QtSLiMEidosConsole.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QDesktopServices>

#include "QtSLiMWindow.h"


void QtSLiMEidosConsole::glueUI(void)
{
    connect(ui->consoleTextEdit, &QtSLiMConsoleTextEdit::executeScript, this, &QtSLiMEidosConsole::executePromptScript);
    
    // connect all QtSLiMEidosConsole slots
    connect(ui->checkScriptButton, &QPushButton::clicked, ui->scriptTextEdit, &QtSLiMTextEdit::checkScript);
    connect(ui->prettyprintButton, &QPushButton::clicked, ui->scriptTextEdit, &QtSLiMTextEdit::prettyprint);
    connect(ui->scriptHelpButton, &QPushButton::clicked, parentSLiMWindow, &QtSLiMWindow::scriptHelpClicked);
    connect(ui->browserButton, &QPushButton::clicked, parentSLiMWindow, &QtSLiMWindow::showBrowserClicked);
    
    connect(ui->executeSelectionButton, &QPushButton::clicked, this, &QtSLiMEidosConsole::executeSelectionClicked);
    connect(ui->executeAllButton, &QPushButton::clicked, this, &QtSLiMEidosConsole::executeAllClicked);

    connect(ui->clearOutputButton, &QPushButton::clicked, ui->consoleTextEdit, &QtSLiMConsoleTextEdit::clearToPrompt);
    
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
}


//
//  private slots
//

void QtSLiMEidosConsole::checkScriptPressed(void)
{
    ui->checkScriptButton->setIcon(QIcon(":/buttons/check_H.png"));
}
void QtSLiMEidosConsole::checkScriptReleased(void)
{
    ui->checkScriptButton->setIcon(QIcon(":/buttons/check.png"));
}
void QtSLiMEidosConsole::prettyprintPressed(void)
{
    ui->prettyprintButton->setIcon(QIcon(":/buttons/prettyprint_H.png"));
}
void QtSLiMEidosConsole::prettyprintReleased(void)
{
    ui->prettyprintButton->setIcon(QIcon(":/buttons/prettyprint.png"));
}
void QtSLiMEidosConsole::scriptHelpPressed(void)
{
    ui->scriptHelpButton->setIcon(QIcon(":/buttons/syntax_help_H.png"));
}
void QtSLiMEidosConsole::scriptHelpReleased(void)
{
    ui->scriptHelpButton->setIcon(QIcon(":/buttons/syntax_help.png"));
}
void QtSLiMEidosConsole::showBrowserPressed(void)
{
    ui->browserButton->setIcon(QIcon(ui->browserButton->isChecked() ? ":/buttons/show_browser.png" : ":/buttons/show_browser_H.png"));
}
void QtSLiMEidosConsole::showBrowserReleased(void)
{
    ui->browserButton->setIcon(QIcon(ui->browserButton->isChecked() ? ":/buttons/show_browser_H.png" : ":/buttons/show_browser.png"));
}
void QtSLiMEidosConsole::executeSelectionPressed(void)
{
    ui->executeSelectionButton->setIcon(QIcon(":/buttons/execute_selection_H.png"));
}
void QtSLiMEidosConsole::executeSelectionReleased(void)
{
    ui->executeSelectionButton->setIcon(QIcon(":/buttons/execute_selection.png"));
}
void QtSLiMEidosConsole::executeAllPressed(void)
{
    ui->executeAllButton->setIcon(QIcon(":/buttons/execute_script_H.png"));
}
void QtSLiMEidosConsole::executeAllReleased(void)
{
    ui->executeAllButton->setIcon(QIcon(":/buttons/execute_script.png"));
}
void QtSLiMEidosConsole::clearOutputPressed(void)
{
    ui->clearOutputButton->setIcon(QIcon(":/buttons/delete_H.png"));
}
void QtSLiMEidosConsole::clearOutputReleased(void)
{
    ui->clearOutputButton->setIcon(QIcon(":/buttons/delete.png"));
}


































