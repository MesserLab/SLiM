//
//  QtSLiMHaplotypeManager.h
//  SLiM
//
//  Created by Ben Haller on 4/3/2020.
//  Copyright (c) 2020-2024 Philipp Messer.  All rights reserved.
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

#include "QtSLiMHaplotypeManager.h"
#include "QtSLiMChromosomeWidget.h"
#include "QtSLiMHaplotypeOptions.h"
#include "QtSLiMHaplotypeProgress.h"
#include "QtSLiMPreferences.h"
#include "QtSLiMExtras.h"

#include <QOpenGLFunctions>
#include <QDialog>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QFileDialog>
#include <QStandardPaths>
#include <QLabel>
#include <QMessageBox>
#include <QDebug>

#include <vector>
#include <algorithm>
#include <utility>

#include "eidos_globals.h"
#include "subpopulation.h"
#include "species.h"




// This class method runs a plot options dialog, and then produces a haplotype plot with a progress panel as it is being constructed
void QtSLiMHaplotypeManager::CreateHaplotypePlot(QtSLiMChromosomeWidgetController *controller)
{
    QtSLiMWindow *slimWindow = controller->slimWindow();
    
    if (!slimWindow)
        return;
    
    Species *displaySpecies = controller->focalDisplaySpecies();
    
    if (!displaySpecies)
    {
        QMessageBox messageBox(slimWindow);
        messageBox.setText("Haplotype Plot");
        messageBox.setInformativeText("A single species must be chosen to create a haplotype plot; the plot will be based upon the selected species.");
        messageBox.setIcon(QMessageBox::Warning);
        messageBox.setWindowModality(Qt::WindowModal);
        messageBox.exec();
        return;
    }
    
    // We need a single chromosome to work with; QtSLiMHaplotypeManager creates a haplotype
    // plot for one chromosome, which makes sense since haplosomes assort independently.
    // If we can't get a single chromosome, then we tell the user to select a chromosome.
    Chromosome *chromosome = slimWindow->focalChromosome();
    
    if (!chromosome)
    {
        QMessageBox messageBox(slimWindow);
        messageBox.setText("Haplotype Plot");
        messageBox.setInformativeText("A single chromosome must be chosen to create a haplotype plot; the plot will be based upon the selected chromosome.");
        messageBox.setIcon(QMessageBox::Warning);
        messageBox.setWindowModality(Qt::WindowModal);
        messageBox.exec();
        return;
    }
    
    QtSLiMHaplotypeOptions optionsPanel(controller->slimWindow());
    
    int result = optionsPanel.exec();
    
    if (result == QDialog::Accepted)
    {
        size_t haplosomeSampleSize = optionsPanel.haplosomeSampleSize();
        QtSLiMHaplotypeManager::ClusteringMethod clusteringMethod = optionsPanel.clusteringMethod();
        QtSLiMHaplotypeManager::ClusteringOptimization clusteringOptimization = optionsPanel.clusteringOptimization();
        
        // First generate the haplotype plot data, with a progress panel
        QtSLiMHaplotypeManager *haplotypeManager = new QtSLiMHaplotypeManager(nullptr, clusteringMethod, clusteringOptimization, controller,
                                                                              displaySpecies, chromosome, QtSLiMRange(0,0), haplosomeSampleSize, true);
        
        if (haplotypeManager->valid_)
        {
            // Make a new window to show the graph
            QWidget *window = new QWidget(controller->slimWindow(), Qt::Window | Qt::Tool);    // the graph window has us as a parent, but is still a standalone window
            
            window->setWindowTitle(QString("Haplotype snapshot (%1)").arg(haplotypeManager->titleString));
            window->setMinimumSize(400, 200);
            window->resize(500, 400);
#ifdef __APPLE__
            // set the window icon only on macOS; on Linux it changes the app icon as a side effect
            window->setWindowIcon(QIcon());
#endif
            
            // Install the haplotype view in the window
            QtSLiMHaplotypeView *haplotypeView = new QtSLiMHaplotypeView(nullptr);
            QVBoxLayout *topLayout = new QVBoxLayout;
            
            window->setLayout(topLayout);
            topLayout->setContentsMargins(0, 0, 0, 0);
            topLayout->setSpacing(0);
            topLayout->addWidget(haplotypeView);
            
            // The haplotype manager is owned by the graph view, as a delegate object
            haplotypeView->setDelegate(haplotypeManager);
            
            // Add a horizontal layout at the bottom, for the action button, and maybe other cruft, over time
            QHBoxLayout *buttonLayout = nullptr;
            
            {
                buttonLayout = new QHBoxLayout;
                
                buttonLayout->setContentsMargins(5, 5, 5, 5);
                buttonLayout->setSpacing(5);
                topLayout->addLayout(buttonLayout);
                
                if (controller->community()->all_species_.size() > 1)
                {
                    // make our species avatar badge
                    QLabel *speciesLabel = new QLabel();
                    speciesLabel->setText(QString::fromStdString(displaySpecies->avatar_));
                    buttonLayout->addWidget(speciesLabel);
                }
                
                QSpacerItem *rightSpacer = new QSpacerItem(16, 5, QSizePolicy::Expanding, QSizePolicy::Minimum);
                buttonLayout->addItem(rightSpacer);
                
                // this code is based on the creation of executeScriptButton in ui_QtSLiMEidosConsole.h
                QtSLiMPushButton *actionButton = new QtSLiMPushButton(window);
                actionButton->setObjectName(QString::fromUtf8("actionButton"));
                actionButton->setMinimumSize(QSize(20, 20));
                actionButton->setMaximumSize(QSize(20, 20));
                actionButton->setFocusPolicy(Qt::NoFocus);
                QIcon icon4;
                icon4.addFile(QtSLiMImagePath("action", false), QSize(), QIcon::Normal, QIcon::Off);
                icon4.addFile(QtSLiMImagePath("action", true), QSize(), QIcon::Normal, QIcon::On);
                actionButton->setIcon(icon4);
                actionButton->setIconSize(QSize(20, 20));
                actionButton->qtslimSetBaseName("action");
                actionButton->setCheckable(true);
                actionButton->setFlat(true);
#if QT_CONFIG(tooltip)
                actionButton->setToolTip("<html><head/><body><p>configure plot</p></body></html>");
#endif // QT_CONFIG(tooltip)
                buttonLayout->addWidget(actionButton);
                
                connect(actionButton, &QPushButton::pressed, haplotypeView, [actionButton, haplotypeView]() { actionButton->qtslimSetHighlight(true); haplotypeView->actionButtonRunMenu(actionButton); });
                connect(actionButton, &QPushButton::released, haplotypeView, [actionButton]() { actionButton->qtslimSetHighlight(false); });
                
                actionButton->setEnabled(true);
            }
            
            // make window actions for all global menu items
            // we do NOT need to do this, because we use Qt::Tool; Qt will use our parent winodw's shortcuts
            //qtSLiMAppDelegate->addActionsForGlobalMenuItems(window);
            
            // Show the window
            window->show();
            window->raise();
            window->activateWindow();
        }
    }
}

