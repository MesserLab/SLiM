//
//  SLiMHaplotypeManager.mm
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


#import "SLiMHaplotypeManager.h"
#import "SLiMWindowController.h"

#import <OpenGL/OpenGL.h>
#include <OpenGL/glu.h>
#include <GLKit/GLKMatrix4.h>

#include <vector>
#include <algorithm>

#include "eidos_globals.h"


@implementation SLiMHaplotypeManager

- (instancetype)initWithClusteringMethod:(SLiMHaplotypeClusteringMethod)clusteringMethod
					  optimizationMethod:(SLiMHaplotypeClusteringOptimization)optimizationMethod
						sourceController:(SLiMWindowController *)controller
							  sampleSize:(int)sampleSize
					 clusterInBackground:(BOOL)clusterInBackground;
{
	if (self = [super init])
	{
		SLiMSim *sim = controller->sim;
		Population &population = sim->population_;
		
		clusterMethod = clusteringMethod;
		clusterOptimization = optimizationMethod;
		
		// Figure out which subpops are selected (or if none are, consider all to be); we will display only the selected subpops
		std::vector<Subpopulation *> selected_subpops;
		
		for (auto subpop_pair : population.subpops_)
			if (subpop_pair.second->gui_selected_)
				selected_subpops.push_back(subpop_pair.second);
		
		if (selected_subpops.size() == 0)
			for (auto subpop_pair : population.subpops_)
				selected_subpops.push_back(subpop_pair.second);
		
		// Figure out whether we're analyzing / displaying a subrange; gross that we go right into the ChromosomeView, I know...
		ChromosomeView *overviewChromosomeView = controller->chromosomeOverview;
		
		if (overviewChromosomeView->hasSelection)
		{
			usingSubrange = YES;
			subrangeFirstBase = overviewChromosomeView->selectionFirstBase;
			subrangeLastBase = overviewChromosomeView->selectionLastBase;
		}
		
		// Also dig to find out whether we're displaying all mutation types or just a subset; if a subset, each MutationType has a display flag
		displayingMuttypeSubset = (controller->chromosomeZoomed->display_muttypes_.size() != 0);
		
		// Set our window title from the controller's state
		NSString *title = @"";
		
		if (selected_subpops.size() == 0)
		{
			// If there are no subpops (which can happen at the very start of running a model, for example), use a dash
			title = @"–";
		}
		else
		{
			bool first_subpop = true;
			
			for (Subpopulation *subpop : selected_subpops)
			{
				if (!first_subpop)
					title = [title stringByAppendingString:@" "];
				
				title = [title stringByAppendingFormat:@"p%d", (int)subpop->subpopulation_id_];
				
				first_subpop = false;
			}
		}
		
		if (usingSubrange)
			title = [title stringByAppendingFormat:@", positions %lld:%lld", (int64_t)subrangeFirstBase, (int64_t)subrangeLastBase];
		
		title = [title stringByAppendingFormat:@", generation %d", (int)sim->generation_];
		
		[self setTitleString:title];
		[self setSubpopCount:(int)selected_subpops.size()];
		
		// Fetch genomes and figure out what we're going to plot; note that we plot only non-null genomes
		for (Subpopulation *subpop : selected_subpops)
			for (Genome *genome : subpop->parent_genomes_)
				if (!genome->IsNull())
					genomes.push_back(genome);
		
		// If a sample is requested, select that now; sampleSize <= 0 means no sampling
		if ((sampleSize > 0) && ((int)genomes.size() > sampleSize))
		{
			Eidos_random_unique(genomes.begin(), genomes.end(), sampleSize);
			genomes.resize(sampleSize);
		}
		
		// Cache all the information about the mutations that we're going to need
		[self configureMutationInfoBufferForController:controller];
		
		// Keep track of the range of subpop IDs we reference, even if not represented by any genomes here
		maxSubpopID = 0;
		minSubpopID = SLIM_MAX_ID_VALUE;
		
		for (Subpopulation *subpop : selected_subpops)
		{
			slim_objectid_t subpop_id = subpop->subpopulation_id_;
			
			minSubpopID = std::min(minSubpopID, subpop_id);
			maxSubpopID = std::max(maxSubpopID, subpop_id);
		}
		
		// Do the rest in the foreground or background as requested
		if (clusterInBackground)
		{
			// start up a progress sheet since we could take a while...
			[controller runHaplotypePlotProgressSheetWithGenomeCount:(int)genomes.size()];
			
			[self performSelectorInBackground:@selector(finishClusteringAnalysisWithBackgroundController:) withObject:controller];
		}
		else
		{
			[self finishClusteringAnalysisWithBackgroundController:nil];	// no controller needed when clustering in the foreground
		}
	}
	
	return self;
}

- (void)finishClusteringAnalysisWithBackgroundController:(SLiMWindowController *)backgroundController
{
	// This method and everything is calls may be executed on a background thread!  If so, backgroundController
	// will be non-nil, and provides the APIs we need to talk to the progress panel.  If we are running in the
	// foreground, backgroundController will be nil.
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	
	// If we're in the background, tell our backgroundController that we're starting
	[backgroundController haplotypeProgressTaskStarting];
	
#if 0
	// Choose a random order
	std::random_shuffle(genomes.begin(), genomes.end());
#else
	// Work out an approximate best sort order
	[self sortGenomesWithBackgroundController:backgroundController];
#endif
	
	if (![backgroundController haplotypeProgressIsCancelled])
	{
		// Remember the subpop ID for each genome
		for (Genome *genome : genomes)
			genomeSubpopIDs.push_back(genome->subpop_->subpopulation_id_);
		
		// Build our plotting data vectors.  Because we are a snapshot, we can't rely on our controller's data
		// at all after this method returns; we have to remember everything we need to create our display list.
		[self configureDisplayBuffers];
	}
	
	// Now we are done with the genomes vector; clear it
	genomes.clear();
	genomes.resize(0);
	
	// If we're in the background, tell our backgroundController that we're done
	[backgroundController haplotypeProgressTaskFinished];
	
	
	[pool release];
}

- (void)dealloc
{
	if (mutationInfo)
	{
		free(mutationInfo);
		mutationInfo = nil;
	}
	
	if (mutationPositions)
	{
		free(mutationPositions);
		mutationPositions = nil;
	}
	
	if (displayList)
	{
		delete displayList;
		displayList = nullptr;
	}
	
	[self setTitleString:nil];
	
	[super dealloc];
}

- (void)configureMutationInfoBufferForController:(SLiMWindowController *)controller
{
	SLiMSim *sim = controller->sim;
	Population &population = sim->population_;
	double scalingFactor = controller->selectionColorScale;
	int registry_size;
	const MutationIndex *registry = population.MutationRegistry(&registry_size);
	const MutationIndex *reg_ptr, *reg_end_ptr = registry + registry_size;
	MutationIndex biggest_index = 0;
	
	// First, find the biggest index presently in use; that's how many entries we need
	for (reg_ptr = registry ; reg_ptr != reg_end_ptr; ++reg_ptr)
	{
		MutationIndex mut_index = *reg_ptr;
		
		if (mut_index > biggest_index)
			biggest_index = mut_index;
	}
	
	// Allocate our mutationInfo buffer with entries for every MutationIndex in use
	mutationIndexCount = biggest_index + 1;
	mutationInfo = (SLiMHaploMutation *)malloc(sizeof(SLiMHaploMutation) * mutationIndexCount);
	mutationPositions = (slim_position_t *)malloc(sizeof(slim_position_t) * mutationIndexCount);
	
	// Copy the information we need on each mutation in use
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	for (reg_ptr = registry ; reg_ptr != reg_end_ptr; ++reg_ptr)
	{
		MutationIndex mut_index = *reg_ptr;
		const Mutation *mut = mut_block_ptr + mut_index;
		slim_position_t mut_position = mut->position_;
		const MutationType *mut_type = mut->mutation_type_ptr_;
		SLiMHaploMutation *haplo_mut = mutationInfo + mut_index;
		
		haplo_mut->position_ = mut_position;
		*(mutationPositions + mut_index) = mut_position;
		
		if (!mut_type->color_.empty())
		{
			haplo_mut->red_ = mut_type->color_red_;
			haplo_mut->green_ = mut_type->color_green_;
			haplo_mut->blue_ = mut_type->color_blue_;
		}
		else
		{
			RGBForSelectionCoeff(mut->selection_coeff_, &haplo_mut->red_, &haplo_mut->green_, &haplo_mut->blue_, scalingFactor);
		}
		
		haplo_mut->neutral_ = (mut->selection_coeff_ == 0.0);
		
		haplo_mut->display_ = mut_type->mutation_type_displayed_;
	}
	
	// Remember the chromosome length
	mutationLastPosition = sim->chromosome_.last_position_;
}

