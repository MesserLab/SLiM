//
//  population.cpp
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


#include "population.h"

#include <fstream>
#include <iomanip>

#include "slim_sim.h"


using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::multimap;


// add new empty subpopulation p_subpop_id of size p_subpop_size
void Population::AddSubpopulation(int p_subpop_id, unsigned int p_subpop_size) 
{ 
	if (count(p_subpop_id) != 0)		{ cerr << "ERROR (AddSubpopulation): subpopulation p" << p_subpop_id << " already exists" << endl; exit(EXIT_FAILURE); }
	if (p_subpop_size < 1)				{ cerr << "ERROR (AddSubpopulation): subpopulation p" << p_subpop_id << " empty" << endl; exit(EXIT_FAILURE); }
	
	Subpopulation *new_subpop = new Subpopulation(p_subpop_size);
	
	insert(std::pair<const int,Subpopulation*>(p_subpop_id, new_subpop));
}

// add new subpopulation p_subpop_id of size p_subpop_size individuals drawn from source subpopulation p_source_subpop_id
void Population::AddSubpopulation(int p_subpop_id, int p_source_subpop_id, unsigned int p_subpop_size) 
{
	if (count(p_subpop_id) != 0)		{ cerr << "ERROR (AddSubpopulation): subpopulation p" << p_subpop_id << " already exists" << endl; exit(EXIT_FAILURE); }
	if (count(p_source_subpop_id) == 0)	{ cerr << "ERROR (AddSubpopulation): source subpopulation p" << p_source_subpop_id << " does not exist" << endl; exit(EXIT_FAILURE); }
	if (p_subpop_size < 1)				{ cerr << "ERROR (AddSubpopulation): subpopulation p" << p_subpop_id << " empty" << endl; exit(EXIT_FAILURE); }
	
	Subpopulation *new_subpop = new Subpopulation(p_subpop_size);
	
	insert(std::pair<const int,Subpopulation*>(p_subpop_id, new_subpop));
	
	Subpopulation &subpop = *(find(p_subpop_id)->second);
	Subpopulation &source_subpop = *(find(p_source_subpop_id)->second);
	
	for (int parent_index = 0; parent_index < subpop.subpop_size_; parent_index++)
	{
		// draw individual from source_subpop and assign to be a parent in subpop
		int migrant_index = source_subpop.DrawIndividual();
		
		subpop.parent_genomes_[2 * parent_index] = source_subpop.parent_genomes_[2 * migrant_index];
		subpop.parent_genomes_[2 * parent_index + 1] = source_subpop.parent_genomes_[2 * migrant_index + 1];
	}
}

// set size of subpopulation p_subpop_id to p_subpop_size
void Population::SetSize(int p_subpop_id, unsigned int p_subpop_size)
{
	if (count(p_subpop_id) == 0)		{ cerr << "ERROR (change size): no subpopulation p" << p_subpop_id << endl; exit(EXIT_FAILURE); }
	
	if (p_subpop_size == 0) // remove subpopulation p_subpop_id
	{
		erase(p_subpop_id);
		
		for (std::pair<const int,Subpopulation*> &subpop_pair : *this)
			subpop_pair.second->migrant_fractions_.erase(p_subpop_id);
	}
	else
	{
		find(p_subpop_id)->second->subpop_size_ = p_subpop_size;
		find(p_subpop_id)->second->child_genomes_.resize(2 * p_subpop_size);
	}
}

// set fraction selfing_fraction of p_subpop_id that reproduces by selfing
void Population::SetSelfing(int p_subpop_id, double p_selfing_fraction) 
{ 
	if (count(p_subpop_id) == 0)								{ cerr << "ERROR (SetSelfing): no subpopulation p" << p_subpop_id << endl; exit(EXIT_FAILURE); }
	if (p_selfing_fraction < 0.0 || p_selfing_fraction > 1.0)	{ cerr << "ERROR (SetSelfing): selfing fraction has to be within [0,1]" << endl; exit(EXIT_FAILURE); }
	
	find(p_subpop_id)->second->selfing_fraction_ = p_selfing_fraction; 
}

// set fraction p_migrant_fraction of p_subpop_id that originates as migrants from p_source_subpop_id per generation  
void Population::SetMigration(int p_subpop_id, int p_source_subpop_id, double p_migrant_fraction) 
{ 
	if (count(p_subpop_id) == 0)								{ cerr << "ERROR (SetMigration): no subpopulation p" << p_subpop_id << endl; exit(EXIT_FAILURE); }
	if (count(p_source_subpop_id) == 0)							{ cerr << "ERROR (SetMigration): no subpopulation p" << p_source_subpop_id << endl; exit(EXIT_FAILURE); }
	if (p_migrant_fraction < 0.0 || p_migrant_fraction > 1.0)	{ cerr << "ERROR (SetMigration): migration fraction has to be within [0,1]" << endl; exit(EXIT_FAILURE); }
	
	if (find(p_subpop_id)->second->migrant_fractions_.count(p_source_subpop_id) != 0)
		find(p_subpop_id)->second->migrant_fractions_.erase(p_source_subpop_id);
	
	find(p_subpop_id)->second->migrant_fractions_.insert(std::pair<const int,double>(p_source_subpop_id, p_migrant_fraction)); 
}

