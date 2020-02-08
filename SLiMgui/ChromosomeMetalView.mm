//
//  ChromosomeMetalView.m
//  SLiMgui
//
//  Created by Ben Haller on 2/4/20.
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


#import "ChromosomeMetalView.h"
#import "ChromosomeView.h"

#import <MetalKit/MetalKit.h>
#import <simd/simd.h>
#import "MetalView_Shared.h"

#import "SLiMWindowController.h"
#import "SLiMHaplotypeManager.h"


@implementation ChromosomeMetalView

#pragma mark Drawing genomic elements

- (NSUInteger)metalDrawGenomicElementsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange withBuffer:(SLiMFlatVertex **)vbPtrMod
{
	Chromosome &chromosome = controller->sim->chromosome_;
	CGFloat previousIntervalLeftEdge = -10000;
	NSUInteger rectCount = 0;
	
	for (GenomicElement *genomicElement : chromosome.GenomicElements())
	{
		slim_position_t startPosition = genomicElement->start_position_;
		slim_position_t endPosition = genomicElement->end_position_;
		NSRect elementRect = [_chromosomeView rectEncompassingBase:startPosition toBase:endPosition interiorRect:interiorRect displayedRange:displayedRange];
		BOOL widthOne = (elementRect.size.width == 1);
		
		// We want to avoid overdrawing width-one intervals, which are important but small, so if the previous interval was width-one,
		// and we are not, and we are about to overdraw it, then we scoot our left edge over one pixel to leave it alone.
		if (!widthOne && (elementRect.origin.x == previousIntervalLeftEdge))
		{
			elementRect.origin.x++;
			elementRect.size.width--;
		}
		
		// draw only the visible part, if any
		elementRect = NSIntersectionRect(elementRect, interiorRect);
		
		if (!NSIsEmptyRect(elementRect))
		{
			rectCount++;
			
			if (vbPtrMod)
			{
				GenomicElementType *geType = genomicElement->genomic_element_type_ptr_;
				float colorRed, colorGreen, colorBlue, colorAlpha;
				
				if (!geType->color_.empty())
				{
					colorRed = geType->color_red_;
					colorGreen = geType->color_green_;
					colorBlue = geType->color_blue_;
					colorAlpha = 1.0;
				}
				else
				{
					slim_objectid_t elementTypeID = geType->genomic_element_type_id_;
					NSColor *elementColor = [controller colorForGenomicElementType:geType withID:elementTypeID];
					double r, g, b, a;
					
					[elementColor getRed:&r green:&g blue:&b alpha:&a];
					
					colorRed = (float)r;
					colorGreen = (float)g;
					colorBlue = (float)b;
					colorAlpha = (float)a;
				}
				
				simd::float4 color = {colorRed, colorGreen, colorBlue, colorAlpha};
				slimMetalFillNSRect(elementRect, &color, vbPtrMod);
			}
			
			// if this interval is just one pixel wide, we want to try to make it visible, by avoiding overdrawing it; so we remember its location
			if (widthOne)
				previousIntervalLeftEdge = elementRect.origin.x;
			else
				previousIntervalLeftEdge = -10000;
		}
	}
	
	return rectCount;
}

#pragma mark Drawing rate maps

