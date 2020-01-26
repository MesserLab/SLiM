//
//  EidosConsoleTextView.h
//  EidosScribe
//
//  Created by Ben Haller on 4/8/15.
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
#import "EidosTextView.h"


// BCH 4/7/2016: So we can build against the OS X 10.9 SDK
#ifndef NS_DESIGNATED_INITIALIZER
#define NS_DESIGNATED_INITIALIZER
#endif


@protocol EidosConsoleTextViewDelegate;


/*
 
 EidosConsoleTextView provides a standard Eidos console textview.  It is integrated into EidosConsoleWindowController,
 so if you use that class you get a console textview for free, and do not have to use this class directly.  If you
 build your own Eidos UI, you might wish to use this class to provide a console.
 
 */


@interface EidosConsoleTextView : EidosTextView
{
@public
	NSRange lastPromptRange;
	
	NSMutableArray *history;
	NSUInteger historyIndex;
	BOOL lastHistoryItemIsProvisional;	// got added to the history by a moveUp: event but is not an executed command
}

// A delegate for Eidos functionality; this is the same object as the NSText/NSTextView delegate, and is not declared explicitly
// here because overriding properties with a different type doesn't really work.  So when you call setDelegate: on EidosTextView,
// you will not get the proper type-checking, and you will get a runtime error if your delegate object does not in fact conform
// to the EidosTextViewDelegate protocol.  But if you declare your conformance to the protocol, you should be fine.  This is a
// little weird, but the only good alternative is to have a separate delegate object for Eidos, which would just be annoying and
// confusing.
//
//@property (nonatomic, assign) id<EidosConsoleTextViewDelegate> delegate;

// Initializers are inherited from NSTextView
//- (instancetype)initWithFrame:(NSRect)frameRect textContainer:(NSTextContainer *)container NS_DESIGNATED_INITIALIZER;
//- (instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;

// The character position following the current Eidos prompt; user input starts at this character position
- (NSUInteger)promptRangeEnd;
- (void)setPromptRangeEnd:(NSUInteger)promptEnd;

// Can be called to show the standard Eidos welcome message in the console
- (void)showWelcomeMessage;

// Adds a new Eidos prompt to the console, on the assumption that output is finished and the console is ready for input
- (void)showPrompt;
- (void)showPrompt:(unichar)promptChar;

// Adds a thin spacer line to the console's output text; this provides nicer formatting for output
- (void)appendSpacer;

// Clears output in the console
- (void)clearOutputToPosition:(NSUInteger)clearPosition;

// This adds a command to the console window's history; it should be called to register all commands executed
// in the console, including commands sent to the delegate to execute by -eidosConsoleTextViewExecuteInput:,
// since the console does not assume that that call results in successful execution.
- (void)registerNewHistoryItem:(NSString *)newItem;

@end




































