// execute a given event in the population; the event is assumed to be due to trigger
void Population::ExecuteEvent(const Event &p_event, int p_generation, const Chromosome &p_chromosome, const SLiMSim &sim, std::vector<int> *p_tracked_mutations)
{
	char event_type = p_event.event_type_;
	const std::vector<std::string> &event_parameters = p_event.parameters_;
	int num_event_parameters = static_cast<int>(event_parameters.size());
	
	switch (event_type)
	{
		case 'P':	// add subpopulation
		{
			string sub = event_parameters[0];
			sub.erase(0, 1); // p
			int subpop_id = atoi(sub.c_str());
			
			int subpop_size = static_cast<int>(atof(event_parameters[1].c_str()));
			
			if (num_event_parameters == 2)		// empty subpopulation
			{ 
				AddSubpopulation(subpop_id, subpop_size);
			}
			else if (num_event_parameters == 3)	// drawn from source population
			{
				string sub2 = event_parameters[2];
				sub2.erase(0, 1); // p
				int source_subpop_id = atoi(sub2.c_str());
				
				AddSubpopulation(subpop_id, source_subpop_id, subpop_size);
			}
			
			break;
		}
		
		case 'N':	// set subpopulation size
		{ 
			string sub = event_parameters[0];
			sub.erase(0, 1); // p
			int subpop_id = atoi(sub.c_str());
			
			int subpop_size = static_cast<int>(atof(event_parameters[1].c_str()));
			
			SetSize(subpop_id, subpop_size);
			
			break;
		}
		
		case 'S':	// set selfing rate
		{ 
			string sub = event_parameters[0];
			sub.erase(0, 1); // p
			int subpop_id = atoi(sub.c_str());
			
			double selfing_fraction = atof(event_parameters[1].c_str());
			
			SetSelfing(subpop_id, selfing_fraction);
			
			break;
		}
		
		case 'M':	// change migration rate
		{
			string sub1 = event_parameters[0];
			sub1.erase(0, 1); // p
			int subpop_id = atoi(sub1.c_str());
			
			string sub2 = event_parameters[1];
			sub2.erase(0, 1); // p
			int source_subpop_id = atoi(sub2.c_str());
			
			double migrant_fraction = atof(event_parameters[2].c_str());
			
			SetMigration(subpop_id, source_subpop_id, migrant_fraction);
			
			break;
		}
		
		case 'A':	// output state of entire population
		{
			if (num_event_parameters == 0)
			{
				cout << "#OUT: " << p_generation << " A" << endl;
				PrintAll();
			}
			else if (num_event_parameters == 1)
			{
				std::ofstream outfile;
				outfile.open(event_parameters[0].c_str());
				
				if (outfile.is_open())
				{
					const std::vector<std::string> &parameters = sim.InputParameters();
					
					for (int i = 0; i < parameters.size(); i++)
						outfile << parameters[i] << endl;
					
					outfile << "#OUT: " << p_generation << " A " << event_parameters[0].c_str() << endl;
					PrintAll(outfile);
					outfile.close(); 
				}
				else
				{
					cerr << "ERROR (output): could not open "<< event_parameters[0].c_str() << endl;
					exit(EXIT_FAILURE);
				}
			}
			
			break;
		}
		
		case 'R':	// output random subpopulation sample
		{
			string sub = event_parameters[0];
			sub.erase(0, 1); // p
			int subpop_id = atoi(sub.c_str());
			
			int sample_size = atoi(event_parameters[1].c_str());   
			
			cout << "#OUT: " << p_generation << " R p" << subpop_id << " " << sample_size << endl;
			
			if (num_event_parameters == 3 && event_parameters[2] == "MS")
				PrintSample_ms(subpop_id, sample_size, p_chromosome);
			else
				PrintSample(subpop_id, sample_size);
			
			break;
		}
		
		case 'F':	// output list of fixed mutations
		{
			cout << "#OUT: " << p_generation << " F " << endl;
			cout << "Mutations:" << endl;
			
			for (int i = 0; i < substitutions_.size(); i++)
			{
				cout << i + 1;
				substitutions_[i].print();
			}
			
			break;
		}
		
		case 'T':	// track mutation-types
		{
			string sub = event_parameters[0];
			sub.erase(0, 1); // m
			p_tracked_mutations->push_back(atoi(sub.c_str()));
			
			break;
		}
	}
}

