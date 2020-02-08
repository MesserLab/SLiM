//
//  SLiMHaplotypeGraphView.mm
//  SLiM
//
//  Created by Ben Haller on 11/6/17.
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


#import "SLiMHaplotypeGraphView.h"
#import "SLiMWindowController.h"
#import "SLiMHaplotypeManager.h"


@implementation SLiMHaplotypeGraphView

- (void)cleanup
{
	//NSLog(@"[SLiMHaplotypeGraphView cleanup]");
	
	if (haplotypeManager)
	{
		[haplotypeManager release];
		haplotypeManager = nil;
	}
}

- (void)dealloc
{
	[self cleanup];
	
	//NSLog(@"[SLiMHaplotypeGraphView dealloc]");
	[super dealloc];
}

- (void)configureForDisplayWithSlimWindowController:(SLiMWindowController *)controller
{
	SLiMHaplotypeClusteringMethod clusterMethod = ([controller haplotypeClustering] == 0) ? kSLiMHaplotypeClusterNearestNeighbor : kSLiMHaplotypeClusterGreedy;
	SLiMHaplotypeClusteringOptimization optMethod = ([controller haplotypeClustering] == 2) ? kSLiMHaplotypeClusterOptimizeWith2opt : kSLiMHaplotypeClusterNoOptimization;
	int sampleSize = ([controller haplotypeSample] == 1) ? [controller haplotypeSampleSize] : -1;
	
	haplotypeManager = [[SLiMHaplotypeManager alloc] initWithClusteringMethod:clusterMethod
														   optimizationMethod:optMethod
															 sourceController:controller
																   sampleSize:sampleSize
														  clusterInBackground:YES];
	
	// Since we requested clusterInBackground:YES, the call above will return more or less immediately.  We can't
	// proceed further until the background clustering has completed.  When that happens, 
}

// This is the second half of configureForDisplayWithSlimWindowController:, effectively;
// it is called by SLiMWindowController after the background clustering has finished.
- (void)configurePlotWindowWithSlimWindowController:(SLiMWindowController *)controller
{
	// Configure our window
	NSWindow *window = [self window];
	
	[window setTitle:[NSString stringWithFormat:@"Haplotype snapshot (%@)", [haplotypeManager titleString]]];
	
	NSRect fittedRect = [self fittedRectUnderChromosomeView:controller->chromosomeZoomed inWindow:[controller window]];
	
	if (fittedRect.size.width < 400)
		fittedRect.size.width = 400;
	if (fittedRect.size.height < 200)
		fittedRect.size.height = 200;
	
	[window setFrame:fittedRect display:NO];
	[window setMinSize:NSMakeSize(400, 200)];
}

- (NSRect)fittedRectUnderChromosomeView:(NSView *)chromosomeView inWindow:(NSWindow *)window
{
	// This method makes a lot of assumptions about the sizes and widths of things, but if it is off, it shouldn't be by very much
	// Note screen coordinates start at lower left and increase as you go up and right
	NSRect cvInWindow = [chromosomeView convertRect:[chromosomeView bounds] toView:nil];
	NSRect cvOnScreen = [window convertRectToScreen:cvInWindow];
	NSRect windowOnScreen = [window frame];
	int remainingHeightInWindow = (int)(cvOnScreen.origin.y - windowOnScreen.origin.y);
	
	return NSMakeRect(windowOnScreen.origin.x + 10, cvOnScreen.origin.y - remainingHeightInWindow, cvOnScreen.size.width + 6, remainingHeightInWindow - 5);
}

- (NSUInteger)drawInVertexBuffer
{
	NSRect bounds = [self bounds];
	NSRect interior = NSInsetRect(bounds, 1, 1);
	
	// We render in two passes: first we add up how many triangles we need to fill, and then we get a vertex buffer
	// and define the triangles.  If this proves inefficient, we can cache work between the two stages, perhaps.
	NSUInteger rectCount = 0;	// accurate only at the end of pass 0 / beginning of pass 1; pass 1 is not required to count correctly
	NSUInteger vertexCount = 0;	// accurate at the end of pass 1
	
	for (int pass = 0; pass <= 1; ++pass)
	{
		id<MTLBuffer> buffer = nil;
		SLiMFlatVertex *vbPtr = NULL;
		
		if (pass == 1)
		{
			// calculate the vertex buffeer size needed, and get a buffer at least that large
			vertexCount = rectCount * 2 * 3;						// 2 triangles per rect, 3 vertices per triangle
			buffer = [self takeVertexBufferWithCapacity:vertexCount * sizeof(SLiMFlatVertex)];	// sizeof() includes data alignment padding
			
			// Now we can get our pointer into the vertex buffer
			vbPtr = (SLiMFlatVertex *)[buffer contents];
		}
		
		SLiMFlatVertex **vbPtrMod = (vbPtr ? &vbPtr : NULL);
		
		// Frame in gray and clear to black
		static simd::float4 fillColor = {0.5f, 0.5f, 0.5f, 1.0f};
		
		rectCount++;
		if (pass == 1)
			slimMetalFillNSRect(bounds, &fillColor, vbPtrMod);
		
		// Draw haplotypes by delegating to our haplotype manager
		rectCount += [haplotypeManager metalDrawHaplotypesInRect:interior displayBlackAndWhite:[self displayBlackAndWhite] showSubpopStrips:[self showSubpopulationStrips] eraseBackground:YES previousFirstBincounts:NULL withBuffer:vbPtrMod];
		
		if (pass == 1)
		{
			// Check that we added the planned number of vertices
			if (vbPtr != (SLiMFlatVertex *)[buffer contents] + vertexCount)
				NSLog(@"vertex count overflow in drawInVertexBuffer; expected %ld, drew %ld!", vertexCount, vbPtr - (SLiMFlatVertex *)[buffer contents]);
			
			// Since the vertex count can be an overestimate, return the realized vertex count
			vertexCount = vbPtr - (SLiMFlatVertex *)[buffer contents];
		}
	}
	
	return vertexCount;
}

