//
//  QtSLiMGraphView_FrequencyTrajectory.cpp
//  SLiM
//
//  Created by Ben Haller on 4/1/2020.
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

#include "QtSLiMGraphView_FrequencyTrajectory.h"

#include <QAction>
#include <QMenu>
#include <QPainterPath>
#include <QComboBox>
#include <QDebug>

#include <string>
#include <vector>

#include "QtSLiMWindow.h"
#include "subpopulation.h"


QtSLiMGraphView_FrequencyTrajectory::QtSLiMGraphView_FrequencyTrajectory(QWidget *p_parent, QtSLiMWindow *controller) : QtSLiMGraphView(p_parent, controller)
{
    setXAxisRangeFromTick();
    
    xAxisLabel_ = "Tick";
    yAxisLabel_ = "Frequency";
    
    allowXAxisUserRescale_ = true;
    allowYAxisUserRescale_ = true;
    
    showHorizontalGridLines_ = true;
    tweakXAxisTickLabelAlignment_ = true;
    
    // Start with no selected subpop or mutation-type; these will be set to the first menu item added when menus are constructed
    selectedSubpopulationID_ = -1;
    selectedMutationTypeIndex_ = -1;
    
    // Start plotting lost mutations, by default
    plotLostMutations_ = true;
    plotFixedMutations_ = true;
    plotActiveMutations_ = true;
    useColorsForPlotting_ = true;
}

void QtSLiMGraphView_FrequencyTrajectory::addedToWindow(void)
{
    // Make our pop-up menu buttons
    QHBoxLayout *button_layout = buttonLayout();
    
    if (button_layout)
    {
        subpopulationButton_ = newButtonInLayout(button_layout);
        connect(subpopulationButton_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_FrequencyTrajectory::subpopulationPopupChanged);
        
        mutationTypeButton_ = newButtonInLayout(button_layout);
        connect(mutationTypeButton_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtSLiMGraphView_FrequencyTrajectory::mutationTypePopupChanged);
        
        addSubpopulationsToMenu(subpopulationButton_, selectedSubpopulationID_);
        addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
    }
}

QtSLiMGraphView_FrequencyTrajectory::~QtSLiMGraphView_FrequencyTrajectory()
{
    // We are responsible for our own destruction
    QtSLiMGraphView_FrequencyTrajectory::invalidateCachedData();
}

void QtSLiMGraphView_FrequencyTrajectory::invalidateCachedData(void)
{
    // first free all the MutationFrequencyHistory objects we've stored
    for (auto &item : frequencyHistoryDict_)
        delete item.second;
    
    for (auto &item : frequencyHistoryColdStorageLost_)
        delete item;
    
    for (auto &item : frequencyHistoryColdStorageFixed_)
        delete item;
    
    // then clear out the storage
    frequencyHistoryDict_.clear();
    frequencyHistoryColdStorageLost_.clear();
    frequencyHistoryColdStorageFixed_.clear();
}

