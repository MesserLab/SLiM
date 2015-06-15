//
//  VariableBrowserController.m
//  SLiM
//
//  Created by Ben Haller on 6/13/15.
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


#import "VariableBrowserController.h"
#import "ScriptValueWrapper.h"

#include <sstream>


@implementation VariableBrowserController

- (instancetype)init
{
	if (self = [super init])
	{
		browserWrappers = [NSMutableArray new];
	}
	
	return self;
}

- (void)dealloc
{
	[browserWrappers release];
	browserWrappers = nil;
	
	[super dealloc];
}

- (void)reloadBrowser
{
	// Reload symbols in outline view
	[browserWrappers removeAllObjects];
	[_browserOutline reloadData];
}

- (void)setDelegate:(NSObject<VariableBrowserDelegate> *)newDelegate
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
#pragma mark NSOutlineView delegate

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
	SymbolTable *symbols = [_delegate symbolTable];
	
	if (symbols)
	{
		if (item == nil)
		{
			std::vector<std::string> readOnlySymbols = symbols->ReadOnlySymbols();
			std::vector<std::string> readWriteSymbols = symbols->ReadWriteSymbols();
			
			return (readOnlySymbols.size() + readWriteSymbols.size());
		}
		else
		{
			ScriptValueWrapper *wrapper = (ScriptValueWrapper *)item;
			ScriptValue_Object *value = (ScriptValue_Object *)(wrapper->wrappedValue);
			std::vector<std::string> readOnlySymbols = value->ReadOnlyMembersOfElements();
			std::vector<std::string> readWriteSymbols = value->ReadWriteMembersOfElements();
			
			return (readOnlySymbols.size() + readWriteSymbols.size());
		}
	}
	
	return 0;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
	SymbolTable *symbols = [_delegate symbolTable];
	
	if (symbols)
	{
		if (item == nil)
		{
			std::vector<std::string> readOnlySymbols = symbols->ReadOnlySymbols();
			std::vector<std::string> readWriteSymbols = symbols->ReadWriteSymbols();
			
			if (index < readOnlySymbols.size())
			{
				std::string symbolName = readOnlySymbols[index];
				ScriptValue *symbolValue = symbols->GetValueForSymbol(symbolName);
				NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
				ScriptValueWrapper *wrapper = [ScriptValueWrapper wrapperForName:symbolObjcName value:symbolValue];
				
				[browserWrappers addObject:wrapper];
				
				return wrapper;
			}
			else
			{
				std::string symbolName = readWriteSymbols[index - readOnlySymbols.size()];
				ScriptValue *symbolValue = symbols->GetValueForSymbol(symbolName);
				NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
				ScriptValueWrapper *wrapper = [ScriptValueWrapper wrapperForName:symbolObjcName value:symbolValue];
				
				[browserWrappers addObject:wrapper];
				
				return wrapper;
			}
		}
		else
		{
			ScriptValueWrapper *wrapper = (ScriptValueWrapper *)item;
			ScriptValue_Object *value = (ScriptValue_Object *)(wrapper->wrappedValue);
			std::vector<std::string> readOnlySymbols = value->ReadOnlyMembersOfElements();
			std::vector<std::string> readWriteSymbols = value->ReadWriteMembersOfElements();
			
			if (index < readOnlySymbols.size())
			{
				std::string symbolName = readOnlySymbols[index];
				ScriptValue *symbolValue = value->GetValueForMemberOfElements(symbolName);
				NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
				ScriptValueWrapper *childWrapper = [ScriptValueWrapper wrapperForName:symbolObjcName value:symbolValue];
				
				[browserWrappers addObject:childWrapper];
				
				return childWrapper;
			}
			else
			{
				std::string symbolName = readWriteSymbols[index - readOnlySymbols.size()];
				ScriptValue *symbolValue = value->GetValueForMemberOfElements(symbolName);
				NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
				ScriptValueWrapper *childWrapper = [ScriptValueWrapper wrapperForName:symbolObjcName value:symbolValue];
				
				[browserWrappers addObject:childWrapper];
				
				return childWrapper;
			}
		}
	}
	
	return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
	if ([item isKindOfClass:[ScriptValueWrapper class]])
	{
		ScriptValueWrapper *wrapper = (ScriptValueWrapper *)item;
		ScriptValue *value = wrapper->wrappedValue;
		
		if (value->Type() == ScriptValueType::kValueObject)
		{
			return YES;
		}
	}
	
	return NO;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
	if ([item isKindOfClass:[ScriptValueWrapper class]])
	{
		ScriptValueWrapper *wrapper = (ScriptValueWrapper *)item;
		
		if (tableColumn == _symbolColumn)
		{
			return wrapper->wrappedName;
		}
		else if (tableColumn == _typeColumn)
		{
			ScriptValueType type = wrapper->wrappedValue->Type();
			std::string type_string = StringForScriptValueType(type);
			const char *type_cstr = type_string.c_str();
			NSString *typeString = [NSString stringWithUTF8String:type_cstr];
			
			if (type == ScriptValueType::kValueObject)
			{
				ScriptValue_Object *object_value = (ScriptValue_Object *)wrapper->wrappedValue;
				std::string element_string = object_value->ElementType();
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
			ScriptValue *value = wrapper->wrappedValue;
			int value_count = value->Count();
			std::ostringstream outstream;
			
			// print values as a comma-separated list with strings quoted; halfway between print() and cat()
			for (int value_index = 0; value_index < value_count; ++value_index)
			{
				ScriptValue *element_value = value->GetValueAtIndex(value_index);
				
				if (value_index > 0)
					outstream << ", ";
				
				outstream << *element_value;
			}
			
			std::string out_str = outstream.str();
			const char *out_cstr = out_str.c_str();
			NSString *outString = [NSString stringWithUTF8String:out_cstr];
			
			return outString;
		}
	}
	
	return nil;
}

@end


































































