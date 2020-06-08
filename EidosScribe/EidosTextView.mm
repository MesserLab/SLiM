//
//  EidosTextView.mm
//  EidosScribe
//
//  Created by Ben Haller on 6/14/15.
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


#import "EidosTextView.h"
#import "EidosTextViewDelegate.h"
#import "EidosConsoleTextView.h"
#import "EidosCocoaExtra.h"
#import "EidosHelpController.h"

#include "eidos_script.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_type_table.h"
#include "eidos_type_interpreter.h"

#include <stdexcept>
#include <memory>


//	EidosTextStorage – a little subclass to make word selection in EidosTextView work the way it should, defined below
@interface EidosTextStorage : NSTextStorage
- (id)init;
- (id)initWithAttributedString:(NSAttributedString *)attrStr NS_DESIGNATED_INITIALIZER;
@end


@interface EidosTextView () <NSTextStorageDelegate>
{
	// these are used in selectionRangeForProposedRange:granularity: to balance delimiters properly
	BOOL inEligibleDoubleClick;
	NSTimeInterval doubleDownTime;
}
@end

@implementation EidosTextView

- (instancetype)initWithFrame:(NSRect)frameRect textContainer:(NSTextContainer *)aTextContainer
{
	if (self = [super initWithFrame:frameRect textContainer:aTextContainer])
	{
		_displayFontSize = 11;
		_shouldRecolorAfterChanges = YES;
	}
	
	return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
	if (self = [super initWithCoder:coder])
	{
		_displayFontSize = 11;
		_shouldRecolorAfterChanges = YES;
	}
	
	return self;
}

- (void)awakeFromNib
{
	// Replace the text storage of the description textview with our custom subclass
	NSLayoutManager *lm = [self layoutManager];
	NSTextStorage *ts = [self textStorage];
	EidosTextStorage *replacementTextStorage = [[EidosTextStorage alloc] initWithAttributedString:ts];	// copy any existing text
	
	[lm replaceTextStorage:replacementTextStorage];
	[replacementTextStorage setDelegate:self];
	[replacementTextStorage release];
	
	// Turn off all of Cocoa's fancy text editing stuff
	[self setAutomaticDashSubstitutionEnabled:NO];
	[self setAutomaticDataDetectionEnabled:NO];
	[self setAutomaticLinkDetectionEnabled:NO];
	[self setAutomaticQuoteSubstitutionEnabled:NO];
	[self setAutomaticSpellingCorrectionEnabled:NO];
	[self setAutomaticTextReplacementEnabled:NO];
	[self setContinuousSpellCheckingEnabled:NO];
	[self setGrammarCheckingEnabled:NO];
	[self turnOffLigatures:nil];
	
	// Fix the font and typing attributes
	[self setFont:[NSFont fontWithName:@"Menlo" size:_displayFontSize]];
	[self setTypingAttributes:[NSDictionary eidosTextAttributesWithColor:nil size:_displayFontSize]];
	
	// Fix text container insets to look a bit nicer; {0,0} by default
	[self setTextContainerInset:NSMakeSize(0.0, 5.0)];
}

// The method signature is inherited from NSTextView, but we want to check that the delegate follows our delegate protocol
- (void)setDelegate:(id<NSTextViewDelegate>)delegate
{
	if (delegate && ![delegate conformsToProtocol:@protocol(EidosTextViewDelegate)])
		NSLog(@"Delegate %@ assigned to EidosTextView %p does not conform to the EidosTextViewDelegate protocol!", delegate, self);
	
	[super setDelegate:delegate];
}

// handle flashing of matching delimiters
- (void)insertText:(id)insertString replacementRange:(NSRange)replacementRange
{
	//bool replacingSelection = NSEqualRanges(replacementRange, [self selectedRange]);
	
	[super insertText:insertString replacementRange:replacementRange];
	
	// if we replaced something other than the selected range, something weird is going on and we shouldn't flash
	// this is commented out because it doesn't work; AppKit likes to call this method with {NSNotFound,0} for typing...
	//if (!replacingSelection)
	//	return;
	
	// if the insert string isn't one character in length, it cannot be a brace character
	if ([insertString length] != 1)
		return;
	
	unichar uch = [insertString characterAtIndex:0];
	
	if ((uch == '}') || (uch == ']') || (uch == ')'))
	{
		// MAINTENANCE NOTE: This also exists, in slightly altered form, in selectionRangeForProposedRange:granularity:, so please maintain the two in parallel!
		NSString *scriptString = [[self textStorage] string];
		int stringLength = (int)[scriptString length];
		int proposedCharacterIndex = (int)[self selectedRange].location - 1;	// just inserted a single character
		BOOL forward = false;
		unichar incrementChar = ' ';
		unichar decrementChar = ' ';
		EidosTokenType token1 = EidosTokenType::kTokenNone;
		EidosTokenType token2 = EidosTokenType::kTokenNone;
		
		// Check for any of the delimiter types we support, and set up to do a scan
		if (uch == '}')		{ forward = false; incrementChar = '}'; decrementChar = '{'; token1 = EidosTokenType::kTokenLBrace; token2 = EidosTokenType::kTokenRBrace; }
		if (uch == ')')		{ forward = false; incrementChar = ')'; decrementChar = '('; token1 = EidosTokenType::kTokenLParen; token2 = EidosTokenType::kTokenRParen; }
		if (uch == ']')		{ forward = false; incrementChar = ']'; decrementChar = '['; token1 = EidosTokenType::kTokenLBracket; token2 = EidosTokenType::kTokenRBracket; }
		
		if (token1 != EidosTokenType::kTokenNone)
		{
			// We've got a double-click on a character that we want to balance-select.  This is complicated mostly
			// because of strings, which are a pain in the butt.  To simplify that issue, we tokenize and search
			// in the token stream parallel to searching in the text.
			std::string script_string([scriptString UTF8String]);
			EidosScript script(script_string);
			
			// Tokenize
			script.Tokenize(true, true);	// make bad tokens as needed, keep nonsignificant tokens
			
			// Find the token containing the double-click point
			const std::vector<EidosToken> &tokens = script.Tokens();
			int token_count = (int)tokens.size();
			const EidosToken *click_token = nullptr;
			int click_token_index;
			
			for (click_token_index = 0; click_token_index < token_count; ++click_token_index)
			{
				click_token = &tokens[click_token_index];
				
				if ((click_token->token_UTF16_start_ <= proposedCharacterIndex) && (click_token->token_UTF16_end_ >= proposedCharacterIndex))
					break;
			}
			
			if (!click_token)
				return;
			
			// OK, this token contains the character that was double-clicked.  We could have the actual token
			// (incrementToken), we could be in a string, or we could be in a comment.  We stay in the domain
			// that we start in; if in a string, for example, only delimiters within strings affect our scan.
			EidosTokenType click_token_type = click_token->token_type_;
			const EidosToken *scan_token = click_token;
			int scan_token_index = click_token_index;
			NSInteger scanPosition = proposedCharacterIndex;
			int balanceCount = 0;
			
			while (YES)
			{
				EidosTokenType scan_token_type = scan_token->token_type_;
				BOOL isCandidate;
				
				if (click_token_type == EidosTokenType::kTokenComment)
					isCandidate = (scan_token_type == EidosTokenType::kTokenComment);
				else if (click_token_type == EidosTokenType::kTokenCommentLong)
					isCandidate = (scan_token_type == EidosTokenType::kTokenCommentLong);
				else if (click_token_type == EidosTokenType::kTokenString)
					isCandidate = (scan_token_type == EidosTokenType::kTokenString);
				else
					isCandidate = ((scan_token_type == token1) || (scan_token_type == token2));
				
				if (isCandidate)
				{
					uch = [scriptString characterAtIndex:scanPosition];
					
					if (uch == incrementChar)
						balanceCount++;
					else if (uch == decrementChar)
						balanceCount--;
					
					// If balanceCount is now zero, all opens have been balanced by closes, and we have found our matching delimiter
					if (balanceCount == 0)
					{
						[self showFindIndicatorForRange:NSMakeRange(scanPosition, 1)];
						return;
					}
				}
				
				// Advance, forward or backward, to the next character
				scanPosition += (forward ? 1 : -1);
				if ((scanPosition == stringLength) || (scanPosition == -1))
				{
					//NSBeep();
					return;
				}
				
				// Make sure we're in the right token
				while ((scan_token->token_UTF16_start_ > scanPosition) || (scan_token->token_UTF16_end_ < scanPosition))
				{
					scan_token_index += (forward ? 1 : -1);
					
					if ((scan_token_index < 0) || (scan_token_index == token_count))
					{
						NSLog(@"ran out of tokens!");
						//NSBeep();
						return;
					}
					
					scan_token = &tokens[scan_token_index];
				}
			}
		}
	}
}

// handle autoindent by matching the whitespace beginning the current line
- (void)insertNewline:(id)sender
{
	NSString *textString = [self string];
	NSUInteger selectionStart = [self selectedRange].location;
	NSCharacterSet *newlineChars = [NSCharacterSet newlineCharacterSet];
	NSCharacterSet *whitespaceChars = [NSCharacterSet whitespaceCharacterSet];
	
	// start at the start of the selection and move backwards to the beginning of the line
	NSUInteger lineStart = selectionStart;
	
	while (lineStart > 0)
	{
		unichar ch = [textString characterAtIndex:lineStart - 1];
		
		if ([newlineChars characterIsMember:ch])
			break;
		
		--lineStart;
	}
	
	// now we're either at the beginning of the content, or the beginning of the line; now find the end of the whitespace there, up to where we started
	NSUInteger whitespaceEnd = lineStart;
	
	while (whitespaceEnd < selectionStart)
	{
		unichar ch = [textString characterAtIndex:whitespaceEnd];
		
		if (![whitespaceChars characterIsMember:ch])
			break;
		
		++whitespaceEnd;
	}
	
	// now we have the range of the leading whitespace; copy that, call super to insert the newline, and then paste in the whitespace
	NSRange whitespaceRange = NSMakeRange(lineStart, whitespaceEnd - lineStart);
	NSString *whitespaceString = [textString substringWithRange:whitespaceRange];
	
	// We use the insert... methods, which handle change notifications and undo correctly
	[super insertNewline:sender];
	[self insertText:whitespaceString replacementRange:NSMakeRange(NSNotFound, NSNotFound)];
}

// handle the home and end keys; rather bizarre that Apple doesn't implement these for us, but whatever...
// OK, Apple *does* implement these, but apparently sometimes their implementation does nothing.  I haven't figured out a reproducible case to make that
// happen, but it does happen, and when it does, this fixes it.  Overriding these methods seems quite harmless, so I'll leave this code in here.
- (void)scrollToBeginningOfDocument:(id)sender
{
	[self scrollRangeToVisible:NSMakeRange(0, 0)];
}
- (void)scrollToEndOfDocument:(id)sender
{
	[self scrollRangeToVisible:NSMakeRange([[self string] length], 0)];
}

// NSTextView copies only plain text for us, because it is set to have rich text turned off.  That setting only means it is turned off for the user; the
// user can't change the font, size, etc.  But we still can, and do, programatically do our syntax formatting.  We want that style information to get
// copied to the pasteboard.  This has gotten even trickier now that our syntax coloring is done with temporary attributes; we need to translate those
// into real attributes here.
- (NSMutableAttributedString *)eidosAttrStringForSelectedRange
{
	NSAttributedString *attrString = [self textStorage];
	NSRange selectedRange = [self selectedRange];
	NSMutableAttributedString *attrStringInRange = [[[attrString attributedSubstringFromRange:selectedRange] mutableCopy] autorelease];
	
	// Loop over the temporary color attributes and set them on the attributed string as real attributes
	NSLayoutManager *lm = [self layoutManager];
	NSRange effectiveRange = NSMakeRange(0, 0);
	NSUInteger charIndex = selectedRange.location;
	
	while (charIndex < selectedRange.location + selectedRange.length)
	{
		NSColor *color = [lm temporaryAttribute:NSForegroundColorAttributeName atCharacterIndex:charIndex longestEffectiveRange:&effectiveRange inRange:selectedRange];
		
		if (color)
			[attrStringInRange addAttribute:NSForegroundColorAttributeName value:color range:NSMakeRange(effectiveRange.location - selectedRange.location, effectiveRange.length)];
		
		charIndex = effectiveRange.location + effectiveRange.length;
	}
	
	return attrStringInRange;
}

- (IBAction)copy:(id)sender
{
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	NSAttributedString *attrStringInRange = [self eidosAttrStringForSelectedRange];
	
	// The documentation sucks, but as far as I can tell, this puts both a plain-text and a rich-text representation on the pasteboard
	[pasteboard clearContents];
	[pasteboard writeObjects:@[attrStringInRange]];
}

- (IBAction)copyAsParagraph:(id)sender
{
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	NSMutableAttributedString *attrStringInRange = [self eidosAttrStringForSelectedRange];
	NSString *originalString = [attrStringInRange string];
	unichar lineBreakChar = NSLineSeparatorCharacter;
	NSString *lineSeparator = [NSString stringWithCharacters:&lineBreakChar length:1];
	
	// Replace new paragraphs (\n) with linebreaks (NSLineSeparatorCharacter) except at the very end (thus -2)
	// We do this by very dumb brute force, for now; doesn't matter
	for (int i = (int)[attrStringInRange length] - 2; i >= 0; --i)
	{
		unichar ch = [originalString characterAtIndex:i];
		
		if (ch == '\n')
			[attrStringInRange replaceCharactersInRange:NSMakeRange(i, 1) withString:lineSeparator];
	}
	
	// The documentation sucks, but as far as I can tell, this puts both a plain-text and a rich-text representation on the pasteboard
	[pasteboard clearContents];
	[pasteboard writeObjects:@[attrStringInRange]];
}

