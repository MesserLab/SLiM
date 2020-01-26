//
//  ScriptMod.m
//  SLiM
//
//  Created by Ben Haller on 3/20/15.
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


#import "ScriptMod.h"


@implementation ScriptModSubclassViewPlaceholder

- (void)drawRect:(NSRect)dirtyRect
{
#if 0
	// can be enabled to allow the position of the custom view to be seen
	NSRect bounds = [self bounds];
	
	[[NSColor whiteColor] set];
	NSRectFill(bounds);
	
	[[NSColor blackColor] set];
	NSFrameRect(bounds);
#endif
}

@end


@implementation ScriptMod

+ (NSRegularExpression *)regexForInt
{
	static NSRegularExpression *regex = nil;
	
	if (!regex)
		regex = [[NSRegularExpression alloc] initWithPattern:@"^[0-9]+$" options:0 error:NULL];
	
	return regex;
}

+ (NSRegularExpression *)regexForIntWithScientificNotation
{
	static NSRegularExpression *regex = nil;
	
	if (!regex)
		regex = [[NSRegularExpression alloc] initWithPattern:@"^[0-9]+(e[0-9]+)?$" options:0 error:NULL];
	
	return regex;
}

+ (NSRegularExpression *)regexForFloat
{
	static NSRegularExpression *regex = nil;
	
	if (!regex)
		regex = [[NSRegularExpression alloc] initWithPattern:@"^\\-?[0-9]+(\\.[0-9]*)?$" options:0 error:NULL];
	
	return regex;
}

+ (NSRegularExpression *)regexForFloatWithScientificNotation
{
	static NSRegularExpression *regex = nil;
	
	if (!regex)
		regex = [[NSRegularExpression alloc] initWithPattern:@"^\\-?[0-9]+(\\.[0-9]*)?(e\\-?[0-9]+)?$" options:0 error:NULL];
	
	return regex;
}

+ (NSRegularExpression *)regexForFilename
{
	static NSRegularExpression *regex = nil;
	
	if (!regex)
		regex = [[NSRegularExpression alloc] initWithPattern:@"^[^\0]+$" options:0 error:NULL];		// Unix allows pretty much anything
	
	return regex;
}

+ (BOOL)validIntValueInTextField:(NSTextField *)textfield withMin:(int64_t)minValue max:(int64_t)maxValue
{
	NSString *stringValue = [textfield stringValue];
	int64_t intValue = [[textfield stringValue] longLongValue];
	
	if ([stringValue length] == 0)
		return NO;
	
	if ([[ScriptMod regexForInt] numberOfMatchesInString:stringValue options:0 range:NSMakeRange(0, [stringValue length])] == 0)
		return NO;
	
	if (intValue < minValue)
		return NO;
	
	if (intValue > maxValue)
		return NO;
	
	return YES;
}

+ (BOOL)validIntWithScientificNotationValueInTextField:(NSTextField *)textfield withMin:(int64_t)minValue max:(int64_t)maxValue
{
	NSString *stringValue = [textfield stringValue];
	int64_t intValue = (int64_t)[textfield doubleValue];		// go through -doubleValue to get scientific notation
	
	if ([stringValue length] == 0)
		return NO;
	
	if ([[ScriptMod regexForIntWithScientificNotation] numberOfMatchesInString:stringValue options:0 range:NSMakeRange(0, [stringValue length])] == 0)
		return NO;
	
	if (intValue < minValue)
		return NO;
	
	if (intValue > maxValue)
		return NO;
	
	return YES;
}

+ (BOOL)validFloatValueInTextField:(NSTextField *)textfield withMin:(double)minValue max:(double)maxValue
{
	return [self validFloatValueInTextField:textfield withMin:minValue max:maxValue excludingMin:NO excludingMax:NO];
}

+ (BOOL)validFloatValueInTextField:(NSTextField *)textfield withMin:(double)minValue max:(double)maxValue excludingMin:(BOOL)excludeMin excludingMax:(BOOL)excludeMax
{
	NSString *stringValue = [textfield stringValue];
	double doubleValue = [textfield doubleValue];
	
	if ([stringValue length] == 0)
		return NO;
	
	if ([[ScriptMod regexForFloat] numberOfMatchesInString:stringValue options:0 range:NSMakeRange(0, [stringValue length])] == 0)
		return NO;
	
	if (doubleValue < minValue)
		return NO;
	
	if (excludeMin && (doubleValue == minValue))
		return NO;
	
	if (doubleValue > maxValue)
		return NO;
	
	if (excludeMax && (doubleValue == maxValue))
		return NO;
	
	return YES;
}

