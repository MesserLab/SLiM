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

#include "QtSLiMHaplotypeManager.h"
#include "QtSLiMWindow.h"
#include "QtSLiMHaplotypeOptions.h"
#include "QtSLiMHaplotypeProgress.h"
#include "QtSLiMExtras.h"
#include "QtSLiMAppDelegate.h"

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
#include <QDebug>

#include <vector>
#include <algorithm>

#include "eidos_globals.h"
#include "subpopulation.h"




// This class method runs a plot options dialog, and then produces a haplotype plot with a progress panel as it is being constructed
void QtSLiMHaplotypeManager::CreateHaplotypePlot(QtSLiMWindow *controller)
{
    QtSLiMHaplotypeOptions optionsPanel(controller);
    
    int result = optionsPanel.exec();
    
    if (result == QDialog::Accepted)
    {
        size_t genomeSampleSize = optionsPanel.genomeSampleSize();
        QtSLiMHaplotypeManager::ClusteringMethod clusteringMethod = optionsPanel.clusteringMethod();
        QtSLiMHaplotypeManager::ClusteringOptimization clusteringOptimization = optionsPanel.clusteringOptimization();
        
        // First generate the haplotype plot data, with a progress panel
        QtSLiMHaplotypeManager *haplotypeManager = new QtSLiMHaplotypeManager(nullptr, clusteringMethod, clusteringOptimization,
                                                                              controller, genomeSampleSize, true);
        
        if (haplotypeManager->valid_)
        {
            // Make a new window to show the graph
            QWidget *window = new QWidget(controller, Qt::Window | Qt::Tool);    // the graph window has us as a parent, but is still a standalone window
            
            window->setWindowTitle(QString("Haplotype snapshot (%1)").arg(haplotypeManager->titleString));
            window->setMinimumSize(400, 200);
            window->resize(500, 400);
#ifdef __APPLE__
            // set the window icon only on macOS; on Linux it changes the app icon as a side effect
            window->setWindowIcon(QIcon());
#endif
            
            // Make the window layout
            QVBoxLayout *topLayout = new QVBoxLayout;
            
            window->setLayout(topLayout);
            topLayout->setMargin(0);
            topLayout->setSpacing(0);
            
            // Make a new haplotype view and add it to the layout
            QtSLiMHaplotypeView *haplotypeView = new QtSLiMHaplotypeView(nullptr);
            
            topLayout->addWidget(haplotypeView);
            
            // The haplotype manager is owned by the graph view, as a delegate object
            haplotypeView->setDelegate(haplotypeManager);
            
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

QtSLiMHaplotypeManager::QtSLiMHaplotypeManager(QObject *parent, ClusteringMethod clusteringMethod, ClusteringOptimization optimizationMethod,
                                               QtSLiMWindow *controller, size_t sampleSize, bool showProgress) :
    QObject(parent)
{
    controller_ = controller;
    
    SLiMSim *sim = controller_->sim;
    Population &population = sim->population_;
    
    clusterMethod = clusteringMethod;
    clusterOptimization = optimizationMethod;
    
    // Figure out which subpops are selected (or if none are, consider all to be); we will display only the selected subpops
    std::vector<Subpopulation *> selected_subpops;
    
    for (auto subpop_pair : population.subpops_)
        if (subpop_pair.second->gui_selected_)
            selected_subpops.push_back(subpop_pair.second);
    
    if (selected_subpops.size() == 0)
        for (auto subpop_pair : population.subpops_)
            selected_subpops.push_back(subpop_pair.second);
    
    // Figure out whether we're analyzing / displaying a subrange; gross that we go right into the ChromosomeView, I know...
    
    controller_->chromosomeSelection(&usingSubrange, &subrangeFirstBase, &subrangeLastBase);
    
    // Also dig to find out whether we're displaying all mutation types or just a subset; if a subset, each MutationType has a display flag
    displayingMuttypeSubset = (controller_->chromosomeDisplayMuttypes().size() != 0);
    
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
    
    title.append(QString(", generation %1").arg(sim->generation_));
    
    titleString = title;
    subpopCount = static_cast<int>(selected_subpops.size());
    
    // Fetch genomes and figure out what we're going to plot; note that we plot only non-null genomes
    for (Subpopulation *subpop : selected_subpops)
        for (Genome *genome : subpop->parent_genomes_)
            if (!genome->IsNull())
                genomes.push_back(genome);
    
    // If a sample is requested, select that now; sampleSize <= 0 means no sampling
    if ((sampleSize > 0) && (genomes.size() > sampleSize))
    {
        Eidos_random_unique(genomes.begin(), genomes.end(), sampleSize);
        genomes.resize(sampleSize);
    }
    
    // Cache all the information about the mutations that we're going to need
    configureMutationInfoBuffer();
    
    // Keep track of the range of subpop IDs we reference, even if not represented by any genomes here
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
        
        progressPanel_ = new QtSLiMHaplotypeProgress(controller_);
        progressPanel_->runProgressWithGenomeCount(genomes.size(), progressSteps);
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

void QtSLiMHaplotypeManager::finishClusteringAnalysis(void)
{
	// Work out an approximate best sort order
	sortGenomes();
	
    if (valid_ && progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
        valid_ = false;
    
    if (valid_)
	{
		// Remember the subpop ID for each genome
		for (Genome *genome : genomes)
			genomeSubpopIDs.push_back(genome->subpop_->subpopulation_id_);
		
		// Build our plotting data vectors.  Because we are a snapshot, we can't rely on our controller's data
		// at all after this method returns; we have to remember everything we need to create our display list.
		configureDisplayBuffers();
	}
	
	// Now we are done with the genomes vector; clear it
	genomes.clear();
	genomes.resize(0);
}

void QtSLiMHaplotypeManager::configureMutationInfoBuffer()
{
    SLiMSim *sim = controller_->sim;
	Population &population = sim->population_;
	double scalingFactor = 0.8; //controller_->selectionColorScale;
    int registry_size;
    const MutationIndex *registry = population.MutationRegistry(&registry_size);
	const MutationIndex *reg_end_ptr = registry + registry_size;
	MutationIndex biggest_index = 0;
	
	// First, find the biggest index presently in use; that's how many entries we need
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
	mutationLastPosition = sim->chromosome_.last_position_;
}

void QtSLiMHaplotypeManager::sortGenomes(void)
{
    size_t genome_count = genomes.size();
	
	if (genome_count == 0)
		return;
	
	std::vector<Genome *> original_genomes = genomes;	// copy the vector because we will need to reorder it below
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
			nearestNeighborSolve(distances, genome_count, final_path);
			break;
			
		case ClusterGreedy:
			greedySolve(distances, genome_count, final_path);
			break;
	}
	
    if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
		goto cancelledExit;
	
	checkPath(final_path, genome_count);
	
    if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
		goto cancelledExit;
	
	if (clusterOptimization != ClusterNoOptimization)
	{
		switch (clusterOptimization)
		{
			case ClusterNoOptimization:
				break;
				
			case ClusterOptimizeWith2opt:
				do2optOptimizationOfSolution(final_path, distances, genome_count);
				break;
		}
		
        if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
            goto cancelledExit;
		
		checkPath(final_path, genome_count);
	}
	
    if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
		goto cancelledExit;
	
	// reorder the genomes vector according to the path we found
	for (size_t genome_index = 0; genome_index < genome_count; ++genome_index)
		genomes[genome_index] = original_genomes[static_cast<size_t>(final_path[genome_index])];
	
cancelledExit:
	free(distances);
}

void QtSLiMHaplotypeManager::configureDisplayBuffers(void)
{
    size_t genome_count = genomes.size();
	
	// Allocate our display list and size it so it has one std::vector<MutationIndex> per genome
	displayList = new std::vector<std::vector<MutationIndex>>;
	
	displayList->resize(genome_count);
	
	// Then save off the information for each genome into the display list
	for (size_t genome_index = 0; genome_index < genome_count; ++genome_index)
	{
		Genome &genome = *genomes[genome_index];
		std::vector<MutationIndex> &genome_display = (*displayList)[genome_index];
		
		if (!usingSubrange)
		{
			// Size our display list to fit the number of mutations in the genome
			size_t mut_count = static_cast<size_t>(genome.mutation_count());
			
			genome_display.reserve(mut_count);
			
			// Loop through mutations to get the mutation indices
			int mutrun_count = genome.mutrun_count_;
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				MutationRun *mutrun = genome.mutruns_[run_index].get();
				const MutationIndex *mut_start_ptr = mutrun->begin_pointer_const();
				const MutationIndex *mut_end_ptr = mutrun->end_pointer_const();
				
				if (displayingMuttypeSubset)
				{
					// displaying a subset of mutation types, need to check
					for (const MutationIndex *mut_ptr = mut_start_ptr; mut_ptr < mut_end_ptr; ++mut_ptr)
					{
						MutationIndex mut_index = *mut_ptr;
						
						if ((mutationInfo + mut_index)->display_)
							genome_display.push_back(*mut_ptr);
					}
				}
				else
				{
					// displaying all mutation types, no need to check
					for (const MutationIndex *mut_ptr = mut_start_ptr; mut_ptr < mut_end_ptr; ++mut_ptr)
						genome_display.push_back(*mut_ptr);
				}
			}
		}
		else
		{
			// We are using a subrange, so we need to check the position of each mutation before adding it
			int mutrun_count = genome.mutrun_count_;
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				MutationRun *mutrun = genome.mutruns_[run_index].get();
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
								genome_display.push_back(mut_index);
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
							genome_display.push_back(mut_index);
					}
				}
			}
		}
	}
}

