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


Population::~Population(void)
{
	//std::cerr << "Population::~Population" << std::endl;
	
	for (auto subpopulation : *this)
		delete subpopulation.second;
	
	for (auto substitution : substitutions_)
		delete substitution;
	
	// The malloced storage of mutation_registry_ will be freed when it is destroyed, but it
	// does not know that the Mutation pointers inside it are owned, so we need to free them.
	const Mutation **registry_iter = mutation_registry_.begin_pointer();
	const Mutation **registry_iter_end = mutation_registry_.end_pointer();
	
	for (; registry_iter != registry_iter_end; ++registry_iter)
		delete *registry_iter;
}

// add new empty subpopulation p_subpop_id of size p_subpop_size
void Population::AddSubpopulation(int p_subpop_id, unsigned int p_subpop_size, double p_initial_sex_ratio, const SLiMSim &p_sim) 
{ 
	if (count(p_subpop_id) != 0)		{ cerr << "ERROR (AddSubpopulation): subpopulation p" << p_subpop_id << " already exists" << endl; exit(EXIT_FAILURE); }
	if (p_subpop_size < 1)				{ cerr << "ERROR (AddSubpopulation): subpopulation p" << p_subpop_id << " empty" << endl; exit(EXIT_FAILURE); }
	
	// make and add the new subpopulation
	Subpopulation *new_subpop = nullptr;
	
	if (p_sim.SexEnabled())
		new_subpop = new Subpopulation(p_subpop_size, p_initial_sex_ratio, p_sim.ModeledChromosomeType(), p_sim.XDominanceCoefficient());	// SEX ONLY
	else
		new_subpop = new Subpopulation(p_subpop_size);
	
	insert(std::pair<const int,Subpopulation*>(p_subpop_id, new_subpop));
}

// add new subpopulation p_subpop_id of size p_subpop_size individuals drawn from source subpopulation p_source_subpop_id
void Population::AddSubpopulation(int p_subpop_id, int p_source_subpop_id, unsigned int p_subpop_size, double p_initial_sex_ratio, const SLiMSim &p_sim) 
{
	if (count(p_subpop_id) != 0)		{ cerr << "ERROR (AddSubpopulation): subpopulation p" << p_subpop_id << " already exists" << endl; exit(EXIT_FAILURE); }
	if (p_subpop_size < 1)				{ cerr << "ERROR (AddSubpopulation): subpopulation p" << p_subpop_id << " empty" << endl; exit(EXIT_FAILURE); }
	
	// make and add the new subpopulation
	Subpopulation *new_subpop = nullptr;
 
	if (p_sim.SexEnabled())
		new_subpop = new Subpopulation(p_subpop_size, p_initial_sex_ratio, p_sim.ModeledChromosomeType(), p_sim.XDominanceCoefficient());	// SEX ONLY
	else
		new_subpop = new Subpopulation(p_subpop_size);
	
	insert(std::pair<const int,Subpopulation*>(p_subpop_id, new_subpop));
	
	// then draw parents from the source population according to fitness, obeying the new subpop's sex ratio
	Subpopulation &subpop = SubpopulationWithID(p_subpop_id);
	Subpopulation &source_subpop = SubpopulationWithID(p_source_subpop_id);
	
	for (int parent_index = 0; parent_index < subpop.parent_subpop_size_; parent_index++)
	{
		// draw individual from source_subpop and assign to be a parent in subpop
		int migrant_index;
		
		if (p_sim.SexEnabled())
		{
			if (parent_index < subpop.parent_first_male_index_)
				migrant_index = source_subpop.DrawFemaleParentUsingFitness();
			else
				migrant_index = source_subpop.DrawMaleParentUsingFitness();
		}
		else
		{
			migrant_index = source_subpop.DrawParentUsingFitness();
		}
		
		subpop.parent_genomes_[2 * parent_index].copy_from_genome(source_subpop.parent_genomes_[2 * migrant_index]);
		subpop.parent_genomes_[2 * parent_index + 1].copy_from_genome(source_subpop.parent_genomes_[2 * migrant_index + 1]);
	}
	
	// UpdateFitness() is not called here - all fitnesses are kept as equal.  This is because the parents were drawn from the source subpopulation according
	// to their fitness already; fitness has already been applied.  If UpdateFitness() were called, fitness would be double-applied in this generation.
}

// set size of subpopulation p_subpop_id to p_subpop_size
void Population::SetSize(int p_subpop_id, unsigned int p_subpop_size)
{
	// SetSize() can only be called when the child generation has not yet been generated.  It sets the size on the child generation,
	// and then that size takes effect when the children are generated from the parents in EvolveSubpopulation().
	if (child_generation_valid)
	{
		cerr << "ERROR (SetSize): called when the child generation was valid" << endl;
		exit(EXIT_FAILURE);
	}
	
	if (count(p_subpop_id) == 0)		{ cerr << "ERROR (SetSize): no subpopulation p" << p_subpop_id << endl; exit(EXIT_FAILURE); }
	
	if (p_subpop_size == 0) // remove subpopulation p_subpop_id
	{
		erase(p_subpop_id);
		
		for (std::pair<const int,Subpopulation*> &subpop_pair : *this)
			subpop_pair.second->migrant_fractions_.erase(p_subpop_id);
	}
	else
	{
		Subpopulation &subpop = SubpopulationWithID(p_subpop_id);
		
		// After we change the subpop size, we need to generate new children genomes to fit the new requirements
		subpop.child_subpop_size_ = p_subpop_size;
		subpop.GenerateChildrenToFit(false);	// false means generate only new children, not new parents
	}
}

// set sex ratio of subpopulation p_subpop_id to p_sex_ratio
void Population::SetSexRatio(int p_subpop_id, double p_sex_ratio)
{
	// SetSexRatio() can only be called when the child generation has not yet been generated.  It sets the sex ratio on the child generation,
	// and then that sex ratio takes effect when the children are generated from the parents in EvolveSubpopulation().
	if (child_generation_valid)
	{
		cerr << "ERROR (SetSexRatio): called when the child generation was valid" << endl;
		exit(EXIT_FAILURE);
	}
	
	if (count(p_subpop_id) == 0)		{ cerr << "ERROR (SetSexRatio): no subpopulation p" << p_subpop_id << endl; exit(EXIT_FAILURE); }
	
	Subpopulation &subpop = SubpopulationWithID(p_subpop_id);
	
	// After we change the subpop sex ratio, we need to generate new children genomes to fit the new requirements
	subpop.child_sex_ratio_ = p_sex_ratio;
	subpop.GenerateChildrenToFit(false);	// false means generate only new children, not new parents
}

// set fraction selfing_fraction of p_subpop_id that reproduces by selfing
void Population::SetSelfing(int p_subpop_id, double p_selfing_fraction) 
{ 
	if (p_selfing_fraction < 0.0 || p_selfing_fraction > 1.0)	{ cerr << "ERROR (SetSelfing): selfing fraction has to be within [0,1]" << endl; exit(EXIT_FAILURE); }
	
	SubpopulationWithID(p_subpop_id).selfing_fraction_ = p_selfing_fraction; 
}

// set fraction p_migrant_fraction of p_subpop_id that originates as migrants from p_source_subpop_id per generation  
void Population::SetMigration(int p_subpop_id, int p_source_subpop_id, double p_migrant_fraction) 
{ 
	if (count(p_source_subpop_id) == 0)							{ cerr << "ERROR (SetMigration): no subpopulation p" << p_source_subpop_id << endl; exit(EXIT_FAILURE); }
	if (p_migrant_fraction < 0.0 || p_migrant_fraction > 1.0)	{ cerr << "ERROR (SetMigration): migration fraction has to be within [0,1]" << endl; exit(EXIT_FAILURE); }
	
	Subpopulation &subpop = SubpopulationWithID(p_subpop_id);
	
	if (subpop.migrant_fractions_.count(p_source_subpop_id) != 0)
		subpop.migrant_fractions_.erase(p_source_subpop_id);
	
	subpop.migrant_fractions_.insert(std::pair<const int,double>(p_source_subpop_id, p_migrant_fraction)); 
}