- (IBAction)paste:(id)sender
{
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	NSString *pbString = nil;
	
	if ([pasteboard canReadObjectForClasses:@[[NSString class]] options:nil])
		pbString = [[pasteboard readObjectsForClasses:@[[NSString class]] options:nil] objectAtIndex:0];
	else if ([pasteboard canReadObjectForClasses:@[[NSAttributedString class]] options:nil])
		pbString = [(NSAttributedString *)[[pasteboard readObjectsForClasses:@[[NSAttributedString class]] options:nil] objectAtIndex:0] string];
	
	if (pbString)
	{
		// This is the point of this override: to standardize the line endings present
		pbString = [pbString stringByReplacingOccurrencesOfString:@"\n\r" withString:@"\n"];
		pbString = [pbString stringByReplacingOccurrencesOfString:@"\r\n" withString:@"\n"];
		pbString = [pbString stringByReplacingOccurrencesOfString:@"\r" withString:@"\n"];
		pbString = [pbString stringByReplacingOccurrencesOfString:@"\U00002028" withString:@"\n"];	// NSLineSeparatorCharacter
		pbString = [pbString stringByReplacingOccurrencesOfString:@"\U00002029" withString:@"\n"];	// NSParagraphSeparatorCharacter
		
		[self insertText:pbString replacementRange:[self selectedRange]];
	}
	else
		[super paste:sender];
}

- (IBAction)pasteAsRichText:(id)sender
{
	[self paste:sender];
}

- (IBAction)pasteAsPlainText:(id)sender
{
	[self paste:sender];
}
	
- (void)undoRedoSelectionRange:(NSRange)range
{
	[[[self undoManager] prepareWithInvocationTarget:self] undoRedoSelectionRange:range];
	
	[self setSelectedRange: range];
}

- (IBAction)shiftSelectionLeft:(id)sender
{
	if ([self isEditable])
	{
		NSTextStorage *ts = [self textStorage];
		NSMutableString *scriptString = [[self string] mutableCopy];
		NSUInteger scriptLength = [scriptString length];
		NSRange selectedRange = [self selectedRange];
		NSCharacterSet *newlineChars = [NSCharacterSet newlineCharacterSet];
		NSUInteger scanPosition;
		
		// start at the start of the selection and scan backwards over non-newline text until we hit a newline or the start of the file
		scanPosition = selectedRange.location;
		
		while (scanPosition > 0)
		{
			if ([newlineChars characterIsMember:[scriptString characterAtIndex:scanPosition - 1]])
				break;
			
			--scanPosition;
		}
		
		// we want to recolor only once, at the end of the whole operation
		[self setShouldRecolorAfterChanges:NO];
		
		// save the current selection so undo restores it
		[[[self undoManager] prepareWithInvocationTarget:self] undoRedoSelectionRange:selectedRange];
		
		// ok, we're at the start of the line that the selection starts on; start removing tabs
		[ts beginEditing];
		
		while ((scanPosition == selectedRange.location) || (scanPosition < selectedRange.location + selectedRange.length))
		{
			// if we are at the very end of the script string, then we have hit the end and we're done
			if (scanPosition == scriptLength)
				break;
			
			// insert a tab at the start of this line and adjust our selection
			if ([scriptString characterAtIndex:scanPosition] == '\t')
			{
				NSRange changeRange = NSMakeRange(scanPosition, 1);
				
				if ([self shouldChangeTextInRange:changeRange replacementString:@""])
				{
					[ts replaceCharactersInRange:changeRange withString:@""];
					[self didChangeText];
					
					[scriptString replaceCharactersInRange:changeRange withString:@""];
					scriptLength--;
					
					if (scanPosition < selectedRange.location)
						selectedRange.location--;
					else
						if (selectedRange.length > 0)
							selectedRange.length--;
				}
			}
			
			// now scan forward to the end of this line
			while (scanPosition < scriptLength)
			{
				if ([newlineChars characterIsMember:[scriptString characterAtIndex:scanPosition]])
					break;
				
				++scanPosition;
			}
			
			// and then scan forward to the beginning of the next line
			while (scanPosition < scriptLength)
			{
				if (![newlineChars characterIsMember:[scriptString characterAtIndex:scanPosition]])
					break;
				
				++scanPosition;
			}
		}
		
		[scriptString release];
		[ts endEditing];
		
		// set the new selection in a way that will work with redo
		[[[self undoManager] prepareWithInvocationTarget:self] undoRedoSelectionRange:selectedRange];
		[self setSelectedRange:selectedRange];
		
		// recolor, coalesced
		[self setShouldRecolorAfterChanges:YES];
		[self recolorAfterChanges];
	}
	else
	{
		NSBeep();
	}
}

- (IBAction)shiftSelectionRight:(id)sender
{
	if ([self isEditable])
	{
		NSTextStorage *ts = [self textStorage];
		NSMutableString *scriptString = [[self string] mutableCopy];
		NSUInteger scriptLength = [scriptString length];
		NSRange selectedRange = [self selectedRange];
		NSCharacterSet *newlineChars = [NSCharacterSet newlineCharacterSet];
		NSUInteger scanPosition;
		
		// start at the start of the selection and scan backwards over non-newline text until we hit a newline or the start of the file
		scanPosition = selectedRange.location;
		
		while (scanPosition > 0)
		{
			if ([newlineChars characterIsMember:[scriptString characterAtIndex:scanPosition - 1]])
				break;
			
			--scanPosition;
		}
		
		// we want to recolor only once, at the end of the whole operation
		[self setShouldRecolorAfterChanges:NO];
		
		// save the current selection so undo restores it
		[[[self undoManager] prepareWithInvocationTarget:self] undoRedoSelectionRange:selectedRange];
		
		// ok, we're at the start of the line that the selection starts on; start inserting tabs
		[ts beginEditing];
		
		while ((scanPosition == selectedRange.location) || (scanPosition < selectedRange.location + selectedRange.length))
		{
			// insert a tab at the start of this line and adjust our selection
			NSRange changeRange = NSMakeRange(scanPosition, 0);
			
			if ([self shouldChangeTextInRange:changeRange replacementString:@"\t"])
			{
				[ts replaceCharactersInRange:changeRange withString:@"\t"];
				[self didChangeText];
				
				[scriptString replaceCharactersInRange:changeRange withString:@"\t"];
				scriptLength++;
				
				if ((scanPosition < selectedRange.location) || (selectedRange.length == 0))
					selectedRange.location++;
				else
					selectedRange.length++;
			}
			
			// now scan forward to the end of this line
			while (scanPosition < scriptLength)
			{
				if ([newlineChars characterIsMember:[scriptString characterAtIndex:scanPosition]])
					break;
				
				++scanPosition;
			}
			
			// and then scan forward to the beginning of the next line
			while (scanPosition < scriptLength)
			{
				if (![newlineChars characterIsMember:[scriptString characterAtIndex:scanPosition]])
					break;
				
				++scanPosition;
			}
			
			// if we are at the very end of the script string, then we have hit the end and we're done
			if (scanPosition == scriptLength)
				break;
		}
		
		[scriptString release];
		[ts endEditing];
		
		// set the new selection in a way that will work with redo
		[[[self undoManager] prepareWithInvocationTarget:self] undoRedoSelectionRange:selectedRange];
		[self setSelectedRange:selectedRange];
		
		// recolor, coalesced
		[self setShouldRecolorAfterChanges:YES];
		[self recolorAfterChanges];
	}
	else
	{
		NSBeep();
	}
}

- (IBAction)commentUncommentSelection:(id)sender
{
	if ([self isEditable])
	{
		NSTextStorage *ts = [self textStorage];
		NSMutableString *scriptString = [[self string] mutableCopy];
		NSUInteger scriptLength = [scriptString length];
		NSRange selectedRange = [self selectedRange];
		NSCharacterSet *newlineChars = [NSCharacterSet newlineCharacterSet];
		NSUInteger scanPosition;
		
		// start at the start of the selection and scan backwards over non-newline text until we hit a newline or the start of the file
		scanPosition = selectedRange.location;
		
		while (scanPosition > 0)
		{
			if ([newlineChars characterIsMember:[scriptString characterAtIndex:scanPosition - 1]])
				break;
			
			--scanPosition;
		}
		
		// decide whether we are commenting or uncommenting; we are only uncommenting if every line spanned by the selection starts with "//"
		BOOL uncommenting = YES;
		NSUInteger scanPositionSave = scanPosition;
		
		while ((scanPosition == selectedRange.location) || (scanPosition < selectedRange.location + selectedRange.length))
		{
			// comment/uncomment at the start of this line and adjust our selection
			if ((scanPosition + 1 >= scriptLength) || ([scriptString characterAtIndex:scanPosition] != '/') || ([scriptString characterAtIndex:scanPosition + 1] != '/'))
			{
				uncommenting = NO;
				break;
			}
			
			// now scan forward to the end of this line
			while (scanPosition < scriptLength)
			{
				if ([newlineChars characterIsMember:[scriptString characterAtIndex:scanPosition]])
					break;
				
				++scanPosition;
			}
			
			// and then scan forward to the beginning of the next line
			while (scanPosition < scriptLength)
			{
				if (![newlineChars characterIsMember:[scriptString characterAtIndex:scanPosition]])
					break;
				
				++scanPosition;
			}
			
			// if we are at the very end of the script string, then we have hit the end and we're done
			if (scanPosition == scriptLength)
				break;
		}
		
		scanPosition = scanPositionSave;
		
		// we want to recolor only once, at the end of the whole operation
		[self setShouldRecolorAfterChanges:NO];
		
		// save the current selection so undo restores it
		[[[self undoManager] prepareWithInvocationTarget:self] undoRedoSelectionRange:selectedRange];
		
		// ok, we're at the start of the line that the selection starts on; start commenting / uncommenting
		[ts beginEditing];
		
		while ((scanPosition == selectedRange.location) || (scanPosition < selectedRange.location + selectedRange.length))
		{
			// if we are at the very end of the script string, then we have hit the end and we're done
			if (uncommenting && (scanPosition == scriptLength))
				break;
			
			// comment/uncomment at the start of this line and adjust our selection
			if (uncommenting)
			{
				NSRange changeRange = NSMakeRange(scanPosition, 2);
				
				if ([self shouldChangeTextInRange:changeRange replacementString:@""])
				{
					[ts replaceCharactersInRange:changeRange withString:@""];
					[self didChangeText];
					
					[scriptString replaceCharactersInRange:changeRange withString:@""];
					scriptLength -= 2;
					
					if (scanPosition < selectedRange.location)
					{
						if (scanPosition == selectedRange.location - 1)
						{
							selectedRange.location--;
							if (selectedRange.length > 0)
								selectedRange.length--;
						}
						else
							selectedRange.location -= 2;
					}
					else
					{
						if (selectedRange.length > 2)
							selectedRange.length -= 2;
						else
							selectedRange.length = 0;
					}
				}
			}
			else
			{
				NSRange changeRange = NSMakeRange(scanPosition, 0);
				
				if ([self shouldChangeTextInRange:changeRange replacementString:@"//"])
				{
					[ts replaceCharactersInRange:changeRange withString:@"//"];
					//[ts setAttributes:[NSDictionary eidosTextAttributesWithColor:[NSColor blackColor]] range:NSMakeRange(scanPosition, 2)];
					[self didChangeText];
					
					[scriptString replaceCharactersInRange:changeRange withString:@"//"];
					scriptLength += 2;
					
					if ((scanPosition < selectedRange.location) || (selectedRange.length == 0))
						selectedRange.location += 2;
					else
						selectedRange.length += 2;
				}
			}
			
			// now scan forward to the end of this line
			while (scanPosition < scriptLength)
			{
				if ([newlineChars characterIsMember:[scriptString characterAtIndex:scanPosition]])
					break;
				
				++scanPosition;
			}
			
			// and then scan forward to the beginning of the next line
			while (scanPosition < scriptLength)
			{
				if (![newlineChars characterIsMember:[scriptString characterAtIndex:scanPosition]])
					break;
				
				++scanPosition;
			}
			
			// if we are at the very end of the script string, then we have hit the end and we're done
			if (!uncommenting && (scanPosition == scriptLength))
				break;
		}
		
		[scriptString release];
		[ts endEditing];
		
		// set the new selection in a way that will work with redo
		[[[self undoManager] prepareWithInvocationTarget:self] undoRedoSelectionRange:selectedRange];
		[self setSelectedRange:selectedRange];
		
		// recolor, coalesced
		[self setShouldRecolorAfterChanges:YES];
		[self recolorAfterChanges];
	}
	else
	{
		NSBeep();
	}
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
	SEL sel = [menuItem action];
	
	// Mimic the way copy: enables, for consistency; enabled only when the selection is not zero-length
	if (sel == @selector(copyAsParagraph:))
		return ([self selectedRange].length > 0);
	
	return [super validateMenuItem:menuItem];
}

