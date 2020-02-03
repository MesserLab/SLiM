//
//  PopulationMetalView_Shared.h
//  SLiM
//
//  Created by Ben Haller on 1/31/20.
//  Copyright (c) 2020 Philipp Messer.  All rights reserved.
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

#ifndef PopulationMetalView_Shared_h
#define PopulationMetalView_Shared_h

#include <simd/simd.h>


// Buffer index values shared between shader and C code
typedef enum PopViewVertexInputIndex
{
    PopViewVertexInputIndexVertices     = 0,
    PopViewVertexInputIndexViewportSize = 1,
} PopViewVertexInputIndex;

// Texture index values shared between shader and C code
typedef enum PopViewTextureIndex
{
    PopViewTextureIndexBaseColor = 0,
} PopViewTextureIndex;

// Vertex structure layouts, shared between shader and C code
typedef struct
{
    simd::float2 position;
    simd::float4 color;
} PopViewFlatVertex;

typedef struct
{
    simd::float2 position;
    simd::float2 textureCoordinate;
} PopViewTexturedVertex;


#endif /* PopulationMetalView_Shared_h */





































