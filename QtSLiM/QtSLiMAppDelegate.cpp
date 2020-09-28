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
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDesktopServices>
#include <QSettings>
#include <QFileDialog>
#include <QDebug>

#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "eidos_globals.h"
#include "eidos_beep.h"
#include "slim_globals.h"


// Check the Qt version and display an error if it is unacceptable
// We enforce Qt 5.9.5 as a hard limit, since it is what Ubuntu 18.04 LTS has preinstalled
#if (QT_VERSION < 0x050905)
#error "SLiMgui requires Qt version 5.9.5 or later.  Please uninstall Qt and then install a more recent version (5.12 LTS recommended)."
#endif


static std::string Eidos_Beep_QT(std::string p_sound_name);


QtSLiMAppDelegate *qtSLiMAppDelegate = nullptr;


// A custom message handler that we can use, optionally, for deubgging.  This is useful if Qt is emitting a warning and you don't know
// where it's coming from; turn on the use of the message handler, and then set a breakpoint inside it, perhaps conditional on msg.
void QtSLiM_MessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
#pragma unused (type, context)
    
    // Test for a specific message so you can break on it when it occurs
    //if (msg.contains("Using QCharRef with an index"))
    //    qDebug() << "HIT WATCH MESSAGE; SET A BREAKPOINT HERE!";
    
    // useful behavior, from https://doc.qt.io/qt-5/qtglobal.html#qInstallMessageHandler
    {
        QByteArray localMsg = msg.toLocal8Bit();
        
#if 1
        time_t rawtime;
        struct tm *timeinfo;
        
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        fprintf(stderr, "%02d:%02d:%02d : ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
#endif
        
#if 0
        // Log file/line number and function/method name
        const char *file = context.file ? context.file : "";
        const char *function = context.function ? context.function : "";
        
        fprintf(stderr, "%s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
#else
        // Just log the message itself
        fprintf(stderr, "%s\n", localMsg.constData());
#endif
        
#if 0
        // Log a backtrace after each message
        Eidos_PrintStacktrace(stderr, 20);
#endif
    }
}


QtSLiMAppDelegate::QtSLiMAppDelegate(QObject *parent) : QObject(parent)
{
    // Install a message handler; this seems useful for debugging
    qInstallMessageHandler(QtSLiM_MessageHandler);
    
    // Determine whether we were launched from a shell or from something else (Finder, Xcode, etc.)
	launchedFromShell_ = (isatty(fileno(stdin)) == 1); // && !SLiM_AmIBeingDebugged();
    
    // Install our custom beep handler
    Eidos_Beep = &Eidos_Beep_QT;
    
    // Let Qt know who we are, for QSettings configuration
    QCoreApplication::setOrganizationName("MesserLab");
    QCoreApplication::setOrganizationDomain("MesserLab.edu");   // Qt expects the domain in the standard order, and reverses it to form "edu.messerlab.QtSLiM.plist" as per Apple's usage
    QCoreApplication::setApplicationName("QtSLiM");             // This governs the location of our prefs, which we keep under edu.MesserLab.QtSLiM
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
    
    appIconHighlighted_.addPixmap(QtSLiMDarkenPixmap(appIcon_.pixmap(16, 16)));
    appIconHighlighted_.addPixmap(QtSLiMDarkenPixmap(appIcon_.pixmap(32, 32)));
    appIconHighlighted_.addPixmap(QtSLiMDarkenPixmap(appIcon_.pixmap(48, 48)));
    appIconHighlighted_.addPixmap(QtSLiMDarkenPixmap(appIcon_.pixmap(64, 64)));
    appIconHighlighted_.addPixmap(QtSLiMDarkenPixmap(appIcon_.pixmap(128, 128)));
    appIconHighlighted_.addPixmap(QtSLiMDarkenPixmap(appIcon_.pixmap(256, 256)));
    appIconHighlighted_.addPixmap(QtSLiMDarkenPixmap(appIcon_.pixmap(512, 512)));
    appIconHighlighted_.addPixmap(QtSLiMDarkenPixmap(appIcon_.pixmap(1024, 1024)));
    
    // Set the application icon; this fixes the app icon in the dock/toolbar/whatever,
    // even if the right icon is not attached for display in the desktop environment
    app->setWindowIcon(appIcon_);
    
#ifdef __APPLE__
    // On macOS, make the global menu bar for use when no other window is open
    // BCH 9/23/2020: I am forced not to do this by a crash on quit, so we continue to delete on close for
    // now (and we continue to quit when the last window closes).  See QTBUG-86874 and QTBUG-86875.  If a
    // fix or workaround for either of those issues is found, the code is otherwise ready to transition to
    // having QtSLiM stay open after the last window closes, on macOS.  Search for those bug numbers to find
    // the other spots in the code related to this mess.
    // BCH 9/24/2020: Note that QTBUG-86875 is fixed in 5.15.1, but we don't want to require that.
    //makeGlobalMenuBar();
#endif
}

QtSLiMAppDelegate::~QtSLiMAppDelegate(void)
{
}


//
//  Document opening
//

QtSLiMWindow *QtSLiMAppDelegate::findMainWindow(const QString &fileName) const
{
    QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();

    const QList<QWidget *> topLevelWidgets = QApplication::topLevelWidgets();
    for (QWidget *widget : topLevelWidgets) {
        QtSLiMWindow *mainWin = qobject_cast<QtSLiMWindow *>(widget);
        if (mainWin && (mainWin->currentFile == canonicalFilePath))
            return mainWin;
    }

    return nullptr;
}

void QtSLiMAppDelegate::newFile_WF(void)
{
    QtSLiMWindow *activeWindow = activeQtSLiMWindow();
    QtSLiMWindow *window = new QtSLiMWindow(QtSLiMWindow::ModelType::WF);
    
    window->tile(activeWindow);
    window->show();
}

void QtSLiMAppDelegate::newFile_nonWF(void)
{
    QtSLiMWindow *activeWindow = activeQtSLiMWindow();
    QtSLiMWindow *window = new QtSLiMWindow(QtSLiMWindow::ModelType::nonWF);
    
    window->tile(activeWindow);
    window->show();
}

QtSLiMWindow *QtSLiMAppDelegate::open(QtSLiMWindow *requester)
{
    QSettings settings;
    QString directory = settings.value("QtSLiMDefaultOpenDirectory", QVariant(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation))).toString();
    QString fileName;
    
    if (!requester)
        requester = activeQtSLiMWindow();
    
    if (requester)
        fileName = QFileDialog::getOpenFileName(requester, QString(), directory, "Files (*.slim *.txt *.png *.jpg *.jpeg *.bmp *.gif)");
    else
        fileName = QFileDialog::getOpenFileName(nullptr, QString(), directory, "Files (*.slim *.txt)");
    
    if (!fileName.isEmpty())
    {
        settings.setValue("QtSLiMDefaultOpenDirectory", QVariant(QFileInfo(fileName).path()));
        return openFile(fileName, requester);
    }
    
    return nullptr;
}

