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

- (id)initWithFrame:(NSRect)frameRect
{
	if (self = [super initWithFrame:frameRect])
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
		int32_t mutationRefCount = mutation->reference_count_;
		double mutationFrequency = mutationRefCount / totalGenomeCount;
		int mutationBin = (int)floor(mutationFrequency * binCount);
		int mutationTypeIndex = mutation->mutation_type_ptr_->mutation_type_index_;
		
		if (mutationBin == binCount)
			mutationBin = binCount - 1;
		
		(spectrum[mutationBin + mutationTypeIndex * binCount])++;	// all bins in sequence for one mutation type, then for the next, etc.
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
	for (int i = 0; i < binCount; ++i)
	{
		double binMinFrequency = i / (double)binCount;
		double binMaxFrequency = (i + 1) / (double)binCount;
		double barLeft = [self roundPlotToDeviceX:binMinFrequency withInteriorRect:interiorRect] + 1.5;
		double barRight = [self roundPlotToDeviceX:binMaxFrequency withInteriorRect:interiorRect] - 1.5;
		
		for (int j = 0; j < mutationTypeCount; ++j)
		{
			uint32 binValue = spectrum[i + j * binCount];
			double barBottom = interiorRect.origin.y - 1;
			double barTop = (binValue == 0 ? interiorRect.origin.y - 1 : [self roundPlotToDeviceY:binValue / (double)total withInteriorRect:interiorRect] + 0.5);
			NSRect barRect = NSMakeRect(barLeft, barBottom, barRight - barLeft, barTop - barBottom);
			
			if (barRect.size.height > 0.0)
			{
				// subdivide into sub-bars for each mutation type, if there is more than one
				if (mutationTypeCount > 1)
				{
					double barWidth = barRect.size.width;
					double subBarWidth = (barWidth - 1) / mutationTypeCount;
					double subbarLeft = SCREEN_ROUND(barRect.origin.x + j * subBarWidth);
					double subbarRight = SCREEN_ROUND(barRect.origin.x + (j + 1) * subBarWidth) + 1;
					
					barRect.origin.x = subbarLeft;
					barRect.size.width = subbarRight - subbarLeft;
				}
				
				// fill and fill
				[[SLiMWindowController colorForIndex:j] set];
				NSRectFill(barRect);
				
				[[NSColor blackColor] set];
				NSFrameRect(barRect);
			}
		}
	}
}

- (NSSize)legendSize
{
	SLiMWindowController *controller = [self slimWindowController];
	SLiMSim *sim = controller->sim;
	int mutationTypeCount = (int)sim->mutation_types_.size();
	
	// If we only have one mutation type, do not show a legend
	if (mutationTypeCount < 2)
		return NSZeroSize;
	
	const int legendRowHeight = 15;
	NSDictionary *legendAttrs = [[self class] attributesForLegendLabels];
	auto mutationTypeIter = sim->mutation_types_.begin();
	NSSize legendSize = NSMakeSize(0, legendRowHeight * mutationTypeCount - 6);
	
	for (int i = 0; i < mutationTypeCount; ++i, ++mutationTypeIter)
	{
		MutationType *mutationType = (*mutationTypeIter).second;
		NSRect swatchRect = NSMakeRect(0, ((mutationTypeCount - 1) * legendRowHeight) - i * legendRowHeight + 3, legendRowHeight - 6, legendRowHeight - 6);
		NSString *labelString = [NSString stringWithFormat:@"m%d", mutationType->mutation_type_id_];
		NSAttributedString *label = [[NSAttributedString alloc] initWithString:labelString attributes:legendAttrs];
		NSSize labelSize = [label size];
		
		legendSize.width = MAX(legendSize.width, swatchRect.origin.x + swatchRect.size.width + 5 + labelSize.width);
		[label release];
	}
	
	//NSLog(@"mutationTypeCount == %d, legendSize == %@", mutationTypeCount, NSStringFromSize(legendSize));
	return legendSize;
}

- (void)drawLegendInRect:(NSRect)legendRect
{
	const int legendRowHeight = 15;
	NSDictionary *legendAttrs = [[self class] attributesForLegendLabels];
	SLiMWindowController *controller = [self slimWindowController];
	SLiMSim *sim = controller->sim;
	int mutationTypeCount = (int)sim->mutation_types_.size();
	auto mutationTypeIter = sim->mutation_types_.begin();
	
	for (int i = 0; i < mutationTypeCount; ++i, ++mutationTypeIter)
	{
		MutationType *mutationType = (*mutationTypeIter).second;
		NSRect swatchRect = NSMakeRect(legendRect.origin.x, legendRect.origin.y + ((mutationTypeCount - 1) * legendRowHeight - 3) - i * legendRowHeight + 3, legendRowHeight - 6, legendRowHeight - 6);
		NSString *labelString = [NSString stringWithFormat:@"m%d", mutationType->mutation_type_id_];
		NSAttributedString *label = [[NSAttributedString alloc] initWithString:labelString attributes:legendAttrs];
		
		[[SLiMWindowController colorForIndex:i] set];
		NSRectFill(swatchRect);
		
		[[NSColor blackColor] set];
		NSFrameRect(swatchRect);
		
		[label drawAtPoint:NSMakePoint(swatchRect.origin.x + swatchRect.size.width + 5, swatchRect.origin.y - 2)];
		[label release];
	}
	
	//NSLog(@"drawLegendInRect: %@", NSStringFromRect(legendRect));
}

@end





























