// Delegate the genome sorting to the appropriate method based on our configuration
- (void)sortGenomesWithBackgroundController:(SLiMWindowController *)backgroundController
{
	int64_t genome_count = (int64_t)genomes.size();
	
	if (genome_count == 0)
		return;
	
	std::vector<Genome *> original_genomes = genomes;	// copy the vector because we will need to reorder it below
	std::vector<int> final_path;
	
	// first get our distance matrix; these are inter-city distances
	int64_t *distances;
	
	if (displayingMuttypeSubset)
	{
		if (usingSubrange)
			distances = [self buildDistanceArrayForSubrangeAndSubtypesWithBackgroundController:backgroundController];
		else
			distances = [self buildDistanceArrayForSubtypesWithBackgroundController:backgroundController];
	}
	else
	{
		if (usingSubrange)
			distances = [self buildDistanceArrayForSubrangeWithBackgroundController:backgroundController];
		else
			distances = [self buildDistanceArrayWithBackgroundController:backgroundController];
	}
	
	if ([backgroundController haplotypeProgressIsCancelled])
		goto cancelledExit;
	
	switch (clusterMethod)
	{
		case kSLiMHaplotypeClusterNearestNeighbor:
			[self nearestNeighborSolveWithDistances:distances size:genome_count solution:final_path backgroundController:backgroundController];
			break;
			
		case kSLiMHaplotypeClusterGreedy:
			[self greedySolveWithDistances:distances size:genome_count solution:final_path backgroundController:backgroundController];
			break;
	}
	
	if ([backgroundController haplotypeProgressIsCancelled])
		goto cancelledExit;
	
	[self checkPath:final_path size:genome_count];
	
	if ([backgroundController haplotypeProgressIsCancelled])
		goto cancelledExit;
	
	if (clusterOptimization != kSLiMHaplotypeClusterNoOptimization)
	{
		switch (clusterOptimization)
		{
			case kSLiMHaplotypeClusterNoOptimization:
				break;
				
			case kSLiMHaplotypeClusterOptimizeWith2opt:
				[self do2optOptimizationOfSolution:final_path withDistances:distances size:genome_count backgroundController:backgroundController];
				break;
		}
		
		if ([backgroundController haplotypeProgressIsCancelled])
			goto cancelledExit;
		
		[self checkPath:final_path size:genome_count];
	}
	
	if ([backgroundController haplotypeProgressIsCancelled])
		goto cancelledExit;
	
	// reorder the genomes vector according to the path we found
	for (int genome_index = 0; genome_index < genome_count; ++genome_index)
		genomes[genome_index] = original_genomes[final_path[genome_index]];
	
cancelledExit:
	free(distances);
}

- (void)configureDisplayBuffers
{
	int64_t genome_count = (int64_t)genomes.size();
	
	// Allocate our display list and size it so it has one std::vector<MutationIndex> per genome
	displayList = new std::vector<std::vector<MutationIndex>>;
	
	displayList->resize(genome_count);
	
	// Then save off the information for each genome into the display list
	for (int genome_index = 0; genome_index < genome_count; ++genome_index)
	{
		Genome &genome = *genomes[genome_index];
		std::vector<MutationIndex> &genome_display = (*displayList)[genome_index];
		
		if (!usingSubrange)
		{
			// Size our display list to fit the number of mutations in the genome
			int mut_count = genome.mutation_count();
			
			genome_display.reserve(mut_count);
			
			// Loop through mutations to get the mutation indices
			int mutrun_count = genome.mutrun_count_;
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				MutationRun *mutrun = genome.mutruns_[run_index].get();
				const MutationIndex *mut_start_ptr = mutrun->begin_pointer_const();
				const MutationIndex *mut_end_ptr = mutrun->end_pointer_const();
				
				if (displayingMuttypeSubset)
				{
					// displaying a subset of mutation types, need to check
					for (const MutationIndex *mut_ptr = mut_start_ptr; mut_ptr < mut_end_ptr; ++mut_ptr)
					{
						MutationIndex mut_index = *mut_ptr;
						
						if ((mutationInfo + mut_index)->display_)
							genome_display.push_back(*mut_ptr);
					}
				}
				else
				{
					// displaying all mutation types, no need to check
					for (const MutationIndex *mut_ptr = mut_start_ptr; mut_ptr < mut_end_ptr; ++mut_ptr)
						genome_display.push_back(*mut_ptr);
				}
			}
		}
		else
		{
			// We are using a subrange, so we need to check the position of each mutation before adding it
			int mutrun_count = genome.mutrun_count_;
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				MutationRun *mutrun = genome.mutruns_[run_index].get();
				const MutationIndex *mut_start_ptr = mutrun->begin_pointer_const();
				const MutationIndex *mut_end_ptr = mutrun->end_pointer_const();
				
				if (displayingMuttypeSubset)
				{
					// displaying a subset of mutation types, need to check
					for (const MutationIndex *mut_ptr = mut_start_ptr; mut_ptr < mut_end_ptr; ++mut_ptr)
					{
						MutationIndex mut_index = *mut_ptr;
						slim_position_t mut_position = *(mutationPositions + mut_index);
						
						if ((mut_position >= subrangeFirstBase) && (mut_position <= subrangeLastBase))
							if ((mutationInfo + mut_index)->display_)
								genome_display.push_back(mut_index);
					}
				}
				else
				{
					// displaying all mutation types, no need to check
					for (const MutationIndex *mut_ptr = mut_start_ptr; mut_ptr < mut_end_ptr; ++mut_ptr)
					{
						MutationIndex mut_index = *mut_ptr;
						slim_position_t mut_position = *(mutationPositions + mut_index);
						
						if ((mut_position >= subrangeFirstBase) && (mut_position <= subrangeLastBase))
							genome_display.push_back(mut_index);
					}
				}
			}
		}
	}
}

static const int kMaxGLRects = 2000;				// 2000 rects
static const int kMaxVertices = kMaxGLRects * 4;	// 4 vertices each

static float *glArrayVertices = nil;
static float *glArrayColors = nil;

- (void)allocateGLBuffers
{
	// Set up the vertex and color arrays
	if (!glArrayVertices)
		glArrayVertices = (float *)malloc(kMaxVertices * 2 * sizeof(float));		// 2 floats per vertex, kMaxVertices vertices
	
	if (!glArrayColors)
		glArrayColors = (float *)malloc(kMaxVertices * 4 * sizeof(float));		// 4 floats per color, kMaxVertices colors
}