QtSLiMWindow *QtSLiMAppDelegate::openFile(const QString &fileName, QtSLiMWindow *requester)
{
    if (!requester)
        requester = activeQtSLiMWindow();
    
    if (fileName.endsWith(".png", Qt::CaseInsensitive) ||
            fileName.endsWith(".jpg", Qt::CaseInsensitive) ||
            fileName.endsWith(".jpeg", Qt::CaseInsensitive) ||
            fileName.endsWith(".bmp", Qt::CaseInsensitive) ||
            fileName.endsWith(".gif", Qt::CaseInsensitive))
    {
        if (requester)
        {
            // An image; this opens as a child of the current SLiM window
            QWidget *imageWindow = requester->imageWindowWithPath(fileName);
            
            if (imageWindow)
            {
                imageWindow->show();
                imageWindow->raise();
                imageWindow->activateWindow();
            }
            
            return requester;
        }
        else
        {
            qApp->beep();
            return nullptr;
        }
    }
    else
    {
        // Should be a .slim or .txt file; look for an existing window for the file, otherwise open a new window (or reuse a reuseable window)
        QtSLiMWindow *existing = findMainWindow(fileName);
        if (existing) {
            existing->show();
            existing->raise();
            existing->activateWindow();
            return existing;
        }
        
        if (requester && requester->windowIsReuseable()) {
            requester->loadFile(fileName);
            return requester;
        }
        
        QtSLiMWindow *window = new QtSLiMWindow(fileName);
        if (window->isUntitled) {
            delete window;
            return nullptr;
        }
        window->tile(requester);
        window->show();
        return window;
    }
}

