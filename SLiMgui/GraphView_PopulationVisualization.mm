//
//  GraphView_PopulationVisualization.m
//  SLiM
//
//  Created by Ben Haller on 3/3/15.
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
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	slim_popsize_t clampedSubpopSize = subpopSize;
	
	if (clampedSubpopSize < 200)
		clampedSubpopSize = 200;
	if (clampedSubpopSize > 10000)
		clampedSubpopSize = 10000;
	
	double subpopRadius = sqrt(clampedSubpopSize) / 500;	// size 10,000 has radius 0.2
	
	if (subpop->gui_radius_scaling_from_user_)
		subpopRadius *= subpop->gui_radius_scaling_;
	
	// draw the circle
	NSRect subpopRect = NSMakeRect(center.x - subpopRadius, center.y - subpopRadius, 2.0 * subpopRadius, 2.0 * subpopRadius);
	
	return subpopRect;
}

- (void)drawSubpop:(Subpopulation *)subpop withID:(slim_objectid_t)subpopID centeredAt:(NSPoint)center controller:(SLiMWindowController *)controller
{
	static NSDictionary *blackLabelAttrs = nil, *whiteLabelAttrs = nil;
	
	if (!blackLabelAttrs)
		blackLabelAttrs = [@{NSFontAttributeName : [NSFont fontWithName:[GraphView labelFontName] size:0.04], NSForegroundColorAttributeName : [NSColor blackColor]} retain];
	if (!whiteLabelAttrs)
		whiteLabelAttrs = [@{NSFontAttributeName : [NSFont fontWithName:[GraphView labelFontName] size:0.04], NSForegroundColorAttributeName : [NSColor whiteColor]} retain];
	
	// figure out the right radius, clamped to reasonable limits
	slim_popsize_t subpopSize = subpop->parent_subpop_size_;
	slim_popsize_t clampedSubpopSize = subpopSize;
	
	if (clampedSubpopSize < 200)
		clampedSubpopSize = 200;
	if (clampedSubpopSize > 10000)
		clampedSubpopSize = 10000;
	
	double subpopRadius = sqrt(clampedSubpopSize) / 500;	// size 10,000 has radius 0.2
	
	if (subpop->gui_radius_scaling_from_user_)
		subpopRadius *= subpop->gui_radius_scaling_;
	
	subpop->gui_radius_ = subpopRadius;
	
	// determine the color
	float colorRed = 0.0, colorGreen = 0.0, colorBlue = 0.0;
	
	if (subpop->gui_color_from_user_)
	{
		colorRed = subpop->gui_color_red_;
		colorGreen = subpop->gui_color_green_;
		colorBlue = subpop->gui_color_blue_;
	}
	else
	{
		// calculate the color from the mean fitness of the population
		double scalingFactor = controller->fitnessColorScale;
		double totalFitness = subpop->parental_total_fitness_;
		double subpopFitnessScaling = subpop->last_fitness_scaling_;
		
		if ((subpopFitnessScaling <= 0.0) || !std::isfinite(subpopFitnessScaling))
			subpopFitnessScaling = 1.0;
		
		// we normalize fitness values with subpopFitnessScaling so individual fitness, unscaled by subpopulation fitness, is used for coloring
		double fitness = (totalFitness / subpopFitnessScaling) / subpopSize;
		RGBForFitness(fitness, &colorRed, &colorGreen, &colorBlue, scalingFactor);
	}
	
	NSColor *color = [NSColor colorWithDeviceRed:colorRed green:colorGreen blue:colorBlue alpha:1.0];	// device, to match OpenGL
	
	// draw the circle
	NSRect subpopRect = NSMakeRect(center.x - subpopRadius, center.y - subpopRadius, 2.0 * subpopRadius, 2.0 * subpopRadius);
	NSBezierPath *subpopCircle = [NSBezierPath bezierPathWithOvalInRect:subpopRect];
	
	[color set];
	[subpopCircle fill];
	
	[[NSColor blackColor] set];
	[subpopCircle setLineWidth:0.002];
	[subpopCircle stroke];
	
	// label it with the subpopulation ID
	NSString *popString = [NSString stringWithFormat:@"p%lld", (int64_t)subpopID];
	double brightness = 0.299 * colorRed + 0.587 * colorGreen + 0.114 * colorBlue;
	NSDictionary *labelAttrs;
	double scaling = 1.0;
	
	if (subpop->gui_radius_scaling_from_user_ && (subpop->gui_radius_scaling_ != 1.0))
	{
		scaling = subpop->gui_radius_scaling_;
		labelAttrs = @{NSFontAttributeName : [NSFont fontWithName:[GraphView labelFontName] size:(0.04 * scaling)], NSForegroundColorAttributeName : ((brightness > 0.5) ? [NSColor blackColor] : [NSColor whiteColor])};
	}
	else
	{
		labelAttrs = ((brightness > 0.5) ? blackLabelAttrs : whiteLabelAttrs);
	}
	
	NSAttributedString *popLabel = [[NSAttributedString alloc] initWithString:popString attributes:labelAttrs];
	NSSize labelSize = [popLabel size];
	
	// Note that labelSize.height seems to be clamped to a minimum of 1.0, although labelSize.width is not, which is odd; the math here seems to work...
	NSPoint drawPoint = NSMakePoint(center.x - labelSize.width / 2, center.y - labelSize.height - 0.008 * scaling);
	
	//NSLog(@"center = %@", NSStringFromPoint(center));
	//NSLog(@"labelSize = %@", NSStringFromSize(labelSize));
	//NSLog(@"drawPoint = %@", NSStringFromPoint(drawPoint));
	
	[popLabel drawAtPoint:drawPoint];
	[popLabel release];
}

