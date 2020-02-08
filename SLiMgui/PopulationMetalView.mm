//
//  PopulationMetalView.m
//  SLiMgui
//
//  Created by Ben Haller on 1/30/20.
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


#import "PopulationMetalView.h"
#import "SLiMWindowController.h"


typedef struct {
	int backgroundType;				// 0 == black, 1 == gray, 2 == white, 3 == named spatial map; if no preference has been set, no entry will exist
	std::string spatialMapName;		// the name of the spatial map chosen, for backgroundType == 3
} PopulationViewBackgroundSettings;


@implementation PopulationMetalView
{
	// display mode: 0 == individuals (non-spatial), 1 == individuals (spatial)
	int displayMode;
	
	// display background preferences, kept indexed by subpopulation id
	std::map<slim_objectid_t, PopulationViewBackgroundSettings> backgroundSettings;
	slim_objectid_t lastContextMenuSubpopID;
	
	// subview tiling, kept indexed by subpopulation id
	std::map<slim_objectid_t, NSRect> subpopTiles;
}

- (void)completeInitialize
{
	// Display setup
	displayMode = -1;	// don't know yet whether the model is spatial or not, which will determine our initial choice
	
	[super completeInitialize];
}

- (NSUInteger)drawIndividualsFromSubpopulation:(Subpopulation *)subpop inArea:(NSRect)bounds withBuffer:(SLiMFlatVertex **)vbPtrMod
{
	//
	//	NOTE this code is parallel to the code in canDisplayIndividualsFromSubpopulation:inArea: and should be maintained in parallel
	//
	
	SLiMWindowController *controller = [[self window] windowController];
	double scalingFactor = controller->fitnessColorScale;
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	int squareSize, viewColumns = 0, viewRows = 0;
	double subpopFitnessScaling = subpop->last_fitness_scaling_;
	
	if ((subpopFitnessScaling <= 0.0) || !std::isfinite(subpopFitnessScaling))
		subpopFitnessScaling = 1.0;
	
	// first figure out the biggest square size that will allow us to display the whole subpopulation
	for (squareSize = 20; squareSize > 1; --squareSize)
	{
		viewColumns = (int)floor((bounds.size.width - 3) / squareSize);
		viewRows = (int)floor((bounds.size.height - 3) / squareSize);
		
		if (viewColumns * viewRows > subpopSize)
		{
			// If we have an empty row at the bottom, then break for sure; this allows us to look nice and symmetrical
			if ((subpopSize - 1) / viewColumns < viewRows - 1)
				break;
			
			// Otherwise, break only if we are getting uncomfortably small; otherwise, let's drop down one square size to allow symmetry
			if (squareSize <= 5)
				break;
		}
	}
	
	if (squareSize > 1)
	{
		if (vbPtrMod)
		{
			// Convert square area to space between squares if possible
			int squareSpacing = 0;
			
			if (squareSize > 2)
			{
				--squareSize;
				++squareSpacing;
			}
			if (squareSize > 5)
			{
				--squareSize;
				++squareSpacing;
			}
			
			double excessSpaceX = bounds.size.width - ((squareSize + squareSpacing) * viewColumns - squareSpacing);
			double excessSpaceY = bounds.size.height - ((squareSize + squareSpacing) * viewRows - squareSpacing);
			int offsetX = (int)floor(excessSpaceX / 2.0);
			int offsetY = (int)floor(excessSpaceY / 2.0);
			
			// If we have an empty row at the bottom, then we can use the same value for offsetY as for offsetX, for symmetry
			if ((subpopSize - 1) / viewColumns < viewRows - 1)
				offsetY = offsetX;
			
			NSRect individualArea = NSMakeRect(bounds.origin.x + offsetX, bounds.origin.y + offsetY, bounds.size.width - offsetX, bounds.size.height - offsetY);
			
			for (int individualArrayIndex = 0; individualArrayIndex < subpopSize; ++individualArrayIndex)
			{
				// Figure out the rect to draw in; note we now use individualArrayIndex here, because the hit-testing code doesn't have an easy way to calculate the displayed individual index...
				float left = (float)(individualArea.origin.x + (individualArrayIndex % viewColumns) * (squareSize + squareSpacing));
				float top = (float)(individualArea.origin.y + (individualArrayIndex / viewColumns) * (squareSize + squareSpacing));
				float r = 0.3f, g = 0.3f, b = 0.3f;		// dark gray default, for a fitness of NaN; should never happen
				
				Individual &individual = *subpop->parent_individuals_[individualArrayIndex];
				
				if (Individual::s_any_individual_color_set_ && !individual.color_.empty())
				{
					r = individual.color_red_;
					g = individual.color_green_;
					b = individual.color_blue_;
				}
				else
				{
					// use individual trait values to determine color; we use fitness values cached in UpdateFitness, so we don't have to call out to fitness callbacks
					// we normalize fitness values with subpopFitnessScaling so individual fitness, unscaled by subpopulation fitness, is used for coloring
					double fitness = individual.cached_fitness_UNSAFE_;
					
					if (!std::isnan(fitness))
						RGBForFitness(fitness / subpopFitnessScaling, &r, &g, &b, scalingFactor);
				}
				
				simd::float4 color = {r, g, b, 1.0};
				
				slimMetalFillRect(left, top, squareSize, squareSize, &color, vbPtrMod);
			}
		}
		
		return subpopSize;
	}
	else
	{
		// This is what we do if we cannot display a subpopulation because there are too many individuals in it to display
		static simd::float4 color = {0.9f, 0.9f, 1.0f, 1.0f};
		
		if (vbPtrMod)
			slimMetalFillNSRect(bounds, &color, vbPtrMod);
		
		return 1;
	}
}