// execute a given event in the population; the event is assumed to be due to trigger
void Population::ExecuteEvent(const Event &p_event, int p_generation, const Chromosome &p_chromosome, const SLiMSim &p_sim, std::vector<MutationType*> *p_tracked_mutations)
{
	char event_type = p_event.event_type_;
	const std::vector<std::string> &event_parameters = p_event.parameters_;
	int num_event_parameters = static_cast<int>(event_parameters.size());
	
	switch (event_type)
	{
		#pragma mark ExecuteEvent: P - Add subpopulation
		case 'P':	// add subpopulation
		{
			string sub = event_parameters[0];
			sub.erase(0, 1); // p
			int subpop_id = atoi(sub.c_str());
			
			int subpop_size = static_cast<int>(atof(event_parameters[1].c_str()));
			
			bool source_subpop_given = false;
			int source_subpop_id = 0;
			
			bool sex_ratio_given = false;
			double sex_ratio = 0.5;			// the default whenever sex is enabled and a ratio is not given for a new pop
			
			// parse optional parameters; we can be pretty loose with this because they were already checked for correctness
			for (int param_index = 2; param_index < num_event_parameters; ++param_index)
			{
				string subx = event_parameters[param_index];
				
				if (subx.at(0) == 'p')
				{
					source_subpop_given = true;
					subx.erase(0, 1); // p
					source_subpop_id = atoi(subx.c_str());
				}
				else
				{
					// SEX ONLY
					sex_ratio_given = true;
					sex_ratio = atof(subx.c_str());
				}
			}
			
			// construct the subpop; we always pass the sex ratio, but AddSubpopulation will not use it if sex is not enabled, for simplicity
			if (!source_subpop_given)
				AddSubpopulation(subpop_id, subpop_size, sex_ratio, p_sim);						// empty subpopulation
			else
				AddSubpopulation(subpop_id, source_subpop_id, subpop_size, sex_ratio, p_sim);	// drawn from source population
			
			break;
		}
		
		#pragma mark ExecuteEvent: N - Set subpopulation size
		case 'N':	// set subpopulation size
		{ 
			string sub = event_parameters[0];
			sub.erase(0, 1); // p
			int subpop_id = atoi(sub.c_str());
			
			int subpop_size = static_cast<int>(atof(event_parameters[1].c_str()));
			
			SetSize(subpop_id, subpop_size);
			
			break;
		}
		
		#pragma mark ExecuteEvent: S - Set selfing rate
		case 'S':	// set selfing rate
		{ 
			string sub = event_parameters[0];
			sub.erase(0, 1); // p
			int subpop_id = atoi(sub.c_str());
			
			double selfing_fraction = atof(event_parameters[1].c_str());
			
			SetSelfing(subpop_id, selfing_fraction);
			
			break;
		}
		
		#pragma mark ExecuteEvent: X - Set subpopulation sex ratio
		case 'X':	// set sex ratio
		{ 
			string sub = event_parameters[0];
			sub.erase(0, 1); // p
			int subpop_id = atoi(sub.c_str());
			
			double sex_ratio = atof(event_parameters[1].c_str());
			
			SetSexRatio(subpop_id, sex_ratio);
			
			break;
		}
		
		#pragma mark ExecuteEvent: M - Change migration rate
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
		
		#pragma mark ExecuteEvent: A - Output all
		case 'A':	// output state of entire population
		{
			if (num_event_parameters == 0)
			{
				cout << "#OUT: " << p_generation << " A" << endl;
				PrintAll(std::cout);
			}
			else if (num_event_parameters == 1)
			{
				std::ofstream outfile;
				outfile.open(event_parameters[0].c_str());
				
				if (outfile.is_open())
				{
					const std::vector<std::string> &input_parameters = p_sim.InputParameters();
					
					for (int i = 0; i < input_parameters.size(); i++)
						outfile << input_parameters[i] << endl;
					
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
		
		#pragma mark ExecuteEvent: R - Output sample
		case 'R':	// output random subpopulation sample
		{
			string sub = event_parameters[0];
			sub.erase(0, 1); // p
			int subpop_id = atoi(sub.c_str());
			
			int sample_size = atoi(event_parameters[1].c_str());
			
			IndividualSex requested_sex = IndividualSex::kUnspecified;
			
			if (num_event_parameters >= 3)
			{
				if (event_parameters[2] == "M")
					requested_sex = IndividualSex::kMale;
				else if (event_parameters[2] == "F")
					requested_sex = IndividualSex::kFemale;
				else if (event_parameters[2] == "*")
					requested_sex = IndividualSex::kUnspecified;
			}
			
			cout << "#OUT: " << p_generation << " R p" << subpop_id << " " << sample_size;
			
			if (p_sim.SexEnabled())
				cout << " " << requested_sex;
			
			cout << endl;
			
			if (num_event_parameters >= 3 && event_parameters[num_event_parameters - 1] == "MS")
				PrintSample_ms(subpop_id, sample_size, p_chromosome, requested_sex);
			else
				PrintSample(subpop_id, sample_size, requested_sex);
			
			break;
		}
		
		#pragma mark ExecuteEvent: F - Output fixed mutations
		case 'F':	// output list of fixed mutations
		{
			cout << "#OUT: " << p_generation << " F " << endl;
			cout << "Mutations:" << endl;
			
			for (int i = 0; i < substitutions_.size(); i++)
			{
				cout << i + 1;
				substitutions_[i]->print(std::cout);
			}
			
			break;
		}
		
		#pragma mark ExecuteEvent: T - Track mutations
		case 'T':	// track mutation-types
		{
			string sub = event_parameters[0];
			sub.erase(0, 1); // m
			int mutation_type_id = atoi(sub.c_str());
			const std::map<int,MutationType*> &mutation_types = p_sim.MutationTypes();
			auto found_muttype_pair = mutation_types.find(mutation_type_id);
			
			if (found_muttype_pair == mutation_types.end())
			{
				std::cerr << "ERROR (ExecuteEvent): mutation type m" << mutation_type_id << " not defined" << endl;
				exit(EXIT_FAILURE);
			}
			
			MutationType *mutation_type_ptr = found_muttype_pair->second;
			
			p_tracked_mutations->push_back(mutation_type_ptr);
			
			break;
		}
	}
}

// introduce a user-defined mutation
void Population::IntroduceMutation(const IntroducedMutation &p_introduced_mutation)
{
	Subpopulation &introduction_subpop = SubpopulationWithID(p_introduced_mutation.subpop_index_);
	
	if (introduction_subpop.child_subpop_size_ < p_introduced_mutation.num_homozygotes_ + p_introduced_mutation.num_heterozygotes_) 
	{ 
		cerr << "ERROR (predetermined mutation): not enough individuals in subpopulation "<< p_introduced_mutation.subpop_index_ << endl;
		exit(EXIT_FAILURE); 
	}
	
	const Mutation *new_mutation = new Mutation(p_introduced_mutation.mutation_type_ptr_, p_introduced_mutation.position_, p_introduced_mutation.mutation_type_ptr_->DrawSelectionCoefficient(), p_introduced_mutation.subpop_index_, p_introduced_mutation.generation_);
	mutation_registry_.push_back(new_mutation);
	
	// BCH 27 Dec. 2014: create map of shuffled children ids, so that we introduce the mutation into random individuals
	// Note this and which_genome, below, represent a change in behavior; introduced mutations used to deliberately coincide in the same individual/strand
	int child_map[introduction_subpop.child_subpop_size_];  
	
	for (int j = 0; j < introduction_subpop.child_subpop_size_; j++)
		child_map[j] = j;
	
	gsl_ran_shuffle(g_rng, child_map, introduction_subpop.child_subpop_size_, sizeof(int));
	
	// introduce homozygotes
	for (int j = 0; j < p_introduced_mutation.num_homozygotes_; j++) 
	{
		int child_index = child_map[j];
		Genome *g1 = &introduction_subpop.child_genomes_[2 * child_index];
		Genome *g2 = &introduction_subpop.child_genomes_[2 * child_index + 1];
		(*g1).insert_sorted_mutation_if_unique(new_mutation);
		(*g2).insert_sorted_mutation_if_unique(new_mutation);
	}
	
	// introduce heterozygotes
	for (int j = p_introduced_mutation.num_homozygotes_; j < p_introduced_mutation.num_homozygotes_ + p_introduced_mutation.num_heterozygotes_; j++) 
	{
		int child_index = child_map[j];
		bool which_genome = g_rng_bool(g_rng);	// BCH 27 Dec. 2014: choose which genome at random, so if two mutations are introduced in the same generation they don't end up linked
		Genome *g1 = &introduction_subpop.child_genomes_[2 * child_index + which_genome];
		(*g1).insert_sorted_mutation_if_unique(new_mutation);
	}
}

// output trajectories of followed mutations and set selection_coeff_ = 0 for partial sweeps 
void Population::TrackMutations(int p_generation, const std::vector<MutationType*> &p_tracked_mutations, std::vector<const PartialSweep*> *p_partial_sweeps)
{
	// find all polymorphism of the types that are to be tracked
	int tracked_mutation_count = static_cast<int>(p_tracked_mutations.size());
	
	if (tracked_mutation_count > 0)
	{
		for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)
		{
			multimap<const int,Polymorphism> polymorphisms;
			
			for (int i = 0; i < 2 * subpop_pair.second->child_subpop_size_; i++)				// go through all children
				for (int k = 0; k < subpop_pair.second->child_genomes_[i].size(); k++)	// go through all mutations
					for (int j = 0; j < tracked_mutation_count; j++)
						if (subpop_pair.second->child_genomes_[i][k]->mutation_type_ptr_ == p_tracked_mutations[j])
							AddMutationToPolymorphismMap(&polymorphisms, *subpop_pair.second->child_genomes_[i][k]);
			
			// output the frequencies of these mutations in each subpopulation
			for (const std::pair<const int,Polymorphism> &polymorphism_pair : polymorphisms) 
			{ 
				cout << "#OUT: " << p_generation << " T p" << subpop_pair.first << " ";
				polymorphism_pair.second.print(cout, polymorphism_pair.first, false /* no id */);
			}
		}
	}
	
	// check partial sweeps
	int partial_sweeps_count = static_cast<int>(p_partial_sweeps->size());
	
	if (partial_sweeps_count > 0)
	{
		multimap<const int,Polymorphism> polymorphisms;
		int current_pop_size = 0;
		
		for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)
			current_pop_size += subpop_pair.second->child_subpop_size_;
		
		// find all polymorphism that are supposed to undergo partial sweeps
		for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)
			for (int i = 0; i < 2 * subpop_pair.second->child_subpop_size_; i++)				// go through all children
				for (int k = 0; k < subpop_pair.second->child_genomes_[i].size(); k++)	// go through all mutations
					for (int j = 0; j < partial_sweeps_count; j++)
						if (subpop_pair.second->child_genomes_[i][k]->position_ == (*p_partial_sweeps)[j]->position_ && subpop_pair.second->child_genomes_[i][k]->mutation_type_ptr_ == (*p_partial_sweeps)[j]->mutation_type_ptr_) 
							AddMutationToPolymorphismMap(&polymorphisms, *subpop_pair.second->child_genomes_[i][k]); 
		
		// check whether a partial sweep has reached its target frequency
		for (const std::pair<const int,Polymorphism> &polymorphism_pair : polymorphisms) 
		{
			for (int j = 0; j < partial_sweeps_count; j++)
			{
				if (polymorphism_pair.first == (*p_partial_sweeps)[j]->position_ && polymorphism_pair.second.mutation_type_ptr_ == (*p_partial_sweeps)[j]->mutation_type_ptr_)
				{
					if (static_cast<double>(polymorphism_pair.second.prevalence_) / (2 * current_pop_size) >= (*p_partial_sweeps)[j]->target_prevalence_)
					{
						// sweep has reached target frequency, set all selection_coeff_ to zero
						Mutation *neutral_mutation = nullptr;
						
						for (std::pair<const int,Subpopulation*> &subpop_pair : *this)
							for (int i = 0; i < 2 * subpop_pair.second->child_subpop_size_; i++)				// go through all children
								for (int k = 0; k < subpop_pair.second->child_genomes_[i].size(); k++)	// go through all mutations
									if (subpop_pair.second->child_genomes_[i][k]->position_ == (*p_partial_sweeps)[j]->position_ && subpop_pair.second->child_genomes_[i][k]->mutation_type_ptr_ == (*p_partial_sweeps)[j]->mutation_type_ptr_)
									{
										// Since Mutation is immutable by design, we replace with a new neutral Mutation object
										if (!neutral_mutation)
										{
											const Mutation &basemut = *subpop_pair.second->child_genomes_[i][k];
											neutral_mutation = new Mutation(basemut.mutation_type_ptr_, basemut.position_, 0.0, basemut.subpop_index_, basemut.generation_);
											mutation_registry_.push_back(neutral_mutation);		// the old mutation will be dropped from the registry at the end of the generation
										}
										
										subpop_pair.second->child_genomes_[i][k] = neutral_mutation;
									}
						
						p_partial_sweeps->erase(p_partial_sweeps->begin() + j);
						
						// check the partial sweep that just backfilled in
						partial_sweeps_count--;
						j--;
					}
				}
			}
		}
	}
}

