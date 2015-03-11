//
//  GraphView.m
//  SLiM
//
//  Created by Ben Haller on 2/27/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
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


#import "GraphView.h"
#import "SLiMWindowController.h"


@implementation GraphView

+ (NSString *)labelFontName
{
	return @"Times New Roman";
}

+ (NSDictionary *)attributesForAxisLabels
{
	static NSDictionary *attrs = nil;
	
	if (!attrs)
		attrs = [[NSDictionary alloc] initWithObjectsAndKeys:[NSFont fontWithName:[self labelFontName] size:14], NSFontAttributeName, nil];
	
	return attrs;
}

+ (NSDictionary *)attributesForTickLabels
{
	static NSDictionary *attrs = nil;
	
	if (!attrs)
		attrs = [[NSDictionary alloc] initWithObjectsAndKeys:[NSFont fontWithName:[self labelFontName] size:10], NSFontAttributeName, nil];
	
	return attrs;
}

+ (NSDictionary *)attributesForLegendLabels
{
	static NSDictionary *attrs = nil;
	
	if (!attrs)
		attrs = [[NSDictionary alloc] initWithObjectsAndKeys:[NSFont fontWithName:[self labelFontName] size:10], NSFontAttributeName, nil];
	
	return attrs;
}

+ (NSColor *)gridLineColor
{
	static NSColor *gridColor = nil;
	
	if (!gridColor)
		gridColor = [[NSColor colorWithCalibratedWhite:0.85 alpha:1.0] retain];
	
	return gridColor;
}

- (id)initWithFrame:(NSRect)frameRect withController:(SLiMWindowController *)controller
{
	if (self = [super initWithFrame:frameRect])
	{
		[self setSlimWindowController:controller];
		
		_showXAxis = TRUE;
		_allowXAxisUserRescale = TRUE;
		_showXAxisTicks = TRUE;
		
		_showYAxis = TRUE;
		_allowYAxisUserRescale = TRUE;
		_showYAxisTicks = TRUE;
		
		_xAxisMin = 0.0;
		_xAxisMax = 1.0;
		_xAxisMajorTickInterval = 0.5;
		_xAxisMinorTickInterval = 0.25;
		_xAxisMajorTickModulus = 2;
		_xAxisTickValuePrecision = 1;

		_yAxisMin = 0.0;
		_yAxisMax = 1.0;
		_yAxisMajorTickInterval = 0.5;
		_yAxisMinorTickInterval = 0.25;
		_yAxisMajorTickModulus = 2;
		_yAxisTickValuePrecision = 1;
		
		_xAxisLabel = [[NSAttributedString alloc] initWithString:@"This is the x-axis, yo" attributes:[[self class] attributesForAxisLabels]];
		_yAxisLabel = [[NSAttributedString alloc] initWithString:@"This is the y-axis, yo" attributes:[[self class] attributesForAxisLabels]];
		
		_legendVisible = YES;
		_showHorizontalGridLines = NO;
		_showVerticalGridLines = NO;
		_showFullBox = NO;
	}
	
	return self;
}

- (void)dealloc
{
	//NSLog(@"[GraphView dealloc]");
	[self invalidateDrawingCache];
	
	[_xAxisLabel release];
	_xAxisLabel = nil;
	
	[_yAxisLabel release];
	_yAxisLabel = nil;
	
	[super dealloc];
}

- (void)setXAxisLabelString:(NSString *)labelString
{
	[self setXAxisLabel:[[[NSAttributedString alloc] initWithString:labelString attributes:[[self class] attributesForAxisLabels]] autorelease]];
}

- (void)setYAxisLabelString:(NSString *)labelString
{
	[self setYAxisLabel:[[[NSAttributedString alloc] initWithString:labelString attributes:[[self class] attributesForAxisLabels]] autorelease]];
}

- (NSRect)interiorRectForBounds:(NSRect)bounds
{
	NSRect interiorRect = bounds;
	
	// For now, 10 pixels margin on a side if there is no axis, 40 pixels margin if there is an axis
	
	if ([self showXAxis])
	{
		interiorRect.origin.x += 50;
		interiorRect.size.width -= 60;
	}
	else
	{
		interiorRect.origin.x += 10;
		interiorRect.size.width -= 20;
	}
	
	if ([self showYAxis])
	{
		interiorRect.origin.y += 50;
		interiorRect.size.height -= 60;
	}
	else
	{
		interiorRect.origin.y += 10;
		interiorRect.size.height -= 20;
	}
	
	return interiorRect;
}

- (double)plotToDeviceX:(double)plotx withInteriorRect:(NSRect)interiorRect
{
	double fractionAlongAxis = (plotx - _xAxisMin) / (_xAxisMax - _xAxisMin);
	
	// We go from the center of the first pixel to the center of the last pixel
	return (fractionAlongAxis * (interiorRect.size.width - 1.0) + interiorRect.origin.x) + 0.5;
}

- (double)plotToDeviceY:(double)ploty withInteriorRect:(NSRect)interiorRect
{
	double fractionAlongAxis = (ploty - _yAxisMin) / (_yAxisMax - _yAxisMin);
	
	// We go from the center of the first pixel to the center of the last pixel
	return (fractionAlongAxis * (interiorRect.size.height - 1.0) + interiorRect.origin.y) + 0.5;
}

