//
//  SLiMScriptTextView.h
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


#import <Cocoa/Cocoa.h>

#include "script_interpreter.h"


// A subclass to provide various niceties for a syntax-colored, autoindenting, tab-stopped text view

@interface SLiMScriptTextView : NSTextView
{
}

+ (NSDictionary *)consoleTextAttributesWithColor:(NSColor *)textColor;	// Menlo 11 with 4-space tabs
- (IBAction)shiftSelectionLeft:(id)sender;
- (IBAction)shiftSelectionRight:(id)sender;
- (void)syntaxColorForSLiMScript;
- (void)syntaxColorForSLiMInput;
- (void)clearSyntaxColoring;
- (void)selectErrorRange;

@end


// A protocol of optional methods that the SLiMScriptTextView's delegate can implement
@protocol SLiMScriptTextViewDelegate <NSObject>
@optional
- (NSRange)textView:(NSTextView *)textView rangeForUserCompletion:(NSRange)suggestedRange;
- (SymbolTable *)globalSymbolsForCompletion;
- (std::vector<FunctionSignature*> *)injectedFunctionSignatures;
@end


































