- (NSUInteger)_metalDrawRateMapIntervalsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange ends:(std::vector<slim_position_t> &)ends rates:(std::vector<double> &)rates hue:(double)hue withBuffer:(SLiMFlatVertex **)vbPtrMod
{
	int recombinationIntervalCount = (int)ends.size();
	slim_position_t intervalStartPosition = 0;
	CGFloat previousIntervalLeftEdge = -10000;
	NSUInteger rectCount = 0;
	
	for (int interval = 0; interval < recombinationIntervalCount; ++interval)
	{
		slim_position_t intervalEndPosition = ends[interval];
		double intervalRate = rates[interval];
		NSRect intervalRect = [_chromosomeView rectEncompassingBase:intervalStartPosition toBase:intervalEndPosition interiorRect:interiorRect displayedRange:displayedRange];
		BOOL widthOne = (intervalRect.size.width == 1);
		
		// We want to avoid overdrawing width-one intervals, which are important but small, so if the previous interval was width-one,
		// and we are not, and we are about to overdraw it, then we scoot our left edge over one pixel to leave it alone.
		if (!widthOne && (intervalRect.origin.x == previousIntervalLeftEdge))
		{
			intervalRect.origin.x++;
			intervalRect.size.width--;
		}
		
		// draw only the visible part, if any
		intervalRect = NSIntersectionRect(intervalRect, interiorRect);
		
		if (!NSIsEmptyRect(intervalRect))
		{
			rectCount++;
			
			if (vbPtrMod)
			{
				// color according to how "hot" the region is
				double r, g, b, a;
				
				if (intervalRate == 0.0)
				{
					// a recombination or mutation rate of 0.0 comes out as black, whereas the lowest brightness below is 0.5; we want to distinguish this
					r = g = b = 0.0;
					a = 1.0;
				}
				else
				{
					// the formula below scales 1e-6 to 1.0 and 1e-9 to 0.0; values outside that range clip, but we want there to be
					// reasonable contrast within the range of values commonly used, so we don't want to make the range too wide
					double lightness, brightness = 1.0, saturation = 1.0;
					
					lightness = (log10(intervalRate) + 9.0) / 3.0;
					lightness = std::max(lightness, 0.0);
					lightness = std::min(lightness, 1.0);
					
					if (lightness >= 0.5)	saturation = 1.0 - ((lightness - 0.5) * 2.0);	// goes from 1.0 at lightness 0.5 to 0.0 at lightness 1.0
					else					brightness = 0.5 + lightness;					// goes from 1.0 at lightness 0.5 to 0.5 at lightness 0.0
					
					NSColor *intervalColor = [NSColor colorWithCalibratedHue:(CGFloat)hue saturation:saturation brightness:brightness alpha:1.0];
					
					[intervalColor getRed:&r green:&g blue:&b alpha:&a];
				}
				
				simd::float4 color = {(float)r, (float)g, (float)b, (float)a};
				slimMetalFillNSRect(intervalRect, &color, vbPtrMod);
			}
			
			// if this interval is just one pixel wide, we want to try to make it visible, by avoiding overdrawing it; so we remember its location
			if (widthOne)
				previousIntervalLeftEdge = intervalRect.origin.x;
			else
				previousIntervalLeftEdge = -10000;
		}
		
		// the next interval starts at the next base after this one ended
		intervalStartPosition = intervalEndPosition + 1;
	}
	
	return rectCount;
}

- (NSUInteger)metalDrawRecombinationIntervalsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange withBuffer:(SLiMFlatVertex **)vbPtrMod
{
	Chromosome &chromosome = controller->sim->chromosome_;
	NSUInteger rectCount = 0;
	
	if (chromosome.single_recombination_map_)
	{
		rectCount += [self _metalDrawRateMapIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange ends:chromosome.recombination_end_positions_H_ rates:chromosome.recombination_rates_H_ hue:0.65 withBuffer:vbPtrMod];
	}
	else
	{
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		rectCount += [self _metalDrawRateMapIntervalsInInteriorRect:topInteriorRect withController:controller displayedRange:displayedRange ends:chromosome.recombination_end_positions_M_ rates:chromosome.recombination_rates_M_ hue:0.65 withBuffer:vbPtrMod];
		rectCount += [self _metalDrawRateMapIntervalsInInteriorRect:bottomInteriorRect withController:controller displayedRange:displayedRange ends:chromosome.recombination_end_positions_F_ rates:chromosome.recombination_rates_F_ hue:0.65 withBuffer:vbPtrMod];
	}
	
	return rectCount;
}

- (NSUInteger)metalDrawMutationIntervalsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange withBuffer:(SLiMFlatVertex **)vbPtrMod
{
	Chromosome &chromosome = controller->sim->chromosome_;
	NSUInteger rectCount = 0;
	
	if (chromosome.single_mutation_map_)
	{
		rectCount += [self _metalDrawRateMapIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange ends:chromosome.mutation_end_positions_H_ rates:chromosome.mutation_rates_H_ hue:0.75 withBuffer:vbPtrMod];
	}
	else
	{
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		rectCount += [self _metalDrawRateMapIntervalsInInteriorRect:topInteriorRect withController:controller displayedRange:displayedRange ends:chromosome.mutation_end_positions_M_ rates:chromosome.mutation_rates_M_ hue:0.75 withBuffer:vbPtrMod];
		rectCount += [self _metalDrawRateMapIntervalsInInteriorRect:bottomInteriorRect withController:controller displayedRange:displayedRange ends:chromosome.mutation_end_positions_F_ rates:chromosome.mutation_rates_F_ hue:0.75 withBuffer:vbPtrMod];
	}
	
	return rectCount;
}

