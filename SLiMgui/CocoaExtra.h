//
//  CocoaExtra.h
//  SLiM
//
//  Created by Ben Haller on 1/22/15.
//  Copyright (c) 2015 Messer Lab, http://messerlab.org/software/. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@class SLiMWindowController;


// An NSTableView subclass that avoids becoming first responder; annoying that this is necessary, sigh...
@interface SLiMTableView : NSTableView
@end

// An NSApplication subclass to direct the showHelp: method to our AppDelegate
@interface SLiMApp : NSApplication
@property (nonatomic, readonly) SLiMWindowController *mainSLiMWindowController;
@end

// An NSDocumentController subclass to make the Recent Documents menu work; well, maybe we don't need it after all...
@interface SLiMDocumentController : NSDocumentController
@end

// A view to show a color stripe for the range of values of a metric such as fitness or selection coefficient
@interface SLiMColorStripeView : NSView
@property (nonatomic) int metricToPlot;		// 1 == fitness, 2 == selection coefficient; this changes which function below gets called
@property (nonatomic) double scalingFactor;
@property (nonatomic) BOOL enabled;
@end

// A view that simply draws white; used as a shim in the script syntax help window
@interface SLiMWhiteView : NSView
@end

// A button that runs a pop-up menu when clicked
@interface SLiMMenuButton : NSButton
@property (nonatomic, retain) NSMenu *menu;
@end

// A cell that draws a color swatch, used in the genomic element type tableview; note it is a subclass of NSTextFieldCell only for IB convenience...
@interface SLiMColorCell : NSTextFieldCell
@end

void RGBForFitness(double fitness, float *colorRed, float *colorGreen, float *colorBlue, double scalingFactor);
void RGBForSelectionCoeff(double selectionCoeff, float *colorRed, float *colorGreen, float *colorBlue, double scalingFactor);




























































