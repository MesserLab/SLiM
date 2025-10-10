//
//  ChromosomeView.m
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
//  Copyright (c) 2015-2025 Benjamin C. Haller.  All rights reserved.
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


#import "ChromosomeView.h"
#import "SLiMWindowController.h"
#import "CocoaExtra.h"

#include "community.h"


NSString *SLiMChromosomeSelectionChangedNotification = @"SLiMChromosomeSelectionChangedNotification";

static NSDictionary *tickAttrs = nil;
static const int numberOfTicksPlusOne = 4;
static const int tickLength = 5;
static const int heightForTicks = 16;
static const int spaceBetweenChromosomes = 5;


@implementation ChromosomeView

@synthesize shouldDrawGenomicElements, shouldDrawFixedSubstitutions, shouldDrawMutations, shouldDrawRateMaps;

+ (void)initialize
{
	if (!tickAttrs)
		tickAttrs = [@{NSForegroundColorAttributeName : [NSColor blackColor], NSFontAttributeName : [NSFont systemFontOfSize:9.0]} retain];
	
	[self exposeBinding:@"enabled"];
}

- (id)initWithFrame:(NSRect)frameRect
{
	if (self = [super initWithFrame:frameRect])
	{
		// this is not called for objects in the nib (I imagine initWithCoder: is instead); put stuff in awakeFromNib
	}
	
	return self;
}

- (void)setEnabled:(BOOL)enabled
{
	if (_enabled != enabled)
	{
		_enabled = enabled;
		[self setNeedsDisplayAll];
	}
}

- (void)setOverview:(BOOL)overview
{
	if (_overview != overview)
	{
		_overview = overview;
		[self setNeedsDisplayAll];
	}
}

- (void)awakeFromNib
{
	display_muttypes_.clear();
	
	[self bind:@"enabled" toObject:[[self window] windowController] withKeyPath:@"invalidSimulation" options:@{NSValueTransformerNameBindingOption : NSNegateBooleanTransformerName}];
}

- (void)dealloc
{
	[self unbind:@"enabled"];
	
	[super dealloc];
}

- (NSRange)displayedRangeForChromosome:(Chromosome *)chromosome
{
	slim_position_t chromosomeLastPosition = chromosome->last_position_;
	
	return NSMakeRange(0, chromosomeLastPosition + 1);	// chromosomeLastPosition + 1 bases are encompassed
}

- (NSRect)rectEncompassingBase:(slim_position_t)startBase toBase:(slim_position_t)endBase interiorRect:(NSRect)interiorRect displayedRange:(NSRange)displayedRange
{
	double startFraction = (startBase - (slim_position_t)displayedRange.location) / (double)(displayedRange.length);
	double leftEdgeDouble = interiorRect.origin.x + startFraction * interiorRect.size.width;
	double endFraction = (endBase + 1 - (slim_position_t)displayedRange.location) / (double)(displayedRange.length);
	double rightEdgeDouble = interiorRect.origin.x + endFraction * interiorRect.size.width;
	int leftEdge, rightEdge;
	
	if (rightEdgeDouble - leftEdgeDouble > 1.0)
	{
		// If the range spans a width of more than one pixel, then use the maximal pixel range
		leftEdge = (int)floor(leftEdgeDouble);
		rightEdge = (int)ceil(rightEdgeDouble);
	}
	else
	{
		// If the range spans a pixel or less, make sure that we end up with a range that is one pixel wide, even if the left-right positions span a pixel boundary
		leftEdge = (int)floor(leftEdgeDouble);
		rightEdge = leftEdge + 1;
	}
	
	return NSMakeRect(leftEdge, interiorRect.origin.y, rightEdge - leftEdge, interiorRect.size.height);
}

- (NSRect)contentRect
{
	NSRect bounds = [self bounds];
	
	// Two things are going on here.  The width gets inset by two pixels on each side because our frame is outset that much from our apparent frame, to
	// make room for the selection knobs to spill over a bit.  The height gets adjusted because our "content rect" does not include our ticks.
	if ([self overview])
		return NSMakeRect(bounds.origin.x + 2, bounds.origin.y, bounds.size.width - 4, bounds.size.height);
	else
		return NSMakeRect(bounds.origin.x + 2, bounds.origin.y + heightForTicks, bounds.size.width - 4, bounds.size.height - heightForTicks);
}

- (NSRect)interiorRect
{
	return NSInsetRect([self contentRect], 1, 1);
}

- (void)setNeedsDisplayAll
{
	[self setNeedsDisplay:YES];
}

- (void)setNeedsDisplayInInterior
{
	[self setNeedsDisplayInRect:[self interiorRect]];
}

// This is a fast macro for when all we need is the offset of a base from the left edge of interiorRect; interiorRect.origin.x is not added here!
// This is based on the same math as rectEncompassingBase:toBase:interiorRect:displayedRange: above, and must be kept in synch with that method.
#define LEFT_OFFSET_OF_BASE(startBase, interiorRect, displayedRange) ((int)floor(((startBase - (slim_position_t)displayedRange.location) / (double)(displayedRange.length)) * interiorRect.size.width))