- (void)drawSubpopStripsInRect:(NSRect)interior
{
	int displayListIndex;
	float *vertices = NULL, *colors = NULL;
	
	// Set up to draw rects
	displayListIndex = 0;
	
	vertices = glArrayVertices;
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, glArrayVertices);
	
	colors = glArrayColors;
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_FLOAT, 0, glArrayColors);
	
	// Loop through the genomes and draw them; we do this in two passes, neutral mutations underneath selected mutations
	int genome_index = 0, genome_count = (int)genomeSubpopIDs.size();
	double height_divisor = genome_count;
	float left = (float)(interior.origin.x);
	float right = (float)(interior.origin.x + interior.size.width);
	NSColorSpace *rgbColorSpace = [NSColorSpace deviceRGBColorSpace];
	
	for (slim_objectid_t genome_subpop_id : genomeSubpopIDs)
	{
		float top = (float)(interior.origin.y + (genome_index / height_divisor) * interior.size.height);
		float bottom = (float)(interior.origin.y + ((genome_index + 1) / height_divisor) * interior.size.height);
		
		if (bottom - top > 1.0)
		{
			// If the range spans a width of more than one pixel, then use the maximal pixel range
			top = floorf(top);
			bottom = ceilf(bottom);
		}
		else
		{
			// If the range spans a pixel or less, make sure that we end up with a range that is one pixel wide, even if the positions span a pixel boundary
			top = floorf(top);
			bottom = top + 1;
		}
		
		*(vertices++) = left;		*(vertices++) = top;
		*(vertices++) = left;		*(vertices++) = bottom;
		*(vertices++) = right;		*(vertices++) = bottom;
		*(vertices++) = right;		*(vertices++) = top;
		
		float colorRed, colorGreen, colorBlue;
		float hue = (genome_subpop_id - minSubpopID) / (float)(maxSubpopID - minSubpopID + 1);
		NSColor *hsbColor = [NSColor colorWithCalibratedHue:hue saturation:1.0 brightness:1.0 alpha:1.0];
		NSColor *rgbColor = [hsbColor colorUsingColorSpace:rgbColorSpace];
		
		colorRed = (float)[rgbColor redComponent];
		colorGreen = (float)[rgbColor greenComponent];
		colorBlue = (float)[rgbColor blueComponent];
		
		for (int j = 0; j < 4; ++j) {
			*(colors++) = colorRed;		*(colors++) = colorGreen;		*(colors++) = colorBlue;	*(colors++) = 1.0;
		}
		
		displayListIndex++;
		
		// If we've filled our buffers, get ready to draw more
		if (displayListIndex == kMaxGLRects)
		{
			// Draw our arrays
			glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
			
			// And get ready to draw more
			vertices = glArrayVertices;
			colors = glArrayColors;
			displayListIndex = 0;
		}
		
		genome_index++;
	}
	
	// Draw any leftovers
	if (displayListIndex)
		glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

- (void)tallyBincounts:(int64_t *)bincounts fromGenomeList:(std::vector<MutationIndex> &)genomeList
{
	EIDOS_BZERO(bincounts, 1024 * sizeof(int64_t));
	
	for (MutationIndex mut_index : genomeList)
		bincounts[mutationInfo[mut_index].position_ % 1024]++;
}

- (int64_t)distanceForBincounts:(int64_t *)bincounts1 fromBincounts:(int64_t *)bincounts2
{
	int64_t distance = 0;
	
	for (int i = 0; i < 1024; ++i)
		distance += abs(bincounts1[i] - bincounts2[i]);
	
	return distance;
}

- (void)drawDisplayListInRect:(NSRect)interior displayBlackAndWhite:(BOOL)displayBW previousFirstBincounts:(int64_t **)previousFirstBincounts
{
	int displayListIndex;
	float *vertices = NULL, *colors = NULL;
	
	// Set up to draw rects
	displayListIndex = 0;
	
	vertices = glArrayVertices;
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, glArrayVertices);
	
	colors = glArrayColors;
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_FLOAT, 0, glArrayColors);
	
	// decide whether to plot in ascending order or descending order; we do this based on a calculated
	// similarity to the previously displayed first genome, so that we maximize visual continuity
	int genome_count = (int)displayList->size();
	bool ascending = true;
	
	if (previousFirstBincounts && (genome_count > 1))
	{
		std::vector<MutationIndex> &first_genome_list = (*displayList)[0];
		std::vector<MutationIndex> &last_genome_list = (*displayList)[genome_count - 1];
		static int64_t *first_genome_bincounts = nullptr;
		static int64_t *last_genome_bincounts = nullptr;
		
		if (!first_genome_bincounts)	first_genome_bincounts = (int64_t *)malloc(1024 * sizeof(int64_t));
		if (!last_genome_bincounts)		last_genome_bincounts = (int64_t *)malloc(1024 * sizeof(int64_t));
		
		[self tallyBincounts:first_genome_bincounts fromGenomeList:first_genome_list];
		[self tallyBincounts:last_genome_bincounts fromGenomeList:last_genome_list];
		
		if (*previousFirstBincounts)
		{
			int64_t first_genome_distance = [self distanceForBincounts:first_genome_bincounts fromBincounts:*previousFirstBincounts];
			int64_t last_genome_distance = [self distanceForBincounts:last_genome_bincounts fromBincounts:*previousFirstBincounts];
			
			if (first_genome_distance > last_genome_distance)
				ascending = false;
			
			free(*previousFirstBincounts);
		}
		
		// take over one of our buffers, to avoid having to copy values
		if (ascending) {
			*previousFirstBincounts = first_genome_bincounts;
			first_genome_bincounts = nullptr;
		} else {
			*previousFirstBincounts = last_genome_bincounts;
			last_genome_bincounts = nullptr;
		}
	}
	
	// Loop through the genomes and draw them; we do this in two passes, neutral mutations underneath selected mutations
	for (int pass_count = 0; pass_count <= 1; ++pass_count)
	{
		BOOL plotting_neutral = (pass_count == 0);
		double height_divisor = genome_count;
		double width_subtractor = (usingSubrange ? subrangeFirstBase : 0);
		double width_divisor = (usingSubrange ? (subrangeLastBase - subrangeFirstBase + 1) : (mutationLastPosition + 1));
		
		for (int genome_index = 0; genome_index < genome_count; ++genome_index)
		{
			std::vector<MutationIndex> &genome_list = (ascending ? (*displayList)[genome_index] : (*displayList)[(genome_count - 1) - genome_index]);
			float top = (float)(interior.origin.y + (genome_index / height_divisor) * interior.size.height);
			float bottom = (float)(interior.origin.y + ((genome_index + 1) / height_divisor) * interior.size.height);
			
			if (bottom - top > 1.0)
			{
				// If the range spans a width of more than one pixel, then use the maximal pixel range
				top = floorf(top);
				bottom = ceilf(bottom);
			}
			else
			{
				// If the range spans a pixel or less, make sure that we end up with a range that is one pixel wide, even if the positions span a pixel boundary
				top = floorf(top);
				bottom = top + 1;
			}
			
			for (MutationIndex mut_index : genome_list)
			{
				SLiMHaploMutation &mut_info = mutationInfo[mut_index];
				
				if (mut_info.neutral_ == plotting_neutral)
				{
					slim_position_t mut_position = mut_info.position_;
					float left = (float)(interior.origin.x + ((mut_position - width_subtractor) / width_divisor) * interior.size.width);
					float right = (float)(interior.origin.x + ((mut_position - width_subtractor + 1) / width_divisor) * interior.size.width);
					
					if (right - left > 1.0)
					{
						// If the range spans a width of more than one pixel, then use the maximal pixel range
						left = floorf(left);
						right = ceilf(right);
					}
					else
					{
						// If the range spans a pixel or less, make sure that we end up with a range that is one pixel wide, even if the positions span a pixel boundary
						left = floorf(left);
						right = left + 1;
					}
					
					*(vertices++) = left;		*(vertices++) = top;
					*(vertices++) = left;		*(vertices++) = bottom;
					*(vertices++) = right;		*(vertices++) = bottom;
					*(vertices++) = right;		*(vertices++) = top;
					
					float colorRed, colorGreen, colorBlue;
					
					if (displayBW) {
						colorRed = 0;					colorGreen = 0;						colorBlue = 0;
					} else {
						colorRed = mut_info.red_;		colorGreen = mut_info.green_;		colorBlue = mut_info.blue_;
					}
					
					for (int j = 0; j < 4; ++j) {
						*(colors++) = colorRed;		*(colors++) = colorGreen;		*(colors++) = colorBlue;	*(colors++) = 1.0;
					}
					
					displayListIndex++;
					
					// If we've filled our buffers, get ready to draw more
					if (displayListIndex == kMaxGLRects)
					{
						// Draw our arrays
						glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
						
						// And get ready to draw more
						vertices = glArrayVertices;
						colors = glArrayColors;
						displayListIndex = 0;
					}
				}
			}
		}
	}
	
	// Draw any leftovers
	if (displayListIndex)
		glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

