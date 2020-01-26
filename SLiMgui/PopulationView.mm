//
//  PopulationView.m
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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
	displayMode = -1;	// don't know yet whether the model is spatial or not, which will determine our initial choice
	
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

/*
// BCH 4/19/2019: these subclass methods are not needed, and are generating a warning in Xcode 10.2, so I'm removing them
 
// Called after being summoned from a NIB/XIB.
- (void)prepareOpenGL
{
}

// Adjust the OpenGL viewport in response to window resize
- (void)reshape
{
}
*/

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
	int squareSize, viewColumns = 0, viewRows = 0;
	double subpopFitnessScaling = subpop->last_fitness_scaling_;
	
	if ((subpopFitnessScaling <= 0.0) || !std::isfinite(subpopFitnessScaling))
		subpopFitnessScaling = 1.0;
	
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
			
			// dark gray default, for a fitness of NaN; should never happen
			float colorRed = 0.3f, colorGreen = 0.3f, colorBlue = 0.3f, colorAlpha = 1.0;
			Individual &individual = *subpop->parent_individuals_[individualArrayIndex];
			
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
		
		// Draw a gear
		// This is an experiment with drawing an "action" button on top of the view contents using OpenGL.
		// The results are decent on a Retina display, but not great on a regular display.  In any case
		// I think having the action button drawn on top of view contents ends up looking strange.
		// Unfortunately there doesn't seem to be anywhere good to put such buttons; so we'll stick with
		// context menus for now.  BCH 1/27/2019
#if 0
		{
			double cx = bounds.size.width - 13.0, cy = 13.0;
			double radius = 8.0;
			float button_color = 1.0f;
			float outline_color = 0.60f;
			
			// ***** draw the off-white button disc as a triangle fan
			vertices = glArrayVertices;
			colors = glArrayColors;
			displayListIndex = 0;
			
			// center point
			*(vertices++) = (float)cx;
			*(vertices++) = (float)cy;
			displayListIndex++;
			
			// fan points
			for (int j = 0; j <= 32; ++j)
			{
				double angle = (j / (32.0)) * (2.0 * M_PI);
				double r = radius * 1.30;
				
				*(vertices++) = (float)(cx + cos(angle) * r);
				*(vertices++) = (float)(cy + sin(angle) * r);
				displayListIndex++;
			}
			
			// colors
			for (int j = 0; j < displayListIndex; ++j)
			{
				*(colors++) = button_color;
				*(colors++) = button_color;
				*(colors++) = button_color;
				*(colors++) = 1.0f;
			}
			
			glDrawArrays(GL_TRIANGLE_FAN, 0, displayListIndex);
			
			// ***** outline the button with gray
			colors = glArrayColors;
			
			for (int j = 0; j < displayListIndex; ++j)
			{
				*(colors++) = outline_color;
				*(colors++) = outline_color;
				*(colors++) = outline_color;
				*(colors++) = 1.0f;
			}
			
			glLineWidth(2.0);
			glEnable(GL_LINE_SMOOTH);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			
			glDrawArrays(GL_LINE_LOOP, 1, displayListIndex - 2);
			
			glLineWidth(1.0);
			glDisable(GL_LINE_SMOOTH);
			glDisable(GL_BLEND);
			
			// ***** draw the gear polygon as a triangle fan
			vertices = glArrayVertices;
			colors = glArrayColors;
			displayListIndex = 0;
			
			// center point
			*(vertices++) = (float)cx;
			*(vertices++) = (float)cy;
			displayListIndex++;
			
			// fan points
			for (int j = 0; j <= 8 * 6; ++j)
			{
				double angle = (j / (8.0 * 6.0)) * (2.0 * M_PI);
				int tooth_step = (j + 1) % 6;	// 6 steps per tooth
				double r = (tooth_step < 3) ? radius : radius * 0.7;
				
				*(vertices++) = (float)(cx + cos(angle) * r);
				*(vertices++) = (float)(cy + sin(angle) * r);
				displayListIndex++;
			}
			
			// colors
			for (int j = 0; j < displayListIndex; ++j)
			{
				*(colors++) = 0.3f;
				*(colors++) = 0.3f;
				*(colors++) = 0.3f;
				*(colors++) = 1.0f;
			}
			
			glDrawArrays(GL_TRIANGLE_FAN, 0, displayListIndex);
			
			// ***** draw the gear outline, to antialias it
			colors = glArrayColors;
			
			for (int j = 0; j < displayListIndex; ++j)
			{
				*(colors++) = button_color;
				*(colors++) = button_color;
				*(colors++) = button_color;
				*(colors++) = 1.0f;
			}
			
			glLineWidth(1.0);
			glEnable(GL_LINE_SMOOTH);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			
			glDrawArrays(GL_LINE_LOOP, 1, displayListIndex - 2);
			
			glLineWidth(1.0);
			glDisable(GL_LINE_SMOOTH);
			glDisable(GL_BLEND);
			
			// ***** draw the interior circle
			vertices = glArrayVertices;
			colors = glArrayColors;
			displayListIndex = 0;
			
			// center point
			*(vertices++) = (float)cx;
			*(vertices++) = (float)cy;
			displayListIndex++;
			
			// fan points
			for (int j = 0; j <= 16; ++j)
			{
				double angle = (j / 16.0) * (2.0 * M_PI);
				double r = radius * 0.25;
				
				*(vertices++) = (float)(cx + cos(angle) * r);
				*(vertices++) = (float)(cy + sin(angle) * r);
				displayListIndex++;
			}
			
			// colors
			for (int j = 0; j < displayListIndex; ++j)
			{
				*(colors++) = button_color;
				*(colors++) = button_color;
				*(colors++) = button_color;
				*(colors++) = 1.0f;
			}
			
			glDrawArrays(GL_TRIANGLE_FAN, 0, displayListIndex);
			
			// ***** draw the inner circle outline, to antialias it
			glLineWidth(1.0);
			glEnable(GL_LINE_SMOOTH);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			
			glDrawArrays(GL_LINE_LOOP, 1, displayListIndex - 2);
			
			glLineWidth(1.0);
			glDisable(GL_LINE_SMOOTH);
			glDisable(GL_BLEND);
		}