// generate children for subpopulation p_subpop_id, drawing from all source populations, handling crossover and mutation
void Population::EvolveSubpopulation(int p_subpop_id, const Chromosome &p_chromosome, int p_generation)
{
	Subpopulation &subpop = SubpopulationWithID(p_subpop_id);
	int child_genome1, child_genome2, parent1, parent2;
	bool sex_enabled = subpop.sex_enabled_;
	double sex_ratio = 0.0;
	int total_children = subpop.child_subpop_size_, total_male_children = 0, total_female_children = 0;
	
	// SEX ONLY
	if (sex_enabled)
	{
		sex_ratio = subpop.child_sex_ratio_;
		total_male_children = static_cast<int>(lround(total_children * sex_ratio));
		total_female_children = total_children - total_male_children;
		
		if (total_male_children <= 0 || total_female_children <= 0)
		{
			cerr << "ERROR (EvolveSubpopulation): sex ratio " << sex_ratio << " results in a unisexual child population" << endl;
			exit(EXIT_FAILURE);
		}
	}
	
	// BCH 27 Dec. 2014: Note that child_map has been removed here, so the order of generated children is NOT RANDOM!
	// Any code that chooses individuals from the population should choose randomly to avoid order-dependency!
	int child_count = 0; // counter over all subpop_size_ children
	int male_child_count = 0, female_child_count = 0;
	
	// draw number of migrant individuals
	int migrant_source_count = static_cast<int>(subpop.migrant_fractions_.size());
	double migration_rates[migrant_source_count + 1];
	unsigned int num_migrants[migrant_source_count + 1];
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
		cerr << "ERROR (EvolveSubpopulation): too many migrants in subpopulation " << p_subpop_id << endl;
		exit(EXIT_FAILURE);
	}
	
	gsl_ran_multinomial(g_rng, migrant_source_count + 1, total_children, migration_rates, num_migrants);
	
	// loop over all migration source populations and generate their offspring
	// FIXME why do we draw parents with equal probability here? is that a bug, or intentional? emailed Philipp 1/1/2015 to ask...
	pop_count = 0;
	
	for (const std::pair<const int,double> &fractions_pair : subpop.migrant_fractions_)
	{
		int source_subpop_id = fractions_pair.first;
		Subpopulation &source_subpop = SubpopulationWithID(source_subpop_id);
		double selfing_fraction = source_subpop.selfing_fraction_;
		int migrant_count = 0, migrants_to_generate = num_migrants[pop_count];
		int number_to_self = static_cast<int>(lround(migrants_to_generate * selfing_fraction));
		
		if (sex_enabled)
		{
			// SEX ONLY
			int male_migrants = static_cast<int>(lround(migrants_to_generate * sex_ratio));
			int female_migrants = migrants_to_generate - male_migrants;
			
			if (male_migrants < 0 || female_migrants < 0)
			{
				cerr << "ERROR (EvolveSubpopulation): negative number of migrants of one sex" << endl;
				exit(EXIT_FAILURE);
			}
			if (female_migrants < number_to_self)
			{
				cerr << "ERROR (EvolveSubpopulation): insufficient female migrants " << female_migrants << " to satisfy selfing demand " << number_to_self << endl;
				exit(EXIT_FAILURE);
			}
			
			// generate females first
			while (migrant_count < female_migrants)
			{
				child_genome1 = 2 * female_child_count;
				child_genome2 = child_genome1 + 1;
				
				// draw parents in source population
				parent1 = source_subpop.DrawFemaleParentUsingFitness();
				
				if (number_to_self > 0)
					parent2 = parent1, --number_to_self;	// self
				else
					parent2 = source_subpop.DrawMaleParentUsingFitness();
				
				// recombination, gene-conversion, mutation
				CrossoverMutation(&subpop, &source_subpop, child_genome1, source_subpop_id, 2 * parent1, 2 * parent1 + 1, p_chromosome, p_generation, IndividualSex::kFemale);
				CrossoverMutation(&subpop, &source_subpop, child_genome2, source_subpop_id, 2 * parent2, 2 * parent2 + 1, p_chromosome, p_generation, IndividualSex::kFemale);
				
				migrant_count++;
				female_child_count++;
			}
			
			// then generate males
			while (migrant_count < female_migrants + male_migrants)
			{
				child_genome1 = 2 * (male_child_count + subpop.child_first_male_index_);
				child_genome2 = child_genome1 + 1;
				
				// draw parents in source population
				parent1 = source_subpop.DrawFemaleParentUsingFitness();
				parent2 = source_subpop.DrawMaleParentUsingFitness();
				
				// recombination, gene-conversion, mutation
				CrossoverMutation(&subpop, &source_subpop, child_genome1, source_subpop_id, 2 * parent1, 2 * parent1 + 1, p_chromosome, p_generation, IndividualSex::kMale);
				CrossoverMutation(&subpop, &source_subpop, child_genome2, source_subpop_id, 2 * parent2, 2 * parent2 + 1, p_chromosome, p_generation, IndividualSex::kMale);
				
				migrant_count++;
				male_child_count++;
			}
		}
		else
		{
			while (migrant_count < migrants_to_generate)
			{
				child_genome1 = 2 * child_count;
				child_genome2 = child_genome1 + 1;
				
				// draw parents in source population
				parent1 = source_subpop.DrawParentUsingFitness();
				
				if (number_to_self > 0)
					parent2 = parent1, --number_to_self;	// self
				else
					parent2 = source_subpop.DrawParentUsingFitness();
				
				// recombination, gene-conversion, mutation
				CrossoverMutation(&subpop, &source_subpop, child_genome1, source_subpop_id, 2 * parent1, 2 * parent1 + 1, p_chromosome, p_generation, IndividualSex::kHermaphrodite);
				CrossoverMutation(&subpop, &source_subpop, child_genome2, source_subpop_id, 2 * parent2, 2 * parent2 + 1, p_chromosome, p_generation, IndividualSex::kHermaphrodite);
				
				migrant_count++;
				child_count++;
			}
		}
		
		pop_count++;
	}
	
	// the remainder of the children are generated by within-population matings
	double selfing_fraction = subpop.selfing_fraction_;
	int native_count = 0, natives_to_generate = num_migrants[migrant_source_count];
	int number_to_self = static_cast<int>(lround(natives_to_generate * selfing_fraction));
	
	if (sex_enabled)
	{
		// SEX ONLY
		int male_natives = static_cast<int>(lround(natives_to_generate * sex_ratio));
		int female_natives = natives_to_generate - male_natives;
		
		if (male_natives < 0 || female_natives < 0)
		{
			cerr << "ERROR (EvolveSubpopulation): negative number of migrants of one sex" << endl;
			exit(EXIT_FAILURE);
		}
		if (female_natives < number_to_self)
		{
			cerr << "ERROR (EvolveSubpopulation): insufficient female migrants " << female_natives << " to satisfy selfing demand " << number_to_self << endl;
			exit(EXIT_FAILURE);
		}
		
		// generate females first
		while (native_count < female_natives) 
		{
			child_genome1 = 2 * female_child_count;
			child_genome2 = child_genome1 + 1;
			
			// draw parents from this subpopulation
			parent1 = subpop.DrawFemaleParentUsingFitness();
			
			if (number_to_self > 0)
				parent2 = parent1, --number_to_self;	// self
			else
				parent2 = subpop.DrawMaleParentUsingFitness();
			
			// recombination, gene-conversion, mutation
			CrossoverMutation(&subpop, &subpop, child_genome1, p_subpop_id, 2 * parent1, 2 * parent1 + 1, p_chromosome, p_generation, IndividualSex::kFemale);
			CrossoverMutation(&subpop, &subpop, child_genome2, p_subpop_id, 2 * parent2, 2 * parent2 + 1, p_chromosome, p_generation, IndividualSex::kFemale);
			
			native_count++;
			female_child_count++;
		}
		
		// then generate males
		while (native_count < female_natives + male_natives) 
		{
			child_genome1 = 2 * (male_child_count + subpop.child_first_male_index_);
			child_genome2 = child_genome1 + 1;
			
			// draw parents from this subpopulation
			parent1 = subpop.DrawFemaleParentUsingFitness();
			parent2 = subpop.DrawMaleParentUsingFitness();
			
			// recombination, gene-conversion, mutation
			CrossoverMutation(&subpop, &subpop, child_genome1, p_subpop_id, 2 * parent1, 2 * parent1 + 1, p_chromosome, p_generation, IndividualSex::kMale);
			CrossoverMutation(&subpop, &subpop, child_genome2, p_subpop_id, 2 * parent2, 2 * parent2 + 1, p_chromosome, p_generation, IndividualSex::kMale);
			
			native_count++;
			male_child_count++;
		}
	}
	else
	{
		while (native_count < natives_to_generate) 
		{
			child_genome1 = 2 * child_count;
			child_genome2 = child_genome1 + 1;
			
			// draw parents from this subpopulation
			parent1 = subpop.DrawParentUsingFitness();
			
			if (number_to_self > 0)
				parent2 = parent1, --number_to_self;	// self
			else
				parent2 = subpop.DrawParentUsingFitness();
			
			// recombination, gene-conversion, mutation
			CrossoverMutation(&subpop, &subpop, child_genome1, p_subpop_id, 2 * parent1, 2 * parent1 + 1, p_chromosome, p_generation, IndividualSex::kHermaphrodite);
			CrossoverMutation(&subpop, &subpop, child_genome2, p_subpop_id, 2 * parent2, 2 * parent2 + 1, p_chromosome, p_generation, IndividualSex::kHermaphrodite);
			
			native_count++;
			child_count++;
		}
	}
	
	child_generation_valid = true;
}

