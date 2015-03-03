//
//  GraphView_MutationFrequencySpectra.m
//  SLiM
//
//  Created by Ben Haller on 2/27/15.
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


#import "GraphView_MutationFrequencySpectra.h"
#import "SLiMWindowController.h"


@implementation GraphView_MutationFrequencySpectra

- (id)initWithFrame:(NSRect)frameRect withController:(SLiMWindowController *)controller
{
	if (self = [super initWithFrame:frameRect withController:controller])
	{
		[self setHistogramBinCount:10];
		
		[self setXAxisMajorTickInterval:0.2];
		[self setXAxisMinorTickInterval:0.1];
		[self setXAxisMajorTickModulus:2];
		[self setXAxisTickValuePrecision:1];
		
		[self setXAxisLabelString:@"Mutation frequency"];
		[self setYAxisLabelString:@"Proportion of mutations"];
		
		[self setShowHorizontalGridLines:YES];
	}
	
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (uint32 *)mutationFrequencySpectrumWithController:(SLiMWindowController *)controller mutationTypeCount:(int)mutationTypeCount
{
	static uint32 *spectrum = NULL;
	static int spectrumBins = 0;
	int binCount = [self histogramBinCount];
	
	// allocate our bins
	if (!spectrum || (spectrumBins != binCount * mutationTypeCount))
	{
		if (spectrum)
			free(spectrum);
		
		spectrumBins = binCount * mutationTypeCount;
		spectrum = (uint32 *)malloc(spectrumBins * sizeof(uint32));
	}
	
	// clear our bins
	for (int i = 0; i < spectrumBins; ++i)
		spectrum[i] = 0;
	
	// get the selected chromosome range; FIXME should this be fixed at graph creation instead?
	ChromosomeView *chromosome = controller->chromosomeOverview;
	BOOL hasSelection = chromosome->hasSelection;
	int selectionFirstBase = chromosome->selectionFirstBase - 1;	// correct from 1-based to 0-based
	int selectionLastBase = chromosome->selectionLastBase - 1;
	
	// tally into our bins
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	double totalGenomeCount = pop.total_genome_count_;
	Genome &mutationRegistry = pop.mutation_registry_;
	const Mutation **mutations = mutationRegistry.mutations_;
	int mutationCount = mutationRegistry.mutation_count_;
	
	for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
	{
		const Mutation *mutation = mutations[mutIndex];
		
		// if the user has selected a subrange of the chromosome, we will work from that
		if (hasSelection)
		{
			int32_t mutationPosition = mutation->position_;
			
			if ((mutationPosition < selectionFirstBase) || (mutationPosition > selectionLastBase))
				continue;
		}
		
		int32_t mutationRefCount = mutation->reference_count_;
		double mutationFrequency = mutationRefCount / totalGenomeCount;
		int mutationBin = (int)floor(mutationFrequency * binCount);
		int mutationTypeIndex = mutation->mutation_type_ptr_->mutation_type_index_;
		
		if (mutationBin == binCount)
			mutationBin = binCount - 1;
		
		(spectrum[mutationTypeIndex + mutationBin * mutationTypeCount])++;	// bins in sequence for each mutation type within one frequency bin, then again for the next frequency bin, etc.
	}
	
	// return the final tally; note this is a pointer in to our static ivar, and must not be freed!
	return spectrum;
}

- (void)drawGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller
{
	int binCount = [self histogramBinCount];
	int mutationTypeCount = (int)controller->sim->mutation_types_.size();
	int spectrumBins = binCount * mutationTypeCount;
	uint32 *spectrum = [self mutationFrequencySpectrumWithController:controller mutationTypeCount:mutationTypeCount];
	uint32 total = 0;
	
	// total up all the bins so we can calculate proportions
	for (int i = 0; i < spectrumBins; ++i)
		total += spectrum[i];
	
	// plot our histogram bars
	[self drawGroupedBarplotInInteriorRect:interiorRect withController:controller buffer:spectrum bufferLength:spectrumBins subBinCount:mutationTypeCount mainBinCount:binCount firstBinValue:0.0 mainBinWidth:0.10 heightNormalizer:(double)total];
}

- (NSSize)legendSize
{
	return [self mutationTypeLegendSize];	// we use the prefab mutation type legend
}

- (void)drawLegendInRect:(NSRect)legendRect
{
	[self drawMutationTypeLegendInRect:legendRect];		// we use the prefab mutation type legend
}

@end





























































