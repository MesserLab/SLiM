//
//  subpopulation.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2023 Philipp Messer.  All rights reserved.
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

// These get called if a null genome is requested but the null junkyard is empty, or if a non-null genome is requested
// but the non-null junkyard is empty; so we know that the primary junkyard for the request cannot service the request.
// If the other junkyard has a genome, we want to repurpose it; this prevents one junkyard from filling up with an
// ever-growing number of genomes while requests to the other junkyard create new genomes (which can happen because
// genomes can be transmogrified between null and non-null after creation).  We create a new genome only if both
// junkyards are empty.

Genome *Subpopulation::_NewSubpopGenome_NULL(GenomeType p_genome_type)
{
	if (genome_junkyard_nonnull.size())
	{
		Genome *back = genome_junkyard_nonnull.back();
		genome_junkyard_nonnull.pop_back();
		
		// got a non-null genome, need to repurpose it to be a null genome
		back->ReinitializeGenomeNullptr(p_genome_type, 0, 0);
		
		return back;
	}
	
	return new (genome_pool_.AllocateChunk()) Genome(p_genome_type);
}

Genome *Subpopulation::_NewSubpopGenome_NONNULL(int p_mutrun_count, slim_position_t p_mutrun_length, GenomeType p_genome_type)
{
	if (genome_junkyard_null.size())
	{
		Genome *back = genome_junkyard_null.back();
		genome_junkyard_null.pop_back();
		
		// got a null genome, need to repurpose it to be a non-null genome cleared to nullptr
		back->ReinitializeGenomeNullptr(p_genome_type, p_mutrun_count, p_mutrun_length);
		
		return back;
	}
	
	return new (genome_pool_.AllocateChunk()) Genome(p_mutrun_count, p_mutrun_length, p_genome_type);
}

// WF only:
void Subpopulation::WipeIndividualsAndGenomes(std::vector<Individual *> &p_individuals, std::vector<Genome *> &p_genomes, slim_popsize_t p_individual_count, slim_popsize_t p_first_male)
{
	Chromosome &chromosome = species_.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	
	if (p_first_male == -1)
	{
		// make hermaphrodites
		if (p_individual_count > 0)
		{
			for (int index = 0; index < p_individual_count; ++index)
			{
				p_genomes[(size_t)index * 2]->ReinitializeGenomeNullptr(GenomeType::kAutosome, mutrun_count, mutrun_length);
				p_genomes[(size_t)index * 2 + 1]->ReinitializeGenomeNullptr(GenomeType::kAutosome, mutrun_count, mutrun_length);
			}
		}
	}
	else
	{
		// make females and males
		for (int index = 0; index < p_individual_count; ++index)
		{
			Genome *genome1 = p_genomes[(size_t)index * 2];
			Genome *genome2 = p_genomes[(size_t)index * 2 + 1];
			Individual *individual = p_individuals[index];
			bool is_female = (index < p_first_male);
			
			individual->sex_ = (is_female ? IndividualSex::kFemale : IndividualSex::kMale);
		
			switch (modeled_chromosome_type_)
			{
				case GenomeType::kAutosome:
				{
					genome1->ReinitializeGenomeNullptr(GenomeType::kAutosome, mutrun_count, mutrun_length);
					genome2->ReinitializeGenomeNullptr(GenomeType::kAutosome, mutrun_count, mutrun_length);
					break;
				}
				case GenomeType::kXChromosome:
				{
					genome1->ReinitializeGenomeNullptr(GenomeType::kXChromosome, mutrun_count, mutrun_length);
					
					if (is_female)	genome2->ReinitializeGenomeNullptr(GenomeType::kXChromosome, mutrun_count, mutrun_length);
					else			genome2->ReinitializeGenomeNullptr(GenomeType::kYChromosome, 0, 0);									// leave as a null genome
					
					break;
				}
				case GenomeType::kYChromosome:
				{
					genome1->ReinitializeGenomeNullptr(GenomeType::kXChromosome, 0, 0);													// leave as a null genome
					
					if (is_female)	genome2->ReinitializeGenomeNullptr(GenomeType::kXChromosome, 0, 0);									// leave as a null genome
					else			genome2->ReinitializeGenomeNullptr(GenomeType::kYChromosome, mutrun_count, mutrun_length);
					
					break;
				}
			}
		}
	}
}

// Reconfigure the child generation to match the set size, sex ratio, etc.  This may involve removing existing individuals,
// or adding new ones.  It may also involve transmogrifying existing individuals to a new sex, etc.  It can also transmogrify
// genomes between a null and non-null state, as a side effect of changing sex.  So this code is really gross and invasive.
void Subpopulation::GenerateChildrenToFitWF()
{
	Chromosome &chromosome = species_.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	
	cached_child_genomes_value_.reset();
	cached_child_individuals_value_.reset();
	
	// First, make the number of Individual objects match, and make the corresponding Genome changes
	int old_individual_count = (int)child_individuals_.size();
	int new_individual_count = child_subpop_size_;
	
	if (new_individual_count > old_individual_count)
	{
		// We also have to make space for the pointers to the genomes and individuals
		child_genomes_.reserve((size_t)new_individual_count * 2);
		child_individuals_.reserve(new_individual_count);
		
		if (species_.HasGenetics())
		{
			for (int new_index = old_individual_count; new_index < new_individual_count; ++new_index)
			{
				// allocate out of our object pools
				// BCH 23 August 2018: passing false to NewSubpopGenome() for p_is_null is sometimes inaccurate, but should
				// be harmless.  If the genomes are ultimately destined to be null genomes, their mutruns buffer will get
				// freed again below.  Now that the disposed genome junkyards can supply each other when empty, there should
				// be no bigger consequence than that performance hit.  It might be nice to figure out, here, what type of
				// genome we will eventually want at this position, and make the right kind up front; but that is a
				// substantial hassle, and this should only matter in unusual models (very large-magnitude population size
				// cycling, primarily – GenerateChildrenToFitWF() often generating many new children).
				Genome *genome1 = NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, GenomeType::kAutosome);
				Genome *genome2 = NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, GenomeType::kAutosome);
				Individual *individual = new (individual_pool_.AllocateChunk()) Individual(this, new_index, genome1, genome2, IndividualSex::kHermaphrodite, -1, /* initial fitness for new subpops */ 1.0, /* p_mean_parent_age */ -1.0F);
				
				child_genomes_.emplace_back(genome1);
				child_genomes_.emplace_back(genome2);
				child_individuals_.emplace_back(individual);
			}
		}
		else
		{
			// In the no-genetics case we know we need null genomes, and we have to create them up front to avoid errors
			for (int new_index = old_individual_count; new_index < new_individual_count; ++new_index)
			{
				Genome *genome1 = NewSubpopGenome_NULL(GenomeType::kAutosome);
				Genome *genome2 = NewSubpopGenome_NULL(GenomeType::kAutosome);
				Individual *individual = new (individual_pool_.AllocateChunk()) Individual(this, new_index, genome1, genome2, IndividualSex::kHermaphrodite, -1, /* initial fitness for new subpops */ 1.0, /* p_mean_parent_age */ -1.0F);
				
				child_genomes_.emplace_back(genome1);
				child_genomes_.emplace_back(genome2);
				child_individuals_.emplace_back(individual);
			}
		}
	}
	else if (new_individual_count < old_individual_count)
	{
		for (int old_index = new_individual_count; old_index < old_individual_count; ++old_index)
		{
			Genome *genome1 = child_genomes_[(size_t)old_index * 2];
			Genome *genome2 = child_genomes_[(size_t)old_index * 2 + 1];
			Individual *individual = child_individuals_[old_index];
			
			// dispose of the genomes and individual
			FreeSubpopGenome(genome1);
			FreeSubpopGenome(genome2);
			
			individual->~Individual();
			individual_pool_.DisposeChunk(const_cast<Individual *>(individual));
		}
		
		child_genomes_.resize((size_t)new_individual_count * 2);
		child_individuals_.resize(new_individual_count);
	}
	
	// Next, fix the type of each genome, and clear them all, and fix individual sex if necessary
	if (sex_enabled_)
	{
		double &sex_ratio = child_sex_ratio_;
		slim_popsize_t &first_male_index = child_first_male_index_;
		
		slim_popsize_t total_males = static_cast<slim_popsize_t>(lround(sex_ratio * new_individual_count));	// round in favor of males, arbitrarily
		
		first_male_index = new_individual_count - total_males;
		
		if (first_male_index <= 0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFitWF): sex ratio of " << sex_ratio << " produced no females." << EidosTerminate();
		else if (first_male_index >= child_subpop_size_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateChildrenToFitWF): sex ratio of " << sex_ratio << " produced no males." << EidosTerminate();
		
		WipeIndividualsAndGenomes(child_individuals_, child_genomes_, new_individual_count, first_male_index);
	}
	else
	{
		WipeIndividualsAndGenomes(child_individuals_, child_genomes_, new_individual_count, -1);	// make hermaphrodites
	}
}

// Generate new individuals to fill out a freshly created subpopulation, including recording in the tree
// sequence unless this is the result of addSubpopSplit() (which does its own recording since parents are
// involved in that case).  This handles both the WF and nonWF cases, which are very similar.
void Subpopulation::GenerateParentsToFit(slim_age_t p_initial_age, double p_sex_ratio, bool p_allow_zero_size, bool p_require_both_sexes, bool p_record_in_treeseq, bool p_haploid, float p_mean_parent_age)
{
	bool pedigrees_enabled = species_.PedigreesEnabled();
	bool recording_tree_sequence = p_record_in_treeseq && species_.RecordingTreeSequence();
	Chromosome &chromosome = species_.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	
	cached_parent_genomes_value_.reset();
	cached_parent_individuals_value_.reset();
	
	if (parent_individuals_.size() || parent_genomes_.size())
		EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateParentsToFit): (internal error) individuals or genomes already present in GenerateParentsToFit()." << EidosTerminate();
	if ((parent_subpop_size_ == 0) && !p_allow_zero_size)
		EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateParentsToFit): (internal error) subpop size of 0 requested." << EidosTerminate();
	
	if (p_haploid)
	{
		if (model_type_ == SLiMModelType::kModelTypeWF)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateParentsToFit): (internal error) cannot create haploid individuals in WF models." << EidosTerminate();
		if (sex_enabled_ && (modeled_chromosome_type_ != GenomeType::kAutosome))
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenerateParentsToFit): (internal error) cannot create haploid individuals when simulating sex chromosomes." << EidosTerminate();
		
		has_null_genomes_ = true;
	}
	
	// We also have to make space for the pointers to the genomes and individuals
	parent_genomes_.reserve((size_t)parent_subpop_size_ * 2);
	parent_individuals_.reserve(parent_subpop_size_);
	
	// Now create new individuals and genomes appropriate for the requested sex ratio and subpop size
	bool has_genetics = species_.HasGenetics();
	std::vector<MutationRun *> shared_empty_runs;
	
	if ((parent_subpop_size_ > 0) && has_genetics)
	{
		// We need to add a *different* empty MutationRun to each mutrun index, so each run comes out of
		// the correct per-thread allocation pool.  See also ExecuteMethod_addEmpty(), which does the same.
		shared_empty_runs.resize(mutrun_count);
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			MutationRunContext &mutrun_context = species_.SpeciesMutationRunContextForMutationRunIndex(run_index);
			
			shared_empty_runs[run_index] = MutationRun::NewMutationRun(mutrun_context);
		}
	}
	
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
			bool is_female = (new_index < first_male_index);
			Genome *genome1, *genome2;
			
			if (has_genetics)
			{
				switch (modeled_chromosome_type_)
				{
					case GenomeType::kAutosome:
					{
						genome1 = NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, GenomeType::kAutosome);
						genome1->ReinitializeGenomeToMutruns(GenomeType::kAutosome, mutrun_count, mutrun_length, shared_empty_runs);
						
						if (p_haploid)
						{
							genome2 = NewSubpopGenome_NULL(GenomeType::kAutosome);
						}
						else
						{
							genome2 = NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, GenomeType::kAutosome);
							genome2->ReinitializeGenomeToMutruns(GenomeType::kAutosome, mutrun_count, mutrun_length, shared_empty_runs);
						}
						break;
					}
					case GenomeType::kXChromosome:
					{
						genome1 = NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, GenomeType::kXChromosome);
						genome1->ReinitializeGenomeToMutruns(GenomeType::kXChromosome, mutrun_count, mutrun_length, shared_empty_runs);
						
						if (is_female)
						{
							genome2 = NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, GenomeType::kXChromosome);
							genome2->ReinitializeGenomeToMutruns(GenomeType::kXChromosome, mutrun_count, mutrun_length, shared_empty_runs);
						}
						else
						{
							genome2 = NewSubpopGenome_NULL(GenomeType::kYChromosome);
						}
						break;
					}
					case GenomeType::kYChromosome:
					{
						genome1 = NewSubpopGenome_NULL(GenomeType::kXChromosome);
						
						if (is_female)
						{
							genome2 = NewSubpopGenome_NULL(GenomeType::kXChromosome);
						}
						else
						{
							genome2 = NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, GenomeType::kYChromosome);
							genome2->ReinitializeGenomeToMutruns(GenomeType::kYChromosome, mutrun_count, mutrun_length, shared_empty_runs);
						}
						break;
					}
				}
			}
			else
			{
				// no-genetics species have null genomes
				switch (modeled_chromosome_type_)
				{
					case GenomeType::kAutosome:
					{
						genome1 = NewSubpopGenome_NULL(GenomeType::kAutosome);
						genome2 = NewSubpopGenome_NULL(GenomeType::kAutosome);
						break;
					}
					case GenomeType::kXChromosome:
					case GenomeType::kYChromosome:
					{
						genome1 = NewSubpopGenome_NULL(GenomeType::kXChromosome);
						genome2 = NewSubpopGenome_NULL(is_female ? GenomeType::kXChromosome : GenomeType::kYChromosome);
						break;
					}
				}
			}
			
			IndividualSex individual_sex = (is_female ? IndividualSex::kFemale : IndividualSex::kMale);
			Individual *individual = new (individual_pool_.AllocateChunk()) Individual(this, new_index, genome1, genome2, individual_sex, p_initial_age, /* initial fitness for new subpops */ 1.0, /* p_mean_parent_age */ p_mean_parent_age);
			
			if (pedigrees_enabled)
				individual->TrackParentage_Parentless(SLiM_GetNextPedigreeID());
			
			// TREE SEQUENCE RECORDING
			if (recording_tree_sequence)
			{
				species_.SetCurrentNewIndividual(individual);
				species_.RecordNewGenome(nullptr, genome1, nullptr, nullptr);
				species_.RecordNewGenome(nullptr, genome2, nullptr, nullptr);
			}
			
			parent_genomes_.emplace_back(genome1);
			parent_genomes_.emplace_back(genome2);
			parent_individuals_.emplace_back(individual);
		}
	}
	else
	{
		// Make hermaphrodites
		for (int new_index = 0; new_index < parent_subpop_size_; ++new_index)
		{
			// allocate out of our object pools
			// start new parental genomes out with a shared empty mutrun; can't be nullptr like child genomes can
			Genome *genome1, *genome2;
			
			if (has_genetics)
			{
				genome1 = NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, GenomeType::kAutosome);
				genome1->ReinitializeGenomeToMutruns(GenomeType::kAutosome, mutrun_count, mutrun_length, shared_empty_runs);
				
				if (p_haploid)
				{
					genome2 = NewSubpopGenome_NULL(GenomeType::kAutosome);
				}
				else
				{
					genome2 = NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, GenomeType::kAutosome);
					genome2->ReinitializeGenomeToMutruns(GenomeType::kAutosome, mutrun_count, mutrun_length, shared_empty_runs);
				}
			}
			else
			{
				// no-genetics species have null genomes
				genome1 = NewSubpopGenome_NULL(GenomeType::kAutosome);
				genome2 = NewSubpopGenome_NULL(GenomeType::kAutosome);
			}
			
			Individual *individual = new (individual_pool_.AllocateChunk()) Individual(this, new_index, genome1, genome2, IndividualSex::kHermaphrodite, p_initial_age, /* initial fitness for new subpops */ 1.0, /* p_mean_parent_age */ p_mean_parent_age);
			
			if (pedigrees_enabled)
				individual->TrackParentage_Parentless(SLiM_GetNextPedigreeID());
			
			// TREE SEQUENCE RECORDING
			if (recording_tree_sequence)
			{
				species_.SetCurrentNewIndividual(individual);
				species_.RecordNewGenome(nullptr, genome1, nullptr, nullptr);
				species_.RecordNewGenome(nullptr, genome2, nullptr, nullptr);
			}
			
			parent_genomes_.emplace_back(genome1);
			parent_genomes_.emplace_back(genome2);
			parent_individuals_.emplace_back(individual);
		}
	}
}