- (void)selectErrorRange
{
	// If there is error-tracking information set, and the error is not attributed to a runtime script
	// such as a lambda or a callback, then we can highlight the error range
	if (!gEidosExecutingRuntimeScript && (gEidosCharacterStartOfErrorUTF16 >= 0) && (gEidosCharacterEndOfErrorUTF16 >= gEidosCharacterStartOfErrorUTF16))
	{
		NSRange charRange = NSMakeRange(gEidosCharacterStartOfErrorUTF16, gEidosCharacterEndOfErrorUTF16 - gEidosCharacterStartOfErrorUTF16 + 1);
		
		[self setSelectedRange:charRange];
		[self scrollRangeToVisible:charRange];
		
		// Set the selection color to red for maximal visibility; this gets set back in setSelectedRanges:affinity:stillSelecting:
		[self setSelectedTextAttributes:@{NSBackgroundColorAttributeName:[NSColor redColor], NSForegroundColorAttributeName:[NSColor whiteColor]}];
	}
	
	// In any case, since we are the ultimate consumer of the error information, we should clear out
	// the error state to avoid misattribution of future errors
	gEidosCharacterStartOfError = -1;
	gEidosCharacterEndOfError = -1;
	gEidosCharacterStartOfErrorUTF16 = -1;
	gEidosCharacterEndOfErrorUTF16 = -1;
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

- (void)setSelectedRanges:(NSArray *)ranges affinity:(NSSelectionAffinity)affinity stillSelecting:(BOOL)stillSelectingFlag
{
	// The text selection color may have been changed by -selectErrorRange; here we just make sure it is set back again
	[self setSelectedTextAttributes:@{NSBackgroundColorAttributeName:[NSColor selectedTextBackgroundColor]}];
	
	[super setSelectedRanges:ranges affinity:affinity stillSelecting:stillSelectingFlag];
}

- (void)mouseDown:(NSEvent *)theEvent
{
	NSUInteger modifiers = [theEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask;		// BCH 4/7/2016: NSEventModifierFlags not defined in 10.9
	
	// If the control key is down, the click will be interpreted by super as a context-menu click even with option down, so we can let that through.
	// Otherwise, option-clicks produce discontiguous selections that we want to prevent since we are not prepared to deal with them.  Instead, we
	// use option-clicks to indicate that the word clicked on should be looked up in EidosHelpController.
	if ((modifiers & NSAlternateKeyMask) && !(modifiers & NSControlKeyMask))
	{
		NSPoint windowPoint = [theEvent locationInWindow];
		NSRect windowRect = NSMakeRect(windowPoint.x, windowPoint.y, 0, 0);
		NSRect screenRect = [[self window] convertRectToScreen:windowRect];	// why is convertBaseToScreen: deprecated??
		NSPoint screenPoint = screenRect.origin;
		NSUInteger charIndex = [self characterIndexForPoint:screenPoint];	// takes a point in screen coordinates, for some weird reason
		
		if (charIndex < [[self textStorage] length])
		{
			NSRange wordRange = [self selectionRangeForProposedRange:NSMakeRange(charIndex, 1) granularity:NSSelectByWord];
			NSString *word = [[self string] substringWithRange:wordRange];
			NSString *trimmedWord = [word stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
			
			// A few substitutions to improve the search
			if ([trimmedWord isEqualToString:@":"])			trimmedWord = @"operator :";
			else if ([trimmedWord isEqualToString:@"("])	trimmedWord = @"operator ()";
			else if ([trimmedWord isEqualToString:@")"])	trimmedWord = @"operator ()";
			else if ([trimmedWord isEqualToString:@","])	trimmedWord = @"calls: operator ()";
			else if ([trimmedWord isEqualToString:@"["])	trimmedWord = @"operator []";
			else if ([trimmedWord isEqualToString:@"]"])	trimmedWord = @"operator []";
			else if ([trimmedWord isEqualToString:@"{"])	trimmedWord = @"compound statements";
			else if ([trimmedWord isEqualToString:@"}"])	trimmedWord = @"compound statements";
			else if ([trimmedWord isEqualToString:@"."])	trimmedWord = @"operator .";
			else if ([trimmedWord isEqualToString:@"="])	trimmedWord = @"operator =";
			else if ([trimmedWord isEqualToString:@"+"])	trimmedWord = @"Arithmetic operators";
			else if ([trimmedWord isEqualToString:@"-"])	trimmedWord = @"Arithmetic operators";
			else if ([trimmedWord isEqualToString:@"*"])	trimmedWord = @"Arithmetic operators";
			else if ([trimmedWord isEqualToString:@"/"])	trimmedWord = @"Arithmetic operators";
			else if ([trimmedWord isEqualToString:@"%"])	trimmedWord = @"Arithmetic operators";
			else if ([trimmedWord isEqualToString:@"^"])	trimmedWord = @"Arithmetic operators";
			else if ([trimmedWord isEqualToString:@"|"])	trimmedWord = @"Logical operators";
			else if ([trimmedWord isEqualToString:@"&"])	trimmedWord = @"Logical operators";
			else if ([trimmedWord isEqualToString:@"!"])	trimmedWord = @"Logical operators";
			else if ([trimmedWord isEqualToString:@"=="])	trimmedWord = @"Comparative operators";
			else if ([trimmedWord isEqualToString:@"!="])	trimmedWord = @"Comparative operators";
			else if ([trimmedWord isEqualToString:@"<="])	trimmedWord = @"Comparative operators";
			else if ([trimmedWord isEqualToString:@">="])	trimmedWord = @"Comparative operators";
			else if ([trimmedWord isEqualToString:@"<"])	trimmedWord = @"Comparative operators";
			else if ([trimmedWord isEqualToString:@">"])	trimmedWord = @"Comparative operators";
			else if ([trimmedWord isEqualToString:@"'"])	trimmedWord = @"type string";
			else if ([trimmedWord isEqualToString:@"\""])	trimmedWord = @"type string";
			else if ([trimmedWord isEqualToString:@";"])	trimmedWord = @"null statements";
			else if ([trimmedWord isEqualToString:@"//"])	trimmedWord = @"comments";
			else if ([trimmedWord isEqualToString:@"if"])	trimmedWord = @"if and if–else statements";
			else if ([trimmedWord isEqualToString:@"else"])	trimmedWord = @"if and if–else statements";
			else if ([trimmedWord isEqualToString:@"for"])	trimmedWord = @"for statements";
			else if ([trimmedWord isEqualToString:@"in"])	trimmedWord = @"for statements";
			else if ([trimmedWord isEqualToString:@"function"])	trimmedWord = @"user-defined functions";
			else
			{
				// Give the delegate a chance to supply additional help-text substitutions
				id delegate = [self delegate];
				
				if ([delegate respondsToSelector:@selector(eidosTextView:helpTextForClickedText:)])
				{
					NSString *delegateResult = [delegate eidosTextView:self helpTextForClickedText:trimmedWord];
					
					// The delegate may return nil or @"" to indicate no substitution desired
					if ([delegateResult length])
						trimmedWord = delegateResult;
				}
			}
			
			// And then look up the hit using EidosHelpController
			if ([trimmedWord length] != 0)
			{
				[self setSelectedRange:wordRange];
				
				EidosHelpController *helpController = [EidosHelpController sharedController];
				
				[helpController enterSearchForString:trimmedWord titlesOnly:YES];
				[[helpController window] makeKeyAndOrderFront:self];
			}
		}
		
		// We need this to keep the help controller window in front after an option-click, otherwise AppKit forces us back on top again
		[NSApp preventWindowOrdering];
		return;
	}
	
	// Start out willing to work with a double-click for purposes of delimiter-balancing;
	// see selectionRangeForProposedRange:proposedCharRange granularity: below
	inEligibleDoubleClick = YES;
	
	[super mouseDown:theEvent];
}

- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent
{
	NSUInteger modifiers = [theEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask;		// BCH 4/7/2016: NSEventModifierFlags not defined in 10.9
	
	// We need this to keep the help controller window in front after an option-click, otherwise AppKit forces us back on top again
	if ((modifiers & NSAlternateKeyMask) && !(modifiers & NSControlKeyMask))
		return YES;
	
	return NO;
}

- (BOOL)shouldDelayWindowOrderingForEvent:(NSEvent *)theEvent
{
	NSUInteger modifiers = [theEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask;		// BCH 4/7/2016: NSEventModifierFlags not defined in 10.9
	
	// We need this to keep the help controller window in front after an option-click, otherwise AppKit forces us back on top again
	if ((modifiers & NSAlternateKeyMask) && !(modifiers & NSControlKeyMask))
		return YES;
	
	return NO;
}

- (NSRange)selectionRangeForProposedRange:(NSRange)proposedCharRange granularity:(NSSelectionGranularity)granularity
{
	if ((granularity == NSSelectByWord) && inEligibleDoubleClick)
	{
		// The proposed range has to be zero-length, otherwise this click sequence is ineligible
		if (proposedCharRange.length == 0)
		{
			NSEvent *event = [NSApp currentEvent];
			NSEventType eventType = [event type];
			NSTimeInterval eventTime = [event timestamp];
			
			if (eventType == NSLeftMouseDown)
			{
				// This is the mouseDown of the double-click; we do not want to modify the selection here, just log the time
				doubleDownTime = eventTime;
			}
			else if (eventType == NSLeftMouseUp)
			{
				// After the double-click interval since the second mouseDown, the mouseUp is no longer eligible
				if (eventTime - doubleDownTime <= [NSEvent doubleClickInterval])
				{
					// MAINTENANCE NOTE: This also exists, in slightly altered form, in insertText:replacementRange:, so please maintain the two in parallel!
					NSString *scriptString = [[self textStorage] string];
					int stringLength = (int)[scriptString length];
					int proposedCharacterIndex = (int)proposedCharRange.location;
					unichar uch = [scriptString characterAtIndex:proposedCharacterIndex];
					BOOL forward = false;
					unichar incrementChar = ' ';
					unichar decrementChar = ' ';
					EidosTokenType token1 = EidosTokenType::kTokenNone;
					EidosTokenType token2 = EidosTokenType::kTokenNone;
					
					// Check for any of the delimiter types we support, and set up to do a scan
					if (uch == '{')		{ forward = true; incrementChar = '{'; decrementChar = '}'; token1 = EidosTokenType::kTokenLBrace; token2 = EidosTokenType::kTokenRBrace; }
					if (uch == '}')		{ forward = false; incrementChar = '}'; decrementChar = '{'; token1 = EidosTokenType::kTokenLBrace; token2 = EidosTokenType::kTokenRBrace; }
					if (uch == '(')		{ forward = true; incrementChar = '('; decrementChar = ')'; token1 = EidosTokenType::kTokenLParen; token2 = EidosTokenType::kTokenRParen; }
					if (uch == ')')		{ forward = false; incrementChar = ')'; decrementChar = '('; token1 = EidosTokenType::kTokenLParen; token2 = EidosTokenType::kTokenRParen; }
					if (uch == '[')		{ forward = true; incrementChar = '['; decrementChar = ']'; token1 = EidosTokenType::kTokenLBracket; token2 = EidosTokenType::kTokenRBracket; }
					if (uch == ']')		{ forward = false; incrementChar = ']'; decrementChar = '['; token1 = EidosTokenType::kTokenLBracket; token2 = EidosTokenType::kTokenRBracket; }
					
					if (token1 != EidosTokenType::kTokenNone)
					{
						// We've got a double-click on a character that we want to balance-select.  This is complicated mostly
						// because of strings, which are a pain in the butt.  To simplify that issue, we tokenize and search
						// in the token stream parallel to searching in the text.
						std::string script_string([scriptString UTF8String]);
						EidosScript script(script_string);
						
						// Tokenize
						script.Tokenize(true, true);	// make bad tokens as needed, keep nonsignificant tokens
						
						// Find the token containing the double-click point
						const std::vector<EidosToken> &tokens = script.Tokens();
						int token_count = (int)tokens.size();
						const EidosToken *click_token = nullptr;
						int click_token_index;
						
						for (click_token_index = 0; click_token_index < token_count; ++click_token_index)
						{
							click_token = &tokens[click_token_index];
							
							if ((click_token->token_UTF16_start_ <= proposedCharacterIndex) && (click_token->token_UTF16_end_ >= proposedCharacterIndex))
								break;
						}
						
						if (click_token)
						{
							// OK, this token contains the character that was double-clicked.  We could have the actual token
							// (incrementToken), we could be in a string, or we could be in a comment.  We stay in the domain
							// that we start in; if in a string, for example, only delimiters within strings affect our scan.
							EidosTokenType click_token_type = click_token->token_type_;
							const EidosToken *scan_token = click_token;
							int scan_token_index = click_token_index;
							NSInteger scanPosition = proposedCharacterIndex;
							int balanceCount = 0;
							
							while (YES)
							{
								EidosTokenType scan_token_type = scan_token->token_type_;
								BOOL isCandidate;
								
								if (click_token_type == EidosTokenType::kTokenComment)
									isCandidate = (scan_token_type == EidosTokenType::kTokenComment);
								else if (click_token_type == EidosTokenType::kTokenCommentLong)
									isCandidate = (scan_token_type == EidosTokenType::kTokenCommentLong);
								else if (click_token_type == EidosTokenType::kTokenString)
									isCandidate = (scan_token_type == EidosTokenType::kTokenString);
								else
									isCandidate = ((scan_token_type == token1) || (scan_token_type == token2));
								
								if (isCandidate)
								{
									uch = [scriptString characterAtIndex:scanPosition];
									
									if (uch == incrementChar)
										balanceCount++;
									else if (uch == decrementChar)
										balanceCount--;
									
									// If balanceCount is now zero, all opens have been balanced by closes, and we have found our matching delimiter
									if (balanceCount == 0)
									{
										inEligibleDoubleClick = false;	// clear this to avoid potential confusion if we get called again later
										
										if (forward)
											return NSMakeRange(proposedCharacterIndex, scanPosition - proposedCharacterIndex + 1);
										else
											return NSMakeRange(scanPosition, proposedCharacterIndex - scanPosition + 1);
									}
								}
								
								// Advance, forward or backward, to the next character; if we reached the end without balancing, beep
								scanPosition += (forward ? 1 : -1);
								if ((scanPosition == stringLength) || (scanPosition == -1))
								{
									NSBeep();
									inEligibleDoubleClick = false;
									return NSMakeRange(proposedCharacterIndex, 1);
								}
								
								// Make sure we're in the right token
								while ((scan_token->token_UTF16_start_ > scanPosition) || (scan_token->token_UTF16_end_ < scanPosition))
								{
									scan_token_index += (forward ? 1 : -1);
									
									if ((scan_token_index < 0) || (scan_token_index == token_count))
									{
										NSLog(@"ran out of tokens!");
										NSBeep();
										inEligibleDoubleClick = false;
										return NSMakeRange(proposedCharacterIndex, 1);
									}
									
									scan_token = &tokens[scan_token_index];
								}
							}
						}
						else
						{
							inEligibleDoubleClick = false;
						}
					}
					else
					{
						inEligibleDoubleClick = false;
					}
				}
				else
				{
					inEligibleDoubleClick = false;
				}
			}
			else
			{
				inEligibleDoubleClick = false;
			}
		}
		else
		{
			inEligibleDoubleClick = false;
		}
	}
	
	return [super selectionRangeForProposedRange:proposedCharRange granularity:granularity];
}


//
//	Syntax coloring
//
#pragma mark -
#pragma mark Syntax coloring

- (void)syntaxColorForEidos
{
	id delegate = [self delegate];
	
	// Construct a Script object from the current script string
	NSString *scriptString = [self string];
	std::string script_string([scriptString UTF8String]);
	EidosScript script(script_string);
	
	// Tokenize
	script.Tokenize(true, true);	// make bad tokens as needed, keep nonsignificant tokens
	
	// Set up our shared colors
	static NSColor *numberLiteralColor = nil;
	static NSColor *stringLiteralColor = nil;
	static NSColor *commentColor = nil;
	static NSColor *identifierColor = nil;
	static NSColor *keywordColor = nil;
	static NSColor *contextKeywordColor = nil;
	
	if (!numberLiteralColor)
	{
		numberLiteralColor = [[NSColor colorWithCalibratedRed:28/255.0 green:0/255.0 blue:207/255.0 alpha:1.0] retain];
		stringLiteralColor = [[NSColor colorWithCalibratedRed:196/255.0 green:26/255.0 blue:22/255.0 alpha:1.0] retain];
		commentColor = [[NSColor colorWithCalibratedRed:0/255.0 green:116/255.0 blue:0/255.0 alpha:1.0] retain];
		identifierColor = [[NSColor colorWithCalibratedRed:63/255.0 green:110/255.0 blue:116/255.0 alpha:1.0] retain];
		keywordColor = [[NSColor colorWithCalibratedRed:170/255.0 green:13/255.0 blue:145/255.0 alpha:1.0] retain];
		contextKeywordColor = [[NSColor colorWithCalibratedRed:80/255.0 green:13/255.0 blue:145/255.0 alpha:1.0] retain];
	}
	
	// Syntax color!
	NSRange fullRange = NSMakeRange(0, [[self textStorage] length]);
	NSLayoutManager *lm = [self layoutManager];
	
	[lm ensureGlyphsForCharacterRange:fullRange];
	[lm removeTemporaryAttribute:NSForegroundColorAttributeName forCharacterRange:fullRange];
	
	for (const EidosToken &token : script.Tokens())
	{
		NSRange tokenRange = NSMakeRange(token.token_UTF16_start_, token.token_UTF16_end_ - token.token_UTF16_start_ + 1);
		
		if (token.token_type_ == EidosTokenType::kTokenNumber)
			[lm addTemporaryAttribute:NSForegroundColorAttributeName value:numberLiteralColor forCharacterRange:tokenRange];
		if (token.token_type_ == EidosTokenType::kTokenString)
			[lm addTemporaryAttribute:NSForegroundColorAttributeName value:stringLiteralColor forCharacterRange:tokenRange];
		if ((token.token_type_ == EidosTokenType::kTokenComment) || (token.token_type_ == EidosTokenType::kTokenCommentLong))
			[lm addTemporaryAttribute:NSForegroundColorAttributeName value:commentColor forCharacterRange:tokenRange];
		if (token.token_type_ > EidosTokenType::kFirstIdentifierLikeToken)
			[lm addTemporaryAttribute:NSForegroundColorAttributeName value:keywordColor forCharacterRange:tokenRange];
		if (token.token_type_ == EidosTokenType::kTokenIdentifier)
		{
			// most identifiers are left as black; only special ones get colored
			const std::string &token_string = token.token_string_;
			
			if ((token_string.compare("T") == 0) ||
				(token_string.compare("F") == 0) ||
				(token_string.compare("E") == 0) ||
				(token_string.compare("PI") == 0) ||
				(token_string.compare("INF") == 0) ||
				(token_string.compare("NAN") == 0) ||
				(token_string.compare("NULL") == 0))
				[lm addTemporaryAttribute:NSForegroundColorAttributeName value:identifierColor forCharacterRange:tokenRange];
			else
			{
				// we also let the Context specify special identifiers that we will syntax color
				if ([delegate respondsToSelector:@selector(eidosTextView:tokenStringIsSpecialIdentifier:)])
				{
					EidosSyntaxHighlightType highlightType = [delegate eidosTextView:self tokenStringIsSpecialIdentifier:token_string];
					
					if (highlightType == EidosSyntaxHighlightType::kHighlightAsIdentifier)
						[lm addTemporaryAttribute:NSForegroundColorAttributeName value:identifierColor forCharacterRange:tokenRange];
					else if (highlightType == EidosSyntaxHighlightType::kHighlightAsKeyword)
						[lm addTemporaryAttribute:NSForegroundColorAttributeName value:keywordColor forCharacterRange:tokenRange];
					else if (highlightType == EidosSyntaxHighlightType::kHighlightAsContextKeyword)
						[lm addTemporaryAttribute:NSForegroundColorAttributeName value:contextKeywordColor forCharacterRange:tokenRange];
				}
			}
		}
	}
}

- (void)syntaxColorForOutput
{
	NSRange fullRange = NSMakeRange(0, [[self textStorage] length]);
	NSLayoutManager *lm = [self layoutManager];
	NSString *string = [self string];
	NSArray *lines = [string componentsSeparatedByString:@"\n"];
	int lineCount = (int)[lines count];
	int stringPosition = 0;
	
	// Set up our shared attributes
	static NSDictionary *poundDirectiveAttrs = nil;
	static NSDictionary *commentAttrs = nil;
	static NSDictionary *subpopAttrs = nil;
	static NSDictionary *genomicElementAttrs = nil;
	static NSDictionary *mutationTypeAttrs = nil;
	
	if (!poundDirectiveAttrs)
	{
		poundDirectiveAttrs = [@{NSForegroundColorAttributeName : [NSColor colorWithCalibratedRed:196/255.0 green:26/255.0 blue:22/255.0 alpha:1.0]} retain];
		commentAttrs = [@{NSForegroundColorAttributeName : [NSColor colorWithCalibratedRed:0/255.0 green:116/255.0 blue:0/255.0 alpha:1.0]} retain];
		subpopAttrs = [@{NSForegroundColorAttributeName : [NSColor colorWithCalibratedRed:28/255.0 green:0/255.0 blue:207/255.0 alpha:1.0]} retain];
		genomicElementAttrs = [@{NSForegroundColorAttributeName : [NSColor colorWithCalibratedRed:63/255.0 green:110/255.0 blue:116/255.0 alpha:1.0]} retain];
		mutationTypeAttrs = [@{NSForegroundColorAttributeName : [NSColor colorWithCalibratedRed:170/255.0 green:13/255.0 blue:145/255.0 alpha:1.0]} retain];
	}
	
	// And then tokenize and color
	[lm removeTemporaryAttribute:NSForegroundColorAttributeName forCharacterRange:fullRange];
	
	for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex)
	{
		NSString *line = [lines objectAtIndex:lineIndex];
		NSRange lineRange = NSMakeRange(stringPosition, (int)[line length]);
		int nextStringPosition = (int)(stringPosition + lineRange.length + 1);			// +1 for the newline
		
		if (lineRange.length)
		{
			//NSLog(@"lineIndex %d, lineRange == %@", lineIndex, NSStringFromRange(lineRange));
			
			// find comments and color and remove them
			NSRange commentRange = [line rangeOfString:@"//"];
			
			if ((commentRange.location != NSNotFound) && (commentRange.length == 2))
			{
				int commentLength = (int)(lineRange.length - commentRange.location);
				
				[lm addTemporaryAttributes:commentAttrs forCharacterRange:NSMakeRange(lineRange.location + commentRange.location, commentLength)];
				
				lineRange.length -= commentLength;
				line = [line substringToIndex:commentRange.location];
			}
			
			// if anything is left...
			if (lineRange.length)
			{
				// remove leading whitespace
				do {
					NSRange leadingWhitespaceRange = [line rangeOfCharacterFromSet:[NSCharacterSet whitespaceCharacterSet] options:NSAnchoredSearch];
					
					if (leadingWhitespaceRange.location == NSNotFound || leadingWhitespaceRange.length == 0)
						break;
					
					lineRange.location += leadingWhitespaceRange.length;
					lineRange.length -= leadingWhitespaceRange.length;
					line = [line substringFromIndex:leadingWhitespaceRange.length];
				} while (YES);
				
				// remove trailing whitespace
				do {
					NSRange trailingWhitespaceRange = [line rangeOfCharacterFromSet:[NSCharacterSet whitespaceCharacterSet] options:NSAnchoredSearch | NSBackwardsSearch];
					
					if (trailingWhitespaceRange.location == NSNotFound || trailingWhitespaceRange.length == 0)
						break;
					
					lineRange.length -= trailingWhitespaceRange.length;
					line = [line substringToIndex:trailingWhitespaceRange.location];
				} while (YES);
				
				// if anything is left...
				if (lineRange.length)
				{
					// find pound directives and color them
					if ([line characterAtIndex:0] == '#')
						[lm addTemporaryAttributes:poundDirectiveAttrs forCharacterRange:lineRange];
					else
					{
						NSRange scanRange = NSMakeRange(0, lineRange.length);
						
						do {
							NSRange tokenRange = [line rangeOfString:@"\\b[pgm][0-9]+\\b" options:NSRegularExpressionSearch range:scanRange];
							
							if (tokenRange.location == NSNotFound || tokenRange.length == 0)
								break;
							
							NSString *substring = [line substringWithRange:tokenRange];
							NSDictionary *syntaxAttrs = nil;
							unichar firstChar = [substring characterAtIndex:0];
							
							if (firstChar == 'p')
								syntaxAttrs = subpopAttrs;
							else if (firstChar == 'g')
								syntaxAttrs = genomicElementAttrs;
							else if (firstChar == 'm')
								syntaxAttrs = mutationTypeAttrs;
							// we don't presently color sX or iX in the output
							
							if (syntaxAttrs)
								[lm addTemporaryAttributes:syntaxAttrs forCharacterRange:NSMakeRange(tokenRange.location + lineRange.location, tokenRange.length)];
							
							scanRange.length = (scanRange.location + scanRange.length) - (tokenRange.location + tokenRange.length);
							scanRange.location = (tokenRange.location + tokenRange.length);
							
							if (scanRange.length < 2)
								break;
						} while (YES);
					}
				}
			}
		}
		
		stringPosition = nextStringPosition;
	}
}

- (void)clearSyntaxColoring
{
	NSRange fullRange = NSMakeRange(0, [[self textStorage] length]);
	NSLayoutManager *lm = [self layoutManager];
	
	[lm ensureGlyphsForCharacterRange:fullRange];
	[lm removeTemporaryAttribute:NSForegroundColorAttributeName forCharacterRange:fullRange];
}

- (void)updateSyntaxColoring
{
	// This method will clear all syntax coloring if EidosSyntaxColoringOption::NoSyntaxColoring is set, so
	// it should not be called on every text change, to allow the console textview to maintain its own coloring.
	// This does not trigger textDidChange (that is done manually), so there is no need for re-entrancy protection.
	switch (_syntaxColoring)
	{
		case kEidosSyntaxColoringNone:
			[self clearSyntaxColoring];
			break;
		case kEidosSyntaxColoringEidos:
			[self syntaxColorForEidos];
			break;
		case kEidosSyntaxColoringOutput:
			[self syntaxColorForOutput];
			break;
	}
}

- (void)setSyntaxColoring:(EidosSyntaxColoringOption)syntaxColoring
{
	if (_syntaxColoring != syntaxColoring)
	{
		_syntaxColoring = syntaxColoring;
		
		[self updateSyntaxColoring];
	}
}

- (void)setDisplayFontSize:(int)fontSize
{
	// This is used by SLiMgui; there is no UI exposing it in EidosScribe
	if (_displayFontSize != fontSize)
	{
		_displayFontSize = fontSize;
		
		NSFont *newFont = [NSFont fontWithName:@"Menlo" size:fontSize];
		NSTextStorage *ts = [self textStorage];
		
		// go through all attribute runs for NSFontAttribute and change them
		[ts beginEditing];
		
		[ts enumerateAttribute:NSFontAttributeName inRange:NSMakeRange(0, ts.length) options:0 usingBlock:^(id value, NSRange range, BOOL *stop) {
			if (value) {
				NSFont *oldFont = (NSFont *)value;
				CGFloat pointSize = [oldFont pointSize];
				
				if ((pointSize >= 6.0) && (pointSize <= 100.0))		// avoid resizing the padding lines in the console...
				{
					[ts removeAttribute:NSFontAttributeName range:range];
					[ts addAttribute:NSFontAttributeName value:newFont range:range];
				}
			}
		}];
		
		[ts endEditing];
		
		// set the typing attributes; this code just makes an assumption about the correct typing attributes
		// based on the class, probably I ought to add a new method on EidosTextView to get a typing attr dict...
		if ([self isKindOfClass:[EidosConsoleTextView class]])
			[self setTypingAttributes:[NSDictionary eidosInputAttrsWithSize:[self displayFontSize]]];
		else
			[self setTypingAttributes:[NSDictionary eidosTextAttributesWithColor:nil size:_displayFontSize]];
	}
}

- (void)recolorAfterChanges
{
	// We fold in syntax coloring as part of every change set.  If _syntaxColoring==NoSyntaxColoring, we don't do
	// anything on text changes; we only clear attributes when NoSyntaxColoring is initially set on the textview.
	if (_syntaxColoring != kEidosSyntaxColoringNone)
		[self updateSyntaxColoring];
}

- (void)didChangeText
{
	// When we used regular attributes on the text storage for syntax coloring, this was done in
	// textStorageDidProcessEditing: since that is the right time to change attributes in response
	// to other changes.  Now that we use temporary attributes, though, we need to do it in this
	// method, so that the layout manager is completely synchronized with the new, changed text.
	[super didChangeText];
	
	if (_shouldRecolorAfterChanges)
		[self recolorAfterChanges];
}

- (void)clearHighlightMatches
{
	NSRange fullRange = NSMakeRange(0, [[self textStorage] length]);
	NSLayoutManager *lm = [self layoutManager];
	
	[lm ensureGlyphsForCharacterRange:fullRange];
	[lm removeTemporaryAttribute:NSBackgroundColorAttributeName forCharacterRange:fullRange];
}

- (void)highlightMatchesForString:(NSString *)matchString
{
	NSColor *highlightColor;
	
	// Get the find highlight color if available, otherwise the standard selected-text background color
	if (@available(macOS 10.13, *)) {
		highlightColor = [NSColor findHighlightColor];
	} else {
		highlightColor = [NSColor selectedControlColor];
	}
	
	// Highlight using the background color on all matches
	NSRange fullRange = NSMakeRange(0, [[self textStorage] length]);
	NSString *string = [[self textStorage] string];
	NSLayoutManager *lm = [self layoutManager];
	
	[lm ensureGlyphsForCharacterRange:fullRange];
	
	// thanks to https://stackoverflow.com/a/7033787/2752221
	NSRange searchRange = fullRange, foundRange;
	
	while (searchRange.location < string.length)
	{
		searchRange.length = string.length - searchRange.location;
		foundRange = [string rangeOfString:matchString options:0 range:searchRange];
		
		if (foundRange.location != NSNotFound)
		{
			[lm addTemporaryAttribute:NSBackgroundColorAttributeName value:highlightColor forCharacterRange:foundRange];
			
			searchRange.location = foundRange.location + 1;
		} else {
			break;
		}
	}
}


//
//	Signature display
//
#pragma mark -
#pragma mark Signature display

- (EidosFunctionMap *)functionMapForScriptString:(NSString *)scriptString includingOptionalFunctions:(BOOL)includingOptionalFunctions
{
	// This returns a function map (owned by the caller) that reflects the best guess we can make, incorporating
	// any functions known to our delegate, as well as all functions we can scrape from the script string.
	std::string script_string([scriptString UTF8String]);
	EidosScript script(script_string);
	
	// Tokenize
	script.Tokenize(true, false);	// make bad tokens as needed, don't keep nonsignificant tokens
	
	return [self functionMapForTokenizedScript:script includingOptionalFunctions:includingOptionalFunctions];
}

- (EidosFunctionMap *)functionMapForTokenizedScript:(EidosScript &)script includingOptionalFunctions:(BOOL)includingOptionalFunctions
{
	// This lower-level function takes a tokenized script object and works from there, allowing reuse of work
	// in the case of attributedSignatureForScriptString:...
	id delegate = [self delegate];
	EidosFunctionMap *functionMapPtr = nullptr;
	
	if ([delegate respondsToSelector:@selector(functionMapForEidosTextView:)])
		functionMapPtr = [delegate functionMapForEidosTextView:self];
	
	if (functionMapPtr)
		functionMapPtr = new EidosFunctionMap(*functionMapPtr);
	else
		functionMapPtr = new EidosFunctionMap(*EidosInterpreter::BuiltInFunctionMap());
	
	// functionMapForEidosTextView: returns the function map for the current interpreter state, and the type-interpreter
	// stuff we do below gives the delegate no chance to intervene (note that SLiMTypeInterpreter does not get in here,
	// unlike in the code completion machinery!).  But sometimes we want SLiM's zero-gen functions to be added to the map
	// in all cases; it would be even better to be smart the way code completion is, but that's more work than it's worth.
	if (includingOptionalFunctions)
		if ([delegate respondsToSelector:@selector(eidosTextView:addOptionalFunctionsToMap:)])
			[delegate eidosTextView:self addOptionalFunctionsToMap:functionMapPtr];
	
	// OK, now we have a starting point.  We now want to use the type-interpreter to add any functions that are declared
	// in the full script, so that such declarations are known to us even before they have actually been executed.
	EidosTypeTable typeTable;
	EidosCallTypeTable callTypeTable;
	EidosSymbolTable *symbols = gEidosConstantsSymbolTable;
	
	if ([delegate respondsToSelector:@selector(eidosTextView:symbolsFromBaseSymbols:)])
		symbols = [delegate eidosTextView:self symbolsFromBaseSymbols:symbols];
	
	if (symbols)
		symbols->AddSymbolsToTypeTable(&typeTable);
	
	script.ParseInterpreterBlockToAST(true, true);	// make bad nodes as needed (i.e. never raise, and produce a correct tree)
	
	EidosTypeInterpreter typeInterpreter(script, typeTable, *functionMapPtr, callTypeTable);
	
	typeInterpreter.TypeEvaluateInterpreterBlock();	// result not used
	
	return functionMapPtr;
}

- (NSAttributedString *)attributedSignatureForScriptString:(NSString *)scriptString selection:(NSRange)selection
{
	if ([scriptString length])
	{
		std::string script_string([scriptString UTF8String]);
		EidosScript script(script_string);
		
		// Tokenize
		script.Tokenize(true, false);	// make bad tokens as needed, don't keep nonsignificant tokens
		
		const std::vector<EidosToken> &tokens = script.Tokens();
		int tokenCount = (int)tokens.size();
		
		//NSLog(@"script string \"%@\" contains %d tokens", scriptString, tokenCount);
		
		// Search forward to find the token position of the start of the selection
		int selectionStart = (int)selection.location;
		int tokenIndex;
		
		for (tokenIndex = 0; tokenIndex < tokenCount; ++tokenIndex)
			if (tokens[tokenIndex].token_UTF16_start_ >= selectionStart)
				break;
		
		//NSLog(@"token %d follows the selection (selectionStart == %d)", tokenIndex, selectionStart);
		//if (tokenIndex == tokenCount)
		//	NSLog(@"   (end of script)");
		//else
		//	NSLog(@"   token string: %s", tokens[tokenIndex]->token_string_.c_str());
		
		// tokenIndex now has the index of the first token *after* the selection start; it can be equal to tokenCount
		// Now we want to scan backward from there, balancing parentheses and looking for the pattern "identifier("
		int backscanIndex = tokenIndex - 1;
		int parenCount = 0, lowestParenCountSeen = 0;
		
		while (backscanIndex > 0)	// last examined position is 1, since we can't look for an identifier at 0 - 1 == -1
		{
			const EidosToken &token = tokens[backscanIndex];
			EidosTokenType tokenType = token.token_type_;
			
			if (tokenType == EidosTokenType::kTokenLParen)
			{
				--parenCount;
				
				if (parenCount < lowestParenCountSeen)
				{
					const EidosToken &previousToken = tokens[backscanIndex - 1];
					EidosTokenType previousTokenType = previousToken.token_type_;
					
					if (previousTokenType == EidosTokenType::kTokenIdentifier)
					{
						// OK, we found the pattern "identifier("; extract the name of the function/method
						// We also figure out here whether it is a method call (tokens like ".identifier(") or not
						NSString *callName = [NSString stringWithUTF8String:previousToken.token_string_.c_str()];
						NSAttributedString *callAttrString = nil;
						
						if ((backscanIndex > 1) && (tokens[backscanIndex - 2].token_type_ == EidosTokenType::kTokenDot))
						{
							// This is a method call, so look up its signature that way
							callAttrString = [self attributedSignatureForMethodName:callName];
						}
						else
						{
							// If this is a function declaration like "function(...)identifier(" then show no signature; it's not a function call
							// Determining this requires a fairly complex backscan, because we also have things like "if (...) identifier(" which
							// are function calls.  This is the price we pay for working at the token level rather than the AST level for this;
							// so it goes.  Note that this backscan is separate from the one done outside this block.  BCH 1 March 2018.
							if ((backscanIndex > 1) && (tokens[backscanIndex - 2].token_type_ == EidosTokenType::kTokenRParen))
							{
								// Start a new backscan starting at the right paren preceding the identifier; we need to scan back to the balancing
								// left paren, and then see if the next thing before that is "function" or not.
								int funcCheckIndex = backscanIndex - 2;
								int funcCheckParens = 0;
								
								while (funcCheckIndex >= 0)
								{
									const EidosToken &backscanToken = tokens[funcCheckIndex];
									EidosTokenType backscanTokenType = backscanToken.token_type_;
									
									if (backscanTokenType == EidosTokenType::kTokenRParen)
										funcCheckParens++;
									else if (backscanTokenType == EidosTokenType::kTokenLParen)
										funcCheckParens--;
									
									--funcCheckIndex;
									
									if (funcCheckParens == 0)
										break;
								}
								
								if ((funcCheckParens == 0) && (funcCheckIndex >= 0) && (tokens[funcCheckIndex].token_type_ == EidosTokenType::kTokenFunction))
									break;
							}
							
							// This is a function call, so look up its signature that way, using our best-guess function map
							EidosFunctionMap *functionMapPtr = [self functionMapForTokenizedScript:script includingOptionalFunctions:YES];
							
							callAttrString = [self attributedSignatureForFunctionName:callName functionMap:functionMapPtr];
							
							delete functionMapPtr;
						}
						
						if (!callAttrString)
						{
							// Assemble an attributed string for our failed lookup message
							NSMutableAttributedString *attrStr = [[[NSMutableAttributedString alloc] init] autorelease];
							NSDictionary *plainAttrs = [NSDictionary eidosOutputAttrsWithSize:_displayFontSize];
							NSDictionary *functionAttrs = [NSDictionary eidosParseAttrsWithSize:_displayFontSize];
							
							[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:callName attributes:functionAttrs] autorelease]];
							[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"() – unrecognized call" attributes:plainAttrs] autorelease]];
							[attrStr addAttribute:NSBaselineOffsetAttributeName value:[NSNumber numberWithFloat:2.0] range:NSMakeRange(0, [attrStr length])];
							
							callAttrString = attrStr;
						}
						
						return callAttrString;
					}
					
					lowestParenCountSeen = parenCount;
				}
			}
			else if (tokenType == EidosTokenType::kTokenRParen)
			{
				++parenCount;
			}
			
			--backscanIndex;
		}
	}
	
	return [[[NSAttributedString alloc] init] autorelease];
}