- (double)roundPlotToDeviceX:(double)plotx withInteriorRect:(NSRect)interiorRect
{
	double fractionAlongAxis = (plotx - _xAxisMin) / (_xAxisMax - _xAxisMin);
	
	// We go from the center of the first pixel to the center of the last pixel, rounded off to pixel midpoints
	return SCREEN_ROUND(fractionAlongAxis * (interiorRect.size.width - 1.0) + interiorRect.origin.x) + 0.5;
}

- (double)roundPlotToDeviceY:(double)ploty withInteriorRect:(NSRect)interiorRect
{
	double fractionAlongAxis = (ploty - _yAxisMin) / (_yAxisMax - _yAxisMin);
	
	// We go from the center of the first pixel to the center of the last pixel, rounded off to pixel midpoints
	return SCREEN_ROUND(fractionAlongAxis * (interiorRect.size.height - 1.0) + interiorRect.origin.y) + 0.5;
}

- (void)rescaleAsNeededWithInteriorRect:(NSRect)interiorRect andController:(SLiMWindowController *)controller
{
}

- (void)drawXAxisTicksWithInteriorRect:(NSRect)interiorRect
{
	NSDictionary *tickAttributes = [[self class] attributesForTickLabels];
	NSBezierPath *path = [NSBezierPath bezierPath];
	double axisMin = [self xAxisMin];
	double axisMax = [self xAxisMax];
	double minorTickInterval = [self xAxisMinorTickInterval];
	int majorTickModulus = [self xAxisMajorTickModulus];
	int tickValuePrecision = [self xAxisTickValuePrecision];
	double tickValue;
	int tickIndex;
	
	for (tickValue = axisMin, tickIndex = 0; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += minorTickInterval, tickIndex++)
	{
		BOOL isMajorTick = ((tickIndex % majorTickModulus) == 0);
		double xValueForTick = SCREEN_ROUND(interiorRect.origin.x + (interiorRect.size.width - 1) * ((tickValue - axisMin) / (axisMax - axisMin))) + 0.5;
		
		//NSLog(@"tickValue == %f, isMajorTick == %@, xValueForTick == %f", tickValue, isMajorTick ? @"YES" : @"NO", xValueForTick);
		
		[path moveToPoint:NSMakePoint(xValueForTick, interiorRect.origin.y - (isMajorTick ? 6 : 3))];
		[path lineToPoint:NSMakePoint(xValueForTick, interiorRect.origin.y - 0.5)];
		
		if (isMajorTick)
		{
			double labelValueForTick;
			
			labelValueForTick = tickValue;
			
			NSString *labelText = [NSString stringWithFormat:@"%.*f", tickValuePrecision, labelValueForTick];
			NSAttributedString *attributedLabel = [[NSMutableAttributedString alloc] initWithString:labelText attributes:tickAttributes];
			NSSize labelSize = [attributedLabel size];
			double labelY = interiorRect.origin.y - (labelSize.height + 4);
			
			if ([self tweakXAxisTickLabelAlignment])
			{
				if (tickValue == axisMin)
					[attributedLabel drawAtPoint:NSMakePoint(xValueForTick - 2.0, labelY)];
				else if (tickValue == axisMax)
					[attributedLabel drawAtPoint:NSMakePoint(xValueForTick - SCREEN_ROUND(labelSize.width) + 2.0, labelY)];
				else
					[attributedLabel drawAtPoint:NSMakePoint(xValueForTick - SCREEN_ROUND(labelSize.width / 2.0), labelY)];
			}
			else
			{
				[attributedLabel drawAtPoint:NSMakePoint(xValueForTick - SCREEN_ROUND(labelSize.width / 2.0), labelY)];
			}
			[attributedLabel release];
		}
	}
	
	[path setLineWidth:1.0];
	[[NSColor blackColor] set];
	[path stroke];
}

- (void)drawXAxisWithInteriorRect:(NSRect)interiorRect
{
	int yAxisFudge = [self showYAxis] ? 1 : 0;
	NSRect axisRect = NSMakeRect(interiorRect.origin.x - yAxisFudge, interiorRect.origin.y - 1, interiorRect.size.width + yAxisFudge, 1);
	
	[[NSColor blackColor] set];
	NSRectFill(axisRect);
	
	// show label
	NSAttributedString *label = [self xAxisLabel];
	NSSize labelSize = [label size];
	
	[label drawAtPoint:NSMakePoint(interiorRect.origin.x + (interiorRect.size.width - labelSize.width) / 2.0, interiorRect.origin.y - 45)];
}

