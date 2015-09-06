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

#include "eidos_value.h"
#include "eidos_property_signature.h"


@implementation EidosValueWrapper

+ (instancetype)wrapperForName:(NSString *)aName parent:(EidosValueWrapper *)parent value:(EidosValue *)aValue
{
	return [[[self alloc] initWithWrappedName:aName parent:parent value:aValue index:-1 of:0] autorelease];
}

+ (instancetype)wrapperForName:(NSString *)aName parent:(EidosValueWrapper *)parent value:(EidosValue *)aValue index:(int)anIndex of:(int)siblingCount
{
	return [[[self alloc] initWithWrappedName:aName parent:parent value:aValue index:anIndex of:siblingCount] autorelease];
}

- (instancetype)initWithWrappedName:(NSString *)aName parent:(EidosValueWrapper *)parent value:(EidosValue *)aValue index:(int)anIndex of:(int)siblingCount
{
	if (self = [super init])
	{
		parentWrapper = [parent retain];
		
		wrappedName = [aName retain];
		wrappedIndex = anIndex;
		wrappedSiblingCount = siblingCount;
		
		wrappedValue = aValue;
		valueIsOurs = wrappedValue->IsTemporary();
		
		// We cache this so that we know whether we are expandable without needing to dereference wrappedValue;
		// we therefore know whether or not we are expandable even after wrappedValue is invalidated
		isExpandable = (wrappedValue->Type() == EidosValueType::kValueObject);
		
		// We want to display Eidos constants in gray text, to de-emphasize them.  For now, we just hard-code them
		// as a hack, because we *don't* want SLiM constants (sim, g1, p1, etc.) to display dimmed
		isConstant = NO;
		
		if (!parentWrapper && ([wrappedName isEqualToString:@"T"] || [wrappedName isEqualToString:@"F"] || [wrappedName isEqualToString:@"E"] || [wrappedName isEqualToString:@"PI"] || [wrappedName isEqualToString:@"INF"] || [wrappedName isEqualToString:@"NAN"] || [wrappedName isEqualToString:@"NULL"]))
			isConstant = YES;
		
		childWrappers = nil;
	}
	
	return self;
}

- (void)dealloc
{
	// At the point that dealloc gets called, the value object that we wrap may already be gone.  This happens if
	// the Context is responsible for the object and something happened to make the object go away.  We therefore
	// can't touch the value pointer at all here unless we own it.  This is why we mark the ones that we own ahead
	// of time, instead of just checking the IsTemporary() flag here.
	
	if (valueIsOurs && (wrappedIndex == -1))
		delete wrappedValue;
	
	wrappedValue = nullptr;
	valueIsOurs = false;
	
	[wrappedName release];
	wrappedName = nil;
	
	[parentWrapper release];
	parentWrapper = nil;
	
	[childWrappers release];
	childWrappers = nil;
	
	[super dealloc];
}

- (void)invalidateWrappedValues
{
	if (valueIsOurs && (wrappedIndex == -1))
		delete wrappedValue;
	
	wrappedValue = nullptr;
	valueIsOurs = false;
	
	[childWrappers makeObjectsPerformSelector:@selector(invalidateWrappedValues)];
}

- (void)releaseChildWrappers
{
	[childWrappers makeObjectsPerformSelector:@selector(releaseChildWrappers)];
	
	[childWrappers release];
	childWrappers = nil;
}

