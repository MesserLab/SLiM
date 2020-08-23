//
//  QtSLiMGraphView_2DFrequencySpectrum.h
//  SLiM
//
//  Created by Ben Haller on 8/18/2020.
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

#ifndef QTSLIMGRAPHVIEW_2DFREQUENCYSPECTRUM_H
#define QTSLIMGRAPHVIEW_2DFREQUENCYSPECTRUM_H

#include "QtSLiMGraphView.h"

class MutationType;


class QtSLiMGraphView_2DFrequencySpectrum : public QtSLiMGraphView
{
    Q_OBJECT
    
public:
    QtSLiMGraphView_2DFrequencySpectrum(QWidget *parent, QtSLiMWindow *controller);
    ~QtSLiMGraphView_2DFrequencySpectrum() override;
    
    QString graphTitle(void) override;
    bool needsButtonLayout(void) override;
    void drawGraph(QPainter &painter, QRect interiorRect) override;
    bool providesStringForData(void) override;
    void appendStringForData(QString &string) override;    
    
public slots:
    void addedToWindow(void) override;
    void controllerRecycled(void) override;
    void updateAfterTick(void) override;
    void subpopulation1PopupChanged(int index);
    void subpopulation2PopupChanged(int index);
    void mutationTypePopupChanged(int index);
    
private:
    // pop-up menu buttons
    QComboBox *subpopulation1Button_ = nullptr;
    QComboBox *subpopulation2Button_ = nullptr;
	QComboBox *mutationTypeButton_ = nullptr;
    
    // The subpop and mutation type selected; -1 indicates no current selection (which will be fixed as soon as the menu is populated)
    slim_objectid_t selectedSubpopulation1ID_;
    slim_objectid_t selectedSubpopulation2ID_;
    int selectedMutationTypeIndex_;
    
    double *mutation2DSFS(void);    
};


#endif // QTSLIMGRAPHVIEW_2DFREQUENCYSPECTRUM_H




















