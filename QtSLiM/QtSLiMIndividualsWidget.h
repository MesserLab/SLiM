//
//  QtSLiMIndividualsWidget.h
//  SLiM
//
//  Created by Ben Haller on 7/30/2019.
//  Copyright (c) 2019-2020 Philipp Messer.  All rights reserved.
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

// Silence deprecated OpenGL warnings on macOS
#define GL_SILENCE_DEPRECATION

#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

#include "slim_globals.h"
#include "subpopulation.h"
#include <map>

class QContextMenuEvent;


typedef struct {
	int backgroundType;				// 0 == black, 1 == gray, 2 == white, 3 == named spatial map; if no preference has been set, no entry will exist
	std::string spatialMapName;		// the name of the spatial map chosen, for backgroundType == 3
} PopulationViewBackgroundSettings;


class QtSLiMIndividualsWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
    
    // display mode: 0 == individuals (non-spatial), 1 == individuals (spatial)
	int displayMode = 0;
	
	// display background preferences, kept indexed by subpopulation id
	std::map<slim_objectid_t, PopulationViewBackgroundSettings> backgroundSettings;
	
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
    virtual void initializeGL() override;
    virtual void resizeGL(int w, int h) override;
    virtual void paintGL() override;

    bool canDisplayIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds);
    QRect spatialDisplayBoundsForSubpopulation(Subpopulation *subpop, QRect tileBounds);
    
    void drawViewFrameInBounds(QRect bounds);
    void drawIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds);
    void cacheDisplayBufferForMapForSubpopulation(SpatialMap *background_map, Subpopulation *subpop);
    void _drawBackgroundSpatialMap(SpatialMap *background_map, QRect bounds, Subpopulation *subpop);
    void chooseDefaultBackgroundSettingsForSubpopulation(PopulationViewBackgroundSettings *background, SpatialMap **returnMap, Subpopulation *subpop);
    void drawSpatialBackgroundInBoundsForSubpopulation(QRect bounds, Subpopulation * subpop, int dimensionality);
    void drawSpatialIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds, int dimensionality);
    
    virtual void contextMenuEvent(QContextMenuEvent *event) override;
};

#endif // QTSLIMINDIVIDUALSWIDGET_H




























