//
//  SLiMDocumentController.m
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


#import "SLiMDocumentController.h"
#import "SLiMDocument.h"
#import "SLiMPDFDocument.h"
#import "FindRecipeController.h"
#import "CocoaExtra.h"


@implementation SLiMDocumentController

- (SLiMDocument *)transientDocumentToReplace
{
	NSArray *documents = [self documents];
	
	if ([documents count] == 1)
	{
		NSDocument *firstDoc = [documents objectAtIndex:0];
		
		if ([firstDoc isKindOfClass:[SLiMDocument class]])
		{
			SLiMDocument *slimDoc = (SLiMDocument *)firstDoc;
			
			if ([slimDoc isTransientAndCanBeReplaced])
				return slimDoc;
		}
	}
	
	return nil;
}

- (void)replaceTransientDocument:(NSArray *)documents
{
	SLiMDocument *transientDoc = [documents objectAtIndex:0], *doc = [documents objectAtIndex:1];
	
	NSArray *controllersToTransfer = [[transientDoc windowControllers] copy];
	NSEnumerator *controllerEnum = [controllersToTransfer objectEnumerator];
	NSWindowController *controller;
	
	[controllersToTransfer makeObjectsPerformSelector:@selector(retain)];
	
	while (controller = [controllerEnum nextObject])
	{
		[doc addWindowController:controller];
		[transientDoc removeWindowController:controller];
	}
	[transientDoc close];
	
	[controllersToTransfer makeObjectsPerformSelector:@selector(release)];
	[controllersToTransfer release];
	
	// Push the model into the new doc again, so that it gets flushed out to the window controller that really manages it
	[doc setDocumentScriptString:[doc documentScriptString]];
}

// When a document is opened, check to see whether there is a document that is already open, and whether it is transient.
// If so, transfer the document's window controllers and close the transient document.
- (void)openDocumentWithContentsOfURL:(NSURL *)absoluteURL display:(BOOL)displayDocument completionHandler:(nonnull void (^)(NSDocument * _Nullable, BOOL, NSError * _Nullable))completionHandler
{
	SLiMDocument *transientDoc = [self transientDocumentToReplace];
	
	if (transientDoc)
	{
		// Defer display so we can replace the transient document first.  We have to have our own handler to intercept the result,
		// process it, and then call our caller's completion handler.
		[super openDocumentWithContentsOfURL:absoluteURL display:NO completionHandler:(^ void (NSDocument *typelessDoc, BOOL already_open, NSError *error)
		{
			if (typelessDoc)
			{
				if ([typelessDoc isKindOfClass:[SLiMDocument class]])
				{
					SLiMDocument *doc = (SLiMDocument *)typelessDoc;
					
					[self replaceTransientDocument:[NSArray arrayWithObjects:transientDoc, doc, nil]];
				}
				
				if (displayDocument)
				{
					[typelessDoc makeWindowControllers];
					[typelessDoc showWindows];
				}
			}
			
			completionHandler(typelessDoc, NO, error);
		})];
	}
	else
	{
		[super openDocumentWithContentsOfURL:absoluteURL display:displayDocument completionHandler:completionHandler];
	}
}

- (IBAction)newNonWFDocument:(id)sender
{
	@try {
		_creatingNonWFDocument = YES;
		
		[self newDocument:sender];
	}
	@finally
	{
		_creatingNonWFDocument = NO;
	}
}

- (IBAction)findRecipe:(id)sender
{
	[FindRecipeController runFindRecipesPanel];
}

