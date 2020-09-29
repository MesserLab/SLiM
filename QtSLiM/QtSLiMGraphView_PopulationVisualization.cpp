//
//  QtSLiMGraphView_PopulationVisualization.cpp
//  SLiM
//
//  Created by Ben Haller on 3/30/2020.
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

#include "QtSLiMGraphView_PopulationVisualization.h"

#include <QAction>
#include <QMenu>
#include <QPainterPath>
#include <QDebug>

#include "QtSLiMWindow.h"
#include "subpopulation.h"


QtSLiMGraphView_PopulationVisualization::QtSLiMGraphView_PopulationVisualization(QWidget *parent, QtSLiMWindow *controller) : QtSLiMGraphView(parent, controller)
{
    showXAxis_ = false;
    showYAxis_ = false;
}

QtSLiMGraphView_PopulationVisualization::~QtSLiMGraphView_PopulationVisualization()
{
}

QString QtSLiMGraphView_PopulationVisualization::graphTitle(void)
{
    return "Population Visualization";
}

QString QtSLiMGraphView_PopulationVisualization::aboutString(void)
{
    return "The Population Visualization graph shows a visual depiction of the population structure of "
           "the model, at the current generation.  Each subpopulation is shown as a circle, with size "
           "proportional to the number of individuals in the subpopulation, and color representing the "
           "mean fitness of the subpopulation.  Arrows show migration between subpopulations, with "
           "the thickness of arrows representing the magnitude of migration.";
}

QRectF QtSLiMGraphView_PopulationVisualization::rectForSubpop(Subpopulation *subpop, QPointF center)
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
	
	return QRectF(center.x() - subpopRadius, center.y() - subpopRadius, 2 * subpopRadius, 2 * subpopRadius);
}

void QtSLiMGraphView_PopulationVisualization::drawSubpop(QPainter &painter, Subpopulation *subpop, slim_objectid_t subpopID, QPointF center)
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
		double fitnessScalingFactor = 0.8; // controller->fitnessColorScale;
		double totalFitness = subpop->parental_total_fitness_;
		double subpopFitnessScaling = subpop->last_fitness_scaling_;
		
		if ((subpopFitnessScaling <= 0.0) || !std::isfinite(subpopFitnessScaling))
			subpopFitnessScaling = 1.0;
		
		// we normalize fitness values with subpopFitnessScaling so individual fitness, unscaled by subpopulation fitness, is used for coloring
		double fitness = ((subpopSize == 0) ? -10000.0 : (totalFitness / subpopFitnessScaling) / subpopSize);
		RGBForFitness(fitness, &colorRed, &colorGreen, &colorBlue, fitnessScalingFactor);
	}
	
    QColor color = QtSLiMColorWithRGB(static_cast<double>(colorRed), static_cast<double>(colorGreen), static_cast<double>(colorBlue), 1.0);
	
	// draw the circle
    painter.setBrush(color);
    painter.setPen(QPen(Qt::black, 0.002));
    painter.drawEllipse(center, subpopRadius, subpopRadius);
    
	// label it with the subpopulation ID
    painter.setWorldMatrixEnabled(false);
    
	QString popString = QString("p%1").arg(subpopID);
	double brightness = static_cast<double>(0.299f * colorRed + 0.587f * colorGreen + 0.114f * colorBlue);
	double scalingFromUser = (subpop->gui_radius_scaling_from_user_ ? subpop->gui_radius_scaling_ : 1.0);
	
    painter.setFont(labelFontOfPointSize(0.04 * scalingFactor_ * scalingFromUser));
    painter.setPen((brightness > 0.5) ? Qt::black : Qt::white);
    painter.setBrush(Qt::NoBrush);
	
    QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, popString);
    QPointF drawPoint = painter.transform().map(center);
    drawPoint.setX(drawPoint.x() - labelBoundingRect.width() / 2.0 + 1.0);
    drawPoint.setY(drawPoint.y() + 0.008 * scalingFactor_ * scalingFromUser);
    
    painter.drawText(drawPoint, popString);
    painter.setWorldMatrixEnabled(true);
}