+ (BOOL)validFloatWithScientificNotationValueInTextField:(NSTextField *)textfield withMin:(double)minValue max:(double)maxValue
{
	return [self validFloatWithScientificNotationValueInTextField:textfield withMin:minValue max:maxValue excludingMin:NO excludingMax:NO];
}

+ (BOOL)validFloatWithScientificNotationValueInTextField:(NSTextField *)textfield withMin:(double)minValue max:(double)maxValue excludingMin:(BOOL)excludeMin excludingMax:(BOOL)excludeMax
{
	NSString *stringValue = [textfield stringValue];
	double doubleValue = [textfield doubleValue];
	
	if ([stringValue length] == 0)
		return NO;
	
	if ([[ScriptMod regexForFloatWithScientificNotation] numberOfMatchesInString:stringValue options:0 range:NSMakeRange(0, [stringValue length])] == 0)
		return NO;
	
	if (doubleValue < minValue)
		return NO;
	
	if (excludeMin && (doubleValue == minValue))
		return NO;
	
	if (doubleValue > maxValue)
		return NO;
	
	if (excludeMax && (doubleValue == maxValue))
		return NO;
	
	return YES;
}

+ (BOOL)validFilenameInTextField:(NSTextField *)textfield
{
	NSString *stringValue = [textfield stringValue];
	
	if ([stringValue length] == 0)
		return NO;
	
	if ([[ScriptMod regexForFilename] numberOfMatchesInString:stringValue options:0 range:NSMakeRange(0, [stringValue length])] == 0)
		return NO;
	
	return YES;
}

+ (NSColor *)validationErrorColor
{
	static NSColor *color = nil;
	
	if (!color)
		color = [[NSColor colorWithCalibratedHue:0.0 saturation:0.15 brightness:1.0 alpha:1.0] retain];
	
	return color;
}

+ (NSColor *)validationErrorFilterColor
{
	static NSColor *color = nil;
	
	if (!color)
		color = [[NSColor colorWithCalibratedHue:0.0 saturation:0.4 brightness:1.0 alpha:1.0] retain];
	
	return color;
}

+ (NSColor *)textColorForEnableState:(BOOL)enabled
{
	return (enabled ? [NSColor blackColor] : [NSColor grayColor]);
}

+ (NSColor *)backgroundColorForValidationState:(BOOL)valid
{
	return (valid ? [NSColor whiteColor] : [ScriptMod validationErrorColor]);
}

+ (void)runWithController:(SLiMWindowController *)windowController
{
	[[windowController document] setTransient:NO]; // Since the user has taken an interest in the window, clear the document's transient status
	
	ScriptMod *scriptMod = [[[self class] alloc] initWithController:windowController];
	
	if ([scriptMod checkEligibility])
	{
		[scriptMod loadConfigurationSheet];
		[scriptMod runConfigurationSheet];
	}
	
	[scriptMod autorelease];
}

- (instancetype)initWithController:(SLiMWindowController *)windowController
{
	if (self = [super init])
	{
		controller = windowController;
		needsRecycle = YES;				// default assumes a recycle is needed
	}
	
	return self;
}

- (void)dealloc
{
	//NSLog(@"ScriptMod dealloc");
	
	[_scriptModSheet release];
	_scriptModSheet = nil;
	
	[_sheetTitleTextField release];
	_sheetTitleTextField = nil;
	
	[_recycleWarning release];
	_recycleWarning = nil;
	
	[_recycleWarningHeightConstraint release];
	_recycleWarningHeightConstraint = nil;
	
	[_recycleImageTextField release];
	_recycleImageTextField = nil;
	
	[_insertOnlyButton release];
	_insertOnlyButton = nil;
	
	[_insertAndExecuteButton release];
	_insertAndExecuteButton = nil;
	
	[_customViewPlaceholder release];
	_customViewPlaceholder = nil;
	
	[_customViewFromSubclass release];
	_customViewFromSubclass = nil;
	
	controller = nil;
	
	[super dealloc];
}

- (BOOL)checkEligibility
{
	return YES;
}

