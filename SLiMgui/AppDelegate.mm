//
//  AppDelegate.m
//  SLiMgui
//
//  Created by Ben Haller on 1/20/15.
//  Copyright (c) 2015-2016 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
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
#import "EidosHelpController.h"
#import "CocoaExtra.h"
#import "EidosCocoaExtra.h"
#import "TipsWindowController.h"
#import <WebKit/WebKit.h>


// User defaults keys
NSString *defaultsLaunchActionKey = @"LaunchAction";
NSString *defaultsSyntaxHighlightScriptKey = @"SyntaxHighlightScript";
NSString *defaultsSyntaxHighlightOutputKey = @"SyntaxHighlightOutput";
NSString *defaultsPlaySoundParseSuccessKey = @"PlaySoundParseSuccess";
NSString *defaultsPlaySoundParseFailureKey = @"PlaySoundParseFailure";

typedef enum SLiMLaunchAction
{
	kLaunchDoNothing = 0,
	kLaunchNewScriptWindow,
	kLaunchRunOpenPanel
} SLiMLaunchAction;


@interface AppDelegate () <NSApplicationDelegate>
{
	// About window cruft
	IBOutlet NSWindow *aboutWindow;
	IBOutlet NSTextField *aboutVersionTextField;
	IBOutlet NSTextField *messerLabLineTextField;
	IBOutlet NSTextField *benHallerLineTextField;
	IBOutlet NSTextField *licenseTextField;
	
	// Help window cruft; kept for possible resurrection, see -showHelp:
	//IBOutlet NSWindow *helpWindow;
	//IBOutlet PDFView *helpPDFView;
}
@end


@implementation AppDelegate

@synthesize openRecipesMenu;

+ (void)initialize
{
	[[NSUserDefaults standardUserDefaults] registerDefaults:@{
															  defaultsLaunchActionKey : @(kLaunchNewScriptWindow),
															  defaultsSyntaxHighlightScriptKey : @YES,
															  defaultsSyntaxHighlightOutputKey : @YES,
															  defaultsPlaySoundParseSuccessKey : @YES,
															  defaultsPlaySoundParseFailureKey : @YES
															  }];
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
	// Warm up our back ends before anything else happens
	Eidos_WarmUp();
	SLiM_WarmUp();
	
	// Poke SLiMDocumentController so it gets set up before NSDocumentController gets in.  Note we don't need to keep a
	// reference to this, because AppKit will return it to us as +[NSDocumentController sharedDocumentController].
	[[SLiMDocumentController alloc] init];
	
	// Create the Open Recipes menu
	[openRecipesMenu removeAllItems];
	
	NSURL *urlForRecipesFolder = [[NSBundle mainBundle] URLForResource:@"Recipes" withExtension:@""];
	NSFileManager *fm = [NSFileManager defaultManager];
	NSDirectoryEnumerator *dirEnum = [fm enumeratorAtURL:urlForRecipesFolder includingPropertiesForKeys:@[NSURLNameKey, NSURLIsDirectoryKey] options:(NSDirectoryEnumerationSkipsHiddenFiles | NSDirectoryEnumerationSkipsSubdirectoryDescendants) errorHandler:nil];
	NSMutableArray *recipeNames = [NSMutableArray array];
	
	for (NSURL *fileURL in dirEnum)
	{
		NSNumber *isDirectory = nil;
		
		[fileURL getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];
		
		if (![isDirectory boolValue])
		{
			NSString *name = nil;
			
			[fileURL getResourceValue:&name forKey:NSURLNameKey error:nil];
			
			if ([name hasPrefix:@"Recipe "] && [name hasSuffix:@".txt"])
			{
				NSString *trimmedName = [name substringWithRange:NSMakeRange(7, [name length] - 11)];
				
				[recipeNames addObject:trimmedName];
			}
		}
	}
	
	[recipeNames sortUsingDescriptors:@[[NSSortDescriptor sortDescriptorWithKey:@"intValue" ascending:YES]]];
	
	NSString *previousItemChapter = nil;
	
	for (NSString *recipeName in recipeNames)
	{
		NSUInteger firstDotIndex = [recipeName rangeOfString:@"."].location;
		NSString *recipeChapter = (firstDotIndex != NSNotFound) ? [recipeName substringToIndex:firstDotIndex] : nil;
		
		// Add a separator item between recipes from different chapters
		if (previousItemChapter && recipeChapter && ![recipeChapter isEqualToString:previousItemChapter])
			[openRecipesMenu addItem:[NSMenuItem separatorItem]];
		
		previousItemChapter = recipeChapter;
		
		NSMenuItem *menuItem = [openRecipesMenu addItemWithTitle:recipeName action:@selector(openRecipe:) keyEquivalent:@""];
		
		[menuItem setTarget:self];
	}
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	switch ((SLiMLaunchAction)[[NSUserDefaults standardUserDefaults] integerForKey:defaultsLaunchActionKey])
	{
		case kLaunchDoNothing:
			break;
		case kLaunchNewScriptWindow:
			[self newDocument:nil];
			break;
		case kLaunchRunOpenPanel:
			[self openDocument:nil];
			break;
	}
	
	[TipsWindowController showTipsWindowOnLaunch];
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
	NSURL *fileURL = [NSURL fileURLWithPath:filename];
	NSString *scriptString = [NSString stringWithContentsOfURL:fileURL usedEncoding:NULL error:NULL];
	SLiMWindowController *windowController = [[SLiMWindowController alloc] initWithWindowNibName:@"SLiMWindow"];
	
	[windowController setScriptStringAndInitializeSimulation:scriptString];
	[[windowController window] setTitleWithRepresentedFilename:[fileURL path]];
	[windowController showWindow:nil];
	
	[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:fileURL];	// remember this script file in the Recent Documents menu
	
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
	[op setAllowedFileTypes:@[@"txt"]];
	[op setTitle:@"Open Script"];
	
	if ([op runModal] == NSFileHandlingPanelOKButton)
	{
		NSURL *fileURL = [op URL];
		NSString *scriptString = [NSString stringWithContentsOfURL:fileURL usedEncoding:NULL error:NULL];
		
		if (scriptString)
		{
			[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:fileURL];	// remember this script file in the Recent Documents menu
			
			SLiMWindowController *windowController = [[SLiMWindowController alloc] initWithWindowNibName:@"SLiMWindow"];
			
			[windowController setScriptStringAndInitializeSimulation:scriptString];
			[[windowController window] setTitleWithRepresentedFilename:[fileURL path]];
			[windowController showWindow:nil];
		}
	}
	
	[op release];
}

