//
//  QtSLiMOpenGL.h
//  SLiM
//
//  Created by Ben Haller on 8/25/2024.
//  Copyright (c) 2024 Philipp Messer.  All rights reserved.
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

#ifndef QTSLIMOPENGL_H
#define QTSLIMOPENGL_H


/*
 * This header defines utility macros and functions for drawing with OpenGL.
 * This should be included only locally within OpenGL-specific rendering code.
 */


// OpenGL buffers for rendering; shared across all rendering code
// These buffers are allocated by QtSLiMAppDelegate at launch
#define kMaxGLRects 4000				// 4000 rects
#define kMaxVertices (kMaxGLRects * 4)	// 4 vertices each

extern float *glArrayVertices;
extern float *glArrayColors;

extern void QtSLiM_AllocateGLBuffers(void);
extern void QtSLiM_FreeGLBuffers(void);


#define SLIM_GL_PREPARE()										\
	int displayListIndex = 0;									\
	float *vertices = glArrayVertices, *colors = glArrayColors;	\
																\
	glEnableClientState(GL_VERTEX_ARRAY);						\
	glVertexPointer(2, GL_FLOAT, 0, glArrayVertices);			\
	glEnableClientState(GL_COLOR_ARRAY);						\
	glColorPointer(4, GL_FLOAT, 0, glArrayColors);

#define SLIM_GL_DEFCOORDS(rect)                                 \
	float left = static_cast<float>(rect.left());               \
	float top = static_cast<float>(rect.top());                 \
	float right = left + static_cast<float>(rect.width());      \
	float bottom = top + static_cast<float>(rect.height());

#define SLIM_GL_PUSHRECT()										\
	*(vertices++) = left;										\
	*(vertices++) = top;										\
	*(vertices++) = left;										\
	*(vertices++) = bottom;										\
	*(vertices++) = right;										\
	*(vertices++) = bottom;										\
	*(vertices++) = right;										\
	*(vertices++) = top;

#define SLIM_GL_PUSHRECT_COLORS()								\
	for (int j = 0; j < 4; ++j)									\
	{															\
		*(colors++) = colorRed;									\
		*(colors++) = colorGreen;								\
		*(colors++) = colorBlue;								\
		*(colors++) = colorAlpha;								\
	}

#define SLIM_GL_CHECKBUFFERS()									\
	displayListIndex++;											\
																\
	if (displayListIndex == kMaxGLRects)						\
	{															\
		glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);		\
																\
		vertices = glArrayVertices;								\
		colors = glArrayColors;									\
		displayListIndex = 0;									\
	}

#define SLIM_GL_FINISH()										\
	if (displayListIndex)										\
	glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);			\
																\
	glDisableClientState(GL_VERTEX_ARRAY);						\
	glDisableClientState(GL_COLOR_ARRAY);


#endif // QTSLIMOPENGL_H




