QtSLiMHaplotypeManager::QtSLiMHaplotypeManager(QObject *p_parent, ClusteringMethod clusteringMethod, ClusteringOptimization optimizationMethod,
                                               QtSLiMChromosomeWidgetController *controller, Species *displaySpecies, Chromosome *chromosome,
                                               QtSLiMRange displayedRange, size_t sampleSize, bool showProgress) :
    QObject(p_parent)
{
    controller_ = controller;
    focalSpeciesName_ = displaySpecies->name_;
    
    Community *community = controller_->community();
    Species *graphSpecies = focalDisplaySpecies();
    Population &population = graphSpecies->population_;
    
    clusterMethod = clusteringMethod;
    clusterOptimization = optimizationMethod;
    
    // Figure out which subpops are selected (or if none are, consider all to be); we will display only the selected subpops
    std::vector<Subpopulation *> selected_subpops;
    
    for (auto subpop_pair : population.subpops_)
        if (subpop_pair.second->gui_selected_)
            selected_subpops.emplace_back(subpop_pair.second);
    
    if (selected_subpops.size() == 0)
        for (auto subpop_pair : population.subpops_)
            selected_subpops.emplace_back(subpop_pair.second);
    
    // Figure out whether we're analyzing / displaying a subrange
    usingSubrange = (displayedRange.length == 0) ? false : true;
    subrangeFirstBase = displayedRange.location;
    subrangeLastBase = displayedRange.location + displayedRange.length - 1;
    
    // Also dig to find out whether we're displaying all mutation types or just a subset; if a subset, each MutationType has a display flag
    displayingMuttypeSubset = (controller_->displayMuttypes_.size() != 0);
    
    // Set our window title from the controller's state
    QString title;
    
    if (selected_subpops.size() == 0)
    {
        // If there are no subpops (which can happen at the very start of running a model, for example), use a dash
        title = "–";
    }
    else
    {
        bool first_subpop = true;
        
        for (Subpopulation *subpop : selected_subpops)
        {
            if (!first_subpop)
                title.append(" ");
            
            title.append(QString("p%1").arg(subpop->subpopulation_id_));
            
            first_subpop = false;
        }
    }
    
    if (usingSubrange)
        title.append(QString(", positions %1:%2").arg(subrangeFirstBase).arg(subrangeLastBase));
    
    title.append(QString(", tick %1").arg(community->Tick()));
    
    if (displaySpecies->Chromosomes().size() > 1)
        title.append(QString(", chromosome '%2'").arg(QString::fromStdString(chromosome->Symbol())));
    
    titleString = title;
    subpopCount = static_cast<int>(selected_subpops.size());
    
    // Fetch haplosomes and figure out what we're going to plot; note that we plot only non-null haplosomes
    slim_chromosome_index_t chromosome_index = chromosome->Index();
    int first_haplosome_index = graphSpecies->FirstHaplosomeIndices()[chromosome_index];
    int last_haplosome_index = graphSpecies->LastHaplosomeIndices()[chromosome_index];
    
    for (Subpopulation *subpop : selected_subpops)
    {
        for (Individual *ind : subpop->parent_individuals_)
        {
            Haplosome **ind_haplosomes = ind->haplosomes_;
            
            for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
            {
                Haplosome *haplosome = ind_haplosomes[haplosome_index];
                
                if (!haplosome->IsNull())
                    haplosomes.emplace_back(haplosome);
            }
        }
    }
    
    // If a sample is requested, select that now; sampleSize <= 0 means no sampling
    if ((sampleSize > 0) && (haplosomes.size() > sampleSize))
    {
        Eidos_random_unique(haplosomes.begin(), haplosomes.end(), sampleSize);
		haplosomes.resize(sampleSize);
    }
    
    // Cache all the information about the mutations that we're going to need
    configureMutationInfoBuffer(chromosome);
    
    // Keep track of the range of subpop IDs we reference, even if not represented by any haplosomes here
    maxSubpopID = 0;
    minSubpopID = SLIM_MAX_ID_VALUE;
    
    for (Subpopulation *subpop : selected_subpops)
    {
        slim_objectid_t subpop_id = subpop->subpopulation_id_;
        
        minSubpopID = std::min(minSubpopID, subpop_id);
        maxSubpopID = std::max(maxSubpopID, subpop_id);
    }
    
    // Show a progress panel if requested
    if (showProgress)
    {
        int progressSteps = (clusterOptimization == QtSLiMHaplotypeManager::ClusterOptimizeWith2opt) ? 3 : 2;
        
        progressPanel_ = new QtSLiMHaplotypeProgress(controller_->slimWindow());
        progressPanel_->runProgressWithHaplosomeCount(haplosomes.size(), progressSteps);
    }
    
    // Do the clustering analysis synchronously, updating the progress panel as we go
    finishClusteringAnalysis();
    
    // Hide the progress panel
    if (progressPanel_)
    {
        progressPanel_->hide();
        delete progressPanel_;
        progressPanel_ = nullptr;
    }
}

QtSLiMHaplotypeManager::~QtSLiMHaplotypeManager(void)
{
    if (mutationInfo)
	{
		free(mutationInfo);
		mutationInfo = nullptr;
	}
	
	if (mutationPositions)
	{
		free(mutationPositions);
		mutationPositions = nullptr;
	}
	
	if (displayList)
	{
		delete displayList;
		displayList = nullptr;
	}
}

Species *QtSLiMHaplotypeManager::focalDisplaySpecies(void)
{
    // We look up our focal species object by name every time, since keeping a pointer to it would be unsafe
    // Before initialize() is done species have not been created, so we return nullptr in that case
    if (controller_ && controller_->community() && (controller_->community()->Tick() >= 1))
		return controller_->community()->SpeciesWithName(focalSpeciesName_);
	
	return nullptr;
}

