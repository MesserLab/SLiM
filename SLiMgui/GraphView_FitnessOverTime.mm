//
//  GraphView_FitnessOverTime.m
//  SLiM
//
//  Created by Ben Haller on 3/1/15.
//	A product of the Messer Lab, http://messerlab.org/software/
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


#import "GraphView_FitnessOverTime.h"
#import "SLiMWindowController.h"


@implementation GraphView_FitnessOverTime

- (void)invalidateDrawingCache
{
	[drawingCache release];
	drawingCache = nil;
	drawingCacheGeneration = 0;
}

- (void)setDefaultYAxisRange
{
	[self setYAxisMin:0.9];
	[self setYAxisMax:1.1];		// dynamic
	[self setYAxisMajorTickInterval:0.1];
	[self setYAxisMinorTickInterval:0.02];
	[self setYAxisMajorTickModulus:5];
	[self setYAxisTickValuePrecision:1];
}

- (id)initWithFrame:(NSRect)frameRect withController:(SLiMWindowController *)controller
{
	if (self = [super initWithFrame:frameRect withController:controller])
	{
		[self setXAxisMax:[self slimWindowController]->sim->time_duration_];
		[self setXAxisMajorTickInterval:10000];
		[self setXAxisMinorTickInterval:1000];
		[self setXAxisMajorTickModulus:10];
		[self setXAxisTickValuePrecision:0];
		
		[self setDefaultYAxisRange];
		
		[self setXAxisLabelString:@"Generation"];
		[self setYAxisLabelString:@"Fitness (rescaled absolute)"];
		
		[self setShowHorizontalGridLines:YES];
		[self setTweakXAxisTickLabelAlignment:YES];
	}
	
	return self;
}

- (void)graphWindowResized
{
	[self invalidateDrawingCache];
	
	[super graphWindowResized];
}

- (void)controllerRecycled
{
	SLiMWindowController *controller = [self slimWindowController];
	
	if (![controller invalidSimulation])
	{
		[self setDefaultYAxisRange];
		[self setXAxisMax:controller->sim->time_duration_];
		
		[self invalidateDrawingCache];
		[self setNeedsDisplay:YES];
	}
	
	[super controllerRecycled];
}

- (void)controllerSelectionChanged
{
	[self invalidateDrawingCache];
	[self setNeedsDisplay:YES];
	
	[super controllerSelectionChanged];
}

- (void)dealloc
{
	[self invalidateDrawingCache];
	
	[super dealloc];
}

- (void)rescaleAsNeededWithInteriorRect:(NSRect)interiorRect andController:(SLiMWindowController *)controller
{
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	double *history = pop.fitnessHistory;
	uint32 historyLength = pop.fitnessHistoryLength;
	double minHistory = INFINITY;
	double maxHistory = -INFINITY;
	
	// find the min and max history value
	for (int i = 0; i < historyLength; ++i)
	{
		double historyEntry = history[i];
		
		if (!isnan(historyEntry))
		{
			if (historyEntry > maxHistory)
				maxHistory = historyEntry;
			if (historyEntry < minHistory)
				minHistory = historyEntry;
		}
	}
	
	// set axis range to encompass the data
	if (!isinf(minHistory) && !isinf(maxHistory))
	{
		if ((minHistory < 0.9) || (maxHistory > 1.1))	// if we're outside our original axis range...
		{
			double axisMin = (minHistory < 0.5 ? 0.0 : 0.5);	// either 0.0 or 0.5
			double axisMax = ceil(maxHistory * 2.0) / 2.0;		// 1.5, 2.0, 2.5, ...
			
			if (axisMax < 1.5)
				axisMax = 1.5;
			
			if ((axisMin != [self yAxisMin]) || (axisMax != [self yAxisMax]))
			{
				[self setYAxisMin:axisMin];
				[self setYAxisMax:axisMax];
				[self setYAxisMajorTickInterval:0.5];
				[self setYAxisMinorTickInterval:0.25];
				[self setYAxisMajorTickModulus:2];
				[self setYAxisTickValuePrecision:1];
				
				[self invalidateDrawingCache];
			}
		}
	}
}

