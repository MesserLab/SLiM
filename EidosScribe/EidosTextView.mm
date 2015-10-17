//
//  EidosTextView.mm
//  EidosScribe
//
//  Created by Ben Haller on 6/14/15.
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


#import "EidosTextView.h"
#import "EidosTextViewDelegate.h"
#import "EidosConsoleTextView.h"
#import "EidosCocoaExtra.h"
#import "EidosHelpController.h"

#include "eidos_script.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#include <stdexcept>


using std::vector;
using std::string;


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
		_shouldRecolorAfterChanges = YES;
	}
	
	return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
	if (self = [super initWithCoder:coder])
	{
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
	[self setFont:[NSFont fontWithName:@"Menlo" size:11.0]];
	[self setTypingAttributes:[NSDictionary eidosTextAttributesWithColor:nil]];
	
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

// NSTextView copies only plain text for us, because it is set to have rich text turned off.  That setting only means it is turned off for the user; the
// user can't change the font, size, etc.  But we still can, and do, programatically to do our syntax formatting.  We want that style information to get
// copied to the pasteboard, and as far as I can tell this subclass is necessary to make it happen.  Seems kind of lame.
- (IBAction)copy:(id)sender
{
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	NSAttributedString *attrString = [self textStorage];
	NSRange selectedRange = [self selectedRange];
	NSAttributedString *attrStringInRange = [attrString attributedSubstringFromRange:selectedRange];
	
	// The documentation sucks, but as far as I can tell, this puts both a plain-text and a rich-text representation on the pasteboard
	[pasteboard clearContents];
	[pasteboard writeObjects:@[attrStringInRange]];
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
					[ts setAttributes:[NSDictionary eidosTextAttributesWithColor:[NSColor blackColor]] range:NSMakeRange(scanPosition, 2)];
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
	NSEventModifierFlags modifiers = [theEvent modifierFlags] & NSDeviceIndependentModifierFlagsMask;
	
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
				[[helpController window] orderFront:self];
			}
		}
		
		return;
	}
	
	// Start out willing to work with a double-click for purposes of delimiter-balancing;
	// see selectionRangeForProposedRange:proposedCharRange granularity: below
	inEligibleDoubleClick = YES;
	
	[super mouseDown:theEvent];
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
						const std::vector<EidosToken *> &tokens = script.Tokens();
						int token_count = (int)tokens.size();
						EidosToken *click_token = nullptr;
						int click_token_index;
						
						for (click_token_index = 0; click_token_index < token_count; ++click_token_index)
						{
							click_token = tokens[click_token_index];
							
							if ((click_token->token_UTF16_start_ <= proposedCharacterIndex) && (click_token->token_UTF16_end_ >= proposedCharacterIndex))
								break;
						}
						
						// OK, this token contains the character that was double-clicked.  We could have the actual token
						// (incrementToken), we could be in a string, or we could be in a comment.  We stay in the domain
						// that we start in; if in a string, for example, only delimiters within strings affect our scan.
						EidosTokenType click_token_type = click_token->token_type_;
						EidosToken *scan_token = click_token;
						int scan_token_index = click_token_index;
						NSInteger scanPosition = proposedCharacterIndex;
						int balanceCount = 0;
						
						while (YES)
						{
							EidosTokenType scan_token_type = scan_token->token_type_;
							BOOL isCandidate;
							
							if (click_token_type == EidosTokenType::kTokenComment)
								isCandidate = (scan_token_type == EidosTokenType::kTokenComment);
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
								
								scan_token = tokens[scan_token_index];
							}
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
	
	if (!numberLiteralColor)
	{
		numberLiteralColor = [[NSColor colorWithCalibratedRed:28/255.0 green:0/255.0 blue:207/255.0 alpha:1.0] retain];
		stringLiteralColor = [[NSColor colorWithCalibratedRed:196/255.0 green:26/255.0 blue:22/255.0 alpha:1.0] retain];
		commentColor = [[NSColor colorWithCalibratedRed:0/255.0 green:116/255.0 blue:0/255.0 alpha:1.0] retain];
		identifierColor = [[NSColor colorWithCalibratedRed:63/255.0 green:110/255.0 blue:116/255.0 alpha:1.0] retain];
		keywordColor = [[NSColor colorWithCalibratedRed:170/255.0 green:13/255.0 blue:145/255.0 alpha:1.0] retain];
	}
	
	// Syntax color!
	NSTextStorage *ts = [self textStorage];
	
	[ts beginEditing];
	
	[ts removeAttribute:NSForegroundColorAttributeName range:NSMakeRange(0, [ts length])];
	
	for (EidosToken *token : script.Tokens())
	{
		NSRange tokenRange = NSMakeRange(token->token_UTF16_start_, token->token_UTF16_end_ - token->token_UTF16_start_ + 1);
		
		if (token->token_type_ == EidosTokenType::kTokenNumber)
			[ts addAttribute:NSForegroundColorAttributeName value:numberLiteralColor range:tokenRange];
		if (token->token_type_ == EidosTokenType::kTokenString)
			[ts addAttribute:NSForegroundColorAttributeName value:stringLiteralColor range:tokenRange];
		if (token->token_type_ == EidosTokenType::kTokenComment)
			[ts addAttribute:NSForegroundColorAttributeName value:commentColor range:tokenRange];
		if (token->token_type_ > EidosTokenType::kFirstIdentifierLikeToken)
			[ts addAttribute:NSForegroundColorAttributeName value:keywordColor range:tokenRange];
		if (token->token_type_ == EidosTokenType::kTokenIdentifier)
		{
			// most identifiers are left as black; only special ones get colored
			const std::string &token_string = token->token_string_;
			
			if ((token_string.compare("T") == 0) ||
				(token_string.compare("F") == 0) ||
				(token_string.compare("E") == 0) ||
				(token_string.compare("PI") == 0) ||
				(token_string.compare("INF") == 0) ||
				(token_string.compare("NAN") == 0) ||
				(token_string.compare("NULL") == 0))
				[ts addAttribute:NSForegroundColorAttributeName value:identifierColor range:tokenRange];
			else
			{
				// we also let the Context specify special identifiers that we will syntax color
				if ([delegate respondsToSelector:@selector(eidosTextView:tokenStringIsSpecialIdentifier:)])
					if ([delegate eidosTextView:self tokenStringIsSpecialIdentifier:token_string])
						[ts addAttribute:NSForegroundColorAttributeName value:identifierColor range:tokenRange];
			}
		}
	}
	
	[ts endEditing];
}

