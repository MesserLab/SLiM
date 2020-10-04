//
//  QtSLiMGraphView_PopulationVisualization.h
//  SLiM
//
//  Created by Ben Haller on 3/30/2020.
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

#ifndef QTSLIMGRAPHVIEW_POPULATIONVISUALIZATION_H
#define QTSLIMGRAPHVIEW_POPULATIONVISUALIZATION_H

#include <QWidget>

#include "QtSLiMGraphView.h"


class QtSLiMGraphView_PopulationVisualization : public QtSLiMGraphView
{
    Q_OBJECT
    
public:
    QtSLiMGraphView_PopulationVisualization(QWidget *parent, QtSLiMWindow *controller);
    virtual ~QtSLiMGraphView_PopulationVisualization() override;
    
    virtual QString graphTitle(void) override;
    virtual QString aboutString(void) override;
    virtual void drawGraph(QPainter &painter, QRect interiorRect) override;
    virtual void subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent *event) override;
    virtual void appendStringForData(QString &string) override;
    
public slots:
    void toggleOptimizedPositions(void);
    
private:
    double scalingFactor_;
    
    bool optimizePositions_;
    double scorePositions(double *center_x, double *center_y, bool *connected, size_t subpopCount);
    void optimizePositions(void);    
    
    QRectF rectForSubpop(Subpopulation *subpop, QPointF center);
    void drawSubpop(QPainter &painter, Subpopulation *subpop, slim_objectid_t subpopID, QPointF center);
    void drawArrowFromSubpopToSubpop(QPainter &painter, Subpopulation *sourceSubpop, Subpopulation *destSubpop, double migrantFraction);
};


#endif // QTSLIMGRAPHVIEW_POPULATIONVISUALIZATION_H





































