//
//  CocoaExtra.m
//  SLiM
//
//  Created by Ben Haller on 1/22/15.
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


#import "CocoaExtra.h"
#import "AppDelegate.h"

#include "eidos_rng.h"
#include "mutation_type.h"
#include "interaction_type.h"

// for SLiM_AmIBeingDebugged()
#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysctl.h>

// From Apple tech note #1361, https://developer.apple.com/library/archive/qa/qa1361/_index.html
// see https://stackoverflow.com/a/2200786/2752221
bool SLiM_AmIBeingDebugged(void)
    // Returns true if the current process is being debugged (either 
    // running under the debugger or has a debugger attached post facto).
{
    int                 junk;
    int                 mib[4];
    struct kinfo_proc   info;
    size_t              size;

    // Initialize the flags so that, if sysctl fails for some bizarre 
    // reason, we get a predictable result.

    info.kp_proc.p_flag = 0;

    // Initialize mib, which tells sysctl the info we want, in this case
    // we're looking for information about a specific process ID.

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    // Call sysctl.

    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    assert(junk == 0);

    // We're being debugged if the P_TRACED flag is set.

    return ( (info.kp_proc.p_flag & P_TRACED) != 0 );
}

@implementation SLiMTableView

- (BOOL)acceptsFirstResponder
{
	return NO;
}

@end


static NSDictionary *tickAttrs = nil;
static NSDictionary *disabledTickAttrs = nil;
static const int numberOfTicks = 4;
static const int tickLength = 4;
static const int heightForTicks = 15;

@implementation SLiMColorStripeView

+ (void)initialize
{
	if (!tickAttrs)
		tickAttrs = [@{NSForegroundColorAttributeName : [NSColor blackColor], NSFontAttributeName : [NSFont systemFontOfSize:9.0]} retain];
	if (!disabledTickAttrs)
		disabledTickAttrs = [@{NSForegroundColorAttributeName : [NSColor colorWithCalibratedWhite:0.6 alpha:1.0], NSFontAttributeName : [NSFont systemFontOfSize:9.0]} retain];
	
	[self exposeBinding:@"enabled"];
}

- (void)setEnabled:(BOOL)enabled
{
	if (_enabled != enabled)
	{
		_enabled = enabled;
		
		[self setNeedsDisplay:YES];
	}
}

- (void)awakeFromNib
{
	[self bind:@"enabled" toObject:[[self window] windowController] withKeyPath:@"invalidSimulation" options:@{NSValueTransformerNameBindingOption : NSNegateBooleanTransformerName}];
}

- (void)dealloc
{
	[self unbind:@"enabled"];
	
	[super dealloc];
}

- (BOOL)isOpaque
{
	return NO;	// we have a 10-pixel margin on our left and right to allow tick labels to overflow our apparent bounds
}

- (void)setMetricToPlot:(int)metricToPlot
{
	if (_metricToPlot != metricToPlot)
	{
		_metricToPlot = metricToPlot;
		
		[self setNeedsDisplay:YES];
	}
}

- (void)drawTicksInContentRect:(NSRect)contentRect
{
	BOOL enabled = [self enabled];
	int metric = [self metricToPlot];
	NSRect interiorRect = NSInsetRect(contentRect, 1, 1);
	
	for (int tickIndex = 0; tickIndex <= numberOfTicks; ++tickIndex)
	{
		double fraction = tickIndex / (double)numberOfTicks;
		int tickLeft = (int)floor(interiorRect.origin.x + fraction * (interiorRect.size.width - 1));
		NSRect tickRect = NSMakeRect(tickLeft, contentRect.origin.y - tickLength, 1, tickLength);
		
		[[NSColor colorWithCalibratedWhite:(enabled ? 0.5 : 0.6) alpha:1.0] set];
		NSRectFill(tickRect);
		
		NSString *tickLabel = nil;
		
		if (metric == 1)
		{
			if (tickIndex == 0) tickLabel = @"0.0";
			if (tickIndex == 1) tickLabel = @"0.5";
			if (tickIndex == 2) tickLabel = @"1.0";
			if (tickIndex == 3) tickLabel = @"2.0";
			if (tickIndex == 4) tickLabel = @"∞";
		}
		else if (metric == 2)
		{
			if (tickIndex == 0) tickLabel = @"−0.5";
			if (tickIndex == 1) tickLabel = @"−0.25";
			if (tickIndex == 2) tickLabel = @"0.0";
			if (tickIndex == 3) tickLabel = @"0.5";
			if (tickIndex == 4) tickLabel = @"1.0";
		}
		
		if (tickLabel)
		{
			NSAttributedString *tickAttrLabel = [[NSAttributedString alloc] initWithString:tickLabel attributes:(enabled ? tickAttrs : disabledTickAttrs)];
			NSSize tickLabelSize = [tickAttrLabel size];
			int tickLabelX = tickLeft - (int)round(tickLabelSize.width / 2.0);
			
			[tickAttrLabel drawAtPoint:NSMakePoint(tickLabelX, contentRect.origin.y - (tickLength + 12))];
			
			[tickAttrLabel release];
		}
	}
}

