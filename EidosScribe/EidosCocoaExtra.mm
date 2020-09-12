//
//  EidosCocoaExtra.m
//  SLiM
//
//  Created by Ben Haller on 9/11/15.
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


#import "EidosCocoaExtra.h"

#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_value.h"
#include "eidos_beep.h"


@implementation NSAttributedString (EidosAdditions)

+ (NSAttributedString *)eidosAttributedStringForCallSignature:(const EidosCallSignature *)signature size:(double)fontSize
{
	if (signature)
	{
		//
		//	Note this logic is paralleled in the function operator<<(ostream &, const EidosCallSignature &).
		//	These two should be kept in synch so the user-visible format of signatures is consistent.
		//
		
		// Build an attributed string showing the call signature with syntax coloring for its parts
		NSMutableAttributedString *attrStr = [[[NSMutableAttributedString alloc] init] autorelease];
		
		std::string &&prefix_string = signature->CallPrefix();
		NSString *prefixString = [NSString stringWithUTF8String:prefix_string.c_str()];	// "", "– ", or "+ "
		std::string &&return_type_string = StringForEidosValueMask(signature->return_mask_, signature->return_class_, "", nullptr);
		NSString *returnTypeString = [NSString stringWithUTF8String:return_type_string.c_str()];
		NSString *functionNameString = [NSString stringWithUTF8String:signature->call_name_.c_str()];
		
		NSDictionary *plainAttrs = [NSDictionary eidosOutputAttrsWithSize:fontSize];
		NSDictionary *typeAttrs = [NSDictionary eidosInputAttrsWithSize:fontSize];
		NSDictionary *functionAttrs = [NSDictionary eidosParseAttrsWithSize:fontSize];
		NSDictionary *paramAttrs = [NSDictionary eidosPromptAttrsWithSize:fontSize];
		
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:prefixString attributes:plainAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"(" attributes:plainAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:returnTypeString attributes:typeAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@")" attributes:plainAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:functionNameString attributes:functionAttrs] autorelease]];
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"(" attributes:plainAttrs] autorelease]];
		
		int arg_mask_count = (int)signature->arg_masks_.size();
		
		if (arg_mask_count == 0)
		{
			[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"void" attributes:typeAttrs] autorelease]];
		}
		else
		{
			for (int arg_index = 0; arg_index < arg_mask_count; ++arg_index)
			{
				EidosValueMask type_mask = signature->arg_masks_[arg_index];
				const std::string &arg_name = signature->arg_names_[arg_index];
				const EidosObjectClass *arg_obj_class = signature->arg_classes_[arg_index];
				EidosValue_SP arg_default = signature->arg_defaults_[arg_index];
				
				// skip private arguments
				if ((arg_name.length() >= 1) && (arg_name[0] == '_'))
					continue;
				
				if (arg_index > 0)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@", " attributes:plainAttrs] autorelease]];
				
				//
				//	Note this logic is paralleled in the function StringForEidosValueMask().
				//	These two should be kept in synch so the user-visible format of signatures is consistent.
				//
				if (arg_name == gEidosStr_ELLIPSIS)
				{
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"..." attributes:plainAttrs] autorelease]];
					continue;
				}
				
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
				else if (stripped_mask == kEidosValueMaskVOID)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"void" attributes:typeAttrs] autorelease]];
				else if (stripped_mask == kEidosValueMaskNULL)
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"NULL" attributes:typeAttrs] autorelease]];
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
					if (stripped_mask & kEidosValueMaskVOID)
						[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"v" attributes:typeAttrs] autorelease]];
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
					const std::string &obj_type_name = arg_obj_class->ElementType();
					NSString *objTypeName = [NSString stringWithUTF8String:obj_type_name.c_str()];
					
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
				{
					if (arg_default && (arg_default != gStaticEidosValueNULLInvisible.get()))
					{
						[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@" = " attributes:plainAttrs] autorelease]];
						
						std::ostringstream default_string_stream;
						
						arg_default->Print(default_string_stream);
						
						std::string &&default_string = default_string_stream.str();
						NSString *defaultString = [NSString stringWithUTF8String:default_string.c_str()];
						
						[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:defaultString attributes:plainAttrs] autorelease]];
					}
					
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"]" attributes:plainAttrs] autorelease]];
				}
			}
		}
		
		[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@")" attributes:plainAttrs] autorelease]];
		
		// if the function is provided by a delegate, show the delegate's name
		//p_outstream << p_signature.CallDelegate();
		
		[attrStr addAttribute:NSBaselineOffsetAttributeName value:[NSNumber numberWithFloat:2.0] range:NSMakeRange(0, [attrStr length])];
		
		return attrStr;
	}
	
	return [[[NSAttributedString alloc] init] autorelease];
}

