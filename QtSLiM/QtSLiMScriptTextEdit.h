//
//  QtSLiMScriptTextEdit.h
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

#ifndef QTSLIMSCRIPTTEXTEDIT_H
#define QTSLIMSCRIPTTEXTEDIT_H

#include <QPlainTextEdit>
#include <QPalette>

#include "eidos_interpreter.h"
#include "eidos_type_interpreter.h"

class QtSLiMOutputHighlighter;
class QtSLiMScriptHighlighter;
class QStatusBar;
class EidosCallSignature;
class QtSLiMWindow;
class QtSLiMEidosConsole;
class QCompleter;
class QHelpEvent;
class QPaintEvent;
class QMouseEvent;

class Species;


// We define NSRange and NSNotFound because we want to be able to represent "no range",
// which QTextCursor cannot do; there is no equivalent of NSNotFound for it.  We should
// never be linked with Objective-C code, so there should be no conflict.  Note these
// definitions are not the same as Apple's; I kept the names just for convenience.

typedef struct _NSRange {
    int location;
    int length;
} NSRange;

static const int NSNotFound = INT_MAX;


// A QPlainTextEdit subclass that provides a signal for an option-click on a script
// symbol.  This also provides some basic smarts for syntax coloring and so forth.
// QtSLiMScriptTextEdit and QtSLiMConsoleTextEdit are both subclasses of this class.
class QtSLiMTextEdit : public QPlainTextEdit
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
    
    QtSLiMTextEdit(const QString &text, QWidget *p_parent = nullptr);
    QtSLiMTextEdit(QWidget *p_parent = nullptr);
    virtual ~QtSLiMTextEdit() override;
    
    // configuration
    void setScriptType(ScriptType type);
    void setSyntaxHighlightType(ScriptHighlightingType type);
    void setOptionClickEnabled(bool enabled);
    void setCodeCompletionEnabled(bool enabled);
    
    // highlight errors
    void highlightError(int startPosition, int endPosition);   
    void selectErrorRange(EidosErrorContext &errorContext);
    QPalette qtslimStandardPalette(void);
    QPalette qtslimErrorPalette(void);
    
    // undo/redo availability; named to avoid future collision with QPlainTextEdit for this obvious feature
    bool qtslimIsUndoAvailable(void) { return undoAvailable_; }
    bool qtslimIsRedoAvailable(void) { return redoAvailable_; }
    bool qtslimIsCopyAvailable(void) { return copyAvailable_; }
    
public slots:
    void checkScript(void);
    void prettyprint(void);
    void reformat(void);
    void prettyprintClicked(void);      // decides whether to reformat or prettyprint based on the option key state
    
signals:
    
protected:
    ScriptType scriptType = ScriptType::NoScriptType;
    ScriptHighlightingType syntaxHighlightingType = QtSLiMTextEdit::NoHighlighting;
    QtSLiMOutputHighlighter *outputHighlighter = nullptr;
    QtSLiMScriptHighlighter *scriptHighlighter = nullptr;
    
    void selfInit(void);
    QStatusBar *statusBarForWindow(void);
    QtSLiMWindow *slimControllerForWindow(void);
    QtSLiMEidosConsole *slimEidosConsoleForWindow(void);
    
    // used to track that we are intercepting a mouse event
    bool optionClickEnabled = false;
    bool optionClickIntercepted = false;
    virtual void mousePressEvent(QMouseEvent *p_event) override;
    virtual void mouseMoveEvent(QMouseEvent *p_event) override;
    virtual void mouseReleaseEvent(QMouseEvent *p_event) override;
    void scriptHelpOptionClick(QString searchString);
    
    // used to maintain the correct cursor (pointing hand when option is pressed)
    void fixMouseCursor(void);
    virtual void enterEvent(QEvent *p_event) override;
    
    // keeping track of undo/redo availability
    bool undoAvailable_ = false;
    bool redoAvailable_ = false;
    bool copyAvailable_ = false;
    
    // used for code completion in script textviews
    bool codeCompletionEnabled = false;
    QCompleter *completer = nullptr;
    
    void autoindentAfterNewline(void);
    virtual void keyPressEvent(QKeyEvent *e) override;
    QStringList completionsForPartialWordRange(NSRange charRange, int *indexOfSelectedItem);
    NSRange rangeForUserCompletion(void);
    
    QStringList globalCompletionsWithTypesFunctionsKeywordsArguments(EidosTypeTable *typeTable, EidosFunctionMap *functionMap, QStringList keywords, QStringList argumentNames);
    QStringList completionsForKeyPathEndingInTokenIndexOfTokenStream(int lastDotTokenIndex, const std::vector<EidosToken> &tokens, EidosTypeTable *typeTable, EidosFunctionMap *functionMap, EidosCallTypeTable *callTypeTable, QStringList keywords);
    QStringList completionsFromArrayMatchingBase(QStringList candidates, QString base);
    QStringList completionsForTokenStream(const std::vector<EidosToken> &tokens, int lastTokenIndex, bool canExtend, EidosTypeTable *typeTable, EidosFunctionMap *functionMap, EidosCallTypeTable *callTypeTable, QStringList keywords, QStringList argumentNames);
    QStringList uniquedArgumentNameCompletions(std::vector<std::string> *argumentCompletions);
    void _completionHandlerWithRangeForCompletion(NSRange *baseRange, QStringList *completions);
    int64_t scoreForCandidateAsCompletionOfString(QString candidate, QString base);
    void slimSpecificCompletion(QString completionScriptString, NSRange selection, EidosTypeTable **typeTable, EidosFunctionMap **functionMap, EidosCallTypeTable **callTypeTable, QStringList *keywords, std::vector<std::string> *argNameCompletions);

    EidosFunctionMap *functionMapForScriptString(QString scriptString, bool includingOptionalFunctions);
    EidosFunctionMap *functionMapForTokenizedScript(EidosScript &script, bool includingOptionalFunctions);
    EidosSymbolTable *symbolsFromBaseSymbols(EidosSymbolTable *baseSymbols);
    
    // status bar signatures
    EidosFunctionSignature_CSP signatureForFunctionName(QString callName, EidosFunctionMap *functionMapPtr);
    const std::vector<EidosMethodSignature_CSP> *slimguiAllMethodSignatures(void);
    EidosMethodSignature_CSP signatureForMethodName(QString callName);
    EidosCallSignature_CSP signatureForScriptSelection(QString &callName);
    
    // virtual function for accessing the script portion of the contents; for normal
    // script textedits this is the whole content and the text cursor, but for the
    // console view it is just the snippet of script following the prompt
    virtual void scriptStringAndSelection(QString &scriptString, int &pos, int &len, int &offset);
    
