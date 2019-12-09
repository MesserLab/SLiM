//
//  QtSLiMScriptTextEdit.h
//  SLiM
//
//  Created by Ben Haller on 11/24/2019.
//  Copyright (c) 2019 Philipp Messer.  All rights reserved.
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

#ifndef QTSLIMSCRIPTTEXTEDIT_H
#define QTSLIMSCRIPTTEXTEDIT_H

#include <QTextEdit>

#include "eidos_interpreter.h"

class QtSLiMOutputHighlighter;
class QtSLiMScriptHighlighter;
class QStatusBar;
class EidosCallSignature;
class QtSLiMWindow;


// A QTextEdit subclass that provides a signal for an option-click on a script symbol
// This also provides some basic smarts for syntax coloring and so forth
// QtSLiMScriptTextEdit and QtSLiMConsoleTextEdit are both subclasses of this class
class QtSLiMTextEdit : public QTextEdit
{
    Q_OBJECT
    
public:
    enum ScriptType {
        NoScriptType = 0,
        EidosScriptType,
        SLiMScriptType
    };
    enum ScriptHighlightingType {
        NoHighlighting = 0,
        ScriptHighlighting,
        OutputHighlighting
    };
    
    QtSLiMTextEdit(const QString &text, QWidget *parent = nullptr);
    QtSLiMTextEdit(QWidget *parent = nullptr);
    ~QtSLiMTextEdit() override;
    
    // configuration
    void setScriptType(ScriptType type);
    void setSyntaxHighlightType(ScriptHighlightingType type);
    void setOptionClickEnabled(bool enabled);
    
    // highlight errors
    void highlightError(int startPosition, int endPosition);   
    void selectErrorRange(void);
    
public slots:
    void checkScript(void);
    void prettyprint(void);
    
signals:
    
protected:
    bool basedOnLiveSimulation = false;
    ScriptType scriptType = ScriptType::NoScriptType;
    ScriptHighlightingType syntaxHighlightingType = QtSLiMTextEdit::NoHighlighting;
    QtSLiMOutputHighlighter *outputHighlighter = nullptr;
    QtSLiMScriptHighlighter *scriptHighlighter = nullptr;
    
    void selfInit(void);
    QStatusBar *statusBarForWindow(void);
    QtSLiMWindow *slimControllerForWindow(void);
    
    // used to track that we are intercepting a mouse event
    bool optionClickEnabled = false;
    bool optionClickIntercepted = false;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void scriptHelpOptionClick(QString searchString);
    
    // used to maintain the correct cursor (pointing hand when option is pressed)
    void fixMouseCursor(void);
    void enterEvent(QEvent *event) override;
    
    // status bar signatures
    const EidosCallSignature *signatureForFunctionName(QString callName, EidosFunctionMap *functionMapPtr);
    const std::vector<const EidosMethodSignature*> *slimguiAllMethodSignatures(void);
    const EidosCallSignature *signatureForMethodName(QString callName);
    EidosFunctionMap *functionMapForTokenizedScript(EidosScript &script);
    QString signatureStringForScriptSelection(QString &callName);
    
    // virtual function for accessing the script portion of the contents; for normal
    // script textedits this is the whole content and te text cursor, but for the
    // console view it is just the snippet of script following the prompt
    virtual void scriptStringAndSelection(QString &scriptString, int &pos, int &len);
    
protected slots:
    void displayFontPrefChanged();
    void scriptSyntaxHighlightPrefChanged();
    void outputSyntaxHighlightPrefChanged();
    bool checkScriptSuppressSuccessResponse(bool suppressSuccessResponse);
    void modifiersChanged(Qt::KeyboardModifiers newModifiers);
    
    void updateStatusFieldFromSelection(void);
};

// A QtSLiMTextEdit subclass that provides various smarts for editing Eidos script
class QtSLiMScriptTextEdit : public QtSLiMTextEdit
{
    Q_OBJECT
    
public:
    QtSLiMScriptTextEdit(const QString &text, QWidget *parent = nullptr) : QtSLiMTextEdit(text, parent) {}
    QtSLiMScriptTextEdit(QWidget *parent = nullptr) : QtSLiMTextEdit(parent) {}
    ~QtSLiMScriptTextEdit() override;
    
public slots:
    void shiftSelectionLeft(void);
    void shiftSelectionRight(void);
    void commentUncommentSelection(void);
    
protected:
    QStringList linesForRoundedSelection(QTextCursor &cursor, bool &movedBack);
};


#endif // QTSLIMSCRIPTTEXTEDIT_H






























