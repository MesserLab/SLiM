//
//  TipsWindowController.m
//  SLiM
//
//  Created by Ben Haller on 12/2/16.
//  Copyright (c) 2016-2020 Philipp Messer.  All rights reserved.
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


#import "TipsWindowController.h"


NSString *SLiMDefaultsShowTipsPanelKey = @"ShowTipsPanel";
NSString *SLiMDefaultsTipsIndexKey = @"TipsIndex";
NSString *SLiMDefaultsTipsCountKey = @"TipsCount";

NSString *SLiMTipsDirectoryName = @"Tips";


@implementation TipsWindowController

+ (void)initialize
{
	[[NSUserDefaults standardUserDefaults] registerDefaults:@{
															  SLiMDefaultsShowTipsPanelKey : @YES,
															  SLiMDefaultsTipsIndexKey : @(-1),		// tip index 0, after advancing automatically
															  SLiMDefaultsTipsCountKey : @(15),		// DO NOT CHANGE; this was the number of tips prior to adding this default
															  }];
}

+ (void)showTipsWindowOnLaunch
{
	if ([[NSUserDefaults standardUserDefaults] boolForKey:SLiMDefaultsShowTipsPanelKey])
	{
		static TipsWindowController *controller = nil;
		
		if (!controller)
			[[[TipsWindowController alloc] init] autorelease];
		
		[controller showTipsPanel];
	}
}

- (instancetype)init
{
	if (self = [super init])
	{
		// Assess the tips on tap
		NSArray *paths = [[NSBundle mainBundle] pathsForResourcesOfType:nil inDirectory:SLiMTipsDirectoryName];
		
		tipsFilenames = [paths valueForKey:@"lastPathComponent"];
		tipsFilenames = [tipsFilenames sortedArrayUsingSelector:@selector(localizedStandardCompare:)];
		[tipsFilenames retain];
		
		NSInteger newTipCount = [tipsFilenames count];
		
		lastTipShown = [[NSUserDefaults standardUserDefaults] integerForKey:SLiMDefaultsTipsIndexKey];
		
		// If we were showing the "end tip" but now we have new tips, then we need to go backwards from the end tip
		NSInteger oldTipCount = [[NSUserDefaults standardUserDefaults] integerForKey:SLiMDefaultsTipsCountKey];
		
		if (newTipCount > oldTipCount)
		{
			if (lastTipShown == oldTipCount)
				lastTipShown--;
			
			[[NSUserDefaults standardUserDefaults] setInteger:newTipCount forKey:SLiMDefaultsTipsCountKey];
		}
	}
	
	return self;
}

- (void)dealloc
{
	[self setTipsWindow:nil];
	[tipsFilenames release];
	
	[super dealloc];
}

- (void)showTipsPanel
{
	// If we've already shown the user all of the available tips, then we don't show the panel again
	// until more become available (in a new version of SLiMgui, presumably)
	if (lastTipShown >= (NSInteger)[tipsFilenames count])
		return;
	
	// Load the tips panel
	[[NSBundle mainBundle] loadNibNamed:@"TipsWindow" owner:self topLevelObjects:nil];
	
	if (_tipsWindow)
	{
		[_tipsWindow setDelegate:self];
		[self retain];
		
		[self nextButtonClicked:nil];	// advance to the next available tip
		
		[_tipsWindow makeKeyAndOrderFront:nil];
	}
}

- (void)windowWillClose:(NSNotification *)notification
{
	bool suppressPanel = ([_suppressPanelCheckbox state] == NSOnState);
	
	if (suppressPanel)
		[[NSUserDefaults standardUserDefaults] setBool:NO forKey:SLiMDefaultsShowTipsPanelKey];
	
	[_tipsWindow setDelegate:nil];
	[self autorelease];
}


//
//	Actions
//

- (IBAction)rewindButtonClicked:(id)sender
{
	lastTipShown = -1;
	
	[self nextButtonClicked:nil];
}

- (IBAction)nextButtonClicked:(id)sender
{
	lastTipShown++;
	[[NSUserDefaults standardUserDefaults] setInteger:lastTipShown forKey:SLiMDefaultsTipsIndexKey];
	//NSLog(@"Advanced to %ld", (long)lastTipShown);
	
	// Set the enable state of the Next button; but note we allow ourselves to go one over, to show the "out of tips" message
	bool moreTips = (lastTipShown >= (NSInteger)[tipsFilenames count]);
	
	[_nextButton setEnabled:!moreTips];
	
	// Show the tip
	NSString *tipPath;
	
	if (lastTipShown < (NSInteger)[tipsFilenames count])
	{
		// Tip index is in range, so grab one from our filename array
		tipPath = [[NSBundle mainBundle] pathForResource:[tipsFilenames objectAtIndex:lastTipShown] ofType:nil inDirectory:SLiMTipsDirectoryName];
		
		[_tipsNumberTextField setStringValue:[NSString stringWithFormat:@"#%ld", (long)lastTipShown + 1]];
	}
	else
	{
		// Tip index is not in range, so put up the "out of tips" message
		tipPath = [[NSBundle mainBundle] pathForResource:@"Tip_END.rtfd" ofType:nil inDirectory:nil];
		
		[_tipsNumberTextField setStringValue:@"∞"];
	}
	
	[_tipsTextView readRTFDFromFile:tipPath];
	[_tipsTextView scrollPoint:NSZeroPoint];
}

- (IBAction)doneButtonClicked:(id)sender
{
	[_tipsWindow performClose:nil];
}

@end





