- (void)drawYAxisTicksWithInteriorRect:(NSRect)interiorRect
{
	NSDictionary *tickAttributes = [[self class] attributesForTickLabels];
	NSBezierPath *path = [NSBezierPath bezierPath];
	double axisMin = [self yAxisMin];
	double axisMax = [self yAxisMax];
	double minorTickInterval = [self yAxisMinorTickInterval];
	int majorTickModulus = [self yAxisMajorTickModulus];
	int tickValuePrecision = [self yAxisTickValuePrecision];
	double tickValue;
	int tickIndex;
	
	for (tickValue = axisMin, tickIndex = 0; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += minorTickInterval, tickIndex++)
	{
		BOOL isMajorTick = ((tickIndex % majorTickModulus) == 0);
		double yValueForTick = SCREEN_ROUND(interiorRect.origin.y + (interiorRect.size.height - 1) * ((tickValue - axisMin) / (axisMax - axisMin))) + 0.5;
		
		//NSLog(@"tickValue == %f, isMajorTick == %@, yValueForTick == %f", tickValue, isMajorTick ? @"YES" : @"NO", yValueForTick);
		
		[path moveToPoint:NSMakePoint(interiorRect.origin.x - (isMajorTick ? 6 : 3), yValueForTick)];
		[path lineToPoint:NSMakePoint(interiorRect.origin.x - 0.5, yValueForTick)];
		
		if (isMajorTick)
		{
			double labelValueForTick;
			
			labelValueForTick = tickValue;
			
			NSString *labelText = [NSString stringWithFormat:@"%.*f", tickValuePrecision, labelValueForTick];
			NSAttributedString *attributedLabel = [[NSMutableAttributedString alloc] initWithString:labelText attributes:tickAttributes];
			NSSize labelSize = [attributedLabel size];
			
			[attributedLabel drawAtPoint:NSMakePoint(interiorRect.origin.x - (labelSize.width + 8), yValueForTick - SCREEN_ROUND(labelSize.height / 2.0) + 2)];
			[attributedLabel release];
		}
	}
	
	[path setLineWidth:1.0];
	[[NSColor blackColor] set];
	[path stroke];
}

- (void)drawYAxisWithInteriorRect:(NSRect)interiorRect
{
	int xAxisFudge = [self showXAxis] ? 1 : 0;
	NSRect axisRect = NSMakeRect(interiorRect.origin.x - 1, interiorRect.origin.y - xAxisFudge, 1, interiorRect.size.height + xAxisFudge);
	
	[[NSColor blackColor] set];
	NSRectFill(axisRect);
	
	// show label, rotated
	NSAttributedString *label = [self yAxisLabel];
	NSSize labelSize = [label size];
	
	[NSGraphicsContext saveGraphicsState];
	
	NSAffineTransform *transform = [NSAffineTransform transform];
	[transform translateXBy:interiorRect.origin.x - 30 yBy:interiorRect.origin.y + SCREEN_ROUND(interiorRect.size.height / 2.0)];
	[transform rotateByDegrees:90];
	[transform concat];
	[label drawAtPoint:NSMakePoint(SCREEN_ROUND(-labelSize.width / 2.0), 0)];
	
	[NSGraphicsContext restoreGraphicsState];
}

- (void)drawFullBoxWithInteriorRect:(NSRect)interiorRect
{
	NSRect axisRect;
	
	// upper x axis
	int yAxisFudge = [self showYAxis] ? 1 : 0;
	
	axisRect = NSMakeRect(interiorRect.origin.x - yAxisFudge, interiorRect.origin.y + interiorRect.size.height, interiorRect.size.width + yAxisFudge + 1, 1);
	
	[[NSColor blackColor] set];
	NSRectFill(axisRect);
	
	// right-hand y axis
	int xAxisFudge = [self showXAxis] ? 1 : 0;
	
	axisRect = NSMakeRect(interiorRect.origin.x + interiorRect.size.width, interiorRect.origin.y - xAxisFudge, 1, interiorRect.size.height + xAxisFudge + 1);
	
	[[NSColor blackColor] set];
	NSRectFill(axisRect);
}

- (void)drawVerticalGridLinesWithInteriorRect:(NSRect)interiorRect
{
	NSBezierPath *path = [NSBezierPath bezierPath];
	double axisMin = [self xAxisMin];
	double axisMax = [self xAxisMax];
	double minorTickInterval = [self xAxisMinorTickInterval];
	double tickValue;
	int tickIndex;
	
	for (tickValue = axisMin, tickIndex = 0; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += minorTickInterval, tickIndex++)
	{
		double xValueForTick = SCREEN_ROUND(interiorRect.origin.x + (interiorRect.size.width - 1) * ((tickValue - axisMin) / (axisMax - axisMin))) + 0.5;
		
		if (fabs((xValueForTick - 0.5) - interiorRect.origin.x) < 0.001)
			continue;
		if ((fabs((xValueForTick - 0.5) - (interiorRect.origin.x + interiorRect.size.width - 1)) < 0.001) && [self showFullBox])
			continue;
		
		[path moveToPoint:NSMakePoint(xValueForTick, interiorRect.origin.y - 0.01)];
		[path lineToPoint:NSMakePoint(xValueForTick, interiorRect.origin.y + interiorRect.size.height + 0.01)];
	}
	
	[path setLineWidth:1.0];
	[[[self class] gridLineColor] set];
	[path stroke];
}

