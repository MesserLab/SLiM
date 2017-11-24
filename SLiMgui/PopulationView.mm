//
//  PopulationView.m
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
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


#import "PopulationView.h"
#import "SLiMWindowController.h"
#import "CocoaExtra.h"
#import "ScriptMod.h"		// we use ScriptMod's validation tools

#import <OpenGL/OpenGL.h>
#include <OpenGL/glu.h>
#include <GLKit/GLKMatrix4.h>


static const int kMaxGLRects = 2000;				// 2000 rects
static const int kMaxVertices = kMaxGLRects * 4;	// 4 vertices each


@implementation PopulationView

- (void)initializeDisplayOptions
{
	displayMode = 0;
	
	// Default values that will appear the first time the options sheet runs
	binCount = 20;
	fitnessMin = 0.0;
	fitnessMax = 2.0;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
	if (self = [super initWithCoder:coder])
	{
		[self initializeDisplayOptions];
	}
	
	return self;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
	if (self = [super initWithFrame:frameRect])
	{
		[self initializeDisplayOptions];
	}
	
	return self;
}

- (void)dealloc
{
	//NSLog(@"[PopulationView dealloc]");
	[self setDisplayOptionsSheet:nil];
	
	[super dealloc];
}

// Called after being summoned from a NIB/XIB.
- (void)prepareOpenGL
{
}

// Adjust the OpenGL viewport in response to window resize
- (void)reshape
{
}

- (void)drawViewFrameInBounds:(NSRect)bounds
{
	int ox = (int)bounds.origin.x, oy = (int)bounds.origin.y;
	
	glColor3f(0.77f, 0.77f, 0.77f);
	glRecti(ox, oy, ox + 1, oy + (int)bounds.size.height);
	glRecti(ox + 1, oy, ox + (int)bounds.size.width - 1, oy + 1);
	glRecti(ox + (int)bounds.size.width - 1, oy, ox + (int)bounds.size.width, oy + (int)bounds.size.height);
	glRecti(ox + 1, oy + (int)bounds.size.height - 1, ox + (int)bounds.size.width - 1, oy + (int)bounds.size.height);
}

