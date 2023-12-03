//
//  QtSLiMIndividualsWidget.cpp
//  SLiM
//
//  Created by Ben Haller on 7/30/2019.
//  Copyright (c) 2019-2023 Philipp Messer.  All rights reserved.
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

#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QtGlobal>
#include <QDebug>

#include <map>
#include <utility>
#include <string>
#include <vector>


// OpenGL constants
static const int kMaxGLRects = 2000;				// 2000 rects
static const int kMaxVertices = kMaxGLRects * 4;	// 4 vertices each


QtSLiMIndividualsWidget::QtSLiMIndividualsWidget(QWidget *p_parent, Qt::WindowFlags f) : QOpenGLWidget(p_parent, f)
{
    preferredDisplayMode = PopulationViewDisplayMode::kDisplaySpatialSeparate;	// prefer spatial display when possible, fall back to individuals
    
    if (!glArrayVertices)
        glArrayVertices = static_cast<float *>(malloc(kMaxVertices * 2 * sizeof(float)));		// 2 floats per vertex, kMaxVertices vertices
    
    if (!glArrayColors)
        glArrayColors = static_cast<float *>(malloc(kMaxVertices * 4 * sizeof(float)));		// 4 floats per color, kMaxVertices colors
}

QtSLiMIndividualsWidget::~QtSLiMIndividualsWidget()
{
    if (glArrayVertices)
	{
		free(glArrayVertices);
		glArrayVertices = nullptr;
	}
	
	if (glArrayColors)
	{
		free(glArrayColors);
		glArrayColors = nullptr;
	}
}

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

void QtSLiMIndividualsWidget::paintGL()
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
        painter.beginNativePainting();
        
        if (inDarkMode)
            glColor3f(0.118f, 0.118f, 0.118f);
        else
            glColor3f(0.9f, 0.9f, 0.9f);
        
		glRecti(0, 0, bounds.width(), bounds.height());
		
        // display a message if we have too many subpops to show
        if (!canDisplayAllIndividuals)
        {
            painter.endNativePainting();
            
            painter.setPen(Qt::darkGray);
            painter.drawText(bounds, Qt::AlignCenter, "too many subpops\nor individuals\nto display – try\nresizing to make\nmore space");
            
            painter.beginNativePainting();
        }
        
		// Frame our view
		drawViewFrameInBounds(bounds);
        
        painter.endNativePainting();
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
                            glColor3f(0.9f, 0.9f, 0.9f);
                            glRecti(tileBounds.left(), tileBounds.top(), (tileBounds.left() + tileBounds.width()), (tileBounds.top() + tileBounds.height()));
                            drawViewFrameInBounds(tileBounds);
                        }
                        
                        drawSpatialBackgroundInBoundsForSubpopulation(spatialDisplayBounds, subpop, displaySpecies->spatial_dimensionality_);
                    }
                    
                    float forceRGB[4];
                    float *forceColor = nullptr;
                    
                    if ((displayMode == PopulationViewDisplayMode::kDisplaySpatialUnified) && (controller->focalSpeciesName.compare("all") == 0))
                    {
                        controller->colorForSpecies(displaySpecies, forceRGB + 0, forceRGB + 1, forceRGB + 2,forceRGB + 3);
                        forceColor = forceRGB;
                    }
                    
					drawSpatialIndividualsFromSubpopulationInArea(subpop, spatialDisplayBounds, displaySpecies->spatial_dimensionality_, forceColor);
					drawViewFrameInBounds(frameBounds); // framed more than once in displayMode 2, which is OK
                    
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
                    
                    if (backgroundColor == 0)
                        glColor3f(0.0, 0.0, 0.0);
                    else if (backgroundColor == 1)
                        glColor3f(0.3f, 0.3f, 0.3f);
                    else if (backgroundColor == 2)
                        glColor3f(1.0, 1.0, 1.0);
                    
					glRecti(tileBounds.left(), tileBounds.top(), (tileBounds.left() + tileBounds.width()), (tileBounds.top() + tileBounds.height()));
					
                    drawViewFrameInBounds(tileBounds);
					drawIndividualsFromSubpopulationInArea(subpop, tileBounds, squareSize);
				}
			}
		}
        
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