- (void)drawHorizontalGridLinesWithInteriorRect:(NSRect)interiorRect
{
	NSBezierPath *path = [NSBezierPath bezierPath];
	double axisMin = [self yAxisMin];
	double axisMax = [self yAxisMax];
	double minorTickInterval = [self yAxisMinorTickInterval];
	double tickValue;
	int tickIndex;
	
	for (tickValue = axisMin, tickIndex = 0; tickValue <= (axisMax + minorTickInterval / 10.0); tickValue += minorTickInterval, tickIndex++)
	{
		double yValueForTick = SCREEN_ROUND(interiorRect.origin.y + (interiorRect.size.height - 1) * ((tickValue - axisMin) / (axisMax - axisMin))) + 0.5;
		
		if (fabs((yValueForTick - 0.5) - interiorRect.origin.y) < 0.001)
			continue;
		if ((fabs((yValueForTick - 0.5) - (interiorRect.origin.y + interiorRect.size.height - 1)) < 0.001) && [self showFullBox])
			continue;
		
		[path moveToPoint:NSMakePoint(interiorRect.origin.x - 0.01, yValueForTick)];
		[path lineToPoint:NSMakePoint(interiorRect.origin.x + interiorRect.size.width + 0.01, yValueForTick)];
	}
	
	[path setLineWidth:1.0];
	[[[self class] gridLineColor] set];
	[path stroke];
}

- (void)drawInvalidMessageInRect:(NSRect)rect
{
	static NSAttributedString *invalidMessage = nil;
	
	if (!invalidMessage)
	{
		NSDictionary *attrs = [NSDictionary dictionaryWithObjectsAndKeys:[NSFont fontWithName:[[self class] labelFontName] size:16], NSFontAttributeName, [NSColor colorWithCalibratedWhite:0.4 alpha:1.0], NSForegroundColorAttributeName, nil];
		
		invalidMessage = [[NSAttributedString alloc] initWithString:@"   invalid\nsimulation" attributes:attrs];
	}
	
	NSPoint centerPoint = NSMakePoint(rect.origin.x + rect.size.width / 2, rect.origin.y + rect.size.height / 2);
	NSSize messageSize = [invalidMessage size];
	NSPoint drawPoint = NSMakePoint(SCREEN_ROUND(centerPoint.x - messageSize.width / 2.0), SCREEN_ROUND(centerPoint.y - messageSize.height / 2.0));
	
	[invalidMessage drawAtPoint:drawPoint];
}

- (void)drawGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller
{
	[[NSColor colorWithCalibratedHue:0.15 saturation:0.15 brightness:1.0 alpha:1.0] set];
	NSRectFill(interiorRect);
}

- (NSSize)legendSize
{
	return NSZeroSize;
}

- (void)drawLegendInRect:(NSRect)legendRect
{
}

- (void)drawLegendInInteriorRect:(NSRect)interiorRect
{
	NSSize legendSize = [self legendSize];
	
	legendSize.width = ceil(legendSize.width);
	legendSize.height = ceil(legendSize.height);
	
	if ((legendSize.width > 0.0) && (legendSize.height > 0.0))
	{
		const int legendMargin = 10;
		NSRect legendRect = NSMakeRect(0, 0, legendSize.width + legendMargin + legendMargin, legendSize.height + legendMargin + legendMargin);
		
		// Position the legend in the upper right, for now; this should be configurable
		legendRect.origin.x = interiorRect.origin.x + interiorRect.size.width - legendRect.size.width;
		legendRect.origin.y = interiorRect.origin.y + interiorRect.size.height - legendRect.size.height;
		
		// Frame the legend and erase it with a slightly translucent wash
		[[NSColor colorWithCalibratedWhite:0.95 alpha:0.9] set];
		NSRectFillUsingOperation(legendRect, NSCompositeSourceOver);
		
		[[NSColor colorWithCalibratedWhite:0.3 alpha:1.0] set];
		NSFrameRect(legendRect);
		
		// Inset and draw the legend content
		legendRect = NSInsetRect(legendRect, legendMargin, legendMargin);
		
		[self drawLegendInRect:legendRect];
	}
}