- (void)drawIndividualsFromSubpopulation:(Subpopulation *)subpop inArea:(NSRect)bounds
{
	//
	//	NOTE this code is parallel to the code in canDisplayIndividualsFromSubpopulation:inArea: and should be maintained in parallel
	//
	
	SLiMWindowController *controller = [[self window] windowController];
	double scalingFactor = controller->fitnessColorScale;
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	double *subpop_fitness = subpop->cached_parental_fitness_;
	BOOL useCachedFitness = (subpop->cached_fitness_size_ == subpopSize);	// needs to have the right number of entries, otherwise we punt
	
	int squareSize, viewColumns = 0, viewRows = 0;
	
	// first figure out the biggest square size that will allow us to display the whole subpopulation
	for (squareSize = 20; squareSize > 1; --squareSize)
	{
		viewColumns = (int)floor((bounds.size.width - 3) / squareSize);
		viewRows = (int)floor((bounds.size.height - 3) / squareSize);
		
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
		
		double excessSpaceX = bounds.size.width - ((squareSize + squareSpacing) * viewColumns - squareSpacing);
		double excessSpaceY = bounds.size.height - ((squareSize + squareSpacing) * viewRows - squareSpacing);
		int offsetX = (int)floor(excessSpaceX / 2.0);
		int offsetY = (int)floor(excessSpaceY / 2.0);
		
		// If we have an empty row at the bottom, then we can use the same value for offsetY as for offsetX, for symmetry
		if ((subpopSize - 1) / viewColumns < viewRows - 1)
			offsetY = offsetX;
		
		NSRect individualArea = NSMakeRect(bounds.origin.x + offsetX, bounds.origin.y + offsetY, bounds.size.width - offsetX, bounds.size.height - offsetY);
		
		static float *glArrayVertices = nil;
		static float *glArrayColors = nil;
		int individualArrayIndex, displayListIndex;
		float *vertices = NULL, *colors = NULL;
		
		// Set up the vertex and color arrays
		if (!glArrayVertices)
			glArrayVertices = (float *)malloc(kMaxVertices * 2 * sizeof(float));		// 2 floats per vertex, kMaxVertices vertices
		
		if (!glArrayColors)
			glArrayColors = (float *)malloc(kMaxVertices * 4 * sizeof(float));		// 4 floats per color, kMaxVertices colors
		
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
			float left = (float)(individualArea.origin.x + (individualArrayIndex % viewColumns) * (squareSize + squareSpacing));
			float top = (float)(individualArea.origin.y + (individualArrayIndex / viewColumns) * (squareSize + squareSpacing));
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
			
			float colorRed = 0.0, colorGreen = 0.0, colorBlue = 0.0, colorAlpha = 1.0;
			
			if (gSLiM_Individual_custom_colors)
			{
				Individual &individual = subpop->parent_individuals_[individualArrayIndex];
				
				if (!individual.color_.empty())
				{
					colorRed = individual.color_red_;
					colorGreen = individual.color_green_;
					colorBlue = individual.color_blue_;
				}
				else
				{
					// use individual trait values to determine color; we used fitness values cached in UpdateFitness, so we don't have to call out to fitness callbacks
					double fitness = (useCachedFitness ? subpop_fitness[individualArrayIndex] : 1.0);
					
					RGBForFitness(fitness, &colorRed, &colorGreen, &colorBlue, scalingFactor);
				}
			}
			else
			{
				// use individual trait values to determine color; we used fitness values cached in UpdateFitness, so we don't have to call out to fitness callbacks
				double fitness = (useCachedFitness ? subpop_fitness[individualArrayIndex] : 1.0);
				
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
		
		int ox = (int)bounds.origin.x, oy = (int)bounds.origin.y;
		
		glRecti(ox + 1, oy + 1, ox + (int)bounds.size.width - 1, oy + (int)bounds.size.height - 1);
	}
}

#define SLIM_MAX_HISTOGRAM_BINS		100

- (void)drawFitnessLinePlotForSubpopulation:(Subpopulation *)subpop inArea:(NSRect)bounds
{
	SLiMWindowController *controller = [[self window] windowController];
	double scalingFactor = controller->fitnessColorScale;
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	double *subpop_fitness = subpop->cached_parental_fitness_;
	
	if (!subpop_fitness || (subpop->cached_fitness_size_ != subpopSize))
		return;
	
	static float *glArrayVertices = nil;
	static float *glArrayColors = nil;
	int displayListIndex = 0;
	float *vertices = NULL, *colors = NULL;
	
	// Set up the vertex and color arrays
	if (!glArrayVertices)
		glArrayVertices = (float *)malloc(SLIM_MAX_HISTOGRAM_BINS * 2 * sizeof(float));		// 2 floats per vertex, SLIM_MAX_HISTOGRAM_BINS vertices
	
	if (!glArrayColors)
		glArrayColors = (float *)malloc(SLIM_MAX_HISTOGRAM_BINS * 4 * sizeof(float));		// 4 floats per color, SLIM_MAX_HISTOGRAM_BINS colors
	
	// Set up to draw lines
	vertices = glArrayVertices;
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, glArrayVertices);
	
	colors = glArrayColors;
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_FLOAT, 0, glArrayColors);
	
	// first tabulate fitness values
	int numberOfBins = binCount;
	slim_popsize_t* binCounts = (slim_popsize_t *)calloc(numberOfBins, sizeof(slim_popsize_t));
	
	for (int individualIndex = 0; individualIndex < subpopSize; ++individualIndex)
	{
		double fitness = subpop_fitness[individualIndex];
		int binIndex = (int)floor(((fitness - fitnessMin) / (fitnessMax - fitnessMin)) * numberOfBins);
		
		if (binIndex < 0) binIndex = 0;
		if (binIndex >= numberOfBins) binIndex = numberOfBins - 1;
		
		binCounts[binIndex]++;
	}
	
	NSRect histogramArea = NSMakeRect(bounds.origin.x + 5, bounds.origin.y + 5, bounds.size.width - 10, bounds.size.height - 10);
	
	// then plot the tabulated values as a line
	for (int binIndex = 0; binIndex < numberOfBins; ++binIndex)
	{
		*(vertices++) = (float)(histogramArea.origin.x + (binIndex / (double)(numberOfBins - 1)) * histogramArea.size.width);
		*(vertices++) = (float)(histogramArea.origin.y + histogramArea.size.height - (binCounts[binIndex] / (double)subpopSize) * histogramArea.size.height);
		
		float colorRed = 0.0, colorGreen = 0.0, colorBlue = 0.0, colorAlpha = 1.0;
		double fitness = ((binIndex + 0.5) / numberOfBins) * (fitnessMax - fitnessMin) + fitnessMin;
		
		RGBForFitness(fitness, &colorRed, &colorGreen, &colorBlue, scalingFactor);
		
		*(colors++) = colorRed;
		*(colors++) = colorGreen;
		*(colors++) = colorBlue;
		*(colors++) = colorAlpha;
		
		displayListIndex++;
	}
	
	// Draw our line
	if (displayListIndex)
	{
		glLineWidth(2.0);
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		glDrawArrays(GL_LINE_STRIP, 0, 1 * displayListIndex);
		
		glLineWidth(1.0);
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
	}
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	
	free(binCounts);
}

