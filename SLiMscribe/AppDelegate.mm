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
#import "ScriptValueWrapper.h"

#include "script_functions.h"
#include "script_functionsignature.h"
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
NSString *defaultsSLiMScriptKey = @"SLiMScript";

static NSString *defaultScriptString = @"// simple neutral simulation\n\n"
										"#MUTATION TYPES\n"
										"m1 0.5 f 0.0 // neutral\n\n"
										"#MUTATION RATE\n"
										"1e-7\n\n"
										"#GENOMIC ELEMENT TYPES\n"
										"g1 m1 1.0 // only one type comprising the neutral mutations\n\n"
										"#CHROMOSOME ORGANIZATION\n"
										"g1 1 100000 // uniform chromosome of length 100 kb\n\n"
										"#RECOMBINATION RATE\n"
										"100000 1e-8\n\n"
										"#GENERATIONS\n"
										"1000\n\n"
										"#DEMOGRAPHY AND STRUCTURE\n"
										"1 P p1 100 // one population of 100 individuals\n\n"
										"#SCRIPT\n"
										"1000 { stop(\"SLiMscribe halt\"); }\n";


@implementation AppDelegate

@synthesize scriptWindow, mainSplitView, scriptTextView, outputTextView, launchSLiMScriptTextView;

+ (void)initialize
{
	[[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
															 [NSNumber numberWithBool:NO], defaultsShowTokensKey,
															 [NSNumber numberWithBool:NO], defaultsShowParseKey,
															 [NSNumber numberWithBool:NO], defaultsShowExecutionKey,
															 defaultScriptString, defaultsSLiMScriptKey,
															 nil]];
}

- (void)dealloc
{
	delete global_symbols;
	global_symbols = nil;
	
	[browserWrappers release];
	browserWrappers = nil;
	
	[super dealloc];
}