void Subpopulation::CheckIndividualIntegrity(void)
{
	ClearErrorPosition();
	
	if (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosNoBlockType)
		EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) executing block type was not maintained correctly." << EidosTerminate();
	
	SLiMModelType model_type = model_type_;
	Chromosome &chromosome = species_.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	bool has_genetics = species_.HasGenetics();
	
	if (has_genetics && ((mutrun_count == 0) || (mutrun_length == 0)))
		EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) species with genetics has mutrun count/length of 0." << EidosTerminate();
	else if (!has_genetics && ((mutrun_count != 0) || (mutrun_length != 0)))
		EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) species with no genetics has non-zero mutrun count/length." << EidosTerminate();
	
	// below we will use this map to check that every mutation run in use is used at only one mutrun index
	robin_hood::unordered_flat_map<const MutationRun *, slim_mutrun_index_t> mutrun_position_map;
	
	//
	//	Check the parental generation; this is essentially the same in WF and nonWF models
	//
	
	if ((int)parent_individuals_.size() != parent_subpop_size_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between parent_subpop_size_ and parent_individuals_.size()." << EidosTerminate();
	if ((int)parent_genomes_.size() != parent_subpop_size_ * 2)
		EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between parent_subpop_size_ and parent_genomes_.size()." << EidosTerminate();
	
	for (int ind_index = 0; ind_index < parent_subpop_size_; ++ind_index)
	{
		Individual *individual = parent_individuals_[ind_index];
		Genome *genome1 = parent_genomes_[(size_t)ind_index * 2];
		Genome *genome2 = parent_genomes_[(size_t)ind_index * 2 + 1];
		bool invalid_age = false;
		
		if (!individual)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for individual." << EidosTerminate();
		if (!genome1 || !genome2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for genome." << EidosTerminate();
		
		if ((individual->genome1_ != genome1) || (individual->genome2_ != genome2))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between parent_genomes_ and individual->genomeX_." << EidosTerminate();
		
		if (individual->index_ != ind_index)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->index_ and ind_index." << EidosTerminate();
	
		if (individual->subpopulation_ != this)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->subpopulation_ and subpopulation." << EidosTerminate();
		
		if ((genome1->individual_ != individual) || (genome2->individual_ != individual))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between genome->individual_ and individual." << EidosTerminate();
		
		if (!genome1->IsNull() && ((genome1->mutrun_count_ != mutrun_count) || (genome1->mutrun_length_ != mutrun_length)))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) genome 1 of individual has the wrong mutrun count/length." << EidosTerminate();
		if (!genome2->IsNull() && ((genome2->mutrun_count_ != mutrun_count) || (genome2->mutrun_length_ != mutrun_length)))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) genome 2 of individual has the wrong mutrun count/length." << EidosTerminate();
		if (!has_genetics && (!genome1->IsNull() || !genome2->IsNull()))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) no-genetics species has non-null genomes." << EidosTerminate();
		
		if (((genome1->mutrun_count_ == 0) && ((genome1->mutrun_length_ != 0) || (genome1->mutruns_ != nullptr))) ||
			((genome1->mutrun_length_ == 0) && ((genome1->mutrun_count_ != 0) || (genome1->mutruns_ != nullptr))))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mutrun count/length/pointer inconsistency." << EidosTerminate();
		if (((genome2->mutrun_count_ == 0) && ((genome2->mutrun_length_ != 0) || (genome2->mutruns_ != nullptr))) ||
			((genome2->mutrun_length_ == 0) && ((genome2->mutrun_count_ != 0) || (genome2->mutruns_ != nullptr))))
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mutrun count/length/pointer inconsistency." << EidosTerminate();
		
		if (species_.PedigreesEnabled())
		{
			if (individual->pedigree_id_ == -1)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) individual has an invalid pedigree ID." << EidosTerminate();
			if ((genome1->genome_id_ != individual->pedigree_id_ * 2) || (genome2->genome_id_ != individual->pedigree_id_ * 2 + 1))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) genome has an invalid genome ID." << EidosTerminate();
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
		
		if (sex_enabled_)
		{
			bool is_female = (ind_index < parent_first_male_index_);
			GenomeType genome1_type, genome2_type;
			bool genome1_null = false, genome2_null = false;
			
			if ((is_female && (individual->sex_ != IndividualSex::kFemale)) ||
				(!is_female && (individual->sex_ != IndividualSex::kMale)))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->sex_ and parent_first_male_index_." << EidosTerminate();
			
			switch (modeled_chromosome_type_)
			{
				case GenomeType::kAutosome:		genome1_type = GenomeType::kAutosome; genome2_type = GenomeType::kAutosome; break;
				case GenomeType::kXChromosome:	genome1_type = GenomeType::kXChromosome; genome2_type = (is_female ? GenomeType::kXChromosome : GenomeType::kYChromosome); genome2_null = !is_female; break;
				case GenomeType::kYChromosome:	genome1_type = GenomeType::kXChromosome; genome2_type = (is_female ? GenomeType::kXChromosome : GenomeType::kYChromosome); genome1_null = true; genome2_null = is_female; break;
				default: EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) unsupported chromosome type." << EidosTerminate();
			}
			
			if (!has_genetics)
			{
				genome1_null = true;
				genome2_null = true;
			}
			
			// BCH 9/21/2021: when modeling autosomes in a sexual simulation, null genomes are now allowed (male and female haploid gametes in an alternation of generations model, for example)
			if ((modeled_chromosome_type_ != GenomeType::kAutosome) && ((genome1->IsNull() != genome1_null) || (genome2->IsNull() != genome2_null)))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between expected and actual null genome status in sex chromosome simulation." << EidosTerminate();
			if ((genome1->Type() != genome1_type) || (genome2->Type() != genome2_type))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between expected and actual genome type in sexual simulation." << EidosTerminate();
		}
		else
		{
			if (individual->sex_ != IndividualSex::kHermaphrodite)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) non-hermaphrodite individual in non-sexual simulation." << EidosTerminate();
			
			// BCH 9/21/2021: In SLiM 3.7 this is no longer an error, since we can get null genomes from addRecombinant() representing haploids etc.
			//if (genome1->IsNull() || genome2->IsNull())
			//	EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null genome in individual in non-sexual simulation." << EidosTerminate();
			
			if ((genome1->Type() != GenomeType::kAutosome) || (genome2->Type() != GenomeType::kAutosome))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) non-autosome genome in individual in non-sexual simulation." << EidosTerminate();
		}
		
		if (child_generation_valid_)
		{
			// When the child generation is valid, all parental genomes should have null mutrun pointers, so mutrun refcounts are correct
			for (int mutrun_index = 0; mutrun_index < genome1->mutrun_count_; ++mutrun_index)
				if (genome1->mutruns_[mutrun_index] != nullptr)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental genome has a nonnull mutrun pointer." << EidosTerminate();
			
			for (int mutrun_index = 0; mutrun_index < genome2->mutrun_count_; ++mutrun_index)
				if (genome2->mutruns_[mutrun_index] != nullptr)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental genome has a nonnull mutrun pointer." << EidosTerminate();
		}
		else
		{
			// When the parental generation is valid, all parental genomes should have non-null mutrun pointers
			for (int mutrun_index = 0; mutrun_index < genome1->mutrun_count_; ++mutrun_index)
				if (genome1->mutruns_[mutrun_index] == nullptr)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental genome has a null mutrun pointer." << EidosTerminate();
			
			for (int mutrun_index = 0; mutrun_index < genome2->mutrun_count_; ++mutrun_index)
				if (genome2->mutruns_[mutrun_index] == nullptr)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a parental genome has a null mutrun pointer." << EidosTerminate();
			
			// check that every mutrun is used at only one mutrun index (particularly salient for empty mutruns)
			for (int mutrun_index = 0; mutrun_index < genome1->mutrun_count_; ++mutrun_index)
			{
				const MutationRun *mutrun = genome1->mutruns_[mutrun_index];
				auto found_iter = mutrun_position_map.find(mutrun);
				
				if (found_iter == mutrun_position_map.end())
					mutrun_position_map[mutrun] = mutrun_index;
				else if (found_iter->second != mutrun_index)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a mutation run was used at more than one position." << EidosTerminate();
			}
			for (int mutrun_index = 0; mutrun_index < genome2->mutrun_count_; ++mutrun_index)
			{
				const MutationRun *mutrun = genome2->mutruns_[mutrun_index];
				auto found_iter = mutrun_position_map.find(mutrun);
				
				if (found_iter == mutrun_position_map.end())
					mutrun_position_map[mutrun] = mutrun_index;
				else if (found_iter->second != mutrun_index)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a mutation run was used at more than one position." << EidosTerminate();
			}
		}
	}
	
	
	//
	//	Check the child generation; this is only in WF models
	//
	
	if (model_type == SLiMModelType::kModelTypeWF)
	{
		if ((int)child_individuals_.size() != child_subpop_size_)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between child_subpop_size_ and child_individuals_.size()." << EidosTerminate();
		if ((int)child_genomes_.size() != child_subpop_size_ * 2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between child_subpop_size_ and child_genomes_.size()." << EidosTerminate();
		
		for (int ind_index = 0; ind_index < child_subpop_size_; ++ind_index)
		{
			Individual *individual = child_individuals_[ind_index];
			Genome *genome1 = child_genomes_[(size_t)ind_index * 2];
			Genome *genome2 = child_genomes_[(size_t)ind_index * 2 + 1];
			
			if (!individual)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for individual." << EidosTerminate();
			if (!genome1 || !genome2)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null pointer for genome." << EidosTerminate();
			
			if ((individual->genome1_ != genome1) || (individual->genome2_ != genome2))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between child_genomes_ and individual->genomeX_." << EidosTerminate();
			
			if (individual->index_ != ind_index)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->index_ and ind_index." << EidosTerminate();
			
			if (individual->subpopulation_ != this)
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->subpopulation_ and subpopulation." << EidosTerminate();
			
			if ((genome1->individual_ != individual) || (genome2->individual_ != individual))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between genome->individual_ and individual." << EidosTerminate();
			
			if (!genome1->IsNull() && ((genome1->mutrun_count_ != mutrun_count) || (genome1->mutrun_length_ != mutrun_length)))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) genome 1 of individual has the wrong mutrun count/length." << EidosTerminate();
			if (!genome2->IsNull() && ((genome2->mutrun_count_ != mutrun_count) || (genome2->mutrun_length_ != mutrun_length)))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) genome 2 of individual has the wrong mutrun count/length." << EidosTerminate();
			if (!has_genetics && (!genome1->IsNull() || !genome2->IsNull()))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) no-genetics species has non-null genomes." << EidosTerminate();
			
			if (((genome1->mutrun_count_ == 0) && ((genome1->mutrun_length_ != 0) || (genome1->mutruns_ != nullptr))) ||
				((genome1->mutrun_length_ == 0) && ((genome1->mutrun_count_ != 0) || (genome1->mutruns_ != nullptr))))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mutrun count/length/pointer inconsistency." << EidosTerminate();
			if (((genome2->mutrun_count_ == 0) && ((genome2->mutrun_length_ != 0) || (genome2->mutruns_ != nullptr))) ||
				((genome2->mutrun_length_ == 0) && ((genome2->mutrun_count_ != 0) || (genome2->mutruns_ != nullptr))))
				EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mutrun count/length/pointer inconsistency." << EidosTerminate();
			
			if (species_.PedigreesEnabled() && child_generation_valid_)
			{
				if (individual->pedigree_id_ == -1)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) individual has an invalid pedigree ID." << EidosTerminate();
				if ((genome1->genome_id_ != individual->pedigree_id_ * 2) || (genome2->genome_id_ != individual->pedigree_id_ * 2 + 1))
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) genome has an invalid genome ID." << EidosTerminate();
			}
			
			if (sex_enabled_)
			{
				bool is_female = (ind_index < child_first_male_index_);
				GenomeType genome1_type, genome2_type;
				bool genome1_null = false, genome2_null = false;
				
				if ((is_female && (individual->sex_ != IndividualSex::kFemale)) ||
					(!is_female && (individual->sex_ != IndividualSex::kMale)))
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between individual->sex_ and child_first_male_index_." << EidosTerminate();
				
				switch (modeled_chromosome_type_)
				{
					case GenomeType::kAutosome:		genome1_type = GenomeType::kAutosome; genome2_type = GenomeType::kAutosome; break;
					case GenomeType::kXChromosome:	genome1_type = GenomeType::kXChromosome; genome2_type = (is_female ? GenomeType::kXChromosome : GenomeType::kYChromosome); genome2_null = !is_female; break;
					case GenomeType::kYChromosome:	genome1_type = GenomeType::kXChromosome; genome2_type = (is_female ? GenomeType::kXChromosome : GenomeType::kYChromosome); genome1_null = true; genome2_null = is_female; break;
					default: EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) unsupported chromosome type." << EidosTerminate();
				}
				
				if (!has_genetics)
				{
					genome1_null = true;
					genome2_null = true;
				}
				
				// BCH 9/21/2021: when modeling autosomes in a sexual simulation, null genomes are now allowed (male and female haploid gametes in an alternation of generations model, for example)
				if ((modeled_chromosome_type_ != GenomeType::kAutosome) && ((genome1->IsNull() != genome1_null) || (genome2->IsNull() != genome2_null)))
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between expected and actual null genome status in sex chromosome simulation." << EidosTerminate();
				if ((genome1->Type() != genome1_type) || (genome2->Type() != genome2_type))
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) mismatch between expected and actual genome type in sexual simulation." << EidosTerminate();
			}
			else
			{
				if (individual->sex_ != IndividualSex::kHermaphrodite)
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) non-hermaphrodite individual in non-sexual simulation." << EidosTerminate();
				
				// BCH 9/21/2021: In SLiM 3.7 this is no longer an error, since we can get null genomes from addRecombinant() representing haploids etc.
				//if (genome1->IsNull() || genome2->IsNull())
				//	EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null genome in individual in non-sexual simulation." << EidosTerminate();
				
				if ((genome1->Type() != GenomeType::kAutosome) || (genome2->Type() != GenomeType::kAutosome))
					EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) non-autosome genome in individual in non-sexual simulation." << EidosTerminate();
			}
			
			if (child_generation_valid_)
			{
				// When the child generation is active, child genomes should have non-null mutrun pointers
				for (int mutrun_index = 0; mutrun_index < genome1->mutrun_count_; ++mutrun_index)
					if (genome1->mutruns_[mutrun_index] == nullptr)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a child genome has a null mutrun pointer." << EidosTerminate();
				
				for (int mutrun_index = 0; mutrun_index < genome2->mutrun_count_; ++mutrun_index)
					if (genome2->mutruns_[mutrun_index] == nullptr)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a child genome has a null mutrun pointer." << EidosTerminate();
				
				// check that every mutrun is used at only one mutrun index (particularly salient for empty mutruns)
				for (int mutrun_index = 0; mutrun_index < genome1->mutrun_count_; ++mutrun_index)
				{
					const MutationRun *mutrun = genome1->mutruns_[mutrun_index];
					auto found_iter = mutrun_position_map.find(mutrun);
					
					if (found_iter == mutrun_position_map.end())
						mutrun_position_map[mutrun] = mutrun_index;
					else if (found_iter->second != mutrun_index)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a mutation run was used at more than one position." << EidosTerminate();
				}
				for (int mutrun_index = 0; mutrun_index < genome2->mutrun_count_; ++mutrun_index)
				{
					const MutationRun *mutrun = genome2->mutruns_[mutrun_index];
					auto found_iter = mutrun_position_map.find(mutrun);
					
					if (found_iter == mutrun_position_map.end())
						mutrun_position_map[mutrun] = mutrun_index;
					else if (found_iter->second != mutrun_index)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a mutation run was used at more than one position." << EidosTerminate();
				}
			}
			else
			{
				// When the parental generation is active, child genomes should have null mutrun pointers, so mutrun refcounts are correct
				for (int mutrun_index = 0; mutrun_index < genome1->mutrun_count_; ++mutrun_index)
					if (genome1->mutruns_[mutrun_index] != nullptr)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a child genome has a nonnull mutrun pointer." << EidosTerminate();
				
				for (int mutrun_index = 0; mutrun_index < genome2->mutrun_count_; ++mutrun_index)
					if (genome2->mutruns_[mutrun_index] != nullptr)
						EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) a child genome has a nonnull mutrun pointer." << EidosTerminate();
			}
		}
	}
	
	//
	// Check that every mutation run is being used at a position corresponding to the pool it was allocated from
	//
	slim_mutrun_index_t mutrun_count_multiplier = species_.chromosome_->mutrun_count_multiplier_;
	
	for (int thread_num = 0; thread_num < species_.SpeciesMutationRunContextCount(); ++thread_num)
	{
		MutationRunContext &mutrun_context = species_.SpeciesMutationRunContextForThread(thread_num);
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
	
	//
	//	Check the genome junkyards; all genomes should contain nullptr mutruns
	//
	
	if (!has_genetics && genome_junkyard_nonnull.size())
		EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) the nonnull genome junkyard should be empty in no-genetics species." << EidosTerminate();
	
	for (Genome *genome : genome_junkyard_nonnull)
	{
		if (genome->IsNull())
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) null genome in the nonnull genome junkyard." << EidosTerminate();
	}
	
	for (Genome *genome : genome_junkyard_null)
	{
		if (!genome->IsNull())
			EIDOS_TERMINATION << "ERROR (Subpopulation::CheckIndividualIntegrity): (internal error) nonnull genome in the null genome junkyard." << EidosTerminate();
	}
}

Subpopulation::Subpopulation(Population &p_population, slim_objectid_t p_subpopulation_id, slim_popsize_t p_subpop_size, bool p_record_in_treeseq, bool p_haploid) :
	self_symbol_(EidosStringRegistry::GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Subpopulation_Class))), 
	community_(p_population.species_.community_), species_(p_population.species_), population_(p_population), model_type_(p_population.model_type_), subpopulation_id_(p_subpopulation_id), name_(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)), genome_pool_(p_population.species_genome_pool_), individual_pool_(p_population.species_individual_pool_),
	genome_junkyard_nonnull(p_population.species_genome_junkyard_nonnull), genome_junkyard_null(p_population.species_genome_junkyard_null), parent_subpop_size_(p_subpop_size), child_subpop_size_(p_subpop_size)
#if defined(SLIMGUI)
	, gui_premigration_size_(p_subpop_size)
#endif
{
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
							 double p_sex_ratio, GenomeType p_modeled_chromosome_type, bool p_haploid) :
	self_symbol_(EidosStringRegistry::GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)), EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Subpopulation_Class))),
	community_(p_population.species_.community_), species_(p_population.species_), population_(p_population), model_type_(p_population.model_type_), subpopulation_id_(p_subpopulation_id), name_(SLiMEidosScript::IDStringWithPrefix('p', p_subpopulation_id)), genome_pool_(p_population.species_genome_pool_), individual_pool_(p_population.species_individual_pool_),
	genome_junkyard_nonnull(p_population.species_genome_junkyard_nonnull), genome_junkyard_null(p_population.species_genome_junkyard_null), parent_subpop_size_(p_subpop_size),
	parent_sex_ratio_(p_sex_ratio), child_subpop_size_(p_subpop_size), child_sex_ratio_(p_sex_ratio), sex_enabled_(true), modeled_chromosome_type_(p_modeled_chromosome_type)
#if defined(SLIMGUI)
	, gui_premigration_size_(p_subpop_size)