- (void)drawRect:(NSRect)dirtyRect
{
	NSRect bounds = [self bounds];
	
	// Erase background
	[[NSColor whiteColor] set];
	NSRectFill(dirtyRect);
	
	// Get our controller and test for validity, so subclasses don't have to worry about this
	SLiMWindowController *controller = [self slimWindowController];
	
	if (![controller invalidSimulation])
	{
		NSRect interiorRect = [self interiorRectForBounds:bounds];
		
		[self rescaleAsNeededWithInteriorRect:interiorRect andController:controller];
		
		// Draw grid lines, if requested, and if tick marks are turned on for the corresponding axis
		if ([self showHorizontalGridLines] && [self showYAxis] && [self showYAxisTicks])
			[self drawHorizontalGridLinesWithInteriorRect:interiorRect];
		
		if ([self showVerticalGridLines] && [self showXAxis] && [self showXAxisTicks])
			[self drawVerticalGridLinesWithInteriorRect:interiorRect];
		
		// Draw the interior of the graph; this will be overridden by the subclass
		// We clip the interior drawing to the interior rect, so outliers get clipped out
		[NSGraphicsContext saveGraphicsState];
		[[NSBezierPath bezierPathWithRect:NSInsetRect(interiorRect, -0.1, -0.1)] addClip];
		
		[self drawGraphInInteriorRect:interiorRect withController:controller];
		
		[NSGraphicsContext restoreGraphicsState];
		
		// If we're caching, skip all overdrawing, since it cannot be cached (it would then appear under new drawing that supplements the cache)
		if (!cachingNow)
		{
			// Overdraw axes, ticks, and axis labels, if requested
			if ([self showXAxis] && [self showXAxisTicks])
				[self drawXAxisTicksWithInteriorRect:interiorRect];
			
			if ([self showYAxis] && [self showYAxisTicks])
				[self drawYAxisTicksWithInteriorRect:interiorRect];
			
			if ([self showXAxis])
				[self drawXAxisWithInteriorRect:interiorRect];
			
			if ([self showYAxis])
				[self drawYAxisWithInteriorRect:interiorRect];
			
			if ([self showFullBox])
				[self drawFullBoxWithInteriorRect:interiorRect];
			
			// Overdraw the legend
			if ([self legendVisible])
				[self drawLegendInInteriorRect:interiorRect];
		}
	}
	else
	{
		// The controller is invalid, so just draw a generic message
		[self drawInvalidMessageInRect:bounds];
	}
}

- (IBAction)toggleLegend:(id)sender
{
	[self setLegendVisible:![self legendVisible]];
	[self setNeedsDisplay:YES];
}

- (IBAction)toggleHorizontalGridLines:(id)sender
{
	[self setShowHorizontalGridLines:![self showHorizontalGridLines]];
	[self setNeedsDisplay:YES];
}

- (IBAction)toggleVerticalGridLines:(id)sender
{
	[self setShowVerticalGridLines:![self showVerticalGridLines]];
	[self setNeedsDisplay:YES];
}

- (IBAction)toggleFullBox:(id)sender
{
	[self setShowFullBox:![self showFullBox]];
	[self setNeedsDisplay:YES];
}

- (IBAction)rescaleSheetOK:(id)sender
{
	SLiMWindowController *controller = [self slimWindowController];
	NSWindow *window = [controller window];
	
	// Give our textfields a chance to validate
	if (![rescaleSheet makeFirstResponder:nil])
		return;
	
	[window endSheet:rescaleSheet returnCode:NSAlertFirstButtonReturn];
}

- (IBAction)rescaleSheetCancel:(id)sender
{
	SLiMWindowController *controller = [self slimWindowController];
	NSWindow *window = [controller window];
	
	[window endSheet:rescaleSheet returnCode:NSAlertSecondButtonReturn];
}

- (IBAction)userRescaleXAxis:(id)sender
{
	if ([self allowXAxisUserRescale])
	{
		SLiMWindowController *controller = [self slimWindowController];
		NSWindow *window = [controller window];
		
		[[NSBundle mainBundle] loadNibNamed:@"GraphAxisRescaleSheet" owner:self topLevelObjects:NULL];
		[rescaleSheet retain];
		
		[rescaleSheetMinTextfield setDoubleValue:[self xAxisMin]];
		[rescaleSheetMaxTextfield setDoubleValue:[self xAxisMax]];
		[rescaleSheetMajorIntervalTextfield setDoubleValue:[self xAxisMajorTickInterval]];
		[rescaleSheetMinorModulusTextfield setIntValue:[self xAxisMajorTickModulus]];
		[rescaleSheetTickPrecisionTextfield setIntValue:[self xAxisTickValuePrecision]];
		
		[window beginSheet:rescaleSheet completionHandler:^(NSModalResponse returnCode) {
			if (returnCode == NSAlertFirstButtonReturn)
			{
				double newMin = [rescaleSheetMinTextfield doubleValue];
				double newMax = [rescaleSheetMaxTextfield doubleValue];
				double newMajorInterval = [rescaleSheetMajorIntervalTextfield doubleValue];
				int newModulus = [rescaleSheetMinorModulusTextfield intValue];
				int newPrecision = [rescaleSheetTickPrecisionTextfield intValue];
				double newMinorInterval = newMajorInterval / newModulus;
				
				[self setXAxisMin:newMin];
				[self setXAxisMax:newMax];
				[self setXAxisMajorTickInterval:newMajorInterval];
				[self setXAxisMajorTickModulus:newModulus];
				[self setXAxisMinorTickInterval:newMinorInterval];
				[self setXAxisTickValuePrecision:newPrecision];
				[self setXAxisIsUserRescaled:YES];
				
				[self invalidateDrawingCache];
				[self setNeedsDisplay:YES];
			}
			
			[rescaleSheet autorelease];
			rescaleSheet = nil;
		}];
	}
}