- (NSAttributedString *)attributedSignatureForFunctionName:(NSString *)callName functionMap:(EidosFunctionMap *)functionMapPtr
{
	std::string call_name([callName UTF8String]);
	
	// Look for a matching function signature for the call name.
	for (const auto& function_iter : *functionMapPtr)
	{
		const EidosFunctionSignature *sig = function_iter.second.get();
		const std::string &sig_call_name = sig->call_name_;
		
		if (sig_call_name.compare(call_name) == 0)
			return [NSAttributedString eidosAttributedStringForCallSignature:sig size:[self displayFontSize]];
	}
	
	return nil;
}

- (NSAttributedString *)attributedSignatureForMethodName:(NSString *)callName
{
	std::string call_name([callName UTF8String]);
	id delegate = [self delegate];
	
	// Look for a method in the global method registry last; for this to work, the Context must register all methods with Eidos.
	// This case is much simpler than the function case, because the user can't declare their own methods.
	const std::vector<EidosMethodSignature_CSP> *methodSignatures = nullptr;
	
	if ([delegate respondsToSelector:@selector(eidosTextViewAllMethodSignatures:)])
		methodSignatures = [delegate eidosTextViewAllMethodSignatures:self];
	
	if (!methodSignatures)
		methodSignatures = gEidos_UndefinedClassObject->Methods();
	
	for (const EidosMethodSignature_CSP &sig : *methodSignatures)
	{
		const std::string &sig_call_name = sig->call_name_;
		
		if (sig_call_name.compare(call_name) == 0)
			return [NSAttributedString eidosAttributedStringForCallSignature:sig.get() size:[self displayFontSize]];
	}
	
	return nil;
}