void QtSLiMAppDelegate::openRecipeWithName(const QString &recipeName, const QString &recipeScript, QtSLiMWindow *requester)
{
    if (!requester)
        requester = activeQtSLiMWindow();
    
    if (requester && requester->windowIsReuseable())
    {
        requester->loadRecipe(recipeName, recipeScript);
        return;
    }
    
    QtSLiMWindow *window = new QtSLiMWindow(recipeName, recipeScript);
    if (!window->isRecipe) {
        delete window;
        return;
    }
    window->tile(requester);
    window->show();
}


//
//  Recipes menu handling
//

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


//
//  Recent document menu handling
//

static const int MaxRecentFiles = 10;
static inline QString recentFilesKey() { return QStringLiteral("QtSLiMRecentFilesList"); }
static inline QString fileKey() { return QStringLiteral("file"); }

static QStringList readRecentFiles(QSettings &settings)
{
    QStringList result;
    const int count = settings.beginReadArray(recentFilesKey());
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        result.append(settings.value(fileKey()).toString());
    }
    settings.endArray();
    return result;
}

static void writeRecentFiles(const QStringList &files, QSettings &settings)
{
    const int count = files.size();
    settings.beginWriteArray(recentFilesKey());
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        settings.setValue(fileKey(), files.at(i));
    }
    settings.endArray();
}

void QtSLiMAppDelegate::updateRecentFileActions()
{
    const QMenu *menu = qobject_cast<const QMenu *>(sender());
    QSettings settings;

    const QStringList recentFiles = readRecentFiles(settings);
    const int count = qMin(int(MaxRecentFiles), recentFiles.size());
    
    for (int i = 0 ; i < MaxRecentFiles; ++i)
    {
        QAction *recentAction = menu->actions()[i];
        
        if (i < count)
        {
            const QString fileName = QFileInfo(recentFiles.at(i)).fileName();
            recentAction->setText(fileName);
            recentAction->setData(recentFiles.at(i));
            recentAction->setVisible(true);
        }
        else
        {
            recentAction->setVisible(false);
        }
    }
}

void QtSLiMAppDelegate::openRecentFile()
{
    const QAction *action = qobject_cast<const QAction *>(sender());
    
    if (action)
        openFile(action->data().toString(), nullptr);
}

void QtSLiMAppDelegate::clearRecentFiles()
{
    QSettings settings;
    QStringList emptyRecentFiles;
    writeRecentFiles(emptyRecentFiles, settings);
}

void QtSLiMAppDelegate::setUpRecentsMenu(QMenu *openRecentSubmenu)
{
    connect(openRecentSubmenu, &QMenu::aboutToShow, this, &QtSLiMAppDelegate::updateRecentFileActions);
    
    for (int i = 0; i < MaxRecentFiles; ++i)
        openRecentSubmenu->addAction(QString(), this, &QtSLiMAppDelegate::openRecentFile)->setVisible(false);
    
    openRecentSubmenu->addSeparator();
    openRecentSubmenu->addAction("Clear Menu", this, &QtSLiMAppDelegate::clearRecentFiles);
}

