//
//  EidosVariableBrowserController.m
//  EidosScribe
//
//  Created by Ben Haller on 6/13/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
//

//	This file is part of Eidos.
//
//	Eidos is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	Eidos is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with Eidos.  If not, see <http://www.gnu.org/licenses/>.


#import "EidosVariableBrowserController.h"
#import "EidosVariableBrowserControllerDelegate.h"
#import "EidosValueWrapper.h"

#include "eidos_property_signature.h"
#include "eidos_symbol_table.h"

#include <sstream>


// Notifications sent to allow other objects that care about the visibility of the browser, such as toggle buttons, to update
NSString *EidosVariableBrowserWillHideNotification = @"EidosVariableBrowserWillHide";
NSString *EidosVariableBrowserWillShowNotification = @"EidosVariableBrowserWillShow";


@interface EidosVariableBrowserController () <NSWindowDelegate, NSOutlineViewDataSource, NSOutlineViewDelegate>
{
@private
	// Wrappers for the currently displayed objects
	NSMutableArray *rootBrowserWrappers;
	
	// A set used to remember expanded items; see -reloadBrowser
	NSMutableSet *expandedSet;
}
@end


@implementation EidosVariableBrowserController

- (instancetype)init
{
	if (self = [super init])
	{
		// We permanently keep a set of all elements that should be expanded; this persists across browser reloads, etc.
		expandedSet = [[NSMutableSet alloc] init];
	}
	
	return self;
}

- (void)dealloc
{
	//NSLog(@"EidosVariableBrowserController dealloc");
	
	[self invalidateRootWrappers];
	
	[expandedSet release];
	expandedSet = nil;
	
	[self cleanup];
	
	[super dealloc];
}

- (void)setDelegate:(NSObject<EidosVariableBrowserControllerDelegate> *)newDelegate
{
	if (newDelegate && ![newDelegate conformsToProtocol:@protocol(EidosVariableBrowserControllerDelegate)])
		NSLog(@"Delegate %@ assigned to EidosVariableBrowserController %p does not conform to the EidosVariableBrowserControllerDelegate protocol!", newDelegate, self);
	
	_delegate = newDelegate;	// nonatomic, assign
	
	// Since our delegate defines what appears in our outline view, we need to reload when the delegate changes
	[self reloadBrowser];
}

- (void)showWindow
{
	if (![_browserWindow isVisible])
	{
		[[NSNotificationCenter defaultCenter] postNotificationName:EidosVariableBrowserWillShowNotification object:self];
		[_browserWindow makeKeyAndOrderFront:nil];
	}
}

- (void)hideWindow
{
	if ([_browserWindow isVisible])
	{
		[[NSNotificationCenter defaultCenter] postNotificationName:EidosVariableBrowserWillHideNotification object:self];
		[_browserWindow performClose:nil];
	}
}

- (void)cleanup
{
	[[self browserWindow] setDelegate:nil];
	[self setBrowserWindow:nil];
	
	[self setDelegate:nil];
}

- (void)expandItemsInSet:(NSSet *)set belowItem:(id)parentItem
{
	// BCH 4/7/2016: The -numberOfChildrenOfItem: and child:ofItem: methods of NSOutlineView do not exist in 10.9,
	// so we have to get the datasource of the outline view and get the information ourselves.
	BOOL respondsToNumberOfChildrenOfItem = [_browserOutline respondsToSelector:@selector(numberOfChildrenOfItem:)];
	BOOL respondsToChildOfItem = [_browserOutline respondsToSelector:@selector(child:ofItem:)];
	id <NSOutlineViewDataSource> datasource = ((respondsToNumberOfChildrenOfItem && respondsToChildOfItem) ? nil : [_browserOutline dataSource]);
	
	NSUInteger childCount = (respondsToNumberOfChildrenOfItem ? [_browserOutline numberOfChildrenOfItem:parentItem] : [datasource outlineView:_browserOutline numberOfChildrenOfItem:parentItem]);
	//NSLog(@"after reload, outline has %lu children (%ld rows), expandedSet has %lu items", (unsigned long)childCount, (long)[_browserOutline numberOfRows], (unsigned long)[expandedSet count]);
	
	for (unsigned int i = 0; i < childCount; ++i)
	{
		id childItem = (respondsToChildOfItem ? [_browserOutline child:i ofItem:parentItem] : [datasource outlineView:_browserOutline child:i ofItem:parentItem]);
		
		if ([expandedSet containsObject:childItem])		// uses -hash and -isEqual:, not pointer equality!
		{
			if ([_browserOutline isExpandable:childItem])
			{
				[_browserOutline expandItem:childItem];
				//NSLog(@"      requested expansion for item with name %@ (%p, hash %lu)", ((EidosValueWrapper *)childItem)->wrappedName, childItem, (unsigned long)[childItem hash]);
				
				// having expanded the item, we forget it from our set; this way if the user collapses it we won't re-expand it
				// note that if it stays expanded, it will be re-added to our set in reloadBrowser
				[expandedSet removeObject:childItem];
				
				[self expandItemsInSet:set belowItem:childItem];
			}
		}
	}
}

