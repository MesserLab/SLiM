//
//  ScriptMod_AddRecombinationRate.m
//  SLiM
//
//  Created by Ben Haller on 3/22/15.
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


#import "ScriptMod_AddRecombinationRate.h"

#include <string>
#include <map>
#include <vector>


@implementation ScriptMod_AddRecombinationRate

- (NSString *)sheetTitle
{
	return @"Add Recombination Rate";
}

- (void)configSheetLoaded
{
	// set initial control values
	[recombinationRateTextField setStringValue:@"0.00000001"];
	
	[super configSheetLoaded];
}

- (IBAction)validateControls:(id)sender
{
	// Determine whether we have valid inputs in all of our fields
	validInput = YES;
	
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
	NSString *rateString = [recombinationRateTextField stringValue];
	
	if (executeNow)
	{
		// queue a -recycle: operation to happen after we're done modifying the script
		[controller performSelector:@selector(recycle:) withObject:nil afterDelay:0.0];
	}
	
	*targetGenPtr = 0;
	
	return [NSString stringWithFormat:@"initialize() {\n\tinitializeRecombinationRate(%@);\n}\n", rateString];
}

@end















































































