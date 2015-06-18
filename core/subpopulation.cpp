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
#include "slim_sim.h"
#include "slim_global.h"
#include "script_functionsignature.h"


using std::string;
using std::endl;


// given the subpop size and sex ratio currently set for the child generation, make new genomes to fit
void Subpopulation::GenerateChildrenToFit(const bool p_parents_also)
{
#ifdef DEBUG
	bool old_log = Genome::LogGenomeCopyAndAssign(false);
#endif
	
	// throw out whatever used to be there
	child_genomes_.clear();
	if (p_parents_also)
		parent_genomes_.clear();
	
	// make new stuff
	if (sex_enabled_)
	{
		// Figure out the first male index from the sex ratio, and exit if we end up with all one sex
		child_first_male_index_ = static_cast<int>(lround((1.0 - child_sex_ratio_) * child_subpop_size_));
		
		if (child_first_male_index_ <= 0)
			SLIM_TERMINATION << "ERROR (GenerateChildrenToFit): child sex ratio of " << child_sex_ratio_ << " produced no females" << slim_terminate();
		else if (child_first_male_index_ >= child_subpop_size_)
			SLIM_TERMINATION << "ERROR (GenerateChildrenToFit): child sex ratio of " << child_sex_ratio_ << " produced no males" << slim_terminate();
		
		if (p_parents_also)
		{
			parent_first_male_index_ = static_cast<int>(lround((1.0 - parent_sex_ratio_) * parent_subpop_size_));
			
			if (parent_first_male_index_ <= 0)
				SLIM_TERMINATION << "ERROR (GenerateChildrenToFit): parent sex ratio of " << parent_sex_ratio_ << " produced no females" << slim_terminate();
			else if (parent_first_male_index_ >= parent_subpop_size_)
				SLIM_TERMINATION << "ERROR (GenerateChildrenToFit): parent sex ratio of " << parent_sex_ratio_ << " produced no males" << slim_terminate();
		}
		
		switch (modeled_chromosome_type_)
		{
			case GenomeType::kAutosome:
				// produces default Genome objects of type GenomeType::kAutosome
				child_genomes_.resize(2 * child_subpop_size_);
				if (p_parents_also)
					parent_genomes_.resize(2 * parent_subpop_size_);
				break;
			case GenomeType::kXChromosome:
			case GenomeType::kYChromosome:
			{
				// if we are not modeling a given chromosome type, then instances of it are null – they will log and exit if used
				Genome x_model = Genome(GenomeType::kXChromosome, modeled_chromosome_type_ != GenomeType::kXChromosome);
				Genome y_model = Genome(GenomeType::kYChromosome, modeled_chromosome_type_ != GenomeType::kYChromosome);
				
				child_genomes_.reserve(2 * child_subpop_size_);
				
				// females get two Xs
				for (int i = 0; i < child_first_male_index_; ++i)
				{
					child_genomes_.push_back(x_model);
					child_genomes_.push_back(x_model);
				}
				
				// males get an X and a Y
				for (int i = child_first_male_index_; i < child_subpop_size_; ++i)
				{
					child_genomes_.push_back(x_model);
					child_genomes_.push_back(y_model);
				}
				
				if (p_parents_also)
				{
					parent_genomes_.reserve(2 * parent_subpop_size_);
					
					// females get two Xs
					for (int i = 0; i < parent_first_male_index_; ++i)
					{
						parent_genomes_.push_back(x_model);
						parent_genomes_.push_back(x_model);
					}
					
					// males get an X and a Y
					for (int i = parent_first_male_index_; i < parent_subpop_size_; ++i)
					{
						parent_genomes_.push_back(x_model);
						parent_genomes_.push_back(y_model);
					}
				}
				break;
			}
		}
	}
	else
	{
		// produces default Genome objects of type GenomeType::kAutosome
		child_genomes_.resize(2 * child_subpop_size_);
		if (p_parents_also)
			parent_genomes_.resize(2 * parent_subpop_size_);
	}
	
#ifdef DEBUG
	Genome::LogGenomeCopyAndAssign(old_log);
#endif
}

Subpopulation::Subpopulation(Population &p_population, int p_subpopulation_id, int p_subpop_size) : population_(p_population), subpopulation_id_(p_subpopulation_id), parent_subpop_size_(p_subpop_size), child_subpop_size_(p_subpop_size)
{
	GenerateChildrenToFit(true);
	
	// Set up to draw random individuals, based initially on equal fitnesses
	double A[parent_subpop_size_];
	
	for (int i = 0; i < parent_subpop_size_; i++)
		A[i] = 1.0;
	
	lookup_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_, A);
}

