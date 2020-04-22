//
//  QtSLiMHaplotypeProgress.cpp
//  SLiM
//
//  Created by Ben Haller on 4/4/2020.
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

#include "QtSLiMHaplotypeProgress.h"
#include "ui_QtSLiMHaplotypeProgress.h"

#include "QtSLiMAppDelegate.h"


QtSLiMHaplotypeProgress::QtSLiMHaplotypeProgress(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QtSLiMHaplotypeProgress)
{
    ui->setupUi(this);
    
    // no window icon
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    setWindowIcon(QIcon());
#endif
    
    // change the app icon to our multi-size app icon for best results
    ui->appIconButton->setIcon(qtSLiMAppDelegate->applicationIcon());
    
    // wire up cancel button
    connect(ui->cancelButton, &QPushButton::clicked, this, [this]() { cancelled_ = true; });
}

QtSLiMHaplotypeProgress::~QtSLiMHaplotypeProgress()
{
    delete ui;
}

void QtSLiMHaplotypeProgress::runProgressWithGenomeCount(size_t genome_count, int stepCount)
{
    // set up initial state
    taskDistances_Value_ = 0;
	taskClustering_Value_ = 0;
	taskOptimization_Value_ = 0;
    
    ui->step1ProgressBar->setRange(0, static_cast<int>(genome_count));
    ui->step2ProgressBar->setRange(0, static_cast<int>(genome_count));
    ui->step3ProgressBar->setRange(0, static_cast<int>(genome_count));
    
    ui->step1ProgressBar->setValue(0);
    ui->step2ProgressBar->setValue(0);
    ui->step3ProgressBar->setValue(0);
    
    // if we're not doing an optimization step, remove those controls
    if (stepCount == 2)
    {
        ui->barBoxLayout->removeWidget(ui->step3Box);   // remove from layout
        ui->step3Box->setParent(nullptr);               // remove from parent widget
        delete ui->step3Box;                            // also deletes ui->step3ProgressBar
        ui->step3Box = nullptr;
        ui->step3ProgressBar = nullptr;
    }
    
    // fix sizing
    setFixedSize(sizeHint());
    setSizeGripEnabled(false);
    
    // make the progress window visible/active
    setModal(true);
    show();
}

bool QtSLiMHaplotypeProgress::haplotypeProgressIsCancelled(void)
{
    // spin the event loop for the panel, so the user can click "Cancel"
    if (!cancelled_)
        QCoreApplication::processEvents();
    
    // return the cancelled state
    return cancelled_;
}

void QtSLiMHaplotypeProgress::setHaplotypeProgress(size_t progress, int stage)
{
    switch (stage)
    {
        case 0: taskDistances_Value_ = static_cast<int>(progress); break;
        case 1: taskClustering_Value_ = static_cast<int>(progress); break;
        case 2: taskOptimization_Value_ = static_cast<int>(progress); break;
    }
    
    ui->step1ProgressBar->setValue(taskDistances_Value_);
    ui->step2ProgressBar->setValue(taskClustering_Value_);
    if (ui->step3ProgressBar)
        ui->step3ProgressBar->setValue(taskOptimization_Value_);
}









