- (NSArray *)childWrappers
{
	if (!childWrappers)
	{
		// If we don't have our cache of child wrappers, set it up on demand
		childWrappers = [NSMutableArray new];
		
		int elementCount = wrappedValue->Count();
		
		// values which are of object type and contain more than one element get displayed as a list of elements
		if (elementCount > 1)
		{
			for (int index = 0; index < elementCount;++ index)
			{
				NSString *childName = [NSString stringWithFormat:@"%@[%ld]", wrappedName, (long)index];
				EidosValue *childValue = wrappedValue->GetValueAtIndex(index, nullptr);
				EidosValueWrapper *childWrapper = [EidosValueWrapper wrapperForName:childName parent:self value:childValue index:index of:elementCount];
				
				[childWrappers addObject:childWrapper];
			}
		}
		else if (wrappedValue->Type() == EidosValueType::kValueObject)
		{
			EidosValue_Object *wrapped_object = ((EidosValue_Object *)wrappedValue);
			const EidosObjectClass *object_class = wrapped_object->Class();
			const std::vector<const EidosPropertySignature *> *properties = object_class->Properties();
			int propertyCount = (int)properties->size();
			
			for (int index = 0; index < propertyCount; ++index)
			{
				const EidosPropertySignature *propertySig = (*properties)[index];
				const std::string &symbolName = propertySig->property_name_;
				EidosGlobalStringID symbolID = propertySig->property_id_;
				EidosValue *symbolValue = wrapped_object->GetPropertyOfElements(symbolID);
				NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
				EidosValueWrapper *childWrapper = [EidosValueWrapper wrapperForName:symbolObjcName parent:self value:symbolValue];
				
				[childWrappers addObject:childWrapper];
			}
		}
	}
	
	return childWrappers;
}

- (BOOL)isExpandable
{
	return isExpandable;
}

+ (NSDictionary *)italicAttrs
{
	static NSDictionary *italicAttrs = nil;
	
	if (!italicAttrs)
	{
		NSFont *baseFont = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]];
		NSFont *italicFont = [[NSFontManager sharedFontManager] convertFont:baseFont toHaveTrait:NSItalicFontMask];
		
		italicAttrs = [@{NSFontAttributeName : italicFont} retain];
	}
	
	return italicAttrs;
}

+ (NSDictionary *)dimmedAttrs
{
	static NSDictionary *dimmedAttrs = nil;
	
	if (!dimmedAttrs)
	{
		NSFont *baseFont = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]];
		
		dimmedAttrs = [@{NSFontAttributeName : baseFont, NSForegroundColorAttributeName : [NSColor colorWithCalibratedWhite:0.5 alpha:1.0]} retain];
	}
	
	return dimmedAttrs;
}

+ (NSDictionary *)centeredDimmedAttrs
{
	static NSDictionary *centeredDimmedAttrs = nil;
	
	if (!centeredDimmedAttrs)
	{
		NSFont *baseFont = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]];
		NSMutableParagraphStyle *paragraphStyle = [NSMutableParagraphStyle new];
		
		[paragraphStyle setAlignment:NSCenterTextAlignment];
		
		centeredDimmedAttrs = [@{NSFontAttributeName : baseFont, NSForegroundColorAttributeName : [NSColor colorWithCalibratedWhite:0.5 alpha:1.0], NSParagraphStyleAttributeName : paragraphStyle} retain];
		
		[paragraphStyle release];
	}
	
	return centeredDimmedAttrs;
}

- (id)displaySymbol
{
	// If this row is a marker for an element within an object we treat it specially
	if (wrappedIndex != -1)
		return [[[NSAttributedString alloc] initWithString:wrappedName attributes:[EidosValueWrapper italicAttrs]] autorelease];
	
	if (isConstant)
		return [[[NSAttributedString alloc] initWithString:wrappedName attributes:[EidosValueWrapper dimmedAttrs]] autorelease];
	
	return wrappedName;
}

- (id)displayType
{
	// If this row is a marker for an element within an object we treat it specially
	if (wrappedIndex != -1)
		return @"";
	
	EidosValueType type = wrappedValue->Type();
	std::string type_string = StringForEidosValueType(type);
	const char *type_cstr = type_string.c_str();
	NSString *typeString = [NSString stringWithUTF8String:type_cstr];
	
	if (type == EidosValueType::kValueObject)
	{
		EidosValue_Object *object_value = (EidosValue_Object *)wrappedValue;
		const std::string &element_string = object_value->ElementType();
		const char *element_cstr = element_string.c_str();
		NSString *elementString = [NSString stringWithUTF8String:element_cstr];
		
		typeString = [NSString stringWithFormat:@"%@<%@>", typeString, elementString];
	}
	
	if (isConstant)
		return [[[NSAttributedString alloc] initWithString:typeString attributes:[EidosValueWrapper dimmedAttrs]] autorelease];
	
	return typeString;
}