- (BOOL)checkSubpopsDefined
{
	if (controller->sim->population_.subpops_.size() == 0)
	{
		NSAlert *alert = [[NSAlert alloc] init];
		
		[alert setAlertStyle:NSCriticalAlertStyle];
		[alert setMessageText:@"No subpopulations defined"];
		[alert setInformativeText:@"No subpopulations are currently defined, so there is nothing for this operation to operate upon.  (Probably you need to step forward to a generation in which subpopulations have been defined.)"];
		[alert addButtonWithTitle:@"OK"];
		
		[alert beginSheetModalForWindow:[controller window] completionHandler:^(NSModalResponse returnCode) { [alert autorelease]; }];
		
		return NO;
	}
	
	return YES;
}

- (BOOL)checkMutationTypesDefined
{
	if (controller->sim->mutation_types_.size() == 0)
	{
		NSAlert *alert = [[NSAlert alloc] init];
		
		[alert setAlertStyle:NSCriticalAlertStyle];
		[alert setMessageText:@"No mutation types defined"];
		[alert setInformativeText:@"No mutation types are currently defined, so there is nothing for this operation to operate upon.  (Probably you need to step forward to a generation in which mutation types have been defined.)"];
		[alert addButtonWithTitle:@"OK"];
		
		[alert beginSheetModalForWindow:[controller window] completionHandler:^(NSModalResponse returnCode) { [alert autorelease]; }];
		
		return NO;
	}
	
	return YES;
}

- (BOOL)checkGenomicElementTypesDefined
{
	if (controller->sim->genomic_element_types_.size() == 0)
	{
		NSAlert *alert = [[NSAlert alloc] init];
		
		[alert setAlertStyle:NSCriticalAlertStyle];
		[alert setMessageText:@"No genomic element types defined"];
		[alert setInformativeText:@"No genomic element types are currently defined, so there is nothing for this operation to operate upon.  (Probably you need to step forward to a generation in which genomic element types have been defined.)"];
		[alert addButtonWithTitle:@"OK"];
		
		[alert beginSheetModalForWindow:[controller window] completionHandler:^(NSModalResponse returnCode) { [alert autorelease]; }];
		
		return NO;
	}
	
	return YES;
}

- (void)swapSubclassViewForPlaceholder
{
	if (_customViewFromSubclass && _customViewPlaceholder)
	{
		NSView *containingView = [_customViewPlaceholder superview];
		
		// turn off autoresizing and set up an initial frame
		[_customViewFromSubclass setTranslatesAutoresizingMaskIntoConstraints:NO];
		[_customViewFromSubclass setFrame:[_customViewPlaceholder frame]];
		
		// add the subclass view to the view hierarchy
		[containingView addSubview:_customViewFromSubclass];
		
		// then replicate all constraints in the superview that apply to the placeholder, redirecting them to the subclass custom view
		NSMutableArray *constraints = [NSMutableArray array];
		
		for (NSLayoutConstraint *constraint in [containingView constraints])
		{
			id firstItem = [constraint firstItem];
			id secondItem = [constraint secondItem];
			
			if (firstItem == _customViewPlaceholder)
				firstItem = _customViewFromSubclass;
			else if (secondItem == _customViewPlaceholder)
				secondItem = _customViewFromSubclass;
			else
				continue;
			
			[constraints addObject:[NSLayoutConstraint constraintWithItem:firstItem attribute:constraint.firstAttribute relatedBy:constraint.relation toItem:secondItem attribute:constraint.secondAttribute multiplier:constraint.multiplier constant:constraint.constant]];
		}
		
		[containingView addConstraints:constraints];
		
		// remove the placeholder
		[_customViewPlaceholder removeFromSuperview];
		[_customViewPlaceholder release];
		_customViewPlaceholder = nil;
	}
}

- (void)loadConfigurationSheet
{
	// load the sheet nib
	[[NSBundle mainBundle] loadNibNamed:@"ScriptModSheet" owner:self topLevelObjects:NULL];
	
	if (_scriptModSheet)
	{
		// load the subclass view nib
		NSString *nibName = [self nibName];
		
		[[NSBundle mainBundle] loadNibNamed:nibName owner:self topLevelObjects:NULL];
		
		if (_customViewFromSubclass)
		{
			// set the sheet title
			[_sheetTitleTextField setStringValue:[self sheetTitle]];
			
			// install the subclass configuration view
			[self swapSubclassViewForPlaceholder];
			
			// initially out of the nib, we are showing the recycle option
			showingRecycleOption = YES;
			recycleWarningConstraintHeight = [_recycleWarningHeightConstraint constant];
			
			// now notify our subclass that the sheet is loaded, and validate controls for display
			[self configSheetLoaded];
			[self validateControls:nil];
		}
		else
		{
			NSLog(@"-loadConfigurationSheet failed to load subclass view nib %@", nibName);
			NSBeep();
		}
	}
	else
	{
		NSLog(@"-loadConfigurationSheet failed to load nib ScriptModSheet");
		NSBeep();
	}
}

