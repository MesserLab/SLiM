//
//  EidosValueWrapper.h
//  EidosScribe
//
//  Created by Ben Haller on 5/31/15.
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


#import <Cocoa/Cocoa.h>

#include "eidos_value.h"


/*
 
 EidosValueWrapper is used by EidosVariableBrowserController; it should never be used directly by client code.
 
 EidosValueWrapper is a rather tricky little beast.  The basic point is to provide the variable browser's NSOutlineView with
 Obj-C objects to represent the items that it displays.  Those items are "really" EidosValues of one sort or another – root
 EidosValues from the current symbol table, or sub-values that represent individual elements, properties, etc. from those
 root values.  Effectively, these values are related in a similar way to key paths, but with individual-element subscripting
 as well; foo.bar[5].baz.foobar[2] is a line that might be displayed in the variable browser, with a corresponding EidosValue.
 
 The first bit of complication comes from the fact that it isn't really kosher to keep EidosValues around unless you own them,
 and we often don't own the values that we wrap here (although sometimes we do).  We can get away with this only be being
 very careful not to dereference pointers that might be invalid.  Whenever the state of the Eidos interpreter changes, we
 throw out all of our old wrappers, thereby getting rid of the EidosValue pointers they contain, which might already be
 invalid by the time the EidosValueWrapper gets dealloced.  If we are careful in this way, we can get away with it.
 
 The second bit of complication, however, is that we want NSOutlineView to keep its expansion state identical across such
 reloads, even though it is displaying a whole new batch of EidosValueWrapper objects.  We therefore need EidosValueWrapper
 to implement -hash and -isEqual:, but those implementations cannot refer to the wrapped EidosValues at all, because they
 might already be invalid.  Hashing and equality therefor need to be based solely on the non-Eidos state of the wrappers.
 This means that a given wrapper needs to "know" its full "key path", not just its own name; hash and equality are determined
 by the full key path, in this scheme.  This is achieved by giving EidosValueWrapper a pointer to its parent, and making
 -hash and -isEqual: be recursive methods that follow the key path upward to the root and integrate information from the
 full path.
 
 */

@interface EidosValueWrapper : NSObject
{
@private
	EidosValueWrapper *parentWrapper;
	
	NSString *wrappedName;		// the displayed name
	int wrappedIndex;			// the index of wrappedValue upon which the row is based; -1 if the row represents the whole value
	int wrappedSiblingCount;	// the number of siblings of this item; used for -hash and -isEqual:
	
	EidosValue *wrappedValue;	// the value upon which the row is based; this may be invalid after the state of the Eidos interpreter changes
	BOOL valueIsOurs;			// if YES, we dispose of the wrapped value ourselves, if NO, the Context owns it
	BOOL isExpandable;			// a cached value; YES if wrappedValue is of type object, NO otherwise
	BOOL isConstant;			// is this value a built-in Eidos constant?
	
	NSMutableArray *childWrappers;
}

+ (instancetype)wrapperForName:(NSString *)aName parent:(EidosValueWrapper *)parent value:(EidosValue *)aValue;
+ (instancetype)wrapperForName:(NSString *)aName parent:(EidosValueWrapper *)parent value:(EidosValue *)aValue index:(int)anIndex of:(int)siblingCount;

- (instancetype)initWithWrappedName:(NSString *)aName parent:(EidosValueWrapper *)parent value:(EidosValue *)aValue index:(int)anIndex of:(int)siblingCount;

- (void)invalidateWrappedValues;
- (void)releaseChildWrappers;

- (NSArray *)childWrappers;

- (BOOL)isExpandable;

- (id)displaySymbol;
- (id)displayType;
- (id)displaySize;
- (id)displayValue;

@end












