- (void)drawRect:(NSRect)dirtyRect
{
	NSRect bounds = [self bounds];
	NSRect contentRect = NSMakeRect(bounds.origin.x + 10, bounds.origin.y + heightForTicks, bounds.size.width - 20, bounds.size.height - heightForTicks);
	NSRect interiorRect = NSInsetRect(contentRect, 1, 1);
	
	// frame the content area itself
	[[NSColor colorWithCalibratedWhite:0.6 alpha:1.0] set];
	NSFrameRect(contentRect);
	
	double scaling = [self scalingFactor];
	int metric = [self metricToPlot];
	
	// draw our stripe
	for (int x = 0; x < interiorRect.size.width; ++x)
	{
		NSRect stripe = NSMakeRect(interiorRect.origin.x + x, interiorRect.origin.y, 1, interiorRect.size.height);
		float red = 0.0, green = 0.0, blue = 0.0;
		double fraction = x / (interiorRect.size.width - 1);
		
		// guarantee that there is a pixel position where fraction is 0.5, so neutrality gets drawn
		if (x == floor((interiorRect.size.width - 1) / 2.0))
			fraction = 0.5;
		
		if (metric == 1)
		{
			double fitness;
			
			if (fraction < 0.5) fitness = fraction * 2.0;						// [0.0, 0.5] -> [0.0, 1.0]
			else if (fraction < 0.75) fitness = (fraction - 0.5) * 4.0 + 1.0;	// [0.5, 0.75] -> [1.0, 2.0]
			else fitness = 0.50 / (0.25 - (fraction - 0.75));					// [0.75, 1.0] -> [2.0, +Inf]
			
			if (fraction == 1.0)
				fitness = 1e100;	// avoid infinity
			
			RGBForFitness(fitness, &red, &green, &blue, scaling);
		}
		else if (metric == 2)
		{
			double selectionCoeff;
			
			if (fraction < 0.5) selectionCoeff = fraction - 0.5;				// [0.0, 0.5] -> [-0.5, 0]
			else selectionCoeff = (fraction - 0.5) * 2.0;						// [0.5, 1.0] -> [0.0, 1.0]
			
			RGBForSelectionCoeff(selectionCoeff, &red, &green, &blue, scaling);
		}
		
		[[NSColor colorWithCalibratedRed:red green:green blue:blue alpha:1.0] set];
		NSRectFill(stripe);
	}
	
	// draw ticks at bottom of content rect
	[self drawTicksInContentRect:contentRect];
	
	// if we are not enabled, wash over the interior with light gray
	if (![self enabled])
	{
		[[NSColor colorWithCalibratedWhite:0.9 alpha:0.8] set];
		NSRectFillUsingOperation(interiorRect, NSCompositeSourceOver);
	}
}

@end

const float greenBrightness = 0.8f;

void RGBForFitness(double value, float *colorRed, float *colorGreen, float *colorBlue, double scalingFactor)
{
	// apply the scaling factor
	value = (value - 1.0) * scalingFactor + 1.0;
	
	if (value <= 0.5)
	{
		// value <= 0.5 is a shade of red, going down to black
		*colorRed = (float)(value * 2.0);
		*colorGreen = 0.0;
		*colorBlue = 0.0;
	}
	else if (value >= 2.0)
	{
		// value >= 2.0 is a shade of green, going up to white
		*colorRed = (float)((value - 2.0) * greenBrightness / value);
		*colorGreen = greenBrightness;
		*colorBlue = (float)((value - 2.0) * greenBrightness / value);
	}
	else if (value <= 1.0)
	{
		// value <= 1.0 (but > 0.5) goes from red (unfit) to yellow (neutral)
		*colorRed = 1.0;
		*colorGreen = (float)((value - 0.5) * 2.0);
		*colorBlue = 0.0;
	}
	else	// 1.0 < value < 2.0
	{
		// value > 1.0 (but < 2.0) goes from yellow (neutral) to green (fit)
		*colorRed = (float)(2.0 - value);
		*colorGreen = (float)(greenBrightness + (1.0 - greenBrightness) * (2.0 - value));
		*colorBlue = 0.0;
	}
}

void RGBForSelectionCoeff(double value, float *colorRed, float *colorGreen, float *colorBlue, double scalingFactor)
{
	// apply a scaling factor; this could be user-adjustible since different models have different relevant fitness ranges
	value *= scalingFactor;
	
	// and add 1, just so we can re-use the same code as in RGBForFitness()
	value += 1.0;
	
	if (value <= 0.0)
	{
		// value <= 0.0 is the darkest shade of red we use
		*colorRed = 0.5;
		*colorGreen = 0.0;
		*colorBlue = 0.0;
	}
	else if (value <= 0.5)
	{
		// value <= 0.5 is a shade of red, going down toward black
		*colorRed = (float)(value + 0.5);
		*colorGreen = 0.0;
		*colorBlue = 0.0;
	}
	else if (value < 1.0)
	{
		// value <= 1.0 (but > 0.5) goes from red (very unfit) to orange (nearly neutral)
		*colorRed = 1.0;
		*colorGreen = (float)((value - 0.5) * 1.0);
		*colorBlue = 0.0;
	}
	else if (value == 1.0)
	{
		// exactly neutral mutations are yellow
		*colorRed = 1.0;
		*colorGreen = 1.0;
		*colorBlue = 0.0;
	}
	else if (value <= 1.5)
	{
		// value > 1.0 (but < 1.5) goes from green (nearly neutral) to cyan (fit)
		*colorRed = 0.0;
		*colorGreen = greenBrightness;
		*colorBlue = (float)((value - 1.0) * 2.0);
	}
	else if (value <= 2.0)
	{
		// value > 1.5 (but < 2.0) goes from cyan (fit) to blue (very fit)
		*colorRed = 0.0;
		*colorGreen = (float)(greenBrightness * ((2.0 - value) * 2.0));
		*colorBlue = 1.0;
	}
	else // (value > 2.0)
	{
		// value > 2.0 is a shade of blue, going up toward white
		*colorRed = (float)((value - 2.0) * 0.75 / value);
		*colorGreen = (float)((value - 2.0) * 0.75 / value);
		*colorBlue = 1.0;
	}
}


@implementation SLiMMenuButton

- (void)dealloc
{
	[self setSlimMenu:nil];
	
	[super dealloc];
}

- (void)fixMenu
{
	NSMenu *menu = [self slimMenu];
	NSDictionary *itemAttrs = @{NSFontAttributeName : [NSFont systemFontOfSize:12.0]};
	
	for (int i = 0; i < [menu numberOfItems]; ++i)
	{
		NSMenuItem *menuItem = [menu itemAtIndex:i];
		NSString *title = [menuItem title];
		NSAttributedString *attrTitle = [[NSAttributedString alloc] initWithString:title attributes:itemAttrs];
		
		[menuItem setAttributedTitle:attrTitle];
		[attrTitle release];
	}
}

