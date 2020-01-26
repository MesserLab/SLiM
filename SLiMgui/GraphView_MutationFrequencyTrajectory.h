//
//  GraphView_MutationFrequencyTrajectory.h
//  SLiM
//
//  Created by Ben Haller on 3/11/15.
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


/*
 
 This graph plots frequency trajectories over time of all of the individual mutations within a mutation type.
 
 */


#import "GraphView.h"

#include "mutation.h"
#include "mutation_type.h"


@class MutationFrequencyHistory;


@interface GraphView_MutationFrequencyTrajectory : GraphView
{
	NSMutableDictionary *frequencyHistoryDict;				// dictionary of MutationFrequencyHistory objects with NSNumber (long long) keys
	NSMutableArray *frequencyHistoryColdStorageLost;		// array of MutationFrequencyHistory objects that have been lost
	NSMutableArray *frequencyHistoryColdStorageFixed;		// array of MutationFrequencyHistory objects that have been fixed
	
	NSPopUpButton *subpopulationButton;
	NSPopUpButton *mutationTypeButton;
	
	slim_generation_t lastGeneration;						// the last generation data was gathered for; used to detect a backward move in time
}

// The subpop and mutation type selected; -1 indicates no current selection (which will be fixed as soon as the menu is populated)
@property (nonatomic) slim_objectid_t selectedSubpopulationID;
@property (nonatomic) int selectedMutationTypeIndex;
@property (nonatomic) BOOL plotLostMutations;
@property (nonatomic) BOOL plotFixedMutations;
@property (nonatomic) BOOL plotActiveMutations;
@property (nonatomic) BOOL useColorsForPlotting;

- (NSPopUpButton *)addPopUpWithAction:(SEL)action;
- (BOOL)addSubpopulationsToMenu;
- (BOOL)addMutationTypesToMenu;
- (void)setConstraintsForPopups;

- (IBAction)subpopulationPopupChanged:(id)sender;
- (IBAction)mutationTypePopupChanged:(id)sender;

@end


// We want to keep a history of frequency values for each mutation of the chosen mutation type in the
// chosen subpopulation.  The history of a mutation should persist after it has vanished, and if a
// new mutation object gets allocated at the same memory location, it should be treated as a distinct
// mutation; so we can't use pointers to identify mutations.  Instead, we keep data on them using a
// unique 64-bit ID generated only when SLiM is running under SLiMgui.  At the end of a generation,
// we loop through all mutations in the registry, and add an entry for that mutation in our data store.
// This is probably O(n^2), but so it goes.  It should only be used for mutation types that generate
// few mutations; if somebody tries to plot every mutation in a common mutation-type, they will suffer.

@interface MutationFrequencyHistory : NSObject
{
@public
	// The 64-bit mutation ID is how we keep track of the mutation we reference; its pointer might go stale and be reused.
	slim_mutationid_t mutationID;
	
	// Mostly we are just a malloced array of uint16s.  The data we're storing is doubles, conceptually, but to minimize our memory footprint
	// (which might be very large!) we convert the doubles, which are guaranteed to be in the range [0.0, 1.0], to uint16s in the range
	// [0, UINT16_MAX] (65535).  The buffer size is the number of entries allocated, the entry count is the number used so far, and the
	// base generation is the first generation recorded; the assumption is that entries are then sequential without gaps.
	int bufferSize, entryCount;
	slim_generation_t baseGeneration;
	uint16_t *entries;
	
	// Remember our mutation type so we can set our line color, etc., if we wish
	const MutationType *mutationType;
	
	// Finally, we keep a flag that we use to figure out if our mutation is dead; if it is, we can be moved into cold storage.
	BOOL updated;
}

- (instancetype)initWithEntry:(uint16_t)value forMutation:(const Mutation *)mutation atBaseGeneration:(slim_generation_t)generation;
- (void)addEntry:(uint16_t)value;

@end


























































