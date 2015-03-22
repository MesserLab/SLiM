//
//  ScriptMod_AddSubpop.m
//  SLiM
//
//  Created by Ben Haller on 3/21/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
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


#import "ScriptMod_AddSubpop.h"

#include <string>
#include <map>
#include <vector>


@implementation ScriptMod_AddSubpop

- (NSString *)sheetTitle
{
	return @"Add Subpopulation";
}

- (void)configSheetLoaded
{
	// set initial control values
	[generationTextField setStringValue:[NSString stringWithFormat:@"%d", controller->sim->generation_]];
	[subpopTextField setStringValue:[NSString stringWithFormat:@"%d", [self bestAvailableSubpopID]]];
	[subpopSizeTextField setStringValue:@"1000"];
	
	[super configSheetLoaded];
}

- (IBAction)validateControls:(id)sender
{
	// Determine whether we have valid inputs in all of our fields
	validInput = YES;
	
	BOOL generationValid = [ScriptMod validIntValueInTextField:generationTextField withMin:1 max:1000000000];
	validInput = validInput && generationValid;
	[generationTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:generationValid]];
	
	BOOL subpopValid = [ScriptMod validIntValueInTextField:subpopTextField withMin:1 max:1000000000] && [self isAvailableSubpopID:[subpopTextField intValue]];
	validInput = validInput && subpopValid;
	[subpopTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:subpopValid]];
	
	BOOL sizeValid = [ScriptMod validIntValueInTextField:subpopSizeTextField withMin:1 max:1000000000];
	validInput = validInput && sizeValid;
	[subpopSizeTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:sizeValid]];
	
	// determine whether we will need to recycle to simulation to make the change take effect
	needsRecycle = ([generationTextField intValue] < controller->sim->generation_);
	
	// now we call super, and it uses validInput and needsRecycle to fix up the UI for us
	[super validateControls:sender];
}

- (NSString *)scriptLineWithExecute:(BOOL)executeNow
{
	int targetGeneration = [generationTextField intValue];
	int populationID = [subpopTextField intValue];
	int newSize = [subpopSizeTextField intValue];
	
	if (executeNow)
	{
		if (needsRecycle)
		{
			// queue a -recycle: operation to happen after we're done modifying the script
			[controller performSelector:@selector(recycle:) withObject:nil afterDelay:0.0];
		}
		else
		{
			// insert the event into the simulation's event map
			NSString *param1 = [NSString stringWithFormat:@"p%d", populationID];
			NSString *param2 = [NSString stringWithFormat:@"%d", newSize];
			std::vector<std::string> event_parameters;
			
			event_parameters.push_back(std::string([param1 UTF8String]));
			event_parameters.push_back(std::string([param2 UTF8String]));
			
			Event *new_event_ptr = new Event('P', event_parameters);
			
			controller->sim->events_.insert(std::pair<const int,Event*>(targetGeneration, new_event_ptr));
		}
	}
	
	return [NSString stringWithFormat:@"%d P p%d %d", targetGeneration, populationID, newSize];
}

@end















































































