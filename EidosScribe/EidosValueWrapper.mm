//
//  EidosValueWrapper.m
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


#import "EidosValueWrapper.h"

@implementation EidosValueWrapper

+ (instancetype)wrapperForName:(NSString *)aName value:(EidosValue *)aValue
{
	return [[[self alloc] initWithWrappedName:aName value:aValue index:-1] autorelease];
}

+ (instancetype)wrapperForName:(NSString *)aName value:(EidosValue *)aValue index:(int)anIndex
{
	return [[[self alloc] initWithWrappedName:aName value:aValue index:anIndex] autorelease];
}

- (instancetype)initWithWrappedName:(NSString *)aName value:(EidosValue *)aValue index:(int)anIndex
{
	if (self = [super init])
	{
		wrappedName = [aName retain];
		wrappedValue = aValue;
		wrappedIndex = anIndex;
		
		valueIsOurs = wrappedValue->IsTemporary();
	}
	
	return self;
}

- (void)dealloc
{
	// At the point that dealloc gets called, the value object that we wrap may already be gone.  This happens if
	// the Context is responsible for the object and something happened to make the object go away.  We therefore
	// can't touch the value pointer at all here unless we own it.  This is why we mark the ones that we own ahead
	// of time, instead of just checking the IsTemporary() flag here.
	
	if ((wrappedIndex == -1) && valueIsOurs)
	{
		delete wrappedValue;
		wrappedValue = nullptr;
	}
	
	[wrappedName release];
	wrappedName = nil;
	
	[super dealloc];
}

@end



















