+ (NSAttributedString *)eidosAttributedStringForPropertySignature:(const EidosPropertySignature *)signature size:(double)fontSize
{
	if (signature)
	{
		//
		//	Note this logic is paralleled in the function operator<<(ostream &, const EidosPropertySignature &).
		//	These two should be kept in synch so the user-visible format of signatures is consistent.
		//
		
		// Build an attributed string showing the call signature with syntax coloring for its parts
		NSMutableAttributedString *attrStr = [[[NSMutableAttributedString alloc] init] autorelease];
		
		std::string &&connector_string = signature->PropertySymbol();
		NSString *connectorString = [NSString stringWithUTF8String:connector_string.c_str()];	// "<–>" or "=>"
		std::string &&value_type_string = StringForEidosValueMask(signature->value_mask_, signature->value_class_, "", nullptr);
		NSString *valueTypeString = [NSString stringWithUTF8String:value_type_string.c_str()];
		NSString *propertyNameString = [NSString stringWithUTF8String:signature->property_name_.c_str()];
		
		NSDictionary *plainAttrs = [NSDictionary eidosOutputAttrsWithSize:fontSize];
		NSDictionary *typeAttrs = [NSDictionary eidosInputAttrsWithSize:fontSize];
		NSDictionary *functionAttrs = [NSDictionary eidosParseAttrsWithSize:fontSize];
		
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

@implementation NSMutableAttributedString (EidosAdditions)

// A shorthand to avoid having to construct autoreleased temporary attributed strings
- (void)eidosAppendString:(NSString *)str attributes:(NSDictionary<NSString *,id> *)attrs
{
	NSAttributedString *tempString = [[NSAttributedString alloc] initWithString:str attributes:attrs];
	
	[self appendAttributedString:tempString];
	[tempString release];
}

@end


@implementation NSDictionary (EidosAdditions)

+ (NSDictionary *)eidosTextAttributesWithColor:(NSColor *)textColor size:(double)fontSize
{
	static double menloFontSize = 0.0;
	static NSFont *menloFont = nil;
	static NSMutableParagraphStyle *paragraphStyle = nil;
	
	if (!menloFont || (menloFontSize != fontSize))
	{
		if (menloFont)
			[menloFont release];
		
		menloFont = [[NSFont fontWithName:@"Menlo" size:fontSize] retain];
	}
	
	if (!paragraphStyle)
	{
		if (paragraphStyle)
			[paragraphStyle release];
		
		paragraphStyle = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
		
		CGFloat tabInterval = [menloFont maximumAdvancement].width * 3;
		NSMutableArray *tabs = [NSMutableArray array];
		
		[paragraphStyle setDefaultTabInterval:tabInterval];
		
		for (int tabStop = 1; tabStop <= 20; ++tabStop)
			[tabs addObject:[[NSTextTab alloc] initWithTextAlignment:NSLeftTextAlignment location:tabInterval * tabStop options:@{}]];	// @{} suppresses a non-null warning, but should not be necessary; Apple header bug
		
		[paragraphStyle setTabStops:tabs];
	}
	
	menloFontSize = fontSize;
	
	if (textColor)
		return @{NSForegroundColorAttributeName : textColor, NSFontAttributeName : menloFont, NSParagraphStyleAttributeName : paragraphStyle};
	else
		return @{NSFontAttributeName : menloFont, NSParagraphStyleAttributeName : paragraphStyle};
}

+ (NSDictionary *)eidosPromptAttrsWithSize:(double)fontSize
{
	static double cachedFontSize = 0.0;
	static NSDictionary *promptAttrs = nil;
	
	if (!promptAttrs || (fontSize != cachedFontSize))
	{
		if (promptAttrs)
			[promptAttrs release];
		
		promptAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:170/255.0 green:13/255.0 blue:145/255.0 alpha:1.0] size:fontSize] retain];
	}
	
	return promptAttrs;
}