static const int kMaxGLRects = 2000;				// 2000 rects
static const int kMaxVertices = kMaxGLRects * 4;	// 4 vertices each

static float *glArrayVertices = nullptr;
static float *glArrayColors = nullptr;

void QtSLiMHaplotypeManager::allocateGLBuffers(void)
{
	// Set up the vertex and color arrays
	if (!glArrayVertices)
		glArrayVertices = static_cast<float *>(malloc(kMaxVertices * 2 * sizeof(float)));		// 2 floats per vertex, kMaxVertices vertices
	
	if (!glArrayColors)
		glArrayColors = static_cast<float *>(malloc(kMaxVertices * 4 * sizeof(float)));		// 4 floats per color, kMaxVertices colors
}

void QtSLiMHaplotypeManager::drawSubpopStripsInRect(QRect interior)
{
    int displayListIndex;
	float *vertices = nullptr, *colors = nullptr;
	
	// Set up to draw rects
	displayListIndex = 0;
	
	vertices = glArrayVertices;
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, glArrayVertices);
	
	colors = glArrayColors;
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_FLOAT, 0, glArrayColors);
	
	// Loop through the genomes and draw them; we do this in two passes, neutral mutations underneath selected mutations
	size_t genome_index = 0, genome_count = genomeSubpopIDs.size();
	float height_divisor = genome_count;
	float left = static_cast<float>(interior.x());
	float right = static_cast<float>(interior.x() + interior.width());
	
	for (slim_objectid_t genome_subpop_id : genomeSubpopIDs)
	{
		float top = interior.y() + (genome_index / height_divisor) * interior.height();
		float bottom = interior.y() + ((genome_index + 1) / height_divisor) * interior.height();
		
		if (bottom - top > 1.0f)
		{
			// If the range spans a width of more than one pixel, then use the maximal pixel range
			top = floorf(top);
			bottom = ceilf(bottom);
		}
		else
		{
			// If the range spans a pixel or less, make sure that we end up with a range that is one pixel wide, even if the positions span a pixel boundary
			top = floorf(top);
			bottom = top + 1;
		}
		
		*(vertices++) = left;		*(vertices++) = top;
		*(vertices++) = left;		*(vertices++) = bottom;
		*(vertices++) = right;		*(vertices++) = bottom;
		*(vertices++) = right;		*(vertices++) = top;
		
		float colorRed, colorGreen, colorBlue;
		double hue = (genome_subpop_id - minSubpopID) / static_cast<double>(maxSubpopID - minSubpopID + 1);
        QColor hsbColor = QtSLiMColorWithHSV(hue, 1.0, 1.0, 1.0);
		QColor rgbColor = hsbColor.toRgb();
        
		colorRed = static_cast<float>(rgbColor.redF());
		colorGreen = static_cast<float>(rgbColor.greenF());
		colorBlue = static_cast<float>(rgbColor.blueF());
		
		for (int j = 0; j < 4; ++j) {
			*(colors++) = colorRed;		*(colors++) = colorGreen;		*(colors++) = colorBlue;	*(colors++) = 1.0;
		}
		
		displayListIndex++;
		
		// If we've filled our buffers, get ready to draw more
		if (displayListIndex == kMaxGLRects)
		{
			// Draw our arrays
			glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
			
			// And get ready to draw more
			vertices = glArrayVertices;
			colors = glArrayColors;
			displayListIndex = 0;
		}
		
		genome_index++;
	}
	
	// Draw any leftovers
	if (displayListIndex)
		glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