- (void)insertScriptLine:(NSString *)lineToInsert targetGeneration:(slim_generation_t)targetGeneration
{
	NSRegularExpression *initializeBlockStartRegex = [NSRegularExpression regularExpressionWithPattern:@"^(s[0-9]+\\s+)?initialize\\s*\\(\\s*\\)" options:0 error:NULL];
	NSRegularExpression *eventBlockStartRegex = [NSRegularExpression regularExpressionWithPattern:@"^(?:s[0-9]+\\s+)?([0-9]+(\\.[0-9]*)?([eE][-+]?[0-9]+)?)" options:0 error:NULL];
	NSRegularExpression *commentLineRegex = [NSRegularExpression regularExpressionWithPattern:@"^\\s*//" options:0 error:NULL];
	
	// Break the script up into lines
	NSTextView *scriptTextView = controller->scriptTextView;
	NSMutableArray *scriptLines = [[[[scriptTextView string] componentsSeparatedByString:@"\n"] mutableCopy] autorelease];
	NSUInteger lineCount = [scriptLines count];
	
	// Search forward until we find the right place for this line
	NSUInteger bestInsertionIndex;
	NSUInteger previousCommentLineCount = 0;	// we don't want to position after a comment; we use this to go just before a series of comment lines
	
	for (bestInsertionIndex = 0; bestInsertionIndex < lineCount; ++bestInsertionIndex)
	{
		NSString *line = [scriptLines objectAtIndex:bestInsertionIndex];
		slim_generation_t lineGeneration = 0;
		
		//NSLog(@"processing line: %@", line);
		
		// if the line is a comment line, increment our counter; if we hit a match right after this, we want to backtrack over comment lines
		if ([commentLineRegex numberOfMatchesInString:line options:0 range:NSMakeRange(0, [line length])])
		{
			previousCommentLineCount++;
			continue;
		}
		else
		{
			if ([initializeBlockStartRegex numberOfMatchesInString:line options:0 range:NSMakeRange(0, [line length])])
			{
				// looks like an initialize() line; that is always generation 0
				lineGeneration = 0;
			}
			else if ([eventBlockStartRegex numberOfMatchesInString:line options:0 range:NSMakeRange(0, [line length])])
			{
				// looks like a script event/callback block of some sort; extract the generation from it
				NSTextCheckingResult *lineMatch = [eventBlockStartRegex firstMatchInString:line options:0 range:NSMakeRange(0, [line length])];
				
				if (lineMatch)
				{
					NSString *genSubstring = [line substringWithRange:[lineMatch rangeAtIndex:1]];
					
					//NSLog(@"genSubstring == \"%@\"", genSubstring);
					
					lineGeneration = SLiMClampToGenerationType((int64_t)[genSubstring doubleValue]);		// extracted generation
				}
			}
			
			// If we have reached the start of a block with a later generation than what we're inserting, we want to stop and insert it right there
			if (lineGeneration > targetGeneration)
				break;
			
			previousCommentLineCount = 0;
		}
	}
	
	// Insert the line, after backtracking over comments preceding our insertion index
	if (previousCommentLineCount && (bestInsertionIndex > previousCommentLineCount))
		bestInsertionIndex -= previousCommentLineCount;
	
	[scriptLines insertObject:lineToInsert atIndex:bestInsertionIndex];
	
	// Set the doctored string back into the textview
	NSUInteger tsLength = [[scriptTextView textStorage] length];
	NSString *replacementString = [scriptLines componentsJoinedByString:@"\n"];
	
	[scriptTextView setSelectedRange:NSMakeRange(0, tsLength)];
	[scriptTextView insertText:replacementString replacementRange:NSMakeRange(tsLength, [replacementString length])];	// insertText: was deprecated in 10.11 so I switched to this; BCH 13 Nov. 2017
	
	// Select the inserted line
	NSUInteger lineStartCharacter = 0;
	
	for (NSUInteger previousLineIndex = 0; previousLineIndex < bestInsertionIndex; ++previousLineIndex)
		lineStartCharacter += [[scriptLines objectAtIndex:previousLineIndex] length] + 1;
	
	NSRange rangeToSelect = NSMakeRange(lineStartCharacter, [lineToInsert length]);
	
	[scriptTextView setSelectedRange:rangeToSelect];
	
	// Scroll the selection to visible and animate the find
	[scriptTextView scrollRangeToVisible:rangeToSelect];
	[scriptTextView showFindIndicatorForRange:rangeToSelect];
}

