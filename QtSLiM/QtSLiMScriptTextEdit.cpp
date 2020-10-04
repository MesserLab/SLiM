//
//  QtSLiMScriptTextEdit.cpp
//  SLiM
//
//  Created by Ben Haller on 11/24/2019.
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


#include "QtSLiMScriptTextEdit.h"

#include <QApplication>
#include <QGuiApplication>
#include <QTextCursor>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QStyle>
#include <QAbstractTextDocumentLayout>
#include <QMessageBox>
#include <QSettings>
#include <QCheckBox>
#include <QMainWindow>
#include <QStatusBar>
#include <QCompleter>
#include <QStringListModel>
#include <QScrollBar>
#include <QTextDocument>
#include <QDebug>

#include "QtSLiMPreferences.h"
#include "QtSLiMEidosPrettyprinter.h"
#include "QtSLiMSyntaxHighlighting.h"
#include "QtSLiMEidosConsole.h"
#include "QtSLiMHelpWindow.h"
#include "QtSLiMAppDelegate.h"
#include "QtSLiMWindow.h"
#include "QtSLiMExtras.h"
#include "QtSLiM_SLiMgui.h"

#include "eidos_script.h"
#include "eidos_token.h"
#include "slim_eidos_block.h"
#include "subpopulation.h"


//
//  QtSLiMTextEdit
//

QtSLiMTextEdit::QtSLiMTextEdit(const QString &text, QWidget *parent) : QTextEdit(text, parent)
{
    selfInit();
}

QtSLiMTextEdit::QtSLiMTextEdit(QWidget *parent) : QTextEdit(parent)
{
    selfInit();
}

void QtSLiMTextEdit::selfInit(void)
{
    // track changes to undo/redo availability
    connect(this, &QTextEdit::undoAvailable, this, [this](bool b) { undoAvailable_ = b; });
    connect(this, &QTextEdit::redoAvailable, this, [this](bool b) { redoAvailable_ = b; });
    connect(this, &QTextEdit::copyAvailable, this, [this](bool b) { copyAvailable_ = b; });
    
    // clear the custom error background color whenever the selection changes
    connect(this, &QTextEdit::selectionChanged, this, [this]() { setPalette(style()->standardPalette()); });
    connect(this, &QTextEdit::cursorPositionChanged, this, [this]() { setPalette(style()->standardPalette()); });
    
    // clear the status bar on a selection change
    connect(this, &QTextEdit::selectionChanged, this, &QtSLiMTextEdit::updateStatusFieldFromSelection);
    connect(this, &QTextEdit::cursorPositionChanged, this, &QtSLiMTextEdit::updateStatusFieldFromSelection);
    
    // Wire up to change the font when the display font pref changes
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::displayFontPrefChanged, this, &QtSLiMTextEdit::displayFontPrefChanged);
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::scriptSyntaxHighlightPrefChanged, this, &QtSLiMTextEdit::scriptSyntaxHighlightPrefChanged);
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::outputSyntaxHighlightPrefChanged, this, &QtSLiMTextEdit::outputSyntaxHighlightPrefChanged);
    
    // Get notified of modifier key changes, so we can change our cursor
    connect(qtSLiMAppDelegate, &QtSLiMAppDelegate::modifiersChanged, this, &QtSLiMTextEdit::modifiersChanged);
    
    // set up the script and output textedits
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    int tabWidth = 0;
    QFont scriptFont = prefs.displayFontPref(&tabWidth);
    
    setFont(scriptFont);
    setTabStopWidth(tabWidth);    // deprecated in 5.10; should use setTabStopDistance(), which requires Qt 5.10; see https://stackoverflow.com/a/54605709/2752221
}

QtSLiMTextEdit::~QtSLiMTextEdit()
{
}

void QtSLiMTextEdit::setScriptType(ScriptType type)
{
    // Configure our script type; this should be called once, early
    scriptType = type;
}

void QtSLiMTextEdit::setSyntaxHighlightType(ScriptHighlightingType type)
{
    // Configure our syntax highlighting; this should be called once, early
    syntaxHighlightingType = type;
    
    scriptSyntaxHighlightPrefChanged();     // create a highlighter if needed
    outputSyntaxHighlightPrefChanged();     // create a highlighter if needed
}

void QtSLiMTextEdit::setOptionClickEnabled(bool enabled)
{
    optionClickEnabled = enabled;
    optionClickIntercepted = false;
}

void QtSLiMTextEdit::displayFontPrefChanged()
{
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    int tabWidth = 0;
    QFont displayFont = prefs.displayFontPref(&tabWidth);
    
    setFont(displayFont);
    setTabStopWidth(tabWidth);      // deprecated in 5.10
}

void QtSLiMTextEdit::scriptSyntaxHighlightPrefChanged()
{
    if (syntaxHighlightingType == QtSLiMTextEdit::ScriptHighlighting)
    {
        QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
        bool highlightPref = prefs.scriptSyntaxHighlightPref();
        
        if (highlightPref && !scriptHighlighter)
        {
            scriptHighlighter = new QtSLiMScriptHighlighter(document());
        }
        else if (!highlightPref && scriptHighlighter)
        {
            scriptHighlighter->setDocument(nullptr);
            scriptHighlighter->setParent(nullptr);
            delete scriptHighlighter;
            scriptHighlighter = nullptr;
        }
    }
}

void QtSLiMTextEdit::outputSyntaxHighlightPrefChanged()
{
    if (syntaxHighlightingType == QtSLiMTextEdit::OutputHighlighting)
    {
        QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
        bool highlightPref = prefs.outputSyntaxHighlightPref();
        
        if (highlightPref && !outputHighlighter)
        {
            outputHighlighter = new QtSLiMOutputHighlighter(document());
        }
        else if (!highlightPref && outputHighlighter)
        {
            outputHighlighter->setDocument(nullptr);
            outputHighlighter->setParent(nullptr);
            delete outputHighlighter;
            outputHighlighter = nullptr;
        }
    }
}

void QtSLiMTextEdit::highlightError(int startPosition, int endPosition)
{
    QTextCursor cursor(document());
    
    cursor.setPosition(startPosition);
    cursor.setPosition(endPosition, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
    
    QPalette p = palette();
    p.setColor(QPalette::Highlight, QColor(QColor(Qt::red).lighter(120)));
    p.setColor(QPalette::HighlightedText, QColor(Qt::black));
    setPalette(p);
    
    // note that this custom selection color is cleared by a connection to QTextEdit::selectionChanged()
}

void QtSLiMTextEdit::selectErrorRange(void)
{
	// If there is error-tracking information set, and the error is not attributed to a runtime script
	// such as a lambda or a callback, then we can highlight the error range
	if (!gEidosExecutingRuntimeScript && (gEidosCharacterStartOfErrorUTF16 >= 0) && (gEidosCharacterEndOfErrorUTF16 >= gEidosCharacterStartOfErrorUTF16))
        highlightError(gEidosCharacterStartOfErrorUTF16, gEidosCharacterEndOfErrorUTF16 + 1);
	
	// In any case, since we are the ultimate consumer of the error information, we should clear out
	// the error state to avoid misattribution of future errors
	gEidosCharacterStartOfError = -1;
	gEidosCharacterEndOfError = -1;
	gEidosCharacterStartOfErrorUTF16 = -1;
	gEidosCharacterEndOfErrorUTF16 = -1;
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

QStatusBar *QtSLiMTextEdit::statusBarForWindow(void)
{
    // This is a bit of a hack because the console window is not a MainWindow subclass, and makes its own status bar
    QWidget *ourWindow = window();
    QMainWindow *mainWindow = dynamic_cast<QMainWindow *>(ourWindow);
    QtSLiMEidosConsole *consoleWindow = dynamic_cast<QtSLiMEidosConsole *>(ourWindow);
    QStatusBar *statusBar = nullptr;
    
    if (mainWindow)
        statusBar = mainWindow->statusBar();
    else if (consoleWindow)
        statusBar = consoleWindow->statusBar();
    
    return statusBar;
}

QtSLiMWindow *QtSLiMTextEdit::slimControllerForWindow(void)
{
    QtSLiMWindow *windowSLiMController = dynamic_cast<QtSLiMWindow *>(window());
    
    if (windowSLiMController)
        return windowSLiMController;
    
    QtSLiMEidosConsole *windowEidosConsole = dynamic_cast<QtSLiMEidosConsole *>(window());
    
    if (windowEidosConsole)
        return windowEidosConsole->parentSLiMWindow;
    
    return nullptr;
}

QtSLiMEidosConsole *QtSLiMTextEdit::slimEidosConsoleForWindow(void)
{
    QtSLiMEidosConsole *windowEidosConsole = dynamic_cast<QtSLiMEidosConsole *>(window());
    
    if (windowEidosConsole)
        return windowEidosConsole;
    
    return nullptr;
}

bool QtSLiMTextEdit::checkScriptSuppressSuccessResponse(bool suppressSuccessResponse)
{
	// Note this does *not* check out scriptString, which represents the state of the script when the SLiMSim object was created
	// Instead, it checks the current script in the script TextView – which is not used for anything until the recycle button is clicked.
	QString currentScriptString = toPlainText();
    QByteArray utf8bytes = currentScriptString.toUtf8();
	const char *cstr = utf8bytes.constData();
	std::string errorDiagnostic;
	
	if (!cstr)
	{
		errorDiagnostic = "The script string could not be read, possibly due to an encoding problem.";
	}
	else
	{
        if (scriptType == EidosScriptType)
        {
            EidosScript script(cstr);
            
            try {
                script.Tokenize();
                script.ParseInterpreterBlockToAST(true);
            }
            catch (...)
            {
                errorDiagnostic = Eidos_GetTrimmedRaiseMessage();
            }
        }
        else if (scriptType == SLiMScriptType)
        {
            SLiMEidosScript script(cstr);
            
            try {
                script.Tokenize();
                script.ParseSLiMFileToAST();
            }
            catch (...)
            {
                errorDiagnostic = Eidos_GetTrimmedRaiseMessage();
            }
        }
        else
        {
            qDebug() << "checkScriptSuppressSuccessResponse() called with no script type set";
        }
	}
	
	bool checkDidSucceed = !(errorDiagnostic.length());
	
	if (!checkDidSucceed || !suppressSuccessResponse)
	{
		if (!checkDidSucceed)
		{
			// On failure, we show an alert describing the error, and highlight the relevant script line
            qApp->beep();
            selectErrorRange();
            
            QString q_errorDiagnostic = QString::fromStdString(errorDiagnostic);
            QMessageBox messageBox(this);
            messageBox.setText("Script error");
            messageBox.setInformativeText(q_errorDiagnostic);
            messageBox.setIcon(QMessageBox::Warning);
            messageBox.setWindowModality(Qt::WindowModal);
            messageBox.setFixedWidth(700);      // seems to be ignored
            messageBox.exec();
            
			// Show the error in the status bar also
            QStatusBar *statusBar = statusBarForWindow();
            
            if (statusBar)
                statusBar->showMessage("<font color='#cc0000' style='font-size: 11px;'>" + q_errorDiagnostic.trimmed().toHtmlEscaped() + "</font>");
		}
		else
		{
            QSettings settings;
            
            if (!settings.value("QtSLiMSuppressScriptCheckSuccessPanel", false).toBool())
			{
                // In SLiMgui we play a "success" sound too, but doing anything besides beeping is apparently difficult with Qt...
                
                QMessageBox messageBox(this);
                messageBox.setText("No script errors");
                messageBox.setInformativeText("No errors found.");
                messageBox.setIcon(QMessageBox::Information);
                messageBox.setWindowModality(Qt::WindowModal);
                messageBox.setCheckBox(new QCheckBox("Do not show this message again", nullptr));
                messageBox.exec();
                
                if (messageBox.checkBox()->isChecked())
                    settings.setValue("QtSLiMSuppressScriptCheckSuccessPanel", true);
            }
		}
	}
	
	return checkDidSucceed;
}

void QtSLiMTextEdit::checkScript(void)
{
    checkScriptSuppressSuccessResponse(false);
}

void QtSLiMTextEdit::_prettyprint_reformat(bool reformat)
{
    if (isEnabled())
	{
		if (checkScriptSuppressSuccessResponse(true))
		{
			// We know the script is syntactically correct, so we can tokenize and parse it without worries
            QString currentScriptString = toPlainText();
            QByteArray utf8bytes = currentScriptString.toUtf8();
            const char *cstr = utf8bytes.constData();
			EidosScript script(cstr);
            
			script.Tokenize(false, true);	// get whitespace and comment tokens
			
			// Then generate a new script string that is prettyprinted
			const std::vector<EidosToken> &tokens = script.Tokens();
			std::string pretty;
			bool success = false;
            
            if (reformat)
                success = Eidos_reformatTokensFromScript(tokens, script, pretty);
            else
                success = Eidos_prettyprintTokensFromScript(tokens, script, pretty);
            
            if (success)
            {
                // We want to replace our text in a way that is undoable; to do this, we use the text cursor
                QString replacementString = QString::fromStdString(pretty);
                
                QTextCursor &&cursor = textCursor();
                
                cursor.beginEditBlock();
                cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
                cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                cursor.insertText(replacementString);
                cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
                cursor.endEditBlock();
                setTextCursor(cursor);
            }
            else
                qApp->beep();
		}
	}
	else
	{
        qApp->beep();
	}
}

void QtSLiMTextEdit::prettyprint(void)
{
    _prettyprint_reformat(false);
}

void QtSLiMTextEdit::reformat(void)
{
    _prettyprint_reformat(true);
}

void QtSLiMTextEdit::prettyprintClicked(void)
{
    // Get the option-key state; if it is pressed, we do a full reformat
    bool optionPressed = QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier);
    
    if (optionPressed)
        reformat();
    else
        prettyprint();
}

void QtSLiMTextEdit::scriptHelpOptionClick(QString searchString)
{
    QtSLiMHelpWindow &helpWindow = QtSLiMHelpWindow::instance();
    
    // A few Eidos substitutions to improve the search
    if (searchString == ":")                    searchString = "operator :";
    else if (searchString == "(")               searchString = "operator ()";
    else if (searchString == ")")               searchString = "operator ()";
    else if (searchString == ",")               searchString = "calls: operator ()";
    else if (searchString == "[")               searchString = "operator []";
    else if (searchString == "]")               searchString = "operator []";
    else if (searchString == "{")               searchString = "compound statements";
    else if (searchString == "}")               searchString = "compound statements";
    else if (searchString == ".")               searchString = "operator .";
    else if (searchString == "=")               searchString = "operator =";
    else if (searchString == "+")               searchString = "Arithmetic operators";
    else if (searchString == "-")               searchString = "Arithmetic operators";
    else if (searchString == "*")               searchString = "Arithmetic operators";
    else if (searchString == "/")               searchString = "Arithmetic operators";
    else if (searchString == "%")               searchString = "Arithmetic operators";
    else if (searchString == "^")               searchString = "Arithmetic operators";
    else if (searchString == "|")               searchString = "Logical operators";
    else if (searchString == "&")               searchString = "Logical operators";
    else if (searchString == "!")               searchString = "Logical operators";
    else if (searchString == "==")              searchString = "Comparative operators";
    else if (searchString == "!=")              searchString = "Comparative operators";
    else if (searchString == "<=")              searchString = "Comparative operators";
    else if (searchString == ">=")              searchString = "Comparative operators";
    else if (searchString == "<")               searchString = "Comparative operators";
    else if (searchString == ">")               searchString = "Comparative operators";
    else if (searchString == "'")               searchString = "type string";
    else if (searchString == "\"")              searchString = "type string";
    else if (searchString == ";")               searchString = "null statements";
    else if (searchString == "//")              searchString = "comments";
    else if (searchString == "if")              searchString = "if and if–else statements";
    else if (searchString == "else")            searchString = "if and if–else statements";
    else if (searchString == "for")             searchString = "for statements";
    else if (searchString == "in")              searchString = "for statements";
    else if (searchString == "function")        searchString = "user-defined functions";
    // and SLiM substitutions; "initialize" is deliberately omitted here so that the initialize...() methods also come up
    else if (searchString == "early")			searchString = "Eidos events";
	else if (searchString == "late")			searchString = "Eidos events";
	else if (searchString == "fitness")         searchString = "fitness() callbacks";
	else if (searchString == "interaction")     searchString = "interaction() callbacks";
	else if (searchString == "mateChoice")      searchString = "mateChoice() callbacks";
	else if (searchString == "modifyChild")     searchString = "modifyChild() callbacks";
	else if (searchString == "recombination")	searchString = "recombination() callbacks";
	else if (searchString == "mutation")		searchString = "mutation() callbacks";
	else if (searchString == "reproduction")	searchString = "reproduction() callbacks";
    
    // now send the search string on to the help window
    helpWindow.enterSearchForString(searchString, true);
}

void QtSLiMTextEdit::mousePressEvent(QMouseEvent *event)
{
    bool optionPressed = (optionClickEnabled && QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier));
    
    if (optionPressed)
    {
        // option-click gets intercepted to bring up help
        optionClickIntercepted = true;
        
        // get the position of the character clicked on; note that this is different from
        // QTextEdit::cursorForPosition(), which returns the closest cursor position
        // *between* characters, not which character was actually clicked on; see
        // https://www.qtcentre.org/threads/45645-QTextEdit-cursorForPosition()-and-character-at-mouse-pointer
        QPointF localPos = event->localPos();
        QPointF documentPos = localPos + QPointF(0, verticalScrollBar()->value());
        int characterPositionClicked = document()->documentLayout()->hitTest(documentPos, Qt::ExactHit);
        
        if (characterPositionClicked == -1)     // occurs if you click between lines of text
            return;
        
        QTextCursor charCursor(document());
        charCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, characterPositionClicked);
        charCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
        
        QString characterString = charCursor.selectedText();
        
        if (characterString.length() != 1)      // not sure if this ever happens, being safe
            return;
        
        QChar character = characterString.at(0);
        
        if (character.isSpace())                // no help on whitespace
            return;
        
        //qDebug() << characterPositionClicked << ": " << charCursor.anchor() << "," << charCursor.position() << "," << charCursor.selectedText();
        
        // if the character is a letter or number, we want to select the word it
        // is contained by and use that as the symbol for lookup; otherwise, 
        // it is symbolic, and we want to try to match the right symbol in the code
        QTextCursor symbolCursor(charCursor);
        
        if (character.isLetterOrNumber())
        {
            symbolCursor.select(QTextCursor::WordUnderCursor);
        }
        else if ((character == '/') || (character == '=') || (character == '<') || (character == '>') || (character == '!'))
        {
            // the character clicked might be part of a multicharacter symbol: // == <= >= !=
            // we will look at two-character groups anchored in the clicked character to test this
            QTextCursor leftPairCursor(document()), rightPairCursor(document());
            leftPairCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, characterPositionClicked - 1);
            leftPairCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 2);
            rightPairCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, characterPositionClicked);
            rightPairCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 2);
            
            QString leftPairString = leftPairCursor.selectedText(), rightPairString = rightPairCursor.selectedText();
            
            if ((leftPairString == "//") || (leftPairString == "==") || (leftPairString == "<=") || (leftPairString == ">=") || (leftPairString == "!="))
                symbolCursor = leftPairCursor;
            else if ((rightPairString == "//") || (rightPairString == "==") || (rightPairString == "<=") || (rightPairString == ">=") || (rightPairString == "!="))
                symbolCursor = rightPairCursor;
            // else we drop through and search for the one-character symbol
        }
        else
        {
            // the character clicked is a one-character symbol; we just drop through
        }
        
        // select the symbol and trigger a lookup
        QString symbol = symbolCursor.selectedText();
        
        if (symbol.length())
        {
            setTextCursor(symbolCursor);
            scriptHelpOptionClick(symbol);
        }
    }
    else
    {
        // all other cases go to super
        optionClickIntercepted = false;
        
        QTextEdit::mousePressEvent(event);
    }
}

