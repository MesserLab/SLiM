//
//  QtSLiMGraphView_2DPopulationSFS.cpp
//  SLiM
//
//  Created by Ben Haller on 8/22/2020.
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

#include "QtSLiMGraphView_2DPopulationSFS.h"

#include <QComboBox>
#include <QDebug>

#include "mutation_type.h"


QtSLiMGraphView_2DPopulationSFS::QtSLiMGraphView_2DPopulationSFS(QWidget *parent, QtSLiMWindow *controller) : QtSLiMGraphView(parent, controller)
{
    histogramBinCount_ = 20;
    allowBinCountRescale_ = true;
    
    heatmapMargins_ = 0;
    allowHeatmapMarginsChange_ = true;
    
    xAxisLabel_ = "Frequency in p1";
    yAxisLabel_ = "Frequency in p2";
    
    allowXAxisUserRescale_ = false;
    allowYAxisUserRescale_ = false;
    
    showHorizontalGridLines_ = false;
    showVerticalGridLines_ = false;
    showFullBox_ = true;
    allowHorizontalGridChange_ = false;
    allowVerticalGridChange_ = false;
    allowFullBoxChange_ = false;
    
    // Default to plotting p1 against p2, with no default mutation type
    selectedSubpopulation1ID_ = 1;
    selectedSubpopulation2ID_ = 2;
    selectedMutationTypeIndex_ = -1;
}

void QtSLiMGraphView_2DPopulationSFS::addedToWindow(void)
{
    // Make our pop-up menu buttons
    QHBoxLayout *layout = buttonLayout();
    
    if (layout)
    {
        subpopulation1Button_ = newButtonInLayout(layout);
        connect(subpopulation1Button_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_2DPopulationSFS::subpopulation1PopupChanged);
        
        subpopulation2Button_ = newButtonInLayout(layout);
        connect(subpopulation2Button_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_2DPopulationSFS::subpopulation2PopupChanged);
        
        mutationTypeButton_ = newButtonInLayout(layout);
        connect(mutationTypeButton_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_2DPopulationSFS::mutationTypePopupChanged);
        
        addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
        addSubpopulationsToMenu(subpopulation2Button_, selectedSubpopulation2ID_);
        addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
    }
}

QtSLiMGraphView_2DPopulationSFS::~QtSLiMGraphView_2DPopulationSFS()
{
}

void QtSLiMGraphView_2DPopulationSFS::subpopulation1PopupChanged(int /* index */)
{
    slim_objectid_t newSubpopID = SLiMClampToObjectidType(subpopulation1Button_->currentData().toInt());
    
    // don't react to non-changes and changes during rebuilds
    if (!rebuildingMenu_ && (selectedSubpopulation1ID_ != newSubpopID))
    {
        selectedSubpopulation1ID_ = newSubpopID;
        xAxisLabel_ = QString("Frequency in p%1").arg(selectedSubpopulation1ID_);
        invalidateDrawingCache();
        update();
    }
}

void QtSLiMGraphView_2DPopulationSFS::subpopulation2PopupChanged(int /* index */)
{
    slim_objectid_t newSubpopID = SLiMClampToObjectidType(subpopulation2Button_->currentData().toInt());
    
    // don't react to non-changes and changes during rebuilds
    if (!rebuildingMenu_ && (selectedSubpopulation2ID_ != newSubpopID))
    {
        selectedSubpopulation2ID_ = newSubpopID;
        yAxisLabel_ = QString("Frequency in p%1").arg(selectedSubpopulation2ID_);
        invalidateDrawingCache();
        update();
    }
}

void QtSLiMGraphView_2DPopulationSFS::mutationTypePopupChanged(int /* index */)
{
    int newMutTypeIndex = mutationTypeButton_->currentData().toInt();
    
    // don't react to non-changes and changes during rebuilds
    if (!rebuildingMenu_ && (selectedMutationTypeIndex_ != newMutTypeIndex))
    {
        selectedMutationTypeIndex_ = newMutTypeIndex;
        invalidateDrawingCache();
        update();
    }
}

