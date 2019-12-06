#ifndef QTSLIMSCRIPTTEXTEDIT_H
#define QTSLIMSCRIPTTEXTEDIT_H

#include <QTextEdit>


// A QTextEdit subclass that provides a signal for an option-click on a script symbol
class QtSLiMTextEdit : public QTextEdit
{
    Q_OBJECT
    
public:
    QtSLiMTextEdit(const QString &text, QWidget *parent = nullptr) : QTextEdit(text, parent) {}
    QtSLiMTextEdit(QWidget *parent = nullptr) : QTextEdit(parent) {}
    ~QtSLiMTextEdit() override;
    
signals:
    void optionClickOnSymbol(QString symbol);
    
protected:
    // used to track that we are intercepting a mouse event
    bool optionClickIntercepted = false;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    
    // used to maintain the correct cursor (pointing hand when option is pressed)
    void fixMouseCursor(void);
    void enterEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
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






























