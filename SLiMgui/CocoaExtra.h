//
//  CocoaExtra.h
//  SLiM
//
//  Created by Ben Haller on 1/22/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
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



















































