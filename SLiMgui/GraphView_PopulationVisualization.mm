//
//  GraphView_PopulationVisualization.m
//  SLiM
//
//  Created by Ben Haller on 3/3/15.
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


#import "GraphView_PopulationVisualization.h"
#import "SLiMWindowController.h"


@implementation GraphView_PopulationVisualization

- (id)initWithFrame:(NSRect)frameRect withController:(SLiMWindowController *)controller
{
	if (self = [super initWithFrame:frameRect withController:controller])
	{
		[self setShowXAxis:NO];
		[self setShowYAxis:NO];
	}
	
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

- (NSRect)rectForSubpop:(Subpopulation *)subpop centeredAt:(NSPoint)center
{
	// figure out the right radius, clamped to reasonable limits
	int subpopSize = subpop->parent_subpop_size_;
	
	if (subpopSize < 200)
		subpopSize = 200;
	if (subpopSize > 10000)
		subpopSize = 10000;
	
	double subpopRadius = sqrt(subpop->parent_subpop_size_) / 500;	// size 10,000 has radius 0.2
	
	// draw the circle
	NSRect subpopRect = NSMakeRect(center.x - subpopRadius, center.y - subpopRadius, 2.0 * subpopRadius, 2.0 * subpopRadius);
	
	return subpopRect;
}

- (void)drawSubpop:(Subpopulation *)subpop withID:(int)subpopID centeredAt:(NSPoint)center controller:(SLiMWindowController *)controller
{
	static NSDictionary *labelAttrs = nil;
	
	if (!labelAttrs)
		labelAttrs = [[NSDictionary alloc] initWithObjectsAndKeys:[NSFont fontWithName:[GraphView labelFontName] size:0.04], NSFontAttributeName, nil];
	
	// figure out the right radius, clamped to reasonable limits
	int subpopSize = subpop->parent_subpop_size_;
	
	if (subpopSize < 200)
		subpopSize = 200;
	if (subpopSize > 10000)
		subpopSize = 10000;
	
	double subpopRadius = sqrt(subpop->parent_subpop_size_) / 500;	// size 10,000 has radius 0.2
	subpop->gui_radius = subpopRadius;
	
	// calculate the color from the mean fitness of the population
	double scalingFactor = controller->fitnessColorScale;
	double totalFitness = subpop->parental_total_fitness_;
	double fitness = totalFitness / subpopSize;
	float colorRed = 0.0, colorGreen = 0.0, colorBlue = 0.0;
	RGBForFitness(fitness, &colorRed, &colorGreen, &colorBlue, scalingFactor);
	NSColor *fitnessColor = [NSColor colorWithDeviceRed:colorRed green:colorGreen blue:colorBlue alpha:1.0];	// device, to match OpenGL
	
	// draw the circle
	NSRect subpopRect = NSMakeRect(center.x - subpopRadius, center.y - subpopRadius, 2.0 * subpopRadius, 2.0 * subpopRadius);
	NSBezierPath *subpopCircle = [NSBezierPath bezierPathWithOvalInRect:subpopRect];
	
	[fitnessColor set];
	[subpopCircle fill];
	
	[[NSColor blackColor] set];
	[subpopCircle setLineWidth:0.002];
	[subpopCircle stroke];
	
	// label it with the subpopulation ID
	NSString *popString = [NSString stringWithFormat:@"p%d", subpopID];
	NSAttributedString *popLabel = [[NSAttributedString alloc] initWithString:popString attributes:labelAttrs];
	NSSize labelSize = [popLabel size];
	
	[popLabel drawAtPoint:NSMakePoint(center.x - labelSize.width / 2, center.y - labelSize.height - 0.008)];
	[popLabel release];
}

- (void)drawArrowFromSubpop:(Subpopulation *)sourceSubpop toSubpop:(Subpopulation *)destSubpop migrantFraction:(double)migrantFraction
{
	double destCenterX = destSubpop->gui_center_x;
	double destCenterY = destSubpop->gui_center_y;
	double sourceCenterX = sourceSubpop->gui_center_x;
	double sourceCenterY = sourceSubpop->gui_center_y;
	
	// we want to draw an arrow connecting these two subpops; first, we need to figure out the endpoints
	// they start and end a small fixed distance outside of the source/dest subpop circles
	double vectorDX = (destCenterX - sourceCenterX);
	double vectorDY = (destCenterY - sourceCenterY);
	double vectorLength = sqrt(vectorDX * vectorDX + vectorDY * vectorDY);
	double sourceEndWeight = (0.01 + sourceSubpop->gui_radius) / vectorLength;
	double sourceEndX = sourceCenterX + (destCenterX - sourceCenterX) * sourceEndWeight;
	double sourceEndY = sourceCenterY + (destCenterY - sourceCenterY) * sourceEndWeight;
	double destEndWeight = (0.01 + destSubpop->gui_radius) / vectorLength;
	double destEndX = destCenterX + (sourceCenterX - destCenterX) * destEndWeight;
	double destEndY = destCenterY + (sourceCenterY - destCenterY) * destEndWeight;
	
	// now, using those endpoints, we have a "partial vector" that goes from just outside the source subpop circle to
	// just outside the dest subpop circle; this partial vector will be the basis for the bezier that we draw, but we
	// need to calculate control points to make the bezier curve outward, using a perpendicular vector
	double partVecDX = destEndX - sourceEndX;	// dx/dy for the partial vector from source to dest that we have just calculated
	double partVecDY = destEndY - sourceEndY;
	double partVecLength = sqrt(partVecDX * partVecDX + partVecDY * partVecDY);		// the length of that partial vector
	double perpendicularFromSourceDX = partVecDY;		// a vector perpendicular to that partial vector, by clockwise rotation
	double perpendicularFromSourceDY = -partVecDX;
	double controlPoint1X = sourceEndX + partVecDX * 0.3 + perpendicularFromSourceDX * 0.1;
	double controlPoint1Y = sourceEndY + partVecDY * 0.3 + perpendicularFromSourceDY * 0.1;
	double controlPoint2X = destEndX - partVecDX * 0.3 + perpendicularFromSourceDX * 0.1;
	double controlPoint2Y = destEndY - partVecDY * 0.3 + perpendicularFromSourceDY * 0.1;
	
	// now we figure out our line width, and we calculate a spatial translation of the bezier to shift in slightly off of
	// the midline, based on the line width, so that incoming and outgoing vectors do not overlap at the start/end points
	double lineWidth = 0.001 * (sqrt(migrantFraction) / 0.03);	// non-linear line width scale
	double finalShiftMagnitude = MAX(lineWidth * 0.75, 0.010);
	double finalShiftX = perpendicularFromSourceDX * finalShiftMagnitude / partVecLength;
	double finalShiftY = perpendicularFromSourceDY * finalShiftMagnitude / partVecLength;
	double arrowheadSize = MAX(lineWidth * 1.5, 0.008);
	
	// we have to use a clipping path to cut back the destination end of the vector, to make room for the arrowhead
	[NSGraphicsContext saveGraphicsState];
	
	double clipRadius = vectorLength - (destSubpop->gui_radius + arrowheadSize + 0.01);
	NSRect clipCircle = NSMakeRect(sourceCenterX - clipRadius, sourceCenterY - clipRadius, clipRadius * 2, clipRadius * 2);
	NSBezierPath *clipBezier = [NSBezierPath bezierPathWithOvalInRect:clipCircle];
	
//	[[NSColor redColor] set];
//	[clipBezier setLineWidth:0.001];
//	[clipBezier stroke];
	[clipBezier addClip];
	
	// now draw the curved line connecting the subpops
	NSBezierPath *bezierLines = [NSBezierPath bezierPath];
	double shiftedSourceEndX = sourceEndX + finalShiftX, shiftedSourceEndY = sourceEndY + finalShiftY;
	double shiftedDestEndX = destEndX + finalShiftX, shiftedDestEndY = destEndY + finalShiftY;
	double shiftedControl1X = controlPoint1X + finalShiftX, shiftedControl1Y = controlPoint1Y + finalShiftY;
	double shiftedControl2X = controlPoint2X + finalShiftX, shiftedControl2Y = controlPoint2Y + finalShiftY;
	
	[bezierLines moveToPoint:NSMakePoint(shiftedSourceEndX, shiftedSourceEndY)];
	//[bezierLines lineToPoint:NSMakePoint(shiftedDestEndX, shiftedDestEndY)];
	[bezierLines curveToPoint:NSMakePoint(shiftedDestEndX, shiftedDestEndY)
				controlPoint1:NSMakePoint(shiftedControl1X, shiftedControl1Y)
				controlPoint2:NSMakePoint(shiftedControl2X, shiftedControl2Y)];
	
	[[NSColor blackColor] set];
	[bezierLines setLineWidth:lineWidth];
	[bezierLines stroke];
	
	// restore the clipping path
	[NSGraphicsContext restoreGraphicsState];
	
	// draw the arrowhead; this is oriented along the line from (shiftedDestEndX, shiftedDestEndY) to (shiftedControl2X, shiftedControl2Y),
	// of length partVecLength, and is calculated using a perpendicular off of that vector
	NSBezierPath *bezierArrowheads = [NSBezierPath bezierPath];
	double angleCorrectionFactor = (arrowheadSize / partVecLength) * 0.5;	// large arrowheads need to be tilted closer to the source-dest pop line
	//NSLog(@"angleCorrectionFactor == %f (arrowheadSize == %f, partVecLength == %f)", angleCorrectionFactor, arrowheadSize, partVecLength);
	double headInsideX = shiftedControl2X * (1 - angleCorrectionFactor) + shiftedSourceEndX * angleCorrectionFactor;
	double headInsideY = shiftedControl2Y * (1 - angleCorrectionFactor) + shiftedSourceEndY * angleCorrectionFactor;
	double headMidlineDX = headInsideX - shiftedDestEndX, headMidlineDY = headInsideY - shiftedDestEndY;
	double headMidlineLength = sqrt(headMidlineDX * headMidlineDX + headMidlineDY * headMidlineDY);
	double headMidlineNormDX = (headMidlineDX / headMidlineLength) * arrowheadSize;					// normalized to have length arrowheadSize
	double headMidlineNormDY = (headMidlineDY / headMidlineLength) * arrowheadSize;
	double headPerpendicular1DX = headMidlineNormDY, headPerpendicular1DY = -headMidlineNormDX;		// perpendicular to the normalized midline
	double headPerpendicular2DX = -headMidlineNormDY, headPerpendicular2DY = headMidlineNormDX;		// just the negation of perpendicular 1
	
	[bezierArrowheads moveToPoint:NSMakePoint(shiftedDestEndX, shiftedDestEndY)];
	[bezierArrowheads lineToPoint:NSMakePoint(shiftedDestEndX + headMidlineNormDX * 1.75 + headPerpendicular1DX * 0.9, shiftedDestEndY + headMidlineNormDY * 1.75 + headPerpendicular1DY * 0.9)];
	[bezierArrowheads lineToPoint:NSMakePoint(shiftedDestEndX + headMidlineNormDX * 1.2, shiftedDestEndY + headMidlineNormDY * 1.2)];
	[bezierArrowheads lineToPoint:NSMakePoint(shiftedDestEndX + headMidlineNormDX * 1.75 + headPerpendicular2DX * 0.9, shiftedDestEndY + headMidlineNormDY * 1.75 + headPerpendicular2DY * 0.9)];
	[bezierArrowheads closePath];
	
	[[NSColor blackColor] set];
	[bezierArrowheads fill];
}

- (void)drawGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller
{
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	int subpopCount = (int)pop.size();
	
	// First, we transform our coordinate system so that a square of size (1,1) fits maximally and centered
	[NSGraphicsContext saveGraphicsState];
	
	NSAffineTransform *transform = [NSAffineTransform transform];
	if (interiorRect.size.width > interiorRect.size.height)
	{
		[transform translateXBy:interiorRect.origin.x yBy:interiorRect.origin.y];
		[transform translateXBy:SCREEN_ROUND((interiorRect.size.width - interiorRect.size.height) / 2) yBy:0];
		[transform scaleBy:interiorRect.size.height];
	}
	else
	{
		[transform translateXBy:interiorRect.origin.x yBy:interiorRect.origin.y];
		[transform translateXBy:0 yBy:SCREEN_ROUND((interiorRect.size.height - interiorRect.size.width) / 2)];
		[transform scaleBy:interiorRect.size.width];
	}
	[transform concat];
	
	// test frame
	[[NSColor blackColor] set];
	NSFrameRectWithWidth(NSMakeRect(0, 0, 1, 1), 0.002);
	
	if (subpopCount == 1)
	{
		auto subpopIter = pop.begin();
		
		// a single subpop is shown as a circle at the center
		[self drawSubpop:(*subpopIter).second withID:(*subpopIter).first centeredAt:NSMakePoint(0.5, 0.5) controller:controller];
	}
	else if (subpopCount > 1)
	{
		// first we do some sizing, to figure out the maximum extent of our subpops, distributed in a ring
		NSRect boundingBox = NSZeroRect;
		
		{
			auto subpopIter = pop.begin();
			
			for (int subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
			{
				Subpopulation *subpop = (*subpopIter).second;
				double theta = (M_PI * 2.0 / subpopCount) * subpopIndex + M_PI_2;
				
				subpop->gui_center_x = 0.5 + cos(theta) * 0.29;
				subpop->gui_center_y = 0.5 + sin(theta) * 0.29;
				NSPoint center = NSMakePoint(subpop->gui_center_x, subpop->gui_center_y);
				NSRect subpopRect = [self rectForSubpop:subpop centeredAt:center];
				
				boundingBox = ((subpopIndex == 0) ? subpopRect : NSUnionRect(boundingBox, subpopRect));
				
				++subpopIter;
			}
		}
		
		// then we translate our coordinate system so that the subpops are centered within our (0, 0, 1, 1) box
		double offsetX = ((1.0 - boundingBox.size.width) / 2.0) - boundingBox.origin.x;
		double offsetY = ((1.0 - boundingBox.size.height) / 2.0) - boundingBox.origin.y;
		
		NSAffineTransform *offsetTransform = [NSAffineTransform transform];
		[offsetTransform translateXBy:offsetX yBy:offsetY];
		[offsetTransform concat];
		
		// then we draw the subpops
		{
			auto subpopIter = pop.begin();
			
			for (int subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
			{
				Subpopulation *subpop = (*subpopIter).second;
				int subpopID = (*subpopIter).first;
				NSPoint center = NSMakePoint(subpop->gui_center_x, subpop->gui_center_y);
				
				[self drawSubpop:subpop withID:subpopID centeredAt:center controller:controller];
				++subpopIter;
			}
		}
		
		// in the multipop case, we need to draw migration arrows, too
		for (auto destSubpopIter = pop.begin(); destSubpopIter != pop.end(); ++destSubpopIter)
		{
			Subpopulation *destSubpop = (*destSubpopIter).second;
			std::map<int,double> &destMigrants = destSubpop->migrant_fractions_;
			
			for (auto sourceSubpopIter = destMigrants.begin(); sourceSubpopIter != destMigrants.end(); ++sourceSubpopIter)
			{
				int sourceSubpopID = (*sourceSubpopIter).first;
				Subpopulation *sourceSubpop = &(pop.SubpopulationWithID(sourceSubpopID));
				double migrantFraction = (*sourceSubpopIter).second;
				
				[self drawArrowFromSubpop:sourceSubpop toSubpop:destSubpop migrantFraction:migrantFraction];
			}
		}
	}
	
	// We're done with our transformed coordinate system
	[NSGraphicsContext restoreGraphicsState];
}

@end





























































