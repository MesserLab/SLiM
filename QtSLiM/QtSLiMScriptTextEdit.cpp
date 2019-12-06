#include "QtSLiMScriptTextEdit.h"

#include <QApplication>
#include <QGuiApplication>
#include <QTextCursor>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QDebug>
#include <QAbstractTextDocumentLayout>


//
//  QtSLiMTextEdit
//

QtSLiMTextEdit::~QtSLiMTextEdit()
{
}


void QtSLiMTextEdit::mousePressEvent(QMouseEvent *event)
{
    bool optionPressed = QGuiApplication::keyboardModifiers().testFlag(Qt::AltModifier);
    
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
            emit optionClickOnSymbol(symbol);   // this connects to QtSLiMWindow::scriptHelpOptionClick(), which has additional lookup smarts
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
    // we want a pointing hand cursor when option is pressed; if the cursor is wrong, fix it
    // note the cursor for QTextEdit is apparently controlled by its viewport
    bool optionPressed = QGuiApplication::queryKeyboardModifiers().testFlag(Qt::AltModifier);
    QWidget *vp = viewport();
    
    if (optionPressed && (vp->cursor().shape() != Qt::PointingHandCursor))
        vp->setCursor(Qt::PointingHandCursor);
    else if (!optionPressed && (vp->cursor().shape() != Qt::IBeamCursor))
        vp->setCursor(Qt::IBeamCursor);
}

void QtSLiMTextEdit::enterEvent(QEvent *event)
{
    // forward to super
    QTextEdit::enterEvent(event);
    fixMouseCursor();
}

void QtSLiMTextEdit::keyPressEvent(QKeyEvent *event)
{
    // forward to super
    QTextEdit::keyPressEvent(event);
    fixMouseCursor();
}

void QtSLiMTextEdit::keyReleaseEvent(QKeyEvent *event)
{
    // forward to super
    QTextEdit::keyReleaseEvent(event);
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
