- (void)cacheDisplayBufferForMap:(SpatialMap *)background_map subpopulation:(Subpopulation *)subpop
{
	// Cache a display buffer for the given background map.  This method should be called only in the 2D "xy"
	// case; in the 1D case we can't know the maximum width ahead of time, so we just draw rects without caching,
	// and in the 3D "xyz" case we don't know which z-slice to use so we can't display the spatial map.
	// The window might be too narrow to display a full-size map right now, but we want to premake a full-size map.
	// The sizing logic here is taken from drawRect:, assuming that we are not constrained in width.
	
	NSRect full_bounds = NSInsetRect([self bounds], 1, 1);
	int max_height = (int)full_bounds.size.height;
	double bounds_x0 = subpop->bounds_x0_, bounds_x1 = subpop->bounds_x1_;
	double bounds_y0 = subpop->bounds_y0_, bounds_y1 = subpop->bounds_y1_;
	double bounds_x_size = bounds_x1 - bounds_x0, bounds_y_size = bounds_y1 - bounds_y0;
	double subpopAspect = bounds_x_size / bounds_y_size;
	int max_width = (int)round(max_height * subpopAspect);
	
	// If we have a display buffer of the wrong size, free it.  Note that the user has no way to change the map
	// or the colormap except to set a whole new map, which will also result in the old one being freed.
	if (background_map->display_buffer_ && ((background_map->buffer_width_ != max_width) || (background_map->buffer_height_ != max_height)))
	{
		free(background_map->display_buffer_);
		background_map->display_buffer_ = nullptr;
	}
	
	// Now allocate and validate the display buffer if we haven't already done so.
	if (!background_map->display_buffer_)
	{
		// if we're caching a new display map, we need to cache a new texture as well
		[(id<MTLTexture>)(background_map->display_texture_) release];
		background_map->display_texture_ = nil;
		
		uint8_t *display_buf = (uint8_t *)malloc(max_width * max_height * 4 * sizeof(uint8_t));
		background_map->display_buffer_ = display_buf;
		background_map->buffer_width_ = max_width;
		background_map->buffer_height_ = max_height;
		
		uint8_t *buf_ptr = display_buf;
		int64_t xsize = background_map->grid_size_[0];
		int64_t ysize = background_map->grid_size_[1];
		double *values = background_map->values_;
		bool interpolate = background_map->interpolate_;
		
		for (int y = max_height - 1; y >= 0; y--)	// reverse order to flip vertically, which gives the right orientation
		{
			for (int x = 0; x < max_width; x++)
			{
				// Look up the nearest map point and get its value; interpolate if requested
				double x_fraction = (x + 0.5) / max_width;		// pixel center
				double y_fraction = (y + 0.5) / max_height;		// pixel center
				double value;
				
				if (interpolate)
				{
					double x_map = x_fraction * (xsize - 1);
					double y_map = y_fraction * (ysize - 1);
					int x1_map = (int)floor(x_map);
					int y1_map = (int)floor(y_map);
					int x2_map = (int)ceil(x_map);
					int y2_map = (int)ceil(y_map);
					double fraction_x2 = x_map - x1_map;
					double fraction_x1 = 1.0 - fraction_x2;
					double fraction_y2 = y_map - y1_map;
					double fraction_y1 = 1.0 - fraction_y2;
					double value_x1_y1 = values[x1_map + y1_map * xsize] * fraction_x1 * fraction_y1;
					double value_x2_y1 = values[x2_map + y1_map * xsize] * fraction_x2 * fraction_y1;
					double value_x1_y2 = values[x1_map + y2_map * xsize] * fraction_x1 * fraction_y2;
					double value_x2_y2 = values[x2_map + y2_map * xsize] * fraction_x2 * fraction_y2;
					
					value = value_x1_y1 + value_x2_y1 + value_x1_y2 + value_x2_y2;
				}
				else
				{
					int x_map = (int)round(x_fraction * (xsize - 1));
					int y_map = (int)round(y_fraction * (ysize - 1));
					
					value = values[x_map + y_map * xsize];
				}
				
				// Given the (interpolated?) value, look up the color, interpolating if necessary
				double rgb[3];
				
				background_map->ColorForValue(value, rgb);
				
				// Write the color values to the buffer, little-endian
				*(buf_ptr++) = (uint8_t)round(rgb[2] * 255.0);	// B
				*(buf_ptr++) = (uint8_t)round(rgb[1] * 255.0);	// G
				*(buf_ptr++) = (uint8_t)round(rgb[0] * 255.0);	// R
				*(buf_ptr++) = (uint8_t)0xFF;					// opaque alpha
			}
		}
		
		// Generate a MTLTexture from the display buffer
		MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
		
		// Indicate that each pixel has a blue, green, red, and alpha channel, where each channel is
		// an 8-bit unsigned normalized value (i.e. 0 maps to 0.0 and 255 maps to 1.0)
		textureDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
		
		// Set the pixel dimensions of the texture
		textureDescriptor.width = max_width;
		textureDescriptor.height = max_height;
		
		// Create the texture from the device by using the descriptor
		id<MTLTexture> texture = [device_ newTextureWithDescriptor:textureDescriptor];
		[textureDescriptor release];
		
		// Calculate the number of bytes per row in the image.
		NSUInteger bytesPerRow = 4 * max_width;
		
		MTLRegion region = {
			{ 0, 0, 0 },										// MTLOrigin
			{(NSUInteger)max_width, (NSUInteger)max_height, 1}	// MTLSize
		};
		
		// Copy the bytes from the data object into the texture
		[texture replaceRegion:region mipmapLevel:0 withBytes:display_buf bytesPerRow:bytesPerRow];
		
		background_map->display_texture_ = texture;
		background_map->texture_free_FUN = &TextureFreeFUN;
		
#if DEBUG
		NSLog(@"cacheDisplayBufferForMap:subpopulation: cached MTLTexture %px with size (%ld, %ld)", background_map->display_texture_, (long)max_width, (long)max_height);
#endif
	}
}

- (NSUInteger)_drawFlatBackgroundSpatialMap:(SpatialMap *)background_map inBounds:(NSRect)bounds forSubpopulation:(Subpopulation *)subpop withBuffer:(SLiMFlatVertex **)vbPtrMod
{
	// We have a spatial map with a color map, so use it to draw the background
	int bounds_x1 = (int)bounds.origin.x;
	int bounds_y1 = (int)bounds.origin.y;
	int bounds_x2 = (int)(bounds.origin.x + bounds.size.width);
	int bounds_y2 = (int)(bounds.origin.y + bounds.size.height);
	
	if (background_map->spatiality_ == 1)
	{
		// This is the spatiality "x" and "y" cases; they are the only 1D spatiality values for which SLiMgui will draw
		// In the 1D case we can't cache a display buffer, since we don't know what aspect ratio to use, so we just
		// draw rects.  Whether those rects are horizontal or vertical will depend on the spatiality of the map.  Most
		// of the code is identical, though, because of the way we handle dimensions, so we share the two cases here.
		bool spatiality_is_x = (background_map->spatiality_string_ == "x");
		int64_t xsize = background_map->grid_size_[0];
		double *values = background_map->values_;
		
		if (background_map->interpolate_)
		{
			// Interpolation, so we need to draw every line individually
			int min_coord = (spatiality_is_x ? bounds_x1 : bounds_y1);
			int max_coord = (spatiality_is_x ? bounds_x2 : bounds_y2);
			
			if (vbPtrMod)
			{
				for (int x = min_coord; x < max_coord; ++x)
				{
					double x_fraction = (x + 0.5 - min_coord) / (max_coord - min_coord);	// values evaluated at pixel centers
					double x_map = x_fraction * (xsize - 1);
					int x1_map = (int)floor(x_map);
					int x2_map = (int)ceil(x_map);
					double fraction_x2 = x_map - x1_map;
					double fraction_x1 = 1.0 - fraction_x2;
					double value_x1 = values[x1_map] * fraction_x1;
					double value_x2 = values[x2_map] * fraction_x2;
					double value = value_x1 + value_x2;
					
					int x1, x2, y1, y2;
					
					if (spatiality_is_x)
					{
						x1 = x;
						x2 = x + 1;
						y1 = bounds_y1;
						y2 = bounds_y2;
					}
					else
					{
						y1 = (max_coord - 1) - x + min_coord;	// flip for y, to use Cartesian coordinates
						y2 = y1 + 1;
						x1 = bounds_x1;
						x2 = bounds_x2;
					}
					
					float rgb[3];
					
					background_map->ColorForValue(value, rgb);
					
					simd::float4 color = {rgb[0], rgb[1], rgb[2], 1.0};
					slimMetalFillRect(x1, y1, x2 - x1, y2 - y1, &color, vbPtrMod);
				}
			}
			
			return (max_coord - min_coord);
		}
		else
		{
			// No interpolation, so we can draw whole grid blocks
			if (vbPtrMod)
			{
				for (int x = 0; x < xsize; x++)
				{
					double value = (spatiality_is_x ? values[x] : values[(xsize - 1) - x]);	// flip for y, to use Cartesian coordinates
					int x1, x2, y1, y2;
					
					if (spatiality_is_x)
					{
						x1 = (int)round(((x - 0.5) / (xsize - 1)) * bounds.size.width + bounds.origin.x);
						x2 = (int)round(((x + 0.5) / (xsize - 1)) * bounds.size.width + bounds.origin.x);
						
						if (x1 < bounds_x1) x1 = bounds_x1;
						if (x2 > bounds_x2) x2 = bounds_x2;
						
						y1 = bounds_y1;
						y2 = bounds_y2;
					}
					else
					{
						y1 = (int)round(((x - 0.5) / (xsize - 1)) * bounds.size.height + bounds.origin.y);
						y2 = (int)round(((x + 0.5) / (xsize - 1)) * bounds.size.height + bounds.origin.y);
						
						if (y1 < bounds_y1) y1 = bounds_y1;
						if (y2 > bounds_y2) y2 = bounds_y2;
						
						x1 = bounds_x1;
						x2 = bounds_x2;
					}
					
					float rgb[3];
					
					background_map->ColorForValue(value, rgb);
					
					simd::float4 color = {rgb[0], rgb[1], rgb[2], 1.0};
					slimMetalFillRect(x1, y1, x2 - x1, y2 - y1, &color, vbPtrMod);
				}
			}
			
			return xsize;
		}
	}
	else // if (background_map->spatiality_ == 2)
	{
		// This is the spatiality "xy" case; it is the only 2D spatiality for which SLiMgui will draw
		// This is now handled by _drawTexturedBackgroundSpatialMap:
		return 0;
	}
}