- (IBAction)openRecipe:(id)sender
{
	NSString *recipeName = [sender title];
	NSString *fullRecipeName = [NSString stringWithFormat:@"Recipe %@", recipeName];
	NSBundle *bundle = [NSBundle mainBundle];
	NSURL *urlForRecipe = [bundle URLForResource:fullRecipeName withExtension:@".txt" subdirectory:@"Recipes"];
	
	if (urlForRecipe)
	{
		NSString *scriptString = [NSString stringWithContentsOfURL:urlForRecipe usedEncoding:NULL error:NULL];
		
		if (scriptString)
		{
			SLiMWindowController *windowController = [[SLiMWindowController alloc] initWithWindowNibName:@"SLiMWindow"];
			
			[windowController setScriptStringAndInitializeSimulation:scriptString];
			[[windowController window] setTitle:recipeName];
			[windowController showWindow:nil];
		}
	}
}

- (IBAction)resetSuppressionFlags:(id)sender
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	[defaults removeObjectForKey:EidosDefaultsSuppressScriptCheckSuccessPanelKey];
	
	[defaults removeObjectForKey:SLiMDefaultsShowTipsPanelKey];
	[defaults removeObjectForKey:SLiMDefaultsTipsIndexKey];
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
	
//	// And we need to make our WebView load our README.html file
//	NSString *readmeURL = [[[NSBundle mainBundle] URLForResource:@"README" withExtension:@"html"] absoluteString];
//	
//	[aboutWebView setShouldCloseWithWindow:YES];
//	[aboutWebView setMainFrameURL:readmeURL];
	
	// Set our version number string
	NSString *bundleVersionString = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
	NSString *bundleVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleVersion"];
	NSString *versionString = [NSString stringWithFormat:@"%@ (build %@)", bundleVersionString, bundleVersion];
	
	[aboutVersionTextField setStringValue:versionString];
	
	// Fix up hyperlinks
	[messerLabLineTextField eidosSetHyperlink:[NSURL URLWithString:@"http://messerlab.org/slim/"] onText:@"http://messerlab.org/slim/"];
	[benHallerLineTextField eidosSetHyperlink:[NSURL URLWithString:@"http://benhaller.com/"] onText:@"http://benhaller.com/"];
	[licenseTextField eidosSetHyperlink:[NSURL URLWithString:@"http://www.gnu.org/licenses/"] onText:@"http://www.gnu.org/licenses/"];
	[licenseTextField eidosSetHyperlink:[NSURL URLWithString:@"http://www.gnu.org/software/gsl/"] onText:@"http://www.gnu.org/software/gsl/"];
	[licenseTextField eidosSetHyperlink:[NSURL URLWithString:@"http://www.boost.org/LICENSE_1_0.txt"] onText:@"http://www.boost.org/LICENSE_1_0.txt"];
	[licenseTextField eidosSetHyperlink:[NSURL URLWithString:@"http://www.boost.org/users/download/#live"] onText:@"http://www.boost.org/users/download/#live"];
	
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
	/*
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
	*/
	
	// We used to show the SLiM 1.8 manual as a PDF in a separate window.  That code is commented out above.
	// I've left HelpWindow.xib in the project, in case we want to go back to doing that sort of thing; but
	// for now, we show the script help window.
	[[EidosHelpController sharedController] showWindow];
}

- (IBAction)sendFeedback:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"mailto:philipp.messer@gmail.com?subject=SLiM%20Feedback"]];
}

- (IBAction)mailingListAnnounce:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"https://groups.google.com/d/forum/slim-announce"]];
}

- (IBAction)mailingListDiscuss:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"https://groups.google.com/d/forum/slim-discuss"]];
}

- (IBAction)showSlimHomePage:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://messerlab.org/slim/"]];
}

- (IBAction)showSlimPublication:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://dx.doi.org/10.1093/molbev/msw211"]];
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

// Dummy actions; see validateMenuItem:
- (IBAction)play:(id)sender {}
- (IBAction)toggleScriptVisibility:(id)sender {}
- (IBAction)toggleConsoleVisibility:(id)sender {}
- (IBAction)toggleBrowserVisibility:(id)sender {}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
	SEL sel = [menuItem action];
	
	// Handle validation for menu items that really belong to SLiMWindowController.  This provides a default validation
	// for these menu items when no SLiMWindowController is receiving.
	if (sel == @selector(play:))
	{
		[menuItem setTitle:@"Play"];
		return NO;
	}
	if (sel == @selector(toggleScriptVisibility:))
	{
		[menuItem setTitle:@"Show Script/Output"];
		return NO;
	}
	if (sel == @selector(toggleConsoleVisibility:))
	{
		[menuItem setTitle:@"Show Eidos Console"];
		return NO;
	}
	if (sel == @selector(toggleBrowserVisibility:))
	{
		[menuItem setTitle:@"Show Variable Browser"];
		return NO;
	}
	
	return YES;
}

@end

































































