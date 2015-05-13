//
//  AppDelegate.m
//  SLiMscribe
//
//  Created by Ben Haller on 4/7/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
//

//	This file is part of SLiM.
//
//	SLiM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
//	the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
//
//	SLiM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License along with SLiM.  If not, see <http://www.gnu.org/licenses/>.


#import "AppDelegate.h"
#import "CocoaExtra.h"
#include "script_functions.h"
#include "script_test.h"

#include <stdexcept>


using std::string;
using std::vector;
using std::map;
using std::endl;
using std::istringstream;
using std::ostringstream;
using std::istream;
using std::ostream;


// User defaults keys
NSString *defaultsShowTokensKey = @"ShowTokens";
NSString *defaultsShowParseKey = @"ShowParse";
NSString *defaultsShowExecutionKey = @"ShowExecution";

static NSString *defaultScriptString = @"// simple neutral simulation\n\n"
										"#MUTATION TYPES\n"
										"m1 0.5 f 0.0 // neutral\n\n"
										"m2 0.5 f 0.1 // adaptive\n\n"
										"#MUTATION RATE\n"
										"1e-7\n\n"
										"#GENOMIC ELEMENT TYPES\n"
										"g1 m1 0.9 m2 0.1 // mostly neutral, some adaptive\n\n"
										"#CHROMOSOME ORGANIZATION\n"
										"g1 1 100000 // uniform chromosome of length 100 kb\n\n"
										"#RECOMBINATION RATE\n"
										"100000 1e-8\n\n"
										"#GENERATIONS\n"
										"1000\n\n"
										"#DEMOGRAPHY AND STRUCTURE\n"
										"1 P p1 500 // one population of 500 individuals\n\n"
										"#SCRIPT\n"
										"1000 { stop(\"SLiMscribe halt\"); }\n";


@implementation AppDelegate

@synthesize scriptWindow, mainSplitView, scriptTextView, outputTextView;

+ (void)initialize
{
	[[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
															 [NSNumber numberWithBool:NO], defaultsShowTokensKey,
															 [NSNumber numberWithBool:NO], defaultsShowParseKey,
															 [NSNumber numberWithBool:NO], defaultsShowExecutionKey,
															 nil]];
}

- (void)dealloc
{
	delete global_symbols;
	global_symbols = nil;
	
	[super dealloc];
}