- (NSString *)launchSimulationWithScript:(NSString *)scriptString
{
	NSString *terminationString = nil;
	
	delete sim;
	sim = nullptr;
	
	std::istringstream infile([scriptString UTF8String]);
	
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
			return terminationString;
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
	
	return terminationString;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	// Get our launch script string and instantiate a simulation
	NSString *launchScript = [[NSUserDefaults standardUserDefaults] stringForKey:defaultsSLiMScriptKey];
	
	[launchSLiMScriptTextView setString:launchScript];
	
	NSString *terminationString = [self launchSimulationWithScript:launchScript];
	
	// Reload symbols in outline view
	[browserWrappers removeAllObjects];
	[_browserOutline reloadData];
	
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
	[launchSLiMScriptTextView setFont:[NSFont fontWithName:@"Menlo" size:11.0]];
	
	[scriptTextView setTypingAttributes:[SLiMSyntaxColoredTextView consoleTextAttributesWithColor:nil]];
	[outputTextView setTypingAttributes:[SLiMSyntaxColoredTextView consoleTextAttributesWithColor:nil]];
	[launchSLiMScriptTextView setTypingAttributes:[SLiMSyntaxColoredTextView consoleTextAttributesWithColor:nil]];
	
	[launchSLiMScriptTextView syntaxColorForSLiMInput];
	
	// Fix text container insets to look a bit nicer; {0,0} by default
	[scriptTextView setTextContainerInset:NSMakeSize(0.0, 5.0)];
	[outputTextView setTextContainerInset:NSMakeSize(0.0, 5.0)];
	
	// Show a welcome message
	[outputTextView showWelcomeMessage];
	[outputTextView showSimulationLaunchSuccess:(sim != nullptr) errorMessage:terminationString];
	
	// And show our prompt
	[outputTextView showPrompt];
	
	// Execute a null statement to get our symbols set up, for code completion etc.
	NSString *errorString = nil;
	
	[self _executeScriptString:@";" tokenString:NULL parseString:NULL executionString:NULL errorString:&errorString addOptionalSemicolon:NO];
	
	if (errorString)
		NSLog(@"Error in initial SLiMscript launch: %@", errorString);
	
	// OK, everything is set up, so make the script window visible
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
		
		// reload outline view to show new global symbols, in case they have changed
		[browserWrappers removeAllObjects];
		[_browserOutline reloadData];
		
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

- (IBAction)toggleBrowserVisibility:(id)sender
{
	// FIXME keep button highlighted to show toggle state
	if ([_browserWindow isVisible])
		[_browserWindow orderOut:nil];
	else
		[_browserWindow makeKeyAndOrderFront:nil];
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

- (void)textDidChange:(NSNotification *)notification
{
	NSTextView *textView = (NSTextView *)[notification object];
	
	// Syntax color the script string
	if (textView == scriptTextView)
	{
		[scriptTextView syntaxColorForSLiMScript];
	}
	
	// Syntax color the input file string and save it to our defaults
	if (textView == launchSLiMScriptTextView)
	{
		NSString *scriptString = [launchSLiMScriptTextView string];
		
		[[NSUserDefaults standardUserDefaults] setObject:scriptString forKey:defaultsSLiMScriptKey];
		
		[launchSLiMScriptTextView syntaxColorForSLiMInput];
	}
}

- (NSMutableArray *)globalCompletionsIncludingStatements:(BOOL)includeStatements
{
	NSMutableArray *globals = [NSMutableArray array];
	
	// First, a sorted list of globals
	for (std::string symbol_name : global_symbols->ReadOnlySymbols())
		[globals addObject:[NSString stringWithUTF8String:symbol_name.c_str()]];
	
	for (std::string symbol_name : global_symbols->ReadWriteSymbols())
		[globals addObject:[NSString stringWithUTF8String:symbol_name.c_str()]];
	
	[globals sortUsingSelector:@selector(compare:)];
	
	// Next, a sorted list of functions, with () appended
	for (const FunctionSignature *sig : ScriptInterpreter::BuiltInFunctions())
	{
		NSString *functionName = [NSString stringWithUTF8String:sig->function_name_.c_str()];
		
		[globals addObject:[functionName stringByAppendingString:@"()"]];
	}
	
	// Finally, provide language keywords as an option if requested
	if (includeStatements)
	{
		[globals addObject:@"break"];
		[globals addObject:@"do"];
		[globals addObject:@"else"];
		[globals addObject:@"for"];
		[globals addObject:@"if"];
		[globals addObject:@"in"];
		[globals addObject:@"next"];
		[globals addObject:@"return"];
		[globals addObject:@"while"];
	}
	
	return globals;
}

- (NSMutableArray *)completionsForKeyPathEndingInTokenIndex:(int)lastDotTokenIndex ofTokenStream:(const std::vector<ScriptToken *> &)tokens
{
	ScriptToken *token = tokens[lastDotTokenIndex];
	TokenType token_type = token->token_type_;
	
	if (token_type != TokenType::kTokenDot)
	{
		NSLog(@"***** completionsForKeyPathEndingInTokenIndex... called for non-kTokenDot token!");
		return nil;
	}
	
	// OK, we've got a key path ending in a dot, and we want to return a list of completions that would work for that key path.
	// We'll trace backward, adding identifiers to a vector to build up the chain of references.  If we hit a bracket, we'll
	// skip back over everything inside it, since subsetting does not change the type; we just need to balance brackets.  If we
	// hit a parenthesis, we give up.  If we hit other things – a semicolon, a comma, a brace – that terminates the key path chain.
	vector<string> identifiers;
	int bracketCount = 0;
	BOOL lastTokenWasDot = YES;
	
	for (int tokenIndex = lastDotTokenIndex - 1; tokenIndex >= 0; --tokenIndex)
	{
		token = tokens[tokenIndex];
		token_type = token->token_type_;
		
		// skip backward over whitespace and comments; they make no difference to us
		if ((token_type == TokenType::kTokenWhitespace) || (token_type == TokenType::kTokenComment))
			continue;
		
		if (bracketCount)
		{
			// If we're inside a bracketed stretch, all we do is balance brackets and run backward.  We don't even clear lastTokenWasDot,
			// because a []. sequence puts us in the same situation as having just seen a dot – we're still waiting for an identifier.
			if (token_type == TokenType::kTokenRBracket)
			{
				bracketCount++;
				continue;
			}
			if (token_type == TokenType::kTokenLBracket)
			{
				bracketCount--;
				continue;
			}
			
			// Check for tokens that simply make no sense, and bail
			if ((token_type == TokenType::kTokenLBrace) || (token_type == TokenType::kTokenRBrace) || (token_type == TokenType::kTokenSemicolon) || (token_type >= TokenType::kFirstIdentifierLikeToken))
				return nil;
			
			continue;
		}
		
		if (!lastTokenWasDot)
		{
			// We just saw an identifier, so the only thing that can continue the key path is a dot
			if (token_type == TokenType::kTokenDot)
			{
				lastTokenWasDot = YES;
				continue;
			}
			
			// the key path has terminated at some non-key-path token, so we're done tracing it
			break;
		}
		
		// OK, the last token was a dot (or a subset preceding a dot).  We're looking for an identifier, but we're willing
		// to get distracted by a subset sequence, since that does not change the type.  Anything else does not make sense.
		// (A method call or function call is possible, actually, but we're not presently equipped to handle them.  The problem
		// is that we don't want to actually call the method/function to get a ScriptValue*, because such calls are heavyweight
		// and can have side effects, but without calling the method/function we have no way to get an instance of the type
		// that it would return.  We need the concept of Class objects, but C++ does not do that.  I miss Objective-C.  I'm not
		// sure how to solve this, really; it would require us to have some kind of artificial Class-object-like thing that
		// would know the properties and methods for a given ScriptObjectElement class.  Big distortion to the architecture.
		// So for now, we just don't trace back through method/function calls, which sucks.  FIXME)
		if (token_type == TokenType::kTokenIdentifier)
		{
			lastTokenWasDot = NO;
			identifiers.push_back(token->token_string_);
			continue;
		}
		else if (token_type == TokenType::kTokenRBracket)
		{
			bracketCount++;
			continue;
		}
		
		// This makes no sense, so bail
		return nil;
	}
	
	// If we were in the middle of tracing the key path when the loop ended, then something is wrong, bail.
	if (lastTokenWasDot || bracketCount)
		return nil;
	
	// OK, we've got an identifier chain in identifiers, in reverse order.  We want to start at the beginning of the key path,
	// and follow it forward through the properties in the chain to arrive at the final type.
	int key_path_index = (int)identifiers.size() - 1;
	string identifier_name = identifiers[key_path_index];
	
	ScriptValue *key_path_value = (global_symbols ? global_symbols->GetValueOrNullForSymbol(identifier_name) : nullptr);
	
	if (!key_path_value)
		return nil;			// unknown symbol at the root, so we have no idea what's going on
	if (key_path_value->Type() != ScriptValueType::kValueObject)
	{
		if (!key_path_value->InSymbolTable()) delete key_path_value;
		return nil;			// the root symbol is not an object, so it should not have a key path off of it; bail
	}
	
	while (--key_path_index >= 0)
	{
		identifier_name = identifiers[key_path_index];
		
		ScriptValue *property_value = ((ScriptValue_Object *)key_path_value)->GetRepresentativeValueOrNullForMemberOfElements(identifier_name);
		
		if (!key_path_value->InSymbolTable()) delete key_path_value;
		key_path_value = property_value;
		
		if (!key_path_value)
			return nil;			// unknown symbol at the root, so we have no idea what's going on
		if (key_path_value->Type() != ScriptValueType::kValueObject)
		{
			if (!key_path_value->InSymbolTable()) delete key_path_value;
			return nil;			// the root symbol is not an object, so it should not have a key path off of it; bail
		}
	}
	
	// OK, we've now got a ScriptValue object that represents the end of the line; the final dot is off of this object.
	// So we want to extract all of its properties and methods, and return them all as candidates.
	NSMutableArray *candidates = [NSMutableArray array];
	ScriptValue_Object *terminus = ((ScriptValue_Object *)key_path_value);
	
	// First, a sorted list of globals
	for (std::string symbol_name : terminus->ReadOnlyMembersOfElements())
		[candidates addObject:[NSString stringWithUTF8String:symbol_name.c_str()]];
	
	for (std::string symbol_name : terminus->ReadWriteMembersOfElements())
		[candidates addObject:[NSString stringWithUTF8String:symbol_name.c_str()]];
	
	[candidates sortUsingSelector:@selector(compare:)];
	
	// Next, a sorted list of functions, with () appended
	for (string method_name : terminus->MethodsOfElements())
	{
		NSString *methodName = [NSString stringWithUTF8String:method_name.c_str()];
		
		[candidates addObject:[methodName stringByAppendingString:@"()"]];
	}
	
	// Dispose of our terminus
	if (!terminus->InSymbolTable()) delete terminus;
	
	return candidates;
}

- (NSArray *)completionsForTokenStream:(const std::vector<ScriptToken *> &)tokens index:(int)lastTokenIndex canExtend:(BOOL)canExtend
{
	// What completions we offer depends on the token stream
	ScriptToken *token = tokens[lastTokenIndex];
	TokenType token_type = token->token_type_;
	
	switch (token_type)
	{
		case TokenType::kTokenNone:
		case TokenType::kTokenEOF:
		case TokenType::kTokenWhitespace:
		case TokenType::kTokenComment:
		case TokenType::kTokenInterpreterBlock:
		case TokenType::kFirstIdentifierLikeToken:
			// These should never be hit
			return nil;
			
		case TokenType::kTokenIdentifier:
		case TokenType::kTokenIf:
		case TokenType::kTokenElse:
		case TokenType::kTokenDo:
		case TokenType::kTokenWhile:
		case TokenType::kTokenFor:
		case TokenType::kTokenIn:
		case TokenType::kTokenNext:
		case TokenType::kTokenBreak:
		case TokenType::kTokenReturn:
			if (canExtend)
			{
				NSMutableArray *completions = nil;
				
				// This is the tricky case, because the identifier we're extending could be the end of a key path like foo.bar[5:8].ba...
				// We need to move backwards from the current token until we find or fail to find a dot token; if we see a dot we're in
				// a key path, otherwise we're in the global context and should filter from those candidates
				for (int previousTokenIndex = lastTokenIndex - 1; previousTokenIndex >= 0; --previousTokenIndex)
				{
					ScriptToken *previous_token = tokens[previousTokenIndex];
					TokenType previous_token_type = previous_token->token_type_;
					
					// if the token we're on is skippable, continue backwards
					if ((previous_token_type == TokenType::kTokenWhitespace) || (previous_token_type == TokenType::kTokenComment))
						continue;
					
					// if the token we're on is a dot, we are indeed at the end of a key path, and can fetch the completions for it
					if (previous_token_type == TokenType::kTokenDot)
					{
						completions = [self completionsForKeyPathEndingInTokenIndex:previousTokenIndex ofTokenStream:tokens];
						break;
					}
					
					// if we see a semicolon or brace, we are in a completely global context
					if ((previous_token_type == TokenType::kTokenSemicolon) || (previous_token_type == TokenType::kTokenLBrace) || (previous_token_type == TokenType::kTokenRBrace))
					{
						completions = [self globalCompletionsIncludingStatements:YES];
						break;
					}
					
					// if we see any other token, we are not in a key path; let's assume we're following an operator
					completions = [self globalCompletionsIncludingStatements:NO];
					break;
				}
				
				// If we ran out of tokens, we're at the beginning of the file and so in the global context
				if (!completions)
					completions = [self globalCompletionsIncludingStatements:YES];
				
				// Now we have an array of possible completions; we just need to remove those that don't start with our existing prefix
				NSString *baseString = [NSString stringWithUTF8String:token->token_string_.c_str()];
				
				for (int completionIndex = (int)[completions count] - 1; completionIndex >= 0; --completionIndex)
				{
					if (![[completions objectAtIndex:completionIndex] hasPrefix:baseString])
						[completions removeObjectAtIndex:completionIndex];
				}
				
				return completions;
			}
			
			// If the previous token was an identifier and we can't extend it, the next thing probably needs to be an operator or something
			return nil;
			
		case TokenType::kTokenNumber:
		case TokenType::kTokenString:
		case TokenType::kTokenRParen:
		case TokenType::kTokenRBracket:
			// We don't have anything to suggest after such tokens; the next thing will need to be an operator, semicolon, etc.
			return nil;
			
		case TokenType::kTokenDot:
			// This is the other tricky case, because we're being asked to extend a key path like foo.bar[5:8].
			return [self completionsForKeyPathEndingInTokenIndex:lastTokenIndex ofTokenStream:tokens];
			
		case TokenType::kTokenSemicolon:
		case TokenType::kTokenLBrace:
		case TokenType::kTokenRBrace:
			// We are in the global context and anything goes, including a new statement
			return [self globalCompletionsIncludingStatements:YES];
			
		case TokenType::kTokenColon:
		case TokenType::kTokenComma:
		case TokenType::kTokenLParen:
		case TokenType::kTokenLBracket:
		case TokenType::kTokenPlus:
		case TokenType::kTokenMinus:
		case TokenType::kTokenMod:
		case TokenType::kTokenMult:
		case TokenType::kTokenExp:
		case TokenType::kTokenAnd:
		case TokenType::kTokenOr:
		case TokenType::kTokenDiv:
		case TokenType::kTokenAssign:
		case TokenType::kTokenEq:
		case TokenType::kTokenLt:
		case TokenType::kTokenLtEq:
		case TokenType::kTokenGt:
		case TokenType::kTokenGtEq:
		case TokenType::kTokenNot:
		case TokenType::kTokenNotEq:
			// We are following an operator, so globals are OK but new statements are not
			return [self globalCompletionsIncludingStatements:NO];
	}
	
	return nil;
}

// one funnel for all completion work, since we use the same pattern to answer both questions...
- (void)_completionHandlerForTextView:(NSTextView *)textView rangeForCompletion:(NSRange *)baseRange completions:(NSArray **)completions
{
	if ((textView == scriptTextView) || (textView == outputTextView))
	{
		NSString *scriptString = [textView string];
		NSRange selection = [textView selectedRange];	// ignore charRange and work from the selection
		NSUInteger rangeOffset = 0;
		
		// correct the script string to have only what is entered after the prompt
		if (textView == outputTextView)
		{
			rangeOffset = [outputTextView promptRangeEnd];
			scriptString = [scriptString substringFromIndex:rangeOffset];
			selection.location -= rangeOffset;
			selection.length -= rangeOffset;
		}
		
		NSUInteger selStart = selection.location;
		
		if (selStart != NSNotFound)
		{
			// Get the substring up to the start of the selection; that is the range relevant for completion
			NSString *scriptSubstring = [scriptString substringToIndex:selStart];
			std::string script_string([scriptSubstring UTF8String]);
			Script script(1, 1, script_string, 0);
			
			// Tokenize
			try
			{
				script.Tokenize(true);	// keep nonsignificant tokens - whitespace and comments
			}
			catch (std::runtime_error err)
			{
				// if we get a raise, we just use as many tokens as we got
			}
			
			auto tokens = script.Tokens();
			int lastTokenIndex = (int)tokens.size() - 1;
			BOOL endedCleanly = NO, lastTokenInterrupted = NO;
			
			// if we ended with an EOF, that means we did not have a raise and there should be no untokenizable range at the end
			if ((lastTokenIndex >= 0) && (tokens[lastTokenIndex]->token_type_ == TokenType::kTokenEOF))
			{
				--lastTokenIndex;
				endedCleanly = YES;
			}
			
			// if we ended with whitespace or a comment, the previous token cannot be extended
			while (lastTokenIndex >= 0) {
				ScriptToken *token = tokens[lastTokenIndex];
				
				if ((token->token_type_ != TokenType::kTokenWhitespace) && (token->token_type_ != TokenType::kTokenComment))
					break;
				
				--lastTokenIndex;
				lastTokenInterrupted = YES;
			}
			
			// now diagnose what range we want to use as a basis for completion
			if (!endedCleanly)
			{
				// the selection is at the end of an untokenizable range; we might be in the middle of a string or a comment,
				// or there might be a tokenization error upstream of us.  let's not try to guess what the situation is.
				if (baseRange) *baseRange = NSMakeRange(NSNotFound, 0);
				if (completions) *completions = nil;
				return;
			}
			else if (lastTokenInterrupted)
			{
				if (lastTokenIndex < 0)
				{
					// We're at the end of nothing but initial whitespace and comments; offer insertion-point completions
					if (baseRange) *baseRange = NSMakeRange(selection.location + rangeOffset, 0);
					if (completions) *completions = [self globalCompletionsIncludingStatements:YES];
					return;
				}
				
				ScriptToken *token = tokens[lastTokenIndex];
				TokenType token_type = token->token_type_;
				
				// the last token cannot be extended, so if the last token is something an identifier can follow, like an
				// operator, then we can offer completions at the insertion point based on that, otherwise punt.
				if ((token_type == TokenType::kTokenNumber) || (token_type == TokenType::kTokenString) || (token_type == TokenType::kTokenRParen) || (token_type == TokenType::kTokenRBracket) || (token_type == TokenType::kTokenIdentifier) || (token_type == TokenType::kTokenIf) || (token_type == TokenType::kTokenWhile) || (token_type == TokenType::kTokenFor) || (token_type == TokenType::kTokenNext) || (token_type == TokenType::kTokenBreak) || (token_type == TokenType::kTokenReturn))
				{
					if (baseRange) *baseRange = NSMakeRange(NSNotFound, 0);
					if (completions) *completions = nil;
					return;
				}
				
				if (baseRange) *baseRange = NSMakeRange(selection.location + rangeOffset, 0);
				if (completions) *completions = [self completionsForTokenStream:tokens index:lastTokenIndex canExtend:NO];
				return;
			}
			else
			{
				if (lastTokenIndex < 0)
				{
					// We're at the very beginning of the script; offer insertion-point completions
					if (baseRange) *baseRange = NSMakeRange(selection.location + rangeOffset, 0);
					if (completions) *completions = [self globalCompletionsIncludingStatements:YES];
					return;
				}
				
				// the last token was not interrupted, so we can offer completions of it if we want to.
				ScriptToken *token = tokens[lastTokenIndex];
				NSRange tokenRange = NSMakeRange(token->token_start_, token->token_end_ - token->token_start_ + 1);
				
				if (token->token_type_ >= TokenType::kTokenIdentifier)
				{
					if (baseRange) *baseRange = NSMakeRange(tokenRange.location + rangeOffset, tokenRange.length);
					if (completions) *completions = [self completionsForTokenStream:tokens index:lastTokenIndex canExtend:YES];
					return;
				}
				
				if ((token->token_type_ == TokenType::kTokenNumber) || (token->token_type_ == TokenType::kTokenString) || (token->token_type_ == TokenType::kTokenRParen) || (token->token_type_ == TokenType::kTokenRBracket))
				{
					if (baseRange) *baseRange = NSMakeRange(NSNotFound, 0);
					if (completions) *completions = nil;
					return;
				}
				
				if (baseRange) *baseRange = NSMakeRange(selection.location + rangeOffset, 0);
				if (completions) *completions = [self completionsForTokenStream:tokens index:lastTokenIndex canExtend:NO];
				return;
			}
		}
	}
}

- (NSArray *)textView:(NSTextView *)textView completions:(NSArray *)words forPartialWordRange:(NSRange)charRange indexOfSelectedItem:(NSInteger *)index
{
	if ((textView == scriptTextView) || (textView == outputTextView))
	{
		NSArray *completions = nil;
		
		[self _completionHandlerForTextView:textView rangeForCompletion:NULL completions:&completions];
		
		return completions;
	}
	
	return words;
}

- (NSRange)textView:(NSTextView *)textView rangeForUserCompletion:(NSRange)suggestedRange
{
	if ((textView == scriptTextView) || (textView == outputTextView))
	{
		NSRange baseRange = NSMakeRange(NSNotFound, 0);
		
		[self _completionHandlerForTextView:textView rangeForCompletion:&baseRange completions:NULL];
		
		return baseRange;
	}
	
	return suggestedRange;
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
	else if (closingWindow == _browserWindow)
	{
		[_browserWindowButton setState:NSOffState];
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
	return proposedMax - 230;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMin ofSubviewAt:(NSInteger)dividerIndex
{
	return proposedMin + 230;
}


//
//	NSOutlineView datasource / delegate methods
//
#pragma mark NSOutlineView delegate

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
	if (global_symbols)
	{
		if (item == nil)
		{
			std::vector<std::string> readOnlySymbols = global_symbols->ReadOnlySymbols();
			std::vector<std::string> readWriteSymbols = global_symbols->ReadWriteSymbols();
			
			return (readOnlySymbols.size() + readWriteSymbols.size());
		}
		else
		{
			ScriptValueWrapper *wrapper = (ScriptValueWrapper *)item;
			ScriptValue_Object *value = (ScriptValue_Object *)(wrapper->wrappedValue);
			std::vector<std::string> readOnlySymbols = value->ReadOnlyMembersOfElements();
			std::vector<std::string> readWriteSymbols = value->ReadWriteMembersOfElements();
			
			return (readOnlySymbols.size() + readWriteSymbols.size());
		}
	}
	
	return 0;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
	if (global_symbols)
	{
		if (!browserWrappers)
			browserWrappers = [NSMutableArray new];
		
		if (item == nil)
		{
			std::vector<std::string> readOnlySymbols = global_symbols->ReadOnlySymbols();
			std::vector<std::string> readWriteSymbols = global_symbols->ReadWriteSymbols();
			
			if (index < readOnlySymbols.size())
			{
				std::string symbolName = readOnlySymbols[index];
				ScriptValue *symbolValue = global_symbols->GetValueForSymbol(symbolName);
				NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
				ScriptValueWrapper *wrapper = [ScriptValueWrapper wrapperForName:symbolObjcName value:symbolValue];
				
				[browserWrappers addObject:wrapper];
				
				return wrapper;
			}
			else
			{
				std::string symbolName = readWriteSymbols[index - readOnlySymbols.size()];
				ScriptValue *symbolValue = global_symbols->GetValueForSymbol(symbolName);
				NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
				ScriptValueWrapper *wrapper = [ScriptValueWrapper wrapperForName:symbolObjcName value:symbolValue];
				
				[browserWrappers addObject:wrapper];
				
				return wrapper;
			}
		}
		else
		{
			ScriptValueWrapper *wrapper = (ScriptValueWrapper *)item;
			ScriptValue_Object *value = (ScriptValue_Object *)(wrapper->wrappedValue);
			std::vector<std::string> readOnlySymbols = value->ReadOnlyMembersOfElements();
			std::vector<std::string> readWriteSymbols = value->ReadWriteMembersOfElements();
			
			if (index < readOnlySymbols.size())
			{
				std::string symbolName = readOnlySymbols[index];
				ScriptValue *symbolValue = value->GetValueForMemberOfElements(symbolName);
				NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
				ScriptValueWrapper *childWrapper = [ScriptValueWrapper wrapperForName:symbolObjcName value:symbolValue];
				
				[browserWrappers addObject:childWrapper];
				
				return wrapper;
			}
			else
			{
				std::string symbolName = readWriteSymbols[index - readOnlySymbols.size()];
				ScriptValue *symbolValue = value->GetValueForMemberOfElements(symbolName);
				NSString *symbolObjcName = [NSString stringWithUTF8String:symbolName.c_str()];
				ScriptValueWrapper *childWrapper = [ScriptValueWrapper wrapperForName:symbolObjcName value:symbolValue];
				
				[browserWrappers addObject:childWrapper];
				
				return wrapper;
			}
		}
	}
	
	return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
	if ([item isKindOfClass:[ScriptValueWrapper class]])
	{
		ScriptValueWrapper *wrapper = (ScriptValueWrapper *)item;
		ScriptValue *value = wrapper->wrappedValue;
		
		if (value->Type() == ScriptValueType::kValueObject)
		{
			return YES;
		}
	}
	
	return NO;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
	if ([item isKindOfClass:[ScriptValueWrapper class]])
	{
		ScriptValueWrapper *wrapper = (ScriptValueWrapper *)item;
		
		return wrapper->wrappedName;
	}
	
	return nil;
}

@end



































































