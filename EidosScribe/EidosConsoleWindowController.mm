//
//  EidosConsoleWindowController.m
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


#import "EidosConsoleWindowController.h"
#import "EidosConsoleWindowControllerDelegate.h"
#import "EidosTextView.h"
#import "EidosCocoaExtra.h"
#import "EidosVariableBrowserControllerDelegate.h"
#import "EidosTextViewDelegate.h"
#import "EidosConsoleTextViewDelegate.h"
#import "EidosHelpController.h"

#include "eidos_script.h"
#include "eidos_global.h"
#include "eidos_interpreter.h"
#include "eidos_call_signature.h"

#include <iostream>
#include <sstream>
#include <stdexcept>


using std::string;
using std::vector;
using std::map;
using std::endl;
using std::istringstream;
using std::istream;
using std::ostream;


// User defaults keys
NSString *EidosDefaultsShowTokensKey = @"EidosShowTokens";
NSString *EidosDefaultsShowParseKey = @"EidosShowParse";
NSString *EidosDefaultsShowExecutionKey = @"EidosShowExecution";
NSString *EidosDefaultsSuppressScriptCheckSuccessPanelKey = @"EidosSuppressScriptCheckSuccessPanel";


@interface EidosConsoleWindowController () <EidosVariableBrowserControllerDelegate, EidosConsoleTextViewDelegate>
{
	// The symbol table for the console interpreter; needs to be wiped whenever the symbol table changes
	EidosSymbolTable *global_symbols;
}
@end


@implementation EidosConsoleWindowController

@synthesize delegate, browserController, scriptWindow, mainSplitView, scriptTextView, outputTextView, statusTextField;

+ (void)initialize
{
	[[NSUserDefaults standardUserDefaults] registerDefaults:@{
															  EidosDefaultsShowTokensKey : @NO,
															  EidosDefaultsShowParseKey : @NO,
															  EidosDefaultsShowExecutionKey : @NO,
															  EidosDefaultsSuppressScriptCheckSuccessPanelKey : @NO
															  }];
}

- (instancetype)init
{
	if (self = [super init])
	{
		[self setInterfaceEnabled:YES];
		
		// Observe notifications to keep our variable browser toggle button up to date
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(browserWillShow:) name:EidosVariableBrowserWillShowNotification object:nil];
		[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(browserWillHide:) name:EidosVariableBrowserWillHideNotification object:nil];
	}
	
	return self;
}

