#include "QtSLiMConsoleTextEdit.h"

#include <QTextCursor>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QDebug>

#include "QtSLiMPreferences.h"
#include "QtSLiMExtras.h"
#include "eidos_globals.h"
#include "slim_globals.h"


// It's tempting to use QChar::LineSeparator here instead of \n, and in some ways it
// produces better behavior (copy/paste to TextEdit produces better results, for example),
// but it might cause the user problems because it's Unicode-specific; and we want command
// lines to be a separate block with margins above and below, also.
static const QString NEWLINE("\n");

// This is margin in pixels above and below command lines, to set them off nicely
static const double BLOCK_MARGIN = 3.0;


QtSLiMConsoleTextEdit::~QtSLiMConsoleTextEdit()
{
}

QTextCharFormat QtSLiMConsoleTextEdit::textFormatForColor(QColor color)
{
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    QFont displayFont(prefs.displayFontPref());
    QTextCharFormat attrs;
    attrs.setFont(displayFont);
    attrs.setForeground(QBrush(color));
    return attrs;
}

void QtSLiMConsoleTextEdit::showWelcome(void)
{
    setCurrentCharFormat(textFormatForColor(Qt::black));
    
    QString welcomeMessage;
    welcomeMessage = welcomeMessage + "Eidos version " + EIDOS_VERSION_STRING + NEWLINE + NEWLINE;		// EIDOS VERSION
    welcomeMessage += "By Benjamin C. Haller (http://benhaller.com/)." + NEWLINE;
    welcomeMessage += "Copyright (c) 2016–2019 P. Messer. All rights reserved." + NEWLINE + NEWLINE;
    welcomeMessage += "Eidos is free software with ABSOLUTELY NO WARRANTY." + NEWLINE;
    welcomeMessage += "Type license() for license and distribution details." + NEWLINE + NEWLINE;
    welcomeMessage += "Go to https://github.com/MesserLab/SLiM for source code," + NEWLINE;
    welcomeMessage += "documentation, examples, and other information." + NEWLINE + NEWLINE;
    welcomeMessage += "Welcome to Eidos!" + NEWLINE + NEWLINE;
    welcomeMessage += "---------------------------------------------------------" + NEWLINE + NEWLINE;
    welcomeMessage += "Connected to QtSLiM simulation." + NEWLINE;
    welcomeMessage = welcomeMessage + "SLiM version " + SLIM_VERSION_STRING + "." + NEWLINE + NEWLINE;      // SLIM VERSION
    welcomeMessage += "---------------------------------------------------------" + NEWLINE + NEWLINE;
    
    insertPlainText(welcomeMessage);
}

void QtSLiMConsoleTextEdit::showPrompt(QChar promptChar)
{
    QTextCharFormat promptAttrs = textFormatForColor(QtSLiMColorWithRGB(170/255.0, 13/255.0, 145/255.0, 1.0));
    QTextCharFormat inputAttrs = textFormatForColor(QtSLiMColorWithRGB(28/255.0, 0/255.0, 207/255.0, 1.0));
    
    moveCursor(QTextCursor::End);
    setCurrentCharFormat(promptAttrs);
    insertPlainText(promptChar);
    moveCursor(QTextCursor::End);
    setCurrentCharFormat(inputAttrs);
    insertPlainText(" ");
    moveCursor(QTextCursor::End);
    
    QTextCursor promptCursor(document());
    promptCursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    promptCursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, 2);
    promptCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 2);
    
    QTextBlockFormat marginBlockFormat;
    marginBlockFormat.setTopMargin(BLOCK_MARGIN);
    marginBlockFormat.setBottomMargin(BLOCK_MARGIN);
    promptCursor.setBlockFormat(marginBlockFormat);
    
    // We remember the prompt range for various purposes such as uneditability of old content
    lastPromptCursor = promptCursor;
    lastPromptCursor.setKeepPositionOnInsert(true);
}

void QtSLiMConsoleTextEdit::showPrompt(void)
{
    showPrompt('>');
}

