//
//  QtSLiMGraphView_PopFitnessDist.cpp
//  SLiM
//
//  Created by Ben Haller on 8/3/2020.
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

#include "QtSLiMGraphView_PopFitnessDist.h"

#include "QtSLiMWindow.h"
#include "slim_sim.h"
#include "population.h"
#include "subpopulation.h"
#include "individual.h"


QtSLiMGraphView_PopFitnessDist::QtSLiMGraphView_PopFitnessDist(QWidget *parent, QtSLiMWindow *controller) : QtSLiMGraphView(parent, controller)
{
    histogramBinCount_ = 50;
    allowBinCountRescale_ = true;
    
    xAxisMax_ = 2.0;
    xAxisMajorTickInterval_ = 1.0;
    xAxisMinorTickInterval_ = 0.2;
    xAxisMajorTickModulus_ = 5;
    xAxisTickValuePrecision_ = 1;
    
    xAxisLabel_ = "Fitness (rescaled absolute)";
    yAxisLabel_ = "Frequency";
    
    allowXAxisUserRescale_ = true;
    allowYAxisUserRescale_ = true;
    
    showHorizontalGridLines_ = true;
}

QtSLiMGraphView_PopFitnessDist::~QtSLiMGraphView_PopFitnessDist()
{
}

QString QtSLiMGraphView_PopFitnessDist::graphTitle(void)
{
    return "Population Fitness Distribution";
}

QString QtSLiMGraphView_PopFitnessDist::aboutString(void)
{
    return "The Population Fitness Distribution graph shows the distribution of fitness "
           "values across all individuals in the population, as a histogram.  Fitness "
           "is 'rescaled absolute' as explained in the Fitness ~ Time graph's about "
           "info.  The number of histogram bins can be changed in the action menu.  The "
           "Subpopulation Fitness Distributions graph provides an alternative that "
           "might also be useful.";
}

double *QtSLiMGraphView_PopFitnessDist::populationFitnessData(void)
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
	
    // bin fitness values from across the population
    SLiMSim *sim = controller_->sim;
    Population &pop = sim->population_;
    
    for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : pop.subpops_)
    {
        const Subpopulation *subpop = subpop_pair.second;
        double subpopFitnessScaling = subpop->last_fitness_scaling_;
        if ((subpopFitnessScaling <= 0.0) || !std::isfinite(subpopFitnessScaling))
            subpopFitnessScaling = 1.0;
        
        for (const Individual *individual : subpop->parent_individuals_)
        {
            double fitness = individual->cached_fitness_UNSAFE_;    // always valid in SLiMgui
            double rescaledFitness = fitness / subpopFitnessScaling;
            int bin = (int)(((rescaledFitness - xAxisMin_) / (xAxisMax_ - xAxisMin_)) * binCount);
            if (bin < 0) bin = 0;
            if (bin >= binCount) bin = binCount - 1;
            
            bins[bin]++;
        }
    }
    
    // normalize the frequencies to a total of 1.0
    double totalCount = 0.0;
    
    for (int i = 0; i < binCount; ++i)
        totalCount += bins[i];
    
    if (totalCount == 0.0)
        totalCount = 1.0;   // counts are all zero; prevent divide by zero below, get 0 instead
    
    for (int i = 0; i < binCount; ++i)
        bins[i] = bins[i] / totalCount;
    
    return bins;
}

void QtSLiMGraphView_PopFitnessDist::drawGraph(QPainter &painter, QRect interiorRect)
{
    double *plotData = populationFitnessData();
	int binCount = histogramBinCount_;
	
	// plot our histogram bars
	drawBarplot(painter, interiorRect, plotData, binCount, xAxisMin_, (xAxisMax_ - xAxisMin_) / binCount);
}

bool QtSLiMGraphView_PopFitnessDist::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_PopFitnessDist::appendStringForData(QString &string)
{
	double *plotData = populationFitnessData();
	int binCount = histogramBinCount_;
	
    for (int i = 0; i < binCount; ++i)
        string.append(QString("%1, ").arg(plotData[i], 0, 'f', 4));
    
    string.append("\n");
}





























