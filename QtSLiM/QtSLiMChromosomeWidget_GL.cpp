//
//  QtSLiMChromosomeWidget_GL.cpp
//  SLiM
//
//  Created by Ben Haller on 8/25/2024.
//  Copyright (c) 2024 Philipp Messer.  All rights reserved.
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


#ifndef SLIM_NO_OPENGL

#include "QtSLiMChromosomeWidget.h"
#include "QtSLiMHaplotypeManager.h"
#include "QtSLiMOpenGL.h"

#include <QtDebug>

#include <map>
#include <algorithm>
#include <vector>


//
//  OpenGL-based drawing; maintain this in parallel with the Qt-based drawing!
//

void QtSLiMChromosomeWidget::glDrawRect(Species *displaySpecies)
{
    bool ready = isEnabled() && !controller_->invalidSimulation();
	QRect interiorRect = getInteriorRect();
    
    // if the simulation is at tick 0, it is not ready
	if (ready)
        if (controller_->community()->Tick() == 0)
			ready = false;
	
    if (ready)
    {
        // erase the content area itself
        glColor3f(0.0f, 0.0f, 0.0f);
		glRecti(interiorRect.left(), interiorRect.top(), interiorRect.left() + interiorRect.width(), interiorRect.top() + interiorRect.height());
        
        Chromosome *chromosome = focalChromosome();
		QtSLiMRange displayedRange = getDisplayedRange(chromosome);
		
		bool splitHeight = (shouldDrawRateMaps() && shouldDrawGenomicElements());
		QRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		int halfHeight = static_cast<int>(ceil(interiorRect.height() / 2.0));
		int remainingHeight = interiorRect.height() - halfHeight;
		
        topInteriorRect.setHeight(halfHeight);
        bottomInteriorRect.setHeight(remainingHeight);
        bottomInteriorRect.translate(0, halfHeight);
        
        // draw recombination intervals in interior
		if (shouldDrawRateMaps())
			glDrawRateMaps(splitHeight ? topInteriorRect : interiorRect, displaySpecies, displayedRange);
		
		// draw genomic elements in interior
		if (shouldDrawGenomicElements())
			glDrawGenomicElements(splitHeight ? bottomInteriorRect : interiorRect, chromosome, displayedRange);
		
		// figure out which mutation types we're displaying
		if (shouldDrawFixedSubstitutions() || shouldDrawMutations())
			updateDisplayedMutationTypes(displaySpecies);
		
		// draw fixed substitutions in interior
		if (shouldDrawFixedSubstitutions())
			glDrawFixedSubstitutions(interiorRect, displaySpecies, displayedRange);
		
		// draw mutations in interior
		if (shouldDrawMutations())
		{
			if (displayHaplotypes())
			{
				// display mutations as a haplotype plot, courtesy of QtSLiMHaplotypeManager; we use ClusterNearestNeighbor and
				// ClusterNoOptimization because they're fast, and NN might also provide a bit more run-to-run continuity
                // we cache the haplotype manager here, so our display remains constant across window resizes and other
                // invalidations; we toss the cache only when the simulation tells us that the model state has changed
                if (!haplotype_mgr_)
                {
                    size_t interiorHeight = static_cast<size_t>(interiorRect.height());	// one sample per available pixel line, for simplicity and speed; 47, in the current UI layout
                    haplotype_mgr_ = new QtSLiMHaplotypeManager(nullptr, QtSLiMHaplotypeManager::ClusterNearestNeighbor, QtSLiMHaplotypeManager::ClusterNoOptimization, controller_, displaySpecies, displayedRange, interiorHeight, false);
                }
                
                if (haplotype_mgr_)
                    haplotype_mgr_->glDrawHaplotypes(interiorRect, false, false, false, &haplotype_previous_bincounts);
			}
			else
			{
				// display mutations as a frequency plot; this is the standard display mode
                glDrawMutations(interiorRect, displaySpecies, displayedRange);
			}
		}
    }
    else
    {
        // erase the content area itself
		glColor3f(0.88f, 0.88f, 0.88f);
        glRecti(0, 0, interiorRect.width(), interiorRect.height());
    }
}

