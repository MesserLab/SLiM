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
#include "stacktrace.h"

class SLiMSim;


class Population : public std::map<int,Subpopulation*>		// OWNED POINTERS
{
	//	This class has its copy constructor and assignment operator disabled, to prevent accidental copying.

public:
	
	Genome mutation_registry_;								// OWNED POINTERS: a registry of all mutations that have been added to this population
	std::vector<const Substitution*> substitutions_;		// OWNED POINTERS: Substitution objects for all fixed mutations
	
	Population(const Population&) = delete;					// no copying
	Population& operator=(const Population&) = delete;		// no copying
	Population(void) = default;								// default constructor
	~Population(void);										// destructor
	
	// find a subpopulation given an id
	inline Subpopulation &SubpopulationWithID(int p_subpop_id) const {
		auto found_pair = find(p_subpop_id);
		
		if (found_pair == this->end())
		{
			std::cerr << "********* SubpopulationWithID: no subpopulation p" << p_subpop_id << std::endl;
			print_stacktrace(stderr);
			std::cerr << "************************************************" << std::endl;
			exit(EXIT_FAILURE);
		}
		
		return *(found_pair->second);
	}
	
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
	void ExecuteEvent(const Event &p_event, int p_generation, const Chromosome &p_chromosome, const SLiMSim &p_sim, std::vector<MutationType*> *p_tracked_mutations);
	
	// introduce a user-defined mutation
	void IntroduceMutation(const IntroducedMutation &p_introduced_mutation);
	
	// output trajectories of followed mutations and set selection_coeff_ = 0 for partial sweeps 
	void TrackMutations(int p_generation, const std::vector<MutationType*> &p_tracked_mutations, std::vector<const PartialSweep*> *p_partial_sweeps);
	
	// generate children for subpopulation p_subpop_id, drawing from all source populations, handling crossover and mutation
	void EvolveSubpopulation(int p_subpop_id, const Chromosome &p_chromosome, int p_generation);
	
	// generate a child genome from parental genomes, with recombination, gene conversion, and mutation
	void CrossoverMutation(Subpopulation *subpop, Subpopulation *source_subpop, int p_child_genome_index, int p_source_subpop_id, int p_parent1_genome_index, int p_parent2_genome_index, const Chromosome &p_chromosome, int p_generation);
	
	// step forward a generation: remove fixed mutations, then make the children become the parents and update fitnesses
	void SwapGenerations(int p_generation);
	
	// count the references to each Mutation in the registry, and remove mutations that are no longer referenced; returns true if any fixed mutations exist
	void ManageMutationReferencesAndRemoveFixedMutations(int p_generation);
	
	// print all mutations and all genomes
	void PrintAll() const;
	
	// print all mutations and all genomes to a file
	void PrintAll(std::ofstream &p_outfile) const;
	
	// print sample of p_sample_size genomes from subpopulation p_subpop_id
	void PrintSample(int p_subpop_id, int p_sample_size) const;
	
	// print sample of p_sample_size genomes from subpopulation p_subpop_id, using "ms" format
	void PrintSample_ms(int p_subpop_id, int p_sample_size, const Chromosome &p_chromosome) const;
};


#endif /* defined(__SLiM__population__) */




































