- (void)glDrawHaplotypesInRect:(NSRect)interior displayBlackAndWhite:(BOOL)displayBW showSubpopStrips:(BOOL)showSubpopStrips eraseBackground:(BOOL)eraseBackground previousFirstBincounts:(int64_t **)previousFirstBincounts
{
	// Erase the background to either black or white, depending on displayBW
	if (eraseBackground)
	{
		if (displayBW)
			glColor3f(1.0f, 1.0f, 1.0f);
		else
			glColor3f(0.0f, 0.0f, 0.0f);
		glRecti((int)interior.origin.x, (int)interior.origin.y, (int)(interior.origin.x + interior.size.width), (int)(interior.origin.y + interior.size.height));
	}
	
	// Make sure our GL data buffers are allocated; these are shared among all instances and drawing routines
	[self allocateGLBuffers];
	
	// Draw subpopulation strips if requested
	if (showSubpopStrips)
	{
		const int stripWidth = 15;
		
		NSRect subpopStripRect = interior;
		
		subpopStripRect.size.width = stripWidth;
		
		[self drawSubpopStripsInRect:subpopStripRect];
		
		interior.origin.x += stripWidth;
		interior.size.width -= stripWidth;
	}
	
	// Draw the haplotypes in the remaining portion of the interior
	[self drawDisplayListInRect:interior displayBlackAndWhite:displayBW previousFirstBincounts:previousFirstBincounts];
}

// NSOpenGLView doesn't cache properly, perhaps unsurprisingly; so we need to redraw the contents using non-GL calls.
// This is parallel to glDrawHaplotypesInRect:displayBlackAndWhite:showSubpopStrips: but generates an NSBitmapImageRep.
// Yes, this is a bit brute force.  I couldn't get NSImage to lock focus and let me draw, so I did this.  :->
- (NSBitmapImageRep *)bitmapImageRepForPlotInRect:(NSRect)interior displayBlackAndWhite:(BOOL)displayBW showSubpopStrips:(BOOL)showSubpopStrips;
{
	const int stripWidth = 15;
	int genome_count = (int)displayList->size();
	slim_position_t width = (int)interior.size.width;
	int height = (int)interior.size.height;
	
	// We don't draw lines thicker than one pixel, so we reduce our dimensions if they are too large
	// This is the one respect in which we don't try to exactly mirror the appearance of the GL view
	slim_position_t ideal_pixel_width = (usingSubrange ? (subrangeLastBase - subrangeFirstBase + 1) : (mutationLastPosition + 1));
	
	if (showSubpopStrips)
		ideal_pixel_width += stripWidth;
	
	if (width > ideal_pixel_width)
		width = ideal_pixel_width;
	if (height > genome_count)
		height = genome_count;
	
	// Make a new bitmap rep.  This is laid out as 0xAABBGGRR, if addressed using a uint32_t pointer.
	NSBitmapImageRep *imageRep = [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
																		 pixelsWide:width
																		 pixelsHigh:height
																	  bitsPerSample:8
																	samplesPerPixel:3
																		   hasAlpha:NO
																		   isPlanar:NO
																	 colorSpaceName:@"NSDeviceRGBColorSpace"
																		bytesPerRow:0
																	   bitsPerPixel:32] autorelease];
	
	unsigned char *plotBase = [imageRep bitmapData];
	NSInteger plotRowbytes = [imageRep bytesPerRow];
	
	// Clear to the background color
	uint32_t bgColor = (displayBW ? 0xFFFFFFFF : 0xFF000000);
	
	for (int y = 0; y < height; ++y)
	{
		uint32_t *rowPtr = (uint32_t *)(plotBase + y * plotRowbytes);
		
		for (int x = 0; x < width; ++x)
			rowPtr[x] = bgColor;
	}
	
	// Draw subpop strips if requested
	if (showSubpopStrips)
	{
		NSColorSpace *rgbColorSpace = [NSColorSpace deviceRGBColorSpace];
		int genome_index = 0;
		
		for (slim_objectid_t genome_subpop_id : genomeSubpopIDs)
		{
			int pixel_y = (int)((genome_index / (double)genome_count) * height);
			uint32_t *rowPtr = (uint32_t *)(plotBase + (height - 1 - pixel_y) * plotRowbytes);
			
			int colorRed, colorGreen, colorBlue;
			float hue = (genome_subpop_id - minSubpopID) / (float)(maxSubpopID - minSubpopID + 1);
			NSColor *hsbColor = [NSColor colorWithCalibratedHue:hue saturation:1.0 brightness:1.0 alpha:1.0];
			NSColor *rgbColor = [hsbColor colorUsingColorSpace:rgbColorSpace];
			
			colorRed = std::min((int)([rgbColor redComponent] * 256), 255);
			colorGreen = std::min((int)([rgbColor greenComponent] * 256), 255);
			colorBlue = std::min((int)([rgbColor blueComponent] * 256), 255);
			
			int32_t pixel_value = 0xFF000000 | (colorBlue << 16) | (colorGreen << 8) | colorRed;
			
			for (int pixel_count = 0; pixel_count < stripWidth; ++pixel_count)
				*(rowPtr + pixel_count) = pixel_value;
			
			genome_index++;
		}
		
		plotBase += (stripWidth * 4);
		width -= stripWidth;
	}
	
	// Draw the haplotypes in the remaining portion of the interior
	for (int pass_count = 0; pass_count <= 1; ++pass_count)
	{
		BOOL plotting_neutral = (pass_count == 0);
		int genome_index = 0;
		double height_divisor = genome_count;
		double width_subtractor = (usingSubrange ? subrangeFirstBase : 0);
		double width_divisor = (usingSubrange ? (subrangeLastBase - subrangeFirstBase + 1) : (mutationLastPosition + 1));
		
		for (std::vector<MutationIndex> &genome_list : *displayList)
		{
			float top = (float)((genome_index / height_divisor) * height);
			int pixel_y = (int)floorf(top);
			uint32_t *rowPtr = (uint32_t *)(plotBase + (height - 1 - pixel_y) * plotRowbytes);
			
			for (MutationIndex mut_index : genome_list)
			{
				SLiMHaploMutation &mut_info = mutationInfo[mut_index];
				
				if (mut_info.neutral_ == plotting_neutral)
				{
					slim_position_t mut_position = mut_info.position_;
					int pixel_x = (int)floorf((float)(((mut_position - width_subtractor) / width_divisor) * width));
					int32_t pixel_value;
					
					if (displayBW)
						pixel_value = 0xFF000000;
					else {
						int colorRed = std::min((int)(mut_info.red_ * 256), 255);
						int colorGreen = std::min((int)(mut_info.green_ * 256), 255);
						int colorBlue = std::min((int)(mut_info.blue_ * 256), 255);
						
						pixel_value = 0xFF000000 | (colorBlue << 16) | (colorGreen << 8) | colorRed;
					}
					
					*(rowPtr + pixel_x) = pixel_value;
				}
			}
			
			genome_index++;
		}
	}
	
	return imageRep;
}


// Traveling Salesman Problem code
//
// We have a set of genomes, each of which may be defined as being a particular distance from each other genome (defined here
// as the number of differences in the mutations contained).  We want to sort the genomes into an order that groups similar
// genomes together, minimizing the overall distance through "genome space" traveled from top to bottom of our display.  This
// is exactly the Traveling Salesman Problem, without returning to the starting "city".  This is a very intensively studied
// problem, is NP-hard, and would take an enormously long time to solve exactly for even a relatively small number of genomes,
// whereas we will routinely have thousands of genomes.  We will find an approximate solution using a fast heuristic algorithm,
// because we are not greatly concerned with the quality of the solution and we are extremely concerned with runtime.  The
// nearest-neighbor method is the fastest heuristic, and is O(N^2) in the number of cities; the Greedy algorithm is slower but
// produces significantly better results.  We can refine our initial solution using the 2-opt method.
#pragma mark Traveling salesman problem