// introduce a user-defined mutation
void Population::IntroduceMutation(const IntroducedMutation &p_introduced_mutation)
{
	if (count(p_introduced_mutation.subpop_index_) == 0)
	{
		cerr << "ERROR (predetermined mutation): subpopulation "<< p_introduced_mutation.subpop_index_ << " does not exist" << endl; exit(EXIT_FAILURE);
	}
	if (find(p_introduced_mutation.subpop_index_)->second->child_genomes_.size() / 2 < p_introduced_mutation.num_homozygotes_ + p_introduced_mutation.num_heterozygotes_) 
	{ 
		cerr << "ERROR (predetermined mutation): not enough individuals in subpopulation "<< p_introduced_mutation.subpop_index_ << endl; exit(EXIT_FAILURE); 
	}
	
	Mutation new_mutation;
	
	new_mutation.position_ = p_introduced_mutation.position_;
	new_mutation.mutation_type_ptr_ = p_introduced_mutation.mutation_type_ptr_;
	new_mutation.selection_coeff_ = static_cast<float>(p_introduced_mutation.mutation_type_ptr_->DrawSelectionCoefficient());
	new_mutation.subpop_index_ = p_introduced_mutation.subpop_index_;
	new_mutation.generation_ = p_introduced_mutation.generation_;
	
	// introduce homozygotes
	for (int j = 0; j < p_introduced_mutation.num_homozygotes_; j++) 
	{ 
		Genome *g1 = &find(p_introduced_mutation.subpop_index_)->second->child_genomes_[2 * j];
		Genome *g2 = &find(p_introduced_mutation.subpop_index_)->second->child_genomes_[2 * j + 1];
		(*g1).push_back(new_mutation);
		(*g2).push_back(new_mutation);
		
		// FIXME can this be done just once, outside the loop?
		sort((*g1).begin(), (*g1).end());
		sort((*g2).begin(), (*g2).end());
		(*g1).erase(unique((*g1).begin(), (*g1).end()), (*g1).end());
		(*g2).erase(unique((*g2).begin(), (*g2).end()), (*g2).end());
	}
	
	// introduce heterozygotes
	for (int j = p_introduced_mutation.num_homozygotes_; j < p_introduced_mutation.num_homozygotes_ + p_introduced_mutation.num_heterozygotes_; j++) 
	{ 
		Genome *g1 = &find(p_introduced_mutation.subpop_index_)->second->child_genomes_[2 * j];
		(*g1).push_back(new_mutation);
		
		// FIXME can this be done just once, outside the loop?
		sort((*g1).begin(), (*g1).end());
		(*g1).erase(unique((*g1).begin(), (*g1).end()), (*g1).end());
	}
}

// output trajectories of followed mutations and set selection_coeff_ = 0 for partial sweeps 
void Population::TrackMutations(int p_generation, const std::vector<int> &p_tracked_mutations, std::vector<PartialSweep> *p_partial_sweeps)
{
	// FIXME this whole loop could be enclosed in if(p_tracked_mutations.size() > 0) couldn't it?
	// find all polymorphism of the types that are to be tracked
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)
	{
		multimap<const int,Polymorphism> polymorphisms;
		
		for (int i = 0; i < 2 * subpop_pair.second->subpop_size_; i++)				// go through all children
			for (int k = 0; k < subpop_pair.second->child_genomes_[i].size(); k++)	// go through all mutations
				for (int j = 0; j < p_tracked_mutations.size(); j++)
					if (subpop_pair.second->child_genomes_[i][k].mutation_type_ptr_->mutation_type_id_ == p_tracked_mutations[j])
						AddMutation(&polymorphisms, subpop_pair.second->child_genomes_[i][k]);
		
		// output the frequencies of these mutations in each subpopulation
		for (const std::pair<const int,Polymorphism> &polymorphism_pair : polymorphisms) 
		{ 
			cout << "#OUT: " << p_generation << " T p" << subpop_pair.first << " ";
			polymorphism_pair.second.print_no_id(polymorphism_pair.first); 
		}
	}
	
	// check partial sweeps
	if (p_partial_sweeps->size() > 0)
	{
		multimap<const int,Polymorphism> polymorphisms;
		int current_pop_size = 0;
		
		for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)
			current_pop_size += subpop_pair.second->subpop_size_;
		
		// find all polymorphism that are supposed to undergo partial sweeps
		for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)
			for (int i = 0; i < 2 * subpop_pair.second->subpop_size_; i++)				// go through all children
				for (int k = 0; k < subpop_pair.second->child_genomes_[i].size(); k++)	// go through all mutations
					for (int j = 0; j < p_partial_sweeps->size(); j++)
						if (subpop_pair.second->child_genomes_[i][k].position_ == (*p_partial_sweeps)[j].position_ && subpop_pair.second->child_genomes_[i][k].mutation_type_ptr_ == (*p_partial_sweeps)[j].mutation_type_ptr_) 
							AddMutation(&polymorphisms, subpop_pair.second->child_genomes_[i][k]); 
		
		// check whether a partial sweep has reached its target frequency
		for (const std::pair<const int,Polymorphism> &polymorphism_pair : polymorphisms) 
		{ 
			for (int j = 0; j < p_partial_sweeps->size(); j++)
			{
				if (polymorphism_pair.first == (*p_partial_sweeps)[j].position_ && polymorphism_pair.second.mutation_type_ptr_ == (*p_partial_sweeps)[j].mutation_type_ptr_)
				{
					if (static_cast<double>(polymorphism_pair.second.prevalence_) / (2 * current_pop_size) >= (*p_partial_sweeps)[j].target_prevalence_)
					{
						// sweep has reached target frequency, set all selection_coeff_ to zero
						for (std::pair<const int,Subpopulation*> &subpop_pair : *this)
							for (int i = 0; i < 2 * subpop_pair.second->subpop_size_; i++)				// go through all children
								for (int k = 0; k < subpop_pair.second->child_genomes_[i].size(); k++)	// go through all mutations
									if (subpop_pair.second->child_genomes_[i][k].position_ == (*p_partial_sweeps)[j].position_ && subpop_pair.second->child_genomes_[i][k].mutation_type_ptr_ == (*p_partial_sweeps)[j].mutation_type_ptr_)
										subpop_pair.second->child_genomes_[i][k].selection_coeff_ = 0.0;
						
						p_partial_sweeps->erase(p_partial_sweeps->begin() + j);
						j--;	// check the partial sweep that just backfilled in
					}
				}
			}
		}
	}
}

