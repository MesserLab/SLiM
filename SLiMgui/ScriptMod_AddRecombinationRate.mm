//
//  ScriptMod_AddRecombinationRate.m
//  SLiM
//
//  Created by Ben Haller on 3/22/15.
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


#import "ScriptMod_AddRecombinationRate.h"

#include <string>
#include <map>
#include <vector>


@implementation ScriptMod_AddRecombinationRate

- (NSString *)sheetTitle
{
	return @"Add Recombination Rate";
}

- (slim_position_t)lastDefinedRecombinationPosition
{
	std::vector<slim_position_t> &endPositions = controller->sim->chromosome_.recombination_end_positions_;
	slim_position_t lastPosition = 0;
	
	for (auto positionIter = endPositions.begin(); positionIter != endPositions.end(); positionIter++)
	{
		slim_position_t endPosition = *positionIter;	// used to have a +1; switched to zero-based
		
		if (endPosition > lastPosition)
			lastPosition = endPosition;
	}
	
	return lastPosition;
}

- (void)configSheetLoaded
{
	slim_position_t lastDefinedPosition = [self lastDefinedRecombinationPosition];
	
	// set initial control values
	[intervalEndPositionTextField setStringValue:[NSString stringWithFormat:@"%lld", (int64_t)lastDefinedPosition + 1]];
	[recombinationRateTextField setStringValue:@"0.00000001"];
	
	[super configSheetLoaded];
}

- (IBAction)validateControls:(id)sender
{
	slim_position_t lastDefinedPosition = [self lastDefinedRecombinationPosition];
	slim_position_t endPosition = SLiMClampToPositionType((int64_t)[intervalEndPositionTextField doubleValue]);		// handle scientific notation
	
	// Determine whether we have valid inputs in all of our fields
	validInput = YES;
	
	BOOL endValid = [ScriptMod validIntWithScientificNotationValueInTextField:intervalEndPositionTextField withMin:1 max:SLIM_MAX_BASE_POSITION];
	endValid = endValid && (!endValid || (endPosition > lastDefinedPosition));
	validInput = validInput && endValid;
	[intervalEndPositionTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:endValid]];
	
	BOOL rateValid = [ScriptMod validFloatWithScientificNotationValueInTextField:recombinationRateTextField withMin:0.0 max:1.0];
	validInput = validInput && rateValid;
	[recombinationRateTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:rateValid]];
	
	// determine whether we will need to recycle to simulation to make the change take effect
	needsRecycle = YES;
	
	// now we call super, and it uses validInput and needsRecycle to fix up the UI for us
	[super validateControls:sender];
}

- (NSString *)scriptLineWithExecute:(BOOL)executeNow targetGeneration:(slim_generation_t *)targetGenPtr
{
	NSString *endPosition = [intervalEndPositionTextField stringValue];
	NSString *rateString = [recombinationRateTextField stringValue];
	
	if (executeNow)
	{
		// queue a -recycle: operation to happen after we're done modifying the script
		[controller performSelector:@selector(recycle:) withObject:nil afterDelay:0.0];
	}
	
	*targetGenPtr = 0;
	
	return [NSString stringWithFormat:@"initialize() {\n\tinitializeRecombinationRate(%@, %@);\n}\n", rateString, endPosition];
}

@end















































