// generate a child genome from parental genomes, with recombination, gene conversion, and mutation
void Population::CrossoverMutation(Subpopulation *subpop, Subpopulation *source_subpop, int p_child_genome_index, int p_source_subpop_id, int p_parent1_genome_index, int p_parent2_genome_index, const Chromosome &p_chromosome, int p_generation, IndividualSex p_child_sex)
{
	// child genome p_child_genome_index in subpopulation p_subpop_id is assigned outcome of cross-overs at breakpoints in all_breakpoints
	// between parent genomes p_parent1_genome_index and p_parent2_genome_index from subpopulation p_source_subpop_id and new mutations added
	// 
	// example: all_breakpoints = (r1, r2)
	// 
	// mutations (      x < r1) assigned from p1
	// mutations (r1 <= x < r2) assigned from p2
	// mutations (r2 <= x     ) assigned from p1
	
	if (p_child_sex == IndividualSex::kUnspecified)
		cerr << "ERROR (CrossoverMutation): Child sex cannot be IndividualSex::kUnspecified" << endl, exit(EXIT_FAILURE);
	
	bool use_only_strand_1 = false;		// if true, we are in a case where crossover cannot occur, and we are to use only parent strand 1
	bool do_swap = true;				// if true, we are to swap the parental strands at the beginning, either 50% of the time (if use_only_strand_1 is false), or always (if use_only_strand_1 is true – in other words, we are directed to use only strand 2)
	
	Genome &child_genome = subpop->child_genomes_[p_child_genome_index];
	GenomeType child_genome_type = child_genome.GenomeType();
	Genome *parent_genome_1 = &(source_subpop->parent_genomes_[p_parent1_genome_index]);
	GenomeType parent1_genome_type = parent_genome_1->GenomeType();
	Genome *parent_genome_2 = &(source_subpop->parent_genomes_[p_parent2_genome_index]);
	GenomeType parent2_genome_type = parent_genome_2->GenomeType();
	
	if (child_genome_type == GenomeType::kAutosome)
	{
		// If we're modeling autosomes, we can disregard p_child_sex entirely; we don't care whether we're modeling sexual or hermaphrodite individuals
		if (parent1_genome_type != GenomeType::kAutosome || parent2_genome_type != GenomeType::kAutosome)
			cerr << "ERROR (CrossoverMutation): Mismatch between parent and child genome types (case 1)" << endl, exit(EXIT_FAILURE);
	}
	else
	{
		// SEX ONLY: If we're modeling sexual individuals, then there are various degenerate cases to be considered, since X and Y don't cross over, there are null chromosomes, etc.
		if (p_child_sex == IndividualSex::kHermaphrodite)
			cerr << "ERROR (CrossoverMutation): A hermaphrodite child is requested but the child genome is not autosomal" << endl, exit(EXIT_FAILURE);
		
		if (parent1_genome_type == GenomeType::kAutosome || parent2_genome_type == GenomeType::kAutosome)
			cerr << "ERROR (CrossoverMutation): Mismatch between parent and child genome types (case 2)" << endl, exit(EXIT_FAILURE);
		
		if (child_genome_type == GenomeType::kXChromosome)
		{
			if (p_child_sex == IndividualSex::kMale)
			{
				// If our parent is male (XY or YX), then we have a mismatch, because we're supposed to be male and we're supposed to be getting an X chromosome, but the X must come from the female
				if (parent1_genome_type == GenomeType::kYChromosome || parent2_genome_type == GenomeType::kYChromosome)
					cerr << "ERROR (CrossoverMutation): Mismatch between parent and child genome types (case 3)" << endl, exit(EXIT_FAILURE);
				
				// else: we're doing inheritance from the female (XX) to get our X chromosome; we treat this just like the autosomal case
			}
			else if (p_child_sex == IndividualSex::kFemale)
			{
				if (parent1_genome_type == GenomeType::kYChromosome && parent2_genome_type == GenomeType::kXChromosome)
				{
					// we're doing inheritance from the male (YX) to get an X chromosome; we need to ensure that we take the X
					use_only_strand_1 = true, do_swap = true;	// use strand 2
				}
				else if (parent1_genome_type == GenomeType::kXChromosome && parent2_genome_type == GenomeType::kYChromosome)
				{
					// we're doing inheritance from the male (XY) to get an X chromosome; we need to ensure that we take the X
					use_only_strand_1 = true, do_swap = false;	// use strand 1
				}
				// else: we're doing inheritance from the female (XX) to get an X chromosome; we treat this just like the autosomal case
			}
		}
		else // (child_genome_type == GenomeType::kYChromosome), so p_child_sex == IndividualSex::kMale
		{
			if (p_child_sex == IndividualSex::kFemale)
				cerr << "ERROR (CrossoverMutation): A female child is requested but the child genome is a Y chromosome" << endl, exit(EXIT_FAILURE);
			
			if (parent1_genome_type == GenomeType::kYChromosome && parent2_genome_type == GenomeType::kXChromosome)
			{
				// we're doing inheritance from the male (YX) to get a Y chromosome; we need to ensure that we take the Y
				use_only_strand_1 = true, do_swap = false;	// use strand 1
			}
			else if (parent1_genome_type == GenomeType::kXChromosome && parent2_genome_type == GenomeType::kYChromosome)
			{
				// we're doing inheritance from the male (XY) to get an X chromosome; we need to ensure that we take the Y
				use_only_strand_1 = true, do_swap = true;	// use strand 2
			}
			else
			{
				// else: we're doing inheritance from the female (XX) to get a Y chromosome, so this is a mismatch
				cerr << "ERROR (CrossoverMutation): Mismatch between parent and child genome types (case 4)" << endl, exit(EXIT_FAILURE);
			}
		}
	}
	
	// swap strands in half of cases to assure random assortment (or in all cases, if use_only_strand_1 == true, meaning that crossover cannot occur)
	if (do_swap && (use_only_strand_1 || g_rng_bool(g_rng)))
	{
		int swap = p_parent1_genome_index;
		p_parent1_genome_index = p_parent2_genome_index;
		p_parent2_genome_index = swap;
		
		Genome *swap2 = parent_genome_1;
		parent_genome_1 = parent_genome_2;
		parent_genome_2 = swap2;
		
		GenomeType swap3 = parent1_genome_type;
		parent1_genome_type = parent2_genome_type;
		parent2_genome_type = swap3;
	}
	
	// check for null cases
	if (child_genome.IsNull())
	{
		if (!use_only_strand_1)
		{
			// If we're trying to cross over, both parental strands had better be null
			if (!parent_genome_1->IsNull() || !parent_genome_2->IsNull())
				cerr << "ERROR (CrossoverMutation): Child genome is null, but crossover is requested and a parental genome is non-null" << endl, exit(EXIT_FAILURE);
		}
		else
		{
			// So we are not crossing over, and we are supposed to use strand 1; it should also be null, otherwise something has gone wrong
			if (!parent_genome_1->IsNull())
				cerr << "Child genome is null, but the parental strand is not" << endl, exit(EXIT_FAILURE);
		}
		
		// a null strand cannot cross over and cannot mutate, so we are done
		return;
	}
	
	if (use_only_strand_1 && parent_genome_1->IsNull())
		cerr << "Child genome is non-null, but the parental strand is null" << endl, exit(EXIT_FAILURE);
	
	if (!use_only_strand_1 && (parent_genome_1->IsNull() || parent_genome_2->IsNull()))
		cerr << "Child genome is non-null, but a parental strand is null" << endl, exit(EXIT_FAILURE);
	
	//
	//	OK!  We should have covered all error cases above, so we can now proceed with more alacrity.  We just need to follow
	//	the instructions given to us from above, namely use_only_strand_1.  We know we are doing a non-null strand.
	//
	
	// start with a clean slate in the child genome
	child_genome.clear();
	
	// determine how many mutations and breakpoints we have
	int num_mutations, num_breakpoints;
	
	if (use_only_strand_1)
	{
		num_breakpoints = 0;
		num_mutations = p_chromosome.DrawMutationCount();
	}
	else
	{
		// get both the number of mutations and the number of breakpoints here; this allows us to draw both jointly, super fast!
		//int num_mutations = p_chromosome.DrawMutationCount();
		//int num_breakpoints = p_chromosome.DrawBreakpointCount();
		
		p_chromosome.DrawMutationAndBreakpointCounts(&num_mutations, &num_breakpoints);
	}
	
	// mutations are usually rare, so let's streamline the case where none occur
	if (num_mutations == 0)
	{
		if (num_breakpoints == 0)
		{
			// no mutations and no crossovers, so the child genome is just a copy of the parental genome
			child_genome.copy_from_genome(source_subpop->parent_genomes_[p_parent1_genome_index]);
		}
		else
		{
			// create vector with uniqued recombination breakpoints
			std::vector<int> all_breakpoints = p_chromosome.DrawBreakpoints(num_breakpoints);
			
			all_breakpoints.push_back(p_chromosome.length_ + 1);
			sort(all_breakpoints.begin(), all_breakpoints.end());
			all_breakpoints.erase(unique(all_breakpoints.begin(), all_breakpoints.end()), all_breakpoints.end());
			
			// do the crossover
			const Mutation **parent1_iter		= source_subpop->parent_genomes_[p_parent1_genome_index].begin_pointer();
			const Mutation **parent2_iter		= source_subpop->parent_genomes_[p_parent2_genome_index].begin_pointer();
			
			const Mutation **parent1_iter_max	= source_subpop->parent_genomes_[p_parent1_genome_index].end_pointer();
			const Mutation **parent2_iter_max	= source_subpop->parent_genomes_[p_parent2_genome_index].end_pointer();
			
			const Mutation **parent_iter		= parent1_iter;
			const Mutation **parent_iter_max	= parent1_iter_max;
			
			int break_index_max = static_cast<int>(all_breakpoints.size());
			
			for (int break_index = 0; break_index != break_index_max; break_index++)
			{
				int breakpoint = all_breakpoints[break_index];
				
				// while there are still old mutations in the parent before the current breakpoint...
				while (parent_iter != parent_iter_max)
				{
					const Mutation *current_mutation = *parent_iter;
					
					if (current_mutation->position_ >= breakpoint)
						break;
					
					// add the old mutation; no need to check for a duplicate here since the parental genome is already duplicate-free
					child_genome.push_back(current_mutation);
					
					parent_iter++;
				}
				
				// we have reached the breakpoint, so swap parents
				parent1_iter = parent2_iter;	parent1_iter_max = parent2_iter_max;
				parent2_iter = parent_iter;		parent2_iter_max = parent_iter_max;
				parent_iter = parent1_iter;		parent_iter_max = parent1_iter_max; 
				
				// skip over anything in the new parent that occurs prior to the breakpoint; it was not the active strand
				while (parent_iter != parent_iter_max && (*parent_iter)->position_ < breakpoint)
					parent_iter++;
			}
		}
	}
	else
	{
		// we have to be careful here not to touch the second strand if we have no breakpoints, because it could be null
		
		// create vector with the mutations to be added
		Genome mutations_to_add;
		
		for (int k = 0; k < num_mutations; k++)
		{
			const Mutation *new_mutation = p_chromosome.DrawNewMutation(p_source_subpop_id, p_generation);
			
			mutations_to_add.insert_sorted_mutation(new_mutation);	// keeps it sorted; since few mutations are expected, this is fast
			mutation_registry_.push_back(new_mutation);
		}
		
		// create vector with uniqued recombination breakpoints
		std::vector<int> all_breakpoints = p_chromosome.DrawBreakpoints(num_breakpoints); 
		all_breakpoints.push_back(p_chromosome.length_ + 1);
		sort(all_breakpoints.begin(), all_breakpoints.end());
		all_breakpoints.erase(unique(all_breakpoints.begin(), all_breakpoints.end()), all_breakpoints.end());
		
		// do the crossover
		const Mutation **parent1_iter		= source_subpop->parent_genomes_[p_parent1_genome_index].begin_pointer();
		const Mutation **parent2_iter		= (num_breakpoints == 0) ? nullptr : source_subpop->parent_genomes_[p_parent2_genome_index].begin_pointer();
		
		const Mutation **parent1_iter_max	= source_subpop->parent_genomes_[p_parent1_genome_index].end_pointer();
		const Mutation **parent2_iter_max	= (num_breakpoints == 0) ? nullptr : source_subpop->parent_genomes_[p_parent2_genome_index].end_pointer();
		
		const Mutation **mutation_iter		= mutations_to_add.begin_pointer();
		const Mutation **mutation_iter_max	= mutations_to_add.end_pointer();
		
		const Mutation **parent_iter		= parent1_iter;
		const Mutation **parent_iter_max	= parent1_iter_max;
		
		int break_index_max = static_cast<int>(all_breakpoints.size());
		int num_mutations_added = 0;
		bool present;
		
		for (int break_index = 0; ; )	// the other parts are below, but this is conceptually a for loop, so I've kept it that way...
		{
			int breakpoint = all_breakpoints[break_index];
			
			// NOTE these caches are valid from here...
			const Mutation *parent_iter_mutation, *mutation_iter_mutation;
			int parent_iter_pos, mutation_iter_pos;
			
			if (parent_iter != parent_iter_max) {
				parent_iter_mutation = *parent_iter;
				parent_iter_pos = parent_iter_mutation->position_;
			} else {
				parent_iter_mutation = nullptr;
				parent_iter_pos = INT_MAX;
			}
			
			if (mutation_iter != mutation_iter_max) {
				mutation_iter_mutation = *mutation_iter;
				mutation_iter_pos = mutation_iter_mutation->position_;
			} else {
				mutation_iter_mutation = nullptr;
				mutation_iter_pos = INT_MAX;
			}
			
			// while there are still old mutations in the parent, or new mutations to be added, before the current breakpoint...
			while ((parent_iter_pos < breakpoint) || (mutation_iter_pos < breakpoint))
			{
				// while an old mutation in the parent is before the breakpoint and before the next new mutation...
				while (parent_iter_pos < breakpoint && parent_iter_pos <= mutation_iter_pos)
				{
					present = false;
					
					// search back through the mutations already added to see if the one we intend to add is already present
					if (num_mutations_added != 0 && child_genome.back()->position_ == parent_iter_pos)
						for (int k = num_mutations_added - 1; k >= 0; k--)
							if (EqualMutations(child_genome[k], parent_iter_mutation))
							{
								present = true;
								break;
							}
					
					// if the mutation was not present, add it
					if (!present)
					{
						child_genome.push_back(parent_iter_mutation);
						num_mutations_added++;
					}
					
					parent_iter++;
					
					if (parent_iter != parent_iter_max) {
						parent_iter_mutation = *parent_iter;
						parent_iter_pos = parent_iter_mutation->position_;
					} else {
						parent_iter_mutation = nullptr;
						parent_iter_pos = INT_MAX;
					}
				}
				
				// while a new mutation is before the breakpoint and before the next old mutation in the parent...
				while (mutation_iter_pos < breakpoint && mutation_iter_pos <= parent_iter_pos)
				{
					present = false;
					
					// search back through the mutations already added to see if the one we intend to add is already present
					if (num_mutations_added != 0 && child_genome.back()->position_ == mutation_iter_pos)
						for (int k = num_mutations_added - 1; k >= 0; k--)
							if (EqualMutations(child_genome[k], mutation_iter_mutation))
							{
								present = true;
								break;
							}
					
					// if the mutation was not present, add it
					if (!present)
					{
						child_genome.push_back(mutation_iter_mutation);
						num_mutations_added++;
					}
					
					mutation_iter++;
					
					if (mutation_iter != mutation_iter_max) {
						mutation_iter_mutation = *mutation_iter;
						mutation_iter_pos = mutation_iter_mutation->position_;
					} else {
						mutation_iter_mutation = nullptr;
						mutation_iter_pos = INT_MAX;
					}
				}
			}
			// NOTE ...to here
			
			// these statements complete our for loop; the are here so that if we have no breakpoints we do not touch the second strand below
			break_index++;
			
			if (break_index == break_index_max)
				break;
			
			// we have reached the breakpoint, so swap parents
			parent1_iter = parent2_iter;	parent1_iter_max = parent2_iter_max;
			parent2_iter = parent_iter;		parent2_iter_max = parent_iter_max;
			parent_iter = parent1_iter;		parent_iter_max = parent1_iter_max; 
			
			// skip over anything in the new parent that occurs prior to the breakpoint; it was not the active strand
			while (parent_iter != parent_iter_max && (*parent_iter)->position_ < breakpoint)
				parent_iter++;
		}
	}
}

