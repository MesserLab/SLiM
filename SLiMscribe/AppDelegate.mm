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
#import "SLiMScriptTextView.h"
#import "ScriptValueWrapper.h"

#include "script_test.h"

#include <stdexcept>


// User defaults keys
NSString *defaultsSLiMScriptKey = @"SLiMScript";

static NSString *defaultScriptString = @"// set up a simple neutral simulation\n"
										"0 {\n"
										"	setMutationRate0(1e-7);\n"
										"	setGenerationRange0(10000);\n"
										"	\n"
										"	// m1 mutation type: neutral\n"
										"	addMutationType0(1, 0.5, \"f\", 0.0);\n"
										"	\n"
										"	// g1 genomic element type: uses m1 for all mutations\n"
										"	addGenomicElementType0(1, 1, 1.0);\n"
										"	\n"
										"	// uniform chromosome of length 100 kb with uniform recombination\n"
										"	addGenomicElement0(1, 0, 99999);\n"
										"	addRecombinationIntervals0(99999, 1e-8);\n"
										"}\n"
										"\n"
										"// create a population of 100 individuals\n"
										"1 {\n"
										"	sim.addSubpop(1, 100);\n"
										"}\n"
										"\n"
										"// halt event, for SLiMscribe\n"
										"\"s1000\" 1000 { stop(\"SLiMscribe halt\"); }\n";


@implementation AppDelegate

@synthesize launchSLiMScriptTextView;

+ (void)initialize
{
	[[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys: defaultScriptString, defaultsSLiMScriptKey, nil]];
}

- (void)dealloc
{
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
		std::string &&terminationMessage = gSLiMTermination.str();
		
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
	
	// Fix textview fonts and typing attributes
	[launchSLiMScriptTextView setFont:[NSFont fontWithName:@"Menlo" size:11.0]];
	[launchSLiMScriptTextView setTypingAttributes:[SLiMScriptTextView consoleTextAttributesWithColor:nil]];
	[launchSLiMScriptTextView syntaxColorForSLiMScript];
	
	// Load our console window nib
	[[NSBundle mainBundle] loadNibNamed:@"ConsoleWindow" owner:self topLevelObjects:NULL];
	
	// Make the script window visible
	[_consoleController showWindow];
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


//
//	ConsoleWindowController delegate methods
//
#pragma mark ConsoleWindowController delegate

- (void)appendWelcomeMessageAddendum
{
	// Get our launch script string and instantiate a simulation
	NSString *launchScript = [[NSUserDefaults standardUserDefaults] stringForKey:defaultsSLiMScriptKey];
	NSString *simErrorString = [self launchSimulationWithScript:launchScript];
	
	BOOL simLaunchSuccess = (sim != nullptr);
	ConsoleTextView *textView = [_consoleController textView];
	NSTextStorage *ts = [textView textStorage];
	NSDictionary *outputAttrs = [ConsoleTextView outputAttrs];
	NSDictionary *errorAttrs = [ConsoleTextView errorAttrs];
	NSAttributedString *launchString;
	NSAttributedString *errorString = nil;
	NSAttributedString *dividerString = [[NSAttributedString alloc] initWithString:@"\n---------------------------------------------------------\n\n" attributes:outputAttrs];
	
	if (simLaunchSuccess)
		launchString = [[NSAttributedString alloc] initWithString:@"SLiM launched and halted; SLiM services are available.\n" attributes:outputAttrs];
	else
		launchString = [[NSAttributedString alloc] initWithString:@"SLiM failed to launch and halt:\n\n" attributes:errorAttrs];
	
	if (!simLaunchSuccess && simErrorString)
		errorString = [[NSAttributedString alloc] initWithString:simErrorString attributes:errorAttrs];
	
	[ts beginEditing];
	[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:launchString];
	if (errorString)
		[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:errorString];
	[ts replaceCharactersInRange:NSMakeRange([ts length], 0) withAttributedString:dividerString];
	[ts endEditing];
	
	[launchString release];
	[errorString release];
	[dividerString release];
	
	// Run startup tests, if enabled
	[self willExecuteScript];
	RunSLiMScriptTests();
	[self didExecuteScript];
}

- (void)injectIntoInterpreter:(ScriptInterpreter *)interpreter
{
	if (sim)
		sim->InjectIntoInterpreter(*interpreter, nullptr);
}

- (std::vector<FunctionSignature*> *)injectedFunctionSignatures
{
	if (sim)
		return sim->InjectedFunctionSignatures();
	
	return nullptr;
}

- (void)checkScriptDidSucceed:(BOOL)succeeded
{
	[[NSSound soundNamed:(succeeded ? @"Bottle" : @"Ping")] play];
}

- (void)willExecuteScript
{
}

- (void)didExecuteScript
{
}

- (void)consoleWindowWillClose
{
	[[NSApplication sharedApplication] terminate:nil];
}


//
//	NSTextView delegate methods
//
#pragma mark NSTextView delegate

- (void)textDidChange:(NSNotification *)notification
{
	NSTextView *textView = (NSTextView *)[notification object];
	
	// Syntax color the input file string and save it to our defaults
	if (textView == launchSLiMScriptTextView)
	{
		NSString *scriptString = [launchSLiMScriptTextView string];
		
		[[NSUserDefaults standardUserDefaults] setObject:scriptString forKey:defaultsSLiMScriptKey];
		
		[launchSLiMScriptTextView syntaxColorForSLiMScript];
	}
}

@end



































































