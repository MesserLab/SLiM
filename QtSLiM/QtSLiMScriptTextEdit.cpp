#include "QtSLiMScriptTextEdit.h"

#include <QGuiApplication>
#include <QTextCursor>
#include <QMouseEvent>
#include <QDebug>
#include <QAbstractTextDocumentLayout>


QtSLiMScriptTextEdit::~QtSLiMScriptTextEdit()
{
}

void QtSLiMScriptTextEdit::mousePressEvent(QMouseEvent *event)
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

void QtSLiMScriptTextEdit::mouseMoveEvent(QMouseEvent *event)
{
    // forward to super, as long as we did not intercept this mouse event
    if (!optionClickIntercepted)
        QTextEdit::mouseMoveEvent(event);
}

void QtSLiMScriptTextEdit::mouseReleaseEvent(QMouseEvent *event)
{
    // forward to super, as long as we did not intercept this mouse event
    if (!optionClickIntercepted)
        QTextEdit::mouseReleaseEvent(event);
    
    optionClickIntercepted = false;
}