- (NSUInteger)_drawTexturedBackgroundSpatialMap:(SpatialMap *)background_map inBounds:(NSRect)bounds forSubpopulation:(Subpopulation *)subpop withBuffer:(SLiMTexturedVertex **)vbPtrMod texture:(id<MTLTexture> *)texture
{
	// We have a spatial map with a color map, so use it to draw the background
	if (background_map->spatiality_ == 1)
	{
		// This is the spatiality "x" and "y" cases; they are the only 1D spatiality values for which SLiMgui will draw
		// This case is handled with flat polygons in _drawFlatBackgroundSpatialMap, above.
		return 0;
	}
	else // if (background_map->spatiality_ == 2)
	{
		// This is the spatiality "xy" case; it is the only 2D spatiality for which SLiMgui will draw
		
		// First, cache the display buffer if needed.  This should produce both a cached pixel buffer
		// and a cached MTLTexture for us to use
		[self cacheDisplayBufferForMap:background_map subpopulation:subpop];
		
		id<MTLTexture> display_texture = (id<MTLTexture>)background_map->display_texture_;
		
		if (display_texture)
		{
			if (vbPtrMod)
			{
				slimMetalTextureNSRect(bounds, vbPtrMod);
				*texture = display_texture;
			}
			
			return 2 * 3;	// one rect, 2 triangles per rect, 3 vertices per triangle
		}
		else
		{
			NSLog(@"_drawTexturedBackgroundSpatialMap: caching the spatial map texture failed");
			return 0;
		}
	}
}

- (void)chooseDefaultBackgroundSettings:(PopulationViewBackgroundSettings *)background map:(SpatialMap **)returnMap forSubpopulation:(Subpopulation *)subpop
{
	// black by default
	background->backgroundType = 0;
	
	// if there are spatial maps defined, we try to choose one, requiring "x" or "y" or "xy", and requiring
	// a color map to be defined, and preferring 2D over 1D, providing the same default behavior as SLiM 2.x
	SpatialMapMap &spatial_maps = subpop->spatial_maps_;
	SpatialMap *background_map = nullptr;
	std::string background_map_name;
	
	for (const auto &map_pair : spatial_maps)
	{
		SpatialMap *map = map_pair.second;
		
		// a map must be "x", "y", or "xy", and must have a defined color map, for us to choose it as a default at all
		if (((map->spatiality_string_ == "x") || (map->spatiality_string_ == "y") || (map->spatiality_string_ == "xy")) && (map->n_colors_ > 0))
		{
			// the map is usable, so now we check whether it's better than the map we previously found, if any
			if ((!background_map) || (map->spatiality_ > background_map->spatiality_))
			{
				background_map = map;
				background_map_name = map_pair.first;
			}
		}
	}
	
	if (background_map)
	{
		background->backgroundType = 3;
		background->spatialMapName = background_map_name;
		*returnMap = background_map;
	}
}

- (void)resolveBackgroundToDisplay:(PopulationViewBackgroundSettings &)background spatialMap:(SpatialMap *&)background_map forSubpopulation:(Subpopulation *)subpop
{
	auto backgroundIter = backgroundSettings.find(subpop->subpopulation_id_);
	
	if (backgroundIter == backgroundSettings.end())
	{
		// The user has not made a choice, so choose a temporary default.  We don't want this choice to "stick",
		// so that we can, e.g., begin as black and then change to a spatial map if one is defined.
		[self chooseDefaultBackgroundSettings:&background map:&background_map forSubpopulation:subpop];
	}
	else
	{
		// The user has made a choice; verify that it is acceptable, and then use it.
		background = backgroundIter->second;
		
		if (background.backgroundType == 3)
		{
			SpatialMapMap &spatial_maps = subpop->spatial_maps_;
			auto map_iter = spatial_maps.find(background.spatialMapName);
			
			if (map_iter != spatial_maps.end())
			{
				background_map = map_iter->second;
				
				// if the user somehow managed to choose a map that is not of an acceptable dimensionality, reject it here
				if ((background_map->spatiality_string_ != "x") && (background_map->spatiality_string_ != "y") && (background_map->spatiality_string_ != "xy"))
					background_map = nil;
			}
		}
		
		// if we're supposed to use a background map but we couldn't find it, or it's unacceptable, revert to black
		if ((background.backgroundType == 3) && !background_map)
			background.backgroundType = 0;
	}
}

- (NSUInteger)drawFlatSpatialBackgroundInBounds:(NSRect)bounds forSubpopulation:(Subpopulation *)subpop dimensionality:(int)dimensionality withBuffer:(SLiMFlatVertex **)vbPtrMod
{
	PopulationViewBackgroundSettings background;
	SpatialMap *background_map = nil;
	
	[self resolveBackgroundToDisplay:background spatialMap:background_map forSubpopulation:subpop];
	
	if ((background.backgroundType == 3) && background_map)
	{
		return [self _drawFlatBackgroundSpatialMap:background_map inBounds:bounds forSubpopulation:subpop withBuffer:vbPtrMod];
	}
	else
	{
		// No background map, so just clear to the preferred background color
		int backgroundColor = background.backgroundType;
		
		if (vbPtrMod)
		{
			static simd::float4 blackColor = {0.0f, 0.0f, 0.0f, 1.0f};
			static simd::float4 grayColor = {0.3f, 0.3f, 0.3f, 1.0f};
			static simd::float4 whiteColor = {1.0f, 1.0f, 1.0f, 1.0f};
			simd::float4 *fillColor;
			
			if (backgroundColor == 0)
				fillColor = &blackColor;
			else if (backgroundColor == 1)
				fillColor = &grayColor;
			else if (backgroundColor == 2)
				fillColor = &whiteColor;
			else
				fillColor = &blackColor;
			
			slimMetalFillNSRect(bounds, fillColor, vbPtrMod);
		}
		
		return 1;	// just a fill rect
	}
}

- (NSUInteger)drawTexturedSpatialBackgroundInBounds:(NSRect)bounds forSubpopulation:(Subpopulation *)subpop dimensionality:(int)dimensionality withBuffer:(SLiMTexturedVertex **)vbPtrMod texture:(id<MTLTexture> *)texture
{
	PopulationViewBackgroundSettings background;
	SpatialMap *background_map = nil;
	
	[self resolveBackgroundToDisplay:background spatialMap:background_map forSubpopulation:subpop];
	
	if ((background.backgroundType == 3) && background_map)
		return [self _drawTexturedBackgroundSpatialMap:background_map inBounds:bounds forSubpopulation:subpop withBuffer:vbPtrMod texture:texture];
	else
		return 0;	// No background map, so the background is flat
}