void QtSLiMChromosomeWidget::glDrawGenomicElements(QRect &interiorRect, Chromosome *chromosome, QtSLiMRange displayedRange)
{
	int previousIntervalLeftEdge = -10000;
	
	SLIM_GL_PREPARE();
	
	for (GenomicElement *genomicElement : chromosome->GenomicElements())
	{
		slim_position_t startPosition = genomicElement->start_position_;
		slim_position_t endPosition = genomicElement->end_position_;
		QRect elementRect = rectEncompassingBaseToBase(startPosition, endPosition, interiorRect, displayedRange);
		bool widthOne = (elementRect.width() == 1);
		
		// We want to avoid overdrawing width-one intervals, which are important but small, so if the previous interval was width-one,
		// and we are not, and we are about to overdraw it, then we scoot our left edge over one pixel to leave it alone.
		if (!widthOne && (elementRect.left() == previousIntervalLeftEdge))
            elementRect.adjust(1, 0, 0, 0);
		
		// draw only the visible part, if any
        elementRect = elementRect.intersected(interiorRect);
		
		if (!elementRect.isEmpty())
		{
			GenomicElementType *geType = genomicElement->genomic_element_type_ptr_;
			float colorRed, colorGreen, colorBlue, colorAlpha;
			
			if (!geType->color_.empty())
			{
				colorRed = geType->color_red_;
				colorGreen = geType->color_green_;
				colorBlue = geType->color_blue_;
				colorAlpha = 1.0;
			}
			else
			{
				slim_objectid_t elementTypeID = geType->genomic_element_type_id_;
                
				controller_->colorForGenomicElementType(geType, elementTypeID, &colorRed, &colorGreen, &colorBlue, &colorAlpha);
			}
			
			SLIM_GL_DEFCOORDS(elementRect);
			SLIM_GL_PUSHRECT();
			SLIM_GL_PUSHRECT_COLORS();
			SLIM_GL_CHECKBUFFERS();
			
			// if this interval is just one pixel wide, we want to try to make it visible, by avoiding overdrawing it; so we remember its location
			if (widthOne)
				previousIntervalLeftEdge = elementRect.left();
			else
				previousIntervalLeftEdge = -10000;
		}
	}
	
	SLIM_GL_FINISH();
}