void QtSLiMIndividualsWidget::drawViewFrameInBounds(QRect bounds)
{
    int ox = bounds.left(), oy = bounds.top();
    bool inDarkMode = QtSLiMInDarkMode();
    
    if (inDarkMode)
        glColor3f(0.067f, 0.067f, 0.067f);
    else
        glColor3f(0.77f, 0.77f, 0.77f);
    
    glRecti(ox, oy, ox + 1, oy + bounds.height());
    glRecti(ox + 1, oy, ox + bounds.width() - 1, oy + 1);
    glRecti(ox + bounds.width() - 1, oy, ox + bounds.width(), oy + bounds.height());
    glRecti(ox + 1, oy + bounds.height() - 1, ox + bounds.width() - 1, oy + bounds.height());
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

void QtSLiMIndividualsWidget::drawIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds, int squareSize)
{
    //
    //	NOTE this code is parallel to the code in canDisplayIndividualsFromSubpopulation:inArea: and should be maintained in parallel
    //
    
    //QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
    double scalingFactor = 0.8; // used to be controller->fitnessColorScale;
    slim_popsize_t subpopSize = subpop->parent_subpop_size_;
    int viewColumns = 0, viewRows = 0;
    
    // our square size is given from above (a consensus based on squareSizeForSubpopulationInArea(); calculate metrics from it
    viewColumns = static_cast<int>(floor((bounds.width() - 3) / squareSize));
    viewRows = static_cast<int>(floor((bounds.height() - 3) / squareSize));
    
    if (viewColumns * viewRows < subpopSize)
        squareSize = 1;
    
    if (squareSize > 1)
    {
        int squareSpacing = 0;
        
        // Convert square area to space between squares if possible
        if (squareSize > 2)
        {
            --squareSize;
            ++squareSpacing;
        }
        if (squareSize > 5)
        {
            --squareSize;
            ++squareSpacing;
        }
        
        double excessSpaceX = bounds.width() - ((squareSize + squareSpacing) * viewColumns - squareSpacing);
        double excessSpaceY = bounds.height() - ((squareSize + squareSpacing) * viewRows - squareSpacing);
        int offsetX = static_cast<int>(floor(excessSpaceX / 2.0));
        int offsetY = static_cast<int>(floor(excessSpaceY / 2.0));
        
        // If we have an empty row at the bottom, then we can use the same value for offsetY as for offsetX, for symmetry
        if ((subpopSize - 1) / viewColumns < viewRows - 1)
            offsetY = offsetX;
        
        QRect individualArea(bounds.left() + offsetX, bounds.top() + offsetY, bounds.width() - offsetX, bounds.height() - offsetY);
        
        int individualArrayIndex, displayListIndex;
        float *vertices = nullptr, *colors = nullptr;
        
        // Set up to draw rects
        displayListIndex = 0;
        
        vertices = glArrayVertices;
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(2, GL_FLOAT, 0, glArrayVertices);
        
        colors = glArrayColors;
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_FLOAT, 0, glArrayColors);
        
        for (individualArrayIndex = 0; individualArrayIndex < subpopSize; ++individualArrayIndex)
        {
            // Figure out the rect to draw in; note we now use individualArrayIndex here, because the hit-testing code doesn't have an easy way to calculate the displayed individual index...
            float left = static_cast<float>(individualArea.left() + (individualArrayIndex % viewColumns) * (squareSize + squareSpacing));
            float top = static_cast<float>(individualArea.top() + (individualArrayIndex / viewColumns) * (squareSize + squareSpacing));
            float right = left + squareSize;
            float bottom = top + squareSize;
            
            *(vertices++) = left;
            *(vertices++) = top;
            *(vertices++) = left;
            *(vertices++) = bottom;
            *(vertices++) = right;
            *(vertices++) = bottom;
            *(vertices++) = right;
            *(vertices++) = top;
            
            // dark gray default, for a fitness of NaN; should never happen
            float colorRed = 0.3f, colorGreen = 0.3f, colorBlue = 0.3f, colorAlpha = 1.0;
            Individual &individual = *subpop->parent_individuals_[static_cast<size_t>(individualArrayIndex)];
            
            if (Individual::s_any_individual_color_set_ && individual.color_set_)
            {
                colorRed = individual.colorR_ / 255.0F;
                colorGreen = individual.colorG_ / 255.0F;
                colorBlue = individual.colorB_ / 255.0F;
            }
            else
            {
                // use individual trait values to determine color; we use fitness values cached in UpdateFitness, so we don't have to call out to mutationEffect() callbacks
                // we use cached_unscaled_fitness_ so individual fitness, unscaled by subpopulation fitness, is used for coloring
                double fitness = individual.cached_unscaled_fitness_;
                
                if (!std::isnan(fitness))
                    RGBForFitness(fitness, &colorRed, &colorGreen, &colorBlue, scalingFactor);
            }
            
            for (int j = 0; j < 4; ++j)
            {
                *(colors++) = colorRed;
                *(colors++) = colorGreen;
                *(colors++) = colorBlue;
                *(colors++) = colorAlpha;
            }
            
            displayListIndex++;
            
            // If we've filled our buffers, get ready to draw more
            if (displayListIndex == kMaxGLRects)
            {
                // Draw our arrays
                glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
                
                // And get ready to draw more
                vertices = glArrayVertices;
                colors = glArrayColors;
                displayListIndex = 0;
            }
        }
        
        // Draw any leftovers
        if (displayListIndex)
        {
            // Draw our arrays
            glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
        }
        
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_COLOR_ARRAY);
    }
    else
    {
        // This is what we do if we cannot display a subpopulation because there are too many individuals in it to display
        glColor3f(0.9f, 0.9f, 1.0f);
        
        int ox = bounds.left(), oy = bounds.top();
        
        glRecti(ox + 1, oy + 1, ox + bounds.width() - 1, oy + bounds.height() - 1);
    }
}

