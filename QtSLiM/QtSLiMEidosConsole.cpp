//
//  QtSLiMEidosConsole.cpp
//  SLiM
//
//  Created by Ben Haller on 12/6/2019.
//  Copyright (c) 2019-2025 Benjamin C. Haller.  All rights reserved.
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

#include <QStatusBar>
#include <QSettings>
#include <QSplitter>
#include <QDebug>

#include <utility>
#include <string>

#include "QtSLiMWindow.h"
#include "QtSLiMVariableBrowser.h"
#include "QtSLiMExtras.h"


QtSLiMEidosConsole::QtSLiMEidosConsole(QtSLiMWindow *p_parent) :
    QWidget(p_parent, Qt::Window),    // the console window has us as a parent, but is still a standalone window
    parentSLiMWindow(p_parent),
    ui(new Ui::QtSLiMEidosConsole)
{
    ui->setupUi(this);
    interpolateSplitters();
    glueUI();
    
    // no window icon
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    setWindowIcon(QIcon());
#endif
    
    // prevent this window from keeping the app running when all main windows are closed
    setAttribute(Qt::WA_QuitOnClose, false);
    
    // add a status bar at the bottom; there is a layout in Designer for it already
    // thanks to https://stackoverflow.com/a/6143818/2752221
    statusBar_ = new QtSLiMStatusBar(this);
    ui->statusBarLayout->addWidget(statusBar_);
    statusBar_->setMaximumHeight(statusBar_->sizeHint().height());
    
    // set up the script view to syntax highlight
    ui->scriptTextEdit->setScriptType(QtSLiMTextEdit::EidosScriptType);
    ui->scriptTextEdit->setSyntaxHighlightType(QtSLiMTextEdit::ScriptHighlighting);
    
    // set up the console view to be Eidos script, but without highlighting
    ui->consoleTextEdit->setScriptType(QtSLiMTextEdit::EidosScriptType);
    ui->consoleTextEdit->setSyntaxHighlightType(QtSLiMTextEdit::NoHighlighting);
    
    // enable option-click in both textedits
    ui->scriptTextEdit->setOptionClickEnabled(true);
    ui->consoleTextEdit->setOptionClickEnabled(true);
    
    // enable code completion in both textedits
    ui->scriptTextEdit->setCodeCompletionEnabled(true);
    ui->consoleTextEdit->setCodeCompletionEnabled(true);
    
    // set initial text in console and show the initial prompt
    QtSLiMConsoleTextEdit *console = ui->consoleTextEdit;
    console->showWelcome();
    console->showPrompt();
    console->setFocus();
    
    // Restore the saved window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMEidosConsole");
    resize(settings.value("size", QSize(550, 400)).toSize());
    move(settings.value("pos", QPoint(25, 45)).toPoint());
    settings.endGroup();
    
    // Enable our UI initially
    setInterfaceEnabled(true);
    
    // Execute a null statement to get our symbols set up, for code completion etc.
	// Note this has the side effect of creating a random number generator gEidos_RNG for our use.
	validateSymbolTableAndFunctionMap();
}

QtSLiMEidosConsole::~QtSLiMEidosConsole()
{
    delete ui;
    
    delete global_symbols;
    global_symbols = nullptr;
    
    if (global_function_map_owned)
        delete global_function_map;
    global_function_map = nullptr;
}

void QtSLiMEidosConsole::interpolateSplitters(void)
{
#if 1
    // add a top-level horizontal splitter
    
    const int splitterMargin = 0;
    QLayout *parentLayout = ui->overallLayout;
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
    
    // change fixed-size views to be flexible, so they cooperate with the splitter
    ui->scriptTextEdit->setMinimumWidth(250);
    ui->consoleTextEdit->setMinimumWidth(250);
    
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
    splitter = new QSplitter(Qt::Horizontal, this);
    
    splitter->setChildrenCollapsible(true);
    splitter->addWidget(scriptWidget);
    splitter->addWidget(outputWidget);
    splitter->setHandleWidth(splitter->handleWidth() + 3);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);    // initially, give 2/3 of the width to the output widget
    
    // and finally, add the splitter to the parent layout
    parentLayout->addWidget(splitter);
    parentLayout->setContentsMargins(0, 0, 0, 0);
#endif
}

void QtSLiMEidosConsole::closeEvent(QCloseEvent *p_event)
{
    // Save the window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMEidosConsole");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
    
    // send our close signal
    emit willClose();
    
    // use super's default behavior
    QWidget::closeEvent(p_event);
}