//
//	Auto-completion
//
#pragma mark -
#pragma mark Auto-completion

- (void)keyDown:(NSEvent *)event
{
	// Sometimes esc and command-. do not seem to be bound to complete:.  I'm not sure if that is a change on 10.12,
	// or a matter of individual key bindings.  In any case, we make sure, in this override, that they always work.
	NSString *chars = [event charactersIgnoringModifiers];
	NSUInteger flags = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;		// BCH 4/7/2016: NSEventModifierFlags not defined in 10.9
	
	if ([chars length] == 1)
	{
		unichar keyChar = [chars characterAtIndex:0];
		
		if ((keyChar == 0x1B) && (flags == 0))
		{
			// escape key pressed
			[self doCommandBySelector:@selector(complete:)];
			return;
		}
		if ((keyChar == '.') && (flags == NSEventModifierFlagCommand))
		{
			// command-. pressed
			[self doCommandBySelector:@selector(complete:)];
			return;
		}
	}
	
	[super keyDown:event];
}

- (void)cancel:(id)sender
{
	// Apple is now sending this for esc and/or command-. in some OS X versions; we want it to continue to trigger complete:
	[self complete:sender];
}

- (void)cancelOperation:(id)sender
{
	// Apple is now sending this for esc and/or command-. in some OS X versions; we want it to continue to trigger complete:
	[self complete:sender];
}

