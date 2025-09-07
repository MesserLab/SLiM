//
//  QtSLiMIndividualsWidget.cpp
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


#include "QtSLiMIndividualsWidget.h"
#include "QtSLiMWindow.h"
#include "QtSLiMPreferences.h"

#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QContextMenuEvent>
#include <QtGlobal>
#include <QDebug>

#include <map>
#include <utility>
#include <string>
#include <vector>


QtSLiMIndividualsWidget::QtSLiMIndividualsWidget(QWidget *p_parent, Qt::WindowFlags f)
#ifndef SLIM_NO_OPENGL
    : QOpenGLWidget(p_parent, f)
#else
    : QWidget(p_parent, f)
#endif
{
    preferredDisplayMode = PopulationViewDisplayMode::kDisplaySpatialSeparate;	// prefer spatial display when possible, fall back to individuals
    
    // We support both OpenGL and non-OpenGL display, because some platforms seem
    // to have problems with OpenGL (https://github.com/MesserLab/SLiM/issues/462)
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::useOpenGLPrefChanged, this, [this]() { update(); });
}

QtSLiMIndividualsWidget::~QtSLiMIndividualsWidget()
{
}

#ifndef SLIM_NO_OPENGL
void QtSLiMIndividualsWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
}

void QtSLiMIndividualsWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    
	// Update the projection
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
}
#endif