void QtSLiMChromosomeWidget::glDrawMutations(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange)
{
	double scalingFactor = 0.8; // used to be controller->selectionColorScale;
	Population &pop = displaySpecies->population_;
	double totalHaplosomeCount = pop.gui_total_haplosome_count_;				// this includes only haplosomes in the selected subpopulations
    int registry_size;
    const MutationIndex *registry = pop.MutationRegistry(&registry_size);
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	// Set up to draw rects
	float colorRed = 0.0f, colorGreen = 0.0f, colorBlue = 0.0f, colorAlpha = 1.0;
	
	SLIM_GL_PREPARE();
	
	if ((registry_size < 1000) || (displayedRange.length < interiorRect.width()))
	{
		// This is the simple version of the display code, avoiding the memory allocations and such
		for (int registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			const Mutation *mutation = mut_block_ptr + registry[registry_index];
			const MutationType *mutType = mutation->mutation_type_ptr_;
			
			if (mutType->mutation_type_displayed_)
			{
				slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
				slim_position_t mutationPosition = mutation->position_;
				QRect mutationTickRect = rectEncompassingBaseToBase(mutationPosition, mutationPosition, interiorRect, displayedRange);
				
				if (!mutType->color_.empty())
				{
					colorRed = mutType->color_red_;
					colorGreen = mutType->color_green_;
					colorBlue = mutType->color_blue_;
				}
				else
				{
					RGBForSelectionCoeff(static_cast<double>(mutation->selection_coeff_), &colorRed, &colorGreen, &colorBlue, scalingFactor);
				}
				
                int height_adjust = mutationTickRect.height() - static_cast<int>(ceil((mutationRefCount / totalHaplosomeCount) * interiorRect.height()));
                mutationTickRect.setTop(mutationTickRect.top() + height_adjust);
                
				SLIM_GL_DEFCOORDS(mutationTickRect);
				SLIM_GL_PUSHRECT();
				SLIM_GL_PUSHRECT_COLORS();
				SLIM_GL_CHECKBUFFERS();
			}
		}
	}
	else
	{
		// We have a lot of mutations, so let's try to be smarter.  It's hard to be smarter.  The overhead from allocating the NSColors and such
		// is pretty negligible; practially all the time is spent in NSRectFill().  Unfortunately, NSRectFillListWithColors() provides basically
		// no speedup; Apple doesn't appear to have optimized it.  So, here's what I came up with.  For each mutation type that uses a fixed DFE,
		// and thus a fixed color, we can do a radix sort of mutations into bins corresponding to each pixel in our displayed image.  Then we
		// can draw each bin just once, making one bar for the highest bar in that bin.  Mutations from non-fixed DFEs, and mutations which have
		// had their selection coefficient changed, will be drawn at the end in the usual (slow) way.
		int displayPixelWidth = interiorRect.width();
		int16_t *heightBuffer = static_cast<int16_t *>(malloc(static_cast<size_t>(displayPixelWidth) * sizeof(int16_t)));
		bool *mutationsPlotted = static_cast<bool *>(calloc(static_cast<size_t>(registry_size), sizeof(bool)));	// faster than using gui_scratch_reference_count_ because of cache locality
		int64_t remainingMutations = registry_size;
		
		// First zero out the scratch refcount, which we use to track which mutations we have drawn already
		//for (int mutIndex = 0; mutIndex < mutationCount; ++mutIndex)
		//	mutations[mutIndex]->gui_scratch_reference_count_ = 0;
		
		// Then loop through the declared mutation types
		std::map<slim_objectid_t,MutationType*> &mut_types = displaySpecies->mutation_types_;
		bool draw_muttypes_sequentially = (mut_types.size() <= 20);	// with a lot of mutation types, the algorithm below becomes very inefficient
		
		for (auto mutationTypeIter : mut_types)
		{
			MutationType *mut_type = mutationTypeIter.second;
			
			if (mut_type->mutation_type_displayed_)
			{
				if (draw_muttypes_sequentially)
				{
					bool mut_type_fixed_color = !mut_type->color_.empty();
					
					// We optimize fixed-DFE mutation types only, and those using a fixed color set by the user
					if ((mut_type->dfe_type_ == DFEType::kFixed) || mut_type_fixed_color)
					{
						slim_selcoeff_t mut_type_selcoeff = (mut_type_fixed_color ? 0.0 : static_cast<slim_selcoeff_t>(mut_type->dfe_parameters_[0]));
						
						EIDOS_BZERO(heightBuffer, static_cast<size_t>(displayPixelWidth) * sizeof(int16_t));
						
						// Scan through the mutation list for mutations of this type with the right selcoeff
						for (int registry_index = 0; registry_index < registry_size; ++registry_index)
						{
							const Mutation *mutation = mut_block_ptr + registry[registry_index];
							
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfloat-equal"
                            // We do want to do an exact floating-point equality compare here; we want to see whether the mutation's selcoeff is unmodified from the fixed DFE
							if ((mutation->mutation_type_ptr_ == mut_type) && (mut_type_fixed_color || (mutation->selection_coeff_ == mut_type_selcoeff)))
#pragma clang diagnostic pop
#pragma GCC diagnostic pop
							{
								slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// includes only refs from the selected subpopulations
								slim_position_t mutationPosition = mutation->position_;
								//NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
								//int xPos = (int)(mutationTickRect.origin.x - interiorRect.origin.x);
								int xPos = LEFT_OFFSET_OF_BASE(mutationPosition, interiorRect, displayedRange);
								int16_t barHeight = static_cast<int16_t>(ceil((mutationRefCount / totalHaplosomeCount) * interiorRect.height()));
								
								if ((xPos >= 0) && (xPos < displayPixelWidth))
									if (barHeight > heightBuffer[xPos])
										heightBuffer[xPos] = barHeight;
								
								// tally this mutation as handled
								//mutation->gui_scratch_reference_count_ = 1;
								mutationsPlotted[registry_index] = true;
								--remainingMutations;
							}
						}
						
						// Now draw all of the mutations we found, by looping through our radix bins
						if (mut_type_fixed_color)
						{
							colorRed = mut_type->color_red_;
							colorGreen = mut_type->color_green_;
							colorBlue = mut_type->color_blue_;
						}
						else
						{
							RGBForSelectionCoeff(static_cast<double>(mut_type_selcoeff), &colorRed, &colorGreen, &colorBlue, scalingFactor);
						}
						
						for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
						{
							int barHeight = heightBuffer[binIndex];
							
							if (barHeight)
							{
								QRect mutationTickRect(interiorRect.x() + binIndex, interiorRect.y(), 1, interiorRect.height());
                                mutationTickRect.setTop(mutationTickRect.top() + interiorRect.height() - barHeight);
								
								SLIM_GL_DEFCOORDS(mutationTickRect);
								SLIM_GL_PUSHRECT();
								SLIM_GL_PUSHRECT_COLORS();
								SLIM_GL_CHECKBUFFERS();
							}
						}
					}
				}
			}
			else
			{
				// We're not displaying this mutation type, so we need to mark off all the mutations belonging to it as handled
				for (int registry_index = 0; registry_index < registry_size; ++registry_index)
				{
					const Mutation *mutation = mut_block_ptr + registry[registry_index];
					
					if (mutation->mutation_type_ptr_ == mut_type)
					{
						// tally this mutation as handled
						//mutation->gui_scratch_reference_count_ = 1;
						mutationsPlotted[registry_index] = true;
						--remainingMutations;
					}
				}
			}
		}
		
		// Draw any undrawn mutations on top; these are guaranteed not to use a fixed color set by the user, since those are all handled above
		if (remainingMutations)
		{
			if (remainingMutations < 1000)
			{
				// Plot the remainder by brute force, since there are not that many
				for (int registry_index = 0; registry_index < registry_size; ++registry_index)
				{
					//if (mutation->gui_scratch_reference_count_ == 0)
					if (!mutationsPlotted[registry_index])
					{
						const Mutation *mutation = mut_block_ptr + registry[registry_index];
						slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
						slim_position_t mutationPosition = mutation->position_;
                        QRect mutationTickRect = rectEncompassingBaseToBase(mutationPosition, mutationPosition, interiorRect, displayedRange);
                        int height_adjust = mutationTickRect.height() - static_cast<int>(ceil((mutationRefCount / totalHaplosomeCount) * interiorRect.height()));
						
                        mutationTickRect.setTop(mutationTickRect.top() + height_adjust);
						RGBForSelectionCoeff(static_cast<double>(mutation->selection_coeff_), &colorRed, &colorGreen, &colorBlue, scalingFactor);
						
						SLIM_GL_DEFCOORDS(mutationTickRect);
						SLIM_GL_PUSHRECT();
						SLIM_GL_PUSHRECT_COLORS();
						SLIM_GL_CHECKBUFFERS();
					}
				}
			}
			else
			{
				// OK, we have a lot of mutations left to draw.  Here we will again use the radix sort trick, to keep track of only the tallest bar in each column
				MutationIndex *mutationBuffer = static_cast<MutationIndex *>(calloc(static_cast<size_t>(displayPixelWidth),  sizeof(MutationIndex)));
				
				EIDOS_BZERO(heightBuffer, static_cast<size_t>(displayPixelWidth) * sizeof(int16_t));
				
				// Find the tallest bar in each column
				for (int registry_index = 0; registry_index < registry_size; ++registry_index)
				{
					//if (mutation->gui_scratch_reference_count_ == 0)
					if (!mutationsPlotted[registry_index])
					{
						MutationIndex mutationBlockIndex = registry[registry_index];
						const Mutation *mutation = mut_block_ptr + mutationBlockIndex;
						slim_refcount_t mutationRefCount = mutation->gui_reference_count_;		// this includes only references made from the selected subpopulations
						slim_position_t mutationPosition = mutation->position_;
						//NSRect mutationTickRect = [self rectEncompassingBase:mutationPosition toBase:mutationPosition interiorRect:interiorRect displayedRange:displayedRange];
						//int xPos = (int)(mutationTickRect.origin.x - interiorRect.origin.x);
						int xPos = LEFT_OFFSET_OF_BASE(mutationPosition, interiorRect, displayedRange);
						int16_t barHeight = static_cast<int16_t>(ceil((mutationRefCount / totalHaplosomeCount) * interiorRect.height()));
						
						if ((xPos >= 0) && (xPos < displayPixelWidth))
						{
							if (barHeight > heightBuffer[xPos])
							{
								heightBuffer[xPos] = barHeight;
								mutationBuffer[xPos] = mutationBlockIndex;
							}
						}
					}
				}
				
				// Now plot the bars
				for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
				{
					int barHeight = heightBuffer[binIndex];
					
					if (barHeight)
					{
                        QRect mutationTickRect(interiorRect.x() + binIndex, interiorRect.y(), 1, interiorRect.height());
                        mutationTickRect.setTop(mutationTickRect.top() + interiorRect.height() - barHeight);
                        
						const Mutation *mutation = mut_block_ptr + mutationBuffer[binIndex];
						
						RGBForSelectionCoeff(static_cast<double>(mutation->selection_coeff_), &colorRed, &colorGreen, &colorBlue, scalingFactor);
						
						SLIM_GL_DEFCOORDS(mutationTickRect);
						SLIM_GL_PUSHRECT();
						SLIM_GL_PUSHRECT_COLORS();
						SLIM_GL_CHECKBUFFERS();
					}
				}
				
				free(mutationBuffer);
			}
		}
		
		free(heightBuffer);
		free(mutationsPlotted);
	}
	
	SLIM_GL_FINISH();
}