- (slim_position_t)baseForPosition:(double)position interiorRect:(NSRect)interiorRect displayedRange:(NSRange)displayedRange
{
	double fraction = (position - interiorRect.origin.x) / interiorRect.size.width;
	slim_position_t base = (slim_position_t)floor(fraction * (displayedRange.length + 1) + displayedRange.location);
	
	return base;
}

- (void)drawTicksInContentRect:(NSRect)contentRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	NSRect interiorRect = NSInsetRect(contentRect, 1, 1);
	
	if (displayedRange.length == 0)
	{
		// Handle the "no genetics" case separately
		if (![self overview])
		{
			NSAttributedString *tickAttrLabel = [[NSAttributedString alloc] initWithString:@"no genetics" attributes:tickAttrs];
			NSSize tickLabelSize = [tickAttrLabel size];
			int tickLabelX = (int)floor(interiorRect.origin.x + (interiorRect.size.width - tickLabelSize.width) / 2.0);
			
			[tickAttrLabel drawAtPoint:NSMakePoint(tickLabelX, contentRect.origin.y - 14)];
			[tickAttrLabel release];
		}
		
		return;
	}
	
	int64_t lastTickIndex = numberOfTicksPlusOne;
	
	// Display fewer ticks when we are displaying a very small number of positions
	lastTickIndex = MIN(lastTickIndex, ((int64_t)displayedRange.length + 1) / 3);
	
	double tickIndexDivisor = ((lastTickIndex == 0) ? 1.0 : (double)lastTickIndex);		// avoid a divide by zero when we are displaying a single site
	
	// BCH 12/25/2024: Start by measuring the tick labels and figuring out who fits.  FIXME we could be even smarter
	// and switch to scientific notation if things get too crowded, but I'll leave that for another day.
	double widthAllLabels = 0.0, widthLeftRightLabels = 0.0, widthRightLabelOnly = 0.0;
	
	for (int tickIndex = 0; tickIndex <= lastTickIndex; ++tickIndex)
	{
		slim_position_t tickBase = (slim_position_t)displayedRange.location + (slim_position_t)ceil((displayedRange.length - 1) * (tickIndex / tickIndexDivisor));	// -1 because we are choosing an in-between-base position that falls, at most, to the left of the last base
		NSString *tickLabel;
		
		if (tickBase >= 1e10)
			tickLabel = [NSString stringWithFormat:@"%.6e", (double)tickBase];
		else
			tickLabel = [NSString stringWithFormat:@"%lld", (long long int)tickBase];
		
		NSAttributedString *tickAttrLabel = [[NSAttributedString alloc] initWithString:tickLabel attributes:tickAttrs];
		NSSize tickLabelSize = [tickAttrLabel size];
		int tickLabelWidth = 5 + (int)tickLabelSize.width;
		[tickAttrLabel release];
		
		widthAllLabels += tickLabelWidth;
		if (tickIndex == 0)
		{
			widthLeftRightLabels += tickLabelWidth;
		}
		if (tickIndex == lastTickIndex)
		{
			widthLeftRightLabels += tickLabelWidth;
			widthRightLabelOnly += tickLabelWidth;
		}
	}
	
	for (int tickIndex = 0; tickIndex <= lastTickIndex; ++tickIndex)
	{
		// BCH 12/25/2024: If we're not going to draw the middle tick labels, skip their tick marks also;
		// and if we're not going to draw any tick labels at all, then skip drawing all the tick marks
		int interiorWidth = (int)interiorRect.size.width;
		
		if ((widthRightLabelOnly > interiorWidth) ||
			((widthAllLabels > interiorWidth) && (tickIndex != 0) && (tickIndex != lastTickIndex)))
			continue;
		
		slim_position_t tickBase = (slim_position_t)displayedRange.location + (slim_position_t)ceil((displayedRange.length - 1) * (tickIndex / tickIndexDivisor));	// -1 because we are choosing an in-between-base position that falls, at most, to the left of the last base
		NSRect tickRect = [self rectEncompassingBase:tickBase toBase:tickBase interiorRect:interiorRect displayedRange:displayedRange];
		
		tickRect.origin.y = contentRect.origin.y - tickLength;
		tickRect.size.height = tickLength;
		
		// if we are displaying a single site or two sites, make a tick mark one pixel wide, rather than a very wide one, which looks weird
		if (displayedRange.length <= 2)
		{
			tickRect.origin.x = (int)floor(tickRect.origin.x + tickRect.size.width / 2.0 - 0.5);
			tickRect.size.width = 1.0;
		}
		
		[[NSColor colorWithCalibratedWhite:0.5 alpha:1.0] set];
		NSRectFill(tickRect);
		
		// BCH 12/25/2024: Using the measurements taken above, decide whether to draw this tick label or not
		if (widthAllLabels > interiorWidth)
		{
			if ((tickIndex != 0) && (tickIndex != lastTickIndex))
				continue;
			if (widthLeftRightLabels > interiorWidth)
			{
				if (tickIndex != lastTickIndex)
					continue;
				if (widthRightLabelOnly > interiorWidth)
					continue;
			}
		}
		
		// BCH 15 May 2018: display in scientific notation for positions at or above 1e10, as it gets a bit ridiculous...
		NSString *tickLabel;
		
		if (tickBase >= 1e10)
			tickLabel = [NSString stringWithFormat:@"%.6e", (double)tickBase];
		else
			tickLabel = [NSString stringWithFormat:@"%lld", (long long int)tickBase];
		
		NSAttributedString *tickAttrLabel = [[NSAttributedString alloc] initWithString:tickLabel attributes:tickAttrs];
		NSSize tickLabelSize = [tickAttrLabel size];
		int tickLabelX = (int)floor(tickRect.origin.x + tickRect.size.width / 2.0);
		BOOL forceCenteredLabel = (displayedRange.length <= 101);	// a selected subrange is never <=101 length, so this is safe even with large chromosomes
		
		if ((tickIndex == lastTickIndex) && !forceCenteredLabel)
			tickLabelX -= (int)round(tickLabelSize.width - 2);
		else if ((tickIndex > 0) || forceCenteredLabel)
			tickLabelX -= (int)round(tickLabelSize.width / 2.0);
		
		[tickAttrLabel drawAtPoint:NSMakePoint(tickLabelX, contentRect.origin.y - (tickLength + 12))];
		[tickAttrLabel release];
	}
}

