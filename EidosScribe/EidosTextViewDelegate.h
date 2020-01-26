//
//  EidosTextViewDelegate.h
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
#include "eidos_type_table.h"
#include "eidos_type_interpreter.h"

@class EidosTextView;


/*
 
 This is an Objective-C++ header, and so can only be included by Objective-C++ compilations (.mm files instead of .m files).
 You should not need to include this header in your .h files, since you can declare protocol conformance in a class-continuation
 category in your .m file, so only classes that conform to this protocol should need to be Objective-C++.
 
 EidosTextViewDelegate is a protocol of optional methods that EidosTextView's delegate can implement, to allow the Context to
 customize code completion in an EidosTextView.  Note that if EidosConsoleWindowController is used, these methods get forwarded
 on by its delegate as well, so that EidosConsoleWindowController also gets Context-defined behavior.

 */

enum class EidosSyntaxHighlightType
{
	kNoSyntaxHighlight,
	kHighlightAsIdentifier,
	kHighlightAsKeyword,
	kHighlightAsContextKeyword
};

@interface EidosTextView(ObjCppExtensions)

// Getting a "definitive" function map using type-interpreting to add to the delegate's function map
- (EidosFunctionMap *)functionMapForScriptString:(NSString *)scriptString includingOptionalFunctions:(BOOL)includingOptionalFunctions;
- (EidosFunctionMap *)functionMapForTokenizedScript:(EidosScript &)script includingOptionalFunctions:(BOOL)includingOptionalFunctions;

@end

@protocol EidosTextViewDelegate <NSTextViewDelegate>
@required

// Supply all method signatures for all methods of all classes; used to show the
// signature for the currently editing method call in the status bar (note that
// multiple methods of the same name but with different signatures should be avoided).
// This is required because without it the status bar's display of methods will not work,
// but if you really don't wish to implement it you can return nullptr.
- (const std::vector<EidosMethodSignature_CSP> *)eidosTextViewAllMethodSignatures:(EidosTextView *)eidosTextView;

@optional

// This allows the Context to define its own symbols beyond those in Eidos itself.
// The returned symbol table is not freed by the caller, since it is assumed to be
// an existing object with a lifetime managed by the callee.
- (EidosSymbolTable *)eidosTextView:(EidosTextView *)eidosTextView symbolsFromBaseSymbols:(EidosSymbolTable *)baseSymbols;

// This allows the Context to define its own functions beyond those in Eidos itself.
// The returned symbol table is not freed by the caller, since it is assumed to be
// an existing object with a lifetime managed by the callee.
- (EidosFunctionMap *)functionMapForEidosTextView:(EidosTextView *)eidosTextView;

// The functionMapForEidosTextView: delegate method returns the current function map from
// the state of the delegate.  That may not include some optional functions, such as SLiM's
// zero-generation functions, that EidosTextView wants to know about in some situations.
// This delegate method requests those optional functions to be added.
- (void)eidosTextView:(EidosTextView *)eidosTextView addOptionalFunctionsToMap:(EidosFunctionMap *)functionMap;

// This allows the Context to define some special identifier tokens that should
// receive different syntax coloring from standard identifiers because they are
// in some way built in or provided by the Context automatically
- (EidosSyntaxHighlightType)eidosTextView:(EidosTextView *)eidosTextView tokenStringIsSpecialIdentifier:(const std::string &)token_string;

// This allows the Context to define substitutions for help searches when the user
// option-clicks a token, to provide more targeted help results.  If no substitution
// is desired, returning nil is recommended.
- (NSString *)eidosTextView:(EidosTextView *)eidosTextView helpTextForClickedText:(NSString *)clickedText;

// This allows the Context to customize the behavior of code completion, depending upon
// the context in which the completion occurs (as determined by the script string, which
// extends up to the end of the selection, and the selection range).  The delegate should
// add types to the type table (which is empty), add functions to the function map (which
// has the built-in Eidos functions already), and add applicable language keywords to the
// keywords array.  If this delegate method is not implemented, EidosTextView will do its
// standard behavior.  In particular, types will be found with ParseInterpreterBlockToAST()
// and TypeEvaluateInterpreterBlock() in addition to symbolsFromBaseSymbols:, functions
// will be found with functionMapForEidosTextView:, and no keywords will be added to the base
// set.  This standard behavior is fine for Contexts that do not define context-dependent
// language constructs in the way that SLiM does.
//
// Note that unlike symbolsFromBaseSymbols: and functionMapForEidosTextView:, here the delegate
// is expected to modify the objects passed to it.  This difference is motivated by the idea
// that the other delegate methods are providing a standard symbol table and function map
// kept by the Context, whereas this delegate method is expected to create context-dependent
// information that differs from the current state of the Context.  The delegate may even
// replace the pointers passed to the type table and/or function map, in order to substitute
// a new object (perhaps a subclass object) for those objects; in that case, the substituted
// object will be freed by the caller (not the delegate), so don't give your private objects.
//
// Also note that the delegate does not need to worry about uniquing or sorting type entries.
//
// Return NO if you want Eidos to do its default behavior, YES if you have taken care of it.
- (BOOL)eidosTextView:(EidosTextView *)eidosTextView completionContextWithScriptString:(NSString *)scriptString selection:(NSRange)selection typeTable:(EidosTypeTable **)typeTable functionMap:(EidosFunctionMap **)functionMap callTypeTable:(EidosCallTypeTable **)callTypeTable keywords:(NSMutableArray *)keywords argumentNameCompletions:(std::vector<std::string> *)argNameCompletions;

@end


















