- (void)mouseDown:(NSEvent *)theEvent
{
	// We do not call super; we do mouse tracking entirely ourselves
	//[super mouseDown:theEvent];
	
	if ([self isEnabled])
	{
		NSRect bounds = [self bounds];
		
		[self highlight:YES];
		
		[self fixMenu];
		[[self slimMenu] popUpMenuPositioningItem:nil atLocation:NSMakePoint(bounds.size.width * 0.80 - 1, bounds.size.height * 0.80 + 1) inView:self];
		
		[self highlight:NO];
	}
}

- (void)mouseDragged:(NSEvent *)theEvent
{
	// We do not call super; we do mouse tracking entirely ourselves
	//[super mouseDragged:theEvent];
}

- (void)mouseUp:(NSEvent *)theEvent
{
	// We do not call super; we do mouse tracking entirely ourselves
	//[super mouseDown:theEvent];
}

@end

@implementation SLiMColorCell

- (void)setObjectValue:(id)objectValue
{
	// Ensure that only NSColor objects are set as our object value
	if ([objectValue isKindOfClass:[NSColor class]])
		[super setObjectValue:objectValue];
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
	NSRect swatchFrame = NSInsetRect(cellFrame, 1, 1);
	
	// Make our swatch be square, if we have enough room to do it
	if (swatchFrame.size.width > swatchFrame.size.height)
	{
		swatchFrame.origin.x = (int)round(swatchFrame.origin.x + (swatchFrame.size.width - swatchFrame.size.height) / 2.0);
		swatchFrame.size.width = swatchFrame.size.height;
	}
	
	[[NSColor blackColor] set];
	NSFrameRect(swatchFrame);
	
	[(NSColor *)[self objectValue] set];
	NSRectFill(NSInsetRect(swatchFrame, 1, 1));
}

@end


@implementation SLiMSelectionView

- (void)drawRect:(NSRect)dirtyRect
{
	static NSDictionary *labelAttrs = nil;
	
	if (!labelAttrs)
		labelAttrs = [@{NSFontAttributeName : [NSFont fontWithName:@"Times New Roman" size:10], NSForegroundColorAttributeName : [NSColor blackColor]} retain];
	
	NSRect bounds = [self bounds];
	SLiMSelectionMarker *marker = (SLiMSelectionMarker *)[self window];
	NSAttributedString *attrLabel = [[NSAttributedString alloc] initWithString:[marker label] attributes:labelAttrs];
	NSSize labelStringSize = [attrLabel size];
	NSSize labelSize = NSMakeSize(round(labelStringSize.width + 8.0), round(labelStringSize.height + 1.0));
	NSRect labelRect = NSMakeRect(bounds.origin.x + round(bounds.size.width / 2.0), bounds.origin.y + bounds.size.height - labelSize.height, labelSize.width, labelSize.height);
	
	if ([marker isLeftMarker])
		labelRect.origin.x -= round(labelSize.width - 1.0);
	
	// Frame our whole bounds, for debugging; note that we draw in only a portion of our bounds, and the rest is transparent
	//[[NSColor blackColor] set];
	//NSFrameRect(bounds);
	
#if 0
	// Debugging code: frame and fill our label rect without using NSBezierPath
	[[NSColor colorWithCalibratedHue:0.15 saturation:0.2 brightness:1.0 alpha:1.0] set];
	NSRectFill(labelRect);
	
	[[NSColor colorWithCalibratedHue:0.15 saturation:0.2 brightness:0.3 alpha:1.0] set];
	NSFrameRect(labelRect);
#else
	// Production code: use NSBezierPath to get a label that has a tag off of it
	NSBezierPath *bez = [NSBezierPath bezierPath];
	NSRect ilr = NSInsetRect(labelRect, 0.5, 0.5);	// inset by 0.5 to place us mid-pixel, so stroke looks good
	const double tagHeight = 5.0;
	
	if ([marker isLeftMarker])
	{
		// label rect with a diagonal tag down from the right edge
		[bez moveToPoint:NSMakePoint(ilr.origin.x, ilr.origin.y)];
		[bez relativeLineToPoint:NSMakePoint(ilr.size.width - tagHeight, 0)];
		[bez relativeLineToPoint:NSMakePoint(tagHeight, -tagHeight)];
		[bez relativeLineToPoint:NSMakePoint(0, ilr.size.height + tagHeight)];
		[bez relativeLineToPoint:NSMakePoint(-ilr.size.width, 0)];
		[bez closePath];
	}
	else
	{
		// label rect with a diagonal tag down from the right edge
		[bez moveToPoint:NSMakePoint(ilr.origin.x + ilr.size.width, ilr.origin.y)];
		[bez relativeLineToPoint:NSMakePoint(- (ilr.size.width - tagHeight), 0)];
		[bez relativeLineToPoint:NSMakePoint(-tagHeight, -tagHeight)];
		[bez relativeLineToPoint:NSMakePoint(0, ilr.size.height + tagHeight)];
		[bez relativeLineToPoint:NSMakePoint(ilr.size.width, 0)];
		[bez closePath];
	}
	
	[[NSColor colorWithCalibratedHue:0.15 saturation:0.2 brightness:1.0 alpha:1.0] set];
	[bez fill];
	
	[[NSColor blackColor] set];
	[bez setLineWidth:1.0];
	[bez stroke];
#endif
	
	[attrLabel drawAtPoint:NSMakePoint(labelRect.origin.x + 4, labelRect.origin.y + 1)];
	[attrLabel release];
}

- (BOOL)isOpaque
{
	return NO;
}

@end

@implementation SLiMSelectionMarker

