//
//  ScriptMod_AddPredeterminedMutation.m
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


#import "ScriptMod_AddPredeterminedMutation.h"

#include <string>
#include <map>
#include <vector>


@implementation ScriptMod_AddPredeterminedMutation

- (NSString *)sheetTitle
{
	return @"Add Predetermined Mutation";
}

- (NSString *)scriptSectionName
{
	return @"#PREDETERMINED MUTATIONS";
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
	[positionTextField setStringValue:@"1"];
	[self configureSubpopulationPopup:subpopPopUpButton];
	[numHomozygotesTextField setStringValue:@"0"];
	[numHeterozygotesTextField setStringValue:@"0"];
	[partialSweepCheckbox setState:NSOffState];
	[terminateFrequencyTextField setStringValue:@"0.5"];
	
	[super configSheetLoaded];
}

- (IBAction)validateControls:(id)sender
{
	// Determine whether we have valid inputs in all of our fields
	validInput = YES;
	
	BOOL generationValid = [ScriptMod validIntWithScientificNotationValueInTextField:generationTextField withMin:1 max:SLIM_MAX_GENERATION];
	validInput = validInput && generationValid;
	[generationTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:generationValid]];
	
	BOOL mutationTypeValid = [mutationTypePopUpButton isEnabled];
	validInput = validInput && mutationTypeValid;
	[mutationTypePopUpButton slimSetTintColor:(mutationTypeValid ? nil : [ScriptMod validationErrorFilterColor])];
	
	BOOL positionValid = [ScriptMod validIntWithScientificNotationValueInTextField:positionTextField withMin:1 max:SLIM_MAX_BASE_POSITION];
	validInput = validInput && positionValid;
	[positionTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:positionValid]];
	
	BOOL subpopValid = [subpopPopUpButton isEnabled];
	validInput = validInput && subpopValid;
	[subpopPopUpButton slimSetTintColor:(subpopValid ? nil : [ScriptMod validationErrorFilterColor])];
	
	int nHomozygotes = [numHomozygotesTextField intValue];
	int nHeterozygotes = [numHeterozygotesTextField intValue];
	BOOL bothZero = ((nHomozygotes == 0) && (nHeterozygotes == 0));
	
	BOOL nHomozygotesValid = [ScriptMod validIntValueInTextField:numHomozygotesTextField withMin:0 max:SLIM_MAX_SUBPOP_SIZE] && !bothZero;
	validInput = validInput && nHomozygotesValid;
	[numHomozygotesTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:nHomozygotesValid]];
	
	BOOL nHeterozygotesValid = [ScriptMod validIntValueInTextField:numHeterozygotesTextField withMin:0 max:SLIM_MAX_SUBPOP_SIZE] && !bothZero;
	validInput = validInput && nHeterozygotesValid;
	[numHeterozygotesTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:nHeterozygotesValid]];
	
	BOOL partialSweepEnabled = ([partialSweepCheckbox state] == NSOnState);
	
	[terminateFrequencyLabel setTextColor:[ScriptMod textColorForEnableState:partialSweepEnabled]];
	[terminateFrequencyTextField setEnabled:partialSweepEnabled];
	[terminateFrequencyTextField setBackgroundColor:[NSColor whiteColor]];
	
	if (partialSweepEnabled)
	{
		BOOL frequencyValid = [ScriptMod validFloatWithScientificNotationValueInTextField:terminateFrequencyTextField withMin:0.0 max:1.0];
		validInput = validInput && frequencyValid;
		[terminateFrequencyTextField setBackgroundColor:[ScriptMod backgroundColorForValidationState:frequencyValid]];
	}
	
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
	NSString *position = [positionTextField stringValue];
	int positionInt = (int)[position doubleValue];
	int subpopulationID = (int)[subpopPopUpButton selectedTag];
	int nHomozygotes = [numHomozygotesTextField intValue];
	int nHeterozygotes = [numHeterozygotesTextField intValue];
	BOOL partialSweep = ([partialSweepCheckbox state] == NSOnState);
	NSString *terminateFreq = [terminateFrequencyTextField stringValue];
	
	if (executeNow)
	{
		if (needsRecycle)
		{
			// queue a -recycle: operation to happen after we're done modifying the script
			[controller performSelector:@selector(recycle:) withObject:nil afterDelay:0.0];
		}
		else
		{
			// add it to the queue
			auto found_muttype_pair = controller->sim->mutation_types_.find(mutationTypeID);
			MutationType *mutation_type_ptr = found_muttype_pair->second;
			IntroducedMutation *new_introduced_mutation = new IntroducedMutation(mutation_type_ptr, positionInt, subpopulationID, targetGenerationInt, nHomozygotes, nHeterozygotes);
			
			controller->sim->introduced_mutations_.insert(std::pair<const int,IntroducedMutation*>(targetGenerationInt, new_introduced_mutation));
			
			if (partialSweep)
			{
				PartialSweep *new_partial_sweep = new PartialSweep(mutation_type_ptr, positionInt, atof([terminateFreq cStringUsingEncoding:NSASCIIStringEncoding]));
				
				controller->sim->partial_sweeps_.push_back(new_partial_sweep);
			}
		}
	}
	
	if (partialSweep)
		return [NSString stringWithFormat:@"%@ m%d %@ p%d %d %d P %@", targetGeneration, mutationTypeID, position, subpopulationID, nHomozygotes, nHeterozygotes, terminateFreq];
	else
		return [NSString stringWithFormat:@"%@ m%d %@ p%d %d %d", targetGeneration, mutationTypeID, position, subpopulationID, nHomozygotes, nHeterozygotes];
}

@end















































