void QtSLiMHaplotypeManager::finishClusteringAnalysis(void)
{
	// Work out an approximate best sort order
	sortHaplosomes();
	
    if (valid_ && progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
        valid_ = false;
    
    if (valid_)
	{
		// Remember the subpop ID for each haplosome
		for (Haplosome *haplosome : haplosomes)
			haplosomeSubpopIDs.emplace_back(haplosome->individual_->subpopulation_->subpopulation_id_);
		
		// Build our plotting data vectors.  Because we are a snapshot, we can't rely on our controller's data
		// at all after this method returns; we have to remember everything we need to create our display list.
		configureDisplayBuffers();
	}
	
	// Now we are done with the haplosomes vector; clear it
	haplosomes.clear();
	haplosomes.resize(0);
}

void QtSLiMHaplotypeManager::configureMutationInfoBuffer(Chromosome *chromosome)
{
    Species *graphSpecies = focalDisplaySpecies();
    
    if (!graphSpecies)
        return;
    
    Population &population = graphSpecies->population_;
	double scalingFactor = 0.8; // used to be controller->selectionColorScale;
    int registry_size;
    const MutationIndex *registry = population.MutationRegistry(&registry_size);
	const MutationIndex *reg_end_ptr = registry + registry_size;
	MutationIndex biggest_index = 0;
	
	// First, find the biggest index presently in use; that's how many entries we need
    // BCH 12/25/2024: With multiple chromosomes, this is rather wasteful; I think this class
    // could be redesigned to capture just the subset of mutations that are live for a given
    // chromosome, essentially re-indexing the mutations, but it's not clear this matters
    // to performance; we just waste a bit of memory here, but it's not a big deal.
	for (const MutationIndex *reg_ptr = registry; reg_ptr != reg_end_ptr; ++reg_ptr)
	{
		MutationIndex mut_index = *reg_ptr;
		
		if (mut_index > biggest_index)
			biggest_index = mut_index;
	}
	
	// Allocate our mutationInfo buffer with entries for every MutationIndex in use
	mutationIndexCount = static_cast<size_t>(biggest_index + 1);
	mutationInfo = static_cast<HaploMutation *>(malloc(sizeof(HaploMutation) * mutationIndexCount));
	mutationPositions = static_cast<slim_position_t *>(malloc(sizeof(slim_position_t) * mutationIndexCount));
	
	// Copy the information we need on each mutation in use
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	for (const MutationIndex *reg_ptr = registry; reg_ptr != reg_end_ptr; ++reg_ptr)
	{
		MutationIndex mut_index = *reg_ptr;
		const Mutation *mut = mut_block_ptr + mut_index;
		slim_position_t mut_position = mut->position_;
		const MutationType *mut_type = mut->mutation_type_ptr_;
		HaploMutation *haplo_mut = mutationInfo + mut_index;
		
		haplo_mut->position_ = mut_position;
		*(mutationPositions + mut_index) = mut_position;
		
		if (!mut_type->color_.empty())
		{
			haplo_mut->red_ = mut_type->color_red_;
			haplo_mut->green_ = mut_type->color_green_;
			haplo_mut->blue_ = mut_type->color_blue_;
		}
		else
		{
			RGBForSelectionCoeff(static_cast<double>(mut->selection_coeff_), &haplo_mut->red_, &haplo_mut->green_, &haplo_mut->blue_, scalingFactor);
		}
		
		haplo_mut->neutral_ = (mut->selection_coeff_ == 0.0f);
		
		haplo_mut->display_ = mut_type->mutation_type_displayed_;
	}
	
	// Remember the chromosome length
	mutationLastPosition = chromosome->last_position_;
}

void QtSLiMHaplotypeManager::sortHaplosomes(void)
{
    size_t haplosome_count = haplosomes.size();
	
	if (haplosome_count == 0)
		return;
	
	std::vector<Haplosome *> original_haplosomes = haplosomes;	// copy the vector because we will need to reorder it below
	std::vector<int> final_path;
	
	// first get our distance matrix; these are inter-city distances
	int64_t *distances;
	
	if (displayingMuttypeSubset)
	{
		if (usingSubrange)
			distances = buildDistanceArrayForSubrangeAndSubtypes();
		else
			distances = buildDistanceArrayForSubtypes();
	}
	else
	{
		if (usingSubrange)
			distances = buildDistanceArrayForSubrange();
		else
			distances = buildDistanceArray();
	}
	
    if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
		goto cancelledExit;
	
	switch (clusterMethod)
	{
		case ClusterNearestNeighbor:
			nearestNeighborSolve(distances, haplosome_count, final_path);
			break;
			
		case ClusterGreedy:
			greedySolve(distances, haplosome_count, final_path);
			break;
	}
	
    if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
		goto cancelledExit;
	
	checkPath(final_path, haplosome_count);
	
    if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
		goto cancelledExit;
	
	if (clusterOptimization != ClusterNoOptimization)
	{
		switch (clusterOptimization)
		{
			case ClusterNoOptimization:
				break;
				
			case ClusterOptimizeWith2opt:
				do2optOptimizationOfSolution(final_path, distances, haplosome_count);
				break;
		}
		
        if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
            goto cancelledExit;
		
		checkPath(final_path, haplosome_count);
	}
	
    if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
		goto cancelledExit;
	
	// reorder the haplosomes vector according to the path we found
	for (size_t haplosome_index = 0; haplosome_index < haplosome_count; ++haplosome_index)
		haplosomes[haplosome_index] = original_haplosomes[static_cast<size_t>(final_path[haplosome_index])];
	
cancelledExit:
	free(distances);
}

void QtSLiMHaplotypeManager::configureDisplayBuffers(void)
{
    size_t haplosome_count = haplosomes.size();
	
	// Allocate our display list and size it so it has one std::vector<MutationIndex> per haplosome
	displayList = new std::vector<std::vector<MutationIndex>>;
	
	displayList->resize(haplosome_count);
	
	// Then save off the information for each haplosome into the display list
	for (size_t haplosome_index = 0; haplosome_index < haplosome_count; ++haplosome_index)
	{
		Haplosome &haplosome = *haplosomes[haplosome_index];
		std::vector<MutationIndex> &haplosome_display = (*displayList)[haplosome_index];
		
		if (!usingSubrange)
		{
			// Size our display list to fit the number of mutations in the haplosome
			size_t mut_count = static_cast<size_t>(haplosome.mutation_count());
			
			haplosome_display.reserve(mut_count);
			
			// Loop through mutations to get the mutation indices
			int mutrun_count = haplosome.mutrun_count_;
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				const MutationRun *mutrun = haplosome.mutruns_[run_index];
				const MutationIndex *mut_start_ptr = mutrun->begin_pointer_const();
				const MutationIndex *mut_end_ptr = mutrun->end_pointer_const();
				
				if (displayingMuttypeSubset)
				{
					// displaying a subset of mutation types, need to check
					for (const MutationIndex *mut_ptr = mut_start_ptr; mut_ptr < mut_end_ptr; ++mut_ptr)
					{
						MutationIndex mut_index = *mut_ptr;
						
						if ((mutationInfo + mut_index)->display_)
							haplosome_display.emplace_back(*mut_ptr);
					}
				}
				else
				{
					// displaying all mutation types, no need to check
					for (const MutationIndex *mut_ptr = mut_start_ptr; mut_ptr < mut_end_ptr; ++mut_ptr)
						haplosome_display.emplace_back(*mut_ptr);
				}
			}
		}
		else
		{
			// We are using a subrange, so we need to check the position of each mutation before adding it
			int mutrun_count = haplosome.mutrun_count_;
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				const MutationRun *mutrun = haplosome.mutruns_[run_index];
				const MutationIndex *mut_start_ptr = mutrun->begin_pointer_const();
				const MutationIndex *mut_end_ptr = mutrun->end_pointer_const();
				
				if (displayingMuttypeSubset)
				{
					// displaying a subset of mutation types, need to check
					for (const MutationIndex *mut_ptr = mut_start_ptr; mut_ptr < mut_end_ptr; ++mut_ptr)
					{
						MutationIndex mut_index = *mut_ptr;
						slim_position_t mut_position = *(mutationPositions + mut_index);
						
						if ((mut_position >= subrangeFirstBase) && (mut_position <= subrangeLastBase))
							if ((mutationInfo + mut_index)->display_)
								haplosome_display.emplace_back(mut_index);
					}
				}
				else
				{
					// displaying all mutation types, no need to check
					for (const MutationIndex *mut_ptr = mut_start_ptr; mut_ptr < mut_end_ptr; ++mut_ptr)
					{
						MutationIndex mut_index = *mut_ptr;
						slim_position_t mut_position = *(mutationPositions + mut_index);
						
						if ((mut_position >= subrangeFirstBase) && (mut_position <= subrangeLastBase))
							haplosome_display.emplace_back(mut_index);
					}
				}
			}
		}
	}
}

void QtSLiMHaplotypeManager::tallyBincounts(int64_t *bincounts, std::vector<MutationIndex> &haplosomeList)
{
    EIDOS_BZERO(bincounts, 1024 * sizeof(int64_t));
	
	for (MutationIndex mut_index : haplosomeList)
		bincounts[mutationInfo[mut_index].position_ % 1024]++;
}

int64_t QtSLiMHaplotypeManager::distanceForBincounts(int64_t *bincounts1, int64_t *bincounts2)
{
    int64_t distance = 0;
	
	for (int i = 0; i < 1024; ++i)
		distance += abs(bincounts1[i] - bincounts2[i]);
	
	return distance;
}

#ifndef SLIM_NO_OPENGL
void QtSLiMHaplotypeManager::glDrawHaplotypes(QRect interior, bool displayBW, bool showSubpopStrips, bool eraseBackground)
{
    // Erase the background to either black or white, depending on displayBW
	if (eraseBackground)
	{
		if (displayBW)
			glColor3f(1.0f, 1.0f, 1.0f);
		else
			glColor3f(0.0f, 0.0f, 0.0f);
		glRecti(interior.x(), interior.y(), interior.x() + interior.width(), interior.y() + interior.height());
	}
	
	// Draw subpopulation strips if requested
	if (showSubpopStrips)
	{
		const int stripWidth = 15;
		QRect subpopStripRect = interior;
		
        subpopStripRect.setWidth(stripWidth);
		glDrawSubpopStripsInRect(subpopStripRect);
        
        interior.adjust(stripWidth, 0, 0, 0);
	}
	
	// Draw the haplotypes in the remaining portion of the interior
	glDrawDisplayListInRect(interior, displayBW);
}
#endif

