//
//  GraphView_MutationLossTimeHistogram.m
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


#import "GraphView_MutationLossTimeHistogram.h"
#import "SLiMWindowController.h"


@implementation GraphView_MutationLossTimeHistogram

- (id)initWithFrame:(NSRect)frameRect withController:(SLiMWindowController *)controller
{
	if (self = [super initWithFrame:frameRect withController:controller])
	{
		[self setHistogramBinCount:10];
		
		[self setXAxisMax:100];
		[self setXAxisMajorTickInterval:20];
		[self setXAxisMinorTickInterval:10];
		[self setXAxisMajorTickModulus:2];
		[self setXAxisTickValuePrecision:0];
		
		[self setXAxisLabelString:@"Mutation loss time"];
		[self setYAxisLabelString:@"Proportion of lost mutations"];
		
		[self setShowHorizontalGridLines:YES];
	}
	
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (void)drawGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller
{
	int binCount = [self histogramBinCount];
	int mutationTypeCount = (int)controller->sim->mutation_types_.size();
	uint32 *histogram = controller->sim->population_.mutationLossTimes;
	uint32 histogramBins = controller->sim->population_.mutationLossGenSlots;	// fewer than binCount * mutationTypeCount may exist
	uint32 total = 0;
	
	// total up all the bins so we can calculate proportions
	for (int i = 0; i < histogramBins; ++i)
		total += histogram[i];
	
	//NSLog(@"histogram %p histogramBins %d total %d", histogram, histogramBins, total);
	
	// plot our histogram bars
	[self drawGroupedBarplotInInteriorRect:interiorRect withController:controller buffer:histogram bufferLength:histogramBins subBinCount:mutationTypeCount mainBinCount:binCount firstBinValue:0.0 mainBinWidth:10.0 heightNormalizer:(double)total];
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





























































