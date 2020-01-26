//
//  FindRecipeController.h
//  SLiM
//
//  Created by Ben Haller on 10/11/18.
//  Copyright (c) 2018-2020 Philipp Messer.  All rights reserved.
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
#import "EidosTextView.h"


@interface FindRecipeController : NSWindowController <NSTableViewDataSource, NSTableViewDelegate, NSTextFieldDelegate>
{
	NSArray *recipeFilenames;
	NSArray *recipeContents;
	
	NSArray *matchRecipeFilenames;
	
	IBOutlet NSTextField *keyword1TextField;
	IBOutlet NSTextField *keyword2TextField;
	IBOutlet NSTextField *keyword3TextField;
	IBOutlet NSButton *buttonOK;
	IBOutlet NSTableView *matchTableView;
	IBOutlet EidosTextView *scriptPreview;
}

+ (void)runFindRecipesPanel;

- (IBAction)okButtonPressed:(id)sender;
- (IBAction)cancelButtonPressed:(id)sender;

@end




























