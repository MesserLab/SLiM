//
//  SLiMPDFView.m
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


#import "SLiMPDFView.h"


@implementation SLiMPDFView

- (instancetype)initWithFrame:(NSRect)frameRect
{
	if (self = [super initWithFrame:frameRect])
	{
	}
	
	return self;
}

- (void)dealloc
{
	[_document release];
	_document = nil;
	
	[super dealloc];
}

- (void)drawRect:(NSRect)dirtyRect
{
	NSRect bounds = [self bounds];
	
	[[NSColor darkGrayColor] set];
	NSRectFill(bounds);
	
	if (_document)
	{
		// thanks to https://ryanbritton.com/2015/09/correctly-drawing-pdfs-in-cocoa/ for this code
		
		CGSize drawingSize = {bounds.size.width, bounds.size.height};
		
		// Document Loading
		CGPDFDocumentRef pdfDocument = [_document documentRef];
		CGPDFPageRef page = CGPDFDocumentGetPage(pdfDocument, 1);
		
		// Start by getting the crop box since only its contents should be drawn
		CGRect cropBox = CGPDFPageGetBoxRect(page, kCGPDFCropBox);
		
		// Account for rotation of the page to figure out the size to create the context. Like images, 
		// rotation can be represented by one of two ways in a PDF: the contents can be pre-rotated
		// in which case nothing needs to be done or the document can have its rotation value set and
		// it means we need to apply the rotation as an affine transformation when drawing
		NSInteger rotationAngle = CGPDFPageGetRotationAngle(page);
		CGFloat angleInRadians = -rotationAngle * (M_PI / 180);
		CGAffineTransform transform = CGAffineTransformMakeRotation(angleInRadians);
		CGRect rotatedCropRect = CGRectApplyAffineTransform(cropBox, transform);
		
		// Figure out the closest size we can draw the PDF at that's no larger than drawingSize
		CGRect bestFit = CGRectMake(0, 0, 0, 0);
		double rotatedCropRectAspect = rotatedCropRect.size.width / rotatedCropRect.size.height;
		double drawingSizeAspect = drawingSize.width / drawingSize.height;
		
		if (rotatedCropRectAspect > drawingSizeAspect)
		{
			bestFit.size.width = drawingSize.width;
			bestFit.size.height = round(rotatedCropRect.size.height * (drawingSize.width / rotatedCropRect.size.width));
			bestFit.origin.y = round((drawingSize.height - bestFit.size.height) / 2);
		}
		else
		{
			bestFit.size.width = round(rotatedCropRect.size.width * (drawingSize.height / rotatedCropRect.size.height));
			bestFit.size.height = drawingSize.height;
			bestFit.origin.x = round((drawingSize.width - bestFit.size.width) / 2);
		}
		
		CGFloat scale = CGRectGetHeight(bestFit) / CGRectGetHeight(rotatedCropRect);
		
		// Get the drawing context
		CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
		
		// Create the affine transformation matrix to align the PDF's CropBox to our drawing context.
		//transform = CGPDFPageGetDrawingTransform(page, kCGPDFCropBox, CGRectMake(0, 0, CGRectGetWidth(bestFit), CGRectGetHeight(bestFit)), 0, true);
		transform = CGPDFPageGetDrawingTransform(page, kCGPDFCropBox, bestFit, 0, true);
		
		if (scale > 1)
		{
			// Since CGPDFPageGetDrawingTransform won't scale up, we need to do it manually
			transform = CGAffineTransformTranslate(transform, CGRectGetMidX(cropBox), CGRectGetMidY(cropBox));
			transform = CGAffineTransformScale(transform, scale, scale);
			transform = CGAffineTransformTranslate(transform, -CGRectGetMidX(cropBox), -CGRectGetMidY(cropBox));
		}
		
		CGContextConcatCTM(context, transform);
		
		// Clip the drawing to the CropBox
		CGContextAddRect(context, cropBox);
		CGContextClip(context);
		
		[[NSColor whiteColor] set];
		NSRectFill(cropBox);
		
		CGContextDrawPDFPage(context, page);
	}
}

- (BOOL)isOpaque
{
	return YES;
}

- (void)setDocument:(PDFDocument *)document
{
	[document retain];
	[_document release];
	_document = document;
	
	[self setNeedsDisplay:YES];
}

- (NSMenu *)menu
{
	NSMenu *menu = [[NSMenu alloc] initWithTitle:@"pdf_menu"];
	NSMenuItem *menuItem;
	
	menuItem = [menu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@""];
	[menuItem setTarget:[[[self window] windowController] document]];
	
	return [menu autorelease];
}

@end


