void QtSLiMTextEdit::mouseMoveEvent(QMouseEvent *event)
{
    // forward to super, as long as we did not intercept this mouse event
    if (!optionClickIntercepted)
        QTextEdit::mouseMoveEvent(event);
}

void QtSLiMTextEdit::mouseReleaseEvent(QMouseEvent *event)
{
    // forward to super, as long as we did not intercept this mouse event
    if (!optionClickIntercepted)
        QTextEdit::mouseReleaseEvent(event);
    
    optionClickIntercepted = false;
}

void QtSLiMTextEdit::fixMouseCursor(void)
{
    if (optionClickEnabled)
    {
        // we want a pointing hand cursor when option is pressed; if the cursor is wrong, fix it
        // note the cursor for QTextEdit is apparently controlled by its viewport
        bool optionPressed = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier);
        QWidget *vp = viewport();
        
        if (optionPressed && (vp->cursor().shape() != Qt::PointingHandCursor))
            vp->setCursor(Qt::PointingHandCursor);
        else if (!optionPressed && (vp->cursor().shape() != Qt::IBeamCursor))
            vp->setCursor(Qt::IBeamCursor);
    }
}

void QtSLiMTextEdit::enterEvent(QEvent *event)
{
    // forward to super
    QTextEdit::enterEvent(event);
    
    // modifiersChanged() generally keeps our cursor correct, but we do it on enterEvent
    // as well just as a fallback; for example, if the mouse is inside us on launch and
    // the modifier is already down, enterEvent() will fix out initial cursor
    fixMouseCursor();
}

void QtSLiMTextEdit::modifiersChanged(Qt::KeyboardModifiers __attribute__((unused)) newModifiers)
{
    // keyPressEvent() and keyReleaseEvent() are sent to us only when we have the focus, but
    // we want to change our cursor even when we don't have focus, so we use an event filter
    fixMouseCursor();
}

EidosFunctionSignature_CSP QtSLiMTextEdit::signatureForFunctionName(QString callName, EidosFunctionMap *functionMapPtr)
{
	std::string call_name = callName.toStdString();
	
	// Look for a matching function signature for the call name.
	for (const auto& function_iter : *functionMapPtr)
	{
		const EidosFunctionSignature_CSP &sig = function_iter.second;
		const std::string &sig_call_name = sig->call_name_;
		
		if (sig_call_name.compare(call_name) == 0)
			return sig;
	}
	
	return nullptr;
}

const std::vector<EidosMethodSignature_CSP> *QtSLiMTextEdit::slimguiAllMethodSignatures(void)
{
	// This adds the methods belonging to the SLiMgui class to those returned by SLiMSim (which does not know about SLiMgui)
	static std::vector<EidosMethodSignature_CSP> *methodSignatures = nullptr;
	
	if (!methodSignatures)
	{
		const std::vector<EidosMethodSignature_CSP> *slimMethods =					SLiMSim::AllMethodSignatures();
		const std::vector<EidosMethodSignature_CSP> *methodsSLiMgui =				gSLiM_SLiMgui_Class->Methods();
		
		methodSignatures = new std::vector<EidosMethodSignature_CSP>(*slimMethods);
		
		methodSignatures->insert(methodSignatures->end(), methodsSLiMgui->begin(), methodsSLiMgui->end());
		
		// *** From here downward this is taken verbatim from SLiMSim::AllMethodSignatures()
		// FIXME should be split into a separate method
		
		// sort by pointer; we want pointer-identical signatures to end up adjacent
		std::sort(methodSignatures->begin(), methodSignatures->end());
		
		// then unique by pointer value to get a list of unique signatures (which may not be unique by name)
		auto unique_end_iter = std::unique(methodSignatures->begin(), methodSignatures->end());
		methodSignatures->resize(static_cast<size_t>(std::distance(methodSignatures->begin(), unique_end_iter)));
		
		// print out any signatures that are identical by name
		std::sort(methodSignatures->begin(), methodSignatures->end(), CompareEidosCallSignatures);
		
		EidosMethodSignature_CSP previous_sig = nullptr;
		
		for (EidosMethodSignature_CSP &sig : *methodSignatures)
		{
			if (previous_sig && (sig->call_name_.compare(previous_sig->call_name_) == 0))
			{
				// We have a name collision.  That is OK as long as the method signatures are identical.
                const EidosMethodSignature *sig1 = sig.get();
                const EidosMethodSignature *sig2 = previous_sig.get();
                
				if ((typeid(*sig1) != typeid(*sig2)) ||
					(sig->is_class_method != previous_sig->is_class_method) ||
					(sig->call_name_ != previous_sig->call_name_) ||
					(sig->return_mask_ != previous_sig->return_mask_) ||
					(sig->return_class_ != previous_sig->return_class_) ||
					(sig->arg_masks_ != previous_sig->arg_masks_) ||
					(sig->arg_names_ != previous_sig->arg_names_) ||
					(sig->arg_classes_ != previous_sig->arg_classes_) ||
					(sig->has_optional_args_ != previous_sig->has_optional_args_) ||
					(sig->has_ellipsis_ != previous_sig->has_ellipsis_))
				std::cout << "Duplicate method name with a different signature: " << sig->call_name_ << std::endl;
			}
			
			previous_sig = sig;
		}
	}
	
	return methodSignatures;
}

EidosMethodSignature_CSP QtSLiMTextEdit::signatureForMethodName(QString callName)
{
	std::string call_name = callName.toStdString();
	
	// Look for a method in the global method registry last; for this to work, the Context must register all methods with Eidos.
	// This case is much simpler than the function case, because the user can't declare their own methods.
	const std::vector<EidosMethodSignature_CSP> *methodSignatures = nullptr;
	
    methodSignatures = slimguiAllMethodSignatures();
    
	if (!methodSignatures)
		methodSignatures = gEidos_UndefinedClassObject->Methods();
	
	for (const EidosMethodSignature_CSP &sig : *methodSignatures)
	{
		const std::string &sig_call_name = sig->call_name_;
		
		if (sig_call_name.compare(call_name) == 0)
			return sig;
	}
	
	return nullptr;
}

//- (EidosFunctionMap *)functionMapForScriptString:(NSString *)scriptString includingOptionalFunctions:(BOOL)includingOptionalFunctions
EidosFunctionMap *QtSLiMTextEdit::functionMapForScriptString(QString scriptString, bool includingOptionalFunctions)
{
	// This returns a function map (owned by the caller) that reflects the best guess we can make, incorporating
	// any functions known to our delegate, as well as all functions we can scrape from the script string.
	std::string script_string = scriptString.toStdString();
	EidosScript script(script_string);
	
	// Tokenize
	script.Tokenize(true, false);	// make bad tokens as needed, don't keep nonsignificant tokens
	
	return functionMapForTokenizedScript(script, includingOptionalFunctions);
}

