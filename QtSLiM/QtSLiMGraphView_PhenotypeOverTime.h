//
//  QtSLiMGraphView_PhenotypeOverTime.h
//  SLiM
//
//  Created by Ben Haller on 7/29/2026.
//  Copyright (c) 2026 Benjamin C. Haller.  All rights reserved.
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

#ifndef QTSLIMGRAPHVIEW_PHENOTYPEOVERTIME_H
#define QTSLIMGRAPHVIEW_PHENOTYPEOVERTIME_H

#include <QWidget>

#include "QtSLiMGraphView.h"

#include <string>


class QPixmap;
class Trait;


class QtSLiMGraphView_PhenotypeOverTime : public QtSLiMGraphView
{
    Q_OBJECT
    
public:
    QtSLiMGraphView_PhenotypeOverTime(QWidget *p_parent, QtSLiMWindow *controller);
    virtual ~QtSLiMGraphView_PhenotypeOverTime() override;
    
    virtual QString graphTitle(void) override;
    virtual QString aboutString(void) override;
    virtual void drawGraph(QPainter &painter, QRect interiorRect) override;
    virtual bool providesStringForData(void) override;
    virtual void appendStringForData(QString &string) override;    
    virtual void subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent *p_event) override;
    
public slots:
    virtual void addedToWindow(void) override;
    virtual void invalidateDrawingCache(void) override;
    virtual void controllerRecycled(void) override;
    virtual void updateAfterTick(void) override;
    void toggleShowSubpopulations(void);
    void toggleDrawLines(void);
    void traitPopupChanged(int index);
    void focalTraitChanged(void);
    
protected:
    virtual QtSLiMLegendSpec legendKey(void) override;
    
    void setFocalTrait(Trait *trait);
    Trait *focalTrait(void);
    
private:
    std::string trait_name_;
    
    // pop-up menu buttons
    QComboBox *traitButton_ = nullptr;
    
    // User-selected display prefs
    bool showSubpopulations_ = false;
    bool drawLines_ = false;
    
    QPixmap *drawingCache_ = nullptr;
	slim_tick_t drawingCacheTick_ = 0;
    
    void setDefaultYAxisRange(void);
    
    void drawPointGraph(QPainter &painter, QRect interiorRect);
    void drawLineGraph(QPainter &painter, QRect interiorRect);
};


#endif // QTSLIMGRAPHVIEW_PHENOTYPEOVERTIME_H





































