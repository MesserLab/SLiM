//
//  QtSLiMScriptTextEdit.h
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

#ifndef QTSLIMSCRIPTTEXTEDIT_H
#define QTSLIMSCRIPTTEXTEDIT_H

#include <QTextEdit>

#include "eidos_interpreter.h"
#include "eidos_type_interpreter.h"

class QtSLiMOutputHighlighter;
class QtSLiMScriptHighlighter;
class QStatusBar;
class EidosCallSignature;
class QtSLiMWindow;
class QtSLiMEidosConsole;
class QCompleter;


// We define NSRange and NSNotFound because we want to be able to represent "no range",
// which QTextCursor cannot do; there is no equivalent of NSNotFound for it.  We should
// never be linked with Objective-C code, so there should be no conflict.  Note these
// definitions are not the same as Apple's; I kept the names just for convenience.

typedef struct _NSRange {
    int location;
    int length;
} NSRange;

static const int NSNotFound = INT_MAX;


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
    virtual ~QtSLiMTextEdit() override;
    
    // configuration
    void setScriptType(ScriptType type);
    void setSyntaxHighlightType(ScriptHighlightingType type);
    void setOptionClickEnabled(bool enabled);
    void setCodeCompletionEnabled(bool enabled);
    
    // highlight errors
    void highlightError(int startPosition, int endPosition);   
    void selectErrorRange(void);
    
    // undo/redo availability; named to avoid future collision with QTextEdit for this obvious feature
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
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    void scriptHelpOptionClick(QString searchString);
    
    // used to maintain the correct cursor (pointing hand when option is pressed)
    void fixMouseCursor(void);
    virtual void enterEvent(QEvent *event) override;
    
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
    int rangeOffsetForCompletionRange(void);
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
    virtual void scriptStringAndSelection(QString &scriptString, int &pos, int &len);
    
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
    QtSLiMScriptTextEdit(const QString &text, QWidget *parent = nullptr);
    QtSLiMScriptTextEdit(QWidget *parent = nullptr);
    virtual ~QtSLiMScriptTextEdit() override;
    
public slots:
    void shiftSelectionLeft(void);
    void shiftSelectionRight(void);
    void commentUncommentSelection(void);
    
protected:
    QStringList linesForRoundedSelection(QTextCursor &cursor, bool &movedBack);
    
    // From here down is the machinery for providing line numbers
    // This code is adapted from https://doc.qt.io/qt-5/qtwidgets-widgets-codeeditor-example.html
public:
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth(void);

protected:
    void initializeLineNumbers(void);
    virtual void resizeEvent(QResizeEvent *event) override;

protected slots:
    virtual void displayFontPrefChanged() override;
    
private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine(void);
    void updateLineNumberArea(QRectF /*rect_f*/) { updateLineNumberArea(); }
    void updateLineNumberArea(int /*slider_pos*/) { updateLineNumberArea(); }
    void updateLineNumberArea(void);

private:
    QWidget *lineNumberArea;
};


#endif // QTSLIMSCRIPTTEXTEDIT_H






