void QtSLiMChromosomeWidget::glDrawFixedSubstitutions(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange)
{
    double scalingFactor = 0.8; // used to be controller->selectionColorScale;
	Population &pop = displaySpecies->population_;
    Chromosome *chromosome = focalChromosome();
	bool chromosomeHasDefaultColor = !chromosome->color_sub_.empty();
	std::vector<Substitution*> &substitutions = pop.substitutions_;
	
	// Set up to draw rects
	float colorRed = 0.2f, colorGreen = 0.2f, colorBlue = 1.0f, colorAlpha = 1.0;
	
	if (chromosomeHasDefaultColor)
	{
		colorRed = chromosome->color_sub_red_;
		colorGreen = chromosome->color_sub_green_;
		colorBlue = chromosome->color_sub_blue_;
	}
	
	SLIM_GL_PREPARE();
	
	if ((substitutions.size() < 1000) || (displayedRange.length < interiorRect.width()))
	{
		// This is the simple version of the display code, avoiding the memory allocations and such
		for (const Substitution *substitution : substitutions)
		{
			if (substitution->mutation_type_ptr_->mutation_type_displayed_)
			{
				slim_position_t substitutionPosition = substitution->position_;
				QRect substitutionTickRect = rectEncompassingBaseToBase(substitutionPosition, substitutionPosition, interiorRect, displayedRange);
				
				if (!shouldDrawMutations() || !chromosomeHasDefaultColor)
				{
					// If we're drawing mutations as well, then substitutions just get colored blue (set above), to contrast
					// If we're not drawing mutations as well, then substitutions get colored by selection coefficient, like mutations
					const MutationType *mutType = substitution->mutation_type_ptr_;
					
					if (!mutType->color_sub_.empty())
					{
						colorRed = mutType->color_sub_red_;
						colorGreen = mutType->color_sub_green_;
						colorBlue = mutType->color_sub_blue_;
					}
					else
					{
						RGBForSelectionCoeff(static_cast<double>(substitution->selection_coeff_), &colorRed, &colorGreen, &colorBlue, scalingFactor);
					}
				}
				
				SLIM_GL_DEFCOORDS(substitutionTickRect);
				SLIM_GL_PUSHRECT();
				SLIM_GL_PUSHRECT_COLORS();
				SLIM_GL_CHECKBUFFERS();
			}
		}
	}
	else
	{
		// We have a lot of substitutions, so do a radix sort, as we do in drawMutationsInInteriorRect: below.
		int displayPixelWidth = interiorRect.width();
		const Substitution **subBuffer = static_cast<const Substitution **>(calloc(static_cast<size_t>(displayPixelWidth), sizeof(Substitution *)));
		
		for (const Substitution *substitution : substitutions)
		{
			if (substitution->mutation_type_ptr_->mutation_type_displayed_)
			{
				slim_position_t substitutionPosition = substitution->position_;
				double startFraction = (substitutionPosition - static_cast<slim_position_t>(displayedRange.location)) / static_cast<double>(displayedRange.length);
				int xPos = static_cast<int>(floor(startFraction * interiorRect.width()));
				
				if ((xPos >= 0) && (xPos < displayPixelWidth))
					subBuffer[xPos] = substitution;
			}
		}
		
		if (shouldDrawMutations() && chromosomeHasDefaultColor)
		{
			// If we're drawing mutations as well, then substitutions just get colored blue, to contrast
			QRect mutationTickRect = interiorRect;
            
			for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
			{
				const Substitution *substitution = subBuffer[binIndex];
				
				if (substitution)
				{
                    mutationTickRect.setX(interiorRect.x() + binIndex);
                    mutationTickRect.setWidth(1);
                    
					// consolidate adjacent lines together, since they are all the same color
					while ((binIndex + 1 < displayPixelWidth) && subBuffer[binIndex + 1])
					{
                        mutationTickRect.setWidth(mutationTickRect.width() + 1);
						binIndex++;
					}
					
					SLIM_GL_DEFCOORDS(mutationTickRect);
					SLIM_GL_PUSHRECT();
					SLIM_GL_PUSHRECT_COLORS();
					SLIM_GL_CHECKBUFFERS();
				}
			}
		}
		else
		{
			// If we're not drawing mutations as well, then substitutions get colored by selection coefficient, like mutations
            QRect mutationTickRect = interiorRect;
			
			for (int binIndex = 0; binIndex < displayPixelWidth; ++binIndex)
			{
				const Substitution *substitution = subBuffer[binIndex];
				
				if (substitution)
				{
					const MutationType *mutType = substitution->mutation_type_ptr_;
					
					if (!mutType->color_sub_.empty())
					{
						colorRed = mutType->color_sub_red_;
						colorGreen = mutType->color_sub_green_;
						colorBlue = mutType->color_sub_blue_;
					}
					else
					{
						RGBForSelectionCoeff(static_cast<double>(substitution->selection_coeff_), &colorRed, &colorGreen, &colorBlue, scalingFactor);
					}
					
                    mutationTickRect.setX(interiorRect.x() + binIndex);
                    mutationTickRect.setWidth(1);
					SLIM_GL_DEFCOORDS(mutationTickRect);
					SLIM_GL_PUSHRECT();
					SLIM_GL_PUSHRECT_COLORS();
					SLIM_GL_CHECKBUFFERS();
				}
			}
		}
		
		free(subBuffer);
	}
	
	SLIM_GL_FINISH();
}

