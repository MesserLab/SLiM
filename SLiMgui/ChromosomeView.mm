//
//  ChromosomeView.m
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
//  Copyright (c) 2015-2020 Philipp Messer.  All rights reserved.
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
#import "SLiMHaplotypeManager.h"


// We now use OpenGL to do some of our drawing, so we need these headers
#import <OpenGL/OpenGL.h>
#include <OpenGL/glu.h>
#include <GLKit/GLKMatrix4.h>

// OpenGL constants
static const int kMaxGLRects = 4000;				// 4000 rects
static const int kMaxVertices = kMaxGLRects * 4;	// 4 vertices each

// OpenGL macros
#define SLIM_GL_PREPARE()										\
	int displayListIndex = 0;									\
	float *vertices = glArrayVertices, *colors = glArrayColors;	\
																\
	glEnableClientState(GL_VERTEX_ARRAY);						\
	glVertexPointer(2, GL_FLOAT, 0, glArrayVertices);			\
	glEnableClientState(GL_COLOR_ARRAY);						\
	glColorPointer(4, GL_FLOAT, 0, glArrayColors);

#define SLIM_GL_DEFCOORDS(rect)									\
	float left = (float)rect.origin.x;							\
	float top = (float)rect.origin.y;							\
	float right = left + (float)rect.size.width;				\
	float bottom = top + (float)rect.size.height;

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


NSString *SLiMChromosomeSelectionChangedNotification = @"SLiMChromosomeSelectionChangedNotification";

static NSDictionary *tickAttrs = nil;
static const int numberOfTicksPlusOne = 4;
static const int tickLength = 5;
static const int heightForTicks = 16;
static const int selectionKnobSizeExtension = 2;	// a 5-pixel-width knob is 2: 2 + 1 + 2, an extension on each side plus the one pixel of the bar in the middle
static const int selectionKnobSize = selectionKnobSizeExtension + selectionKnobSizeExtension + 1;


@implementation ChromosomeView

@synthesize referenceChromosomeView, shouldDrawGenomicElements, shouldDrawFixedSubstitutions, shouldDrawMutations, shouldDrawRateMaps;

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

- (void)awakeFromNib
{
	display_muttypes_.clear();
	
	[self bind:@"enabled" toObject:[[self window] windowController] withKeyPath:@"invalidSimulation" options:@{NSValueTransformerNameBindingOption : NSNegateBooleanTransformerName}];
	
	if (_proxyGLView)
	{
		//NSLog(@"Setting up OpenGL buffers...");
		
		// Set up the vertex and color arrays
		if (!glArrayVertices)
			glArrayVertices = (float *)malloc(kMaxVertices * 2 * sizeof(float));		// 2 floats per vertex, kMaxVertices vertices
		
		if (!glArrayColors)
			glArrayColors = (float *)malloc(kMaxVertices * 4 * sizeof(float));		// 4 floats per color, kMaxVertices colors
	}
}

- (void)removeSelectionMarkers
{
	[startMarker close];
	[startMarker release];
	startMarker = nil;
	
	[endMarker close];
	[endMarker release];
	endMarker = nil;
}

- (void)dealloc
{
	[self setReferenceChromosomeView:nil];
	[self setProxyGLView:nil];
	
	[self unbind:@"enabled"];
	
	[self removeSelectionMarkers];
	
	if (glArrayVertices)
	{
		free(glArrayVertices);
		glArrayVertices = NULL;
	}
	
	if (glArrayColors)
	{
		free(glArrayColors);
		glArrayColors = NULL;
	}
	
	if (haplotype_previous_bincounts)
	{
		free(haplotype_previous_bincounts);
		haplotype_previous_bincounts = NULL;
	}
	
	[super dealloc];
}

- (void)setReferenceChromosomeView:(ChromosomeView *)refView
{
	if (referenceChromosomeView != refView)
	{
		[refView retain];
		[referenceChromosomeView release];
		referenceChromosomeView = refView;
		
		[[NSNotificationCenter defaultCenter] removeObserver:self];
		
		if (referenceChromosomeView)
			[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(referenceChromosomeViewSelectionChanged:) name:SLiMChromosomeSelectionChangedNotification object:referenceChromosomeView];
	}
}

- (void)referenceChromosomeViewSelectionChanged:(NSNotification *)note
{
	[self setNeedsDisplayAll];
}

- (NSRange)selectedRange
{
	if (hasSelection)
	{
		return NSMakeRange(selectionFirstBase, selectionLastBase - selectionFirstBase + 1);	// number of bases encompassed; a selection from x to x encompasses 1 base
	}
	else
	{
		SLiMWindowController *controller = (SLiMWindowController *)[[self window] windowController];
		Chromosome &chromosome = controller->sim->chromosome_;
		slim_position_t chromosomeLastPosition = chromosome.last_position_;
		
		return NSMakeRange(0, chromosomeLastPosition + 1);	// chromosomeLastPosition + 1 bases are encompassed
	}
}

- (void)setSelectedRange:(NSRange)selectionRange
{
	if ([self isSelectable] && (selectionRange.length >= 1))
	{
		selectionFirstBase = (slim_position_t)selectionRange.location;
		selectionLastBase = (slim_position_t)(selectionRange.location + selectionRange.length) - 1;
		hasSelection = YES;
		
		// Save the selection for restoring across recycles, etc.
		savedSelectionFirstBase = selectionFirstBase;
		savedSelectionLastBase = selectionLastBase;
		savedHasSelection = hasSelection;
	}
	else if (hasSelection)
	{
		hasSelection = NO;
		
		// Save the selection for restoring across recycles, etc.
		savedHasSelection = hasSelection;
	}
	else
	{
		// Save the selection for restoring across recycles, etc.
		savedHasSelection = NO;
		
		return;
	}
	
	// Our selection changed, so update and post a change notification
	[self setNeedsDisplayAll];
	
	[[NSNotificationCenter defaultCenter] postNotificationName:SLiMChromosomeSelectionChangedNotification object:self];
}