- (void)runConfigurationSheet
{
	if (_scriptModSheet)
	{
		NSWindow *window = [controller window];

		[self retain];	// we retain ourselves when we start our configuration sheet run, and release ourselves when the sheet finishes
		
		// run the sheet
		[window beginSheet:_scriptModSheet completionHandler:^(NSModalResponse returnCode) {
			NSString *scriptLine = nil;
			slim_generation_t targetGeneration = -1;
			
			if (returnCode == NSAlertFirstButtonReturn)
				scriptLine = [self scriptLineWithExecute:YES targetGeneration:&targetGeneration];
			else if (returnCode == NSAlertSecondButtonReturn)
				scriptLine = [self scriptLineWithExecute:NO targetGeneration:&targetGeneration];
			
			if (scriptLine)
				[self insertScriptLine:scriptLine targetGeneration:targetGeneration];
			
			[_scriptModSheet autorelease];
			_scriptModSheet = nil;
			
			// release the retain we took at the top
			[self autorelease];
		}];
	}
}

- (IBAction)configureSheetInsertExecute:(id)sender
{
	NSWindow *window = [controller window];
	
	[window endSheet:_scriptModSheet returnCode:NSAlertFirstButtonReturn];
}

- (IBAction)configureSheetInsert:(id)sender
{
	NSWindow *window = [controller window];
	
	[window endSheet:_scriptModSheet returnCode:NSAlertSecondButtonReturn];
}

- (IBAction)configureSheetCancel:(id)sender
{
	NSWindow *window = [controller window];
	
	[window endSheet:_scriptModSheet returnCode:NSAlertThirdButtonReturn];
}


// These methods are the points where the subclass plugs in to the configuration panel run

- (void)configSheetLoaded
{
}

- (void)configureSubpopulationPopup:(NSPopUpButton *)button
{
	NSMenuItem *lastItem;
	slim_objectid_t firstTag = -1;
	
	// Depopulate and populate the menu
	[button removeAllItems];
	
	if (![controller invalidSimulation])
	{
		Population &population = controller->sim->population_;
		
		for (auto popIter = population.subpops_.begin(); popIter != population.subpops_.end(); ++popIter)
		{
			slim_objectid_t subpopID = popIter->first;
			//Subpopulation *subpop = popIter->second;
			NSString *subpopString = [NSString stringWithFormat:@"p%lld", (int64_t)subpopID];
			
			[button addItemWithTitle:subpopString];
			lastItem = [button lastItem];
			[lastItem setTag:subpopID];
			
			// Remember the first item we add; we will use this item's tag to make a selection if needed
			if (firstTag == -1)
				firstTag = subpopID;
		}
	}
	
	[button slimSortMenuItemsByTag];
	
	// If it is empty, add an explanatory item and disable it
	BOOL enabled = ([button numberOfItems] >= 1);
	
	[button setEnabled:enabled];
	if (!enabled)
		[button addItemWithTitle:@"<none>"];
	
	// Fix the selection and then select the chosen subpopulation
	[button selectItemWithTag:firstTag];
	[button synchronizeTitleAndSelectedItem];
}

- (BOOL)isAvailableSubpopID:(slim_objectid_t)subpopID
{
	if (![controller invalidSimulation])
	{
		Population &population = controller->sim->population_;
		
		if (population.subpops_.find(subpopID) != population.subpops_.end())
			return NO;
	}
	
	return YES;
}

- (slim_objectid_t)bestAvailableSubpopID
{
	slim_objectid_t firstUnusedID = 1;
	
	if (![controller invalidSimulation])
	{
		Population &population = controller->sim->population_;
		
		for (auto popIter = population.subpops_.begin(); popIter != population.subpops_.end(); ++popIter)
		{
			slim_objectid_t subpopID = popIter->first;
			
			if (subpopID >= firstUnusedID)
				firstUnusedID = subpopID + 1;
		}
	}
	
	return firstUnusedID;
}

