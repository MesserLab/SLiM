//
//  SLiMScriptTextView.mm
//  SLiM
//
//  Created by Ben Haller on 6/14/15.
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


#import "SLiMScriptTextView.h"

#include "script.h"
#include "slim_global.h"
#include "script_functionsignature.h"

#include <stdexcept>


using std::vector;
using std::string;


@implementation SLiMScriptTextView

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
		return [NSDictionary dictionaryWithObjectsAndKeys:textColor, NSForegroundColorAttributeName, menlo11Font, NSFontAttributeName, paragraphStyle, NSParagraphStyleAttributeName, nil];
	else
		return [NSDictionary dictionaryWithObjectsAndKeys:menlo11Font, NSFontAttributeName, paragraphStyle, NSParagraphStyleAttributeName, nil];
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
	
	[super insertNewline:sender];
	[self insertText:whitespaceString];
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
	[pasteboard writeObjects:[NSArray arrayWithObject:attrStringInRange]];
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
				[ts replaceCharactersInRange:NSMakeRange(scanPosition, 1) withString:@""];
				[scriptString replaceCharactersInRange:NSMakeRange(scanPosition, 1) withString:@""];
				scriptLength--;
				
				if (scanPosition < selectedRange.location)
					selectedRange.location--;
				else
					selectedRange.length--;
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
		
		[ts endEditing];
		[self setSelectedRange:selectedRange];
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
		
		// ok, we're at the start of the line that the selection starts on; start inserting tabs
		[ts beginEditing];
		
		while ((scanPosition == selectedRange.location) || (scanPosition < selectedRange.location + selectedRange.length))
		{
			// insert a tab at the start of this line and adjust our selection
			[ts replaceCharactersInRange:NSMakeRange(scanPosition, 0) withString:@"\t"];
			[scriptString replaceCharactersInRange:NSMakeRange(scanPosition, 0) withString:@"\t"];
			scriptLength++;
			
			if ((scanPosition < selectedRange.location) || (selectedRange.length == 0))
				selectedRange.location++;
			else
				selectedRange.length++;
			
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
		
		[ts endEditing];
		[self setSelectedRange:selectedRange];
	}
	else
	{
		NSBeep();
	}
}

// Check whether a token string is a special identifier like "pX", "gX", or "mX"
- (BOOL)tokenStringIsSpecialIdentifier:(const std::string &)token_string
{
	int len = (int)token_string.length();
	
	if (len >= 2)
	{
		unichar first_ch = token_string[0];
		
		if ((first_ch == 'p') || (first_ch == 'g') || (first_ch == 'm'))
		{
			for (int ch_index = 1; ch_index < len; ++ch_index)
			{
				unichar idx_ch = token_string[ch_index];
				
				if ((idx_ch < '0') || (idx_ch > '9'))
					return NO;
			}
			
			return YES;
		}
	}
	
	return NO;
}

