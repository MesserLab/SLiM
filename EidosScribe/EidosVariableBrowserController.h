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


@protocol EidosVariableBrowserDelegate <NSObject>
- (EidosSymbolTable *)symbolTable;
@end


// Notifications sent to allow other objects that care about the visibility of the browser, such as toggle buttons, to update
extern NSString *EidosVariableBrowserWillHideNotification;
extern NSString *EidosVariableBrowserWillShowNotification;


@interface EidosVariableBrowserController : NSObject <NSWindowDelegate, NSOutlineViewDataSource, NSOutlineViewDelegate>
{
	// Wrappers for the currently displayed objects
	NSMutableArray *rootBrowserWrappers;
	
	// A set used to remember expanded items; see -reloadBrowser
	NSMutableSet *expandedSet;
}

@property (nonatomic, assign) IBOutlet NSObject<EidosVariableBrowserDelegate> *delegate;

@property (nonatomic, retain) IBOutlet NSWindow *browserWindow;
@property (nonatomic, assign) IBOutlet NSOutlineView *browserOutline;
@property (nonatomic, assign) IBOutlet NSTableColumn *symbolColumn;
@property (nonatomic, assign) IBOutlet NSTableColumn *typeColumn;
@property (nonatomic, assign) IBOutlet NSTableColumn *sizeColumn;
@property (nonatomic, assign) IBOutlet NSTableColumn *valueColumn;

- (void)reloadBrowser;

- (IBAction)toggleBrowserVisibility:(id)sender;

@end


































