- (NSUInteger)drawSpatialIndividualsFromSubpopulation:(Subpopulation *)subpop inArea:(NSRect)bounds dimensionality:(int)dimensionality withBuffer:(SLiMFlatVertex **)vbPtrMod
{
	SLiMWindowController *controller = [[self window] windowController];
	double scalingFactor = controller->fitnessColorScale;
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	
	if (!vbPtrMod)
		return subpopSize * 2;	// 1 rect for the fill, 1 for the frame because we do it with an underlying rect fill
	
	double bounds_x0 = subpop->bounds_x0_, bounds_x1 = subpop->bounds_x1_;
	double bounds_y0 = subpop->bounds_y0_, bounds_y1 = subpop->bounds_y1_;
	double bounds_x_size = bounds_x1 - bounds_x0, bounds_y_size = bounds_y1 - bounds_y0;
	double subpopFitnessScaling = subpop->last_fitness_scaling_;
	
	if ((subpopFitnessScaling <= 0.0) || !std::isfinite(subpopFitnessScaling))
		subpopFitnessScaling = 1.0;
	
	NSRect individualArea = NSMakeRect(bounds.origin.x, bounds.origin.y, bounds.size.width - 1, bounds.size.height - 1);
	
	// First we outline all individuals
	static simd::float4 frameColor = {0.25f, 0.25f, 0.25f, 1.0};
	if (dimensionality == 1)
		srandom(controller->sim->Generation());
	
	for (int individualArrayIndex = 0; individualArrayIndex < subpopSize; ++individualArrayIndex)
	{
		// Figure out the rect to draw in; note we now use individualArrayIndex here, because the hit-testing code doesn't have an easy way to calculate the displayed individual index...
		Individual &individual = *subpop->parent_individuals_[individualArrayIndex];
		float position_x, position_y;
		
		if (dimensionality == 1)
		{
			position_x = (float)((individual.spatial_x_ - bounds_x0) / bounds_x_size);
			position_y = (float)(random() / (double)INT32_MAX);
			
			// We don't skip out-of-bounds points with Metal; the overhead of checking is probably larger than the
			// overhead of clipping them out on the GPU, and omitting planned vertices throws our vertex count off
			//if ((position_x < 0.0) || (position_x > 1.0))		// skip points that are out of bounds
			//	continue;
		}
		else
		{
			position_x = (float)((individual.spatial_x_ - bounds_x0) / bounds_x_size);
			position_y = (float)((individual.spatial_y_ - bounds_y0) / bounds_y_size);
			
			//if ((position_x < 0.0) || (position_x > 1.0) || (position_y < 0.0) || (position_y > 1.0))		// skip points that are out of bounds
			//	continue;
		}
		
		float centerX = (float)(individualArea.origin.x + round(position_x * individualArea.size.width) + 0.5);
		float centerY = (float)(individualArea.origin.y + individualArea.size.height - round(position_y * individualArea.size.height) + 0.5);
		
		float left = centerX - 2.5f;
		float top = centerY - 2.5f;
		float right = centerX + 2.5f;
		float bottom = centerY + 2.5f;
		
		if (left < individualArea.origin.x) left = (float)individualArea.origin.x;
		if (top < individualArea.origin.y) top = (float)individualArea.origin.y;
		if (right > individualArea.origin.x + individualArea.size.width + 1) right = (float)(individualArea.origin.x + individualArea.size.width + 1);
		if (bottom > individualArea.origin.y + individualArea.size.height + 1) bottom = (float)(individualArea.origin.y + individualArea.size.height + 1);
		
		slimMetalFillRect(left, top, right - left, bottom - top, &frameColor, vbPtrMod);
	}
	
	// Then we draw all individuals
	if (dimensionality == 1)
		srandom(controller->sim->Generation());
	
	for (int individualArrayIndex = 0; individualArrayIndex < subpopSize; ++individualArrayIndex)
	{
		// Figure out the rect to draw in; note we now use individualArrayIndex here, because the hit-testing code doesn't have an easy way to calculate the displayed individual index...
		Individual &individual = *subpop->parent_individuals_[individualArrayIndex];
		float position_x, position_y;
		
		if (dimensionality == 1)
		{
			position_x = (float)((individual.spatial_x_ - bounds_x0) / bounds_x_size);
			position_y = (float)(random() / (double)INT32_MAX);
			
			//if ((position_x < 0.0) || (position_x > 1.0))		// skip points that are out of bounds
			//	continue;
		}
		else
		{
			position_x = (float)((individual.spatial_x_ - bounds_x0) / bounds_x_size);
			position_y = (float)((individual.spatial_y_ - bounds_y0) / bounds_y_size);
			
			//if ((position_x < 0.0) || (position_x > 1.0) || (position_y < 0.0) || (position_y > 1.0))		// skip points that are out of bounds
			//	continue;
		}
		
		float centerX = (float)(individualArea.origin.x + round(position_x * individualArea.size.width) + 0.5);
		float centerY = (float)(individualArea.origin.y + individualArea.size.height - round(position_y * individualArea.size.height) + 0.5);
		float left = centerX - 1.5f;
		float top = centerY - 1.5f;
		float right = centerX + 1.5f;
		float bottom = centerY + 1.5f;
		
		// clipping deliberately not done here; because individual rects are 3x3, they will fall at most one pixel
		// outside our drawing area, and thus the flaw will be covered by the view frame when it overdraws
		
		// dark gray default, for a fitness of NaN; should never happen
		float r = 0.3f, g = 0.3f, b = 0.3f;
		
		if (Individual::s_any_individual_color_set_ && !individual.color_.empty())
		{
			r = individual.color_red_;
			g = individual.color_green_;
			b = individual.color_blue_;
		}
		else
		{
			// use individual trait values to determine color; we used fitness values cached in UpdateFitness, so we don't have to call out to fitness callbacks
			// we normalize fitness values with subpopFitnessScaling so individual fitness, unscaled by subpopulation fitness, is used for coloring
			double fitness = individual.cached_fitness_UNSAFE_;
			
			if (!std::isnan(fitness))
				RGBForFitness(fitness / subpopFitnessScaling, &r, &g, &b, scalingFactor);
		}
		
		simd::float4 fillColor = {r, g, b, 1.0};
		slimMetalFillRect(left, top, right - left, bottom - top, &fillColor, vbPtrMod);
	}
	
	return subpopSize * 2;
}

//
//	The rendering code is split into three steps, because we want to:
//
//		- draw background fills and frames that underlie everything else (using flat-colored triangles)
//		- draw spatial maps, which are conceptually part of the background (but use texture-mapped triangles)
//		- draw individuals, which go on top of the backgrounds (using flat-colored triangles)
//
//	Since the pipeline state has to be switched between flat and textured, we basically run through the
//	subview tiling logic three times, for the three steps, and draw what is needed at each step.  This
//	unfortunately means of lot of duplicated logic, which is pretty brittle, but a better design is not
//	obvious.  See drawRect: for how this plays out in practice.
//

