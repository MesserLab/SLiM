//
//  AppDelegate.m
//  SLiMgui
//
//  Created by Ben Haller on 1/20/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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
#import "SLiMDocument.h"
#import "SLiMDocumentController.h"
#import "EidosHelpController.h"
#import "CocoaExtra.h"
#import "EidosCocoaExtra.h"
#import "eidos_beep.h"
#import "TipsWindowController.h"
#import <WebKit/WebKit.h>

#include <stdio.h>
#include <unistd.h>


// User defaults keys
NSString *defaultsLaunchActionKey = @"LaunchAction";
NSString *defaultsDisplayFontSizeKey = @"DisplayFontSize";
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
															  defaultsDisplayFontSizeKey : @11,
															  defaultsSyntaxHighlightScriptKey : @YES,
															  defaultsSyntaxHighlightOutputKey : @YES,
															  defaultsPlaySoundParseSuccessKey : @YES,
															  defaultsPlaySoundParseFailureKey : @YES
															  }];
}

- (void)dealloc
{
	[self setOpenRecipesMenu:nil];
	
	[super dealloc];
}

- (void)setUpRecipesMenu
{
	[openRecipesMenu removeAllItems];
	
	// Add the Find Recipe... menu item and separator
	{
		NSMenuItem *menuItem = [openRecipesMenu addItemWithTitle:@"Find Recipe..." action:@selector(findRecipe:) keyEquivalent:@"O"];
		
		[menuItem setTarget:[NSDocumentController sharedDocumentController]];
		
		[openRecipesMenu addItem:[NSMenuItem separatorItem]];
	}
	
	// Find recipes in our bundle
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
			
			if ([name hasPrefix:@"Recipe "])
			{
				if ([name hasSuffix:@".txt"])
				{
					// Remove the .txt extension for SLiM models
					NSString *trimmedName = [name substringWithRange:NSMakeRange(7, [name length] - 11)];
					
					[recipeNames addObject:trimmedName];
				}
				else if ([name hasSuffix:@".py"])
				{
					// Leave the .py extension for Python models
					NSString *trimmedName = [name substringWithRange:NSMakeRange(7, [name length] - 7)];
					
					[recipeNames addObject:[trimmedName stringByAppendingString:@" 🐍"]];
				}
			}
		}
	}
	
	[recipeNames sortUsingComparator:^NSComparisonResult(id obj1, id obj2) {
		return [(NSString *)obj1 compare:(NSString *)obj2 options:NSNumericSearch];
	}];
	
	NSString *previousItemChapter = nil;
	NSMenu *chapterSubmenu = nil;
	
	for (NSString *recipeName in recipeNames)
	{
		NSUInteger firstDotIndex = [recipeName rangeOfString:@"."].location;
		NSString *recipeChapter = (firstDotIndex != NSNotFound) ? [recipeName substringToIndex:firstDotIndex] : nil;
		
		// Create a submenu for each chapter
		if (recipeChapter && ![recipeChapter isEqualToString:previousItemChapter])
		{
			int recipeChapterValue = [recipeChapter intValue];
			NSString *chapterName = nil;
			
			switch (recipeChapterValue)
			{
				case 4: chapterName = @"Getting started: Neutral evolution in a panmictic population";		break;
				case 5: chapterName = @"Demography and population structure";								break;
				case 6: chapterName = @"Sexual reproduction";												break;
				case 7: chapterName = @"Mutation types, genomic elements, and chromosome structure";		break;
				case 8: chapterName = @"SLiMgui visualizations for polymorphism patterns";					break;
				case 9:	chapterName = @"Selective sweeps";													break;
				case 10:chapterName = @"Context-dependent selection using fitness() callbacks";				break;
				case 11:chapterName = @"Complex mating schemes using mateChoice() callbacks";				break;
				case 12:chapterName = @"Direct child modifications using modifyChild() callbacks";			break;
				case 13:chapterName = @"Phenotypes, fitness functions, quantitative traits, and QTLs";		break;
				case 14:chapterName = @"Advanced models";													break;
				case 15:chapterName = @"Continuous-space models and interactions";							break;
				case 16:chapterName = @"Going beyond Wright-Fisher models: nonWF model recipes";			break;
				case 17:chapterName = @"Tree-sequence recording: tracking population history";				break;
				case 18:chapterName = @"Modeling explicit nucleotides";										break;
				default: break;
			}
			
			if (chapterName)
			{
				NSString *fullChapterName = [NSString stringWithFormat:@"%d – %@", recipeChapterValue, chapterName];
				NSMenuItem *mainItem = [openRecipesMenu addItemWithTitle:fullChapterName action:NULL keyEquivalent:@""];
				
				chapterSubmenu = [[[NSMenu alloc] init] autorelease];
				
				[mainItem setSubmenu:chapterSubmenu];
			}
			else
			{
				NSLog(@"unrecognized chapter value %d", recipeChapterValue);
				NSBeep();
				break;
			}
		}
		
		// Move on to the current chapter
		previousItemChapter = recipeChapter;
		
		// And now add the menu item for the recipe
		NSMenuItem *menuItem = [chapterSubmenu addItemWithTitle:recipeName action:@selector(openRecipe:) keyEquivalent:@""];
		
		[menuItem setTarget:[NSDocumentController sharedDocumentController]];
	}
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
	// Determine whether we were launched from a shell or from something else (Finder, Xcode, etc.)
	launchedFromShell_ = (isatty(fileno(stdin)) == 1) && !SLiM_AmIBeingDebugged();
	//NSLog(@"launched from %@", launchedFromShell_ ? @"shell" : @"Finder");
	
    // Require light appearance, at least for now; supporting dark mode would require custom art etc.
    if ([NSApp respondsToSelector:@selector(setAppearance:)])
        [NSApp setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];
    
	// Install our custom beep handler
	Eidos_Beep = &Eidos_Beep_MACOS;
	
	// Warm up our back ends before anything else happens
	Eidos_WarmUp();
	SLiM_WarmUp();
	gEidosContextClasses.push_back(gSLiM_SLiMgui_Class);			// available only in SLiMgui
	Eidos_FinishWarmUp();
	
	// Remember our current working directory, to return to whenever we are not inside SLiM/Eidos
	app_cwd_ = Eidos_CurrentDirectory();
	//NSLog(@"current directory == %s", app_cwd_.c_str());
	
	// Create the Open Recipes menu
	[self setUpRecipesMenu];
}

