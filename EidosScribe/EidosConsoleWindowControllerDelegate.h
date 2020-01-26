//
//  EidosConsoleWindowControllerDelegate.h
//  EidosScribe
//
//  Created by Ben Haller on 9/10/15.
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

#include <vector>
#include <string>

#include "eidos_interpreter.h"
#include "EidosTextViewDelegate.h"

@class EidosConsoleWindowController;


/*
 
 This is an Objective-C++ header, and so can only be included by Objective-C++ compilations (.mm files instead of .m files).
 You should not need to include this header in your .h files, since you can declare protocol conformance in a class-continuation
 category in your .m file, so only classes that conform to this protocol should need to be Objective-C++.
 
 EidosConsoleWindowControllerDelegate is a protocol of optional methods that EidosConsoleWindowController's delegate can
 implement, to provide various Context-defined behaviors and modifications.

 */

@protocol EidosConsoleWindowControllerDelegate
@required

// a message from EidosTextViewDelegate that we essentially forward on to our delegate; see EidosTextView.h
- (const std::vector<EidosMethodSignature_CSP> *)eidosConsoleWindowControllerAllMethodSignatures:(EidosConsoleWindowController *)eidosConsoleController;

@optional

// If provided, this context object will be handed to EidosInterpreter objects created by the console
// controller when interpreting Eidos code; the context can then be obtained by Context implementations
// of functions and method using GetEidosContext(), to recover the context object for their own use
- (EidosContext *)eidosConsoleWindowControllerEidosContext:(EidosConsoleWindowController *)eidosConsoleController;

// This allows the Context to append its own welcome message to the console window on startup
- (void)eidosConsoleWindowControllerAppendWelcomeMessageAddendum:(EidosConsoleWindowController *)eidosConsoleController;

// This allows the Context to define its own symbols beyond those in Eidos itself
- (EidosSymbolTable *)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController symbolsFromBaseSymbols:(EidosSymbolTable *)baseSymbols;

// This allows the Context to define its own functions beyond those in Eidos itself
// The returned symbol table is not freed by the caller, since it is assumed to be
// an existing object with a lifetime managed by the callee.
- (EidosFunctionMap *)functionMapForEidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController;

// The functionMapForEidosConsoleWindowController: delegate method returns the current function map
// from the state of the delegate.  That may not include some optional functions, such as SLiM's
// zero-generation functions, that EidosConsoleWindowController wants to know about in some situations.
// This delegate method requests those optional functions to be added.
- (void)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController addOptionalFunctionsToMap:(EidosFunctionMap *)functionMap;

// This notifies the delegate that a script check operation did or did not succeed, allowing custom UI
- (void)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController checkScriptDidSucceed:(BOOL)succeeded;

// This delegate method is called immediately before a script block is executed, allowing custom setup
- (void)eidosConsoleWindowControllerWillExecuteScript:(EidosConsoleWindowController *)eidosConsoleController;

// This delegate method is called immediately after a script block is executed, allowing custom tear-down
- (void)eidosConsoleWindowControllerDidExecuteScript:(EidosConsoleWindowController *)eidosConsoleController;

// This delegate method is called just before a console window is closed
- (void)eidosConsoleWindowControllerConsoleWindowWillClose:(EidosConsoleWindowController *)eidosConsoleController;

// messages from EidosTextViewDelegate that we essentially forward on to our delegate; see EidosTextView.h
- (EidosSyntaxHighlightType)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController tokenStringIsSpecialIdentifier:(const std::string &)token_string;
- (NSString *)eidosConsoleWindowController:(EidosConsoleWindowController *)eidosConsoleController helpTextForClickedText:(NSString *)clickedText;

@end


// We also provide here a method on EidosConsoleWindowController that returns a C++ object and thus cannot be declared
// in the main header.  If you need to call this method, you can simply include this header to get its interface.

@interface EidosConsoleWindowController (CxxAdditions)

// provides access to the symbol table of the console window, sometimes used by the Context for completion or other tasks
- (EidosSymbolTable *)symbols;

@end
