- (void)drawWithRenderEncoder:(id<MTLRenderCommandEncoder>_Nonnull)renderEncoder
{
	[renderEncoder setRenderPipelineState:flatPipelineState_OriginBottom_];
	
	NSUInteger vertexCount = [self drawInVertexBuffer];
	
	[renderEncoder setVertexBuffer:vertexBuffers_[currentBuffer_] offset:0 atIndex:SLiMVertexInputIndexVertices];
	[renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertexCount];
}

- (IBAction)copy:(id)sender
{
	NSRect bounds = [self bounds];
	NSRect interior = NSInsetRect(bounds, 1, 1);
	NSBitmapImageRep *imageRep = [haplotypeManager bitmapImageRepForPlotInRect:interior displayBlackAndWhite:[self displayBlackAndWhite] showSubpopStrips:[self showSubpopulationStrips]];
	NSData *data = [imageRep representationUsingType:NSBitmapImageFileTypeJPEG properties:[NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithDouble:1.0], NSImageCompressionFactor, nil]];
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	
	[pasteboard declareTypes:[NSArray arrayWithObject:@"public.jpeg"] owner:nil];
	[pasteboard setData:data forType:@"public.jpeg"];
}

- (IBAction)saveGraph:(id)sender
{
	NSRect bounds = [self bounds];
	NSRect interior = NSInsetRect(bounds, 1, 1);
	NSBitmapImageRep *imageRep = [haplotypeManager bitmapImageRepForPlotInRect:interior displayBlackAndWhite:[self displayBlackAndWhite] showSubpopStrips:[self showSubpopulationStrips]];
	NSData *data = [imageRep representationUsingType:NSBitmapImageFileTypeJPEG properties:[NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithDouble:1.0], NSImageCompressionFactor, nil]];
	
	//SLiMWindowController *controller = [self slimWindowController];
	NSSavePanel *sp = [[NSSavePanel savePanel] retain];
	
	[sp setTitle:@"Export Plot"];
	[sp setNameFieldLabel:@"Export As:"];
	[sp setMessage:@"Export the plot to a file:"];
	[sp setExtensionHidden:NO];
	[sp setCanSelectHiddenExtension:NO];
	[sp setAllowedFileTypes:@[@"jpg"]];
	
	[sp beginSheetModalForWindow:[self window] completionHandler:^(NSInteger result) {
		if (result == NSFileHandlingPanelOKButton)
		{
			[data writeToURL:[sp URL] atomically:YES];
			
			[sp autorelease];
		}
	}];
}

- (IBAction)toggleDisplayBW:(id)sender
{
	[self setDisplayBlackAndWhite:![self displayBlackAndWhite]];
	[self setNeedsDisplay:YES];
}

- (IBAction)toggleSubpopStrips:(id)sender
{
	[self setShowSubpopulationStrips:![self showSubpopulationStrips]];
	[self setNeedsDisplay:YES];
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
	NSMenu *menu = [[NSMenu alloc] initWithTitle:@"plot_menu"];
	
	// Toggle BW vs. color
	{
		NSMenuItem *menuItem = [menu addItemWithTitle:([self displayBlackAndWhite] ? @"Display Colors" : @"Display Black & White") action:@selector(toggleDisplayBW:) keyEquivalent:@""];
		
		[menuItem setTarget:self];
	}
	
	[menu addItem:[NSMenuItem separatorItem]];
	
	// Toggle subpopulation strips
	{
		NSMenuItem *menuItem = [menu addItemWithTitle:([self showSubpopulationStrips] ? @"Hide Subpopulation Strips" : @"Show Subpopulation Strips") action:@selector(toggleSubpopStrips:) keyEquivalent:@""];
		
		[menuItem setTarget:self];
	}
	
	[menu addItem:[NSMenuItem separatorItem]];
	
	// Copy/export the plot
	{
		NSMenuItem *menuItem;
		
		menuItem = [menu addItemWithTitle:@"Copy Plot" action:@selector(copy:) keyEquivalent:@""];
		[menuItem setTarget:self];
		
		menuItem = [menu addItemWithTitle:@"Export Plot..." action:@selector(saveGraph:) keyEquivalent:@""];
		[menuItem setTarget:self];
	}
	
	return [menu autorelease];
}

@end




























