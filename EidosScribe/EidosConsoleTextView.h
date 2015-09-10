//
//  EidosConsoleTextView.h
//  EidosScribe
//
//  Created by Ben Haller on 4/8/15.
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
#import "EidosTextView.h"

@class EidosConsoleTextView;


/*
 
 EidosConsoleTextView provides a standard Eidos console textview.  It is integrated into EidosConsoleWindowController,
 so if you use that class you get a console textview for free, and do not have to use this class directly.  If you
 build your own Eidos UI, you might wish to use this class to provide a console.
 
 */


// A protocol that the delegate should respond to, to be notified when the user presses return/enter; this should
// trigger execution of the current line of input (i.e., all text after -promptRangeEnd).
@protocol EidosConsoleTextViewDelegate <NSObject>
- (void)eidosConsoleTextViewExecuteInput:(EidosConsoleTextView *)textView;
@end


@interface EidosConsoleTextView : EidosTextView
{
@public
	// At present this class is designed very quickly; EidosConsoleWindowController rummages around inside its
	// ivars freely.  It would be nice to clean up this design at some point.  FIXME
	NSRange lastPromptRange;
	
	NSMutableArray *history;
	int historyIndex;
	BOOL lastHistoryItemIsProvisional;	// got added to the history by a moveUp: event but is not an executed command
}

// Attribute dictionaries for some syntax coloring types
+ (NSDictionary *)promptAttrs;
+ (NSDictionary *)inputAttrs;
+ (NSDictionary *)outputAttrs;
+ (NSDictionary *)errorAttrs;
+ (NSDictionary *)tokensAttrs;
+ (NSDictionary *)parseAttrs;
+ (NSDictionary *)executionAttrs;

// Inisializers are inherited from NSTextView
//- (instancetype)initWithFrame:(NSRect)frameRect textContainer:(NSTextContainer *)container NS_DESIGNATED_INITIALIZER;
//- (instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;

// The last character position in the current Eidos prompt; user input starts at the next character position
- (NSUInteger)promptRangeEnd;

// Can be called to show the standard Eidos welcome message in the console
- (void)showWelcomeMessage;

// Adds a new Eidos prompt to the console, on the assumption that output is finished and the console is ready for input
- (void)showPrompt;

// Adds a thin spacer line to the console's output text; this provides nicer formatting for output
- (void)appendSpacer;

// Clears all output in the console
- (void)clearOutput;

// This adds a command to the console window's history; it should be called to register all commands executed
// in the console, including commands sent to the delegate to execute by -eidosConsoleTextViewExecuteInput:,
// since the console does not assume that that call results in successful execution.
- (void)registerNewHistoryItem:(NSString *)newItem;

@end




































































