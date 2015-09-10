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


/*
 
 A subclass to provide various niceties for a syntax-colored, autoindenting, tab-stopped text view.
 This class uses the standard NSTextView delegate object, but will use the optional methods defined
 by the EidosTextViewDelegate protocol if implemented by that delegate.

 */

class EidosCallSignature;

typedef enum EidosSyntaxColoringOption
{
	kEidosSyntaxColoringNone = 0,
	kEidosSyntaxColoringEidos,
	kEidosSyntaxColoringOutput
} EidosSyntaxColoringOption;


@interface EidosTextView : NSTextView
{
}

// The syntax coloring option being used
@property (nonatomic) EidosSyntaxColoringOption syntaxColoring;

// A flag to temporarily disable syntax coloring, used to coalesce multiple changes into a single recolor
@property (nonatomic) BOOL shouldRecolorAfterChanges;

// The standard font (Menlo 11 with 3-space tabs) with a given color, used to assemble attributed strings
+ (NSDictionary *)consoleTextAttributesWithColor:(NSColor *)textColor;

// Same designated initializers as NSTextView
- (instancetype)initWithFrame:(NSRect)frameRect textContainer:(NSTextContainer *)container NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;

// Actions associated with code editing
- (IBAction)shiftSelectionLeft:(id)sender;
- (IBAction)shiftSelectionRight:(id)sender;
- (IBAction)commentUncommentSelection:(id)sender;

// If an error occurs while tokenizing/parsing/executing Eidos code in this textview, call this to highlight the error
- (void)selectErrorRange;

// Called after disabling syntax coloring with shouldRecolorAfterChanges, to provide the coalesced recoloring
- (void)recolorAfterChanges;

// These methods are used to construct the function/method prototypes shown in the status bar; client code
// probably will not call them, but possibly they could be used for context-sensitive help or something
- (NSAttributedString *)attributedSignatureForScriptString:(NSString *)scriptString selection:(NSRange)selection;
- (NSAttributedString *)attributedSignatureForCallName:(NSString *)callName isMethodCall:(BOOL)isMethodCall;
- (NSAttributedString *)attributedStringForSignature:(const EidosCallSignature *)signature;

@end




































