void QtSLiMGraphView_PopulationVisualization::drawArrowFromSubpopToSubpop(QPainter &painter, Subpopulation *sourceSubpop, Subpopulation *destSubpop, double migrantFraction)
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
	double finalShiftMagnitude = std::max(lineWidth * 0.75, 0.010);
	double finalShiftX = perpendicularFromSourceDX * finalShiftMagnitude / partVecLength;
	double finalShiftY = perpendicularFromSourceDY * finalShiftMagnitude / partVecLength;
	double arrowheadSize = std::max(lineWidth * 1.5, 0.008);
	
	// we have to use a clipping path to cut back the destination end of the vector, to make room for the arrowhead
    painter.save();
	
	double clipRadius = vectorLength - (destSubpop->gui_radius_ + arrowheadSize + 0.01);
	QRectF clipCircle = QRectF(sourceCenterX - clipRadius, sourceCenterY - clipRadius, clipRadius * 2, clipRadius * 2);
    QPainterPath clipBezier;
    
    clipBezier.addEllipse(clipCircle);
    painter.setClipPath(clipBezier, Qt::IntersectClip);
    
	// now draw the curved line connecting the subpops
    QPainterPath bezierLines;
	double shiftedSourceEndX = sourceEndX + finalShiftX, shiftedSourceEndY = sourceEndY + finalShiftY;
	double shiftedDestEndX = destEndX + finalShiftX, shiftedDestEndY = destEndY + finalShiftY;
	double shiftedControl1X = controlPoint1X + finalShiftX, shiftedControl1Y = controlPoint1Y + finalShiftY;
	double shiftedControl2X = controlPoint2X + finalShiftX, shiftedControl2Y = controlPoint2Y + finalShiftY;
	
    bezierLines.moveTo(QPointF(shiftedSourceEndX, shiftedSourceEndY));
    bezierLines.cubicTo(QPointF(shiftedControl1X, shiftedControl1Y), QPointF(shiftedControl2X, shiftedControl2Y), QPointF(shiftedDestEndX, shiftedDestEndY));
    
    painter.strokePath(bezierLines, QPen(Qt::black, lineWidth));
	
	// restore the clipping path
    painter.restore();
	
	// draw the arrowhead; this is oriented along the line from (shiftedDestEndX, shiftedDestEndY) to (shiftedControl2X, shiftedControl2Y),
	// of length partVecLength, and is calculated using a perpendicular off of that vector
    QPainterPath bezierArrowheads;
	double angleCorrectionFactor = (arrowheadSize / partVecLength) * 0.5;	// large arrowheads need to be tilted closer to the source-dest pop line
	double headInsideX = shiftedControl2X * (1 - angleCorrectionFactor) + shiftedSourceEndX * angleCorrectionFactor;
	double headInsideY = shiftedControl2Y * (1 - angleCorrectionFactor) + shiftedSourceEndY * angleCorrectionFactor;
	double headMidlineDX = headInsideX - shiftedDestEndX, headMidlineDY = headInsideY - shiftedDestEndY;
	double headMidlineLength = sqrt(headMidlineDX * headMidlineDX + headMidlineDY * headMidlineDY);
	double headMidlineNormDX = (headMidlineDX / headMidlineLength) * arrowheadSize;					// normalized to have length arrowheadSize
	double headMidlineNormDY = (headMidlineDY / headMidlineLength) * arrowheadSize;
	double headPerpendicular1DX = headMidlineNormDY, headPerpendicular1DY = -headMidlineNormDX;		// perpendicular to the normalized midline
	double headPerpendicular2DX = -headMidlineNormDY, headPerpendicular2DY = headMidlineNormDX;		// just the negation of perpendicular 1
	
    bezierArrowheads.moveTo(shiftedDestEndX, shiftedDestEndY);
    bezierArrowheads.lineTo(shiftedDestEndX + headMidlineNormDX * 1.75 + headPerpendicular1DX * 0.9, shiftedDestEndY + headMidlineNormDY * 1.75 + headPerpendicular1DY * 0.9);
    bezierArrowheads.lineTo(shiftedDestEndX + headMidlineNormDX * 1.2, shiftedDestEndY + headMidlineNormDY * 1.2);
    bezierArrowheads.lineTo(shiftedDestEndX + headMidlineNormDX * 1.75 + headPerpendicular2DX * 0.9, shiftedDestEndY + headMidlineNormDY * 1.75 + headPerpendicular2DY * 0.9);
    bezierArrowheads.closeSubpath();
    
    painter.fillPath(bezierArrowheads, Qt::black);
}