EidosFunctionMap *QtSLiMTextEdit::functionMapForTokenizedScript(EidosScript &script, bool includingOptionalFunctions)
{
    // This lower-level function takes a tokenized script object and works from there, allowing reuse of work
    // in the case of attributedSignatureForScriptString:...
    QtSLiMWindow *windowSLiMController = slimControllerForWindow();
    SLiMSim *sim = (windowSLiMController ? windowSLiMController->sim : nullptr);
    bool invalidSimulation = (windowSLiMController ? windowSLiMController->invalidSimulation() : true);
    
    // start with all the functions that are available in the current simulation context
    EidosFunctionMap *functionMapPtr = nullptr;
    
    if (sim && !invalidSimulation)
        functionMapPtr = new EidosFunctionMap(sim->FunctionMap());
    else
        functionMapPtr = new EidosFunctionMap(*EidosInterpreter::BuiltInFunctionMap());
    
    // functionMapForEidosTextView: returns the function map for the current interpreter state, and the type-interpreter
    // stuff we do below gives the delegate no chance to intervene (note that SLiMTypeInterpreter does not get in here,
    // unlike in the code completion machinery!).  But sometimes we want SLiM's zero-gen functions to be added to the map
    // in all cases; it would be even better to be smart the way code completion is, but that's more work than it's worth.
    if (includingOptionalFunctions)
    {
        // add SLiM functions that are context-dependent
        SLiMSim::AddZeroGenerationFunctionsToMap(*functionMapPtr);
        SLiMSim::AddSLiMFunctionsToMap(*functionMapPtr);
    }
    
    // OK, now we have a starting point.  We now want to use the type-interpreter to add any functions that are declared
    // in the full script, so that such declarations are known to us even before they have actually been executed.
    EidosTypeTable typeTable;
    EidosCallTypeTable callTypeTable;
    EidosSymbolTable *symbols = gEidosConstantsSymbolTable;
    
    symbols = symbolsFromBaseSymbols(symbols);
    
    if (symbols)
        symbols->AddSymbolsToTypeTable(&typeTable);
    
    script.ParseInterpreterBlockToAST(true, true);	// make bad nodes as needed (i.e. never raise, and produce a correct tree)
    
    EidosTypeInterpreter typeInterpreter(script, typeTable, *functionMapPtr, callTypeTable);
    
    typeInterpreter.TypeEvaluateInterpreterBlock();	// result not used
    
    return functionMapPtr;
}

EidosSymbolTable *QtSLiMTextEdit::symbolsFromBaseSymbols(EidosSymbolTable *baseSymbols)
{
    // in SLiMgui this is a delegate method, eidosTextView:symbolsFromBaseSymbols:
    // the point is simply to substitute in a console symbol table when one is available
    QtSLiMEidosConsole *consoleWindow = slimEidosConsoleForWindow();
    
    if (consoleWindow)
        return consoleWindow->symbolTable();
    
    return baseSymbols;
}

void QtSLiMTextEdit::scriptStringAndSelection(QString &scriptString, int &pos, int &len)
{
    // by default, the entire contents of the textedit are considered "script"
    scriptString = toPlainText();
    
    QTextCursor selection(textCursor());
    pos = selection.selectionStart();
    len = selection.selectionEnd() - pos;
}

EidosCallSignature_CSP QtSLiMTextEdit::signatureForScriptSelection(QString &callName)
{
    // Note we return a copy of the signature, owned by the caller
    QString scriptString;
    int selectionStart, selectionEnd;
    
    scriptStringAndSelection(scriptString, selectionStart, selectionEnd);
    
    if (scriptString.length())
	{
		std::string script_string = scriptString.toStdString();
		EidosScript script(script_string);
		
		// Tokenize
		script.Tokenize(true, false);	// make bad tokens as needed, don't keep nonsignificant tokens
		
		const std::vector<EidosToken> &tokens = script.Tokens();
		size_t tokenCount = tokens.size();
		
		// Search forward to find the token position of the start of the selection
		size_t tokenIndex;
		
		for (tokenIndex = 0; tokenIndex < tokenCount; ++tokenIndex)
			if (tokens[tokenIndex].token_UTF16_start_ >= selectionStart)
				break;
		
		// tokenIndex now has the index of the first token *after* the selection start; it can be equal to tokenCount
		// Now we want to scan backward from there, balancing parentheses and looking for the pattern "identifier("
		int backscanIndex = static_cast<int>(tokenIndex) - 1;
		int parenCount = 0, lowestParenCountSeen = 0;
		
		while (backscanIndex > 0)	// last examined position is 1, since we can't look for an identifier at 0 - 1 == -1
		{
			const EidosToken &token = tokens[static_cast<size_t>(backscanIndex)];
			EidosTokenType tokenType = token.token_type_;
			
			if (tokenType == EidosTokenType::kTokenLParen)
			{
				--parenCount;
				
				if (parenCount < lowestParenCountSeen)
				{
					const EidosToken &previousToken = tokens[static_cast<size_t>(backscanIndex) - 1];
					EidosTokenType previousTokenType = previousToken.token_type_;
					
					if (previousTokenType == EidosTokenType::kTokenIdentifier)
					{
						// OK, we found the pattern "identifier("; extract the name of the function/method
						// We also figure out here whether it is a method call (tokens like ".identifier(") or not
                        callName = QString::fromStdString(previousToken.token_string_);
                        
						if ((backscanIndex > 1) && (tokens[static_cast<size_t>(backscanIndex) - 2].token_type_ == EidosTokenType::kTokenDot))
						{
							// This is a method call, so look up its signature that way
							EidosMethodSignature_CSP callSignature = signatureForMethodName(callName);
                            
                            return std::move(callSignature);    // std::move() here avoids a bug in old compilers, according to a compiler warning...
                        }
						else
						{
							// If this is a function declaration like "function(...)identifier(" then show no signature; it's not a function call
							// Determining this requires a fairly complex backscan, because we also have things like "if (...) identifier(" which
							// are function calls.  This is the price we pay for working at the token level rather than the AST level for this;
							// so it goes.  Note that this backscan is separate from the one done outside this block.  BCH 1 March 2018.
							if ((backscanIndex > 1) && (tokens[static_cast<size_t>(backscanIndex) - 2].token_type_ == EidosTokenType::kTokenRParen))
							{
								// Start a new backscan starting at the right paren preceding the identifier; we need to scan back to the balancing
								// left paren, and then see if the next thing before that is "function" or not.
								int funcCheckIndex = backscanIndex - 2;
								int funcCheckParens = 0;
								
								while (funcCheckIndex >= 0)
								{
									const EidosToken &backscanToken = tokens[static_cast<size_t>(funcCheckIndex)];
									EidosTokenType backscanTokenType = backscanToken.token_type_;
									
									if (backscanTokenType == EidosTokenType::kTokenRParen)
										funcCheckParens++;
									else if (backscanTokenType == EidosTokenType::kTokenLParen)
										funcCheckParens--;
									
									--funcCheckIndex;
									
									if (funcCheckParens == 0)
										break;
								}
								
								if ((funcCheckParens == 0) && (funcCheckIndex >= 0) && (tokens[static_cast<size_t>(funcCheckIndex)].token_type_ == EidosTokenType::kTokenFunction))
									break;
							}
							
							// This is a function call, so look up its signature that way, using our best-guess function map
							EidosFunctionMap *functionMapPtr = functionMapForTokenizedScript(script, true);
                            EidosFunctionSignature_CSP callSignature = signatureForFunctionName(callName, functionMapPtr);
                            
							delete functionMapPtr;              // note that callSignature survives this deletion because of shared_ptr
                            
                            return std::move(callSignature);    // std::move() here avoids a bug in old compilers, according to a compiler warning...
						}
					}
					
					lowestParenCountSeen = parenCount;
				}
			}
			else if (tokenType == EidosTokenType::kTokenRParen)
			{
				++parenCount;
			}
			
			--backscanIndex;
		}
	}
	
	return nullptr;
}

void QtSLiMTextEdit::updateStatusFieldFromSelection(void)
{
    if (scriptType != NoScriptType)
    {
        QString callName;
        EidosCallSignature_CSP signature = signatureForScriptSelection(callName);
        
        if (!signature && (scriptType == SLiMScriptType))
        {
            // Handle SLiM callback signatures
            if (callName == "initialize")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("initialize", nullptr, kEidosValueMaskVOID)));
                signature = callbackSig;
            }
            else if (callName == "early")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("early", nullptr, kEidosValueMaskVOID)));
                signature = callbackSig;
            }
            else if (callName == "late")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("late", nullptr, kEidosValueMaskVOID)));
                signature = callbackSig;
            }
            else if (callName == "fitness")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("fitness", nullptr, kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddObject_SN("mutationType", gSLiM_MutationType_Class)->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
                signature = callbackSig;
            }
            else if (callName == "interaction")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("interaction", nullptr, kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddObject_S("interactionType", gSLiM_InteractionType_Class)->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
                signature = callbackSig;
            }
            else if (callName == "mateChoice")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("mateChoice", nullptr, kEidosValueMaskNULL | kEidosValueMaskFloat | kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
                signature = callbackSig;
            }
            else if (callName == "modifyChild")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("modifyChild", nullptr, kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
                signature = callbackSig;
            }
            else if (callName == "recombination")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("recombination", nullptr, kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
                signature = callbackSig;
            }
            else if (callName == "mutation")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("mutation", nullptr, kEidosValueMaskLogical | kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Mutation_Class))->AddObject_OSN("mutationType", gSLiM_MutationType_Class, gStaticEidosValueNULLInvisible)->AddObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
                signature = callbackSig;
            }
            else if (callName == "reproduction")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("reproduction", nullptr, kEidosValueMaskVOID))->AddObject_OSN("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible)->AddString_OSN("sex", gStaticEidosValueNULLInvisible));
                signature = callbackSig;
            }
        }
        
        QString displayString;
        
        if (signature)
            displayString = QString::fromStdString(signature->SignatureString());
        else if (!signature && callName.length())
            displayString = callName + "() – unrecognized call";
        
        QStatusBar *statusBar = statusBarForWindow();
        
        if (displayString.length())
        {
            // The status bar now supports display of an HTML string, so we use QTextDocument and ColorizeCallSignature() to make one
            QTextDocument td;
            
            td.setPlainText(displayString);
            
            QTextCursor tc(&td);
            tc.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
            tc.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            
#ifdef __APPLE__
            ColorizeCallSignature(signature.get(), 11, tc);
#else
            ColorizeCallSignature(signature.get(), 9, tc);
#endif
            
            statusBar->showMessage(td.toHtml());
        }
        else
        {
            statusBar->clearMessage();
        }
    }
}

// Completion support

void QtSLiMTextEdit::setCodeCompletionEnabled(bool enabled)
{
    codeCompletionEnabled = enabled;
    
    if (codeCompletionEnabled && !completer)
    {
        if (completer)
            QObject::disconnect(completer, nullptr, this, nullptr);
        
        completer = new QCompleter(this);
        
        // Make a dummy model for construction
        QStringList words;
        words << "foo";
        words << "bar";
        words << "baz";
        
        completer->setModel(new QStringListModel(words, completer));
        completer->setModelSorting(QCompleter::UnsortedModel);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setWrapAround(false);
        completer->setWidget(this);
        
        connect(completer, QOverload<const QString &>::of(&QCompleter::activated), this, &QtSLiMTextEdit::insertCompletion);
    }
}

void QtSLiMTextEdit::insertCompletion(const QString& completionOriginal)
{
    if (completer->widget() != this)
        return;
    
    // If the completion string ends in ") {}" we add newlines to it here; we don't want to show multi-line completions
    // in the popup, but we want to produce them for the user when the completion is accepted; see slimSpecificCompletion()
    QString completion = completionOriginal;
    bool multilineCompletion = false;
    
    if (completion.endsWith(") { }"))
    {
        completion.replace(") { }", ") {\n\t\n}\n");
        multilineCompletion = true;
    }
    
    // The cursor that we used as a completion root gets replaced completely by the completion string
    NSRange completionRange = rangeForUserCompletion();
    
    if (completionRange.location != NSNotFound)
    {
        QTextCursor tc = textCursor();
        int endPosition = std::max(tc.selectionEnd(), completionRange.location + completionRange.length);   // the completion is off the selection start, but we want to replace any selected text also
        
        tc.setPosition(completionRange.location, QTextCursor::MoveAnchor);
        tc.setPosition(endPosition, QTextCursor::KeepAnchor);
        
        tc.insertText(completion);
        
        // If the completion is multiline, put the insertion point inside the braces of the completion
        if (multilineCompletion)
            tc.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 3);
        
        setTextCursor(tc);
    }
    else
    {
        qApp->beep();
    }
}

void QtSLiMTextEdit::autoindentAfterNewline(void)
{
    // We are called by QtSLiMTextEdit::keyPressEvent() immediately after it calls
    // super to insert a newline in response to Key_Enter or Key_Return
    if (scriptType != NoScriptType)
    {
        QTextCursor tc = textCursor();
        int selStart = tc.selectionStart(), selEnd = tc.selectionEnd();
        const QString scriptString = toPlainText();
        
        // verify that we have an insertion point immediately following a newline
        if ((selStart == selEnd) && (selStart > 0) && (scriptString[selStart - 1] == '\n'))
        {
            QTextCursor previousLine = tc;
            previousLine.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor);
            previousLine.movePosition(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
            
            QString lineString = previousLine.selectedText();
            QString whitespace;
            
            for (int pos = 0; pos < lineString.length(); ++pos)
            {
                QChar qch = lineString[pos];
                
                if (qch.isSpace())
                    whitespace.append(qch);
                else
                    break;
            }
            
            // insert the same whitespace as the previous line had, joining the undo block for the newline insertion
            if (whitespace.length())
            {
                tc.joinPreviousEditBlock();
                tc.insertText(whitespace);
                tc.endEditBlock();
            }
        }
    }
}

