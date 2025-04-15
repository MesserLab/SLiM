//
//  subpopulation.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2025 Philipp Messer.  All rights reserved.
//	A product of the Messer Lab, http://messerlab.org/slim/
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


#include "subpopulation.h"
#include "community.h"
#include "species.h"
#include "slim_globals.h"
#include "population.h"
#include "interaction_type.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "eidos_ast_node.h"
#include "eidos_globals.h"
#include "eidos_class_Image.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <assert.h>
#include <string>
#include <map>
#include <utility>
#include <cmath>


#pragma mark -
#pragma mark Subpopulation
#pragma mark -

// WF only:
void Subpopulation::WipeIndividualsAndHaplosomes(std::vector<Individual *> &p_individuals, slim_popsize_t p_individual_count, slim_popsize_t p_first_male)
{
	if (!species_.HasGenetics())
	{
		// With no genetics, most of the work here is unnecessary, but we do need to fix the sex of the individuals
		if (p_first_male >= 0)
		{
			for (int index = 0; index < p_individual_count; ++index)
			{
				Individual *individual = p_individuals[index];
				bool is_female = (index < p_first_male);
				
				individual->sex_ = (is_female ? IndividualSex::kFemale : IndividualSex::kMale);
			}
		}
		return;
	}
	
	// The logic here is similar to GenerateIndividualEmpty(), but instead of making a new individual,
	// we are recycling an existing individual.
	const std::vector<Chromosome *> &chromosomes = species_.Chromosomes();
	
	if (p_first_male == -1)
	{
		// make hermaphrodites
		for (int index = 0; index < p_individual_count; ++index)
		{
			Individual *individual = p_individuals[index];
			Haplosome **haplosomes = individual->haplosomes_;
			int haplosome_index = 0;
			
			for (Chromosome *chromosome : chromosomes)
			{
				// Determine what kind of haplosomes to make for this chromosome
				ChromosomeType chromosomeType = chromosome->Type();
				
				switch (chromosomeType)
				{
					case ChromosomeType::kA_DiploidAutosome:
					{
						// non-null for all
						haplosomes[haplosome_index]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						haplosomes[haplosome_index+1]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						haplosome_index += 2;
						break;
					}
					case ChromosomeType::kH_HaploidAutosome:
					{
						// non-null for all
						haplosomes[haplosome_index]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						haplosome_index += 1;
						break;
					}
					case ChromosomeType::kHNull_HaploidAutosomeWithNull:
					{
						// non-null + null for all
						haplosomes[haplosome_index]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						haplosomes[haplosome_index+1]->ReinitializeHaplosomeToNull(individual);
						haplosome_index += 2;
						break;
					}
					case ChromosomeType::kX_XSexChromosome:
					case ChromosomeType::kY_YSexChromosome:
					case ChromosomeType::kZ_ZSexChromosome:
					case ChromosomeType::kW_WSexChromosome:
					case ChromosomeType::kHF_HaploidFemaleInherited:
					case ChromosomeType::kFL_HaploidFemaleLine:
					case ChromosomeType::kHM_HaploidMaleInherited:
					case ChromosomeType::kML_HaploidMaleLine:
					case ChromosomeType::kNullY_YSexChromosomeWithNull:
						EIDOS_TERMINATION << "ERROR (Subpopulation::WipeIndividualsAndHaplosomes): (internal error) chromosome type is illegal in non-sexual simulations." << EidosTerminate();
				}
			}
		}
	}
	else
	{
		// make females and males
		for (int index = 0; index < p_individual_count; ++index)
		{
			Individual *individual = p_individuals[index];
			Haplosome **haplosomes = individual->haplosomes_;
			int haplosome_index = 0;
			bool is_female = (index < p_first_male);
			
			individual->sex_ = (is_female ? IndividualSex::kFemale : IndividualSex::kMale);
		
			for (Chromosome *chromosome : chromosomes)
			{
				// Determine what kind of haplosomes to make for this chromosome
				ChromosomeType chromosomeType = chromosome->Type();
				
				switch (chromosomeType)
				{
					case ChromosomeType::kA_DiploidAutosome:
					{
						// non-null for all
						haplosomes[haplosome_index]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						haplosomes[haplosome_index+1]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						haplosome_index += 2;
						break;
					}
					case ChromosomeType::kHF_HaploidFemaleInherited:
					case ChromosomeType::kHM_HaploidMaleInherited:
					case ChromosomeType::kH_HaploidAutosome:
					{
						// non-null for all
						haplosomes[haplosome_index]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						haplosome_index += 1;
						break;
					}
					case ChromosomeType::kX_XSexChromosome:
					{
						// XX for females, X- for males
						haplosomes[haplosome_index]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						
						if (is_female)
							haplosomes[haplosome_index+1]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						else
							haplosomes[haplosome_index+1]->ReinitializeHaplosomeToNull(individual);
						haplosome_index += 2;
						break;
					}
					case ChromosomeType::kY_YSexChromosome:
					case ChromosomeType::kML_HaploidMaleLine:
					{
						// - for females, Y for males
						if (is_female)
							haplosomes[haplosome_index]->ReinitializeHaplosomeToNull(individual);
						else
							haplosomes[haplosome_index]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						haplosome_index += 1;
						break;
					}
					case ChromosomeType::kZ_ZSexChromosome:
					{
						// ZZ for males, -Z for females
						if (is_female)
							haplosomes[haplosome_index]->ReinitializeHaplosomeToNull(individual);
						else
							haplosomes[haplosome_index]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						
						haplosomes[haplosome_index+1]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						haplosome_index += 2;
						break;
					}
					case ChromosomeType::kW_WSexChromosome:
					case ChromosomeType::kFL_HaploidFemaleLine:
					{
						// - for males, W for females
						if (is_female)
							haplosomes[haplosome_index]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						else
							haplosomes[haplosome_index]->ReinitializeHaplosomeToNull(individual);
						haplosome_index += 1;
						break;
					}
					case ChromosomeType::kHNull_HaploidAutosomeWithNull:
					{
						// non-null + null for all
						haplosomes[haplosome_index]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						haplosomes[haplosome_index+1]->ReinitializeHaplosomeToNull(individual);
						haplosome_index += 2;
						break;
					}
					case ChromosomeType::kNullY_YSexChromosomeWithNull:
					{
						// -- for females, -Y for males
						haplosomes[haplosome_index]->ReinitializeHaplosomeToNull(individual);
						
						if (is_female)
							haplosomes[haplosome_index+1]->ReinitializeHaplosomeToNull(individual);
						else
							haplosomes[haplosome_index+1]->ReinitializeHaplosomeToNonNull(individual, chromosome);
						
						haplosome_index += 2;
						break;
					}
				}
			}
		}
	}
}

// Reconfigure the child generation to match the set size, sex ratio, etc.  This may involve removing existing individuals,
// or adding new ones.  It may also involve transmogrifying existing individuals to a new sex, etc.  It can also transmogrify
// haplosomes between a null and non-null state, as a side effect of changing sex.  So this code is really gross and invasive.
void Subpopulation::GenerateChildrenToFitWF()
{
	// First, make the number of Individual objects match, and make the corresponding Haplosome changes
	int old_individual_count = (int)child_individuals_.size();
	int new_individual_count = child_subpop_size_;
	
	if (new_individual_count > old_individual_count)
	{
		// We also have to make space for the pointers to the haplosomes and individuals
		child_individuals_.reserve(new_individual_count);
		
		if (species_.HasGenetics())
		{
			const std::vector<Chromosome *> &chromosome_for_haplosome_index = species_.ChromosomesForHaplosomeIndices();
			const std::vector<uint8_t> &chromosome_subindices = species_.ChromosomeSubindicesForHaplosomeIndices();
			int haplosome_count_per_individual = HaplosomeCountPerIndividual();
			
			// We just allocate null haplosomes here for all chromosomes; there is no mutrun buffer allocation
			// overhead for that.  WipeIndividualsAndHaplosomes(), called below, will fix them to be the correct
			// type as needed.  There is a little extra overhead for that, presumably, but it only matters when
			// the population size (or perhaps sex ratio, etc.) changes.
			if ((haplosome_count_per_individual == 2) && (species_.Chromosomes().size() == 1))
			{
				Chromosome *chromosome = chromosome_for_haplosome_index[0];
				
				// special-case the simple diploid case to avoid the loop
				for (int new_index = old_individual_count; new_index < new_individual_count; ++new_index)
				{
					// allocate out of our junkyard and object pool
					Individual *individual = NewSubpopIndividual(new_index, IndividualSex::kHermaphrodite, -1, /* initial fitness for new subpops */ 1.0, /* p_mean_parent_age */ -1.0F);
					
					individual->AddHaplosomeAtIndex(chromosome->NewHaplosome_NULL(individual, 0), 0);
					individual->AddHaplosomeAtIndex(chromosome->NewHaplosome_NULL(individual, 1), 1);
					
					child_individuals_.emplace_back(individual);
				}
			}
			else
			{
				for (int new_index = old_individual_count; new_index < new_individual_count; ++new_index)
				{
					// allocate out of our junkyard and object pool
					Individual *individual = NewSubpopIndividual(new_index, IndividualSex::kHermaphrodite, -1, /* initial fitness for new subpops */ 1.0, /* p_mean_parent_age */ -1.0F);
					
					for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
					{
						Chromosome *chromosome = chromosome_for_haplosome_index[haplosome_index];
						uint8_t chromosome_subindex = chromosome_subindices[haplosome_index];
						Haplosome *haplosome = chromosome->NewHaplosome_NULL(individual, chromosome_subindex);
						
						individual->AddHaplosomeAtIndex(haplosome, haplosome_index);
					}
					
					child_individuals_.emplace_back(individual);
				}
			}
		}
		else
		{
			// In the no-genetics case no haplosomes are needed, so it is very simple
			for (int new_index = old_individual_count; new_index < new_individual_count; ++new_index)
			{
				Individual *individual = NewSubpopIndividual(new_index, IndividualSex::kHermaphrodite, -1, /* initial fitness for new subpops */ 1.0, /* p_mean_parent_age */ -1.0F);
				
				child_individuals_.emplace_back(individual);
			}
		}
	}
	else if (new_individual_count < old_individual_count)
	{
		for (int old_index = new_individual_count; old_index < old_individual_count; ++old_index)
		{
			Individual *individual = child_individuals_[old_index];
			
			// dispose of the individual
			_FreeSubpopIndividual(individual);
		}
		
		child_individuals_.resize(new_individual_count);
	}
	
	// Next, fix the type of each haplosome, and clear them all, and fix individual sex if necessary
	if (sex_enabled_)
	{
		double sex_ratio = child_sex_ratio_;
		slim_popsize_t &first_male_index = child_first_male_index_;
		
		slim_popsize_t total_males = static_cast<slim_popsize_t>(lround(sex_ratio * new_individual_count));	// round in favor of males, arbitrarily
		
		first_male_index = new_individual_count - total_males;
		
		if (first_male_index <= 0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFitWF): sex ratio of " << sex_ratio << " produced no females." << EidosTerminate();
		else if (first_male_index >= child_subpop_size_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFitWF): sex ratio of " << sex_ratio << " produced no males." << EidosTerminate();
		
		WipeIndividualsAndHaplosomes(child_individuals_, new_individual_count, first_male_index);
	}
	else
	{
		WipeIndividualsAndHaplosomes(child_individuals_, new_individual_count, -1);	// make hermaphrodites
	}
}

// Generate new individuals to fill out a freshly created subpopulation, including recording in the tree
// sequence unless this is the result of addSubpopSplit() (which does its own recording since parents are
// involved in that case).  This handles both the WF and nonWF cases, which are very similar.
void Subpopulation::GenerateParentsToFit(slim_age_t p_initial_age, double p_sex_ratio, bool p_allow_zero_size, bool p_require_both_sexes, bool p_record_in_treeseq, bool p_haploid, float p_mean_parent_age)
{
	bool recording_tree_sequence = p_record_in_treeseq && species_.RecordingTreeSequence();
	
	cached_parent_individuals_value_.reset();
	
	if (parent_individuals_.size())
		EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateParentsToFit): (internal error) individuals already present in GenerateParentsToFit()." << EidosTerminate();
	if ((parent_subpop_size_ == 0) && !p_allow_zero_size)
		EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateParentsToFit): (internal error) subpop size of 0 requested." << EidosTerminate();
	
	// Make space for the pointers to the haplosomes and individuals
	parent_individuals_.reserve(parent_subpop_size_);
	
	// Now create new individuals and haplosomes appropriate for the requested sex ratio and subpop size
	// MULTICHROM FIXME there was code here to make shared empty mutation runs, reused across all of the new individuals.  I'm not sure
	// how big a deal this is.  It will probably only affect the time to generate the first generation; I think it wouldn't matter after
	// that at all.  Anyhow, it doesn't presently fit into the new scheme, so I'm removing it for now.  Perhaps GenerateIndividualEmpty()
	// could do something smart here in future?  How about: the Chromosome keeps a single empty mutrun object that is shared across all
	// uses, and users of it are never allowed to modify it, but must instead make a new mutrun if they need to modify it?
	if (sex_enabled_)
	{
		slim_popsize_t total_males = static_cast<slim_popsize_t>(lround(p_sex_ratio * parent_subpop_size_));	// round in favor of males, arbitrarily
		slim_popsize_t first_male_index = parent_subpop_size_ - total_males;
		
		parent_first_male_index_ = first_male_index;
		
		if (p_require_both_sexes)
		{
			if (first_male_index <= 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateParentsToFit): sex ratio of " << p_sex_ratio << " produced no females." << EidosTerminate();
			else if (first_male_index >= parent_subpop_size_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateParentsToFit): sex ratio of " << p_sex_ratio << " produced no males." << EidosTerminate();
		}
		
		// Make females and then males
		for (int new_index = 0; new_index < parent_subpop_size_; ++new_index)
		{
			IndividualSex child_sex = (new_index < first_male_index) ? IndividualSex::kFemale : IndividualSex::kMale;
			Individual *ind = GenerateIndividualEmpty(/* index */ new_index,
													  /* sex */ child_sex,
													  /* age */ p_initial_age,
													  /* fitness */ 1.0,
													  p_mean_parent_age,
													  /* haplosome1_null */ false,
													  /* haplosome2_null */ p_haploid,
													  /* run_modify_child */ false,
													  recording_tree_sequence);
			
			parent_individuals_.emplace_back(ind);
		}
	}
	else
	{
		// Make hermaphrodites
		for (int new_index = 0; new_index < parent_subpop_size_; ++new_index)
		{
			IndividualSex child_sex = IndividualSex::kHermaphrodite;
			Individual *ind = GenerateIndividualEmpty(/* index */ new_index,
													  /* sex */ child_sex,
													  /* age */ p_initial_age,
													  /* fitness */ 1.0,
													  p_mean_parent_age,
													  /* haplosome1_null */ false,
													  /* haplosome2_null */ p_haploid,
													  /* run_modify_child */ false,
													  recording_tree_sequence);
			
			parent_individuals_.emplace_back(ind);
		}
	}
}

void Subpopulation::CheckIndividualIntegrity(void)
{
	ClearErrorPosition();
	
	if (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosNoBlockType)
		EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) executing block type was not maintained correctly." << EidosTerminate();
	
	SLiMModelType model_type = model_type_;
	const std::vector<Chromosome *> &chromosomes = species_.Chromosomes();
	size_t chromosomes_count = chromosomes.size();
	bool has_genetics = species_.HasGenetics();
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	if (!has_genetics && (chromosomes_count != 0))
		EIDOS_TERMINATION << "ERROR (Community::Species_CheckIntegrity): (internal error) chromosome present in no-genetics species." << EidosTerminate();
	
	for (Chromosome *chromosome : chromosomes)
	{
		int32_t mutrun_count = chromosome->mutrun_count_;
		slim_position_t mutrun_length = chromosome->mutrun_length_;
		
		if (has_genetics && ((mutrun_count == 0) || (mutrun_length == 0)))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) species with genetics has mutrun count/length of 0." << EidosTerminate();
		else if (!has_genetics && ((mutrun_count != 0) || (mutrun_length != 0)))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) species with no genetics has non-zero mutrun count/length." << EidosTerminate();
	}
	
	// below we will use this map to check that every mutation run in use is used at only one mutrun index
	robin_hood::unordered_flat_map<const MutationRun *, slim_mutrun_index_t> mutrun_position_map;
	
	//
	//	Check the parental generation; this is essentially the same in WF and nonWF models
	//
	
	if ((int)parent_individuals_.size() != parent_subpop_size_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between parent_subpop_size_ and parent_individuals_.size()." << EidosTerminate();
	
	for (int ind_index = 0; ind_index < parent_subpop_size_; ++ind_index)
	{
		Individual *individual = parent_individuals_[ind_index];
		bool invalid_age = false;
		
		if (!individual)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for individual." << EidosTerminate();
		
		if (individual->index_ != ind_index)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->index_ and ind_index." << EidosTerminate();
	
		if (individual->subpopulation_ != this)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->subpopulation_ and subpopulation." << EidosTerminate();
		
		if (species_.PedigreesEnabled())
		{
			if (individual->pedigree_id_ == -1)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) individual has an invalid pedigree ID." << EidosTerminate();
		}
		
		if (model_type == SLiMModelType::kModelTypeWF)
		{
			if (individual->age_ != -1)
				invalid_age = true;
		}
		else
		{
			if (individual->age_ < 0)
				invalid_age = true;
		}
		
		if (invalid_age)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) invalid value for individual->age_." << EidosTerminate();
		
		bool is_female = (ind_index < parent_first_male_index_);	// only used below in sexual simulations
		
		if (sex_enabled_)
		{
			if ((is_female && (individual->sex_ != IndividualSex::kFemale)) ||
				(!is_female && (individual->sex_ != IndividualSex::kMale)))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->sex_ and parent_first_male_index_." << EidosTerminate();
		}
		else
		{
			if (individual->sex_ != IndividualSex::kHermaphrodite)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) non-hermaphrodite individual in non-sexual simulation." << EidosTerminate();
		}
		
		// check that we agree with the species on the haplosome count per individual
		// (we can't check the individual's haplosome count because it simply uses haplosome_count_per_individual_; it has no count of its own!)
		if (haplosome_count_per_individual_ != species_.HaplosomeCountPerIndividual())
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome count is incorrect." << EidosTerminate();
		
		// loop over chromosomes one by one and check the haplosomes of each chromosome
		int haplosome_index = 0;
		
		for (size_t chromosome_index = 0; chromosome_index < chromosomes_count; chromosome_index++)
		{
			Chromosome *chromosome = chromosomes[chromosome_index];
			ChromosomeType chromosome_type = chromosome->Type();
			
			// check haplosome indices
			int haplosome_count = chromosome->IntrinsicPloidy();
			
			// check haplosome 1 for this chromosome
			{
				Haplosome *haplosome1 = individual->haplosomes_[haplosome_index];
				
				if (!haplosome1)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for haplosome1." << EidosTerminate();
				if (haplosome1->individual_ != individual)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between haplosome1->individual_ and individual." << EidosTerminate();
				if (haplosome1->AssociatedChromosome() != chromosome)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome1->AssociatedChromosome() mismatch." << EidosTerminate();
				
				if (!haplosome1->IsNull())
				{
					slim_position_t mutrun_count = chromosome->mutrun_count_;
					slim_position_t mutrun_length = chromosome->mutrun_length_;
					
					if ((haplosome1->mutrun_count_ != mutrun_count) || (haplosome1->mutrun_length_ != mutrun_length))
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome1 of individual has the wrong mutrun count/length." << EidosTerminate();
					
					// check that every mutation in the haplosome belongs to the right chromosome
					for (int run_index = 0; run_index < mutrun_count; ++run_index)
					{
						const MutationRun *mutrun = haplosome1->mutruns_[run_index];
						const MutationIndex *mutrun_iter = mutrun->begin_pointer_const();
						const MutationIndex *mutrun_end_iter = mutrun->end_pointer_const();
						
						for (; mutrun_iter != mutrun_end_iter; ++mutrun_iter)
						{
							Mutation *mut = (mut_block_ptr + *mutrun_iter);
							
							if (mut->chromosome_index_ != haplosome1->chromosome_index_)
								EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mutation with chromosome_index_ " << (unsigned int)mut->chromosome_index_ << " is present in haplosome with chromosome_index_ " << (unsigned int)haplosome1->chromosome_index_ << "." << EidosTerminate();
						}
					}
				}
				
				if (((haplosome1->mutrun_count_ == 0) && ((haplosome1->mutrun_length_ != 0) || (haplosome1->mutruns_ != nullptr))) ||
					((haplosome1->mutrun_length_ == 0) && ((haplosome1->mutrun_count_ != 0) || (haplosome1->mutruns_ != nullptr))))
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome1 mutrun count/length/pointer inconsistency." << EidosTerminate();
				
				if (species_.PedigreesEnabled())
				{
					if (haplosome1->haplosome_id_ != individual->pedigree_id_ * 2)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome1 has an invalid haplosome pedigree ID." << EidosTerminate();
				}
				
				bool null_problem = false;
				
				switch (chromosome_type)
				{
						// haplosome1 should be null for these types
					case ChromosomeType::kNullY_YSexChromosomeWithNull:
						if (!haplosome1->IsNull())
							null_problem = true;
						break;
						
						// haplosome1 should be non-null for these types
					case ChromosomeType::kX_XSexChromosome:
					case ChromosomeType::kHF_HaploidFemaleInherited:
					case ChromosomeType::kHM_HaploidMaleInherited:
						if (haplosome1->IsNull())
							null_problem = true;
						break;
						
						// haplosome1 should be null in females, non-null in males
					case ChromosomeType::kY_YSexChromosome:
					case ChromosomeType::kZ_ZSexChromosome:
					case ChromosomeType::kML_HaploidMaleLine:
						if (is_female != haplosome1->IsNull())
							null_problem = true;
						break;
						
						// haplosome1 should be null in males, non-null in females
					case ChromosomeType::kW_WSexChromosome:
					case ChromosomeType::kFL_HaploidFemaleLine:
						if (is_female == haplosome1->IsNull())
							null_problem = true;
						break;
						
						// we allow either for these types (to allow haplodiploidy, alternation of generations, etc.)
					case ChromosomeType::kA_DiploidAutosome:
					case ChromosomeType::kH_HaploidAutosome:
					case ChromosomeType::kHNull_HaploidAutosomeWithNull:
						break;
				}
				
				if (null_problem)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome1 mismatch between expected and actual null haplosome status." << EidosTerminate();
				
				if (population_.child_generation_valid_)
				{
#if SLIM_CLEAR_HAPLOSOMES
					// When the child generation is valid, all parental haplosomes should have null mutrun pointers [OBSOLETE: so mutrun refcounts are correct]
					for (int mutrun_index = 0; mutrun_index < haplosome1->mutrun_count_; ++mutrun_index)
						if (haplosome1->mutruns_[mutrun_index] != nullptr)
							EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental haplosome has a nonnull mutrun pointer." << EidosTerminate();
#endif
				}
				else
				{
#if SLIM_CLEAR_HAPLOSOMES
					// When the parental generation is valid, all parental haplosomes should have non-null mutrun pointers
					for (int mutrun_index = 0; mutrun_index < haplosome1->mutrun_count_; ++mutrun_index)
						if (haplosome1->mutruns_[mutrun_index] == nullptr)
							EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental haplosome has a null mutrun pointer." << EidosTerminate();
#endif
					
					// check that every mutrun is used at only one mutrun index (particularly salient for empty mutruns)
					for (int mutrun_index = 0; mutrun_index < haplosome1->mutrun_count_; ++mutrun_index)
					{
						const MutationRun *mutrun = haplosome1->mutruns_[mutrun_index];
						auto found_iter = mutrun_position_map.find(mutrun);
						
						if (found_iter == mutrun_position_map.end())
							mutrun_position_map[mutrun] = mutrun_index;
						else if (found_iter->second != mutrun_index)
						{
							// see Population::FreeUnusedMutationRuns() for debugging code helpful for this error
							EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a mutation run was used at more than one position." << EidosTerminate();
						}
					}
				}
			}
			
			// check haplosome 2 for this chromosome
			if (haplosome_count == 2)
			{
				Haplosome *haplosome2 = individual->haplosomes_[haplosome_index + 1];
				
				if (!haplosome2)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for haplosome2." << EidosTerminate();
				if (haplosome2->individual_ != individual)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between haplosome2->individual_ and individual." << EidosTerminate();
				if (haplosome2->AssociatedChromosome() != chromosome)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome2->AssociatedChromosome() mismatch." << EidosTerminate();
				
				if (!haplosome2->IsNull())
				{
					slim_position_t mutrun_count = chromosome->mutrun_count_;
					slim_position_t mutrun_length = chromosome->mutrun_length_;
					
					if ((haplosome2->mutrun_count_ != mutrun_count) || (haplosome2->mutrun_length_ != mutrun_length))
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome2 of individual has the wrong mutrun count/length." << EidosTerminate();
					
					// check that every mutation in the haplosome belongs to the right chromosome
					for (int run_index = 0; run_index < mutrun_count; ++run_index)
					{
						const MutationRun *mutrun = haplosome2->mutruns_[run_index];
						const MutationIndex *mutrun_iter = mutrun->begin_pointer_const();
						const MutationIndex *mutrun_end_iter = mutrun->end_pointer_const();
						
						for (; mutrun_iter != mutrun_end_iter; ++mutrun_iter)
						{
							Mutation *mut = (mut_block_ptr + *mutrun_iter);
							
							if (mut->chromosome_index_ != haplosome2->chromosome_index_)
								EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mutation with chromosome_index_ " << (unsigned int)mut->chromosome_index_ << " is present in haplosome with chromosome_index_ " << (unsigned int)haplosome2->chromosome_index_ << "." << EidosTerminate();
						}
					}
				}
				
				if (((haplosome2->mutrun_count_ == 0) && ((haplosome2->mutrun_length_ != 0) || (haplosome2->mutruns_ != nullptr))) ||
					((haplosome2->mutrun_length_ == 0) && ((haplosome2->mutrun_count_ != 0) || (haplosome2->mutruns_ != nullptr))))
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome2 mutrun count/length/pointer inconsistency." << EidosTerminate();
				
				if (species_.PedigreesEnabled())
				{
					if (haplosome2->haplosome_id_ != individual->pedigree_id_ * 2 + 1)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome2 has an invalid haplosome pedigree ID." << EidosTerminate();
				}
				
				bool null_problem = false;
				
				switch (chromosome_type)
				{
						// haplosome2 should be null for these types
					case ChromosomeType::kHNull_HaploidAutosomeWithNull:
						if (!haplosome2->IsNull())
							null_problem = true;
						break;
						
						// haplosome2 should be non-null for these types
					case ChromosomeType::kZ_ZSexChromosome:
						if (haplosome2->IsNull())
							null_problem = true;
						break;
						
						// haplosome2 should be null in females, non-null in males
					case ChromosomeType::kNullY_YSexChromosomeWithNull:
						if (is_female != haplosome2->IsNull())
							null_problem = true;
						break;
						
						// haplosome2 should be null in males, non-null in females
					case ChromosomeType::kX_XSexChromosome:
						if (is_female == haplosome2->IsNull())
							null_problem = true;
						break;
						
						// we allow either for these types (to allow haplodiploidy, alternation of generations, etc.)
					case ChromosomeType::kA_DiploidAutosome:
						break;
						
						// haplosome2 should not exist at all for these types
					case ChromosomeType::kH_HaploidAutosome:
					case ChromosomeType::kY_YSexChromosome:
					case ChromosomeType::kW_WSexChromosome:
					case ChromosomeType::kHF_HaploidFemaleInherited:
					case ChromosomeType::kFL_HaploidFemaleLine:
					case ChromosomeType::kHM_HaploidMaleInherited:
					case ChromosomeType::kML_HaploidMaleLine:
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) chromosome type should never be used for haplosome2." << EidosTerminate();
						break;
				}
				
				if (null_problem)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome2 mismatch between expected and actual null haplosome status." << EidosTerminate();
				
				if (population_.child_generation_valid_)
				{
#if SLIM_CLEAR_HAPLOSOMES
					// When the child generation is valid, all parental haplosomes should have null mutrun pointers [OBSOLETE: so mutrun refcounts are correct]
					for (int mutrun_index = 0; mutrun_index < haplosome2->mutrun_count_; ++mutrun_index)
						if (haplosome2->mutruns_[mutrun_index] != nullptr)
							EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental haplosome has a nonnull mutrun pointer." << EidosTerminate();
#endif
				}
				else
				{
#if SLIM_CLEAR_HAPLOSOMES
					// When the parental generation is valid, all parental haplosomes should have non-null mutrun pointers
					for (int mutrun_index = 0; mutrun_index < haplosome2->mutrun_count_; ++mutrun_index)
						if (haplosome2->mutruns_[mutrun_index] == nullptr)
							EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental haplosome has a null mutrun pointer." << EidosTerminate();
#endif
					
					// check that every mutrun is used at only one mutrun index (particularly salient for empty mutruns)
					for (int mutrun_index = 0; mutrun_index < haplosome2->mutrun_count_; ++mutrun_index)
					{
						const MutationRun *mutrun = haplosome2->mutruns_[mutrun_index];
						auto found_iter = mutrun_position_map.find(mutrun);
						
						if (found_iter == mutrun_position_map.end())
							mutrun_position_map[mutrun] = mutrun_index;
						else if (found_iter->second != mutrun_index)
						{
							// see Population::FreeUnusedMutationRuns() for debugging code helpful for this error
							EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a mutation run was used at more than one position." << EidosTerminate();
						}
					}
				}
			}
			
			haplosome_index += haplosome_count;
		}
	}
	
	//
	//	Check the child generation; this is only in WF models
	//
	
	if (model_type == SLiMModelType::kModelTypeWF)
	{
		if ((int)child_individuals_.size() != child_subpop_size_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between child_subpop_size_ and child_individuals_.size()." << EidosTerminate();
		
		for (int ind_index = 0; ind_index < child_subpop_size_; ++ind_index)
		{
			Individual *individual = child_individuals_[ind_index];
			
			if (!individual)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for individual." << EidosTerminate();
			
			if (individual->index_ != ind_index)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->index_ and ind_index." << EidosTerminate();
			
			if (individual->subpopulation_ != this)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->subpopulation_ and subpopulation." << EidosTerminate();
			
			if (species_.PedigreesEnabled() && population_.child_generation_valid_)
			{
				if (individual->pedigree_id_ == -1)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) individual has an invalid pedigree ID." << EidosTerminate();
			}
			
			if (individual->age_ != -1)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) invalid value for individual->age_." << EidosTerminate();
			
			bool is_female = (ind_index < child_first_male_index_);		// only used below in sexual simulations
			
			if (sex_enabled_)
			{
				if ((is_female && (individual->sex_ != IndividualSex::kFemale)) ||
					(!is_female && (individual->sex_ != IndividualSex::kMale)))
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->sex_ and child_first_male_index_." << EidosTerminate();
			}
			else
			{
				if (individual->sex_ != IndividualSex::kHermaphrodite)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) non-hermaphrodite individual in non-sexual simulation." << EidosTerminate();
			}
			
			// loop over chromosomes one by one and check the haplosomes of each chromosome
			int haplosome_index = 0;
			
			for (size_t chromosome_index = 0; chromosome_index < chromosomes_count; chromosome_index++)
			{
				Chromosome *chromosome = chromosomes[chromosome_index];
				ChromosomeType chromosome_type = chromosome->Type();
				
				// check haplosome indices
				int haplosome_count = chromosome->IntrinsicPloidy();
				
				// check haplosome 1 for this chromosome
				{
					Haplosome *haplosome1 = individual->haplosomes_[haplosome_index];
					
					if (!haplosome1)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for haplosome1." << EidosTerminate();
					if (haplosome1->individual_ != individual)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between haplosome1->individual_ and individual." << EidosTerminate();
					if (haplosome1->AssociatedChromosome() != chromosome)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome1->AssociatedChromosome() mismatch." << EidosTerminate();
					
					if (!haplosome1->IsNull())
					{
						slim_position_t mutrun_count = chromosome->mutrun_count_;
						slim_position_t mutrun_length = chromosome->mutrun_length_;
						
						if ((haplosome1->mutrun_count_ != mutrun_count) || (haplosome1->mutrun_length_ != mutrun_length))
							EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome1 of individual has the wrong mutrun count/length." << EidosTerminate();
						
						// do not check haplosomes in the child generation; they are conceptually cleared to
						// nullptr (but can actually even contain garbage, unless SLIM_CLEAR_HAPLOSOMES is set
					}
					
					if (((haplosome1->mutrun_count_ == 0) && ((haplosome1->mutrun_length_ != 0) || (haplosome1->mutruns_ != nullptr))) ||
						((haplosome1->mutrun_length_ == 0) && ((haplosome1->mutrun_count_ != 0) || (haplosome1->mutruns_ != nullptr))))
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome1 mutrun count/length/pointer inconsistency." << EidosTerminate();
					
					// we don't check pedigree IDs for the child generation; they are not expected to be set up
					
					bool null_problem = false;
					
					switch (chromosome_type)
					{
							// haplosome1 should be null for these types
						case ChromosomeType::kNullY_YSexChromosomeWithNull:
							if (!haplosome1->IsNull())
								null_problem = true;
							break;
							
							// haplosome1 should be non-null for these types
						case ChromosomeType::kX_XSexChromosome:
						case ChromosomeType::kHF_HaploidFemaleInherited:
						case ChromosomeType::kHM_HaploidMaleInherited:
							if (haplosome1->IsNull())
								null_problem = true;
							break;
							
							// haplosome1 should be null in females, non-null in males
						case ChromosomeType::kY_YSexChromosome:
						case ChromosomeType::kZ_ZSexChromosome:
						case ChromosomeType::kML_HaploidMaleLine:
							if (is_female != haplosome1->IsNull())
								null_problem = true;
							break;
							
							// haplosome1 should be null in males, non-null in females
						case ChromosomeType::kW_WSexChromosome:
						case ChromosomeType::kFL_HaploidFemaleLine:
							if (is_female == haplosome1->IsNull())
								null_problem = true;
							break;
							
							// we allow either for these types (to allow haplodiploidy, alternation of generations, etc.)
						case ChromosomeType::kA_DiploidAutosome:
						case ChromosomeType::kH_HaploidAutosome:
						case ChromosomeType::kHNull_HaploidAutosomeWithNull:
							break;
					}
					
					if (null_problem)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome1 mismatch between expected and actual null haplosome status." << EidosTerminate();
					
					if (!population_.child_generation_valid_)
					{
#if SLIM_CLEAR_HAPLOSOMES
						// When the parental generation is valid, all child haplosomes should have null mutrun pointers [OBSOLETE: so mutrun refcounts are correct]
						for (int mutrun_index = 0; mutrun_index < haplosome1->mutrun_count_; ++mutrun_index)
							if (haplosome1->mutruns_[mutrun_index] != nullptr)
								EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a child haplosome has a nonnull mutrun pointer." << EidosTerminate();
#endif
					}
					else
					{
#if SLIM_CLEAR_HAPLOSOMES
						// When the child generation is valid, all child haplosomes should have non-null mutrun pointers
						for (int mutrun_index = 0; mutrun_index < haplosome1->mutrun_count_; ++mutrun_index)
							if (haplosome1->mutruns_[mutrun_index] == nullptr)
								EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a child haplosome has a null mutrun pointer." << EidosTerminate();
#endif
						
						// check that every mutrun is used at only one mutrun index (particularly salient for empty mutruns)
						for (int mutrun_index = 0; mutrun_index < haplosome1->mutrun_count_; ++mutrun_index)
						{
							const MutationRun *mutrun = haplosome1->mutruns_[mutrun_index];
							auto found_iter = mutrun_position_map.find(mutrun);
							
							if (found_iter == mutrun_position_map.end())
								mutrun_position_map[mutrun] = mutrun_index;
							else if (found_iter->second != mutrun_index)
							{
								// see Population::FreeUnusedMutationRuns() for debugging code helpful for this error
								EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a mutation run was used at more than one position." << EidosTerminate();
							}
						}
					}
				}
				
				// check haplosome 2 for this chromosome
				if (haplosome_count == 2)
				{
					Haplosome *haplosome2 = individual->haplosomes_[haplosome_index + 1];
					
					if (!haplosome2)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for haplosome2." << EidosTerminate();
					if (haplosome2->individual_ != individual)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between haplosome2->individual_ and individual." << EidosTerminate();
					if (haplosome2->AssociatedChromosome() != chromosome)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome2->AssociatedChromosome() mismatch." << EidosTerminate();
					
					if (!haplosome2->IsNull())
					{
						slim_position_t mutrun_count = chromosome->mutrun_count_;
						slim_position_t mutrun_length = chromosome->mutrun_length_;
						
						if ((haplosome2->mutrun_count_ != mutrun_count) || (haplosome2->mutrun_length_ != mutrun_length))
							EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome2 of individual has the wrong mutrun count/length." << EidosTerminate();
						
						// do not check haplosomes in the child generation; they are conceptually cleared to
						// nullptr (but can actually even contain garbage, unless SLIM_CLEAR_HAPLOSOMES is set
					}
					
					if (((haplosome2->mutrun_count_ == 0) && ((haplosome2->mutrun_length_ != 0) || (haplosome2->mutruns_ != nullptr))) ||
						((haplosome2->mutrun_length_ == 0) && ((haplosome2->mutrun_count_ != 0) || (haplosome2->mutruns_ != nullptr))))
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome2 mutrun count/length/pointer inconsistency." << EidosTerminate();
					
					// we don't check pedigree IDs for the child generation; they are not expected to be set up
					
					bool null_problem = false;
					
					switch (chromosome_type)
					{
							// haplosome2 should be null for these types
						case ChromosomeType::kHNull_HaploidAutosomeWithNull:
							if (!haplosome2->IsNull())
								null_problem = true;
							break;
							
							// haplosome2 should be non-null for these types
						case ChromosomeType::kZ_ZSexChromosome:
							if (haplosome2->IsNull())
								null_problem = true;
							break;
							
							// haplosome2 should be null in females, non-null in males
						case ChromosomeType::kNullY_YSexChromosomeWithNull:
							if (is_female != haplosome2->IsNull())
								null_problem = true;
							break;
							
							// haplosome2 should be null in males, non-null in females
						case ChromosomeType::kX_XSexChromosome:
							if (is_female == haplosome2->IsNull())
								null_problem = true;
							break;
							
							// we allow either for these types (to allow haplodiploidy, alternation of generations, etc.)
						case ChromosomeType::kA_DiploidAutosome:
							break;
							
							// haplosome2 should not exist at all for these types
						case ChromosomeType::kH_HaploidAutosome:
						case ChromosomeType::kY_YSexChromosome:
						case ChromosomeType::kW_WSexChromosome:
						case ChromosomeType::kHF_HaploidFemaleInherited:
						case ChromosomeType::kFL_HaploidFemaleLine:
						case ChromosomeType::kHM_HaploidMaleInherited:
						case ChromosomeType::kML_HaploidMaleLine:
							EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) chromosome type should never be used for haplosome2." << EidosTerminate();
							break;
					}
					
					if (null_problem)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) haplosome2 mismatch between expected and actual null haplosome status." << EidosTerminate();
					
					if (!population_.child_generation_valid_)
					{
#if SLIM_CLEAR_HAPLOSOMES
						// When the parental generation is valid, all child haplosomes should have null mutrun pointers [OBSOLETE: so mutrun refcounts are correct]
						for (int mutrun_index = 0; mutrun_index < haplosome2->mutrun_count_; ++mutrun_index)
							if (haplosome2->mutruns_[mutrun_index] != nullptr)
								EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental haplosome has a nonnull mutrun pointer." << EidosTerminate();
#endif
					}
					else
					{
#if SLIM_CLEAR_HAPLOSOMES
						// When the child generation is valid, all child haplosomes should have non-null mutrun pointers
						for (int mutrun_index = 0; mutrun_index < haplosome2->mutrun_count_; ++mutrun_index)
							if (haplosome2->mutruns_[mutrun_index] == nullptr)
								EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental haplosome has a null mutrun pointer." << EidosTerminate();
#endif
						
						// check that every mutrun is used at only one mutrun index (particularly salient for empty mutruns)
						for (int mutrun_index = 0; mutrun_index < haplosome2->mutrun_count_; ++mutrun_index)
						{
							const MutationRun *mutrun = haplosome2->mutruns_[mutrun_index];
							auto found_iter = mutrun_position_map.find(mutrun);
							
							if (found_iter == mutrun_position_map.end())
								mutrun_position_map[mutrun] = mutrun_index;
							else if (found_iter->second != mutrun_index)
							{
								// see Population::FreeUnusedMutationRuns() for debugging code helpful for this error
								EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a mutation run was used at more than one position." << EidosTerminate();
							}
						}
					}
				}
				
				haplosome_index += haplosome_count;
			}
		}
	}
	
	//
	// Check that every mutation run is being used at a position corresponding to the pool it was allocated from
	//
	for (Chromosome *chromosome : chromosomes)
	{
		slim_mutrun_index_t mutrun_count_multiplier = chromosome->mutrun_count_multiplier_;
		
		for (int thread_num = 0; thread_num < chromosome->ChromosomeMutationRunContextCount(); ++thread_num)
		{
			MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(thread_num);
			MutationRunPool &in_use_pool = mutrun_context.in_use_pool_;
			
			for (const MutationRun *mutrun : in_use_pool)
			{
				auto found_iter = mutrun_position_map.find(mutrun);
				
				if (found_iter != mutrun_position_map.end())
				{
					slim_mutrun_index_t used_at_index = found_iter->second;
					int correct_thread_num = (int)(used_at_index / mutrun_count_multiplier);
					
					if (correct_thread_num != thread_num)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a mutation run is used at a position that does not correspond to its allocation pool." << EidosTerminate();
				}
			}
		}
	}
	
	//
	//	Check the haplosome junkyards for correct state; BCH 10/15/2024 note that clearing to nullptr is no longer required
	//
	for (Chromosome *chromosome : chromosomes)
	{
		const std::vector<Haplosome *> &haplosomes_junkyard_nonnull = chromosome->HaplosomesJunkyardNonnull();
		const std::vector<Haplosome *> &haplosomes_junkyard_null = chromosome->HaplosomesJunkyardNull();
		
		if (!has_genetics && haplosomes_junkyard_null.size())
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) the null haplosome junkyard should be empty in no-genetics species." << EidosTerminate();
		if (!has_genetics && haplosomes_junkyard_nonnull.size())
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) the nonnull haplosome junkyard should be empty in no-genetics species." << EidosTerminate();
		
		for (Haplosome *haplosome : haplosomes_junkyard_nonnull)
		{
			if (haplosome->IsNull())
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null haplosome in the nonnull haplosome junkyard." << EidosTerminate();
			
#if SLIM_CLEAR_HAPLOSOMES
			haplosome->check_cleared_to_nullptr();
#endif
		}
		
		for (Haplosome *haplosome : haplosomes_junkyard_null)
		{
			if (!haplosome->IsNull())
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) nonnull haplosome in the null haplosome junkyard." << EidosTerminate();
			
#if SLIM_CLEAR_HAPLOSOMES
			haplosome->check_cleared_to_nullptr();
#endif
		}
	}
}

Subpopulation::Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size, bool p_record_in_treeseq, bool p_haploid) :
	self_symbol_(EidosStringRegistry::GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(this, gSLiM_Subpopulation_Class))), 
	community_(p_population.species_.community_), species_(p_population.species_), population_(p_population),
	model_type_(p_population.model_type_), subpopulation_id_(p_subpopulation_id), name_(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)),
	individual_pool_(p_population.species_individual_pool_), individuals_junkyard_(p_population.species_individuals_junkyard_),
	haplosome_count_per_individual_(p_population.species_.HaplosomeCountPerIndividual()),
	parent_subpop_size_(p_subpop_size), child_subpop_size_(p_subpop_size)
#if defined(SLIMGUI)
	, gui_premigration_size_(p_subpop_size)
#endif
{
	// if the species knows that its chromosomes involve null haplosomes, then we inherit that knowledge
	has_null_haplosomes_ = species_.ChromosomesUseNullHaplosomes();
	
	// self_symbol_ is always a constant, but can't be marked as such on construction
	self_symbol_.second->MarkAsConstant();
	
	if (model_type_ == SLiMModelType::kModelTypeWF)
	{
		GenerateParentsToFit(/* p_initial_age */ -1, /* p_sex_ratio */ 0.0, /* p_allow_zero_size */ false, /* p_require_both_sexes */ true, /* p_record_in_treeseq */ p_record_in_treeseq, p_haploid, /* p_mean_parent_age */ -1.0F);
		GenerateChildrenToFitWF();
	}
	else
	{
		GenerateParentsToFit(/* p_initial_age */ 0, /* p_sex_ratio */ 0.0, /* p_allow_zero_size */ true, /* p_require_both_sexes */ false, /* p_record_in_treeseq */ p_record_in_treeseq, p_haploid, /* p_mean_parent_age */ 0.0F);
	}
	
	if (model_type_ == SLiMModelType::kModelTypeWF)
	{
		// Set up to draw random individuals, based initially on equal fitnesses
		cached_parental_fitness_ = (double *)realloc(cached_parental_fitness_, sizeof(double) * parent_subpop_size_);
		if (!cached_parental_fitness_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::Subpopulation): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
		cached_fitness_capacity_ = parent_subpop_size_;
		cached_fitness_size_ = parent_subpop_size_;
		
		double *fitness_buffer_ptr = cached_parental_fitness_;
		
		for (slim_popsize_t i = 0; i < parent_subpop_size_; i++)
			*(fitness_buffer_ptr++) = 1.0;
		
		lookup_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_, cached_parental_fitness_);
	}
}

// SEX ONLY
Subpopulation::Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size, bool p_record_in_treeseq,
							 double p_sex_ratio, bool p_haploid) :
	self_symbol_(EidosStringRegistry::GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(this, gSLiM_Subpopulation_Class))),
	community_(p_population.species_.community_), species_(p_population.species_), population_(p_population),
	model_type_(p_population.model_type_), subpopulation_id_(p_subpopulation_id), name_(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)),
	individual_pool_(p_population.species_individual_pool_), individuals_junkyard_(p_population.species_individuals_junkyard_),
	haplosome_count_per_individual_(p_population.species_.HaplosomeCountPerIndividual()),
	parent_subpop_size_(p_subpop_size), parent_sex_ratio_(p_sex_ratio), child_subpop_size_(p_subpop_size), child_sex_ratio_(p_sex_ratio), sex_enabled_(true)
#if defined(SLIMGUI)
	, gui_premigration_size_(p_subpop_size)
#endif
{
	// if the species knows that its chromosomes involve null haplosomes, then we inherit that knowledge
	has_null_haplosomes_ = species_.ChromosomesUseNullHaplosomes();
	
	// self_symbol_ is always a constant, but can't be marked as such on construction
	self_symbol_.second->MarkAsConstant();
	
	if (model_type_ == SLiMModelType::kModelTypeWF)
	{
		GenerateParentsToFit(/* p_initial_age */ -1, /* p_sex_ratio */ p_sex_ratio, /* p_allow_zero_size */ false, /* p_require_both_sexes */ true, /* p_record_in_treeseq */ p_record_in_treeseq, p_haploid, /* p_mean_parent_age */ -1.0F);
		GenerateChildrenToFitWF();
	}
	else
	{
		GenerateParentsToFit(/* p_initial_age */ 0, /* p_sex_ratio */ p_sex_ratio, /* p_allow_zero_size */ true, /* p_require_both_sexes */ false, /* p_record_in_treeseq */ p_record_in_treeseq, p_haploid, /* p_mean_parent_age */ 0.0F);
	}
	
	if (model_type_ == SLiMModelType::kModelTypeWF)
	{
		// Set up to draw random females, based initially on equal fitnesses
		cached_parental_fitness_ = (double *)realloc(cached_parental_fitness_, sizeof(double) * parent_subpop_size_);
		cached_male_fitness_ = (double *)realloc(cached_male_fitness_, sizeof(double) * parent_subpop_size_);
		if (!cached_parental_fitness_ || !cached_male_fitness_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::Subpopulation): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
		cached_fitness_capacity_ = parent_subpop_size_;
		cached_fitness_size_ = parent_subpop_size_;
		
		double *fitness_buffer_ptr = cached_parental_fitness_;
		double *male_buffer_ptr = cached_male_fitness_;
		
		for (slim_popsize_t i = 0; i < parent_first_male_index_; i++)
		{
			*(fitness_buffer_ptr++) = 1.0;
			*(male_buffer_ptr++) = 0.0;				// this vector has 0 for all females, for mateChoice() callbacks
		}
		
		// Set up to draw random males, based initially on equal fitnesses
		slim_popsize_t num_males = parent_subpop_size_ - parent_first_male_index_;
		
		for (slim_popsize_t i = 0; i < num_males; i++)
		{
			*(fitness_buffer_ptr++) = 1.0;
			*(male_buffer_ptr++) = 1.0;
		}
		
		lookup_female_parent_ = gsl_ran_discrete_preproc(parent_first_male_index_, cached_parental_fitness_);
		lookup_male_parent_ = gsl_ran_discrete_preproc(num_males, cached_parental_fitness_ + parent_first_male_index_);
	}
	
	if (model_type_ == SLiMModelType::kModelTypeNonWF)
	{
		// OK, so.  When reading a nonWF tree-seq file, we get passed in a sex ratio that is the sex ratio of the individuals in the file.
		// That's good, so the individuals that get created have the correct sex.  However, we want the sex ratio ivars in the subpop
		// to have their default value of 0.0, ultimately; those variables are not supposed to be updated/accurate in nonWF models.  This
		// fell through the cracks because, who cares if an undefined/unused variable has the wrong value?  But then we write out that
		// non-zero value to the .trees file, and that was confusing Peter's test code.  So now we explicitly set the ivars to 0.0 here,
		// now that we're done using the value.  This might also occur when a new subpop is created in a nonWF model; the initial sex ratio
		// value might have been sticking around permanently in the ivar, even though it would not be accurate any more.  BCH 9/7/2020
		parent_sex_ratio_ = 0.0;
		child_sex_ratio_ = 0.0;
	}
}


Subpopulation::~Subpopulation(void)
{
	//std::cout << "Subpopulation::~Subpopulation" << std::endl;
	
	if (lookup_parent_)
		gsl_ran_discrete_free(lookup_parent_);
	
	if (lookup_female_parent_)
		gsl_ran_discrete_free(lookup_female_parent_);
	
	if (lookup_male_parent_)
		gsl_ran_discrete_free(lookup_male_parent_);
	
	if (cached_parental_fitness_)
		free(cached_parental_fitness_);
	
	if (cached_male_fitness_)
		free(cached_male_fitness_);
	
	{
		// dispose of haplosomes and individuals with our object pools
		// note that these might get reused; this is not necessarily the simulation end
		for (Individual *individual : parent_individuals_)
			FreeSubpopIndividual(individual);
		
		for (Individual *individual : child_individuals_)
			FreeSubpopIndividual(individual);
		
		for (Individual *individual : nonWF_offspring_individuals_)
			FreeSubpopIndividual(individual);
	}
	
	for (const auto &map_pair : spatial_maps_)
	{
		SpatialMap *map_ptr = map_pair.second;
		
		if (map_ptr)
			map_ptr->Release();
	}
}

void Subpopulation::SetName(const std::string &p_name)
{
	if (p_name == name_)
		return;
	
	if (p_name.length() == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::SetName): property name must not be zero-length." << EidosTerminate();
	
	bool isSubpopID = (p_name == SLiMEidosScript::IDStringWithPrefix('p', subpopulation_id_));
	
	// check that names of the form "pX" match our self-symbol; can't use somebody else's identifier!
	if (!isSubpopID && SLiMEidosScript::StringIsIDWithPrefix(p_name, 'p'))
		EIDOS_TERMINATION << "ERROR (Subpopulation::SetName): property name must not be a subpopulation symbol ('p1', 'p2', etc.) unless it matches the symbol of the subpopulation itself." << EidosTerminate();
	
	// we suggest in the doc that subpopulation names ought to be Python identifiers, for maximum interoperability, but
	// we do not enforce that here since tskit itself does not require this; this is more of an msprime/Demes thing
	
	// names have to be unique across all time, and cannot be shared or reused; check and remember names here
	// note that we don't unique or track names that match the subpop ID, since they are guaranteed unique
	// and cannot be used by any other subpop anyway (and no other subpop can have the same ID)
	if (!isSubpopID)
	{
		if (community_.SubpopulationNameInUse(p_name))
			EIDOS_TERMINATION << "ERROR (Subpopulation::SetName): property name must be unique across all subpopulations; " << p_name << " is already in use, or was previously used." << EidosTerminate();
		
		species_.used_subpop_names_.emplace(p_name);	// added; never removed unless the simulation state is reset
	}
	
	// we also need to keep track of the last used name for each subpop id, even if it is the generic name
	species_.used_subpop_ids_[subpopulation_id_] = p_name;
	
	name_ = p_name;
}

slim_refcount_t Subpopulation::NullHaplosomeCount(void)
{
	if (population_.child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::NullHaplosomeCount): (internal error) called with child generation active!" << EidosTerminate();
	
	int haplosome_count_per_individual = species_.HaplosomeCountPerIndividual();
	slim_refcount_t null_haplosome_count = 0;
	
	for (Individual *ind : parent_individuals_)
	{
		Haplosome **haplosomes = ind->haplosomes_;
		
		for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
		{
			Haplosome *haplosome = haplosomes[haplosome_index];
			
			if (haplosome->IsNull())
				null_haplosome_count++;
		}
	}
	
	return null_haplosome_count;
}

#if (defined(_OPENMP) && SLIM_USE_NONNEUTRAL_CACHES)
void Subpopulation::FixNonNeutralCaches_OMP(void)
{
	// This is used in the parallel fitness evaluation case to fix caches up front
	// This is task-based; note the top-level parallel is *not* a parallel for loop!
#pragma omp parallel default(none)
	{
#pragma omp single
		{
			int32_t nonneutral_change_counter = species_.nonneutral_change_counter_;
			int32_t nonneutral_regime = species_.last_nonneutral_regime_;
#error 	FIXME this *2 is now based on an incorrect assumption of simple diploidy
			slim_popsize_t haplosomeCount = parent_subpop_size_ * 2;
			
			for (slim_popsize_t haplosome_index = 0; haplosome_index < haplosomeCount; haplosome_index++)
			{
				Haplosome *haplosome = parent_haplosomes_[haplosome_index];
				const int32_t mutrun_count = haplosome->mutrun_count_;
				
				for (int run_index = 0; run_index < mutrun_count; ++run_index)
				{
					const MutationRun *mutrun = haplosome->mutruns_[run_index];
					
					// This will start a new task if the mutrun needs to validate
					// its nonneutral cache.  It avoids doing so more than once.
					mutrun->validate_nonneutral_cache(nonneutral_change_counter, nonneutral_regime);
				}
			}
		}
	}
}
#endif

void Subpopulation::UpdateFitness(std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks, std::vector<SLiMEidosBlock*> &p_fitnessEffect_callbacks)
{
	const std::map<slim_objectid_t,MutationType*> &mut_types = species_.MutationTypes();
	
	// The FitnessOfParent...() methods called by this method rely upon cached fitness values
	// kept inside the Mutation objects.  Those caches may need to be validated before we can
	// calculate fitness values.  We check for that condition and repair it first.
	if (species_.any_dominance_coeff_changed_)
	{
		population_.ValidateMutationFitnessCaches();	// note one subpop triggers it, but the recaching occurs for the whole sim
		species_.any_dominance_coeff_changed_ = false;
	}
	
	// This function calculates the population mean fitness as a side effect
	double totalFitness = 0.0;
	
	// Figure out our callback scenario: zero, one, or many?  See the comment below, above FitnessOfParentWithHaplosomeIndices_NoCallbacks(),
	// for more info on this complication.  Here we just figure out which version to call and set up for it.
	int mutationEffect_callback_count = (int)p_mutationEffect_callbacks.size();
	bool mutationEffect_callbacks_exist = (mutationEffect_callback_count > 0);
	bool single_mutationEffect_callback = false;
	
	if (mutationEffect_callback_count == 1)
	{
		slim_objectid_t mutation_type_id = p_mutationEffect_callbacks[0]->mutation_type_id_;
        MutationType *found_muttype = species_.MutationTypeWithID(mutation_type_id);
		
		if (found_muttype)
		{
			if (mut_types.size() > 1)
			{
				// We have a single callback that applies to a known mutation type among more than one defined type; we can optimize that
				single_mutationEffect_callback = true;
			}
			// else there is only one mutation type, so the callback applies to all mutations in the simulation
		}
		else
		{
			// The only callback refers to a mutation type that doesn't exist, so we effectively have no callbacks; we probably never hit this
			mutationEffect_callback_count = 0;
			(void)mutationEffect_callback_count;		// tell the static analyzer that we know we just did a dead store
			mutationEffect_callbacks_exist = false;
		}
	}
	
	// Can we skip chromosome-based fitness calculations altogether, and just call fitnessEffect() callbacks if any?
	// We can do this if (a) all mutation types either use a neutral DFE, or have been made neutral with a "return 1.0;"
	// mutationEffect() callback that is active, (b) for the mutation types that use a neutral DFE, no mutation has had its
	// selection coefficient changed, and (c) no mutationEffect() callbacks are active apart from "return 1.0;" type callbacks.
	// This is often the case for QTL-based models (such as Misha's coral model), and should produce a big speed gain,
	// so we do a pre-check here for this case.  Note that we can ignore fitnessEffect() callbacks in this situation,
	// because they are explicitly documented as potentially being executed after mutationEffect() callbacks, so
	// they are not allowed, as a matter of policy, to alter the operation of mutationEffect() callbacks.
	bool skip_chromosomal_fitness = true;
	
	// Looping through all of the mutation types and setting flags can be very expensive, so as a first pass we check
	// whether it is even conceivable that we will be able to have skip_chromosomal_fitness == true.  If the simulation
	// is not pure neutral and we have no mutationEffect() callback that could change that, it is a no-go without checking the
	// mutation types at all.
	if (!species_.pure_neutral_)
	{
		skip_chromosomal_fitness = false;	// we're not pure neutral, so we have to prove that it is possible
		
		for (SLiMEidosBlock *mutationEffect_callback : p_mutationEffect_callbacks)
		{
			if (mutationEffect_callback->block_active_)
			{
				const EidosASTNode *compound_statement_node = mutationEffect_callback->compound_statement_node_;
				
				if (compound_statement_node->cached_return_value_)
				{
					// The script is a constant expression such as "{ return 1.1; }"
					EidosValue *result = compound_statement_node->cached_return_value_.get();
					
					if ((result->Type() == EidosValueType::kValueFloat) || (result->Count() == 1))
					{
						if (result->FloatData()[0] == 1.0)
						{
							// we have a mutationEffect() callback that is neutral-making, so it could conceivably work;
							// change our minds but keep checking
							skip_chromosomal_fitness = true;
							continue;
						}
					}
				}
				
				// if we reach this point, we have an active callback that is not neutral-making, so we fail and we're done
				skip_chromosomal_fitness = false;
				break;
			}
		}
	}
	
	// At this point it appears conceivable that we could skip, but we don't yet know.  We need to do a more thorough
	// check, actually tracking precisely which mutation types are neutral and non-neutral, and which are made neutral
	// by mutationEffect() callbacks.  Note this block is the only place where is_pure_neutral_now_ is valid or used!!!
	if (skip_chromosomal_fitness)
	{
		// first set a flag on all mut types indicating whether they are pure neutral according to their DFE
		for (auto &mut_type_iter : mut_types)
			mut_type_iter.second->is_pure_neutral_now_ = mut_type_iter.second->all_pure_neutral_DFE_;
		
		// then go through the mutationEffect() callback list and set the pure neutral flag for mut types neutralized by an active callback
		for (SLiMEidosBlock *mutationEffect_callback : p_mutationEffect_callbacks)
		{
			if (mutationEffect_callback->block_active_)
			{
				const EidosASTNode *compound_statement_node = mutationEffect_callback->compound_statement_node_;
				
				if (compound_statement_node->cached_return_value_)
				{
					// The script is a constant expression such as "{ return 1.1; }"
					EidosValue *result = compound_statement_node->cached_return_value_.get();
					
					if ((result->Type() == EidosValueType::kValueFloat) && (result->Count() == 1))
					{
						if (result->FloatData()[0] == 1.0)
						{
							// the callback returns 1.0, so it makes the mutation types to which it applies become neutral
							slim_objectid_t mutation_type_id = mutationEffect_callback->mutation_type_id_;
							
							if (mutation_type_id == -1)
							{
								for (auto &mut_type_iter : mut_types)
									mut_type_iter.second->is_pure_neutral_now_ = true;
							}
							else
							{
                                MutationType *found_muttype = species_.MutationTypeWithID(mutation_type_id);
                                
								if (found_muttype)
									found_muttype->is_pure_neutral_now_ = true;
							}
							
							continue;
						}
					}
				}
				
				// if we reach this point, we have an active callback that is not neutral-making, so we fail and we're done
				skip_chromosomal_fitness = false;
				break;
			}
		}
		
		// finally, tabulate the pure-neutral flags of all the mut types into an overall flag for whether we can skip
		if (skip_chromosomal_fitness)
		{
			for (auto &mut_type_iter : mut_types)
			{
				if (!mut_type_iter.second->is_pure_neutral_now_)
				{
					skip_chromosomal_fitness = false;
					break;
				}
			}
		}
		else
		{
			// At this point, there is an active callback that is not neutral-making, so we really can't reliably depend
			// upon is_pure_neutral_now_; that rogue callback could make other callbacks active/inactive, etc.  So in
			// principle we should now go through and clear the is_pure_neutral_now_ flags to avoid any confusion.  But
			// we are the only ones to use is_pure_neutral_now_, and we're done using it, so we can skip that work.
			
			//for (auto &mut_type_iter : mut_types)
			//	mut_type_iter.second->is_pure_neutral_now_ = false;
		}
	}
	
	// Figure out global callbacks; these are callbacks with NULL supplied for the mut-type id, which means that they are called
	// exactly once per individual, for every individual regardless of genetics, to provide an entry point for alternate fitness definitions
	int fitnessEffect_callback_count = (int)p_fitnessEffect_callbacks.size();
	bool fitnessEffect_callbacks_exist = (fitnessEffect_callback_count > 0);
	
	// We optimize the pure neutral case, as long as no mutationEffect() or fitnessEffect() callbacks are defined; fitness values are then simply 1.0, for everybody.
	// BCH 12 Jan 2018: now fitness_scaling_ modifies even pure_neutral_ models, but the framework here remains valid
	bool pure_neutral = (!mutationEffect_callbacks_exist && !fitnessEffect_callbacks_exist && species_.pure_neutral_);
	double subpop_fitness_scaling = subpop_fitness_scaling_;
	
	// Reset our override of individual cached fitness values; we make this decision afresh with each UpdateFitness() call.  See
	// the header for further comments on this mechanism.
	individual_cached_fitness_OVERRIDE_ = false;
	
	// Decide whether we need to shuffle the order of operations.  This occurs only if (a) we have mutationEffect() or fitnessEffect() callbacks
	// that are enabled, and (b) at least one of them has no cached optimization set.  Otherwise, the order of operations doesn't matter.
	bool needs_shuffle = false;
	
	if (species_.RandomizingCallbackOrder())
	{
		if (!needs_shuffle)
			for (SLiMEidosBlock *callback : p_fitnessEffect_callbacks)
				if (!callback->compound_statement_node_->cached_return_value_ && !callback->has_cached_optimization_)
				{
					needs_shuffle = true;
					break;
				}
		
		if (!needs_shuffle)
			for (SLiMEidosBlock *callback : p_mutationEffect_callbacks)
				if (!callback->compound_statement_node_->cached_return_value_ && !callback->has_cached_optimization_)
				{
					needs_shuffle = true;
					break;
				}
	}
	
	// determine the templated version of FitnessOfParent() that we will call out to for fitness evaluation
	// see Population::EvolveSubpopulation() for further comments on this optimization technique
	double (Subpopulation::*FitnessOfParent_TEMPLATED)(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
	
	if (species_.DoingAnyMutationRunExperiments())
	{
		// If *any* chromosome is doing mutrun experiments, we can't template them out
		if (!mutationEffect_callbacks_exist)
			FitnessOfParent_TEMPLATED = &Subpopulation::FitnessOfParent<true, false, false>;
		else if (single_mutationEffect_callback)
			FitnessOfParent_TEMPLATED = &Subpopulation::FitnessOfParent<true, true, true>;
		else
			FitnessOfParent_TEMPLATED = &Subpopulation::FitnessOfParent<true, true, false>;
	}
	else
	{
		if (!mutationEffect_callbacks_exist)
			FitnessOfParent_TEMPLATED = &Subpopulation::FitnessOfParent<false, false, false>;
		else if (single_mutationEffect_callback)
			FitnessOfParent_TEMPLATED = &Subpopulation::FitnessOfParent<false, true, true>;
		else
			FitnessOfParent_TEMPLATED = &Subpopulation::FitnessOfParent<false, true, false>;
	}
	
	// calculate fitnesses in parent population and cache the values
	if (sex_enabled_)
	{
		// SEX ONLY
		double totalMaleFitness = 0.0, totalFemaleFitness = 0.0;
		
		// Set up to draw random females
		if (pure_neutral)
		{
			if (Individual::s_any_individual_fitness_scaling_set_)
			{
				EIDOS_BENCHMARK_START(EidosBenchmarkType::k_FITNESS_SEX_1);
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_FITNESS_SEX_1);
#pragma omp parallel for schedule(static) default(none) shared(parent_subpop_size_) firstprivate(subpop_fitness_scaling) reduction(+: totalFemaleFitness) if(parent_subpop_size_ >= EIDOS_OMPMIN_FITNESS_SEX_1) num_threads(thread_count)
				for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
				{
					double fitness = parent_individuals_[female_index]->fitness_scaling_;
					
#ifdef SLIMGUI
					parent_individuals_[female_index]->cached_unscaled_fitness_ = fitness;
#endif
					
					fitness *= subpop_fitness_scaling;
					parent_individuals_[female_index]->cached_fitness_UNSAFE_ = fitness;
					totalFemaleFitness += fitness;
				}
				EIDOS_BENCHMARK_END(EidosBenchmarkType::k_FITNESS_SEX_1);
			}
			else
			{
#ifdef SLIMGUI
				for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
					parent_individuals_[female_index]->cached_unscaled_fitness_ = 1.0;
#endif
				
				double fitness = subpop_fitness_scaling;	// no individual fitness_scaling_
				
				// Here we override setting up every cached_fitness_UNSAFE_ value, and set up a subpop-level cache instead.
				// This is why cached_fitness_UNSAFE_ is marked "UNSAFE".  See the header for details on this.
				if (model_type_ == SLiMModelType::kModelTypeWF)
				{
					individual_cached_fitness_OVERRIDE_ = true;
					individual_cached_fitness_OVERRIDE_value_ = fitness;
				}
				else
				{
					EIDOS_BENCHMARK_START(EidosBenchmarkType::k_FITNESS_SEX_2);
					EIDOS_THREAD_COUNT(gEidos_OMP_threads_FITNESS_SEX_2);
#pragma omp parallel for schedule(static) default(none) shared(parent_subpop_size_) firstprivate(fitness) if(parent_subpop_size_ >= EIDOS_OMPMIN_FITNESS_SEX_2) num_threads(thread_count)
					for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
					{
						parent_individuals_[female_index]->cached_fitness_UNSAFE_ = fitness;
					}
					EIDOS_BENCHMARK_END(EidosBenchmarkType::k_FITNESS_SEX_2);
				}
				
				totalFemaleFitness = fitness * parent_first_male_index_;
			}
		}
		else if (skip_chromosomal_fitness)
		{
			if (!needs_shuffle)
			{
				for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
				{
					double fitness = parent_individuals_[female_index]->fitness_scaling_;
					
					if (fitnessEffect_callbacks_exist && (fitness > 0.0))
						fitness *= ApplyFitnessEffectCallbacks(p_fitnessEffect_callbacks, female_index);
					
#ifdef SLIMGUI
					parent_individuals_[female_index]->cached_unscaled_fitness_ = fitness;
#endif
					
					fitness *= subpop_fitness_scaling;
					parent_individuals_[female_index]->cached_fitness_UNSAFE_ = fitness;
					totalFemaleFitness += fitness;
				}
			}
			else
			{
				// general case for females without chromosomal fitness; shuffle buffer needed
				slim_popsize_t *shuffle_buf = species_.BorrowShuffleBuffer(parent_first_male_index_);
				
				for (slim_popsize_t shuffle_index = 0; shuffle_index < parent_first_male_index_; shuffle_index++)
				{
					slim_popsize_t female_index = shuffle_buf[shuffle_index];
					double fitness = parent_individuals_[female_index]->fitness_scaling_;
					
					if (fitnessEffect_callbacks_exist && (fitness > 0.0))
						fitness *= ApplyFitnessEffectCallbacks(p_fitnessEffect_callbacks, female_index);
					
#ifdef SLIMGUI
					parent_individuals_[female_index]->cached_unscaled_fitness_ = fitness;
#endif
					
					fitness *= subpop_fitness_scaling;
					parent_individuals_[female_index]->cached_fitness_UNSAFE_ = fitness;
					totalFemaleFitness += fitness;
				}
				
				species_.ReturnShuffleBuffer();
			}
		}
		else
		{
			if (!needs_shuffle)
			{
				// FIXME should have some additional criterion for whether to go parallel with this, like the number of mutations
				if (!mutationEffect_callbacks_exist && !fitnessEffect_callbacks_exist)
				{
					// a separate loop for parallelization of the no-callback case
					
#if (defined(_OPENMP) && SLIM_USE_NONNEUTRAL_CACHES)
					// we need to fix the nonneutral caches in a separate pass first
					// because all the correct caches need to get flushed to everyone
					// before beginning fitness evaluation, for efficiency
					// beginend_nonneutral_pointers() handles the non-parallel case
					FixNonNeutralCaches_OMP();
#endif
					
					EIDOS_BENCHMARK_START(EidosBenchmarkType::k_FITNESS_SEX_3);
					EIDOS_THREAD_COUNT(gEidos_OMP_threads_FITNESS_SEX_3);
#pragma omp parallel for schedule(dynamic, 16) default(none) shared(parent_first_male_index_, subpop_fitness_scaling) reduction(+: totalFemaleFitness) if(parent_subpop_size_ >= EIDOS_OMPMIN_FITNESS_SEX_3) num_threads(thread_count)
					for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
					{
						double fitness = parent_individuals_[female_index]->fitness_scaling_;
						
						if (fitness > 0.0)
						{
							fitness *= (this->*FitnessOfParent_TEMPLATED)(female_index, p_mutationEffect_callbacks);
							
#ifdef SLIMGUI
							parent_individuals_[female_index]->cached_unscaled_fitness_ = fitness;
#endif
							
							fitness *= subpop_fitness_scaling;
						}
						else
						{
#ifdef SLIMGUI
							parent_individuals_[female_index]->cached_unscaled_fitness_ = fitness;
#endif
						}
						
						parent_individuals_[female_index]->cached_fitness_UNSAFE_ = fitness;
						totalFemaleFitness += fitness;
					}
					EIDOS_BENCHMARK_END(EidosBenchmarkType::k_FITNESS_SEX_3);
				}
				else	// at least one mutationEffect() or fitnessEffect() callback; not parallelized
				{
					for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
					{
						double fitness = parent_individuals_[female_index]->fitness_scaling_;
						
						if (fitness > 0.0)
						{
							fitness *= (this->*FitnessOfParent_TEMPLATED)(female_index, p_mutationEffect_callbacks);
							
							// multiply in the effects of any fitnessEffect() callbacks
							if (fitnessEffect_callbacks_exist && (fitness > 0.0))
								fitness *= ApplyFitnessEffectCallbacks(p_fitnessEffect_callbacks, female_index);
							
#ifdef SLIMGUI
							parent_individuals_[female_index]->cached_unscaled_fitness_ = fitness;
#endif
							
							fitness *= subpop_fitness_scaling;
						}
						else
						{
#ifdef SLIMGUI
							parent_individuals_[female_index]->cached_unscaled_fitness_ = fitness;
#endif
						}
						
						parent_individuals_[female_index]->cached_fitness_UNSAFE_ = fitness;
						totalFemaleFitness += fitness;
					}
				}
			}
			else
			{
				// general case for females; we use the shuffle buffer to randomize processing order
				slim_popsize_t *shuffle_buf = species_.BorrowShuffleBuffer(parent_first_male_index_);
				
				for (slim_popsize_t shuffle_index = 0; shuffle_index < parent_first_male_index_; shuffle_index++)
				{
					slim_popsize_t female_index = shuffle_buf[shuffle_index];
					double fitness = parent_individuals_[female_index]->fitness_scaling_;
					
					if (fitness > 0.0)
					{
						fitness *= (this->*FitnessOfParent_TEMPLATED)(female_index, p_mutationEffect_callbacks);
						
						// multiply in the effects of any fitnessEffect() callbacks
						if (fitnessEffect_callbacks_exist && (fitness > 0.0))
							fitness *= ApplyFitnessEffectCallbacks(p_fitnessEffect_callbacks, female_index);
						
#ifdef SLIMGUI
						parent_individuals_[female_index]->cached_unscaled_fitness_ = fitness;
#endif
						
						fitness *= subpop_fitness_scaling;
					}
					else
					{
#ifdef SLIMGUI
						parent_individuals_[female_index]->cached_unscaled_fitness_ = fitness;
#endif
					}
					
					parent_individuals_[female_index]->cached_fitness_UNSAFE_ = fitness;
					totalFemaleFitness += fitness;
				}
				
				species_.ReturnShuffleBuffer();
			}
		}
		
		totalFitness += totalFemaleFitness;
		if ((model_type_ == SLiMModelType::kModelTypeWF) && (totalFemaleFitness <= 0.0))
			EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of females is <= 0.0." << EidosTerminate(nullptr);
		
		// Set up to draw random males
		if (pure_neutral)
		{
			if (Individual::s_any_individual_fitness_scaling_set_)
			{
				EIDOS_BENCHMARK_START(EidosBenchmarkType::k_FITNESS_SEX_1);
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_FITNESS_SEX_1);
#pragma omp parallel for schedule(static) default(none) shared(parent_subpop_size_) firstprivate(subpop_fitness_scaling) reduction(+: totalMaleFitness) if(parent_subpop_size_ >= EIDOS_OMPMIN_FITNESS_SEX_1) num_threads(thread_count)
				for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
				{
					double fitness = parent_individuals_[male_index]->fitness_scaling_;
					
#ifdef SLIMGUI
					parent_individuals_[male_index]->cached_unscaled_fitness_ = fitness;
#endif
					
					fitness *= subpop_fitness_scaling;
					parent_individuals_[male_index]->cached_fitness_UNSAFE_ = fitness;
					totalMaleFitness += fitness;
				}
				EIDOS_BENCHMARK_END(EidosBenchmarkType::k_FITNESS_SEX_1);
			}
			else
			{
#ifdef SLIMGUI
				for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
					parent_individuals_[male_index]->cached_unscaled_fitness_ = 1.0;
#endif
				
				double fitness = subpop_fitness_scaling;	// no individual fitness_scaling_
				
				// Here we override setting up every cached_fitness_UNSAFE_ value, and set up a subpop-level cache instead.
				// This is why cached_fitness_UNSAFE_ is marked "UNSAFE".  See the header for details on this.
				if (model_type_ == SLiMModelType::kModelTypeWF)
				{
					individual_cached_fitness_OVERRIDE_ = true;
					individual_cached_fitness_OVERRIDE_value_ = fitness;
				}
				else
				{
					EIDOS_BENCHMARK_START(EidosBenchmarkType::k_FITNESS_SEX_2);
					EIDOS_THREAD_COUNT(gEidos_OMP_threads_FITNESS_SEX_2);
#pragma omp parallel for schedule(static) default(none) shared(parent_subpop_size_) firstprivate(fitness) if(parent_subpop_size_ >= EIDOS_OMPMIN_FITNESS_SEX_2) num_threads(thread_count)
					for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
					{
						parent_individuals_[male_index]->cached_fitness_UNSAFE_ = fitness;
					}
					EIDOS_BENCHMARK_END(EidosBenchmarkType::k_FITNESS_SEX_2);
				}
				
				if (parent_subpop_size_ > parent_first_male_index_)
					totalMaleFitness = fitness * (parent_subpop_size_ - parent_first_male_index_);
			}
		}
		else if (skip_chromosomal_fitness)
		{
			if (!needs_shuffle)
			{
				for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
				{
					double fitness = parent_individuals_[male_index]->fitness_scaling_;
					
					if (fitnessEffect_callbacks_exist && (fitness > 0.0))
						fitness *= ApplyFitnessEffectCallbacks(p_fitnessEffect_callbacks, male_index);
					
#ifdef SLIMGUI
					parent_individuals_[male_index]->cached_unscaled_fitness_ = fitness;
#endif
					
					fitness *= subpop_fitness_scaling;
					parent_individuals_[male_index]->cached_fitness_UNSAFE_ = fitness;
					totalMaleFitness += fitness;
				}
			}
			else
			{
				// general case for females without chromosomal fitness; shuffle buffer needed
				slim_popsize_t male_count = parent_subpop_size_ - parent_first_male_index_;
				slim_popsize_t *shuffle_buf = species_.BorrowShuffleBuffer(male_count);
				
				for (slim_popsize_t shuffle_index = 0; shuffle_index < male_count; shuffle_index++)
				{
					slim_popsize_t male_index = parent_first_male_index_ + shuffle_buf[shuffle_index];
					double fitness = parent_individuals_[male_index]->fitness_scaling_;
					
					if (fitnessEffect_callbacks_exist && (fitness > 0.0))
						fitness *= ApplyFitnessEffectCallbacks(p_fitnessEffect_callbacks, male_index);
					
#ifdef SLIMGUI
					parent_individuals_[male_index]->cached_unscaled_fitness_ = fitness;
#endif
					
					fitness *= subpop_fitness_scaling;
					parent_individuals_[male_index]->cached_fitness_UNSAFE_ = fitness;
					totalMaleFitness += fitness;
				}
				
				species_.ReturnShuffleBuffer();
			}
		}
		else
		{
			if (!needs_shuffle)
			{
				// FIXME should have some additional criterion for whether to go parallel with this, like the number of mutations
				if (!mutationEffect_callbacks_exist && !fitnessEffect_callbacks_exist)
				{
					// a separate loop for parallelization of the no-callback case
					// note that we rely on the fixup of non-neutral caches done above
					EIDOS_BENCHMARK_START(EidosBenchmarkType::k_FITNESS_SEX_3);
					EIDOS_THREAD_COUNT(gEidos_OMP_threads_FITNESS_SEX_3);
#pragma omp parallel for schedule(dynamic, 16) default(none) shared(parent_first_male_index_, parent_subpop_size_, subpop_fitness_scaling) reduction(+: totalMaleFitness) if(parent_subpop_size_ >= EIDOS_OMPMIN_FITNESS_SEX_3) num_threads(thread_count)
					for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
					{
						double fitness = parent_individuals_[male_index]->fitness_scaling_;
						
						if (fitness > 0.0)
						{
							fitness *= (this->*FitnessOfParent_TEMPLATED)(male_index, p_mutationEffect_callbacks);
							
#ifdef SLIMGUI
							parent_individuals_[male_index]->cached_unscaled_fitness_ = fitness;
#endif
							
							fitness *= subpop_fitness_scaling;
						}
						else
						{
#ifdef SLIMGUI
							parent_individuals_[male_index]->cached_unscaled_fitness_ = fitness;
#endif
						}
						
						parent_individuals_[male_index]->cached_fitness_UNSAFE_ = fitness;
						totalMaleFitness += fitness;
					}
					EIDOS_BENCHMARK_END(EidosBenchmarkType::k_FITNESS_SEX_3);
				}
				else	// at least one mutationEffect() or fitnessEffect() callback; not parallelized
				{
					for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
					{
						double fitness = parent_individuals_[male_index]->fitness_scaling_;
						
						if (fitness > 0.0)
						{
							fitness *= (this->*FitnessOfParent_TEMPLATED)(male_index, p_mutationEffect_callbacks);
							
							// multiply in the effects of any fitnessEffect() callbacks
							if (fitnessEffect_callbacks_exist && (fitness > 0.0))
								fitness *= ApplyFitnessEffectCallbacks(p_fitnessEffect_callbacks, male_index);
							
#ifdef SLIMGUI
							parent_individuals_[male_index]->cached_unscaled_fitness_ = fitness;
#endif
							
							fitness *= subpop_fitness_scaling;
						}
						else
						{
#ifdef SLIMGUI
							parent_individuals_[male_index]->cached_unscaled_fitness_ = fitness;
#endif
						}
						
						parent_individuals_[male_index]->cached_fitness_UNSAFE_ = fitness;
						totalMaleFitness += fitness;
					}
				}
			}
			else
			{
				// general case for males; we use the shuffle buffer to randomize processing order
				slim_popsize_t male_count = parent_subpop_size_ - parent_first_male_index_;
				slim_popsize_t *shuffle_buf = species_.BorrowShuffleBuffer(male_count);
				
				for (slim_popsize_t shuffle_index = 0; shuffle_index < male_count; shuffle_index++)
				{
					slim_popsize_t male_index = parent_first_male_index_ + shuffle_buf[shuffle_index];
					double fitness = parent_individuals_[male_index]->fitness_scaling_;
					
					if (fitness > 0.0)
					{
						fitness *= (this->*FitnessOfParent_TEMPLATED)(male_index, p_mutationEffect_callbacks);
						
						// multiply in the effects of any fitnessEffect() callbacks
						if (fitnessEffect_callbacks_exist && (fitness > 0.0))
							fitness *= ApplyFitnessEffectCallbacks(p_fitnessEffect_callbacks, male_index);
						
#ifdef SLIMGUI
						parent_individuals_[male_index]->cached_unscaled_fitness_ = fitness;
#endif
						
						fitness *= subpop_fitness_scaling;
					}
					else
					{
#ifdef SLIMGUI
						parent_individuals_[male_index]->cached_unscaled_fitness_ = fitness;
#endif
					}
					
					parent_individuals_[male_index]->cached_fitness_UNSAFE_ = fitness;
					totalMaleFitness += fitness;
				}
				
				species_.ReturnShuffleBuffer();
			}
		}
		
		totalFitness += totalMaleFitness;
		
		if (model_type_ == SLiMModelType::kModelTypeWF)
		{
			if (totalMaleFitness <= 0.0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of males is <= 0.0." << EidosTerminate(nullptr);
			if (!std::isfinite(totalFitness))
				EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of subpopulation is not finite; numerical error will prevent accurate simulation." << EidosTerminate(nullptr);
		}
	}
	else
	{
		if (pure_neutral)
		{
			if (Individual::s_any_individual_fitness_scaling_set_)
			{
				EIDOS_BENCHMARK_START(EidosBenchmarkType::k_FITNESS_ASEX_1);
				EIDOS_THREAD_COUNT(gEidos_OMP_threads_FITNESS_ASEX_1);
#pragma omp parallel for schedule(static) default(none) shared(parent_subpop_size_) firstprivate(subpop_fitness_scaling) reduction(+: totalFitness) if(parent_subpop_size_ >= EIDOS_OMPMIN_FITNESS_ASEX_1) num_threads(thread_count)
				for (slim_popsize_t individual_index = 0; individual_index < parent_subpop_size_; individual_index++)
				{
					double fitness = parent_individuals_[individual_index]->fitness_scaling_;
					
#ifdef SLIMGUI
					parent_individuals_[individual_index]->cached_unscaled_fitness_ = fitness;
#endif
					
					fitness *= subpop_fitness_scaling;
					parent_individuals_[individual_index]->cached_fitness_UNSAFE_ = fitness;
					totalFitness += fitness;
				}
				EIDOS_BENCHMARK_END(EidosBenchmarkType::k_FITNESS_ASEX_1);
			}
			else
			{
#ifdef SLIMGUI
				for (slim_popsize_t individual_index = 0; individual_index < parent_subpop_size_; individual_index++)
					parent_individuals_[individual_index]->cached_unscaled_fitness_ = 1.0;
#endif
				
				double fitness = subpop_fitness_scaling;	// no individual fitness_scaling_
				
				// Here we override setting up every cached_fitness_UNSAFE_ value, and set up a subpop-level cache instead.
				// This is why cached_fitness_UNSAFE_ is marked "UNSAFE".  See the header for details on this.
				if (model_type_ == SLiMModelType::kModelTypeWF)
				{
					individual_cached_fitness_OVERRIDE_ = true;
					individual_cached_fitness_OVERRIDE_value_ = fitness;
				}
				else
				{
					EIDOS_BENCHMARK_START(EidosBenchmarkType::k_FITNESS_ASEX_2);
					EIDOS_THREAD_COUNT(gEidos_OMP_threads_FITNESS_ASEX_2);
#pragma omp parallel for schedule(static) default(none) shared(parent_subpop_size_) firstprivate(fitness) if(parent_subpop_size_ >= EIDOS_OMPMIN_FITNESS_ASEX_2) num_threads(thread_count)
					for (slim_popsize_t individual_index = 0; individual_index < parent_subpop_size_; individual_index++)
					{
						parent_individuals_[individual_index]->cached_fitness_UNSAFE_ = fitness;
					}
					EIDOS_BENCHMARK_END(EidosBenchmarkType::k_FITNESS_ASEX_2);
				}
				
				totalFitness = fitness * parent_subpop_size_;
			}
		}
		else if (skip_chromosomal_fitness)
		{
			if (!needs_shuffle)
			{
				for (slim_popsize_t individual_index = 0; individual_index < parent_subpop_size_; individual_index++)
				{
					double fitness = parent_individuals_[individual_index]->fitness_scaling_;
					
					// multiply in the effects of any fitnessEffect() callbacks
					if (fitnessEffect_callbacks_exist && (fitness > 0.0))
						fitness *= ApplyFitnessEffectCallbacks(p_fitnessEffect_callbacks, individual_index);
					
#ifdef SLIMGUI
					parent_individuals_[individual_index]->cached_unscaled_fitness_ = fitness;
#endif
					
					fitness *= subpop_fitness_scaling;
					parent_individuals_[individual_index]->cached_fitness_UNSAFE_ = fitness;
					totalFitness += fitness;
				}
			}
			else
			{
				// general case for hermaphrodites without chromosomal fitness; shuffle buffer needed
				slim_popsize_t *shuffle_buf = species_.BorrowShuffleBuffer(parent_subpop_size_);
				
				for (slim_popsize_t shuffle_index = 0; shuffle_index < parent_subpop_size_; shuffle_index++)
				{
					slim_popsize_t individual_index = shuffle_buf[shuffle_index];
					double fitness = parent_individuals_[individual_index]->fitness_scaling_;
					
					// multiply in the effects of any fitnessEffect() callbacks
					if (fitnessEffect_callbacks_exist && (fitness > 0.0))
						fitness *= ApplyFitnessEffectCallbacks(p_fitnessEffect_callbacks, individual_index);
					
#ifdef SLIMGUI
					parent_individuals_[individual_index]->cached_unscaled_fitness_ = fitness;
#endif
					
					fitness *= subpop_fitness_scaling;
					parent_individuals_[individual_index]->cached_fitness_UNSAFE_ = fitness;
					totalFitness += fitness;
				}
				
				species_.ReturnShuffleBuffer();
			}
		}
		else
		{
			if (!needs_shuffle)
			{
				// FIXME should have some additional criterion for whether to go parallel with this, like the number of mutations
				if (!mutationEffect_callbacks_exist && !fitnessEffect_callbacks_exist)
				{
					// a separate loop for parallelization of the no-callback case
					
#if (defined(_OPENMP) && SLIM_USE_NONNEUTRAL_CACHES)
					// we need to fix the nonneutral caches in a separate pass first
					// because all the correct caches need to get flushed to everyone
					// before beginning fitness evaluation, for efficiency
					// beginend_nonneutral_pointers() handles the non-parallel case
					FixNonNeutralCaches_OMP();
#endif
					
					EIDOS_BENCHMARK_START(EidosBenchmarkType::k_FITNESS_ASEX_3);
					EIDOS_THREAD_COUNT(gEidos_OMP_threads_FITNESS_ASEX_3);
#pragma omp parallel for schedule(dynamic, 16) default(none) shared(parent_subpop_size_, subpop_fitness_scaling) reduction(+: totalFitness) if(parent_subpop_size_ >= EIDOS_OMPMIN_FITNESS_ASEX_3) num_threads(thread_count)
					for (slim_popsize_t individual_index = 0; individual_index < parent_subpop_size_; individual_index++)
					{
						double fitness = parent_individuals_[individual_index]->fitness_scaling_;
						
						if (fitness > 0.0)
						{
							fitness *= (this->*FitnessOfParent_TEMPLATED)(individual_index, p_mutationEffect_callbacks);
							
#ifdef SLIMGUI
							parent_individuals_[individual_index]->cached_unscaled_fitness_ = fitness;
#endif
							
							fitness *= subpop_fitness_scaling;
						}
						else
						{
#ifdef SLIMGUI
							parent_individuals_[individual_index]->cached_unscaled_fitness_ = fitness;
#endif
						}
						
						parent_individuals_[individual_index]->cached_fitness_UNSAFE_ = fitness;
						totalFitness += fitness;
					}
					EIDOS_BENCHMARK_END(EidosBenchmarkType::k_FITNESS_ASEX_3);
				}
				else	// at least one mutationEffect() or fitnessEffect() callback; not parallelized
				{
					for (slim_popsize_t individual_index = 0; individual_index < parent_subpop_size_; individual_index++)
					{
						double fitness = parent_individuals_[individual_index]->fitness_scaling_;
						
						if (fitness > 0.0)
						{
							fitness *= (this->*FitnessOfParent_TEMPLATED)(individual_index, p_mutationEffect_callbacks);
							
							// multiply in the effects of any fitnessEffect() callbacks
							if (fitnessEffect_callbacks_exist && (fitness > 0.0))
								fitness *= ApplyFitnessEffectCallbacks(p_fitnessEffect_callbacks, individual_index);
							
#ifdef SLIMGUI
							parent_individuals_[individual_index]->cached_unscaled_fitness_ = fitness;
#endif
							
							fitness *= subpop_fitness_scaling;
						}
						else
						{
#ifdef SLIMGUI
							parent_individuals_[individual_index]->cached_unscaled_fitness_ = fitness;
#endif
						}
						
						parent_individuals_[individual_index]->cached_fitness_UNSAFE_ = fitness;
						totalFitness += fitness;
					}
				}
			}
			else
			{
				// general case for hermaphrodites; we use the shuffle buffer to randomize processing order
				slim_popsize_t *shuffle_buf = species_.BorrowShuffleBuffer(parent_subpop_size_);
				
				for (slim_popsize_t shuffle_index = 0; shuffle_index < parent_subpop_size_; shuffle_index++)
				{
					slim_popsize_t individual_index = shuffle_buf[shuffle_index];
					double fitness = parent_individuals_[individual_index]->fitness_scaling_;
					
					if (fitness > 0.0)
					{
						fitness *= (this->*FitnessOfParent_TEMPLATED)(individual_index, p_mutationEffect_callbacks);
						
						// multiply in the effects of any fitnessEffect() callbacks
						if (fitnessEffect_callbacks_exist && (fitness > 0.0))
							fitness *= ApplyFitnessEffectCallbacks(p_fitnessEffect_callbacks, individual_index);
						
#ifdef SLIMGUI
						parent_individuals_[individual_index]->cached_unscaled_fitness_ = fitness;
#endif
						
						fitness *= subpop_fitness_scaling;
					}
					else
					{
#ifdef SLIMGUI
						parent_individuals_[individual_index]->cached_unscaled_fitness_ = fitness;
#endif
					}
					
					parent_individuals_[individual_index]->cached_fitness_UNSAFE_ = fitness;
					totalFitness += fitness;
				}
				
				species_.ReturnShuffleBuffer();
			}
		}
		
		if (model_type_ == SLiMModelType::kModelTypeWF)
		{
			if (totalFitness <= 0.0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of all individuals is <= 0.0." << EidosTerminate(nullptr);
			if (!std::isfinite(totalFitness))
				EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateFitness): total fitness of subpopulation is not finite; numerical error will prevent accurate simulation." << EidosTerminate(nullptr);
		}
	}
	
	if (model_type_ == SLiMModelType::kModelTypeWF)
		UpdateWFFitnessBuffers(pure_neutral && !Individual::s_any_individual_fitness_scaling_set_);
}

// WF only:
void Subpopulation::UpdateWFFitnessBuffers(bool p_pure_neutral)
{
	// This is called only by UpdateFitness(), after the fitness of all individuals has been updated, and only in WF models.
	// It updates the cached_parental_fitness_ and cached_male_fitness_ buffers, and then generates new lookup tables for mate choice.
	
	// Reallocate the fitness buffers to be large enough
	if (cached_fitness_capacity_ < parent_subpop_size_)
	{
		cached_parental_fitness_ = (double *)realloc(cached_parental_fitness_, sizeof(double) * parent_subpop_size_);
		if (!cached_parental_fitness_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateWFFitnessBuffers): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
		if (sex_enabled_)
		{
			cached_male_fitness_ = (double *)realloc(cached_male_fitness_, sizeof(double) * parent_subpop_size_);
			if (!cached_male_fitness_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::UpdateWFFitnessBuffers): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		}
		
		cached_fitness_capacity_ = parent_subpop_size_;
	}
	
	// Set up the fitness buffers with the new information
	if (individual_cached_fitness_OVERRIDE_)
	{
		// This is the optimized case, where all individuals have the same fitness and it is cached at the subpop level
		double universal_cached_fitness = individual_cached_fitness_OVERRIDE_value_;
		
		if (sex_enabled_)
		{
			for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
			{
				cached_parental_fitness_[female_index] = universal_cached_fitness;
				cached_male_fitness_[female_index] = 0;
			}
			
			for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
			{
				cached_parental_fitness_[male_index] = universal_cached_fitness;
				cached_male_fitness_[male_index] = universal_cached_fitness;
			}
		}
		else
		{
			for (slim_popsize_t i = 0; i < parent_subpop_size_; i++)
				cached_parental_fitness_[i] = universal_cached_fitness;
		}
	}
	else
	{
		// This is the normal case, where cached_fitness_UNSAFE_ has the cached fitness values for each individual
		if (sex_enabled_)
		{
			for (slim_popsize_t female_index = 0; female_index < parent_first_male_index_; female_index++)
			{
				double fitness = parent_individuals_[female_index]->cached_fitness_UNSAFE_;
				
				cached_parental_fitness_[female_index] = fitness;
				cached_male_fitness_[female_index] = 0;
			}
			
			for (slim_popsize_t male_index = parent_first_male_index_; male_index < parent_subpop_size_; male_index++)
			{
				double fitness = parent_individuals_[male_index]->cached_fitness_UNSAFE_;
				
				cached_parental_fitness_[male_index] = fitness;
				cached_male_fitness_[male_index] = fitness;
			}
		}
		else
		{
			for (slim_popsize_t i = 0; i < parent_subpop_size_; i++)
				cached_parental_fitness_[i] = parent_individuals_[i]->cached_fitness_UNSAFE_;
		}
	}
	
	cached_fitness_size_ = parent_subpop_size_;
	
	// Remake our mate-choice lookup tables
	if (sex_enabled_)
	{
		// throw out the old tables
		if (lookup_female_parent_)
		{
			gsl_ran_discrete_free(lookup_female_parent_);
			lookup_female_parent_ = nullptr;
		}
		
		if (lookup_male_parent_)
		{
			gsl_ran_discrete_free(lookup_male_parent_);
			lookup_male_parent_ = nullptr;
		}
		
		// in pure neutral models we don't set up the discrete preproc
		if (!p_pure_neutral)
		{
			lookup_female_parent_ = gsl_ran_discrete_preproc(parent_first_male_index_, cached_parental_fitness_);
			lookup_male_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_ - parent_first_male_index_, cached_parental_fitness_ + parent_first_male_index_);
		}
	}
	else
	{
		// throw out the old tables
		if (lookup_parent_)
		{
			gsl_ran_discrete_free(lookup_parent_);
			lookup_parent_ = nullptr;
		}
		
		// in pure neutral models we don't set up the discrete preproc
		if (!p_pure_neutral)
		{
			lookup_parent_ = gsl_ran_discrete_preproc(parent_subpop_size_, cached_parental_fitness_);
		}
	}
}

double Subpopulation::ApplyMutationEffectCallbacks(MutationIndex p_mutation, int p_homozygous, double p_computed_fitness, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks, Individual *p_individual)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Population::ApplyMutationEffectCallbacks(): running Eidos callback");
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	slim_objectid_t mutation_type_id = (gSLiM_Mutation_Block + p_mutation)->mutation_type_ptr_->mutation_type_id_;
	
	for (SLiMEidosBlock *mutationEffect_callback : p_mutationEffect_callbacks)
	{
		if (mutationEffect_callback->block_active_)
		{
			slim_objectid_t callback_mutation_type_id = mutationEffect_callback->mutation_type_id_;
			
			if ((callback_mutation_type_id == -1) || (callback_mutation_type_id == mutation_type_id))
			{
#ifndef DEBUG_POINTS_ENABLED
#error "DEBUG_POINTS_ENABLED is not defined; include eidos_globals.h"
#endif
#if DEBUG_POINTS_ENABLED
				// SLiMgui debugging point
				EidosDebugPointIndent indenter;
				
				{
					EidosInterpreterDebugPointsSet *debug_points = community_.DebugPoints();
					EidosToken *decl_token = mutationEffect_callback->root_node_->token_;
					
					if (debug_points && debug_points->set.size() && (decl_token->token_line_ != -1) &&
						(debug_points->set.find(decl_token->token_line_) != debug_points->set.end()))
					{
						SLIM_ERRSTREAM << EidosDebugPointIndent::Indent() << "#DEBUG mutationEffect(m" << mutationEffect_callback->mutation_type_id_;
						if (mutationEffect_callback->subpopulation_id_ != -1)
							SLIM_ERRSTREAM << ", p" << mutationEffect_callback->subpopulation_id_;
						SLIM_ERRSTREAM << ")";
						
						if (mutationEffect_callback->block_id_ != -1)
							SLIM_ERRSTREAM << " s" << mutationEffect_callback->block_id_;
						
						SLIM_ERRSTREAM << " (line " << (decl_token->token_line_ + 1) << community_.DebugPointInfo() << ")" << std::endl;
						indenter.indent();
					}
				}
#endif
				
				// The callback is active and matches the mutation type id of the mutation, so we need to execute it
				// This code is similar to Population::ExecuteScript, but we set up an additional symbol table, and we use the return value
				const EidosASTNode *compound_statement_node = mutationEffect_callback->compound_statement_node_;
				
				if (compound_statement_node->cached_return_value_)
				{
					// The script is a constant expression such as "{ return 1.1; }", so we can short-circuit it completely
					EidosValue_SP result_SP = compound_statement_node->cached_return_value_;
					EidosValue *result = result_SP.get();
					
					if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
						EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyMutationEffectCallbacks): mutationEffect() callbacks must provide a float singleton return value." << EidosTerminate(mutationEffect_callback->identifier_token_);
					
#if DEBUG
					// this checks the value type at runtime
					p_computed_fitness = result->FloatData()[0];
#else
					// unsafe cast for speed
					p_computed_fitness = ((EidosValue_Float *)result)->data()[0];
#endif
					
					// the cached value is owned by the tree, so we do not dispose of it
					// there is also no script output to handle
				}
				else if (mutationEffect_callback->has_cached_optimization_)
				{
					// We can special-case particular simple callbacks for speed.  This is similar to the cached_return_value_
					// mechanism above, but it is done in SLiM, not in Eidos, and is specific to callbacks, not general.
					// The has_cached_optimization_ flag is the umbrella flag for all such optimizations; we then figure
					// out below which cached optimization is in effect for this callback.  See Community::OptimizeScriptBlock()
					// for comments on the specific cases optimized here.
					if (mutationEffect_callback->has_cached_opt_reciprocal)
					{
						double A = mutationEffect_callback->cached_opt_A_;
						
						p_computed_fitness = (A / p_computed_fitness);	// p_computed_fitness is effect
					}
					else
					{
						EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyMutationEffectCallbacks): (internal error) cached optimization flag mismatch" << EidosTerminate(mutationEffect_callback->identifier_token_);
					}
				}
				else
				{
					// local variables for the callback parameters that we might need to allocate here, and thus need to free below
					EidosValue_Object local_mut(gSLiM_Mutation_Block + p_mutation, gSLiM_Mutation_Class);
					EidosValue_Float local_effect(p_computed_fitness);
					
					// We need to actually execute the script; we start a block here to manage the lifetime of the symbol table
					{
						EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &community_.SymbolTable());
						EidosSymbolTable client_symbols(EidosSymbolTableType::kLocalVariablesTable, &callback_symbols);
						EidosFunctionMap &function_map = community_.FunctionMap();
						EidosInterpreter interpreter(mutationEffect_callback->compound_statement_node_, client_symbols, function_map, &community_, SLIM_OUTSTREAM, SLIM_ERRSTREAM);
						
						if (mutationEffect_callback->contains_self_)
							callback_symbols.InitializeConstantSymbolEntry(mutationEffect_callback->SelfSymbolTableEntry());		// define "self"
						
						// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
						// We can use that method because we know the lifetime of the symbol table is shorter than that of
						// the value objects, and we know that the values we are setting here will not change (the objects
						// referred to by the values may change, but the values themselves will not change).
						if (mutationEffect_callback->contains_mut_)
						{
							local_mut.StackAllocated();			// prevent Eidos_intrusive_ptr from trying to delete this
							callback_symbols.InitializeConstantSymbolEntry(gID_mut, EidosValue_SP(&local_mut));
						}
						if (mutationEffect_callback->contains_effect_)
						{
							local_effect.StackAllocated();		// prevent Eidos_intrusive_ptr from trying to delete this
							callback_symbols.InitializeConstantSymbolEntry(gID_effect, EidosValue_SP(&local_effect));
						}
						if (mutationEffect_callback->contains_individual_)
							callback_symbols.InitializeConstantSymbolEntry(gID_individual, p_individual->CachedEidosValue());
						if (mutationEffect_callback->contains_subpop_)
							callback_symbols.InitializeConstantSymbolEntry(gID_subpop, SymbolTableEntry().second);
						
						// p_homozygous == -1 means the mutation faces a null haplosome; otherwise, 0 means heterozyg., 1 means homozyg.
						// that gets translated into Eidos values of NULL, F, and T, respectively
						if (mutationEffect_callback->contains_homozygous_)
						{
							if (p_homozygous == -1)
								callback_symbols.InitializeConstantSymbolEntry(gID_homozygous, gStaticEidosValueNULL);
							else
								callback_symbols.InitializeConstantSymbolEntry(gID_homozygous, (p_homozygous != 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
						}
						
						try
						{
							// Interpret the script; the result from the interpretation must be a singleton double used as a new fitness value
							EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(mutationEffect_callback->script_);
							EidosValue *result = result_SP.get();
							
							if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
								EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyMutationEffectCallbacks): mutationEffect() callbacks must provide a float singleton return value." << EidosTerminate(mutationEffect_callback->identifier_token_);
							
#if DEBUG
							// this checks the value type at runtime
							p_computed_fitness = result->FloatData()[0];
#else
							// unsafe cast for speed
							p_computed_fitness = ((EidosValue_Float *)result)->data()[0];
#endif
						}
						catch (...)
						{
							throw;
						}
						
					}
				}
			}
		}
	}
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMutationEffectCallback)]);
#endif
	
	return p_computed_fitness;
}

double Subpopulation::ApplyFitnessEffectCallbacks(std::vector<SLiMEidosBlock*> &p_fitnessEffect_callbacks, slim_popsize_t p_individual_index)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Population::ApplyFitnessEffectCallbacks(): running Eidos callback");
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	double computed_fitness = 1.0;
	Individual *individual = parent_individuals_[p_individual_index];
	
	for (SLiMEidosBlock *fitnessEffect_callback : p_fitnessEffect_callbacks)
	{
		if (fitnessEffect_callback->block_active_)
		{
#if DEBUG_POINTS_ENABLED
			// SLiMgui debugging point
			EidosDebugPointIndent indenter;
			
			{
				EidosInterpreterDebugPointsSet *debug_points = community_.DebugPoints();
				EidosToken *decl_token = fitnessEffect_callback->root_node_->token_;
				
				if (debug_points && debug_points->set.size() && (decl_token->token_line_ != -1) &&
					(debug_points->set.find(decl_token->token_line_) != debug_points->set.end()))
				{
					SLIM_ERRSTREAM << EidosDebugPointIndent::Indent() << "#DEBUG fitnessEffect(";
					if (fitnessEffect_callback->subpopulation_id_ != -1)
						SLIM_ERRSTREAM << "p" << fitnessEffect_callback->subpopulation_id_;
					SLIM_ERRSTREAM << ")";
					
					if (fitnessEffect_callback->block_id_ != -1)
						SLIM_ERRSTREAM << " s" << fitnessEffect_callback->block_id_;
					
					SLIM_ERRSTREAM << " (line " << (decl_token->token_line_ + 1) << community_.DebugPointInfo() << ")" << std::endl;
					indenter.indent();
				}
			}
#endif
						
			// The callback is active, so we need to execute it
			// This code is similar to Population::ExecuteScript, but we set up an additional symbol table, and we use the return value
			const EidosASTNode *compound_statement_node = fitnessEffect_callback->compound_statement_node_;
			
			if (compound_statement_node->cached_return_value_)
			{
				// The script is a constant expression such as "{ return 1.1; }", so we can short-circuit it completely
				EidosValue_SP result_SP = compound_statement_node->cached_return_value_;
				EidosValue *result = result_SP.get();
				
				if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
					EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyFitnessEffectCallbacks): fitnessEffect() callbacks must provide a float singleton return value." << EidosTerminate(fitnessEffect_callback->identifier_token_);
				
#if DEBUG
				// this checks the value type at runtime
				computed_fitness *= result->FloatData()[0];
#else
				// unsafe cast for speed
				computed_fitness *= ((EidosValue_Float *)result)->data()[0];
#endif
				
				// the cached value is owned by the tree, so we do not dispose of it
				// there is also no script output to handle
			}
			else if (fitnessEffect_callback->has_cached_optimization_)
			{
				// We can special-case particular simple callbacks for speed.  This is similar to the cached_return_value_
				// mechanism above, but it is done in SLiM, not in Eidos, and is specific to callbacks, not general.
				// The has_cached_optimization_ flag is the umbrella flag for all such optimizations; we then figure
				// out below which cached optimization is in effect for this callback.  See Community::OptimizeScriptBlock()
				// for comments on the specific cases optimized here.
				if (fitnessEffect_callback->has_cached_opt_dnorm1_)
				{
					double A = fitnessEffect_callback->cached_opt_A_;
					double B = fitnessEffect_callback->cached_opt_B_;
					double C = fitnessEffect_callback->cached_opt_C_;
					double D = fitnessEffect_callback->cached_opt_D_;
					
					computed_fitness *= (D + (gsl_ran_gaussian_pdf(individual->tagF_value_ - A, B) / C));
				}
				else
				{
					EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyFitnessEffectCallbacks): (internal error) cached optimization flag mismatch" << EidosTerminate(fitnessEffect_callback->identifier_token_);
				}
			}
			else
			{
				// We need to actually execute the script; we start a block here to manage the lifetime of the symbol table
				{
					EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &community_.SymbolTable());
					EidosSymbolTable client_symbols(EidosSymbolTableType::kLocalVariablesTable, &callback_symbols);
					EidosFunctionMap &function_map = community_.FunctionMap();
					EidosInterpreter interpreter(fitnessEffect_callback->compound_statement_node_, client_symbols, function_map, &community_, SLIM_OUTSTREAM, SLIM_ERRSTREAM);
					
					if (fitnessEffect_callback->contains_self_)
						callback_symbols.InitializeConstantSymbolEntry(fitnessEffect_callback->SelfSymbolTableEntry());		// define "self"
					
					// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
					// We can use that method because we know the lifetime of the symbol table is shorter than that of
					// the value objects, and we know that the values we are setting here will not change (the objects
					// referred to by the values may change, but the values themselves will not change).
					if (fitnessEffect_callback->contains_individual_)
						callback_symbols.InitializeConstantSymbolEntry(gID_individual, individual->CachedEidosValue());
					if (fitnessEffect_callback->contains_subpop_)
						callback_symbols.InitializeConstantSymbolEntry(gID_subpop, SymbolTableEntry().second);
					
					try
					{
						// Interpret the script; the result from the interpretation must be a singleton double used as a new fitness value
						EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(fitnessEffect_callback->script_);
						EidosValue *result = result_SP.get();
						
						if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
							EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyFitnessEffectCallbacks): fitnessEffect() callbacks must provide a float singleton return value." << EidosTerminate(fitnessEffect_callback->identifier_token_);
						
#if DEBUG
						// this checks the value type at runtime
						computed_fitness *= result->FloatData()[0];
#else
						// unsafe cast for speed
						computed_fitness *= ((EidosValue_Float *)result)->data()[0];
#endif
					}
					catch (...)
					{
						// There is an error-reporting coordinate system problem here.  The error might have
						// occurred directly in the callback, or in something the callback called, like a
						// user-defined function or a lambda.  Also, we might have been called directly by
						// the SLiM core, and have the user script's coordinates set up, or the call to us
						// might have been triggered by a call like recalculateFitness() called in an event
						// or from a user-defined function.  So there are all sorts of possibilities on both
						// ends.  Since we didn't save and set the error context, the way calls to user-
						// defined functions and lambdas do (because it would be slow), we don't really have
						// a way to repair the situation now, I don't think.  We can't even tell whether
						// things are screwed up or not.  We will do nothing.  Most of the time, the error
						// was directly in our code and we were called by SLiM or an event, so everything
						// is in user script coordinates and we'll get away with it.  In very unusual
						// circumstances, the error position reported will be wrong in the GUI.  Fixing that
						// would require a total redesign of the error-tracking mechanism.  BCH 3/12/2025
						throw;
					}
					
				}
			}
			
			// If any callback puts us at or below zero, we can short-circuit the rest
			if (computed_fitness <= 0.0)
			{
				computed_fitness = 0.0;
				break;
			}
		}
	}
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosFitnessEffectCallback)]);
#endif
	
	return computed_fitness;
}

// FitnessOfParent() has three templated versions, for no callbacks, a single callback, and multiple callbacks.  That
// pattern extends downward to _Fitness_DiploidChromosome() and _Fitness_HaploidChromosome(), which is the level where
// it actually matters; in FitnessOfParent() the template flags just get passed through.  The goal of this design is
// twofold.  First, it allows the case without mutationEffect() callbacks to run at full speed.  Second, it allows the
// single-callback case to be optimized in a special way.  When there is just a single callback, it usually refers to
// a mutation type that is relatively uncommon.  The model might have neutral mutations in most cases, plus a rare
// (or unique) mutation type that is subject to more complex selection, for example.  We can optimize that very common
// case substantially by making the callout to ApplyMutationEffectCallbacks() only for mutations of the mutation type
// that the callback modifies.  This pays off mostly when there are many common mutations with no callback, plus one
// rare mutation type that has a callback.  A model of neutral drift across a long chromosome with a high mutation
// rate, with an introduced beneficial mutation with a selection coefficient extremely close to 0, for example, would
// hit this case hard and see a speedup of as much as 25%, so the additional complexity seems worth it (since that's
// quite a realistic and common case).  The only unfortunate thing about this design is that p_mutationEffect_callbacks
// has to get passed all the way down, even when we know we won't use it.  LTO might optimize that away, with luck.
template <const bool f_mutrunexps, const bool f_callbacks, const bool f_singlecallback>
double Subpopulation::FitnessOfParent(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks)
{
	// calculate the fitness of the individual at index p_individual_index in the parent population
	// this loops through all chromosomes, handling ploidy and callbacks as needed
	double w = 1.0;
	Individual *individual = parent_individuals_[p_individual_index];
	int haplosome_index = 0;
	
	for (Chromosome *chromosome : species_.Chromosomes())
	{
		if (f_mutrunexps) chromosome->StartMutationRunExperimentClock();
		
		switch (chromosome->Type())
		{
				// diploid, possibly with one or both being null haplosomes
			case ChromosomeType::kA_DiploidAutosome:
			case ChromosomeType::kX_XSexChromosome:
			case ChromosomeType::kZ_ZSexChromosome:
			{
				Haplosome *haplosome1 = individual->haplosomes_[haplosome_index];
				Haplosome *haplosome2 = individual->haplosomes_[haplosome_index+1];
				
				w *= _Fitness_DiploidChromosome<f_callbacks, f_singlecallback>(haplosome1, haplosome2, p_mutationEffect_callbacks);
				if (w <= 0.0) {
					if (f_mutrunexps) chromosome->StopMutationRunExperimentClock("FitnessOfParent()");
					return 0.0;
				}
				
				haplosome_index += 2;
				break;
			}
				
				// haploid, possibly null
			case ChromosomeType::kH_HaploidAutosome:
			case ChromosomeType::kY_YSexChromosome:
			case ChromosomeType::kW_WSexChromosome:
			case ChromosomeType::kHF_HaploidFemaleInherited:
			case ChromosomeType::kFL_HaploidFemaleLine:
			case ChromosomeType::kHM_HaploidMaleInherited:
			case ChromosomeType::kML_HaploidMaleLine:
			{
				Haplosome *haplosome = individual->haplosomes_[haplosome_index];
				
				w *= _Fitness_HaploidChromosome<f_callbacks, f_singlecallback>(haplosome, p_mutationEffect_callbacks);
				if (w <= 0.0) {
					if (f_mutrunexps) chromosome->StopMutationRunExperimentClock("FitnessOfParent()");
					return 0.0;
				}
				
				haplosome_index += 1;
				break;
			}
				
				// special cases: haploid but with an accompanying null
			case ChromosomeType::kHNull_HaploidAutosomeWithNull:
			{
				Haplosome *haplosome = individual->haplosomes_[haplosome_index];
				
				w *= _Fitness_HaploidChromosome<f_callbacks, f_singlecallback>(haplosome, p_mutationEffect_callbacks);
				if (w <= 0.0) {
					if (f_mutrunexps) chromosome->StopMutationRunExperimentClock("FitnessOfParent()");
					return 0.0;
				}
				
				haplosome_index += 2;
				break;
			}
			case ChromosomeType::kNullY_YSexChromosomeWithNull:
			{
				Haplosome *haplosome = individual->haplosomes_[haplosome_index+1];
				
				w *= _Fitness_HaploidChromosome<f_callbacks, f_singlecallback>(haplosome, p_mutationEffect_callbacks);
				if (w <= 0.0) {
					if (f_mutrunexps) chromosome->StopMutationRunExperimentClock("FitnessOfParent()");
					return 0.0;
				}
				
				haplosome_index += 2;
				break;
			}
		}
		
		if (f_mutrunexps) chromosome->StopMutationRunExperimentClock("FitnessOfParent()");
	}
	
	return w;
}

template double Subpopulation::FitnessOfParent<false, false, false>(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
template double Subpopulation::FitnessOfParent<false, true, false>(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
template double Subpopulation::FitnessOfParent<false, true, true>(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
template double Subpopulation::FitnessOfParent<true, false, false>(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
template double Subpopulation::FitnessOfParent<true, true, false>(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
template double Subpopulation::FitnessOfParent<true, true, true>(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);

template <const bool f_callbacks, const bool f_singlecallback>
double Subpopulation::_Fitness_DiploidChromosome(Haplosome *haplosome1, Haplosome *haplosome2, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks)
{
	double w = 1.0;
	bool haplosome1_null = haplosome1->IsNull();
	bool haplosome2_null = haplosome2->IsNull();
	
#if SLIM_USE_NONNEUTRAL_CACHES
	int32_t nonneutral_change_counter = species_.nonneutral_change_counter_;
	int32_t nonneutral_regime = species_.last_nonneutral_regime_;
#endif
	
	// resolve the mutation type for the single callback case; we don't pass this in to keep the non-callback case simple and fast
	MutationType *single_callback_mut_type;
	
	if (f_singlecallback)
	{
		// our caller already did this lookup, to select this case, so this lookup is guaranteed to succeed
		slim_objectid_t mutation_type_id = p_mutationEffect_callbacks[0]->mutation_type_id_;
		
		single_callback_mut_type = species_.MutationTypeWithID(mutation_type_id);
	}
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	if (haplosome1_null && haplosome2_null)
	{
		// both haplosomes are null placeholders; no mutations, no fitness effects
		return w;
	}
	else if (haplosome1_null || haplosome2_null)
	{
		// one haplosome is null, so we just need to scan through the non-null haplosome and account
		// for its mutations, including the hemizygous dominance coefficient
		const Haplosome *haplosome = haplosome1_null ? haplosome2 : haplosome1;
		const int32_t mutrun_count = haplosome->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			const MutationRun *mutrun = haplosome->mutruns_[run_index];
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *haplosome_iter, *haplosome_max;
			
			mutrun->beginend_nonneutral_pointers(&haplosome_iter, &haplosome_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *haplosome_iter = mutrun->begin_pointer_const();
			const MutationIndex *haplosome_max = mutrun->end_pointer_const();
#endif
			
			// with an unpaired chromosome, we multiply each selection coefficient by the hemizygous dominance coefficient
			// this is for a single X chromosome in a male, for example; dosage compensation, as opposed to heterozygosity
			while (haplosome_iter != haplosome_max)
			{
				MutationIndex haplosome_mutindex = *haplosome_iter++;
				Mutation *mutation = mut_block_ptr + haplosome_mutindex;
				
				if (f_callbacks && (!f_singlecallback || (mutation->mutation_type_ptr_ == single_callback_mut_type)))
				{
					w *= ApplyMutationEffectCallbacks(haplosome_mutindex, -1, mutation->cached_one_plus_hemizygousdom_sel_, p_mutationEffect_callbacks, haplosome->individual_);
					
					if (w <= 0.0)
						return 0.0;
				}
				else
				{
					w *= mutation->cached_one_plus_hemizygousdom_sel_;
				}
			}
		}
		
		return w;
	}
	else
	{
		// both haplosomes are being modeled, so we need to scan through and figure out which mutations are heterozygous and which are homozygous
		const int32_t mutrun_count = haplosome1->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			const MutationRun *mutrun1 = haplosome1->mutruns_[run_index];
			const MutationRun *mutrun2 = haplosome2->mutruns_[run_index];
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *haplosome1_iter, *haplosome2_iter, *haplosome1_max, *haplosome2_max;
			
			mutrun1->beginend_nonneutral_pointers(&haplosome1_iter, &haplosome1_max, nonneutral_change_counter, nonneutral_regime);
			mutrun2->beginend_nonneutral_pointers(&haplosome2_iter, &haplosome2_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *haplosome1_iter = mutrun1->begin_pointer_const();
			const MutationIndex *haplosome2_iter = mutrun2->begin_pointer_const();
			
			const MutationIndex *haplosome1_max = mutrun1->end_pointer_const();
			const MutationIndex *haplosome2_max = mutrun2->end_pointer_const();
#endif
			
			// first, handle the situation before either haplosome iterator has reached the end of its haplosome, for simplicity/speed
			if (haplosome1_iter != haplosome1_max && haplosome2_iter != haplosome2_max)
			{
				MutationIndex haplosome1_mutindex = *haplosome1_iter, haplosome2_mutindex = *haplosome2_iter;
				slim_position_t haplosome1_iter_position = (mut_block_ptr + haplosome1_mutindex)->position_, haplosome2_iter_position = (mut_block_ptr + haplosome2_mutindex)->position_;
				
				do
				{
					if (haplosome1_iter_position < haplosome2_iter_position)
					{
						// Process a mutation in haplosome1 since it is leading
						Mutation *mutation = mut_block_ptr + haplosome1_mutindex;
						
						if (f_callbacks && (!f_singlecallback || (mutation->mutation_type_ptr_ == single_callback_mut_type)))
						{
							w *= ApplyMutationEffectCallbacks(haplosome1_mutindex, false, mutation->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, haplosome1->individual_);
							
							if (w <= 0.0)
								return 0.0;
						}
						else
						{
							w *= mutation->cached_one_plus_dom_sel_;
						}
						
						if (++haplosome1_iter == haplosome1_max)
							break;
						else {
							haplosome1_mutindex = *haplosome1_iter;
							haplosome1_iter_position = (mut_block_ptr + haplosome1_mutindex)->position_;
						}
					}
					else if (haplosome1_iter_position > haplosome2_iter_position)
					{
						// Process a mutation in haplosome2 since it is leading
						Mutation *mutation = mut_block_ptr + haplosome2_mutindex;
						
						if (f_callbacks && (!f_singlecallback || (mutation->mutation_type_ptr_ == single_callback_mut_type)))
						{
							w *= ApplyMutationEffectCallbacks(haplosome2_mutindex, false, mutation->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, haplosome1->individual_);
							
							if (w <= 0.0)
								return 0.0;
						}
						else
						{
							w *= mutation->cached_one_plus_dom_sel_;
						}
						
						if (++haplosome2_iter == haplosome2_max)
							break;
						else {
							haplosome2_mutindex = *haplosome2_iter;
							haplosome2_iter_position = (mut_block_ptr + haplosome2_mutindex)->position_;
						}
					}
					else
					{
						// Look for homozygosity: haplosome1_iter_position == haplosome2_iter_position
						slim_position_t position = haplosome1_iter_position;
						const MutationIndex *haplosome1_start = haplosome1_iter;
						
						// advance through haplosome1 as long as we remain at the same position, handling one mutation at a time
						do
						{
							const MutationIndex *haplosome2_matchscan = haplosome2_iter; 
							
							// advance through haplosome2 with haplosome2_matchscan, looking for a match for the current mutation in haplosome1, to determine whether we are homozygous or not
							while (haplosome2_matchscan != haplosome2_max && (mut_block_ptr + *haplosome2_matchscan)->position_ == position)
							{
								if (haplosome1_mutindex == *haplosome2_matchscan)
								{
									// a match was found, so we multiply our fitness by the full selection coefficient
									Mutation *mutation = mut_block_ptr + haplosome1_mutindex;
									
									if (f_callbacks && (!f_singlecallback || (mutation->mutation_type_ptr_ == single_callback_mut_type)))
									{
										w *= ApplyMutationEffectCallbacks(haplosome1_mutindex, true, mutation->cached_one_plus_sel_, p_mutationEffect_callbacks, haplosome1->individual_);
										
										if (w <= 0.0)
											return 0.0;
									}
									else
									{
										w *= mutation->cached_one_plus_sel_;
									}
									goto homozygousExit1;
								}
								
								haplosome2_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							{
								Mutation *mutation = mut_block_ptr + haplosome1_mutindex;
								
								if (f_callbacks && (!f_singlecallback || (mutation->mutation_type_ptr_ == single_callback_mut_type)))
								{
									w *= ApplyMutationEffectCallbacks(haplosome1_mutindex, false, mutation->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, haplosome1->individual_);
									
									if (w <= 0.0)
										return 0.0;
								}
								else
								{
									w *= mutation->cached_one_plus_dom_sel_;
								}
							}
							
						homozygousExit1:
							
							if (++haplosome1_iter == haplosome1_max)
								break;
							else {
								haplosome1_mutindex = *haplosome1_iter;
								haplosome1_iter_position = (mut_block_ptr + haplosome1_mutindex)->position_;
							}
						} while (haplosome1_iter_position == position);
						
						// advance through haplosome2 as long as we remain at the same position, handling one mutation at a time
						do
						{
							const MutationIndex *haplosome1_matchscan = haplosome1_start; 
							
							// advance through haplosome1 with haplosome1_matchscan, looking for a match for the current mutation in haplosome2, to determine whether we are homozygous or not
							while (haplosome1_matchscan != haplosome1_max && (mut_block_ptr + *haplosome1_matchscan)->position_ == position)
							{
								if (haplosome2_mutindex == *haplosome1_matchscan)
								{
									// a match was found; we know this match was already found by the haplosome1 loop above, so our fitness has already been multiplied appropriately
									goto homozygousExit2;
								}
								
								haplosome1_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							{
								Mutation *mutation = mut_block_ptr + haplosome2_mutindex;
								
								if (f_callbacks && (!f_singlecallback || (mutation->mutation_type_ptr_ == single_callback_mut_type)))
								{
									w *= ApplyMutationEffectCallbacks(haplosome2_mutindex, false, mutation->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, haplosome1->individual_);
									
									if (w <= 0.0)
										return 0.0;
								}
								else
								{
									w *= mutation->cached_one_plus_dom_sel_;
								}
							}
							
						homozygousExit2:
							
							if (++haplosome2_iter == haplosome2_max)
								break;
							else {
								haplosome2_mutindex = *haplosome2_iter;
								haplosome2_iter_position = (mut_block_ptr + haplosome2_mutindex)->position_;
							}
						} while (haplosome2_iter_position == position);
						
						// break out if either haplosome has reached its end
						if (haplosome1_iter == haplosome1_max || haplosome2_iter == haplosome2_max)
							break;
					}
				} while (true);
			}
			
			// one or the other haplosome has now reached its end, so now we just need to handle the remaining mutations in the unfinished haplosome
#if DEBUG
			assert(!(haplosome1_iter != haplosome1_max && haplosome2_iter != haplosome2_max));
#endif
			
			// if haplosome1 is unfinished, finish it
			while (haplosome1_iter != haplosome1_max)
			{
				MutationIndex haplosome1_mutindex = *haplosome1_iter++;
				Mutation *mutation = mut_block_ptr + haplosome1_mutindex;
				
				if (f_callbacks && (!f_singlecallback || (mutation->mutation_type_ptr_ == single_callback_mut_type)))
				{
					w *= ApplyMutationEffectCallbacks(haplosome1_mutindex, false, mutation->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, haplosome1->individual_);
					
					if (w <= 0.0)
						return 0.0;
				}
				else
				{
					w *= mutation->cached_one_plus_dom_sel_;
				}
			}
			
			// if haplosome2 is unfinished, finish it
			while (haplosome2_iter != haplosome2_max)
			{
				MutationIndex haplosome2_mutindex = *haplosome2_iter++;
				Mutation *mutation = mut_block_ptr + haplosome2_mutindex;
				
				if (f_callbacks && (!f_singlecallback || (mutation->mutation_type_ptr_ == single_callback_mut_type)))
				{
					w *= ApplyMutationEffectCallbacks(haplosome2_mutindex, false, mutation->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, haplosome1->individual_);
					
					if (w <= 0.0)
						return 0.0;
				}
				else
				{
					w *= mutation->cached_one_plus_dom_sel_;
				}
			}
		}
		
		return w;
	}
}

template double Subpopulation::_Fitness_DiploidChromosome<false, false>(Haplosome *haplosome1, Haplosome *haplosome2, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
template double Subpopulation::_Fitness_DiploidChromosome<true, false>(Haplosome *haplosome1, Haplosome *haplosome2, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
template double Subpopulation::_Fitness_DiploidChromosome<true, true>(Haplosome *haplosome1, Haplosome *haplosome2, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);

template <const bool f_callbacks, const bool f_singlecallback>
double Subpopulation::_Fitness_HaploidChromosome(Haplosome *haplosome, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks)
{
	if (haplosome->IsNull())
	{
		// the haplosome is a null placeholder; no mutations, no fitness effects
		return 1.0;
	}
	else
	{
		// we just need to scan through the haplosome and account for its mutations,
		// using the homozygous fitness effect (no dominance effects with haploidy)
#if SLIM_USE_NONNEUTRAL_CACHES
		int32_t nonneutral_change_counter = species_.nonneutral_change_counter_;
		int32_t nonneutral_regime = species_.last_nonneutral_regime_;
#endif
		
		// resolve the mutation type for the single callback case; we don't pass this in to keep the non-callback case simple and fast
		MutationType *single_callback_mut_type;
		
		if (f_singlecallback)
		{
			// our caller already did this lookup, to select this case, so this lookup is guaranteed to succeed
			slim_objectid_t mutation_type_id = p_mutationEffect_callbacks[0]->mutation_type_id_;
			
			single_callback_mut_type = species_.MutationTypeWithID(mutation_type_id);
		}
		
		Mutation *mut_block_ptr = gSLiM_Mutation_Block;
		const int32_t mutrun_count = haplosome->mutrun_count_;
		double w = 1.0;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			const MutationRun *mutrun = haplosome->mutruns_[run_index];
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *haplosome_iter, *haplosome_max;
			
			mutrun->beginend_nonneutral_pointers(&haplosome_iter, &haplosome_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *haplosome_iter = mutrun->begin_pointer_const();
			const MutationIndex *haplosome_max = mutrun->end_pointer_const();
#endif
			
			// with a haploid chromosome, we use the homozygous fitness effect
			while (haplosome_iter != haplosome_max)
			{
				MutationIndex haplosome_mutation = *haplosome_iter++;
				Mutation *mutation = (mut_block_ptr + haplosome_mutation);
				
				if (f_callbacks && (!f_singlecallback || (mutation->mutation_type_ptr_ == single_callback_mut_type)))
				{
					w *= ApplyMutationEffectCallbacks(haplosome_mutation, -1, mutation->cached_one_plus_sel_, p_mutationEffect_callbacks, haplosome->individual_);
					
					if (w <= 0.0)
						return 0.0;
				}
				else
				{
					w *= mutation->cached_one_plus_sel_;
				}
			}
		}
		
		return w;
	}
}

template double Subpopulation::_Fitness_HaploidChromosome<false, false>(Haplosome *haplosome, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
template double Subpopulation::_Fitness_HaploidChromosome<true, false>(Haplosome *haplosome, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);
template double Subpopulation::_Fitness_HaploidChromosome<true, true>(Haplosome *haplosome, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks);

// WF only:
void Subpopulation::TallyLifetimeReproductiveOutput(void)
{
	if (species_.PedigreesEnabled())
	{
		lifetime_reproductive_output_MH_.resize(0);
		lifetime_reproductive_output_F_.resize(0);
		
		if (species_.SexEnabled())
		{
			for (Individual *ind : parent_individuals_)
			{
				if (ind->sex_ == IndividualSex::kFemale)
					lifetime_reproductive_output_F_.emplace_back(ind->reproductive_output_);
				else
					lifetime_reproductive_output_MH_.emplace_back(ind->reproductive_output_);
			}
		}
		else
		{
			for (Individual *ind : parent_individuals_)
				lifetime_reproductive_output_MH_.emplace_back(ind->reproductive_output_);
		}
	}
}

void Subpopulation::SwapChildAndParentHaplosomes(void)
{
	bool will_need_new_children = false;
	
	// If there are any differences between the parent and child haplosome setups (due to change in subpop size, sex ratio, etc.), we will need to create new child haplosomes after swapping
	// This is because the parental haplosomes, which are based on the old parental values, will get swapped in to the children, but they will be out of date.
	if ((parent_subpop_size_ != child_subpop_size_) || (parent_sex_ratio_ != child_sex_ratio_) || (parent_first_male_index_ != child_first_male_index_))
		will_need_new_children = true;
	
	// Execute the swap of the individuals
	child_individuals_.swap(parent_individuals_);
	cached_parent_individuals_value_.reset();
	
	// Clear out any dictionary values and color values stored in what are now the child individuals; since this is per-individual it
	// takes a significant amount of time, so we try to minimize the overhead by doing it only when these facilities have been used
	// BCH 6 April 2019: likewise, now, with resetting tags in individuals and haplosomes to the "unset" value
	// BCH 21 November 2020: likewise, now, for resetting reproductive output
	if (Individual::s_any_individual_dictionary_set_)
	{
		for (Individual *child : child_individuals_)
			child->RemoveAllKeys();
			// no call to ContentsChanged() here, for speed; we know child is a Dictionary not a DataFrame
	}
	
	if (Individual::s_any_individual_color_set_)
	{
		for (Individual *child : child_individuals_)
			child->ClearColor();
	}
	
	if (Individual::s_any_individual_tag_set_ || Individual::s_any_individual_tagF_set_)
	{
		for (Individual *child : child_individuals_)
		{
			child->tag_value_ = SLIM_TAG_UNSET_VALUE;
			child->tagF_value_ = SLIM_TAGF_UNSET_VALUE;
		}
	}
	if (Individual::s_any_individual_tagL_set_)
	{
		for (Individual *child : child_individuals_)
		{
			child->tagL0_set_ = false;
			child->tagL1_set_ = false;
			child->tagL2_set_ = false;
			child->tagL3_set_ = false;
			child->tagL4_set_ = false;
		}
	}
	if (Individual::s_any_haplosome_tag_set_)
	{
		int haplosome_count_per_individual = HaplosomeCountPerIndividual();
		
		for (Individual *child : child_individuals_)
		{
			Haplosome **haplosomes = child->haplosomes_;
			
			for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
			{
				Haplosome *haplosome = haplosomes[haplosome_index];
				
				haplosome->tag_value_ = SLIM_TAG_UNSET_VALUE;
			}
		}
	}
	
	if (species_.PedigreesEnabled())
	{
		for (Individual *child : child_individuals_)
			child->reproductive_output_ = 0;
	}
	
	// The parents now have the values that used to belong to the children.
	parent_subpop_size_ = child_subpop_size_;
	parent_sex_ratio_ = child_sex_ratio_;
	parent_first_male_index_ = child_first_male_index_;
	
	// The parental haplosomes, which have now been swapped into the child haplosome vactor, no longer fit the bill.  We need to throw them out and generate new haplosome vectors.
	if (will_need_new_children)
		GenerateChildrenToFitWF();
}

template <const bool f_mutrunexps, const bool f_pedigree_rec, const bool f_treeseq, const bool f_callbacks, const bool f_spatial>
Individual *Subpopulation::GenerateIndividualCrossed(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex)
{
	Subpopulation &parent1_subpop = *p_parent1->subpopulation_;
	Subpopulation &parent2_subpop = *p_parent2->subpopulation_;
	
#if DEBUG
	IndividualSex parent1_sex = p_parent1->sex_;
	IndividualSex parent2_sex = p_parent2->sex_;
	
	if ((sex_enabled_ && (parent1_sex != IndividualSex::kFemale)) || (!sex_enabled_ && (parent1_sex != IndividualSex::kHermaphrodite)))
		EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualCrossed): parent1 must be female in sexual models, or hermaphroditic in non-sexual models." << EidosTerminate();
	if ((sex_enabled_ && (parent2_sex != IndividualSex::kMale)) || (!sex_enabled_ && (parent2_sex != IndividualSex::kHermaphrodite)))
		EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualCrossed): parent2 must be male in sexual models, or hermaphroditic in non-sexual models." << EidosTerminate();
	if ((p_parent1->index_ == -1) || (p_parent2->index_ == -1))
		EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualCrossed): parent1 and parent2 must be visible in a subpopulation (i.e., may not be new juveniles)." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if ((&parent1_subpop.species_ != &this->species_) || (&parent2_subpop.species_ != &this->species_))
		EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualCrossed): addCrossed() requires that both parents belong to the same species as the target subpopulation." << EidosTerminate();
#endif
	
	// Figure out callbacks, which are based on the subpopulation of each parent
	std::vector<SLiMEidosBlock*> *parent1_recombination_callbacks = nullptr;
	std::vector<SLiMEidosBlock*> *parent2_recombination_callbacks = nullptr;
	std::vector<SLiMEidosBlock*> *parent1_mutation_callbacks = nullptr;
	std::vector<SLiMEidosBlock*> *parent2_mutation_callbacks = nullptr;
	std::vector<SLiMEidosBlock*> *modify_child_callbacks_ = nullptr;
	
	if (f_callbacks)
	{
		parent1_recombination_callbacks = &parent1_subpop.registered_recombination_callbacks_;
		parent2_recombination_callbacks = &parent2_subpop.registered_recombination_callbacks_;
		parent1_mutation_callbacks = &parent1_subpop.registered_mutation_callbacks_;
		parent2_mutation_callbacks = &parent2_subpop.registered_mutation_callbacks_;
		modify_child_callbacks_ = &parent1_subpop.registered_modify_child_callbacks_;
		
		if (!parent1_recombination_callbacks->size()) parent1_recombination_callbacks = nullptr;
		if (!parent2_recombination_callbacks->size()) parent2_recombination_callbacks = nullptr;
		if (!parent1_mutation_callbacks->size()) parent1_mutation_callbacks = nullptr;
		if (!parent2_mutation_callbacks->size()) parent2_mutation_callbacks = nullptr;
		if (!modify_child_callbacks_->size()) modify_child_callbacks_ = nullptr;
	}
	
	// Create the offspring and record it
	Individual *individual = NewSubpopIndividual(/* index */ -1, p_child_sex, /* age */ 0, /* fitness */ NAN, /* p_mean_parent_age */ (p_parent1->age_ + (float)p_parent2->age_) / 2.0F);
	
	slim_pedigreeid_t individual_pid = f_pedigree_rec ? SLiM_GetNextPedigreeID() : 0;
	
	if (f_pedigree_rec)
		individual->TrackParentage_Biparental(individual_pid, *p_parent1, *p_parent2);
	
	// TREE SEQUENCE RECORDING
	if (f_treeseq)
		species_.SetCurrentNewIndividual(individual);
	
	// BCH 9/26/2023: inherit the spatial position of the first parent by default, to set up for deviatePositions()/pointDeviated()
	if (f_spatial)
		individual->InheritSpatialPosition(species_.SpatialDimensionality(), p_parent1);
	
	// Configure the offspring's haplosomes one by one
	int currentHaplosomeIndex = 0;
	
	for (Chromosome *chromosome : species_.Chromosomes())
	{
#if DEBUG
		if (!species_.HasGenetics())
			EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualCrossed): (internal error) a chromosome is defined for a no-genetics species!" << EidosTerminate();
#endif
		
		if (f_mutrunexps) chromosome->StartMutationRunExperimentClock();
		
		// Determine what kind of haplosomes to make for this chromosome
		ChromosomeType chromosomeType = chromosome->Type();
		Haplosome *haplosome1 = nullptr, *haplosome2 = nullptr;
		
		switch (chromosomeType)
		{
			case ChromosomeType::kA_DiploidAutosome:
			{
				// each haplosome is generated by recombination between a pair of parental haplosomes
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// parent 1 copy 1
					Haplosome *parental_haplosome2 = p_parent1->haplosomes_[currentHaplosomeIndex+1];		// parent 1 copy 2
					haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
					
					population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parental_haplosome2, parent1_recombination_callbacks, parent1_mutation_callbacks);
				}
				{
					Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// parent 2 copy 1
					Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex+1];		// parent 2 copy 2
					haplosome2 = chromosome->NewHaplosome_NONNULL(individual, 1);
					
					population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome1, parental_haplosome2, parent2_recombination_callbacks, parent2_mutation_callbacks);
				}
				break;
			}
			case ChromosomeType::kH_HaploidAutosome:
			{
				// the haplosome is generated by recombination between the haplosomes of the two parents
				Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];				// parent 1 copy
				Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex];				// parent 2 copy
				haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				
				population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parental_haplosome2, parent1_recombination_callbacks, parent1_mutation_callbacks);
				break;
			}
			case ChromosomeType::kX_XSexChromosome:
			{
				// one X comes from recombination from the female parent, the other (to females only) clonally from the male parent
				// so females are XX, males are X-
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// female's X 1
					Haplosome *parental_haplosome2 = p_parent1->haplosomes_[currentHaplosomeIndex+1];		// female's X 2
					haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
					
					population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parental_haplosome2, parent1_recombination_callbacks, parent1_mutation_callbacks);
				}
				{
					if (p_child_sex == IndividualSex::kFemale)
					{
						Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];		// male's X (from female)
						haplosome2 = chromosome->NewHaplosome_NONNULL(individual, 1);
						
						population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome1, parent2_mutation_callbacks);
					}
					else
					{
						Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex+1];	// male's - (from male)
						haplosome2 = chromosome->NewHaplosome_NULL(individual, 1);
						
						Haplosome::DebugCheckStructureMatch(parental_haplosome2, haplosome2, chromosome);
					}
				}
				break;
			}
			case ChromosomeType::kY_YSexChromosome:
			{
				// the Y comes (to males only) clonally from the male parent
				// so females are -, males are Y
				if (p_child_sex == IndividualSex::kMale)
				{
					Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// male's Y
					haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
					
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parent2_mutation_callbacks);
				}
				else
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// female's -
					haplosome1 = chromosome->NewHaplosome_NULL(individual, 0);
					
					Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
				}
				break;
			}
			case ChromosomeType::kZ_ZSexChromosome:
			{
				// one Z comes (to males only) clonally from the female parent, the other from recombination from the male parent
				// so females are -Z, males are ZZ (note we think of it as WZ, not ZW, since the female parent is always the first parent)
				{
					if (p_child_sex == IndividualSex::kMale)
					{
						Haplosome *parental_haplosome2 = p_parent1->haplosomes_[currentHaplosomeIndex+1];	// female's Z
						haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
						
						population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome2, parent1_mutation_callbacks);
					}
					else
					{
						Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];		// female's -
						haplosome1 = chromosome->NewHaplosome_NULL(individual, 0);
						
						Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
					}
				}
				{
					Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// male's Z
					Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex+1];		// male's Z
					haplosome2 = chromosome->NewHaplosome_NONNULL(individual, 1);
					
					population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome1, parental_haplosome2, parent2_recombination_callbacks, parent2_mutation_callbacks);
				}
				break;
			}
			case ChromosomeType::kW_WSexChromosome:
			{
				// the W comes (to females only) clonally from the female parent
				// so females are W, males are -
				if (p_child_sex == IndividualSex::kFemale)
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// female's W
					haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
					
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parent1_mutation_callbacks);
				}
				else
				{
					Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// male's -
					haplosome1 = chromosome->NewHaplosome_NULL(individual, 0);
					
					Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
				}
				break;
			}
			case ChromosomeType::kHF_HaploidFemaleInherited:
			{
				// haploid, inherited clonally from the female for both sexes, like the W for females
				Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// female's copy
				haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				
				population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parent1_mutation_callbacks);
				break;
			}
			case ChromosomeType::kFL_HaploidFemaleLine:
			{
				// this comes (to females only) clonally from the female parent, just like a W
				if (p_child_sex == IndividualSex::kFemale)
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// female's copy
					haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
					
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parent1_mutation_callbacks);
				}
				else
				{
					Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// male's -
					haplosome1 = chromosome->NewHaplosome_NULL(individual, 0);
					
					Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
				}
				break;
			}
			case ChromosomeType::kHM_HaploidMaleInherited:
			{
				// haploid, inherited clonally from the male for both sexes, like the Y for males
				Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// male's copy
				haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				
				population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parent2_mutation_callbacks);
				break;
			}
			case ChromosomeType::kML_HaploidMaleLine:
			{
				// this comes (to males only) clonally from the male parent, just like a Y
				if (p_child_sex == IndividualSex::kMale)
				{
					Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// male's copy
					haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
					
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parent2_mutation_callbacks);
				}
				else
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// female's -
					haplosome1 = chromosome->NewHaplosome_NULL(individual, 0);
					
					Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
				}
				break;
			}
			case ChromosomeType::kHNull_HaploidAutosomeWithNull:
			{
				EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualCrossed): chromosome type 'H-' does not allow reproduction by biparental cross (only cloning); chromosome type 'H' provides greater flexibility for modeling haploids." << EidosTerminate();
				break;
			}
			case ChromosomeType::kNullY_YSexChromosomeWithNull:
			{
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// parent 1 copy 1
					Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex];			// parent 2 copy 1
					haplosome1 = chromosome->NewHaplosome_NULL(individual, 0);
					
					Haplosome::DebugCheckStructureMatch(parental_haplosome1, parental_haplosome2, haplosome1, chromosome);
				}
				{
					if (p_child_sex == IndividualSex::kMale)
					{
						Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex+1];		// male's Y
						haplosome2 = chromosome->NewHaplosome_NONNULL(individual, 1);
						
						population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome2, parent2_mutation_callbacks);
					}
					else
					{
						Haplosome *parental_haplosome2 = p_parent1->haplosomes_[currentHaplosomeIndex+1];		// female's -
						haplosome2 = chromosome->NewHaplosome_NULL(individual, 1);
						
						Haplosome::DebugCheckStructureMatch(parental_haplosome2, haplosome2, chromosome);
					}
				}
				break;
			}
		}
		
		if (f_mutrunexps) chromosome->StopMutationRunExperimentClock("GenerateIndividualCrossed()");
		
		// For each haplosome generated, we need to add them to the individual.  We also need
		// to record the null haplosomes for tree-seq; non-null haplosomes were already
		// recorded by the methods above, HaplosomeCrossed() and HaplosomeCloned().  We also
		// have to set their haplosome_id_ as appropriate.
		if (haplosome1)
		{
			individual->AddHaplosomeAtIndex(haplosome1, currentHaplosomeIndex);
			
			if (f_pedigree_rec)
				haplosome1->haplosome_id_ = individual_pid * 2;
			
			if (f_treeseq && haplosome1->IsNull())
					species_.RecordNewHaplosome_NULL(haplosome1);
		}
		if (haplosome2)
		{
			individual->AddHaplosomeAtIndex(haplosome2, currentHaplosomeIndex+1);
			
			if (f_pedigree_rec)
				haplosome2->haplosome_id_ = individual_pid * 2 + 1;
			
			if (f_treeseq && haplosome2->IsNull())
				species_.RecordNewHaplosome_NULL(haplosome2);
		}
		
		// move forward 1 or 2 indices in haplosomes_, depending on whether a haplosome2 was created (even if it is null)
		currentHaplosomeIndex += (haplosome2 ? 2 : 1);
	}
	
	// Run the candidate past modifyChild() callbacks; the first parent subpop's registered callbacks are used
	if (modify_child_callbacks_)
	{
		bool proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, p_parent1, p_parent2, /* p_is_selfing */ false, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, *modify_child_callbacks_);
		
		// If the child was rejected, un-record it and dispose of it
		if (!proposed_child_accepted)
		{
			if (f_pedigree_rec)
				individual->RevokeParentage_Biparental(*p_parent1, *p_parent2);
			
			FreeSubpopIndividual(individual);
			individual = nullptr;
			
			// TREE SEQUENCE RECORDING
			if (f_treeseq)
				species_.RetractNewIndividual();
		}
	}
	
	return individual;
}

template Individual *Subpopulation::GenerateIndividualCrossed<false, false, false, false, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, false, false, false, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, false, false, true, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, false, false, true, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, false, true, false, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, false, true, false, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, false, true, true, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, false, true, true, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, true, false, false, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, true, false, false, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, true, false, true, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, true, false, true, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, true, true, false, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, true, true, false, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, true, true, true, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<false, true, true, true, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, false, false, false, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, false, false, false, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, false, false, true, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, false, false, true, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, false, true, false, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, false, true, false, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, false, true, true, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, false, true, true, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, true, false, false, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, true, false, false, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, true, false, true, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, true, false, true, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, true, true, false, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, true, true, false, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, true, true, true, false>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template Individual *Subpopulation::GenerateIndividualCrossed<true, true, true, true, true>(Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);

template <const bool f_mutrunexps, const bool f_pedigree_rec, const bool f_treeseq, const bool f_callbacks, const bool f_spatial>
Individual *Subpopulation::GenerateIndividualSelfed(Individual *p_parent)
{
	Subpopulation &parent_subpop = *p_parent->subpopulation_;
	
#if DEBUG
	IndividualSex parent_sex = p_parent->sex_;
	
	if (parent_sex != IndividualSex::kHermaphrodite)
		EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualSelfed): parent must be hermaphroditic." << EidosTerminate();
	if (p_parent->index_ == -1)
		EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualSelfed): parent must be visible in a subpopulation (i.e., may not be a new juvenile)." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if (&parent_subpop.species_ != &this->species_)
		EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualSelfed): addSelfed() requires that parent belongs to the same species as the target subpopulation." << EidosTerminate();
#endif
	
	// Figure out callbacks, which are based on the subpopulation of each parent
	std::vector<SLiMEidosBlock*> *recombination_callbacks = nullptr;
	std::vector<SLiMEidosBlock*> *mutation_callbacks = nullptr;
	std::vector<SLiMEidosBlock*> *modify_child_callbacks_ = nullptr;
	
	if (f_callbacks)
	{
		recombination_callbacks = &parent_subpop.registered_recombination_callbacks_;
		mutation_callbacks = &parent_subpop.registered_mutation_callbacks_;
		modify_child_callbacks_ = &parent_subpop.registered_modify_child_callbacks_;
		
		if (!recombination_callbacks->size()) recombination_callbacks = nullptr;
		if (!mutation_callbacks->size()) mutation_callbacks = nullptr;
		if (!modify_child_callbacks_->size()) modify_child_callbacks_ = nullptr;
	}
	
	// Create the offspring and record it
	Individual *individual = NewSubpopIndividual(/* index */ -1, IndividualSex::kHermaphrodite, /* age */ 0, /* fitness */ NAN, /* p_mean_parent_age */ p_parent->age_);
	
	slim_pedigreeid_t individual_pid = f_pedigree_rec ? SLiM_GetNextPedigreeID() : 0;
	
	if (f_pedigree_rec)
		individual->TrackParentage_Uniparental(individual_pid, *p_parent);
	
	// TREE SEQUENCE RECORDING
	if (f_treeseq)
		species_.SetCurrentNewIndividual(individual);
	
	// BCH 9/26/2023: inherit the spatial position of the parent by default, to set up for deviatePositions()/pointDeviated()
	if (f_spatial)
		individual->InheritSpatialPosition(species_.SpatialDimensionality(), p_parent);
	
	// Configure the offspring's haplosomes one by one
	int currentHaplosomeIndex = 0;
	
	for (Chromosome *chromosome : species_.Chromosomes())
	{
#if DEBUG
		if (!species_.HasGenetics())
			EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualSelfed): (internal error) a chromosome is defined for a no-genetics species!" << EidosTerminate();
#endif
		
		if (f_mutrunexps) chromosome->StartMutationRunExperimentClock();
		
		// Determine what kind of haplosomes to make for this chromosome
		ChromosomeType chromosomeType = chromosome->Type();
		Haplosome *haplosome1 = nullptr, *haplosome2 = nullptr;
		
		switch (chromosomeType)
		{
			case ChromosomeType::kA_DiploidAutosome:
			{
				// each haplosome is generated by recombination between the pair of parental haplosomes
				Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];			// parent copy 1
				Haplosome *parental_haplosome2 = p_parent->haplosomes_[currentHaplosomeIndex+1];		// parent copy 2
				
				haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parental_haplosome2, recombination_callbacks, mutation_callbacks);
				
				haplosome2 = chromosome->NewHaplosome_NONNULL(individual, 1);
				population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome1, parental_haplosome2, recombination_callbacks, mutation_callbacks);
				break;
			}
			case ChromosomeType::kH_HaploidAutosome:
			{
				// the haplosome is generated by recombination between the haplosome of the parent and itself
				// but since the one haplosome is identical to itself, that is the same thing as cloning
				Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];				// parent copy
				haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				
				population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				break;
			}
			case ChromosomeType::kHNull_HaploidAutosomeWithNull:
			{
				EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualSelfed): chromosome type 'H-' does not allow reproduction by selfing (only cloning); chromosome type 'H' provides greater flexibility for modeling haploids." << EidosTerminate();
				break;
			}
			case ChromosomeType::kX_XSexChromosome:
			case ChromosomeType::kY_YSexChromosome:
			case ChromosomeType::kZ_ZSexChromosome:
			case ChromosomeType::kW_WSexChromosome:
			case ChromosomeType::kHF_HaploidFemaleInherited:
			case ChromosomeType::kFL_HaploidFemaleLine:
			case ChromosomeType::kHM_HaploidMaleInherited:
			case ChromosomeType::kML_HaploidMaleLine:
			case ChromosomeType::kNullY_YSexChromosomeWithNull:
				EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualEmpty): (internal error) sex-specific chromosome type not supported for selfing." << EidosTerminate();
				break;
		}
		
		if (f_mutrunexps) chromosome->StopMutationRunExperimentClock("GenerateIndividualSelfed()");
		
		// For each haplosome generated, we need to add them to the individual.  We also need
		// to record the null haplosomes for tree-seq; non-null haplosomes were already
		// recorded by the methods above, HaplosomeCrossed() and HaplosomeCloned().  We also
		// have to set their haplosome_id_ as appropriate.
		if (haplosome1)
		{
			individual->AddHaplosomeAtIndex(haplosome1, currentHaplosomeIndex);
			
			if (f_pedigree_rec)
				haplosome1->haplosome_id_ = individual_pid * 2;
			
			if (f_treeseq && haplosome1->IsNull())
					species_.RecordNewHaplosome_NULL(haplosome1);
		}
		if (haplosome2)
		{
			individual->AddHaplosomeAtIndex(haplosome2, currentHaplosomeIndex+1);
			
			if (f_pedigree_rec)
				haplosome2->haplosome_id_ = individual_pid * 2 + 1;
			
			if (f_treeseq && haplosome2->IsNull())
				species_.RecordNewHaplosome_NULL(haplosome2);
		}
		
		// move forward 1 or 2 indices in haplosomes_, depending on whether a haplosome2 was created (even if it is null)
		currentHaplosomeIndex += (haplosome2 ? 2 : 1);
	}
	
	// Run the candidate past modifyChild() callbacks; the first parent subpop's registered callbacks are used
	if (modify_child_callbacks_)
	{
		bool proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, p_parent, p_parent, /* p_is_selfing */ true, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, *modify_child_callbacks_);
		
		// If the child was rejected, un-record it and dispose of it
		if (!proposed_child_accepted)
		{
			if (f_pedigree_rec)
				individual->RevokeParentage_Uniparental(*p_parent);
			
			FreeSubpopIndividual(individual);
			individual = nullptr;
			
			// TREE SEQUENCE RECORDING
			if (f_treeseq)
				species_.RetractNewIndividual();
		}
	}
	
	return individual;
}

template Individual *Subpopulation::GenerateIndividualSelfed<false, false, false, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, false, false, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, false, false, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, false, false, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, false, true, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, false, true, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, false, true, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, false, true, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, true, false, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, true, false, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, true, false, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, true, false, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, true, true, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, true, true, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, true, true, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<false, true, true, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, false, false, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, false, false, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, false, false, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, false, false, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, false, true, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, false, true, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, false, true, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, false, true, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, true, false, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, true, false, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, true, false, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, true, false, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, true, true, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, true, true, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, true, true, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualSelfed<true, true, true, true, true>(Individual *p_parent);

template <const bool f_mutrunexps, const bool f_pedigree_rec, const bool f_treeseq, const bool f_callbacks, const bool f_spatial>
Individual *Subpopulation::GenerateIndividualCloned(Individual *p_parent)
{
	IndividualSex parent_sex = p_parent->sex_;
	Subpopulation &parent_subpop = *p_parent->subpopulation_;
	
#if DEBUG
	if (p_parent->index_ == -1)
		EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualCloned): parent must be visible in a subpopulation (i.e., may not be a new juvenile)." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if (&parent_subpop.species_ != &this->species_)
		EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualCloned): addCloned() requires that parent belongs to the same species as the target subpopulation." << EidosTerminate();
#endif
	
	// Figure out callbacks, which are based on the subpopulation of each parent
	std::vector<SLiMEidosBlock*> *mutation_callbacks = nullptr;
	std::vector<SLiMEidosBlock*> *modify_child_callbacks_ = nullptr;
	
	if (f_callbacks)
	{
		mutation_callbacks = &parent_subpop.registered_mutation_callbacks_;
		modify_child_callbacks_ = &parent_subpop.registered_modify_child_callbacks_;
		
		if (!mutation_callbacks->size()) mutation_callbacks = nullptr;
		if (!modify_child_callbacks_->size()) modify_child_callbacks_ = nullptr;
	}
	
	// Create the offspring and record it
	Individual *individual = NewSubpopIndividual(/* index */ -1, parent_sex, /* age */ 0, /* fitness */ NAN, /* p_mean_parent_age */ p_parent->age_);
	
	slim_pedigreeid_t individual_pid = f_pedigree_rec ? SLiM_GetNextPedigreeID() : 0;
	
	if (f_pedigree_rec)
		individual->TrackParentage_Uniparental(individual_pid, *p_parent);
	
	// TREE SEQUENCE RECORDING
	if (f_treeseq)
		species_.SetCurrentNewIndividual(individual);
	
	// BCH 9/26/2023: inherit the spatial position of the parent by default, to set up for deviatePositions()/pointDeviated()
	if (f_spatial)
		individual->InheritSpatialPosition(species_.SpatialDimensionality(), p_parent);
	
	// Configure the offspring's haplosomes one by one
	int currentHaplosomeIndex = 0;
	
	for (Chromosome *chromosome : species_.Chromosomes())
	{
#if DEBUG
		if (!species_.HasGenetics())
			EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualCloned): (internal error) a chromosome is defined for a no-genetics species!" << EidosTerminate();
#endif
		
		if (f_mutrunexps) chromosome->StartMutationRunExperimentClock();
		
		// Determine what kind of haplosomes to make for this chromosome
		// We just faithfully clone the existing haplosomes of the parent, regardless of type
		Haplosome *haplosome1 = nullptr, *haplosome2 = nullptr;
		
		if (chromosome->IntrinsicPloidy() == 2)
		{
			Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];
			
			if (parental_haplosome1->IsNull())
			{
				haplosome1 = chromosome->NewHaplosome_NULL(individual, 0);
			}
			else
			{
				haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
			}
			
			Haplosome *parental_haplosome2 = p_parent->haplosomes_[currentHaplosomeIndex+1];
			
			if (parental_haplosome2->IsNull())
			{
				haplosome2 = chromosome->NewHaplosome_NULL(individual, 1);
			}
			else
			{
				haplosome2 = chromosome->NewHaplosome_NONNULL(individual, 1);
				population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome2, mutation_callbacks);
			}
		}
		else
		{
			Haplosome *parental_haplosome = p_parent->haplosomes_[currentHaplosomeIndex];
			
			if (parental_haplosome->IsNull())
			{
				haplosome1 = chromosome->NewHaplosome_NULL(individual, 0);
			}
			else
			{
				haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome, mutation_callbacks);
			}
		}
		
		if (f_mutrunexps) chromosome->StopMutationRunExperimentClock("GenerateIndividualCloned()");
		
		// For each haplosome generated, we need to add them to the individual.  We also need
		// to record the null haplosomes for tree-seq; non-null haplosomes were already
		// recorded by the methods above, HaplosomeCrossed() and HaplosomeCloned().  We also
		// have to set their haplosome_id_ as appropriate.
		if (haplosome1)
		{
			individual->AddHaplosomeAtIndex(haplosome1, currentHaplosomeIndex);
			
			if (f_pedigree_rec)
				haplosome1->haplosome_id_ = individual_pid * 2;
			
			if (f_treeseq && haplosome1->IsNull())
					species_.RecordNewHaplosome_NULL(haplosome1);
		}
		if (haplosome2)
		{
			individual->AddHaplosomeAtIndex(haplosome2, currentHaplosomeIndex+1);
			
			if (f_pedigree_rec)
				haplosome2->haplosome_id_ = individual_pid * 2 + 1;
			
			if (f_treeseq && haplosome2->IsNull())
				species_.RecordNewHaplosome_NULL(haplosome2);
		}
		
		// move forward 1 or 2 indices in haplosomes_, depending on whether a haplosome2 was created (even if it is null)
		currentHaplosomeIndex += (haplosome2 ? 2 : 1);
	}
	
	// Run the candidate past modifyChild() callbacks; the first parent subpop's registered callbacks are used
	if (modify_child_callbacks_)
	{
		bool proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, p_parent, p_parent, /* p_is_selfing */ false, /* p_is_cloning */ true, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, *modify_child_callbacks_);
		
		// If the child was rejected, un-record it and dispose of it
		if (!proposed_child_accepted)
		{
			// revoke parentage
			if (f_pedigree_rec)
				individual->RevokeParentage_Uniparental(*p_parent);
			
			FreeSubpopIndividual(individual);
			individual = nullptr;
			
			// TREE SEQUENCE RECORDING
			if (f_treeseq)
				species_.RetractNewIndividual();
		}
	}
	
	return individual;
}

template Individual *Subpopulation::GenerateIndividualCloned<false, false, false, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, false, false, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, false, false, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, false, false, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, false, true, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, false, true, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, false, true, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, false, true, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, true, false, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, true, false, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, true, false, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, true, false, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, true, true, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, true, true, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, true, true, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<false, true, true, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, false, false, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, false, false, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, false, false, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, false, false, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, false, true, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, false, true, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, false, true, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, false, true, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, true, false, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, true, false, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, true, false, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, true, false, true, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, true, true, false, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, true, true, false, true>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, true, true, true, false>(Individual *p_parent);
template Individual *Subpopulation::GenerateIndividualCloned<true, true, true, true, true>(Individual *p_parent);

Individual *Subpopulation::GenerateIndividualEmpty(slim_popsize_t p_individual_index, IndividualSex p_child_sex, slim_age_t p_age, double p_fitness, float p_mean_parent_age, bool p_haplosome1_null, bool p_haplosome2_null, bool p_run_modify_child, bool p_record_in_treeseq)
{
	// Create the offspring and record it
	Individual *individual = NewSubpopIndividual(p_individual_index, p_child_sex, p_age, p_fitness, p_mean_parent_age);
	
	bool pedigrees_enabled = species_.PedigreesEnabled();
	slim_pedigreeid_t individual_pid = pedigrees_enabled ? SLiM_GetNextPedigreeID() : 0;
	
	if (pedigrees_enabled)
		individual->TrackParentage_Parentless(individual_pid);
	
	// TREE SEQUENCE RECORDING
	if (p_record_in_treeseq)
		species_.SetCurrentNewIndividual(individual);
	
	// BCH 9/26/2023: note that there is no parent, so the spatial position of the offspring is left uninitialized.
	// individual->InheritSpatialPosition(species_.spatial_dimensionality_, ???)
	
	// Configure the offspring's haplosomes one by one
	int currentHaplosomeIndex = 0;
	
	for (Chromosome *chromosome : species_.Chromosomes())
	{
#if DEBUG
		if (!species_.HasGenetics())
			EIDOS_TERMINATION << "ERROR (Population::GenerateIndividualEmpty): (internal error) a chromosome is defined for a no-genetics species!" << EidosTerminate();
#endif
		
		chromosome->StartMutationRunExperimentClock();
		
		// Determine what kind of haplosomes to make for this chromosome
		ChromosomeType chromosomeType = chromosome->Type();
		Haplosome *haplosome1 = nullptr, *haplosome2 = nullptr;
		
		switch (chromosomeType)
		{
			case ChromosomeType::kA_DiploidAutosome:
			{
				// the flags p_haplosome1_null / p_haplosome2_null apply only here!
				// need to set has_null_haplosomes_ here because these haplosomes
				// are not normally null, according to the chromosome type
				if (p_haplosome1_null)
				{
					haplosome1 = chromosome->NewHaplosome_NULL(individual, 0);
					has_null_haplosomes_ = true;
				}
				else
				{
					haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				}
				
				if (p_haplosome2_null)
				{
					haplosome2 = chromosome->NewHaplosome_NULL(individual, 1);
					has_null_haplosomes_ = true;
				}
				else
				{
					haplosome2 = chromosome->NewHaplosome_NONNULL(individual, 1);
				}
				break;
			}
			case ChromosomeType::kH_HaploidAutosome:
			case ChromosomeType::kHF_HaploidFemaleInherited:
			case ChromosomeType::kHM_HaploidMaleInherited:
			{
				// non-null for all
				haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				break;
			}
			case ChromosomeType::kX_XSexChromosome:
			{
				// XX for females, X- for males
				haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				
				if (p_child_sex == IndividualSex::kMale)
					haplosome2 = chromosome->NewHaplosome_NULL(individual, 1);
				else
					haplosome2 = chromosome->NewHaplosome_NONNULL(individual, 1);
				break;
			}
			case ChromosomeType::kY_YSexChromosome:
			case ChromosomeType::kML_HaploidMaleLine:
			{
				// - for females, Y for males
				if (p_child_sex == IndividualSex::kMale)
					haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				else
					haplosome1 = chromosome->NewHaplosome_NULL(individual, 0);
				break;
			}
			case ChromosomeType::kZ_ZSexChromosome:
			{
				// ZZ for males, -Z for females
				if (p_child_sex == IndividualSex::kFemale)
					haplosome1 = chromosome->NewHaplosome_NULL(individual, 0);
				else
					haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				
				haplosome2 = chromosome->NewHaplosome_NONNULL(individual, 1);
				break;
			}
			case ChromosomeType::kW_WSexChromosome:
			case ChromosomeType::kFL_HaploidFemaleLine:
			{
				// - for males, W for females
				if (p_child_sex == IndividualSex::kFemale)
					haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				else
					haplosome1 = chromosome->NewHaplosome_NULL(individual, 0);
				break;
			}
			case ChromosomeType::kHNull_HaploidAutosomeWithNull:
			{
				// non-null + null for all
				haplosome1 = chromosome->NewHaplosome_NONNULL(individual, 0);
				haplosome2 = chromosome->NewHaplosome_NULL(individual, 1);
				break;
			}
			case ChromosomeType::kNullY_YSexChromosomeWithNull:
			{
				// -- for females, -Y for males
				haplosome1 = chromosome->NewHaplosome_NULL(individual, 0);
				
				if (p_child_sex == IndividualSex::kMale)
					haplosome2 = chromosome->NewHaplosome_NONNULL(individual, 1);
				else
					haplosome2 = chromosome->NewHaplosome_NULL(individual, 1);
				break;
			}
		}
		
		// For each haplosome generated, we need to add them to the individual.  We also need
		// to record all haplosomes for tree-seq; since even the non-null haplosomes are empty,
		// not filled by HaplosomeCrossed() and HaplosomeCloned() which record them, they need
		// to be recorded here also.
		
		// We need to add a *different* empty MutationRun to each mutrun index, so each run comes out of
		// the correct per-thread allocation pool.  Would be nice to share these empty runs across
		// multiple calls to addEmpty(), but that's hard now since we don't have refcounts.  How about
		// we maintain a set of empty mutruns, one for each position, in the Species, and whenever we
		// need an empty mutrun we reuse from that pool – after checking that the run is still empty??
		if (haplosome1)
		{
#if SLIM_CLEAR_HAPLOSOMES
			haplosome1->check_cleared_to_nullptr();
#endif
			if (!haplosome1->IsNull())
			{
				int32_t mutrun_count = chromosome->mutrun_count_;
				
				for (int run_index = 0; run_index < mutrun_count; ++run_index)
				{
					MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForMutationRunIndex(run_index);
					const MutationRun *mutrun = MutationRun::NewMutationRun(mutrun_context);
					
					haplosome1->mutruns_[run_index] = mutrun;
				}
			}
			
			individual->AddHaplosomeAtIndex(haplosome1, currentHaplosomeIndex);
			
			if (pedigrees_enabled)
				haplosome1->haplosome_id_ = individual_pid * 2;
			
			if (p_record_in_treeseq)
			{
				if (haplosome1->IsNull())
					species_.RecordNewHaplosome_NULL(haplosome1);
				else
					species_.RecordNewHaplosome(nullptr, 0, haplosome1, nullptr, nullptr);
			}
		}
		if (haplosome2)
		{
#if SLIM_CLEAR_HAPLOSOMES
			haplosome2->check_cleared_to_nullptr();
#endif
			if (!haplosome2->IsNull())
			{
				int32_t mutrun_count = chromosome->mutrun_count_;
				
				for (int run_index = 0; run_index < mutrun_count; ++run_index)
				{
					MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForMutationRunIndex(run_index);
					const MutationRun *mutrun = MutationRun::NewMutationRun(mutrun_context);
					
					haplosome2->mutruns_[run_index] = mutrun;
				}
			}
			
			individual->AddHaplosomeAtIndex(haplosome2, currentHaplosomeIndex+1);
			
			if (pedigrees_enabled)
				haplosome2->haplosome_id_ = individual_pid * 2 + 1;
			
			if (p_record_in_treeseq)
			{
				if (haplosome2->IsNull())
					species_.RecordNewHaplosome_NULL(haplosome2);
				else
					species_.RecordNewHaplosome(nullptr, 0, haplosome2, nullptr, nullptr);
			}
		}
		
		chromosome->StopMutationRunExperimentClock("GenerateIndividualEmpty()");
		
		// move forward 1 or 2, depending on whether a haplosome2 was created
		currentHaplosomeIndex += (haplosome2 ? 2 : 1);
	}
	
	if (p_run_modify_child)
	{
		// Run the candidate past modifyChild() callbacks; the target subpop's registered callbacks are used
		if (registered_modify_child_callbacks_.size())
		{
			bool proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, nullptr, nullptr, /* p_is_selfing */ false, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, registered_modify_child_callbacks_);
			
			// If the child was rejected, un-record it and dispose of it
			if (!proposed_child_accepted)
			{
				if (pedigrees_enabled)
					individual->RevokeParentage_Parentless();
				
				FreeSubpopIndividual(individual);
				individual = nullptr;
				
				// TREE SEQUENCE RECORDING
				if (p_record_in_treeseq)
					species_.RetractNewIndividual();
			}
		}
	}
	
	return individual;
}

template <const bool f_mutrunexps, const bool f_pedigree_rec, const bool f_treeseq, const bool f_callbacks, const bool f_spatial>
bool Subpopulation::MungeIndividualCrossed(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex)
{
	Subpopulation &parent1_subpop = *p_parent1->subpopulation_;
	
#if DEBUG
	Subpopulation &parent2_subpop = *p_parent2->subpopulation_;
	
	if (&parent1_subpop != &parent2_subpop)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed): parent1 and parent2 must belong to the same subpopulation; that is assumed, since this method is called only for WF reproduction." << EidosTerminate();
	
	IndividualSex parent1_sex = p_parent1->sex_;
	IndividualSex parent2_sex = p_parent2->sex_;
	
	if ((sex_enabled_ && (parent1_sex != IndividualSex::kFemale)) || (!sex_enabled_ && (parent1_sex != IndividualSex::kHermaphrodite)))
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed): parent1 must be female in sexual models, or hermaphroditic in non-sexual models." << EidosTerminate();
	if ((sex_enabled_ && (parent2_sex != IndividualSex::kMale)) || (!sex_enabled_ && (parent2_sex != IndividualSex::kHermaphrodite)))
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed): parent2 must be male in sexual models, or hermaphroditic in non-sexual models." << EidosTerminate();
	if ((p_parent1->index_ == -1) || (p_parent2->index_ == -1))
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed): parent1 and parent2 must be visible in a subpopulation (i.e., may not be new juveniles)." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if ((&parent1_subpop.species_ != &this->species_) || (&parent2_subpop.species_ != &this->species_))
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed): biparental crossing requires that both parents belong to the same species as the target subpopulation." << EidosTerminate();
#endif
	
	// Figure out callbacks, which are based on the subpopulation of the parents (which must be the same)
	std::vector<SLiMEidosBlock*> *recombination_callbacks = nullptr;
	std::vector<SLiMEidosBlock*> *mutation_callbacks = nullptr;
	std::vector<SLiMEidosBlock*> *modify_child_callbacks_ = nullptr;
	
	if (f_callbacks)
	{
		recombination_callbacks = &parent1_subpop.registered_recombination_callbacks_;
		mutation_callbacks = &parent1_subpop.registered_mutation_callbacks_;
		modify_child_callbacks_ = &parent1_subpop.registered_modify_child_callbacks_;
		
		if (!recombination_callbacks->size()) recombination_callbacks = nullptr;
		if (!mutation_callbacks->size()) mutation_callbacks = nullptr;
		if (!modify_child_callbacks_->size()) modify_child_callbacks_ = nullptr;
	}
	
	// Record the offspring
	if (f_pedigree_rec)
		individual->TrackParentage_Biparental(p_pedigree_id, *p_parent1, *p_parent2);
	
	// TREE SEQUENCE RECORDING
	if (f_treeseq)
		species_.SetCurrentNewIndividual(individual);
	
	// BCH 9/26/2023: inherit the spatial position of the first parent by default, to set up for deviatePositions()/pointDeviated()
	if (f_spatial)
		individual->InheritSpatialPosition(species_.SpatialDimensionality(), p_parent1);
	
	// Configure the offspring's haplosomes one by one
	Haplosome **haplosomes = individual->haplosomes_;
	int currentHaplosomeIndex = 0;
	
	for (Chromosome *chromosome : species_.Chromosomes())
	{
#if DEBUG
		if (!species_.HasGenetics())
			EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed): (internal error) a chromosome is defined for a no-genetics species!" << EidosTerminate();
#endif
		
		if (f_mutrunexps) chromosome->StartMutationRunExperimentClock();
		
		// Determine what kind of haplosomes to make for this chromosome
		ChromosomeType chromosomeType = chromosome->Type();
		Haplosome *haplosome1 = nullptr, *haplosome2 = nullptr;
		
		switch (chromosomeType)
		{
			case ChromosomeType::kA_DiploidAutosome:
			{
				// each haplosome is generated by recombination between a pair of parental haplosomes
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// parent 1 copy 1
					Haplosome *parental_haplosome2 = p_parent1->haplosomes_[currentHaplosomeIndex+1];		// parent 1 copy 2
					haplosome1 = haplosomes[currentHaplosomeIndex];
					
					population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parental_haplosome2, recombination_callbacks, mutation_callbacks);
				}
				{
					Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// parent 2 copy 1
					Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex+1];		// parent 2 copy 2
					haplosome2 = haplosomes[currentHaplosomeIndex+1];
					
					population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome1, parental_haplosome2, recombination_callbacks, mutation_callbacks);
				}
				currentHaplosomeIndex += 2;
				break;
			}
			case ChromosomeType::kH_HaploidAutosome:
			{
				// the haplosome is generated by recombination between the haplosomes of the two parents
				Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];				// parent 1 copy
				Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex];				// parent 2 copy
				haplosome1 = haplosomes[currentHaplosomeIndex];
				
				population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parental_haplosome2, recombination_callbacks, mutation_callbacks);
				
				currentHaplosomeIndex += 1;
				break;
			}
			case ChromosomeType::kX_XSexChromosome:
			{
				// one X comes from recombination from the female parent, the other (to females only) clonally from the male parent
				// so females are XX, males are X-
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// female's X 1
					Haplosome *parental_haplosome2 = p_parent1->haplosomes_[currentHaplosomeIndex+1];		// female's X 2
					haplosome1 = haplosomes[currentHaplosomeIndex];
					
					population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parental_haplosome2, recombination_callbacks, mutation_callbacks);
				}
				{
					if (p_child_sex == IndividualSex::kFemale)
					{
						Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];		// male's X (from female)
						haplosome2 = haplosomes[currentHaplosomeIndex+1];
						
						population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome1, mutation_callbacks);
					}
					else
					{
						Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex+1];	// male's - (from male)
						haplosome2 = haplosomes[currentHaplosomeIndex+1];
						
						Haplosome::DebugCheckStructureMatch(parental_haplosome2, haplosome2, chromosome);
					}
				}
				currentHaplosomeIndex += 2;
				break;
			}
			case ChromosomeType::kY_YSexChromosome:
			{
				// the Y comes (to males only) clonally from the male parent
				// so females are -, males are Y
				if (p_child_sex == IndividualSex::kMale)
				{
					Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// male's Y
					haplosome1 = haplosomes[currentHaplosomeIndex];
					
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				}
				else
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// female's -
					haplosome1 = haplosomes[currentHaplosomeIndex];
					
					Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
				}
				
				currentHaplosomeIndex += 1;
				break;
			}
			case ChromosomeType::kZ_ZSexChromosome:
			{
				// one Z comes (to males only) clonally from the female parent, the other from recombination from the male parent
				// so females are -Z, males are ZZ (note we think of it as WZ, not ZW, since the female parent is always the first parent)
				{
					if (p_child_sex == IndividualSex::kMale)
					{
						Haplosome *parental_haplosome2 = p_parent1->haplosomes_[currentHaplosomeIndex+1];	// female's Z
						haplosome1 = haplosomes[currentHaplosomeIndex];
						
						population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome2, mutation_callbacks);
					}
					else
					{
						Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];		// female's -
						haplosome1 = haplosomes[currentHaplosomeIndex];
						
						Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
					}
				}
				{
					Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// male's Z
					Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex+1];		// male's Z
					haplosome2 = haplosomes[currentHaplosomeIndex+1];
					
					population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome1, parental_haplosome2, recombination_callbacks, mutation_callbacks);
				}
				currentHaplosomeIndex += 2;
				break;
			}
			case ChromosomeType::kW_WSexChromosome:
			{
				// the W comes (to females only) clonally from the female parent
				// so females are W, males are -
				if (p_child_sex == IndividualSex::kFemale)
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// female's W
					haplosome1 = haplosomes[currentHaplosomeIndex];
					
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				}
				else
				{
					Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// male's -
					haplosome1 = haplosomes[currentHaplosomeIndex];
					
					Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
				}
				
				currentHaplosomeIndex += 1;
				break;
			}
			case ChromosomeType::kHF_HaploidFemaleInherited:
			{
				// haploid, inherited clonally from the female for both sexes, like the W for females
				Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// female's copy
				haplosome1 = haplosomes[currentHaplosomeIndex];
				
				population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				
				currentHaplosomeIndex += 1;
				break;
			}
			case ChromosomeType::kFL_HaploidFemaleLine:
			{
				// this comes (to females only) clonally from the female parent, just like a W
				if (p_child_sex == IndividualSex::kFemale)
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// female's copy
					haplosome1 = haplosomes[currentHaplosomeIndex];
					
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				}
				else
				{
					Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// male's -
					haplosome1 = haplosomes[currentHaplosomeIndex];
					
					Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
				}
				
				currentHaplosomeIndex += 1;
				break;
			}
			case ChromosomeType::kHM_HaploidMaleInherited:
			{
				// haploid, inherited clonally from the male for both sexes, like the Y for males
				Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// male's copy
				haplosome1 = haplosomes[currentHaplosomeIndex];
				
				population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				
				currentHaplosomeIndex += 1;
				break;
			}
			case ChromosomeType::kML_HaploidMaleLine:
			{
				// this comes (to males only) clonally from the male parent, just like a Y
				if (p_child_sex == IndividualSex::kMale)
				{
					Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// male's copy
					haplosome1 = haplosomes[currentHaplosomeIndex];
					
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				}
				else
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// female's -
					haplosome1 = haplosomes[currentHaplosomeIndex];
					
					Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
				}
				
				currentHaplosomeIndex += 1;
				break;
			}
			case ChromosomeType::kHNull_HaploidAutosomeWithNull:
			{
				EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed): chromosome type 'H-' does not allow reproduction by biparental cross (only cloning); chromosome type 'H' provides greater flexibility for modeling haploids." << EidosTerminate();
				break;
			}
			case ChromosomeType::kNullY_YSexChromosomeWithNull:
			{
				{
					Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// parent 1 copy 1
					Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex];			// parent 2 copy 1
					haplosome1 = haplosomes[currentHaplosomeIndex];
					
					Haplosome::DebugCheckStructureMatch(parental_haplosome1, parental_haplosome2, haplosome1, chromosome);
				}
				{
					if (p_child_sex == IndividualSex::kMale)
					{
						Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex+1];		// male's Y
						haplosome2 = haplosomes[currentHaplosomeIndex+1];
						
						population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome2, mutation_callbacks);
					}
					else
					{
						Haplosome *parental_haplosome2 = p_parent1->haplosomes_[currentHaplosomeIndex+1];		// female's -
						haplosome2 = haplosomes[currentHaplosomeIndex+1];
						
						Haplosome::DebugCheckStructureMatch(parental_haplosome2, haplosome2, chromosome);
					}
				}
				currentHaplosomeIndex += 2;
				break;
			}
		}
		
		if (f_mutrunexps) chromosome->StopMutationRunExperimentClock("MungeIndividualCrossed()");
		
		// We need to record the null haplosomes for tree-seq; non-null haplosomes were already
		// recorded by the methods above, HaplosomeCrossed() and HaplosomeCloned().  We also
		// have to set their haplosome_id_ as appropriate.
		if (haplosome1)
		{
			if (f_pedigree_rec)
				haplosome1->haplosome_id_ = p_pedigree_id * 2;
			
			if (f_treeseq && haplosome1->IsNull())
					species_.RecordNewHaplosome_NULL(haplosome1);
		}
		if (haplosome2)
		{
			if (f_pedigree_rec)
				haplosome2->haplosome_id_ = p_pedigree_id * 2 + 1;
			
			if (f_treeseq && haplosome2->IsNull())
				species_.RecordNewHaplosome_NULL(haplosome2);
		}
	}
	
	// Run the candidate past modifyChild() callbacks; the first parent subpop's registered callbacks are used
	if (modify_child_callbacks_)
	{
		bool proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, p_parent1, p_parent2, /* p_is_selfing */ false, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, *modify_child_callbacks_);
		
		// If the child was rejected, un-record it and dispose of it
		if (!proposed_child_accepted)
		{
			// back out child state we created; this restores it to a reuseable state
			// FIXME we could back out the assigned pedigree ID too
			
#if SLIM_CLEAR_HAPLOSOMES
			// BCH 10/15/2024: We used to need to clear here, but we no longer do.  We don't even need to free
			// the haplosomes we made above; they will be garbage collected by FreeUnusedMutationRuns().
			int haplosome_count_per_individual = species_.HaplosomeCountPerIndividual();
			
			for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				haplosomes[haplosome_index]->clear_to_nullptr();
#endif
			
			// revoke parentage
			if (f_pedigree_rec)
				individual->RevokeParentage_Biparental(*p_parent1, *p_parent2);
			
			// TREE SEQUENCE RECORDING
			if (f_treeseq)
				species_.RetractNewIndividual();
			
			return false;
		}
	}
	
	return true;
}

template bool Subpopulation::MungeIndividualCrossed<false, false, false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, false, false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, false, false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, false, false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, false, true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, false, true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, false, true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, false, true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, true, false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, true, false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, true, false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, true, false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, true, true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, true, true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, true, true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<false, true, true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, false, false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, false, false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, false, false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, false, false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, false, true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, false, true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, false, true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, false, true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, true, false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, true, false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, true, false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, true, false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, true, true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, true, true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, true, true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed<true, true, true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);

// this more limited templated variant assumes there is one chromosome, the chromosome type is "A", and f_mutrunexps=F and f_callbacks=F
template <const bool f_pedigree_rec, const bool f_treeseq, const bool f_spatial>
bool Subpopulation::MungeIndividualCrossed_1CH_A(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, __attribute__ ((unused)) IndividualSex p_child_sex)
{
#if DEBUG
	Subpopulation &parent1_subpop = *p_parent1->subpopulation_;
	Subpopulation &parent2_subpop = *p_parent2->subpopulation_;
	
	if (&parent1_subpop != &parent2_subpop)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed_1CH): parent1 and parent2 must belong to the same subpopulation; that is assumed, since this method is called only for WF reproduction." << EidosTerminate();
	
	IndividualSex parent1_sex = p_parent1->sex_;
	IndividualSex parent2_sex = p_parent2->sex_;
	
	if ((sex_enabled_ && (parent1_sex != IndividualSex::kFemale)) || (!sex_enabled_ && (parent1_sex != IndividualSex::kHermaphrodite)))
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed_1CH): parent1 must be female in sexual models, or hermaphroditic in non-sexual models." << EidosTerminate();
	if ((sex_enabled_ && (parent2_sex != IndividualSex::kMale)) || (!sex_enabled_ && (parent2_sex != IndividualSex::kHermaphrodite)))
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed_1CH): parent2 must be male in sexual models, or hermaphroditic in non-sexual models." << EidosTerminate();
	if ((p_parent1->index_ == -1) || (p_parent2->index_ == -1))
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed_1CH): parent1 and parent2 must be visible in a subpopulation (i.e., may not be new juveniles)." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if ((&parent1_subpop.species_ != &this->species_) || (&parent2_subpop.species_ != &this->species_))
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed_1CH): biparental crossing requires that both parents belong to the same species as the target subpopulation." << EidosTerminate();
#endif
	
	// Record the offspring
	if (f_pedigree_rec)
		individual->TrackParentage_Biparental(p_pedigree_id, *p_parent1, *p_parent2);
	
	// TREE SEQUENCE RECORDING
	if (f_treeseq)
		species_.SetCurrentNewIndividual(individual);
	
	// BCH 9/26/2023: inherit the spatial position of the first parent by default, to set up for deviatePositions()/pointDeviated()
	if (f_spatial)
		individual->InheritSpatialPosition(species_.SpatialDimensionality(), p_parent1);
	
	// Configure the offspring's haplosomes one by one
	Haplosome **haplosomes = individual->haplosomes_;
	const int currentHaplosomeIndex = 0;
	Chromosome *chromosome = species_.Chromosomes()[0];
	
	// Determine what kind of haplosomes to make for this chromosome
	Haplosome *haplosome1, *haplosome2;
	
	// each haplosome is generated by recombination between a pair of parental haplosomes
	{
		Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];			// parent 1 copy 1
		Haplosome *parental_haplosome2 = p_parent1->haplosomes_[currentHaplosomeIndex+1];		// parent 1 copy 2
		haplosome1 = haplosomes[currentHaplosomeIndex];
		
		population_.HaplosomeCrossed<f_treeseq, false>(*chromosome, *haplosome1, parental_haplosome1, parental_haplosome2, nullptr, nullptr);
	}
	{
		Haplosome *parental_haplosome1 = p_parent2->haplosomes_[currentHaplosomeIndex];			// parent 2 copy 1
		Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex+1];		// parent 2 copy 2
		haplosome2 = haplosomes[currentHaplosomeIndex+1];
		
		population_.HaplosomeCrossed<f_treeseq, false>(*chromosome, *haplosome2, parental_haplosome1, parental_haplosome2, nullptr, nullptr);
	}
	
	{
		if (f_pedigree_rec)
			haplosome1->haplosome_id_ = p_pedigree_id * 2;
	}
	{
		if (f_pedigree_rec)
			haplosome2->haplosome_id_ = p_pedigree_id * 2 + 1;
	}
	
	return true;
}

template bool Subpopulation::MungeIndividualCrossed_1CH_A<false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_A<false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_A<false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_A<false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_A<true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_A<true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_A<true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_A<true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);

// this more limited templated variant assumes there is one chromosome, the chromosome type is "H", and f_mutrunexps=F and f_callbacks=F
template <const bool f_pedigree_rec, const bool f_treeseq, const bool f_spatial>
bool Subpopulation::MungeIndividualCrossed_1CH_H(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, __attribute__ ((unused)) IndividualSex p_child_sex)
{
#if DEBUG
	Subpopulation &parent1_subpop = *p_parent1->subpopulation_;
	Subpopulation &parent2_subpop = *p_parent2->subpopulation_;
	
	if (&parent1_subpop != &parent2_subpop)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed_1CH): parent1 and parent2 must belong to the same subpopulation; that is assumed, since this method is called only for WF reproduction." << EidosTerminate();
	
	IndividualSex parent1_sex = p_parent1->sex_;
	IndividualSex parent2_sex = p_parent2->sex_;
	
	if ((sex_enabled_ && (parent1_sex != IndividualSex::kFemale)) || (!sex_enabled_ && (parent1_sex != IndividualSex::kHermaphrodite)))
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed_1CH): parent1 must be female in sexual models, or hermaphroditic in non-sexual models." << EidosTerminate();
	if ((sex_enabled_ && (parent2_sex != IndividualSex::kMale)) || (!sex_enabled_ && (parent2_sex != IndividualSex::kHermaphrodite)))
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed_1CH): parent2 must be male in sexual models, or hermaphroditic in non-sexual models." << EidosTerminate();
	if ((p_parent1->index_ == -1) || (p_parent2->index_ == -1))
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed_1CH): parent1 and parent2 must be visible in a subpopulation (i.e., may not be new juveniles)." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if ((&parent1_subpop.species_ != &this->species_) || (&parent2_subpop.species_ != &this->species_))
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCrossed_1CH): biparental crossing requires that both parents belong to the same species as the target subpopulation." << EidosTerminate();
#endif
	
	// Record the offspring
	if (f_pedigree_rec)
		individual->TrackParentage_Biparental(p_pedigree_id, *p_parent1, *p_parent2);
	
	// TREE SEQUENCE RECORDING
	if (f_treeseq)
		species_.SetCurrentNewIndividual(individual);
	
	// BCH 9/26/2023: inherit the spatial position of the first parent by default, to set up for deviatePositions()/pointDeviated()
	if (f_spatial)
		individual->InheritSpatialPosition(species_.SpatialDimensionality(), p_parent1);
	
	// Configure the offspring's haplosomes one by one
	Haplosome **haplosomes = individual->haplosomes_;
	const int currentHaplosomeIndex = 0;
	Chromosome *chromosome = species_.Chromosomes()[0];
	
	// Determine what kind of haplosomes to make for this chromosome
	Haplosome *haplosome1;
	
	{
		// the haplosome is generated by recombination between the haplosomes of the two parents
		Haplosome *parental_haplosome1 = p_parent1->haplosomes_[currentHaplosomeIndex];				// parent 1 copy
		Haplosome *parental_haplosome2 = p_parent2->haplosomes_[currentHaplosomeIndex];				// parent 2 copy
		haplosome1 = haplosomes[currentHaplosomeIndex];
		
		population_.HaplosomeCrossed<f_treeseq, false>(*chromosome, *haplosome1, parental_haplosome1, parental_haplosome2, nullptr, nullptr);
	}
	
	{
		if (f_pedigree_rec)
			haplosome1->haplosome_id_ = p_pedigree_id * 2;
	}
	
	return true;
}

template bool Subpopulation::MungeIndividualCrossed_1CH_H<false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_H<false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_H<false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_H<false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_H<true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_H<true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_H<true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
template bool Subpopulation::MungeIndividualCrossed_1CH_H<true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);

template <const bool f_mutrunexps, const bool f_pedigree_rec, const bool f_treeseq, const bool f_callbacks, const bool f_spatial>
bool Subpopulation::MungeIndividualSelfed(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent)
{
	Subpopulation &parent_subpop = *p_parent->subpopulation_;
	
#if DEBUG
	IndividualSex parent_sex = p_parent->sex_;
	
	if (parent_sex != IndividualSex::kHermaphrodite)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualSelfed): parent must be hermaphroditic." << EidosTerminate();
	if (p_parent->index_ == -1)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualSelfed): parent must be visible in a subpopulation (i.e., may not be a new juvenile)." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if (&parent_subpop.species_ != &this->species_)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualSelfed): selfing requires that parent belongs to the same species as the target subpopulation." << EidosTerminate();
#endif
	
	// Figure out callbacks, which are based on the subpopulation of each parent
	std::vector<SLiMEidosBlock*> *recombination_callbacks = nullptr;
	std::vector<SLiMEidosBlock*> *mutation_callbacks = nullptr;
	std::vector<SLiMEidosBlock*> *modify_child_callbacks_ = nullptr;
	
	if (f_callbacks)
	{
		recombination_callbacks = &parent_subpop.registered_recombination_callbacks_;
		mutation_callbacks = &parent_subpop.registered_mutation_callbacks_;
		modify_child_callbacks_ = &parent_subpop.registered_modify_child_callbacks_;
		
		if (!recombination_callbacks->size()) recombination_callbacks = nullptr;
		if (!mutation_callbacks->size()) mutation_callbacks = nullptr;
		if (!modify_child_callbacks_->size()) modify_child_callbacks_ = nullptr;
	}
	
	// Record the offspring
	if (f_pedigree_rec)
		individual->TrackParentage_Uniparental(p_pedigree_id, *p_parent);
	
	// TREE SEQUENCE RECORDING
	if (f_treeseq)
		species_.SetCurrentNewIndividual(individual);
	
	// BCH 9/26/2023: inherit the spatial position of the first parent by default, to set up for deviatePositions()/pointDeviated()
	if (f_spatial)
		individual->InheritSpatialPosition(species_.SpatialDimensionality(), p_parent);
	
	// Configure the offspring's haplosomes one by one
	Haplosome **haplosomes = individual->haplosomes_;
	int currentHaplosomeIndex = 0;
	
	for (Chromosome *chromosome : species_.Chromosomes())
	{
#if DEBUG
		if (!species_.HasGenetics())
			EIDOS_TERMINATION << "ERROR (Population::MungeIndividualSelfed): (internal error) a chromosome is defined for a no-genetics species!" << EidosTerminate();
#endif
		
		if (f_mutrunexps) chromosome->StartMutationRunExperimentClock();
		
		// Determine what kind of haplosomes to make for this chromosome
		ChromosomeType chromosomeType = chromosome->Type();
		Haplosome *haplosome1 = nullptr, *haplosome2 = nullptr;
		
		switch (chromosomeType)
		{
			case ChromosomeType::kA_DiploidAutosome:
			{
				// each haplosome is generated by recombination between the pair of parental haplosomes
				Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];			// parent copy 1
				Haplosome *parental_haplosome2 = p_parent->haplosomes_[currentHaplosomeIndex+1];		// parent copy 2
				
				haplosome1 = haplosomes[currentHaplosomeIndex];
				population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, parental_haplosome2, recombination_callbacks, mutation_callbacks);
				
				haplosome2 = haplosomes[currentHaplosomeIndex+1];
				population_.HaplosomeCrossed<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome1, parental_haplosome2, recombination_callbacks, mutation_callbacks);
				
				currentHaplosomeIndex += 2;
				break;
			}
			case ChromosomeType::kH_HaploidAutosome:
			{
				// the haplosome is generated by recombination between the haplosome of the parent and itself
				// but since the one haplosome is identical to itself, that is the same thing as cloning
				Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];			// parent copy
				haplosome1 = haplosomes[currentHaplosomeIndex];
				
				population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				
				currentHaplosomeIndex += 1;
				break;
			}
			case ChromosomeType::kHNull_HaploidAutosomeWithNull:
			{
				EIDOS_TERMINATION << "ERROR (Population::MungeIndividualSelfed): chromosome type 'H-' does not allow reproduction by selfing (only cloning); chromosome type 'H' provides greater flexibility for modeling haploids." << EidosTerminate();
				break;
			}
			case ChromosomeType::kX_XSexChromosome:
			case ChromosomeType::kY_YSexChromosome:
			case ChromosomeType::kZ_ZSexChromosome:
			case ChromosomeType::kW_WSexChromosome:
			case ChromosomeType::kHF_HaploidFemaleInherited:
			case ChromosomeType::kFL_HaploidFemaleLine:
			case ChromosomeType::kHM_HaploidMaleInherited:
			case ChromosomeType::kML_HaploidMaleLine:
			case ChromosomeType::kNullY_YSexChromosomeWithNull:
				EIDOS_TERMINATION << "ERROR (Population::MungeIndividualSelfed): (internal error) sex-specific chromosome type not supported for selfing." << EidosTerminate();
				break;
		}
		
		if (f_mutrunexps) chromosome->StopMutationRunExperimentClock("MungeIndividualSelfed()");
		
		// We need to record the null haplosomes for tree-seq; non-null haplosomes were already
		// recorded by the methods above, HaplosomeCrossed() and HaplosomeCloned().  We also
		// have to set their haplosome_id_ as appropriate.
		if (haplosome1)
		{
			if (f_pedigree_rec)
				haplosome1->haplosome_id_ = p_pedigree_id * 2;
			
			if (f_treeseq && haplosome1->IsNull())
					species_.RecordNewHaplosome_NULL(haplosome1);
		}
		if (haplosome2)
		{
			if (f_pedigree_rec)
				haplosome2->haplosome_id_ = p_pedigree_id * 2 + 1;
			
			if (f_treeseq && haplosome2->IsNull())
				species_.RecordNewHaplosome_NULL(haplosome2);
		}
	}
	
	// Run the candidate past modifyChild() callbacks; the first parent subpop's registered callbacks are used
	if (modify_child_callbacks_)
	{
		bool proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, p_parent, p_parent, /* p_is_selfing */ true, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, *modify_child_callbacks_);
		
		// If the child was rejected, un-record it and dispose of it
		if (!proposed_child_accepted)
		{
			// back out child state we created; this restores it to a reuseable state
			// FIXME we could back out the assigned pedigree ID too
			
#if SLIM_CLEAR_HAPLOSOMES
			// BCH 10/15/2024: We used to need to clear here, but we no longer do.  We don't even need to free
			// the haplosomes we made above; they will be garbage collected by FreeUnusedMutationRuns().
			int haplosome_count_per_individual = species_.HaplosomeCountPerIndividual();
			
			for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				haplosomes[haplosome_index]->clear_to_nullptr();
#endif
			
			// revoke parentage
			if (f_pedigree_rec)
				individual->RevokeParentage_Uniparental(*p_parent);
			
			// TREE SEQUENCE RECORDING
			if (f_treeseq)
				species_.RetractNewIndividual();
			
			return false;
		}
	}
	
	return true;
}

template bool Subpopulation::MungeIndividualSelfed<false, false, false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, false, false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, false, false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, false, false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, false, true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, false, true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, false, true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, false, true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, true, false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, true, false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, true, false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, true, false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, true, true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, true, true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, true, true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<false, true, true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, false, false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, false, false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, false, false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, false, false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, false, true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, false, true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, false, true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, false, true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, true, false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, true, false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, true, false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, true, false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, true, true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, true, true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, true, true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualSelfed<true, true, true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);

template <const bool f_mutrunexps, const bool f_pedigree_rec, const bool f_treeseq, const bool f_callbacks, const bool f_spatial>
bool Subpopulation::MungeIndividualCloned(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent)
{
	IndividualSex parent_sex = p_parent->sex_;
	Subpopulation &parent_subpop = *p_parent->subpopulation_;
	
#if DEBUG
	if (p_parent->index_ == -1)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCloned): parent must be visible in a subpopulation (i.e., may not be a new juvenile)." << EidosTerminate();
	if (individual->sex_ != parent_sex)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCloned): child sex does not match parent sex (which, for cloning, it should)." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if (&parent_subpop.species_ != &this->species_)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCloned): cloning requires that parent belongs to the same species as the target subpopulation." << EidosTerminate();
#endif
	
	// Figure out callbacks, which are based on the subpopulation of the parents (which must be the same)
	std::vector<SLiMEidosBlock*> *mutation_callbacks = nullptr;
	std::vector<SLiMEidosBlock*> *modify_child_callbacks_ = nullptr;
	
	if (f_callbacks)
	{
		// Figure out callbacks, which are based on the subpopulation of each parent
		mutation_callbacks = &parent_subpop.registered_mutation_callbacks_;
		modify_child_callbacks_ = &parent_subpop.registered_modify_child_callbacks_;
		
		if (!mutation_callbacks->size()) mutation_callbacks = nullptr;
		if (!modify_child_callbacks_->size()) modify_child_callbacks_ = nullptr;
	}
	
	// Record the offspring
	if (f_pedigree_rec)
		individual->TrackParentage_Uniparental(p_pedigree_id, *p_parent);
	
	// TREE SEQUENCE RECORDING
	if (f_treeseq)
		species_.SetCurrentNewIndividual(individual);
	
	// BCH 9/26/2023: inherit the spatial position of the parent by default, to set up for deviatePositions()/pointDeviated()
	if (f_spatial)
		individual->InheritSpatialPosition(species_.SpatialDimensionality(), p_parent);
	
	// Configure the offspring's haplosomes one by one
	Haplosome **haplosomes = individual->haplosomes_;
	int currentHaplosomeIndex = 0;
	
	for (Chromosome *chromosome : species_.Chromosomes())
	{
#if DEBUG
		if (!species_.HasGenetics())
			EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCloned): (internal error) a chromosome is defined for a no-genetics species!" << EidosTerminate();
#endif
		
		if (f_mutrunexps) chromosome->StartMutationRunExperimentClock();
		
		// Determine what kind of haplosomes to make for this chromosome
		// We just faithfully clone the existing haplosomes of the parent, regardless of type
		ChromosomeType chromosomeType = chromosome->Type();
		Haplosome *haplosome1 = nullptr, *haplosome2 = nullptr;
		
		switch (chromosomeType)
		{
				// these chromosome types keep two haplosomes per individual
			case ChromosomeType::kA_DiploidAutosome:
			{
				{
					Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];
					haplosome1 = haplosomes[currentHaplosomeIndex];
					
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				}
				{
					Haplosome *parental_haplosome2 = p_parent->haplosomes_[currentHaplosomeIndex+1];
					haplosome2 = haplosomes[currentHaplosomeIndex+1];
					
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome2, mutation_callbacks);
				}
				currentHaplosomeIndex += 2;
				break;
			}
			case ChromosomeType::kX_XSexChromosome:
			{
				{
					Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];
					haplosome1 = haplosomes[currentHaplosomeIndex];
					
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				}
				{
					Haplosome *parental_haplosome2 = p_parent->haplosomes_[currentHaplosomeIndex+1];
					haplosome2 = haplosomes[currentHaplosomeIndex+1];
					
					if (parent_sex == IndividualSex::kFemale)
						population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome2, mutation_callbacks);
					else
						Haplosome::DebugCheckStructureMatch(parental_haplosome2, haplosome2, chromosome);
				}
				currentHaplosomeIndex += 2;
				break;
			}
			case ChromosomeType::kZ_ZSexChromosome:
			{
				Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];
				haplosome1 = haplosomes[currentHaplosomeIndex];
				
				if (parent_sex == IndividualSex::kMale)
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				else
					Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
				
				Haplosome *parental_haplosome2 = p_parent->haplosomes_[currentHaplosomeIndex+1];
				haplosome2 = haplosomes[currentHaplosomeIndex+1];
				
				population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome2, mutation_callbacks);
				
				currentHaplosomeIndex += 2;
				break;
			}
			case ChromosomeType::kHNull_HaploidAutosomeWithNull:
			{
				Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];
				haplosome1 = haplosomes[currentHaplosomeIndex];
				
				population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				
				Haplosome *parental_haplosome2 = p_parent->haplosomes_[currentHaplosomeIndex+1];
				haplosome2 = haplosomes[currentHaplosomeIndex+1];
				
				Haplosome::DebugCheckStructureMatch(parental_haplosome2, haplosome2, chromosome);
				
				currentHaplosomeIndex += 2;
				break;
			}
			case ChromosomeType::kNullY_YSexChromosomeWithNull:
			{
				Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];
				haplosome1 = haplosomes[currentHaplosomeIndex];
				
				Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
			
				Haplosome *parental_haplosome2 = p_parent->haplosomes_[currentHaplosomeIndex+1];
				haplosome2 = haplosomes[currentHaplosomeIndex+1];
				
				if (parent_sex == IndividualSex::kMale)
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome2, parental_haplosome2, mutation_callbacks);
				else
					Haplosome::DebugCheckStructureMatch(parental_haplosome2, haplosome2, chromosome);
				
				currentHaplosomeIndex += 2;
				break;
			}
				
				// these chromosome types keep one haplosome per individual
			case ChromosomeType::kH_HaploidAutosome:
			case ChromosomeType::kHM_HaploidMaleInherited:		// note male inheritance is not honored by cloning
			case ChromosomeType::kHF_HaploidFemaleInherited:	// note female inheritance is not honored by cloning
			{
				Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];	// parent 1 copy
				haplosome1 = haplosomes[currentHaplosomeIndex];
				
				population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				
				currentHaplosomeIndex += 1;
				break;
			}
			case ChromosomeType::kY_YSexChromosome:
			case ChromosomeType::kML_HaploidMaleLine:
			{
				Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];
				haplosome1 = haplosomes[currentHaplosomeIndex];
				
				if (parent_sex == IndividualSex::kMale)
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				else
					Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
				
				currentHaplosomeIndex += 1;
				break;
			}
			case ChromosomeType::kW_WSexChromosome:
			case ChromosomeType::kFL_HaploidFemaleLine:
			{
				Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];
				haplosome1 = haplosomes[currentHaplosomeIndex];
				
				if (parent_sex == IndividualSex::kFemale)
					population_.HaplosomeCloned<f_treeseq, f_callbacks>(*chromosome, *haplosome1, parental_haplosome1, mutation_callbacks);
				else
					Haplosome::DebugCheckStructureMatch(parental_haplosome1, haplosome1, chromosome);
				
				currentHaplosomeIndex += 1;
				break;
			}
		}
		
		if (f_mutrunexps) chromosome->StopMutationRunExperimentClock("MungeIndividualCloned()");
		
		// We need to record the null haplosomes for tree-seq; non-null haplosomes were already
		// recorded by the methods above, HaplosomeCrossed() and HaplosomeCloned().  We also
		// have to set their haplosome_id_ as appropriate.
		if (haplosome1)
		{
			if (f_pedigree_rec)
				haplosome1->haplosome_id_ = p_pedigree_id * 2;
			
			if (f_treeseq && haplosome1->IsNull())
				species_.RecordNewHaplosome_NULL(haplosome1);
		}
		if (haplosome2)
		{
			if (f_pedigree_rec)
				haplosome2->haplosome_id_ = p_pedigree_id * 2 + 1;
			
			if (f_treeseq && haplosome2->IsNull())
				species_.RecordNewHaplosome_NULL(haplosome2);
		}
	}
	
	// Run the candidate past modifyChild() callbacks; the first parent subpop's registered callbacks are used
	if (modify_child_callbacks_)
	{
		bool proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, p_parent, p_parent, /* p_is_selfing */ false, /* p_is_cloning */ true, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, *modify_child_callbacks_);
		
		// If the child was rejected, un-record it and dispose of it
		if (!proposed_child_accepted)
		{
			// back out child state we created; this restores it to a reuseable state
			// FIXME we could back out the assigned pedigree ID too
			
#if SLIM_CLEAR_HAPLOSOMES
			// BCH 10/15/2024: We used to need to clear here, but we no longer do.  We don't even need to free
			// the haplosomes we made above; they will be garbage collected by FreeUnusedMutationRuns().
			int haplosome_count_per_individual = species_.HaplosomeCountPerIndividual();
			
			for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				haplosomes[haplosome_index]->clear_to_nullptr();
#endif
			
			// revoke parentage
			if (f_pedigree_rec)
				individual->RevokeParentage_Uniparental(*p_parent);
			
			// TREE SEQUENCE RECORDING
			if (f_treeseq)
				species_.RetractNewIndividual();
			
			return false;
		}
	}
	
	return true;
}

template bool Subpopulation::MungeIndividualCloned<false, false, false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, false, false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, false, false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, false, false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, false, true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, false, true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, false, true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, false, true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, true, false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, true, false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, true, false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, true, false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, true, true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, true, true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, true, true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<false, true, true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, false, false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, false, false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, false, false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, false, false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, false, true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, false, true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, false, true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, false, true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, true, false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, true, false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, true, false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, true, false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, true, true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, true, true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, true, true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned<true, true, true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);

template <const bool f_pedigree_rec, const bool f_treeseq, const bool f_spatial>
bool Subpopulation::MungeIndividualCloned_1CH_A(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent)
{
#if DEBUG
	IndividualSex parent_sex = p_parent->sex_;
	Subpopulation &parent_subpop = *p_parent->subpopulation_;
	
	if (p_parent->index_ == -1)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCloned): parent must be visible in a subpopulation (i.e., may not be a new juvenile)." << EidosTerminate();
	if (individual->sex_ != parent_sex)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCloned): child sex does not match parent sex (which, for cloning, it should)." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if (&parent_subpop.species_ != &this->species_)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCloned): cloning requires that parent belongs to the same species as the target subpopulation." << EidosTerminate();
#endif
	
	// Record the offspring
	if (f_pedigree_rec)
		individual->TrackParentage_Uniparental(p_pedigree_id, *p_parent);
	
	// TREE SEQUENCE RECORDING
	if (f_treeseq)
		species_.SetCurrentNewIndividual(individual);
	
	// BCH 9/26/2023: inherit the spatial position of the parent by default, to set up for deviatePositions()/pointDeviated()
	if (f_spatial)
		individual->InheritSpatialPosition(species_.SpatialDimensionality(), p_parent);
	
	// Configure the offspring's haplosomes one by one
	Haplosome **haplosomes = individual->haplosomes_;
	const int currentHaplosomeIndex = 0;
	Chromosome *chromosome = species_.Chromosomes()[0];
	
#if DEBUG
	if (!species_.HasGenetics())
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCloned): (internal error) a chromosome is defined for a no-genetics species!" << EidosTerminate();
#endif
	
	// Determine what kind of haplosomes to make for this chromosome
	// We just faithfully clone the existing haplosomes of the parent, regardless of type
	Haplosome *haplosome1, *haplosome2;
	
	{
		Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];
		haplosome1 = haplosomes[currentHaplosomeIndex];
		
		population_.HaplosomeCloned<f_treeseq, false>(*chromosome, *haplosome1, parental_haplosome1, nullptr);
	}
	{
		Haplosome *parental_haplosome2 = p_parent->haplosomes_[currentHaplosomeIndex+1];
		haplosome2 = haplosomes[currentHaplosomeIndex+1];
		
		population_.HaplosomeCloned<f_treeseq, false>(*chromosome, *haplosome2, parental_haplosome2, nullptr);
	}
	
	{
		if (f_pedigree_rec)
			haplosome1->haplosome_id_ = p_pedigree_id * 2;
		
		if (f_treeseq && haplosome1->IsNull())
			species_.RecordNewHaplosome_NULL(haplosome1);
	}
	{
		if (f_pedigree_rec)
			haplosome2->haplosome_id_ = p_pedigree_id * 2 + 1;
		
		if (f_treeseq && haplosome2->IsNull())
			species_.RecordNewHaplosome_NULL(haplosome2);
	}
	
	return true;
}

template bool Subpopulation::MungeIndividualCloned_1CH_A<false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_A<false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_A<false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_A<false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_A<true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_A<true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_A<true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_A<true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);

template <const bool f_pedigree_rec, const bool f_treeseq, const bool f_spatial>
bool Subpopulation::MungeIndividualCloned_1CH_H(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent)
{
#if DEBUG
	IndividualSex parent_sex = p_parent->sex_;
	Subpopulation &parent_subpop = *p_parent->subpopulation_;
	
	if (p_parent->index_ == -1)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCloned): parent must be visible in a subpopulation (i.e., may not be a new juvenile)." << EidosTerminate();
	if (individual->sex_ != parent_sex)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCloned): child sex does not match parent sex (which, for cloning, it should)." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if (&parent_subpop.species_ != &this->species_)
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCloned): cloning requires that parent belongs to the same species as the target subpopulation." << EidosTerminate();
#endif
	
	// Record the offspring
	if (f_pedigree_rec)
		individual->TrackParentage_Uniparental(p_pedigree_id, *p_parent);
	
	// TREE SEQUENCE RECORDING
	if (f_treeseq)
		species_.SetCurrentNewIndividual(individual);
	
	// BCH 9/26/2023: inherit the spatial position of the parent by default, to set up for deviatePositions()/pointDeviated()
	if (f_spatial)
		individual->InheritSpatialPosition(species_.SpatialDimensionality(), p_parent);
	
	// Configure the offspring's haplosomes one by one
	Haplosome **haplosomes = individual->haplosomes_;
	const int currentHaplosomeIndex = 0;
	Chromosome *chromosome = species_.Chromosomes()[0];
	
#if DEBUG
	if (!species_.HasGenetics())
		EIDOS_TERMINATION << "ERROR (Population::MungeIndividualCloned): (internal error) a chromosome is defined for a no-genetics species!" << EidosTerminate();
#endif
	
	Haplosome *haplosome1;
	
	{
		Haplosome *parental_haplosome1 = p_parent->haplosomes_[currentHaplosomeIndex];	// parent 1 copy
		haplosome1 = haplosomes[currentHaplosomeIndex];
		
		population_.HaplosomeCloned<f_treeseq, false>(*chromosome, *haplosome1, parental_haplosome1, nullptr);
	}
	
	{
		if (f_pedigree_rec)
			haplosome1->haplosome_id_ = p_pedigree_id * 2;
		
		if (f_treeseq && haplosome1->IsNull())
			species_.RecordNewHaplosome_NULL(haplosome1);
	}
	
	return true;
}

template bool Subpopulation::MungeIndividualCloned_1CH_H<false, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_H<false, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_H<false, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_H<false, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_H<true, false, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_H<true, false, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_H<true, true, false>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
template bool Subpopulation::MungeIndividualCloned_1CH_H<true, true, true>(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);

// nonWF only:
void Subpopulation::ApplyReproductionCallbacks(std::vector<SLiMEidosBlock*> &p_reproduction_callbacks, slim_popsize_t p_individual_index)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Population::ApplyReproductionCallbacks(): running Eidos callback");
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	Individual *individual = parent_individuals_[p_individual_index];
	
	for (SLiMEidosBlock *reproduction_callback : p_reproduction_callbacks)
	{
		if (reproduction_callback->block_active_)
		{
			IndividualSex sex_specificity = reproduction_callback->sex_specificity_;
			
			if ((sex_specificity == IndividualSex::kUnspecified) || (sex_specificity == individual->sex_))
			{
#if DEBUG_POINTS_ENABLED
				// SLiMgui debugging point
				EidosDebugPointIndent indenter;
				
				{
					EidosInterpreterDebugPointsSet *debug_points = community_.DebugPoints();
					EidosToken *decl_token = reproduction_callback->root_node_->token_;
					
					if (debug_points && debug_points->set.size() && (decl_token->token_line_ != -1) &&
						(debug_points->set.find(decl_token->token_line_) != debug_points->set.end()))
					{
						SLIM_ERRSTREAM << EidosDebugPointIndent::Indent() << "#DEBUG reproduction(";
						if ((reproduction_callback->subpopulation_id_ != -1) && (reproduction_callback->sex_specificity_ != IndividualSex::kUnspecified))
							SLIM_ERRSTREAM << "p" << reproduction_callback->subpopulation_id_ << ", \"" << reproduction_callback->sex_specificity_ << "\"";
						else if (reproduction_callback->subpopulation_id_ != -1)
							SLIM_ERRSTREAM << "p" << reproduction_callback->subpopulation_id_;
						else if (reproduction_callback->sex_specificity_ != IndividualSex::kUnspecified)
							SLIM_ERRSTREAM << "NULL, \"" << reproduction_callback->sex_specificity_ << "\"";
						SLIM_ERRSTREAM << ")";
						
						if (reproduction_callback->block_id_ != -1)
							SLIM_ERRSTREAM << " s" << reproduction_callback->block_id_;
						
						SLIM_ERRSTREAM << " (line " << (decl_token->token_line_ + 1) << community_.DebugPointInfo() << ")" << std::endl;
						indenter.indent();
					}
				}
#endif
				
				// This code is similar to Population::ExecuteScript, but we set up an additional symbol table, and we use the return value
				// We need to actually execute the script; we start a block here to manage the lifetime of the symbol table
				{
					EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &community_.SymbolTable());
					EidosSymbolTable client_symbols(EidosSymbolTableType::kLocalVariablesTable, &callback_symbols);
					EidosFunctionMap &function_map = community_.FunctionMap();
					EidosInterpreter interpreter(reproduction_callback->compound_statement_node_, client_symbols, function_map, &community_, SLIM_OUTSTREAM, SLIM_ERRSTREAM);
					
					if (reproduction_callback->contains_self_)
						callback_symbols.InitializeConstantSymbolEntry(reproduction_callback->SelfSymbolTableEntry());		// define "self"
					
					// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
					// We can use that method because we know the lifetime of the symbol table is shorter than that of
					// the value objects, and we know that the values we are setting here will not change (the objects
					// referred to by the values may change, but the values themselves will not change).
					if (reproduction_callback->contains_individual_)
						callback_symbols.InitializeConstantSymbolEntry(gID_individual, individual->CachedEidosValue());
					if (reproduction_callback->contains_subpop_)
						callback_symbols.InitializeConstantSymbolEntry(gID_subpop, SymbolTableEntry().second);
					
					try
					{
						// Interpret the script; the result from the interpretation must be NULL, for now (to allow future extension)
						EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(reproduction_callback->script_);
						EidosValue *result = result_SP.get();
						
						if (result->Type() != EidosValueType::kValueVOID)
						{
							if (result->Type() == EidosValueType::kValueNULL)
								EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyReproductionCallbacks): reproduction() callbacks must not return a value (i.e., must return void).  (NULL has been returned here instead; NULL was the required return value in the SLiM 3 prerelease, but the policy has been changed.)" << EidosTerminate(reproduction_callback->identifier_token_);
							
							EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyReproductionCallbacks): reproduction() callbacks must not return a value (i.e., must return void)." << EidosTerminate(reproduction_callback->identifier_token_);
						}
					}
					catch (...)
					{
						throw;
					}
					
				}
			}
		}
	}
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosReproductionCallback)]);
#endif
}

// nonWF only:
void Subpopulation::ReproduceSubpopulation(void)
{
	// if there are no reproduction() callbacks active, we can avoid all the work
	if (registered_reproduction_callbacks_.size() == 0)
		return;
	
	if (species_.RandomizingCallbackOrder())
	{
		slim_popsize_t *shuffle_buf = species_.BorrowShuffleBuffer(parent_subpop_size_);
		
		for (slim_popsize_t shuffle_index = 0; shuffle_index < parent_subpop_size_; shuffle_index++)
		{
			slim_popsize_t individual_index = shuffle_buf[shuffle_index];
			
			ApplyReproductionCallbacks(registered_reproduction_callbacks_, individual_index);
		}
		
		species_.ReturnShuffleBuffer();
	}
	else
	{
		for (int individual_index = 0; individual_index < parent_subpop_size_; ++individual_index)
			ApplyReproductionCallbacks(registered_reproduction_callbacks_, individual_index);
	}
}

// nonWF only:
void Subpopulation::MergeReproductionOffspring(void)
{
	// NOTE: this method is called by Population::ResolveSurvivalPhaseMovement() as well as by reproduction, since the logic is identical!
	int new_count = (int)nonWF_offspring_individuals_.size();
	
	if (sex_enabled_)
	{
		// resize to create new slots for the new individuals
		try {
			parent_individuals_.resize(parent_individuals_.size() + new_count);
		}
		catch (...) {
			EIDOS_TERMINATION << "ERROR (Subpopulation::MergeReproductionOffspring): (internal error) resize() exception with parent_individuals_.size() == " << parent_individuals_.size() << ", new_count == " << new_count << "." << EidosTerminate();
		}
		
		// in sexual models, females must be put before males and parent_first_male_index_ must be adjusted
		Individual **parent_individual_ptrs = parent_individuals_.data();
		int old_male_count = parent_subpop_size_ - parent_first_male_index_;
		int new_female_count = 0;
		
		// count the new females
		for (int new_index = 0; new_index < new_count; ++new_index)
			if (nonWF_offspring_individuals_[new_index]->sex_ == IndividualSex::kFemale)
				new_female_count++;
		
		// move old males up that many slots to make room; need to fix the index_ ivars of the moved males
		memmove(parent_individual_ptrs + parent_first_male_index_ + new_female_count, parent_individual_ptrs + parent_first_male_index_, old_male_count * sizeof(Individual *));
		
		for (int moved_index = 0; moved_index < old_male_count; moved_index++)
		{
			int new_index = parent_first_male_index_ + new_female_count + moved_index;
			
			parent_individual_ptrs[new_index]->index_ = new_index;
		}
		
		// put all the new stuff into place; if the code above did its job, there are slots waiting for each item
		int new_female_position = parent_first_male_index_;
		int new_male_position = parent_subpop_size_ + new_female_count;
		
		for (int new_index = 0; new_index < new_count; ++new_index)
		{
			Individual *individual = nonWF_offspring_individuals_[new_index];
			slim_popsize_t insert_index;
			
			if (individual->sex_ == IndividualSex::kFemale)
				insert_index = new_female_position++;
			else
				insert_index = new_male_position++;
			
			individual->index_ = insert_index;
			parent_individual_ptrs[insert_index] = individual;
		}
		
		parent_first_male_index_ += new_female_count;
	}
	else
	{
		// reserve space for the new offspring to be merged in
		try {
			parent_individuals_.reserve(parent_individuals_.size() + new_count);
		}
		catch (...) {
			EIDOS_TERMINATION << "ERROR (Subpopulation::MergeReproductionOffspring): (internal error) reserve() exception with parent_individuals_.size() == " << parent_individuals_.size() << ", new_count == " << new_count << "." << EidosTerminate();
		}
		
		// in hermaphroditic models there is no ordering, so just add new stuff at the end
		for (int new_index = 0; new_index < new_count; ++new_index)
		{
			Individual *individual = nonWF_offspring_individuals_[new_index];
			
			individual->index_ = parent_subpop_size_ + new_index;
			parent_individuals_.emplace_back(individual);
		}
	}
	
	// final cleanup
	parent_subpop_size_ += new_count;
	
	cached_parent_individuals_value_.reset();
	
	nonWF_offspring_individuals_.resize(0);
}

// nonWF only:
bool Subpopulation::ApplySurvivalCallbacks(std::vector<SLiMEidosBlock*> &p_survival_callbacks, Individual *p_individual, double p_fitness, double p_draw, bool p_surviving)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Population::ApplySurvivalCallbacks(): running Eidos callback");
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	Subpopulation *move_destination = nullptr;
	
	for (SLiMEidosBlock *survival_callback : p_survival_callbacks)
	{
		if (survival_callback->block_active_)
		{
#ifndef DEBUG_POINTS_ENABLED
#error "DEBUG_POINTS_ENABLED is not defined; include eidos_globals.h"
#endif
#if DEBUG_POINTS_ENABLED
			// SLiMgui debugging point
			EidosDebugPointIndent indenter;
			
			{
				EidosInterpreterDebugPointsSet *debug_points = community_.DebugPoints();
				EidosToken *decl_token = survival_callback->root_node_->token_;
				
				if (debug_points && debug_points->set.size() && (decl_token->token_line_ != -1) &&
					(debug_points->set.find(decl_token->token_line_) != debug_points->set.end()))
				{
					SLIM_ERRSTREAM << EidosDebugPointIndent::Indent() << "#DEBUG survival(";
					if (survival_callback->subpopulation_id_ != -1)
						SLIM_ERRSTREAM << "p" << survival_callback->subpopulation_id_;
					SLIM_ERRSTREAM << ")";
					
					if (survival_callback->block_id_ != -1)
						SLIM_ERRSTREAM << " s" << survival_callback->block_id_;
					
					SLIM_ERRSTREAM << " (line " << (decl_token->token_line_ + 1) << community_.DebugPointInfo() << ")" << std::endl;
					indenter.indent();
				}
			}
#endif
			
			// The callback is active, so we need to execute it
			// This code is similar to Population::ExecuteScript, but we set up an additional symbol table, and we use the return value
			{
				// local variables for the callback parameters that we might need to allocate here, and thus need to free below
				EidosValue_Float local_fitness(p_fitness);
				EidosValue_Float local_draw(p_draw);
				
				// We need to actually execute the script; we start a block here to manage the lifetime of the symbol table
				{
					EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &community_.SymbolTable());
					EidosSymbolTable client_symbols(EidosSymbolTableType::kLocalVariablesTable, &callback_symbols);
					EidosFunctionMap &function_map = community_.FunctionMap();
					EidosInterpreter interpreter(survival_callback->compound_statement_node_, client_symbols, function_map, &community_, SLIM_OUTSTREAM, SLIM_ERRSTREAM);
					
					if (survival_callback->contains_self_)
						callback_symbols.InitializeConstantSymbolEntry(survival_callback->SelfSymbolTableEntry());		// define "self"
					
					// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
					// We can use that method because we know the lifetime of the symbol table is shorter than that of
					// the value objects, and we know that the values we are setting here will not change (the objects
					// referred to by the values may change, but the values themselves will not change).
					if (survival_callback->contains_fitness_)
					{
						local_fitness.StackAllocated();		// prevent Eidos_intrusive_ptr from trying to delete this
						callback_symbols.InitializeConstantSymbolEntry(gID_fitness, EidosValue_SP(&local_fitness));
					}
					if (survival_callback->contains_draw_)
					{
						local_draw.StackAllocated();		// prevent Eidos_intrusive_ptr from trying to delete this
						callback_symbols.InitializeConstantSymbolEntry(gID_draw, EidosValue_SP(&local_draw));
					}
					if (survival_callback->contains_individual_)
						callback_symbols.InitializeConstantSymbolEntry(gID_individual, p_individual->CachedEidosValue());
					if (survival_callback->contains_subpop_)
						callback_symbols.InitializeConstantSymbolEntry(gID_subpop, SymbolTableEntry().second);
					if (survival_callback->contains_surviving_)
						callback_symbols.InitializeConstantSymbolEntry(gID_surviving, p_surviving ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
					
					try
					{
						// Interpret the script; the result from the interpretation must be a singleton double used as a new fitness value
						EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(survival_callback->script_);
						EidosValue *result = result_SP.get();
						EidosValueType result_type = result->Type();
						
						if (result_type == EidosValueType::kValueNULL)
						{
							// NULL means don't change the existing decision
						}
						else if ((result_type == EidosValueType::kValueLogical) &&
								 (result->Count() == 1))
						{
							// T or F means change the existing decision to that value
#if DEBUG
							// this checks the value type at runtime
							p_surviving = result->LogicalData()[0];
#else
							// unsafe cast for speed
							p_surviving = ((EidosValue_Logical *)result)->data()[0];
#endif
							
							move_destination = nullptr;		// cancel a previously made move decision; T/F says "do not move"
						}
						else if ((result_type == EidosValueType::kValueObject) &&
								 (result->Count() == 1) &&
								 (((EidosValue_Object *)result)->Class() == gSLiM_Subpopulation_Class))
						{
							// a Subpopulation object means the individual should move to that subpop (and live); this is done in a post-pass
							// moving to one's current subpopulation is not-moving; it is equivalent to returning T (i.e., forces survival)
							p_surviving = true;
							
#if DEBUG
							// this checks the value type at runtime
							Subpopulation *destination = (Subpopulation *)result->ObjectData()[0];
#else
							// unsafe cast for speed
							Subpopulation *destination = (Subpopulation *)((EidosValue_Object *)result)->data()[0];
#endif
							
							if (destination != this)
								move_destination = destination;
						}
						else
						{
							EIDOS_TERMINATION << "ERROR (Subpopulation::ApplySurvivalCallbacks): survival() callbacks must provide a return value of NULL, T, F, or object<Subpopulation>$." << EidosTerminate(survival_callback->identifier_token_);
						}
					}
					catch (...)
					{
						throw;
					}
				}
			}
		}
	}
	
	if (move_destination)
	{
		// if the callbacks used settled upon a decision of moving the individual, note that now; we do this in a delayed fashion
		// so that if there is more than one survival() callback active, we register only the final verdict after all callbacks
		move_destination->nonWF_survival_moved_individuals_.emplace_back(p_individual);
	}
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosSurvivalCallback)]);
#endif
	
	return p_surviving;
}

void Subpopulation::ViabilitySurvival(std::vector<SLiMEidosBlock*> &p_survival_callbacks)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Subpopulation::ViabilitySurvival(): usage of statics, probably many other issues");
	
	// Loop through our individuals and do draws based on fitness to determine who dies; dead individuals get compacted out
	Individual **individual_data = parent_individuals_.data();
	int survived_individual_index = 0;
	int females_deceased = 0;
	bool individuals_died = false;
	bool pedigrees_enabled = species_.PedigreesEnabled();
	bool no_callbacks = (p_survival_callbacks.size() == 0);
	
	// We keep a global static buffer that records survival decisions
	static uint8_t *survival_buffer = nullptr;
	static int survival_buf_capacity = 0;
	
	if (parent_subpop_size_ > survival_buf_capacity)
	{
		survival_buf_capacity = parent_subpop_size_ * 2;		// double capacity to avoid excessive reallocation
		if (survival_buffer)
			free(survival_buffer);
		survival_buffer = (uint8_t *)malloc(survival_buf_capacity * sizeof(uint8_t));
	}
	
	// clear lifetime reproductive outputs, in preparation for new values
	if (pedigrees_enabled)
	{
		lifetime_reproductive_output_MH_.resize(0);
		lifetime_reproductive_output_F_.resize(0);
	}
	
	// pre-plan mortality; this avoids issues with callbacks accessing the subpop state while buffers are being modified
	if (no_callbacks)
	{
		// this is the simple case with no callbacks and thus no shuffle buffer
		EIDOS_BENCHMARK_START(EidosBenchmarkType::k_SURVIVAL);
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_SURVIVAL);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, survival_buffer, parent_subpop_size_) firstprivate(individual_data) if(parent_subpop_size_ >= EIDOS_OMPMIN_SURVIVAL) num_threads(thread_count)
		{
			uint8_t *survival_buf_perthread = survival_buffer;
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			
#pragma omp for schedule(dynamic, 1024) nowait
			for (int individual_index = 0; individual_index < parent_subpop_size_; ++individual_index)
			{
				Individual *individual = individual_data[individual_index];
				double fitness = individual->cached_fitness_UNSAFE_;	// never overridden in nonWF models, so this is safe with no check
				uint8_t survived;
				
				if (fitness <= 0.0)			survived = false;
				else if (fitness >= 1.0)	survived = true;
				else						survived = (Eidos_rng_uniform(rng) < fitness);
				
				survival_buf_perthread[individual_index] = survived;
			}
		}
		EIDOS_BENCHMARK_END(EidosBenchmarkType::k_SURVIVAL);
	}
	else
	{
		// this is the complex case with callbacks, and therefore a shuffle buffer to randomize processing order
		slim_popsize_t *shuffle_buf = species_.BorrowShuffleBuffer(parent_subpop_size_);
		gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
		
		for (slim_popsize_t shuffle_index = 0; shuffle_index < parent_subpop_size_; shuffle_index++)
		{
			slim_popsize_t individual_index = shuffle_buf[shuffle_index];
			Individual *individual = individual_data[individual_index];
			double fitness = individual->cached_fitness_UNSAFE_;	// never overridden in nonWF models, so this is safe with no check
			double draw = Eidos_rng_uniform(rng);		// always need a draw to pass to the callback
			uint8_t survived = (draw < fitness);
			
			// run the survival() callbacks to allow the above decision to be modified
			survived = ApplySurvivalCallbacks(p_survival_callbacks, individual, fitness, draw, survived);
			
			survival_buffer[individual_index] = survived;
		}
		
		species_.ReturnShuffleBuffer();
	}
	
	// execute the mortality plan; this never uses the shuffle buffer since we are no longer running callbacks
	for (int individual_index = 0; individual_index < parent_subpop_size_; ++individual_index)
	{
		Individual *individual = individual_data[individual_index];
		uint8_t survived = survival_buffer[individual_index];
		
		if (survived)
		{
			// individuals that survive get copied down to the next available slot
			if (survived_individual_index != individual_index)
			{
				individual_data[survived_individual_index] = individual;
				individual_data[survived_individual_index]->index_ = survived_individual_index;
			}
			
			survived_individual_index++;
		}
		else
		{
			// individuals that do not survive get deallocated, and will be overwritten
			if (pedigrees_enabled)
			{
				if (sex_enabled_)
				{
					if (individual->sex_ == IndividualSex::kFemale)
					{
						females_deceased++;
						lifetime_reproductive_output_F_.emplace_back(individual->reproductive_output_);
					}
					else
					{
						lifetime_reproductive_output_MH_.emplace_back(individual->reproductive_output_);
					}
				}
				else
				{
					lifetime_reproductive_output_MH_.emplace_back(individual->reproductive_output_);
				}
			}
			else
			{
				if (sex_enabled_ && (individual->sex_ == IndividualSex::kFemale))
					females_deceased++;
			}
			
			FreeSubpopIndividual(individual);
			
			individuals_died = true;
		}
	}
	
	// Then fix our bookkeeping for the first male index, subpop size, caches, etc.
	if (individuals_died)
	{
		parent_subpop_size_ = survived_individual_index;
		
		if (sex_enabled_)
			parent_first_male_index_ -= females_deceased;
		
		parent_individuals_.resize(parent_subpop_size_);
		
		cached_parent_individuals_value_.reset();
	}
}

// nonWF only:
void Subpopulation::IncrementIndividualAges(void)
{
	// Loop through our individuals and increment all their ages by one
	std::vector<Individual *> &parents = parent_individuals_;
	size_t parent_count = parents.size();
	
	EIDOS_BENCHMARK_START(EidosBenchmarkType::k_AGE_INCR);
	EIDOS_THREAD_COUNT(gEidos_OMP_threads_AGE_INCR);
#pragma omp parallel for schedule(static) default(none) shared(parent_count) firstprivate(parents) if(parent_count >= EIDOS_OMPMIN_AGE_INCR) num_threads(thread_count)
	for (size_t parent_index = 0; parent_index < parent_count; ++parent_index)
	{
		(parents[parent_index]->age_)++;
	}
	EIDOS_BENCHMARK_END(EidosBenchmarkType::k_AGE_INCR);
}

size_t Subpopulation::MemoryUsageForParentTables(void)
{
	size_t usage = 0;
	
	if (lookup_parent_)
		usage += lookup_parent_->K * (sizeof(size_t) + sizeof(double));
	
	if (lookup_female_parent_)
		usage += lookup_female_parent_->K * (sizeof(size_t) + sizeof(double));
	
	if (lookup_male_parent_)
		usage += lookup_male_parent_->K * (sizeof(size_t) + sizeof(double));
	
	return usage;
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosClass *Subpopulation::Class(void) const
{
	return gSLiM_Subpopulation_Class;
}

void Subpopulation::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassNameForDisplay() << "<p" << subpopulation_id_ << ">";
}

EidosValue_SP Subpopulation::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_id:				// ACCELERATED
		{
			if (!cached_value_subpop_id_)
				cached_value_subpop_id_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(subpopulation_id_));
			return cached_value_subpop_id_;
		}
		case gID_firstMaleIndex:	// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(parent_first_male_index_));
		}
		case gID_haplosomes:
		{
			int haplosome_count_per_individual = species_.HaplosomeCountPerIndividual();
			size_t expected_haplosome_count = parent_individuals_.size() * haplosome_count_per_individual;
			EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class))->reserve(expected_haplosome_count);
			
			for (Individual *ind : parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					vec->push_object_element_no_check_NORR(haplosome);
				}
			}
			
			return EidosValue_SP(vec);
		}
		case gID_haplosomesNonNull:
		{
			int haplosome_count_per_individual = species_.HaplosomeCountPerIndividual();
			size_t expected_haplosome_count = parent_individuals_.size() * haplosome_count_per_individual;
			EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class))->reserve(expected_haplosome_count);
			
			for (Individual *ind : parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
						vec->push_object_element_no_check_NORR(haplosome);
				}
			}
			
			return EidosValue_SP(vec);
		}
		case gID_individuals:
		{
			slim_popsize_t subpop_size = parent_subpop_size_;
			
			// Check for an outdated cache; this should never happen, so we flag it as an error
			if (cached_parent_individuals_value_ && (cached_parent_individuals_value_->Count() != subpop_size))
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): (internal error) cached_parent_individuals_value_ out of date." << EidosTerminate();
			
			// Build and return an EidosValue_Object with the current set of individuals in it
			if (!cached_parent_individuals_value_)
			{
				EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class))->reserve(subpop_size);
				cached_parent_individuals_value_ = EidosValue_SP(vec);
				
				for (slim_popsize_t individual_index = 0; individual_index < subpop_size; individual_index++)
					vec->push_object_element_no_check_NORR(parent_individuals_[individual_index]);
			}
			
			return cached_parent_individuals_value_;
		}
		case gID_immigrantSubpopIDs:
		{
			if (model_type_ == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property immigrantSubpopIDs is not available in nonWF models." << EidosTerminate();
			
			EidosValue_Int *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Int();
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto migrant_pair : migrant_fractions_)
				vec->push_int(migrant_pair.first);
			
			return result_SP;
		}
		case gID_immigrantSubpopFractions:
		{
			if (model_type_ == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property immigrantSubpopFractions is not available in nonWF models." << EidosTerminate();
			
			EidosValue_Float *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Float();
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto migrant_pair : migrant_fractions_)
				vec->push_float(migrant_pair.second);
			
			return result_SP;
		}
		case gID_lifetimeReproductiveOutput:
		{
			if (!species_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property lifetimeReproductiveOutput is not available because pedigree recording has not been enabled." << EidosTerminate();
			
			std::vector<int32_t> &lifetime_rep_M = lifetime_reproductive_output_MH_;
			std::vector<int32_t> &lifetime_rep_F = lifetime_reproductive_output_F_;
			int lifetime_rep_count_M = (int)lifetime_rep_M.size();
			int lifetime_rep_count_F = (int)lifetime_rep_F.size();
			EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(lifetime_rep_count_M + lifetime_rep_count_F);
			
			for (int value_index = 0; value_index < lifetime_rep_count_M; ++value_index)
				int_result->set_int_no_check(lifetime_rep_M[value_index], value_index);
			for (int value_index = 0; value_index < lifetime_rep_count_F; ++value_index)
				int_result->set_int_no_check(lifetime_rep_F[value_index], value_index + lifetime_rep_count_M);
			
			return EidosValue_SP(int_result);
		}
		case gID_lifetimeReproductiveOutputM:
		{
			if (!species_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property lifetimeReproductiveOutputM is not available because pedigree recording has not been enabled." << EidosTerminate();
			if (!species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property lifetimeReproductiveOutputM is not defined since separate sexes are not enabled." << EidosTerminate();
			
			std::vector<int32_t> &lifetime_rep = lifetime_reproductive_output_MH_;
			int lifetime_rep_count = (int)lifetime_rep.size();
			EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(lifetime_rep_count);
			
			for (int value_index = 0; value_index < lifetime_rep_count; ++value_index)
				int_result->set_int_no_check(lifetime_rep[value_index], value_index);
			
			return EidosValue_SP(int_result);
		}
		case gID_lifetimeReproductiveOutputF:
		{
			if (!species_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property lifetimeReproductiveOutputF is not available because pedigree recording has not been enabled." << EidosTerminate();
			if (!species_.SexEnabled())
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property lifetimeReproductiveOutputF is not defined since separate sexes are not enabled." << EidosTerminate();
			
			std::vector<int32_t> &lifetime_rep = lifetime_reproductive_output_F_;
			int lifetime_rep_count = (int)lifetime_rep.size();
			EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(lifetime_rep_count);
			
			for (int value_index = 0; value_index < lifetime_rep_count; ++value_index)
				int_result->set_int_no_check(lifetime_rep[value_index], value_index);
			
			return EidosValue_SP(int_result);
		}
		case gID_name:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(name_));
		}
		case gID_description:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String(description_));
		}
		case gID_selfingRate:
		{
			if (model_type_ == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property selfingRate is not available in nonWF models." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(selfing_fraction_));
		}
		case gID_cloningRate:
		{
			if (model_type_ == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property cloningRate is not available in nonWF models." << EidosTerminate();
			
			if (sex_enabled_)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float{female_clone_fraction_, male_clone_fraction_});
			else
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(female_clone_fraction_));
		}
		case gID_sexRatio:
		{
			if (model_type_ == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property sexRatio is not available in nonWF models." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(parent_sex_ratio_));
		}
		case gID_spatialBounds:
		{
			int dimensionality = species_.SpatialDimensionality();
			
			switch (dimensionality)
			{
				case 0: return gStaticEidosValue_Float_ZeroVec;
				case 1: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float{bounds_x0_, bounds_x1_});
				case 2: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float{bounds_x0_, bounds_y0_, bounds_x1_, bounds_y1_});
				case 3: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float{bounds_x0_, bounds_y0_, bounds_z0_, bounds_x1_, bounds_y1_, bounds_z1_});
				default:	return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
		case gID_spatialMaps:
		{
			EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_SpatialMap_Class))->reserve(spatial_maps_.size());
			
			for (const auto &spatialMapIter : spatial_maps_)
				vec->push_object_element_no_check_RR(spatialMapIter.second);
			
			return EidosValue_SP(vec);
		}
		case gID_species:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(&species_, gSLiM_Species_Class));
		}
		case gID_individualCount:		// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(parent_subpop_size_));
		}
			
			// variables
		case gID_tag:					// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property tag accessed on subpopulation before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(tag_value));
		}
		case gID_fitnessScaling:		// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(subpop_fitness_scaling_));
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue *Subpopulation::GetProperty_Accelerated_id(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Subpopulation *value = (Subpopulation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->subpopulation_id_, value_index);
	}
	
	return int_result;
}

EidosValue *Subpopulation::GetProperty_Accelerated_firstMaleIndex(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Subpopulation *value = (Subpopulation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->parent_first_male_index_, value_index);
	}
	
	return int_result;
}

EidosValue *Subpopulation::GetProperty_Accelerated_individualCount(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Subpopulation *value = (Subpopulation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->parent_subpop_size_, value_index);
	}
	
	return int_result;
}

EidosValue *Subpopulation::GetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Subpopulation *value = (Subpopulation *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty_Accelerated_tag): property tag accessed on subpopulation before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

EidosValue *Subpopulation::GetProperty_Accelerated_fitnessScaling(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Subpopulation *value = (Subpopulation *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->subpop_fitness_scaling_, value_index);
	}
	
	return float_result;
}

void Subpopulation::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	switch (p_property_id)
	{
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			tag_value_ = value;
			return;
		}
		case gID_fitnessScaling:	// ACCELERATED
		{
			subpop_fitness_scaling_ = p_value.FloatAtIndex_NOCAST(0, nullptr);
			
			if ((subpop_fitness_scaling_ < 0.0) || std::isnan(subpop_fitness_scaling_))
				EIDOS_TERMINATION << "ERROR (Subpopulation::SetProperty): property fitnessScaling must be >= 0.0." << EidosTerminate();
			
			return;
		}
		case gID_name:
		{
			SetName(p_value.StringAtIndex_NOCAST(0, nullptr));
			return;
		}
		case gID_description:
		{
			std::string description = p_value.StringAtIndex_NOCAST(0, nullptr);
			
			// there are no restrictions on descriptions at all
			
			description_ = description;
			return;
		}
			
		default:
		{
			return super::SetProperty(p_property_id, p_value);
		}
	}
}

void Subpopulation::SetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	// SLiMCastToUsertagTypeOrRaise() is a no-op at present
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex_NOCAST(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Subpopulation *)(p_values[value_index]))->tag_value_ = source_value;
	}
	else
	{
		const int64_t *source_data = p_source.IntData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Subpopulation *)(p_values[value_index]))->tag_value_ = source_data[value_index];
	}
}

void Subpopulation::SetProperty_Accelerated_fitnessScaling(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex_NOCAST(0, nullptr);
		
		if ((source_value < 0.0) || std::isnan(source_value))
			EIDOS_TERMINATION << "ERROR (Subpopulation::SetProperty_Accelerated_fitnessScaling): property fitnessScaling must be >= 0.0." << EidosTerminate();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Subpopulation *)(p_values[value_index]))->subpop_fitness_scaling_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			double source_value = source_data[value_index];
			
			if ((source_value < 0.0) || std::isnan(source_value))
				EIDOS_TERMINATION << "ERROR (Subpopulation::SetProperty_Accelerated_fitnessScaling): property fitnessScaling must be >= 0.0." << EidosTerminate();
			
			((Subpopulation *)(p_values[value_index]))->subpop_fitness_scaling_ = source_value;
		}
	}
}

EidosValue_SP Subpopulation::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
			// WF only:
		case gID_setMigrationRates:		return ExecuteMethod_setMigrationRates(p_method_id, p_arguments, p_interpreter);
		case gID_setCloningRate:		return ExecuteMethod_setCloningRate(p_method_id, p_arguments, p_interpreter);
		case gID_setSelfingRate:		return ExecuteMethod_setSelfingRate(p_method_id, p_arguments, p_interpreter);
		case gID_setSexRatio:			return ExecuteMethod_setSexRatio(p_method_id, p_arguments, p_interpreter);
		case gID_setSubpopulationSize:	return ExecuteMethod_setSubpopulationSize(p_method_id, p_arguments, p_interpreter);
			
			// nonWF only:
		case gID_addCloned:				return ExecuteMethod_addCloned(p_method_id, p_arguments, p_interpreter);
		case gID_addCrossed:			return ExecuteMethod_addCrossed(p_method_id, p_arguments, p_interpreter);
		case gID_addEmpty:				return ExecuteMethod_addEmpty(p_method_id, p_arguments, p_interpreter);
		case gID_addMultiRecombinant:	return ExecuteMethod_addMultiRecombinant(p_method_id, p_arguments, p_interpreter);
		case gID_addRecombinant:		return ExecuteMethod_addRecombinant(p_method_id, p_arguments, p_interpreter);
		case gID_addSelfed:				return ExecuteMethod_addSelfed(p_method_id, p_arguments, p_interpreter);
		case gID_removeSubpopulation:	return ExecuteMethod_removeSubpopulation(p_method_id, p_arguments, p_interpreter);
		case gID_takeMigrants:			return ExecuteMethod_takeMigrants(p_method_id, p_arguments, p_interpreter);

		case gID_haplosomesForChromosomes:	return ExecuteMethod_haplosomesForChromosomes(p_method_id, p_arguments, p_interpreter);
		case gID_deviatePositions:		return ExecuteMethod_deviatePositions(p_method_id, p_arguments, p_interpreter);
		case gID_deviatePositionsWithMap:	return ExecuteMethod_deviatePositionsWithMap(p_method_id, p_arguments, p_interpreter);
		case gID_pointDeviated:			return ExecuteMethod_pointDeviated(p_method_id, p_arguments, p_interpreter);
		case gID_pointInBounds:			return ExecuteMethod_pointInBounds(p_method_id, p_arguments, p_interpreter);
		case gID_pointReflected:		return ExecuteMethod_pointReflected(p_method_id, p_arguments, p_interpreter);
		case gID_pointStopped:			return ExecuteMethod_pointStopped(p_method_id, p_arguments, p_interpreter);
		case gID_pointPeriodic:			return ExecuteMethod_pointPeriodic(p_method_id, p_arguments, p_interpreter);
		case gID_pointUniform:			return ExecuteMethod_pointUniform(p_method_id, p_arguments, p_interpreter);
		case gID_pointUniformWithMap:	return ExecuteMethod_pointUniformWithMap(p_method_id, p_arguments, p_interpreter);
		case gID_setSpatialBounds:		return ExecuteMethod_setSpatialBounds(p_method_id, p_arguments, p_interpreter);
		case gID_cachedFitness:			return ExecuteMethod_cachedFitness(p_method_id, p_arguments, p_interpreter);
		case gID_sampleIndividuals:		return ExecuteMethod_sampleIndividuals(p_method_id, p_arguments, p_interpreter);
		case gID_subsetIndividuals:		return ExecuteMethod_subsetIndividuals(p_method_id, p_arguments, p_interpreter);
		case gID_defineSpatialMap:		return ExecuteMethod_defineSpatialMap(p_method_id, p_arguments, p_interpreter);
		case gID_addSpatialMap:			return ExecuteMethod_addSpatialMap(p_method_id, p_arguments, p_interpreter);
		case gID_removeSpatialMap:		return ExecuteMethod_removeSpatialMap(p_method_id, p_arguments, p_interpreter);
		case gID_spatialMapColor:		return ExecuteMethod_spatialMapColor(p_method_id, p_arguments, p_interpreter);
		case gID_spatialMapImage:		return ExecuteMethod_spatialMapImage(p_method_id, p_arguments, p_interpreter);
		case gID_spatialMapValue:		return ExecuteMethod_spatialMapValue(p_method_id, p_arguments, p_interpreter);
		case gID_outputMSSample:
		case gID_outputVCFSample:
		case gID_outputSample:			return ExecuteMethod_outputXSample(p_method_id, p_arguments, p_interpreter);
		case gID_configureDisplay:		return ExecuteMethod_configureDisplay(p_method_id, p_arguments, p_interpreter);
			
		default:						return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

// nonWF only:
IndividualSex Subpopulation::_ValidateHaplosomesAndChooseSex(ChromosomeType p_chromosome_type, bool p_haplosome1_null, bool p_haplosome2_null, EidosValue *p_sex_value, bool p_sex_enabled, const char *p_caller_name)
{
	IndividualSex offspring_sex;
	
	// First, figure out what sex the user has requested and bounds-check it
	EidosValueType sex_value_type = p_sex_value->Type();
	
	if (p_sex_enabled)
	{
		if (sex_value_type == EidosValueType::kValueNULL)
		{
			// A sex value of NULL can be interpreted in either of two ways.  If the haplosome
			// configuration implies that the offspring must be a particular sex, then we
			// infer that sex.  If not, then we choose the sex of the individual with equal
			// probability.  We will defer the decision using kUnspecified here.
			offspring_sex = IndividualSex::kUnspecified;
		}
		else if (sex_value_type == EidosValueType::kValueString)
		{
			// if a string is provided, it must be either "M" or "F"
			const std::string &sex_string = p_sex_value->StringData()[0];
			
			if (sex_string == "M")
				offspring_sex = IndividualSex::kMale;
			else if (sex_string == "F")
				offspring_sex = IndividualSex::kFemale;
			else
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): unrecognized value '" << sex_string << "' for parameter sex passed to " << p_caller_name << "." << EidosTerminate();
		}
		else // if (sex_value_type == EidosValueType::kValueFloat)
		{
			double sex_prob = p_sex_value->FloatData()[0];
			
			if ((sex_prob >= 0.0) && (sex_prob <= 1.0))
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				
				offspring_sex = ((Eidos_rng_uniform(rng) < sex_prob) ? IndividualSex::kMale : IndividualSex::kFemale);
			}
			else
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): probability " << sex_prob << " out of range [0.0, 1.0] for parameter sex passed to " << p_caller_name << "." << EidosTerminate();
		}
	}
	else
	{
		if (sex_value_type != EidosValueType::kValueNULL)
			EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): NULL must be passed for parameter sex passed to " << p_caller_name << ", in non-sexual models." << EidosTerminate();
		
		offspring_sex = IndividualSex::kHermaphrodite;
	}
	
	// Second, simplify the code below by checking that all haploid chromosome types have
	// a null second haplosome.  The types listed here allow a non-null second haplosome.
	// This is marked as internal error because the caller should have already checked this.
#if DEBUG
	if (!p_haplosome2_null)
		if ((p_chromosome_type != ChromosomeType::kA_DiploidAutosome) &&
			(p_chromosome_type != ChromosomeType::kX_XSexChromosome) &&
			(p_chromosome_type != ChromosomeType::kZ_ZSexChromosome) &&
			(p_chromosome_type != ChromosomeType::kNullY_YSexChromosomeWithNull))
			EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): (internal error) for chromosome type '" << p_chromosome_type <<"', " << p_caller_name << " requires that the second offspring haplosome is configured to be a null haplosome (since chromosome type '" << p_chromosome_type << "' is haploid)." << EidosTerminate();
#endif
	
	// Third, check the chromosome type and validate.  If a sex was chosen explicitly above,
	// this will raise if the haplosomes supplied are not compatible with that sex.  If a sex
	// was not explicitly chosen (i.e., NULL was passed), and the haplosomes provided imply
	// the sex, it will be chosen here.
	switch (p_chromosome_type)
	{
		// chromosome types where we need to check the second haplosome, because it can be used
		//
		case ChromosomeType::kA_DiploidAutosome:
		{
			// This type allows any null haplosome configuration in any individual, in addRecombinant() and addMultiRecombinant()
			break;
		}
		case ChromosomeType::kX_XSexChromosome:
		{
			// Males are required to be X-, females to be XX
			if (offspring_sex == IndividualSex::kUnspecified)
				offspring_sex = (p_haplosome2_null ? IndividualSex::kMale : IndividualSex::kFemale);
			
			if ((offspring_sex == IndividualSex::kMale) && (p_haplosome1_null || !p_haplosome2_null))
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'X', " << p_caller_name << " requires that for a male offspring the first haplosome is non-null and the second is null (X-)." << EidosTerminate();
			if ((offspring_sex == IndividualSex::kFemale) && (p_haplosome1_null || p_haplosome2_null))
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'X', " << p_caller_name << " requires that for a female offspring both haplosomes are non-null (XX)." << EidosTerminate();
			break;
		}
		case ChromosomeType::kZ_ZSexChromosome:
		{
			// Males are required to be ZZ, females to be -Z
			if (offspring_sex == IndividualSex::kUnspecified)
				offspring_sex = (p_haplosome1_null ? IndividualSex::kFemale : IndividualSex::kMale);
			
			if ((offspring_sex == IndividualSex::kMale) && (p_haplosome1_null || p_haplosome2_null))
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'Z', " << p_caller_name << " requires that for a male offspring both haplosomes are non-null (ZZ)." << EidosTerminate();
			if ((offspring_sex == IndividualSex::kFemale) && (!p_haplosome1_null || p_haplosome2_null))
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'Z', " << p_caller_name << " requires that for a female offspring the first haplosome is null and the second is nonnull (-Z)." << EidosTerminate();
			break;
		}
		case ChromosomeType::kNullY_YSexChromosomeWithNull:
		{
			// Males are required to be -Y, females to be --
			if (offspring_sex == IndividualSex::kUnspecified)
				offspring_sex = (p_haplosome2_null ? IndividualSex::kFemale : IndividualSex::kMale);
			
			if ((offspring_sex == IndividualSex::kMale) && (!p_haplosome1_null || p_haplosome2_null))
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type '-Y', " << p_caller_name << " requires that for a male offspring the first haplosome is null and the second is nonnull (-Y)." << EidosTerminate();
			if ((offspring_sex == IndividualSex::kFemale) && (!p_haplosome1_null || !p_haplosome2_null))
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type '-Y', " << p_caller_name << " requires that for a female offspring both haplosomes are null (--)." << EidosTerminate();
			break;
		}
			
		// chromosomes types that are always haploid, so the second haplosome was already checked above
		//
		case ChromosomeType::kH_HaploidAutosome:
		{
			// This type allows any null haplosome configuration in any individual, in addRecombinant() and addMultiRecombinant()
			break;
		}
		case ChromosomeType::kY_YSexChromosome:
		{
			// Males are required to be Y, females to be -
			if (offspring_sex == IndividualSex::kUnspecified)
				offspring_sex = (p_haplosome1_null ? IndividualSex::kFemale : IndividualSex::kMale);
			
			if ((offspring_sex == IndividualSex::kMale) && p_haplosome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'Y', " << p_caller_name << " requires that for a male offspring the first haplosome is non-null (Y) (and the second haplosome is null, since this is a haploid chromosome type)." << EidosTerminate();
			if ((offspring_sex == IndividualSex::kFemale) && !p_haplosome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'Y', " << p_caller_name << " requires that for a female offspring the first haplosome is null (and the second haplosome is null, since this is a haploid chromosome type)." << EidosTerminate();
			break;
		}
		case ChromosomeType::kW_WSexChromosome:
		{
			// Males are required to be -, females to be W
			if (offspring_sex == IndividualSex::kUnspecified)
				offspring_sex = (p_haplosome1_null ? IndividualSex::kMale : IndividualSex::kFemale);
			
			if ((offspring_sex == IndividualSex::kMale) && !p_haplosome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'W', " << p_caller_name << " requires that for a male offspring the first haplosome is null (and the second haplosome is null, since this is a haploid chromosome type)." << EidosTerminate();
			if ((offspring_sex == IndividualSex::kFemale) && p_haplosome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'W', " << p_caller_name << " requires that for a female offspring the first haplosome is non-null (W) (and the second haplosome is null, since this is a haploid chromosome type)." << EidosTerminate();
			break;
		}
		case ChromosomeType::kHF_HaploidFemaleInherited:
		{
			// Required present in both sexes; not informative about sex
			if (!p_haplosome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'HF', " << p_caller_name << " requires that the first haplosome is non-null in both sexes (and the second haplosome is null, since this is a haploid chromosome type)." << EidosTerminate();
			break;
		}
		case ChromosomeType::kFL_HaploidFemaleLine:
		{
			// Males are required to be -, females to be present (like W)
			if (offspring_sex == IndividualSex::kUnspecified)
				offspring_sex = (p_haplosome1_null ? IndividualSex::kMale : IndividualSex::kFemale);
			
			if ((offspring_sex == IndividualSex::kMale) && !p_haplosome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'FL', " << p_caller_name << " requires that for a male offspring the first haplosome is null (and the second haplosome is null, since this is a haploid chromosome type)." << EidosTerminate();
			if ((offspring_sex == IndividualSex::kFemale) && p_haplosome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'FL', " << p_caller_name << " requires that for a female offspring the first haplosome is non-null (and the second haplosome is null, since this is a haploid chromosome type)." << EidosTerminate();
			break;
		}
		case ChromosomeType::kHM_HaploidMaleInherited:
		{
			// Required present in both sexes; not informative about sex
			if (!p_haplosome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'HM', " << p_caller_name << " requires that the first haplosome is non-null in both sexes (and the second haplosome is null, since this is a haploid chromosome type)." << EidosTerminate();
			break;
		}
		case ChromosomeType::kML_HaploidMaleLine:
		{
			// Males are required to be present, females to be - (like Y)
			if (offspring_sex == IndividualSex::kUnspecified)
				offspring_sex = (p_haplosome1_null ? IndividualSex::kFemale : IndividualSex::kMale);
			
			if ((offspring_sex == IndividualSex::kMale) && p_haplosome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'ML', " << p_caller_name << " requires that for a male offspring the first haplosome is non-null (and the second haplosome is null, since this is a haploid chromosome type)." << EidosTerminate();
			if ((offspring_sex == IndividualSex::kFemale) && !p_haplosome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'ML', " << p_caller_name << " requires that for a female offspring the first haplosome is null (and the second haplosome is null, since this is a haploid chromosome type)." << EidosTerminate();
			break;
		}
		case ChromosomeType::kHNull_HaploidAutosomeWithNull:
		{
			// Required H- in both sexes (not flexible like type H); not informative about sex
			if (p_haplosome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::_ValidateHaplosomesAndChooseSex): for chromosome type 'H-', " << p_caller_name << " requires that the first haplosome is non-null (and the second haplosome is null, since this is a haploid chromosome type).  If you want a haploid chromosome type that allows arbitrary null haplosomes to be present, use type 'H'." << EidosTerminate();
			break;
		}
	}
	
	// If the sex was not determined by the code above, we return IndividualSex::kUnspecified
	// and the caller will decide what to do with that (important for addMultiRecombinant(),
	// where the sex might be determined by any of the chromosomes being handled).
	return offspring_sex;
}

IndividualSex Subpopulation::_SexForSexValue(EidosValue *p_sex_value, bool p_sex_enabled)
{
	EidosValueType sex_value_type = p_sex_value->Type();
	IndividualSex sex;
	
	if (p_sex_enabled)
	{
		if (sex_value_type == EidosValueType::kValueNULL)
		{
			// in sexual simulations, NULL (the default) means pick a sex with equal probability
			Eidos_RNG_State *rng = EIDOS_STATE_RNG(omp_get_thread_num());
			
			sex = (Eidos_RandomBool(rng) ? IndividualSex::kMale : IndividualSex::kFemale);
		}
		else if (sex_value_type == EidosValueType::kValueString)
		{
			// if a string is provided, it must be either "M" or "F"
			const std::string &sex_string = p_sex_value->StringData()[0];
			
			if (sex_string == "M")
				sex = IndividualSex::kMale;
			else if (sex_string == "F")
				sex = IndividualSex::kFemale;
			else
				EIDOS_TERMINATION << "ERROR (Subpopulation::HaplosomeConfigurationForSex): unrecognized value '" << sex_string << "' for parameter sex." << EidosTerminate();
		}
		else // if (sex_value_type == EidosValueType::kValueFloat)
		{
			double sex_prob = p_sex_value->FloatData()[0];
			
			if ((sex_prob >= 0.0) && (sex_prob <= 1.0))
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				
				sex = ((Eidos_rng_uniform(rng) < sex_prob) ? IndividualSex::kMale : IndividualSex::kFemale);
			}
			else
				EIDOS_TERMINATION << "ERROR (Subpopulation::HaplosomeConfigurationForSex): probability " << sex_prob << " out of range [0.0, 1.0] for parameter sex." << EidosTerminate();
		}
	}
	else
	{
		if (sex_value_type != EidosValueType::kValueNULL)
			EIDOS_TERMINATION << "ERROR (Subpopulation::HaplosomeConfigurationForSex): sex must be NULL in non-sexual models." << EidosTerminate();
		
		sex = IndividualSex::kHermaphrodite;
	}
	
	return sex;
}

//	*********************	– (o<Individual>)addCloned(object<Individual>$ parent, [integer$ count = 1], [logical$ defer = F])
//
EidosValue_SP Subpopulation::ExecuteMethod_addCloned(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): addCloned() is not available in WF models." << EidosTerminate();
	
	// TIMING RESTRICTION
	if (community_.CycleStage() != SLiMCycleStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): addCloned() may only be called from a reproduction() callback." << EidosTerminate();
	if (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): addCloned() may not be called from a nested callback." << EidosTerminate();
	
	// Get and check the parent
	EidosValue *parent_value = p_arguments[0].get();
	Individual *parent = (Individual *)parent_value->ObjectData()[0];
	Subpopulation &parent_subpop = *parent->subpopulation_;
	
	// SPECIES CONSISTENCY CHECK
	if (&parent_subpop.species_ != &this->species_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): addCloned() requires that parent belongs to the same species as the target subpopulation." << EidosTerminate();
	
	// Check for some other illegal setups
	if (parent->index_ == -1)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): parent must be visible in a subpopulation (i.e., may not be a new juvenile)." << EidosTerminate();
	
	// Check the count and short-circuit if it is zero
	EidosValue *count_value = p_arguments[1].get();
	int64_t child_count = count_value->IntData()[0];
	
	if ((child_count < 0) || (child_count > SLIM_MAX_SUBPOP_SIZE))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): addCloned() requires an offspring count >= 0 and <= 1000000000." << EidosTerminate();
	
	EidosValue_Object *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class))->reserve(child_count);	// reserve enough space for all results
	
	if (child_count == 0)
		return EidosValue_SP(result);
	
	// Generate the number of children requested
#if DEFER_BROKEN
	// The "defer" flag is simply disregarded at the moment; its design has rotted away,
	// and needs to be remade anew once things have settled down.
	EidosValue *defer_value = p_arguments[2].get();
	bool defer = defer_value->LogicalData()[0];
	
	if (defer)
	{
		std::vector<SLiMEidosBlock*> *parent_mutation_callbacks = &parent_subpop.registered_mutation_callbacks_;
		
		if (!parent_mutation_callbacks->size()) parent_mutation_callbacks = nullptr;
		
		if (parent_mutation_callbacks)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): deferred reproduction cannot be used when mutation() callbacks are enabled." << EidosTerminate();
	}
#endif
	
	for (int64_t child_index = 0; child_index < child_count; ++child_index)
	{
		// Make the new individual as a candidate
		Individual *individual = (this->*(population_.GenerateIndividualCloned_TEMPLATED))(parent);
		
		if (individual)
		{
			nonWF_offspring_individuals_.emplace_back(individual);
			result->push_object_element_NORR(individual);
			
#if defined(SLIMGUI)
			{
				IndividualSex parent_sex = parent->sex_;
				
				// note that parent_sex is also the child sex, since this is cloning
				if ((parent_sex == IndividualSex::kHermaphrodite) || (parent_sex == IndividualSex::kMale))
					gui_offspring_cloned_M_++;
				if ((parent_sex == IndividualSex::kHermaphrodite) || (parent_sex == IndividualSex::kFemale))
					gui_offspring_cloned_F_++;
				
				// this offspring came from a parent in parent_subpop but ended up here, so it is, in effect, a migrant;
				// we tally things, SLiMgui display purposes, as if it were generated in parent_subpop and then moved
				parent_subpop.gui_premigration_size_++;
				if (&parent_subpop != this)
					gui_migrants_[parent_subpop.subpopulation_id_]++;
			}
#endif
		}
	}
	
	return EidosValue_SP(result);
}

//	*********************	– (o<Individual>)addCrossed(object<Individual>$ parent1, object<Individual>$ parent2, [Nfs$ sex = NULL], [integer$ count = 1], [logical$ defer = F])
//
EidosValue_SP Subpopulation::ExecuteMethod_addCrossed(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): addCrossed() is not available in WF models." << EidosTerminate();

	// TIMING RESTRICTION
	if (community_.CycleStage() != SLiMCycleStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): addCrossed() may only be called from a reproduction() callback." << EidosTerminate();
	if (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): addCrossed() may not be called from a nested callback." << EidosTerminate();
	
	// Get and check the first parent (the mother)
	EidosValue *parent1_value = p_arguments[0].get();
	Individual *parent1 = (Individual *)parent1_value->ObjectData()[0];
	IndividualSex parent1_sex = parent1->sex_;
	Subpopulation &parent1_subpop = *parent1->subpopulation_;
	
	if ((parent1_sex != IndividualSex::kFemale) && (parent1_sex != IndividualSex::kHermaphrodite))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): parent1 must be female in sexual models (or hermaphroditic in non-sexual models)." << EidosTerminate();
	
	// Get and check the second parent (the father)
	EidosValue *parent2_value = p_arguments[1].get();
	Individual *parent2 = (Individual *)parent2_value->ObjectData()[0];
	IndividualSex parent2_sex = parent2->sex_;
	Subpopulation &parent2_subpop = *parent2->subpopulation_;
	
	if ((parent2_sex != IndividualSex::kMale) && (parent2_sex != IndividualSex::kHermaphrodite))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): parent2 must be male in sexual models (or hermaphroditic in non-sexual models)." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if ((&parent1_subpop.species_ != &this->species_) || (&parent2_subpop.species_ != &this->species_))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): addCrossed() requires that both parents belong to the same species as the target subpopulation." << EidosTerminate();
	
	// Check for some other illegal setups
	if ((parent1->index_ == -1) || (parent2->index_ == -1))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): parent1 and parent2 must be visible in a subpopulation (i.e., may not be new juveniles)." << EidosTerminate();
	
	if (species_.PreventIncidentalSelfing() && (parent1 == parent2))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): parent1 and parent2 must be different individuals, since preventIncidentalSelfing has been set to T (use addSelfed to generate a non-incidentally selfed offspring)." << EidosTerminate();
	
	// Check the count and short-circuit if it is zero
	EidosValue *count_value = p_arguments[3].get();
	int64_t child_count = count_value->IntData()[0];
	
	if ((child_count < 0) || (child_count > SLIM_MAX_SUBPOP_SIZE))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): addCrossed() requires an offspring count >= 0 and <= 1000000000." << EidosTerminate();
	
	EidosValue_Object *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class))->reserve(child_count);	// reserve enough space for all results
	
	if (child_count == 0)
		return EidosValue_SP(result);
	
	// Generate the number of children requested
#if DEFER_BROKEN
	// The "defer" flag is simply disregarded at the moment; its design has rotted away,
	// and needs to be remade anew once things have settled down.
	EidosValue *defer_value = p_arguments[4].get();
	bool defer = defer_value->LogicalData()[0];
	
	if (defer)
	{
		std::vector<SLiMEidosBlock*> *parent1_recombination_callbacks = &parent1_subpop.registered_recombination_callbacks_;
		std::vector<SLiMEidosBlock*> *parent2_recombination_callbacks = &parent2_subpop.registered_recombination_callbacks_;
		std::vector<SLiMEidosBlock*> *parent1_mutation_callbacks = &parent1_subpop.registered_mutation_callbacks_;
		std::vector<SLiMEidosBlock*> *parent2_mutation_callbacks = &parent2_subpop.registered_mutation_callbacks_;
		
		if (!parent1_recombination_callbacks->size()) parent1_recombination_callbacks = nullptr;
		if (!parent2_recombination_callbacks->size()) parent2_recombination_callbacks = nullptr;
		if (!parent1_mutation_callbacks->size()) parent1_mutation_callbacks = nullptr;
		if (!parent2_mutation_callbacks->size()) parent2_mutation_callbacks = nullptr;
		
		if (parent1_recombination_callbacks || parent2_recombination_callbacks || parent1_mutation_callbacks || parent2_mutation_callbacks)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): deferred reproduction cannot be used when recombination() or mutation() callbacks are enabled." << EidosTerminate();
	}
#endif
	
	EidosValue *sex_value = p_arguments[2].get();
	
	for (int64_t child_index = 0; child_index < child_count; ++child_index)
	{
		// Determine the sex of the offspring based on the sex parameter
		IndividualSex child_sex = _SexForSexValue(sex_value, sex_enabled_);
		
		// Make the new individual; if it doesn't pass modifyChild(), nullptr will be returned
		Individual *individual = (this->*(population_.GenerateIndividualCrossed_TEMPLATED))(parent1, parent2, child_sex);
		
		// If the child was accepted, add it to our staging area and to our result vector
		if (individual)
		{
			nonWF_offspring_individuals_.emplace_back(individual);
			result->push_object_element_NORR(individual);
			
#if defined(SLIMGUI)
			{
				gui_offspring_crossed_++;
				
				// this offspring came from parents in parent1_subpop and parent2_subpop but ended up here, so it is, in effect, a migrant;
				// we tally things, SLiMgui display purposes, as if it were generated in those other subpops and then moved
				parent1_subpop.gui_premigration_size_ += 0.5;
				parent2_subpop.gui_premigration_size_ += 0.5;
				if (&parent1_subpop != this)
					gui_migrants_[parent1_subpop.subpopulation_id_] += 0.5;
				if (&parent2_subpop != this)
					gui_migrants_[parent2_subpop.subpopulation_id_] += 0.5;
			}
#endif
		}
	}
	
	return EidosValue_SP(result);
}

//	*********************	– (o<Individual>)addEmpty([Nfs$ sex = NULL], [Nl$ haplosome1Null = NULL], [Nl$ haplosome2Null = NULL], [integer$ count = 1])
//
EidosValue_SP Subpopulation::ExecuteMethod_addEmpty(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): addEmpty() is not available in WF models." << EidosTerminate();

	// TIMING RESTRICTION
	if (community_.CycleStage() != SLiMCycleStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): addEmpty() may only be called from a reproduction() callback." << EidosTerminate();
	if (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): addEmpty() may not be called from a nested callback." << EidosTerminate();
	
	// Check the count and short-circuit if it is zero
	EidosValue *count_value = p_arguments[3].get();
	int64_t child_count = count_value->IntData()[0];
	
	if ((child_count < 0) || (child_count > SLIM_MAX_SUBPOP_SIZE))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): addEmpty() requires an offspring count >= 0 and <= 1000000000." << EidosTerminate();
	
	EidosValue_Object *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class))->reserve(child_count);	// reserve enough space for all results
	
	if (child_count == 0)
		return EidosValue_SP(result);
	
	// Process other parameters; haplosome1Null and haplosome2Null are now interpreted specifically for chromosome type "A" only,
	// saying whether the corresponding haplosome for a type "A" chromosome should be null (true) or non-null (false /NULL).
	EidosValue *sex_value = p_arguments[0].get();
	EidosValue *haplosome1Null_value = p_arguments[1].get();
	EidosValue *haplosome2Null_value = p_arguments[2].get();
	
	bool haplosome1_null = false, haplosome2_null = false;
	
	if (haplosome1Null_value->Type() != EidosValueType::kValueNULL)
		haplosome1_null = haplosome1Null_value->LogicalAtIndex_NOCAST(0, nullptr);
	if (haplosome2Null_value->Type() != EidosValueType::kValueNULL)
		haplosome2_null = haplosome2Null_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	// Generate the number of children requested
	bool record_in_treeseq = species_.RecordingTreeSequence();
	
	for (int64_t child_index = 0; child_index < child_count; ++child_index)
	{
		// Determine the sex of the offspring based on the sex parameter
		IndividualSex child_sex = _SexForSexValue(sex_value, sex_enabled_);
		
		// Make the new individual; if it doesn't pass modifyChild(), nullptr will be returned
		Individual *individual = GenerateIndividualEmpty(/* index */ -1,
														 /* sex */ child_sex,
														 /* age */ 0,
														 /* fitness */ NAN,
														 /* mean_parent_age */ 0.0F,
														 /* haplosome1_null */ haplosome1_null,
														 /* haplosome2_null */ haplosome2_null,
														 /* run_modify_child */ true,
														 record_in_treeseq);
		
		// If the child was accepted, add it to our staging area and to our result vector
		if (individual)
		{
			nonWF_offspring_individuals_.emplace_back(individual);
			result->push_object_element_NORR(individual);
			
#if defined(SLIMGUI)
			{
				gui_offspring_empty_++;
				gui_premigration_size_++;
			}
#endif
		}
	}
	
	return EidosValue_SP(result);
}

//	*********************	– (object<Individual>)addMultiRecombinant(object<Dictionary>$ pattern, [Nfs$ sex = NULL], [No<Individual>$ parent1 = NULL], [No<Individual>$ parent2 = NULL], [Nl$ randomizeStrands = NULL], [integer$ count = 1], [logical$ defer = F])
EidosValue_SP Subpopulation::ExecuteMethod_addMultiRecombinant(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// This code is strongly patterned on the code for addRecombinant(), and should be maintained in parallel
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() is not available in WF models." << EidosTerminate();

	// TIMING RESTRICTION
	if (community_.CycleStage() != SLiMCycleStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() may only be called from a reproduction() callback." << EidosTerminate();
	if (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() may not be called from a nested callback." << EidosTerminate();
	
	// We could technically make this work in the no-genetics case, if the parameters specify that both
	// child haplosomes are null, but there's really no reason for anybody to use addRecombinant() in that
	// case, and getting all the logic correct below would be error-prone.
	if (!species_.HasGenetics())
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() may not be called for a no-genetics species." << EidosTerminate();
	
	// Get arguments and do trivial processing
	EidosValue *pattern_value = p_arguments[0].get();
	EidosValue *sex_value = p_arguments[1].get();
	EidosValue *parent1_value = p_arguments[2].get();
	EidosValue *parent2_value = p_arguments[3].get();
	EidosValue *randomizeStrands_value = p_arguments[4].get();
	EidosValue *count_value = p_arguments[5].get();
	int64_t child_count = count_value->IntData()[0];
	
#if DEFER_BROKEN
	// The "defer" flag is simply disregarded at the moment; its design has rotted away,
	// and needs to be remade anew once things have settled down.
	EidosValue *defer_value = p_arguments[6].get();
	bool defer = defer_value->LogicalData()[0];
#endif
	
	// Prepare for processing the pattern dictionary.  Iterating through pattern is a bit tricky because it
	// could use either integer or string keys, and we want to share all the code that follows after a value
	// has been looked up.  The solution here comes from https://stackoverflow.com/a/44661987/2752221.
	EidosDictionaryUnretained *pattern = (EidosDictionaryUnretained *)pattern_value->ObjectData()[0];
	bool pattern_keysAreIntegers = pattern->KeysAreIntegers();
	bool pattern_keysAreStrings = pattern->KeysAreStrings();
	int pattern_keyCount = pattern->KeyCount();
	
	// We need to be careful, because a new empty dictionary has no dictionary symbols; it hasn't decided yet
	// whether it uses integer or string keys.  We can't make an iterator for a dictionary in that state.
	bool pattern_empty = (pattern_keyCount == 0);
	const EidosDictionaryHashTable_IntegerKeys *pattern_integerKeys = (!pattern_empty && pattern_keysAreIntegers ? pattern->DictionarySymbols_IntegerKeys() : nullptr);
	const EidosDictionaryHashTable_StringKeys *pattern_stringKeys = (!pattern_empty && pattern_keysAreStrings ? pattern->DictionarySymbols_StringKeys() : nullptr);
	
	typedef decltype(std::begin(std::declval<const EidosDictionaryHashTable_IntegerKeys&>())) pattern_integer_keys_iter_type;
	typedef decltype(std::begin(std::declval<const EidosDictionaryHashTable_StringKeys&>())) pattern_string_keys_iter_type;
	
	// Check the count and short-circuit if it is zero
	if ((child_count < 0) || (child_count > SLIM_MAX_SUBPOP_SIZE))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() requires an offspring count >= 0 and <= 1000000000." << EidosTerminate();
	
	EidosValue_Object *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class))->reserve(child_count);	// reserve enough space for all results
	
	if (child_count == 0)
		return EidosValue_SP(result);
	
	// PASS 1:
	//
	// Check the pattern dictionary and determine the child sex.  This has to be done before the main loop,
	// even though it means iterating through the pattern dictionary twice, because we need to determine the
	// child sex up front, and it could be constrained by any entry in the pattern dictionary (and those
	// entries could even conflict).  It also has the virtue of validating the structure of the pattern
	// dictionary, so the actual reproduction code later can be simpler.  We also determine the values of
	// randomizeStrands and mean_parent_age here, for similar reasons.
	IndividualSex constrained_child_sex = IndividualSex::kUnspecified;
	bool randomizeStrands = false;
	float mean_parent_age = 0.0;
	int mean_parent_age_non_null_count = 0;
	pattern_integer_keys_iter_type pattern_integer_key_iter;
	pattern_string_keys_iter_type pattern_string_key_iter;
	
	if (pattern_integerKeys)		pattern_integer_key_iter = pattern_integerKeys->begin();
	else if (pattern_stringKeys)	pattern_string_key_iter = pattern_stringKeys->begin();
	
	// note that if pattern_keyCount == 0, neither iterator will be set up
	
	for (int pattern_keyIndex = 0; pattern_keyIndex < pattern_keyCount; ++pattern_keyIndex)
	{
		// Each entry in pattern has a key that identifies a chromosome (id or symbol), and a dictionary value.
		Chromosome *inheritance_chromosome;
		EidosValue *valueForKey;
		
		if (pattern_keysAreIntegers) {
			int64_t chromosome_id = pattern_integer_key_iter->first;
			
			inheritance_chromosome = species_.ChromosomeFromID(chromosome_id);
			if (!inheritance_chromosome)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() could not find a chromosome in the focal species with an id of " << chromosome_id << "." << EidosTerminate();
			
			valueForKey = pattern_integer_key_iter->second.get();
			pattern_integer_key_iter = std::next(pattern_integer_key_iter);
		} else {
			const std::string &chromosome_symbol = pattern_string_key_iter->first;
			
			inheritance_chromosome = species_.ChromosomeFromSymbol(chromosome_symbol);
			if (!inheritance_chromosome)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() could not find a chromosome in the focal species with a symbol of " << chromosome_symbol << "." << EidosTerminate();
			
			valueForKey = pattern_string_key_iter->second.get();
			pattern_string_key_iter = std::next(pattern_string_key_iter);
		}
		
		if (!valueForKey || (valueForKey->Count() != 1) || (valueForKey->Type() != EidosValueType::kValueObject))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() requires that the values within the pattern dictionary are singletons of class Dictionary (or a subclass of Dictionary)." << EidosTerminate();
		
		EidosDictionaryUnretained *inheritance = dynamic_cast<EidosDictionaryUnretained *>(valueForKey->ObjectData()[0]);
		if (!inheritance)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() requires that the values within the pattern dictionary are of class Dictionary (or a subclass of Dictionary)." << EidosTerminate();
		
		if (!inheritance->KeysAreStrings())
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() requires that the inheritance dictionaries within the pattern dictionary use strings for keys." << EidosTerminate();
		
		// OK, now we have an inheritance dictionary, and need to decode it; zero entries means all keys are NULL
		EidosValue *strand1_value = nullptr;
		EidosValue *strand2_value = nullptr;
		EidosValue *breaks1_value = nullptr;
		EidosValue *strand3_value = nullptr;
		EidosValue *strand4_value = nullptr;
		EidosValue *breaks2_value = nullptr;
		
		if (inheritance->KeyCount() > 0)
		{
			const EidosDictionaryHashTable_StringKeys *inheritance_stringKeys = inheritance->DictionarySymbols_StringKeys();
			
			for (auto const &inheritance_element : *inheritance_stringKeys)
			{
				const std::string &inheritance_key = inheritance_element.first;
				EidosValue * const inheritance_value = inheritance_element.second.get();
				
				if (inheritance_key == gStr_strand1)		strand1_value = inheritance_value;
				else if (inheritance_key == gStr_strand2)	strand2_value = inheritance_value;
				else if (inheritance_key == gStr_breaks1)	breaks1_value = inheritance_value;
				else if (inheritance_key == gStr_strand3)	strand3_value = inheritance_value;
				else if (inheritance_key == gStr_strand4)	strand4_value = inheritance_value;
				else if (inheritance_key == gStr_breaks2)	breaks2_value = inheritance_value;
				else
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): unrecognized inheritance dictionary key '" << inheritance_key << "'; keys must be one of 'strand1', 'strand2', 'breaks1', 'strand3', 'strand4', or 'breaks2'." << EidosTerminate();
			}
		}
		
		// OK, now we've decoded the inheritance dictionary, and we need to validate it
		
		// Get the haplosomes for the supplied strands, or nullptr for NULL.  Eidos has done no validation!
		Haplosome *strand1 = nullptr;
		Haplosome *strand2 = nullptr;
		Haplosome *strand3 = nullptr;
		Haplosome *strand4 = nullptr;
		
		if (strand1_value)
		{
			if ((strand1_value->Type() == EidosValueType::kValueObject) && (strand1_value->Count() == 1) && (((EidosValue_Object *)strand1_value)->Class() == gSLiM_Haplosome_Class))
				strand1 = (Haplosome *)strand1_value->ObjectData()[0];
			else if (strand1_value->Type() != EidosValueType::kValueNULL)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): inheritance dictionary key 'strand1' must either be omitted (NULL), or its value must be a singleton object of class Haplosome." << EidosTerminate();
		}
		
		if (strand2_value)
		{
			if ((strand2_value->Type() == EidosValueType::kValueObject) && (strand2_value->Count() == 1) && (((EidosValue_Object *)strand2_value)->Class() == gSLiM_Haplosome_Class))
				strand2 = (Haplosome *)strand2_value->ObjectData()[0];
			else if (strand2_value->Type() != EidosValueType::kValueNULL)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): inheritance dictionary key 'strand2' must either be omitted (NULL), or its value must be a singleton object of class Haplosome." << EidosTerminate();
		}
		
		if (strand3_value)
		{
			if ((strand3_value->Type() == EidosValueType::kValueObject) && (strand3_value->Count() == 1) && (((EidosValue_Object *)strand3_value)->Class() == gSLiM_Haplosome_Class))
				strand3 = (Haplosome *)strand3_value->ObjectData()[0];
			else if (strand3_value->Type() != EidosValueType::kValueNULL)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): inheritance dictionary key 'strand3' must either be omitted (NULL), or its value must be a singleton object of class Haplosome." << EidosTerminate();
		}
		
		if (strand4_value)
		{
			if ((strand4_value->Type() == EidosValueType::kValueObject) && (strand4_value->Count() == 1) && (((EidosValue_Object *)strand4_value)->Class() == gSLiM_Haplosome_Class))
				strand4 = (Haplosome *)strand4_value->ObjectData()[0];
			else if (strand4_value->Type() != EidosValueType::kValueNULL)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): inheritance dictionary key 'strand4' must either be omitted (NULL), or its value must be a singleton object of class Haplosome." << EidosTerminate();
		}
		
		// New in SLiM 5, we raise if a null haplosome was passed in; remarkably, this was not checked for
		// previously, and could lead to a crash if the user tried to do it!  It never makes sense to do it,
		// really; if a null haplosome is involved you can't have breakpoints, so you *know* there's a null
		// haplosome since you have to make that breakpoint decision, so just pass NULL.  This prevents the
		// user from accidentally passing in a null haplosome and having addRecombinant() do a weird thing.
		if ((strand1 && strand1->IsNull()) || (strand2 && strand2->IsNull()) || (strand3 && strand3->IsNull()) || (strand4 && strand4->IsNull()))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): a parental strand for addMultiRecombinant() is a null haplosome, which is not allowed; pass NULL instead." << EidosTerminate();
		
		// The parental strands must be visible in the subpopulation
		Individual *strand1_parent = (strand1 ? strand1->individual_ : nullptr);
		Individual *strand2_parent = (strand2 ? strand2->individual_ : nullptr);
		Individual *strand3_parent = (strand3 ? strand3->individual_ : nullptr);
		Individual *strand4_parent = (strand4 ? strand4->individual_ : nullptr);
		
		if ((strand1 && (strand1_parent->index_ == -1)) || (strand2 && (strand2_parent->index_ == -1)) ||
			(strand3 && (strand3_parent->index_ == -1)) || (strand4 && (strand4_parent->index_ == -1)))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): a parental strand is not visible in the subpopulation (i.e., belongs to a new juvenile)." << EidosTerminate();
		
		// SPECIES CONSISTENCY CHECK
		if ((strand1_parent && (&strand1_parent->subpopulation_->species_ != &species_)) ||
			(strand2_parent && (&strand2_parent->subpopulation_->species_ != &species_)) ||
			(strand3_parent && (&strand3_parent->subpopulation_->species_ != &species_)) ||
			(strand4_parent && (&strand4_parent->subpopulation_->species_ != &species_)))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() requires that all source haplosomes belong to the same species as the target subpopulation." << EidosTerminate();
		
		// Check that all haplosomes belong to the chromosome that was specified for this inheritance dictionary
		slim_chromosome_index_t chromosome_index = inheritance_chromosome->Index();
		
		if ((strand1 && (strand1->chromosome_index_ != chromosome_index)) ||
			(strand2 && (strand2->chromosome_index_ != chromosome_index)) ||
			(strand3 && (strand3->chromosome_index_ != chromosome_index)) ||
			(strand4 && (strand4->chromosome_index_ != chromosome_index)))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): all strands (haplosomes) in an inheritance dictionary must belong to the chromosome referred to by that inheritance dictionary (as specified by the pattern dictionary key for which the inheritance dictionary is the value)." << EidosTerminate();
		
		// If the first strand of a pair is NULL, the second must also be NULL.  As above, the user has to know
		// which strands are null and which are not anyway, so this should not be a hardship.
		if (!strand1 && strand2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): if strand1 is NULL, strand2 must also be NULL." << EidosTerminate();
		if (!strand3 && strand4)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): if strand3 is NULL, strand4 must also be NULL." << EidosTerminate();
		
		// Determine whether we are randomizing strands; note this sets randomizeStrands once for each
		// iteration, but always to the same value.  We need to check each inheritance dictionary against
		// the specified value of randomizeStrands to validate it, so we need it in pass 1.
		if (randomizeStrands_value->Type() == EidosValueType::kValueLogical)
		{
			randomizeStrands = randomizeStrands_value->LogicalData()[0];
		}
		else
		{
			// the default, NULL, raises an error unless the choice does not matter
			if ((strand1 && strand2) || (strand3 && strand4))
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() requires that T or F be supplied for randomizeStrands if recombination is involved in offspring generation (as it is here); it needs to know whether to randomize strands or not." << EidosTerminate();
			
			// doesn't matter, leave it as false
		}
		
		// Determine whether or not we're making a null haplosome in each position; this is determined wholly
		// by the input strands.  We will determine whether this is actually legal, according to sex and
		// chromosome type, below.
		bool haplosome1_null = (!strand1 && !strand2);
		bool haplosome2_null = (!strand3 && !strand4);
		
		// Determine whether we're going to make a second haplosome -- if the chromosome type is diploid.
		// The second haplosome might be a null haplosome; the question is whether to make one at all.
		// _ValidateHaplosomesAndChooseSex() does this check, but only in DEBUG; this is our responsibility.
		ChromosomeType inheritance_chromosome_type = inheritance_chromosome->Type();
		bool make_second_haplosome = false;
		
		if ((inheritance_chromosome_type == ChromosomeType::kA_DiploidAutosome) ||
			(inheritance_chromosome_type == ChromosomeType::kX_XSexChromosome) ||
			(inheritance_chromosome_type == ChromosomeType::kZ_ZSexChromosome) ||
			(inheritance_chromosome_type == ChromosomeType::kNullY_YSexChromosomeWithNull))
			make_second_haplosome = true;
		
		if (!haplosome2_null && !make_second_haplosome)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): for chromosome type '" << inheritance_chromosome_type <<"', addMultiRecombinant() requires that the second offspring haplosome is configured to be a null haplosome (since chromosome type '" << inheritance_chromosome_type << "' is intrinsically haploid)." << EidosTerminate();
		
		// If we're generating any null haplosomes, we need to remember that in the Subpopulation state,
		// to turn off optimizations.  If the chromosome is haploid, we chack only haplosome1_null.
		if (haplosome1_null || (haplosome2_null && make_second_haplosome))
			has_null_haplosomes_ = true;
		
		// Check that the breakpoint vectors make sense; breakpoints may not be supplied for a NULL pair or
		// a half-NULL pair, but must be supplied for a non-NULL pair.  BCH 9/20/2021: Added logic here in
		// support of the new semantics that (NULL, NULL, NULL) makes a null haplosome, not an empty haplosome.
		// BCH 1/8/2025: Changed logic here in support of the new semantics that (haplosome, haplosome, NULL)
		// requests that SLiM generate breakpoints for a cross in the usual way, so the user doesn't have to.
		//
		// First we need to check the types of the supplied breakpoint vectors; again, Eidos has done no
		// validation for us!  We do not sort/unique/bounds-check the breakpoint vectors here, since we don't
		// need them until the second pass.
		if (!breaks1_value)
			breaks1_value = gStaticEidosValueNULL.get();
		if (!breaks2_value)
			breaks2_value = gStaticEidosValueNULL.get();
		
		if ((breaks1_value->Type() != EidosValueType::kValueNULL) && (breaks1_value->Type() != EidosValueType::kValueInt))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): inheritance dictionary key 'breaks1' must either be omitted (NULL), or its value must be of type integer." << EidosTerminate();
		
		if ((breaks2_value->Type() != EidosValueType::kValueNULL) && (breaks2_value->Type() != EidosValueType::kValueInt))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): inheritance dictionary key 'breaks2' must either be omitted (NULL), or its value must be of type integer." << EidosTerminate();
		
		int breaks1count = breaks1_value->Count(), breaks2count = breaks2_value->Count();
		
		if (breaks1count != 0)
		{
			if (haplosome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): with a NULL strand1 and strand2, breaks1 must be NULL or empty." << EidosTerminate();
			else if (!strand2)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): non-empty breaks1 supplied with a NULL strand2; recombination between strand1 and strand2 is not possible, so breaks1 must be NULL or empty." << EidosTerminate();
		}
		if (breaks2count != 0)
		{
			if (haplosome2_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): with a NULL strand3 and strand4, breaks2 must be NULL or empty." << EidosTerminate();
			else if (!strand4)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): non-empty breaks2 supplied with a NULL strand4; recombination between strand3 and strand4 is not possible, so breaks2 must be NULL or empty." << EidosTerminate();
		}
		
		// The mean parent age is averaged across the mean parent age for each non-null child haplosome
		if (strand1_parent && strand2_parent)
		{
			mean_parent_age += ((strand1_parent->age_ + (float)strand2_parent->age_) / 2.0F);
			mean_parent_age_non_null_count++;
		}
		else if (strand1_parent)
		{
			mean_parent_age += strand1_parent->age_;
			mean_parent_age_non_null_count++;
		}
		else if (strand2_parent)
		{
			mean_parent_age += strand2_parent->age_;
			mean_parent_age_non_null_count++;
		}
		else
		{
			// this child haplosome is generated from NULL, NULL for parents, so there is no parent
		}
		
		if (strand3_parent && strand4_parent)
		{
			mean_parent_age += ((strand3_parent->age_ + (float)strand4_parent->age_) / 2.0F);
			mean_parent_age_non_null_count++;
		}
		else if (strand3_parent)
		{
			mean_parent_age += strand3_parent->age_;
			mean_parent_age_non_null_count++;
		}
		else if (strand4_parent)
		{
			mean_parent_age += strand4_parent->age_;
			mean_parent_age_non_null_count++;
		}
		else
		{
			// this child haplosome is generated from NULL, NULL for parents, so there is no parent
		}
		
		// Figure out what sex the offspring has to be, based on the chromosome type and haplosomes provided.
		// If sex_value specifies a sex, or a probability of a sex, then the sex will be chosen and checked
		// against the user-supplied data.  If it is NULL, then if the data constrains the choice it will be
		// chosen.  Otherwise, IndividualSex::kUnspecified will be returned, allowing a free choice below.
		// We allow the user to play many games with addRecombinant(), but the sex-linked constraints of the
		// chromosome type must be followed, because it is assumed in many places.
		IndividualSex inheritance_child_sex = _ValidateHaplosomesAndChooseSex(inheritance_chromosome_type, haplosome1_null, haplosome2_null, sex_value, sex_enabled_, "addMultiRecombinant()");
		
		if (constrained_child_sex == IndividualSex::kUnspecified)
		{
			// So far we have seen no constraint; accept the choice _ValidateHaplosomesAndChooseSex() made.
			// Note that this might still be IndividualSex::kUnspecified, until we get to a constraint.
			constrained_child_sex = inheritance_child_sex;
		}
		else if ((inheritance_child_sex != IndividualSex::kUnspecified) && (constrained_child_sex != inheritance_child_sex))
		{
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() was unable to determine the sex of the offspring, because it appears to be constrained to be different sexes by the specifications in different inheritance dictionaries." << EidosTerminate();
		}
	}
	
	// set the mean parent age now that we have validated all of the parental haplosomes
	if (mean_parent_age_non_null_count > 0)
		mean_parent_age = mean_parent_age / mean_parent_age_non_null_count;
	
	// Figure out the parents for purposes of pedigree recording.  If only one parent was supplied, use it
	// for both, just as we do for cloning and selfing; it makes relatedness() work.  Note mean_parent_age
	// comes from the strands.  BCH 9/26/2023: The first parent can now also be used for spatial positioning,
	// even if pedigree tracking is not enabled.  BCH 1/10/2025: We infer selfing/cloning for the parental
	// pattern here, which addRecombinant() does not do, because we need that to infer missing inheritance
	// dictionaries below.
	bool pedigrees_enabled = species_.PedigreesEnabled();
	Individual *pedigree_parent1 = nullptr;
	Individual *pedigree_parent2 = nullptr;
	//bool is_selfing = false;		// the inheritance patterm is the same for selfing as for crossing, so we don't need this
	bool is_cloning = false;
	
	if (parent1_value->Type() != EidosValueType::kValueNULL)
		pedigree_parent1 = (Individual *)parent1_value->ObjectData()[0];
	if (parent2_value->Type() != EidosValueType::kValueNULL)
		pedigree_parent2 = (Individual *)parent2_value->ObjectData()[0];
	
	//if (pedigree_parent1 == pedigree_parent2)
	//	is_selfing = true;
	
	if (pedigree_parent1 && !pedigree_parent2)
	{
		pedigree_parent2 = pedigree_parent1;
		is_cloning = true;
	}
	if (pedigree_parent2 && !pedigree_parent1)
	{
		pedigree_parent1 = pedigree_parent2;
		is_cloning = true;
	}
	
	if (pedigree_parent1)
	{
		// if we have pedigree parents, we need to sanity-check them
		if ((&pedigree_parent1->subpopulation_->species_ != &species_) || (&pedigree_parent2->subpopulation_->species_ != &species_))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): addMultiRecombinant() requires that both parents belong to the same species as the target subpopulation." << EidosTerminate();
		
		if ((pedigree_parent1->index_ == -1) || (pedigree_parent2->index_ == -1))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): parent1 and parent2 must be visible in a subpopulation (i.e., may not be new juveniles)." << EidosTerminate();
	}
	
	// Generate the number of children requested, using mutation() callbacks from the target subpopulation (this)
	Eidos_RNG_State *rng_state = EIDOS_STATE_RNG(omp_get_thread_num());
	std::vector<SLiMEidosBlock*> *mutation_callbacks = &registered_mutation_callbacks_;
	
	if (!mutation_callbacks->size())
		mutation_callbacks = nullptr;
	
#if DEFER_BROKEN
	if (defer && mutation_callbacks)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): deferred reproduction cannot be used when mutation() callbacks are enabled." << EidosTerminate();
#endif
	
	for (int64_t child_index = 0; child_index < child_count; ++child_index)
	{
		// If the child's sex is unconstrained, each child generated draws its sex independently
		IndividualSex child_sex = constrained_child_sex;
		
		if (child_sex == IndividualSex::kUnspecified)
			child_sex = (Eidos_RandomBool(rng_state) ? IndividualSex::kMale : IndividualSex::kFemale);
		
		// Make the new individual as a candidate; we make its haplosomes in the pass 2 loop below
		Individual *individual = NewSubpopIndividual(/* index */ -1, child_sex, /* age */ 0, /* fitness */ NAN, mean_parent_age);
		slim_pedigreeid_t pid = 0;
		
		if (pedigrees_enabled)
		{
			pid = SLiM_GetNextPedigreeID();
			
			if (pedigree_parent1 == nullptr)
				individual->TrackParentage_Parentless(pid);
			else if (pedigree_parent1 == pedigree_parent2)
				individual->TrackParentage_Uniparental(pid, *pedigree_parent1);
			else
				individual->TrackParentage_Biparental(pid, *pedigree_parent1, *pedigree_parent2);
		}
		
		// TREE SEQUENCE RECORDING
		if (species_.RecordingTreeSequence())
			species_.SetCurrentNewIndividual(individual);
		
		// BCH 9/26/2023: inherit the spatial position of pedigree_parent1 by default, to set up for deviatePositions()/pointDeviated()
		// Note that, unlike other addX() methods, the first parent is not necessarily defined; in that case, the
		// spatial position of the offspring is left uninitialized.
		if (pedigree_parent1)
			individual->InheritSpatialPosition(species_.SpatialDimensionality(), pedigree_parent1);
		
		// PASS 2:
		//
		// Loop through all chromosomes; if an inheritance dictionary specifies what to do, follow those
		// instructions, otherwise produce the default behavior based on the chromosome type assuming
		// the given parents
		const std::vector<Chromosome *> &chromosomes = species_.Chromosomes();
		int currentHaplosomeIndex = 0;
		
		for (Chromosome *chromosome : chromosomes)
		{
			ChromosomeType chromosome_type = chromosome->Type();
			EidosDictionaryUnretained *inheritance = nullptr;
			
			if (pattern_integerKeys)
			{
				auto found_iter = pattern_integerKeys->find(chromosome->ID());
				
				if (found_iter != pattern_integerKeys->end())
					inheritance = (EidosDictionaryUnretained *)(found_iter->second.get()->ObjectData()[0]);
			}
			else if (pattern_stringKeys)
			{
				auto found_iter = pattern_stringKeys->find(chromosome->Symbol());
				
				if (found_iter != pattern_stringKeys->end())
					inheritance = (EidosDictionaryUnretained *)(found_iter->second.get()->ObjectData()[0]);
			}
			
			// note that if pattern_keyCount == 0, neither hash will be set up; inheritance will remain nullptr
			
			if (!inheritance && !pedigree_parent1)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant):  addMultiRecombinant() has insufficient information to handle a chromosome (id " << chromosome->ID() << ", symbol '" << chromosome->Symbol() << "'); no inheritance dictionary was specified for this chromosome, and inference of a default behavior is not possible since parent1 and parent2 are NULL." << EidosTerminate();
			
			// this is the information we will need, from the inheritance dictionary or synthesized
			Haplosome *strand1 = nullptr;
			Haplosome *strand2 = nullptr;
			Haplosome *strand3 = nullptr;
			Haplosome *strand4 = nullptr;
			std::vector<slim_position_t> breakvec1, breakvec2;
			bool generate_breakvec1 = false;
			bool generate_breakvec2 = false;
			
			if (inheritance)
			{
				// process the inheritance dictionary; note that validation was done in pass 1
				EidosValue *breaks1_value = nullptr;
				EidosValue *breaks2_value = nullptr;
				
				{
					EidosValue *strand1_value = nullptr;
					EidosValue *strand2_value = nullptr;
					EidosValue *strand3_value = nullptr;
					EidosValue *strand4_value = nullptr;
					
					if (inheritance->KeyCount() > 0)
					{
						const EidosDictionaryHashTable_StringKeys *inheritance_stringKeys = inheritance->DictionarySymbols_StringKeys();
						
						for (auto const &inheritance_element : *inheritance_stringKeys)
						{
							const std::string &inheritance_key = inheritance_element.first;
							EidosValue * const inheritance_value = inheritance_element.second.get();
							
							if (inheritance_key == gStr_strand1)		strand1_value = inheritance_value;
							else if (inheritance_key == gStr_strand2)	strand2_value = inheritance_value;
							else if (inheritance_key == gStr_breaks1)	breaks1_value = inheritance_value;
							else if (inheritance_key == gStr_strand3)	strand3_value = inheritance_value;
							else if (inheritance_key == gStr_strand4)	strand4_value = inheritance_value;
							else if (inheritance_key == gStr_breaks2)	breaks2_value = inheritance_value;
						}
					}
					
					// Get the haplosomes for the supplied strands, or nullptr for NULL
					if (strand1_value && (strand1_value->Type() == EidosValueType::kValueObject))
						strand1 = (Haplosome *)strand1_value->ObjectData()[0];
					if (strand2_value && (strand2_value->Type() == EidosValueType::kValueObject))
						strand2 = (Haplosome *)strand2_value->ObjectData()[0];
					if (strand3_value && (strand3_value->Type() == EidosValueType::kValueObject))
						strand3 = (Haplosome *)strand3_value->ObjectData()[0];
					if (strand4_value && (strand4_value->Type() == EidosValueType::kValueObject))
						strand4 = (Haplosome *)strand4_value->ObjectData()[0];
				}
				
				// Copy the breakpoints into local buffers, since we will sort and modify them below.
				// If NULL is supplied for breaks, that is a request for the recombination breakpoints
				// to be generated automatically for a cross.  Note that the breaks count could also be
				// zero because integer(0) was passed; that just means "no breakpoints for this cross".
				if (!breaks1_value)
					breaks1_value = gStaticEidosValueNULL.get();
				if (!breaks2_value)
					breaks2_value = gStaticEidosValueNULL.get();
				
				int breaks1count = breaks1_value->Count(), breaks2count = breaks2_value->Count();
				
				if (breaks1count)
				{
					const int64_t *breaks1_data = breaks1_value->IntData();
					
					for (int break_index = 0; break_index < breaks1count; ++break_index)
						breakvec1.emplace_back(SLiMCastToPositionTypeOrRaise(breaks1_data[break_index]));
				}
				else if ((breaks1_value->Type() == EidosValueType::kValueNULL) && strand1 && strand2)
				{
					generate_breakvec1 = true;
				}
				
				if (breaks2count)
				{
					const int64_t *breaks2_data = breaks2_value->IntData();
					
					for (int break_index = 0; break_index < breaks2count; ++break_index)
						breakvec2.emplace_back(SLiMCastToPositionTypeOrRaise(breaks2_data[break_index]));
				}
				else if ((breaks2_value->Type() == EidosValueType::kValueNULL) && strand3 && strand4)
				{
					generate_breakvec2 = true;
				}
				
				// Randomly swap initial copy strands, if requested and applicable; this should not alter
				// any of the decisions we made earlier about null vs. non-null, child sex, etc.
				if (randomizeStrands)
				{
					if (strand1 && strand2 && Eidos_RandomBool(rng_state))
						std::swap(strand1, strand2);
					if (strand3 && strand4 && Eidos_RandomBool(rng_state))
						std::swap(strand3, strand4);
				}
			}
			else if (is_cloning)
			{
				// Infer the inheritance dictionary info as addPatternForClone() would (same code path).
				species_.InferInheritanceForClone(chromosome, pedigree_parent1, child_sex, &strand1, &strand3, "addMultiRecombinant()");
				
				// no breakpoints are involved; strand2 and strand4 are not involved
			}
			else // crossed and selfed cases
			{
				// Infer the inheritance dictionary info as addPatternForCross() would (same code path).
				// Note that this randomizes the strand order for us, so we can ignore randomizeStrands.
				species_.InferInheritanceForCross(chromosome, pedigree_parent1, pedigree_parent2, child_sex, &strand1, &strand2, &strand3, &strand4, "addMultiRecombinant()");
				
				// when both parental strands are present, we want to generate breaks
				if (strand1 && strand2)
					generate_breakvec1 = true;
				if (strand3 && strand4)
					generate_breakvec2 = true;
			}
			
			// Generate breakpoints.  If the user passed NULL for breaks1, but strand1 and strand2 are
			// both non-NULL, it used to be an error but in SLiM 5 it requests that SLiM generate the
			// breakpoints for the cross automatically, avoiding the need to call drawBreakpoints().
			// This also allows addRecombinant() and addMultiRecombinant() to do gene conversion with
			// heteroduplex mismatch repair and gBGC, which was not previously possible.
			std::vector<slim_position_t> heteroduplex1, heteroduplex2;
			
			if (generate_breakvec1)
			{
				chromosome->DrawBreakpoints(pedigree_parent1, strand1, strand2, /* p_num_breakpoints */ -1, breakvec1, &heteroduplex1, "addMultiRecombinant()");
			}
			else
			{
				std::sort(breakvec1.begin(), breakvec1.end());
				breakvec1.erase(unique(breakvec1.begin(), breakvec1.end()), breakvec1.end());
			}
			
			if (generate_breakvec2)
			{
				chromosome->DrawBreakpoints(pedigree_parent2, strand3, strand4, /* p_num_breakpoints */ -1, breakvec2, &heteroduplex2, "addMultiRecombinant()");
			}
			else
			{
				std::sort(breakvec2.begin(), breakvec2.end());
				breakvec2.erase(unique(breakvec2.begin(), breakvec2.end()), breakvec2.end());
			}
			
			if (breakvec1.size() && (breakvec1.back() > chromosome->last_position_))
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): breaks1 contained a value (" << breakvec1.back() << ") that lies beyond the end of the chromosome." << EidosTerminate();
			if (breakvec2.size() && (breakvec2.back() > chromosome->last_position_))
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addMultiRecombinant): breaks2 contained a value (" << breakvec2.back() << ") that lies beyond the end of the chromosome." << EidosTerminate();
			
			// We used to need to look for a leading 0 in the breaks vectors, and swap the corresponding strands,
			// but HaplosomeRecombined() now handles that for us since it is shared functionality.
			
			//
			// Now, one way or another, we have all the information we need to generate the offspring
			// haplosomes for the current chromosome.  Note that for the selfed/crossed case this still
			// goes through HaplosomeRecombined(), not HaplosomeCrossed().  This is actually good; we
			// get consistent behavior from all paths through this method, whether the inheritance
			// dictionary is supplied or inferred.  Inference of an inheritance dictionary should give
			// exactly the same result as setting up the dictionary with addPatternForCross() etc.
			//
			
			// Get parents of the strands
			Individual *strand1_parent = (strand1 ? strand1->individual_ : nullptr);
			Individual *strand2_parent = (strand2 ? strand2->individual_ : nullptr);
			Individual *strand3_parent = (strand3 ? strand3->individual_ : nullptr);
			Individual *strand4_parent = (strand4 ? strand4->individual_ : nullptr);
			
			// Determine whether or not we're making a null haplosome in each position
			bool haplosome1_null = (!strand1 && !strand2);
			bool haplosome2_null = (!strand3 && !strand4);
			
			// Determine whether we're going to make a second haplosome -- if the chromosome type is diploid.
			// We do this also in pass 1, but we need to do it again because we need make_second_haplosome.
			bool make_second_haplosome = false;
			
			if ((chromosome_type == ChromosomeType::kA_DiploidAutosome) ||
				(chromosome_type == ChromosomeType::kX_XSexChromosome) ||
				(chromosome_type == ChromosomeType::kZ_ZSexChromosome) ||
				(chromosome_type == ChromosomeType::kNullY_YSexChromosomeWithNull))
				make_second_haplosome = true;
			
			// Make the new haplosomes for this chromosome
			Haplosome *haplosome1 = haplosome1_null ? chromosome->NewHaplosome_NULL(individual, 0) : chromosome->NewHaplosome_NONNULL(individual, 0);
			Haplosome *haplosome2 = nullptr;
			
			if (make_second_haplosome)
				haplosome2 = haplosome2_null ? chromosome->NewHaplosome_NULL(individual, 1) : chromosome->NewHaplosome_NONNULL(individual, 1);
			
			if (pedigrees_enabled)
			{
				haplosome1->haplosome_id_ = pid * 2;
				if (make_second_haplosome)
					haplosome2->haplosome_id_ = pid * 2 + 1;
			}
			
			// This has to happen after SetCurrentNewIndividual(), since it patches the node metadata
			individual->AddHaplosomeAtIndex(haplosome1, currentHaplosomeIndex);
			if (make_second_haplosome)
				individual->AddHaplosomeAtIndex(haplosome2, currentHaplosomeIndex + 1);
			
			chromosome->StartMutationRunExperimentClock();
			
			// Construct the first child haplosome, depending upon whether recombination is requested, etc.
			if (strand1)
			{
				if (strand2 && breakvec1.size())
				{
					if (sex_enabled_ && !chromosome->UsingSingleMutationMap() && (strand1_parent->sex_ != strand2_parent->sex_))
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand1 and strand2 come from individuals of different sex, and sex-specific mutation rate maps are in use, so it is not clear which mutation rate map to use." << EidosTerminate();
					
#if DEFER_BROKEN
					if (defer)
					{
						population_.deferred_reproduction_recombinant_.emplace_back(SLiM_DeferredReproductionType::kRecombinant, this, strand1, strand2, breakvec1, haplosome1);
					}
					else
#endif
					{
						(population_.*(population_.HaplosomeRecombined_TEMPLATED))(*chromosome, *haplosome1, strand1, strand2, breakvec1, mutation_callbacks);
						
						if (heteroduplex1.size() > 0)
							population_.DoHeteroduplexRepair(heteroduplex1, breakvec1.data(), (int)breakvec1.size(), strand1, strand2, haplosome1);
					}
				}
				else
				{
#if DEFER_BROKEN
					if (defer)
					{
						// clone one haplosome, using a second strand of nullptr
						population_.deferred_reproduction_recombinant_.emplace_back(SLiM_DeferredReproductionType::kRecombinant, this, strand1, nullptr, breakvec1, haplosome1);
					}
					else
#endif
					{
						(population_.*(population_.HaplosomeCloned_TEMPLATED))(*chromosome, *haplosome1, strand1, mutation_callbacks);
					}
				}
			}
			else
			{
				// both strands are NULL, so we make a null haplosome; we do nothing but record it
				if (species_.RecordingTreeSequence())
					species_.RecordNewHaplosome_NULL(haplosome1);
				
#if DEBUG
				if (!haplosome1_null)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): (internal error) haplosome1_null is false with NULL parental strands!" << EidosTerminate();
#endif
			}
			
			// Construct the second child haplosome, depending upon whether recombination is requested, etc.
			if (strand3)
			{
				if (strand4 && breakvec2.size())
				{
					if (sex_enabled_ && !chromosome->UsingSingleMutationMap() && (strand3_parent->sex_ != strand4_parent->sex_))
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand3 and strand4 come from individuals of different sex, and sex-specific mutation rate maps are in use, so it is not clear which mutation rate map to use." << EidosTerminate();
					
#if DEFER_BROKEN
					if (defer)
					{
						population_.deferred_reproduction_recombinant_.emplace_back(SLiM_DeferredReproductionType::kRecombinant, this, strand3, strand4, breakvec2, haplosome2);
					}
					else
#endif
					{
						(population_.*(population_.HaplosomeRecombined_TEMPLATED))(*chromosome, *haplosome2, strand3, strand4, breakvec2, mutation_callbacks);
						
						if (heteroduplex2.size() > 0)
							population_.DoHeteroduplexRepair(heteroduplex2, breakvec2.data(), (int)breakvec2.size(), strand3, strand4, haplosome2);
					}
				}
				else
				{
#if DEFER_BROKEN
					if (defer)
					{
						// clone one haplosome, using a second strand of nullptr; note that in this case we pass the child sex, not the parent sex
						population_.deferred_reproduction_recombinant_.emplace_back(SLiM_DeferredReproductionType::kRecombinant, this, strand3, nullptr, breakvec2, haplosome2);
					}
					else
#endif
					{
						(population_.*(population_.HaplosomeCloned_TEMPLATED))(*chromosome, *haplosome2, strand3, mutation_callbacks);
					}
				}
			}
			else if (make_second_haplosome)
			{
				// both strands are NULL and this is a diploid chromosome, so we record a null haplosome
				if (species_.RecordingTreeSequence())
					species_.RecordNewHaplosome_NULL(haplosome2);
				
#if DEBUG
				if (!haplosome2_null)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): (internal error) haplosome2_null is false with NULL parental strands!" << EidosTerminate();
#endif
			}
			
			chromosome->StopMutationRunExperimentClock("addMultiRecombinant()");
			currentHaplosomeIndex += (make_second_haplosome ? 2 : 1);
		}
		
		// Run the candidate past modifyChild() callbacks; the target subpop's registered callbacks are used
		if (registered_modify_child_callbacks_.size())
		{
			// BCH 4/5/2022: When removing excess pseudo-parameters from callbacks, we lost a bit of
			// functionality here: we used to pass the four recombinant strands to the callback as the
			// four "parental haplosomes".  But that was always kind of fictional, and it was never
			// documented, and I doubt anybody was using it, and they can do the same without the
			// modifyChild() callback, so I'm not viewing this loss of functionality as an obstacle
			// to making this change.
			// BCH 1/10/2025: Note that we pass nullptr for the parents, and false for p_is_selfing and
			// p_is_cloning.  We could use pedigree_parent1 and pedigree_parent2, and draw inferences from
			// them about selfing/cloning, but that would not always be correct; it would just be a guess.
			bool proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, /* p_parent1 */ nullptr, /* p_parent2 */ nullptr, /* p_is_selfing */ false, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, registered_modify_child_callbacks_);
			
			if (!proposed_child_accepted)
			{
				// If the child was rejected, un-record it and dispose of it
				if (pedigrees_enabled)
				{
					if (pedigree_parent1 == nullptr)
						individual->RevokeParentage_Parentless();
					else if (pedigree_parent1 == pedigree_parent2)
						individual->RevokeParentage_Uniparental(*pedigree_parent1);
					else
						individual->RevokeParentage_Biparental(*pedigree_parent1, *pedigree_parent2);
				}
				
				FreeSubpopIndividual(individual);
				individual = nullptr;
				
				// TREE SEQUENCE RECORDING
				if (species_.RecordingTreeSequence())
					species_.RetractNewIndividual();
			}
		}
		
		// If the child was accepted, add it to our staging area and to our result vector
		if (individual)
		{
			nonWF_offspring_individuals_.emplace_back(individual);
			result->push_object_element_NORR(individual);
			
#if defined(SLIMGUI)
			{
				gui_offspring_crossed_++;
				
				// This offspring came from parents in various subpops but ended up here, so it is, in effect,
				// a migrant; we tally things, for SLiMgui display purposes, as if it were generated in the
				// parental subpops and then moved.  This is gross, but runs only in SLiMgui, so whatever :->
				// We do not set its migrant_ flag, though; that flag is only for takeMigrants() in nonWF.
				// Note that we use pedigree_parent1 and pedigree_parent2 for this, rather than the parents
				// of the strands; in the addMultiRecombinant() case there are potentially many strands with
				// many different parents.  It is just too complex to try to keep track of just for SLiMgui.
				if (pedigree_parent1)
				{
					pedigree_parent1->subpopulation_->gui_premigration_size_ += 0.5;
					if (pedigree_parent1->subpopulation_ != this)
						gui_migrants_[pedigree_parent1->subpopulation_->subpopulation_id_] += 0.5;
				}
				if (pedigree_parent2)
				{
					pedigree_parent2->subpopulation_->gui_premigration_size_ += 0.5;
					if (pedigree_parent2->subpopulation_ != this)
						gui_migrants_[pedigree_parent2->subpopulation_->subpopulation_id_] += 0.5;
				}
			}
#endif
		}
	}
	
	return EidosValue_SP(result);
}

//	*********************	– (o<Individual>)addRecombinant(No<Haplosome>$ strand1, No<Haplosome>$ strand2, Ni breaks1,
//															No<Haplosome>$ strand3, No<Haplosome>$ strand4, Ni breaks2,
//															[Nfs$ sex = NULL], [No<Individual>$ parent1 = NULL], [No<Individual>$ parent2 = NULL],
//															[Nl$ randomizeStrands = NULL], [integer$ count = 1], [logical$ defer = F])
//
EidosValue_SP Subpopulation::ExecuteMethod_addRecombinant(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): addRecombinant() is not available in WF models." << EidosTerminate();

	// TIMING RESTRICTION
	if (community_.CycleStage() != SLiMCycleStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): addRecombinant() may only be called from a reproduction() callback." << EidosTerminate();
	if (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): addRecombinant() may not be called from a nested callback." << EidosTerminate();
	
	// We could technically make this work in the no-genetics case, if the parameters specify that both
	// child haplosomes are null, but there's really no reason for anybody to use addRecombinant() in that
	// case, and getting all the logic correct below would be error-prone.
	if (!species_.HasGenetics())
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): addRecombinant() may not be called for a no-genetics species." << EidosTerminate();
	
	// This method may only be used in single-chromosome models; it is for the simple case, and for backward compatibility.
	if (species_.Chromosomes().size() != 1)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): addRecombinant() may only be called in single-chromosome models; see addMultiRecombinant() for the multi-chromosome version of this method." << EidosTerminate();
	
	Chromosome *chromosome = species_.Chromosomes()[0];
	
	// Get arguments and do trivial processing
	EidosValue *breaks1_value = p_arguments[2].get();
	EidosValue *breaks2_value = p_arguments[5].get();
	EidosValue *sex_value = p_arguments[6].get();
	EidosValue *parent1_value = p_arguments[7].get();
	EidosValue *parent2_value = p_arguments[8].get();
	EidosValue *randomizeStrands_value = p_arguments[9].get();
	EidosValue *count_value = p_arguments[10].get();
	int64_t child_count = count_value->IntData()[0];
	
#if DEFER_BROKEN
	// The "defer" flag is simply disregarded at the moment; its design has rotted away,
	// and needs to be remade anew once things have settled down.
	EidosValue *defer_value = p_arguments[11].get();
	bool defer = defer_value->LogicalData()[0];
#endif
	
	// Check the count and short-circuit if it is zero
	if ((child_count < 0) || (child_count > SLIM_MAX_SUBPOP_SIZE))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): addRecombinant() requires an offspring count >= 0 and <= 1000000000." << EidosTerminate();
	
	EidosValue_Object *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class))->reserve(child_count);	// reserve enough space for all results
	
	if (child_count == 0)
		return EidosValue_SP(result);
	
	// Note that empty Haplosome vectors are not legal values for the strandX parameters; the strands must
	// either be NULL or singleton.  If strand1 and strand2 are both NULL, breaks1 must be NULL/empty, and
	// the offspring haplosome will be empty and will not receive mutations.  If strand1 is non-NULL and
	// strand2 is NULL, breaks1 must be NULL/empty, and the offspring haplosome will be cloned with mutations.
	// If strand1 and strand2 are both non-NULL, breaks1 must be non-NULL but need not be sorted, and
	// recombination with mutations will occur.  If strand1 is NULL and strand2 is non-NULL, that is presently
	// an error, but may be given a meaning later.  The same is true, mutatis mutandis, for strand3, strand4,
	// and breaks2.  The sex parameter is interpreted as in addCrossed().
	// BCH 9/20/2021: For SLiM 3.7, these semantics are changing a little.  Now, when strand1 and strand2 are
	// both NULL and breaks1 is NULL/empty, the offspring haplosome will be a *null* haplosome (not just empty),
	// and as before will not receive mutations.  That is the way it always should have worked.  Again, mutatis
	// mutandis, for strand3, strand4, and breaks2.  See https://github.com/MesserLab/SLiM/issues/205.
	// BCH 1/8/2025: A further change for SLiM 5: if strand1 and strand2 are both non-NULL (doing a cross),
	// breaks1 may now be NULL indicating that addRecombinant() should draw breakpoints in the usual way.
	
	// Get the haplosomes for the supplied strands, or nullptr for NULL
	Haplosome *strand1;
	Haplosome *strand2;
	Haplosome *strand3;
	Haplosome *strand4;
	
	{
		EidosValue *strand1_value = p_arguments[0].get();
		EidosValue *strand2_value = p_arguments[1].get();
		EidosValue *strand3_value = p_arguments[3].get();
		EidosValue *strand4_value = p_arguments[4].get();
		
		strand1 = ((strand1_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Haplosome *)strand1_value->ObjectData()[0]);
		strand2 = ((strand2_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Haplosome *)strand2_value->ObjectData()[0]);
		strand3 = ((strand3_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Haplosome *)strand3_value->ObjectData()[0]);
		strand4 = ((strand4_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Haplosome *)strand4_value->ObjectData()[0]);
	}
	
	// New in SLiM 5, we raise if a null haplosome was passed in; remarkably, this was not checked for
	// previously, and could lead to a crash if the user tried to do it!  It never makes sense to do it,
	// really; if a null haplosome is involved you can't have breakpoints, so you *know* there's a null
	// haplosome since you have to make that breakpoint decision, so just pass NULL.  This prevents the
	// user from accidentally passing in a null haplosome and having addRecombinant() do a weird thing.
	if ((strand1 && strand1->IsNull()) || (strand2 && strand2->IsNull()) || (strand3 && strand3->IsNull()) || (strand4 && strand4->IsNull()))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): a parental strand for addRecombinant() is a null haplosome, which is not allowed; pass NULL instead." << EidosTerminate();
	
	// The parental strands must be visible in the subpopulation
	Individual *strand1_parent = (strand1 ? strand1->individual_ : nullptr);
	Individual *strand2_parent = (strand2 ? strand2->individual_ : nullptr);
	Individual *strand3_parent = (strand3 ? strand3->individual_ : nullptr);
	Individual *strand4_parent = (strand4 ? strand4->individual_ : nullptr);
	
	if ((strand1 && (strand1_parent->index_ == -1)) || (strand2 && (strand2_parent->index_ == -1)) ||
		(strand3 && (strand3_parent->index_ == -1)) || (strand4 && (strand4_parent->index_ == -1)))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): a parental strand is not visible in the subpopulation (i.e., belongs to a new juvenile)." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if ((strand1_parent && (&strand1_parent->subpopulation_->species_ != &species_)) ||
		(strand2_parent && (&strand2_parent->subpopulation_->species_ != &species_)) ||
		(strand3_parent && (&strand3_parent->subpopulation_->species_ != &species_)) ||
		(strand4_parent && (&strand4_parent->subpopulation_->species_ != &species_)))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): addRecombinant() requires that all source haplosomes belong to the same species as the target subpopulation." << EidosTerminate();
	
	// We do not need to check that associated chromosomes for the strands match, because this method can
	// only be called in a single-chromosome model; addMultiRecombinant() will need to check that, though
	
	// If the first strand of a pair is NULL, the second must also be NULL.  As above, the user has to know
	// which strands are null and which are not anyway, so this should not be a hardship.
	if (!strand1 && strand2)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): if strand1 is NULL, strand2 must also be NULL." << EidosTerminate();
	if (!strand3 && strand4)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): if strand3 is NULL, strand4 must also be NULL." << EidosTerminate();
	
	// Determine whether we are randomizing strands; if we are, we will do it for each child generated
	bool randomizeStrands = false;
	
	if (randomizeStrands_value->Type() == EidosValueType::kValueLogical)
	{
		randomizeStrands = randomizeStrands_value->LogicalData()[0];
	}
	else
	{
		// the default, NULL, raises an error unless the choice does not matter
		if ((strand1 && strand2) || (strand3 && strand4))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): addRecombinant() requires that T or F be supplied for randomizeStrands if recombination is involved in offspring generation (as it is here); it needs to know whether to randomize strands or not. (The default used to be F, but that was bug-prone since the correct value in most models is T; so now you need to think and make an explicit choice.)" << EidosTerminate();
		
		// doesn't matter, leave it as false
	}
	
	// Determine whether or not we're making a null haplosome in each position; this is determined wholly
	// by the input strands.  We will determine whether this is actually legal, according to sex and
	// chromosome type, below.
	bool haplosome1_null = (!strand1 && !strand2);
	bool haplosome2_null = (!strand3 && !strand4);
	
	// Determine whether we're going to make a second haplosome -- if the chromosome type is diploid.
	// The second haplosome might be a null haplosome; the question is whether to make one at all.
	// _ValidateHaplosomesAndChooseSex() does this check, but only in DEBUG; this is our responsibility.
	ChromosomeType chromosome_type = chromosome->Type();
	bool make_second_haplosome = false;
	
	if ((chromosome_type == ChromosomeType::kA_DiploidAutosome) ||
		(chromosome_type == ChromosomeType::kX_XSexChromosome) ||
		(chromosome_type == ChromosomeType::kZ_ZSexChromosome) ||
		(chromosome_type == ChromosomeType::kNullY_YSexChromosomeWithNull))
		make_second_haplosome = true;
	
	if (!haplosome2_null && !make_second_haplosome)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): for chromosome type '" << chromosome_type <<"', addRecombinant() requires that the second offspring haplosome is configured to be a null haplosome (since chromosome type '" << chromosome_type << "' is intrinsically haploid)." << EidosTerminate();
	
	// If we're generating any null haplosomes, we need to remember that in the Subpopulation state,
	// to turn off optimizations.  If the chromosome is haploid, we check only haplosome1_null.
	if (haplosome1_null || (haplosome2_null && make_second_haplosome))
		has_null_haplosomes_ = true;
	
	// Check that the breakpoint vectors make sense; breakpoints may not be supplied for a NULL pair or
	// a half-NULL pair, but must be supplied for a non-NULL pair.  BCH 9/20/2021: Added logic here in
	// support of the new semantics that (NULL, NULL, NULL) makes a null haplosome, not an empty haplosome.
	// BCH 1/8/2025: Changed logic here in support of the new semantics that (haplosome, haplosome, NULL)
	// requests that SLiM generate breakpoints for a cross in the usual way, so the user doesn't have to.
	int breaks1count = breaks1_value->Count(), breaks2count = breaks2_value->Count();
	
	if (breaks1count != 0)
	{
		if (haplosome1_null)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): with a NULL strand1 and strand2, breaks1 must be NULL or empty." << EidosTerminate();
		else if (!strand2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): non-empty breaks1 supplied with a NULL strand2; recombination between strand1 and strand2 is not possible, so breaks1 must be NULL or empty." << EidosTerminate();
	}
	if (breaks2count != 0)
	{
		if (haplosome2_null)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): with a NULL strand3 and strand4, breaks2 must be NULL or empty." << EidosTerminate();
		else if (!strand4)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): non-empty breaks2 supplied with a NULL strand4; recombination between strand3 and strand4 is not possible, so breaks2 must be NULL or empty." << EidosTerminate();
	}
	
	// Copy the breakpoints into local buffers, since we will sort and modify them below.
	// If NULL is supplied for breaks, that is a request for the recombination breakpoints
	// to be generated automatically for a cross.  Note that the breaks count could also be
	// zero because integer(0) was passed; that just means "no breakpoints for this cross".
	std::vector<slim_position_t> breakvec1, breakvec2;
	bool generate_breakvec1 = false;
	bool generate_breakvec2 = false;
	
	if (breaks1count)
	{
		const int64_t *breaks1_data = breaks1_value->IntData();
		
		for (int break_index = 0; break_index < breaks1count; ++break_index)
			breakvec1.emplace_back(SLiMCastToPositionTypeOrRaise(breaks1_data[break_index]));
	}
	else if ((breaks1_value->Type() == EidosValueType::kValueNULL) && strand1 && strand2)
	{
		generate_breakvec1 = true;
	}
	
	if (breaks2count)
	{
		const int64_t *breaks2_data = breaks2_value->IntData();
		
		for (int break_index = 0; break_index < breaks2count; ++break_index)
			breakvec2.emplace_back(SLiMCastToPositionTypeOrRaise(breaks2_data[break_index]));
	}
	else if ((breaks2_value->Type() == EidosValueType::kValueNULL) && strand3 && strand4)
	{
		generate_breakvec2 = true;
	}
	
	// The mean parent age is averaged across the mean parent age for each non-null child haplosome
	float mean_parent_age = 0.0;
	int mean_parent_age_non_null_count = 0;
	
	if (strand1_parent && strand2_parent)
	{
		mean_parent_age += ((strand1_parent->age_ + (float)strand2_parent->age_) / 2.0F);
		mean_parent_age_non_null_count++;
	}
	else if (strand1_parent)
	{
		mean_parent_age += strand1_parent->age_;
		mean_parent_age_non_null_count++;
	}
	else if (strand2_parent)
	{
		mean_parent_age += strand2_parent->age_;
		mean_parent_age_non_null_count++;
	}
	else
	{
		// this child haplosome is generated from NULL, NULL for parents, so there is no parent
	}
	
	if (strand3_parent && strand4_parent)
	{
		mean_parent_age += ((strand3_parent->age_ + (float)strand4_parent->age_) / 2.0F);
		mean_parent_age_non_null_count++;
	}
	else if (strand3_parent)
	{
		mean_parent_age += strand3_parent->age_;
		mean_parent_age_non_null_count++;
	}
	else if (strand4_parent)
	{
		mean_parent_age += strand4_parent->age_;
		mean_parent_age_non_null_count++;
	}
	else
	{
		// this child haplosome is generated from NULL, NULL for parents, so there is no parent
	}
	
	if (mean_parent_age_non_null_count > 0)
		mean_parent_age = mean_parent_age / mean_parent_age_non_null_count;
	
	// Figure out the parents for purposes of pedigree recording.  If only one parent was supplied, use it
	// for both, just as we do for cloning and selfing; it makes relatedness() work.  Note mean_parent_age
	// comes from the strands.  BCH 9/26/2023: the first parent can now also be used for spatial positioning,
	// even if pedigree tracking is not enabled.  BCH 1/10/2025: We do not need to infer selfing/cloning
	// here, the way addMultiRecombinant() does, since we don't need to infer inheritance patterns.
	bool pedigrees_enabled = species_.PedigreesEnabled();
	Individual *pedigree_parent1 = nullptr;
	Individual *pedigree_parent2 = nullptr;
	
	if (parent1_value->Type() != EidosValueType::kValueNULL)
		pedigree_parent1 = (Individual *)parent1_value->ObjectData()[0];
	if (parent2_value->Type() != EidosValueType::kValueNULL)
		pedigree_parent2 = (Individual *)parent2_value->ObjectData()[0];
	
	if (pedigree_parent1 && !pedigree_parent2)
		pedigree_parent2 = pedigree_parent1;
	if (pedigree_parent2 && !pedigree_parent1)
		pedigree_parent1 = pedigree_parent2;
	
	if (pedigree_parent1)
	{
		// if we have pedigree parents, we need to sanity-check them
		if ((&pedigree_parent1->subpopulation_->species_ != &species_) || (&pedigree_parent2->subpopulation_->species_ != &species_))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): addRecombinant() requires that both parents belong to the same species as the target subpopulation." << EidosTerminate();
		
		if ((pedigree_parent1->index_ == -1) || (pedigree_parent2->index_ == -1))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): parent1 and parent2 must be visible in a subpopulation (i.e., may not be new juveniles)." << EidosTerminate();
	}
	
	// Figure out what sex the offspring has to be, based on the chromosome type and haplosomes provided.
	// If sex_value specifies a sex, or a probability of a sex, then the sex will be chosen and checked
	// against the user-supplied data.  If it is NULL, then if the data constrains the choice it will be
	// chosen.  Otherwise, IndividualSex::kUnspecified will be returned, allowing a free choice below.
	// We allow the user to play many games with addRecombinant(), but the sex-linked constraints of the
	// chromosome type must be followed, because it is assumed in many places.
	IndividualSex constrained_child_sex = _ValidateHaplosomesAndChooseSex(chromosome_type, haplosome1_null, haplosome2_null, sex_value, sex_enabled_, "addRecombinant()");
	
	// Generate the number of children requested, using mutation() callbacks from the target subpopulation (this)
	Eidos_RNG_State *rng_state = EIDOS_STATE_RNG(omp_get_thread_num());
	std::vector<SLiMEidosBlock*> *mutation_callbacks = &registered_mutation_callbacks_;
	
	if (!mutation_callbacks->size())
		mutation_callbacks = nullptr;
	
#if DEFER_BROKEN
	if (defer && mutation_callbacks)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): deferred reproduction cannot be used when mutation() callbacks are enabled." << EidosTerminate();
#endif
	
	for (int64_t child_index = 0; child_index < child_count; ++child_index)
	{
		// If the child's sex is unconstrained, each child generated draws its sex independently
		IndividualSex child_sex = constrained_child_sex;
		
		if (child_sex == IndividualSex::kUnspecified)
			child_sex = (Eidos_RandomBool(rng_state) ? IndividualSex::kMale : IndividualSex::kFemale);
		
		// Randomly swap initial copy strands, if requested and applicable; this should not alter
		// any of the decisions we made earlier about null vs. non-null, child sex, etc.  Note that
		// with child_count > 1 we'll be swapping back and forth, but that's fine.
		if (randomizeStrands)
		{
			if (strand1 && strand2 && Eidos_RandomBool(rng_state))
			{
				std::swap(strand1, strand2);
				std::swap(strand1_parent, strand2_parent);
			}
			if (strand3 && strand4 && Eidos_RandomBool(rng_state))
			{
				std::swap(strand3, strand4);
				std::swap(strand3_parent, strand4_parent);
			}
		}
		
		// Generate breakpoints.  If the user passed NULL for breaks1, but strand1 and strand2 are
		// both non-NULL, it used to be an error but in SLiM 5 it requests that SLiM generate the
		// breakpoints for the cross automatically, avoiding the need to call drawBreakpoints().
		// This also allows addRecombinant() and addMultiRecombinant() to do gene conversion with
		// heteroduplex mismatch repair and gBGC, which was not previously possible.
		std::vector<slim_position_t> heteroduplex1, heteroduplex2;
		
		if (generate_breakvec1)
		{
			breakvec1.resize(0);		// we might be reusing this vector from a previous child
			
			chromosome->DrawBreakpoints(pedigree_parent1, strand1, strand2, /* p_num_breakpoints */ -1, breakvec1, &heteroduplex1, "addRecombinant()");
		}
		else
		{
			std::sort(breakvec1.begin(), breakvec1.end());
			breakvec1.erase(unique(breakvec1.begin(), breakvec1.end()), breakvec1.end());
		}
		
		if (generate_breakvec2)
		{
			breakvec2.resize(0);		// we might be reusing this vector from a previous child
			
			chromosome->DrawBreakpoints(pedigree_parent2, strand3, strand4, /* p_num_breakpoints */ -1, breakvec2, &heteroduplex2, "addRecombinant()");
		}
		else
		{
			std::sort(breakvec2.begin(), breakvec2.end());
			breakvec2.erase(unique(breakvec2.begin(), breakvec2.end()), breakvec2.end());
		}
		
		if (breakvec1.size() && (breakvec1.back() > chromosome->last_position_))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): breaks1 contained a value (" << breakvec1.back() << ") that lies beyond the end of the chromosome." << EidosTerminate();
		if (breakvec2.size() && (breakvec2.back() > chromosome->last_position_))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): breaks2 contained a value (" << breakvec2.back() << ") that lies beyond the end of the chromosome." << EidosTerminate();
		
		// We used to need to look for a leading 0 in the breaks vectors, and swap the corresponding strands,
		// but HaplosomeRecombined() now handles that for us since it is shared functionality.
		
		// Make the new individual as a candidate
		Individual *individual = NewSubpopIndividual(/* index */ -1, child_sex, /* age */ 0, /* fitness */ NAN, mean_parent_age);
		Haplosome *haplosome1 = haplosome1_null ? chromosome->NewHaplosome_NULL(individual, 0) : chromosome->NewHaplosome_NONNULL(individual, 0);
		Haplosome *haplosome2 = nullptr;
		
		if (make_second_haplosome)
			haplosome2 = haplosome2_null ? chromosome->NewHaplosome_NULL(individual, 1) : chromosome->NewHaplosome_NONNULL(individual, 1);
		
		if (pedigrees_enabled)
		{
			slim_pedigreeid_t pid = SLiM_GetNextPedigreeID();
			
			if (pedigree_parent1 == nullptr)
				individual->TrackParentage_Parentless(pid);
			else if (pedigree_parent1 == pedigree_parent2)
				individual->TrackParentage_Uniparental(pid, *pedigree_parent1);
			else
				individual->TrackParentage_Biparental(pid, *pedigree_parent1, *pedigree_parent2);
			
			haplosome1->haplosome_id_ = pid * 2;
			if (make_second_haplosome)
				haplosome2->haplosome_id_ = pid * 2 + 1;
		}
		
		// TREE SEQUENCE RECORDING
		if (species_.RecordingTreeSequence())
			species_.SetCurrentNewIndividual(individual);
		
		// This has to happen after SetCurrentNewIndividual(), since it patches the node metadata
		individual->AddHaplosomeAtIndex(haplosome1, 0);
		if (make_second_haplosome)
			individual->AddHaplosomeAtIndex(haplosome2, 1);
		
		// BCH 9/26/2023: inherit the spatial position of pedigree_parent1 by default, to set up for deviatePositions()/pointDeviated()
		// Note that, unlike other addX() methods, the first parent is not necessarily defined; in that case, the
		// spatial position of the offspring is left uninitialized.
		if (pedigree_parent1)
			individual->InheritSpatialPosition(species_.SpatialDimensionality(), pedigree_parent1);
		
		chromosome->StartMutationRunExperimentClock();
		
		// Construct the first child haplosome, depending upon whether recombination is requested, etc.
		if (strand1)
		{
			if (strand2 && breakvec1.size())
			{
				if (sex_enabled_ && !chromosome->UsingSingleMutationMap() && (strand1_parent->sex_ != strand2_parent->sex_))
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand1 and strand2 come from individuals of different sex, and sex-specific mutation rate maps are in use, so it is not clear which mutation rate map to use." << EidosTerminate();
				
#if DEFER_BROKEN
				if (defer)
				{
					population_.deferred_reproduction_recombinant_.emplace_back(SLiM_DeferredReproductionType::kRecombinant, this, strand1, strand2, breakvec1, haplosome1);
				}
				else
#endif
				{
					(population_.*(population_.HaplosomeRecombined_TEMPLATED))(*chromosome, *haplosome1, strand1, strand2, breakvec1, mutation_callbacks);
					
					if (heteroduplex1.size() > 0)
						population_.DoHeteroduplexRepair(heteroduplex1, breakvec1.data(), (int)breakvec1.size(), strand1, strand2, haplosome1);
				}
			}
			else
			{
#if DEFER_BROKEN
				if (defer)
				{
					// clone one haplosome, using a second strand of nullptr
					population_.deferred_reproduction_recombinant_.emplace_back(SLiM_DeferredReproductionType::kRecombinant, this, strand1, nullptr, breakvec1, haplosome1);
				}
				else
#endif
				{
					(population_.*(population_.HaplosomeCloned_TEMPLATED))(*chromosome, *haplosome1, strand1, mutation_callbacks);
				}
			}
		}
		else
		{
			// both strands are NULL, so we make a null haplosome; we do nothing but record it
			if (species_.RecordingTreeSequence())
				species_.RecordNewHaplosome_NULL(haplosome1);
			
#if DEBUG
			if (!haplosome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): (internal error) haplosome1_null is false with NULL parental strands!" << EidosTerminate();
#endif
		}
		
		// Construct the second child haplosome, depending upon whether recombination is requested, etc.
		if (strand3)
		{
			if (strand4 && breakvec2.size())
			{
				if (sex_enabled_ && !chromosome->UsingSingleMutationMap() && (strand3_parent->sex_ != strand4_parent->sex_))
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand3 and strand4 come from individuals of different sex, and sex-specific mutation rate maps are in use, so it is not clear which mutation rate map to use." << EidosTerminate();
				
#if DEFER_BROKEN
				if (defer)
				{
					population_.deferred_reproduction_recombinant_.emplace_back(SLiM_DeferredReproductionType::kRecombinant, this, strand3, strand4, breakvec2, haplosome2);
				}
				else
#endif
				{
					(population_.*(population_.HaplosomeRecombined_TEMPLATED))(*chromosome, *haplosome2, strand3, strand4, breakvec2, mutation_callbacks);
					
					if (heteroduplex2.size() > 0)
						population_.DoHeteroduplexRepair(heteroduplex2, breakvec2.data(), (int)breakvec2.size(), strand3, strand4, haplosome2);
				}
			}
			else
			{
#if DEFER_BROKEN
				if (defer)
				{
					// clone one haplosome, using a second strand of nullptr; note that in this case we pass the child sex, not the parent sex
					population_.deferred_reproduction_recombinant_.emplace_back(SLiM_DeferredReproductionType::kRecombinant, this, strand3, nullptr, breakvec2, haplosome2);
				}
				else
#endif
				{
					(population_.*(population_.HaplosomeCloned_TEMPLATED))(*chromosome, *haplosome2, strand3, mutation_callbacks);
				}
			}
		}
		else if (make_second_haplosome)
		{
			// both strands are NULL and this is a diploid chromosome, so we record a null haplosome
			if (species_.RecordingTreeSequence())
				species_.RecordNewHaplosome_NULL(haplosome2);
			
#if DEBUG
			if (!haplosome2_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): (internal error) haplosome2_null is false with NULL parental strands!" << EidosTerminate();
#endif
		}
		
		chromosome->StopMutationRunExperimentClock("addMultiRecombinant()");
		
		// Run the candidate past modifyChild() callbacks; the target subpop's registered callbacks are used
		if (registered_modify_child_callbacks_.size())
		{
			// BCH 4/5/2022: When removing excess pseudo-parameters from callbacks, we lost a bit of
			// functionality here: we used to pass the four recombinant strands to the callback as the
			// four "parental haplosomes".  But that was always kind of fictional, and it was never
			// documented, and I doubt anybody was using it, and they can do the same without the
			// modifyChild() callback, so I'm not viewing this loss of functionality as an obstacle
			// to making this change.
			// BCH 1/10/2025: Note that we pass nullptr for the parents, and false for p_is_selfing and
			// p_is_cloning.  We could use pedigree_parent1 and pedigree_parent2, and draw inferences from
			// them about selfing/cloning, but that would not always be correct; it would just be a guess.
			bool proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, /* p_parent1 */ nullptr, /* p_parent2 */ nullptr, /* p_is_selfing */ false, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, registered_modify_child_callbacks_);
			
			if (!proposed_child_accepted)
			{
				// If the child was rejected, un-record it and dispose of it
				if (pedigrees_enabled)
				{
					if (pedigree_parent1 == nullptr)
						individual->RevokeParentage_Parentless();
					else if (pedigree_parent1 == pedigree_parent2)
						individual->RevokeParentage_Uniparental(*pedigree_parent1);
					else
						individual->RevokeParentage_Biparental(*pedigree_parent1, *pedigree_parent2);
				}
				
				FreeSubpopIndividual(individual);
				individual = nullptr;
				
				// TREE SEQUENCE RECORDING
				if (species_.RecordingTreeSequence())
					species_.RetractNewIndividual();
			}
		}
		
		if (individual)
		{
			nonWF_offspring_individuals_.emplace_back(individual);
			result->push_object_element_NORR(individual);
			
#if defined(SLIMGUI)
			{
				gui_offspring_crossed_++;
				
				// This offspring came from parents in various subpops but ended up here, so it is, in effect,
				// a migrant; we tally things, for SLiMgui display purposes, as if it were generated in the
				// parental subpops and then moved.  This is gross, but runs only in SLiMgui, so whatever :->
				// We do not set its migrant_ flag, though; that flag is only for takeMigrants() in nonWF.
				// Note that we use pedigree_parent1 and pedigree_parent2 for this, rather than the parents
				// of the strands; in the addMultiRecombinant() case there are potentially many strands with
				// many different parents.  It is just too complex to try to keep track of just for SLiMgui.
				// BCH 1/7/2025: It used to be that addRecombinant() did this by strand, but I've dumbed it
				// down to match addMultiRecombinant(); nobody will ever notice.  Doing it by strand was
				// approximate too, and maybe even less good in some scenarios.
				if (pedigree_parent1)
				{
					pedigree_parent1->subpopulation_->gui_premigration_size_ += 0.5;
					if (pedigree_parent1->subpopulation_ != this)
						gui_migrants_[pedigree_parent1->subpopulation_->subpopulation_id_] += 0.5;
				}
				if (pedigree_parent2)
				{
					pedigree_parent2->subpopulation_->gui_premigration_size_ += 0.5;
					if (pedigree_parent2->subpopulation_ != this)
						gui_migrants_[pedigree_parent2->subpopulation_->subpopulation_id_] += 0.5;
				}
			}
#endif
		}
	}
	
	return EidosValue_SP(result);
}

//	*********************	– (o<Individual>)addSelfed(object<Individual>$ parent, [integer$ count = 1], [logical$ defer = F])
//
EidosValue_SP Subpopulation::ExecuteMethod_addSelfed(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): addSelfed() is not available in WF models." << EidosTerminate();
	
	// TIMING RESTRICTION
	if (community_.CycleStage() != SLiMCycleStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): addSelfed() may only be called from a reproduction() callback." << EidosTerminate();
	if (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): addSelfed() may not be called from a nested callback." << EidosTerminate();
	
	// Get and check the parent
	EidosValue *parent_value = p_arguments[0].get();
	Individual *parent = (Individual *)parent_value->ObjectData()[0];
	IndividualSex parent_sex = parent->sex_;
	Subpopulation &parent_subpop = *parent->subpopulation_;
	
	if (parent_sex != IndividualSex::kHermaphrodite)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): parent must be hermaphroditic in addSelfed()." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if (&parent_subpop.species_ != &this->species_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): addSelfed() requires that parent belongs to the same species as the target subpopulation." << EidosTerminate();
	
	// Check for some other illegal setups
	if (parent->index_ == -1)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): parent must be visible in a subpopulation (i.e., may not be a new juvenile)." << EidosTerminate();
	
	// Check the count and short-circuit if it is zero
	EidosValue *count_value = p_arguments[1].get();
	int64_t child_count = count_value->IntData()[0];
	
	if ((child_count < 0) || (child_count > SLIM_MAX_SUBPOP_SIZE))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): addSelfed() requires an offspring count >= 0 and <= 1000000000." << EidosTerminate();
	
	EidosValue_Object *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class))->reserve(child_count);	// reserve enough space for all results
	
	if (child_count == 0)
		return EidosValue_SP(result);
	
	// Generate the number of children requested
#if DEFER_BROKEN
	// The "defer" flag is simply disregarded at the moment; its design has rotted away,
	// and needs to be remade anew once things have settled down.
	EidosValue *defer_value = p_arguments[2].get();
	bool defer = defer_value->LogicalData()[0];
	
	if (defer)
	{
		std::vector<SLiMEidosBlock*> *parent_recombination_callbacks = &parent_subpop.registered_recombination_callbacks_;
		std::vector<SLiMEidosBlock*> *parent_mutation_callbacks = &parent_subpop.registered_mutation_callbacks_;
		
		if (!parent_recombination_callbacks->size()) parent_recombination_callbacks = nullptr;
		if (!parent_mutation_callbacks->size()) parent_mutation_callbacks = nullptr;
		
		if (parent_recombination_callbacks || parent_mutation_callbacks)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): deferred reproduction cannot be used when recombination() or mutation() callbacks are enabled." << EidosTerminate();
	}
#endif
	
	for (int64_t child_index = 0; child_index < child_count; ++child_index)
	{
		// Make the new individual; if it doesn't pass modifyChild(), nullptr will be returned
		Individual *individual = (this->*(population_.GenerateIndividualSelfed_TEMPLATED))(parent);
		
		if (individual)
		{
			nonWF_offspring_individuals_.emplace_back(individual);
			result->push_object_element_NORR(individual);
			
#if defined(SLIMGUI)
			{
				gui_offspring_selfed_++;
				
				// this offspring came from a parent in parent_subpop but ended up here, so it is, in effect, a migrant;
				// we tally things, SLiMgui display purposes, as if it were generated in parent_subpop and then moved
				parent_subpop.gui_premigration_size_++;
				if (&parent_subpop != this)
					gui_migrants_[parent_subpop.subpopulation_id_]++;
			}
#endif
		}
	}
	
	return EidosValue_SP(result);
}

//	*********************	- (void)takeMigrants(object<Individual> migrants)
//
EidosValue_SP Subpopulation::ExecuteMethod_takeMigrants(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_takeMigrants): takeMigrants() is not available in WF models." << EidosTerminate();
	
	// TIMING RESTRICTION
	if (community_.executing_species_ == &species_)
		if ((community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventFirst) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_takeMigrants): takeMigrants() must be called directly from a first(), early(), or late() event, when called on the currently executing species." << EidosTerminate();
	
	EidosValue_Object *migrants_value = (EidosValue_Object *)p_arguments[0].get();
	int migrant_count = migrants_value->Count();
	int moved_count = 0;
	
	if (migrant_count == 0)
		return gStaticEidosValueVOID;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividuals(migrants_value);
	
	if (species != &this->species_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_takeMigrants): takeMigrants() requires that all individuals belong to the same species as the target subpopulation." << EidosTerminate();
	
	if (has_been_removed_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_takeMigrants): takeMigrants() should not be called to add individuals to a subpopulation that has been removed." << EidosTerminate();

	// Loop over the migrants and move them one by one
	int haplosome_count_per_individual = species->HaplosomeCountPerIndividual();
	Individual * const *migrants = (Individual * const *)migrants_value->ObjectData();
	
	for (int migrant_index = 0; migrant_index < migrant_count; ++migrant_index)
	{
		Individual *migrant = migrants[migrant_index];
		Subpopulation *source_subpop = migrant->subpopulation_;
		
		if (source_subpop != this)
		{
#if defined(SLIMGUI)
			// before doing anything else, tally this incoming migrant for SLiMgui
			++gui_migrants_[source_subpop->subpopulation_id_];
#endif
			
			slim_popsize_t source_subpop_size = source_subpop->parent_subpop_size_;
			slim_popsize_t source_subpop_index = migrant->index_;
			
			if (source_subpop_index < 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_takeMigrants): takeMigrants() may not move an individual that is not visible in a subpopulation.  This error may also occur if you try to migrate the same individual more than once in a single takeMigrants() call (i.e., if the migrants vector is not uniqued)." << EidosTerminate();
			
			// remove the originals from source_subpop's vectors
			if (migrant->sex_ == IndividualSex::kFemale)
			{
				// females have to be backfilled by the last female, and then that hole is backfilled by a male, and the first male index changes
				slim_popsize_t source_first_male = source_subpop->parent_first_male_index_;
				
				if (source_subpop_index < source_first_male - 1)
				{
					Individual *backfill = source_subpop->parent_individuals_[source_first_male - 1];
					
					source_subpop->parent_individuals_[source_subpop_index] = backfill;
					backfill->index_ = source_subpop_index;
				}
				
				if (source_first_male - 1 < source_subpop_size - 1)
				{
					Individual *backfill = source_subpop->parent_individuals_[source_subpop_size - 1];
					
					source_subpop->parent_individuals_[source_first_male - 1] = backfill;
					backfill->index_ = source_first_male - 1;
				}
				
				source_subpop->parent_subpop_size_ = --source_subpop_size;
				source_subpop->parent_individuals_.resize(source_subpop_size);
				
				source_subpop->parent_first_male_index_ = --source_first_male;
			}
			else
			{
				// males and hermaphrodites can be removed with a simple backfill from the end of the vector
				if (source_subpop_index < source_subpop_size - 1)
				{
					Individual *backfill = source_subpop->parent_individuals_[source_subpop_size - 1];
					
					source_subpop->parent_individuals_[source_subpop_index] = backfill;
					backfill->index_ = source_subpop_index;
				}
				
				source_subpop->parent_subpop_size_ = --source_subpop_size;
				source_subpop->parent_individuals_.resize(source_subpop_size);
			}
			
			// insert the migrant into ourselves
			if ((migrant->sex_ == IndividualSex::kFemale) && (parent_first_male_index_ < parent_subpop_size_))
			{
				// room has to be made for females by shifting the first male and changing the first male index
				Individual *backfill = parent_individuals_[parent_first_male_index_];
				
				parent_individuals_.emplace_back(backfill);
				backfill->index_ = parent_subpop_size_;
				
				parent_individuals_[parent_first_male_index_] = migrant;
				
				migrant->subpopulation_ = this;
				migrant->index_ = parent_first_male_index_;
				
				parent_subpop_size_++;
				parent_first_male_index_++;
			}
			else
			{
				// males and hermaphrodites can just be added to the end; so can females, if no males are present
				parent_individuals_.emplace_back(migrant);
				
				migrant->subpopulation_ = this;
				migrant->index_ = parent_subpop_size_;
				
				parent_subpop_size_++;
				if (migrant->sex_ == IndividualSex::kFemale)
					parent_first_male_index_++;
			}
			
			// has_null_haplosomes_ needs to reflect the presence of null haplosomes
			if (!has_null_haplosomes_ && source_subpop->has_null_haplosomes_)
			{
				Haplosome **haplosomes = migrant->haplosomes_;
				
				for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					if (haplosome->IsNull())
						has_null_haplosomes_ = true;
				}
			}
			
			// set the migrant flag of the migrated individual; note this is not set if the individual was already in the destination subpop
			migrant->migrant_ = true;
			
			moved_count++;
		}
	}
	
	if (moved_count)
	{
		// First, clear our haplosome and individual caches in all subpopulations; any subpops involved in
		// the migration would be invalidated anyway so this probably isn't even that much overkill in
		// most models.  Note that the child haplosomes/individuals caches don't need to be thrown away,
		// because they aren't used in nonWF models and this is a nonWF-only method.
		for (auto subpop_pair : population_.subpops_)
			subpop_pair.second->cached_parent_individuals_value_.reset();
		
		// Invalidate interactions; we just do this for all subpops, for now, rather than trying to
		// selectively invalidate only the subpops involved in the migrations that occurred
		community_.InvalidateInteractionsForSpecies(&species_);
		
		// cached mutation counts/frequencies are no longer accurate; mark the cache as invalid
		population_.InvalidateMutationReferencesCache();
	}
	
	return gStaticEidosValueVOID;
}

// WF only:
//	*********************	- (void)setMigrationRates(object sourceSubpops, numeric rates)
//
EidosValue_SP Subpopulation::ExecuteMethod_setMigrationRates(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeNonWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setMigrationRates): setMigrationRates() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *sourceSubpops_value = p_arguments[0].get();
	EidosValue *rates_value = p_arguments[1].get();
	
	int source_subpops_count = sourceSubpops_value->Count();
	int rates_count = rates_value->Count();
	std::vector<slim_objectid_t> subpops_seen;
	
	if (source_subpops_count != rates_count)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setMigrationRates): setMigrationRates() requires sourceSubpops and rates to be equal in size." << EidosTerminate();
	
	for (int value_index = 0; value_index < source_subpops_count; ++value_index)
	{
		EidosObject *source_subpop = SLiM_ExtractSubpopulationFromEidosValue_io(sourceSubpops_value, value_index, &species_.community_, &species_, "setMigrationRates()");		// SPECIES CONSISTENCY CHECK
		slim_objectid_t source_subpop_id = ((Subpopulation *)(source_subpop))->subpopulation_id_;
		
		if (source_subpop_id == subpopulation_id_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setMigrationRates): setMigrationRates() does not allow migration to be self-referential (originating within the destination subpopulation)." << EidosTerminate();
		if (std::find(subpops_seen.begin(), subpops_seen.end(), source_subpop_id) != subpops_seen.end())
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setMigrationRates): setMigrationRates() two rates set for subpopulation p" << source_subpop_id << "." << EidosTerminate();
		
		double migrant_fraction = rates_value->NumericAtIndex_NOCAST(value_index, nullptr);
		
		population_.SetMigration(*this, source_subpop_id, migrant_fraction);
		subpops_seen.emplace_back(source_subpop_id);
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	- (object<Haplosome>)haplosomesForChromosomes([Niso<Chromosome> chromosomes = NULL], [Ni$ index = NULL], [logical$ includeNulls = T])
//
EidosValue_SP Subpopulation::ExecuteMethod_haplosomesForChromosomes(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *chromosomes_value = p_arguments[0].get();
	EidosValue *index_value = p_arguments[1].get();
	EidosValue *includeNulls_value = p_arguments[2].get();
	
	// assemble a vector of chromosome indices we're fetching
	std::vector<slim_chromosome_index_t> chromosome_indices;
	
	species_.GetChromosomeIndicesFromEidosValue(chromosome_indices, chromosomes_value);
	
	// get index and includeNulls
	int64_t index = -1;	// for NULL
	
	if (index_value->Type() == EidosValueType::kValueInt)
	{
		index = index_value->IntAtIndex_NOCAST(0, nullptr);
		
		if ((index != 0) && (index != 1))
			EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_haplosomesForChromosomes): haplosomesForChromosomes() requires that index is 0, 1, or NULL." << EidosTerminate();
	}
	
	bool includeNulls = includeNulls_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	// fetch the requested haplosomes for all individuals
	EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Haplosome_Class));
	
	for (Individual *individual : parent_individuals_)
		individual->AppendHaplosomesForChromosomes(vec, chromosome_indices, index, includeNulls);
	
	return EidosValue_SP(vec);
}

//	*********************	– (object<Individual>)deviatePositions(No<Individual> individuals, string$ boundary, numeric$ maxDistance, string$ functionType, ...)
//
EidosValue_SP Subpopulation::ExecuteMethod_deviatePositions(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	// NOTE: most of the code of this method is shared with pointDeviated()
	
	SLiMCycleStage cycle_stage = community_.CycleStage();
	
	// TIMING RESTRICTION
	if ((cycle_stage != SLiMCycleStage::kWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kWFStage1ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kWFStage5ExecuteLateScripts) &&
		(cycle_stage != SLiMCycleStage::kNonWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_deviatePositions): deviatePositions() may only be called from a first(), early(), or late() event." << EidosTerminate();
	if ((community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventFirst) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_deviatePositions): deviatePositions() may not be called from inside a callback." << EidosTerminate();
	
	int dimensionality = species_.SpatialDimensionality();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositions): deviatePositions() cannot be called in non-spatial simulations." << EidosTerminate();
	
	EidosValue *individuals_value = p_arguments[0].get();
	Individual * const *individuals;
	int individuals_count;
	
	if (individuals_value->Type() == EidosValueType::kValueNULL)
	{
		// NULL requests that the positions of all individuals in the subpop should be deviated
		individuals = parent_individuals_.data();
		individuals_count = parent_subpop_size_;
	}
	else
	{
		individuals = (Individual * const *)individuals_value->ObjectData();
		individuals_count = individuals_value->Count();
	}
	
	// Make a return vector that is initially empty; unless boundary is "absorbing", it will remain empty
	EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class));
	EidosValue_Object *result = ((EidosValue_Object *)result_SP.get());
	
	if (individuals_count == 0)
		return result_SP;
	
	EidosValue_String *boundary_value = (EidosValue_String *)p_arguments[1].get();
	const std::string &boundary_str = boundary_value->StringRefAtIndex_NOCAST(0, nullptr);
	BoundaryCondition boundary;
	
	if (boundary_str.compare("none") == 0)
		boundary = BoundaryCondition::kNone;
	else if (boundary_str.compare("stopping") == 0)
		boundary = BoundaryCondition::kStopping;
	else if (boundary_str.compare("reflecting") == 0)
		boundary = BoundaryCondition::kReflecting;
	else if (boundary_str.compare("reprising") == 0)
		boundary = BoundaryCondition::kReprising;
	else if (boundary_str.compare("absorbing") == 0)
		boundary = BoundaryCondition::kAbsorbing;
	else if (boundary_str.compare("periodic") == 0)
		boundary = BoundaryCondition::kPeriodic;
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositions): unrecognized boundary condition '" << boundary_str << "'." << EidosTerminate();
	
	// Periodic boundaries are a bit complicated.  If only some dimensions are periodic, 'none' will be used
	// for the non-periodic boundaries, and the user can then use pointReflected(), pointStopped(), etc. to
	// enforce a boundary condition on those dimensions.
	bool periodic_x = false, periodic_y = false, periodic_z = false;
	
	if (boundary == BoundaryCondition::kPeriodic)
	{
		// Since periodic boundaries depend upon the species configuration, we require all individuals to belong to the target species here
		// In other cases, it doesn't seem necessary to enforce this, and it might be useful to be able to violate it
		if (community_.SpeciesForIndividualsVector(individuals, individuals_count) != &species_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositions): deviatePositions() requires that all individuals belong to the same species as the target subpopulation, when periodic boundaries are requested." << EidosTerminate();
		
		species_.SpatialPeriodicity(&periodic_x, &periodic_y, &periodic_z);
		
		if (!periodic_x && !periodic_y && !periodic_z)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositions): deviatePositions() cannot apply periodic boundary conditions in a model without periodic boundaries." << EidosTerminate();
	}
	
	EidosValue *maxDistance_value = p_arguments[2].get();
	double max_distance = maxDistance_value->NumericAtIndex_NOCAST(0, nullptr);
	
	SpatialKernelType k_type;
	int k_param_count;
	int kernel_count = SpatialKernel::PreprocessArguments(dimensionality, max_distance, p_arguments, 3, /* p_expect_max_density */ false, &k_type, &k_param_count);
	
	if ((kernel_count != 1) && (kernel_count != individuals_count))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositions): deviatePositions() requires that the number of spatial kernels defined (by the supplied kernel-definition arguments) either must be 1, or must equal the number of individuals being processed (" << kernel_count << " kernels defined; " << individuals_count << " individuals processed)." << EidosTerminate();
	
	SpatialKernel kernel0(dimensionality, max_distance, p_arguments, 3, 0, /* p_expect_max_density */ false, k_type, k_param_count);	// uses our arguments starting at index 3
	
	// I'm not going to worry about unrolling each case, for dimensionality by boundary by kernel type; it would
	// be a ton of cases (3 x 5 x 5 = 75), and the overhead for the switches ought to be small compared to the
	// overhead of drawing a displacement from the kernel, which requires a random number draw.  I tested doing
	// a special-case here for dimensionality==2, boundary==1 (stopping), kernel.kernel_type==kNormal,
	// maxDistance==INF, and it clocked at 6.47 seconds versus 7.85 seconds for the unoptimized code below;
	// that's about a 17.6% speedup, which is worthwhile for a handful of special cases like that.  I think
	// normal deviations in 2D with an INF maxDistance are the 95% case, if not 99%; several boundary conditions
	// are common, though.
	if ((kernel_count == 1) && (dimensionality == 2) && (kernel0.kernel_type_ == SpatialKernelType::kNormal) && std::isinf(kernel0.max_distance_) && ((boundary == BoundaryCondition::kStopping) || (boundary == BoundaryCondition::kReflecting) || (boundary == BoundaryCondition::kReprising) || (boundary == BoundaryCondition::kAbsorbing) || ((boundary == BoundaryCondition::kPeriodic) && periodic_x && periodic_y)))
	{
		gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
		double stddev = kernel0.kernel_param2_;
		double bx0 = bounds_x0_, bx1 = bounds_x1_;
		double by0 = bounds_y0_, by1 = bounds_y1_;
		
		if (boundary == BoundaryCondition::kStopping)
		{
			// FIXME: TO BE PARALLELIZED
			for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
			{
				Individual *ind = individuals[individual_index];
				double a0 = ind->spatial_x_ + gsl_ran_gaussian(rng, stddev);
				double a1 = ind->spatial_y_ + gsl_ran_gaussian(rng, stddev);
				
				a0 = std::max(bx0, std::min(bx1, a0));
				a1 = std::max(by0, std::min(by1, a1));
				
				ind->spatial_x_ = a0;
				ind->spatial_y_ = a1;
			}
		}
		else if (boundary == BoundaryCondition::kReflecting)
		{
			// FIXME: TO BE PARALLELIZED
			for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
			{
				Individual *ind = individuals[individual_index];
				double a0 = ind->spatial_x_ + gsl_ran_gaussian(rng, stddev);
				double a1 = ind->spatial_y_ + gsl_ran_gaussian(rng, stddev);
				
				while (true)
				{
					if (a0 < bx0) a0 = bx0 + (bx0 - a0);
					else if (a0 > bx1) a0 = bx1 - (a0 - bx1);
					else break;
				}
				while (true)
				{
					if (a1 < by0) a1 = by0 + (by0 - a1);
					else if (a1 > by1) a1 = by1 - (a1 - by1);
					else break;
				}
				
				ind->spatial_x_ = a0;
				ind->spatial_y_ = a1;
			}
		}
		else if (boundary == BoundaryCondition::kReprising)
		{
			// FIXME: TO BE PARALLELIZED
			for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
			{
				Individual *ind = individuals[individual_index];
				double a0_original = ind->spatial_x_;
				double a1_original = ind->spatial_y_;
				
			reprise_specialcase:
				double a0 = a0_original + gsl_ran_gaussian(rng, stddev);
				double a1 = a1_original + gsl_ran_gaussian(rng, stddev);
				
				if ((a0 < bx0) || (a0 > bx1) ||
					(a1 < by0) || (a1 > by1))
					goto reprise_specialcase;
				
				ind->spatial_x_ = a0;
				ind->spatial_y_ = a1;
			}
		}
		else if (boundary == BoundaryCondition::kAbsorbing)
		{
			// FIXME: TO BE PARALLELIZED
			for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
			{
				Individual *ind = individuals[individual_index];
				double a0 = ind->spatial_x_ + gsl_ran_gaussian(rng, stddev);
				double a1 = ind->spatial_y_ + gsl_ran_gaussian(rng, stddev);
				
				if ((a0 < bx0) || (a0 > bx1) ||
					(a1 < by0) || (a1 > by1))
					result->push_object_element_capcheck_NORR(ind);
				
				ind->spatial_x_ = a0;
				ind->spatial_y_ = a1;
			}
		}
		else if (boundary == BoundaryCondition::kPeriodic)
		{
			// FIXME: TO BE PARALLELIZED
			for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
			{
				Individual *ind = individuals[individual_index];
				double a0 = ind->spatial_x_ + gsl_ran_gaussian(rng, stddev);
				double a1 = ind->spatial_y_ + gsl_ran_gaussian(rng, stddev);
				
				// (note periodic_x and periodic_y are required to be true above)
				while (a0 < 0.0)	a0 += bx1;
				while (a0 > bx1)	a0 -= bx1;
				while (a1 < 0.0)	a1 += by1;
				while (a1 > by1)	a1 -= by1;
				
				ind->spatial_x_ = a0;
				ind->spatial_y_ = a1;
			}
		}
		return result_SP;
	}
	
	// main code path; note that here we may have multiple kernels defined, one per individual
	switch (dimensionality)
	{
		case 1:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_;
			
			// FIXME: TO BE PARALLELIZED
			for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
			{
				SpatialKernel kernel((kernel_count == 1) ? kernel0 : SpatialKernel(dimensionality, max_distance, p_arguments, 3, individual_index, /* p_expect_max_density */ false, k_type, k_param_count));
				Individual *ind = individuals[individual_index];
				double a[1];
				
			reprise_1:
				kernel.DrawDisplacement_S1(a);
				a[0] += ind->spatial_x_;
				
				// enforce the boundary condition
				switch (boundary)
				{
					case BoundaryCondition::kNone:
						break;
					case BoundaryCondition::kStopping:
						a[0] = std::max(bx0, std::min(bx1, a[0]));
						break;
					case BoundaryCondition::kReflecting:
						while (true)
						{
							if (a[0] < bx0) a[0] = bx0 + (bx0 - a[0]);
							else if (a[0] > bx1) a[0] = bx1 - (a[0] - bx1);
							else break;
						}
						break;
					case BoundaryCondition::kReprising:
						if ((a[0] < bx0) || (a[0] > bx1))
							goto reprise_1;
						break;
					case BoundaryCondition::kAbsorbing:
						if ((a[0] < bx0) || (a[0] > bx1))
							result->push_object_element_capcheck_NORR(ind);
						break;
					case BoundaryCondition::kPeriodic:			// (periodic_x must be true)
						while (a[0] < 0.0)	a[0] += bx1;
						while (a[0] > bx1)	a[0] -= bx1;
						break;
				}
				
				ind->spatial_x_ = a[0];
			}
			break;
		}
		case 2:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_;
			double by0 = bounds_y0_, by1 = bounds_y1_;
			
			// FIXME: TO BE PARALLELIZED
			for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
			{
				SpatialKernel kernel((kernel_count == 1) ? kernel0 : SpatialKernel(dimensionality, max_distance, p_arguments, 3, individual_index, /* p_expect_max_density */ false, k_type, k_param_count));
				Individual *ind = individuals[individual_index];
				double a[2];
				
			reprise_2:
				kernel.DrawDisplacement_S2(a);
				a[0] += ind->spatial_x_;
				a[1] += ind->spatial_y_;
				
				// enforce the boundary condition
				switch (boundary)
				{
					case BoundaryCondition::kNone:
						break;
					case BoundaryCondition::kStopping:
						a[0] = std::max(bx0, std::min(bx1, a[0]));
						a[1] = std::max(by0, std::min(by1, a[1]));
						break;
					case BoundaryCondition::kReflecting:
						while (true)
						{
							if (a[0] < bx0) a[0] = bx0 + (bx0 - a[0]);
							else if (a[0] > bx1) a[0] = bx1 - (a[0] - bx1);
							else break;
						}
						while (true)
						{
							if (a[1] < by0) a[1] = by0 + (by0 - a[1]);
							else if (a[1] > by1) a[1] = by1 - (a[1] - by1);
							else break;
						}
						break;
					case BoundaryCondition::kReprising:
						if ((a[0] < bx0) || (a[0] > bx1) ||
							(a[1] < by0) || (a[1] > by1))
							goto reprise_2;
						break;
					case BoundaryCondition::kAbsorbing:
						if ((a[0] < bx0) || (a[0] > bx1) ||
							(a[1] < by0) || (a[1] > by1))
							result->push_object_element_capcheck_NORR(ind);
						break;
					case BoundaryCondition::kPeriodic:
						if (periodic_x)
						{
							while (a[0] < 0.0)	a[0] += bx1;
							while (a[0] > bx1)	a[0] -= bx1;
						}
						if (periodic_y)
						{
							while (a[1] < 0.0)	a[1] += by1;
							while (a[1] > by1)	a[1] -= by1;
						}
						break;
				}
				
				ind->spatial_x_ = a[0];
				ind->spatial_y_ = a[1];
			}
			break;
		}
		case 3:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_;
			double by0 = bounds_y0_, by1 = bounds_y1_;
			double bz0 = bounds_z0_, bz1 = bounds_z1_;
			
			// FIXME: TO BE PARALLELIZED
			for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
			{
				SpatialKernel kernel((kernel_count == 1) ? kernel0 : SpatialKernel(dimensionality, max_distance, p_arguments, 3, individual_index, /* p_expect_max_density */ false, k_type, k_param_count));
				Individual *ind = individuals[individual_index];
				double a[3];
				
			reprise_3:
				kernel.DrawDisplacement_S3(a);
				a[0] += ind->spatial_x_;
				a[1] += ind->spatial_y_;
				a[2] += ind->spatial_z_;
				
				// enforce the boundary condition
				switch (boundary)
				{
					case BoundaryCondition::kNone:
						break;
					case BoundaryCondition::kStopping:
						a[0] = std::max(bx0, std::min(bx1, a[0]));
						a[1] = std::max(by0, std::min(by1, a[1]));
						a[2] = std::max(bz0, std::min(bz1, a[2]));
						break;
					case BoundaryCondition::kReflecting:
						while (true)
						{
							if (a[0] < bx0) a[0] = bx0 + (bx0 - a[0]);
							else if (a[0] > bx1) a[0] = bx1 - (a[0] - bx1);
							else break;
						}
						while (true)
						{
							if (a[1] < by0) a[1] = by0 + (by0 - a[1]);
							else if (a[1] > by1) a[1] = by1 - (a[1] - by1);
							else break;
						}
						while (true)
						{
							if (a[2] < bz0) a[2] = bz0 + (bz0 - a[2]);
							else if (a[2] > bz1) a[2] = bz1 - (a[2] - bz1);
							else break;
						}
						break;
					case BoundaryCondition::kReprising:
						if ((a[0] < bx0) || (a[0] > bx1) ||
							(a[1] < by0) || (a[1] > by1) ||
							(a[2] < bz0) || (a[2] > bz1))
							goto reprise_3;
						break;
					case BoundaryCondition::kAbsorbing:
						if ((a[0] < bx0) || (a[0] > bx1) ||
							(a[1] < by0) || (a[1] > by1) ||
							(a[2] < bz0) || (a[2] > bz1))
							result->push_object_element_capcheck_NORR(ind);
						break;
					case BoundaryCondition::kPeriodic:
						if (periodic_x)
						{
							while (a[0] < 0.0)	a[0] += bx1;
							while (a[0] > bx1)	a[0] -= bx1;
						}
						if (periodic_y)
						{
							while (a[1] < 0.0)	a[1] += by1;
							while (a[1] > by1)	a[1] -= by1;
						}
						if (periodic_z)
						{
							while (a[2] < 0.0)	a[2] += bz1;
							while (a[2] > bz1)	a[2] -= bz1;
						}
						break;
				}
				
				ind->spatial_x_ = a[0];
				ind->spatial_y_ = a[1];
				ind->spatial_z_ = a[2];
			}
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositions): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return result_SP;
}

//	*********************	– (object<Individual>)deviatePositionsWithMap(No<Individual> individuals, string$ boundary, so<SpatialMap>$ map, numeric$ maxDistance, string$ functionType, ...)
//
EidosValue_SP Subpopulation::ExecuteMethod_deviatePositionsWithMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	// NOTE: most of the code of this method is shared with pointDeviated(), and even more, with deviatePositions()
	
	SLiMCycleStage cycle_stage = community_.CycleStage();
	
	// TIMING RESTRICTION
	if ((cycle_stage != SLiMCycleStage::kWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kWFStage1ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kWFStage5ExecuteLateScripts) &&
		(cycle_stage != SLiMCycleStage::kNonWFStage0ExecuteFirstScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage2ExecuteEarlyScripts) && (cycle_stage != SLiMCycleStage::kNonWFStage6ExecuteLateScripts))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() may only be called from a first(), early(), or late() event." << EidosTerminate();
	if ((community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventFirst) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
		EIDOS_TERMINATION << "ERROR (Species::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() may not be called from inside a callback." << EidosTerminate();
	
	int dimensionality = species_.SpatialDimensionality();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() cannot be called in non-spatial simulations." << EidosTerminate();
	
	EidosValue *individuals_value = p_arguments[0].get();
	Individual * const *individuals;
	int individuals_count;
	
	if (individuals_value->Type() == EidosValueType::kValueNULL)
	{
		// NULL requests that the positions of all individuals in the subpop should be deviated
		individuals = parent_individuals_.data();
		individuals_count = parent_subpop_size_;
	}
	else
	{
		individuals = (Individual * const *)individuals_value->ObjectData();
		individuals_count = individuals_value->Count();
	}
	
	// Make a return vector that is initially empty; unless boundary is "absorbing", it will remain empty
	EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class));
	EidosValue_Object *result = ((EidosValue_Object *)result_SP.get());
	
	if (individuals_count == 0)
		return result_SP;
	
	EidosValue_String *boundary_value = (EidosValue_String *)p_arguments[1].get();
	const std::string &boundary_str = boundary_value->StringRefAtIndex_NOCAST(0, nullptr);
	BoundaryCondition boundary;
	
	if (boundary_str.compare("reprising") == 0)
		boundary = BoundaryCondition::kReprising;
	else if (boundary_str.compare("absorbing") == 0)
		boundary = BoundaryCondition::kAbsorbing;
	else if ((boundary_str.compare("none") == 0) ||
			 (boundary_str.compare("stopping") == 0) ||
			 (boundary_str.compare("reflecting") == 0) ||
			 (boundary_str.compare("periodic") == 0))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() does not support boundary condition '" << boundary_str << "'." << EidosTerminate();
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositionsWithMap): unrecognized boundary condition '" << boundary_str << "'." << EidosTerminate();
	
	// Periodic boundaries are a bit complicated.  Unlike deviatePositions(), we automatically apply periodic
	// boundaries if they are defined, so that the deviated point is within spatial bounds and can be checked
	// against the spatial map.
	// Since periodic boundaries depend upon the species configuration, we require all individuals to belong to the target species here
	// In other cases, it doesn't seem necessary to enforce this, and it might be useful to be able to violate it
	bool periodic_x = false, periodic_y = false, periodic_z = false;
	
	species_.SpatialPeriodicity(&periodic_x, &periodic_y, &periodic_z);
	
	if (periodic_x || periodic_y || periodic_z)
	{
		if (community_.SpeciesForIndividualsVector(individuals, individuals_count) != &species_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() requires that all individuals belong to the same species as the target subpopulation, when periodic boundaries are in effect." << EidosTerminate();
	}
	
	// Get the spatial map's name; see ExecuteMethod_spatialMapValue() for the origin of this code
	EidosValue *map_value = p_arguments[2].get();
	SpatialMap *map = nullptr;
	std::string map_name;
	
	if (map_value->Type() == EidosValueType::kValueString)
	{
		map_name = ((EidosValue_String *)map_value)->StringRefAtIndex_NOCAST(0, nullptr);
		
		if (map_name.length() == 0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() map name must not be zero-length." << EidosTerminate();
	}
	else
	{
		map = (SpatialMap *)map_value->ObjectElementAtIndex_NOCAST(0, nullptr);
		map_name = map->name_;
	}
	
	// Find the SpatialMap by name; we do this lookup even if a map object was supplied, to check that that map is present
	auto map_iter = spatial_maps_.find(map_name);
	
	if (map_iter != spatial_maps_.end())
	{
		SpatialMap *found_map = map_iter->second;
		
		if (map && (found_map != map))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() could not find map in the target subpopulation (although it did find a different map with the same name)." << EidosTerminate();
		
		map = found_map;
	}
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() could not find map '" << map_name << "' in the target subpopulation." << EidosTerminate();
	
	bool map_interpolated = map->interpolate_;
	
	// The map is required to have spatiality equal to the species dimensionality.  This is because the value
	// queries to the map using ValueAtPoint_S2() etc. are in the map's spatiality, and we don't want to have
	// to be translating points in and out of that spatiality.  Other methods don't have this problem because
	// either they get passed a point (which must be in the map or interaction type's spatiality) or they have
	// cached the point in the correct spatiality internally (as InteractionType does).  Users who have a map
	// in a weird spatiality can always do the work of this method themselves, with more basic calls.
	if (dimensionality != map->spatiality_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() requires that the map's spatiality is equal to the dimensionality of the species." << EidosTerminate();
	
	// Process the max distance and kernel
	EidosValue *maxDistance_value = p_arguments[3].get();
	double max_distance = maxDistance_value->NumericAtIndex_NOCAST(0, nullptr);
	
	SpatialKernelType k_type;
	int k_param_count;
	int kernel_count = SpatialKernel::PreprocessArguments(dimensionality, max_distance, p_arguments, 4, /* p_expect_max_density */ false, &k_type, &k_param_count);
	
	if ((kernel_count != 1) && (kernel_count != individuals_count))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() requires that the number of spatial kernels defined (by the supplied kernel-definition arguments) either must be 1, or must equal the number of individuals being processed (" << kernel_count << " kernels defined; " << individuals_count << " individuals processed)." << EidosTerminate();
	
	SpatialKernel kernel0(dimensionality, max_distance, p_arguments, 4, 0, /* p_expect_max_density */ false, k_type, k_param_count);	// uses our arguments starting at index 4
	
	// I'm not going to worry about unrolling each case, for dimensionality by boundary by kernel type; it would
	// be a ton of cases (3 x 5 x 5 = 75), and the overhead for the switches ought to be small compared to the
	// overhead of drawing a displacement from the kernel, which requires a random number draw.  However, common
	// 2D cases are optimized here; see deviatePositions().  This provides about a 10% speedup compared to the
	// general-purpose code below.  The sub-optimization here for non-interpolated maps provides another 3% or so,
	// which is pretty marginal, but it's an easy optimization.
	if ((kernel_count == 1) && (dimensionality == 2) && (kernel0.kernel_type_ == SpatialKernelType::kNormal) && std::isinf(kernel0.max_distance_) && !periodic_x && !periodic_y && !periodic_z && ((boundary == BoundaryCondition::kReprising) || (boundary == BoundaryCondition::kAbsorbing)))
	{
		gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
		double stddev = kernel0.kernel_param2_;
		double bx0 = bounds_x0_, bx1 = bounds_x1_;
		double by0 = bounds_y0_, by1 = bounds_y1_;
		double bounds_size_x = bx1 - bx0;
		double bounds_size_y = by1 - by0;
		
		if (boundary == BoundaryCondition::kReprising)
		{
			if (map_interpolated)
			{
				// FIXME: TO BE PARALLELIZED
				for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
				{
					Individual *ind = individuals[individual_index];
					double a0_original = ind->spatial_x_;
					double a1_original = ind->spatial_y_;
					int num_tries = 0;
					
				reprise_specialcase:
					if (++num_tries == 1000000)
						EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() failed to find a successful deviated point by reprising after 1 million attempts; terminating to avoid infinite loop." << EidosTerminate();
					
					double a0 = a0_original + gsl_ran_gaussian(rng, stddev);
					double a1 = a1_original + gsl_ran_gaussian(rng, stddev);
					
					if ((a0 < bx0) || (a0 > bx1) ||
						(a1 < by0) || (a1 > by1))
						goto reprise_specialcase;
					
					// within the spatial bounds, so now we have to check the map
					double a_normalized[2];
					
					a_normalized[0] = (a0 - bx0) / bounds_size_x;
					a_normalized[1] = (a1 - by0) / bounds_size_y;
					
					double value_for_point = map->ValueAtPoint_S2(a_normalized);
					
					if (value_for_point <= 0)
					{
						// habitability 0: always reprise
						goto reprise_specialcase;
					}
					else if (value_for_point >= 1)
					{
						// habitability 1: never reprise (drop through)
					}
					else
					{
						// intermediate: do a random number draw, where value_for_point is P(within bounds)
						if (Eidos_rng_uniform(rng) > value_for_point)
							goto reprise_specialcase;
					}
					
					ind->spatial_x_ = a0;
					ind->spatial_y_ = a1;
				}
			}
			else	// !map_interpolated
			{
				// FIXME: TO BE PARALLELIZED
				for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
				{
					Individual *ind = individuals[individual_index];
					double a0_original = ind->spatial_x_;
					double a1_original = ind->spatial_y_;
					int num_tries = 0;
					
				reprise_specialcase_nointerp:
					if (++num_tries == 1000000)
						EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() failed to find a successful deviated point by reprising after 1 million attempts; terminating to avoid infinite loop." << EidosTerminate();
					
					double a0 = a0_original + gsl_ran_gaussian(rng, stddev);
					double a1 = a1_original + gsl_ran_gaussian(rng, stddev);
					
					if ((a0 < bx0) || (a0 > bx1) ||
						(a1 < by0) || (a1 > by1))
						goto reprise_specialcase_nointerp;
					
					// within the spatial bounds, so now we have to check the map
					double a0_normalized = (a0 - bx0) / bounds_size_x;
					double a1_normalized = (a1 - by0) / bounds_size_y;
					double value_for_point = map->ValueAtPoint_S2_NOINTERPOLATE(a0_normalized, a1_normalized);
					
					if (value_for_point <= 0)
					{
						// habitability 0: always reprise
						goto reprise_specialcase_nointerp;
					}
					else if (value_for_point >= 1)
					{
						// habitability 1: never reprise (drop through)
					}
					else
					{
						// intermediate: do a random number draw, where value_for_point is P(within bounds)
						if (Eidos_rng_uniform(rng) > value_for_point)
							goto reprise_specialcase_nointerp;
					}
					
					ind->spatial_x_ = a0;
					ind->spatial_y_ = a1;
				}
			}
		}
		else if (boundary == BoundaryCondition::kAbsorbing)
		{
			if (map_interpolated)
			{
				// FIXME: TO BE PARALLELIZED
				for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
				{
					Individual *ind = individuals[individual_index];
					double a0 = ind->spatial_x_ + gsl_ran_gaussian(rng, stddev);
					double a1 = ind->spatial_y_ + gsl_ran_gaussian(rng, stddev);
					
					if ((a0 < bx0) || (a0 > bx1) ||
						(a1 < by0) || (a1 > by1))
					{
						result->push_object_element_capcheck_NORR(ind);
					}
					else
					{
						// within the spatial bounds, so now we have to check the map
						double a_normalized[2];
						
						a_normalized[0] = (a0 - bx0) / bounds_size_x;
						a_normalized[1] = (a1 - by0) / bounds_size_y;
						
						double value_for_point = map->ValueAtPoint_S2(a_normalized);
						
						if (value_for_point <= 0)
						{
							// habitability 0: always absorb
							result->push_object_element_capcheck_NORR(ind);
						}
						else if (value_for_point >= 1)
						{
							// habitability 1: never absorb (drop through)
						}
						else
						{
							// intermediate: do a random number draw, where value_for_point is P(within bounds)
							if (Eidos_rng_uniform(rng) > value_for_point)
								result->push_object_element_capcheck_NORR(ind);
						}
					}
					
					ind->spatial_x_ = a0;
					ind->spatial_y_ = a1;
				}
			}
			else	// !map_interpolated
			{
				// FIXME: TO BE PARALLELIZED
				for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
				{
					Individual *ind = individuals[individual_index];
					double a0 = ind->spatial_x_ + gsl_ran_gaussian(rng, stddev);
					double a1 = ind->spatial_y_ + gsl_ran_gaussian(rng, stddev);
					
					if ((a0 < bx0) || (a0 > bx1) ||
						(a1 < by0) || (a1 > by1))
					{
						result->push_object_element_capcheck_NORR(ind);
					}
					else
					{
						// within the spatial bounds, so now we have to check the map
						double a0_normalized = (a0 - bx0) / bounds_size_x;
						double a1_normalized = (a1 - by0) / bounds_size_y;
						double value_for_point = map->ValueAtPoint_S2_NOINTERPOLATE(a0_normalized, a1_normalized);
						
						if (value_for_point <= 0)
						{
							// habitability 0: always absorb
							result->push_object_element_capcheck_NORR(ind);
						}
						else if (value_for_point >= 1)
						{
							// habitability 1: never absorb (drop through)
						}
						else
						{
							// intermediate: do a random number draw, where value_for_point is P(within bounds)
							if (Eidos_rng_uniform(rng) > value_for_point)
								result->push_object_element_capcheck_NORR(ind);
						}
					}
					
					ind->spatial_x_ = a0;
					ind->spatial_y_ = a1;
				}
			}
		}
		return result_SP;
	}
	
	// main code path; note that here we may have multiple kernels defined, one per individual
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	switch (dimensionality)
	{
		case 1:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_;
			double bounds_size_x = bx1 - bx0;
			
			// FIXME: TO BE PARALLELIZED
			for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
			{
				SpatialKernel kernel((kernel_count == 1) ? kernel0 : SpatialKernel(dimensionality, max_distance, p_arguments, 4, individual_index, /* p_expect_max_density */ false, k_type, k_param_count));
				Individual *ind = individuals[individual_index];
				double a[1];
				int num_tries = 0;
				
			reprise_1:
				kernel.DrawDisplacement_S1(a);
				a[0] += ind->spatial_x_;
				
				if (periodic_x)
				{
					while (a[0] < 0.0)	a[0] += bx1;
					while (a[0] > bx1)	a[0] -= bx1;
				}
				
				// enforce the boundary condition
				switch (boundary)
				{
					case BoundaryCondition::kReprising:
					{
						if (++num_tries == 1000000)
							EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() failed to find a successful deviated point by reprising after 1 million attempts; terminating to avoid infinite loop." << EidosTerminate();
						
						if ((a[0] < bx0) || (a[0] > bx1))
							goto reprise_1;
						
						// within the spatial bounds, so now we have to check the map
						double a_normalized[1];
						
						a_normalized[0] = (a[0] - bx0) / bounds_size_x;
						
						double value_for_point = map->ValueAtPoint_S1(a_normalized);
						
						if (value_for_point <= 0)
						{
							// habitability 0: always reprise
							goto reprise_1;
						}
						else if (value_for_point >= 1)
						{
							// habitability 1: never reprise (drop through)
						}
						else
						{
							// intermediate: do a random number draw, where value_for_point is P(within bounds)
							if (Eidos_rng_uniform(rng) > value_for_point)
								goto reprise_1;
						}
						
						break;
					}
					case BoundaryCondition::kAbsorbing:
					{
						if ((a[0] < bx0) || (a[0] > bx1))
						{
							result->push_object_element_capcheck_NORR(ind);
						}
						else
						{
							// within the spatial bounds, so now we have to check the map
							double a_normalized[1];
							
							a_normalized[0] = (a[0] - bx0) / bounds_size_x;
							
							double value_for_point = map->ValueAtPoint_S1(a_normalized);
							
							if (value_for_point <= 0)
							{
								// habitability 0: always reprise
								result->push_object_element_capcheck_NORR(ind);
							}
							else if (value_for_point >= 1)
							{
								// habitability 1: never reprise (drop through)
							}
							else
							{
								// intermediate: do a random number draw, where value_for_point is P(within bounds)
								if (Eidos_rng_uniform(rng) > value_for_point)
									result->push_object_element_capcheck_NORR(ind);
							}
						}
						
						break;
					}
					case BoundaryCondition::kNone:
					case BoundaryCondition::kStopping:
					case BoundaryCondition::kReflecting:
					case BoundaryCondition::kPeriodic:
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositionsWithMap): (internal error) unsupported boundary condition." << EidosTerminate();
				}
				
				ind->spatial_x_ = a[0];
			}
			break;
		}
		case 2:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_;
			double by0 = bounds_y0_, by1 = bounds_y1_;
			double bounds_size_x = bx1 - bx0;
			double bounds_size_y = by1 - by0;
			
			// FIXME: TO BE PARALLELIZED
			for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
			{
				SpatialKernel kernel((kernel_count == 1) ? kernel0 : SpatialKernel(dimensionality, max_distance, p_arguments, 4, individual_index, /* p_expect_max_density */ false, k_type, k_param_count));
				Individual *ind = individuals[individual_index];
				double a[2];
				int num_tries = 0;
				
			reprise_2:
				kernel.DrawDisplacement_S2(a);
				a[0] += ind->spatial_x_;
				a[1] += ind->spatial_y_;
				
				if (periodic_x)
				{
					while (a[0] < 0.0)	a[0] += bx1;
					while (a[0] > bx1)	a[0] -= bx1;
				}
				if (periodic_y)
				{
					while (a[1] < 0.0)	a[1] += by1;
					while (a[1] > by1)	a[1] -= by1;
				}
				
				// enforce the boundary condition
				switch (boundary)
				{
					case BoundaryCondition::kReprising:
					{
						if (++num_tries == 1000000)
							EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() failed to find a successful deviated point by reprising after 1 million attempts; terminating to avoid infinite loop." << EidosTerminate();
						
						if ((a[0] < bx0) || (a[0] > bx1) ||
							(a[1] < by0) || (a[1] > by1))
							goto reprise_2;
						
						// within the spatial bounds, so now we have to check the map
						double a_normalized[2];
						
						a_normalized[0] = (a[0] - bx0) / bounds_size_x;
						a_normalized[1] = (a[1] - by0) / bounds_size_y;
						
						double value_for_point = map->ValueAtPoint_S2(a_normalized);
						
						if (value_for_point <= 0)
						{
							// habitability 0: always reprise
							goto reprise_2;
						}
						else if (value_for_point >= 1)
						{
							// habitability 1: never reprise (drop through)
						}
						else
						{
							// intermediate: do a random number draw, where value_for_point is P(within bounds)
							if (Eidos_rng_uniform(rng) > value_for_point)
								goto reprise_2;
						}
						
						break;
					}
					case BoundaryCondition::kAbsorbing:
					{
						if ((a[0] < bx0) || (a[0] > bx1) ||
							(a[1] < by0) || (a[1] > by1))
						{
							result->push_object_element_capcheck_NORR(ind);
						}
						else
						{
							// within the spatial bounds, so now we have to check the map
							double a_normalized[2];
							
							a_normalized[0] = (a[0] - bx0) / bounds_size_x;
							a_normalized[1] = (a[1] - by0) / bounds_size_y;
							
							double value_for_point = map->ValueAtPoint_S2(a_normalized);
							
							if (value_for_point <= 0)
							{
								// habitability 0: always reprise
								result->push_object_element_capcheck_NORR(ind);
							}
							else if (value_for_point >= 1)
							{
								// habitability 1: never reprise (drop through)
							}
							else
							{
								// intermediate: do a random number draw, where value_for_point is P(within bounds)
								if (Eidos_rng_uniform(rng) > value_for_point)
									result->push_object_element_capcheck_NORR(ind);
							}
						}
						
						break;
					}
					case BoundaryCondition::kNone:
					case BoundaryCondition::kStopping:
					case BoundaryCondition::kReflecting:
					case BoundaryCondition::kPeriodic:
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositionsWithMap): (internal error) unsupported boundary condition." << EidosTerminate();
				}
				
				ind->spatial_x_ = a[0];
				ind->spatial_y_ = a[1];
			}
			break;
		}
		case 3:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_;
			double by0 = bounds_y0_, by1 = bounds_y1_;
			double bz0 = bounds_z0_, bz1 = bounds_z1_;
			double bounds_size_x = bx1 - bx0;
			double bounds_size_y = by1 - by0;
			double bounds_size_z = bz1 - bz0;
			
			// FIXME: TO BE PARALLELIZED
			for (int individual_index = 0; individual_index < individuals_count; ++individual_index)
			{
				SpatialKernel kernel((kernel_count == 1) ? kernel0 : SpatialKernel(dimensionality, max_distance, p_arguments, 4, individual_index, /* p_expect_max_density */ false, k_type, k_param_count));
				Individual *ind = individuals[individual_index];
				double a[3];
				int num_tries = 0;
				
			reprise_3:
				kernel.DrawDisplacement_S3(a);
				a[0] += ind->spatial_x_;
				a[1] += ind->spatial_y_;
				a[2] += ind->spatial_z_;
				
				if (periodic_x)
				{
					while (a[0] < 0.0)	a[0] += bx1;
					while (a[0] > bx1)	a[0] -= bx1;
				}
				if (periodic_y)
				{
					while (a[1] < 0.0)	a[1] += by1;
					while (a[1] > by1)	a[1] -= by1;
				}
				if (periodic_z)
				{
					while (a[2] < 0.0)	a[2] += bz1;
					while (a[2] > bz1)	a[2] -= bz1;
				}
				
				// enforce the boundary condition
				switch (boundary)
				{
					case BoundaryCondition::kReprising:
					{
						if (++num_tries == 1000000)
							EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_deviatePositionsWithMap): deviatePositionsWithMap() failed to find a successful deviated point by reprising after 1 million attempts; terminating to avoid infinite loop." << EidosTerminate();
						
						if ((a[0] < bx0) || (a[0] > bx1) ||
							(a[1] < by0) || (a[1] > by1) ||
							(a[2] < bz0) || (a[2] > bz1))
							goto reprise_3;
						
						// within the spatial bounds, so now we have to check the map
						double a_normalized[3];
						
						a_normalized[0] = (a[0] - bx0) / bounds_size_x;
						a_normalized[1] = (a[1] - by0) / bounds_size_y;
						a_normalized[2] = (a[2] - bz0) / bounds_size_z;
						
						double value_for_point = map->ValueAtPoint_S3(a_normalized);
						
						if (value_for_point <= 0)
						{
							// habitability 0: always reprise
							goto reprise_3;
						}
						else if (value_for_point >= 1)
						{
							// habitability 1: never reprise (drop through)
						}
						else
						{
							// intermediate: do a random number draw, where value_for_point is P(within bounds)
							if (Eidos_rng_uniform(rng) > value_for_point)
								goto reprise_3;
						}
						
						break;
					}
					case BoundaryCondition::kAbsorbing:
					{
						if ((a[0] < bx0) || (a[0] > bx1) ||
							(a[1] < by0) || (a[1] > by1) ||
							(a[2] < bz0) || (a[2] > bz1))
						{
							result->push_object_element_capcheck_NORR(ind);
						}
						else
						{
							// within the spatial bounds, so now we have to check the map
							double a_normalized[3];
							
							a_normalized[0] = (a[0] - bx0) / bounds_size_x;
							a_normalized[1] = (a[1] - by0) / bounds_size_y;
							a_normalized[2] = (a[2] - bz0) / bounds_size_z;
							
							double value_for_point = map->ValueAtPoint_S3(a_normalized);
							
							if (value_for_point <= 0)
							{
								// habitability 0: always reprise
								result->push_object_element_capcheck_NORR(ind);
							}
							else if (value_for_point >= 1)
							{
								// habitability 1: never reprise (drop through)
							}
							else
							{
								// intermediate: do a random number draw, where value_for_point is P(within bounds)
								if (Eidos_rng_uniform(rng) > value_for_point)
									result->push_object_element_capcheck_NORR(ind);
							}
						}
						
						break;
					}
					case BoundaryCondition::kNone:
					case BoundaryCondition::kStopping:
					case BoundaryCondition::kReflecting:
					case BoundaryCondition::kPeriodic:
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositionsWithMap): (internal error) unsupported boundary condition." << EidosTerminate();
				}
				
				ind->spatial_x_ = a[0];
				ind->spatial_y_ = a[1];
				ind->spatial_z_ = a[2];
			}
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_deviatePositionsWithMap): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return result_SP;
}

//	*********************	– (float)pointDeviated(integer$ n, float point, string$ boundary, numeric$ maxDistance, string$ functionType, ...)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointDeviated(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	// NOTE: most of the code of this method is shared with deviatePositions()
	
	int dimensionality = species_.SpatialDimensionality();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): pointDeviated() cannot be called in non-spatial simulations." << EidosTerminate();
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t n = n_value->IntAtIndex_NOCAST(0, nullptr);
	
	if (n < 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): pointDeviated() requires n >= 0." << EidosTerminate();
	if (n == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	int64_t length_out = n * dimensionality;
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(length_out);
	double *float_result_data = float_result->data_mutable();
	double *float_result_ptr = float_result_data;
	
	EidosValue *point_value = p_arguments[1].get();
	int point_count = point_value->Count();
	const double *point_buf = point_value->FloatData();
	const double *point_buf_ptr = point_buf;
	
	if (point_count % dimensionality != 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): pointDeviated() requires the length of point to be a multiple of the model dimensionality (i.e., point should contain an integer number of complete points of the correct dimensionality)." << EidosTerminate();
	
	point_count /= dimensionality;		// convert from float elements to points
	
	if ((point_count != 1) && ((int64_t)point_count != n))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): pointDeviated() requires point to be contain either a single spatial point (to be deviated n times) or n spatial points (each to be deviated once)." << EidosTerminate();
	
	EidosValue_String *boundary_value = (EidosValue_String *)p_arguments[2].get();
	const std::string &boundary_str = boundary_value->StringRefAtIndex_NOCAST(0, nullptr);
	BoundaryCondition boundary;
	
	if (boundary_str.compare("none") == 0)
		boundary = BoundaryCondition::kNone;
	else if (boundary_str.compare("stopping") == 0)
		boundary = BoundaryCondition::kStopping;
	else if (boundary_str.compare("reflecting") == 0)
		boundary = BoundaryCondition::kReflecting;
	else if (boundary_str.compare("reprising") == 0)
		boundary = BoundaryCondition::kReprising;
	else if (boundary_str.compare("periodic") == 0)
		boundary = BoundaryCondition::kPeriodic;
	else if (boundary_str.compare("absorbing") == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): pointDeviated() does not support boundary condition 'absorbing', but see Subpopulation method deviatePositions()." << EidosTerminate();
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): unrecognized boundary condition '" << boundary_str << "'." << EidosTerminate();
	
	// Periodic boundaries are a bit complicated.  If only some dimensions are periodic, 'none' will be used
	// for the non-periodic boundaries, and the user can then use pointReflected(), pointStopped(), etc. to
	// enforce a boundary condition on those dimensions.
	bool periodic_x = false, periodic_y = false, periodic_z = false;
	
	if (boundary == BoundaryCondition::kPeriodic)
	{
		species_.SpatialPeriodicity(&periodic_x, &periodic_y, &periodic_z);
		
		if (!periodic_x && !periodic_y && !periodic_z)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): pointDeviated() cannot apply periodic boundary conditions in a model without periodic boundaries." << EidosTerminate();
	}
	
	EidosValue *maxDistance_value = p_arguments[3].get();
	double max_distance = maxDistance_value->NumericAtIndex_NOCAST(0, nullptr);
	
	SpatialKernelType k_type;
	int k_param_count;
	int kernel_count = SpatialKernel::PreprocessArguments(dimensionality, max_distance, p_arguments, 4, /* p_expect_max_density */ false, &k_type, &k_param_count);
	
	if ((kernel_count != 1) && (kernel_count != point_count))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): pointDeviated() requires that the number of spatial kernels defined (by the supplied kernel-definition arguments) either must be 1, or must equal the number of points being processed (" << kernel_count << " kernels defined; " << point_count << " individuals processed)." << EidosTerminate();
	
	SpatialKernel kernel0(dimensionality, max_distance, p_arguments, 4, 0, /* p_expect_max_density */ false, k_type, k_param_count);	// uses our arguments starting at index 4
	
	// I'm not going to worry about unrolling each case, for dimensionality by boundary by kernel type; it would
	// be a ton of cases (3 x 5 x 5 = 75), and the overhead for the switches ought to be small compared to the
	// overhead of drawing a displacement from the kernel, which requires a random number draw.  I tested doing
	// a special-case here for dimensionality==2, boundary==1 (stopping), kernel.kernel_type==kNormal,
	// maxDistance==INF, and it clocked at 6.47 seconds versus 7.85 seconds for the unoptimized code below;
	// that's about a 17.6% speedup, which is worthwhile for a handful of special cases like that.  I think
	// normal deviations in 2D with an INF maxDistance are the 95% case, if not 99%; several boundary conditions
	// are common, though.
	if ((kernel_count == 1) && (dimensionality == 2) && (kernel0.kernel_type_ == SpatialKernelType::kNormal) && std::isinf(kernel0.max_distance_) && ((boundary == BoundaryCondition::kStopping) || (boundary == BoundaryCondition::kReflecting) || (boundary == BoundaryCondition::kReprising) || ((boundary == BoundaryCondition::kPeriodic) && periodic_x && periodic_y)))
	{
		gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
		double stddev = kernel0.kernel_param2_;
		double bx0 = bounds_x0_, bx1 = bounds_x1_;
		double by0 = bounds_y0_, by1 = bounds_y1_;
		
		if (boundary == BoundaryCondition::kStopping)
		{
			for (int result_index = 0; result_index < n; ++result_index)
			{
				double a0 = *(point_buf_ptr++) + gsl_ran_gaussian(rng, stddev);
				double a1 = *(point_buf_ptr++) + gsl_ran_gaussian(rng, stddev);
				
				a0 = std::max(bx0, std::min(bx1, a0));
				a1 = std::max(by0, std::min(by1, a1));
				
				*(float_result_ptr++) = a0;
				*(float_result_ptr++) = a1;
			}
		}
		else if (boundary == BoundaryCondition::kReflecting)
		{
			for (int result_index = 0; result_index < n; ++result_index)
			{
				double a0 = *(point_buf_ptr++) + gsl_ran_gaussian(rng, stddev);
				double a1 = *(point_buf_ptr++) + gsl_ran_gaussian(rng, stddev);
				
				while (true)
				{
					if (a0 < bx0) a0 = bx0 + (bx0 - a0);
					else if (a0 > bx1) a0 = bx1 - (a0 - bx1);
					else break;
				}
				while (true)
				{
					if (a1 < by0) a1 = by0 + (by0 - a1);
					else if (a1 > by1) a1 = by1 - (a1 - by1);
					else break;
				}
				
				*(float_result_ptr++) = a0;
				*(float_result_ptr++) = a1;
			}
		}
		else if (boundary == BoundaryCondition::kReprising)
		{
			for (int result_index = 0; result_index < n; ++result_index)
			{
				double a0_original = *(point_buf_ptr++);
				double a1_original = *(point_buf_ptr++);
				
			reprise_specialcase:
				double a0 = a0_original + gsl_ran_gaussian(rng, stddev);
				double a1 = a1_original + gsl_ran_gaussian(rng, stddev);
				
				if ((a0 < bx0) || (a0 > bx1) ||
					(a1 < by0) || (a1 > by1))
					goto reprise_specialcase;
				
				*(float_result_ptr++) = a0;
				*(float_result_ptr++) = a1;
			}
		}
		else if (boundary == BoundaryCondition::kPeriodic)
		{
			for (int result_index = 0; result_index < n; ++result_index)
			{
				double a0 = *(point_buf_ptr++) + gsl_ran_gaussian(rng, stddev);
				double a1 = *(point_buf_ptr++) + gsl_ran_gaussian(rng, stddev);
				
				// (note periodic_x and periodic_y are required to be true above)
				while (a0 < 0.0)	a0 += bx1;
				while (a0 > bx1)	a0 -= bx1;
				while (a1 < 0.0)	a1 += by1;
				while (a1 > by1)	a1 -= by1;
				
				*(float_result_ptr++) = a0;
				*(float_result_ptr++) = a1;
			}
		}
		return EidosValue_SP(float_result);
	}
	
	switch (dimensionality)
	{
		case 1:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_;
			int point_buf_ptr_inc = (point_count > 1) ? 1 : 0;		// move to the next point, unless we're repeatedly processing a single point
			
			for (int result_index = 0; result_index < n; ++result_index)
			{
				double a[1];
				SpatialKernel kernel((kernel_count == 1) ? kernel0 : SpatialKernel(dimensionality, max_distance, p_arguments, 4, result_index, /* p_expect_max_density */ false, k_type, k_param_count));
				
			reprise_1:
				kernel.DrawDisplacement_S1(a);
				a[0] += point_buf_ptr[0];
				
				// enforce the boundary condition
				switch (boundary)
				{
					case BoundaryCondition::kNone:
						break;
					case BoundaryCondition::kStopping:
						a[0] = std::max(bx0, std::min(bx1, a[0]));
						break;
					case BoundaryCondition::kReflecting:
						while (true)
						{
							if (a[0] < bx0) a[0] = bx0 + (bx0 - a[0]);
							else if (a[0] > bx1) a[0] = bx1 - (a[0] - bx1);
							else break;
						}
						break;
					case BoundaryCondition::kReprising:
						if ((a[0] < bx0) || (a[0] > bx1))
							goto reprise_1;
						break;
					case BoundaryCondition::kPeriodic:			// (periodic_x must be true)
						while (a[0] < 0.0)	a[0] += bx1;
						while (a[0] > bx1)	a[0] -= bx1;
						break;
					case BoundaryCondition::kAbsorbing:			// ruled out above
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): (internal error) absorbing boundaries not implemented." << EidosTerminate();
				}
				
				*(float_result_ptr++) = a[0];
				point_buf_ptr += point_buf_ptr_inc;
			}
			break;
		}
		case 2:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_;
			double by0 = bounds_y0_, by1 = bounds_y1_;
			int point_buf_ptr_inc = (point_count > 1) ? 2 : 0;		// move to the next point, unless we're repeatedly processing a single point
			
			for (int result_index = 0; result_index < n; ++result_index)
			{
				SpatialKernel kernel((kernel_count == 1) ? kernel0 : SpatialKernel(dimensionality, max_distance, p_arguments, 4, result_index, /* p_expect_max_density */ false, k_type, k_param_count));
				double a[2];
				
			reprise_2:
				kernel.DrawDisplacement_S2(a);
				a[0] += point_buf_ptr[0];
				a[1] += point_buf_ptr[1];
				
				// enforce the boundary condition
				switch (boundary)
				{
					case BoundaryCondition::kNone:
						break;
					case BoundaryCondition::kStopping:
						a[0] = std::max(bx0, std::min(bx1, a[0]));
						a[1] = std::max(by0, std::min(by1, a[1]));
						break;
					case BoundaryCondition::kReflecting:
						while (true)
						{
							if (a[0] < bx0) a[0] = bx0 + (bx0 - a[0]);
							else if (a[0] > bx1) a[0] = bx1 - (a[0] - bx1);
							else break;
						}
						while (true)
						{
							if (a[1] < by0) a[1] = by0 + (by0 - a[1]);
							else if (a[1] > by1) a[1] = by1 - (a[1] - by1);
							else break;
						}
						break;
					case BoundaryCondition::kReprising:
						if ((a[0] < bx0) || (a[0] > bx1) ||
							(a[1] < by0) || (a[1] > by1))
							goto reprise_2;
						break;
					case BoundaryCondition::kPeriodic:
						if (periodic_x)
						{
							while (a[0] < 0.0)	a[0] += bx1;
							while (a[0] > bx1)	a[0] -= bx1;
						}
						if (periodic_y)
						{
							while (a[1] < 0.0)	a[1] += by1;
							while (a[1] > by1)	a[1] -= by1;
						}
						break;
					case BoundaryCondition::kAbsorbing:			// ruled out above
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): (internal error) absorbing boundaries not implemented." << EidosTerminate();
				}
				
				*(float_result_ptr++) = a[0];
				*(float_result_ptr++) = a[1];
				point_buf_ptr += point_buf_ptr_inc;
			}
			break;
		}
		case 3:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_;
			double by0 = bounds_y0_, by1 = bounds_y1_;
			double bz0 = bounds_z0_, bz1 = bounds_z1_;
			int point_buf_ptr_inc = (point_count > 1) ? 3 : 0;		// move to the next point, unless we're repeatedly processing a single point
			
			for (int result_index = 0; result_index < n; ++result_index)
			{
				SpatialKernel kernel((kernel_count == 1) ? kernel0 : SpatialKernel(dimensionality, max_distance, p_arguments, 4, result_index, /* p_expect_max_density */ false, k_type, k_param_count));
				double a[3];
				
			reprise_3:
				kernel.DrawDisplacement_S3(a);
				a[0] += point_buf_ptr[0];
				a[1] += point_buf_ptr[1];
				a[2] += point_buf_ptr[2];
				
				// enforce the boundary condition
				switch (boundary)
				{
					case BoundaryCondition::kNone:
						break;
					case BoundaryCondition::kStopping:
						a[0] = std::max(bx0, std::min(bx1, a[0]));
						a[1] = std::max(by0, std::min(by1, a[1]));
						a[2] = std::max(bz0, std::min(bz1, a[2]));
						break;
					case BoundaryCondition::kReflecting:
						while (true)
						{
							if (a[0] < bx0) a[0] = bx0 + (bx0 - a[0]);
							else if (a[0] > bx1) a[0] = bx1 - (a[0] - bx1);
							else break;
						}
						while (true)
						{
							if (a[1] < by0) a[1] = by0 + (by0 - a[1]);
							else if (a[1] > by1) a[1] = by1 - (a[1] - by1);
							else break;
						}
						while (true)
						{
							if (a[2] < bz0) a[2] = bz0 + (bz0 - a[2]);
							else if (a[2] > bz1) a[2] = bz1 - (a[2] - bz1);
							else break;
						}
						break;
					case BoundaryCondition::kReprising:
						if ((a[0] < bx0) || (a[0] > bx1) ||
							(a[1] < by0) || (a[1] > by1) ||
							(a[2] < bz0) || (a[2] > bz1))
							goto reprise_3;
						break;
					case BoundaryCondition::kPeriodic:
						if (periodic_x)
						{
							while (a[0] < 0.0)	a[0] += bx1;
							while (a[0] > bx1)	a[0] -= bx1;
						}
						if (periodic_y)
						{
							while (a[1] < 0.0)	a[1] += by1;
							while (a[1] > by1)	a[1] -= by1;
						}
						if (periodic_z)
						{
							while (a[2] < 0.0)	a[2] += bz1;
							while (a[2] > bz1)	a[2] -= bz1;
						}
						break;
					case BoundaryCondition::kAbsorbing:			// ruled out above
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): (internal error) absorbing boundaries not implemented." << EidosTerminate();
				}
				
				*(float_result_ptr++) = a[0];
				*(float_result_ptr++) = a[1];
				*(float_result_ptr++) = a[2];
				point_buf_ptr += point_buf_ptr_inc;
			}
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return EidosValue_SP(float_result);
}

//	*********************	– (logical)pointInBounds(float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointInBounds(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *point_value = p_arguments[0].get();
	
	int dimensionality = species_.SpatialDimensionality();
	int value_count = point_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointInBounds): pointInBounds() cannot be called in non-spatial simulations." << EidosTerminate();
	if (value_count == 0)
		return gStaticEidosValue_Logical_ZeroVec;
	
	int point_count = value_count / dimensionality;
	
	if (value_count != point_count * dimensionality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointInBounds): pointInBounds() requires the length of point to be an exact multiple of the spatial dimensionality of the simulation (i.e., point must contain zero or more complete points)." << EidosTerminate();
	
	const double *point_buf = point_value->FloatData();
	
	if (point_count == 1)
	{
		// single-point case, do it separately to return a singleton logical value
		switch (dimensionality)
		{
			case 1:
			{
				double x = point_buf[0];
				return ((x >= bounds_x0_) && (x <= bounds_x1_))
					? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			case 2:
			{
				double x = point_buf[0];
				double y = point_buf[1];
				return ((x >= bounds_x0_) && (x <= bounds_x1_) && (y >= bounds_y0_) && (y <= bounds_y1_))
					? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			case 3:
			{
				double x = point_buf[0];
				double y = point_buf[1];
				double z = point_buf[2];
				return ((x >= bounds_x0_) && (x <= bounds_x1_) && (y >= bounds_y0_) && (y <= bounds_y1_) && (z >= bounds_z0_) && (z <= bounds_z1_))
					? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
			}
			default:
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointInBounds): (internal error) unrecognized dimensionality." << EidosTerminate();
		}
	}
	
	// multiple-point case, new in SLiM 3
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(point_count);
	eidos_logical_t *logical_result_data = logical_result->data_mutable();
	
	switch (dimensionality)
	{
		case 1:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_IN_BOUNDS_1D);
#pragma omp parallel for schedule(static) default(none) shared(point_count) firstprivate(point_buf, logical_result_data, bx0, bx1) if(point_count >= EIDOS_OMPMIN_POINT_IN_BOUNDS_1D) num_threads(thread_count)
			for (int point_index = 0; point_index < point_count; ++point_index)
			{
				double x = point_buf[point_index];
				eidos_logical_t in_bounds = ((x >= bx0) && (x <= bx1));
				
				logical_result_data[point_index] = in_bounds;
			}
			break;
		}
		case 2:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_, by0 = bounds_y0_, by1 = bounds_y1_;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_IN_BOUNDS_2D);
#pragma omp parallel for schedule(static) default(none) shared(point_count) firstprivate(point_buf, logical_result_data, bx0, bx1, by0, by1) if(point_count >= EIDOS_OMPMIN_POINT_IN_BOUNDS_2D) num_threads(thread_count)
			for (int point_index = 0; point_index < point_count; ++point_index)
			{
				double x = point_buf[(size_t)point_index * 2];
				double y = point_buf[(size_t)point_index * 2 + 1];
				eidos_logical_t in_bounds = ((x >= bx0) && (x <= bx1) && (y >= by0) && (y <= by1));
				
				logical_result_data[point_index] = in_bounds;
			}
			break;
		}
		case 3:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_, by0 = bounds_y0_, by1 = bounds_y1_, bz0 = bounds_z0_, bz1 = bounds_z1_;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_IN_BOUNDS_3D);
#pragma omp parallel for schedule(static) default(none) shared(point_count) firstprivate(point_buf, logical_result_data, bx0, bx1, by0, by1, bz0, bz1) if(point_count >= EIDOS_OMPMIN_POINT_IN_BOUNDS_3D) num_threads(thread_count)
			for (int point_index = 0; point_index < point_count; ++point_index)
			{
				double x = point_buf[(size_t)point_index * 3];
				double y = point_buf[(size_t)point_index * 3 + 1];
				double z = point_buf[(size_t)point_index * 3 + 2];
				eidos_logical_t in_bounds = ((x >= bx0) && (x <= bx1) && (y >= by0) && (y <= by1) && (z >= bz0) && (z <= bz1));
				
				logical_result_data[point_index] = in_bounds;
			}
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointInBounds): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return EidosValue_SP(logical_result);
}			

//	*********************	– (float)pointReflected(float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointReflected(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *point_value = p_arguments[0].get();
	
	int dimensionality = species_.SpatialDimensionality();
	int value_count = point_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointReflected): pointReflected() cannot be called in non-spatial simulations." << EidosTerminate();
	if (value_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	int point_count = value_count / dimensionality;
	
	if (value_count != point_count * dimensionality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointReflected): pointReflected() requires the length of point to be an exact multiple of the spatial dimensionality of the simulation (i.e., point must contain zero or more complete points)." << EidosTerminate();
	
	const double *point_buf = point_value->FloatData();
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(value_count);
	double *float_result_data = float_result->data_mutable();
	
	switch (dimensionality)
	{
		case 1:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_REFLECTED_1D);
#pragma omp parallel for schedule(static) default(none) shared(point_count) firstprivate(point_buf, float_result_data, bx0, bx1) if(point_count >= EIDOS_OMPMIN_POINT_REFLECTED_1D) num_threads(thread_count)
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = point_buf[point_index];
				while (true)
				{
					if (x < bx0) x = bx0 + (bx0 - x);
					else if (x > bx1) x = bx1 - (x - bx1);
					else break;
				}
				float_result_data[point_index] = x;
			}
			break;
		}
		case 2:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_, by0 = bounds_y0_, by1 = bounds_y1_;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_REFLECTED_2D);
#pragma omp parallel for schedule(static) default(none) shared(point_count) firstprivate(point_buf, float_result_data, bx0, bx1, by0, by1) if(point_count >= EIDOS_OMPMIN_POINT_REFLECTED_2D) num_threads(thread_count)
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = point_buf[point_index * 2];
				while (true)
				{
					if (x < bx0) x = bx0 + (bx0 - x);
					else if (x > bx1) x = bx1 - (x - bx1);
					else break;
				}
				float_result_data[point_index * 2] = x;
				
				double y = point_buf[point_index * 2 + 1];
				while (true)
				{
					if (y < by0) y = by0 + (by0 - y);
					else if (y > by1) y = by1 - (y - by1);
					else break;
				}
				float_result_data[point_index * 2 + 1] = y;
			}
			break;
		}
		case 3:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_, by0 = bounds_y0_, by1 = bounds_y1_, bz0 = bounds_z0_, bz1 = bounds_z1_;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_REFLECTED_3D);
#pragma omp parallel for schedule(static) default(none) shared(point_count) firstprivate(point_buf, float_result_data, bx0, bx1, by0, by1, bz0, bz1) if(point_count >= EIDOS_OMPMIN_POINT_REFLECTED_3D) num_threads(thread_count)
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = point_buf[point_index * 3];
				while (true)
				{
					if (x < bx0) x = bx0 + (bx0 - x);
					else if (x > bx1) x = bx1 - (x - bx1);
					else break;
				}
				float_result_data[point_index * 3] = x;
				
				double y = point_buf[point_index * 3 + 1];
				while (true)
				{
					if (y < by0) y = by0 + (by0 - y);
					else if (y > by1) y = by1 - (y - by1);
					else break;
				}
				float_result_data[point_index * 3 + 1] = y;
				
				double z = point_buf[point_index * 3 + 2];
				while (true)
				{
					if (z < bz0) z = bz0 + (bz0 - z);
					else if (z > bz1) z = bz1 - (z - bz1);
					else break;
				}
				float_result_data[point_index * 3 + 2] = z;
			}
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointReflected): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return EidosValue_SP(float_result);
}			

//	*********************	– (float)pointStopped(float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointStopped(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *point_value = p_arguments[0].get();
	
	int dimensionality = species_.SpatialDimensionality();
	int value_count = point_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointStopped): pointStopped() cannot be called in non-spatial simulations." << EidosTerminate();
	if (value_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	int point_count = value_count / dimensionality;
	
	if (value_count != point_count * dimensionality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointStopped): pointStopped() requires the length of point to be an exact multiple of the spatial dimensionality of the simulation (i.e., point must contain zero or more complete points)." << EidosTerminate();
	
	const double *point_buf = point_value->FloatData();
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(value_count);
	double *float_result_data = float_result->data_mutable();
	
	switch (dimensionality)
	{
		case 1:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_STOPPED_1D);
#pragma omp parallel for schedule(static) default(none) shared(point_count) firstprivate(point_buf, float_result_data, bx0, bx1) if(point_count >= EIDOS_OMPMIN_POINT_STOPPED_1D) num_threads(thread_count)
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = point_buf[point_index];
				float_result_data[point_index] = std::max(bx0, std::min(bx1, x));
			}
			break;
		}
		case 2:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_, by0 = bounds_y0_, by1 = bounds_y1_;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_STOPPED_2D);
#pragma omp parallel for schedule(static) default(none) shared(point_count) firstprivate(point_buf, float_result_data, bx0, bx1, by0, by1) if(point_count >= EIDOS_OMPMIN_POINT_STOPPED_2D) num_threads(thread_count)
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = point_buf[point_index * 2];
				float_result_data[point_index * 2] = std::max(bx0, std::min(bx1, x));
				
				double y = point_buf[point_index * 2 + 1];
				float_result_data[point_index * 2 + 1] = std::max(by0, std::min(by1, y));
			}
			break;
		}
		case 3:
		{
			double bx0 = bounds_x0_, bx1 = bounds_x1_, by0 = bounds_y0_, by1 = bounds_y1_, bz0 = bounds_z0_, bz1 = bounds_z1_;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_STOPPED_3D);
#pragma omp parallel for schedule(static) default(none) shared(point_count) firstprivate(point_buf, float_result_data, bx0, bx1, by0, by1, bz0, bz1) if(point_count >= EIDOS_OMPMIN_POINT_STOPPED_3D) num_threads(thread_count)
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = point_buf[point_index * 3];
				float_result_data[point_index * 3] = std::max(bx0, std::min(bx1, x));
				
				double y = point_buf[point_index * 3 + 1];
				float_result_data[point_index * 3 + 1] = std::max(by0, std::min(by1, y));
				
				double z = point_buf[point_index * 3 + 2];
				float_result_data[point_index * 3 + 2] = std::max(bz0, std::min(bz1, z));
			}
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointStopped): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return EidosValue_SP(float_result);
}			

//	*********************	– (float)pointPeriodic(float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointPeriodic(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *point_value = p_arguments[0].get();
	
	int dimensionality = species_.SpatialDimensionality();
	int value_count = point_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointPeriodic): pointPeriodic() cannot be called in non-spatial simulations." << EidosTerminate();
	
	bool periodic_x, periodic_y, periodic_z;
	
	species_.SpatialPeriodicity(&periodic_x, &periodic_y, &periodic_z);
	
	if (!periodic_x && !periodic_y && !periodic_z)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointPeriodic): pointPeriodic() cannot be called when no periodic spatial dimension has been set up." << EidosTerminate();
	
	if (value_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	int point_count = value_count / dimensionality;
	
	if (value_count != point_count * dimensionality)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointPeriodic): pointPeriodic() requires the length of point to be an exact multiple of the spatial dimensionality of the simulation (i.e., point must contain zero or more complete points)." << EidosTerminate();
	
	const double *point_buf = point_value->FloatData();
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(value_count);
	double *float_result_data = float_result->data_mutable();
	
	// Wrap coordinates; note that we assume here that bounds_x0_ == bounds_y0_ == bounds_z0_ == 0,
	// which is enforced when periodic boundary conditions are set, in setSpatialBounds().  Note also
	// that we don't use fmod(); maybe we should, rather than looping, but then we have to worry about
	// sign, and since new spatial points are probably usually close to being in bounds, these loops
	// probably won't execute more than once anyway...
	switch (dimensionality)
	{
		case 1:
		{
			double bx1 = bounds_x1_;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_PERIODIC_1D);
#pragma omp parallel for schedule(static) default(none) shared(point_count) firstprivate(point_buf, float_result_data, bx1, periodic_x) if(point_count >= EIDOS_OMPMIN_POINT_PERIODIC_1D) num_threads(thread_count)
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = point_buf[point_index];
				if (periodic_x)
				{
					while (x < 0.0)			x += bx1;
					while (x > bx1)			x -= bx1;
				}
				float_result_data[point_index] = x;
			}
			break;
		}
		case 2:
		{
			double bx1 = bounds_x1_, by1 = bounds_y1_;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_PERIODIC_2D);
#pragma omp parallel for schedule(static) default(none) shared(point_count) firstprivate(point_buf, float_result_data, bx1, by1, periodic_x, periodic_y) if(point_count >= EIDOS_OMPMIN_POINT_PERIODIC_2D) num_threads(thread_count)
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = point_buf[point_index * 2];
				if (periodic_x)
				{
					while (x < 0.0)			x += bx1;
					while (x > bx1)			x -= bx1;
				}
				float_result_data[point_index * 2] = x;
				
				double y = point_buf[point_index * 2 + 1];
				if (periodic_y)
				{
					while (y < 0.0)			y += by1;
					while (y > by1)			y -= by1;
				}
				float_result_data[point_index * 2 + 1] = y;
			}
			break;
		}
		case 3:
		{
			double bx1 = bounds_x1_, by1 = bounds_y1_, bz1 = bounds_z1_;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_PERIODIC_3D);
#pragma omp parallel for schedule(static) default(none) shared(point_count) firstprivate(point_buf, float_result_data, bx1, by1, bz1, periodic_x, periodic_y, periodic_z) if(point_count >= EIDOS_OMPMIN_POINT_PERIODIC_3D) num_threads(thread_count)
			for (int64_t point_index = 0; point_index < point_count; ++point_index)
			{
				double x = point_buf[point_index * 3];
				if (periodic_x)
				{
					while (x < 0.0)			x += bx1;
					while (x > bx1)			x -= bx1;
				}
				float_result_data[point_index * 3] = x;
				
				double y = point_buf[point_index * 3 + 1];
				if (periodic_y)
				{
					while (y < 0.0)			y += by1;
					while (y > by1)			y -= by1;
				}
				float_result_data[point_index * 3 + 1] = y;
				
				double z = point_buf[point_index * 3 + 2];
				if (periodic_z)
				{
					while (z < 0.0)			z += bz1;
					while (z > bz1)			z -= bz1;
				}
				float_result_data[point_index * 3 + 2] = z;
			}
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointPeriodic): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return EidosValue_SP(float_result);
}			

//	*********************	– (float)pointUniform([integer$ n = 1])
//
EidosValue_SP Subpopulation::ExecuteMethod_pointUniform(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	int dimensionality = species_.SpatialDimensionality();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniform): pointUniform() cannot be called in non-spatial simulations." << EidosTerminate();
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t point_count = n_value->IntAtIndex_NOCAST(0, nullptr);
	
	if (point_count < 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniform): pointUniform() requires n >= 0." << EidosTerminate();
	if (point_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	int64_t length_out = point_count * dimensionality;
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(length_out);
	double *float_result_data = float_result->data_mutable();
	EidosValue_SP result_SP = EidosValue_SP(float_result);
	
	switch (dimensionality)
	{
		case 1:
		{
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_UNIFORM_1D);
#pragma omp parallel default(none) shared(point_count, gEidos_RNG_PERTHREAD) firstprivate(float_result_data) if(point_count >= EIDOS_OMPMIN_POINT_UNIFORM_1D) num_threads(thread_count)
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				double xsize = bounds_x1_ - bounds_x0_, xbase = bounds_x0_;
				
#pragma omp for schedule(static)
				for (int64_t point_index = 0; point_index < point_count; ++point_index)
				{
					float_result_data[point_index] = Eidos_rng_uniform(rng) * xsize + xbase;
				}
			}
			break;
		}
		case 2:
		{
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_UNIFORM_2D);
#pragma omp parallel default(none) shared(point_count, gEidos_RNG_PERTHREAD) firstprivate(float_result_data) if(point_count >= EIDOS_OMPMIN_POINT_UNIFORM_2D) num_threads(thread_count)
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				double xsize = bounds_x1_ - bounds_x0_, xbase = bounds_x0_;
				double ysize = bounds_y1_ - bounds_y0_, ybase = bounds_y0_;
				
#pragma omp for schedule(static)
				for (int64_t point_index = 0; point_index < point_count; ++point_index)
				{
					float_result_data[point_index * 2] = Eidos_rng_uniform(rng) * xsize + xbase;
					float_result_data[point_index * 2 + 1] = Eidos_rng_uniform(rng) * ysize + ybase;
				}
			}
			break;
		}
		case 3:
		{
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_POINT_UNIFORM_3D);
#pragma omp parallel default(none) shared(point_count, gEidos_RNG_PERTHREAD) firstprivate(float_result_data) if(point_count >= EIDOS_OMPMIN_POINT_UNIFORM_3D) num_threads(thread_count)
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				double xsize = bounds_x1_ - bounds_x0_, xbase = bounds_x0_;
				double ysize = bounds_y1_ - bounds_y0_, ybase = bounds_y0_;
				double zsize = bounds_z1_ - bounds_z0_, zbase = bounds_z0_;
				
#pragma omp for schedule(static)
				for (int64_t point_index = 0; point_index < point_count; ++point_index)
				{
					float_result_data[point_index * 3] = Eidos_rng_uniform(rng) * xsize + xbase;
					float_result_data[point_index * 3 + 1] = Eidos_rng_uniform(rng) * ysize + ybase;
					float_result_data[point_index * 3 + 2] = Eidos_rng_uniform(rng) * zsize + zbase;
				}
			}
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniform): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return result_SP;
}

//	*********************	– (float)pointUniformWithMap(integer$ n, so<SpatialMap>$ map)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointUniformWithMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	int dimensionality = species_.SpatialDimensionality();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniformWithMap): pointUniformWithMap() cannot be called in non-spatial simulations." << EidosTerminate();
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t point_count = n_value->IntAtIndex_NOCAST(0, nullptr);
	
	if (point_count < 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniformWithMap): pointUniformWithMap() requires n >= 0." << EidosTerminate();
	if (point_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	// Get the spatial map's name; see ExecuteMethod_spatialMapValue() for the origin of this code
	EidosValue *map_value = p_arguments[1].get();
	SpatialMap *map = nullptr;
	std::string map_name;
	
	if (map_value->Type() == EidosValueType::kValueString)
	{
		map_name = ((EidosValue_String *)map_value)->StringRefAtIndex_NOCAST(0, nullptr);
		
		if (map_name.length() == 0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniformWithMap): pointUniformWithMap() map name must not be zero-length." << EidosTerminate();
	}
	else
	{
		map = (SpatialMap *)map_value->ObjectElementAtIndex_NOCAST(0, nullptr);
		map_name = map->name_;
	}
	
	// Find the SpatialMap by name; we do this lookup even if a map object was supplied, to check that that map is present
	auto map_iter = spatial_maps_.find(map_name);
	
	if (map_iter != spatial_maps_.end())
	{
		SpatialMap *found_map = map_iter->second;
		
		if (map && (found_map != map))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniformWithMap): pointUniformWithMap() could not find map in the target subpopulation (although it did find a different map with the same name)." << EidosTerminate();
		
		map = found_map;
	}
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniformWithMap): pointUniformWithMap() could not find map '" << map_name << "' in the target subpopulation." << EidosTerminate();
	
	// The map is required to have spatiality equal to the species dimensionality.  This is because the value
	// queries to the map using ValueAtPoint_S2() etc. are in the map's spatiality, and we don't want to have
	// to be translating points in and out of that spatiality.  Other methods don't have this problem because
	// either they get passed a point (which must be in the map or interaction type's spatiality) or they have
	// cached the point in the correct spatiality internally (as InteractionType does).  Users who have a map
	// in a weird spatiality can always do the work of this method themselves, with more basic calls.
	if (dimensionality != map->spatiality_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniformWithMap): pointUniformWithMap() requires that the map's spatiality is equal to the dimensionality of the species." << EidosTerminate();
	
	// Generate the points
	int64_t length_out = point_count * dimensionality;
	EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(length_out);
	double *float_result_data = float_result->data_mutable();
	EidosValue_SP result_SP = EidosValue_SP(float_result);
	
	// FIXME: PARALLELIZE
	switch (dimensionality)
	{
		case 1:
		{
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				double xsize = bounds_x1_ - bounds_x0_, xbase = bounds_x0_;
				
				for (int64_t point_index = 0; point_index < point_count; ++point_index)
				{
					double *point_base = &(float_result_data[point_index]);
					int num_tries = 0;
					
					do {
						if (++num_tries == 1000000)
							EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_pointUniformWithMap): pointUniformWithMap() failed to find a successful drawn point after 1 million attempts; terminating to avoid infinite loop." << EidosTerminate();
						
						// ValueAtPoint_S1() requires points normalized to [0, 1] in the map's spatiality
						point_base[0] = Eidos_rng_uniform(rng);
						double value_for_point = map->ValueAtPoint_S1(point_base);
						
						if (value_for_point <= 0)
							continue;
						else if (value_for_point >= 1)
							break;
						else if (Eidos_rng_uniform(rng) <= value_for_point)
							break;
					} while (true);
					
					point_base[0] = point_base[0] * xsize + xbase;
				}
			}
			break;
		}
		case 2:
		{
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				double xsize = bounds_x1_ - bounds_x0_, xbase = bounds_x0_;
				double ysize = bounds_y1_ - bounds_y0_, ybase = bounds_y0_;
				
				for (int64_t point_index = 0; point_index < point_count; ++point_index)
				{
					double *point_base = &(float_result_data[point_index * 2]);
					int num_tries = 0;
					
					do {
						if (++num_tries == 1000000)
							EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_pointUniformWithMap): pointUniformWithMap() failed to find a successful drawn point after 1 million attempts; terminating to avoid infinite loop." << EidosTerminate();
						
						// ValueAtPoint_S2() requires points normalized to [0, 1] in the map's spatiality
						point_base[0] = Eidos_rng_uniform(rng);
						point_base[1] = Eidos_rng_uniform(rng);
						double value_for_point = map->ValueAtPoint_S2(point_base);
						
						if (value_for_point <= 0)
							continue;
						else if (value_for_point >= 1)
							break;
						else if (Eidos_rng_uniform(rng) <= value_for_point)
							break;
					} while (true);
					
					point_base[0] = point_base[0] * xsize + xbase;
					point_base[1] = point_base[1] * ysize + ybase;
				}
			}
			break;
		}
		case 3:
		{
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				double xsize = bounds_x1_ - bounds_x0_, xbase = bounds_x0_;
				double ysize = bounds_y1_ - bounds_y0_, ybase = bounds_y0_;
				double zsize = bounds_z1_ - bounds_z0_, zbase = bounds_z0_;
				
				for (int64_t point_index = 0; point_index < point_count; ++point_index)
				{
					double *point_base = &(float_result_data[point_index * 3]);
					int num_tries = 0;
					
					do {
						if (++num_tries == 1000000)
							EIDOS_TERMINATION << "ERROR (SpatialMap::ExecuteMethod_pointUniformWithMap): pointUniformWithMap() failed to find a successful drawn point after 1 million attempts; terminating to avoid infinite loop." << EidosTerminate();
						
						// ValueAtPoint_S3() requires points normalized to [0, 1] in the map's spatiality
						point_base[0] = Eidos_rng_uniform(rng);
						point_base[1] = Eidos_rng_uniform(rng);
						point_base[2] = Eidos_rng_uniform(rng);
						double value_for_point = map->ValueAtPoint_S3(point_base);
						
						if (value_for_point <= 0)
							continue;
						else if (value_for_point >= 1)
							break;
						else if (Eidos_rng_uniform(rng) <= value_for_point)
							break;
					} while (true);
					
					point_base[0] = point_base[0] * xsize + xbase;
					point_base[1] = point_base[1] * ysize + ybase;
					point_base[2] = point_base[2] * zsize + zbase;
				}
			}
			break;
		}
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniformWithMap): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	return result_SP;
}			

// WF only:
//	*********************	- (void)setCloningRate(numeric rate)
//
EidosValue_SP Subpopulation::ExecuteMethod_setCloningRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeNonWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *rate_value = p_arguments[0].get();
	
	int value_count = rate_value->Count();
	
	if (sex_enabled_)
	{
		// SEX ONLY: either one or two values may be specified; if two, it is female at 0, male at 1
		if ((value_count < 1) || (value_count > 2))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires a rate vector containing either one or two values, in sexual simulations." << EidosTerminate();
		
		double female_cloning_fraction = rate_value->NumericAtIndex_NOCAST(0, nullptr);
		double male_cloning_fraction = (value_count == 2) ? rate_value->NumericAtIndex_NOCAST(1, nullptr) : female_cloning_fraction;
		
		if ((female_cloning_fraction < 0.0) || (female_cloning_fraction > 1.0) || std::isnan(female_cloning_fraction))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires cloning fractions within [0,1] (" << EidosStringForFloat(female_cloning_fraction) << " supplied)." << EidosTerminate();
		if ((male_cloning_fraction < 0.0) || (male_cloning_fraction > 1.0) || std::isnan(male_cloning_fraction))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires cloning fractions within [0,1] (" << EidosStringForFloat(male_cloning_fraction) << " supplied)." << EidosTerminate();
		
		female_clone_fraction_ = female_cloning_fraction;
		male_clone_fraction_ = male_cloning_fraction;
	}
	else
	{
		// ASEX ONLY: only one value may be specified
		if (value_count != 1)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires a rate vector containing exactly one value, in asexual simulations.." << EidosTerminate();
		
		double cloning_fraction = rate_value->NumericAtIndex_NOCAST(0, nullptr);
		
		if ((cloning_fraction < 0.0) || (cloning_fraction > 1.0) || std::isnan(cloning_fraction))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires cloning fractions within [0,1] (" << EidosStringForFloat(cloning_fraction) << " supplied)." << EidosTerminate();
		
		female_clone_fraction_ = cloning_fraction;
		male_clone_fraction_ = cloning_fraction;
	}
	
	return gStaticEidosValueVOID;
}			

// WF only:
//	*********************	- (void)setSelfingRate(numeric$ rate)
//
EidosValue_SP Subpopulation::ExecuteMethod_setSelfingRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeNonWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSelfingRate): setSelfingRate() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *rate_value = p_arguments[0].get();
	
	double selfing_fraction = rate_value->NumericAtIndex_NOCAST(0, nullptr);
	
	if ((selfing_fraction != 0.0) && sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSelfingRate): setSelfingRate() is limited to the hermaphroditic case, and cannot be called in sexual simulations." << EidosTerminate();
	
	if ((selfing_fraction < 0.0) || (selfing_fraction > 1.0) || std::isnan(selfing_fraction))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSelfingRate): setSelfingRate() requires a selfing fraction within [0,1] (" << EidosStringForFloat(selfing_fraction) << " supplied)." << EidosTerminate();
	
	selfing_fraction_ = selfing_fraction;
	
	return gStaticEidosValueVOID;
}			

// WF only:
//	*********************	- (void)setSexRatio(float$ sexRatio)
//
EidosValue_SP Subpopulation::ExecuteMethod_setSexRatio(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeNonWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): setSexRatio() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *sexRatio_value = p_arguments[0].get();
	
	// SetSexRatio() sets the sex ratio on the child generation, and then that sex ratio takes effect
	// when the children are generated from the parents in EvolveSubpopulation().
	
	// SEX ONLY
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): setSexRatio() is limited to the sexual case, and cannot be called in asexual simulations." << EidosTerminate();
	
	double sex_ratio = sexRatio_value->FloatAtIndex_NOCAST(0, nullptr);
	
	if ((sex_ratio < 0.0) || (sex_ratio > 1.0) || std::isnan(sex_ratio))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): setSexRatio() requires a sex ratio within [0,1] (" << EidosStringForFloat(sex_ratio) << " supplied)." << EidosTerminate();
	
	// After we change the subpop sex ratio, we need to generate new children haplosomes to fit the new requirements
	child_sex_ratio_ = sex_ratio;
	GenerateChildrenToFitWF();
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)setSpatialBounds(numeric position)
//
EidosValue_SP Subpopulation::ExecuteMethod_setSpatialBounds(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *position_value = p_arguments[0].get();
	
	int dimensionality = species_.SpatialDimensionality();
	int value_count = position_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSpatialBounds): setSpatialBounds() cannot be called in non-spatial simulations." << EidosTerminate();
	
	if (value_count != dimensionality * 2)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSpatialBounds): setSpatialBounds() requires twice as many coordinates as the spatial dimensionality of the simulation." << EidosTerminate();
	
	bool bad_bounds = false, bad_periodic_bounds = false;
	bool periodic_x, periodic_y, periodic_z;
	
	species_.SpatialPeriodicity(&periodic_x, &periodic_y, &periodic_z);
	
	switch (dimensionality)
	{
		case 1:
			bounds_x0_ = position_value->NumericAtIndex_NOCAST(0, nullptr);	bounds_x1_ = position_value->NumericAtIndex_NOCAST(1, nullptr);
			
			if (bounds_x1_ <= bounds_x0_)
				bad_bounds = true;
			if (periodic_x && (bounds_x0_ != 0.0))
				bad_periodic_bounds = true;
			
			break;
		case 2:
			bounds_x0_ = position_value->NumericAtIndex_NOCAST(0, nullptr);	bounds_x1_ = position_value->NumericAtIndex_NOCAST(2, nullptr);
			bounds_y0_ = position_value->NumericAtIndex_NOCAST(1, nullptr);	bounds_y1_ = position_value->NumericAtIndex_NOCAST(3, nullptr);
			
			if ((bounds_x1_ <= bounds_x0_) || (bounds_y1_ <= bounds_y0_))
				bad_bounds = true;
			if ((periodic_x && (bounds_x0_ != 0.0)) || (periodic_y && (bounds_y0_ != 0.0)))
				bad_periodic_bounds = true;
			
			break;
		case 3:
			bounds_x0_ = position_value->NumericAtIndex_NOCAST(0, nullptr);	bounds_x1_ = position_value->NumericAtIndex_NOCAST(3, nullptr);
			bounds_y0_ = position_value->NumericAtIndex_NOCAST(1, nullptr);	bounds_y1_ = position_value->NumericAtIndex_NOCAST(4, nullptr);
			bounds_z0_ = position_value->NumericAtIndex_NOCAST(2, nullptr);	bounds_z1_ = position_value->NumericAtIndex_NOCAST(5, nullptr);
			
			if ((bounds_x1_ <= bounds_x0_) || (bounds_y1_ <= bounds_y0_) || (bounds_z1_ <= bounds_z0_))
				bad_bounds = true;
			if ((periodic_x && (bounds_x0_ != 0.0)) || (periodic_y && (bounds_y0_ != 0.0)) || (periodic_z && (bounds_z0_ != 0.0)))
				bad_periodic_bounds = true;
			
			break;
		default:
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSpatialBounds): (internal error) unrecognized dimensionality." << EidosTerminate();
	}
	
	if (bad_bounds)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSpatialBounds): setSpatialBounds() requires min coordinates to be less than max coordinates." << EidosTerminate();
	
	// When a spatial dimension has been declared to be periodic, we require a min coordinate of 0.0 for that dimension
	// to keep the wrapping math simple and fast; it would not be hard to generalize the math but it would be slower
	if (bad_periodic_bounds)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSpatialBounds): setSpatialBounds() requires min coordinates to be 0.0 for dimensions that are periodic." << EidosTerminate();
	
	// Check that all spatial maps attached to this subpop are compatible with our new bounds; error if not
	for (const auto &map_iter : spatial_maps_)
	{
		SpatialMap *map = map_iter.second;
		
		if (!map->IsCompatibleWithSubpopulation(this))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSpatialBounds): setSpatialBounds() new spatial bounds are not compatible with an attached map named '" << map->name_ << "'; use removeSpatialMap() to remove incompatible spatial maps before changing the spatial bounds.  (This enforces internal consistency and avoids accidentally stretching a map to new spatial bounds.)" << EidosTerminate();
	}
	
	return gStaticEidosValueVOID;
}			

// WF only:
//	*********************	- (void)setSubpopulationSize(integer$ size)
//
EidosValue_SP Subpopulation::ExecuteMethod_setSubpopulationSize(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeNonWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSubpopulationSize): setSubpopulationSize() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *size_value = p_arguments[0].get();
	
	slim_popsize_t subpop_size = SLiMCastToPopsizeTypeOrRaise(size_value->IntAtIndex_NOCAST(0, nullptr));
	
	population_.SetSize(*this, subpop_size);
	
	return gStaticEidosValueVOID;
}

// nonWF only:
//	*********************	- (void)removeSubpopulation()
//
EidosValue_SP Subpopulation::ExecuteMethod_removeSubpopulation(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_removeSubpopulation): removeSubpopulation() is not available in WF models." << EidosTerminate();
	
	// TIMING RESTRICTION
	if (community_.executing_species_ == &species_)
		if ((community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventFirst) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_removeSubpopulation): removeSubpopulation() must be called directly from a first(), early(), or late() event, when called on the currently executing species." << EidosTerminate();
	
	population_.RemoveSubpopulation(*this);
	
	return gStaticEidosValueVOID;
}

//	*********************	- (float)cachedFitness(Ni indices)
//
EidosValue_SP Subpopulation::ExecuteMethod_cachedFitness(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *indices_value = p_arguments[0].get();
	
	// TIMING RESTRICTION
	if (model_type_ == SLiMModelType::kModelTypeWF)
	{
		if (community_.executing_species_ == &species_)
			if (community_.CycleStage() == SLiMCycleStage::kWFStage6CalculateFitness)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() may not be called for the currently executing species while its fitness values are being calculated." << EidosTerminate();
		
		// We used to disallow calling cachedFitness() in late() events in WF models in all cases (since a commit on 5 March 2018),
		// because the cached fitness values at that point are typically garbage.  However, it is useful to allow the user to call
		// recalculateFitness() and then cachedFitness(); in that case the cached fitness values are valid.  This allows WF models
		// to interpose logic that changes fitness values based upon fitness values (such as hard selection).
		if ((community_.CycleStage() == SLiMCycleStage::kWFStage5ExecuteLateScripts) && !species_.has_recalculated_fitness_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() generally cannot be called during late() events in WF models, since the new generation does not yet have fitness values (which are calculated immediately after late() events have executed).  If you really need to get fitness values in a late() event, you can call recalculateFitness() first to force fitness value recalculation to occur, but that is not something to do lightly; proceed with caution.  Usually it is better to access fitness values after SLiM has calculated them, in a first() or early() event." << EidosTerminate();
	}
	else
	{
		if (community_.executing_species_ == &species_)
			if (community_.CycleStage() == SLiMCycleStage::kNonWFStage3CalculateFitness)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() may not be called for the currently executing species while its fitness values are being calculated." << EidosTerminate();
		// in nonWF models uncalculated fitness values for new individuals are guaranteed to be NaN, so there is no need for a check here
	}
	
	bool do_all_indices = (indices_value->Type() == EidosValueType::kValueNULL);
	slim_popsize_t index_count = (do_all_indices ? parent_subpop_size_ : SLiMCastToPopsizeTypeOrRaise(indices_value->Count()));
	EidosValue_Float *float_return = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(index_count);
	EidosValue_SP result_SP = EidosValue_SP(float_return);
	const int64_t *indices = (do_all_indices ? nullptr : indices_value->IntData());
	
	for (slim_popsize_t value_index = 0; value_index < index_count; value_index++)
	{
		slim_popsize_t index = value_index;
		
		if (!do_all_indices)
		{
			index = SLiMCastToPopsizeTypeOrRaise(indices[value_index]);
			
			if (index >= parent_subpop_size_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() index " << index << " out of range." << EidosTerminate();
		}
		
		double fitness = (individual_cached_fitness_OVERRIDE_ ? individual_cached_fitness_OVERRIDE_value_ : parent_individuals_[index]->cached_fitness_UNSAFE_);
		
		float_return->set_float_no_check(fitness, value_index);
	}
	
	return result_SP;
}

//  *********************	– (No<Individual>)sampleIndividuals(integer$ size, [logical$ replace = F], [No<Individual>$ exclude = NULL], [Ns$ sex = NULL], [Ni$ tag = NULL], [Ni$ minAge = NULL], [Ni$ maxAge = NULL], [Nl$ migrant = NULL], [Nl$ tagL0 = NULL], [Nl$ tagL1 = NULL], [Nl$ tagL2 = NULL], [Nl$ tagL3 = NULL], [Nl$ tagL4 = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_sampleIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// This method is patterned closely upon Eidos_ExecuteFunction_sample(), but with no weights vector, and with various ways to narrow down the candidate pool
	EidosValue_SP result_SP(nullptr);
	
	int64_t sample_size = p_arguments[0]->IntAtIndex_NOCAST(0, nullptr);
	bool replace = p_arguments[1]->LogicalAtIndex_NOCAST(0, nullptr);
	int x_count = parent_subpop_size_;
	
	if (sample_size < 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): sampleIndividuals() requires a sample size >= 0 (" << sample_size << " supplied)." << EidosTerminate(nullptr);
	if ((sample_size == 0) || (x_count == 0))
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class));
	
	// a specific individual may be excluded
	EidosValue *exclude_value = p_arguments[2].get();
	Individual *excluded_individual = nullptr;
	slim_popsize_t excluded_index = -1;
	
	if (exclude_value->Type() != EidosValueType::kValueNULL)
		excluded_individual = (Individual *)exclude_value->ObjectElementAtIndex_NOCAST(0, nullptr);
	
	if (excluded_individual)
	{
		// SPECIES CONSISTENCY CHECK
		if (excluded_individual->subpopulation_ != this)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): the excluded individual must belong to the subpopulation being sampled." << EidosTerminate(nullptr);
		
		if (excluded_individual->index_ == -1)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): the excluded individual must be a valid, visible individual (not a newly generated child)." << EidosTerminate(nullptr);
		
		excluded_index = excluded_individual->index_;
	}
	
	// a sex may be specified
	EidosValue *sex_value = p_arguments[3].get();
	IndividualSex sex = IndividualSex::kUnspecified;
	
	if (sex_value->Type() != EidosValueType::kValueNULL)
	{
		const std::string &sex_string = ((EidosValue_String *)sex_value)->StringRefAtIndex_NOCAST(0, nullptr);
		
		if (sex_string == "M")			sex = IndividualSex::kMale;
		else if (sex_string == "F")		sex = IndividualSex::kFemale;
		else
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): unrecognized value for sex in sampleIndividuals(); sex must be 'F', 'M', or NULL." << EidosTerminate(nullptr);
		
		if (!sex_enabled_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): sex must be NULL in non-sexual models." << EidosTerminate(nullptr);
	}
	
	// a tag value may be specified; if so, tag values must be defined for all individuals
	EidosValue *tag_value = p_arguments[4].get();
	bool tag_specified = (tag_value->Type() != EidosValueType::kValueNULL);
	slim_usertag_t tag = (tag_specified ? tag_value->IntAtIndex_NOCAST(0, nullptr) : 0);
	
	// an age min or max may be specified in nonWF models
	EidosValue *ageMin_value = p_arguments[5].get();
	EidosValue *ageMax_value = p_arguments[6].get();
	bool ageMin_specified = (ageMin_value->Type() != EidosValueType::kValueNULL);
	bool ageMax_specified = (ageMax_value->Type() != EidosValueType::kValueNULL);
	int64_t ageMin = (ageMin_specified ? ageMin_value->IntAtIndex_NOCAST(0, nullptr) : -1);
	int64_t ageMax = (ageMax_specified ? ageMax_value->IntAtIndex_NOCAST(0, nullptr) : INT64_MAX);
	
	if ((ageMin_specified || ageMax_specified) && (model_type_ != SLiMModelType::kModelTypeNonWF))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): ageMin and ageMax may only be specified in nonWF models." << EidosTerminate(nullptr);
	
	// a migrant value may be specified
	EidosValue *migrant_value = p_arguments[7].get();
	bool migrant_specified = (migrant_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t migrant = (migrant_specified ? migrant_value->LogicalAtIndex_NOCAST(0, nullptr) : false);
	
	// logical tag values, tagL0 - tagL4, may be specified; if so, those tagL values must be defined for all individuals
	EidosValue *tagL0_value = p_arguments[8].get();
	bool tagL0_specified = (tagL0_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL0 = (tagL0_specified ? tagL0_value->LogicalAtIndex_NOCAST(0, nullptr) : false);
	EidosValue *tagL1_value = p_arguments[9].get();
	bool tagL1_specified = (tagL1_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL1 = (tagL1_specified ? tagL1_value->LogicalAtIndex_NOCAST(0, nullptr) : false);
	EidosValue *tagL2_value = p_arguments[10].get();
	bool tagL2_specified = (tagL2_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL2 = (tagL2_specified ? tagL2_value->LogicalAtIndex_NOCAST(0, nullptr) : false);
	EidosValue *tagL3_value = p_arguments[11].get();
	bool tagL3_specified = (tagL3_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL3 = (tagL3_specified ? tagL3_value->LogicalAtIndex_NOCAST(0, nullptr) : false);
	EidosValue *tagL4_value = p_arguments[12].get();
	bool tagL4_specified = (tagL4_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL4 = (tagL4_specified ? tagL4_value->LogicalAtIndex_NOCAST(0, nullptr) : false);
	bool any_tagL_specified = (tagL0_specified || tagL1_specified || tagL2_specified || tagL3_specified || tagL4_specified);
	
	// determine the range the sample will be drawn from; this does not take into account tag, tagLX, or ageMin/ageMax
	int first_candidate_index, last_candidate_index, candidate_count;
	
	if (sex == IndividualSex::kUnspecified)
	{
		first_candidate_index = 0;
		last_candidate_index = parent_subpop_size_ - 1;
		candidate_count = parent_subpop_size_;
	}
	else if (sex == IndividualSex::kFemale)
	{
		first_candidate_index = 0;
		last_candidate_index = parent_first_male_index_ - 1;
		candidate_count = parent_first_male_index_;
	}
	else // if (sex == IndividualSex::kMale)
	{
		first_candidate_index = parent_first_male_index_;
		last_candidate_index = parent_subpop_size_ - 1;
		candidate_count = (last_candidate_index - first_candidate_index) + 1;
	}
	
	if ((excluded_index >= first_candidate_index) && (excluded_index <= last_candidate_index))
		candidate_count--;
	else
		excluded_index = -1;
	
	if (!tag_specified && !ageMin_specified && !ageMax_specified && !migrant_specified && !any_tagL_specified)
	{
		// we're in the simple case of no specifed tag/ageMin/ageMax/migrant/tagL, so maybe we can handle it quickly
		if (candidate_count == 0)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class));
		else if (!replace && (candidate_count < sample_size))
			sample_size = candidate_count;
		
		if (sample_size == 1)
		{
			// a sample size of 1 is very common; make it as fast as we can by getting a singleton EidosValue directly from x
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			int sample_index = (int)Eidos_rng_uniform_int(rng, candidate_count) + first_candidate_index;
			
			if ((excluded_index != -1) && (sample_index >= excluded_index))
				sample_index++;
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(parent_individuals_[sample_index], gSLiM_Individual_Class));
		}
		else if (replace)
		{
			// with replacement, we can just do a series of independent draws
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class));
			EidosValue_Object *result = ((EidosValue_Object *)result_SP.get())->resize_no_initialize(sample_size);
			EidosObject **object_result_data = result->data_mutable();
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_SAMPLE_INDIVIDUALS_1);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, sample_size) firstprivate(candidate_count, first_candidate_index, excluded_index, object_result_data) if(sample_size >= EIDOS_OMPMIN_SAMPLE_INDIVIDUALS_1) num_threads(thread_count)
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				
#pragma omp for schedule(static)
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					int sample_index = (int)Eidos_rng_uniform_int(rng, candidate_count) + first_candidate_index;
					
					if ((excluded_index != -1) && (sample_index >= excluded_index))
						sample_index++;
					
					object_result_data[samples_generated] = parent_individuals_[sample_index];
				}
			}
			
			// Retain all of the objects chosen; this is not done in parallel because it would require locks
			// This is dead code at the moment, because Individual does not use retain/release
			if (gSLiM_Individual_Class->UsesRetainRelease())
			{
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					EidosObject *object_element = object_result_data[samples_generated];
					static_cast<EidosDictionaryRetained *>(object_element)->Retain();		// unsafe cast to avoid virtual function overhead
				}
			}
			
			return result_SP;
		}
		else if (sample_size == 2)
		{
			// a sample size of two without replacement is expected to be common (interacting pairs) so optimize for it
			// note that the code above guarantees that here there are at least two candidates to draw
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class));
			EidosValue_Object *result = ((EidosValue_Object *)result_SP.get())->resize_no_initialize(sample_size);
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			
			int sample_index1 = (int)Eidos_rng_uniform_int(rng, candidate_count) + first_candidate_index;
			
			if ((excluded_index != -1) && (sample_index1 >= excluded_index))
				sample_index1++;
			
			result->set_object_element_no_check_NORR(parent_individuals_[sample_index1], 0);
			
			int sample_index2;
			
			do
			{
				sample_index2 = (int)Eidos_rng_uniform_int(rng, candidate_count) + first_candidate_index;
				
				if ((excluded_index != -1) && (sample_index2 >= excluded_index))
					sample_index2++;
			}
			while (sample_index2 == sample_index1);
			
			result->set_object_element_no_check_NORR(parent_individuals_[sample_index2], 1);
			
			return result_SP;
		}
		
		// note that we drop through here if none of the three special cases above is hit
	}
	
	// BCH 12/17/2019: Adding an optimization here.  It is common to call sampleIndividuals() on a large subpopulation
	// to draw a single mate in a reproduction() callback.  It takes a long time to build the index vector below; let's
	// try optimizing that specific case by trying a few random individuals to see if we get lucky.  If we don't,
	// we'll drop through to the base case code, without having lost much time.  At a guess, I'm requiring a candidate
	// count of at least 30 since with a smaller size than that building the index vector won't hurt much anyway.
	// I'm limiting this to 20 tries, so we don't spend too much time on it; the ideal limit will of course depend on
	// the number of candidates versus the number of *valid* candidates, and there's no way to know.
	if ((sample_size == 1) && (candidate_count >= 30))
	{
		gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
		
		for (int try_count = 0; try_count < 20; ++try_count)
		{
			int sample_index = (int)Eidos_rng_uniform_int(rng, candidate_count) + first_candidate_index;
			
			if ((excluded_index != -1) && (sample_index >= excluded_index))
				sample_index++;
			
			Individual *candidate = parent_individuals_[sample_index];
			
			if (tag_specified)
			{
				slim_usertag_t candidate_tag = candidate->tag_value_;
				
				if (candidate_tag == SLIM_TAG_UNSET_VALUE)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): a tag constraint was specified, but an individual in the subpopulation does not have a defined tag value, so that constraint cannot be applied." << EidosTerminate(nullptr);
				
				if (candidate_tag != tag)
					continue;
			}
			if (migrant_specified && (candidate->migrant_ != migrant))
				continue;
			if (ageMin_specified && (candidate->age_ < ageMin))
				continue;
			if (ageMax_specified && (candidate->age_ > ageMax))
				continue;
			if (any_tagL_specified)
			{
				if (tagL0_specified)
				{
					if (!candidate->tagL0_set_)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): a tagL0 constraint was specified, but an individual in the subpopulation does not have a defined tagL0 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
					if (candidate->tagL0_value_ != tagL0)
						continue;
				}
				if (tagL1_specified)
				{
					if (!candidate->tagL1_set_)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): a tagL1 constraint was specified, but an individual in the subpopulation does not have a defined tagL1 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
					if (candidate->tagL1_value_ != tagL1)
						continue;
				}
				if (tagL2_specified)
				{
					if (!candidate->tagL2_set_)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): a tagL2 constraint was specified, but an individual in the subpopulation does not have a defined tagL2 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
					if (candidate->tagL2_value_ != tagL2)
						continue;
				}
				if (tagL3_specified)
				{
					if (!candidate->tagL3_set_)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): a tagL3 constraint was specified, but an individual in the subpopulation does not have a defined tagL3 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
					if (candidate->tagL3_value_ != tagL3)
						continue;
				}
				if (tagL4_specified)
				{
					if (!candidate->tagL4_set_)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): a tagL4 constraint was specified, but an individual in the subpopulation does not have a defined tagL4 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
					if (candidate->tagL4_value_ != tagL4)
						continue;
				}
			}
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(parent_individuals_[sample_index], gSLiM_Individual_Class));
		}
	}
	
	// base case
	{
		// get indices of individuals; we sample from this vector and then look up the corresponding individual
		// see sample() for some discussion of this implementation
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("Subpopulation::ExecuteMethod_sampleIndividuals(): usage of statics");
		
		static int *index_buffer = nullptr;
		static int buffer_capacity = 0;
		
		if (last_candidate_index + 1 > buffer_capacity)		// just make it big enough for last_candidate_index, not worth worrying
		{
			buffer_capacity = (last_candidate_index + 1) * 2;		// double whenever we go over capacity, to avoid reallocations
			if (index_buffer)
				free(index_buffer);
			index_buffer = (int *)malloc(buffer_capacity * sizeof(int));	// no need to realloc, we don't need the old data
			if (!index_buffer)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		}
		
		candidate_count = 0;	// we will count how many candidates we actually end up with
		
		if (!tag_specified && !ageMin_specified && !ageMax_specified && !migrant_specified && !any_tagL_specified)
		{
			if (excluded_index == -1)
			{
				for (int value_index = first_candidate_index; value_index <= last_candidate_index; ++value_index)
					index_buffer[candidate_count++] = value_index;
			}
			else
			{
				for (int value_index = first_candidate_index; value_index <= last_candidate_index; ++value_index)
				{
					if (value_index != excluded_index)
						index_buffer[candidate_count++] = value_index;
				}
			}
		}
		else
		{
			for (int value_index = first_candidate_index; value_index <= last_candidate_index; ++value_index)
			{
				Individual *candidate = parent_individuals_[value_index];
				
				if (tag_specified)
				{
					slim_usertag_t candidate_tag = candidate->tag_value_;
					
					if (candidate_tag == SLIM_TAG_UNSET_VALUE)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): a tag constraint was specified, but an individual in the subpopulation does not have a defined tag value, so that constraint cannot be applied." << EidosTerminate(nullptr);
					
					if (candidate_tag != tag)
						continue;
				}
				if (migrant_specified && (candidate->migrant_ != migrant))
					continue;
				if (ageMin_specified && (candidate->age_ < ageMin))
					continue;
				if (ageMax_specified && (candidate->age_ > ageMax))
					continue;
				if (value_index == excluded_index)
					continue;
				if (any_tagL_specified)
				{
					if (tagL0_specified)
					{
						if (!candidate->tagL0_set_)
							EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): a tagL0 constraint was specified, but an individual in the subpopulation does not have a defined tagL0 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
						if (candidate->tagL0_value_ != tagL0)
							continue;
					}
					if (tagL1_specified)
					{
						if (!candidate->tagL1_set_)
							EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): a tagL1 constraint was specified, but an individual in the subpopulation does not have a defined tagL1 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
						if (candidate->tagL1_value_ != tagL1)
							continue;
					}
					if (tagL2_specified)
					{
						if (!candidate->tagL2_set_)
							EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): a tagL2 constraint was specified, but an individual in the subpopulation does not have a defined tagL2 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
						if (candidate->tagL2_value_ != tagL2)
							continue;
					}
					if (tagL3_specified)
					{
						if (!candidate->tagL3_set_)
							EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): a tagL3 constraint was specified, but an individual in the subpopulation does not have a defined tagL3 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
						if (candidate->tagL3_value_ != tagL3)
							continue;
					}
					if (tagL4_specified)
					{
						if (!candidate->tagL4_set_)
							EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): a tagL4 constraint was specified, but an individual in the subpopulation does not have a defined tagL4 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
						if (candidate->tagL4_value_ != tagL4)
							continue;
					}
				}
				
				index_buffer[candidate_count++] = value_index;
			}
		}
		
		if (candidate_count == 0)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class));
		else if (!replace && (candidate_count < sample_size))
			sample_size = candidate_count;
		
		// do the sampling
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class));
		EidosValue_Object *result = ((EidosValue_Object *)result_SP.get())->resize_no_initialize(sample_size);
		EidosObject **object_result_data = result->data_mutable();
		
		if (replace)
		{
			// base case with replacement
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_SAMPLE_INDIVIDUALS_2);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, sample_size, index_buffer) firstprivate(candidate_count, object_result_data) if(sample_size >= EIDOS_OMPMIN_SAMPLE_INDIVIDUALS_2) num_threads(thread_count)
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				
#pragma omp for schedule(static)
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					int rose_index = (int)Eidos_rng_uniform_int(rng, (uint32_t)candidate_count);
					
					object_result_data[samples_generated] = parent_individuals_[index_buffer[rose_index]];
				}
			}
			
			// Retain all of the objects chosen; this is not done in parallel because it would require locks
			// This is dead code at the moment, because Individual does not use retain/release
			if (gSLiM_Individual_Class->UsesRetainRelease())
			{
				for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
				{
					EidosObject *object_element = object_result_data[samples_generated];
					static_cast<EidosDictionaryRetained *>(object_element)->Retain();		// unsafe cast to avoid virtual function overhead
				}
			}
		}
		else
		{
			// base case without replacement; this is not parallelized because of contention over index_buffer removals
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			
			for (int64_t samples_generated = 0; samples_generated < sample_size; ++samples_generated)
			{
#if DEBUG
			// this error should never occur, since we checked the count above
			if (candidate_count <= 0)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): (internal error) sampleIndividuals() ran out of eligible individuals from which to sample." << EidosTerminate(nullptr);		// CODE COVERAGE: This is dead code
#endif
			
				int rose_index = (int)Eidos_rng_uniform_int(rng, (uint32_t)candidate_count);
				
				result->set_object_element_no_check_NORR(parent_individuals_[index_buffer[rose_index]], samples_generated);
				
				index_buffer[rose_index] = index_buffer[--candidate_count];
			}
		}
	}
	
	return result_SP;
}

//  *********************	– (object<Individual>)subsetIndividuals([No<Individual>$ exclude = NULL], [Ns$ sex = NULL], [Ni$ tag = NULL], [Ni$ minAge = NULL], [Ni$ maxAge = NULL], [Nl$ migrant = NULL], [Nl$ tagL0 = NULL], [Nl$ tagL1 = NULL], [Nl$ tagL2 = NULL], [Nl$ tagL3 = NULL], [Nl$ tagL4 = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_subsetIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// This method is patterned closely upon ExecuteMethod_sampleIndividuals(), but without the sampling aspect
	EidosValue_SP result_SP(nullptr);
	
	int x_count = parent_subpop_size_;
	
	if (x_count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class));
	
	// a specific individual may be excluded
	EidosValue *exclude_value = p_arguments[0].get();
	Individual *excluded_individual = nullptr;
	slim_popsize_t excluded_index = -1;
	
	if (exclude_value->Type() != EidosValueType::kValueNULL)
		excluded_individual = (Individual *)exclude_value->ObjectElementAtIndex_NOCAST(0, nullptr);
	
	if (excluded_individual)
	{
		// SPECIES CONSISTENCY CHECK
		if (excluded_individual->subpopulation_ != this)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): the excluded individual must belong to the subpopulation being subset." << EidosTerminate(nullptr);
		
		if (excluded_individual->index_ == -1)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): the excluded individual must be a valid, visible individual (not a newly generated child)." << EidosTerminate(nullptr);
		
		excluded_index = excluded_individual->index_;
	}
	
	// a sex may be specified
	EidosValue *sex_value = p_arguments[1].get();
	IndividualSex sex = IndividualSex::kUnspecified;
	
	if (sex_value->Type() != EidosValueType::kValueNULL)
	{
		const std::string &sex_string = ((EidosValue_String *)sex_value)->StringRefAtIndex_NOCAST(0, nullptr);
		
		if (sex_string == "M")			sex = IndividualSex::kMale;
		else if (sex_string == "F")		sex = IndividualSex::kFemale;
		else
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): unrecognized value for sex in subsetIndividuals(); sex must be 'F', 'M', or NULL." << EidosTerminate(nullptr);
		
		if (!sex_enabled_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): sex must be NULL in non-sexual models." << EidosTerminate(nullptr);
	}
	
	// a tag value may be specified; if so, tag values must be defined for all individuals
	EidosValue *tag_value = p_arguments[2].get();
	bool tag_specified = (tag_value->Type() != EidosValueType::kValueNULL);
	slim_usertag_t tag = (tag_specified ? tag_value->IntAtIndex_NOCAST(0, nullptr) : 0);
	
	// an age min or max may be specified in nonWF models
	EidosValue *ageMin_value = p_arguments[3].get();
	EidosValue *ageMax_value = p_arguments[4].get();
	bool ageMin_specified = (ageMin_value->Type() != EidosValueType::kValueNULL);
	bool ageMax_specified = (ageMax_value->Type() != EidosValueType::kValueNULL);
	int64_t ageMin = (ageMin_specified ? ageMin_value->IntAtIndex_NOCAST(0, nullptr) : -1);
	int64_t ageMax = (ageMax_specified ? ageMax_value->IntAtIndex_NOCAST(0, nullptr) : INT64_MAX);
	
	if ((ageMin_specified || ageMax_specified) && (model_type_ != SLiMModelType::kModelTypeNonWF))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): ageMin and ageMax may only be specified in nonWF models." << EidosTerminate(nullptr);
	
	// a migrant value may be specified
	EidosValue *migrant_value = p_arguments[5].get();
	bool migrant_specified = (migrant_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t migrant = (migrant_specified ? migrant_value->LogicalAtIndex_NOCAST(0, nullptr) : false);
	
	// logical tag values, tagL0 - tagL4, may be specified; if so, those tagL values must be defined for all individuals
	EidosValue *tagL0_value = p_arguments[6].get();
	bool tagL0_specified = (tagL0_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL0 = (tagL0_specified ? tagL0_value->LogicalAtIndex_NOCAST(0, nullptr) : false);
	EidosValue *tagL1_value = p_arguments[7].get();
	bool tagL1_specified = (tagL1_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL1 = (tagL1_specified ? tagL1_value->LogicalAtIndex_NOCAST(0, nullptr) : false);
	EidosValue *tagL2_value = p_arguments[8].get();
	bool tagL2_specified = (tagL2_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL2 = (tagL2_specified ? tagL2_value->LogicalAtIndex_NOCAST(0, nullptr) : false);
	EidosValue *tagL3_value = p_arguments[9].get();
	bool tagL3_specified = (tagL3_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL3 = (tagL3_specified ? tagL3_value->LogicalAtIndex_NOCAST(0, nullptr) : false);
	EidosValue *tagL4_value = p_arguments[10].get();
	bool tagL4_specified = (tagL4_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL4 = (tagL4_specified ? tagL4_value->LogicalAtIndex_NOCAST(0, nullptr) : false);
	bool any_tagL_specified = (tagL0_specified || tagL1_specified || tagL2_specified || tagL3_specified || tagL4_specified);
	
	// determine the range the sample will be drawn from; this does not take into account tag, tagLX, or ageMin/ageMax
	int first_candidate_index, last_candidate_index, candidate_count;
	
	if (sex == IndividualSex::kUnspecified)
	{
		first_candidate_index = 0;
		last_candidate_index = parent_subpop_size_ - 1;
		candidate_count = parent_subpop_size_;
	}
	else if (sex == IndividualSex::kFemale)
	{
		first_candidate_index = 0;
		last_candidate_index = parent_first_male_index_ - 1;
		candidate_count = parent_first_male_index_;
	}
	else // if (sex == IndividualSex::kMale)
	{
		first_candidate_index = parent_first_male_index_;
		last_candidate_index = parent_subpop_size_ - 1;
		candidate_count = (last_candidate_index - first_candidate_index) + 1;
	}
	
	if ((excluded_index >= first_candidate_index) && (excluded_index <= last_candidate_index))
		candidate_count--;
	else
		excluded_index = -1;
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Individual_Class));
	EidosValue_Object *result = ((EidosValue_Object *)result_SP.get());
	
	if (!tag_specified && !ageMin_specified && !ageMax_specified && !migrant_specified && !any_tagL_specified)
	{
		// usually there will be no specifed tag/ageMin/ageMax/tagL, so handle it more quickly; reserve since we know the size within 1
		result->reserve(candidate_count);
		
		if (excluded_index == -1)
		{
			for (int value_index = first_candidate_index; value_index <= last_candidate_index; ++value_index)
				result->push_object_element_no_check_NORR(parent_individuals_[value_index]);
		}
		else
		{
			for (int value_index = first_candidate_index; value_index <= last_candidate_index; ++value_index)
			{
				if (value_index == excluded_index)
					continue;
				
				result->push_object_element_no_check_NORR(parent_individuals_[value_index]);
			}
		}
	}
	else
	{
		// this is the full case, a bit slower; we might reject the large majority of individuals, so we don't reserve here
		for (int value_index = first_candidate_index; value_index <= last_candidate_index; ++value_index)
		{
			Individual *candidate = parent_individuals_[value_index];
			
			if (tag_specified)
			{
				slim_usertag_t candidate_tag = candidate->tag_value_;
				
				if (candidate_tag == SLIM_TAG_UNSET_VALUE)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): a tag constraint was specified, but an individual in the subpopulation does not have a defined tag value, so that constraint cannot be applied." << EidosTerminate(nullptr);
				
				if (candidate_tag != tag)
					continue;
			}
			if (migrant_specified && (candidate->migrant_ != migrant))
				continue;
			if (ageMin_specified && (candidate->age_ < ageMin))
				continue;
			if (ageMax_specified && (candidate->age_ > ageMax))
				continue;
			if (value_index == excluded_index)
				continue;
			if (any_tagL_specified)
			{
				if (tagL0_specified)
				{
					if (!candidate->tagL0_set_)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): a tagL0 constraint was specified, but an individual in the subpopulation does not have a defined tagL0 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
					if (candidate->tagL0_value_ != tagL0)
						continue;
				}
				if (tagL1_specified)
				{
					if (!candidate->tagL1_set_)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): a tagL1 constraint was specified, but an individual in the subpopulation does not have a defined tagL1 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
					if (candidate->tagL1_value_ != tagL1)
						continue;
				}
				if (tagL2_specified)
				{
					if (!candidate->tagL2_set_)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): a tagL2 constraint was specified, but an individual in the subpopulation does not have a defined tagL2 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
					if (candidate->tagL2_value_ != tagL2)
						continue;
				}
				if (tagL3_specified)
				{
					if (!candidate->tagL3_set_)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): a tagL3 constraint was specified, but an individual in the subpopulation does not have a defined tagL3 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
					if (candidate->tagL3_value_ != tagL3)
						continue;
				}
				if (tagL4_specified)
				{
					if (!candidate->tagL4_set_)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): a tagL4 constraint was specified, but an individual in the subpopulation does not have a defined tagL4 value, so that constraint cannot be applied." << EidosTerminate(nullptr);
					if (candidate->tagL4_value_ != tagL4)
						continue;
				}
			}
			
			result->push_object_element_capcheck_NORR(parent_individuals_[value_index]);
		}
	}
	
	return result_SP;
}

//	*********************	– (object<SpatialMap>$)defineSpatialMap(string$ name, string$ spatiality, numeric values, [logical$ interpolate = F], [Nif valueRange = NULL], [Ns colors = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_defineSpatialMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue_String *name_value = (EidosValue_String *)p_arguments[0].get();
	EidosValue_String *spatiality_value = (EidosValue_String *)p_arguments[1].get();
	EidosValue *values = p_arguments[2].get();
	EidosValue *interpolate_value = p_arguments[3].get();
	EidosValue *value_range = p_arguments[4].get();
	EidosValue *colors = p_arguments[5].get();
	
	const std::string &map_name = name_value->StringRefAtIndex_NOCAST(0, nullptr);
	
	if (map_name.length() == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() map name must not be zero-length." << EidosTerminate();
	
	const std::string &spatiality_string = spatiality_value->StringRefAtIndex_NOCAST(0, nullptr);
	bool interpolate = interpolate_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	// Make our SpatialMap object and populate it with the values provided
	SpatialMap *spatial_map = new SpatialMap(map_name, spatiality_string, this, values, interpolate, value_range, colors);
	
	if (!spatial_map->IsCompatibleWithSubpopulation(this))
	{
		spatial_map->Release();
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() requires the spatial map to be compatible with the target subpopulation; spatiality cannot utilize spatial dimensions beyond those set for the target species, and spatial bounds must match." << EidosTerminate();
	}
	
	// Add the new SpatialMap to our map for future reference
	auto map_iter = spatial_maps_.find(map_name);
	
	if (map_iter != spatial_maps_.end())
	{
		// there is an existing entry under this name; remove it
		SpatialMap *old_map = map_iter->second;
		
		spatial_maps_.erase(map_iter);
		old_map->Release();
	}
	
	spatial_maps_.emplace(map_name, spatial_map);	// already retained by new SpatialMap(); that is the retain for spatial_maps_
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(spatial_map, gSLiM_SpatialMap_Class));
}

//	*********************	– (void)addSpatialMap(object<SpatialMap>$ map)
//
EidosValue_SP Subpopulation::ExecuteMethod_addSpatialMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *map_value = (EidosValue *)p_arguments[0].get();
	SpatialMap *spatial_map = (SpatialMap *)map_value->ObjectElementAtIndex_NOCAST(0, nullptr);
	std::string map_name = spatial_map->name_;
	
	// Check for an existing entry under this name; that is an error, unless it is the same spatial map object
	auto map_iter = spatial_maps_.find(map_name);
	
	if (map_iter != spatial_maps_.end())
	{
		SpatialMap *old_map = map_iter->second;
		
		if (old_map == spatial_map)
			return gStaticEidosValueVOID;
		
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSpatialMap): addSpatialMap() found an existing map of the same name ('" << map_name << "'); map names must be unique within each subpopulation." << EidosTerminate();
	}
	
	// Check the new spatial map's compatibility with ourselves
	if (!spatial_map->IsCompatibleWithSubpopulation(this))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSpatialMap): addSpatialMap() requires the spatial map to be compatible with the target subpopulation in terms of spatiality/dimensionality and bounds." << EidosTerminate();
	
	// Add the new SpatialMap to our map for future reference
	spatial_map->Retain();
	spatial_maps_.emplace(map_name, spatial_map);
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)removeSpatialMap(so<SpatialMap>$ map)
//
EidosValue_SP Subpopulation::ExecuteMethod_removeSpatialMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *map_value = (EidosValue *)p_arguments[0].get();
	
	if (map_value->Type() == EidosValueType::kValueString)
	{
		std::string map_name = map_value->StringAtIndex_NOCAST(0, nullptr);
		
		auto map_iter = spatial_maps_.find(map_name);
		
		if (map_iter != spatial_maps_.end())
		{
			// there is an existing entry under this name; remove it and we're done
			SpatialMap *old_map = map_iter->second;
			
			spatial_maps_.erase(map_iter);
			old_map->Release();
			
			return gStaticEidosValueVOID;
		}
	}
	else
	{
		SpatialMap *map = (SpatialMap *)map_value->ObjectElementAtIndex_NOCAST(0, nullptr);
		std::string map_name = map->name_;
		auto map_iter = spatial_maps_.find(map_name);
		
		if (map_iter != spatial_maps_.end())
		{
			// there is an existing entry under this name; if it matches then we're done, if it doesn't, that's an error
			SpatialMap *found_map = map_iter->second;
			
			if (found_map == map)
			{
				spatial_maps_.erase(map_iter);
				map->Release();
				
				return gStaticEidosValueVOID;
			}
			
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_removeSpatialMap): removeSpatialMap() found a map of the same name, but it does not match the map requested for removal." << EidosTerminate();
		}
	}
	
	EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_removeSpatialMap): removeSpatialMap() did not find the map requested to be removed." << EidosTerminate();
	
	return gStaticEidosValueVOID;
}

//	*********************	- (string)spatialMapColor(string$ name, numeric value)
//
EidosValue_SP Subpopulation::ExecuteMethod_spatialMapColor(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue_String *name_value = (EidosValue_String *)p_arguments[0].get();
	const std::string &map_name = name_value->StringRefAtIndex_NOCAST(0, nullptr);
	
	if (map_name.length() == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapColor): spatialMapColor() map name must not be zero-length." << EidosTerminate();
	
	// Find the named SpatialMap
	auto map_iter = spatial_maps_.find(map_name);
	
	if (map_iter != spatial_maps_.end())
	{
		static bool beenHere = false;
		SpatialMap *map = map_iter->second;
		
		if (!beenHere && !gEidosSuppressWarnings) {
			SLIM_ERRSTREAM << "#WARNING (Subpopulation::ExecuteMethod_spatialMapColor): spatialMapColor() has been deprecated; use the SpatialMap method mapColor() instead." << std::endl;
			beenHere = true;
		}
		
		// call out to SpatialMap::ExecuteMethod_mapValue() to do the work
		std::vector<EidosValue_SP> subcall_args = {p_arguments[1]};
		
		return map->ExecuteMethod_mapColor(p_method_id, subcall_args, p_interpreter);
	}
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapColor): spatialMapColor() could not find map with name " << map_name << "." << EidosTerminate();
}

//	(object<Image>$)spatialMapImage(string$ name, [Ni$ width = NULL], [Ni$ height = NULL], [logical$ centers = F], [logical$ color = T])
//
EidosValue_SP Subpopulation::ExecuteMethod_spatialMapImage(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue_String *name_value = (EidosValue_String *)p_arguments[0].get();
	const std::string &map_name = name_value->StringRefAtIndex_NOCAST(0, nullptr);
	
	if (map_name.length() == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapImage): spatialMapImage() map name must not be zero-length." << EidosTerminate();
	
	// Find the named SpatialMap
	auto map_iter = spatial_maps_.find(map_name);
	
	if (map_iter != spatial_maps_.end())
	{
		static bool beenHere = false;
		SpatialMap *map = map_iter->second;
		
		if (!beenHere && !gEidosSuppressWarnings) {
			SLIM_ERRSTREAM << "#WARNING (Subpopulation::ExecuteMethod_spatialMapImage): spatialMapImage() has been deprecated; use the SpatialMap method mapImage() instead." << std::endl;
			beenHere = true;
		}
		
		// call out to SpatialMap::ExecuteMethod_mapValue() to do the work
		std::vector<EidosValue_SP> subcall_args = {p_arguments[1], p_arguments[2], p_arguments[3], p_arguments[4]};
		
		return map->ExecuteMethod_mapImage(p_method_id, subcall_args, p_interpreter);
	}
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapImage): spatialMapImage() could not find map with name " << map_name << "." << EidosTerminate();
}

//	*********************	– (float)spatialMapValue(so<SpatialMap>$ map, float point)
//
EidosValue_SP Subpopulation::ExecuteMethod_spatialMapValue(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// This method, unlike spatialMapColor() and spatialMapImage(), has not been deprecated.  The logic is this.
	// We want to un-clog the Subpopulation API, so specialized functionality of SpatialMap should not be mirrored
	// there.  However, spatialMapValue() is the core functionality, and it is a nice convenience to be able to
	// do it through Subpopulation without having to keep track of the SpatialMap object, just as was done before
	// SLiM 4.1.  In addition, using this API has some code safety benefits: the Subpopulation will check itself for
	// compatibility with the spatial map, and SLiMgui will display the spatial map correctly (which would not
	// be the case if it had not been added to the target subpopulation at all).  Nevertheless, the new SpatialMap
	// method -mapValue() is also available, and can be used instead of this.  This redundancy seems worthwhile.
	EidosValue *map_value = p_arguments[0].get();
	SpatialMap *map = nullptr;
	std::string map_name;
	
	if (map_value->Type() == EidosValueType::kValueString)
	{
		map_name = ((EidosValue_String *)map_value)->StringRefAtIndex_NOCAST(0, nullptr);
		
		if (map_name.length() == 0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): spatialMapValue() map name must not be zero-length." << EidosTerminate();
	}
	else
	{
		map = (SpatialMap *)map_value->ObjectElementAtIndex_NOCAST(0, nullptr);
		map_name = map->name_;
	}
	
	// Find the SpatialMap by name; we do this lookup even if a map object was supplied, to check that that map is present
	auto map_iter = spatial_maps_.find(map_name);
	
	if (map_iter != spatial_maps_.end())
	{
		SpatialMap *found_map = map_iter->second;
		
		if (map && (found_map != map))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): spatialMapValue() could not find map in the target subpopulation (although it did find a different map with the same name)." << EidosTerminate();
		
		// call out to SpatialMap::ExecuteMethod_mapValue() to do the work
		std::vector<EidosValue_SP> subcall_args = {p_arguments[1]};
		
		return found_map->ExecuteMethod_mapValue(p_method_id, subcall_args, p_interpreter);
	}
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): spatialMapValue() could not find map '" << map_name << "' in the target subpopulation." << EidosTerminate();
}

#undef SLiMClampCoordinate

//	*********************	– (void)outputMSSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [Ns$ filePath = NULL], [logical$ append=F], [logical$ filterMonomorphic = F], [Niso<Chromosome>$ chromosome = NULL])
//	*********************	– (void)outputSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [Ns$ filePath = NULL], [logical$ append=F], [Niso<Chromosome>$ chromosome = NULL])
//	*********************	– (void)outputVCFSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [logical$ outputMultiallelics = T], [Ns$ filePath = NULL], [logical$ append=F], [logical$ simplifyNucleotides = F], [logical$ outputNonnucleotides = T], [logical$ groupAsIndividuals = T], [Niso<Chromosome>$ chromosome = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_outputXSample(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *sampleSize_value = p_arguments[0].get();
	EidosValue *replace_value = p_arguments[1].get();
	EidosValue *requestedSex_value = p_arguments[2].get();
	EidosValue *outputMultiallelics_arg = ((p_method_id == gID_outputVCFSample) ? p_arguments[3].get() : nullptr);
	EidosValue *filePath_arg = ((p_method_id == gID_outputVCFSample) ? p_arguments[4].get() : p_arguments[3].get());
	EidosValue *append_arg = ((p_method_id == gID_outputVCFSample) ? p_arguments[5].get() : p_arguments[4].get());
	EidosValue *filterMonomorphic_arg = ((p_method_id == gID_outputMSSample) ? p_arguments[5].get() : nullptr);
	EidosValue *simplifyNucleotides_arg = ((p_method_id == gID_outputVCFSample) ? p_arguments[6].get() : nullptr);
	EidosValue *outputNonnucleotides_arg = ((p_method_id == gID_outputVCFSample) ? p_arguments[7].get() : nullptr);
	EidosValue *groupAsIndividuals_arg = ((p_method_id == gID_outputVCFSample) ? p_arguments[8].get() : nullptr);
	EidosValue *chromosome_arg;
	
	if (p_method_id == gID_outputMSSample)
		chromosome_arg = p_arguments[6].get();
	else if (p_method_id == gID_outputSample)
		chromosome_arg = p_arguments[5].get();
	else if (p_method_id == gID_outputVCFSample)
		chromosome_arg = p_arguments[9].get();
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_outputXSample): (internal error) unrecognized method id." << EidosTerminate();
	
	std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
	
	// TIMING RESTRICTION
	if (!community_.warned_early_output_)
	{
		if ((community_.CycleStage() == SLiMCycleStage::kWFStage0ExecuteFirstScripts) ||
			(community_.CycleStage() == SLiMCycleStage::kWFStage1ExecuteEarlyScripts))
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ErrorOutputStream() << "#WARNING (Subpopulation::ExecuteMethod_outputXSample): " << EidosStringRegistry::StringForGlobalStringID(p_method_id) << "() should probably not be called from a first() or early() event in a WF model; the output will reflect state at the beginning of the cycle, not the end." << std::endl;
				community_.warned_early_output_ = true;
			}
		}
	}
	
	slim_popsize_t sample_size = SLiMCastToPopsizeTypeOrRaise(sampleSize_value->IntAtIndex_NOCAST(0, nullptr));
	
	bool replace = replace_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	IndividualSex requested_sex;
	
	std::string sex_string = requestedSex_value->StringAtIndex_NOCAST(0, nullptr);
	
	if (sex_string.compare("M") == 0)
		requested_sex = IndividualSex::kMale;
	else if (sex_string.compare("F") == 0)
		requested_sex = IndividualSex::kFemale;
	else if (sex_string.compare("*") == 0)
		requested_sex = IndividualSex::kUnspecified;
	else
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_outputXSample): " << EidosStringRegistry::StringForGlobalStringID(p_method_id) << "() requested sex '" << sex_string << "' unsupported." << EidosTerminate();
	
	if (!species_.SexEnabled() && requested_sex != IndividualSex::kUnspecified)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_outputXSample): " << EidosStringRegistry::StringForGlobalStringID(p_method_id) << "() requested sex is not legal in a non-sexual simulation." << EidosTerminate();
	
	bool output_multiallelics = true;
	
	if (p_method_id == gID_outputVCFSample)
		output_multiallelics = outputMultiallelics_arg->LogicalAtIndex_NOCAST(0, nullptr);
	
	bool simplify_nucs = false;
	
	if (p_method_id == gID_outputVCFSample)
		simplify_nucs = simplifyNucleotides_arg->LogicalAtIndex_NOCAST(0, nullptr);
	
	bool output_nonnucs = true;
	
	if (p_method_id == gID_outputVCFSample)
		output_nonnucs = outputNonnucleotides_arg->LogicalAtIndex_NOCAST(0, nullptr);
	
	bool group_as_individuals = true;
	
	if (p_method_id == gID_outputVCFSample)
		group_as_individuals = groupAsIndividuals_arg->LogicalAtIndex_NOCAST(0, nullptr);
	
	bool filter_monomorphic = false;
	
	if (p_method_id == gID_outputMSSample)
		filter_monomorphic = filterMonomorphic_arg->LogicalAtIndex_NOCAST(0, nullptr);
	
	// BCH 2/3/2025: in a multi-chromosome model we need to know which chromosome to sample with; the decision
	// is that the Subpopulation methods will probably be deprecated soon, and so porting them forward should
	// take the easiest path forward.  In their place I will add a new suite of output methods on Individual.
	// The Haplosome output methods will be for an arbitrary set of haplosomes, and will always be single-
	// chromosome; the Individual output methods will be for an arbitrary set of individuals, and will be able
	// to output one or all chromosomes as desired.  Then the Subpopulation methods are end-of-lifed because
	// the only value add they give is that they take the sample for you, and there are easy and good ways to
	// do that now yourself; so they would no longer be worth the API bloat, and would go away.  For consistency
	// I'm requiring a single chromosome for outputVCFSample() too, even though it samples individuals and thus
	// could support multiple chromosomes; best to consistently phase out the old and bring in the new.
	const std::vector<Chromosome *> &chromosomes = species_.Chromosomes();
	Chromosome *chromosome;
	
	if (chromosomes.size() == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_outputXSample): output cannot be generated from a species with no genetics." << EidosTerminate();
	
	if (chromosome_arg->Type() == EidosValueType::kValueNULL)
	{
		if (chromosomes.size() > 1)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_outputXSample): output requires a non-NULL value for chromosome in multi-chromosome models." << EidosTerminate();
		
		chromosome = chromosomes[0];
	}
	else
	{
		// NULL case handled above
		chromosome = species_.GetChromosomeFromEidosValue(chromosome_arg);
	}
	
	// Figure out the right output stream
	std::ofstream outfile;
	bool has_file = false;
	std::string outfile_path;
	
	if (filePath_arg->Type() != EidosValueType::kValueNULL)
	{
		outfile_path = Eidos_ResolvedPath(filePath_arg->StringAtIndex_NOCAST(0, nullptr));
		bool append = append_arg->LogicalAtIndex_NOCAST(0, nullptr);
		
		outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		has_file = true;
		
		if (!outfile.is_open())
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_outputXSample): " << EidosStringRegistry::StringForGlobalStringID(p_method_id) << "() could not open "<< outfile_path << "." << EidosTerminate();
	}
	else
	{
		// before writing anything, erase a progress line if we've got one up, to try to make a clean slate
		Eidos_EraseProgress();
	}
	
	std::ostream &out = *(has_file ? dynamic_cast<std::ostream *>(&outfile) : dynamic_cast<std::ostream *>(&output_stream));
	
	if (!has_file || (p_method_id == gID_outputSample))
	{
		// Output header line.  BCH 3/6/2022: Note that the cycle was added after the tick in SLiM 4.
		out << "#OUT: " << community_.Tick() << " " << species_.Cycle() << " S";
		
		if (p_method_id == gID_outputSample)
			out << "S";
		else if (p_method_id == gID_outputMSSample)
			out << "M";
		else if (p_method_id == gID_outputVCFSample)
			out << "V";
		
		out << " p" << subpopulation_id_ << " " << sample_size;
		
		if (species_.SexEnabled())
			out << " " << requested_sex;
		
		if (chromosomes.size() > 1)
		{
			output_stream << " " << chromosome->Type();						// chromosome type, with >1 chromosome
			output_stream << " \"" << chromosome->Symbol() << "\"";			// chromosome symbol, with >1 chromosome
		}
		
		if (has_file)
			out << " " << outfile_path;
		
		out << std::endl;
	}
	
	// Call out to produce the actual sample
	if (p_method_id == gID_outputSample)
		population_.PrintSample_SLiM(out, *this, sample_size, replace, requested_sex, *chromosome);
	else if (p_method_id == gID_outputMSSample)
		population_.PrintSample_MS(out, *this, sample_size, replace, requested_sex, *chromosome, filter_monomorphic);
	else if (p_method_id == gID_outputVCFSample)
		population_.PrintSample_VCF(out, *this, sample_size, replace, requested_sex, *chromosome, output_multiallelics, simplify_nucs, output_nonnucs, group_as_individuals);
	
	if (has_file)
		outfile.close(); 
	
	return gStaticEidosValueVOID;
}

//	*********************	– (void)configureDisplay([Nf center = NULL], [Nf$ scale = NULL], [Ns$ color = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_configureDisplay(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	EidosValue *center_value = p_arguments[0].get();
	EidosValue *scale_value = p_arguments[1].get();
	EidosValue *color_value = p_arguments[2].get();
	
	// This method doesn't actually do anything unless we're running under SLiMgui
	
	if (center_value->Type() == EidosValueType::kValueNULL)
	{
#ifdef SLIMGUI
		gui_center_from_user_ = false;
#endif
	}
	else
	{
		int center_count = center_value->Count();
		
		if (center_count != 2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_configureDisplay): configureDisplay() requires that center be of exactly size 2 (x and y)." << EidosTerminate();
		
		double x = center_value->FloatAtIndex_NOCAST(0, nullptr);
		double y = center_value->FloatAtIndex_NOCAST(1, nullptr);
		
		if ((x < 0.0) || (x > 1.0) || (y < 0.0) || (y > 1.0))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_configureDisplay): configureDisplay() requires that the specified center be within [0,1] for both x and y." << EidosTerminate();
		
#ifdef SLIMGUI
		gui_center_x_ = x;
		gui_center_y_ = y;
		gui_center_from_user_ = true;
#endif
	}
	
	if (scale_value->Type() == EidosValueType::kValueNULL)
	{
#ifdef SLIMGUI
		gui_radius_scaling_from_user_ = false;
#endif
	}
	else
	{
		double scale = scale_value->FloatAtIndex_NOCAST(0, nullptr);
		
		if ((scale <= 0.0) || (scale > 5.0))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_configureDisplay): configureDisplay() requires that the specified scale be within (0,5]." << EidosTerminate();
		
#ifdef SLIMGUI
		gui_radius_scaling_ = scale;
		gui_radius_scaling_from_user_ = true;
#endif
	}
	
	if (color_value->Type() == EidosValueType::kValueNULL)
	{
#ifdef SLIMGUI
		gui_color_from_user_ = false;
#endif
	}
	else
	{
		std::string &&color = color_value->StringAtIndex_NOCAST(0, nullptr);
		
		if (color.empty())
		{
#ifdef SLIMGUI
			gui_color_from_user_ = false;
#endif
		}
		else
		{
			float color_red, color_green, color_blue;
			
			Eidos_GetColorComponents(color, &color_red, &color_green, &color_blue);
			
#ifdef SLIMGUI
			gui_color_red_ = color_red;
			gui_color_green_ = color_green;
			gui_color_blue_ = color_blue;
			gui_color_from_user_ = true;
#endif
		}
	}
	
	return gStaticEidosValueVOID;
}


//
//	Subpopulation_Class
//
#pragma mark -
#pragma mark Subpopulation_Class
#pragma mark -

EidosClass *gSLiM_Subpopulation_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *Subpopulation_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Subpopulation_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,								true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Subpopulation::GetProperty_Accelerated_id));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_firstMaleIndex,					true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Subpopulation::GetProperty_Accelerated_firstMaleIndex));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_haplosomes,						true,	kEidosValueMaskObject, gSLiM_Haplosome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_haplosomesNonNull,					true,	kEidosValueMaskObject, gSLiM_Haplosome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_individuals,					true,	kEidosValueMaskObject, gSLiM_Individual_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_immigrantSubpopIDs,				true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_immigrantSubpopFractions,		true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_lifetimeReproductiveOutput,		true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_lifetimeReproductiveOutputM,	true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_lifetimeReproductiveOutputF,	true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_name,							false,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_description,					false,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_selfingRate,					true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_cloningRate,					true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_sexRatio,						true,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_spatialBounds,					true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_spatialMaps,					true,	kEidosValueMaskObject, gSLiM_SpatialMap_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_species,						true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Species_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_individualCount,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Subpopulation::GetProperty_Accelerated_individualCount));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,							false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Subpopulation::GetProperty_Accelerated_tag)->DeclareAcceleratedSet(Subpopulation::SetProperty_Accelerated_tag));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_fitnessScaling,					false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Subpopulation::GetProperty_Accelerated_fitnessScaling)->DeclareAcceleratedSet(Subpopulation::SetProperty_Accelerated_fitnessScaling));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Subpopulation_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Subpopulation_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMigrationRates, kEidosValueMaskVOID))->AddIntObject("sourceSubpops", gSLiM_Subpopulation_Class)->AddNumeric("rates"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_haplosomesForChromosomes, kEidosValueMaskObject, gSLiM_Haplosome_Class))->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskOptional, "chromosomes", gSLiM_Chromosome_Class, gStaticEidosValueNULL)->AddInt_OSN("index", gStaticEidosValueNULL)->AddLogical_OS("includeNulls", gStaticEidosValue_LogicalT));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_deviatePositions, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_N("individuals", gSLiM_Individual_Class)->AddString_S("boundary")->AddNumeric_S(gStr_maxDistance)->AddString_S("functionType")->AddEllipsis());
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_deviatePositionsWithMap, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_N("individuals", gSLiM_Individual_Class)->AddString_S("boundary")->AddArg(kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskSingleton, "map", gSLiM_SpatialMap_Class)->AddNumeric_S(gStr_maxDistance)->AddString_S("functionType")->AddEllipsis());
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointDeviated, kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddFloat("point")->AddString_S("boundary")->AddNumeric_S(gStr_maxDistance)->AddString_S("functionType")->AddEllipsis());
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointInBounds, kEidosValueMaskLogical))->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointReflected, kEidosValueMaskFloat))->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointStopped, kEidosValueMaskFloat))->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointPeriodic, kEidosValueMaskFloat))->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointUniform, kEidosValueMaskFloat))->AddInt_OS(gEidosStr_n, gStaticEidosValue_Integer1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointUniformWithMap, kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddArg(kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskSingleton, "map", gSLiM_SpatialMap_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setCloningRate, kEidosValueMaskVOID))->AddNumeric("rate"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSelfingRate, kEidosValueMaskVOID))->AddNumeric_S("rate"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSexRatio, kEidosValueMaskVOID))->AddFloat_S("sexRatio"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSpatialBounds, kEidosValueMaskVOID))->AddNumeric("bounds"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSubpopulationSize, kEidosValueMaskVOID))->AddInt_S("size"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addCloned, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("parent", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddLogical_OS("defer", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addCrossed, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("parent1", gSLiM_Individual_Class)->AddObject_S("parent2", gSLiM_Individual_Class)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskFloat | kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskOptional, "sex", nullptr, gStaticEidosValueNULL)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddLogical_OS("defer", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addEmpty, kEidosValueMaskObject, gSLiM_Individual_Class))->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskFloat | kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskOptional, "sex", nullptr, gStaticEidosValueNULL)->AddLogical_OSN("haplosome1Null", gStaticEidosValueNULL)->AddLogical_OSN("haplosome2Null", gStaticEidosValueNULL)->AddInt_OS("count", gStaticEidosValue_Integer1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addMultiRecombinant, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("pattern", gEidosDictionaryUnretained_Class)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskFloat | kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskOptional, "sex", nullptr, gStaticEidosValueNULL)->AddObject_OSN("parent1", gSLiM_Individual_Class, gStaticEidosValueNULL)->AddObject_OSN("parent2", gSLiM_Individual_Class, gStaticEidosValueNULL)->AddLogical_OSN("randomizeStrands", gStaticEidosValueNULL)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddLogical_OS("defer", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addRecombinant, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_SN(gStr_strand1, gSLiM_Haplosome_Class)->AddObject_SN(gStr_strand2, gSLiM_Haplosome_Class)->AddInt_N(gStr_breaks1)->AddObject_SN(gStr_strand3, gSLiM_Haplosome_Class)->AddObject_SN(gStr_strand4, gSLiM_Haplosome_Class)->AddInt_N(gStr_breaks2)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskFloat | kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskOptional, "sex", nullptr, gStaticEidosValueNULL)->AddObject_OSN("parent1", gSLiM_Individual_Class, gStaticEidosValueNULL)->AddObject_OSN("parent2", gSLiM_Individual_Class, gStaticEidosValueNULL)->AddLogical_OSN("randomizeStrands", gStaticEidosValueNULL)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddLogical_OS("defer", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addSelfed, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("parent", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddLogical_OS("defer", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_takeMigrants, kEidosValueMaskVOID))->AddObject("migrants", gSLiM_Individual_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_removeSubpopulation, kEidosValueMaskVOID)));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_cachedFitness, kEidosValueMaskFloat))->AddInt_N("indices"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_sampleIndividuals, kEidosValueMaskObject, gSLiM_Individual_Class))->AddInt_S("size")->AddLogical_OS("replace", gStaticEidosValue_LogicalF)->AddObject_OSN("exclude", gSLiM_Individual_Class, gStaticEidosValueNULL)->AddString_OSN("sex", gStaticEidosValueNULL)->AddInt_OSN("tag", gStaticEidosValueNULL)->AddInt_OSN("minAge", gStaticEidosValueNULL)->AddInt_OSN("maxAge", gStaticEidosValueNULL)->AddLogical_OSN("migrant", gStaticEidosValueNULL)->AddLogical_OSN("tagL0", gStaticEidosValueNULL)->AddLogical_OSN("tagL1", gStaticEidosValueNULL)->AddLogical_OSN("tagL2", gStaticEidosValueNULL)->AddLogical_OSN("tagL3", gStaticEidosValueNULL)->AddLogical_OSN("tagL4", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_subsetIndividuals, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_OSN("exclude", gSLiM_Individual_Class, gStaticEidosValueNULL)->AddString_OSN("sex", gStaticEidosValueNULL)->AddInt_OSN("tag", gStaticEidosValueNULL)->AddInt_OSN("minAge", gStaticEidosValueNULL)->AddInt_OSN("maxAge", gStaticEidosValueNULL)->AddLogical_OSN("migrant", gStaticEidosValueNULL)->AddLogical_OSN("tagL0", gStaticEidosValueNULL)->AddLogical_OSN("tagL1", gStaticEidosValueNULL)->AddLogical_OSN("tagL2", gStaticEidosValueNULL)->AddLogical_OSN("tagL3", gStaticEidosValueNULL)->AddLogical_OSN("tagL4", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_defineSpatialMap, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_SpatialMap_Class))->AddString_S("name")->AddString_S("spatiality")->AddNumeric("values")->AddLogical_OS(gStr_interpolate, gStaticEidosValue_LogicalF)->AddNumeric_ON("valueRange", gStaticEidosValueNULL)->AddString_ON("colors", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addSpatialMap, kEidosValueMaskVOID, gSLiM_SpatialMap_Class))->AddObject_S("map", gSLiM_SpatialMap_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_removeSpatialMap, kEidosValueMaskVOID, gSLiM_SpatialMap_Class))->AddArg(kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskSingleton, "map", gSLiM_SpatialMap_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_spatialMapColor, kEidosValueMaskString))->AddString_S("name")->AddNumeric("value")->MarkDeprecated());
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_spatialMapImage, kEidosValueMaskObject | kEidosValueMaskSingleton, gEidosImage_Class))->AddString_S("name")->AddInt_OSN(gEidosStr_width, gStaticEidosValueNULL)->AddInt_OSN(gEidosStr_height, gStaticEidosValueNULL)->AddLogical_OS("centers", gStaticEidosValue_LogicalF)->AddLogical_OS(gEidosStr_color, gStaticEidosValue_LogicalT)->MarkDeprecated());
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_spatialMapValue, kEidosValueMaskFloat))->AddArg(kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskSingleton, "map", gSLiM_SpatialMap_Class)->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputMSSample, kEidosValueMaskVOID))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("filterMonomorphic", gStaticEidosValue_LogicalF)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskSingleton, "chromosome", gSLiM_Chromosome_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputVCFSample, kEidosValueMaskVOID))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddLogical_OS("outputMultiallelics", gStaticEidosValue_LogicalT)->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("simplifyNucleotides", gStaticEidosValue_LogicalF)->AddLogical_OS("outputNonnucleotides", gStaticEidosValue_LogicalT)->AddLogical_OS("groupAsIndividuals", gStaticEidosValue_LogicalT)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskSingleton, "chromosome", gSLiM_Chromosome_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputSample, kEidosValueMaskVOID))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskInt | kEidosValueMaskString | kEidosValueMaskObject | kEidosValueMaskOptional | kEidosValueMaskSingleton, "chromosome", gSLiM_Chromosome_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_configureDisplay, kEidosValueMaskVOID))->AddFloat_ON("center", gStaticEidosValueNULL)->AddFloat_OSN("scale", gStaticEidosValueNULL)->AddString_OSN(gEidosStr_color, gStaticEidosValueNULL));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}




































































