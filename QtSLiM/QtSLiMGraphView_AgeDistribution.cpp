//
//  QtSLiMGraphView_AgeDistribution.cpp
//  SLiM
//
//  Created by Ben Haller on 8/30/2020.
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

#include "QtSLiMGraphView_AgeDistribution.h"

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


QtSLiMGraphView_AgeDistribution::QtSLiMGraphView_AgeDistribution(QWidget *p_parent, QtSLiMWindow *controller) : QtSLiMGraphView(p_parent, controller)
{
    histogramBinCount_ = 10;        // max age (no age 0 since we display after tick increment); this rescales automatically
    allowBinCountRescale_ = false;
    
    x0_ = 0;
    x1_ = histogramBinCount_;
    
    xAxisMin_ = x0_;
    xAxisMax_ = x1_;
    xAxisHistogramStyle_ = true;
    xAxisTickValuePrecision_ = 0;
    tweakXAxisTickLabelAlignment_ = true;
    
    xAxisLabel_ = "Age";
    yAxisLabel_ = "Frequency";
    
    allowXAxisUserRescale_ = false;
    allowYAxisUserRescale_ = false;
    
    showHorizontalGridLines_ = true;
    allowHorizontalGridChange_ = true;
    allowVerticalGridChange_ = false;
    allowFullBoxChange_ = true;
    
    selectedSubpopulation1ID_ = 1;
}

void QtSLiMGraphView_AgeDistribution::addedToWindow(void)
{
    // Make our pop-up menu buttons
    QHBoxLayout *button_layout = buttonLayout();
    
    if (button_layout)
    {
        subpopulation1Button_ = newButtonInLayout(button_layout);
        connect(subpopulation1Button_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_AgeDistribution::subpopulation1PopupChanged);
        
        addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
    }
}

QtSLiMGraphView_AgeDistribution::~QtSLiMGraphView_AgeDistribution()
{
}

void QtSLiMGraphView_AgeDistribution::subpopulation1PopupChanged(int /* index */)
{
    slim_objectid_t newSubpopID = SLiMClampToObjectidType(subpopulation1Button_->currentData().toInt());
    
    // don't react to non-changes and changes during rebuilds
    if (!rebuildingMenu_ && (selectedSubpopulation1ID_ != newSubpopID))
    {
        selectedSubpopulation1ID_ = newSubpopID;
        
        // Reset our autoscaling x axis
        histogramBinCount_ = 10;
        xAxisMax_ = histogramBinCount_;
        x1_ = xAxisMax_;               // the same as xAxisMax_, for base plots
        
        invalidateCachedData();
        update();
    }
}

void QtSLiMGraphView_AgeDistribution::controllerRecycled(void)
{
	if (!controller_->invalidSimulation())
		update();
    
    // Remake our popups, whether or not the controller is valid
    addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
    
    // Reset our autoscaling x axis
    histogramBinCount_ = 10;
    xAxisMax_ = histogramBinCount_;
    x1_ = xAxisMax_;               // the same as xAxisMax_, for base plots
    
    // Reset our autoscaling y axis
    yAxisMax_ = 1.0;
    y1_ = yAxisMax_;               // the same as yAxisMax_, for base plots
    yAxisMajorTickInterval_ = 0.5;
    yAxisMinorTickInterval_ = 0.25;
    
	QtSLiMGraphView::controllerRecycled();
}

QString QtSLiMGraphView_AgeDistribution::graphTitle(void)
{
    return "Age Distribution";
}

QString QtSLiMGraphView_AgeDistribution::aboutString(void)
{
    return "The Age Distribution graph shows the distribution of age values within a chosen subpopulation.  The "
           "x axis is individual age (in cycles, in the SLiM sense of the term); the y axis is the frequency "
           "of a given age in the population, normalized to a total of 1.0.  This graph is only meaningful "
           "for nonWF models; WF models have non-overlapping generations without age structure.  Note that "
           "display occurs <i>after</i> the cycle counter increments, so new offspring will have age 1.";
}

void QtSLiMGraphView_AgeDistribution::updateAfterTick(void)
{
    // Rebuild the subpop and muttype menus; this has the side effect of checking and fixing our selections, and that,
	// in turn, will have the side effect of invaliding our cache and fetching new data if needed
    addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
	
    invalidateCachedData();
	QtSLiMGraphView::updateAfterTick();
}

QString QtSLiMGraphView_AgeDistribution::disableMessage(void)
{
    if (controller_ && !controller_->invalidSimulation())
    {
        if (controller_->community->ModelType() == SLiMModelType::kModelTypeWF)
            return "requires a\nnonWF model";
        
        Species *graphSpecies = focalDisplaySpecies();
        
        if (graphSpecies->SubpopulationWithID(selectedSubpopulation1ID_) == nullptr)
            return "no\ndata";
    }
    
    return "";
}

