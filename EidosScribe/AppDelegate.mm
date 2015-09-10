//
//  AppDelegate.m
//  EidosScribe
//
//  Created by Ben Haller on 4/7/15.
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


#import "AppDelegate.h"
#import "EidosTextView.h"
#import "EidosValueWrapper.h"

#include "eidos_test.h"

#include <stdexcept>


@implementation AppDelegate

- (void)dealloc
{
	[_consoleController setDelegate:nil];
	
	[super dealloc];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	// Give Eidos a chance to warm up
	Eidos_RegisterGlobalStringsAndIDs();
	
	// Load our console window nib, which runs Eidos tests as a side effect
	[[NSBundle mainBundle] loadNibNamed:@"ConsoleWindow" owner:self topLevelObjects:NULL];
	
	// Make the script window visible
	[_consoleController showWindow];
}


//
//	Actions
//
#pragma mark -
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
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"mailto:philipp.messer@gmail.com?subject=Eidos%20Feedback"]];
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
//	EidosConsoleControllerDelegate methods
//
#pragma mark -
#pragma mark EidosConsoleControllerDelegate

- (void)appendWelcomeMessageAddendum
{
	// Run startup tests, if enabled
	RunEidosTests();
}

- (void)consoleWindowWillClose
{
	[[NSApplication sharedApplication] terminate:nil];
}

@end



































