#ifndef SLIM_NO_OPENGL
void QtSLiMIndividualsWidget::paintGL()
#else
void QtSLiMIndividualsWidget::paintEvent(QPaintEvent * /* p_paint_event */)
#endif
{
    QPainter painter(this);
    bool inDarkMode = QtSLiMInDarkMode();
    
    //painter.eraseRect(rect());      // erase to background color, which is not guaranteed
    painter.fillRect(rect(), Qt::red);
    
    //
	//	NOTE this code is parallel to code in tileSubpopulations() and both should be maintained!
	//
	
	QRect bounds = rect();
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
	std::vector<Subpopulation*> selectedSubpopulations = controller->selectedSubpopulations();
	int selectedSubpopCount = static_cast<int>(selectedSubpopulations.size());
    bool displayingUnified = ((preferredDisplayMode == PopulationViewDisplayMode::kDisplaySpatialUnified) && canDisplayUnified(selectedSubpopulations));
	
    // In SLiMgui this has to be called in advance, to swap in/out the error view, but here we
    // can simply call it before each update to pre-plan, making for a simpler design
    tileSubpopulations(selectedSubpopulations);
    
	if ((selectedSubpopCount == 0) || !canDisplayAllIndividuals)
	{
		// clear to a shade of gray
        if (inDarkMode)
            painter.fillRect(QRect(0, 0, bounds.width(), bounds.height()), QtSLiMColorWithWhite(0.118, 1.0));
        else
            painter.fillRect(QRect(0, 0, bounds.width(), bounds.height()), QtSLiMColorWithWhite(0.9, 1.0));
		
        // display a message if we have too many subpops to show
        if (!canDisplayAllIndividuals)
        {
            painter.setPen(Qt::darkGray);
            painter.drawText(bounds, Qt::AlignCenter, "too many subpops\nor individuals\nto display – try\nresizing to make\nmore space");
        }
        
		// Frame our view
		qtDrawViewFrameInBounds(bounds, painter);
	}
	else
	{
        // Clear to background gray; we always do this now, because the tile title bars are on the window background
		painter.fillRect(rect(), palette().color(QPalette::Window));
		
        // Show title bars above each subpop tile
        static QFont *titleFont = nullptr;
        static QIcon actionIcon_LIGHT, actionIcon_DARK;
        
        if (!titleFont)
        {
            titleFont = new QFont();
            
#ifdef __linux__
            // font sizes are calibrated for macOS; on Linux they need to be a little smaller
            titleFont->setPointSize(titleFont->pointSize() * 0.75);
#endif
            
            actionIcon_LIGHT.addFile(":/buttons/action.png", QSize(), QIcon::Normal, QIcon::Off);
            actionIcon_LIGHT.addFile(":/buttons/action_H.png", QSize(), QIcon::Normal, QIcon::On);
            actionIcon_DARK.addFile(":/buttons_DARK/action_DARK.png", QSize(), QIcon::Normal, QIcon::Off);
            actionIcon_DARK.addFile(":/buttons_DARK/action_H_DARK.png", QSize(), QIcon::Normal, QIcon::On);
        }
        
        QIcon *actionIcon = (inDarkMode ? &actionIcon_DARK : &actionIcon_LIGHT);
        
        painter.save();
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setPen(inDarkMode ? Qt::white : Qt::black);
        painter.setFont(*titleFont);
        
        for (Subpopulation *subpop : selectedSubpopulations)
		{
			auto tileIter = subpopTiles.find(subpop->subpopulation_id_);
			
			if (tileIter != subpopTiles.end())
			{
				QRect tileBounds = tileIter->second;
				QRect buttonBounds(tileBounds.left(), tileBounds.top(), 20, 20);
                
                if (subpop->subpopulation_id_ == actionButtonHighlightSubpopID_)
                    actionIcon->paint(&painter, buttonBounds, Qt::AlignCenter, QIcon::Normal, QIcon::On);
                else
                    actionIcon->paint(&painter, buttonBounds, Qt::AlignCenter, QIcon::Normal, QIcon::Off);
                
                int titleX = tileBounds.left() + 23;
                int titleY = tileBounds.top() + 17;
                int textFlags = (Qt::TextDontClip | Qt::TextSingleLine | Qt::AlignBottom | Qt::AlignLeft);
                QString title;
                
                if (displayingUnified)
                {
                    title = "Unified (all subpopulations)";
                }
                else
                {
                    title = QString("p%1").arg(subpop->subpopulation_id_);
                    
                    if (controller->community->all_species_.size() > 1)
                        title.append(" ").append(QString::fromStdString(subpop->species_.avatar_));
                }
                
                painter.drawText(QRect(titleX, titleY, 0, 0), textFlags, title);
            }
            
            if (displayingUnified)
                break;
        }
        
        painter.restore();
        
        // find a consensus square size for non-spatial display, for consistency
        int squareSize = 20;
        
        for (Subpopulation *subpop : selectedSubpopulations)
        {
            PopulationViewDisplayMode displayMode = (displayingUnified ? PopulationViewDisplayMode::kDisplaySpatialUnified : displayModeForSubpopulation(subpop));
            
            if (displayMode == PopulationViewDisplayMode::kDisplayIndividuals)
            {
                auto tileIter = subpopTiles.find(subpop->subpopulation_id_);
                
                if (tileIter != subpopTiles.end())
                {
                    QRect tileBounds = tileIter->second;
                    
                    // remove a margin at the top for the title bar
                    tileBounds.setTop(tileBounds.top() + 22);
                    
                    int thisSquareSize = squareSizeForSubpopulationInArea(subpop, tileBounds);
                    
                    if ((thisSquareSize < squareSize) && (thisSquareSize >= 1))
                        squareSize = thisSquareSize;
                }
            }
        }
        
        // And now draw the tiles themselves
        bool useGL = QtSLiMPreferencesNotifier::instance().useOpenGLPref();
        
        if (useGL)
            painter.beginNativePainting();
        
        bool clearBackground = true;    // used for display mode 2 to prevent repeated clearing
        
		for (Subpopulation *subpop : selectedSubpopulations)
		{
            Species *displaySpecies = &subpop->species_;
            PopulationViewDisplayMode displayMode = (displayingUnified ? PopulationViewDisplayMode::kDisplaySpatialUnified : displayModeForSubpopulation(subpop));
            
			auto tileIter = subpopTiles.find(subpop->subpopulation_id_);
			
			if (tileIter != subpopTiles.end())
			{
				QRect tileBounds = tileIter->second;
                
                // remove a margin at the top for the title bar
                tileBounds.setTop(tileBounds.top() + 22);
				
                if ((displayMode == PopulationViewDisplayMode::kDisplaySpatialSeparate) || (displayMode == PopulationViewDisplayMode::kDisplaySpatialUnified))
				{
                    QRect spatialDisplayBounds = spatialDisplayBoundsForSubpopulation(subpop, tileBounds);
                    QRect frameBounds = spatialDisplayBounds.adjusted(-1, -1, 1, 1);
                    
                    if (clearBackground)
                    {
                        if (frameBounds != tileBounds)
                        {
                            // If we have inset the tileBounds because of aspect ratio considerations
                            // in spatialDisplayBoundsForSubpopulation() (which only happens in 2D),
                            // clear to a shade of gray and frame the overall tileBounds
#ifndef SLIM_NO_OPENGL
                            if (useGL)
                            {
                                glColor3f(0.9f, 0.9f, 0.9f);
                                glRecti(tileBounds.left(), tileBounds.top(), (tileBounds.left() + tileBounds.width()), (tileBounds.top() + tileBounds.height()));
                                glDrawViewFrameInBounds(tileBounds);
                            }
                            else
#endif
                            {
                                painter.fillRect(tileBounds, QtSLiMColorWithWhite(0.9, 1.0));
                                qtDrawViewFrameInBounds(tileBounds, painter);
                            }
                        }
                        
#ifndef SLIM_NO_OPENGL
                        if (useGL)
                            glDrawSpatialBackgroundInBoundsForSubpopulation(spatialDisplayBounds, subpop, displaySpecies->spatial_dimensionality_);
                        else
#endif
                            qtDrawSpatialBackgroundInBoundsForSubpopulation(spatialDisplayBounds, subpop, displaySpecies->spatial_dimensionality_, painter);
                    }
                    
                    float forceRGB[4];
                    float *forceColor = nullptr;
                    
                    if ((displayMode == PopulationViewDisplayMode::kDisplaySpatialUnified) && (controller->focalSpeciesName.compare("all") == 0))
                    {
                        controller->colorForSpecies(displaySpecies, forceRGB + 0, forceRGB + 1, forceRGB + 2,forceRGB + 3);
                        forceColor = forceRGB;
                    }
                    
#ifndef SLIM_NO_OPENGL
                    if (useGL)
                    {
                        glDrawSpatialIndividualsFromSubpopulationInArea(subpop, spatialDisplayBounds, displaySpecies->spatial_dimensionality_, forceColor);
                        glDrawViewFrameInBounds(frameBounds); // framed more than once in displayMode 2, which is OK
                    }
                    else
#endif
                    {
                        qtDrawSpatialIndividualsFromSubpopulationInArea(subpop, spatialDisplayBounds, displaySpecies->spatial_dimensionality_, forceColor, painter);
                        qtDrawViewFrameInBounds(frameBounds, painter); // framed more than once in displayMode 2, which is OK
                    }
                    
                    if (displayMode == 2)
                        clearBackground = false;
				}
				else	// displayMode == PopulationViewDisplayMode::kDisplayIndividuals
				{
                    auto backgroundIter = subviewSettings.find(subpop->subpopulation_id_);
                    PopulationViewSettings background;
                    
                    chooseDefaultBackgroundSettingsForSubpopulation(&background, nullptr, subpop);
                    
                    if ((backgroundIter != subviewSettings.end()) && (backgroundIter->second.backgroundType <= 2))
                        background = backgroundIter->second;
                    
                    int backgroundColor = background.backgroundType;
                    float bgGray = 0.0;
                    
                    if (backgroundColor == 0)
                        bgGray = 0.0;
                    else if (backgroundColor == 1)
                        bgGray = 0.3f;
                    else if (backgroundColor == 2)
                        bgGray = 1.0;
                        
#ifndef SLIM_NO_OPENGL
                    if (useGL)
                    {
                        glColor3f(bgGray, bgGray, bgGray);
                        glRecti(tileBounds.left(), tileBounds.top(), (tileBounds.left() + tileBounds.width()), (tileBounds.top() + tileBounds.height()));
                        
                        glDrawViewFrameInBounds(tileBounds);
                        glDrawIndividualsFromSubpopulationInArea(subpop, tileBounds, squareSize);
                    }
                    else
#endif
                    {
                        painter.fillRect(tileBounds, QtSLiMColorWithWhite(bgGray, 1.0));
                        
                        qtDrawViewFrameInBounds(tileBounds, painter);
                        qtDrawIndividualsFromSubpopulationInArea(subpop, tileBounds, squareSize, painter);
                    }
				}
			}
		}
        
        if (useGL)
            painter.endNativePainting();
	}
}