// step forward a generation: remove fixed mutations, then make the children become the parents and update fitnesses
void Population::SwapGenerations(int p_generation)
{
	// go through all genomes and increment mutation reference counts
	int total_genome_count = TallyMutationReferences();
	
	// remove any mutations that have been eliminated or have fixed
	RemoveFixedMutations(p_generation, total_genome_count);
	
	// check that the mutation registry does not have any "zombies" – mutations that have been removed and should no longer be there
#if DEBUG_MUTATION_ZOMBIES
	CheckMutationRegistry();
#endif
	
	// make children the new parents and update fitnesses
	for (std::pair<const int,Subpopulation*> &subpop_pair : *this)
	{ 
		Subpopulation *subpop = subpop_pair.second;
		
		// the children become the parents
		subpop->SwapChildAndParentGenomes();
		
		// and then we calculate the fitnesses of the parents and make lookup tables
		subpop->UpdateFitness(); 
	}
	
	// flip our flag to indicate that the good genomes are now in the parental generation, and the next child generation is ready to be produced
	child_generation_valid = false;
}

// count the total number of times that each Mutation in the registry is referenced by a population, and return the maximum possible number of references (i.e. fixation)
int Population::TallyMutationReferences(void)
{
	int total_genome_count = 0;
	
	// first zero out the refcounts in all registered Mutation objects
	const Mutation **registry_iter = mutation_registry_.begin_pointer();
	const Mutation **registry_iter_end = mutation_registry_.end_pointer();
	
	for (; registry_iter != registry_iter_end; ++registry_iter)
		(*registry_iter)->reference_count_ = 0;
	
	// then increment the refcounts through all pointers to Mutation in all genomes
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)			// subpopulations
	{
		Subpopulation *subpop = subpop_pair.second;
		int subpop_genome_count = 2 * subpop->child_subpop_size_;
		std::vector<Genome> &subpop_genomes = subpop->child_genomes_;
		
		for (int i = 0; i < subpop_genome_count; i++)								// child genomes
		{
			Genome &genome = subpop_genomes[i];
			
			if (!genome.IsNull())
			{
				const Mutation **genome_iter = genome.begin_pointer();
				const Mutation **genome_end_iter = genome.end_pointer();
				
				for (; genome_iter != genome_end_iter; ++genome_iter)
					++((*genome_iter)->reference_count_);
				
				total_genome_count++;	// count only non-null genomes to determine fixation
			}
		}
	}
	
	return total_genome_count;
}

