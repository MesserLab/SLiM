//
//  AppDelegate.m
//  SLiMgui
//
//  Created by Ben Haller on 1/20/15.
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
#import "SLiMWindowController.h"
#import "CocoaExtra.h"
#import <WebKit/WebKit.h>


// User defaults keys
NSString *defaultsLaunchActionKey = @"LaunchAction";
NSString *defaultsSyntaxHighlightScriptKey = @"SyntaxHighlightScript";
NSString *defaultsSyntaxHighlightOutputKey = @"SyntaxHighlightOutput";
NSString *defaultsPlaySoundParseSuccessKey = @"PlaySoundParseSuccess";
NSString *defaultsPlaySoundParseFailureKey = @"PlaySoundParseFailure";

NSString *defaultsSuppressScriptCheckSuccessPanelKey = @"SuppressScriptCheckSuccessPanel";


@implementation AppDelegate

+ (void)initialize
{
	[[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
															 [NSNumber numberWithInt:1], defaultsLaunchActionKey,
															 [NSNumber numberWithBool:YES], defaultsSyntaxHighlightScriptKey,
															 [NSNumber numberWithBool:YES], defaultsSyntaxHighlightOutputKey,
															 [NSNumber numberWithBool:YES], defaultsPlaySoundParseSuccessKey,
															 [NSNumber numberWithBool:YES], defaultsPlaySoundParseFailureKey,
															 nil]];
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
	// Poke SLiMDocumentController so it gets set up before NSDocumentController gets in.  Note we don't need to keep a
	// reference to this, because AppKit will return it to us as +[NSDocumentController sharedDocumentController].
	[[SLiMDocumentController alloc] init];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	switch ([[NSUserDefaults standardUserDefaults] integerForKey:defaultsLaunchActionKey])
	{
		case 0:	// do nothing
			break;
		case 1:	// new script window
			[self newDocument:nil];
			break;
		case 2:	// open panel
			[self openDocument:nil];
			break;
	}
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
}

- (IBAction)newDocument:(id)sender
{
	SLiMWindowController *windowController = [[SLiMWindowController alloc] initWithWindowNibName:@"SLiMWindow"];
	
	[windowController setDefaultScriptStringAndInitializeSimulation];
	[windowController showWindow:nil];
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	NSURL *url = [NSURL fileURLWithPath:filename];
	NSString *scriptString = [NSString stringWithContentsOfURL:url usedEncoding:NULL error:NULL];
	SLiMWindowController *windowController = [[SLiMWindowController alloc] initWithWindowNibName:@"SLiMWindow"];
	
	[windowController setScriptStringAndInitializeSimulation:scriptString];
	[windowController showWindow:nil];
	
	[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:url];	// remember this script file in the Recent Documents menu
	
	return YES;
}

- (IBAction)openDocument:(id)sender
{
	NSOpenPanel *op = [[NSOpenPanel openPanel] retain];
	
	[op setCanChooseDirectories:NO];
	[op setCanChooseFiles:YES];
	[op setAllowsMultipleSelection:NO];
	[op setExtensionHidden:NO];
	[op setCanSelectHiddenExtension:NO];
	[op setAllowedFileTypes:[NSArray arrayWithObject:@"txt"]];
	[op setTitle:@"Open Script"];
	
	if ([op runModal] == NSFileHandlingPanelOKButton)
	{
		NSString *scriptString = [NSString stringWithContentsOfURL:[op URL] usedEncoding:NULL error:NULL];
		
		if (scriptString)
		{
			[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:[op URL]];	// remember this script file in the Recent Documents menu
			
			SLiMWindowController *windowController = [[SLiMWindowController alloc] initWithWindowNibName:@"SLiMWindow"];
			
			[windowController setScriptStringAndInitializeSimulation:scriptString];
			[windowController showWindow:nil];
		}
	}
	
	[op release];
}

- (IBAction)resetSuppressionFlags:(id)sender
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	[defaults removeObjectForKey:defaultsSuppressScriptCheckSuccessPanelKey];
}


//
//	About window management
//