bool QtSLiMIndividualsWidget::canDisplayUnified(std::vector<Subpopulation*> &selectedSubpopulations)
{
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
    Community *community = controller->community;
    int selectedSubpopCount = static_cast<int>(selectedSubpopulations.size());
    
    if (community->simulation_valid_ && (community->Tick() >= 1))
	{
        if (selectedSubpopCount <= 1)
            return false;                // unified display requires more than one subpop
        
        // we need all the subpops to have the same spatial bounds and dimensionality, so their coordinate systems match up
        // we presently allow periodicity to not match; not sure about that one way or the other
        double x0 = 0, x1 = 0, y0 = 0, y1 = 0, z0 = 0, z1 = 0;  // suppress unused warnings
        int dimensionality = 0;
        bool first = true;
        
        for (Subpopulation *subpop : selectedSubpopulations)
        {
            Species *subpop_species = &subpop->species_;
            
            if (subpop_species->spatial_dimensionality_ == 0)
                return false;
            
            if (first)
            {
                x0 = subpop->bounds_x0_;    x1 = subpop->bounds_x1_;
                y0 = subpop->bounds_y0_;    y1 = subpop->bounds_y1_;
                z0 = subpop->bounds_z0_;    z1 = subpop->bounds_z1_;
                dimensionality = subpop_species->spatial_dimensionality_;
                first = false;
            }
            else
            {
                if ((x0 != subpop->bounds_x0_) || (x1 != subpop->bounds_x1_) ||
                        (y0 != subpop->bounds_y0_) || (y1 != subpop->bounds_y1_) ||
                        (z0 != subpop->bounds_z0_) || (z1 != subpop->bounds_z1_) ||
                        (dimensionality != subpop_species->spatial_dimensionality_))
                    return false;
            }
        }
        
        return true;
    }
    
    return true;    // allow unified to be chosen as long as we have no information to the contrary
}

PopulationViewDisplayMode QtSLiMIndividualsWidget::displayModeForSubpopulation(Subpopulation *subpopulation)
{
    // the decision regarding unified display is made external to this method, so we don't worry about it here
    // We just need to choose between individual versus spatial display
    if (preferredDisplayMode == PopulationViewDisplayMode::kDisplayIndividuals)
        return PopulationViewDisplayMode::kDisplayIndividuals;
    
    if (subpopulation->species_.spatial_dimensionality_ == 0)
        return PopulationViewDisplayMode::kDisplayIndividuals;
    
    return PopulationViewDisplayMode::kDisplaySpatialSeparate;
}

