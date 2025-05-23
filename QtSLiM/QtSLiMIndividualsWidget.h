//
//  QtSLiMIndividualsWidget.h
//  SLiM
//
//  Created by Ben Haller on 7/30/2019.
//  Copyright (c) 2019-2025 Benjamin C. Haller.  All rights reserved.
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

#ifndef SLIM_NO_OPENGL
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#endif

#include "slim_globals.h"
#include "subpopulation.h"
#include <map>

class QContextMenuEvent;


typedef struct {
	int backgroundType;				// 0 == black, 1 == gray, 2 == white, 3 == named spatial map; if no preference has been set, no entry will exist
	std::string spatialMapName;		// the name of the spatial map chosen, for backgroundType == 3
	bool showGridPoints;            // if true show spatial grid points, for backgroundType == 3
} PopulationViewSettings;

typedef enum {
    kDisplayIndividuals = 0,
    kDisplaySpatialSeparate,
    kDisplaySpatialUnified
} PopulationViewDisplayMode;

#ifndef SLIM_NO_OPENGL
class QtSLiMIndividualsWidget : public QOpenGLWidget, protected QOpenGLFunctions
#else
class QtSLiMIndividualsWidget : public QWidget
#endif
{
    Q_OBJECT
    
    // display mode: this is now just a suggestion; each subpop will fall back to what it is able to do
	PopulationViewDisplayMode preferredDisplayMode = kDisplaySpatialSeparate;
	
	// per-subview display preferences, kept indexed by subpopulation id
	std::map<slim_objectid_t, PopulationViewSettings> subviewSettings;
	
	// subview tiling, kept indexed by subpopulation id
	std::map<slim_objectid_t, QRect> subpopTiles;
    bool canDisplayAllIndividuals = false;
    
    // action button tracking support
    slim_objectid_t actionButtonHighlightSubpopID_ = -1;
    
public:
    explicit QtSLiMIndividualsWidget(QWidget *p_parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    virtual ~QtSLiMIndividualsWidget() override;
    
    void tileSubpopulations(std::vector<Subpopulation*> &selectedSubpopulations);
    void runContextMenuAtPoint(QPoint globalPoint, Subpopulation *subpopForEvent);
    
protected:
#ifndef SLIM_NO_OPENGL
    virtual void initializeGL() override;
    virtual void resizeGL(int w, int h) override;
    virtual void paintGL() override;
#else
    virtual void paintEvent(QPaintEvent *event) override;
#endif

    bool canDisplayUnified(std::vector<Subpopulation*> &selectedSubpopulations);
    PopulationViewDisplayMode displayModeForSubpopulation(Subpopulation *subpopulation);
    bool canDisplayIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds);
    QRect spatialDisplayBoundsForSubpopulation(Subpopulation *subpop, QRect tileBounds);
    
    int squareSizeForSubpopulationInArea(Subpopulation *subpop, QRect bounds);
    void cacheDisplayBufferForMapForSubpopulation(SpatialMap *background_map, Subpopulation *subpop, bool flipped);
    void chooseDefaultBackgroundSettingsForSubpopulation(PopulationViewSettings *settings, SpatialMap **returnMap, Subpopulation *subpop);
    
    // OpenGL drawing; this is the primary drawing code
#ifndef SLIM_NO_OPENGL
    void glDrawViewFrameInBounds(QRect bounds);
    void glDrawIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds, int squareSize);
    void _glDrawBackgroundSpatialMap(SpatialMap *background_map, QRect bounds, Subpopulation *subpop, bool showGridPoints);
    void glDrawSpatialBackgroundInBoundsForSubpopulation(QRect bounds, Subpopulation * subpop, int dimensionality);
    void glDrawSpatialIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds, int dimensionality, float *forceColor);
#endif
    
    // Qt-based drawing, provided as a backup if OpenGL has problems on a given platform
    void qtDrawViewFrameInBounds(QRect bounds, QPainter &painter);
    void qtDrawIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds, int squareSize, QPainter &painter);
    void _qtDrawBackgroundSpatialMap(SpatialMap *background_map, QRect bounds, Subpopulation *subpop, bool showGridPoints, QPainter &painter);
    void qtDrawSpatialBackgroundInBoundsForSubpopulation(QRect bounds, Subpopulation * subpop, int dimensionality, QPainter &painter);
    void qtDrawSpatialIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds, int dimensionality, float *forceColor, QPainter &painter);
    
    virtual void contextMenuEvent(QContextMenuEvent *p_event) override;
    virtual void mousePressEvent(QMouseEvent *p_event) override;
};

#endif // QTSLIMINDIVIDUALSWIDGET_H




























