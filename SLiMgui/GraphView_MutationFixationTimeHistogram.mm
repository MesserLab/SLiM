//
//  GraphView_MutationFixationTimeHistogram.m
//  SLiM
//
//  Created by Ben Haller on 3/1/15.
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


#import "GraphView_MutationFixationTimeHistogram.h"
#import "SLiMWindowController.h"


@implementation GraphView_MutationFixationTimeHistogram

- (id)initWithFrame:(NSRect)frameRect withController:(SLiMWindowController *)controller
{
	if (self = [super initWithFrame:frameRect withController:controller])
	{
		[self setHistogramBinCount:10];
		
		[self setXAxisMax:1000];
		[self setXAxisMajorTickInterval:200];
		[self setXAxisMinorTickInterval:100];
		[self setXAxisMajorTickModulus:2];
		[self setXAxisTickValuePrecision:0];
		
		[self setXAxisLabelString:@"Mutation fixation time"];
		[self setYAxisLabelString:@"Proportion of fixed mutations"];
		
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
	uint32 *histogram = controller->sim->population_.mutationFixationTimes;
	uint32 histogramBins = controller->sim->population_.mutationFixationGenSlots;	// fewer than binCount * mutationTypeCount may exist
	uint32 total = 0;
	static double *rebin = NULL;
	static uint32 rebinBins = 0;
	uint32 usedRebinBins = binCount * mutationTypeCount;
	
	// re-bin for display; SLiM bins every 10 generations, but right now we want to plot every 100 generations as a bin
	if (!rebin || (rebinBins < usedRebinBins))
	{
		rebinBins = usedRebinBins;
		rebin = (double *)realloc(rebin, rebinBins * sizeof(double));
	}
	
	for (int i = 0; i < usedRebinBins; ++i)
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
	
	// total up all the bins so we can calculate proportions
	for (int i = 0; i < usedRebinBins; ++i)
		total += rebin[i];
	
	// normalize to a total height of one
	for (int i = 0; i < usedRebinBins; ++i)
		rebin[i] /= total;
	
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

- (NSSize)legendSize
{
	return [self mutationTypeLegendSize];	// we use the prefab mutation type legend
}

- (void)drawLegendInRect:(NSRect)legendRect
{
	[self drawMutationTypeLegendInRect:legendRect];		// we use the prefab mutation type legend
}

@end





























