void QtSLiMGraphView_2DPopulationSFS::controllerRecycled(void)
{
	if (!controller_->invalidSimulation())
		update();
	
    // Remake our popups, whether or not the controller is valid
    addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
    addSubpopulationsToMenu(subpopulation2Button_, selectedSubpopulation2ID_);
	addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
    
	QtSLiMGraphView::controllerRecycled();
}

QString QtSLiMGraphView_2DPopulationSFS::graphTitle(void)
{
    return "2D Population SFS";
}

QString QtSLiMGraphView_2DPopulationSFS::aboutString(void)
{
    return "The 2D Population SFS graph shows a Site Frequency Spectrum (SFS) for two entire subpopulations in "
           "the population, for mutations of a given mutation type.  Since mutation occurrence counts across "
           "whole subpopulations might be very large, the x and y axes here are the frequencies of a given mutation "
           "in the two subpopulations, from 0.0 to 1.0 on each axis, rather than occurrence counts.  The z axis, "
           "represented with color, is the proportion of mutations (among those present in either of the two "
           "subpopulations) that fall within a binned range of frequencies in the two subpopulations; a proportion "
           "of zero is represented by white, and the maximum observed proportion is represented by black (rescaled each "
           "time the graph redisplays), with heat colors from yellow (low) through red and up to black (high).  The "
           "number of frequency bins can be customized from the action menu.  The 2D Sample SFS graph provides an "
           "alternative that might also be useful.";
}

void QtSLiMGraphView_2DPopulationSFS::updateAfterTick(void)
{
    // Rebuild the subpop and muttype menus; this has the side effect of checking and fixing our selections, and that,
	// in turn, will have the side effect of invaliding our cache and fetching new data if needed
    addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
    addSubpopulationsToMenu(subpopulation2Button_, selectedSubpopulation2ID_, selectedSubpopulation1ID_);
	addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
	
    invalidateDrawingCache();
	QtSLiMGraphView::updateAfterTick();
}

QString QtSLiMGraphView_2DPopulationSFS::disableMessage(void)
{
    if (controller_ && !controller_->invalidSimulation())
    {
        Subpopulation *subpop1 = controller_->sim->SubpopulationWithID(selectedSubpopulation1ID_);
        Subpopulation *subpop2 = controller_->sim->SubpopulationWithID(selectedSubpopulation2ID_);
        MutationType *muttype = controller_->sim->MutationTypeWithID(selectedMutationTypeIndex_);
        
        if (!subpop1 || !subpop2 || !muttype)
            return "no\ndata";
    }
    
return "";
}


void QtSLiMGraphView_2DPopulationSFS::drawGraph(QPainter &painter, QRect interiorRect)
{
    double *sfs2dbuf = mutation2DSFS();
    
    if (sfs2dbuf)
    {
        drawHeatmap(painter, interiorRect, sfs2dbuf, histogramBinCount_, histogramBinCount_);
        free(sfs2dbuf);
    }
}

bool QtSLiMGraphView_2DPopulationSFS::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_2DPopulationSFS::appendStringForData(QString &string)
{
    double *plotData = mutation2DSFS();
	
    for (int y = 0; y < histogramBinCount_; ++y)
    {
        for (int x = 0; x < histogramBinCount_; ++x)
            string.append(QString("%1, ").arg(plotData[x + y * histogramBinCount_], 0, 'f', 4));
        string.append("\n");
    }
    
    free(plotData);
}