void QtSLiMHaplotypeManager::tallyBincounts(int64_t *bincounts, std::vector<MutationIndex> &genomeList)
{
    EIDOS_BZERO(bincounts, 1024 * sizeof(int64_t));
	
	for (MutationIndex mut_index : genomeList)
		bincounts[mutationInfo[mut_index].position_ % 1024]++;
}

int64_t QtSLiMHaplotypeManager::distanceForBincounts(int64_t *bincounts1, int64_t *bincounts2)
{
    int64_t distance = 0;
	
	for (int i = 0; i < 1024; ++i)
		distance += abs(bincounts1[i] - bincounts2[i]);
	
	return distance;
}

void QtSLiMHaplotypeManager::drawDisplayListInRect(QRect interior, bool displayBW, int64_t **previousFirstBincounts)
{
    int displayListIndex;
	float *vertices = nullptr, *colors = nullptr;
	
	// Set up to draw rects
	displayListIndex = 0;
	
	vertices = glArrayVertices;
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, glArrayVertices);
	
	colors = glArrayColors;
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_FLOAT, 0, glArrayColors);
	
	// decide whether to plot in ascending order or descending order; we do this based on a calculated
	// similarity to the previously displayed first genome, so that we maximize visual continuity
	size_t genome_count = displayList->size();
	bool ascending = true;
	
	if (previousFirstBincounts && (genome_count > 1))
	{
		std::vector<MutationIndex> &first_genome_list = (*displayList)[0];
		std::vector<MutationIndex> &last_genome_list = (*displayList)[genome_count - 1];
		static int64_t *first_genome_bincounts = nullptr;
		static int64_t *last_genome_bincounts = nullptr;
		
		if (!first_genome_bincounts)	first_genome_bincounts = static_cast<int64_t *>(malloc(1024 * sizeof(int64_t)));
		if (!last_genome_bincounts)		last_genome_bincounts = static_cast<int64_t *>(malloc(1024 * sizeof(int64_t)));
		
		tallyBincounts(first_genome_bincounts, first_genome_list);
		tallyBincounts(last_genome_bincounts, last_genome_list);
		
		if (*previousFirstBincounts)
		{
			int64_t first_genome_distance = distanceForBincounts(first_genome_bincounts, *previousFirstBincounts);
			int64_t last_genome_distance = distanceForBincounts(last_genome_bincounts, *previousFirstBincounts);
			
			if (first_genome_distance > last_genome_distance)
				ascending = false;
			
			free(*previousFirstBincounts);
		}
		
		// take over one of our buffers, to avoid having to copy values
		if (ascending) {
			*previousFirstBincounts = first_genome_bincounts;
			first_genome_bincounts = nullptr;
		} else {
			*previousFirstBincounts = last_genome_bincounts;
			last_genome_bincounts = nullptr;
		}
	}
	
	// Loop through the genomes and draw them; we do this in two passes, neutral mutations underneath selected mutations
	for (int pass_count = 0; pass_count <= 1; ++pass_count)
	{
		bool plotting_neutral = (pass_count == 0);
		float height_divisor = genome_count;
		float width_subtractor = (usingSubrange ? subrangeFirstBase : 0);
		float width_divisor = (usingSubrange ? (subrangeLastBase - subrangeFirstBase + 1) : (mutationLastPosition + 1));
		
		for (size_t genome_index = 0; genome_index < genome_count; ++genome_index)
		{
			std::vector<MutationIndex> &genome_list = (ascending ? (*displayList)[genome_index] : (*displayList)[(genome_count - 1) - genome_index]);
			float top = interior.y() + (genome_index / height_divisor) * interior.height();
			float bottom = interior.y() + ((genome_index + 1) / height_divisor) * interior.height();
			
			if (bottom - top > 1.0f)
			{
				// If the range spans a width of more than one pixel, then use the maximal pixel range
				top = floorf(top);
				bottom = ceilf(bottom);
			}
			else
			{
				// If the range spans a pixel or less, make sure that we end up with a range that is one pixel wide, even if the positions span a pixel boundary
				top = floorf(top);
				bottom = top + 1;
			}
			
			for (MutationIndex mut_index : genome_list)
			{
				HaploMutation &mut_info = mutationInfo[mut_index];
				
				if (mut_info.neutral_ == plotting_neutral)
				{
					slim_position_t mut_position = mut_info.position_;
					float left = interior.x() + ((mut_position - width_subtractor) / width_divisor) * interior.width();
					float right = interior.x() + ((mut_position - width_subtractor + 1) / width_divisor) * interior.width();
					
					if (right - left > 1.0f)
					{
						// If the range spans a width of more than one pixel, then use the maximal pixel range
						left = floorf(left);
						right = ceilf(right);
					}
					else
					{
						// If the range spans a pixel or less, make sure that we end up with a range that is one pixel wide, even if the positions span a pixel boundary
						left = floorf(left);
						right = left + 1;
					}
					
					*(vertices++) = left;		*(vertices++) = top;
					*(vertices++) = left;		*(vertices++) = bottom;
					*(vertices++) = right;		*(vertices++) = bottom;
					*(vertices++) = right;		*(vertices++) = top;
					
					float colorRed, colorGreen, colorBlue;
					
					if (displayBW) {
						colorRed = 0;					colorGreen = 0;						colorBlue = 0;
					} else {
						colorRed = mut_info.red_;		colorGreen = mut_info.green_;		colorBlue = mut_info.blue_;
					}
					
					for (int j = 0; j < 4; ++j) {
						*(colors++) = colorRed;		*(colors++) = colorGreen;		*(colors++) = colorBlue;	*(colors++) = 1.0;
					}
					
					displayListIndex++;
					
					// If we've filled our buffers, get ready to draw more
					if (displayListIndex == kMaxGLRects)
					{
						// Draw our arrays
						glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
						
						// And get ready to draw more
						vertices = glArrayVertices;
						colors = glArrayColors;
						displayListIndex = 0;
					}
				}
			}
		}
	}
	
	// Draw any leftovers
	if (displayListIndex)
		glDrawArrays(GL_QUADS, 0, 4 * displayListIndex);
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

