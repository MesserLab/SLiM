//
//  GraphView_FitnessOverTime.m
//  SLiM
//
//  Created by Ben Haller on 3/1/15.
//  Copyright (c) 2015-2023 Philipp Messer.  All rights reserved.
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


#import "GraphView_FitnessOverTime.h"
#import "SLiMWindowController.h"

#include "community.h"


@implementation GraphView_FitnessOverTime

- (void)invalidateDrawingCache
{
	[drawingCache release];
	drawingCache = nil;
	drawingCacheTick = 0;
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
		[self setXAxisRangeFromTick];
		[self setDefaultYAxisRange];
		
		[self setXAxisLabelString:@"Tick"];
		[self setYAxisLabelString:@"Fitness (rescaled)"];
		
		[self setAllowXAxisUserRescale:YES];
		[self setAllowYAxisUserRescale:YES];
		
		[self setShowHorizontalGridLines:YES];
		[self setTweakXAxisTickLabelAlignment:YES];
		
		[self setShowSubpopulations:YES];
	}
	
	return self;
}

- (void)controllerRecycled
{
	SLiMWindowController *controller = [self slimWindowController];
	
	if (![controller invalidSimulation])
	{
		if (![self yAxisIsUserRescaled])
			[self setDefaultYAxisRange];
		if (![self xAxisIsUserRescaled])
			[self setXAxisRangeFromTick];
		
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
	[super dealloc];
}

- (void)updateAfterTick
{
	Species *displaySpecies = [self focalDisplaySpecies];
	
	if (displaySpecies && ![self yAxisIsUserRescaled])
	{
		Population &pop = displaySpecies->population_;
		double minHistory = INFINITY;
		double maxHistory = -INFINITY;
		BOOL showSubpops = [self showSubpopulations] && (pop.fitness_histories_.size() > 2);
		
		for (auto history_record_iter : pop.fitness_histories_)
		{
			if (showSubpops || (history_record_iter.first == -1))
			{
				FitnessHistory &history_record = history_record_iter.second;
				double *history = history_record.history_;
				slim_tick_t historyLength = history_record.history_length_;
				
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
	
	[super updateAfterTick];
}

- (void)drawPointGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller
{
	Species *displaySpecies = [self focalDisplaySpecies];
	Population &pop = displaySpecies->population_;
	slim_tick_t completedTicks = controller->community->Tick() - 1;
	
	// The tick counter can get set backwards, in which case our drawing cache is invalid – it contains drawing of things in the
	// future that may no longer happen.  So we need to detect that case and invalidate our cache.
	if (!cachingNow && drawingCache && (drawingCacheTick > completedTicks))
	{
		//NSLog(@"backward tick change detected, invalidating drawing cache");
		[self invalidateDrawingCache];
	}
	
	// If we're not caching, then: if our cache is invalid OR we have crossed a 1000-tick boundary since we last cached, cache an image
	if (!cachingNow && (!drawingCache || ((completedTicks / 1000) > (drawingCacheTick / 1000))))
	{
		[self invalidateDrawingCache];
		
		NSBitmapImageRep *bitmap = [self bitmapImageRepForCachingDisplayInRect:interiorRect];
		
		cachingNow = YES;
		//NSLog(@"recaching!  completedTicks == %d", completedTicks);
		[bitmap setSize:interiorRect.size];
		[self cacheDisplayInRect:interiorRect toBitmapImageRep:bitmap];
		drawingCache = [[NSImage alloc] initWithSize:interiorRect.size];
		[drawingCache addRepresentation:bitmap];
		drawingCacheTick = completedTicks;
		cachingNow = NO;
	}
	
	// Now draw our cache, if we have one
	if (drawingCache)
		[drawingCache drawInRect:interiorRect];
	
	// Draw fixation events
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	for (const Substitution *substitution : substitutions)
	{
		slim_tick_t fixation_tick = substitution->fixation_tick_;
		
		// If we are caching, draw all events; if we are not, draw only those that are not already in the cache
		if (!cachingNow && (fixation_tick < drawingCacheTick))
			continue;
		
		double substitutionX = [self plotToDeviceX:fixation_tick withInteriorRect:interiorRect];
		NSRect substitutionRect = NSMakeRect(substitutionX - 0.5, interiorRect.origin.x, 1.0, interiorRect.size.height);
		
		[[NSColor colorWithCalibratedRed:0.2 green:0.2 blue:1.0 alpha:0.2] set];
		NSRectFillUsingOperation(substitutionRect, NSCompositingOperationSourceOver);
	}
	
	// Draw the fitness history as a scatter plot; better suited to caching of the image
	BOOL showSubpops = [self showSubpopulations] && (pop.fitness_histories_.size() > 2);
	BOOL drawSubpopsGray = (showSubpops && (pop.fitness_histories_.size() > 8));	// 7 subpops + pop
	
	// First draw subpops
	if (showSubpops)
	{
		for (auto history_record_iter : pop.fitness_histories_)
		{
			if (history_record_iter.first != -1)
			{
				FitnessHistory &history_record = history_record_iter.second;
				double *history = history_record.history_;
				slim_tick_t historyLength = history_record.history_length_;
				
				if (drawSubpopsGray)
					[[NSColor colorWithCalibratedWhite:0.5 alpha:1.0] set];
				else
					[[SLiMWindowController whiteContrastingColorForIndex:history_record_iter.first] set];
				
				// If we're caching now, draw all points; otherwise, if we have a cache, draw only additional points
				slim_tick_t firstHistoryEntryToDraw = (cachingNow ? 0 : (drawingCache ? drawingCacheTick : 0));
				
				for (slim_tick_t i = firstHistoryEntryToDraw; (i < historyLength) && (i < completedTicks); ++i)
				{
					double historyEntry = history[i];
					
					if (!isnan(historyEntry))
					{
						NSPoint historyPoint = NSMakePoint([self plotToDeviceX:i withInteriorRect:interiorRect], [self plotToDeviceY:historyEntry withInteriorRect:interiorRect]);
						
						NSRectFill(NSMakeRect(historyPoint.x - 0.5, historyPoint.y - 0.5, 1.0, 1.0));
					}
				}
			}
		}
	}
	
	// Then draw the mean population fitness
	for (auto history_record_iter : pop.fitness_histories_)
	{
		if (history_record_iter.first == -1)
		{
			FitnessHistory &history_record = history_record_iter.second;
			double *history = history_record.history_;
			slim_tick_t historyLength = history_record.history_length_;
			
			[[NSColor blackColor] set];
			
			// If we're caching now, draw all points; otherwise, if we have a cache, draw only additional points
			slim_tick_t firstHistoryEntryToDraw = (cachingNow ? 0 : (drawingCache ? drawingCacheTick : 0));
			
			for (slim_tick_t i = firstHistoryEntryToDraw; (i < historyLength) && (i < completedTicks); ++i)
			{
				double historyEntry = history[i];
				
				if (!isnan(historyEntry))
				{
					NSPoint historyPoint = NSMakePoint([self plotToDeviceX:i withInteriorRect:interiorRect], [self plotToDeviceY:historyEntry withInteriorRect:interiorRect]);
					
					NSRectFill(NSMakeRect(historyPoint.x - 0.5, historyPoint.y - 0.5, 1.0, 1.0));
				}
			}
		}
	}
}

- (void)drawLineGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller
{
	Species *displaySpecies = [self focalDisplaySpecies];
	Population &pop = displaySpecies->population_;
	slim_tick_t completedTicks = controller->community->Tick() - 1;
	
	// Draw fixation events
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	for (const Substitution *substitution : substitutions)
	{
		slim_tick_t fixation_tick = substitution->fixation_tick_;
		double substitutionX = [self plotToDeviceX:fixation_tick withInteriorRect:interiorRect];
		NSRect substitutionRect = NSMakeRect(substitutionX - 0.5, interiorRect.origin.x, 1.0, interiorRect.size.height);
		
		[[NSColor colorWithCalibratedRed:0.2 green:0.2 blue:1.0 alpha:0.2] set];
		NSRectFillUsingOperation(substitutionRect, NSCompositingOperationSourceOver);
	}
	
	// Draw the fitness history as a scatter plot; better suited to caching of the image
	BOOL showSubpops = [self showSubpopulations] && (pop.fitness_histories_.size() > 2);
	BOOL drawSubpopsGray = (showSubpops && (pop.fitness_histories_.size() > 8));	// 7 subpops + pop
	
	// First draw subpops
	if (showSubpops)
	{
		for (auto history_record_iter : pop.fitness_histories_)
		{
			if (history_record_iter.first != -1)
			{
				FitnessHistory &history_record = history_record_iter.second;
				double *history = history_record.history_;
				slim_tick_t historyLength = history_record.history_length_;
				NSBezierPath *linePath = [NSBezierPath bezierPath];
				BOOL startedLine = NO;
				
				for (slim_tick_t i = 0; (i < historyLength) && (i < completedTicks); ++i)
				{
					double historyEntry = history[i];
					
					if (isnan(historyEntry))
					{
						startedLine = NO;
					}
					else
					{
						NSPoint historyPoint = NSMakePoint([self plotToDeviceX:i withInteriorRect:interiorRect], [self plotToDeviceY:historyEntry withInteriorRect:interiorRect]);
						
						if (startedLine)	[linePath lineToPoint:historyPoint];
						else				[linePath moveToPoint:historyPoint];
						
						startedLine = YES;
					}
				}
				
				if (drawSubpopsGray)
					[[NSColor colorWithCalibratedWhite:0.5 alpha:1.0] set];
				else
					[[SLiMWindowController whiteContrastingColorForIndex:history_record_iter.first] set];
				
				[linePath setLineWidth:1.0];
				[linePath stroke];
			}
		}
	}
	
	// Then draw the mean population fitness
	for (auto history_record_iter : pop.fitness_histories_)
	{
		if (history_record_iter.first == -1)
		{
			FitnessHistory &history_record = history_record_iter.second;
			double *history = history_record.history_;
			slim_tick_t historyLength = history_record.history_length_;
			NSBezierPath *linePath = [NSBezierPath bezierPath];
			BOOL startedLine = NO;
			
			for (slim_tick_t i = 0; (i < historyLength) && (i < completedTicks); ++i)
			{
				double historyEntry = history[i];
				
				if (isnan(historyEntry))
				{
					startedLine = NO;
				}
				else
				{
					NSPoint historyPoint = NSMakePoint([self plotToDeviceX:i withInteriorRect:interiorRect], [self plotToDeviceY:historyEntry withInteriorRect:interiorRect]);
					
					if (startedLine)	[linePath lineToPoint:historyPoint];
					else				[linePath moveToPoint:historyPoint];
					
					startedLine = YES;
				}
			}
			
			[[NSColor blackColor] set];
			[linePath setLineWidth:1.5];
			[linePath stroke];
		}
	}
}

- (void)drawGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller
{
	if ([self drawLines])
		[self drawLineGraphInInteriorRect:interiorRect withController:controller];
	else
		[self drawPointGraphInInteriorRect:interiorRect withController:controller];
}

- (NSString *)stringForDataWithController:(SLiMWindowController *)controller
{
	NSMutableString *string = [NSMutableString stringWithString:@"# Graph data: fitness ~ tick\n"];
	Species *displaySpecies = [self focalDisplaySpecies];
	Population &pop = displaySpecies->population_;
	slim_tick_t completedTicks = controller->community->Tick() - 1;
	
	[string appendString:[self dateline]];
	
	// Fixation events
	[string appendString:@"\n\n# Fixation ticks:\n"];
	
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	for (const Substitution *substitution : substitutions)
	{
		slim_tick_t fixation_tick = substitution->fixation_tick_;
		
		[string appendFormat:@"%lld, ", (long long int)fixation_tick];
	}
	
	// Fitness history
	[string appendString:@"\n\n# Fitness history:\n"];
	
	for (auto history_record_iter : pop.fitness_histories_)
	{
		if (history_record_iter.first == -1)
		{
			FitnessHistory &history_record = history_record_iter.second;
			double *history = history_record.history_;
			slim_tick_t historyLength = history_record.history_length_;
			
			for (slim_tick_t i = 0; (i < historyLength) && (i < completedTicks); ++i)
				[string appendFormat:@"%.4f, ", history[i]];
			
			[string appendString:@"\n"];
		}
	}
	
	// Subpopulation fitness histories
	BOOL showSubpops = [self showSubpopulations] && (pop.fitness_histories_.size() > 2);
	
	if (showSubpops)
	{
		for (auto history_record_iter : pop.fitness_histories_)
		{
			if (history_record_iter.first != -1)
			{
				FitnessHistory &history_record = history_record_iter.second;
				double *history = history_record.history_;
				slim_tick_t historyLength = history_record.history_length_;
				
				[string appendFormat:@"\n\n# Fitness history (subpopulation p%d):\n", history_record_iter.first];
				
				for (slim_tick_t i = 0; (i < historyLength) && (i < completedTicks); ++i)
					[string appendFormat:@"%.4f, ", history[i]];
				
				[string appendString:@"\n"];
			}
		}
	}
	
	// Get rid of extra commas
	[string replaceOccurrencesOfString:@", \n" withString:@"\n" options:0 range:NSMakeRange(0, [string length])];
	
	return string;
}

- (NSArray *)legendKey
{
	Species *displaySpecies = [self focalDisplaySpecies];
	Population &pop = displaySpecies->population_;
	BOOL showSubpops = [self showSubpopulations] && (pop.fitness_histories_.size() > 2);
	BOOL drawSubpopsGray = (showSubpops && (pop.fitness_histories_.size() > 8));	// 7 subpops + pop
	
	if (!showSubpops)
		return nil;
	
	NSMutableArray *legendKey = [NSMutableArray array];
	
	[legendKey addObject:@[@"All", [NSColor blackColor]]];
	
	if (drawSubpopsGray)
	{
		[legendKey addObject:@[@"pX", [NSColor colorWithCalibratedWhite:0.5 alpha:1.0]]];
	}
	else
	{
		for (auto history_record_iter : pop.fitness_histories_)
		{
			if (history_record_iter.first != -1)
			{
				NSString *labelString = [NSString stringWithFormat:@"p%lld", (long long int)history_record_iter.first];
				
				[legendKey addObject:@[labelString, [SLiMWindowController whiteContrastingColorForIndex:history_record_iter.first]]];
			}
		}
	}
	
	return legendKey;
}

- (IBAction)toggleShowSubpopulations:(id)sender
{
	[self setShowSubpopulations:![self showSubpopulations]];
	[self invalidateDrawingCache];
	[self setNeedsDisplay:YES];
}

- (IBAction)toggleDrawLines:(id)sender
{
	[self setDrawLines:![self drawLines]];
	[self invalidateDrawingCache];
	[self setNeedsDisplay:YES];
}

- (void)subclassAddItemsToMenu:(NSMenu *)menu forEvent:(NSEvent *)theEvent
{
	if (menu)
	{
		NSMenuItem *menuItem;
		
		menuItem = [menu addItemWithTitle:([self showSubpopulations] ? @"Hide Subpopulations" : @"Show Subpopulations") action:@selector(toggleShowSubpopulations:) keyEquivalent:@""];
		[menuItem setTarget:self];
		
		menuItem = [menu addItemWithTitle:([self drawLines] ? @"Draw Points (Faster)" : @"Draw Lines (Slower)") action:@selector(toggleDrawLines:) keyEquivalent:@""];
		[menuItem setTarget:self];
	}
}

@end





























































