//
//  EidosCocoaExtra.m
//  SLiM
//
//  Created by Ben Haller on 9/11/15.
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


#import "EidosCocoaExtra.h"

#include "eidos_call_signature.h"
#include "eidos_property_signature.h"


@implementation NSAttributedString (EidosAdditions)

+ (NSAttributedString *)eidosAttributedStringForCallSignature:(const EidosCallSignature *)signature
{
	if (signature)
	{
		//
		//	Note this logic is paralleled in the function operator<<(ostream &, const EidosCallSignature &).
		//	These two should be kept in synch so the user-visible format of signatures is consistent.
		//
		
		// Build an attributed string showing the call signature with syntax coloring for its parts
		NSMutableAttributedString *attrStr = [[[NSMutableAttributedString alloc] init] autorelease];
		
		NSString *prefixString = [NSString stringWithUTF8String:signature->CallPrefix().c_str()];	// "", "– ", or "+ "
		NSString *returnTypeString = [NSString stringWithUTF8String:StringForEidosValueMask(signature->return_mask_, signature->return_class_, "").c_str()];
		NSString *functionNameString = [NSString stringWithUTF8String:signature->function_name_.c_str()];
		
		NSDictionary *plainAttrs = [NSDictionary eidosOutputAttrs];
		NSDictionary *typeAttrs = [NSDictionary eidosInputAttrs];
		NSDictionary *functionAttrs = [NSDictionary eidosParseAttrs];
		NSDictionary *paramAttrs = [NSDictionary eidosPromptAttrs];
		
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:prefixString attributes:plainAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"(" attributes:plainAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:returnTypeString attributes:typeAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@")" attributes:plainAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:functionNameString attributes:functionAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"(" attributes:plainAttrs] autorelease]];
		
		int arg_mask_count = (int)signature->arg_masks_.size();
		
		if (arg_mask_count == 0)
		{
			if (!signature->has_ellipsis_)
				[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"void" attributes:typeAttrs] autorelease]];
		}
		else
		{
			for (int arg_index = 0; arg_index < arg_mask_count; ++arg_index)
			{
				EidosValueMask type_mask = signature->arg_masks_[arg_index];
				const std::string &arg_name = signature->arg_names_[arg_index];
				const EidosObjectClass *arg_obj_class = signature->arg_classes_[arg_index];
				
				if (arg_index > 0)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@", " attributes:plainAttrs] autorelease]];
				
				//
				//	Note this logic is paralleled in the function StringForEidosValueMask().
				//	These two should be kept in synch so the user-visible format of signatures is consistent.
				//
				bool is_optional = !!(type_mask & kEidosValueMaskOptional);
				bool requires_singleton = !!(type_mask & kEidosValueMaskSingleton);
				EidosValueMask stripped_mask = type_mask & kEidosValueMaskFlagStrip;
				
				if (is_optional)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"[" attributes:plainAttrs] autorelease]];
				
				if (stripped_mask == kEidosValueMaskNone)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"?" attributes:typeAttrs] autorelease]];
				else if (stripped_mask == kEidosValueMaskAny)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"*" attributes:typeAttrs] autorelease]];
				else if (stripped_mask == kEidosValueMaskAnyBase)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"+" attributes:typeAttrs] autorelease]];
				else if (stripped_mask == kEidosValueMaskNULL)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"void" attributes:typeAttrs] autorelease]];
				else if (stripped_mask == kEidosValueMaskLogical)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"logical" attributes:typeAttrs] autorelease]];
				else if (stripped_mask == kEidosValueMaskString)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"string" attributes:typeAttrs] autorelease]];
				else if (stripped_mask == kEidosValueMaskInt)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"integer" attributes:typeAttrs] autorelease]];
				else if (stripped_mask == kEidosValueMaskFloat)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"float" attributes:typeAttrs] autorelease]];
				else if (stripped_mask == kEidosValueMaskObject)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"object" attributes:typeAttrs] autorelease]];
				else if (stripped_mask == kEidosValueMaskNumeric)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"numeric" attributes:typeAttrs] autorelease]];
				else
				{
					if (stripped_mask & kEidosValueMaskNULL)
						[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"N" attributes:typeAttrs] autorelease]];
					if (stripped_mask & kEidosValueMaskLogical)
						[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"l" attributes:typeAttrs] autorelease]];
					if (stripped_mask & kEidosValueMaskInt)
						[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"i" attributes:typeAttrs] autorelease]];
					if (stripped_mask & kEidosValueMaskFloat)
						[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"f" attributes:typeAttrs] autorelease]];
					if (stripped_mask & kEidosValueMaskString)
						[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"s" attributes:typeAttrs] autorelease]];
					if (stripped_mask & kEidosValueMaskObject)
						[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"o" attributes:typeAttrs] autorelease]];
				}
				
				if (arg_obj_class && (stripped_mask & kEidosValueMaskObject))
				{
					NSString *objTypeName = [NSString stringWithUTF8String:arg_obj_class->ElementType().c_str()];
					
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"<" attributes:typeAttrs] autorelease]];
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:objTypeName attributes:typeAttrs] autorelease]];
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@">" attributes:typeAttrs] autorelease]];
				}
				
				if (requires_singleton)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"$" attributes:typeAttrs] autorelease]];
				
				if (arg_name.length() > 0)
				{
					NSString *argName = [NSString stringWithUTF8String:arg_name.c_str()];
					
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@" " attributes:plainAttrs] autorelease]];	// non-breaking space
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:argName attributes:paramAttrs] autorelease]];
				}
				
				if (is_optional)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"]" attributes:plainAttrs] autorelease]];
			}
		}
		
		if (signature->has_ellipsis_)
		{
			if (arg_mask_count > 0)
				[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@", " attributes:plainAttrs] autorelease]];
			
			[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"..." attributes:typeAttrs] autorelease]];
		}
		
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@")" attributes:plainAttrs] autorelease]];
		
		// if the function is provided by a delegate, show the delegate's name
		//p_outstream << p_signature.CallDelegate();
		
		[attrStr addAttribute:NSBaselineOffsetAttributeName value:[NSNumber numberWithFloat:2.0] range:NSMakeRange(0, [attrStr length])];
		
		return attrStr;
	}
	
	return [[[NSAttributedString alloc] init] autorelease];
}

