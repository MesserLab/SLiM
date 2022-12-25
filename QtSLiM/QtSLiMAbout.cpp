//
//  QtSLiMAbout.cpp
//  SLiM
//
//  Created by Ben Haller on 8/2/2019.
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


#include "QtSLiMAbout.h"
#include "ui_QtSLiMAbout.h"

#include "QtSLiMAppDelegate.h"

#include "slim_globals.h"

// Get our Git commit SHA-1, as C string "g_GIT_SHA1"
#include "../cmake/GitSHA1.h"


QtSLiMAbout::QtSLiMAbout(QWidget *p_parent) : QDialog(p_parent), ui(new Ui::QtSLiMAbout)
{
    ui->setupUi(this);
    
    // change the app icon to our multi-size app icon for best results
    ui->appIconButton->setIcon(qtSLiMAppDelegate->applicationIcon());
    
    // prevent this window from keeping the app running when all main windows are closed
    setAttribute(Qt::WA_QuitOnClose, false);
    
    // disable resizing
    layout()->setSizeConstraint(QLayout::SetFixedSize);
    setSizeGripEnabled(false);
    
    // fix version number; we incorporate the Git commit number here too, if we can
    QString versionString("version " + QString(SLIM_VERSION_STRING) + " (Qt " + QT_VERSION_STR + ", Git SHA-1 ");
    QString gitSHA(g_GIT_SHA1);
    
    if (gitSHA.startsWith("unknown"))
        versionString.append("unknown");
    else if (gitSHA == "GITDIR-NOTFOUND")
        versionString.append("not available");
    else
        versionString.append(gitSHA.left(7));  // standard truncation is the last seven hex digits of the SHA-1
    
    versionString.append(")");
    
    ui->versionLabel->setText(versionString);
    
    // make window actions for all global menu items
    qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
}

QtSLiMAbout::~QtSLiMAbout()
{
    delete ui;
}