void QtSLiMChromosomeWidget::_glDrawRateMapIntervals(QRect &interiorRect, __attribute__((__unused__)) Species *displaySpecies, QtSLiMRange displayedRange, std::vector<slim_position_t> &ends, std::vector<double> &rates, double hue)
{
	size_t recombinationIntervalCount = ends.size();
	slim_position_t intervalStartPosition = 0;
	int previousIntervalLeftEdge = -10000;
	
	SLIM_GL_PREPARE();
	
	for (size_t interval = 0; interval < recombinationIntervalCount; ++interval)
	{
		slim_position_t intervalEndPosition = ends[interval];
		double intervalRate = rates[interval];
		QRect intervalRect = rectEncompassingBaseToBase(intervalStartPosition, intervalEndPosition, interiorRect, displayedRange);
		bool widthOne = (intervalRect.width() == 1);
		
		// We want to avoid overdrawing width-one intervals, which are important but small, so if the previous interval was width-one,
		// and we are not, and we are about to overdraw it, then we scoot our left edge over one pixel to leave it alone.
		if (!widthOne && (intervalRect.left() == previousIntervalLeftEdge))
            intervalRect.adjust(1, 0, 0, 0);
		
		// draw only the visible part, if any
        intervalRect = intervalRect.intersected(interiorRect);
		
        if (!intervalRect.isEmpty())
		{
			// color according to how "hot" the region is
            float colorRed, colorGreen, colorBlue, colorAlpha;
			
			if (intervalRate == 0.0)
			{
				// a recombination or mutation rate of 0.0 comes out as black, whereas the lowest brightness below is 0.5; we want to distinguish this
				colorRed = colorGreen = colorBlue = 0.0;
				colorAlpha = 1.0;
			}
			else
			{
				// the formula below scales 1e-6 to 1.0 and 1e-9 to 0.0; values outside that range clip, but we want there to be
				// reasonable contrast within the range of values commonly used, so we don't want to make the range too wide
				double lightness, brightness = 1.0, saturation = 1.0;
				
				lightness = (log10(intervalRate) + 9.0) / 3.0;
				lightness = std::max(lightness, 0.0);
				lightness = std::min(lightness, 1.0);
				
				if (lightness >= 0.5)	saturation = 1.0 - ((lightness - 0.5) * 2.0);	// goes from 1.0 at lightness 0.5 to 0.0 at lightness 1.0
				else					brightness = 0.5 + lightness;					// goes from 1.0 at lightness 0.5 to 0.5 at lightness 0.0
				
                QColor intervalColor = QtSLiMColorWithHSV(hue, saturation, brightness, 1.0);

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
                // In Qt5, getRgbF() expects pointers to qreal, which is double
                double r, g, b, a;
				intervalColor.getRgbF(&r, &g, &b, &a);
                
                colorRed = static_cast<float>(r);
                colorGreen = static_cast<float>(g);
                colorBlue = static_cast<float>(b);
                colorAlpha = static_cast<float>(a);
#else
                // In Qt6, getRgbF() expects pointers to float
                intervalColor.getRgbF(&colorRed, &colorGreen, &colorBlue, &colorAlpha);
#endif
			}
			
			SLIM_GL_DEFCOORDS(intervalRect);
			SLIM_GL_PUSHRECT();
			SLIM_GL_PUSHRECT_COLORS();
			SLIM_GL_CHECKBUFFERS();
			
			// if this interval is just one pixel wide, we want to try to make it visible, by avoiding overdrawing it; so we remember its location
			if (widthOne)
				previousIntervalLeftEdge = intervalRect.left();
			else
				previousIntervalLeftEdge = -10000;
		}
		
		// the next interval starts at the next base after this one ended
		intervalStartPosition = intervalEndPosition + 1;
	}
	
	SLIM_GL_FINISH();
}

