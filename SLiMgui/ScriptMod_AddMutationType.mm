//
//  ScriptMod_AddMutationType.m
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


#import "ScriptMod_AddMutationType.h"

#include <string>
#include <map>
#include <vector>


@implementation ScriptMod_AddMutationType

- (NSString *)sheetTitle
{
	return @"Add Mutation Type";
}

- (NSString *)scriptSectionName
{
	return @"#MUTATION TYPES";
}

- (NSString *)sortingGrepPattern
{
	return [ScriptMod identifierSortingGrepPattern];
}

- (void)configSheetLoaded
{
	// set initial control values
	[mutationTypeTextField setStringValue:[NSString stringWithFormat:@"%d", [self bestAvailableMuttypeID]]];
	[dominanceCoeffTextField setStringValue:@"1.0"];
	[fixedSelCoeffTextField setStringValue:@"1.0"];
	[expMeanSelCoeffTextField setStringValue:@"1.0"];
	[gammaMeanSelCoeffTextField setStringValue:@"1.0"];
	[gammaAlphaTextField setStringValue:@"1.0"];
	
	[super configSheetLoaded];
}

- (IBAction)validateControls:(id)sender
{
	// Determine whether we have valid inputs in all of our fields
	validInput = YES;
	
	BOOL mutationTypeValid = [ScriptMod validIntValueInTextField:mutationTypeTextField withMin:1 max:SLIM_MAX_ID_VALUE] && [self isAvailableMuttypeID:[mutationTypeTextField intValue]];
	validInput = validInput && mutationTypeValid;
	[mutationTypeTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:mutationTypeValid]];
	
	BOOL dominanceCoeffValid = [ScriptMod validFloatValueInTextField:dominanceCoeffTextField withMin:-1000000000.0 max:1000000000.0];
	validInput = validInput && dominanceCoeffValid;
	[dominanceCoeffTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:dominanceCoeffValid]];
	
	int dfeTag = (int)[dfeMatrix selectedTag];
	
	// enable/disable all DFE param textfields and labels
	[fixedDFEParamsLabel setTextColor:[ScriptMod textColorForEnableState:(dfeTag == 0)]];
	[fixedSelCoeffTextField setEnabled:(dfeTag == 0)];
	[expDFEParamsLabel setTextColor:[ScriptMod textColorForEnableState:(dfeTag == 1)]];
	[expMeanSelCoeffTextField setEnabled:(dfeTag == 1)];
	[gammaDFEParamsLabel1 setTextColor:[ScriptMod textColorForEnableState:(dfeTag == 2)]];
	[gammaDFEParamsLabel2 setTextColor:[ScriptMod textColorForEnableState:(dfeTag == 2)]];
	[gammaMeanSelCoeffTextField setEnabled:(dfeTag == 2)];
	[gammaAlphaTextField setEnabled:(dfeTag == 2)];
	
	// set all DFE param textfields to a white background, and then validate them below; this way if they are not enabled they are always white
	[fixedSelCoeffTextField setBackgroundColor:[NSColor whiteColor]];
	[expMeanSelCoeffTextField setBackgroundColor:[NSColor whiteColor]];
	[gammaMeanSelCoeffTextField setBackgroundColor:[NSColor whiteColor]];
	[gammaMeanSelCoeffTextField setBackgroundColor:[NSColor whiteColor]];
	[gammaAlphaTextField setBackgroundColor:[NSColor whiteColor]];
	
	if (dfeTag == 0)		// fixed
	{
		BOOL fixedCoeffValid = [ScriptMod validFloatValueInTextField:fixedSelCoeffTextField withMin:0.0 max:999.0];
		validInput = validInput && fixedCoeffValid;
		[fixedSelCoeffTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:fixedCoeffValid]];
	}
	else if (dfeTag == 1)	// exponential
	{
		BOOL meanCoeffValid = [ScriptMod validFloatValueInTextField:expMeanSelCoeffTextField withMin:0.0 max:999.0 excludingMin:YES excludingMax:NO];
		validInput = validInput && meanCoeffValid;
		[expMeanSelCoeffTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:meanCoeffValid]];
	}
	else if (dfeTag == 2)	// gamma
	{
		BOOL meanCoeffValid = [ScriptMod validFloatValueInTextField:gammaMeanSelCoeffTextField withMin:0.0 max:999.0 excludingMin:YES excludingMax:NO];
		validInput = validInput && meanCoeffValid;
		[gammaMeanSelCoeffTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:meanCoeffValid]];
		
		BOOL alphaValid = [ScriptMod validFloatValueInTextField:gammaAlphaTextField withMin:0.0 max:999.0];
		validInput = validInput && alphaValid;
		[gammaAlphaTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:alphaValid]];
	}
	
	// determine whether we will need to recycle to simulation to make the change take effect
	needsRecycle = YES;
	
	// now we call super, and it uses validInput and needsRecycle to fix up the UI for us
	[super validateControls:sender];
}

- (NSString *)scriptLineWithExecute:(BOOL)executeNow
{
	int mutationTypeID = [mutationTypeTextField intValue];
	NSString *dominanceCoeffString = [dominanceCoeffTextField stringValue];
	int dfeTag = (int)[dfeMatrix selectedTag];
	
	if (executeNow)
	{
		// queue a -recycle: operation to happen after we're done modifying the script
		[controller performSelector:@selector(recycle:) withObject:nil afterDelay:0.0];
	}
	
	if (dfeTag == 0)		// fixed
		return [NSString stringWithFormat:@"m%d %@ f %@", mutationTypeID, dominanceCoeffString, [fixedSelCoeffTextField stringValue]];
	else if (dfeTag == 1)	// exponential
		return [NSString stringWithFormat:@"m%d %@ e %@", mutationTypeID, dominanceCoeffString, [expMeanSelCoeffTextField stringValue]];
	else if (dfeTag == 2)	// gamma
		return [NSString stringWithFormat:@"m%d %@ g %@ %@", mutationTypeID, dominanceCoeffString, [gammaMeanSelCoeffTextField stringValue], [gammaAlphaTextField stringValue]];
	
	return [NSString stringWithFormat:@"m%d %@", mutationTypeID, dominanceCoeffString];
}

@end















































