double *QtSLiMGraphView_2DPopulationSFS::mutation2DSFS(void)
{
    SLiMSim *sim = controller_->sim;
    Population &population = sim->population_;
    int registry_size;
    const MutationIndex *registry = population.MutationRegistry(&registry_size);
    const MutationIndex *registry_iter_end = registry + registry_size;
    
    // Find our subpops and mutation type
    Subpopulation *subpop1 = sim->SubpopulationWithID(selectedSubpopulation1ID_);
    Subpopulation *subpop2 = sim->SubpopulationWithID(selectedSubpopulation2ID_);
    MutationType *muttype = sim->MutationTypeWithID(selectedMutationTypeIndex_);
    
    if (!subpop1 || !subpop2 || !muttype)
		return nullptr;
    
    // Get frequencies in subpop1 and subpop2
    Mutation *mut_block_ptr = gSLiM_Mutation_Block;
    std::vector<slim_refcount_t> refcounts1, refcounts2;
    size_t subpop1_total_genome_count, subpop2_total_genome_count;
    
    {
        subpop1_total_genome_count = tallyGUIMutationReferences(selectedSubpopulation1ID_, selectedMutationTypeIndex_);
        
        for (const MutationIndex *registry_iter = registry; registry_iter != registry_iter_end; ++registry_iter)
        {
            const Mutation *mutation = mut_block_ptr + *registry_iter;
            if (mutation->mutation_type_ptr_->mutation_type_index_ == selectedMutationTypeIndex_)
                refcounts1.push_back(mutation->gui_scratch_reference_count_);
        }
        
        if (subpop1_total_genome_count == 0)
            subpop1_total_genome_count = 1;     // counts will all be zero; prevent NAN frequency, make it zero instead
    }
    {
        subpop2_total_genome_count = tallyGUIMutationReferences(selectedSubpopulation2ID_, selectedMutationTypeIndex_);
        
        for (const MutationIndex *registry_iter = registry; registry_iter != registry_iter_end; ++registry_iter)
        {
            const Mutation *mutation = mut_block_ptr + *registry_iter;
            if (mutation->mutation_type_ptr_->mutation_type_index_ == selectedMutationTypeIndex_)
                refcounts2.push_back(mutation->gui_scratch_reference_count_);
        }
        
        if (subpop2_total_genome_count == 0)
            subpop2_total_genome_count = 1;     // counts will all be zero; prevent NAN frequency, make it zero instead
    }
    
    // Tally up the binned 2D SFS from the 1D data
    double *sfs2dbuf = (double *)calloc(histogramBinCount_ * histogramBinCount_, sizeof(double));
    size_t refcounts_size = refcounts1.size();
    
    for (size_t refcount_index = 0; refcount_index < refcounts_size; ++refcount_index)
    {
        slim_refcount_t count1 = refcounts1[refcount_index];
        slim_refcount_t count2 = refcounts2[refcount_index];
        
        // exclude mutations that are not present in either subpopulation
        if ((count1 > 0) || (count2 > 0))
        {
            double freq1 = count1 / (double)subpop1_total_genome_count;
            double freq2 = count2 / (double)subpop2_total_genome_count;
            int bin1 = static_cast<int>(round(freq1 * (histogramBinCount_ - 1)));
            int bin2 = static_cast<int>(round(freq2 * (histogramBinCount_ - 1)));
            sfs2dbuf[bin1 + bin2 * histogramBinCount_]++;
        }
    }
    
    // Normalize the bin counts to [0,1]; 0 is reserved for actual zero counts, the rest are on a log scale
    double maxCount = 0;
    
    for (int i = 0; i < histogramBinCount_ * histogramBinCount_; ++i)
        maxCount = std::max(maxCount, sfs2dbuf[i]);
    
    if (maxCount > 0.0)
    {
        double logMaxCount = std::log10(maxCount + 1);
        
        for (int i = 0; i < histogramBinCount_ * histogramBinCount_; ++i)
        {
            double value = sfs2dbuf[i];
            
            if (value != 0.0)
                sfs2dbuf[i] = std::log10(value + 1) / logMaxCount;
        }
    }
    
    // return the final tally; note the caller takes responsibility for freeing this buffer!
    return sfs2dbuf;
}



























