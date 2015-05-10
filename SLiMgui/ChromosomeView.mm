//
//  ChromosomeView.m
//  SLiM
//
//  Created by Ben Haller on 1/21/15.
//  Copyright (c) 2015 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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


NSString *SLiMChromosomeSelectionChangedNotification = @"SLiMChromosomeSelectionChangedNotification";

static NSDictionary *tickAttrs = nil;
const int numberOfTicks = 4;
const int tickLength = 5;
const int heightForTicks = 16;
const int selectionKnobSizeExtension = 2;	// a 5-pixel-width knob is 2: 2 + 1 + 2, an extension on each side plus the one pixel of the bar in the middle
const int selectionKnobSize = selectionKnobSizeExtension + selectionKnobSizeExtension + 1;

@implementation ChromosomeView

@synthesize referenceChromosomeView, shouldDrawGenomicElements, shouldDrawFixedSubstitutions, shouldDrawMutations, shouldDrawRecombinationIntervals;

+ (void)initialize
{
	if (!tickAttrs)
		tickAttrs = [[NSDictionary alloc] initWithObjectsAndKeys:[NSColor blackColor], NSForegroundColorAttributeName, [NSFont systemFontOfSize:9.0], NSFontAttributeName, nil];
	
	[self exposeBinding:@"enabled"];
}

- (id)initWithFrame:(NSRect)frameRect
{
	if (self = [super initWithFrame:frameRect])
	{
		// this does not seem to be called for objects in the nib...
	}
	
	return self;
}

- (void)setEnabled:(BOOL)enabled
{
	if (_enabled != enabled)
	{
		_enabled = enabled;
		
		[self setNeedsDisplay:YES];
	}
}

- (void)awakeFromNib
{
	[self bind:@"enabled" toObject:[[self window] windowController] withKeyPath:@"invalidSimulation" options:[NSDictionary dictionaryWithObject:NSNegateBooleanTransformerName forKey:NSValueTransformerNameBindingOption]];
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
	
	[self unbind:@"enabled"];
	
	[self removeSelectionMarkers];
	
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
	[self setNeedsDisplay:YES];
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
		int chromosomeLength = chromosome.length_;
		
		return NSMakeRange(1, chromosomeLength + 1);	// chromosomeLength is zero-based, so chromosomeLength + 1 bases are encompassed
	}
}

- (void)setSelectedRange:(NSRange)selectionRange
{
	if ([self isSelectable] && (selectionRange.length >= 1))
	{
		selectionFirstBase = (int)selectionRange.location;
		selectionLastBase = (int)(selectionRange.location + selectionRange.length) - 1;
		hasSelection = YES;
	}
	else if (hasSelection)
	{
		hasSelection = NO;
	}
	else
	{
		return;
	}
	
	// Our selection changed, so update and post a change notification
	[self setNeedsDisplay:YES];
	
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
		int chromosomeLength = chromosome.length_;
		
		return NSMakeRange(1, chromosomeLength + 1);	// chromosomeLength is zero-based, so chromosomeLength + 1 bases are encompassed
	}
}

- (NSRect)rectEncompassingBase:(int)startBase toBase:(int)endBase interiorRect:(NSRect)interiorRect displayedRange:(NSRange)displayedRange
{
	double startFraction = (startBase - (int)displayedRange.location) / (double)(displayedRange.length);
	double leftEdgeDouble = interiorRect.origin.x + startFraction * interiorRect.size.width;
	double endFraction = (endBase + 1 - (int)displayedRange.location) / (double)(displayedRange.length);
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

- (int)baseForPosition:(double)position interiorRect:(NSRect)interiorRect displayedRange:(NSRange)displayedRange
{
	double fraction = (position - interiorRect.origin.x) / interiorRect.size.width;
	int base = (int)floor(fraction * (displayedRange.length + 1) + displayedRange.location);
	
	return base;
}

- (void)drawTicksInContentRect:(NSRect)contentRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	NSRect interiorRect = [self interiorRect];
	
	for (int position = 0; position <= numberOfTicks; ++position)
	{
		int tickBase = (int)displayedRange.location + (int)floor((displayedRange.length - 1) * (position / (double)numberOfTicks));	// -1 because we are choosing an in-between-base position that falls, at most, to the left of the last base
		NSRect tickRect = [self rectEncompassingBase:tickBase toBase:tickBase interiorRect:interiorRect displayedRange:displayedRange];
		
		tickRect.origin.y = contentRect.origin.y - tickLength;
		tickRect.size.height = tickLength;
		
		[[NSColor colorWithCalibratedWhite:0.5 alpha:1.0] set];
		NSRectFill(tickRect);
		
		NSString *tickLabel = [NSString stringWithFormat:@"%d", tickBase];
		NSAttributedString *tickAttrLabel = [[NSAttributedString alloc] initWithString:tickLabel attributes:tickAttrs];
		NSSize tickLabelSize = [tickAttrLabel size];
		int tickLabelX = (int)floor(tickRect.origin.x + tickRect.size.width / 2.0);
		
		if (position == numberOfTicks)
			tickLabelX -= (tickLabelSize.width - 2);
		else if (position > 0)
			tickLabelX -= (int)round(tickLabelSize.width / 2.0);
		
		[tickAttrLabel drawAtPoint:NSMakePoint(tickLabelX, contentRect.origin.y - (tickLength + 12))];
		[tickAttrLabel release];
	}
}