static bool is_line_intersection(double p0_x, double p0_y, double p1_x, double p1_y, double p2_x, double p2_y, double p3_x, double p3_y)
{
	double s02_x, s02_y, s10_x, s10_y, s32_x, s32_y, s_numer, t_numer, denom;
	s10_x = p1_x - p0_x;
	s10_y = p1_y - p0_y;
	s32_x = p3_x - p2_x;
	s32_y = p3_y - p2_y;
	
	denom = s10_x * s32_y - s32_x * s10_y;
	if (fabs(denom - 0) < 0.00001)
		return false; // Collinear
	bool denomPositive = denom > 0;
	
	s02_x = p0_x - p2_x;
	s02_y = p0_y - p2_y;
	s_numer = s10_x * s02_y - s10_y * s02_x;
	if ((s_numer < 0) == denomPositive)
		return false; // No collision
	
	t_numer = s32_x * s02_y - s32_y * s02_x;
	if ((t_numer < 0) == denomPositive)
		return false; // No collision
	
	if (((s_numer > denom) == denomPositive) || ((t_numer > denom) == denomPositive))
		return false; // No collision
	
	return true;
}

double QtSLiMGraphView_PopulationVisualization::scorePositions(double *center_x, double *center_y, bool *connected, size_t subpopCount)
{
	double score = 0.0;
	double meanEdge = 0.0;
	int edgeCount = 0;
	double minx = std::numeric_limits<double>::infinity(), maxy = -std::numeric_limits<double>::infinity();
	
	// First we calculate the mean edge length; we will consider this the optimum length
	for (size_t subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
	{
		double x = center_x[subpopIndex];
		double y = center_y[subpopIndex];
		
		// If any node has a NaN value, that is an immediate disqualifier; I'm not sure how it happens, but it occasionally does
		if (std::isnan(x) || std::isnan(y))
			return -100000000;
		
		if (x < minx) minx = x;
		if (y > maxy) maxy = y;
		
		for (size_t sourceIndex = subpopIndex + 1; sourceIndex < subpopCount; ++sourceIndex)
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
	for (size_t subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
	{
		double x = center_x[subpopIndex];
		double y = center_y[subpopIndex];
		
		for (size_t sourceIndex = subpopIndex + 1; sourceIndex < subpopCount; ++sourceIndex)
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
				for (size_t secondSubpop = subpopIndex + 1; secondSubpop < subpopCount; ++secondSubpop)
					for (size_t secondSource = secondSubpop + 1; secondSource < subpopCount; ++secondSource)
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
void QtSLiMGraphView_PopulationVisualization::optimizePositions(void)
{
    SLiMSim *sim = controller_->sim;
	Population &pop = sim->population_;
	size_t subpopCount = pop.subpops_.size();
	
	if (subpopCount == 0)
		return;
	
	double width = 0.58, length = 0.58;		// allows for the radii of the vertices at max subpop size
	double area = width * length;
	double k = sqrt(area / subpopCount);
	double kSquared = k * k;
	bool *connected;
	
	connected = static_cast<bool *>(calloc(subpopCount * subpopCount, sizeof(bool)));
	
	// We start by figuring out connectivity
	auto subpopIter = pop.subpops_.begin();
	
	for (size_t subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
	{
		Subpopulation *subpop = (*subpopIter).second;
		
		for (const std::pair<const slim_objectid_t,double> &fractions_pair : subpop->migrant_fractions_)
		{
			slim_objectid_t migrant_source_id = fractions_pair.first;
			
			// We need to get from the source ID to the index of the source subpop in the pop array
			auto sourceIter = pop.subpops_.begin();
			size_t sourceIndex;
			
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
			connected[subpopIndex * subpopCount + sourceIndex] = true;
			connected[sourceIndex * subpopCount + subpopIndex] = true;
		}
		
		++subpopIter;
	}
	
	double *pos_x, *pos_y;		// vertex positions
	double *disp_x, *disp_y;	// vertex forces/displacements
	double *best_x, *best_y;	// best vertex positions from multiple runs
	double best_score = -std::numeric_limits<double>::infinity();
	
	pos_x = static_cast<double *>(malloc(sizeof(double) * subpopCount));
	pos_y = static_cast<double *>(malloc(sizeof(double) * subpopCount));
	disp_x = static_cast<double *>(malloc(sizeof(double) * subpopCount));
	disp_y = static_cast<double *>(malloc(sizeof(double) * subpopCount));
	best_x = static_cast<double *>(malloc(sizeof(double) * subpopCount));
	best_y = static_cast<double *>(malloc(sizeof(double) * subpopCount));
	
	// We do multiple separate runs from different starting configurations, to try to find the optimal solution
	for (int trialIteration = 0; trialIteration < 50; ++trialIteration)
	{
		double temperature = width / 5.0;
		
		// initialize positions; this is basically the G := (V,E) step of the Fruchterman & Reingold algorithm
		for (size_t subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
		{
			pos_x[subpopIndex] = (random() / static_cast<double>(INT32_MAX)) * width - width/2;
			pos_y[subpopIndex] = (random() / static_cast<double>(INT32_MAX)) * length - length/2;
		}
		
		// Then we do the core loop of the Fruchterman & Reingold algorithm, which calculates forces and displacements
		for (int optimizeIteration = 1; optimizeIteration < 1000; ++optimizeIteration)
		{
			// Calculate repulsive forces
			for (size_t v = 0; v < subpopCount; ++v)
			{
				disp_x[v] = 0.0;
				disp_y[v] = 0.0;
				
				for (size_t u = 0; u < subpopCount; ++u)
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
			for (size_t v = 0; v < subpopCount; ++v)
			{
				for (size_t u = v + 1; u < subpopCount; ++u)
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
			for (size_t v = 0; v < subpopCount; ++v)
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
		double candidate_score = scorePositions(pos_x, pos_y, connected, subpopCount);
		
		if (candidate_score > best_score)
		{
			for (size_t v = 0; v < subpopCount; ++v)
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
	
	for (size_t subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
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

void QtSLiMGraphView_PopulationVisualization::drawGraph(QPainter &painter, QRect interiorRect)
{
    SLiMSim *sim = controller_->sim;
	Population &pop = sim->population_;
	int subpopCount = static_cast<int>(pop.subpops_.size());
	
	if (subpopCount == 0)
	{
        // this is an ugly hack that assumes things about QtSLiMGraphView's implementation
        // we restore() twice to get back to the original coordinate system for drawMessage()
        // then we save() twice so that the expected number of pops are still available
        painter.restore();
        painter.restore();
		drawMessage(painter, "no subpopulations", rect());
        painter.save();
        painter.save();
		return;
	}
	
	// First, we transform our coordinate system so that a square of size (1,1) fits maximally and centered
	painter.save();
    
    QTransform transform;
    if (interiorRect.width() > interiorRect.height())
    {
        transform.translate(interiorRect.x(), interiorRect.y());
        transform.translate(SLIM_SCREEN_ROUND((interiorRect.width() - interiorRect.height()) / 2.0), 0);
        scalingFactor_ = interiorRect.height();
    }
    else
    {
        transform.translate(interiorRect.x(), interiorRect.y());
        transform.translate(0, SLIM_SCREEN_ROUND((interiorRect.height() - interiorRect.width()) / 2.0));
        scalingFactor_ = interiorRect.width();
    }
    transform.scale(scalingFactor_, scalingFactor_);
    painter.setWorldTransform(transform, true);
    
	// test frame
    //painter.setBrush(Qt::NoBrush);
    //painter.setPen(QPen(Qt::black, 0.002));
    //painter.drawRect(QRect(0, 0, 1, 1));
	
	if (subpopCount == 1)
	{
		auto subpopIter = pop.subpops_.begin();
		
		// a single subpop is shown as a circle at the center
		drawSubpop(painter, (*subpopIter).second, (*subpopIter).first, QPointF(0.5, 0.5));
	}
	else if (subpopCount > 1)
	{
		// first we distribute our subpops in a ring
		bool allUserConfigured = true;
		
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
		if ((sim->ModelType() == SLiMModelType::kModelTypeWF) && optimizePositions_ && (subpopCount > 2))
			optimizePositions();
#endif	// SLIM_WF_ONLY
		
		if (!allUserConfigured)
		{
			// then do some sizing, to figure out the maximum extent of our subpops
			QRectF boundingBox = QRectF();
			
			{
				auto subpopIter = pop.subpops_.begin();
				
				for (int subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
				{
					Subpopulation *subpop = (*subpopIter).second;
					
					QPointF center(subpop->gui_center_x_, subpop->gui_center_y_);
					QRectF subpopRect = rectForSubpop(subpop, center);
					
					boundingBox = ((subpopIndex == 0) ? subpopRect : boundingBox.united(subpopRect));
					
					++subpopIter;
				}
			}
			
			// then we translate our coordinate system so that the subpops are centered within our (0, 0, 1, 1) box
			double offsetX = ((1.0 - boundingBox.width()) / 2.0) - boundingBox.x();
			double offsetY = ((1.0 - boundingBox.height()) / 2.0) - boundingBox.y();
			
            QTransform offsetTransform;
            offsetTransform.translate(offsetX, offsetY);
            painter.setWorldTransform(offsetTransform, true);
		}
		
		// then we draw the subpops
		{
			auto subpopIter = pop.subpops_.begin();
			
			for (int subpopIndex = 0; subpopIndex < subpopCount; ++subpopIndex)
			{
				Subpopulation *subpop = (*subpopIter).second;
				slim_objectid_t subpopID = (*subpopIter).first;
				QPointF center(subpop->gui_center_x_, subpop->gui_center_y_);
				
				drawSubpop(painter, subpop, subpopID, center);
				++subpopIter;
			}
		}
		
		// in the multipop case, we need to draw migration arrows, too
#if (defined(SLIM_WF_ONLY) && defined(SLIM_NONWF_ONLY))
		{
			for (auto destSubpopIter : pop.subpops_)
			{
				Subpopulation *destSubpop = destSubpopIter.second;
				std::map<slim_objectid_t,double> &destMigrants = (sim->ModelType() == SLiMModelType::kModelTypeWF) ? destSubpop->migrant_fractions_ : destSubpop->gui_migrants_;
				
				for (auto sourceSubpopIter : destMigrants)
				{
					slim_objectid_t sourceSubpopID = sourceSubpopIter.first;
                    Subpopulation *sourceSubpop = sim->SubpopulationWithID(sourceSubpopID);
					
					if (sourceSubpop)
					{
						double migrantFraction = sourceSubpopIter.second;
						
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
						
						drawArrowFromSubpopToSubpop(painter, sourceSubpop, destSubpop, migrantFraction);
					}
				}
			}
		}
#endif
	}
	
	// We're done with our transformed coordinate system
    painter.restore();
}

void QtSLiMGraphView_PopulationVisualization::toggleOptimizedPositions(void)
{
    optimizePositions_ = !optimizePositions_;
    update();
}

void QtSLiMGraphView_PopulationVisualization::subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent * /* event */)
{
    QAction *menuItem = contextMenu.addAction(optimizePositions_ ? "Standard Positions" : "Optimized Positions", this, &QtSLiMGraphView_PopulationVisualization::toggleOptimizedPositions);
    SLiMSim *sim = controller_->sim;
    Population &pop = sim->population_;
    
    // If any subpop has a user-defined center, disable position optimization; it doesn't know how to
    // handle those, and there's no way to revert back after it messes things up, and so forth
    for (auto subpopIter : pop.subpops_)
        if (subpopIter.second->gui_center_from_user_)
        {
            menuItem->setEnabled(false);
            return;
        }
    
    menuItem->setEnabled(true);
}

void QtSLiMGraphView_PopulationVisualization::appendStringForData(QString & /* string */)
{
    // No data string
}





























