//
//  QtSLiMGraphView_LifetimeReproduction.cpp
//  SLiM
//
//  Created by Ben Haller on 11/30/2020.
//  Copyright (c) 2020-2023 Philipp Messer.  All rights reserved.
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

#include "QtSLiMGraphView_LifetimeReproduction.h"

#include <QComboBox>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QApplication>
#include <QGuiApplication>
#include <QDebug>

#include <string>
#include <algorithm>

#include "QtSLiMWindow.h"
#include "subpopulation.h"


QtSLiMGraphView_LifetimeReproduction::QtSLiMGraphView_LifetimeReproduction(QWidget *p_parent, QtSLiMWindow *controller) : QtSLiMGraphView(p_parent, controller)
{
    histogramBinCount_ = 11;        // max reproductive output (from 0 to 10); this rescales automatically
    allowBinCountRescale_ = false;
    
    xAxisMin_ = -1;                 // zero is included
    xAxisMax_ = histogramBinCount_ - 1;
    xAxisHistogramStyle_ = true;
    xAxisTickValuePrecision_ = 0;
    tweakXAxisTickLabelAlignment_ = true;
    
    xAxisLabel_ = "Lifetime reproduction";
    yAxisLabel_ = "Frequency";
    
    allowXAxisUserRescale_ = false;
    allowYAxisUserRescale_ = false;
    
    showHorizontalGridLines_ = true;
    allowHorizontalGridChange_ = true;
    allowVerticalGridChange_ = false;
    allowFullBoxChange_ = true;
    
    selectedSubpopulation1ID_ = 1;
}

void QtSLiMGraphView_LifetimeReproduction::addedToWindow(void)
{
    // Make our pop-up menu buttons
    QHBoxLayout *button_layout = buttonLayout();
    
    if (button_layout)
    {
        subpopulation1Button_ = newButtonInLayout(button_layout);
        connect(subpopulation1Button_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_LifetimeReproduction::subpopulation1PopupChanged);
        
        addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
    }
}

QtSLiMGraphView_LifetimeReproduction::~QtSLiMGraphView_LifetimeReproduction()
{
}

void QtSLiMGraphView_LifetimeReproduction::subpopulation1PopupChanged(int /* index */)
{
    slim_objectid_t newSubpopID = SLiMClampToObjectidType(subpopulation1Button_->currentData().toInt());
    
    // don't react to non-changes and changes during rebuilds
    if (!rebuildingMenu_ && (selectedSubpopulation1ID_ != newSubpopID))
    {
        selectedSubpopulation1ID_ = newSubpopID;
        
        // Reset our autoscaling x axis
        histogramBinCount_ = 11;
        xAxisMax_ = histogramBinCount_ - 1;
        
        invalidateCachedData();
        update();
    }
}

void QtSLiMGraphView_LifetimeReproduction::controllerRecycled(void)
{
	if (!controller_->invalidSimulation())
		update();
    
    // Remake our popups, whether or not the controller is valid
    addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
    
    // Reset our autoscaling x axis
    histogramBinCount_ = 11;
    xAxisMax_ = histogramBinCount_ - 1;
    
    // Reset our autoscaling y axis
    yAxisMax_ = 1.0;
    yAxisMajorTickInterval_ = 0.5;
    yAxisMinorTickInterval_ = 0.25;
    
	QtSLiMGraphView::controllerRecycled();
}

QString QtSLiMGraphView_LifetimeReproduction::graphTitle(void)
{
    return "Lifetime Reproductive Output";
}

QString QtSLiMGraphView_LifetimeReproduction::aboutString(void)
{
    return "The Lifetime Reproductive Output graph shows the distribution of lifetime reproductive output within "
           "a chosen subpopulation, for individuals that died in the last tick.  The x axis is individual "
           "lifetime reproductive output; the y axis is the frequency of a given lifetime reproductive output "
           "in the population, normalized to a total of 1.0.";
}

void QtSLiMGraphView_LifetimeReproduction::updateAfterTick(void)
{
    // Rebuild the subpop and muttype menus; this has the side effect of checking and fixing our selections, and that,
	// in turn, will have the side effect of invaliding our cache and fetching new data if needed
    addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
	
    invalidateCachedData();
	QtSLiMGraphView::updateAfterTick();
}

QString QtSLiMGraphView_LifetimeReproduction::disableMessage(void)
{
    Species *graphSpecies = focalDisplaySpecies();
    
    if (graphSpecies)
    {
        if (graphSpecies->SubpopulationWithID(selectedSubpopulation1ID_) == nullptr)
            return "no\ndata";
    }
    
    return "";
}