- (void)reloadBrowser
{
	// Reload symbols in our outline view.  This is complicated because we generally do a reload to a state that has zero items (while
	// script is executing) and then do another reload to a state that has items again (because the script is done executing), and we
	// want the expansion state to be preserved across that rather jumpy process.  The basic scheme is: (1) When we are about to
	// reload, we save all expanded items by going through all the rows of the outline view, getting the item for each row, finding
	// out if it is expanded, and if it is, keeping it in an NSSet – but we do this only if the outline view has items in it.  Then,
	// (2) After reloading, we go through each new item, see if it exists in our saved set (which uses -hash and -isEqual:, which
	// EidosValueWrapper implements for this purpose), and if so, we expand it and recurse down to check its children as well.
	
	// First, invalidate all of our old wrapped objects, to make sure that any invalid dereferences cause a clean crash
	[rootBrowserWrappers makeObjectsPerformSelector:@selector(invalidateWrappedValues)];
	
	// Then go through the outline view and save all items that are expanded
	NSUInteger rowCount = [_browserOutline numberOfRows];
	
	// BCH 26 Jan. 2020: On 10.15 NSOutlineView apparently caches the number of rows, so even though we would now return 0 from
	// outlineView:numberOfChildrenOfItem:, it gives us a non-zero count and then logs errors when we call itemAtRow:.  So now
	// we explicitly check that we actually have a symbol table, and if not, correct the number of rows to zero.
	if (![_delegate symbolTableForEidosVariableBrowserController:self])
		rowCount = 0;
	
	if (rowCount > 0)
	{
		// The outline has items in it, so we will start a new set to save the expanded items
		for (unsigned int i = 0; i < rowCount; ++i)
		{
			id rowItem = [_browserOutline itemAtRow:i];
			
			// Because we are in transition, it is possible for rowItem to be nil.  Specifically, the outline view may cache the
			// number of rows, and thus rowCount may be non-zero; and yet when a given row is asked for, the outline view may
			// then attempt to obtain it from the delegate (because NSOutlineView loads rows lazily), and the row may no longer
			// be there because the symbol table has changed (or been tossed entirely).  This is OK, actually.  Any row that
			// comes back as nil here is not expanded; a row doesn't get expanded if it hasn't even been loaded.  So rows that
			// come back as nil here can simply be ignored, since our only goal is to log a set of the expanded rows.  This
			// situation does, however, point out the trickiness inherent in asking NSOutlineView about its own state; it will
			// not necessarily answer in a self-consistent fashion!
			
			if (rowItem)
			{
				if ([_browserOutline isItemExpanded:rowItem])
				{
					[expandedSet addObject:rowItem];
					//NSLog(@"remembering item with name %@ (%p, hash %lu) as expanded", ((EidosValueWrapper *)rowItem)->wrappedName, rowItem, (unsigned long)[rowItem hash]);
				}
			}
		}
	}
	
	// Clear out our wrapper arrays; the ones that are expanded are now retained by expandedSet
	[self invalidateRootWrappers];
	
	// Reload the outline view from scratch
	[_browserOutline reloadData];
	
	// Go through and expand items as needed
	[self expandItemsInSet:expandedSet belowItem:nil];
}