- (void)syntaxColorForSLiMScript
{
	// Construct a Script object from the current script string
	NSString *scriptString = [self string];
	std::string script_string([scriptString UTF8String]);
	Script script(script_string, 0);
	
	// Tokenize
	try
	{
		script.Tokenize(true);	// keep nonsignificant tokens - whitespace and comments
	}
	catch (std::runtime_error err)
	{
		// if we get a raise, we just use as many tokens as we got; clear the error string buffer
		GetUntrimmedRaiseMessage();
		
		//NSString *errorString = [NSString stringWithUTF8String:GetUntrimmedRaiseMessage().c_str()];
		//NSLog(@"raise during syntax coloring tokenization: %@", errorString);
	}
	
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
	
	for (ScriptToken *token : script.Tokens())
	{
		NSRange tokenRange = NSMakeRange(token->token_start_, token->token_end_ - token->token_start_ + 1);
		
		if (token->token_type_ == TokenType::kTokenNumber)
			[ts addAttribute:NSForegroundColorAttributeName value:numberLiteralColor range:tokenRange];
		if (token->token_type_ == TokenType::kTokenString)
			[ts addAttribute:NSForegroundColorAttributeName value:stringLiteralColor range:tokenRange];
		if (token->token_type_ == TokenType::kTokenComment)
			[ts addAttribute:NSForegroundColorAttributeName value:commentColor range:tokenRange];
		if (token->token_type_ > TokenType::kFirstIdentifierLikeToken)
			[ts addAttribute:NSForegroundColorAttributeName value:keywordColor range:tokenRange];
		if (token->token_type_ == TokenType::kTokenIdentifier)
		{
			// most identifiers are left as black; only special ones get colored
			const std::string &token_string = token->token_string_;
			
			if ((token_string.compare("T") == 0) ||
				(token_string.compare("F") == 0) ||
				(token_string.compare("E") == 0) ||
				(token_string.compare("PI") == 0) ||
				(token_string.compare("INF") == 0) ||
				(token_string.compare("NAN") == 0) ||
				(token_string.compare("NULL") == 0) ||
				(token_string.compare("sim") == 0) ||
				[self tokenStringIsSpecialIdentifier:token_string])
				[ts addAttribute:NSForegroundColorAttributeName value:identifierColor range:tokenRange];
		}
	}
	
	[ts endEditing];
}

- (void)syntaxColorForSLiMInput
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
		poundDirectiveAttrs = [[NSDictionary alloc] initWithObjectsAndKeys:[NSColor colorWithCalibratedRed:196/255.0 green:26/255.0 blue:22/255.0 alpha:1.0], NSForegroundColorAttributeName, nil];
		commentAttrs = [[NSDictionary alloc] initWithObjectsAndKeys:[NSColor colorWithCalibratedRed:0/255.0 green:116/255.0 blue:0/255.0 alpha:1.0], NSForegroundColorAttributeName, nil];
		subpopAttrs = [[NSDictionary alloc] initWithObjectsAndKeys:[NSColor colorWithCalibratedRed:28/255.0 green:0/255.0 blue:207/255.0 alpha:1.0], NSForegroundColorAttributeName, nil];
		genomicElementAttrs = [[NSDictionary alloc] initWithObjectsAndKeys:[NSColor colorWithCalibratedRed:63/255.0 green:110/255.0 blue:116/255.0 alpha:1.0], NSForegroundColorAttributeName, nil];
		mutationTypeAttrs = [[NSDictionary alloc] initWithObjectsAndKeys:[NSColor colorWithCalibratedRed:170/255.0 green:13/255.0 blue:145/255.0 alpha:1.0], NSForegroundColorAttributeName, nil];
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

- (void)selectErrorRange
{
	if ((gCharacterStartOfParseError >= 0) && (gCharacterEndOfParseError >= gCharacterStartOfParseError))
	{
		NSRange charRange = NSMakeRange(gCharacterStartOfParseError, gCharacterEndOfParseError - gCharacterStartOfParseError + 1);
		
		[self setSelectedRange:charRange];
		[self scrollRangeToVisible:charRange];
	}
}

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
	SymbolTable *globalSymbolTable = nullptr;
	id delegate = [self delegate];
	
	if ([delegate respondsToSelector:@selector(globalSymbolsForCompletion)])
		globalSymbolTable = [delegate globalSymbolsForCompletion];
	
	// First, a sorted list of globals
	if (globalSymbolTable)
	{
		for (std::string symbol_name : globalSymbolTable->ReadOnlySymbols())
			[globals addObject:[NSString stringWithUTF8String:symbol_name.c_str()]];
		
		for (std::string symbol_name : globalSymbolTable->ReadWriteSymbols())
			[globals addObject:[NSString stringWithUTF8String:symbol_name.c_str()]];
	}
	
	[globals sortUsingSelector:@selector(compare:)];
	
	// Next, a sorted list of injected functions, with () appended
	if (delegate)
	{
		std::vector<FunctionSignature*> *signatures = nullptr;
		
		if ([delegate respondsToSelector:@selector(injectedFunctionSignatures)])
			signatures = [delegate injectedFunctionSignatures];
		
		if (signatures)
		{
			for (const FunctionSignature *sig : *signatures)
			{
				NSString *functionName = [NSString stringWithUTF8String:sig->function_name_.c_str()];
				
				[globals addObject:[functionName stringByAppendingString:@"()"]];
			}
		}
	}
	
	// Next, a sorted list of functions, with () appended
	for (const FunctionSignature *sig : ScriptInterpreter::BuiltInFunctions())
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
		
		// SLiM keywords
		[globals addObject:@"fitness"];
		[globals addObject:@"mateChoice"];
		[globals addObject:@"modifyChild"];
	}
	
	return globals;
}

