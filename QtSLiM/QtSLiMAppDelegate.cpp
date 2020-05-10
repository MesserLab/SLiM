//
//  QtSLiMAppDelegate.cpp
//  SLiM
//
//  Created by Ben Haller on 7/13/2019.
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


#include "QtSLiMAppDelegate.h"
#include "QtSLiMWindow.h"
#include "QtSLiMFindRecipe.h"
#include "QtSLiM_SLiMgui.h"
#include "QtSLiMScriptTextEdit.h"
#include "QtSLiMAbout.h"
#include "QtSLiMPreferences.h"
#include "QtSLiMHelpWindow.h"
#include "QtSLiMFindPanel.h"
#include "QtSLiMEidosConsole.h"
#include "QtSLiMVariableBrowser.h"
#include "QtSLiMConsoleTextEdit.h"

#include <QApplication>
#include <QOpenGLWidget>
#include <QSurfaceFormat>
#include <QMenu>
#include <QAction>
#include <QDir>
#include <QCollator>
#include <QKeyEvent>
#include <QtGlobal>
#include <QDebug>

#include <stdio.h>
#include <unistd.h>

#include "eidos_globals.h"
#include "eidos_beep.h"
#include "slim_globals.h"


// Check the Qt version and display an error if it is unacceptable
// We enforce Qt 5.9.5 as a hard limit, since it is what Ubuntu 18.04 LTS has preinstalled
#if (QT_VERSION < 0x050905)
#error "QtSLiM requires Qt version 5.9.5 or later.  Please uninstall Qt and then install a more recent version (5.12 LTS recommended)."
#endif


static std::string Eidos_Beep_QT(std::string p_sound_name);


QtSLiMAppDelegate *qtSLiMAppDelegate = nullptr;

QtSLiMAppDelegate::QtSLiMAppDelegate(QObject *parent) : QObject(parent)
{
    // Determine whether we were launched from a shell or from something else (Finder, Xcode, etc.)
	launchedFromShell_ = (isatty(fileno(stdin)) == 1); // && !SLiM_AmIBeingDebugged();
    
    // Install our custom beep handler
    Eidos_Beep = &Eidos_Beep_QT;
    
    // Let Qt know who we are, for QSettings configuration
    QCoreApplication::setOrganizationName("MesserLab");
    QCoreApplication::setOrganizationDomain("MesserLab.edu");   // Qt expects the domain in the standard order, and reverses it to form "edu.messerlab.QtSLiM.plist" as per Apple's usage
    QCoreApplication::setApplicationName("QtSLiM");
    QCoreApplication::setApplicationVersion(SLIM_VERSION_STRING);
    
    // Warm up our back ends before anything else happens
    Eidos_WarmUp();
    SLiM_WarmUp();
    gEidosContextClasses.push_back(gSLiM_SLiMgui_Class);			// available only in SLiMgui
    Eidos_FinishWarmUp();

    // Remember our current working directory, to return to whenever we are not inside SLiM/Eidos
    app_cwd_ = Eidos_CurrentDirectory();

    // Set up the format for OpenGL buffers globally, so that it applies to all windows and contexts
    // This defaults to OpenGL 2.0, which is what we want, so right now we don't customize
    QSurfaceFormat format;
    //format.setDepthBufferSize(24);
    //format.setStencilBufferSize(8);
    //format.setVersion(3, 2);
    //format.setProfile(QSurfaceFormat::CompatibilityProfile);
    QSurfaceFormat::setDefaultFormat(format);
    
    // Connect to the app to find out when we're terminating
    QApplication *app = qApp;

    connect(app, &QApplication::lastWindowClosed, this, &QtSLiMAppDelegate::lastWindowClosed);
    connect(app, &QApplication::aboutToQuit, this, &QtSLiMAppDelegate::aboutToQuit);

    // Install our event filter to detect modifier key changes
    app->installEventFilter(this);
    
    // Track the last focused main window, for activeQtSLiMWindow()
    connect(app, &QApplication::focusChanged, this, &QtSLiMAppDelegate::focusChanged);
    
    // We assume we are the global instance; FIXME singleton pattern would be good
    qtSLiMAppDelegate = this;
    
    // Cache app-wide icons
    slimDocumentIcon_.addFile(":/icons/DocIcon16.png");
    slimDocumentIcon_.addFile(":/icons/DocIcon32.png");
    slimDocumentIcon_.addFile(":/icons/DocIcon48.png");
    slimDocumentIcon_.addFile(":/icons/DocIcon64.png");
    slimDocumentIcon_.addFile(":/icons/DocIcon128.png");
    slimDocumentIcon_.addFile(":/icons/DocIcon256.png");
    slimDocumentIcon_.addFile(":/icons/DocIcon512.png");
    
    genericDocumentIcon_.addFile(":/icons/GenericDocIcon16.png");
    genericDocumentIcon_.addFile(":/icons/GenericDocIcon32.png");
    
    appIcon_.addFile(":/icons/AppIcon16.png");
    appIcon_.addFile(":/icons/AppIcon32.png");
    appIcon_.addFile(":/icons/AppIcon48.png");
    appIcon_.addFile(":/icons/AppIcon64.png");
    appIcon_.addFile(":/icons/AppIcon128.png");
    appIcon_.addFile(":/icons/AppIcon256.png");
    appIcon_.addFile(":/icons/AppIcon512.png");
    appIcon_.addFile(":/icons/AppIcon1024.png");
    
    // Set the application icon; this fixes in app icon in the dock/toolbar/whatever,
    // even if the right icon is not attached for display in the desktop environment
    app->setWindowIcon(appIcon_);
}

