//
//  QtSLiMGraphView_1DSampleSFS.cpp
//  SLiM
//
//  Created by Ben Haller on 8/20/2020.
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

#include "QtSLiMGraphView_1DSampleSFS.h"

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


QtSLiMGraphView_1DSampleSFS::QtSLiMGraphView_1DSampleSFS(QWidget *parent, QtSLiMWindow *controller) : QtSLiMGraphView(parent, controller)
{
    histogramBinCount_ = 20;        // this is also the genome sample size
    allowBinCountRescale_ = false;
    
    xAxisMin_ = 0;
    xAxisMax_ = histogramBinCount_;
    xAxisHistogramStyle_ = true;
    xAxisTickValuePrecision_ = 0;
    
    yAxisMin_ = -0.05;      // on log scale; we want a frequency of 1 to show slightly above baseline
    yAxisMax_ = 3.0;        // on log scale; maximum power of 10
    yAxisMajorTickInterval_ = 1;
    yAxisMinorTickInterval_ = 1/9.0;
    yAxisMajorTickModulus_ = 9;         // 9 ticks per major; ticks at 1:10 are represented by values 0:9, and 0 and 9 both need to be modulo 0
    yAxisLog_ = true;       // changes positioning of ticks, grid lines, etc.
    
    xAxisLabel_ = "Count in sample";
    yAxisLabel_ = "Number of mutations";
    
    allowXAxisUserRescale_ = false;
    allowYAxisUserRescale_ = true;
    
    showHorizontalGridLines_ = true;
    showGridLinesMajorOnly_ = true;
    allowHorizontalGridChange_ = true;
    allowVerticalGridChange_ = false;
    allowFullBoxChange_ = true;
    
    selectedSubpopulation1ID_ = 1;
    selectedMutationTypeIndex_ = -1;
}

void QtSLiMGraphView_1DSampleSFS::addedToWindow(void)
{
    // Make our pop-up menu buttons
    QHBoxLayout *layout = buttonLayout();
    
    if (layout)
    {
        subpopulation1Button_ = newButtonInLayout(layout);
        connect(subpopulation1Button_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_1DSampleSFS::subpopulation1PopupChanged);
        
        mutationTypeButton_ = newButtonInLayout(layout);
        connect(mutationTypeButton_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_1DSampleSFS::mutationTypePopupChanged);
        
        addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
        addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
    }
}

QtSLiMGraphView_1DSampleSFS::~QtSLiMGraphView_1DSampleSFS()
{
}

void QtSLiMGraphView_1DSampleSFS::subpopulation1PopupChanged(int /* index */)
{
    slim_objectid_t newSubpopID = SLiMClampToObjectidType(subpopulation1Button_->currentData().toInt());
    
    // don't react to non-changes and changes during rebuilds
    if (!rebuildingMenu_ && (selectedSubpopulation1ID_ != newSubpopID))
    {
        selectedSubpopulation1ID_ = newSubpopID;
        invalidateDrawingCache();
        update();
    }
}

void QtSLiMGraphView_1DSampleSFS::mutationTypePopupChanged(int /* index */)
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

void QtSLiMGraphView_1DSampleSFS::controllerRecycled(void)
{
	if (!controller_->invalidSimulation())
		update();
	
    // Remake our popups, whether or not the controller is valid
    addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
	addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
    
	QtSLiMGraphView::controllerRecycled();
}

QString QtSLiMGraphView_1DSampleSFS::graphTitle(void)
{
    return "1D Sample SFS";
}

QString QtSLiMGraphView_1DSampleSFS::aboutString(void)
{
    return "The 1D Sample SFS graph shows a Site Frequency Spectrum (SFS) for a sample of genomes taken "
           "(with replacement) from a given subpopulation, for mutations of a given mutation type.  The x axis "
           "here is the occurrence count of a given mutation within the sample, from 1 to the sample size.  The "
           "y axis is the number of mutations in the sample with that specific occurrence count, on a log "
           "scale.  The y axis range and the sample size can be customized from the action menu.  The 1D "
           "Population SFS graph provides an alternative that might also be useful.";
}

void QtSLiMGraphView_1DSampleSFS::invalidateDrawingCache(void)
{
    if (sfs1dbuf_)
    {
        free(sfs1dbuf_);
        sfs1dbuf_ = nullptr;
    }
    
    QtSLiMGraphView::invalidateDrawingCache();
}

void QtSLiMGraphView_1DSampleSFS::updateAfterTick(void)
{
    // Rebuild the subpop and muttype menus; this has the side effect of checking and fixing our selections, and that,
	// in turn, will have the side effect of invaliding our cache and fetching new data if needed
    addSubpopulationsToMenu(subpopulation1Button_, selectedSubpopulation1ID_);
	addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
	
    invalidateDrawingCache();
	QtSLiMGraphView::updateAfterTick();
}