// generate children for subpopulation p_subpop_id, drawing from all source populations, handling crossover and mutation
void Population::EvolveSubpopulation(int p_subpop_id, const Chromosome &p_chromosome, int p_generation)
{
	Subpopulation &subpop = *(find(p_subpop_id)->second);
	int child_genome1, child_genome2, parent1, parent2;
	
	// create map of shuffled children ids
	int child_map[subpop.subpop_size_];  
	
	for (int j = 0; j < subpop.subpop_size_; j++)
		child_map[j] = j;
	
	gsl_ran_shuffle(g_rng, child_map, subpop.subpop_size_, sizeof(int));
	
	int child_count = 0; // counter over all subpop_size_ children (will get mapped to child_map[child_count])
	
	// draw number of migrant individuals
	double migration_rates[subpop.migrant_fractions_.size() + 1];
	unsigned int num_migrants[subpop.migrant_fractions_.size() + 1];
	double migration_rate_sum = 0.0;
	int pop_count = 0;
	
	for (const std::pair<const int,double> &fractions_pair : subpop.migrant_fractions_)
	{
		migration_rates[pop_count] = fractions_pair.second;
		migration_rate_sum += fractions_pair.second;
		pop_count++;
	}
	
	if (migration_rate_sum <= 1.0)
	{
		// the remaining fraction is within-subpopulation mating
		migration_rates[pop_count] = 1.0 - migration_rate_sum;
	}
	else
	{
		cerr << "ERROR (evolve subpopulation): too many migrants in subpopulation " << p_subpop_id << endl;
		exit(EXIT_FAILURE);
	}
	
	gsl_ran_multinomial(g_rng, subpop.migrant_fractions_.size() + 1, subpop.subpop_size_, migration_rates, num_migrants);
	
	// loop over all migration source populations
	pop_count = 0;
	
	for (const std::pair<const int,double> &fractions_pair : subpop.migrant_fractions_)
	{
		int source_subpop_id = fractions_pair.first;
		Subpopulation &source_subpop = *(find(source_subpop_id)->second);
		double selfing_fraction = source_subpop.selfing_fraction_;
		int migrant_count = 0;
		
		while (migrant_count < num_migrants[pop_count])
		{
			child_genome1 = 2 * child_map[child_count];
			child_genome2 = child_genome1 + 1;
			
			// draw parents in source population
			parent1 = static_cast<int>(gsl_rng_uniform_int(g_rng, source_subpop.parent_genomes_.size() / 2));
			
			if (selfing_fraction == 1.0)
				parent2 = parent1;	// self
			else if (selfing_fraction == 0.0 || gsl_rng_uniform(g_rng) >= selfing_fraction)
				parent2 = static_cast<int>(gsl_rng_uniform_int(g_rng, source_subpop.parent_genomes_.size() / 2));
			else
				parent2 = parent1;	// self
			
			// recombination, gene-conversion, mutation
			CrossoverMutation(&subpop, &source_subpop, child_genome1, source_subpop_id, 2 * parent1, 2 * parent1 + 1, p_chromosome, p_generation);
			CrossoverMutation(&subpop, &source_subpop, child_genome2, source_subpop_id, 2 * parent2, 2 * parent2 + 1, p_chromosome, p_generation);
			
			migrant_count++;
			child_count++;
		}
		
		pop_count++;
	}
	
	// the remainder of the children are generated by within-population matings
	while (child_count < subpop.subpop_size_) 
	{
		child_genome1 = 2 * child_map[child_count];
		child_genome2 = child_genome1 + 1;
		double selfing_fraction = subpop.selfing_fraction_;
		
		// draw parents from this subpopulation
		parent1 = subpop.DrawIndividual();
		
		if (selfing_fraction == 1.0)
			parent2 = parent1;	// self
		else if (selfing_fraction == 0.0 || gsl_rng_uniform(g_rng) >= selfing_fraction)
			parent2 = subpop.DrawIndividual();
		else
			parent2 = parent1;	// self
		
		// recombination, gene-conversion, mutation
		CrossoverMutation(&subpop, &subpop, child_genome1, p_subpop_id, 2 * parent1, 2 * parent1 + 1, p_chromosome, p_generation);
		CrossoverMutation(&subpop, &subpop, child_genome2, p_subpop_id, 2 * parent2, 2 * parent2 + 1, p_chromosome, p_generation);
		
		child_count++;
	}
}