// This allocates and builds an array of distances between genomes.  The returned array is owned by the caller.  This is where
// we spend the large majority of our time, at present; this algorithm is O(N^2), but has a large constant (because really also
// it depends on the length of the chromosome, the configuration of mutation runs, etc.).  This method runs prior to the actual
// Traveling Salesman Problem; here we're just figuring out the distances between our "cities".  We have four versions of this
// method, for speed; this is the base version, and separate versions are below that handle a chromosome subrange and/or a
// subset of all of the mutation types.
- (int64_t *)buildDistanceArrayWithBackgroundController:(SLiMWindowController *)backgroundController
{
	int64_t genome_count = (int64_t)genomes.size();
	int64_t *distances = (int64_t *)malloc(genome_count * genome_count * sizeof(int64_t));
	uint8_t *mutation_seen = (uint8_t *)calloc(mutationIndexCount, sizeof(uint8_t));
	uint8_t seen_marker = 1;
	
	for (int i = 0; i < genome_count; ++i)
	{
		Genome *genome1 = genomes[i];
		int64_t *distance_column = distances + i;
		int64_t *distance_row = distances + i * genome_count;
		int mutrun_count = genome1->mutrun_count_;
		MutationRun_SP *genome1_mutruns = genome1->mutruns_;
		
		distance_row[i] = 0;
		
		for (int j = i + 1; j < genome_count; ++j)
		{
			Genome *genome2 = genomes[j];
			MutationRun_SP *genome2_mutruns = genome2->mutruns_;
			int64_t distance = 0;
			
			for (int mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				MutationRun *genome1_mutrun = genome1_mutruns[mutrun_index].get();
				MutationRun *genome2_mutrun = genome2_mutruns[mutrun_index].get();
				int genome1_mutcount = genome1_mutrun->size();
				int genome2_mutcount = genome2_mutrun->size();
				
				if (genome1_mutrun == genome2_mutrun)
					;										// identical runs have no differences
				else if (genome1_mutcount == 0)
					distance += genome2_mutcount;
				else if (genome2_mutcount == 0)
					distance += genome1_mutcount;
				else
				{
					// We use a radix strategy to count the number of mismatches; assume up front that all mutations are mismatched,
					// and then subtract two for each mutation that turns out to be shared, using a uint8_t buffer to track usage.
					distance += genome1_mutcount + genome2_mutcount;
					
					const MutationIndex *mutrun1_end = genome1_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun1_ptr = genome1_mutrun->begin_pointer_const(); mutrun1_ptr != mutrun1_end; ++mutrun1_ptr)
						mutation_seen[*mutrun1_ptr] = seen_marker;
					
					const MutationIndex *mutrun2_end = genome2_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun2_ptr = genome2_mutrun->begin_pointer_const(); mutrun2_ptr != mutrun2_end; ++mutrun2_ptr)
						if (mutation_seen[*mutrun2_ptr] == seen_marker)
							distance -= 2;
					
					// To avoid having to clear the usage buffer every time, we play an additional trick: we use an incrementing
					// marker value to indicate usage, and clear the buffer only when it reaches 255.  Makes about a 10% difference!
					seen_marker++;
					
					if (seen_marker == 0)
					{
						EIDOS_BZERO(mutation_seen, mutationIndexCount);
						
						seen_marker = 1;
					}
				}
			}
			
			// set the distance at both mirrored locations in the distance buffer
			*(distance_column + j * genome_count) = distance;
			*(distance_row + j) = distance;
		}
		
		if ([backgroundController haplotypeProgressIsCancelled])
			break;
		
		[backgroundController setHaplotypeProgress:(i + 1) forStage:0];
	}
	
	free(mutation_seen);
	
	return distances;
}

// This does the same thing as buildDistanceArrayForGenomes:, but uses the chosen subrange of each genome
- (int64_t *)buildDistanceArrayForSubrangeWithBackgroundController:(SLiMWindowController *)backgroundController
{
	slim_position_t firstBase = subrangeFirstBase, lastBase = subrangeLastBase;
	
	int64_t genome_count = (int64_t)genomes.size();
	int64_t *distances = (int64_t *)malloc(genome_count * genome_count * sizeof(int64_t));
	uint8_t *mutation_seen = (uint8_t *)calloc(mutationIndexCount, sizeof(uint8_t));
	uint8_t seen_marker = 1;
	
	for (int i = 0; i < genome_count; ++i)
	{
		Genome *genome1 = genomes[i];
		int64_t *distance_column = distances + i;
		int64_t *distance_row = distances + i * genome_count;
		slim_position_t mutrun_length = genome1->mutrun_length_;
		int mutrun_count = genome1->mutrun_count_;
		MutationRun_SP *genome1_mutruns = genome1->mutruns_;
		
		distance_row[i] = 0;
		
		for (int j = i + 1; j < genome_count; ++j)
		{
			Genome *genome2 = genomes[j];
			MutationRun_SP *genome2_mutruns = genome2->mutruns_;
			int64_t distance = 0;
			
			for (int mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				// Skip mutation runs outside of the subrange we're focused on
				if ((mutrun_length * mutrun_index > lastBase) || (mutrun_length * mutrun_index + mutrun_length - 1 < firstBase))
					continue;
				
				// OK, this mutrun intersects with our chosen subrange; proceed
				MutationRun *genome1_mutrun = genome1_mutruns[mutrun_index].get();
				MutationRun *genome2_mutrun = genome2_mutruns[mutrun_index].get();
				
				if (genome1_mutrun == genome2_mutrun)
					;										// identical runs have no differences
				else
				{
					// We use a radix strategy to count the number of mismatches.  Note this is done a bit differently than in
					// buildDistanceArrayForGenomes:; here we do not add the total and then subtract matches.
					const MutationIndex *mutrun1_end = genome1_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun1_ptr = genome1_mutrun->begin_pointer_const(); mutrun1_ptr != mutrun1_end; ++mutrun1_ptr)
					{
						MutationIndex mut1_index = *mutrun1_ptr;
						slim_position_t mut1_position = mutationPositions[mut1_index];
						
						if ((mut1_position >= firstBase) && (mut1_position <= lastBase))
						{
							mutation_seen[mut1_index] = seen_marker;
							distance++;		// assume unmatched
						}
					}
					
					const MutationIndex *mutrun2_end = genome2_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun2_ptr = genome2_mutrun->begin_pointer_const(); mutrun2_ptr != mutrun2_end; ++mutrun2_ptr)
					{
						MutationIndex mut2_index = *mutrun2_ptr;
						slim_position_t mut2_position = mutationPositions[mut2_index];
						
						if ((mut2_position >= firstBase) && (mut2_position <= lastBase))
						{
							if (mutation_seen[mut2_index] == seen_marker)
								distance -= 1;	// matched, so decrement to compensate for the assumption of non-match above
							else
								distance++;		// not matched, so increment
						}
					}
					
					// To avoid having to clear the usage buffer every time, we play an additional trick: we use an incrementing
					// marker value to indicate usage, and clear the buffer only when it reaches 255.  Makes about a 10% difference!
					seen_marker++;
					
					if (seen_marker == 0)
					{
						EIDOS_BZERO(mutation_seen, mutationIndexCount);
						
						seen_marker = 1;
					}
				}
			}
			
			// set the distance at both mirrored locations in the distance buffer
			*(distance_column + j * genome_count) = distance;
			*(distance_row + j) = distance;
		}
		
		if ([backgroundController haplotypeProgressIsCancelled])
			break;
		
		[backgroundController setHaplotypeProgress:(i + 1) forStage:0];
	}
	
	free(mutation_seen);
	
	return distances;
}

