#ifndef QTSLIMSCRIPTTEXTEDIT_H
#define QTSLIMSCRIPTTEXTEDIT_H

#include <QTextEdit>

class QtSLiMOutputHighlighter;
class QtSLiMScriptHighlighter;
class QStatusBar;


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
    ScriptType scriptType = ScriptType::NoScriptType;
    ScriptHighlightingType syntaxHighlightingType = QtSLiMTextEdit::NoHighlighting;
    QtSLiMOutputHighlighter *outputHighlighter = nullptr;
    QtSLiMScriptHighlighter *scriptHighlighter = nullptr;
    
    void selfInit(void);
    QStatusBar *statusBarForWindow(void);
    
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
    
protected slots:
    void displayFontPrefChanged();
    void scriptSyntaxHighlightPrefChanged();
    void outputSyntaxHighlightPrefChanged();
    bool checkScriptSuppressSuccessResponse(bool suppressSuccessResponse);
    void modifiersChanged(Qt::KeyboardModifiers newModifiers);
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






























