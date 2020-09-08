//
//  QtSLiMFindPanel.cpp
//  SLiM
//
//  Created by Ben Haller on 3/24/2020.
//  Copyright (c) 2020 Philipp Messer.  All rights reserved.
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


#include "QtSLiMFindPanel.h"
#include "ui_QtSLiMFindPanel.h"

#include <QTextEdit>
#include <QApplication>
#include <QSettings>
#include <QClipboard>
#include <QTextBlock>
#include <QDebug>

#include "QtSLiMAppDelegate.h"
#include "QtSLiMWindow.h"


QtSLiMFindPanel &QtSLiMFindPanel::instance(void)
{
    static QtSLiMFindPanel *inst = nullptr;
    
    if (!inst)
        inst = new QtSLiMFindPanel(nullptr);
    
    return *inst;
}

QtSLiMFindPanel::QtSLiMFindPanel(QWidget *parent) : QDialog(parent), ui(new Ui::QtSLiMFindPanel)
{
    ui->setupUi(this);
    
    QSettings settings;
    
    // no window icon
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    setWindowIcon(QIcon());
#endif
    
    // prevent this window from keeping the app running when all main windows are closed
    setAttribute(Qt::WA_QuitOnClose, false);
    
    // Connect the panel UI
    connect(ui->findNextButton, &QPushButton::clicked, this, &QtSLiMFindPanel::findNext);
    connect(ui->findPreviousButton, &QPushButton::clicked, this, &QtSLiMFindPanel::findPrevious);
    connect(ui->replaceAndFindButton, &QPushButton::clicked, this, &QtSLiMFindPanel::replaceAndFind);
    connect(ui->replaceButton, &QPushButton::clicked, this, &QtSLiMFindPanel::replace);
    connect(ui->replaceAllButton, &QPushButton::clicked, this, &QtSLiMFindPanel::replaceAll);
    
    connect(ui->matchCaseCheckBox, &QPushButton::clicked, this, &QtSLiMFindPanel::optionsChanged);
    connect(ui->wholeWordCheckBox, &QPushButton::clicked, this, &QtSLiMFindPanel::optionsChanged);
    connect(ui->wrapAroundCheckBox, &QPushButton::clicked, this, &QtSLiMFindPanel::optionsChanged);
    
    connect(ui->findTextLineEdit, &QLineEdit::textChanged, this, &QtSLiMFindPanel::findTextChanged);
    connect(ui->replaceTextLineEdit, &QLineEdit::textChanged, this, &QtSLiMFindPanel::replaceTextChanged);
    
    // Set up the find and replace fields
    ui->findTextLineEdit->setClearButtonEnabled(true);
    ui->replaceTextLineEdit->setClearButtonEnabled(true);
    
    changingFindText = true;
    ui->findTextLineEdit->clear();
    ui->replaceTextLineEdit->clear();
    changingFindText = false;
    
    // If Qt's clipboard supports a find buffer (currently macOS only), talk to it
    QClipboard *clipboard = QGuiApplication::clipboard();
    
    if (clipboard && clipboard->supportsFindBuffer())
    {
        // Note that this logs "QMime::convertToMime: unhandled mimetype: text/plain" in Qt 5.9.8 if the
        // find buffer is empty; there seems to be no way to avoid that log, so whatever
        QString findText = clipboard->text(QClipboard::FindBuffer);
        
        changingFindText = true;
        ui->findTextLineEdit->setText(findText);
        changingFindText = false;
        
        connect(clipboard, &QClipboard::findBufferChanged, this, &QtSLiMFindPanel::findBufferChanged);
    }
    else
    {
        ui->findTextLineEdit->setText(settings.value("QtSLiMFindPanel/findText", "").toString());
    }
    
    ui->replaceTextLineEdit->setText(settings.value("QtSLiMFindPanel/replaceText", "").toString());
    
    fixEnableState();
    
    // Restore saved options
    settings.beginGroup("QtSLiMFindPanel");
    ui->matchCaseCheckBox->setChecked(settings.value("matchCase", false).toBool());
    ui->wholeWordCheckBox->setChecked(settings.value("wholeWord", false).toBool());
    ui->wrapAroundCheckBox->setChecked(settings.value("wrapAround", true).toBool());
    settings.endGroup();
    
    // Clear the status text
    ui->statusText->clear();
    
    // The initial height should be enforced as the minimum and maximum height
    setMinimumHeight(height());
    setMaximumHeight(height());
    
    // Restore the saved window position; see https://doc.qt.io/qt-5/qsettings.html#details
    settings.beginGroup("QtSLiMFindPanel");
    resize(settings.value("size", QSize(width(), height())).toSize());
    move(settings.value("pos", QPoint(25, 45)).toPoint());
    settings.endGroup();
    
    // make window actions for all global menu items
    qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
}

