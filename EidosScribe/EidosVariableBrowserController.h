//
//  EidosVariableBrowserController.h
//  EidosScribe
//
//  Created by Ben Haller on 6/13/15.
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


#import <Cocoa/Cocoa.h>

#include "eidos_symbol_table.h"


/*
 
 EidosVariableBrowserController provides a prefab variable browser for Eidos.  It is integrated into
 EidosConsoleWindowController, so if you use that class, you get the variable browser for free.  If
 you build your own Eidos UI, you can use EidosVariableBrowserController directly.  In that case,
 you will need to extract it from ConsoleWindow.xib.
 
 */


@class EidosVariableBrowserController;


// The variable browser controller gets the symbols to display from its delegate; if you are using
// EidosConsoleWindowController it typically acts as the delegate for the variable browser, but
// if you are building your own Eidos user interface you will need to provide this delegate method.
@protocol EidosVariableBrowserDelegate <NSObject>
- (EidosSymbolTable *)symbolTableForEidosVariableBrowserController:(EidosVariableBrowserController *)browserController;
@end


// Notifications sent to allow other objects that care about the visibility of the browser, such as toggle buttons, to update
extern NSString *EidosVariableBrowserWillHideNotification;
extern NSString *EidosVariableBrowserWillShowNotification;


@interface EidosVariableBrowserController : NSObject <NSWindowDelegate, NSOutlineViewDataSource, NSOutlineViewDelegate>
{
@private
	// Wrappers for the currently displayed objects
	NSMutableArray *rootBrowserWrappers;
	
	// A set used to remember expanded items; see -reloadBrowser
	NSMutableSet *expandedSet;
}

// The delegate is often EidosConsoleWindowController, but can be your own delegate object
@property (nonatomic, assign) IBOutlet NSObject<EidosVariableBrowserDelegate> *delegate;

// These properties are used by the nib, and are not likely to be used by clients
@property (nonatomic, retain) IBOutlet NSWindow *browserWindow;
@property (nonatomic, assign) IBOutlet NSOutlineView *browserOutline;
@property (nonatomic, assign) IBOutlet NSTableColumn *symbolColumn;
@property (nonatomic, assign) IBOutlet NSTableColumn *typeColumn;
@property (nonatomic, assign) IBOutlet NSTableColumn *sizeColumn;
@property (nonatomic, assign) IBOutlet NSTableColumn *valueColumn;

// Trigger a reload of the variable browser when symbols have changed
- (void)reloadBrowser;

// Make the browser show or hide; will send the appropriate notifications
- (IBAction)toggleBrowserVisibility:(id)sender;

@end


































































