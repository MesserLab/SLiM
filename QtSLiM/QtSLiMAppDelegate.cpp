//
//  QtSLiMAppDelegate.cpp
//  SLiM
//
//  Created by Ben Haller on 7/13/2019.
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
#include "QtSLiMTablesDrawer.h"
#include "QtSLiMDebugOutputWindow.h"
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
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QLabel>
#include <QDebug>

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string>
#include <algorithm>

#include "eidos_globals.h"
#include "eidos_beep.h"
#include "slim_globals.h"


// Check the Qt version and display an error if it is unacceptable.  This can be turned off with
// -D NO_QT_VERSION_ERROR; at present, that is used to allow GitHub Actions to test version
// combinations that are not officially supported.  I do not recommend that end users use this flag.
#ifdef NO_QT_VERSION_ERROR

#warning "Qt version check for SLiMgui disabled by -D NO_QT_VERSION_ERROR"

#else

#ifdef __APPLE__
// On macOS we enforce Qt 5.15.2 as a hard limit; macOS does not have Qt preinstalled, and there is
// not much reason for anybody to use a version prior to 5.15.2 for a build.  5.15.2 is the only
// LTS version with support for macOS 11, dark mode, and various other things we want.  However,
// if you need to build against an earlier Qt version (because you're using a macOS version earlier
// than 10.13, perhaps), you can disable this check using the above flag and your build will probably
// work; just note that that configuration is unsupported.
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 2))
#error "SLiMgui on macOS requires Qt version 5.15.2 or later.  Please uninstall Qt and then install a more recent version (5.15.2 recommended)."
#endif
#else
// On Linux we enforce Qt 5.9.5 as a hard limit, since it is what Ubuntu 18.04 LTS has preinstalled.
// We don't rely on any specific post 5.9.5 features on Linux, and the best Qt version on Linux is
// probably the version that a given distro has chosen to preinstall.
#if (QT_VERSION < QT_VERSION_CHECK(5, 9, 5))
#error "SLiMgui on Linux requires Qt version 5.9.5 or later.  Please uninstall Qt and then install a more recent version (5.12 LTS or 5.15 LTS recommended)."
#endif
#endif

// Also now check for a Qt version of 6.0 or greater and decline; that is completely untested
// and probably doesn't even compile.  That will be a whole new adventure.
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#error "SLiMgui does not build against Qt 6.  Please uninstall Qt 6 and install Qt 5 (5.15.2 recommended)."
#endif

#endif


static std::string Eidos_Beep_QT(const std::string &p_sound_name);


QtSLiMAppDelegate *qtSLiMAppDelegate = nullptr;


// A custom message handler that we can use, optionally, for debugging.  This is useful if Qt is emitting a warning and you don't know
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
        
        fflush(stderr);
    }
}


QtSLiMAppDelegate::QtSLiMAppDelegate(QObject *p_parent) : QObject(p_parent)
{
    // Install a message handler; this seems useful for debugging
    qInstallMessageHandler(QtSLiM_MessageHandler);
    
    // Determine whether we were launched from a shell or from something else (Finder, Xcode, etc.)
	launchedFromShell_ = (isatty(fileno(stdin)) == 1); // && !SLiM_AmIBeingDebugged();
    
    // Install our custom beep handler
    Eidos_Beep = &Eidos_Beep_QT;
    
    // Let Qt know who we are, for QSettings configuration
    QCoreApplication::setOrganizationName("MesserLab");
    QCoreApplication::setOrganizationDomain("messerlab.org");   // Qt expects the domain in the standard order, and reverses it to form "org.messerlab.SLiMgui.plist" as per Apple's usage
    QCoreApplication::setApplicationName("SLiMgui");             // This governs the location of our prefs, which we keep under org.messerlab.SLiMgui
    QCoreApplication::setApplicationVersion(SLIM_VERSION_STRING);
    
    // Warm up our back ends before anything else happens, including our own class objects
    Eidos_WarmUp();
    SLiM_WarmUp();
    
    gSLiM_SLiMgui_Class = new SLiMgui_Class(gStr_SLiMgui, gEidosDictionaryUnretained_Class);
    gSLiM_SLiMgui_Class->CacheDispatchTables();

    // Free the Eidos RNG; we want it to be uninitialized whenever we are not executing a
    // simulation.  We keep our own RNG state, per-window.  Note that SLiMguiLegacy does
    // not contain code corresponding to this in its delegate, because the order of its
    // initialization is different; the Eidos RNG gets freed by -startNewSimulationFromScript
    // before it causes any trouble.
	if (gEidos_RNG_Initialized)
	{
		_Eidos_FreeOneRNG(gEidos_RNG_SINGLE);
		gEidos_RNG_Initialized = false;
	}
    
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
}