- (void)configureMutationTypePopup:(NSPopUpButton *)button addNoneItem:(BOOL)needsNoneItem;
{
	NSMenuItem *lastItem;
	slim_objectid_t firstTag = -1;
	
	// Depopulate and populate the menu
	[button removeAllItems];
	
	if (needsNoneItem)
	{
		[button addItemWithTitle:@"<none>"];
		lastItem = [button lastItem];
		[lastItem setTag:-1];
	}
	
	if (![controller invalidSimulation])
	{
		std::map<slim_objectid_t,MutationType*> &mutationTypes = controller->sim->mutation_types_;
		
		for (auto muttypeIter = mutationTypes.begin(); muttypeIter != mutationTypes.end(); ++muttypeIter)
		{
			slim_objectid_t muttypeID = muttypeIter->first;
			NSString *muttypeString = [NSString stringWithFormat:@"m%lld", (int64_t)muttypeID];
			
			[button addItemWithTitle:muttypeString];
			lastItem = [button lastItem];
			[lastItem setTag:muttypeID];
			
			// Remember the first item we add; we will use this item's tag to make a selection if needed
			if (!needsNoneItem && (firstTag == -1))
				firstTag = muttypeID;
		}
	}
	
	// If it is empty, add an explanatory item and disable it
	BOOL enabled = ([button numberOfItems] >= 1);
	
	[button setEnabled:enabled];
	if (!enabled)
		[button addItemWithTitle:@"<none>"];
	
	// Fix the selection and then select the chosen subpopulation
	[button selectItemWithTag:firstTag];
	[button synchronizeTitleAndSelectedItem];
}

- (void)configureMutationTypePopup:(NSPopUpButton *)button
{
	[self configureMutationTypePopup:button addNoneItem:NO];
}

- (BOOL)isAvailableMuttypeID:(slim_objectid_t)muttypeID
{
	if (![controller invalidSimulation])
	{
		std::map<slim_objectid_t,MutationType*> &mutationTypes = controller->sim->mutation_types_;
		
		if (mutationTypes.find(muttypeID) != mutationTypes.end())
			return NO;
	}
	
	return YES;
}

- (slim_objectid_t)bestAvailableMuttypeID
{
	slim_objectid_t firstUnusedID = 1;
	
	if (![controller invalidSimulation])
	{
		std::map<slim_objectid_t,MutationType*> &mutationTypes = controller->sim->mutation_types_;
		
		for (auto muttypeIter = mutationTypes.begin(); muttypeIter != mutationTypes.end(); ++muttypeIter)
		{
			slim_objectid_t muttypeID = muttypeIter->first;
			
			if (muttypeID >= firstUnusedID)
				firstUnusedID = muttypeID + 1;
		}
	}
	
	return firstUnusedID;
}

- (void)configureGenomicElementTypePopup:(NSPopUpButton *)button
{
	NSMenuItem *lastItem;
	slim_objectid_t firstTag = -1;
	
	// Depopulate and populate the menu
	[button removeAllItems];
	
	if (![controller invalidSimulation])
	{
		std::map<slim_objectid_t,GenomicElementType*> &genomicElementTypes = controller->sim->genomic_element_types_;
		
		for (auto genomicElementTypeIter = genomicElementTypes.begin(); genomicElementTypeIter != genomicElementTypes.end(); ++genomicElementTypeIter)
		{
			slim_objectid_t genomicElementTypeID = genomicElementTypeIter->first;
			NSString *genomicElementTypeString = [NSString stringWithFormat:@"g%lld", (int64_t)genomicElementTypeID];
			
			[button addItemWithTitle:genomicElementTypeString];
			lastItem = [button lastItem];
			[lastItem setTag:genomicElementTypeID];
			
			// Remember the first item we add; we will use this item's tag to make a selection if needed
			if (firstTag == -1)
				firstTag = genomicElementTypeID;
		}
	}
	
	// If it is empty, add an explanatory item and disable it
	BOOL enabled = ([button numberOfItems] >= 1);
	
	[button setEnabled:enabled];
	if (!enabled)
		[button addItemWithTitle:@"<none>"];
	
	// Fix the selection and then select the chosen subpopulation
	[button selectItemWithTag:firstTag];
	[button synchronizeTitleAndSelectedItem];
}

