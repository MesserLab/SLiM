//
//  EidosAppDelegate.h
//  EidosScribe
//
//  Created by Ben Haller on 4/7/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
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


#import <Cocoa/Cocoa.h>

@class EidosConsoleWindowController;


/*
 
 EidosAppDelegate is an NSApplication delegate for EidosScribe that provides some behavior for menu items and such.
 If you make your own Context app, you are unlikely to want to reuse this class, although it might illustrate
 how to set up an EidosConsoleWindowController.
 
 */


@interface EidosAppDelegate : NSObject
{
}

// An outlet for the console controller, associated with EidosConsoleWindow.xib
@property (nonatomic, retain) IBOutlet EidosConsoleWindowController *consoleController;

// Actions for menu commands in MainMenu.xib
- (IBAction)showAboutWindow:(id)sender;

- (IBAction)sendFeedback:(id)sender;
- (IBAction)showMesserLab:(id)sender;
- (IBAction)showBenHaller:(id)sender;
- (IBAction)showStickSoftware:(id)sender;

@end

































