- (IBAction)userRescaleYAxis:(id)sender
{
	if ([self allowYAxisUserRescale])
	{
		SLiMWindowController *controller = [self slimWindowController];
		NSWindow *window = [controller window];
		
		[[NSBundle mainBundle] loadNibNamed:@"GraphAxisRescaleSheet" owner:self topLevelObjects:NULL];
		[rescaleSheet retain];
		
		[rescaleSheetMinTextfield setDoubleValue:[self yAxisMin]];
		[rescaleSheetMaxTextfield setDoubleValue:[self yAxisMax]];
		[rescaleSheetMajorIntervalTextfield setDoubleValue:[self yAxisMajorTickInterval]];
		[rescaleSheetMinorModulusTextfield setIntValue:[self yAxisMajorTickModulus]];
		[rescaleSheetTickPrecisionTextfield setIntValue:[self yAxisTickValuePrecision]];
		
		[window beginSheet:rescaleSheet completionHandler:^(NSModalResponse returnCode) {
			if (returnCode == NSAlertFirstButtonReturn)
			{
				double newMin = [rescaleSheetMinTextfield doubleValue];
				double newMax = [rescaleSheetMaxTextfield doubleValue];
				double newMajorInterval = [rescaleSheetMajorIntervalTextfield doubleValue];
				int newModulus = [rescaleSheetMinorModulusTextfield intValue];
				int newPrecision = [rescaleSheetTickPrecisionTextfield intValue];
				double newMinorInterval = newMajorInterval / newModulus;
				
				[self setYAxisMin:newMin];
				[self setYAxisMax:newMax];
				[self setYAxisMajorTickInterval:newMajorInterval];
				[self setYAxisMajorTickModulus:newModulus];
				[self setYAxisMinorTickInterval:newMinorInterval];
				[self setYAxisTickValuePrecision:newPrecision];
				[self setYAxisIsUserRescaled:YES];
				
				[self invalidateDrawingCache];
				[self setNeedsDisplay:YES];
			}
			
			[rescaleSheet autorelease];
			rescaleSheet = nil;
		}];
	}
}

- (IBAction)copy:(id)sender
{
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	
	[pasteboard declareTypes:[NSArray arrayWithObject:NSPDFPboardType] owner:self];
	
	// We set generatingPDF to allow customization for PDF generation, such as not rounding to pixel values
	generatingPDF = YES;
	[self writePDFInsideRect:[self bounds] toPasteboard:pasteboard];
	generatingPDF = NO;
}

- (IBAction)saveGraph:(id)sender
{
	// We set generatingPDF to allow customization for PDF generation, such as not rounding to pixel values
	generatingPDF = YES;
	NSData *pdfData = [self dataWithPDFInsideRect:[self bounds]];
	generatingPDF = NO;
	
	SLiMWindowController *controller = [self slimWindowController];
	NSSavePanel *sp = [[NSSavePanel savePanel] retain];
	
	[sp setTitle:@"Export Graph"];
	[sp setNameFieldLabel:@"Export As:"];
	[sp setMessage:@"Export the graph to a file:"];
	[sp setExtensionHidden:NO];
	[sp setCanSelectHiddenExtension:NO];
	[sp setAllowedFileTypes:[NSArray arrayWithObject:@"pdf"]];
	
	[sp beginSheetModalForWindow:[controller window] completionHandler:^(NSInteger result) {
		if (result == NSFileHandlingPanelOKButton)
		{
			[pdfData writeToURL:[sp URL] atomically:YES];
			
			[sp autorelease];
		}
	}];
}

- (NSString *)dateline
{
	NSString *dateString = [NSDateFormatter localizedStringFromDate:[NSDate date] dateStyle:NSDateFormatterShortStyle timeStyle:NSDateFormatterMediumStyle];
	
	return [NSString stringWithFormat:@"# %@", dateString];
}

- (IBAction)copyData:(id)sender
{
	if ([self respondsToSelector:@selector(stringForDataWithController:)])
	{
		NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
		
		[pasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:self];
		[pasteboard setString:[self stringForDataWithController:[self slimWindowController]] forType:NSStringPboardType];
	}
}

