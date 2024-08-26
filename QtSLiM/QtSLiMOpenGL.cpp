//
//  QtSLiMOpenGL.cpp
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


#include "QtSLiMOpenGL.h"

#include <cstdlib>


float *glArrayVertices = nullptr;
float *glArrayColors = nullptr;

void QtSLiM_AllocateGLBuffers(void)
{
    if (!glArrayVertices)
        glArrayVertices = static_cast<float *>(malloc(kMaxVertices * 2 * sizeof(float)));		// 2 floats per vertex, kMaxVertices vertices
    
    if (!glArrayColors)
        glArrayColors = static_cast<float *>(malloc(kMaxVertices * 4 * sizeof(float)));		// 4 floats per color, kMaxVertices colors
}

void QtSLiM_FreeGLBuffers(void)
{
    if (glArrayVertices)
    {
        free(glArrayVertices);
        glArrayVertices = nullptr;
    }
    
    if (glArrayColors)
    {
        free(glArrayColors);
        glArrayColors = nullptr;
    }
}





