void QtSLiMGraphView_FrequencyTrajectory::fetchDataForFinishedTick(void)
{
    Community *community = controller_->community;
    Species *graphSpecies = focalDisplaySpecies();
    
    if (!graphSpecies)
        return;
    
    Population &population = graphSpecies->population_;
    int registry_size;
    const MutationIndex *registry = population.MutationRegistry(&registry_size);
    const MutationIndex *registry_iter_end = registry + registry_size;
    
    if (population.child_generation_valid_)
    {
        qDebug() << "child_generation_valid_ set in fetchDataForFinishedTick";
        return;
    }
    
    // Check that the subpop and muttype we're supposed to be surveying exists; if not, bail.
    bool hasSubpop = true, hasMuttype = true;
    
    if (!graphSpecies->SubpopulationWithID(selectedSubpopulationID_))
        hasSubpop = addSubpopulationsToMenu(subpopulationButton_, selectedSubpopulationID_);
    if (!graphSpecies->MutationTypeWithIndex(selectedMutationTypeIndex_))
        hasMuttype = addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
    if (!hasSubpop || !hasMuttype)
        return;
    
    // Start by zeroing out the "updated" flags; this is how we find dead mutations
    for (auto &pair_ref : frequencyHistoryDict_)
        pair_ref.second->updated = false;
    
    // Tally reference counts within selectedSubpopulationID_
    size_t subpop_total_genome_count = tallyGUIMutationReferences(selectedSubpopulationID_, selectedMutationTypeIndex_);
    
    if (subpop_total_genome_count == 0)
        subpop_total_genome_count = 1;  // refcounts will all be zero; prevent NAN values below, make them 0 instead
    
    // Now we can run through the mutations and use the tallies in gui_scratch_reference_count to update our histories
    Mutation *mut_block_ptr = gSLiM_Mutation_Block;
    
    for (const MutationIndex *registry_iter = registry; registry_iter != registry_iter_end; ++registry_iter)
    {
        const Mutation *mutation = mut_block_ptr + *registry_iter;
        slim_refcount_t refcount = mutation->gui_scratch_reference_count_;
        
        if (refcount)
        {
            uint16_t value = static_cast<uint16_t>((static_cast<size_t>(refcount) * static_cast<size_t>(UINT16_MAX)) / subpop_total_genome_count);
            slim_mutationid_t mutationID = mutation->mutation_id_;
            auto history_iter = frequencyHistoryDict_.find(mutationID);
            
            //NSLog(@"mutation refcount %d has uint16_t value %d, found history %p for id %lld", refcount, value, history, (long long int)mutation->mutation_id_);
            
            if (history_iter != frequencyHistoryDict_.end())
            {
                // We have a history for this mutation, so we just need to add an entry; this sets the updated flag
                MutationFrequencyHistory *history = history_iter->second;
                
                history->addEntry(value);
            }
            else
            {
                // No history, so we make one starting at this tick; this also sets the updated flag
                // Note we use community->Tick() - 1, because the tick counter has already been advanced to the next tick
                MutationFrequencyHistory *history = new MutationFrequencyHistory(value, mutation, community->Tick() - 1);
                
                frequencyHistoryDict_.emplace(mutationID, history);
            }
        }
    }
    
    // OK, now every mutation that has frequency >0 in our subpop has got a current entry.  But what about mutations that used to circulate,
    // but don't any more?  These could still be active in a different subpop, or they might be gone – lost or fixed.  For the former case,
    // we need to add an entry with frequency zero.  For the latter case, we need to put their history into "cold storage" for efficiency.
    std::vector<MutationFrequencyHistory *> historiesToAddToColdStorage;
    
    for (auto entry_iter : frequencyHistoryDict_)
    {
        MutationFrequencyHistory *history = entry_iter.second;
        
        if (!history->updated)
        {
            slim_mutationid_t historyID = history->mutationID;
            bool mutationStillExists = false;
            
            for (const MutationIndex *mutation_iter = registry; mutation_iter != registry_iter_end; ++mutation_iter)
            {
                const Mutation *mutation = mut_block_ptr + *mutation_iter;
                slim_mutationid_t mutationID = mutation->mutation_id_;
                
                if (historyID == mutationID)
                {
                    mutationStillExists = true;
                    break;
                }
            }
            
            if (mutationStillExists)
            {
                // The mutation is still around, so just add a zero entry for it
                history->addEntry(0);
            }
            else
            {
                // The mutation is gone, so we need to put its history into cold storage, but we can't modify
                // our dictionary since we are enumerating it, so we just make a record and do it below
                historiesToAddToColdStorage.emplace_back(history);
            }
        }
    }
    
    // Now, if historiesToAddToColdStorage is non-nil, we have histories to put into cold storage; do it now
    for (MutationFrequencyHistory *history : historiesToAddToColdStorage)
    {
        // The remaining tricky bit is that we have to figure out whether the vanished mutation was fixed or lost; we do this by
        // scanning through all our Substitution objects, which use the same unique IDs as Mutations use.  We need to know this
        // for two reasons: to add the final entry for the mutation, and to put it into the correct cold storage array.
        slim_mutationid_t mutationID = history->mutationID;
        bool wasFixed = false;
        
        std::vector<Substitution*> &substitutions = population.substitutions_;
        
        for (const Substitution *substitution : substitutions)
        {
            if (substitution->mutation_id_ == mutationID)
            {
                wasFixed = true;
                break;
            }
        }
        
        if (wasFixed)
        {
            history->addEntry(UINT16_MAX);
            frequencyHistoryColdStorageFixed_.emplace_back(history);
        }
        else
        {
            history->addEntry(0);
            frequencyHistoryColdStorageLost_.emplace_back(history);
        }
        
        auto history_iter = frequencyHistoryDict_.find(mutationID);
        frequencyHistoryDict_.erase(history_iter);
    }
    
    //NSLog(@"frequencyHistoryDict has %lld entries, frequencyHistoryColdStorageLost has %lld entries, frequencyHistoryColdStorageFixed has %lld entries", (long long int)[frequencyHistoryDict count], (long long int)[frequencyHistoryColdStorageLost count], (long long int)[frequencyHistoryColdStorageFixed count]);
    
    lastTick_ = community->Tick();
}