QtSLiMFindPanel::~QtSLiMFindPanel(void)
{
    qDebug() << "QtSLiMFindPanel::~QtSLiMFindPanel()";
    
    delete ui;
}

QTextEdit *QtSLiMFindPanel::targetTextEditRequireModifiable(bool requireModifiable)
{
    // We rely on QtSLiMAppDelegate to track the active window list for us;
    // our target is the frontmost window that is not our own window
    QWidget *currentFocusWindow = qtSLiMAppDelegate->activeWindowExcluding(this);
    
    if (currentFocusWindow)
    {
        //qDebug() << "targetTextEditRequireModifiable() found active window" << currentFocusWindow->windowTitle();
        
        // Given a target window, we target the focusWidget *if* it is a textedit
        QWidget *focusWidget = currentFocusWindow->focusWidget();
        QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
        
//        if (focusWidget)
//            qDebug() << "   focusWidget" << focusWidget << " " << focusWidget->objectName() << ", textEdit" << textEdit;
//        else
//            qDebug() << "   NO FOCUSWIDGET";
        
        // If we've found a textedit, return it if it satisfies requirements
        // There is no fallback, nor should there be; the focused textedit is our target
        if (textEdit)
        {
            if (!textEdit->isEnabled())
                return nullptr;
            if (requireModifiable && textEdit->isReadOnly())
                return nullptr;
            
            return textEdit;
        }
    }
    else
    {
        //qDebug() << "targetTextEditRequireModifiable() : NO ACTIVE WINDOW";
    }
    
    return nullptr;
}

void QtSLiMFindPanel::showFindPanel(void)
{
    show();
    raise();
    activateWindow();
}

void QtSLiMFindPanel::closeEvent(QCloseEvent *event)
{
    // Save the window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMFindPanel");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
    
    // use super's default behavior
    QDialog::closeEvent(event);
}

bool QtSLiMFindPanel::findForwardWrapBeep(QTextEdit *target, bool forward, bool wrap, bool beepIfNotFound)
{
    // This method is the only place where I looked at its source code, but for this method at least,
    // thanks to Lorenzo Bettini for his QtFindReplaceDialog project, http://qtfindreplace.sourceforge.net
    // It is under the LGPL, so to the extent that I did lean on his code here, it is GPL-compatible.
    
    if (!target)
    {
        qDebug() << "QtSLiMFindPanel::find() called with no target textEdit!";
        return false;
    }
    
    QString findString = ui->findTextLineEdit->text();
    QTextDocument::FindFlags findFlags;
    
    if (!forward)
        findFlags |= QTextDocument::FindBackward;
    if (ui->matchCaseCheckBox->isChecked())
        findFlags |= QTextDocument::FindCaseSensitively;
    if (ui->wholeWordCheckBox->isChecked())
        findFlags |= QTextDocument::FindWholeWords;
    
    // There is a bug, fixed in Qt 5.12.5, where finding backwards fails to find the first occurrence
    // that it ought to find, in specific circumstances: the selection must start at the start of a
    // line, and the first previous occurrence must be in the preceding line.  The find() method gets
    // confused by the preceding block's end.  See https://bugreports.qt.io/browse/QTBUG-48035.  I do
    // not attempt to work around this bug here; the workaround would be a bit complex, and the bug
    // has been fixed, and it's unlikely to bite anyone – it's an edge case, and Find Previous is
    // relatively unusual.  But I've put this as a reminder, in case the bug gets reported to me.
    
    bool result = target->find(findString, findFlags);
    
    if (!result && wrap)
    {
        // If we're wrapping around, do the wrap and try again
        QTextCursor originalCursor(target->textCursor());
        
        if (forward)
            target->moveCursor(QTextCursor::Start);
        else
            target->moveCursor(QTextCursor::End);
        
        result = target->find(findString, findFlags);
        
        if (!result)
            target->setTextCursor(originalCursor);
    }
    
    if (!result)
    {
        ui->statusText->setText("no match found ");
        if (beepIfNotFound)
            qApp->beep();
    }
    
    return result;
}