void QtSLiMTextEdit::keyPressEvent(QKeyEvent *event)
{
    // Without a completer, we just call super
    if (!completer)
    {
        QTextEdit::keyPressEvent(event);
        return;
    }
    
    if (completer->popup()->isVisible()) {
        // The following keys are forwarded by the completer to the widget
       switch (event->key()) {
       case Qt::Key_Enter:
       case Qt::Key_Return:
       case Qt::Key_Escape:
       case Qt::Key_Tab:
       case Qt::Key_Backtab:
            event->ignore();
            return; // let the completer do default behavior
       default:
           break;
       }
    }
    
    // if we have a visible completer popup, the key pressed is not one of the special keys above (including escape)
    // our completion key shortcut is the escape key, so check for that now
    bool isShortcut = ((event->modifiers() == Qt::NoModifier) && event->key() == Qt::Key_Escape); // escape
    
    if (!isShortcut)
    {
        // any key other than escape and the special keys above causes the completion popup to hide
        completer->popup()->hide();
        QTextEdit::keyPressEvent(event);
        
        // implement autoindent
        if ((event->modifiers() == Qt::NoModifier) && ((event->key() == Qt::Key_Enter) || (event->key() == Qt::Key_Return)))
            autoindentAfterNewline();
        
        return;
    }
    
    // we have a completer and the shortcut has been pressed; initiate completion
    
    // first, figure out the range of text we are completing (the "root")
    NSRange completionRange = rangeForUserCompletion();
    
    if (completionRange.location != NSNotFound)
    {
        QTextCursor completionRootCursor = textCursor();
        completionRootCursor.setPosition(completionRange.location, QTextCursor::MoveAnchor);
        //completionRootCursor.setPosition(completionRange.location + completionRange.length, QTextCursor::KeepAnchor); // this aligns the popup with the right edge of the selection, but we want the left edge, I think...
        
        // get the correct context-sensitive word list for the completer
        QStringList completions = completionsForPartialWordRange(completionRange, nullptr);
        
        completer->setModel(new QStringListModel(completions, completer));
        
        // place the completer appropriately for the cursor; the doc is a bit vague, but this seems to work
        QRect cr = cursorRect(completionRootCursor);
        
        cr.setWidth(completer->popup()->sizeHintForColumn(0)
                    + completer->popup()->verticalScrollBar()->sizeHint().width());
        
        // zero out the completer's completion root; we do not use it, because we implement our own matching algorithm
        completer->setCompletionPrefix("");
        completer->popup()->setCurrentIndex(completer->completionModel()->index(0, 0));
        
        completer->complete(cr); // pop up below the current cursor rect in the textview
    }
    else
    {
        qApp->beep();
    }
    
}

// the rest here is completion code adapted from EidosScribe and SLiMgui
// we mirror the NSTextView APIs completionsForPartialWordRange:indexOfSelectedItem: and
// rangeForUserCompletion to allow our ported code to function identically to SLiMgui

// - (NSArray *)completionsForPartialWordRange:(NSRange)charRange indexOfSelectedItem:(NSInteger *)index
QStringList QtSLiMTextEdit::completionsForPartialWordRange(NSRange __attribute__((__unused__)) charRange, int * __attribute__((__unused__)) indexOfSelectedItem)
{
	QStringList completions;        //NSArray *completions = nil;
	
	_completionHandlerWithRangeForCompletion(nullptr, &completions);
    
	return completions;
}

// - (NSRange)rangeForUserCompletion
NSRange QtSLiMTextEdit::rangeForUserCompletion(void)
{
    NSRange baseRange = {NSNotFound, 0};
    
	_completionHandlerWithRangeForCompletion(&baseRange, nullptr);
    
	return baseRange;
}

//- (NSMutableArray *)globalCompletionsWithTypes:(EidosTypeTable *)typeTable functions:(EidosFunctionMap *)functionMap keywords:(NSArray *)keywords argumentNames:(NSArray *)argumentNames
QStringList QtSLiMTextEdit::globalCompletionsWithTypesFunctionsKeywordsArguments(EidosTypeTable *typeTable, EidosFunctionMap *functionMap, QStringList keywords, QStringList argumentNames)
{
	QStringList globals;
	
	// First add entries for symbols in our type table (from Eidos constants, defined symbols, or our delegate)
	if (typeTable)
	{
		std::vector<std::string> typedSymbols = typeTable->AllSymbols();
		
		for (std::string &symbol_name : typedSymbols)
            globals << QString::fromStdString(symbol_name);
	}
	
	// Sort the symbols, who knows what order they come from EidosTypeTable in...
    globals.sort();
	
	// Next, if we have argument names that are completion matches, we want them at the top
	if (argumentNames.size())
	{
        QStringList oldGlobals = globals;
        
        globals = argumentNames;
        globals.append(oldGlobals);
	}
	
	// Next, a sorted list of functions, with () appended
	if (functionMap)
	{
		for (const auto& function_iter : *functionMap)
		{
			const EidosFunctionSignature *sig = function_iter.second.get();
			QString functionName = QString::fromStdString(sig->call_name_);
			
			// Exclude internal functions such as _Test()
            if (!functionName.startsWith("_"))
            {
                functionName.append("()");
                globals.append(functionName);
			}
		}
	}
	
	// Finally, provide language keywords as an option if requested
	if (keywords.size())
		globals.append(keywords);
	
	return globals;
}

//- (NSMutableArray *)completionsForKeyPathEndingInTokenIndex:(int)lastDotTokenIndex ofTokenStream:(const std::vector<EidosToken> &)tokens withTypes:(EidosTypeTable *)typeTable functions:(EidosFunctionMap *)functionMap callTypes:(EidosCallTypeTable *)callTypeTable keywords:(NSArray *)keywords
QStringList QtSLiMTextEdit::completionsForKeyPathEndingInTokenIndexOfTokenStream(int lastDotTokenIndex, const std::vector<EidosToken> &tokens, EidosTypeTable *typeTable, EidosFunctionMap *functionMap, EidosCallTypeTable *callTypeTable, QStringList __attribute__((__unused__)) keywords)
{
	const EidosToken *token = &tokens[static_cast<size_t>(lastDotTokenIndex)];
	EidosTokenType token_type = token->token_type_;
	
	if (token_type != EidosTokenType::kTokenDot)
	{
		qDebug() << "***** completionsForKeyPathEndingInTokenIndex... called for non-kTokenDot token!";
		return QStringList();
	}
	
	// OK, we've got a key path ending in a dot, and we want to return a list of completions that would work for that key path.
	// We'll trace backward, adding identifiers to a vector to build up the chain of references.  If we hit a bracket, we'll
	// skip back over everything inside it, since subsetting does not change the type; we just need to balance brackets.  If we
	// hit a parenthesis, we do similarly.  If we hit other things – a semicolon, a comma, a brace – that terminates the key path chain.
	std::vector<std::string> identifiers;
	std::vector<bool> identifiers_are_calls;
	std::vector<int32_t> identifier_positions;
	int bracketCount = 0, parenCount = 0;
	bool lastTokenWasDot = true, justFinishedParenBlock = false;
	
	for (int tokenIndex = lastDotTokenIndex - 1; tokenIndex >= 0; --tokenIndex)
	{
		token = &tokens[static_cast<size_t>(tokenIndex)];
		token_type = token->token_type_;
		
		// skip backward over whitespace and comments; they make no difference to us
		if ((token_type == EidosTokenType::kTokenWhitespace) || (token_type == EidosTokenType::kTokenComment) || (token_type == EidosTokenType::kTokenCommentLong))
			continue;
		
		if (bracketCount)
		{
			// If we're inside a bracketed stretch, all we do is balance brackets and run backward.  We don't even clear lastTokenWasDot,
			// because a []. sequence puts us in the same situation as having just seen a dot – we're still waiting for an identifier.
			if (token_type == EidosTokenType::kTokenRBracket)
			{
				bracketCount++;
				continue;
			}
			if (token_type == EidosTokenType::kTokenLBracket)
			{
				bracketCount--;
				continue;
			}
			
			// Check for tokens that simply make no sense, and bail
			if ((token_type == EidosTokenType::kTokenLBrace) || (token_type == EidosTokenType::kTokenRBrace) || (token_type == EidosTokenType::kTokenSemicolon) || (token_type >= EidosTokenType::kFirstIdentifierLikeToken))
				return QStringList();
			
			continue;
		}
		else if (parenCount)
		{
			// If we're inside a paren stretch – which could be a parenthesized expression or a function call – we do similarly
			// to the brackets case, just balancing parens and running backward.  We don't clear lastTokenWasDot, because a
			// (). sequence puts us in the same situation (almost) as having just seen a dot – waiting for an identifier.
			if (token_type == EidosTokenType::kTokenRParen)
			{
				parenCount++;
				continue;
			}
			if (token_type == EidosTokenType::kTokenLParen)
			{
				parenCount--;
				
				if (parenCount == 0)
					justFinishedParenBlock = true;
				continue;
			}
			
			// Check for tokens that simply make no sense, and bail
			if ((token_type == EidosTokenType::kTokenLBrace) || (token_type == EidosTokenType::kTokenRBrace) || (token_type == EidosTokenType::kTokenSemicolon) || (token_type >= EidosTokenType::kFirstIdentifierLikeToken))
				return QStringList();
			
			continue;
		}
		
		if (!lastTokenWasDot)
		{
			// We just saw an identifier, so the only thing that can continue the key path is a dot
			if (token_type == EidosTokenType::kTokenDot)
			{
				lastTokenWasDot = true;
				justFinishedParenBlock = false;
				continue;
			}
			
			// the key path has terminated at some non-key-path token, so we're done tracing it
			break;
		}
		
		// OK, the last token was a dot (or a subset preceding a dot).  We're looking for an identifier, but we're willing
		// to get distracted by a subset sequence, since that does not change the type.  Anything else does not make sense.
		if (token_type == EidosTokenType::kTokenIdentifier)
		{
			identifiers.emplace_back(token->token_string_);
			identifiers_are_calls.push_back(justFinishedParenBlock);
			identifier_positions.emplace_back(token->token_start_);
			
			// set up to continue searching the key path backwards
			lastTokenWasDot = false;
			justFinishedParenBlock = false;
			continue;
		}
		else if (token_type == EidosTokenType::kTokenRBracket)
		{
			bracketCount++;
			continue;
		}
		else if (token_type == EidosTokenType::kTokenRParen)
		{
			parenCount++;
			continue;
		}
		
		// This makes no sense, so bail
		return QStringList();
	}
	
	// If we were in the middle of tracing the key path when the loop ended, then something is wrong, bail.
	if (lastTokenWasDot || bracketCount || parenCount)
		return QStringList();
	
	// OK, we've got an identifier chain in identifiers, in reverse order.  We want to start at
	// the beginning of the key path, and figure out what the class of the key path root is
	int key_path_index = static_cast<int>(identifiers.size()) - 1;
	std::string &identifier_name = identifiers[static_cast<size_t>(key_path_index)];
	EidosGlobalStringID identifier_ID = Eidos_GlobalStringIDForString(identifier_name);
	bool identifier_is_call = identifiers_are_calls[static_cast<size_t>(key_path_index)];
	const EidosObjectClass *key_path_class = nullptr;
	
	if (identifier_is_call)
	{
		// The root identifier is a call, so it should be a function call; try to look it up
		for (const auto& function_iter : *functionMap)
		{
			const EidosFunctionSignature *sig = function_iter.second.get();
			
			if (sig->call_name_.compare(identifier_name) == 0)
			{
				key_path_class = sig->return_class_;
				
				// In some cases, the function signature does not have the information we need, because the class of the return value
				// of the function depends upon its parameters.  This is the case for functions like sample(), rep(), and so forth.
				// For this case, we have a special mechanism set up, whereby the EidosTypeInterpreter has logged the class of the
				// return value of function calls that it has evaluated.  We can look up the correct class in that log.  This is kind
				// of a gross solution, but short of rewriting all the completion code, it seems to be the easiest fix.  (Rewriting
				// to fix this more properly would involve doing code completion using a type-annotated tree, without any of the
				// token-stream handling that we have now; that would be a better design, but I'm going to save that rewrite for later.)
				if (!key_path_class)
				{
					auto callTypeIter = callTypeTable->find(identifier_positions[static_cast<size_t>(key_path_index)]);
					
					if (callTypeIter != callTypeTable->end())
						key_path_class = callTypeIter->second;
				}
				
				break;
			}
		}
	}
	else if (typeTable)
	{
		// The root identifier is not a call, so it should be a global symbol; try to look it up
		EidosTypeSpecifier type_specifier = typeTable->GetTypeForSymbol(identifier_ID);
		
		if (!!(type_specifier.type_mask & kEidosValueMaskObject))
			key_path_class = type_specifier.object_class;
	}
	
	if (!key_path_class)
		return QStringList();				// unknown symbol at the root
	
	// Now we've got a class for the root of the key path; follow forward through the key path to arrive at the final type.
	while (--key_path_index >= 0)
	{
		identifier_name = identifiers[static_cast<size_t>(key_path_index)];
		identifier_is_call = identifiers_are_calls[static_cast<size_t>(key_path_index)];
		
		EidosGlobalStringID identifier_id = Eidos_GlobalStringIDForString(identifier_name);
		
		if (identifier_id == gEidosID_none)
			return QStringList();			// unrecognized identifier in the key path, so there is probably a typo and we can't complete off of it
		
		if (identifier_is_call)
		{
			// We have a method call; look up its signature and get the class
			const EidosCallSignature *call_signature = key_path_class->SignatureForMethod(identifier_id);
			
			if (!call_signature)
				return QStringList();			// no signature, so the class does not support the method given
			
			key_path_class = call_signature->return_class_;
		}
		else
		{
			// We have a property; look up its signature and get the class
			const EidosPropertySignature *property_signature = key_path_class->SignatureForProperty(identifier_id);
			
			if (!property_signature)
				return QStringList();			// no signature, so the class does not support the property given
			
			key_path_class = property_signature->value_class_;
		}
		
		if (!key_path_class)
			return QStringList();			// unknown symbol at the root; the property yields a non-object type
	}
	
	// OK, we've now got a EidosValue object that represents the end of the line; the final dot is off of this object.
	// So we want to extract all of its properties and methods, and return them all as candidates.
	QStringList candidates;
	const EidosObjectClass *terminus = key_path_class;
	
	// First, a sorted list of globals
	for (auto symbol_sig : *terminus->Properties())
		candidates << QString::fromStdString(symbol_sig->property_name_);
	
	candidates.sort();
	
	// Next, a sorted list of methods, with () appended
	for (auto method_sig : *terminus->Methods())
	{
		QString methodName = QString::fromStdString(method_sig->call_name_);
		
        methodName.append("()");
		candidates << methodName;
	}
	
	return candidates;
}