- (void)drawFitnessBarPlotForSubpopulations:(std::vector<Subpopulation*> &)selectedSubpopulations inArea:(NSRect)bounds
{
	SLiMWindowController *controller = [[self window] windowController];
	double scalingFactor = controller->fitnessColorScale;
	int selectedSubpopCount = (int)(selectedSubpopulations.size());
	
	// first tabulate fitness values
	int numberOfBins = binCount;
	slim_popsize_t* binCounts = (slim_popsize_t *)calloc(numberOfBins, sizeof(slim_popsize_t));
	slim_popsize_t binTotal = 0;
	
	for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
	{
		Subpopulation *subpop = selectedSubpopulations[subpopIndex];
		slim_popsize_t subpopSize = subpop->parent_subpop_size_;
		double *subpop_fitness = subpop->cached_parental_fitness_;
		
		if (!subpop_fitness || (subpop->cached_fitness_size_ != subpopSize))
			continue;
		
		for (int individualIndex = 0; individualIndex < subpopSize; ++individualIndex)
		{
			double fitness = subpop_fitness[individualIndex];
			int binIndex = (int)floor(((fitness - fitnessMin) / (fitnessMax - fitnessMin)) * numberOfBins);
			
			if (binIndex < 0) binIndex = 0;
			if (binIndex >= numberOfBins) binIndex = numberOfBins - 1;
			
			binCounts[binIndex]++;
		}
		
		binTotal += subpopSize;
	}
	
	// then draw the barplot
	static float *glArrayVertices = nil;
	static float *glArrayColors = nil;
	int displayListIndex;
	float *vertices = NULL, *colors = NULL;
	const int numVertices = SLIM_MAX_HISTOGRAM_BINS * 4 * 2;	// four corners for each rect, 2 rects per bar for frame and fill
	
	// Set up the vertex and color arrays
	if (!glArrayVertices)
		glArrayVertices = (float *)malloc(numVertices * 2 * sizeof(float));		// 2 floats per vertex, numVertices vertices
	
	if (!glArrayColors)
		glArrayColors = (float *)malloc(numVertices * 4 * sizeof(float));		// 4 floats per color, numVertices colors
	
	// Set up to draw rects
	displayListIndex = 0;
	
	vertices = glArrayVertices;
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, glArrayVertices);
	
	colors = glArrayColors;
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_FLOAT, 0, glArrayColors);
	
	NSRect histogramArea = NSMakeRect(bounds.origin.x + 4, bounds.origin.y + 6, bounds.size.width - 8, bounds.size.height - 12);
	
	for (int binIndex = 0; binIndex < numberOfBins; ++binIndex)
	{
		// Figure out the rect for the bar within histogramArea
		float left = (float)(histogramArea.origin.x + (binIndex / (double)numberOfBins) * histogramArea.size.width);
		float top = (float)(histogramArea.origin.y + histogramArea.size.height - (binCounts[binIndex] / (double)binTotal) * histogramArea.size.height);
		float right = (float)(histogramArea.origin.x + ((binIndex + 1) / (double)numberOfBins) * histogramArea.size.width);
		float bottom = (float)(histogramArea.origin.y + histogramArea.size.height);
		
		// First draw a rect for the frame of the bar
		*(vertices++) = left + 1;
		*(vertices++) = top - 1;
		*(vertices++) = left + 1;
		*(vertices++) = bottom + 1;
		*(vertices++) = right - 1;
		*(vertices++) = bottom + 1;
		*(vertices++) = right - 1;
		*(vertices++) = top - 1;
		
		for (int j = 0; j < 4; ++j)
		{
			*(colors++) = 1.0;
			*(colors++) = 1.0;
			*(colors++) = 1.0;
			*(colors++) = 1.0;
		}
		
		displayListIndex++;
		
		// Then draw a rect for the interior of the bar
		*(vertices++) = left + 2;
		*(vertices++) = top;
		*(vertices++) = left + 2;
		*(vertices++) = bottom;
		*(vertices++) = right - 2;
		*(vertices++) = bottom;
		*(vertices++) = right - 2;
		*(vertices++) = top;
		
		float colorRed = 0.0, colorGreen = 0.0, colorBlue = 0.0, colorAlpha = 1.0;
		double fitness = ((binIndex + 0.5) / numberOfBins) * (fitnessMax - fitnessMin) + fitnessMin;
		
		RGBForFitness(fitness, &colorRed, &colorGreen, &colorBlue, scalingFactor);
		
		for (int j = 0; j < 4; ++j)
		{
			*(colors++) = colorRed;
			*(colors++) = colorGreen;
			*(colors++) = colorBlue;
			*(colors++) = colorAlpha;
		}
		
		displayListIndex++;
	}
	
	// Draw all the bars
	if (displayListIndex)
		glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	
	free(binCounts);
}

- (void)cacheDisplayBufferForMap:(SpatialMap *)background_map subpopulation:(Subpopulation *)subpop
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
	
	NSRect full_bounds = NSInsetRect([self bounds], 1, 1);
	int max_height = (int)full_bounds.size.height;
	double bounds_x0 = subpop->bounds_x0_, bounds_x1 = subpop->bounds_x1_;
	double bounds_y0 = subpop->bounds_y0_, bounds_y1 = subpop->bounds_y1_;
	double bounds_x_size = bounds_x1 - bounds_x0, bounds_y_size = bounds_y1 - bounds_y0;
	double subpopAspect = bounds_x_size / bounds_y_size;
	int max_width = (int)round(max_height * subpopAspect);
	
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
		uint8_t *display_buf = (uint8_t *)malloc(max_width * max_height * 3 * sizeof(uint8_t));
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
					int x1_map = (int)floor(x_map);
					int y1_map = (int)floor(y_map);
					int x2_map = (int)ceil(x_map);
					int y2_map = (int)ceil(y_map);
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
					int x_map = (int)round(x_fraction * (xsize - 1));
					int y_map = (int)round(y_fraction * (ysize - 1));
					
					value = values[x_map + y_map * xsize];
				}
				
				// Given the (interpolated?) value, look up the color, interpolating if necessary
				double rgb[3];
				
				background_map->ColorForValue(value, rgb);
				
				// Write the color values to the buffer
				*(buf_ptr++) = (uint8_t)round(rgb[0] * 255.0);
				*(buf_ptr++) = (uint8_t)round(rgb[1] * 255.0);
				*(buf_ptr++) = (uint8_t)round(rgb[2] * 255.0);
			}
		}
	}
}

