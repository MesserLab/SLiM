//
//  SLiMDocumentController.m
//  SLiM
//
//  Created by Ben Haller on 2/2/17.
//  Copyright (c) 2017-2018 Philipp Messer.  All rights reserved.
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

- (IBAction)openRecipe:(id)sender
{
	NSString *recipeName = [sender title];
	NSString *fullRecipeName = [NSString stringWithFormat:@"Recipe %@", recipeName];
	NSBundle *bundle = [NSBundle mainBundle];
	NSURL *urlForRecipe = [bundle URLForResource:fullRecipeName withExtension:@".txt" subdirectory:@"Recipes"];
	
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
				[doc setRecipeName:recipeName];					// tell the doc it's a recipe doc, so it can manage its title properly
				[[doc slimWindowController] synchronizeWindowTitleWithDocumentName];	// remake the window title
			}
		}
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








