- (NSArray *)completionsForPartialWordRange:(NSRange)charRange indexOfSelectedItem:(NSInteger *)index
{
	NSArray *completions = nil;
	
#if EIDOS_DEBUG_COMPLETION
	std::cout << "<<<<<<<<<<<<<< completionsForPartialWordRange: START" << std::endl;
#endif
	
	[self _completionHandlerWithRangeForCompletion:NULL completions:&completions];
	
#if EIDOS_DEBUG_COMPLETION
	std::cout << "<<<<<<<<<<<<<< completionsForPartialWordRange: END" << std::endl;
#endif
	
	return completions;
}

- (NSRange)rangeForUserCompletion
{
	NSRange baseRange = NSMakeRange(NSNotFound, 0);
	
#if EIDOS_DEBUG_COMPLETION
	std::cout << "<<<<<<<<<<<<<< rangeForUserCompletion: START" << std::endl;
#endif
	
	[self _completionHandlerWithRangeForCompletion:&baseRange completions:NULL];
	
#if EIDOS_DEBUG_COMPLETION
	std::cout << "<<<<<<<<<<<<<< rangeForUserCompletion: END" << std::endl;
#endif
	
	return baseRange;
}

- (NSMutableArray *)globalCompletionsWithTypes:(EidosTypeTable *)typeTable functions:(EidosFunctionMap *)functionMap keywords:(NSArray *)keywords argumentNames:(NSArray *)argumentNames
{
	NSMutableArray *globals = [NSMutableArray array];
	
	// First add entries for symbols in our type table (from Eidos constants, defined symbols, or our delegate)
	if (typeTable)
	{
		std::vector<std::string> typedSymbols = typeTable->AllSymbols();
		
		for (std::string &symbol_name : typedSymbols)
			[globals addObject:[NSString stringWithUTF8String:symbol_name.c_str()]];
	}
	
	// Sort the symbols, who knows what order they come from EidosTypeTable in...
	[globals sortUsingSelector:@selector(compare:)];
	
	// Next, if we have argument names that are completion matches, we want them at the top
	if (argumentNames && [argumentNames count])
	{
		NSArray *oldGlobals = globals;
		
		globals = [[argumentNames mutableCopy] autorelease];
		[globals addObjectsFromArray:oldGlobals];
	}
	
	// Next, a sorted list of functions, with () appended
	if (functionMap)
	{
		for (const auto& function_iter : *functionMap)
		{
			const EidosFunctionSignature *sig = function_iter.second.get();
			NSString *functionName = [NSString stringWithUTF8String:sig->call_name_.c_str()];
			
			// Exclude internal functions such as _Test()
			if (![functionName hasPrefix:@"_"])
				[globals addObject:[functionName stringByAppendingString:@"()"]];
		}
	}
	
	// Finally, provide language keywords as an option if requested
	if (keywords)
		[globals addObjectsFromArray:keywords];
	
	return globals;
}

- (NSMutableArray *)completionsForKeyPathEndingInTokenIndex:(int)lastDotTokenIndex ofTokenStream:(const std::vector<EidosToken> &)tokens withTypes:(EidosTypeTable *)typeTable functions:(EidosFunctionMap *)functionMap callTypes:(EidosCallTypeTable *)callTypeTable keywords:(NSArray *)keywords
{
	const EidosToken *token = &tokens[lastDotTokenIndex];
	EidosTokenType token_type = token->token_type_;
	
	if (token_type != EidosTokenType::kTokenDot)
	{
		NSLog(@"***** completionsForKeyPathEndingInTokenIndex... called for non-kTokenDot token!");
		return nil;
	}
	
	// OK, we've got a key path ending in a dot, and we want to return a list of completions that would work for that key path.
	// We'll trace backward, adding identifiers to a vector to build up the chain of references.  If we hit a bracket, we'll
	// skip back over everything inside it, since subsetting does not change the type; we just need to balance brackets.  If we
	// hit a parenthesis, we do similarly.  If we hit other things – a semicolon, a comma, a brace – that terminates the key path chain.
	std::vector<std::string> identifiers;
	std::vector<bool> identifiers_are_calls;
	std::vector<int32_t> identifier_positions;
	int bracketCount = 0, parenCount = 0;
	BOOL lastTokenWasDot = YES, justFinishedParenBlock = NO;
	
	for (int tokenIndex = lastDotTokenIndex - 1; tokenIndex >= 0; --tokenIndex)
	{
		token = &tokens[tokenIndex];
		token_type = token->token_type_;
		
		// skip backward over whitespace and comments; they make no difference to us
		if ((token_type == EidosTokenType::kTokenWhitespace) || (token_type == EidosTokenType::kTokenComment) || (token_type == EidosTokenType::kTokenCommentLong))
			continue;
		
		if (bracketCount)
		{
			// If we're inside a bracketed stretch, all we do is balance brackets and run backward.  We don't even clear lastTokenWasDot,
			// because a []. sequence puts us in the same situation as having just seen a dot – we're still waiting for an identifier.
			if (token_type == EidosTokenType::kTokenRBracket)
			{
				bracketCount++;
				continue;
			}
			if (token_type == EidosTokenType::kTokenLBracket)
			{
				bracketCount--;
				continue;
			}
			
			// Check for tokens that simply make no sense, and bail
			if ((token_type == EidosTokenType::kTokenLBrace) || (token_type == EidosTokenType::kTokenRBrace) || (token_type == EidosTokenType::kTokenSemicolon) || (token_type >= EidosTokenType::kFirstIdentifierLikeToken))
				return nil;
			
			continue;
		}
		else if (parenCount)
		{
			// If we're inside a paren stretch – which could be a parenthesized expression or a function call – we do similarly
			// to the brackets case, just balancing parens and running backward.  We don't clear lastTokenWasDot, because a
			// (). sequence puts us in the same situation (almost) as having just seen a dot – waiting for an identifier.
			if (token_type == EidosTokenType::kTokenRParen)
			{
				parenCount++;
				continue;
			}
			if (token_type == EidosTokenType::kTokenLParen)
			{
				parenCount--;
				
				if (parenCount == 0)
					justFinishedParenBlock = YES;
				continue;
			}
			
			// Check for tokens that simply make no sense, and bail
			if ((token_type == EidosTokenType::kTokenLBrace) || (token_type == EidosTokenType::kTokenRBrace) || (token_type == EidosTokenType::kTokenSemicolon) || (token_type >= EidosTokenType::kFirstIdentifierLikeToken))
				return nil;
			
			continue;
		}
		
		if (!lastTokenWasDot)
		{
			// We just saw an identifier, so the only thing that can continue the key path is a dot
			if (token_type == EidosTokenType::kTokenDot)
			{
				lastTokenWasDot = YES;
				justFinishedParenBlock = NO;
				continue;
			}
			
			// the key path has terminated at some non-key-path token, so we're done tracing it
			break;
		}
		
		// OK, the last token was a dot (or a subset preceding a dot).  We're looking for an identifier, but we're willing
		// to get distracted by a subset sequence, since that does not change the type.  Anything else does not make sense.
		if (token_type == EidosTokenType::kTokenIdentifier)
		{
			identifiers.emplace_back(token->token_string_);
			identifiers_are_calls.push_back(justFinishedParenBlock);
			identifier_positions.emplace_back(token->token_start_);
			
			// set up to continue searching the key path backwards
			lastTokenWasDot = NO;
			justFinishedParenBlock = NO;
			continue;
		}
		else if (token_type == EidosTokenType::kTokenRBracket)
		{
			bracketCount++;
			continue;
		}
		else if (token_type == EidosTokenType::kTokenRParen)
		{
			parenCount++;
			continue;
		}
		
		// This makes no sense, so bail
		return nil;
	}
	
	// If we were in the middle of tracing the key path when the loop ended, then something is wrong, bail.
	if (lastTokenWasDot || bracketCount || parenCount)
		return nil;
	
	// OK, we've got an identifier chain in identifiers, in reverse order.  We want to start at
	// the beginning of the key path, and figure out what the class of the key path root is
	int key_path_index = (int)identifiers.size() - 1;
	std::string &identifier_name = identifiers[key_path_index];
	EidosGlobalStringID identifier_ID = Eidos_GlobalStringIDForString(identifier_name);
	bool identifier_is_call = identifiers_are_calls[key_path_index];
	const EidosObjectClass *key_path_class = nullptr;
	
	if (identifier_is_call)
	{
		// The root identifier is a call, so it should be a function call; try to look it up
		for (const auto& function_iter : *functionMap)
		{
			const EidosFunctionSignature *sig = function_iter.second.get();
			
			if (sig->call_name_.compare(identifier_name) == 0)
			{
				key_path_class = sig->return_class_;
				
				// In some cases, the function signature does not have the information we need, because the class of the return value
				// of the function depends upon its parameters.  This is the case for functions like sample(), rep(), and so forth.
				// For this case, we have a special mechanism set up, whereby the EidosTypeInterpreter has logged the class of the
				// return value of function calls that it has evaluated.  We can look up the correct class in that log.  This is kind
				// of a gross solution, but short of rewriting all the completion code, it seems to be the easiest fix.  (Rewriting
				// to fix this more properly would involve doing code completion using a type-annotated tree, without any of the
				// token-stream handling that we have now; that would be a better design, but I'm going to save that rewrite for later.)
				if (!key_path_class)
				{
					auto callTypeIter = callTypeTable->find(identifier_positions[key_path_index]);
					
					if (callTypeIter != callTypeTable->end())
						key_path_class = callTypeIter->second;
				}
				
				break;
			}
		}
	}
	else if (typeTable)
	{
		// The root identifier is not a call, so it should be a global symbol; try to look it up
		EidosTypeSpecifier type_specifier = typeTable->GetTypeForSymbol(identifier_ID);
		
		if (!!(type_specifier.type_mask & kEidosValueMaskObject))
			key_path_class = type_specifier.object_class;
	}
	
	if (!key_path_class)
		return nil;				// unknown symbol at the root
	
	// Now we've got a class for the root of the key path; follow forward through the key path to arrive at the final type.
	while (--key_path_index >= 0)
	{
		identifier_name = identifiers[key_path_index];
		identifier_is_call = identifiers_are_calls[key_path_index];
		
		EidosGlobalStringID identifier_id = Eidos_GlobalStringIDForString(identifier_name);
		
		if (identifier_id == gEidosID_none)
			return nil;			// unrecognized identifier in the key path, so there is probably a typo and we can't complete off of it
		
		if (identifier_is_call)
		{
			// We have a method call; look up its signature and get the class
			const EidosCallSignature *call_signature = key_path_class->SignatureForMethod(identifier_id);
			
			if (!call_signature)
				return nil;			// no signature, so the class does not support the method given
			
			key_path_class = call_signature->return_class_;
		}
		else
		{
			// We have a property; look up its signature and get the class
			const EidosPropertySignature *property_signature = key_path_class->SignatureForProperty(identifier_id);
			
			if (!property_signature)
				return nil;			// no signature, so the class does not support the property given
			
			key_path_class = property_signature->value_class_;
		}
		
		if (!key_path_class)
			return nil;			// unknown symbol at the root; the property yields a non-object type
	}
	
	// OK, we've now got a EidosValue object that represents the end of the line; the final dot is off of this object.
	// So we want to extract all of its properties and methods, and return them all as candidates.
	NSMutableArray *candidates = [NSMutableArray array];
	const EidosObjectClass *terminus = key_path_class;
	
	// First, a sorted list of globals
	for (auto symbol_sig : *terminus->Properties())
		[candidates addObject:[NSString stringWithUTF8String:symbol_sig->property_name_.c_str()]];
	
	[candidates sortUsingSelector:@selector(compare:)];
	
	// Next, a sorted list of methods, with () appended
	for (auto method_sig : *terminus->Methods())
	{
		NSString *methodName = [NSString stringWithUTF8String:method_sig->call_name_.c_str()];
		
		[candidates addObject:[methodName stringByAppendingString:@"()"]];
	}
	
	return candidates;
}