void QtSLiMAppDelegate::setUpRecipesMenu(QMenu *openRecipesMenu, QAction *findRecipeAction)
{
    // This is called by QtSLiMWindow::initializeUI().  We are to take control of the
    // recipes submenu, filling it in and implementing the Find Recipe panel.
    connect(findRecipeAction, &QAction::triggered, this, &QtSLiMAppDelegate::findRecipe);
    
    // Find recipes in our resources and sort by numeric order
    QDir recipesDir(":/recipes/", "Recipe *.*", QDir::NoSort, QDir::Files | QDir::NoSymLinks);
    QStringList entryList = recipesDir.entryList(QStringList("Recipe *.*"));   // the previous name filter seems to be ignored
    QCollator collator;
    
    collator.setNumericMode(true);
    std::sort(entryList.begin(), entryList.end(), collator);
    //qDebug() << entryList;
    
    // Append an action for each, organized into submenus
    QString previousItemChapter;
    QMenu *chapterSubmenu = nullptr;
    
    for (QString &entryName : entryList)
    {
        QString recipeName;
        
        // Determine the menu item text for the recipe, removing "Recipe " and ".txt", and adding pythons
        if (entryName.endsWith(".txt"))
            recipeName = entryName.mid(7, entryName.length() - 11);
        else if (entryName.endsWith(".py"))
            recipeName = entryName.mid(7, entryName.length() - 7) + QString(" 🐍");
        
        int firstDotIndex = recipeName.indexOf('.');
        
        if (firstDotIndex != -1)
        {
            QString recipeChapter = recipeName.left(firstDotIndex);
            
            // Create a submenu for each chapter
            if (recipeChapter != previousItemChapter)
            {
                int recipeChapterValue = recipeChapter.toInt();
                QString chapterName;
                
                switch (recipeChapterValue)
                {
                    case 4: chapterName = "Getting started: Neutral evolution in a panmictic population";		break;
                    case 5: chapterName = "Demography and population structure";								break;
                    case 6: chapterName = "Sexual reproduction";												break;
                    case 7: chapterName = "Mutation types, genomic elements, and chromosome structure";         break;
                    case 8: chapterName = "SLiMgui visualizations for polymorphism patterns";					break;
                    case 9:	chapterName = "Selective sweeps";													break;
                    case 10:chapterName = "Context-dependent selection using fitness() callbacks";				break;
                    case 11:chapterName = "Complex mating schemes using mateChoice() callbacks";				break;
                    case 12:chapterName = "Direct child modifications using modifyChild() callbacks";			break;
                    case 13:chapterName = "Phenotypes, fitness functions, quantitative traits, and QTLs";		break;
                    case 14:chapterName = "Advanced models";													break;
                    case 15:chapterName = "Continuous-space models and interactions";							break;
                    case 16:chapterName = "Going beyond Wright-Fisher models: nonWF model recipes";             break;
                    case 17:chapterName = "Tree-sequence recording: tracking population history";				break;
                    case 18:chapterName = "Modeling explicit nucleotides";										break;
                    default: break;
                }
                
                if (chapterName.length())
                {
                    QString fullChapterName = QString("%1 – %2").arg(QString::number(recipeChapterValue), chapterName);
                    QAction *mainItem = openRecipesMenu->addAction(fullChapterName);
                    
                    chapterSubmenu = new QMenu(fullChapterName, openRecipesMenu);
                    mainItem->setMenu(chapterSubmenu);
                }
                else
                {
                    qDebug() << "unrecognized chapter value " << recipeChapterValue;
                    qApp->beep();
                    break;
                }
            }
            
            // Move on to the current chapter
            previousItemChapter = recipeChapter;
            
            // And now add the menu item for the recipe
            QAction *menuItem = chapterSubmenu->addAction(recipeName);
            
            connect(menuItem, &QAction::triggered, this, &QtSLiMAppDelegate::openRecipe);
            menuItem->setData(QVariant(entryName));
        }
    }
}

