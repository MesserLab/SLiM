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

#import <OpenGL/OpenGL.h>
#include <OpenGL/glu.h>
#include <GLKit/GLKMatrix4.h>


@implementation SLiMHaplotypeGraphView

- (instancetype)initWithCoder:(NSCoder *)coder
{
	if (self = [super initWithCoder:coder])
	{
	}
	
	return self;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
	if (self = [super initWithFrame:frameRect])
	{
	}
	
	return self;
}

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

/*
// BCH 4/19/2019: these subclass methods are not needed, and are generating a warning in Xcode 10.2, so I'm removing them
 
// Called after being summoned from a NIB/XIB.
- (void)prepareOpenGL
{
}

// Adjust the OpenGL viewport in response to window resize
- (void)reshape
{
}
*/

- (void)drawRect:(NSRect)rect
{
	NSRect bounds = [self bounds];
	NSRect interior = NSInsetRect(bounds, 1, 1);
	
	// Update the viewport; using backingBounds here instead of bounds makes the view hi-res-aware,
	// while still remaining point-based since the ortho projection we use below uses bounds.
	NSRect backingBounds = [self convertRectToBacking:bounds];
	
	glViewport(0, 0, (int)backingBounds.size.width, (int)backingBounds.size.height);
	
	// Update the projection
	glMatrixMode(GL_PROJECTION);
	GLKMatrix4 orthoMat = GLKMatrix4MakeOrtho(0.0, (int)bounds.size.width, 0.0, (int)bounds.size.height, -1.0f, 1.0f);
	//GLKMatrix4 orthoMat = GLKMatrix4MakeOrtho(0.0, (int)bounds.size.width, (int)bounds.size.height, 0.0, -1.0f, 1.0f);
	glLoadMatrixf(orthoMat.m);
	glMatrixMode(GL_MODELVIEW);
	
	// Frame in gray and clear to black
	glColor3f(0.5f, 0.5f, 0.5f);
	glRecti(0, 0, (int)bounds.size.width, (int)bounds.size.height);
	
	// Draw haplotypes by delegating to our haplotype manager
	[haplotypeManager glDrawHaplotypesInRect:interior displayBlackAndWhite:[self displayBlackAndWhite] showSubpopStrips:[self showSubpopulationStrips] eraseBackground:YES previousFirstBincounts:NULL];
	
	// Flush
	[[self openGLContext] flushBuffer];
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




























