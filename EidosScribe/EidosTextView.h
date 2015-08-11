//
//  EidosTextView.h
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


#import <Cocoa/Cocoa.h>

#include "eidos_interpreter.h"


// A subclass to provide various niceties for a syntax-colored, autoindenting, tab-stopped text view

class EidosCallSignature;

@interface EidosTextView : NSTextView
{
	int syntaxColorState_;	// 0 = off, 1 = Eidos, 2 = output; this should be a property, and the syntax coloring
							// should be managed by this class internally, I suppose...
}

+ (NSDictionary *)consoleTextAttributesWithColor:(NSColor *)textColor;	// Menlo 11 with 3-space tabs

- (IBAction)shiftSelectionLeft:(id)sender;
- (IBAction)shiftSelectionRight:(id)sender;
- (IBAction)commentUncommentSelection:(id)sender;

- (void)selectErrorRange;

- (void)syntaxColorForEidos;
- (void)syntaxColorForOutput;
- (void)clearSyntaxColoring;

- (NSAttributedString *)attributedSignatureForScriptString:(NSString *)scriptString selection:(NSRange)selection;
- (NSAttributedString *)attributedSignatureForCallName:(NSString *)callName isMethodCall:(BOOL)isMethodCall;
- (NSAttributedString *)attributedStringForSignature:(const EidosCallSignature *)signature;

@end


// A protocol of optional methods that the EidosTextView's delegate can implement
@protocol EidosTextViewDelegate <NSObject>
@optional
- (NSRange)textView:(NSTextView *)textView rangeForUserCompletion:(NSRange)suggestedRange;
- (EidosSymbolTable *)globalSymbolTableForCompletion;
- (NSArray *)languageKeywordsForCompletion;
- (const std::vector<const EidosFunctionSignature*> *)injectedFunctionSignatures;
- (const std::vector<const EidosMethodSignature*> *)allMethodSignatures;
- (bool)tokenStringIsSpecialIdentifier:(const std::string &)token_string;
@end


































































