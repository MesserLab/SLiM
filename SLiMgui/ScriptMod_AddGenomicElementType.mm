//
//  ScriptMod_AddGenomicElementType.m
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


#import "ScriptMod_AddGenomicElementType.h"

#include <string>
#include <map>
#include <vector>


@implementation ScriptMod_AddGenomicElementType

- (NSString *)sheetTitle
{
	return @"Add Genomic Element Type";
}

- (BOOL)checkEligibility
{
	return ([self checkMutationTypesDefined]);
}

- (void)configSheetLoaded
{
	// set initial control values
	[genomicElementTypeTextField setStringValue:[NSString stringWithFormat:@"%lld", (int64_t)[self bestAvailableGenomicElementTypeID]]];
	
	[self configureMutationTypePopup:mutationType1PopUp];
	[mutationType1ProportionTextField setStringValue:@"1.0"];
	
	[self configureMutationTypePopup:mutationType2PopUp addNoneItem:YES];
	[mutationType2ProportionTextField setStringValue:@"1.0"];
	
	[self configureMutationTypePopup:mutationType3PopUp addNoneItem:YES];
	[mutationType3ProportionTextField setStringValue:@"1.0"];
	
	[super configSheetLoaded];
}

- (IBAction)validateControls:(id)sender
{
	// Determine whether we have valid inputs in all of our fields
	validInput = YES;
	
	BOOL genomicElementTypeValid = [ScriptMod validIntValueInTextField:genomicElementTypeTextField withMin:1 max:SLIM_MAX_ID_VALUE] && [self isAvailableGenomicElementTypeID:SLiMClampToObjectidType([[genomicElementTypeTextField stringValue] longLongValue])];
	validInput = validInput && genomicElementTypeValid;
	[genomicElementTypeTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:genomicElementTypeValid]];
	
	// gather info on the mutation type popup buttons; note that the definition of "enabled" varies, because the first type is required but the others are optional
	BOOL mutationType1PopupEnabled = [mutationType1PopUp isEnabled];
	BOOL mutationType2PopupEnabled = ([mutationType2PopUp selectedTag] != -1);
	BOOL mutationType3PopupEnabled = ([mutationType3PopUp selectedTag] != -1);
	NSInteger mutationTypePopUp1Tag = [mutationType1PopUp selectedTag];
	NSInteger mutationTypePopUp2Tag = [mutationType2PopUp selectedTag];
	NSInteger mutationTypePopUp3Tag = [mutationType3PopUp selectedTag];
	BOOL popUpConflict1_2 = (mutationType1PopupEnabled && mutationType2PopupEnabled && (mutationTypePopUp1Tag == mutationTypePopUp2Tag));
	BOOL popUpConflict1_3 = (mutationType1PopupEnabled && mutationType3PopupEnabled && (mutationTypePopUp1Tag == mutationTypePopUp3Tag));
	BOOL popUpConflict2_3 = (mutationType2PopupEnabled && mutationType3PopupEnabled && (mutationTypePopUp2Tag == mutationTypePopUp3Tag));
	
	// first check the mutation type 1 controls; a mutation type must be chosen here or it is a validation error
	BOOL mutationType1Valid = mutationType1PopupEnabled && !popUpConflict1_2 && !popUpConflict1_3;
	validInput = validInput && mutationType1Valid;
	[mutationType1PopUp slimSetTintColor:(mutationType1Valid ? nil : [ScriptMod validationErrorFilterColor])];
	
	[mutationType1ProportionLabel setTextColor:[ScriptMod textColorForEnableState:mutationType1PopupEnabled]];
	[mutationType1ProportionTextField setEnabled:mutationType1PopupEnabled];
	
	BOOL mutationType1ProportionValid = [ScriptMod validFloatWithScientificNotationValueInTextField:mutationType1ProportionTextField withMin:0.0 max:1000000.0 excludingMin:YES excludingMax:NO];
	validInput = validInput && mutationType1ProportionValid;
	[mutationType1ProportionTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:mutationType1ProportionValid]];
	
	// then check the mutation type 2 controls; if a mutation type is chosen, then the proportion field is enabled and validated
	[mutationType2ProportionLabel setTextColor:[ScriptMod textColorForEnableState:mutationType2PopupEnabled]];
	[mutationType2ProportionTextField setEnabled:mutationType2PopupEnabled];
	[mutationType2ProportionTextField setBackgroundColor:[NSColor whiteColor]];
	
	BOOL mutationType2Valid = !popUpConflict1_2 && !popUpConflict2_3;
	validInput = validInput && mutationType2Valid;
	[mutationType2PopUp slimSetTintColor:(mutationType2Valid ? nil : [ScriptMod validationErrorFilterColor])];
	
	if (mutationType2PopupEnabled)
	{
		BOOL mutationType2ProportionValid = [ScriptMod validFloatWithScientificNotationValueInTextField:mutationType2ProportionTextField withMin:0.0 max:1000000.0 excludingMin:YES excludingMax:NO];
		validInput = validInput && mutationType2ProportionValid;
		[mutationType2ProportionTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:mutationType2ProportionValid]];
	}
	
	// then check the mutation type 3 controls; if a mutation type is chosen, then the proportion field is enabled and validated
	[mutationType3ProportionLabel setTextColor:[ScriptMod textColorForEnableState:mutationType3PopupEnabled]];
	[mutationType3ProportionTextField setEnabled:mutationType3PopupEnabled];
	[mutationType3ProportionTextField setBackgroundColor:[NSColor whiteColor]];
	
	BOOL mutationType3Valid = !popUpConflict1_3 && !popUpConflict2_3;
	validInput = validInput && mutationType3Valid;
	[mutationType3PopUp slimSetTintColor:(mutationType3Valid ? nil : [ScriptMod validationErrorFilterColor])];
	
	if (mutationType3PopupEnabled)
	{
		BOOL mutationType3ProportionValid = [ScriptMod validFloatWithScientificNotationValueInTextField:mutationType3ProportionTextField withMin:0.0 max:1000000.0 excludingMin:YES excludingMax:NO];
		validInput = validInput && mutationType3ProportionValid;
		[mutationType3ProportionTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:mutationType3ProportionValid]];
	}
	
	// determine whether we will need to recycle to simulation to make the change take effect
	needsRecycle = YES;
	
	// now we call super, and it uses validInput and needsRecycle to fix up the UI for us
	[super validateControls:sender];
}