void QtSLiMGraphView_FrequencyTrajectory::subpopulationPopupChanged(int /* index */)
{
    slim_objectid_t newSubpopID = SLiMClampToObjectidType(subpopulationButton_->currentData().toInt());
    
    // don't react to non-changes and changes during rebuilds
    if (!rebuildingMenu_ && (selectedSubpopulationID_ != newSubpopID))
    {
        selectedSubpopulationID_ = newSubpopID;
        invalidateCachedData();
        fetchDataForFinishedTick();
        update();
    }
}

void QtSLiMGraphView_FrequencyTrajectory::mutationTypePopupChanged(int /* index */)
{
    int newMutTypeIndex = mutationTypeButton_->currentData().toInt();
    
    // don't react to non-changes and changes during rebuilds
    if (!rebuildingMenu_ && (selectedMutationTypeIndex_ != newMutTypeIndex))
    {
        selectedMutationTypeIndex_ = newMutTypeIndex;
        invalidateCachedData();
        fetchDataForFinishedTick();
        update();
    }
}

void QtSLiMGraphView_FrequencyTrajectory::controllerRecycled(void)
{
	if (!controller_->invalidSimulation())
	{
		if (!xAxisIsUserRescaled_)
			setXAxisRangeFromTick();
		
		update();
	}
	
    // Remake our popups, whether or not the controller is valid
	invalidateCachedData();
	addSubpopulationsToMenu(subpopulationButton_, selectedSubpopulationID_);
	addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
    
	QtSLiMGraphView::controllerRecycled();
}

void QtSLiMGraphView_FrequencyTrajectory::controllerTickFinished(void)
{
    QtSLiMGraphView::controllerTickFinished();
    
    // Check for an unexpected change in Tick(), in which case we invalidate all our histories and start over
    Community *community = controller_->community;
	
	if (lastTick_ != community->Tick() - 1)
	{
        invalidateCachedData();
        update();
    }
	
	// Fetch and store the frequencies for all mutations of the selected mutation type(s), within the subpopulation selected
    fetchDataForFinishedTick();
}

QString QtSLiMGraphView_FrequencyTrajectory::graphTitle(void)
{
    return "Mutation Frequency Trajectories";
}

QString QtSLiMGraphView_FrequencyTrajectory::aboutString(void)
{
    return "The Mutation Frequency Trajectories graph shows historical trajectories of mutation "
           "frequencies over time, within a given subpopulation and for a given "
           "mutation type.  Color represents whether a given mutation was "
           "lost (red), fixed population-wide and substituted by SLiM (blue), or is "
           "still segregating (black).  These categories can be separately enabled "
           "or disabled in the action menu.  Because of the large amount of data "
           "recorded for this graph, recording is only enabled when the graph is "
           "open, and only for the selected subpopulation and mutation type; to "
           "fill in missing data, it is necessary to recycle and run when the graph "
           "window is already open and configured as desired.";
}