- (void)invalidateRootWrappers
{
	[rootBrowserWrappers makeObjectsPerformSelector:@selector(releaseChildWrappers)];
	[rootBrowserWrappers release];
	rootBrowserWrappers = nil;		// set up to be recached
}

- (NSArray *)rootWrappers
{
	if (!rootBrowserWrappers)
	{
		// If we don't have our root wrappers, set up the cache now
		rootBrowserWrappers = [NSMutableArray new];
		
		EidosSymbolTable *symbols = [_delegate symbolTableForEidosVariableBrowserController:self];
		
		{
			std::vector<std::string> readOnlySymbols = symbols->ReadOnlySymbols();
			int readOnlySymbolCount = (int)readOnlySymbols.size();
			
			for (int index = 0; index < readOnlySymbolCount;++ index)
			{
				const std::string &symbolName = readOnlySymbols[index];
				EidosValue_SP symbolValue = symbols->GetValueOrRaiseForSymbol(Eidos_GlobalStringIDForString(symbolName));
				NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
				EidosValueWrapper *wrapper = [EidosValueWrapper wrapperForName:symbolObjcName parent:nil value:symbolValue];
				
				[rootBrowserWrappers addObject:wrapper];
			}
		}
		
		{
			std::vector<std::string> readWriteSymbols = symbols->ReadWriteSymbols();
			int readWriteSymbolCount = (int)readWriteSymbols.size();
			
			for (int index = 0; index < readWriteSymbolCount;++ index)
			{
				const std::string &symbolName = readWriteSymbols[index];
				EidosValue_SP symbolValue = symbols->GetValueOrRaiseForSymbol(Eidos_GlobalStringIDForString(symbolName));
				NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
				EidosValueWrapper *wrapper = [EidosValueWrapper wrapperForName:symbolObjcName parent:nil value:symbolValue];
				
				[rootBrowserWrappers addObject:wrapper];
			}
		}
	}
	
	return rootBrowserWrappers;
}

- (IBAction)toggleBrowserVisibility:(id)sender
{
	if ([_browserWindow isVisible])
		[self hideWindow];
	else
		[self showWindow];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
	SEL sel = [menuItem action];
	
	if (sel == @selector(toggleBrowserVisibility:))
		[menuItem setTitle:([_browserWindow isVisible] ? @"Hide Variable Browser" : @"Show Variable Browser")];
	
	return YES;
}


//
//	NSWindow delegate methods
//
#pragma mark -
#pragma mark NSWindow delegate

- (void)windowWillClose:(NSNotification *)notification
{
	NSWindow *closingWindow = [notification object];
	
	if (closingWindow == _browserWindow)
		[[NSNotificationCenter defaultCenter] postNotificationName:EidosVariableBrowserWillHideNotification object:self];
}


//
//	NSOutlineView datasource / delegate methods
//
#pragma mark -
#pragma mark NSOutlineView delegate

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
	if ([_delegate symbolTableForEidosVariableBrowserController:self])
	{
		NSArray *wrapperArray = (item ? [(EidosValueWrapper *)item childWrappers] : [self rootWrappers]);
		NSInteger count = [wrapperArray count];
		
		//NSLog(@"count == %ld", (long)count);
		return count;
	}
	
	//NSLog(@"count == 0");
	return 0;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
	if ([_delegate symbolTableForEidosVariableBrowserController:self])
	{
		NSArray *wrapperArray = (item ? [(EidosValueWrapper *)item childWrappers] : [self rootWrappers]);
		id child = [wrapperArray objectAtIndex:index];
		
		//NSLog(@"child at index %ld == %@", (long)index, child);
		return child;
	}
	
	//NSLog(@"child at index %ld == NULL", (long)index);
	return (id _Nonnull)nil;	// get rid of the static analyzer warning
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
	if (item)
		return [(EidosValueWrapper *)item isExpandable];
	
	return NO;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
	if (item)
	{
		EidosValueWrapper *wrapper = (EidosValueWrapper *)item;
		
		if (tableColumn == _symbolColumn)
			return [wrapper displaySymbol];
		if (tableColumn == _typeColumn)
			return [wrapper displayType];
		if (tableColumn == _sizeColumn)
			return [wrapper displaySize];
		if (tableColumn == _valueColumn)
			return [wrapper displayValue];
	}
	
	return @"";
}

@end


































