void QtSLiMAppDelegate::prependToRecentFiles(const QString &fileName)
{
    QSettings settings;

    const QStringList oldRecentFiles = readRecentFiles(settings);
    QStringList recentFiles = oldRecentFiles;
    recentFiles.removeAll(fileName);
    recentFiles.prepend(fileName);
    if (oldRecentFiles != recentFiles)
        writeRecentFiles(recentFiles, settings);
}


//
//  Event filtering
//

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
        // the user has requested that a file be opened
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
        QString filePath = openEvent->file();
        
        openFile(filePath, nullptr);
        
        return true;    // filter this event, i.e., prevent any further Qt handling of it
    }
    
    // standard event processing
    return QObject::eventFilter(obj, event);
}


//
//  public slots
//

void QtSLiMAppDelegate::appDidFinishLaunching(QtSLiMWindow *initialWindow)
{
    // Display a startup message in the initial window's status bar
    if (initialWindow)
        initialWindow->displayStartupMessage();
}

void QtSLiMAppDelegate::lastWindowClosed(void)
{
}

void QtSLiMAppDelegate::aboutToQuit(void)
{
    //qDebug() << "QtSLiMAppDelegate::aboutToQuit";
}

void QtSLiMAppDelegate::findRecipe(void)
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
        
        openRecipeWithName(trimmedName, recipeScript, nullptr);
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
                QString trimmedName = resourceName;
                
                if (trimmedName.endsWith(".txt"))
                    trimmedName.chop(4);
                
                openRecipeWithName(trimmedName, recipeScript, nullptr);
            }
        }
     }
}

void QtSLiMAppDelegate::playStateChanged(void)
{
    bool anyPlaying = false;
    
    for (QWidget *widget : qApp->topLevelWidgets())
    {
        QtSLiMWindow *mainWin = qobject_cast<QtSLiMWindow *>(widget);
        
        if (mainWin && mainWin->isPlaying())
            anyPlaying = true;
    }
    
    qApp->setWindowIcon(anyPlaying ? appIconHighlighted_ : appIcon_);
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
        QAction *actionReformatScript = new QAction("Reformat Script", this);
        actionReformatScript->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::ALT + Qt::Key_Equal);
        connect(actionReformatScript, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_reformatScript);
        window->addAction(actionReformatScript);
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
    {
        QAction *actionJumpToLine = new QAction("Jump to Line", this);
        actionJumpToLine->setShortcut(Qt::CTRL + Qt::Key_L);
        connect(actionJumpToLine, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_jumpToLine);
        window->addAction(actionJumpToLine);
    }
    
    {
        QAction *actionMinimize = new QAction("Minimize", this);
        actionMinimize->setShortcut(Qt::CTRL + Qt::Key_M);
        connect(actionMinimize, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_minimize);
        window->addAction(actionMinimize);
    }
    {
        QAction *actionZoom = new QAction("Zoom", this);
        //actionZoom->setShortcut(Qt::CTRL + Qt::Key_M);
        connect(actionZoom, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_zoom);
        window->addAction(actionZoom);
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
    {
        qApp->closeAllWindows();
        
#ifdef __APPLE__
        // On macOS, simply closing all windows doesn't suffice; we need to explicitly quit
        // BCH 9/23/2020: I am forced not to do this by a crash on quit, so we continue to delete on close for
        // now (and we continue to quit when the last window closes).  See QTBUG-86874 and QTBUG-86875.  If a
        // fix or workaround for either of those issues is found, the code is otherwise ready to transition to
        // having QtSLiM stay open after the last window closes, on macOS.  Search for those bug numbers to find
        // the other spots in the code related to this mess.
        // BCH 9/24/2020: Note that QTBUG-86875 is fixed in 5.15.1, but we don't want to require that.
        //qApp->quit();
#endif
    }
}

