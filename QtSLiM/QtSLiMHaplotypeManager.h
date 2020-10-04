//
//  QtSLiMHaplotypeManager.h
//  SLiM
//
//  Created by Ben Haller on 4/3/2020.
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

#ifndef QTSLIMHAPLOTYPEMANAGER_H
#define QTSLIMHAPLOTYPEMANAGER_H

// Silence deprecated OpenGL warnings on macOS
#define GL_SILENCE_DEPRECATION

#include <QObject>
#include <QRect>
#include <QString>
#include <QWidget>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

#include "slim_globals.h"
#include "mutation.h"

class QtSLiMWindow;
class Genome;
class QtSLiMHaplotypeProgress;


class QtSLiMHaplotypeManager : public QObject
{
    Q_OBJECT
    
public:
    enum ClusteringMethod {
        ClusterNearestNeighbor,
        ClusterGreedy
    };
    enum ClusteringOptimization {
        ClusterNoOptimization,
        ClusterOptimizeWith2opt
    };
    
    // This class method runs a plot options dialog, and then produces a haplotype plot with a progress panel as it is being constructed
    static void CreateHaplotypePlot(QtSLiMWindow *controller);
    
    // Constructing a QtSLiMHaplotypeManager directly is also allowing, if you don't want options or progress
    QtSLiMHaplotypeManager(QObject *parent, ClusteringMethod clusteringMethod, ClusteringOptimization optimizationMethod,
                           QtSLiMWindow *controller, size_t sampleSize, bool showProgress);
    ~QtSLiMHaplotypeManager(void);
    
    void glDrawHaplotypes(QRect interior, bool displayBW, bool showSubpopStrips, bool eraseBackground, int64_t **previousFirstBincounts);
    
    // Public properties
    QString titleString;
    int subpopCount = 0;
    bool valid_ = true;     // set to false if the user cancels the progress panel

private:
    QtSLiMWindow *controller_ = nullptr;
    QtSLiMHaplotypeProgress *progressPanel_ = nullptr;
    
    ClusteringMethod clusterMethod;
	ClusteringOptimization clusterOptimization;
    
    // Display list data structures.  We map every Mutation in the registry to a struct we define that keeps the necessary information
    // to display that mutation: position and color.  We use MutationIndex to index into a vector of those structs, using the same
    // index values used by the registry for simplicity.  Each genome is then turned into a vector of MutationIndex that lets
    // us plot the mutations for that genome.
    struct HaploMutation {
        slim_position_t position_;
        float red_, green_, blue_;
        bool neutral_;					// selection_coeff_ == 0.0, used to display neutral mutations under selected mutations
        bool display_;					// from the mutation type's mutation_type_displayed_ flag
    };
    
    // Genomes: note that this vector points back into SLiM's data structures, so using it is not safe in general.  It is used
	// by this class only while building the display list below; after that stage, we clear this vector.  The work to build the
	// display list gets done on a background thread, but the SLiMgui window is blocked by the progress panel during that time.
	std::vector<Genome *> genomes;
	
	// Display list
	HaploMutation *mutationInfo = nullptr;                  // a buffer of SLiMHaploMutation providing display information per mutation
	slim_position_t *mutationPositions = nullptr;           // the same info as in mutationInfo, but in a single buffer for access efficiency
	slim_position_t mutationLastPosition = 0;               // from the chromosome
	size_t mutationIndexCount = 0;                          // the number of MutationIndex values in use
	std::vector<std::vector<MutationIndex>> *displayList = nullptr;	// a vector of genome information, where each genome is a vector of MutationIndex
	
	// Subpopulation information
	std::vector<slim_objectid_t> genomeSubpopIDs;			// the subpop ID for each genome, corresponding to the display list order
	slim_objectid_t maxSubpopID = 0;
	slim_objectid_t minSubpopID = 0;
	
	// Chromosome subrange information
	bool usingSubrange = false;
	slim_position_t subrangeFirstBase = 0, subrangeLastBase = 0;
	
	// Mutation type display information
	bool displayingMuttypeSubset = false;
    
    void finishClusteringAnalysis(void);
    void configureMutationInfoBuffer(void);
    void sortGenomes(void);
    void configureDisplayBuffers(void);
    void allocateGLBuffers(void);
    void drawSubpopStripsInRect(QRect interior);
    void tallyBincounts(int64_t *bincounts, std::vector<MutationIndex> &genomeList);
    int64_t distanceForBincounts(int64_t *bincounts1, int64_t *bincounts2);
    void drawDisplayListInRect(QRect interior, bool displayBW, int64_t **previousFirstBincounts);
    
    int64_t *buildDistanceArray(void);
    int64_t *buildDistanceArrayForSubrange(void);
    int64_t *buildDistanceArrayForSubtypes(void);
    int64_t *buildDistanceArrayForSubrangeAndSubtypes(void);
    int indexOfMostIsolatedGenomeWithDistances(int64_t *distances, size_t genome_count);
    void nearestNeighborSolve(int64_t *distances, size_t genome_count, std::vector<int> &solution);
    void greedySolve(int64_t *distances, size_t genome_count, std::vector<int> &solution);
    bool checkPath(std::vector<int> &path, size_t genome_count);
    int64_t lengthOfPath(std::vector<int> &path, int64_t *distances, size_t genome_count);
    void do2optOptimizationOfSolution(std::vector<int> &path, int64_t *distances, size_t genome_count);
};


//
// QtSLiMHaplotypeView
//
// This class is private to QtSLiMHaplotypeManager, but is declared here so MOC gets it automatically
//

class QtSLiMHaplotypeView : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
    
public:
    explicit QtSLiMHaplotypeView(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    virtual ~QtSLiMHaplotypeView(void) override;
    
    void setDelegate(QtSLiMHaplotypeManager *delegate) { delegate_ = delegate; delegate_->setParent(this); }
    
private:
    QtSLiMHaplotypeManager *delegate_ = nullptr;
    
    bool displayBlackAndWhite_ = false;
    bool showSubpopulationStrips_ = false;
    
    virtual void initializeGL() override;
    virtual void resizeGL(int w, int h) override;
    virtual void paintGL() override;
    
    virtual void contextMenuEvent(QContextMenuEvent *event) override;
};


#endif // QTSLIMHAPLOTYPEMANAGER_H




