- (BOOL)isAvailableGenomicElementTypeID:(slim_objectid_t)genomicElementTypeID
{
	if (![controller invalidSimulation])
	{
		std::map<slim_objectid_t,GenomicElementType*> &genomicElementTypes = controller->sim->genomic_element_types_;
		
		if (genomicElementTypes.find(genomicElementTypeID) != genomicElementTypes.end())
			return NO;
	}
	
	return YES;
}

- (slim_objectid_t)bestAvailableGenomicElementTypeID
{
	slim_objectid_t firstUnusedID = 1;
	
	if (![controller invalidSimulation])
	{
		std::map<slim_objectid_t,GenomicElementType*> &genomicElementTypes = controller->sim->genomic_element_types_;
		
		for (auto genomicElementTypeIter = genomicElementTypes.begin(); genomicElementTypeIter != genomicElementTypes.end(); ++genomicElementTypeIter)
		{
			slim_objectid_t geltypeID = genomicElementTypeIter->first;
			
			if (geltypeID >= firstUnusedID)
				firstUnusedID = geltypeID + 1;
		}
	}
	
	return firstUnusedID;
}

- (IBAction)validateControls:(id)sender
{
	// if the subclass says it does not need to recycle, hide the warning and set its height constraint to 0, and hide the recycle symbol
	// we animate these changes, since otherwise the change is rather large and abrupt
	BOOL animateChanges = [_scriptModSheet isVisible];
	
	if (validInput || !animateChanges)	// !animateChanges allows the recycle option to adjust optimally before the sheet is visible
	{
		if (!needsRecycle && showingRecycleOption)
		{
			if (animateChanges)
			{
				[NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
					[context setDuration:0.2];
					//[context setAllowsImplicitAnimation:YES];
					
					// animation code
					[_recycleWarning.animator setHidden:YES];
					[_recycleImageTextField.animator setHidden:YES];
					[_recycleWarningHeightConstraint.animator setConstant:0.0];
					[_insertAndExecuteButton.animator setTitle:@"Insert & Execute"];
				} completionHandler:^{
					// animation completed
				}];
			}
			else
			{
				[_recycleWarning setHidden:YES];
				[_recycleImageTextField setHidden:YES];
				[_recycleWarningHeightConstraint setConstant:0.0];
				[_insertAndExecuteButton setTitle:@"Insert & Execute"];
			}
			
			showingRecycleOption = NO;
		}
		else if (needsRecycle && !showingRecycleOption)
		{
			if (animateChanges)
			{
				[NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
					[context setDuration:0.2];
					//[context setAllowsImplicitAnimation:YES];
					
					// animation code
					[_recycleWarning.animator setHidden:NO];
					[_recycleImageTextField.animator setHidden:NO];
					[_recycleWarningHeightConstraint.animator setConstant:recycleWarningConstraintHeight];
					[_insertAndExecuteButton.animator setTitle:@"Insert & Recycle"];
				} completionHandler:^{
					// animation completed
				}];
			}
			else
			{
				[_recycleWarning setHidden:NO];
				[_recycleImageTextField setHidden:NO];
				[_recycleWarningHeightConstraint setConstant:recycleWarningConstraintHeight];
				[_insertAndExecuteButton setTitle:@"Insert & Recycle"];
			}
			
			showingRecycleOption = YES;
		}
	}
	
	// if the input is invalid, we need to disable our buttons
	[_insertOnlyButton setEnabled:validInput];
	[_insertAndExecuteButton setEnabled:validInput];
	[_recycleImageTextField setAlphaValue:validInput ? 1.0 : 0.4];
}


// These methods can be overridden by subclasses to provide information to the superclass

- (NSString *)sheetTitle
{
	return @"<subclass override>";
}

- (NSString *)nibName
{
	return [self className];
}

- (NSString *)scriptLineWithExecute:(BOOL)executeNow targetGeneration:(slim_generation_t *)targetGenPtr
{
	NSLog(@"-[ScriptMod scriptLineWithExecute:targetGeneration:] reached, indicates a subclass error");
	
	return nil;
}

//
//	NSTextField delegate methods
//
#pragma mark NSTextField delegate

- (void)controlTextDidChange:(NSNotification *)notification
{
	[self validateControls:nil];
}

@end















































































