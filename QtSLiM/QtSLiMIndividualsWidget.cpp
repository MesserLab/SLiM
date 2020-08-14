//
//  QtSLiMIndividualsWidget.cpp
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


#include "QtSLiMIndividualsWidget.h"
#include "QtSLiMWindow.h"

#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QtGlobal>
#include <QDebug>


// OpenGL constants
static const int kMaxGLRects = 2000;				// 2000 rects
static const int kMaxVertices = kMaxGLRects * 4;	// 4 vertices each


QtSLiMIndividualsWidget::QtSLiMIndividualsWidget(QWidget *parent, Qt::WindowFlags f) : QOpenGLWidget(parent, f)
{
    displayMode = -1;	// don't know yet whether the model is spatial or not, which will determine our initial choice
    
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
    
    //painter.eraseRect(rect());      // erase to background color, which is not guaranteed
    painter.fillRect(rect(), Qt::red);
    
    //
	//	NOTE this code is parallel to code in tileSubpopulations: and both should be maintained!
	//
	
	QRect bounds = rect();
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
	SLiMSim *sim = controller->sim;
	std::vector<Subpopulation*> selectedSubpopulations = controller->selectedSubpopulations();
	int selectedSubpopCount = static_cast<int>(selectedSubpopulations.size());
	
    // In SLiMgui this has to be called in advance, to swap in/out the error view, but here we
    // can simply call it before each update to pre-plan, making for a simpler design
    tileSubpopulations(selectedSubpopulations);
    
	// Decide on our display mode
	if (!controller->invalidSimulation() && sim && sim->simulation_valid_ && (sim->generation_ >= 1))
	{
		if (displayMode == -1)
			displayMode = ((sim->spatial_dimensionality_ == 0) ? 0 : 1);
		if ((displayMode == 1) && (sim->spatial_dimensionality_ == 0))
			displayMode = 0;
	}
	
	if ((selectedSubpopCount == 0) || !canDisplayAllIndividuals)
	{
		// clear to a shade of gray
        painter.beginNativePainting();
        
		glColor3f(0.9f, 0.9f, 0.9f);
		glRecti(0, 0, bounds.width(), bounds.height());
		
        // display a message if we have too many subpops to show
        if (!canDisplayAllIndividuals)
        {
            painter.endNativePainting();
            
            painter.setPen(Qt::darkGray);
            painter.drawText(bounds, Qt::AlignCenter, "too many subpops\nor individuals\nto display –\ncontrol-click to\nselect a different\ndisplay mode");
            
            painter.beginNativePainting();
        }
        
		// Frame our view
		drawViewFrameInBounds(bounds);
        
        painter.endNativePainting();
	}
	else
	{
        // Clear to background gray if any background will show through
		if (selectedSubpopulations.size() > 1)
            painter.fillRect(rect(), palette().color(QPalette::Window));
		
        painter.beginNativePainting();
        
		for (Subpopulation *subpop : selectedSubpopulations)
		{
			auto tileIter = subpopTiles.find(subpop->subpopulation_id_);
			
			if (tileIter != subpopTiles.end())
			{
				QRect tileBounds = tileIter->second;
				
                if ((displayMode == 1) && (sim->spatial_dimensionality_ == 1))
				{
					drawSpatialBackgroundInBoundsForSubpopulation(tileBounds, subpop, sim->spatial_dimensionality_);
					drawSpatialIndividualsFromSubpopulationInArea(subpop, tileBounds.adjusted(1, 1, -1, -1), sim->spatial_dimensionality_);
					drawViewFrameInBounds(tileBounds);
				}
				else if ((displayMode == 1) && (sim->spatial_dimensionality_ > 1))
				{
					// clear to a shade of gray
					glColor3f(0.9f, 0.9f, 0.9f);
                    glRecti(tileBounds.left(), tileBounds.top(), (tileBounds.left() + tileBounds.width()), (tileBounds.top() + tileBounds.height()));
					
					// Frame our view
					drawViewFrameInBounds(tileBounds);
					
					// Now determine a subframe and draw spatial information inside that.
					QRect spatialDisplayBounds = spatialDisplayBoundsForSubpopulation(subpop, tileBounds);
					
					drawSpatialBackgroundInBoundsForSubpopulation(spatialDisplayBounds, subpop, sim->spatial_dimensionality_);
					drawSpatialIndividualsFromSubpopulationInArea(subpop, spatialDisplayBounds, sim->spatial_dimensionality_);
                    drawViewFrameInBounds(spatialDisplayBounds.adjusted(-1, -1, 1, 1));
				}
				else	// displayMode == 0
				{
					// Clear to white
					glColor3f(1.0, 1.0, 1.0);
					glRecti(tileBounds.left(), tileBounds.top(), (tileBounds.left() + tileBounds.width()), (tileBounds.top() + tileBounds.height()));
					
                    drawViewFrameInBounds(tileBounds);
					drawIndividualsFromSubpopulationInArea(subpop, tileBounds);
				}
			}
		}
        
        painter.endNativePainting();
	}
}