- (void)drawGenomicElementsInInteriorRect:(NSRect)interiorRect chromosome:(Chromosome *)chromosome withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	CGFloat previousIntervalLeftEdge = -10000;
	
	for (GenomicElement *genomicElement : chromosome->GenomicElements())
	{
		slim_position_t startPosition = genomicElement->start_position_;
		slim_position_t endPosition = genomicElement->end_position_;
		NSRect elementRect = [self rectEncompassingBase:startPosition toBase:endPosition interiorRect:interiorRect displayedRange:displayedRange];
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
			GenomicElementType *geType = genomicElement->genomic_element_type_ptr_;
			NSColor *elementColor = [controller colorForGenomicElementType:geType withID:geType->genomic_element_type_id_];
			
			[elementColor set];
			NSRectFill(elementRect);
			
			// if this interval is just one pixel wide, we want to try to make it visible, by avoiding overdrawing it; so we remember its location
			if (widthOne)
				previousIntervalLeftEdge = elementRect.origin.x;
			else
				previousIntervalLeftEdge = -10000;
		}
	}
}

- (void)_drawRateMapIntervalsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange ends:(std::vector<slim_position_t> &)ends rates:(std::vector<double> &)rates
{
	int recombinationIntervalCount = (int)ends.size();
	slim_position_t intervalStartPosition = 0;
	CGFloat previousIntervalLeftEdge = -10000;
	
	for (int interval = 0; interval < recombinationIntervalCount; ++interval)
	{
		slim_position_t intervalEndPosition = ends[interval];
		double intervalRate = rates[interval];
		NSRect intervalRect = [self rectEncompassingBase:intervalStartPosition toBase:intervalEndPosition interiorRect:interiorRect displayedRange:displayedRange];
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
			// color according to how "hot" the region is
			double brightness = 0.0;
			double saturation = 1.0;
			
			if (intervalRate > 0.0)
			{
				if (intervalRate > 1.0e-8)
				{
					if (intervalRate < 5.0e-8)
					{
						brightness = 0.5 + 0.5 * ((intervalRate - 1.0e-8) / 4.0e-8);
					}
					else
					{
						brightness = 1.0;
						
						if (intervalRate < 1.0e-7)
							saturation = 1.0 - ((intervalRate - 5.0e-8) * 2.0e7);
						else
							saturation = 0.0;
					}
				}
				else
				{
					brightness = 0.5;
				}
			}
			NSColor *intervalColor = [NSColor colorWithCalibratedHue:0.65 saturation:saturation brightness:brightness alpha:1.0];
			
			[intervalColor set];
			NSRectFill(intervalRect);
			
			// if this interval is just one pixel wide, we want to try to make it visible, by avoiding overdrawing it; so we remember its location
			if (widthOne)
				previousIntervalLeftEdge = intervalRect.origin.x;
			else
				previousIntervalLeftEdge = -10000;
		}
		
		// the next interval starts at the next base after this one ended
		intervalStartPosition = intervalEndPosition + 1;
	}
}

