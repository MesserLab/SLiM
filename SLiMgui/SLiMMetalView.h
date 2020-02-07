//
//  SLiMMetalView.h
//  SLiMgui
//
//  Created by Ben Haller on 2/5/20.
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


#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>	// I think this header perhaps has the side effect of turning on required nullability declarations? annoying...
#import <simd/simd.h>

#import "MetalView_Shared.h"


// The maximum number of frames in flight; see Apple's "CPU-GPU Synchronization" example for explanation of this design
#define SLiM_MaxFramesInFlight	3

// A function that can be used from C++ to dispose of a texture
void TextureFreeFUN(void * _Nullable texture);


@interface SLiMMetalView : MTKView
{
	@protected
	// The Metal device (i.e., GPU) that we are rendering to; this can change!
    id<MTLDevice> device_;

    // Render pipelines generated from the vertex and fragment shaders in the .metal shader file.
    id<MTLRenderPipelineState> flatPipelineState_OriginTop_;
    id<MTLRenderPipelineState> flatPipelineState_OriginBottom_;
    id<MTLRenderPipelineState> texturedPipelineState_;

    // The command queue used to pass commands to the device.
    id<MTLCommandQueue> commandQueue_;
	
    // A semaphore used to ensure that buffers read by the GPU are not simultaneously written by the CPU.
    dispatch_semaphore_t inFlightSemaphore_;
	
	// The vertex buffers we use for rendering; ensure it has sufficient capacity before using it, with takeVertexBufferWithCapacity:
	id<MTLBuffer> vertexBuffers_[SLiM_MaxFramesInFlight];

    // The index of the Metal buffer in vertexBuffers_ to write to for the current frame.
    NSUInteger currentBuffer_;
	
    // The current size of the view, in AppKit (i.e., visual not device) coordinates, used as an input to the vertex shader.
    simd::float2 viewportSize_;
}

- (nonnull instancetype)initWithFrame:(CGRect)frameRect device:(nullable id<MTLDevice>)device NS_DESIGNATED_INITIALIZER;
- (nonnull instancetype)initWithCoder:(nonnull NSCoder *)coder NS_DESIGNATED_INITIALIZER;

// These are methods subclasses may wish to override to get control at specific moments
- (void)completeInitialize;
- (void)releaseDeviceResources;
- (void)adaptToDevice:(id<MTLDevice>_Nullable)newDevice;
- (id<MTLBuffer>_Nonnull)takeVertexBufferWithCapacity:(NSUInteger)requestedCapacity;

// A small texture object for testing purposes
- (id<MTLTexture>_Nonnull)testTexture;

// Subclasses must override this to render in the renderEncoder that has been set up
- (void)drawWithRenderEncoder:(id<MTLRenderCommandEncoder>_Nonnull)renderEncoder;

// Subclasses will generally *not* override this; usually drawWithRenderEncoder: is sufficient
- (void)drawRect:(NSRect)dirtyRect;

@end


//	Metal rendering utility functions

inline void slimMetalFillRect(float x, float y, float w, float h, simd::float4 * _Nullable color, SLiMFlatVertex * _Nullable * _Nullable vbPtrMod)
{
	// This "draws" a rect in a vertex buffer by adding two triangles.  It assumes flat shading,
	// and so sets the fill color only on the first vertex of each triangle.  It advances the
	// buffer pointer by 6; the client must ensure that room exists in the buffer.
	SLiMFlatVertex *vbPtr = *vbPtrMod;
	
	vbPtr[0].position.x = x;
	vbPtr[0].position.y = y;
	vbPtr[0].color = *color;
	
	vbPtr[1].position.x = x;
	vbPtr[1].position.y = y + h;
	
	vbPtr[2].position.x = x + w;
	vbPtr[2].position.y = y;
	
	vbPtr[3].position.x = x + w;
	vbPtr[3].position.y = y + h;
	vbPtr[3].color = *color;
	
	vbPtr[4].position.x = x;
	vbPtr[4].position.y = y + h;
	
	vbPtr[5].position.x = x + w;
	vbPtr[5].position.y = y;
	
	*vbPtrMod = vbPtr + 6;
}

inline void slimMetalTextureRect(float x, float y, float w, float h, SLiMTexturedVertex * _Nullable * _Nullable vbPtrMod)
{
	// This "draws" a rect in a vertex buffer by adding two triangles.  It assumes
	// the rect should have a texture overlaid on it completely.  It advances the
	// buffer pointer by 6; the client must ensure that room exists in the buffer.
	SLiMTexturedVertex *vbPtr = *vbPtrMod;
	
	vbPtr[0].position.x = x;
	vbPtr[0].position.y = y;
	vbPtr[0].textureCoordinate.x = 0.0f;
	vbPtr[0].textureCoordinate.y = 0.0f;
	
	vbPtr[1].position.x = x;
	vbPtr[1].position.y = y + h;
	vbPtr[1].textureCoordinate.x = 0.0f;
	vbPtr[1].textureCoordinate.y = 1.0f;
	
	vbPtr[2].position.x = x + w;
	vbPtr[2].position.y = y;
	vbPtr[2].textureCoordinate.x = 1.0f;
	vbPtr[2].textureCoordinate.y = 0.0f;
	
	vbPtr[3].position.x = x + w;
	vbPtr[3].position.y = y + h;
	vbPtr[3].textureCoordinate.x = 1.0f;
	vbPtr[3].textureCoordinate.y = 1.0f;
	
	vbPtr[4].position.x = x;
	vbPtr[4].position.y = y + h;
	vbPtr[4].textureCoordinate.x = 0.0f;
	vbPtr[4].textureCoordinate.y = 1.0f;
	
	vbPtr[5].position.x = x + w;
	vbPtr[5].position.y = y;
	vbPtr[5].textureCoordinate.x = 1.0f;
	vbPtr[5].textureCoordinate.y = 0.0f;
	
	*vbPtrMod = vbPtr + 6;
}

inline void slimMetalFillNSRect(NSRect rect, simd::float4 * _Nullable color, SLiMFlatVertex * _Nullable * _Nullable vbPtrMod)
{
	slimMetalFillRect((float)rect.origin.x, (float)rect.origin.y, (float)rect.size.width, (float)rect.size.height, color, vbPtrMod);
}

inline void slimMetalTextureNSRect(NSRect rect, SLiMTexturedVertex * _Nullable * _Nullable vbPtrMod)
{
	slimMetalTextureRect((float)rect.origin.x, (float)rect.origin.y, (float)rect.size.width, (float)rect.size.height, vbPtrMod);
}

void slimMetalFrameNSRect(NSRect rect, simd::float4 * _Nullable color, SLiMFlatVertex * _Nullable * _Nullable vbPtrMod);
