// generate a child genome from parental genomes, with recombination, gene conversion, and mutation
void Population::CrossoverMutation(Subpopulation *subpop, Subpopulation *source_subpop, int p_child_genome_index, int p_source_subpop_id, int p_parent1_genome_index, int p_parent2_genome_index, const Chromosome &p_chromosome, int p_generation)
{
	// child genome p_child_genome_index in subpopulation p_subpop_id is assigned outcome of cross-overs at breakpoints in all_breakpoints
	// between parent genomes p_parent1_genome_index and p_parent2_genome_index from subpopulation p_source_subpop_id and new mutations added
	// 
	// example: all_breakpoints = (r1, r2)
	// 
	// mutations (      x < r1) assigned from p1
	// mutations (r1 <= x < r2) assigned from p2
	// mutations (r2 <= x     ) assigned from p1
	
	// swap parent1_genome_index and parent2_genome_index in half of cases, to assure random assortment
	if (gsl_rng_uniform_int(g_rng, 2) == 0)
	{
		int swap = p_parent1_genome_index;
		p_parent1_genome_index = p_parent2_genome_index;
		p_parent2_genome_index = swap;
	}
	
	// start with a clean slate in the child genome
	Genome &child_genome = subpop->child_genomes_[p_child_genome_index];
	child_genome.clear();
	
	// mutations are usually rare, so let's streamline the case where none occur
	int num_mutations = p_chromosome.DrawMutationCount();
	
	if (num_mutations == 0)
	{
		// create vector with uniqued recombination breakpoints
		std::vector<int> all_breakpoints = p_chromosome.DrawBreakpoints(); 
		all_breakpoints.push_back(p_chromosome.length_ + 1);
		sort(all_breakpoints.begin(), all_breakpoints.end());
		all_breakpoints.erase(unique(all_breakpoints.begin(), all_breakpoints.end()), all_breakpoints.end());
		
		// do the crossover
		std::vector<Mutation>::const_iterator parent1_iter		= source_subpop->parent_genomes_[p_parent1_genome_index].begin();
		std::vector<Mutation>::const_iterator parent2_iter		= source_subpop->parent_genomes_[p_parent2_genome_index].begin();
		
		std::vector<Mutation>::const_iterator parent1_iter_max	= source_subpop->parent_genomes_[p_parent1_genome_index].end();
		std::vector<Mutation>::const_iterator parent2_iter_max	= source_subpop->parent_genomes_[p_parent2_genome_index].end();
		
		std::vector<Mutation>::const_iterator parent_iter			= parent1_iter;
		std::vector<Mutation>::const_iterator parent_iter_max		= parent1_iter_max;
		
		int break_index_max = static_cast<int>(all_breakpoints.size());
		
		for (int break_index = 0; break_index != break_index_max; break_index++)
		{
			int breakpoint = all_breakpoints[break_index];
			
			// while there are still old mutations in the parent before the current breakpoint...
			while (parent_iter != parent_iter_max && parent_iter->position_ < breakpoint)
			{
				// add the old mutation; no need to check for a duplicate here since the parental genome is already duplicate-free
				child_genome.push_back(*parent_iter);
				
				parent_iter++;
			}
			
			// we have reached the breakpoint, so swap parents
			parent1_iter = parent2_iter;	parent1_iter_max = parent2_iter_max;
			parent2_iter = parent_iter;		parent2_iter_max = parent_iter_max;
			parent_iter = parent1_iter;		parent_iter_max = parent1_iter_max; 
			
			// skip over anything in the new parent that occurs prior to the breakpoint; it was not the active strand
			while (parent_iter != parent_iter_max && parent_iter->position_ < breakpoint)
				parent_iter++;
		}
	}
	else
	{
		// create vector with the mutations to be added
		std::vector<Mutation> mutations_to_add;
		
		for (int k = 0; k < num_mutations; k++)
			mutations_to_add.push_back(p_chromosome.DrawNewMutation(p_source_subpop_id, p_generation));
		
		sort(mutations_to_add.begin(), mutations_to_add.end());
		
		// create vector with uniqued recombination breakpoints
		std::vector<int> all_breakpoints = p_chromosome.DrawBreakpoints(); 
		all_breakpoints.push_back(p_chromosome.length_ + 1);
		sort(all_breakpoints.begin(), all_breakpoints.end());
		all_breakpoints.erase(unique(all_breakpoints.begin(), all_breakpoints.end()), all_breakpoints.end());
		
		// do the crossover
		std::vector<Mutation>::const_iterator parent1_iter		= source_subpop->parent_genomes_[p_parent1_genome_index].begin();
		std::vector<Mutation>::const_iterator parent2_iter		= source_subpop->parent_genomes_[p_parent2_genome_index].begin();
		
		std::vector<Mutation>::const_iterator parent1_iter_max	= source_subpop->parent_genomes_[p_parent1_genome_index].end();
		std::vector<Mutation>::const_iterator parent2_iter_max	= source_subpop->parent_genomes_[p_parent2_genome_index].end();
		
		std::vector<Mutation>::const_iterator mutation_iter		= mutations_to_add.begin();
		std::vector<Mutation>::const_iterator mutation_iter_max	= mutations_to_add.end();
		
		std::vector<Mutation>::const_iterator parent_iter			= parent1_iter;
		std::vector<Mutation>::const_iterator parent_iter_max		= parent1_iter_max;
		
		int break_index_max = static_cast<int>(all_breakpoints.size());
		int num_mutations_added = 0;
		bool present;
		
		for (int break_index = 0; break_index != break_index_max; break_index++)
		{
			int breakpoint = all_breakpoints[break_index];
			
			// while there are still old mutations in the parent, or new mutations to be added, before the current breakpoint...
			while ((parent_iter != parent_iter_max && parent_iter->position_ < breakpoint) || (mutation_iter != mutation_iter_max && mutation_iter->position_ < breakpoint))
			{
				// while an old mutation in the parent is before the breakpoint and before the next new mutation...
				while (parent_iter != parent_iter_max && parent_iter->position_ < breakpoint && (mutation_iter == mutation_iter_max || parent_iter->position_ <= mutation_iter->position_))
				{
					present = false;
					
					// search back through the mutations already added to see if the one we intend to add is already present
					if (num_mutations_added != 0 && child_genome.back().position_ == parent_iter->position_)
						for (int k = num_mutations_added - 1; k >= 0; k--)
							if (child_genome[k] == *parent_iter)
							{
								present = true;
								break;
							}
					
					// if the mutation was not present, add it
					if (!present)
					{
						child_genome.push_back(*parent_iter);
						num_mutations_added++;
					}
					
					parent_iter++;
				}
				
				// while a new mutation is before the breakpoint and before the next old mutation in the parent...
				while (mutation_iter != mutation_iter_max && mutation_iter->position_ < breakpoint && (parent_iter == parent_iter_max || mutation_iter->position_ <= parent_iter->position_))
				{
					present = false;
					
					// search back through the mutations already added to see if the one we intend to add is already present
					if (num_mutations_added != 0 && child_genome.back().position_ == mutation_iter->position_)
						for (int k = num_mutations_added - 1; k >= 0; k--)
							if (child_genome[k] == *mutation_iter)
							{
								present = true;
								break;
							}
					
					// if the mutation was not present, add it
					if (!present)
					{
						child_genome.push_back(*mutation_iter);
						num_mutations_added++;
					}
					
					mutation_iter++;
				}
			}
			
			// we have reached the breakpoint, so swap parents
			parent1_iter = parent2_iter;	parent1_iter_max = parent2_iter_max;
			parent2_iter = parent_iter;		parent2_iter_max = parent_iter_max;
			parent_iter = parent1_iter;		parent_iter_max = parent1_iter_max; 
			
			// skip over anything in the new parent that occurs prior to the breakpoint; it was not the active strand
			while (parent_iter != parent_iter_max && parent_iter->position_ < breakpoint)
				parent_iter++;
		}
	}
}

