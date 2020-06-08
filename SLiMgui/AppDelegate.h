//
//  AppDelegate.h
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


#import <Cocoa/Cocoa.h>
#import <WebKit/WebView.h>
#import <Quartz/Quartz.h>

#include <string>


@class SLiMWindowController;
@class EidosTextView;


// User defaults keys
extern NSString *defaultsLaunchActionKey;
extern NSString *defaultsDisplayFontSizeKey;
extern NSString *defaultsSyntaxHighlightScriptKey;
extern NSString *defaultsSyntaxHighlightOutputKey;
extern NSString *defaultsPlaySoundParseSuccessKey;
extern NSString *defaultsPlaySoundParseFailureKey;


@interface AppDelegate : NSObject <NSApplicationDelegate>
{
	std::string app_cwd_;		// the app's current working directory, set up by applicationWillFinishLaunching:
	bool launchedFromShell_;	// TRUE if launched from shell, FALSE if launched from Finder/other
}

@property (nonatomic, retain) IBOutlet NSMenu *openRecipesMenu;

- (std::string &)SLiMguiCurrentWorkingDirectory;
- (bool)launchedFromShell;

- (IBAction)resetSuppressionFlags:(id)sender;

- (IBAction)showAboutWindow:(id)sender;

- (IBAction)showHelp:(id)sender;
- (IBAction)sendFeedback:(id)sender;
- (IBAction)slimWorkshops:(id)sender;
- (IBAction)mailingListAnnounce:(id)sender;
- (IBAction)mailingListDiscuss:(id)sender;
- (IBAction)showMesserLab:(id)sender;
- (IBAction)showBenHaller:(id)sender;
- (IBAction)showStickSoftware:(id)sender;

@end

























































