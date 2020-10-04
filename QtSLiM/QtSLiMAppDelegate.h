//
//  QtSLiMAppDelegate.h
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

#ifndef QTSLIMAPPDELEGATE_H
#define QTSLIMAPPDELEGATE_H

#include <QObject>
#include <QVector>
#include <QIcon>
#include <QPointer>
#include <QMainWindow>
#include <string>

class QMenuBar;
class QMenu;
class QAction;
class QtSLiMWindow;
class QtSLiMAppDelegate;
 

extern QtSLiMAppDelegate *qtSLiMAppDelegate;    // global instance

class QtSLiMAppDelegate : public QObject
{
    Q_OBJECT

    std::string app_cwd_;       // the app's current working directory
    bool launchedFromShell_;	// true if launched from shell, false if launched from Finder/other
    
    QIcon appIcon_;
    QIcon appIconHighlighted_;
    QIcon slimDocumentIcon_;
    QIcon genericDocumentIcon_;
    
public:
    explicit QtSLiMAppDelegate(QObject *parent);
    virtual ~QtSLiMAppDelegate(void) override;

    // Whether we were launched from a shell (true) or Finder/other (false)
    bool launchedFromShell(void) { return launchedFromShell_; }
    
    // The current working directory for the app
    std::string &QtSLiMCurrentWorkingDirectory(void) { return app_cwd_; }
    
    // Tracking the current active main window
    QtSLiMWindow *activeQtSLiMWindow(void);             // the frontmost window that is a QtSLiMWindow
    QWidget *activeWindow(void);                        // the frontmost window
    QWidget *activeWindowExcluding(QWidget *excluded);  // the frontmost window that is not excluded
    
    // Document opening
    QtSLiMWindow *findMainWindow(const QString &fileName) const;
    void newFile_WF(void);
    void newFile_nonWF(void);
    QtSLiMWindow *open(QtSLiMWindow *requester);
    QtSLiMWindow *openFile(const QString &fileName, QtSLiMWindow *requester);
    void openRecipeWithName(const QString &recipeName, const QString &recipeScript, QtSLiMWindow *requester);

    // Recipes and Recents menus
    void setUpRecipesMenu(QMenu *openRecipesSubmenu, QAction *findRecipeAction);
    void setUpRecentsMenu(QMenu *openRecentSubmenu);
    
    void prependToRecentFiles(const QString &fileName);
    
    // App-wide shared icons
    QIcon applicationIcon(void) { return appIcon_; }
    QIcon slimDocumentIcon(void) { return slimDocumentIcon_; }
    QIcon genericDocumentIcon(void) { return genericDocumentIcon_; }
    
    // Global actions: designated as "window" actions to avoid ambiguity, but defined on every window and handled by us
    void addActionsForGlobalMenuItems(QWidget *window);
    
public slots:
    void appDidFinishLaunching(QtSLiMWindow *initialWindow);
    
    void findRecipe(void);
    void openRecipe(void);
    
    void playStateChanged(void);
    
    // These are slots for menu bar actions that get dispatched to the focal widget/window by us.
    // Every window should use addActionsForGlobalMenuItems() to connect to these slots (except
    // QtSLiMWindow, which connects the main menu bar actions to these slots instead).  This gives
    // us a little bit of a "first responder" type functionality, where menu items work globally
    // and get dispatched to the right target according to focus.
    void dispatch_preferences(void);
    void dispatch_about(void);
    void dispatch_help(void);
    void dispatch_quit(void);
    
    void dispatch_newWF(void);
    void dispatch_newNonWF(void);
    void dispatch_open(void);
    void dispatch_close(void);
    
    void dispatch_shiftLeft(void);
    void dispatch_shiftRight(void);
    void dispatch_commentUncomment(void);
    
    void dispatch_undo(void);
    void dispatch_redo(void);
    void dispatch_cut(void);
    void dispatch_copy(void);
    void dispatch_paste(void);
    void dispatch_delete(void);
    void dispatch_selectAll(void);
    
    void dispatch_findShow(void);
    void dispatch_findNext(void);
    void dispatch_findPrevious(void);
    void dispatch_replaceAndFind(void);
    void dispatch_useSelectionForFind(void);
    void dispatch_useSelectionForReplace(void);
    void dispatch_jumpToSelection(void);
    void dispatch_jumpToLine(void);
    
    void dispatch_checkScript(void);
    void dispatch_prettyprintScript(void);
    void dispatch_reformatScript(void);
    void dispatch_showEidosConsole(void);
    void dispatch_showVariableBrowser(void);
    void dispatch_clearOutput(void);
    void dispatch_executeSelection(void);
    void dispatch_executeAll(void);

    void dispatch_minimize(void);
    void dispatch_zoom(void);
    
    void dispatch_helpWorkshops(void);
    void dispatch_helpFeedback(void);
    void dispatch_helpSLiMDiscuss(void);
    void dispatch_helpSLiMAnnounce(void);
    void dispatch_helpSLiMHome(void);
    void dispatch_helpSLiMExtras(void);
    void dispatch_helpMesserLab(void);
    void dispatch_helpBenHaller(void);
    void dispatch_helpStickSoftware(void);
    
signals:
    void modifiersChanged(Qt::KeyboardModifiers newModifiers);
    void activeWindowListChanged(void);
    
private:
    virtual bool eventFilter(QObject *obj, QEvent *event) override;
    
    QVector<QPointer<QWidget>> focusedWindowList;       // a list of all windows, from front to back
    void pruneWindowList(void);                         // remove all windows that are closed or hidden
    bool queuedActiveWindowUpdate = false;
    
    void makeGlobalMenuBar(void);                       // make the global menu bar, for use with no main windows open; macOS only, does nothing on other platforms
    QMenuBar *windowlessMenuBar = nullptr;
    
    void updateRecentFileActions(void);
    void openRecentFile(void);
    void clearRecentFiles(void);
    bool hasRecentFiles(void);
    
private slots:
    void lastWindowClosed(void);
    void aboutToQuit(void);
    void focusChanged(QWidget *old, QWidget *now);
    void updateActiveWindowList(void);
};


#endif // QTSLIMAPPDELEGATE_H







































