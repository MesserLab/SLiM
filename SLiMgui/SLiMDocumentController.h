//
//  SLiMDocumentController.h
//  SLiM
//
//  Created by Ben Haller on 2/2/17.
//  Copyright (c) 2017-2020 Philipp Messer.  All rights reserved.
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


// This class provides some additional functionality to support transient documents.  It is instantiated in MainMenu.xib,
// and thus becomes the shared document controller for the app.


@class SLiMDocument;


@interface SLiMDocumentController : NSDocumentController
{
}

@property (nonatomic) BOOL creatingNonWFDocument;		// a flag set across newNonWFDocument: to signal that the new document is a nonWF document, not a WF document

- (SLiMDocument *)transientDocumentToReplace;
- (void)replaceTransientDocument:(NSArray *)documents;

- (void)openRecipeWithFilename:(NSString *)filename;

- (IBAction)newNonWFDocument:(id)sender;
- (IBAction)findRecipe:(id)sender;
- (IBAction)openRecipe:(id)sender;

@end











