// handle negative fixation (remove from the registry) and positive fixation (convert to Substitution), using reference counts from TallyMutationReferences()
void Population::RemoveFixedMutations(int p_generation, int p_total_genome_count)
{
	Genome fixed_mutation_accumulator;
	
	// remove Mutation objects that are no longer referenced, freeing them; avoid using an iterator since it would be invalidated
	int registry_length = mutation_registry_.size();
	
	for (int i = 0; i < registry_length; ++i)
	{
		const Mutation *mutation = mutation_registry_[i];
		int32_t reference_count = mutation->reference_count_;
		
		if (reference_count == 0)
		{
#if DEBUG_MUTATIONS
			cerr << "Mutation unreferenced, will remove: " << mutation << endl;
#endif
			
			// We have an unreferenced mutation object, so we want to remove it quickly
			if (i == registry_length - 1)
			{
				mutation_registry_.pop_back();
				
#if DEBUG_MUTATION_ZOMBIES
				const_cast<Mutation *>(mutation)->mutation_type_ptr_ = nullptr;		// render lethal
				mutation->reference_count_ = -1;									// zombie
#else
				delete mutation;
#endif
				
				--registry_length;
			}
			else
			{
				const Mutation *last_mutation = mutation_registry_[registry_length - 1];
				mutation_registry_[i] = last_mutation;
				mutation_registry_.pop_back();
				
#if DEBUG_MUTATION_ZOMBIES
				const_cast<Mutation *>(mutation)->mutation_type_ptr_ = nullptr;		// render lethal
				mutation->reference_count_ = -1;									// zombie
#else
				delete mutation;
#endif
				
				--registry_length;
				--i;	// revisit this index
			}
		}
		else if (reference_count == p_total_genome_count)
		{
#if DEBUG_MUTATIONS
			cerr << "Mutation fixed, will substitute: " << mutation << endl;
#endif
			
			// If this mutation was counted in every genome, then it is fixed; log it
			fixed_mutation_accumulator.insert_sorted_mutation(mutation);
		}
	}
	
	// replace fixed mutations with Substitution objects
	if (fixed_mutation_accumulator.size() > 0)
	{
		for (std::pair<const int,Subpopulation*> &subpop_pair : *this)		// subpopulations
		{
			for (int i = 0; i < 2 * subpop_pair.second->child_subpop_size_; i++)	// child genomes
			{
				Genome *genome = &(subpop_pair.second->child_genomes_[i]);
				
				// Fixed mutations are removed by looking at refcounts, so fixed_mutation_accumulator is not needed here
				if (!genome->IsNull())
					genome->RemoveFixedMutations(p_total_genome_count);
			}
		}
		
		for (int i = 0; i < fixed_mutation_accumulator.size(); i++)
			substitutions_.push_back(new Substitution(*(fixed_mutation_accumulator[i]), p_generation));
	}
}