- (void)drawRecombinationIntervalsInInteriorRect:(NSRect)interiorRect chromosome:(Chromosome *)chromosome withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	if (chromosome->single_recombination_map_)
	{
		[self _drawRateMapIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange ends:chromosome->recombination_end_positions_H_ rates:chromosome->recombination_rates_H_];
	}
	else
	{
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		[self _drawRateMapIntervalsInInteriorRect:topInteriorRect withController:controller displayedRange:displayedRange ends:chromosome->recombination_end_positions_M_ rates:chromosome->recombination_rates_M_];
		[self _drawRateMapIntervalsInInteriorRect:bottomInteriorRect withController:controller displayedRange:displayedRange ends:chromosome->recombination_end_positions_F_ rates:chromosome->recombination_rates_F_];
	}
}

- (void)drawMutationIntervalsInInteriorRect:(NSRect)interiorRect chromosome:(Chromosome *)chromosome withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	if (chromosome->single_mutation_map_)
	{
		[self _drawRateMapIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange ends:chromosome->mutation_end_positions_H_ rates:chromosome->mutation_rates_H_];
	}
	else
	{
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		[self _drawRateMapIntervalsInInteriorRect:topInteriorRect withController:controller displayedRange:displayedRange ends:chromosome->mutation_end_positions_M_ rates:chromosome->mutation_rates_M_];
		[self _drawRateMapIntervalsInInteriorRect:bottomInteriorRect withController:controller displayedRange:displayedRange ends:chromosome->mutation_end_positions_F_ rates:chromosome->mutation_rates_F_];
	}
}

- (void)drawRateMapsInInteriorRect:(NSRect)interiorRect chromosome:(Chromosome *)chromosome withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	BOOL recombinationWorthShowing = NO;
	BOOL mutationWorthShowing = NO;
	
	if (chromosome->single_mutation_map_)
		mutationWorthShowing = (chromosome->mutation_end_positions_H_.size() > 1);
	else
		mutationWorthShowing = ((chromosome->mutation_end_positions_M_.size() > 1) || (chromosome->mutation_end_positions_F_.size() > 1));
	
	if (chromosome->single_recombination_map_)
		recombinationWorthShowing = (chromosome->recombination_end_positions_H_.size() > 1);
	else
		recombinationWorthShowing = ((chromosome->recombination_end_positions_M_.size() > 1) || (chromosome->recombination_end_positions_F_.size() > 1));
	
	// If neither map is worth showing, we show just the recombination map, to mirror the behavior of 2.4 and earlier
	if ((!mutationWorthShowing && !recombinationWorthShowing) || (!mutationWorthShowing && recombinationWorthShowing))
	{
		[self drawRecombinationIntervalsInInteriorRect:interiorRect chromosome:chromosome withController:controller displayedRange:displayedRange];
	}
	else if (mutationWorthShowing && !recombinationWorthShowing)
	{
		[self drawMutationIntervalsInInteriorRect:interiorRect chromosome:chromosome withController:controller displayedRange:displayedRange];
	}
	else	// mutationWorthShowing && recombinationWorthShowing
	{
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		[self drawRecombinationIntervalsInInteriorRect:topInteriorRect chromosome:chromosome withController:controller displayedRange:displayedRange];
		[self drawMutationIntervalsInInteriorRect:bottomInteriorRect chromosome:chromosome withController:controller displayedRange:displayedRange];
	}
}

- (void)drawFixedSubstitutionsInInteriorRect:(NSRect)interiorRect chromosome:(Chromosome *)chromosome withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	double scalingFactor = 0.8; // used to be controller->selectionColorScale;
	Species *displaySpecies = [controller focalDisplaySpecies];
	Population &pop = displaySpecies->population_;
	bool chromosomeHasDefaultColor = !chromosome->color_sub_.empty();
	
	float colorRed = 0.2f, colorGreen = 0.2f, colorBlue = 1.0f;
	
	if (chromosomeHasDefaultColor)
	{
		colorRed = chromosome->color_sub_red_;
		colorGreen = chromosome->color_sub_green_;
		colorBlue = chromosome->color_sub_blue_;
	}
	
	[[NSColor colorWithCalibratedRed:colorRed green:colorGreen blue:colorBlue alpha:1.0] set];
	
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	if ((substitutions.size() < 1000) || (displayedRange.length < interiorRect.size.width))
	{
		// This is the simple version of the display code, avoiding the memory allocations and such
		for (const Substitution *substitution : substitutions)
		{
			if (substitution->mutation_type_ptr_->mutation_type_displayed_)
			{
				slim_position_t substitutionPosition = substitution->position_;
				NSRect substitutionTickRect = [self rectEncompassingBase:substitutionPosition toBase:substitutionPosition interiorRect:interiorRect displayedRange:displayedRange];
				
				if (!shouldDrawMutations || !chromosomeHasDefaultColor)
				{
					// If we're drawing mutations as well, then substitutions just get colored blue, to contrast
					// If we're not drawing mutations as well, then substitutions get colored by selection coefficient, like mutations
					const MutationType *mutType = substitution->mutation_type_ptr_;
					
					if (!mutType->color_sub_.empty())
					{
						[[NSColor colorWithCalibratedRed:mutType->color_sub_red_ green:mutType->color_sub_green_ blue:mutType->color_sub_blue_ alpha:1.0] set];
					}
					else
					{
						RGBForSelectionCoeff(substitution->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
						[[NSColor colorWithCalibratedRed:colorRed green:colorGreen blue:colorBlue alpha:1.0] set];
					}
				}
				
				NSRectFill(substitutionTickRect);
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
					
					NSRectFill(mutationTickRect);
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
						[[NSColor colorWithCalibratedRed:mutType->color_sub_red_ green:mutType->color_sub_green_ blue:mutType->color_sub_blue_ alpha:1.0] set];
					}
					else
					{
						RGBForSelectionCoeff(substitution->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
						[[NSColor colorWithCalibratedRed:colorRed green:colorGreen blue:colorBlue alpha:1.0] set];
					}
					
					mutationTickRect.origin.x = interiorRect.origin.x + binIndex;
					NSRectFill(mutationTickRect);
				}
			}
		}
		
		free(subBuffer);
	}
}

