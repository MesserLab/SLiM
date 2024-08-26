//
//  QtSLiMHaplotypeManager_GL.h
//  SLiM
//
//  Created by Ben Haller on 8/26/2024.
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

#include "QtSLiMHaplotypeManager.h"
#include "QtSLiMExtras.h"
#include "QtSLiMOpenGL.h"

#include <QDebug>

#include <vector>
#include <algorithm>
#include <utility>


void QtSLiMHaplotypeManager::glDrawSubpopStripsInRect(QRect interior)
{
    // Set up to draw rects
    SLIM_GL_PREPARE();
	
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
        
        SLIM_GL_PUSHRECT();
		
		float colorRed, colorGreen, colorBlue, colorAlpha;
		double hue = (genome_subpop_id - minSubpopID) / static_cast<double>(maxSubpopID - minSubpopID + 1);
        QColor hsbColor = QtSLiMColorWithHSV(hue, 1.0, 1.0, 1.0);
		QColor rgbColor = hsbColor.toRgb();
        
		colorRed = static_cast<float>(rgbColor.redF());
		colorGreen = static_cast<float>(rgbColor.greenF());
		colorBlue = static_cast<float>(rgbColor.blueF());
        colorAlpha = 1.0;
        
        SLIM_GL_PUSHRECT_COLORS();
        SLIM_GL_CHECKBUFFERS();
		
		genome_index++;
	}
	
	// Draw any leftovers
    SLIM_GL_FINISH();
}

void QtSLiMHaplotypeManager::glDrawDisplayListInRect(QRect interior, bool displayBW, int64_t **previousFirstBincounts)
{
	// Set up to draw rects
    SLIM_GL_PREPARE();
	
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
                    
                    SLIM_GL_PUSHRECT();
					
					float colorRed, colorGreen, colorBlue, colorAlpha = 1.0;
					
					if (displayBW) {
						colorRed = 0;					colorGreen = 0;						colorBlue = 0;
					} else {
						colorRed = mut_info.red_;		colorGreen = mut_info.green_;		colorBlue = mut_info.blue_;
					}
                    
                    SLIM_GL_PUSHRECT_COLORS();
                    SLIM_GL_CHECKBUFFERS();
				}
			}
		}
	}
	
	// Draw any leftovers
    SLIM_GL_FINISH();
}

#endif






