void QtSLiMHaplotypeManager::qtDrawHaplotypes(QRect interior, bool displayBW, bool showSubpopStrips, bool eraseBackground, QPainter &painter)
{
    // Erase the background to either black or white, depending on displayBW
    if (eraseBackground)
    {
        painter.fillRect(interior, displayBW ? Qt::white : Qt::black);
    }
    
    // Draw subpopulation strips if requested
    if (showSubpopStrips)
    {
        const int stripWidth = 15;
        QRect subpopStripRect = interior;
        
        subpopStripRect.setWidth(stripWidth);
        qtDrawSubpopStripsInRect(subpopStripRect, painter);
        
        interior.adjust(stripWidth, 0, 0, 0);
    }
    
    // Draw the haplotypes in the remaining portion of the interior
    qtDrawDisplayListInRect(interior, displayBW, painter);
}

// Traveling Salesman Problem code
//
// We have a set of haplosomes, each of which may be defined as being a particular distance from each other haplosome (defined here
// as the number of differences in the mutations contained).  We want to sort the haplosomes into an order that groups similar
// haplosomes together, minimizing the overall distance through "haplosome space" traveled from top to bottom of our display.  This
// is exactly the Traveling Salesman Problem, without returning to the starting "city".  This is a very intensively studied
// problem, is NP-hard, and would take an enormously long time to solve exactly for even a relatively small number of haplosomes,
// whereas we will routinely have thousands of haplosomes.  We will find an approximate solution using a fast heuristic algorithm,
// because we are not greatly concerned with the quality of the solution and we are extremely concerned with runtime.  The
// nearest-neighbor method is the fastest heuristic, and is O(N^2) in the number of cities; the Greedy algorithm is slower but
// produces significantly better results.  We can refine our initial solution using the 2-opt method.

#pragma mark Traveling salesman problem

// This allocates and builds an array of distances between haplosomes.  The returned array is owned by the caller.  This is where
// we spend the large majority of our time, at present; this algorithm is O(N^2), but has a large constant (because really also
// it depends on the length of the chromosome, the configuration of mutation runs, etc.).  This method runs prior to the actual
// Traveling Salesman Problem; here we're just figuring out the distances between our "cities".  We have four versions of this
// method, for speed; this is the base version, and separate versions are below that handle a chromosome subrange and/or a
// subset of all of the mutation types.
int64_t *QtSLiMHaplotypeManager::buildDistanceArray(void)
{
    size_t haplosome_count = haplosomes.size();
	int64_t *distances = static_cast<int64_t *>(malloc(haplosome_count * haplosome_count * sizeof(int64_t)));
	uint8_t *mutation_seen = static_cast<uint8_t *>(calloc(mutationIndexCount, sizeof(uint8_t)));
	uint8_t seen_marker = 1;
	
	for (size_t i = 0; i < haplosome_count; ++i)
	{
		Haplosome *haplosome1 = haplosomes[i];
		int64_t *distance_column = distances + i;
		int64_t *distance_row = distances + i * haplosome_count;
		int mutrun_count = haplosome1->mutrun_count_;
		const MutationRun **haplosome1_mutruns = haplosome1->mutruns_;
		
		distance_row[i] = 0;
		
		for (size_t j = i + 1; j < haplosome_count; ++j)
		{
			Haplosome *haplosome2 = haplosomes[j];
			const MutationRun **haplosome2_mutruns = haplosome2->mutruns_;
			int64_t distance = 0;
			
			for (int mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				const MutationRun *haplosome1_mutrun = haplosome1_mutruns[mutrun_index];
				const MutationRun *haplosome2_mutrun = haplosome2_mutruns[mutrun_index];
				int haplosome1_mutcount = haplosome1_mutrun->size();
				int haplosome2_mutcount = haplosome2_mutrun->size();
				
				if (haplosome1_mutrun == haplosome2_mutrun)
					;										// identical runs have no differences
				else if (haplosome1_mutcount == 0)
					distance += haplosome2_mutcount;
				else if (haplosome2_mutcount == 0)
					distance += haplosome1_mutcount;
				else
				{
					// We use a radix strategy to count the number of mismatches; assume up front that all mutations are mismatched,
					// and then subtract two for each mutation that turns out to be shared, using a uint8_t buffer to track usage.
					distance += haplosome1_mutcount + haplosome2_mutcount;
					
					const MutationIndex *mutrun1_end = haplosome1_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun1_ptr = haplosome1_mutrun->begin_pointer_const(); mutrun1_ptr != mutrun1_end; ++mutrun1_ptr)
						mutation_seen[*mutrun1_ptr] = seen_marker;
					
					const MutationIndex *mutrun2_end = haplosome2_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun2_ptr = haplosome2_mutrun->begin_pointer_const(); mutrun2_ptr != mutrun2_end; ++mutrun2_ptr)
						if (mutation_seen[*mutrun2_ptr] == seen_marker)
							distance -= 2;
					
					// To avoid having to clear the usage buffer every time, we play an additional trick: we use an incrementing
					// marker value to indicate usage, and clear the buffer only when it reaches 255.  Makes about a 10% difference!
					seen_marker++;
					
					if (seen_marker == 0)
					{
						EIDOS_BZERO(mutation_seen, mutationIndexCount);
						
						seen_marker = 1;
					}
				}
			}
			
			// set the distance at both mirrored locations in the distance buffer
			*(distance_column + j * haplosome_count) = distance;
			*(distance_row + j) = distance;
		}
		
        if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
            break;
		
        if (progressPanel_)
            progressPanel_->setHaplotypeProgress(i + 1, 0);
	}
	
	free(mutation_seen);
	
	return distances;
}

// This does the same thing as buildDistanceArrayForHaplosomes:, but uses the chosen subrange of each haplosome
int64_t *QtSLiMHaplotypeManager::buildDistanceArrayForSubrange(void)
{
    slim_position_t firstBase = subrangeFirstBase, lastBase = subrangeLastBase;
	
	size_t haplosome_count = haplosomes.size();
	int64_t *distances = static_cast<int64_t *>(malloc(haplosome_count * haplosome_count * sizeof(int64_t)));
	uint8_t *mutation_seen = static_cast<uint8_t *>(calloc(mutationIndexCount, sizeof(uint8_t)));
	uint8_t seen_marker = 1;
	
	for (size_t i = 0; i < haplosome_count; ++i)
	{
		Haplosome *haplosome1 = haplosomes[i];
		int64_t *distance_column = distances + i;
		int64_t *distance_row = distances + i * haplosome_count;
		slim_position_t mutrun_length = haplosome1->mutrun_length_;
		int mutrun_count = haplosome1->mutrun_count_;
		const MutationRun **haplosome1_mutruns = haplosome1->mutruns_;
		
		distance_row[i] = 0;
		
		for (size_t j = i + 1; j < haplosome_count; ++j)
		{
			Haplosome *haplosome2 = haplosomes[j];
			const MutationRun **haplosome2_mutruns = haplosome2->mutruns_;
			int64_t distance = 0;
			
			for (int mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				// Skip mutation runs outside of the subrange we're focused on
				if ((mutrun_length * mutrun_index > lastBase) || (mutrun_length * mutrun_index + mutrun_length - 1 < firstBase))
					continue;
				
				// OK, this mutrun intersects with our chosen subrange; proceed
				const MutationRun *haplosome1_mutrun = haplosome1_mutruns[mutrun_index];
				const MutationRun *haplosome2_mutrun = haplosome2_mutruns[mutrun_index];
				
				if (haplosome1_mutrun == haplosome2_mutrun)
					;										// identical runs have no differences
				else
				{
					// We use a radix strategy to count the number of mismatches.  Note this is done a bit differently than in
					// buildDistanceArrayForHaplosomes:; here we do not add the total and then subtract matches.
					const MutationIndex *mutrun1_end = haplosome1_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun1_ptr = haplosome1_mutrun->begin_pointer_const(); mutrun1_ptr != mutrun1_end; ++mutrun1_ptr)
					{
						MutationIndex mut1_index = *mutrun1_ptr;
						slim_position_t mut1_position = mutationPositions[mut1_index];
						
						if ((mut1_position >= firstBase) && (mut1_position <= lastBase))
						{
							mutation_seen[mut1_index] = seen_marker;
							distance++;		// assume unmatched
						}
					}
					
					const MutationIndex *mutrun2_end = haplosome2_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun2_ptr = haplosome2_mutrun->begin_pointer_const(); mutrun2_ptr != mutrun2_end; ++mutrun2_ptr)
					{
						MutationIndex mut2_index = *mutrun2_ptr;
						slim_position_t mut2_position = mutationPositions[mut2_index];
						
						if ((mut2_position >= firstBase) && (mut2_position <= lastBase))
						{
							if (mutation_seen[mut2_index] == seen_marker)
								distance -= 1;	// matched, so decrement to compensate for the assumption of non-match above
							else
								distance++;		// not matched, so increment
						}
					}
					
					// To avoid having to clear the usage buffer every time, we play an additional trick: we use an incrementing
					// marker value to indicate usage, and clear the buffer only when it reaches 255.  Makes about a 10% difference!
					seen_marker++;
					
					if (seen_marker == 0)
					{
						EIDOS_BZERO(mutation_seen, mutationIndexCount);
						
						seen_marker = 1;
					}
				}
			}
			
			// set the distance at both mirrored locations in the distance buffer
			*(distance_column + j * haplosome_count) = distance;
			*(distance_row + j) = distance;
		}
		
        if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
            break;
		
        if (progressPanel_)
            progressPanel_->setHaplotypeProgress(i + 1, 0);
	}
	
	free(mutation_seen);
	
	return distances;
}