- (void)step1_drawBackdropInRenderEncoder:(id<MTLRenderCommandEncoder>)renderEncoder
{
	//
	//	NOTE this code is parallel to code in the other steps, and in tileSubpopulations:, and all should be maintained in parallel!
	//
	
	static simd::float4 frameColor = {0.77f, 0.77f, 0.77f, 1.0f};
	
	NSRect bounds = [self bounds];
	SLiMWindowController *controller = [[self window] windowController];
	SLiMSim *sim = controller->sim;
	std::vector<Subpopulation*> selectedSubpopulations = [controller selectedSubpopulations];
	int selectedSubpopCount = (int)(selectedSubpopulations.size());
	
	// Decide on our display mode
	if (!controller->invalidSimulation && sim && sim->simulation_valid_ && (sim->generation_ >= 1))
	{
		if (displayMode == -1)
			displayMode = ((sim->spatial_dimensionality_ == 0) ? 0 : 1);
		if ((displayMode == 1) && (sim->spatial_dimensionality_ == 0))
			displayMode = 0;
	}
	
	// Pre-plan to figure out how many vertices we will need and how we will clear/frame
	NSUInteger rectCount = 0;
	simd::float4 *fillColor = NULL;
	
	if (selectedSubpopCount == 0)
	{
		// no selected subpops, so we will just clear and frame
		static simd::float4 aFillColor = {0.9f, 0.9f, 0.9f, 1.0f};
		fillColor = &aFillColor;
	}
	else if (selectedSubpopCount > 10)
	{
		// we should be hidden in this case, but just in case, we fill and frame
		static simd::float4 aFillColor = {0.9f, 0.9f, 1.0f, 1.0f};
		fillColor = &aFillColor;
	}
	else
	{
		// spatial and non-spatial display of individuals
		fillColor = NULL;
		
		if (selectedSubpopulations.size() > 1)
			rectCount++;	// we will do our own fill to the background color
		
		for (Subpopulation *subpop : selectedSubpopulations)
		{
			auto tileIter = subpopTiles.find(subpop->subpopulation_id_);
			
			if (tileIter != subpopTiles.end())
			{
				if ((displayMode == 1) && (sim->spatial_dimensionality_ == 1))
				{
				}
				else if ((displayMode == 1) && (sim->spatial_dimensionality_ > 1))
				{
					rectCount += 1;		// 1 rect for the tile fill
					rectCount += 4;		// 4 rects for the tile frame
				}
				else	// displayMode == 0
				{
					rectCount += 1;		// 1 rect for the background fill
				}
			}
		}
	}
	
	if (fillColor)
		rectCount += 1;		// 1 rect for the background fill
	
	if (rectCount)
	{
		// Then we ensure there is adequate space in our vertex buffer
		NSUInteger vertexCount = rectCount * 2 * 3;						// 2 triangles per rect, 3 vertices per triangle
		SLiMFlatVertex tempVertexBuffer[vertexCount];
		
		// Now we can get our pointer into the vertex buffer
		SLiMFlatVertex *vbPtr = tempVertexBuffer;
		
		// Clear to the chosen background color, if any
		if (fillColor)
			slimMetalFillNSRect(bounds, fillColor, &vbPtr);
		
		// Finally we render into the vertex buffer in the manner we planned
		if ((selectedSubpopCount > 0) && (selectedSubpopCount <= 10))
		{
			// spatial and non-spatial display of individuals
			if (selectedSubpopulations.size() > 1)
			{
				// we will clear the background area ourselves, but do not frame it, providing the appearance of tiled subviews
				static simd::float4 aFillColor = {0.93f, 0.93f, 0.93f, 1.0f};
				slimMetalFillNSRect(bounds, &aFillColor, &vbPtr);
			}
			
			for (Subpopulation *subpop : selectedSubpopulations)
			{
				auto tileIter = subpopTiles.find(subpop->subpopulation_id_);
				
				if (tileIter != subpopTiles.end())
				{
					NSRect tileBounds = tileIter->second;
					
					if ((displayMode == 1) && (sim->spatial_dimensionality_ == 1))
					{
					}
					else if ((displayMode == 1) && (sim->spatial_dimensionality_ > 1))
					{
						// clear to a shade of gray
						static simd::float4 backdropColor = {0.9f, 0.9f, 0.9f, 1.0f};
						
						slimMetalFillNSRect(tileBounds, &backdropColor, &vbPtr);
						
						// Frame our view
						slimMetalFrameNSRect(tileBounds, &frameColor, &vbPtr);
					}
					else	// displayMode == 0
					{
						// Clear to white
						static simd::float4 whiteColor = {1.0f, 1.0f, 1.0f, 1.0f};
						slimMetalFillNSRect(tileBounds, &whiteColor, &vbPtr);
					}
				}
			}
		}
		
		// Check that we added the planned number of vertices
		if (vbPtr != tempVertexBuffer + vertexCount)
			NSLog(@"vertex count mismatch in step1_drawBackdropInRenderEncoder; expected %ld, drew %ld!", vertexCount, vbPtr - tempVertexBuffer);
		
		// Draw the primitives into the encoder
		[renderEncoder setVertexBytes:tempVertexBuffer length:sizeof(tempVertexBuffer) atIndex:SLiMVertexInputIndexVertices];
		[renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertexCount];
	}
}

- (void)step2_drawTexturedBackgroundsInRenderEncoder:(id<MTLRenderCommandEncoder>)renderEncoder
{
	//
	//	NOTE this code is parallel to code in the other steps, and in tileSubpopulations:, and all should be maintained in parallel!
	//
	
	SLiMWindowController *controller = [[self window] windowController];
	SLiMSim *sim = controller->sim;
	std::vector<Subpopulation*> selectedSubpopulations = [controller selectedSubpopulations];
	int selectedSubpopCount = (int)(selectedSubpopulations.size());
	
	// Decide on our display mode
	if (!controller->invalidSimulation && sim && sim->simulation_valid_ && (sim->generation_ >= 1))
	{
		if (displayMode == -1)
			displayMode = ((sim->spatial_dimensionality_ == 0) ? 0 : 1);
		if ((displayMode == 1) && (sim->spatial_dimensionality_ == 0))
			displayMode = 0;
	}
	
	// Follow the logic of -drawVertexBufferContents and draw textured backgrounds as needed
	if ((selectedSubpopCount > 0) && (selectedSubpopCount <= 10))
	{
		// spatial and non-spatial display of individuals
		for (Subpopulation *subpop : selectedSubpopulations)
		{
			auto tileIter = subpopTiles.find(subpop->subpopulation_id_);
			
			if (tileIter != subpopTiles.end())
			{
				NSRect tileBounds = tileIter->second;
				
				NSUInteger vertexCount = 1 * 2 * 3;		// one rect, 2 triangles per rect, 3 vertices per triangle
				SLiMTexturedVertex tempVertexBuffer[vertexCount];
				SLiMTexturedVertex *vbPtr = tempVertexBuffer;
				id<MTLTexture> texture = nullptr; //[self testTexture];
				
				if ((displayMode == 1) && (sim->spatial_dimensionality_ == 1))
				{
					[self drawTexturedSpatialBackgroundInBounds:tileBounds forSubpopulation:subpop dimensionality:sim->spatial_dimensionality_ withBuffer:&vbPtr texture:&texture];
				}
				else if ((displayMode == 1) && (sim->spatial_dimensionality_ > 1))
				{
					// Now determine a subframe and draw spatial information inside that.
					NSRect spatialDisplayBounds = [self spatialDisplayBoundsForSubpopulation:subpop tileBounds:tileBounds];
					
					[self drawTexturedSpatialBackgroundInBounds:spatialDisplayBounds forSubpopulation:subpop dimensionality:sim->spatial_dimensionality_ withBuffer:&vbPtr texture:&texture];
				}
				
				if (vbPtr == tempVertexBuffer + vertexCount)
				{
					if (texture)
					{
						[renderEncoder setFragmentTexture:texture atIndex:SLiMTextureIndexBaseColor];
						[renderEncoder setVertexBytes:tempVertexBuffer length:sizeof(tempVertexBuffer) atIndex:SLiMVertexInputIndexVertices];
						[renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertexCount];
					}
					else
					{
						NSLog(@"step2_drawTexturedBackgroundsInRenderEncoder: missing texture!");
					}
				}
				else if (vbPtr != tempVertexBuffer)
				{
					NSLog(@"step2_drawTexturedBackgroundsInRenderEncoder: unexpected number of vertices generated!");
				}
			}
		}
	}
}

