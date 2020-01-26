//
//  GraphView_MutationFixationTimeHistogram.m
//  SLiM
//
//  Created by Ben Haller on 3/1/15.
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


#import "GraphView_MutationFixationTimeHistogram.h"
#import "SLiMWindowController.h"


@implementation GraphView_MutationFixationTimeHistogram

- (id)initWithFrame:(NSRect)frameRect withController:(SLiMWindowController *)controller
{
	if (self = [super initWithFrame:frameRect withController:controller])
	{
		[self setHistogramBinCount:10];
		//[self setAllowXAxisBinRescale:YES];	// not supported yet
		
		[self setXAxisMax:1000];
		[self setXAxisMajorTickInterval:200];
		[self setXAxisMinorTickInterval:100];
		[self setXAxisMajorTickModulus:2];
		[self setXAxisTickValuePrecision:0];
		
		[self setXAxisLabelString:@"Mutation fixation time"];
		[self setYAxisLabelString:@"Proportion of fixed mutations"];
		
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

- (double *)fixationTimeDataWithController:(SLiMWindowController *)controller
{
	int binCount = [self histogramBinCount];
	int mutationTypeCount = (int)controller->sim->mutation_types_.size();
	slim_generation_t *histogram = controller->sim->population_.mutation_fixation_times_;
	int64_t histogramBins = (int64_t)controller->sim->population_.mutation_fixation_gen_slots_;	// fewer than binCount * mutationTypeCount may exist
	static double *rebin = NULL;
	static uint32_t rebinBins = 0;
	uint32_t usedRebinBins = binCount * mutationTypeCount;
	
	// re-bin for display; SLiM bins every 10 generations, but right now we want to plot every 100 generations as a bin
	if (!rebin || (rebinBins < usedRebinBins))
	{
		rebinBins = usedRebinBins;
		rebin = (double *)realloc(rebin, rebinBins * sizeof(double));
	}
	
	for (unsigned int i = 0; i < usedRebinBins; ++i)
		rebin[i] = 0.0;
	
	for (int i = 0; i < binCount * 10; ++i)
	{
		for (int j = 0; j < mutationTypeCount; ++j)
		{
			int histIndex = j + i * mutationTypeCount;
			
			if (histIndex < histogramBins)
				rebin[j + (i / 10) * mutationTypeCount] += histogram[histIndex];
		}
	}
	
	// normalize within each mutation type
	for (int mutationTypeIndex = 0; mutationTypeIndex < mutationTypeCount; ++mutationTypeIndex)
	{
		uint32_t total = 0;
		
		for (int bin = 0; bin < binCount; ++bin)
		{
			int binIndex = mutationTypeIndex + bin * mutationTypeCount;
			
			total += (int64_t)rebin[binIndex];
		}
		
		for (int bin = 0; bin < binCount; ++bin)
		{
			int binIndex = mutationTypeIndex + bin * mutationTypeCount;
			
			rebin[binIndex] /= (double)total;
		}
	}
	
	return rebin;
}

- (void)drawGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller
{
	double *plotData = [self fixationTimeDataWithController:controller];
	int binCount = [self histogramBinCount];
	int mutationTypeCount = (int)controller->sim->mutation_types_.size();
	
	// plot our histogram bars
	[self drawGroupedBarplotInInteriorRect:interiorRect withController:controller buffer:plotData subBinCount:mutationTypeCount mainBinCount:binCount firstBinValue:0.0 mainBinWidth:100.0];
}

- (NSArray *)legendKey
{
	return [self mutationTypeLegendKey];	// we use the prefab mutation type legend
}

- (NSString *)stringForDataWithController:(SLiMWindowController *)controller
{
	NSMutableString *string = [NSMutableString stringWithString:@"# Graph data: Mutation fixation time histogram\n"];
	
	[string appendString:[self dateline]];
	[string appendString:@"\n\n"];
	
	double *plotData = [self fixationTimeDataWithController:controller];
	int binCount = [self histogramBinCount];
	SLiMSim *sim = controller->sim;
	int mutationTypeCount = (int)sim->mutation_types_.size();
	
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





























































