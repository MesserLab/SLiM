//
//  subpopulation.cpp
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


#include <iostream>
#include <assert.h>

#include "subpopulation.h"
#include "stacktrace.h"


bool Subpopulation::s_log_copy_and_assign_ = true;


Subpopulation::Subpopulation(const Subpopulation& p_original)
{
	if (s_log_copy_and_assign_)
	{
		std::clog << "********* Subpopulation::Subpopulation(Subpopulation&) called!" << std::endl;
		print_stacktrace(stderr);
		std::clog << "************************************************" << std::endl;
	}
	
	lookup_individual_ = p_original.lookup_individual_;   
	subpop_size_ = p_original.subpop_size_;
	selfing_fraction_ = p_original.selfing_fraction_;
	
	bool old_log = Genome::LogGenomeCopyAndAssign(false);
	parent_genomes_ = p_original.parent_genomes_;
	child_genomes_ = p_original.child_genomes_;
	Genome::LogGenomeCopyAndAssign(old_log);
	
	migrant_fractions_ = p_original.migrant_fractions_;
}

Subpopulation& Subpopulation::operator= (const Subpopulation& p_original)
{
	if (s_log_copy_and_assign_)
	{
		std::clog << "********* Subpopulation::operator=(Subpopulation&) called!" << std::endl;
		print_stacktrace(stderr);
		std::clog << "************************************************" << std::endl;
	}
	
	lookup_individual_ = p_original.lookup_individual_;   
	subpop_size_ = p_original.subpop_size_;
	selfing_fraction_ = p_original.selfing_fraction_;

	bool old_log = Genome::LogGenomeCopyAndAssign(false);
	parent_genomes_ = p_original.parent_genomes_;
	child_genomes_ = p_original.child_genomes_;
	Genome::LogGenomeCopyAndAssign(old_log);

	migrant_fractions_ = p_original.migrant_fractions_;
	
	return *this;
}

bool Subpopulation::LogSubpopulationCopyAndAssign(bool p_log)
{
	bool old_value = s_log_copy_and_assign_;
	
	s_log_copy_and_assign_ = p_log;
	
	return old_value;
}

Subpopulation::Subpopulation(int p_subpop_size)
{
	subpop_size_ = p_subpop_size;
	selfing_fraction_ = 0.0;
	
	bool old_log = Genome::LogGenomeCopyAndAssign(false);
	parent_genomes_.resize(2 * subpop_size_);
	child_genomes_.resize(2 * subpop_size_);
	Genome::LogGenomeCopyAndAssign(old_log);
	
	// Set up to draw random individuals, based initially on equal fitnesses
	double A[subpop_size_];
	
	for (int i = 0; i < subpop_size_; i++)
		A[i] = 1.0;
	
	lookup_individual_ = gsl_ran_discrete_preproc(subpop_size_, A);
}

int Subpopulation::DrawIndividual() const
{
	return static_cast<int>(gsl_ran_discrete(g_rng, lookup_individual_));
}

void Subpopulation::UpdateFitness(const Chromosome &p_chromosome)
{
	// calculate fitnesses in parent population and create new lookup table
	gsl_ran_discrete_free(lookup_individual_);
	
	double A[static_cast<int>(parent_genomes_.size() / 2)];
	
	for (int i = 0; i < static_cast<int>(parent_genomes_.size() / 2); i++)
		A[i] = FitnessOfIndividualWithGenomeIndices(2 * i, 2 * i + 1, p_chromosome);
	
	lookup_individual_ = gsl_ran_discrete_preproc(static_cast<int>(parent_genomes_.size() / 2), A);
}