void QtSLiMGraphView_AgeDistribution::drawGraph(QPainter &painter, QRect interiorRect)
{
    int binCount = histogramBinCount_;
    Species *graphSpecies = focalDisplaySpecies();
    bool tallySexesSeparately = graphSpecies->sex_enabled_;
	double *ageDist = ageDistribution(&binCount, tallySexesSeparately);
    int totalBinCount = tallySexesSeparately ? (binCount * 2) : binCount;
	
    if (ageDist)
    {
        // rescale the x axis if needed
        if (binCount != histogramBinCount_)
        {
            histogramBinCount_ = binCount;
            xAxisMax_ = histogramBinCount_;
            x1_ = xAxisMax_;               // the same as xAxisMax_, for base plots
            invalidateCachedData();
        }
        
        // rescale the y axis if needed
        double maxFreq = 0.000000001;   // guarantee a non-zero axis range
        
        for (int binIndex = 0; binIndex < totalBinCount; ++binIndex)
            maxFreq = std::max(maxFreq, ageDist[binIndex]);
        
        double ceilingFreq = std::ceil(maxFreq * 5.0) / 5.0;    // 0.2 / 0.4 / 0.6 / 0.8 / 1.0
        
        if ((ceilingFreq > yAxisMax_) ||
                ((ceilingFreq < yAxisMax_) && (maxFreq + 0.05 < ceilingFreq)))    // require a margin of error to jump down
        {
            yAxisMax_ = ceilingFreq;
            y1_ = yAxisMax_;               // the same as yAxisMax_, for base plots
            yAxisMajorTickInterval_ = ceilingFreq / 2.0;
            yAxisMinorTickInterval_ = ceilingFreq / 4.0;
        }
        
        // plot our histogram bars
        if (tallySexesSeparately)
            drawGroupedBarplot(painter, interiorRect, ageDist, 2, histogramBinCount_, 0.0, 1.0);
        else
            drawBarplot(painter, interiorRect, ageDist, histogramBinCount_, 0.0, 1.0);
        
        free(ageDist);
    }
}

QtSLiMLegendSpec QtSLiMGraphView_AgeDistribution::legendKey(void)
{
    Species *graphSpecies = focalDisplaySpecies();
    bool tallySexesSeparately = graphSpecies->sex_enabled_;
    
	if (tallySexesSeparately)
    {
        QtSLiMLegendSpec legend_key;
        
        legend_key.emplace_back("M", controller_->blackContrastingColorForIndex(0));
        legend_key.emplace_back("F", controller_->blackContrastingColorForIndex(1));
        
        return legend_key;
    }
    else
    {
        return QtSLiMLegendSpec();
    }
}

bool QtSLiMGraphView_AgeDistribution::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_AgeDistribution::appendStringForData(QString &string)
{
    int binCount = histogramBinCount_;
    Species *graphSpecies = focalDisplaySpecies();
    bool tallySexesSeparately = graphSpecies->sex_enabled_;
	double *ageDist = ageDistribution(&binCount, tallySexesSeparately);
	
    if (ageDist)
    {
        if (tallySexesSeparately)
        {
            string.append("M : ");
            
            for (int i = 0; i < binCount; ++i)
                string.append(QString("%1, ").arg(ageDist[i * 2], 0, 'f', 4));
            
            string.append("\n\nF : ");
            
            for (int i = 0; i < binCount; ++i)
                string.append(QString("%1, ").arg(ageDist[i * 2 + 1], 0, 'f', 4));
        }
        else
        {
            for (int i = 0; i < binCount; ++i)
                string.append(QString("%1, ").arg(ageDist[i], 0, 'f', 4));
        }
        
        free(ageDist);
    }
    
    string.append("\n");
}

double *QtSLiMGraphView_AgeDistribution::ageDistribution(int *binCount, bool tallySexesSeparately)
{
    // Find our subpop
    Species *graphSpecies = focalDisplaySpecies();
    Subpopulation *subpop1 = graphSpecies->SubpopulationWithID(selectedSubpopulation1ID_);
    
    if (!subpop1)
        return nullptr;
    
    // Find the maximum age and choose the new bin count
    slim_age_t maxAge = 1;
    
    for (const Individual *individual : subpop1->parent_individuals_)
        maxAge = std::max(maxAge, individual->age_);
    
    // compare to the logic in QtSLiMGraphView_LifetimeReproduction::reproductionDistribution();
    // it is different here because we subtract 1 from every age in the tallying code below,
    // there is no bin for age==0, and age==1 goes into bin #0; confusing!
    if (maxAge > *binCount)
        *binCount = (slim_age_t)(std::ceil(maxAge / 10.0) * 10.0);
    
    int newBinCount = *binCount;
    
    // Tally into our bins
    int totalBinCount = (tallySexesSeparately ? newBinCount * 2 : newBinCount);
    double *ageTallies = static_cast<double *>(calloc(totalBinCount, sizeof(double)));
    
    for (const Individual *individual : subpop1->parent_individuals_)
    {
        slim_age_t age = individual->age_ - 1;  // age 1 is bin 0, age binCount is bin binCount-1
        
        if (age < 0) age = 0;
        if (age >= newBinCount) age = newBinCount - 1;
        
        if (tallySexesSeparately)
        {
            if (individual->sex_ == IndividualSex::kFemale)
                ageTallies[age * 2 + 1]++;
            else
                ageTallies[age * 2]++;
        }
        else
        {
            ageTallies[age]++;
        }
    }
    
    // Normalize to 1
    
    if (tallySexesSeparately)
    {
        // males
        double totalTallies = 0.0;
        
        for (int i = 0; i < newBinCount; ++i)
            totalTallies += ageTallies[i * 2];
        
        if (totalTallies > 0.0)
            for (int i = 0; i < newBinCount; ++i)
                ageTallies[i * 2] /= totalTallies;
        
        // females
        totalTallies = 0.0;
        
        for (int i = 0; i < newBinCount; ++i)
            totalTallies += ageTallies[i * 2 + 1];
        
        if (totalTallies > 0.0)
            for (int i = 0; i < newBinCount; ++i)
                ageTallies[i * 2 + 1] /= totalTallies;
    }
    else
    {
        double totalTallies = 0.0;
        
        for (int i = 0; i < newBinCount; ++i)
            totalTallies += ageTallies[i];
        
        if (totalTallies > 0.0)
            for (int i = 0; i < newBinCount; ++i)
                ageTallies[i] /= totalTallies;
    }
    
    // Return the final tally; note that the caller takes ownership of the buffer
	return ageTallies;
}






























