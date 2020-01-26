//
//  SLiMHaplotypeManager.h
//  SLiM
//
//  Created by Ben Haller on 11/8/17.
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
class Genome;


// Display list data structures.  We map every Mutation in the registry to a struct we define that keeps the necessary information
// to display that mutation: position and color.  We use MutationIndex to index into a vector of those structs, using the same
// index values used by the registry for simplicity.  Each genome is then turned into a vector of MutationIndex that lets
// us plot the mutations for that genome.
typedef struct {
	slim_position_t position_;
	float red_, green_, blue_;
	BOOL neutral_;					// selection_coeff_ == 0.0, used to display neutral mutations under selected mutations
	BOOL display_;					// from the mutation type's mutation_type_displayed_ flag
} SLiMHaploMutation;

typedef enum
{
	kSLiMHaplotypeClusterNearestNeighbor,
	kSLiMHaplotypeClusterGreedy
} SLiMHaplotypeClusteringMethod;

typedef enum
{
	kSLiMHaplotypeClusterNoOptimization,
	kSLiMHaplotypeClusterOptimizeWith2opt
} SLiMHaplotypeClusteringOptimization;


// SLiMHaplotypeManager handles collecting information, clustering, building a display list, and drawing
@interface SLiMHaplotypeManager : NSObject
{
	SLiMHaplotypeClusteringMethod clusterMethod;
	SLiMHaplotypeClusteringOptimization clusterOptimization;
	
	// Genomes: note that this vector points back into SLiM's data structures, so using it is not safe in general.  It is used
	// by this class only while building the display list below; after that stage, we clear this vector.  The work to build the
	// display list gets done on a background thread, but the SLiMgui window is blocked by the progress panel during that time.
	std::vector<Genome *> genomes;
	
	// Display list
	SLiMHaploMutation *mutationInfo;						// a buffer of SLiMHaploMutation providing display information per mutation
	slim_position_t *mutationPositions;						// the same info as in mutationInfo, but in a single buffer for access efficiency
	slim_position_t mutationLastPosition;					// from the chromosome
	int mutationIndexCount;									// the number of MutationIndex values in use
	std::vector<std::vector<MutationIndex>> *displayList;	// a vector of genome information, where each genome is a vector of MutationIndex
	
	// Subpopulation information
	std::vector<slim_objectid_t> genomeSubpopIDs;			// the subpop ID for each genome, corresponding to the display list order
	slim_objectid_t maxSubpopID;
	slim_objectid_t minSubpopID;
	
	// Chromosome subrange information
	BOOL usingSubrange;
	slim_position_t subrangeFirstBase, subrangeLastBase;
	
	// Mutation type display information
	BOOL displayingMuttypeSubset;
}

@property (nonatomic, retain) NSString *titleString;
@property (nonatomic) int subpopCount;

- (instancetype)initWithClusteringMethod:(SLiMHaplotypeClusteringMethod)clusteringMethod
					  optimizationMethod:(SLiMHaplotypeClusteringOptimization)optimizationMethod
						sourceController:(SLiMWindowController *)controller
							  sampleSize:(int)sampleSize
					 clusterInBackground:(BOOL)clusterInBackground;

- (void)glDrawHaplotypesInRect:(NSRect)interior displayBlackAndWhite:(BOOL)displayBW showSubpopStrips:(BOOL)showSubpopStrips eraseBackground:(BOOL)eraseBackground previousFirstBincounts:(int64_t **)previousFirstBincounts;
- (NSBitmapImageRep *)bitmapImageRepForPlotInRect:(NSRect)interior displayBlackAndWhite:(BOOL)displayBW showSubpopStrips:(BOOL)showSubpopStrips;

@end
































