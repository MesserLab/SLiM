//
//  ScriptMod_AddMutationType.m
//  SLiM
//
//  Created by Ben Haller on 3/21/15.
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


#import "ScriptMod_AddMutationType.h"

#include <string>
#include <map>
#include <vector>


@implementation ScriptMod_AddMutationType

- (NSString *)sheetTitle
{
	return @"Add Mutation Type";
}

- (void)configSheetLoaded
{
	// set initial control values
	[mutationTypeTextField setStringValue:[NSString stringWithFormat:@"%lld", (int64_t)[self bestAvailableMuttypeID]]];
	[dominanceCoeffTextField setStringValue:@"1.0"];
	[fixedSelCoeffTextField setStringValue:@"0.0"];
	[expMeanSelCoeffTextField setStringValue:@"0.1"];
	[gammaMeanSelCoeffTextField setStringValue:@"-0.1"];
	[gammaAlphaTextField setStringValue:@"1.0"];
	[normalMeanSelCoeffTextField setStringValue:@"0.0"];
	[normalSigmaTextField setStringValue:@"0.1"];
	[weibullLambdaTextField setStringValue:@"1.0"];
	[weibullKTextField setStringValue:@"1.0"];
	
	[super configSheetLoaded];
}

- (IBAction)validateControls:(id)sender
{
	// Determine whether we have valid inputs in all of our fields
	validInput = YES;
	
	BOOL mutationTypeValid = [ScriptMod validIntValueInTextField:mutationTypeTextField withMin:1 max:SLIM_MAX_ID_VALUE] && [self isAvailableMuttypeID:SLiMClampToObjectidType([[mutationTypeTextField stringValue] longLongValue])];
	validInput = validInput && mutationTypeValid;
	[mutationTypeTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:mutationTypeValid]];
	
	BOOL dominanceCoeffValid = [ScriptMod validFloatValueInTextField:dominanceCoeffTextField withMin:-1000000000.0 max:1000000000.0];
	validInput = validInput && dominanceCoeffValid;
	[dominanceCoeffTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:dominanceCoeffValid]];
	
	NSInteger dfeTag = [dfeMatrix selectedTag];
	
	// enable/disable all DFE param textfields and labels
	[fixedDFEParamsLabel setTextColor:[ScriptMod textColorForEnableState:(dfeTag == 0)]];
	[fixedSelCoeffTextField setEnabled:(dfeTag == 0)];
	[expDFEParamsLabel setTextColor:[ScriptMod textColorForEnableState:(dfeTag == 1)]];
	[expMeanSelCoeffTextField setEnabled:(dfeTag == 1)];
	[gammaDFEParamsLabel1 setTextColor:[ScriptMod textColorForEnableState:(dfeTag == 2)]];
	[gammaDFEParamsLabel2 setTextColor:[ScriptMod textColorForEnableState:(dfeTag == 2)]];
	[gammaMeanSelCoeffTextField setEnabled:(dfeTag == 2)];
	[gammaAlphaTextField setEnabled:(dfeTag == 2)];
	[normalDFEParamsLabel1 setTextColor:[ScriptMod textColorForEnableState:(dfeTag == 3)]];
	[normalDFEParamsLabel2 setTextColor:[ScriptMod textColorForEnableState:(dfeTag == 3)]];
	[normalMeanSelCoeffTextField setEnabled:(dfeTag == 3)];
	[normalSigmaTextField setEnabled:(dfeTag == 3)];
	[weibullDFEParamsLabel1 setTextColor:[ScriptMod textColorForEnableState:(dfeTag == 4)]];
	[weibullDFEParamsLabel2 setTextColor:[ScriptMod textColorForEnableState:(dfeTag == 4)]];
	[weibullLambdaTextField setEnabled:(dfeTag == 4)];
	[weibullKTextField setEnabled:(dfeTag == 4)];
	
	// set all DFE param textfields to a white background, and then validate them below; this way if they are not enabled they are always white
	[fixedSelCoeffTextField setBackgroundColor:[NSColor whiteColor]];
	[expMeanSelCoeffTextField setBackgroundColor:[NSColor whiteColor]];
	[gammaMeanSelCoeffTextField setBackgroundColor:[NSColor whiteColor]];
	[gammaAlphaTextField setBackgroundColor:[NSColor whiteColor]];
	[normalMeanSelCoeffTextField setBackgroundColor:[NSColor whiteColor]];
	[normalSigmaTextField setBackgroundColor:[NSColor whiteColor]];
	[weibullLambdaTextField setBackgroundColor:[NSColor whiteColor]];
	[weibullKTextField setBackgroundColor:[NSColor whiteColor]];
	
	if (dfeTag == 0)		// fixed
	{
		BOOL fixedCoeffValid = [ScriptMod validFloatValueInTextField:fixedSelCoeffTextField withMin:-999.0 max:999.0];
		validInput = validInput && fixedCoeffValid;
		[fixedSelCoeffTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:fixedCoeffValid]];
	}
	else if (dfeTag == 1)	// exponential
	{
		BOOL meanCoeffValid = [ScriptMod validFloatValueInTextField:expMeanSelCoeffTextField withMin:-999.0 max:999.0 excludingMin:YES excludingMax:NO];
		validInput = validInput && meanCoeffValid;
		[expMeanSelCoeffTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:meanCoeffValid]];
	}
	else if (dfeTag == 2)	// gamma
	{
		BOOL meanCoeffValid = [ScriptMod validFloatValueInTextField:gammaMeanSelCoeffTextField withMin:-999.0 max:999.0 excludingMin:YES excludingMax:NO];
		validInput = validInput && meanCoeffValid;
		[gammaMeanSelCoeffTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:meanCoeffValid]];
		
		BOOL alphaValid = [ScriptMod validFloatValueInTextField:gammaAlphaTextField withMin:0.0 max:999.0];
		validInput = validInput && alphaValid;
		[gammaAlphaTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:alphaValid]];
	}
	else if (dfeTag == 3)	// normal
	{
		BOOL meanCoeffValid = [ScriptMod validFloatValueInTextField:normalMeanSelCoeffTextField withMin:-999.0 max:999.0 excludingMin:YES excludingMax:NO];
		validInput = validInput && meanCoeffValid;
		[normalMeanSelCoeffTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:meanCoeffValid]];
		
		BOOL sigmaValid = [ScriptMod validFloatValueInTextField:normalSigmaTextField withMin:0.0 max:999.0];
		validInput = validInput && sigmaValid;
		[normalSigmaTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:sigmaValid]];
	}
	else if (dfeTag == 4)	// Weibull
	{
		BOOL lambdaValid = [ScriptMod validFloatValueInTextField:weibullLambdaTextField withMin:0.0 max:999.0 excludingMin:YES excludingMax:NO];
		validInput = validInput && lambdaValid;
		[weibullLambdaTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:lambdaValid]];
		
		BOOL kValid = [ScriptMod validFloatValueInTextField:weibullKTextField withMin:0.0 max:999.0 excludingMin:YES excludingMax:NO];
		validInput = validInput && kValid;
		[weibullKTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:kValid]];
	}
	
	// determine whether we will need to recycle to simulation to make the change take effect
	needsRecycle = YES;
	
	// now we call super, and it uses validInput and needsRecycle to fix up the UI for us
	[super validateControls:sender];
}