QtSLiMAppDelegate::~QtSLiMAppDelegate(void)
{
    qtSLiMAppDelegate = nullptr;    // kill the global shared instance, for safety
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
        if (mainWin && (mainWin->isZombieWindow_ == false) && (mainWin->currentFile == canonicalFilePath))
            return mainWin;
    }

    return nullptr;
}

void QtSLiMAppDelegate::newFile_WF(bool includeComments)
{
    QtSLiMWindow *currentActiveWindow = activeQtSLiMWindow();
    QtSLiMWindow *window = new QtSLiMWindow(QtSLiMWindow::ModelType::WF, includeComments);
    
    window->tile(currentActiveWindow);
    window->show();
}

void QtSLiMAppDelegate::newFile_nonWF(bool includeComments)
{
    QtSLiMWindow *currentActiveWindow = activeQtSLiMWindow();
    QtSLiMWindow *window = new QtSLiMWindow(QtSLiMWindow::ModelType::nonWF, includeComments);
    
    window->tile(currentActiveWindow);
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
    if (recipeName.endsWith(".py"))
    {
        // Python recipes live in GitHub and get opened in the user's browser.  This seems like the best solution;
        // we don't want SLiMgui to become a Python IDE, so we don't want to open them in-app, but there is no
        // good cross-platform solution for opening them in an app that makes sense, or even for revealing them
        // on the desktop.  See https://github.com/MesserLab/SLiM/issues/137 for discussion.
        QString urlString = "https://raw.githubusercontent.com/MesserLab/SLiM/master/QtSLiM/recipes/";
        urlString.append(recipeName);
        
        QDesktopServices::openUrl(QUrl(urlString, QUrl::TolerantMode));
    }
    else
    {
        // SLiM recipes get opened in a new window (or reusing the current window, if applicable)
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
    
    // BCH 11/14/2023: Note that on certain platforms, QCollator seems to do the wrong thing here.  This may be
    // related to https://bugreports.qt.io/browse/QTBUG-54537.  It's their bug, and fairly harmless; so it goes.
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
                    case 10:chapterName = "Context-dependent selection using mutationEffect() callbacks";		break;
                    case 11:chapterName = "Complex mating schemes using mateChoice() callbacks";				break;
                    case 12:chapterName = "Direct child modifications using modifyChild() callbacks";			break;
                    case 13:chapterName = "Phenotypes, fitness functions, quantitative traits, and QTLs";		break;
                    case 14:chapterName = "Advanced WF models";													break;
                    case 15:chapterName = "Going beyond Wright-Fisher models: nonWF model recipes";             break;
                    case 16:chapterName = "Continuous-space models, interactions, and spatial maps";			break;
                    case 17:chapterName = "Tree-sequence recording: tracking population history";				break;
                    case 18:chapterName = "Modeling explicit nucleotides";										break;
                    case 19:chapterName = "Multispecies modeling";                                              break;
                    case 22:chapterName = "Parallel SLiM: Running SLiM multithreaded";                          break;
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
    QList<QAction *> actions = menu->actions();
    
    for (int i = 0 ; i < MaxRecentFiles; ++i)
    {
        QAction *recentAction = actions[i];
        
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

bool QtSLiMAppDelegate::eventFilter(QObject *p_obj, QEvent *p_event)
{
    QEvent::Type type = p_event->type();
    
    if ((type == QEvent::KeyPress) || (type == QEvent::KeyRelease))
    {
        // emit modifier changed signals for use by the app
        QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(p_event);
        
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
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(p_event);
        QString filePath = openEvent->file();
        
        openFile(filePath, nullptr);
        
        return true;    // filter this event, i.e., prevent any further Qt handling of it
    }
    else if (type == QEvent::ApplicationPaletteChange)
    {
        if (inDarkMode_ != QtSLiMInDarkMode())
        {
            inDarkMode_ = QtSLiMInDarkMode();
            //qDebug() << "inDarkMode_ == " << inDarkMode_;
            emit applicationPaletteChanged();
        }
    }
    else if (type == QEvent::StatusTip)
    {
        // These are events that Qt sends to itself, to display "status tips" in the status bar for widgets the
        // mouse is over, like buttons and menu bar items.  This is not a feature I presently want to use, and
        // on Linux these events get sent even when the tip is empty, and cause the status bar to be cleared
        // for no apparent reason.  So I'm going to just disable these events for now.

        return true;    // filter this event, i.e., prevent any further Qt handling of it
    }
    
    // standard event processing
    return QObject::eventFilter(p_obj, p_event);
}


//
//  public slots
//

void QtSLiMAppDelegate::appDidFinishLaunching(QtSLiMWindow *initialWindow)
{
    // Display a startup message in the initial window's status bar
    if (initialWindow)
        initialWindow->displayStartupMessage();
    
    // Cache our dark mode flag so we can respond to palette changes later
    inDarkMode_ = QtSLiMInDarkMode();
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
        const QStringList resourceNames = findRecipePanel.selectedRecipeFilenames();
        
        for (const QString &resourceName : resourceNames)
        {
            //qDebug() << "recipe name:" << resourceName;
            
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

void QtSLiMAppDelegate::openRecipe(void)
{
    QAction *action = qobject_cast<QAction *>(sender());
    
    if (action)
    {
        const QVariant &data = action->data();
        QString resourceName = data.toString();
        
        if (resourceName.length())
        {
            //qDebug() << "recipe name:" << resourceName;
            
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
    const QWidgetList topLevelWidgets = qApp->topLevelWidgets();
    
    for (QWidget *widget : topLevelWidgets)
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
        QAction *actionShowCycle_WF = new QAction("Show WF Tick Cycle", this);
        //actionAbout->setShortcut(Qt::CTRL + Qt::Key_Comma);
        connect(actionShowCycle_WF, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_showCycle_WF);
        window->addAction(actionShowCycle_WF);
    }
    {
        QAction *actionShowCycle_nonWF = new QAction("Show nonWF Tick Cycle", this);
        //actionAbout->setShortcut(Qt::CTRL + Qt::Key_Comma);
        connect(actionShowCycle_nonWF, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_showCycle_nonWF);
        window->addAction(actionShowCycle_nonWF);
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
        QAction *actionNewWF_commentless = new QAction("New WF (Commentless)", this);
        actionNewWF_commentless->setShortcut(Qt::CTRL + Qt::AltModifier + Qt::Key_N);
        connect(actionNewWF_commentless, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_newWF_commentless);
        window->addAction(actionNewWF_commentless);
    }
    {
        QAction *actionNewNonWF = new QAction("New nonWF", this);
        actionNewNonWF->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_N);
        connect(actionNewNonWF, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_newNonWF);
        window->addAction(actionNewNonWF);
    }
    {
        QAction *actionNewNonWF_commentless = new QAction("New nonWF (Commentless)", this);
        actionNewNonWF_commentless->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::AltModifier + Qt::Key_N);
        connect(actionNewNonWF_commentless, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_newNonWF_commentless);
        window->addAction(actionNewNonWF_commentless);
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
        QAction *actionFocusOnScript = new QAction("Focus on Script", this);
        actionFocusOnScript->setShortcut(Qt::CTRL + Qt::Key_1);
        connect(actionFocusOnScript, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_focusOnScript);
        window->addAction(actionFocusOnScript);
    }
    {
        QAction *actionFocusOnConsole = new QAction("Focus on Console", this);
        actionFocusOnConsole->setShortcut(Qt::CTRL + Qt::Key_2);
        connect(actionFocusOnConsole, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_focusOnConsole);
        window->addAction(actionFocusOnConsole);
    }
    {
        QAction *actionCheckScript = new QAction("Check Script", this);
        actionCheckScript->setShortcut(Qt::CTRL + Qt::Key_Equal);
        connect(actionCheckScript, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_checkScript);
        window->addAction(actionCheckScript);
    }
    {
        QAction *actionPrettyprintScript = new QAction("Prettyprint Script", this);
        //actionPrettyprintScript->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_Equal);       // this now goes to actionBiggerFont
        connect(actionPrettyprintScript, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_prettyprintScript);
        window->addAction(actionPrettyprintScript);
    }
    {
        QAction *actionReformatScript = new QAction("Reformat Script", this);               // removed since the shortcut for actionPrettyprintScript was removed
        //actionReformatScript->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::ALT + Qt::Key_Equal);
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
        QAction *actionBiggerFont = new QAction("Bigger Font", this);
        actionBiggerFont->setShortcut(Qt::CTRL + Qt::Key_Plus);
        connect(actionBiggerFont, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_biggerFont);
        window->addAction(actionBiggerFont);
    }
    {
        QAction *actionSmallerFont = new QAction("Smaller Font", this);
        actionSmallerFont->setShortcut(Qt::CTRL + Qt::Key_Minus);
        connect(actionSmallerFont, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_smallerFont);
        window->addAction(actionSmallerFont);
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
        QAction *actionClearDebug = new QAction("Clear Debug Points", this);
        actionClearDebug->setShortcut(Qt::CTRL + Qt::ALT + Qt::Key_K);
        connect(actionClearDebug, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_clearDebugPoints);
        window->addAction(actionClearDebug);
    }
    {
        QAction *actionShowDebuggingOutput = new QAction("Show Debugging Output", this);
        actionShowDebuggingOutput->setShortcut(Qt::CTRL + Qt::Key_D);
        connect(actionShowDebuggingOutput, &QAction::triggered, qtSLiMAppDelegate, &QtSLiMAppDelegate::dispatch_showDebuggingOutput);
        window->addAction(actionShowDebuggingOutput);
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

QtSLiMWindow *QtSLiMAppDelegate::dispatchQtSLiMWindowFromSecondaries(void)
{
    // The QtSLiMWindow associated with the focused widget; this should be used for
    // dispatching actions that we want to work inside secondary windows that are
    // associated with a particular QtSLiMWindow.
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : nullptr);
    
    // If we have no focused widget, just work from the active window
    if (!focusWindow)
        focusWindow = activeWindow();
    
    QtSLiMWindow *slimWindow = dynamic_cast<QtSLiMWindow*>(focusWindow);
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    QtSLiMVariableBrowser *varBrowser = dynamic_cast<QtSLiMVariableBrowser*>(focusWindow);
    QtSLiMTablesDrawer *tablesDrawer = dynamic_cast<QtSLiMTablesDrawer*>(focusWindow);
    QtSLiMDebugOutputWindow *debugOutputWindow = dynamic_cast<QtSLiMDebugOutputWindow*>(focusWindow);
    
    if (eidosConsole)
        slimWindow = eidosConsole->parentSLiMWindow;
    else if (varBrowser)
        slimWindow = varBrowser->parentEidosConsole->parentSLiMWindow;
    else if (tablesDrawer)
        slimWindow = tablesDrawer->parentSLiMWindow;
    else if (debugOutputWindow)
        slimWindow = debugOutputWindow->parentSLiMWindow;
    
    // If we still can't find it, try using the parent of the active window
    // This makes graph windows work for dispatch; they are just QWidgets but their parent is correct
    if (focusWindow && !slimWindow)
        slimWindow = dynamic_cast<QtSLiMWindow*>(focusWindow->parent());
    
    return slimWindow;
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

QWidget *QtSLiMAppDelegate::globalImageWindowWithPath(const QString &path, const QString &title, double scaleFactor)
{
    // This is based on QtSLiMWindow::imageWindowWithPath(), but makes a simple global window
    // without context menus and other fluff, for simple display of help-related images
    QImage image(path);
    
    if (image.isNull())
    {
        qApp->beep();
        return nullptr;
    }
    
    int window_width = round(image.width() * scaleFactor);
    int window_height = round(image.height() * scaleFactor);
    
    QWidget *image_window = new QWidget(nullptr, Qt::Window | Qt::Tool);    // a parentless standalone window
    
    image_window->setWindowTitle(title);
    image_window->setFixedSize(window_width, window_height);
    
    // Make the image view
    QLabel *imageView = new QLabel();
    
    imageView->setStyleSheet("QLabel { background-color : white; }");
    imageView->setBackgroundRole(QPalette::Base);
    imageView->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageView->setScaledContents(true);
    imageView->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    imageView->setPixmap(QPixmap::fromImage(image));
    
    // Install imageView in the window
    QVBoxLayout *topLayout = new QVBoxLayout;
    
    image_window->setLayout(topLayout);
    topLayout->setMargin(0);
    topLayout->setSpacing(0);
    topLayout->addWidget(imageView);
    
    // Position the window nicely
    //positionNewSubsidiaryWindow(image_window);
    
    // make window actions for all global menu items
    // this does not seem to be necessary on macOS, but maybe it is on Linux; will need testing FIXME
    //qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
    
    image_window->setAttribute(Qt::WA_DeleteOnClose, true);
    
    return image_window;
}

void QtSLiMAppDelegate::dispatch_showCycle_WF(void)
{
    QWidget *imageWindow = globalImageWindowWithPath(":/help/TickCycle_WF.png", "WF Cycle", 0.32);
    
    if (imageWindow)
    {
        imageWindow->show();
        imageWindow->raise();
        imageWindow->activateWindow();
    }
}

void QtSLiMAppDelegate::dispatch_showCycle_nonWF(void)
{
    QWidget *imageWindow = globalImageWindowWithPath(":/help/TickCycle_nonWF.png", "nonWF Cycle", 0.32);
    
    if (imageWindow)
    {
        imageWindow->show();
        imageWindow->raise();
        imageWindow->activateWindow();
    }
}

void QtSLiMAppDelegate::dispatch_help(void)
{
    QtSLiMHelpWindow &helpWindow = QtSLiMHelpWindow::instance();
    
    helpWindow.show();
    helpWindow.raise();
    helpWindow.activateWindow();
}

void QtSLiMAppDelegate::dispatch_biggerFont(void)
{
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    
    prefs.displayFontBigger();
}

void QtSLiMAppDelegate::dispatch_smallerFont(void)
{
    QtSLiMPreferencesNotifier &prefs = QtSLiMPreferencesNotifier::instance();
    
    prefs.displayFontSmaller();
}

void QtSLiMAppDelegate::dispatch_quit(void)
{
    if (qApp)
    {
        closeRejected_ = false;
        qApp->closeAllWindows();
        
        if (!closeRejected_)
        {
            // On macOS, explicitly quit since last window close doesn't auto-quit, for Qt 5.15.2.
            // Builds against older Qt versions will just quit on the last window close, because
            // QTBUG-86874 and QTBUG-86875 prevent this from working.
#ifdef __APPLE__
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 2))
            qApp->quit();
#endif
#endif
        }
    }
}

void QtSLiMAppDelegate::dispatch_newWF(void)
{
    newFile_WF(true);
}

void QtSLiMAppDelegate::dispatch_newWF_commentless(void)
{
    newFile_WF(false);
}

void QtSLiMAppDelegate::dispatch_newNonWF(void)
{
    newFile_nonWF(true);
}

void QtSLiMAppDelegate::dispatch_newNonWF_commentless(void)
{
    newFile_nonWF(false);
}

void QtSLiMAppDelegate::dispatch_open(void)
{
    open(nullptr);
}

void QtSLiMAppDelegate::dispatch_close(void)
{
    // We close the "active" window, which is a bit different from the front window
    // It can be nullptr; in that case it's hard to know what to do
    QWidget *currentActiveWindow = QApplication::activeWindow();
    
    if (currentActiveWindow)
        currentActiveWindow->close();
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
    QPlainTextEdit *plainTextEdit = dynamic_cast<QPlainTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
        lineEdit->undo();
    else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
        textEdit->undo();
    else if (plainTextEdit && plainTextEdit->isEnabled() && !plainTextEdit->isReadOnly())
        plainTextEdit->undo();
}

void QtSLiMAppDelegate::dispatch_redo(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
    QPlainTextEdit *plainTextEdit = dynamic_cast<QPlainTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
        lineEdit->redo();
    else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
        textEdit->redo();
    else if (plainTextEdit && plainTextEdit->isEnabled() && !plainTextEdit->isReadOnly())
        plainTextEdit->redo();
}

void QtSLiMAppDelegate::dispatch_cut(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
    QPlainTextEdit *plainTextEdit = dynamic_cast<QPlainTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
        lineEdit->cut();
    else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
        textEdit->cut();
    else if (plainTextEdit && plainTextEdit->isEnabled() && !plainTextEdit->isReadOnly())
        plainTextEdit->cut();
}

void QtSLiMAppDelegate::dispatch_copy(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
    QPlainTextEdit *plainTextEdit = dynamic_cast<QPlainTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled())
        lineEdit->copy();
    else if (textEdit && textEdit->isEnabled())
        textEdit->copy();
    else if (plainTextEdit && plainTextEdit->isEnabled())
        plainTextEdit->copy();
}

void QtSLiMAppDelegate::dispatch_paste(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
    QPlainTextEdit *plainTextEdit = dynamic_cast<QPlainTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
        lineEdit->paste();
    else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
        textEdit->paste();
    else if (plainTextEdit && plainTextEdit->isEnabled() && !plainTextEdit->isReadOnly())
        plainTextEdit->paste();
}

void QtSLiMAppDelegate::dispatch_delete(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
    QPlainTextEdit *plainTextEdit = dynamic_cast<QPlainTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled() && !lineEdit->isReadOnly())
        lineEdit->insert("");
    else if (textEdit && textEdit->isEnabled() && !textEdit->isReadOnly())
        textEdit->insertPlainText("");
    else if (plainTextEdit && plainTextEdit->isEnabled() && !plainTextEdit->isReadOnly())
        plainTextEdit->insertPlainText("");
}

void QtSLiMAppDelegate::dispatch_selectAll(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QLineEdit *lineEdit = dynamic_cast<QLineEdit*>(focusWidget);
    QTextEdit *textEdit = dynamic_cast<QTextEdit*>(focusWidget);
    QPlainTextEdit *plainTextEdit = dynamic_cast<QPlainTextEdit*>(focusWidget);
    
    if (lineEdit && lineEdit->isEnabled())
        lineEdit->selectAll();
    else if (textEdit && textEdit->isEnabled())
        textEdit->selectAll();
    else if (plainTextEdit && plainTextEdit->isEnabled())
        plainTextEdit->selectAll();
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

void QtSLiMAppDelegate::dispatch_focusOnScript(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : activeWindow());
    QtSLiMWindow *slimWindow = dynamic_cast<QtSLiMWindow*>(focusWindow);
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    
    if (slimWindow)
        slimWindow->scriptTextEdit()->setFocus();
    else if (eidosConsole)
        eidosConsole->scriptTextEdit()->setFocus();
}

void QtSLiMAppDelegate::dispatch_focusOnConsole(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : activeWindow());
    QtSLiMWindow *slimWindow = dynamic_cast<QtSLiMWindow*>(focusWindow);
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    
    if (slimWindow)
        slimWindow->outputTextEdit()->setFocus();
    else if (eidosConsole)
        eidosConsole->consoleTextEdit()->setFocus();
}

void QtSLiMAppDelegate::dispatch_checkScript(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : activeWindow());
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
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : activeWindow());
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
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : activeWindow());
    QtSLiMWindow *slimWindow = dynamic_cast<QtSLiMWindow*>(focusWindow);
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    
    if (slimWindow)
        slimWindow->scriptTextEdit()->reformat();
    else if (eidosConsole)
        eidosConsole->scriptTextEdit()->reformat();
}