void QtSLiMIndividualsWidget::tileSubpopulations(std::vector<Subpopulation*> &selectedSubpopulations)
{
	//
	//	NOTE this code is parallel to code in paintGL() and both should be maintained!
	//
	
	// We will decide upon new tiles for our subpopulations here, so start out empty
	subpopTiles.clear();
	
    QRect bounds = rect();
    int selectedSubpopCount = static_cast<int>(selectedSubpopulations.size());
    bool displayingUnified = ((preferredDisplayMode == PopulationViewDisplayMode::kDisplaySpatialUnified) && canDisplayUnified(selectedSubpopulations));
    
	if (selectedSubpopCount == 0)
	{
		canDisplayAllIndividuals = true;
	}
    else if (displayingUnified)
	{
        // set all tiles to be the full bounds for overlay mode
        for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
        {
            Subpopulation *subpop = selectedSubpopulations[static_cast<size_t>(subpopIndex)];
            
            subpopTiles.emplace(subpop->subpopulation_id_, bounds);
        }
        canDisplayAllIndividuals = true;
	}
    else if (selectedSubpopCount == 1)
	{
        Subpopulation *selectedSubpop = selectedSubpopulations[0];
        PopulationViewDisplayMode displayMode = displayModeForSubpopulation(selectedSubpop);
        
        subpopTiles.emplace(selectedSubpop->subpopulation_id_, bounds);
        
        if (displayMode == PopulationViewDisplayMode::kDisplaySpatialSeparate)
        {
            canDisplayAllIndividuals = true;
        }
        else
        {
            bounds.setTop(bounds.top() + 22);     // take out title bar space
            
            canDisplayAllIndividuals = canDisplayIndividualsFromSubpopulationInArea(selectedSubpop, bounds);
        }
    }
    else // not unified, more than one subpop
	{
		// adaptively finds the layout that maximizes the pixel area covered; fails if no layout is satisfactory
        QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
        int minBoxWidth = ((controller->community->all_species_.size() > 1) ? 70 : 50);     // room for avatars
		int64_t bestTotalExtent = 0;
		
        canDisplayAllIndividuals = false;
        
		for (int rowCount = 1; rowCount <= selectedSubpopCount; ++rowCount)
		{
			int columnCount = static_cast<int>(ceil(selectedSubpopCount / static_cast<double>(rowCount)));
			int interBoxSpace = 5;
			int totalInterboxHeight = interBoxSpace * (rowCount - 1);
			int totalInterboxWidth = interBoxSpace * (columnCount - 1);
			double boxWidth = (bounds.width() - totalInterboxWidth) / static_cast<double>(columnCount);
			double boxHeight = (bounds.height() - totalInterboxHeight) / static_cast<double>(rowCount);
			std::map<slim_objectid_t, QRect> candidateTiles;
			int64_t totalExtent = 0;
            
            // Round the box width down, for consistency, and calculate an offset to center the tiles
            // So the visual width of the individuals view is quantized in such a way as to evenly subdivide
            // We don't do this with the height since the effect of height variation is less visible, and
            // having the visual height of the view not match the neighboring views would look weird
            boxWidth = floor(boxWidth);
            
            int leftOffset = floor(bounds.width() - (boxWidth * columnCount + totalInterboxWidth)) / 2;
			
			for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
			{
				int columnIndex = subpopIndex % columnCount;
				int rowIndex = subpopIndex / columnCount;
				int boxLeft = qRound(bounds.x() + leftOffset + columnIndex * (interBoxSpace + boxWidth));
				int boxRight = qRound(bounds.x() + leftOffset + columnIndex * (interBoxSpace + boxWidth) + boxWidth);
				int boxTop = qRound(bounds.y() + rowIndex * (interBoxSpace + boxHeight));
				int boxBottom = qRound(bounds.y() + rowIndex * (interBoxSpace + boxHeight) + boxHeight);
				QRect boxBounds(boxLeft, boxTop, boxRight - boxLeft, boxBottom - boxTop);
				Subpopulation *subpop = selectedSubpopulations[static_cast<size_t>(subpopIndex)];
                PopulationViewDisplayMode displayMode = displayModeForSubpopulation(subpop);
                
                // Too narrow or short a box size (figuring in 22 pixels for the title bar) is not allowed
                if ((boxWidth < minBoxWidth) || (boxHeight < ((displayMode == PopulationViewDisplayMode::kDisplaySpatialSeparate) ? 72 : 42)))
                    goto layoutRejected;
				
                //qDebug() << "boxWidth ==" << boxWidth << "for rowCount ==" << rowCount;
                
				candidateTiles.emplace(subpop->subpopulation_id_, boxBounds);
				
				// find out what pixel area actually gets used by this box, and use that to choose the optimal layout
                boxBounds.setTop(boxBounds.top() + 22);     // take out title bar space
                
                if (displayMode == PopulationViewDisplayMode::kDisplaySpatialSeparate)
                {
                    // for spatial display, squeeze to the spatial aspect ratio
                    boxBounds = spatialDisplayBoundsForSubpopulation(subpop, boxBounds);
                }
                else
                {
                    // for non-spatial display, check that the individuals will fit in the allotted area
                    if (!canDisplayIndividualsFromSubpopulationInArea(subpop, boxBounds))
                    {
                        totalExtent = 0;
                        break;
                    }
                }
                
				int64_t extent = static_cast<int64_t>(boxBounds.width()) * static_cast<int64_t>(boxBounds.height());
				
				totalExtent += extent;
			}
			
			if (totalExtent > bestTotalExtent)
			{
				bestTotalExtent = totalExtent;
				std::swap(subpopTiles, candidateTiles);
                canDisplayAllIndividuals = true;
			}
layoutRejected:
            ;
		}
	}
}

