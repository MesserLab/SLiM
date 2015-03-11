//
//  GraphView_MutationFrequencyTrajectory.m
//  SLiM
//
//  Created by Ben Haller on 3/11/15.
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


#import "GraphView_MutationFrequencyTrajectory.h"
#import "SLiMWindowController.h"


@implementation GraphView_MutationFrequencyTrajectory

- (id)initWithFrame:(NSRect)frameRect withController:(SLiMWindowController *)controller
{
	if (self = [super initWithFrame:frameRect withController:controller])
	{
		[self setXAxisMax:[self slimWindowController]->sim->time_duration_];
		[self setXAxisMajorTickInterval:10000];
		[self setXAxisMinorTickInterval:1000];
		[self setXAxisMajorTickModulus:10];
		[self setXAxisTickValuePrecision:0];
		
		[self setXAxisLabelString:@"Generation"];
		[self setYAxisLabelString:@"Frequency"];
		
		[self setAllowXAxisUserRescale:YES];
		[self setAllowYAxisUserRescale:YES];
		
		[self setShowHorizontalGridLines:YES];
		[self setTweakXAxisTickLabelAlignment:YES];
	}
	
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (void)controllerRecycled
{
	SLiMWindowController *controller = [self slimWindowController];
	
	if (![controller invalidSimulation])
	{
		if (![self xAxisIsUserRescaled])
			[self setXAxisMax:controller->sim->time_duration_];
		
		[self setNeedsDisplay:YES];
	}
	
	[super controllerRecycled];
}

- (void)controllerGenerationFinished
{
	[super controllerGenerationFinished];
	
	
}

- (void)drawGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller
{
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;

	// line plot
	NSBezierPath *linePath = [NSBezierPath bezierPath];
	double *history = pop.fitnessHistory;
	uint32 historyLength = pop.fitnessHistoryLength;
	BOOL startedPath = NO;
	
	for (int i = 0; i < historyLength; ++i)
	{
		double historyEntry = history[i];
		
		if (!isnan(historyEntry))
		{
			NSPoint historyPoint = NSMakePoint([self plotToDeviceX:i withInteriorRect:interiorRect], [self plotToDeviceY:historyEntry withInteriorRect:interiorRect]);
			
			if (!startedPath)
			{
				[linePath moveToPoint:historyPoint];
				startedPath = YES;
			}
			else
			{
				[linePath lineToPoint:historyPoint];
			}
		}
	}
	
	[linePath setLineWidth:1.0];
	[[NSColor blackColor] set];
	[linePath stroke];
}

//- (NSString *)stringForDataWithController:(SLiMWindowController *)controller
//{
//}

@end





























































