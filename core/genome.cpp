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
#include "stacktrace.h"


#ifdef DEBUG
bool Genome::s_log_copy_and_assign_ = true;
#endif

// set p_merge_genome to be a merged genome consisting only of the mutations that are present in both p_genome1 and p_genome2
// NOTE this function is not used in the present design; it has been obsoleted by Population::ManageMutationReferencesAndRemoveFixedMutations(int p_generation).
void GenomeWithFixedMutations(const Genome &p_genome1, const Genome &p_genome2, Genome *p_merge_genome)
{
	p_merge_genome->clear();
	
	const Mutation **genome1_iter = p_genome1.begin_pointer();
	const Mutation **genome2_iter = p_genome2.begin_pointer();
	
	const Mutation **genome1_max = p_genome1.end_pointer();
	const Mutation **genome2_max = p_genome2.end_pointer();
	
	while (genome1_iter != genome1_max && genome2_iter != genome2_max)
	{
		// advance genome1_iter while genome1_iter.x < genome2_iter.x
		while (genome1_iter != genome1_max && genome2_iter != genome2_max && (*genome1_iter)->position_ < (*genome2_iter)->position_)
			genome1_iter++;
		
		// advance genome2_iter while genome2_iter.x < genome1_iter.x
		while (genome1_iter != genome1_max && genome2_iter != genome2_max && (*genome2_iter)->position_ < (*genome1_iter)->position_)
			genome2_iter++;
		
		// identify shared mutations at positions x and add to G
		if (genome2_iter != genome2_max && genome1_iter != genome1_max && (*genome2_iter)->position_ == (*genome1_iter)->position_)
		{
			int position = (*genome1_iter)->position_;
			
			const Mutation **temp;
			
			while (genome1_iter != genome1_max && (*genome1_iter)->position_ == position)
			{
				temp = genome2_iter;
				
				while (temp != genome2_max && (*temp)->position_ == position)
				{
					if ((*temp)->mutation_type_ptr_ == (*genome1_iter)->mutation_type_ptr_ && (*temp)->selection_coeff_ == (*genome1_iter)->selection_coeff_)
						p_merge_genome->push_back(*genome1_iter);
					
					temp++;
				}
				
				genome1_iter++;
			}
		}
	}
}

// set p_merge_genome to be a merged genome consisting only of the mutations in p_genome1 that are not in p_genome2
void GenomeWithPolymorphicMutations(const Genome &p_genome1, const Genome &p_genome2, Genome *p_merge_genome)
{
	p_merge_genome->clear();
	
	const Mutation **genome1_iter = p_genome1.begin_pointer();
	const Mutation **genome2_iter = p_genome2.begin_pointer();
	
	const Mutation **genome1_max = p_genome1.end_pointer();
	const Mutation **genome2_max = p_genome2.end_pointer();
	
	while (genome1_iter != genome1_max && genome2_iter != genome2_max)
	{
		// advance genome1_iter while genome1_iter.position_ < genome2_iter.position_
		while (genome1_iter != genome1_max && genome2_iter != genome2_max && (*genome1_iter)->position_ < (*genome2_iter)->position_)
		{
			p_merge_genome->push_back(*genome1_iter);
			genome1_iter++;
		}
		
		// advance genome2_iter while genome2_iter.position_ < genome1_iter.position_
		while (genome2_iter != genome2_max && genome1_iter != genome1_max && (*genome2_iter)->position_ < (*genome1_iter)->position_)
			genome2_iter++;
		
		// identify polymorphic mutations at position_ and add to merge_genome
		if (genome2_iter != genome2_max && genome1_iter != genome1_max && (*genome2_iter)->position_ == (*genome1_iter)->position_)
		{
			int position = (*genome1_iter)->position_;
			
			// go through p_genome1 and check for those mutations that are not present in p_genome2
			const Mutation **temp = genome2_iter;
			
			while (genome1_iter != genome1_max && (*genome1_iter)->position_ == position)
			{
				bool polymorphic = true;
				
				while (temp != genome2_max && (*temp)->position_ == position)
				{
					if ((*genome1_iter)->mutation_type_ptr_ == (*temp)->mutation_type_ptr_ && (*genome1_iter)->selection_coeff_ == (*temp)->selection_coeff_)
						polymorphic = false;
					
					temp++;
				}
				
				if (polymorphic)
					p_merge_genome->push_back(*genome1_iter);
				
				genome1_iter++;
			}
			
			while (genome2_iter != genome2_max && (*genome2_iter)->position_ == position)
				genome2_iter++;
		}
	}
	
	while (genome1_iter != genome1_max)
	{
		p_merge_genome->push_back(*genome1_iter);
		genome1_iter++;
	}
}


//
//	Methods to enforce limited copying
//

Genome::Genome(const Genome &p_original)
{
#ifdef DEBUG
	if (s_log_copy_and_assign_)
	{
		std::clog << "********* Genome::Genome(Genome&) called!" << std::endl;
		print_stacktrace(stderr);
		std::clog << "************************************************" << std::endl;
	}
#endif
	
	int source_mutation_count = p_original.mutation_count_;
	
	// first we need to ensure that we have sufficient capacity
	if (source_mutation_count > mutation_capacity_)
	{
		mutation_capacity_ = p_original.mutation_capacity_;		// just use the same capacity as the source
		mutations_ = (const Mutation **)realloc(mutations_, mutation_capacity_ * sizeof(Mutation*));
	}
	
	// then copy all pointers from the source to ourselves
	memcpy(mutations_, p_original.mutations_, source_mutation_count * sizeof(Mutation*));
	mutation_count_ = source_mutation_count;
}

Genome& Genome::operator= (const Genome& p_original)
{
#ifdef DEBUG
	if (s_log_copy_and_assign_)
	{
		std::clog << "********* Genome::operator=(Genome&) called!" << std::endl;
		print_stacktrace(stderr);
		std::clog << "************************************************" << std::endl;
	}
#endif
	
	if (this != &p_original)
	{
		int source_mutation_count = p_original.mutation_count_;
		
		// first we need to ensure that we have sufficient capacity
		if (source_mutation_count > mutation_capacity_)
		{
			mutation_capacity_ = p_original.mutation_capacity_;		// just use the same capacity as the source
			mutations_ = (const Mutation **)realloc(mutations_, mutation_capacity_ * sizeof(Mutation*));
		}
		
		// then copy all pointers from the source to ourselves
		memcpy(mutations_, p_original.mutations_, source_mutation_count * sizeof(Mutation*));
		mutation_count_ = source_mutation_count;
	}
	
	return *this;
}

#ifdef DEBUG
bool Genome::LogGenomeCopyAndAssign(bool p_log)
{
	bool old_value = s_log_copy_and_assign_;
	
	s_log_copy_and_assign_ = p_log;
	
	return old_value;
}
#endif


































































