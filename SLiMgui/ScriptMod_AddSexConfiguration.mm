//
//  ScriptMod_AddSexConfiguration.m
//  SLiM
//
//  Created by Ben Haller on 8/5/15.
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


#import "ScriptMod_AddSexConfiguration.h"

#include <string>
#include <map>
#include <vector>


@implementation ScriptMod_AddSexConfiguration

- (NSString *)sheetTitle
{
	return @"Add Sex Configuration";
}

- (void)configSheetLoaded
{
	// set initial control values
	[dominanceCoeffTextField setStringValue:@"1.0"];
	
	[super configSheetLoaded];
}

- (IBAction)validateControls:(id)sender
{
	// Determine whether we have valid inputs in all of our fields
	validInput = YES;
	
	GenomeType chromosomeType = (GenomeType)[chromosomeTypeMatrix selectedTag];
	
	// enable/disable the X dominance coefficient textfield and label
	[dominanceCoeffLabel setTextColor:[ScriptMod textColorForEnableState:(chromosomeType == GenomeType::kXChromosome)]];
	[dominanceCoeffTextField setEnabled:(chromosomeType == GenomeType::kXChromosome)];
	
	// set the X dominance coefficient textfield to a white background, and then validate it below; this way if it is not enabled it is always white
	[dominanceCoeffTextField setBackgroundColor:[NSColor whiteColor]];
	
	if (chromosomeType == GenomeType::kXChromosome)
	{
		BOOL dominanceCoeffValid = [ScriptMod validFloatValueInTextField:dominanceCoeffTextField withMin:-1000000000.0 max:1000000000.0];
		validInput = validInput && dominanceCoeffValid;
		[dominanceCoeffTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:dominanceCoeffValid]];
	}
	
	// determine whether we will need to recycle to simulation to make the change take effect
	needsRecycle = YES;
	
	// now we call super, and it uses validInput and needsRecycle to fix up the UI for us
	[super validateControls:sender];
}

- (NSString *)scriptLineWithExecute:(BOOL)executeNow targetGeneration:(slim_generation_t *)targetGenPtr
{
	GenomeType chromosomeType = (GenomeType)[chromosomeTypeMatrix selectedTag];
	
	if (executeNow)
	{
		// queue a -recycle: operation to happen after we're done modifying the script
		[controller performSelector:@selector(recycle:) withObject:nil afterDelay:0.0];
	}
	
	*targetGenPtr = 0;
	
	if (chromosomeType == GenomeType::kAutosome)
		return [NSString stringWithFormat:@"initialize() {\n\tinitializeSex(\"A\");\n}\n"];
	else if (chromosomeType == GenomeType::kXChromosome)
		return [NSString stringWithFormat:@"initialize() {\n\tinitializeSex(\"X\", %@);\n}\n", [dominanceCoeffTextField stringValue]];
	else if (chromosomeType == GenomeType::kYChromosome)
		return [NSString stringWithFormat:@"initialize() {\n\tinitializeSex(\"Y\");\n}\n"];
	
	return [NSString stringWithFormat:@"initialize() {\n\tinitializeSex(\"\");\n}\n"];
}

@end















































































