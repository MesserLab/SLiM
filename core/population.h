//
//  population.h
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
 
 The class Population represents the entire simulated population as a map of one or more subpopulations.  This class is where much
 of the simulation logic resides; the population is called to put events into effect, to evolve, and so forth.
 
 */

#ifndef __SLiM__population__
#define __SLiM__population__


#include <vector>
#include <map>
#include <string>

#include "subpopulation.h"
#include "substitution.h"
#include "chromosome.h"
#include "polymorphism.h"
#include "event.h"
#include "introduced_mutation.h"
#include "partial_sweep.h"


class Population : public std::map<int,Subpopulation>
{
public: 
	
	std::vector<Substitution> substitutions_;
	std::vector<std::string> parameters_;
	
	// add new empty subpopulation p_subpop_id of size p_subpop_size
	void AddSubpopulation(int p_subpop_id, unsigned int p_subpop_size);
	
	// add new subpopulation p_subpop_id of size p_subpop_size individuals drawn from source subpopulation p_source_subpop_id
	void AddSubpopulation(int p_subpop_id, int p_source_subpop_id, unsigned int p_subpop_size);
	
	// set size of subpopulation p_subpop_id to p_subpop_size
	void SetSize(int p_subpop_id, unsigned int p_subpop_size);
	
	// set fraction selfing_fraction of p_subpop_id that reproduces by selfing
	void SetSelfing(int p_subpop_id, double p_selfing_fraction);
	
	// set fraction p_migrant_fraction of p_subpop_id that originates as migrants from p_source_subpop_id per generation  
	void SetMigration(int p_subpop_id, int p_source_subpop_id, double p_migrant_fraction);
	
	// execute a given event in the population; the event is assumed to be due to trigger
	void ExecuteEvent(const Event &p_event, int p_generation, const Chromosome &p_chromosome, std::vector<int> &p_tracked_mutations);
	
	// introduce a user-defined mutation
	void IntroduceMutation(IntroducedMutation p_introduced_mutation, const Chromosome &p_chromosome);
	
	// output trajectories of followed mutations and set selection_coeff_ = 0 for partial sweeps 
	void TrackMutations(int p_generation, const std::vector<int> &p_tracked_mutations, std::vector<PartialSweep> &p_partial_sweeps, const Chromosome &p_chromosome);
	
	// generate children for subpopulation p_subpop_id, drawing from all source populations, handling crossover and mutation
	void EvolveSubpopulation(int p_subpop_id, const Chromosome &p_chromosome, int p_generation);
	
	// generate a child genome from parental genomes, with recombination, gene conversion, and mutation
	void CrossoverMutation(int p_subpop_id, int p_child_genome_index, int p_source_subpop_id, int p_parent1_genome_index, int p_parent2_genome_index, const Chromosome &p_chromosome, int p_generation);
	
	// step forward a generation: remove fixed mutations, then make the children become the parents and update fitnesses
	void SwapGenerations(int p_generation, const Chromosome &p_chromosome);
	
	// find mutations that are fixed in all child subpopulations and remove them
	void RemoveFixedMutations(int p_generation);
	
	// print all mutations and all genomes
	void PrintAll(const Chromosome &p_chromosome) const;
	
	// print all mutations and all genomes to a file
	void PrintAll(std::ofstream &p_outfile, const Chromosome &p_chromosome) const;
	
	// print sample of p_sample_size genomes from subpopulation p_subpop_id
	void PrintSample(int p_subpop_id, int p_sample_size, const Chromosome &p_chromosome) const;
	
	// print sample of p_sample_size genomes from subpopulation p_subpop_id, using "ms" format
	void PrintSample_ms(int p_subpop_id, int p_sample_size, const Chromosome &p_chromosome) const;
	
	// find p_mutation in p_polymorphisms and return its id
	int FindMutation(const std::multimap<int,Polymorphism> &p_polymorphisms, Mutation p_mutation) const;
	
	// if mutation p_mutation is present in p_polymorphisms increase its prevalence, otherwise add it
	void AddMutation(std::multimap<int,Polymorphism> &p_polymorphisms, Mutation p_mutation) const;
};


#endif /* defined(__SLiM__population__) */




































