- (IBAction)saveData:(id)sender
{
	if ([self respondsToSelector:@selector(stringForDataWithController:)])
	{
		SLiMWindowController *controller = [self slimWindowController];
		NSString *dataString = [self stringForDataWithController:controller];
		NSSavePanel *sp = [[NSSavePanel savePanel] retain];
		
		[sp setTitle:@"Export Data"];
		[sp setNameFieldLabel:@"Export As:"];
		[sp setMessage:@"Export the graph data to a file:"];
		[sp setExtensionHidden:NO];
		[sp setCanSelectHiddenExtension:NO];
		[sp setAllowedFileTypes:[NSArray arrayWithObject:@"txt"]];
		
		[sp beginSheetModalForWindow:[controller window] completionHandler:^(NSInteger result) {
			if (result == NSFileHandlingPanelOKButton)
			{
				[dataString writeToURL:[sp URL] atomically:YES encoding:NSUTF8StringEncoding error:NULL];
				
				[sp autorelease];
			}
		}];
	}
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
	SLiMWindowController *controller = [self slimWindowController];
	
	if (![controller invalidSimulation] && ![[controller window] attachedSheet])
	{
		BOOL addedItems = NO;
		NSMenu *menu = [[NSMenu alloc] initWithTitle:@"graph_menu"];
		
		// Toggle legend visibility
		NSSize legendSize = [self legendSize];
		
		if ((legendSize.width > 0.0) && (legendSize.height > 0.0))
		{
			NSMenuItem *menuItem = [menu addItemWithTitle:([self legendVisible] ? @"Hide Legend" : @"Show Legend") action:@selector(toggleLegend:) keyEquivalent:@""];
			
			[menuItem setTarget:self];
			addedItems = YES;
		}
		
		// Toggle horizontal grid line visibility
		if ([self showYAxis] && [self showYAxisTicks])
		{
			NSMenuItem *menuItem = [menu addItemWithTitle:([self showHorizontalGridLines] ? @"Hide Horizontal Grid" : @"Show Horizontal Grid") action:@selector(toggleHorizontalGridLines:) keyEquivalent:@""];
			
			[menuItem setTarget:self];
			addedItems = YES;
		}
		
		// Toggle vertical grid line visibility
		if ([self showXAxis] && [self showXAxisTicks])
		{
			NSMenuItem *menuItem = [menu addItemWithTitle:([self showVerticalGridLines] ? @"Hide Vertical Grid" : @"Show Vertical Grid") action:@selector(toggleVerticalGridLines:) keyEquivalent:@""];
			
			[menuItem setTarget:self];
			addedItems = YES;
		}
		
		// Toggle box visibility
		if ([self showXAxis] && [self showYAxis])
		{
			NSMenuItem *menuItem = [menu addItemWithTitle:([self showFullBox] ? @"Hide Full Box" : @"Show Full Box") action:@selector(toggleFullBox:) keyEquivalent:@""];
			
			[menuItem setTarget:self];
			addedItems = YES;
		}
		
		// Add a separator if we had any visibility-toggle menu items above
		if (addedItems)
			[menu addItem:[NSMenuItem separatorItem]];
		addedItems = NO;
		
		// Rescale x axis
		if ([self showXAxis] && [self showXAxisTicks] && [self allowXAxisUserRescale])
		{
			NSMenuItem *menuItem = [menu addItemWithTitle:@"Change X Axis Scale..." action:@selector(userRescaleXAxis:) keyEquivalent:@""];
			
			[menuItem setTarget:self];
			addedItems = YES;
		}
		
		// Rescale y axis
		if ([self showYAxis] && [self showYAxisTicks] && [self allowYAxisUserRescale])
		{
			NSMenuItem *menuItem = [menu addItemWithTitle:@"Change Y Axis Scale..." action:@selector(userRescaleYAxis:) keyEquivalent:@""];
			
			[menuItem setTarget:self];
			addedItems = YES;
		}
		
		// Add a separator if we had any visibility-toggle menu items above
		if (addedItems)
			[menu addItem:[NSMenuItem separatorItem]];
		addedItems = NO;
		
		// Copy/export the graph image
		{
			NSMenuItem *menuItem;
			
			menuItem = [menu addItemWithTitle:@"Copy Graph" action:@selector(copy:) keyEquivalent:@""];
			[menuItem setTarget:self];
			
			menuItem = [menu addItemWithTitle:@"Export Graph..." action:@selector(saveGraph:) keyEquivalent:@""];
			[menuItem setTarget:self];
		}
		
		// Copy/export the data to the clipboard
		if ([self respondsToSelector:@selector(stringForDataWithController:)])
		{
			NSMenuItem *menuItem;
			
			[menu addItem:[NSMenuItem separatorItem]];
			
			menuItem = [menu addItemWithTitle:@"Copy Data" action:@selector(copyData:) keyEquivalent:@""];
			[menuItem setTarget:self];
			
			menuItem = [menu addItemWithTitle:@"Export Data..." action:@selector(saveData:) keyEquivalent:@""];
			[menuItem setTarget:self];
		}
		
		return [menu autorelease];
	}
	
	return nil;
}

- (void)invalidateDrawingCache
{
	// GraphView has no drawing cache, but it supports the idea of one
}

- (void)graphWindowResized
{
	[self invalidateDrawingCache];
}

- (void)controllerRecycled
{
	[self invalidateDrawingCache];
	[self setNeedsDisplay:YES];
}

- (void)controllerSelectionChanged
{
}

- (void)controllerGenerationFinished
{
}

- (void)setNeedsDisplay
{
	[self setNeedsDisplay:YES];
}

@end


@implementation GraphView (PrefabAdditions)

