//
//  QtSLiMGraphView_2DSampleSFS.cpp
//  SLiM
//
//  Created by Ben Haller on 8/18/2020.
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

#include "QtSLiMGraphView_2DSampleSFS.h"

#include <QComboBox>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QApplication>
#include <QGuiApplication>
#include <QDebug>

#include "QtSLiMWindow.h"
#include "subpopulation.h"
#include "mutation_type.h"


QtSLiMGraphView_2DSampleSFS::QtSLiMGraphView_2DSampleSFS(QWidget *parent, QtSLiMWindow *controller) : QtSLiMGraphView(parent, controller)
{
    histogramBinCount_ = 21;        // this is the genome sample size + 1
    allowBinCountRescale_ = false;
    
    xAxisMin_ = -1;                 // zero is included, unlike the 1D plot
    xAxisMax_ = histogramBinCount_ - 1;
    xAxisHistogramStyle_ = true;
    xAxisTickValuePrecision_ = 0;
    
    yAxisMin_ = -1;                 // zero is included, unlike the 1D plot
    yAxisMax_ = histogramBinCount_ - 1;
    yAxisHistogramStyle_ = true;
    yAxisTickValuePrecision_ = 0;
    
    zAxisMax_ = 1000;   // 10^3
    
    heatmapMargins_ = 0;
    allowHeatmapMarginsChange_ = true;
    
    xAxisLabel_ = "Count in p1 sample";
    yAxisLabel_ = "Count in p2 sample";
    
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

void QtSLiMGraphView_2DSampleSFS::addedToWindow(void)
{
    // Make our pop-up menu buttons
    QHBoxLayout *layout = buttonLayout();
    
    if (layout)
    {
        subpopulation1Button_ = newButtonInLayout(layout);
        connect(subpopulation1Button_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_2DSampleSFS::subpopulation1PopupChanged);
        
        subpopulation2Button_ = newButtonInLayout(layout);
        connect(subpopulation2Button_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_2DSampleSFS::subpopulation2PopupChanged);
        
        mutationTypeButton_ = newButtonInLayout(layout);
        connect(mutationTypeButton_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_2DSampleSFS::mutationTypePopupChanged);
        
        addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
        addSubpopulationsToMenu(subpopulation2Button_, selectedSubpopulation2ID_);
        addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
    }
}

QtSLiMGraphView_2DSampleSFS::~QtSLiMGraphView_2DSampleSFS()
{
}

void QtSLiMGraphView_2DSampleSFS::subpopulation1PopupChanged(int /* index */)
{
    slim_objectid_t newSubpopID = SLiMClampToObjectidType(subpopulation1Button_->currentData().toInt());
    
    // don't react to non-changes and changes during rebuilds
    if (!rebuildingMenu_ && (selectedSubpopulation1ID_ != newSubpopID))
    {
        selectedSubpopulation1ID_ = newSubpopID;
        xAxisLabel_ = QString("Count in p%1 sample").arg(selectedSubpopulation1ID_);
        invalidateDrawingCache();
        update();
    }
}

void QtSLiMGraphView_2DSampleSFS::subpopulation2PopupChanged(int /* index */)
{
    slim_objectid_t newSubpopID = SLiMClampToObjectidType(subpopulation2Button_->currentData().toInt());
    
    // don't react to non-changes and changes during rebuilds
    if (!rebuildingMenu_ && (selectedSubpopulation2ID_ != newSubpopID))
    {
        selectedSubpopulation2ID_ = newSubpopID;
        yAxisLabel_ = QString("Count in p%1 sample").arg(selectedSubpopulation2ID_);
        invalidateDrawingCache();
        update();
    }
}

void QtSLiMGraphView_2DSampleSFS::mutationTypePopupChanged(int /* index */)
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

void QtSLiMGraphView_2DSampleSFS::controllerRecycled(void)
{
	if (!controller_->invalidSimulation())
		update();
	
    // Remake our popups, whether or not the controller is valid
    addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
    addSubpopulationsToMenu(subpopulation2Button_, selectedSubpopulation2ID_);
	addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
    
	QtSLiMGraphView::controllerRecycled();
}

QString QtSLiMGraphView_2DSampleSFS::graphTitle(void)
{
    return "2D Sample SFS";
}

void QtSLiMGraphView_2DSampleSFS::invalidateDrawingCache(void)
{
    if (sfs2dbuf_)
    {
        free(sfs2dbuf_);
        sfs2dbuf_ = nullptr;
    }
    
    QtSLiMGraphView::invalidateDrawingCache();
}

void QtSLiMGraphView_2DSampleSFS::updateAfterTick(void)
{
    // Rebuild the subpop and muttype menus; this has the side effect of checking and fixing our selections, and that,
	// in turn, will have the side effect of invaliding our cache and fetching new data if needed
    addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
    addSubpopulationsToMenu(subpopulation2Button_, selectedSubpopulation2ID_, selectedSubpopulation1ID_);
	addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
	
    invalidateDrawingCache();
	QtSLiMGraphView::updateAfterTick();
}

void QtSLiMGraphView_2DSampleSFS::drawGraph(QPainter &painter, QRect interiorRect)
{
    uint64_t *sfs2dbuf = mutation2DSFS();
    
    if (sfs2dbuf)
    {
        // plot our histogram bars
        double *sfsTransformed = (double *)calloc(histogramBinCount_ * histogramBinCount_, sizeof(double));
        double logZMax = log10(zAxisMax_);
        
        for (int i = 0; i < histogramBinCount_ * histogramBinCount_; ++i)
        {
            uint64_t value = sfs2dbuf[i];
            
            sfsTransformed[i] = (value == 0) ? -1000 : (log10(value) / logZMax);
        }
        
        drawHeatmap(painter, interiorRect, sfsTransformed, histogramBinCount_, histogramBinCount_);
        
        free(sfsTransformed);
    }
}

bool QtSLiMGraphView_2DSampleSFS::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_2DSampleSFS::appendStringForData(QString &string)
{
	uint64_t *plotData = mutation2DSFS();
	
    for (int y = 0; y < histogramBinCount_; ++y)
    {
        for (int x = 0; x < histogramBinCount_; ++x)
            string.append(QString("%1, ").arg(plotData[x + y * histogramBinCount_]));
        string.append("\n");
    }
}

void QtSLiMGraphView_2DSampleSFS::changeZAxisScale(void)
{
    QStringList choices = QtSLiMRunLineEditArrayDialog(window(), "Choose a z-axis maximum:",
                                                       QStringList{"Maximum value:"},
                                                       QStringList{QString::number(zAxisMax_)});
    
    if (choices.length() == 1)
    {
        int newZMax = choices[0].toInt();
        
        if ((newZMax >= 10) && (newZMax <= 1000000))
        {
            zAxisMax_ = (double)newZMax;
            
            invalidateDrawingCache();
            update();
        }
        else qApp->beep();
    }
}

void QtSLiMGraphView_2DSampleSFS::changeSampleSize(void)
{
    // Similar to "Change Bin Count...", just different branding
    QStringList choices = QtSLiMRunLineEditArrayDialog(window(), "Choose a sample size:", QStringList("Sample size:"), QStringList(QString::number(histogramBinCount_ - 1)));
    
    if (choices.length() == 1)
    {
        int newSampleSize = choices[0].toInt();
        
        if ((newSampleSize > 1) && (newSampleSize <= 500))
        {
            histogramBinCount_ = newSampleSize + 1;
            xAxisMax_ = histogramBinCount_ - 1;
            yAxisMax_ = histogramBinCount_ - 1;
            invalidateDrawingCache();
            update();
        }
        else qApp->beep();
    }
}

void QtSLiMGraphView_2DSampleSFS::subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent * /* event */)
{
    contextMenu.addAction("Change Z Axis Scale...", this, &QtSLiMGraphView_2DSampleSFS::changeZAxisScale);
    contextMenu.addAction("Change Sample Size...", this, &QtSLiMGraphView_2DSampleSFS::changeSampleSize);
}