#endif
		
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
	double subpopFitnessScaling = subpop->last_fitness_scaling_;
	
	if ((subpopFitnessScaling <= 0.0) || !std::isfinite(subpopFitnessScaling))
		subpopFitnessScaling = 1.0;
	
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
		Individual &individual = *subpop->parent_individuals_[individualIndex];
		double fitness = individual.cached_fitness_UNSAFE_;
		
		if (!std::isnan(fitness))
		{
			// we normalize fitness values with subpopFitnessScaling so individual fitness, unscaled by subpopulation fitness, is used for coloring
			int binIndex = (int)floor((((fitness / subpopFitnessScaling) - fitnessMin) / (fitnessMax - fitnessMin)) * numberOfBins);
			
			if (binIndex < 0) binIndex = 0;
			if (binIndex >= numberOfBins) binIndex = numberOfBins - 1;
			
			binCounts[binIndex]++;
		}
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
		double subpopFitnessScaling = subpop->last_fitness_scaling_;
		
		if ((subpopFitnessScaling <= 0.0) || !std::isfinite(subpopFitnessScaling))
		subpopFitnessScaling = 1.0;
		
		for (int individualIndex = 0; individualIndex < subpopSize; ++individualIndex)
		{
			Individual &individual = *subpop->parent_individuals_[individualIndex];
			double fitness = individual.cached_fitness_UNSAFE_;
			
			if (!std::isnan(fitness))
			{
				// we normalize fitness values with subpopFitnessScaling so individual fitness, unscaled by subpopulation fitness, is used for coloring
				int binIndex = (int)floor((((fitness / subpopFitnessScaling) - fitnessMin) / (fitnessMax - fitnessMin)) * numberOfBins);
				
				if (binIndex < 0) binIndex = 0;
				if (binIndex >= numberOfBins) binIndex = numberOfBins - 1;
				
				binCounts[binIndex]++;
			}
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

- (void)_drawBackgroundSpatialMap:(SpatialMap *)background_map inBounds:(NSRect)bounds forSubpopulation:(Subpopulation *)subpop
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

- (void)chooseDefaultBackgroundSettings:(PopulationViewBackgroundSettings *)background map:(SpatialMap **)returnMap forSubpopulation:(Subpopulation *)subpop
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

- (void)drawSpatialBackgroundInBounds:(NSRect)bounds forSubpopulation:(Subpopulation *)subpop dimensionality:(int)dimensionality
{
	auto backgroundIter = backgroundSettings.find(subpop->subpopulation_id_);
	PopulationViewBackgroundSettings background;
	SpatialMap *background_map = nil;
	
	if (backgroundIter == backgroundSettings.end())
	{
		// The user has not made a choice, so choose a temporary default.  We don't want this choice to "stick",
		// so that we can, e.g., begin as black and then change to a spatial map if one is defined.
		[self chooseDefaultBackgroundSettings:&background map:&background_map forSubpopulation:subpop];
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
					background_map = nil;
			}
		}
		
		// if we're supposed to use a background map but we couldn't find it, or it's unacceptable, revert to black
		if ((background.backgroundType == 3) && !background_map)
			background.backgroundType = 0;
	}
	
	if ((background.backgroundType == 3) && background_map)
	{
		[self _drawBackgroundSpatialMap:background_map inBounds:bounds forSubpopulation:subpop];
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
	double subpopFitnessScaling = subpop->last_fitness_scaling_;
	
	if ((subpopFitnessScaling <= 0.0) || !std::isfinite(subpopFitnessScaling))
		subpopFitnessScaling = 1.0;
	
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
		Individual &individual = *subpop->parent_individuals_[individualArrayIndex];
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
		Individual &individual = *subpop->parent_individuals_[individualArrayIndex];
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

- (void)drawRect:(NSRect)rect
{
	//
	//	NOTE this code is parallel to code in tileSubpopulations: and both should be maintained!
	//
	
	NSRect bounds = [self bounds];
	SLiMWindowController *controller = [[self window] windowController];
	SLiMSim *sim = controller->sim;
	std::vector<Subpopulation*> selectedSubpopulations = [controller selectedSubpopulations];
	int selectedSubpopCount = (int)(selectedSubpopulations.size());
	
	// Decide on our display mode
	if (!controller->invalidSimulation && sim && sim->simulation_valid_ && (sim->generation_ >= 1))
	{
		if (displayMode == -1)
			displayMode = ((sim->spatial_dimensionality_ == 0) ? 0 : 1);
		if ((displayMode == 1) && (sim->spatial_dimensionality_ == 0))
			displayMode = 0;
	}
	
	// Update the viewport; using backingBounds here instead of bounds makes the view hi-res-aware,
	// while still remaining point-based since the ortho projection we use below uses bounds.
	NSRect backingBounds = [self convertRectToBacking:bounds];
	
	glViewport(0, 0, (int)backingBounds.size.width, (int)backingBounds.size.height);
	
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
	else if (displayMode == 2)
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
	else if (displayMode == 3)
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
	else
	{
		if (selectedSubpopulations.size() > 1)
		{
			// Clear to background gray; FIXME at present this hard-codes the OS X 10.10 background color...
			glColor3f(0.93f, 0.93f, 0.93f);
			glRecti(0, 0, (int)bounds.size.width, (int)bounds.size.height);
		}
		
		for (Subpopulation *subpop : selectedSubpopulations)
		{
			auto tileIter = subpopTiles.find(subpop->subpopulation_id_);
			
			if (tileIter != subpopTiles.end())
			{
				NSRect tileBounds = tileIter->second;
				
				if ((displayMode == 1) && (sim->spatial_dimensionality_ == 1))
				{
					[self drawSpatialBackgroundInBounds:tileBounds forSubpopulation:subpop dimensionality:sim->spatial_dimensionality_];
					[self drawSpatialIndividualsFromSubpopulation:subpop inArea:NSInsetRect(tileBounds, 1, 1) dimensionality:sim->spatial_dimensionality_];
					[self drawViewFrameInBounds:tileBounds];
				}
				else if ((displayMode == 1) && (sim->spatial_dimensionality_ > 1))
				{
					// clear to a shade of gray
					glColor3f(0.9f, 0.9f, 0.9f);
					glRecti((int)tileBounds.origin.x, (int)tileBounds.origin.y, (int)(tileBounds.origin.x + tileBounds.size.width), (int)(tileBounds.origin.y + tileBounds.size.height));
					
					// Frame our view
					[self drawViewFrameInBounds:tileBounds];
					
					// Now determine a subframe and draw spatial information inside that.
					NSRect spatialDisplayBounds = [self spatialDisplayBoundsForSubpopulation:subpop tileBounds:tileBounds];
					
					[self drawSpatialBackgroundInBounds:spatialDisplayBounds forSubpopulation:subpop dimensionality:sim->spatial_dimensionality_];
					[self drawSpatialIndividualsFromSubpopulation:subpop inArea:spatialDisplayBounds dimensionality:sim->spatial_dimensionality_];
					[self drawViewFrameInBounds:NSInsetRect(spatialDisplayBounds, -1, -1)];
				}
				else	// displayMode == 0
				{
					// Clear to white
					glColor3f(1.0, 1.0, 1.0);
					glRecti((int)tileBounds.origin.x, (int)tileBounds.origin.y, (int)(tileBounds.origin.x + tileBounds.size.width), (int)(tileBounds.origin.y + tileBounds.size.height));
					
					[self drawViewFrameInBounds:tileBounds];
					[self drawIndividualsFromSubpopulation:subpop inArea:tileBounds];
				}
			}
		}
	}
	
	[[self openGLContext] flushBuffer];
}


//
//	Subarea tiling
//
#pragma mark Subarea tiling

- (BOOL)tileSubpopulations:(std::vector<Subpopulation*> &)selectedSubpopulations
{
	//
	//	NOTE this code is parallel to code in drawRect: and both should be maintained!
	//
	
	// We will decide upon new tiles for our subpopulations here, so start out empty
	subpopTiles.clear();
	
	NSRect bounds = [self bounds];
	SLiMWindowController *controller = [[self window] windowController];
	SLiMSim *sim = controller->sim;
	int selectedSubpopCount = (int)selectedSubpopulations.size();
	
	// Decide on our display mode
	if (!controller->invalidSimulation && sim && sim->simulation_valid_ && (sim->generation_ >= 1))
	{
		if (displayMode == -1)
			displayMode = ((sim->spatial_dimensionality_ == 0) ? 0 : 1);
		if ((displayMode == 1) && (sim->spatial_dimensionality_ == 0))
			displayMode = 0;
	}
	
	if (selectedSubpopCount == 0)
	{
		return YES;
	}
	else if (displayMode == 2)
	{
		return YES;
	}
	else if (displayMode == 3)
	{
		return YES;
	}
	else if (selectedSubpopCount > 10)
	{
		return NO;
	}
	else if (selectedSubpopCount == 1)
	{
		Subpopulation *selectedSubpop = selectedSubpopulations[0];
		
		subpopTiles.insert(std::pair<slim_objectid_t, NSRect>(selectedSubpop->subpopulation_id_, bounds));
		
		if ((displayMode == 1) && (sim->spatial_dimensionality_ == 1))
		{
			return YES;
		}
		else if ((displayMode == 1) && (sim->spatial_dimensionality_ > 1))
		{
			return YES;
		}
		else
		{
			return [self canDisplayIndividualsFromSubpopulation:selectedSubpop inArea:bounds];
		}
	}
	else if (displayMode == 1)
	{
		// spatial display adaptively finds the layout the maximizes the pixel area covered, and cannot fail
		int64_t bestTotalExtent = 0;
		
		for (int rowCount = 1; rowCount <= selectedSubpopCount; ++rowCount)
		{
			int columnCount = (int)ceil(selectedSubpopCount / (double)rowCount);
			int interBoxSpace = 5;
			int totalInterboxHeight = interBoxSpace * (rowCount - 1);
			int totalInterboxWidth = interBoxSpace * (columnCount - 1);
			double boxWidth = (bounds.size.width - totalInterboxWidth) / (double)columnCount;
			double boxHeight = (bounds.size.height - totalInterboxHeight) / (double)rowCount;
			std::map<slim_objectid_t, NSRect> candidateTiles;
			int64_t totalExtent = 0;
			
			for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
			{
				int columnIndex = subpopIndex % columnCount;
				int rowIndex = subpopIndex / columnCount;
				double boxLeft = round(bounds.origin.x + columnIndex * (interBoxSpace + boxWidth));
				double boxRight = round(bounds.origin.x + columnIndex * (interBoxSpace + boxWidth) + boxWidth);
				double boxTop = round(bounds.origin.y + rowIndex * (interBoxSpace + boxHeight));
				double boxBottom = round(bounds.origin.y + rowIndex * (interBoxSpace + boxHeight) + boxHeight);
				NSRect boxBounds = NSMakeRect(boxLeft, boxTop, boxRight - boxLeft, boxBottom - boxTop);
				Subpopulation *subpop = selectedSubpopulations[subpopIndex];
				
				candidateTiles.insert(std::pair<slim_objectid_t, NSRect>(subpop->subpopulation_id_, boxBounds));
				
				// find out what pixel area actually gets used by this box, and use that to choose the optimal layout
				NSRect spatialDisplayBounds = [self spatialDisplayBoundsForSubpopulation:subpop tileBounds:boxBounds];
				int64_t extent = (int64_t)spatialDisplayBounds.size.width * (int64_t)spatialDisplayBounds.size.height;
				
				totalExtent += extent;
			}
			
			if (totalExtent > bestTotalExtent)
			{
				bestTotalExtent = totalExtent;
				std::swap(subpopTiles, candidateTiles);
			}
		}
		
		return YES;
	}
	else	// displayMode == 0
	{
		// non-spatial display always uses vertically stacked maximum-width tiles, but can fail if they are too small
		int interBoxSpace = 5;
		int totalInterbox = interBoxSpace * (selectedSubpopCount - 1);
		double boxHeight = (bounds.size.height - totalInterbox) / (double)selectedSubpopCount;
		
		for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
		{
			double boxTop = round(bounds.origin.y + subpopIndex * (interBoxSpace + boxHeight));
			double boxBottom = round(bounds.origin.y + subpopIndex * (interBoxSpace + boxHeight) + boxHeight);
			NSRect boxBounds = NSMakeRect(bounds.origin.x, boxTop, bounds.size.width, boxBottom - boxTop);
			Subpopulation *subpop = selectedSubpopulations[subpopIndex];
			
			subpopTiles.insert(std::pair<slim_objectid_t, NSRect>(subpop->subpopulation_id_, boxBounds));
			
			if (![self canDisplayIndividualsFromSubpopulation:subpop inArea:boxBounds])
			{
				subpopTiles.clear();
				return NO;
			}
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

- (NSRect)spatialDisplayBoundsForSubpopulation:(Subpopulation *)subpop tileBounds:(NSRect)tileBounds
{
	// Determine a subframe for drawing spatial information inside.  The subframe we use is the maximal subframe
	// with integer boundaries that preserves, as closely as possible, the aspect ratio of the subpop's bounds.
	NSRect spatialDisplayBounds = NSInsetRect(tileBounds, 1, 1);
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
	
	return spatialDisplayBounds;
}


//
//	Actions
//
#pragma mark Actions

- (IBAction)setDisplayStyle:(id)sender
{
	NSMenuItem *senderMenuItem = (NSMenuItem *)sender;
	int newDisplayMode = (int)[senderMenuItem tag];
	
	if ((newDisplayMode == 2) || (newDisplayMode == 3))
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

- (IBAction)setDisplayBackground:(id)sender
{
	NSMenuItem *senderMenuItem = (NSMenuItem *)sender;
	int newDisplayBackground = (int)([senderMenuItem tag] - 10);
	auto backgroundIter = backgroundSettings.find(lastContextMenuSubpopID);
	PopulationViewBackgroundSettings *background = ((backgroundIter == backgroundSettings.end()) ? nil : &backgroundIter->second);
	std::string mapName;
	
	// If the user has selected a spatial map, extract its name
	if (newDisplayBackground == 3)
	{
		NSString *menuItemTitle = [senderMenuItem title];
		NSArray<NSString *> *parts = [menuItemTitle componentsSeparatedByString:@"\""];
		
		if ([parts count] == 5)
		{
			NSString *mapNameString = [parts objectAtIndex:1];
			const char *mapNameString_inner = [mapNameString UTF8String];
			
			mapName = std::string(mapNameString_inner);
		}
		
		if (mapName.length() == 0)
			return;
	}
	
	if (background)
	{
		background->backgroundType = newDisplayBackground;
		background->spatialMapName = mapName;
		[self setNeedsDisplay:YES];
	}
	else
	{
		backgroundSettings.insert(std::pair<const int, PopulationViewBackgroundSettings>(lastContextMenuSubpopID, PopulationViewBackgroundSettings{newDisplayBackground, mapName}));
		[self setNeedsDisplay:YES];
	}
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
	SLiMWindowController *controller = [[self window] windowController];
	SLiMSim *sim = controller->sim;
	bool disableAll = false;
	
	// When the simulation is not valid and initialized, the context menu is disabled
	if (controller->invalidSimulation || !sim || !sim->simulation_valid_ || (sim->generation_ < 1))
		disableAll = true;
	
	NSMenu *menu = [[NSMenu alloc] initWithTitle:@"population_menu"];
	NSMenuItem *menuItem;
	
	[menu setAutoenablesItems:NO];
	
	menuItem = [menu addItemWithTitle:@"Display Individuals (non-spatial)" action:@selector(setDisplayStyle:) keyEquivalent:@""];
	[menuItem setTag:0];
	[menuItem setTarget:self];
	[menuItem setEnabled:!disableAll];
	
	menuItem = [menu addItemWithTitle:@"Display Individuals (spatial)" action:@selector(setDisplayStyle:) keyEquivalent:@""];
	[menuItem setTag:1];
	[menuItem setTarget:self];
	[menuItem setEnabled:(!disableAll && (sim->spatial_dimensionality_ > 0))];
	
	menuItem = [menu addItemWithTitle:@"Display Fitness Line Plot (per subpopulation)..." action:@selector(setDisplayStyle:) keyEquivalent:@""];
	[menuItem setTag:2];
	[menuItem setTarget:self];
	[menuItem setEnabled:!disableAll];
	
	menuItem = [menu addItemWithTitle:@"Display Fitness Bar Plot (aggregated)..." action:@selector(setDisplayStyle:) keyEquivalent:@""];
	[menuItem setTag:3];
	[menuItem setTarget:self];
	[menuItem setEnabled:!disableAll];
	
	// Check the item corresponding to our current display preference, if any
	if (!disableAll)
	{
		menuItem = [menu itemWithTag:displayMode];
		[menuItem setState:NSOnState];
	}
	
	// If we're displaying spatially, provide background options (colors, spatial maps)
	if (!disableAll && (sim->spatial_dimensionality_ > 0) && (displayMode == 1))
	{
		// determine which subpopulation the click was in
		std::vector<Subpopulation*> selectedSubpopulations = [controller selectedSubpopulations];
		Subpopulation *subpopForEvent = nullptr;
		NSPoint eventPoint = [theEvent locationInWindow];
		NSPoint viewPoint = [self convertPoint:eventPoint fromView:nil];
		
		// our tile coordinates are in the OpenGL coordinate system, which has the origin at top left
		viewPoint.y = [self bounds].size.height - viewPoint.y;
		
		for (Subpopulation *subpop : selectedSubpopulations)
		{
			slim_objectid_t subpop_id = subpop->subpopulation_id_;
			auto tileIter = subpopTiles.find(subpop_id);
			
			if (tileIter != subpopTiles.end())
			{
				NSRect tileRect = tileIter->second;
				
				if (NSPointInRect(viewPoint, tileRect))
				{
					subpopForEvent = subpop;
					break;
				}
			}
		}
		
		if (subpopForEvent)
		{
			[menu addItem:[NSMenuItem separatorItem]];
			
			menuItem = [menu addItemWithTitle:[NSString stringWithFormat:@"Background for p%d:", subpopForEvent->subpopulation_id_] action:NULL keyEquivalent:@""];
			[menuItem setTag:-1];
			[menuItem setEnabled:false];
			
			menuItem = [menu addItemWithTitle:@"Black Background" action:@selector(setDisplayBackground:) keyEquivalent:@""];
			[menuItem setTag:10];
			[menuItem setTarget:self];
			[menuItem setEnabled:!disableAll];
			
			menuItem = [menu addItemWithTitle:@"Gray Background" action:@selector(setDisplayBackground:) keyEquivalent:@""];
			[menuItem setTag:11];
			[menuItem setTarget:self];
			[menuItem setEnabled:!disableAll];
			
			menuItem = [menu addItemWithTitle:@"White Background" action:@selector(setDisplayBackground:) keyEquivalent:@""];
			[menuItem setTag:12];
			[menuItem setTarget:self];
			[menuItem setEnabled:!disableAll];
			
			// look for spatial maps to offer as choices; need to scan the defined maps for the ones we can use
			SpatialMapMap &spatial_maps = subpopForEvent->spatial_maps_;
			
			for (const auto &map_pair : spatial_maps)
			{
				SpatialMap *map = map_pair.second;
				
				// We used to display only maps with a color scale; now we just make up a color scale if none is given.  Only
				// "x", "y", and "xy" maps are considered displayable; We can't display a z coordinate, and we can't display
				// even the x or y portion of "xz", "yz", and "xyz" maps since we don't know which z-slice to use.
				bool displayable = ((map->spatiality_string_ == "x") || (map->spatiality_string_ == "y") || (map->spatiality_string_ == "xy"));
				NSString *menuItemTitle;
				
				if (map->spatiality_ == 1)
					menuItemTitle = [NSString stringWithFormat:@"Spatial Map \"%s\" (\"%s\", %d)", map_pair.first.c_str(), map->spatiality_string_.c_str(), (int)map->grid_size_[0]];
				else if (map->spatiality_ == 2)
					menuItemTitle = [NSString stringWithFormat:@"Spatial Map \"%s\" (\"%s\", %d×%d)", map_pair.first.c_str(), map->spatiality_string_.c_str(), (int)map->grid_size_[0], (int)map->grid_size_[1]];
				else // (map->spatiality_ == 3)
					menuItemTitle = [NSString stringWithFormat:@"Spatial Map \"%s\" (\"%s\", %d×%d×%d)", map_pair.first.c_str(), map->spatiality_string_.c_str(), (int)map->grid_size_[0], (int)map->grid_size_[1], (int)map->grid_size_[2]];
				
				menuItem = [menu addItemWithTitle:menuItemTitle action:@selector(setDisplayBackground:) keyEquivalent:@""];
				[menuItem setTag:13];
				[menuItem setTarget:self];
				[menuItem setEnabled:(!disableAll && displayable)];
			}
			
			// check the menu item for the preferred display option; if we're in auto mode, don't check anything (could put a dash by the currently chosen style?)
			auto backgroundIter = backgroundSettings.find(subpopForEvent->subpopulation_id_);
			PopulationViewBackgroundSettings *background = ((backgroundIter == backgroundSettings.end()) ? nil : &backgroundIter->second);
			
			if (background)
			{
				menuItem = nil;
				
				if (background->backgroundType == 3)
				{
					// We have to find the menu item to check by scanning and looking for the right title
					NSString *chosenMapPrefix = [NSString stringWithFormat:@"Spatial Map \"%s\" (\"", background->spatialMapName.c_str()];
					NSArray<NSMenuItem *> *menuItems = [menu itemArray];
					
					for (NSMenuItem *item : menuItems)
					{
						if ([[item title] hasPrefix:chosenMapPrefix])
						{
							menuItem = item;
							break;
						}
					}
				}
				else
				{
					// We can find the menu item to check by tag
					menuItem = [menu itemWithTag:background->backgroundType + 10];
				}
				
				[menuItem setState:NSOnState];
			}
			
			// remember which subpopulation this context menu is for
			lastContextMenuSubpopID = subpopForEvent->subpopulation_id_;
		}
	}
	
	return [menu autorelease];
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






















