// This does the same thing as buildDistanceArrayForHaplosomes:, but uses only mutations of a mutation type that is chosen for display
int64_t *QtSLiMHaplotypeManager::buildDistanceArrayForSubtypes(void)
{
    size_t haplosome_count = haplosomes.size();
	int64_t *distances = static_cast<int64_t *>(malloc(haplosome_count * haplosome_count * sizeof(int64_t)));
	uint8_t *mutation_seen = static_cast<uint8_t *>(calloc(mutationIndexCount, sizeof(uint8_t)));
	uint8_t seen_marker = 1;
	
	for (size_t i = 0; i < haplosome_count; ++i)
	{
		Haplosome *haplosome1 = haplosomes[i];
		int64_t *distance_column = distances + i;
		int64_t *distance_row = distances + i * haplosome_count;
		int mutrun_count = haplosome1->mutrun_count_;
		const MutationRun **haplosome1_mutruns = haplosome1->mutruns_;
		
		distance_row[i] = 0;
		
		for (size_t j = i + 1; j < haplosome_count; ++j)
		{
			Haplosome *haplosome2 = haplosomes[j];
			const MutationRun **haplosome2_mutruns = haplosome2->mutruns_;
			int64_t distance = 0;
			
			for (int mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				const MutationRun *haplosome1_mutrun = haplosome1_mutruns[mutrun_index];
				const MutationRun *haplosome2_mutrun = haplosome2_mutruns[mutrun_index];
				
				if (haplosome1_mutrun == haplosome2_mutrun)
					;										// identical runs have no differences
				else
				{
					// We use a radix strategy to count the number of mismatches.  Note this is done a bit differently than in
					// buildDistanceArrayForHaplosomes:; here we do not add the total and then subtract matches.
					const MutationIndex *mutrun1_end = haplosome1_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun1_ptr = haplosome1_mutrun->begin_pointer_const(); mutrun1_ptr != mutrun1_end; ++mutrun1_ptr)
					{
						MutationIndex mut1_index = *mutrun1_ptr;
						
						if (mutationInfo[mut1_index].display_)
						{
							mutation_seen[mut1_index] = seen_marker;
							distance++;		// assume unmatched
						}
					}
					
					const MutationIndex *mutrun2_end = haplosome2_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun2_ptr = haplosome2_mutrun->begin_pointer_const(); mutrun2_ptr != mutrun2_end; ++mutrun2_ptr)
					{
						MutationIndex mut2_index = *mutrun2_ptr;
						
						if (mutationInfo[mut2_index].display_)
						{
							if (mutation_seen[mut2_index] == seen_marker)
								distance -= 1;	// matched, so decrement to compensate for the assumption of non-match above
							else
								distance++;		// not matched, so increment
						}
					}
					
					// To avoid having to clear the usage buffer every time, we play an additional trick: we use an incrementing
					// marker value to indicate usage, and clear the buffer only when it reaches 255.  Makes about a 10% difference!
					seen_marker++;
					
					if (seen_marker == 0)
					{
						EIDOS_BZERO(mutation_seen, mutationIndexCount);
						
						seen_marker = 1;
					}
				}
			}
			
			// set the distance at both mirrored locations in the distance buffer
			*(distance_column + j * haplosome_count) = distance;
			*(distance_row + j) = distance;
		}
		
        if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
            break;
		
        if (progressPanel_)
            progressPanel_->setHaplotypeProgress(i + 1, 0);
	}
	
	free(mutation_seen);
	
	return distances;
}

// This does the same thing as buildDistanceArrayForHaplosomes:, but uses the chosen subrange of each haplosome, and only mutations of mutation types being displayed
int64_t *QtSLiMHaplotypeManager::buildDistanceArrayForSubrangeAndSubtypes(void)
{
    slim_position_t firstBase = subrangeFirstBase, lastBase = subrangeLastBase;
	
	size_t haplosome_count = haplosomes.size();
	int64_t *distances = static_cast<int64_t *>(malloc(haplosome_count * haplosome_count * sizeof(int64_t)));
	uint8_t *mutation_seen = static_cast<uint8_t *>(calloc(mutationIndexCount, sizeof(uint8_t)));
	uint8_t seen_marker = 1;
	
	for (size_t i = 0; i < haplosome_count; ++i)
	{
		Haplosome *haplosome1 = haplosomes[i];
		int64_t *distance_column = distances + i;
		int64_t *distance_row = distances + i * haplosome_count;
		slim_position_t mutrun_length = haplosome1->mutrun_length_;
		int mutrun_count = haplosome1->mutrun_count_;
		const MutationRun **haplosome1_mutruns = haplosome1->mutruns_;
		
		distance_row[i] = 0;
		
		for (size_t j = i + 1; j < haplosome_count; ++j)
		{
			Haplosome *haplosome2 = haplosomes[j];
			const MutationRun **haplosome2_mutruns = haplosome2->mutruns_;
			int64_t distance = 0;
			
			for (int mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				// Skip mutation runs outside of the subrange we're focused on
				if ((mutrun_length * mutrun_index > lastBase) || (mutrun_length * mutrun_index + mutrun_length - 1 < firstBase))
					continue;
				
				// OK, this mutrun intersects with our chosen subrange; proceed
				const MutationRun *haplosome1_mutrun = haplosome1_mutruns[mutrun_index];
				const MutationRun *haplosome2_mutrun = haplosome2_mutruns[mutrun_index];
				
				if (haplosome1_mutrun == haplosome2_mutrun)
					;										// identical runs have no differences
				else
				{
					// We use a radix strategy to count the number of mismatches.  Note this is done a bit differently than in
					// buildDistanceArrayForHaplosomes:; here we do not add the total and then subtract matches.
					const MutationIndex *mutrun1_end = haplosome1_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun1_ptr = haplosome1_mutrun->begin_pointer_const(); mutrun1_ptr != mutrun1_end; ++mutrun1_ptr)
					{
						MutationIndex mut1_index = *mutrun1_ptr;
						slim_position_t mut1_position = mutationPositions[mut1_index];
						
						if ((mut1_position >= firstBase) && (mut1_position <= lastBase))
						{
							if (mutationInfo[mut1_index].display_)
							{
								mutation_seen[mut1_index] = seen_marker;
								distance++;		// assume unmatched
							}
						}
					}
					
					const MutationIndex *mutrun2_end = haplosome2_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun2_ptr = haplosome2_mutrun->begin_pointer_const(); mutrun2_ptr != mutrun2_end; ++mutrun2_ptr)
					{
						MutationIndex mut2_index = *mutrun2_ptr;
						slim_position_t mut2_position = mutationPositions[mut2_index];
						
						if ((mut2_position >= firstBase) && (mut2_position <= lastBase))
						{
							if (mutationInfo[mut2_index].display_)
							{
								if (mutation_seen[mut2_index] == seen_marker)
									distance -= 1;	// matched, so decrement to compensate for the assumption of non-match above
								else
									distance++;		// not matched, so increment
							}
						}
					}
					
					// To avoid having to clear the usage buffer every time, we play an additional trick: we use an incrementing
					// marker value to indicate usage, and clear the buffer only when it reaches 255.  Makes about a 10% difference!
					seen_marker++;
					
					if (seen_marker == 0)
					{
						EIDOS_BZERO(mutation_seen, mutationIndexCount);
						
						seen_marker = 1;
					}
				}
			}
			
			// set the distance at both mirrored locations in the distance buffer
			*(distance_column + j * haplosome_count) = distance;
			*(distance_row + j) = distance;
		}
		
        if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
            break;
		
        if (progressPanel_)
            progressPanel_->setHaplotypeProgress(i + 1, 0);
	}
	
	free(mutation_seen);
	
	return distances;
}

