//
//  QtSLiMAppDelegate.h
//  SLiM
//
//  Created by Ben Haller on 7/13/2019.
//  Copyright (c) 2019 Philipp Messer.  All rights reserved.
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
#include <string>

class QMenu;
class QAction;
class QtSLiMWindow;
class QtSLiMAppDelegate;
 

extern QtSLiMAppDelegate *qtSLiMAppDelegate;    // global instance

class QtSLiMAppDelegate : public QObject
{
    Q_OBJECT

    std::string app_cwd_;	// the app's current working directory

public:
    explicit QtSLiMAppDelegate(QObject *parent);

    std::string &QtSLiMCurrentWorkingDirectory(void) { return app_cwd_; }
    
    void setUpRecipesMenu(QMenu *openRecipesSubmenu, QAction *findRecipeAction);

public slots:
    void lastWindowClosed(void);
    void aboutToQuit(void);
    
    void findRecipe(void);
    void openRecipe(void);
    
signals:
    void modifiersChanged(Qt::KeyboardModifiers newModifiers);
    
private:
    QtSLiMWindow *activeQtSLiMWindow(void);  
    
    bool eventFilter(QObject *obj, QEvent *event) override;
};


#endif // QTSLIMAPPDELEGATE_H







