//
//  QtSLiMScriptTextEdit.cpp
//  SLiM
//
//  Created by Ben Haller on 11/24/2019.
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


#include "QtSLiMScriptTextEdit.h"

#include <QApplication>
#include <QGuiApplication>
#include <QTextCursor>
#include <QPaintEvent>
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
#include <QMenu>
#include <QToolTip>
#include <QDebug>

#include <utility>
#include <memory>
#include <string>
#include <algorithm>
#include <vector>

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
#include "eidos_interpreter.h"
#include "interaction_type.h"
#include "eidos_sorting.h"


//
//  QtSLiMTextEdit
//

QtSLiMTextEdit::QtSLiMTextEdit(const QString &text, QWidget *p_parent) : QPlainTextEdit(text, p_parent)
{
    selfInit();
}

QtSLiMTextEdit::QtSLiMTextEdit(QWidget *p_parent) : QPlainTextEdit(p_parent)
{
    selfInit();
}

void QtSLiMTextEdit::selfInit(void)
{
    // track changes to undo/redo availability
    connect(this, &QPlainTextEdit::undoAvailable, this, [this](bool b) { undoAvailable_ = b; });
    connect(this, &QPlainTextEdit::redoAvailable, this, [this](bool b) { redoAvailable_ = b; });
    connect(this, &QPlainTextEdit::copyAvailable, this, [this](bool b) { copyAvailable_ = b; });
    
    // clear the custom error background color whenever the selection changes
    connect(this, &QPlainTextEdit::selectionChanged, this, [this]() { setPalette(qtslimStandardPalette()); });
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this]() { setPalette(qtslimStandardPalette()); });
    
    // because we mess with the palette, we have to reset it on dark mode changes; this resets the error
    // highlighting if it is set up, which is a bug, but not one worth worrying about I suppose...
    connect(qtSLiMAppDelegate, &QtSLiMAppDelegate::applicationPaletteChanged, this, [this]() { setPalette(qtslimStandardPalette()); });
    
    // clear the status bar on a selection change
    connect(this, &QPlainTextEdit::selectionChanged, this, &QtSLiMTextEdit::updateStatusFieldFromSelection);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &QtSLiMTextEdit::updateStatusFieldFromSelection);
    
    // Wire up to change the font when the display font pref changes
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::displayFontPrefChanged, this, &QtSLiMTextEdit::displayFontPrefChanged);
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::scriptSyntaxHighlightPrefChanged, this, &QtSLiMTextEdit::scriptSyntaxHighlightPrefChanged);
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::outputSyntaxHighlightPrefChanged, this, &QtSLiMTextEdit::outputSyntaxHighlightPrefChanged);
    
    // Get notified of modifier key changes, so we can change our cursor
    connect(qtSLiMAppDelegate, &QtSLiMAppDelegate::modifiersChanged, this, &QtSLiMTextEdit::modifiersChanged);
    
    // set up tab stops based on the display font
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    double tabWidth = 0;
    QFont scriptFont = prefs.displayFontPref(&tabWidth);
    
    setFont(scriptFont);
#if (QT_VERSION < QT_VERSION_CHECK(5, 10, 0))
    setTabStopWidth((int)floor(tabWidth));      // deprecated in 5.10
#else
    setTabStopDistance(tabWidth);               // added in 5.10
#endif
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
    double tabWidth = 0;
    QFont displayFont = prefs.displayFontPref(&tabWidth);
    
    setFont(displayFont);
#if (QT_VERSION < QT_VERSION_CHECK(5, 10, 0))
    setTabStopWidth((int)floor(tabWidth));      // deprecated in 5.10
#else
    setTabStopDistance(tabWidth);               // added in 5.10
#endif
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
    QTextCursor highlight_cursor(document());
    
    highlight_cursor.setPosition(startPosition);
    highlight_cursor.setPosition(endPosition, QTextCursor::KeepAnchor);
    setTextCursor(highlight_cursor);
    centerCursor();
    
    setPalette(qtslimErrorPalette());
    
    // note that this custom selection color is cleared by a connection to QPlainTextEdit::selectionChanged()
}

void QtSLiMTextEdit::selectErrorRange(EidosErrorContext &errorContext)
{
	// If there is error-tracking information set, and the error is not attributed to a runtime script
	// such as a lambda or a callback, then we can highlight the error range
	if (!errorContext.executingRuntimeScript && (errorContext.errorPosition.characterStartOfErrorUTF16 >= 0) && (errorContext.errorPosition.characterEndOfErrorUTF16 >= errorContext.errorPosition.characterStartOfErrorUTF16))
        highlightError(errorContext.errorPosition.characterStartOfErrorUTF16, errorContext.errorPosition.characterEndOfErrorUTF16 + 1);
}

QPalette QtSLiMTextEdit::qtslimStandardPalette(void)
{
    // Returns the standard palette for QtSLiMTextEdit, which could depend on platform and dark mode
    return qApp->palette(this);
}