// makes a new marker with no label and no tip point, not shown
+ (instancetype)new
{
	return [[[self class] alloc] initWithContentRect:NSMakeRect(0, 0, 150, 20) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];	// 150x20 should suffice, unless we change our font size...
}

- (instancetype)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)aStyle backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag
{
	if (self = [super initWithContentRect:contentRect styleMask:aStyle backing:bufferingType defer:flag])
	{
		[self setFloatingPanel:YES];
		[self setBecomesKeyOnlyIfNeeded:YES];
		[self setHasShadow:NO];
		[self setOpaque:NO];
		[self setBackgroundColor:[NSColor clearColor]];
		
		SLiMSelectionView *view = [[SLiMSelectionView alloc] initWithFrame:contentRect];
		
		[self setContentView:view];
		[view release];
		
		_tipPoint = NSMakePoint(round(contentRect.origin.x + contentRect.size.width / 2.0), contentRect.origin.y);
		_label = [@"1000000000" retain];
	}
	
	return self;
}
	
- (void)dealloc
{
	[self setLabel:nil];
	
	[super dealloc];
}

- (void)setLabel:(NSString *)label
{
	if (![_label isEqualToString:label])
	{
		[label retain];
		[_label release];
		_label = label;
		
		[[self contentView] setNeedsDisplay:YES];
	}
}

- (void)setTipPoint:(NSPoint)tipPoint
{
	if (!NSEqualPoints(_tipPoint, tipPoint))
	{
		NSPoint origin = [self frame].origin;
		
		origin.x += (tipPoint.x - _tipPoint.x);
		origin.y += (tipPoint.y - _tipPoint.y);
		
		_tipPoint = tipPoint;
		
		[self setFrameOrigin:origin];
	}
}

@end


@implementation SLiMPlaySliderToolTipView

- (void)drawRect:(NSRect)dirtyRect
{
	static NSDictionary *labelAttrs = nil;
	
	if (!labelAttrs)
		labelAttrs = [@{NSFontAttributeName : [NSFont fontWithName:@"Times New Roman" size:10], NSForegroundColorAttributeName : [NSColor blackColor]} retain];
	
	NSRect bounds = [self bounds];
	SLiMPlaySliderToolTipWindow *tooltipWindow = (SLiMPlaySliderToolTipWindow *)[self window];
	NSAttributedString *attrLabel = [[NSAttributedString alloc] initWithString:[tooltipWindow label] attributes:labelAttrs];
	NSSize labelStringSize = [attrLabel size];
	NSSize labelSize = NSMakeSize(round(labelStringSize.width + 8.0), round(labelStringSize.height + 1.0));
	NSRect labelRect = NSMakeRect(bounds.origin.x, bounds.origin.y + bounds.size.height - labelSize.height, labelSize.width, labelSize.height);
	
	// Frame our whole bounds, for debugging; note that we draw in only a portion of our bounds, and the rest is transparent
	//[[NSColor blackColor] set];
	//NSFrameRect(bounds);
	
	// Frame and fill our label rect
	[[NSColor colorWithCalibratedHue:0.15 saturation:0.2 brightness:1.0 alpha:1.0] set];
	NSRectFill(labelRect);
	
	[[NSColor colorWithCalibratedHue:0.15 saturation:0.2 brightness:0.3 alpha:1.0] set];
	NSFrameRect(labelRect);
	
	[attrLabel drawAtPoint:NSMakePoint(labelRect.origin.x + 4, labelRect.origin.y + 1)];
	[attrLabel release];
}

- (BOOL)isOpaque
{
	return NO;
}

@end

@implementation SLiMPlaySliderToolTipWindow

// makes a new marker with no label and no tip point, not shown
+ (instancetype)new
{
	return [[[self class] alloc] initWithContentRect:NSMakeRect(0, 0, 50, 20) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];	// 50x20 should suffice, unless we change our font size...
}

- (instancetype)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)aStyle backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag
{
	if (self = [super initWithContentRect:contentRect styleMask:aStyle backing:bufferingType defer:flag])
	{
		[self setFloatingPanel:YES];
		[self setBecomesKeyOnlyIfNeeded:YES];
		[self setHasShadow:NO];
		[self setOpaque:NO];
		[self setBackgroundColor:[NSColor clearColor]];
		
		SLiMPlaySliderToolTipView *view = [[SLiMPlaySliderToolTipView alloc] initWithFrame:contentRect];
		
		[self setContentView:view];
		[view release];
		
		_tipPoint = NSMakePoint(contentRect.origin.x, contentRect.origin.y);
		_label = [@"1000000000" retain];
	}
	
	return self;
}

- (void)dealloc
{
	[self setLabel:nil];
	
	[super dealloc];
}

- (void)setLabel:(NSString *)label
{
	if (![_label isEqualToString:label])
	{
		[label retain];
		[_label release];
		_label = label;
		
		[[self contentView] setNeedsDisplay:YES];
	}
}

- (void)setTipPoint:(NSPoint)tipPoint
{
	if (!NSEqualPoints(_tipPoint, tipPoint))
	{
		NSPoint origin = [self frame].origin;
		
		origin.x += (tipPoint.x - _tipPoint.x);
		origin.y += (tipPoint.y - _tipPoint.y);
		
		_tipPoint = tipPoint;
		
		[self setFrameOrigin:origin];
	}
}

@end


@implementation SLiMFunctionGraphToolTipView