- (void)awakeFromNib
{
	[self setInterfaceEnabled:YES];
	
	// Tell Cocoa that we can go full-screen
	[scriptWindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
	
	// Fix our splitview's position restore, which NSSplitView sometimes screws up
	[mainSplitView eidosRestoreAutosavedPositions];
	
	// Show a welcome message
	[outputTextView showWelcomeMessage];
	
	if ([delegate respondsToSelector:@selector(eidosConsoleWindowControllerAppendWelcomeMessageAddendum:)])
		[delegate eidosConsoleWindowControllerAppendWelcomeMessageAddendum:self];
	
	// And show our prompt
	[outputTextView showPrompt];
	
	// Set up syntax coloring for the script view; note the console view's coloring is handled internally by us
	[scriptTextView setSyntaxColoring:kEidosSyntaxColoringEidos];
	
	// Execute a null statement to get our symbols set up, for code completion etc.
	// Note this has the side effect of creating a random number generator gEidos_rng for our use.
	[self validateSymbolTable];
}

- (void)dealloc
{
	//NSLog(@"EidosConsoleWindowController dealloc");
	
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	
	[self finalize];
	
	[self setMainSplitView:nil];
	[self setScriptTextView:nil];
	[self setOutputTextView:nil];
	[self setStatusTextField:nil];
	[self setBrowserToggleButton:nil];
	
	[super dealloc];
}

- (void)setDelegate:(NSObject<EidosConsoleWindowControllerDelegate> *)newDelegate
{
	if (newDelegate && ![newDelegate conformsToProtocol:@protocol(EidosConsoleWindowControllerDelegate)])
		NSLog(@"Delegate %@ assigned to EidosConsoleWindowController %p does not conform to the EidosConsoleWindowControllerDelegate protocol!", newDelegate, self);
	
	delegate = newDelegate;	// nonatomic, assign
}

- (NSObject<EidosConsoleWindowControllerDelegate> *)delegate
{
	return delegate;
}

- (void)browserWillShow:(NSNotification *)note
{
	if ([note object] == browserController)
		[_browserToggleButton setState:NSOnState];
}

- (void)browserWillHide:(NSNotification *)note
{
	if ([note object] == browserController)
		[_browserToggleButton setState:NSOffState];
}

- (void)showWindow
{
	[scriptWindow makeKeyAndOrderFront:nil];
	[scriptWindow makeFirstResponder:outputTextView];
}

- (void)hideWindow
{
	[scriptWindow performClose:nil];
}

- (void)finalize
{
	delete global_symbols;
	global_symbols = nil;
	
	[mainSplitView setDelegate:nil];
	[self setMainSplitView:nil];
	
	[scriptWindow setDelegate:nil];
	[self setScriptWindow:nil];
	
	[browserController finalize];
	[self setBrowserController:nil];
	
	[self setDelegate:nil];
}

- (EidosConsoleTextView *)textView;
{
	return outputTextView;
}

- (void)invalidateSymbolTable
{
	if (global_symbols)
	{
		delete global_symbols;
		global_symbols = nil;
	}
	
	[browserController reloadBrowser];
}

- (void)validateSymbolTable
{
	if (!global_symbols)
	{
		NSString *errorString = nil;
		
		[self _executeScriptString:@";" tokenString:NULL parseString:NULL executionString:NULL errorString:&errorString withOptionalSemicolon:NO];
		
		if (errorString)
			NSLog(@"Error in validateSymbolTable: %@", errorString);
	}
	
	[browserController reloadBrowser];
}

- (NSString *)_executeScriptString:(NSString *)scriptString tokenString:(NSString **)tokenString parseString:(NSString **)parseString executionString:(NSString **)executionString errorString:(NSString **)errorString withOptionalSemicolon:(BOOL)semicolonOptional
{
	string script_string([scriptString UTF8String]);
	EidosScript script(script_string);
	string output;
	
	// Make the final semicolon optional if requested; this allows input like "6+7" in the console
	if (semicolonOptional)
		script.SetFinalSemicolonOptional(true);
	
	// Tokenize
	try
	{
		script.Tokenize();
		
		if (tokenString)
		{
			std::ostringstream token_stream;
			
			script.PrintTokens(token_stream);
			
			*tokenString = [NSString stringWithUTF8String:token_stream.str().c_str()];
			
#if 0
			// Do a checkback on UTF8 token ranges versus UTF16 token ranges by replicating the token string using the UTF16 ranges
			const std::vector<EidosToken *> &tokens = script.Tokens();
			NSMutableString *replicateTokenString = [NSMutableString string];
			BOOL firstToken = YES;
			
			for (const EidosToken *token : tokens)
			{
				int32_t token_UTF16_start = token->token_UTF16_start_;
				int32_t token_UTF16_end = token->token_UTF16_end_;
				NSRange tokenRange = NSMakeRange(token_UTF16_start, token_UTF16_end - token_UTF16_start + 1);
				
				[replicateTokenString appendString:(firstToken ? @"" : @" ")];
				
				if (tokenRange.location == [scriptString length])
					[replicateTokenString appendString:@"EOF"];
				else
					[replicateTokenString appendString:[scriptString substringWithRange:tokenRange]];
				
				firstToken = NO;
			}
			
			[replicateTokenString appendString:@"\n"];
			*tokenString = [*tokenString stringByAppendingString:replicateTokenString];
#endif
		}
	}
	catch (std::runtime_error err)
	{
		*errorString = [NSString stringWithUTF8String:EidosGetUntrimmedRaiseMessage().c_str()];
		return nil;
	}
	
	// Parse, an "interpreter block" bounded by an EOF rather than a "script block" that requires braces
	try
	{
		script.ParseInterpreterBlockToAST();
		
		if (parseString)
		{
			std::ostringstream parse_stream;
			
			script.PrintAST(parse_stream);
			
			*parseString = [NSString stringWithUTF8String:parse_stream.str().c_str()];
		}
	}
	catch (std::runtime_error err)
	{
		*errorString = [NSString stringWithUTF8String:EidosGetUntrimmedRaiseMessage().c_str()];
		return nil;
	}
	
	// Get a symbol table and let our delegate add symbols to it
	if (!global_symbols)
	{
		global_symbols = gEidosConstantsSymbolTable;
		
		if ([delegate respondsToSelector:@selector(eidosConsoleWindowController:symbolsFromBaseSymbols:)])
			global_symbols = [delegate eidosConsoleWindowController:self symbolsFromBaseSymbols:global_symbols];
		
		global_symbols = new EidosSymbolTable(false, global_symbols);	// add a table for script-defined variables on top
	}
	
	// Get a function map and let our delegate add functions to it
	EidosFunctionMap *function_map = EidosInterpreter::BuiltInFunctionMap();
	
	if ([delegate respondsToSelector:@selector(eidosConsoleWindowController:functionMapFromBaseMap:)])
		function_map = [delegate eidosConsoleWindowController:self functionMapFromBaseMap:function_map];
	
	// Get the EidosContext, if any, from the delegate
	EidosContext *eidos_context = nullptr;
	
	if ([delegate respondsToSelector:@selector(eidosConsoleWindowControllerEidosContext:)])
		eidos_context = [delegate eidosConsoleWindowControllerEidosContext:self];
	
	// Interpret the parsed block
	if ([delegate respondsToSelector:@selector(eidosConsoleWindowControllerWillExecuteScript:)])
		[delegate eidosConsoleWindowControllerWillExecuteScript:self];
	
	EidosInterpreter interpreter(script, *global_symbols, *function_map, eidos_context);
	
	try
	{
		if (executionString)
			interpreter.SetShouldLogExecution(true);
		
		EidosValue_SP result = interpreter.EvaluateInterpreterBlock(true);	// result not used
		output = interpreter.ExecutionOutput();
		
		// reload outline view to show new global symbols, in case they have changed
		[browserController reloadBrowser];
		
		if (executionString)
			*executionString = [NSString stringWithUTF8String:interpreter.ExecutionLog().c_str()];
	}
	catch (std::runtime_error err)
	{
		if ([delegate respondsToSelector:@selector(eidosConsoleWindowControllerDidExecuteScript:)])
			[delegate eidosConsoleWindowControllerDidExecuteScript:self];
		
		output = interpreter.ExecutionOutput();
		
		*errorString = [NSString stringWithUTF8String:EidosGetUntrimmedRaiseMessage().c_str()];
		
		return [NSString stringWithUTF8String:output.c_str()];
	}
	
	if ([delegate respondsToSelector:@selector(eidosConsoleWindowControllerDidExecuteScript:)])
		[delegate eidosConsoleWindowControllerDidExecuteScript:self];
	
	return [NSString stringWithUTF8String:output.c_str()];
}

- (void)executeScriptString:(NSString *)scriptString withOptionalSemicolon:(BOOL)semicolonOptional
{
	NSTextStorage *ts = [outputTextView textStorage];
	NSString *tokenString = nil, *parseString = nil, *executionString = nil, *errorString = nil;
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	BOOL showTokens = [defaults boolForKey:EidosDefaultsShowTokensKey];
	BOOL showParse = [defaults boolForKey:EidosDefaultsShowParseKey];
	BOOL showExecution = [defaults boolForKey:EidosDefaultsShowExecutionKey];
	NSUInteger promptEnd = [outputTextView promptRangeEnd];
	NSRange scriptRange = NSMakeRange(promptEnd, [scriptString length]);
	
	NSString *result = [self _executeScriptString:scriptString
									  tokenString:(showTokens ? &tokenString : NULL)
									  parseString:(showParse ? &parseString : NULL)
								  executionString:(showExecution ? &executionString : NULL)
									  errorString:&errorString
							withOptionalSemicolon:semicolonOptional];
	
	if ([errorString containsString:@"unexpected token 'EOF'"])
	{
		// The user has entered an incomplete script line, so we need to append a newline...
		NSAttributedString *outputString1 = [[NSAttributedString alloc] initWithString:@"\n" attributes:[NSDictionary eidosInputAttrs]];
		
		[ts beginEditing];
		[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:outputString1];
		[ts endEditing];
		
		[outputString1 release];
		
		// ...and issue a continuation prompt to await further input
		[outputTextView showPrompt:'+'];
		originalPromptEnd = promptEnd;
		isContinuationPrompt = YES;
	}
	else
	{
		// make the attributed strings we will append
		NSAttributedString *outputString1 = [[NSAttributedString alloc] initWithString:@"\n" attributes:[NSDictionary eidosInputAttrs]];
		NSAttributedString *outputString2 = (result ? [[NSAttributedString alloc] initWithString:result attributes:[NSDictionary eidosOutputAttrs]] : nil);
		NSAttributedString *errorAttrString = (errorString ? [[NSAttributedString alloc] initWithString:errorString attributes:[NSDictionary eidosErrorAttrs]] : nil);
		NSAttributedString *tokenAttrString = (tokenString ? [[NSAttributedString alloc] initWithString:tokenString attributes:[NSDictionary eidosTokensAttrs]] : nil);
		NSAttributedString *parseAttrString = (parseString ? [[NSAttributedString alloc] initWithString:parseString attributes:[NSDictionary eidosParseAttrs]] : nil);
		NSAttributedString *executionAttrString = (executionString ? [[NSAttributedString alloc] initWithString:executionString attributes:[NSDictionary eidosExecutionAttrs]] : nil);;
		
		// do the editing
		[ts beginEditing];
		[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:outputString1];
		[outputTextView appendSpacer];
		
		if (tokenAttrString)
		{
			[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:tokenAttrString];
			[outputTextView appendSpacer];
		}
		if (parseAttrString)
		{
			[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:parseAttrString];
			[outputTextView appendSpacer];
		}
		if (executionAttrString)
		{
			[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:executionAttrString];
			[outputTextView appendSpacer];
		}
		if ([outputString2 length])
		{
			[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:outputString2];
			[outputTextView appendSpacer];
		}
		if (errorAttrString)
		{
			[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:errorAttrString];
			[outputTextView appendSpacer];
		}
		
		if (errorString && !gEidosExecutingRuntimeScript && (gEidosCharacterStartOfErrorUTF16 >= 0) && (gEidosCharacterEndOfErrorUTF16 >= gEidosCharacterStartOfErrorUTF16) && (scriptRange.location != NSNotFound))
		{
			// An error occurred, so let's try to highlight it in the input
			int errorTokenStart = gEidosCharacterStartOfErrorUTF16 + (int)scriptRange.location;
			int errorTokenEnd = gEidosCharacterEndOfErrorUTF16 + (int)scriptRange.location;
			
			NSRange charRange = NSMakeRange(errorTokenStart, errorTokenEnd - errorTokenStart + 1);
			
			[ts addAttribute:NSBackgroundColorAttributeName value:[NSColor redColor] range:charRange];
			[ts addAttribute:NSForegroundColorAttributeName value:[NSColor whiteColor] range:charRange];
		}
		
		[ts endEditing];
		
		// clean up
		[outputString1 release];
		[tokenAttrString release];
		[parseAttrString release];
		[executionAttrString release];
		[outputString2 release];
		[errorAttrString release];
		
		// and show a new prompt
		[outputTextView showPrompt];
	}
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
	SEL sel = [menuItem action];
	BOOL uiEnabled = [self interfaceEnabled];
	
	if (sel == @selector(checkScript:))
		return uiEnabled;
	if (sel == @selector(executeAll:))
		return uiEnabled;
	if (sel == @selector(executeSelection:))
		return uiEnabled;
	if (sel == @selector(toggleConsoleVisibility:))
		[menuItem setTitle:([scriptWindow isVisible] ? @"Hide Eidos Console" : @"Show Eidos Console")];
	if (sel == @selector(toggleBrowserVisibility:))
		return [browserController validateMenuItem:menuItem];
	
	return YES;
}


//
//	Actions
//
#pragma mark -
#pragma mark Actions

- (IBAction)checkScript:(id)sender
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	NSString *currentScriptString = [scriptTextView string];
	const char *cstr = [currentScriptString UTF8String];
	NSString *errorDiagnostic = nil;
	
	if (!cstr)
	{
		errorDiagnostic = [@"The script string could not be read, possibly due to an encoding problem." retain];
	}
	else
	{
		EidosScript script(cstr);
		
		try {
			script.Tokenize();
			script.ParseInterpreterBlockToAST();
		}
		catch (std::runtime_error err)
		{
			errorDiagnostic = [[NSString stringWithUTF8String:EidosGetTrimmedRaiseMessage().c_str()] retain];
		}
	}
	
	if ([delegate respondsToSelector:@selector(eidosConsoleWindowController:checkScriptDidSucceed:)])
		[delegate eidosConsoleWindowController:self checkScriptDidSucceed:!errorDiagnostic];
	else
		[[NSSound soundNamed:(!errorDiagnostic ? @"Bottle" : @"Ping")] play];
	
	if (errorDiagnostic)
	{
		// On failure, we show an alert describing the error, and highlight the relevant script line
		NSAlert *alert = [[NSAlert alloc] init];
		
		[alert setAlertStyle:NSWarningAlertStyle];
		[alert setMessageText:@"Script error"];
		[alert setInformativeText:errorDiagnostic];
		[alert addButtonWithTitle:@"OK"];
		
		[alert beginSheetModalForWindow:scriptWindow completionHandler:^(NSModalResponse returnCode) { [alert autorelease]; }];
		
		[scriptTextView selectErrorRange];
		
		// Show the error in the status bar also
		NSString *trimmedError = [errorDiagnostic stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
		NSDictionary *errorAttrs = [NSDictionary eidosTextAttributesWithColor:[NSColor redColor]];
		NSMutableAttributedString *errorAttrString = [[[NSMutableAttributedString alloc] initWithString:trimmedError attributes:errorAttrs] autorelease];
		
		[errorAttrString addAttribute:NSBaselineOffsetAttributeName value:[NSNumber numberWithFloat:2.0] range:NSMakeRange(0, [errorAttrString length])];
		[statusTextField setAttributedStringValue:errorAttrString];
	}
	else
	{
		// On success, we optionally show a success alert sheet
		if (![defaults boolForKey:EidosDefaultsSuppressScriptCheckSuccessPanelKey])
		{
			NSAlert *alert = [[NSAlert alloc] init];
			
			[alert setAlertStyle:NSInformationalAlertStyle];
			[alert setMessageText:@"No script errors"];
			[alert setInformativeText:@"No errors found."];
			[alert addButtonWithTitle:@"OK"];
			[alert setShowsSuppressionButton:YES];
			
			[alert beginSheetModalForWindow:scriptWindow completionHandler:^(NSModalResponse returnCode) {
				if ([[alert suppressionButton] state] == NSOnState)
					[defaults setBool:YES forKey:EidosDefaultsSuppressScriptCheckSuccessPanelKey];
				[alert autorelease];
			}];
		}
	}
	
	[errorDiagnostic release];
}

- (IBAction)showScriptHelp:(id)sender
{
	[[EidosHelpController sharedController] showWindow];
}

- (IBAction)clearOutput:(id)sender
{
	[outputTextView clearOutput];
}

- (void)fixDumbSelectionBug:(id)unused
{
	// See comment below in toggleConsoleVisibility:
	[outputTextView setSelectable:YES];
	[outputTextView setEditable:YES];
	[scriptWindow makeFirstResponder:outputTextView];
}

- (IBAction)toggleConsoleVisibility:(id)sender
{
	if ([scriptWindow isVisible])
		[scriptWindow performClose:nil];
	else
	{
		[scriptWindow makeKeyAndOrderFront:nil];
		
		// I have no idea what the heck is going on here, but without this code, the console window comes up – in SLiMgui only – with
		// the insertion point blinking several lines up from the prompt, in a nonsensical position.  It looks to me like an AppKit bug;
		// I think the Kit is deciding where the insertion point is prior to some view resize or relayout operation, and the position
		// isn't getting recalculated after that resize/relayout.  I couldn't figure out why it was doing that, or why it happens only
		// in SLiMgui, and this is the cleanest workaround I could find.  Sheesh.  BCH 9 February 2016.
		[outputTextView setSelectable:NO];
		[scriptWindow makeFirstResponder:nil];
		[self performSelector:@selector(fixDumbSelectionBug:) withObject:nil afterDelay:0.0];
	}
}

- (IBAction)toggleBrowserVisibility:(id)sender
{
	[browserController toggleBrowserVisibility:nil];
}

- (void)elideContinuationPrompt
{
	// This replaces the continuation prompt, if there is one, with a space, and switches the active prompt back to the original prompt;
	// the net effect is as if the user entered a newline and two spaces at the original prompt, with no continuation.  Note that the
	// two spaces at the beginning of continuation lines is mirrored in fullInputString, below.
	if (isContinuationPrompt)
	{
		NSTextStorage *ts = [outputTextView textStorage];
		NSDictionary *inputAttrs = [NSDictionary eidosInputAttrs];
		NSAttributedString *promptString1 = [[NSAttributedString alloc] initWithString:@" " attributes:inputAttrs];
		NSUInteger promptEnd = [outputTextView promptRangeEnd];
		
		[ts beginEditing];
		[ts replaceCharactersInRange:NSMakeRange(promptEnd - 2, 1) withAttributedString:promptString1];
		[ts endEditing];
		
		[outputTextView setPromptRangeEnd:originalPromptEnd];
		isContinuationPrompt = NO;
	}
}

- (NSString *)fullInputString
{
	[self elideContinuationPrompt];
	
	NSString *fullInputString = [outputTextView string];
	NSUInteger promptEnd = [outputTextView promptRangeEnd];
	
	return [fullInputString substringFromIndex:promptEnd];
}

- (IBAction)executeAll:(id)sender
{
	NSTextStorage *ts = [outputTextView textStorage];
	NSDictionary *inputAttrs = [NSDictionary eidosInputAttrs];
	NSString *fullScriptString = [scriptTextView string];
	NSAttributedString *scriptAttrString = [[[NSAttributedString alloc] initWithString:fullScriptString attributes:inputAttrs] autorelease];
	NSUInteger promptEnd = [outputTextView promptRangeEnd];
	
	// The contents of the current prompt line get replaced by the execution block
	[ts beginEditing];
	[ts replaceCharactersInRange:NSMakeRange(promptEnd, [ts length] - promptEnd) withAttributedString:scriptAttrString];
	[ts endEditing];
	
	// The current prompt might be a continuation prompt, so now we get the full input string from the original prompt
	NSString *executionString = [self fullInputString];
	
	[outputTextView registerNewHistoryItem:executionString];
	[self executeScriptString:executionString withOptionalSemicolon:YES];
}

- (IBAction)executeSelection:(id)sender
{
	NSTextStorage *ts = [outputTextView textStorage];
	NSString *fullScriptString = [scriptTextView string];
	NSUInteger scriptLength = [fullScriptString length];
	NSRange selectedRange = [scriptTextView selectedRange];
	NSCharacterSet *newlineChars = [NSCharacterSet newlineCharacterSet];
	NSRange executionRange = selectedRange;	// indices of the first and last characters to execute
	
	// If the selection is an insertion point, execute the whole line
	if (executionRange.length == 0)
	{
		// start at the start of the selection and move backwards to the beginning of the line
		while (executionRange.location > 0)
		{
			unichar ch = [fullScriptString characterAtIndex:executionRange.location - 1];
			
			if ([newlineChars characterIsMember:ch])
				break;
			
			--executionRange.location;
			++executionRange.length;
		}
		
		// now move the end of the selection backwards to remove any newlines from the end of the range to execute
		while (executionRange.length > 0)
		{
			unichar ch = [fullScriptString characterAtIndex:executionRange.location + executionRange.length - 1];
			
			if (![newlineChars characterIsMember:ch])
				break;
			
			--executionRange.length;
		}
		
		// now move the end of the selection forwards to the end of the line, not including the newline
		while (executionRange.location + executionRange.length < scriptLength)
		{
			unichar ch = [fullScriptString characterAtIndex:executionRange.location + executionRange.length];
			
			if ([newlineChars characterIsMember:ch])
				break;
			
			++executionRange.length;
		}
	}
	
	// now execute the range we have found
	if (executionRange.length > 0)
	{
		NSString *scriptString = [fullScriptString substringWithRange:executionRange];
		NSDictionary *inputAttrs = [NSDictionary eidosInputAttrs];
		NSAttributedString *scriptAttrString = [[[NSAttributedString alloc] initWithString:scriptString attributes:inputAttrs] autorelease];
		NSUInteger promptEnd = [outputTextView promptRangeEnd];
		
		// The contents of the current prompt line get replaced by the execution line
		[ts beginEditing];
		[ts replaceCharactersInRange:NSMakeRange(promptEnd, [ts length] - promptEnd) withAttributedString:scriptAttrString];
		[ts endEditing];
		
		// The current prompt might be a continuation prompt, so now we get the full input string from the original prompt
		NSString *executionString = [self fullInputString];
		
		[outputTextView registerNewHistoryItem:executionString];
		[self executeScriptString:executionString withOptionalSemicolon:YES];
	}
	else
	{
		NSBeep();
	}
}


//
//	VariableBrowserControllerDelegate methods
//
#pragma mark -
#pragma mark VariableBrowserControllerDelegate

- (EidosSymbolTable *)symbolTableForEidosVariableBrowserController:(EidosVariableBrowserController *)browserController
{
	return global_symbols;
}


//
//	NSTextViewDelegate methods
//
#pragma mark -
#pragma mark NSTextViewDelegate

- (NSRange)textView:(NSTextView *)textView willChangeSelectionFromCharacterRange:(NSRange)oldRange toCharacterRange:(NSRange)newRange
{
	if (textView == outputTextView)
	{
		// prevent a zero-length selection (i.e. an insertion point) in the history
		if ((newRange.length == 0) && (newRange.location < [outputTextView promptRangeEnd]))
			return NSMakeRange([[outputTextView string] length], 0);
	}
	
	return newRange;
}

- (BOOL)textView:(NSTextView *)textView shouldChangeTextInRange:(NSRange)affectedCharRange replacementString:(NSString *)replacementString
{
	if (textView == outputTextView)
	{
		// prevent the user from changing anything above the current prompt
		if (affectedCharRange.location < [outputTextView promptRangeEnd])
			return NO;
	}
	
	return YES;
}


//
//	EidosConsoleTextViewDelegate methods
//
#pragma mark -
#pragma mark EidosConsoleTextViewDelegate

- (void)eidosConsoleTextViewExecuteInput:(EidosConsoleTextView *)textView
{
	if (textView == outputTextView)
	{
		if (isContinuationPrompt && ([[outputTextView string] length] == [outputTextView promptRangeEnd]))
		{
			// If the user has hit return at an empty continuation prompt, we take that as a sign that they want to get out of it
			NSString *executionString = [self fullInputString];
			
			[outputTextView registerNewHistoryItem:executionString];
			
			NSTextStorage *ts = [outputTextView textStorage];
			NSAttributedString *outputString1 = [[NSAttributedString alloc] initWithString:@"\n" attributes:[NSDictionary eidosInputAttrs]];
			
			[ts beginEditing];
			[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:outputString1];
			[ts endEditing];
			
			[outputString1 release];
			
			// show a new prompt
			[outputTextView showPrompt];
		}
		else
		{
			// The current prompt might be a non-empty continuation prompt, so now we get the full input string from the original prompt
			NSString *executionString = [self fullInputString];
			
			[outputTextView registerNewHistoryItem:executionString];
			[self executeScriptString:executionString withOptionalSemicolon:YES];
		}
	}
}


//
//	EidosTextViewDelegate methods
//
#pragma mark -
#pragma mark EidosTextViewDelegate

- (NSArray *)eidosTextViewLanguageKeywordsForCompletion:(EidosTextView *)eidosTextView
{
	if ([delegate respondsToSelector:@selector(eidosConsoleWindowControllerLanguageKeywordsForCompletion:)])
		return [delegate eidosConsoleWindowControllerLanguageKeywordsForCompletion:self];
	else
		return nil;
}

- (EidosSymbolTable *)eidosTextView:(EidosTextView *)eidosTextView symbolsFromBaseSymbols:(EidosSymbolTable *)baseSymbols
{
	return global_symbols;	// we keep our own symbol table, so we don't call our delegate here
}

- (EidosFunctionMap *)eidosTextView:(EidosTextView *)eidosTextView functionMapFromBaseMap:(EidosFunctionMap *)baseFunctionMap
{
	if ([delegate respondsToSelector:@selector(eidosConsoleWindowController:functionMapFromBaseMap:)])
		return [delegate eidosConsoleWindowController:self functionMapFromBaseMap:baseFunctionMap];
	else
		return baseFunctionMap;
}

- (const std::vector<const EidosMethodSignature*> *)eidosTextViewAllMethodSignatures:(EidosTextView *)eidosTextView
{
	if ([delegate respondsToSelector:@selector(eidosConsoleWindowControllerAllMethodSignatures:)])
		return [delegate eidosConsoleWindowControllerAllMethodSignatures:self];
	else
		return nullptr;
}

- (BOOL)eidosTextView:(EidosTextView *)eidosTextView tokenStringIsSpecialIdentifier:(const std::string &)token_string
{
	if ([delegate respondsToSelector:@selector(eidosConsoleWindowController:tokenStringIsSpecialIdentifier:)])
		return [delegate eidosConsoleWindowController:self tokenStringIsSpecialIdentifier:token_string];
	else
		return NO;
}

- (NSString *)eidosTextView:(EidosTextView *)eidosTextView helpTextForClickedText:(NSString *)clickedText
{
	if ([delegate respondsToSelector:@selector(eidosConsoleWindowController:helpTextForClickedText:)])
		return [delegate eidosConsoleWindowController:self helpTextForClickedText:clickedText];
	else
		return nil;
}

- (void)textViewDidChangeSelection:(NSNotification *)notification
{
	NSTextView *textView = (NSTextView *)[notification object];
	
	if (textView == outputTextView)
	{
		NSUInteger promptEnd = [outputTextView promptRangeEnd];
		NSRange selectedRange = [outputTextView selectedRange];
		
		if ((promptEnd > 0) && (selectedRange.location >= promptEnd))
		{
			NSString *outputString = [outputTextView string];
			NSString *scriptString = [outputString substringFromIndex:promptEnd];
			
			selectedRange.location -= promptEnd;
			
			[statusTextField setAttributedStringValue:[outputTextView attributedSignatureForScriptString:scriptString selection:selectedRange]];
		}
		else
		{
			[statusTextField setStringValue:@""];
		}
	}
	else if (textView == scriptTextView)
	{
		[statusTextField setAttributedStringValue:[scriptTextView attributedSignatureForScriptString:[scriptTextView string] selection:[scriptTextView selectedRange]]];
	}
}


//
//	NSWindow delegate methods
//
#pragma mark -
#pragma mark NSWindow delegate

- (void)windowWillClose:(NSNotification *)notification
{
	NSWindow *closingWindow = [notification object];
	
	if (closingWindow == scriptWindow)
	{
		// The variable browser is an inspector on our state, but we don't close it here.  In EidosScribe,
		// we are quitting at this point anyway, so it doesn't matter.  In SLiMgui, the var browser is
		// still meaningful even with our window closed, since it shows the current simulation state.
		// This may need to be revisited for other Contexts.
		
		//if ([[browserController browserWindow] isVisible])
		//	[browserController toggleBrowserVisibility:self];
		
		// Let our delegate do something; EidosScribe quits, SLiMgui toggles its console button
		if ([delegate respondsToSelector:@selector(eidosConsoleWindowControllerConsoleWindowWillClose:)])
			[delegate eidosConsoleWindowControllerConsoleWindowWillClose:self];
	}
}


//
//	NSSplitView delegate methods
//
#pragma mark -
#pragma mark NSSplitView delegate

- (BOOL)splitView:(NSSplitView *)splitView canCollapseSubview:(NSView *)subview
{
	return YES;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMaxCoordinate:(CGFloat)proposedMax ofSubviewAt:(NSInteger)dividerIndex
{
	return proposedMax - 230;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMin ofSubviewAt:(NSInteger)dividerIndex
{
	return proposedMin + 230;
}

@end

@implementation EidosConsoleWindowController (CxxAdditions)

// provides access to the symbol table of the console window, sometimes used by the Context for completion or other tasks
- (EidosSymbolTable *)symbols
{
	return global_symbols;
}

@end



































