- (void)launchSimulation
{
	delete sim;
	sim = nullptr;
	
	std::istringstream infile([defaultScriptString UTF8String]);
	
	try
	{
		sim = new SLiMSim(infile);
		
		if (sim)
			sim->RunToEnd();
	}
	catch (std::runtime_error)
	{
		// We want to catch a SLiMscript raise here, so that the simulation is frozen at the stage when scripts would normally be executed.
		std::string terminationMessage = gSLiMTermination.str();
		NSString *terminationString = nil;
		
		if (!terminationMessage.empty())
		{
			const char *cstr = terminationMessage.c_str();
			
			terminationString = [NSString stringWithUTF8String:cstr];
			
			gSLiMTermination.clear();
			gSLiMTermination.str("");
			
			//NSLog(@"%@", str);
		}
		
		if ([terminationString isEqualToString:@"ERROR (ExecuteFunctionCall): stop() called.\n"])
		{
			NSLog(@"SLiM simulation constructed and halted correctly; SLiM services are available.");
			return;
		}
		else
		{
			NSLog(@"SLiM simulation halted unexpectedly: %@", terminationString);
			// drop through to exit below
		}
	}
	
	// If we reach here, we did not get the script raise we want, which is a problem.
	NSLog(@"SLiM simulation did not halt correctly; SLiM services will not be available.");
	
	delete sim;
	sim = nullptr;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	// Instantiate a simulation
	[self launchSimulation];
	
	// Run startup tests, if enabled
	RunSLiMScriptTests();
	
	// Tell Cocoa that we can go full-screen
	[scriptWindow setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
	
	// Turn off all automatic substitution stuff, etc.; turning them off in the nib doesn't seem to actually turn them off, on 10.10.1
	[scriptTextView setAutomaticDashSubstitutionEnabled:NO];
	[scriptTextView setAutomaticDataDetectionEnabled:NO];
	[scriptTextView setAutomaticLinkDetectionEnabled:NO];
	[scriptTextView setAutomaticQuoteSubstitutionEnabled:NO];
	[scriptTextView setAutomaticSpellingCorrectionEnabled:NO];
	[scriptTextView setAutomaticTextReplacementEnabled:NO];
	[scriptTextView setContinuousSpellCheckingEnabled:NO];
	[scriptTextView setGrammarCheckingEnabled:NO];
	[scriptTextView turnOffLigatures:nil];
	
	[outputTextView setAutomaticDashSubstitutionEnabled:NO];
	[outputTextView setAutomaticDataDetectionEnabled:NO];
	[outputTextView setAutomaticLinkDetectionEnabled:NO];
	[outputTextView setAutomaticQuoteSubstitutionEnabled:NO];
	[outputTextView setAutomaticSpellingCorrectionEnabled:NO];
	[outputTextView setAutomaticTextReplacementEnabled:NO];
	[outputTextView setContinuousSpellCheckingEnabled:NO];
	[outputTextView setGrammarCheckingEnabled:NO];
	[outputTextView turnOffLigatures:nil];
	
	// Fix textview fonts and typing attributes
	[scriptTextView setFont:[NSFont fontWithName:@"Menlo" size:11.0]];
	[outputTextView setFont:[NSFont fontWithName:@"Menlo" size:11.0]];
	
	[scriptTextView setTypingAttributes:[SLiMSyntaxColoredTextView consoleTextAttributesWithColor:nil]];
	[outputTextView setTypingAttributes:[SLiMSyntaxColoredTextView consoleTextAttributesWithColor:nil]];
	
	// Fix text container insets to look a bit nicer; {0,0} by default
	[scriptTextView setTextContainerInset:NSMakeSize(0.0, 5.0)];
	[outputTextView setTextContainerInset:NSMakeSize(0.0, 5.0)];
	
	// Show a welcome message
	[outputTextView showWelcomeMessage];
	
	// And show our prompt
	[outputTextView showPrompt];
	
	[scriptWindow makeFirstResponder:outputTextView];
}

- (NSString *)_executeScriptString:(NSString *)scriptString tokenString:(NSString **)tokenString parseString:(NSString **)parseString executionString:(NSString **)executionString errorString:(NSString **)errorString addOptionalSemicolon:(BOOL)addSemicolon
{
	string script_string([scriptString UTF8String]);
	Script script(1, 1, script_string, 0);
	ScriptValue *result;
	string output;
	
	// Tokenize
	try
	{
		script.Tokenize();
		
		// Add a semicolon if needed; this allows input like "6+7" in the console
		if (addSemicolon)
			script.AddOptionalSemicolon();
		
		if (tokenString)
		{
			ostringstream token_stream;
			
			script.PrintTokens(token_stream);
			
			*tokenString = [NSString stringWithUTF8String:token_stream.str().c_str()];
		}
	}
	catch (std::runtime_error err)
	{
		*errorString = [NSString stringWithUTF8String:GetUntrimmedRaiseMessage().c_str()];
		return nil;
	}
	
	// Parse, an "interpreter block" bounded by an EOF rather than a "script block" that requires braces
	try
	{
		script.ParseInterpreterBlockToAST();
		
		if (parseString)
		{
			ostringstream parse_stream;
			
			script.PrintAST(parse_stream);
			
			*parseString = [NSString stringWithUTF8String:parse_stream.str().c_str()];
		}
	}
	catch (std::runtime_error err)
	{
		*errorString = [NSString stringWithUTF8String:GetUntrimmedRaiseMessage().c_str()];
		return nil;
	}
	
	// Interpret the parsed block
	ScriptInterpreter interpreter(script, global_symbols);		// give the interpreter the symbol table
	global_symbols = nullptr;
	
	if (sim)
		sim->InjectIntoInterpreter(interpreter);
	
	try
	{
		if (executionString)
			interpreter.SetShouldLogExecution(true);
		
		result = interpreter.EvaluateInterpreterBlock();
		output = interpreter.ExecutionOutput();
		global_symbols = interpreter.YieldSymbolTable();			// take the symbol table back
		
		if (executionString)
			*executionString = [NSString stringWithUTF8String:interpreter.ExecutionLog().c_str()];
	}
	catch (std::runtime_error err)
	{
		output = interpreter.ExecutionOutput();
		global_symbols = interpreter.YieldSymbolTable();			// take the symbol table back despite the raise
		
		*errorString = [NSString stringWithUTF8String:GetUntrimmedRaiseMessage().c_str()];
		
		return [NSString stringWithUTF8String:output.c_str()];
	}
	
	return [NSString stringWithUTF8String:output.c_str()];
}

- (void)executeScriptString:(NSString *)scriptString addOptionalSemicolon:(BOOL)addSemicolon
{
	NSTextStorage *ts = [outputTextView textStorage];
	NSString *tokenString = nil, *parseString = nil, *executionString = nil, *errorString = nil;
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	BOOL showTokens = [defaults boolForKey:defaultsShowTokensKey];
	BOOL showParse = [defaults boolForKey:defaultsShowParseKey];
	BOOL showExecution = [defaults boolForKey:defaultsShowExecutionKey];
	
	NSString *result = [self _executeScriptString:scriptString
									  tokenString:(showTokens ? &tokenString : NULL)
									  parseString:(showParse ? &parseString : NULL)
								  executionString:(showExecution ? &executionString : NULL)
									  errorString:&errorString
							 addOptionalSemicolon:addSemicolon];
	
	// make the attributed strings we will append
	NSAttributedString *outputString1 = [[NSAttributedString alloc] initWithString:@"\n" attributes:[ConsoleTextView inputAttrs]];
	NSAttributedString *outputString2 = (result ? [[NSAttributedString alloc] initWithString:result attributes:[ConsoleTextView outputAttrs]] : nil);
	NSAttributedString *errorAttrString = (errorString ? [[NSAttributedString alloc] initWithString:errorString attributes:[ConsoleTextView errorAttrs]] : nil);
	NSAttributedString *tokenAttrString = (tokenString ? [[NSAttributedString alloc] initWithString:tokenString attributes:[ConsoleTextView tokensAttrs]] : nil);
	NSAttributedString *parseAttrString = (parseString ? [[NSAttributedString alloc] initWithString:parseString attributes:[ConsoleTextView parseAttrs]] : nil);
	NSAttributedString *executionAttrString = (executionString ? [[NSAttributedString alloc] initWithString:executionString attributes:[ConsoleTextView executionAttrs]] : nil);;
	
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


//
//	Actions
//
#pragma mark Actions

- (IBAction)showAboutWindow:(id)sender
{
	[[NSBundle mainBundle] loadNibNamed:@"AboutWindow" owner:self topLevelObjects:NULL];
	
	// The window is the top-level object in this nib.  It will release itself when closed, so we will retain it on its behalf here.
	// Note that the aboutWindow and aboutWebView outlets do not get zeroed out when the about window closes; but we only use them here.
	[aboutWindow retain];
	
	// Set our version number string
	NSString *bundleVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
	NSString *versionString = [NSString stringWithFormat:@"v. %@", bundleVersion];
	
	[aboutVersionTextField setStringValue:versionString];
	
	// Now that everything is set up, show the window
	[aboutWindow makeKeyAndOrderFront:nil];
}

- (IBAction)sendFeedback:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"mailto:philipp.messer@gmail.com?subject=SLiM%20Feedback"]];
}