- (void)drawRect:(NSRect)dirtyRect
{
	static NSDictionary *labelAttrs = nil;
	static NSDictionary *questionMarkAttrs = nil;
	
	if (!labelAttrs)
	{
		labelAttrs = [@{NSFontAttributeName : [NSFont fontWithName:@"Times New Roman" size:9], NSForegroundColorAttributeName : [NSColor blackColor]} retain];
		questionMarkAttrs = [@{NSFontAttributeName : [NSFont fontWithName:@"Times New Roman" size:18], NSForegroundColorAttributeName : [NSColor blackColor]} retain];
	}
	
	NSRect bounds = [self bounds];
	SLiMFunctionGraphToolTipWindow *tooltipWindow = (SLiMFunctionGraphToolTipWindow *)[self window];
	
	// Frame and fill our label rect
	[[NSColor colorWithCalibratedHue:0.0 saturation:0.0 brightness:0.95 alpha:1.0] set];
	NSRectFill(bounds);
	
	[[NSColor colorWithCalibratedHue:0.15 saturation:0.0 brightness:0.75 alpha:1.0] set];
	NSFrameRect(bounds);
	
	// Plan our plotting
	MutationType *mut_type = [tooltipWindow mutType];
	InteractionType *interaction_type = [tooltipWindow interactionType];
	
	if ((!mut_type && !interaction_type) || (mut_type && interaction_type))
		return;
	
	int sample_size;
	std::vector<double> draws;
	bool draw_positive = false, draw_negative = false;
	bool heights_negative = false;
	double axis_min, axis_max;
	bool draw_axis_midpoint = true, custom_axis_max = false;
	
	if (mut_type)
	{
		// Generate draws for a mutation type; this case is stochastic, based upon a large number of DFE samples.
		// Draw all the values we will plot; we need our own private RNG so we don't screw up the simulation's.
		// Drawing selection coefficients could raise, if they are type "s" and there is an error in the script,
		// so we run the sampling inside a try/catch block; if we get a raise, we just show a "?" in the plot.
		static Eidos_RNG_State local_rng;
		
		sample_size = (mut_type->dfe_type_ == DFEType::kScript) ? 100000 : 1000000;	// large enough to make curves pretty smooth, small enough to be reasonably fast
		draws.reserve(sample_size);
		
		std::swap(local_rng, gEidos_RNG);	// swap in our local RNG
		
		if (!EIDOS_GSL_RNG)
			Eidos_InitializeRNG();
		Eidos_SetRNGSeed(10);		// arbitrary seed, but the same seed every time
		
		//std::clock_t start = std::clock();
		
		try
		{
			for (int sample_count = 0; sample_count < sample_size; ++sample_count)
			{
				double draw = mut_type->DrawSelectionCoefficient();
				
				draws.push_back(draw);
				
				if (draw < 0.0)			draw_negative = true;
				else if (draw > 0.0)	draw_positive = true;
			}
		}
		catch (...)
		{
			draws.clear();
			draw_negative = true;
			draw_positive = true;
		}
		
		//NSLog(@"Draws took %f seconds", (std::clock() - start) / (double)CLOCKS_PER_SEC);
		
		std::swap(local_rng, gEidos_RNG);	// swap out our local RNG; restore the standard RNG
		
		// figure out axis limits
		if (draw_negative && !draw_positive)
		{
			axis_min = -1.0;
			axis_max = 0.0;
		}
		else if (draw_positive && !draw_negative)
		{
			axis_min = 0.0;
			axis_max = 1.0;
		}
		else
		{
			axis_min = -1.0;
			axis_max = 1.0;
		}
	}
	else // if (interaction_type)
	{
		// Since interaction types are deterministic, we don't need draws; we will just calculate our
		// bin heights directly below.
		sample_size = 0;
		draw_negative = false;
		draw_positive = true;
		axis_min = 0.0;
		if ((interaction_type->max_distance_ < 1.0) || std::isinf(interaction_type->max_distance_))
		{
			axis_max = 1.0;
		}
		else
		{
			axis_max = interaction_type->max_distance_;
			draw_axis_midpoint = false;
			custom_axis_max = true;
		}
		heights_negative = (interaction_type->if_param1_ < 0.0);	// this is a negative-strength interaction, if T
	}
	
	// Draw the graph axes and ticks
	NSRect graphRect = NSMakeRect(bounds.origin.x + 6, bounds.origin.y + (heights_negative ? 5 : 14), bounds.size.width - 12, bounds.size.height - 20);
	CGFloat axis_y = (heights_negative ? graphRect.origin.y + graphRect.size.height - 1 : graphRect.origin.y);
	CGFloat tickoff3 = (heights_negative ? 1 : -3);
	CGFloat tickoff1 = (heights_negative ? 1 : -1);
	
	[[NSColor colorWithCalibratedHue:0.15 saturation:0.0 brightness:0.2 alpha:1.0] set];
	NSRectFill(NSMakeRect(graphRect.origin.x, axis_y, graphRect.size.width, 1));
	
	NSRectFill(NSMakeRect(graphRect.origin.x, axis_y + tickoff3, 1, 3));
	NSRectFill(NSMakeRect(graphRect.origin.x + (graphRect.size.width - 1) * 0.125, axis_y + tickoff1, 1, 1));
	NSRectFill(NSMakeRect(graphRect.origin.x + (graphRect.size.width - 1) * 0.25, axis_y + tickoff1, 1, 1));
	NSRectFill(NSMakeRect(graphRect.origin.x + (graphRect.size.width - 1) * 0.375, axis_y + tickoff1, 1, 1));
	NSRectFill(NSMakeRect(graphRect.origin.x + (graphRect.size.width - 1) * 0.5, axis_y + tickoff3, 1, 3));
	NSRectFill(NSMakeRect(graphRect.origin.x + (graphRect.size.width - 1) * 0.625, axis_y + tickoff1, 1, 1));
	NSRectFill(NSMakeRect(graphRect.origin.x + (graphRect.size.width - 1) * 0.75, axis_y + tickoff1, 1, 1));
	NSRectFill(NSMakeRect(graphRect.origin.x + (graphRect.size.width - 1) * 0.875, axis_y + tickoff1, 1, 1));
	NSRectFill(NSMakeRect(graphRect.origin.x + graphRect.size.width - 1, axis_y + tickoff3, 1, 3));
	
	// Draw the axis labels
	std::ostringstream ss;
	ss << axis_max;
	std::string ss_str = ss.str();
	NSString *axis_max_pretty_string = [NSString stringWithUTF8String:ss_str.c_str()];
	
	NSString *axis_min_label = (axis_min == 0.0 ? @"0" : @"−1");
	NSString *axis_half_label = (axis_min == 0.0 ? @"0.5" : (axis_max == 0.0 ? @"−0.5" : @"0"));
	NSString *axis_max_label = (custom_axis_max ? axis_max_pretty_string : (axis_max == 0.0 ? @"0" : @"1"));
	NSSize min_label_size = [axis_min_label sizeWithAttributes:labelAttrs];
	NSSize half_label_size = [axis_half_label sizeWithAttributes:labelAttrs];
	NSSize max_label_size = [axis_max_label sizeWithAttributes:labelAttrs];
	double min_label_halfwidth = round(min_label_size.width / 2.0);
	double half_label_halfwidth = round(half_label_size.width / 2.0);
	double max_label_halfwidth = round(max_label_size.width / 2.0);
	CGFloat label_y = (heights_negative ? bounds.origin.y + bounds.size.height - 12 : bounds.origin.y + 1);
	
	[axis_min_label drawAtPoint:NSMakePoint(bounds.origin.x + 7 - min_label_halfwidth, label_y) withAttributes:labelAttrs];
	if (draw_axis_midpoint)
		[axis_half_label drawAtPoint:NSMakePoint(bounds.origin.x + 38.5 - half_label_halfwidth, label_y) withAttributes:labelAttrs];
	if (custom_axis_max)
		[axis_max_label drawAtPoint:NSMakePoint(bounds.origin.x + 72 - round(max_label_size.width), label_y) withAttributes:labelAttrs];
	else
		[axis_max_label drawAtPoint:NSMakePoint(bounds.origin.x + 70 - max_label_halfwidth, label_y) withAttributes:labelAttrs];
	
	// If we had an exception while drawing values, just show a question mark and return
	if (mut_type && !draws.size())
	{
		NSString *questionMark = @"?";
		NSSize q_size = [questionMark sizeWithAttributes:questionMarkAttrs];
		double q_halfwidth = round(q_size.width / 2.0);
		
		[questionMark drawAtPoint:NSMakePoint(bounds.origin.x + bounds.size.width / 2.0 - q_halfwidth, bounds.origin.y + 22) withAttributes:questionMarkAttrs];
		return;
	}
	
	NSRect interiorRect = NSMakeRect(graphRect.origin.x, graphRect.origin.y + (heights_negative ? 0 : 2), graphRect.size.width, graphRect.size.height - 2);
	
	// Tabulate the distribution from the samples we took; the math here is a bit subtle, because when we are doing a -1 to +1 axis
	// we want those values to fall at bin centers, but when we're doing 0 to +1 or -1 to 0 we want 0 to fall at the bin edge.
	int half_bin_count = (int)round(interiorRect.size.width);
	int bin_count = half_bin_count * 2;								// 2x bins to look nice on Retina displays
	double *bins = (double *)calloc(bin_count, sizeof(double));
	
	if (sample_size)
	{
		// sample-based tabulation into a histogram; mutation types only, right now
		for (int sample_count = 0; sample_count < sample_size; ++sample_count)
		{
			double sel_coeff = draws[sample_count];
			int bin_index;
			
			if ((axis_min == -1.0) && (axis_max == 1.0))
				bin_index = (int)floor(((sel_coeff + 1.0) / 2.0) * (bin_count - 1) + 0.5);
			else if ((axis_min == -1.0) && (axis_max == 0.0))
				bin_index = (int)ceil((sel_coeff + 1.0) * (bin_count - 1 - 0.5) + 0.5);		// 0.0 maps to bin_count - 1, -1.0 maps to the center of bin 0
			else // if ((axis_min == 0.0) && (axis_max == 1.0))
				bin_index = (int)floor(sel_coeff * (bin_count - 1 + 0.5));					// 0.0 maps to 0, 1.0 maps to the center of bin_count - 1
			
			if ((bin_index >= 0) && (bin_index < bin_count))
				bins[bin_index]++;
		}
	}
	else
	{
		// non-sample-based construction of a function by evaluation; interaction types only, right now
		double max_x = interaction_type->max_distance_;
		
		for (int bin_index = 0; bin_index < bin_count; ++bin_index)
		{
			double bin_left = (bin_index / (double)bin_count) * axis_max;
			double bin_right = ((bin_index + 1) / (double)bin_count) * axis_max;
			double total_value = 0.0;
			
			for (int evaluate_index = 0; evaluate_index <= 999; ++evaluate_index)
			{
				double evaluate_x = bin_left + (bin_right - bin_left) / 999;
				
				if (evaluate_x < max_x)
					total_value += interaction_type->CalculateStrengthNoCallbacks(evaluate_x);
			}
			
			bins[bin_index] = total_value / 1000.0;
		}
	}
	
	// If we only have samples equal to zero, replicate the center column for symmetry
	if (!draw_positive && !draw_negative)
	{
		double zero_count = std::max(bins[half_bin_count - 1], bins[half_bin_count]);	// whichever way it rounds...
		
		bins[half_bin_count - 1] = zero_count;
		bins[half_bin_count] = zero_count;
	}
	
	// Find the maximum-magnitude bin count
	double max_bin = 0;
	
	if (heights_negative)
	{
		for (int bin_index = 0; bin_index < bin_count; ++bin_index)
			max_bin = std::min(max_bin, bins[bin_index]);
	}
	else
	{
		for (int bin_index = 0; bin_index < bin_count; ++bin_index)
			max_bin = std::max(max_bin, bins[bin_index]);
	}
	
	// Plot the bins
	[[NSColor colorWithCalibratedHue:0.15 saturation:0.0 brightness:0.0 alpha:1.0] set];
	
	if (heights_negative)
	{
		for (int bin_index = 0; bin_index < bin_count; ++bin_index)
		{
			if (bins[bin_index] < 0)
			{
				double height = interiorRect.size.height * (bins[bin_index] / max_bin);
				
				NSRectFill(NSMakeRect(interiorRect.origin.x + bin_index * 0.5, interiorRect.origin.y + interiorRect.size.height - height, 0.5, height));
			}
		}
	}
	else
	{
		for (int bin_index = 0; bin_index < bin_count; ++bin_index)
		{
			if (bins[bin_index] > 0)
				NSRectFill(NSMakeRect(interiorRect.origin.x + bin_index * 0.5, interiorRect.origin.y, 0.5, interiorRect.size.height * (bins[bin_index] / max_bin)));
		}
	}
	
	free(bins);
}