uint64_t *QtSLiMGraphView_2DSampleSFS::mutation2DSFS(void)
{
    if (!sfs2dbuf_)
    {
        SLiMSim *sim = controller_->sim;
        Population &population = sim->population_;
        MutationRun &mutationRegistry = population.mutation_registry_;
        Mutation *mut_block_ptr = gSLiM_Mutation_Block;
        const MutationIndex *registry_iter = mutationRegistry.begin_pointer_const();
        const MutationIndex *registry_iter_end = mutationRegistry.end_pointer_const();
        
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
        
        // Get frequencies for a sample taken from subpop1
        std::vector<slim_refcount_t> refcounts1;
        std::vector<Genome *> sample1Genomes = subpop1->CurrentGenomes();
        Eidos_random_unique(sample1Genomes.begin(), sample1Genomes.end(), histogramBinCount_ - 1);
        sample1Genomes.resize(histogramBinCount_ - 1);
        
        tallyGUIMutationReferences(sample1Genomes, selectedMutationTypeIndex_);
        
        for (registry_iter = mutationRegistry.begin_pointer_const(); registry_iter != registry_iter_end; ++registry_iter)
        {
            const Mutation *mutation = mut_block_ptr + *registry_iter;
            if (mutation->mutation_type_ptr_->mutation_type_index_ == selectedMutationTypeIndex_)
                refcounts1.push_back(mutation->gui_scratch_reference_count_);
        }
        
        // Get frequencies for a sample taken from subpop2
        std::vector<slim_refcount_t> refcounts2;
        std::vector<Genome *> sample2Genomes = subpop2->CurrentGenomes();
        Eidos_random_unique(sample2Genomes.begin(), sample2Genomes.end(), histogramBinCount_ - 1);
        sample2Genomes.resize(histogramBinCount_ - 1);
        
        tallyGUIMutationReferences(sample2Genomes, selectedMutationTypeIndex_);
        
        for (registry_iter = mutationRegistry.begin_pointer_const(); registry_iter != registry_iter_end; ++registry_iter)
        {
            const Mutation *mutation = mut_block_ptr + *registry_iter;
            if (mutation->mutation_type_ptr_->mutation_type_index_ == selectedMutationTypeIndex_)
                refcounts2.push_back(mutation->gui_scratch_reference_count_);
        }
        
        // Tally up the binned 2D SFS from the 1D data
        sfs2dbuf_ = (uint64_t *)calloc(histogramBinCount_ * histogramBinCount_, sizeof(uint64_t));
        size_t refcounts_size = refcounts1.size();
        
        for (size_t refcount_index = 0; refcount_index < refcounts_size; ++refcount_index)
        {
            slim_refcount_t mutationRefCount1 = refcounts1[refcount_index];
            slim_refcount_t mutationRefCount2 = refcounts2[refcount_index];
            
            if ((mutationRefCount1 > 0) || (mutationRefCount2 > 0))   // exclude mutations not present in either sample
                sfs2dbuf_[mutationRefCount1 + mutationRefCount2 * histogramBinCount_]++;
        }
    }
    
    // Return the final tally; note that we retain ownership of this buffer and the caller should not free it!
    return sfs2dbuf_;
}



