// step forward a generation: remove fixed mutations, then make the children become the parents and update fitnesses
void Population::SwapGenerations(int p_generation)
{
	// find and remove fixed mutations from the children in all subpopulations
	RemoveFixedMutations(p_generation);
	
	// make children the new parents and update fitnesses
	for (std::pair<const int,Subpopulation*> &subpop_pair : *this)
	{ 
		subpop_pair.second->SwapChildAndParentGenomes();
		subpop_pair.second->UpdateFitness(); 
	}
}

// find mutations that are fixed in all child subpopulations and remove them
void Population::RemoveFixedMutations(int p_generation)
{
	bool old_log = Genome::LogGenomeCopyAndAssign(false);
	
	// start with a genome that contains all of the mutations in one genome of one individual; any fixed mutations must be present in that genome
	Genome fixed_mutation_accumulator = begin()->second->child_genomes_[0];
	
	// loop through all genomes and intersect them with fixed_mutation_accumulator to drop unfixed mutations
	// FIXME terminating as soon as fixed_mutation_accumulator is empty might be a good optimization here...
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)			// subpopulations
		for (int i = 0; i < 2 * subpop_pair.second->subpop_size_; i++)		// child genomes
			fixed_mutation_accumulator = GenomeWithFixedMutations(subpop_pair.second->child_genomes_[i], fixed_mutation_accumulator);
	
	// then remove fixed mutations from all genomes, and make new Substitution objects for them
	if (fixed_mutation_accumulator.size() > 0)
	{
		for (std::pair<const int,Subpopulation*> &subpop_pair : *this)		// subpopulations
			for (int i = 0; i < 2 * subpop_pair.second->subpop_size_; i++)	// child genomes
				subpop_pair.second->child_genomes_[i] = GenomeWithPolymorphicMutations(subpop_pair.second->child_genomes_[i], fixed_mutation_accumulator);
		
		for (int i = 0; i < fixed_mutation_accumulator.size(); i++)
			substitutions_.push_back(Substitution(fixed_mutation_accumulator[i], p_generation));
	}
	
	Genome::LogGenomeCopyAndAssign(old_log);
}