- (IBAction)showMesserLab:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://messerlab.org/"]];
}

- (IBAction)showBenHaller:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://www.benhaller.com/"]];
}

- (IBAction)showStickSoftware:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://www.sticksoftware.com/"]];
}

- (IBAction)checkScript:(id)sender
{
	NSLog(@"checkScript:");
}

- (IBAction)showScriptHelp:(id)sender
{
	NSTextStorage *ts = [outputTextView textStorage];
	NSAttributedString *scriptAttrString = [[NSAttributedString alloc] initWithString:@"help()" attributes:[ConsoleTextView inputAttrs]];
	NSUInteger promptEnd = [outputTextView promptRangeEnd];
	
	[ts beginEditing];
	[ts replaceCharactersInRange:NSMakeRange(promptEnd, [ts length] - promptEnd) withAttributedString:scriptAttrString];
	[ts endEditing];
	
	[outputTextView registerNewHistoryItem:@"help()"];
	[self executeScriptString:@"help()" addOptionalSemicolon:YES];
}

- (IBAction)clearOutput:(id)sender
{
	[outputTextView clearOutput];
}

- (IBAction)executeAll:(id)sender
{
	NSTextStorage *ts = [outputTextView textStorage];
	NSString *fullScriptString = [scriptTextView string];
	NSAttributedString *scriptAttrString = [[NSAttributedString alloc] initWithString:fullScriptString attributes:[ConsoleTextView inputAttrs]];
	NSUInteger promptEnd = [outputTextView promptRangeEnd];
	
	[ts beginEditing];
	[ts replaceCharactersInRange:NSMakeRange(promptEnd, [ts length] - promptEnd) withAttributedString:scriptAttrString];
	[ts endEditing];
	
	[outputTextView registerNewHistoryItem:fullScriptString];
	[self executeScriptString:fullScriptString addOptionalSemicolon:NO];
}

