//
//  ChromosomeGLView.m
//  SLiM
//
//  Created by Ben Haller on 12/5/16.
//  Copyright (c) 2015-2017 Philipp Messer.  All rights reserved.
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

// Called after being summoned from a NIB/XIB.
- (void)prepareOpenGL
{
}

// Adjust the OpenGL viewport in response to window resize
- (void)reshape
{
}

- (void)drawRect:(NSRect)rect
{
	NSRect bounds = [self bounds];
	
	// Update the viewport
	glViewport(0, 0, (int)bounds.size.width, (int)bounds.size.height);
	
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

@end






























