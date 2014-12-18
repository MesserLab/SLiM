//
//  genome.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/software/
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


#include "genome.h"

// return a merged genome consisting only of the mutations that are present in both p_genome1 and p_genome2
Genome GenomeWithFixedMutations(Genome& p_genome1, Genome& p_genome2)
{
	Genome merge_genome;
	
	std::vector<Mutation>::iterator genome1_iter = p_genome1.begin();
	std::vector<Mutation>::iterator genome2_iter = p_genome2.begin();
	
	std::vector<Mutation>::iterator genome1_max = p_genome1.end();
	std::vector<Mutation>::iterator genome2_max = p_genome2.end();
	
	while (genome1_iter != genome1_max && genome2_iter != genome2_max)
	{
		// advance genome1_iter while genome1_iter.x < genome2_iter.x
		while (genome1_iter != genome1_max && genome2_iter != genome2_max && (*genome1_iter).position_ < (*genome2_iter).position_)
			genome1_iter++;
		
		// advance genome2_iter while genome2_iter.x < genome1_iter.x
		while (genome1_iter != genome1_max && genome2_iter != genome2_max && (*genome2_iter).position_ < (*genome1_iter).position_)
			genome2_iter++;
		
		// identify shared mutations at positions x and add to G
		if (genome2_iter != genome2_max && genome1_iter != genome1_max && (*genome2_iter).position_ == (*genome1_iter).position_)
		{
			int position = (*genome1_iter).position_;
			
			std::vector<Mutation>::iterator temp;
			
			while (genome1_iter != genome1_max && (*genome1_iter).position_ == position)
			{
				temp = genome2_iter;
				
				while (temp != genome2_max && (*temp).position_ == position)
				{
					if ((*temp).mutation_type_==(*genome1_iter).mutation_type_ && (*temp).selection_coeff_==(*genome1_iter).selection_coeff_)
						merge_genome.push_back(*genome1_iter);
					
					temp++;
				}
				
				genome1_iter++;
			}
		}
	}
	
	return merge_genome;
}

// return a merged genome consisting only of the mutations in p_genome1 that are not in p_genome2
Genome GenomeWithPolymorphicMutations(Genome& p_genome1, Genome& p_genome2)
{
	Genome merge_genome;
	
	std::vector<Mutation>::iterator genome1_iter = p_genome1.begin();
	std::vector<Mutation>::iterator genome2_iter = p_genome2.begin();
	
	std::vector<Mutation>::iterator genome1_max = p_genome1.end();
	std::vector<Mutation>::iterator genome2_max = p_genome2.end();
	
	while (genome1_iter != genome1_max && genome2_iter != genome2_max)
	{
		// advance genome1_iter while genome1_iter.position_ < genome2_iter.position_
		while (genome1_iter != genome1_max && genome2_iter != genome2_max && (*genome1_iter).position_ < (*genome2_iter).position_)
		{
			merge_genome.push_back(*genome1_iter);
			genome1_iter++;
		}
		
		// advance genome2_iter while genome2_iter.position_ < genome1_iter.position_
		while (genome2_iter != genome2_max && genome1_iter != genome1_max && (*genome2_iter).position_ < (*genome1_iter).position_)
			genome2_iter++;
		
		// identify polymorphic mutations at position_ and add to merge_genome
		if (genome2_iter != genome2_max && genome1_iter != genome1_max && (*genome2_iter).position_ == (*genome1_iter).position_)
		{
			int position = (*genome1_iter).position_;
			
			// go through p_genome1 and check for those mutations that are not present in p_genome2
			std::vector<Mutation>::iterator temp = genome2_iter;
			
			while (genome1_iter != genome1_max && (*genome1_iter).position_ == position)
			{
				bool polymorphic = true;
				
				while (temp != genome2_max && (*temp).position_ == position)
				{
					if ((*genome1_iter).mutation_type_==(*temp).mutation_type_ && (*genome1_iter).selection_coeff_==(*temp).selection_coeff_)
						polymorphic = false;
					
					temp++;
				}
				
				if (polymorphic)
					merge_genome.push_back(*genome1_iter);
				
				genome1_iter++;
			}
			
			while (genome2_iter != genome2_max && (*genome2_iter).position_ == position)
				genome2_iter++;
		}
	}
	
	while (genome1_iter != genome1_max)
	{
		merge_genome.push_back(*genome1_iter);
		genome1_iter++;
	}
	
	return merge_genome;
}




































