//- (int64_t)eidosScoreAsCompletionOfString:(NSString *)base
int64_t QtSLiMTextEdit::scoreForCandidateAsCompletionOfString(QString candidate, QString base)
{
    // Evaluate the quality of the target as a completion for completionBase and return a score.
	// We look for each character of completionBase in candidate, in order, case-insensitive; all
	// characters must be present in order for the target to be a completion at all.  Beyond that,
	// a higher score is garnered if the matches in candidate are (1) either uppercase or the 0th character,
	// and (2) if they are relatively near the beginning, and (3) if they occur contiguously.
	int64_t score = 0;
	int baseLength = base.length();
	
	// Do the comparison scan; find a match for each composed character sequence in base.  I *think*
    // QString contains QChars that represent composed character sequences already, so I think in this
    // port of the Objective-C code maybe I can ignore that issue...?  We work use rangeOfString: to do
    // searches, to avoid issues with diacritical marks, alternative composition sequences, casing, etc.
	int firstUnusedIndex = 0, firstUnmatchedIndex = 0;
	
	do
	{
		//NSRange baseRangeToMatch = [base rangeOfComposedCharacterSequenceAtIndex:firstUnmatchedIndex];
		//NSString *stringToMatch = [base substringWithRange:baseRangeToMatch];
        int baseIndexToMatch = firstUnmatchedIndex;
        QString stringToMatch = base.mid(baseIndexToMatch, 1);
        QString uppercaseStringToMatch = stringToMatch.toUpper();
		int candidateMatchIndex;
		
		if ((stringToMatch == uppercaseStringToMatch) && (firstUnmatchedIndex != 0))
		{
			// If the character in base is uppercase, we only want to match an uppercase character in candidate.
			// The exception is the first character of base; WTF should match writeTempFile() well.
            candidateMatchIndex = candidate.indexOf(stringToMatch, firstUnusedIndex);
			score += 1000;	// uppercase match
		}
		else
		{
			// If the character in base is not uppercase, we will match any case in candidate, but we prefer a
			// lowercase character if it matches the very next part of candidate, otherwise we prefer uppercase.
            candidateMatchIndex = candidate.indexOf(stringToMatch, firstUnusedIndex);
			
			if (candidateMatchIndex == firstUnusedIndex)
			{
				score += 2000;	// next-character match is even better than upper-case; continuity trumps camelcase
			}
			else
			{
				int uppercaseMatchIndex = candidate.indexOf(uppercaseStringToMatch, firstUnusedIndex);
				
				if (uppercaseMatchIndex != -1)
				{
					candidateMatchIndex = uppercaseMatchIndex;
					score += 1000;	// uppercase match
				}
				else if (firstUnusedIndex > 0)
				{
					// This match is crap; we're jumping forward to a lowercase letter, so it's unlikely to be what
					// the user wants.  So we bail.  This can be commented out to return lower-quality matches.
					return INT64_MIN;
				}
			}
		}
		
		// no match in candidate for the composed character sequence in base; candidate is not a good completion of base
		if (candidateMatchIndex == -1)
			return INT64_MIN;
		
		// matching the very beginning of candidate is very good; we really want to match the start of a candidate
		// otherwise, earlier matches are better; a match at position 0 gets the largest score increment
		if (candidateMatchIndex == 0)
			score += 100000;
		else
			score -= candidateMatchIndex;
		
		// move firstUnusedIndex to follow the matched range in candidate
		firstUnusedIndex = candidateMatchIndex + 1;
		
		// move to the next composed character sequence in base
		firstUnmatchedIndex = baseIndexToMatch + 1;
		if (firstUnmatchedIndex >= baseLength)
			break;
	}
	while (true);
	
	// We want argument-name matches to be at the top, always, when they are available, so bump their score
	if (candidate.endsWith("="))
		score += 1000000;
	
	return score;
}

//- (NSArray *)completionsFromArray:(NSArray *)candidates matchingBase:(NSString *)base
QStringList QtSLiMTextEdit::completionsFromArrayMatchingBase(QStringList candidates, QString base)
{
	QStringList completions;
	int candidateCount = candidates.size();
	
#if 0
	// This is simple prefix-based completion; if a candidates begins with base, then it is used
	for (int candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex)
	{
		QString candidate = candidates[candidateIndex];
		
		if (candidate.startsWith(base))
			completions << candidate;
	}
#else
	// This is part-based completion, where iTr will complete to initializeTreeSequence() and iGTy
	// will complete to initializeGenomicElementType().  To do this, we use a special comparator
	// that returns a score for the quality of the match, and then we sort all matches by score.
	std::vector<int64_t> scores;
	QStringList unsortedCompletions;
	
	for (int candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex)
	{
		QString candidate = candidates[candidateIndex];
		int64_t score = scoreForCandidateAsCompletionOfString(candidate, base);
		
		if (score != INT64_MIN)
		{
			unsortedCompletions << candidate;
			scores.push_back(score);
		}
	}
	
	if (scores.size())
	{
		std::vector<int64_t> order = EidosSortIndexes(scores.data(), scores.size(), false);
		
		for (int64_t index : order)
			completions << unsortedCompletions[static_cast<int>(index)];
	}
#endif
	
	return completions;
}

//- (NSArray *)completionsForTokenStream:(const std::vector<EidosToken> &)tokens index:(int)lastTokenIndex canExtend:(BOOL)canExtend withTypes:(EidosTypeTable *)typeTable functions:(EidosFunctionMap *)functionMap callTypes:(EidosCallTypeTable *)callTypeTable keywords:(NSArray *)keywords argumentNames:(NSArray *)argumentNames
QStringList QtSLiMTextEdit::completionsForTokenStream(const std::vector<EidosToken> &tokens, int lastTokenIndex, bool canExtend, EidosTypeTable *typeTable, EidosFunctionMap *functionMap, EidosCallTypeTable *callTypeTable, QStringList keywords, QStringList argumentNames)
{
	// What completions we offer depends on the token stream
	const EidosToken &token = tokens[static_cast<size_t>(lastTokenIndex)];
	EidosTokenType token_type = token.token_type_;
	
	switch (token_type)
	{
		case EidosTokenType::kTokenNone:
		case EidosTokenType::kTokenEOF:
		case EidosTokenType::kTokenWhitespace:
		case EidosTokenType::kTokenComment:
		case EidosTokenType::kTokenCommentLong:
		case EidosTokenType::kTokenInterpreterBlock:
		case EidosTokenType::kTokenContextFile:
		case EidosTokenType::kTokenContextEidosBlock:
		case EidosTokenType::kFirstIdentifierLikeToken:
			// These should never be hit
			return QStringList();
			
		case EidosTokenType::kTokenIdentifier:
		case EidosTokenType::kTokenIf:
		case EidosTokenType::kTokenWhile:
		case EidosTokenType::kTokenFor:
		case EidosTokenType::kTokenNext:
		case EidosTokenType::kTokenBreak:
		case EidosTokenType::kTokenFunction:
		case EidosTokenType::kTokenReturn:
		case EidosTokenType::kTokenElse:
		case EidosTokenType::kTokenDo:
		case EidosTokenType::kTokenIn:
			if (canExtend)
			{
				QStringList completions;
				
				// This is the tricky case, because the identifier we're extending could be the end of a key path like foo.bar[5:8].ba...
				// We need to move backwards from the current token until we find or fail to find a dot token; if we see a dot we're in
				// a key path, otherwise we're in the global context and should filter from those candidates
				for (int previousTokenIndex = lastTokenIndex - 1; previousTokenIndex >= 0; --previousTokenIndex)
				{
					const EidosToken &previous_token = tokens[static_cast<size_t>(previousTokenIndex)];
					EidosTokenType previous_token_type = previous_token.token_type_;
					
					// if the token we're on is skippable, continue backwards
					if ((previous_token_type == EidosTokenType::kTokenWhitespace) || (previous_token_type == EidosTokenType::kTokenComment) || (previous_token_type == EidosTokenType::kTokenCommentLong))
						continue;
					
					// if the token we're on is a dot, we are indeed at the end of a key path, and can fetch the completions for it
					if (previous_token_type == EidosTokenType::kTokenDot)
					{
                        completions = completionsForKeyPathEndingInTokenIndexOfTokenStream(previousTokenIndex, tokens, typeTable, functionMap, callTypeTable, keywords);
						break;
					}
					
					// if we see a semicolon or brace, we are in a completely global context
					if ((previous_token_type == EidosTokenType::kTokenSemicolon) || (previous_token_type == EidosTokenType::kTokenLBrace) || (previous_token_type == EidosTokenType::kTokenRBrace))
					{
                        completions = globalCompletionsWithTypesFunctionsKeywordsArguments(typeTable, functionMap, keywords, QStringList());
						break;
					}
					
					// if we see any other token, we are not in a key path; let's assume we're following an operator
                    completions = globalCompletionsWithTypesFunctionsKeywordsArguments(typeTable, functionMap, QStringList(), argumentNames);
					break;
				}
				
				// If we ran out of tokens, we're at the beginning of the file and so in the global context
				if (completions.size() == 0)
                    completions = globalCompletionsWithTypesFunctionsKeywordsArguments(typeTable, functionMap, keywords, QStringList());
				
				// Now we have an array of possible completions; we just need to remove those that don't complete the base string,
				// according to a heuristic algorithm, and sort those that do match by a score of their closeness of match.
                return completionsFromArrayMatchingBase(completions, QString::fromStdString(token.token_string_));
			}
			else if ((token_type == EidosTokenType::kTokenReturn) || (token_type == EidosTokenType::kTokenElse) || (token_type == EidosTokenType::kTokenDo) || (token_type == EidosTokenType::kTokenIn))
			{
				// If you can't extend and you're following an identifier, you presumably need an operator or a keyword or something;
				// you can't have two identifiers in a row.  The same is true of keywords that do not take an expression after them.
				// But return, else, do, and in can be followed immediately by an expression, so here we handle that case.  Identifiers
				// and other keywords will drop through to return nil below, expressing that we cannot complete in that case.
				// We used to put return, else, do, and in down the the operators at the bottom, but when canExtend is YES that
				// prevents them from completing to other things ("in" to "inSLiMgui", for example); moving them up to this case
				// allows that completion to work, but necessitates the addition of this block to get the correct functionality when
				// canExtend is NO.  BCH 1/22/2019
                return globalCompletionsWithTypesFunctionsKeywordsArguments(typeTable, functionMap, QStringList(), argumentNames);
			}
			
			// If the previous token was an identifier and we can't extend it, the next thing probably needs to be an operator or something
			return QStringList();
			
		case EidosTokenType::kTokenBad:
		case EidosTokenType::kTokenNumber:
		case EidosTokenType::kTokenString:
		case EidosTokenType::kTokenRParen:
		case EidosTokenType::kTokenRBracket:
		case EidosTokenType::kTokenSingleton:
			// We don't have anything to suggest after such tokens; the next thing will need to be an operator, semicolon, etc.
			return QStringList();
			
		case EidosTokenType::kTokenDot:
			// This is the other tricky case, because we're being asked to extend a key path like foo.bar[5:8].
            return completionsForKeyPathEndingInTokenIndexOfTokenStream(lastTokenIndex, tokens, typeTable, functionMap, callTypeTable, keywords);
			
		case EidosTokenType::kTokenSemicolon:
		case EidosTokenType::kTokenLBrace:
		case EidosTokenType::kTokenRBrace:
			// We are in the global context and anything goes, including a new statement
            return globalCompletionsWithTypesFunctionsKeywordsArguments(typeTable, functionMap, keywords, QStringList());
			
		case EidosTokenType::kTokenColon:
		case EidosTokenType::kTokenComma:
		case EidosTokenType::kTokenLParen:
		case EidosTokenType::kTokenLBracket:
		case EidosTokenType::kTokenPlus:
		case EidosTokenType::kTokenMinus:
		case EidosTokenType::kTokenMod:
		case EidosTokenType::kTokenMult:
		case EidosTokenType::kTokenExp:
		case EidosTokenType::kTokenAnd:
		case EidosTokenType::kTokenOr:
		case EidosTokenType::kTokenDiv:
		case EidosTokenType::kTokenConditional:
		case EidosTokenType::kTokenAssign:
		case EidosTokenType::kTokenEq:
		case EidosTokenType::kTokenLt:
		case EidosTokenType::kTokenLtEq:
		case EidosTokenType::kTokenGt:
		case EidosTokenType::kTokenGtEq:
		case EidosTokenType::kTokenNot:
		case EidosTokenType::kTokenNotEq:
			// We are following an operator or similar, so globals are OK but new statements are not
            return globalCompletionsWithTypesFunctionsKeywordsArguments(typeTable, functionMap, QStringList(), argumentNames);
	}
	
	return QStringList();
}