- (BOOL)isOpaque
{
	return YES;
}

@end

@implementation SLiMFunctionGraphToolTipWindow

// makes a new marker with no label and no tip point, not shown
+ (instancetype)new
{
	return [[[self class] alloc] initWithContentRect:NSMakeRect(0, 0, 77, 50) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
}

- (instancetype)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)aStyle backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag
{
	if (self = [super initWithContentRect:contentRect styleMask:aStyle backing:bufferingType defer:flag])
	{
		[self setFloatingPanel:YES];
		[self setBecomesKeyOnlyIfNeeded:YES];
		[self setHasShadow:NO];
		[self setOpaque:NO];
		[self setBackgroundColor:[NSColor clearColor]];
		
		SLiMFunctionGraphToolTipView *view = [[SLiMFunctionGraphToolTipView alloc] initWithFrame:contentRect];
		
		[self setContentView:view];
		[view release];
		
		_tipPoint = NSMakePoint(contentRect.origin.x, contentRect.origin.y);
		_mutType = nullptr;
		_interactionType = nullptr;
	}
	
	return self;
}

- (void)setMutType:(MutationType *)mutType
{
	if (_mutType != mutType)
	{
		_mutType = mutType;
		
		[[self contentView] setNeedsDisplay:YES];
	}
}

