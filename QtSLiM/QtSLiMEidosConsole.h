//
//  QtSLiMEidosConsole.h
//  SLiM
//
//  Created by Ben Haller on 12/6/2019.
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

#ifndef QTSLIMEIDOSCONSOLE_H
#define QTSLIMEIDOSCONSOLE_H

#include <QWidget>
#include <QString>
#include <QTextCursor>

class QCloseEvent;
class QtSLiMWindow;
class QStatusBar;
class QSplitter;
class QtSLiMScriptTextEdit;
class QtSLiMConsoleTextEdit;
class QtSLiMVariableBrowser;

#include "eidos_script.h"
#include "eidos_globals.h"
#include "eidos_interpreter.h"
#include "eidos_call_signature.h"


namespace Ui {
class QtSLiMEidosConsole;
}

class QtSLiMEidosConsole : public QWidget
{
    Q_OBJECT
    
public:
    QtSLiMWindow *parentSLiMWindow = nullptr;     // a copy of parent with the correct class, for convenience
    
    explicit QtSLiMEidosConsole(QtSLiMWindow *parent = nullptr);
    virtual ~QtSLiMEidosConsole() override;
    
    // Enable/disable the user interface as the simulation's state changes
    void setInterfaceEnabled(bool enabled);
    
    // Variable browser state
    void setVariableBrowserVisibility(bool visible);
    
    // Throw away the current symbol table
    void invalidateSymbolTableAndFunctionMap(void);
    
    // Make a new symbol table from our delegate's current state; this actually executes a minimal script, ";",
    // to produce the symbol table as a side effect of setting up for the script's execution
    void validateSymbolTableAndFunctionMap(void);
    
    EidosSymbolTable *symbolTable(void) { return global_symbols; }
    
    // Execute the given script string, with the terminating semicolon being optional if requested
    void executeScriptString(QString scriptString, bool semicolonOptional);
    
    // Access to key UI items
    QStatusBar *statusBar(void);
    QtSLiMScriptTextEdit *scriptTextEdit(void);
    QtSLiMConsoleTextEdit *consoleTextEdit(void);
    QtSLiMVariableBrowser *variableBrowser(void);
    
public slots:
    void executeAllClicked(void);
    void executeSelectionClicked(void);
    
signals:
    void willClose(void);
    
    //
    //  UI glue, defined in QtSLiMEidosConsole_glue.cpp
    //
    
private slots:
    void checkScriptPressed(void);
    void checkScriptReleased(void);
    void prettyprintPressed(void);
    void prettyprintReleased(void);
    void scriptHelpPressed(void);
    void scriptHelpReleased(void);
    void showBrowserPressed(void);
    void showBrowserReleased(void);
    
    void executeSelectionPressed(void);
    void executeSelectionReleased(void);
    void executeAllPressed(void);
    void executeAllReleased(void);
    void executePromptScript(QString executionString);

    void clearOutputPressed(void);
    void clearOutputReleased(void);
    
    virtual void closeEvent(QCloseEvent *event) override;
    
private:
    Ui::QtSLiMEidosConsole *ui;
    QStatusBar *statusBar_ = nullptr;
    void glueUI(void);
    
    bool interfaceEnabled = false;              // set to false when the simulation is running or invalid
    
    // Variable browser support
    void updateVariableBrowserButtonStates(bool visible);
    QtSLiMVariableBrowser *variableBrowser_ = nullptr;
    
    // The symbol table for the console interpreter; needs to be wiped whenever the symbol table changes
	EidosSymbolTable *global_symbols = nullptr;
	
	// The function map for the console interpreter; carries over from invocation to invocation
	EidosFunctionMap *global_function_map = nullptr;
    bool global_function_map_owned = false;

    // Execution internals
    QString _executeScriptString(QString scriptString, QString *tokenString, QString *parseString,
                                 QString *executionString, QString *errorString, bool semicolonOptional);
    
    
    // splitter support
    void interpolateSplitters(void);
    QWidget *scriptWidget = nullptr;
    QWidget *outputWidget = nullptr;
    QSplitter *splitter = nullptr;
};


#endif // QTSLIMEIDOSCONSOLE_H


