bool QtSLiMIndividualsWidget::canDisplayIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds)
{
    //
	//	NOTE this code is parallel to the code in drawIndividualsFromSubpopulation:inArea: and should be maintained in parallel
	//
	
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	int squareSize, viewColumns = 0, viewRows = 0;
	
	// first figure out the biggest square size that will allow us to display the whole subpopulation
	for (squareSize = 20; squareSize > 1; --squareSize)
	{
		viewColumns = static_cast<int>(floor((bounds.width() - 3) / squareSize));
		viewRows = static_cast<int>(floor((bounds.height() - 3) / squareSize));
		
		if (viewColumns * viewRows > subpopSize)
		{
			// If we have an empty row at the bottom, then break for sure; this allows us to look nice and symmetrical
			if ((subpopSize - 1) / viewColumns < viewRows - 1)
				break;
			
			// Otherwise, break only if we are getting uncomfortably small; otherwise, let's drop down one square size to allow symmetry
			if (squareSize <= 5)
				break;
		}
	}
	
	return (squareSize > 1);
}

QRect QtSLiMIndividualsWidget::spatialDisplayBoundsForSubpopulation(Subpopulation *subpop, QRect tileBounds)
{
	// Determine a subframe for drawing spatial information inside.  The subframe we use is the maximal subframe
	// with integer boundaries that preserves, as closely as possible, the aspect ratio of the subpop's bounds.
    // If sim->spatial_dimensionality_ is 1, there are no aspect ratio considerations so we just inset.
	QRect spatialDisplayBounds = tileBounds.adjusted(1, 1, -1, -1);
    
    if (subpop->species_.spatial_dimensionality_ > 1)
    {
        double displayAspect = spatialDisplayBounds.width() / static_cast<double>(spatialDisplayBounds.height());
        double bounds_x0 = subpop->bounds_x0_, bounds_x1 = subpop->bounds_x1_;
        double bounds_y0 = subpop->bounds_y0_, bounds_y1 = subpop->bounds_y1_;
        double bounds_x_size = bounds_x1 - bounds_x0, bounds_y_size = bounds_y1 - bounds_y0;
        double subpopAspect = bounds_x_size / bounds_y_size;
        
        if (subpopAspect > displayAspect)
        {
            // The display bounds will need to shrink vertically to match the subpop
            int idealSize = qRound(spatialDisplayBounds.width() / subpopAspect);
            int roundedOffset = qRound((spatialDisplayBounds.height() - idealSize) / 2.0);
            
            spatialDisplayBounds.setY(spatialDisplayBounds.y() + roundedOffset);
            spatialDisplayBounds.setHeight(idealSize);
        }
        else if (subpopAspect < displayAspect)
        {
            // The display bounds will need to shrink horizontally to match the subpop
            int idealSize = qRound(spatialDisplayBounds.height() * subpopAspect);
            int roundedOffset = qRound((spatialDisplayBounds.width() - idealSize) / 2.0);
            
            spatialDisplayBounds.setX(spatialDisplayBounds.x() + roundedOffset);
            spatialDisplayBounds.setWidth(idealSize);
        }
    }
    
	return spatialDisplayBounds;
}

int QtSLiMIndividualsWidget::squareSizeForSubpopulationInArea(Subpopulation *subpop, QRect bounds)
{
    slim_popsize_t subpopSize = subpop->parent_subpop_size_;
    int squareSize, viewColumns = 0, viewRows = 0;
    
    // first figure out the biggest square size that will allow us to display the whole subpopulation
    for (squareSize = 20; squareSize > 1; --squareSize)
    {
        viewColumns = static_cast<int>(floor((bounds.width() - 3) / squareSize));
        viewRows = static_cast<int>(floor((bounds.height() - 3) / squareSize));
        
        if (viewColumns * viewRows > subpopSize)
        {
            // If we have an empty row at the bottom, then break for sure; this allows us to look nice and symmetrical
            if ((subpopSize - 1) / viewColumns < viewRows - 1)
                break;
            
            // Otherwise, break only if we are getting uncomfortably small; otherwise, let's drop down one square size to allow symmetry
            if (squareSize <= 5)
                break;
        }
    }
    
    return squareSize;
}

