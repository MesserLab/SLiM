//
//  EidosTextView.h
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


#import <Cocoa/Cocoa.h>


// BCH 4/7/2016: So we can build against the OS X 10.9 SDK
#ifndef NS_DESIGNATED_INITIALIZER
#define NS_DESIGNATED_INITIALIZER
#endif


@protocol EidosTextViewDelegate;


/*
 
 A subclass to provide various niceties for a syntax-colored, autoindenting, tab-stopped text view.
 This class uses the standard NSTextView delegate object, but will use the optional methods defined
 by the EidosTextViewDelegate protocol if implemented by that delegate.

 */

typedef enum EidosSyntaxColoringOption
{
	kEidosSyntaxColoringNone = 0,
	kEidosSyntaxColoringEidos,
	kEidosSyntaxColoringOutput
} EidosSyntaxColoringOption;


@interface EidosTextView : NSTextView
{
}

// A delegate for Eidos functionality; this is the same object as the NSText/NSTextView delegate, and is not declared explicitly
// here because overriding properties with a different type doesn't really work.  So when you call setDelegate: on EidosTextView,
// you will not get the proper type-checking, and you will get a runtime error if your delegate object does not in fact conform
// to the EidosTextViewDelegate protocol.  But if you declare your conformance to the protocol, you should be fine.  This is a
// little weird, but the only good alternative is to have a separate delegate object for Eidos, which would just be annoying and
// confusing.
//
//@property (nonatomic, assign) id<EidosTextViewDelegate> delegate;

// The syntax coloring option being used
@property (nonatomic) EidosSyntaxColoringOption syntaxColoring;

// The font size (of Menlo) being used
@property (nonatomic) int displayFontSize;

// A flag to temporarily disable syntax coloring, used to coalesce multiple changes into a single recolor
@property (nonatomic) BOOL shouldRecolorAfterChanges;

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

// These are used by the Find Recipe panel in SLiM to highlight matches with search terms
- (void)clearHighlightMatches;
- (void)highlightMatchesForString:(NSString *)matchString;

// This method is used to construct the function/method prototypes shown in the status bar; client code
// probably will not call it, but possibly it could be used for context-sensitive help or something
- (NSAttributedString *)attributedSignatureForScriptString:(NSString *)scriptString selection:(NSRange)selection;

@end




































