// SEX ONLY
Subpopulation::Subpopulation(Population &p_population, int p_subpopulation_id, int p_subpop_size, double p_sex_ratio, GenomeType p_modeled_chromosome_type, double p_x_chromosome_dominance_coeff) :
	population_(p_population), subpopulation_id_(p_subpopulation_id), sex_enabled_(true), parent_subpop_size_(p_subpop_size), child_subpop_size_(p_subpop_size), parent_sex_ratio_(p_sex_ratio), child_sex_ratio_(p_sex_ratio), modeled_chromosome_type_(p_modeled_chromosome_type), x_chromosome_dominance_coeff_(p_x_chromosome_dominance_coeff)
{
	GenerateChildrenToFit(true);
	
	// Set up to draw random females, based initially on equal fitnesses
	double A[parent_first_male_index_];
	
	for (int i = 0; i < parent_first_male_index_; i++)
		A[i] = 1.0;
	
	lookup_female_parent_ = gsl_ran_discrete_preproc(parent_first_male_index_, A);
	
	// Set up to draw random males, based initially on equal fitnesses
	int num_males = parent_subpop_size_ - parent_first_male_index_;
	double B[num_males];
	
	for (int i = 0; i < num_males; i++)
		B[i] = 1.0;
	
	lookup_male_parent_ = gsl_ran_discrete_preproc(num_males, B);
}

Subpopulation::~Subpopulation(void)
{
	//SLIM_ERRSTREAM << "Subpopulation::~Subpopulation" << std::endl;
	
	gsl_ran_discrete_free(lookup_parent_);
	gsl_ran_discrete_free(lookup_female_parent_);
	gsl_ran_discrete_free(lookup_male_parent_);
}

void Subpopulation::UpdateFitness(std::vector<SLiMScriptBlock*> &p_fitness_callbacks)
{
#ifdef SLIMGUI
	// When running under SLiMgui, this function calculates the population mean fitness as a side effect
	double totalFitness = 0.0;
	
	// Under SLiM, we also cache the calculated fitness values, for use in PopulationView
	gui_cached_parental_fitness_.clear();
#endif
	
	// calculate fitnesses in parent population and create new lookup table
	if (sex_enabled_)
	{
		// SEX ONLY
		gsl_ran_discrete_free(lookup_female_parent_);
		gsl_ran_discrete_free(lookup_male_parent_);
		
		// Set up to draw random females
		double A[parent_first_male_index_];
		
		for (int i = 0; i < parent_first_male_index_; i++)
		{
			double fitness = FitnessOfParentWithGenomeIndices(2 * i, 2 * i + 1, p_fitness_callbacks);
			
			A[i] = fitness;
#ifdef SLIMGUI
			totalFitness += fitness;
			gui_cached_parental_fitness_.push_back(fitness);
#endif
		}
		
		lookup_female_parent_ = gsl_ran_discrete_preproc(parent_first_male_index_, A);
		
		// Set up to draw random males
		int num_males = parent_subpop_size_ - parent_first_male_index_;
		double B[num_males];
		
		for (int i = 0; i < num_males; i++)
		{
			double fitness = FitnessOfParentWithGenomeIndices(2 * (i + parent_first_male_index_), 2 * (i + parent_first_male_index_) + 1, p_fitness_callbacks);
			
			B[i] = fitness;
#ifdef SLIMGUI
			totalFitness += fitness;
			gui_cached_parental_fitness_.push_back(fitness);
#endif
		}
		
		lookup_male_parent_ = gsl_ran_discrete_preproc(num_males, B);
	}
	else
	{
		gsl_ran_discrete_free(lookup_parent_);
		
		double A[parent_subpop_size_];
		
		for (int i = 0; i < parent_subpop_size_; i++)
		{
			double fitness = FitnessOfParentWithGenomeIndices(2 * i, 2 * i + 1, p_fitness_callbacks);
			
			A[i] = fitness;
#ifdef SLIMGUI
			totalFitness += fitness;
			gui_cached_parental_fitness_.push_back(fitness);
#endif
		}
		
		lookup_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_, A);
	}
	
#ifdef SLIMGUI
	parental_total_fitness_ = totalFitness;
#endif
}

double Subpopulation::ApplyFitnessCallbacks(Mutation *p_mutation, int p_homozygous, double p_computed_fitness, std::vector<SLiMScriptBlock*> &p_fitness_callbacks, Genome *genome1, Genome *genome2)
{
	int mutation_type_id = p_mutation->mutation_type_ptr_->mutation_type_id_;
	SLiMSim &sim = population_.sim_;
	
	for (SLiMScriptBlock *fitness_callback : p_fitness_callbacks)
	{
		if (fitness_callback->active_)
		{
			int callback_mutation_type_id = fitness_callback->mutation_type_id_;
			
			if ((callback_mutation_type_id == -1) || (callback_mutation_type_id == mutation_type_id))
			{
				// The callback is active and matches the mutation type id of the mutation, so we need to execute it
				// This code is similar to Population::ExecuteScript, but we inject some additional values, and we use the return value
				ScriptInterpreter interpreter(fitness_callback->compound_statement_node_);
				SymbolTable &global_symbols = interpreter.BorrowSymbolTable();
				
				sim.InjectIntoInterpreter(interpreter, fitness_callback);
				
				global_symbols.SetConstantForSymbol("mut", new ScriptValue_Object(p_mutation));
				global_symbols.SetConstantForSymbol("relFitness", new ScriptValue_Float(p_computed_fitness));
				global_symbols.SetConstantForSymbol("genome1", new ScriptValue_Object(genome1));
				global_symbols.SetConstantForSymbol("genome2", new ScriptValue_Object(genome2));
				global_symbols.SetConstantForSymbol("subpop", new ScriptValue_Object(this));
				
				// p_homozygous == -1 means the mutation is opposed by a NULL chromosome; otherwise, 0 means heterozyg., 1 means homozyg.
				// that gets translated into SLiMScript values of NULL, F, and T, respectively
				if (p_homozygous == -1)
					global_symbols.SetConstantForSymbol("homozygous", new ScriptValue_NULL());
				else
					global_symbols.SetConstantForSymbol("homozygous", new ScriptValue_Logical(p_homozygous != 0));
				
				// Interpret the script; the result from the interpretation must be a singleton double used as a new fitness value
				ScriptValue *result = interpreter.EvaluateScriptBlock();
				
				if ((result->Type() != ScriptValueType::kValueFloat) || (result->Count() != 1))
					SLIM_TERMINATION << "ERROR (ApplyFitnessCallbacksToFitness): fitness() callbacks must provide a float singleton return value." << slim_terminate();
				
				p_computed_fitness = result->FloatAtIndex(0);
				
				if (!result->InSymbolTable()) delete result;
				
				// Output generated by the interpreter goes to our output stream
				SLIM_OUTSTREAM << interpreter.ExecutionOutput();
			}
		}
	}
	
	return p_computed_fitness;
}

