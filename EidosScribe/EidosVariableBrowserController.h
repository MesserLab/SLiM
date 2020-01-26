//
//  EidosVariableBrowserController.h
//  EidosScribe
//
//  Created by Ben Haller on 6/13/15.
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


// BCH 4/7/2016: So we can build against the OS X 10.9 SDK
#ifndef NS_DESIGNATED_INITIALIZER
#define NS_DESIGNATED_INITIALIZER
#endif


@protocol EidosVariableBrowserControllerDelegate;


/*
 
 EidosVariableBrowserController provides a prefab variable browser for Eidos.  It is integrated into
 EidosConsoleWindowController, so if you use that class, you get the variable browser for free.  If
 you build your own Eidos UI, you can use EidosVariableBrowserController directly.  In that case,
 you will need to extract it from EidosConsoleWindow.xib.
 
 */


// Notifications sent to allow other objects that care about the visibility of the browser, such as toggle buttons, to update
extern NSString *EidosVariableBrowserWillHideNotification;
extern NSString *EidosVariableBrowserWillShowNotification;


@interface EidosVariableBrowserController : NSObject
{
}

// The delegate is often EidosConsoleWindowController, but can be your own delegate object
@property (nonatomic, assign) IBOutlet NSObject<EidosVariableBrowserControllerDelegate> *delegate;

// These properties are used by the nib, and are not likely to be used by clients
@property (nonatomic, retain) IBOutlet NSWindow *browserWindow;
@property (nonatomic, assign) IBOutlet NSOutlineView *browserOutline;
@property (nonatomic, assign) IBOutlet NSTableColumn *symbolColumn;
@property (nonatomic, assign) IBOutlet NSTableColumn *typeColumn;
@property (nonatomic, assign) IBOutlet NSTableColumn *sizeColumn;
@property (nonatomic, assign) IBOutlet NSTableColumn *valueColumn;

// Normally EidosConsoleWindowController is instantiated in the EidosConsoleWindow.xib nib
- (instancetype)init NS_DESIGNATED_INITIALIZER;

// Show/hide the browser window
- (void)showWindow;
- (void)hideWindow;

// Tell the controller that the browser window should be disposed of, not just closed; breaks retain loops
- (void)cleanup;

// Trigger a reload of the variable browser when symbols have changed
- (void)reloadBrowser;

// Make the browser show or hide; will send the appropriate notifications
- (IBAction)toggleBrowserVisibility:(id)sender;

@end


































































