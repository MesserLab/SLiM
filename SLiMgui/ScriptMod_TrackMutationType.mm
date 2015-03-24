//
//  ScriptMod_TrackMutationType.m
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


#import "ScriptMod_TrackMutationType.h"

#include <string>
#include <map>
#include <vector>


@implementation ScriptMod_TrackMutationType

- (NSString *)sheetTitle
{
	return @"Track Mutation Type";
}

- (NSString *)scriptSectionName
{
	return @"#OUTPUT";
}

- (NSString *)sortingGrepPattern
{
	return [ScriptMod scientificIntSortingGrepPattern];
}

- (void)configSheetLoaded
{
	// set initial control values
	[generationTextField setStringValue:[NSString stringWithFormat:@"%d", controller->sim->generation_]];
	[self configureMutationTypePopup:mutationTypePopUpButton];
	
	[super configSheetLoaded];
}

- (IBAction)validateControls:(id)sender
{
	// Determine whether we have valid inputs in all of our fields
	validInput = YES;
	
	BOOL generationValid = [ScriptMod validIntWithScientificNotationValueInTextField:generationTextField withMin:1 max:1000000000];
	validInput = validInput && generationValid;
	[generationTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:generationValid]];
	
	BOOL mutationTypeValid = [mutationTypePopUpButton isEnabled];
	validInput = validInput && mutationTypeValid;
	[mutationTypePopUpButton slimSetTintColor:(mutationTypeValid ? nil : [ScriptMod validationErrorFilterColor])];
	
	// determine whether we will need to recycle to simulation to make the change take effect
	needsRecycle = ((int)[generationTextField doubleValue] < controller->sim->generation_);		// handle scientific notation
	
	// now we call super, and it uses validInput and needsRecycle to fix up the UI for us
	[super validateControls:sender];
}

- (NSString *)scriptLineWithExecute:(BOOL)executeNow
{
	NSString *targetGeneration = [generationTextField stringValue];
	int targetGenerationInt = (int)[targetGeneration doubleValue];
	int mutationTypeID = (int)[mutationTypePopUpButton selectedTag];
	
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
			NSString *param1 = [NSString stringWithFormat:@"m%d", mutationTypeID];
			std::vector<std::string> event_parameters;
			
			event_parameters.push_back(std::string([param1 UTF8String]));
			
			Event *new_event_ptr = new Event('T', event_parameters);
			
			controller->sim->events_.insert(std::pair<const int,Event*>(targetGenerationInt, new_event_ptr));
		}
	}
	
	return [NSString stringWithFormat:@"%@ T m%d", targetGeneration, mutationTypeID];
}

@end















































