void Population::CheckMutationRegistry(void)
{
	const Mutation **registry_iter = mutation_registry_.begin_pointer();
	const Mutation **registry_iter_end = mutation_registry_.end_pointer();
	
	// first check that we don't have any zombies in our registry
	for (; registry_iter != registry_iter_end; ++registry_iter)
		if ((*registry_iter)->reference_count_ == -1)
			cerr << "Zombie found in registry with address " << (*registry_iter) << endl;
	
	// then check that we don't have any zombies in any genomes
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)			// subpopulations
	{
		Subpopulation *subpop = subpop_pair.second;
		int subpop_genome_count = 2 * subpop->child_subpop_size_;
		std::vector<Genome> &subpop_genomes = subpop->child_genomes_;
		
		for (int i = 0; i < subpop_genome_count; i++)								// child genomes
		{
			Genome &genome = subpop_genomes[i];
			
			if (!genome.IsNull())
			{
				const Mutation **genome_iter = genome.begin_pointer();
				const Mutation **genome_end_iter = genome.end_pointer();
				
				for (; genome_iter != genome_end_iter; ++genome_iter)
					if ((*genome_iter)->reference_count_ == -1)
						cerr << "Zombie found in genome with address " << (*genome_iter) << endl;
			}
		}
	}
}

// print all mutations and all genomes to a stream
void Population::PrintAll(std::ostream &p_out) const
{
	// This function is written to be able to print the population whether child_generation_valid is true or false.
	// This is a little tricky, so be careful when modifying this code!
	
	p_out << "Populations:" << endl;
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)
	{
		Subpopulation *subpop = subpop_pair.second;
		int subpop_size = (child_generation_valid ? subpop->child_subpop_size_ : subpop->parent_subpop_size_);
		double subpop_sex_ratio = (child_generation_valid ? subpop->child_sex_ratio_ : subpop->parent_sex_ratio_);
		
		p_out << "p" << subpop_pair.first << " " << subpop_size;
		
		// SEX ONLY
		if (subpop->sex_enabled_)
			p_out << " S " << subpop_sex_ratio;
		else
			p_out << " H";
		
		p_out << endl;
	}
	
	multimap<const int,Polymorphism> polymorphisms;
	
	// add all polymorphisms
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)			// go through all subpopulations
	{
		Subpopulation *subpop = subpop_pair.second;
		int subpop_size = (child_generation_valid ? subpop->child_subpop_size_ : subpop->parent_subpop_size_);
		
		for (int i = 0; i < 2 * subpop_size; i++)				// go through all children
		{
			Genome &genome = child_generation_valid ? subpop->child_genomes_[i] : subpop->parent_genomes_[i];
			
			if (!genome.IsNull())
			{
				for (int k = 0; k < genome.size(); k++)	// go through all mutations
					AddMutationToPolymorphismMap(&polymorphisms, *genome[k]);
			}
		}
	}
	
	// print all polymorphisms
	p_out << "Mutations:"  << endl;
	
	for (const std::pair<const int,Polymorphism> &polymorphism_pair : polymorphisms)
		polymorphism_pair.second.print(p_out, polymorphism_pair.first);
	
	// print all individuals
	p_out << "Individuals:" << endl;
	
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)			// go through all subpopulations
	{
		Subpopulation *subpop = subpop_pair.second;
		int subpop_id = subpop_pair.first;
		int subpop_size = (child_generation_valid ? subpop->child_subpop_size_ : subpop->parent_subpop_size_);
		int first_male_index = (child_generation_valid ? subpop->child_first_male_index_ : subpop->parent_first_male_index_);
		
		for (int i = 0; i < subpop_size; i++)								// go through all children
		{
			p_out << "p" << subpop_id << ":i" << i + 1;								// individual identifier
			
			if (subpop->sex_enabled_)
				p_out << ((i < first_male_index) ? " F " : " M ");			// sex: SEX ONLY
			else
				p_out << " H ";														// hermaphrodite
			
			p_out << "p" << subpop_id << ":" << (i * 2 + 1);						// genome identifier 1
			p_out << " p" << subpop_id << ":" << (i * 2 + 2);						// genome identifier 2
			p_out << endl;
		}
	}
	
	// print all genomes
	p_out << "Genomes:" << endl;
	
	for (const std::pair<const int,Subpopulation*> &subpop_pair : *this)			// go through all subpopulations
	{
		Subpopulation *subpop = subpop_pair.second;
		int subpop_id = subpop_pair.first;
		int subpop_size = (child_generation_valid ? subpop->child_subpop_size_ : subpop->parent_subpop_size_);
		
		for (int i = 0; i < 2 * subpop_size; i++)							// go through all children
		{
			Genome &genome = child_generation_valid ? subpop->child_genomes_[i] : subpop->parent_genomes_[i];
			
			p_out << "p" << subpop_id << ":" << i + 1 << " " << genome.GenomeType();
			
			if (genome.IsNull())
			{
				p_out << " <null>";
			}
			else
			{
				for (int k = 0; k < genome.size(); k++)								// go through all mutations
				{
					int id = FindMutationInPolymorphismMap(polymorphisms, *genome[k]);
					p_out << " " << id; 
				}
			}
			
			p_out << endl;
		}
	}
}

