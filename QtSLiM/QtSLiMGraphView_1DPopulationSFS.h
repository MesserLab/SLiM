//
//  QtSLiMGraphView_1DPopulationSFS.h
//  SLiM
//
//  Created by Ben Haller on 3/27/2020.
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

#ifndef QTSLIMGRAPHVIEW_1DPOPULATIONSFS_H
#define QTSLIMGRAPHVIEW_1DPOPULATIONSFS_H

#include <QWidget>

#include "QtSLiMGraphView.h"


class QtSLiMGraphView_1DPopulationSFS : public QtSLiMGraphView
{
    Q_OBJECT
    
public:
    QtSLiMGraphView_1DPopulationSFS(QWidget *parent, QtSLiMWindow *controller);
    virtual ~QtSLiMGraphView_1DPopulationSFS() override;
    
    virtual QString graphTitle(void) override;
    virtual QString aboutString(void) override;
    virtual void drawGraph(QPainter &painter, QRect interiorRect) override;
    virtual QtSLiMLegendSpec legendKey(void) override;
    virtual bool providesStringForData(void) override;
    virtual void appendStringForData(QString &string) override;    
    
public slots:
    virtual void controllerSelectionChanged(void) override;
    
private:
    double *populationSFS(int mutationTypeCount);    
};


#endif // QTSLIMGRAPHVIEW_1DPOPULATIONSFS_H





