- (NSMutableArray *)completionsForKeyPathEndingInTokenIndex:(int)lastDotTokenIndex ofTokenStream:(const std::vector<ScriptToken *> &)tokens
{
	ScriptToken *token = tokens[lastDotTokenIndex];
	TokenType token_type = token->token_type_;
	
	if (token_type != TokenType::kTokenDot)
	{
		NSLog(@"***** completionsForKeyPathEndingInTokenIndex... called for non-kTokenDot token!");
		return nil;
	}
	
	// OK, we've got a key path ending in a dot, and we want to return a list of completions that would work for that key path.
	// We'll trace backward, adding identifiers to a vector to build up the chain of references.  If we hit a bracket, we'll
	// skip back over everything inside it, since subsetting does not change the type; we just need to balance brackets.  If we
	// hit a parenthesis, we give up.  If we hit other things – a semicolon, a comma, a brace – that terminates the key path chain.
	vector<string> identifiers;
	int bracketCount = 0;
	BOOL lastTokenWasDot = YES;
	
	for (int tokenIndex = lastDotTokenIndex - 1; tokenIndex >= 0; --tokenIndex)
	{
		token = tokens[tokenIndex];
		token_type = token->token_type_;
		
		// skip backward over whitespace and comments; they make no difference to us
		if ((token_type == TokenType::kTokenWhitespace) || (token_type == TokenType::kTokenComment))
			continue;
		
		if (bracketCount)
		{
			// If we're inside a bracketed stretch, all we do is balance brackets and run backward.  We don't even clear lastTokenWasDot,
			// because a []. sequence puts us in the same situation as having just seen a dot – we're still waiting for an identifier.
			if (token_type == TokenType::kTokenRBracket)
			{
				bracketCount++;
				continue;
			}
			if (token_type == TokenType::kTokenLBracket)
			{
				bracketCount--;
				continue;
			}
			
			// Check for tokens that simply make no sense, and bail
			if ((token_type == TokenType::kTokenLBrace) || (token_type == TokenType::kTokenRBrace) || (token_type == TokenType::kTokenSemicolon) || (token_type >= TokenType::kFirstIdentifierLikeToken))
				return nil;
			
			continue;
		}
		
		if (!lastTokenWasDot)
		{
			// We just saw an identifier, so the only thing that can continue the key path is a dot
			if (token_type == TokenType::kTokenDot)
			{
				lastTokenWasDot = YES;
				continue;
			}
			
			// the key path has terminated at some non-key-path token, so we're done tracing it
			break;
		}
		
		// OK, the last token was a dot (or a subset preceding a dot).  We're looking for an identifier, but we're willing
		// to get distracted by a subset sequence, since that does not change the type.  Anything else does not make sense.
		// (A method call or function call is possible, actually, but we're not presently equipped to handle them.  The problem
		// is that we don't want to actually call the method/function to get a ScriptValue*, because such calls are heavyweight
		// and can have side effects, but without calling the method/function we have no way to get an instance of the type
		// that it would return.  We need the concept of Class objects, but C++ does not do that.  I miss Objective-C.  I'm not
		// sure how to solve this, really; it would require us to have some kind of artificial Class-object-like thing that
		// would know the properties and methods for a given ScriptObjectElement class.  Big distortion to the architecture.
		// So for now, we just don't trace back through method/function calls, which sucks.  FIXME)
		if (token_type == TokenType::kTokenIdentifier)
		{
			lastTokenWasDot = NO;
			identifiers.push_back(token->token_string_);
			continue;
		}
		else if (token_type == TokenType::kTokenRBracket)
		{
			bracketCount++;
			continue;
		}
		
		// This makes no sense, so bail
		return nil;
	}
	
	// If we were in the middle of tracing the key path when the loop ended, then something is wrong, bail.
	if (lastTokenWasDot || bracketCount)
		return nil;
	
	// OK, we've got an identifier chain in identifiers, in reverse order.  We want to start at the beginning of the key path,
	// and follow it forward through the properties in the chain to arrive at the final type.
	int key_path_index = (int)identifiers.size() - 1;
	string identifier_name = identifiers[key_path_index];
	
	SymbolTable *globalSymbolTable = nullptr;
	id delegate = [self delegate];
	
	if ([delegate respondsToSelector:@selector(globalSymbolsForCompletion)])
		globalSymbolTable = [delegate globalSymbolsForCompletion];
	
	ScriptValue *key_path_value = (globalSymbolTable ? globalSymbolTable->GetValueOrNullForSymbol(identifier_name) : nullptr);
	
	if (!key_path_value)
		return nil;			// unknown symbol at the root, so we have no idea what's going on
	if (key_path_value->Type() != ScriptValueType::kValueObject)
	{
		if (!key_path_value->InSymbolTable()) delete key_path_value;
		return nil;			// the root symbol is not an object, so it should not have a key path off of it; bail
	}
	
	while (--key_path_index >= 0)
	{
		identifier_name = identifiers[key_path_index];
		
		ScriptValue *property_value = ((ScriptValue_Object *)key_path_value)->GetRepresentativeValueOrNullForMemberOfElements(identifier_name);
		
		if (!key_path_value->InSymbolTable()) delete key_path_value;
		key_path_value = property_value;
		
		if (!key_path_value)
			return nil;			// unknown symbol at the root, so we have no idea what's going on
		if (key_path_value->Type() != ScriptValueType::kValueObject)
		{
			if (!key_path_value->InSymbolTable()) delete key_path_value;
			return nil;			// the root symbol is not an object, so it should not have a key path off of it; bail
		}
	}
	
	// OK, we've now got a ScriptValue object that represents the end of the line; the final dot is off of this object.
	// So we want to extract all of its properties and methods, and return them all as candidates.
	NSMutableArray *candidates = [NSMutableArray array];
	ScriptValue_Object *terminus = ((ScriptValue_Object *)key_path_value);
	
	// First, a sorted list of globals
	for (std::string symbol_name : terminus->ReadOnlyMembersOfElements())
		[candidates addObject:[NSString stringWithUTF8String:symbol_name.c_str()]];
	
	for (std::string symbol_name : terminus->ReadWriteMembersOfElements())
		[candidates addObject:[NSString stringWithUTF8String:symbol_name.c_str()]];
	
	[candidates sortUsingSelector:@selector(compare:)];
	
	// Next, a sorted list of functions, with () appended
	for (string method_name : terminus->MethodsOfElements())
	{
		NSString *methodName = [NSString stringWithUTF8String:method_name.c_str()];
		
		[candidates addObject:[methodName stringByAppendingString:@"()"]];
	}
	
	// Dispose of our terminus
	if (!terminus->InSymbolTable()) delete terminus;
	
	return candidates;
}