double Subpopulation::FitnessOfIndividualWithGenomeIndices(int p_genome_index1, int p_genome_index2, const Chromosome &p_chromosome) const
{
	// calculate the fitness of the individual constituted by genome1 and genome2 in the parent population
	double w = 1.0;
	
	std::vector<Mutation>::const_iterator genome1_iter = parent_genomes_[p_genome_index1].begin();
	std::vector<Mutation>::const_iterator genome2_iter = parent_genomes_[p_genome_index2].begin();
	
	std::vector<Mutation>::const_iterator genome1_max = parent_genomes_[p_genome_index1].end();
	std::vector<Mutation>::const_iterator genome2_max = parent_genomes_[p_genome_index2].end();
	
	// we cache the last observed dominance coefficient, for speed; this avoids the find(), the float->double conversion, etc.
	double cached_dominance_coeff = 0.0;
	int cached_dominance_coeff_mutation_type = INT_MIN;		// flag value that the cache is not set up
	
	// first, handle the situation before either genome iterator has reached the end of its genome, for simplicity/speed
	if (genome1_iter != genome1_max && genome2_iter != genome2_max)
	{
		int genome1_iter_position = genome1_iter->position_, genome2_iter_position = genome2_iter->position_;
		
		do
		{
			if (genome1_iter_position < genome2_iter_position)
			{
				// Process a mutation in genome1 since it is leading
				float selection_coeff = genome1_iter->selection_coeff_;
				
				if (selection_coeff != 0.0f)
				{
					int mutation_type = genome1_iter->mutation_type_;
					
					if (mutation_type != cached_dominance_coeff_mutation_type)
					{
						cached_dominance_coeff = p_chromosome.mutation_types_.find(mutation_type)->second.dominance_coeff_;
						cached_dominance_coeff_mutation_type = mutation_type;
					}
					
					w *= (1.0 + cached_dominance_coeff * selection_coeff);
					
					if (w <= 0.0)
						return 0.0;
				}
				
				genome1_iter++;
				
				if (genome1_iter == genome1_max)
					break;
				else
					genome1_iter_position = genome1_iter->position_;
			}
			else if (genome1_iter_position > genome2_iter_position)
			{
				// Process a mutation in genome2 since it is leading
				float selection_coeff = genome2_iter->selection_coeff_;
				
				if (selection_coeff != 0.0f)
				{
					int mutation_type = genome2_iter->mutation_type_;
					
					if (mutation_type != cached_dominance_coeff_mutation_type)
					{
						cached_dominance_coeff = p_chromosome.mutation_types_.find(mutation_type)->second.dominance_coeff_;
						cached_dominance_coeff_mutation_type = mutation_type;
					}
					
					w *= (1.0 + cached_dominance_coeff * selection_coeff);
					
					if (w <= 0.0)
						return 0.0;
				}
				
				genome2_iter++;
				
				if (genome2_iter == genome2_max)
					break;
				else
					genome2_iter_position = genome2_iter->position_;
			}
			else
			{
				// Look for homozygosity: genome1_iter_position == genome2_iter_position
				int position = genome1_iter->position_; 
				std::vector<Mutation>::const_iterator genome1_start = genome1_iter;
				
				// advance through genome1 as long as we remain at the same position, handling one mutation at a time
				do
				{
					float selection_coeff = genome1_iter->selection_coeff_;
					
					if (selection_coeff != 0.0f)
					{
						int mutation_type = genome1_iter->mutation_type_;
						std::vector<Mutation>::const_iterator genome2_matchscan = genome2_iter; 
						bool homozygous = false;
						
						// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
						while (genome2_matchscan != genome2_max && genome2_matchscan->position_ == position)
						{
							if (mutation_type == genome2_matchscan->mutation_type_ && selection_coeff == genome2_matchscan->selection_coeff_) 
							{
								// a match was found, so we multiply our fitness by the full selection coefficient
								w *= (1.0 + selection_coeff);
								homozygous = true;
								break;
							}
							
							genome2_matchscan++;
						}
						
						// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
						if (!homozygous)
						{
							if (mutation_type != cached_dominance_coeff_mutation_type)
							{
								cached_dominance_coeff = p_chromosome.mutation_types_.find(mutation_type)->second.dominance_coeff_;
								cached_dominance_coeff_mutation_type = mutation_type;
							}
							
							w *= (1.0 + cached_dominance_coeff * selection_coeff);
							
							if (w <= 0.0)
								return 0.0;
						}
					}
					
					genome1_iter++;
				} while (genome1_iter != genome1_max && genome1_iter->position_ == position);
				
				// advance through genome2 as long as we remain at the same position, handling one mutation at a time
				do
				{
					float selection_coeff = genome2_iter->selection_coeff_;
					
					if (selection_coeff != 0.0f)
					{
						int mutation_type = genome2_iter->mutation_type_;
						std::vector<Mutation>::const_iterator genome1_matchscan = genome1_start; 
						bool homozygous = false;
						
						// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
						while (genome1_matchscan != genome1_max && genome1_matchscan->position_ == position)
						{
							if (mutation_type == genome1_matchscan->mutation_type_ && selection_coeff == genome1_matchscan->selection_coeff_)
							{
								// a match was found; we know this match was already found by the genome1 loop above, so our fitness has already been multiplied appropriately
								homozygous = true;
								break;
							}
							
							genome1_matchscan++;
						}
						
						// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
						if (!homozygous)
						{
							if (mutation_type != cached_dominance_coeff_mutation_type)
							{
								cached_dominance_coeff = p_chromosome.mutation_types_.find(mutation_type)->second.dominance_coeff_;
								cached_dominance_coeff_mutation_type = mutation_type;
							}
							
							w *= (1.0 + cached_dominance_coeff * selection_coeff);
							
							if (w <= 0.0)
								return 0.0;
						}
					}
					
					genome2_iter++;
				} while (genome2_iter != genome2_max && genome2_iter->position_ == position);
				
				// get things back in order for the top-level loop: break out if either genome has reached its end, otherwise get the position indices up to date
				if (genome1_iter == genome1_max || genome2_iter == genome2_max)
					break;
				
				genome1_iter_position = genome1_iter->position_;
				genome2_iter_position = genome2_iter->position_;
			}
		} while (true);
	}
	
	// one or the other genome has now reached its end, so now we just need to handle the remaining mutations in the unfinished genome
	assert(!(genome1_iter != genome1_max && genome2_iter != genome2_max));
	
	// if genome1 is unfinished, finish it
	while (genome1_iter != genome1_max)
	{
		float selection_coeff = genome1_iter->selection_coeff_;
		
		if (selection_coeff != 0.0f)
		{
			int mutation_type = genome1_iter->mutation_type_;
			
			if (mutation_type != cached_dominance_coeff_mutation_type)
			{
				cached_dominance_coeff = p_chromosome.mutation_types_.find(mutation_type)->second.dominance_coeff_;
				cached_dominance_coeff_mutation_type = mutation_type;
			}
			
			w *= (1.0 + cached_dominance_coeff * selection_coeff);
			
			if (w <= 0.0)
				return 0.0;
		}
		
		genome1_iter++;
	}
	
	// if genome2 is unfinished, finish it
	while (genome2_iter != genome2_max)
	{
		float selection_coeff = genome2_iter->selection_coeff_;
		
		if (selection_coeff != 0.0f)
		{
			int mutation_type = genome2_iter->mutation_type_;
			
			if (mutation_type != cached_dominance_coeff_mutation_type)
			{
				cached_dominance_coeff = p_chromosome.mutation_types_.find(mutation_type)->second.dominance_coeff_;
				cached_dominance_coeff_mutation_type = mutation_type;
			}
			
			w *= (1.0 + cached_dominance_coeff * selection_coeff);
			
			if (w <= 0.0)
				return 0.0;
		}
		
		genome2_iter++;
	}
	
	return w;
}

void Subpopulation::SwapChildAndParentGenomes()
{
	child_genomes_.swap(parent_genomes_);
	child_genomes_.resize(2 * subpop_size_);
}





































