void QtSLiMHaplotypeManager::glDrawHaplotypes(QRect interior, bool displayBW, bool showSubpopStrips, bool eraseBackground, int64_t **previousFirstBincounts)
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
	
	// Make sure our GL data buffers are allocated; these are shared among all instances and drawing routines
	allocateGLBuffers();
	
	// Draw subpopulation strips if requested
	if (showSubpopStrips)
	{
		const int stripWidth = 15;
		QRect subpopStripRect = interior;
		
        subpopStripRect.setWidth(stripWidth);
		drawSubpopStripsInRect(subpopStripRect);
        
        interior.adjust(stripWidth, 0, 0, 0);
	}
	
	// Draw the haplotypes in the remaining portion of the interior
	drawDisplayListInRect(interior, displayBW, previousFirstBincounts);
}

// Traveling Salesman Problem code
//
// We have a set of genomes, each of which may be defined as being a particular distance from each other genome (defined here
// as the number of differences in the mutations contained).  We want to sort the genomes into an order that groups similar
// genomes together, minimizing the overall distance through "genome space" traveled from top to bottom of our display.  This
// is exactly the Traveling Salesman Problem, without returning to the starting "city".  This is a very intensively studied
// problem, is NP-hard, and would take an enormously long time to solve exactly for even a relatively small number of genomes,
// whereas we will routinely have thousands of genomes.  We will find an approximate solution using a fast heuristic algorithm,
// because we are not greatly concerned with the quality of the solution and we are extremely concerned with runtime.  The
// nearest-neighbor method is the fastest heuristic, and is O(N^2) in the number of cities; the Greedy algorithm is slower but
// produces significantly better results.  We can refine our initial solution using the 2-opt method.

#pragma mark Traveling salesman problem

