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
	return [[[self alloc] initWithWrappedName:aName value:aValue] autorelease];
}

- (instancetype)initWithWrappedName:(NSString *)aName value:(EidosValue *)aValue
{
	if (self = [super init])
	{
		wrappedName = [aName retain];
		wrappedValue = aValue;
	}
	
	return self;
}

- (void)dealloc
{
	[wrappedName release];
	wrappedName = nil;
	
	[super dealloc];
}

@end