void QtSLiMIndividualsWidget::cacheDisplayBufferForMapForSubpopulation(SpatialMap *background_map, Subpopulation *subpop, bool flipped)
{
	// Cache a display buffer for the given background map.  This method should be called only in the 2D "xy"
	// case; in the 1D case we can't know the maximum width ahead of time, so we just draw rects without caching,
	// and in the 3D "xyz" case we don't know which z-slice to use so we can't display the spatial map.
	// The window might be too narrow to display a full-size map right now, but we want to premake a full-size map.
	// The sizing logic here is taken from drawRect:, assuming that we are not constrained in width.
	
	// By the way, it may occur to the reader to wonder why we keep the buffer as uint8_t values, given that we
	// convert to and from uint_8 for the display code that uses float RGB components.  My rationale is that
	// this drastically cuts the amount of memory that has to be accessed, and that the display code, in particular,
	// is probably memory-bound since most of the work is done in the GPU.  I haven't done any speed tests to
	// confirm that hunch, though.  In any case, it's plenty fast and I don't see significant display artifacts.
	
	QRect full_bounds = rect().adjusted(1, 1, -1, -1);
	int max_height = full_bounds.height();
	double bounds_x0 = subpop->bounds_x0_, bounds_x1 = subpop->bounds_x1_;
	double bounds_y0 = subpop->bounds_y0_, bounds_y1 = subpop->bounds_y1_;
	double bounds_x_size = bounds_x1 - bounds_x0, bounds_y_size = bounds_y1 - bounds_y0;
	double subpopAspect = bounds_x_size / bounds_y_size;
	int max_width = qRound(max_height * subpopAspect);
	
	// If we have a display buffer of the wrong size, free it.  This should only happen when the user changes
	// the Subpopulation's spatialBounds after it has displayed; it should not happen with a window resize.
	// The user has no way to change the map or the colormap except to set a whole new map, which will also
	// result in the old one being freed, so we're already safe in those circumstances.
    if (background_map->display_buffer_ && ((background_map->buffer_width_ != max_width) || (background_map->buffer_height_ != max_height) || (background_map->buffer_flipped_ != flipped)))
	{
		free(background_map->display_buffer_);
		background_map->display_buffer_ = nullptr;
	}
	
	// Now allocate and validate the display buffer if we haven't already done so.
	if (!background_map->display_buffer_)
	{
		uint8_t *display_buf = static_cast<uint8_t *>(malloc(static_cast<size_t>(max_width * max_height * 3) * sizeof(uint8_t)));
		background_map->display_buffer_ = display_buf;
		background_map->buffer_width_ = max_width;
        background_map->buffer_height_ = max_height;
        background_map->buffer_flipped_ = flipped;
        
        background_map->FillRGBBuffer(display_buf, max_width, max_height, flipped, /* no_interpolation */ false);
	}
}

void QtSLiMIndividualsWidget::chooseDefaultBackgroundSettingsForSubpopulation(PopulationViewSettings *background, SpatialMap **returnMap, Subpopulation *subpop)
{
    PopulationViewDisplayMode displayMode = displayModeForSubpopulation(subpop);
    bool inDarkMode = QtSLiMInDarkMode();
    
    if (displayMode == PopulationViewDisplayMode::kDisplayIndividuals)
    {
        // black or white following the dark mode setting, by default
        if (inDarkMode)
            background->backgroundType = 0;
        else
            background->backgroundType = 2;
    }
    else
    {
        // black by default
        background->backgroundType = 0;
        
        // if there are spatial maps defined, we try to choose one, requiring "x" or "y" or "xy", and requiring
        // a color map to be defined, and preferring 2D over 1D, providing the same default behavior as SLiM 2.x
        SpatialMapMap &spatial_maps = subpop->spatial_maps_;
        SpatialMap *background_map = nullptr;
        std::string background_map_name;
        
        for (const auto &map_pair : spatial_maps)
        {
            SpatialMap *map = map_pair.second;
            
            // a map must be "x", "y", or "xy", and must have a defined color map, for us to choose it as a default at all
            if (((map->spatiality_string_ == "x") || (map->spatiality_string_ == "y") || (map->spatiality_string_ == "xy")) && (map->n_colors_ > 0))
            {
                // the map is usable, so now we check whether it's better than the map we previously found, if any
                if ((!background_map) || (map->spatiality_ > background_map->spatiality_))
                {
                    background_map = map;
                    background_map_name = map_pair.first;
                }
            }
        }
        
        if (background_map)
        {
            background->backgroundType = 3;
            background->spatialMapName = background_map_name;
            background->showGridPoints = false;
            *returnMap = background_map;
        }
    }
}