- (void)drawArrowFromSubpop:(Subpopulation *)sourceSubpop toSubpop:(Subpopulation *)destSubpop migrantFraction:(double)migrantFraction
{
	double destCenterX = destSubpop->gui_center_x_;
	double destCenterY = destSubpop->gui_center_y_;
	double sourceCenterX = sourceSubpop->gui_center_x_;
	double sourceCenterY = sourceSubpop->gui_center_y_;
	
	// we want to draw an arrow connecting these two subpops; first, we need to figure out the endpoints
	// they start and end a small fixed distance outside of the source/dest subpop circles
	double vectorDX = (destCenterX - sourceCenterX);
	double vectorDY = (destCenterY - sourceCenterY);
	double vectorLength = sqrt(vectorDX * vectorDX + vectorDY * vectorDY);
	double sourceEndWeight = (0.01 + sourceSubpop->gui_radius_) / vectorLength;
	double sourceEndX = sourceCenterX + (destCenterX - sourceCenterX) * sourceEndWeight;
	double sourceEndY = sourceCenterY + (destCenterY - sourceCenterY) * sourceEndWeight;
	double destEndWeight = (0.01 + destSubpop->gui_radius_) / vectorLength;
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
	
	double clipRadius = vectorLength - (destSubpop->gui_radius_ + arrowheadSize + 0.01);
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

BOOL is_line_intersection(double p0_x, double p0_y, double p1_x, double p1_y, double p2_x, double p2_y, double p3_x, double p3_y);
BOOL is_line_intersection(double p0_x, double p0_y, double p1_x, double p1_y, double p2_x, double p2_y, double p3_x, double p3_y)
{
	double s02_x, s02_y, s10_x, s10_y, s32_x, s32_y, s_numer, t_numer, denom;
	s10_x = p1_x - p0_x;
	s10_y = p1_y - p0_y;
	s32_x = p3_x - p2_x;
	s32_y = p3_y - p2_y;
	
	denom = s10_x * s32_y - s32_x * s10_y;
	if (denom == 0)
		return FALSE; // Collinear
	bool denomPositive = denom > 0;
	
	s02_x = p0_x - p2_x;
	s02_y = p0_y - p2_y;
	s_numer = s10_x * s02_y - s10_y * s02_x;
	if ((s_numer < 0) == denomPositive)
		return FALSE; // No collision
	
	t_numer = s32_x * s02_y - s32_y * s02_x;
	if ((t_numer < 0) == denomPositive)
		return FALSE; // No collision
	
	if (((s_numer > denom) == denomPositive) || ((t_numer > denom) == denomPositive))
		return FALSE; // No collision
	
	return TRUE;
}

- (double)scorePositionsWithX:(double *)center_x y:(double *)center_y connected:(BOOL *)connected count:(int)subpopCount
{
	double score = 0.0;
	double meanEdge = 0.0;
	int edgeCount = 0;
	double minx = INFINITY, maxy = -INFINITY;
	
	// First we calculate the mean edge length; we will consider this the optimum length
	for (int subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
	{
		double x = center_x[subpopIndex];
		double y = center_y[subpopIndex];
		
		// If any node has a NaN value, that is an immediate disqualifier; I'm not sure how it happens, but it occasionally does
		if (isnan(x) || isnan(y))
			return -100000000;
		
		if (x < minx) minx = x;
		if (y > maxy) maxy = y;
		
		for (int sourceIndex = subpopIndex + 1; sourceIndex < subpopCount; ++sourceIndex)
		{
			if (connected[subpopIndex * subpopCount + sourceIndex])
			{
				double dx = x - center_x[sourceIndex];
				double dy = y - center_y[sourceIndex];
				double distanceSquared = dx * dx + dy * dy;
				double distance = sqrt(distanceSquared);
				
				meanEdge += distance;
				edgeCount++;
			}
		}
	}
	
	meanEdge /= edgeCount;
	
	// Add a little score if the first subpop is near the upper left
	if ((fabs(center_x[0] - minx) < 0.05) && (fabs(center_y[0] - maxy) < 0.05))
	{
		score += 0.01;
		
		// Add a little more score if the second subpop is to its right in roughly the same row
		if ((center_x[1] - center_x[0] > meanEdge/2) && (fabs(center_y[0] - center_y[1]) < 0.05))
			score += 0.01;
	}
	
	// Score distances and crossings
	for (int subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
	{
		double x = center_x[subpopIndex];
		double y = center_y[subpopIndex];
		
		for (int sourceIndex = subpopIndex + 1; sourceIndex < subpopCount; ++sourceIndex)
		{
			double dx = x - center_x[sourceIndex];
			double dy = y - center_y[sourceIndex];
			double distanceSquared = dx * dx + dy * dy;
			double distance = sqrt(distanceSquared);
			
			// being closer than k invokes a penalty
			if (distance < meanEdge)
				score -= (meanEdge - distance);
			
			// on the other hand, distance between connected subpops is very bad; this is above all what we want to optimize
			if (connected[subpopIndex * subpopCount + sourceIndex])
			{
				if (distance > meanEdge)
					score -= (distance - meanEdge);
				
				// Detect crossings
				for (int secondSubpop = subpopIndex + 1; secondSubpop < subpopCount; ++secondSubpop)
					for (int secondSource = secondSubpop + 1; secondSource < subpopCount; ++secondSource)
						if (connected[secondSubpop * subpopCount + secondSource])
						{
							double x0 = x, x1 = center_x[sourceIndex], x2 = center_x[secondSubpop], x3 = center_x[secondSource];
							double y0 = y, y1 = center_y[sourceIndex], y2 = center_y[secondSubpop], y3 = center_y[secondSource];
							
							// I test intersection with slightly shortened line segments, because I don't want endpoints that touch to be marked as intersections
							if (is_line_intersection(x0*0.99 + x1*0.01, y0*0.99 + y1*0.01,
													 x0*0.01 + x1*0.99, y0*0.01 + y1*0.99,
													 x2*0.99 + x3*0.01, y2*0.99 + y3*0.01,
													 x2*0.01 + x3*0.99, y2*0.01 + y3*0.99))
								score -= 100;
						}
			}
		}
	}
	
	return score;
}

#ifdef SLIM_WF_ONLY
// This is a simple implementation of the algorithm of Fruchterman and Reingold 1991;
// there are better algorithms out there, but this one is simple...
- (void)optimizeSubpopPositionsWithController:(SLiMWindowController *)controller
{
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	int subpopCount = (int)pop.subpops_.size();
	
	if (subpopCount == 0)
		return;
	
	double width = 0.58, length = 0.58;		// allows for the radii of the vertices at max subpop size
	double area = width * length;
	double k = sqrt(area / subpopCount);
	double kSquared = k * k;
	BOOL *connected;
	
	connected = (BOOL *)calloc(subpopCount * subpopCount, sizeof(BOOL));
	
	// We start by figuring out connectivity
	auto subpopIter = pop.subpops_.begin();
	
	for (int subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
	{
		Subpopulation *subpop = (*subpopIter).second;
		
		for (const std::pair<const slim_objectid_t,double> &fractions_pair : subpop->migrant_fractions_)
		{
			slim_objectid_t migrant_source_id = fractions_pair.first;
			
			// We need to get from the source ID to the index of the source subpop in the pop array
			auto sourceIter = pop.subpops_.begin();
			int sourceIndex;
			
			for (sourceIndex = 0; sourceIndex < subpopCount; ++sourceIndex)
			{
				if ((*sourceIter).first == migrant_source_id)
					break;
				
				++sourceIter;
			}
			
			if (sourceIndex == subpopCount)
			{
				free(connected);
				return;
			}
			
			// Mark the connection bidirectionally
			connected[subpopIndex * subpopCount + sourceIndex] = YES;
			connected[sourceIndex * subpopCount + subpopIndex] = YES;
		}
		
		++subpopIter;
	}
	
	double *pos_x, *pos_y;		// vertex positions
	double *disp_x, *disp_y;	// vertex forces/displacements
	double *best_x, *best_y;	// best vertex positions from multiple runs
	double best_score = -INFINITY;
	
	pos_x = (double *)malloc(sizeof(double) * subpopCount);
	pos_y = (double *)malloc(sizeof(double) * subpopCount);
	disp_x = (double *)malloc(sizeof(double) * subpopCount);
	disp_y = (double *)malloc(sizeof(double) * subpopCount);
	best_x = (double *)malloc(sizeof(double) * subpopCount);
	best_y = (double *)malloc(sizeof(double) * subpopCount);
	
	// We do multiple separate runs from different starting configurations, to try to find the optimal solution
	for (int trialIteration = 0; trialIteration < 50; ++trialIteration)
	{
		double temperature = width / 5.0;
		
		// initialize positions; this is basically the G := (V,E) step of the Fruchterman & Reingold algorithm
		for (int subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
		{
			pos_x[subpopIndex] = (random() / (double)INT32_MAX) * width - width/2;
			pos_y[subpopIndex] = (random() / (double)INT32_MAX) * length - length/2;
		}
		
		// Then we do the core loop of the Fruchterman & Reingold algorithm, which calculates forces and displacements
		for (int optimizeIteration = 1; optimizeIteration < 1000; ++optimizeIteration)
		{
			// Calculate repulsive forces
			for (int v = 0; v < subpopCount; ++v)
			{
				disp_x[v] = 0.0;
				disp_y[v] = 0.0;
				
				for (int u = 0; u < subpopCount; ++u)
				{
					if (u != v)
					{
						double delta_x = pos_x[v] - pos_x[u];
						double delta_y = pos_y[v] - pos_y[u];
						double delta_magnitude_squared = delta_x * delta_x + delta_y * delta_y;
						double multiplier = kSquared / delta_magnitude_squared;
						
						// This is a speed-optimized version of the pseudocode version commented out below
						disp_x[v] += delta_x * multiplier;
						disp_y[v] += delta_y * multiplier;
						
						//double delta_magnitude = sqrt(delta_magnitude_squared);
						
						//disp_x[v] += (delta_x / delta_magnitude) * (kSquared / delta_magnitude);
						//disp_y[v] += (delta_y / delta_magnitude) * (kSquared / delta_magnitude);
					}
				}
			}
			
			// Calculate attractive forces
			for (int v = 0; v < subpopCount; ++v)
			{
				for (int u = v + 1; u < subpopCount; ++u)
				{
					if (connected[v * subpopCount + u])
					{
						// There is an edge between u and v
						double delta_x = pos_x[v] - pos_x[u];
						double delta_y = pos_y[v] - pos_y[u];
						double delta_magnitude_squared = delta_x * delta_x + delta_y * delta_y;
						double delta_magnitude = sqrt(delta_magnitude_squared);
						double multiplier = (delta_magnitude_squared / k) / delta_magnitude;
						double delta_multiplier_x = delta_x * multiplier;
						double delta_multiplier_y = delta_y * multiplier;
						
						disp_x[v] -= delta_multiplier_x;
						disp_y[v] -= delta_multiplier_y;
						disp_x[u] += delta_multiplier_x;
						disp_y[u] += delta_multiplier_y;
					}
				}
			}
			
			// Limit max displacement to temperature t and prevent displacement outside frame
			for (int v = 0; v < subpopCount; ++v)
			{
				double delta_x = disp_x[v];
				double delta_y = disp_y[v];
				double delta_magnitude_squared = delta_x * delta_x + delta_y * delta_y;
				double delta_magnitude = sqrt(delta_magnitude_squared);
				
				if (delta_magnitude < temperature)
				{
					pos_x[v] += disp_x[v];
					pos_y[v] += disp_y[v];
				}
				else
				{
					pos_x[v] += (disp_x[v] / delta_magnitude) * temperature;
					pos_y[v] += (disp_y[v] / delta_magnitude) * temperature;
				}
				
				if (pos_x[v] < -width/2) pos_x[v] = -width/2;
				if (pos_y[v] < -length/2) pos_y[v] = -length/2;
				if (pos_x[v] > width/2) pos_x[v] = width/2;
				if (pos_y[v] > length/2) pos_y[v] = length/2;
			}
			
			// reduce the temperature as the layout approaches a better configuration
			// Fruchterman & Reingold are vague about exactly what they did here, but there is a rapid cooling phase (quenching)
			// and then a constant low-temperature phase (simmering); I've taken a guess at what that might look like
			temperature = temperature * 0.95;
			
			if (temperature < 0.002)
				temperature = 0.002;
		}
		
		// Test the final candidate and keep the best candidate
		double candidate_score = [self scorePositionsWithX:pos_x y:pos_y connected:connected count:subpopCount];
		
		if (candidate_score > best_score)
		{
			for (int v = 0; v < subpopCount; ++v)
			{
				best_x[v] = pos_x[v];
				best_y[v] = pos_y[v];
			}
			best_score = candidate_score;
			//NSLog(@"better candidate, new score == %f", best_score);
		}
	}
	
	// Finally, we set the positions we have arrived at back into the subpops
	subpopIter = pop.subpops_.begin();
	
	for (int subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
	{
		Subpopulation *subpop = (*subpopIter).second;
		
		subpop->gui_center_x_ = best_x[subpopIndex] + 0.5;
		subpop->gui_center_y_ = best_y[subpopIndex] + 0.5;
		subpop->gui_center_from_user_ = false;		// optimization overrides previously set display settings
		++subpopIter;
	}
	
	free(pos_x);
	free(pos_y);
	free(disp_x);
	free(disp_y);
	free(connected);
	free(best_x);
	free(best_y);
}
#endif	// SLIM_WF_ONLY

- (void)drawGraphInInteriorRect:(NSRect)interiorRect withController:(SLiMWindowController *)controller
{
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	int subpopCount = (int)pop.subpops_.size();
	
	if (subpopCount == 0)
	{
		[self drawMessage:@"no subpopulations" inRect:interiorRect];
		return;
	}
	
	// First, we transform our coordinate system so that a square of size (1,1) fits maximally and centered
	[NSGraphicsContext saveGraphicsState];
	
	NSAffineTransform *transform = [NSAffineTransform transform];
	if (interiorRect.size.width > interiorRect.size.height)
	{
		[transform translateXBy:interiorRect.origin.x yBy:interiorRect.origin.y];
		[transform translateXBy:SLIM_SCREEN_ROUND((interiorRect.size.width - interiorRect.size.height) / 2) yBy:0];
		[transform scaleBy:interiorRect.size.height];
	}
	else
	{
		[transform translateXBy:interiorRect.origin.x yBy:interiorRect.origin.y];
		[transform translateXBy:0 yBy:SLIM_SCREEN_ROUND((interiorRect.size.height - interiorRect.size.width) / 2)];
		[transform scaleBy:interiorRect.size.width];
	}
	[transform concat];
	
	// test frame
	//[[NSColor blackColor] set];
	//NSFrameRectWithWidth(NSMakeRect(0, 0, 1, 1), 0.002);
	
	if (subpopCount == 1)
	{
		auto subpopIter = pop.subpops_.begin();
		
		// a single subpop is shown as a circle at the center
		[self drawSubpop:(*subpopIter).second withID:(*subpopIter).first centeredAt:NSMakePoint(0.5, 0.5) controller:controller];
	}
	else if (subpopCount > 1)
	{
		// first we distribute our subpops in a ring
		BOOL allUserConfigured = true;
		
		{
			auto subpopIter = pop.subpops_.begin();
			
			for (int subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
			{
				Subpopulation *subpop = (*subpopIter).second;
				double theta = (M_PI * 2.0 / subpopCount) * subpopIndex + M_PI_2;
				
				if (!subpop->gui_center_from_user_)
				{
					subpop->gui_center_x_ = 0.5 - cos(theta) * 0.29;
					subpop->gui_center_y_ = 0.5 + sin(theta) * 0.29;
					allUserConfigured = false;
				}
				++subpopIter;
			}
		}
		
		// if position optimization is on, we do that to optimize the positions of the subpops
#ifdef SLIM_WF_ONLY
		if ((sim->ModelType() == SLiMModelType::kModelTypeWF) && _optimizePositions && (subpopCount > 2))
			[self optimizeSubpopPositionsWithController:controller];
#endif	// SLIM_WF_ONLY
		
		if (!allUserConfigured)
		{
			// then do some sizing, to figure out the maximum extent of our subpops
			NSRect boundingBox = NSZeroRect;
			
			{
				auto subpopIter = pop.subpops_.begin();
				
				for (int subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
				{
					Subpopulation *subpop = (*subpopIter).second;
					
					NSPoint center = NSMakePoint(subpop->gui_center_x_, subpop->gui_center_y_);
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
		}
		
		// then we draw the subpops
		{
			auto subpopIter = pop.subpops_.begin();
			
			for (int subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
			{
				Subpopulation *subpop = (*subpopIter).second;
				slim_objectid_t subpopID = (*subpopIter).first;
				NSPoint center = NSMakePoint(subpop->gui_center_x_, subpop->gui_center_y_);
				
				[self drawSubpop:subpop withID:subpopID centeredAt:center controller:controller];
				++subpopIter;
			}
		}
		
		// in the multipop case, we need to draw migration arrows, too
#if (defined(SLIM_WF_ONLY) && defined(SLIM_NONWF_ONLY))
		{
			for (auto destSubpopIter = pop.subpops_.begin(); destSubpopIter != pop.subpops_.end(); ++destSubpopIter)
			{
				Subpopulation *destSubpop = (*destSubpopIter).second;
				std::map<slim_objectid_t,double> &destMigrants = (sim->ModelType() == SLiMModelType::kModelTypeWF) ? destSubpop->migrant_fractions_ : destSubpop->gui_migrants_;
				
				for (auto sourceSubpopIter = destMigrants.begin(); sourceSubpopIter != destMigrants.end(); ++sourceSubpopIter)
				{
					slim_objectid_t sourceSubpopID = (*sourceSubpopIter).first;
					auto sourceSubpopPair = pop.subpops_.find(sourceSubpopID);
					
					if (sourceSubpopPair != pop.subpops_.end())
					{
						Subpopulation *sourceSubpop = sourceSubpopPair->second;
						double migrantFraction = (*sourceSubpopIter).second;
						
						// The gui_migrants_ map is raw migration counts, which need to be converted to a fraction of the sourceSubpop pre-migration size
						if (sim->ModelType() == SLiMModelType::kModelTypeNonWF)
						{
							if (sourceSubpop->gui_premigration_size_ <= 0)
								continue;
							
							migrantFraction /= sourceSubpop->gui_premigration_size_;
							
							if (migrantFraction < 0.0)
								migrantFraction = 0.0;
							if (migrantFraction > 1.0)
								migrantFraction = 1.0;
						}
						
						[self drawArrowFromSubpop:sourceSubpop toSubpop:destSubpop migrantFraction:migrantFraction];
					}
				}
			}
		}
#endif
	}
	
	// We're done with our transformed coordinate system
	[NSGraphicsContext restoreGraphicsState];
}

- (IBAction)toggleOptimizedPositions:(id)sender
{
	[self setOptimizePositions:![self optimizePositions]];
	[self setNeedsDisplay:YES];
}

- (void)subclassAddItemsToMenu:(NSMenu *)menu forEvent:(NSEvent *)theEvent
{
	if (menu)
	{
		NSMenuItem *menuItem = [menu addItemWithTitle:([self optimizePositions] ? @"Standard Positions" : @"Optimized Positions") action:@selector(toggleOptimizedPositions:) keyEquivalent:@""];
		
		[menuItem setTarget:self];
	}
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
	SEL sel = [menuItem action];
	SLiMWindowController *controller = [self slimWindowController];
	
	if (!controller)
		return NO;
	
	SLiMSim *sim = controller->sim;
	Population &pop = sim->population_;
	
	if (sel == @selector(toggleOptimizedPositions:))
	{
		// If any subpop has a user-defined center, disable position optimization; it doesn't know how to
		// handle those, and there's no way to revert back after it messes things up, and so forth
		for (auto subpopIter = pop.subpops_.begin(); subpopIter != pop.subpops_.end(); ++subpopIter)
			if (((*subpopIter).second)->gui_center_from_user_)
				return NO;
	}
	
	return YES;
}

@end





























































