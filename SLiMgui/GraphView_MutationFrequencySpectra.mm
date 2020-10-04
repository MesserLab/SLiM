//
//  GraphView_MutationFrequencySpectra.m
//  SLiM
//
//  Created by Ben Haller on 2/27/15.
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


#import "GraphView_MutationFrequencySpectra.h"
#import "SLiMWindowController.h"


@implementation GraphView_MutationFrequencySpectra

- (id)initWithFrame:(NSRect)frameRect withController:(SLiMWindowController *)controller
{
	if (self = [super initWithFrame:frameRect withController:controller])
	{
		[self setHistogramBinCount:10];
		[self setAllowXAxisBinRescale:YES];
		
		[self setXAxisMajorTickInterval:0.2];
		[self setXAxisMinorTickInterval:0.1];
		[self setXAxisMajorTickModulus:2];
		[self setXAxisTickValuePrecision:1];
		
		[self setXAxisLabelString:@"Mutation frequency"];
		[self setYAxisLabelString:@"Proportion of mutations"];
		
		[self setAllowXAxisUserRescale:NO];
		[self setAllowYAxisUserRescale:YES];
		
		[self setShowHorizontalGridLines:YES];
	}
	
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (double *)mutationFrequencySpectrumWithController:(SLiMWindowController *)controller mutationTypeCount:(int)mutationTypeCount
{
	static uint32_t *spectrum = NULL;			// used for tallying
	static double *doubleSpectrum = NULL;	// not used for tallying, to avoid precision issues
	static uint32_t spectrumBins = 0;
	int binCount = [self histogramBinCount];
	uint32_t usedSpectrumBins = binCount * mutationTypeCount;
	
	// allocate our bins
	if (!spectrum || (spectrumBins < usedSpectrumBins))
	{
		spectrumBins = usedSpectrumBins;
		spectrum = (uint32_t *)realloc(spectrum, spectrumBins * sizeof(uint32_t));
		doubleSpectrum = (double *)realloc(doubleSpectrum, spectrumBins * sizeof(double));
	}
	
	// clear our bins
	for (unsigned int i = 0; i < usedSpectrumBins; ++i)
		spectrum[i] = 0;
	
	// get the selected chromosome range
	ChromosomeView *chromosome = controller->chromosomeOverview;
	BOOL hasSelection = chromosome->hasSelection;
	slim_position_t selectionFirstBase = chromosome->selectionFirstBase;
	slim_position_t selectionLastBase = chromosome->selectionLastBase;
	
	// tally into our bins
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	
	pop.TallyMutationReferences(nullptr, false);	// update tallies; usually this will just use the cache set up by Population::MaintainRegistry()
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
	double totalGenomeCount = pop.total_genome_count_;
	int registry_size;
	const MutationIndex *registry = pop.MutationRegistry(&registry_size);
	
	for (int registry_index = 0; registry_index < registry_size; ++registry_index)
	{
		const Mutation *mutation = mut_block_ptr + registry[registry_index];
		
		// if the user has selected a subrange of the chromosome, we will work from that
		if (hasSelection)
		{
			slim_position_t mutationPosition = mutation->position_;
			
			if ((mutationPosition < selectionFirstBase) || (mutationPosition > selectionLastBase))
				continue;
		}
		
		slim_refcount_t mutationRefCount = *(refcount_block_ptr + mutation->BlockIndex());
		double mutationFrequency = mutationRefCount / totalGenomeCount;
		int mutationBin = (int)floor(mutationFrequency * binCount);
		int mutationTypeIndex = mutation->mutation_type_ptr_->mutation_type_index_;
		
		if (mutationBin == binCount)
			mutationBin = binCount - 1;
		
		(spectrum[mutationTypeIndex + mutationBin * mutationTypeCount])++;	// bins in sequence for each mutation type within one frequency bin, then again for the next frequency bin, etc.
	}
	
	// normalize within each mutation type
	for (int mutationTypeIndex = 0; mutationTypeIndex < mutationTypeCount; ++mutationTypeIndex)
	{
		uint32_t total = 0;
		
		for (int bin = 0; bin < binCount; ++bin)
		{
			int binIndex = mutationTypeIndex + bin * mutationTypeCount;
			
			total += spectrum[binIndex];
		}
		
		for (int bin = 0; bin < binCount; ++bin)
		{
			int binIndex = mutationTypeIndex + bin * mutationTypeCount;
			
			doubleSpectrum[binIndex] = spectrum[binIndex] / (double)total;
		}
	}
	
	// return the final tally; note this is a pointer in to our static ivar, and must not be freed!
	return doubleSpectrum;
}

- (void)drawGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller
{
	int binCount = [self histogramBinCount];
	int mutationTypeCount = (int)controller->sim->mutation_types_.size();
	double *spectrum = [self mutationFrequencySpectrumWithController:controller mutationTypeCount:mutationTypeCount];
	
	// plot our histogram bars
	[self drawGroupedBarplotInInteriorRect:interiorRect withController:controller buffer:spectrum subBinCount:mutationTypeCount mainBinCount:binCount firstBinValue:0.0 mainBinWidth:(1.0 / binCount)];
	
	// if we have a limited selection range, overdraw a note about that
	ChromosomeView *chromosome = controller->chromosomeOverview;
	
	if (chromosome->hasSelection)
	{
		slim_position_t selectionFirstBase = chromosome->selectionFirstBase;
		slim_position_t selectionLastBase = chromosome->selectionLastBase;
		static NSDictionary *attrs = nil;
		
		if (!attrs)
			attrs = [@{NSFontAttributeName : [NSFont fontWithName:[GraphView labelFontName] size:10], NSForegroundColorAttributeName : [NSColor darkGrayColor]} retain];
		
		NSString *labelText = [NSString stringWithFormat:@"%lld – %lld", (int64_t)selectionFirstBase, (int64_t)selectionLastBase];
		NSAttributedString *attributedLabel = [[NSMutableAttributedString alloc] initWithString:labelText attributes:attrs];
		NSSize labelSize = [attributedLabel size];
		double labelX = interiorRect.origin.x + (interiorRect.size.width - labelSize.width) / 2.0;
		double labelY = interiorRect.origin.y + interiorRect.size.height - (labelSize.height + 4);
		
		[attributedLabel drawAtPoint:NSMakePoint(labelX, labelY)];
		[attributedLabel release];
	}
}

- (NSArray *)legendKey
{
	return [self mutationTypeLegendKey];	// we use the prefab mutation type legend
}

- (void)controllerSelectionChanged
{
	[self setNeedsDisplay:YES];
}

- (NSString *)stringForDataWithController:(SLiMWindowController *)controller
{
	NSMutableString *string = [NSMutableString stringWithString:@"# Graph data: Mutation frequency spectrum\n"];
	ChromosomeView *chromosome = controller->chromosomeOverview;
	
	if (chromosome->hasSelection)
	{
		slim_position_t selectionFirstBase = chromosome->selectionFirstBase;
		slim_position_t selectionLastBase = chromosome->selectionLastBase;
		
		[string appendFormat:@"# Selected chromosome range: %lld – %lld\n", (int64_t)selectionFirstBase, (int64_t)selectionLastBase];
	}
	
	[string appendString:[self dateline]];
	[string appendString:@"\n\n"];
	
	int binCount = [self histogramBinCount];
	SLiMSim *sim = controller->sim;
	int mutationTypeCount = (int)sim->mutation_types_.size();
	double *plotData = [self mutationFrequencySpectrumWithController:controller mutationTypeCount:mutationTypeCount];
	
	for (auto mutationTypeIter = sim->mutation_types_.begin(); mutationTypeIter != sim->mutation_types_.end(); ++mutationTypeIter)
	{
		MutationType *mutationType = (*mutationTypeIter).second;
		int mutationTypeIndex = mutationType->mutation_type_index_;		// look up the index used for this mutation type in the history info; not necessarily sequential!
		
		[string appendFormat:@"\"m%lld\", ", (int64_t)mutationType->mutation_type_id_];
		
		for (int i = 0; i < binCount; ++i)
		{
			int histIndex = mutationTypeIndex + i * mutationTypeCount;
			
			[string appendFormat:@"%.4f, ", plotData[histIndex]];
		}
		
		[string appendString:@"\n"];
	}
	
	// Get rid of extra commas
	[string replaceOccurrencesOfString:@", \n" withString:@"\n" options:0 range:NSMakeRange(0, [string length])];
	
	return string;
}

@end





























