bool QtSLiMAppDelegate::eventFilter(QObject *obj, QEvent *event)
{
    QEvent::Type type = event->type();
    
    if ((type == QEvent::KeyPress) || (type == QEvent::KeyRelease))
    {
        // emit modifier changed signals for use by the app
        QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(event);
        
        if (keyEvent)
        {
            Qt::Key key = static_cast<Qt::Key>(keyEvent->key());     // why does this return int?
            
            if ((key == Qt::Key_Shift) ||
                    (key == Qt::Key_Control) ||
                    (key == Qt::Key_Meta) ||
                    (key == Qt::Key_Alt) ||
                    (key == Qt::Key_AltGr) ||
                    (key == Qt::Key_CapsLock) ||
                    (key == Qt::Key_NumLock) ||
                    (key == Qt::Key_ScrollLock))
                emit modifiersChanged(keyEvent->modifiers());
        }
    }
    else if ((type == QEvent::WindowActivate) || (type == QEvent::WindowDeactivate) || (type == QEvent::WindowStateChange) ||
             (type == QEvent::WindowBlocked) || (type == QEvent::WindowUnblocked) || (type == QEvent::HideToParent) ||
             (type == QEvent::ShowToParent) || (type == QEvent::Close))
    {
        // track the active window; we use a timer here because the active window is not yet accurate in all cases
        // we also want to coalesce the work involved, so we use a flag to avoid scheduling more than one update
        if (!queuedActiveWindowUpdate)
        {
            queuedActiveWindowUpdate = true;
            //qDebug() << "SCHEDULED... event type ==" << type;
            QTimer::singleShot(0, this, &QtSLiMAppDelegate::updateActiveWindowList);
        }
        else
        {
            //qDebug() << "REDUNDANT... event type ==" << type;
        }
    }
    else if (type == QEvent::FileOpen)
    {
        // the user has requested that a file be opened; we find the currently active main window
        // and have it service the request, so that image files become "owned" by the active main
        // window, and so that the active main window will be reused if it's untitled and reuseable
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
        QString filePath = openEvent->file();
        QtSLiMWindow *window = activeQtSLiMWindow();
        
        if (window)
            window->eidos_openDocument(filePath);   // just calls openFile()
        
        return true;    // filter this event, i.e., prevent any further Qt handling of it
    }
    
    // standard event processing
    return QObject::eventFilter(obj, event);
}


//
//  public slots
//

void QtSLiMAppDelegate::lastWindowClosed(void)
{
    //qDebug() << "QtSLiMAppDelegate::lastWindowClosed";
}

void QtSLiMAppDelegate::aboutToQuit(void)
{
    //qDebug() << "QtSLiMAppDelegate::aboutToQuit";
}

void QtSLiMAppDelegate::findRecipe(void)
{
    // We delegate the opening itself to the active window, so that it can tile
    QtSLiMWindow *activeWindow = activeQtSLiMWindow();
    
    if (activeWindow)
    {
        QtSLiMFindRecipe findRecipePanel(nullptr);
        
        int result = findRecipePanel.exec();
        
        if (result == QDialog::Accepted)
        {
            QString resourceName = findRecipePanel.selectedRecipeFilename();
            QString recipeScript = findRecipePanel.selectedRecipeScript();
            QString trimmedName = resourceName;
            
            if (trimmedName.endsWith(".txt"))
                trimmedName.chop(4);
            
            activeWindow->openRecipe(trimmedName, recipeScript);
        }
    }
    else
    {
        // beep if there is no QtSLiMWindow to handle the action, but this should never happen
        qApp->beep();
    }
}

