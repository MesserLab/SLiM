//
//  QtSLiMIndividualsWidget.h
//  SLiM
//
//  Created by Ben Haller on 7/30/2019.
//  Copyright (c) 2019 Philipp Messer.  All rights reserved.
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

#ifndef QTSLIMINDIVIDUALSWIDGET_H
#define QTSLIMINDIVIDUALSWIDGET_H

#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

#include "slim_globals.h"
#include "subpopulation.h"
#include <map>


typedef struct {
	int backgroundType;				// 0 == black, 1 == gray, 2 == white, 3 == named spatial map; if no preference has been set, no entry will exist
	std::string spatialMapName;		// the name of the spatial map chosen, for backgroundType == 3
} PopulationViewBackgroundSettings;


class QtSLiMIndividualsWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
    
    // display mode: 0 == individuals (non-spatial), 1 == individuals (spatial), 2 == fitness distribution line plots, 3 == fitness distribution barplot
	int displayMode = 0;
	
	// used in displayMode 2 and 3, set by PopulationViewOptionsSheet.xib
//	int binCount;
//	double fitnessMin;
//	double fitnessMax;
	
	// display background preferences, kept indexed by subpopulation id
//	std::map<slim_objectid_t, PopulationViewBackgroundSettings> backgroundSettings;
//	slim_objectid_t lastContextMenuSubpopID;
	
	// subview tiling, kept indexed by subpopulation id
	std::map<slim_objectid_t, QRect> subpopTiles;
    bool canDisplayAllIndividuals = false;
    
    // OpenGL buffers
	float *glArrayVertices = nullptr;
	float *glArrayColors = nullptr;
    
public:
    explicit QtSLiMIndividualsWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    virtual ~QtSLiMIndividualsWidget() override;
    
    void tileSubpopulations(std::vector<Subpopulation*> &selectedSubpopulations);
    
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    bool canDisplayIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds);
    
    void drawViewFrameInBounds(QRect bounds);
    void drawIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds);
};

#endif // QTSLIMINDIVIDUALSWIDGET_H




