//- (NSUInteger)rangeOffsetForCompletionRange
int QtSLiMTextEdit::rangeOffsetForCompletionRange(void)
{
	// This is for EidosConsoleTextView to be able to remove the prompt string from the string being completed
	return 0;
}

//- (NSArray *)uniquedArgumentNameCompletions:(std::vector<std::string> *)argumentCompletions
QStringList QtSLiMTextEdit::uniquedArgumentNameCompletions(std::vector<std::string> *argumentCompletions)
{
	// put argument-name completions, if any, at the top of the list; we unique them (preserving order) and add "="
	if (argumentCompletions && argumentCompletions->size())
	{
		QStringList completionsWithArgs;
		
		for (std::string &arg_completion : *argumentCompletions)
            completionsWithArgs << QString::fromStdString(arg_completion).append("=");
		
        completionsWithArgs.removeDuplicates();
		return completionsWithArgs;
	}
	
	return QStringList();
}

//- (BOOL)eidosTextView:(EidosTextView *)eidosTextView completionContextWithScriptString:(NSString *)completionScriptString selection:(NSRange)selection typeTable:(EidosTypeTable **)typeTable functionMap:(EidosFunctionMap **)functionMap callTypeTable:(EidosCallTypeTable **)callTypeTable keywords:(NSMutableArray *)keywords argumentNameCompletions:(std::vector<std::string> *)argNameCompletions
void QtSLiMTextEdit::slimSpecificCompletion(QString completionScriptString, NSRange selection, EidosTypeTable **typeTable, EidosFunctionMap **functionMap, EidosCallTypeTable **callTypeTable, QStringList *keywords, std::vector<std::string> *argNameCompletions)
{
    // Code completion in the console window and other ancillary EidosTextViews should use the standard code completion
    // machinery in EidosTextView.  In the script view, however, we want things to behave somewhat differently.  In
    // other contexts, we want the variables and functions available to depend solely upon the current state of the
    // simulation; whatever is actually available is what code completion provides.  In the script view, however, we
    // want to be smarter than that.  Initialization functions should be available when the user is completing
    // inside an initialize() callback, and not available otherwise, regardless of the current simulation state.
    // Similarly, variables associated with particular types of callbacks should always be available within those
    // callbacks; variables defined in script blocks other than the focal block should not be visible in code
    // completion; defined constants should be available everywhere; and it should be assumed that variables with
    // names like pX, mX, gX, and sX have their usual types even if they are not presently defined.  This delegate
    // method accomplishes all of those things, by replacing the standard EidosTextView completion handling.
    std::string script_string(completionScriptString.toStdString());
    SLiMEidosScript script(script_string);
    
    // Parse an "interpreter block" bounded by an EOF rather than a "script block" that requires braces
    script.Tokenize(true, false);				// make bad tokens as needed, do not keep nonsignificant tokens
    script.ParseSLiMFileToAST(true);			// make bad nodes as needed (i.e. never raise, and produce a correct tree)
    
    // Substitute a type table of class SLiMTypeTable and add any defined symbols to it.  We use SLiMTypeTable so that
    // variables like pX, gX, mX, and sX have a known object type even if they are not presently defined in the simulation.
    *typeTable = new SLiMTypeTable();
    
    QtSLiMWindow *windowSLiMController = slimControllerForWindow();
    QtSLiMEidosConsole *consoleController = (windowSLiMController ? windowSLiMController->ConsoleController() : nullptr);
    EidosSymbolTable *symbols = (consoleController ? consoleController->symbolTable() : nullptr);
    
    if (symbols)
        symbols->AddSymbolsToTypeTable(*typeTable);
    
    // Use the script text view's facility for using type-interpreting to get a "definitive" function map.  This way
    // all functions that are defined, even if below the completion point, end up in the function map.
    *functionMap = functionMapForScriptString(toPlainText(), false);
    
    SLiMSim::AddSLiMFunctionsToMap(**functionMap);
    
    // Now we scan through the children of the root node, each of which is the root of a SLiM script block.  The last
    // script block is the one we are actually completing inside, but we also want to do a quick scan of any other
    // blocks we find, solely to add entries for any defineConstant() calls we can decode.
    const EidosASTNode *script_root = script.AST();
    
    if (script_root && (script_root->children_.size() > 0))
    {
        EidosASTNode *completion_block = script_root->children_.back();
        
        // If the last script block has a range that ends before the start of the selection, then we are completing after the end
        // of that block, at the outer level of the script.  Detect that case and fall through to the handler for it at the end.
        int32_t completion_block_end = completion_block->token_->token_end_;
        
        if (static_cast<int>(selection.location) > completion_block_end)
        {
            // Selection is after end of completion_block
            completion_block = nullptr;
        }
        
        if (completion_block)
        {
            for (EidosASTNode *script_block_node : script_root->children_)
            {
                // script_block_node can have various children, such as an sX identifier, start and end generations, a block type
                // identifier like late(), and then the root node of the compound statement for the script block.  We want to
                // decode the parts that are important to us, without the complication of making SLiMEidosBlock objects.
                EidosASTNode *block_statement_root = nullptr;
                SLiMEidosBlockType block_type = SLiMEidosBlockType::SLiMEidosEventEarly;
                
                for (EidosASTNode *block_child : script_block_node->children_)
                {
                    EidosToken *child_token = block_child->token_;
                    
                    if (child_token->token_type_ == EidosTokenType::kTokenIdentifier)
                    {
                        const std::string &child_string = child_token->token_string_;
                        
                        if (child_string.compare(gStr_early) == 0)				block_type = SLiMEidosBlockType::SLiMEidosEventEarly;
                        else if (child_string.compare(gStr_late) == 0)			block_type = SLiMEidosBlockType::SLiMEidosEventLate;
                        else if (child_string.compare(gStr_initialize) == 0)	block_type = SLiMEidosBlockType::SLiMEidosInitializeCallback;
                        else if (child_string.compare(gStr_fitness) == 0)		block_type = SLiMEidosBlockType::SLiMEidosFitnessCallback;	// can't distinguish global fitness callbacks, but no need to
                        else if (child_string.compare(gStr_interaction) == 0)	block_type = SLiMEidosBlockType::SLiMEidosInteractionCallback;
                        else if (child_string.compare(gStr_mateChoice) == 0)	block_type = SLiMEidosBlockType::SLiMEidosMateChoiceCallback;
                        else if (child_string.compare(gStr_modifyChild) == 0)	block_type = SLiMEidosBlockType::SLiMEidosModifyChildCallback;
                        else if (child_string.compare(gStr_recombination) == 0)	block_type = SLiMEidosBlockType::SLiMEidosRecombinationCallback;
                        else if (child_string.compare(gStr_mutation) == 0)		block_type = SLiMEidosBlockType::SLiMEidosMutationCallback;
                        else if (child_string.compare(gStr_reproduction) == 0)	block_type = SLiMEidosBlockType::SLiMEidosReproductionCallback;
                        
                        // Check for an sX designation on a script block and, if found, add a symbol for it
                        else if ((block_child == script_block_node->children_[0]) && (child_string.length() >= 2))
                        {
                            if (child_string[0] == 's')
                            {
                                bool all_numeric = true;
                                
                                for (size_t idx = 1; idx < child_string.length(); ++idx)
                                    if (!isdigit(child_string[idx]))
                                        all_numeric = false;
                                
                                if (all_numeric)
                                {
                                    EidosGlobalStringID constant_id = Eidos_GlobalStringIDForString(child_string);
                                    
                                    (*typeTable)->SetTypeForSymbol(constant_id, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class});
                                }
                            }
                        }
                    }
                    else if (child_token->token_type_ == EidosTokenType::kTokenLBrace)
                    {
                        block_statement_root = block_child;
                    }
                    else if (child_token->token_type_ == EidosTokenType::kTokenFunction)
                    {
                        // We handle function blocks a bit differently; see below
                        block_type = SLiMEidosBlockType::SLiMEidosUserDefinedFunction;
                        
                        if (block_child->children_.size() >= 4)
                            block_statement_root = block_child->children_[3];
                    }
                }
                
                // Now we know the type of the node, and the root node of its compound statement; extract what we want
                if (block_statement_root)
                {
                    // The symbol sim is defined in all blocks except initialize() blocks; we need to add and remove it
                    // dynamically so that each block has it defined or not defined as necessary.  Since the completion block
                    // is last, the sim symbol will be correctly defined at the end of this process.
                    if (block_type == SLiMEidosBlockType::SLiMEidosInitializeCallback)
                        (*typeTable)->RemoveTypeForSymbol(gID_sim);
                    else
                        (*typeTable)->SetTypeForSymbol(gID_sim, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_SLiMSim_Class});
                    
                    // The slimgui symbol is always available within a block, but not at the top level
                    (*typeTable)->SetTypeForSymbol(gID_slimgui, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_SLiMgui_Class});
                    
                    // Do the same for the zero-generation functions, which should be defined in initialization() blocks and
                    // not in other blocks; we add and remove them dynamically so they are defined as appropriate.  We ought
                    // to do this for other block-specific stuff as well (like the stuff below), but it is unlikely to matter.
                    // Note that we consider the zero-gen functions to always be defined inside function blocks, since the
                    // function might be called from the zero gen (we have no way of knowing definitively).
                    if ((block_type == SLiMEidosBlockType::SLiMEidosInitializeCallback) || (block_type == SLiMEidosBlockType::SLiMEidosUserDefinedFunction))
                        SLiMSim::AddZeroGenerationFunctionsToMap(**functionMap);
                    else
                        SLiMSim::RemoveZeroGenerationFunctionsFromMap(**functionMap);
                    
                    if (script_block_node == completion_block)
                    {
                        // This is the block we're actually completing in the context of; it is also the last block in the script
                        // snippet that we're working with.  We want to first define any callback-associated variables for the block.
                        // Note that self is not defined inside functions, even though they are SLiMEidosBlocks; we pretend we are Eidos.
                        if (block_type == SLiMEidosBlockType::SLiMEidosUserDefinedFunction)
                            (*typeTable)->RemoveTypeForSymbol(gID_self);
                        else
                            (*typeTable)->SetTypeForSymbol(gID_self, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_SLiMEidosBlock_Class});
                        
                        switch (block_type)
                        {
                        case SLiMEidosBlockType::SLiMEidosEventEarly:
                            break;
                        case SLiMEidosBlockType::SLiMEidosEventLate:
                            break;
                        case SLiMEidosBlockType::SLiMEidosInitializeCallback:
                            (*typeTable)->RemoveSymbolsOfClass(gSLiM_Subpopulation_Class);	// subpops defined upstream from us still do not exist for us
                            break;
                        case SLiMEidosBlockType::SLiMEidosFitnessCallback:
                        case SLiMEidosBlockType::SLiMEidosFitnessGlobalCallback:
                            (*typeTable)->SetTypeForSymbol(gID_mut,				EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Mutation_Class});
                            (*typeTable)->SetTypeForSymbol(gID_homozygous,		EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
                            (*typeTable)->SetTypeForSymbol(gID_relFitness,		EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
                            (*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_genome1,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_genome2,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
                            break;
                        case SLiMEidosBlockType::SLiMEidosInteractionCallback:
                            (*typeTable)->SetTypeForSymbol(gID_distance,		EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
                            (*typeTable)->SetTypeForSymbol(gID_strength,		EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
                            (*typeTable)->SetTypeForSymbol(gID_receiver,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_exerter,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
                            break;
                        case SLiMEidosBlockType::SLiMEidosMateChoiceCallback:
                            (*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_genome1,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_genome2,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
                            (*typeTable)->SetTypeForSymbol(gID_sourceSubpop,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
                            (*typeTable)->SetTypeForSymbol(gEidosID_weights,	EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
                            break;
                        case SLiMEidosBlockType::SLiMEidosModifyChildCallback:
                            (*typeTable)->SetTypeForSymbol(gID_child,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_childGenome1,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_childGenome2,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_childIsFemale,	EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
                            (*typeTable)->SetTypeForSymbol(gID_parent1,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_parent1Genome1,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_parent1Genome2,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_isCloning,		EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
                            (*typeTable)->SetTypeForSymbol(gID_isSelfing,		EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
                            (*typeTable)->SetTypeForSymbol(gID_parent2,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_parent2Genome1,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_parent2Genome2,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
                            (*typeTable)->SetTypeForSymbol(gID_sourceSubpop,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
                            break;
                        case SLiMEidosBlockType::SLiMEidosRecombinationCallback:
                            (*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_genome1,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_genome2,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
                            (*typeTable)->SetTypeForSymbol(gID_breakpoints,		EidosTypeSpecifier{kEidosValueMaskInt, nullptr});
                            break;
                        case SLiMEidosBlockType::SLiMEidosMutationCallback:
                            (*typeTable)->SetTypeForSymbol(gID_mut,				EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Mutation_Class});
                            (*typeTable)->SetTypeForSymbol(gID_parent,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_element,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_GenomicElement_Class});
                            (*typeTable)->SetTypeForSymbol(gID_genome,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
                            (*typeTable)->SetTypeForSymbol(gID_originalNuc,		EidosTypeSpecifier{kEidosValueMaskInt, nullptr});
                            break;
                        case SLiMEidosBlockType::SLiMEidosReproductionCallback:
                            (*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_genome1,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_genome2,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Genome_Class});
                            (*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
                            break;
                        case SLiMEidosBlockType::SLiMEidosUserDefinedFunction:
                        {
                            // Similar to the local variables that are defined for callbacks above, here we need to define the parameters to the
                            // function, by parsing the relevant AST nodes; this is parallel to EidosTypeInterpreter::TypeEvaluate_FunctionDecl()
                            EidosASTNode *function_declaration_node = script_block_node->children_[0];
                            const EidosASTNode *param_list_node = function_declaration_node->children_[2];
                            const std::vector<EidosASTNode *> &param_nodes = param_list_node->children_;
                            std::vector<std::string> used_param_names;
                            
                            for (EidosASTNode *param_node : param_nodes)
                            {
                                const std::vector<EidosASTNode *> &param_children = param_node->children_;
                                int param_children_count = static_cast<int>(param_children.size());
                                
                                if ((param_children_count == 2) || (param_children_count == 3))
                                {
                                    EidosTypeSpecifier &param_type = param_children[0]->typespec_;
                                    const std::string &param_name = param_children[1]->token_->token_string_;
                                    
                                    // Check param_name; it needs to not be used by another parameter
                                    if (std::find(used_param_names.begin(), used_param_names.end(), param_name) != used_param_names.end())
                                        continue;
                                    
                                    if (param_children_count >= 2)
                                    {
                                        // param_node has 2 or 3 children (type, identifier, [default]); we don't care about default values
                                        (*typeTable)->SetTypeForSymbol(Eidos_GlobalStringIDForString(param_name), param_type);
                                    }
                                }
                            }
                            break;
                        }
                        case SLiMEidosBlockType::SLiMEidosNoBlockType: break;	// never hit
                        }
                    }
                    
                    if (script_block_node == completion_block)
                    {
                        // Make a type interpreter and add symbols to our type table using it
                        // We use SLiMTypeInterpreter because we want to pick up definitions of SLiM constants
                        SLiMTypeInterpreter typeInterpreter(block_statement_root, **typeTable, **functionMap, **callTypeTable);
                        
                        typeInterpreter.TypeEvaluateInterpreterBlock_AddArgumentCompletions(argNameCompletions, script_string.length());	// result not used
                        
                        return;
                    }
                    else
                    {
                        // This is not the block we're completing in.  We want to add symbols for any constant-defining calls
                        // in this block; apart from that, this block cannot affect the completion block, due to scoping.
                        
                        // Make a type interpreter and add symbols to our type table using it
                        // We use SLiMTypeInterpreter because we want to pick up definitions of SLiM constants
                        SLiMTypeInterpreter typeInterpreter(block_statement_root, **typeTable, **functionMap, **callTypeTable, true);
                        
                        typeInterpreter.TypeEvaluateInterpreterBlock();	// result not used
                    }
                }
            }
        }
    }
    
    // We drop through to here if we have a bad or empty script root, or if the final script block (completion_block) didn't
    // have a compound statement (meaning its starting brace has not yet been typed), or if we're completing outside of any
    // existing script block.  In these sorts of cases, we want to return completions for the outer level of a SLiM script.
    // This means that standard Eidos language keywords like "while", "next", etc. are not legal, but SLiM script block
    // keywords like "early", "late", "fitness", "interaction", "mateChoice", "modifyChild", "recombination", "mutation",
    // and "reproduction" are.
    // Note that the strings here are display strings; they are fixed to contain newlines in insertCompletion()
    keywords->clear();
    (*keywords) << "initialize() { }";
    (*keywords) << "early() { }";
    (*keywords) << "late() { }";
    (*keywords) << "fitness() { }";
    (*keywords) << "interaction() { }";
    (*keywords) << "mateChoice() { }";
    (*keywords) << "modifyChild() { }";
    (*keywords) << "recombination() { }";
    (*keywords) << "mutation() { }";
    (*keywords) << "reproduction() { }";
    (*keywords) << "function (void)name(void) { }";
    
    // At the outer level, functions are also not legal
    (*functionMap)->clear();
    
    // And no variables exist except SLiM objects like pX, gX, mX, sX
    std::vector<EidosGlobalStringID> symbol_ids = (*typeTable)->AllSymbolIDs();
    
    for (EidosGlobalStringID symbol_id : symbol_ids)
        if (((*typeTable)->GetTypeForSymbol(symbol_id).type_mask != kEidosValueMaskObject) || (symbol_id == gID_sim) || (symbol_id == gID_slimgui))
            (*typeTable)->RemoveTypeForSymbol(symbol_id);
}

//- (void)_completionHandlerWithRangeForCompletion:(NSRange *)baseRange completions:(NSArray **)completions
void QtSLiMTextEdit::_completionHandlerWithRangeForCompletion(NSRange *baseRange, QStringList *completions)
{
	QString scriptString = toPlainText();
    NSRange selection = {textCursor().selectionStart(), textCursor().selectionEnd() - textCursor().selectionStart()};	// ignore charRange and work from the selection
	int rangeOffset = rangeOffsetForCompletionRange();
	
	// correct the script string to have only what is entered after the prompt, if we are a EidosConsoleTextView
	if (rangeOffset)
	{
        scriptString.remove(0, rangeOffset);
        selection.location -= rangeOffset;
		selection.length -= rangeOffset;
	}
	
	int selStart = selection.location;
	
	//if (selStart != NSNotFound)       // I don't think this can happen in Qt; you always have a text cursor...
	{
		// Get the substring up to the start of the selection; that is the range relevant for completion
		QString scriptSubstring = scriptString.left(selStart);
		std::string script_string(scriptSubstring.toStdString());
		
		// Do shared completion processing that can be intercepted by our delegate: getting a type table for defined variables,
		// as well as a function map and any added language keywords, all of which depend upon the point of completion
		EidosTypeTable typeTable;
		EidosTypeTable *typeTablePtr = &typeTable;
		EidosFunctionMap functionMap(*EidosInterpreter::BuiltInFunctionMap());
		EidosFunctionMap *functionMapPtr = &functionMap;
		EidosCallTypeTable callTypeTable;
		EidosCallTypeTable *callTypeTablePtr = &callTypeTable;
        QStringList keywords = {"break", "do", "else", "for", "if", "in", "next", "return", "while", "function"};
		std::vector<std::string> argumentCompletions;
		
		if (scriptType == ScriptType::SLiMScriptType)
            slimSpecificCompletion(scriptSubstring, selection, &typeTablePtr, &functionMapPtr, &callTypeTablePtr, &keywords, &argumentCompletions);
		
		// set up automatic disposal of a substitute type table or function map provided by delegate
		std::unique_ptr<EidosTypeTable> raii_typeTablePtr((typeTablePtr != &typeTable) ? typeTablePtr : nullptr);
		std::unique_ptr<EidosFunctionMap> raii_functionMapPtr((functionMapPtr != &functionMap) ? functionMapPtr : nullptr);
		std::unique_ptr<EidosCallTypeTable> raii_callTypeTablePtr((callTypeTablePtr != &callTypeTable) ? callTypeTablePtr : nullptr);
		
		if (scriptType != ScriptType::SLiMScriptType)
		{
			// First, set up a base type table using the symbol table
			EidosSymbolTable *symbols = gEidosConstantsSymbolTable;
			
            symbols = symbolsFromBaseSymbols(symbols);
			
			if (symbols)
				symbols->AddSymbolsToTypeTable(typeTablePtr);
			
			// Next, a definitive function map that covers all functions defined in the entire script string (not just the script above
			// the completion point); this seems best, for mutually recursive functions etc..  Duplicate it back into functionMap and
			// delete the original, so we don't get confused.
			EidosFunctionMap *definitive_function_map = functionMapForScriptString(scriptString, false);
			
			functionMap = *definitive_function_map;
			delete definitive_function_map;
			
			// Next, add type table entries based on parsing and analysis of the user's code
			EidosScript script(script_string);
			
			script.Tokenize(true, false);					// make bad tokens as needed, do not keep nonsignificant tokens
			script.ParseInterpreterBlockToAST(true, true);	// make bad nodes as needed (i.e. never raise, and produce a correct tree)
			
			EidosTypeInterpreter typeInterpreter(script, *typeTablePtr, *functionMapPtr, *callTypeTablePtr);
			
			typeInterpreter.TypeEvaluateInterpreterBlock_AddArgumentCompletions(&argumentCompletions, script_string.length());	// result not used
		}
		
		// Tokenize; we can't use the tokenization done above, as we want whitespace tokens here...
		EidosScript script(script_string);
		script.Tokenize(true, true);	// make bad tokens as needed, keep nonsignificant tokens
		
		const std::vector<EidosToken> &tokens = script.Tokens();
		int lastTokenIndex = static_cast<int>(tokens.size()) - 1;
		bool endedCleanly = false, lastTokenInterrupted = false;
		
		// if we ended with an EOF, that means we did not have a raise and there should be no untokenizable range at the end
		if ((lastTokenIndex >= 0) && (tokens[static_cast<size_t>(lastTokenIndex)].token_type_ == EidosTokenType::kTokenEOF))
		{
			--lastTokenIndex;
			endedCleanly = true;
		}
		
		// if we are at the end of a comment, without whitespace following it, then we are actually in the comment, and cannot complete
		// BCH 5 August 2017: Note that EidosTokenType::kTokenCommentLong is deliberately omitted here; this rule does not apply to it
		if ((lastTokenIndex >= 0) && (tokens[static_cast<size_t>(lastTokenIndex)].token_type_ == EidosTokenType::kTokenComment))
		{
            if (baseRange) *baseRange = {NSNotFound, 0};
			if (completions) *completions = QStringList();
			return;
		}
		
		// if we ended with whitespace or a comment, the previous token cannot be extended
		while (lastTokenIndex >= 0) {
			const EidosToken &token = tokens[static_cast<size_t>(lastTokenIndex)];
			
			if ((token.token_type_ != EidosTokenType::kTokenWhitespace) && (token.token_type_ != EidosTokenType::kTokenComment) && (token.token_type_ != EidosTokenType::kTokenCommentLong))
				break;
			
			--lastTokenIndex;
			lastTokenInterrupted = true;
		}
		
		// now diagnose what range we want to use as a basis for completion
		if (!endedCleanly)
		{
			// the selection is at the end of an untokenizable range; we might be in the middle of a string or a comment,
			// or there might be a tokenization error upstream of us.  let's not try to guess what the situation is.
            if (baseRange) *baseRange = {NSNotFound, 0};
			if (completions) *completions = QStringList();
			return;
		}
		else
		{
			if (lastTokenIndex < 0)
			{
				// We're at the end of nothing but initial whitespace and comments; or if (!lastTokenInterrupted),
				// we're at the very beginning of the file.  Either way, offer insertion-point completions.
                if (baseRange) *baseRange = {selection.location + rangeOffset, 0};
				if (completions) *completions = globalCompletionsWithTypesFunctionsKeywordsArguments(typeTablePtr, functionMapPtr, keywords, QStringList());
				return;
			}
			
			const EidosToken &token = tokens[static_cast<size_t>(lastTokenIndex)];
			EidosTokenType token_type = token.token_type_;
			
			// BCH 31 May 2016: If the previous token is a right-paren, that is a tricky case because we could be following
			// for(), an if(), or while (), in which case we should allow an identifier to follow the right paren, or we could
			// be following parentheses for grouping, i.e. (a+b), or parentheses for a function call, foo(), in which case we
			// should not allow an identifier to follow the right paren.  This annoyance is basically because the right paren
			// serves a lot of different functions in the language and so just knowing that we are after one is not sufficient.
			// So we will walk backwards, balancing our parenthesis count, to try to figure out which case we are in.  Note
			// that even this code is not quite right; it mischaracterizes the do...while() case as allowing an identifier to
			// follow, because it sees the "while".  This is harder to fix, and do...while() is not a common construct, and
			// the mistake is pretty harmless, so whatever.
			if (token_type == EidosTokenType::kTokenRParen)
			{
				int parenCount = 1;
				int walkbackIndex = lastTokenIndex;
				
				// First walk back until our paren count balances
				while (--walkbackIndex >= 0)
				{
					const EidosToken &walkback_token = tokens[static_cast<size_t>(walkbackIndex)];
					EidosTokenType walkback_token_type = walkback_token.token_type_;
					
					if (walkback_token_type == EidosTokenType::kTokenRParen)			parenCount++;
					else if (walkback_token_type == EidosTokenType::kTokenLParen)		parenCount--;
					
					if (parenCount == 0)
						break;
				}
				
				// Then walk back over whitespace, and if the first non-white thing we see is right, allow completion
				while (--walkbackIndex >= 0)
				{
					const EidosToken &walkback_token = tokens[static_cast<size_t>(walkbackIndex)];
					EidosTokenType walkback_token_type = walkback_token.token_type_;
					
					if ((walkback_token_type != EidosTokenType::kTokenWhitespace) && (walkback_token_type != EidosTokenType::kTokenComment) && (walkback_token_type != EidosTokenType::kTokenCommentLong))
					{
						if ((walkback_token_type == EidosTokenType::kTokenFor) || (walkback_token_type == EidosTokenType::kTokenWhile) || (walkback_token_type == EidosTokenType::kTokenIf))
						{
							// We are at the end of for(), if(), or while(), so we allow global completions as if we were after a semicolon
                            if (baseRange) *baseRange = {selection.location + rangeOffset, 0};
							if (completions) *completions = globalCompletionsWithTypesFunctionsKeywordsArguments(typeTablePtr, functionMapPtr, keywords, QStringList());
							return;
						}
						break;	// we didn't hit one of the favored cases, so the code below will reject completion
					}
				}
			}
			
			if (lastTokenInterrupted)
			{
				// the last token cannot be extended, so if the last token is something an identifier can follow, like an
				// operator, then we can offer completions at the insertion point based on that, otherwise punt.
				if ((token_type == EidosTokenType::kTokenNumber) || (token_type == EidosTokenType::kTokenString) || (token_type == EidosTokenType::kTokenRParen) || (token_type == EidosTokenType::kTokenRBracket) || (token_type == EidosTokenType::kTokenIdentifier) || (token_type == EidosTokenType::kTokenIf) || (token_type == EidosTokenType::kTokenWhile) || (token_type == EidosTokenType::kTokenFor) || (token_type == EidosTokenType::kTokenNext) || (token_type == EidosTokenType::kTokenBreak) || (token_type == EidosTokenType::kTokenFunction))
				{
                    if (baseRange) *baseRange = {NSNotFound, 0};
					if (completions) *completions = QStringList();
					return;
				}
				
                if (baseRange) *baseRange = {selection.location + rangeOffset, 0};
				if (completions)
				{
                    QStringList argumentCompletionsArray = uniquedArgumentNameCompletions(&argumentCompletions);
                    
                    *completions = completionsForTokenStream(tokens, lastTokenIndex, false, typeTablePtr, functionMapPtr, callTypeTablePtr, keywords, argumentCompletionsArray);
				}
				
				return;
			}
			else
			{
				// the last token was not interrupted, so we can offer completions of it if we want to.
                NSRange tokenRange = {token.token_UTF16_start_, token.token_UTF16_end_ - token.token_UTF16_start_ + 1};
				
				if (token_type >= EidosTokenType::kTokenIdentifier)
				{
                    if (baseRange) *baseRange = {tokenRange.location + rangeOffset, tokenRange.length};
					if (completions)
					{
                        QStringList argumentCompletionsArray = uniquedArgumentNameCompletions(&argumentCompletions);
						
                        *completions = completionsForTokenStream(tokens, lastTokenIndex, true, typeTablePtr, functionMapPtr, callTypeTablePtr, keywords, argumentCompletionsArray);
					}
					return;
				}
				
				if ((token_type == EidosTokenType::kTokenNumber) || (token_type == EidosTokenType::kTokenString) || (token_type == EidosTokenType::kTokenRParen) || (token_type == EidosTokenType::kTokenRBracket))
				{
                    if (baseRange) *baseRange = {NSNotFound, 0};
					if (completions) *completions = QStringList();
					return;
				}
				
                if (baseRange) *baseRange = {selection.location + rangeOffset, 0};
				if (completions)
				{
                    QStringList argumentCompletionsArray = uniquedArgumentNameCompletions(&argumentCompletions);
					
                    *completions = completionsForTokenStream(tokens, lastTokenIndex, false, typeTablePtr, functionMapPtr, callTypeTablePtr, keywords, argumentCompletionsArray);
				}
				return;
			}
		}
	}
}


//
//  LineNumberArea
//

// This provides line numbers in QtSLiMScriptTextEdit
// This code is adapted from https://doc.qt.io/qt-5/qtwidgets-widgets-codeeditor-example.html
class LineNumberArea : public QWidget
{
public:
    LineNumberArea(QtSLiMScriptTextEdit *editor) : QWidget(editor), codeEditor(editor) {}

    virtual QSize sizeHint() const override { return QSize(codeEditor->lineNumberAreaWidth(), 0); }

protected:
    virtual void paintEvent(QPaintEvent *event) override { codeEditor->lineNumberAreaPaintEvent(event); }

private:
    QtSLiMScriptTextEdit *codeEditor;
};


//
//  QtSLiMScriptTextEdit
//

QtSLiMScriptTextEdit::QtSLiMScriptTextEdit(const QString &text, QWidget *parent) : QtSLiMTextEdit(text, parent)
{
    initializeLineNumbers();
}

QtSLiMScriptTextEdit::QtSLiMScriptTextEdit(QWidget *parent) : QtSLiMTextEdit(parent)
{
    initializeLineNumbers();
}

void QtSLiMScriptTextEdit::initializeLineNumbers(void)
{
    // This code is adapted from https://doc.qt.io/qt-5/qtwidgets-widgets-codeeditor-example.html
    lineNumberArea = new LineNumberArea(this);
    
    connect(this->document(), SIGNAL(blockCountChanged(int)), this, SLOT(updateLineNumberAreaWidth(int)));
    connect(this->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(updateLineNumberArea(int)));
    connect(this, SIGNAL(textChanged()), this, SLOT(updateLineNumberArea()));
    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(updateLineNumberArea()));
    
    connect(this, &QtSLiMScriptTextEdit::cursorPositionChanged, this, &QtSLiMScriptTextEdit::highlightCurrentLine);
    
    // Watch prefs for line numbering and highlighting
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::showLineNumbersPrefChanged, this, [this]() { updateLineNumberArea(); });
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::highlightCurrentLinePrefChanged, this, [this]() { highlightCurrentLine(); });
    
    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
}

QtSLiMScriptTextEdit::~QtSLiMScriptTextEdit()
{
}

QStringList QtSLiMScriptTextEdit::linesForRoundedSelection(QTextCursor &cursor, bool &movedBack)
{
    // find the start and end of the blocks we're operating on
    int anchor = cursor.anchor(), pos = cursor.position();
    if (anchor > pos)
        std::swap(anchor, pos);
    movedBack = false;
    
    QTextCursor startBlockCursor(cursor);
    startBlockCursor.setPosition(anchor, QTextCursor::MoveAnchor);
    startBlockCursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    QTextCursor endBlockCursor(cursor);
    endBlockCursor.setPosition(pos, QTextCursor::MoveAnchor);
    if (endBlockCursor.atBlockStart() && (pos > anchor))
    {
        // the selection includes the newline at the end of the last line; we need to move backward to avoid swallowing the following line
        endBlockCursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::MoveAnchor);
        movedBack = true;
    }
    endBlockCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
    
    // select the whole lines we're operating on
    cursor.beginEditBlock();
    cursor.setPosition(startBlockCursor.position(), QTextCursor::MoveAnchor);
    cursor.setPosition(endBlockCursor.position(), QTextCursor::KeepAnchor);
    
    // separate the lines, remove a tab at the start of each, and rejoin them
    QString selectedString = cursor.selectedText();
    QRegularExpression lineEndMatch("\\R", QRegularExpression::UseUnicodePropertiesOption);
    
    return selectedString.split(lineEndMatch, QString::KeepEmptyParts);
}

void QtSLiMScriptTextEdit::shiftSelectionLeft(void)
{
     if (isEnabled() && !isReadOnly())
	{
        QTextCursor &&cursor = textCursor();
        bool movedBack;
        QStringList lines = linesForRoundedSelection(cursor, movedBack);
        
        for (QString &line : lines)
            if (line.length() && (line[0] == '\t'))
                line.remove(0, 1);
        
        QString replacementString = lines.join(QChar::ParagraphSeparator);
		
        // end the editing block, producing one undo-able operation
        cursor.insertText(replacementString);
        cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor, replacementString.length());
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, replacementString.length());
        if (movedBack)
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
		cursor.endEditBlock();
        setTextCursor(cursor);
	}
	else
	{
		qApp->beep();
	}
}

void QtSLiMScriptTextEdit::shiftSelectionRight(void)
{
    if (isEnabled() && !isReadOnly())
	{
        QTextCursor &&cursor = textCursor();
        bool movedBack;
        QStringList lines = linesForRoundedSelection(cursor, movedBack);
        
        for (QString &line : lines)
            line.insert(0, '\t');
        
        QString replacementString = lines.join(QChar::ParagraphSeparator);
		
        // end the editing block, producing one undo-able operation
        cursor.insertText(replacementString);
        cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor, replacementString.length());
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, replacementString.length());
        if (movedBack)
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
		cursor.endEditBlock();
        setTextCursor(cursor);
	}
	else
	{
		qApp->beep();
	}
}

void QtSLiMScriptTextEdit::commentUncommentSelection(void)
{
    if (isEnabled() && !isReadOnly())
	{
        QTextCursor &&cursor = textCursor();
        bool movedBack;
        QStringList lines = linesForRoundedSelection(cursor, movedBack);
        
        // decide whether we are commenting or uncommenting; we are only uncommenting if every line spanned by the selection starts with "//"
		bool uncommenting = true;
        
        for (QString &line : lines)
        {
            if (!line.startsWith("//"))
            {
                uncommenting = false;
                break;
            }
        }
        
        // now do the comment / uncomment
        if (uncommenting)
        {
            for (QString &line : lines)
                line.remove(0, 2);
        }
        else
        {
            for (QString &line : lines)
                line.insert(0, "//");
        }
        
        QString replacementString = lines.join(QChar::ParagraphSeparator);
		
        // end the editing block, producing one undo-able operation
        cursor.insertText(replacementString);
        cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor, replacementString.length());
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, replacementString.length());
        if (movedBack)
            cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
		cursor.endEditBlock();
        setTextCursor(cursor);
	}
	else
	{
		qApp->beep();
	}
}

