//
//  QtSLiMGraphView_1DPopulationSFS.cpp
//  SLiM
//
//  Created by Ben Haller on 3/27/2020.
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

#include "QtSLiMGraphView_1DPopulationSFS.h"

#include "QtSLiMWindow.h"

#include <string>


QtSLiMGraphView_1DPopulationSFS::QtSLiMGraphView_1DPopulationSFS(QWidget *p_parent, QtSLiMWindow *controller) : QtSLiMGraphView(p_parent, controller)
{
    histogramBinCount_ = 10;
    allowBinCountRescale_ = true;
    
    xAxisMajorTickInterval_ = 0.2;
    xAxisMinorTickInterval_ = 0.1;
    xAxisMajorTickModulus_ = 2;
    xAxisTickValuePrecision_ = 1;
    
    xAxisLabel_ = "Mutation frequency";
    yAxisLabel_ = "Proportion of mutations";
    
    allowXAxisUserRescale_ = false;
    allowYAxisUserRescale_ = false;
    
    showHorizontalGridLines_ = true;
}

QtSLiMGraphView_1DPopulationSFS::~QtSLiMGraphView_1DPopulationSFS()
{
}

QString QtSLiMGraphView_1DPopulationSFS::graphTitle(void)
{
    return "1D Population SFS";
}

QString QtSLiMGraphView_1DPopulationSFS::aboutString(void)
{
    return "The 1D Population SFS graph shows a Site Frequency Spectrum (SFS) for the entire population.  Since "
           "mutation occurrence counts across the whole population might be very large, the x axis here is the "
           "frequency of a given mutation, from 0.0 to 1.0, rather than an occurrence count.  The y axis is the "
           "proportion of all mutations that fall within a given binned frequency range.  The number of frequency "
           "bins can be customized from the action menu.  The 1D Sample SFS graph provides an alternative that "
           "might also be useful.";
}

double *QtSLiMGraphView_1DPopulationSFS::populationSFS(int mutationTypeCount)
{
    static uint32_t *spectrum = nullptr;			// used for tallying
	static double *doubleSpectrum = nullptr;	// not used for tallying, to avoid precision issues
	static size_t spectrumBins = 0;
	int binCount = histogramBinCount_;
	size_t usedSpectrumBins = static_cast<size_t>(binCount * mutationTypeCount);
	
	// allocate our bins
	if (!spectrum || (spectrumBins < usedSpectrumBins))
	{
		spectrumBins = usedSpectrumBins;
		spectrum = static_cast<uint32_t *>(realloc(spectrum, spectrumBins * sizeof(uint32_t)));
		doubleSpectrum = static_cast<double *>(realloc(doubleSpectrum, spectrumBins * sizeof(double)));
	}
	
	// clear our bins
	for (size_t i = 0; i < usedSpectrumBins; ++i)
		spectrum[i] = 0;
	
	// get the selected chromosome range
    Species *graphSpecies = focalDisplaySpecies();
	
	// tally into our bins
	Population &pop = graphSpecies->population_;
	
	pop.TallyMutationReferencesAcrossPopulation(/* p_clock_for_mutrun_experiments */ false);	// update tallies; usually this will just use the cache set up by Population::MaintainRegistry()
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
    int registry_size;
    const MutationIndex *registry = pop.MutationRegistry(&registry_size);
	
	for (int registry_index = 0; registry_index < registry_size; ++registry_index)
	{
		const Mutation *mutation = mut_block_ptr + registry[registry_index];
        Chromosome *mut_chromosome = graphSpecies->Chromosomes()[mutation->chromosome_index_];
        double totalHaplosomeCount = ((mut_chromosome->total_haplosome_count_ == 0) ? 1 : mut_chromosome->total_haplosome_count_);   // prevent a zero count from producing NAN frequencies below
		
		slim_refcount_t mutationRefCount = *(refcount_block_ptr + mutation->BlockIndex());
		double mutationFrequency = mutationRefCount / totalHaplosomeCount;
		int mutationBin = static_cast<int>(floor(mutationFrequency * binCount));
		int mutationTypeIndex = mutation->mutation_type_ptr_->mutation_type_index_;
		
		if (mutationBin == binCount)
			mutationBin = binCount - 1;
		
		(spectrum[mutationTypeIndex + mutationBin * mutationTypeCount])++;	// bins in sequence for each mutation type within one frequency bin, then again for the next frequency bin, etc.
	}
	
	// normalize within each mutation type
	for (int mutationTypeIndex = 0; mutationTypeIndex < mutationTypeCount; ++mutationTypeIndex)
	{
		uint32_t total = 0;
		
		for (int bin = 0; bin < binCount; ++bin)
		{
			int binIndex = mutationTypeIndex + bin * mutationTypeCount;
			
			total += spectrum[binIndex];
		}
		
        for (int bin = 0; bin < binCount; ++bin)
        {
            int binIndex = mutationTypeIndex + bin * mutationTypeCount;
            
            doubleSpectrum[binIndex] = (total > 0) ? (spectrum[binIndex] / static_cast<double>(total)) : 0.0;
        }
    }
	
	// return the final tally; note this is a pointer in to our static ivar, and must not be freed!
	return doubleSpectrum;
}

void QtSLiMGraphView_1DPopulationSFS::drawGraph(QPainter &painter, QRect interiorRect)
{
	int binCount = histogramBinCount_;
    Species *graphSpecies = focalDisplaySpecies();
	int mutationTypeCount = static_cast<int>(graphSpecies->mutation_types_.size());
	double *spectrum = populationSFS(mutationTypeCount);
	
	// plot our histogram bars
	drawGroupedBarplot(painter, interiorRect, spectrum, mutationTypeCount, binCount, 0.0, (1.0 / binCount));
}

QtSLiMLegendSpec QtSLiMGraphView_1DPopulationSFS::legendKey(void)
{
	return mutationTypeLegendKey();     // we use the prefab mutation type legend
}

bool QtSLiMGraphView_1DPopulationSFS::providesStringForData(void)
{
    return true;
}

void QtSLiMGraphView_1DPopulationSFS::appendStringForData(QString &string)
{
    // get the selected chromosome range
    Species *graphSpecies = focalDisplaySpecies();
	int binCount = histogramBinCount_;
	int mutationTypeCount = static_cast<int>(graphSpecies->mutation_types_.size());
	double *plotData = populationSFS(mutationTypeCount);
	
	for (auto mutationTypeIter : graphSpecies->mutation_types_)
	{
		MutationType *mutationType = mutationTypeIter.second;
		int mutationTypeIndex = mutationType->mutation_type_index_;		// look up the index used for this mutation type in the history info; not necessarily sequential!
		
        string.append(QString("\"m%1\", ").arg(mutationType->mutation_type_id_));
		
		for (int i = 0; i < binCount; ++i)
		{
			int histIndex = mutationTypeIndex + i * mutationTypeCount;
			
            string.append(QString("%1, ").arg(plotData[histIndex], 0, 'f', 4));
		}
		
        string.append("\n");
	}
}





