- (void)restoreLastSelection
{
	if ([self isSelectable] && savedHasSelection)
	{
		selectionFirstBase = savedSelectionFirstBase;
		selectionLastBase = savedSelectionLastBase;
		hasSelection = savedHasSelection;
	}
	else if (hasSelection)
	{
		hasSelection = NO;
	}
	else
	{
		// We want to always post the notification, to make sure updating happens correctly;
		// this ensures that correct ticks marks get drawn after a recycle, etc.
		//return;
	}
	
	// Our selection changed, so update and post a change notification
	[self setNeedsDisplayAll];
	
	[[NSNotificationCenter defaultCenter] postNotificationName:SLiMChromosomeSelectionChangedNotification object:self];
}

- (NSRange)displayedRange
{
	ChromosomeView *reference = [self referenceChromosomeView];
	
	if (reference)
		return [reference selectedRange];
	else
	{
		SLiMWindowController *controller = (SLiMWindowController *)[[self window] windowController];
		Chromosome &chromosome = controller->sim->chromosome_;
		slim_position_t chromosomeLastPosition = chromosome.last_position_;
		
		return NSMakeRange(0, chromosomeLastPosition + 1);	// chromosomeLastPosition + 1 bases are encompassed
	}
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
	return NSMakeRect(bounds.origin.x + 2, bounds.origin.y + heightForTicks, bounds.size.width - 4, bounds.size.height - heightForTicks);
}

- (NSRect)interiorRect
{
	return NSInsetRect([self contentRect], 1, 1);
}

- (void)setNeedsDisplayAll
{
	[self setNeedsDisplay:YES];
	if (_proxyGLView)
		[_proxyGLView setNeedsDisplay:YES];
}

- (void)setNeedsDisplayInInterior
{
	[self setNeedsDisplayInRect:[self interiorRect]];
    if (_proxyGLView)
        [_proxyGLView setNeedsDisplay:YES];
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

#pragma mark Drawing ticks

- (void)drawTicksInContentRect:(NSRect)contentRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	NSRect interiorRect = [self interiorRect];
	int64_t lastTickIndex = numberOfTicksPlusOne;
	
	// Display fewer ticks when we are displaying a very small number of positions
	lastTickIndex = MIN(lastTickIndex, ((int64_t)displayedRange.length + 1) / 3);
	
	double tickIndexDivisor = ((lastTickIndex == 0) ? 1.0 : (double)lastTickIndex);		// avoid a divide by zero when we are displaying a single site
	
	for (int tickIndex = 0; tickIndex <= lastTickIndex; ++tickIndex)
	{
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
		
		// BCH 15 May 2018: display in scientific notation for positions at or above 1e10, as it gets a bit ridiculous...
		NSString *tickLabel;
		
		if (tickBase >= 1e10)
			tickLabel = [NSString stringWithFormat:@"%.6e", (double)tickBase];
		else
			tickLabel = [NSString stringWithFormat:@"%lld", (int64_t)tickBase];
		
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

#pragma mark Drawing genomic elements

- (void)drawGenomicElementsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	Chromosome &chromosome = controller->sim->chromosome_;
	CGFloat previousIntervalLeftEdge = -10000;
	
	for (GenomicElement *genomicElement : chromosome.GenomicElements())
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

- (void)glDrawGenomicElementsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	Chromosome &chromosome = controller->sim->chromosome_;
	CGFloat previousIntervalLeftEdge = -10000;
	
	SLIM_GL_PREPARE();
	
	for (GenomicElement *genomicElement : chromosome.GenomicElements())
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
			
			SLIM_GL_DEFCOORDS(elementRect);
			SLIM_GL_PUSHRECT();
			SLIM_GL_PUSHRECT_COLORS();
			SLIM_GL_CHECKBUFFERS();
			
			// if this interval is just one pixel wide, we want to try to make it visible, by avoiding overdrawing it; so we remember its location
			if (widthOne)
				previousIntervalLeftEdge = elementRect.origin.x;
			else
				previousIntervalLeftEdge = -10000;
		}
	}
	
	SLIM_GL_FINISH();
}

#pragma mark Drawing rate map intervals

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

- (void)_glDrawRateMapIntervalsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange ends:(std::vector<slim_position_t> &)ends rates:(std::vector<double> &)rates hue:(double)hue
{
	int recombinationIntervalCount = (int)ends.size();
	slim_position_t intervalStartPosition = 0;
	CGFloat previousIntervalLeftEdge = -10000;
	
	SLIM_GL_PREPARE();
	
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
			
			float colorRed = (float)r, colorGreen = (float)g, colorBlue = (float)b, colorAlpha = (float)a;
			
			SLIM_GL_DEFCOORDS(intervalRect);
			SLIM_GL_PUSHRECT();
			SLIM_GL_PUSHRECT_COLORS();
			SLIM_GL_CHECKBUFFERS();
			
			// if this interval is just one pixel wide, we want to try to make it visible, by avoiding overdrawing it; so we remember its location
			if (widthOne)
				previousIntervalLeftEdge = intervalRect.origin.x;
			else
				previousIntervalLeftEdge = -10000;
		}
		
		// the next interval starts at the next base after this one ended
		intervalStartPosition = intervalEndPosition + 1;
	}
	
	SLIM_GL_FINISH();
}

#pragma mark Drawing recombination intervals

- (void)drawRecombinationIntervalsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	Chromosome &chromosome = controller->sim->chromosome_;
	
	if (chromosome.single_recombination_map_)
	{
		[self _drawRateMapIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange ends:chromosome.recombination_end_positions_H_ rates:chromosome.recombination_rates_H_];
	}
	else
	{
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		[self _drawRateMapIntervalsInInteriorRect:topInteriorRect withController:controller displayedRange:displayedRange ends:chromosome.recombination_end_positions_M_ rates:chromosome.recombination_rates_M_];
		[self _drawRateMapIntervalsInInteriorRect:bottomInteriorRect withController:controller displayedRange:displayedRange ends:chromosome.recombination_end_positions_F_ rates:chromosome.recombination_rates_F_];
	}
}

- (void)glDrawRecombinationIntervalsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	Chromosome &chromosome = controller->sim->chromosome_;
	
	if (chromosome.single_recombination_map_)
	{
		[self _glDrawRateMapIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange ends:chromosome.recombination_end_positions_H_ rates:chromosome.recombination_rates_H_ hue:0.65];
	}
	else
	{
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		[self _glDrawRateMapIntervalsInInteriorRect:topInteriorRect withController:controller displayedRange:displayedRange ends:chromosome.recombination_end_positions_M_ rates:chromosome.recombination_rates_M_ hue:0.65];
		[self _glDrawRateMapIntervalsInInteriorRect:bottomInteriorRect withController:controller displayedRange:displayedRange ends:chromosome.recombination_end_positions_F_ rates:chromosome.recombination_rates_F_ hue:0.65];
	}
}

#pragma mark Drawing mutation rate intervals

- (void)drawMutationIntervalsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	Chromosome &chromosome = controller->sim->chromosome_;
	
	if (chromosome.single_mutation_map_)
	{
		[self _drawRateMapIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange ends:chromosome.mutation_end_positions_H_ rates:chromosome.mutation_rates_H_];
	}
	else
	{
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		[self _drawRateMapIntervalsInInteriorRect:topInteriorRect withController:controller displayedRange:displayedRange ends:chromosome.mutation_end_positions_M_ rates:chromosome.mutation_rates_M_];
		[self _drawRateMapIntervalsInInteriorRect:bottomInteriorRect withController:controller displayedRange:displayedRange ends:chromosome.mutation_end_positions_F_ rates:chromosome.mutation_rates_F_];
	}
}

- (void)glDrawMutationIntervalsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	Chromosome &chromosome = controller->sim->chromosome_;
	
	if (chromosome.single_mutation_map_)
	{
		[self _glDrawRateMapIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange ends:chromosome.mutation_end_positions_H_ rates:chromosome.mutation_rates_H_ hue:0.75];
	}
	else
	{
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		[self _glDrawRateMapIntervalsInInteriorRect:topInteriorRect withController:controller displayedRange:displayedRange ends:chromosome.mutation_end_positions_M_ rates:chromosome.mutation_rates_M_ hue:0.75];
		[self _glDrawRateMapIntervalsInInteriorRect:bottomInteriorRect withController:controller displayedRange:displayedRange ends:chromosome.mutation_end_positions_F_ rates:chromosome.mutation_rates_F_ hue:0.75];
	}
}

#pragma mark Drawing rate maps

- (void)drawRateMapsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	Chromosome &chromosome = controller->sim->chromosome_;
	BOOL recombinationWorthShowing = NO;
	BOOL mutationWorthShowing = NO;
	
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
		[self drawRecombinationIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
	}
	else if (mutationWorthShowing && !recombinationWorthShowing)
	{
		[self drawMutationIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
	}
	else	// mutationWorthShowing && recombinationWorthShowing
	{
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		[self drawRecombinationIntervalsInInteriorRect:topInteriorRect withController:controller displayedRange:displayedRange];
		[self drawMutationIntervalsInInteriorRect:bottomInteriorRect withController:controller displayedRange:displayedRange];
	}
}

- (void)glDrawRateMapsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	Chromosome &chromosome = controller->sim->chromosome_;
	BOOL recombinationWorthShowing = NO;
	BOOL mutationWorthShowing = NO;
	
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
		[self glDrawRecombinationIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
	}
	else if (mutationWorthShowing && !recombinationWorthShowing)
	{
		[self glDrawMutationIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
	}
	else	// mutationWorthShowing && recombinationWorthShowing
	{
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		[self glDrawRecombinationIntervalsInInteriorRect:topInteriorRect withController:controller displayedRange:displayedRange];
		[self glDrawMutationIntervalsInInteriorRect:bottomInteriorRect withController:controller displayedRange:displayedRange];
	}
}


#pragma mark Drawing substitutions

- (void)drawFixedSubstitutionsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	double scalingFactor = controller->selectionColorScale;
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	Chromosome &chromosome = sim->chromosome_;
	bool chromosomeHasDefaultColor = !chromosome.color_sub_.empty();
	
	float colorRed = 0.2f, colorGreen = 0.2f, colorBlue = 1.0f;
	
	if (chromosomeHasDefaultColor)
	{
		colorRed = chromosome.color_sub_red_;
		colorGreen = chromosome.color_sub_green_;
		colorBlue = chromosome.color_sub_blue_;
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

- (void)glDrawFixedSubstitutionsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	double scalingFactor = controller->selectionColorScale;
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	Chromosome &chromosome = sim->chromosome_;
	bool chromosomeHasDefaultColor = !chromosome.color_sub_.empty();
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	// Set up to draw rects
	float colorRed = 0.2f, colorGreen = 0.2f, colorBlue = 1.0f, colorAlpha = 1.0;
	
	if (chromosomeHasDefaultColor)
	{
		colorRed = chromosome.color_sub_red_;
		colorGreen = chromosome.color_sub_green_;
		colorBlue = chromosome.color_sub_blue_;
	}
	
	SLIM_GL_PREPARE();
	
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
				
				SLIM_GL_DEFCOORDS(substitutionTickRect);
				SLIM_GL_PUSHRECT();
				SLIM_GL_PUSHRECT_COLORS();
				SLIM_GL_CHECKBUFFERS();
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
					
					SLIM_GL_DEFCOORDS(mutationTickRect);
					SLIM_GL_PUSHRECT();
					SLIM_GL_PUSHRECT_COLORS();
					SLIM_GL_CHECKBUFFERS();
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
					SLIM_GL_DEFCOORDS(mutationTickRect);
					SLIM_GL_PUSHRECT();
					SLIM_GL_PUSHRECT_COLORS();
					SLIM_GL_CHECKBUFFERS();
				}
			}
		}
		
		free(subBuffer);
	}
	
	SLIM_GL_FINISH();
}

#pragma mark Drawing mutations

