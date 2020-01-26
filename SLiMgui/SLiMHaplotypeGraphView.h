//
//  SLiMHaplotypeGraphView.h
//  SLiM
//
//  Created by Ben Haller on 11/6/17.
//  Copyright (c) 2017-2020 Philipp Messer.  All rights reserved.
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
#include "mutation.h"


@class SLiMWindowController;
@class SLiMHaplotypeManager;


// SLiMHaplotypeGraphView is an NSOpenGLView that provides a context for an SLiMHaplotypeManager that displays haplotypes in a window
@interface SLiMHaplotypeGraphView : NSOpenGLView
{
	SLiMHaplotypeManager *haplotypeManager;
}

@property (nonatomic) BOOL displayBlackAndWhite;
@property (nonatomic) BOOL showSubpopulationStrips;

- (void)configureForDisplayWithSlimWindowController:(SLiMWindowController *)controller;		// step one, starts background processing
- (void)configurePlotWindowWithSlimWindowController:(SLiMWindowController *)controller;		// step two, after background processing
- (void)cleanup;

- (IBAction)copy:(id)sender;
- (IBAction)saveGraph:(id)sender;

- (NSMenu *)menuForEvent:(NSEvent *)theEvent;

@end






