double Subpopulation::FitnessOfParentWithGenomeIndices(int p_genome_index1, int p_genome_index2, std::vector<SLiMScriptBlock*> &p_fitness_callbacks)
{
	// Are any fitness() callbacks active this generation?  Unfortunately, all the code below is bifurcated based on this flag.  This is for two reasons.  First,
	// it allows the case without fitness() callbacks to run at full speed; testing the flag each time around the loop through the mutations in the genomes
	// would presumably result in a large speed penalty.  Second, the non-callback case short-circuits when the selection coefficient is exactly 0.0f, as an
	// optimization; but that optimization would be invalid in the callback case, since callbacks can change the relative fitness of ostensibly neutral
	// mutations.  For reasons of maintainability, the branches on fitness_callbacks_exist below should be kept in synch as closely as possible.
	bool fitness_callbacks_exist = (p_fitness_callbacks.size() > 0);
	
	// calculate the fitness of the individual constituted by genome1 and genome2 in the parent population
	double w = 1.0;
	
	Genome *genome1 = &(parent_genomes_[p_genome_index1]);
	Genome *genome2 = &(parent_genomes_[p_genome_index2]);
	
	if (genome1->IsNull() && genome2->IsNull())
	{
		// SEX ONLY: both genomes are placeholders; for example, we might be simulating the Y chromosome, and this is a female
		return w;
	}
	else if (genome1->IsNull() || genome2->IsNull())
	{
		// SEX ONLY: one genome is null, so we just need to scan through the modeled genome and account for its mutations, including the x-dominance coefficient
		const Genome *genome = genome1->IsNull() ? genome2 : genome1;
		Mutation **genome_iter = genome->begin_pointer();
		Mutation **genome_max = genome->end_pointer();
		
		if (genome->GenomeType() == GenomeType::kXChromosome)
		{
			// with an unpaired X chromosome, we need to multiply each selection coefficient by the X chromosome dominance coefficient
			if (fitness_callbacks_exist)
			{
				while (genome_iter != genome_max)
				{
					Mutation *genome_mutation = *genome_iter;
					float selection_coeff = genome_mutation->selection_coeff_;
					double rel_fitness = (1.0 + x_chromosome_dominance_coeff_ * selection_coeff);
					
					w *= ApplyFitnessCallbacks(genome_mutation, -1, rel_fitness, p_fitness_callbacks, genome1, genome2);
					
					if (w <= 0.0)
						return 0.0;
					
					genome_iter++;
				}
			}
			else
			{
				while (genome_iter != genome_max)
				{
					const Mutation *genome_mutation = *genome_iter;
					float selection_coeff = genome_mutation->selection_coeff_;
					
					if (selection_coeff != 0.0f)
					{
						w *= (1.0 + x_chromosome_dominance_coeff_ * selection_coeff);
						
						if (w <= 0.0)
							return 0.0;
					}
					
					genome_iter++;
				}
			}
		}
		else
		{
			// with other types of unpaired chromosomes (like the Y chromosome of a male when we are modeling the Y) there is no dominance coefficient
			if (fitness_callbacks_exist)
			{
				while (genome_iter != genome_max)
				{
					Mutation *genome_mutation = *genome_iter;
					float selection_coeff = genome_mutation->selection_coeff_;
					double rel_fitness = (1.0 + selection_coeff);
					
					w *= ApplyFitnessCallbacks(genome_mutation, -1, rel_fitness, p_fitness_callbacks, genome1, genome2);
					
					if (w <= 0.0)
						return 0.0;
					
					genome_iter++;
				}
			}
			else
			{
				while (genome_iter != genome_max)
				{
					const Mutation *genome_mutation = *genome_iter;
					float selection_coeff = genome_mutation->selection_coeff_;
					
					if (selection_coeff != 0.0f)
					{
						w *= (1.0 + selection_coeff);
						
						if (w <= 0.0)
							return 0.0;
					}
					
					genome_iter++;
				}
			}
		}
		
		return w;
	}
	else
	{
		// both genomes are being modeled, so we need to scan through and figure out which mutations are heterozygous and which are homozygous
		Mutation **genome1_iter = genome1->begin_pointer();
		Mutation **genome2_iter = genome2->begin_pointer();
		
		Mutation **genome1_max = genome1->end_pointer();
		Mutation **genome2_max = genome2->end_pointer();
		
		// first, handle the situation before either genome iterator has reached the end of its genome, for simplicity/speed
		if (genome1_iter != genome1_max && genome2_iter != genome2_max)
		{
			Mutation *genome1_mutation = *genome1_iter, *genome2_mutation = *genome2_iter;
			int genome1_iter_position = genome1_mutation->position_, genome2_iter_position = genome2_mutation->position_;
			
			if (fitness_callbacks_exist)
			{
				do
				{
					if (genome1_iter_position < genome2_iter_position)
					{
						// Process a mutation in genome1 since it is leading
						float selection_coeff = genome1_mutation->selection_coeff_;
						double rel_fitness = (1.0 + genome1_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
						
						w *= ApplyFitnessCallbacks(genome1_mutation, false, rel_fitness, p_fitness_callbacks, genome1, genome2);
						
						if (w <= 0.0)
							return 0.0;
						
						genome1_iter++;
						
						if (genome1_iter == genome1_max)
							break;
						else {
							genome1_mutation = *genome1_iter;
							genome1_iter_position = genome1_mutation->position_;
						}
					}
					else if (genome1_iter_position > genome2_iter_position)
					{
						// Process a mutation in genome2 since it is leading
						float selection_coeff = genome2_mutation->selection_coeff_;
						double rel_fitness = (1.0 + genome2_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);

						w *= ApplyFitnessCallbacks(genome2_mutation, false, rel_fitness, p_fitness_callbacks, genome1, genome2);
						
						if (w <= 0.0)
							return 0.0;
						
						genome2_iter++;
						
						if (genome2_iter == genome2_max)
							break;
						else {
							genome2_mutation = *genome2_iter;
							genome2_iter_position = genome2_mutation->position_;
						}
					}
					else
					{
						// Look for homozygosity: genome1_iter_position == genome2_iter_position
						int position = genome1_iter_position;
						Mutation **genome1_start = genome1_iter;
						
						// advance through genome1 as long as we remain at the same position, handling one mutation at a time
						do
						{
							float selection_coeff = genome1_mutation->selection_coeff_;
							MutationType *mutation_type_ptr = genome1_mutation->mutation_type_ptr_;
							Mutation **genome2_matchscan = genome2_iter; 
							bool homozygous = false;
							
							// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
							while (genome2_matchscan != genome2_max && (*genome2_matchscan)->position_ == position)
							{
								if (mutation_type_ptr == (*genome2_matchscan)->mutation_type_ptr_ && selection_coeff == (*genome2_matchscan)->selection_coeff_)		// FIXME should use pointer equality!!!
								{
									// a match was found, so we multiply our fitness by the full selection coefficient
									double rel_fitness = (1.0 + selection_coeff);
									
									w *= ApplyFitnessCallbacks(genome1_mutation, true, rel_fitness, p_fitness_callbacks, genome1, genome2);
									homozygous = true;
									
									if (w <= 0.0)
										return 0.0;
									
									break;
								}
								
								genome2_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							if (!homozygous)
							{
								double rel_fitness = (1.0 + mutation_type_ptr->dominance_coeff_ * selection_coeff);
								
								w *= ApplyFitnessCallbacks(genome1_mutation, false, rel_fitness, p_fitness_callbacks, genome1, genome2);
								
								if (w <= 0.0)
									return 0.0;
							}
							
							genome1_iter++;
							
							if (genome1_iter == genome1_max)
								break;
							else {
								genome1_mutation = *genome1_iter;
								genome1_iter_position = genome1_mutation->position_;
							}
						} while (genome1_iter_position == position);
						
						// advance through genome2 as long as we remain at the same position, handling one mutation at a time
						do
						{
							float selection_coeff = genome2_mutation->selection_coeff_;
							MutationType *mutation_type_ptr = genome2_mutation->mutation_type_ptr_;
							Mutation **genome1_matchscan = genome1_start; 
							bool homozygous = false;
							
							// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
							while (genome1_matchscan != genome1_max && (*genome1_matchscan)->position_ == position)
							{
								if (mutation_type_ptr == (*genome1_matchscan)->mutation_type_ptr_ && selection_coeff == (*genome1_matchscan)->selection_coeff_)		// FIXME should use pointer equality!!!
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
								double rel_fitness = (1.0 + mutation_type_ptr->dominance_coeff_ * selection_coeff);
								
								w *= ApplyFitnessCallbacks(genome2_mutation, false, rel_fitness, p_fitness_callbacks, genome1, genome2);
								
								if (w <= 0.0)
									return 0.0;
							}
							
							genome2_iter++;
							
							if (genome2_iter == genome2_max)
								break;
							else {
								genome2_mutation = *genome2_iter;
								genome2_iter_position = genome2_mutation->position_;
							}
						} while (genome2_iter_position == position);
						
						// break out if either genome has reached its end
						if (genome1_iter == genome1_max || genome2_iter == genome2_max)
							break;
					}
				} while (true);
			}
			else
			{
				do
				{
					if (genome1_iter_position < genome2_iter_position)
					{
						// Process a mutation in genome1 since it is leading
						float selection_coeff = genome1_mutation->selection_coeff_;
						
						if (selection_coeff != 0.0f)
						{
							w *= (1.0 + genome1_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
							
							if (w <= 0.0)
								return 0.0;
						}
						
						genome1_iter++;
						
						if (genome1_iter == genome1_max)
							break;
						else {
							genome1_mutation = *genome1_iter;
							genome1_iter_position = genome1_mutation->position_;
						}
					}
					else if (genome1_iter_position > genome2_iter_position)
					{
						// Process a mutation in genome2 since it is leading
						float selection_coeff = genome2_mutation->selection_coeff_;
						
						if (selection_coeff != 0.0f)
						{
							w *= (1.0 + genome2_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
							
							if (w <= 0.0)
								return 0.0;
						}
						
						genome2_iter++;
						
						if (genome2_iter == genome2_max)
							break;
						else {
							genome2_mutation = *genome2_iter;
							genome2_iter_position = genome2_mutation->position_;
						}
					}
					else
					{
						// Look for homozygosity: genome1_iter_position == genome2_iter_position
						int position = genome1_iter_position;
						Mutation **genome1_start = genome1_iter;
						
						// advance through genome1 as long as we remain at the same position, handling one mutation at a time
						do
						{
							float selection_coeff = genome1_mutation->selection_coeff_;
							
							if (selection_coeff != 0.0f)
							{
								MutationType *mutation_type_ptr = genome1_mutation->mutation_type_ptr_;
								Mutation **genome2_matchscan = genome2_iter; 
								bool homozygous = false;
								
								// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
								while (genome2_matchscan != genome2_max && (*genome2_matchscan)->position_ == position)
								{
									if (mutation_type_ptr == (*genome2_matchscan)->mutation_type_ptr_ && selection_coeff == (*genome2_matchscan)->selection_coeff_) 		// FIXME should use pointer equality!!!
									{
										// a match was found, so we multiply our fitness by the full selection coefficient
										w *= (1.0 + selection_coeff);
										homozygous = true;
										
										if (w <= 0.0)
											return 0.0;
										
										break;
									}
									
									genome2_matchscan++;
								}
								
								// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
								if (!homozygous)
								{
									w *= (1.0 + mutation_type_ptr->dominance_coeff_ * selection_coeff);
									
									if (w <= 0.0)
										return 0.0;
								}
							}
							
							genome1_iter++;
							
							if (genome1_iter == genome1_max)
								break;
							else {
								genome1_mutation = *genome1_iter;
								genome1_iter_position = genome1_mutation->position_;
							}
						} while (genome1_iter_position == position);
						
						// advance through genome2 as long as we remain at the same position, handling one mutation at a time
						do
						{
							float selection_coeff = genome2_mutation->selection_coeff_;
							
							if (selection_coeff != 0.0f)
							{
								MutationType *mutation_type_ptr = genome2_mutation->mutation_type_ptr_;
								Mutation **genome1_matchscan = genome1_start; 
								bool homozygous = false;
								
								// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
								while (genome1_matchscan != genome1_max && (*genome1_matchscan)->position_ == position)
								{
									if (mutation_type_ptr == (*genome1_matchscan)->mutation_type_ptr_ && selection_coeff == (*genome1_matchscan)->selection_coeff_)		// FIXME should use pointer equality!!!
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
									w *= (1.0 + mutation_type_ptr->dominance_coeff_ * selection_coeff);
									
									if (w <= 0.0)
										return 0.0;
								}
							}
							
							genome2_iter++;
							
							if (genome2_iter == genome2_max)
								break;
							else {
								genome2_mutation = *genome2_iter;
								genome2_iter_position = genome2_mutation->position_;
							}
						} while (genome2_iter_position == position);
						
						// break out if either genome has reached its end
						if (genome1_iter == genome1_max || genome2_iter == genome2_max)
							break;
					}
				} while (true);
			}
		}
		
		// one or the other genome has now reached its end, so now we just need to handle the remaining mutations in the unfinished genome
		assert(!(genome1_iter != genome1_max && genome2_iter != genome2_max));
		
		// if genome1 is unfinished, finish it
		if (fitness_callbacks_exist)
		{
			while (genome1_iter != genome1_max)
			{
				Mutation *genome1_mutation = *genome1_iter;
				float selection_coeff = genome1_mutation->selection_coeff_;
				double rel_fitness = (1.0 + genome1_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
				
				w *= ApplyFitnessCallbacks(genome1_mutation, false, rel_fitness, p_fitness_callbacks, genome1, genome2);
				
				if (w <= 0.0)
					return 0.0;
				
				genome1_iter++;
			}
		}
		else
		{
			while (genome1_iter != genome1_max)
			{
				const Mutation *genome1_mutation = *genome1_iter;
				float selection_coeff = genome1_mutation->selection_coeff_;
				
				if (selection_coeff != 0.0f)
				{
					w *= (1.0 + genome1_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
					
					if (w <= 0.0)
						return 0.0;
				}
				
				genome1_iter++;
			}
		}
		
		// if genome2 is unfinished, finish it
		if (fitness_callbacks_exist)
		{
			while (genome2_iter != genome2_max)
			{
				Mutation *genome2_mutation = *genome2_iter;
				float selection_coeff = genome2_mutation->selection_coeff_;
				double rel_fitness = (1.0 + genome2_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
				
				w *= ApplyFitnessCallbacks(genome2_mutation, false, rel_fitness, p_fitness_callbacks, genome1, genome2);
				
				if (w <= 0.0)
					return 0.0;
				
				genome2_iter++;
			}
		}
		else
		{
			while (genome2_iter != genome2_max)
			{
				const Mutation *genome2_mutation = *genome2_iter;
				float selection_coeff = genome2_mutation->selection_coeff_;
				
				if (selection_coeff != 0.0f)
				{
					w *= (1.0 + genome2_mutation->mutation_type_ptr_->dominance_coeff_ * selection_coeff);
					
					if (w <= 0.0)
						return 0.0;
				}
				
				genome2_iter++;
			}
		}
		
		return w;
	}
}

void Subpopulation::SwapChildAndParentGenomes(void)
{
	bool will_need_new_children = false;
	
	// If there are any differences between the parent and child genome setups (due to change in subpop size, sex ratio, etc.), we will need to create new child genomes after swapping
	// This is because the parental genomes, which are based on the old parental values, will get swapped in to the children, but they will be out of date.
	if (parent_subpop_size_ != child_subpop_size_ || parent_sex_ratio_ != child_sex_ratio_ || parent_first_male_index_ != child_first_male_index_)
		will_need_new_children = true;
	
	// Execute the genome swap
	child_genomes_.swap(parent_genomes_);
	
	// The parents now have the values that used to belong to the children.
	parent_subpop_size_ = child_subpop_size_;
	parent_sex_ratio_ = child_sex_ratio_;
	parent_first_male_index_ = child_first_male_index_;
	
	// mark the child generation as invalid, until it is generated
	child_generation_valid = false;
	
	// The parental genomes, which have now been swapped into the child genome vactor, no longer fit the bill.  We need to throw them out and generate new genome vectors.
	if (will_need_new_children)
		GenerateChildrenToFit(false);	// false means generate only new children, not new parents
}

//
// SLiMscript support
//
std::string Subpopulation::ElementType(void) const
{
	return "Subpopulation";
}

void Subpopulation::Print(std::ostream &p_ostream) const
{
	p_ostream << ElementType() << "<p" << subpopulation_id_ << ">";
}

std::vector<std::string> Subpopulation::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back("id");								// subpopulation_id_
	constants.push_back("firstMaleIndex");					// parent_first_male_index_ / child_first_male_index_
	constants.push_back("genomes");							// parent_genomes_ / child_genomes_
	constants.push_back("immigrantSubpopIDs");				// migrant_fractions_
	constants.push_back("immigrantSubpopFractions");		// migrant_fractions_
	constants.push_back("selfingFraction");					// selfing_fraction_
	constants.push_back("sexRatio");						// parent_sex_ratio_ / child_sex_ratio_
	constants.push_back("size");							// parent_subpop_size_ / child_subpop_size_
	
	return constants;
}

std::vector<std::string> Subpopulation::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	return variables;
}

ScriptValue *Subpopulation::GetValueForMember(const std::string &p_member_name)
{
	// constants
	if (p_member_name.compare("id") == 0)
		return new ScriptValue_Int(subpopulation_id_);
	if (p_member_name.compare("firstMaleIndex") == 0)
		return new ScriptValue_Int(child_generation_valid ? child_first_male_index_ : parent_first_male_index_);
	if (p_member_name.compare("genomes") == 0)
	{
		ScriptValue_Object *vec = new ScriptValue_Object();
		
		if (child_generation_valid)
			for (auto genome_iter = child_genomes_.begin(); genome_iter != child_genomes_.end(); genome_iter++)
				vec->PushElement(&(*genome_iter));		// operator * can be overloaded by the iterator
		else
			for (auto genome_iter = parent_genomes_.begin(); genome_iter != parent_genomes_.end(); genome_iter++)
				vec->PushElement(&(*genome_iter));		// operator * can be overloaded by the iterator
		
		return vec;
	}
	if (p_member_name.compare("immigrantSubpopIDs") == 0)
	{
		ScriptValue_Int *vec = new ScriptValue_Int();
		
		for (auto migrant_pair = migrant_fractions_.begin(); migrant_pair != migrant_fractions_.end(); ++migrant_pair)
			vec->PushInt(migrant_pair->first);
		
		return vec;
	}
	if (p_member_name.compare("immigrantSubpopFractions") == 0)
	{
		ScriptValue_Float *vec = new ScriptValue_Float();
		
		for (auto migrant_pair = migrant_fractions_.begin(); migrant_pair != migrant_fractions_.end(); ++migrant_pair)
			vec->PushFloat(migrant_pair->second);
		
		return vec;
	}
	if (p_member_name.compare("selfingFraction") == 0)
		return new ScriptValue_Float(selfing_fraction_);
	if (p_member_name.compare("sexRatio") == 0)
		return new ScriptValue_Float(child_generation_valid ? child_sex_ratio_ : parent_sex_ratio_);
	if (p_member_name.compare("size") == 0)
		return new ScriptValue_Int(child_generation_valid ? child_subpop_size_ : parent_subpop_size_);
	
	// variables
	
	return ScriptObjectElement::GetValueForMember(p_member_name);
}

void Subpopulation::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	// remove this FIXME – no longer a writeable property
	if (p_member_name.compare("selfingFraction") == 0)
	{
		TypeCheckValue(__func__, p_member_name, p_value, kScriptValueMaskInt | kScriptValueMaskFloat);
		
		double value = p_value->FloatAtIndex(0);
		RangeCheckValue(__func__, p_member_name, (value >= 0.0) && (value <= 1.0));
		
		selfing_fraction_ = (int)value;
		return;
	}
	
	return ScriptObjectElement::SetValueForMember(p_member_name, p_value);
}

std::vector<std::string> Subpopulation::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	methods.push_back("changeMigrationRates");
	methods.push_back("changeSelfingRate");
	methods.push_back("changeSexRatio");
	methods.push_back("changeSubpopulationSize");
	methods.push_back("fitness");
	methods.push_back("outputMSSample");
	methods.push_back("outputSample");
	
	return methods;
}

const FunctionSignature *Subpopulation::SignatureForMethod(std::string const &p_method_name) const
{
	// Signatures are all preallocated, for speed
	static FunctionSignature *changeMigrationRatesSig = nullptr;
	static FunctionSignature *changeSelfingRateSig = nullptr;
	static FunctionSignature *changeSexRatioSig = nullptr;
	static FunctionSignature *changeSubpopulationSizeSig = nullptr;
	static FunctionSignature *fitnessSig = nullptr;
	static FunctionSignature *outputMSSampleSig = nullptr;
	static FunctionSignature *outputSampleSig = nullptr;
	
	if (!changeMigrationRatesSig)
	{
		changeMigrationRatesSig = (new FunctionSignature("changeMigrationRates", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddObject()->AddNumeric();
		changeSelfingRateSig = (new FunctionSignature("changeSelfingRate", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddNumeric_S();
		changeSexRatioSig = (new FunctionSignature("changeSexRatio", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddFloat_S();
		changeSubpopulationSizeSig = (new FunctionSignature("changeSubpopulationSize", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddInt_S();
		fitnessSig = (new FunctionSignature("fitness", FunctionIdentifier::kNoFunction, kScriptValueMaskFloat))->SetInstanceMethod()->AddInt();
		outputMSSampleSig = (new FunctionSignature("outputMSSample", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddInt_S()->AddString_OS();
		outputSampleSig = (new FunctionSignature("outputSample", FunctionIdentifier::kNoFunction, kScriptValueMaskNULL))->SetInstanceMethod()->AddInt_S()->AddString_OS();
	}
	
	if (p_method_name.compare("changeMigrationRates") == 0)
		return changeMigrationRatesSig;
	else if (p_method_name.compare("changeSelfingRate") == 0)
		return changeSelfingRateSig;
	else if (p_method_name.compare("changeSexRatio") == 0)
		return changeSexRatioSig;
	else if (p_method_name.compare("changeSubpopulationSize") == 0)
		return changeSubpopulationSizeSig;
	else if (p_method_name.compare("fitness") == 0)
		return fitnessSig;
	else if (p_method_name.compare("outputMSSample") == 0)
		return outputMSSampleSig;
	else if (p_method_name.compare("outputSample") == 0)
		return outputSampleSig;
	else
		return ScriptObjectElement::SignatureForMethod(p_method_name);
}

ScriptValue *Subpopulation::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	int num_arguments = (int)p_arguments.size();
	ScriptValue *arg0_value = ((num_arguments >= 1) ? p_arguments[0] : nullptr);
	ScriptValue *arg1_value = ((num_arguments >= 2) ? p_arguments[1] : nullptr);
	
	
	//
	//	*********************	- (void)changeMigrationRates(object sourceSubpops, numeric rates)
	//
#pragma mark -changeMigrationRates()
	
	if (p_method_name.compare("changeMigrationRates") == 0)
	{
		int source_subpops_count = arg0_value->Count();
		int rates_count = arg1_value->Count();
		
		if (source_subpops_count != rates_count)
			SLIM_TERMINATION << "ERROR (Subpopulation::ExecuteMethod): changeMigrationRates() requires sourceSubpops and rates to be equal in size." << slim_terminate();
		if (((ScriptValue_Object *)arg0_value)->ElementType().compare("Subpopulation") != 0)
			SLIM_TERMINATION << "ERROR (Subpopulation::ExecuteMethod): changeMigrationRates() requires sourceSubpops to be a Subpopulation object." << slim_terminate();
		
		for (int value_index = 0; value_index < source_subpops_count; ++value_index)
		{
			ScriptObjectElement *source_subpop = arg0_value->ElementAtIndex(value_index);
			int source_subpop_id = ((Subpopulation *)(source_subpop))->subpopulation_id_;
			double migrant_fraction = arg1_value->FloatAtIndex(value_index);
			
			population_.SetMigration(subpopulation_id_, source_subpop_id, migrant_fraction);
		}
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	
	
	//
	//	*********************	- (void)changeSelfingRate(numeric$ rate)
	//
#pragma mark -changeSelfingRate()
	
	else if (p_method_name.compare("changeSelfingRate") == 0)
	{
		double selfing_fraction = arg0_value->FloatAtIndex(0);
		
		population_.SetSelfing(subpopulation_id_, selfing_fraction);
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	
	
	//
	//	*********************	- (void)changeSexRatio(float$ sexRatio)
	//
#pragma mark -changeSexRatio()
	
	else if (p_method_name.compare("changeSexRatio") == 0)
	{
		double sex_ratio = arg0_value->FloatAtIndex(0);
		
		population_.SetSexRatio(subpopulation_id_, sex_ratio);
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	
	
	//
	//	*********************	- (void)changeSubpopulationSize(integer$ size)
	//
#pragma mark -changeSubpopulationSize()
	
	else if (p_method_name.compare("changeSubpopulationSize") == 0)
	{
		int subpop_size = (int)arg0_value->IntAtIndex(0);
		
		population_.SetSize(subpopulation_id_, subpop_size);
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	
	
	//
	//	*********************	- (float)fitness(integer indices)
	//
#pragma mark -fitness()
	
	else if (p_method_name.compare("fitness") == 0)
	{
		if (child_generation_valid)
			SLIM_TERMINATION << "ERROR (Subpopulation::ExecuteMethod): fitness() may only be called when the parental generation is active (before offspring generation)." << slim_terminate();
		
		// We fetch callbacks from the sim; we use the callbacks from the current stage, for our subpopulation ID
		// We do not comprise our own generation stage, so we do not deregister blocks when we are done
		SLiMSim &sim = population_.sim_;
		std::vector<SLiMScriptBlock*> fitness_callbacks = sim.ScriptBlocksMatching(sim.Generation(), SLiMScriptBlockType::SLiMScriptFitnessCallback, -1, subpopulation_id_);
		
		bool do_all_indices = (arg0_value->Type() == ScriptValueType::kValueNULL);
		int index_count = (do_all_indices ? parent_subpop_size_ : arg0_value->Count());
		ScriptValue_Float *float_return = new ScriptValue_Float();
		
		for (int value_index = 0; value_index < index_count; value_index++)
		{
			int index = (do_all_indices ? value_index : (int)arg0_value->IntAtIndex(value_index));
			double fitness = FitnessOfParentWithGenomeIndices(2 * index, 2 * index + 1, fitness_callbacks);
			
			float_return->PushFloat(fitness);
		}
		
		return float_return;
	}
	
	
	//
	//	*********************	- (void)outputMSSample(integer$ sampleSize, [string$ requestedSex])
	//	*********************	- (void)outputSample(integer$ sampleSize, [string$ requestedSex])
	//
#pragma mark -outputMSSample()
#pragma mark -outputSample()
	
	else if ((p_method_name.compare("outputMSSample") == 0) || (p_method_name.compare("outputSample") == 0))
	{
		int sample_size = (int)arg0_value->IntAtIndex(0);
		IndividualSex requested_sex = IndividualSex::kUnspecified;
		
		if (num_arguments == 2)
		{
			string sex_string = arg1_value->StringAtIndex(0);
			
			if (sex_string.compare("M") == 0)
				requested_sex = IndividualSex::kMale;
			else if (sex_string.compare("F") == 0)
				requested_sex = IndividualSex::kFemale;
			else if (sex_string.compare("*") == 0)
				requested_sex = IndividualSex::kUnspecified;
		}
		
		SLiMSim &sim = population_.sim_;
		
		SLIM_OUTSTREAM << "#OUT: " << sim.Generation() << " R p" << subpopulation_id_ << " " << sample_size;
		
		if (sim.SexEnabled())
			SLIM_OUTSTREAM << " " << requested_sex;
		
		SLIM_OUTSTREAM << endl;
		
		if (p_method_name.compare("outputSample") == 0)
			population_.PrintSample(subpopulation_id_, sample_size, requested_sex);
		else
			population_.PrintSample_ms(subpopulation_id_, sample_size, sim.Chromosome(), requested_sex);
		
		return ScriptValue_NULL::ScriptValue_NULL_Invisible();
	}
	
	
	else
		return ScriptObjectElement::ExecuteMethod(p_method_name, p_arguments, p_output_stream, p_interpreter);
}




































































