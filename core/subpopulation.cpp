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
			SLIM_TERMINATION << "ERROR (GenerateChildrenToFit): child sex ratio of " << child_sex_ratio_ << " produced no females" << std::endl << slim_terminate();
		else if (child_first_male_index_ >= child_subpop_size_)
			SLIM_TERMINATION << "ERROR (GenerateChildrenToFit): child sex ratio of " << child_sex_ratio_ << " produced no males" << std::endl << slim_terminate();
		
		if (p_parents_also)
		{
			parent_first_male_index_ = static_cast<int>(lround((1.0 - parent_sex_ratio_) * parent_subpop_size_));
			
			if (parent_first_male_index_ <= 0)
				SLIM_TERMINATION << "ERROR (GenerateChildrenToFit): parent sex ratio of " << parent_sex_ratio_ << " produced no females" << std::endl << slim_terminate();
			else if (parent_first_male_index_ >= parent_subpop_size_)
				SLIM_TERMINATION << "ERROR (GenerateChildrenToFit): parent sex ratio of " << parent_sex_ratio_ << " produced no males" << std::endl << slim_terminate();
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

Subpopulation::Subpopulation(int p_subpop_size) : parent_subpop_size_(p_subpop_size), child_subpop_size_(p_subpop_size)
{
	GenerateChildrenToFit(true);
	
	// Set up to draw random individuals, based initially on equal fitnesses
	double A[parent_subpop_size_];
	
	for (int i = 0; i < parent_subpop_size_; i++)
		A[i] = 1.0;
	
	lookup_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_, A);
}

// SEX ONLY
Subpopulation::Subpopulation(int p_subpop_size, double p_sex_ratio, GenomeType p_modeled_chromosome_type, double p_x_chromosome_dominance_coeff) :
	sex_enabled_(true), parent_subpop_size_(p_subpop_size), child_subpop_size_(p_subpop_size), parent_sex_ratio_(p_sex_ratio), child_sex_ratio_(p_sex_ratio), modeled_chromosome_type_(p_modeled_chromosome_type), x_chromosome_dominance_coeff_(p_x_chromosome_dominance_coeff)
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

void Subpopulation::UpdateFitness()
{
#ifdef SLIMGUI
	// When running under SLiMgui, this function calculates the population mean fitness as a side effect
	double totalFitness = 0.0;
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
			double fitness = FitnessOfParentWithGenomeIndices(2 * i, 2 * i + 1);
			
			A[i] = fitness;
#ifdef SLIMGUI
			totalFitness += fitness;
#endif
		}
		
		lookup_female_parent_ = gsl_ran_discrete_preproc(parent_first_male_index_, A);
		
		// Set up to draw random males
		int num_males = parent_subpop_size_ - parent_first_male_index_;
		double B[num_males];
		
		for (int i = 0; i < num_males; i++)
		{
			double fitness = FitnessOfParentWithGenomeIndices(2 * (i + parent_first_male_index_), 2 * (i + parent_first_male_index_) + 1);
			
			B[i] = fitness;
#ifdef SLIMGUI
			totalFitness += fitness;
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
			double fitness = FitnessOfParentWithGenomeIndices(2 * i, 2 * i + 1);
			
			A[i] = fitness;
#ifdef SLIMGUI
			totalFitness += fitness;
#endif
		}
		
		lookup_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_, A);
	}
	
#ifdef SLIMGUI
	parental_total_fitness_ = totalFitness;
#endif
}

