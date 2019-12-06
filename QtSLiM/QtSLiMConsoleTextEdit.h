#ifndef QTSLIMCONSOLETEXTEDIT_H
#define QTSLIMCONSOLETEXTEDIT_H

#include <QTextEdit>
#include <QTextCharFormat>
#include <QColor>
#include <QStringList>

#include "QtSLiMScriptTextEdit.h"   // defines our superclass, QtSLiMTextEdit


class QtSLiMConsoleTextEdit : public QtSLiMTextEdit
{
    Q_OBJECT
    
public:
    QtSLiMConsoleTextEdit(const QString &text, QWidget *parent = nullptr);
    QtSLiMConsoleTextEdit(QWidget *parent = nullptr);
    ~QtSLiMConsoleTextEdit() override;
    
    static QTextCharFormat textFormatForColor(QColor color);
    
    // Show the initial welcome message
    void showWelcome(void);
    
    // Prompts and spacers
    void showPrompt(QChar promptChar);
    void showPrompt(void);
    void showContinuationPrompt(void);
    void clearToPrompt(void);
    void appendSpacer(void);
    
    // Commands and history
    void appendExecution(QString result, QString errorString, QString tokenString, QString parseString, QString executionString);
    QString currentCommandAtPrompt(void);
    void setCommandAtPrompt(QString newCommand);
    void registerNewHistoryItem(QString newItem);
    
public slots:
    void previousHistory(void);
    void nextHistory(void);
    void executeCurrentPrompt(void);
    
signals:
    void executeScript(QString script);
    
protected:
    void selfInit(void);
    void keyPressEvent(QKeyEvent *event) override;
    
    // handling input prompts and continuation
    QTextCursor lastPromptCursor;
    bool isContinuationPrompt = false;
    int originalPromptEnd = 0;
    
    void elideContinuationPrompt(void);
    QString fullInputString(void);
    
    // handling the command history
    QStringList history;
    int historyIndex = 0;
    bool lastHistoryItemIsProvisional = false;	// got added to the history by a moveUp: event but is not an executed command
    
    // handling the selection and editability
    bool insideMouseTracking = false, sawSelectionChange = false;
    
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void handleSelectionChanged(void);
    void adjustSelection(void);
    void dragMoveEvent(QDragMoveEvent *event) override;
    
signals:
    void selectionWasChangedDuringLastEvent(void);
};


#endif // QTSLIMCONSOLETEXTEDIT_H

































