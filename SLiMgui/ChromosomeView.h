//
//  ChromosomeView.h
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
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

#import "CocoaExtra.h"
#include "slim_globals.h"
#include "ChromosomeGLView.h"

#include <vector>


extern NSString *SLiMChromosomeSelectionChangedNotification;


@interface ChromosomeView : NSView
{
@public
	// Selection
	BOOL hasSelection;
	slim_position_t selectionFirstBase, selectionLastBase;
	
	// Selection memory – saved and restored across events like recycles
	BOOL savedHasSelection;
	slim_position_t savedSelectionFirstBase, savedSelectionLastBase;
	
	// Tracking
	BOOL isTracking;
	slim_position_t trackingStartBase, trackingLastBase;
	int trackingXAdjust;	// to keep the cursor stuck on a knob that is click-dragged
	SLiMSelectionMarker *startMarker, *endMarker;
	
	// OpenGL buffers
	float *glArrayVertices;
	float *glArrayColors;
	
	// Display options
	BOOL display_haplotypes_;						// if NO, displaying frequencies; if YES, displaying haplotypes
	int64_t *haplotype_previous_bincounts;			// used by SLiMHaplotypeManager to keep the sort order stable
	std::vector<slim_objectid_t> display_muttypes_;	// if empty, display all mutation types; otherwise, display only the muttypes chosen
}

@property (nonatomic, retain) ChromosomeView *referenceChromosomeView;		// asked for the range to display
@property (nonatomic, getter=isSelectable) BOOL selectable;
@property (nonatomic) BOOL enabled;

@property (nonatomic) BOOL shouldDrawGenomicElements;
@property (nonatomic) BOOL shouldDrawRateMaps;
@property (nonatomic) BOOL shouldDrawMutations;
@property (nonatomic) BOOL shouldDrawFixedSubstitutions;

@property (nonatomic, retain) IBOutlet ChromosomeGLView *proxyGLView;	// we can have an OpenGL subview to do our interior drawing for us

- (NSRange)selectedRange;
- (void)setSelectedRange:(NSRange)selectionRange;
- (void)restoreLastSelection;

- (NSRange)displayedRange;	// either the full chromosome range, or the selected range of our reference chromosome

- (void)setNeedsDisplayInInterior;	// set to need display only within the interior; avoid redrawing ticks unnecessarily

// called by proxyGLView to do OpenGL drawing in the interior
- (void)glDrawRect:(NSRect)dirtyRect;

@end









































































