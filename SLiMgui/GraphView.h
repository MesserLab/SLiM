//
//  GraphView.h
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


#import <Cocoa/Cocoa.h>


@class SLiMWindowController;


// A quick and dirty macro to enable rounding of coordinates to the nearest pixel only when we are not generating PDF
// FIXME this ought to be revisited in the light of Retina displays, printing, etc.
#define SCREEN_ROUND(x)		(generatingPDF ? (x) : round(x))


@interface GraphView : NSView
{
	// set to YES during a copy: operation, to allow customization
	BOOL generatingPDF;
	
	// caching for drawing speed is up to subclasses, if they want to do it, but we provide minimal support in GraphView to make it work smoothly
	// this flag is to prevent recursion in the drawing code, and to disable drawing of things that don't belong in a cache, such as the legend
	BOOL cachingNow;
}

@property (nonatomic, assign) SLiMWindowController *slimWindowController;

@property (nonatomic) BOOL showXAxis;
@property (nonatomic) BOOL showXAxisTicks;
@property (nonatomic) double xAxisMin, xAxisMax;
@property (nonatomic) double xAxisMajorTickInterval, xAxisMinorTickInterval;
@property (nonatomic) int xAxisMajorTickModulus, xAxisTickValuePrecision;
@property (nonatomic, retain) NSAttributedString *xAxisLabel;

@property (nonatomic) BOOL showYAxis;
@property (nonatomic) BOOL showYAxisTicks;
@property (nonatomic) double yAxisMin, yAxisMax;
@property (nonatomic) double yAxisMajorTickInterval, yAxisMinorTickInterval;
@property (nonatomic) int yAxisMajorTickModulus, yAxisTickValuePrecision;
@property (nonatomic, retain) NSAttributedString *yAxisLabel;

@property (nonatomic) BOOL legendVisible;
@property (nonatomic) BOOL showHorizontalGridLines;
@property (nonatomic) BOOL showVerticalGridLines;
@property (nonatomic) BOOL showFullBox;

@property (nonatomic) BOOL tweakXAxisTickLabelAlignment;

+ (NSString *)labelFontName;
+ (NSDictionary *)attributesForAxisLabels;
+ (NSDictionary *)attributesForTickLabels;
+ (NSDictionary *)attributesForLegendLabels;
+ (NSColor *)gridLineColor;

- (id)initWithFrame:(NSRect)frameRect withController:(SLiMWindowController *)controller;		// designated initializer

- (void)setXAxisLabelString:(NSString *)labelString;
- (void)setYAxisLabelString:(NSString *)labelString;

- (NSRect)interiorRectForBounds:(NSRect)bounds;

- (double)plotToDeviceX:(double)plotx withInteriorRect:(NSRect)interiorRect;
- (double)plotToDeviceY:(double)ploty withInteriorRect:(NSRect)interiorRect;
- (double)roundPlotToDeviceX:(double)plotx withInteriorRect:(NSRect)interiorRect;	// rounded off to the nearest midpixel
- (double)roundPlotToDeviceY:(double)ploty withInteriorRect:(NSRect)interiorRect;	// rounded off to the nearest midpixel

- (void)rescaleAsNeededWithInteriorRect:(NSRect)interiorRect andController:(SLiMWindowController *)controller;		// called prior to drawing, to allow dynamic axis rescaling

- (void)drawXAxisTicksWithInteriorRect:(NSRect)interiorRect;
- (void)drawXAxisWithInteriorRect:(NSRect)interiorRect;

- (void)drawYAxisTicksWithInteriorRect:(NSRect)interiorRect;
- (void)drawYAxisWithInteriorRect:(NSRect)interiorRect;

- (void)drawVerticalGridLinesWithInteriorRect:(NSRect)interiorRect;
- (void)drawHorizontalGridLinesWithInteriorRect:(NSRect)interiorRect;

- (void)drawInvalidMessageInRect:(NSRect)rect;

- (void)drawGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller;

- (NSSize)legendSize;
- (void)drawLegendInRect:(NSRect)legendRect;

- (IBAction)copy:(id)sender;
- (IBAction)copyData:(id)sender;
- (NSString *)dateline;

- (NSMenu *)menuForEvent:(NSEvent *)theEvent;

- (void)graphWindowResized;				// called by SLiMWindowController to let the GraphView do whatever recalculation, cache invalidation, etc. it might want to do
- (void)controllerRecycled;				// called by SLiMWindowController when the simulation is recycled, to let the GraphView do whatever re-initialization is needed
- (void)controllerSelectionChanged;		// called by SLiMWindowController when the selection changes, to let the GraphView respond
- (void)setNeedsDisplay;				// shorthand for setNeedsDisplay:YES, to allow use of performSelector:

@end

@interface GraphView (PrefabAdditions)

// a prefab legend that shows all of the mutation types, with color swatches and labels
- (NSSize)mutationTypeLegendSize;
- (void)drawMutationTypeLegendInRect:(NSRect)legendRect;

// a prefab method to draw simple barplots
- (void)drawBarplotInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller buffer:(double *)buffer binCount:(int)binCount firstBinValue:(double)firstBinValue binWidth:(double)binWidth;

// a prefab method to draw grouped barplots
- (void)drawGroupedBarplotInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller buffer:(double *)buffer subBinCount:(int)subBinCount mainBinCount:(int)mainBinCount firstBinValue:(double)firstBinValue mainBinWidth:(double)mainBinWidth;

@end

@interface GraphView (OptionalSubclassMethods)

- (NSString *)stringForDataWithController:(SLiMWindowController *)controller;

@end

























































