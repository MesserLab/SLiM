//
//  ScriptValueWrapper.h
//  SLiM
//
//  Created by Ben Haller on 5/31/15.
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