void QtSLiMAppDelegate::dispatch_newWF(void)
{
    newFile_WF();
}

void QtSLiMAppDelegate::dispatch_newNonWF(void)
{
    newFile_nonWF();
}

void QtSLiMAppDelegate::dispatch_open(void)
{
    open(nullptr);
}

void QtSLiMAppDelegate::dispatch_close(void)
{
    // We close the "active" window, which is a bit different from the front window
    // It can be nullptr; in that case it's hard to know what to do
    QWidget *activeWindow = QApplication::activeWindow();
    
    if (activeWindow)
        activeWindow->close();
    else
        qApp->beep();
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

void QtSLiMAppDelegate::dispatch_jumpToLine(void)
{
    QtSLiMFindPanel::instance().jumpToLine();
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

void QtSLiMAppDelegate::dispatch_reformatScript(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : nullptr);
    QtSLiMWindow *slimWindow = dynamic_cast<QtSLiMWindow*>(focusWindow);
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    
    if (slimWindow)
        slimWindow->scriptTextEdit()->reformat();
    else if (eidosConsole)
        eidosConsole->scriptTextEdit()->reformat();
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

void QtSLiMAppDelegate::dispatch_minimize(void)
{
    // We minimize the "active" window, which is a bit different from the front window
    // It can be nullptr; in that case it's hard to know what to do
    QWidget *activeWindow = QApplication::activeWindow();
    
    if (activeWindow)
    {
        bool isMinimized = activeWindow->windowState() & Qt::WindowMinimized;
        
        if (isMinimized)
            activeWindow->setWindowState((activeWindow->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        else
            activeWindow->setWindowState(activeWindow->windowState() | Qt::WindowMinimized);    // Qt refuses to minimize Qt::Tool windows; see https://bugreports.qt.io/browse/QTBUG-86520
    }
    else
        qApp->beep();
}

void QtSLiMAppDelegate::dispatch_zoom(void)
{
    // We zoom the "active" window, which is a bit different from the front window
    // It can be nullptr; in that case it's hard to know what to do
    QWidget *activeWindow = QApplication::activeWindow();
    
    if (activeWindow)
    {
        bool isZoomed = activeWindow->windowState() & Qt::WindowMaximized;
        
        if (isZoomed)
            activeWindow->setWindowState((activeWindow->windowState() & ~Qt::WindowMaximized) | Qt::WindowActive);
        else
            activeWindow->setWindowState((activeWindow->windowState() | Qt::WindowMaximized) | Qt::WindowActive);
    }
    else
        qApp->beep();
}

void QtSLiMAppDelegate::dispatch_helpWorkshops(void)
{
    QDesktopServices::openUrl(QUrl("http://benhaller.com/workshops/workshops.html", QUrl::TolerantMode));
}

void QtSLiMAppDelegate::dispatch_helpFeedback(void)
{
    QDesktopServices::openUrl(QUrl("mailto:bhaller@mac.com?subject=SLiM%20Feedback", QUrl::TolerantMode));
}

void QtSLiMAppDelegate::dispatch_helpSLiMDiscuss(void)
{
    QDesktopServices::openUrl(QUrl("https://groups.google.com/d/forum/slim-discuss", QUrl::TolerantMode));
}

void QtSLiMAppDelegate::dispatch_helpSLiMAnnounce(void)
{
    QDesktopServices::openUrl(QUrl("https://groups.google.com/d/forum/slim-announce", QUrl::TolerantMode));
}

void QtSLiMAppDelegate::dispatch_helpSLiMHome(void)
{
    QDesktopServices::openUrl(QUrl("http://messerlab.org/slim/", QUrl::TolerantMode));
}

void QtSLiMAppDelegate::dispatch_helpSLiMExtras(void)
{
    QDesktopServices::openUrl(QUrl("https://github.com/MesserLab/SLiM-Extras", QUrl::TolerantMode));
}

void QtSLiMAppDelegate::dispatch_helpMesserLab(void)
{
    QDesktopServices::openUrl(QUrl("http://messerlab.org/", QUrl::TolerantMode));
}

void QtSLiMAppDelegate::dispatch_helpBenHaller(void)
{
    QDesktopServices::openUrl(QUrl("http://www.benhaller.com/", QUrl::TolerantMode));
}

void QtSLiMAppDelegate::dispatch_helpStickSoftware(void)
{
    QDesktopServices::openUrl(QUrl("http://www.sticksoftware.com/", QUrl::TolerantMode));
}

void QtSLiMAppDelegate::makeGlobalMenuBar(void)
{
#ifdef __APPLE__
    // Make a global menubar that gets used when no window is open, on macOS
    // We do this only on demand because otherwise Qt seem to get confused about the automatic menu item repositioning
    // It's really gross to have to duplicate the menu structure here in code, but I don't see an easy way to copy existing menus
    if (!windowlessMenuBar)
    {
        windowlessMenuBar = new QMenuBar(nullptr);
        QMenu *fileMenu = new QMenu("File", windowlessMenuBar);
        
        windowlessMenuBar->addMenu(fileMenu);
        
        fileMenu->addAction("About SLiMgui", this, &QtSLiMAppDelegate::dispatch_about);
        fileMenu->addAction("Preferences...", this, &QtSLiMAppDelegate::dispatch_preferences, Qt::CTRL + Qt::Key_Comma);
        fileMenu->addSeparator();
        fileMenu->addAction("New", this, &QtSLiMAppDelegate::dispatch_newWF, Qt::CTRL + Qt::Key_N);
        fileMenu->addAction("New (nonWF)", this, &QtSLiMAppDelegate::dispatch_newNonWF, Qt::CTRL + Qt::SHIFT + Qt::Key_N);
        fileMenu->addAction("Open...", this, &QtSLiMAppDelegate::dispatch_open, Qt::CTRL + Qt::Key_O);
        QMenu *openRecent = fileMenu->addMenu("Open Recent");
        QMenu *openRecipe = fileMenu->addMenu("Open Recipe");
        fileMenu->addSeparator();
        fileMenu->addAction("Close", this, &QtSLiMAppDelegate::dispatch_close, Qt::CTRL + Qt::Key_W);
        QAction *actionSave = fileMenu->addAction("Save");
        actionSave->setShortcut(Qt::CTRL + Qt::Key_S);
        actionSave->setEnabled(false);
        QAction *actionSaveAs = fileMenu->addAction("Save As...");
        actionSaveAs->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_S);
        actionSaveAs->setEnabled(false);
        fileMenu->addAction("Revert to Saved")->setEnabled(false);
        fileMenu->addSeparator();
        fileMenu->addAction("Quit SLiMgui", this, &QtSLiMAppDelegate::dispatch_quit, Qt::CTRL + Qt::Key_Q);
        
        setUpRecentsMenu(openRecent);
        
        QAction *findRecipeAction = openRecipe->addAction("Find Recipe...");
        findRecipeAction->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_O);
        openRecipe->addSeparator();
        setUpRecipesMenu(openRecipe, findRecipeAction);
        
        QMenu *editMenu = new QMenu("Edit", windowlessMenuBar);
        
        windowlessMenuBar->addMenu(editMenu);
        
        editMenu->addAction("Undo", this, &QtSLiMAppDelegate::dispatch_undo, Qt::CTRL + Qt::Key_Z)->setEnabled(false);
        editMenu->addAction("Redo", this, &QtSLiMAppDelegate::dispatch_redo, Qt::CTRL + Qt::SHIFT + Qt::Key_Z)->setEnabled(false);
        editMenu->addSeparator();
        editMenu->addAction("Cut", this, &QtSLiMAppDelegate::dispatch_cut, Qt::CTRL + Qt::Key_X)->setEnabled(false);
        editMenu->addAction("Copy", this, &QtSLiMAppDelegate::dispatch_copy, Qt::CTRL + Qt::Key_C)->setEnabled(false);
        editMenu->addAction("Paste", this, &QtSLiMAppDelegate::dispatch_paste, Qt::CTRL + Qt::Key_V)->setEnabled(false);
        editMenu->addAction("Delete", this, &QtSLiMAppDelegate::dispatch_delete)->setEnabled(false);
        editMenu->addAction("Select All", this, &QtSLiMAppDelegate::dispatch_selectAll, Qt::CTRL + Qt::Key_A)->setEnabled(false);
        editMenu->addSeparator();
        
        QMenu *findMenu = editMenu->addMenu("Find");
        
        findMenu->addAction("Find...", this, &QtSLiMAppDelegate::dispatch_findShow, Qt::CTRL + Qt::Key_F);
        findMenu->addAction("Find Next", this, &QtSLiMAppDelegate::dispatch_findNext, Qt::CTRL + Qt::Key_G)->setEnabled(false);
        findMenu->addAction("Find Previous", this, &QtSLiMAppDelegate::dispatch_findPrevious, Qt::CTRL + Qt::SHIFT + Qt::Key_G)->setEnabled(false);
        findMenu->addAction("Replace && Find", this, &QtSLiMAppDelegate::dispatch_replaceAndFind, Qt::CTRL + Qt::ALT + Qt::Key_G)->setEnabled(false);
        findMenu->addAction("Use Selection for Find", this, &QtSLiMAppDelegate::dispatch_useSelectionForFind, Qt::CTRL + Qt::Key_E)->setEnabled(false);
        findMenu->addAction("Use Selection for Replace", this, &QtSLiMAppDelegate::dispatch_useSelectionForReplace, Qt::CTRL + Qt::ALT + Qt::Key_E)->setEnabled(false);
        findMenu->addAction("Jump to Selection", this, &QtSLiMAppDelegate::dispatch_jumpToSelection, Qt::CTRL + Qt::Key_J)->setEnabled(false);
        findMenu->addAction("Jump to Line", this, &QtSLiMAppDelegate::dispatch_jumpToLine, Qt::CTRL + Qt::Key_L)->setEnabled(false);
        
        QMenu *helpMenu = new QMenu("Help", windowlessMenuBar);
        
        windowlessMenuBar->addMenu(helpMenu);
        
        helpMenu->addAction("SLiMgui Help", this, &QtSLiMAppDelegate::dispatch_help);
        helpMenu->addAction("SLiMgui Workshops", this, &QtSLiMAppDelegate::dispatch_helpWorkshops);
        helpMenu->addSeparator();
        helpMenu->addAction("Send Feedback on SLiM", this, &QtSLiMAppDelegate::dispatch_helpFeedback);
        helpMenu->addAction("Mailing List: slim-announce", this, &QtSLiMAppDelegate::dispatch_helpSLiMAnnounce);
        helpMenu->addAction("Mailing List: slim-discuss", this, &QtSLiMAppDelegate::dispatch_helpSLiMDiscuss);
        helpMenu->addSeparator();
        helpMenu->addAction("SLiM Home Page", this, &QtSLiMAppDelegate::dispatch_helpSLiMHome);
        helpMenu->addAction("SLiM-Extras on GitHub", this, &QtSLiMAppDelegate::dispatch_helpSLiMExtras);
        helpMenu->addSeparator();
        helpMenu->addAction("About the Messer Lab", this, &QtSLiMAppDelegate::dispatch_helpMesserLab)->setMenuRole(QAction::NoRole);
        helpMenu->addAction("About Ben Haller", this, &QtSLiMAppDelegate::dispatch_helpBenHaller)->setMenuRole(QAction::NoRole);
        helpMenu->addAction("About Stick Software", this, &QtSLiMAppDelegate::dispatch_helpStickSoftware)->setMenuRole(QAction::NoRole);
    }
#endif
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



