- (NSUInteger)metalDrawRateMapsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange withBuffer:(SLiMFlatVertex **)vbPtrMod
{
	Chromosome &chromosome = controller->sim->chromosome_;
	BOOL recombinationWorthShowing = NO;
	BOOL mutationWorthShowing = NO;
	NSUInteger rectCount = 0;
	
	if (chromosome.single_mutation_map_)
		mutationWorthShowing = (chromosome.mutation_end_positions_H_.size() > 1);
	else
		mutationWorthShowing = ((chromosome.mutation_end_positions_M_.size() > 1) || (chromosome.mutation_end_positions_F_.size() > 1));
	
	if (chromosome.single_recombination_map_)
		recombinationWorthShowing = (chromosome.recombination_end_positions_H_.size() > 1);
	else
		recombinationWorthShowing = ((chromosome.recombination_end_positions_M_.size() > 1) || (chromosome.recombination_end_positions_F_.size() > 1));
	
	// If neither map is worth showing, we show just the recombination map, to mirror the behavior of 2.4 and earlier
	if ((!mutationWorthShowing && !recombinationWorthShowing) || (!mutationWorthShowing && recombinationWorthShowing))
	{
		rectCount += [self metalDrawRecombinationIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange withBuffer:vbPtrMod];
	}
	else if (mutationWorthShowing && !recombinationWorthShowing)
	{
		rectCount += [self metalDrawMutationIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange withBuffer:vbPtrMod];
	}
	else	// mutationWorthShowing && recombinationWorthShowing
	{
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		rectCount += [self metalDrawRecombinationIntervalsInInteriorRect:topInteriorRect withController:controller displayedRange:displayedRange withBuffer:vbPtrMod];
		rectCount += [self metalDrawMutationIntervalsInInteriorRect:bottomInteriorRect withController:controller displayedRange:displayedRange withBuffer:vbPtrMod];
	}
	
	return rectCount;
}

#pragma mark Drawing substitutions