void QtSLiMAppDelegate::dispatch_showEidosConsole(void)
{
    QtSLiMWindow *slimWindow = dispatchQtSLiMWindowFromSecondaries();
    
    if (slimWindow)
        slimWindow->showConsoleClicked();
}

void QtSLiMAppDelegate::dispatch_showVariableBrowser(void)
{
    QtSLiMWindow *slimWindow = dispatchQtSLiMWindowFromSecondaries();
    
    if (slimWindow)
        slimWindow->showBrowserClicked();
}

void QtSLiMAppDelegate::dispatch_showDebuggingOutput(void)
{
    QtSLiMWindow *slimWindow = dispatchQtSLiMWindowFromSecondaries();
    
    if (slimWindow)
        slimWindow->debugOutputClicked();
}

void QtSLiMAppDelegate::dispatch_clearOutput(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : activeWindow());
    QtSLiMWindow *slimWindow = dynamic_cast<QtSLiMWindow*>(focusWindow);
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    
    if (slimWindow)
        slimWindow->clearOutputClicked();
    else if (eidosConsole)
        eidosConsole->consoleTextEdit()->clearToPrompt();
}

void QtSLiMAppDelegate::dispatch_clearDebugPoints(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : activeWindow());
    QtSLiMWindow *slimWindow = dynamic_cast<QtSLiMWindow*>(focusWindow);
    
    if (slimWindow)
        slimWindow->clearDebugPointsClicked();
}