- (NSString *)scriptLineWithExecute:(BOOL)executeNow targetGeneration:(slim_generation_t *)targetGenPtr
{
	slim_objectid_t genomicElementTypeID = SLiMClampToObjectidType([[genomicElementTypeTextField stringValue] longLongValue]);
	slim_objectid_t mutationType1ID = SLiMClampToObjectidType([mutationType1PopUp selectedTag]);
	NSString *mutationType1Proportion = [mutationType1ProportionTextField stringValue];
	slim_objectid_t mutationType2ID = SLiMClampToObjectidType([mutationType2PopUp selectedTag]);
	NSString *mutationType2Proportion = [mutationType2ProportionTextField stringValue];
	slim_objectid_t mutationType3ID = SLiMClampToObjectidType([mutationType3PopUp selectedTag]);
	NSString *mutationType3Proportion = [mutationType3ProportionTextField stringValue];
	
	if (executeNow)
	{
		// queue a -recycle: operation to happen after we're done modifying the script
		[controller performSelector:@selector(recycle:) withObject:nil afterDelay:0.0];
	}
	
	*targetGenPtr = 0;
	
	if ((mutationType2ID != -1) && (mutationType3ID != -1))
		return [NSString stringWithFormat:@"initialize() {\n\tinitializeGenomicElementType(%d, c(%d, %d, %d), c(%@, %@, %@));\n}\n", genomicElementTypeID, mutationType1ID, mutationType2ID, mutationType3ID, mutationType1Proportion, mutationType2Proportion, mutationType3Proportion];
	else if (mutationType2ID != -1)
		return [NSString stringWithFormat:@"initialize() {\n\tinitializeGenomicElementType(%d, c(%d, %d), c(%@, %@));\n}\n", genomicElementTypeID, mutationType1ID, mutationType2ID, mutationType1Proportion, mutationType2Proportion];
	else if (mutationType3ID != -1)
		return [NSString stringWithFormat:@"initialize() {\n\tinitializeGenomicElementType(%d, c(%d, %d), c(%@, %@));\n}\n", genomicElementTypeID, mutationType1ID, mutationType3ID, mutationType1Proportion, mutationType3Proportion];
	else
		return [NSString stringWithFormat:@"initialize() {\n\tinitializeGenomicElementType(%d, %d, %@);\n}\n", genomicElementTypeID, mutationType1ID, mutationType1Proportion];
}

@end















































































