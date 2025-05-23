//
//  QtSLiMGraphView_SubpopFitnessDists.cpp
//  SLiM
//
//  Created by Ben Haller on 8/8/2020.
//  Copyright (c) 2020-2025 Benjamin C. Haller.  All rights reserved.
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

#include "QtSLiMGraphView_SubpopFitnessDists.h"

#include "QtSLiMWindow.h"
#include "species.h"
#include "population.h"
#include "subpopulation.h"
#include "individual.h"

#include <utility>
#include <string>

#include <QPainterPath>

QtSLiMGraphView_SubpopFitnessDists::QtSLiMGraphView_SubpopFitnessDists(QWidget *p_parent, QtSLiMWindow *controller) : QtSLiMGraphView(p_parent, controller)
{
    histogramBinCount_ = 50;
    allowBinCountRescale_ = true;
    
    x1_ = 2.0;
    
    xAxisMax_ = x1_;
    xAxisMajorTickInterval_ = 1.0;
    xAxisMinorTickInterval_ = 0.2;
    xAxisMajorTickModulus_ = 5;
    xAxisTickValuePrecision_ = 1;
    
    xAxisLabel_ = "Fitness (rescaled)";
    yAxisLabel_ = "Frequency";
    
    allowXAxisUserRescale_ = true;
    allowYAxisUserRescale_ = true;
    
    showHorizontalGridLines_ = true;
}

QtSLiMGraphView_SubpopFitnessDists::~QtSLiMGraphView_SubpopFitnessDists()
{
}

QString QtSLiMGraphView_SubpopFitnessDists::graphTitle(void)
{
    return "Subpopulation Fitness Distributions";
}

QString QtSLiMGraphView_SubpopFitnessDists::aboutString(void)
{
    return "The Subpopulation Fitness Distributions graph shows the distribution of fitness "
           "values for each subpopulation as a separate line.  The primary purpose of this "
           "visualization is to allow the fitness distributions of many subpopulations "
           "to be compared visually.  Fitness is 'rescaled' as explained in the "
           "Fitness ~ Time graph's about info.  The number of histogram bins can be changed "
           "in the action menu.  The Population Fitness Distribution graph provides an "
           "alternative that might also be useful.";
}

double *QtSLiMGraphView_SubpopFitnessDists::subpopulationFitnessData(const Subpopulation *requestedSubpop)
{
    int binCount = histogramBinCount_;
	static double *bins = nullptr;
	static size_t allocedBins = 0;
	
	if (!bins || (allocedBins < (size_t)binCount))
	{
		allocedBins = binCount;
		bins = static_cast<double *>(realloc(bins, allocedBins * sizeof(double)));
	}
	
	for (int i = 0; i < binCount; ++i)
		bins[i] = 0.0;
	
    // bin fitness values from one subpop or from across the population
    Species *graphSpecies = focalDisplaySpecies();
    Population &pop = graphSpecies->population_;
    
    for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : pop.subpops_)
    {
        const Subpopulation *subpop = subpop_pair.second;
        
        if (requestedSubpop && (subpop != requestedSubpop))
            continue;
        
        for (const Individual *individual : subpop->parent_individuals_)
        {
            double fitness = individual->cached_unscaled_fitness_;
            int bin = (int)(((fitness - xAxisMin_) / (xAxisMax_ - xAxisMin_)) * binCount);
            if (bin < 0) bin = 0;
            if (bin >= binCount) bin = binCount - 1;
            
            bins[bin]++;
        }
    }
    
    // normalize the frequencies to a total of 1.0
    double totalCount = 0.0;
    
    for (int i = 0; i < binCount; ++i)
        totalCount += bins[i];
    
    if (totalCount == 0)
        totalCount = 1;  // if counts are all zero, avoid a divide by zero below and just end up with 0 instead
    
    for (int i = 0; i < binCount; ++i)
        bins[i] = bins[i] / totalCount;
    
    return bins;
}

