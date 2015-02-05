//
//  ChromosomeView.h
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
//  Copyright (c) 2015 Messer Lab, http://messerlab.org/software/. All rights reserved.
//

#import <Cocoa/Cocoa.h>


extern NSString *SLiMChromosomeSelectionChangedNotification;

//
//	The back end of SLiM uses zero-based positions internally, while using 1-based positions in its input and output.  SLiMgui follows this
//	convention, presenting 1-based positions to the user while interfacing to the 0-based back end.  ChromosomeView is the nexus where all
//	of this gets maximally confusing.  Whenever a position is taken from back end, or pushed out to the back end, a conversion must occur.
//	Positions kept by the front end, such as the selection and tracking positions below, are 1-based, since they live only in the GUI.
//	Tread carefully.
//

@interface ChromosomeView : NSView
{
	// Selection
	BOOL hasSelection;
	int selectionFirstBase, selectionLastBase;
	
	// Tracking
	BOOL isTracking;
	int trackingStartBase, trackingLastBase;
	int trackingXAdjust;	// to keep the cursor stuck on a knob that is click-dragged
}

@property (nonatomic, retain) ChromosomeView *referenceChromosomeView;		// asked for the range to display
@property (nonatomic, getter=isSelectable) BOOL selectable;
@property (nonatomic) BOOL enabled;

@property (nonatomic) BOOL shouldDrawGenomicElements;
@property (nonatomic) BOOL shouldDrawRecombinationIntervals;
@property (nonatomic) BOOL shouldDrawMutations;
@property (nonatomic) BOOL shouldDrawFixedSubstitutions;

- (NSRange)selectedRange;
- (void)setSelectedRange:(NSRange)selectionRange;

- (NSRange)displayedRange;	// either the full chromosome range, or the selected range of our reference chromosome

@end









































































