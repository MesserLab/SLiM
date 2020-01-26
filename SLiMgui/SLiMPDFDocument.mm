//
//  SLiMPDFDocument.mm
//  SLiM
//
//  Created by Ben Haller on 4/20/17.
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


#import "SLiMPDFDocument.h"
#import "SLiMPDFWindowController.h"


@implementation SLiMPDFDocument

- (instancetype)init
{
	if (self = [super init])
	{
	}
	
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (SLiMPDFWindowController *)slimPDFWindowController
{
	NSArray *controllers = [self windowControllers];
	
	if ([controllers count] > 0)
	{
		NSWindowController *mainController = [controllers objectAtIndex:0];
		
		if ([mainController isKindOfClass:[SLiMPDFWindowController class]])
			return (SLiMPDFWindowController *)mainController;
	}
	
	return nil;
}

- (void)givePDFData:(NSData *)data toController:(SLiMPDFWindowController *)controller
{
	// Get our window controller to display the PDF data we have
	if (pdfData && [pdfData length])
	{
		PDFDocument *doc = [[PDFDocument alloc] initWithData:pdfData];
		
		[controller setDisplayPDFDocument:doc];
		[doc release];
	}
	else
	{
		[controller setDisplayPDFDocument:nil];
	}
}

- (void)makeWindowControllers
{
	NSArray *myControllers = [self windowControllers];
	
	// If this document displaced a transient document, it will already have been assigned
	// a window controller. If that is not the case, create one.
	if ([myControllers count] == 0)
	{
		SLiMPDFWindowController *controller = [[[SLiMPDFWindowController alloc] init] autorelease];
		
		[self addWindowController:controller];
		[self givePDFData:pdfData toController:controller];
	}
}

- (void)windowControllerDidLoadNib:(NSWindowController *)aController
{
	// Note this method is not called, because we override -makeWindowControllers
	[super windowControllerDidLoadNib:aController];
}

- (BOOL)readFromURL:(NSURL *)url ofType:(NSString *)typeName error:(NSError * _Nullable *)outError
{
	if ([super readFromURL:url ofType:typeName error:outError])
	{
		NSString *filePath = [url path];
		NSFileManager *fm = [NSFileManager defaultManager];
		
		[modificationDate release];
		modificationDate = [[[fm attributesOfItemAtPath:filePath error:nil] fileModificationDate] retain];
		
		return YES;
	}
	
	return NO;
}

- (BOOL)readFromData:(NSData *)data ofType:(NSString *)typeName error:(NSError **)outError
{
	if ([typeName isEqualToString:@"com.adobe.pdf"])
	{
		if (pdfData != data)
		{
			[pdfData release];
			pdfData = [data retain];
		}
		
		SLiMPDFWindowController *controller = [self slimPDFWindowController];
		
		if (controller)
			[self givePDFData:pdfData toController:controller];
		
		return YES;
	}
	
    if (outError)
        *outError = [NSError errorWithDomain:NSOSStatusErrorDomain code:unimpErr userInfo:nil];
    return NO;
}

- (void)revertAfterChange:(id)sender
{
	NSURL *fileURL  = [self fileURL];
	NSString *filePath = [fileURL path];
	NSFileManager *fm = [NSFileManager defaultManager];
	NSDate *newModDate = [[fm attributesOfItemAtPath:filePath error:nil] fileModificationDate];
	
	if ([newModDate timeIntervalSinceDate:modificationDate] > 0.0)
	{
		//NSLog(@"   reverting...");
		
		NSURL *url = [self fileURL];
		[self revertToContentsOfURL:url ofType:@"com.adobe.pdf" error:NULL];
	}
}

- (void)presentedItemDidChange
{
	//NSLog(@"presentedItemDidChange");
	
	[self performSelectorOnMainThread:@selector(revertAfterChange:) withObject:nil waitUntilDone:NO];
}

+ (NSArray<NSString *> *)writableTypes
{
	return [NSArray array];
}

+ (BOOL)isNativeType:(NSString *)type
{
	return NO;
}

- (void)copy:(id)sender
{
	if (pdfData && [pdfData length])
	{
		NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
		
		[pasteboard declareTypes:@[NSPDFPboardType] owner:self];
		[pasteboard setData:pdfData forType:NSPDFPboardType];
	}
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item
{
	// I would have expected that returning an empty array for writeableTypes would cause Save As...
	// to be disabled, but that is apparently not the case.  I can't find any better way than this.
	if ([item action] == @selector(saveDocumentAs:))
		return NO;

	if ([item action] == @selector(copy:))
		return (pdfData && [pdfData length]);
	
	//NSLog(@"sel = %@", NSStringFromSelector([item action]));
	
	return [super validateUserInterfaceItem:item];
}

+ (BOOL)autosavesInPlace
{
	return NO;
}

+ (BOOL)autosavesDrafts
{
	return NO;
}

+ (BOOL)preservesVersions
{
	return NO;
}

@end























































