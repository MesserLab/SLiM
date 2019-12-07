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
#include <QDebug>

#include "QtSLiMPreferences.h"
#include "QtSLiMEidosPrettyprinter.h"
#include "QtSLiMSyntaxHighlighting.h"
#include "QtSLiMEidosConsole.h"
#include "QtSLiMHelpWindow.h"
#include "QtSLiMAppDelegate.h"
#include "eidos_script.h"
#include "eidos_token.h"
#include "slim_eidos_block.h"


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
    // clear the custom error background color whenever the selection changes
    connect(this, &QTextEdit::selectionChanged, [this]() { setPalette(style()->standardPalette()); });
    connect(this, &QTextEdit::cursorPositionChanged, [this]() { setPalette(style()->standardPalette()); });
    
    // clear the status bar on a selection change; FIXME upgrade this to updateStatusFieldFromSelection() eventually...
    connect(this, &QTextEdit::selectionChanged, [this]() { statusBarForWindow()->clearMessage(); });
    connect(this, &QTextEdit::cursorPositionChanged, [this]() { statusBarForWindow()->clearMessage(); });
    
    // Wire up to change the font when the display font pref changes
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::displayFontPrefChanged, this, &QtSLiMTextEdit::displayFontPrefChanged);
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::scriptSyntaxHighlightPrefChanged, this, &QtSLiMTextEdit::scriptSyntaxHighlightPrefChanged);
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::outputSyntaxHighlightPrefChanged, this, &QtSLiMTextEdit::outputSyntaxHighlightPrefChanged);
    
    // Get notified of modifier key changes, so we can change out cursor
    connect(qtSLiMAppDelegate, &QtSLiMAppDelegate::modifiersChanged, this, &QtSLiMTextEdit::modifiersChanged);
    
    // set up the script and output textedits
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    int tabWidth = 0;
    QFont scriptFont = prefs.displayFontPref(&tabWidth);
    
    setFont(scriptFont);
    setTabStopWidth(tabWidth);    // should use setTabStopDistance(), which requires Qt 5.10; see https://stackoverflow.com/a/54605709/2752221
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
    setTabStopWidth(tabWidth);
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
            {
                statusBar->setStyleSheet("color: #cc0000; font-size: 11px;");
                statusBar->showMessage(q_errorDiagnostic.trimmed());
            }
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

void QtSLiMTextEdit::prettyprint(void)
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
			
            if (Eidos_prettyprintTokensFromScript(tokens, script, pretty))
                setPlainText(QString::fromStdString(pretty));
			else
                qApp->beep();
		}
	}
	else
	{
        qApp->beep();
	}
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
        int characterPositionClicked = document()->documentLayout()->hitTest(event->localPos(), Qt::ExactHit);
        
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


//
//  QtSLiMScriptTextEdit
//

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
            if (line[0] == '\t')
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
