void QtSLiMGraphView_SubpopFitnessDists::drawGraph(QPainter &painter, QRect interiorRect)
{
    Species *graphSpecies = focalDisplaySpecies();
	Population &pop = graphSpecies->population_;
    bool showSubpops = true;
	bool drawSubpopsGray = (showSubpops && (pop.subpops_.size() > 8));	// 7 subpops + pop
    int binCount = histogramBinCount_;
    
    // First draw subpop fitness distributions
	if (showSubpops)
	{
        for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : pop.subpops_)
        {
            const Subpopulation *subpop = subpop_pair.second;
            double *plotData = subpopulationFitnessData(subpop);
            QPainterPath linePath;
            bool startedLine = false;
            
            for (int i = 0; i < binCount; ++i)
            {
                QPointF historyPoint(plotToDeviceX(xAxisMin_ + (xAxisMax_ - xAxisMin_) * (i + 0.5) / binCount, interiorRect), plotToDeviceY(plotData[i], interiorRect));
                
                if (startedLine)    linePath.lineTo(historyPoint);
                else                linePath.moveTo(historyPoint);
                
                startedLine = true;
            }
            
            if (drawSubpopsGray)
                painter.strokePath(linePath, QPen(QtSLiMColorWithWhite(0.5, 1.0), 1.0));
            else
                painter.strokePath(linePath, QPen(controller_->whiteContrastingColorForIndex(subpop->subpopulation_id_), 1.0));
        }
	}
	
	// Then draw the population fitness distribution
    double *plotData = subpopulationFitnessData(nullptr);
    QPainterPath linePath;
    bool startedLine = false;
    
    for (int i = 0; i < binCount; ++i)
    {
        QPointF historyPoint(plotToDeviceX(xAxisMin_ + (xAxisMax_ - xAxisMin_) * (i + 0.5) / binCount, interiorRect), plotToDeviceY(plotData[i], interiorRect));
        
        if (startedLine)    linePath.lineTo(historyPoint);
        else                linePath.moveTo(historyPoint);
        
        startedLine = true;
    }
    
    painter.strokePath(linePath, QPen(Qt::black, 1.5));
}

bool QtSLiMGraphView_SubpopFitnessDists::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_SubpopFitnessDists::appendStringForData(QString &string)
{
    Species *graphSpecies = focalDisplaySpecies();
	Population &pop = graphSpecies->population_;
    bool showSubpops = true;
    int binCount = histogramBinCount_;
	
    // First add subpop fitness distributions
	if (showSubpops)
	{
        for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : pop.subpops_)
        {
            const Subpopulation *subpop = subpop_pair.second;
            double *plotData = subpopulationFitnessData(subpop);
            
            string.append(QString("# Fitness distribution (subpopulation p%1):\n").arg(subpop->subpopulation_id_));
            
            for (int i = 0; i < binCount; ++i)
                string.append(QString("%1, ").arg(plotData[i], 0, 'f', 4));
                
            string.append("\n\n");
        }
	}
	
	// Then add the population fitness distribution
    double *plotData = subpopulationFitnessData(nullptr);
    
    string.append("# Fitness distribution (population):\n");
    
    for (int i = 0; i < binCount; ++i)
        string.append(QString("%1, ").arg(plotData[i], 0, 'f', 4));
        
    string.append("\n");
}

QtSLiMLegendSpec QtSLiMGraphView_SubpopFitnessDists::legendKey(void)
{
    Species *graphSpecies = focalDisplaySpecies();
    Population &pop = graphSpecies->population_;
	bool showSubpops = true;
	bool drawSubpopsGray = (showSubpops && (pop.subpops_.size() > 8));	// 7 subpops + pop
	
	if (!showSubpops)
		return QtSLiMLegendSpec();
	
    QtSLiMLegendSpec legend_key;

    legend_key.emplace_back("All", Qt::black);
	
	if (drawSubpopsGray)
	{
        legend_key.emplace_back("pX", QtSLiMColorWithWhite(0.5, 1.0));
	}
	else
	{
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : pop.subpops_)
		{
            slim_objectid_t subpop_id = subpop_pair.second->subpopulation_id_;
            QString labelString = QString("p%1").arg(subpop_id);
            
            legend_key.emplace_back(labelString, controller_->whiteContrastingColorForIndex(subpop_id));
        }
	}
	
	return legend_key;
}





























