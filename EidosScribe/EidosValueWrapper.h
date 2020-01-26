//
//  EidosValueWrapper.h
//  EidosScribe
//
//  Created by Ben Haller on 5/31/15.
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


#import <Cocoa/Cocoa.h>

#include "eidos_value.h"


// BCH 4/7/2016: So we can build against the OS X 10.9 SDK
#ifndef NS_DESIGNATED_INITIALIZER
#define NS_DESIGNATED_INITIALIZER
#endif


/*
 
 This is an Objective-C++ header, and so can only be included by Objective-C++ compilations (.mm files instead of .m files).
 You should not need to include this header in your .h files, since you can declare protocol conformance in a class-continuation
 category in your .m file, so only classes that conform to this protocol should need to be Objective-C++.  However,
 EidosValueWrapper is used by EidosVariableBrowserController; it should never be used directly by client code.
 
 EidosValueWrapper is a rather tricky little beast.  The basic point is to provide the variable browser's NSOutlineView with
 Obj-C objects to represent the items that it displays.  Those items are "really" EidosValues of one sort or another – root
 EidosValues from the current symbol table, or sub-values that represent individual elements, properties, etc. from those
 root values.  Effectively, these values are related in a similar way to key paths, but with individual-element subscripting
 as well; foo.bar[5].baz.foobar[2] is a line that might be displayed in the variable browser, with a corresponding EidosValue.
 
 The first bit of complication comes from the fact that it isn't really kosher to keep EidosValues around unless you own them,
 so we participate in the smart pointer scheme used with EidosValue in eidos.  Whenever the state of the Eidos interpreter changes,
 we throw out all of our old wrappers, thereby getting rid of the EidosValue pointers they contain; we have a retain on them,
 but they are no longer part of the interpreter state and so should not be used.
 
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
}

+ (instancetype)wrapperForName:(NSString *)aName parent:(EidosValueWrapper *)parent value:(EidosValue_SP)aValue;
+ (instancetype)wrapperForName:(NSString *)aName parent:(EidosValueWrapper *)parent value:(EidosValue_SP)aValue index:(int)anIndex of:(int)siblingCount;

- (instancetype)init __attribute__((unavailable("This method is not available")));	// superclass designated initializer is not valid
- (instancetype)initWithWrappedName:(NSString *)aName parent:(EidosValueWrapper *)parent value:(EidosValue_SP)aValue index:(int)anIndex of:(int)siblingCount NS_DESIGNATED_INITIALIZER;

- (void)invalidateWrappedValues;
- (void)releaseChildWrappers;

- (NSArray *)childWrappers;

- (BOOL)isExpandable;

- (id)displaySymbol;
- (id)displayType;
- (id)displaySize;
- (id)displayValue;

@end












