- (NSArray *)completionsFromArray:(NSArray *)candidates matchingBase:(NSString *)base
{
	NSMutableArray *completions = [NSMutableArray array];
	NSInteger candidateCount = [candidates count];
	
#if 0
	// This is simple prefix-based completion; if a candidates begins with base, then it is used
	for (int candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex)
	{
		NSString *candidate = [candidates objectAtIndex:candidateIndex];
		
		if ([candidate hasPrefix:base])
			[completions addObject:candidate];
	}
#else
	// This is part-based completion, where iTr will complete to initializeTreeSequence() and iGTy
	// will complete to initializeGenomicElementType().  To do this, we use a special comparator
	// that returns a score for the quality of the match, and then we sort all matches by score.
	std::vector<int64_t> scores;
	NSMutableArray *unsortedCompletions = [NSMutableArray array];
	
	for (int candidateIndex = 0; candidateIndex < candidateCount; ++candidateIndex)
	{
		NSString *candidate = [candidates objectAtIndex:candidateIndex];
		int64_t score = [candidate eidosScoreAsCompletionOfString:base];
		
		if (score != INT64_MIN)
		{
			[unsortedCompletions addObject:candidate];
			scores.push_back(score);
		}
	}
	
	if (scores.size())
	{
		std::vector<int64_t> order = EidosSortIndexes(scores.data(), scores.size(), false);
		
		for (int64_t index : order)
			[completions addObject:[unsortedCompletions objectAtIndex:index]];
	}
#endif
	
	return completions;
}

- (NSArray *)completionsForTokenStream:(const std::vector<EidosToken> &)tokens index:(int)lastTokenIndex canExtend:(BOOL)canExtend withTypes:(EidosTypeTable *)typeTable functions:(EidosFunctionMap *)functionMap callTypes:(EidosCallTypeTable *)callTypeTable keywords:(NSArray *)keywords argumentNames:(NSArray *)argumentNames
{
	// What completions we offer depends on the token stream
	const EidosToken &token = tokens[lastTokenIndex];
	EidosTokenType token_type = token.token_type_;
	
	switch (token_type)
	{
		case EidosTokenType::kTokenNone:
		case EidosTokenType::kTokenEOF:
		case EidosTokenType::kTokenWhitespace:
		case EidosTokenType::kTokenComment:
		case EidosTokenType::kTokenCommentLong:
		case EidosTokenType::kTokenInterpreterBlock:
		case EidosTokenType::kTokenContextFile:
		case EidosTokenType::kTokenContextEidosBlock:
		case EidosTokenType::kFirstIdentifierLikeToken:
			// These should never be hit
			return nil;
			
		case EidosTokenType::kTokenIdentifier:
		case EidosTokenType::kTokenIf:
		case EidosTokenType::kTokenWhile:
		case EidosTokenType::kTokenFor:
		case EidosTokenType::kTokenNext:
		case EidosTokenType::kTokenBreak:
		case EidosTokenType::kTokenFunction:
		case EidosTokenType::kTokenReturn:
		case EidosTokenType::kTokenElse:
		case EidosTokenType::kTokenDo:
		case EidosTokenType::kTokenIn:
			if (canExtend)
			{
				NSMutableArray *completions = nil;
				
				// This is the tricky case, because the identifier we're extending could be the end of a key path like foo.bar[5:8].ba...
				// We need to move backwards from the current token until we find or fail to find a dot token; if we see a dot we're in
				// a key path, otherwise we're in the global context and should filter from those candidates
				for (int previousTokenIndex = lastTokenIndex - 1; previousTokenIndex >= 0; --previousTokenIndex)
				{
					const EidosToken &previous_token = tokens[previousTokenIndex];
					EidosTokenType previous_token_type = previous_token.token_type_;
					
					// if the token we're on is skippable, continue backwards
					if ((previous_token_type == EidosTokenType::kTokenWhitespace) || (previous_token_type == EidosTokenType::kTokenComment) || (previous_token_type == EidosTokenType::kTokenCommentLong))
						continue;
					
					// if the token we're on is a dot, we are indeed at the end of a key path, and can fetch the completions for it
					if (previous_token_type == EidosTokenType::kTokenDot)
					{
						completions = [self completionsForKeyPathEndingInTokenIndex:previousTokenIndex ofTokenStream:tokens withTypes:typeTable functions:functionMap callTypes:callTypeTable keywords:keywords];
						break;
					}
					
					// if we see a semicolon or brace, we are in a completely global context
					if ((previous_token_type == EidosTokenType::kTokenSemicolon) || (previous_token_type == EidosTokenType::kTokenLBrace) || (previous_token_type == EidosTokenType::kTokenRBrace))
					{
						completions = [self globalCompletionsWithTypes:typeTable functions:functionMap keywords:keywords argumentNames:nil];
						break;
					}
					
					// if we see any other token, we are not in a key path; let's assume we're following an operator
					completions = [self globalCompletionsWithTypes:typeTable functions:functionMap keywords:nil argumentNames:argumentNames];
					break;
				}
				
				// If we ran out of tokens, we're at the beginning of the file and so in the global context
				if (!completions)
					completions = [self globalCompletionsWithTypes:typeTable functions:functionMap keywords:keywords argumentNames:nil];
				
				// Now we have an array of possible completions; we just need to remove those that don't complete the base string,
				// according to a heuristic algorithm, and sort those that do match by a score of their closeness of match.
				return [self completionsFromArray:completions matchingBase:[NSString stringWithUTF8String:token.token_string_.c_str()]];
			}
			else if ((token_type == EidosTokenType::kTokenReturn) || (token_type == EidosTokenType::kTokenElse) || (token_type == EidosTokenType::kTokenDo) || (token_type == EidosTokenType::kTokenIn))
			{
				// If you can't extend and you're following an identifier, you presumably need an operator or a keyword or something;
				// you can't have two identifiers in a row.  The same is true of keywords that do not take an expression after them.
				// But return, else, do, and in can be followed immediately by an expression, so here we handle that case.  Identifiers
				// and other keywords will drop through to return nil below, expressing that we cannot complete in that case.
				// We used to put return, else, do, and in down the the operators at the bottom, but when canExtend is YES that
				// prevents them from completing to other things ("in" to "inSLiMgui", for example); moving them up to this case
				// allows that completion to work, but necessitates the addition of this block to get the correct functionality when
				// canExtend is NO.  BCH 1/22/2019
				return [self globalCompletionsWithTypes:typeTable functions:functionMap keywords:nil argumentNames:argumentNames];
			}
			
			// If the previous token was an identifier and we can't extend it, the next thing probably needs to be an operator or something
			return nil;
			
		case EidosTokenType::kTokenBad:
		case EidosTokenType::kTokenNumber:
		case EidosTokenType::kTokenString:
		case EidosTokenType::kTokenRParen:
		case EidosTokenType::kTokenRBracket:
		case EidosTokenType::kTokenSingleton:
			// We don't have anything to suggest after such tokens; the next thing will need to be an operator, semicolon, etc.
			return nil;
			
		case EidosTokenType::kTokenDot:
			// This is the other tricky case, because we're being asked to extend a key path like foo.bar[5:8].
			return [self completionsForKeyPathEndingInTokenIndex:lastTokenIndex ofTokenStream:tokens withTypes:typeTable functions:functionMap callTypes:callTypeTable keywords:keywords];
			
		case EidosTokenType::kTokenSemicolon:
		case EidosTokenType::kTokenLBrace:
		case EidosTokenType::kTokenRBrace:
			// We are in the global context and anything goes, including a new statement
			return [self globalCompletionsWithTypes:typeTable functions:functionMap keywords:keywords argumentNames:nil];
			
		case EidosTokenType::kTokenColon:
		case EidosTokenType::kTokenComma:
		case EidosTokenType::kTokenLParen:
		case EidosTokenType::kTokenLBracket:
		case EidosTokenType::kTokenPlus:
		case EidosTokenType::kTokenMinus:
		case EidosTokenType::kTokenMod:
		case EidosTokenType::kTokenMult:
		case EidosTokenType::kTokenExp:
		case EidosTokenType::kTokenAnd:
		case EidosTokenType::kTokenOr:
		case EidosTokenType::kTokenDiv:
		case EidosTokenType::kTokenConditional:
		case EidosTokenType::kTokenAssign:
		case EidosTokenType::kTokenEq:
		case EidosTokenType::kTokenLt:
		case EidosTokenType::kTokenLtEq:
		case EidosTokenType::kTokenGt:
		case EidosTokenType::kTokenGtEq:
		case EidosTokenType::kTokenNot:
		case EidosTokenType::kTokenNotEq:
			// We are following an operator or similar, so globals are OK but new statements are not
			return [self globalCompletionsWithTypes:typeTable functions:functionMap keywords:nil argumentNames:argumentNames];
	}
	
	return nil;
}

- (NSUInteger)rangeOffsetForCompletionRange
{
	// This is for EidosConsoleTextView to be able to remove the prompt string from the string being completed
	return 0;
}

- (NSArray *)uniquedArgumentNameCompletions:(std::vector<std::string> *)argumentCompletions
{
	// put argument-name completions, if any, at the top of the list; we unique them (preserving order) and add "="
	if (argumentCompletions && argumentCompletions->size())
	{
		NSMutableArray *completionsWithArgs = [NSMutableArray array];
		
		for (std::string &arg_completion : *argumentCompletions)
		{
			NSString *argNameString = [NSString stringWithUTF8String:arg_completion.c_str()];
			NSString *argNameWithEquals = [argNameString stringByAppendingString:@"="];
			
			if (![completionsWithArgs containsObject:argNameWithEquals])
				[completionsWithArgs addObject:argNameWithEquals];
		}
		
		return completionsWithArgs;
	}
	
	return nil;
}

