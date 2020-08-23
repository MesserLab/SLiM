//
//  QtSLiMGraphView_2DFrequencySpectrum.cpp
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

#include "QtSLiMGraphView_2DFrequencySpectrum.h"

#include <QComboBox>
#include <QDebug>

#include "mutation_type.h"


QtSLiMGraphView_2DFrequencySpectrum::QtSLiMGraphView_2DFrequencySpectrum(QWidget *parent, QtSLiMWindow *controller) : QtSLiMGraphView(parent, controller)
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

bool QtSLiMGraphView_2DFrequencySpectrum::needsButtonLayout(void)
{
    return true;
}

void QtSLiMGraphView_2DFrequencySpectrum::addedToWindow(void)
{
    // Make our pop-up menu buttons
    QHBoxLayout *layout = buttonLayout();
    
    if (layout)
    {
        subpopulation1Button_ = newButtonInLayout(layout);
        connect(subpopulation1Button_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_2DFrequencySpectrum::subpopulation1PopupChanged);
        
        subpopulation2Button_ = newButtonInLayout(layout);
        connect(subpopulation2Button_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_2DFrequencySpectrum::subpopulation2PopupChanged);
        
        mutationTypeButton_ = newButtonInLayout(layout);
        connect(mutationTypeButton_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_2DFrequencySpectrum::mutationTypePopupChanged);
        
        QSpacerItem *rightSpacer = new QSpacerItem(16, 5, QSizePolicy::Expanding, QSizePolicy::Minimum);
        layout->addItem(rightSpacer);
        
        addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
        addSubpopulationsToMenu(subpopulation2Button_, selectedSubpopulation2ID_);
        addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
    }
}

QtSLiMGraphView_2DFrequencySpectrum::~QtSLiMGraphView_2DFrequencySpectrum()
{
}

void QtSLiMGraphView_2DFrequencySpectrum::subpopulation1PopupChanged(int /* index */)
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

void QtSLiMGraphView_2DFrequencySpectrum::subpopulation2PopupChanged(int /* index */)
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

void QtSLiMGraphView_2DFrequencySpectrum::mutationTypePopupChanged(int /* index */)
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

void QtSLiMGraphView_2DFrequencySpectrum::controllerRecycled(void)
{
	if (!controller_->invalidSimulation())
		update();
	
    // Remake our popups, whether or not the controller is valid
    addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
    addSubpopulationsToMenu(subpopulation2Button_, selectedSubpopulation2ID_);
	addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
    
	QtSLiMGraphView::controllerRecycled();
}

QString QtSLiMGraphView_2DFrequencySpectrum::graphTitle(void)
{
    return "2D Mutation Frequency Spectrum";
}

void QtSLiMGraphView_2DFrequencySpectrum::updateAfterTick(void)
{
    // Rebuild the subpop and muttype menus; this has the side effect of checking and fixing our selections, and that,
	// in turn, will have the side effect of invaliding our cache and fetching new data if needed
    addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
    addSubpopulationsToMenu(subpopulation2Button_, selectedSubpopulation2ID_, selectedSubpopulation1ID_);
	addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
	
    invalidateDrawingCache();
	QtSLiMGraphView::updateAfterTick();
}

void QtSLiMGraphView_2DFrequencySpectrum::drawGraph(QPainter &painter, QRect interiorRect)
{
    double *sfs2dbuf = mutation2DSFS();
    
    if (sfs2dbuf)
    {
        drawHeatmap(painter, interiorRect, sfs2dbuf, histogramBinCount_, histogramBinCount_);
        free(sfs2dbuf);
    }
}

bool QtSLiMGraphView_2DFrequencySpectrum::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_2DFrequencySpectrum::appendStringForData(QString &string)
{
    double *plotData = mutation2DSFS();
	
    for (int y = 0; y < histogramBinCount_; ++y)
    {
        for (int x = 0; x < histogramBinCount_; ++x)
            string.append(QString("%1, ").arg(plotData[x + y * histogramBinCount_], 0, 'f', 4));
        string.append("\n");
    }
}

double *QtSLiMGraphView_2DFrequencySpectrum::mutation2DSFS(void)
{
    SLiMSim *sim = controller_->sim;
    Population &population = sim->population_;
    MutationRun &mutationRegistry = population.mutation_registry_;
    
    // Find our subpops and mutation type
    Subpopulation *subpop1 = nullptr;
    Subpopulation *subpop2 = nullptr;
    MutationType *muttype = nullptr;
    
    for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population.subpops_)
		if (subpop_pair.first == selectedSubpopulation1ID_)	// find our chosen subpop
			subpop1 = subpop_pair.second;
	
    for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : population.subpops_)
		if (subpop_pair.first == selectedSubpopulation2ID_)	// find our chosen subpop
			subpop2 = subpop_pair.second;
	
	for (const std::pair<const slim_objectid_t,MutationType*> &muttype_pair : sim->mutation_types_)
		if (muttype_pair.second->mutation_type_index_ == selectedMutationTypeIndex_)	// find our chosen muttype
			muttype = muttype_pair.second;
    
    if (!subpop1 || !subpop2 || !muttype)
		return nullptr;
    
    // Get frequencies in subpop1 and subpop2
    Mutation *mut_block_ptr = gSLiM_Mutation_Block;
    const MutationIndex *registry_iter = mutationRegistry.begin_pointer_const();
	const MutationIndex *registry_iter_end = mutationRegistry.end_pointer_const();
    std::vector<slim_refcount_t> refcounts1, refcounts2;
    size_t subpop1_total_genome_count, subpop2_total_genome_count;
    
    {
        subpop1_total_genome_count = tallyGUIMutationReferences(selectedSubpopulation1ID_, selectedMutationTypeIndex_);
        
        for (registry_iter = mutationRegistry.begin_pointer_const(); registry_iter != registry_iter_end; ++registry_iter)
        {
            const Mutation *mutation = mut_block_ptr + *registry_iter;
            if (mutation->mutation_type_ptr_->mutation_type_index_ == selectedMutationTypeIndex_)
                refcounts1.push_back(mutation->gui_scratch_reference_count_);
        }
    }
    {
        subpop2_total_genome_count = tallyGUIMutationReferences(selectedSubpopulation2ID_, selectedMutationTypeIndex_);
        
        for (registry_iter = mutationRegistry.begin_pointer_const(); registry_iter != registry_iter_end; ++registry_iter)
        {
            const Mutation *mutation = mut_block_ptr + *registry_iter;
            if (mutation->mutation_type_ptr_->mutation_type_index_ == selectedMutationTypeIndex_)
                refcounts2.push_back(mutation->gui_scratch_reference_count_);
        }
    }
    
    // Tally up the binned 2D SFS from the 1D data
    double *sfs2dbuf = (double *)calloc(histogramBinCount_ * histogramBinCount_, sizeof(double));
    size_t refcounts_size = refcounts1.size();
    
    for (size_t refcount_index = 0; refcount_index < refcounts_size; ++refcount_index)
    {
        slim_refcount_t count1 = refcounts1[refcount_index];
        slim_refcount_t count2 = refcounts2[refcount_index];
        
        double freq1 = count1 / (double)subpop1_total_genome_count;
        double freq2 = count2 / (double)subpop2_total_genome_count;
        int bin1 = static_cast<int>(round(freq1 * (histogramBinCount_ - 1)));
        int bin2 = static_cast<int>(round(freq2 * (histogramBinCount_ - 1)));
        sfs2dbuf[bin1 + bin2 * histogramBinCount_]++;
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
    
    return sfs2dbuf;
}



























