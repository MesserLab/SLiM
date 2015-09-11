//
//  EidosTextViewDelegate.h
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

#include <vector>
#include <string>

@class EidosTextView;
class EidosSymbolTable;
class EidosFunctionSignature;
class EidosMethodSignature;


/*
 
 This is an Objective-C++ header, and so can only be included by Objective-C++ compilations (.mm files instead of .m files).
 You should not need to include this header in your .h files, since you can declare protocol conformance in a class-continuation
 category in your .m file, so only classes that conform to this protocol should need to be Objective-C++.
 
 EidosTextViewDelegate is a protocol of optional methods that EidosTextView's delegate can implement, to allow the Context to
 customize code completion in an EidosTextView.  Note that if EidosConsoleWindowController is used, these methods get forwarded
 on by its delegate as well, so that EidosConsoleWindowController also gets Context-defined behavior.

 */


@protocol EidosTextViewDelegate
@required

// Provide a global symbol table to use for completion; EidosConsoleWindowController
// provides the symbol table for the ongoing interpreter context, for example.
// This is required because without it code completion is completely disabled,
// but if you really don't wish to implement it you can return nullptr.
- (EidosSymbolTable *)eidosTextViewGlobalSymbolTableForCompletion:(EidosTextView *)eidosTextView;

// Supply all method signatures for all methods of all classes; used to show the
// signature for the currently editing method call in the status bar (note that
// multiple methods of the same name but with different signatures should be avoided).
// This is required because without it the status bar's display of methods will not work,
// but if you really don't wish to implement it you can return nullptr.
- (const std::vector<const EidosMethodSignature*> *)eidosTextViewAllMethodSignatures:(EidosTextView *)eidosTextView;

@optional

// Supply additional language keywords that should be completed; used if the Context
// extends the grammar of Eidos, otherwise can be unimplemented
- (NSArray *)eidosTextViewLanguageKeywordsForCompletion:(EidosTextView *)eidosTextView;

// Supply additional function signatures for functions added by the Context, if any;
// otherwise can be unimplemented
- (const std::vector<const EidosFunctionSignature*> *)eidosTextViewInjectedFunctionSignatures:(EidosTextView *)eidosTextView;

// This allows the Context to define some special identifier tokens that should
// receive different syntax coloring from standard identifiers because they are
// in some way built in or provided by the Context automatically
- (bool)eidosTextView:(EidosTextView *)eidosTextView tokenStringIsSpecialIdentifier:(const std::string &)token_string;

@end





