void QtSLiMChromosomeWidget::glDrawRecombinationIntervals(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange)
{
    Chromosome *chromosome = focalChromosome();
	
	if (chromosome->single_recombination_map_)
	{
		_glDrawRateMapIntervals(interiorRect, displaySpecies, displayedRange, chromosome->recombination_end_positions_H_, chromosome->recombination_rates_H_, 0.65);
	}
	else
	{
		QRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		int halfHeight = static_cast<int>(ceil(interiorRect.height() / 2.0));
		int remainingHeight = interiorRect.height() - halfHeight;
		
        topInteriorRect.setHeight(halfHeight);
        bottomInteriorRect.setHeight(remainingHeight);
        bottomInteriorRect.translate(0, halfHeight);
		
		_glDrawRateMapIntervals(topInteriorRect, displaySpecies, displayedRange, chromosome->recombination_end_positions_M_, chromosome->recombination_rates_M_, 0.65);
		_glDrawRateMapIntervals(bottomInteriorRect, displaySpecies, displayedRange, chromosome->recombination_end_positions_F_, chromosome->recombination_rates_F_, 0.65);
	}
}

void QtSLiMChromosomeWidget::glDrawMutationIntervals(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange)
{
    Chromosome *chromosome = focalChromosome();
	
	if (chromosome->single_mutation_map_)
	{
		_glDrawRateMapIntervals(interiorRect, displaySpecies, displayedRange, chromosome->mutation_end_positions_H_, chromosome->mutation_rates_H_, 0.75);
	}
	else
	{
		QRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		int halfHeight = static_cast<int>(ceil(interiorRect.height() / 2.0));
		int remainingHeight = interiorRect.height() - halfHeight;
		
        topInteriorRect.setHeight(halfHeight);
        bottomInteriorRect.setHeight(remainingHeight);
        bottomInteriorRect.translate(0, halfHeight);
		
		_glDrawRateMapIntervals(topInteriorRect, displaySpecies, displayedRange, chromosome->mutation_end_positions_M_, chromosome->mutation_rates_M_, 0.75);
		_glDrawRateMapIntervals(bottomInteriorRect, displaySpecies, displayedRange, chromosome->mutation_end_positions_F_, chromosome->mutation_rates_F_, 0.75);
	}
}