- (IBAction)executeSelection:(id)sender
{
	NSTextStorage *ts = [outputTextView textStorage];
	NSString *fullScriptString = [scriptTextView string];
	NSUInteger scriptLength = [fullScriptString length];
	NSRange selectedRange = [scriptTextView selectedRange];
	NSCharacterSet *newlineChars = [NSCharacterSet newlineCharacterSet];
	NSRange executionRange = selectedRange;	// indices of the first and last characters to execute
	
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
	
	// now execute the range we have found
	if (executionRange.length > 0)
	{
		NSString *scriptString = [fullScriptString substringWithRange:executionRange];
		NSAttributedString *scriptAttrString = [[NSAttributedString alloc] initWithString:scriptString attributes:[ConsoleTextView inputAttrs]];
		NSUInteger promptEnd = [outputTextView promptRangeEnd];
		
		[ts beginEditing];
		[ts replaceCharactersInRange:NSMakeRange(promptEnd, [ts length] - promptEnd) withAttributedString:scriptAttrString];
		[ts endEditing];
		
		[outputTextView registerNewHistoryItem:scriptString];
		[self executeScriptString:scriptString addOptionalSemicolon:NO];
	}
	else
	{
		NSBeep();
	}
}


//
//	NSTextView delegate methods
//
#pragma mark NSTextView delegate

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

- (void)executeConsoleInput:(ConsoleTextView *)textView
{
	if (textView == outputTextView)
	{
		NSString *outputString = [outputTextView string];
		NSString *scriptString = [outputString substringFromIndex:[outputTextView promptRangeEnd]];
		
		[outputTextView registerNewHistoryItem:scriptString];
		[self executeScriptString:scriptString addOptionalSemicolon:YES];
	}
}


//
//	NSWindow delegate methods
//
#pragma mark NSWindow delegate

- (void)windowWillClose:(NSNotification *)notification
{
	NSWindow *closingWindow = [notification object];
	
	if (closingWindow == scriptWindow)
	{
		[scriptWindow setDelegate:nil];
		[scriptWindow release];
		scriptWindow = nil;
		
		[[NSApplication sharedApplication] terminate:nil];
	}
}


//
//	NSSplitView delegate methods
//
#pragma mark NSSplitView delegate

- (BOOL)splitView:(NSSplitView *)splitView canCollapseSubview:(NSView *)subview
{
	return YES;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMaxCoordinate:(CGFloat)proposedMax ofSubviewAt:(NSInteger)dividerIndex
{
	return proposedMax - 200;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMin ofSubviewAt:(NSInteger)dividerIndex
{
	return proposedMin + 200;
}

@end


































