void QtSLiMAppDelegate::dispatch_executeSelection(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : activeWindow());
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    
    if (eidosConsole)
        eidosConsole->executeSelectionClicked();
}

void QtSLiMAppDelegate::dispatch_executeAll(void)
{
    QWidget *focusWidget = QApplication::focusWidget();
    QWidget *focusWindow = (focusWidget ? focusWidget->window() : activeWindow());
    QtSLiMEidosConsole *eidosConsole = dynamic_cast<QtSLiMEidosConsole*>(focusWindow);
    
    if (eidosConsole)
        eidosConsole->executeAllClicked();
}

void QtSLiMAppDelegate::dispatch_minimize(void)
{
    // We minimize the "active" window, which is a bit different from the front window
    // It can be nullptr; in that case it's hard to know what to do
    QWidget *currentActiveWindow = QApplication::activeWindow();
    
    if (currentActiveWindow)
    {
        bool isMinimized = currentActiveWindow->windowState() & Qt::WindowMinimized;
        
        if (isMinimized)
            currentActiveWindow->setWindowState((currentActiveWindow->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        else
            currentActiveWindow->setWindowState(currentActiveWindow->windowState() | Qt::WindowMinimized);    // Qt refuses to minimize Qt::Tool windows; see https://bugreports.qt.io/browse/QTBUG-86520
    }
    else
        qApp->beep();
}

void QtSLiMAppDelegate::dispatch_zoom(void)
{
    // We zoom the "active" window, which is a bit different from the front window
    // It can be nullptr; in that case it's hard to know what to do
    QWidget *currentActiveWindow = QApplication::activeWindow();
    
    if (currentActiveWindow)
    {
        bool isZoomed = currentActiveWindow->windowState() & Qt::WindowMaximized;
        
        if (isZoomed)
            currentActiveWindow->setWindowState((currentActiveWindow->windowState() & ~Qt::WindowMaximized) | Qt::WindowActive);
        else
            currentActiveWindow->setWindowState((currentActiveWindow->windowState() | Qt::WindowMaximized) | Qt::WindowActive);
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
        {
            // prune zombie windows
            bool isZombie = false;
            QtSLiMWindow *qtSLiMWindow = qobject_cast<QtSLiMWindow *>(focused_window_ptr);
            
            if (qtSLiMWindow)
                isZombie = qtSLiMWindow->isZombieWindow_;
            
            if (!isZombie)
                continue;
        }
        
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
    // We use this for dispatching commands that could go to any QtSLiMWindow, but that
    // open a new document window that gets tiled; it's best to tile from the user's
    // frontmost document window, I suppose.  We do not use it for other dispatch; see
    // dispatchQtSLiMWindowFromSecondaries() for that.
    QWidget *currentActiveWindow = qApp->activeWindow();
    QtSLiMWindow *currentActiveQtSLiMWindow = qobject_cast<QtSLiMWindow *>(currentActiveWindow);
    
    if (currentActiveQtSLiMWindow && !currentActiveQtSLiMWindow->isZombieWindow_)
        return currentActiveQtSLiMWindow;
    
    // If that fails, use the last focused main window, as tracked by focusChanged()
    pruneWindowList();
    
    for (QPointer<QWidget> &focused_window_ptr : focusedWindowList)
    {
        if (focused_window_ptr)
        {
            QWidget *focused_window = focused_window_ptr.data();
            QtSLiMWindow *focusedQtSLiMWindow = qobject_cast<QtSLiMWindow *>(focused_window);
            
            if (focusedQtSLiMWindow && !focusedQtSLiMWindow->isZombieWindow_)
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
std::string Eidos_Beep_QT(const std::string &__attribute__((__unused__)) p_sound_name)
{
    std::string return_string;
    
    qApp->beep();
    
    return return_string;
}



























