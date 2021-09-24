//
//  QtSLiMDebugOutputWindow.cpp
//  SLiM
//
//  Created by Ben Haller on 02/06/2021.
//  Copyright (c) 2021 Philipp Messer.  All rights reserved.
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

#ifdef _WIN32
#include "config.h"
#endif

#include "QtSLiMDebugOutputWindow.h"
#include "ui_QtSLiMDebugOutputWindow.h"

#include <QSettings>
#include <QDebug>

#include "QtSLiMWindow.h"
#include "QtSLiMEidosConsole.h"
#include "QtSLiMAppDelegate.h"


//
//  QtSLiMDebugOutputWindow
//

QtSLiMDebugOutputWindow::QtSLiMDebugOutputWindow(QtSLiMWindow *p_parent) :
    QWidget(p_parent, Qt::Window),    // the console window has us as a parent, but is still a standalone window
    parentSLiMWindow(p_parent),
    ui(new Ui::QtSLiMDebugOutputWindow)
{
    ui->setupUi(this);
    
    // no window icon
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    setWindowIcon(QIcon());
#endif
    
    // prevent this window from keeping the app running when all main windows are closed
    setAttribute(Qt::WA_QuitOnClose, false);
    
    // Restore the saved window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMDebugOutputWindow");
    resize(settings.value("size", QSize(400, 300)).toSize());
    move(settings.value("pos", QPoint(25, 445)).toPoint());
    settings.endGroup();
    
    // glue UI; no separate file since this is very simple
    connect(ui->clearOutputButton, &QPushButton::clicked, this, &QtSLiMDebugOutputWindow::clearOutputClicked);
    ui->clearOutputButton->qtslimSetBaseName("delete");
    connect(ui->clearOutputButton, &QPushButton::pressed, this, &QtSLiMDebugOutputWindow::clearOutputPressed);
    connect(ui->clearOutputButton, &QPushButton::released, this, &QtSLiMDebugOutputWindow::clearOutputReleased);
    
    // QtSLiMTextEdit attributes
    ui->debugOutputTextEdit->setOptionClickEnabled(false);
    ui->debugOutputTextEdit->setCodeCompletionEnabled(false);
    ui->debugOutputTextEdit->setScriptType(QtSLiMTextEdit::NoScriptType);
    ui->debugOutputTextEdit->setSyntaxHighlightType(QtSLiMTextEdit::OutputHighlighting);
    ui->debugOutputTextEdit->setReadOnly(true);
    
    // make window actions for all global menu items
    qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
}

QtSLiMDebugOutputWindow::~QtSLiMDebugOutputWindow()
{
    delete ui;
}

QtSLiMTextEdit *QtSLiMDebugOutputWindow::debugOutputTextView(void)
{
    return ui->debugOutputTextEdit;
}

void QtSLiMDebugOutputWindow::clearOutputClicked(void)
{
    ui->debugOutputTextEdit->setPlainText("");
}

void QtSLiMDebugOutputWindow::clearOutputPressed(void)
{
    ui->clearOutputButton->qtslimSetHighlight(true);
}

void QtSLiMDebugOutputWindow::clearOutputReleased(void)
{
    ui->clearOutputButton->qtslimSetHighlight(false);
}

void QtSLiMDebugOutputWindow::closeEvent(QCloseEvent *p_event)
{
    // Save the window position; see https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;
    
    settings.beginGroup("QtSLiMDebugOutputWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.endGroup();
    
    // send our close signal
    emit willClose();
    
    // use super's default behavior
    QWidget::closeEvent(p_event);
}

