- (void)updateDisplayedMutationTypes
{
	// We use a flag in MutationType to indicate whether we're drawing that type or not; we update those flags here,
	// before every drawing of mutations, from the vector of mutation type IDs that we keep internally
	SLiMWindowController *controller = (SLiMWindowController *)[[self window] windowController];
	
	if (controller)
	{
		SLiMSim *sim = controller->sim;
		
		if (sim)
		{
			std::map<slim_objectid_t,MutationType*> &muttypes = sim->mutation_types_;
			
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

- (void)drawMutationsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	double scalingFactor = controller->selectionColorScale;
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	double totalGenomeCount = pop.gui_total_genome_count_;				// this includes only genomes in the selected subpopulations
	int registry_size;
	const MutationIndex *registry = pop.MutationRegistry(&registry_size);
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	if ((registry_size < 1000) || (displayedRange.length < interiorRect.size.width))
	{
		// This is the simple version of the display code, avoiding the memory allocations and such
		for (int registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			const Mutation *mutation = mut_block_ptr + registry[registry_index];
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
				
				mutationTickRect.size.height = (int)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
				NSRectFill(mutationTickRect);
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
		for (auto mutationTypeIter : controller->sim->mutation_types_)
		{
			MutationType *mut_type = mutationTypeIter.second;
			
			if (mut_type->mutation_type_displayed_)
			{
				bool mut_type_fixed_color = !mut_type->color_.empty();
				
				// We optimize fixed-DFE mutation types only, and those using a fixed color set by the user
				if ((mut_type->dfe_type_ == DFEType::kFixed) || mut_type_fixed_color)
				{
					slim_selcoeff_t mut_type_selcoeff = (mut_type_fixed_color ? 0.0 : (slim_selcoeff_t)mut_type->dfe_parameters_[0]);
					
					EIDOS_BZERO(heightBuffer, displayPixelWidth * sizeof(int16_t));
					
					// Scan through the mutation list for mutations of this type with the right selcoeff
					for (int registry_index = 0; registry_index < registry_size; ++registry_index)
					{
						const Mutation *mutation = mut_block_ptr + registry[registry_index];
						
						if ((mutation->mutation_type_ptr_ == mut_type) && (mut_type_fixed_color || (mutation->selection_coeff_ == mut_type_selcoeff)))
						{
							slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// includes only refs from the selected subpopulations
							slim_position_t mutationPosition = mutation->position_;
							//NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
							//int xPos = (int)(mutationTickRect.origin.x - interiorRect.origin.x);
							int xPos = LEFT_OFFSET_OF_BASE(mutationPosition, interiorRect, displayedRange);
							int16_t height = (int16_t)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
							
							if ((xPos >= 0) && (xPos < displayPixelWidth))
								if (height > heightBuffer[xPos])
									heightBuffer[xPos] = height;
							
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
						slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
						slim_position_t mutationPosition = mutation->position_;
						NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
						
						mutationTickRect.size.height = (int)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
						RGBForSelectionCoeff(mutation->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
						[[NSColor colorWithCalibratedRed:colorRed green:colorGreen blue:colorBlue alpha:1.0] set];
						NSRectFill(mutationTickRect);
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
						slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
						slim_position_t mutationPosition = mutation->position_;
						//NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
						//int xPos = (int)(mutationTickRect.origin.x - interiorRect.origin.x);
						int xPos = LEFT_OFFSET_OF_BASE(mutationPosition, interiorRect, displayedRange);
						int16_t height = (int16_t)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
						
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

- (void)glDrawMutationsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	double scalingFactor = controller->selectionColorScale;
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	double totalGenomeCount = pop.gui_total_genome_count_;				// this includes only genomes in the selected subpopulations
	int registry_size;
	const MutationIndex *registry = pop.MutationRegistry(&registry_size);
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	// Set up to draw rects
	float colorRed = 0.0f, colorGreen = 0.0f, colorBlue = 0.0f, colorAlpha = 1.0;
	
	SLIM_GL_PREPARE();
	
	if ((registry_size < 1000) || (displayedRange.length < interiorRect.size.width))
	{
		// This is the simple version of the display code, avoiding the memory allocations and such
		for (int registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			const Mutation *mutation = mut_block_ptr + registry[registry_index];
			const MutationType *mutType = mutation->mutation_type_ptr_;
			
			if (mutType->mutation_type_displayed_)
			{
				slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
				slim_position_t mutationPosition = mutation->position_;
				NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
				
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
				
				mutationTickRect.size.height = (int)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
				SLIM_GL_DEFCOORDS(mutationTickRect);
				SLIM_GL_PUSHRECT();
				SLIM_GL_PUSHRECT_COLORS();
				SLIM_GL_CHECKBUFFERS();
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
		int16_t *heightBuffer = (int16_t *)malloc(displayPixelWidth * sizeof(int16_t));
		bool *mutationsPlotted = (bool *)calloc(registry_size, sizeof(bool));	// faster than using gui_scratch_reference_count_ because of cache locality
		int64_t remainingMutations = registry_size;
		
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
						
						EIDOS_BZERO(heightBuffer, displayPixelWidth * sizeof(int16_t));
						
						// Scan through the mutation list for mutations of this type with the right selcoeff
						for (int registry_index = 0; registry_index < registry_size; ++registry_index)
						{
							const Mutation *mutation = mut_block_ptr + registry[registry_index];
							
							if ((mutation->mutation_type_ptr_ == mut_type) && (mut_type_fixed_color || (mutation->selection_coeff_ == mut_type_selcoeff)))
							{
								slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// includes only refs from the selected subpopulations
								slim_position_t mutationPosition = mutation->position_;
								//NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
								//int xPos = (int)(mutationTickRect.origin.x - interiorRect.origin.x);
								int xPos = LEFT_OFFSET_OF_BASE(mutationPosition, interiorRect, displayedRange);
								int16_t height = (int16_t)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
								
								if ((xPos >= 0) && (xPos < displayPixelWidth))
									if (height > heightBuffer[xPos])
										heightBuffer[xPos] = height;
								
								// tally this mutation as handled
								//mutation->gui_scratch_reference_count_ = 1;
								mutationsPlotted[registry_index] = true;
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
						
						for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
						{
							int height = heightBuffer[binIndex];
							
							if (height)
							{
								NSRect mutationTickRect = NSMakeRect(interiorRect.origin.x + binIndex, interiorRect.origin.y, 1, height);
								
								SLIM_GL_DEFCOORDS(mutationTickRect);
								SLIM_GL_PUSHRECT();
								SLIM_GL_PUSHRECT_COLORS();
								SLIM_GL_CHECKBUFFERS();
							}
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
						slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
						slim_position_t mutationPosition = mutation->position_;
						NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
						
						mutationTickRect.size.height = (int)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
						RGBForSelectionCoeff(mutation->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
						
						SLIM_GL_DEFCOORDS(mutationTickRect);
						SLIM_GL_PUSHRECT();
						SLIM_GL_PUSHRECT_COLORS();
						SLIM_GL_CHECKBUFFERS();
					}
				}
			}
			else
			{
				// OK, we have a lot of mutations left to draw.  Here we will again use the radix sort trick, to keep track of only the tallest bar in each column
				MutationIndex *mutationBuffer = (MutationIndex *)calloc(displayPixelWidth,  sizeof(MutationIndex));
				
				EIDOS_BZERO(heightBuffer, displayPixelWidth * sizeof(int16_t));
				
				// Find the tallest bar in each column
				for (int registry_index = 0; registry_index < registry_size; ++registry_index)
				{
					//if (mutation->gui_scratch_reference_count_ == 0)
					if (!mutationsPlotted[registry_index])
					{
						MutationIndex mutationBlockIndex = registry[registry_index];
						const Mutation *mutation = mut_block_ptr + mutationBlockIndex;
						slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
						slim_position_t mutationPosition = mutation->position_;
						//NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
						//int xPos = (int)(mutationTickRect.origin.x - interiorRect.origin.x);
						int xPos = LEFT_OFFSET_OF_BASE(mutationPosition, interiorRect, displayedRange);
						int16_t height = (int16_t)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
						
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
				
				// Now plot the bars
				for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
				{
					int height = heightBuffer[binIndex];
					
					if (height)
					{
						NSRect mutationTickRect = NSMakeRect(interiorRect.origin.x + binIndex, interiorRect.origin.y, 1, height);
						const Mutation *mutation = mut_block_ptr + mutationBuffer[binIndex];
						
						RGBForSelectionCoeff(mutation->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
						
						SLIM_GL_DEFCOORDS(mutationTickRect);
						SLIM_GL_PUSHRECT();
						SLIM_GL_PUSHRECT_COLORS();
						SLIM_GL_CHECKBUFFERS();
					}
				}
				
				free(mutationBuffer);
			}
		}
		
		free(heightBuffer);
		free(mutationsPlotted);
	}
	
	SLIM_GL_FINISH();
}

#pragma mark Other drawing

- (void)overlaySelectionInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	if (hasSelection)
	{
		// darken the interior of the selection slightly
		NSRect selectionRect = [self rectEncompassingBase:selectionFirstBase toBase:selectionLastBase interiorRect:interiorRect displayedRange:displayedRange];
		
		[[NSColor colorWithCalibratedWhite:0.0 alpha:0.30] set];
		NSRectFillUsingOperation(selectionRect, NSCompositeSourceOver);
		
		// draw a bar at the start and end of the selection
		NSRect selectionStartBar1 = NSMakeRect(selectionRect.origin.x - 1, interiorRect.origin.y, 1, interiorRect.size.height);
		NSRect selectionStartBar2 = NSMakeRect(selectionRect.origin.x, interiorRect.origin.y - 5, 1, interiorRect.size.height + 5);
		//NSRect selectionStartBar3 = NSMakeRect(selectionRect.origin.x + 1, interiorRect.origin.y, 1, interiorRect.size.height);
		//NSRect selectionEndBar1 = NSMakeRect(selectionRect.origin.x + selectionRect.size.width - 2, interiorRect.origin.y, 1, interiorRect.size.height);
		NSRect selectionEndBar2 = NSMakeRect(selectionRect.origin.x + selectionRect.size.width - 1, interiorRect.origin.y - 5, 1, interiorRect.size.height + 5);
		NSRect selectionEndBar3 = NSMakeRect(selectionRect.origin.x + selectionRect.size.width, interiorRect.origin.y, 1, interiorRect.size.height);
		
		[[NSColor colorWithCalibratedWhite:1.0 alpha:0.15] set];
		NSRectFillUsingOperation(selectionStartBar1, NSCompositeSourceOver);
		//NSRectFillUsingOperation(selectionEndBar1, NSCompositeSourceOver);
		
		[[NSColor blackColor] set];
		NSRectFill(selectionStartBar2);
		NSRectFill(selectionEndBar2);
		
		[[NSColor colorWithCalibratedWhite:0.0 alpha:0.30] set];
		//NSRectFillUsingOperation(selectionStartBar3, NSCompositeSourceOver);
		NSRectFillUsingOperation(selectionEndBar3, NSCompositeSourceOver);
		
		// draw a ball at the end of each bar
		NSRect selectionStartBall = NSMakeRect(selectionRect.origin.x - selectionKnobSizeExtension, interiorRect.origin.y - (selectionKnobSize + 2), selectionKnobSize, selectionKnobSize);
		NSRect selectionEndBall = NSMakeRect(selectionRect.origin.x + selectionRect.size.width - (selectionKnobSizeExtension + 1), interiorRect.origin.y - (selectionKnobSize + 2), selectionKnobSize, selectionKnobSize);
		
		[[NSColor blackColor] set];	// outline
		[[NSBezierPath bezierPathWithOvalInRect:selectionStartBall] fill];
		[[NSBezierPath bezierPathWithOvalInRect:selectionEndBall] fill];
		
		[[NSColor colorWithCalibratedWhite:0.3 alpha:1.0] set];	// interior
		[[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(selectionStartBall, 1.0, 1.0)] fill];
		[[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(selectionEndBall, 1.0, 1.0)] fill];
		
		[[NSColor colorWithCalibratedWhite:1.0 alpha:0.5] set];	// highlight
		[[NSBezierPath bezierPathWithOvalInRect:NSOffsetRect(NSInsetRect(selectionStartBall, 1.5, 1.5), -1.0, 1.0)] fill];
		[[NSBezierPath bezierPathWithOvalInRect:NSOffsetRect(NSInsetRect(selectionEndBall, 1.5, 1.5), -1.0, 1.0)] fill];
	}
}

- (void)drawRect:(NSRect)dirtyRect
{
	SLiMWindowController *controller = (SLiMWindowController *)[[self window] windowController];
	bool ready = ([self enabled] && ![controller invalidSimulation]);
	NSRect contentRect = [self contentRect];
	NSRect interiorRect = [self interiorRect];
	
	// if the simulation is at generation 0, it is not ready
	if (ready)
		if (controller->sim->generation_ == 0)
			ready = NO;
	
	if (ready)
	{
		// erase the content area itself
		[[NSColor colorWithCalibratedWhite:0.0 alpha:1.0] set];
		NSRectFill(interiorRect);
		
		NSRange displayedRange = [self displayedRange];
		
		BOOL splitHeight = (shouldDrawRateMaps && shouldDrawGenomicElements);
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		// draw ticks at bottom of content rect
		if (!NSContainsRect(interiorRect, dirtyRect))
			[self drawTicksInContentRect:contentRect withController:controller displayedRange:displayedRange];
		
		if (!_proxyGLView)
		{
			// If we have a proxy NSOpenGLView set up, then it does all of our interior drawing.  It will call glDrawRect: on us
			// for us to draw on its behalf.  That method, below, does all of the tasks here, but with OpenGL calls instead.
			
			// draw recombination intervals in interior
			if (shouldDrawRateMaps)
				[self drawRateMapsInInteriorRect:(splitHeight ? topInteriorRect : interiorRect) withController:controller displayedRange:displayedRange];
			
			// draw genomic elements in interior
			if (shouldDrawGenomicElements)
				[self drawGenomicElementsInInteriorRect:(splitHeight ? bottomInteriorRect : interiorRect) withController:controller displayedRange:displayedRange];
			
			// figure out which mutation types we're displaying
			if (shouldDrawFixedSubstitutions || shouldDrawMutations)
				[self updateDisplayedMutationTypes];
			
			// draw fixed substitutions in interior
			if (shouldDrawFixedSubstitutions)
				[self drawFixedSubstitutionsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
			
			// draw mutations in interior
			if (shouldDrawMutations)
				[self drawMutationsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
		}
		
		// frame near the end, so that any roundoff errors that caused overdrawing by a pixel get cleaned up
		[[NSColor colorWithCalibratedWhite:0.6 alpha:1.0] set];
		NSFrameRect(contentRect);
		
		// overlay the selection last, since it bridges over the frame
		if (hasSelection)
			[self overlaySelectionInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
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

- (void)glDrawRect:(NSRect)dirtyRect
{
	SLiMWindowController *controller = (SLiMWindowController *)[[self window] windowController];
	bool ready = ([self enabled] && ![controller invalidSimulation]);
	NSRect interiorRect = [self interiorRect];
	
	interiorRect.origin = NSZeroPoint;	// We're drawing in the OpenGLView's coordinates, which have an origin of zero for the interior rect
	
	// if the simulation is at generation 0, it is not ready
	if (ready)
		if (controller->sim->generation_ == 0)
			ready = NO;
	
	if (ready)
	{
		// erase the content area itself
		glColor3f(0.0f, 0.0f, 0.0f);
		glRecti(0, 0, (int)interiorRect.size.width, (int)interiorRect.size.height);
		
		NSRange displayedRange = [self displayedRange];
		
		BOOL splitHeight = (shouldDrawRateMaps && shouldDrawGenomicElements);
		NSRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		CGFloat halfHeight = ceil(interiorRect.size.height / 2.0);
		CGFloat remainingHeight = interiorRect.size.height - halfHeight;
		
		topInteriorRect.size.height = halfHeight;
		topInteriorRect.origin.y += remainingHeight;
		bottomInteriorRect.size.height = remainingHeight;
		
		// draw recombination intervals in interior
		if (shouldDrawRateMaps)
			[self glDrawRateMapsInInteriorRect:(splitHeight ? topInteriorRect : interiorRect) withController:controller displayedRange:displayedRange];
		
		// draw genomic elements in interior
		if (shouldDrawGenomicElements)
			[self glDrawGenomicElementsInInteriorRect:(splitHeight ? bottomInteriorRect : interiorRect) withController:controller displayedRange:displayedRange];
		
		// figure out which mutation types we're displaying
		if (shouldDrawFixedSubstitutions || shouldDrawMutations)
			[self updateDisplayedMutationTypes];
		
		// draw fixed substitutions in interior
		if (shouldDrawFixedSubstitutions)
			[self glDrawFixedSubstitutionsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
		
		// draw mutations in interior
		if (shouldDrawMutations)
		{
			if (display_haplotypes_)
			{
				// display mutations as a haplotype plot, courtesy of SLiMHaplotypeManager; we use kSLiMHaplotypeClusterNearestNeighbor and
				// kSLiMHaplotypeClusterNoOptimization because they're fast, and NN might also provide a bit more run-to-run continuity
				int interiorHeight = (int)ceil(interiorRect.size.height);	// one sample per available pixel line, for simplicity and speed; 47, in the current UI layout
				SLiMHaplotypeManager *haplotypeManager = [[SLiMHaplotypeManager alloc] initWithClusteringMethod:kSLiMHaplotypeClusterNearestNeighbor optimizationMethod:kSLiMHaplotypeClusterNoOptimization sourceController:controller sampleSize:interiorHeight clusterInBackground:NO];
				
				[haplotypeManager glDrawHaplotypesInRect:interiorRect displayBlackAndWhite:NO showSubpopStrips:NO eraseBackground:NO previousFirstBincounts:&haplotype_previous_bincounts];
				
				// it's a little bit odd to throw away haplotypeManager here; if the user drag-resizes the window, we do a new display each
				// time, with a new sample, and so the haplotype display changes with every pixel resize change; we could keep this...?
				[haplotypeManager release];
			}
			else
			{
				// display mutations as a frequency plot; this is the standard display mode
				[self glDrawMutationsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
			}
		}
		
		// overlay the selection last, since it bridges over the frame
		if (hasSelection)
			NSLog(@"Selection set on a ChromosomeView that is drawing using OpenGL!");
	}
	else
	{
		// erase the content area itself
		glColor3f(0.88f, 0.88f, 0.88f);
		glRecti(0, 0, (int)interiorRect.size.width, (int)interiorRect.size.height);
	}
}

#pragma mark Mouse tracking

- (void)mouseDown:(NSEvent *)theEvent
{
	SLiMWindowController *controller = (SLiMWindowController *)[[self window] windowController];
	bool ready = ([self isSelectable] && [self enabled] && ![controller invalidSimulation]);
	
	// if the simulation is at generation 0, it is not ready
	if (ready)
		if (controller->sim->generation_ == 0)
			ready = NO;
	
	if (ready)
	{
		NSRect contentRect = [self contentRect];
		NSRect interiorRect = [self interiorRect];
		NSRange displayedRange = [self displayedRange];
		NSPoint curPoint = [self convertPoint:[theEvent locationInWindow] fromView:nil];
		
		// We deal with rounded x-coordinates to make adjusting the click point easier
		curPoint.x = (int)round(curPoint.x);
		
		// Option-clicks just set the selection to the clicked genomic element, no questions asked
		if ([theEvent modifierFlags] & NSAlternateKeyMask)
		{
			if (NSPointInRect(curPoint, contentRect))
			{
				slim_position_t clickedBase = [self baseForPosition:curPoint.x interiorRect:interiorRect displayedRange:displayedRange];
				NSRange selectionRange = NSMakeRange(0, 0);
				Chromosome &chromosome = controller->sim->chromosome_;
				
				for (GenomicElement *genomicElement : chromosome.GenomicElements())
				{
					slim_position_t startPosition = genomicElement->start_position_;
					slim_position_t endPosition = genomicElement->end_position_;
					
					if ((clickedBase >= startPosition) && (clickedBase <= endPosition))
						selectionRange = NSMakeRange(startPosition, endPosition - startPosition + 1);
				}
				
				[self setSelectedRange:selectionRange];
				return;
			}
		}
		
		// first check for a hit in one of our selection handles
		if (hasSelection)
		{
			NSRect selectionRect = [self rectEncompassingBase:selectionFirstBase toBase:selectionLastBase interiorRect:interiorRect displayedRange:displayedRange];
			double leftEdge = selectionRect.origin.x;
			double rightEdge = selectionRect.origin.x + selectionRect.size.width - 1;	// -1 to be on the left edge of the right-edge pixel strip
			NSRect leftSelectionBar = NSMakeRect(leftEdge - 2, selectionRect.origin.y - 1, 5, selectionRect.size.height + 2);
			NSRect leftSelectionKnob = NSMakeRect(leftEdge - (selectionKnobSizeExtension + 1), selectionRect.origin.y - (selectionKnobSize + 2 + 1), (selectionKnobSizeExtension + 1) * 2 + 1, selectionKnobSize + 2);
			NSRect rightSelectionBar = NSMakeRect(rightEdge - 2, selectionRect.origin.y - 1, 5, selectionRect.size.height + 2);
			NSRect rightSelectionKnob = NSMakeRect(rightEdge - (selectionKnobSizeExtension + 1), selectionRect.origin.y - (selectionKnobSize + 2 + 1), (selectionKnobSizeExtension + 1) * 2 + 1, selectionKnobSize + 2);
			
			if (NSPointInRect(curPoint, leftSelectionBar) || NSPointInRect(curPoint, leftSelectionKnob))
			{
				isTracking = YES;
				trackingXAdjust = (int)(curPoint.x - leftEdge) - 1;		// I'm not sure why the -1 is needed, but it is...
				trackingStartBase = selectionLastBase;	// we're dragging the left knob, so the right knob is the tracking anchor
				trackingLastBase = [self baseForPosition:(curPoint.x - trackingXAdjust) interiorRect:interiorRect displayedRange:displayedRange];	// instead of selectionFirstBase, so the selection does not change at all if the mouse does not move
				
				[self mouseDragged:theEvent];	// the click may not be aligned exactly on the center of the bar, so clicking might shift it a bit; do that now
				return;
			}
			else if (NSPointInRect(curPoint, rightSelectionBar) || NSPointInRect(curPoint, rightSelectionKnob))
			{
				isTracking = YES;
				trackingXAdjust = (int)(curPoint.x - rightEdge);
				trackingStartBase = selectionFirstBase;	// we're dragging the right knob, so the left knob is the tracking anchor
				trackingLastBase = [self baseForPosition:(curPoint.x - trackingXAdjust) interiorRect:interiorRect displayedRange:displayedRange];	// instead of selectionLastBase, so the selection does not change at all if the mouse does not move
				
				[self mouseDragged:theEvent];	// the click may not be aligned exactly on the center of the bar, so clicking might shift it a bit; do that now
				return;
			}
		}
		
		if (NSPointInRect(curPoint, contentRect))
		{
			isTracking = YES;
			trackingStartBase = [self baseForPosition:curPoint.x interiorRect:interiorRect displayedRange:displayedRange];
			trackingLastBase = trackingStartBase;
			trackingXAdjust = 0;
			
			// We start off with no selection, and wait for the user to drag out a selection
			if (hasSelection)
			{
				hasSelection = NO;
				
				// Save the selection for restoring across recycles, etc.
				savedHasSelection = hasSelection;
				
				[self setNeedsDisplayAll];
				[[NSNotificationCenter defaultCenter] postNotificationName:SLiMChromosomeSelectionChangedNotification object:self];
			}
		}
	}
}

- (void)setUpMarker:(SLiMSelectionMarker **)marker atBase:(slim_position_t)selectionBase isLeft:(BOOL)isLeftMarker
{
	BOOL justCreated = NO;
	
	// if the marker is not yet created, create it
	if (!*marker)
	{
		*marker = [SLiMSelectionMarker new];
		justCreated = YES;
	}
	
	// then move the marker to the appropriate position
	NSRect interiorRect = [self interiorRect];
	NSRange displayedRange = [self displayedRange];
	NSRect selectionRect = [self rectEncompassingBase:selectionFirstBase toBase:selectionLastBase interiorRect:interiorRect displayedRange:displayedRange];
	NSPoint selectionStartTipPoint = NSMakePoint(selectionRect.origin.x, interiorRect.origin.y + interiorRect.size.height - 3);
	NSPoint selectionEndTipPoint = NSMakePoint(selectionRect.origin.x + selectionRect.size.width - 1, interiorRect.origin.y + interiorRect.size.height - 3);
	NSPoint tipPoint = (isLeftMarker ? selectionStartTipPoint : selectionEndTipPoint);
	
	tipPoint = [self convertPoint:tipPoint toView:nil];
	tipPoint = [[self window] convertRectToScreen:NSMakeRect(tipPoint.x, tipPoint.y, 0, 0)].origin;
	
	// BCH 15 May 2018: display in scientific notation for positions at or above 1e10, as it gets a bit ridiculous...
	if (selectionBase >= 1e10)
		[*marker setLabel:[NSString stringWithFormat:@"%.6e", (double)selectionBase]];
	else
		[*marker setLabel:[NSString stringWithFormat:@"%lld", (int64_t)selectionBase]];
	
	[*marker setTipPoint:tipPoint];
	[*marker setIsLeftMarker:isLeftMarker];
	
	if (justCreated)
		[*marker orderFront:nil];
}

- (void)_mouseTrackEvent:(NSEvent *)theEvent
{
	NSRect interiorRect = [self interiorRect];
	NSRange displayedRange = [self displayedRange];
	NSPoint curPoint = [self convertPoint:[theEvent locationInWindow] fromView:nil];
	
	// We deal with rounded x-coordinates to make adjusting the click point easier
	curPoint.x = (int)round(curPoint.x);
	
	NSPoint correctedPoint = NSMakePoint(curPoint.x - trackingXAdjust, curPoint.y);
	slim_position_t trackingNewBase = [self baseForPosition:correctedPoint.x interiorRect:interiorRect displayedRange:displayedRange];
	BOOL selectionChanged = NO;
	
	if (trackingNewBase != trackingLastBase)
	{
		trackingLastBase = trackingNewBase;
		
		slim_position_t trackingLeftBase = trackingStartBase, trackingRightBase = trackingLastBase;
		
		if (trackingLeftBase > trackingRightBase)
		{
			trackingLeftBase = trackingLastBase;
			trackingRightBase = trackingStartBase;
		}
		
		if (trackingLeftBase <= (slim_position_t)displayedRange.location)
			trackingLeftBase = (slim_position_t)displayedRange.location;
		if (trackingRightBase > (slim_position_t)(displayedRange.location + displayedRange.length) - 1)
			trackingRightBase = (slim_position_t)(displayedRange.location + displayedRange.length) - 1;
		
		if (trackingRightBase <= trackingLeftBase + 100)
		{
			if (hasSelection)
				selectionChanged = YES;
			
			hasSelection = NO;
			
			// Save the selection for restoring across recycles, etc.
			savedHasSelection = hasSelection;
			
			[self removeSelectionMarkers];
		}
		else
		{
			selectionChanged = YES;
			hasSelection = YES;
			selectionFirstBase = trackingLeftBase;
			selectionLastBase = trackingRightBase;
			
			// Save the selection for restoring across recycles, etc.
			savedSelectionFirstBase = selectionFirstBase;
			savedSelectionLastBase = selectionLastBase;
			savedHasSelection = hasSelection;
			
			[self setUpMarker:&startMarker atBase:selectionFirstBase isLeft:YES];
			[self setUpMarker:&endMarker atBase:selectionLastBase isLeft:NO];
		}
		
		if (selectionChanged)
		{
			[self setNeedsDisplayAll];
			[[NSNotificationCenter defaultCenter] postNotificationName:SLiMChromosomeSelectionChangedNotification object:self];
		}
	}
}

- (void)mouseDragged:(NSEvent *)theEvent
{
	if ([self isSelectable] && isTracking)
		[self _mouseTrackEvent:theEvent];
}

- (void)mouseUp:(NSEvent *)theEvent
{
	if ([self isSelectable] && isTracking)
	{
		[self _mouseTrackEvent:theEvent];
		[self removeSelectionMarkers];
	}
	
	isTracking = NO;
}

- (IBAction)displayFrequencies:(id)sender
{
	if (display_haplotypes_)
	{
		display_haplotypes_ = NO;
		
		[self setNeedsDisplayAll];
	}
}

- (IBAction)displayHaplotypes:(id)sender
{
	if (!display_haplotypes_)
	{
		display_haplotypes_ = YES;
		
		[self setNeedsDisplayAll];
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
			display_muttypes_.push_back(muttype_id);
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
	SLiMSim *sim = controller->sim;
	
	if (sim)
	{
		display_muttypes_.clear();
		
		std::map<slim_objectid_t,MutationType*> &muttypes = sim->mutation_types_;
		
		for (auto muttype_iter : muttypes)
		{
			MutationType *muttype = muttype_iter.second;
			slim_objectid_t muttype_id = muttype->mutation_type_id_;
			
			if ((muttype->dfe_type_ != DFEType::kFixed) || (muttype->dfe_parameters_[0] != 0.0))
				display_muttypes_.push_back(muttype_id);
		}
		
		[self setNeedsDisplayAll];
	}
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
	SLiMWindowController *controller = (SLiMWindowController *)[[self window] windowController];
	
	if (![controller invalidSimulation] && ![[controller window] attachedSheet] && ![self isSelectable] && [self enabled])
	{
		SLiMSim *sim = controller->sim;
		
		if (sim)
		{
			std::map<slim_objectid_t,MutationType*> &muttypes = sim->mutation_types_;
			
			if (muttypes.size() > 0)
			{
				NSMenu *menu = [[NSMenu alloc] initWithTitle:@"chromosome_menu"];
				NSMenuItem *menuItem;
				
				menuItem = [menu addItemWithTitle:@"Display Frequencies" action:@selector(displayFrequencies:) keyEquivalent:@""];
				[menuItem setTarget:self];
				[menuItem setState:(display_haplotypes_ ? NSOffState : NSOnState)];
				
				menuItem = [menu addItemWithTitle:@"Display Haplotypes" action:@selector(displayHaplotypes:) keyEquivalent:@""];
				[menuItem setTarget:self];
				[menuItem setState:(display_haplotypes_ ? NSOnState : NSOffState)];
				
				[menu addItem:[NSMenuItem separatorItem]];
				
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
					
					all_muttypes.push_back(muttype_id);
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









































































