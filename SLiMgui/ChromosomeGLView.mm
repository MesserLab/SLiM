//
//  ChromosomeGLView.m
//  SLiM
//
//  Created by Ben Haller on 12/5/16.
//  Copyright (c) 2016-2020 Philipp Messer.  All rights reserved.
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


#import "ChromosomeGLView.h"
#import "ChromosomeView.h"

#import <OpenGL/OpenGL.h>
#include <OpenGL/glu.h>
#include <GLKit/GLKMatrix4.h>


@implementation ChromosomeGLView

- (void)dealloc
{
	//NSLog(@"[ChromosomeGLView dealloc]");
	[self setDelegate:nil];
	
	[super dealloc];
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
	
    // Update the viewport; using backingBounds here instead of bounds makes the view hi-res-aware,
    // while still remaining point-based since the ortho projection we use below uses bounds.
    NSRect backingBounds = [self convertRectToBacking:bounds];
    
    glViewport(0, 0, (int)backingBounds.size.width, (int)backingBounds.size.height);
    
	// Update the projection
	glMatrixMode(GL_PROJECTION);
	GLKMatrix4 orthoMat = GLKMatrix4MakeOrtho(0.0, (int)bounds.size.width, 0.0, (int)bounds.size.height, -1.0f, 1.0f);
	glLoadMatrixf(orthoMat.m);
	glMatrixMode(GL_MODELVIEW);
	
	// Have our delegate do the actual drawing
	if (_delegate)
		[_delegate glDrawRect:rect];
	
	// Flush
	[[self openGLContext] flushBuffer];
}

- (NSMenu *)menuForEvent:(NSEvent *)event
{
	return [[self superview] menuForEvent:event];
}

@end






























