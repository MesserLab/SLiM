//
//  QtSLiMGraphView_FrequencySpectra.cpp
//  SLiM
//
//  Created by Ben Haller on 3/27/2020.
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

#include "QtSLiMGraphView_FrequencySpectra.h"

#include "QtSLiMWindow.h"


QtSLiMGraphView_FrequencySpectra::QtSLiMGraphView_FrequencySpectra(QWidget *parent, QtSLiMWindow *controller) : QtSLiMGraphView(parent, controller)
{
    histogramBinCount_ = 10;
    allowXAxisBinRescale_ = true;
    
    xAxisMajorTickInterval_ = 0.2;
    xAxisMinorTickInterval_ = 0.1;
    xAxisMajorTickModulus_ = 2;
    xAxisTickValuePrecision_ = 1;
    
    xAxisLabel_ = "Mutation frequency";
    yAxisLabel_ = "Proportion of mutations";
    
    allowXAxisUserRescale_ = false;
    allowYAxisUserRescale_ = true;
    
    showHorizontalGridLines_ = true;
}

QtSLiMGraphView_FrequencySpectra::~QtSLiMGraphView_FrequencySpectra()
{
}

QString QtSLiMGraphView_FrequencySpectra::graphTitle(void)
{
    return "Mutation Frequency Spectrum";
}

double *QtSLiMGraphView_FrequencySpectra::mutationFrequencySpectrum(int mutationTypeCount)
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
	bool hasSelection;
	slim_position_t selectionFirstBase;
	slim_position_t selectionLastBase;
    controller_->chromosomeSelection(&hasSelection, &selectionFirstBase, &selectionLastBase);
	
	// tally into our bins
	SLiMSim *sim = controller_->sim;
	Population &pop = sim->population_;
	
	pop.TallyMutationReferences(nullptr, false);	// update tallies; usually this will just use the cache set up by Population::MaintainRegistry()
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
	double totalGenomeCount = pop.total_genome_count_;
	MutationRun &mutationRegistry = pop.mutation_registry_;
	const MutationIndex *mutations = mutationRegistry.begin_pointer_const();
	int mutationCount = static_cast<int>(mutationRegistry.end_pointer_const() - mutations);
	
	for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
	{
		const Mutation *mutation = mut_block_ptr + mutations[mutIndex];
		
		// if the user has selected a subrange of the chromosome, we will work from that
		if (hasSelection)
		{
			slim_position_t mutationPosition = mutation->position_;
			
			if ((mutationPosition < selectionFirstBase) || (mutationPosition > selectionLastBase))
				continue;
		}
		
		slim_refcount_t mutationRefCount = *(refcount_block_ptr + mutation->BlockIndex());
		double mutationFrequency = mutationRefCount / totalGenomeCount;
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
			
			doubleSpectrum[binIndex] = spectrum[binIndex] / static_cast<double>(total);
		}
	}
	
	// return the final tally; note this is a pointer in to our static ivar, and must not be freed!
	return doubleSpectrum;
}

void QtSLiMGraphView_FrequencySpectra::drawGraph(QPainter &painter, QRect interiorRect)
{
	int binCount = histogramBinCount_;
	int mutationTypeCount = static_cast<int>(controller_->sim->mutation_types_.size());
	double *spectrum = mutationFrequencySpectrum(mutationTypeCount);
	
	// plot our histogram bars
	drawGroupedBarplot(painter, interiorRect, spectrum, mutationTypeCount, binCount, 0.0, (1.0 / binCount));
	
	// if we have a limited selection range, overdraw a note about that
    bool hasSelection;
	slim_position_t selectionFirstBase;
	slim_position_t selectionLastBase;
    controller_->chromosomeSelection(&hasSelection, &selectionFirstBase, &selectionLastBase);
	
	if (hasSelection)
	{
        painter.setFont(QtSLiMGraphView::fontForTickLabels());
        painter.setBrush(Qt::darkGray);
		
        QString labelText = QString("%1 – %2").arg(selectionFirstBase).arg(selectionLastBase);
        QRect labelBoundingRect = painter.boundingRect(QRect(), Qt::TextDontClip | Qt::TextSingleLine, labelText);
		double labelX = interiorRect.x() + (interiorRect.width() - labelBoundingRect.width()) / 2.0;
		double labelY = interiorRect.y() + interiorRect.height() - (labelBoundingRect.height() + 4);
		
        labelY = painter.transform().map(QPointF(labelX, labelY)).y();
        
        painter.setWorldMatrixEnabled(false);
        painter.drawText(QPointF(labelX, labelY), labelText);
        painter.setWorldMatrixEnabled(true);
	}
}

QtSLiMLegendSpec QtSLiMGraphView_FrequencySpectra::legendKey(void)
{
	return mutationTypeLegendKey();     // we use the prefab mutation type legend
}

void QtSLiMGraphView_FrequencySpectra::controllerSelectionChanged(void)
{
    update();
}

bool QtSLiMGraphView_FrequencySpectra::providesStringForData(void)
{
    return true;
}

QString QtSLiMGraphView_FrequencySpectra::stringForData(void)
{
    QString string("# Graph data: Mutation frequency spectrum\n");
    
    // get the selected chromosome range
	bool hasSelection;
	slim_position_t selectionFirstBase;
	slim_position_t selectionLastBase;
    controller_->chromosomeSelection(&hasSelection, &selectionFirstBase, &selectionLastBase);

	if (hasSelection)
        string.append(QString("# Selected chromosome range: %1 – %2\n").arg(selectionFirstBase).arg(selectionLastBase));
	
    string.append(dateline());
    string.append("\n\n");
	
	int binCount = histogramBinCount_;
	SLiMSim *sim = controller_->sim;
	int mutationTypeCount = static_cast<int>(sim->mutation_types_.size());
	double *plotData = mutationFrequencySpectrum(mutationTypeCount);
	
	for (auto mutationTypeIter = sim->mutation_types_.begin(); mutationTypeIter != sim->mutation_types_.end(); ++mutationTypeIter)
	{
		MutationType *mutationType = (*mutationTypeIter).second;
		int mutationTypeIndex = mutationType->mutation_type_index_;		// look up the index used for this mutation type in the history info; not necessarily sequential!
		
        string.append(QString("\"m%1\", ").arg(mutationType->mutation_type_id_));
		
		for (int i = 0; i < binCount; ++i)
		{
			int histIndex = mutationTypeIndex + i * mutationTypeCount;
			
            string.append(QString("%1, ").arg(plotData[histIndex], 0, 'f', 4));
		}
		
        string.append("\n");
	}
	
	// Get rid of extra commas
    string.replace(", \n", "\n");
	
	return string;
}





























