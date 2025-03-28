//
//  QtSLiMIndividualsWidget_QT.cpp
//  SLiM
//
//  Created by Ben Haller on 8/25/2024.
//  Copyright (c) 2024-2025 Philipp Messer.  All rights reserved.
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
#include "QtSLiMExtras.h"
#include "QtSLiMOpenGL_Emulation.h"

#include <QDebug>

#include <map>
#include <utility>
#include <string>
#include <vector>


//
//  Qt-based drawing; maintain this in parallel with the OpenGL-based drawing!
//

void QtSLiMIndividualsWidget::qtDrawViewFrameInBounds(QRect bounds, QPainter &painter)
{
    bool inDarkMode = QtSLiMInDarkMode();
    
    if (inDarkMode)
        QtSLiMFrameRect(bounds, QtSLiMColorWithWhite(0.067, 1.0), painter);
    else
        QtSLiMFrameRect(bounds, QtSLiMColorWithWhite(0.77, 1.0), painter);
}

void QtSLiMIndividualsWidget::qtDrawIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds, int squareSize, QPainter &painter)
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
        
        int individualArrayIndex;
        
        // Set up to draw rects
        SLIM_GL_PREPARE();
        
        for (individualArrayIndex = 0; individualArrayIndex < subpopSize; ++individualArrayIndex)
        {
            // Figure out the rect to draw in; note we now use individualArrayIndex here, because the hit-testing code doesn't have an easy way to calculate the displayed individual index...
            float left = static_cast<float>(individualArea.left() + (individualArrayIndex % viewColumns) * (squareSize + squareSpacing));
            float top = static_cast<float>(individualArea.top() + (individualArrayIndex / viewColumns) * (squareSize + squareSpacing));
            float right = left + squareSize;
            float bottom = top + squareSize;
            
            SLIM_GL_PUSHRECT();
            
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
            
            SLIM_GL_PUSHRECT_COLORS();
            SLIM_GL_CHECKBUFFERS_NORECT();
        }
        
        // Draw any leftovers
        SLIM_GL_FINISH();
    }
    else
    {
        // This is what we do if we cannot display a subpopulation because there are too many individuals in it to display
        QRect insetBounds = bounds.adjusted(1, 1, -1, -1);
        
        painter.fillRect(insetBounds, QtSLiMColorWithRGB(0.9, 0.9, 1.0, 1.0));
    }
}