// Since we want to solve the Traveling Salesman Problem without returning to the original city, the choice of the initial city
// may be quite important to the solution we get.  It seems reasonable to start at the city that is the most isolated, i.e. has
// the largest distance from itself to any other city.  By starting with this city, we avoid having to have two edges connecting
// to it, both of which would be relatively long.  However, this is just a guess, and might be modified by refinement later.
int QtSLiMHaplotypeManager::indexOfMostIsolatedHaplosomeWithDistances(int64_t *distances, size_t haplosome_count)
{
	int64_t greatest_isolation = -1;
	int greatest_isolation_index = -1;
	
	for (size_t i = 0; i < haplosome_count; ++i)
	{
		int64_t isolation = INT64_MAX;
		int64_t *row_ptr = distances + i * haplosome_count;
		
		for (size_t j = 0; j < haplosome_count; ++j)
		{
			int64_t distance = row_ptr[j];
			
			// distances of 0 don't count for isolation estimation; we really want the most isolated identical cluster of haplosomes
			// this also serves to take care of the j == i case for us without special-casing, which is nice...
			if (distance == 0)
				continue;
			
			if (distance < isolation)
				isolation = distance;
		}
		
		if (isolation > greatest_isolation)
		{
			greatest_isolation = isolation;
			greatest_isolation_index = static_cast<int>(i);
		}
	}
	
	return greatest_isolation_index;
}

// The nearest-neighbor method provides an initial solution for the Traveling Salesman Problem by beginning with a chosen city
// (see indexOfMostIsolatedHaplosomeWithDistances:size: above) and adding successive cities according to which is closest to the
// city we have reached thus far.  This is quite simple to implement, and runs in O(N^2) time.  However, the greedy algorithm
// below runs only a little more slowly, and produces significantly better results, so unless speed is essential it is better.
void QtSLiMHaplotypeManager::nearestNeighborSolve(int64_t *distances, size_t haplosome_count, std::vector<int> &solution)
{
    size_t haplosomes_left = haplosome_count;
	
	solution.reserve(haplosome_count);
	
	// we have to make a copy of the distances matrix, as we modify it internally
	int64_t *distances_copy = static_cast<int64_t *>(malloc(haplosome_count * haplosome_count * sizeof(int64_t)));
	
	memcpy(distances_copy, distances, haplosome_count * haplosome_count * sizeof(int64_t));
	
	// find the haplosome that is farthest from any other haplosome; this will be our starting point, for now
	int last_path_index = indexOfMostIsolatedHaplosomeWithDistances(distances_copy, haplosome_count);
	
	do
	{
		// add the chosen haplosome to our path
		solution.emplace_back(last_path_index);
		
        if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
            break;
		
        if (progressPanel_)
            progressPanel_->setHaplotypeProgress(haplosome_count - haplosomes_left + 1, 1);
		
		// if we just added the last haplosome, we're done
		if (--haplosomes_left == 0)
			break;
		
		// otherwise, mark the chosen haplosome as unavailable by setting distances to it to INT64_MAX
		int64_t *column_ptr = distances_copy + last_path_index;
		
		for (size_t i = 0; i < haplosome_count; ++i)
		{
			*column_ptr = INT64_MAX;
			column_ptr += haplosome_count;
		}
		
		// now we need to find the next city, which will be the nearest neighbor of the last city
		int64_t *row_ptr = distances_copy + last_path_index * static_cast<int>(haplosome_count);
		int64_t nearest_neighbor_distance = INT64_MAX;
		int nearest_neighbor_index = -1;
		
		for (size_t i = 0; i < haplosome_count; ++i)
		{
			int64_t distance = row_ptr[i];
			
			if (distance < nearest_neighbor_distance)
			{
				nearest_neighbor_distance = distance;
				nearest_neighbor_index = static_cast<int>(i);
			}
		}
		
		// found the next city; add it to the path by looping back to the top
		last_path_index = nearest_neighbor_index;
	}
	while (true);
	
	free(distances_copy);
}

// The greedy method provides an initial solution for the Traveling Salesman Problem by sorting all possible edges,
// and then iteratively adding the shortest legal edge to the path until the full path has been constructed.  This
// is a little more complex than nearest neighbor, and runs a bit more slowly, but gives a somewhat better result.
typedef struct {
	int i, k;
	int64_t d;
} greedy_edge;

static bool operator<(const greedy_edge &i, const greedy_edge &j) __attribute__((unused));
static bool operator>(const greedy_edge &i, const greedy_edge &j) __attribute__((unused));
static bool operator==(const greedy_edge &i, const greedy_edge &j) __attribute__((unused));
static bool operator!=(const greedy_edge &i, const greedy_edge &j) __attribute__((unused));

static bool operator<(const greedy_edge &i, const greedy_edge &j) { return (i.d < j.d); }
static bool operator>(const greedy_edge &i, const greedy_edge &j) { return (i.d > j.d); }
static bool operator==(const greedy_edge &i, const greedy_edge &j) { return (i.d == j.d); }
static bool operator!=(const greedy_edge &i, const greedy_edge &j) { return (i.d != j.d); }