+ (NSAttributedString *)eidosAttributedStringForPropertySignature:(const EidosPropertySignature *)signature
{
	if (signature)
	{
		//
		//	Note this logic is paralleled in the function operator<<(ostream &, const EidosPropertySignature &).
		//	These two should be kept in synch so the user-visible format of signatures is consistent.
		//
		
		// Build an attributed string showing the call signature with syntax coloring for its parts
		NSMutableAttributedString *attrStr = [[[NSMutableAttributedString alloc] init] autorelease];
		
		NSString *connectorString = [NSString stringWithUTF8String:signature->PropertySymbol().c_str()];	// "<–>" or "=>"
		NSString *valueTypeString = [NSString stringWithUTF8String:StringForEidosValueMask(signature->value_mask_, signature->value_class_, "").c_str()];
		NSString *propertyNameString = [NSString stringWithUTF8String:signature->property_name_.c_str()];
		
		NSDictionary *plainAttrs = [NSDictionary eidosOutputAttrs];
		NSDictionary *typeAttrs = [NSDictionary eidosInputAttrs];
		NSDictionary *functionAttrs = [NSDictionary eidosParseAttrs];
		NSDictionary *paramAttrs = [NSDictionary eidosPromptAttrs];
		
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:propertyNameString attributes:functionAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@" " attributes:plainAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:connectorString attributes:plainAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@" (" attributes:plainAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:valueTypeString attributes:typeAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@")" attributes:plainAttrs] autorelease]];
		
		[attrStr addAttribute:NSBaselineOffsetAttributeName value:[NSNumber numberWithFloat:2.0] range:NSMakeRange(0, [attrStr length])];
		
		return attrStr;
	}
	
	return [[[NSAttributedString alloc] init] autorelease];
}