void QtSLiMIndividualsWidget::cacheDisplayBufferForMapForSubpopulation(SpatialMap *background_map, Subpopulation *subpop)
{
	// Cache a display buffer for the given background map.  This method should be called only in the 2D "xy"
	// case; in the 1D case we can't know the maximum width ahead of time, so we just draw rects without caching,
	// and in the 3D "xyz" case we don't know which z-slice to use so we can't display the spatial map.
	// The window might be too narrow to display a full-size map right now, but we want to premake a full-size map.
	// The sizing logic here is taken from drawRect:, assuming that we are not constrained in width.
	
	// By the way, it may occur to the reader to wonder why we keep the buffer as uint8_t values, given that we
	// convert to and from uin8_t for the display code that uses float RGB components.  My rationale is that
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
	if (background_map->display_buffer_ && ((background_map->buffer_width_ != max_width) || (background_map->buffer_height_ != max_height)))
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
		
		uint8_t *buf_ptr = display_buf;
		int64_t xsize = background_map->grid_size_[0];
		int64_t ysize = background_map->grid_size_[1];
		double *values = background_map->values_;
		bool interpolate = background_map->interpolate_;
		
		for (int yc = 0; yc < max_height; yc++)
		{
			for (int xc = 0; xc < max_width; xc++)
			{
				// Look up the nearest map point and get its value; interpolate if requested
				double x_fraction = (xc + 0.5) / max_width;		// pixel center
				double y_fraction = (yc + 0.5) / max_height;		// pixel center
				double value;
				
				if (interpolate)
				{
					double x_map = x_fraction * (xsize - 1);
					double y_map = y_fraction * (ysize - 1);
					int x1_map = static_cast<int>(floor(x_map));
					int y1_map = static_cast<int>(floor(y_map));
					int x2_map = static_cast<int>(ceil(x_map));
					int y2_map = static_cast<int>(ceil(y_map));
					double fraction_x2 = x_map - x1_map;
					double fraction_x1 = 1.0 - fraction_x2;
					double fraction_y2 = y_map - y1_map;
					double fraction_y1 = 1.0 - fraction_y2;
					double value_x1_y1 = values[x1_map + y1_map * xsize] * fraction_x1 * fraction_y1;
					double value_x2_y1 = values[x2_map + y1_map * xsize] * fraction_x2 * fraction_y1;
					double value_x1_y2 = values[x1_map + y2_map * xsize] * fraction_x1 * fraction_y2;
					double value_x2_y2 = values[x2_map + y2_map * xsize] * fraction_x2 * fraction_y2;
					
					value = value_x1_y1 + value_x2_y1 + value_x1_y2 + value_x2_y2;
				}
				else
				{
					int x_map = qRound(x_fraction * (xsize - 1));
					int y_map = qRound(y_fraction * (ysize - 1));
					
					value = values[x_map + y_map * xsize];
				}
				
				// Given the (interpolated?) value, look up the color, interpolating if necessary
				double rgb[3];
				
				background_map->ColorForValue(value, rgb);
				
				// Write the color values to the buffer
				*(buf_ptr++) = static_cast<uint8_t>(round(rgb[0] * 255.0));
				*(buf_ptr++) = static_cast<uint8_t>(round(rgb[1] * 255.0));
				*(buf_ptr++) = static_cast<uint8_t>(round(rgb[2] * 255.0));
			}
		}
	}
}

