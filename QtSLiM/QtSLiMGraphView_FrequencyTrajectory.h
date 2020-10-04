//
//  QtSLiMGraphView_FrequencyTrajectory.h
//  SLiM
//
//  Created by Ben Haller on 4/1/2020.
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

#ifndef QTSLIMGRAPHVIEW_FREQUENCYTRAJECTORY_H
#define QTSLIMGRAPHVIEW_FREQUENCYTRAJECTORY_H

#include <QWidget>
#include <unordered_map>
#include <vector>

#include "QtSLiMGraphView.h"
#include "mutation.h"
#include "mutation_type.h"

class MutationFrequencyHistory;


class QtSLiMGraphView_FrequencyTrajectory : public QtSLiMGraphView
{
    Q_OBJECT
    
public:
    QtSLiMGraphView_FrequencyTrajectory(QWidget *parent, QtSLiMWindow *controller);
    virtual ~QtSLiMGraphView_FrequencyTrajectory() override;
    
    virtual QString graphTitle(void) override;
    virtual QString aboutString(void) override;
    virtual void drawGraph(QPainter &painter, QRect interiorRect) override;
    virtual bool providesStringForData(void) override;
    virtual void appendStringForData(QString &string) override;    
    virtual void subclassAddItemsToMenu(QMenu &contextMenu, QContextMenuEvent *event) override;
    virtual QString disableMessage(void) override;
    
public slots:
    virtual void addedToWindow(void) override;
    void invalidateCachedData(void);
    virtual void controllerRecycled(void) override;
    virtual void controllerGenerationFinished(void) override;
    virtual void updateAfterTick(void) override;
    void toggleShowLostMutations(void);
    void toggleShowFixedMutations(void);
    void toggleShowActiveMutations(void);
    void toggleUseColorsForPlotting(void);
    void subpopulationPopupChanged(int index);
    void mutationTypePopupChanged(int index);
    
protected:
    virtual QtSLiMLegendSpec legendKey(void) override;    
    
private:
    // Mutation history storage
    std::unordered_map<slim_mutationid_t, MutationFrequencyHistory *> frequencyHistoryDict_;    // unordered_map of active MutationFrequencyHistory objects, with slim_mutationid_t keys
    std::vector<MutationFrequencyHistory *> frequencyHistoryColdStorageLost_;                   // vector of MutationFrequencyHistory objects that have been lost
    std::vector<MutationFrequencyHistory *> frequencyHistoryColdStorageFixed_;                  // vector of MutationFrequencyHistory objects that have been fixed
    slim_generation_t lastGeneration_ = 0;                                                      // the last generation data was gathered for; used to detect a backward move in time
    
    // pop-up menu buttons
    QComboBox *subpopulationButton_ = nullptr;
	QComboBox *mutationTypeButton_ = nullptr;
    
    // The subpop and mutation type selected; -1 indicates no current selection (which will be fixed as soon as the menu is populated)
    slim_objectid_t selectedSubpopulationID_;
    int selectedMutationTypeIndex_;
    
    // User-selected display prefs
    bool plotLostMutations_ = false;
    bool plotFixedMutations_ = false;
    bool plotActiveMutations_ = false;
    bool useColorsForPlotting_ = false;
    
    void fetchDataForFinishedGeneration(void);
    void drawHistory(QPainter &painter, MutationFrequencyHistory *history, QRect interiorRect);
    void appendEntriesToString(std::vector<MutationFrequencyHistory *> &array, QString &string, slim_generation_t completedGenerations);
};


// We want to keep a history of frequency values for each mutation of the chosen mutation type in the
// chosen subpopulation.  The history of a mutation should persist after it has vanished, and if a
// new mutation object gets allocated at the same memory location, it should be treated as a distinct
// mutation; so we can't use pointers to identify mutations.  Instead, we keep data on them using a
// unique 64-bit ID generated only when SLiM is running under SLiMgui.  At the end of a generation,
// we loop through all mutations in the registry, and add an entry for that mutation in our data store.
// This is probably O(n^2), but so it goes.  It should only be used for mutation types that generate
// few mutations; if somebody tries to plot every mutation in a common mutation-type, they will suffer.

class MutationFrequencyHistory
{
public:
	// The 64-bit mutation ID is how we keep track of the mutation we reference; its pointer might go stale and be reused.
	slim_mutationid_t mutationID;
	
    // We keep a flag that we use to figure out if our mutation is dead; if it is, we can be moved into cold storage.
	bool updated;
    
	// Mostly we are just a malloced array of uint16s.  The data we're storing is doubles, conceptually, but to minimize our memory footprint
	// (which might be very large!) we convert the doubles, which are guaranteed to be in the range [0.0, 1.0], to uint16s in the range
	// [0, UINT16_MAX] (65535).  The buffer size is the number of entries allocated, the entry count is the number used so far, and the
	// base generation is the first generation recorded; the assumption is that entries are then sequential without gaps.
	uint32_t bufferSize, entryCount;
	slim_generation_t baseGeneration;
	uint16_t *entries;
	
	// Remember our mutation type so we can set our line color, etc., if we wish
	const MutationType *mutationType;
	
public:
    MutationFrequencyHistory(uint16_t value, const Mutation *mutation, slim_generation_t generation);
    ~MutationFrequencyHistory();
    
    inline void addEntry(uint16_t value)
    {
        if (entryCount == bufferSize)
        {
            // We need to expand
            bufferSize += 1024;
            entries = static_cast<uint16_t *>(realloc(entries, bufferSize * sizeof(uint16_t)));
        }
        
        entries[entryCount++] = value;
        updated = true;
    }
};


#endif // QTSLIMGRAPHVIEW_FREQUENCYTRAJECTORY_H





