void QtSLiMIndividualsWidget::_qtDrawBackgroundSpatialMap(SpatialMap *background_map, QRect bounds, Subpopulation *subpop, bool showGridPoints, QPainter &painter)
{
    // We have a spatial map with a color map, so use it to draw the background
	int bounds_x1 = bounds.x();
	int bounds_y1 = bounds.y();
	int bounds_x2 = bounds.x() + bounds.width();
	int bounds_y2 = bounds.y() + bounds.height();
	
    {
        // Set up to draw rects
        SLIM_GL_PREPARE();
        
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
                    
                    float left, right, top, bottom;
                    
                    if (spatiality_is_x)
                    {
                        left = xc;
                        right = xc + 1;
                        top = bounds_y1;
                        bottom = bounds_y2;
                    }
                    else
                    {
                        top = (max_coord - 1) - xc + min_coord;	// flip for y, to use Cartesian coordinates
                        bottom = top + 1;
                        left = bounds_x1;
                        right = bounds_x2;
                    }
                    
                    float rgb[3];
                    background_map->ColorForValue(value, rgb);
                    
                    float colorRed = rgb[0];
                    float colorGreen = rgb[1];
                    float colorBlue = rgb[2];
                    float colorAlpha = 1.0;
                    
                    SLIM_GL_PUSHRECT();
                    SLIM_GL_PUSHRECT_COLORS();
                    SLIM_GL_CHECKBUFFERS_NORECT();
                }
            }
            else
            {
                // No interpolation, so we can draw whole grid blocks
                for (int xc = 0; xc < xsize; xc++)
                {
                    double value = (spatiality_is_x ? values[xc] : values[(xsize - 1) - xc]);	// flip for y, to use Cartesian coordinates
                    float left, right, top, bottom;
                    
                    if (spatiality_is_x)
                    {
                        left = qRound(((xc - 0.5) / (xsize - 1)) * bounds.width() + bounds.x());
                        right = qRound(((xc + 0.5) / (xsize - 1)) * bounds.width() + bounds.x());
                        
                        if (left < bounds_x1) left = bounds_x1;
                        if (right > bounds_x2) right = bounds_x2;
                        
                        top = bounds_y1;
                        bottom = bounds_y2;
                    }
                    else
                    {
                        top = qRound(((xc - 0.5) / (xsize - 1)) * bounds.height() + bounds.y());
                        bottom = qRound(((xc + 0.5) / (xsize - 1)) * bounds.height() + bounds.y());
                        
                        if (top < bounds_y1) top = bounds_y1;
                        if (bottom > bounds_y2) bottom = bounds_y2;
                        
                        left = bounds_x1;
                        right = bounds_x2;
                    }
                    
                    float rgb[3];
                    
                    background_map->ColorForValue(value, rgb);
                    
                    float colorRed = rgb[0];
                    float colorGreen = rgb[1];
                    float colorBlue = rgb[2];
                    float colorAlpha = 1.0;
                    
                    SLIM_GL_PUSHRECT();
                    SLIM_GL_PUSHRECT_COLORS();
                    SLIM_GL_CHECKBUFFERS_NORECT();
                }
            }
        }
        else // if (background_map->spatiality_ == 2)
        {
            // This is the spatiality "xy" case; it is the only 2D spatiality for which SLiMgui will draw
            
            // First, cache the display buffer if needed.  If this succeeds, we'll use it.
            // It should always succeed, so the tile-drawing code below is dead code, kept for parallelism with the 1D case.
            cacheDisplayBufferForMapForSubpopulation(background_map, subpop, /* flipped */ true);
            
            const uint8_t *display_buf = background_map->display_buffer_;
            
            if (display_buf)
            {
                // Use a cached display buffer to draw.
                int buf_width = background_map->buffer_width_;
                int buf_height = background_map->buffer_height_;
                QImage bufferImage(display_buf, buf_width, buf_height, buf_width * 3, QImage::Format_RGB888);
                
                painter.drawImage(bounds, bufferImage);
            }
        }
        
        // Draw any leftovers
        SLIM_GL_FINISH();
    }
    
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
            SLIM_GL_PREPARE();
            
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
                        
                        SLIM_GL_PUSHRECT();
                        
                        float colorRed;
                        float colorGreen;
                        float colorBlue;
                        float colorAlpha;
                        
                        if (pass == 0)
                        {
                            colorRed = 1.0;
                            colorGreen = 0.25;
                            colorBlue = 0.25;
                            colorAlpha = 1.0;
                        }
                        else
                        {
                            // look up the map's color at this grid point
                            float rgb[3];
                            double value = values[x + y * xsize];
                            
                            background_map->ColorForValue(value, rgb);
                            
                            colorRed = rgb[0];
                            colorGreen = rgb[1];
                            colorBlue = rgb[2];
                            colorAlpha = 1.0;
                        }
                        
                        SLIM_GL_PUSHRECT_COLORS();
                        SLIM_GL_CHECKBUFFERS_NORECT();
                    }
                }
            }
            
            // Draw any leftovers
            SLIM_GL_FINISH();
        }
    }
}

void QtSLiMIndividualsWidget::qtDrawSpatialBackgroundInBoundsForSubpopulation(QRect bounds, Subpopulation * subpop, int /* dimensionality */, QPainter &painter)
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
		_qtDrawBackgroundSpatialMap(background_map, bounds, subpop, background.showGridPoints, painter);
	}
	else
	{
		// No background map, so just clear to the preferred background color
		int backgroundColor = background.backgroundType;
		
		if (backgroundColor == 0)
            painter.fillRect(bounds, Qt::black);
		else if (backgroundColor == 1)
            painter.fillRect(bounds, QtSLiMColorWithWhite(0.3, 1.0));
		else if (backgroundColor == 2)
            painter.fillRect(bounds, Qt::white);
		else
            painter.fillRect(bounds, Qt::black);
	}
}

void QtSLiMIndividualsWidget::qtDrawSpatialIndividualsFromSubpopulationInArea(Subpopulation *subpop, QRect bounds, int dimensionality, float *forceColor, QPainter &painter)
{
    QtSLiMWindow *controller = dynamic_cast<QtSLiMWindow *>(window());
	double scalingFactor = 0.8; // used to be controller->fitnessColorScale;
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	double bounds_x0 = subpop->bounds_x0_, bounds_x1 = subpop->bounds_x1_;
	double bounds_y0 = subpop->bounds_y0_, bounds_y1 = subpop->bounds_y1_;
	double bounds_x_size = bounds_x1 - bounds_x0, bounds_y_size = bounds_y1 - bounds_y0;
	
	QRect individualArea(bounds.x(), bounds.y(), bounds.width() - 1, bounds.height() - 1);
	
	int individualArrayIndex;
	
	// Set up to draw rects
    SLIM_GL_PREPARE();
	
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
        
        float colorRed = 0.25;
        float colorGreen = 0.25;
        float colorBlue = 0.25;
        float colorAlpha = 1.0;
        
        SLIM_GL_PUSHRECT();
        SLIM_GL_PUSHRECT_COLORS();
        SLIM_GL_CHECKBUFFERS_NORECT();
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
        
        SLIM_GL_PUSHRECT();
		
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
        
        SLIM_GL_PUSHRECT_COLORS();
        SLIM_GL_CHECKBUFFERS_NORECT();
	}
	
	// Draw any leftovers
    SLIM_GL_FINISH();
}








