- (void)syntaxColorForOutput
{
	NSTextStorage *textStorage = [self textStorage];
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
	[textStorage beginEditing];
	[textStorage removeAttribute:NSForegroundColorAttributeName range:NSMakeRange(0, [textStorage length])];
	
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
				
				[textStorage addAttributes:commentAttrs range:NSMakeRange(lineRange.location + commentRange.location, commentLength)];
				
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
						[textStorage addAttributes:poundDirectiveAttrs range:lineRange];
					else
					{
						NSRange scanRange = NSMakeRange(0, lineRange.length);
						
						do {
							NSRange tokenRange = [line rangeOfString:@"\\b[pgm][0-9]+\\b" options:NSRegularExpressionSearch range:scanRange];
							
							if (tokenRange.location == NSNotFound || tokenRange.length == 0)
								break;
							
							NSString *substring = [line substringWithRange:tokenRange];
							NSDictionary *syntaxAttrs = nil;
							
							if ([substring characterAtIndex:0] == 'p')
								syntaxAttrs = subpopAttrs;
							else if ([substring characterAtIndex:0] == 'g')
								syntaxAttrs = genomicElementAttrs;
							else if ([substring characterAtIndex:0] == 'm')
								syntaxAttrs = mutationTypeAttrs;
							
							if (syntaxAttrs)
								[textStorage addAttributes:syntaxAttrs range:NSMakeRange(tokenRange.location + lineRange.location, tokenRange.length)];
							
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
	
	[textStorage endEditing];
}

- (void)clearSyntaxColoring
{
	NSTextStorage *textStorage = [self textStorage];
	
	[textStorage beginEditing];
	[textStorage removeAttribute:NSForegroundColorAttributeName range:NSMakeRange(0, [textStorage length])];
	[textStorage endEditing];
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

- (void)recolorAfterChanges
{
	// We fold in syntax coloring as part of every change set.  If _syntaxColoring==NoSyntaxColoring, we don't do
	// anything on text changes; we only clear attributes when NoSyntaxColoring is initially set on the textview.
	if (_syntaxColoring != kEidosSyntaxColoringNone)
		[self updateSyntaxColoring];
}

- (void)textStorageDidProcessEditing:(NSNotification *)notification
{
	// I used to do this in an override of -didChangeText, but that did not work well; I think the text system
	// did not expect attribute changes at that time.  This delegate method is specifically intended for this.
	if (_shouldRecolorAfterChanges)
		[self recolorAfterChanges];
}


//
//	Signature display
//
#pragma mark -
#pragma mark Signature display

- (NSAttributedString *)attributedSignatureForScriptString:(NSString *)scriptString selection:(NSRange)selection
{
	if ([scriptString length])
	{
		std::string script_string([scriptString UTF8String]);
		EidosScript script(script_string);
		
		// Tokenize
		script.Tokenize(true, true);	// make bad tokens as needed, keep nonsignificant tokens
		
		const vector<EidosToken *> &tokens = script.Tokens();
		int tokenCount = (int)tokens.size();
		
		//NSLog(@"script string \"%@\" contains %d tokens", scriptString, tokenCount);
		
		// Search forward to find the token position of the start of the selection
		int selectionStart = (int)selection.location;
		int tokenIndex;
		
		for (tokenIndex = 0; tokenIndex < tokenCount; ++tokenIndex)
			if (tokens[tokenIndex]->token_UTF16_start_ >= selectionStart)
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
			EidosToken *token = tokens[backscanIndex];
			EidosTokenType tokenType = token->token_type_;
			
			if (tokenType == EidosTokenType::kTokenLParen)
			{
				--parenCount;
				
				if (parenCount < lowestParenCountSeen)
				{
					EidosToken *previousToken = tokens[backscanIndex - 1];
					EidosTokenType previousTokenType = previousToken->token_type_;
					
					if (previousTokenType == EidosTokenType::kTokenIdentifier)
					{
						// OK, we found the pattern "identifier("; extract the name of the function/method
						// We also figure out here whether it is a method call (tokens like ".identifier(") or not
						NSString *callName = [NSString stringWithUTF8String:previousToken->token_string_.c_str()];
						BOOL isMethodCall = NO;
						
						if ((backscanIndex > 1) && (tokens[backscanIndex - 2]->token_type_ == EidosTokenType::kTokenDot))
							isMethodCall = YES;
						
						return [self attributedSignatureForCallName:callName isMethodCall:isMethodCall];
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

- (NSAttributedString *)attributedSignatureForCallName:(NSString *)callName isMethodCall:(BOOL)isMethodCall
{
	// We need to figure out what the signature is for the call name.
	std::string call_name([callName UTF8String]);
	id delegate = [self delegate];
	
	if (!isMethodCall)
	{
		// Look for a matching function signature, including those from the Context
		EidosFunctionMap *functionMap = EidosInterpreter::BuiltInFunctionMap();
		
		if ([delegate respondsToSelector:@selector(eidosTextView:functionMapFromBaseMap:)])
			functionMap = [delegate eidosTextView:self functionMapFromBaseMap:functionMap];
		
		for (const auto& function_iter : *functionMap)
		{
			const EidosFunctionSignature *sig = function_iter.second;
			const std::string &sig_call_name = sig->function_name_;
			
			if (sig_call_name.compare(call_name) == 0)
				return [NSAttributedString eidosAttributedStringForCallSignature:sig];
		}
	}
	
	if (isMethodCall)
	{
		// Look for a method in the global method registry last; for this to work, the Context must register all methods with Eidos
		const std::vector<const EidosMethodSignature *> *methodSignatures = nullptr;
		
		if ([delegate respondsToSelector:@selector(eidosTextViewAllMethodSignatures:)])
			methodSignatures = [delegate eidosTextViewAllMethodSignatures:self];
		
		if (!methodSignatures)
			methodSignatures = gEidos_UndefinedClassObject->Methods();
		
		for (const EidosMethodSignature *sig : *methodSignatures)
		{
			const std::string &sig_call_name = sig->function_name_;
			
			if (sig_call_name.compare(call_name) == 0)
				return [NSAttributedString eidosAttributedStringForCallSignature:sig];
		}
	}
	
	// Assemble an attributed string for our failed lookup message
	NSMutableAttributedString *attrStr = [[[NSMutableAttributedString alloc] init] autorelease];
	NSDictionary *plainAttrs = [NSDictionary eidosOutputAttrs];
	NSDictionary *functionAttrs = [NSDictionary eidosParseAttrs];
	
	[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:callName attributes:functionAttrs] autorelease]];
	[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"() – unrecognized call" attributes:plainAttrs] autorelease]];
	[attrStr addAttribute:NSBaselineOffsetAttributeName value:[NSNumber numberWithFloat:2.0] range:NSMakeRange(0, [attrStr length])];
	
	return attrStr;
}


//
//	Auto-completion
//
#pragma mark -
#pragma mark Auto-completion

- (NSArray *)completionsForPartialWordRange:(NSRange)charRange indexOfSelectedItem:(NSInteger *)index
{
	NSArray *completions = nil;
	
	[self _completionHandlerWithRangeForCompletion:NULL completions:&completions];
	
	return completions;
}

- (NSRange)rangeForUserCompletion
{
	NSRange baseRange = NSMakeRange(NSNotFound, 0);
	
	[self _completionHandlerWithRangeForCompletion:&baseRange completions:NULL];
	
	return baseRange;
}

- (NSMutableArray *)globalCompletionsIncludingStatements:(BOOL)includeStatements
{
	NSMutableArray *globals = [NSMutableArray array];
	id delegate = [self delegate];
	
	// First, a sorted list of globals
	EidosSymbolTable *globalSymbolTable = gEidosConstantsSymbolTable;
	
	if ([delegate respondsToSelector:@selector(eidosTextView:symbolsFromBaseSymbols:)])
		globalSymbolTable = [delegate eidosTextView:self symbolsFromBaseSymbols:globalSymbolTable];
	
	for (std::string &symbol_name : globalSymbolTable->AllSymbols())
		[globals addObject:[NSString stringWithUTF8String:symbol_name.c_str()]];
	
	[globals sortUsingSelector:@selector(compare:)];
	
	// Next, a sorted list of functions, with () appended
	EidosFunctionMap *function_map = EidosInterpreter::BuiltInFunctionMap();
	
	if ([delegate respondsToSelector:@selector(eidosTextView:functionMapFromBaseMap:)])
		function_map = [delegate eidosTextView:self functionMapFromBaseMap:function_map];
	
	for (const auto& function_iter : *function_map)
	{
		const EidosFunctionSignature *sig = function_iter.second;
		NSString *functionName = [NSString stringWithUTF8String:sig->function_name_.c_str()];
		
		[globals addObject:[functionName stringByAppendingString:@"()"]];
	}
	
	// Finally, provide language keywords as an option if requested
	if (includeStatements)
	{
		[globals addObject:@"break"];
		[globals addObject:@"do"];
		[globals addObject:@"else"];
		[globals addObject:@"for"];
		[globals addObject:@"if"];
		[globals addObject:@"in"];
		[globals addObject:@"next"];
		[globals addObject:@"return"];
		[globals addObject:@"while"];
		
		// keywords from our Context, if any
		if ([delegate respondsToSelector:@selector(eidosTextViewLanguageKeywordsForCompletion:)])
		{
			NSArray *keywords = [delegate eidosTextViewLanguageKeywordsForCompletion:self];
			
			if (keywords)
				[globals addObjectsFromArray:keywords];
		}
	}
	
	return globals;
}

- (NSMutableArray *)completionsForKeyPathEndingInTokenIndex:(int)lastDotTokenIndex ofTokenStream:(const std::vector<EidosToken *> &)tokens
{
	EidosToken *token = tokens[lastDotTokenIndex];
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
	vector<string> identifiers;
	vector<bool> identifiers_are_calls;
	int bracketCount = 0, parenCount = 0;
	BOOL lastTokenWasDot = YES, justFinishedParenBlock = NO;
	
	for (int tokenIndex = lastDotTokenIndex - 1; tokenIndex >= 0; --tokenIndex)
	{
		token = tokens[tokenIndex];
		token_type = token->token_type_;
		
		// skip backward over whitespace and comments; they make no difference to us
		if ((token_type == EidosTokenType::kTokenWhitespace) || (token_type == EidosTokenType::kTokenComment))
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
	string &identifier_name = identifiers[key_path_index];
	EidosGlobalStringID identifier_ID = EidosGlobalStringIDForString(identifier_name);
	bool identifier_is_call = identifiers_are_calls[key_path_index];
	const EidosObjectClass *key_path_class = nullptr;
	
	if (identifier_is_call)
	{
		// The root identifier is a call, so it should be a function call; try to look it up
		id delegate = [self delegate];
		
		// Look in the delegate's list of functions first
		EidosFunctionMap *function_map = EidosInterpreter::BuiltInFunctionMap();
		
		if ([delegate respondsToSelector:@selector(eidosTextView:functionMapFromBaseMap:)])
			function_map = [delegate eidosTextView:self functionMapFromBaseMap:function_map];
		
		for (const auto& function_iter : *function_map)
		{
			const EidosFunctionSignature *sig = function_iter.second;
			
			if (sig->function_name_.compare(identifier_name) == 0)
			{
				key_path_class = sig->return_class_;
				break;
			}
		}
	}
	else
	{
		// The root identifier is not a call, so it should be a global symbol; try to look it up
		EidosSymbolTable *globalSymbolTable = gEidosConstantsSymbolTable;
		id delegate = [self delegate];
		
		if ([delegate respondsToSelector:@selector(eidosTextView:symbolsFromBaseSymbols:)])
			globalSymbolTable = [delegate eidosTextView:self symbolsFromBaseSymbols:globalSymbolTable];
		
		if (!globalSymbolTable->ContainsSymbol(identifier_ID))	// check first so we never get a raise
			return nil;
		
		EidosValue *key_path_root = globalSymbolTable->GetValueOrRaiseForSymbol(identifier_ID).get();
		
		if (!key_path_root)
			return nil;			// unknown symbol at the root, so we have no idea what's going on
		if (key_path_root->Type() != EidosValueType::kValueObject)
			return nil;			// the root symbol is not an object, so it should not have a key path off of it; bail
		
		key_path_class = ((EidosValue_Object *)key_path_root)->Class();
	}
	
	if (!key_path_class)
		return nil;				// unknown symbol at the root
	
	// Now we've got a class for the root of the key path; follow forward through the key path to arrive at the final type.
	while (--key_path_index >= 0)
	{
		identifier_name = identifiers[key_path_index];
		identifier_is_call = identifiers_are_calls[key_path_index];
		
		EidosGlobalStringID identifier_id = EidosGlobalStringIDForString(identifier_name);
		
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
		NSString *methodName = [NSString stringWithUTF8String:method_sig->function_name_.c_str()];
		
		[candidates addObject:[methodName stringByAppendingString:@"()"]];
	}
	
	return candidates;
}

- (NSArray *)completionsForTokenStream:(const std::vector<EidosToken *> &)tokens index:(int)lastTokenIndex canExtend:(BOOL)canExtend
{
	// What completions we offer depends on the token stream
	EidosToken *token = tokens[lastTokenIndex];
	EidosTokenType token_type = token->token_type_;
	
	switch (token_type)
	{
		case EidosTokenType::kTokenNone:
		case EidosTokenType::kTokenEOF:
		case EidosTokenType::kTokenWhitespace:
		case EidosTokenType::kTokenComment:
		case EidosTokenType::kTokenInterpreterBlock:
		case EidosTokenType::kTokenContextFile:
		case EidosTokenType::kTokenContextEidosBlock:
		case EidosTokenType::kFirstIdentifierLikeToken:
			// These should never be hit
			return nil;
			
		case EidosTokenType::kTokenIdentifier:
		case EidosTokenType::kTokenIf:
		case EidosTokenType::kTokenElse:
		case EidosTokenType::kTokenDo:
		case EidosTokenType::kTokenWhile:
		case EidosTokenType::kTokenFor:
		case EidosTokenType::kTokenIn:
		case EidosTokenType::kTokenNext:
		case EidosTokenType::kTokenBreak:
		case EidosTokenType::kTokenReturn:
			if (canExtend)
			{
				NSMutableArray *completions = nil;
				
				// This is the tricky case, because the identifier we're extending could be the end of a key path like foo.bar[5:8].ba...
				// We need to move backwards from the current token until we find or fail to find a dot token; if we see a dot we're in
				// a key path, otherwise we're in the global context and should filter from those candidates
				for (int previousTokenIndex = lastTokenIndex - 1; previousTokenIndex >= 0; --previousTokenIndex)
				{
					EidosToken *previous_token = tokens[previousTokenIndex];
					EidosTokenType previous_token_type = previous_token->token_type_;
					
					// if the token we're on is skippable, continue backwards
					if ((previous_token_type == EidosTokenType::kTokenWhitespace) || (previous_token_type == EidosTokenType::kTokenComment))
						continue;
					
					// if the token we're on is a dot, we are indeed at the end of a key path, and can fetch the completions for it
					if (previous_token_type == EidosTokenType::kTokenDot)
					{
						completions = [self completionsForKeyPathEndingInTokenIndex:previousTokenIndex ofTokenStream:tokens];
						break;
					}
					
					// if we see a semicolon or brace, we are in a completely global context
					if ((previous_token_type == EidosTokenType::kTokenSemicolon) || (previous_token_type == EidosTokenType::kTokenLBrace) || (previous_token_type == EidosTokenType::kTokenRBrace))
					{
						completions = [self globalCompletionsIncludingStatements:YES];
						break;
					}
					
					// if we see any other token, we are not in a key path; let's assume we're following an operator
					completions = [self globalCompletionsIncludingStatements:NO];
					break;
				}
				
				// If we ran out of tokens, we're at the beginning of the file and so in the global context
				if (!completions)
					completions = [self globalCompletionsIncludingStatements:YES];
				
				// Now we have an array of possible completions; we just need to remove those that don't start with our existing prefix
				NSString *baseString = [NSString stringWithUTF8String:token->token_string_.c_str()];
				
				for (int completionIndex = (int)[completions count] - 1; completionIndex >= 0; --completionIndex)
				{
					if (![[completions objectAtIndex:completionIndex] hasPrefix:baseString])
						[completions removeObjectAtIndex:completionIndex];
				}
				
				return completions;
			}
			
			// If the previous token was an identifier and we can't extend it, the next thing probably needs to be an operator or something
			return nil;
			
		case EidosTokenType::kTokenBad:
		case EidosTokenType::kTokenNumber:
		case EidosTokenType::kTokenString:
		case EidosTokenType::kTokenRParen:
		case EidosTokenType::kTokenRBracket:
			// We don't have anything to suggest after such tokens; the next thing will need to be an operator, semicolon, etc.
			return nil;
			
		case EidosTokenType::kTokenDot:
			// This is the other tricky case, because we're being asked to extend a key path like foo.bar[5:8].
			return [self completionsForKeyPathEndingInTokenIndex:lastTokenIndex ofTokenStream:tokens];
			
		case EidosTokenType::kTokenSemicolon:
		case EidosTokenType::kTokenLBrace:
		case EidosTokenType::kTokenRBrace:
			// We are in the global context and anything goes, including a new statement
			return [self globalCompletionsIncludingStatements:YES];
			
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
		case EidosTokenType::kTokenAssign:
		case EidosTokenType::kTokenEq:
		case EidosTokenType::kTokenLt:
		case EidosTokenType::kTokenLtEq:
		case EidosTokenType::kTokenGt:
		case EidosTokenType::kTokenGtEq:
		case EidosTokenType::kTokenNot:
		case EidosTokenType::kTokenNotEq:
			// We are following an operator, so globals are OK but new statements are not
			return [self globalCompletionsIncludingStatements:NO];
	}
	
	return nil;
}

- (NSUInteger)rangeOffsetForCompletionRange
{
	// This is for EidosConsoleTextView to be able to remove the prompt string from the string being completed
	return 0;
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
		EidosScript script(script_string);
		
		// Tokenize
		script.Tokenize(true, true);	// make bad tokens as needed, keep nonsignificant tokens
		
		auto tokens = script.Tokens();
		int lastTokenIndex = (int)tokens.size() - 1;
		BOOL endedCleanly = NO, lastTokenInterrupted = NO;
		
		// if we ended with an EOF, that means we did not have a raise and there should be no untokenizable range at the end
		if ((lastTokenIndex >= 0) && (tokens[lastTokenIndex]->token_type_ == EidosTokenType::kTokenEOF))
		{
			--lastTokenIndex;
			endedCleanly = YES;
		}
		
		// if we ended with whitespace or a comment, the previous token cannot be extended
		while (lastTokenIndex >= 0) {
			EidosToken *token = tokens[lastTokenIndex];
			
			if ((token->token_type_ != EidosTokenType::kTokenWhitespace) && (token->token_type_ != EidosTokenType::kTokenComment))
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
		else if (lastTokenInterrupted)
		{
			if (lastTokenIndex < 0)
			{
				// We're at the end of nothing but initial whitespace and comments; offer insertion-point completions
				if (baseRange) *baseRange = NSMakeRange(selection.location + rangeOffset, 0);
				if (completions) *completions = [self globalCompletionsIncludingStatements:YES];
				return;
			}
			
			EidosToken *token = tokens[lastTokenIndex];
			EidosTokenType token_type = token->token_type_;
			
			// the last token cannot be extended, so if the last token is something an identifier can follow, like an
			// operator, then we can offer completions at the insertion point based on that, otherwise punt.
			if ((token_type == EidosTokenType::kTokenNumber) || (token_type == EidosTokenType::kTokenString) || (token_type == EidosTokenType::kTokenRParen) || (token_type == EidosTokenType::kTokenRBracket) || (token_type == EidosTokenType::kTokenIdentifier) || (token_type == EidosTokenType::kTokenIf) || (token_type == EidosTokenType::kTokenWhile) || (token_type == EidosTokenType::kTokenFor) || (token_type == EidosTokenType::kTokenNext) || (token_type == EidosTokenType::kTokenBreak) || (token_type == EidosTokenType::kTokenReturn))
			{
				if (baseRange) *baseRange = NSMakeRange(NSNotFound, 0);
				if (completions) *completions = nil;
				return;
			}
			
			if (baseRange) *baseRange = NSMakeRange(selection.location + rangeOffset, 0);
			if (completions) *completions = [self completionsForTokenStream:tokens index:lastTokenIndex canExtend:NO];
			return;
		}
		else
		{
			if (lastTokenIndex < 0)
			{
				// We're at the very beginning of the script; offer insertion-point completions
				if (baseRange) *baseRange = NSMakeRange(selection.location + rangeOffset, 0);
				if (completions) *completions = [self globalCompletionsIncludingStatements:YES];
				return;
			}
			
			// the last token was not interrupted, so we can offer completions of it if we want to.
			EidosToken *token = tokens[lastTokenIndex];
			NSRange tokenRange = NSMakeRange(token->token_UTF16_start_, token->token_UTF16_end_ - token->token_UTF16_start_ + 1);
			
			if (token->token_type_ >= EidosTokenType::kTokenIdentifier)
			{
				if (baseRange) *baseRange = NSMakeRange(tokenRange.location + rangeOffset, tokenRange.length);
				if (completions) *completions = [self completionsForTokenStream:tokens index:lastTokenIndex canExtend:YES];
				return;
			}
			
			if ((token->token_type_ == EidosTokenType::kTokenNumber) || (token->token_type_ == EidosTokenType::kTokenString) || (token->token_type_ == EidosTokenType::kTokenRParen) || (token->token_type_ == EidosTokenType::kTokenRBracket))
			{
				if (baseRange) *baseRange = NSMakeRange(NSNotFound, 0);
				if (completions) *completions = nil;
				return;
			}
			
			if (baseRange) *baseRange = NSMakeRange(selection.location + rangeOffset, 0);
			if (completions) *completions = [self completionsForTokenStream:tokens index:lastTokenIndex canExtend:NO];
			return;
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
	[self edited:NSTextStorageEditedCharacters range:range changeInLength:0];
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




























































