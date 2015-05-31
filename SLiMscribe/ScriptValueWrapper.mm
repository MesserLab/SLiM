//
//  ScriptValueWrapper.m
//  SLiM
//
//  Created by Ben Haller on 5/31/15.
//  Copyright (c) 2015 Messer Lab, http://messerlab.org/software/. All rights reserved.
//

#import "ScriptValueWrapper.h"

@implementation ScriptValueWrapper

+ (instancetype)wrapperForName:(NSString *)aName value:(ScriptValue *)aValue
{
	return [[[self alloc] initWithWrappedName:aName value:aValue] autorelease];
}

- (instancetype)initWithWrappedName:(NSString *)aName value:(ScriptValue *)aValue
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
