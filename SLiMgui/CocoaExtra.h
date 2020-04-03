//
//  CocoaExtra.h
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


#import <Cocoa/Cocoa.h>


@class SLiMWindowController;
class MutationType;
class InteractionType;

// Returns true if we are running under the debugger
bool SLiM_AmIBeingDebugged(void);

// An NSTableView subclass that avoids becoming first responder; annoying that this is necessary, sigh...
@interface SLiMTableView : NSTableView
@end

// A view to show a color stripe for the range of values of a metric such as fitness or selection coefficient
@interface SLiMColorStripeView : NSView
@property (nonatomic) int metricToPlot;		// 1 == fitness, 2 == selection coefficient; this changes which function below gets called
@property (nonatomic) double scalingFactor;
@property (nonatomic) BOOL enabled;
@end

// A button that runs a pop-up menu when clicked
@interface SLiMMenuButton : NSButton
@property (nonatomic, retain) NSMenu *slimMenu;
@end

// A cell that draws a color swatch, used in the genomic element type tableview; note it is a subclass of NSTextFieldCell only for IB convenience...
@interface SLiMColorCell : NSTextFieldCell
@end

void RGBForFitness(double fitness, float *colorRed, float *colorGreen, float *colorBlue, double scalingFactor);
void RGBForSelectionCoeff(double selectionCoeff, float *colorRed, float *colorGreen, float *colorBlue, double scalingFactor);

// Classes to show a selection index marker when dragging out a selection in a ChromosomeView
@interface SLiMSelectionView : NSView
@end

@interface SLiMSelectionMarker : NSPanel
@property (nonatomic, retain) NSString *label;
@property (nonatomic) NSPoint tipPoint;
@property (nonatomic) BOOL isLeftMarker;

+ (instancetype)new;	// makes a new marker, not shown; set it up with a label and tip point and then call orderFront:

@end

// Classes to show a custom tooltip view; this code is derived from SLiMSelectionView / SLiMSelectionMarker
// This is not a general-purpose class really; it is specifically for labeling the Play Speed slider
@interface SLiMPlaySliderToolTipView : NSView
@end

@interface SLiMPlaySliderToolTipWindow : NSPanel
@property (nonatomic, retain) NSString *label;
@property (nonatomic) NSPoint tipPoint;

+ (instancetype)new;	// makes a new marker, not shown; set it up with a label and tip point and then call orderFront:

@end

// Classes to show a custom tooltip view displaying a graph of a mutation type's DFE in the muttype table view
// Now also used for similarly displaying an interaction type's IF in the interaction type table view
@interface SLiMFunctionGraphToolTipView : NSView
@end

@interface SLiMFunctionGraphToolTipWindow : NSPanel
@property (nonatomic, assign) MutationType *mutType;
@property (nonatomic, assign) InteractionType *interactionType;
@property (nonatomic) NSPoint tipPoint;

+ (instancetype)new;	// makes a new marker, not shown; set it up with a mutType/intType and tip point and then call orderFront:

@end


// A category to help us position windows visibly
@interface NSScreen (SLiMWindowFrames)
+ (BOOL)visibleCandidateWindowFrame:(NSRect)candidateFrame;
@end

// A category to add sorting of menus
@interface NSPopUpButton (SLiMSorting)
- (void)slimSortMenuItemsByTag;
@end

// A category to add tinting of NSPopUpButton, used in the ScriptMod panels for validation
@interface NSButton (SLiMTinting)
- (void)slimSetTintColor:(NSColor *)tintColor;
@end

// A subclass to make an NSTextView that selects its content when clicked, for the generation textfield
@interface SLiMAutoselectTextField : NSTextField
@end

// A subclass for a view that forces its (single) subview to match its own bounds, except that a half-pixel
// alignment in this view will be corrected in the subview; this makes OpenGL views play nice with Retina.
// This code is not general-purpose at the moment, it works only specifically with SLiMgui's view setup.
@interface SLiMLayoutRoundoffView : NSView
@end

@interface NSString (SLiMBytes)
+ (NSString *)stringForByteCount:(int64_t)bytes;
@end

@interface NSColor (SLiMHeatColors)
+ (NSColor *)slimColorForFraction:(double)fraction;
@end

@interface NSAttributedString (SLiMBytes)
+ (NSAttributedString *)attributedStringForByteCount:(int64_t)bytes total:(double)total attributes:(NSDictionary *)attrs;
@end

// Create a path for a temporary file; see https://stackoverflow.com/a/8307013/2752221
// Code is originally from https://developer.apple.com/library/archive/samplecode/SimpleURLConnections/Introduction/Intro.html#//apple_ref/doc/uid/DTS40009245
@interface NSString (SLiMTempFiles)
+ (NSString *)slimPathForTemporaryFileWithPrefix:(NSString *)prefix;
@end


















































