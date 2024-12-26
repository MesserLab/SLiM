//
//  ChromosomeView.h
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
//  Copyright (c) 2015-2024 Philipp Messer.  All rights reserved.
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

#include <vector>


class Chromosome;

extern NSString *SLiMChromosomeSelectionChangedNotification;


@interface ChromosomeView : NSView
{
@public
	// Display options
	std::vector<slim_objectid_t> display_muttypes_;	// if empty, display all mutation types; otherwise, display only the muttypes chosen
}

@property (nonatomic) BOOL enabled;
@property (nonatomic) BOOL overview;

@property (nonatomic) BOOL shouldDrawGenomicElements;
@property (nonatomic) BOOL shouldDrawRateMaps;
@property (nonatomic) BOOL shouldDrawMutations;
@property (nonatomic) BOOL shouldDrawFixedSubstitutions;

- (NSRange)displayedRangeForChromosome:(Chromosome *)chromosome;	// the full chromosome range

- (void)setNeedsDisplayInInterior;	// set to need display only within the interior; avoid redrawing ticks unnecessarily

@end









































