- (IBAction)showAboutWindow:(id)sender
{
	[[NSBundle mainBundle] loadNibNamed:@"AboutWindow" owner:self topLevelObjects:NULL];
	
	// The window is the top-level object in this nib.  It will release itself when closed, so we will retain it on its behalf here.
	// Note that the aboutWindow and aboutWebView outlets do not get zeroed out when the about window closes; but we only use them here.
	[aboutWindow retain];
	
	// And we need to make our WebView load our README.html file
	NSString *readmeURL = [[[NSBundle mainBundle] URLForResource:@"README" withExtension:@"html"] absoluteString];
	
	[aboutWebView setShouldCloseWithWindow:YES];
	[aboutWebView setMainFrameURL:readmeURL];
	
	// Set our version number string
	NSString *bundleVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
	NSString *versionString = [NSString stringWithFormat:@"v. %@", bundleVersion];
	
	[aboutVersionTextField setStringValue:versionString];
	
	// Now that everything is set up, show the window
	[aboutWindow makeKeyAndOrderFront:nil];
}

- (void)webView:(WebView *)webView decidePolicyForNavigationAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id <WebPolicyDecisionListener>)listener
{
	if ([actionInformation objectForKey:WebActionElementKey])
	{
		[listener ignore];
		[[NSWorkspace sharedWorkspace] openURL:[request URL]];
	}
	else
	{
		[listener use];
	}
}


//
//	Help
//

- (IBAction)showHelp:(id)sender
{
	if (!helpWindow)
	{
		[[NSBundle mainBundle] loadNibNamed:@"HelpWindow" owner:self topLevelObjects:NULL];
		
		// The window is the top-level object in this nib.  It will not release itself when closed, so it will stay around forever
		// (to improve reopening speed).  We retain it here on its behalf.
		[helpWindow retain];
		
		// And we need to make our WebView load our README.html file
		NSURL *manualURL = [[NSBundle mainBundle] URLForResource:@"SLiM_manual" withExtension:@"pdf"];
		
		[helpPDFView setDocument:[[[PDFDocument alloc] initWithURL:manualURL] autorelease]];
	}
	
	// Now that everything is set up, show the window
	[helpWindow makeKeyAndOrderFront:nil];
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

- (void)showScriptSyntaxHelpForSLiMWindowController:(SLiMWindowController *)windowController
{
	if (!scriptSyntaxWindow)
	{
		[[NSBundle mainBundle] loadNibNamed:@"ScriptSyntaxHelp" owner:self topLevelObjects:NULL];
		
		// The window is the top-level object in this nib.  It will not release itself when closed, so it will stay around forever
		// (to improve reopening speed).  We retain it here on its behalf.
		[scriptSyntaxWindow retain];
		
		// The textview is a separate object that needs to be added to the window's content view, because we don't want
		// it inside a scrollview/clipview, and IB is weird about allowing that...
		[scriptSyntaxTextView setFrame:[scriptSyntaxWhiteView frame]];
		[scriptSyntaxWhiteView addSubview:scriptSyntaxTextView];
		[scriptSyntaxTextView setFrame:NSOffsetRect([scriptSyntaxTextView frame], 0, -5)];
		
		// Load our help script, format it, etc.
		NSURL *scriptURL = [[NSBundle mainBundle] URLForResource:@"ScriptSyntaxHelp" withExtension:@"txt"];
		NSString *scriptString = [NSString stringWithContentsOfURL:scriptURL encoding:NSUTF8StringEncoding error:NULL];
		
		if (scriptString)
		{
			[scriptSyntaxTextView setString:scriptString];
			[scriptSyntaxTextView setFont:[NSFont fontWithName:@"Menlo" size:9.0]];
			[SLiMWindowController syntaxColorTextView:scriptSyntaxTextView];
		}
	}
	
	if (([scriptSyntaxWindow occlusionState] & NSWindowOcclusionStateVisible) == 0)
	{
		// Position the window on the left side of the simulation window if we can
		NSRect windowFrame = [scriptSyntaxWindow frame];
		NSRect mainWindowFrame = [[windowController window] frame];
		
		// try on the left side; if that doesn't work, we use the position specified in the nib
		{
			NSRect candidateFrame = windowFrame;
			
			candidateFrame.origin.x = mainWindowFrame.origin.x - (candidateFrame.size.width + 5);
			candidateFrame.origin.y = (mainWindowFrame.origin.y + mainWindowFrame.size.height - candidateFrame.size.height);
			
			if ([NSScreen visibleCandidateWindowFrame:candidateFrame])
				[scriptSyntaxWindow setFrameOrigin:candidateFrame.origin];
		}
	}
	
	// Now that everything is set up, show the window
	[scriptSyntaxWindow makeKeyAndOrderFront:nil];
}

@end

































