void QtSLiMHaplotypeManager::greedySolve(int64_t *distances, size_t haplosome_count, std::vector<int> &solution)
{
    // The first thing we need to do is sort all possible edges in ascending order by length;
	// we don't need to differentiate a->b versus b->a since our distances are symmetric
	std::vector<greedy_edge> edge_buf;
	size_t edge_count = (haplosome_count * (haplosome_count - 1)) / 2;
	
	edge_buf.reserve(edge_count);	// one of the two factors is even so /2 is safe
	
	for (size_t i = 0; i < haplosome_count - 1; ++i)
	{
		for (size_t k = i + 1; k < haplosome_count; ++k)
			edge_buf.emplace_back(greedy_edge{static_cast<int>(i), static_cast<int>(k), *(distances + i + k * haplosome_count)});
	}
    
	if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
		return;
	
	if (progressPanel_)
	{
        // We have a progress panel, so we do an incremental sort
        BareBoneIIQS<greedy_edge> sorter(edge_buf.data(), edge_count);
        
        for (size_t i = 0; i < haplosome_count - 1; ++i)
        {
            for (size_t k = i + 1; k < haplosome_count; ++k)
                sorter.next();
            
            if (progressPanel_->haplotypeProgressIsCancelled())
                return;
            
            progressPanel_->setHaplotypeProgress(i, 1);
        }
	}
	else
	{
		// If we're not running with a progress panel, we have no progress indicator so we can just use std::sort()
		std::sort(edge_buf.begin(), edge_buf.end());
	}
	
	if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
		return;
	
	// Now we take take the first legal edge from the top of edge_buf and add it to our path. "Legal" means it
	// doesn't increase the degree of either participating node above 2, and doesn't create a cycle.  We check
	// the first condition by keeping a vector of the degrees of all nodes, so that's easy.  We check the second
	// condition by keeping a vector of "group" tags for each participating node; an edge that joins two nodes
	// in the same group creates a cycle and is thus illegal (maybe there's a better way to detect cycles but I
	// haven't thought of it yet :->).
	std::vector<greedy_edge> path_components;
	uint8_t *node_degrees = static_cast<uint8_t *>(calloc(sizeof(uint8_t), haplosome_count));
	int *node_groups = static_cast<int *>(calloc(sizeof(int), haplosome_count));
	int next_node_group = 1;
	
	path_components.reserve(haplosome_count);
	
	for (size_t edge_index = 0; edge_index < edge_count; ++edge_index)
	{
		greedy_edge &candidate_edge = edge_buf[edge_index];
		
		// Get the participating nodes and check that they still have a free end
		int i = candidate_edge.i;
		
		if (node_degrees[i] == 2)
			continue;
		
		int k = candidate_edge.k;
		
		if (node_degrees[k] == 2)
			continue;
		
		// Check whether they are in the same group (and not 0), in which case this edge would create a cycle
		int group_i = node_groups[i];
		int group_k = node_groups[k];
		
		if ((group_i != 0) && (group_i == group_k))
			continue;
		
		// OK, the edge is legal.  Add it to our path, and maintain the group tags
		path_components.emplace_back(candidate_edge);
		node_degrees[i]++;
		node_degrees[k]++;
		
		if ((group_i == 0) && (group_k == 0))
		{
			// making a new group
			node_groups[i] = next_node_group;
			node_groups[k] = next_node_group;
			next_node_group++;
		}
		else if (group_i == 0)
		{
			// adding node i to an existing group
			node_groups[i] = group_k;
		}
		else if (group_k == 0)
		{
			// adding node k to an existing group
			node_groups[k] = group_i;
		}
		else
		{
			// joining two groups; one gets assimilated
			// the assimilation could probably be done more efficiently but this overhead won't matter
			for (size_t node_index = 0; node_index < haplosome_count; ++node_index)
				if (node_groups[node_index] == group_k)
					node_groups[node_index] = group_i;
		}
		
		if (path_components.size() == haplosome_count - 1)		// no return edge
			break;
		
        if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
            goto cancelExit;
	}
	
	// Check our work
	{
		int degree1_count = 0, degree2_count = 0, universal_group = node_groups[0];
		
		for (size_t node_index = 0; node_index < haplosome_count; ++node_index)
		{
			if (node_degrees[node_index] == 1) ++degree1_count;
			else if (node_degrees[node_index] == 2) ++degree2_count;
			else qDebug() << "node of degree other than 1 or 2 seen (degree" << node_degrees[node_index] << ")";
			
			if (node_groups[node_index] != universal_group)
				qDebug() << "node of non-matching group seen (group" << node_groups[node_index] << ")";
		}
        
        // suppress "variable set but not used" warnings, since we may want these bookkeeping variables at some point...
        (void)degree1_count;
        (void)degree2_count;
	}
	
	if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
		goto cancelExit;
	
	// Finally, we have a jumble of edges that are in no order, and we need to make a coherent path from them.
	// We start at the first degree-1 node we find, which is one of the two ends; doesn't matter which.
	{
		size_t remaining_edge_count = haplosome_count - 1;
		size_t last_index;
		
		for (last_index = 0; last_index < haplosome_count; ++last_index)
			if (node_degrees[last_index] == 1)
				break;
		
		solution.emplace_back(static_cast<int>(last_index));
		
		do
		{
			// look for an edge involving last_index that we haven't used yet (there should be only one)
			size_t next_edge_index;
			int next_index = INT_MAX;	// get rid of the unitialized var warning, and cause a crash if we have a bug
			
			for (next_edge_index = 0; next_edge_index < remaining_edge_count; ++next_edge_index)
			{
				greedy_edge &candidate_edge = path_components[next_edge_index];
				
				if (candidate_edge.i == static_cast<int>(last_index))
				{
					next_index = candidate_edge.k;
					break;
				}
				else if (candidate_edge.k == static_cast<int>(last_index))
				{
					next_index = candidate_edge.i;
					break;
				}
			}
			
            if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
                break;
			
			// found it; assimilate it into the path and remove it from path_components
			solution.emplace_back(next_index);
			last_index = static_cast<size_t>(next_index);
			
			path_components[next_edge_index] = path_components[--remaining_edge_count];
		}
		while (remaining_edge_count > 0);
	}
	
cancelExit:
	free(node_degrees);
	free(node_groups);
}

// check that a given path visits every city exactly once
bool QtSLiMHaplotypeManager::checkPath(std::vector<int> &path, size_t haplosome_count)
{
    uint8_t *visits = static_cast<uint8_t *>(calloc(sizeof(uint8_t), haplosome_count));
	
	if (path.size() != haplosome_count)
	{
		qDebug() << "checkPath:size: path is wrong length";
		free(visits);
		return false;
	}
	
	for (size_t i = 0; i < haplosome_count; ++i)
	{
		int city_index = path[i];
		
		visits[city_index]++;
	}
	
	for (size_t i = 0; i < haplosome_count; ++i)
		if (visits[i] != 1)
		{
			qDebug() << "checkPath:size: city visited wrong count (" << visits[i] << ")";
			free(visits);
			return false;
		}
	
	free(visits);
	return true;
}

// calculate the length of a given path
int64_t QtSLiMHaplotypeManager::lengthOfPath(std::vector<int> &path, int64_t *distances, size_t haplosome_count)
{
	int64_t length = 0;
	int current_city = path[0];
	
	for (size_t city_index = 1; city_index < haplosome_count; city_index++)
	{
		int next_city = path[city_index];
		
		length += *(distances + static_cast<size_t>(current_city) * haplosome_count + next_city);
		current_city = next_city;
	}
	
	return length;
}