- (void)drawGenomicElementsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	Chromosome &chromosome = controller->sim->chromosome_;
	
	for (GenomicElement &genomicElement : chromosome)
	{
		int startPosition = genomicElement.start_position_ + 1;		// +1 because the back end is 0-based
		int endPosition = genomicElement.end_position_ + 1;
		NSRect elementRect = [self rectEncompassingBase:startPosition toBase:endPosition interiorRect:interiorRect displayedRange:displayedRange];
		
		// if we're drawing recombination intervals as well, then they get the top half and we take the bottom half
		if (shouldDrawRecombinationIntervals)
			elementRect.size.height = (int)round(elementRect.size.height / 2.0);
		
		// draw only the visible part, if any
		elementRect = NSIntersectionRect(elementRect, interiorRect);
		
		if (!NSIsEmptyRect(elementRect))
		{
			int elementTypeID = genomicElement.genomic_element_type_ptr_->genomic_element_type_id_;
			NSColor *elementColor = [controller colorForGenomicElementTypeID:elementTypeID];
			
			[elementColor set];
			NSRectFill(elementRect);
		}
	}
}

- (void)drawRecombinationIntervalsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	Chromosome &chromosome = controller->sim->chromosome_;
	int recombinationIntervalCount = (int)chromosome.recombination_end_positions_.size();
	int intervalStartPosition = 1;
	
	for (int interval = 0; interval < recombinationIntervalCount; ++interval)
	{
		int intervalEndPosition = chromosome.recombination_end_positions_[interval] + 1;	// +1 because the back end is 0-based
		double intervalRate = chromosome.recombination_rates_[interval];
		NSRect intervalRect = [self rectEncompassingBase:intervalStartPosition toBase:intervalEndPosition interiorRect:interiorRect displayedRange:displayedRange];
		
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
		}
		
		// the next interval starts at the next base after this one ended
		intervalStartPosition = intervalEndPosition + 1;
	}
}

- (void)drawFixedSubstitutionsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	double scalingFactor = controller->selectionColorScale;
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	std::vector<SLIMCONST Substitution*> &substitutions = pop.substitutions_;
	
	for (const Substitution *substitution : substitutions)
	{
		int substitutionPosition = substitution->position_ + 1;		// +1 because the back end is 0-based
		NSRect substitutionTickRect = [self rectEncompassingBase:substitutionPosition toBase:substitutionPosition interiorRect:interiorRect displayedRange:displayedRange];
		
		if (shouldDrawMutations)
		{
			// If we're drawing mutations as well, then substitutions just get colored blue, to contrast
			[[NSColor colorWithCalibratedRed:0.2 green:0.2 blue:1.0 alpha:1.0] set];
		}
		else
		{
			// If we're not drawing mutations as well, then substitutions get colored by selection coefficient, like mutations
			float colorRed = 0.0, colorGreen = 0.0, colorBlue = 0.0;
			
			RGBForSelectionCoeff(substitution->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
			[[NSColor colorWithCalibratedRed:colorRed green:colorGreen blue:colorBlue alpha:1.0] set];
		}
		
		NSRectFill(substitutionTickRect);
	}
}