- (id)displaySize
{
	// If this row is a marker for an element within an object we treat it specially
	if (wrappedIndex != -1)
		return @"";
	
	NSString *sizeString = [NSString stringWithFormat:@"%d", wrappedValue->Count()];
	
	if (isConstant)
		return [[[NSAttributedString alloc] initWithString:sizeString attributes:[EidosValueWrapper centeredDimmedAttrs]] autorelease];
	
	return sizeString;
}

- (id)displayValue
{
	// If this row is a marker for an element within an object we treat it specially
	if (wrappedIndex != -1)
		return @"";
	
	int value_count = wrappedValue->Count();
	std::ostringstream outstream;
	
	// print values as a comma-separated list with strings quoted; halfway between print() and cat()
	for (int value_index = 0; value_index < value_count; ++value_index)
	{
		EidosValue *element_value = wrappedValue->GetValueAtIndex(value_index, nullptr);
		
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
	
	if (isConstant)
		return [[[NSAttributedString alloc] initWithString:outString attributes:[EidosValueWrapper dimmedAttrs]] autorelease];
	
	return outString;
}

//
//	NSObject subclass overrides, to redefine equality
//

- (BOOL)isEqual:(id)anObject
{
	if (self == anObject)
		return YES;
	if (![anObject isMemberOfClass:[EidosValueWrapper class]])
		return NO;
	
	EidosValueWrapper *otherWrapper = (EidosValueWrapper *)anObject;
	
	if (wrappedIndex != otherWrapper->wrappedIndex)
		return NO;
	if (wrappedSiblingCount != otherWrapper->wrappedSiblingCount)
		return NO;
	if (![wrappedName isEqualToString:otherWrapper->wrappedName])
		return NO;
	
	// We call through to isEqualToWrapper: only if the parent wrapper is defined for both objects;
	// this is partly for speed, and partly because [nil isEqualToWrapper:nil] would give NO.
	EidosValueWrapper *otherWrapperParent = otherWrapper->parentWrapper;
	
	if (parentWrapper == otherWrapperParent)
		return YES;
	if ((parentWrapper && !otherWrapperParent) || (!parentWrapper && otherWrapperParent))
		return NO;
	
	if (![parentWrapper isEqualToWrapper:otherWrapperParent])
		return NO;
	
	return YES;
}

- (BOOL)isEqualToWrapper:(EidosValueWrapper *)otherWrapper
{
	// Note that this method is missing the self==object test at the beginning!  This is because it
	// was already done by the caller; this method is not designed to be called externally!
	// Similarly, it does not check for object==nil, so it will crash if called with nil!
	
	if (wrappedIndex != otherWrapper->wrappedIndex)
		return NO;
	if (wrappedSiblingCount != otherWrapper->wrappedSiblingCount)
		return NO;
	if (![wrappedName isEqualToString:otherWrapper->wrappedName])
		return NO;
	
	// We call through to isEqualToWrapper: only if the parent wrapper is defined for both objects;
	// this is partly for speed, and partly because [nil isEqualToWrapper:nil] would give NO.
	EidosValueWrapper *otherWrapperParent = otherWrapper->parentWrapper;
	
	if (parentWrapper == otherWrapperParent)
		return YES;
	if ((parentWrapper && !otherWrapperParent) || (!parentWrapper && otherWrapperParent))
		return NO;
	
	if (![parentWrapper isEqualToWrapper:otherWrapperParent])
		return NO;
	
	return YES;
}

- (NSUInteger)hash
{
	NSUInteger hash = [wrappedName hash];
	
	hash ^= wrappedIndex;
	hash ^= (wrappedSiblingCount << 16);
	
	if (parentWrapper)
		hash ^= ([parentWrapper hash] << 1);
	
	//NSLog(@"hash for item named %@ == %lu", wrappedName, (unsigned long)hash);
	
	return hash;
}

@end



















































