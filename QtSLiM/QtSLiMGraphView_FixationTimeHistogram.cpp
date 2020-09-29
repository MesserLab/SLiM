//
//  QtSLiMGraphView_FixationTimeHistogram.cpp
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

#include "QtSLiMGraphView_FixationTimeHistogram.h"

#include "QtSLiMWindow.h"


QtSLiMGraphView_FixationTimeHistogram::QtSLiMGraphView_FixationTimeHistogram(QWidget *parent, QtSLiMWindow *controller) : QtSLiMGraphView(parent, controller)
{
    histogramBinCount_ = 10;
    //allowBinCountRescale_ = true;     // not supported yet
    
    xAxisMax_ = 1000;
    xAxisMajorTickInterval_ = 200;
    xAxisMinorTickInterval_ = 100;
    xAxisMajorTickModulus_ = 2;
    xAxisTickValuePrecision_ = 0;
    
    xAxisLabel_ = "Mutation fixation time";
    yAxisLabel_ = "Proportion of fixed mutations";
    
    allowXAxisUserRescale_ = false;
    allowYAxisUserRescale_ = true;
    
    showHorizontalGridLines_ = true;
}

QtSLiMGraphView_FixationTimeHistogram::~QtSLiMGraphView_FixationTimeHistogram()
{
}

QString QtSLiMGraphView_FixationTimeHistogram::graphTitle(void)
{
    return "Mutation Fixation Time";
}

QString QtSLiMGraphView_FixationTimeHistogram::aboutString(void)
{
    return "The Mutation Fixation Time graph shows a histogram of mutation fixation times, "
           "for those mutations that have fixed.  The proportions are calculated and plotted "
           "separately for each mutation type, for comparison.";
}

double *QtSLiMGraphView_FixationTimeHistogram::fixationTimeData(void)
{
    int binCount = histogramBinCount_;
	int mutationTypeCount = static_cast<int>(controller_->sim->mutation_types_.size());
	slim_generation_t *histogram = controller_->sim->population_.mutation_fixation_times_;
	int64_t histogramBins = static_cast<int64_t>(controller_->sim->population_.mutation_fixation_gen_slots_);	// fewer than binCount * mutationTypeCount may exist
	static double *rebin = nullptr;
	static size_t rebinBins = 0;
	size_t usedRebinBins = static_cast<size_t>(binCount * mutationTypeCount);
	
    // re-bin for display; SLiM bins every 10 generations, but right now we want to plot every 100 generations as a bin
	if (!rebin || (rebinBins < usedRebinBins))
	{
		rebinBins = usedRebinBins;
		rebin = static_cast<double *>(realloc(rebin, rebinBins * sizeof(double)));
	}
	
	for (size_t i = 0; i < usedRebinBins; ++i)
		rebin[i] = 0.0;
	
	for (int i = 0; i < binCount * 10; ++i)
	{
		for (int j = 0; j < mutationTypeCount; ++j)
		{
			int histIndex = j + i * mutationTypeCount;
			
			if (histIndex < histogramBins)
                rebin[j + (i / 10) * mutationTypeCount] += histogram[histIndex];
		}
	}
	
	// normalize within each mutation type
	for (int mutationTypeIndex = 0; mutationTypeIndex < mutationTypeCount; ++mutationTypeIndex)
	{
		int64_t total = 0;
		
		for (int bin = 0; bin < binCount; ++bin)
		{
			int binIndex = mutationTypeIndex + bin * mutationTypeCount;
			
			total += static_cast<int64_t>(rebin[binIndex]);
		}
        
        if (total == 0)
            total = 1;  // if counts are all zero, avoid a divide by zero below and just end up with 0 instead
		
		for (int bin = 0; bin < binCount; ++bin)
		{
			int binIndex = mutationTypeIndex + bin * mutationTypeCount;
			
			rebin[binIndex] /= static_cast<double>(total);
		}
	}
	
	return rebin;
}

void QtSLiMGraphView_FixationTimeHistogram::drawGraph(QPainter &painter, QRect interiorRect)
{
    double *plotData = fixationTimeData();
	int binCount = histogramBinCount_;
    int mutationTypeCount = static_cast<int>(controller_->sim->mutation_types_.size());
	
	// plot our histogram bars
	drawGroupedBarplot(painter, interiorRect, plotData, mutationTypeCount, binCount, 0.0, 100.0);
}

QtSLiMLegendSpec QtSLiMGraphView_FixationTimeHistogram::legendKey(void)
{
	return mutationTypeLegendKey();     // we use the prefab mutation type legend
}

bool QtSLiMGraphView_FixationTimeHistogram::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_FixationTimeHistogram::appendStringForData(QString &string)
{
	double *plotData = fixationTimeData();
	int binCount = histogramBinCount_;
	SLiMSim *sim = controller_->sim;
    int mutationTypeCount = static_cast<int>(sim->mutation_types_.size());
	
	for (auto mutationTypeIter : sim->mutation_types_)
	{
		MutationType *mutationType = mutationTypeIter.second;
		int mutationTypeIndex = mutationType->mutation_type_index_;		// look up the index used for this mutation type in the history info; not necessarily sequential!
		
        string.append(QString("\"m%1\", ").arg(mutationType->mutation_type_id_));
		
		for (int i = 0; i < binCount; ++i)
		{
			int histIndex = mutationTypeIndex + i * mutationTypeCount;
			
            string.append(QString("%1, ").arg(plotData[histIndex], 0, 'f', 4));
		}
		
        string.append("\n");
	}
}





