- (void)drawSpatialBackgroundInBounds:(NSRect)bounds forSubpopulation:(Subpopulation *)subpop dimensionality:(int)dimensionality
{
	SpatialMapMap &spatial_maps = subpop->spatial_maps_;
	SpatialMap *background_map = nullptr;
	
	for (const SpatialMapPair &map_pair : spatial_maps)
	{
		SpatialMap *map = map_pair.second;
		
		if (map->n_colors_ > 0)
		{
			// Having a color map means this spatial map wants to be displayed in SLiMgui.  We take the best one we find:
			// an "xy" map is preferred to an "x" or a "y" map.  Other map types are not allowed.  We can't display a z
			// coordinate, and we can't display even the x or y portion of "xz", "yz", and "xyz" maps since we don't know
			// which z-slice through the map to use.
			if ((map->spatiality_string_ == "x") || (map->spatiality_string_ == "y") || (map->spatiality_string_ == "xy"))
			{
				if (!background_map || (map->spatiality_ > background_map->spatiality_))
				{
					background_map = map;
					
					// if we found an xy map, we won't do better than that; otherwise, keep looking
					if (background_map->spatiality_ == 2)
						break;
				}
			}
		}
	}
	
	if (background_map)
	{
		// We have a spatial map with a color map, so use it to draw the background
		int bounds_x1 = (int)bounds.origin.x;
		int bounds_y1 = (int)bounds.origin.y;
		int bounds_x2 = (int)(bounds.origin.x + bounds.size.width);
		int bounds_y2 = (int)(bounds.origin.y + bounds.size.height);
		
		//glColor3f(0.0, 0.0, 0.0);
		//glRecti(bounds_x1, bounds_y1, bounds_x2, bounds_y2);
		
		static float *glArrayVertices = nil;
		static float *glArrayColors = nil;
		int displayListIndex;
		float *vertices = NULL, *colors = NULL;
		
		// Set up the vertex and color arrays
		if (!glArrayVertices)
			glArrayVertices = (float *)malloc(kMaxVertices * 2 * sizeof(float));		// 2 floats per vertex, kMaxVertices vertices
		
		if (!glArrayColors)
			glArrayColors = (float *)malloc(kMaxVertices * 4 * sizeof(float));		// 4 floats per color, kMaxVertices colors
		
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
					int x1_map = (int)floor(x_map);
					int x2_map = (int)ceil(x_map);
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
						x1 = (int)round(((x - 0.5) / (xsize - 1)) * bounds.size.width + bounds.origin.x);
						x2 = (int)round(((x + 0.5) / (xsize - 1)) * bounds.size.width + bounds.origin.x);
						
						if (x1 < bounds_x1) x1 = bounds_x1;
						if (x2 > bounds_x2) x2 = bounds_x2;
						
						y1 = bounds_y1;
						y2 = bounds_y2;
					}
					else
					{
						y1 = (int)round(((x - 0.5) / (xsize - 1)) * bounds.size.height + bounds.origin.y);
						y2 = (int)round(((x + 0.5) / (xsize - 1)) * bounds.size.height + bounds.origin.y);
						
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
			[self cacheDisplayBufferForMap:background_map subpopulation:subpop];
			
			uint8_t *display_buf = background_map->display_buffer_;
			
			if (display_buf)
			{
				// Use a cached display buffer to draw.
				int buf_width = background_map->buffer_width_;
				int buf_height = background_map->buffer_height_;
				bool display_full_size = (((int)bounds.size.width == buf_width) && ((int)bounds.size.height == buf_height));
				float scale_x = (float)(bounds.size.width / buf_width);
				float scale_y = (float)(bounds.size.height / buf_height);
				
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
					int y1 = (int)round(((y - 0.5) / (ysize - 1)) * bounds.size.height + bounds.origin.y);
					int y2 = (int)round(((y + 0.5) / (ysize - 1)) * bounds.size.height + bounds.origin.y);
					
					if (y1 < bounds_y1) y1 = bounds_y1;
					if (y2 > bounds_y2) y2 = bounds_y2;
					
					// Flip our display, since our coordinate system is flipped relative to our buffer
					double *values_row = values + ((ysize - 1) - y) * xsize;
					
					for (int x = 0; x < xsize; x++)
					{
						double value = *(values_row + x);
						int x1 = (int)round(((x - 0.5) / (xsize - 1)) * bounds.size.width + bounds.origin.x);
						int x2 = (int)round(((x + 0.5) / (xsize - 1)) * bounds.size.width + bounds.origin.x);
						
						if (x1 < bounds_x1) x1 = bounds_x1;
						if (x2 > bounds_x2) x2 = bounds_x2;
						
						float value_fraction = (float)((value - background_map->min_value_) / (background_map->max_value_ - background_map->min_value_));
						float color_index = value_fraction * (n_colors - 1);
						int color_index_1 = (int)floorf(color_index);
						int color_index_2 = (int)ceilf(color_index);
						
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
	else
	{
		// No background map, so just clear to black
		glColor3f(0.0, 0.0, 0.0);
		glRecti((int)bounds.origin.x, (int)bounds.origin.y, (int)(bounds.origin.x + bounds.size.width), (int)(bounds.origin.y + bounds.size.height));
	}
}

- (void)drawSpatialIndividualsFromSubpopulation:(Subpopulation *)subpop inArea:(NSRect)bounds dimensionality:(int)dimensionality
{
	SLiMWindowController *controller = [[self window] windowController];
	double scalingFactor = controller->fitnessColorScale;
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	double bounds_x0 = subpop->bounds_x0_, bounds_x1 = subpop->bounds_x1_;
	double bounds_y0 = subpop->bounds_y0_, bounds_y1 = subpop->bounds_y1_;
	double bounds_x_size = bounds_x1 - bounds_x0, bounds_y_size = bounds_y1 - bounds_y0;
	double *subpop_fitness = subpop->cached_parental_fitness_;
	BOOL useCachedFitness = (subpop->cached_fitness_size_ == subpopSize);	// needs to have the right number of entries, otherwise we punt
	
	NSRect individualArea = NSMakeRect(bounds.origin.x, bounds.origin.y, bounds.size.width - 1, bounds.size.height - 1);
	
	static float *glArrayVertices = nil;
	static float *glArrayColors = nil;
	int individualArrayIndex, displayListIndex;
	float *vertices = NULL, *colors = NULL;
	
	// Set up the vertex and color arrays
	if (!glArrayVertices)
		glArrayVertices = (float *)malloc(kMaxVertices * 2 * sizeof(float));		// 2 floats per vertex, kMaxVertices vertices
	
	if (!glArrayColors)
		glArrayColors = (float *)malloc(kMaxVertices * 4 * sizeof(float));		// 4 floats per color, kMaxVertices colors
	
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
		srandom(controller->sim->Generation());
	
	for (individualArrayIndex = 0; individualArrayIndex < subpopSize; ++individualArrayIndex)
	{
		// Figure out the rect to draw in; note we now use individualArrayIndex here, because the hit-testing code doesn't have an easy way to calculate the displayed individual index...
		Individual &individual = subpop->parent_individuals_[individualArrayIndex];
		float position_x, position_y;
		
		if (dimensionality == 1)
		{
			position_x = (float)((individual.spatial_x_ - bounds_x0) / bounds_x_size);
			position_y = (float)(random() / (double)INT32_MAX);
			
			if ((position_x < 0.0) || (position_x > 1.0))		// skip points that are out of bounds
				continue;
		}
		else
		{
			position_x = (float)((individual.spatial_x_ - bounds_x0) / bounds_x_size);
			position_y = (float)((individual.spatial_y_ - bounds_y0) / bounds_y_size);
			
			if ((position_x < 0.0) || (position_x > 1.0) || (position_y < 0.0) || (position_y > 1.0))		// skip points that are out of bounds
				continue;
		}
		
		float centerX = (float)(individualArea.origin.x + round(position_x * individualArea.size.width) + 0.5);
		float centerY = (float)(individualArea.origin.y + individualArea.size.height - round(position_y * individualArea.size.height) + 0.5);
		
		float left = centerX - 2.5f;
		float top = centerY - 2.5f;
		float right = centerX + 2.5f;
		float bottom = centerY + 2.5f;
		
		if (left < individualArea.origin.x) left = (float)individualArea.origin.x;
		if (top < individualArea.origin.y) top = (float)individualArea.origin.y;
		if (right > individualArea.origin.x + individualArea.size.width + 1) right = (float)(individualArea.origin.x + individualArea.size.width + 1);
		if (bottom > individualArea.origin.y + individualArea.size.height + 1) bottom = (float)(individualArea.origin.y + individualArea.size.height + 1);
		
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
		srandom(controller->sim->Generation());
	
	for (individualArrayIndex = 0; individualArrayIndex < subpopSize; ++individualArrayIndex)
	{
		// Figure out the rect to draw in; note we now use individualArrayIndex here, because the hit-testing code doesn't have an easy way to calculate the displayed individual index...
		Individual &individual = subpop->parent_individuals_[individualArrayIndex];
		float position_x, position_y;
		
		if (dimensionality == 1)
		{
			position_x = (float)((individual.spatial_x_ - bounds_x0) / bounds_x_size);
			position_y = (float)(random() / (double)INT32_MAX);
			
			if ((position_x < 0.0) || (position_x > 1.0))		// skip points that are out of bounds
				continue;
		}
		else
		{
			position_x = (float)((individual.spatial_x_ - bounds_x0) / bounds_x_size);
			position_y = (float)((individual.spatial_y_ - bounds_y0) / bounds_y_size);
			
			if ((position_x < 0.0) || (position_x > 1.0) || (position_y < 0.0) || (position_y > 1.0))		// skip points that are out of bounds
				continue;
		}
		
		float centerX = (float)(individualArea.origin.x + round(position_x * individualArea.size.width) + 0.5);
		float centerY = (float)(individualArea.origin.y + individualArea.size.height - round(position_y * individualArea.size.height) + 0.5);
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
		
		float colorRed = 0.0, colorGreen = 0.0, colorBlue = 0.0, colorAlpha = 1.0;
		
		if (gSLiM_Individual_custom_colors)
		{
			if (!individual.color_.empty())
			{
				colorRed = individual.color_red_;
				colorGreen = individual.color_green_;
				colorBlue = individual.color_blue_;
			}
			else
			{
				// use individual trait values to determine color; we used fitness values cached in UpdateFitness, so we don't have to call out to fitness callbacks
				double fitness = (useCachedFitness ? subpop_fitness[individualArrayIndex] : 1.0);
				
				RGBForFitness(fitness, &colorRed, &colorGreen, &colorBlue, scalingFactor);
			}
		}
		else
		{
			// use individual trait values to determine color; we used fitness values cached in UpdateFitness, so we don't have to call out to fitness callbacks
			double fitness = (useCachedFitness ? subpop_fitness[individualArrayIndex] : 1.0);
			
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

- (void)drawRect:(NSRect)rect
{
	NSRect bounds = [self bounds];
	SLiMWindowController *controller = [[self window] windowController];
	SLiMSim *sim = controller->sim;
	std::vector<Subpopulation*> selectedSubpopulations = [controller selectedSubpopulations];
	int selectedSubpopCount = (int)(selectedSubpopulations.size());
	
	//
	//	NOTE this code is parallel to code in canDisplaySubpopulations: and both should be maintained!
	//
	
	// Update the viewport
	glViewport(0, 0, (int)bounds.size.width, (int)bounds.size.height);
	
	// Update the projection
	glMatrixMode(GL_PROJECTION);
	GLKMatrix4 orthoMat = GLKMatrix4MakeOrtho(0.0, (int)bounds.size.width, (int)bounds.size.height, 0.0, -1.0f, 1.0f);
	glLoadMatrixf(orthoMat.m);
	glMatrixMode(GL_MODELVIEW);
	
	if (selectedSubpopCount == 0)
	{
		// clear to a shade of gray
		glColor3f(0.9f, 0.9f, 0.9f);
		glRecti(0, 0, (int)bounds.size.width, (int)bounds.size.height);
		
		// Frame our view
		[self drawViewFrameInBounds:bounds];
	}
	else if (displayMode == 1)
	{
		// Display fitness line plots for each subpopulation
		glColor3f(0.2f, 0.2f, 0.2f);
		glRecti(0, 0, (int)bounds.size.width, (int)bounds.size.height);
		
		for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
		{
			Subpopulation *subpop = selectedSubpopulations[subpopIndex];
			
			// Draw all the individuals
			[self drawFitnessLinePlotForSubpopulation:subpop inArea:bounds];
		}
		
		// Frame our view
		[self drawViewFrameInBounds:bounds];
	}
	else if (displayMode == 2)
	{
		// Display an aggregated fitness bar plot across all selected subpopulations
		glColor3f(0.0f, 0.0f, 0.0f);
		glRecti(0, 0, (int)bounds.size.width, (int)bounds.size.height);
		
		[self drawFitnessBarPlotForSubpopulations:selectedSubpopulations inArea:bounds];
		
		// Frame our view
		[self drawViewFrameInBounds:bounds];
	}
	else if (selectedSubpopCount > 10)
	{
		// We should be hidden in this case, but just in case, let's draw...
		
		// clear to our "too much information" shade
		glColor3f(0.9f, 0.9f, 1.0f);
		glRecti(0, 0, (int)bounds.size.width, (int)bounds.size.height);
		
		// Frame our view
		[self drawViewFrameInBounds:bounds];
	}
	else if (selectedSubpopCount == 1)
	{
		Subpopulation *subpop = selectedSubpopulations[0];
		
		if (sim->spatial_dimensionality_ == 1)
		{
			[self drawSpatialBackgroundInBounds:bounds forSubpopulation:subpop dimensionality:sim->spatial_dimensionality_];
			
			// Now determine a subframe and draw spatial information inside that
			NSRect spatialDisplayBounds = NSInsetRect(bounds, 1, 1);
			
			[self drawSpatialIndividualsFromSubpopulation:subpop inArea:spatialDisplayBounds dimensionality:sim->spatial_dimensionality_];
			
			[self drawViewFrameInBounds:NSInsetRect(spatialDisplayBounds, -1, -1)];
		}
		else if (sim->spatial_dimensionality_ > 1)
		{
			// clear to a shade of gray
			glColor3f(0.9f, 0.9f, 0.9f);
			glRecti(0, 0, (int)bounds.size.width, (int)bounds.size.height);
			
			// Frame our view
			[self drawViewFrameInBounds:bounds];
			
			// Now determine a subframe and draw spatial information inside that.  The subframe we use is the maximal subframe
			// with integer boundaries that preserves, as closely as possible, the aspect ratio of the subpop's bounds.
			NSRect spatialDisplayBounds = NSInsetRect(bounds, 1, 1);
			double displayAspect = spatialDisplayBounds.size.width / spatialDisplayBounds.size.height;
			double bounds_x0 = subpop->bounds_x0_, bounds_x1 = subpop->bounds_x1_;
			double bounds_y0 = subpop->bounds_y0_, bounds_y1 = subpop->bounds_y1_;
			double bounds_x_size = bounds_x1 - bounds_x0, bounds_y_size = bounds_y1 - bounds_y0;
			double subpopAspect = bounds_x_size / bounds_y_size;
			
			if (subpopAspect > displayAspect)
			{
				// The display bounds will need to shrink vertically to match the subpop
				double idealSize = round(spatialDisplayBounds.size.width / subpopAspect);
				double roundedOffset = round((spatialDisplayBounds.size.height - idealSize) / 2.0);
				
				spatialDisplayBounds.origin.y += roundedOffset;
				spatialDisplayBounds.size.height = idealSize;
			}
			else if (subpopAspect < displayAspect)
			{
				// The display bounds will need to shrink horizontally to match the subpop
				double idealSize = round(spatialDisplayBounds.size.height * subpopAspect);
				double roundedOffset = round((spatialDisplayBounds.size.width - idealSize) / 2.0);
				
				spatialDisplayBounds.origin.x += roundedOffset;
				spatialDisplayBounds.size.width = idealSize;
			}
			
			//spatialDisplayBounds.origin.x += floor((spatialDisplayBounds.size.width - spatialDisplayBounds.size.height) / 2.0);
			//spatialDisplayBounds.size.width = spatialDisplayBounds.size.height;
			
			[self drawSpatialBackgroundInBounds:spatialDisplayBounds forSubpopulation:subpop dimensionality:sim->spatial_dimensionality_];
			
			[self drawSpatialIndividualsFromSubpopulation:subpop inArea:spatialDisplayBounds dimensionality:sim->spatial_dimensionality_];
			
			[self drawViewFrameInBounds:NSInsetRect(spatialDisplayBounds, -1, -1)];
		}
		else
		{
			// Clear to white
			glColor3f(1.0, 1.0, 1.0);
			glRecti(0, 0, (int)bounds.size.width, (int)bounds.size.height);
			
			// Frame our view
			[self drawViewFrameInBounds:bounds];
			
			// Draw all the individuals
			[self drawIndividualsFromSubpopulation:subpop inArea:bounds];
		}
	}
	else
	{
		int interBoxSpace = 5;
		int totalInterbox = interBoxSpace * (selectedSubpopCount - 1);
		double boxHeight = (bounds.size.height - totalInterbox) / (double)selectedSubpopCount;
		
		// Clear to background gray; FIXME at present this hard-codes the OS X 10.10 background color...
		glColor3f(0.93f, 0.93f, 0.93f);
		glRecti(0, 0, (int)bounds.size.width, (int)bounds.size.height);
		
		for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
		{
			double boxTop = round(bounds.origin.y + subpopIndex * (interBoxSpace + boxHeight));
			double boxBottom = round(bounds.origin.y + subpopIndex * (interBoxSpace + boxHeight) + boxHeight);
			NSRect boxBounds = NSMakeRect(bounds.origin.x, boxTop, bounds.size.width, boxBottom - boxTop);
			Subpopulation *subpop = selectedSubpopulations[subpopIndex];
			
			// Clear to white
			glColor3f(1.0, 1.0, 1.0);
			glRecti((int)boxBounds.origin.x, (int)boxBounds.origin.y, (int)(boxBounds.origin.x + boxBounds.size.width), (int)(boxBounds.origin.y + boxBounds.size.height));
			
			// Frame our view
			[self drawViewFrameInBounds:boxBounds];
			
			// Draw all the individuals
			[self drawIndividualsFromSubpopulation:subpop inArea:boxBounds];
		}
	}
	
	[[self openGLContext] flushBuffer];
}

- (BOOL)canDisplaySubpopulations:(std::vector<Subpopulation*> &)selectedSubpopulations
{
	NSRect bounds = [self bounds];
	int selectedSubpopCount = (int)selectedSubpopulations.size();
	SLiMWindowController *controller = [[self window] windowController];
	SLiMSim *sim = controller->sim;
	
	// The individual-based display can't display more than a limited number of individuals;
	// we have to go through all the work to know where the limit is, though.  This code is
	// parallel to the code in drawRect: and should be maintained in parallel.
	if (selectedSubpopCount == 0)
	{
		return YES;
	}
	else if (displayMode == 1)
	{
		return YES;
	}
	else if (displayMode == 2)
	{
		return YES;
	}
	else if (selectedSubpopCount > 10)
	{
		return NO;
	}
	else if (selectedSubpopCount == 1)
	{
		if (sim->spatial_dimensionality_ == 1)
		{
			return YES;
		}
		else if (sim->spatial_dimensionality_ > 1)
		{
			return YES;
		}
		else
		{
			return [self canDisplayIndividualsFromSubpopulation:selectedSubpopulations[0] inArea:bounds];
		}
	}
	else
	{
		int interBoxSpace = 5;
		int totalInterbox = interBoxSpace * (selectedSubpopCount - 1);
		double boxHeight = (bounds.size.height - totalInterbox) / (double)selectedSubpopCount;
		
		for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
		{
			double boxTop = round(bounds.origin.y + subpopIndex * (interBoxSpace + boxHeight));
			double boxBottom = round(bounds.origin.y + subpopIndex * (interBoxSpace + boxHeight) + boxHeight);
			NSRect boxBounds = NSMakeRect(bounds.origin.x, boxTop, bounds.size.width, boxBottom - boxTop);
			Subpopulation *subpop = selectedSubpopulations[subpopIndex];
			
			if (![self canDisplayIndividualsFromSubpopulation:subpop inArea:boxBounds])
				return NO;
		}
		
		return YES;
	}
}

- (BOOL)canDisplayIndividualsFromSubpopulation:(Subpopulation *)subpop inArea:(NSRect)bounds
{
	//
	//	NOTE this code is parallel to the code in drawIndividualsFromSubpopulation:inArea: and should be maintained in parallel
	//
	
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	int squareSize, viewColumns = 0, viewRows = 0;
	
	// first figure out the biggest square size that will allow us to display the whole subpopulation
	for (squareSize = 20; squareSize > 1; --squareSize)
	{
		viewColumns = (int)floor((bounds.size.width - 3) / squareSize);
		viewRows = (int)floor((bounds.size.height - 3) / squareSize);
		
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

- (IBAction)setDisplayStyle:(id)sender
{
	int newDisplayMode = (int)[sender tag];
	
	if ((newDisplayMode == 1) || (newDisplayMode == 2))
	{
		// Run a sheet for display options, which will set the new mode if the user confirms
		[self runDisplayOptionsSheetForMode:newDisplayMode];
	}
	else
	{
		// This option does not require a sheet, so we just do it
		displayMode = newDisplayMode;
		[self setNeedsDisplay:YES];
		[[[self window] windowController] updatePopulationViewHiding];
	}
}


//
//	Options sheet handling
//
#pragma mark Options sheet handling

- (void)runDisplayOptionsSheetForMode:(int)newDisplayMode
{
	// Nil out our outlets for a bit of safety, and then load our sheet nib
	_displayOptionsSheet = nil;
	_binCountTextField = nil;
	_fitnessMinTextField = nil;
	_fitnessMaxTextField = nil;
	_okButton = nil;
	
	[[NSBundle mainBundle] loadNibNamed:@"PopulationViewOptionsSheet" owner:self topLevelObjects:NULL];
	
	// Run the sheet in our window
	if (_displayOptionsSheet)
	{
		[_binCountTextField setStringValue:[NSString stringWithFormat:@"%d", binCount]];
		[_fitnessMinTextField setStringValue:[NSString stringWithFormat:@"%0.1f", fitnessMin]];
		[_fitnessMaxTextField setStringValue:[NSString stringWithFormat:@"%0.1f", fitnessMax]];
		
		[self validateSheetControls:nil];
		
		NSWindow *window = [self window];
		
		[window beginSheet:_displayOptionsSheet completionHandler:^(NSModalResponse returnCode) {
			if (returnCode == NSAlertFirstButtonReturn)
			{
				// pull values from controls, set the new mode, and redisplay
				binCount = [[_binCountTextField stringValue] intValue];
				fitnessMin = [[_fitnessMinTextField stringValue] doubleValue];
				fitnessMax = [[_fitnessMaxTextField stringValue] doubleValue];
				
				displayMode = newDisplayMode;
				[self setNeedsDisplay:YES];
				[[[self window] windowController] updatePopulationViewHiding];
			}
			
			[_displayOptionsSheet autorelease];
			_displayOptionsSheet = nil;
		}];
	}
}

- (IBAction)validateSheetControls:(id)sender
{
	// Determine whether we have valid inputs in all of our fields
	BOOL validInput = YES;
	
	BOOL binCountValid = [ScriptMod validIntValueInTextField:_binCountTextField withMin:2 max:SLIM_MAX_HISTOGRAM_BINS];
	validInput = validInput && binCountValid;
	[_binCountTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:binCountValid]];
	
	BOOL fitnessMinValid = [ScriptMod validFloatValueInTextField:_fitnessMinTextField withMin:0.0 max:10.0];
	validInput = validInput && fitnessMinValid;
	[_fitnessMinTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:fitnessMinValid]];
	
	BOOL fitnessMaxValid = [ScriptMod validFloatValueInTextField:_fitnessMaxTextField withMin:0.0 max:10.0];
	validInput = validInput && fitnessMaxValid;
	[_fitnessMaxTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:fitnessMaxValid]];
	
	// if the input is invalid, we need to disable our OK button
	[_okButton setEnabled:validInput];
}

- (IBAction)displaySheetOK:(id)sender
{
	NSWindow *window = [self window];
	
	[window endSheet:_displayOptionsSheet returnCode:NSAlertFirstButtonReturn];
}

- (IBAction)displaySheetCancel:(id)sender
{
	NSWindow *window = [self window];
	
	[window endSheet:_displayOptionsSheet returnCode:NSAlertSecondButtonReturn];
}

- (void)controlTextDidChange:(NSNotification *)notification
{
	// NSTextField delegate method
	[self validateSheetControls:nil];
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
	NSMenu *menu = [[NSMenu alloc] initWithTitle:@"population_menu"];
	NSMenuItem *menuItem;
	
	menuItem = [menu addItemWithTitle:@"Display Individuals" action:@selector(setDisplayStyle:) keyEquivalent:@""];
	[menuItem setTag:0];
	[menuItem setTarget:self];
	
	menuItem = [menu addItemWithTitle:@"Display Fitness Line Plot (per subpopulation)..." action:@selector(setDisplayStyle:) keyEquivalent:@""];
	[menuItem setTag:1];
	[menuItem setTarget:self];
	
	menuItem = [menu addItemWithTitle:@"Display Fitness Bar Plot (aggregated)..." action:@selector(setDisplayStyle:) keyEquivalent:@""];
	[menuItem setTag:2];
	[menuItem setTarget:self];
	
	// Check the item corresponding to our current display preference, if any
	menuItem = [menu itemWithTag:displayMode];
	[menuItem setState:NSOnState];
	
	return [menu autorelease];
}

@end


@implementation PopulationErrorView

- (void)drawMessage:(NSString *)messageString inRect:(NSRect)rect
{
	static NSDictionary *attrs = nil;
	
	if (!attrs)
		attrs = [@{NSFontAttributeName : [NSFont fontWithName:@"Times New Roman" size:14], NSForegroundColorAttributeName : [NSColor colorWithCalibratedWhite:0.4 alpha:1.0]} retain];
	
	NSAttributedString *attrMessage = [[NSAttributedString alloc] initWithString:messageString attributes:attrs];
	NSPoint centerPoint = NSMakePoint(rect.origin.x + rect.size.width / 2, rect.origin.y + rect.size.height / 2);
	NSSize messageSize = [attrMessage size];
	NSPoint drawPoint = NSMakePoint(centerPoint.x - messageSize.width / 2.0, centerPoint.y - messageSize.height / 2.0);
	
	[attrMessage drawAtPoint:drawPoint];
	[attrMessage release];
}

- (void)drawRect:(NSRect)rect
{
	NSRect bounds = [self bounds];
	
	// Erase background
	[[NSColor colorWithDeviceWhite:0.9 alpha:1.0] set];
	NSRectFill(bounds);
	
	// Frame the view
	[[NSColor colorWithDeviceWhite:0.77 alpha:1.0] set];
	NSFrameRect(bounds);
	
	// Draw the message
	[self drawMessage:@"too many subpops\n   or individuals\n     to display –\n  control-click to\n select a different\n    display mode" inRect:NSInsetRect(bounds, 1, 1)];
}

- (BOOL)isOpaque
{
	return YES;
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
	SLiMWindowController *controller = [[self window] windowController];
	PopulationView *popView = controller->populationView;
	
	return [popView menuForEvent:theEvent];
}

@end






















