#endif
{
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
		// dispose of genomes and individuals with our object pools
		for (Genome *genome : parent_genomes_)
		{
			genome->~Genome();
			genome_pool_.DisposeChunk(const_cast<Genome *>(genome));
		}
		
		for (Individual *individual : parent_individuals_)
		{
			individual->~Individual();
			individual_pool_.DisposeChunk(const_cast<Individual *>(individual));
		}
		
		for (Genome *genome : child_genomes_)
		{
			genome->~Genome();
			genome_pool_.DisposeChunk(const_cast<Genome *>(genome));
		}
		
		for (Individual *individual : child_individuals_)
		{
			individual->~Individual();
			individual_pool_.DisposeChunk(const_cast<Individual *>(individual));
		}
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
		
		species_.subpop_names_.emplace(p_name);	// added; never removed unless the simulation state is reset
	}
	
	name_ = p_name;
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
			slim_popsize_t genomeCount = parent_subpop_size_ * 2;
			
			for (slim_popsize_t genome_index = 0; genome_index < genomeCount; genome_index++)
			{
				Genome *genome = parent_genomes_[genome_index];
				const int32_t mutrun_count = genome->mutrun_count_;
				
				for (int run_index = 0; run_index < mutrun_count; ++run_index)
				{
					const MutationRun *mutrun = genome->mutruns_[run_index];
					
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
	
	// Figure out our callback scenario: zero, one, or many?  See the comment below, above FitnessOfParentWithGenomeIndices_NoCallbacks(),
	// for more info on this complication.  Here we just figure out which version to call and set up for it.
	int mutationEffect_callback_count = (int)p_mutationEffect_callbacks.size();
	bool mutationEffect_callbacks_exist = (mutationEffect_callback_count > 0);
	bool single_mutationEffect_callback = false;
	MutationType *single_callback_mut_type = nullptr;
	
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
				single_callback_mut_type = found_muttype;
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
						if (result->FloatAtIndex(0, nullptr) == 1.0)
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
						if (result->FloatAtIndex(0, nullptr) == 1.0)
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
							fitness *= FitnessOfParentWithGenomeIndices_NoCallbacks(female_index);
							
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
							if (!mutationEffect_callbacks_exist)
								fitness *= FitnessOfParentWithGenomeIndices_NoCallbacks(female_index);
							else if (single_mutationEffect_callback)
								fitness *= FitnessOfParentWithGenomeIndices_SingleCallback(female_index, p_mutationEffect_callbacks, single_callback_mut_type);
							else
								fitness *= FitnessOfParentWithGenomeIndices_Callbacks(female_index, p_mutationEffect_callbacks);
							
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
						if (!mutationEffect_callbacks_exist)
							fitness *= FitnessOfParentWithGenomeIndices_NoCallbacks(female_index);
						else if (single_mutationEffect_callback)
							fitness *= FitnessOfParentWithGenomeIndices_SingleCallback(female_index, p_mutationEffect_callbacks, single_callback_mut_type);
						else
							fitness *= FitnessOfParentWithGenomeIndices_Callbacks(female_index, p_mutationEffect_callbacks);
						
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
							fitness *= FitnessOfParentWithGenomeIndices_NoCallbacks(male_index);
							
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
							if (!mutationEffect_callbacks_exist)
								fitness *= FitnessOfParentWithGenomeIndices_NoCallbacks(male_index);
							else if (single_mutationEffect_callback)
								fitness *= FitnessOfParentWithGenomeIndices_SingleCallback(male_index, p_mutationEffect_callbacks, single_callback_mut_type);
							else
								fitness *= FitnessOfParentWithGenomeIndices_Callbacks(male_index, p_mutationEffect_callbacks);
							
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
						if (!mutationEffect_callbacks_exist)
							fitness *= FitnessOfParentWithGenomeIndices_NoCallbacks(male_index);
						else if (single_mutationEffect_callback)
							fitness *= FitnessOfParentWithGenomeIndices_SingleCallback(male_index, p_mutationEffect_callbacks, single_callback_mut_type);
						else
							fitness *= FitnessOfParentWithGenomeIndices_Callbacks(male_index, p_mutationEffect_callbacks);
						
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
							fitness *= FitnessOfParentWithGenomeIndices_NoCallbacks(individual_index);
							
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
							if (!mutationEffect_callbacks_exist)
								fitness *= FitnessOfParentWithGenomeIndices_NoCallbacks(individual_index);
							else if (single_mutationEffect_callback)
								fitness *= FitnessOfParentWithGenomeIndices_SingleCallback(individual_index, p_mutationEffect_callbacks, single_callback_mut_type);
							else
								fitness *= FitnessOfParentWithGenomeIndices_Callbacks(individual_index, p_mutationEffect_callbacks);
							
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
						if (!mutationEffect_callbacks_exist)
							fitness *= FitnessOfParentWithGenomeIndices_NoCallbacks(individual_index);
						else if (single_mutationEffect_callback)
							fitness *= FitnessOfParentWithGenomeIndices_SingleCallback(individual_index, p_mutationEffect_callbacks, single_callback_mut_type);
						else
							fitness *= FitnessOfParentWithGenomeIndices_Callbacks(individual_index, p_mutationEffect_callbacks);
						
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
					
					p_computed_fitness = result->FloatAtIndex(0, nullptr);
					
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
					EidosValue_Object_singleton local_mut(gSLiM_Mutation_Block + p_mutation, gSLiM_Mutation_Class);
					EidosValue_Float_singleton local_effect(p_computed_fitness);
					
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
						
						// p_homozygous == -1 means the mutation is opposed by a NULL chromosome; otherwise, 0 means heterozyg., 1 means homozyg.
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
							
							p_computed_fitness = result->FloatAtIndex(0, nullptr);
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
				
				computed_fitness *= result->FloatAtIndex(0, nullptr);
				
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
						
						computed_fitness *= result->FloatAtIndex(0, nullptr);
					}
					catch (...)
					{
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

// FitnessOfParentWithGenomeIndices has three versions, for no callbacks, a single callback, and multiple callbacks.  This is for two reasons.  First,
// it allows the case without mutationEffect() callbacks to run at full speed.  Second, the non-callback case short-circuits when the selection coefficient
// is exactly 0.0f, as an optimization; but that optimization would be invalid in the callback case, since callbacks can change the relative fitness
// of ostensibly neutral mutations.  For reasons of maintainability, the three versions should be kept in synch as closely as possible.
//
// When there is just a single callback, it usually refers to a mutation type that is relatively uncommon.  The model might have neutral mutations in most
// cases, plus a rare (or unique) mutation type that is subject to more complex selection, for example.  We can optimize that very common case substantially
// by making the callout to ApplyMutationEffectCallbacks() only for mutations of the mutation type that the callback modifies.  This pays off mostly when there
// are many common mutations with no callback, plus one rare mutation type that has a callback.  A model of neutral drift across a long chromosome with a
// high mutation rate, with an introduced beneficial mutation with a selection coefficient extremely close to 0, for example, would hit this case hard and
// see a speedup of as much as 25%, so the additional complexity seems worth it (since that's quite a realistic and common case).

// This version of FitnessOfParentWithGenomeIndices assumes no callbacks exist.  It tests for neutral mutations and skips processing them.
//
double Subpopulation::FitnessOfParentWithGenomeIndices_NoCallbacks(slim_popsize_t p_individual_index)
{
	// calculate the fitness of the individual constituted by genome1 and genome2 in the parent population
	double w = 1.0;
	
#if SLIM_USE_NONNEUTRAL_CACHES
	int32_t nonneutral_change_counter = species_.nonneutral_change_counter_;
	int32_t nonneutral_regime = species_.last_nonneutral_regime_;
#endif
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	Genome *genome1 = parent_genomes_[(size_t)p_individual_index * 2];
	Genome *genome2 = parent_genomes_[(size_t)p_individual_index * 2 + 1];
	bool genome1_null = genome1->IsNull();
	bool genome2_null = genome2->IsNull();
	
	if (genome1_null && genome2_null)
	{
		// both genomes are placeholders; for example, we might be simulating the Y chromosome, and this is a female
		return w;
	}
	else if (genome1_null || genome2_null)
	{
		// one genome is null, so we just need to scan through the modeled genome and account for its mutations, including the haploid dominance coefficient
		const Genome *genome = genome1_null ? genome2 : genome1;
		const int32_t mutrun_count = genome->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			const MutationRun *mutrun = genome->mutruns_[run_index];
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *genome_iter, *genome_max;
			
			mutrun->beginend_nonneutral_pointers(&genome_iter, &genome_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *genome_iter = mutrun->begin_pointer_const();
			const MutationIndex *genome_max = mutrun->end_pointer_const();
#endif
			
			// with an unpaired chromosome, we need to multiply each selection coefficient by the haploid dominance coefficient
			while (genome_iter != genome_max)
				w *= (mut_block_ptr + *genome_iter++)->cached_one_plus_haploiddom_sel_;
		}
		
		return w;
	}
	else
	{
		// both genomes are being modeled, so we need to scan through and figure out which mutations are heterozygous and which are homozygous
		const int32_t mutrun_count = genome1->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			const MutationRun *mutrun1 = genome1->mutruns_[run_index];
			const MutationRun *mutrun2 = genome2->mutruns_[run_index];
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *genome1_iter, *genome2_iter, *genome1_max, *genome2_max;
			
			mutrun1->beginend_nonneutral_pointers(&genome1_iter, &genome1_max, nonneutral_change_counter, nonneutral_regime);
			mutrun2->beginend_nonneutral_pointers(&genome2_iter, &genome2_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *genome1_iter = mutrun1->begin_pointer_const();
			const MutationIndex *genome2_iter = mutrun2->begin_pointer_const();
			
			const MutationIndex *genome1_max = mutrun1->end_pointer_const();
			const MutationIndex *genome2_max = mutrun2->end_pointer_const();
#endif
			
			// first, handle the situation before either genome iterator has reached the end of its genome, for simplicity/speed
			if (genome1_iter != genome1_max && genome2_iter != genome2_max)
			{
				MutationIndex genome1_mutation = *genome1_iter, genome2_mutation = *genome2_iter;
				slim_position_t genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_, genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
				
				do
				{
					if (genome1_iter_position < genome2_iter_position)
					{
						// Process a mutation in genome1 since it is leading
						w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_;
						
						if (++genome1_iter == genome1_max)
							break;
						else {
							genome1_mutation = *genome1_iter;
							genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_;
						}
					}
					else if (genome1_iter_position > genome2_iter_position)
					{
						// Process a mutation in genome2 since it is leading
						w *= (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_;
						
						if (++genome2_iter == genome2_max)
							break;
						else {
							genome2_mutation = *genome2_iter;
							genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
						}
					}
					else
					{
						// Look for homozygosity: genome1_iter_position == genome2_iter_position
						slim_position_t position = genome1_iter_position;
						const MutationIndex *genome1_start = genome1_iter;
						
						// advance through genome1 as long as we remain at the same position, handling one mutation at a time
						do
						{
							const MutationIndex *genome2_matchscan = genome2_iter; 
							
							// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
							while (genome2_matchscan != genome2_max && (mut_block_ptr + *genome2_matchscan)->position_ == position)
							{
								if (genome1_mutation == *genome2_matchscan) 		// note pointer equality test
								{
									// a match was found, so we multiply our fitness by the full selection coefficient
									w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_sel_;
									goto homozygousExit1;
								}
								
								genome2_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_;
							
						homozygousExit1:
							
							if (++genome1_iter == genome1_max)
								break;
							else {
								genome1_mutation = *genome1_iter;
								genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_;
							}
						} while (genome1_iter_position == position);
						
						// advance through genome2 as long as we remain at the same position, handling one mutation at a time
						do
						{
							const MutationIndex *genome1_matchscan = genome1_start; 
							
							// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
							while (genome1_matchscan != genome1_max && (mut_block_ptr + *genome1_matchscan)->position_ == position)
							{
								if (genome2_mutation == *genome1_matchscan)		// note pointer equality test
								{
									// a match was found; we know this match was already found by the genome1 loop above, so our fitness has already been multiplied appropriately
									goto homozygousExit2;
								}
								
								genome1_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							w *= (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_;
							
						homozygousExit2:
							
							if (++genome2_iter == genome2_max)
								break;
							else {
								genome2_mutation = *genome2_iter;
								genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
							}
						} while (genome2_iter_position == position);
						
						// break out if either genome has reached its end
						if (genome1_iter == genome1_max || genome2_iter == genome2_max)
							break;
					}
				} while (true);
			}
			
			// one or the other genome has now reached its end, so now we just need to handle the remaining mutations in the unfinished genome
#if DEBUG
			assert(!(genome1_iter != genome1_max && genome2_iter != genome2_max));
#endif
			
			// if genome1 is unfinished, finish it
			while (genome1_iter != genome1_max)
				w *= (mut_block_ptr + *genome1_iter++)->cached_one_plus_dom_sel_;
			
			// if genome2 is unfinished, finish it
			while (genome2_iter != genome2_max)
				w *= (mut_block_ptr + *genome2_iter++)->cached_one_plus_dom_sel_;
		}
		
		return w;
	}
}

// This version of FitnessOfParentWithGenomeIndices assumes multiple callbacks exist.  It doesn't optimize neutral mutations since they might be modified by callbacks.
//
double Subpopulation::FitnessOfParentWithGenomeIndices_Callbacks(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks)
{
	// calculate the fitness of the individual constituted by genome1 and genome2 in the parent population
	double w = 1.0;
	
#if SLIM_USE_NONNEUTRAL_CACHES
	int32_t nonneutral_change_counter = species_.nonneutral_change_counter_;
	int32_t nonneutral_regime = species_.last_nonneutral_regime_;
#endif
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	Individual *individual = parent_individuals_[p_individual_index];
	Genome *genome1 = parent_genomes_[(size_t)p_individual_index * 2];
	Genome *genome2 = parent_genomes_[(size_t)p_individual_index * 2 + 1];
	bool genome1_null = genome1->IsNull();
	bool genome2_null = genome2->IsNull();
	
	if (genome1_null && genome2_null)
	{
		// both genomes are placeholders; for example, we might be simulating the Y chromosome, and this is a female
		return w;
	}
	else if (genome1_null || genome2_null)
	{
		// one genome is null, so we just need to scan through the modeled genome and account for its mutations, including the haploid dominance coefficient
		const Genome *genome = genome1_null ? genome2 : genome1;
		const int32_t mutrun_count = genome->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			const MutationRun *mutrun = genome->mutruns_[run_index];
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *genome_iter, *genome_max;
			
			mutrun->beginend_nonneutral_pointers(&genome_iter, &genome_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *genome_iter = mutrun->begin_pointer_const();
			const MutationIndex *genome_max = mutrun->end_pointer_const();
#endif
			
			// with an unpaired chromosome, we need to multiply each selection coefficient by the haploid dominance coefficient
			while (genome_iter != genome_max)
			{
				MutationIndex genome_mutation = *genome_iter;
				
				w *= ApplyMutationEffectCallbacks(genome_mutation, -1, (mut_block_ptr + genome_mutation)->cached_one_plus_haploiddom_sel_, p_mutationEffect_callbacks, individual);
				
				if (w <= 0.0)
					return 0.0;
				
				genome_iter++;
			}
		}
		
		return w;
	}
	else
	{
		// both genomes are being modeled, so we need to scan through and figure out which mutations are heterozygous and which are homozygous
		const int32_t mutrun_count = genome1->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			const MutationRun *mutrun1 = genome1->mutruns_[run_index];
			const MutationRun *mutrun2 = genome2->mutruns_[run_index];
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *genome1_iter, *genome2_iter, *genome1_max, *genome2_max;
			
			mutrun1->beginend_nonneutral_pointers(&genome1_iter, &genome1_max, nonneutral_change_counter, nonneutral_regime);
			mutrun2->beginend_nonneutral_pointers(&genome2_iter, &genome2_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *genome1_iter = mutrun1->begin_pointer_const();
			const MutationIndex *genome2_iter = mutrun2->begin_pointer_const();
			
			const MutationIndex *genome1_max = mutrun1->end_pointer_const();
			const MutationIndex *genome2_max = mutrun2->end_pointer_const();
#endif
			
			// first, handle the situation before either genome iterator has reached the end of its genome, for simplicity/speed
			if (genome1_iter != genome1_max && genome2_iter != genome2_max)
			{
				MutationIndex genome1_mutation = *genome1_iter, genome2_mutation = *genome2_iter;
				slim_position_t genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_, genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
				
				do
				{
					if (genome1_iter_position < genome2_iter_position)
					{
						// Process a mutation in genome1 since it is leading
						w *= ApplyMutationEffectCallbacks(genome1_mutation, false, (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, individual);
						
						if (w <= 0.0)
							return 0.0;
						
						if (++genome1_iter == genome1_max)
							break;
						else {
							genome1_mutation = *genome1_iter;
							genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_;
						}
					}
					else if (genome1_iter_position > genome2_iter_position)
					{
						// Process a mutation in genome2 since it is leading
						w *= ApplyMutationEffectCallbacks(genome2_mutation, false, (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, individual);
						
						if (w <= 0.0)
							return 0.0;
						
						if (++genome2_iter == genome2_max)
							break;
						else {
							genome2_mutation = *genome2_iter;
							genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
						}
					}
					else
					{
						// Look for homozygosity: genome1_iter_position == genome2_iter_position
						slim_position_t position = genome1_iter_position;
						const MutationIndex *genome1_start = genome1_iter;
						
						// advance through genome1 as long as we remain at the same position, handling one mutation at a time
						do
						{
							const MutationIndex *genome2_matchscan = genome2_iter; 
							
							// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
							while (genome2_matchscan != genome2_max && (mut_block_ptr + *genome2_matchscan)->position_ == position)
							{
								if (genome1_mutation == *genome2_matchscan)		// note pointer equality test
								{
									// a match was found, so we multiply our fitness by the full selection coefficient
									w *= ApplyMutationEffectCallbacks(genome1_mutation, true, (mut_block_ptr + genome1_mutation)->cached_one_plus_sel_, p_mutationEffect_callbacks, individual);
									
									goto homozygousExit3;
								}
								
								genome2_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							w *= ApplyMutationEffectCallbacks(genome1_mutation, false, (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, individual);
							
						homozygousExit3:
							
							if (w <= 0.0)
								return 0.0;
							
							if (++genome1_iter == genome1_max)
								break;
							else {
								genome1_mutation = *genome1_iter;
								genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_;
							}
						} while (genome1_iter_position == position);
						
						// advance through genome2 as long as we remain at the same position, handling one mutation at a time
						do
						{
							const MutationIndex *genome1_matchscan = genome1_start; 
							
							// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
							while (genome1_matchscan != genome1_max && (mut_block_ptr + *genome1_matchscan)->position_ == position)
							{
								if (genome2_mutation == *genome1_matchscan)		// note pointer equality test
								{
									// a match was found; we know this match was already found by the genome1 loop above, so our fitness has already been multiplied appropriately
									goto homozygousExit4;
								}
								
								genome1_matchscan++;
							}
							
							// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
							w *= ApplyMutationEffectCallbacks(genome2_mutation, false, (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, individual);
							
							if (w <= 0.0)
								return 0.0;
							
						homozygousExit4:
							
							if (++genome2_iter == genome2_max)
								break;
							else {
								genome2_mutation = *genome2_iter;
								genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
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
				MutationIndex genome1_mutation = *genome1_iter;
				
				w *= ApplyMutationEffectCallbacks(genome1_mutation, false, (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, individual);
				
				if (w <= 0.0)
					return 0.0;
				
				genome1_iter++;
			}
			
			// if genome2 is unfinished, finish it
			while (genome2_iter != genome2_max)
			{
				MutationIndex genome2_mutation = *genome2_iter;
				
				w *= ApplyMutationEffectCallbacks(genome2_mutation, false, (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, individual);
				
				if (w <= 0.0)
					return 0.0;
				
				genome2_iter++;
			}
		}
		
		return w;
	}
}

// This version of FitnessOfParentWithGenomeIndices assumes a single callback exists, modifying the given mutation type.  It is a hybrid of the previous two versions.
//
double Subpopulation::FitnessOfParentWithGenomeIndices_SingleCallback(slim_popsize_t p_individual_index, std::vector<SLiMEidosBlock*> &p_mutationEffect_callbacks, MutationType *p_single_callback_mut_type)
{
	// calculate the fitness of the individual constituted by genome1 and genome2 in the parent population
	double w = 1.0;
	
#if SLIM_USE_NONNEUTRAL_CACHES
	int32_t nonneutral_change_counter = species_.nonneutral_change_counter_;
	int32_t nonneutral_regime = species_.last_nonneutral_regime_;
#endif
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	Individual *individual = parent_individuals_[p_individual_index];
	Genome *genome1 = parent_genomes_[(size_t)p_individual_index * 2];
	Genome *genome2 = parent_genomes_[(size_t)p_individual_index * 2 + 1];
	bool genome1_null = genome1->IsNull();
	bool genome2_null = genome2->IsNull();
	
	if (genome1_null && genome2_null)
	{
		// both genomes are placeholders; for example, we might be simulating the Y chromosome, and this is a female
		return w;
	}
	else if (genome1_null || genome2_null)
	{
		// one genome is null, so we just need to scan through the modeled genome and account for its mutations, including the haploid dominance coefficient
		const Genome *genome = genome1_null ? genome2 : genome1;
		const int32_t mutrun_count = genome->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			const MutationRun *mutrun = genome->mutruns_[run_index];
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *genome_iter, *genome_max;
			
			mutrun->beginend_nonneutral_pointers(&genome_iter, &genome_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *genome_iter = mutrun->begin_pointer_const();
			const MutationIndex *genome_max = mutrun->end_pointer_const();
#endif
			
			// with an unpaired chromosome, we need to multiply each selection coefficient by the haploid dominance coefficient
			while (genome_iter != genome_max)
			{
				MutationIndex genome_mutation = *genome_iter;
				
				if ((mut_block_ptr + genome_mutation)->mutation_type_ptr_ == p_single_callback_mut_type)
				{
					w *= ApplyMutationEffectCallbacks(genome_mutation, -1, (mut_block_ptr + genome_mutation)->cached_one_plus_haploiddom_sel_, p_mutationEffect_callbacks, individual);
					
					if (w <= 0.0)
						return 0.0;
				}
				else
				{
					w *= (mut_block_ptr + genome_mutation)->cached_one_plus_haploiddom_sel_;
				}
				
				genome_iter++;
			}
		}
		
		return w;
	}
	else
	{
		// both genomes are being modeled, so we need to scan through and figure out which mutations are heterozygous and which are homozygous
		const int32_t mutrun_count = genome1->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			const MutationRun *mutrun1 = genome1->mutruns_[run_index];
			const MutationRun *mutrun2 = genome2->mutruns_[run_index];
			
#if SLIM_USE_NONNEUTRAL_CACHES
			// Cache non-neutral mutations and read from the non-neutral buffers
			const MutationIndex *genome1_iter, *genome2_iter, *genome1_max, *genome2_max;
			
			mutrun1->beginend_nonneutral_pointers(&genome1_iter, &genome1_max, nonneutral_change_counter, nonneutral_regime);
			mutrun2->beginend_nonneutral_pointers(&genome2_iter, &genome2_max, nonneutral_change_counter, nonneutral_regime);
#else
			// Read directly from the MutationRun buffers
			const MutationIndex *genome1_iter = mutrun1->begin_pointer_const();
			const MutationIndex *genome2_iter = mutrun2->begin_pointer_const();
			
			const MutationIndex *genome1_max = mutrun1->end_pointer_const();
			const MutationIndex *genome2_max = mutrun2->end_pointer_const();
#endif
			
			// first, handle the situation before either genome iterator has reached the end of its genome, for simplicity/speed
			if (genome1_iter != genome1_max && genome2_iter != genome2_max)
			{
				MutationIndex genome1_mutation = *genome1_iter, genome2_mutation = *genome2_iter;
				slim_position_t genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_, genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
				
				do
				{
					if (genome1_iter_position < genome2_iter_position)
					{
						// Process a mutation in genome1 since it is leading
						MutationType *genome1_muttype = (mut_block_ptr + genome1_mutation)->mutation_type_ptr_;
						
						if (genome1_muttype == p_single_callback_mut_type)
						{
							w *= ApplyMutationEffectCallbacks(genome1_mutation, false, (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, individual);
							
							if (w <= 0.0)
								return 0.0;
						}
						else
						{
							w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_;
						}
						
						if (++genome1_iter == genome1_max)
							break;
						else {
							genome1_mutation = *genome1_iter;
							genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_;
						}
					}
					else if (genome1_iter_position > genome2_iter_position)
					{
						// Process a mutation in genome2 since it is leading
						MutationType *genome2_muttype = (mut_block_ptr + genome2_mutation)->mutation_type_ptr_;
						
						if (genome2_muttype == p_single_callback_mut_type)
						{
							w *= ApplyMutationEffectCallbacks(genome2_mutation, false, (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, individual);
							
							if (w <= 0.0)
								return 0.0;
						}
						else
						{
							w *= (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_;
						}
						
						if (++genome2_iter == genome2_max)
							break;
						else {
							genome2_mutation = *genome2_iter;
							genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
						}
					}
					else
					{
						// Look for homozygosity: genome1_iter_position == genome2_iter_position
						slim_position_t position = genome1_iter_position;
						const MutationIndex *genome1_start = genome1_iter;
						
						// advance through genome1 as long as we remain at the same position, handling one mutation at a time
						do
						{
							MutationType *genome1_muttype = (mut_block_ptr + genome1_mutation)->mutation_type_ptr_;
							
							if (genome1_muttype == p_single_callback_mut_type)
							{
								const MutationIndex *genome2_matchscan = genome2_iter; 
								
								// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
								while (genome2_matchscan != genome2_max && (mut_block_ptr + *genome2_matchscan)->position_ == position)
								{
									if (genome1_mutation == *genome2_matchscan)		// note pointer equality test
									{
										// a match was found, so we multiply our fitness by the full selection coefficient
										w *= ApplyMutationEffectCallbacks(genome1_mutation, true, (mut_block_ptr + genome1_mutation)->cached_one_plus_sel_, p_mutationEffect_callbacks, individual);
										
										goto homozygousExit5;
									}
									
									genome2_matchscan++;
								}
								
								// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
								w *= ApplyMutationEffectCallbacks(genome1_mutation, false, (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, individual);
								
							homozygousExit5:
								
								if (w <= 0.0)
									return 0.0;
							}
							else
							{
								const MutationIndex *genome2_matchscan = genome2_iter; 
								
								// advance through genome2 with genome2_matchscan, looking for a match for the current mutation in genome1, to determine whether we are homozygous or not
								while (genome2_matchscan != genome2_max && (mut_block_ptr + *genome2_matchscan)->position_ == position)
								{
									if (genome1_mutation == *genome2_matchscan) 		// note pointer equality test
									{
										// a match was found, so we multiply our fitness by the full selection coefficient
										w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_sel_;
										goto homozygousExit6;
									}
									
									genome2_matchscan++;
								}
								
								// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
								w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_;
								
							homozygousExit6:
								;
							}
							
							if (++genome1_iter == genome1_max)
								break;
							else {
								genome1_mutation = *genome1_iter;
								genome1_iter_position = (mut_block_ptr + genome1_mutation)->position_;
							}
						} while (genome1_iter_position == position);
						
						// advance through genome2 as long as we remain at the same position, handling one mutation at a time
						do
						{
							MutationType *genome2_muttype = (mut_block_ptr + genome2_mutation)->mutation_type_ptr_;
							
							if (genome2_muttype == p_single_callback_mut_type)
							{
								const MutationIndex *genome1_matchscan = genome1_start; 
								
								// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
								while (genome1_matchscan != genome1_max && (mut_block_ptr + *genome1_matchscan)->position_ == position)
								{
									if (genome2_mutation == *genome1_matchscan)		// note pointer equality test
									{
										// a match was found; we know this match was already found by the genome1 loop above, so our fitness has already been multiplied appropriately
										goto homozygousExit7;
									}
									
									genome1_matchscan++;
								}
								
								// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
								w *= ApplyMutationEffectCallbacks(genome2_mutation, false, (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, individual);
								
								if (w <= 0.0)
									return 0.0;
								
							homozygousExit7:
								;
							}
							else
							{
								const MutationIndex *genome1_matchscan = genome1_start; 
								
								// advance through genome1 with genome1_matchscan, looking for a match for the current mutation in genome2, to determine whether we are homozygous or not
								while (genome1_matchscan != genome1_max && (mut_block_ptr + *genome1_matchscan)->position_ == position)
								{
									if (genome2_mutation == *genome1_matchscan)		// note pointer equality test
									{
										// a match was found; we know this match was already found by the genome1 loop above, so our fitness has already been multiplied appropriately
										goto homozygousExit8;
									}
									
									genome1_matchscan++;
								}
								
								// no match was found, so we are heterozygous; we multiply our fitness by the selection coefficient and the dominance coefficient
								w *= (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_;
								
							homozygousExit8:
								;
							}
							
							if (++genome2_iter == genome2_max)
								break;
							else {
								genome2_mutation = *genome2_iter;
								genome2_iter_position = (mut_block_ptr + genome2_mutation)->position_;
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
				MutationIndex genome1_mutation = *genome1_iter;
				MutationType *genome1_muttype = (mut_block_ptr + genome1_mutation)->mutation_type_ptr_;
				
				if (genome1_muttype == p_single_callback_mut_type)
				{
					w *= ApplyMutationEffectCallbacks(genome1_mutation, false, (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, individual);
					
					if (w <= 0.0)
						return 0.0;
				}
				else
				{
					w *= (mut_block_ptr + genome1_mutation)->cached_one_plus_dom_sel_;
				}
				
				genome1_iter++;
			}
			
			// if genome2 is unfinished, finish it
			while (genome2_iter != genome2_max)
			{
				MutationIndex genome2_mutation = *genome2_iter;
				MutationType *genome2_muttype = (mut_block_ptr + genome2_mutation)->mutation_type_ptr_;
				
				if (genome2_muttype == p_single_callback_mut_type)
				{
					w *= ApplyMutationEffectCallbacks(genome2_mutation, false, (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_, p_mutationEffect_callbacks, individual);
					
					if (w <= 0.0)
						return 0.0;
				}
				else
				{
					w *= (mut_block_ptr + genome2_mutation)->cached_one_plus_dom_sel_;
				}
				
				genome2_iter++;
			}
		}
		
		return w;
	}
}

// WF only:
void Subpopulation::TallyLifetimeReproductiveOutput(void)
{
	if (species_.PedigreesEnabled())
	{
		lifetime_reproductive_output_MH_.clear();
		lifetime_reproductive_output_F_.clear();
		
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

void Subpopulation::SwapChildAndParentGenomes(void)
{
	bool will_need_new_children = false;
	
	// If there are any differences between the parent and child genome setups (due to change in subpop size, sex ratio, etc.), we will need to create new child genomes after swapping
	// This is because the parental genomes, which are based on the old parental values, will get swapped in to the children, but they will be out of date.
	if (parent_subpop_size_ != child_subpop_size_ || parent_sex_ratio_ != child_sex_ratio_ || parent_first_male_index_ != child_first_male_index_)
		will_need_new_children = true;
	
	// Execute the genome swap
	child_genomes_.swap(parent_genomes_);
	cached_child_genomes_value_.swap(cached_parent_genomes_value_);
	
	// Execute a swap of individuals as well; since individuals carry so little baggage, this is mostly important just for moving tag values
	child_individuals_.swap(parent_individuals_);
	cached_child_individuals_value_.swap(cached_parent_individuals_value_);
	
	// Clear out any dictionary values and color values stored in what are now the child individuals; since this is per-individual it
	// takes a significant amount of time, so we try to minimize the overhead by doing it only when these facilities have been used
	// BCH 6 April 2019: likewise, now, with resetting tags in individuals and genomes to the "unset" value
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
	if (Individual::s_any_genome_tag_set_)
	{
		for (Individual *child : child_individuals_)
		{
			child->genome1_->tag_value_ = SLIM_TAG_UNSET_VALUE;
			child->genome2_->tag_value_ = SLIM_TAG_UNSET_VALUE;
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
	
	// mark the child generation as invalid, until it is generated
	child_generation_valid_ = false;
	
	// The parental genomes, which have now been swapped into the child genome vactor, no longer fit the bill.  We need to throw them out and generate new genome vectors.
	if (will_need_new_children)
		GenerateChildrenToFitWF();
}

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
		parent_genomes_.resize(parent_genomes_.size() + (size_t)new_count * 2);
		parent_individuals_.resize(parent_individuals_.size() + new_count);
		
		// in sexual models, females must be put before males and parent_first_male_index_ must be adjusted
		Genome **parent_genome_ptrs = parent_genomes_.data();
		Individual **parent_individual_ptrs = parent_individuals_.data();
		int old_male_count = parent_subpop_size_ - parent_first_male_index_;
		int new_female_count = 0;
		
		// count the new females
		for (int new_index = 0; new_index < new_count; ++new_index)
			if (nonWF_offspring_individuals_[new_index]->sex_ == IndividualSex::kFemale)
				new_female_count++;
		
		// move old males up that many slots to make room; need to fix the index_ ivars of the moved males
		memmove(parent_individual_ptrs + parent_first_male_index_ + new_female_count, parent_individual_ptrs + parent_first_male_index_, old_male_count * sizeof(Individual *));
		memmove(parent_genome_ptrs + (size_t)(parent_first_male_index_ + new_female_count) * 2, parent_genome_ptrs + (size_t)parent_first_male_index_ * 2, (size_t)old_male_count * 2 * sizeof(Genome *));
		
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
			Genome *genome1 = nonWF_offspring_genomes_[(size_t)new_index * 2];
			Genome *genome2 = nonWF_offspring_genomes_[(size_t)new_index * 2 + 1];
			Individual *individual = nonWF_offspring_individuals_[new_index];
			slim_popsize_t insert_index;
			
			if (individual->sex_ == IndividualSex::kFemale)
				insert_index = new_female_position++;
			else
				insert_index = new_male_position++;
			
			individual->index_ = insert_index;
			
			parent_genome_ptrs[(size_t)insert_index * 2] = genome1;
			parent_genome_ptrs[(size_t)insert_index * 2 + 1] = genome2;
			parent_individual_ptrs[insert_index] = individual;
		}
		
		parent_first_male_index_ += new_female_count;
	}
	else
	{
		// reserve space for the new offspring to be merged in
		parent_genomes_.reserve(parent_genomes_.size() + (size_t)new_count * 2);
		parent_individuals_.reserve(parent_individuals_.size() + new_count);
		
		// in hermaphroditic models there is no ordering, so just add new stuff at the end
		for (int new_index = 0; new_index < new_count; ++new_index)
		{
			Genome *genome1 = nonWF_offspring_genomes_[(size_t)new_index * 2];
			Genome *genome2 = nonWF_offspring_genomes_[(size_t)new_index * 2 + 1];
			Individual *individual = nonWF_offspring_individuals_[new_index];
			
			individual->index_ = parent_subpop_size_ + new_index;
			
			parent_genomes_.emplace_back(genome1);
			parent_genomes_.emplace_back(genome2);
			parent_individuals_.emplace_back(individual);
		}
	}
	
	// final cleanup
	parent_subpop_size_ += new_count;
	
	cached_parent_genomes_value_.reset();
	cached_parent_individuals_value_.reset();
	
	nonWF_offspring_genomes_.clear();
	nonWF_offspring_individuals_.clear();
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
				EidosValue_Float_singleton local_fitness(p_fitness);
				EidosValue_Float_singleton local_draw(p_draw);
				
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
							p_surviving = result->LogicalAtIndex(0, nullptr);
							move_destination = nullptr;		// cancel a previously made move decision; T/F says "do not move"
						}
						else if ((result_type == EidosValueType::kValueObject) &&
								 (result->Count() == 1) &&
								 (((EidosValue_Object *)result)->Class() == gSLiM_Subpopulation_Class))
						{
							// a Subpopulation object means the individual should move to that subpop (and live); this is done in a post-pass
							// moving to one's current subpopulation is not-moving; it is equivalent to returning T (i.e., forces survival)
							p_surviving = true;
							
							Subpopulation *destination = (Subpopulation *)result->ObjectElementAtIndex(0, survival_callback->identifier_token_);
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
	Genome **genome_data = parent_genomes_.data();
	Individual **individual_data = parent_individuals_.data();
	int survived_genome_index = 0;
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
		lifetime_reproductive_output_MH_.clear();
		lifetime_reproductive_output_F_.clear();
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
				genome_data[survived_genome_index] = genome_data[(size_t)individual_index * 2];
				genome_data[survived_genome_index + 1] = genome_data[(size_t)individual_index * 2 + 1];
				individual_data[survived_individual_index] = individual;
				
				// fix the individual's index_
				individual_data[survived_individual_index]->index_ = survived_individual_index;
			}
			
			survived_genome_index += 2;
			survived_individual_index++;
		}
		else
		{
			// individuals that do not survive get deallocated, and will be overwritten
			Genome *genome1 = genome_data[(size_t)individual_index * 2];
			Genome *genome2 = genome_data[(size_t)individual_index * 2 + 1];
			
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
			
			FreeSubpopGenome(genome1);
			FreeSubpopGenome(genome2);
			
			individual->~Individual();
			individual_pool_.DisposeChunk(const_cast<Individual *>(individual));
			
			individuals_died = true;
		}
	}
	
	// Then fix our bookkeeping for the first male index, subpop size, caches, etc.
	if (individuals_died)
	{
		parent_subpop_size_ = survived_individual_index;
		
		if (sex_enabled_)
			parent_first_male_index_ -= females_deceased;
		
		parent_genomes_.resize((size_t)parent_subpop_size_ * 2);
		parent_individuals_.resize(parent_subpop_size_);
		
		cached_parent_genomes_value_.reset();
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
	p_ostream << Class()->ClassName() << "<p" << subpopulation_id_ << ">";
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
				cached_value_subpop_id_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(subpopulation_id_));
			return cached_value_subpop_id_;
		}
		case gID_firstMaleIndex:	// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(CurrentFirstMaleIndex()));
		case gID_genomes:
		{
			if (child_generation_valid_)
			{
				if (!cached_child_genomes_value_)
				{
					EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->reserve(child_genomes_.size());
					cached_child_genomes_value_ = EidosValue_SP(vec);
					
					for (auto genome_iter : child_genomes_)
						vec->push_object_element_no_check_NORR(genome_iter);
				}
				/*
				else
				{
					// check that the cache is correct
					EidosValue_Object_vector *vec = cached_child_genomes_value_->ObjectElementVector_Mutable();
					const std::vector<EidosObject *> *vec_direct = vec->ObjectElementVector();
					int vec_size = (int)vec_direct->size();
					
					if (vec_size == (int)child_genomes_.size())
					{
						for (int i = 0; i < vec_size; ++i)
							if ((*vec_direct)[i] != &(child_genomes_[i]))
								EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): value mismatch in cached_child_genomes_value_." << EidosTerminate();
					}
					else
						EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): size mismatch in cached_child_genomes_value_." << EidosTerminate();
				}
				*/
				
				return cached_child_genomes_value_;
			}
			else
			{
				if (!cached_parent_genomes_value_)
				{
					EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->reserve(parent_genomes_.size());
					cached_parent_genomes_value_ = EidosValue_SP(vec);
					
					for (auto genome_iter : parent_genomes_)
						vec->push_object_element_no_check_NORR(genome_iter);
				}
				/*
				else
				{
					// check that the cache is correct
					EidosValue_Object_vector *vec = cached_parent_genomes_value_->ObjectElementVector_Mutable();
					const std::vector<EidosObject *> *vec_direct = vec->ObjectElementVector();
					int vec_size = (int)vec_direct->size();
					
					if (vec_size == (int)parent_genomes_.size())
					{
						for (int i = 0; i < vec_size; ++i)
							if ((*vec_direct)[i] != &(parent_genomes_[i]))
								EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): value mismatch in cached_parent_genomes_value_." << EidosTerminate();
					}
					else
						EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): size mismatch in cached_parent_genomes_value_." << EidosTerminate();
				}
				*/
				
				return cached_parent_genomes_value_;
			}
		}
		case gID_genomesNonNull:
		{
			if (child_generation_valid_)
			{
				EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->reserve(child_genomes_.size());
				
				for (auto genome_iter : child_genomes_)
					if (!genome_iter->IsNull())
						vec->push_object_element_no_check_NORR(genome_iter);
				
				return EidosValue_SP(vec);
			}
			else
			{
				EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->reserve(parent_genomes_.size());
				
				for (auto genome_iter : parent_genomes_)
					if (!genome_iter->IsNull())
						vec->push_object_element_no_check_NORR(genome_iter);
				
				return EidosValue_SP(vec);
			}
		}
		case gID_individuals:
		{
			if (child_generation_valid_)
			{
				slim_popsize_t subpop_size = child_subpop_size_;
				
				// Check for an outdated cache; this should never happen, so we flag it as an error
				if (cached_child_individuals_value_ && (cached_child_individuals_value_->Count() != subpop_size))
					EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): (internal error) cached_child_individuals_value_ out of date." << EidosTerminate();
				
				// Build and return an EidosValue_Object_vector with the current set of individuals in it
				if (!cached_child_individuals_value_)
				{
					EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->reserve(subpop_size);
					cached_child_individuals_value_ = EidosValue_SP(vec);
					
					for (slim_popsize_t individual_index = 0; individual_index < subpop_size; individual_index++)
						vec->push_object_element_no_check_NORR(child_individuals_[individual_index]);
				}
				
				return cached_child_individuals_value_;
			}
			else
			{
				slim_popsize_t subpop_size = parent_subpop_size_;
				
				// Check for an outdated cache; this should never happen, so we flag it as an error
				if (cached_parent_individuals_value_ && (cached_parent_individuals_value_->Count() != subpop_size))
					EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): (internal error) cached_parent_individuals_value_ out of date." << EidosTerminate();
				
				// Build and return an EidosValue_Object_vector with the current set of individuals in it
				if (!cached_parent_individuals_value_)
				{
					EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->reserve(subpop_size);
					cached_parent_individuals_value_ = EidosValue_SP(vec);
					
					for (slim_popsize_t individual_index = 0; individual_index < subpop_size; individual_index++)
						vec->push_object_element_no_check_NORR(parent_individuals_[individual_index]);
				}
				
				return cached_parent_individuals_value_;
			}
		}
		case gID_immigrantSubpopIDs:
		{
			if (model_type_ == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property immigrantSubpopIDs is not available in nonWF models." << EidosTerminate();
			
			EidosValue_Int_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (auto migrant_pair : migrant_fractions_)
				vec->push_int(migrant_pair.first);
			
			return result_SP;
		}
		case gID_immigrantSubpopFractions:
		{
			if (model_type_ == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property immigrantSubpopFractions is not available in nonWF models." << EidosTerminate();
			
			EidosValue_Float_vector *vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector();
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
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(lifetime_rep_count_M + lifetime_rep_count_F);
			
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
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(lifetime_rep_count);
			
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
			EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(lifetime_rep_count);
			
			for (int value_index = 0; value_index < lifetime_rep_count; ++value_index)
				int_result->set_int_no_check(lifetime_rep[value_index], value_index);
			
			return EidosValue_SP(int_result);
		}
		case gID_name:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(name_));
		}
		case gID_description:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(description_));
		}
		case gID_selfingRate:
		{
			if (model_type_ == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property selfingRate is not available in nonWF models." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(selfing_fraction_));
		}
		case gID_cloningRate:
		{
			if (model_type_ == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property cloningRate is not available in nonWF models." << EidosTerminate();
			
			if (sex_enabled_)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{female_clone_fraction_, male_clone_fraction_});
			else
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(female_clone_fraction_));
		}
		case gID_sexRatio:
		{
			if (model_type_ == SLiMModelType::kModelTypeNonWF)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property sexRatio is not available in nonWF models." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(child_generation_valid_ ? child_sex_ratio_ : parent_sex_ratio_));
		}
		case gID_spatialBounds:
		{
			int dimensionality = species_.SpatialDimensionality();
			
			switch (dimensionality)
			{
				case 0: return gStaticEidosValue_Float_ZeroVec;
				case 1: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{bounds_x0_, bounds_x1_});
				case 2: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{bounds_x0_, bounds_y0_, bounds_x1_, bounds_y1_});
				case 3: return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{bounds_x0_, bounds_y0_, bounds_z0_, bounds_x1_, bounds_y1_, bounds_z1_});
				default:	return gStaticEidosValueNULL;	// never hit; here to make the compiler happy
			}
		}
		case gID_spatialMaps:
		{
			EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_SpatialMap_Class))->reserve(spatial_maps_.size());
			
			for (const auto &spatialMapIter : spatial_maps_)
				vec->push_object_element_no_check_RR(spatialMapIter.second);
			
			return EidosValue_SP(vec);
		}
		case gID_species:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(&species_, gSLiM_Species_Class));
		}
		case gID_individualCount:		// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(CurrentSubpopSize()));
			
			// variables
		case gID_tag:					// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Subpopulation::GetProperty): property tag accessed on subpopulation before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
		case gID_fitnessScaling:		// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(subpop_fitness_scaling_));
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue *Subpopulation::GetProperty_Accelerated_id(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Subpopulation *value = (Subpopulation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->subpopulation_id_, value_index);
	}
	
	return int_result;
}

EidosValue *Subpopulation::GetProperty_Accelerated_firstMaleIndex(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Subpopulation *value = (Subpopulation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->CurrentFirstMaleIndex(), value_index);
	}
	
	return int_result;
}

EidosValue *Subpopulation::GetProperty_Accelerated_individualCount(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Subpopulation *value = (Subpopulation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->CurrentSubpopSize(), value_index);
	}
	
	return int_result;
}

EidosValue *Subpopulation::GetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
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
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
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
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
		case gID_fitnessScaling:	// ACCELERATED
		{
			subpop_fitness_scaling_ = p_value.FloatAtIndex(0, nullptr);
			
			if ((subpop_fitness_scaling_ < 0.0) || std::isnan(subpop_fitness_scaling_))
				EIDOS_TERMINATION << "ERROR (Subpopulation::SetProperty): property fitnessScaling must be >= 0.0." << EidosTerminate();
			
			return;
		}
		case gID_name:
		{
			SetName(p_value.StringAtIndex(0, nullptr));
			return;
		}
		case gID_description:
		{
			std::string description = p_value.StringAtIndex(0, nullptr);
			
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
		int64_t source_value = p_source.IntAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Subpopulation *)(p_values[value_index]))->tag_value_ = source_value;
	}
	else
	{
		const int64_t *source_data = p_source.IntVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Subpopulation *)(p_values[value_index]))->tag_value_ = source_data[value_index];
	}
}

void Subpopulation::SetProperty_Accelerated_fitnessScaling(EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex(0, nullptr);
		
		if ((source_value < 0.0) || std::isnan(source_value))
			EIDOS_TERMINATION << "ERROR (Subpopulation::SetProperty_Accelerated_fitnessScaling): property fitnessScaling must be >= 0.0." << EidosTerminate();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Subpopulation *)(p_values[value_index]))->subpop_fitness_scaling_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatVector()->data();
		
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
		case gID_addRecombinant:		return ExecuteMethod_addRecombinant(p_method_id, p_arguments, p_interpreter);
		case gID_addSelfed:				return ExecuteMethod_addSelfed(p_method_id, p_arguments, p_interpreter);
		case gID_removeSubpopulation:	return ExecuteMethod_removeSubpopulation(p_method_id, p_arguments, p_interpreter);
		case gID_takeMigrants:			return ExecuteMethod_takeMigrants(p_method_id, p_arguments, p_interpreter);

		case gID_pointDeviated:			return ExecuteMethod_pointDeviated(p_method_id, p_arguments, p_interpreter);
		case gID_pointInBounds:			return ExecuteMethod_pointInBounds(p_method_id, p_arguments, p_interpreter);
		case gID_pointReflected:		return ExecuteMethod_pointReflected(p_method_id, p_arguments, p_interpreter);
		case gID_pointStopped:			return ExecuteMethod_pointStopped(p_method_id, p_arguments, p_interpreter);
		case gID_pointPeriodic:			return ExecuteMethod_pointPeriodic(p_method_id, p_arguments, p_interpreter);
		case gID_pointUniform:			return ExecuteMethod_pointUniform(p_method_id, p_arguments, p_interpreter);
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
IndividualSex Subpopulation::_GenomeConfigurationForSex(EidosValue *p_sex_value, GenomeType &p_genome1_type, GenomeType &p_genome2_type, bool &p_genome1_null, bool &p_genome2_null)
{
	EidosValueType sex_value_type = p_sex_value->Type();
	IndividualSex sex;
	
	if (sex_enabled_)
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
			std::string sex_string = p_sex_value->StringAtIndex(0, nullptr);
			
			if (sex_string == "M")
				sex = IndividualSex::kMale;
			else if (sex_string == "F")
				sex = IndividualSex::kFemale;
			else
				EIDOS_TERMINATION << "ERROR (Subpopulation::GenomeConfigurationForSex): unrecognized value '" << sex_string << "' for parameter sex." << EidosTerminate();
		}
		else // if (sex_value_type == EidosValueType::kValueFloat)
		{
			double sex_prob = p_sex_value->FloatAtIndex(0, nullptr);
			
			if ((sex_prob >= 0.0) && (sex_prob <= 1.0))
			{
				gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
				
				sex = ((Eidos_rng_uniform(rng) < sex_prob) ? IndividualSex::kMale : IndividualSex::kFemale);
			}
			else
				EIDOS_TERMINATION << "ERROR (Subpopulation::GenomeConfigurationForSex): probability " << sex_prob << " out of range [0.0, 1.0] for parameter sex." << EidosTerminate();
		}
		
		switch (modeled_chromosome_type_)
		{
			case GenomeType::kAutosome:
				p_genome1_type = GenomeType::kAutosome;
				p_genome2_type = GenomeType::kAutosome;
				p_genome1_null = false;
				p_genome2_null = false;
				break;
			case GenomeType::kXChromosome:
				p_genome1_type = GenomeType::kXChromosome;
				p_genome2_type = ((sex == IndividualSex::kMale) ? GenomeType::kYChromosome : GenomeType::kXChromosome);
				p_genome1_null = false;
				p_genome2_null = (sex == IndividualSex::kMale);
				break;
			case GenomeType::kYChromosome:
				p_genome1_type = GenomeType::kXChromosome;
				p_genome2_type = ((sex == IndividualSex::kMale) ? GenomeType::kYChromosome : GenomeType::kXChromosome);
				p_genome1_null = true;
				p_genome2_null = (sex == IndividualSex::kFemale);
				break;
		}
	}
	else
	{
		if (sex_value_type != EidosValueType::kValueNULL)
			EIDOS_TERMINATION << "ERROR (Subpopulation::GenomeConfigurationForSex): sex must be NULL in non-sexual models." << EidosTerminate();
		
		sex = IndividualSex::kHermaphrodite;
		p_genome1_type = GenomeType::kAutosome;
		p_genome2_type = GenomeType::kAutosome;
		p_genome1_null = false;
		p_genome2_null = false;
	}
	
	return sex;
}

//	*********************	– (o<Individual>)addCloned(object<Individual>$ parent, [integer$ count = 1], [logical$ defer = F])
//
EidosValue_SP Subpopulation::ExecuteMethod_addCloned(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): method -addCloned() is not available in WF models." << EidosTerminate();
	
	// TIMING RESTRICTION
	if (community_.CycleStage() != SLiMCycleStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): method -addCloned() may only be called from a reproduction() callback." << EidosTerminate();
	if (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): method -addCloned() may not be called from a nested callback." << EidosTerminate();
	
	// Get and check the first parent (the mother)
	EidosValue *parent_value = p_arguments[0].get();
	Individual *parent = (Individual *)parent_value->ObjectElementAtIndex(0, nullptr);
	IndividualSex parent_sex = parent->sex_;
	Subpopulation &parent_subpop = *parent->subpopulation_;
	
	// SPECIES CONSISTENCY CHECK
	if (&parent_subpop.species_ != &this->species_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): addCloned() requires that parent belongs to the same species as the target subpopulation." << EidosTerminate();
	
	// Check for some other illegal setups
	if (parent->index_ == -1)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): parent must be visible in a subpopulation (i.e., may not be a new juvenile)." << EidosTerminate();
	
	// Check the count and short-circuit if it is zero
	EidosValue *count_value = p_arguments[1].get();
	int64_t child_count = count_value->IntAtIndex(0, nullptr);
	
	if ((child_count < 0) || (child_count > SLIM_MAX_SUBPOP_SIZE))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): addCloned() requires an offspring count >= 0 and <= 1000000000." << EidosTerminate();
	
	EidosValue_Object_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->reserve(child_count);	// reserve enough space for all results
	
	if (child_count == 0)
		return EidosValue_SP(result);
	
	// Determine the sex of the offspring, and the consequent expected genome types
	GenomeType genome1_type = parent->genome1_->Type(), genome2_type = parent->genome2_->Type();
	bool genome1_null = parent->genome1_->IsNull(), genome2_null = parent->genome2_->IsNull();
	IndividualSex child_sex = parent_sex;
	
	// Generate the number of children requested
	Chromosome &chromosome = species_.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	Genome &parent_genome_1 = *parent_subpop.parent_genomes_[2 * (size_t)parent->index_];
	Genome &parent_genome_2 = *parent_subpop.parent_genomes_[2 * (size_t)parent->index_ + 1];
	std::vector<SLiMEidosBlock*> *parent_mutation_callbacks = &parent_subpop.registered_mutation_callbacks_;
	std::vector<SLiMEidosBlock*> &modify_child_callbacks_ = parent_subpop.registered_modify_child_callbacks_;
	
	if (!parent_mutation_callbacks->size()) parent_mutation_callbacks = nullptr;
	
	bool pedigrees_enabled = species_.PedigreesEnabled();
	
	EidosValue *defer_value = p_arguments[2].get();
	bool defer = defer_value->LogicalAtIndex(0, nullptr);
	
	if (defer && parent_mutation_callbacks)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCloned): deferred reproduction cannot be used when mutation() callbacks are enabled." << EidosTerminate();
	
	for (int64_t child_index = 0; child_index < child_count; ++child_index)
	{
		// Make the new individual as a candidate
		Genome *genome1 = genome1_null ? NewSubpopGenome_NULL(genome1_type) : NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, genome1_type);
		Genome *genome2 = genome2_null ? NewSubpopGenome_NULL(genome2_type) : NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, genome2_type);
		Individual *individual = new (individual_pool_.AllocateChunk()) Individual(this, /* index */ -1, genome1, genome2, child_sex, /* age */ 0, /* fitness */ NAN, /* p_mean_parent_age */ parent->age_);
		
		if (pedigrees_enabled)
			individual->TrackParentage_Uniparental(SLiM_GetNextPedigreeID(), *parent);
		
		// TREE SEQUENCE RECORDING
		if (species_.RecordingTreeSequence())
		{
			species_.SetCurrentNewIndividual(individual);
			species_.RecordNewGenome(nullptr, genome1, &parent_genome_1, nullptr);
			species_.RecordNewGenome(nullptr, genome2, &parent_genome_2, nullptr);
		}
		
		// BCH 9/26/2023: inherit the spatial position of the first parent by default, to set up for pointDeviated()
		individual->InheritSpatialPosition(species_.SpatialDimensionality(), parent);
		
		if (defer)
		{
			population_.deferred_reproduction_nonrecombinant_.emplace_back(SLiM_DeferredReproductionType::kClonal, parent, parent, genome1, genome2, child_sex);
		}
		else
		{
			population_.DoClonalMutation(&parent_subpop, *genome1, parent_genome_1, child_sex, parent_mutation_callbacks);
			population_.DoClonalMutation(&parent_subpop, *genome2, parent_genome_2, child_sex, parent_mutation_callbacks);
		}
		
		// Run the candidate past modifyChild() callbacks; the parent subpop's registered callbacks are used
		bool proposed_child_accepted = true;
		
		if (modify_child_callbacks_.size())
		{
			proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, parent, parent, /* p_is_selfing */ false, /* p_is_cloning */ true, /* p_target_subpop */ this, /* p_source_subpop */ &parent_subpop, modify_child_callbacks_);
			
			if (pedigrees_enabled && !proposed_child_accepted)
				individual->RevokeParentage_Uniparental(*parent);
			
			_ProcessNewOffspring(proposed_child_accepted, individual, genome1, genome2, result);
		}
		else
		{
			_ProcessNewOffspring(true, individual, genome1, genome2, result);
		}
		
