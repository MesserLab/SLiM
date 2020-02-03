//
//  PopulationMetalView.metal
//  SLiMgui
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


#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;


// Include header shared between this Metal shader code and C code executing Metal API commands.
#import "PopulationMetalView_Shared.h"


//
//	Flat vertex and fragment functions
//

// Vertex shader outputs and fragment shader inputs
typedef struct
{
    float4 position [[position]];	// the clip space position of the vertex as returned by the vertex function
	float4 color [[flat]];			// the color of the first vertex is used for all vertices, avoiding interpolation
} FlatRasterizerData;

vertex FlatRasterizerData flatVertexShader(const uint vertexID [[vertex_id]],
										   const device PopViewFlatVertex *vertices [[buffer(PopViewVertexInputIndexVertices)]],
										   constant float2 *viewportSizePointer [[buffer(PopViewVertexInputIndexViewportSize)]])
{
    FlatRasterizerData out;

    // Get the currect vertex position in pixel space, with (0,0) at top-left
    float2 pixelSpacePosition = vertices[vertexID].position.xy;

    // Get the viewport size
    float2 viewportSize = *viewportSizePointer;
    
    // Convert from pixel space, with (0,0) at top-left, to clip space
    out.position = float4(0.0, 0.0, 0.0, 1.0);
    out.position.x = pixelSpacePosition.x / (viewportSize.x / 2.0) - 1.0;
    out.position.y = 1.0 - pixelSpacePosition.y / (viewportSize.y / 2.0);
	
    // Pass the input color directly to the rasterizer.
    out.color = vertices[vertexID].color;

    return out;
}

fragment float4 flatFragmentShader(FlatRasterizerData in [[stage_in]])
{
    // Return the (un)interpolated color.
    return in.color;
}


//
//	Textured vertex and fragment functions
//

typedef struct
{
    float4 position [[position]];	// the clip space position of the vertex as returned by the vertex function
    float2 textureCoordinate;		// this will be interpolated between vertices
} TexturedRasterizerData;

vertex TexturedRasterizerData texturedVertexShader(const uint vertexID [[vertex_id]],
												   const device PopViewTexturedVertex *vertices [[buffer(PopViewVertexInputIndexVertices)]],
												   constant float2 *viewportSizePointer [[buffer(PopViewVertexInputIndexViewportSize)]])
{
    TexturedRasterizerData out;

    // Get the currect vertex position in pixel space, with (0,0) at top-left
    float2 pixelSpacePosition = vertices[vertexID].position.xy;

    // Get the viewport size
    float2 viewportSize = *viewportSizePointer;
    
    // Convert from pixel space, with (0,0) at top-left, to clip space
    out.position = float4(0.0, 0.0, 0.0, 1.0);
    out.position.x = pixelSpacePosition.x / (viewportSize.x / 2.0) - 1.0;
    out.position.y = 1.0 - pixelSpacePosition.y / (viewportSize.y / 2.0);
	
    // Pass the input textureCoordinate directly to the rasterizer
    out.textureCoordinate = vertices[vertexID].textureCoordinate;

    return out;
}

fragment float4 texturedFragmentShader(TexturedRasterizerData in [[stage_in]],
									   texture2d<half> colorTexture [[ texture(PopViewTextureIndexBaseColor) ]])
{
    constexpr sampler textureSampler (mag_filter::linear, min_filter::linear);

    // Sample the texture to obtain a color
    const half4 colorSample = colorTexture.sample(textureSampler, in.textureCoordinate);

    // return the color of the texture
    return float4(colorSample);
}




