QPalette QtSLiMTextEdit::qtslimErrorPalette(void)
{
    // Returns a palette for QtSLiMTextEdit for highlighting errors, which could depend on platform and dark mode
    // Note that this is based on the current palette, and derives only the highlight colors
    QPalette p = palette();
    p.setColor(QPalette::Highlight, QColor(QColor(Qt::red).lighter(120)));
    p.setColor(QPalette::HighlightedText, QColor(Qt::black));
    return p;
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
	// Note this does *not* check out scriptString, which represents the state of the script when the Community object was created
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
            EidosScript script(cstr, -1);
            
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
            
            EidosErrorContext errorContext = gEidosErrorContext;
            
            gEidosErrorContext = EidosErrorContext{{-1, -1, -1, -1}, nullptr, false};
            
            selectErrorRange(errorContext);
            
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

void QtSLiMTextEdit::_prettyprint_reformat(bool p_reformat)
{
    if (isEnabled())
	{
		if (checkScriptSuppressSuccessResponse(true))
		{
			// We know the script is syntactically correct, so we can tokenize and parse it without worries
            QString currentScriptString = toPlainText();
            QByteArray utf8bytes = currentScriptString.toUtf8();
            const char *cstr = utf8bytes.constData();
			EidosScript script(cstr, -1);
            
			script.Tokenize(false, true);	// get whitespace and comment tokens
			
			// Then generate a new script string that is prettyprinted
			const std::vector<EidosToken> &tokens = script.Tokens();
			std::string pretty;
			bool success = false;
            
            if (p_reformat)
                success = Eidos_reformatTokensFromScript(tokens, script, pretty);
            else
                success = Eidos_prettyprintTokensFromScript(tokens, script, pretty);
            
            if (success)
            {
                // We want to replace our text in a way that is undoable; to do this, we use the text cursor
                QString replacementString = QString::fromStdString(pretty);
                
                QTextCursor &&replacement_cursor = textCursor();
                
                replacement_cursor.beginEditBlock();
                replacement_cursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
                replacement_cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                replacement_cursor.insertText(replacementString);
                replacement_cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
                replacement_cursor.endEditBlock();
                setTextCursor(replacement_cursor);
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
    else if (searchString == "first")			searchString = "Eidos events";
    else if (searchString == "early")			searchString = "Eidos events";
	else if (searchString == "late")			searchString = "Eidos events";
	else if (searchString == "mutationEffect")  searchString = "mutationEffect() callbacks";
	else if (searchString == "fitnessEffect")   searchString = "fitnessEffect() callbacks";
	else if (searchString == "interaction")     searchString = "interaction() callbacks";
	else if (searchString == "mateChoice")      searchString = "mateChoice() callbacks";
	else if (searchString == "modifyChild")     searchString = "modifyChild() callbacks";
	else if (searchString == "recombination")	searchString = "recombination() callbacks";
	else if (searchString == "mutation")		searchString = "mutation() callbacks";
	else if (searchString == "survival")		searchString = "survival() callbacks";
	else if (searchString == "reproduction")	searchString = "reproduction() callbacks";
    
    // now send the search string on to the help window
    helpWindow.enterSearchForString(searchString, true);
}

void QtSLiMTextEdit::mousePressEvent(QMouseEvent *p_event)
{
    bool optionPressed = (optionClickEnabled && QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier));
    
    if (optionPressed)
    {
        // option-click gets intercepted to bring up help
        optionClickIntercepted = true;
        
        // get the position of the character clicked on; note that cursorForPosition()
        // returns the closest cursor position *between* characters, not which character
        // was actually clicked on, so we try to compensate here by fudging the position
        // leftward by half a character width; we used to use hitTest() for this purpose,
        // but for QPlainTextEdit hitTest() always returns -1 for some reason.
        const QFont &displayFont = QtSLiMPreferencesNotifier::instance().displayFontPref(nullptr);
        QFontMetricsF fm(displayFont);
        int fudgeFactor;
        
#if (QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
        fudgeFactor = std::round(fm.width(" ") / 2.0) + 1;                // deprecated in 5.11
#else
        fudgeFactor = std::round(fm.horizontalAdvance(" ") / 2.0) + 1;    // added in Qt 5.11
#endif
        
        QPoint localPos = p_event->localPos().toPoint();
        QPoint fudgedPoint(std::max(0, localPos.x() - fudgeFactor), localPos.y());
        int characterPositionClicked = cursorForPosition(fudgedPoint).position();
        
        //qDebug() << "localPos ==" << localPos << ", characterPositionClicked ==" << characterPositionClicked;
        
        if (characterPositionClicked == -1)     // occurs if you click between lines of text
            return;
        
        QTextCursor charCursor(document());
        charCursor.setPosition(characterPositionClicked, QTextCursor::MoveAnchor);
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
            // start at the anchor and find the encompassing word
            symbolCursor.setPosition(symbolCursor.anchor(), QTextCursor::MoveAnchor);
            symbolCursor.movePosition(QTextCursor::StartOfWord, QTextCursor::MoveAnchor);
            symbolCursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
        }
        else if ((character == '/') || (character == '=') || (character == '<') || (character == '>') || (character == '!'))
        {
            // the character clicked might be part of a multicharacter symbol: // == <= >= !=
            // we will look at two-character groups anchored in the clicked character to test this
            QTextCursor leftPairCursor(document()), rightPairCursor(document());
            leftPairCursor.setPosition(characterPositionClicked - 1, QTextCursor::MoveAnchor);
            leftPairCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 2);
            rightPairCursor.setPosition(characterPositionClicked, QTextCursor::MoveAnchor);
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
        
        QPlainTextEdit::mousePressEvent(p_event);
    }
}

void QtSLiMTextEdit::mouseMoveEvent(QMouseEvent *p_event)
{
    // forward to super, as long as we did not intercept this mouse event
    if (!optionClickIntercepted)
        QPlainTextEdit::mouseMoveEvent(p_event);
}

void QtSLiMTextEdit::mouseReleaseEvent(QMouseEvent *p_event)
{
    // forward to super, as long as we did not intercept this mouse event
    if (!optionClickIntercepted)
        QPlainTextEdit::mouseReleaseEvent(p_event);
    
    optionClickIntercepted = false;
}

void QtSLiMTextEdit::fixMouseCursor(void)
{
    if (optionClickEnabled)
    {
        // we want a pointing hand cursor when option is pressed; if the cursor is wrong, fix it
        // note the cursor for QPlainTextEdit is apparently controlled by its viewport
        bool optionPressed = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier);
        QWidget *vp = viewport();
        
        if (optionPressed && (vp->cursor().shape() != Qt::PointingHandCursor))
            vp->setCursor(Qt::PointingHandCursor);
        else if (!optionPressed && (vp->cursor().shape() != Qt::IBeamCursor))
            vp->setCursor(Qt::IBeamCursor);
    }
}

void QtSLiMTextEdit::enterEvent(QEvent *p_event)
{
    // forward to super
    QPlainTextEdit::enterEvent(p_event);
    
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

EidosMethodSignature_CSP QtSLiMTextEdit::signatureForMethodName(QString callName)
{
	std::string call_name = callName.toStdString();
	
    // Look for a matching method signature for the call name.
	const std::vector<EidosMethodSignature_CSP> methodSignatures = EidosClass::RegisteredClassMethods(true, true);
	
	for (const EidosMethodSignature_CSP &sig : methodSignatures)
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
	EidosScript script(script_string, -1);
	
	// Tokenize
	script.Tokenize(true, false);	// make bad tokens as needed, don't keep nonsignificant tokens
	
	return functionMapForTokenizedScript(script, includingOptionalFunctions);
}

EidosFunctionMap *QtSLiMTextEdit::functionMapForTokenizedScript(EidosScript &script, bool includingOptionalFunctions)
{
    // This lower-level function takes a tokenized script object and works from there, allowing reuse of work
    // in the case of attributedSignatureForScriptString:...
    QtSLiMWindow *windowSLiMController = slimControllerForWindow();
    Community *community = (windowSLiMController ? windowSLiMController->community : nullptr);
    bool invalidSimulation = (windowSLiMController ? windowSLiMController->invalidSimulation() : true);
    
    // start with all the functions that are available in the current simulation context
    EidosFunctionMap *functionMapPtr = nullptr;
    
    if (community && !invalidSimulation)
        functionMapPtr = new EidosFunctionMap(community->FunctionMap());
    else
        functionMapPtr = new EidosFunctionMap(*EidosInterpreter::BuiltInFunctionMap());
    
    // functionMapForEidosTextView: returns the function map for the current interpreter state, and the type-interpreter
    // stuff we do below gives the delegate no chance to intervene (note that SLiMTypeInterpreter does not get in here,
    // unlike in the code completion machinery!).  But sometimes we want SLiM's zero-gen functions to be added to the map
    // in all cases; it would be even better to be smart the way code completion is, but that's more work than it's worth.
    if (includingOptionalFunctions)
    {
        // add SLiM functions that are context-dependent
        Community::AddZeroTickFunctionsToMap(*functionMapPtr);
        Community::AddSLiMFunctionsToMap(*functionMapPtr);
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

void QtSLiMTextEdit::scriptStringAndSelection(QString &scriptString, int &position, int &length, int &offset)
{
    // by default, the entire contents of the textedit are considered "script"
    scriptString = toPlainText();
    
    QTextCursor selection_cursor(textCursor());
    position = selection_cursor.selectionStart();
    length = selection_cursor.selectionEnd() - position;
    offset = 0;
}

EidosCallSignature_CSP QtSLiMTextEdit::signatureForScriptSelection(QString &callName)
{
    // Note we return a copy of the signature, owned by the caller
    QString scriptString;
    int selectionStart, selectionLength, rangeOffset;
    
    scriptStringAndSelection(scriptString, selectionStart, selectionLength, rangeOffset);
    
    if (scriptString.length())
	{
		std::string script_string = scriptString.toStdString();
		EidosScript script(script_string, -1);
		
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
            else if (callName == "first")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("first", nullptr, kEidosValueMaskVOID)));
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
            else if (callName == "mutationEffect")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("mutationEffect", nullptr, kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddObject_S("mutationType", gSLiM_MutationType_Class)->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
                signature = callbackSig;
            }
            else if (callName == "fitnessEffect")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("fitnessEffect", nullptr, kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
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
            else if (callName == "survival")
            {
                static EidosCallSignature_CSP callbackSig = nullptr;
                if (!callbackSig) callbackSig = EidosCallSignature_CSP((new EidosFunctionSignature("survival", nullptr, kEidosValueMaskNULL | kEidosValueMaskLogical | kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Subpopulation_Class))->AddObject_OS("subpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULLInvisible));
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
            
            if (signature)
            {
                QTextCursor tc(&td);
                tc.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
                tc.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                
#ifdef __linux__
                ColorizeCallSignature(signature.get(), 9, tc);
#else
                ColorizeCallSignature(signature.get(), 11, tc);
#endif
            }
            
            // hanging indent for multiline wrapping aesthetics
            {
                QTextCursor tc(&td);
                tc.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
                tc.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                
                QTextBlockFormat blockFormat;
                blockFormat.setLeftMargin(30);
                blockFormat.setTextIndent(-30);
                
                tc.setBlockFormat(blockFormat);
            }
            
            QString htmlString = td.toHtml();
            //qDebug() << "setting HTML:" << htmlString;
            
            statusBar->showMessage(htmlString);
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
        
        connect(completer, QOverload<const QString &>::of(&QCompleter::activated), this, &QtSLiMTextEdit::insertCompletion);    // note this activated() signal was deprecated in 5.15, use QComboBox::textActivated() which was added in 5.14
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
        
        // If the character after the completion range is '(', suppress trailing parentheses on the completion
        QTextCursor afterCompletion(tc);
        afterCompletion.setPosition(afterCompletion.position(), QTextCursor::MoveAnchor);
        afterCompletion.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, 1);
        
        if (afterCompletion.selectedText() == '(')
        {
            if (completion.endsWith("()"))
                completion.chop(2);
        }
        
        // Replace the completion range with the completion string
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
            previousLine.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
            
            QString lineString = previousLine.selectedText();
            QString whitespace;
            
            for (int position = 0; position < lineString.length(); ++position)
            {
                QChar qch = lineString[position];
                
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

void QtSLiMTextEdit::keyPressEvent(QKeyEvent *p_event)
{
    // Without a completer, we just call super
    if (!completer)
    {
        QPlainTextEdit::keyPressEvent(p_event);
        return;
    }
    
    if (completer->popup()->isVisible()) {
        // The following keys are forwarded by the completer to the widget
       switch (p_event->key()) {
       case Qt::Key_Enter:
       case Qt::Key_Return:
       case Qt::Key_Escape:
       case Qt::Key_Tab:
       case Qt::Key_Backtab:
            p_event->ignore();
            return; // let the completer do default behavior
       default:
           break;
       }
    }
    
    // if we have a visible completer popup, the key pressed is not one of the special keys above (including escape)
    // our completion key shortcut is the escape key, so check for that now
    bool isShortcut = ((p_event->modifiers() == Qt::NoModifier) && p_event->key() == Qt::Key_Escape); // escape
    
    if (!isShortcut)
    {
        // any key other than escape and the special keys above causes the completion popup to hide
        completer->popup()->hide();
        QPlainTextEdit::keyPressEvent(p_event);
        
        // implement autoindent
        if ((p_event->modifiers() == Qt::NoModifier) && ((p_event->key() == Qt::Key_Enter) || (p_event->key() == Qt::Key_Return)))
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
	EidosGlobalStringID identifier_ID = EidosStringRegistry::GlobalStringIDForString(identifier_name);
	bool identifier_is_call = identifiers_are_calls[static_cast<size_t>(key_path_index)];
	const EidosClass *key_path_class = nullptr;
	
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
		
		EidosGlobalStringID identifier_id = EidosStringRegistry::GlobalStringIDForString(identifier_name);
		
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
	const EidosClass *terminus = key_path_class;
	
	// First, a sorted list of globals
	for (const auto &symbol_sig : *terminus->Properties())
    {
        if (!symbol_sig->deprecated_)
            candidates << QString::fromStdString(symbol_sig->property_name_);
	}
    
	candidates.sort();
	
	// Next, a sorted list of methods, with () appended
	for (const auto &method_sig : *terminus->Methods())
	{
        if (!method_sig->deprecated_)
        {
            QString methodName = QString::fromStdString(method_sig->call_name_);
            
            methodName.append("()");
            candidates << methodName;
        }
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
	// This is part-based completion, where iTr will complete to initializeTreeSeq() and iGTy
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
			scores.emplace_back(score);
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
        case EidosTokenType::kTokenAssign_R:
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
    
    Community::AddSLiMFunctionsToMap(**functionMap);
    
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
                // skip species/ticks specifiers, which are identifier token nodes at the top level of the AST with one child
                if ((script_block_node->token_->token_type_ == EidosTokenType::kTokenIdentifier) && (script_block_node->children_.size() == 1))
                    continue;
                
                // script_block_node can have various children, such as an sX identifier, start and end ticks, a block type
                // identifier like late(), and then the root node of the compound statement for the script block.  We want to
                // decode the parts that are important to us, without the complication of making SLiMEidosBlock objects.
                EidosASTNode *block_statement_root = nullptr;
                SLiMEidosBlockType block_type = SLiMEidosBlockType::SLiMEidosNoBlockType;
                
                for (EidosASTNode *block_child : script_block_node->children_)
                {
                    EidosToken *child_token = block_child->token_;
                    
                    if (child_token->token_type_ == EidosTokenType::kTokenIdentifier)
                    {
                        const std::string &child_string = child_token->token_string_;
                        
                        if (child_string.compare(gStr_first) == 0)					block_type = SLiMEidosBlockType::SLiMEidosEventFirst;
                        else if (child_string.compare(gStr_early) == 0)         	block_type = SLiMEidosBlockType::SLiMEidosEventEarly;
                        else if (child_string.compare(gStr_late) == 0)				block_type = SLiMEidosBlockType::SLiMEidosEventLate;
                        else if (child_string.compare(gStr_initialize) == 0)		block_type = SLiMEidosBlockType::SLiMEidosInitializeCallback;
                        else if (child_string.compare(gStr_fitnessEffect) == 0)		block_type = SLiMEidosBlockType::SLiMEidosFitnessEffectCallback;
                        else if (child_string.compare(gStr_mutationEffect) == 0)	block_type = SLiMEidosBlockType::SLiMEidosMutationEffectCallback;
                        else if (child_string.compare(gStr_interaction) == 0)		block_type = SLiMEidosBlockType::SLiMEidosInteractionCallback;
                        else if (child_string.compare(gStr_mateChoice) == 0)		block_type = SLiMEidosBlockType::SLiMEidosMateChoiceCallback;
                        else if (child_string.compare(gStr_modifyChild) == 0)		block_type = SLiMEidosBlockType::SLiMEidosModifyChildCallback;
                        else if (child_string.compare(gStr_recombination) == 0)		block_type = SLiMEidosBlockType::SLiMEidosRecombinationCallback;
                        else if (child_string.compare(gStr_mutation) == 0)			block_type = SLiMEidosBlockType::SLiMEidosMutationCallback;
                        else if (child_string.compare(gStr_survival) == 0)			block_type = SLiMEidosBlockType::SLiMEidosSurvivalCallback;
                        else if (child_string.compare(gStr_reproduction) == 0)		block_type = SLiMEidosBlockType::SLiMEidosReproductionCallback;
                        
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
                                    EidosGlobalStringID constant_id = EidosStringRegistry::GlobalStringIDForString(child_string);
                                    
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
                    // The species/community symbols are  defined in all blocks except initialize() blocks; we need to add
                    // and remove them dynamically so that each block has it defined or not defined as necessary.  Since
                    // the completion block is last, the symbols will be correctly defined at the end of this process.
                    if (block_type == SLiMEidosBlockType::SLiMEidosInitializeCallback)
                    {
                        std::vector<EidosGlobalStringID> symbol_ids = (*typeTable)->AllSymbolIDs();
                        
                        for (EidosGlobalStringID symbol_id : symbol_ids)
                        {
                            EidosTypeSpecifier typeSpec = (*typeTable)->GetTypeForSymbol(symbol_id);
                            
                            if ((typeSpec.type_mask == kEidosValueMaskObject) && ((typeSpec.object_class == gSLiM_Community_Class) || (typeSpec.object_class == gSLiM_Species_Class)))
                                (*typeTable)->RemoveTypeForSymbol(symbol_id);
                        }
                    }
                    else
                    {
                        Community *community = (windowSLiMController ? windowSLiMController->community : nullptr);
                        
                        (*typeTable)->SetTypeForSymbol(gID_community, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Community_Class});
                        
                        if (community)
                        {
                            for (Species *species : community->AllSpecies())
                            {
                                EidosGlobalStringID species_symbol = species->self_symbol_.first;
                                
                                (*typeTable)->SetTypeForSymbol(species_symbol, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Species_Class});
                            }
                        }
                        else
                        {
                            // We don't have a community object, so we don't have a vector of species; this is usually because of a failed parse
                            // In this case, we try to keep things functional by just assuming the single-species case and defining "sim"
                            (*typeTable)->SetTypeForSymbol(gID_sim, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Species_Class});
                        }
                    }
                    
                    // The slimgui symbol is always available within a block, but not at the top level
                    (*typeTable)->SetTypeForSymbol(gID_slimgui, EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_SLiMgui_Class});
                    
                    // Do the same for the zero-tick functions, which should be defined in initialization() blocks and
                    // not in other blocks; we add and remove them dynamically so they are defined as appropriate.  We ought
                    // to do this for other block-specific stuff as well (like the stuff below), but it is unlikely to matter.
                    // Note that we consider the zero-gen functions to always be defined inside function blocks, since the
                    // function might be called from the zero gen (we have no way of knowing definitively).
                    if ((block_type == SLiMEidosBlockType::SLiMEidosInitializeCallback) || (block_type == SLiMEidosBlockType::SLiMEidosUserDefinedFunction))
                        Community::AddZeroTickFunctionsToMap(**functionMap);
                    else
                        Community::RemoveZeroTickFunctionsFromMap(**functionMap);
                    
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
                        case SLiMEidosBlockType::SLiMEidosEventFirst:
                            break;
                        case SLiMEidosBlockType::SLiMEidosEventEarly:
                            break;
                        case SLiMEidosBlockType::SLiMEidosEventLate:
                            break;
                        case SLiMEidosBlockType::SLiMEidosInitializeCallback:
                            (*typeTable)->RemoveSymbolsOfClass(gSLiM_Subpopulation_Class);	// subpops defined upstream from us still do not exist for us
                            break;
                        case SLiMEidosBlockType::SLiMEidosFitnessEffectCallback:
                            (*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
                            break;
                        case SLiMEidosBlockType::SLiMEidosMutationEffectCallback:
                            (*typeTable)->SetTypeForSymbol(gID_mut,				EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Mutation_Class});
                            (*typeTable)->SetTypeForSymbol(gID_homozygous,		EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
                            (*typeTable)->SetTypeForSymbol(gID_effect,			EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
                            (*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
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
                            (*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
                            (*typeTable)->SetTypeForSymbol(gID_sourceSubpop,	EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
                            (*typeTable)->SetTypeForSymbol(gEidosID_weights,	EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
                            break;
                        case SLiMEidosBlockType::SLiMEidosModifyChildCallback:
                            (*typeTable)->SetTypeForSymbol(gID_child,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_parent1,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_isCloning,		EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
                            (*typeTable)->SetTypeForSymbol(gID_isSelfing,		EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
                            (*typeTable)->SetTypeForSymbol(gID_parent2,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
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
                        case SLiMEidosBlockType::SLiMEidosSurvivalCallback:
                            (*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
                            (*typeTable)->SetTypeForSymbol(gID_subpop,			EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Subpopulation_Class});
                            (*typeTable)->SetTypeForSymbol(gID_surviving,       EidosTypeSpecifier{kEidosValueMaskLogical, nullptr});
                            (*typeTable)->SetTypeForSymbol(gID_fitness,			EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
                            (*typeTable)->SetTypeForSymbol(gID_draw,			EidosTypeSpecifier{kEidosValueMaskFloat, nullptr});
                            break;
                        case SLiMEidosBlockType::SLiMEidosReproductionCallback:
                            (*typeTable)->SetTypeForSymbol(gID_individual,		EidosTypeSpecifier{kEidosValueMaskObject, gSLiM_Individual_Class});
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
                                        (*typeTable)->SetTypeForSymbol(EidosStringRegistry::GlobalStringIDForString(param_name), param_type);
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
                        // However, constant-defining calls might use the types of variables, like defineConstant("foo", bar)
                        // where the type of foo comes from the type of bar; so we need to keep track of all symbols even
                        // though they will fall out of scope.  We therefore use a separate local type table with a reference
                        // upward to our main type table.
                        EidosTypeTable scopedTypeTable(**typeTable);
                        
                        // Make a type interpreter and add symbols to our type table using it
                        // We use SLiMTypeInterpreter because we want to pick up definitions of SLiM constants
                        SLiMTypeInterpreter typeInterpreter(block_statement_root, scopedTypeTable, **functionMap, **callTypeTable);
                        
                        typeInterpreter.SetExternalTypeTable(*typeTable);		// defined constants/variables should also go into the global scope
                        typeInterpreter.TypeEvaluateInterpreterBlock();         // result not used
                    }
                }
            }
        }
    }
    
    // We drop through to here if we have a bad or empty script root, or if the final script block (completion_block) didn't
    // have a compound statement (meaning its starting brace has not yet been typed), or if we're completing outside of any
    // existing script block.  In these sorts of cases, we want to return completions for the outer level of a SLiM script.
    // This means that standard Eidos language keywords like "while", "next", etc. are not legal, but SLiM script block
    // keywords like "first", "early", "late", "mutationEffect", "fitnessEffect", "interaction", "mateChoice", "modifyChild",
    // "recombination", "mutation", "survival", and "reproduction" are.  We also add "species" and "ticks" here for
    // multispecies models.
    // Note that the strings here are display strings; they are fixed to contain newlines in insertCompletion()
    keywords->clear();
    (*keywords) << "initialize() { }";
    (*keywords) << "first() { }";
    (*keywords) << "early() { }";
    (*keywords) << "late() { }";
    (*keywords) << "mutationEffect() { }";
    (*keywords) << "fitnessEffect() { }";
    (*keywords) << "interaction() { }";
    (*keywords) << "mateChoice() { }";
    (*keywords) << "modifyChild() { }";
    (*keywords) << "recombination() { }";
    (*keywords) << "mutation() { }";
    (*keywords) << "survival() { }";
    (*keywords) << "reproduction() { }";
    (*keywords) << "function (void)name(void) { }";
    (*keywords) << "species";
    (*keywords) << "ticks";
    
    // At the outer level, functions are also not legal
    (*functionMap)->clear();
    
    // And no variables exist except SLiM objects like pX, gX, mX, sX and species symbols
    std::vector<EidosGlobalStringID> symbol_ids = (*typeTable)->AllSymbolIDs();
    
    for (EidosGlobalStringID symbol_id : symbol_ids)
    {
        EidosTypeSpecifier typeSpec = (*typeTable)->GetTypeForSymbol(symbol_id);
        
        if ((typeSpec.type_mask != kEidosValueMaskObject) || (typeSpec.object_class == gSLiM_Community_Class) || (typeSpec.object_class == gSLiM_SLiMgui_Class))
            (*typeTable)->RemoveTypeForSymbol(symbol_id);
    }
}

//- (void)_completionHandlerWithRangeForCompletion:(NSRange *)baseRange completions:(NSArray **)completions
void QtSLiMTextEdit::_completionHandlerWithRangeForCompletion(NSRange *baseRange, QStringList *completions)
{
    QString scriptString;
    int selectionStart, selectionLength, rangeOffset;
    
    scriptStringAndSelection(scriptString, selectionStart, selectionLength, rangeOffset);

    NSRange selection = {selectionStart, selectionLength};	// ignore charRange and work from the selection
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
			EidosScript script(script_string, -1);
			
			script.Tokenize(true, false);					// make bad tokens as needed, do not keep nonsignificant tokens
			script.ParseInterpreterBlockToAST(true, true);	// make bad nodes as needed (i.e. never raise, and produce a correct tree)
			
			EidosTypeInterpreter typeInterpreter(script, *typeTablePtr, *functionMapPtr, *callTypeTablePtr);
			
			typeInterpreter.TypeEvaluateInterpreterBlock_AddArgumentCompletions(&argumentCompletions, script_string.length());	// result not used
		}
		
		// Tokenize; we can't use the tokenization done above, as we want whitespace tokens here...
		EidosScript script(script_string, -1);
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
    LineNumberArea(QtSLiMScriptTextEdit *editor);

    virtual QSize sizeHint() const override { return QSize(codeEditor->lineNumberAreaWidth(), 0); }

protected:
    virtual bool event(QEvent *p_event) override;
    virtual void paintEvent(QPaintEvent *p_paintEvent) override { codeEditor->lineNumberAreaPaintEvent(p_paintEvent); }
    virtual void mousePressEvent(QMouseEvent *p_mouseEvent) override { codeEditor->lineNumberAreaMouseEvent(p_mouseEvent); }
    virtual void contextMenuEvent(QContextMenuEvent *p_event) override { codeEditor->lineNumberAreaContextMenuEvent(p_event); }
    virtual void wheelEvent(QWheelEvent *p_wheelEvent) override { codeEditor->lineNumberAreaWheelEvent(p_wheelEvent); }

private:
    QtSLiMScriptTextEdit *codeEditor;
};

LineNumberArea::LineNumberArea(QtSLiMScriptTextEdit *editor) : QWidget(editor), codeEditor(editor)
{
    setMouseTracking(true);     // for live tooltip updating (debug point gutter vs. line numbers)
}

bool LineNumberArea::event(QEvent *p_event)
{
    if (p_event->type() == QEvent::ToolTip) {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(p_event);
        codeEditor->lineNumberAreaToolTipEvent(helpEvent); 
        return true;
    }
    return QWidget::event(p_event);
}


//
//  QtSLiMScriptTextEdit
//

QtSLiMScriptTextEdit::QtSLiMScriptTextEdit(const QString &text, QWidget *p_parent) : QtSLiMTextEdit(text, p_parent)
{
    sharedInit();
}

QtSLiMScriptTextEdit::QtSLiMScriptTextEdit(QWidget *p_parent) : QtSLiMTextEdit(p_parent)
{
    sharedInit();
}

void QtSLiMScriptTextEdit::sharedInit(void)
{
    setCenterOnScroll(true);
    
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
    connect(qtSLiMAppDelegate, &QtSLiMAppDelegate::applicationPaletteChanged, this, &QtSLiMScriptTextEdit::highlightCurrentLine);
    
    // Watch prefs for line numbering and highlighting
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::showLineNumbersPrefChanged, this, [this]() { updateLineNumberArea(); });
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::highlightCurrentLinePrefChanged, this, [this]() { highlightCurrentLine(); });
    
    updateLineNumberAreaWidth(0);
    highlightCurrentLine();
    
    // We now set up to maintain our debugging icons here too
    connect(this, SIGNAL(textChanged()), this, SLOT(updateDebugPoints()));
    
    // Watch for changes to the controller's change count, so we can disable debug points when the document needs recycling
    // Also watch for the end of model initialization; species colors may have changed, necessitating an update
    QtSLiMWindow *controller = slimControllerForWindow();
    
    if (controller)
    {
        connect(controller, &QtSLiMWindow::controllerChangeCountChanged, this, &QtSLiMScriptTextEdit::controllerChangeCountChanged);
        connect(controller, &QtSLiMWindow::controllerTickFinished, this, &QtSLiMScriptTextEdit::controllerTickFinished);
    }
}

QtSLiMScriptTextEdit::~QtSLiMScriptTextEdit()
{
}

QStringList QtSLiMScriptTextEdit::linesForRoundedSelection(QTextCursor &p_cursor, bool &movedBack)
{
    // find the start and end of the blocks we're operating on
    int anchor = p_cursor.anchor(), position = p_cursor.position();
    if (anchor > position)
        std::swap(anchor, position);
    movedBack = false;
    
    QTextCursor startBlockCursor(p_cursor);
    startBlockCursor.setPosition(anchor, QTextCursor::MoveAnchor);
    startBlockCursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    QTextCursor endBlockCursor(p_cursor);
    endBlockCursor.setPosition(position, QTextCursor::MoveAnchor);
    if (endBlockCursor.atBlockStart() && (position > anchor))
    {
        // the selection includes the newline at the end of the last line; we need to move backward to avoid swallowing the following line
        endBlockCursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::MoveAnchor);
        movedBack = true;
    }
    endBlockCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
    
    // select the whole lines we're operating on
    p_cursor.beginEditBlock();
    p_cursor.setPosition(startBlockCursor.position(), QTextCursor::MoveAnchor);
    p_cursor.setPosition(endBlockCursor.position(), QTextCursor::KeepAnchor);
    
    // separate the lines, remove a tab at the start of each, and rejoin them
    QString selectedString = p_cursor.selectedText();
    static const QRegularExpression lineEndMatch("\\R", QRegularExpression::UseUnicodePropertiesOption);
    
#if (QT_VERSION < QT_VERSION_CHECK(5, 14, 0))
    return selectedString.split(lineEndMatch, QString::KeepEmptyParts);     // deprecated in 5.14
#else
    return selectedString.split(lineEndMatch, Qt::KeepEmptyParts);          // added in 5.14
#endif
}

void QtSLiMScriptTextEdit::shiftSelectionLeft(void)
{
     if (isEnabled() && !isReadOnly())
	{
        QTextCursor &&edit_cursor = textCursor();
        bool movedBack;
        QStringList lines = linesForRoundedSelection(edit_cursor, movedBack);
        
        for (QString &line : lines)
            if (line.length() && (line[0] == '\t'))
                line.remove(0, 1);
        
        QString replacementString = lines.join(QChar::ParagraphSeparator);
		
        // end the editing block, producing one undo-able operation
        edit_cursor.insertText(replacementString);
        edit_cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor, replacementString.length());
        edit_cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, replacementString.length());
        if (movedBack)
            edit_cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
		edit_cursor.endEditBlock();
        setTextCursor(edit_cursor);
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
        QTextCursor &&edit_cursor = textCursor();
        bool movedBack;
        QStringList lines = linesForRoundedSelection(edit_cursor, movedBack);
        
        for (QString &line : lines)
            line.insert(0, '\t');
        
        QString replacementString = lines.join(QChar::ParagraphSeparator);
		
        // end the editing block, producing one undo-able operation
        edit_cursor.insertText(replacementString);
        edit_cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor, replacementString.length());
        edit_cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, replacementString.length());
        if (movedBack)
            edit_cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
		edit_cursor.endEditBlock();
        setTextCursor(edit_cursor);
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
        QTextCursor &&edit_cursor = textCursor();
        bool movedBack;
        QStringList lines = linesForRoundedSelection(edit_cursor, movedBack);
        
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
        edit_cursor.insertText(replacementString);
        edit_cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::MoveAnchor, replacementString.length());
        edit_cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, replacementString.length());
        if (movedBack)
            edit_cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
		edit_cursor.endEditBlock();
        setTextCursor(edit_cursor);
	}
	else
	{
		qApp->beep();
	}
}

// From here down is the machinery for providing line numbers with LineNumberArea
// This code is adapted from https://doc.qt.io/qt-5/qtwidgets-widgets-codeeditor-example.html

int QtSLiMScriptTextEdit::lineNumberAreaWidth()
{
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    
    // We now show debugging icons in the line number area too, since they are kept by line number
    // The line number area therefore no longer goes down to width 0 when the pref is disabled
    if (scriptType == SLiMScriptType)
    {
#if (QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
        lineNumberAreaBugWidth = 3 + fontMetrics().width("9") * 2;                 // deprecated in 5.11
#else
        lineNumberAreaBugWidth = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * 2;   // added in Qt 5.11
#endif
    }
    else
    {
        lineNumberAreaBugWidth = 0;
    }
    
    if (!prefsNotifier.showLineNumbersPref())
        return lineNumberAreaBugWidth;
    
    int digits = 1;
    int max = qMax(1, document()->blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

#if (QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
    int space = 13 + fontMetrics().width("9") * digits;                 // deprecated in 5.11
#else
    int space = 13 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;   // added in Qt 5.11
#endif
    
    return lineNumberAreaBugWidth + space;
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
    QRect contents_rect = contentsRect();
    lineNumberArea->update(0, contents_rect.y(), lineNumberArea->width(), contents_rect.height());
    updateLineNumberAreaWidth(0);
    
    int dy = verticalScrollBar()->sliderPosition();
    if (dy > -1)
        lineNumberArea->scroll(0, dy);
}

void QtSLiMScriptTextEdit::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void QtSLiMScriptTextEdit::toggleDebuggingForLine(int lineNumber)
{
    if (scriptType != SLiMScriptType)
        return;
    
    // First figure out whether we have a debugging point at the line in question
    bool hasExistingCursor = false;
    
    //qDebug() << "toggleDebuggingForLine():" << lineNumber;
    //qDebug() << "block contents:" << document()->findBlockByNumber(lineNumber).text();
    
    for (int cursorIndex = 0; cursorIndex < (int)bugCursors.size(); cursorIndex++)
    {
        QTextCursor &existingCursor = bugCursors[cursorIndex];
        QTextBlock cursorBlock = existingCursor.block();
        int blockNumber = cursorBlock.blockNumber();
        
        if (blockNumber == lineNumber)
        {
            hasExistingCursor = true;
            bugCursors.erase(bugCursors.begin() + cursorIndex);
            --cursorIndex;
        }
    }
    
    if (hasExistingCursor)
    {
        // This line number had a debugging point; we cleared it above
    }
    else
    {
        // This line number does not currently have a debugging point; add one
        QTextDocument *doc = document();
        QTextBlock block = doc->findBlockByNumber(lineNumber);
        QString blockText = block.text();
        
        // Find the first non-whitespace character in the block; the debug point starts at that character
        int firstNonWhitespace = 0;
        
        for (firstNonWhitespace = 0; firstNonWhitespace < blockText.length(); ++firstNonWhitespace)
        {
            QChar qch = blockText[firstNonWhitespace];
            
            if ((qch != " ") && (qch != "\t"))
                break;
        }
        
        // If the block contains nothing but whitespace, decline to set a debugging point
        // This kind of makes sense semantically, and in any case if it's an empty line there's no text to put our cursor on
        if (firstNonWhitespace == blockText.length())
            return;
        
        // Make a text cursor encompassing the remainder of the block, from the first non-whitespace character
        QTextCursor tc(block);
        tc.movePosition(QTextCursor::NextCharacter, QTextCursor::MoveAnchor, firstNonWhitespace);
        tc.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor, 1);
        
        // Remember the cursor as a debug point
        bugCursors.emplace_back(tc);
    }
    
    // Since the cursors changed, we need to recache our line numbers
    updateDebugPoints();
}

void QtSLiMScriptTextEdit::updateDebugPoints(void)
{
    // prevent re-entrancy
    if (coloringDebugPointCursors)
        return;
    
    //qDebug() << "updateDebugPoints():" << bugCursors.size() << "debug cursors exist";
    
    // Generate a new bugLines vector from our text cursors; we vet the cursors at the same time,
    // and remove any cursors that are zero-length, or that are now duplicates
    EidosInterpreterDebugPointsSet newBugLines;
    int bugCursorCount = (int)bugCursors.size();
    
    for (int bugCursorIndex = 0; bugCursorIndex < bugCursorCount; ++bugCursorIndex)
    {
        QTextCursor &bugCursor = bugCursors[bugCursorIndex];
        bool zeroLength = !bugCursor.hasSelection();
        
        // Fix cursors to encompass their block; they can get out of whack due to typing
        // This is probably usually unnecessary, but it's a trivial amount of work
        if (!zeroLength)
        {
            bugCursor.setPosition(bugCursor.selectionStart(), QTextCursor::MoveAnchor);
            bugCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor, 1);
        }
        
        QTextBlock cursorBlock = bugCursor.block();
        int blockNumber = cursorBlock.blockNumber();
        
        if (!zeroLength && (newBugLines.set.find(blockNumber) == newBugLines.set.end()))
        {
            // this line is not already marked as debug; mark it now
            newBugLines.set.emplace(blockNumber);
            continue;
        }
        
        // discard a redundant or zero-length cursor; this generally results from lines being
        // merged together in the editor (redundant), or being deleted (zero-length)
        bugCursors.erase(bugCursors.begin() + bugCursorIndex);
        bugCursorIndex--;
        bugCursorCount--;
    }
    
    // Set a temporary color on our cursors for debugging
#if 0
    {
        coloringDebugPointCursors = true;
        
        QTextCharFormat clearFormat;
        QTextCharFormat highlightFormat;
        
        clearFormat.setBackground(QBrush(QColor(255, 255, 255)));
        highlightFormat.setBackground(QBrush(QColor(220, 255, 220)));
        
        QTextCursor tc(document());
        tc.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
        tc.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        
        tc.setCharFormat(clearFormat);
        
        for (QTextCursor &bugCursor : bugCursors)
            bugCursor.setCharFormat(highlightFormat);
        
        coloringDebugPointCursors = false;
    }
#endif
    
    // If bugLines changed, we need to recache/redraw
    if (newBugLines.set != bugLines.set)
    {
        std::swap(newBugLines.set, bugLines.set);
        
        if (debugPointsEnabled)
            enabledBugLines.set = bugLines.set;
        else
            enabledBugLines.set.clear();
        
        lineNumberArea->update();
    }
    
    //qDebug() << "   updateDebugPoints():" << bugLines.size() << "debug points set";
}

void QtSLiMScriptTextEdit::controllerChangeCountChanged(int changeCount)
{
    if (changeCount == 0)
    {
        // recycled and unedited; debug points enabled
        debugPointsEnabled = true;
        enabledBugLines.set = bugLines.set;
        lineNumberArea->update();
    }
    else
    {
        // edited without a recycle; debug points disabled
        debugPointsEnabled = false;
        enabledBugLines.set.clear();
        lineNumberArea->update();
    }
}

void QtSLiMScriptTextEdit::controllerTickFinished(void)
{
    // If we just finished initialize() callbacks, species colors may have changed
    QtSLiMWindow *controller = slimControllerForWindow();
    
    if (controller && controller->community && (controller->community->Tick() == 1))
        lineNumberArea->update();
}

// light appearance: standard blue highlight
static QColor lineHighlightColor = QtSLiMColorWithHSV(3.6/6.0, 0.1, 1.0, 1.0);

// dark appearance: dark blue highlight
static QColor lineHighlightColor_DARK = QtSLiMColorWithHSV(4.0/6.0, 0.5, 0.30, 1.0);

void QtSLiMScriptTextEdit::highlightCurrentLine()
{
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    QList<QTextEdit::ExtraSelection> extra_selections;
    
    if (!isReadOnly() && prefsNotifier.highlightCurrentLinePref()) {
        bool inDarkMode = QtSLiMInDarkMode();
        QTextEdit::ExtraSelection selection;
        
        selection.format.setBackground(inDarkMode ? lineHighlightColor_DARK : lineHighlightColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extra_selections.append(selection);
    }
    
    setExtraSelections(extra_selections);
}

// light appearance
static QColor bugAreaBackground = QtSLiMColorWithWhite(0.95, 1.0);
static QColor lineAreaBackground = QtSLiMColorWithWhite(0.92, 1.0);
static QColor lineAreaNumber = QtSLiMColorWithWhite(0.75, 1.0);
static QColor lineAreaNumberCurrent = QtSLiMColorWithWhite(0.4, 1.0);

// dark appearance
static QColor bugAreaBackground_DARK = QtSLiMColorWithWhite(0.05, 1.0);
static QColor lineAreaBackground_DARK = QtSLiMColorWithWhite(0.08, 1.0);
static QColor lineAreaNumber_DARK = QtSLiMColorWithWhite(0.25, 1.0);
static QColor lineAreaNumberCurrent_DARK = QtSLiMColorWithWhite(0.6, 1.0);

void QtSLiMScriptTextEdit::lineNumberAreaToolTipEvent(QHelpEvent *p_helpEvent)
{
    // provide different tooltips for the debug point gutter versus the line number area
    QPointF localPos = p_helpEvent->pos();
    
    if ((lineNumberAreaBugWidth == 0) || ((localPos.x() < 0) || (localPos.x() >= lineNumberAreaBugWidth + 2)))     // +2 for a little slop
        QToolTip::showText(p_helpEvent->globalPos(), "<html><head/><body><p>script line numbers</p></body></html>");
    else
        QToolTip::showText(p_helpEvent->globalPos(), "<html><head/><body><p>debug point gutter (click to set/clear a debug point)</p></body></html>");
}

void QtSLiMScriptTextEdit::lineNumberAreaPaintEvent(QPaintEvent *p_paintEvent)
{
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    bool showBlockColors = true;
    bool showLineNumbers = prefsNotifier.showLineNumbersPref();
    int bugCount = (int)bugLines.set.size();
    
    // Fill the background with the appropriate colors
    QRect overallRect = contentsRect();
    
    if (overallRect.width() <= 0)
        return;
    
    overallRect.setWidth(lineNumberArea->width());
    
    QRect bugRect = overallRect;
    QRect lineNumberRect = overallRect;
    bugRect.setWidth(lineNumberAreaBugWidth);
    lineNumberRect.adjust(lineNumberAreaBugWidth, 0, 0, 0);
    
    bool inDarkMode = QtSLiMInDarkMode();
    QPainter painter(lineNumberArea);
    
    // We now show slightly different background colors for the debug point gutter vs. the line number area
    //painter.fillRect(overallRect, inDarkMode ? lineAreaBackground_DARK : lineAreaBackground);
    painter.fillRect(bugRect, inDarkMode ? bugAreaBackground_DARK : bugAreaBackground);
    painter.fillRect(lineNumberRect, inDarkMode ? lineAreaBackground_DARK : lineAreaBackground);
    
    if ((bugCount == 0) && !showLineNumbers)
        return;
    
    static QIcon *bugIcon_LIGHT = nullptr;
    static QIcon *bugIcon_DARK = nullptr;
    
    if (!inDarkMode && !bugIcon_LIGHT && (bugCount > 0))
        bugIcon_LIGHT = new QIcon(":/icons/bug.png");
    if (inDarkMode && !bugIcon_DARK && (bugCount > 0))
        bugIcon_DARK = new QIcon(":/icons/bug_DARK.png");
    
    QIcon *bugIcon = (inDarkMode ? bugIcon_DARK : bugIcon_LIGHT);
    
    // Prepare to show block colors in multispecies mode; we translate into line numbers and colors on demand
    // We postpone this display until after initialize() so we don't show the default colors, which is weird
    std::vector<int> blockColorStarts, blockColorEnds;
    std::vector<QColor> blockColors;
    QtSLiMWindow *controller = slimControllerForWindow();
    int blockColorCount = 0;
    
    if (showBlockColors && controller && controller->community && (controller->community->Tick() >= 1) && blockCursors.size())
    {
        for (int index = 0; index < (int)blockCursors.size(); ++index)
        {
            QTextCursor &tc = blockCursors[index];
            
            if (tc.hasSelection())
            {
                QTextCursor start_tc(tc);
                QTextCursor end_tc(tc);
                start_tc.setPosition(start_tc.selectionStart(), QTextCursor::MoveAnchor);
                end_tc.setPosition(end_tc.selectionEnd(), QTextCursor::MoveAnchor);
                
                blockColorStarts.emplace_back(start_tc.block().blockNumber());
                blockColorEnds.emplace_back(end_tc.block().blockNumber());
                blockColors.emplace_back(controller->qcolorForSpecies(blockSpecies[index]));
            }
        }
        
        blockColorCount = blockColors.size();
    }
    else
    {
        showBlockColors = false;
    }
    
    // Draw the numbers and bug symbols (displaying the current line number in col_1)
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int cursorBlockNumber = textCursor().blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    
    painter.setPen(inDarkMode ? lineAreaNumber_DARK : lineAreaNumber);
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    while (block.isValid() && (top <= p_paintEvent->rect().bottom()))
    {
        if (block.isVisible() && (bottom >= p_paintEvent->rect().top()))
        {
            if (showBlockColors)
            {
                for (int index = 0; index < blockColorCount; ++index)
                {
                    int startBlockNumber = blockColorStarts[index];
                    int endBlockNumber = blockColorEnds[index];
                    
                    if ((blockNumber >= startBlockNumber) && (blockNumber <= endBlockNumber))
                    {
                        QRect speciesHighlightBounds(lineNumberRect.left() + lineNumberRect.width() - 4, top, 2, bottom - top);
                        
                        if (blockNumber == startBlockNumber)
                            speciesHighlightBounds.adjust(0, 1, 0, 0);
                        if (blockNumber == endBlockNumber)
                            speciesHighlightBounds.adjust(0, 0, 0, -1);
                        
                        speciesHighlightBounds = speciesHighlightBounds.intersected(overallRect);
                        
                        painter.fillRect(speciesHighlightBounds, blockColors[index]);
                        break;
                    }
                }
            }
            if (showLineNumbers)
            {
                if (cursorBlockNumber == blockNumber) painter.setPen(inDarkMode ? lineAreaNumberCurrent_DARK : lineAreaNumberCurrent);
                
                QString number = QString::number(blockNumber + 1);
                painter.drawText(-7, top, lineNumberArea->width(), fontMetrics().height(), Qt::AlignRight, number);
                
                if (cursorBlockNumber == blockNumber) painter.setPen(inDarkMode ? lineAreaNumber_DARK : lineAreaNumber);
            }
            if (bugCount)
            {
                if (bugLines.set.find(blockNumber) != bugLines.set.end())
                {
                    QRect bugIconBounds(bugRect.left(), top, bugRect.width(), bottom - top);
                    
                    //painter.fillRect(bugIconBounds, Qt::green);
                    
                    // enforce square bounds for drawing
                    if (bugIconBounds.width() != bugIconBounds.height())
                    {
                        int iconWidth = bugIconBounds.width();
                        int iconHeight = bugIconBounds.height();
                        int adjust = std::abs(iconWidth - iconHeight);
                        int halfAdjust = adjust / 2;
                        int remainder = adjust - halfAdjust;
                        
                        if (iconWidth > iconHeight)
                            bugIconBounds.adjust(halfAdjust, 0, -remainder, 0);
                        else
                            bugIconBounds.adjust(0, halfAdjust, 0, -remainder);
                    }
                    
                    // shrink the bug at large sizes
                    if (bugIconBounds.width() > 24)
                        bugIconBounds.adjust(3, 3, -3, -3);
                    else if (bugIconBounds.width() > 20)
                        bugIconBounds.adjust(2, 2, -2, -2);
                    else if (bugIconBounds.width() > 16)
                        bugIconBounds.adjust(1, 1, -1, -1);
                    
                    // shift left slightly if we can
                    if (bugIconBounds.left() > bugRect.left())
                        bugIconBounds.adjust(-1, 0, -1, 0);
                    
                    //qDebug() << "bugIconBounds ==" << bugIconBounds;
                    //painter.fillRect(bugIconBounds, Qt::red);
                    
                    if (debugPointsEnabled)
                        bugIcon->paint(&painter, bugIconBounds, Qt::AlignCenter, QIcon::Normal, QIcon::Off);
                    else
                        bugIcon->paint(&painter, bugIconBounds, Qt::AlignCenter, QIcon::Disabled, QIcon::Off);
                }
            }
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
    
    painter.restore();
}

void QtSLiMScriptTextEdit::lineNumberAreaMouseEvent(QMouseEvent *p_mouseEvent)
{
    if (lineNumberAreaBugWidth == 0)
        return;
    
    // For some reason, Qt calls mousePressEvent() first for control-clicks and right-clicks,
    // and *then* calls contextMenuEvent(), so we need to detect the context menu situation
    // and return without doing anything.  Note that Qt::RightButton is set for control-clicks!
    if (p_mouseEvent->button() == Qt::RightButton)
        return;
    
    QPointF localPos = p_mouseEvent->localPos();
    qreal localY = localPos.y();
    
    //qDebug() << "localY ==" << localY;
    
    if ((localPos.x() < 0) || (localPos.x() >= lineNumberAreaBugWidth + 2))     // +2 for a little slop
            return;
    
    // Find the position of the click in the document.  We loop through the blocks manually.
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    
    while (block.isValid())
    {
        if (block.isVisible())
        {
            if ((localY >= top) && (localY <= bottom))
            {
                toggleDebuggingForLine(blockNumber);
                break;
            }
            else if (localY < top)
            {
                break;
            }
        }
        
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void QtSLiMScriptTextEdit::lineNumberAreaContextMenuEvent(QContextMenuEvent *p_event)
{
    if (lineNumberAreaBugWidth == 0)
        return;
    
    p_event->accept();
    
    QMenu contextMenu("line_area_menu", this);
    
    QAction *clearDebugPointsAction = contextMenu.addAction("Clear Debug Points");
    
    // Run the context menu synchronously
    QAction *action = contextMenu.exec(p_event->globalPos());
    
    // Act upon the chosen action; we just do it right here instead of dealing with slots
    if (action)
    {
        if (action == clearDebugPointsAction)
            clearDebugPoints();
    }
}

void QtSLiMScriptTextEdit::lineNumberAreaWheelEvent(QWheelEvent *p_wheelEvent)
{
    // We want wheel events in the debug/line number area to scroll the script textview, so it forwards them to us here
    // I'm not sure of the legality of this, since the event is tailored for a different widget, but it seems to work...
    wheelEvent(p_wheelEvent);
}

void QtSLiMScriptTextEdit::clearDebugPoints(void)
{
    bugCursors.clear();
    updateDebugPoints();
}

void QtSLiMScriptTextEdit::clearScriptBlockColoring(void)
{
    blockCursors.clear();
    blockSpecies.clear();
}

void QtSLiMScriptTextEdit::addScriptBlockColoring(int startPos, int endPos, Species *species)
{
    QTextCursor tc(document());
    tc.setPosition(startPos, QTextCursor::MoveAnchor);
    tc.setPosition(endPos, QTextCursor::KeepAnchor);
    
    blockCursors.emplace_back(tc);
    blockSpecies.emplace_back(species);
}

