void QtSLiMAppDelegate::openRecipe(void)
{
    QAction *action = qobject_cast<QAction *>(sender());
    
    if (action)
    {
        const QVariant &data = action->data();
        QString resourceName = data.toString();
        
        if (resourceName.length())
        {
            QString resourcePath = ":/recipes/" + resourceName;
            QFile recipeFile(resourcePath);
            
            if (recipeFile.open(QFile::ReadOnly | QFile::Text))
            {
                QTextStream recipeTextStream(&recipeFile);
                QString recipeScript = recipeTextStream.readAll();
                
                // We delegate the opening itself to the active window, so that it can tile
                QtSLiMWindow *activeWindow = activeQtSLiMWindow();
                
                if (activeWindow)
                {
                    QString trimmedName = resourceName;
                    
                    if (trimmedName.endsWith(".txt"))
                        trimmedName.chop(4);
                    
                    activeWindow->openRecipe(trimmedName, recipeScript);
                }
                else
                {
                    // beep if there is no QtSLiMWindow to handle the action, but this should never happen
                    qApp->beep();
                }
            }
        }
     }
}

//
//  "First responder" type dispatch for actions shared across the app
//

// Here is the problem driving this design.  We want various actions, like "Close" and "New nonWF" and so forth,
// to work (with their shortcut) regardless of what window is frontmost; they should be global actions that
// are, at least conceptually, handled by the app, not by any specific widget or window.  Originally I tried
// defining these as "application" shortcuts, with Qt::ApplicationShortcut; that worked well on macOS, but on
// Linux they were deemed "ambiguous" by Qt when more than one main window was open – Qt didn't know which
// main window to dispatch to, for some reason.  Designating them as "window" shortcuts, with Qt::WindowShortcut,
// therefore seems to be a forced move.  However, they then work only if a main window is the active window, on
// Linux, unless we add them as window actions to every window in the app.  So that's what we do – add them to
// every window.  This helper method takes a window and adds all the global actions to it.  I feel like I'm
// maybe missing something with this design; there must be a more elegant way to do this in Qt.  But I've gone
// around and around on the menu item dispatch mechanism, and this is the best solution I've found.

// Note that it remains the case that the menu bar is owned by QtSLiMWindow, and that each QtSLiMWindow has its
// own menu bar; on Linux that is visible to the user, on macOS it is not.  QtSLiMWindow is therefore responsible
// for menu item enabling and validation, even for the global actions defined here.  The validation logic needs
// to be parallel to the dispatch logic here.  Arguably, we could have QtSLiMWindow delegate the validation to
// QtSLiMAppDelegate so that all the global action logic is together in this class, but since QtSLiMWindow owns
// the menu items, and they are private, I left the validation code there.

// Some actions added here have no shortcut.  Since they are not associated with any menu item or toolbar item,
// they will never actually be called.  Conceptually, they are global actions, though, and if a shortcut is
// added for them later, they will become callable here.  So they are declared here as placeholders.