#if defined(SLIMGUI)
		if (proposed_child_accepted)
		{
			if ((child_sex == IndividualSex::kHermaphrodite) || (child_sex == IndividualSex::kMale))
				gui_offspring_cloned_M_++;
			if ((child_sex == IndividualSex::kHermaphrodite) || (child_sex == IndividualSex::kFemale))
				gui_offspring_cloned_F_++;
			
			// this offspring came from a parent in parent_subpop but ended up here, so it is, in effect, a migrant;
			// we tally things, SLiMgui display purposes, as if it were generated in parent_subpop and then moved
			parent_subpop.gui_premigration_size_++;
			if (&parent_subpop != this)
				gui_migrants_[parent_subpop.subpopulation_id_]++;
		}
#endif
	}
	
	return EidosValue_SP(result);
}

//	*********************	– (o<Individual>)addCrossed(object<Individual>$ parent1, object<Individual>$ parent2, [Nfs$ sex = NULL], [integer$ count = 1], [logical$ defer = F])
//
EidosValue_SP Subpopulation::ExecuteMethod_addCrossed(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): method -addCrossed() is not available in WF models." << EidosTerminate();

	// TIMING RESTRICTION
	if (community_.CycleStage() != SLiMCycleStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): method -addCrossed() may only be called from a reproduction() callback." << EidosTerminate();
	if (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): method -addCrossed() may not be called from a nested callback." << EidosTerminate();
	
	// Get and check the first parent (the mother)
	EidosValue *parent1_value = p_arguments[0].get();
	Individual *parent1 = (Individual *)parent1_value->ObjectElementAtIndex(0, nullptr);
	IndividualSex parent1_sex = parent1->sex_;
	Subpopulation &parent1_subpop = *parent1->subpopulation_;
	
	if ((parent1_sex != IndividualSex::kFemale) && (parent1_sex != IndividualSex::kHermaphrodite))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): parent1 must be female in sexual models (or hermaphroditic in non-sexual models)." << EidosTerminate();
	
	// Get and check the second parent (the father)
	EidosValue *parent2_value = p_arguments[1].get();
	Individual *parent2 = (Individual *)parent2_value->ObjectElementAtIndex(0, nullptr);
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
	int64_t child_count = count_value->IntAtIndex(0, nullptr);
	
	if ((child_count < 0) || (child_count > SLIM_MAX_SUBPOP_SIZE))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): addCrossed() requires an offspring count >= 0 and <= 1000000000." << EidosTerminate();
	
	EidosValue_Object_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->reserve(child_count);	// reserve enough space for all results
	
	if (child_count == 0)
		return EidosValue_SP(result);
	
	// Generate the number of children requested
	Chromosome &chromosome = species_.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	
	std::vector<SLiMEidosBlock*> *parent1_recombination_callbacks = &parent1_subpop.registered_recombination_callbacks_;
	std::vector<SLiMEidosBlock*> *parent2_recombination_callbacks = &parent2_subpop.registered_recombination_callbacks_;
	std::vector<SLiMEidosBlock*> *parent1_mutation_callbacks = &parent1_subpop.registered_mutation_callbacks_;
	std::vector<SLiMEidosBlock*> *parent2_mutation_callbacks = &parent2_subpop.registered_mutation_callbacks_;
	std::vector<SLiMEidosBlock*> &modify_child_callbacks_ = parent1_subpop.registered_modify_child_callbacks_;
	
	if (!parent1_recombination_callbacks->size()) parent1_recombination_callbacks = nullptr;
	if (!parent2_recombination_callbacks->size()) parent2_recombination_callbacks = nullptr;
	if (!parent1_mutation_callbacks->size()) parent1_mutation_callbacks = nullptr;
	if (!parent2_mutation_callbacks->size()) parent2_mutation_callbacks = nullptr;
	
	bool pedigrees_enabled = species_.PedigreesEnabled();
	
	EidosValue *defer_value = p_arguments[4].get();
	bool defer = defer_value->LogicalAtIndex(0, nullptr);
	
	if (defer && (parent1_recombination_callbacks || parent2_recombination_callbacks || parent1_mutation_callbacks || parent2_mutation_callbacks))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addCrossed): deferred reproduction cannot be used when recombination() or mutation() callbacks are enabled." << EidosTerminate();
	
	EidosValue *sex_value = p_arguments[2].get();
	
	for (int64_t child_index = 0; child_index < child_count; ++child_index)
	{
		// Determine the sex of the offspring based on the sex parameter, and the consequent expected genome types
		GenomeType genome1_type, genome2_type;
		bool genome1_null, genome2_null;
		IndividualSex child_sex = _GenomeConfigurationForSex(sex_value, genome1_type, genome2_type, genome1_null, genome2_null);
		
		if (!species_.HasGenetics())
		{
			genome1_null = true;
			genome2_null = true;
			has_null_genomes_ = true;
		}
		else
		{
			if (genome1_null || genome2_null)
				has_null_genomes_ = true;
		}
		
		// Make the new individual as a candidate
		Genome *genome1 = genome1_null ? NewSubpopGenome_NULL(genome1_type) : NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, genome1_type);
		Genome *genome2 = genome2_null ? NewSubpopGenome_NULL(genome2_type) : NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, genome2_type);
		Individual *individual = new (individual_pool_.AllocateChunk()) Individual(this, /* index */ -1, genome1, genome2, child_sex, /* age */ 0, /* fitness */ NAN, /* p_mean_parent_age */ (parent1->age_ + (float)parent2->age_) / 2.0F);
		
		if (pedigrees_enabled)
			individual->TrackParentage_Biparental(SLiM_GetNextPedigreeID(), *parent1, *parent2);
		
		// TREE SEQUENCE RECORDING
		if (species_.RecordingTreeSequence())
			species_.SetCurrentNewIndividual(individual);
		
		// BCH 9/26/2023: inherit the spatial position of the first parent by default, to set up for pointDeviated()
		individual->InheritSpatialPosition(species_.SpatialDimensionality(), parent1);
		
		if (defer)
		{
			population_.deferred_reproduction_nonrecombinant_.emplace_back(SLiM_DeferredReproductionType::kCrossoverMutation, parent1, parent2, genome1, genome2, child_sex);
		}
		else
		{
			population_.DoCrossoverMutation(&parent1_subpop, *genome1, parent1->index_, child_sex, parent1_sex, parent1_recombination_callbacks, parent1_mutation_callbacks);
			population_.DoCrossoverMutation(&parent2_subpop, *genome2, parent2->index_, child_sex, parent2_sex, parent2_recombination_callbacks, parent2_mutation_callbacks);
		}
		
		// Run the candidate past modifyChild() callbacks; the first parent subpop's registered callbacks are used
		bool proposed_child_accepted = true;
		
		if (modify_child_callbacks_.size())
		{
			proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, parent1, parent2, /* p_is_selfing */ false, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, modify_child_callbacks_);
			
			if (pedigrees_enabled && !proposed_child_accepted)
				individual->RevokeParentage_Biparental(*parent1, *parent2);
			
			_ProcessNewOffspring(proposed_child_accepted, individual, genome1, genome2, result);
		}
		else
		{
			_ProcessNewOffspring(true, individual, genome1, genome2, result);
		}
		