- (void)updateDisplayedMutationTypes
{
	// We use a flag in MutationType to indicate whether we're drawing that type or not; we update those flags here,
	// before every drawing of mutations, from the vector of mutation type IDs that we keep internally
	SLiMWindowController *controller = (SLiMWindowController *)[[self window] windowController];
	
	if (controller)
	{
		Species *displaySpecies = [controller focalDisplaySpecies];
		
		if (displaySpecies)
		{
			std::map<slim_objectid_t,MutationType*> &muttypes = displaySpecies->mutation_types_;
			
			for (auto muttype_iter : muttypes)
			{
				MutationType *muttype = muttype_iter.second;
				
				if (display_muttypes_.size())
				{
					slim_objectid_t muttype_id = muttype->mutation_type_id_;
					
					muttype->mutation_type_displayed_ = (std::find(display_muttypes_.begin(), display_muttypes_.end(), muttype_id) != display_muttypes_.end());
				}
				else
				{
					muttype->mutation_type_displayed_ = true;
				}
			}
		}
	}
}

- (void)drawMutationsInInteriorRect:(NSRect)interiorRect chromosome:(Chromosome *)chromosome withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	double scalingFactor = 0.8; // used to be controller->selectionColorScale;
	Species *displaySpecies = [controller focalDisplaySpecies];
	slim_chromosome_index_t chromosome_index = chromosome->Index();
	Population &pop = displaySpecies->population_;
	double totalHaplosomeCount = chromosome->gui_total_haplosome_count_;				// this includes only haplosomes in the selected subpopulations
	int registry_size;
	const MutationIndex *registry = pop.MutationRegistry(&registry_size);
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	if ((registry_size < 1000) || (displayedRange.length < interiorRect.size.width))
	{
		// This is the simple version of the display code, avoiding the memory allocations and such
		for (int registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			const Mutation *mutation = mut_block_ptr + registry[registry_index];
			
			if (mutation->chromosome_index_ == chromosome_index)	// display only mutations in the displayed chromosome
			{
				const MutationType *mutType = mutation->mutation_type_ptr_;
				
				if (mutType->mutation_type_displayed_)
				{
					slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
					slim_position_t mutationPosition = mutation->position_;
					NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
					
					if (!mutType->color_.empty())
					{
						[[NSColor colorWithCalibratedRed:mutType->color_red_ green:mutType->color_green_ blue:mutType->color_blue_ alpha:1.0] set];
					}
					else
					{
						float colorRed = 0.0, colorGreen = 0.0, colorBlue = 0.0;
						RGBForSelectionCoeff(mutation->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
						[[NSColor colorWithCalibratedRed:colorRed green:colorGreen blue:colorBlue alpha:1.0] set];
					}
					
					mutationTickRect.size.height = (int)ceil((mutationRefCount / totalHaplosomeCount) * interiorRect.size.height);
					NSRectFill(mutationTickRect);
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
		float colorRed = 0.0, colorGreen = 0.0, colorBlue = 0.0;
		int displayPixelWidth = (int)interiorRect.size.width;
		int16_t *heightBuffer = (int16_t *)malloc(displayPixelWidth * sizeof(int16_t));
		bool *mutationsPlotted = (bool *)calloc(registry_size, sizeof(bool));	// faster than using gui_scratch_reference_count_ because of cache locality
		int64_t remainingMutations = registry_size;
		
		// First zero out the scratch refcount, which we use to track which mutations we have drawn already
		//for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
		//	mutations[mutIndex]->gui_scratch_reference_count_ = 0;
		
		// Then loop through the declared mutation types
		for (auto mutationTypeIter : displaySpecies->mutation_types_)
		{
			MutationType *mut_type = mutationTypeIter.second;
			
			if (mut_type->mutation_type_displayed_)
			{
				bool mut_type_fixed_color = !mut_type->color_.empty();
				EffectDistributionInfo &ed_info = mut_type->effect_distributions_[0];	// FIXME MULTITRAIT
				
				// We optimize fixed-DFE mutation types only, and those using a fixed color set by the user
				if ((ed_info.dfe_type_ == DFEType::kFixed) || mut_type_fixed_color)
				{
					slim_effect_t mut_type_selcoeff = (mut_type_fixed_color ? 0.0 : (slim_effect_t)ed_info.dfe_parameters_[0]);
					
					EIDOS_BZERO(heightBuffer, displayPixelWidth * sizeof(int16_t));
					
					// Scan through the mutation list for mutations of this type with the right selcoeff
					for (int registry_index = 0; registry_index < registry_size; ++registry_index)
					{
						const Mutation *mutation = mut_block_ptr + registry[registry_index];
						
						if ((mutation->mutation_type_ptr_ == mut_type) && (mut_type_fixed_color || (mutation->selection_coeff_ == mut_type_selcoeff)))
						{
							if (mutation->chromosome_index_ == chromosome_index)
							{
								slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// includes only refs from the selected subpopulations
								slim_position_t mutationPosition = mutation->position_;
								//NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
								//int xPos = (int)(mutationTickRect.origin.x - interiorRect.origin.x);
								int xPos = LEFT_OFFSET_OF_BASE(mutationPosition, interiorRect, displayedRange);
								int16_t height = (int16_t)ceil((mutationRefCount / totalHaplosomeCount) * interiorRect.size.height);
								
								if ((xPos >= 0) && (xPos < displayPixelWidth))
									if (height > heightBuffer[xPos])
										heightBuffer[xPos] = height;
							}
							
							// tally this mutation as handled
							//mutation->gui_scratch_reference_count_ = 1;
							mutationsPlotted[registry_index] = true;
							--remainingMutations;
						}
					}
					
					// Now draw all of the mutations we found, by looping through our radix bins
					if (mut_type_fixed_color)
					{
						[[NSColor colorWithCalibratedRed:mut_type->color_red_ green:mut_type->color_green_ blue:mut_type->color_blue_ alpha:1.0] set];
					}
					else
					{
						RGBForSelectionCoeff(mut_type_selcoeff, &colorRed, &colorGreen, &colorBlue, scalingFactor);
						[[NSColor colorWithCalibratedRed:colorRed green:colorGreen blue:colorBlue alpha:1.0] set];
					}
					
					for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
					{
						int height = heightBuffer[binIndex];
						
						if (height)
						{
							NSRect mutationTickRect = NSMakeRect(interiorRect.origin.x + binIndex, interiorRect.origin.y, 1, height);
							
							NSRectFill(mutationTickRect);
						}
					}
				}
			}
			else
			{
				// We're not displaying this mutation type, so we need to mark off all the mutations belonging to it as handled
				for (int registry_index = 0; registry_index < registry_size; ++registry_index)
				{
					const Mutation *mutation = mut_block_ptr + registry[registry_index];
					
					if (mutation->mutation_type_ptr_ == mut_type)
					{
						// tally this mutation as handled
						//mutation->gui_scratch_reference_count_ = 1;
						mutationsPlotted[registry_index] = true;
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
				for (int registry_index = 0; registry_index < registry_size; ++registry_index)
				{
					//if (mutation->gui_scratch_reference_count_ == 0)
					if (!mutationsPlotted[registry_index])
					{
						const Mutation *mutation = mut_block_ptr + registry[registry_index];
						
						if (mutation->chromosome_index_ == chromosome_index)
						{
							slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
							slim_position_t mutationPosition = mutation->position_;
							NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
							
							mutationTickRect.size.height = (int)ceil((mutationRefCount / totalHaplosomeCount) * interiorRect.size.height);
							RGBForSelectionCoeff(mutation->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
							[[NSColor colorWithCalibratedRed:colorRed green:colorGreen blue:colorBlue alpha:1.0] set];
							NSRectFill(mutationTickRect);
						}
					}
				}
			}
			else
			{
				// OK, we have a lot of mutations left to draw.  Here we will again use the radix sort trick, to keep track of only the tallest bar in each column
				MutationIndex *mutationBuffer = (MutationIndex *)calloc(displayPixelWidth, sizeof(MutationIndex));
				
				EIDOS_BZERO(heightBuffer, displayPixelWidth * sizeof(int16_t));
				
				// Find the tallest bar in each column
				for (int registry_index = 0; registry_index < registry_size; ++registry_index)
				{
					//if (mutation->gui_scratch_reference_count_ == 0)
					if (!mutationsPlotted[registry_index])
					{
						MutationIndex mutationBlockIndex = registry[registry_index];
						const Mutation *mutation = mut_block_ptr + mutationBlockIndex;
						
						if (mutation->chromosome_index_ == chromosome_index)
						{
							slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
							slim_position_t mutationPosition = mutation->position_;
							//NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
							//int xPos = (int)(mutationTickRect.origin.x - interiorRect.origin.x);
							int xPos = LEFT_OFFSET_OF_BASE(mutationPosition, interiorRect, displayedRange);
							int16_t height = (int16_t)ceil((mutationRefCount / totalHaplosomeCount) * interiorRect.size.height);
							
							if ((xPos >= 0) && (xPos < displayPixelWidth))
							{
								if (height > heightBuffer[xPos])
								{
									heightBuffer[xPos] = height;
									mutationBuffer[xPos] = mutationBlockIndex;
								}
							}
						}
					}
				}
				
				// Now plot the bars
				for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
				{
					int height = heightBuffer[binIndex];
					
					if (height)
					{
						NSRect mutationTickRect = NSMakeRect(interiorRect.origin.x + binIndex, interiorRect.origin.y, 1, height);
						const Mutation *mutation = mut_block_ptr + mutationBuffer[binIndex];
						
						RGBForSelectionCoeff(mutation->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
						[[NSColor colorWithCalibratedRed:colorRed green:colorGreen blue:colorBlue alpha:1.0] set];
						NSRectFill(mutationTickRect);
					}
				}
				
				free(mutationBuffer);
			}
		}
		
		free(heightBuffer);
		free(mutationsPlotted);
	}
}

- (void)drawRect:(NSRect)dirtyRect
{
	SLiMWindowController *controller = (SLiMWindowController *)[[self window] windowController];
	bool ready = ([self enabled] && ![controller invalidSimulation]);
	NSRect contentRect = [self contentRect];
	NSRect interiorRect = [self interiorRect];
	
	// if the simulation is at tick 0, it is not ready
	if (ready)
		if (controller->community->Tick() == 0)
			ready = NO;
	
	if (ready)
	{
		Species *displaySpecies = [controller focalDisplaySpecies];
		const std::vector<Chromosome *> &chromosomes = displaySpecies->Chromosomes();
		int chromosomeCount = (int)chromosomes.size();
		int64_t availableWidth = (int64_t)(contentRect.size.width - (chromosomeCount * 2) - (chromosomeCount - 1) * spaceBetweenChromosomes);
		int64_t totalLength = 0;
		
		for (Chromosome *chrom : chromosomes)
		{
			slim_position_t chromLength = (chrom->last_position_ + 1);
			
			totalLength += chromLength;
		}
		
		int64_t remainingLength = totalLength;
		int leftPosition = (int)contentRect.origin.x;
		
		for (Chromosome *chrom : chromosomes)
		{
			double scale = (double)availableWidth / remainingLength;
			slim_position_t chromLength = (chrom->last_position_ + 1);
			int width = (int)round(chromLength * scale);
			int paddedWidth = 2 + width;
			NSRect chromContentRect = NSMakeRect(leftPosition, contentRect.origin.y, paddedWidth, contentRect.size.height);
			NSRect chromInteriorRect = NSInsetRect(chromContentRect, 1, 1);
			
			// erase the content area itself
			[[NSColor colorWithCalibratedWhite:0.0 alpha:1.0] set];
			NSRectFill(chromInteriorRect);
			
			NSRange displayedRange = [self displayedRangeForChromosome:chrom];
			
			BOOL splitHeight = (shouldDrawRateMaps && shouldDrawGenomicElements);
			NSRect topInteriorRect = chromInteriorRect, bottomInteriorRect = chromInteriorRect;
			CGFloat halfHeight = ceil(chromInteriorRect.size.height / 2.0);
			CGFloat remainingHeight = chromInteriorRect.size.height - halfHeight;
			
			topInteriorRect.size.height = halfHeight;
			topInteriorRect.origin.y += remainingHeight;
			bottomInteriorRect.size.height = remainingHeight;
			
			// draw ticks at bottom of content rect
			if (![self overview] && !NSContainsRect(chromInteriorRect, dirtyRect))
				[self drawTicksInContentRect:chromContentRect withController:controller displayedRange:displayedRange];
			
			// draw recombination intervals in interior
			if (shouldDrawRateMaps)
				[self drawRateMapsInInteriorRect:(splitHeight ? topInteriorRect : chromInteriorRect) chromosome:chrom withController:controller displayedRange:displayedRange];
			
			// draw genomic elements in interior
			if (shouldDrawGenomicElements)
				[self drawGenomicElementsInInteriorRect:(splitHeight ? bottomInteriorRect : chromInteriorRect) chromosome:chrom withController:controller displayedRange:displayedRange];
			
			// figure out which mutation types we're displaying
			if (shouldDrawFixedSubstitutions || shouldDrawMutations)
				[self updateDisplayedMutationTypes];
			
			// draw fixed substitutions in interior
			if (shouldDrawFixedSubstitutions)
				[self drawFixedSubstitutionsInInteriorRect:chromInteriorRect chromosome:chrom withController:controller displayedRange:displayedRange];
			
			// draw mutations in interior
			if (shouldDrawMutations)
				[self drawMutationsInInteriorRect:chromInteriorRect chromosome:chrom withController:controller displayedRange:displayedRange];
			
			// frame near the end, so that any roundoff errors that caused overdrawing by a pixel get cleaned up
			[[NSColor colorWithCalibratedWhite:0.6 alpha:1.0] set];
			NSFrameRect(chromContentRect);
			
			leftPosition += (paddedWidth + spaceBetweenChromosomes);
			availableWidth -= width;
			remainingLength -= chromLength;
		}
	}
	else
	{
		// erase the content area itself
		[[NSColor colorWithCalibratedWhite:0.88 alpha:1.0] set];
		NSRectFill(interiorRect);
		
		// frame
		[[NSColor colorWithCalibratedWhite:0.6 alpha:1.0] set];
		NSFrameRect(contentRect);
	}
}

- (IBAction)filterMutations:(id)sender
{
	NSMenuItem *senderMenuItem = (NSMenuItem *)sender;
	slim_objectid_t muttype_id = (int)[senderMenuItem tag];
	
	if (muttype_id == -1)
	{
		display_muttypes_.clear();
	}
	else
	{
		auto present_iter = std::find(display_muttypes_.begin(), display_muttypes_.end(), muttype_id);
		
		if (present_iter == display_muttypes_.end())
		{
			// this mut-type is not being displayed, so add it to our display list
			display_muttypes_.emplace_back(muttype_id);
		}
		else
		{
			// this mut-type is being displayed, so remove it from our display list
			display_muttypes_.erase(present_iter);
		}
	}
	
	[self setNeedsDisplayAll];
}
	
- (IBAction)filterNonNeutral:(id)sender
{
	SLiMWindowController *controller = (SLiMWindowController *)[[self window] windowController];
	Species *displaySpecies = [controller focalDisplaySpecies];
	
	if (displaySpecies)
	{
		display_muttypes_.clear();
		
		std::map<slim_objectid_t,MutationType*> &muttypes = displaySpecies->mutation_types_;
		
		for (auto muttype_iter : muttypes)
		{
			MutationType *muttype = muttype_iter.second;
			slim_objectid_t muttype_id = muttype->mutation_type_id_;
			
			if (!muttype->IsPureNeutralDFE())
				display_muttypes_.emplace_back(muttype_id);
		}
		
		[self setNeedsDisplayAll];
	}
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
	SLiMWindowController *controller = (SLiMWindowController *)[[self window] windowController];
	
	if (![controller invalidSimulation] && ![[controller window] attachedSheet] && ![self overview] && [self enabled])
	{
		Species *displaySpecies = [controller focalDisplaySpecies];
		
		if (displaySpecies)
		{
			std::map<slim_objectid_t,MutationType*> &muttypes = displaySpecies->mutation_types_;
			
			if (muttypes.size() > 0)
			{
				NSMenu *menu = [[NSMenu alloc] initWithTitle:@"chromosome_menu"];
				NSMenuItem *menuItem;
				
				menuItem = [menu addItemWithTitle:@"Display All Mutations" action:@selector(filterMutations:) keyEquivalent:@""];
				[menuItem setTag:-1];
				[menuItem setTarget:self];
				
				if (display_muttypes_.size() == 0)
					[menuItem setState:NSOnState];
				
				// Make a sorted list of all mutation types we know – those that exist, and those that used to exist that we are displaying
				std::vector<slim_objectid_t> all_muttypes;
				
				for (auto muttype_iter : muttypes)
				{
					MutationType *muttype = muttype_iter.second;
					slim_objectid_t muttype_id = muttype->mutation_type_id_;
					
					all_muttypes.emplace_back(muttype_id);
				}
				
				all_muttypes.insert(all_muttypes.end(), display_muttypes_.begin(), display_muttypes_.end());
				
				// Avoid building a huge menu, which will hang the app
				if (all_muttypes.size() <= 500)
				{
					std::sort(all_muttypes.begin(), all_muttypes.end());
					all_muttypes.resize(std::distance(all_muttypes.begin(), std::unique(all_muttypes.begin(), all_muttypes.end())));
					
					// Then add menu items for each of those muttypes
					for (slim_objectid_t muttype_id : all_muttypes)
					{
						menuItem = [menu addItemWithTitle:[NSString stringWithFormat:@"Display m%d", (int)muttype_id] action:@selector(filterMutations:) keyEquivalent:@""];
						[menuItem setTag:muttype_id];
						[menuItem setTarget:self];
						
						if (std::find(display_muttypes_.begin(), display_muttypes_.end(), muttype_id) != display_muttypes_.end())
							[menuItem setState:NSOnState];
					}
				}
				
				[menu addItem:[NSMenuItem separatorItem]];
				
				menuItem = [menu addItemWithTitle:@"Select Non-Neutral MutationTypes" action:@selector(filterNonNeutral:) keyEquivalent:@""];
				[menuItem setTarget:self];
				
				return [menu autorelease];
			}
		}
	}
	
	return nil;
}

@end









































































