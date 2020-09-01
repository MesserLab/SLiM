//
//  QtSLiMConsoleTextEdit.cpp
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


#include "QtSLiMConsoleTextEdit.h"

#include <QTextCursor>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QCompleter>
#include <QAbstractItemView>
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


QtSLiMConsoleTextEdit::QtSLiMConsoleTextEdit(const QString &text, QWidget *parent) : QtSLiMTextEdit(text, parent)
{
    selfInit();
}

QtSLiMConsoleTextEdit::QtSLiMConsoleTextEdit(QWidget *parent) : QtSLiMTextEdit(parent)
{
    selfInit();
}

void QtSLiMConsoleTextEdit::selfInit(void)
{
    // set up to react to selection changes; see these methods for comments
    connect(this, &QTextEdit::selectionChanged, this, &QtSLiMConsoleTextEdit::handleSelectionChanged);
    connect(this, &QTextEdit::cursorPositionChanged, this, &QtSLiMConsoleTextEdit::handleSelectionChanged);
    connect(this, &QtSLiMConsoleTextEdit::selectionWasChangedDuringLastEvent, this, &QtSLiMConsoleTextEdit::adjustSelectionAndReadOnly, Qt::QueuedConnection);
}

QtSLiMConsoleTextEdit::~QtSLiMConsoleTextEdit()
{
}

QTextCharFormat QtSLiMConsoleTextEdit::textFormatForColor(QColor color)
{
    QTextCharFormat attrs;
    attrs.setForeground(QBrush(color));
    return attrs;
}

void QtSLiMConsoleTextEdit::scriptStringAndSelection(QString &scriptString, int &pos, int &len)
{
    // here we provide a subclass definition of what we consider "script": just what follows the prompt
    QTextCursor commandCursor(lastPromptCursor);
    commandCursor.setPosition(commandCursor.position(), QTextCursor::MoveAnchor);
    commandCursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    
    scriptString = commandCursor.selectedText();
    
    QTextCursor selection = textCursor();
    pos = selection.anchor();
    len = selection.position() - pos;
    
    pos -= commandCursor.anchor();  // compensate for snipping off everything from the prompt back
}

