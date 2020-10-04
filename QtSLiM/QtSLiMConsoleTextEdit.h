//
//  QtSLiMConsoleTextEdit.h
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
    virtual ~QtSLiMConsoleTextEdit() override;
    
    static QTextCharFormat textFormatForColor(QColor color);
    
    // Show the initial welcome message
    void showWelcome(void);
    
    // Prompts and spacers
    void showPrompt(QChar promptChar);
    void showPrompt(void);
    void showContinuationPrompt(void);
    void clearToPrompt(void);
    
    // Commands and history
    void appendExecution(QString result, QString errorString, QString tokenString, QString parseString, QString executionString);
    QString currentCommandAtPrompt(void);
    void setCommandAtPrompt(QString newCommand);
    void registerNewHistoryItem(QString newItem);
    
    // Fix the selection to be legal, and adjust the read-only setting accordingly
    void adjustSelectionAndReadOnly(void);
    
    // Read-only state handling; this is a consequence of two independent flags, for us
    void setReadOnlyDueToSelection(bool flag) { roDTS = flag; updateReadOnly(); }
    void setReadOnlyDueToActivation(bool flag) { roDTA = flag; updateReadOnly(); }
    void updateReadOnly(void) { setReadOnly(roDTS | roDTA); }
    
public slots:
    void previousHistory(void);
    void nextHistory(void);
    void executeCurrentPrompt(void);
    
signals:
    void executeScript(QString script);
    
protected:
    void selfInit(void);
    virtual void keyPressEvent(QKeyEvent *event) override;
    
    // handling input prompts and continuation
    QTextCursor lastPromptCursor;
    bool isContinuationPrompt = false;
    int originalPromptEnd = 0;
    
    void elideContinuationPrompt(void);
    QString fullInputString(void);
    
    virtual void scriptStringAndSelection(QString &scriptString, int &pos, int &len) override;
    
    // handling the command history
    QStringList history;
    int historyIndex = 0;
    bool lastHistoryItemIsProvisional = false;	// got added to the history by a moveUp: event but is not an executed command
    
    // handling read-only status
    bool roDTS = false;
    bool roDTA = false;
    
    // handling the selection and editability
    bool insideMouseTracking = false, sawSelectionChange = false;
    
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    void handleSelectionChanged(void);
    virtual void dragMoveEvent(QDragMoveEvent *event) override;
    
signals:
    void selectionWasChangedDuringLastEvent(void);
};


#endif // QTSLIMCONSOLETEXTEDIT_H

