double Subpopulation::FitnessOfParentWithGenomeIndices(int p_genome_index1, int p_genome_index2) const
{
	// calculate the fitness of the individual constituted by genome1 and genome2 in the parent population
	double w = 1.0;
	
	const Genome *genome1 = &(parent_genomes_[p_genome_index1]);
	const Genome *genome2 = &(parent_genomes_[p_genome_index2]);
	
	if (genome1->IsNull() && genome2->IsNull())
	{
		// SEX ONLY: both genomes are placeholders; for example, we might be simulating the Y chromosome, and this is a female
		return w;
	}
	else if (genome1->IsNull() || genome2->IsNull())
	{
		// SEX ONLY: one genome is null, so we just need to scan through the modeled genome and account for its mutations, including the x-dominance coefficient
		const Genome *genome = genome1->IsNull() ? genome2 : genome1;
		SLIMCONST Mutation **genome_iter = genome->begin_pointer();
		SLIMCONST Mutation **genome_max = genome->end_pointer();
		
		if (genome->GenomeType() == GenomeType::kXChromosome)
		{
			// with an unpaired X chromosome, we need to multiply each selection coefficient by the X chromosome dominance coefficient
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
		else
		{
			// with other types of unpaired chromosomes (like the Y chromosome of a male when we are modeling the Y) there is no dominance coefficient
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
		
		return w;
	}
	else
	{
		// both genomes are being modeled, so we need to scan through and figure out which mutations are heterozygous and which are homozygous
		SLIMCONST Mutation **genome1_iter = genome1->begin_pointer();
		SLIMCONST Mutation **genome2_iter = genome2->begin_pointer();
		
		SLIMCONST Mutation **genome1_max = genome1->end_pointer();
		SLIMCONST Mutation **genome2_max = genome2->end_pointer();
		
		// first, handle the situation before either genome iterator has reached the end of its genome, for simplicity/speed
		if (genome1_iter != genome1_max && genome2_iter != genome2_max)
		{
			const Mutation *genome1_mutation = *genome1_iter, *genome2_mutation = *genome2_iter;
			int genome1_iter_position = genome1_mutation->position_, genome2_iter_position = genome2_mutation->position_;
			
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
					SLIMCONST Mutation **genome1_start = genome1_iter;
					
					// advance through genome1 as long as we remain at the same position, handling one mutation at a time
					do
					{
						float selection_coeff = genome1_mutation->selection_coeff_;
						
						if (selection_coeff != 0.0f)
						{
							SLIMCONST MutationType *mutation_type_ptr = genome1_mutation->mutation_type_ptr_;
							SLIMCONST Mutation **genome2_matchscan = genome2_iter; 
							bool homozygous = false;
							
							// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
							while (genome2_matchscan != genome2_max && (*genome2_matchscan)->position_ == position)
							{
								if (mutation_type_ptr == (*genome2_matchscan)->mutation_type_ptr_ && selection_coeff == (*genome2_matchscan)->selection_coeff_) 
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
							SLIMCONST MutationType *mutation_type_ptr = genome2_mutation->mutation_type_ptr_;
							SLIMCONST Mutation **genome1_matchscan = genome1_start; 
							bool homozygous = false;
							
							// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
							while (genome1_matchscan != genome1_max && (*genome1_matchscan)->position_ == position)
							{
								if (mutation_type_ptr == (*genome1_matchscan)->mutation_type_ptr_ && selection_coeff == (*genome1_matchscan)->selection_coeff_)
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
		
		// one or the other genome has now reached its end, so now we just need to handle the remaining mutations in the unfinished genome
		assert(!(genome1_iter != genome1_max && genome2_iter != genome2_max));
		
		// if genome1 is unfinished, finish it
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
		
		// if genome2 is unfinished, finish it
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
	
	// The parental genomes, which have now been swapped into the child genome vactor, no longer fit the bill.  We need to throw them out and generate new genome vectors.
	if (will_need_new_children)
		GenerateChildrenToFit(false);	// false means generate only new children, not new parents
}

#ifndef SLIMCORE
//
// SLiMscript support
//
std::string Subpopulation::ElementType(void) const
{
	return "Subpopulation";
}

std::vector<std::string> Subpopulation::ReadOnlyMembers(void) const
{
	std::vector<std::string> constants = ScriptObjectElement::ReadOnlyMembers();
	
	constants.push_back("immigrantSubpopIDs");				// migrant_fractions_
	constants.push_back("immigrantSubpopFractions");		// migrant_fractions_
	constants.push_back("sexRatio");						// child_sex_ratio_
	constants.push_back("size");							// child_subpop_size_
	
	return constants;
}

std::vector<std::string> Subpopulation::ReadWriteMembers(void) const
{
	std::vector<std::string> variables = ScriptObjectElement::ReadWriteMembers();
	
	variables.push_back("selfingFraction");					// selfing_fraction_
	
	return variables;
}

ScriptValue *Subpopulation::GetValueForMember(const std::string &p_member_name)
{
	// constants
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
	if (p_member_name.compare("sexRatio") == 0)
		return new ScriptValue_Float(child_sex_ratio_);
	if (p_member_name.compare("size") == 0)
		return new ScriptValue_Int(child_subpop_size_);
	
	// variables
	if (p_member_name.compare("selfingFraction") == 0)
		return new ScriptValue_Float(selfing_fraction_);
	
	return ScriptObjectElement::GetValueForMember(p_member_name);
}

void Subpopulation::SetValueForMember(const std::string &p_member_name, ScriptValue *p_value)
{
	if (p_member_name.compare("selfingFraction") == 0)
	{
		TypeCheckValue(__func__, p_member_name, p_value, kScriptValueMaskInt | kScriptValueMaskFloat);
		
		double value = p_value->FloatAtIndex(0);
		RangeCheckValue(__func__, p_member_name, (value >= 0.0) && (value <= 1.0));
		
		selfing_fraction_ = (int)value;
		return;
	}
	
	// Check for constants that the user should not try to set
	if ((p_member_name.compare("immigrantSubpopIDs") == 0) ||
		(p_member_name.compare("sexRatio") == 0) ||
		(p_member_name.compare("size") == 0) ||
		(p_member_name.compare("immigrantSubpopFractions") == 0))
		ConstantSetError(__func__, p_member_name);
	
	return ScriptObjectElement::SetValueForMember(p_member_name, p_value);
}

std::vector<std::string> Subpopulation::Methods(void) const
{
	std::vector<std::string> methods = ScriptObjectElement::Methods();
	
	// setMigration(subpopIDs, fractions)
	// sexSexRatio(ration)
	// setSize(size)
	
	return methods;
}

const FunctionSignature *Subpopulation::SignatureForMethod(std::string const &p_method_name) const
{
	return ScriptObjectElement::SignatureForMethod(p_method_name);
}

ScriptValue *Subpopulation::ExecuteMethod(std::string const &p_method_name, std::vector<ScriptValue*> const &p_arguments, std::ostream &p_output_stream, ScriptInterpreter &p_interpreter)
{
	return ScriptObjectElement::ExecuteMethod(p_method_name, p_arguments, p_output_stream, p_interpreter);
}

/*
	std::vector<Genome> child_genomes_;				// all genomes in the child generation; each individual gets two genomes, males are XY (not YX)
*/

#endif	// #ifndef SLIMCORE


































