#if defined(SLIMGUI)
		if (proposed_child_accepted)
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
	
	return EidosValue_SP(result);
}

//	*********************	– (o<Individual>)addEmpty([Nfs$ sex = NULL], [Nl$ genome1Null = NULL], [Nl$ genome2Null = NULL], [integer$ count = 1])
//
EidosValue_SP Subpopulation::ExecuteMethod_addEmpty(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): method -addEmpty() is not available in WF models." << EidosTerminate();

	// TIMING RESTRICTION
	if (community_.CycleStage() != SLiMCycleStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): method -addEmpty() may only be called from a reproduction() callback." << EidosTerminate();
	if (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): method -addEmpty() may not be called from a nested callback." << EidosTerminate();
	
	// Check the count and short-circuit if it is zero
	EidosValue *count_value = p_arguments[3].get();
	int64_t child_count = count_value->IntAtIndex(0, nullptr);
	
	if ((child_count < 0) || (child_count > SLIM_MAX_SUBPOP_SIZE))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): addEmpty() requires an offspring count >= 0 and <= 1000000000." << EidosTerminate();
	
	EidosValue_Object_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->reserve(child_count);	// reserve enough space for all results
	
	if (child_count == 0)
		return EidosValue_SP(result);
	
	// Generate the number of children requested
	Chromosome &chromosome = species_.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	EidosValue *sex_value = p_arguments[0].get();
	EidosValue *genome1Null_value = p_arguments[1].get();
	EidosValue *genome2Null_value = p_arguments[2].get();
	
	for (int64_t child_index = 0; child_index < child_count; ++child_index)
	{
		GenomeType genome1_type, genome2_type;
		bool genome1_null, genome2_null;
		IndividualSex child_sex = _GenomeConfigurationForSex(sex_value, genome1_type, genome2_type, genome1_null, genome2_null);
		
		if (!species_.HasGenetics())
		{
			genome1_null = true;
			genome2_null = true;
			has_null_genomes_ = true;
			
			if (((genome1Null_value->Type() != EidosValueType::kValueNULL) && !genome1Null_value->LogicalAtIndex(0, nullptr)) ||
				((genome2Null_value->Type() != EidosValueType::kValueNULL) && !genome2Null_value->LogicalAtIndex(0, nullptr)))
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): in a no-genetics species, null genomes are required." << EidosTerminate();
		}
		else
		{
			if (genome1Null_value->Type() != EidosValueType::kValueNULL)
			{
				bool requestedNull = genome1Null_value->LogicalAtIndex(0, nullptr);
				
				if ((requestedNull != genome1_null) && sex_enabled_ && (modeled_chromosome_type_ != GenomeType::kAutosome))
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): when simulating sex chromosomes, which genomes are null is dictated by sex and cannot be changed." << EidosTerminate();
				
				genome1_null = requestedNull;
			}
			
			if (genome2Null_value->Type() != EidosValueType::kValueNULL)
			{
				bool requestedNull = genome2Null_value->LogicalAtIndex(0, nullptr);
				
				if ((requestedNull != genome2_null) && sex_enabled_ && (modeled_chromosome_type_ != GenomeType::kAutosome))
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addEmpty): when simulating sex chromosomes, which genomes are null is dictated by sex and cannot be changed." << EidosTerminate();
				
				genome2_null = requestedNull;
			}
			
			if (genome1_null || genome2_null)
				has_null_genomes_ = true;
		}
		
		// Make the new individual as a candidate
		Genome *genome1 = genome1_null ? NewSubpopGenome_NULL(genome1_type) : NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, genome1_type);
		Genome *genome2 = genome2_null ? NewSubpopGenome_NULL(genome2_type) : NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, genome2_type);
		Individual *individual = new (individual_pool_.AllocateChunk()) Individual(this, /* index */ -1, genome1, genome2, child_sex, /* age */ 0, /* fitness */ NAN, /* p_mean_parent_age */ 0.0F);
		bool pedigrees_enabled = species_.PedigreesEnabled();
		
		if (pedigrees_enabled)
			individual->TrackParentage_Parentless(SLiM_GetNextPedigreeID());
		
		// TREE SEQUENCE RECORDING
		if (species_.RecordingTreeSequence())
		{
			species_.SetCurrentNewIndividual(individual);
			species_.RecordNewGenome(nullptr, genome1, nullptr, nullptr);
			species_.RecordNewGenome(nullptr, genome2, nullptr, nullptr);
		}
		
		// BCH 9/26/2023:  note that there is no parent, so the spatial position of the offspring is left uninitialized.
		// individual->InheritSpatialPosition(species_.spatial_dimensionality_, ???)
		
		// set up empty mutation runs, since we're not calling DoCrossoverMutation() or DoClonalMutation()
#if DEBUG
		genome1->check_cleared_to_nullptr();
		genome2->check_cleared_to_nullptr();
#endif
		
		// We need to add a *different* empty MutationRun to each mutrun index, so each run comes out of
		// the correct per-thread allocation pool.  Would be nice to share these empty runs across
		// multiple calls to addEmpty(), but that's hard now since we don't have refcounts.  How about
		// we maintain a set of empty mutruns, one for each position, in the Species, and whenever we
		// need an empty mutrun we reuse from that pool – after checking that the run is still empty??
		if (!genome1_null || !genome2_null)
		{
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				MutationRunContext &mutrun_context = species_.SpeciesMutationRunContextForMutationRunIndex(run_index);
				const MutationRun *mutrun = MutationRun::NewMutationRun(mutrun_context);
				
				if (!genome1_null)
					genome1->mutruns_[run_index] = mutrun;
				if (!genome2_null)
					genome2->mutruns_[run_index] = mutrun;
			}
		}
		
		// Run the candidate past modifyChild() callbacks; the target subpop's registered callbacks are used
		bool proposed_child_accepted = true;
		
		if (registered_modify_child_callbacks_.size())
		{
			proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, nullptr, nullptr, /* p_is_selfing */ false, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, registered_modify_child_callbacks_);
			
			if (pedigrees_enabled && !proposed_child_accepted)
				individual->RevokeParentage_Parentless();
			
			_ProcessNewOffspring(proposed_child_accepted, individual, genome1, genome2, result);
		}
		else
		{
			_ProcessNewOffspring(true, individual, genome1, genome2, result);
		}
		
#if defined(SLIMGUI)
		if (proposed_child_accepted) gui_offspring_empty_++;
		
		gui_premigration_size_++;
#endif
	}
	
	return EidosValue_SP(result);
}

