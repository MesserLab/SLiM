//
//  FindRecipeController.m
//  SLiMgui
//
//  Created by Ben Haller on 10/11/18.
//  Copyright (c) 2018-2020 Philipp Messer.  All rights reserved.
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


#import "FindRecipeController.h"
#import "SLiMDocumentController.h"
#import "AppDelegate.h"


@implementation FindRecipeController

+ (void)runFindRecipesPanel
{
	FindRecipeController *controller = [[FindRecipeController alloc] initWithWindowNibName:@"FindRecipePanel"];
	
	[[NSApplication sharedApplication] runModalForWindow:[controller window]];
	
	[[controller window] close];
	[controller release];
}

- (void)loadRecipes
{
	NSURL *urlForRecipesFolder = [[NSBundle mainBundle] URLForResource:@"Recipes" withExtension:@""];
	NSFileManager *fm = [NSFileManager defaultManager];
	NSDirectoryEnumerator *dirEnum = [fm enumeratorAtURL:urlForRecipesFolder includingPropertiesForKeys:@[NSURLNameKey, NSURLIsDirectoryKey] options:(NSDirectoryEnumerationSkipsHiddenFiles | NSDirectoryEnumerationSkipsSubdirectoryDescendants) errorHandler:nil];
	NSMutableArray *filenames = [NSMutableArray array];
	
	for (NSURL *fileURL in dirEnum)
	{
		NSNumber *isDirectory = nil;
		
		[fileURL getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];
		
		if (![isDirectory boolValue])
		{
			NSString *name = nil;
			
			[fileURL getResourceValue:&name forKey:NSURLNameKey error:nil];
			
			if ([name hasPrefix:@"Recipe "])
				[filenames addObject:name];
		}
	}
	
	[filenames sortUsingComparator:^NSComparisonResult(id obj1, id obj2) {
		return [(NSString *)obj1 compare:(NSString *)obj2 options:NSNumericSearch];
	}];
	
	recipeFilenames = [filenames retain];
	matchRecipeFilenames = [recipeFilenames retain];
	
	// Look up recipe file contents and cache them
	NSMutableArray *contents = [NSMutableArray array];
	
	for (NSString *filename : recipeFilenames)
	{
		NSString *filePath = [[urlForRecipesFolder path] stringByAppendingPathComponent:filename];
		NSString *fileContents = [NSString stringWithContentsOfFile:filePath usedEncoding:nil error:nil];
		
		[contents addObject:fileContents];
	}
	
	recipeContents = [contents retain];
}

- (NSString *)displayStringForRecipeFilename:(NSString *)name
{
	if ([name hasSuffix:@".txt"])
	{
		// Remove the .txt extension for SLiM models
		name = [name substringWithRange:NSMakeRange(7, [name length] - 11)];
	}
	else if ([name hasSuffix:@".py"])
	{
		// Leave the .py extension for Python models
		name = [name substringWithRange:NSMakeRange(7, [name length] - 7)];
		name = [name stringByAppendingString:@" 🐍"];
	}
	
	return name;
}

- (BOOL)recipeIndex:(int)recipeIndex matchesKeyword:(NSString *)keyword
{
	// an empty keyword matches all recipes
	if (!keyword || ([keyword length] == 0))
		return YES;
	
	// look for a match in the filename
	NSString *filename = [recipeFilenames objectAtIndex:recipeIndex];
	
	if ([filename rangeOfString:keyword options:NSCaseInsensitiveSearch | NSDiacriticInsensitiveSearch].location != NSNotFound)
		return YES;
	
	// look for a match in the file contents
	NSString *contents = [recipeContents objectAtIndex:recipeIndex];
	
	if ([contents rangeOfString:keyword options:NSCaseInsensitiveSearch | NSDiacriticInsensitiveSearch].location != NSNotFound)
		return YES;
	
	return NO;
}

- (void)constructMatchList
{
	NSMutableArray *matches = [NSMutableArray array];
	NSString *keyword1 = [keyword1TextField stringValue];
	NSString *keyword2 = [keyword2TextField stringValue];
	NSString *keyword3 = [keyword3TextField stringValue];
	
	for (int i = 0; i < (int)[recipeFilenames count]; ++i)
	{
		if ([self recipeIndex:i matchesKeyword:keyword1] && [self recipeIndex:i matchesKeyword:keyword2] && [self recipeIndex:i matchesKeyword:keyword3])
			[matches addObject:[recipeFilenames objectAtIndex:i]];
	}
	
	[matchRecipeFilenames release];
	matchRecipeFilenames = [matches retain];
}

- (void)dealloc
{
	[recipeFilenames release];
	recipeFilenames = nil;
	
	[recipeContents release];
	recipeContents = nil;
	
	[matchRecipeFilenames release];
	matchRecipeFilenames = nil;
	
	[super dealloc];
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    
	[self loadRecipes];
	[matchTableView reloadData];
	[matchTableView setNeedsDisplay];
	
	[self validateOK];
	
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	[scriptPreview setSyntaxColoring:[defaults boolForKey:defaultsSyntaxHighlightScriptKey] ? kEidosSyntaxColoringEidos : kEidosSyntaxColoringNone];
	[scriptPreview setDisplayFontSize:10];
}

- (void)validateOK
{
	[buttonOK setEnabled:([matchTableView numberOfSelectedRows] > 0)];
}