void QtSLiMConsoleTextEdit::showContinuationPrompt(void)
{
    // The user has entered an incomplete script line, so we need to append a newline...
    moveCursor(QTextCursor::End);
    insertPlainText(NEWLINE);
    
    // ...and issue a continuation prompt to await further input
    int promptEnd = lastPromptCursor.position();
    
    showPrompt('+');
    originalPromptEnd = promptEnd;
    isContinuationPrompt = true;
}

void QtSLiMConsoleTextEdit::appendExecution(QString result, QString errorString, QString tokenString, QString parseString, QString executionString)
{
    moveCursor(QTextCursor::End);
    insertPlainText(NEWLINE);
    
    appendSpacer();
    
    if (tokenString.length())
    {
        moveCursor(QTextCursor::End);
        setCurrentCharFormat(textFormatForColor(QtSLiMColorWithRGB(100/255.0, 56/255.0, 32/255.0, 1.0)));
        insertPlainText(tokenString);
        appendSpacer();
    }
    if (parseString.length())
    {
        moveCursor(QTextCursor::End);
        setCurrentCharFormat(textFormatForColor(QtSLiMColorWithRGB(0/255.0, 116/255.0, 0/255.0, 1.0)));
        insertPlainText(parseString);
        appendSpacer();
    }
    if (executionString.length())
    {
        moveCursor(QTextCursor::End);
        setCurrentCharFormat(textFormatForColor(QtSLiMColorWithRGB(63/255.0, 110/255.0, 116/255.0, 1.0)));
        insertPlainText(executionString);
        appendSpacer();
    }
    if (result.length())
    {
        QTextBlockFormat plainBlockFormat;
        
        moveCursor(QTextCursor::End);
        setCurrentCharFormat(textFormatForColor(QtSLiMColorWithRGB(0/255.0, 0/255.0, 0/255.0, 1.0)));
        textCursor().setBlockFormat(plainBlockFormat);
        insertPlainText(result);
        appendSpacer();
    }
    if (errorString.length())
    {
        QTextBlockFormat marginBlockFormat;
        marginBlockFormat.setTopMargin(BLOCK_MARGIN);
        marginBlockFormat.setBottomMargin(BLOCK_MARGIN);
        
        moveCursor(QTextCursor::End);
        setCurrentCharFormat(textFormatForColor(QtSLiMColorWithRGB(196/255.0, 26/255.0, 22/255.0, 1.0)));
        textCursor().setBlockFormat(marginBlockFormat);
        insertPlainText(errorString);
        appendSpacer();
        
        /*
        if (!gEidosExecutingRuntimeScript && (gEidosCharacterStartOfErrorUTF16 >= 0) && (gEidosCharacterEndOfErrorUTF16 >= gEidosCharacterStartOfErrorUTF16) && (scriptRange.location != NSNotFound))
		{
			// An error occurred, so let's try to highlight it in the input
			int errorTokenStart = gEidosCharacterStartOfErrorUTF16 + (int)scriptRange.location;
			int errorTokenEnd = gEidosCharacterEndOfErrorUTF16 + (int)scriptRange.location;
			
			NSRange charRange = NSMakeRange(errorTokenStart, errorTokenEnd - errorTokenStart + 1);
			
			[ts addAttribute:NSBackgroundColorAttributeName value:[NSColor redColor] range:charRange];
			[ts addAttribute:NSForegroundColorAttributeName value:[NSColor whiteColor] range:charRange];
		}
        */
    }
    
    // scroll to bottom
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void QtSLiMConsoleTextEdit::clearToPrompt(void)
{
    if (isContinuationPrompt)
    {
        QTextCursor deleteCursor(lastPromptCursor);
        deleteCursor.setPosition(originalPromptEnd - 2, QTextCursor::MoveAnchor);
        deleteCursor.movePosition(QTextCursor::Start, QTextCursor::KeepAnchor);
        deleteCursor.insertText("");
        originalPromptEnd = 2;
    }
    else
    {
        QTextCursor deleteCursor(lastPromptCursor);
        deleteCursor.setPosition(deleteCursor.anchor(), QTextCursor::MoveAnchor);
        deleteCursor.movePosition(QTextCursor::Start, QTextCursor::KeepAnchor);
        deleteCursor.insertText("");
    }
}

void QtSLiMConsoleTextEdit::appendSpacer(void)
{
    // With Qt, we do not use spacers the way we do in SLiMgui; I think the
    // HTML-based nature of Qt's text editing is making it difficult for
    // that to work.  Instead, we use block formats to set up spacing around
    // command lines, which is probably a better design anyway.  I've left
    // the calls to appendSpacer() intact, and just commented out this code,
    // for now since I'm still experimenting with this.
    
    /*
    QTextCharFormat spacerAttrs(textFormatForColor(Qt::black));
    spacerAttrs.setFontPointSize(BLOCK_MARGIN);
    
    QTextCursor lastCharCursor(document());
    lastCharCursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    lastCharCursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor);
    QString lastChar = lastCharCursor.selectedText();
    
    // we only add a spacer newline if the current contents already end in a newline; we don't introduce new breaks
    if ((lastChar == "\n") || (lastChar == QChar::ParagraphSeparator) || (lastChar == QChar::LineSeparator))
    {
        moveCursor(QTextCursor::End);
        insertPlainText(NEWLINE);
        moveCursor(QTextCursor::End);
        
        // we set the line height not only for the newline we just added, but for the
        // preceding newline as well; Qt seems to require this for it to have visible effect
        // FIXME this is still not working perfectly for the first execution, for some reason!
        QTextCursor spacerCursor(document());
        lastCharCursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
        lastCharCursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 2);
        lastCharCursor.setCharFormat(spacerAttrs);
    }
    */
}

QString QtSLiMConsoleTextEdit::currentCommandAtPrompt(void)
{
    QTextCursor commandCursor(lastPromptCursor);
    commandCursor.setPosition(commandCursor.position(), QTextCursor::MoveAnchor);
    commandCursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    return commandCursor.selectedText();
}

void QtSLiMConsoleTextEdit::setCommandAtPrompt(QString newCommand)
{
    newCommand = newCommand.trimmed();  // trim off whitespace around the command line
    
    QTextCursor commandCursor(lastPromptCursor);
    commandCursor.setPosition(commandCursor.position(), QTextCursor::MoveAnchor);
    commandCursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    commandCursor.setKeepPositionOnInsert(false);    
    commandCursor.insertText(newCommand);
    moveCursor(QTextCursor::End);
}

void QtSLiMConsoleTextEdit::registerNewHistoryItem(QString newItem)
{
    // If there is a provisional item on the top of the stack, get rid of it and replace it
	if (lastHistoryItemIsProvisional)
	{
        history.pop_back();
		lastHistoryItemIsProvisional = false;
	}
	
    history.push_back(newItem);
	historyIndex = history.count();	// a new prompt, one beyond the last history item
}

void QtSLiMConsoleTextEdit::elideContinuationPrompt(void)
{
    // This replaces the continuation prompt, if there is one, with a space, and switches the active prompt back to the original prompt;
	// the net effect is as if the user entered a newline and two spaces at the original prompt, with no continuation.  Note that the
	// two spaces at the beginning of continuation lines is mirrored in fullInputString, below.
	if (isContinuationPrompt)
	{
        QTextCharFormat inputAttrs = textFormatForColor(QtSLiMColorWithRGB(28/255.0, 0/255.0, 207/255.0, 1.0));
        QTextCursor fixCursor(lastPromptCursor);
        fixCursor.setPosition(lastPromptCursor.anchor(), QTextCursor::MoveAnchor);
        fixCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
        fixCursor.insertText(" ", inputAttrs);
        
        lastPromptCursor.setPosition(originalPromptEnd - 2, QTextCursor::MoveAnchor);
        lastPromptCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 2);
        isContinuationPrompt = false;
	}
}