void QtSLiMGraphView_FrequencyTrajectory::updateAfterTick(void)
{
    // Rebuild the subpop and muttype menus; this has the side effect of checking and fixing our selections, and that,
	// in turn, will have the side effect of invaliding our cache and fetching new data if needed
	addSubpopulationsToMenu(subpopulationButton_, selectedSubpopulationID_);
	addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
	
	QtSLiMGraphView::updateAfterTick();
}

QString QtSLiMGraphView_FrequencyTrajectory::disableMessage(void)
{
    Species *graphSpecies = focalDisplaySpecies();
    
    if (graphSpecies)
    {
        bool hasSubpop = true, hasMuttype = true;
        
        if (!graphSpecies->SubpopulationWithID(selectedSubpopulationID_))
            hasSubpop = addSubpopulationsToMenu(subpopulationButton_, selectedSubpopulationID_);
        if (!graphSpecies->MutationTypeWithIndex(selectedMutationTypeIndex_))
            hasMuttype = addMutationTypesToMenu(mutationTypeButton_, selectedMutationTypeIndex_);
        if (!hasSubpop || !hasMuttype)
            return "no\ndata";
    }
    
    return "";
}

void QtSLiMGraphView_FrequencyTrajectory::drawHistory(QPainter &painter, MutationFrequencyHistory *history, QRect interiorRect)
{
    size_t entryCount = history->entryCount;
	
	if (entryCount > 1)		// one entry just generates a moveto
	{
		uint16_t *entries = history->entries;
        QPainterPath linePath;
		uint16_t firstValue = *entries;
		double firstFrequency = static_cast<double>(firstValue) / UINT16_MAX;
		slim_tick_t tick = history->baseTick;
		QPointF firstPoint(plotToDeviceX(tick, interiorRect), plotToDeviceY(firstFrequency, interiorRect));
		
        linePath.moveTo(firstPoint);
		
		for (size_t entryIndex = 1; entryIndex < entryCount; ++entryIndex)
		{
			uint16_t value = entries[entryIndex];
			double frequency = static_cast<double>(value) / UINT16_MAX;
			QPointF nextPoint(plotToDeviceX(++tick, interiorRect), plotToDeviceY(frequency, interiorRect));
			
            linePath.lineTo(nextPoint);
		}
		
        painter.drawPath(linePath);
	}
}

void QtSLiMGraphView_FrequencyTrajectory::drawGraph(QPainter &painter, QRect interiorRect)
{
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::black, 1.0));
    
	// Go through all our history entries and draw a line for each.  First we draw the ones in cold storage, then the active ones.
	if (plotLostMutations_)
	{
		if (useColorsForPlotting_)
            painter.setPen(QPen(Qt::red, 1.0));
		
        for (MutationFrequencyHistory *history : frequencyHistoryColdStorageLost_)
            drawHistory(painter, history, interiorRect);
	}
	
	if (plotFixedMutations_)
	{
		if (useColorsForPlotting_)
            painter.setPen(QPen(QtSLiMColorWithRGB(0.4, 0.4, 1.0, 1.0), 1.0));
		
        for (MutationFrequencyHistory *history : frequencyHistoryColdStorageFixed_)
            drawHistory(painter, history, interiorRect);
	}
	
	if (plotActiveMutations_)
	{
		if (useColorsForPlotting_)
            painter.setPen(QPen(Qt::black, 1.0));
		
        for (auto history_pair : frequencyHistoryDict_)
            drawHistory(painter, history_pair.second, interiorRect);
	}
}

void QtSLiMGraphView_FrequencyTrajectory::toggleShowLostMutations(void)
{
    plotLostMutations_ = !plotLostMutations_;
    update();
}

void QtSLiMGraphView_FrequencyTrajectory::toggleShowFixedMutations(void)
{
    plotFixedMutations_ = !plotFixedMutations_;
    update();
}

void QtSLiMGraphView_FrequencyTrajectory::toggleShowActiveMutations(void)
{
    plotActiveMutations_ = !plotActiveMutations_;
    update();
}

void QtSLiMGraphView_FrequencyTrajectory::toggleUseColorsForPlotting(void)
{
    useColorsForPlotting_ = !useColorsForPlotting_;
    update();
}