void QtSLiMIndividualsWidget::_drawBackgroundSpatialMap(SpatialMap *background_map, QRect bounds, Subpopulation *subpop, bool showGridPoints)
{
    // We have a spatial map with a color map, so use it to draw the background
	int bounds_x1 = bounds.x();
	int bounds_y1 = bounds.y();
	int bounds_x2 = bounds.x() + bounds.width();
	int bounds_y2 = bounds.y() + bounds.height();
	
	//glColor3f(0.0, 0.0, 0.0);
	//glRecti(bounds_x1, bounds_y1, bounds_x2, bounds_y2);
	
	int displayListIndex;
	float *vertices = nullptr, *colors = nullptr;
	
	// Set up to draw rects
	displayListIndex = 0;
	
	vertices = glArrayVertices;
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, glArrayVertices);
	
	colors = glArrayColors;
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_FLOAT, 0, glArrayColors);
	
	if (background_map->spatiality_ == 1)
	{
		// This is the spatiality "x" and "y" cases; they are the only 1D spatiality values for which SLiMgui will draw
		// In the 1D case we can't cache a display buffer, since we don't know what aspect ratio to use, so we just
		// draw rects.  Whether those rects are horizontal or vertical will depend on the spatiality of the map.  Most
		// of the code is identical, though, because of the way we handle dimensions, so we share the two cases here.
		bool spatiality_is_x = (background_map->spatiality_string_ == "x");
		int64_t xsize = background_map->grid_size_[0];
		double *values = background_map->values_;
		
		if (background_map->interpolate_)
		{
			// Interpolation, so we need to draw every line individually
			int min_coord = (spatiality_is_x ? bounds_x1 : bounds_y1);
			int max_coord = (spatiality_is_x ? bounds_x2 : bounds_y2);
			
			for (int xc = min_coord; xc < max_coord; ++xc)
			{
				double x_fraction = (xc + 0.5 - min_coord) / (max_coord - min_coord);	// values evaluated at pixel centers
				double x_map = x_fraction * (xsize - 1);
				int x1_map = static_cast<int>(floor(x_map));
				int x2_map = static_cast<int>(ceil(x_map));
				double fraction_x2 = x_map - x1_map;
				double fraction_x1 = 1.0 - fraction_x2;
				double value_x1 = values[x1_map] * fraction_x1;
				double value_x2 = values[x2_map] * fraction_x2;
				double value = value_x1 + value_x2;
				
				int x1, x2, y1, y2;
				
				if (spatiality_is_x)
				{
					x1 = xc;
					x2 = xc + 1;
					y1 = bounds_y1;
					y2 = bounds_y2;
				}
				else
				{
					y1 = (max_coord - 1) - xc + min_coord;	// flip for y, to use Cartesian coordinates
					y2 = y1 + 1;
					x1 = bounds_x1;
					x2 = bounds_x2;
				}
				
				float rgb[3];
				
				background_map->ColorForValue(value, rgb);
				
				//glColor3f(red, green, blue);
				//glRecti(x1, y1, x2, y2);
				
				*(vertices++) = x1;
				*(vertices++) = y1;
				*(vertices++) = x1;
				*(vertices++) = y2;
				*(vertices++) = x2;
				*(vertices++) = y2;
				*(vertices++) = x2;
				*(vertices++) = y1;
				
				for (int j = 0; j < 4; ++j)
				{
					*(colors++) = rgb[0];
					*(colors++) = rgb[1];
					*(colors++) = rgb[2];
					*(colors++) = 1.0;
				}
				
				displayListIndex++;
				
				// If we've filled our buffers, get ready to draw more
				if (displayListIndex == kMaxGLRects)
				{
					// Draw our arrays
					glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
					
					// And get ready to draw more
					vertices = glArrayVertices;
					colors = glArrayColors;
					displayListIndex = 0;
				}
			}
		}
		else
		{
			// No interpolation, so we can draw whole grid blocks
			for (int xc = 0; xc < xsize; xc++)
			{
				double value = (spatiality_is_x ? values[xc] : values[(xsize - 1) - xc]);	// flip for y, to use Cartesian coordinates
				int x1, x2, y1, y2;
				
				if (spatiality_is_x)
				{
					x1 = qRound(((xc - 0.5) / (xsize - 1)) * bounds.width() + bounds.x());
					x2 = qRound(((xc + 0.5) / (xsize - 1)) * bounds.width() + bounds.x());
					
					if (x1 < bounds_x1) x1 = bounds_x1;
					if (x2 > bounds_x2) x2 = bounds_x2;
					
					y1 = bounds_y1;
					y2 = bounds_y2;
				}
				else
				{
					y1 = qRound(((xc - 0.5) / (xsize - 1)) * bounds.height() + bounds.y());
					y2 = qRound(((xc + 0.5) / (xsize - 1)) * bounds.height() + bounds.y());
					
					if (y1 < bounds_y1) y1 = bounds_y1;
					if (y2 > bounds_y2) y2 = bounds_y2;
					
					x1 = bounds_x1;
					x2 = bounds_x2;
				}
				
				float rgb[3];
				
				background_map->ColorForValue(value, rgb);
				
				//glColor3f(red, green, blue);
				//glRecti(x1, y1, x2, y2);
				
				*(vertices++) = x1;
				*(vertices++) = y1;
				*(vertices++) = x1;
				*(vertices++) = y2;
				*(vertices++) = x2;
				*(vertices++) = y2;
				*(vertices++) = x2;
				*(vertices++) = y1;
				
				for (int j = 0; j < 4; ++j)
				{
					*(colors++) = rgb[0];
					*(colors++) = rgb[1];
					*(colors++) = rgb[2];
					*(colors++) = 1.0;
				}
				
				displayListIndex++;
				
				// If we've filled our buffers, get ready to draw more
				if (displayListIndex == kMaxGLRects)
				{
					// Draw our arrays
					glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
					
					// And get ready to draw more
					vertices = glArrayVertices;
					colors = glArrayColors;
					displayListIndex = 0;
				}
			}
		}
	}
	else // if (background_map->spatiality_ == 2)
	{
		// This is the spatiality "xy" case; it is the only 2D spatiality for which SLiMgui will draw
		
		// First, cache the display buffer if needed.  If this succeeds, we'll use it.
		// It should always succeed, so the tile-drawing code below is dead code, kept for parallelism with the 1D case.
		cacheDisplayBufferForMapForSubpopulation(background_map, subpop);
		
		uint8_t *display_buf = background_map->display_buffer_;
		
		if (display_buf)
		{
			// Use a cached display buffer to draw.
			// FIXME I think there is a bug here somewhere, the boundaries of the pixels fluctuate oddly when the
			// individuals pane is resized, even if the actual area the map is displaying in doesn't change size.
			// Maybe try using GL_POINTS?
			int buf_width = background_map->buffer_width_;
			int buf_height = background_map->buffer_height_;
			bool display_full_size = ((bounds.width() == buf_width) && (bounds.height() == buf_height));
			float scale_x = bounds.width() / static_cast<float>(buf_width);
			float scale_y = bounds.height() / static_cast<float>(buf_height);
			
			// Then run through the pixels in the display buffer and draw them; this could be done
			// with some sort of OpenGL image-drawing method instead, but it's actually already
			// remarkably fast, at least on my machine, and drawing an image with OpenGL seems very
			// gross, and I tried it once before and couldn't get it to work well...
			for (int yc = 0; yc < buf_height; yc++)
			{
				// We flip the buffer vertically; it's the simplest way to get it into the right coordinate space
				uint8_t *buf_ptr = display_buf + ((buf_height - 1) - yc) * buf_width * 3;
				
				for (int xc = 0; xc < buf_width; xc++)
				{
					float red = *(buf_ptr++) / 255.0f;
					float green = *(buf_ptr++) / 255.0f;
					float blue = *(buf_ptr++) / 255.0f;
					float left, right, top, bottom;
					
					if (display_full_size)
					{
						left = bounds_x1 + xc;
						right = left + 1.0f;
						top = bounds_y1 + yc;
						bottom = top + 1.0f;
					}
					else
					{
						left = bounds_x1 + xc * scale_x;
						right = bounds_x1 + (xc + 1) * scale_x;
						top = bounds_y1 + yc * scale_y;
						bottom = bounds_y1 + (yc + 1) * scale_y;
					}
					
					*(vertices++) = left;
					*(vertices++) = top;
					*(vertices++) = left;
					*(vertices++) = bottom;
					*(vertices++) = right;
					*(vertices++) = bottom;
					*(vertices++) = right;
					*(vertices++) = top;
					
					for (int j = 0; j < 4; ++j)
					{
						*(colors++) = red;
						*(colors++) = green;
						*(colors++) = blue;
						*(colors++) = 1.0;
					}
					
					displayListIndex++;
					
					// If we've filled our buffers, get ready to draw more
					if (displayListIndex == kMaxGLRects)
					{
						// Draw our arrays
						glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
						
						// And get ready to draw more
						vertices = glArrayVertices;
						colors = glArrayColors;
						displayListIndex = 0;
					}
				}
			}
		}
		else
		{
			// Draw rects for each map tile, without caching.  Not as slow as you might expect,
			// but for really big maps it does get cumbersome.  This is dead code now, overridden
			// by the buffer-drawing code above, which also handles interpolation correctly.
			int64_t xsize = background_map->grid_size_[0];
			int64_t ysize = background_map->grid_size_[1];
			double *values = background_map->values_;
			int n_colors = background_map->n_colors_;
			
			for (int yc = 0; yc < ysize; yc++)
			{
				int y1 = qRound(((yc - 0.5) / (ysize - 1)) * bounds.height() + bounds.y());
				int y2 = qRound(((yc + 0.5) / (ysize - 1)) * bounds.height() + bounds.y());
				
				if (y1 < bounds_y1) y1 = bounds_y1;
				if (y2 > bounds_y2) y2 = bounds_y2;
				
				// Flip our display, since our coordinate system is flipped relative to our buffer
				double *values_row = values + ((ysize - 1) - yc) * xsize;
				
				for (int xc = 0; xc < xsize; xc++)
				{
					double value = *(values_row + xc);
					int x1 = qRound(((xc - 0.5) / (xsize - 1)) * bounds.width() + bounds.x());
					int x2 = qRound(((xc + 0.5) / (xsize - 1)) * bounds.width() + bounds.x());
					
					if (x1 < bounds_x1) x1 = bounds_x1;
					if (x2 > bounds_x2) x2 = bounds_x2;
					
					float value_fraction = (background_map->colors_min_ < background_map->colors_max_) ? static_cast<float>((value - background_map->colors_min_) / (background_map->colors_max_ - background_map->colors_min_)) : 0.0f;
					float color_index = value_fraction * (n_colors - 1);
					int color_index_1 = static_cast<int>(floorf(color_index));
					int color_index_2 = static_cast<int>(ceilf(color_index));
					
					if (color_index_1 < 0) color_index_1 = 0;
					if (color_index_1 >= n_colors) color_index_1 = n_colors - 1;
					if (color_index_2 < 0) color_index_2 = 0;
					if (color_index_2 >= n_colors) color_index_2 = n_colors - 1;
					
					float color_2_weight = color_index - color_index_1;
					float color_1_weight = 1.0f - color_2_weight;
					
					float red1 = background_map->red_components_[color_index_1];
					float green1 = background_map->green_components_[color_index_1];
					float blue1 = background_map->blue_components_[color_index_1];
					float red2 = background_map->red_components_[color_index_2];
					float green2 = background_map->green_components_[color_index_2];
					float blue2 = background_map->blue_components_[color_index_2];
					float red = red1 * color_1_weight + red2 * color_2_weight;
					float green = green1 * color_1_weight + green2 * color_2_weight;
					float blue = blue1 * color_1_weight + blue2 * color_2_weight;
					
					//glColor3f(red, green, blue);
					//glRecti(x1, y1, x2, y2);
					
					*(vertices++) = x1;
					*(vertices++) = y1;
					*(vertices++) = x1;
					*(vertices++) = y2;
					*(vertices++) = x2;
					*(vertices++) = y2;
					*(vertices++) = x2;
					*(vertices++) = y1;
					
					for (int j = 0; j < 4; ++j)
					{
						*(colors++) = red;
						*(colors++) = green;
						*(colors++) = blue;
						*(colors++) = 1.0;
					}
					
					displayListIndex++;
					
					// If we've filled our buffers, get ready to draw more
					if (displayListIndex == kMaxGLRects)
					{
						// Draw our arrays
						glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
						
						// And get ready to draw more
						vertices = glArrayVertices;
						colors = glArrayColors;
						displayListIndex = 0;
					}
					
					//std::cout << "x = " << x << ", y = " << y << ", value = " << value << ": color_index = " << color_index << ", color_index_1 = " << color_index_1 << ", color_index_2 = " << color_index_2 << ", color_1_weight = " << color_1_weight << ", color_2_weight = " << color_2_weight << ", red = " << red << std::endl;
				}
			}
		}
	}
	
	// Draw any leftovers
	if (displayListIndex)
	{
		// Draw our arrays
		glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
	}
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
    
    if (showGridPoints)
    {
        // BCH 9/29/2023 new feature: draw boxes showing where the grid nodes are, since that is rather confusing!
        float margin_outer = 5.5f;
        float margin_inner = 3.5f;
        float spacing = 10.0f;
        int64_t xsize = background_map->grid_size_[0];
        int64_t ysize = background_map->grid_size_[1];
        double *values = background_map->values_;
        
        // require that there is sufficient space that we're not just showing a packed grid of squares
        // downsize to small and smaller depictions as needed
        if (((xsize - 1) * (margin_outer * 2.0 + spacing) > bounds_x2) || ((ysize - 1) * (margin_outer * 2.0 + spacing) > bounds_y2))
        {
            margin_outer = 4.5f;
            margin_inner = 2.5f;
            spacing = 8.0;
        }
        if (((xsize - 1) * (margin_outer * 2.0 + spacing) > bounds_x2) || ((ysize - 1) * (margin_outer * 2.0 + spacing) > bounds_y2))
        {
            margin_outer = 3.5f;
            margin_inner = 1.5f;
            spacing = 6.0;
        }
        if (((xsize - 1) * (margin_outer * 2.0 + spacing) > bounds_x2) || ((ysize - 1) * (margin_outer * 2.0 + spacing) > bounds_y2))
        {
            margin_outer = 1.0f;
            margin_inner = 0.0f;
            spacing = 2.0;
        }
        
        if (((xsize - 1) * (margin_outer * 2.0 + spacing) <= bounds_x2) && ((ysize - 1) * (margin_outer * 2.0 + spacing) <= bounds_y2))
        {
            // Set up to draw rects
            displayListIndex = 0;
            
            vertices = glArrayVertices;
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(2, GL_FLOAT, 0, glArrayVertices);
            
            colors = glArrayColors;
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(4, GL_FLOAT, 0, glArrayColors);
            
            // first pass we draw squares to make outlines, second pass we draw the interiors in color
            for (int pass = 0; pass <= 1; ++pass)
            {
                const float margin = ((pass == 0) ? margin_outer : margin_inner);
                
                if (margin == 0.0)
                    continue;
                
                for (int x = 0; x < xsize; ++x)
                {
                    for (int y = 0; y < ysize; ++y)
                    {
                        float position_x = x / (float)(xsize - 1);	// 0 to 1
                        float position_y = y / (float)(ysize - 1);	// 0 to 1
                        
                        float centerX = (float)(bounds_x1 + round(position_x * bounds.width()));
                        float centerY = (float)(bounds_y1 + bounds.height() - round(position_y * bounds.height()));
                        float left = centerX - margin;
                        float top = centerY - margin;
                        float right = centerX + margin;
                        float bottom = centerY + margin;
                        
                        if (left < bounds_x1)
                            left = bounds_x1;
                        if (top < bounds_y1)
                            top = bounds_y1;
                        if (right > bounds_x2)
                            right = bounds_x2;
                        if (bottom > bounds_y2)
                            bottom = bounds_y2;
                        
                        *(vertices++) = left;
                        *(vertices++) = top;
                        *(vertices++) = left;
                        *(vertices++) = bottom;
                        *(vertices++) = right;
                        *(vertices++) = bottom;
                        *(vertices++) = right;
                        *(vertices++) = top;
                        
                        if (pass == 0)
                        {
                            for (int j = 0; j < 4; ++j)
                            {
                                *(colors++) = 1.0;
                                *(colors++) = 0.25;
                                *(colors++) = 0.25;
                                *(colors++) = 1.0;
                            }
                        }
                        else
                        {
                            // look up the map's color at this grid point
                            float rgb[3];
                            double value = values[x + y * xsize];
                            
                            background_map->ColorForValue(value, rgb);
                            
                            for (int j = 0; j < 4; ++j)
                            {
                                *(colors++) = rgb[0];
                                *(colors++) = rgb[1];
                                *(colors++) = rgb[2];
                                *(colors++) = 1.0;
                            }
                        }
                        
                        displayListIndex++;
                        
                        // If we've filled our buffers, get ready to draw more
                        if (displayListIndex == kMaxGLRects)
                        {
                            // Draw our arrays
                            glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
                            
                            // And get ready to draw more
                            vertices = glArrayVertices;
                            colors = glArrayColors;
                            displayListIndex = 0;
                        }
                    }
                }
            }
            
            // Draw any leftovers
            if (displayListIndex)
            {
                // Draw our arrays
                glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
            }
            
            glDisableClientState(GL_VERTEX_ARRAY);
            glDisableClientState(GL_COLOR_ARRAY);
        }
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

void QtSLiMIndividualsWidget::drawSpatialBackgroundInBoundsForSubpopulation(QRect bounds, Subpopulation * subpop, int /* dimensionality */)
{
    auto backgroundIter = subviewSettings.find(subpop->subpopulation_id_);
	PopulationViewSettings background;
	SpatialMap *background_map = nullptr;
	
	if (backgroundIter == subviewSettings.end())
	{
		// The user has not made a choice, so choose a temporary default.  We don't want this choice to "stick",
		// so that we can, e.g., begin as black and then change to a spatial map if one is defined.
		chooseDefaultBackgroundSettingsForSubpopulation(&background, &background_map, subpop);
	}
	else
	{
		// The user has made a choice; verify that it is acceptable, and then use it.
		background = backgroundIter->second;
		
		if (background.backgroundType == 3)
		{
			SpatialMapMap &spatial_maps = subpop->spatial_maps_;
			auto map_iter = spatial_maps.find(background.spatialMapName);
			
			if (map_iter != spatial_maps.end())
			{
				background_map = map_iter->second;
				
				// if the user somehow managed to choose a map that is not of an acceptable dimensionality, reject it here
				if ((background_map->spatiality_string_ != "x") && (background_map->spatiality_string_ != "y") && (background_map->spatiality_string_ != "xy"))
					background_map = nullptr;
			}
		}
		
		// if we're supposed to use a background map but we couldn't find it, or it's unacceptable, revert to black
		if ((background.backgroundType == 3) && !background_map)
			background.backgroundType = 0;
	}
	
	if ((background.backgroundType == 3) && background_map)
	{
		_drawBackgroundSpatialMap(background_map, bounds, subpop, background.showGridPoints);
	}
	else
	{
		// No background map, so just clear to the preferred background color
		int backgroundColor = background.backgroundType;
		
		if (backgroundColor == 0)
			glColor3f(0.0, 0.0, 0.0);
		else if (backgroundColor == 1)
			glColor3f(0.3f, 0.3f, 0.3f);
		else if (backgroundColor == 2)
			glColor3f(1.0, 1.0, 1.0);
		else
			glColor3f(0.0, 0.0, 0.0);
		
		glRecti(bounds.x(), bounds.y(), bounds.x() + bounds.width(), bounds.y() + bounds.height());
	}
}

void QtSLiMIndividualsWidget::drawSpatialIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds, int dimensionality, float *forceColor)
{
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
	double scalingFactor = 0.8; // used to be controller->fitnessColorScale;
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	double bounds_x0 = subpop->bounds_x0_, bounds_x1 = subpop->bounds_x1_;
	double bounds_y0 = subpop->bounds_y0_, bounds_y1 = subpop->bounds_y1_;
	double bounds_x_size = bounds_x1 - bounds_x0, bounds_y_size = bounds_y1 - bounds_y0;
	
	QRect individualArea(bounds.x(), bounds.y(), bounds.width() - 1, bounds.height() - 1);
	
	int individualArrayIndex, displayListIndex;
	float *vertices = nullptr, *colors = nullptr;
	
	// Set up to draw rects
	displayListIndex = 0;
	
	vertices = glArrayVertices;
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, glArrayVertices);
	
	colors = glArrayColors;
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_FLOAT, 0, glArrayColors);
	
	// First we outline all individuals
	if (dimensionality == 1)
		srandom(static_cast<unsigned int>(controller->community->Tick()));
	
	for (individualArrayIndex = 0; individualArrayIndex < subpopSize; ++individualArrayIndex)
	{
		// Figure out the rect to draw in; note we now use individualArrayIndex here, because the hit-testing code doesn't have an easy way to calculate the displayed individual index...
		Individual &individual = *subpop->parent_individuals_[static_cast<size_t>(individualArrayIndex)];
		float position_x, position_y;
		
		if (dimensionality == 1)
		{
			position_x = static_cast<float>((individual.spatial_x_ - bounds_x0) / bounds_x_size);
			position_y = static_cast<float>(random() / static_cast<double>(INT32_MAX));
			
			if ((position_x < 0.0f) || (position_x > 1.0f))		// skip points that are out of bounds
				continue;
		}
		else
		{
			position_x = static_cast<float>((individual.spatial_x_ - bounds_x0) / bounds_x_size);
			position_y = static_cast<float>((individual.spatial_y_ - bounds_y0) / bounds_y_size);
			
			if ((position_x < 0.0f) || (position_x > 1.0f) || (position_y < 0.0f) || (position_y > 1.0f))		// skip points that are out of bounds
				continue;
		}
		
		float centerX = static_cast<float>(individualArea.x() + round(position_x * individualArea.width()) + 0.5f);
		float centerY = static_cast<float>(individualArea.y() + individualArea.height() - round(position_y * individualArea.height()) + 0.5f);
		
		float left = centerX - 2.5f;
		float top = centerY - 2.5f;
		float right = centerX + 2.5f;
		float bottom = centerY + 2.5f;
		
		if (left < individualArea.x()) left = static_cast<float>(individualArea.x());
		if (top < individualArea.y()) top = static_cast<float>(individualArea.y());
		if (right > individualArea.x() + individualArea.width() + 1) right = static_cast<float>(individualArea.x() + individualArea.width() + 1);
		if (bottom > individualArea.y() + individualArea.height() + 1) bottom = static_cast<float>(individualArea.y() + individualArea.height() + 1);
		
		*(vertices++) = left;
		*(vertices++) = top;
		*(vertices++) = left;
		*(vertices++) = bottom;
		*(vertices++) = right;
		*(vertices++) = bottom;
		*(vertices++) = right;
		*(vertices++) = top;
		
		for (int j = 0; j < 4; ++j)
		{
			*(colors++) = 0.25;
			*(colors++) = 0.25;
			*(colors++) = 0.25;
			*(colors++) = 1.0;
		}
		
		displayListIndex++;
		
		// If we've filled our buffers, get ready to draw more
		if (displayListIndex == kMaxGLRects)
		{
			// Draw our arrays
			glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
			
			// And get ready to draw more
			vertices = glArrayVertices;
			colors = glArrayColors;
			displayListIndex = 0;
		}
	}
	
	// Then we draw all individuals
	if (dimensionality == 1)
        srandom(static_cast<unsigned int>(controller->community->Tick()));
	
	for (individualArrayIndex = 0; individualArrayIndex < subpopSize; ++individualArrayIndex)
	{
		// Figure out the rect to draw in; note we now use individualArrayIndex here, because the hit-testing code doesn't have an easy way to calculate the displayed individual index...
        Individual &individual = *subpop->parent_individuals_[static_cast<size_t>(individualArrayIndex)];
		float position_x, position_y;
		
		if (dimensionality == 1)
		{
            position_x = static_cast<float>((individual.spatial_x_ - bounds_x0) / bounds_x_size);
			position_y = static_cast<float>(random() / static_cast<double>(INT32_MAX));
			
			if ((position_x < 0.0f) || (position_x > 1.0f))		// skip points that are out of bounds
				continue;
		}
		else
		{
            position_x = static_cast<float>((individual.spatial_x_ - bounds_x0) / bounds_x_size);
			position_y = static_cast<float>((individual.spatial_y_ - bounds_y0) / bounds_y_size);
			
			if ((position_x < 0.0f) || (position_x > 1.0f) || (position_y < 0.0f) || (position_y > 1.0f))		// skip points that are out of bounds
				continue;
		}
		
        float centerX = static_cast<float>(individualArea.x() + round(position_x * individualArea.width()) + 0.5f);
		float centerY = static_cast<float>(individualArea.y() + individualArea.height() - round(position_y * individualArea.height()) + 0.5f);
		float left = centerX - 1.5f;
		float top = centerY - 1.5f;
		float right = centerX + 1.5f;
		float bottom = centerY + 1.5f;
		
		// clipping deliberately not done here; because individual rects are 3x3, they will fall at most one pixel
		// outside our drawing area, and thus the flaw will be covered by the view frame when it overdraws
		
		*(vertices++) = left;
		*(vertices++) = top;
		*(vertices++) = left;
		*(vertices++) = bottom;
		*(vertices++) = right;
		*(vertices++) = bottom;
		*(vertices++) = right;
		*(vertices++) = top;
		
		// dark gray default, for a fitness of NaN; should never happen
		float colorRed = 0.3f, colorGreen = 0.3f, colorBlue = 0.3f, colorAlpha = 1.0;
		
		if (Individual::s_any_individual_color_set_ && individual.color_set_)
		{
			colorRed = individual.colorR_ / 255.0F;
			colorGreen = individual.colorG_ / 255.0F;
			colorBlue = individual.colorB_ / 255.0F;
		}
        else if (forceColor)
        {
            // forceColor is used to make each species draw with a distinctive color in multispecies models in unified display mode
            colorRed = forceColor[0];
			colorGreen = forceColor[1];
			colorBlue = forceColor[2];
        }
		else
		{
			// use individual trait values to determine color; we used fitness values cached in UpdateFitness, so we don't have to call out to mutationEffect() callbacks
			// we use cached_unscaled_fitness_ so individual fitness, unscaled by subpopulation fitness, is used for coloring
			double fitness = individual.cached_unscaled_fitness_;
			
			if (!std::isnan(fitness))
				RGBForFitness(fitness, &colorRed, &colorGreen, &colorBlue, scalingFactor);
		}
		
		for (int j = 0; j < 4; ++j)
		{
			*(colors++) = colorRed;
			*(colors++) = colorGreen;
			*(colors++) = colorBlue;
			*(colors++) = colorAlpha;
		}
		
		displayListIndex++;
		
		// If we've filled our buffers, get ready to draw more
		if (displayListIndex == kMaxGLRects)
		{
			// Draw our arrays
			glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
			
			// And get ready to draw more
			vertices = glArrayVertices;
			colors = glArrayColors;
			displayListIndex = 0;
		}
	}
	
	// Draw any leftovers
	if (displayListIndex)
	{
		// Draw our arrays
		glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
	}
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
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
            if (static_cast<QMetaType::Type>(action->data().type()) == QMetaType::QString)  // for some reason this method's return type is apparently misdeclared; see the doc
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
        runContextMenuAtPoint(p_event->globalPos(), subpopForEvent);
    
    // redraw to get rid of action button highlight
    actionButtonHighlightSubpopID_ = -1;
    update();
}






































