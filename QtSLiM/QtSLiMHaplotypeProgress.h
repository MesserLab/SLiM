//
//  QtSLiMHaplotypeProgress.h
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

#ifndef QTSLIMHAPLOTYPEPROGRESS_H
#define QTSLIMHAPLOTYPEPROGRESS_H

#include <QDialog>


namespace Ui {
class QtSLiMHaplotypeProgress;
}

class QtSLiMHaplotypeProgress : public QDialog
{
    Q_OBJECT
    
public:
    explicit QtSLiMHaplotypeProgress(QWidget *parent = nullptr);
    virtual ~QtSLiMHaplotypeProgress() override;
    
    void runProgressWithGenomeCount(size_t genome_count, int stepCount);
    bool haplotypeProgressIsCancelled(void);
    
    void setHaplotypeProgress(size_t progress, int stage);
    
private:
    Ui::QtSLiMHaplotypeProgress *ui;
    
    int taskDistances_Value_;
	int taskClustering_Value_;
	int taskOptimization_Value_;
    
    bool cancelled_ = false;
};


#endif // QTSLIMHAPLOTYPEPROGRESS_H





