// one funnel for all completion work, since we use the same pattern to answer both questions...
- (void)_completionHandlerWithRangeForCompletion:(NSRange *)baseRange completions:(NSArray **)completions
{
	NSString *scriptString = [self string];
	NSRange selection = [self selectedRange];	// ignore charRange and work from the selection
	NSUInteger rangeOffset = [self rangeOffsetForCompletionRange];
	
	// correct the script string to have only what is entered after the prompt, if we are a EidosConsoleTextView
	if (rangeOffset)
	{
		scriptString = [scriptString substringFromIndex:rangeOffset];
		selection.location -= rangeOffset;
		selection.length -= rangeOffset;
	}
	
	NSUInteger selStart = selection.location;
	
	if (selStart != NSNotFound)
	{
		// Get the substring up to the start of the selection; that is the range relevant for completion
		NSString *scriptSubstring = [scriptString substringToIndex:selStart];
		std::string script_string([scriptSubstring UTF8String]);
		
		// Do shared completion processing that can be intercepted by our delegate: getting a type table for defined variables,
		// as well as a function map and any added language keywords, all of which depend upon the point of completion
		id delegate = [self delegate];
		EidosTypeTable typeTable;
		EidosTypeTable *typeTablePtr = &typeTable;
		EidosFunctionMap functionMap(*EidosInterpreter::BuiltInFunctionMap());
		EidosFunctionMap *functionMapPtr = &functionMap;
		EidosCallTypeTable callTypeTable;
		EidosCallTypeTable *callTypeTablePtr = &callTypeTable;
		NSMutableArray *keywords = [NSMutableArray arrayWithObjects:@"break", @"do", @"else", @"for", @"if", @"in", @"next", @"return", @"while", @"function", nil];
		std::vector<std::string> argumentCompletions;
		BOOL delegateHandled = NO;
		
		if ([delegate respondsToSelector:@selector(eidosTextView:completionContextWithScriptString:selection:typeTable:functionMap:callTypeTable:keywords:argumentNameCompletions:)])
		{
			delegateHandled = [delegate eidosTextView:self completionContextWithScriptString:scriptSubstring selection:selection typeTable:&typeTablePtr functionMap:&functionMapPtr callTypeTable:&callTypeTablePtr keywords:keywords argumentNameCompletions:&argumentCompletions];
		}
		
		// set up automatic disposal of a substitute type table or function map provided by delegate
		std::unique_ptr<EidosTypeTable> raii_typeTablePtr((typeTablePtr != &typeTable) ? typeTablePtr : nullptr);
		std::unique_ptr<EidosFunctionMap> raii_functionMapPtr((functionMapPtr != &functionMap) ? functionMapPtr : nullptr);
		std::unique_ptr<EidosCallTypeTable> raii_callTypeTablePtr((callTypeTablePtr != &callTypeTable) ? callTypeTablePtr : nullptr);
		
		if (!delegateHandled)
		{
			// First, set up a base type table using the symbol table
			EidosSymbolTable *symbols = gEidosConstantsSymbolTable;
			
			if ([delegate respondsToSelector:@selector(eidosTextView:symbolsFromBaseSymbols:)])
				symbols = [delegate eidosTextView:self symbolsFromBaseSymbols:symbols];
			
			if (symbols)
				symbols->AddSymbolsToTypeTable(typeTablePtr);
			
			// Next, a definitive function map that covers all functions defined in the entire script string (not just the script above
			// the completion point); this seems best, for mutually recursive functions etc..  Duplicate it back into functionMap and
			// delete the original, so we don't get confused.
			EidosFunctionMap *definitive_function_map = [self functionMapForScriptString:scriptString includingOptionalFunctions:NO];
			
			functionMap = *definitive_function_map;
			delete definitive_function_map;
			
			// Next, add type table entries based on parsing and analysis of the user's code
			EidosScript script(script_string);
			
#if EIDOS_DEBUG_COMPLETION
			std::cout << "Eidos script:\n" << script_string << std::endl << std::endl;
#endif
			
			script.Tokenize(true, false);					// make bad tokens as needed, do not keep nonsignificant tokens
			script.ParseInterpreterBlockToAST(true, true);	// make bad nodes as needed (i.e. never raise, and produce a correct tree)
			
#if EIDOS_DEBUG_COMPLETION
			std::ostringstream parse_stream;
			script.PrintAST(parse_stream);
			std::cout << "Eidos AST:\n" << parse_stream.str() << std::endl << std::endl;
#endif
			
			EidosTypeInterpreter typeInterpreter(script, *typeTablePtr, *functionMapPtr, *callTypeTablePtr);
			
			typeInterpreter.TypeEvaluateInterpreterBlock_AddArgumentCompletions(&argumentCompletions, script_string.length());	// result not used
		}
		
#if EIDOS_DEBUG_COMPLETION
		std::cout << "Type table:\n" << *typeTablePtr << std::endl;
#endif
		
		// Tokenize; we can't use the tokenization done above, as we want whitespace tokens here...
		EidosScript script(script_string);
		script.Tokenize(true, true);	// make bad tokens as needed, keep nonsignificant tokens
		
#if EIDOS_DEBUG_COMPLETION
		std::cout << "Eidos token stream:" << std::endl;
		script.PrintTokens(std::cout);
#endif
		
		const std::vector<EidosToken> &tokens = script.Tokens();
		int lastTokenIndex = (int)tokens.size() - 1;
		BOOL endedCleanly = NO, lastTokenInterrupted = NO;
		
		// if we ended with an EOF, that means we did not have a raise and there should be no untokenizable range at the end
		if ((lastTokenIndex >= 0) && (tokens[lastTokenIndex].token_type_ == EidosTokenType::kTokenEOF))
		{
			--lastTokenIndex;
			endedCleanly = YES;
		}
		
		// if we are at the end of a comment, without whitespace following it, then we are actually in the comment, and cannot complete
		// BCH 5 August 2017: Note that EidosTokenType::kTokenCommentLong is deliberately omitted here; this rule does not apply to it
		if ((lastTokenIndex >= 0) && (tokens[lastTokenIndex].token_type_ == EidosTokenType::kTokenComment))
		{
			if (baseRange) *baseRange = NSMakeRange(NSNotFound, 0);
			if (completions) *completions = nil;
			return;
		}
		
		// if we ended with whitespace or a comment, the previous token cannot be extended
		while (lastTokenIndex >= 0) {
			const EidosToken &token = tokens[lastTokenIndex];
			
			if ((token.token_type_ != EidosTokenType::kTokenWhitespace) && (token.token_type_ != EidosTokenType::kTokenComment) && (token.token_type_ != EidosTokenType::kTokenCommentLong))
				break;
			
			--lastTokenIndex;
			lastTokenInterrupted = YES;
		}
		
		// now diagnose what range we want to use as a basis for completion
		if (!endedCleanly)
		{
			// the selection is at the end of an untokenizable range; we might be in the middle of a string or a comment,
			// or there might be a tokenization error upstream of us.  let's not try to guess what the situation is.
			if (baseRange) *baseRange = NSMakeRange(NSNotFound, 0);
			if (completions) *completions = nil;
			return;
		}
		else
		{
			if (lastTokenIndex < 0)
			{
				// We're at the end of nothing but initial whitespace and comments; or if (!lastTokenInterrupted),
				// we're at the very beginning of the file.  Either way, offer insertion-point completions.
				if (baseRange) *baseRange = NSMakeRange(selection.location + rangeOffset, 0);
				if (completions) *completions = [self globalCompletionsWithTypes:typeTablePtr functions:functionMapPtr keywords:keywords argumentNames:nil];
				return;
			}
			
			const EidosToken &token = tokens[lastTokenIndex];
			EidosTokenType token_type = token.token_type_;
			
			// BCH 31 May 2016: If the previous token is a right-paren, that is a tricky case because we could be following
			// for(), an if(), or while (), in which case we should allow an identifier to follow the right paren, or we could
			// be following parentheses for grouping, i.e. (a+b), or parentheses for a function call, foo(), in which case we
			// should not allow an identifier to follow the right paren.  This annoyance is basically because the right paren
			// serves a lot of different functions in the language and so just knowing that we are after one is not sufficient.
			// So we will walk backwards, balancing our parenthesis count, to try to figure out which case we are in.  Note
			// that even this code is not quite right; it mischaracterizes the do...while() case as allowing an identifier to
			// follow, because it sees the "while".  This is harder to fix, and do...while() is not a common construct, and
			// the mistake is pretty harmless, so whatever.
			if (token_type == EidosTokenType::kTokenRParen)
			{
				int parenCount = 1;
				int walkbackIndex = lastTokenIndex;
				
				// First walk back until our paren count balances
				while (--walkbackIndex >= 0)
				{
					const EidosToken &walkback_token = tokens[walkbackIndex];
					EidosTokenType walkback_token_type = walkback_token.token_type_;
					
					if (walkback_token_type == EidosTokenType::kTokenRParen)			parenCount++;
					else if (walkback_token_type == EidosTokenType::kTokenLParen)		parenCount--;
					
					if (parenCount == 0)
						break;
				}
				
				// Then walk back over whitespace, and if the first non-white thing we see is right, allow completion
				while (--walkbackIndex >= 0)
				{
					const EidosToken &walkback_token = tokens[walkbackIndex];
					EidosTokenType walkback_token_type = walkback_token.token_type_;
					
					if ((walkback_token_type != EidosTokenType::kTokenWhitespace) && (walkback_token_type != EidosTokenType::kTokenComment) && (walkback_token_type != EidosTokenType::kTokenCommentLong))
					{
						if ((walkback_token_type == EidosTokenType::kTokenFor) || (walkback_token_type == EidosTokenType::kTokenWhile) || (walkback_token_type == EidosTokenType::kTokenIf))
						{
							// We are at the end of for(), if(), or while(), so we allow global completions as if we were after a semicolon
							if (baseRange) *baseRange = NSMakeRange(selection.location + rangeOffset, 0);
							if (completions) *completions = [self globalCompletionsWithTypes:typeTablePtr functions:functionMapPtr keywords:keywords argumentNames:nil];
							return;
						}
						break;	// we didn't hit one of the favored cases, so the code below will reject completion
					}
				}
			}
			
			if (lastTokenInterrupted)
			{
				// the last token cannot be extended, so if the last token is something an identifier can follow, like an
				// operator, then we can offer completions at the insertion point based on that, otherwise punt.
				if ((token_type == EidosTokenType::kTokenNumber) || (token_type == EidosTokenType::kTokenString) || (token_type == EidosTokenType::kTokenRParen) || (token_type == EidosTokenType::kTokenRBracket) || (token_type == EidosTokenType::kTokenIdentifier) || (token_type == EidosTokenType::kTokenIf) || (token_type == EidosTokenType::kTokenWhile) || (token_type == EidosTokenType::kTokenFor) || (token_type == EidosTokenType::kTokenNext) || (token_type == EidosTokenType::kTokenBreak) || (token_type == EidosTokenType::kTokenFunction))
				{
					if (baseRange) *baseRange = NSMakeRange(NSNotFound, 0);
					if (completions) *completions = nil;
					return;
				}
				
				if (baseRange) *baseRange = NSMakeRange(selection.location + rangeOffset, 0);
				if (completions)
				{
					NSArray *argumentCompletionsArray = [self uniquedArgumentNameCompletions:&argumentCompletions];
					
					*completions = [self completionsForTokenStream:tokens index:lastTokenIndex canExtend:NO withTypes:typeTablePtr functions:functionMapPtr callTypes:callTypeTablePtr keywords:keywords argumentNames:argumentCompletionsArray];
				}
				
				return;
			}
			else
			{
				// the last token was not interrupted, so we can offer completions of it if we want to.
				NSRange tokenRange = NSMakeRange(token.token_UTF16_start_, token.token_UTF16_end_ - token.token_UTF16_start_ + 1);
				
				if (token_type >= EidosTokenType::kTokenIdentifier)
				{
					if (baseRange) *baseRange = NSMakeRange(tokenRange.location + rangeOffset, tokenRange.length);
					if (completions)
					{
						NSArray *argumentCompletionsArray = [self uniquedArgumentNameCompletions:&argumentCompletions];
						
						*completions = [self completionsForTokenStream:tokens index:lastTokenIndex canExtend:YES withTypes:typeTablePtr functions:functionMapPtr callTypes:callTypeTablePtr keywords:keywords argumentNames:argumentCompletionsArray];
					}
					return;
				}
				
				if ((token_type == EidosTokenType::kTokenNumber) || (token_type == EidosTokenType::kTokenString) || (token_type == EidosTokenType::kTokenRParen) || (token_type == EidosTokenType::kTokenRBracket))
				{
					if (baseRange) *baseRange = NSMakeRange(NSNotFound, 0);
					if (completions) *completions = nil;
					return;
				}
				
				if (baseRange) *baseRange = NSMakeRange(selection.location + rangeOffset, 0);
				if (completions)
				{
					NSArray *argumentCompletionsArray = [self uniquedArgumentNameCompletions:&argumentCompletions];
					
					*completions = [self completionsForTokenStream:tokens index:lastTokenIndex canExtend:NO withTypes:typeTablePtr functions:functionMapPtr callTypes:callTypeTablePtr keywords:keywords argumentNames:argumentCompletionsArray];
				}
				return;
			}
		}
	}
}

@end


//
//	EidosTextStorage
//
#pragma mark -
#pragma mark EidosTextStorage

@interface EidosTextStorage ()
{
	NSMutableAttributedString *contents;
}
@end

@implementation EidosTextStorage

- (id)initWithAttributedString:(NSAttributedString *)attrStr
{
	if (self = [super init])
	{
		contents = attrStr ? [attrStr mutableCopy] : [[NSMutableAttributedString alloc] init];
	}
	return self;
}

- init
{
	return [self initWithAttributedString:nil];
}

- (void)dealloc
{
	[contents release];
	[super dealloc];
}

// The next set of methods are the primitives for attributed and mutable attributed string...

- (NSString *)string
{
	return [contents string];
}

- (NSDictionary *)attributesAtIndex:(NSUInteger)location effectiveRange:(NSRange *)range
{
	return [contents attributesAtIndex:location effectiveRange:range];
}

- (void)replaceCharactersInRange:(NSRange)range withString:(NSString *)str
{
	NSUInteger origLen = [self length];
	[contents replaceCharactersInRange:range withString:str];
	[self edited:NSTextStorageEditedCharacters range:range changeInLength:[self length] - origLen];
}

- (void)setAttributes:(NSDictionary *)attrs range:(NSRange)range
{
	[contents setAttributes:attrs range:range];
	[self edited:NSTextStorageEditedAttributes range:range changeInLength:0];
}

// And now the actual reason for this subclass: to provide code-aware word selection behavior

- (NSRange)doubleClickAtIndex:(NSUInteger)location
{
	// Start by calling super to get a proposed range.  This is documented to raise if location >= [self length]
	// or location < 0, so in the code below we can assume that location indicates a valid character position.
	NSRange superRange = [super doubleClickAtIndex:location];
	NSString *string = [self string];
	NSUInteger stringLength = [string length];
	unichar uch = [string characterAtIndex:location];
	
	// If the user has actually double-clicked a period, we want to just return the range of the period.
	if (uch == '.')
		return NSMakeRange(location, 1);
	
	// Two-character tokens should be considered words, so look for those and fix as needed
	if (location < stringLength - 1)
	{
		// we have another character following us, so let's get it and check for cases
		unichar uch_after = [string characterAtIndex:location + 1];
		
		if (((uch == '/') && (uch_after == '/')) ||
			((uch == '=') && (uch_after == '=')) ||
			((uch == '<') && (uch_after == '=')) ||
			((uch == '>') && (uch_after == '=')) ||
			((uch == '!') && (uch_after == '=')))
			return NSMakeRange(location, 2);
	}
	
	if (location > 0)
	{
		// we have another character preceding us, so let's get it and check for cases
		unichar uch_before = [string characterAtIndex:location - 1];
		
		if (((uch_before == '/') && (uch == '/')) ||
			((uch_before == '=') && (uch == '=')) ||
			((uch_before == '<') && (uch == '=')) ||
			((uch_before == '>') && (uch == '=')) ||
			((uch_before == '!') && (uch == '=')))
			return NSMakeRange(location - 1, 2);
	}
	
	// Another case where super's behavior is wrong involves the dot operator; x.y should not be considered a word.
	// So we check for a period before or after the anchor position, and trim away the periods and everything
	// past them on both sides.  This will correctly handle longer sequences like foo.bar.baz.is.a.test.
	NSRange candidateRangeBeforeLocation = NSMakeRange(superRange.location, location - superRange.location);
	NSRange candidateRangeAfterLocation = NSMakeRange(location + 1, NSMaxRange(superRange) - (location + 1));
	NSRange periodBeforeRange = [string rangeOfString:@"." options:NSBackwardsSearch range:candidateRangeBeforeLocation];
	NSRange periodAfterRange = [string rangeOfString:@"." options:(NSStringCompareOptions)0 range:candidateRangeAfterLocation];
	
	if (periodBeforeRange.location != NSNotFound)
	{
		// Change superRange to start after the preceding period; fix its length so its end remains unchanged.
		superRange.length -= (periodBeforeRange.location + 1 - superRange.location);
		superRange.location = periodBeforeRange.location + 1;
	}
	
	if (periodAfterRange.location != NSNotFound)
	{
		// Change superRange to end before the following period
		superRange.length -= (NSMaxRange(superRange) - periodAfterRange.location);
	}
	
	return superRange;
}

@end




























































