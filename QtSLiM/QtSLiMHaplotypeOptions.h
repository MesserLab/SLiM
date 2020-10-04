//
//  QtSLiMHaplotypeOptions.h
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

#ifndef QTSLIMHAPLOTYPEOPTIONS_H
#define QTSLIMHAPLOTYPEOPTIONS_H

#include <QDialog>

#include "QtSLiMHaplotypeManager.h"


namespace Ui {
class QtSLiMHaplotypeOptions;
}

class QtSLiMHaplotypeOptions : public QDialog
{
    Q_OBJECT
    
public:
    explicit QtSLiMHaplotypeOptions(QWidget *parent = nullptr);
    virtual ~QtSLiMHaplotypeOptions() override;
    
    size_t genomeSampleSize(void);    // 0 indicates "all genomes"
    QtSLiMHaplotypeManager::ClusteringMethod clusteringMethod(void);
    QtSLiMHaplotypeManager::ClusteringOptimization clusteringOptimization(void);
    
private:
    Ui::QtSLiMHaplotypeOptions *ui;
    
    virtual void done(int r) override;
};


#endif // QTSLIMHAPLOTYPEOPTIONS_H



































