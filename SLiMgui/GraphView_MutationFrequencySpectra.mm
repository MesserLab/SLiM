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
	static uint32 *spectrum = NULL;			// used for tallying
	static double *doubleSpectrum = NULL;	// not used for tallying, to avoid precision issues
	static uint32 spectrumBins = 0;
	int binCount = [self histogramBinCount];
	uint32 usedSpectrumBins = binCount * mutationTypeCount;
	
	// allocate our bins
	if (!spectrum || (spectrumBins < usedSpectrumBins))
	{
		spectrumBins = usedSpectrumBins;
		spectrum = (uint32 *)realloc(spectrum, spectrumBins * sizeof(uint32));
		doubleSpectrum = (double *)realloc(doubleSpectrum, spectrumBins * sizeof(double));
	}
	
	// clear our bins
	for (int i = 0; i < usedSpectrumBins; ++i)
		spectrum[i] = 0;
	
	// get the selected chromosome range
	ChromosomeView *chromosome = controller->chromosomeOverview;
	BOOL hasSelection = chromosome->hasSelection;
	int selectionFirstBase = chromosome->selectionFirstBase - 1;	// correct from 1-based to 0-based
	int selectionLastBase = chromosome->selectionLastBase - 1;
	
	// tally into our bins
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	double totalGenomeCount = pop.total_genome_count_;
	Genome &mutationRegistry = pop.mutation_registry_;
	Mutation **mutations = mutationRegistry.mutations_;
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
	
	// normalize within each mutation type
	for (int mutationTypeIndex = 0; mutationTypeIndex < mutationTypeCount; ++mutationTypeIndex)
	{
		uint32 total = 0;
		
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
	[self drawGroupedBarplotInInteriorRect:interiorRect withController:controller buffer:spectrum subBinCount:mutationTypeCount mainBinCount:binCount firstBinValue:0.0 mainBinWidth:0.10];
	
	// if we have a limited selection range, overdraw a note about that
	ChromosomeView *chromosome = controller->chromosomeOverview;
	
	if (chromosome->hasSelection)
	{
		int selectionFirstBase = chromosome->selectionFirstBase;
		int selectionLastBase = chromosome->selectionLastBase;
		static NSDictionary *attrs = nil;
		
		if (!attrs)
			attrs = [[NSDictionary alloc] initWithObjectsAndKeys:[NSFont fontWithName:[GraphView labelFontName] size:10], NSFontAttributeName, [NSColor darkGrayColor], NSForegroundColorAttributeName, nil];
		
		NSString *labelText = [NSString stringWithFormat:@"%d – %d", selectionFirstBase, selectionLastBase];
		NSAttributedString *attributedLabel = [[NSMutableAttributedString alloc] initWithString:labelText attributes:attrs];
		NSSize labelSize = [attributedLabel size];
		double labelX = interiorRect.origin.y + (interiorRect.size.width - labelSize.width) / 2.0;
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
		int selectionFirstBase = chromosome->selectionFirstBase;
		int selectionLastBase = chromosome->selectionLastBase;
		
		[string appendFormat:@"# Selected chromosome range: %d – %d\n", selectionFirstBase, selectionLastBase];
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
		
		[string appendFormat:@"\"m%d\", ", mutationType->mutation_type_id_];
		
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





























