// print all mutations and all genomes
// FIXME can this be merged with the function below?
void Population::PrintAll() const
{
	cout << "Populations:" << endl;
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)
		cout << "p" << subpop_pair.first << " " << subpop_pair.second->subpop_size_ << endl;
	
	multimap<const int,Polymorphism> polymorphisms;
	
	// add all polymorphisms
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)						// go through all subpopulations
		for (int i = 0; i < 2 * subpop_pair.second->subpop_size_; i++)					// go through all children
			for (int k = 0; k < subpop_pair.second->child_genomes_[i].size(); k++)		// go through all mutations
				AddMutation(&polymorphisms, subpop_pair.second->child_genomes_[i][k]);
	
	// print all polymorphisms
	cout << "Mutations:"  << endl;
	
	for (const std::pair<const int,Polymorphism> &polymorphism_pair : polymorphisms)
		polymorphism_pair.second.print(polymorphism_pair.first);
	
	// print all genomes
	cout << "Genomes:" << endl;
	
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)						// go through all subpopulations
	{
		for (int i = 0; i < 2 * subpop_pair.second->subpop_size_; i++)					// go through all children
		{
			cout << "p" << subpop_pair.first << ":" << i + 1;
			
			for (int k = 0; k < subpop_pair.second->child_genomes_[i].size(); k++)		// go through all mutations
			{
				int id = FindMutation(polymorphisms, subpop_pair.second->child_genomes_[i][k]);
				cout << " " << id; 
			}
			
			cout << endl;
		}
	}
}

// print all mutations and all genomes to a file
void Population::PrintAll(std::ofstream &p_outfile) const
{
	p_outfile << "Populations:" << endl;
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)
		p_outfile << "p" << subpop_pair.first << " " << subpop_pair.second->subpop_size_ << endl;
	
	multimap<const int,Polymorphism> polymorphisms;
	
	// add all polymorphisms
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)					// go through all subpopulations
		for (int i = 0; i < 2 * subpop_pair.second->subpop_size_; i++)				// go through all children
			for (int k = 0; k < subpop_pair.second->child_genomes_[i].size(); k++)	// go through all mutations
				AddMutation(&polymorphisms, subpop_pair.second->child_genomes_[i][k]);
	
	// print all polymorphisms
	p_outfile << "Mutations:"  << endl;
	
	for (const std::pair<const int,Polymorphism> &polymorphism_pair : polymorphisms) 
		polymorphism_pair.second.print(p_outfile, polymorphism_pair.first);
	
	// print all genomes
	p_outfile << "Genomes:" << endl;
	
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)					// go through all subpopulations
	{
		for (int i = 0; i < 2 * subpop_pair.second->subpop_size_; i++)				// go through all children
		{
			p_outfile << "p" << subpop_pair.first << ":" << i + 1;
			
			for (int k = 0; k < subpop_pair.second->child_genomes_[i].size(); k++)	// go through all mutations
			{
				int id = FindMutation(polymorphisms, subpop_pair.second->child_genomes_[i][k]);
				p_outfile << " " << id; 
			}
			
			p_outfile << endl;
		}
	}
}

// print sample of p_sample_size genomes from subpopulation p_subpop_id
// FIXME whole lotta find(p_subpop_id) going on here; the result of that will be constant across this function, no?
void Population::PrintSample(int p_subpop_id, int p_sample_size) const
{
	if (count(p_subpop_id) == 0)
	{
		cerr << "ERROR (output): subpopulation p" << p_subpop_id << " does not exists" << endl;
		exit(EXIT_FAILURE);
	}
	
	// assemble the sample and get the polymorphisms within it
	std::vector<int> sample; 
	multimap<const int,Polymorphism> polymorphisms;
	
	for (int s = 0; s < p_sample_size; s++)
	{
		int j = static_cast<int>(gsl_rng_uniform_int(g_rng, find(p_subpop_id)->second->child_genomes_.size()));
		
		sample.push_back(j);
		
		for (int k = 0; k < find(p_subpop_id)->second->child_genomes_[j].size(); k++)			// go through all mutations
			AddMutation(&polymorphisms, find(p_subpop_id)->second->child_genomes_[j][k]);
	}
	
	// print the sample's polymorphisms
	cout << "Mutations:"  << endl;
	
	for (const std::pair<const int,Polymorphism> &polymorphism_pair : polymorphisms) 
		polymorphism_pair.second.print(polymorphism_pair.first);
	
	// print the sample's genomes
	cout << "Genomes:" << endl;
	
	for (int j = 0; j < sample.size(); j++)														// go through all children
	{
		cout << "p" << find(p_subpop_id)->first << ":" << sample[j] + 1;
		
		for (int k = 0; k < find(p_subpop_id)->second->child_genomes_[sample[j]].size(); k++)	// go through all mutations
		{
			int mutation_id = FindMutation(polymorphisms, find(p_subpop_id)->second->child_genomes_[sample[j]][k]);
			
			cout << " " << mutation_id;
		}
		
		cout << endl;
	}
}