- (void)updatePreview
{
	NSInteger selectedRowIndex = [matchTableView selectedRow];
	
	if (selectedRowIndex == -1)
	{
		[scriptPreview setString:@""];
	}
	else
	{
		NSString *filename = [matchRecipeFilenames objectAtIndex:selectedRowIndex];
		NSBundle *bundle = [NSBundle mainBundle];
		NSURL *urlForRecipe = [bundle URLForResource:[filename stringByDeletingPathExtension] withExtension:[filename pathExtension] subdirectory:@"Recipes"];
		NSString *scriptString = [NSString stringWithContentsOfURL:urlForRecipe usedEncoding:NULL error:NULL];
		
		[scriptPreview setString:scriptString];
		[scriptPreview recolorAfterChanges];
		[self highlightPreview];
	}
}

- (void)highlightPreview
{
	// Highlight matches in the selected recipe
	[scriptPreview clearHighlightMatches];
	
	NSString *keyword1 = [keyword1TextField stringValue];
	NSString *keyword2 = [keyword2TextField stringValue];
	NSString *keyword3 = [keyword3TextField stringValue];
	
	if ([keyword1 length])
		[scriptPreview highlightMatchesForString:keyword1];
	if ([keyword2 length])
		[scriptPreview highlightMatchesForString:keyword2];
	if ([keyword3 length])
		[scriptPreview highlightMatchesForString:keyword3];
}

- (void)keywordChanged:(id)sender
{
	// Remember the title of the currently selected recipes
	NSInteger selectedRowIndex = [matchTableView selectedRow];
	NSString *selectedRowText = nil;
	
	if (selectedRowIndex != -1)
		selectedRowText = [[matchRecipeFilenames objectAtIndex:selectedRowIndex] retain];
	
	// Filter the recipe list by the keywords entered
	[self constructMatchList];
	
	// Reload the tableview with recipes that match the new keywords
	[matchTableView reloadData];
	
	// Restore the selection to the extent possible
	if (selectedRowText)
	{
		BOOL foundSelection = NO;
		
		for (NSInteger i = 0; i < [matchTableView numberOfRows]; ++i)
		{
			NSString *rowText = [matchRecipeFilenames objectAtIndex:i];
			
			if ([rowText isEqualToString:selectedRowText])
			{
				[matchTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:i] byExtendingSelection:NO];
				foundSelection = YES;
				break;
			}
		}
		
		if (foundSelection)
		{
			[self highlightPreview];
		}
		else
		{
			// tableViewSelectionDidChange: is not called when the selection is lost by reloadData
			[self validateOK];
			[self updatePreview];
		}
		
		[selectedRowText release];
	}
}

- (IBAction)okButtonPressed:(id)sender
{
	// Open the selected recipe(s)
	NSInteger selectedRowIndex = [matchTableView selectedRow];
	
	if (selectedRowIndex != -1)
		[[NSDocumentController sharedDocumentController] openRecipeWithFilename:[matchRecipeFilenames objectAtIndex:selectedRowIndex]];
	
	// Then terminate the modal run loop
	[[NSApplication sharedApplication] stopModal];
}

- (IBAction)cancelButtonPressed:(id)sender
{
	// Terminate the modal run loop
	[[NSApplication sharedApplication] abortModal];
}

// NSTableViewDataSource
#pragma mark -
#pragma mark NSTableViewDataSource

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
	return [matchRecipeFilenames count];
}

- (nullable id)tableView:(NSTableView *)tableView objectValueForTableColumn:(nullable NSTableColumn *)tableColumn row:(NSInteger)row
{
	if ((row < 0) || (row >= (NSInteger)[matchRecipeFilenames count]))
		return nil;
	
	return [self displayStringForRecipeFilename:[matchRecipeFilenames objectAtIndex:row]];
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
	// This is mostly Apple boilerplate code; I couldn't get a cell-based table to work, so I gave up and made it view-based
	
	// Get an existing cell with the MyView identifier if it exists
	NSTextField *result = [tableView makeViewWithIdentifier:@"MyView" owner:self];
	
	// There is no existing cell to reuse so create a new one
	if (result == nil) {
		// Create the new NSTextField with a frame of the {0,0} with the width of the table.
		// Note that the height of the frame is not really relevant, because the row height will modify the height.
		result = [[NSTextField alloc] initWithFrame:NSZeroRect];
		
		// The identifier of the NSTextField instance is set to MyView.
		// This allows the cell to be reused.
		result.identifier = @"MyView";
		
		// BCH: configure the textfield view to look the way we want; could get it from the nib instead but this is easy
		[result setBordered:NO];
		[result setDrawsBackground:NO];
		[result setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
		
		// the truncation of lines with ellipses works, but expansion tooltips only show when the row is selected, grr
		[[result cell] setLineBreakMode:NSLineBreakByTruncatingTail];
		[[result cell] setTruncatesLastVisibleLine:YES];
		[result setAllowsExpansionToolTips:YES];
		
		// make the textfield uneditable
		[result setEditable:NO];
	}
	
	return result;
}

// NSTableViewDelegate
#pragma mark -
#pragma mark NSTableViewDelegate

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
	[self validateOK];
	[self updatePreview];
}

// NSTextFieldDelegate
#pragma mark -
#pragma mark NSTextFieldDelegate

- (void)controlTextDidChange:(NSNotification *)obj
{
	[self keywordChanged:nil];
}

@end




























