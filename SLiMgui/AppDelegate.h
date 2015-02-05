//
//  AppDelegate.h
//  SLiMgui
//
//  Created by Ben Haller on 1/20/15.
//  Copyright (c) 2015 Messer Lab, http://messerlab.org/software/. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <WebKit/WebView.h>
#import <Quartz/Quartz.h>


@class SLiMWhiteView;


// User defaults keys
extern NSString *defaultsLaunchActionKey;
extern NSString *defaultsSyntaxHighlightScriptKey;
extern NSString *defaultsSyntaxHighlightOutputKey;
extern NSString *defaultsPlaySoundParseSuccessKey;
extern NSString *defaultsPlaySoundParseFailureKey;

extern NSString *defaultsSuppressScriptCheckSuccessPanelKey;


@interface AppDelegate : NSObject <NSApplicationDelegate>
{
	// About window cruft
	IBOutlet NSWindow *aboutWindow;
	IBOutlet WebView *aboutWebView;
	IBOutlet NSTextField *aboutVersionTextField;
	
	// Help window cruft
	IBOutlet NSWindow *helpWindow;
	IBOutlet PDFView *helpPDFView;
	
	// Script Syntax Help
	IBOutlet NSWindow *scriptSyntaxWindow;
	IBOutlet SLiMWhiteView *scriptSyntaxWhiteView;
	IBOutlet NSTextView *scriptSyntaxTextView;
}

- (IBAction)newDocument:(id)sender;
- (IBAction)openDocument:(id)sender;

- (IBAction)resetSuppressionFlags:(id)sender;

- (IBAction)showAboutWindow:(id)sender;

- (IBAction)showHelp:(id)sender;
- (IBAction)sendFeedback:(id)sender;
- (IBAction)showMesserLab:(id)sender;
- (IBAction)showBenHaller:(id)sender;
- (IBAction)showStickSoftware:(id)sender;
- (IBAction)showScriptSyntaxHelp:(id)sender;

@end

























