- (NSString *)scriptLineWithExecute:(BOOL)executeNow targetGeneration:(slim_generation_t *)targetGenPtr
{
	slim_objectid_t mutationTypeID = SLiMClampToObjectidType([[mutationTypeTextField stringValue] longLongValue]);
	NSString *dominanceCoeffString = [dominanceCoeffTextField stringValue];
	NSInteger dfeTag = [dfeMatrix selectedTag];
	
	if (executeNow)
	{
		// queue a -recycle: operation to happen after we're done modifying the script
		[controller performSelector:@selector(recycle:) withObject:nil afterDelay:0.0];
	}
	
	*targetGenPtr = 0;
	
	if (dfeTag == 0)		// fixed
		return [NSString stringWithFormat:@"initialize() {\n\tinitializeMutationType(%d, %@, \"f\", %@);\n}\n", mutationTypeID, dominanceCoeffString, [fixedSelCoeffTextField stringValue]];
	else if (dfeTag == 1)	// exponential
		return [NSString stringWithFormat:@"initialize() {\n\tinitializeMutationType(%d, %@, \"e\", %@);\n}\n", mutationTypeID, dominanceCoeffString, [expMeanSelCoeffTextField stringValue]];
	else if (dfeTag == 2)	// gamma
		return [NSString stringWithFormat:@"initialize() {\n\tinitializeMutationType(%d, %@, \"g\", %@, %@);\n}\n", mutationTypeID, dominanceCoeffString, [gammaMeanSelCoeffTextField stringValue], [gammaAlphaTextField stringValue]];
	else if (dfeTag == 3)	// normal
		return [NSString stringWithFormat:@"initialize() {\n\tinitializeMutationType(%d, %@, \"n\", %@, %@);\n}\n", mutationTypeID, dominanceCoeffString, [normalMeanSelCoeffTextField stringValue], [normalSigmaTextField stringValue]];
	else if (dfeTag == 4)	// Weibull
		return [NSString stringWithFormat:@"initialize() {\n\tinitializeMutationType(%d, %@, \"w\", %@, %@);\n}\n", mutationTypeID, dominanceCoeffString, [weibullLambdaTextField stringValue], [weibullKTextField stringValue]];
	
	return [NSString stringWithFormat:@"initialize() {\n\tinitializeMutationType(%d, %@);\n}\n", mutationTypeID, dominanceCoeffString];
}

@end















































