void QtSLiMFindPanel::findNext(void)
{
    ui->statusText->clear();
    
    QTextEdit *target = targetTextEditRequireModifiable(false);
    QString findString = ui->findTextLineEdit->text();
    
    if (!target || !findString.length()) { qApp->beep(); return; }
    
    findForwardWrapBeep(target, true, ui->wrapAroundCheckBox->isChecked(), true);
}

void QtSLiMFindPanel::findPrevious(void)
{
    ui->statusText->clear();
    
    QTextEdit *target = targetTextEditRequireModifiable(false);
    QString findString = ui->findTextLineEdit->text();
    
    if (!target || !findString.length()) { qApp->beep(); return; }
    
    findForwardWrapBeep(target, false, ui->wrapAroundCheckBox->isChecked(), true);
}

void QtSLiMFindPanel::replaceAndFind(void)
{
    ui->statusText->clear();
    
    QTextEdit *target = targetTextEditRequireModifiable(true);
    QString findString = ui->findTextLineEdit->text();
    
    if (!target || !findString.length()) { qApp->beep(); return; }
    
    // if the selection is non-empty and equals the find string, replace; then find
    if (target->textCursor().hasSelection())
    {
        QString selectedText = target->textCursor().selectedText();
        
        if (0 == QString::compare(selectedText, ui->findTextLineEdit->text(), ui->matchCaseCheckBox->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive))
            target->textCursor().insertText(ui->replaceTextLineEdit->text());
    }
    
    findForwardWrapBeep(target, true, ui->wrapAroundCheckBox->isChecked(), true);
    jumpToSelection();
}

void QtSLiMFindPanel::replace(void)
{
    ui->statusText->clear();
    
    QTextEdit *target = targetTextEditRequireModifiable(true);
    QString findString = ui->findTextLineEdit->text();
    
    if (!target || !findString.length()) { qApp->beep(); return; }
    
    // beep if the selection is empty
    if (!target->textCursor().hasSelection()) { qApp->beep(); return; }
    
    target->textCursor().insertText(ui->replaceTextLineEdit->text());
}

void QtSLiMFindPanel::replaceAll(void)
{
    ui->statusText->clear();
    
    QTextEdit *target = targetTextEditRequireModifiable(true);
    QString findString = ui->findTextLineEdit->text();
    
    if (!target || !findString.length()) { qApp->beep(); return; }
    
    // Search from the document start
    QTextCursor originalCursor(target->textCursor());
    bool hasOccurrence = false;
    int replaceCount = 0;
    
    target->moveCursor(QTextCursor::Start);
    hasOccurrence = findForwardWrapBeep(target, true, false, true);   // beeps if none found
    
    // Then, assuming we found at least one occurrence, loop replacing and finding
    if (hasOccurrence)
    {
        target->textCursor().beginEditBlock();      // make this a single undoable action
        
        while (hasOccurrence)
        {
            target->textCursor().insertText(ui->replaceTextLineEdit->text());
            replaceCount++;
            hasOccurrence = findForwardWrapBeep(target, true, false, false);
        }
        
        target->textCursor().endEditBlock();        // undoable action ends
    }
    
    // Restore the original cursor as closely as we can
    target->setTextCursor(originalCursor);
    jumpToSelection();
    
    // show the replacement count
    ui->statusText->setText(QString("replaced %1 occurrence%2 ").arg(replaceCount).arg(replaceCount == 1 ? "" : "s"));
}

void QtSLiMFindPanel::useSelectionForFind(void)
{
    ui->statusText->clear();
    
    QTextEdit *target = targetTextEditRequireModifiable(false);
    QString selectionString = target->textCursor().selectedText();
    
    if (selectionString.length())
        ui->findTextLineEdit->setText(selectionString);     // this will trigger QtSLiMFindPanel::findTextChanged()
    else
        qApp->beep();
}

void QtSLiMFindPanel::useSelectionForReplace(void)
{
    ui->statusText->clear();
    
    QTextEdit *target = targetTextEditRequireModifiable(false);
    QString selectionString = target->textCursor().selectedText();
    
    if (selectionString.length())
        ui->replaceTextLineEdit->setText(selectionString);     // this will trigger QtSLiMFindPanel::replaceTextChanged()
    else
        qApp->beep();
}

void QtSLiMFindPanel::jumpToSelection(void)
{
    ui->statusText->clear();
    
    QTextEdit *target = targetTextEditRequireModifiable(false);
    
    // ensureCursorVisible() doesn't do a good job with full-line selections, so we temporarily change the selection
    QTextCursor savedCursor(target->textCursor());
    QTextCursor positionCursor(savedCursor);
    QTextCursor anchorCursor(savedCursor);
    
    positionCursor.setPosition(savedCursor.position());
    anchorCursor.setPosition(savedCursor.anchor());
    
    target->setTextCursor(positionCursor);
    target->ensureCursorVisible();
    target->setTextCursor(anchorCursor);
    target->ensureCursorVisible();
    
    // restore the user's selection
    target->setTextCursor(savedCursor);
}

