//
//  subpopulation.h
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

/*
 
 The class Subpopulation represents one simulated subpopulation, defined primarily by the genomes of the individuals it contains.
 Since one Genome object represents the mutations along one chromosome, and since SLiM presently simulates diploid individuals,
 each individual is represented by two genomes in the genome vector: individual i is represented by genomes 2*i and 2*i+1.
 A subpopulations also knows its size, its selfing fraction, and what fraction it receives as migrants from other subpopulations.
 
 */

#ifndef __SLiM__subpopulation__
#define __SLiM__subpopulation__


#include <vector>

#include "g_rng.h"
#include "genome.h"
#include "chromosome.h"


class Subpopulation
{
private:
	
	gsl_ran_discrete_t *lookup_individual;   
	
public:
	
	int    subpop_size_; // subpopulation size  
	double selfing_fraction_; // selfing fraction
	
	std::vector<Genome> parent_genomes_;
	std::vector<Genome> child_genomes_;
	
	std::map<int,double> migrant_fractions_; // m[i]: fraction made up of migrants from subpopulation i per generation
 
	
	Subpopulation(int p_subpop_size);
	
	int DrawIndividual() const;
	
	void UpdateFitness(const Chromosome &p_chromosome);
	
	double FitnessOfIndividualWithGenomeIndices(int p_genome_index1, int p_genome_index2, const Chromosome &p_chromosome) const;
	
	void SwapChildAndParentGenomes();
};


#endif /* defined(__SLiM__subpopulation__) */




































































