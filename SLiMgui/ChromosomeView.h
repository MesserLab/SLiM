//
//  ChromosomeView.h
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
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

#import "CocoaExtra.h"


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
@public
	// Selection
	BOOL hasSelection;
	int selectionFirstBase, selectionLastBase;
	
	// Tracking
	BOOL isTracking;
	int trackingStartBase, trackingLastBase;
	int trackingXAdjust;	// to keep the cursor stuck on a knob that is click-dragged
	SLiMSelectionMarker *startMarker, *endMarker;
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









































