void QtSLiMIndividualsWidget::tileSubpopulations(std::vector<Subpopulation*> &selectedSubpopulations)
{
	//
	//	NOTE this code is parallel to code in drawRect: and both should be maintained!
	//
	
	// We will decide upon new tiles for our subpopulations here, so start out empty
	subpopTiles.clear();
	
    QRect bounds = rect();
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
	SLiMSim *sim = controller->sim;
    int selectedSubpopCount = static_cast<int>(selectedSubpopulations.size());
	
	// Decide on our display mode
    if (!controller->invalidSimulation() && sim && sim->simulation_valid_ && (sim->generation_ >= 1))
	{
		if (displayMode == -1)
			displayMode = ((sim->spatial_dimensionality_ == 0) ? 0 : 1);
		if ((displayMode == 1) && (sim->spatial_dimensionality_ == 0))
			displayMode = 0;
	}
	
	if (selectedSubpopCount == 0)
	{
		canDisplayAllIndividuals = true;
	}
	else if (selectedSubpopCount > 10)
	{
		canDisplayAllIndividuals = false;
	}
	else if (selectedSubpopCount == 1)
	{
		Subpopulation *selectedSubpop = selectedSubpopulations[0];
		
		subpopTiles.insert(std::pair<slim_objectid_t, QRect>(selectedSubpop->subpopulation_id_, bounds));
		
		if ((displayMode == 1) && (sim->spatial_dimensionality_ == 1))
		{
			canDisplayAllIndividuals = true;
		}
		else if ((displayMode == 1) && (sim->spatial_dimensionality_ > 1))
		{
			canDisplayAllIndividuals = true;
		}
		else
		{
            canDisplayAllIndividuals = canDisplayIndividualsFromSubpopulationInArea(selectedSubpop, bounds);
		}
	}
    else if (displayMode == 1)
	{
		// spatial display adaptively finds the layout the maximizes the pixel area covered, and cannot fail
		int64_t bestTotalExtent = 0;
		
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
			
			for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
			{
				int columnIndex = subpopIndex % columnCount;
				int rowIndex = subpopIndex / columnCount;
				int boxLeft = qRound(bounds.x() + columnIndex * (interBoxSpace + boxWidth));
				int boxRight = qRound(bounds.x() + columnIndex * (interBoxSpace + boxWidth) + boxWidth);
				int boxTop = qRound(bounds.y() + rowIndex * (interBoxSpace + boxHeight));
				int boxBottom = qRound(bounds.y() + rowIndex * (interBoxSpace + boxHeight) + boxHeight);
				QRect boxBounds(boxLeft, boxTop, boxRight - boxLeft, boxBottom - boxTop);
				Subpopulation *subpop = selectedSubpopulations[static_cast<size_t>(subpopIndex)];
				
				candidateTiles.insert(std::pair<slim_objectid_t, QRect>(subpop->subpopulation_id_, boxBounds));
				
				// find out what pixel area actually gets used by this box, and use that to choose the optimal layout
				QRect spatialDisplayBounds = spatialDisplayBoundsForSubpopulation(subpop, boxBounds);
				int64_t extent = static_cast<int64_t>(spatialDisplayBounds.width()) * static_cast<int64_t>(spatialDisplayBounds.height());
				
				totalExtent += extent;
			}
			
			if (totalExtent > bestTotalExtent)
			{
				bestTotalExtent = totalExtent;
				std::swap(subpopTiles, candidateTiles);
			}
		}
		
		canDisplayAllIndividuals = true;
	}
	else	// displayMode == 0
	{
		// non-spatial display always uses vertically stacked maximum-width tiles, but can fail if they are too small
		int interBoxSpace = 5;
		int totalInterbox = interBoxSpace * (selectedSubpopCount - 1);
		double boxHeight = (bounds.height() - totalInterbox) / static_cast<double>(selectedSubpopCount);
		
        canDisplayAllIndividuals = true;
        
		for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
		{
			int boxTop = qRound(bounds.top() + subpopIndex * (interBoxSpace + boxHeight));
			int boxBottom = qRound(bounds.top() + subpopIndex * (interBoxSpace + boxHeight) + boxHeight);
			QRect boxBounds(bounds.left(), boxTop, bounds.width(), boxBottom - boxTop);
			Subpopulation *subpop = selectedSubpopulations[static_cast<size_t>(subpopIndex)];
			
			subpopTiles.insert(std::pair<slim_objectid_t, QRect>(subpop->subpopulation_id_, boxBounds));
			
			if (!canDisplayIndividualsFromSubpopulationInArea(subpop, boxBounds))
			{
				subpopTiles.clear();
				canDisplayAllIndividuals = false;
                break;
			}
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
	QRect spatialDisplayBounds = tileBounds.adjusted(1, 1, -1, -1);
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
	
	return spatialDisplayBounds;
}

void QtSLiMIndividualsWidget::drawViewFrameInBounds(QRect bounds)
{
	int ox = bounds.left(), oy = bounds.top();
	
	glColor3f(0.77f, 0.77f, 0.77f);
	glRecti(ox, oy, ox + 1, oy + bounds.height());
	glRecti(ox + 1, oy, ox + bounds.width() - 1, oy + 1);
	glRecti(ox + bounds.width() - 1, oy, ox + bounds.width(), oy + bounds.height());
	glRecti(ox + 1, oy + bounds.height() - 1, ox + bounds.width() - 1, oy + bounds.height());
}

void QtSLiMIndividualsWidget::drawIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds)
{
	//
	//	NOTE this code is parallel to the code in canDisplayIndividualsFromSubpopulation:inArea: and should be maintained in parallel
	//
	
    //QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
	double scalingFactor = 0.8; // controller->fitnessColorScale;
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	int squareSize, viewColumns = 0, viewRows = 0;
	double subpopFitnessScaling = subpop->last_fitness_scaling_;
	
	if ((subpopFitnessScaling <= 0.0) || !std::isfinite(subpopFitnessScaling))
		subpopFitnessScaling = 1.0;
	
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
			
			if (Individual::s_any_individual_color_set_ && !individual.color_.empty())
			{
				colorRed = individual.color_red_;
				colorGreen = individual.color_green_;
				colorBlue = individual.color_blue_;
			}
			else
			{
				// use individual trait values to determine color; we use fitness values cached in UpdateFitness, so we don't have to call out to fitness callbacks
				// we normalize fitness values with subpopFitnessScaling so individual fitness, unscaled by subpopulation fitness, is used for coloring
				double fitness = individual.cached_fitness_UNSAFE_;
				
				if (!std::isnan(fitness))
					RGBForFitness(fitness / subpopFitnessScaling, &colorRed, &colorGreen, &colorBlue, scalingFactor);
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
		
		for (int y = 0; y < max_height; y++)
		{
			for (int x = 0; x < max_width; x++)
			{
				// Look up the nearest map point and get its value; interpolate if requested
				double x_fraction = (x + 0.5) / max_width;		// pixel center
				double y_fraction = (y + 0.5) / max_height;		// pixel center
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

void QtSLiMIndividualsWidget::_drawBackgroundSpatialMap(SpatialMap *background_map, QRect bounds, Subpopulation *subpop)
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
			
			for (int x = min_coord; x < max_coord; ++x)
			{
				double x_fraction = (x + 0.5 - min_coord) / (max_coord - min_coord);	// values evaluated at pixel centers
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
					x1 = x;
					x2 = x + 1;
					y1 = bounds_y1;
					y2 = bounds_y2;
				}
				else
				{
					y1 = (max_coord - 1) - x + min_coord;	// flip for y, to use Cartesian coordinates
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
			for (int x = 0; x < xsize; x++)
			{
				double value = (spatiality_is_x ? values[x] : values[(xsize - 1) - x]);	// flip for y, to use Cartesian coordinates
				int x1, x2, y1, y2;
				
				if (spatiality_is_x)
				{
					x1 = qRound(((x - 0.5) / (xsize - 1)) * bounds.width() + bounds.x());
					x2 = qRound(((x + 0.5) / (xsize - 1)) * bounds.width() + bounds.x());
					
					if (x1 < bounds_x1) x1 = bounds_x1;
					if (x2 > bounds_x2) x2 = bounds_x2;
					
					y1 = bounds_y1;
					y2 = bounds_y2;
				}
				else
				{
					y1 = qRound(((x - 0.5) / (xsize - 1)) * bounds.height() + bounds.y());
					y2 = qRound(((x + 0.5) / (xsize - 1)) * bounds.height() + bounds.y());
					
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
			int buf_width = background_map->buffer_width_;
			int buf_height = background_map->buffer_height_;
			bool display_full_size = ((bounds.width() == buf_width) && (bounds.height() == buf_height));
			float scale_x = bounds.width() / static_cast<float>(buf_width);
			float scale_y = bounds.height() / static_cast<float>(buf_height);
			
			// Then run through the pixels in the display buffer and draw them; this could be done
			// with some sort of OpenGL image-drawing method instead, but it's actually already
			// remarkably fast, at least on my machine, and drawing an image with OpenGL seems very
			// gross, and I tried it once before and couldn't get it to work well...
			for (int y = 0; y < buf_height; y++)
			{
				// We flip the buffer vertically; it's the simplest way to get it into the right coordinate space
				uint8_t *buf_ptr = display_buf + ((buf_height - 1) - y) * buf_width * 3;
				
				for (int x = 0; x < buf_width; x++)
				{
					float red = *(buf_ptr++) / 255.0f;
					float green = *(buf_ptr++) / 255.0f;
					float blue = *(buf_ptr++) / 255.0f;
					float left, right, top, bottom;
					
					if (display_full_size)
					{
						left = bounds_x1 + x;
						right = left + 1.0f;
						top = bounds_y1 + y;
						bottom = top + 1.0f;
					}
					else
					{
						left = bounds_x1 + x * scale_x;
						right = bounds_x1 + (x + 1) * scale_x;
						top = bounds_y1 + y * scale_y;
						bottom = bounds_y1 + (y + 1) * scale_y;
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
			
			for (int y = 0; y < ysize; y++)
			{
				int y1 = qRound(((y - 0.5) / (ysize - 1)) * bounds.height() + bounds.y());
				int y2 = qRound(((y + 0.5) / (ysize - 1)) * bounds.height() + bounds.y());
				
				if (y1 < bounds_y1) y1 = bounds_y1;
				if (y2 > bounds_y2) y2 = bounds_y2;
				
				// Flip our display, since our coordinate system is flipped relative to our buffer
				double *values_row = values + ((ysize - 1) - y) * xsize;
				
				for (int x = 0; x < xsize; x++)
				{
					double value = *(values_row + x);
					int x1 = qRound(((x - 0.5) / (xsize - 1)) * bounds.width() + bounds.x());
					int x2 = qRound(((x + 0.5) / (xsize - 1)) * bounds.width() + bounds.x());
					
					if (x1 < bounds_x1) x1 = bounds_x1;
					if (x2 > bounds_x2) x2 = bounds_x2;
					
					float value_fraction = static_cast<float>((value - background_map->min_value_) / (background_map->max_value_ - background_map->min_value_));
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
}

void QtSLiMIndividualsWidget::chooseDefaultBackgroundSettingsForSubpopulation(PopulationViewBackgroundSettings *background, SpatialMap **returnMap, Subpopulation *subpop)
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
		*returnMap = background_map;
	}
}

void QtSLiMIndividualsWidget::drawSpatialBackgroundInBoundsForSubpopulation(QRect bounds, Subpopulation * subpop, int /* dimensionality */)
{
    auto backgroundIter = backgroundSettings.find(subpop->subpopulation_id_);
	PopulationViewBackgroundSettings background;
	SpatialMap *background_map = nullptr;
	
	if (backgroundIter == backgroundSettings.end())
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
		_drawBackgroundSpatialMap(background_map, bounds, subpop);
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

void QtSLiMIndividualsWidget::drawSpatialIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds, int dimensionality)
{
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
	double scalingFactor = 0.8; // controller->fitnessColorScale;
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	double bounds_x0 = subpop->bounds_x0_, bounds_x1 = subpop->bounds_x1_;
	double bounds_y0 = subpop->bounds_y0_, bounds_y1 = subpop->bounds_y1_;
	double bounds_x_size = bounds_x1 - bounds_x0, bounds_y_size = bounds_y1 - bounds_y0;
	double subpopFitnessScaling = subpop->last_fitness_scaling_;
	
	if ((subpopFitnessScaling <= 0.0) || !std::isfinite(subpopFitnessScaling))
		subpopFitnessScaling = 1.0;
	
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
		srandom(static_cast<unsigned int>(controller->sim->Generation()));
	
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
        srandom(static_cast<unsigned int>(controller->sim->Generation()));
	
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
		
		if (Individual::s_any_individual_color_set_ && !individual.color_.empty())
		{
			colorRed = individual.color_red_;
			colorGreen = individual.color_green_;
			colorBlue = individual.color_blue_;
		}
		else
		{
			// use individual trait values to determine color; we used fitness values cached in UpdateFitness, so we don't have to call out to fitness callbacks
			// we normalize fitness values with subpopFitnessScaling so individual fitness, unscaled by subpopulation fitness, is used for coloring
			double fitness = individual.cached_fitness_UNSAFE_;
			
			if (!std::isnan(fitness))
				RGBForFitness(fitness / subpopFitnessScaling, &colorRed, &colorGreen, &colorBlue, scalingFactor);
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

void QtSLiMIndividualsWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
	SLiMSim *sim = controller->sim;
	bool disableAll = false;
	
	// When the simulation is not valid and initialized, the context menu is disabled
	if (controller->invalidSimulation() || !sim || !sim->simulation_valid_ || (sim->generation_ < 1))
		disableAll = true;
	
    QMenu contextMenu("population_menu", this);
    
    QAction *displayNonSpatial = contextMenu.addAction("Display Individuals (non-spatial)");
    displayNonSpatial->setData(0);
    displayNonSpatial->setCheckable(true);
    displayNonSpatial->setEnabled(!disableAll);
    
    QAction *displaySpatial = contextMenu.addAction("Display Individuals (spatial)");
    displaySpatial->setData(1);
    displaySpatial->setCheckable(true);
    displaySpatial->setEnabled(!disableAll && (sim->spatial_dimensionality_ > 0));
    
    // Check the item corresponding to our current display preference, if any
    if (!disableAll)
        (displayMode == 0 ? displayNonSpatial : displaySpatial)->setChecked(true);
    
    QActionGroup *displayGroup = new QActionGroup(this);    // On Linux this provides a radio-button-group appearance
    displayGroup->addAction(displayNonSpatial);
    displayGroup->addAction(displaySpatial);
    
	// If we're displaying spatially, find the subpop that was clicked in
    Subpopulation *subpopForEvent = nullptr;
    
	if (!disableAll && (sim->spatial_dimensionality_ > 0) && (displayMode == 1))
	{
		std::vector<Subpopulation*> selectedSubpopulations = controller->selectedSubpopulations();
        QPoint viewPoint = event->pos();
		
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
    
    // If a spatial subpop was clicked in, provide background options (colors, spatial maps)
    if (subpopForEvent)
    {
        contextMenu.addSeparator();
        
        QAction *headerAction = contextMenu.addAction(QString("Background for p%1:").arg(subpopForEvent->subpopulation_id_));
        headerAction->setData(-1);
        headerAction->setEnabled(false);
        
        // check the menu item for the preferred display option; if we're in auto mode, don't check anything (could put a dash by the currently chosen style?)
        auto backgroundIter = backgroundSettings.find(subpopForEvent->subpopulation_id_);
        PopulationViewBackgroundSettings *background = ((backgroundIter == backgroundSettings.end()) ? nullptr : &backgroundIter->second);
        int backgroundType = (background ? background->backgroundType : -1);
        
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
        
        // look for spatial maps to offer as choices; need to scan the defined maps for the ones we can use
        SpatialMapMap &spatial_maps = subpopForEvent->spatial_maps_;
        
        for (const auto &map_pair : spatial_maps)
        {
            SpatialMap *map = map_pair.second;
            
            // We used to display only maps with a color scale; now we just make up a color scale if none is given.  Only
            // "x", "y", and "xy" maps are considered displayable; We can't display a z coordinate, and we can't display
            // even the x or y portion of "xz", "yz", and "xyz" maps since we don't know which z-slice to use.
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
            
            QAction *mapAction = contextMenu.addAction(menuItemTitle);
            mapAction->setData(mapName);
            mapAction->setCheckable(true);
            mapAction->setChecked((backgroundType == 3) && (mapName == QString::fromStdString(background->spatialMapName)));
            mapAction->setEnabled(!disableAll && displayable);
            
            backgroundGroup->addAction(mapAction);
        }
    }
	
    // Run the context menu synchronously
    QAction *action = contextMenu.exec(event->globalPos());
    
    // Act upon the chosen action; we just do it right here instead of dealing with slots
    if (action)
    {
        if ((action == displayNonSpatial) || (action == displaySpatial))
        {
            // - (IBAction)setDisplayStyle:(id)sender
            int newDisplayMode = action->data().toInt();
            
            if (newDisplayMode != displayMode)
            {
                displayMode = newDisplayMode;
                update();
            }
        }
        else if (subpopForEvent)
        {
            // - (IBAction)setDisplayBackground:(id)sender
            int newDisplayBackground;
            auto backgroundIter = backgroundSettings.find(subpopForEvent->subpopulation_id_);
            PopulationViewBackgroundSettings *background = ((backgroundIter == backgroundSettings.end()) ? nullptr : &backgroundIter->second);
            std::string mapName;
            
            // If the user has selected a spatial map, extract its name
            if (static_cast<QMetaType::Type>(action->data().type()) == QMetaType::QString)  // for some reason this method's return type is apparently misdeclared; see the doc
            {
                mapName = action->data().toString().toStdString();
                
                if (mapName.length() == 0)
                    return;
                
                newDisplayBackground = 3;
            }
            else
            {
                newDisplayBackground = action->data().toInt() - 10;
            }
            
            // Update the existing background entry, or make a new entry
            if (background)
            {
                background->backgroundType = newDisplayBackground;
                background->spatialMapName = mapName;
                update();
            }
            else
            {
                backgroundSettings.insert(std::pair<const int, PopulationViewBackgroundSettings>(subpopForEvent->subpopulation_id_, PopulationViewBackgroundSettings{newDisplayBackground, mapName}));
                update();
            }
        }
    }
}




