// print sample of p_sample_size genomes from subpopulation p_subpop_id, using "ms" format
void Population::PrintSample_ms(int p_subpop_id, int p_sample_size, const Chromosome &p_chromosome) const
{
	if (count(p_subpop_id) == 0)
	{
		cerr << "ERROR (output): subpopulation p" << p_subpop_id << " does not exists" << endl;
		exit(EXIT_FAILURE);
	}
	
	std::vector<int> sample; 
	multimap<const int,Polymorphism> polymorphisms;
	
	for (int s = 0; s < p_sample_size; s++)
	{
		int j = static_cast<int>(gsl_rng_uniform_int(g_rng, find(p_subpop_id)->second->child_genomes_.size()));
		
		sample.push_back(j);
		
		for (int k = 0; k < find(p_subpop_id)->second->child_genomes_[j].size(); k++)			// go through all mutations
			AddMutation(&polymorphisms, find(p_subpop_id)->second->child_genomes_[j][k]);
	}
	
	// print header
	cout << endl << "//" << endl << "segsites: " << polymorphisms.size() << endl;
	
	// print the sample's positions
	if (polymorphisms.size() > 0)
	{
		cout << "positions:";
		
		for (const std::pair<const int,Polymorphism> &polymorphism_pair : polymorphisms) 
			cout << " " << std::fixed << std::setprecision(7) << static_cast<double>(polymorphism_pair.first + 1) / (p_chromosome.length_ + 1); 
		
		cout << endl;
	}
	
	// print the sample's genotypes
	for (int j = 0; j < sample.size(); j++)														// go through all children
	{
		string genotype(polymorphisms.size(), '0'); // fill with 0s
		
		for (int k = 0; k < find(p_subpop_id)->second->child_genomes_[sample[j]].size(); k++)	// go through all mutations
		{
			Mutation mutation = find(p_subpop_id)->second->child_genomes_[sample[j]][k];
			int position = 0;
			
			for (const std::pair<const int,Polymorphism> &polymorphism_pair : polymorphisms) 
			{
				if (polymorphism_pair.first == mutation.position_ && polymorphism_pair.second.mutation_type_ptr_ == mutation.mutation_type_ptr_ && polymorphism_pair.second.selection_coeff_ == mutation.selection_coeff_)
				{
					// mark this polymorphism as present in the genome, and move on since this mutation can't also match any other polymorphism
					genotype.replace(position, 1, "1");
					break;
				}
				
				position++;
			}
		}
		
		cout << genotype << endl;
	}
}

// find p_mutation in p_polymorphisms and return its id
int Population::FindMutation(const multimap<const int,Polymorphism> &p_polymorphisms, Mutation p_mutation) const
{
	// iterate through all mutations with same position
	std::pair<multimap<const int,Polymorphism>::const_iterator,multimap<const int,Polymorphism>::const_iterator> range = p_polymorphisms.equal_range(p_mutation.position_);
	multimap<const int,Polymorphism>::const_iterator polymorphisms_iter;
	
	for (polymorphisms_iter = range.first; polymorphisms_iter != range.second; polymorphisms_iter++)
		if (polymorphisms_iter->second.mutation_type_ptr_ == p_mutation.mutation_type_ptr_ && polymorphisms_iter->second.selection_coeff_ == p_mutation.selection_coeff_) 
			return polymorphisms_iter->second.mutation_id_;
	
	return 0;
}

// if mutation p_mutation is present in p_polymorphisms increase its prevalence, otherwise add it
void Population::AddMutation(multimap<const int,Polymorphism> *p_polymorphisms, Mutation p_mutation) const
{
	// iterate through all mutations with same position
	std::pair<multimap<const int,Polymorphism>::iterator,multimap<const int,Polymorphism>::iterator> range = p_polymorphisms->equal_range(p_mutation.position_);
	multimap<const int,Polymorphism>::iterator polymorphisms_iter;
	
	for (polymorphisms_iter = range.first; polymorphisms_iter != range.second; polymorphisms_iter++)
		if (polymorphisms_iter->second.mutation_type_ptr_ == p_mutation.mutation_type_ptr_ && polymorphisms_iter->second.selection_coeff_ == p_mutation.selection_coeff_) 
		{ 
			polymorphisms_iter->second.prevalence_++;
			return;
		}
	
	// the mutation was not found, so add it to p_polymorphisms
	int mutation_id = static_cast<int>(p_polymorphisms->size()) + 1;
	Polymorphism new_polymorphism = Polymorphism(mutation_id, p_mutation.mutation_type_ptr_, p_mutation.selection_coeff_, p_mutation.subpop_index_, p_mutation.generation_, 1);
	
	p_polymorphisms->insert(std::pair<const int,Polymorphism>(p_mutation.position_, new_polymorphism));
}





































