//	*********************	– (o<Individual>)addRecombinant(No<Genome>$ strand1, No<Genome>$ strand2, Ni breaks1,
//															No<Genome>$ strand3, No<Genome>$ strand4, Ni breaks2,
//															[Nfs$ sex = NULL], [No<Individual>$ parent1 = NULL], [No<Individual>$ parent2 = NULL],
//															[l$ randomizeStrands = F], [integer$ count = 1], [logical$ defer = F])
//
EidosValue_SP Subpopulation::ExecuteMethod_addRecombinant(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): method -addRecombinant() is not available in WF models." << EidosTerminate();

	// TIMING RESTRICTION
	if (community_.CycleStage() != SLiMCycleStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): method -addRecombinant() may only be called from a reproduction() callback." << EidosTerminate();
	if (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): method -addRecombinant() may not be called from a nested callback." << EidosTerminate();
	
	// We could technically make this work in the no-genetics case, if the parameters specify that both child genomes are null, but there's
	// really no reason for anybody to use addRecombinant() in that case, and getting all the logic correct below would be error-prone.
	if (!species_.HasGenetics())
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): method -addRecombinant() may not be called for a no-genetics species; recombination requires genetics." << EidosTerminate();
	
	// Check the count and short-circuit if it is zero
	EidosValue *count_value = p_arguments[10].get();
	int64_t child_count = count_value->IntAtIndex(0, nullptr);
	
	if ((child_count < 0) || (child_count > SLIM_MAX_SUBPOP_SIZE))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): addRecombinant() requires an offspring count >= 0 and <= 1000000000." << EidosTerminate();
	
	EidosValue_Object_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->reserve(child_count);	// reserve enough space for all results
	
	if (child_count == 0)
		return EidosValue_SP(result);
	
	// Note that empty Genome vectors are not legal values for the strandX parameters; the strands must either be NULL or singleton.
	// If strand1 and strand2 are both NULL, breaks1 must be NULL/empty, and the offspring genome will be empty and will not receive mutations.
	// If strand1 is non-NULL and strand2 is NULL, breaks1 must be NULL/empty, and the offspring genome will be cloned with mutations.
	// If strand1 and strand2 are both non-NULL, breaks1 must be non-NULL but need not be sorted, and recombination with mutations will occur.
	// If strand1 is NULL and strand2 is non-NULL, that is presently an error, but may be given a meaning later.
	// The same is true, mutatis mutandis, for strand3, strand4, and breaks2.  The sex parameter is interpreted as in addCrossed().
	// BCH 9/20/2021: For SLiM 3.7, these semantics are changing a little.  Now, when strand1 and strand2 are both NULL and breaks1 is NULL/empty,
	// the offspring genome will be a *null* genome (not just empty), and as before will not receive mutations.  That is the way it always should
	// have worked.  Again, mutatis mutandis, for strand3, strand4, and breaks2.  See https://github.com/MesserLab/SLiM/issues/205.
	EidosValue *strand1_value = p_arguments[0].get();
	EidosValue *strand2_value = p_arguments[1].get();
	EidosValue *breaks1_value = p_arguments[2].get();
	EidosValue *strand3_value = p_arguments[3].get();
	EidosValue *strand4_value = p_arguments[4].get();
	EidosValue *breaks2_value = p_arguments[5].get();
	EidosValue *sex_value = p_arguments[6].get();
	
	// Get the genomes for the supplied strands, or nullptr for NULL
	Genome *strand1 = ((strand1_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Genome *)strand1_value->ObjectElementAtIndex(0, nullptr));
	Genome *strand2 = ((strand2_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Genome *)strand2_value->ObjectElementAtIndex(0, nullptr));
	Genome *strand3 = ((strand3_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Genome *)strand3_value->ObjectElementAtIndex(0, nullptr));
	Genome *strand4 = ((strand4_value->Type() == EidosValueType::kValueNULL) ? nullptr : (Genome *)strand4_value->ObjectElementAtIndex(0, nullptr));
	
	// The parental strands must be visible in the subpopulation, and we need to be able to find them to check their sex
	Individual *strand1_parent = (strand1 ? strand1->individual_ : nullptr);
	Individual *strand2_parent = (strand2 ? strand2->individual_ : nullptr);
	Individual *strand3_parent = (strand3 ? strand3->individual_ : nullptr);
	Individual *strand4_parent = (strand4 ? strand4->individual_ : nullptr);
	
	if ((strand1 && (strand1_parent->index_ == -1)) || (strand2 && (strand2_parent->index_ == -1)) ||
		(strand3 && (strand3_parent->index_ == -1)) || (strand4 && (strand4_parent->index_ == -1)))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): a parental strand is not visible in the subpopulation (i.e., belongs to a new juvenile)." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	if ((strand1_parent && (&strand1_parent->subpopulation_->species_ != &this->species_)) ||
		(strand2_parent && (&strand2_parent->subpopulation_->species_ != &this->species_)) ||
		(strand3_parent && (&strand3_parent->subpopulation_->species_ != &this->species_)) ||
		(strand4_parent && (&strand4_parent->subpopulation_->species_ != &this->species_)))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): addRecombinant() requires that all source genomes belong to the same species as the target subpopulation." << EidosTerminate();
	
	// If both strands are non-NULL for a pair, they must be the same type of genome
	if (strand1 && strand2 && (strand1->Type() != strand2->Type()))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand1 and strand2 are not the same type of genome, and thus cannot recombine." << EidosTerminate();
	if (strand3 && strand4 && (strand3->Type() != strand4->Type()))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand3 and strand4 are not the same type of genome, and thus cannot recombine." << EidosTerminate();
	
	// If the first strand of a pair is NULL, the second must also be NULL
	if (!strand1 && strand2)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): if strand1 is NULL, strand2 must also be NULL." << EidosTerminate();
	if (!strand3 && strand4)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): if strand3 is NULL, strand4 must also be NULL." << EidosTerminate();
	
	// If both pairs have a non-NULL genome, they must both be autosomal or both be non-autosomal (this should never be hit)
	if (strand1 && strand3 &&
		(((strand1->Type() == GenomeType::kAutosome) && (strand3->Type() != GenomeType::kAutosome)) || 
		 ((strand3->Type() == GenomeType::kAutosome) && (strand1->Type() != GenomeType::kAutosome))))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): autosomal genomes cannot be mixed with non-autosomal genomes." << EidosTerminate();
	
	// The first pair cannot be Y chromosomes; those must be supplied in the second pair (if at all)
	if (strand1 && (strand1->Type() == GenomeType::kYChromosome))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): the Y chromosome must be supplied as the second pair of strands in sexual models." << EidosTerminate();
	
	// Figure out what sex we're generating, and what that implies about the offspring genomes
	if ((sex_value->Type() == EidosValueType::kValueNULL) && strand3)
	{
		// NULL can mean "infer the child sex from the strands given"; do that here
		// if strand3 is supplied and is a sex chromosome, it determines the sex of the offspring (strand4 must be NULL or matching type)
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("Subpopulation::ExecuteMethod_addRecombinant(): usage of statics");
		
		static EidosValue_SP static_sex_string_F;
		static EidosValue_SP static_sex_string_M;
		
		if (!static_sex_string_F) static_sex_string_F = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("F"));
		if (!static_sex_string_M) static_sex_string_M = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("M"));
		
		if (strand3->Type() == GenomeType::kXChromosome)
			sex_value = static_sex_string_F.get();
		else if (strand3->Type() == GenomeType::kYChromosome)
			sex_value = static_sex_string_M.get();
	}
	
	// Figure out the parents for purposes of pedigree recording.  	If only one parent was supplied, use it for both,
	// just as we do for cloning and selfing; it makes relatedness() work correctly
	// BCH 9/26/2023 the first parent can now also be used for spatial positioning, even if pedigree tracking is not enabled.
	bool pedigrees_enabled = species_.PedigreesEnabled();
	Individual *pedigree_parent1 = nullptr;
	Individual *pedigree_parent2 = nullptr;
	EidosValue *parent1_value = p_arguments[7].get();
	EidosValue *parent2_value = p_arguments[8].get();
	
	if (parent1_value->Type() != EidosValueType::kValueNULL)
		pedigree_parent1 = (Individual *)parent1_value->ObjectElementAtIndex(0, nullptr);
	if (parent2_value->Type() != EidosValueType::kValueNULL)
		pedigree_parent2 = (Individual *)parent2_value->ObjectElementAtIndex(0, nullptr);
	
	if (pedigree_parent1 && !pedigree_parent2)
		pedigree_parent2 = pedigree_parent1;
	if (pedigree_parent2 && !pedigree_parent1)
		pedigree_parent1 = pedigree_parent2;
	
	// Generate the number of children requested
	Chromosome &chromosome = species_.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	std::vector<SLiMEidosBlock*> *mutation_callbacks = &registered_mutation_callbacks_;
	
	if (!mutation_callbacks->size())
		mutation_callbacks = nullptr;
	
	EidosValue *randomizeStrands_value = p_arguments[9].get();
	bool randomizeStrands = randomizeStrands_value->LogicalAtIndex(0, nullptr);
	
	EidosValue *defer_value = p_arguments[11].get();
	bool defer = defer_value->LogicalAtIndex(0, nullptr);
	
	if (defer && mutation_callbacks)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): deferred reproduction cannot be used when mutation() callbacks are enabled." << EidosTerminate();
	
	for (int64_t child_index = 0; child_index < child_count; ++child_index)
	{
		GenomeType genome1_type, genome2_type;
		bool genome1_null, genome2_null;
		IndividualSex child_sex = _GenomeConfigurationForSex(sex_value, genome1_type, genome2_type, genome1_null, genome2_null);
		
		// Randomly swap initial copy strands, if requested and applicable
		if (randomizeStrands)
		{
			Eidos_RNG_State *rng_state = EIDOS_STATE_RNG(omp_get_thread_num());
			
			if (strand1 && strand2 && Eidos_RandomBool(rng_state))
			{
				std::swap(strand1, strand2);
				std::swap(strand1_parent, strand2_parent);
				//std::swap(strand1_value, strand2_value);		// not used henceforth
			}
			if (strand3 && strand4 && Eidos_RandomBool(rng_state))
			{
				std::swap(strand3, strand4);
				std::swap(strand3_parent, strand4_parent);
				//std::swap(strand3_value, strand4_value);		// not used henceforth
			}
		}
		
		// Check that the chosen sex makes sense with respect to the strands given
		// BCH 9/20/2021: Improved the logic here because in sexual sex-chromosome models the null/nonnull state of the offspring genomes is dictated by the sex.
		if (strand1 && genome1_type != strand1->Type())
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): the type of strand1 does not match the expectation from the sex of the generated offspring." << EidosTerminate();
		if (strand3 && genome2_type != strand3->Type())
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): the type of strand3 does not match the expectation from the sex of the generated offspring." << EidosTerminate();
		
		if (genome1_type != GenomeType::kAutosome)
		{
			if ((genome1_null == true) && strand1)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): the first offspring genome must be a null genome, according to its sex, but a parental genome was supplied for it." << EidosTerminate();
			if ((genome1_null == false) && !strand1)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): the first offspring genome must not be a null genome, according to its sex, but no parental genome was supplied for it." << EidosTerminate();
		}
		if (genome2_type != GenomeType::kAutosome)
		{
			if ((genome2_null == true) && strand3)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): the second offspring genome must be a null genome, according to its sex, but a parental genome was supplied for it." << EidosTerminate();
			if ((genome2_null == false) && !strand3)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): the second offspring genome must not be a null genome, according to its sex, but no parental genome was supplied for it." << EidosTerminate();
		}
		
		// Check that the breakpoint vectors make sense; breakpoints may not be supplied for a NULL pair or a half-NULL pair, but must be supplied for a non-NULL pair
		// BCH 9/20/2021: Added logic here in support of the new semantics that (NULL, NULL, NULL) makes a null genome, not an empty genome
		int breaks1count = breaks1_value->Count(), breaks2count = breaks2_value->Count();
		
		if (!strand1 && !strand2)
		{
			if (breaks1count == 0)
				genome1_null = true;	// note that according to the checks above, if this is required in a sex-chromosome simulation is is already set
			else
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): with a NULL strand1 and strand2, breaks1 must be NULL or empty." << EidosTerminate();
		}
		else if ((breaks1count != 0) && !strand2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): non-empty breaks1 supplied with a NULL strand2; recombination between strand1 and strand2 is not possible, so breaks1 must be NULL or empty." << EidosTerminate();
		
		if (!strand3 && !strand4)
		{
			if (breaks2count == 0)
				genome2_null = true;	// note that according to the checks above, if this is required in a sex-chromosome simulation is is already set
			else
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): with a NULL strand3 and strand4, breaks2 must be NULL or empty." << EidosTerminate();
		}
		else if ((breaks2count != 0) && !strand4)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): non-empty breaks2 supplied with a NULL strand4; recombination between strand3 and strand4 is not possible, so breaks2 must be NULL or empty." << EidosTerminate();
		
		if ((breaks1_value->Type() == EidosValueType::kValueNULL) && strand1 && strand2)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand1 and strand2 are both supplied, so breaks1 may not be NULL (but may be empty)." << EidosTerminate();
		if ((breaks2_value->Type() == EidosValueType::kValueNULL) && strand3 && strand4)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand3 and strand4 are both supplied, so breaks2 may not be NULL (but may be empty)." << EidosTerminate();
		
		if (genome1_null || genome2_null)
			has_null_genomes_ = true;
		
		// Sort and unique and bounds-check the breakpoints
		std::vector<slim_position_t> breakvec1, breakvec2;
		
		if (breaks1count)
		{
			for (int break_index = 0; break_index < breaks1count; ++break_index)
				breakvec1.emplace_back(SLiMCastToPositionTypeOrRaise(breaks1_value->IntAtIndex(break_index, nullptr)));
			
			std::sort(breakvec1.begin(), breakvec1.end());
			breakvec1.erase(unique(breakvec1.begin(), breakvec1.end()), breakvec1.end());
			
			if (breakvec1.back() > chromosome.last_position_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): breaks1 contained a value (" << breakvec1.back() << ") that lies beyond the end of the chromosome." << EidosTerminate();
			
			// handle a breakpoint at position 0, which swaps the initial strand; DoRecombinantMutation() does not like this
			if (breakvec1.front() == 0)
			{
				breakvec1.erase(breakvec1.begin());
				std::swap(strand1, strand2);
				std::swap(strand1_parent, strand2_parent);
				//std::swap(strand1_value, strand2_value);		// not used henceforth
			}
		}
		
		if (breaks2count)
		{
			for (int break_index = 0; break_index < breaks2count; ++break_index)
				breakvec2.emplace_back(SLiMCastToPositionTypeOrRaise(breaks2_value->IntAtIndex(break_index, nullptr)));
			
			std::sort(breakvec2.begin(), breakvec2.end());
			breakvec2.erase(unique(breakvec2.begin(), breakvec2.end()), breakvec2.end());
			
			if (breakvec2.back() > chromosome.last_position_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): breaks2 contained a value (" << breakvec2.back() << ") that lies beyond the end of the chromosome." << EidosTerminate();
			
			// handle a breakpoint at position 0, which swaps the initial strand; DoRecombinantMutation() does not like this
			if (breakvec2.front() == 0)
			{
				breakvec2.erase(breakvec2.begin());
				std::swap(strand3, strand4);
				std::swap(strand3_parent, strand4_parent);
				//std::swap(strand3_value, strand4_value);		// not used henceforth
			}
		}
		
		// Figure out the mean parent age; it is averaged across the mean parent age for each non-null child genome
		float mean_parent_age = 0.0;
		int non_null_count = 0;
		
		if (strand1_parent && strand2_parent)
		{
			mean_parent_age += ((strand1_parent->age_ + (float)strand2_parent->age_) / 2.0F);
			non_null_count++;
		}
		else if (strand1_parent)
		{
			mean_parent_age += strand1_parent->age_;
			non_null_count++;
		}
		else if (strand2_parent)
		{
			mean_parent_age += strand2_parent->age_;
			non_null_count++;
		}
		else
		{
			// this child genome is generated from NULL, nULL for parents, so there is no parent to average the age of
		}
		
		if (strand3_parent && strand4_parent)
		{
			mean_parent_age += ((strand3_parent->age_ + (float)strand4_parent->age_) / 2.0F);
			non_null_count++;
		}
		else if (strand3_parent)
		{
			mean_parent_age += strand3_parent->age_;
			non_null_count++;
		}
		else if (strand4_parent)
		{
			mean_parent_age += strand4_parent->age_;
			non_null_count++;
		}
		else
		{
			// this child genome is generated from NULL, nULL for parents, so there is no parent to average the age of
		}
		
		if (non_null_count > 0)
			mean_parent_age = mean_parent_age / non_null_count;
		
		// Make the new individual as a candidate
		Genome *genome1 = genome1_null ? NewSubpopGenome_NULL(genome1_type) : NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, genome1_type);
		Genome *genome2 = genome2_null ? NewSubpopGenome_NULL(genome2_type) : NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, genome2_type);
		Individual *individual = new (individual_pool_.AllocateChunk()) Individual(this, /* index */ -1, genome1, genome2, child_sex, /* age */ 0, /* fitness */ NAN, mean_parent_age);
		
		if (pedigrees_enabled)
		{
			if (pedigree_parent1 == nullptr)
				individual->TrackParentage_Parentless(SLiM_GetNextPedigreeID());
			else if (pedigree_parent1 == pedigree_parent2)
				individual->TrackParentage_Uniparental(SLiM_GetNextPedigreeID(), *pedigree_parent1);
			else
				individual->TrackParentage_Biparental(SLiM_GetNextPedigreeID(), *pedigree_parent1, *pedigree_parent2);
		}
		
		// TREE SEQUENCE RECORDING
		if (species_.RecordingTreeSequence())
			species_.SetCurrentNewIndividual(individual);
		
		// BCH 9/26/2023: inherit the spatial position of pedigree_parent1 by default, to set up for pointDeviated()
		// Note that, unlike other addX() methods, the first parent is not necessarily defined; in that case, the
		// spatial position of the offspring is left uninitialized.
		if (pedigree_parent1)
			individual->InheritSpatialPosition(species_.SpatialDimensionality(), pedigree_parent1);
		
		// Construct the first child genome, depending upon whether recombination is requested, etc.
		if (strand1)
		{
			if (strand2 && breakvec1.size())
			{
				// determine the sex of the "parent"; we need this to choose the mutation rate map for new mutations
				// if we can't figure out a consistent sex, and we need one because we have separate mutation rate maps, it is an error
				// this seems unlikely to bite anybody, so it is not worth adding another parameter to allow it to be resolved
				IndividualSex parent_sex = IndividualSex::kHermaphrodite;
				
				if (sex_enabled_ && !chromosome.UsingSingleMutationMap())
				{
					if (strand1_parent && strand2_parent)
					{
						if (strand1_parent->sex_ == strand2_parent->sex_)
							parent_sex = strand1_parent->sex_;
					}
					else if (strand1_parent)
						parent_sex = strand1_parent->sex_;
					else if (strand2_parent)
						parent_sex = strand2_parent->sex_;
					
					if (parent_sex == IndividualSex::kHermaphrodite)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand1 and strand2 come from individuals of different sex, and sex-specific mutation rate maps are in use, so it is not clear which mutation rate map to use." << EidosTerminate();
				}
				
				// both strands are non-NULL and we have a breakpoint, so we do recombination between them
				if (species_.RecordingTreeSequence())
					species_.RecordNewGenome(&breakvec1, genome1, strand1, strand2);
				
				if (defer)
				{
					population_.deferred_reproduction_recombinant_.emplace_back(SLiM_DeferredReproductionType::kRecombinant, this, strand1, strand2, breakvec1, genome1, parent_sex);
				}
				else
				{
					population_.DoRecombinantMutation(/* p_mutorigin_subpop */ this, *genome1, strand1, strand2, parent_sex, breakvec1, mutation_callbacks);
				}
			}
			else
			{
				// one strand is non-NULL but the other is NULL, so we clone the non-NULL strand
				if (species_.RecordingTreeSequence())
					species_.RecordNewGenome(nullptr, genome1, strand1, nullptr);
				
				if (defer)
				{
					// clone one genome, using a second strand of nullptr; note that in this case we pass the child sex, not the parent sex
					population_.deferred_reproduction_recombinant_.emplace_back(SLiM_DeferredReproductionType::kRecombinant, this, strand1, nullptr, breakvec1, genome1, child_sex);
				}
				else
				{
					population_.DoClonalMutation(/* p_mutorigin_subpop */ this, *genome1, *strand1, child_sex, mutation_callbacks);
				}
			}
		}
		else
		{
			// both strands are NULL, so we make a null genome; we do nothing but record it
			if (species_.RecordingTreeSequence())
				species_.RecordNewGenome(nullptr, genome1, nullptr, nullptr);
			
#if DEBUG
			if (!genome1_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): (internal error) genome1_null is false with NULL parental strands!" << EidosTerminate();
#endif
		}
		
		// Construct the second child genome, depending upon whether recombination is requested, etc.
		if (strand3)
		{
			if (strand4 && breakvec2.size())
			{
				// determine the sex of the "parent"; we need this to choose the mutation rate map for new mutations
				// if we can't figure out a consistent sex, and we need one because we have separate mutation rate maps, it is an error
				// this seems unlikely to bite anybody, so it is not worth adding another parameter to allow it to be resolved
				IndividualSex parent_sex = IndividualSex::kHermaphrodite;
				
				if (sex_enabled_ && !chromosome.UsingSingleMutationMap())
				{
					if (strand3_parent && strand4_parent)
					{
						if (strand3_parent->sex_ == strand4_parent->sex_)
							parent_sex = strand3_parent->sex_;
					}
					else if (strand3_parent)
						parent_sex = strand3_parent->sex_;
					else if (strand4_parent)
						parent_sex = strand4_parent->sex_;
					
					if (parent_sex == IndividualSex::kHermaphrodite)
						EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): strand3 and strand4 come from individuals of different sex, and sex-specific mutation rate maps are in use, so it is not clear which mutation rate map to use." << EidosTerminate();
				}
				
				// both strands are non-NULL and we have a breakpoint, so we do recombination between them
				if (species_.RecordingTreeSequence())
					species_.RecordNewGenome(&breakvec2, genome2, strand3, strand4);
				
				if (defer)
				{
					population_.deferred_reproduction_recombinant_.emplace_back(SLiM_DeferredReproductionType::kRecombinant, this, strand3, strand4, breakvec2, genome2, parent_sex);
				}
				else
				{
					population_.DoRecombinantMutation(/* p_mutorigin_subpop */ this, *genome2, strand3, strand4, parent_sex, breakvec2, mutation_callbacks);
				}
			}
			else
			{
				// one strand is non-NULL but the other is NULL, so we clone the non-NULL strand
				if (species_.RecordingTreeSequence())
					species_.RecordNewGenome(nullptr, genome2, strand3, nullptr);
				
				if (defer)
				{
					// clone one genome, using a second strand of nullptr; note that in this case we pass the child sex, not the parent sex
					population_.deferred_reproduction_recombinant_.emplace_back(SLiM_DeferredReproductionType::kRecombinant, this, strand3, nullptr, breakvec2, genome2, child_sex);
				}
				else
				{
					population_.DoClonalMutation(/* p_mutorigin_subpop */ this, *genome2, *strand3, child_sex, mutation_callbacks);
				}
			}
		}
		else
		{
			// both strands are NULL, so we make a null genome; we do nothing but record it
			if (species_.RecordingTreeSequence())
				species_.RecordNewGenome(nullptr, genome2, nullptr, nullptr);
			
#if DEBUG
			if (!genome2_null)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addRecombinant): (internal error) genome2_null is false with NULL parental strands!" << EidosTerminate();
