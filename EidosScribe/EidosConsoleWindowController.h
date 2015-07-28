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
- (void)appendWelcomeMessageAddendum;
- (void)injectIntoInterpreter:(EidosInterpreter *)interpreter;
- (std::vector<EidosFunctionSignature*> *)injectedFunctionSignatures;
- (void)checkScriptDidSucceed:(BOOL)succeeded;
- (void)willExecuteScript;
- (void)didExecuteScript;
- (void)consoleWindowWillClose;

// User interface is enabled if !(continuousPlayOn || generationPlayOn).  It would be a bit better to have
// a single -userInterfaceEnabled property, but these properties already exist on SLiMWindowController and
// are the properties that most of the other user interface keys off of...
@property (nonatomic) BOOL continuousPlayOn;
@property (nonatomic) BOOL generationPlayOn;

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


































































