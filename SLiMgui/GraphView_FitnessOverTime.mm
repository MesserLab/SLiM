//
//  GraphView_FitnessOverTime.m
//  SLiM
//
//  Created by Ben Haller on 3/1/15.
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
		[self setXAxisRangeFromGeneration];
		[self setDefaultYAxisRange];
		
		[self setXAxisLabelString:@"Generation"];
		[self setYAxisLabelString:@"Fitness (rescaled absolute)"];
		
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
			[self setXAxisRangeFromGeneration];
		
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
	SLiMWindowController *controller = [self slimWindowController];
	
	if (![controller invalidSimulation] && ![self yAxisIsUserRescaled])
	{
		SLiMSim *sim = controller->sim;
		Population &pop = sim->population_;
		double minHistory = INFINITY;
		double maxHistory = -INFINITY;
		BOOL showSubpops = [self showSubpopulations] && (pop.fitness_histories_.size() > 2);
		
		for (auto history_record_iter : pop.fitness_histories_)
		{
			if (showSubpops || (history_record_iter.first == -1))
			{
				FitnessHistory &history_record = history_record_iter.second;
				double *history = history_record.history_;
				slim_generation_t historyLength = history_record.history_length_;
				
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
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	slim_generation_t completedGenerations = sim->generation_ - 1;
	
	// The generation counter can get set backwards, in which case our drawing cache is invalid – it contains drawing of things in the
	// future that may no longer happen.  So we need to detect that case and invalidate our cache.
	if (!cachingNow && drawingCache && (drawingCacheGeneration > completedGenerations))
	{
		//NSLog(@"backward generation change detected, invalidating drawing cache");
		[self invalidateDrawingCache];
	}
	
	// If we're not caching, then: if our cache is invalid OR we have crossed a 1000-generation boundary since we last cached, cache an image
	if (!cachingNow && (!drawingCache || ((completedGenerations / 1000) > (drawingCacheGeneration / 1000))))
	{
		[self invalidateDrawingCache];
		
		NSBitmapImageRep *bitmap = [self bitmapImageRepForCachingDisplayInRect:interiorRect];
		
		cachingNow = YES;
		//NSLog(@"recaching!  completedGenerations == %d", completedGenerations);
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
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	for (const Substitution *substitution : substitutions)
	{
		slim_generation_t fixation_gen = substitution->fixation_generation_;
		
		// If we are caching, draw all events; if we are not, draw only those that are not already in the cache
		if (!cachingNow && (fixation_gen < drawingCacheGeneration))
			continue;
		
		double substitutionX = [self plotToDeviceX:fixation_gen withInteriorRect:interiorRect];
		NSRect substitutionRect = NSMakeRect(substitutionX - 0.5, interiorRect.origin.x, 1.0, interiorRect.size.height);
		
		[[NSColor colorWithCalibratedRed:0.2 green:0.2 blue:1.0 alpha:0.2] set];
		NSRectFillUsingOperation(substitutionRect, NSCompositeSourceOver);
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
				slim_generation_t historyLength = history_record.history_length_;
				
				if (drawSubpopsGray)
					[[NSColor colorWithCalibratedWhite:0.5 alpha:1.0] set];
				else
					[[SLiMWindowController whiteContrastingColorForIndex:history_record_iter.first] set];
				
				// If we're caching now, draw all points; otherwise, if we have a cache, draw only additional points
				slim_generation_t firstHistoryEntryToDraw = (cachingNow ? 0 : (drawingCache ? drawingCacheGeneration : 0));
				
				for (slim_generation_t i = firstHistoryEntryToDraw; (i < historyLength) && (i < completedGenerations); ++i)
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
			slim_generation_t historyLength = history_record.history_length_;
			
			[[NSColor blackColor] set];
			
			// If we're caching now, draw all points; otherwise, if we have a cache, draw only additional points
			slim_generation_t firstHistoryEntryToDraw = (cachingNow ? 0 : (drawingCache ? drawingCacheGeneration : 0));
			
			for (slim_generation_t i = firstHistoryEntryToDraw; (i < historyLength) && (i < completedGenerations); ++i)
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
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	slim_generation_t completedGenerations = sim->generation_ - 1;
	
	// Draw fixation events
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	for (const Substitution *substitution : substitutions)
	{
		slim_generation_t fixation_gen = substitution->fixation_generation_;
		double substitutionX = [self plotToDeviceX:fixation_gen withInteriorRect:interiorRect];
		NSRect substitutionRect = NSMakeRect(substitutionX - 0.5, interiorRect.origin.x, 1.0, interiorRect.size.height);
		
		[[NSColor colorWithCalibratedRed:0.2 green:0.2 blue:1.0 alpha:0.2] set];
		NSRectFillUsingOperation(substitutionRect, NSCompositeSourceOver);
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
				slim_generation_t historyLength = history_record.history_length_;
				NSBezierPath *linePath = [NSBezierPath bezierPath];
				BOOL startedLine = NO;
				
				for (slim_generation_t i = 0; (i < historyLength) && (i < completedGenerations); ++i)
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
			slim_generation_t historyLength = history_record.history_length_;
			NSBezierPath *linePath = [NSBezierPath bezierPath];
			BOOL startedLine = NO;
			
			for (slim_generation_t i = 0; (i < historyLength) && (i < completedGenerations); ++i)
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
	NSMutableString *string = [NSMutableString stringWithString:@"# Graph data: fitness ~ generation\n"];
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	slim_generation_t completedGenerations = sim->generation_ - 1;
	
	[string appendString:[self dateline]];
	
	// Fixation events
	[string appendString:@"\n\n# Fixation generations:\n"];
	
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	for (const Substitution *substitution : substitutions)
	{
		slim_generation_t fixation_gen = substitution->fixation_generation_;
		
		[string appendFormat:@"%lld, ", (int64_t)fixation_gen];
	}
	
	// Fitness history
	[string appendString:@"\n\n# Fitness history:\n"];
	
	for (auto history_record_iter : pop.fitness_histories_)
	{
		if (history_record_iter.first == -1)
		{
			FitnessHistory &history_record = history_record_iter.second;
			double *history = history_record.history_;
			slim_generation_t historyLength = history_record.history_length_;
			
			for (slim_generation_t i = 0; (i < historyLength) && (i < completedGenerations); ++i)
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
				slim_generation_t historyLength = history_record.history_length_;
				
				[string appendFormat:@"\n\n# Fitness history (subpopulation p%d):\n", history_record_iter.first];
				
				for (slim_generation_t i = 0; (i < historyLength) && (i < completedGenerations); ++i)
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
	SLiMWindowController *controller = [self slimWindowController];
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
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
				NSString *labelString = [NSString stringWithFormat:@"p%lld", (int64_t)history_record_iter.first];
				
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





























