void QtSLiMIndividualsWidget::runContextMenuAtPoint(QPoint globalPoint, Subpopulation *subpopForEvent)
{
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
    Community *community = controller->community;
	bool disableAll = false;
	
	// When the simulation is not valid and initialized, the context menu is disabled
	if (!community || !community->simulation_valid_ || (community->Tick() < 1))
		disableAll = true;
	
    QMenu contextMenu("population_menu", this);
    
    QAction *titleAction1 = contextMenu.addAction("Global preferred display mode:");
    QFont titleFont = titleAction1->font();
    titleFont.setBold(true);
    titleFont.setItalic(true);
    titleAction1->setFont(titleFont);
    titleAction1->setEnabled(false);
    
    QAction *displayNonSpatial = contextMenu.addAction("Display Non-spatial");
    displayNonSpatial->setData(0);
    displayNonSpatial->setCheckable(true);
    displayNonSpatial->setEnabled(!disableAll);
    
    QAction *displaySpatial = contextMenu.addAction("Display Spatial (separate)");
    displaySpatial->setData(1);
    displaySpatial->setCheckable(true);
    displaySpatial->setEnabled(!disableAll);
    
    QAction *displayUnified = contextMenu.addAction("Display Spatial (unified)");
    displayUnified->setData(2);
    displayUnified->setCheckable(true);
    displayUnified->setEnabled(!disableAll);
    
    // Check the item corresponding to our current display preference, if any
    if (!disableAll)
    {
        if (preferredDisplayMode == PopulationViewDisplayMode::kDisplayIndividuals)         displayNonSpatial->setChecked(true);
        if (preferredDisplayMode == PopulationViewDisplayMode::kDisplaySpatialSeparate)     displaySpatial->setChecked(true);
        if (preferredDisplayMode == PopulationViewDisplayMode::kDisplaySpatialUnified)      displayUnified->setChecked(true);
    }
    
    QActionGroup *displayGroup = new QActionGroup(this);    // On Linux this provides a radio-button-group appearance
    displayGroup->addAction(displayNonSpatial);
    displayGroup->addAction(displaySpatial);
    displayGroup->addAction(displayUnified);
    
    // Provide background options (colors, spatial maps for spatial subpops)
    if (subpopForEvent && !disableAll)
    {
        contextMenu.addSeparator();
        
        QAction *titleAction2 = contextMenu.addAction("For this subview:");
        titleAction2->setFont(titleFont);
        titleAction2->setEnabled(false);
        
        QAction *headerAction = contextMenu.addAction(QString("Background for p%1:").arg(subpopForEvent->subpopulation_id_));
        headerAction->setData(-1);
        headerAction->setEnabled(false);
        
        // check the menu item for the preferred display option; if we're in auto mode, don't check anything (could put a dash by the currently chosen style?)
        auto backgroundIter = subviewSettings.find(subpopForEvent->subpopulation_id_);
        PopulationViewSettings *background = ((backgroundIter == subviewSettings.end()) ? nullptr : &backgroundIter->second);
        int backgroundType = (background ? background->backgroundType : -1);
        bool showGrid = (background ? background->showGridPoints : false);
        
        QAction *blackAction = contextMenu.addAction("Black Background");
        blackAction->setData(10);
        blackAction->setCheckable(true);
        blackAction->setChecked(backgroundType == 0);
        blackAction->setEnabled(!disableAll);
        
        QAction *grayAction = contextMenu.addAction("Gray Background");
        grayAction->setData(11);
        grayAction->setCheckable(true);
        grayAction->setChecked(backgroundType == 1);
        grayAction->setEnabled(!disableAll);
        
        QAction *whiteAction = contextMenu.addAction("White Background");
        whiteAction->setData(12);
        whiteAction->setCheckable(true);
        whiteAction->setChecked(backgroundType == 2);
        whiteAction->setEnabled(!disableAll);
        
        QActionGroup *backgroundGroup = new QActionGroup(this);    // On Linux this provides a radio-button-group appearance
        backgroundGroup->addAction(blackAction);
        backgroundGroup->addAction(grayAction);
        backgroundGroup->addAction(whiteAction);
        
        if (preferredDisplayMode > 0)
        {
            // look for spatial maps to offer as choices; need to scan the defined maps for the ones we can use
            SpatialMapMap &spatial_maps = subpopForEvent->spatial_maps_;
            
            for (const auto &map_pair : spatial_maps)
            {
                SpatialMap *map = map_pair.second;
                
                // We used to display only maps with a color scale; now we just make up a color scale if none is given.  Only
                // "x", "y", and "xy" maps are considered displayable; we can't display a z coordinate, and we can't display
                // even the x or y portion of "xz", "yz", and "xyz" maps.
                bool displayable = ((map->spatiality_string_ == "x") || (map->spatiality_string_ == "y") || (map->spatiality_string_ == "xy"));
                QString mapName = QString::fromStdString(map_pair.first);
                QString spatialityName = QString::fromStdString(map->spatiality_string_);
                QString menuItemTitle;
                
                if (map->spatiality_ == 1)
                    menuItemTitle = QString("Spatial Map \"%1\" (\"%2\", %3)").arg(mapName).arg(spatialityName).arg(map->grid_size_[0]);
                else if (map->spatiality_ == 2)
                    menuItemTitle = QString("Spatial Map \"%1\" (\"%2\", %3×%4)").arg(mapName).arg(spatialityName).arg(map->grid_size_[0]).arg(map->grid_size_[1]);
                else // (map->spatiality_ == 3)
                    menuItemTitle = QString("Spatial Map \"%1\" (\"%2\", %3×%4×%5)").arg(mapName).arg(spatialityName).arg(map->grid_size_[0]).arg(map->grid_size_[1]).arg(map->grid_size_[2]);
                
                QAction *mapAction1 = contextMenu.addAction(menuItemTitle);
                mapAction1->setData(mapName);
                mapAction1->setCheckable(true);
                mapAction1->setChecked((backgroundType == 3) && (mapName == QString::fromStdString(background->spatialMapName) && !showGrid));
                mapAction1->setEnabled(!disableAll && displayable);
                
                backgroundGroup->addAction(mapAction1);
                
                // 9/29/2023: now we support displaying spatial maps with a display of the underlying grid, too
                // There is a second menu item for each map, with "with grid" added to the title and "__WITH_GRID" added to the data
                menuItemTitle.append(" with grid");
                QString mapDataName = mapName;
                mapDataName.append("__WITH_GRID");
                QAction *mapAction2 = contextMenu.addAction(menuItemTitle);
                mapAction2->setData(mapDataName);
                mapAction2->setCheckable(true);
                mapAction2->setChecked((backgroundType == 3) && (mapName == QString::fromStdString(background->spatialMapName) && showGrid));
                mapAction2->setEnabled(!disableAll && displayable);
                
                backgroundGroup->addAction(mapAction2);
            }
        }
    }
	
    // Run the context menu synchronously
    QAction *action = contextMenu.exec(globalPoint);
    
    // Act upon the chosen action; we just do it right here instead of dealing with slots
    if (action)
    {
        if ((action == displayNonSpatial) || (action == displaySpatial) || (action == displayUnified))
        {
            // - (IBAction)setDisplayStyle:(id)sender
            PopulationViewDisplayMode newDisplayMode = (PopulationViewDisplayMode)action->data().toInt();
            
            if (newDisplayMode != preferredDisplayMode)
            {
                preferredDisplayMode = newDisplayMode;
                update();
            }
        }
        else if (subpopForEvent)
        {
            // - (IBAction)setDisplayBackground:(id)sender
            int newDisplayBackground;
            bool newShowGrid = false;
            auto backgroundIter = subviewSettings.find(subpopForEvent->subpopulation_id_);
            PopulationViewSettings *background = ((backgroundIter == subviewSettings.end()) ? nullptr : &backgroundIter->second);
            std::string mapName;
            
            // If the user has selected a spatial map, extract its name
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
            if (static_cast<QMetaType::Type>(action->data().type()) == QMetaType::QString)  // for some reason this method's return type is apparently misdeclared; see the doc
#else
            if (action->data().typeId() == QMetaType::QString)
#endif
            {
                QString qMapName = action->data().toString();
                
                // detect the "with grid" ending if present
                if (qMapName.endsWith("__WITH_GRID"))
                {
                    qMapName.chop(11);
                    newShowGrid = true;
                }
                
                mapName = qMapName.toStdString();
                
                if (mapName.length() == 0)
                    return;
                
                newDisplayBackground = 3;
            }
            else
            {
                newDisplayBackground = action->data().toInt() - 10;
                newShowGrid = false;
            }
            
            // Update the existing background entry, or make a new entry
            if (background)
            {
                background->backgroundType = newDisplayBackground;
                background->spatialMapName = mapName;
                background->showGridPoints = newShowGrid;
                update();
            }
            else
            {
                subviewSettings.emplace(subpopForEvent->subpopulation_id_, PopulationViewSettings{newDisplayBackground, mapName, newShowGrid});
                update();
            }
        }
    }
}

