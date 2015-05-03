//
//  AppDelegate.h
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


#import <Cocoa/Cocoa.h>
#import "ConsoleTextView.h"

#include "script.h"
#include "script_value.h"
#include "script_interpreter.h"
#include "slim_sim.h"


@interface AppDelegate : NSObject <NSApplicationDelegate, ConsoleTextViewDelegate>
{
	// About window cruft
	IBOutlet NSWindow *aboutWindow;
	IBOutlet NSTextField *aboutVersionTextField;
	
	// Interpreter logistical support
	SymbolTable *global_symbols;
	SLiMSim *sim;
}

@property (retain) IBOutlet NSWindow *scriptWindow;
@property (retain) IBOutlet NSSplitView *mainSplitView;
@property (retain) IBOutlet NSTextView *scriptTextView;
@property (retain) IBOutlet ConsoleTextView *outputTextView;

- (void)executeScriptString:(NSString *)scriptString addOptionalSemicolon:(BOOL)addSemicolon;

- (IBAction)showAboutWindow:(id)sender;

- (IBAction)sendFeedback:(id)sender;
- (IBAction)showMesserLab:(id)sender;
- (IBAction)showBenHaller:(id)sender;
- (IBAction)showStickSoftware:(id)sender;

- (IBAction)checkScript:(id)sender;
- (IBAction)showScriptHelp:(id)sender;
- (IBAction)clearOutput:(id)sender;
- (IBAction)executeAll:(id)sender;
- (IBAction)executeSelection:(id)sender;

@end



































