- (NSUInteger)step3_drawIndividualsInVertexBuffer
{
	//
	//	NOTE this code is parallel to code in the other steps, and in tileSubpopulations:, and all should be maintained in parallel!
	//
	
	static simd::float4 frameColor = {0.77f, 0.77f, 0.77f, 1.0f};
	
	NSRect bounds = [self bounds];
	SLiMWindowController *controller = [[self window] windowController];
	SLiMSim *sim = controller->sim;
	std::vector<Subpopulation*> selectedSubpopulations = [controller selectedSubpopulations];
	int selectedSubpopCount = (int)(selectedSubpopulations.size());
	
	// Decide on our display mode
	if (!controller->invalidSimulation && sim && sim->simulation_valid_ && (sim->generation_ >= 1))
	{
		if (displayMode == -1)
			displayMode = ((sim->spatial_dimensionality_ == 0) ? 0 : 1);
		if ((displayMode == 1) && (sim->spatial_dimensionality_ == 0))
			displayMode = 0;
	}
	
	// Pre-plan to figure out how many vertices we will need and how we will clear/frame
	NSUInteger rectCount = 0;
	bool needsOverallFrame = NO;
	
	if ((selectedSubpopCount == 0) || (selectedSubpopCount > 10))
	{
		needsOverallFrame = YES;
	}
	else
	{
		// spatial and non-spatial display of individuals
		needsOverallFrame = NO;
		
		for (Subpopulation *subpop : selectedSubpopulations)
		{
			auto tileIter = subpopTiles.find(subpop->subpopulation_id_);
			
			if (tileIter != subpopTiles.end())
			{
				NSRect tileBounds = tileIter->second;
				
				if ((displayMode == 1) && (sim->spatial_dimensionality_ == 1))
				{
					rectCount += [self drawFlatSpatialBackgroundInBounds:tileBounds forSubpopulation:subpop dimensionality:sim->spatial_dimensionality_ withBuffer:NULL];
					rectCount += [self drawSpatialIndividualsFromSubpopulation:subpop inArea:NSInsetRect(tileBounds, 1, 1) dimensionality:sim->spatial_dimensionality_ withBuffer:NULL];
					rectCount += 4;		// 4 rects for the tile frame
				}
				else if ((displayMode == 1) && (sim->spatial_dimensionality_ > 1))
				{
					NSRect spatialDisplayBounds = [self spatialDisplayBoundsForSubpopulation:subpop tileBounds:tileBounds];
					
					rectCount += [self drawFlatSpatialBackgroundInBounds:spatialDisplayBounds forSubpopulation:subpop dimensionality:sim->spatial_dimensionality_ withBuffer:NULL];
					rectCount += [self drawSpatialIndividualsFromSubpopulation:subpop inArea:spatialDisplayBounds dimensionality:sim->spatial_dimensionality_ withBuffer:NULL];
					rectCount += 4;		// 4 rects for the spatial bounds frame
				}
				else	// displayMode == 0
				{
					rectCount += [self drawIndividualsFromSubpopulation:subpop inArea:tileBounds withBuffer:NULL];
					rectCount += 4;		// 4 rects for the tile frame
				}
			}
		}
	}
	
	if (needsOverallFrame)
		rectCount += 4;		// 4 rects for the view frame
	
	NSUInteger vertexCount = rectCount * 2 * 3;						// 2 triangles per rect, 3 vertices per triangle
	
	if (vertexCount)
	{
		// Then we ensure there is adequate space in our vertex buffer
		NSUInteger bufferLength = vertexCount * sizeof(SLiMFlatVertex);	// sizeof() includes the necessary padding for data alignment
		
		id<MTLBuffer> buffer = [self takeVertexBufferWithCapacity:bufferLength];
		
		// Now we can get our pointer into the vertex buffer
		SLiMFlatVertex *vbPtr = (SLiMFlatVertex *)[buffer contents];
		
		// Finally we render into the vertex buffer in the manner we planned
		if ((selectedSubpopCount > 0) && (selectedSubpopCount <= 10))
		{
			// spatial and non-spatial display of individuals
			for (Subpopulation *subpop : selectedSubpopulations)
			{
				auto tileIter = subpopTiles.find(subpop->subpopulation_id_);
				
				if (tileIter != subpopTiles.end())
				{
					NSRect tileBounds = tileIter->second;
					
					if ((displayMode == 1) && (sim->spatial_dimensionality_ == 1))
					{
						[self drawFlatSpatialBackgroundInBounds:tileBounds forSubpopulation:subpop dimensionality:sim->spatial_dimensionality_ withBuffer:&vbPtr];
						
						[self drawSpatialIndividualsFromSubpopulation:subpop inArea:NSInsetRect(tileBounds, 1, 1) dimensionality:sim->spatial_dimensionality_ withBuffer:&vbPtr];
						
						slimMetalFrameNSRect(tileBounds, &frameColor, &vbPtr);
					}
					else if ((displayMode == 1) && (sim->spatial_dimensionality_ > 1))
					{
						// Now determine a subframe and draw spatial information inside that.
						NSRect spatialDisplayBounds = [self spatialDisplayBoundsForSubpopulation:subpop tileBounds:tileBounds];
						
						[self drawFlatSpatialBackgroundInBounds:spatialDisplayBounds forSubpopulation:subpop dimensionality:sim->spatial_dimensionality_ withBuffer:&vbPtr];
						
						[self drawSpatialIndividualsFromSubpopulation:subpop inArea:spatialDisplayBounds dimensionality:sim->spatial_dimensionality_ withBuffer:&vbPtr];
						
						slimMetalFrameNSRect(NSInsetRect(spatialDisplayBounds, -1, -1), &frameColor, &vbPtr);
					}
					else	// displayMode == 0
					{
						[self drawIndividualsFromSubpopulation:subpop inArea:tileBounds withBuffer:&vbPtr];
						
						slimMetalFrameNSRect(tileBounds, &frameColor, &vbPtr);
					}
				}
			}
		}
		
		// Frame on top, if we did an overall fill in step 1; the two go together
		if (needsOverallFrame)
			slimMetalFrameNSRect(bounds, &frameColor, &vbPtr);
		
		// Check that we added the planned number of vertices
		if (vbPtr != (SLiMFlatVertex *)[buffer contents] + vertexCount)
			NSLog(@"vertex count mismatch in step3_drawIndividualsInVertexBuffer; expected %ld, drew %ld!", vertexCount, vbPtr - (SLiMFlatVertex *)[buffer contents]);
	}
	
	return vertexCount;
}

- (void)drawWithRenderEncoder:(id<MTLRenderCommandEncoder>_Nonnull)renderEncoder
{
	// Step 1: render the background fills that underlie everything, using ad hoc buffers
	{
		[renderEncoder setRenderPipelineState:flatPipelineState_OriginTop_];
		
		[self step1_drawBackdropInRenderEncoder:renderEncoder];
	}
	
	// Step 2: render any textured backgrounds for spatial subpopulations, using ad hoc buffers
	{
		[renderEncoder setRenderPipelineState:texturedPipelineState_];
		
		[self step2_drawTexturedBackgroundsInRenderEncoder:renderEncoder];
	}
	
	// Step 3: render individuals and overlying frames, using our preallocated vertex buffers
	{
		[renderEncoder setRenderPipelineState:flatPipelineState_OriginTop_];
		
		NSUInteger vertexCount = [self step3_drawIndividualsInVertexBuffer];
		
		[renderEncoder setVertexBuffer:vertexBuffers_[currentBuffer_] offset:0 atIndex:SLiMVertexInputIndexVertices];
		[renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertexCount];
	}
}


//
//	Subarea tiling
//
#pragma mark Subarea tiling