void QtSLiMAppDelegate::addActionsForGlobalMenuItems(QWidget *window)
{
    {
        QAction *actionPreferences = new QAction("Preferences", this);
        actionPreferences->setShortcut(Qt::CTRL + Qt::Key_Comma);
        connect(actionPreferences, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_preferences);
        window->addAction(actionPreferences);
    }
    {
        QAction *actionAbout = new QAction("About", this);
        //actionAbout->setShortcut(Qt::CTRL + Qt::Key_Comma);
        connect(actionAbout, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_about);
        window->addAction(actionAbout);
    }
    {
        QAction *actionHelp = new QAction("Help", this);
        //actionHelp->setShortcut(Qt::CTRL + Qt::Key_Comma);
        connect(actionHelp, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_help);
        window->addAction(actionHelp);
    }
    {
        QAction *actionQuit = new QAction("Quit", this);
        actionQuit->setShortcut(Qt::CTRL + Qt::Key_Q);
        connect(actionQuit, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_quit);
        window->addAction(actionQuit);
    }
    
    {
        QAction *actionNewWF = new QAction("New WF", this);
        actionNewWF->setShortcut(Qt::CTRL + Qt::Key_N);
        connect(actionNewWF, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_newWF);
        window->addAction(actionNewWF);
    }
    {
        QAction *actionNewNonWF = new QAction("New nonWF", this);
        actionNewNonWF->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_N);
        connect(actionNewNonWF, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_newNonWF);
        window->addAction(actionNewNonWF);
    }
    {
        QAction *actionOpen = new QAction("Open", this);
        actionOpen->setShortcut(Qt::CTRL + Qt::Key_O);
        connect(actionOpen, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_open);
        window->addAction(actionOpen);
    }
    {
        QAction *actionClose = new QAction("Close", this);
        actionClose->setShortcut(Qt::CTRL + Qt::Key_W);
        connect(actionClose, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_close);
        window->addAction(actionClose);
    }
    
    {
        QAction *actionCheckScript = new QAction("Check Script", this);
        actionCheckScript->setShortcut(Qt::CTRL + Qt::Key_Equal);
        connect(actionCheckScript, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_checkScript);
        window->addAction(actionCheckScript);
    }
    {
        QAction *actionPrettyprintScript = new QAction("Prettyprint Script", this);
        actionPrettyprintScript->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_Equal);
        connect(actionPrettyprintScript, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_prettyprintScript);
        window->addAction(actionPrettyprintScript);
    }
    {
        QAction *actionShowScriptHelp = new QAction("Show Script Help", this);
        //actionShowScriptHelp->setShortcut(Qt::CTRL + Qt::Key_K);
        connect(actionShowScriptHelp, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_help);
        window->addAction(actionShowScriptHelp);
    }
    {
        QAction *actionShowEidosConsole = new QAction("Show Eidos Console", this);
        actionShowEidosConsole->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_E);
        connect(actionShowEidosConsole, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_showEidosConsole);
        window->addAction(actionShowEidosConsole);
    }
    {
        QAction *actionShowVariableBrowser = new QAction("Show Variable Browser", this);
        actionShowVariableBrowser->setShortcut(Qt::CTRL + Qt::Key_B);
        connect(actionShowVariableBrowser, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_showVariableBrowser);
        window->addAction(actionShowVariableBrowser);
    }
    {
        QAction *actionClearOutput = new QAction("Clear Output", this);
        actionClearOutput->setShortcut(Qt::CTRL + Qt::Key_K);
        connect(actionClearOutput, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_clearOutput);
        window->addAction(actionClearOutput);
    }
    {
        QAction *actionExecuteSelection = new QAction("Execute Selection", this);
        actionExecuteSelection->setShortcut(Qt::CTRL + Qt::Key_Return);
        connect(actionExecuteSelection, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_executeSelection);
        window->addAction(actionExecuteSelection);
    }
    {
        QAction *actionExecuteAll = new QAction("Execute All", this);
        actionExecuteAll->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_Return);
        connect(actionExecuteAll, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_executeAll);
        window->addAction(actionExecuteAll);
    }
    
    {
        QAction *actionShiftLeft = new QAction("Shift Left", this);
        actionShiftLeft->setShortcut(Qt::CTRL + Qt::Key_BracketLeft);
        connect(actionShiftLeft, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_shiftLeft);
        window->addAction(actionShiftLeft);
    }
    {
        QAction *actionShiftRight = new QAction("Shift Right", this);
        actionShiftRight->setShortcut(Qt::CTRL + Qt::Key_BracketRight);
        connect(actionShiftRight, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_shiftRight);
        window->addAction(actionShiftRight);
    }
    {
        QAction *actionCommentUncomment = new QAction("CommentUncomment", this);
        actionCommentUncomment->setShortcut(Qt::CTRL + Qt::Key_Slash);
        connect(actionCommentUncomment, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_commentUncomment);
        window->addAction(actionCommentUncomment);
    }
    
    {
        QAction *actionUndo = new QAction("Undo", this);
        actionUndo->setShortcut(Qt::CTRL + Qt::Key_Z);
        connect(actionUndo, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_undo);
        window->addAction(actionUndo);
    }
    {
        QAction *actionRedo = new QAction("Redo", this);
        actionRedo->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_Z);
        connect(actionRedo, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_redo);
        window->addAction(actionRedo);
    }
    {
        QAction *actionCut = new QAction("Cut", this);
        actionCut->setShortcut(Qt::CTRL + Qt::Key_X);
        connect(actionCut, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_cut);
        window->addAction(actionCut);
    }
    {
        QAction *actionCopy = new QAction("Copy", this);
        actionCopy->setShortcut(Qt::CTRL + Qt::Key_C);
        connect(actionCopy, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_copy);
        window->addAction(actionCopy);
    }
    {
        QAction *actionPaste = new QAction("Paste", this);
        actionPaste->setShortcut(Qt::CTRL + Qt::Key_V);
        connect(actionPaste, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_paste);
        window->addAction(actionPaste);
    }
    {
        QAction *actionDelete = new QAction("Delete", this);
        //actionDelete->setShortcut(Qt::CTRL + Qt::Key_V);
        connect(actionDelete, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_delete);
        window->addAction(actionDelete);
    }
    {
        QAction *actionSelectAll = new QAction("Select All", this);
        actionSelectAll->setShortcut(Qt::CTRL + Qt::Key_A);
        connect(actionSelectAll, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_selectAll);
        window->addAction(actionSelectAll);
    }
    
    {
        QAction *actionFindShow = new QAction("Find...", this);
        actionFindShow->setShortcut(Qt::CTRL + Qt::Key_F);
        connect(actionFindShow, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_findShow);
        window->addAction(actionFindShow);
    }
    {
        QAction *actionFindNext = new QAction("Find Next", this);
        actionFindNext->setShortcut(Qt::CTRL + Qt::Key_G);
        connect(actionFindNext, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_findNext);
        window->addAction(actionFindNext);
    }
    {
        QAction *actionFindPrevious = new QAction("Find Previous", this);
        actionFindPrevious->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_G);
        connect(actionFindPrevious, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_findPrevious);
        window->addAction(actionFindPrevious);
    }
    {
        QAction *actionReplaceAndFind = new QAction("Replace and Find", this);
        actionReplaceAndFind->setShortcut(Qt::CTRL + Qt::ALT + Qt::Key_G);
        connect(actionReplaceAndFind, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_replaceAndFind);
        window->addAction(actionReplaceAndFind);
    }
    {
        QAction *actionUseSelectionForFind = new QAction("Use Selection for Find", this);
        actionUseSelectionForFind->setShortcut(Qt::CTRL + Qt::Key_E);
        connect(actionUseSelectionForFind, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_useSelectionForFind);
        window->addAction(actionUseSelectionForFind);
    }
    {
        QAction *actionUseSelectionForReplace = new QAction("Use Selection for Replace", this);
        actionUseSelectionForReplace->setShortcut(Qt::CTRL + Qt::ALT + Qt::Key_E);
        connect(actionUseSelectionForReplace, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_useSelectionForReplace);
        window->addAction(actionUseSelectionForReplace);
    }
    {
        QAction *actionJumpToSelection = new QAction("Jump to Selection", this);
        actionJumpToSelection->setShortcut(Qt::CTRL + Qt::Key_J);
        connect(actionJumpToSelection, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_jumpToSelection);
        window->addAction(actionJumpToSelection);
    }
}

