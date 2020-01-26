//
//  EidosConsoleWindowController.h
//  EidosScribe
//
//  Created by Ben Haller on 6/13/15.
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
#import "EidosConsoleTextView.h"
#import "EidosVariableBrowserController.h"


// BCH 4/7/2016: So we can build against the OS X 10.9 SDK
#ifndef NS_DESIGNATED_INITIALIZER
#define NS_DESIGNATED_INITIALIZER
#endif


@protocol EidosConsoleWindowControllerDelegate;


/*
 
 EidosConsoleWindowController provides a prefab Eidos console window containing a script view, a console
 view, a status bar, and various toolbar buttons.  It can be reused in Context code if you just want
 a standard Eidos console, and can be customized by supplying a delegate to it.
 
 */


// Defaults keys used to control various aspects of the user experience
extern NSString *EidosDefaultsShowTokensKey;
extern NSString *EidosDefaultsShowParseKey;
extern NSString *EidosDefaultsShowExecutionKey;
extern NSString *EidosDefaultsSuppressScriptCheckSuccessPanelKey;


@interface EidosConsoleWindowController : NSObject
{
	// ivars for handling input continuation
	BOOL isContinuationPrompt;
	NSUInteger originalPromptEnd;
}

// A delegate may be provided to customize various aspects of this class; see EidosConsoleWindowControllerDelegate.h
@property (nonatomic, assign) IBOutlet NSObject<EidosConsoleWindowControllerDelegate> *delegate;

// This property controls the enable state of UI that depends on the state of Eidos or its Context.  Some of
// the console window's UI does not; you can show/hide script help at any time, even if Eidos or its Context
// is in an invalid state, for example.  Other UI does; you can't execute if things are in an invalid state.
@property (nonatomic) BOOL interfaceEnabled;


// Outlets from EidosConsoleWindow.xib; it is unlikely that client code will need to access these
@property (nonatomic, retain) IBOutlet EidosVariableBrowserController *browserController;

@property (nonatomic, retain) IBOutlet NSWindow *scriptWindow;
@property (nonatomic, assign) IBOutlet NSSplitView *bottomSplitView;
@property (nonatomic, assign) IBOutlet EidosTextView *scriptTextView;
@property (nonatomic, assign) IBOutlet EidosConsoleTextView *outputTextView;
@property (nonatomic, assign) IBOutlet NSTextField *statusTextField;

@property (nonatomic, assign) IBOutlet NSButton *browserToggleButton;

// Normally EidosConsoleWindowController is instantiated in the EidosConsoleWindow.xib nib
- (instancetype)init NS_DESIGNATED_INITIALIZER;

// Show the console window and make the console output first responder
- (void)showWindow;
- (void)hideWindow;

// Tell the controller that the console window should be disposed of, not just closed; breaks retain loops
- (void)cleanup;

// Get the console textview; this can be used to append new output in the console, for example
- (EidosConsoleTextView *)textView;

// Throw away the current symbol table
- (void)invalidateSymbolTableAndFunctionMap;

// Make a new symbol table from our delegate's current state; this actually executes a minimal script, ";",
// to produce the symbol table as a side effect of setting up for the script's execution
- (void)validateSymbolTableAndFunctionMap;

// Execute the given script string, with the terminating semicolon being optional if requested
- (void)executeScriptString:(NSString *)scriptString withOptionalSemicolon:(BOOL)semicolonOptional;


// Actions used by EidosConsoleWindow.xib; may be called directly

// Check the syntax of the current script; will call eidosConsoleWindowController:checkScriptDidSucceed:
// if implemented by the delegate
- (IBAction)checkScript:(id)sender;

// Prettyprint the current script (after checking its syntax)
- (IBAction)prettyprintScript:(id)sender;

// Shows the shared script help window
- (IBAction)showScriptHelp:(id)sender;

// Clears all output in the console textview
- (IBAction)clearOutput:(id)sender;

// Executes all script currently in the script textview
- (IBAction)executeAll:(id)sender;

// Executes the line(s) containing the selection in the script textview
- (IBAction)executeSelection:(id)sender;

// Toggles the visibility of the console window
- (IBAction)toggleConsoleVisibility:(id)sender;

// Toggles the visibility of the variables browser
- (IBAction)toggleBrowserVisibility:(id)sender;

@end





































































