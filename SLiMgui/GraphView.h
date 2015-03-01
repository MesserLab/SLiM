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
	BOOL generatingPDF;		// set to YES during a copy: operation, to allow customization
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

+ (NSString *)labelFontName;
+ (NSDictionary *)attributesForAxisLabels;
+ (NSDictionary *)attributesForTickLabels;
+ (NSDictionary *)attributesForLegendLabels;
+ (NSColor *)gridLineColor;

- (id)initWithFrame:(NSRect)frameRect;		// designated initializer

- (void)setXAxisLabelString:(NSString *)labelString;
- (void)setYAxisLabelString:(NSString *)labelString;

- (NSRect)interiorRectForBounds:(NSRect)bounds;

- (double)plotToDeviceX:(double)plotx withInteriorRect:(NSRect)interiorRect;
- (double)plotToDeviceY:(double)ploty withInteriorRect:(NSRect)interiorRect;
- (double)roundPlotToDeviceX:(double)plotx withInteriorRect:(NSRect)interiorRect;	// rounded off to the nearest midpixel
- (double)roundPlotToDeviceY:(double)ploty withInteriorRect:(NSRect)interiorRect;	// rounded off to the nearest midpixel

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

- (NSMenu *)menuForEvent:(NSEvent *)theEvent;

@end




























































