//
//  ScriptMod_AddGenomicElement.m
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


#import "ScriptMod_AddGenomicElement.h"

#include <string>
#include <map>
#include <vector>


@implementation ScriptMod_AddGenomicElement

- (NSString *)sheetTitle
{
	return @"Add Genomic Element to Chromosome";
}

- (BOOL)checkEligibility
{
	return ([self checkGenomicElementTypesDefined]);
}

- (void)configSheetLoaded
{
	// find last defined chromosome position
	Chromosome &chromosome = controller->sim->chromosome_;
	slim_position_t lastPosition = 0;
	
	for (GenomicElement *element : chromosome.GenomicElements())
	{
		slim_position_t endPosition = element->end_position_;
		
		if (endPosition > lastPosition)
			lastPosition = endPosition;
	}
	
	// set initial control values
	[self configureGenomicElementTypePopup:genomicElementTypePopUpButton];
	[startPositionTextField setStringValue:[NSString stringWithFormat:@"%lld", (int64_t)lastPosition + 1]];
	[endPositionTextField setStringValue:[NSString stringWithFormat:@"%lld", (int64_t)lastPosition + 1]];
	
	[super configSheetLoaded];
}

- (IBAction)validateControls:(id)sender
{
	// Determine whether we have valid inputs in all of our fields
	validInput = YES;
	
	BOOL genomicElementTypeValid = [genomicElementTypePopUpButton isEnabled];
	validInput = validInput && genomicElementTypeValid;
	[genomicElementTypePopUpButton slimSetTintColor:(genomicElementTypeValid ? nil : [ScriptMod validationErrorFilterColor])];
	
	slim_position_t startPosition = SLiMClampToPositionType((int64_t)[startPositionTextField doubleValue]);	// handle scientific notation
	slim_position_t endPosition = SLiMClampToPositionType((int64_t)[endPositionTextField doubleValue]);		// handle scientific notation
	
	BOOL startValid = [ScriptMod validIntWithScientificNotationValueInTextField:startPositionTextField withMin:1 max:SLIM_MAX_BASE_POSITION];
	validInput = validInput && startValid;
	[startPositionTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:startValid]];
	
	BOOL endValid = [ScriptMod validIntWithScientificNotationValueInTextField:endPositionTextField withMin:1 max:SLIM_MAX_BASE_POSITION];
	endValid = endValid && (!startValid || !endValid || (startPosition <= endPosition));		// no inverted ranges
	validInput = validInput && endValid;
	[endPositionTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:endValid]];
	
	// determine whether we will need to recycle to simulation to make the change take effect
	needsRecycle = YES;
	
	// now we call super, and it uses validInput and needsRecycle to fix up the UI for us
	[super validateControls:sender];
}

- (NSString *)scriptLineWithExecute:(BOOL)executeNow targetGeneration:(slim_generation_t *)targetGenPtr
{
	slim_objectid_t genomicElementTypeID = SLiMClampToObjectidType([genomicElementTypePopUpButton selectedTag]);
	NSString *startPosition = [startPositionTextField stringValue];
	NSString *endPosition = [endPositionTextField stringValue];
	
	if (executeNow)
	{
		// queue a -recycle: operation to happen after we're done modifying the script
		[controller performSelector:@selector(recycle:) withObject:nil afterDelay:0.0];
	}
	
	*targetGenPtr = 0;
	
	return [NSString stringWithFormat:@"initialize() {\n\tinitializeGenomicElement(%d, %@, %@);\n}\n", genomicElementTypeID, startPosition, endPosition];
}

@end















































































