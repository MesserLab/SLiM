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

+ (NSArray *)standardScriptSections
{
	static NSArray *sections = nil;
	
	if (sections == nil)
	{
		sections = [NSArray arrayWithObjects:
					@"#SEX",
					@"#MUTATION TYPES",
					@"#MUTATION RATE",
					@"#GENOMIC ELEMENT TYPES",
					@"#CHROMOSOME ORGANIZATION",
					@"#RECOMBINATION RATE",
					@"#GENERATIONS",
					@"#DEMOGRAPHY AND STRUCTURE",
					@"#OUTPUT",
					@"#GENE CONVERSION",
					@"#PREDETERMINED MUTATIONS",
					@"#SCRIPT",						// provisional SLiM 2.0 addition, but does no harm being here...
					@"#INITIALIZATION",
					@"#SEED",
					nil];
	}
	
	return sections;
}

+ (NSRegularExpression *)regexForInt
{
	static NSRegularExpression *regex = nil;
	
	if (!regex)
		regex = [[NSRegularExpression alloc] initWithPattern:@"^[0-9]+$" options:0 error:NULL];
	
	return regex;
}

+ (NSRegularExpression *)regexForScriptSectionHead
{
	static NSRegularExpression *regex = nil;
	
	if (!regex)
		regex = [[NSRegularExpression alloc] initWithPattern:@"^#[A-Z ]+" options:0 error:NULL];
	
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

- (void)insertScriptLine:(NSString *)lineToInsert
{
	NSString *sectionName = [self scriptSectionName];
	NSString *sortGrepPattern = [self sortingGrepPattern];
	NSRegularExpression *sortRegex = [NSRegularExpression regularExpressionWithPattern:sortGrepPattern options:0 error:NULL];
	NSTextView *scriptTextView = controller->scriptTextView;
	NSMutableArray *scriptLines = [[[scriptTextView string] componentsSeparatedByString:@"\n"] mutableCopy];
	
//	NSLog(@"insertScriptLine: sectionName == %@", sectionName);
//	NSLog(@"insertScriptLine: lineToInsert == %@", lineToInsert);
	
	// Find the lines corresponding to the section we need
	NSRegularExpression *scriptSectionRegex = [ScriptMod regexForScriptSectionHead];
	BOOL foundSection = NO;
	NSUInteger lastLineIndex = NSNotFound;
	NSUInteger firstLineIndex = [scriptLines indexOfObjectPassingTest:^BOOL(NSString *obj, NSUInteger idx, BOOL *stop) {
		return [obj hasPrefix:sectionName];
	}];
	
	if (firstLineIndex != NSNotFound)
	{
		foundSection = YES;
		
		NSRange indexRangeForSectionEndSearch = NSMakeRange(firstLineIndex + 1, [scriptLines count] - 1 - (firstLineIndex + 1));
		
		lastLineIndex = [scriptLines indexOfObjectAtIndexes:[NSIndexSet indexSetWithIndexesInRange:indexRangeForSectionEndSearch] options:0 passingTest:^BOOL(id obj, NSUInteger idx, BOOL *stop) {
			return ([scriptSectionRegex numberOfMatchesInString:obj options:0 range:NSMakeRange(0, [obj length])] > 0);
		}];
		
		if (lastLineIndex == NSNotFound)
			lastLineIndex = [scriptLines count] - 1;
		else
			lastLineIndex--;
	}
	
//	NSLog(@"insertScriptLine: point 1: firstLineIndex %d, lastLineIndex %d", (int)firstLineIndex, (int)lastLineIndex);
	
	// If we didn't find a section, find a place to insert it, and insert a new section
	if (!foundSection)
	{
		NSArray *sections = [ScriptMod standardScriptSections];
		NSUInteger sectionNameCount = [sections count];
		NSUInteger indexOfTargetSection = [sections indexOfObjectIdenticalTo:sectionName];
		
		if (indexOfTargetSection == NSNotFound)
		{
			NSLog(@"Unknown section name %@ from -scriptSectionName", sectionName);
			return;
		}
		
		NSUInteger nextSectionLineIndex = NSNotFound;
		
		for (NSUInteger nextSectionIndex = indexOfTargetSection + 1; nextSectionIndex < sectionNameCount; ++nextSectionIndex)
		{
			NSString *nextSectionName = [sections objectAtIndex:nextSectionIndex];
			
			nextSectionLineIndex = [scriptLines indexOfObjectPassingTest:^BOOL(NSString *obj, NSUInteger idx, BOOL *stop) {
				return [obj hasPrefix:nextSectionName];
			}];
			
			if (nextSectionLineIndex != NSNotFound)
				break;
		}
		
		if (nextSectionLineIndex == NSNotFound)
			nextSectionLineIndex = [scriptLines count];
		
		[scriptLines insertObject:sectionName atIndex:nextSectionLineIndex];
		[scriptLines insertObject:@"" atIndex:nextSectionLineIndex + 1];
		
		firstLineIndex = nextSectionLineIndex;
		lastLineIndex = nextSectionLineIndex + 1;
		
//		NSLog(@"insertScriptLine: point 2: firstLineIndex %d, lastLineIndex %d", (int)firstLineIndex, (int)lastLineIndex);
	}
	
//	for (NSUInteger debugIndex = firstLineIndex; debugIndex <= lastLineIndex; ++debugIndex)
//		NSLog(@"   line %d: %@", (int)debugIndex, [scriptLines objectAtIndex:debugIndex]);
	
	// OK, we now have a section delineated by line indices; find the right position for our new line
	NSUInteger insertionIndex = firstLineIndex + 1, bestInsertionIndex = firstLineIndex + 1;
	NSTextCheckingResult *insertionMatch = [sortRegex firstMatchInString:lineToInsert options:0 range:NSMakeRange(0, [lineToInsert length])];
	
	if (insertionMatch)
	{
		int insertionPriority = [[lineToInsert substringWithRange:[insertionMatch rangeAtIndex:1]] intValue];		// extracted sort priority
		
		for (insertionIndex = firstLineIndex + 1; insertionIndex <= lastLineIndex; ++insertionIndex)
		{
			NSString *line = [scriptLines objectAtIndex:insertionIndex];
			NSTextCheckingResult *lineMatch = [sortRegex firstMatchInString:line options:0 range:NSMakeRange(0, [line length])];
			
			if (lineMatch)
			{
				int linePriority = [[line substringWithRange:[lineMatch rangeAtIndex:1]] intValue];		// extracted sort priority
				
				if (linePriority <= insertionPriority)
					bestInsertionIndex = insertionIndex + 1;
				else
					break;
			}
		}
	}
	
	// And now that we have a position, we insert it
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
			
			if (returnCode == NSAlertFirstButtonReturn)
				scriptLine = [self scriptLineWithExecute:YES];
			else if (returnCode == NSAlertSecondButtonReturn)
				scriptLine = [self scriptLineWithExecute:NO];
			
			if (scriptLine)
				[self insertScriptLine:scriptLine];
			
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
	
	// If it is empty, disable it
	[button setEnabled:([button numberOfItems] >= 1)];
	
	// Fix the selection and then select the chosen subpopulation
	[button selectItemWithTag:firstTag];
	[button synchronizeTitleAndSelectedItem];
}

- (IBAction)validateControls:(id)sender
{
	// if the subclass says it does not need to recycle, hide the warning and set its height constraint to 0, and hide the recycle symbol
	if (validInput)
	{
		if (!needsRecycle && showingRecycleOption)
		{
			[_recycleWarning setHidden:YES];
			[_recycleImageTextField setHidden:YES];
			[_recycleWarningHeightConstraint setConstant:0.0];
			[_insertAndExecuteButton setTitle:@"Insert & Execute"];
			showingRecycleOption = NO;
		}
		else if (needsRecycle && !showingRecycleOption)
		{
			[_recycleWarning setHidden:NO];
			[_recycleImageTextField setHidden:NO];
			[_recycleWarningHeightConstraint setConstant:recycleWarningConstraintHeight];
			[_insertAndExecuteButton setTitle:@"Insert & Recycle"];
			showingRecycleOption = YES;
		}
	}
	
	// if the input is invalid, we need to disable our buttons
	[_insertOnlyButton setEnabled:validInput];
	[_insertAndExecuteButton setEnabled:validInput];
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

- (NSString *)scriptSectionName
{
	return @"#DEMOGRAPHY AND STRUCTURE";
}

- (NSString *)scriptLineWithExecute:(BOOL)executeNow
{
	NSLog(@"-[ScriptMod scriptLineWithExecute:] reached, indicates a subclass error");
	
	return nil;
}

- (NSString *)sortingGrepPattern
{
	return @"^[a-z]?([0-9]+) .*$";		// matches lines starting with things like 1500, p3, m2, and extracts the number
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















































































