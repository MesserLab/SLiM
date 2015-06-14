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
		
		return wrapper->wrappedName;
	}
	
	return nil;
}

@end


































