void QtSLiMEidosConsole::showBrowserClicked(void)
{
    if (!variableBrowser_)
    {
        variableBrowser_ = new QtSLiMVariableBrowser(this);
        
        if (variableBrowser_)
        {
            variableBrowser_->setAttribute(Qt::WA_DeleteOnClose);
            
            // wire ourselves up to monitor the console for closing, to free
            connect(variableBrowser_, &QtSLiMVariableBrowser::willClose, this, [this]() {
                variableBrowser_ = nullptr;     // deleted on close
            });
        }
        else
        {
            qDebug() << "Could not create variable browser";
        }
    }
    
    QtSLiMMakeWindowVisibleAndExposed(variableBrowser_);
}

QStatusBar *QtSLiMEidosConsole::statusBar(void)
{
    return statusBar_;
}

QtSLiMScriptTextEdit *QtSLiMEidosConsole::scriptTextEdit(void)
{
    return ui->scriptTextEdit;
}

QtSLiMConsoleTextEdit *QtSLiMEidosConsole::consoleTextEdit(void)
{
    return ui->consoleTextEdit;
}

QtSLiMVariableBrowser *QtSLiMEidosConsole::variableBrowser(void)
{
    return variableBrowser_;
}

// enable/disable the user interface as the simulation's state changes
void QtSLiMEidosConsole::setInterfaceEnabled(bool enabled)
{
    ui->checkScriptButton->setEnabled(enabled);
    ui->prettyprintButton->setEnabled(enabled);
    ui->executeAllButton->setEnabled(enabled);
    ui->executeSelectionButton->setEnabled(enabled);
    ui->consoleTextEdit->setReadOnlyDueToActivation(!enabled);
}

// Throw away the current symbol table
void QtSLiMEidosConsole::invalidateSymbolTableAndFunctionMap(void)
{
    if (global_symbols)
	{
		delete global_symbols;
		global_symbols = nullptr;
	}
	
	if (global_function_map)
	{
        if (global_function_map_owned)
            delete global_function_map;
		global_function_map = nullptr;
	}
	
    if (variableBrowser_)
        variableBrowser_->reloadBrowser(false);     // false tells the browser we're now invalid
}

// Make a new symbol table from our delegate's current state; this actually executes a minimal script, ";",
// to produce the symbol table as a side effect of setting up for the script's execution
void QtSLiMEidosConsole::validateSymbolTableAndFunctionMap(void)
{
    if (!global_symbols || !global_function_map)
	{
		QString errorString;
		
		_executeScriptString(";", nullptr, nullptr, nullptr, &errorString, false);
		
		if (errorString.length())
			qDebug() << "Error in validateSymbolTableAndFunctionMap: " << errorString;
	}
	
    if (variableBrowser_)
        variableBrowser_->reloadBrowser(true);
}