#endif
		}
		
		// Run the candidate past modifyChild() callbacks; the target subpop's registered callbacks are used
		bool proposed_child_accepted = true;
		
		if (registered_modify_child_callbacks_.size())
		{
			// BCH 4/5/2022: When removing excess pseudo-parameters from callbacks, we lost a bit of functionality here: we used to pass
			// the four recombinant strands to the callback as the four "parental genomes".  But that was always kind of fictional, and
			// it was never documented, and I doubt anybody was using it, and they can do the same without the modifyChild() callback,
			// so I'm not viewing this loss of functionality as an obstacle to making this change.
			proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, /* p_parent1 */ nullptr, /* p_parent2 */ nullptr, /* p_is_selfing */ false, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ nullptr, registered_modify_child_callbacks_);
			
			if (pedigrees_enabled && !proposed_child_accepted)
			{
				if (pedigree_parent1 == nullptr)
					individual->RevokeParentage_Parentless();
				else if (pedigree_parent1 == pedigree_parent2)
					individual->RevokeParentage_Uniparental(*pedigree_parent1);
				else
					individual->RevokeParentage_Biparental(*pedigree_parent1, *pedigree_parent2);
			}
			
			_ProcessNewOffspring(proposed_child_accepted, individual, genome1, genome2, result);
		}
		else
		{
			_ProcessNewOffspring(true, individual, genome1, genome2, result);
		}
		
#if defined(SLIMGUI)
		if (proposed_child_accepted)
		{
			gui_offspring_crossed_++;
			
			// this offspring came from parents in various subpops but ended up here, so it is, in effect, a migrant;
			// we tally things, for SLiMgui display purposes, as if it were generated in the parental subpops and then moved
			// this is pretty gross, but runs only in SLiMgui, so whatever :->
			Subpopulation *strand1_subpop = (strand1_parent ? strand1_parent->subpopulation_ : nullptr);
			Subpopulation *strand2_subpop = (strand2_parent ? strand2_parent->subpopulation_ : nullptr);
			Subpopulation *strand3_subpop = (strand3_parent ? strand3_parent->subpopulation_ : nullptr);
			Subpopulation *strand4_subpop = (strand4_parent ? strand4_parent->subpopulation_ : nullptr);
			bool both_offspring_strands_inherited = (strand1_subpop && strand3_subpop);
			double strand1_weight = 0.0, strand2_weight = 0.0, strand3_weight = 0.0, strand4_weight = 0.0;
			
			if (strand1_subpop && strand2_subpop)
			{
				strand1_weight = (both_offspring_strands_inherited ? 0.25 : 0.5);
				strand2_weight = (both_offspring_strands_inherited ? 0.25 : 0.5);
			}
			else if (strand1_subpop)
			{
				strand1_weight = (both_offspring_strands_inherited ? 0.5 : 1.0);
			}
			
			if (strand3_subpop && strand4_subpop)
			{
				strand3_weight = (both_offspring_strands_inherited ? 0.25 : 0.5);
				strand4_weight = (both_offspring_strands_inherited ? 0.25 : 0.5);
			}
			else if (strand3_subpop)
			{
				strand3_weight = (both_offspring_strands_inherited ? 0.5 : 1.0);
			}
			
			if (strand1_weight > 0)
			{
				strand1_subpop->gui_premigration_size_ += strand1_weight;
				if (strand1_subpop != this)
					gui_migrants_[strand1_subpop->subpopulation_id_]++;
			}
			if (strand2_weight > 0)
			{
				strand2_subpop->gui_premigration_size_ += strand2_weight;
				if (strand2_subpop != this)
					gui_migrants_[strand2_subpop->subpopulation_id_]++;
			}
			if (strand3_weight > 0)
			{
				strand3_subpop->gui_premigration_size_ += strand3_weight;
				if (strand3_subpop != this)
					gui_migrants_[strand3_subpop->subpopulation_id_]++;
			}
			if (strand4_weight > 0)
			{
				strand4_subpop->gui_premigration_size_ += strand4_weight;
				if (strand4_subpop != this)
					gui_migrants_[strand4_subpop->subpopulation_id_]++;
			}
		}
#endif
	}
	
	return EidosValue_SP(result);
}

//	*********************	– (o<Individual>)addSelfed(object<Individual>$ parent, [integer$ count = 1], [logical$ defer = F])
//
EidosValue_SP Subpopulation::ExecuteMethod_addSelfed(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): method -addSelfed() is not available in WF models." << EidosTerminate();
	
	// TIMING RESTRICTION
	if (community_.CycleStage() != SLiMCycleStage::kNonWFStage1GenerateOffspring)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): method -addSelfed() may only be called from a reproduction() callback." << EidosTerminate();
	if (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosReproductionCallback)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): method -addSelfed() may not be called from a nested callback." << EidosTerminate();
	
	// Get and check the first parent (the mother)
	EidosValue *parent_value = p_arguments[0].get();
	Individual *parent = (Individual *)parent_value->ObjectElementAtIndex(0, nullptr);
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
	int64_t child_count = count_value->IntAtIndex(0, nullptr);
	
	if ((child_count < 0) || (child_count > SLIM_MAX_SUBPOP_SIZE))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): addSelfed() requires an offspring count >= 0 and <= 1000000000." << EidosTerminate();
	
	EidosValue_Object_vector *result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->reserve(child_count);	// reserve enough space for all results
	
	if (child_count == 0)
		return EidosValue_SP(result);
	
	// Determine the sex of the offspring, and the consequent expected genome types; for selfing this is predetermined
	GenomeType genome1_type = GenomeType::kAutosome, genome2_type = GenomeType::kAutosome;
	bool genome1_null, genome2_null;
	IndividualSex child_sex = IndividualSex::kHermaphrodite;
	
	if (!species_.HasGenetics())
	{
		genome1_null = true;
		genome2_null = true;
		has_null_genomes_ = true;
	}
	else
	{
		genome1_null = false;
		genome2_null = false;
	}
	
	// Generate the number of children requested
	Chromosome &chromosome = species_.TheChromosome();
	int32_t mutrun_count = chromosome.mutrun_count_;
	slim_position_t mutrun_length = chromosome.mutrun_length_;
	std::vector<SLiMEidosBlock*> &modify_child_callbacks_ = parent_subpop.registered_modify_child_callbacks_;
	std::vector<SLiMEidosBlock*> *parent_recombination_callbacks = &parent_subpop.registered_recombination_callbacks_;
	std::vector<SLiMEidosBlock*> *parent_mutation_callbacks = &parent_subpop.registered_mutation_callbacks_;
	
	if (!parent_recombination_callbacks->size()) parent_recombination_callbacks = nullptr;
	if (!parent_mutation_callbacks->size()) parent_mutation_callbacks = nullptr;
	
	bool pedigrees_enabled = species_.PedigreesEnabled();
	
	EidosValue *defer_value = p_arguments[2].get();
	bool defer = defer_value->LogicalAtIndex(0, nullptr);
	
	if (defer && (parent_recombination_callbacks || parent_mutation_callbacks))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_addSelfed): deferred reproduction cannot be used when recombination() or mutation() callbacks are enabled." << EidosTerminate();
	
	for (int64_t child_index = 0; child_index < child_count; ++child_index)
	{
		// Make the new individual as a candidate
		Genome *genome1 = genome1_null ? NewSubpopGenome_NULL(genome1_type) : NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, genome1_type);
		Genome *genome2 = genome2_null ? NewSubpopGenome_NULL(genome2_type) : NewSubpopGenome_NONNULL(mutrun_count, mutrun_length, genome2_type);
		Individual *individual = new (individual_pool_.AllocateChunk()) Individual(this, /* index */ -1, genome1, genome2, child_sex, /* age */ 0, /* fitness */ NAN, /* p_mean_parent_age */ parent->age_);
		
		if (pedigrees_enabled)
			individual->TrackParentage_Uniparental(SLiM_GetNextPedigreeID(), *parent);
		
		// TREE SEQUENCE RECORDING
		if (species_.RecordingTreeSequence())
			species_.SetCurrentNewIndividual(individual);
		
		// BCH 9/26/2023: inherit the spatial position of the first parent by default, to set up for pointDeviated()
		individual->InheritSpatialPosition(species_.SpatialDimensionality(), parent);
		
		if (defer)
		{
			population_.deferred_reproduction_nonrecombinant_.emplace_back(SLiM_DeferredReproductionType::kSelfed, parent, parent, genome1, genome2, child_sex);
		}
		else
		{
			population_.DoCrossoverMutation(&parent_subpop, *genome1, parent->index_, child_sex, parent_sex, parent_recombination_callbacks, parent_mutation_callbacks);
			population_.DoCrossoverMutation(&parent_subpop, *genome2, parent->index_, child_sex, parent_sex, parent_recombination_callbacks, parent_mutation_callbacks);
		}
		
		// Run the candidate past modifyChild() callbacks; the parent subpop's registered callbacks are used
		bool proposed_child_accepted = true;
		
		if (modify_child_callbacks_.size())
		{
			proposed_child_accepted = population_.ApplyModifyChildCallbacks(individual, parent, parent, /* p_is_selfing */ true, /* p_is_cloning */ false, /* p_target_subpop */ this, /* p_source_subpop */ &parent_subpop, modify_child_callbacks_);
			
			if (pedigrees_enabled && !proposed_child_accepted)
				individual->RevokeParentage_Uniparental(*parent);
			
			_ProcessNewOffspring(proposed_child_accepted, individual, genome1, genome2, result);
		}
		else
		{
			_ProcessNewOffspring(true, individual, genome1, genome2, result);
		}
		
#if defined(SLIMGUI)
		if (proposed_child_accepted)
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
	
	return EidosValue_SP(result);
}

//	*********************	- (void)takeMigrants(object<Individual> migrants)
//
EidosValue_SP Subpopulation::ExecuteMethod_takeMigrants(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_takeMigrants): method -takeMigrants() is not available in WF models." << EidosTerminate();
	
	// TIMING RESTRICTION
	if (community_.executing_species_ == &species_)
		if ((community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventFirst) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_takeMigrants): method -takeMigrants() must be called directly from a first(), early(), or late() event, when called on the currently executing species." << EidosTerminate();
	
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
	for (int migrant_index = 0; migrant_index < migrant_count; ++migrant_index)
	{
		Individual *migrant = (Individual *)migrants_value->ObjectElementAtIndex(migrant_index, nullptr);
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
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_takeMigrants): method -takeMigrants() may not move an individual that is not visible in a subpopulation.  This error may also occur if you try to migrate the same individual more than once in a single takeMigrants() call (i.e., if the migrants vector is not uniqued)." << EidosTerminate();
			
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
					
					source_subpop->parent_genomes_[(size_t)source_subpop_index * 2] = source_subpop->parent_genomes_[(size_t)(source_first_male - 1) * 2];
					source_subpop->parent_genomes_[(size_t)source_subpop_index * 2 + 1] = source_subpop->parent_genomes_[(size_t)(source_first_male - 1) * 2 + 1];
				}
				
				if (source_first_male - 1 < source_subpop_size - 1)
				{
					Individual *backfill = source_subpop->parent_individuals_[source_subpop_size - 1];
					
					source_subpop->parent_individuals_[source_first_male - 1] = backfill;
					backfill->index_ = source_first_male - 1;
					
					source_subpop->parent_genomes_[(size_t)(source_first_male - 1) * 2] = source_subpop->parent_genomes_[(size_t)(source_subpop_size - 1) * 2];
					source_subpop->parent_genomes_[(size_t)(source_first_male - 1) * 2 + 1] = source_subpop->parent_genomes_[(size_t)(source_subpop_size - 1) * 2 + 1];
				}
				
				source_subpop->parent_subpop_size_ = --source_subpop_size;
				source_subpop->parent_individuals_.resize(source_subpop_size);
				source_subpop->parent_genomes_.resize((size_t)source_subpop_size * 2);
				
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
					
					source_subpop->parent_genomes_[(size_t)source_subpop_index * 2] = source_subpop->parent_genomes_[(size_t)(source_subpop_size - 1) * 2];
					source_subpop->parent_genomes_[(size_t)source_subpop_index * 2 + 1] = source_subpop->parent_genomes_[(size_t)(source_subpop_size - 1) * 2 + 1];
				}
				
				source_subpop->parent_subpop_size_ = --source_subpop_size;
				source_subpop->parent_individuals_.resize(source_subpop_size);
				source_subpop->parent_genomes_.resize((size_t)source_subpop_size * 2);
			}
			
			// insert the migrant into ourselves
			if ((migrant->sex_ == IndividualSex::kFemale) && (parent_first_male_index_ < parent_subpop_size_))
			{
				// room has to be made for females by shifting the first male and changing the first male index
				Individual *backfill = parent_individuals_[parent_first_male_index_];
				
				parent_individuals_.emplace_back(backfill);
				parent_genomes_.emplace_back(parent_genomes_[(size_t)parent_first_male_index_ * 2]);
				parent_genomes_.emplace_back(parent_genomes_[(size_t)parent_first_male_index_ * 2 + 1]);
				backfill->index_ = parent_subpop_size_;
				
				parent_individuals_[parent_first_male_index_] = migrant;
				parent_genomes_[(size_t)parent_first_male_index_ * 2] = migrant->genome1_;
				parent_genomes_[(size_t)parent_first_male_index_ * 2 + 1] = migrant->genome2_;
				migrant->subpopulation_ = this;
				migrant->index_ = parent_first_male_index_;
				
				parent_subpop_size_++;
				parent_first_male_index_++;
			}
			else
			{
				// males and hermaphrodites can just be added to the end; so can females, if no males are present
				parent_individuals_.emplace_back(migrant);
				parent_genomes_.emplace_back(migrant->genome1_);
				parent_genomes_.emplace_back(migrant->genome2_);
				migrant->subpopulation_ = this;
				migrant->index_ = parent_subpop_size_;
				
				parent_subpop_size_++;
				if (migrant->sex_ == IndividualSex::kFemale)
					parent_first_male_index_++;
			}
			
			// set the migrant flag of the migrated individual; note this is not set if the individual was already in the destination subpop
			migrant->migrant_ = true;
			
			moved_count++;
		}
	}
	
	if (moved_count)
	{
		// First, clear our genome and individual caches in all subpopulations; any subpops involved in
		// the migration would be invalidated anyway so this probably isn't even that much overkill in
		// most models.  Note that the child genomes/individuals caches don't need to be thrown away,
		// because they aren't used in nonWF models and this is a nonWF-only method.
		for (auto subpop_pair : population_.subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			
			subpop->cached_parent_genomes_value_.reset();
			subpop->cached_parent_individuals_value_.reset();
		}
		
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
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setMigrationRates): method -setMigrationRates() is not available in nonWF models." << EidosTerminate();
	
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
		
		double migrant_fraction = rates_value->FloatAtIndex(value_index, nullptr);
		
		population_.SetMigration(*this, source_subpop_id, migrant_fraction);
		subpops_seen.emplace_back(source_subpop_id);
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	– (float)pointDeviated(integer$ n, float point, string$ boundary, numeric$ maxDistance, string$ functionType, ...)
//
EidosValue_SP Subpopulation::ExecuteMethod_pointDeviated(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	int dimensionality = species_.SpatialDimensionality();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): pointDeviated() cannot be called in non-spatial simulations." << EidosTerminate();
	
	EidosValue *n_value = p_arguments[0].get();
	int64_t n = n_value->IntAtIndex(0, nullptr);
	
	if (n < 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): pointDeviated() requires n >= 0." << EidosTerminate();
	if (n == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	int64_t length_out = n * dimensionality;
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(length_out);
	double *float_result_data = float_result->data();
	double *float_result_ptr = float_result_data;
	
	EidosValue *point_value = p_arguments[1].get();
	int point_count = point_value->Count();
	const EidosValue_Float_vector *point_vec = (point_count == 1) ? nullptr : point_value->FloatVector();
	double point_singleton = (point_count == 1) ? point_value->FloatAtIndex(0, nullptr) : 0.0;
	const double *point_buf = (point_count == 1) ? &point_singleton : point_vec->data();
	const double *point_buf_ptr = point_buf;
	
	if (point_count % dimensionality != 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): pointDeviated() requires the length of point to be a multiple of the model dimensionality (i.e., point should contain an integer number of complete points of the correct dimensionality)." << EidosTerminate();
	
	point_count /= dimensionality;		// convert from float elements to points
	
	if ((point_count != 1) && ((int64_t)point_count != n))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointDeviated): pointDeviated() requires point to be contain either a single spatial point (to be deviated n times) or n spatial points (each to be deviated once)." << EidosTerminate();
	
	EidosValue_String *boundary_value = (EidosValue_String *)p_arguments[2].get();
	const std::string &boundary_str = boundary_value->StringRefAtIndex(0, nullptr);
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
	double max_distance = maxDistance_value->FloatAtIndex(0, nullptr);
	
	SpatialKernel kernel(dimensionality, max_distance, p_arguments, 4, /* p_expect_max_density */ false);	// uses our arguments starting at index 3
	
	// I'm not going to worry about unrolling each case, for dimensionality by boundary by kernel type; it would
	// be a ton of cases (3 x 5 x 5 = 75), and the overhead for the switches ought to be small compared to the
	// overhead of drawing a displacement from the kernel, which requires a random number draw.  I tested doing
	// a special-case here for dimensionality==2, boundary==1 (stopping), kernel.kernel_type==kNormal,
	// maxDistance==INF, and it clocked at 6.47 seconds versus 7.85 seconds for the unoptimized code below;
	// that's about a 17.6% speedup, which is worthwhile for a handful of special cases like that.  I think
	// normal deviations in 2D with an INF maxDistance are the 95% case, if not 99%; several boundary conditions
	// are common, though.
	if ((dimensionality == 2) && (kernel.kernel_type_ == SpatialKernelType::kNormal) && std::isinf(kernel.max_distance_) && ((boundary == BoundaryCondition::kStopping) || (boundary == BoundaryCondition::kReflecting) || (boundary == BoundaryCondition::kReprising) || ((boundary == BoundaryCondition::kPeriodic) && periodic_x && periodic_y)))
	{
		gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
		double stddev = kernel.kernel_param2_;
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
	
	if ((point_count == 1) && (dimensionality == 1))
	{
		// singleton case, get it out of the way
		double x = point_value->FloatAtIndex(0, nullptr);
		return ((x >= bounds_x0_) && (x <= bounds_x1_)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF;
	}
	
	const EidosValue_Float_vector *float_vec = point_value->FloatVector();
	const double *point_buf = float_vec->data();
	
	if (point_count == 1)
	{
		// single-point case, do it separately to return a singleton logical value, and handle the multi-point case more quickly
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
	eidos_logical_t *logical_result_data = logical_result->data();
	
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
	
	if ((point_count == 1) && (dimensionality == 1))
	{
		// Handle the singleton separately, so we can handle the non-singleton case more quickly
		double x = point_value->FloatAtIndex(0, nullptr);
		
		while (true)
		{
			if (x < bounds_x0_) x = bounds_x0_ + (bounds_x0_ - x);
			else if (x > bounds_x1_) x = bounds_x1_ - (x - bounds_x1_);
			else break;
		}
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x));
	}
	
	// non-singleton general case
	const EidosValue_Float_vector *float_vec = point_value->FloatVector();
	const double *point_buf = float_vec->data();
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(value_count);
	double *float_result_data = float_result->data();
	
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
	
	if ((point_count == 1) && (dimensionality == 1))
	{
		// Handle the singleton separately, so we can handle the non-singleton case more quickly
		double x = point_value->FloatAtIndex(0, nullptr);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(std::max(bounds_x0_, std::min(bounds_x1_, x))));
	}
	
	// non-singleton general case
	const EidosValue_Float_vector *float_vec = point_value->FloatVector();
	const double *point_buf = float_vec->data();
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(value_count);
	double *float_result_data = float_result->data();
	
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
	
	if ((point_count == 1) && (dimensionality == 1))
	{
		// Handle the singleton separately, so we can handle the non-singleton case more quickly
		double x = point_value->FloatAtIndex(0, nullptr);
		
		if (periodic_x)
		{
			while (x < 0.0)			x += bounds_x1_;
			while (x > bounds_x1_)	x -= bounds_x1_;
		}
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(x));
	}
	
	// non-singleton general case
	const EidosValue_Float_vector *float_vec = point_value->FloatVector();
	const double *point_buf = float_vec->data();
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(value_count);
	double *float_result_data = float_result->data();
	
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
	int64_t point_count = n_value->IntAtIndex(0, nullptr);
	
	if (point_count < 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_pointUniform): pointUniform() requires n >= 0." << EidosTerminate();
	if (point_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	int64_t length_out = point_count * dimensionality;
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(length_out);
	double *float_result_data = float_result->data();
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

// WF only:
//	*********************	- (void)setCloningRate(numeric rate)
//
EidosValue_SP Subpopulation::ExecuteMethod_setCloningRate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (model_type_ == SLiMModelType::kModelTypeNonWF)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): method -setCloningRate() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *rate_value = p_arguments[0].get();
	
	int value_count = rate_value->Count();
	
	if (sex_enabled_)
	{
		// SEX ONLY: either one or two values may be specified; if two, it is female at 0, male at 1
		if ((value_count < 1) || (value_count > 2))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setCloningRate): setCloningRate() requires a rate vector containing either one or two values, in sexual simulations." << EidosTerminate();
		
		double female_cloning_fraction = rate_value->FloatAtIndex(0, nullptr);
		double male_cloning_fraction = (value_count == 2) ? rate_value->FloatAtIndex(1, nullptr) : female_cloning_fraction;
		
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
		
		double cloning_fraction = rate_value->FloatAtIndex(0, nullptr);
		
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
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSelfingRate): method -setSelfingRate() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *rate_value = p_arguments[0].get();
	
	double selfing_fraction = rate_value->FloatAtIndex(0, nullptr);
	
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
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): method -setSexRatio() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *sexRatio_value = p_arguments[0].get();
	
	// SetSexRatio() can only be called when the child generation has not yet been generated.  It sets the sex ratio on the child generation,
	// and then that sex ratio takes effect when the children are generated from the parents in EvolveSubpopulation().
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): setSexRatio() called when the child generation was valid." << EidosTerminate();
	
	// SEX ONLY
	if (!sex_enabled_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): setSexRatio() is limited to the sexual case, and cannot be called in asexual simulations." << EidosTerminate();
	
	double sex_ratio = sexRatio_value->FloatAtIndex(0, nullptr);
	
	if ((sex_ratio < 0.0) || (sex_ratio > 1.0) || std::isnan(sex_ratio))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSexRatio): setSexRatio() requires a sex ratio within [0,1] (" << EidosStringForFloat(sex_ratio) << " supplied)." << EidosTerminate();
	
	// After we change the subpop sex ratio, we need to generate new children genomes to fit the new requirements
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
			bounds_x0_ = position_value->FloatAtIndex(0, nullptr);	bounds_x1_ = position_value->FloatAtIndex(1, nullptr);
			
			if (bounds_x1_ <= bounds_x0_)
				bad_bounds = true;
			if (periodic_x && (bounds_x0_ != 0.0))
				bad_periodic_bounds = true;
			
			break;
		case 2:
			bounds_x0_ = position_value->FloatAtIndex(0, nullptr);	bounds_x1_ = position_value->FloatAtIndex(2, nullptr);
			bounds_y0_ = position_value->FloatAtIndex(1, nullptr);	bounds_y1_ = position_value->FloatAtIndex(3, nullptr);
			
			if ((bounds_x1_ <= bounds_x0_) || (bounds_y1_ <= bounds_y0_))
				bad_bounds = true;
			if ((periodic_x && (bounds_x0_ != 0.0)) || (periodic_y && (bounds_y0_ != 0.0)))
				bad_periodic_bounds = true;
			
			break;
		case 3:
			bounds_x0_ = position_value->FloatAtIndex(0, nullptr);	bounds_x1_ = position_value->FloatAtIndex(3, nullptr);
			bounds_y0_ = position_value->FloatAtIndex(1, nullptr);	bounds_y1_ = position_value->FloatAtIndex(4, nullptr);
			bounds_z0_ = position_value->FloatAtIndex(2, nullptr);	bounds_z1_ = position_value->FloatAtIndex(5, nullptr);
			
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
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_setSubpopulationSize): method -setSubpopulationSize() is not available in nonWF models." << EidosTerminate();
	
	EidosValue *size_value = p_arguments[0].get();
	
	slim_popsize_t subpop_size = SLiMCastToPopsizeTypeOrRaise(size_value->IntAtIndex(0, nullptr));
	
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
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_removeSubpopulation): method -removeSubpopulation() is not available in WF models." << EidosTerminate();
	
	// TIMING RESTRICTION
	if (community_.executing_species_ == &species_)
		if ((community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventFirst) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventEarly) && (community_.executing_block_type_ != SLiMEidosBlockType::SLiMEidosEventLate))
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_removeSubpopulation): method -removeSubpopulation() must be called directly from a first(), early(), or late() event, when called on the currently executing species." << EidosTerminate();
	
	population_.RemoveSubpopulation(*this);
	
	return gStaticEidosValueVOID;
}