void QtSLiMConsoleTextEdit::showWelcome(void)
{
    setCurrentCharFormat(textFormatForColor(Qt::black));
    
    QString welcomeMessage;
    welcomeMessage = welcomeMessage + "Eidos version " + EIDOS_VERSION_STRING + NEWLINE + NEWLINE;		// EIDOS VERSION
    welcomeMessage += "By Benjamin C. Haller (http://benhaller.com/)." + NEWLINE;
    welcomeMessage += "Copyright (c) 2016–2020 P. Messer. All rights reserved." + NEWLINE + NEWLINE;
    welcomeMessage += "Eidos is free software with ABSOLUTELY NO WARRANTY." + NEWLINE;
    welcomeMessage += "Type license() for license and distribution details." + NEWLINE + NEWLINE;
    welcomeMessage += "Go to https://github.com/MesserLab/SLiM for source code," + NEWLINE;
    welcomeMessage += "documentation, examples, and other information." + NEWLINE + NEWLINE;
    welcomeMessage += "Welcome to Eidos!" + NEWLINE + NEWLINE;
    welcomeMessage += "---------------------------------------------------------" + NEWLINE + NEWLINE;
    welcomeMessage += "Connected to SLiMgui simulation." + NEWLINE;
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
    
    // When a new prompt appears, one can no longer undo/redo previous changes
    document()->clearUndoRedoStacks();
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
    
    if (tokenString.length())
    {
        moveCursor(QTextCursor::End);
        setCurrentCharFormat(textFormatForColor(QtSLiMColorWithRGB(100/255.0, 56/255.0, 32/255.0, 1.0)));
        insertPlainText(tokenString);
    }
    if (parseString.length())
    {
        moveCursor(QTextCursor::End);
        setCurrentCharFormat(textFormatForColor(QtSLiMColorWithRGB(0/255.0, 116/255.0, 0/255.0, 1.0)));
        insertPlainText(parseString);
    }
    if (executionString.length())
    {
        moveCursor(QTextCursor::End);
        setCurrentCharFormat(textFormatForColor(QtSLiMColorWithRGB(63/255.0, 110/255.0, 116/255.0, 1.0)));
        insertPlainText(executionString);
    }
    if (result.length())
    {
        QTextBlockFormat plainBlockFormat;
        
        moveCursor(QTextCursor::End);
        setCurrentCharFormat(textFormatForColor(QtSLiMColorWithRGB(0/255.0, 0/255.0, 0/255.0, 1.0)));
        textCursor().setBlockFormat(plainBlockFormat);
        insertPlainText(result);
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
        
        if (!gEidosExecutingRuntimeScript &&
                (gEidosCharacterStartOfErrorUTF16 >= 0) &&
                (gEidosCharacterEndOfErrorUTF16 >= gEidosCharacterStartOfErrorUTF16))
		{
			// An error occurred, so let's try to highlight it in the input
            int promptEnd = lastPromptCursor.position();
			int errorTokenStart = gEidosCharacterStartOfErrorUTF16 + promptEnd;
			int errorTokenEnd = gEidosCharacterEndOfErrorUTF16 + promptEnd;
			
            QTextCursor highlightCursor(lastPromptCursor);
            highlightCursor.setPosition(errorTokenStart, QTextCursor::MoveAnchor);
            highlightCursor.setPosition(errorTokenEnd + 1, QTextCursor::KeepAnchor);
            
            QTextCharFormat highlightFormat;
            highlightFormat.setForeground(QBrush(Qt::black));
            highlightFormat.setBackground(QBrush(QColor(QColor(Qt::red).lighter(120))));
            
            highlightCursor.setCharFormat(highlightFormat);
            
            // note that unlike the error highlighting in the scripting views, this error
            // highlighting is permanent, and will not be removed by later cursor changes
		}
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
    
    // Clearing the console is not undoable, and it clears the undo/redo history
    document()->clearUndoRedoStacks();
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
    // With a completer, we have to call super in some cases to let it handle the completer logic
    // This code is parallel to the code in QtSLiMTextEdit::keyPressEvent()
    if (completer)
    {
        if (completer->popup()->isVisible()) {
            // The following keys are forwarded by the completer to the widget
           switch (event->key()) {
           case Qt::Key_Enter:
           case Qt::Key_Return:
           case Qt::Key_Escape:
           case Qt::Key_Tab:
           case Qt::Key_Backtab:
                QtSLiMTextEdit::keyPressEvent(event);
                return; // let super pass these keys on to the completer
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
            // then we drop through to our normal key handling below
            completer->popup()->hide();
        }
        else
        {
            // the completer shortcut has been pressed; let super run the completer
            QtSLiMTextEdit::keyPressEvent(event);
            return;
        }
    }
    
    if (event->matches(QKeySequence::MoveToPreviousLine))
    {
        // up-arrow pressed; cycle through the command history
        previousHistory();
        event->accept();
    }
    else if (event->matches(QKeySequence::MoveToNextLine))
    {
        // down-arrow pressed; cycle through the command history
        nextHistory();
        event->accept();
    }
    else if (event->matches(QKeySequence::InsertLineSeparator) || event->matches(QKeySequence::InsertParagraphSeparator))
    {
        // return/enter pressed; execute the statement(s) entered
        executeCurrentPrompt();
        event->accept();
    }
    else if (event->key() == Qt::Key_Escape)
    {
        // escape pressed; this should be handled by the code above, but if we get here just ignore it
        //qDebug() << "escape!";
        event->accept();
    }
    else if (event->matches(QKeySequence::MoveToEndOfBlock) ||
             event->matches(QKeySequence::MoveToEndOfDocument) ||
             event->matches(QKeySequence::MoveToEndOfLine) ||
             event->matches(QKeySequence::MoveToNextChar) ||
             event->matches(QKeySequence::MoveToNextLine) ||
             event->matches(QKeySequence::MoveToNextPage) ||
             event->matches(QKeySequence::MoveToNextWord) ||
             event->matches(QKeySequence::MoveToPreviousChar) ||
             event->matches(QKeySequence::MoveToPreviousLine) ||
             event->matches(QKeySequence::MoveToPreviousPage) ||
             event->matches(QKeySequence::MoveToPreviousWord) ||
             event->matches(QKeySequence::MoveToStartOfBlock) ||
             event->matches(QKeySequence::MoveToStartOfDocument) ||
             event->matches(QKeySequence::MoveToStartOfLine) ||
             event->matches(QKeySequence::SelectAll) ||
             event->matches(QKeySequence::SelectEndOfBlock) ||
             event->matches(QKeySequence::SelectEndOfDocument) ||
             event->matches(QKeySequence::SelectEndOfLine) ||
             event->matches(QKeySequence::SelectNextChar) ||
             event->matches(QKeySequence::SelectNextLine) ||
             event->matches(QKeySequence::SelectNextPage) ||
             event->matches(QKeySequence::SelectNextWord) ||
             event->matches(QKeySequence::SelectPreviousChar) ||
             event->matches(QKeySequence::SelectPreviousLine) ||
             event->matches(QKeySequence::SelectPreviousPage) ||
             event->matches(QKeySequence::SelectPreviousWord) ||
             event->matches(QKeySequence::SelectStartOfBlock) ||
             event->matches(QKeySequence::SelectStartOfDocument) ||
             event->matches(QKeySequence::SelectStartOfLine))
    {
        if (isReadOnly())
        {
            // being read-only interferes with keyboard selection changes, for some odd reason,
            // so we change ourselves to editable around the call to super
            setReadOnly(false);
            QtSLiMTextEdit::keyPressEvent(event);
            updateReadOnly();   // maybe roDTS has changed, so maybe we no longer want to be read-only
        }
        else if (event->matches(QKeySequence::SelectAll))
        {
            // in the non-read-only case, we want Select All to select everything in the console, rather
            // than follow the selection-clipping convention below, I think...
            QtSLiMTextEdit::keyPressEvent(event);
        }
        else
        {
            // in the non-read-only case, the cursor is inside the command area, and we want to clip movement and
            // selection to stay within the command area; the user can select other content if they want to, but
            // by default we assume their intention is to work within the command line (except select all, above)
            QtSLiMTextEdit::keyPressEvent(event);
            
            QTextCursor selection(textCursor());
            int anchor = std::max(selection.anchor(), lastPromptCursor.position());
            int pos = std::max(selection.position(), lastPromptCursor.position());
            
            selection.setPosition(anchor, QTextCursor::MoveAnchor);
            selection.setPosition(pos, QTextCursor::KeepAnchor);
            setTextCursor(selection);
        }
    }
    else if (event->matches(QKeySequence::Backspace) || event->matches(QKeySequence::DeleteStartOfWord) || (event->key() == Qt::Key_Backspace))
    {
        // suppress backspace if it would backspace into the prompt; note QKeySequence::Backspace doesn't seem to work!
        QTextCursor cursor(textCursor());
        
        if ((cursor.position() == cursor.anchor()) && (cursor.position() == lastPromptCursor.position()))
        {
            event->accept();
            return;
        }
        
        QtSLiMTextEdit::keyPressEvent(event);
    }
    else if (event->matches(QKeySequence::DeleteCompleteLine))
    {
        // I'm not sure how to produce this on a keyboard, but let's make sure it doesn't delete the prompt
        setCommandAtPrompt("");
        event->accept();
    }
    else
    {
        // if the key was not handled above, pass the event to super
        QtSLiMTextEdit::keyPressEvent(event);
    }
}

void QtSLiMConsoleTextEdit::mousePressEvent(QMouseEvent *event)
{
    // keep track of when we are inside a mouse tracking loop
    insideMouseTracking = true;
    sawSelectionChange = false;
    
    QtSLiMTextEdit::mousePressEvent(event);
}

void QtSLiMConsoleTextEdit::mouseReleaseEvent(QMouseEvent *event)
{
    QtSLiMTextEdit::mouseReleaseEvent(event);
    
    // we're done with the mouse tracking loop; if it changed the selection, we now adjust
    insideMouseTracking = false;
    if (sawSelectionChange)
        adjustSelectionAndReadOnly();
}

void QtSLiMConsoleTextEdit::handleSelectionChanged(void)
{
    // This is a handler for both selectionChanged and cursorPositionChanged signals,
    // because neither one is emitted consistently enough to be usable in general,
    // unfortunately.  So for a given change, we may be called just once (through one
    // of those signals) or twice.  We want to defer our reaction until the end of
    // the event being handled, because sometimes Qt will change one aspect of the
    // selection (the anchor, say), send one signal, then change another aspect of
    // the selection (the position, say), and send another signal; if we react to
    // the first change before the second change has also been made we will misjudge
    // what needs to be done.
    
    //qDebug() << "handleSelectionChanged";
    
    if (insideMouseTracking)
    {
        // when mouse-tracking, selection changes get deferred until tracking is complete
        // adjustSelectionAndReadOnly() will be called by QtSLiMConsoleTextEdit::mouseReleaseEvent()
        sawSelectionChange = true;
    }
    else
    {
        // when not mouse-tracking, selection changes get adjusted at the end of
        // this event loop callout so we are sure all changes have been made
        // adjustSelectionAndReadOnly() will be called by a queued connection set up in selfInit()
        emit selectionWasChangedDuringLastEvent();
    }
}

void QtSLiMConsoleTextEdit::adjustSelectionAndReadOnly(void)
{
    //qDebug() << "adjustSelectionAndReadOnly";
    
    QTextCursor selection(textCursor());
    
    if (selection.position() == selection.anchor())
    {
        // a zero-length selection has been requested; if it falls before the end of the prompt, we adjust it
        // to fall at the end of the console instead; note we will get a re-entrant call back from this
        // a special case: if the new position is just to the left of the prompt end, move to the prompt end
        if (selection.position() == lastPromptCursor.position() - 1)
            moveCursor(QTextCursor::Right);
        else if (selection.position() < lastPromptCursor.position())
            moveCursor(QTextCursor::End);
        
        setReadOnlyDueToSelection(false);
    }
    else
    {
        // a non-zero-length selection has been requested; we allow any such selection, but if it encompasses
        // characters before the end of the current prompt, we make ourselves non-editable to prevent changes
        int start = selection.selectionStart();
        
        if (start < lastPromptCursor.position())
            setReadOnlyDueToSelection(true);
        else
            setReadOnlyDueToSelection(false);
    }
}

// prevent drops outside of the prompt area
// thanks to https://github.com/uglide/QtConsole/blob/master/src/qconsole.cpp
void QtSLiMConsoleTextEdit::dragMoveEvent(QDragMoveEvent *event)
{
    // Figure out where the drop would go
    QTextCursor dropCursor = cursorForPosition(event->pos());
    
    //qDebug() << "dragMoveEvent: " << dropCursor.position();
    
    if (dropCursor.position() < lastPromptCursor.position())
    {
        // Ignore the event if the drop would be outside the command area
        event->ignore(cursorRect(dropCursor));
    }
    else
    {
        // Accept the event if the drop is inside the command area
        event->accept(cursorRect(dropCursor));
    }
}




























