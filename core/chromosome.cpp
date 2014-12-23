//
//  chromosome.cpp
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


#include "chromosome.h"

#include <iostream>

#include "g_rng.h"


// initialize the random lookup tables used by Chromosome to draw mutation and recombination events
void Chromosome::InitializeDraws()
{
	if (size() == 0)
	{
		std::cerr << "ERROR (Initialize): empty chromosome" << std::endl;
		exit(1);
	}
	if (recombination_rates_.size() == 0)
	{
		std::cerr << "ERROR (Initialize): recombination rate not specified" << std::endl;
		exit(1);
	}
	if (!(overall_mutation_rate_ >= 0))
	{
		std::cerr << "ERROR (Initialize): invalid mutation rate" << std::endl;
		exit(1);
	}
	
	length_ = 0;
	
	for (int i = 0; i < size(); i++)
	{
		if (genomic_element_types_.count((*this)[i].genomic_element_type_) == 0) 
		{
			std::cerr << "ERROR (Initialize): genomic element type " << (*this)[i].genomic_element_type_ << " not defined" << std::endl;
			exit(1); 
		}
	}
	
	for (std::map<int,GenomicElementType>::const_iterator genomic_element_type_iter = genomic_element_types_.begin(); genomic_element_type_iter != genomic_element_types_.end(); genomic_element_type_iter++)
	{
		for (int j = 0; j < genomic_element_type_iter->second.mutation_types_.size(); j++)
		{
			if (mutation_types_.count(genomic_element_type_iter->second.mutation_types_[j]) == 0)
			{
				std::cerr << "ERROR (Initialize): mutation type " << genomic_element_type_iter->second.mutation_types_[j] << " not defined" << std::endl;
				exit(1); 
			}
		}
	}
	
	double A[size()];
	int l = 0;
	
	for (int i = 0; i < size(); i++) 
	{ 
		if ((*this)[i].end_position_ > length_)
			length_ = (*this)[i].end_position_;
		
		int l_i = (*this)[i].end_position_ - (*this)[i].start_position_ + 1;
		
		A[i] = static_cast<double>(l_i);
		l += l_i;
	}
	
	lookup_mutation = gsl_ran_discrete_preproc(size(), A);
	overall_mutation_rate_ = overall_mutation_rate_ * static_cast<double>(l);
	
	double B[recombination_rates_.size()];
	
	B[0] = recombination_rates_[0] * static_cast<double>(recombination_end_positions_[0]);
	overall_recombination_rate_ += B[0];
	
	for (int i = 1; i < recombination_rates_.size(); i++) 
	{ 
		B[i] = recombination_rates_[i] * static_cast<double>(recombination_end_positions_[i] - recombination_end_positions_[i - 1]);
		overall_recombination_rate_+= B[i];
		
		if (recombination_end_positions_[i] > length_)
			length_ = recombination_end_positions_[i];
	}
	
	lookup_recombination = gsl_ran_discrete_preproc(recombination_rates_.size(), B);
}

// draw the number of mutations that occur, based on the overall mutation rate
int Chromosome::DrawMutationCount() const
{
	return gsl_ran_poisson(g_rng, overall_mutation_rate_);
}

// draw a new mutation, based on the genomic element types present and their mutational proclivities
Mutation Chromosome::DrawNewMutation(int p_subpop_index, int p_generation) const
{
	int genomic_element = static_cast<int>(gsl_ran_discrete(g_rng, lookup_mutation));
	const GenomicElement &source_element = (*this)[genomic_element];
	
	GenomicElementType genomic_element_type = genomic_element_types_.find(source_element.genomic_element_type_)->second;
	
	int mutation_type_id = genomic_element_type.DrawMutationType();
	
	MutationType mutation_type = mutation_types_.find(mutation_type_id)->second;
	
	int position = source_element.start_position_ + static_cast<int>(gsl_rng_uniform_int(g_rng, source_element.end_position_ - source_element.start_position_ + 1));  
	
	double selection_coeff = mutation_type.DrawSelectionCoefficient();
	
	return Mutation(mutation_type_id, position, selection_coeff, p_subpop_index, p_generation);
}

// choose a set of recombination breakpoints, based on recombination intervals, overall recombination rate, and gene conversion probability
std::vector<int> Chromosome::DrawBreakpoints() const
{
	vector<int> breakpoints;
	
	// draw recombination breakpoints
	int num_breakpoints = gsl_ran_poisson(g_rng, overall_recombination_rate_);
	
	for (int i = 0; i < num_breakpoints; i++)
	{
		int breakpoint = 0;
		int recombination_interval = static_cast<int>(gsl_ran_discrete(g_rng, lookup_recombination));
		
		// choose a breakpoint anywhere in the chosen recombination interval with equal probability
		if (recombination_interval == 0)
			breakpoint = static_cast<int>(gsl_rng_uniform_int(g_rng, recombination_end_positions_[recombination_interval]));
		else
			breakpoint = recombination_end_positions_[recombination_interval - 1] + static_cast<int>(gsl_rng_uniform_int(g_rng, recombination_end_positions_[recombination_interval] - recombination_end_positions_[recombination_interval - 1]));
		
		breakpoints.push_back(breakpoint);
		
		// recombination can result in gene conversion, with probability gene_conversion_fraction_
		if (gsl_rng_uniform(g_rng) < gene_conversion_fraction_)
		{
			// for gene conversion, choose a second breakpoint that is relatively likely to be near to the first
			int breakpoint2 = breakpoint + gsl_ran_geometric(g_rng, 1.0 / gene_conversion_avg_length_);
			
			breakpoints.push_back(breakpoint2);
		}
	}
	
	return breakpoints;
}





































































