//
//  EidosVariableBrowserController.m
//  EidosScribe
//
//  Created by Ben Haller on 6/13/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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
#import "EidosValueWrapper.h"

#include "eidos_property_signature.h"

#include <sstream>


@implementation EidosVariableBrowserController

- (instancetype)init
{
	if (self = [super init])
	{
	}
	
	return self;
}

- (void)dealloc
{
	[rootBrowserWrappers release];
	rootBrowserWrappers = nil;
	
	[expandedSet release];
	expandedSet = nil;
	
	[super dealloc];
}

- (void)expandItemsInSet:(NSSet *)set belowItem:(id)parentItem
{
	NSUInteger childCount = [_browserOutline numberOfChildrenOfItem:parentItem];
	//NSLog(@"after reload, outline has %lu children (%ld rows), expandedSet has %lu items", (unsigned long)childCount, (long)[_browserOutline numberOfRows], (unsigned long)[expandedSet count]);
	
	for (int i = 0; i < childCount; ++i)
	{
		id childItem = [_browserOutline child:i ofItem:parentItem];
		
		if ([expandedSet containsObject:childItem])		// uses -hash and -isEqual:, not pointer equality!
		{
			if ([_browserOutline isExpandable:childItem])
			{
				[_browserOutline expandItem:childItem];
				//NSLog(@"      requesting expansion for item with name %@ (%p, hash %lu)", ((EidosValueWrapper *)childItem)->wrappedName, childItem, (unsigned long)[childItem hash]);
				
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
	
	if (rowCount > 0)
	{
		// The outline has items in it, so we will start a new set to save the expanded items
		[expandedSet release];
		expandedSet = [[NSMutableSet alloc] init];
		
		for (int i = 0; i < rowCount; ++i)
		{
			id rowItem = [_browserOutline itemAtRow:i];
			
			if ([_browserOutline isItemExpanded:rowItem])
			{
				[expandedSet addObject:rowItem];
				//NSLog(@"remembering item with name %@ (%p, hash %lu) as expanded", ((EidosValueWrapper *)rowItem)->wrappedName, rowItem, (unsigned long)[rowItem hash]);
			}
		}
	}
	
	// Clear out our wrapper arrays; the ones that are expanded are now retained by expandedSet
	[rootBrowserWrappers makeObjectsPerformSelector:@selector(releaseChildWrappers)];
	[rootBrowserWrappers release];
	rootBrowserWrappers = nil;		// this needs to be recached
	
	// Reload the outline view from scratch
	[_browserOutline reloadData];
	
	// Go through and expand items as needed
	[self expandItemsInSet:expandedSet belowItem:nil];
}

- (void)setDelegate:(NSObject<EidosVariableBrowserDelegate> *)newDelegate
{
	_delegate = newDelegate;
	
	[self reloadBrowser];
}

- (IBAction)toggleBrowserVisibility:(id)sender
{
	if ([_browserWindow isVisible])
		[_browserWindow performClose:nil];
	else
		[_browserWindow makeKeyAndOrderFront:nil];
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
	{
		[_browserWindowButton setState:NSOffState];
	}
}


//
//	NSOutlineView datasource / delegate methods
//
#pragma mark -
#pragma mark NSOutlineView delegate

- (void)recacheRootWrappers
{
	[rootBrowserWrappers release];
	rootBrowserWrappers = [NSMutableArray new];
	
	EidosSymbolTable *symbols = [_delegate symbolTable];
	
	std::vector<std::string> readOnlySymbols = symbols->ReadOnlySymbols();
	int readOnlySymbolCount = (int)readOnlySymbols.size();
	std::vector<std::string> readWriteSymbols = symbols->ReadWriteSymbols();
	int readWriteSymbolCount = (int)readWriteSymbols.size();
	
	for (int index = 0; index < readOnlySymbolCount;++ index)
	{
		const std::string &symbolName = readOnlySymbols[index];
		EidosValue *symbolValue = symbols->GetValueOrRaiseForSymbol(symbolName);
		NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
		EidosValueWrapper *wrapper = [EidosValueWrapper wrapperForName:symbolObjcName parent:nil value:symbolValue];
		
		[rootBrowserWrappers addObject:wrapper];
	}
	
	for (int index = 0; index < readWriteSymbolCount;++ index)
	{
		const std::string &symbolName = readWriteSymbols[index - readOnlySymbols.size()];
		EidosValue *symbolValue = symbols->GetValueOrRaiseForSymbol(symbolName);
		NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
		EidosValueWrapper *wrapper = [EidosValueWrapper wrapperForName:symbolObjcName parent:nil value:symbolValue];
		
		[rootBrowserWrappers addObject:wrapper];
	}
}

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
	EidosSymbolTable *symbols = [_delegate symbolTable];
	
	if (symbols)
	{
		if (item == nil)
		{
			if (!rootBrowserWrappers)
				[self recacheRootWrappers];
			
			return [rootBrowserWrappers count];
		}
		else
		{
			EidosValueWrapper *wrapper = (EidosValueWrapper *)item;
			
			if (!wrapper->childWrappers)
				[wrapper recacheWrappers];
			
			return [wrapper->childWrappers count];
		}
	}
	
	return 0;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
	EidosSymbolTable *symbols = [_delegate symbolTable];
	
	if (symbols)
	{
		if (item == nil)
		{
			if (!rootBrowserWrappers)
				[self recacheRootWrappers];
			
			return [rootBrowserWrappers objectAtIndex:index];
		}
		else
		{
			EidosValueWrapper *wrapper = (EidosValueWrapper *)item;
			
			if (!wrapper->childWrappers)
				[wrapper recacheWrappers];
			
			return [wrapper->childWrappers objectAtIndex:index];
		}
	}
	
	return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
	if ([item isKindOfClass:[EidosValueWrapper class]])
	{
		EidosValueWrapper *wrapper = (EidosValueWrapper *)item;
		
		// this is a cached value, set an init time in EidosValueWrapper to avoid a null dereference here; if the value is an object, this is YES
		// if it has >1 element, expansion will show a list of elements; if it has one element, expansion shows property values
		return wrapper->isExpandable;
	}
	
	return NO;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
	if ([item isKindOfClass:[EidosValueWrapper class]])
	{
		EidosValueWrapper *wrapper = (EidosValueWrapper *)item;
		int valueIndex = wrapper->wrappedIndex;
		
		if (valueIndex != -1)
		{
			// This row is a marker for an element within an object, so we treat it specially
			if (tableColumn == _symbolColumn)
			{
				static NSDictionary *indexLineAttrs = nil;
				
				if (!indexLineAttrs)
				{
					NSFont *baseFont = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]];
					NSFont *italicFont = [[NSFontManager sharedFontManager] convertFont:baseFont toHaveTrait:NSItalicFontMask];
					
					indexLineAttrs = [[NSDictionary dictionaryWithObjectsAndKeys:italicFont, NSFontAttributeName, nil] retain];
				}
				
				NSAttributedString *attrName = [[NSAttributedString alloc] initWithString:wrapper->wrappedName attributes:indexLineAttrs];
				
				return [attrName autorelease];
			}
			
			return @"";
		}
		
		if (tableColumn == _symbolColumn)
		{
			return wrapper->wrappedName;
		}
		else if (tableColumn == _typeColumn)
		{
			EidosValueType type = wrapper->wrappedValue->Type();
			std::string type_string = StringForEidosValueType(type);
			const char *type_cstr = type_string.c_str();
			NSString *typeString = [NSString stringWithUTF8String:type_cstr];
			
			if (type == EidosValueType::kValueObject)
			{
				EidosValue_Object *object_value = (EidosValue_Object *)wrapper->wrappedValue;
				const std::string &element_string = object_value->ElementType();
				const char *element_cstr = element_string.c_str();
				NSString *elementString = [NSString stringWithUTF8String:element_cstr];
				
				typeString = [NSString stringWithFormat:@"%@<%@>", typeString, elementString];
			}
			
			return typeString;
		}
		else if (tableColumn == _sizeColumn)
		{
			int size = wrapper->wrappedValue->Count();
			
			return [NSString stringWithFormat:@"%d", size];
		}
		else if (tableColumn == _valueColumn)
		{
			EidosValue *value = wrapper->wrappedValue;
			int value_count = value->Count();
			std::ostringstream outstream;
			
			// print values as a comma-separated list with strings quoted; halfway between print() and cat()
			for (int value_index = 0; value_index < value_count; ++value_index)
			{
				EidosValue *element_value = value->GetValueAtIndex(value_index, nullptr);
				
				if (value_index > 0)
				{
					outstream << ", ";
					
					// terminate the list at some reasonable point, otherwise we generate massively long strings for large vectors...
					if (value_index > 50)
					{
						outstream << ", ...";
						break;
					}
				}
				
				outstream << *element_value;
				
				if (element_value->IsTemporary()) delete element_value;
			}
			
			NSString *outString = [NSString stringWithUTF8String:outstream.str().c_str()];
			
			return outString;
		}
	}
	
	return nil;
}

@end


































































