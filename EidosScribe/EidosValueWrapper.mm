//
//  EidosValueWrapper.m
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


#import "EidosValueWrapper.h"

#include "eidos_value.h"
#include "eidos_property_signature.h"


@interface EidosValueWrapper ()
{
@private
	EidosValueWrapper *parentWrapper;
	
	NSString *wrappedName;		// the displayed name
	int wrappedIndex;			// the index of wrappedValue upon which the row is based; -1 if the row represents the whole value
	int wrappedSiblingCount;	// the number of siblings of this item; used for -hash and -isEqual:
	
	EidosValue_SP wrappedValue;	// the value upon which the row is based
	BOOL isExpandable;			// a cached value; YES if wrappedValue is of type object, NO otherwise
	BOOL isConstant;			// is this value a built-in Eidos constant?
	
	NSMutableArray *childWrappers;
}
@end


@implementation EidosValueWrapper

+ (instancetype)wrapperForName:(NSString *)aName parent:(EidosValueWrapper *)parent value:(EidosValue_SP)aValue
{
	return [[[self alloc] initWithWrappedName:aName parent:parent value:std::move(aValue) index:-1 of:0] autorelease];
}

+ (instancetype)wrapperForName:(NSString *)aName parent:(EidosValueWrapper *)parent value:(EidosValue_SP)aValue index:(int)anIndex of:(int)siblingCount
{
	return [[[self alloc] initWithWrappedName:aName parent:parent value:std::move(aValue) index:anIndex of:siblingCount] autorelease];
}

- (instancetype)init
{
	// The superclass designated initializer is not valid for us
	NSAssert(false, @"-init is not defined for this class");
	
	return [self initWithWrappedName:nil parent:nil value:EidosValue_SP(nullptr) index:0 of:0];
}

- (instancetype)initWithWrappedName:(NSString *)aName parent:(EidosValueWrapper *)parent value:(EidosValue_SP)aValue index:(int)anIndex of:(int)siblingCount
{
	if (self = [super init])
	{
		parentWrapper = [parent retain];
		
		wrappedName = [aName retain];
		wrappedIndex = anIndex;
		wrappedSiblingCount = siblingCount;
		
		wrappedValue = std::move(aValue);
		
		// We cache this so that we know whether we are expandable without needing to dereference wrappedValue;
		// we therefore know whether or not we are expandable even after wrappedValue is invalidated
		if (wrappedValue)
			isExpandable = (wrappedValue->Type() == EidosValueType::kValueObject);
		else
			isExpandable = false;
		
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
	wrappedValue.reset();
	
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
	wrappedValue.reset();
	
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
		
		// If we have no wrapped value, we have no children
		if (!wrappedValue)
			return childWrappers;
		
		int elementCount = wrappedValue->Count();
		
		// values which are of object type and contain more than one element get displayed as a list of elements
		if (elementCount > 1)
		{
			for (int index = 0; index < elementCount;++ index)
			{
				NSString *childName = [NSString stringWithFormat:@"%@[%ld]", wrappedName, (long)index];
				EidosValue_SP childValue = wrappedValue->GetValueAtIndex(index, nullptr);
				EidosValueWrapper *childWrapper = [EidosValueWrapper wrapperForName:childName parent:self value:std::move(childValue) index:index of:elementCount];
				
				[childWrappers addObject:childWrapper];
			}
		}
		else if (wrappedValue->Type() == EidosValueType::kValueObject)
		{
			EidosValue_Object *wrapped_object = ((EidosValue_Object *)wrappedValue.get());
			const EidosObjectClass *object_class = wrapped_object->Class();
			const std::vector<EidosPropertySignature_CSP> *properties = object_class->Properties();
			int propertyCount = (int)properties->size();
			bool oldSuppressWarnings = gEidosSuppressWarnings, inaccessibleCaught = false;
			
			gEidosSuppressWarnings = true;		// prevent warnings from questionable property accesses from producing warnings in the user's output pane
			
			for (int index = 0; index < propertyCount; ++index)
			{
				const EidosPropertySignature_CSP &propertySig = (*properties)[index];
				const std::string &symbolName = propertySig->property_name_;
				EidosGlobalStringID symbolID = propertySig->property_id_;
				NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
				EidosValue_SP symbolValue;
				
				// protect against raises in property accesses due to inaccessible properties
				try {
					symbolValue = wrapped_object->GetPropertyOfElements(symbolID);
				} catch (...) {
					//std::cout << "caught inaccessible property " << symbolName << std::endl;
					inaccessibleCaught = true;
				}
				
				EidosValueWrapper *childWrapper = [EidosValueWrapper wrapperForName:symbolObjcName parent:self value:std::move(symbolValue)];
				
				[childWrappers addObject:childWrapper];
			}
			
			gEidosSuppressWarnings = oldSuppressWarnings;
			
			if (inaccessibleCaught)
			{
				// throw away the raise message(s) so they don't confuse us
				gEidosTermination.clear();
				gEidosTermination.str(gEidosStr_empty_string);
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
	
	// If the value is inaccessible display nothing
	if (!wrappedValue)
		return @"";
	
	EidosValueType type = wrappedValue->Type();
	std::string type_string = StringForEidosValueType(type);
	const char *type_cstr = type_string.c_str();
	NSString *typeString = [NSString stringWithUTF8String:type_cstr];
	
	if (type == EidosValueType::kValueObject)
	{
		EidosValue_Object *object_value = (EidosValue_Object *)wrappedValue.get();
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
	
	// If the value is inaccessible display nothing
	if (!wrappedValue)
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
	
	// If the value is inaccessible display "<inaccessible>"
	if (!wrappedValue)
		return @"<inaccessible>";
	
	int value_count = wrappedValue->Count();
	std::ostringstream outstream;
	
	// print values as a comma-separated list with strings quoted; halfway between print() and cat()
	for (int value_index = 0; value_index < value_count; ++value_index)
	{
		EidosValue_SP element_value = wrappedValue->GetValueAtIndex(value_index, nullptr);
		
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
	}
	
	std::string &&out_string = outstream.str();
	NSString *outString = [NSString stringWithUTF8String:out_string.c_str()];
	
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



















