- (BOOL)tileSubpopulations:(std::vector<Subpopulation*> &)selectedSubpopulations
{
	//
	//	NOTE this code is parallel to code in the three rendering steps, and all should be maintained in parallel!
	//
	
	// We will decide upon new tiles for our subpopulations here, so start out empty
	subpopTiles.clear();
	
	NSRect bounds = [self bounds];
	SLiMWindowController *controller = [[self window] windowController];
	SLiMSim *sim = controller->sim;
	int selectedSubpopCount = (int)selectedSubpopulations.size();
	
	// Decide on our display mode
	if (!controller->invalidSimulation && sim && sim->simulation_valid_ && (sim->generation_ >= 1))
	{
		if (displayMode == -1)
			displayMode = ((sim->spatial_dimensionality_ == 0) ? 0 : 1);
		if ((displayMode == 1) && (sim->spatial_dimensionality_ == 0))
			displayMode = 0;
	}
	
	if (selectedSubpopCount == 0)
	{
		return YES;
	}
	else if (selectedSubpopCount > 10)
	{
		return NO;
	}
	else if (selectedSubpopCount == 1)
	{
		Subpopulation *selectedSubpop = selectedSubpopulations[0];
		
		subpopTiles.insert(std::pair<slim_objectid_t, NSRect>(selectedSubpop->subpopulation_id_, bounds));
		
		if ((displayMode == 1) && (sim->spatial_dimensionality_ == 1))
		{
			return YES;
		}
		else if ((displayMode == 1) && (sim->spatial_dimensionality_ > 1))
		{
			return YES;
		}
		else
		{
			return [self canDisplayIndividualsFromSubpopulation:selectedSubpop inArea:bounds];
		}
	}
	else if (displayMode == 1)
	{
		// spatial display adaptively finds the layout that maximizes the pixel area covered, and cannot fail
		int64_t bestTotalExtent = 0;
		
		for (int rowCount = 1; rowCount <= selectedSubpopCount; ++rowCount)
		{
			int columnCount = (int)ceil(selectedSubpopCount / (double)rowCount);
			int interBoxSpace = 5;
			int totalInterboxHeight = interBoxSpace * (rowCount - 1);
			int totalInterboxWidth = interBoxSpace * (columnCount - 1);
			double boxWidth = (bounds.size.width - totalInterboxWidth) / (double)columnCount;
			double boxHeight = (bounds.size.height - totalInterboxHeight) / (double)rowCount;
			std::map<slim_objectid_t, NSRect> candidateTiles;
			int64_t totalExtent = 0;
			
			for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
			{
				int columnIndex = subpopIndex % columnCount;
				int rowIndex = subpopIndex / columnCount;
				double boxLeft = round(bounds.origin.x + columnIndex * (interBoxSpace + boxWidth));
				double boxRight = round(bounds.origin.x + columnIndex * (interBoxSpace + boxWidth) + boxWidth);
				double boxTop = round(bounds.origin.y + rowIndex * (interBoxSpace + boxHeight));
				double boxBottom = round(bounds.origin.y + rowIndex * (interBoxSpace + boxHeight) + boxHeight);
				NSRect boxBounds = NSMakeRect(boxLeft, boxTop, boxRight - boxLeft, boxBottom - boxTop);
				Subpopulation *subpop = selectedSubpopulations[subpopIndex];
				
				candidateTiles.insert(std::pair<slim_objectid_t, NSRect>(subpop->subpopulation_id_, boxBounds));
				
				// find out what pixel area actually gets used by this box, and use that to choose the optimal layout
				NSRect spatialDisplayBounds = [self spatialDisplayBoundsForSubpopulation:subpop tileBounds:boxBounds];
				int64_t extent = (int64_t)spatialDisplayBounds.size.width * (int64_t)spatialDisplayBounds.size.height;
				
				totalExtent += extent;
			}
			
			if (totalExtent > bestTotalExtent)
			{
				bestTotalExtent = totalExtent;
				std::swap(subpopTiles, candidateTiles);
			}
		}
		
		return YES;
	}
	else	// displayMode == 0
	{
		// non-spatial display always uses vertically stacked maximum-width tiles, but can fail if they are too small
		int interBoxSpace = 5;
		int totalInterbox = interBoxSpace * (selectedSubpopCount - 1);
		double boxHeight = (bounds.size.height - totalInterbox) / (double)selectedSubpopCount;
		
		for (int subpopIndex = 0; subpopIndex < selectedSubpopCount; subpopIndex++)
		{
			double boxTop = round(bounds.origin.y + subpopIndex * (interBoxSpace + boxHeight));
			double boxBottom = round(bounds.origin.y + subpopIndex * (interBoxSpace + boxHeight) + boxHeight);
			NSRect boxBounds = NSMakeRect(bounds.origin.x, boxTop, bounds.size.width, boxBottom - boxTop);
			Subpopulation *subpop = selectedSubpopulations[subpopIndex];
			
			subpopTiles.insert(std::pair<slim_objectid_t, NSRect>(subpop->subpopulation_id_, boxBounds));
			
			if (![self canDisplayIndividualsFromSubpopulation:subpop inArea:boxBounds])
			{
				subpopTiles.clear();
				return NO;
			}
		}
		
		return YES;
	}
}

- (BOOL)canDisplayIndividualsFromSubpopulation:(Subpopulation *)subpop inArea:(NSRect)bounds
{
	//
	//	NOTE this code is parallel to the code in drawIndividualsFromSubpopulation:inArea: and should be maintained in parallel
	//
	
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	int squareSize, viewColumns = 0, viewRows = 0;
	
	// first figure out the biggest square size that will allow us to display the whole subpopulation
	for (squareSize = 20; squareSize > 1; --squareSize)
	{
		viewColumns = (int)floor((bounds.size.width - 3) / squareSize);
		viewRows = (int)floor((bounds.size.height - 3) / squareSize);
		
		if (viewColumns * viewRows > subpopSize)
		{
			// If we have an empty row at the bottom, then break for sure; this allows us to look nice and symmetrical
			if ((subpopSize - 1) / viewColumns < viewRows - 1)
				break;
			
			// Otherwise, break only if we are getting uncomfortably small; otherwise, let's drop down one square size to allow symmetry
			if (squareSize <= 5)
				break;
		}
	}
	
	return (squareSize > 1);
}

- (NSRect)spatialDisplayBoundsForSubpopulation:(Subpopulation *)subpop tileBounds:(NSRect)tileBounds
{
	// Determine a subframe for drawing spatial information inside.  The subframe we use is the maximal subframe
	// with integer boundaries that preserves, as closely as possible, the aspect ratio of the subpop's bounds.
	NSRect spatialDisplayBounds = NSInsetRect(tileBounds, 1, 1);
	double displayAspect = spatialDisplayBounds.size.width / spatialDisplayBounds.size.height;
	double bounds_x0 = subpop->bounds_x0_, bounds_x1 = subpop->bounds_x1_;
	double bounds_y0 = subpop->bounds_y0_, bounds_y1 = subpop->bounds_y1_;
	double bounds_x_size = bounds_x1 - bounds_x0, bounds_y_size = bounds_y1 - bounds_y0;
	double subpopAspect = bounds_x_size / bounds_y_size;
	
	if (subpopAspect > displayAspect)
	{
		// The display bounds will need to shrink vertically to match the subpop
		double idealSize = round(spatialDisplayBounds.size.width / subpopAspect);
		double roundedOffset = round((spatialDisplayBounds.size.height - idealSize) / 2.0);
		
		spatialDisplayBounds.origin.y += roundedOffset;
		spatialDisplayBounds.size.height = idealSize;
	}
	else if (subpopAspect < displayAspect)
	{
		// The display bounds will need to shrink horizontally to match the subpop
		double idealSize = round(spatialDisplayBounds.size.height * subpopAspect);
		double roundedOffset = round((spatialDisplayBounds.size.width - idealSize) / 2.0);
		
		spatialDisplayBounds.origin.x += roundedOffset;
		spatialDisplayBounds.size.width = idealSize;
	}
	
	return spatialDisplayBounds;
}


//
//	Actions
//
#pragma mark Actions

- (IBAction)setDisplayStyle:(id)sender
{
	NSMenuItem *senderMenuItem = (NSMenuItem *)sender;
	int newDisplayMode = (int)[senderMenuItem tag];
	
	if (displayMode != newDisplayMode)
	{
		displayMode = newDisplayMode;
		[self setNeedsDisplay:YES];
		[[[self window] windowController] updatePopulationViewHiding];
	}
}