// From here down is the machinery for providing line nunbers with LineNumberArea
// This code is adapted from https://doc.qt.io/qt-5/qtwidgets-widgets-codeeditor-example.html
// adaptations from QPlainTextEdit to QTextEdit were used from https://stackoverflow.com/a/24596246/2752221

int QtSLiMScriptTextEdit::lineNumberAreaWidth()
{
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    
    if (!prefsNotifier.showLineNumbersPref())
        return 0;
    
    int digits = 1;
    int max = qMax(1, document()->blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    //int space = 13 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;   // added in Qt 5.11
    int space = 13 + fontMetrics().width("9") * digits;                 // deprecated (in 5.11, I assume)
    
    return space;
}

void QtSLiMScriptTextEdit::displayFontPrefChanged()
{
    QtSLiMTextEdit::displayFontPrefChanged();
    updateLineNumberArea();
}

void QtSLiMScriptTextEdit::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void QtSLiMScriptTextEdit::updateLineNumberArea(void)
{
    QRect rect = contentsRect();
    lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
    updateLineNumberAreaWidth(0);
    
    int dy = verticalScrollBar()->sliderPosition();
    if (dy > -1)
        lineNumberArea->scroll(0, dy);
}

void QtSLiMScriptTextEdit::resizeEvent(QResizeEvent *e)
{
    QTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

// standard blue highlight
static QColor lineHighlightColor = QtSLiMColorWithHSV(3.6/6.0, 0.1, 1.0, 1.0);

// alternate yellow highlight
//static QColor lineHighlightColor = QtSLiMColorWithHSV(1/6.0, 0.2, 1.0, 1.0);

void QtSLiMScriptTextEdit::highlightCurrentLine()
{
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    QList<QTextEdit::ExtraSelection> extraSelections;
    
    if (!isReadOnly() && prefsNotifier.highlightCurrentLinePref()) {
        QTextEdit::ExtraSelection selection;
        
        selection.format.setBackground(lineHighlightColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }
    
    setExtraSelections(extraSelections);
}

// standard light appearance
static QColor lineAreaBackground = QtSLiMColorWithWhite(0.92, 1.0);
static QColor lineAreaNumber = QtSLiMColorWithWhite(0.75, 1.0);
static QColor lineAreaNumber_Current = QtSLiMColorWithWhite(0.4, 1.0);

// alternate darker appearance
//static QColor lineAreaBackground = Qt::lightGray;
//static QColor lineAreaNumber = QtSLiMColorWithHSV(0.0, 0.0, 0.4, 1.0);
//static QColor lineAreaNumber_Current = QtSLiMColorWithHSV(1/6.0, 0.7, 1.0, 1.0);

void QtSLiMScriptTextEdit::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    if (contentsRect().width() <= 0)
        return;
    
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), lineAreaBackground);
    int blockNumber = 0;
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    int cursorBlockNumber = textCursor().blockNumber();

    int translate_y = -verticalScrollBar()->sliderPosition();
    
    // Draw the numbers (displaying the current line number in col_1)
    painter.setPen(lineAreaNumber);
    
    while (block.isValid())
    {
        if (block.isVisible())
        {
            if (cursorBlockNumber == blockNumber) painter.setPen(lineAreaNumber_Current);
            
            QRectF blockBounds = this->document()->documentLayout()->blockBoundingRect(block);
            
            QString number = QString::number(blockNumber + 1);
            painter.drawText(-7, blockBounds.top() + translate_y, lineNumberArea->width(), fontMetrics().height(), Qt::AlignRight, number);
            
            if (cursorBlockNumber == blockNumber) painter.setPen(lineAreaNumber);
        }

        block = block.next();
        ++blockNumber;
    }
}