// Do "2-opt" optimization of a given path, which involves inverting ranges of the path that lead to a better solution.
// This is quite time-consuming and improves the result only marginally, so we do not want it to be the default, but it
// might be useful to provide as an option.  This method always takes the first optimization it sees that moves in a
// positive direction; I tried taking the best optimization available at each step, instead, and it ran half as fast
// and achieved results that were no better on average, so I didn't even keep that code.
void QtSLiMHaplotypeManager::do2optOptimizationOfSolution(std::vector<int> &path, int64_t *distances, size_t haplosome_count)
{
    // Figure out the length of the current path
	int64_t original_distance = lengthOfPath(path, distances, haplosome_count);
	int64_t best_distance = original_distance;
	
	//NSLog(@"2-opt initial length: %lld", (long long int)best_distance);
	
	// Iterate until we can find no 2-opt improvement; this algorithm courtesy of https://en.wikipedia.org/wiki/2-opt
	size_t farthest_i = 0;	// for our progress bar
	
startAgain:
	for (size_t i = 0; i < haplosome_count - 1; i++)
	{
		for (size_t k = i + 1; k < haplosome_count; ++k)
		{
			// First, try the proposed path without actually constructing it; we just need to subtract the lengths of the
			// edges being removed and add the lengths of the edges being added, rather than constructing the whole new
			// path and measuring its length.  If we have a path 1:9 and are inverting i=3 to k=5, it looks like:
			//
			//		1	2	3	4	5	6	7	8	9
			//			   (i		k)
			//
			//		1	2  (5	4	3)	6	7	8	9
			//
			// So the 2-3 edge and the 5-6 edge were subtracted, and the 2-5 edge and the 3-6 edge were added.  Note that
			// we can only get away with juggling the distances this way because our problem is symmetric; the length of
			// 3-4-5 is guaranteed the same as the length of the reversed segment 5-4-3.  If the reversed segment is at
			// one or the other end of the path, we only need to patch up one edge; we don't return to the start city.
			// Note also that i and k are not haplosome indexes; they are indexes into our current path, which provides us
			// with the relevant haplosomes indexes.
			int64_t new_distance = best_distance;
			size_t index_i = static_cast<size_t>(path[i]);
			size_t index_k = static_cast<size_t>(path[k]);
			
			if (i > 0)
			{
				size_t index_i_minus_1 = static_cast<size_t>(path[i - 1]);
				
				new_distance -= *(distances + index_i_minus_1 + index_i * haplosome_count);	// remove edge (i-1)-(i)
				new_distance += *(distances + index_i_minus_1 + index_k * haplosome_count);	// add edge (i-1)-(k)
			}
			if (k < haplosome_count - 1)
			{
				size_t index_k_plus_1 = static_cast<size_t>(path[k + 1]);
				
				new_distance -= *(distances + index_k + index_k_plus_1 * haplosome_count);		// remove edge (k)-(k+1)
				new_distance += *(distances + index_i + index_k_plus_1 * haplosome_count);		// add edge (i)-(k+1)
			}
			
			if (new_distance < best_distance)
			{
				// OK, the new path is an improvement, so let's take it.  We construct it by inverting the sequence
				// from i to k in our path vector, by swapping elements until we reach the center.
				for (size_t inversion_length = 0; ; inversion_length++)
				{
					size_t swap1 = i + inversion_length;
					size_t swap2 = k - inversion_length;
					
					if (swap1 >= swap2)
						break;
					
					std::swap(path[swap1], path[swap2]);
				}
				
				best_distance = new_distance;
				
				//NSLog(@"Improved path length: %lld (inverted from %d to %d)", (long long int)best_distance, i, k);
				//NSLog(@"   checkback: new path length is %lld", (long long int)[self lengthOfPath:path withDistances:distances size:haplosome_count]);
				goto startAgain;
			}
		}
		
		// We update our progress bar according to the furthest we have ever gotten in the outer loop; we keep having to start
		// over again, and there's no way to know how many times we're going to do that, so this seems like the best estimator.
		farthest_i = std::max(farthest_i, i + 1);
		
        if (progressPanel_)
            progressPanel_->setHaplotypeProgress(farthest_i, 2);
		
        if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
            break;
	}
	
	//NSLog(@"Distance changed from %lld to %lld (%.3f%% improvement)", (long long int)original_distance, (long long int)best_distance, ((original_distance - best_distance) / (double)original_distance) * 100.0);
}


//
// QtSLiMHaplotypeView
//
// This class is private to QtSLiMHaplotypeManager, but is declared here so MOC gets it automatically
//

QtSLiMHaplotypeView::QtSLiMHaplotypeView(QWidget *p_parent, Qt::WindowFlags f)
#ifndef SLIM_NO_OPENGL
    : QOpenGLWidget(p_parent, f)
#else
    : QWidget(p_parent, f)
#endif
{
    // We support both OpenGL and non-OpenGL display, because some platforms seem
    // to have problems with OpenGL (https://github.com/MesserLab/SLiM/issues/462)
    QtSLiMPreferencesNotifier &prefsNotifier = QtSLiMPreferencesNotifier::instance();
    
    connect(&prefsNotifier, &QtSLiMPreferencesNotifier::useOpenGLPrefChanged, this, [this]() { update(); });
}

QtSLiMHaplotypeView::~QtSLiMHaplotypeView(void)
{
    delegate_ = nullptr;
}

#ifndef SLIM_NO_OPENGL
void QtSLiMHaplotypeView::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
}

void QtSLiMHaplotypeView::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    
	// Update the projection
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
}
#endif

#ifndef SLIM_NO_OPENGL
void QtSLiMHaplotypeView::paintGL()
#else
void QtSLiMHaplotypeView::paintEvent(QPaintEvent * /* p_paint_event */)
#endif
{
    QPainter painter(this);
    
    painter.eraseRect(rect());      // erase to background color, which is not guaranteed
    
    // inset and frame with gray
    QRect interior = rect();
    
    interior.adjust(5, 5, -5, 0);   // 0 because the action button layout already has margin
    painter.fillRect(interior, Qt::gray);
    interior.adjust(1, 1, -1, -1);
    
    if (delegate_)
    {
#ifndef SLIM_NO_OPENGL
        if (QtSLiMPreferencesNotifier::instance().useOpenGLPref())
        {
            painter.beginNativePainting();
            delegate_->glDrawHaplotypes(interior, displayBlackAndWhite_, showSubpopulationStrips_, true);
            painter.endNativePainting();
        }
        else
#endif
        {
            delegate_->qtDrawHaplotypes(interior, displayBlackAndWhite_, showSubpopulationStrips_, true, painter);
        }
    }
}

void QtSLiMHaplotypeView::actionButtonRunMenu(QtSLiMPushButton *p_actionButton)
{
    contextMenuEvent(nullptr);
    
    // This is not called by Qt, for some reason (nested tracking loops?), so we call it explicitly
    p_actionButton->qtslimSetHighlight(false);
}

void QtSLiMHaplotypeView::contextMenuEvent(QContextMenuEvent *p_event)
{
    QMenu contextMenu("graph_menu", this);
    
    QAction *bwColorToggle = contextMenu.addAction(displayBlackAndWhite_ ? "Display Colors" : "Display Black && White");
    QAction *subpopStripsToggle = contextMenu.addAction(showSubpopulationStrips_ ? "Hide Subpopulation Strips" : "Show Subpopulation Strips");
    
    contextMenu.addSeparator();
    
    QAction *copyPlot = contextMenu.addAction("Copy Plot");
    QAction *exportPlot = contextMenu.addAction("Export Plot...");
    
    // Run the context menu synchronously
    QPoint menuPos = (p_event ? p_event->globalPos() : QCursor::pos());
    QAction *action = contextMenu.exec(menuPos);
    
    // Act upon the chosen action; we just do it right here instead of dealing with slots
    if (action)
    {
        if (action == bwColorToggle)
        {
            displayBlackAndWhite_ = !displayBlackAndWhite_;
            update();
        }
        if (action == subpopStripsToggle)
        {
            showSubpopulationStrips_ = !showSubpopulationStrips_;
            update();
        }
#ifndef SLIM_NO_OPENGL
        if (action == copyPlot)
        {
            QImage snap = grabFramebuffer();
            QSize snapSize = snap.size();
            QImage interior = snap.copy(5, 5, snapSize.width() - 10, snapSize.height() - 10);
            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setImage(interior);
        }
        if (action == exportPlot)
        {
            // FIXME maybe this should use QtSLiMDefaultSaveDirectory?  see QtSLiMWindow::saveAs()
            QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
            QFileInfo fileInfo(QDir(desktopPath), "haplotypes.png");
            QString path = fileInfo.absoluteFilePath();
            QString fileName = QFileDialog::getSaveFileName(this, "Export Graph", path);
            
            if (!fileName.isEmpty())
            {
                QImage snap = grabFramebuffer();
                QSize snapSize = snap.size();
                QImage interior = snap.copy(5, 5, snapSize.width() - 10, snapSize.height() - 10);
                
                interior.save(fileName, "PNG", 100);    // JPG does not come out well; colors washed out
            }
        }
#endif
    }
}







