+ (NSDictionary *)eidosInputAttrsWithSize:(double)fontSize
{
	static double cachedFontSize = 0.0;
	static NSDictionary *inputAttrs = nil;
	
	if (!inputAttrs || (fontSize != cachedFontSize))
	{
		if (inputAttrs)
			[inputAttrs release];
		
		inputAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:28/255.0 green:0/255.0 blue:207/255.0 alpha:1.0] size:fontSize] retain];
	}
	
	return inputAttrs;
}

+ (NSDictionary *)eidosOutputAttrsWithSize:(double)fontSize
{
	static double cachedFontSize = 0.0;
	static NSDictionary *outputAttrs = nil;
	
	if (!outputAttrs || (fontSize != cachedFontSize))
	{
		if (outputAttrs)
			[outputAttrs release];
		
		outputAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:0/255.0 green:0/255.0 blue:0/255.0 alpha:1.0] size:fontSize] retain];
	}
	
	return outputAttrs;
}

+ (NSDictionary *)eidosErrorAttrsWithSize:(double)fontSize
{
	static double cachedFontSize = 0.0;
	static NSDictionary *errorAttrs = nil;
	
	if (!errorAttrs || (fontSize != cachedFontSize))
	{
		if (errorAttrs)
			[errorAttrs release];
		
		errorAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:196/255.0 green:26/255.0 blue:22/255.0 alpha:1.0] size:fontSize] retain];
	}
	
	return errorAttrs;
}

+ (NSDictionary *)eidosTokensAttrsWithSize:(double)fontSize
{
	static double cachedFontSize = 0.0;
	static NSDictionary *tokensAttrs = nil;
	
	if (!tokensAttrs || (fontSize != cachedFontSize))
	{
		if (tokensAttrs)
			[tokensAttrs release];
		
		tokensAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:100/255.0 green:56/255.0 blue:32/255.0 alpha:1.0] size:fontSize] retain];
	}
	
	return tokensAttrs;
}

+ (NSDictionary *)eidosParseAttrsWithSize:(double)fontSize
{
	static double cachedFontSize = 0.0;
	static NSDictionary *parseAttrs = nil;
	
	if (!parseAttrs || (fontSize != cachedFontSize))
	{
		if (parseAttrs)
			[parseAttrs release];
		
		parseAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:0/255.0 green:116/255.0 blue:0/255.0 alpha:1.0] size:fontSize] retain];
	}
	
	return parseAttrs;
}

+ (NSDictionary *)eidosExecutionAttrsWithSize:(double)fontSize
{
	static double cachedFontSize = 0.0;
	static NSDictionary *executionAttrs = nil;
	
	if (!executionAttrs || (fontSize != cachedFontSize))
	{
		if (executionAttrs)
			[executionAttrs release];
		
		executionAttrs = [[NSDictionary eidosTextAttributesWithColor:[NSColor colorWithCalibratedRed:63/255.0 green:110/255.0 blue:116/255.0 alpha:1.0] size:fontSize] retain];
	}
	
	return executionAttrs;
}

+ (NSDictionary *)eidosBaseAttributes:(NSDictionary *)baseAttrs withHyperlink:(NSString *)link
{
	NSMutableDictionary *attrs = [NSMutableDictionary dictionaryWithDictionary:baseAttrs];
	
	[attrs setObject:link forKey:NSLinkAttributeName];
	
	return attrs;
}

@end


@interface NSObject (EidosSplitViewExtensions)
- (void)respondToSizeChangeForSplitView:(NSSplitView *)splitView;
@end

@implementation NSSplitView (EidosAdditions)