protected slots:
    virtual void displayFontPrefChanged();
    void scriptSyntaxHighlightPrefChanged();
    void outputSyntaxHighlightPrefChanged();
    bool checkScriptSuppressSuccessResponse(bool suppressSuccessResponse);
    void modifiersChanged(Qt::KeyboardModifiers newModifiers);
    
    void updateStatusFieldFromSelection(void);
    
    void insertCompletion(const QString &completion);
    void _prettyprint_reformat(bool reformat);
};

// A QtSLiMTextEdit subclass that provides various smarts for editing Eidos script
class QtSLiMScriptTextEdit : public QtSLiMTextEdit
{
    Q_OBJECT
    
public:
    QtSLiMScriptTextEdit(const QString &text, QWidget *p_parent = nullptr);
    QtSLiMScriptTextEdit(QWidget *p_parent = nullptr);
    virtual ~QtSLiMScriptTextEdit() override;
    
    EidosInterpreterDebugPointsSet &debuggingPoints(void) { return enabledBugLines; }
    
    void clearScriptBlockColoring(void);
    void addScriptBlockColoring(int startPos, int endPos, Species *species);
    
public slots:
    void shiftSelectionLeft(void);
    void shiftSelectionRight(void);
    void commentUncommentSelection(void);
    void clearDebugPoints(void);
    
protected:
    QStringList linesForRoundedSelection(QTextCursor &cursor, bool &movedBack);
    
    // From here down is the machinery for providing line numbers
    // This code is adapted from https://doc.qt.io/qt-5/qtwidgets-widgets-codeeditor-example.html
public:
    void lineNumberAreaToolTipEvent(QHelpEvent *p_helpEvent);
    void lineNumberAreaPaintEvent(QPaintEvent *p_paintEvent);
    void lineNumberAreaMouseEvent(QMouseEvent *p_mouseEvent);
    void lineNumberAreaContextMenuEvent(QContextMenuEvent *p_event);
    void lineNumberAreaWheelEvent(QWheelEvent *p_wheelEvent);
    int lineNumberAreaWidth(void);

protected:
    void sharedInit(void);
    void initializeLineNumbers(void);
    virtual void resizeEvent(QResizeEvent *p_event) override;

protected slots:
    virtual void displayFontPrefChanged() override;
    
private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine(void);
    void updateLineNumberArea(QRectF /*rect_f*/) { updateLineNumberArea(); }
    void updateLineNumberArea(int /*slider_pos*/) { updateLineNumberArea(); }
    void updateLineNumberArea(void);
    void updateDebugPoints(void);
    void controllerChangeCountChanged(int changeCount);
    void controllerTickFinished(void);

private:
    QWidget *lineNumberArea;
    
    // Debug points
    int lineNumberAreaBugWidth = 0;
    std::vector<QTextCursor> bugCursors;                // we use QTextCursor to maintain the positions of debugging points across edits
    EidosInterpreterDebugPointsSet bugLines;            // line numbers for debug points; std::unordered_set<int> or equivalent
    EidosInterpreterDebugPointsSet enabledBugLines;     // for public consumption; empty when debug points are disabled
    bool debugPointsEnabled = true;                     // disabled when edited without a recycle (recycle button is green)
    bool coloringDebugPointCursors = false;             // a flag to prevent re-entrancy
    
    void toggleDebuggingForLine(int lineNumber);
    
    // Species coloring
    std::vector<QTextCursor> blockCursors;              // we use QTextCursor to maintain the positions of script blocks across edits
    std::vector<Species *> blockSpecies;                // the corresponding Species object for each block
};


#endif // QTSLIMSCRIPTTEXTEDIT_H






























