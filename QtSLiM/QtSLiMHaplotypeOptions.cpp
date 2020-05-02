//
//  QtSLiMHaplotypeOptions.cpp
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

#include "QtSLiMHaplotypeOptions.h"
#include "ui_QtSLiMHaplotypeOptions.h"

#include <QApplication>

#include "QtSLiMAppDelegate.h"


QtSLiMHaplotypeOptions::QtSLiMHaplotypeOptions(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QtSLiMHaplotypeOptions)
{
    ui->setupUi(this);
    
    // no window icon
#ifdef __APPLE__
    // set the window icon only on macOS; on Linux it changes the app icon as a side effect
    setWindowIcon(QIcon());
#endif
    
    // change the app icon to our multi-size app icon for best results
    ui->appIconButton->setIcon(qtSLiMAppDelegate->applicationIcon());
    
    // fix sizing
    setFixedSize(sizeHint());
    setSizeGripEnabled(false);
    
    // enabled/disable the sample size lineEdit
    connect(ui->genomesSampleRadio, &QAbstractButton::toggled, this, [this]() { ui->sampleSizeLineEdit->setEnabled(ui->genomesSampleRadio->isChecked()); });
}

QtSLiMHaplotypeOptions::~QtSLiMHaplotypeOptions()
{
    delete ui;
}

void QtSLiMHaplotypeOptions::done(int r)
{
    // do validation; see https://www.qtcentre.org/threads/8048-Validate-Data-in-QDialog
    bool usingSampleSize = ui->genomesSampleRadio->isChecked();
    
    if ((QDialog::Accepted == r) && usingSampleSize)  // ok was pressed and the sample size field is being used
    {
        QString sampleSizeText = ui->sampleSizeLineEdit->text();
        size_t sampleSizeInt = sampleSizeText.toULong();
        QString validatedText = QString("%1").arg(sampleSizeInt);
        
        if ((sampleSizeText == validatedText) && (sampleSizeInt > 1))
        {
            QDialog::done(r);
            return;
        }
        else
        {
            qApp->beep();
            return;
        }
    }
    else    // cancel, close or exc was pressed
    {
        QDialog::done(r);
        return;
    }
}

size_t QtSLiMHaplotypeOptions::genomeSampleSize(void)
{
    bool usingSampleSize = ui->genomesSampleRadio->isChecked();
    
    if (!usingSampleSize)
        return 0;
    
    return ui->sampleSizeLineEdit->text().toULong();
}

QtSLiMHaplotypeManager::ClusteringMethod QtSLiMHaplotypeOptions::clusteringMethod(void)
{
    if (ui->clusterNearestRadio->isChecked())       return QtSLiMHaplotypeManager::ClusterNearestNeighbor;
    if (ui->clusterGreedyRadio->isChecked())        return QtSLiMHaplotypeManager::ClusterGreedy;
    if (ui->clusterGreedyOpt2Radio->isChecked())    return QtSLiMHaplotypeManager::ClusterGreedy;
    return QtSLiMHaplotypeManager::ClusterNearestNeighbor;
}

QtSLiMHaplotypeManager::ClusteringOptimization QtSLiMHaplotypeOptions::clusteringOptimization(void)
{
    if (ui->clusterNearestRadio->isChecked())       return QtSLiMHaplotypeManager::ClusterNoOptimization;
    if (ui->clusterGreedyRadio->isChecked())        return QtSLiMHaplotypeManager::ClusterNoOptimization;
    if (ui->clusterGreedyOpt2Radio->isChecked())    return QtSLiMHaplotypeManager::ClusterOptimizeWith2opt;
    return QtSLiMHaplotypeManager::ClusterNoOptimization;
}





