// print sample of p_sample_size genomes from subpopulation p_subpop_id
void Population::PrintSample(int p_subpop_id, int p_sample_size, IndividualSex p_requested_sex) const
{
	if (!child_generation_valid)
	{
		cerr << "ERROR (PrintSample): called when the child generation was not valid" << endl;
		exit(EXIT_FAILURE);
	}
	
	Subpopulation &subpop = SubpopulationWithID(p_subpop_id);
	
	if (p_requested_sex == IndividualSex::kFemale && subpop.modeled_chromosome_type_ == GenomeType::kYChromosome)
	{
		cerr << "ERROR (PrintSample): called to output Y chromosomes from females" << endl;
		exit(EXIT_FAILURE);
	}
	
	// assemble a sample (with replacement, for statistics) and get the polymorphisms within it
	std::vector<int> sample; 
	multimap<const int,Polymorphism> polymorphisms;
	
	for (int s = 0; s < p_sample_size; s++)
	{
		int j;
		
		// Scan for a genome that is not null and that belongs to a child of the requested sex
		do {
			j = static_cast<int>(gsl_rng_uniform_int(g_rng, subpop.child_genomes_.size()));		// select a random genome (not a random individual)
		} while (subpop.child_genomes_[j].IsNull() || (subpop.sex_enabled_ && p_requested_sex != IndividualSex::kUnspecified && subpop.SexOfChild(j / 2) != p_requested_sex));
		
		sample.push_back(j);
		
		for (int k = 0; k < subpop.child_genomes_[j].size(); k++)			// go through all mutations
			AddMutationToPolymorphismMap(&polymorphisms, *subpop.child_genomes_[j][k]);
	}
	
	// print the sample's polymorphisms
	cout << "Mutations:"  << endl;
	
	for (const std::pair<const int,Polymorphism> &polymorphism_pair : polymorphisms) 
		polymorphism_pair.second.print(cout, polymorphism_pair.first);
	
	// print the sample's genomes
	cout << "Genomes:" << endl;
	
	for (int j = 0; j < sample.size(); j++)														// go through all children
	{
		Genome &genome = subpop.child_genomes_[sample[j]];
		
		cout << "p" << p_subpop_id << ":" << sample[j] + 1 << " " << genome.GenomeType();
		
		if (genome.IsNull())
		{
			cout << " <null>";
		}
		else
		{
			for (int k = 0; k < genome.size(); k++)	// go through all mutations
			{
				int mutation_id = FindMutationInPolymorphismMap(polymorphisms, *genome[k]);
				
				cout << " " << mutation_id;
			}
		}
		
		cout << endl;
	}
}

// print sample of p_sample_size genomes from subpopulation p_subpop_id, using "ms" format
void Population::PrintSample_ms(int p_subpop_id, int p_sample_size, const Chromosome &p_chromosome, IndividualSex p_requested_sex) const
{
	if (!child_generation_valid)
	{
		cerr << "ERROR (PrintSample_ms): called when the child generation was not valid" << endl;
		exit(EXIT_FAILURE);
	}
	
	Subpopulation &subpop = SubpopulationWithID(p_subpop_id);
	
	if (p_requested_sex == IndividualSex::kFemale && subpop.modeled_chromosome_type_ == GenomeType::kYChromosome)
	{
		cerr << "ERROR (PrintSample_ms): called to output Y chromosomes from females" << endl;
		exit(EXIT_FAILURE);
	}
	
	// assemble a sample (with replacement, for statistics) and get the polymorphisms within it
	std::vector<int> sample; 
	multimap<const int,Polymorphism> polymorphisms;
	
	for (int s = 0; s < p_sample_size; s++)
	{
		int j;
		
		// Scan for a genome that is not null and that belongs to a child of the requested sex
		do {
			j = static_cast<int>(gsl_rng_uniform_int(g_rng, subpop.child_genomes_.size()));		// select a random genome (not a random individual)
		} while (subpop.child_genomes_[j].IsNull() || (subpop.sex_enabled_ && p_requested_sex != IndividualSex::kUnspecified && subpop.SexOfChild(j / 2) != p_requested_sex));
		
		sample.push_back(j);
		
		for (int k = 0; k < subpop.child_genomes_[j].size(); k++)			// go through all mutations
			AddMutationToPolymorphismMap(&polymorphisms, *subpop.child_genomes_[j][k]);
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
		
		for (int k = 0; k < subpop.child_genomes_[sample[j]].size(); k++)	// go through all mutations
		{
			const Mutation *mutation = subpop.child_genomes_[sample[j]][k];
			int position = 0;
			
			for (const std::pair<const int,Polymorphism> &polymorphism_pair : polymorphisms) 
			{
				if (polymorphism_pair.first == mutation->position_ && polymorphism_pair.second.mutation_type_ptr_ == mutation->mutation_type_ptr_ && polymorphism_pair.second.selection_coeff_ == mutation->selection_coeff_)
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






































