- (void)setInteractionType:(InteractionType *)interactionType
{
	if (_interactionType != interactionType)
	{
		_interactionType = interactionType;
		
		[[self contentView] setNeedsDisplay:YES];
	}
}

- (void)setTipPoint:(NSPoint)tipPoint
{
	if (!NSEqualPoints(_tipPoint, tipPoint))
	{
		NSPoint origin = [self frame].origin;
		
		origin.x += (tipPoint.x - _tipPoint.x);
		origin.y += (tipPoint.y - _tipPoint.y);
		
		_tipPoint = tipPoint;
		
		[self setFrameOrigin:origin];
	}
}

@end


@implementation NSScreen (SLiMWindowFrames)

+ (BOOL)visibleCandidateWindowFrame:(NSRect)candidateFrame
{
	NSArray *screens = [NSScreen screens];
	NSUInteger nScreens = [screens count];
	
	for (NSUInteger i = 0; i < nScreens; ++i)
	{
		NSScreen *screen = [screens objectAtIndex:i];
		NSRect screenFrame = [screen visibleFrame];
		
		if (NSContainsRect(screenFrame, candidateFrame))
			return YES;
	}
	
	return NO;
}

@end


@implementation NSPopUpButton (SLiMSorting)

- (void)slimSortMenuItemsByTag
{
	NSMenu *menu = [self menu];
	int nItems = (int)[menu numberOfItems];
	
	// completely dumb bubble sort; not worth worrying about
	do
	{
		BOOL foundSwap = NO;
		
		for (int i = 0; i < nItems - 1; ++i)
		{
			NSMenuItem *firstItem = [menu itemAtIndex:i];
			NSMenuItem *secondItem = [menu itemAtIndex:i + 1];
			NSInteger firstTag = [firstItem tag];
			NSInteger secondTag = [secondItem tag];
			
			if (firstTag > secondTag)
			{
				[secondItem retain];
				[menu removeItemAtIndex:i + 1];
				[menu insertItem:secondItem atIndex:i];
				[secondItem release];
				
				foundSwap = YES;
			}
		}
		
		if (!foundSwap)
			break;
	}
	while (YES);
}

@end

@implementation NSButton (SLiMTinting)

- (void)slimSetTintColor:(NSColor *)tintColor
{
	[self setContentFilters:[NSArray array]];
	
	if (tintColor)
	{
		if (!self.layer)
			[self setWantsLayer:YES];
		
		CIFilter *tintFilter = [CIFilter filterWithName:@"CIColorMatrix"];
		
		if (tintFilter)
		{
			CGFloat redComponent = [tintColor redComponent];
			CGFloat greenComponent = [tintColor greenComponent];
			CGFloat blueComponent = [tintColor blueComponent];
			
			//NSLog(@"tintColor: redComponent == %f, greenComponent == %f, blueComponent == %f", redComponent, greenComponent, blueComponent);
			
			// The goal is to use CIColorMatrix to multiply color components so that white turns into tintColor; these vectors do that
			CIVector *rVector = [CIVector vectorWithX:redComponent Y:0.0 Z:0.0 W:0.0];
			CIVector *gVector = [CIVector vectorWithX:0.0 Y:greenComponent Z:0.0 W:0.0];
			CIVector *bVector = [CIVector vectorWithX:0.0 Y:0.0 Z:blueComponent W:0.0];
			
			[tintFilter setDefaults];
			[tintFilter setValue:rVector forKey:@"inputRVector"];
			[tintFilter setValue:gVector forKey:@"inputGVector"];
			[tintFilter setValue:bVector forKey:@"inputBVector"];
			
			[self setContentFilters:@[tintFilter]];
		}
		else
		{
			NSLog(@"could not create [CIFilter filterWithName:@\"CIColorMatrix\"]");
		}
	}
	else
	{
		// BCH added 21 May 2017: revert to non-layer-backed when tinting is removed.  This restores the previous behavior
		// of the view, which is important for making the play and profile buttons draw correctly where they overlap.  The
		// doc does not explicitly say that setWantsLayer:NO removes the layer, but in practice it does seem to.
		if (self.layer)
			[self setWantsLayer:NO];
	}
	
	[self setNeedsDisplay];
	[self.layer setNeedsDisplay];
}

