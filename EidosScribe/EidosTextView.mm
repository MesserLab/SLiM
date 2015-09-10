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

#include "eidos_script.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"

#include <stdexcept>


using std::vector;
using std::string;


@implementation EidosTextView

// produce standard text attributes including our font (Menlo 11), tab stops (every three spaces), and font colors for syntax coloring
+ (NSDictionary *)consoleTextAttributesWithColor:(NSColor *)textColor
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
	[self setTypingAttributes:[EidosTextView consoleTextAttributesWithColor:nil]];
	
	// Fix text container insets to look a bit nicer; {0,0} by default
	[self setTextContainerInset:NSMakeSize(0.0, 5.0)];
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
		int scriptLength = (int)[scriptString length];
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
		int scriptLength = (int)[scriptString length];
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
		int scriptLength = (int)[scriptString length];
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
					[ts setAttributes:[EidosTextView consoleTextAttributesWithColor:[NSColor blackColor]] range:NSMakeRange(scanPosition, 2)];
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
	if (!gEidosExecutingRuntimeScript && (gEidosCharacterStartOfError >= 0) && (gEidosCharacterEndOfError >= gEidosCharacterStartOfError))
	{
		NSRange charRange = NSMakeRange(gEidosCharacterStartOfError, gEidosCharacterEndOfError - gEidosCharacterStartOfError + 1);
		
		[self setSelectedRange:charRange];
		[self scrollRangeToVisible:charRange];
		
		// Set the selection color to red for maximal visibility; this gets set back in setSelectedRanges:affinity:stillSelecting:
		[self setSelectedTextAttributes:@{NSBackgroundColorAttributeName:[NSColor redColor], NSForegroundColorAttributeName:[NSColor whiteColor]}];
	}
	
	// In any case, since we are the ultimate consumer of the error information, we should clear out
	// the error state to avoid misattribution of future errors
	gEidosCharacterStartOfError = -1;
	gEidosCharacterEndOfError = -1;
	gEidosCurrentScript = nullptr;
	gEidosExecutingRuntimeScript = false;
}

- (void)setSelectedRanges:(NSArray *)ranges affinity:(NSSelectionAffinity)affinity stillSelecting:(BOOL)stillSelectingFlag
{
	// The text selection color may have been changed by -selectErrorRange; here we just make sure it is set back again
	[self setSelectedTextAttributes:@{NSBackgroundColorAttributeName:[NSColor selectedTextBackgroundColor]}];
	
	[super setSelectedRanges:ranges affinity:affinity stillSelecting:stillSelectingFlag];
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
		NSRange tokenRange = NSMakeRange(token->token_start_, token->token_end_ - token->token_start_ + 1);
		
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

- (void)didChangeText
{
	if (_shouldRecolorAfterChanges)
		[self recolorAfterChanges];
	
	[super didChangeText];
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
			if (tokens[tokenIndex]->token_start_ >= selectionStart)
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
	// We need to figure out what the signature is for the call name.  We check injected function names first,
	// standard function names second, and then we check the global method registry to see if we've got a
	// match there.  That is thus the priority, in case of duplicate names.
	std::string call_name([callName UTF8String]);
	id delegate = [self delegate];
	
	if (!isMethodCall)
	{
		// Look for a matching injected function signature first
		const std::vector<const EidosFunctionSignature *> *injectedSignatures = nullptr;
		
		if ([delegate respondsToSelector:@selector(eidosTextViewInjectedFunctionSignatures:)])
			injectedSignatures = [delegate eidosTextViewInjectedFunctionSignatures:self];
		
		if (injectedSignatures)
		{
			for (const EidosFunctionSignature *sig : *injectedSignatures)
			{
				const std::string &sig_call_name = sig->function_name_;
				
				if (sig_call_name.compare(call_name) == 0)
					return [self attributedStringForSignature:sig];
			}
		}
		
		// Look for a matching built-in function second
		const vector<const EidosFunctionSignature *> &builtinSignatures = EidosInterpreter::BuiltInFunctions();
		
		for (const EidosFunctionSignature *sig : builtinSignatures)
		{
			const std::string &sig_call_name = sig->function_name_;
			
			if (sig_call_name.compare(call_name) == 0)
				return [self attributedStringForSignature:sig];
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
				return [self attributedStringForSignature:sig];
		}
	}
	
	// Assemble an attributed string for our failed lookup message
	NSMutableAttributedString *attrStr = [[[NSMutableAttributedString alloc] init] autorelease];
	NSDictionary *plainAttrs = [EidosConsoleTextView outputAttrs];
	NSDictionary *functionAttrs = [EidosConsoleTextView parseAttrs];
	
	[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:callName attributes:functionAttrs] autorelease]];
	[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@"() – unrecognized call" attributes:plainAttrs] autorelease]];
	[attrStr addAttribute:NSBaselineOffsetAttributeName value:[NSNumber numberWithFloat:2.0] range:NSMakeRange(0, [attrStr length])];
	
	return attrStr;
}