QString QtSLiMGraphView_1DSampleSFS::disableMessage(void)
{
    if (controller_ && !controller_->invalidSimulation())
    {
        Subpopulation *subpop1 = controller_->sim->SubpopulationWithID(selectedSubpopulation1ID_);
        MutationType *muttype = controller_->sim->MutationTypeWithID(selectedMutationTypeIndex_);
        
        if (!subpop1 || !muttype)
            return "no\ndata";
    }
    
return "";
}

void QtSLiMGraphView_1DSampleSFS::drawGraph(QPainter &painter, QRect interiorRect)
{
	uint64_t *sfs1dbuf = mutation1DSFS();
	
    if (sfs1dbuf)
    {
        // plot our histogram bars
        double *sfsTransformed = (double *)calloc(histogramBinCount_, sizeof(double));
        
        for (int i = 0; i < histogramBinCount_; ++i)
        {
            uint64_t value = sfs1dbuf[i];
            
            sfsTransformed[i] = (value == 0) ? -1000 : log10(value);
        }
        
        drawBarplot(painter, interiorRect, sfsTransformed, histogramBinCount_, 0.0, 1.0);
        
        free(sfsTransformed);
    }
}

bool QtSLiMGraphView_1DSampleSFS::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_1DSampleSFS::appendStringForData(QString &string)
{
	uint64_t *plotData = mutation1DSFS();
	
    for (int i = 0; i < histogramBinCount_; ++i)
        string.append(QString("%1, ").arg(plotData[i]));
    
    string.append("\n");
}

void QtSLiMGraphView_1DSampleSFS::changeSampleSize(void)
{
    // Similar to "Change Bin Count...", just different branding
    QStringList choices = QtSLiMRunLineEditArrayDialog(window(), "Choose a sample size:", QStringList("Sample size:"), QStringList(QString::number(histogramBinCount_)));
    
    if (choices.length() == 1)
    {
        int newSampleSize = choices[0].toInt();
        
        if ((newSampleSize > 1) && (newSampleSize <= 500))
        {
            histogramBinCount_ = newSampleSize;
            xAxisMax_ = histogramBinCount_;
            invalidateDrawingCache();
            update();
        }
        else qApp->beep();
    }
}

void QtSLiMGraphView_1DSampleSFS::subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent * /* event */)
{
    contextMenu.addAction("Change Sample Size...", this, &QtSLiMGraphView_1DSampleSFS::changeSampleSize);
}

uint64_t *QtSLiMGraphView_1DSampleSFS::mutation1DSFS(void)
{
    if (!sfs1dbuf_)
    {
        SLiMSim *sim = controller_->sim;
        Population &population = sim->population_;
        
        // Find our subpops and mutation type
        Subpopulation *subpop1 = sim->SubpopulationWithID(selectedSubpopulation1ID_);
        MutationType *muttype = sim->MutationTypeWithID(selectedMutationTypeIndex_);
        
        if (!subpop1 || !muttype)
            return nullptr;
        
        // Get frequencies for a sample taken (with replacement) from subpop1
        {
            std::vector<Genome *> sampleGenomes;
            std::vector<Genome *> &subpopGenomes = subpop1->CurrentGenomes();
            size_t subpopGenomeCount = subpopGenomes.size();
            
            if (subpopGenomeCount)
                for (int i = 0; i < histogramBinCount_ - 1; ++i)
                    sampleGenomes.push_back(subpopGenomes[random() % subpopGenomeCount]);
            
            tallyGUIMutationReferences(sampleGenomes, selectedMutationTypeIndex_);
        }
        
        // Tally into our bins
        sfs1dbuf_ = static_cast<uint64_t *>(calloc(histogramBinCount_, sizeof(uint64_t)));
        Mutation *mut_block_ptr = gSLiM_Mutation_Block;
        int registry_size;
        const MutationIndex *registry = population.MutationRegistry(&registry_size);
        
        for (int registry_index = 0; registry_index < registry_size; ++registry_index)
        {
            const Mutation *mutation = mut_block_ptr + registry[registry_index];
            slim_refcount_t mutationRefCount = mutation->gui_scratch_reference_count_;
            int mutationBin = mutationRefCount - 1;
            
            if (mutationBin >= 0)           // mutationBin is -1 if the mutation is not present in the sample at all
                (sfs1dbuf_[mutationBin])++;
        }
    }
    
    // Return the final tally; note that we retain ownership of this buffer and only free it when we want to force a recache
	return sfs1dbuf_;
}






