- (void)drawMutationsInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller displayedRange:(NSRange)displayedRange
{
	double scalingFactor = controller->selectionColorScale;
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	double totalGenomeCount = pop.gui_total_genome_count_;				// this includes only genomes in the selected subpopulations
	Genome &mutationRegistry = pop.mutation_registry_;
	SLIMCONST Mutation **mutations = mutationRegistry.mutations_;
	int mutationCount = mutationRegistry.mutation_count_;
	
	for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
	{
		const Mutation *mutation = mutations[mutIndex];
		int32_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
		int mutationPosition = mutation->position_ + 1;					// +1 because the back end is 0-based
		NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
		float colorRed = 0.0, colorGreen = 0.0, colorBlue = 0.0;
		
		mutationTickRect.size.height = (int)ceil((mutationRefCount / totalGenomeCount) * interiorRect.size.height);
		RGBForSelectionCoeff(mutation->selection_coeff_, &colorRed, &colorGreen, &colorBlue, scalingFactor);
		[[NSColor colorWithCalibratedRed:colorRed green:colorGreen blue:colorBlue alpha:1.0] set];
		NSRectFill(mutationTickRect);
	}
}

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

- (void)drawRect:(NSRect)dirtyRect
{
	SLiMWindowController *controller = [[self window] windowController];
	NSRect contentRect = [self contentRect];
	NSRect interiorRect = [self interiorRect];
	
	if ([self enabled] && ![controller invalidSimulation])
	{
		// erase the content area itself
		[[NSColor colorWithCalibratedWhite:0.0 alpha:1.0] set];
		NSRectFill(interiorRect);
		
		NSRange displayedRange = [self displayedRange];
		
		// draw ticks at bottom of content rect
		[self drawTicksInContentRect:contentRect withController:controller displayedRange:displayedRange];
		
		// draw recombination intervals in interior
		if (shouldDrawRecombinationIntervals)
			[self drawRecombinationIntervalsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
		
		// draw genomic elements in interior
		if (shouldDrawGenomicElements)
			[self drawGenomicElementsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
		
		// draw fixed substitutions in interior
		if (shouldDrawFixedSubstitutions)
			[self drawFixedSubstitutionsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
		
		// draw mutations in interior
		if (shouldDrawMutations)
			[self drawMutationsInInteriorRect:interiorRect withController:controller displayedRange:displayedRange];
		
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

- (void)mouseDown:(NSEvent *)theEvent
{
	if ([self isSelectable] && [self enabled] && ![[[self window] windowController] invalidSimulation])
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
				int clickedBase = [self baseForPosition:curPoint.x interiorRect:interiorRect displayedRange:displayedRange];	// this is 1-based
				NSRange selectionRange = NSMakeRange(0, 0);
				SLiMWindowController *controller = (SLiMWindowController *)[[self window] windowController];
				Chromosome &chromosome = controller->sim->chromosome_;
				
				for (GenomicElement &genomicElement : chromosome)
				{
					int startPosition = genomicElement.start_position_ + 1;		// +1 because the back end is 0-based
					int endPosition = genomicElement.end_position_ + 1;
					
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
				[self setNeedsDisplay:YES];
				[[NSNotificationCenter defaultCenter] postNotificationName:SLiMChromosomeSelectionChangedNotification object:self];
			}
		}
	}
}

- (void)setUpMarker:(SLiMSelectionMarker **)marker atBase:(int)selectionBase isLeft:(BOOL)isLeftMarker
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
	
	[*marker setLabel:[NSString stringWithFormat:@"%d", selectionBase]];
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
	int trackingNewBase = [self baseForPosition:correctedPoint.x interiorRect:interiorRect displayedRange:displayedRange];
	BOOL selectionChanged = NO;
	
	if (trackingNewBase != trackingLastBase)
	{
		trackingLastBase = trackingNewBase;
		
		int trackingLeftBase = trackingStartBase, trackingRightBase = trackingLastBase;
		
		if (trackingLeftBase > trackingRightBase)
			trackingLeftBase = trackingLastBase, trackingRightBase = trackingStartBase;
		
		if (trackingLeftBase <= (int)displayedRange.location)
			trackingLeftBase = (int)displayedRange.location;
		if (trackingRightBase > (int)(displayedRange.location + displayedRange.length) - 1)
			trackingRightBase = (int)(displayedRange.location + displayedRange.length) - 1;
		
		if (trackingRightBase <= trackingLeftBase + 100)
		{
			if (hasSelection)
				selectionChanged = YES;
			
			hasSelection = NO;
			
			[self removeSelectionMarkers];
		}
		else
		{
			selectionChanged = YES;
			hasSelection = YES;
			selectionFirstBase = trackingLeftBase;
			selectionLastBase = trackingRightBase;
			
			[self setUpMarker:&startMarker atBase:selectionFirstBase isLeft:YES];
			[self setUpMarker:&endMarker atBase:selectionLastBase isLeft:NO];
		}
		
		if (selectionChanged)
		{
			[self setNeedsDisplay:YES];
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

@end









































































