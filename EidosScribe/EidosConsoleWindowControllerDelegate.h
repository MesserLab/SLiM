//
//  EidosConsoleWindowControllerDelegate.h
//  EidosScribe
//
//  Created by Ben Haller on 9/10/15.
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

@class EidosConsoleWindowController;


/*
 
 A protocol of optional methods that EidosConsoleWindowController's delegate can implement,
 to provide various Context-defined behaviors and modifications.

 */

@protocol EidosConsoleWindowControllerDelegate <NSObject>
@optional

// If provided, this context object will be handed to EidosInterpreter objects created by the console
// controller when interpreting Eidos code; the context can then be obtained by Context implementations
// of functions and method using GetEidosContext(), to recover the context object for their own use
- (EidosContext *)eidosConsoleWindowControllerEidosContext:(EidosConsoleWindowController *)eidosConsoleController;

// This allows the Context to append its own welcome message to the console window on startup
- (void)eidosConsoleWindowControllerAppendWelcomeMessageAddendum:(EidosConsoleWindowController *)eidosConsoleController;

// This allows Context-defined functions and symbols to be injected into new interpreters prior to execution
- (void)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController injectIntoInterpreter:(EidosInterpreter *)interpreter;

// This notifies the delegate that a script check operation did or did not succeed, allowing custom UI
- (void)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController checkScriptDidSucceed:(BOOL)succeeded;

// This delegate method is called immediately before a script block is executed, allowing custom setup
- (void)eidosConsoleWindowControllerWillExecuteScript:(EidosConsoleWindowController *)eidosConsoleController;

// This delegate method is called immediately after a script block is executed, allowing custom tear-down
- (void)eidosConsoleWindowControllerDidExecuteScript:(EidosConsoleWindowController *)eidosConsoleController;

// This delegate method is called just before a console window is closed
- (void)eidosConsoleWindowControllerConsoleWindowWillClose:(EidosConsoleWindowController *)eidosConsoleController;

// messages from EidosTextViewDelegate that we essentially forward on to our delegate; see EidosTextView.h
- (NSArray *)eidosConsoleWindowControllerLanguageKeywordsForCompletion:(EidosConsoleWindowController *)eidosConsoleController;
- (const std::vector<const EidosFunctionSignature*> *)eidosConsoleWindowControllerInjectedFunctionSignatures:(EidosConsoleWindowController *)eidosConsoleController;
- (const std::vector<const EidosMethodSignature*> *)eidosConsoleWindowControllerAllMethodSignatures:(EidosConsoleWindowController *)eidosConsoleController;
- (bool)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController tokenStringIsSpecialIdentifier:(const std::string &)token_string;
@end