//	*********************	- (float)cachedFitness(Ni indices)
//
EidosValue_SP Subpopulation::ExecuteMethod_cachedFitness(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *indices_value = p_arguments[0].get();
	
	// This should never be hit, I think; there is no script execution opportunity while the child generation is active
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() may only be called when the parental generation is active (before or during offspring generation)." << EidosTerminate();
	
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
	
	if (index_count == 1)
	{
		slim_popsize_t index = 0;
		
		if (!do_all_indices)
		{
			index = SLiMCastToPopsizeTypeOrRaise(indices_value->IntAtIndex(0, nullptr));
			
			if (index >= parent_subpop_size_)
				EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() index " << index << " out of range." << EidosTerminate();
		}
		
		double fitness = (individual_cached_fitness_OVERRIDE_ ? individual_cached_fitness_OVERRIDE_value_ : parent_individuals_[index]->cached_fitness_UNSAFE_);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(fitness));
	}
	else
	{
		EidosValue_Float_vector *float_return = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(index_count);
		EidosValue_SP result_SP = EidosValue_SP(float_return);
		
		for (slim_popsize_t value_index = 0; value_index < index_count; value_index++)
		{
			slim_popsize_t index = value_index;
			
			if (!do_all_indices)
			{
				index = SLiMCastToPopsizeTypeOrRaise(indices_value->IntAtIndex(value_index, nullptr));
				
				if (index >= parent_subpop_size_)
					EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_cachedFitness): cachedFitness() index " << index << " out of range." << EidosTerminate();
			}
			
			double fitness = (individual_cached_fitness_OVERRIDE_ ? individual_cached_fitness_OVERRIDE_value_ : parent_individuals_[index]->cached_fitness_UNSAFE_);
			
			float_return->set_float_no_check(fitness, value_index);
		}
		
		return result_SP;
	}
}

//  *********************	– (No<Individual>)sampleIndividuals(integer$ size, [logical$ replace = F], [No<Individual>$ exclude = NULL], [Ns$ sex = NULL], [Ni$ tag = NULL], [Ni$ minAge = NULL], [Ni$ maxAge = NULL], [Nl$ migrant = NULL], [Nl$ tagL0 = NULL], [Nl$ tagL1 = NULL], [Nl$ tagL2 = NULL], [Nl$ tagL3 = NULL], [Nl$ tagL4 = NULL])
//
EidosValue_SP Subpopulation::ExecuteMethod_sampleIndividuals(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// This method is patterned closely upon Eidos_ExecuteFunction_sample(), but with no weights vector, and with various ways to narrow down the candidate pool
	EidosValue_SP result_SP(nullptr);
	
	int64_t sample_size = p_arguments[0]->IntAtIndex(0, nullptr);
	bool replace = p_arguments[1]->LogicalAtIndex(0, nullptr);
	int x_count = parent_subpop_size_;
	
	if (sample_size < 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): sampleIndividuals() requires a sample size >= 0 (" << sample_size << " supplied)." << EidosTerminate(nullptr);
	if ((sample_size == 0) || (x_count == 0))
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	// a specific individual may be excluded
	EidosValue *exclude_value = p_arguments[2].get();
	Individual *excluded_individual = nullptr;
	slim_popsize_t excluded_index = -1;
	
	if (exclude_value->Type() != EidosValueType::kValueNULL)
		excluded_individual = (Individual *)exclude_value->ObjectElementAtIndex(0, nullptr);
	
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
		const std::string &sex_string = ((EidosValue_String *)sex_value)->StringRefAtIndex(0, nullptr);
		
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
	slim_usertag_t tag = (tag_specified ? tag_value->IntAtIndex(0, nullptr) : 0);
	
	// an age min or max may be specified in nonWF models
	EidosValue *ageMin_value = p_arguments[5].get();
	EidosValue *ageMax_value = p_arguments[6].get();
	bool ageMin_specified = (ageMin_value->Type() != EidosValueType::kValueNULL);
	bool ageMax_specified = (ageMax_value->Type() != EidosValueType::kValueNULL);
	int64_t ageMin = (ageMin_specified ? ageMin_value->IntAtIndex(0, nullptr) : -1);
	int64_t ageMax = (ageMax_specified ? ageMax_value->IntAtIndex(0, nullptr) : INT64_MAX);
	
	if ((ageMin_specified || ageMax_specified) && (model_type_ != SLiMModelType::kModelTypeNonWF))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_sampleIndividuals): ageMin and ageMax may only be specified in nonWF models." << EidosTerminate(nullptr);
	
	// a migrant value may be specified
	EidosValue *migrant_value = p_arguments[7].get();
	bool migrant_specified = (migrant_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t migrant = (migrant_specified ? migrant_value->LogicalAtIndex(0, nullptr) : false);
	
	// logical tag values, tagL0 - tagL4, may be specified; if so, those tagL values must be defined for all individuals
	EidosValue *tagL0_value = p_arguments[8].get();
	bool tagL0_specified = (tagL0_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL0 = (tagL0_specified ? tagL0_value->LogicalAtIndex(0, nullptr) : false);
	EidosValue *tagL1_value = p_arguments[9].get();
	bool tagL1_specified = (tagL1_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL1 = (tagL1_specified ? tagL1_value->LogicalAtIndex(0, nullptr) : false);
	EidosValue *tagL2_value = p_arguments[10].get();
	bool tagL2_specified = (tagL2_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL2 = (tagL2_specified ? tagL2_value->LogicalAtIndex(0, nullptr) : false);
	EidosValue *tagL3_value = p_arguments[11].get();
	bool tagL3_specified = (tagL3_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL3 = (tagL3_specified ? tagL3_value->LogicalAtIndex(0, nullptr) : false);
	EidosValue *tagL4_value = p_arguments[12].get();
	bool tagL4_specified = (tagL4_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL4 = (tagL4_specified ? tagL4_value->LogicalAtIndex(0, nullptr) : false);
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
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		else if (!replace && (candidate_count < sample_size))
			sample_size = candidate_count;
		
		if (sample_size == 1)
		{
			// a sample size of 1 is very common; make it as fast as we can by getting a singleton EidosValue directly from x
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			int sample_index = (int)Eidos_rng_uniform_int(rng, candidate_count) + first_candidate_index;
			
			if ((excluded_index != -1) && (sample_index >= excluded_index))
				sample_index++;
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(parent_individuals_[sample_index], gSLiM_Individual_Class));
		}
		else if (replace)
		{
			// with replacement, we can just do a series of independent draws
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
			EidosValue_Object_vector *result = ((EidosValue_Object_vector *)result_SP.get())->resize_no_initialize(sample_size);
			EidosObject **object_result_data = result->data();
			
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
			result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
			EidosValue_Object_vector *result = ((EidosValue_Object_vector *)result_SP.get())->resize_no_initialize(sample_size);
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
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(parent_individuals_[sample_index], gSLiM_Individual_Class));
		}
	}
	
	// base case
	{
		// get indices of individuals; we sample from this vector and then look up the corresponding individual
		// see sample() for some discussion of this implementation
		THREAD_SAFETY_IN_ACTIVE_PARALLEL("Subpopulation::ExecuteMethod_sampleIndividuals(): usage of statics");
		
		static int *index_buffer = nullptr;
		static int buffer_capacity = 0;
		
		if (last_candidate_index > buffer_capacity)		// just make it big enough for last_candidate_index, not worth worrying
		{
			buffer_capacity = last_candidate_index * 2;		// double whenever we go over capacity, to avoid reallocations
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
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		else if (!replace && (candidate_count < sample_size))
			sample_size = candidate_count;
		
		// do the sampling
		result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		EidosValue_Object_vector *result = ((EidosValue_Object_vector *)result_SP.get())->resize_no_initialize(sample_size);
		EidosObject **object_result_data = result->data();
		
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
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	// a specific individual may be excluded
	EidosValue *exclude_value = p_arguments[0].get();
	Individual *excluded_individual = nullptr;
	slim_popsize_t excluded_index = -1;
	
	if (exclude_value->Type() != EidosValueType::kValueNULL)
		excluded_individual = (Individual *)exclude_value->ObjectElementAtIndex(0, nullptr);
	
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
		const std::string &sex_string = ((EidosValue_String *)sex_value)->StringRefAtIndex(0, nullptr);
		
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
	slim_usertag_t tag = (tag_specified ? tag_value->IntAtIndex(0, nullptr) : 0);
	
	// an age min or max may be specified in nonWF models
	EidosValue *ageMin_value = p_arguments[3].get();
	EidosValue *ageMax_value = p_arguments[4].get();
	bool ageMin_specified = (ageMin_value->Type() != EidosValueType::kValueNULL);
	bool ageMax_specified = (ageMax_value->Type() != EidosValueType::kValueNULL);
	int64_t ageMin = (ageMin_specified ? ageMin_value->IntAtIndex(0, nullptr) : -1);
	int64_t ageMax = (ageMax_specified ? ageMax_value->IntAtIndex(0, nullptr) : INT64_MAX);
	
	if ((ageMin_specified || ageMax_specified) && (model_type_ != SLiMModelType::kModelTypeNonWF))
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_subsetIndividuals): ageMin and ageMax may only be specified in nonWF models." << EidosTerminate(nullptr);
	
	// a migrant value may be specified
	EidosValue *migrant_value = p_arguments[5].get();
	bool migrant_specified = (migrant_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t migrant = (migrant_specified ? migrant_value->LogicalAtIndex(0, nullptr) : false);
	
	// logical tag values, tagL0 - tagL4, may be specified; if so, those tagL values must be defined for all individuals
	EidosValue *tagL0_value = p_arguments[6].get();
	bool tagL0_specified = (tagL0_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL0 = (tagL0_specified ? tagL0_value->LogicalAtIndex(0, nullptr) : false);
	EidosValue *tagL1_value = p_arguments[7].get();
	bool tagL1_specified = (tagL1_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL1 = (tagL1_specified ? tagL1_value->LogicalAtIndex(0, nullptr) : false);
	EidosValue *tagL2_value = p_arguments[8].get();
	bool tagL2_specified = (tagL2_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL2 = (tagL2_specified ? tagL2_value->LogicalAtIndex(0, nullptr) : false);
	EidosValue *tagL3_value = p_arguments[9].get();
	bool tagL3_specified = (tagL3_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL3 = (tagL3_specified ? tagL3_value->LogicalAtIndex(0, nullptr) : false);
	EidosValue *tagL4_value = p_arguments[10].get();
	bool tagL4_specified = (tagL4_value->Type() != EidosValueType::kValueNULL);
	eidos_logical_t tagL4 = (tagL4_specified ? tagL4_value->LogicalAtIndex(0, nullptr) : false);
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
	
	result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	EidosValue_Object_vector *result = ((EidosValue_Object_vector *)result_SP.get());
	
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
	
	const std::string &map_name = name_value->StringRefAtIndex(0, nullptr);
	
	if (map_name.length() == 0)
		EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_defineSpatialMap): defineSpatialMap() map name must not be zero-length." << EidosTerminate();
	
	const std::string &spatiality_string = spatiality_value->StringRefAtIndex(0, nullptr);
	bool interpolate = interpolate_value->LogicalAtIndex(0, nullptr);
	
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
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(spatial_map, gSLiM_SpatialMap_Class));
}

//	*********************	– (void)addSpatialMap(object<SpatialMap>$ map)
//
EidosValue_SP Subpopulation::ExecuteMethod_addSpatialMap(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *map_value = (EidosValue *)p_arguments[0].get();
	SpatialMap *spatial_map = (SpatialMap *)map_value->ObjectElementAtIndex(0, nullptr);
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
		std::string map_name = map_value->StringAtIndex(0, nullptr);
		
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
		SpatialMap *map = (SpatialMap *)map_value->ObjectElementAtIndex(0, nullptr);
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
	const std::string &map_name = name_value->StringRefAtIndex(0, nullptr);
	
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
	const std::string &map_name = name_value->StringRefAtIndex(0, nullptr);
	
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
		map_name = ((EidosValue_String *)map_value)->StringRefAtIndex(0, nullptr);
		
		if (map_name.length() == 0)
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_spatialMapValue): spatialMapValue() map name must not be zero-length." << EidosTerminate();
	}
	else
	{
		map = (SpatialMap *)map_value->ObjectElementAtIndex(0, nullptr);
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

//	*********************	– (void)outputMSSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [Ns$ filePath = NULL], [logical$ append=F], [logical$ filterMonomorphic = F])
//	*********************	– (void)outputSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [Ns$ filePath = NULL], [logical$ append=F])
//	*********************	– (void)outputVCFSample(integer$ sampleSize, [logical$ replace = T], [string$ requestedSex = "*"], [logical$ outputMultiallelics = T], [Ns$ filePath = NULL], [logical$ append=F], [logical$ simplifyNucleotides = F], [logical$ outputNonnucleotides = T])
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
	
	slim_popsize_t sample_size = SLiMCastToPopsizeTypeOrRaise(sampleSize_value->IntAtIndex(0, nullptr));
	
	bool replace = replace_value->LogicalAtIndex(0, nullptr);
	
	IndividualSex requested_sex;
	
	std::string sex_string = requestedSex_value->StringAtIndex(0, nullptr);
	
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
		output_multiallelics = outputMultiallelics_arg->LogicalAtIndex(0, nullptr);
	
	bool simplify_nucs = false;
	
	if (p_method_id == gID_outputVCFSample)
		simplify_nucs = simplifyNucleotides_arg->LogicalAtIndex(0, nullptr);
	
	bool output_nonnucs = true;
	
	if (p_method_id == gID_outputVCFSample)
		output_nonnucs = outputNonnucleotides_arg->LogicalAtIndex(0, nullptr);
	
	bool filter_monomorphic = false;
	
	if (p_method_id == gID_outputMSSample)
		filter_monomorphic = filterMonomorphic_arg->LogicalAtIndex(0, nullptr);
	
	// Figure out the right output stream
	std::ofstream outfile;
	bool has_file = false;
	std::string outfile_path;
	
	if (filePath_arg->Type() != EidosValueType::kValueNULL)
	{
		outfile_path = Eidos_ResolvedPath(filePath_arg->StringAtIndex(0, nullptr));
		bool append = append_arg->LogicalAtIndex(0, nullptr);
		
		outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		has_file = true;
		
		if (!outfile.is_open())
			EIDOS_TERMINATION << "ERROR (Subpopulation::ExecuteMethod_outputXSample): " << EidosStringRegistry::StringForGlobalStringID(p_method_id) << "() could not open "<< outfile_path << "." << EidosTerminate();
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
		
		if (has_file)
			out << " " << outfile_path;
		
		out << std::endl;
	}
	
	// Call out to produce the actual sample
	if (p_method_id == gID_outputSample)
		population_.PrintSample_SLiM(out, *this, sample_size, replace, requested_sex);
	else if (p_method_id == gID_outputMSSample)
		population_.PrintSample_MS(out, *this, sample_size, replace, requested_sex, species_.TheChromosome(), filter_monomorphic);
	else if (p_method_id == gID_outputVCFSample)
		population_.PrintSample_VCF(out, *this, sample_size, replace, requested_sex, output_multiallelics, simplify_nucs, output_nonnucs);
	
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
		
		double x = center_value->FloatAtIndex(0, nullptr);
		double y = center_value->FloatAtIndex(1, nullptr);
		
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
		double scale = scale_value->FloatAtIndex(0, nullptr);
		
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
		std::string &&color = color_value->StringAtIndex(0, nullptr);
		
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
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomes,						true,	kEidosValueMaskObject, gSLiM_Genome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomesNonNull,					true,	kEidosValueMaskObject, gSLiM_Genome_Class)));
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
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointDeviated, kEidosValueMaskFloat))->AddInt_S(gEidosStr_n)->AddFloat("point")->AddString_S("boundary")->AddNumeric_S(gStr_maxDistance)->AddString_S("functionType")->AddEllipsis());
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointInBounds, kEidosValueMaskLogical))->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointReflected, kEidosValueMaskFloat))->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointStopped, kEidosValueMaskFloat))->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointPeriodic, kEidosValueMaskFloat))->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_pointUniform, kEidosValueMaskFloat))->AddInt_OS(gEidosStr_n, gStaticEidosValue_Integer1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setCloningRate, kEidosValueMaskVOID))->AddNumeric("rate"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSelfingRate, kEidosValueMaskVOID))->AddNumeric_S("rate"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSexRatio, kEidosValueMaskVOID))->AddFloat_S("sexRatio"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSpatialBounds, kEidosValueMaskVOID))->AddNumeric("bounds"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSubpopulationSize, kEidosValueMaskVOID))->AddInt_S("size"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addCloned, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("parent", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddLogical_OS("defer", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addCrossed, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("parent1", gSLiM_Individual_Class)->AddObject_S("parent2", gSLiM_Individual_Class)->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskFloat | kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskOptional, "sex", nullptr, gStaticEidosValueNULL)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddLogical_OS("defer", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addEmpty, kEidosValueMaskObject, gSLiM_Individual_Class))->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskFloat | kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskOptional, "sex", nullptr, gStaticEidosValueNULL)->AddLogical_OSN("genome1Null", gStaticEidosValueNULL)->AddLogical_OSN("genome2Null", gStaticEidosValueNULL)->AddInt_OS("count", gStaticEidosValue_Integer1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addRecombinant, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_SN("strand1", gSLiM_Genome_Class)->AddObject_SN("strand2", gSLiM_Genome_Class)->AddInt_N("breaks1")->AddObject_SN("strand3", gSLiM_Genome_Class)->AddObject_SN("strand4", gSLiM_Genome_Class)->AddInt_N("breaks2")->AddArgWithDefault(kEidosValueMaskNULL | kEidosValueMaskFloat | kEidosValueMaskString | kEidosValueMaskSingleton | kEidosValueMaskOptional, "sex", nullptr, gStaticEidosValueNULL)->AddObject_OSN("parent1", gSLiM_Individual_Class, gStaticEidosValueNULL)->AddObject_OSN("parent2", gSLiM_Individual_Class, gStaticEidosValueNULL)->AddLogical_OS("randomizeStrands", gStaticEidosValue_LogicalF)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddLogical_OS("defer", gStaticEidosValue_LogicalF));
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
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputMSSample, kEidosValueMaskVOID))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("filterMonomorphic", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputVCFSample, kEidosValueMaskVOID))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddLogical_OS("outputMultiallelics", gStaticEidosValue_LogicalT)->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("simplifyNucleotides", gStaticEidosValue_LogicalF)->AddLogical_OS("outputNonnucleotides", gStaticEidosValue_LogicalT));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_outputSample, kEidosValueMaskVOID))->AddInt_S("sampleSize")->AddLogical_OS("replace", gStaticEidosValue_LogicalT)->AddString_OS("requestedSex", gStaticEidosValue_StringAsterisk)->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_configureDisplay, kEidosValueMaskVOID))->AddFloat_ON("center", gStaticEidosValueNULL)->AddFloat_OSN("scale", gStaticEidosValueNULL)->AddString_OSN(gEidosStr_color, gStaticEidosValueNULL));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}




































