- (void)openRecipeWithFilename:(NSString *)filename
{
	if ([[filename pathExtension] isEqualToString:@"py"])
	{
		NSBundle *bundle = [NSBundle mainBundle];
		NSURL *urlForRecipe = [bundle URLForResource:[filename stringByDeletingPathExtension] withExtension:[filename pathExtension] subdirectory:@"Recipes"];
		
		if (urlForRecipe)
		{
			// Duplicate the file into /tmp; there seems to be no protection against the user modifying files inside the app bundle!
			// Setting the stationery bit wouldn't work either, I guess; I guess it would create a duplicate file inside the app bundle :-O
			// This all seems pretty broken; maybe it's because I'm running under Xcode, but I want it to behave right even in that case.
			// This at least prevents the user from seeing/modifying the file inside the app bundle, but it will have a weird name/path.
			NSFileManager *fm = [NSFileManager defaultManager];
			NSString *tempPath = [NSString slimPathForTemporaryFileWithPrefix:[filename stringByDeletingPathExtension]];
			NSString *tempFile = [tempPath stringByAppendingPathExtension:@"py"];
			
			[fm copyItemAtPath:[urlForRecipe path] toPath:tempFile error:NULL];
			
			// Then open Python files in whatever app the user has set up for that
			[[NSWorkspace sharedWorkspace] openFile:tempFile];
		}
	}
	else if ([[filename pathExtension] isEqualToString:@"txt"])
	{
		NSBundle *bundle = [NSBundle mainBundle];
		NSURL *urlForRecipe = [bundle URLForResource:[filename stringByDeletingPathExtension] withExtension:[filename pathExtension] subdirectory:@"Recipes"];
		
		if (urlForRecipe)
		{
			NSString *scriptString = [NSString stringWithContentsOfURL:urlForRecipe usedEncoding:NULL error:NULL];
			
			if (scriptString)
			{
				SLiMDocument *transientDoc = [self transientDocumentToReplace];
				NSDocument *typelessDoc = [[NSDocumentController sharedDocumentController] openUntitledDocumentAndDisplay:NO error:NULL];
				
				if (typelessDoc && [typelessDoc isKindOfClass:[SLiMDocument class]])
				{
					SLiMDocument *doc = (SLiMDocument *)typelessDoc;
					
					if (transientDoc)
						[self replaceTransientDocument:[NSArray arrayWithObjects:transientDoc, doc, nil]];
					
					[doc makeWindowControllers];
					[doc showWindows];
					
					[doc setDocumentScriptString:scriptString];
					
					NSString *recipeName = [filename stringByDeletingPathExtension];
					
					if ([recipeName hasPrefix:@"Recipe "])
						recipeName = [recipeName substringFromIndex:7];
					
					[doc setRecipeName:recipeName];			// tell the doc it's a recipe doc, so it can manage its title properly
					[[doc slimWindowController] synchronizeWindowTitleWithDocumentName];	// remake the window title
				}
			}
		}
	}
	else
	{
		NSLog(@"Unrecognized recipe extension %@", [filename pathExtension]);
		NSBeep();
	}
}

- (IBAction)openRecipe:(id)sender
{
	NSMenuItem *senderMenuItem = (NSMenuItem *)sender;
	NSString *recipeName = [senderMenuItem title];
	
	if ([recipeName hasSuffix:@".py 🐍"])
	{
		NSString *trimmedRecipeName = [recipeName stringByReplacingOccurrencesOfString:@" 🐍" withString:@""];
		NSString *fullRecipeName = [NSString stringWithFormat:@"Recipe %@", trimmedRecipeName];
		
		[self openRecipeWithFilename:fullRecipeName];
	}
	else
	{
		NSString *fullRecipeName = [[NSString stringWithFormat:@"Recipe %@", recipeName] stringByAppendingPathExtension:@"txt"];
		
		[self openRecipeWithFilename:fullRecipeName];
	}
}

// When a second document is added, the first document's transient status is cleared.
// This happens when the user selects "New" when a transient document already exists.
- (void)addDocument:(NSDocument *)newDoc
{
	NSArray *documents = [self documents];
	
	if ([documents count] == 1)
	{
		NSDocument *firstDoc = [documents objectAtIndex:0];
		
		if ([firstDoc isKindOfClass:[SLiMDocument class]])
		{
			SLiMDocument *slimDoc = (SLiMDocument *)firstDoc;
			
			if ([slimDoc isTransient])
				[slimDoc setTransient:NO];
		}
	}
	
	[super addDocument:newDoc];
}

- (void)noteNewRecentDocument:(NSDocument *)document
{
	// prevent PDF documents from being added to the recent documents list
	if ([document isKindOfClass:[SLiMPDFDocument class]])
		return;
	
	[super noteNewRecentDocument:document];
}

@end








