void QtSLiMAppDelegate::dispatch_preferences(void)
{
    QtSLiMPreferences &prefsWindow = QtSLiMPreferences::instance();
    
    prefsWindow.show();
    prefsWindow.raise();
    prefsWindow.activateWindow();
}

void QtSLiMAppDelegate::dispatch_about(void)
{
    QtSLiMAbout *aboutWindow = new QtSLiMAbout(nullptr);
    
    aboutWindow->setAttribute(Qt::WA_DeleteOnClose);    
    
    aboutWindow->show();
    aboutWindow->raise();
    aboutWindow->activateWindow();
}

void QtSLiMAppDelegate::dispatch_help(void)
{
    QtSLiMHelpWindow &helpWindow = QtSLiMHelpWindow::instance();
    
    helpWindow.show();
    helpWindow.raise();
    helpWindow.activateWindow();
}

void QtSLiMAppDelegate::dispatch_quit(void)
{
    if (qApp)
        qApp->closeAllWindows();
}

void QtSLiMAppDelegate::dispatch_newWF(void)
{
    QtSLiMWindow *activeWindow = activeQtSLiMWindow();
    if (activeWindow)
        activeWindow->newFile_WF();
}

void QtSLiMAppDelegate::dispatch_newNonWF(void)
{
    QtSLiMWindow *activeWindow = activeQtSLiMWindow();
    if (activeWindow)
        activeWindow->newFile_nonWF();
}

void QtSLiMAppDelegate::dispatch_open(void)
{
    QtSLiMWindow *activeWindow = activeQtSLiMWindow();
    if (activeWindow)
        activeWindow->open();
}

void QtSLiMAppDelegate::dispatch_close(void)
{
    // We close the "active" window, which is a bit different from the front window
    // It can be nullptr; in that case it's hard to know what to do
    QWidget *activeWindow = QApplication::activeWindow();
    
    if (activeWindow)
        activeWindow->close();
}

void QtSLiMAppDelegate::dispatch_shiftLeft(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QtSLiMScriptTextEdit *scriptEdit = dynamic_cast<QtSLiMScriptTextEdit*>(focusWidget);
    
    if (scriptEdit && scriptEdit->isEnabled() && !scriptEdit->isReadOnly())
        scriptEdit->shiftSelectionLeft();
}

void QtSLiMAppDelegate::dispatch_shiftRight(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QtSLiMScriptTextEdit *scriptEdit = dynamic_cast<QtSLiMScriptTextEdit*>(focusWidget);
    
    if (scriptEdit && scriptEdit->isEnabled() && !scriptEdit->isReadOnly())
        scriptEdit->shiftSelectionRight();
}