// This does the same thing as buildDistanceArrayForGenomes:, but uses only mutations of a mutation type that is chosen for display
- (int64_t *)buildDistanceArrayForSubtypesWithBackgroundController:(SLiMWindowController *)backgroundController
{
	int64_t genome_count = (int64_t)genomes.size();
	int64_t *distances = (int64_t *)malloc(genome_count * genome_count * sizeof(int64_t));
	uint8_t *mutation_seen = (uint8_t *)calloc(mutationIndexCount, sizeof(uint8_t));
	uint8_t seen_marker = 1;
	
	for (int i = 0; i < genome_count; ++i)
	{
		Genome *genome1 = genomes[i];
		int64_t *distance_column = distances + i;
		int64_t *distance_row = distances + i * genome_count;
		int mutrun_count = genome1->mutrun_count_;
		MutationRun_SP *genome1_mutruns = genome1->mutruns_;
		
		distance_row[i] = 0;
		
		for (int j = i + 1; j < genome_count; ++j)
		{
			Genome *genome2 = genomes[j];
			MutationRun_SP *genome2_mutruns = genome2->mutruns_;
			int64_t distance = 0;
			
			for (int mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				MutationRun *genome1_mutrun = genome1_mutruns[mutrun_index].get();
				MutationRun *genome2_mutrun = genome2_mutruns[mutrun_index].get();
				
				if (genome1_mutrun == genome2_mutrun)
					;										// identical runs have no differences
				else
				{
					// We use a radix strategy to count the number of mismatches.  Note this is done a bit differently than in
					// buildDistanceArrayForGenomes:; here we do not add the total and then subtract matches.
					const MutationIndex *mutrun1_end = genome1_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun1_ptr = genome1_mutrun->begin_pointer_const(); mutrun1_ptr != mutrun1_end; ++mutrun1_ptr)
					{
						MutationIndex mut1_index = *mutrun1_ptr;
						
						if (mutationInfo[mut1_index].display_)
						{
							mutation_seen[mut1_index] = seen_marker;
							distance++;		// assume unmatched
						}
					}
					
					const MutationIndex *mutrun2_end = genome2_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun2_ptr = genome2_mutrun->begin_pointer_const(); mutrun2_ptr != mutrun2_end; ++mutrun2_ptr)
					{
						MutationIndex mut2_index = *mutrun2_ptr;
						
						if (mutationInfo[mut2_index].display_)
						{
							if (mutation_seen[mut2_index] == seen_marker)
								distance -= 1;	// matched, so decrement to compensate for the assumption of non-match above
							else
								distance++;		// not matched, so increment
						}
					}
					
					// To avoid having to clear the usage buffer every time, we play an additional trick: we use an incrementing
					// marker value to indicate usage, and clear the buffer only when it reaches 255.  Makes about a 10% difference!
					seen_marker++;
					
					if (seen_marker == 0)
					{
						EIDOS_BZERO(mutation_seen, mutationIndexCount);
						
						seen_marker = 1;
					}
				}
			}
			
			// set the distance at both mirrored locations in the distance buffer
			*(distance_column + j * genome_count) = distance;
			*(distance_row + j) = distance;
		}
		
		if ([backgroundController haplotypeProgressIsCancelled])
			break;
		
		[backgroundController setHaplotypeProgress:(i + 1) forStage:0];
	}
	
	free(mutation_seen);
	
	return distances;
}

// This does the same thing as buildDistanceArrayForGenomes:, but uses the chosen subrange of each genome, and only mutations of mutation types being displayed
- (int64_t *)buildDistanceArrayForSubrangeAndSubtypesWithBackgroundController:(SLiMWindowController *)backgroundController
{
	slim_position_t firstBase = subrangeFirstBase, lastBase = subrangeLastBase;
	
	int64_t genome_count = (int64_t)genomes.size();
	int64_t *distances = (int64_t *)malloc(genome_count * genome_count * sizeof(int64_t));
	uint8_t *mutation_seen = (uint8_t *)calloc(mutationIndexCount, sizeof(uint8_t));
	uint8_t seen_marker = 1;
	
	for (int i = 0; i < genome_count; ++i)
	{
		Genome *genome1 = genomes[i];
		int64_t *distance_column = distances + i;
		int64_t *distance_row = distances + i * genome_count;
		slim_position_t mutrun_length = genome1->mutrun_length_;
		int mutrun_count = genome1->mutrun_count_;
		MutationRun_SP *genome1_mutruns = genome1->mutruns_;
		
		distance_row[i] = 0;
		
		for (int j = i + 1; j < genome_count; ++j)
		{
			Genome *genome2 = genomes[j];
			MutationRun_SP *genome2_mutruns = genome2->mutruns_;
			int64_t distance = 0;
			
			for (int mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				// Skip mutation runs outside of the subrange we're focused on
				if ((mutrun_length * mutrun_index > lastBase) || (mutrun_length * mutrun_index + mutrun_length - 1 < firstBase))
					continue;
				
				// OK, this mutrun intersects with our chosen subrange; proceed
				MutationRun *genome1_mutrun = genome1_mutruns[mutrun_index].get();
				MutationRun *genome2_mutrun = genome2_mutruns[mutrun_index].get();
				
				if (genome1_mutrun == genome2_mutrun)
					;										// identical runs have no differences
				else
				{
					// We use a radix strategy to count the number of mismatches.  Note this is done a bit differently than in
					// buildDistanceArrayForGenomes:; here we do not add the total and then subtract matches.
					const MutationIndex *mutrun1_end = genome1_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun1_ptr = genome1_mutrun->begin_pointer_const(); mutrun1_ptr != mutrun1_end; ++mutrun1_ptr)
					{
						MutationIndex mut1_index = *mutrun1_ptr;
						slim_position_t mut1_position = mutationPositions[mut1_index];
						
						if ((mut1_position >= firstBase) && (mut1_position <= lastBase))
						{
							if (mutationInfo[mut1_index].display_)
							{
								mutation_seen[mut1_index] = seen_marker;
								distance++;		// assume unmatched
							}
						}
					}
					
					const MutationIndex *mutrun2_end = genome2_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun2_ptr = genome2_mutrun->begin_pointer_const(); mutrun2_ptr != mutrun2_end; ++mutrun2_ptr)
					{
						MutationIndex mut2_index = *mutrun2_ptr;
						slim_position_t mut2_position = mutationPositions[mut2_index];
						
						if ((mut2_position >= firstBase) && (mut2_position <= lastBase))
						{
							if (mutationInfo[mut2_index].display_)
							{
								if (mutation_seen[mut2_index] == seen_marker)
									distance -= 1;	// matched, so decrement to compensate for the assumption of non-match above
								else
									distance++;		// not matched, so increment
							}
						}
					}
					
					// To avoid having to clear the usage buffer every time, we play an additional trick: we use an incrementing
					// marker value to indicate usage, and clear the buffer only when it reaches 255.  Makes about a 10% difference!
					seen_marker++;
					
					if (seen_marker == 0)
					{
						EIDOS_BZERO(mutation_seen, mutationIndexCount);
						
						seen_marker = 1;
					}
				}
			}
			
			// set the distance at both mirrored locations in the distance buffer
			*(distance_column + j * genome_count) = distance;
			*(distance_row + j) = distance;
		}
		
		if ([backgroundController haplotypeProgressIsCancelled])
			break;
		
		[backgroundController setHaplotypeProgress:(i + 1) forStage:0];
	}
	
	free(mutation_seen);
	
	return distances;
}

// Since we want to solve the Traveling Salesman Problem without returning to the original city, the choice of the initial city
// may be quite important to the solution we get.  It seems reasonable to start at the city that is the most isolated, i.e. has
// the largest distance from itself to any other city.  By starting with this city, we avoid having to have two edges connecting
// to it, both of which would be relatively long.  However, this is just a guess, and might be modified by refinement later.
- (int)indexOfMostIsolatedGenomeWithDistances:(int64_t *)distances size:(int64_t)genome_count
{
	int64_t greatest_isolation = -1;
	int greatest_isolation_index = -1;
	
	for (int i = 0; i < genome_count; ++i)
	{
		int64_t isolation = INT64_MAX;
		int64_t *row_ptr = distances + i * genome_count;
		
		for (int j = 0; j < genome_count; ++j)
		{
			int64_t distance = row_ptr[j];
			
			// distances of 0 don't count for isolation estimation; we really want the most isolated identical cluster of genomes
			// this also serves to take care of the j == i case for us without special-casing, which is nice...
			if (distance == 0)
				continue;
			
			if (distance < isolation)
				isolation = distance;
		}
		
		if (isolation > greatest_isolation)
		{
			greatest_isolation = isolation;
			greatest_isolation_index = i;
		}
	}
	
	return greatest_isolation_index;
}