// Low-level script execution
QString QtSLiMEidosConsole::_executeScriptString(QString scriptString, QString *tokenString, QString *parseString,
                             QString *executionString, QString *errorString, bool semicolonOptional)
{
    // the back end can't handle Unicode well at present, being based on std::string...
    scriptString.replace(QChar::ParagraphSeparator, "\n");
    scriptString.replace(QChar::LineSeparator, "\n");
    
    std::string script_string(scriptString.toStdString());
	EidosScript script(script_string);
	std::string output;
	
	// Unfortunately, running readFromPopulationFile() is too much of a shock for SLiMgui.  It invalidates variables that are being displayed in
	// the variable browser, in such an abrupt way that it causes a crash.  Basically, the code in readFromPopulationFile() that "cleans" all
	// references to mutations and such does not have any way to clean SLiMgui's references, and so those stale references cause a crash.
	// There is probably a better solution, but for now, we look for code containing readFromPopulationFile() and special-case it.  The user
	// could circumvent this and trigger a crash, so this is just a band-aid; a proper solution is needed.  Another problem with this band-aid
	// is that SLiMgui's display does not refresh to show the new population state.  Indeed, that is an issue with anything that changes the
	// visible state, such as adding new mutations.  There needs to be some way for Eidos code to tell SLiMgui that UI refreshing is needed,
	// and to clean references to variables that are about to invalidated.  FIXME
	bool safeguardReferences = false;
	
    if (scriptString.contains("readFromPopulationFile"))
		safeguardReferences = true;
	
	if (safeguardReferences)
		invalidateSymbolTableAndFunctionMap();
	
	// Make the final semicolon optional if requested; this allows input like "6+7" in the console
	if (semicolonOptional)
		script.SetFinalSemicolonOptional(true);
	
	// Tokenize
	try
	{
		script.Tokenize();
		
		if (tokenString)
		{
			std::ostringstream token_stream;
			
			script.PrintTokens(token_stream);
			
			std::string &&token_string = token_stream.str();
            *tokenString = QString::fromStdString(token_string);
		}
	}
	catch (...)
    {
        std::string &&error_string = Eidos_GetUntrimmedRaiseMessage();
        *errorString = QString::fromStdString(error_string);
        
        // move the error outside of the currentScript context if possible; the ranges
        // should have already been moved by TranslateErrorContextToUserScript()
        if (gEidosErrorContext.currentScript == &script)
        {
#if EIDOS_DEBUG_ERROR_POSITIONS
            std::cout << "-[EidosConsoleWindowController _executeScriptString:...]: clearing gEidosErrorContext.currentScript after error in tokenization." << std::endl;
#endif
            gEidosErrorContext.currentScript = nullptr;
        }
        else if (gEidosErrorContext.currentScript)
        {
            // The error got translated to a script we don't recognize; clear the error info,
            // all we can do is show the error string to the user, with no position
            errorString->insert(0, "A tokenization error occurred in a different script context, so the error position cannot be highlighted in the console:\n");
            
#if EIDOS_DEBUG_ERROR_POSITIONS
            std::cout << "-[EidosConsoleWindowController _executeScriptString:...]: error in tokenization traced to a different script; clearing all error info." << std::endl;
#endif
            ClearErrorContext();
        }
        
        return nullptr;
    }
    
	// Parse, an "interpreter block" bounded by an EOF rather than a "script block" that requires braces
	try
	{
		script.ParseInterpreterBlockToAST(true);
		
		if (parseString)
		{
			std::ostringstream parse_stream;
			
			script.PrintAST(parse_stream);
			
			std::string &&parse_string = parse_stream.str();
			*parseString = QString::fromStdString(parse_string);
		}
	}
	catch (...)
    {
        std::string &&error_string = Eidos_GetUntrimmedRaiseMessage();
        *errorString = QString::fromStdString(error_string);
        
        // move the error outside of the currentScript context if possible; the ranges
        // should have already been moved by TranslateErrorContextToUserScript()
        if (gEidosErrorContext.currentScript == &script)
        {
#if EIDOS_DEBUG_ERROR_POSITIONS
            std::cout << "-[EidosConsoleWindowController _executeScriptString:...]: clearing gEidosErrorContext.currentScript after error in parsing." << std::endl;
#endif
            gEidosErrorContext.currentScript = nullptr;
        }
        else if (gEidosErrorContext.currentScript)
        {
            // The error got translated to a script we don't recognize; clear the error info,
            // all we can do is show the error string to the user, with no position
            errorString->insert(0, "A parsing error occurred in a different script context, so the error position cannot be highlighted in the console:\n");
            
#if EIDOS_DEBUG_ERROR_POSITIONS
            std::cout << "-[EidosConsoleWindowController _executeScriptString:...]: error in parsing traced to a different script; clearing all error info." << std::endl;
#endif
            ClearErrorContext();
        }
        
        return nullptr;
    }
    
	// Get a symbol table and let SLiM add symbols to it
	if (!global_symbols)
	{
		global_symbols = gEidosConstantsSymbolTable;
		
        // in SLiMgui this comes from the delegate method eidosConsoleWindowController:symbolsFromBaseSymbols:
        if (parentSLiMWindow->community && !parentSLiMWindow->invalidSimulation())
            global_symbols = parentSLiMWindow->community->SymbolsFromBaseSymbols(global_symbols);
		
        // With the advant of global versus local symbol tables, the semantics here have gotten a little tricky.  In EidosScribe
		// we want the console to work in the global variables table directly, which we need to create; that will be our symbol
		// table.  In SLiM, we want the console to work in a local variables table; SLiM has its own global variables table,
		// which we don't want to clutter up, just as if were were in a callback.  So here, we now check whether a global variables
		// table is already in the chain, and if so, we create a local variables table; otherwise we create a global variables table.
		bool global_variables_table_exists = false;
		EidosSymbolTable *scan_table = global_symbols;
		
		while (scan_table)
		{
			if (scan_table->TableType() == EidosSymbolTableType::kGlobalVariablesTable)
			{
				global_variables_table_exists = true;
				break;
			}
			
			scan_table = scan_table->ChainSymbolTable();
		}
		
		EidosSymbolTableType console_table_type = global_variables_table_exists ? EidosSymbolTableType::kLocalVariablesTable : EidosSymbolTableType::kGlobalVariablesTable;
		
		global_symbols = new EidosSymbolTable(console_table_type, global_symbols);	// add a table for script-defined variables on top
	}
	
	// Get a function map from SLiM, or make one ourselves
	if (!global_function_map)
	{
        global_function_map_owned = false;
		
        if (parentSLiMWindow->community && !parentSLiMWindow->invalidSimulation())
			global_function_map = &parentSLiMWindow->community->FunctionMap();
		
		if (!global_function_map)
		{
			global_function_map = new EidosFunctionMap(*EidosInterpreter::BuiltInFunctionMap());
			global_function_map_owned = true;
		}
	}
	
	// Get the EidosContext, if any, from SLiM
	EidosContext *eidos_context = parentSLiMWindow->community;
	
	// Interpret the parsed block
    parentSLiMWindow->willExecuteScript();
	
    std::ostringstream outstream;	// in the Eidos console, one output stream for both types of output
	EidosInterpreter interpreter(script, *global_symbols, *global_function_map, eidos_context, outstream, outstream);
	
	try
	{
		if (executionString)
			interpreter.SetShouldLogExecution(true);
		
		EidosValue_SP result = interpreter.EvaluateInterpreterBlock(true, true);	// print output, return the last statement value (result not used)
		output = outstream.str();
		
        if (variableBrowser_)
            variableBrowser_->reloadBrowser(true);
		
		if (executionString)
		{
			std::string &&execution_string = interpreter.ExecutionLog();
			*executionString = QString::fromStdString(execution_string);
		}
	}
	catch (...)
    {
        parentSLiMWindow->didExecuteScript();
        
        output = outstream.str();
        
        std::string &&error_string = Eidos_GetUntrimmedRaiseMessage();
        *errorString = QString::fromStdString(error_string);
        
        // move the error outside of the currentScript context if possible; the ranges
        // should have already been moved by TranslateErrorContextToUserScript()
        if (gEidosErrorContext.currentScript == &script)
        {
#if EIDOS_DEBUG_ERROR_POSITIONS
            std::cout << "-[EidosConsoleWindowController _executeScriptString:...]: clearing gEidosErrorContext.currentScript after error in execution." << std::endl;
#endif
            gEidosErrorContext.currentScript = nullptr;
        }
        else if (gEidosErrorContext.currentScript)
        {
            // The error got translated to a script we don't recognize; clear the error info,
            // all we can do is show the error string to the user, with no position
            errorString->insert(0, "An execution error occurred in a different script context, so the error position cannot be highlighted in the console:\n");
            
#if EIDOS_DEBUG_ERROR_POSITIONS
            std::cout << "-[EidosConsoleWindowController _executeScriptString:...]: error in execution traced to a different script; clearing all error info." << std::endl;
#endif
            ClearErrorContext();
        }
        
        return QString::fromStdString(output);
    }
    
	parentSLiMWindow->didExecuteScript();
	
	// See comment on safeguardReferences above
	if (safeguardReferences)
		validateSymbolTableAndFunctionMap();
    
    // Flush buffered output to files after every script execution, so the user sees the results
    // NOTE THAT THE WORKING DIRECTORY HAS BEEN CHANGED BACK AT THIS POINT!
    bool flush_success = Eidos_FlushFiles();
    
    if (!flush_success)
        *errorString = "ERROR (Eidos_FlushFiles): A compressed file buffer failed to write out to disk.  Please check file paths, filesystem writeability and permissions, available disk space, and other possible causes of file I/O problems.\n";
    
    return QString::fromStdString(output);
}