- (void)drawGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller
{
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	int completedGenerations = sim->generation_ - 1;
	
	// If we're not caching, then: if our cache is invalid OR we have crossed a 1000-generation boundary since we last cached, cache an image
	if (!cachingNow && (!drawingCache || ((completedGenerations / 1000) > (drawingCacheGeneration / 1000))))
	{
		[self invalidateDrawingCache];
		
		NSBitmapImageRep *bitmap = [self bitmapImageRepForCachingDisplayInRect:interiorRect];
		
		cachingNow = YES;
		//NSLog(@"recaching!");
		[bitmap setSize:interiorRect.size];
		[self cacheDisplayInRect:interiorRect toBitmapImageRep:bitmap];
		drawingCache = [[NSImage alloc] initWithSize:interiorRect.size];
		[drawingCache addRepresentation:bitmap];
		drawingCacheGeneration = completedGenerations;
		cachingNow = NO;
	}
	
	// Now draw our cache, if we have one
	if (drawingCache)
		[drawingCache drawInRect:interiorRect];
	
	// Draw fixation events
	std::vector<const Substitution*> &substitutions = pop.substitutions_;
	
	for (const Substitution *substitution : substitutions)
	{
		int fixation_time = substitution->fixation_time_;
		
		// If we are caching, draw all events; if we are not, draw only those that are not already in the cache
		if (!cachingNow && (fixation_time < drawingCacheGeneration))
			continue;
		
		double substitutionX = [self plotToDeviceX:fixation_time withInteriorRect:interiorRect];
		NSRect substitutionRect = NSMakeRect(substitutionX - 0.5, interiorRect.origin.x, 1.0, interiorRect.size.height);
		
		[[NSColor colorWithCalibratedRed:0.2 green:0.2 blue:1.0 alpha:0.2] set];
		NSRectFillUsingOperation(substitutionRect, NSCompositeSourceOver);
	}
	
	// Draw the fitness history
#if 0
	// line plot
	NSBezierPath *linePath = [NSBezierPath bezierPath];
	double *history = pop.fitnessHistory;
	uint32 historyLength = pop.fitnessHistoryLength;
	BOOL startedPath = NO;
	
	for (int i = 0; i < historyLength; ++i)
	{
		double historyEntry = history[i];
		
		if (!isnan(historyEntry))
		{
			NSPoint historyPoint = NSMakePoint([self plotToDeviceX:i withInteriorRect:interiorRect], [self plotToDeviceY:historyEntry withInteriorRect:interiorRect]);
			
			if (!startedPath)
			{
				[linePath moveToPoint:historyPoint];
				startedPath = YES;
			}
			else
			{
				[linePath lineToPoint:historyPoint];
			}
		}
	}
	
	[linePath setLineWidth:1.0];
	[[NSColor blackColor] set];
	[linePath stroke];
#else
	// scatter plot; better suited to caching of the image
	double *history = pop.fitnessHistory;
	uint32 historyLength = pop.fitnessHistoryLength;
	
	[[NSColor blackColor] set];
	
	// If we're caching now, draw all points; otherwise, if we have a cache, draw only additional points
	int firstHistoryEntryToDraw = (cachingNow ? 0 : (drawingCache ? drawingCacheGeneration : 0));
	
	for (int i = firstHistoryEntryToDraw; (i < historyLength) && (i < completedGenerations); ++i)
	{
		double historyEntry = history[i];
		
		if (!isnan(historyEntry))
		{
			NSPoint historyPoint = NSMakePoint([self plotToDeviceX:i withInteriorRect:interiorRect], [self plotToDeviceY:historyEntry withInteriorRect:interiorRect]);
			
			NSRectFill(NSMakeRect(historyPoint.x - 0.5, historyPoint.y - 0.5, 1.0, 1.0));
		}
	}
#endif
}

- (NSString *)stringForDataWithController:(SLiMWindowController *)controller
{
	NSMutableString *string = [NSMutableString stringWithString:@"# Graph data: fitness ~ generation\n"];
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	int completedGenerations = sim->generation_ - 1;
	
	[string appendString:[self dateline]];
	
	// Fixation events
	[string appendString:@"\n\n# Fixation generations:\n"];
	
	std::vector<const Substitution*> &substitutions = pop.substitutions_;
	
	for (const Substitution *substitution : substitutions)
	{
		int fixation_time = substitution->fixation_time_;
		
		[string appendFormat:@"%d, ", fixation_time];
	}
	
	// Fitness history
	[string appendString:@"\n\n# Fitness history:\n"];
	
	double *history = pop.fitnessHistory;
	uint32 historyLength = pop.fitnessHistoryLength;
	
	for (int i = 0; (i < historyLength) && (i < completedGenerations); ++i)
		[string appendFormat:@"%.4f, ", history[i]];
	
	[string appendString:@"\n"];
	
	// Get rid of extra commas
	[string replaceOccurrencesOfString:@", \n" withString:@"\n" options:0 range:NSMakeRange(0, [string length])];
	
	return string;
}

@end





























