- (void)eidosRestoreAutosavedPositionsWithName:(NSString *)autosaveName
{
	NSString *key = [NSString stringWithFormat:@"NSSplitView Subview Frames %@", autosaveName];
	NSArray *subviewFrames = [[NSUserDefaults standardUserDefaults] valueForKey:key];
	
	// the last frame is skipped because I have one less divider than I have frames
	for (NSUInteger i = 0; i < subviewFrames.count; i++)
	{
		if (i < self.subviews.count)	 // safety-check (in case number of views have been removed while dev)
		{
			// this is the saved frame data - it's an NSString
			NSString *frameString = subviewFrames[i];
			NSArray *components = [frameString componentsSeparatedByString:@", "];
			
			// Manage the 'hidden state' per view
			BOOL hidden = [components[4] boolValue];
			NSView *subView = [self subviews][i];
			[subView setHidden:hidden];
			
			// Set height (horizontal) or width (vertical)
			if (![self isVertical])		// BCH 4/7/2016: vertical property not available in 10.9
			{
				CGFloat height = [components[3] floatValue];
				[subView setFrameSize:NSMakeSize(subView.frame.size.width, height)];
			}
			else
			{
				CGFloat width = [components[2] floatValue];
				[subView setFrameSize:NSMakeSize(width, subView.frame.size.height)];
			}
		}
	}
	
	// Notify our delegate of the resize.  Note that this is not an NSSplitViewDelegate method; we're doing our own thing here
	NSObject *delegate = [self delegate];
	
	if (delegate)
	{
		if ([delegate respondsToSelector:@selector(respondToSizeChangeForSplitView:)])
			[delegate respondToSizeChangeForSplitView:self];
	}
}

@end


@implementation NSTextField (EidosAdditions)

- (void)eidosSetHyperlink:(NSURL *)url onText:(NSString *)text
{
	NSMutableAttributedString *attrString = [[self attributedStringValue] mutableCopy];
	NSString *string = [attrString string];
	NSRange range = [string rangeOfString:text];
	
	if (range.location != NSNotFound)
	{
		// Apple sez: both are needed, otherwise hyperlink won't accept mousedown
		[self setAllowsEditingTextAttributes: YES];
		[self setSelectable: YES];
		
		// Add the link attribute
		[attrString beginEditing];
		
		[attrString addAttribute:NSLinkAttributeName value:[url absoluteString] range:range];																// link
		[attrString addAttribute:NSForegroundColorAttributeName value:[NSColor colorWithCalibratedRed:0.0 green:0.0 blue:0.7 alpha:1.0] range:range];		// dark blue
		[attrString addAttribute:NSUnderlineStyleAttributeName value:[NSNumber numberWithInt:NSUnderlineStyleSingle] range:range];							// underlined
		
		[attrString endEditing];
		
		[self setAttributedStringValue:attrString];
	}
	
	[attrString release];
}

@end


// Eidos_Beep() is declared in eidos_beep.h, with a default implementation; in the GUI case (EidosScribe and SLiMgui)
// it is redefined here, because we want to be able to use Objective-C and Cocoa.
std::string Eidos_Beep_MACOS(std::string p_sound_name)
{
	NSString *soundName = [NSString stringWithUTF8String:p_sound_name.c_str()];
	
	if (!soundName || ![soundName length])
		soundName = @"Pop";
	
	static NSSound *cachedSound = nil;
	static NSTimeInterval cachedSoundDuration = 0;
	static NSString *cachedSoundName = nil;
	static NSDate *playFinishDate = nil;
	
	std::string return_string;
	
	// If playFinishDate is non-nil, we started playing a sound before, and we need to wait synchronously here for it to finish
	if (playFinishDate)
	{
		NSTimeInterval remainingTime = [playFinishDate timeIntervalSinceNow];
		
		if (remainingTime > 0)
			usleep((useconds_t)(remainingTime * 1000000.0));
		
		[playFinishDate release];
		playFinishDate = nil;
	}
	
	// If cachedSound is non-nil, we played it in the past, and we should now make sure that it is stopped, otherwise it ignores -play
	if (cachedSound)
		[cachedSound stop];
	
	// Now we switch cachedSound over to the requested sound
	if (!cachedSound || ![soundName isEqualToString:cachedSoundName])
	{
		[cachedSound release];
		[cachedSoundName release];
		
		cachedSound = [[NSSound soundNamed:soundName] retain];
		
		// If the requested sound did not exist, we fall back on the default, which ought to always exist
		if (!cachedSound)
		{
			return_string = "#WARNING (Eidos_Beep): function beep() could not find the requested sound.";
			
			soundName = @"Pop";
			cachedSound = [[NSSound soundNamed:soundName] retain];
		}
		
		cachedSoundName = [soundName retain];
		cachedSoundDuration = [cachedSound duration];
	}
	
	// Start playing our sound.  We return immediately, but we make a note of when we expect the sound to stop playing.  If we get called
	// again to play a sound before the current one has finished, we will synchronously wait the remaining time, above.
	[cachedSound play];
	playFinishDate = [[NSDate dateWithTimeIntervalSinceNow:cachedSoundDuration] retain];
	
	return return_string;
}


