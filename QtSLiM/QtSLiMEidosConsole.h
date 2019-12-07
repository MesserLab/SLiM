#ifndef QTSLIMEIDOSCONSOLE_H
#define QTSLIMEIDOSCONSOLE_H

#include <QDialog>
#include <QString>
#include <QTextCursor>

class QCloseEvent;
class QtSLiMWindow;

#include "eidos_script.h"
#include "eidos_globals.h"
#include "eidos_interpreter.h"
#include "eidos_call_signature.h"


namespace Ui {
class QtSLiMEidosConsole;
}

class QtSLiMEidosConsole : public QDialog
{
    Q_OBJECT
    
public:
    QtSLiMWindow *parentSLiMWindow = nullptr;     // a copy of parent with the correct class, for convenience
    
    explicit QtSLiMEidosConsole(QtSLiMWindow *parent = nullptr);
    virtual ~QtSLiMEidosConsole() override;
    
    // Enable/disable the user interface as the simulation's state changes
    void setInterfaceEnabled(bool enabled);
    
    // Throw away the current symbol table
    void invalidateSymbolTableAndFunctionMap(void);
    
    // Make a new symbol table from our delegate's current state; this actually executes a minimal script, ";",
    // to produce the symbol table as a side effect of setting up for the script's execution
    void validateSymbolTableAndFunctionMap(void);
    
    // Execute the given script string, with the terminating semicolon being optional if requested
    void executeScriptString(QString scriptString, bool semicolonOptional);
    
public slots:
    void checkScriptClicked(void);
    void prettyprintClicked(void);
    void clearOutputClicked(void);
    void executeAllClicked(void);
    void executeSelectionClicked(void);
    
signals:
    void willClose(void);
    
    //
    //  UI glue, defined in QtSLiMWindow_glue.cpp
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
    
    void closeEvent(QCloseEvent *event) override;
    
private:
    Ui::QtSLiMEidosConsole *ui;
    void glueUI(void);
    
    bool interfaceEnabled = false;              // set to false when the simulation is running or invalid
    
    // The symbol table for the console interpreter; needs to be wiped whenever the symbol table changes
	EidosSymbolTable *global_symbols = nullptr;
	
	// The function map for the console interpreter; carries over from invocation to invocation
	EidosFunctionMap *global_function_map = nullptr;
    bool global_function_map_owned = false;

    // Execution internals
    QString _executeScriptString(QString scriptString, QString *tokenString, QString *parseString,
                                 QString *executionString, QString *errorString, bool semicolonOptional);
};


#endif // QTSLIMEIDOSCONSOLE_H


