void QtSLiMIndividualsWidget::contextMenuEvent(QContextMenuEvent *p_event)
{
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
    Community *community = controller->community;
    bool disableAll = false;
	
	// When the simulation is not valid and initialized, the context menu is disabled
    if (!community || !community->simulation_valid_ || (community->Tick() < 1))
		disableAll = true;
    
    // Find the subpop that was clicked in; in "unified" display mode, this is the first selected subpop
    Subpopulation *subpopForEvent = nullptr;
    
	if (!disableAll)
	{
		std::vector<Subpopulation*> selectedSubpopulations = controller->selectedSubpopulations();
        QPoint viewPoint = p_event->pos();
		
		// our tile coordinates are in the OpenGL coordinate system, which has the origin at top left
		//viewPoint.y = [self bounds].size.height - viewPoint.y;
		
		for (Subpopulation *subpop : selectedSubpopulations)
		{
			slim_objectid_t subpop_id = subpop->subpopulation_id_;
			auto tileIter = subpopTiles.find(subpop_id);
			
			if (tileIter != subpopTiles.end())
			{
				QRect tileRect = tileIter->second;
				
                if (tileRect.contains(viewPoint))
				{
					subpopForEvent = subpop;
					break;
				}
			}
		}
    }
    
    runContextMenuAtPoint(p_event->globalPos(), subpopForEvent);
}

void QtSLiMIndividualsWidget::mousePressEvent(QMouseEvent *p_event)
{
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
    Community *community = controller->community;
	
	// When the simulation is not valid and initialized, the context menu is disabled
    if (!community || !community->simulation_valid_ || (community->Tick() < 1))
		return;
    
    std::vector<Subpopulation*> selectedSubpopulations = controller->selectedSubpopulations();
	int selectedSubpopCount = static_cast<int>(selectedSubpopulations.size());
	
    if ((selectedSubpopCount == 0) || !canDisplayAllIndividuals)
        return;
    
    QPoint mousePos = p_event->pos();
    Subpopulation *subpopForEvent = nullptr;
    
    for (Subpopulation *subpop : selectedSubpopulations)
    {
        auto tileIter = subpopTiles.find(subpop->subpopulation_id_);
        
        if (tileIter != subpopTiles.end())
        {
            QRect tileBounds = tileIter->second;
            QRect buttonBounds(tileBounds.left(), tileBounds.top(), 20, 20);
            double xd = (mousePos.x() - buttonBounds.left()) / (double)buttonBounds.width() - 0.5;
            double yd = (mousePos.y() - buttonBounds.top()) / (double)buttonBounds.height() - 0.5;
            double distance = std::sqrt(xd * xd + yd * yd);
            
            if (buttonBounds.contains(mousePos) && (distance <= 0.51))
            {
                actionButtonHighlightSubpopID_ = subpop->subpopulation_id_;
                update();
                
                subpopForEvent = subpop;
                break;
            }
        }
    }
    
    if (subpopForEvent)
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        runContextMenuAtPoint(p_event->globalPos(), subpopForEvent);
#else
        runContextMenuAtPoint(p_event->globalPosition().toPoint(), subpopForEvent);
#endif
    
    // redraw to get rid of action button highlight
    actionButtonHighlightSubpopID_ = -1;
    update();
}






