// Execute the given script string, with the terminating semicolon being optional if requested
void QtSLiMEidosConsole::executeScriptString(QString scriptString, bool semicolonOptional)
{
    QString tokenString, parseString, executionString, errorString;
    bool showTokens = false; //[defaults boolForKey:EidosDefaultsShowTokensKey];
	bool showParse = false; //[defaults boolForKey:EidosDefaultsShowParseKey];
	bool showExecution = false; //[defaults boolForKey:EidosDefaultsShowExecutionKey];
    QtSLiMConsoleTextEdit *console = ui->consoleTextEdit;
    
    QString result = _executeScriptString(scriptString,
                                          showTokens ? &tokenString : nullptr,
                                          showParse ? &parseString : nullptr,
                                          showExecution ? &executionString : nullptr,
                                          &errorString,
                                          semicolonOptional);
    
    if (errorString.contains("unexpected token 'EOF'"))
    {
        // The user has entered an incomplete script line, so we use a continuation prompt
        console->showContinuationPrompt();
    }
    else
    {
        console->appendExecution(result, errorString, tokenString, parseString, executionString);
        console->showPrompt();
    }
}


//
//  public slots
//

void QtSLiMEidosConsole::executeAllClicked(void)
{
    QString all = ui->scriptTextEdit->toPlainText();
    
    ui->consoleTextEdit->setCommandAtPrompt(all);
    ui->consoleTextEdit->executeCurrentPrompt();
}

void QtSLiMEidosConsole::executeSelectionClicked(void)
{
    QTextCursor selectionCursor(ui->scriptTextEdit->textCursor());
    
    if (selectionCursor.selectionStart() == selectionCursor.selectionEnd())
    {
        // zero-length selections get extended to encompass the full line
        selectionCursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
        selectionCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    }
    
    QString selection = selectionCursor.selectedText();
    
    ui->consoleTextEdit->setCommandAtPrompt(selection);
    ui->consoleTextEdit->executeCurrentPrompt();
}

void QtSLiMEidosConsole::executePromptScript(QString executionString)
{
    executeScriptString(executionString, true);
}

































