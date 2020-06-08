//
//  QtSLiMAbout.cpp
//  SLiM
//
//  Created by Ben Haller on 8/2/2019.
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


#include "QtSLiMAbout.h"
#include "ui_QtSLiMAbout.h"

#include "QtSLiMAppDelegate.h"

#include "slim_globals.h"


QtSLiMAbout::QtSLiMAbout(QWidget *parent) : QDialog(parent), ui(new Ui::QtSLiMAbout)
{
    ui->setupUi(this);
    
    // change the app icon to our multi-size app icon for best results
    ui->appIconButton->setIcon(qtSLiMAppDelegate->applicationIcon());
    
    // prevent this window from keeping the app running when all main windows are closed
    setAttribute(Qt::WA_QuitOnClose, false);
    
    // disable resizing
    layout()->setSizeConstraint(QLayout::SetFixedSize);
    setSizeGripEnabled(false);
    
    // fix version number; FIXME would be nice to figure out a way to get the build number on Linux...
    ui->versionLabel->setText("version " + QString(SLIM_VERSION_STRING) + " (Qt " + QT_VERSION_STR + ")");
    
    // make window actions for all global menu items
    qtSLiMAppDelegate->addActionsForGlobalMenuItems(this);
}

QtSLiMAbout::~QtSLiMAbout()
{
    delete ui;
}
