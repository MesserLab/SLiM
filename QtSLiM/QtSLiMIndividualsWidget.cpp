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

#include <QDebug>


// OpenGL constants
static const int kMaxGLRects = 2000;				// 2000 rects
static const int kMaxVertices = kMaxGLRects * 4;	// 4 vertices each


QtSLiMIndividualsWidget::QtSLiMIndividualsWidget(QWidget *parent, Qt::WindowFlags f) : QOpenGLWidget(parent, f)
{
    displayMode = -1;	// don't know yet whether the model is spatial or not, which will determine our initial choice
	
//	// Default values that will appear the first time the options sheet runs
//	binCount = 20;
//	fitnessMin = 0.0;
//	fitnessMax = 2.0;
}

QtSLiMIndividualsWidget::~QtSLiMIndividualsWidget()
{
    //[self setDisplayOptionsSheet:nil];
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
				
				//else	// displayMode == 0
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
	else if (displayMode == 2)
	{
		canDisplayAllIndividuals = true;
	}
	else if (displayMode == 3)
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
//    else if (displayMode == 1)
//	{
//		// spatial display adaptively finds the layout the maximizes the pixel area covered, and cannot fail
//		int64_t bestTotalExtent = 0;
		
//		for (int rowCount = 1; rowCount <= selectedSubpopCount; ++rowCount)
//		{
//			int columnCount = (int)ceil(selectedSubpopCount / (double)rowCount);
//			int interBoxSpace = 5;
//			int totalInterboxHeight = interBoxSpace * (rowCount - 1);
//			int totalInterboxWidth = interBoxSpace * (columnCount - 1);
//			double boxWidth = (bounds.size.width - totalInterboxWidth) / (double)columnCount;
//			double boxHeight = (bounds.size.height - totalInterboxHeight) / (double)rowCount;
//			std::map<slim_objectid_t, NSRect> candidateTiles;
//			int64_t totalExtent = 0;
			
//			for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
//			{
//				int columnIndex = subpopIndex % columnCount;
//				int rowIndex = subpopIndex / columnCount;
//				double boxLeft = round(bounds.origin.x + columnIndex * (interBoxSpace + boxWidth));
//				double boxRight = round(bounds.origin.x + columnIndex * (interBoxSpace + boxWidth) + boxWidth);
//				double boxTop = round(bounds.origin.y + rowIndex * (interBoxSpace + boxHeight));
//				double boxBottom = round(bounds.origin.y + rowIndex * (interBoxSpace + boxHeight) + boxHeight);
//				NSRect boxBounds = NSMakeRect(boxLeft, boxTop, boxRight - boxLeft, boxBottom - boxTop);
//				Subpopulation *subpop = selectedSubpopulations[subpopIndex];
				
//				candidateTiles.insert(std::pair<slim_objectid_t, NSRect>(subpop->subpopulation_id_, boxBounds));
				
//				// find out what pixel area actually gets used by this box, and use that to choose the optimal layout
//				NSRect spatialDisplayBounds = [self spatialDisplayBoundsForSubpopulation:subpop tileBounds:boxBounds];
//				int64_t extent = (int64_t)spatialDisplayBounds.size.width * (int64_t)spatialDisplayBounds.size.height;
				
//				totalExtent += extent;
//			}
			
//			if (totalExtent > bestTotalExtent)
//			{
//				bestTotalExtent = totalExtent;
//				std::swap(subpopTiles, candidateTiles);
//			}
//		}
		
//		canDisplayAllIndividuals = true;
//	}
	else	// displayMode == 0
	{
		// non-spatial display always uses vertically stacked maximum-width tiles, but can fail if they are too small
		int interBoxSpace = 5;
		int totalInterbox = interBoxSpace * (selectedSubpopCount - 1);
		double boxHeight = (bounds.height() - totalInterbox) / static_cast<double>(selectedSubpopCount);
		
        canDisplayAllIndividuals = true;
        
		for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
		{
			int boxTop = static_cast<int>(round(bounds.top() + subpopIndex * (interBoxSpace + boxHeight)));
			int boxBottom = static_cast<int>(round(bounds.top() + subpopIndex * (interBoxSpace + boxHeight) + boxHeight));
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
		
		static float *glArrayVertices = nullptr;
		static float *glArrayColors = nullptr;
		int individualArrayIndex, displayListIndex;
		float *vertices = nullptr, *colors = nullptr;
		
		// Set up the vertex and color arrays
		if (!glArrayVertices)
			glArrayVertices = static_cast<float *>(malloc(kMaxVertices * 2 * sizeof(float)));		// 2 floats per vertex, kMaxVertices vertices
		
		if (!glArrayColors)
			glArrayColors = static_cast<float *>(malloc(kMaxVertices * 4 * sizeof(float)));		// 4 floats per color, kMaxVertices colors
		
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






































