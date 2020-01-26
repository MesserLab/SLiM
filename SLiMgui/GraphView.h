//
//  GraphView.h
//  SLiM
//
//  Created by Ben Haller on 2/27/15.
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


#import <Cocoa/Cocoa.h>

#include "slim_globals.h"


@class SLiMWindowController;


// A quick and dirty macro to enable rounding of coordinates to the nearest pixel only when we are not generating PDF
// FIXME this ought to be revisited in the light of Retina displays, printing, etc.
#define SLIM_SCREEN_ROUND(x)		(generatingPDF ? (x) : round(x))


@interface GraphView : NSView
{
	// set to YES during a copy: operation, to allow customization
	BOOL generatingPDF;
	
	// caching for drawing speed is up to subclasses, if they want to do it, but we provide minimal support in GraphView to make it work smoothly
	// this flag is to prevent recursion in the drawing code, and to disable drawing of things that don't belong in a cache, such as the legend
	BOOL cachingNow;
	
	// GraphAxisRescaleSheet outlets
	IBOutlet NSWindow *rescaleSheet;
	IBOutlet NSTextField *rescaleSheetMinTextfield;
	IBOutlet NSTextField *rescaleSheetMaxTextfield;
	IBOutlet NSTextField *rescaleSheetMajorIntervalTextfield;
	IBOutlet NSTextField *rescaleSheetMinorModulusTextfield;
	IBOutlet NSTextField *rescaleSheetTickPrecisionTextfield;
	
	// GraphBarRescaleSheet outlets
	IBOutlet NSWindow *rescaleBarsSheet;
	IBOutlet NSTextField *rescaleBarsSheetCountTextfield;
}

@property (nonatomic, assign) SLiMWindowController *slimWindowController;

@property (nonatomic) BOOL showXAxis;
@property (nonatomic) BOOL allowXAxisUserRescale;
@property (nonatomic) BOOL xAxisIsUserRescaled;
@property (nonatomic) BOOL showXAxisTicks;
@property (nonatomic) double xAxisMin, xAxisMax;
@property (nonatomic) double xAxisMajorTickInterval, xAxisMinorTickInterval;
@property (nonatomic) int xAxisMajorTickModulus, xAxisTickValuePrecision;
@property (nonatomic, retain) NSAttributedString *xAxisLabel;

@property (nonatomic) BOOL showYAxis;
@property (nonatomic) BOOL allowYAxisUserRescale;
@property (nonatomic) BOOL yAxisIsUserRescaled;
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

- (void)cleanup;

- (void)setXAxisLabelString:(NSString *)labelString;
- (void)setYAxisLabelString:(NSString *)labelString;

- (NSRect)interiorRectForBounds:(NSRect)bounds;

- (double)plotToDeviceX:(double)plotx withInteriorRect:(NSRect)interiorRect;
- (double)plotToDeviceY:(double)ploty withInteriorRect:(NSRect)interiorRect;
- (double)roundPlotToDeviceX:(double)plotx withInteriorRect:(NSRect)interiorRect;	// rounded off to the nearest midpixel
- (double)roundPlotToDeviceY:(double)ploty withInteriorRect:(NSRect)interiorRect;	// rounded off to the nearest midpixel

- (void)willDrawWithInteriorRect:(NSRect)interiorRect andController:(SLiMWindowController *)controller;		// called prior to drawing, to allow dynamic axis rescaling and other adjustments

- (void)drawXAxisTicksWithInteriorRect:(NSRect)interiorRect;
- (void)drawXAxisWithInteriorRect:(NSRect)interiorRect;

- (void)drawYAxisTicksWithInteriorRect:(NSRect)interiorRect;
- (void)drawYAxisWithInteriorRect:(NSRect)interiorRect;

- (void)drawVerticalGridLinesWithInteriorRect:(NSRect)interiorRect;
- (void)drawHorizontalGridLinesWithInteriorRect:(NSRect)interiorRect;

- (void)drawMessage:(NSString *)messageString inRect:(NSRect)rect;

- (void)drawGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller;

- (NSArray *)legendKey;							// subclasses can provide an NSArray of key entries, each an NSArray of an NSString and an NSColor
- (NSSize)legendSize;							// unless overridden, this calls -legendKey and follows its intructions
- (void)drawLegendInRect:(NSRect)legendRect;	// unless overridden, this calls -legendKey and follows its intructions

- (IBAction)rescaleSheetOK:(id)sender;
- (IBAction)rescaleSheetCancel:(id)sender;
- (IBAction)userRescaleXAxis:(id)sender;
- (IBAction)userRescaleYAxis:(id)sender;

- (IBAction)copy:(id)sender;
- (IBAction)saveGraph:(id)sender;
- (IBAction)copyData:(id)sender;
- (IBAction)saveData:(id)sender;
- (NSString *)dateline;

- (NSMenu *)menuForEvent:(NSEvent *)theEvent;
- (void)subclassAddItemsToMenu:(NSMenu *)menu forEvent:(NSEvent *)theEvent;	// subclasses normally add their menu items here so they appear above the copy graph/data menu items

- (void)invalidateDrawingCache;			// GraphView does not keep a drawing cache, but it supports having one; this gets called in situations when such a cache would be invalidated
- (void)graphWindowResized;				// called by SLiMWindowController to let the GraphView do whatever recalculation, cache invalidation, etc. it might want to do
- (void)controllerRecycled;				// called by SLiMWindowController when the simulation is recycled, to let the GraphView do whatever re-initialization is needed
- (void)controllerSelectionChanged;		// called by SLiMWindowController when the selection changes, to let the GraphView respond
- (void)controllerGenerationFinished;	// called by SLiMWindowController when a simulation generation ends, to allow per-generation data gathering; redrawing should not be done here
- (void)updateAfterTick;				// by default, calls setNeedsDisplay:YES; can also perform other updating work

// Additional properties that conceptually belong to PrefabAdditions below
@property (nonatomic) int histogramBinCount;		// provided for barplots
@property (nonatomic) BOOL allowXAxisBinRescale;	// if YES, the GraphView will provide a context menu item and run a panel to set the bar count

@end

@interface GraphView (PrefabAdditions)

// A prefab method to set up a good x axis to span the generation range, whatever it might be
- (void)setXAxisRangeFromGeneration;

// a prefab legend that shows all of the mutation types, with color swatches and labels
- (NSArray *)mutationTypeLegendKey;

// a prefab method to draw simple barplots
- (void)drawBarplotInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller buffer:(double *)buffer binCount:(int)binCount firstBinValue:(double)firstBinValue binWidth:(double)binWidth;

// a prefab method to draw grouped barplots
- (void)drawGroupedBarplotInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller buffer:(double *)buffer subBinCount:(int)subBinCount mainBinCount:(int)mainBinCount firstBinValue:(double)firstBinValue mainBinWidth:(double)mainBinWidth;

@end

@interface GraphView (OptionalSubclassMethods)

- (NSString *)stringForDataWithController:(SLiMWindowController *)controller;

@end

























































