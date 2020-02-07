//
//  SLiMMetalView.m
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


#import "SLiMMetalView.h"

#import <MetalKit/MetalKit.h>
#import <simd/simd.h>
#import "MetalView_Shared.h"


// A function that can be used from C++ to dispose of a texture
void TextureFreeFUN(void *texture)
{
	id<MTLTexture> texture_id = (id<MTLTexture>)texture;
	[texture_id release];
}


@implementation SLiMMetalView

- (void)releaseDeviceResources
{
	[flatPipelineState_OriginTop_ release];
	flatPipelineState_OriginTop_ = nil;
	
	[flatPipelineState_OriginBottom_ release];
	flatPipelineState_OriginBottom_ = nil;
	
	[texturedPipelineState_ release];
	texturedPipelineState_ = nil;
	
	[commandQueue_ release];
	commandQueue_ = nil;
	
	[device_ release];
	device_ = nil;
	
	// note that we do NOT release the vertex buffers here, even though they are associated with the device,
	// because they might currently be in use by a command buffer; instead, whenever we are about to start
	// using a vertex buffer we check that it comes from the correct device, and reallocate it if not
}

- (void)dealloc
{
	// dispose of vertex buffers and inFlightSemaphore_; if they are still in use by a command buffer, it
	// will have a retain on them, so releasing them here is not a problem
	for (int i = 0; i < SLiM_MaxFramesInFlight; ++i)
	{
		[vertexBuffers_[i] release];
		vertexBuffers_[i] = nil;
	}
	
	dispatch_release(inFlightSemaphore_);
	
	// and then we can release everything else
	[self releaseDeviceResources];
	
	[super dealloc];
}