void QtSLiMGraphView_LifetimeReproduction::drawGraph(QPainter &painter, QRect interiorRect)
{
    int binCount = histogramBinCount_;
    Species *graphSpecies = focalDisplaySpecies();
    bool tallySexesSeparately = graphSpecies->sex_enabled_;
	double *reproductionDist = reproductionDistribution(&binCount, tallySexesSeparately);
    int totalBinCount = tallySexesSeparately ? (binCount * 2) : binCount;
	
    if (reproductionDist)
    {
        // rescale the x axis if needed
        if (binCount != histogramBinCount_)
        {
            histogramBinCount_ = binCount;
            xAxisMax_ = histogramBinCount_ - 1;
            invalidateCachedData();
        }
        
        // rescale the y axis if needed
        double maxFreq = 0.000000001;   // guarantee a non-zero axis range
        
        for (int binIndex = 0; binIndex < totalBinCount; ++binIndex)
            maxFreq = std::max(maxFreq, reproductionDist[binIndex]);
        
        double ceilingFreq = std::ceil(maxFreq * 5.0) / 5.0;    // 0.2 / 0.4 / 0.6 / 0.8 / 1.0
        
        if ((ceilingFreq > yAxisMax_) ||
                ((ceilingFreq < yAxisMax_) && (maxFreq + 0.05 < ceilingFreq)))    // require a margin of error to jump down
        {
            yAxisMax_ = ceilingFreq;
            yAxisMajorTickInterval_ = ceilingFreq / 2.0;
            yAxisMinorTickInterval_ = ceilingFreq / 4.0;
        }
        
        // plot our histogram bars; note that xAxisMin_ is -1 so we use that as a minimum
        if (tallySexesSeparately)
            drawGroupedBarplot(painter, interiorRect, reproductionDist, 2, histogramBinCount_, -1.0, 1.0);
        else
            drawBarplot(painter, interiorRect, reproductionDist, histogramBinCount_, -1.0, 1.0);
        
        free(reproductionDist);
    }
}

QtSLiMLegendSpec QtSLiMGraphView_LifetimeReproduction::legendKey(void)
{
    Species *graphSpecies = focalDisplaySpecies();
    bool tallySexesSeparately = graphSpecies->sex_enabled_;
    
	if (tallySexesSeparately)
    {
        QtSLiMLegendSpec legend_key;
        
        legend_key.resize(2);
        legend_key[0].first = "M";
        legend_key[0].second = controller_->blackContrastingColorForIndex(0);
        legend_key[1].first = "F";
        legend_key[1].second = controller_->blackContrastingColorForIndex(1);
        
        return legend_key;
    }
    else
    {
        return QtSLiMLegendSpec();
    }
}

bool QtSLiMGraphView_LifetimeReproduction::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_LifetimeReproduction::appendStringForData(QString &string)
{
    int binCount = histogramBinCount_;
    Species *graphSpecies = focalDisplaySpecies();
    bool tallySexesSeparately = graphSpecies->sex_enabled_;
	double *reproductionDist = reproductionDistribution(&binCount, tallySexesSeparately);
	
    if (reproductionDist)
    {
        if (tallySexesSeparately)
        {
            string.append("M : ");
            
            for (int i = 0; i < binCount; ++i)
                string.append(QString("%1, ").arg(reproductionDist[i * 2], 0, 'f', 4));
            
            string.append("\n\nF : ");
            
            for (int i = 0; i < binCount; ++i)
                string.append(QString("%1, ").arg(reproductionDist[i * 2 + 1], 0, 'f', 4));
        }
        else
        {
            for (int i = 0; i < binCount; ++i)
                string.append(QString("%1, ").arg(reproductionDist[i], 0, 'f', 4));
        }
        
        free(reproductionDist);
    }
    
    string.append("\n");
}

double *QtSLiMGraphView_LifetimeReproduction::reproductionDistribution(int *binCount, bool tallySexesSeparately)
{
    // Find our subpop
    Species *graphSpecies = focalDisplaySpecies();
    Subpopulation *subpop1 = graphSpecies->SubpopulationWithID(selectedSubpopulation1ID_);
    
    if (!subpop1)
        return nullptr;
    
    // Find the maximum age and choose the new bin count
    slim_age_t maxReproduction = 0;
    
    for (int reproduction : subpop1->lifetime_reproductive_output_F_)
        maxReproduction = std::max(maxReproduction, reproduction);
    for (int reproduction : subpop1->lifetime_reproductive_output_MH_)
        maxReproduction = std::max(maxReproduction, reproduction);
    
    if (maxReproduction > *binCount)
        *binCount = (int)(std::ceil(maxReproduction / 10.0) * 10.0 + 1);
    
    int newBinCount = *binCount;
    
    // Tally into our bins
    int totalBinCount = (tallySexesSeparately ? newBinCount * 2 : newBinCount);
    double *reproductionTallies = static_cast<double *>(calloc(totalBinCount, sizeof(double)));
    
    for (int reproduction : subpop1->lifetime_reproductive_output_F_)
    {
        if (tallySexesSeparately)
            reproductionTallies[reproduction * 2 + 1]++;
        else
            reproductionTallies[reproduction]++;
    }
    for (int reproduction : subpop1->lifetime_reproductive_output_MH_)
    {
        if (tallySexesSeparately)
            reproductionTallies[reproduction * 2]++;
        else
            reproductionTallies[reproduction]++;
    }
    
    // Normalize to 1
    
    if (tallySexesSeparately)
    {
        // males
        double totalTallies = 0.0;
        
        for (int i = 0; i < newBinCount; ++i)
            totalTallies += reproductionTallies[i * 2];
        
        if (totalTallies > 0.0)
            for (int i = 0; i < newBinCount; ++i)
                reproductionTallies[i * 2] /= totalTallies;
        
        // females
        totalTallies = 0.0;
        
        for (int i = 0; i < newBinCount; ++i)
            totalTallies += reproductionTallies[i * 2 + 1];
        
        if (totalTallies > 0.0)
            for (int i = 0; i < newBinCount; ++i)
                reproductionTallies[i * 2 + 1] /= totalTallies;
    }
    else
    {
        double totalTallies = 0.0;
        
        for (int i = 0; i < newBinCount; ++i)
            totalTallies += reproductionTallies[i];
        
        if (totalTallies > 0.0)
            for (int i = 0; i < newBinCount; ++i)
                reproductionTallies[i] /= totalTallies;
    }
    
    // Return the final tally; note that the caller takes ownership of the buffer
	return reproductionTallies;
}






























