//
//  QtSLiMGraphView_AgeDistribution.h
//  SLiM
//
//  Created by Ben Haller on 8/30/2020.
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

#ifndef QTSLIMGRAPHVIEW_AGEDISTRIBUTION_H
#define QTSLIMGRAPHVIEW_AGEDISTRIBUTION_H

#include <QWidget>

#include "QtSLiMGraphView.h"


class QtSLiMGraphView_AgeDistribution : public QtSLiMGraphView
{
    Q_OBJECT
    
public:
    QtSLiMGraphView_AgeDistribution(QWidget *parent, QtSLiMWindow *controller);
    virtual ~QtSLiMGraphView_AgeDistribution() override;
    
    virtual QString graphTitle(void) override;
    virtual QString aboutString(void) override;
    virtual void drawGraph(QPainter &painter, QRect interiorRect) override;
    virtual QtSLiMLegendSpec legendKey(void) override;
    virtual bool providesStringForData(void) override;
    virtual void appendStringForData(QString &string) override;    
    virtual QString disableMessage(void) override;
    
public slots:
    virtual void addedToWindow(void) override;
    virtual void controllerRecycled(void) override;
    virtual void updateAfterTick(void) override;
    void subpopulation1PopupChanged(int index);
    
private:
    // pop-up menu buttons
    QComboBox *subpopulation1Button_ = nullptr;
    
    // The subpop selected; -1 indicates no current selection (which will be fixed as soon as the menu is populated)
    slim_objectid_t selectedSubpopulation1ID_;
    
    double *ageDistribution(int *binCount, bool tallySexesSeparately);
};


#endif // QTSLIMGRAPHVIEW_AGEDISTRIBUTION_H





