- (NSSize)mutationTypeLegendSize
{
	SLiMWindowController *controller = [self slimWindowController];
	SLiMSim *sim = controller->sim;
	int mutationTypeCount = (int)sim->mutation_types_.size();
	
	// If we only have one mutation type, do not show a legend
	if (mutationTypeCount < 2)
		return NSZeroSize;
	
	const int legendRowHeight = 15;
	NSDictionary *legendAttrs = [[self class] attributesForLegendLabels];
	NSSize legendSize = NSMakeSize(0, legendRowHeight * mutationTypeCount - 6);
	
	for (auto mutationTypeIter = sim->mutation_types_.begin(); mutationTypeIter != sim->mutation_types_.end(); ++mutationTypeIter)
	{
		MutationType *mutationType = (*mutationTypeIter).second;
		int mutationTypeIndex = mutationType->mutation_type_index_;		// look up the index used for this mutation type in the history info; not necessarily sequential!
		NSRect swatchRect = NSMakeRect(0, ((mutationTypeCount - 1) * legendRowHeight) - mutationTypeIndex * legendRowHeight + 3, legendRowHeight - 6, legendRowHeight - 6);
		NSString *labelString = [NSString stringWithFormat:@"m%d", mutationType->mutation_type_id_];
		NSAttributedString *label = [[NSAttributedString alloc] initWithString:labelString attributes:legendAttrs];
		NSSize labelSize = [label size];
		
		legendSize.width = MAX(legendSize.width, swatchRect.origin.x + swatchRect.size.width + 5 + labelSize.width);
		[label release];
	}
	
	return legendSize;
}

- (void)drawMutationTypeLegendInRect:(NSRect)legendRect
{
	const int legendRowHeight = 15;
	NSDictionary *legendAttrs = [[self class] attributesForLegendLabels];
	SLiMWindowController *controller = [self slimWindowController];
	SLiMSim *sim = controller->sim;
	int mutationTypeCount = (int)sim->mutation_types_.size();
	
	for (auto mutationTypeIter = sim->mutation_types_.begin(); mutationTypeIter != sim->mutation_types_.end(); ++mutationTypeIter)
	{
		MutationType *mutationType = (*mutationTypeIter).second;
		int mutationTypeIndex = mutationType->mutation_type_index_;		// look up the index used for this mutation type in the history info; not necessarily sequential!
		NSRect swatchRect = NSMakeRect(legendRect.origin.x, legendRect.origin.y + ((mutationTypeCount - 1) * legendRowHeight - 3) - mutationTypeIndex * legendRowHeight + 3, legendRowHeight - 6, legendRowHeight - 6);
		NSString *labelString = [NSString stringWithFormat:@"m%d", mutationType->mutation_type_id_];
		NSAttributedString *label = [[NSAttributedString alloc] initWithString:labelString attributes:legendAttrs];
		
		[[SLiMWindowController colorForIndex:mutationTypeIndex] set];
		NSRectFill(swatchRect);
		
		[[NSColor blackColor] set];
		NSFrameRect(swatchRect);
		
		[label drawAtPoint:NSMakePoint(swatchRect.origin.x + swatchRect.size.width + 5, swatchRect.origin.y - 2)];
		[label release];
	}
}

- (void)drawGroupedBarplotInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller buffer:(double *)buffer subBinCount:(int)subBinCount mainBinCount:(int)mainBinCount firstBinValue:(double)firstBinValue mainBinWidth:(double)mainBinWidth
{
	for (int mainBinIndex = 0; mainBinIndex < mainBinCount; ++mainBinIndex)
	{
		double binMinValue = mainBinIndex * mainBinWidth + firstBinValue;
		double binMaxValue = (mainBinIndex + 1) * mainBinWidth + firstBinValue;
		double barLeft = [self roundPlotToDeviceX:binMinValue withInteriorRect:interiorRect] + 1.5;
		double barRight = [self roundPlotToDeviceX:binMaxValue withInteriorRect:interiorRect] - 1.5;
		
		for (int subBinIndex = 0; subBinIndex < subBinCount; ++subBinIndex)
		{
			int actualBinIndex = subBinIndex + mainBinIndex * subBinCount;
			double binValue = buffer[actualBinIndex];
			double barBottom = interiorRect.origin.y - 1;
			double barTop = (binValue == 0 ? interiorRect.origin.y - 1 : [self roundPlotToDeviceY:binValue withInteriorRect:interiorRect] + 0.5);
			NSRect barRect = NSMakeRect(barLeft, barBottom, barRight - barLeft, barTop - barBottom);
			
			if (barRect.size.height > 0.0)
			{
				// subdivide into sub-bars for each mutation type, if there is more than one
				if (subBinCount > 1)
				{
					double barWidth = barRect.size.width;
					double subBarWidth = (barWidth - 1) / subBinCount;
					double subbarLeft = SCREEN_ROUND(barRect.origin.x + subBinIndex * subBarWidth);
					double subbarRight = SCREEN_ROUND(barRect.origin.x + (subBinIndex + 1) * subBarWidth) + 1;
					
					barRect.origin.x = subbarLeft;
					barRect.size.width = subbarRight - subbarLeft;
				}
				
				// fill and fill
				[[SLiMWindowController colorForIndex:subBinIndex] set];
				NSRectFill(barRect);
				
				[[NSColor blackColor] set];
				NSFrameRect(barRect);
			}
		}
	}
}

- (void)drawBarplotInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller buffer:(double *)buffer binCount:(int)binCount firstBinValue:(double)firstBinValue binWidth:(double)binWidth
{
	[self drawGroupedBarplotInInteriorRect:interiorRect withController:controller buffer:buffer subBinCount:1 mainBinCount:binCount firstBinValue:firstBinValue mainBinWidth:binWidth];
}

@end



























