- (NSUInteger)metalDrawFixedSubstitutionsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange withBuffer:(SLiMFlatVertex **)vbPtrMod
{
	double scalingFactor = controller->selectionColorScale;
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	Chromosome &chromosome = sim->chromosome_;
	BOOL shouldDrawMutations = [_chromosomeView shouldDrawMutations];
	bool chromosomeHasDefaultColor = !chromosome.color_sub_.empty();
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	if (!vbPtrMod)
	{
		// When being asked for a rect count, we return a maximum count, not an exact count, to avoid excess work
		// The max count is not tremendously large, because it is limited by the way we use radix sorting in our display
		// The code here mirrors the structure of the display code below
		NSUInteger rectCount = 0;
		
		if (substitutions.size() < 1000)
		{
			rectCount = substitutions.size();
		}
		else if (displayedRange.length < interiorRect.size.width)
		{
			// the max count of substitutions.size() could be way too big in this case, so we have to count with position-based clipping as below
			for (const Substitution *substitution : substitutions)
			{
				if (substitution->mutation_type_ptr_->mutation_type_displayed_)
				{
					slim_position_t substitutionPosition = substitution->position_;
					
					if ((substitutionPosition >= (slim_position_t)displayedRange.location) && (substitutionPosition < (slim_position_t)(displayedRange.location + displayedRange.length)))
						rectCount++;
				}
			}
		}
		else
		{
			int displayPixelWidth = (int)interiorRect.size.width;
			
			rectCount = displayPixelWidth;		// radix-based display
		}
		
		return rectCount;
	}
	
	// Set up to draw rects
	float colorRed = 0.2f, colorGreen = 0.2f, colorBlue = 1.0f, colorAlpha = 1.0;
	
	if (chromosomeHasDefaultColor)
	{
		colorRed = chromosome.color_sub_red_;
		colorGreen = chromosome.color_sub_green_;
		colorBlue = chromosome.color_sub_blue_;
	}
	
	if (substitutions.size() < 1000)
	{
		// This is the simple version of the display code, avoiding the memory allocations and such
		for (const Substitution *substitution : substitutions)
		{
			if (substitution->mutation_type_ptr_->mutation_type_displayed_)
			{
				slim_position_t substitutionPosition = substitution->position_;
				NSRect substitutionTickRect = [_chromosomeView rectEncompassingBase:substitutionPosition toBase:substitutionPosition interiorRect:interiorRect displayedRange:displayedRange];
				
				if (!shouldDrawMutations || !chromosomeHasDefaultColor)
				{
					// If we're drawing mutations as well, then substitutions just get colored blue (set above), to contrast
					// If we're not drawing mutations as well, then substitutions get colored by selection coefficient, like mutations
					const MutationType *mutType = substitution->mutation_type_ptr_;
					
					if (!mutType->color_sub_.empty())
					{
						colorRed = mutType->color_sub_red_;
						colorGreen = mutType->color_sub_green_;
						colorBlue = mutType->color_sub_blue_;
					}
					else
					{
						RGBForSelectionCoeff(substitution->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
					}
				}
				
				simd::float4 color = {colorRed, colorGreen, colorBlue, colorAlpha};
				slimMetalFillNSRect(substitutionTickRect, &color, vbPtrMod);
			}
		}
	}
	else if (displayedRange.length < interiorRect.size.width)
	{
		// We hit this case if we are zoomed in such that bars are wider than one pixel; similar to the case above, but check if the position is in range before drawing
		for (const Substitution *substitution : substitutions)
		{
			if (substitution->mutation_type_ptr_->mutation_type_displayed_)
			{
				slim_position_t substitutionPosition = substitution->position_;
				
				if ((substitutionPosition >= (slim_position_t)displayedRange.location) && (substitutionPosition < (slim_position_t)(displayedRange.location + displayedRange.length)))
				{
					NSRect substitutionTickRect = [_chromosomeView rectEncompassingBase:substitutionPosition toBase:substitutionPosition interiorRect:interiorRect displayedRange:displayedRange];
					
					if (!shouldDrawMutations || !chromosomeHasDefaultColor)
					{
						// If we're drawing mutations as well, then substitutions just get colored blue (set above), to contrast
						// If we're not drawing mutations as well, then substitutions get colored by selection coefficient, like mutations
						const MutationType *mutType = substitution->mutation_type_ptr_;
						
						if (!mutType->color_sub_.empty())
						{
							colorRed = mutType->color_sub_red_;
							colorGreen = mutType->color_sub_green_;
							colorBlue = mutType->color_sub_blue_;
						}
						else
						{
							RGBForSelectionCoeff(substitution->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
						}
					}
					
					simd::float4 color = {colorRed, colorGreen, colorBlue, colorAlpha};
					slimMetalFillNSRect(substitutionTickRect, &color, vbPtrMod);
				}
			}
		}
	}
	else
	{
		// We have a lot of substitutions, so do a radix sort, as we do in drawMutationsInInteriorRect: below.
		int displayPixelWidth = (int)interiorRect.size.width;
		const Substitution **subBuffer = (const Substitution **)calloc(displayPixelWidth, sizeof(Substitution *));
		
		for (const Substitution *substitution : substitutions)
		{
			if (substitution->mutation_type_ptr_->mutation_type_displayed_)
			{
				slim_position_t substitutionPosition = substitution->position_;
				double startFraction = (substitutionPosition - (slim_position_t)displayedRange.location) / (double)(displayedRange.length);
				int xPos = (int)floor(startFraction * interiorRect.size.width);
				
				if ((xPos >= 0) && (xPos < displayPixelWidth))
					subBuffer[xPos] = substitution;
			}
		}
		
		if (shouldDrawMutations && chromosomeHasDefaultColor)
		{
			// If we're drawing mutations as well, then substitutions just get colored blue, to contrast
			NSRect mutationTickRect = NSMakeRect(interiorRect.origin.x, interiorRect.origin.y, 1, interiorRect.size.height);
			
			for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
			{
				const Substitution *substitution = subBuffer[binIndex];
				
				if (substitution)
				{
					mutationTickRect.origin.x = interiorRect.origin.x + binIndex;
					mutationTickRect.size.width = 1;
					
					// consolidate adjacent lines together, since they are all the same color
					while ((binIndex + 1 < displayPixelWidth) && subBuffer[binIndex + 1])
					{
						mutationTickRect.size.width++;
						binIndex++;
					}
					
					simd::float4 color = {colorRed, colorGreen, colorBlue, colorAlpha};
					slimMetalFillNSRect(mutationTickRect, &color, vbPtrMod);
				}
			}
		}
		else
		{
			// If we're not drawing mutations as well, then substitutions get colored by selection coefficient, like mutations
			NSRect mutationTickRect = NSMakeRect(interiorRect.origin.x, interiorRect.origin.y, 1, interiorRect.size.height);
			
			for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
			{
				const Substitution *substitution = subBuffer[binIndex];
				
				if (substitution)
				{
					const MutationType *mutType = substitution->mutation_type_ptr_;
					
					if (!mutType->color_sub_.empty())
					{
						colorRed = mutType->color_sub_red_;
						colorGreen = mutType->color_sub_green_;
						colorBlue = mutType->color_sub_blue_;
					}
					else
					{
						RGBForSelectionCoeff(substitution->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
					}
					
					mutationTickRect.origin.x = interiorRect.origin.x + binIndex;
					
					simd::float4 color = {colorRed, colorGreen, colorBlue, colorAlpha};
					slimMetalFillNSRect(mutationTickRect, &color, vbPtrMod);
				}
			}
		}
		
		free(subBuffer);
	}
	
	return 0;	// when drawing, we do not count rects
}

#pragma mark Drawing mutations

- (NSUInteger)metalDrawMutationsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange withBuffer:(SLiMFlatVertex **)vbPtrMod
{
	double scalingFactor = controller->selectionColorScale;
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	double totalGenomeCount = pop.gui_total_genome_count_;				// this includes only genomes in the selected subpopulations
	MutationRun &mutationRegistry = pop.mutation_registry_;
	const MutationIndex *mutations = mutationRegistry.begin_pointer_const();
	int mutationCount = (int)(mutationRegistry.end_pointer_const() - mutations);
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	if (!vbPtrMod)
	{
		// When being asked for a rect count, we return a maximum count, not an exact count, to avoid excess work
		// The max count is not tremendously large, because it is limited by the way we use radix sorting in our display
		// The code here mirrors the structure of the display code below
		NSUInteger rectCount = 0;
		
		if (mutationCount < 1000)
		{
			rectCount = mutationCount;
		}
		else if (displayedRange.length < interiorRect.size.width)
		{
			// the max count of mutationCount could be way too big in this case, so we have to count with position-based clipping as below
			for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
			{
				const Mutation *mutation = mut_block_ptr + mutations[mutIndex];
				slim_position_t mutationPosition = mutation->position_;
				
				if ((mutationPosition >= (slim_position_t)displayedRange.location) && (mutationPosition < (slim_position_t)(displayedRange.location + displayedRange.length)))
					rectCount++;
			}
		}
		else
		{
			int displayPixelWidth = (int)interiorRect.size.width;
			std::map<slim_objectid_t,MutationType*> &mut_types = controller->sim->mutation_types_;
			bool draw_muttypes_sequentially = (mut_types.size() <= 20);	// with a lot of mutation types, the algorithm below becomes very inefficient
			
			for (auto mutationTypeIter = mut_types.begin(); mutationTypeIter != mut_types.end(); ++mutationTypeIter)
			{
				MutationType *mut_type = mutationTypeIter->second;
				
				if (mut_type->mutation_type_displayed_)
				{
					if (draw_muttypes_sequentially)
						rectCount += displayPixelWidth;		// radix-based display
				}
			}
			
			// remaining mutations
			rectCount += std::max(999, displayPixelWidth);	// either a max of 999, or a radix-based display; assume whichever is larger
		}
		
		return rectCount;
	}
	
	// Set up to draw rects
	float colorRed = 0.0f, colorGreen = 0.0f, colorBlue = 0.0f, colorAlpha = 1.0;
	
	if (mutationCount < 1000)
	{
		// This is the simple version of the display code, avoiding the memory allocations and such
		for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
		{
			const Mutation *mutation = mut_block_ptr + mutations[mutIndex];
			const MutationType *mutType = mutation->mutation_type_ptr_;
			
			if (mutType->mutation_type_displayed_)
			{
				slim_position_t mutationPosition = mutation->position_;
				NSRect mutationTickRect = [_chromosomeView rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
				
				if (!mutType->color_.empty())
				{
					colorRed = mutType->color_red_;
					colorGreen = mutType->color_green_;
					colorBlue = mutType->color_blue_;
				}
				else
				{
					RGBForSelectionCoeff(mutation->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
				}
				
				slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
				mutationTickRect.size.height = (int)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
				
				simd::float4 color = {colorRed, colorGreen, colorBlue, colorAlpha};
				slimMetalFillNSRect(mutationTickRect, &color, vbPtrMod);
			}
		}
	}
	else if (displayedRange.length < interiorRect.size.width)
	{
		// We hit this case if we are zoomed in such that bars are wider than one pixel; similar to the case above, but check if the position is in range before drawing
		for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
		{
			const Mutation *mutation = mut_block_ptr + mutations[mutIndex];
			const MutationType *mutType = mutation->mutation_type_ptr_;
			
			if (mutType->mutation_type_displayed_)
			{
				slim_position_t mutationPosition = mutation->position_;
				
				if ((mutationPosition >= (slim_position_t)displayedRange.location) && (mutationPosition < (slim_position_t)(displayedRange.location + displayedRange.length)))
				{
					NSRect mutationTickRect = [_chromosomeView rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
					
					if (!mutType->color_.empty())
					{
						colorRed = mutType->color_red_;
						colorGreen = mutType->color_green_;
						colorBlue = mutType->color_blue_;
					}
					else
					{
						RGBForSelectionCoeff(mutation->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
					}
					
					slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
					mutationTickRect.size.height = (int)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
					
					simd::float4 color = {colorRed, colorGreen, colorBlue, colorAlpha};
					slimMetalFillNSRect(mutationTickRect, &color, vbPtrMod);
				}
			}
		}
	}
	else
	{
		// We have a lot of mutations, so let's try to be smarter.  It's hard to be smarter.  The overhead from allocating the NSColors and such
		// is pretty negligible; practially all the time is spent in NSRectFill().  Unfortunately, NSRectFillListWithColors() provides basically
		// no speedup; Apple doesn't appear to have optimized it.  So, here's what I came up with.  For each mutation type that uses a fixed DFE,
		// and thus a fixed color, we can do a radix sort of mutations into bins corresponding to each pixel in our displayed image.  Then we
		// can draw each bin just once, making one bar for the highest bar in that bin.  Mutations from non-fixed DFEs, and mutations which have
		// had their selection coefficient changed, will be drawn at the end in the usual (slow) way.
		int displayPixelWidth = (int)interiorRect.size.width;
		slim_refcount_t *refcountBuffer = (slim_refcount_t *)malloc(displayPixelWidth * sizeof(slim_refcount_t));
		bool *mutationsPlotted = (bool *)calloc(mutationCount, sizeof(bool));	// faster than using gui_scratch_reference_count_ because of cache locality
		int64_t remainingMutations = mutationCount;
		
		// First zero out the scratch refcount, which we use to track which mutations we have drawn already
		//for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
		//	mutations[mutIndex]->gui_scratch_reference_count_ = 0;
		
		// Then loop through the declared mutation types
		std::map<slim_objectid_t,MutationType*> &mut_types = controller->sim->mutation_types_;
		bool draw_muttypes_sequentially = (mut_types.size() <= 20);	// with a lot of mutation types, the algorithm below becomes very inefficient
		
		for (auto mutationTypeIter = mut_types.begin(); mutationTypeIter != mut_types.end(); ++mutationTypeIter)
		{
			MutationType *mut_type = mutationTypeIter->second;
			
			if (mut_type->mutation_type_displayed_)
			{
				if (draw_muttypes_sequentially)
				{
					bool mut_type_fixed_color = !mut_type->color_.empty();
					
					// We optimize fixed-DFE mutation types only, and those using a fixed color set by the user
					if ((mut_type->dfe_type_ == DFEType::kFixed) || mut_type_fixed_color)
					{
						slim_selcoeff_t mut_type_selcoeff = (mut_type_fixed_color ? 0.0 : (slim_selcoeff_t)mut_type->dfe_parameters_[0]);
						
						EIDOS_BZERO(refcountBuffer, displayPixelWidth * sizeof(slim_refcount_t));
						
						// Scan through the mutation list for mutations of this type with the right selcoeff
						for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
						{
							const Mutation *mutation = mut_block_ptr + mutations[mutIndex];
							
							if ((mutation->mutation_type_ptr_ == mut_type) && (mut_type_fixed_color || (mutation->selection_coeff_ == mut_type_selcoeff)))
							{
								slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// includes only refs from the selected subpopulations
								slim_position_t mutationPosition = mutation->position_;
								//NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
								//int xPos = (int)(mutationTickRect.origin.x - interiorRect.origin.x);
								int xPos = LEFT_OFFSET_OF_BASE(mutationPosition, interiorRect, displayedRange);
								
								if ((xPos >= 0) && (xPos < displayPixelWidth))
									if (mutationRefCount > refcountBuffer[xPos])
										refcountBuffer[xPos] = mutationRefCount;
								
								// tally this mutation as handled
								//mutation->gui_scratch_reference_count_ = 1;
								mutationsPlotted[mutIndex] = true;
								--remainingMutations;
							}
						}
						
						// Now draw all of the mutations we found, by looping through our radix bins
						if (mut_type_fixed_color)
						{
							colorRed = mut_type->color_red_;
							colorGreen = mut_type->color_green_;
							colorBlue = mut_type->color_blue_;
						}
						else
						{
							RGBForSelectionCoeff(mut_type_selcoeff, &colorRed, &colorGreen, &colorBlue, scalingFactor);
						}
						
						simd::float4 color = {colorRed, colorGreen, colorBlue, colorAlpha};
						
						for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
						{
							slim_refcount_t mutationRefCount = refcountBuffer[binIndex];
							
							if (mutationRefCount)
							{
								int16_t height = (int16_t)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
								NSRect mutationTickRect = NSMakeRect(interiorRect.origin.x + binIndex, interiorRect.origin.y, 1, height);
								slimMetalFillNSRect(mutationTickRect, &color, vbPtrMod);
							}
						}
					}
				}
			}
			else
			{
				// We're not displaying this mutation type, so we need to mark off all the mutations belonging to it as handled
				for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
				{
					const Mutation *mutation = mut_block_ptr + mutations[mutIndex];
					
					if (mutation->mutation_type_ptr_ == mut_type)
					{
						// tally this mutation as handled
						//mutation->gui_scratch_reference_count_ = 1;
						mutationsPlotted[mutIndex] = true;
						--remainingMutations;
					}
				}
			}
		}
		
		// Draw any undrawn mutations on top; these are guaranteed not to use a fixed color set by the user, since those are all handled above
		if (remainingMutations)
		{
			if (remainingMutations < 1000)
			{
				// Plot the remainder by brute force, since there are not that many
				for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
				{
					//if (mutation->gui_scratch_reference_count_ == 0)
					if (!mutationsPlotted[mutIndex])
					{
						const Mutation *mutation = mut_block_ptr + mutations[mutIndex];
						slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
						slim_position_t mutationPosition = mutation->position_;
						NSRect mutationTickRect = [_chromosomeView rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
						
						mutationTickRect.size.height = (int)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
						RGBForSelectionCoeff(mutation->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
						
						simd::float4 color = {colorRed, colorGreen, colorBlue, colorAlpha};
						
						slimMetalFillNSRect(mutationTickRect, &color, vbPtrMod);
					}
				}
			}
			else
			{
				// OK, we have a lot of mutations left to draw.  Here we will again use the radix sort trick, to keep track of only the tallest bar in each column
				MutationIndex *mutationBuffer = (MutationIndex *)calloc(displayPixelWidth,  sizeof(MutationIndex));
				
				EIDOS_BZERO(refcountBuffer, displayPixelWidth * sizeof(slim_refcount_t));
				
				// Find the tallest bar in each column
				for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
				{
					//if (mutation->gui_scratch_reference_count_ == 0)
					if (!mutationsPlotted[mutIndex])
					{
						MutationIndex mutationBlockIndex = mutations[mutIndex];
						const Mutation *mutation = mut_block_ptr + mutationBlockIndex;
						slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
						slim_position_t mutationPosition = mutation->position_;
						//NSRect mutationTickRect = [_chromosomeView rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
						//int xPos = (int)(mutationTickRect.origin.x - interiorRect.origin.x);
						int xPos = LEFT_OFFSET_OF_BASE(mutationPosition, interiorRect, displayedRange);
						
						if ((xPos >= 0) && (xPos < displayPixelWidth))
						{
							if (mutationRefCount > refcountBuffer[xPos])
							{
								refcountBuffer[xPos] = mutationRefCount;
								mutationBuffer[xPos] = mutationBlockIndex;
							}
						}
					}
				}
				
				// Now plot the bars
				for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
				{
					int mutationRefCount = refcountBuffer[binIndex];
					
					if (mutationRefCount)
					{
						int16_t height = (int16_t)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
						NSRect mutationTickRect = NSMakeRect(interiorRect.origin.x + binIndex, interiorRect.origin.y, 1, height);
						const Mutation *mutation = mut_block_ptr + mutationBuffer[binIndex];
						
						RGBForSelectionCoeff(mutation->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
						
						simd::float4 color = {colorRed, colorGreen, colorBlue, colorAlpha};
						
						slimMetalFillNSRect(mutationTickRect, &color, vbPtrMod);
					}
				}
				
				free(mutationBuffer);
			}
		}
		
		free(refcountBuffer);
		free(mutationsPlotted);
	}
	
	return 0;	// when drawing, we do not count rects
}

#pragma mark Other drawing

- (NSUInteger)drawInVertexBuffer
{
	// We render in two passes: first we add up how many triangles we need to fill, and then we get a vertex buffer
	// and define the triangles.  If this proves inefficient, we can cache work between the two stages, perhaps.
	NSUInteger rectCount = 0;	// accurate only at the end of pass 0 / beginning of pass 1; pass 1 is not required to count correctly
	NSUInteger vertexCount = 0;	// accurate at the end of pass 1
	
	SLiMHaplotypeManager *haplotypeManager = nil;	// created in pass 0 and kept through pass 1
	
	for (int pass = 0; pass <= 1; ++pass)
	{
		id<MTLBuffer> buffer = nil;
		SLiMFlatVertex *vbPtr = NULL;
		
		if (pass == 1)
		{
			// calculate the vertex buffeer size needed, and get a buffer at least that large
			vertexCount = rectCount * 2 * 3;						// 2 triangles per rect, 3 vertices per triangle
			buffer = [self takeVertexBufferWithCapacity:vertexCount * sizeof(SLiMFlatVertex)];	// sizeof() includes data alignment padding
			
			// Now we can get our pointer into the vertex buffer
			vbPtr = (SLiMFlatVertex *)[buffer contents];
		}
		
		SLiMFlatVertex **vbPtrMod = (vbPtr ? &vbPtr : NULL);
		SLiMWindowController *controller = (SLiMWindowController *)[[self window] windowController];
		bool ready = ([_chromosomeView enabled] && ![controller invalidSimulation]);
		NSRect interiorRect = [self bounds];
		
		// if the simulation is at generation 0, it is not ready
		if (ready)
			if (controller->sim->generation_ == 0)
				ready = NO;
		
		if (ready)
		{
			// erase the content area itself
			static simd::float4 fillColor = {0.0f, 0.0f, 0.0f, 1.0f};
			
			rectCount++;
			if (pass == 1)
				slimMetalFillNSRect(interiorRect, &fillColor, vbPtrMod);
			
			NSRange displayedRange = [_chromosomeView displayedRange];
			BOOL shouldDrawGenomicElements = [_chromosomeView shouldDrawGenomicElements];
			BOOL shouldDrawRateMaps = [_chromosomeView shouldDrawRateMaps];
			BOOL shouldDrawMutations = [_chromosomeView shouldDrawMutations];
			BOOL shouldDrawFixedSubstitutions = [_chromosomeView shouldDrawFixedSubstitutions];
			BOOL display_haplotypes = _chromosomeView->display_haplotypes_;
			
			BOOL splitHeight = (shouldDrawRateMaps && shouldDrawGenomicElements);
			NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
			CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
			CGFloat remainingHeight = interiorRect.size.height - halfHeight;
			
			topInteriorRect.size.height = halfHeight;
			topInteriorRect.origin.y += remainingHeight;
			bottomInteriorRect.size.height = remainingHeight;
			
			// draw recombination intervals in interior
			if (shouldDrawRateMaps)
				rectCount += [self metalDrawRateMapsInInteriorRect:(splitHeight ? topInteriorRect : interiorRect) withController:controller displayedRange:displayedRange withBuffer:vbPtrMod];
			
			// draw genomic elements in interior
			if (shouldDrawGenomicElements)
				rectCount += [self metalDrawGenomicElementsInInteriorRect:(splitHeight ? bottomInteriorRect : interiorRect) withController:controller displayedRange:displayedRange withBuffer:vbPtrMod];
			
			// figure out which mutation types we're displaying
			if (shouldDrawFixedSubstitutions || shouldDrawMutations)
				[_chromosomeView updateDisplayedMutationTypes];
			
			// draw fixed substitutions in interior
			if (shouldDrawFixedSubstitutions)
				rectCount += [self metalDrawFixedSubstitutionsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange withBuffer:vbPtrMod];
			
			// draw mutations in interior
			if (shouldDrawMutations)
			{
				if (display_haplotypes)
				{
					// display mutations as a haplotype plot, courtesy of SLiMHaplotypeManager; we use kSLiMHaplotypeClusterNearestNeighbor and
					// kSLiMHaplotypeClusterNoOptimization because they're fast, and NN might also provide a bit more run-to-run continuity
					int interiorHeight = (int)ceil(interiorRect.size.height);	// one sample per available pixel line, for simplicity and speed; 47, in the current UI layout
					
					if (pass == 0)
						haplotypeManager = [[SLiMHaplotypeManager alloc] initWithClusteringMethod:kSLiMHaplotypeClusterNearestNeighbor optimizationMethod:kSLiMHaplotypeClusterNoOptimization sourceController:controller sampleSize:interiorHeight clusterInBackground:NO];
					
					rectCount += [haplotypeManager metalDrawHaplotypesInRect:interiorRect displayBlackAndWhite:NO showSubpopStrips:NO eraseBackground:NO previousFirstBincounts:&(_chromosomeView->haplotype_previous_bincounts) withBuffer:vbPtrMod];
					
					// it's a little bit odd to throw away haplotypeManager here; if the user drag-resizes the window, we do a new display each
					// time, with a new sample, and so the haplotype display changes with every pixel resize change; we could keep this...?
					if (pass == 1)
						[haplotypeManager release];
				}
				else
				{
					// display mutations as a frequency plot; this is the standard display mode
					rectCount += [self metalDrawMutationsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange withBuffer:vbPtrMod];
				}
			}
			
			// overlay the selection last, since it bridges over the frame
			if (_chromosomeView->hasSelection)
				NSLog(@"Selection set on a ChromosomeView that is drawing using Metal!");
		}
		else
		{
			// erase the content area itself
			static simd::float4 fillColor = {0.88f, 0.88f, 0.88f, 1.0f};
			
			rectCount++;
			if (pass == 1)
				slimMetalFillNSRect(interiorRect, &fillColor, vbPtrMod);
		}
		
		if (pass == 1)
		{
			// Check that we added the planned number of vertices; note that for efficiency reasons our count is a *maximum* count, not an exact count, in this method
			if (vbPtr > (SLiMFlatVertex *)[buffer contents] + vertexCount)
				NSLog(@"vertex count overflow in drawInVertexBuffer; expected %ld, drew %ld!", vertexCount, vbPtr - (SLiMFlatVertex *)[buffer contents]);
			
			// Since the vertex count can be an overestimate, return the realized vertex count
			vertexCount = vbPtr - (SLiMFlatVertex *)[buffer contents];
		}
	}
	
	return vertexCount;
}

- (void)drawWithRenderEncoder:(id<MTLRenderCommandEncoder>_Nonnull)renderEncoder
{
	[renderEncoder setRenderPipelineState:flatPipelineState_OriginBottom_];
	
	NSUInteger vertexCount = [self drawInVertexBuffer];
	
	[renderEncoder setVertexBuffer:vertexBuffers_[currentBuffer_] offset:0 atIndex:SLiMVertexInputIndexVertices];
	[renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertexCount];
}

@end


































