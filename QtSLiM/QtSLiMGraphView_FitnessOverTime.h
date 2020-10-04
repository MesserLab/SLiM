//
//  QtSLiMGraphView_FitnessOverTime.h
//  SLiM
//
//  Created by Ben Haller on 3/31/2020.
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

#ifndef QTSLIMGRAPHVIEW_FITNESSOVERTIME_H
#define QTSLIMGRAPHVIEW_FITNESSOVERTIME_H

#include <QWidget>

#include "QtSLiMGraphView.h"

class QPixmap;


class QtSLiMGraphView_FitnessOverTime : public QtSLiMGraphView
{
    Q_OBJECT
    
public:
    QtSLiMGraphView_FitnessOverTime(QWidget *parent, QtSLiMWindow *controller);
    virtual ~QtSLiMGraphView_FitnessOverTime() override;
    
    virtual QString graphTitle(void) override;
    virtual QString aboutString(void) override;
    virtual void drawGraph(QPainter &painter, QRect interiorRect) override;
    virtual bool providesStringForData(void) override;
    virtual void appendStringForData(QString &string) override;    
    virtual void subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent *event) override;
    
public slots:
    virtual void invalidateDrawingCache(void) override;
    virtual void controllerRecycled(void) override;
    virtual void controllerSelectionChanged(void) override;
    virtual void updateAfterTick(void) override;
    void toggleShowSubpopulations(void);
    void toggleDrawLines(void);
    
protected:
    virtual QtSLiMLegendSpec legendKey(void) override;    
    
private:
    bool showSubpopulations_ = false;
    bool drawLines_ = false;
    
    QPixmap *drawingCache_ = nullptr;
	slim_generation_t drawingCacheGeneration_ = 0;
    
    void setDefaultYAxisRange(void);
    
    void drawPointGraph(QPainter &painter, QRect interiorRect);
    void drawLineGraph(QPainter &painter, QRect interiorRect);
};


#endif // QTSLIMGRAPHVIEW_FITNESSOVERTIME_H





































