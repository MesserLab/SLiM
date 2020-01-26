//
//  SLiMDocument.h
//  SLiM
//
//  Created by Ben Haller on 1/31/17.
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


@class SLiMWindowController;


@interface SLiMDocument : NSDocument
{
	// This is our model, effectively.  We don't really follow MVC very closely at all.  When we are getting created, we have to use
	// this ivar to save our model state temporarily, because a SLiMWindowController is not yet created for us to stuff the model
	// into.  That is the only time we use this ivar, though; once the controller is created, it keeps the model for us.  Not the
	// most elegant design; probably a bunch of stuff should move from SLiMWindowController over to here.
	NSString *documentScriptString;
	
	// Change count tracking relative to our last recycle (which is change count 0)
	int slimChangeCount;
	
	// Transient document support
	BOOL transient;
}

@property (retain, nonatomic) NSString *recipeName;

+ (NSString *)defaultWFScriptString;
+ (NSString *)defaultNonWFScriptString;

- (NSString *)documentScriptString;
- (void)setDocumentScriptString:(NSString *)newString;

- (SLiMWindowController *)slimWindowController;

- (BOOL)changedSinceRecycle;
- (void)resetSLiMChangeCount;

// Transient documents
- (BOOL)isTransient;
- (void)setTransient:(BOOL)flag;
- (BOOL)isTransientAndCanBeReplaced;

@end




































