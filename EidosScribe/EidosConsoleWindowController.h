//
//  EidosConsoleWindowController.h
//  EidosScribe
//
//  Created by Ben Haller on 6/13/15.
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
#import "EidosConsoleTextView.h"
#import "EidosVariableBrowserController.h"

#include "eidos_interpreter.h"


extern NSString *defaultsSuppressScriptCheckSuccessPanelKey;


@protocol EidosConsoleControllerDelegate <NSObject>
@optional
- (void)appendWelcomeMessageAddendum;
- (void)injectIntoInterpreter:(EidosInterpreter *)interpreter;
- (void)checkScriptDidSucceed:(BOOL)succeeded;
- (void)willExecuteScript;
- (void)didExecuteScript;
- (void)consoleWindowWillClose;

// messages from EidosTextViewDelegate that we forward on to our delegate
- (NSArray *)languageKeywordsForCompletion;
- (const std::vector<const EidosFunctionSignature*> *)injectedFunctionSignatures;
- (const std::vector<const EidosMethodSignature*> *)allMethodSignatures;
@end


@interface EidosConsoleWindowController : NSObject <EidosVariableBrowserDelegate, EidosConsoleTextViewDelegate>
{
	// The symbol table for the console interpreter; needs to be wiped whenever the symbol table changes
	EidosSymbolTable *global_symbols;
}

@property (nonatomic, assign) IBOutlet NSObject<EidosConsoleControllerDelegate> *delegate;
@property (nonatomic, retain) IBOutlet EidosVariableBrowserController *browserController;

@property (nonatomic, retain) IBOutlet NSWindow *scriptWindow;
@property (nonatomic, assign) IBOutlet NSSplitView *mainSplitView;
@property (nonatomic, assign) IBOutlet EidosTextView *scriptTextView;
@property (nonatomic, assign) IBOutlet EidosConsoleTextView *outputTextView;
@property (nonatomic, assign) IBOutlet NSTextField *statusTextField;

// This property controls the enable state of UI that depends on the state of Eidos or its Context.  Some of
// the console window's UI does not; you can show/hide script help at any time, even if Eidos or its Context
// is in an invalid state, for example.  Other UI does; you can't execute if things are in an invalid state.
@property (nonatomic) BOOL interfaceEnabled;

- (void)showWindow;
- (EidosConsoleTextView *)textView;

- (EidosSymbolTable *)symbols;
- (void)invalidateSymbolTable;		// throw away the current symbol table
- (void)validateSymbolTable;		// make a new symbol table from our delegate's current state

- (void)executeScriptString:(NSString *)scriptString addOptionalSemicolon:(BOOL)addSemicolon;

- (IBAction)checkScript:(id)sender;
- (IBAction)showScriptHelp:(id)sender;
- (IBAction)clearOutput:(id)sender;
- (IBAction)executeAll:(id)sender;
- (IBAction)executeSelection:(id)sender;

- (IBAction)toggleConsoleVisibility:(id)sender;

@end


































