@end


@implementation NSDictionary (EidosAdditions)

+ (NSDictionary *)eidosTextAttributesWithColor:(NSColor *)textColor
{
	static NSFont *menlo11Font = nil;
	static NSMutableParagraphStyle *paragraphStyle = nil;
	
	if (!menlo11Font)
		menlo11Font = [[NSFont fontWithName:@"Menlo" size:11.0] retain];
	
	if (!paragraphStyle)
	{
		paragraphStyle = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
		
		CGFloat tabInterval = [menlo11Font maximumAdvancement].width * 3;
		NSMutableArray *tabs = [NSMutableArray array];
		
		[paragraphStyle setDefaultTabInterval:tabInterval];
		
		for (int tabStop = 1; tabStop <= 20; ++tabStop)
			[tabs addObject:[[NSTextTab alloc] initWithTextAlignment:NSLeftTextAlignment location:tabInterval * tabStop options:nil]];
		
		[paragraphStyle setTabStops:tabs];
	}
	
	if (textColor)
		return @{NSForegroundColorAttributeName : textColor, NSFontAttributeName : menlo11Font, NSParagraphStyleAttributeName : paragraphStyle};
	else
		return @{NSFontAttributeName : menlo11Font, NSParagraphStyleAttributeName : paragraphStyle};
}

+ (NSDictionary *)eidosPromptAttrs
{
	static NSDictionary *promptAttrs = nil;
	
	if (!promptAttrs)
		promptAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:170/255.0 green:13/255.0 blue:145/255.0 alpha:1.0]] retain];
	
	return promptAttrs;
}

+ (NSDictionary *)eidosInputAttrs
{
	static NSDictionary *inputAttrs = nil;
	
	if (!inputAttrs)
		inputAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:28/255.0 green:0/255.0 blue:207/255.0 alpha:1.0]] retain];
	
	return inputAttrs;
}

+ (NSDictionary *)eidosOutputAttrs
{
	static NSDictionary *outputAttrs = nil;
	
	if (!outputAttrs)
		outputAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:0/255.0 green:0/255.0 blue:0/255.0 alpha:1.0]] retain];
	
	return outputAttrs;
}

+ (NSDictionary *)eidosErrorAttrs
{
	static NSDictionary *errorAttrs = nil;
	
	if (!errorAttrs)
		errorAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:196/255.0 green:26/255.0 blue:22/255.0 alpha:1.0]] retain];
	
	return errorAttrs;
}

+ (NSDictionary *)eidosTokensAttrs
{
	static NSDictionary *tokensAttrs = nil;
	
	if (!tokensAttrs)
		tokensAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:100/255.0 green:56/255.0 blue:32/255.0 alpha:1.0]] retain];
	
	return tokensAttrs;
}

+ (NSDictionary *)eidosParseAttrs
{
	static NSDictionary *parseAttrs = nil;
	
	if (!parseAttrs)
		parseAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:0/255.0 green:116/255.0 blue:0/255.0 alpha:1.0]] retain];
	
	return parseAttrs;
}

+ (NSDictionary *)eidosExecutionAttrs
{
	static NSDictionary *executionAttrs = nil;
	
	if (!executionAttrs)
		executionAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:63/255.0 green:110/255.0 blue:116/255.0 alpha:1.0]] retain];
	
	return executionAttrs;
}

+ (NSDictionary *)eidosBaseAttributes:(NSDictionary *)baseAttrs withHyperlink:(NSString *)link
{
	NSMutableDictionary *attrs = [NSMutableDictionary dictionaryWithDictionary:baseAttrs];
	
	[attrs setObject:link forKey:NSLinkAttributeName];
	
	return attrs;
}

@end


















