- (void)adaptToDevice:(id<MTLDevice>)newDevice
{
	NSAssert(newDevice, @"Nil device passed to adaptToDevice:");
	
	if (newDevice != device_)
	{
#if DEBUG
		if (device_ != nil)
			NSLog(@"adaptToDevice: ADAPTING TO NEW METAL DEVICE");
#endif
		
		// Release our old resources
		[self releaseDeviceResources];
		
		// Set ourselves to use the default device
		device_ = [newDevice retain];
		[self setDevice:device_];
		
		// Load all the shader files in the project, and find the functions we're using
		id<MTLLibrary> defaultLibrary = [device_ newDefaultLibrary];
		NSAssert(defaultLibrary, @"No default Metal library");
		
		// Configure a pipeline descriptor and a pipeline state for rendering flat polygons (with the origin at the top)
		{
			id<MTLFunction> flatVertexFunction = [defaultLibrary newFunctionWithName:@"flatVertexShader_OriginTop"];
			id<MTLFunction> flatFragmentFunction = [defaultLibrary newFunctionWithName:@"flatFragmentShader"];
			NSAssert(flatVertexFunction, @"No flatVertexShader");
			NSAssert(flatFragmentFunction, @"No flatFragmentShader");
			
			MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
			NSError *error = NULL;
			
			[pipelineStateDescriptor setLabel:@"PopulationMetalView Flat Pipeline"];
			[pipelineStateDescriptor setVertexFunction:flatVertexFunction];
			[pipelineStateDescriptor setFragmentFunction:flatFragmentFunction];
			[pipelineStateDescriptor setSampleCount:[self sampleCount]];
			[[pipelineStateDescriptor colorAttachments][0] setPixelFormat:[self colorPixelFormat]];
			if (@available(macOS 10.13, *)) {
				[[pipelineStateDescriptor vertexBuffers][SLiMVertexInputIndexVertices] setMutability:MTLMutabilityImmutable];
			} else {
				// No worries, just very slightly less efficient since Metal can't take advantage of the immutability of the buffer
			}
			
			flatPipelineState_OriginTop_ = [device_ newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
			NSAssert(flatPipelineState_OriginTop_, @"Failed to create flat pipeline state: %@", error);
			
			[flatVertexFunction release];
			[flatFragmentFunction release];
			[pipelineStateDescriptor release];
		}
		
		// Configure a pipeline descriptor and a pipeline state for rendering flat polygons (with the origin at the bottom)
		{
			id<MTLFunction> flatVertexFunction = [defaultLibrary newFunctionWithName:@"flatVertexShader_OriginBottom"];
			id<MTLFunction> flatFragmentFunction = [defaultLibrary newFunctionWithName:@"flatFragmentShader"];
			NSAssert(flatVertexFunction, @"No flatVertexShader");
			NSAssert(flatFragmentFunction, @"No flatFragmentShader");
			
			MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
			NSError *error = NULL;
			
			[pipelineStateDescriptor setLabel:@"PopulationMetalView Flat Pipeline"];
			[pipelineStateDescriptor setVertexFunction:flatVertexFunction];
			[pipelineStateDescriptor setFragmentFunction:flatFragmentFunction];
			[pipelineStateDescriptor setSampleCount:[self sampleCount]];
			[[pipelineStateDescriptor colorAttachments][0] setPixelFormat:[self colorPixelFormat]];
			if (@available(macOS 10.13, *)) {
				[[pipelineStateDescriptor vertexBuffers][SLiMVertexInputIndexVertices] setMutability:MTLMutabilityImmutable];
			} else {
				// No worries, just very slightly less efficient since Metal can't take advantage of the immutability of the buffer
			}
			
			flatPipelineState_OriginBottom_ = [device_ newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
			NSAssert(flatPipelineState_OriginBottom_, @"Failed to create flat pipeline state: %@", error);
			
			[flatVertexFunction release];
			[flatFragmentFunction release];
			[pipelineStateDescriptor release];
		}
		
		// Configure a pipeline descriptor and a pipeline state for rendering textured polygons
		{
			id<MTLFunction> texturedVertexFunction = [defaultLibrary newFunctionWithName:@"texturedVertexShader"];
			id<MTLFunction> texturedFragmentFunction = [defaultLibrary newFunctionWithName:@"texturedFragmentShader"];
			NSAssert(texturedVertexFunction, @"No texturedVertexShader");
			NSAssert(texturedFragmentFunction, @"No texturedFragmentShader");
			
			MTLRenderPipelineDescriptor *pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
			NSError *error = NULL;
			
			[pipelineStateDescriptor setLabel:@"PopulationMetalView Textured Pipeline"];
			[pipelineStateDescriptor setVertexFunction:texturedVertexFunction];
			[pipelineStateDescriptor setFragmentFunction:texturedFragmentFunction];
			[pipelineStateDescriptor setSampleCount:[self sampleCount]];
			[[pipelineStateDescriptor colorAttachments][0] setPixelFormat:[self colorPixelFormat]];
			if (@available(macOS 10.13, *)) {
				[[pipelineStateDescriptor vertexBuffers][SLiMVertexInputIndexVertices] setMutability:MTLMutabilityImmutable];
			} else {
				// No worries, just very slightly less efficient since Metal can't take advantage of the immutability of the buffer
			}
			
			texturedPipelineState_ = [device_ newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
			NSAssert(texturedPipelineState_, @"Failed to create textured pipeline state: %@", error);
			
			[texturedVertexFunction release];
			[texturedFragmentFunction release];
			[pipelineStateDescriptor release];
		}
		
		[defaultLibrary release];
		
		// Create the command queue
		commandQueue_ = [device_ newCommandQueue];
		NSAssert(commandQueue_, @"No command queue");
		
		// Vertex buffers will be created, or fixed to belong to our new device, on demand later
	}
}

- (void)completeInitialize
{
	// Metal setup
	[self setPaused:YES];
	[self setEnableSetNeedsDisplay:YES];
	
	id<MTLDevice> defaultDevice = MTLCreateSystemDefaultDevice();
	NSAssert(defaultDevice, @"Metal is not supported on this device");
	
	[self adaptToDevice:defaultDevice];
	[defaultDevice release];
	
	// Make our semaphore to manage our set of vertex buffers (which will be created on demand).
	// We start it at SLiM_MaxFramesInFlight, to indicate that all buffers are available for use.
	inFlightSemaphore_ = dispatch_semaphore_create(SLiM_MaxFramesInFlight);
}

- (nonnull instancetype)initWithFrame:(CGRect)frameRect device:(nullable id<MTLDevice>)device
{
	if (self = [super initWithFrame:frameRect device:device])
	{
		[self completeInitialize];
	}
	
	return self;
}

- (nonnull instancetype)initWithCoder:(nonnull NSCoder *)coder
{
	if (self = [super initWithCoder:coder])
	{
		[self completeInitialize];
	}
	
	return self;
}

- (id<MTLBuffer>)takeVertexBufferWithCapacity:(NSUInteger)requestedCapacity
{
    // Wait to ensure only `SLiM_MaxFramesInFlight` number of frames are getting processed
    // by any stage in the Metal pipeline (CPU, GPU, Metal, Drivers, etc.).
    long dispatch_result = dispatch_semaphore_wait(inFlightSemaphore_, 0);		// 0 is DISPATCH_WALLTIME_NOW, but that is 10.14+
	
	if (dispatch_result != 0)
	{
		// This is for debugging purposes, to see whether we ever stall waiting for a buffer
#if DEBUG
		NSLog(@"stalled waiting for a vertex buffer to land...");
#endif
		dispatch_semaphore_wait(inFlightSemaphore_, DISPATCH_TIME_FOREVER);
	}
	
    // Iterate through the Metal buffers, and cycle back to the first when you've written to the last
    currentBuffer_ = (currentBuffer_ + 1) % SLiM_MaxFramesInFlight;
	
	// Make sure the vertex buffer we intend to use belongs to the current device
	id<MTLBuffer> buffer = vertexBuffers_[currentBuffer_];
	
	if ([buffer device] != device_)
	{
		[buffer release];
		buffer = nil;
	}
	
	// And now ensure that it fits the requested capacity
	NSUInteger oldCapacity = [buffer length];
	
	if ((requestedCapacity > oldCapacity) || (oldCapacity == 0))
	{
		// First, ditch the old buffer
		[buffer release];
		buffer = nil;
		
		// Decide on a new capacity: enforce a minimum of 10,000 vertices, otherwise
		// at least double in capacity whenever we expand, to avoid excessive reallocs
		NSUInteger newCapacity;
		
		if (requestedCapacity < 10000 * sizeof(SLiMFlatVertex))
			newCapacity = 10000 * sizeof(SLiMFlatVertex);
		else if (requestedCapacity < oldCapacity * 2)
			newCapacity = oldCapacity * 2;
		else
			newCapacity = requestedCapacity;
		
#if DEBUG
		if (oldCapacity > 0)
			NSLog(@"REALLOCATING buffer #%ld to %ld bytes", (unsigned long)currentBuffer_, (unsigned long)newCapacity);
#endif
		
		// Allocate the new buffer; note that we guarantee we will never read from the vertex buffer!
		buffer = [device_ newBufferWithLength:newCapacity options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
		[buffer setLabel:[NSString stringWithFormat:@"PopulationMetalView Vertex Buffer #%lu", (unsigned long)currentBuffer_]];
		
		vertexBuffers_[currentBuffer_] = buffer;
	}
	
	return buffer;
}

- (id<MTLTexture>)testTexture
{
	// This constructs a small texture for testing purposes; it is not used in the production code
	static const int width = 4;
	static const int height = 4;
	static const uint32_t bgra8data[width * height] = {		// 0xAARRGGBB, which is confusingly called BGRA; AA seems to be unused
		0xFF0000FF, 0xFF0000FF, 0xFF0000FF, 0xFF0000FF,
		0xFFFF00FF, 0xFF0000FF, 0xFF0000FF, 0xFF00FFFF,
		0x00FF00FF, 0xFF0000FF, 0xFF0000FF, 0x0000FFFF,
		0x000000FF, 0xFF000000, 0xFF000000, 0x000000FF,
	};
	
    MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
    
    // Indicate that each pixel has a blue, green, red, and alpha channel, where each channel is
    // an 8-bit unsigned normalized value (i.e. 0 maps to 0.0 and 255 maps to 1.0)
    textureDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
    
    // Set the pixel dimensions of the texture
    textureDescriptor.width = width;
    textureDescriptor.height = height;
    
    // Create the texture from the device by using the descriptor
    id<MTLTexture> texture = [device_ newTextureWithDescriptor:textureDescriptor];
	[textureDescriptor release];
    
    // Calculate the number of bytes per row in the image.
    NSUInteger bytesPerRow = 4 * width;
    
    MTLRegion region = {
        { 0, 0, 0 },        // MTLOrigin
        {width, height, 1} 	// MTLSize
    };
    
    // Copy the bytes from the data object into the texture
    [texture replaceRegion:region mipmapLevel:0 withBytes:bgra8data bytesPerRow:bytesPerRow];
	
    return [texture autorelease];
}

- (void)drawWithRenderEncoder:(id<MTLRenderCommandEncoder>)renderEncoder
{
}

- (void)drawRect:(NSRect)dirtyRect
{
	// Check that we are using our preferred device, and switch if necessary
	NSNumber *screenNumber = [[[self window] screen] deviceDescription][@"NSScreenNumber"];
	CGDirectDisplayID viewDisplayID = [screenNumber unsignedIntValue];
	id <MTLDevice> preferredDevice = CGDirectDisplayCopyCurrentMetalDevice(viewDisplayID);
	
	if (preferredDevice != device_)
		[self adaptToDevice:preferredDevice];
	
	[preferredDevice release];
	
    // Save the size of the drawable to pass to the vertex shader for scaling; this is in AppKit coordinates,
	// not device coordinates, so that our drawing looks the same whether we're on Retina or not; on Retina
	// one "drawing pixel" will map to more than one "device pixel" because of this setup
	CGSize drawableSize = [self drawableSize];
	CGSize frameSize = [self frame].size;
	
	viewportSize_.x = (float)frameSize.width;
    viewportSize_.y = (float)frameSize.height;
	
    // Obtain a renderPassDescriptor generated from the view's drawable textures.
    MTLRenderPassDescriptor *renderPassDescriptor = [self currentRenderPassDescriptor];

    if (renderPassDescriptor == nil)
	{
#if DEBUG
		NSLog(@"drawRect: no renderPassDescriptor, skipping frame...");
#endif
	}
	else
    {
		// Set a background color of magenta, which we use to detect drawing coverage problems
		renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(1, 0, 1, 1);
		
		// Create a new command buffer for each render pass to the current drawable.
		id<MTLCommandBuffer> commandBuffer = [commandQueue_ commandBuffer];
		[commandBuffer setLabel:@"PopulationMetalViewCommandBuffer"];
		NSAssert(commandBuffer, @"drawRect: No commandBuffer");

		// Create a render command encoder.
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [renderEncoder setLabel:@"PopulationMetalViewRenderEncoder"];
		NSAssert(renderEncoder, @"drawRect: No renderEncoder");

        // Set the region of the drawable to draw into; this is in device coordinates as defined by the drawable
		// We use depth for layering: the background fills at are depth 1.0, spatial maps at 0.5, and individuals at 0.0
        [renderEncoder setViewport:(MTLViewport){0.0, 0.0, drawableSize.width, drawableSize.height, 0.0, 1.0}];
		[renderEncoder setVertexBytes:&viewportSize_ length:sizeof(viewportSize_) atIndex:SLiMVertexInputIndexViewportSize];
		
		[self drawWithRenderEncoder:renderEncoder];
		
		// End encoding now that we're done with all rendering
        [renderEncoder endEncoding];

        // Schedule a present once the framebuffer is complete using the current drawable.
        [commandBuffer presentDrawable:[self currentDrawable]];
		
		// Add a completion handler that signals `inFlightSemaphore_` when Metal and the GPU have fully
		// finished processing the commands that were encoded for this frame.
		// This completion indicates that the dynamic buffers that were written to in this frame are no
		// longer needed by Metal and the GPU; therefore, the CPU can overwrite the buffer contents
		// without corrupting any rendering operations.  Note that this block retains inFlightSemaphore_,
		// and the command buffer retains the vertex buffer that it's using.
		__block dispatch_semaphore_t block_semaphore = inFlightSemaphore_;
		[commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) {
			dispatch_semaphore_signal(block_semaphore);
		}];
		
		// Finalize rendering here & push the command buffer to the GPU.
		[commandBuffer commit];
    }
}

@end


//	Metal rendering utility functions

void slimMetalFrameNSRect(NSRect rect, simd::float4 *color, SLiMFlatVertex **vbPtrMod)
{
	float ox = (float)rect.origin.x, oy = (float)rect.origin.y;
	float h = (float)rect.size.height, w = (float)rect.size.width;
	
	slimMetalFillRect(ox, oy, 1, h, color, vbPtrMod);
	slimMetalFillRect(ox + 1, oy, w - 2, 1, color, vbPtrMod);
	slimMetalFillRect(ox + w - 1, oy, 1, h, color, vbPtrMod);
	slimMetalFillRect(ox + 1, oy + h - 1, w - 2, 1, color, vbPtrMod);
}


































