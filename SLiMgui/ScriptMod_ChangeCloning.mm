//
//  ScriptMod_ChangeCloning.m
//  SLiM
//
//  Created by Ben Haller on 8/5/15.
//  Copyright (c) 2015-2022 Philipp Messer.  All rights reserved.
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


#import "ScriptMod_ChangeCloning.h"

#include <string>
#include <map>
#include <vector>


@implementation ScriptMod_ChangeCloning

- (NSString *)sheetTitle
{
	return @"Change Cloning Rate";
}

- (BOOL)checkEligibility
{
	return ([self checkSubpopsDefined]);
}

- (void)configSheetLoaded
{
	// set initial control values
	[generationTextField setStringValue:[NSString stringWithFormat:@"%lld", (int64_t)controller->sim->generation_]];
	[self configureSubpopulationPopup:subpopPopUpButton];
	[cloningRateTextField setStringValue:@"0.0"];
	
	[super configSheetLoaded];
}

- (IBAction)validateControls:(id)sender
{
	// Determine whether we have valid inputs in all of our fields
	validInput = YES;
	
	BOOL generationValid = [ScriptMod validIntWithScientificNotationValueInTextField:generationTextField withMin:1 max:SLIM_MAX_GENERATION];
	validInput = validInput && generationValid;
	[generationTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:generationValid]];
	
	BOOL subpopValid = [subpopPopUpButton isEnabled];
	validInput = validInput && subpopValid;
	[subpopPopUpButton slimSetTintColor:(subpopValid ? nil : [ScriptMod validationErrorFilterColor])];
	
	BOOL rateValid = [ScriptMod validFloatWithScientificNotationValueInTextField:cloningRateTextField withMin:0.0 max:1.0];
	validInput = validInput && rateValid;
	[cloningRateTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:rateValid]];
	
	// determine whether we will need to recycle to simulation to make the change take effect
	needsRecycle = ((int64_t)[generationTextField doubleValue] < controller->sim->generation_);		// handle scientific notation
	
	// now we call super, and it uses validInput and needsRecycle to fix up the UI for us
	[super validateControls:sender];
}

- (NSString *)scriptLineWithExecute:(BOOL)executeNow targetGeneration:(slim_generation_t *)targetGenPtr
{
	NSString *targetGeneration = [generationTextField stringValue];
	slim_generation_t targetGenerationInt = SLiMClampToGenerationType((int64_t)[targetGeneration doubleValue]);
	slim_objectid_t populationID = SLiMClampToObjectidType([subpopPopUpButton selectedTag]);
	NSString *newRate = [cloningRateTextField stringValue];
	
	NSString *scriptInternal = [NSString stringWithFormat:@"{\n\tp%d.setCloningRate(%@);\n}", populationID, newRate];
	NSString *scriptCommand = [NSString stringWithFormat:@"%@ %@\n", targetGeneration, scriptInternal];
	
	if (executeNow)
	{
		if (needsRecycle)
		{
			// queue a -recycle: operation to happen after we're done modifying the script
			[controller performSelector:@selector(recycle:) withObject:nil afterDelay:0.0];
		}
		else
		{
			// insert the script into the simulation's script block list
			std::string scriptInternalString([scriptInternal UTF8String]);
			SLiMEidosBlock *script_block = new SLiMEidosBlock(-1, scriptInternalString, -1, SLiMEidosBlockType::SLiMEidosEventEarly, targetGenerationInt, targetGenerationInt);
			
			[controller addScriptBlockToSimulation:script_block];	// takes ownership from us
		}
	}
	
	*targetGenPtr = targetGenerationInt;
	
	return scriptCommand;
}

@end















































