// This allocates and builds an array of distances between genomes.  The returned array is owned by the caller.  This is where
// we spend the large majority of our time, at present; this algorithm is O(N^2), but has a large constant (because really also
// it depends on the length of the chromosome, the configuration of mutation runs, etc.).  This method runs prior to the actual
// Traveling Salesman Problem; here we're just figuring out the distances between our "cities".  We have four versions of this
// method, for speed; this is the base version, and separate versions are below that handle a chromosome subrange and/or a
// subset of all of the mutation types.
int64_t *QtSLiMHaplotypeManager::buildDistanceArray(void)
{
    size_t genome_count = genomes.size();
	int64_t *distances = static_cast<int64_t *>(malloc(genome_count * genome_count * sizeof(int64_t)));
	uint8_t *mutation_seen = static_cast<uint8_t *>(calloc(mutationIndexCount, sizeof(uint8_t)));
	uint8_t seen_marker = 1;
	
	for (size_t i = 0; i < genome_count; ++i)
	{
		Genome *genome1 = genomes[i];
		int64_t *distance_column = distances + i;
		int64_t *distance_row = distances + i * genome_count;
		int mutrun_count = genome1->mutrun_count_;
		MutationRun_SP *genome1_mutruns = genome1->mutruns_;
		
		distance_row[i] = 0;
		
		for (size_t j = i + 1; j < genome_count; ++j)
		{
			Genome *genome2 = genomes[j];
			MutationRun_SP *genome2_mutruns = genome2->mutruns_;
			int64_t distance = 0;
			
			for (int mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				MutationRun *genome1_mutrun = genome1_mutruns[mutrun_index].get();
				MutationRun *genome2_mutrun = genome2_mutruns[mutrun_index].get();
				int genome1_mutcount = genome1_mutrun->size();
				int genome2_mutcount = genome2_mutrun->size();
				
				if (genome1_mutrun == genome2_mutrun)
					;										// identical runs have no differences
				else if (genome1_mutcount == 0)
					distance += genome2_mutcount;
				else if (genome2_mutcount == 0)
					distance += genome1_mutcount;
				else
				{
					// We use a radix strategy to count the number of mismatches; assume up front that all mutations are mismatched,
					// and then subtract two for each mutation that turns out to be shared, using a uint8_t buffer to track usage.
					distance += genome1_mutcount + genome2_mutcount;
					
					const MutationIndex *mutrun1_end = genome1_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun1_ptr = genome1_mutrun->begin_pointer_const(); mutrun1_ptr != mutrun1_end; ++mutrun1_ptr)
						mutation_seen[*mutrun1_ptr] = seen_marker;
					
					const MutationIndex *mutrun2_end = genome2_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun2_ptr = genome2_mutrun->begin_pointer_const(); mutrun2_ptr != mutrun2_end; ++mutrun2_ptr)
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
			*(distance_column + j * genome_count) = distance;
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

// This does the same thing as buildDistanceArrayForGenomes:, but uses the chosen subrange of each genome
int64_t *QtSLiMHaplotypeManager::buildDistanceArrayForSubrange(void)
{
    slim_position_t firstBase = subrangeFirstBase, lastBase = subrangeLastBase;
	
	size_t genome_count = genomes.size();
	int64_t *distances = static_cast<int64_t *>(malloc(genome_count * genome_count * sizeof(int64_t)));
	uint8_t *mutation_seen = static_cast<uint8_t *>(calloc(mutationIndexCount, sizeof(uint8_t)));
	uint8_t seen_marker = 1;
	
	for (size_t i = 0; i < genome_count; ++i)
	{
		Genome *genome1 = genomes[i];
		int64_t *distance_column = distances + i;
		int64_t *distance_row = distances + i * genome_count;
		slim_position_t mutrun_length = genome1->mutrun_length_;
		int mutrun_count = genome1->mutrun_count_;
		MutationRun_SP *genome1_mutruns = genome1->mutruns_;
		
		distance_row[i] = 0;
		
		for (size_t j = i + 1; j < genome_count; ++j)
		{
			Genome *genome2 = genomes[j];
			MutationRun_SP *genome2_mutruns = genome2->mutruns_;
			int64_t distance = 0;
			
			for (int mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				// Skip mutation runs outside of the subrange we're focused on
				if ((mutrun_length * mutrun_index > lastBase) || (mutrun_length * mutrun_index + mutrun_length - 1 < firstBase))
					continue;
				
				// OK, this mutrun intersects with our chosen subrange; proceed
				MutationRun *genome1_mutrun = genome1_mutruns[mutrun_index].get();
				MutationRun *genome2_mutrun = genome2_mutruns[mutrun_index].get();
				
				if (genome1_mutrun == genome2_mutrun)
					;										// identical runs have no differences
				else
				{
					// We use a radix strategy to count the number of mismatches.  Note this is done a bit differently than in
					// buildDistanceArrayForGenomes:; here we do not add the total and then subtract matches.
					const MutationIndex *mutrun1_end = genome1_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun1_ptr = genome1_mutrun->begin_pointer_const(); mutrun1_ptr != mutrun1_end; ++mutrun1_ptr)
					{
						MutationIndex mut1_index = *mutrun1_ptr;
						slim_position_t mut1_position = mutationPositions[mut1_index];
						
						if ((mut1_position >= firstBase) && (mut1_position <= lastBase))
						{
							mutation_seen[mut1_index] = seen_marker;
							distance++;		// assume unmatched
						}
					}
					
					const MutationIndex *mutrun2_end = genome2_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun2_ptr = genome2_mutrun->begin_pointer_const(); mutrun2_ptr != mutrun2_end; ++mutrun2_ptr)
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
			*(distance_column + j * genome_count) = distance;
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

// This does the same thing as buildDistanceArrayForGenomes:, but uses only mutations of a mutation type that is chosen for display
int64_t *QtSLiMHaplotypeManager::buildDistanceArrayForSubtypes(void)
{
    size_t genome_count = genomes.size();
	int64_t *distances = static_cast<int64_t *>(malloc(genome_count * genome_count * sizeof(int64_t)));
	uint8_t *mutation_seen = static_cast<uint8_t *>(calloc(mutationIndexCount, sizeof(uint8_t)));
	uint8_t seen_marker = 1;
	
	for (size_t i = 0; i < genome_count; ++i)
	{
		Genome *genome1 = genomes[i];
		int64_t *distance_column = distances + i;
		int64_t *distance_row = distances + i * genome_count;
		int mutrun_count = genome1->mutrun_count_;
		MutationRun_SP *genome1_mutruns = genome1->mutruns_;
		
		distance_row[i] = 0;
		
		for (size_t j = i + 1; j < genome_count; ++j)
		{
			Genome *genome2 = genomes[j];
			MutationRun_SP *genome2_mutruns = genome2->mutruns_;
			int64_t distance = 0;
			
			for (int mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				MutationRun *genome1_mutrun = genome1_mutruns[mutrun_index].get();
				MutationRun *genome2_mutrun = genome2_mutruns[mutrun_index].get();
				
				if (genome1_mutrun == genome2_mutrun)
					;										// identical runs have no differences
				else
				{
					// We use a radix strategy to count the number of mismatches.  Note this is done a bit differently than in
					// buildDistanceArrayForGenomes:; here we do not add the total and then subtract matches.
					const MutationIndex *mutrun1_end = genome1_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun1_ptr = genome1_mutrun->begin_pointer_const(); mutrun1_ptr != mutrun1_end; ++mutrun1_ptr)
					{
						MutationIndex mut1_index = *mutrun1_ptr;
						
						if (mutationInfo[mut1_index].display_)
						{
							mutation_seen[mut1_index] = seen_marker;
							distance++;		// assume unmatched
						}
					}
					
					const MutationIndex *mutrun2_end = genome2_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun2_ptr = genome2_mutrun->begin_pointer_const(); mutrun2_ptr != mutrun2_end; ++mutrun2_ptr)
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
			*(distance_column + j * genome_count) = distance;
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

// This does the same thing as buildDistanceArrayForGenomes:, but uses the chosen subrange of each genome, and only mutations of mutation types being displayed
int64_t *QtSLiMHaplotypeManager::buildDistanceArrayForSubrangeAndSubtypes(void)
{
    slim_position_t firstBase = subrangeFirstBase, lastBase = subrangeLastBase;
	
	size_t genome_count = genomes.size();
	int64_t *distances = static_cast<int64_t *>(malloc(genome_count * genome_count * sizeof(int64_t)));
	uint8_t *mutation_seen = static_cast<uint8_t *>(calloc(mutationIndexCount, sizeof(uint8_t)));
	uint8_t seen_marker = 1;
	
	for (size_t i = 0; i < genome_count; ++i)
	{
		Genome *genome1 = genomes[i];
		int64_t *distance_column = distances + i;
		int64_t *distance_row = distances + i * genome_count;
		slim_position_t mutrun_length = genome1->mutrun_length_;
		int mutrun_count = genome1->mutrun_count_;
		MutationRun_SP *genome1_mutruns = genome1->mutruns_;
		
		distance_row[i] = 0;
		
		for (size_t j = i + 1; j < genome_count; ++j)
		{
			Genome *genome2 = genomes[j];
			MutationRun_SP *genome2_mutruns = genome2->mutruns_;
			int64_t distance = 0;
			
			for (int mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
			{
				// Skip mutation runs outside of the subrange we're focused on
				if ((mutrun_length * mutrun_index > lastBase) || (mutrun_length * mutrun_index + mutrun_length - 1 < firstBase))
					continue;
				
				// OK, this mutrun intersects with our chosen subrange; proceed
				MutationRun *genome1_mutrun = genome1_mutruns[mutrun_index].get();
				MutationRun *genome2_mutrun = genome2_mutruns[mutrun_index].get();
				
				if (genome1_mutrun == genome2_mutrun)
					;										// identical runs have no differences
				else
				{
					// We use a radix strategy to count the number of mismatches.  Note this is done a bit differently than in
					// buildDistanceArrayForGenomes:; here we do not add the total and then subtract matches.
					const MutationIndex *mutrun1_end = genome1_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun1_ptr = genome1_mutrun->begin_pointer_const(); mutrun1_ptr != mutrun1_end; ++mutrun1_ptr)
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
					
					const MutationIndex *mutrun2_end = genome2_mutrun->end_pointer_const();
					
					for (const MutationIndex *mutrun2_ptr = genome2_mutrun->begin_pointer_const(); mutrun2_ptr != mutrun2_end; ++mutrun2_ptr)
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
			*(distance_column + j * genome_count) = distance;
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
int QtSLiMHaplotypeManager::indexOfMostIsolatedGenomeWithDistances(int64_t *distances, size_t genome_count)
{
	int64_t greatest_isolation = -1;
	int greatest_isolation_index = -1;
	
	for (size_t i = 0; i < genome_count; ++i)
	{
		int64_t isolation = INT64_MAX;
		int64_t *row_ptr = distances + i * genome_count;
		
		for (size_t j = 0; j < genome_count; ++j)
		{
			int64_t distance = row_ptr[j];
			
			// distances of 0 don't count for isolation estimation; we really want the most isolated identical cluster of genomes
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
// (see indexOfMostIsolatedGenomeWithDistances:size: above) and adding successive cities according to which is closest to the
// city we have reached thus far.  This is quite simple to implement, and runs in O(N^2) time.  However, the greedy algorithm
// below runs only a little more slowly, and produces significantly better results, so unless speed is essential it is better.
void QtSLiMHaplotypeManager::nearestNeighborSolve(int64_t *distances, size_t genome_count, std::vector<int> &solution)
{
    size_t genomes_left = genome_count;
	
	solution.reserve(genome_count);
	
	// we have to make a copy of the distances matrix, as we modify it internally
	int64_t *distances_copy = static_cast<int64_t *>(malloc(genome_count * genome_count * sizeof(int64_t)));
	
	memcpy(distances_copy, distances, genome_count * genome_count * sizeof(int64_t));
	
	// find the genome that is farthest from any other genome; this will be our starting point, for now
	int last_path_index = indexOfMostIsolatedGenomeWithDistances(distances_copy, genome_count);
	
	do
	{
		// add the chosen genome to our path
		solution.push_back(last_path_index);
		
        if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
            break;
		
        if (progressPanel_)
            progressPanel_->setHaplotypeProgress(genome_count - genomes_left + 1, 1);
		
		// if we just added the last genome, we're done
		if (--genomes_left == 0)
			break;
		
		// otherwise, mark the chosen genome as unavailable by setting distances to it to INT64_MAX
		int64_t *column_ptr = distances_copy + last_path_index;
		
		for (size_t i = 0; i < genome_count; ++i)
		{
			*column_ptr = INT64_MAX;
			column_ptr += genome_count;
		}
		
		// now we need to find the next city, which will be the nearest neighbor of the last city
		int64_t *row_ptr = distances_copy + last_path_index * static_cast<int>(genome_count);
		int64_t nearest_neighbor_distance = INT64_MAX;
		int nearest_neighbor_index = -1;
		
		for (size_t i = 0; i < genome_count; ++i)
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

void QtSLiMHaplotypeManager::greedySolve(int64_t *distances, size_t genome_count, std::vector<int> &solution)
{
    // The first thing we need to do is sort all possible edges in ascending order by length;
	// we don't need to differentiate a->b versus b->a since our distances are symmetric
	std::vector<greedy_edge> edge_buf;
	size_t edge_count = (genome_count * (genome_count - 1)) / 2;
	
	edge_buf.reserve(edge_count);	// one of the two factors is even so /2 is safe
	
	for (size_t i = 0; i < genome_count - 1; ++i)
	{
		for (size_t k = i + 1; k < genome_count; ++k)
			edge_buf.emplace_back(greedy_edge{static_cast<int>(i), static_cast<int>(k), *(distances + i + k * genome_count)});
	}
    
	if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
		return;
	
	if (progressPanel_)
	{
        // We have a progress panel, so we do an incremental sort
        BareBoneIIQS<greedy_edge> sorter(edge_buf.data(), edge_count);
        
        for (size_t i = 0; i < genome_count - 1; ++i)
        {
            for (size_t k = i + 1; k < genome_count; ++k)
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
	uint8_t *node_degrees = static_cast<uint8_t *>(calloc(sizeof(uint8_t), genome_count));
	int *node_groups = static_cast<int *>(calloc(sizeof(int), genome_count));
	int next_node_group = 1;
	
	path_components.reserve(genome_count);
	
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
		path_components.push_back(candidate_edge);
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
			for (size_t node_index = 0; node_index < genome_count; ++node_index)
				if (node_groups[node_index] == group_k)
					node_groups[node_index] = group_i;
		}
		
		if (path_components.size() == genome_count - 1)		// no return edge
			break;
		
        if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
            goto cancelExit;
	}
	
	// Check our work
	{
		int degree1_count = 0, degree2_count = 0, universal_group = node_groups[0];
		
		for (size_t node_index = 0; node_index < genome_count; ++node_index)
		{
			if (node_degrees[node_index] == 1) ++degree1_count;
			else if (node_degrees[node_index] == 2) ++degree2_count;
			else qDebug() << "node of degree other than 1 or 2 seen (degree" << node_degrees[node_index] << ")";
			
			if (node_groups[node_index] != universal_group)
				qDebug() << "node of non-matching group seen (group" << node_groups[node_index] << ")";
		}
	}
	
	if (progressPanel_ && progressPanel_->haplotypeProgressIsCancelled())
		goto cancelExit;
	
	// Finally, we have a jumble of edges that are in no order, and we need to make a coherent path from them.
	// We start at the first degree-1 node we find, which is one of the two ends; doesn't matter which.
	{
		size_t remaining_edge_count = genome_count - 1;
		size_t last_index;
		
		for (last_index = 0; last_index < genome_count; ++last_index)
			if (node_degrees[last_index] == 1)
				break;
		
		solution.push_back(static_cast<int>(last_index));
		
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
			solution.push_back(next_index);
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
bool QtSLiMHaplotypeManager::checkPath(std::vector<int> &path, size_t genome_count)
{
    uint8_t *visits = static_cast<uint8_t *>(calloc(sizeof(uint8_t), genome_count));
	
	if (path.size() != genome_count)
	{
		qDebug() << "checkPath:size: path is wrong length";
		free(visits);
		return false;
	}
	
	for (size_t i = 0; i < genome_count; ++i)
	{
		int city_index = path[i];
		
		visits[city_index]++;
	}
	
	for (size_t i = 0; i < genome_count; ++i)
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
int64_t QtSLiMHaplotypeManager::lengthOfPath(std::vector<int> &path, int64_t *distances, size_t genome_count)
{
	int64_t length = 0;
	int current_city = path[0];
	
	for (size_t city_index = 1; city_index < genome_count; city_index++)
	{
		int next_city = path[city_index];
		
		length += *(distances + static_cast<size_t>(current_city) * genome_count + next_city);
		current_city = next_city;
	}
	
	return length;
}

// Do "2-opt" optimization of a given path, which involves inverting ranges of the path that lead to a better solution.
// This is quite time-consuming and improves the result only marginally, so we do not want it to be the default, but it
// might be useful to provide as an option.  This method always takes the first optimization it sees that moves in a
// positive direction; I tried taking the best optimization available at each step, instead, and it ran half as fast
// and achieved results that were no better on average, so I didn't even keep that code.
void QtSLiMHaplotypeManager::do2optOptimizationOfSolution(std::vector<int> &path, int64_t *distances, size_t genome_count)
{
    // Figure out the length of the current path
	int64_t original_distance = lengthOfPath(path, distances, genome_count);
	int64_t best_distance = original_distance;
	
	//NSLog(@"2-opt initial length: %lld", best_distance);
	
	// Iterate until we can find no 2-opt improvement; this algorithm courtesy of https://en.wikipedia.org/wiki/2-opt
	size_t farthest_i = 0;	// for our progress bar
	
startAgain:
	for (size_t i = 0; i < genome_count - 1; i++)
	{
		for (size_t k = i + 1; k < genome_count; ++k)
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
			// Note also that i and k are not genome indexes; they are indexes into our current path, which provides us
			// with the relevant genomes indexes.
			int64_t new_distance = best_distance;
			size_t index_i = static_cast<size_t>(path[i]);
			size_t index_k = static_cast<size_t>(path[k]);
			
			if (i > 0)
			{
				size_t index_i_minus_1 = static_cast<size_t>(path[i - 1]);
				
				new_distance -= *(distances + index_i_minus_1 + index_i * genome_count);	// remove edge (i-1)-(i)
				new_distance += *(distances + index_i_minus_1 + index_k * genome_count);	// add edge (i-1)-(k)
			}
			if (k < genome_count - 1)
			{
				size_t index_k_plus_1 = static_cast<size_t>(path[k + 1]);
				
				new_distance -= *(distances + index_k + index_k_plus_1 * genome_count);		// remove edge (k)-(k+1)
				new_distance += *(distances + index_i + index_k_plus_1 * genome_count);		// add edge (i)-(k+1)
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
				
				//NSLog(@"Improved path length: %lld (inverted from %d to %d)", best_distance, i, k);
				//NSLog(@"   checkback: new path length is %lld", [self lengthOfPath:path withDistances:distances size:genome_count]);
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
	
	//NSLog(@"Distance changed from %lld to %lld (%.3f%% improvement)", original_distance, best_distance, ((original_distance - best_distance) / (double)original_distance) * 100.0);
}


//
// QtSLiMHaplotypeView
//
// This class is private to QtSLiMHaplotypeManager, but is declared here so MOC gets it automatically
//

QtSLiMHaplotypeView::QtSLiMHaplotypeView(QWidget *parent, Qt::WindowFlags f) :
    QOpenGLWidget(parent, f)
{
}

QtSLiMHaplotypeView::~QtSLiMHaplotypeView(void)
{
    delegate_ = nullptr;
}

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

void QtSLiMHaplotypeView::paintGL()
{
    QPainter painter(this);
    
    painter.eraseRect(rect());      // erase to background color, which is not guaranteed
    
    // inset and frame with gray
    QRect interior = rect();
    
    interior.adjust(5, 5, -5, -5);
    painter.fillRect(interior, Qt::gray);
    interior.adjust(1, 1, -1, -1);
    
    if (delegate_)
    {
        painter.beginNativePainting();
        delegate_->glDrawHaplotypes(interior, displayBlackAndWhite_, showSubpopulationStrips_, true, nullptr);
        painter.endNativePainting();
    }
}

void QtSLiMHaplotypeView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu contextMenu("graph_menu", this);
    
    QAction *bwColorToggle = contextMenu.addAction(displayBlackAndWhite_ ? "Display Colors" : "Display Black && White");
    QAction *subpopStripsToggle = contextMenu.addAction(showSubpopulationStrips_ ? "Hide Subpopulation Strips" : "Show Subpopulation Strips");
    
    contextMenu.addSeparator();
    
    QAction *copyPlot = contextMenu.addAction("Copy Plot");
    QAction *exportPlot = contextMenu.addAction("Export Plot...");
    
    // Run the context menu synchronously
    QAction *action = contextMenu.exec(event->globalPos());
    
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
    }
}







