void QtSLiMAppDelegate::dispatch_commentUncomment(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QtSLiMScriptTextEdit *scriptEdit = dynamic_cast<QtSLiMScriptTextEdit*>(focusWidget);
    
    if (scriptEdit && scriptEdit->isEnabled() && !scriptEdit->isReadOnly())
        scriptEdit->commentUncommentSelection();
}

void QtSLiMAppDelegate::dispatch_undo(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
        lineEdit->undo();
    else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
        textEdit->undo();
}

void QtSLiMAppDelegate::dispatch_redo(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
        lineEdit->redo();
    else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
        textEdit->redo();
}

void QtSLiMAppDelegate::dispatch_cut(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
        lineEdit->cut();
    else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
        textEdit->cut();
}

void QtSLiMAppDelegate::dispatch_copy(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled())
        lineEdit->copy();
    else if (textEdit && textEdit->isEnabled())
        textEdit->copy();
}

void QtSLiMAppDelegate::dispatch_paste(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
        lineEdit->paste();
    else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
        textEdit->paste();
}

void QtSLiMAppDelegate::dispatch_delete(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
        lineEdit->insert("");
    else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
        textEdit->insertPlainText("");
}

void QtSLiMAppDelegate::dispatch_selectAll(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled())
        lineEdit->selectAll();
    else if (textEdit && textEdit->isEnabled())
        textEdit->selectAll();
}

void QtSLiMAppDelegate::dispatch_findShow(void)
{
    QtSLiMFindPanel::instance().showFindPanel();
}

void QtSLiMAppDelegate::dispatch_findNext(void)
{
    QtSLiMFindPanel::instance().findNext();
}

void QtSLiMAppDelegate::dispatch_findPrevious(void)
{
    QtSLiMFindPanel::instance().findPrevious();
}

void QtSLiMAppDelegate::dispatch_replaceAndFind(void)
{
    QtSLiMFindPanel::instance().replaceAndFind();
}

void QtSLiMAppDelegate::dispatch_useSelectionForFind(void)
{
    QtSLiMFindPanel::instance().useSelectionForFind();
}

void QtSLiMAppDelegate::dispatch_useSelectionForReplace(void)
{
    QtSLiMFindPanel::instance().useSelectionForReplace();
}

void QtSLiMAppDelegate::dispatch_jumpToSelection(void)
{
    QtSLiMFindPanel::instance().jumpToSelection();
}

void QtSLiMAppDelegate::dispatch_checkScript(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : nullptr);
    QtSLiMWindow *slimWindow = dynamic_cast<QtSLiMWindow*>(focusWindow);
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    
    if (slimWindow)
        slimWindow->scriptTextEdit()->checkScript();
    else if (eidosConsole)
        eidosConsole->scriptTextEdit()->checkScript();
}

void QtSLiMAppDelegate::dispatch_prettyprintScript(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : nullptr);
    QtSLiMWindow *slimWindow = dynamic_cast<QtSLiMWindow*>(focusWindow);
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    
    if (slimWindow)
        slimWindow->scriptTextEdit()->prettyprint();
    else if (eidosConsole)
        eidosConsole->scriptTextEdit()->prettyprint();
}

void QtSLiMAppDelegate::dispatch_showEidosConsole(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : nullptr);
    QtSLiMWindow *slimWindow = dynamic_cast<QtSLiMWindow*>(focusWindow);
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    
    if (slimWindow)
        slimWindow->showConsoleClicked();
    else if (eidosConsole)
        eidosConsole->parentSLiMWindow->showConsoleClicked();
}

void QtSLiMAppDelegate::dispatch_showVariableBrowser(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : nullptr);
    QtSLiMWindow *slimWindow = dynamic_cast<QtSLiMWindow*>(focusWindow);
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    QtSLiMVariableBrowser *varBrowser = dynamic_cast<QtSLiMVariableBrowser*>(focusWindow);
    
    if (slimWindow)
        slimWindow->showBrowserClicked();
    else if (eidosConsole)
        eidosConsole->parentSLiMWindow->showBrowserClicked();
    else if (varBrowser)
        varBrowser->parentEidosConsole->parentSLiMWindow->showBrowserClicked();
}

void QtSLiMAppDelegate::dispatch_clearOutput(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : nullptr);
    QtSLiMWindow *slimWindow = dynamic_cast<QtSLiMWindow*>(focusWindow);
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    
    if (slimWindow)
        slimWindow->clearOutputClicked();
    else if (eidosConsole)
        eidosConsole->consoleTextEdit()->clearToPrompt();
}