QString QtSLiMConsoleTextEdit::fullInputString(void)
{
	elideContinuationPrompt();
	
    QTextCursor commandCursor(lastPromptCursor);
    
    commandCursor.setPosition(commandCursor.position(), QTextCursor::MoveAnchor);
    commandCursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    return commandCursor.selectedText();
}

void QtSLiMConsoleTextEdit::previousHistory(void)
{
    if (historyIndex > 0)
	{
		// If the user has typed at the current prompt and it is unsaved, save it in the history before going up
		if (historyIndex == history.count())
		{
			QString commandString = currentCommandAtPrompt();
			
			if (commandString.length() > 0)
			{
				// If there is a provisional item on the top of the stack, get rid of it and replace it
				if (lastHistoryItemIsProvisional)
				{
                    history.pop_back();
					lastHistoryItemIsProvisional = false;
					--historyIndex;
				}
				
                history.push_back(commandString);
				lastHistoryItemIsProvisional = true;
			}
		}
		
		// if the only item on the stack was provisional, and we just replaced it, we may have nowhere up to go
		if (historyIndex > 0)
		{
			--historyIndex;
		
			setCommandAtPrompt(history[historyIndex]);
		}
		
		//NSLog(@"moveUp: end:\nhistory = %@\nhistoryIndex = %d\nlastHistoryItemIsProvisional = %@", history, historyIndex, lastHistoryItemIsProvisional ? @"YES" : @"NO");
	}
}

