//
//  ScriptMod.m
//  SLiM
//
//  Created by Ben Haller on 3/20/15.
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

+ (BOOL)validIntValueInTextField:(NSTextField *)textfield withMin:(int)minValue max:(int)maxValue
{
	NSString *stringValue = [textfield stringValue];
	int intValue = [textfield intValue];
	
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

+ (BOOL)validIntWithScientificNotationValueInTextField:(NSTextField *)textfield withMin:(int)minValue max:(int)maxValue
{
	NSString *stringValue = [textfield stringValue];
	int intValue = (int)[textfield doubleValue];		// go through -doubleValue to get scientific notation
	
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
	ScriptMod *scriptMod = [[[self class] alloc] initWithController:windowController];
	
	[scriptMod loadConfigurationSheet];
	[scriptMod runConfigurationSheet];
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

- (void)insertScriptLine:(NSString *)lineToInsert targetGeneration:(int)targetGeneration
{
	NSRegularExpression *initializeBlockStartRegex = [NSRegularExpression regularExpressionWithPattern:@"^(s[0-9]+\\s+)?initialize\\s*\\(\\s*\\)" options:0 error:NULL];
	NSRegularExpression *eventBlockStartRegex = [NSRegularExpression regularExpressionWithPattern:@"^(?:s[0-9]+\\s+)?([0-9]+(\\.[0-9]*)?([eE][-+]?[0-9]+)?)" options:0 error:NULL];
	NSRegularExpression *commentLineRegex = [NSRegularExpression regularExpressionWithPattern:@"^\\s*//" options:0 error:NULL];
	
	// Break the script up into lines
	NSTextView *scriptTextView = controller->scriptTextView;
	NSMutableArray *scriptLines = [[[scriptTextView string] componentsSeparatedByString:@"\n"] mutableCopy];
	NSUInteger lineCount = [scriptLines count];
	
	// Search forward until we find the right place for this line
	NSUInteger bestInsertionIndex;
	int previousCommentLineCount = 0;	// we don't want to position after a comment; we use this to go just before a series of comment lines
	
	for (bestInsertionIndex = 0; bestInsertionIndex < lineCount; ++bestInsertionIndex)
	{
		NSString *line = [scriptLines objectAtIndex:bestInsertionIndex];
		int lineGeneration = 0;
		
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
					
					lineGeneration = (int)[genSubstring doubleValue];		// extracted generation
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
	[scriptTextView setString:[scriptLines componentsJoinedByString:@"\n"]];
	[controller textDidChange:[NSNotification notificationWithName:NSTextDidChangeNotification object:scriptTextView]];
	
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
			int targetGeneration = -1;
			
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
	int firstTag = -1;
	
	// Depopulate and populate the menu
	[button removeAllItems];
	
	if (![controller invalidSimulation])
	{
		Population &population = controller->sim->population_;
		
		for (auto popIter = population.begin(); popIter != population.end(); ++popIter)
		{
			int subpopID = popIter->first;
			//Subpopulation *subpop = popIter->second;
			NSString *subpopString = [NSString stringWithFormat:@"p%d", subpopID];
			
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

- (BOOL)isAvailableSubpopID:(int)subpopID
{
	if (![controller invalidSimulation])
	{
		Population &population = controller->sim->population_;
		
		if (population.find(subpopID) != population.end())
			return NO;
	}
	
	return YES;
}

- (int)bestAvailableSubpopID
{
	int firstUnusedID = 1;
	
	if (![controller invalidSimulation])
	{
		Population &population = controller->sim->population_;
		
		for (auto popIter = population.begin(); popIter != population.end(); ++popIter)
		{
			int subpopID = popIter->first;
			
			if (subpopID >= firstUnusedID)
				firstUnusedID = subpopID + 1;
		}
	}
	
	return firstUnusedID;
}

- (void)configureMutationTypePopup:(NSPopUpButton *)button addNoneItem:(BOOL)needsNoneItem;
{
	NSMenuItem *lastItem;
	int firstTag = -1;
	
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
		std::map<int,MutationType*> &mutationTypes = controller->sim->mutation_types_;
		
		for (auto muttypeIter = mutationTypes.begin(); muttypeIter != mutationTypes.end(); ++muttypeIter)
		{
			int muttypeID = muttypeIter->first;
			NSString *muttypeString = [NSString stringWithFormat:@"m%d", muttypeID];
			
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

- (BOOL)isAvailableMuttypeID:(int)muttypeID
{
	if (![controller invalidSimulation])
	{
		std::map<int,MutationType*> &mutationTypes = controller->sim->mutation_types_;
		
		if (mutationTypes.find(muttypeID) != mutationTypes.end())
			return NO;
	}
	
	return YES;
}

- (int)bestAvailableMuttypeID
{
	int firstUnusedID = 1;
	
	if (![controller invalidSimulation])
	{
		std::map<int,MutationType*> &mutationTypes = controller->sim->mutation_types_;
		
		for (auto muttypeIter = mutationTypes.begin(); muttypeIter != mutationTypes.end(); ++muttypeIter)
		{
			int subpopID = muttypeIter->first;
			
			if (subpopID >= firstUnusedID)
				firstUnusedID = subpopID + 1;
		}
	}
	
	return firstUnusedID;
}

- (void)configureGenomicElementTypePopup:(NSPopUpButton *)button
{
	NSMenuItem *lastItem;
	int firstTag = -1;
	
	// Depopulate and populate the menu
	[button removeAllItems];
	
	if (![controller invalidSimulation])
	{
		std::map<int,GenomicElementType*> &genomicElementTypes = controller->sim->genomic_element_types_;
		
		for (auto genomicElementTypeIter = genomicElementTypes.begin(); genomicElementTypeIter != genomicElementTypes.end(); ++genomicElementTypeIter)
		{
			int genomicElementTypeID = genomicElementTypeIter->first;
			NSString *genomicElementTypeString = [NSString stringWithFormat:@"g%d", genomicElementTypeID];
			
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

- (BOOL)isAvailableGenomicElementTypeID:(int)genomicElementTypeID
{
	if (![controller invalidSimulation])
	{
		std::map<int,GenomicElementType*> &genomicElementTypes = controller->sim->genomic_element_types_;
		
		if (genomicElementTypes.find(genomicElementTypeID) != genomicElementTypes.end())
			return NO;
	}
	
	return YES;
}

- (int)bestAvailableGenomicElementTypeID
{
	int firstUnusedID = 1;
	
	if (![controller invalidSimulation])
	{
		std::map<int,GenomicElementType*> &genomicElementTypes = controller->sim->genomic_element_types_;
		
		for (auto genomicElementTypeIter = genomicElementTypes.begin(); genomicElementTypeIter != genomicElementTypes.end(); ++genomicElementTypeIter)
		{
			int subpopID = genomicElementTypeIter->first;
			
			if (subpopID >= firstUnusedID)
				firstUnusedID = subpopID + 1;
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

- (NSString *)scriptLineWithExecute:(BOOL)executeNow targetGeneration:(int *)targetGenPtr
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















































