void QtSLiMAppDelegate::dispatch_executeSelection(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : nullptr);
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    
    if (eidosConsole)
        eidosConsole->executeSelectionClicked();
}

void QtSLiMAppDelegate::dispatch_executeAll(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : nullptr);
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    
    if (eidosConsole)
        eidosConsole->executeAllClicked();
}


//
//  Active QtSLiMWindow tracking
//
//  For the Find window and similar modeless interactions, we need to be able to find the active
//  main window, which Qt does not provide (the "active window" is not necessarily a main window).
//  To do this, we have to track focus changes, to maintain a list of windows that is sorted from
//  front to back.
//

void QtSLiMAppDelegate::updateActiveWindowList(void)
{
    QWidget *window = qApp->activeWindow();
    
    if (window)
    {
        // window is now the front window, so move it to the front of focusedQtSLiMWindowList
        focusedWindowList.removeOne(window);
        focusedWindowList.push_front(window);
    }
    
    // keep the window list trim and accurate
    pruneWindowList();
    
    // emit our signal
    emit activeWindowListChanged();
    
    // we're done updating, we can now update again if something new happens
    queuedActiveWindowUpdate = false;
    
    // debug output
//    qDebug() << "New window list:";
//
//    for (QPointer<QWidget> &window_ptr : focusedWindowList)
//        if (window_ptr)
//            qDebug() << "   " << window_ptr->windowTitle();
}

void QtSLiMAppDelegate::focusChanged(QWidget * /* old */, QWidget * /* now */)
{
    // track the active window; we use a timer here because the active window is not yet accurate in all cases
    // we also want to coalesce the work involved, so we use a flag to avoid scheduling more than one update
    if (!queuedActiveWindowUpdate)
    {
        queuedActiveWindowUpdate = true;
        QTimer::singleShot(0, this, &QtSLiMAppDelegate::updateActiveWindowList);
    }
}

void QtSLiMAppDelegate::pruneWindowList(void)
{
    int windowListCount = focusedWindowList.size();
    
    for (int listIndex = 0; listIndex < windowListCount; listIndex++)
    {
        QPointer<QWidget> &focused_window_ptr = focusedWindowList[listIndex];
        
        if (focused_window_ptr && focused_window_ptr->isVisible() && !focused_window_ptr->isHidden())
            continue;
        
        // prune
        focusedWindowList.removeAt(listIndex);
        windowListCount--;
        listIndex--;
    }
}

QtSLiMWindow *QtSLiMAppDelegate::activeQtSLiMWindow(void)
{
    // First try qApp's active window; if the SLiM window is key, this suffices
    // This allows Qt to define the active main window in some platform-specific way,
    // perhaps based upon which window the cursor is in, for example; for the
    // activeWindowExcluding() method we want our window list to be the sole authority,
    // but for this method I don't think we do...?
    QWidget *activeWindow = qApp->activeWindow();
    QtSLiMWindow *activeQtSLiMWindow = qobject_cast<QtSLiMWindow *>(activeWindow);
    
    if (activeQtSLiMWindow)
        return activeQtSLiMWindow;
    
    // If that fails, use the last focused main window, as tracked by focusChanged()
    pruneWindowList();
    
    for (QPointer<QWidget> &focused_window_ptr : focusedWindowList)
    {
        if (focused_window_ptr)
        {
            QWidget *focused_window = focused_window_ptr.data();
            QtSLiMWindow *focusedQtSLiMWindow = qobject_cast<QtSLiMWindow *>(focused_window);
            
            if (focusedQtSLiMWindow)
                return focusedQtSLiMWindow;
        }
    }
    
    return nullptr;
}

QWidget *QtSLiMAppDelegate::activeWindow(void)
{
    // QApplication can handle this one
    return qApp->activeWindow();
}

QWidget *QtSLiMAppDelegate::activeWindowExcluding(QWidget *excluded)
{
    // Use the last focused window, as tracked by focusChanged()
    pruneWindowList();
    
    for (QPointer<QWidget> &focused_window_ptr : focusedWindowList)
    {
        if (focused_window_ptr)
        {
            QWidget *focused_window = focused_window_ptr.data();
            
            if (focused_window != excluded)
                return focused_window;
        }
    }
    
    return nullptr;
}


// This is declared in eidos_beep.h, but in QtSLiM it is actually defined here,
// so that we can produce the beep sound with Qt
std::string Eidos_Beep_QT(std::string __attribute__((__unused__)) p_sound_name)
{
    std::string return_string;
    
    qApp->beep();
    
    return return_string;
}




