// The nearest-neighbor method provides an initial solution for the Traveling Salesman Problem by beginning with a chosen city
// (see indexOfMostIsolatedGenomeWithDistances:size: above) and adding successive cities according to which is closest to the
// city we have reached thus far.  This is quite simple to implement, and runs in O(N^2) time.  However, the greedy algorithm
// below runs only a little more slowly, and produces significantly better results, so unless speed is essential it is better.
- (void)nearestNeighborSolveWithDistances:(int64_t *)distances size:(int64_t)genome_count solution:(std::vector<int> &)solution backgroundController:(SLiMWindowController *)backgroundController
{
	int64_t genomes_left = genome_count;
	
	solution.reserve(genome_count);
	
	// we have to make a copy of the distances matrix, as we modify it internally
	int64_t *distances_copy = (int64_t *)malloc(genome_count * genome_count * sizeof(int64_t));
	
	memcpy(distances_copy, distances, genome_count * genome_count * sizeof(int64_t));
	
	// find the genome that is farthest from any other genome; this will be our starting point, for now
	int last_path_index = [self indexOfMostIsolatedGenomeWithDistances:distances_copy size:genome_count];
	
	do
	{
		// add the chosen genome to our path
		solution.push_back(last_path_index);
		
		if ([backgroundController haplotypeProgressIsCancelled])
			break;
		
		[backgroundController setHaplotypeProgress:(int)(genome_count - genomes_left + 1) forStage:1];
		
		// if we just added the last genome, we're done
		if (--genomes_left == 0)
			break;
		
		// otherwise, mark the chosen genome as unavailable by setting distances to it to INT64_MAX
		int64_t *column_ptr = distances_copy + last_path_index;
		
		for (int i = 0; i < genome_count; ++i)
		{
			*column_ptr = INT64_MAX;
			column_ptr += genome_count;
		}
		
		// now we need to find the next city, which will be the nearest neighbor of the last city
		int64_t *row_ptr = distances_copy + last_path_index * genome_count;
		int64_t nearest_neighbor_distance = INT64_MAX;
		int nearest_neighbor_index = -1;
		
		for (int i = 0; i < genome_count; ++i)
		{
			int64_t distance = row_ptr[i];
			
			if (distance < nearest_neighbor_distance)
			{
				nearest_neighbor_distance = distance;
				nearest_neighbor_index = i;
			}
		}
		
		// found the next city; add it to the path by looping back to the top
		last_path_index = nearest_neighbor_index;
	}
	while (true);
	
	free(distances_copy);
}

// The greedy method provides an initial solution for the Traveling Salesman Problem by sorting all possible edges,
// and then iteratively adding the shortest legal edge to the path until the full path has been constructed.  This
// is a little more complex than nearest neighbor, and runs a bit more slowly, but gives a somewhat better result.
typedef struct {
	int i, k;
	int64_t d;
} greedy_edge;

bool comp_greedy_edge(greedy_edge &i, greedy_edge &j);
bool comp_greedy_edge(greedy_edge &i, greedy_edge &j) { return (i.d < j.d); }

// progress indicator support for the std::sort in the greedy algorithm; see comment below
static int64_t slim_greedy_progress_max;
static volatile int64_t slim_greedy_progress;
static int slim_greedy_progress_scale;

bool comp_greedy_edge_count(greedy_edge &i, greedy_edge &j);
bool comp_greedy_edge_count(greedy_edge &i, greedy_edge &j) { slim_greedy_progress++; return (i.d < j.d); }

- (void)greedyPeriodicCounterUpdateWithBackgroundController:(SLiMWindowController *)backgroundController
{
	// This runs in its own separate background thread, and just messages the main thread periodically to update
	// the progress bar.  We use the globals above to govern the progress bar reading; they are updated by our
	// custom comparator function comp_greedy_edge_count().  We run until we are told, through the flag
	// haplotypeProgressGreedySortProgressFlag on backgroundController, that we are supposed to finish.
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	
	while (backgroundController->haplotypeProgressGreedySortProgressFlag == 1)
	{
		int progress = (int)((slim_greedy_progress / (double)slim_greedy_progress_max) * slim_greedy_progress_scale);
		
		progress = std::min(progress, slim_greedy_progress_scale);
		
		//NSLog(@"progress: %d", progress);
		[backgroundController setHaplotypeProgress:progress forStage:1];
		
		[NSThread sleepForTimeInterval:0.01];
	}
	
	backgroundController->haplotypeProgressGreedySortProgressFlag = -1;		// signal that we are exiting
	
	
	[pool release];
}

- (void)greedySolveWithDistances:(int64_t *)distances size:(int64_t)genome_count solution:(std::vector<int> &)solution backgroundController:(SLiMWindowController *)backgroundController
{
	// The first thing we need to do is sort all possible edges in ascending order by length;
	// we don't need to differentiate a->b versus b->a since our distances are symmetric
	std::vector<greedy_edge> edge_buf;
	int64_t edge_count = ((int64_t)genome_count * (genome_count - 1)) / 2;
	
	edge_buf.reserve(edge_count);	// one of the two factors is even so /2 is safe
	
	for (int i = 0; i < genome_count - 1; ++i)
	{
		for (int k = i + 1; k < genome_count; ++k)
			edge_buf.emplace_back(greedy_edge{i, k, *(distances + i + k * genome_count)});
	}
	
	if ([backgroundController haplotypeProgressIsCancelled])
		return;
	
	if (backgroundController)
	{
		// We're running in the background, sorting the edges can take a long time, we want to show progress,
		// but std::sort() provides no progress.  What to do?  We could switch to doing a large number of
		// std::partial_sort() calls, incrementing progress in between, but that would greatly increase the
		// total time for the sorting to complete.  We could write our own sort code, or clone std::sort's
		// template code and insert progress code into it, or something gross like that.  Instead, here's an
		// interesting idea: https://stackoverflow.com/a/13898735/2752221 .  The idea is to increment your
		// progress counter in your comparator function!  Then call std::sort() as usual to do the sort, and
		// update the progress bar periodically on a separate thread.  So that's what we do.  Note that the
		// way I've implemented this is not re-entrant, so if the user creates multiple haplotype plots
		// simultaneously, in multiple windows, they will collide in the progress counters and their progress
		// bars will be incorrect.  I can live with it.
		slim_greedy_progress_max = (int64_t)(edge_count * log2(edge_count) / 2);	// n log n estimated comparisons; seems to be half that for some reason, in practice
		slim_greedy_progress = 0;
		slim_greedy_progress_scale = (int)genome_count;					// this is the GUI progress bar's max scale
		
		backgroundController->haplotypeProgressGreedySortProgressFlag = 1;	// indicate we are running the sort
		
		[self performSelectorInBackground:@selector(greedyPeriodicCounterUpdateWithBackgroundController:) withObject:backgroundController];
		
		std::sort(edge_buf.begin(), edge_buf.end(), comp_greedy_edge_count);
		
		backgroundController->haplotypeProgressGreedySortProgressFlag = 0;	// indicate we are done with the sort
		
		// wait for the background thread to die; yes, we ought to use NSConditionLock, but this will be quick...
		while (backgroundController->haplotypeProgressGreedySortProgressFlag == 0)
			;
		
		[backgroundController setHaplotypeProgress:(int)genome_count forStage:1];	// fill out the bar
	}
	else
	{
		// If we're not running in the background, we have no progress indicator so we can just use std::sort()
		std::sort(edge_buf.begin(), edge_buf.end(), comp_greedy_edge);
	}
	
	if ([backgroundController haplotypeProgressIsCancelled])
		return;
	
	// Now we take take the first legal edge from the top of edge_buf and add it to our path. "Legal" means it
	// doesn't increase the degree of either participating node above 2, and doesn't create a cycle.  We check
	// the first condition by keeping a vector of the degrees of all nodes, so that's easy.  We check the second
	// condition by keeping a vector of "group" tags for each participating node; an edge that joins two nodes
	// in the same group creates a cycle and is thus illegal (maybe there's a better way to detect cycles but I
	// haven't thought of it yet :->).
	std::vector<greedy_edge> path_components;
	uint8_t *node_degrees = (uint8_t *)calloc(sizeof(uint8_t), genome_count);
	int *node_groups = (int *)calloc(sizeof(int), genome_count);
	int next_node_group = 1;
	
	path_components.reserve(genome_count);
	
	for (int64_t edge_index = 0; edge_index < edge_count; ++edge_index)
	{
		greedy_edge &candidate_edge = edge_buf[edge_index];
		
		// Get the participating nodes and check that they still have a free end
		int i = candidate_edge.i;
		
		if (node_degrees[i] == 2)
			continue;
		
		int k = candidate_edge.k;
		
		if (node_degrees[k] == 2)
			continue;
		
		// Check whether they are in the same group (and not 0), in which case this edge would create a cycle
		int group_i = node_groups[i];
		int group_k = node_groups[k];
		
		if ((group_i != 0) && (group_i == group_k))
			continue;
		
		// OK, the edge is legal.  Add it to our path, and maintain the group tags
		path_components.push_back(candidate_edge);
		node_degrees[i]++;
		node_degrees[k]++;
		
		if ((group_i == 0) && (group_k == 0))
		{
			// making a new group
			node_groups[i] = next_node_group;
			node_groups[k] = next_node_group;
			next_node_group++;
		}
		else if (group_i == 0)
		{
			// adding node i to an existing group
			node_groups[i] = group_k;
		}
		else if (group_k == 0)
		{
			// adding node k to an existing group
			node_groups[k] = group_i;
		}
		else
		{
			// joining two groups; one gets assimilated
			// the assimilation could probably be done more efficiently but this overhead won't matter
			for (int node_index = 0; node_index < genome_count; ++node_index)
				if (node_groups[node_index] == group_k)
					node_groups[node_index] = group_i;
		}
		
		if ((int)path_components.size() == genome_count - 1)		// no return edge
			break;
		
		if ([backgroundController haplotypeProgressIsCancelled])
			goto cancelExit;
	}
	
	// Check our work
	{
		int degree1_count = 0, degree2_count = 0, universal_group = node_groups[0];
		
		for (int node_index = 0; node_index < genome_count; ++node_index)
		{
			if (node_degrees[node_index] == 1) ++degree1_count;
			else if (node_degrees[node_index] == 2) ++degree2_count;
			else NSLog(@"node of degree other than 1 or 2 seen (degree %d)", (int)node_degrees[node_index]);
			
			if (node_groups[node_index] != universal_group)
				NSLog(@"node of non-matching group seen (group %d)", (int)node_groups[node_index]);
		}
	}
	
	if ([backgroundController haplotypeProgressIsCancelled])
		goto cancelExit;
	
	// Finally, we have a jumble of edges that are in no order, and we need to make a coherent path from them.
	// We start at the first degree-1 node we find, which is one of the two ends; doesn't matter which.
	{
		int remaining_edge_count = (int)genome_count - 1;
		int last_index;
		
		for (last_index = 0; last_index < genome_count; ++last_index)
			if (node_degrees[last_index] == 1)
				break;
		
		solution.push_back(last_index);
		
		do
		{
			// look for an edge involving last_index that we haven't used yet (there should be only one)
			int next_edge_index;
			int next_index = INT_MAX;	// get rid of the unitialized var warning, and cause a crash if we have a bug
			
			for (next_edge_index = 0; next_edge_index < remaining_edge_count; ++next_edge_index)
			{
				greedy_edge &candidate_edge = path_components[next_edge_index];
				
				if (candidate_edge.i == last_index)
				{
					next_index = candidate_edge.k;
					break;
				}
				else if (candidate_edge.k == last_index)
				{
					next_index = candidate_edge.i;
					break;
				}
			}
			
			if ([backgroundController haplotypeProgressIsCancelled])
				break;
			
			// found it; assimilate it into the path and remove it from path_components
			solution.push_back(next_index);
			last_index = next_index;
			
			path_components[next_edge_index] = path_components[--remaining_edge_count];
		}
		while (remaining_edge_count > 0);
	}
	
cancelExit:
	free(node_degrees);
	free(node_groups);
}