void QtSLiMFindPanel::jumpToLine(void)
{
    QTextEdit *target = targetTextEditRequireModifiable(false);
    
    int lineIndex = target->textCursor().block().blockNumber();
    
    QStringList choices = QtSLiMRunLineEditArrayDialog(target->window(), "Jump to Line:",
                                                       QStringList{"Line number:"},
                                                       QStringList{QString::number(lineIndex)});
    
    if (choices.length() == 1)
    {
        lineIndex = choices[0].toInt();
        
        if (lineIndex < 1) { lineIndex = 1; qApp->beep(); }
        if (lineIndex > target->document()->blockCount()) { lineIndex = target->document()->blockCount(); qApp->beep(); }
        
        QTextCursor lineCursor(target->document());
        
        lineCursor.setPosition(0);
        lineCursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, lineIndex - 1);
        
        target->setTextCursor(lineCursor);
        target->ensureCursorVisible();
    }
}

void QtSLiMFindPanel::findBufferChanged(void)
{
    // If the clipboard's find buffer changes, we need to (1) update the find lineEdit, and (2) update our status text
    
    // We use changingFindText to avoid responding to find text changes we cause ourselves
    if (!changingFindText)
    {
        QClipboard *clipboard = QGuiApplication::clipboard();
        
        if (clipboard && clipboard->supportsFindBuffer())
        {
            QString findText = clipboard->text(QClipboard::FindBuffer);
            
            changingFindText = true;
            ui->findTextLineEdit->setText(findText);
            changingFindText = false;
            
            ui->statusText->clear();
            fixEnableState();
        }
    }
}

void QtSLiMFindPanel::findTextChanged(void)
{
    // If the find text lineEdit changes, we need to (1) update the clipboard, and (2) update our status text
    
    // We use changingFindText to avoid responding to find text changes we cause ourselves
    if (!changingFindText)
    {
        QString findText = ui->findTextLineEdit->text();
        
        if (findText.length())  // don't change the find buffer if we have no find text
        {
            QClipboard *clipboard = QGuiApplication::clipboard();
            
            if (clipboard && clipboard->supportsFindBuffer())
            {
                changingFindText = true;
                clipboard->setText(findText, QClipboard::FindBuffer);
                changingFindText = false;
            }
            else
            {
                QSettings settings;
                
                settings.setValue("QtSLiMFindPanel/findText", findText);
            }
        }
        
        ui->statusText->clear();
        fixEnableState();
    }
}

void QtSLiMFindPanel::replaceTextChanged(void)
{
    ui->statusText->clear();
    
    // Save the replace string to prefs; unlike findTextChanged() we do this even when the replace string is zero-length
    QSettings settings;
    
    settings.setValue("QtSLiMFindPanel/replaceText", ui->replaceTextLineEdit->text());
}

void QtSLiMFindPanel::optionsChanged(void)
{
    ui->statusText->clear();
    
    // When the search options change, we need to write options to prefs
    QSettings settings;
    
    settings.beginGroup("QtSLiMFindPanel");
    settings.setValue("matchCase", ui->matchCaseCheckBox->isChecked());
    settings.setValue("wholeWord", ui->wholeWordCheckBox->isChecked());
    settings.setValue("wrapAround", ui->wrapAroundCheckBox->isChecked());
    settings.endGroup();
}

void QtSLiMFindPanel::fixEnableState(void)
{
    bool hasFindText = (ui->findTextLineEdit->text().length() > 0);
    bool hasTarget = (QtSLiMFindPanel::targetTextEditRequireModifiable(false) != nullptr);
    bool hasModifiableTarget = (QtSLiMFindPanel::targetTextEditRequireModifiable(true) != nullptr);
    
    ui->findNextButton->setEnabled(hasFindText && hasTarget);
    ui->findPreviousButton->setEnabled(hasFindText && hasTarget);
    ui->replaceAndFindButton->setEnabled(hasFindText && hasModifiableTarget);    
    ui->replaceButton->setEnabled(hasFindText && hasModifiableTarget);    
    ui->replaceAllButton->setEnabled(hasFindText && hasModifiableTarget);    
}




