- (IBAction)setDisplayBackground:(id)sender
{
	NSMenuItem *senderMenuItem = (NSMenuItem *)sender;
	int newDisplayBackground = (int)([senderMenuItem tag] - 10);
	auto backgroundIter = backgroundSettings.find(lastContextMenuSubpopID);
	PopulationViewBackgroundSettings *background = ((backgroundIter == backgroundSettings.end()) ? nil : &backgroundIter->second);
	std::string mapName;
	
	// If the user has selected a spatial map, extract its name
	if (newDisplayBackground == 3)
	{
		NSString *menuItemTitle = [senderMenuItem title];
		NSArray<NSString *> *parts = [menuItemTitle componentsSeparatedByString:@"\""];
		
		if ([parts count] == 5)
		{
			NSString *mapNameString = [parts objectAtIndex:1];
			const char *mapNameString_inner = [mapNameString UTF8String];
			
			mapName = std::string(mapNameString_inner);
		}
		
		if (mapName.length() == 0)
			return;
	}
	
	if (background)
	{
		background->backgroundType = newDisplayBackground;
		background->spatialMapName = mapName;
		[self setNeedsDisplay:YES];
	}
	else
	{
		backgroundSettings.insert(std::pair<const int, PopulationViewBackgroundSettings>(lastContextMenuSubpopID, PopulationViewBackgroundSettings{newDisplayBackground, mapName}));
		[self setNeedsDisplay:YES];
	}
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
	SLiMWindowController *controller = [[self window] windowController];
	SLiMSim *sim = controller->sim;
	bool disableAll = false;
	
	// When the simulation is not valid and initialized, the context menu is disabled
	if (controller->invalidSimulation || !sim || !sim->simulation_valid_ || (sim->generation_ < 1))
		disableAll = true;
	
	NSMenu *menu = [[NSMenu alloc] initWithTitle:@"population_menu"];
	NSMenuItem *menuItem;
	
	[menu setAutoenablesItems:NO];
	
	menuItem = [menu addItemWithTitle:@"Display Individuals (non-spatial)" action:@selector(setDisplayStyle:) keyEquivalent:@""];
	[menuItem setTag:0];
	[menuItem setTarget:self];
	[menuItem setEnabled:!disableAll];
	
	menuItem = [menu addItemWithTitle:@"Display Individuals (spatial)" action:@selector(setDisplayStyle:) keyEquivalent:@""];
	[menuItem setTag:1];
	[menuItem setTarget:self];
	[menuItem setEnabled:(!disableAll && (sim->spatial_dimensionality_ > 0))];
	
	// Check the item corresponding to our current display preference, if any
	if (!disableAll)
	{
		menuItem = [menu itemWithTag:displayMode];
		[menuItem setState:NSOnState];
	}
	
	// If we're displaying spatially, provide background options (colors, spatial maps)
	if (!disableAll && (sim->spatial_dimensionality_ > 0) && (displayMode == 1))
	{
		// determine which subpopulation the click was in
		std::vector<Subpopulation*> selectedSubpopulations = [controller selectedSubpopulations];
		Subpopulation *subpopForEvent = nullptr;
		NSPoint eventPoint = [theEvent locationInWindow];
		NSPoint viewPoint = [self convertPoint:eventPoint fromView:nil];
		
		// our tile coordinates are in our Metal coordinate system, which has the origin at top left
		viewPoint.y = [self bounds].size.height - viewPoint.y;
		
		for (Subpopulation *subpop : selectedSubpopulations)
		{
			slim_objectid_t subpop_id = subpop->subpopulation_id_;
			auto tileIter = subpopTiles.find(subpop_id);
			
			if (tileIter != subpopTiles.end())
			{
				NSRect tileRect = tileIter->second;
				
				if (NSPointInRect(viewPoint, tileRect))
				{
					subpopForEvent = subpop;
					break;
				}
			}
		}
		
		if (subpopForEvent)
		{
			[menu addItem:[NSMenuItem separatorItem]];
			
			menuItem = [menu addItemWithTitle:[NSString stringWithFormat:@"Background for p%d:", subpopForEvent->subpopulation_id_] action:NULL keyEquivalent:@""];
			[menuItem setTag:-1];
			[menuItem setEnabled:false];
			
			menuItem = [menu addItemWithTitle:@"Black Background" action:@selector(setDisplayBackground:) keyEquivalent:@""];
			[menuItem setTag:10];
			[menuItem setTarget:self];
			[menuItem setEnabled:!disableAll];
			
			menuItem = [menu addItemWithTitle:@"Gray Background" action:@selector(setDisplayBackground:) keyEquivalent:@""];
			[menuItem setTag:11];
			[menuItem setTarget:self];
			[menuItem setEnabled:!disableAll];
			
			menuItem = [menu addItemWithTitle:@"White Background" action:@selector(setDisplayBackground:) keyEquivalent:@""];
			[menuItem setTag:12];
			[menuItem setTarget:self];
			[menuItem setEnabled:!disableAll];
			
			// look for spatial maps to offer as choices; need to scan the defined maps for the ones we can use
			SpatialMapMap &spatial_maps = subpopForEvent->spatial_maps_;
			
			for (const auto &map_pair : spatial_maps)
			{
				SpatialMap *map = map_pair.second;
				
				// We used to display only maps with a color scale; now we just make up a color scale if none is given.  Only
				// "x", "y", and "xy" maps are considered displayable; We can't display a z coordinate, and we can't display
				// even the x or y portion of "xz", "yz", and "xyz" maps since we don't know which z-slice to use.
				bool displayable = ((map->spatiality_string_ == "x") || (map->spatiality_string_ == "y") || (map->spatiality_string_ == "xy"));
				NSString *menuItemTitle;
				
				if (map->spatiality_ == 1)
					menuItemTitle = [NSString stringWithFormat:@"Spatial Map \"%s\" (\"%s\", %d)", map_pair.first.c_str(), map->spatiality_string_.c_str(), (int)map->grid_size_[0]];
				else if (map->spatiality_ == 2)
					menuItemTitle = [NSString stringWithFormat:@"Spatial Map \"%s\" (\"%s\", %d×%d)", map_pair.first.c_str(), map->spatiality_string_.c_str(), (int)map->grid_size_[0], (int)map->grid_size_[1]];
				else // (map->spatiality_ == 3)
					menuItemTitle = [NSString stringWithFormat:@"Spatial Map \"%s\" (\"%s\", %d×%d×%d)", map_pair.first.c_str(), map->spatiality_string_.c_str(), (int)map->grid_size_[0], (int)map->grid_size_[1], (int)map->grid_size_[2]];
				
				menuItem = [menu addItemWithTitle:menuItemTitle action:@selector(setDisplayBackground:) keyEquivalent:@""];
				[menuItem setTag:13];
				[menuItem setTarget:self];
				[menuItem setEnabled:(!disableAll && displayable)];
			}
			
			// check the menu item for the preferred display option; if we're in auto mode, don't check anything (could put a dash by the currently chosen style?)
			auto backgroundIter = backgroundSettings.find(subpopForEvent->subpopulation_id_);
			PopulationViewBackgroundSettings *background = ((backgroundIter == backgroundSettings.end()) ? nil : &backgroundIter->second);
			
			if (background)
			{
				menuItem = nil;
				
				if (background->backgroundType == 3)
				{
					// We have to find the menu item to check by scanning and looking for the right title
					NSString *chosenMapPrefix = [NSString stringWithFormat:@"Spatial Map \"%s\" (\"", background->spatialMapName.c_str()];
					NSArray<NSMenuItem *> *menuItems = [menu itemArray];
					
					for (NSMenuItem *item : menuItems)
					{
						if ([[item title] hasPrefix:chosenMapPrefix])
						{
							menuItem = item;
							break;
						}
					}
				}
				else
				{
					// We can find the menu item to check by tag
					menuItem = [menu itemWithTag:background->backgroundType + 10];
				}
				
				[menuItem setState:NSOnState];
			}
			
			// remember which subpopulation this context menu is for
			lastContextMenuSubpopID = subpopForEvent->subpopulation_id_;
		}
	}
	
	return [menu autorelease];
}

@end


@implementation PopulationErrorView

- (void)drawMessage:(NSString *)messageString inRect:(NSRect)rect
{
	static NSDictionary *attrs = nil;
	
	if (!attrs)
		attrs = [@{NSFontAttributeName : [NSFont fontWithName:@"Times New Roman" size:14], NSForegroundColorAttributeName : [NSColor colorWithCalibratedWhite:0.4 alpha:1.0]} retain];
	
	NSAttributedString *attrMessage = [[NSAttributedString alloc] initWithString:messageString attributes:attrs];
	NSPoint centerPoint = NSMakePoint(rect.origin.x + rect.size.width / 2, rect.origin.y + rect.size.height / 2);
	NSSize messageSize = [attrMessage size];
	NSPoint drawPoint = NSMakePoint(centerPoint.x - messageSize.width / 2.0, centerPoint.y - messageSize.height / 2.0);
	
	[attrMessage drawAtPoint:drawPoint];
	[attrMessage release];
}

- (void)drawRect:(NSRect)rect
{
	NSRect bounds = [self bounds];
	
	// Erase background
	[[NSColor colorWithDeviceWhite:0.9 alpha:1.0] set];
	NSRectFill(bounds);
	
	// Frame the view
	[[NSColor colorWithDeviceWhite:0.77 alpha:1.0] set];
	NSFrameRect(bounds);
	
	// Draw the message
	[self drawMessage:@"too many subpops\n   or individuals\n     to display –\n  control-click to\n select a different\n    display mode" inRect:NSInsetRect(bounds, 1, 1)];
}

- (BOOL)isOpaque
{
	return YES;
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
	SLiMWindowController *controller = [[self window] windowController];
	PopulationMetalView *popView = controller->populationView;
	
	return [popView menuForEvent:theEvent];
}

@end
