void QtSLiMConsoleTextEdit::nextHistory(void)
{
    if (historyIndex <= history.count())
	{
		// If the user has typed at the current prompt and it is unsaved, save it in the history before going down
		if (historyIndex == history.count())
		{
			QString commandString = currentCommandAtPrompt();
			
			if (commandString.length() > 0)
			{
				// If there is a provisional item on the top of the stack, get rid of it and replace it
				if (lastHistoryItemIsProvisional)
				{
                    history.pop_back();
					lastHistoryItemIsProvisional = false;
					--historyIndex;
				}
				
                history.push_back(commandString);
				lastHistoryItemIsProvisional = true;
			}
			else
				return;
		}
		
		++historyIndex;
		
		if (historyIndex == history.count())
			setCommandAtPrompt("");
		else
			setCommandAtPrompt(history[historyIndex]);
		
		//NSLog(@"moveDown: end:\nhistory = %@\nhistoryIndex = %d\nlastHistoryItemIsProvisional = %@", history, historyIndex, lastHistoryItemIsProvisional ? @"YES" : @"NO");
	}
}

void QtSLiMConsoleTextEdit::executeCurrentPrompt(void)
{
    QTextCursor endCursor(document());
    endCursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    
    if (isContinuationPrompt && (lastPromptCursor.position() == endCursor.position()))
    {
        // If the user has hit return at an empty continuation prompt, we take that as a sign that they want to get out of it
        QString executionString = fullInputString();
        
        registerNewHistoryItem(executionString);
        
        moveCursor(QTextCursor::End);
        insertPlainText(NEWLINE);
        
        // show a new prompt
        showPrompt();
    }
    else
    {
        // The current prompt might be a non-empty continuation prompt, so now we get the full input string from the original prompt
        QString command = fullInputString();
        
        //qDebug() << "execute " << command;
        registerNewHistoryItem(command);
        emit executeScript(command);
    }
}

void QtSLiMConsoleTextEdit::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::MoveToPreviousLine))
    {
        // up-arrow pressed; cycle through the command history
        previousHistory();
    }
    else if (event->matches(QKeySequence::MoveToNextLine))
    {
        // down-arrow pressed; cycle through the command history
        nextHistory();
    }
    else if (event->matches(QKeySequence::InsertLineSeparator) || event->matches(QKeySequence::InsertParagraphSeparator))
    {
        // return/enter pressed; execute the statement(s) entered
        executeCurrentPrompt();
    }
    else if (event->key() == Qt::Key_Escape)
    {
        // escape pressed; code completion
        qDebug() << "escape!";
    }
    else
    {
        // if the key was not handled above, pass the event to super
        QTextEdit::keyPressEvent(event);
    }
}
