- (NSAttributedString *)attributedStringForSignature:(const EidosCallSignature *)signature
{
	if (signature)
	{
		//
		//	Note this logic is paralleled in the function operator<<(ostream &, const EidosCallSignature &).
		//	These two should be kept in synch so the user-visible format of signatures is consistent.
		//
		
		// Build an attributed string showing the call signature with syntax coloring for its parts
		NSMutableAttributedString *attrStr = [[[NSMutableAttributedString alloc] init] autorelease];
		
		NSString *prefixString = [NSString stringWithUTF8String:signature->CallPrefix().c_str()];	// "", "- ", or "+ "
		NSString *returnTypeString = [NSString stringWithUTF8String:StringForEidosValueMask(signature->return_mask_, signature->return_class_, "").c_str()];
		NSString *functionNameString = [NSString stringWithUTF8String:signature->function_name_.c_str()];
		
		NSDictionary *plainAttrs = [EidosConsoleTextView outputAttrs];
		NSDictionary *typeAttrs = [EidosConsoleTextView inputAttrs];
		NSDictionary *functionAttrs = [EidosConsoleTextView parseAttrs];
		NSDictionary *paramAttrs = [EidosConsoleTextView promptAttrs];
		
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
				const string &arg_name = signature->arg_names_[arg_index];
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
					
					[attrStr appendAttributedString:[[[NSAttributedString alloc] initWithString:@" " attributes:plainAttrs] autorelease]];
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
	EidosSymbolTable *globalSymbolTable = nullptr;
	
	if ([delegate respondsToSelector:@selector(eidosTextViewGlobalSymbolTableForCompletion:)])
		globalSymbolTable = [delegate eidosTextViewGlobalSymbolTableForCompletion:self];
	
	if (globalSymbolTable)
	{
		for (std::string &symbol_name : globalSymbolTable->ReadOnlySymbols())
			[globals addObject:[NSString stringWithUTF8String:symbol_name.c_str()]];
		
		for (std::string &symbol_name : globalSymbolTable->ReadWriteSymbols())
			[globals addObject:[NSString stringWithUTF8String:symbol_name.c_str()]];
	}
	
	[globals sortUsingSelector:@selector(compare:)];
	
	// Next, a sorted list of injected functions, with () appended
	const std::vector<const EidosFunctionSignature*> *signatures = nullptr;
	
	if ([delegate respondsToSelector:@selector(eidosTextViewInjectedFunctionSignatures:)])
		signatures = [delegate eidosTextViewInjectedFunctionSignatures:self];
	
	if (signatures)
	{
		for (const EidosFunctionSignature *sig : *signatures)
		{
			NSString *functionName = [NSString stringWithUTF8String:sig->function_name_.c_str()];
			
			[globals addObject:[functionName stringByAppendingString:@"()"]];
		}
	}
	
	// Next, a sorted list of functions, with () appended
	for (const EidosFunctionSignature *sig : EidosInterpreter::BuiltInFunctions())
	{
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
			identifiers.push_back(token->token_string_);
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
	bool identifier_is_call = identifiers_are_calls[key_path_index];
	const EidosObjectClass *key_path_class = nullptr;
	
	if (identifier_is_call)
	{
		// The root identifier is a call, so it should be a function call; try to look it up
		id delegate = [self delegate];
		
		// Look in the delegate's list of functions first
		const std::vector<const EidosFunctionSignature*> *signatures = nullptr;
		
		if ([delegate respondsToSelector:@selector(eidosTextViewInjectedFunctionSignatures:)])
			signatures = [delegate eidosTextViewInjectedFunctionSignatures:self];
		
		if (signatures)
		{
			for (const EidosFunctionSignature *sig : *signatures)
				if (sig->function_name_.compare(identifier_name) == 0)
				{
					key_path_class = sig->return_class_;
					break;
				}
		}
		
		// Next, a sorted list of functions, with () appended
		if (!key_path_class)
		{
			for (const EidosFunctionSignature *sig : EidosInterpreter::BuiltInFunctions())
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
		EidosSymbolTable *globalSymbolTable = nullptr;
		id delegate = [self delegate];
		
		if ([delegate respondsToSelector:@selector(eidosTextViewGlobalSymbolTableForCompletion:)])
			globalSymbolTable = [delegate eidosTextViewGlobalSymbolTableForCompletion:self];
		
		EidosValue *key_path_root = (globalSymbolTable ? globalSymbolTable->GetValueOrNullForSymbol(identifier_name) : nullptr);
		
		if (!key_path_root)
			return nil;			// unknown symbol at the root, so we have no idea what's going on
		if (key_path_root->Type() != EidosValueType::kValueObject)
		{
			if (key_path_root->IsTemporary()) delete key_path_root;
			return nil;			// the root symbol is not an object, so it should not have a key path off of it; bail
		}
		
		key_path_class = ((EidosValue_Object *)key_path_root)->Class();
		
		if (key_path_root->IsTemporary()) delete key_path_root;
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
			NSRange tokenRange = NSMakeRange(token->token_start_, token->token_end_ - token->token_start_ + 1);
			
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





























































