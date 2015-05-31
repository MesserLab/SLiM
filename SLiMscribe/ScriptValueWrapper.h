//
//  ScriptValueWrapper.h
//  SLiM
//
//  Created by Ben Haller on 5/31/15.
//  Copyright (c) 2015 Messer Lab, http://messerlab.org/software/. All rights reserved.
//

#import <Foundation/Foundation.h>

#include "script_value.h"


@interface ScriptValueWrapper : NSObject
{
@public
	NSString *wrappedName;
	ScriptValue *wrappedValue;
}

+ (instancetype)wrapperForName:(NSString *)aName value:(ScriptValue *)aValue;

- (instancetype)initWithWrappedName:(NSString *)aName value:(ScriptValue *)aValue;

@end