- (std::string &)SLiMguiCurrentWorkingDirectory
{
	return app_cwd_;
}

- (bool)launchedFromShell
{
	return launchedFromShell_;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	// check for required fonts, to prevent problems downstream
	NSFont *times = [NSFont fontWithName:@"Times New Roman" size:13.0];
	NSFont *menlo = [NSFont fontWithName:@"Menlo" size:13.0];
	NSFont *optima = [NSFont fontWithName:@"Optima" size:13.0];
	NSFont *optima_b = [NSFont fontWithName:@"Optima-Bold" size:13.0];
	NSFont *optima_i = [NSFont fontWithName:@"Optima-Italic" size:13.0];
	NSString *fontName = nil;
	
	if (!times)				fontName = @"Times New Roman";
	else if (!menlo)		fontName = @"Menlo";
	else if (!optima)		fontName = @"Optima";
	else if (!optima_b)		fontName = @"Optima-Bold";
	else if (!optima_i)		fontName = @"Optima-Italic";
	
	if (fontName)
	{
		NSAlert *alert = [[NSAlert alloc] init];
		
		[alert setAlertStyle:NSCriticalAlertStyle];
		[alert setMessageText:@"Missing font"];
		[alert setInformativeText:[NSString stringWithFormat:@"The standard system font %@ is required for SLiMgui to run, but appears to be missing.  Please check your macOS installation.\n\nSLiMgui will now exit.", fontName]];
		[alert addButtonWithTitle:@"OK"];
		
		[alert runModal];
		[[NSApplication sharedApplication] terminate:nil];
		return;
	}
	
	// perform the launch action
	SLiMLaunchAction launchAction = (SLiMLaunchAction)[[NSUserDefaults standardUserDefaults] integerForKey:defaultsLaunchActionKey];
	
	switch (launchAction)
	{
		case kLaunchDoNothing:
			break;
		case kLaunchNewScriptWindow:
			[self performSelector:@selector(openNewDocumentIfNeeded:) withObject:nil afterDelay:0.01];
			break;
		case kLaunchRunOpenPanel:
			[[NSDocumentController sharedDocumentController] openDocument:nil];
			break;
	}
	
	[TipsWindowController showTipsWindowOnLaunch];
}

- (void)openNewDocumentIfNeeded:(id)sender
{
	NSUInteger documentCount = [[[NSDocumentController sharedDocumentController] documents] count];
	
	// Open an untitled document if there is no document (restored, opened)
	if (documentCount == 0)
	{
		SLiMDocument *doc = [[NSDocumentController sharedDocumentController] openUntitledDocumentAndDisplay:YES error:NULL];
		
		[doc setTransient:YES];
	}
}

- (BOOL)applicationShouldOpenUntitledFile:(NSApplication *)sender
{
	// Prevent AppKit from opening a new untitled window on launch.  We do this because we do this ourselves
	// instead; and we do it ourselves instead because Apple seems to have screwed it up.  (I.e., sometimes
	// this method does not get called even when no other document is being restored.)
	return NO;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag
{
	// Prevent a new untitled window from opening if we are already running, have no open windows, and are activated.
	// This is non-standard behavior, but I really hate the standard behavior, so.
	return NO;
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
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

- (IBAction)slimWorkshops:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://benhaller.com/workshops/workshops.html"]];
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

- (IBAction)showSlimExtrasPage:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"https://github.com/MesserLab/SLiM-Extras"]];
}

- (IBAction)showSlimPublication:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"https://doi.org/10.1093/molbev/msw211"]];
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
- (IBAction)profile:(id)sender {}
- (IBAction)toggleConsoleVisibility:(id)sender {}
- (IBAction)toggleBrowserVisibility:(id)sender {}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
	SEL sel = [menuItem action];
	
	// Handle validation for menu items that really belong to SLiMWindowController.  This provides a default validation
	// for these menu items when no SLiMWindowController is receiving.  The point is to reset the titles of these menu
	// items back to their base state, not just to disable them (which would happen anyway).
	if (sel == @selector(play:))
	{
		[menuItem setTitle:@"Play"];
		return NO;
	}
	if (sel == @selector(profile:))
	{
		[menuItem setTitle:@"Profile"];
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

































