- (NSArray *)completionsForTokenStream:(const std::vector<ScriptToken *> &)tokens index:(int)lastTokenIndex canExtend:(BOOL)canExtend
{
	// What completions we offer depends on the token stream
	ScriptToken *token = tokens[lastTokenIndex];
	TokenType token_type = token->token_type_;
	
	switch (token_type)
	{
		case TokenType::kTokenNone:
		case TokenType::kTokenEOF:
		case TokenType::kTokenWhitespace:
		case TokenType::kTokenComment:
		case TokenType::kTokenInterpreterBlock:
		case TokenType::kTokenSLiMFile:
		case TokenType::kTokenSLiMScriptBlock:
		case TokenType::kFirstIdentifierLikeToken:
			// These should never be hit
			return nil;
			
		case TokenType::kTokenIdentifier:
		case TokenType::kTokenIf:
		case TokenType::kTokenElse:
		case TokenType::kTokenDo:
		case TokenType::kTokenWhile:
		case TokenType::kTokenFor:
		case TokenType::kTokenIn:
		case TokenType::kTokenNext:
		case TokenType::kTokenBreak:
		case TokenType::kTokenReturn:
		case TokenType::kTokenFitness:
		case TokenType::kTokenMateChoice:
		case TokenType::kTokenModifyChild:
			if (canExtend)
			{
				NSMutableArray *completions = nil;
				
				// This is the tricky case, because the identifier we're extending could be the end of a key path like foo.bar[5:8].ba...
				// We need to move backwards from the current token until we find or fail to find a dot token; if we see a dot we're in
				// a key path, otherwise we're in the global context and should filter from those candidates
				for (int previousTokenIndex = lastTokenIndex - 1; previousTokenIndex >= 0; --previousTokenIndex)
				{
					ScriptToken *previous_token = tokens[previousTokenIndex];
					TokenType previous_token_type = previous_token->token_type_;
					
					// if the token we're on is skippable, continue backwards
					if ((previous_token_type == TokenType::kTokenWhitespace) || (previous_token_type == TokenType::kTokenComment))
						continue;
					
					// if the token we're on is a dot, we are indeed at the end of a key path, and can fetch the completions for it
					if (previous_token_type == TokenType::kTokenDot)
					{
						completions = [self completionsForKeyPathEndingInTokenIndex:previousTokenIndex ofTokenStream:tokens];
						break;
					}
					
					// if we see a semicolon or brace, we are in a completely global context
					if ((previous_token_type == TokenType::kTokenSemicolon) || (previous_token_type == TokenType::kTokenLBrace) || (previous_token_type == TokenType::kTokenRBrace))
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
			
		case TokenType::kTokenNumber:
		case TokenType::kTokenString:
		case TokenType::kTokenRParen:
		case TokenType::kTokenRBracket:
			// We don't have anything to suggest after such tokens; the next thing will need to be an operator, semicolon, etc.
			return nil;
			
		case TokenType::kTokenDot:
			// This is the other tricky case, because we're being asked to extend a key path like foo.bar[5:8].
			return [self completionsForKeyPathEndingInTokenIndex:lastTokenIndex ofTokenStream:tokens];
			
		case TokenType::kTokenSemicolon:
		case TokenType::kTokenLBrace:
		case TokenType::kTokenRBrace:
			// We are in the global context and anything goes, including a new statement
			return [self globalCompletionsIncludingStatements:YES];
			
		case TokenType::kTokenColon:
		case TokenType::kTokenComma:
		case TokenType::kTokenLParen:
		case TokenType::kTokenLBracket:
		case TokenType::kTokenPlus:
		case TokenType::kTokenMinus:
		case TokenType::kTokenMod:
		case TokenType::kTokenMult:
		case TokenType::kTokenExp:
		case TokenType::kTokenAnd:
		case TokenType::kTokenOr:
		case TokenType::kTokenDiv:
		case TokenType::kTokenAssign:
		case TokenType::kTokenEq:
		case TokenType::kTokenLt:
		case TokenType::kTokenLtEq:
		case TokenType::kTokenGt:
		case TokenType::kTokenGtEq:
		case TokenType::kTokenNot:
		case TokenType::kTokenNotEq:
			// We are following an operator, so globals are OK but new statements are not
			return [self globalCompletionsIncludingStatements:NO];
	}
	
	return nil;
}

- (NSUInteger)rangeOffsetForCompletionRange
{
	// This is for ConsoleTextView to be able to remove the prompt string from the string being completed
	return 0;
}

// one funnel for all completion work, since we use the same pattern to answer both questions...
- (void)_completionHandlerWithRangeForCompletion:(NSRange *)baseRange completions:(NSArray **)completions
{
	NSString *scriptString = [self string];
	NSRange selection = [self selectedRange];	// ignore charRange and work from the selection
	NSUInteger rangeOffset = [self rangeOffsetForCompletionRange];
	
	// correct the script string to have only what is entered after the prompt, if we are a ConsoleTextView
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
		Script script(script_string, 0);
		
		// Tokenize
		try
		{
			script.Tokenize(true);	// keep nonsignificant tokens - whitespace and comments
		}
		catch (std::runtime_error err)
		{
			// if we get a raise, we just use as many tokens as we got; clear the error string buffer
			GetUntrimmedRaiseMessage();
		}
		
		auto tokens = script.Tokens();
		int lastTokenIndex = (int)tokens.size() - 1;
		BOOL endedCleanly = NO, lastTokenInterrupted = NO;
		
		// if we ended with an EOF, that means we did not have a raise and there should be no untokenizable range at the end
		if ((lastTokenIndex >= 0) && (tokens[lastTokenIndex]->token_type_ == TokenType::kTokenEOF))
		{
			--lastTokenIndex;
			endedCleanly = YES;
		}
		
		// if we ended with whitespace or a comment, the previous token cannot be extended
		while (lastTokenIndex >= 0) {
			ScriptToken *token = tokens[lastTokenIndex];
			
			if ((token->token_type_ != TokenType::kTokenWhitespace) && (token->token_type_ != TokenType::kTokenComment))
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
			
			ScriptToken *token = tokens[lastTokenIndex];
			TokenType token_type = token->token_type_;
			
			// the last token cannot be extended, so if the last token is something an identifier can follow, like an
			// operator, then we can offer completions at the insertion point based on that, otherwise punt.
			if ((token_type == TokenType::kTokenNumber) || (token_type == TokenType::kTokenString) || (token_type == TokenType::kTokenRParen) || (token_type == TokenType::kTokenRBracket) || (token_type == TokenType::kTokenIdentifier) || (token_type == TokenType::kTokenIf) || (token_type == TokenType::kTokenWhile) || (token_type == TokenType::kTokenFor) || (token_type == TokenType::kTokenNext) || (token_type == TokenType::kTokenBreak) || (token_type == TokenType::kTokenReturn) || (token_type == TokenType::kTokenFitness) || (token_type == TokenType::kTokenMateChoice) || (token_type == TokenType::kTokenModifyChild))
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
			ScriptToken *token = tokens[lastTokenIndex];
			NSRange tokenRange = NSMakeRange(token->token_start_, token->token_end_ - token->token_start_ + 1);
			
			if (token->token_type_ >= TokenType::kTokenIdentifier)
			{
				if (baseRange) *baseRange = NSMakeRange(tokenRange.location + rangeOffset, tokenRange.length);
				if (completions) *completions = [self completionsForTokenStream:tokens index:lastTokenIndex canExtend:YES];
				return;
			}
			
			if ((token->token_type_ == TokenType::kTokenNumber) || (token->token_type_ == TokenType::kTokenString) || (token->token_type_ == TokenType::kTokenRParen) || (token->token_type_ == TokenType::kTokenRBracket))
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





























