@end

@implementation SLiMAutoselectTextField

- (void)mouseDown:(NSEvent *)theEvent
{
	[[self currentEditor] selectAll:nil];
}

@end

@implementation SLiMLayoutRoundoffView

- (void)layout
{
	// We want to manually layout our (single) subview, to correct for subpixel positioning that screws up our drawing.
	// The documentation for -layout is abysmal, but it turns out what you want to do is remove all constraints from the
	// view that you want to manually lay out, so that the autolayout code is not fighting with you, and *then* use
	// -layout in the superview of that view to manually set the frame of the subview.  We remove the relevant constraints
	// in -windowDidLoad in SLiMWindowController.  Here we just check for a half-pixel position in our own bounds, and if
	// present, tweak our subview's frame to remove it.  Seems to work; took hours upon hours to figure out.  :-<
	NSArray *subviews = [self subviews];
	
	if ([subviews count] == 1)
	{
		NSView *subview = [subviews objectAtIndex:0];
		NSRect selfBounds = [self frame];
		NSRect frame = selfBounds;
		
		if (selfBounds.size.height != round(selfBounds.size.height))
		{
			frame.origin.y += 0.5;
			frame.size.height -= 0.5;
		}
		
		//NSLog(@"selfBounds: %@", NSStringFromRect(selfBounds));
		//NSLog(@"frame: %@", NSStringFromRect(frame));
		
		[subview setFrame:frame];
		[subview setNeedsLayout:YES];
	}
	
	[super layout];
}

@end


// Work around Apple's bug that they never fix that causes console logs on startup
// Thanks to TyngJJ on https://forums.developer.apple.com/thread/49052 for this workaround
// FIXME check whether they have fixed it yet, from time to time...

@interface NSWindow (FirstResponding)  
-(void)_setFirstResponder:(NSResponder *)responder;  
@end  
@interface NSDrawerWindow : NSWindow  
@end  
@implementation NSDrawerWindow (FirstResponding)  
-(void)_setFirstResponder:(NSResponder *)responder {  
	if (![responder isKindOfClass:NSView.class] || [(NSView *)responder window] == self)  
		[super _setFirstResponder:responder];  
}  
@end  

@implementation NSString (SLiMBytes)
+ (NSString *)stringForByteCount:(int64_t)bytes
{
	if (bytes > 512L * 1024L * 1024L * 1024L)
		return [NSString stringWithFormat:@"%0.2f TB", bytes / (1024.0 * 1024.0 * 1024.0 * 1024.0)];
	else if (bytes > 512L * 1024L * 1024L)
		return [NSString stringWithFormat:@"%0.2f GB", bytes / (1024.0 * 1024.0 * 1024.0)];
	else if (bytes > 512L * 1024L)
		return [NSString stringWithFormat:@"%0.2f MB", bytes / (1024.0 * 1024.0)];
	else if (bytes > 512L)
		return [NSString stringWithFormat:@"%0.2f KB", bytes / 1024.0];
	else
		return [NSString stringWithFormat:@"%lld bytes", bytes];
}
@end

@implementation NSColor (SLiMHeatColors)
#define SLIM_YELLOW_FRACTION 0.10
#define SLIM_SATURATION 0.75

+ (NSColor *)slimColorForFraction:(double)fraction
{
	if (fraction < SLIM_YELLOW_FRACTION)
	{
		// small fractions fall on a ramp from white (0.0) to yellow (SLIM_YELLOW_FRACTION)
		return [NSColor colorWithCalibratedHue:(1.0 / 6.0) saturation:(fraction / SLIM_YELLOW_FRACTION) * SLIM_SATURATION brightness:1.0 alpha:1.0];
	}
	else
	{
		// larger fractions ramp from yellow (SLIM_YELLOW_FRACTION) to red (1.0)
		return [NSColor colorWithCalibratedHue:(1.0 / 6.0) * (1.0 - (fraction - SLIM_YELLOW_FRACTION) / (1.0 - SLIM_YELLOW_FRACTION)) saturation:SLIM_SATURATION brightness:1.0 alpha:1.0];
	}
}
@end

@implementation NSAttributedString (SLiMBytes)
+ (NSAttributedString *)attributedStringForByteCount:(int64_t)bytes total:(double)total attributes:(NSDictionary *)attrs
{
	NSString *byteString = [NSString stringForByteCount:bytes];
	double fraction = bytes / total;
	NSColor *fractionColor = [NSColor slimColorForFraction:fraction];
	NSMutableDictionary *colorAttrs = [NSMutableDictionary dictionaryWithDictionary:attrs];
	
	[colorAttrs setObject:fractionColor forKey:NSBackgroundColorAttributeName];
	
	return [[[NSAttributedString alloc] initWithString:byteString attributes:colorAttrs] autorelease];
}
@end

// Create a path for a temporary file; see https://stackoverflow.com/a/8307013/2752221
// Code is originally from https://developer.apple.com/library/archive/samplecode/SimpleURLConnections/Introduction/Intro.html#//apple_ref/doc/uid/DTS40009245
@implementation NSString (SLiMTempFiles)
+ (NSString *)slimPathForTemporaryFileWithPrefix:(NSString *)prefix
{
	NSString *  result;
	CFUUIDRef   uuid;
	CFStringRef uuidStr;
	
	uuid = CFUUIDCreate(NULL);
	assert(uuid != NULL);
	
	uuidStr = CFUUIDCreateString(NULL, uuid);
	assert(uuidStr != NULL);
	
	result = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"%@-%@", prefix, uuidStr]];
	assert(result != nil);
	
	CFRelease(uuidStr);
	CFRelease(uuid);
	
	return result;
}
@end





