void QtSLiMGraphView_FrequencyTrajectory::subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent * /* event */)
{
    contextMenu.addAction(plotLostMutations_ ? "Hide Lost Mutations" : "Show Lost Mutations", this, &QtSLiMGraphView_FrequencyTrajectory::toggleShowLostMutations);
    contextMenu.addAction(plotFixedMutations_ ? "Hide Fixed Mutations" : "Show Fixed Mutations", this, &QtSLiMGraphView_FrequencyTrajectory::toggleShowFixedMutations);
    contextMenu.addAction(plotActiveMutations_ ? "Hide Active Mutations" : "Show Active Mutations", this, &QtSLiMGraphView_FrequencyTrajectory::toggleShowActiveMutations);
    contextMenu.addSeparator();
    contextMenu.addAction(useColorsForPlotting_ ? "Black Plot Lines" : "Colored Plot Lines", this, &QtSLiMGraphView_FrequencyTrajectory::toggleUseColorsForPlotting);
}

QtSLiMLegendSpec QtSLiMGraphView_FrequencyTrajectory::legendKey(void)
{
	if (!useColorsForPlotting_)
		return QtSLiMLegendSpec();
	
	QtSLiMLegendSpec legend_key;
	
	if (plotLostMutations_)
		legend_key.emplace_back("lost", Qt::red);
	
	if (plotFixedMutations_)
		legend_key.emplace_back("fixed", QtSLiMColorWithRGB(0.4, 0.4, 1.0, 1.0));
	
	if (plotActiveMutations_)
		legend_key.emplace_back("active", Qt::black);
	
	return legend_key;
}

bool QtSLiMGraphView_FrequencyTrajectory::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_FrequencyTrajectory::appendEntriesToString(std::vector<MutationFrequencyHistory *> &array, QString &string, slim_tick_t completedTicks)
{
    for (MutationFrequencyHistory *history : array)
    {
        int entryCount = static_cast<int>(history->entryCount);
		slim_tick_t baseTick = history->baseTick;
		
		for (slim_tick_t tick = 1; tick <= completedTicks; ++tick)
		{
			if (tick < baseTick)
                string.append("NA, ");
			else if (tick - baseTick < entryCount)
                string.append(QString("%1, ").arg(static_cast<double>(history->entries[tick - baseTick]) / UINT16_MAX, 0, 'f', 4));
			else
                string.append("NA, ");
		}
		
        string.append("\n");
    }
}

void QtSLiMGraphView_FrequencyTrajectory::appendStringForData(QString &string)
{
    Community *community = controller_->community;
	slim_tick_t completedTicks = community->Tick() - 1;
	
    if (plotLostMutations_)
	{
		string.append("# Lost mutations:\n");
		appendEntriesToString(frequencyHistoryColdStorageLost_, string, completedTicks);
        string.append("\n\n");
	}
	
	if (plotFixedMutations_)
	{
		string.append("# Fixed mutations:\n");
		appendEntriesToString(frequencyHistoryColdStorageFixed_, string, completedTicks);
        string.append("\n\n");
	}
	
	if (plotActiveMutations_)
	{
		string.append("# Active mutations:\n");
        
        std::vector<MutationFrequencyHistory *> allActive;
        
        for (auto &pair_ref : frequencyHistoryDict_)
            allActive.emplace_back(pair_ref.second);
        
		appendEntriesToString(allActive, string, completedTicks);
        string.append("\n\n");
	}
}


//
//  MutationFrequencyHistory
//

MutationFrequencyHistory::MutationFrequencyHistory(uint16_t value, const Mutation *mutation, slim_tick_t tick)
{
    mutationID = mutation->mutation_id_;
    mutationType = mutation->mutation_type_ptr_;
    baseTick = tick;
    
    bufferSize = 0;
    entryCount = 0;
    entries = nullptr;
    
    addEntry(value);
}

MutationFrequencyHistory::~MutationFrequencyHistory()
{
	if (entries)
	{
		free(entries);
		entries = nullptr;
		
		bufferSize = 0;
		entryCount = 0;
	}
}






