void QtSLiMChromosomeWidget::glDrawRateMaps(QRect &interiorRect, Species *displaySpecies, QtSLiMRange displayedRange)
{
    Chromosome *chromosome = focalChromosome();
	bool recombinationWorthShowing = false;
	bool mutationWorthShowing = false;
	
	if (chromosome->single_mutation_map_)
		mutationWorthShowing = (chromosome->mutation_end_positions_H_.size() > 1);
	else
		mutationWorthShowing = ((chromosome->mutation_end_positions_M_.size() > 1) || (chromosome->mutation_end_positions_F_.size() > 1));
	
	if (chromosome->single_recombination_map_)
		recombinationWorthShowing = (chromosome->recombination_end_positions_H_.size() > 1);
	else
		recombinationWorthShowing = ((chromosome->recombination_end_positions_M_.size() > 1) || (chromosome->recombination_end_positions_F_.size() > 1));
	
	// If neither map is worth showing, we show just the recombination map, to mirror the behavior of 2.4 and earlier
	if ((!mutationWorthShowing && !recombinationWorthShowing) || (!mutationWorthShowing && recombinationWorthShowing))
	{
		glDrawRecombinationIntervals(interiorRect, displaySpecies, displayedRange);
	}
	else if (mutationWorthShowing && !recombinationWorthShowing)
	{
		glDrawMutationIntervals(interiorRect, displaySpecies, displayedRange);
	}
	else	// mutationWorthShowing && recombinationWorthShowing
	{
		QRect topInteriorRect = interiorRect, bottomInteriorRect = interiorRect;
		int halfHeight = static_cast<int>(ceil(interiorRect.height() / 2.0));
		int remainingHeight = interiorRect.height() - halfHeight;
		
        topInteriorRect.setHeight(halfHeight);
        bottomInteriorRect.setHeight(remainingHeight);
        bottomInteriorRect.translate(0, halfHeight);
        
		glDrawRecombinationIntervals(topInteriorRect, displaySpecies, displayedRange);
		glDrawMutationIntervals(bottomInteriorRect, displaySpecies, displayedRange);
	}
}

#endif



