// check that a given path visits every city exactly once
- (BOOL)checkPath:(std::vector<int> &)path size:(int64_t)genome_count
{
	uint8_t *visits = (uint8_t *)calloc(sizeof(uint8_t), genome_count);
	
	if ((int)path.size() != genome_count)
	{
		NSLog(@"checkPath:size: path is wrong length");
		free(visits);
		return NO;
	}
	
	for (int i = 0; i < genome_count; ++i)
	{
		int city_index = path[i];
		
		visits[city_index]++;
	}
	
	for (int i = 0; i < genome_count; ++i)
		if (visits[i] != 1)
		{
			NSLog(@"checkPath:size: city visited wrong count (%d)", (int)visits[i]);
			free(visits);
			return NO;
		}
	
	free(visits);
	return YES;
}

// calculate the length of a given path
- (int64_t)lengthOfPath:(std::vector<int> &)path withDistances:(int64_t *)distances size:(int64_t)genome_count
{
	int64_t length = 0;
	int current_city = path[0];
	
	for (int city_index = 1; city_index < genome_count; city_index++)
	{
		int next_city = path[city_index];
		
		length += *(distances + current_city * genome_count + next_city);
		current_city = next_city;
	}
	
	return length;
}

// Do "2-opt" optimization of a given path, which involves inverting ranges of the path that lead to a better solution.
// This is quite time-consuming and improves the result only marginally, so we do not want it to be the default, but it
// might be useful to provide as an option.  This method always takes the first optimization it sees that moves in a
// positive direction; I tried taking the best optimization available at each step, instead, and it ran half as fast
// and achieved results that were no better on average, so I didn't even keep that code.
- (void)do2optOptimizationOfSolution:(std::vector<int> &)path withDistances:(int64_t *)distances size:(int64_t)genome_count backgroundController:(SLiMWindowController *)backgroundController
{
	// Figure out the length of the current path
	int64_t original_distance = [self lengthOfPath:path withDistances:distances size:genome_count];
	int64_t best_distance = original_distance;
	
	//NSLog(@"2-opt initial length: %lld", best_distance);
	
	// Iterate until we can find no 2-opt improvement; this algorithm courtesy of https://en.wikipedia.org/wiki/2-opt
	int farthest_i = 0;	// for our progress bar
	
startAgain:
	for (int i = 0; i < genome_count - 1; i++)
	{
		for (int k = i + 1; k < genome_count; ++k)
		{
			// First, try the proposed path without actually constructing it; we just need to subtract the lengths of the
			// edges being removed and add the lengths of the edges being added, rather than constructing the whole new
			// path and measuring its length.  If we have a path 1:9 and are inverting i=3 to k=5, it looks like:
			//
			//		1	2	3	4	5	6	7	8	9
			//			   (i		k)
			//
			//		1	2  (5	4	3)	6	7	8	9
			//
			// So the 2-3 edge and the 5-6 edge were subtracted, and the 2-5 edge and the 3-6 edge were added.  Note that
			// we can only get away with juggling the distances this way because our problem is symmetric; the length of
			// 3-4-5 is guaranteed the same as the length of the reversed segment 5-4-3.  If the reversed segment is at
			// one or the other end of the path, we only need to patch up one edge; we don't return to the start city.
			// Note also that i and k are not genome indexes; they are indexes into our current path, which provides us
			// with the relevant genomes indexes.
			int64_t new_distance = best_distance;
			int index_i = path[i];
			int index_k = path[k];
			
			if (i > 0)
			{
				int index_i_minus_1 = path[i - 1];
				
				new_distance -= *(distances + index_i_minus_1 + index_i * genome_count);	// remove edge (i-1)-(i)
				new_distance += *(distances + index_i_minus_1 + index_k * genome_count);	// add edge (i-1)-(k)
			}
			if (k < genome_count - 1)
			{
				int index_k_plus_1 = path[k + 1];
				
				new_distance -= *(distances + index_k + index_k_plus_1 * genome_count);		// remove edge (k)-(k+1)
				new_distance += *(distances + index_i + index_k_plus_1 * genome_count);		// add edge (i)-(k+1)
			}
			
			if (new_distance < best_distance)
			{
				// OK, the new path is an improvement, so let's take it.  We construct it by inverting the sequence
				// from i to k in our path vector, by swapping elements until we reach the center.
				for (int inversion_length = 0; ; inversion_length++)
				{
					int swap1 = i + inversion_length;
					int swap2 = k - inversion_length;
					
					if (swap1 >= swap2)
						break;
					
					std::swap(path[swap1], path[swap2]);
				}
				
				best_distance = new_distance;
				
				//NSLog(@"Improved path length: %lld (inverted from %d to %d)", best_distance, i, k);
				//NSLog(@"   checkback: new path length is %lld", [self lengthOfPath:path withDistances:distances size:genome_count]);
				goto startAgain;
			}
		}
		
		// We update our progress bar according to the furthest we have ever gotten in the outer loop; we keep having to start
		// over again, and there's no way to know how many times we're going to do that, so this seems like the best estimator.
		farthest_i = std::max(farthest_i, i + 1);
		
		[backgroundController setHaplotypeProgress:farthest_i forStage:2];
		
		if ([backgroundController haplotypeProgressIsCancelled])
			break;
	}
	
	//NSLog(@"Distance changed from %lld to %lld (%.3f%% improvement)", original_distance, best_distance, ((original_distance - best_distance) / (double)original_distance) * 100.0);
}

@end



