@implementation NSString (EidosAdditions)

- (int64_t)eidosScoreAsCompletionOfString:(NSString *)base
{
	// Evaluate the quality of the target as a completion for completionBase and return a score.
	// We look for each character of completionBase in self, in order, case-insensitive; all
	// characters must be present in order for the target to be a completion at all.  Beyond that,
	// a higher score is garnered if the matches in self are (1) either uppercase or the 0th character,
	// and (2) if they are relatively near the beginning, and (3) if they occur contiguously.
	int64_t score = 0;
	NSUInteger selfLength = [self length], baseLength = [base length];
	
	// Do the comparison scan; find a match for each composed character sequence in base.  We work
	// with composed character sequences and use rangeOfString: to do searches, to avoid issues with
	// diacritical marks, alternative composition sequences, casing, etc.
	NSUInteger firstUnusedIndex = 0, firstUnmatchedIndex = 0;
	
	do
	{
		NSRange baseRangeToMatch = [base rangeOfComposedCharacterSequenceAtIndex:firstUnmatchedIndex];
		NSString *stringToMatch = [base substringWithRange:baseRangeToMatch];
		NSString *uppercaseStringToMatch = [stringToMatch uppercaseString];
		NSRange selfMatchRange;
		
		if ([stringToMatch isEqualToString:uppercaseStringToMatch] && (firstUnmatchedIndex != 0))
		{
			// If the character in base is uppercase, we only want to match an uppercase character in self.
			// The exception is the first character of base; WTF should match writeTempFile() well.
			selfMatchRange = [self rangeOfString:stringToMatch
										 options:(NSDiacriticInsensitiveSearch | NSWidthInsensitiveSearch)
										   range:NSMakeRange(firstUnusedIndex, selfLength - firstUnusedIndex)];
			score += 1000;	// uppercase match
		}
		else
		{
			// If the character in base is not uppercase, we will match any case in self, but we prefer a
			// lowercase character if it matches the very next part of self, otherwise we prefer uppercase.
			selfMatchRange = [self rangeOfString:stringToMatch
										 options:(NSDiacriticInsensitiveSearch | NSWidthInsensitiveSearch)
										   range:NSMakeRange(firstUnusedIndex, selfLength - firstUnusedIndex)];
			
			if (selfMatchRange.location == firstUnusedIndex)
			{
				score += 2000;	// next-character match is even better than upper-case; continuity trumps camelcase
			}
			else
			{
				NSRange uppercaseMatchRange = [self rangeOfString:uppercaseStringToMatch
														  options:(NSDiacriticInsensitiveSearch | NSWidthInsensitiveSearch)
															range:NSMakeRange(firstUnusedIndex, selfLength - firstUnusedIndex)];
				
				if (uppercaseMatchRange.location != NSNotFound)
				{
					selfMatchRange = uppercaseMatchRange;
					score += 1000;	// uppercase match
				}
				else if (firstUnusedIndex > 0)
				{
					// This match is crap; we're jumping forward to a lowercase letter, so it's unlikely to be what
					// the user wants.  So we bail.  This can be commented out to return lower-quality matches.
					return INT64_MIN;
				}
			}
		}
		
		// no match in self for the composed character sequence in base; self is not a good completion of base
		if (selfMatchRange.location == NSNotFound)
			return INT64_MIN;
		
		// matching the very beginning of self is very good; we really want to match the start of a candidate
		// otherwise, earlier matches are better; a match at position 0 gets the largest score increment
		if (selfMatchRange.location == 0)
			score += 100000;
		else
			score -= selfMatchRange.location;
		
		// move firstUnusedIndex to follow the matched range in self
		firstUnusedIndex = selfMatchRange.location + selfMatchRange.length;
		
		// move to the next composed character sequence in base
		firstUnmatchedIndex = baseRangeToMatch.location + baseRangeToMatch.length;
		if (firstUnmatchedIndex >= baseLength)
			break;
	}
	while (YES);
	
	// We want argument-name matches to be at the top, always, when they are available, so bump their score
	if ([self hasSuffix:@"="])
		score += 1000000;
	
	return score;
}

@end












































