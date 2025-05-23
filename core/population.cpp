//
//  population.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2025 Benjamin C. Haller.  All rights reserved.
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


#include "population.h"

#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <utility>
#include <unordered_map>
#include <ctime>

#include "community.h"
#include "species.h"
#include "slim_globals.h"
#include "eidos_script.h"
#include "eidos_interpreter.h"
#include "eidos_symbol_table.h"
#include "polymorphism.h"
#include "subpopulation.h"
#include "interaction_type.h"

#include "eidos_globals.h"
#if EIDOS_ROBIN_HOOD_HASHING
#include "robin_hood.h"
#elif STD_UNORDERED_MAP_HASHING
#include <unordered_map>
#endif


// the initial capacities for the haplosome and individual pools here are just guesses at balancing low default memory usage, maximizing locality, and minimization of additional allocs
Population::Population(Species &p_species) : model_type_(p_species.model_type_), community_(p_species.community_), species_(p_species), species_haplosome_pool_(p_species.species_haplosome_pool_), species_individual_pool_(p_species.species_individual_pool_)
{
}

Population::~Population(void)
{
	RemoveAllSubpopulationInfo();
	
#ifdef SLIMGUI
	// release malloced storage for SLiMgui statistics collection
	for (auto history_record_iter : fitness_histories_)
	{
		FitnessHistory &history_record = history_record_iter.second;
		
		if (history_record.history_)
		{
			free(history_record.history_);
			history_record.history_ = nullptr;
			history_record.history_length_ = 0;
		}
	}
    
    for (auto history_record_iter : subpop_size_histories_)
	{
		SubpopSizeHistory &history_record = history_record_iter.second;
		
		if (history_record.history_)
		{
			free(history_record.history_);
			history_record.history_ = nullptr;
			history_record.history_length_ = 0;
		}
	}
#endif
	
	// dispose of any freed subpops
	PurgeRemovedSubpopulations();
	
	// dispose of individuals within our junkyard
	for (Individual *individual : species_individuals_junkyard_)
	{
		individual->~Individual();
		species_individual_pool_.DisposeChunk(const_cast<Individual *>(individual));
	}
	species_individuals_junkyard_.clear();
}

void Population::RemoveAllSubpopulationInfo(void)
{
    // BEWARE: do not access sim_ in this method!  This is called from
    // Population::~Population(), at which point sim_ no longer exists!
    
	// Free all subpopulations and then clear out our subpopulation list
	for (auto subpopulation : subpops_)
		delete subpopulation.second;
	
	subpops_.clear();
	
	// Free all substitutions and clear out the substitution vector
	for (auto substitution : substitutions_)
		substitution->Release();
	
	substitutions_.resize(0);
	treeseq_substitutions_map_.clear();
	
	// The malloced storage of the mutation registry will be freed when it is destroyed, but it
	// does not know that the Mutation pointers inside it are owned, so we need to release them.
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	int registry_size;
	const MutationIndex *registry_iter = MutationRegistry(&registry_size);
	const MutationIndex *registry_iter_end = registry_iter + registry_size;
	
	for (; registry_iter != registry_iter_end; ++registry_iter)
		(mut_block_ptr + *registry_iter)->Release();
	
	mutation_registry_.clear();
	
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
	// If we're keeping any separate registries inside mutation types, clear those now as well
    // NOTE: The access of sim_ here is permissible because it will not happen after sim_ has been
    // destructed, due to the clearing of keeping_muttype_registries_ at the end of this block.
    // This block will execute when this method is called directly by Species::~Species(), and
    // then it will not execute again when this method is called by Population::~Population().
    // This design could probably stand to get cleaned up.  FIXME
	if (keeping_muttype_registries_)
	{
		for (auto muttype_iter : species_.MutationTypes())
		{
			MutationType *muttype = muttype_iter.second;
			
			if (muttype->keeping_muttype_registry_)
			{
				muttype->muttype_registry_.clear();
				muttype->keeping_muttype_registry_ = false;
			}
		}
		
		keeping_muttype_registries_ = false;
	}
#endif
	
#ifdef SLIMGUI
	// release malloced storage for SLiMgui statistics collection
	if (mutation_loss_times_)
	{
		free(mutation_loss_times_);
		mutation_loss_times_ = nullptr;
		mutation_loss_tick_slots_ = 0;
	}
	if (mutation_fixation_times_)
	{
		free(mutation_fixation_times_);
		mutation_fixation_times_ = nullptr;
		mutation_fixation_tick_slots_ = 0;
	}
	// Don't throw away the fitness history; it is perfectly valid even if the population has just been changed completely.  It happened.
	// If the read is followed by setting the cycle backward, individual fitness history entries will be invalidated in response.
//	if (fitness_history_)
//	{
//		free(fitness_history_);
//		fitness_history_ = nullptr;
//		fitness_history_length_ = 0;
//	}
#endif
}

// add new empty subpopulation p_subpop_id of size p_subpop_size
Subpopulation *Population::AddSubpopulation(slim_objectid_t p_subpop_id, slim_popsize_t p_subpop_size, double p_initial_sex_ratio, bool p_haploid) 
{ 
	if (community_.SubpopulationIDInUse(p_subpop_id))
		EIDOS_TERMINATION << "ERROR (Population::AddSubpopulation): subpopulation p" << p_subpop_id << " has been used already, and cannot be used again (to prevent conflicts)." << EidosTerminate();
	if ((p_subpop_size < 1) && (model_type_ == SLiMModelType::kModelTypeWF))	// allowed in nonWF models
		EIDOS_TERMINATION << "ERROR (Population::AddSubpopulation): subpopulation p" << p_subpop_id << " empty." << EidosTerminate();
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::AddSubpopulation): (internal error) called with child generation active!." << EidosTerminate();
	
	// make and add the new subpopulation
	Subpopulation *new_subpop = nullptr;
	
	if (species_.SexEnabled())
		new_subpop = new Subpopulation(*this, p_subpop_id, p_subpop_size, true, p_initial_sex_ratio, p_haploid);	// SEX ONLY
	else
		new_subpop = new Subpopulation(*this, p_subpop_id, p_subpop_size, true, p_haploid);
	
#ifdef SLIMGUI
	// When running under SLiMgui, we need to decide whether this subpopulation comes in selected or not.  We can't defer that
	// to SLiMgui's next update, because then mutation tallies are not kept properly up to date, resulting in a bad GUI refresh.
	// The rule is: if all currently existing subpops are selected, then the new subpop comes in selected as well.
    bool gui_all_selected = true;
    
    for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
        if (!subpop_pair.second->gui_selected_)
        {
            gui_all_selected = false;
            break;
        }
    
    new_subpop->gui_selected_ = gui_all_selected;
#endif
	
	subpops_.emplace(p_subpop_id, new_subpop);
	species_.used_subpop_ids_.emplace(p_subpop_id, new_subpop->name_);
	
	// cached mutation counts/frequencies are no longer accurate; mark the cache as invalid
	InvalidateMutationReferencesCache();
	
	return new_subpop;
}

// WF only:
// add new subpopulation p_subpop_id of size p_subpop_size individuals drawn from source subpopulation p_source_subpop_id
Subpopulation *Population::AddSubpopulationSplit(slim_objectid_t p_subpop_id, Subpopulation &p_source_subpop, slim_popsize_t p_subpop_size, double p_initial_sex_ratio)
{
	if (community_.SubpopulationIDInUse(p_subpop_id))
		EIDOS_TERMINATION << "ERROR (Population::AddSubpopulationSplit): subpopulation p" << p_subpop_id << " has been used already, and cannot be used again (to prevent conflicts)." << EidosTerminate();
	if (p_subpop_size < 1)
		EIDOS_TERMINATION << "ERROR (Population::AddSubpopulationSplit): subpopulation p" << p_subpop_id << " empty." << EidosTerminate();
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::AddSubpopulationSplit): (internal error) called with child generation active!." << EidosTerminate();
	
	// make and add the new subpopulation; note that we tell Subpopulation::Subpopulation() not to record tree-seq information
	Subpopulation *new_subpop = nullptr;
 
	if (species_.SexEnabled())
		new_subpop = new Subpopulation(*this, p_subpop_id, p_subpop_size, false, p_initial_sex_ratio, false);	// SEX ONLY
	else
		new_subpop = new Subpopulation(*this, p_subpop_id, p_subpop_size, false, false);
	
#ifdef SLIMGUI
	// When running under SLiMgui, we need to decide whether this subpopulation comes in selected or not.  We can't defer that
	// to SLiMgui's next update, because then mutation tallies are not kept properly up to date, resulting in a bad GUI refresh.
	// The rule is: if all currently existing subpops are selected, then the new subpop comes in selected as well.
    bool gui_all_selected = true;
    
    for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
        if (!subpop_pair.second->gui_selected_)
        {
            gui_all_selected = false;
            break;
        }
    
    new_subpop->gui_selected_ = gui_all_selected;
#endif
	
	subpops_.emplace(p_subpop_id, new_subpop);
	species_.used_subpop_ids_.emplace(p_subpop_id, new_subpop->name_);
	
	// then draw parents from the source population according to fitness, obeying the new subpop's sex ratio
	Subpopulation &subpop = *new_subpop;
	bool recording_tree_sequence = species_.RecordingTreeSequence();
	
	// TREE SEQUENCE RECORDING
	if (recording_tree_sequence)
	{
		// OK, so, this is gross.  The user can call addSubpopSplit(), and the new individuals created as a side effect need to
		// arrive in the tree-seq tables stamped with a later time than their parents, but as far as SLiM is concerned they
		// were created at the same time as the parents; they exist at the same point in time, and it's WF, therefore they
		// were created at the same time, Q.E.D.  So to get the tree-seq code not to object, we add a small offset to the
		// tick counter.  Since addSubpopSplit() could be called multiple times in sequence, splitting a new subpop off
		// from each previous one in a linear fashion, each call to addSubpopSplit() needs to increase this offset slightly.
		// The offset is reset to zero each time the tree sequence's tick counter advances.  This is similar to the timing
		// problem that made us create tree_seq_tick_ in the first place, due to children arriving in the same SLiM tick as
		// their parents, but at least that only happens once per tick in a predictable fashion.
		species_.AboutToSplitSubpop();
	}
	
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	for (slim_popsize_t parent_index = 0; parent_index < subpop.parent_subpop_size_; parent_index++)
	{
		// draw individual from p_source_subpop and assign to be a parent in subpop
		// BCH 4/25/2018: we have to tree-seq record the new individuals and haplosomes here, with the correct parent information
		// Peter observes that biologically, it might make sense for each new haplosome in the split subpop to actually be a
		// clone of the original haplosome in the sense that it has the same parent as the original haplosomes, with the same
		// recombination breakpoints, etc.  But that would be quite hard to implement, especially since the parent in question
		// might have been simplified away already, and that script could have modified the original, and so forth.  Having
		// the new haplosome just inherit exactly from the original seems reasonable enough; for practical purposes it shouldn't
		// matter.
		slim_popsize_t migrant_index;
		
		if (species_.SexEnabled())
		{
			if (parent_index < subpop.parent_first_male_index_)
				migrant_index = p_source_subpop.DrawFemaleParentUsingFitness(rng);
			else
				migrant_index = p_source_subpop.DrawMaleParentUsingFitness(rng);
		}
		else
		{
			migrant_index = p_source_subpop.DrawParentUsingFitness(rng);
		}
		
		// TREE SEQUENCE RECORDING
		if (recording_tree_sequence)
			species_.SetCurrentNewIndividual(subpop.parent_individuals_[parent_index]);
		
		Haplosome **source_individual_haplosomes = p_source_subpop.parent_individuals_[migrant_index]->haplosomes_;
		Haplosome **dest_individual_haplosomes = subpop.parent_individuals_[parent_index]->haplosomes_;
		int haplosome_count_per_individual = species_.HaplosomeCountPerIndividual();
		
		for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
		{
			Haplosome *source_haplosome = source_individual_haplosomes[haplosome_index];
			Haplosome *dest_haplosome = dest_individual_haplosomes[haplosome_index];
			
			dest_haplosome->copy_from_haplosome(*source_haplosome);	// transmogrifies to null if needed
			
			// TREE SEQUENCE RECORDING
			if (recording_tree_sequence)
			{
				if (source_haplosome->IsNull())
					species_.RecordNewHaplosome_NULL(dest_haplosome);
				else
					species_.RecordNewHaplosome(nullptr, 0, dest_haplosome, source_haplosome, nullptr);
			}
		}
	}
	
	// cached mutation counts/frequencies are no longer accurate; mark the cache as invalid
	InvalidateMutationReferencesCache();
	
	// UpdateFitness() is not called here - all fitnesses are kept as equal.  This is because the parents were drawn from the source subpopulation according
	// to their fitness already; fitness has already been applied.  If UpdateFitness() were called, fitness would be double-applied in this cycle.
	
	return new_subpop;
}

// WF only:
// set size of subpopulation p_subpop_id to p_subpop_size
void Population::SetSize(Subpopulation &p_subpop, slim_popsize_t p_subpop_size)
{
	// SetSize() can only be called when the child generation has not yet been generated.  It sets the size on the child generation,
	// and then that size takes effect when the children are generated from the parents in EvolveSubpopulation().
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::SetSize): called when the child generation was valid." << EidosTerminate();
	
	if (p_subpop_size == 0) // remove subpopulation p_subpop_id
	{
		slim_objectid_t subpop_id = p_subpop.subpopulation_id_;
		
		// only remove if we have not already removed
		if (subpops_.count(subpop_id))
		{
			// Note that we don't free the subpopulation here, because there may be live references to it; instead we keep it to the end of the cycle and then free it
			// First we remove the symbol for the subpop
			community_.SymbolTable().RemoveConstantForSymbol(p_subpop.SymbolTableEntry().first);
			
			// Then we immediately remove the subpop from our list of subpops
			subpops_.erase(subpop_id);
			
			for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
				subpop_pair.second->migrant_fractions_.erase(subpop_id);
			
			// remember the subpop for later disposal
			removed_subpops_.emplace_back(&p_subpop);
			
			// cached mutation counts/frequencies are no longer accurate; mark the cache as invalid
			InvalidateMutationReferencesCache();
		}
	}
	else
	{
		// After we change the subpop size, we need to generate new children haplosomes to fit the new requirements
		p_subpop.child_subpop_size_ = p_subpop_size;
		p_subpop.GenerateChildrenToFitWF();
	}
}

// nonWF only:
// remove subpopulation p_subpop_id from the model entirely
void Population::RemoveSubpopulation(Subpopulation &p_subpop)
{
	slim_objectid_t subpop_id = p_subpop.subpopulation_id_;
	
	// only remove if we have not already removed
	if (subpops_.count(subpop_id))
	{
		// Note that we don't free the subpopulation here, because there may be live references to it; instead we keep it to the end of the cycle and then free it
		community_.InvalidateInteractionsForSubpopulation(&p_subpop);
		
		// First we remove the symbol for the subpop
		community_.SymbolTable().RemoveConstantForSymbol(p_subpop.SymbolTableEntry().first);
		
		// Then we immediately remove the subpop from our list of subpops
		subpops_.erase(subpop_id);
		
		// remember the subpop for later disposal
		removed_subpops_.emplace_back(&p_subpop);
		
		// and let it know that it is invalid
		p_subpop.has_been_removed_ = true;
		
		// cached mutation counts/frequencies are no longer accurate; mark the cache as invalid
		InvalidateMutationReferencesCache();
	}
}

// nonWF only:
// move individuals as requested by survival() callbacks
void Population::ResolveSurvivalPhaseMovement(void)
{
	// So, we have a survival() callback that has requested that some individuals move during the viability/survival phase.
	// We want to handle this as efficiently as we can; we could have many individuals moving between subpops in arbitrary
	// ways.  We will remove all moving individuals from their current subpops in a single pass, and then add them to their
	// new subpops in a single pass.  If just one individual is moving, this will be inefficient since the algorithm is O(N)
	// in the number of individuals, but I think it makes sense to optimize for the many-moving case for now.
	bool sex_enabled = species_.SexEnabled();
	
	// mark all individuals in all subpops as not-moving
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
		for (Individual *individual : (subpop_pair.second)->parent_individuals_)
			individual->scratch_ = 0;
	
	// mark moving individuals in all subpops as moving
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
		for (Individual *individual : (subpop_pair.second)->nonWF_survival_moved_individuals_)
			individual->scratch_ = 1;
	
	// loop through subpops and remove all individuals that are leaving, compacting downwards; similar to Subpopulation::ViabilitySurvival()
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
	{ 
		Subpopulation *subpop = subpop_pair.second;
		Individual **individual_data = subpop->parent_individuals_.data();
		int remaining_individual_index = 0;
		int females_leaving = 0;
		bool individuals_leaving = false;
		
		for (int individual_index = 0; individual_index < subpop->parent_subpop_size_; ++individual_index)
		{
			Individual *individual = individual_data[individual_index];
			bool remaining = (individual->scratch_ == 0);
			
			if (remaining)
			{
				// individuals that remain get copied down to the next available slot
				if (remaining_individual_index != individual_index)
				{
					individual_data[remaining_individual_index] = individual;
					
					// fix the individual's index_
					individual_data[remaining_individual_index]->index_ = remaining_individual_index;
				}
				
				remaining_individual_index++;
			}
			else
			{
				// individuals that do not remain get tallied and removed at the end
				if (sex_enabled && (individual->sex_ == IndividualSex::kFemale))
					females_leaving++;
				
				individuals_leaving = true;
			}
		}
		
		// Then fix our bookkeeping for the first male index, subpop size, caches, etc.
		if (individuals_leaving)
		{
			subpop->parent_subpop_size_ = remaining_individual_index;
			
			if (sex_enabled)
				subpop->parent_first_male_index_ -= females_leaving;
			
			subpop->parent_individuals_.resize(subpop->parent_subpop_size_);
			
			subpop->cached_parent_individuals_value_.reset();
		}
	}
	
	// loop through subpops and append individuals that are arriving; we do this using Subpopulation::MergeReproductionOffspring()
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
	{ 
		Subpopulation *subpop = subpop_pair.second;
		
		subpop->nonWF_offspring_individuals_.swap(subpop->nonWF_survival_moved_individuals_);
		
		for (Individual *individual : subpop->nonWF_offspring_individuals_)
		{
#if defined(SLIMGUI)
			// tally this as an incoming migrant for SLiMgui
			++subpop->gui_migrants_[individual->subpopulation_->subpopulation_id_];
#endif
			
			individual->subpopulation_ = subpop;
			individual->migrant_ = true;
		}
		
		subpop->MergeReproductionOffspring();
	}
	
	// Invalidate interactions; we just do this for all subpops, for now, rather than trying to
	// selectively invalidate only the subpops involved in the migrations that occurred
	community_.InvalidateInteractionsForSpecies(&species_);
}

void Population::PurgeRemovedSubpopulations(void)
{
	if (removed_subpops_.size())
	{
		for (auto removed_subpop : removed_subpops_)
			delete removed_subpop;
		
		removed_subpops_.resize(0);
	}
}

#if DEFER_BROKEN
// The "defer" flag is simply disregarded at the moment; its design has rotted away,
// and needs to be remade anew once things have settled down.
void Population::CheckForDeferralInHaplosomesVector(Haplosome **p_haplosomes, size_t p_elements_size, const std::string &p_caller)
{
	if (HasDeferredHaplosomes())
	{
		for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
		{
			Haplosome *haplosome = p_haplosomes[element_index];
			
			if (haplosome->IsDeferred())
				EIDOS_TERMINATION << "ERROR (" << p_caller << "): the mutations of deferred haplosomes cannot be accessed." << EidosTerminate();
		}
	}
}

void Population::CheckForDeferralInHaplosomes(EidosValue_Object *p_haplosomes, const std::string &p_caller)
{
	if (HasDeferredHaplosomes())
	{
		int element_count = p_haplosomes->Count();
		EidosObject * const *haplosomes_data = p_haplosomes->ObjectData();
		
		for (int element_index = 0; element_index < element_count; ++element_index)
		{
			Haplosome *haplosome = (Haplosome *)haplosomes_data[element_index];
			
			if (haplosome->IsDeferred())
				EIDOS_TERMINATION << "ERROR (" << p_caller << "): the mutations of deferred haplosomes cannot be accessed." << EidosTerminate();
		}
	}
}

void Population::CheckForDeferralInIndividualsVector(Individual * const *p_individuals, size_t p_elements_size, const std::string &p_caller)
{
	if (HasDeferredHaplosomes())
	{
		int haplosome_count_per_individual = species_.HaplosomeCountPerIndividual();
		
		for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
		{
			Individual *ind = p_individuals[element_index];
			Haplosome **haplosomes = ind->haplosomes_;
			
			for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
			{
				Haplosome *haplosome = haplosomes[haplosome_index];
				
				if (haplosome->IsDeferred())
					EIDOS_TERMINATION << "ERROR (" << p_caller << "): the mutations of deferred haplosomes cannot be accessed." << EidosTerminate();
			}
		}
	}
}
#endif

// nonWF only:
#if DEFER_BROKEN
// The "defer" flag is simply disregarded at the moment; its design has rotted away,
// and needs to be remade anew once things have settled down.
// This method uses the old reproduction methods, which have been removed from the code base; it needs a complete rework
void Population::DoDeferredReproduction(void)
{
	size_t deferred_count_nonrecombinant = deferred_reproduction_nonrecombinant_.size();
	size_t deferred_count_recombinant = deferred_reproduction_recombinant_.size();
	size_t deferred_count_total = deferred_count_nonrecombinant + deferred_count_recombinant;
	
	if (deferred_count_total == 0)
		return;
	
	// before going parallel, we need to ensure that we have enough capacity in the
	// mutation block; we can't expand it while parallel, due to race conditions
	// see Population::EvolveSubpopulation() for the equivalent WF code
#ifdef _OPENMP
	do {
		int registry_size;
		MutationRegistry(&registry_size);
		size_t est_mutation_block_slots_remaining_PRE = gSLiM_Mutation_Block_Capacity - registry_size;
		double overall_mutation_rate = std::max(species_.chromosome_->overall_mutation_rate_F_, species_.chromosome_->overall_mutation_rate_M_);	// already multiplied by L
		size_t est_slots_needed = (size_t)ceil(2 * deferred_count_total * overall_mutation_rate);	// 2 because diploid, in the worst case
		
		size_t ten_times_demand = 10 * est_slots_needed;
		
		if (est_mutation_block_slots_remaining_PRE <= ten_times_demand)
		{
			SLiM_IncreaseMutationBlockCapacity();
			est_mutation_block_slots_remaining_PRE = gSLiM_Mutation_Block_Capacity - registry_size;
			
			//std::cerr << "Tick " << community_.Tick() << ": DOUBLED CAPACITY ***********************************" << std::endl;
		}
		else
			break;
	} while (true);
#endif
	
	// now generate the haplosomes of the deferred offspring in parallel
	EIDOS_BENCHMARK_START(EidosBenchmarkType::k_DEFERRED_REPRO);
	
	EIDOS_THREAD_COUNT(gEidos_OMP_threads_DEFERRED_REPRO);
#pragma omp parallel for schedule(dynamic, 1) default(none) shared(deferred_count_nonrecombinant) if(deferred_count_nonrecombinant >= EIDOS_OMPMIN_DEFERRED_REPRO) num_threads(thread_count)
	for (size_t deferred_index = 0; deferred_index < deferred_count_nonrecombinant; ++deferred_index)
	{
		SLiM_DeferredReproduction_NonRecombinant &deferred_rec = deferred_reproduction_nonrecombinant_[deferred_index];
		
		if ((deferred_rec.type_ == SLiM_DeferredReproductionType::kCrossoverMutation) || (deferred_rec.type_ == SLiM_DeferredReproductionType::kSelfed))
		{
			DoCrossoverMutation(deferred_rec.parent1_->subpopulation_, *deferred_rec.child_haplosome_1_, deferred_rec.parent1_->index_, deferred_rec.child_sex_, deferred_rec.parent1_->sex_, nullptr, nullptr);
			
			DoCrossoverMutation(deferred_rec.parent2_->subpopulation_, *deferred_rec.child_haplosome_2_, deferred_rec.parent2_->index_, deferred_rec.child_sex_, deferred_rec.parent2_->sex_, nullptr, nullptr);
		}
		else if (deferred_rec.type_ == SLiM_DeferredReproductionType::kClonal)
		{
#warning the use of haplosomes_[0] and haplosomes_[1] here needs to be updated for multichrom
			DoClonalMutation(deferred_rec.parent1_->subpopulation_, *deferred_rec.child_haplosome_1_, *deferred_rec.parent1_->haplosomes_[0], deferred_rec.child_sex_, nullptr);
			
			DoClonalMutation(deferred_rec.parent1_->subpopulation_, *deferred_rec.child_haplosome_2_, *deferred_rec.parent1_->haplosomes_[1], deferred_rec.child_sex_, nullptr);
		}
	}
	
	//EIDOS_THREAD_COUNT(gEidos_OMP_threads_DEFERRED_REPRO);	// this loop shares the same key
#pragma omp parallel for schedule(dynamic, 1) default(none) shared(deferred_count_recombinant) if(deferred_count_recombinant >= EIDOS_OMPMIN_DEFERRED_REPRO) num_threads(thread_count)
	for (size_t deferred_index = 0; deferred_index < deferred_count_recombinant; ++deferred_index)
	{
		SLiM_DeferredReproduction_Recombinant &deferred_rec = deferred_reproduction_recombinant_[deferred_index];
		
		if (deferred_rec.strand2_ == nullptr)
		{
			DoClonalMutation(deferred_rec.mutorigin_subpop_, *deferred_rec.child_haplosome_, *deferred_rec.strand1_, deferred_rec.sex_, nullptr);
		}
		else if (deferred_rec.type_ == SLiM_DeferredReproductionType::kRecombinant)
		{
			DoRecombinantMutation(deferred_rec.mutorigin_subpop_, *deferred_rec.child_haplosome_, deferred_rec.strand1_, deferred_rec.strand2_, deferred_rec.sex_, deferred_rec.break_vec_, nullptr);
		}
	}
	
	EIDOS_BENCHMARK_END(EidosBenchmarkType::k_DEFERRED_REPRO);
	
	// Clear the deferred reproduction queue
	deferred_reproduction_nonrecombinant_.resize(0);
	deferred_reproduction_recombinant_.resize(0);
}
#endif

// WF only:
// set fraction p_migrant_fraction of p_subpop_id that originates as migrants from p_source_subpop_id per cycle  
void Population::SetMigration(Subpopulation &p_subpop, slim_objectid_t p_source_subpop_id, double p_migrant_fraction) 
{ 
	if (subpops_.count(p_source_subpop_id) == 0)
		EIDOS_TERMINATION << "ERROR (Population::SetMigration): no subpopulation p" << p_source_subpop_id << "." << EidosTerminate();
	if ((p_migrant_fraction < 0.0) || (p_migrant_fraction > 1.0) || std::isnan(p_migrant_fraction))
		EIDOS_TERMINATION << "ERROR (Population::SetMigration): migration fraction has to be within [0,1] (" << EidosStringForFloat(p_migrant_fraction) << " supplied)." << EidosTerminate();
	
	if (p_subpop.migrant_fractions_.count(p_source_subpop_id) != 0)
		p_subpop.migrant_fractions_.erase(p_source_subpop_id);
	
	if (p_migrant_fraction > 0.0)	// BCH 4 March 2015: Added this if so we don't put a 0.0 migration rate into the table; harmless but looks bad in SLiMgui...
		p_subpop.migrant_fractions_.emplace(p_source_subpop_id, p_migrant_fraction); 
}

// WF only:
// apply mateChoice() callbacks to a mating event with a chosen first parent; the return is the second parent index, or -1 to force a redraw
slim_popsize_t Population::ApplyMateChoiceCallbacks(slim_popsize_t p_parent1_index, Subpopulation *p_subpop, Subpopulation *p_source_subpop, std::vector<SLiMEidosBlock*> &p_mate_choice_callbacks)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Population::ApplyMateChoiceCallbacks(): running Eidos callback");
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	SLiMEidosBlockType old_executing_block_type = community_.executing_block_type_;
	community_.executing_block_type_ = SLiMEidosBlockType::SLiMEidosMateChoiceCallback;
	
	// We start out using standard weights taken from the source subpopulation.  If, when we are done handling callbacks, we are still
	// using those standard weights, then we can do a draw using our fast lookup tables.  Otherwise, we will do a draw the hard way.
	bool sex_enabled = p_subpop->sex_enabled_;
	double *standard_weights = (sex_enabled ? p_source_subpop->cached_male_fitness_ : p_source_subpop->cached_parental_fitness_);
	double *current_weights = standard_weights;
	slim_popsize_t weights_length = p_source_subpop->cached_fitness_size_;
	bool weights_modified = false;
	Individual *chosen_mate = nullptr;			// callbacks can return an Individual instead of a weights vector, held here
	bool weights_reflect_chosen_mate = false;	// if T, a weights vector has been created with a 1 for the chosen mate, to pass to the next callback
	SLiMEidosBlock *last_interventionist_mate_choice_callback = nullptr;
	
	for (SLiMEidosBlock *mate_choice_callback : p_mate_choice_callbacks)
	{
		if (mate_choice_callback->block_active_)
		{
#if DEBUG_POINTS_ENABLED
			// SLiMgui debugging point
			EidosDebugPointIndent indenter;
			
			{
				EidosInterpreterDebugPointsSet *debug_points = community_.DebugPoints();
				EidosToken *decl_token = mate_choice_callback->root_node_->token_;
				
				if (debug_points && debug_points->set.size() && (decl_token->token_line_ != -1) &&
					(debug_points->set.find(decl_token->token_line_) != debug_points->set.end()))
				{
					SLIM_ERRSTREAM << EidosDebugPointIndent::Indent() << "#DEBUG mateChoice(";
					if (mate_choice_callback->subpopulation_id_ != -1)
						SLIM_ERRSTREAM << "p" << mate_choice_callback->subpopulation_id_;
					SLIM_ERRSTREAM << ")";
					
					if (mate_choice_callback->block_id_ != -1)
						SLIM_ERRSTREAM << " s" << mate_choice_callback->block_id_;
					
					SLIM_ERRSTREAM << " (line " << (decl_token->token_line_ + 1) << community_.DebugPointInfo() << ")" << std::endl;
					indenter.indent();
				}
			}
#endif
			
			// local variables for the callback parameters that we might need to allocate here, and thus need to free below
			EidosValue_SP local_weights_ptr;
			bool redraw_mating = false;
			
			if (chosen_mate && !weights_reflect_chosen_mate && mate_choice_callback->contains_weights_)
			{
				// A previous callback said it wanted a specific individual to be the mate.  We now need to make a weights vector
				// to represent that, since we have another callback that wants an incoming weights vector.
				if (!weights_modified)
				{
					current_weights = (double *)malloc(sizeof(double) * weights_length);	// allocate a new weights vector
					if (!current_weights)
						EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
					weights_modified = true;
				}
				
				EIDOS_BZERO(current_weights, sizeof(double) * weights_length);
				current_weights[chosen_mate->index_] = 1.0;
				
				weights_reflect_chosen_mate = true;
			}
			
			// The callback is active, so we need to execute it; we start a block here to manage the lifetime of the symbol table
			{
				EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &community_.SymbolTable());
				EidosSymbolTable client_symbols(EidosSymbolTableType::kLocalVariablesTable, &callback_symbols);
				EidosFunctionMap &function_map = community_.FunctionMap();
				EidosInterpreter interpreter(mate_choice_callback->compound_statement_node_, client_symbols, function_map, &community_, SLIM_OUTSTREAM, SLIM_ERRSTREAM);
				
				if (mate_choice_callback->contains_self_)
					callback_symbols.InitializeConstantSymbolEntry(mate_choice_callback->SelfSymbolTableEntry());		// define "self"
				
				// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
				// We can use that method because we know the lifetime of the symbol table is shorter than that of
				// the value objects, and we know that the values we are setting here will not change (the objects
				// referred to by the values may change, but the values themselves will not change).
				if (mate_choice_callback->contains_individual_)
				{
					Individual *parent1 = p_source_subpop->parent_individuals_[p_parent1_index];
					callback_symbols.InitializeConstantSymbolEntry(gID_individual, parent1->CachedEidosValue());
				}
				
				if (mate_choice_callback->contains_subpop_)
					callback_symbols.InitializeConstantSymbolEntry(gID_subpop, p_subpop->SymbolTableEntry().second);
				
				if (mate_choice_callback->contains_sourceSubpop_)
					callback_symbols.InitializeConstantSymbolEntry(gID_sourceSubpop, p_source_subpop->SymbolTableEntry().second);
				
				if (mate_choice_callback->contains_weights_)
				{
					local_weights_ptr = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(current_weights, weights_length));
					callback_symbols.InitializeConstantSymbolEntry(gEidosID_weights, local_weights_ptr);
				}
				
				try
				{
					// Interpret the script; the result from the interpretation can be one of several things, so this is a bit complicated
					EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(mate_choice_callback->script_);
					EidosValue *result = result_SP.get();
					EidosValueType result_type = result->Type();
					
					if (result_type == EidosValueType::kValueVOID)
						EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): mateChoice() callbacks must explicitly return a value." << EidosTerminate(mate_choice_callback->identifier_token_);
					else if (result_type == EidosValueType::kValueNULL)
					{
						// NULL indicates that the mateChoice() callback did not wish to alter the weights, so we do nothing
					}
					else if (result_type == EidosValueType::kValueObject)
					{
						// A singleton vector of type Individual may be returned to choose a specific mate
						if ((result->Count() == 1) && (((EidosValue_Object *)result)->Class() == gSLiM_Individual_Class))
						{
#if DEBUG
							// this checks the value type at runtime
							chosen_mate = (Individual *)result->ObjectData()[0];
#else
							// unsafe cast for speed
							chosen_mate = (Individual *)((EidosValue_Object *)result)->data()[0];
#endif
							weights_reflect_chosen_mate = false;
							
							// remember this callback for error attribution below
							last_interventionist_mate_choice_callback = mate_choice_callback;
						}
						else
						{
							EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): invalid return value for mateChoice() callback." << EidosTerminate(mate_choice_callback->identifier_token_);
						}
					}
					else if (result_type == EidosValueType::kValueFloat)
					{
						int result_count = result->Count();
						
						if (result_count == 0)
						{
							// a return of float(0) indicates that there is no acceptable mate for the first parent; the first parent must be redrawn
							redraw_mating = true;
						}
						else if (result_count == weights_length)
						{
							// if we used to have a specific chosen mate, we don't any more
							chosen_mate = nullptr;
							weights_reflect_chosen_mate = false;
							
							// a non-zero float vector must match the size of the source subpop, and provides a new set of weights for us to use
							if (!weights_modified)
							{
								current_weights = (double *)malloc(sizeof(double) * weights_length);	// allocate a new weights vector
								if (!current_weights)
									EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
								weights_modified = true;
							}
							
							// use FloatData() to get the values, copy them with memcpy()
							memcpy(current_weights, result->FloatData(), sizeof(double) * weights_length);
							
							// remember this callback for error attribution below
							last_interventionist_mate_choice_callback = mate_choice_callback;
						}
						else
						{
							EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): invalid return value for mateChoice() callback." << EidosTerminate(mate_choice_callback->identifier_token_);
						}
					}
					else
					{
						EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): invalid return value for mateChoice() callback." << EidosTerminate(mate_choice_callback->identifier_token_);
					}
				}
				catch (...)
				{
					throw;
				}
			}
			
			// If this callback told us not to generate the child, we do not call the rest of the callback chain; we're done
			if (redraw_mating)
			{
				if (weights_modified)
					free(current_weights);
				
				community_.executing_block_type_ = old_executing_block_type;
				
#if (SLIMPROFILING == 1)
				// PROFILING
				SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMateChoiceCallback)]);
#endif
				
				return -1;
			}
		}
	}
	
	// If we have a specific chosen mate, then we don't need to draw, but we do need to check the sex of the proposed mate
	if (chosen_mate)
	{
		slim_popsize_t drawn_parent = chosen_mate->index_;
		
		if (weights_modified)
			free(current_weights);
		
		if (sex_enabled)
		{
			if (drawn_parent < p_source_subpop->parent_first_male_index_)
				EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): second parent chosen by mateChoice() callback is female." << EidosTerminate(last_interventionist_mate_choice_callback->identifier_token_);
		}
		
		community_.executing_block_type_ = old_executing_block_type;
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMateChoiceCallback)]);
#endif
		
		return drawn_parent;
	}
	
	// If a callback supplied a different set of weights, we need to use those weights to draw a male parent
	if (weights_modified)
	{
		slim_popsize_t drawn_parent = -1;
		double weights_sum = 0;
		int positive_count = 0;
		
		// first we assess the weights vector: get its sum, bounds-check it, etc.
		for (slim_popsize_t weight_index = 0; weight_index < weights_length; ++weight_index)
		{
			double x = current_weights[weight_index];
			
			if (!std::isfinite(x))
				EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): weight returned by mateChoice() callback is not finite." << EidosTerminate(last_interventionist_mate_choice_callback->identifier_token_);
			
			if (x > 0.0)
			{
				positive_count++;
				weights_sum += x;
				continue;
			}
			
			if (x < 0.0)
				EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): weight returned by mateChoice() callback is less than 0.0." << EidosTerminate(last_interventionist_mate_choice_callback->identifier_token_);
		}
		
		if (weights_sum <= 0.0)
		{
			// We used to consider this an error; now we consider it to represent the first parent having no acceptable choice, so we
			// re-draw.  Returning float(0) is essentially equivalent, except that it short-circuits the whole mateChoice() callback
			// chain, whereas returning a vector of 0 values can be modified by a downstream mateChoice() callback.  Usually that is
			// not an important distinction.  Returning float(0) is faster in principle, but if one is already constructing a vector
			// of weights that can simply end up being all zero, then this path is much easier.  BCH 5 March 2017
			//EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): weights returned by mateChoice() callback sum to 0.0 or less." << EidosTerminate(last_interventionist_mate_choice_callback->identifier_token_);
			free(current_weights);
			
			community_.executing_block_type_ = old_executing_block_type;
			
#if (SLIMPROFILING == 1)
			// PROFILING
			SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMateChoiceCallback)]);
#endif
			
			return -1;
		}
		
		// then we draw from the weights vector
		if (positive_count == 1)
		{
			// there is only a single positive value, so the callback has chosen a parent for us; we just need to locate it
			// we could have noted it above, but I don't want to slow down that loop, since many positive weights is the likely case
			for (slim_popsize_t weight_index = 0; weight_index < weights_length; ++weight_index)
				if (current_weights[weight_index] > 0.0)
				{
					drawn_parent = weight_index;
					break;
				}
		}
		else if (positive_count <= weights_length / 4)	// the threshold here is a guess
		{
			// there are just a few positive values, so try to be faster about scanning for them by checking for zero first
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			double the_rose_in_the_teeth = Eidos_rng_uniform_pos(rng) * weights_sum;
			double bachelor_sum = 0.0;
			
			for (slim_popsize_t weight_index = 0; weight_index < weights_length; ++weight_index)
			{
				double weight = current_weights[weight_index];
				
				if (weight > 0.0)
				{
					bachelor_sum += weight;
					
					if (the_rose_in_the_teeth <= bachelor_sum)
					{
						drawn_parent = weight_index;
						break;
					}
				}
			}
		}
		else
		{
			// there are many positive values, so we need to do a uniform draw and see who gets the rose
			gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
			double the_rose_in_the_teeth = Eidos_rng_uniform_pos(rng) * weights_sum;
			double bachelor_sum = 0.0;
			
			for (slim_popsize_t weight_index = 0; weight_index < weights_length; ++weight_index)
			{
				bachelor_sum += current_weights[weight_index];
				
				if (the_rose_in_the_teeth <= bachelor_sum)
				{
					drawn_parent = weight_index;
					break;
				}
			}
		}
		
		// we should always have a chosen parent at this point
		if (drawn_parent == -1)
			EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): failed to choose a mate." << EidosTerminate(last_interventionist_mate_choice_callback->identifier_token_);
		
		free(current_weights);
		
		if (sex_enabled)
		{
			if (drawn_parent < p_source_subpop->parent_first_male_index_)
				EIDOS_TERMINATION << "ERROR (Population::ApplyMateChoiceCallbacks): second parent chosen by mateChoice() callback is female." << EidosTerminate(last_interventionist_mate_choice_callback->identifier_token_);
		}
		
		community_.executing_block_type_ = old_executing_block_type;
		
#if (SLIMPROFILING == 1)
		// PROFILING
		SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMateChoiceCallback)]);
#endif
		
		return drawn_parent;
	}
	
	community_.executing_block_type_ = old_executing_block_type;
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosMateChoiceCallback)]);
#endif
	
	// The standard behavior, with no active callbacks, is to draw a male parent using the standard fitness values
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	return (sex_enabled ? p_source_subpop->DrawMaleParentUsingFitness(rng) : p_source_subpop->DrawParentUsingFitness(rng));
}

// apply modifyChild() callbacks to a generated child; a return of false means "do not use this child, generate a new one"
bool Population::ApplyModifyChildCallbacks(Individual *p_child, Individual *p_parent1, Individual *p_parent2, bool p_is_selfing, bool p_is_cloning, Subpopulation *p_target_subpop, Subpopulation *p_source_subpop, std::vector<SLiMEidosBlock*> &p_modify_child_callbacks)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Population::ApplyModifyChildCallbacks(): running Eidos callback");
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	// note the focal child during the callback, so we can prevent illegal operations during the callback
	SLiMEidosBlockType old_executing_block_type = community_.executing_block_type_;
	community_.executing_block_type_ = SLiMEidosBlockType::SLiMEidosModifyChildCallback;
	community_.focal_modification_child_ = p_child;
	
	for (SLiMEidosBlock *modify_child_callback : p_modify_child_callbacks)
	{
		if (modify_child_callback->block_active_)
		{
#if DEBUG_POINTS_ENABLED
			// SLiMgui debugging point
			EidosDebugPointIndent indenter;
			
			{
				EidosInterpreterDebugPointsSet *debug_points = community_.DebugPoints();
				EidosToken *decl_token = modify_child_callback->root_node_->token_;
				
				if (debug_points && debug_points->set.size() && (decl_token->token_line_ != -1) &&
					(debug_points->set.find(decl_token->token_line_) != debug_points->set.end()))
				{
					SLIM_ERRSTREAM << EidosDebugPointIndent::Indent() << "#DEBUG modifyChild(";
					if (modify_child_callback->subpopulation_id_ != -1)
						SLIM_ERRSTREAM << "p" << modify_child_callback->subpopulation_id_;
					SLIM_ERRSTREAM << ")";
					
					if (modify_child_callback->block_id_ != -1)
						SLIM_ERRSTREAM << " s" << modify_child_callback->block_id_;
					
					SLIM_ERRSTREAM << " (line " << (decl_token->token_line_ + 1) << community_.DebugPointInfo() << ")" << std::endl;
					indenter.indent();
				}
			}
#endif
			
			// The callback is active, so we need to execute it
			EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &community_.SymbolTable());
			EidosSymbolTable client_symbols(EidosSymbolTableType::kLocalVariablesTable, &callback_symbols);
			EidosFunctionMap &function_map = community_.FunctionMap();
			EidosInterpreter interpreter(modify_child_callback->compound_statement_node_, client_symbols, function_map, &community_, SLIM_OUTSTREAM, SLIM_ERRSTREAM);
			
			if (modify_child_callback->contains_self_)
				callback_symbols.InitializeConstantSymbolEntry(modify_child_callback->SelfSymbolTableEntry());		// define "self"
			
			// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
			// We can use that method because we know the lifetime of the symbol table is shorter than that of
			// the value objects, and we know that the values we are setting here will not change (the objects
			// referred to by the values may change, but the values themselves will not change).
			if (modify_child_callback->contains_child_)
				callback_symbols.InitializeConstantSymbolEntry(gID_child, p_child->CachedEidosValue());
			
			if (modify_child_callback->contains_parent1_)
				callback_symbols.InitializeConstantSymbolEntry(gID_parent1, p_parent1 ? p_parent1->CachedEidosValue() : (EidosValue_SP)gStaticEidosValueNULL);
			
			if (modify_child_callback->contains_isSelfing_)
				callback_symbols.InitializeConstantSymbolEntry(gID_isSelfing, p_is_selfing ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			
			if (modify_child_callback->contains_isCloning_)
				callback_symbols.InitializeConstantSymbolEntry(gID_isCloning, p_is_cloning ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			
			if (modify_child_callback->contains_parent2_)
				callback_symbols.InitializeConstantSymbolEntry(gID_parent2, p_parent2 ? p_parent2->CachedEidosValue() : (EidosValue_SP)gStaticEidosValueNULL);
			
			if (modify_child_callback->contains_subpop_)
				callback_symbols.InitializeConstantSymbolEntry(gID_subpop, p_target_subpop->SymbolTableEntry().second);
			
			if (modify_child_callback->contains_sourceSubpop_)
				callback_symbols.InitializeConstantSymbolEntry(gID_sourceSubpop, p_source_subpop ? p_source_subpop->SymbolTableEntry().second : (EidosValue_SP)gStaticEidosValueNULL);
			
			try
			{
				// Interpret the script; the result from the interpretation must be a singleton double used as a new fitness value
				EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(modify_child_callback->script_);
				EidosValue *result = result_SP.get();
				
				if ((result->Type() != EidosValueType::kValueLogical) || (result->Count() != 1))
					EIDOS_TERMINATION << "ERROR (Population::ApplyModifyChildCallbacks): modifyChild() callbacks must provide a logical singleton return value." << EidosTerminate(modify_child_callback->identifier_token_);
				
#if DEBUG
				// this checks the value type at runtime
				eidos_logical_t generate_child = result->LogicalData()[0];
#else
				// unsafe cast for speed
				eidos_logical_t generate_child = ((EidosValue_Logical *)result)->data()[0];
#endif
				
				// If this callback told us not to generate the child, we do not call the rest of the callback chain; we're done
				if (!generate_child)
				{
					community_.executing_block_type_ = old_executing_block_type;
					community_.focal_modification_child_ = nullptr;
					
#if (SLIMPROFILING == 1)
					// PROFILING
					SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosModifyChildCallback)]);
#endif
					
					return false;
				}
			}
			catch (...)
			{
				throw;
			}
		}
	}
	
	community_.executing_block_type_ = old_executing_block_type;
	community_.focal_modification_child_ = nullptr;
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosModifyChildCallback)]);
#endif
	
	return true;
}

// WF only:
// generate children for subpopulation p_subpop_id, drawing from all source populations, handling crossover and mutation
void Population::EvolveSubpopulation(Subpopulation &p_subpop, bool p_mate_choice_callbacks_present, bool p_modify_child_callbacks_present, bool p_recombination_callbacks_present, bool p_mutation_callbacks_present, bool p_type_s_dfe_present)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Population::EvolveSubpopulation(): usage of statics, probably many other issues");
	
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());		// for use outside of parallel blocks
	
	// determine the templated version of the Munge...() methods that we will call out to for reproduction
	// this is an optimization technique that lets us optimize away unused cruft at compile time
	// some relevant posts that were helpful in figuring out the correct syntax:
	// 	http://goodliffe.blogspot.com/2011/07/c-declaring-pointer-to-template-method.html
	// 	https://stackoverflow.com/questions/115703/storing-c-template-function-definitions-in-a-cpp-file
	// 	https://stackoverflow.com/questions/22275786/change-boolean-flags-into-template-arguments
	// and a Godbolt experiment I did to confirm that this really works: https://godbolt.org/z/Mva4Kbhrd
	bool pedigrees_enabled = species_.PedigreesEnabled();
	bool recording_tree_sequence = species_.RecordingTreeSequence();
	bool has_munge_callback = (p_modify_child_callbacks_present || p_recombination_callbacks_present || p_mutation_callbacks_present);
	bool is_spatial = (species_.SpatialDimensionality() >= 1);
	bool mutrun_exp_timing_per_individual = species_.DoingAnyMutationRunExperiments() && (species_.Chromosomes().size() > 1);
	
	bool (Subpopulation::*MungeIndividualCrossed_TEMPLATED)(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent1, Individual *p_parent2, IndividualSex p_child_sex);
	bool (Subpopulation::*MungeIndividualSelfed_TEMPLATED)(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
	bool (Subpopulation::*MungeIndividualCloned_TEMPLATED)(Individual *individual, slim_pedigreeid_t p_pedigree_id, Individual *p_parent);
	
	if (mutrun_exp_timing_per_individual)
	{
		if (pedigrees_enabled)
		{
			if (recording_tree_sequence)
			{
				if (has_munge_callback)	// has any of the callbacks that the Munge...() methods care about; this can be refined later
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, true, true, true, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, true, true, true, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, true, true, true, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, true, true, true, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, true, true, true, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, true, true, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, true, true, false, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, true, true, false, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, true, true, false, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, true, true, false, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, true, true, false, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, true, true, false, false>;
					}
				}
			}
			else
			{
				if (has_munge_callback)
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, true, false, true, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, true, false, true, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, true, false, true, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, true, false, true, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, true, false, true, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, true, false, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, true, false, false, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, true, false, false, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, true, false, false, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, true, false, false, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, true, false, false, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, true, false, false, false>;
					}
				}
			}
		}
		else
		{
			if (recording_tree_sequence)
			{
				if (has_munge_callback)
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, false, true, true, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, false, true, true, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, false, true, true, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, false, true, true, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, false, true, true, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, false, true, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, false, true, false, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, false, true, false, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, false, true, false, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, false, true, false, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, false, true, false, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, false, true, false, false>;
					}
				}
			}
			else
			{
				if (has_munge_callback)
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, false, false, true, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, false, false, true, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, false, false, true, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, false, false, true, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, false, false, true, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, false, false, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, false, false, false, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, false, false, false, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, false, false, false, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<true, false, false, false, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<true, false, false, false, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<true, false, false, false, false>;
					}
				}
			}
		}
	}
	else
	{
		if (pedigrees_enabled)
		{
			if (recording_tree_sequence)
			{
				if (has_munge_callback)
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, true, true, true, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, true, true, true, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, true, true, true, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, true, true, true, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, true, true, true, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, true, true, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, true, true, false, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, true, true, false, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, true, true, false, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, true, true, false, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, true, true, false, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, true, true, false, false>;
					}
				}
			}
			else
			{
				if (has_munge_callback)
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, true, false, true, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, true, false, true, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, true, false, true, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, true, false, true, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, true, false, true, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, true, false, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, true, false, false, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, true, false, false, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, true, false, false, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, true, false, false, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, true, false, false, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, true, false, false, false>;
					}
				}
			}
		}
		else
		{
			if (recording_tree_sequence)
			{
				if (has_munge_callback)
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, false, true, true, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, false, true, true, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, false, true, true, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, false, true, true, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, false, true, true, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, false, true, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, false, true, false, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, false, true, false, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, false, true, false, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, false, true, false, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, false, true, false, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, false, true, false, false>;
					}
				}
			}
			else
			{
				if (has_munge_callback)
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, false, false, true, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, false, false, true, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, false, false, true, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, false, false, true, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, false, false, true, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, false, false, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, false, false, false, true>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, false, false, false, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, false, false, false, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed<false, false, false, false, false>;
						MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed<false, false, false, false, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned<false, false, false, false, false>;
					}
				}
			}
		}
	}
	
	// refine the above choice with a custom version of optimizations for simple "A" and "H" cases
	if (!mutrun_exp_timing_per_individual && !has_munge_callback && (species_.Chromosomes().size() == 1))
	{
		Chromosome *chromosome = species_.Chromosomes()[0];
		ChromosomeType chromosome_type = chromosome->Type();
		
		if (chromosome_type == ChromosomeType::kA_DiploidAutosome)
		{
			if (pedigrees_enabled)
			{
				if (recording_tree_sequence)
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_A<true, true, true>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_A<true, true, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_A<true, true, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_A<true, true, false>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_A<true, true, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_A<true, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_A<true, false, true>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_A<true, false, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_A<true, false, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_A<true, false, false>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_A<true, false, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_A<true, false, false>;
					}
				}
			}
			else
			{
				if (recording_tree_sequence)
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_A<false, true, true>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_A<false, true, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_A<false, true, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_A<false, true, false>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_A<false, true, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_A<false, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_A<false, false, true>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_A<false, false, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_A<false, false, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_A<false, false, false>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_A<false, false, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_A<false, false, false>;
					}
				}
			}
		}
		else if (chromosome_type == ChromosomeType::kH_HaploidAutosome)
		{
			if (pedigrees_enabled)
			{
				if (recording_tree_sequence)
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_H<true, true, true>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_H<true, true, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_H<true, true, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_H<true, true, false>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_H<true, true, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_H<true, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_H<true, false, true>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_H<true, false, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_H<true, false, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_H<true, false, false>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_H<true, false, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_H<true, false, false>;
					}
				}
			}
			else
			{
				if (recording_tree_sequence)
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_H<false, true, true>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_H<false, true, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_H<false, true, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_H<false, true, false>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_H<false, true, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_H<false, true, false>;
					}
				}
				else
				{
					if (is_spatial)
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_H<false, false, true>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_H<false, false, true>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_H<false, false, true>;
					}
					else
					{
						MungeIndividualCrossed_TEMPLATED = &Subpopulation::MungeIndividualCrossed_1CH_H<false, false, false>;
						//MungeIndividualSelfed_TEMPLATED = &Subpopulation::MungeIndividualSelfed_1CH_H<false, false, false>;
						MungeIndividualCloned_TEMPLATED = &Subpopulation::MungeIndividualCloned_1CH_H<false, false, false>;
					}
				}
			}
		}
	}
	
	bool prevent_incidental_selfing = species_.PreventIncidentalSelfing();
	bool sex_enabled = p_subpop.sex_enabled_;
	slim_popsize_t total_children = p_subpop.child_subpop_size_;
	
	// set up to draw migrants; this works the same in the sex and asex cases, and for males / females / hermaphrodites
	// the way the code is now structured, "migrant" really includes everybody; we are a migrant source subpop for ourselves
	int migrant_source_count = static_cast<int>(p_subpop.migrant_fractions_.size());
	double migration_rates[migrant_source_count + 1];
	Subpopulation *migration_sources[migrant_source_count + 1];
	unsigned int num_migrants[migrant_source_count + 1];			// used by client code below; type constrained by gsl_ran_multinomial()
	
	if (migrant_source_count > 0)
	{
		double migration_rate_sum = 0.0;
		int pop_count = 0;
		
		for (const std::pair<const slim_objectid_t,double> &fractions_pair : p_subpop.migrant_fractions_)
		{
			slim_objectid_t migrant_source_id = fractions_pair.first;
            Subpopulation *migrant_source = species_.SubpopulationWithID(migrant_source_id);
			
			if (!migrant_source)
				EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): no migrant source subpopulation p" << migrant_source_id << "." << EidosTerminate();
			
			migration_rates[pop_count] = fractions_pair.second;
			migration_sources[pop_count] = migrant_source;
			migration_rate_sum += fractions_pair.second;
			pop_count++;
		}
		
		if (migration_rate_sum <= 1.0)
		{
			// the remaining fraction is within-subpopulation mating
			migration_rates[pop_count] = 1.0 - migration_rate_sum;
			migration_sources[pop_count] = &p_subpop;
		}
		else
			EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): too many migrants in subpopulation p" << p_subpop.subpopulation_id_ << "; migration fractions must sum to <= 1.0." << EidosTerminate();
	}
	else
	{
		migration_rates[0] = 1.0;
		migration_sources[0] = &p_subpop;
	}
	
	// SEX ONLY: the sex and asex cases share code but work a bit differently; the sex cases generates females and then males in
	// separate passes, and selfing is disabled in the sex case.  This block sets up for the sex case to diverge in these ways.
	slim_popsize_t total_female_children = 0, total_male_children = 0;
	int number_of_sexes = 1;
	
	if (sex_enabled)
	{
		double sex_ratio = p_subpop.child_sex_ratio_;
		
		total_male_children = static_cast<slim_popsize_t>(lround(total_children * sex_ratio));		// sex ratio is defined as proportion male; round in favor of males, arbitrarily
		total_female_children = total_children - total_male_children;
		number_of_sexes = 2;
		
		if (total_male_children <= 0 || total_female_children <= 0)
			EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): sex ratio " << sex_ratio << " results in a unisexual child population." << EidosTerminate();
	}
	
	// Mutrun experiment timing can be per-individual, per-chromosome, but that entails a lot of timing overhead.
	// To avoid that overhead, in single-chromosome models we just time across the whole round of reproduction
	// instead.  Note that in this case we chose a template above for the Munge...() methods that does not time.
	// FIXME 4/14/2025: It remains true that in multi-chrom models the timing overhead will be very high.  There
	// are various ways that could potentially be cut down.  (a) not measure in every tick, (b) stop measuring
	// once you've settled down into stasis, (c) measure a subset of all reproductions.  This should be done in
	// future, but we're out of time for now.
	if (species_.DoingAnyMutationRunExperiments() && (species_.Chromosomes().size() == 1))
		species_.Chromosomes()[0]->StartMutationRunExperimentClock();
	
	if (p_mate_choice_callbacks_present || p_modify_child_callbacks_present || p_recombination_callbacks_present || p_mutation_callbacks_present || p_type_s_dfe_present)
	{
		// CALLBACKS PRESENT: We need to generate offspring in a randomized order.  This way the callbacks are presented with potential offspring
		// a random order, and so it is much easier to write a callback that runs for less than the full offspring generation phase (influencing a
		// limited number of mating events, for example).  So in this code branch, we prepare an overall plan for migration and sex, and then execute
		// that plan in an order randomized with Eidos_ran_shuffle().  BCH 28 September 2016: When sex is enabled, we want to generate male and female
		// offspring in shuffled order.  However, the vector of child individuals is organized into females first, then males, so we need to fill that
		// vector in an unshuffled order or we end up trying to generate a male offspring into a female slot, or vice versa.  See the usage of
		// child_index_F, child_index_M, and child_index in the shuffle cases below.
		
		if (migrant_source_count == 0)
		{
			// CALLBACKS, NO MIGRATION: Here we are drawing all offspring from the local pool, so we can optimize a lot.  We only need to shuffle
			// the order in which males and females are generated, if we're running with sex, selfing, or cloning; if not, we actually don't need
			// to shuffle at all, because no aspect of the mating algorithm is predetermined.
			slim_popsize_t child_count = 0;	// counter over all subpop_size_ children
			Subpopulation &source_subpop = p_subpop;
			double selfing_fraction = source_subpop.selfing_fraction_;
			double cloning_fraction = source_subpop.female_clone_fraction_;
			
			// figure out our callback situation for this source subpop; callbacks come from the source, not the destination
			// we used to get other callbacks here too, but that is no longer necessary; the Munge...() methods handle it
			std::vector<SLiMEidosBlock*> *mate_choice_callbacks = nullptr;
			
			if (p_mate_choice_callbacks_present && source_subpop.registered_mate_choice_callbacks_.size())
				mate_choice_callbacks = &source_subpop.registered_mate_choice_callbacks_;
			
			if (sex_enabled || (selfing_fraction > 0.0) || (cloning_fraction > 0.0))
			{
				// We have either sex, selfing, or cloning as attributes of each individual child, so we need to pre-plan and shuffle.
				
				// BCH 13 March 2018: We were allocating a buffer for the pre-plan on the stack, and that was overflowing the stack when
				// we had a large number of children; changing to using a static allocated buffer that we reuse.  Note the buffer here is
				// separate from the one below, and uses a different struct type, despite the similarity.
				typedef struct
				{
					IndividualSex planned_sex;
					uint8_t planned_cloned;
					uint8_t planned_selfed;
				} offspring_plan_no_source;
				
				static offspring_plan_no_source *planned_offspring = nullptr;
				static int64_t planned_offspring_alloc_size = 0;
				
				if (planned_offspring_alloc_size < total_children)
				{
					planned_offspring = (offspring_plan_no_source *)realloc(planned_offspring, total_children * sizeof(offspring_plan_no_source));		// NOLINT(*-realloc-usage) : realloc failure is a fatal error anyway
					if (!planned_offspring)
						EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
					planned_offspring_alloc_size = total_children;
				}
				
				for (int sex_index = 0; sex_index < number_of_sexes; ++sex_index)
				{
					slim_popsize_t total_children_of_sex;
					IndividualSex child_sex;
					
					if (sex_enabled)
					{
						total_children_of_sex = ((sex_index == 0) ? total_female_children : total_male_children);
						child_sex = ((sex_index == 0) ? IndividualSex::kFemale : IndividualSex::kMale);
					}
					else
					{
						total_children_of_sex = total_children;
						child_sex = IndividualSex::kHermaphrodite;
					}
					
					slim_popsize_t migrants_to_generate = total_children_of_sex;
					
					if (migrants_to_generate > 0)
					{
						// figure out how many from this source subpop are the result of selfing and/or cloning
						slim_popsize_t number_to_self = 0, number_to_clone = 0;
						
						if (selfing_fraction > 0)
						{
							if (cloning_fraction > 0)
							{
								double fractions[3] = {selfing_fraction, cloning_fraction, 1.0 - (selfing_fraction + cloning_fraction)};
								unsigned int counts[3] = {0, 0, 0};
								
								if (fractions[2] < 0.0)
									EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): selfingRate + cloningRate > 1.0; cannot generate offspring satisfying constraints." << EidosTerminate(nullptr);
								
								gsl_ran_multinomial(rng, 3, (unsigned int)migrants_to_generate, fractions, counts);
								
								number_to_self = static_cast<slim_popsize_t>(counts[0]);
								number_to_clone = static_cast<slim_popsize_t>(counts[1]);
							}
							else
								number_to_self = static_cast<slim_popsize_t>(gsl_ran_binomial(rng, selfing_fraction, (unsigned int)migrants_to_generate));
						}
						else if (cloning_fraction > 0)
							number_to_clone = static_cast<slim_popsize_t>(gsl_ran_binomial(rng, cloning_fraction, (unsigned int)migrants_to_generate));
						
						// generate all selfed, cloned, and autogamous offspring in one shared loop
						slim_popsize_t migrant_count = 0;
						
						while (migrant_count < migrants_to_generate)
						{
							offspring_plan_no_source *offspring_plan_ptr = planned_offspring + child_count;
							
							offspring_plan_ptr->planned_sex = child_sex;
							
							if (number_to_clone > 0)
							{
								offspring_plan_ptr->planned_cloned = true;
								offspring_plan_ptr->planned_selfed = false;
								--number_to_clone;
							}
							else if (number_to_self > 0)
							{
								offspring_plan_ptr->planned_cloned = false;
								offspring_plan_ptr->planned_selfed = true;
								--number_to_self;
							}
							else
							{
								offspring_plan_ptr->planned_cloned = false;
								offspring_plan_ptr->planned_selfed = false;
							}
							
							// change all our counters
							migrant_count++;
							child_count++;
						}
					}
				}
				
				Eidos_ran_shuffle(rng, planned_offspring, total_children);
				
				// Now we can run through our plan vector and generate each planned child in order.
				slim_popsize_t child_index_F = 0, child_index_M = total_female_children, child_index;
				
				for (child_count = 0; child_count < total_children; ++child_count)
				{
					// Get the plan for this offspring from our shuffled plan vector
					offspring_plan_no_source *offspring_plan_ptr = planned_offspring + child_count;
					
					IndividualSex child_sex = offspring_plan_ptr->planned_sex;
					bool selfed, cloned;
					int num_tries = 0;
					
					// Find the appropriate index for the child we are generating; we need to put males and females in the right spots
					if (sex_enabled)
					{
						if (child_sex == IndividualSex::kFemale)
							child_index = child_index_F++;
						else
							child_index = child_index_M++;
					}
					else
					{
						child_index = child_count;
					}
					
					// We loop back to here to retry child generation if a mateChoice() or modifyChild() callback causes our first attempt at
					// child generation to fail.  The first time we generate a given child index, we follow our plan; subsequent times, we
					// draw selfed and cloned randomly based on the probabilities set for the source subpop.  This allows the callbacks to
					// actually influence the proportion selfed/cloned, through e.g. lethal epistatic interactions or failed mate search.
				retryChild:
					
					if (num_tries > 1000000)
						EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): failed to generate child after 1 million attempts; terminating to avoid infinite loop." << EidosTerminate();
					
					if (num_tries == 0)
					{
						// first mating event, so follow our original plan for this offspring
						// note we could draw self/cloned as below even for the first try; this code path is just more efficient,
						// since it avoids a gsl_ran_uniform() for each child, in favor of one gsl_ran_multinomial() above
						selfed = offspring_plan_ptr->planned_selfed;
						cloned = offspring_plan_ptr->planned_cloned;
					}
					else
					{
						// a whole new mating event, so we draw selfed/cloned based on the source subpop's probabilities
						selfed = false;
						cloned = false;
						
						if (selfing_fraction > 0)
						{
							if (cloning_fraction > 0)
							{
								double draw = Eidos_rng_uniform(rng);
								
								if (draw < selfing_fraction)							selfed = true;
								else if (draw < selfing_fraction + cloning_fraction)	cloned = true;
							}
							else
							{
								double draw = Eidos_rng_uniform(rng);
								
								if (draw < selfing_fraction)							selfed = true;
							}
						}
						else if (cloning_fraction > 0)
						{
							double draw = Eidos_rng_uniform(rng);
							
							if (draw < cloning_fraction)								cloned = true;
						}
						
						// we do not redraw the sex of the child, however, because that is predetermined; we need to hit our target ratio
						// we could trade our planned sex for a randomly chosen planned sex from the remaining children to generate, but
						// that gets a little complicated because of selfing, and I'm having trouble imagining a scenario where it matters
					}
					
					bool child_accepted;
					
					if (cloned)
					{
						slim_popsize_t parent1;
						
						if (sex_enabled)
							parent1 = (child_sex == IndividualSex::kFemale) ? source_subpop.DrawFemaleParentUsingFitness(rng) : source_subpop.DrawMaleParentUsingFitness(rng);
						else
							parent1 = source_subpop.DrawParentUsingFitness(rng);
						
						slim_pedigreeid_t individual_pid = pedigrees_enabled ? SLiM_GetNextPedigreeID() : 0;
						Individual *new_child = p_subpop.child_individuals_[child_index];
						new_child->migrant_ = false;
						
						child_accepted = (p_subpop.*MungeIndividualCloned_TEMPLATED)(new_child, individual_pid, source_subpop.parent_individuals_[parent1]);
					}
					else
					{
						slim_popsize_t parent1;

						if (sex_enabled)
							parent1 = source_subpop.DrawFemaleParentUsingFitness(rng);
						else
							parent1 = source_subpop.DrawParentUsingFitness(rng);
						
						if (selfed)
						{
							slim_pedigreeid_t individual_pid = pedigrees_enabled ? SLiM_GetNextPedigreeID() : 0;
							
							Individual *new_child = p_subpop.child_individuals_[child_index];
							new_child->migrant_ = false;
							
							child_accepted = (p_subpop.*MungeIndividualSelfed_TEMPLATED)(new_child, individual_pid, source_subpop.parent_individuals_[parent1]);
						}
						else
						{
							slim_popsize_t parent2;
							
							if (!mate_choice_callbacks)
							{
								if (sex_enabled)
								{
									parent2 = source_subpop.DrawMaleParentUsingFitness(rng);
								}
								else
								{
									do
										parent2 = source_subpop.DrawParentUsingFitness(rng);	// selfing possible!
									while (prevent_incidental_selfing && (parent2 == parent1));
								}
							}
							else
							{
								do
									parent2 = ApplyMateChoiceCallbacks(parent1, &p_subpop, &source_subpop, *mate_choice_callbacks);
								while (prevent_incidental_selfing && (parent2 == parent1));
								
								if (parent2 == -1)
								{
									// The mateChoice() callbacks rejected parent1 altogether, so we need to choose a new parent1 and start over
									num_tries++;
									goto retryChild;
								}
							}
							
							slim_pedigreeid_t individual_pid = pedigrees_enabled ? SLiM_GetNextPedigreeID() : 0;
							
							Individual *new_child = p_subpop.child_individuals_[child_index];
							new_child->migrant_ = false;
							
							child_accepted = (p_subpop.*MungeIndividualCrossed_TEMPLATED)(new_child, individual_pid, source_subpop.parent_individuals_[parent1], source_subpop.parent_individuals_[parent2], child_sex);
						}
					}
					
					if (!child_accepted)
					{
						// The modifyChild() callbacks suppressed the child altogether; this is juvenile migrant mortality, basically, so
						// we need to even change the source subpop for our next attempt.  In this case, however, we have no migration.
						num_tries++;
						goto retryChild;
					}
				}
			}
			else
			{
				// CALLBACKS, NO MIGRATION, NO SEX, NO SELFING, NO CLONING: so we don't need to preplan or shuffle, each child is generated in the same exact way.
				int num_tries = 0;
				
				while (child_count < total_children)
				{
					slim_popsize_t parent1, parent2;
					
					parent1 = source_subpop.DrawParentUsingFitness(rng);
					
					if (!mate_choice_callbacks)
					{
						do
							parent2 = source_subpop.DrawParentUsingFitness(rng);	// selfing possible!
						while (prevent_incidental_selfing && (parent2 == parent1));
					}
					else
					{
						while (true)	// loop while parent2 == -1, indicating a request for a new first parent
						{
							do
								parent2 = ApplyMateChoiceCallbacks(parent1, &p_subpop, &source_subpop, *mate_choice_callbacks);
							while (prevent_incidental_selfing && (parent2 == parent1));
							
							if (parent2 != -1)
								break;
							
							// parent1 was rejected by the callbacks, so we need to redraw a new parent1
							num_tries++;
							parent1 = source_subpop.DrawParentUsingFitness(rng);
							
							if (num_tries > 1000000)
								EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): failed to generate child after 1 million attempts; terminating to avoid infinite loop." << EidosTerminate();
						}
					}
					
					slim_pedigreeid_t individual_pid = pedigrees_enabled ? SLiM_GetNextPedigreeID() : 0;
					
					Individual *new_child = p_subpop.child_individuals_[child_count];
					new_child->migrant_ = false;
					
					bool child_accepted = (p_subpop.*MungeIndividualCrossed_TEMPLATED)(new_child, individual_pid, source_subpop.parent_individuals_[parent1], source_subpop.parent_individuals_[parent2], IndividualSex::kHermaphrodite);
					
					if (!child_accepted)
					{
						num_tries++;
						
						if (num_tries > 1000000)
							EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): failed to generate child after 1 million attempts; terminating to avoid infinite loop." << EidosTerminate();
						
						continue;
					}
					
					// if the child was accepted, change all our counters to start afresh
					child_count++;
					num_tries = 0;
				}
			}
		}
		else
		{
			// CALLBACKS WITH MIGRATION: Here we need to shuffle the migration source subpops, as well as the offspring sex.  This makes things so
			// different that it is worth treating it as an entirely separate case; it is far less optimizable than the case without migration.
			// Note that this is pretty much the general case of this whole method; all the other cases are optimized sub-cases of this code!
			slim_popsize_t child_count = 0;	// counter over all subpop_size_ children
			
			// Pre-plan and shuffle.
			
			// BCH 13 March 2018: We were allocating a buffer for the pre-plan on the stack, and that was overflowing the stack when
			// we had a large number of children; changing to using a static allocated buffer that we reuse.  Note the buffer here is
			// separate from the one below, and uses a different struct type, despite the similarity.
			typedef struct
			{
				Subpopulation *planned_source;
				IndividualSex planned_sex;
				uint8_t planned_cloned;
				uint8_t planned_selfed;
			} offspring_plan_with_source;
			
			static offspring_plan_with_source *planned_offspring = nullptr;
			static int64_t planned_offspring_alloc_size = 0;
			
			if (planned_offspring_alloc_size < total_children)
			{
				planned_offspring = (offspring_plan_with_source *)realloc(planned_offspring, total_children * sizeof(offspring_plan_with_source));		// NOLINT(*-realloc-usage) : realloc failure is a fatal error anyway
				if (!planned_offspring)
					EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
				planned_offspring_alloc_size = total_children;
			}
			
			for (int sex_index = 0; sex_index < number_of_sexes; ++sex_index)
			{
				slim_popsize_t total_children_of_sex;
				IndividualSex child_sex;
				
				if (sex_enabled)
				{
					total_children_of_sex = ((sex_index == 0) ? total_female_children : total_male_children);
					child_sex = ((sex_index == 0) ? IndividualSex::kFemale : IndividualSex::kMale);
				}
				else
				{
					total_children_of_sex = total_children;
					child_sex = IndividualSex::kHermaphrodite;
				}
				
				// draw the number of individuals from the migrant source subpops, and from ourselves, for the current sex
				if (migrant_source_count == 0)
					num_migrants[0] = (unsigned int)total_children_of_sex;
				else
					gsl_ran_multinomial(rng, migrant_source_count + 1, (unsigned int)total_children_of_sex, migration_rates, num_migrants);
				
				// loop over all source subpops, including ourselves
				for (int pop_count = 0; pop_count < migrant_source_count + 1; ++pop_count)
				{
					slim_popsize_t migrants_to_generate = static_cast<slim_popsize_t>(num_migrants[pop_count]);
					
					if (migrants_to_generate > 0)
					{
						Subpopulation &source_subpop = *(migration_sources[pop_count]);
						double selfing_fraction = sex_enabled ? 0.0 : source_subpop.selfing_fraction_;
						double cloning_fraction = (sex_index == 0) ? source_subpop.female_clone_fraction_ : source_subpop.male_clone_fraction_;
						
						// figure out how many from this source subpop are the result of selfing and/or cloning
						slim_popsize_t number_to_self = 0, number_to_clone = 0;
						
						if (selfing_fraction > 0)
						{
							if (cloning_fraction > 0)
							{
								double fractions[3] = {selfing_fraction, cloning_fraction, 1.0 - (selfing_fraction + cloning_fraction)};
								unsigned int counts[3] = {0, 0, 0};
								
								if (fractions[2] < 0.0)
									EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): selfingRate + cloningRate > 1.0; cannot generate offspring satisfying constraints." << EidosTerminate(nullptr);
								
								gsl_ran_multinomial(rng, 3, (unsigned int)migrants_to_generate, fractions, counts);
								
								number_to_self = static_cast<slim_popsize_t>(counts[0]);
								number_to_clone = static_cast<slim_popsize_t>(counts[1]);
							}
							else
								number_to_self = static_cast<slim_popsize_t>(gsl_ran_binomial(rng, selfing_fraction, (unsigned int)migrants_to_generate));
						}
						else if (cloning_fraction > 0)
							number_to_clone = static_cast<slim_popsize_t>(gsl_ran_binomial(rng, cloning_fraction, (unsigned int)migrants_to_generate));
						
						// generate all selfed, cloned, and autogamous offspring in one shared loop
						slim_popsize_t migrant_count = 0;
						
						while (migrant_count < migrants_to_generate)
						{
							offspring_plan_with_source *offspring_plan_ptr = planned_offspring + child_count;
							
							offspring_plan_ptr->planned_source = &source_subpop;
							offspring_plan_ptr->planned_sex = child_sex;
							
							if (number_to_clone > 0)
							{
								offspring_plan_ptr->planned_cloned = true;
								offspring_plan_ptr->planned_selfed = false;
								--number_to_clone;
							}
							else if (number_to_self > 0)
							{
								offspring_plan_ptr->planned_cloned = false;
								offspring_plan_ptr->planned_selfed = true;
								--number_to_self;
							}
							else
							{
								offspring_plan_ptr->planned_cloned = false;
								offspring_plan_ptr->planned_selfed = false;
							}
							
							// change all our counters
							migrant_count++;
							child_count++;
						}
					}
				}
			}
			
			Eidos_ran_shuffle(rng, planned_offspring, total_children);
			
			// Now we can run through our plan vector and generate each planned child in order.
			slim_popsize_t child_index_F = 0, child_index_M = total_female_children, child_index;
			
			for (child_count = 0; child_count < total_children; ++child_count)
			{
				// Get the plan for this offspring from our shuffled plan vector
				offspring_plan_with_source *offspring_plan_ptr = planned_offspring + child_count;
				
				Subpopulation *source_subpop = offspring_plan_ptr->planned_source;
				IndividualSex child_sex = offspring_plan_ptr->planned_sex;
				bool selfed, cloned;
				int num_tries = 0;
				
				// Find the appropriate index for the child we are generating; we need to put males and females in the right spots
				if (sex_enabled)
				{
					if (child_sex == IndividualSex::kFemale)
						child_index = child_index_F++;
					else
						child_index = child_index_M++;
				}
				else
				{
					child_index = child_count;
				}
				
				// We loop back to here to retry child generation if a modifyChild() callback causes our first attempt at
				// child generation to fail.  The first time we generate a given child index, we follow our plan; subsequent times, we
				// draw selfed and cloned randomly based on the probabilities set for the source subpop.  This allows the callbacks to
				// actually influence the proportion selfed/cloned, through e.g. lethal epistatic interactions or failed mate search.
			retryWithNewSourceSubpop:
				
				// figure out our callback situation for this source subpop; callbacks come from the source, not the destination
				// we used to get other callbacks here too, but that is no longer necessary; the Munge...() methods handle it
				std::vector<SLiMEidosBlock*> *mate_choice_callbacks = nullptr;
				
				if (source_subpop->registered_mate_choice_callbacks_.size())
					mate_choice_callbacks = &source_subpop->registered_mate_choice_callbacks_;
				
				// Similar to retryWithNewSourceSubpop: but assumes that the subpop remains unchanged; used after a failed mateChoice()
				// callback, which rejects parent1 but does not cause a redraw of the source subpop.
			retryWithSameSourceSubpop:
				
				if (num_tries > 1000000)
					EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): failed to generate child after 1 million attempts; terminating to avoid infinite loop." << EidosTerminate();
				
				if (num_tries == 0)
				{
					// first mating event, so follow our original plan for this offspring
					// note we could draw self/cloned as below even for the first try; this code path is just more efficient,
					// since it avoids a gsl_ran_uniform() for each child, in favor of one gsl_ran_multinomial() above
					selfed = offspring_plan_ptr->planned_selfed;
					cloned = offspring_plan_ptr->planned_cloned;
				}
				else
				{
					// a whole new mating event, so we draw selfed/cloned based on the source subpop's probabilities
					double selfing_fraction = sex_enabled ? 0.0 : source_subpop->selfing_fraction_;
					double cloning_fraction = (child_sex != IndividualSex::kMale) ? source_subpop->female_clone_fraction_ : source_subpop->male_clone_fraction_;
					
					selfed = false;
					cloned = false;
					
					if (selfing_fraction > 0)
					{
						if (cloning_fraction > 0)
						{
							double draw = Eidos_rng_uniform(rng);
							
							if (draw < selfing_fraction)							selfed = true;
							else if (draw < selfing_fraction + cloning_fraction)	cloned = true;
						}
						else
						{
							double draw = Eidos_rng_uniform(rng);
							
							if (draw < selfing_fraction)							selfed = true;
						}
					}
					else if (cloning_fraction > 0)
					{
						double draw = Eidos_rng_uniform(rng);
						
						if (draw < cloning_fraction)								cloned = true;
					}
					
					// we do not redraw the sex of the child, however, because that is predetermined; we need to hit our target ratio
					// we could trade our planned sex for a randomly chosen planned sex from the remaining children to generate, but
					// that gets a little complicated because of selfing, and I'm having trouble imagining a scenario where it matters
				}
				
				bool child_accepted;
				
				if (cloned)
				{
					slim_popsize_t parent1;
					
					if (sex_enabled)
						parent1 = (child_sex == IndividualSex::kFemale) ? source_subpop->DrawFemaleParentUsingFitness(rng) : source_subpop->DrawMaleParentUsingFitness(rng);
					else
						parent1 = source_subpop->DrawParentUsingFitness(rng);
					
					slim_pedigreeid_t individual_pid = pedigrees_enabled ? SLiM_GetNextPedigreeID() : 0;
					
					Individual *new_child = p_subpop.child_individuals_[child_index];
					new_child->migrant_ = (source_subpop != &p_subpop);
					
					child_accepted = (p_subpop.*MungeIndividualCloned_TEMPLATED)(new_child, individual_pid, source_subpop->parent_individuals_[parent1]);
				}
				else
				{
					slim_popsize_t parent1;
					
					if (sex_enabled)
						parent1 = source_subpop->DrawFemaleParentUsingFitness(rng);
					else
						parent1 = source_subpop->DrawParentUsingFitness(rng);
					
					if (selfed)
					{
						slim_pedigreeid_t individual_pid = pedigrees_enabled ? SLiM_GetNextPedigreeID() : 0;
						
						Individual *new_child = p_subpop.child_individuals_[child_index];
						new_child->migrant_ = (source_subpop != &p_subpop);
						
						child_accepted = (p_subpop.*MungeIndividualSelfed_TEMPLATED)(new_child, individual_pid, source_subpop->parent_individuals_[parent1]);
					}
					else
					{
						slim_popsize_t parent2;
						
						if (!mate_choice_callbacks)
						{
							if (sex_enabled)
							{
								parent2 = source_subpop->DrawMaleParentUsingFitness(rng);
							}
							else
							{
								do
									parent2 = source_subpop->DrawParentUsingFitness(rng);	// selfing possible!
								while (prevent_incidental_selfing && (parent2 == parent1));
							}
						}
						else
						{
							do
								parent2 = ApplyMateChoiceCallbacks(parent1, &p_subpop, source_subpop, *mate_choice_callbacks);
							while (prevent_incidental_selfing && (parent2 == parent1));
							
							if (parent2 == -1)
							{
								// The mateChoice() callbacks rejected parent1 altogether, so we need to choose a new parent1 and start over
								num_tries++;
								goto retryWithSameSourceSubpop;
							}
						}
						
						slim_pedigreeid_t individual_pid = pedigrees_enabled ? SLiM_GetNextPedigreeID() : 0;
						
						Individual *new_child = p_subpop.child_individuals_[child_index];
						new_child->migrant_ = (source_subpop != &p_subpop);
						
						child_accepted = (p_subpop.*MungeIndividualCrossed_TEMPLATED)(new_child, individual_pid, source_subpop->parent_individuals_[parent1], source_subpop->parent_individuals_[parent2], child_sex);
					}
				}
				
				if (!child_accepted)
				{
					// The modifyChild() callbacks suppressed the child altogether; this is juvenile migrant mortality, basically, so
					// we need to even change the source subpop for our next attempt, so that differential mortality between different
					// migration sources leads to differential representation in the offspring generation – more offspring from the
					// subpop that is more successful at contributing migrants.
					gsl_ran_multinomial(rng, migrant_source_count + 1, 1, migration_rates, num_migrants);
					
					for (int pop_count = 0; pop_count < migrant_source_count + 1; ++pop_count)
						if (num_migrants[pop_count] > 0)
						{
							source_subpop = migration_sources[pop_count];
							break;
						}
					
					num_tries++;
					goto retryWithNewSourceSubpop;
				}
			}
		}
	}
	else
	{
		// NO CALLBACKS PRESENT: offspring can be generated in a fixed (i.e. predetermined) order.  This is substantially faster, since it avoids
		// some setup overhead, including the Eidos_ran_shuffle() call.  All code that accesses individuals within a subpopulation needs to be aware of
		// the fact that the individuals might be in a non-random order, because of this code path.  BEWARE!
		
		// In some cases the code below parallelizes, when we're running multithreaded.  The main condition, already satisfied simply by virtue of
		// being in this code path, is that there are no callbacks enabled, of any type, that influence the process of reproduction.  This is because
		// we can't run Eidos code in parallel, at least for now.  At the moment, the DSB recombination model is also not allowed; it hasn't been tested.
#ifdef _OPENMP
		bool can_parallelize = true;
		
		for (Chromosome *chromosome : species_.Chromosomes())
			if (chromosome.using_DSB_model_)
			{
				can_parallelize = false;
				break;
			}
#endif
		
		// We loop to generate females first (sex_index == 0) and males second (sex_index == 1).
		// In nonsexual simulations number_of_sexes == 1 and this loops just once.
		slim_popsize_t child_count = 0;	// counter over all subpop_size_ children
		
		for (int sex_index = 0; sex_index < number_of_sexes; ++sex_index)
		{
			slim_popsize_t total_children_of_sex;
			IndividualSex child_sex;
			
			if (sex_enabled)
			{
				total_children_of_sex = ((sex_index == 0) ? total_female_children : total_male_children);
				child_sex = ((sex_index == 0) ? IndividualSex::kFemale : IndividualSex::kMale);
			}
			else
			{
				total_children_of_sex = total_children;
				child_sex = IndividualSex::kHermaphrodite;
			}
			
			// draw the number of individuals from the migrant source subpops, and from ourselves, for the current sex
			if (migrant_source_count == 0)
				num_migrants[0] = (unsigned int)total_children_of_sex;
			else
				gsl_ran_multinomial(rng, migrant_source_count + 1, (unsigned int)total_children_of_sex, migration_rates, num_migrants);
			
			// loop over all source subpops, including ourselves
			for (int pop_count = 0; pop_count < migrant_source_count + 1; ++pop_count)
			{
				slim_popsize_t migrants_to_generate = static_cast<slim_popsize_t>(num_migrants[pop_count]);
				
				if (migrants_to_generate > 0)
				{
					Subpopulation &source_subpop = *(migration_sources[pop_count]);
					double selfing_fraction = sex_enabled ? 0.0 : source_subpop.selfing_fraction_;
					double cloning_fraction = (sex_index == 0) ? source_subpop.female_clone_fraction_ : source_subpop.male_clone_fraction_;
					
					// figure out how many from this source subpop are the result of selfing and/or cloning
					slim_popsize_t number_to_self = 0, number_to_clone = 0;
					
					if (selfing_fraction > 0)
					{
						if (cloning_fraction > 0)
						{
							double fractions[3] = {selfing_fraction, cloning_fraction, 1.0 - (selfing_fraction + cloning_fraction)};
							unsigned int counts[3] = {0, 0, 0};
							
							if (fractions[2] < 0.0)
								EIDOS_TERMINATION << "ERROR (Population::EvolveSubpopulation): selfingRate + cloningRate > 1.0; cannot generate offspring satisfying constraints." << EidosTerminate(nullptr);
							
							gsl_ran_multinomial(rng, 3, (unsigned int)migrants_to_generate, fractions, counts);
							
							number_to_self = static_cast<slim_popsize_t>(counts[0]);
							number_to_clone = static_cast<slim_popsize_t>(counts[1]);
						}
						else
							number_to_self = static_cast<slim_popsize_t>(gsl_ran_binomial(rng, selfing_fraction, (unsigned int)migrants_to_generate));
					}
					else if (cloning_fraction > 0)
						number_to_clone = static_cast<slim_popsize_t>(gsl_ran_binomial(rng, cloning_fraction, (unsigned int)migrants_to_generate));
					
					// We get a whole block of pedigree IDs to use in the loop below, avoiding race conditions / locking
					// We are also going to use Individual objects from a block starting at base_child_count
					slim_pedigreeid_t base_pedigree_id = SLiM_GetNextPedigreeID_Block(migrants_to_generate);
					slim_popsize_t base_child_count = child_count;
					
					// We need to make sure we have adequate capacity in the global mutation block for new mutations before we go parallel;
					// if SLiM_IncreaseMutationBlockCapacity() is called while parallel, it is a fatal error.  So we make a guess at how
					// much free space we will need, and preallocate here as needed, regardless of will_parallelize; no reason not to.
#ifdef _OPENMP
					bool will_parallelize = can_parallelize && (migrants_to_generate >= EIDOS_OMPMIN_WF_REPRO);
					size_t est_mutation_block_slots_remaining_PRE = 0;
					//size_t actual_mutation_block_slots_remaining_PRE = 0;
					double overall_mutation_rate = 0;
					size_t est_slots_needed = 0;
					
					{
						do {
							int registry_size;
							MutationRegistry(&registry_size);
							est_mutation_block_slots_remaining_PRE = gSLiM_Mutation_Block_Capacity - registry_size;
							//actual_mutation_block_slots_remaining_PRE = SLiMMemoryUsageForFreeMutations() / sizeof(Mutation);
							overall_mutation_rate = std::max(species_.chromosome_->overall_mutation_rate_F_, species_.chromosome_->overall_mutation_rate_M_);	// already multiplied by L
							est_slots_needed = (size_t)ceil(2 * migrants_to_generate * overall_mutation_rate);	// 2 because diploid, in the worst case
							
							size_t ten_times_demand = 10 * est_slots_needed;
							
							if (est_mutation_block_slots_remaining_PRE <= ten_times_demand)
							{
								SLiM_IncreaseMutationBlockCapacity();
								est_mutation_block_slots_remaining_PRE = gSLiM_Mutation_Block_Capacity - registry_size;
								
								//std::cerr << "Tick " << community_.Tick() << ": DOUBLED CAPACITY ***********************************" << std::endl;
							}
							else
								break;
						} while (true);
						
						//std::cerr << "Tick " << community_.Tick() << ":" << std::endl;
						//std::cerr << "   before reproduction, " << actual_mutation_block_slots_remaining_PRE << " actual slots remaining (" << est_mutation_block_slots_remaining_PRE << " estimated)" << std::endl;
						//std::cerr << "   demand for new mutations estimated at " << est_slots_needed << " (" << migrants_to_generate << " offspring, E(muts) == " << overall_mutation_rate << ")" << std::endl;
					}
#endif
					
					// generate all selfed, cloned, and autogamous offspring in one shared loop
					if ((number_to_self == 0) && (number_to_clone == 0))
					{
						// a simple loop for the base case with no selfing, no cloning, and no callbacks; we split into two cases by sex_enabled for maximal speed
						if (sex_enabled)
						{
							EIDOS_BENCHMARK_START(EidosBenchmarkType::k_WF_REPRO);
							EIDOS_THREAD_COUNT(gEidos_OMP_threads_WF_REPRO);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, migrants_to_generate, base_child_count, base_pedigree_id, pedigrees_enabled, p_subpop, source_subpop, child_sex, prevent_incidental_selfing) if(will_parallelize) num_threads(thread_count)
							{
								gsl_rng *parallel_rng = EIDOS_GSL_RNG(omp_get_thread_num());
								
#pragma omp for schedule(dynamic, 1)
								for (slim_popsize_t migrant_count = 0; migrant_count < migrants_to_generate; migrant_count++)
								{
									slim_popsize_t parent1 = source_subpop.DrawFemaleParentUsingFitness(parallel_rng);
									slim_popsize_t parent2 = source_subpop.DrawMaleParentUsingFitness(parallel_rng);
									
									slim_popsize_t this_child_index = base_child_count + migrant_count;
									Individual *new_child = p_subpop.child_individuals_[this_child_index];
									new_child->migrant_ = (&source_subpop != &p_subpop);
									
									(p_subpop.*MungeIndividualCrossed_TEMPLATED)(new_child, base_pedigree_id + migrant_count, source_subpop.parent_individuals_[parent1], source_subpop.parent_individuals_[parent2], child_sex);
									new_child->migrant_ = (&source_subpop != &p_subpop);
								}
							}
							EIDOS_BENCHMARK_END(EidosBenchmarkType::k_WF_REPRO);
							
							child_count += migrants_to_generate;
						}
						else
						{
							EIDOS_BENCHMARK_START(EidosBenchmarkType::k_WF_REPRO);
							EIDOS_THREAD_COUNT(gEidos_OMP_threads_WF_REPRO);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, migrants_to_generate, base_child_count, base_pedigree_id, pedigrees_enabled, p_subpop, source_subpop, child_sex, prevent_incidental_selfing) if(will_parallelize) num_threads(thread_count)
							{
								gsl_rng *parallel_rng = EIDOS_GSL_RNG(omp_get_thread_num());
								
#pragma omp for schedule(dynamic, 1)
								for (slim_popsize_t migrant_count = 0; migrant_count < migrants_to_generate; migrant_count++)
								{
									slim_popsize_t parent1 = source_subpop.DrawParentUsingFitness(parallel_rng);
									slim_popsize_t parent2;
									
									do
										parent2 = source_subpop.DrawParentUsingFitness(parallel_rng);	// note this does not prohibit selfing!
									while (prevent_incidental_selfing && (parent2 == parent1));
									
									slim_popsize_t this_child_index = base_child_count + migrant_count;
									Individual *new_child = p_subpop.child_individuals_[this_child_index];
									new_child->migrant_ = (&source_subpop != &p_subpop);
									
									(p_subpop.*MungeIndividualCrossed_TEMPLATED)(new_child, base_pedigree_id + migrant_count, source_subpop.parent_individuals_[parent1], source_subpop.parent_individuals_[parent2], child_sex);
								}
							}
							EIDOS_BENCHMARK_END(EidosBenchmarkType::k_WF_REPRO);
							
							child_count += migrants_to_generate;
						}
					}
					else
					{
						// the full loop with support for selfing/cloning (but no callbacks, since we're in that overall branch)
						EIDOS_BENCHMARK_START(EidosBenchmarkType::k_WF_REPRO);
						EIDOS_THREAD_COUNT(gEidos_OMP_threads_WF_REPRO);
#pragma omp parallel default(none) shared(gEidos_RNG_PERTHREAD, migrants_to_generate, number_to_clone, number_to_self, base_child_count, base_pedigree_id, pedigrees_enabled, p_subpop, source_subpop, sex_enabled, child_sex, recording_tree_sequence, prevent_incidental_selfing) if(will_parallelize) num_threads(thread_count)
						{
							gsl_rng *parallel_rng = EIDOS_GSL_RNG(omp_get_thread_num());
							
#pragma omp for schedule(dynamic, 1)
							for (slim_popsize_t migrant_count = 0; migrant_count < migrants_to_generate; migrant_count++)
							{
								if (migrant_count < number_to_clone)
								{
									slim_popsize_t parent1;
									
									if (sex_enabled)
										parent1 = (child_sex == IndividualSex::kFemale) ? source_subpop.DrawFemaleParentUsingFitness(parallel_rng) : source_subpop.DrawMaleParentUsingFitness(parallel_rng);
									else
										parent1 = source_subpop.DrawParentUsingFitness(parallel_rng);
									
									slim_popsize_t this_child_index = base_child_count + migrant_count;
									Individual *new_child = p_subpop.child_individuals_[this_child_index];
									new_child->migrant_ = (&source_subpop != &p_subpop);
									
									(p_subpop.*MungeIndividualCloned_TEMPLATED)(new_child, base_pedigree_id + migrant_count, source_subpop.parent_individuals_[parent1]);
								}
								else
								{
									slim_popsize_t parent1;
									
									if (sex_enabled)
										parent1 = source_subpop.DrawFemaleParentUsingFitness(parallel_rng);
									else
										parent1 = source_subpop.DrawParentUsingFitness(parallel_rng);
									
									slim_popsize_t this_child_index = base_child_count + migrant_count;
									Individual *new_child = p_subpop.child_individuals_[this_child_index];
									new_child->migrant_ = (&source_subpop != &p_subpop);
									
									if (migrant_count < number_to_clone + number_to_self)
									{
										(p_subpop.*MungeIndividualSelfed_TEMPLATED)(new_child, base_pedigree_id + migrant_count, source_subpop.parent_individuals_[parent1]);
									}
									else
									{
										slim_popsize_t parent2;
										
										if (sex_enabled)
										{
											parent2 = source_subpop.DrawMaleParentUsingFitness(parallel_rng);
										}
										else
										{
											do
												parent2 = source_subpop.DrawParentUsingFitness(parallel_rng);	// selfing possible!
											while (prevent_incidental_selfing && (parent2 == parent1));
										}
										
										(p_subpop.*MungeIndividualCrossed_TEMPLATED)(new_child, base_pedigree_id + migrant_count, source_subpop.parent_individuals_[parent1], source_subpop.parent_individuals_[parent2], child_sex);
									}
								}
							}
						}
						EIDOS_BENCHMARK_END(EidosBenchmarkType::k_WF_REPRO);
						
						child_count += migrants_to_generate;
					}
					
#ifdef _OPENMP
					//if (will_parallelize)
					//{
					//	size_t actual_mutation_block_slots_remaining_POST = SLiMMemoryUsageForFreeMutations() / sizeof(Mutation);
					//	
					//	std::cerr << "   after reproduction, " << actual_mutation_block_slots_remaining_POST << " actual slots remaining" << std::endl;
					//	std::cerr << "   actual demand for new mutations was " << (actual_mutation_block_slots_remaining_PRE - actual_mutation_block_slots_remaining_POST) << " (" << est_slots_needed << " was estimated)" << std::endl;
					//}
#endif
				}
			}
		}
	}
	
	// Mutrun experiment timing can be per-individual, per-chromosome, but that entails a lot of timing overhead.
	// To avoid that overhead, in single-chromosome models we just time across the whole round of reproduction
	// instead.  Note that in this case we chose a template above for the Munge...() methods that does not time.
	if (species_.DoingAnyMutationRunExperiments() && (species_.Chromosomes().size() == 1))
		species_.Chromosomes()[0]->StopMutationRunExperimentClock("EvolveSubpopulation()");
}

// apply recombination() callbacks to a generated child; a return of true means breakpoints were changed
bool Population::ApplyRecombinationCallbacks(Individual *p_parent, Haplosome *p_haplosome1, Haplosome *p_haplosome2, std::vector<slim_position_t> &p_crossovers, std::vector<SLiMEidosBlock*> &p_recombination_callbacks)
{
	THREAD_SAFETY_IN_ANY_PARALLEL("Population::ApplyRecombinationCallbacks(): running Eidos callback");
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	// note the focal child during the callback, so we can prevent illegal operations during the callback
	SLiMEidosBlockType old_executing_block_type = community_.executing_block_type_;
	community_.executing_block_type_ = SLiMEidosBlockType::SLiMEidosRecombinationCallback;
	
	bool crossovers_changed = false;
	EidosValue_SP local_crossovers_ptr;
	
	for (SLiMEidosBlock *recombination_callback : p_recombination_callbacks)
	{
		if (recombination_callback->block_active_)
		{
			if (recombination_callback->chromosome_id_ != -1)
			{
				// check that this callback applies to the focal chromosome
				int64_t focal_chromosome_id = p_haplosome1->AssociatedChromosome()->ID();
				
				if (recombination_callback->chromosome_id_ != focal_chromosome_id)
					continue;
			}
			
#if DEBUG_POINTS_ENABLED
			// SLiMgui debugging point
			EidosDebugPointIndent indenter;
			
			{
				EidosInterpreterDebugPointsSet *debug_points = community_.DebugPoints();
				EidosToken *decl_token = recombination_callback->root_node_->token_;
				
				if (debug_points && debug_points->set.size() && (decl_token->token_line_ != -1) &&
					(debug_points->set.find(decl_token->token_line_) != debug_points->set.end()))
				{
					SLIM_ERRSTREAM << EidosDebugPointIndent::Indent() << "#DEBUG recombination(";
					if (recombination_callback->subpopulation_id_ != -1)
						SLIM_ERRSTREAM << "p" << recombination_callback->subpopulation_id_;
					SLIM_ERRSTREAM << ")";
					
					if (recombination_callback->block_id_ != -1)
						SLIM_ERRSTREAM << " s" << recombination_callback->block_id_;
					
					SLIM_ERRSTREAM << " (line " << (decl_token->token_line_ + 1) << community_.DebugPointInfo() << ")" << std::endl;
					indenter.indent();
				}
			}
#endif
			
			// The callback is active, so we need to execute it
			EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &community_.SymbolTable());
			EidosSymbolTable client_symbols(EidosSymbolTableType::kLocalVariablesTable, &callback_symbols);
			EidosFunctionMap &function_map = community_.FunctionMap();
			EidosInterpreter interpreter(recombination_callback->compound_statement_node_, client_symbols, function_map, &community_, SLIM_OUTSTREAM, SLIM_ERRSTREAM);
			
			if (recombination_callback->contains_self_)
				callback_symbols.InitializeConstantSymbolEntry(recombination_callback->SelfSymbolTableEntry());		// define "self"
			
			// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
			// We can use that method because we know the lifetime of the symbol table is shorter than that of
			// the value objects, and we know that the values we are setting here will not change (the objects
			// referred to by the values may change, but the values themselves will not change).
			if (recombination_callback->contains_individual_)
				callback_symbols.InitializeConstantSymbolEntry(gID_individual, p_parent->CachedEidosValue());
			if (recombination_callback->contains_haplosome1_)
				callback_symbols.InitializeConstantSymbolEntry(gID_haplosome1, p_haplosome1->CachedEidosValue());
			if (recombination_callback->contains_haplosome2_)
				callback_symbols.InitializeConstantSymbolEntry(gID_haplosome2, p_haplosome2->CachedEidosValue());
			if (recombination_callback->contains_subpop_)
				callback_symbols.InitializeConstantSymbolEntry(gID_subpop, p_parent->subpopulation_->SymbolTableEntry().second);
			
			// All the variable entries for the crossovers and gene conversion start/end points
			if (recombination_callback->contains_breakpoints_)
			{
				if (!local_crossovers_ptr)
					local_crossovers_ptr = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(p_crossovers));
				client_symbols.SetValueForSymbolNoCopy(gID_breakpoints, local_crossovers_ptr);
			}
			
			try
			{
				// Interpret the script; the result from the interpretation must be a singleton logical, T if breakpoints have been changed, F otherwise
				EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(recombination_callback->script_);
				EidosValue *result = result_SP.get();
				
				if ((result->Type() != EidosValueType::kValueLogical) || (result->Count() != 1))
					EIDOS_TERMINATION << "ERROR (Population::ApplyRecombinationCallbacks): recombination() callbacks must provide a logical singleton return value." << EidosTerminate(recombination_callback->identifier_token_);
				
#if DEBUG
				// this checks the value type at runtime
				eidos_logical_t breakpoints_changed = result->LogicalData()[0];
#else
				// unsafe cast for speed
				eidos_logical_t breakpoints_changed = ((EidosValue_Logical *)result)->data()[0];
#endif
				
				// If the callback says that breakpoints were changed, check for an actual change in value for the variables referenced by the callback
				if (breakpoints_changed)
				{
					if (recombination_callback->contains_breakpoints_)
					{
						EidosValue_SP new_crossovers = client_symbols.GetValueOrRaiseForSymbol(gID_breakpoints);
						
						if (new_crossovers != local_crossovers_ptr)
						{
							if (new_crossovers->Type() != EidosValueType::kValueInt)
								EIDOS_TERMINATION << "ERROR (Population::ApplyRecombinationCallbacks): recombination() callbacks must provide output values (breakpoints) of type integer." << EidosTerminate(recombination_callback->identifier_token_);
							
							new_crossovers.swap(local_crossovers_ptr);
							crossovers_changed = true;
						}
					}
				}
			}
			catch (...)
			{
				throw;
			}
		}
	}
	
	// Read out the final values of breakpoint vectors that changed
	bool breakpoints_changed = false;
	
	if (crossovers_changed)
	{
		int count = local_crossovers_ptr->Count();
		
		p_crossovers.resize(count);		// zero-fills only new entries at the margin, so is minimally wasteful
		
		const int64_t *new_crossover_data = local_crossovers_ptr->IntData();
		slim_position_t *p_crossovers_data = p_crossovers.data();
		
		for (int value_index = 0; value_index < count; ++value_index)
			p_crossovers_data[value_index] = (slim_position_t)new_crossover_data[value_index];
		
		breakpoints_changed = true;
	}
	
	community_.executing_block_type_ = old_executing_block_type;
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosRecombinationCallback)]);
#endif
	
	return breakpoints_changed;
}

// generate a child haplosome from parental haplosomes, with recombination, gene conversion, and mutation
template <const bool f_treeseq, const bool f_callbacks>
void Population::HaplosomeCrossed(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<SLiMEidosBlock*> *p_recombination_callbacks, std::vector<SLiMEidosBlock*> *p_mutation_callbacks)
{
#if DEBUG
	// This method is designed to run in parallel, but only if no callbacks are enabled
	if (p_recombination_callbacks || p_mutation_callbacks)
		THREAD_SAFETY_IN_ANY_PARALLEL("Population::HaplosomeCrossed(): recombination and mutation callbacks are not allowed when executing in parallel");
	
	if (!p_child_haplosome.individual_)
		EIDOS_TERMINATION << "ERROR (Population::HaplosomeCrossed): (internal error) individual_ pointer for child haplosome not set." << EidosTerminate();
	
	// BCH 9/20/2024: With the multichomosome redesign, the child and parent haplosome indices must always match; we are generating a
	// new offspring haplosome from parental haplosomes of the exact same chromosome (not just the same chomosome type).
	slim_chromosome_index_t chromosome_index = p_child_haplosome.chromosome_index_;
	slim_chromosome_index_t parent1_chromosome_index = parent_haplosome_1->chromosome_index_;
	slim_chromosome_index_t parent2_chromosome_index = parent_haplosome_2->chromosome_index_;
	
	if ((parent1_chromosome_index != chromosome_index) || (parent2_chromosome_index != chromosome_index))
		EIDOS_TERMINATION << "ERROR (Population::HaplosomeCrossed): (internal error) mismatch between parent and child chromosomes (child chromosome index == " << chromosome_index << ", parent 1 == " << parent1_chromosome_index << ", parent 2 == " << parent2_chromosome_index << ")." << EidosTerminate();
	
	if (p_child_haplosome.IsNull() || parent_haplosome_1->IsNull() || parent_haplosome_2->IsNull())
		EIDOS_TERMINATION << "ERROR (Population::HaplosomeCrossed): (internal error) null haplosomes cannot be passed to HaplosomeCrossed()." << EidosTerminate();
	
	Haplosome::DebugCheckStructureMatch(parent_haplosome_1, parent_haplosome_2, &p_child_haplosome, &p_chromosome);
#endif
#if SLIM_CLEAR_HAPLOSOMES
	// start with a clean slate in the child haplosome; we now expect child haplosomes to be cleared for us
	p_child_haplosome.check_cleared_to_nullptr();
#endif
	
	// swap strands in half of cases to assure random assortment (or in all cases, if use_only_strand_1 == true, meaning that crossover cannot occur)
	bool do_swap = true;				// if true, we are to swap the parental strands at the beginning 50% of the time
	
	if (do_swap)
	{
		if (Eidos_RandomBool(EIDOS_STATE_RNG(omp_get_thread_num())))
			std::swap(parent_haplosome_1, parent_haplosome_2);
	}
	
	// some behaviors -- which callbacks to use, which recombination/mutation rate to use, subpop of origin for mutations, etc. --
	// depend upon characteristics of the first parent, so we fetch the necessary properties here
	Individual *parent_individual = parent_haplosome_1->individual_;
	Subpopulation *source_subpop = parent_individual->subpopulation_;
	IndividualSex parent_sex = parent_individual->sex_;
	
	// determine how many mutations and breakpoints we have
	int num_mutations, num_breakpoints;
	
#if defined(__GNUC__) && !defined(__clang__)
	// Work around GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=27557
	static thread_local std::vector<slim_position_t> all_breakpoints;
#else
	static std::vector<slim_position_t> all_breakpoints;	// avoid buffer reallocs, etc.; we are guaranteed not to be re-entrant by the addX() methods
#pragma omp threadprivate (all_breakpoints)
#endif
	
	all_breakpoints.resize(0);
	
	std::vector<slim_position_t> heteroduplex;				// a vector of heteroduplex starts/ends, used only with complex gene conversion tracts
															// this is not static since we don't want to call resize(0) every time for a rare edge case
	
	{
#ifdef USE_GSL_POISSON
		// When using the GSL's poisson draw, we have to draw the mutation count and breakpoint count separately;
		// the DrawMutationAndBreakpointCounts() method does not support USE_GSL_POISSON
		num_mutations = chromosome.DrawMutationCount(p_parent_sex);
		num_breakpoints = chromosome.DrawBreakpointCount(p_parent_sex);
#else
		// get both the number of mutations and the number of breakpoints here; this allows us to draw both jointly, super fast!
		p_chromosome.DrawMutationAndBreakpointCounts(parent_sex, &num_mutations, &num_breakpoints);
#endif
		
		//std::cout << num_mutations << " mutations, " << num_breakpoints << " breakpoints" << std::endl;
		
		// draw the breakpoints based on the recombination rate map, and sort and unique the result
		// we don't use Chromosome::DrawBreakpoints(), for speed, but this code mirrors it
		if (num_breakpoints)
		{
			if (p_chromosome.using_DSB_model_)
				p_chromosome._DrawDSBBreakpoints(parent_sex, num_breakpoints, all_breakpoints, heteroduplex);
			else
				p_chromosome._DrawCrossoverBreakpoints(parent_sex, num_breakpoints, all_breakpoints);
			
			// all_breakpoints is sorted and uniqued at this point
			
			if (f_callbacks && p_recombination_callbacks)
			{
				// a non-zero number of breakpoints, with recombination callbacks
				if (p_chromosome.using_DSB_model_ && (p_chromosome.simple_conversion_fraction_ != 1.0))
					EIDOS_TERMINATION << "ERROR (Population::HaplosomeCrossed): recombination() callbacks may not be used when complex gene conversion tracts are in use, since recombination() callbacks have no support for heteroduplex regions." << EidosTerminate();
				
				bool breaks_changed = ApplyRecombinationCallbacks(parent_individual, parent_haplosome_1, parent_haplosome_2, all_breakpoints, *p_recombination_callbacks);
				num_breakpoints = (int)all_breakpoints.size();
				
				// we only sort/unique if the breakpoints have changed, since they were sorted/uniqued before
				if (breaks_changed && (num_breakpoints > 1))
				{
					std::sort(all_breakpoints.begin(), all_breakpoints.end());
					all_breakpoints.erase(unique(all_breakpoints.begin(), all_breakpoints.end()), all_breakpoints.end());
				}
			}
			else
			{
				// a non-zero number of breakpoints, without recombination callbacks
			}
		}
		else if (f_callbacks && p_recombination_callbacks)
		{
			// zero breakpoints from the SLiM core, but we have recombination() callbacks
			if (p_chromosome.using_DSB_model_ && (p_chromosome.simple_conversion_fraction_ != 1.0))
				EIDOS_TERMINATION << "ERROR (Population::HaplosomeCrossed): recombination() callbacks may not be used when complex gene conversion tracts are in use, since recombination() callbacks have no support for heteroduplex regions." << EidosTerminate();
			
			ApplyRecombinationCallbacks(parent_individual, parent_haplosome_1, parent_haplosome_2, all_breakpoints, *p_recombination_callbacks);
			num_breakpoints = (int)all_breakpoints.size();
			
			if (num_breakpoints > 1)
			{
				std::sort(all_breakpoints.begin(), all_breakpoints.end());
				all_breakpoints.erase(unique(all_breakpoints.begin(), all_breakpoints.end()), all_breakpoints.end());
			}
		}
		else
		{
			// no breakpoints or DSBs, no recombination() callbacks
		}
	}
	
	// we need a defined end breakpoint, so we add it now
	all_breakpoints.emplace_back(p_chromosome.last_position_mutrun_ + 10);
	
	// A leading zero in the breakpoints vector switches copy strands before copying begins.
	// We want to handle that up front, primarily because we don't want to record it in treeseq.
	// We only need to do this once, since the breakpoints vector is sorted/uniqued here.
	// For efficiency, we switch to a head pointer here; DO NOT USE all_breakpoints HEREAFTER!
	slim_position_t *breakpoints_ptr = all_breakpoints.data();
	int breakpoints_count = (int)all_breakpoints.size();
	
	if (*breakpoints_ptr == 0)	// guaranteed to exist since we added an element above
	{
		breakpoints_ptr++;
		breakpoints_count--;
		std::swap(parent_haplosome_1, parent_haplosome_2);
	}
	
	// TREE SEQUENCE RECORDING
	bool recording_tree_sequence = f_treeseq;
	bool recording_tree_sequence_mutations = f_treeseq && species_.RecordingTreeSequenceMutations();
	
	if (recording_tree_sequence)
	{
#pragma omp critical (TreeSeqNewHaplosome)
		{
			species_.RecordNewHaplosome(breakpoints_ptr, breakpoints_count, &p_child_haplosome, parent_haplosome_1, parent_haplosome_2);
		}
	}
	
	// mutations are usually rare, so let's streamline the case where none occur
	if (num_mutations == 0)
	{
		if (num_breakpoints == 0)
		{
			//
			// no mutations and no crossovers, so the child haplosome is just a copy of the parental haplosome
			//
			
			p_child_haplosome.copy_from_haplosome(*parent_haplosome_1);
		}
		else
		{
			//
			// no mutations, but we do have crossovers, so we just need to interleave the two parental haplosomes
			//
			
			Mutation *mut_block_ptr = gSLiM_Mutation_Block;
			Haplosome *parent_haplosome = parent_haplosome_1;
			slim_position_t mutrun_length = p_child_haplosome.mutrun_length_;
			int mutrun_count = p_child_haplosome.mutrun_count_;
			int first_uncompleted_mutrun = 0;
			
			for (int break_index = 0; break_index < breakpoints_count; break_index++)
			{
				slim_position_t breakpoint = breakpoints_ptr[break_index];
				slim_mutrun_index_t break_mutrun_index = (slim_mutrun_index_t)(breakpoint / mutrun_length);
				
				// Copy over mutation runs until we arrive at the run in which the breakpoint occurs
				while (break_mutrun_index > first_uncompleted_mutrun)
				{
					p_child_haplosome.mutruns_[first_uncompleted_mutrun] = parent_haplosome->mutruns_[first_uncompleted_mutrun];
					++first_uncompleted_mutrun;
					
					if (first_uncompleted_mutrun >= mutrun_count)
						break;
				}
				
				// Now we are supposed to process a breakpoint in first_uncompleted_mutrun; check whether that means we're done
				if (first_uncompleted_mutrun >= mutrun_count)
					break;
				
				// The break occurs to the left of the base position of the breakpoint; check whether that is between runs
				if (breakpoint > break_mutrun_index * mutrun_length)
				{
					// The breakpoint occurs *inside* the run, so process the run by copying mutations and switching strands
					int this_mutrun_index = first_uncompleted_mutrun;
					const MutationIndex *parent1_iter		= parent_haplosome_1->mutruns_[this_mutrun_index]->begin_pointer_const();
					const MutationIndex *parent2_iter		= parent_haplosome_2->mutruns_[this_mutrun_index]->begin_pointer_const();
					const MutationIndex *parent1_iter_max	= parent_haplosome_1->mutruns_[this_mutrun_index]->end_pointer_const();
					const MutationIndex *parent2_iter_max	= parent_haplosome_2->mutruns_[this_mutrun_index]->end_pointer_const();
					const MutationIndex *parent_iter		= parent1_iter;
					const MutationIndex *parent_iter_max	= parent1_iter_max;
					MutationRunContext &mutrun_context_LOCKED = p_chromosome.ChromosomeMutationRunContextForMutationRunIndex(this_mutrun_index);
					MutationRun *child_mutrun = p_child_haplosome.WillCreateRun_LOCKED(this_mutrun_index, mutrun_context_LOCKED);
					
					while (true)
					{
						// while there are still old mutations in the parent before the current breakpoint...
						while (parent_iter != parent_iter_max)
						{
							MutationIndex current_mutation = *parent_iter;
							
							if ((mut_block_ptr + current_mutation)->position_ >= breakpoint)
								break;
							
							// add the old mutation; no need to check for a duplicate here since the parental haplosome is already duplicate-free
							child_mutrun->emplace_back(current_mutation);
							
							parent_iter++;
						}
						
						// we have reached the breakpoint, so swap parents; we want the "current strand" variables to change, so no std::swap()
						parent1_iter = parent2_iter;	parent1_iter_max = parent2_iter_max;	parent_haplosome_1 = parent_haplosome_2;
						parent2_iter = parent_iter;		parent2_iter_max = parent_iter_max;		parent_haplosome_2 = parent_haplosome;
						parent_iter = parent1_iter;		parent_iter_max = parent1_iter_max;		parent_haplosome = parent_haplosome_1;
						
						// skip over anything in the new parent that occurs prior to the breakpoint; it was not the active strand
						while (parent_iter != parent_iter_max && (mut_block_ptr + *parent_iter)->position_ < breakpoint)
							parent_iter++;
						
						// we have now handled the current breakpoint, so move on to the next breakpoint; advance the enclosing for loop here
						break_index++;
						
						// if we just handled the last breakpoint, which is guaranteed to be at or beyond lastPosition+1, then we are done
						if (break_index == breakpoints_count)
							break;
						
						// otherwise, figure out the new breakpoint, and continue looping on the current mutation run, which needs to be finished
						breakpoint = breakpoints_ptr[break_index];
						break_mutrun_index = (slim_mutrun_index_t)(breakpoint / mutrun_length);
						
						// if the next breakpoint is outside this mutation run, then finish the run and break out
						if (break_mutrun_index > this_mutrun_index)
						{
							while (parent_iter != parent_iter_max)
								child_mutrun->emplace_back(*(parent_iter++));
							
							break_index--;	// the outer loop will want to handle the current breakpoint again at the mutation-run level
							break;
						}
					}
					
					// We have completed this run
					++first_uncompleted_mutrun;
				}
				else
				{
					// The breakpoint occurs *between* runs, so just switch parent strands and the breakpoint is handled
					parent_haplosome_1 = parent_haplosome_2;
					parent_haplosome_2 = parent_haplosome;
					parent_haplosome = parent_haplosome_1;
				}
			}
		}
	}
	else
	{
		// we have at least one new mutation, so set up for that case (which splits into two cases below)
		
		// Generate all of the mutation positions as a separate stage, because we need to unique them.  See DrawSortedUniquedMutationPositions.
#if defined(__GNUC__) && !defined(__clang__)
		// Work around GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=27557
		static thread_local std::vector<std::pair<slim_position_t, GenomicElement *>> mut_positions;
#else
		static std::vector<std::pair<slim_position_t, GenomicElement *>> mut_positions;
#pragma omp threadprivate (mut_positions)
#endif
		
		mut_positions.resize(0);
		
		num_mutations = p_chromosome.DrawSortedUniquedMutationPositions(num_mutations, parent_sex, mut_positions);
		
		// Create vector with the mutations to be added
#if defined(__GNUC__) && !defined(__clang__)
		// Work around GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=27557
		static thread_local std::vector<MutationIndex> mutations_to_add;
#else
		static std::vector<MutationIndex> mutations_to_add;
#pragma omp threadprivate (mutations_to_add)
#endif
		
		mutations_to_add.resize(0);
		
#ifdef _OPENMP
		bool saw_error_in_critical = false;
#endif
		
		// BCH 7/29/2023: I tried making a simple code path here that generated the new MutationIndex values in a critical region and then
		// did all the rest of the work outside the critical region.  It wasn't a noticeable win; mutation generation just isn't that
		// central of a bottleneck.  If you're making so many mutations that contention for this critical region matters, you're probably
		// completely bogged down in recombination and mutation registry maintenance.  Not worth the added code complexity.
#pragma omp critical (MutationAlloc)
		{
			try {
				if (species_.IsNucleotideBased() || (f_callbacks && p_mutation_callbacks))
				{
					// In nucleotide-based models, p_chromosome.DrawNewMutationExtended() will return new mutations to us with nucleotide_ set correctly.
					// To do that, and to adjust mutation rates correctly, it needs to know which parental haplosome the mutation occurred on the
					// background of, so that it can get the original nucleotide or trinucleotide context.  This code path is also used if mutation()
					// callbacks are enabled, since that also wants to be able to see the context of the mutation.
					for (int k = 0; k < num_mutations; k++)
					{
						MutationIndex new_mutation = p_chromosome.DrawNewMutationExtended(mut_positions[k], source_subpop->subpopulation_id_, community_.Tick(), parent_haplosome_1, parent_haplosome_2, breakpoints_ptr, breakpoints_count, p_mutation_callbacks);
						
						if (new_mutation != -1)
							mutations_to_add.emplace_back(new_mutation);			// positions are already sorted
						
						// see further comments below, in the non-nucleotide case; they apply here as well
					}
				}
				else
				{
					// In non-nucleotide-based models, p_chromosome.DrawNewMutation() will return new mutations to us with nucleotide_ == -1
					for (int k = 0; k < num_mutations; k++)
					{
						MutationIndex new_mutation = p_chromosome.DrawNewMutation(mut_positions[k], source_subpop->subpopulation_id_, community_.Tick());
						
						mutations_to_add.emplace_back(new_mutation);			// positions are already sorted
						
						// no need to worry about pure_neutral_ or all_pure_neutral_DFE_ here; the mutation is drawn from a registered genomic element type
						// we can't handle the stacking policy here, since we don't yet know what the context of the new mutation will be; we do it below
						// we add the new mutation to the registry below, if the stacking policy says the mutation can actually be added
					}
				}
			} catch (...) {
				// DrawNewMutation() / DrawNewMutationExtended() can raise, but it is (presumably) rare; we can leak mutations here
				// It occurs primarily with type 's' DFEs; an error in the user's script can cause a raise through here.
#ifdef _OPENMP
				saw_error_in_critical = true;		// can't throw from a critical region, even when not inside a parallel region!
#else
				throw;
#endif
			}
		}	// end #pragma omp critical (MutationAlloc)
		
#ifdef _OPENMP
		if (saw_error_in_critical)
		{
			// Note that the previous error message is still in gEidosTermination, so we just tack an addendum onto it and re-raise, in effect
			EIDOS_TERMINATION << "ERROR (Population::HaplosomeCrossed): An exception was caught inside a critical region." << EidosTerminate();
		}
#endif
		
		Mutation *mut_block_ptr = gSLiM_Mutation_Block;
		const MutationIndex *mutation_iter		= mutations_to_add.data();
		const MutationIndex *mutation_iter_max	= mutation_iter + mutations_to_add.size();
		
		MutationIndex mutation_iter_mutation_index;
		slim_position_t mutation_iter_pos;
		
		if (mutation_iter != mutation_iter_max) {
			mutation_iter_mutation_index = *mutation_iter;
			mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
		} else {
			mutation_iter_mutation_index = -1;
			mutation_iter_pos = SLIM_INF_BASE_POSITION;
		}
		
		slim_position_t mutrun_length = p_child_haplosome.mutrun_length_;
		int mutrun_count = p_child_haplosome.mutrun_count_;
		slim_mutrun_index_t mutation_mutrun_index = (slim_mutrun_index_t)(mutation_iter_pos / mutrun_length);
		
		Haplosome *parent_haplosome = parent_haplosome_1;
		int first_uncompleted_mutrun = 0;
		
		if (num_breakpoints == 0)
		{
			//
			// mutations without breakpoints; we have to be careful here not to touch the second strand, because it could be null
			//
			
			while (true)
			{
				// Copy over mutation runs until we arrive at the run in which the mutation occurs
				while (mutation_mutrun_index > first_uncompleted_mutrun)
				{
					p_child_haplosome.mutruns_[first_uncompleted_mutrun] = parent_haplosome->mutruns_[first_uncompleted_mutrun];
					++first_uncompleted_mutrun;
					
					if (first_uncompleted_mutrun >= mutrun_count)
						break;
				}
				
				if (first_uncompleted_mutrun >= mutrun_count)
					break;
				
				// The mutation occurs *inside* the run, so process the run by copying mutations
				int this_mutrun_index = first_uncompleted_mutrun;
				const MutationIndex *parent_iter		= parent_haplosome->mutruns_[this_mutrun_index]->begin_pointer_const();
				const MutationIndex *parent_iter_max	= parent_haplosome->mutruns_[this_mutrun_index]->end_pointer_const();
				MutationRunContext &mutrun_context_LOCKED = p_chromosome.ChromosomeMutationRunContextForMutationRunIndex(this_mutrun_index);
				MutationRun *child_mutrun = p_child_haplosome.WillCreateRun_LOCKED(this_mutrun_index, mutrun_context_LOCKED);
				
				// add any additional new mutations that occur before the end of the mutation run; there is at least one
				do
				{
					// add any parental mutations that occur before or at the next new mutation's position
					while (parent_iter != parent_iter_max)
					{
						MutationIndex current_mutation = *parent_iter;
						slim_position_t current_mutation_pos = (mut_block_ptr + current_mutation)->position_;
						
						if (current_mutation_pos > mutation_iter_pos)
							break;
						
						child_mutrun->emplace_back(current_mutation);
						parent_iter++;
					}
					
					// add the new mutation, which might overlap with the last added old mutation
					Mutation *new_mut = mut_block_ptr + mutation_iter_mutation_index;
					MutationType *new_mut_type = new_mut->mutation_type_ptr_;
					
					if (child_mutrun->enforce_stack_policy_for_addition(new_mut->position_, new_mut_type))
					{
						// The mutation was passed by the stacking policy, so we can add it to the child haplosome and the registry
						child_mutrun->emplace_back(mutation_iter_mutation_index);
						
						if (new_mut->state_ != MutationState::kInRegistry)
						{
							#pragma omp critical (MutationRegistryAdd)
							{
								MutationRegistryAdd(new_mut);
							}
						}
						
						// TREE SEQUENCE RECORDING
						if (recording_tree_sequence_mutations)
						{
#pragma omp critical (TreeSeqNewDerivedState)
							{
								species_.RecordNewDerivedState(&p_child_haplosome, new_mut->position_, *child_mutrun->derived_mutation_ids_at_position(new_mut->position_));
							}
						}
					}
					else if (new_mut->state_ == MutationState::kNewMutation)	// new and needs to be disposed of
					{
						// The mutation was rejected by the stacking policy, so we have to release it
#pragma omp critical (MutationAlloc)
						{
							new_mut->Release_PARALLEL();
						}
					}
					
					if (++mutation_iter != mutation_iter_max) {
						mutation_iter_mutation_index = *mutation_iter;
						mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
					} else {
						mutation_iter_mutation_index = -1;
						mutation_iter_pos = SLIM_INF_BASE_POSITION;
					}
					
					mutation_mutrun_index = (slim_mutrun_index_t)(mutation_iter_pos / mutrun_length);
				}
				while (mutation_mutrun_index == this_mutrun_index);
				
				// finish up any parental mutations that come after the last new mutation in the mutation run
				while (parent_iter != parent_iter_max)
					child_mutrun->emplace_back(*(parent_iter++));
				
				// We have completed this run
				++first_uncompleted_mutrun;
				
				if (first_uncompleted_mutrun >= mutrun_count)
					break;
			}
		}
		else
		{
			//
			// mutations and crossovers; this is the most complex case
			//
			
			int break_index = 0;
			slim_position_t breakpoint = breakpoints_ptr[break_index];
			slim_mutrun_index_t break_mutrun_index = (slim_mutrun_index_t)(breakpoint / mutrun_length);
			
			while (true)	// loop over breakpoints until we have handled the last one, which comes at the end
			{
				if (mutation_mutrun_index < break_mutrun_index)
				{
					// Copy over mutation runs until we arrive at the run in which the mutation occurs
					while (mutation_mutrun_index > first_uncompleted_mutrun)
					{
						p_child_haplosome.mutruns_[first_uncompleted_mutrun] = parent_haplosome->mutruns_[first_uncompleted_mutrun];
						++first_uncompleted_mutrun;
						
						// We can't be done, since we have a mutation waiting to be placed, so we don't need to check
					}
					
					// Mutations can't occur between mutation runs the way breakpoints can, so we don't need to check that either
				}
				else
				{
					// Copy over mutation runs until we arrive at the run in which the breakpoint occurs
					while (break_mutrun_index > first_uncompleted_mutrun)
					{
						p_child_haplosome.mutruns_[first_uncompleted_mutrun] = parent_haplosome->mutruns_[first_uncompleted_mutrun];
						++first_uncompleted_mutrun;
						
						if (first_uncompleted_mutrun >= mutrun_count)
							break;
					}
					
					// Now we are supposed to process a breakpoint in first_uncompleted_mutrun; check whether that means we're done
					if (first_uncompleted_mutrun >= mutrun_count)
						break;
					
					// If the breakpoint occurs *between* runs, just switch parent strands and the breakpoint is handled
					if (breakpoint == break_mutrun_index * mutrun_length)
					{
						parent_haplosome_1 = parent_haplosome_2;
						parent_haplosome_2 = parent_haplosome;
						parent_haplosome = parent_haplosome_1;
						
						// go to next breakpoint; this advances the for loop
						if (++break_index == breakpoints_count)
							break;
						
						breakpoint = breakpoints_ptr[break_index];
						break_mutrun_index = (slim_mutrun_index_t)(breakpoint / mutrun_length);
						
						continue;
					}
				}
				
				// The event occurs *inside* the run, so process the run by copying mutations and switching strands
				int this_mutrun_index = first_uncompleted_mutrun;
				MutationRunContext &mutrun_context_LOCKED = p_chromosome.ChromosomeMutationRunContextForMutationRunIndex(this_mutrun_index);
				MutationRun *child_mutrun = p_child_haplosome.WillCreateRun_LOCKED(this_mutrun_index, mutrun_context_LOCKED);
				const MutationIndex *parent1_iter		= parent_haplosome_1->mutruns_[this_mutrun_index]->begin_pointer_const();
				const MutationIndex *parent1_iter_max	= parent_haplosome_1->mutruns_[this_mutrun_index]->end_pointer_const();
				const MutationIndex *parent_iter		= parent1_iter;
				const MutationIndex *parent_iter_max	= parent1_iter_max;
				
				if (break_mutrun_index == this_mutrun_index)
				{
					const MutationIndex *parent2_iter		= parent_haplosome_2->mutruns_[this_mutrun_index]->begin_pointer_const();
					const MutationIndex *parent2_iter_max	= parent_haplosome_2->mutruns_[this_mutrun_index]->end_pointer_const();
					
					if (mutation_mutrun_index == this_mutrun_index)
					{
						//
						// =====  this_mutrun_index has both breakpoint(s) and new mutation(s); this is the really nasty case
						//
						
						while (true)
						{
							// while there are still old mutations in the parent before the current breakpoint...
							while (parent_iter != parent_iter_max)
							{
								MutationIndex current_mutation = *parent_iter;
								slim_position_t current_mutation_pos = (mut_block_ptr + current_mutation)->position_;
								
								if (current_mutation_pos >= breakpoint)
									break;
								
								// add any new mutations that occur before the parental mutation; we know the parental mutation is in this run, so these are too
								while (mutation_iter_pos < current_mutation_pos)
								{
									Mutation *new_mut = mut_block_ptr + mutation_iter_mutation_index;
									MutationType *new_mut_type = new_mut->mutation_type_ptr_;
									
									if (child_mutrun->enforce_stack_policy_for_addition(new_mut->position_, new_mut_type))
									{
										// The mutation was passed by the stacking policy, so we can add it to the child haplosome and the registry
										child_mutrun->emplace_back(mutation_iter_mutation_index);
										
										if (new_mut->state_ != MutationState::kInRegistry)
										{
											#pragma omp critical (MutationRegistryAdd)
											{
												MutationRegistryAdd(new_mut);
											}
										}
										
										// TREE SEQUENCE RECORDING
										if (recording_tree_sequence_mutations)
										{
#pragma omp critical (TreeSeqNewDerivedState)
											{
												species_.RecordNewDerivedState(&p_child_haplosome, new_mut->position_, *child_mutrun->derived_mutation_ids_at_position(new_mut->position_));
											}
										}
									}
									else if (new_mut->state_ == MutationState::kNewMutation)	// new and needs to be disposed of
									{
										// The mutation was rejected by the stacking policy, so we have to release it
#pragma omp critical (MutationAlloc)
										{
											new_mut->Release_PARALLEL();
										}
									}
									
									if (++mutation_iter != mutation_iter_max) {
										mutation_iter_mutation_index = *mutation_iter;
										mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
									} else {
										mutation_iter_mutation_index = -1;
										mutation_iter_pos = SLIM_INF_BASE_POSITION;
									}
									
									mutation_mutrun_index = (slim_mutrun_index_t)(mutation_iter_pos / mutrun_length);
								}
								
								// add the old mutation; no need to check for a duplicate here since the parental haplosome is already duplicate-free
								child_mutrun->emplace_back(current_mutation);
								
								parent_iter++;
							}
							
							// add any new mutations that occur before the breakpoint; for these we have to check that they fall within this mutation run
							while ((mutation_iter_pos < breakpoint) && (mutation_mutrun_index == this_mutrun_index))
							{
								Mutation *new_mut = mut_block_ptr + mutation_iter_mutation_index;
								MutationType *new_mut_type = new_mut->mutation_type_ptr_;
								
								if (child_mutrun->enforce_stack_policy_for_addition(new_mut->position_, new_mut_type))
								{
									// The mutation was passed by the stacking policy, so we can add it to the child haplosome and the registry
									child_mutrun->emplace_back(mutation_iter_mutation_index);
									
									if (new_mut->state_ != MutationState::kInRegistry)
									{
										#pragma omp critical (MutationRegistryAdd)
										{
											MutationRegistryAdd(new_mut);
										}
									}
									
									// TREE SEQUENCE RECORDING
									if (recording_tree_sequence_mutations)
									{
#pragma omp critical (TreeSeqNewDerivedState)
										{
											species_.RecordNewDerivedState(&p_child_haplosome, new_mut->position_, *child_mutrun->derived_mutation_ids_at_position(new_mut->position_));
										}
									}
								}
								else if (new_mut->state_ == MutationState::kNewMutation)	// new and needs to be disposed of
								{
									// The mutation was rejected by the stacking policy, so we have to release it
#pragma omp critical (MutationAlloc)
									{
										new_mut->Release_PARALLEL();
									}
								}
								
								if (++mutation_iter != mutation_iter_max) {
									mutation_iter_mutation_index = *mutation_iter;
									mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
								} else {
									mutation_iter_mutation_index = -1;
									mutation_iter_pos = SLIM_INF_BASE_POSITION;
								}
								
								mutation_mutrun_index = (slim_mutrun_index_t)(mutation_iter_pos / mutrun_length);
							}
							
							// we have finished the parental mutation run; if the breakpoint we are now working toward lies beyond the end of the
							// current mutation run, then we have completed this run and can exit to the outer loop which will handle the rest
							if (break_mutrun_index > this_mutrun_index)
								break;		// the outer loop will want to handle this breakpoint again at the mutation-run level
							
							// we have reached the breakpoint, so swap parents; we want the "current strand" variables to change, so no std::swap()
							parent1_iter = parent2_iter;	parent1_iter_max = parent2_iter_max;	parent_haplosome_1 = parent_haplosome_2;
							parent2_iter = parent_iter;		parent2_iter_max = parent_iter_max;		parent_haplosome_2 = parent_haplosome;
							parent_iter = parent1_iter;		parent_iter_max = parent1_iter_max;		parent_haplosome = parent_haplosome_1;
							
							// skip over anything in the new parent that occurs prior to the breakpoint; it was not the active strand
							while (parent_iter != parent_iter_max && (mut_block_ptr + *parent_iter)->position_ < breakpoint)
								parent_iter++;
							
							// we have now handled the current breakpoint, so move on; if we just handled the last breakpoint, then we are done
							if (++break_index == breakpoints_count)
								break;
							
							// otherwise, figure out the new breakpoint, and continue looping on the current mutation run, which needs to be finished
							breakpoint = breakpoints_ptr[break_index];
							break_mutrun_index = (slim_mutrun_index_t)(breakpoint / mutrun_length);
						}
						
						// if we just handled the last breakpoint, which is guaranteed to be at or beyond lastPosition+1, then we are done
						if (break_index == breakpoints_count)
							break;
						
						// We have completed this run
						++first_uncompleted_mutrun;
					}
					else
					{
						//
						// =====  this_mutrun_index has only breakpoint(s), no new mutations
						//
						
						while (true)
						{
							// while there are still old mutations in the parent before the current breakpoint...
							while (parent_iter != parent_iter_max)
							{
								MutationIndex current_mutation = *parent_iter;
								
								if ((mut_block_ptr + current_mutation)->position_ >= breakpoint)
									break;
								
								// add the old mutation; no need to check for a duplicate here since the parental haplosome is already duplicate-free
								child_mutrun->emplace_back(current_mutation);
								
								parent_iter++;
							}
							
							// we have reached the breakpoint, so swap parents; we want the "current strand" variables to change, so no std::swap()
							parent1_iter = parent2_iter;	parent1_iter_max = parent2_iter_max;	parent_haplosome_1 = parent_haplosome_2;
							parent2_iter = parent_iter;		parent2_iter_max = parent_iter_max;		parent_haplosome_2 = parent_haplosome;
							parent_iter = parent1_iter;		parent_iter_max = parent1_iter_max;		parent_haplosome = parent_haplosome_1;
							
							// skip over anything in the new parent that occurs prior to the breakpoint; it was not the active strand
							while (parent_iter != parent_iter_max && (mut_block_ptr + *parent_iter)->position_ < breakpoint)
								parent_iter++;
							
							// we have now handled the current breakpoint, so move on; if we just handled the last breakpoint, then we are done
							if (++break_index == breakpoints_count)
								break;
							
							// otherwise, figure out the new breakpoint, and continue looping on the current mutation run, which needs to be finished
							breakpoint = breakpoints_ptr[break_index];
							break_mutrun_index = (slim_mutrun_index_t)(breakpoint / mutrun_length);
							
							// if the next breakpoint is outside this mutation run, then finish the run and break out
							if (break_mutrun_index > this_mutrun_index)
							{
								while (parent_iter != parent_iter_max)
									child_mutrun->emplace_back(*(parent_iter++));
								
								break;	// the outer loop will want to handle this breakpoint again at the mutation-run level
							}
						}
						
						// if we just handled the last breakpoint, which is guaranteed to be at or beyond lastPosition+1, then we are done
						if (break_index == breakpoints_count)
							break;
						
						// We have completed this run
						++first_uncompleted_mutrun;
					}
				}
				else if (mutation_mutrun_index == this_mutrun_index)
				{
					//
					// =====  this_mutrun_index has only new mutation(s), no breakpoints
					//
					
					// add any additional new mutations that occur before the end of the mutation run; there is at least one
					do
					{
						// add any parental mutations that occur before or at the next new mutation's position
						while (parent_iter != parent_iter_max)
						{
							MutationIndex current_mutation = *parent_iter;
							slim_position_t current_mutation_pos = (mut_block_ptr + current_mutation)->position_;
							
							if (current_mutation_pos > mutation_iter_pos)
								break;
							
							child_mutrun->emplace_back(current_mutation);
							parent_iter++;
						}
						
						// add the new mutation, which might overlap with the last added old mutation
						Mutation *new_mut = mut_block_ptr + mutation_iter_mutation_index;
						MutationType *new_mut_type = new_mut->mutation_type_ptr_;
						
						if (child_mutrun->enforce_stack_policy_for_addition(new_mut->position_, new_mut_type))
						{
							// The mutation was passed by the stacking policy, so we can add it to the child haplosome and the registry
							child_mutrun->emplace_back(mutation_iter_mutation_index);
							
							if (new_mut->state_ != MutationState::kInRegistry)
							{
								#pragma omp critical (MutationRegistryAdd)
								{
									MutationRegistryAdd(new_mut);
								}
							}
							
							// TREE SEQUENCE RECORDING
							if (recording_tree_sequence_mutations)
							{
#pragma omp critical (TreeSeqNewDerivedState)
								{
									species_.RecordNewDerivedState(&p_child_haplosome, new_mut->position_, *child_mutrun->derived_mutation_ids_at_position(new_mut->position_));
								}
							}
						}
						else if (new_mut->state_ == MutationState::kNewMutation)	// new and needs to be disposed of
						{
							// The mutation was rejected by the stacking policy, so we have to release it
#pragma omp critical (MutationAlloc)
							{
								new_mut->Release_PARALLEL();
							}
						}
						
						if (++mutation_iter != mutation_iter_max) {
							mutation_iter_mutation_index = *mutation_iter;
							mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
						} else {
							mutation_iter_mutation_index = -1;
							mutation_iter_pos = SLIM_INF_BASE_POSITION;
						}
						
						mutation_mutrun_index = (slim_mutrun_index_t)(mutation_iter_pos / mutrun_length);
					}
					while (mutation_mutrun_index == this_mutrun_index);
					
					// finish up any parental mutations that come after the last new mutation in the mutation run
					while (parent_iter != parent_iter_max)
						child_mutrun->emplace_back(*(parent_iter++));
					
					// We have completed this run
					++first_uncompleted_mutrun;
				}
				else
				{
					EIDOS_TERMINATION << "ERROR (Population::HaplosomeCrossed): (internal error) logic fail." << EidosTerminate();
				}
			}
		}
	}
	
	// debugging check
#if 0
	for (int i = 0; i < child_haplosome.mutrun_count_; ++i)
		if (child_haplosome.mutruns_[i].get() == nullptr)
			EIDOS_TERMINATION << "ERROR (Population::HaplosomeCrossed): (internal error) null mutation run left at end of crossover-mutation." << EidosTerminate();
#endif
	
	if (heteroduplex.size() > 0)
		DoHeteroduplexRepair(heteroduplex, breakpoints_ptr, breakpoints_count, parent_haplosome_1, parent_haplosome_2, &p_child_haplosome);
}

template void Population::HaplosomeCrossed<false, false>(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<SLiMEidosBlock*> *p_recombination_callbacks, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
template void Population::HaplosomeCrossed<false, true>(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<SLiMEidosBlock*> *p_recombination_callbacks, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
template void Population::HaplosomeCrossed<true, false>(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<SLiMEidosBlock*> *p_recombination_callbacks, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
template void Population::HaplosomeCrossed<true, true>(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<SLiMEidosBlock*> *p_recombination_callbacks, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);

// generate a child haplosome from parental haplosomes, clonally with mutation
template <const bool f_treeseq, const bool f_callbacks>
void Population::HaplosomeCloned(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome, std::vector<SLiMEidosBlock*> *p_mutation_callbacks)
{
#if DEBUG
	// This method is designed to run in parallel, but only if no callbacks are enabled
	if (p_mutation_callbacks)
		THREAD_SAFETY_IN_ANY_PARALLEL("Population::HaplosomeCloned(): mutation callbacks are not allowed when executing in parallel");
	
	if (!p_child_haplosome.individual_)
		EIDOS_TERMINATION << "ERROR (Population::HaplosomeCloned): (internal error) individual_ pointer for child haplosome not set." << EidosTerminate();
	
	// BCH 9/20/2024: With the multichomosome redesign, the child and parent haplosome types must always match; we are generating a
	// new offspring haplosome clonally for a parental haplosome of the same type
	slim_chromosome_index_t chromosome_index = p_child_haplosome.chromosome_index_;
	slim_chromosome_index_t parent_chromosome_index = parent_haplosome->chromosome_index_;
	
	if (parent_chromosome_index != chromosome_index)
		EIDOS_TERMINATION << "ERROR (Population::HaplosomeCloned): (internal error) mismatch between parent and child chromosomes (child chromosome index == " << chromosome_index << ", parent == " << parent_chromosome_index << ")." << EidosTerminate();
	
	if (p_child_haplosome.IsNull() || parent_haplosome->IsNull())
		EIDOS_TERMINATION << "ERROR (Population::HaplosomeCloned): (internal error) null haplosomes cannot be passed to HaplosomeCloned()." << EidosTerminate();
	
	Haplosome::DebugCheckStructureMatch(parent_haplosome, &p_child_haplosome, &p_chromosome);
#endif
#if SLIM_CLEAR_HAPLOSOMES
	// start with a clean slate in the child haplosome; we now expect child haplosomes to be cleared for us
	p_child_haplosome.check_cleared_to_nullptr();
#endif
	
	// some behaviors -- which callbacks to use, which recombination/mutation rate to use, subpop of origin for mutations, etc. --
	// depend upon characteristics of the first parent, so we fetch the necessary properties here
	Subpopulation *source_subpop = parent_haplosome->individual_->subpopulation_;
	IndividualSex parent_sex = parent_haplosome->individual_->sex_;

	// determine how many mutations and breakpoints we have
	int num_mutations = p_chromosome.DrawMutationCount(parent_sex);	// the parent sex is the same as the child sex
	
	// TREE SEQUENCE RECORDING
	// FIXME MULTICHROM separate critical region for each chromosome, too!
	bool recording_tree_sequence = f_treeseq;
	bool recording_tree_sequence_mutations = f_treeseq && species_.RecordingTreeSequenceMutations();
	
	if (recording_tree_sequence)
	{
#pragma omp critical (TreeSeqNewHaplosome)
		{
			species_.RecordNewHaplosome(nullptr, 0, &p_child_haplosome, parent_haplosome, nullptr);
		}
	}
	
	// mutations are usually rare, so let's streamline the case where none occur
	if (num_mutations == 0)
	{
		// no mutations, so the child haplosome is just a copy of the parental haplosome
		p_child_haplosome.copy_from_haplosome(*parent_haplosome);
	}
	else
	{
		// Generate all of the mutation positions as a separate stage, because we need to unique them.  See DrawSortedUniquedMutationPositions.
#if defined(__GNUC__) && !defined(__clang__)
		// Work around GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=27557
		static thread_local std::vector<std::pair<slim_position_t, GenomicElement *>> mut_positions;
#else
		static std::vector<std::pair<slim_position_t, GenomicElement *>> mut_positions;
#pragma omp threadprivate (mut_positions)
#endif
		
		mut_positions.resize(0);
		
		num_mutations = p_chromosome.DrawSortedUniquedMutationPositions(num_mutations, parent_sex, mut_positions);
		
		// Create vector with the mutations to be added
#if defined(__GNUC__) && !defined(__clang__)
		// Work around GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=27557
		static thread_local std::vector<MutationIndex> mutations_to_add;
#else
		static std::vector<MutationIndex> mutations_to_add;
#pragma omp threadprivate (mutations_to_add)
#endif
		
		mutations_to_add.resize(0);
		
#ifdef _OPENMP
		bool saw_error_in_critical = false;
#endif
		
#pragma omp critical (MutationAlloc)
		{
			try {
				if (species_.IsNucleotideBased() || (f_callbacks && p_mutation_callbacks))
				{
					// In nucleotide-based models, p_chromosome.DrawNewMutationExtended() will return new mutations to us with nucleotide_ set correctly.
					// To do that, and to adjust mutation rates correctly, it needs to know which parental haplosome the mutation occurred on the
					// background of, so that it can get the original nucleotide or trinucleotide context.  This code path is also used if mutation()
					// callbacks are enabled, since that also wants to be able to see the context of the mutation.
					for (int k = 0; k < num_mutations; k++)
					{
						MutationIndex new_mutation = p_chromosome.DrawNewMutationExtended(mut_positions[k], source_subpop->subpopulation_id_, community_.Tick(), parent_haplosome, nullptr, nullptr, 0, p_mutation_callbacks);
						
						if (new_mutation != -1)
							mutations_to_add.emplace_back(new_mutation);			// positions are already sorted
						
						// see further comments below, in the non-nucleotide case; they apply here as well
					}
				}
				else
				{
					// In non-nucleotide-based models, chromosome.DrawNewMutation() will return new mutations to us with nucleotide_ == -1
					for (int k = 0; k < num_mutations; k++)
					{
						MutationIndex new_mutation = p_chromosome.DrawNewMutation(mut_positions[k], source_subpop->subpopulation_id_, community_.Tick());	// the parent sex is the same as the child sex
						
						mutations_to_add.emplace_back(new_mutation);			// positions are already sorted
						
						// no need to worry about pure_neutral_ or all_pure_neutral_DFE_ here; the mutation is drawn from a registered genomic element type
						// we can't handle the stacking policy here, since we don't yet know what the context of the new mutation will be; we do it below
						// we add the new mutation to the registry below, if the stacking policy says the mutation can actually be added
					}
				}
			} catch (...) {
				// DrawNewMutation() / DrawNewMutationExtended() can raise, but it is (presumably) rare; we can leak mutations here
				// It occurs primarily with type 's' DFEs; an error in the user's script can cause a raise through here.
#ifdef _OPENMP
				saw_error_in_critical = true;		// can't throw from a critical region, even when not inside a parallel region!
#else
				throw;
#endif
			}
		}	// end #pragma omp critical (MutationAlloc)
		
#ifdef _OPENMP
		if (saw_error_in_critical)
		{
			// Note that the previous error message is still in gEidosTermination, so we just tack an addendum onto it and re-raise, in effect
			EIDOS_TERMINATION << "ERROR (Population::HaplosomeCloned): An exception was caught inside a critical region." << EidosTerminate();
		}
#endif
		
		// if there are no mutations, the child haplosome is just a copy of the parental haplosome
		// this can happen with nucleotide-based models because -1 can be returned by DrawNewMutationExtended()
		if (mutations_to_add.size() == 0)
		{
			p_child_haplosome.copy_from_haplosome(*parent_haplosome);
			return;
		}
		
		// loop over mutation runs and either (1) copy the mutrun pointer from the parent, or (2) make a new mutrun by modifying that of the parent
		Mutation *mut_block_ptr = gSLiM_Mutation_Block;
		
		int mutrun_count = p_child_haplosome.mutrun_count_;
		slim_position_t mutrun_length = p_child_haplosome.mutrun_length_;
		
		const MutationIndex *mutation_iter		= mutations_to_add.data();
		const MutationIndex *mutation_iter_max	= mutation_iter + mutations_to_add.size();
		MutationIndex mutation_iter_mutation_index = *mutation_iter;
		slim_position_t mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
		slim_mutrun_index_t mutation_iter_mutrun_index = (slim_mutrun_index_t)(mutation_iter_pos / mutrun_length);
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			if (mutation_iter_mutrun_index > run_index)
			{
				// no mutations in this run, so just copy the run pointer
				p_child_haplosome.mutruns_[run_index] = parent_haplosome->mutruns_[run_index];
			}
			else
			{
				// interleave the parental haplosome with the new mutations
				MutationRunContext &mutrun_context_LOCKED = p_chromosome.ChromosomeMutationRunContextForMutationRunIndex(run_index);
				MutationRun *child_run = p_child_haplosome.WillCreateRun_LOCKED(run_index, mutrun_context_LOCKED);
				const MutationRun *parent_run = parent_haplosome->mutruns_[run_index];
				const MutationIndex *parent_iter		= parent_run->begin_pointer_const();
				const MutationIndex *parent_iter_max	= parent_run->end_pointer_const();
				
				// while there is at least one new mutation left to place in this run... (which we know is true when we first reach here)
				do
				{
					// while an old mutation in the parent is before or at the next new mutation...
					while ((parent_iter != parent_iter_max) && ((mut_block_ptr + *parent_iter)->position_ <= mutation_iter_pos))
					{
						// we know the mutation is not already present, since mutations on the parent strand are already uniqued,
						// and new mutations are, by definition, new and thus cannot match the existing mutations
						child_run->emplace_back(*parent_iter);
						parent_iter++;
					}
					
					// while a new mutation in this run is before the next old mutation in the parent... (which we know is true when we first reach here)
					slim_position_t parent_iter_pos = (parent_iter == parent_iter_max) ? (SLIM_INF_BASE_POSITION) : (mut_block_ptr + *parent_iter)->position_;
					
					do
					{
						// we know the mutation is not already present, since mutations on the parent strand are already uniqued,
						// and new mutations are, by definition, new and thus cannot match the existing mutations
						Mutation *new_mut = mut_block_ptr + mutation_iter_mutation_index;
						MutationType *new_mut_type = new_mut->mutation_type_ptr_;
						
						if (child_run->enforce_stack_policy_for_addition(mutation_iter_pos, new_mut_type))
						{
							// The mutation was passed by the stacking policy, so we can add it to the child haplosome and the registry
							child_run->emplace_back(mutation_iter_mutation_index);
							
							if (new_mut->state_ != MutationState::kInRegistry)
							{
								#pragma omp critical (MutationRegistryAdd)
								{
									MutationRegistryAdd(new_mut);
								}
							}
							
							// TREE SEQUENCE RECORDING
							if (recording_tree_sequence_mutations)
							{
#pragma omp critical (TreeSeqNewDerivedState)
								{
									species_.RecordNewDerivedState(&p_child_haplosome, mutation_iter_pos, *child_run->derived_mutation_ids_at_position(mutation_iter_pos));
								}
							}
						}
						else if (new_mut->state_ == MutationState::kNewMutation)	// new and needs to be disposed of
						{
							// The mutation was rejected by the stacking policy, so we have to release it
#pragma omp critical (MutationAlloc)
							{
								new_mut->Release_PARALLEL();
							}
						}
						
						// move to the next mutation
						mutation_iter++;
						
						if (mutation_iter == mutation_iter_max)
						{
							mutation_iter_mutation_index = -1;
							mutation_iter_pos = SLIM_INF_BASE_POSITION;
						}
						else
						{
							mutation_iter_mutation_index = *mutation_iter;
							mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
						}
						
						mutation_iter_mutrun_index = (slim_mutrun_index_t)(mutation_iter_pos / mutrun_length);
						
						// if we're out of new mutations for this run, transfer down to the simpler loop below
						if (mutation_iter_mutrun_index != run_index)
							goto noNewMutationsLeft;
					}
					while (mutation_iter_pos < parent_iter_pos);
					
					// at this point we know we have a new mutation to place in this run, but it falls after the next parental mutation, so we loop back
				}
				while (true);
				
				// complete the mutation run after all new mutations within this run have been placed
			noNewMutationsLeft:
				while (parent_iter != parent_iter_max)
				{
					child_run->emplace_back(*parent_iter);
					parent_iter++;
				}
			}
		}
	}
}

template void Population::HaplosomeCloned<false, false>(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
template void Population::HaplosomeCloned<false, true>(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
template void Population::HaplosomeCloned<true, false>(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
template void Population::HaplosomeCloned<true, true>(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);

// generate a child haplosome from parental haplosomes, with recombination and mutation, and user-specified breakpoints
template <const bool f_treeseq, const bool f_callbacks>
void Population::HaplosomeRecombined(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<slim_position_t> &p_breakpoints, std::vector<SLiMEidosBlock*> *p_mutation_callbacks)
{
#if DEBUG
	// This method is designed to run in parallel, but only if no callbacks are enabled
	if (p_mutation_callbacks)
		THREAD_SAFETY_IN_ANY_PARALLEL("Population::HaplosomeRecombined(): recombination and mutation callbacks are not allowed when executing in parallel");
	
	if (p_breakpoints.size() == 0)
		EIDOS_TERMINATION << "ERROR (Population::HaplosomeRecombined): (internal error) Called with an empty breakpoint array." << EidosTerminate();
	
	if (!parent_haplosome_1 || !parent_haplosome_2)
		EIDOS_TERMINATION << "ERROR (Population::HaplosomeRecombined): (internal error) Null haplosome pointer." << EidosTerminate();
	
	if (!p_child_haplosome.individual_)
		EIDOS_TERMINATION << "ERROR (Population::HaplosomeRecombined): (internal error) individual_ pointer for child haplosome not set." << EidosTerminate();
	
	// BCH 9/20/2024: With the multichomosome redesign, the child and parent haplosome indices must always match; we are generating a
	// new offspring haplosome from parental haplosomes of the exact same chromosome (not just the same chomosome type).
	slim_chromosome_index_t chromosome_index = p_child_haplosome.chromosome_index_;
	slim_chromosome_index_t parent1_chromosome_index = parent_haplosome_1->chromosome_index_;
	slim_chromosome_index_t parent2_chromosome_index = parent_haplosome_2->chromosome_index_;
	
	if ((parent1_chromosome_index != chromosome_index) || (parent2_chromosome_index != chromosome_index))
		EIDOS_TERMINATION << "ERROR (Population::HaplosomeRecombined): (internal error) mismatch between parent and child chromosomes (child chromosome index == " << chromosome_index << ", parent 1 == " << parent1_chromosome_index << ", parent 2 == " << parent2_chromosome_index << ")." << EidosTerminate();
	
	if (p_child_haplosome.IsNull() || parent_haplosome_1->IsNull() || parent_haplosome_2->IsNull())
		EIDOS_TERMINATION << "ERROR (Population::HaplosomeRecombined): (internal error) null haplosomes cannot be passed to HaplosomeRecombined()." << EidosTerminate();
	
	Haplosome::DebugCheckStructureMatch(parent_haplosome_1, parent_haplosome_2, &p_child_haplosome, &p_chromosome);
#endif
#if SLIM_CLEAR_HAPLOSOMES
	// start with a clean slate in the child haplosome; we now expect child haplosomes to be cleared for us
	p_child_haplosome.check_cleared_to_nullptr();
#endif
	
	// for addRecombinant() and addMultiRecombinant(), we use the destination subpop as the mutation origin
	Subpopulation *dest_subpop = p_child_haplosome.individual_->subpopulation_;
	
	// The parent sex is used for mutation generation; we might have sex-specific mutation
	// rates.  Which parent to use to determine the sex-specific mutation rate is ambiguous.
	// At present, the caller guarantees that the two parents are of the same sex, if sex-
	// specific mutation rates are in use, so we can just use parent_haplosome_1's sex.
	IndividualSex parent_sex = parent_haplosome_1->individual_->sex_;
	
	// determine how many mutations we have
	int num_mutations = p_chromosome.DrawMutationCount(parent_sex);
	
	// we need a defined end breakpoint, so we add it now; we don't want more than one, though,
	// and in some cases the caller might already have this breakpoint added, so we need to
	// check (that happens with multiple children in addRecombinant(), for example)
	if ((p_breakpoints.size() == 0) || (p_breakpoints.back() <= p_chromosome.last_position_mutrun_))
		p_breakpoints.emplace_back(p_chromosome.last_position_mutrun_ + 10);
	
	// A leading zero in the breakpoints vector switches copy strands before copying begins.
	// We want to handle that up front, primarily because we don't want to record it in treeseq.
	// We only need to do this once, since the breakpoints vector is sorted/uniqued here.
	// For efficiency, we switch to a head pointer here; DO NOT USE p_breakpoints HEREAFTER!
	slim_position_t *breakpoints_ptr = p_breakpoints.data();
	int breakpoints_count = (int)p_breakpoints.size();
	
	if (*breakpoints_ptr == 0)	// guaranteed to exist since we added an element above
	{
		breakpoints_ptr++;
		breakpoints_count--;
		std::swap(parent_haplosome_1, parent_haplosome_2);
	}
	
	// TREE SEQUENCE RECORDING
	bool recording_tree_sequence = f_treeseq;
	bool recording_tree_sequence_mutations = f_treeseq && species_.RecordingTreeSequenceMutations();
	
	if (recording_tree_sequence)
	{
#pragma omp critical (TreeSeqNewHaplosome)
		{
			species_.RecordNewHaplosome(breakpoints_ptr, breakpoints_count, &p_child_haplosome, parent_haplosome_1, parent_haplosome_2);
		}
	}
	
	// mutations are usually rare, so let's streamline the case where none occur
	if (num_mutations == 0)
	{
		//
		// no mutations, but we do have crossovers, so we just need to interleave the two parental haplosomes
		//
		
		Mutation *mut_block_ptr = gSLiM_Mutation_Block;
		Haplosome *parent_haplosome = parent_haplosome_1;
		slim_position_t mutrun_length = p_child_haplosome.mutrun_length_;
		int mutrun_count = p_child_haplosome.mutrun_count_;
		int first_uncompleted_mutrun = 0;
		
		for (int break_index = 0; break_index < breakpoints_count; break_index++)
		{
			slim_position_t breakpoint = breakpoints_ptr[break_index];
			slim_mutrun_index_t break_mutrun_index = (slim_mutrun_index_t)(breakpoint / mutrun_length);
			
			// Copy over mutation runs until we arrive at the run in which the breakpoint occurs
			while (break_mutrun_index > first_uncompleted_mutrun)
			{
				p_child_haplosome.mutruns_[first_uncompleted_mutrun] = parent_haplosome->mutruns_[first_uncompleted_mutrun];
				++first_uncompleted_mutrun;
				
				if (first_uncompleted_mutrun >= mutrun_count)
					break;
			}
			
			// Now we are supposed to process a breakpoint in first_uncompleted_mutrun; check whether that means we're done
			if (first_uncompleted_mutrun >= mutrun_count)
				break;
			
			// The break occurs to the left of the base position of the breakpoint; check whether that is between runs
			if (breakpoint > break_mutrun_index * mutrun_length)
			{
				// The breakpoint occurs *inside* the run, so process the run by copying mutations and switching strands
				int this_mutrun_index = first_uncompleted_mutrun;
				const MutationIndex *parent1_iter		= parent_haplosome_1->mutruns_[this_mutrun_index]->begin_pointer_const();
				const MutationIndex *parent2_iter		= parent_haplosome_2->mutruns_[this_mutrun_index]->begin_pointer_const();
				const MutationIndex *parent1_iter_max	= parent_haplosome_1->mutruns_[this_mutrun_index]->end_pointer_const();
				const MutationIndex *parent2_iter_max	= parent_haplosome_2->mutruns_[this_mutrun_index]->end_pointer_const();
				const MutationIndex *parent_iter		= parent1_iter;
				const MutationIndex *parent_iter_max	= parent1_iter_max;
				MutationRunContext &mutrun_context_LOCKED = p_chromosome.ChromosomeMutationRunContextForMutationRunIndex(this_mutrun_index);
				MutationRun *child_mutrun = p_child_haplosome.WillCreateRun_LOCKED(this_mutrun_index, mutrun_context_LOCKED);
				
				while (true)
				{
					// while there are still old mutations in the parent before the current breakpoint...
					while (parent_iter != parent_iter_max)
					{
						MutationIndex current_mutation = *parent_iter;
						
						if ((mut_block_ptr + current_mutation)->position_ >= breakpoint)
							break;
						
						// add the old mutation; no need to check for a duplicate here since the parental haplosome is already duplicate-free
						child_mutrun->emplace_back(current_mutation);
						
						parent_iter++;
					}
					
					// we have reached the breakpoint, so swap parents; we want the "current strand" variables to change, so no std::swap()
					parent1_iter = parent2_iter;	parent1_iter_max = parent2_iter_max;	parent_haplosome_1 = parent_haplosome_2;
					parent2_iter = parent_iter;		parent2_iter_max = parent_iter_max;		parent_haplosome_2 = parent_haplosome;
					parent_iter = parent1_iter;		parent_iter_max = parent1_iter_max;		parent_haplosome = parent_haplosome_1;
					
					// skip over anything in the new parent that occurs prior to the breakpoint; it was not the active strand
					while (parent_iter != parent_iter_max && (mut_block_ptr + *parent_iter)->position_ < breakpoint)
						parent_iter++;
					
					// we have now handled the current breakpoint, so move on to the next breakpoint; advance the enclosing for loop here
					break_index++;
					
					// if we just handled the last breakpoint, which is guaranteed to be at or beyond lastPosition+1, then we are done
					if (break_index == breakpoints_count)
						break;
					
					// otherwise, figure out the new breakpoint, and continue looping on the current mutation run, which needs to be finished
					breakpoint = breakpoints_ptr[break_index];
					break_mutrun_index = (slim_mutrun_index_t)(breakpoint / mutrun_length);
					
					// if the next breakpoint is outside this mutation run, then finish the run and break out
					if (break_mutrun_index > this_mutrun_index)
					{
						while (parent_iter != parent_iter_max)
							child_mutrun->emplace_back(*(parent_iter++));
						
						break_index--;	// the outer loop will want to handle the current breakpoint again at the mutation-run level
						break;
					}
				}
				
				// We have completed this run
				++first_uncompleted_mutrun;
			}
			else
			{
				// The breakpoint occurs *between* runs, so just switch parent strands and the breakpoint is handled
				parent_haplosome_1 = parent_haplosome_2;
				parent_haplosome_2 = parent_haplosome;
				parent_haplosome = parent_haplosome_1;
			}
		}
	}
	else
	{
		// we have at least one new mutation, so set up for that case (which splits into two cases below)
		
		// Generate all of the mutation positions as a separate stage, because we need to unique them.  See DrawSortedUniquedMutationPositions.
#if defined(__GNUC__) && !defined(__clang__)
		// Work around GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=27557
		static thread_local std::vector<std::pair<slim_position_t, GenomicElement *>> mut_positions;
#else
		static std::vector<std::pair<slim_position_t, GenomicElement *>> mut_positions;
#pragma omp threadprivate (mut_positions)
#endif
		
		mut_positions.resize(0);
		
		num_mutations = p_chromosome.DrawSortedUniquedMutationPositions(num_mutations, parent_sex, mut_positions);
		
		// Create vector with the mutations to be added
#if defined(__GNUC__) && !defined(__clang__)
		// Work around GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=27557
		static thread_local std::vector<MutationIndex> mutations_to_add;
#else
		static std::vector<MutationIndex> mutations_to_add;
#pragma omp threadprivate (mutations_to_add)
#endif
		
		mutations_to_add.resize(0);
		
#ifdef _OPENMP
		bool saw_error_in_critical = false;
#endif
		
#pragma omp critical (MutationAlloc)
		{
			try {
				if (species_.IsNucleotideBased() || (f_callbacks && p_mutation_callbacks))
				{
					// In nucleotide-based models, p_chromosome.DrawNewMutationExtended() will return new mutations to us with nucleotide_ set correctly.
					// To do that, and to adjust mutation rates correctly, it needs to know which parental haplosome the mutation occurred on the
					// background of, so that it can get the original nucleotide or trinucleotide context.  This code path is also used if mutation()
					// callbacks are enabled, since that also wants to be able to see the context of the mutation.
					for (int k = 0; k < num_mutations; k++)
					{
						MutationIndex new_mutation = p_chromosome.DrawNewMutationExtended(mut_positions[k], dest_subpop->subpopulation_id_, community_.Tick(), parent_haplosome_1, parent_haplosome_2, breakpoints_ptr, breakpoints_count, p_mutation_callbacks);
						
						if (new_mutation != -1)
							mutations_to_add.emplace_back(new_mutation);			// positions are already sorted
						
						// see further comments below, in the non-nucleotide case; they apply here as well
					}
				}
				else
				{
					// In non-nucleotide-based models, p_chromosome.DrawNewMutation() will return new mutations to us with nucleotide_ == -1
					for (int k = 0; k < num_mutations; k++)
					{
						MutationIndex new_mutation = p_chromosome.DrawNewMutation(mut_positions[k], dest_subpop->subpopulation_id_, community_.Tick());
						
						mutations_to_add.emplace_back(new_mutation);			// positions are already sorted
						
						// no need to worry about pure_neutral_ or all_pure_neutral_DFE_ here; the mutation is drawn from a registered genomic element type
						// we can't handle the stacking policy here, since we don't yet know what the context of the new mutation will be; we do it below
						// we add the new mutation to the registry below, if the stacking policy says the mutation can actually be added
					}
				}
			} catch (...) {
				// DrawNewMutation() / DrawNewMutationExtended() can raise, but it is (presumably) rare; we can leak mutations here
				// It occurs primarily with type 's' DFEs; an error in the user's script can cause a raise through here.
#ifdef _OPENMP
				saw_error_in_critical = true;		// can't throw from a critical region, even when not inside a parallel region!
#else
				throw;
#endif
			}
		}	// end #pragma omp critical (MutationAlloc)
		
#ifdef _OPENMP
		if (saw_error_in_critical)
		{
			// Note that the previous error message is still in gEidosTermination, so we just tack an addendum onto it and re-raise, in effect
			EIDOS_TERMINATION << "ERROR (Population::HaplosomeRecombined): An exception was caught inside a critical region." << EidosTerminate();
		}
#endif
		
		Mutation *mut_block_ptr = gSLiM_Mutation_Block;
		const MutationIndex *mutation_iter		= mutations_to_add.data();
		const MutationIndex *mutation_iter_max	= mutation_iter + mutations_to_add.size();
		
		MutationIndex mutation_iter_mutation_index;
		slim_position_t mutation_iter_pos;
		
		if (mutation_iter != mutation_iter_max) {
			mutation_iter_mutation_index = *mutation_iter;
			mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
		} else {
			mutation_iter_mutation_index = -1;
			mutation_iter_pos = SLIM_INF_BASE_POSITION;
		}
		
		slim_position_t mutrun_length = p_child_haplosome.mutrun_length_;
		int mutrun_count = p_child_haplosome.mutrun_count_;
		slim_mutrun_index_t mutation_mutrun_index = (slim_mutrun_index_t)(mutation_iter_pos / mutrun_length);
		
		Haplosome *parent_haplosome = parent_haplosome_1;
		int first_uncompleted_mutrun = 0;
		
		//
		// mutations and crossovers; this is the most complex case
		//
		int break_index = 0;
		slim_position_t breakpoint = breakpoints_ptr[break_index];
		slim_mutrun_index_t break_mutrun_index = (slim_mutrun_index_t)(breakpoint / mutrun_length);
		
		while (true)	// loop over breakpoints until we have handled the last one, which comes at the end
		{
			if (mutation_mutrun_index < break_mutrun_index)
			{
				// Copy over mutation runs until we arrive at the run in which the mutation occurs
				while (mutation_mutrun_index > first_uncompleted_mutrun)
				{
					p_child_haplosome.mutruns_[first_uncompleted_mutrun] = parent_haplosome->mutruns_[first_uncompleted_mutrun];
					++first_uncompleted_mutrun;
					
					// We can't be done, since we have a mutation waiting to be placed, so we don't need to check
				}
				
				// Mutations can't occur between mutation runs the way breakpoints can, so we don't need to check that either
			}
			else
			{
				// Copy over mutation runs until we arrive at the run in which the breakpoint occurs
				while (break_mutrun_index > first_uncompleted_mutrun)
				{
					p_child_haplosome.mutruns_[first_uncompleted_mutrun] = parent_haplosome->mutruns_[first_uncompleted_mutrun];
					++first_uncompleted_mutrun;
					
					if (first_uncompleted_mutrun >= mutrun_count)
						break;
				}
				
				// Now we are supposed to process a breakpoint in first_uncompleted_mutrun; check whether that means we're done
				if (first_uncompleted_mutrun >= mutrun_count)
					break;
				
				// If the breakpoint occurs *between* runs, just switch parent strands and the breakpoint is handled
				if (breakpoint == break_mutrun_index * mutrun_length)
				{
					parent_haplosome_1 = parent_haplosome_2;
					parent_haplosome_2 = parent_haplosome;
					parent_haplosome = parent_haplosome_1;
					
					// go to next breakpoint; this advances the for loop
					if (++break_index == breakpoints_count)
						break;
					
					breakpoint = breakpoints_ptr[break_index];
					break_mutrun_index = (slim_mutrun_index_t)(breakpoint / mutrun_length);
					
					continue;
				}
			}
			
			// The event occurs *inside* the run, so process the run by copying mutations and switching strands
			int this_mutrun_index = first_uncompleted_mutrun;
			MutationRunContext &mutrun_context_LOCKED = p_chromosome.ChromosomeMutationRunContextForMutationRunIndex(this_mutrun_index);
			MutationRun *child_mutrun = p_child_haplosome.WillCreateRun_LOCKED(this_mutrun_index, mutrun_context_LOCKED);
			const MutationIndex *parent1_iter		= parent_haplosome_1->mutruns_[this_mutrun_index]->begin_pointer_const();
			const MutationIndex *parent1_iter_max	= parent_haplosome_1->mutruns_[this_mutrun_index]->end_pointer_const();
			const MutationIndex *parent_iter		= parent1_iter;
			const MutationIndex *parent_iter_max	= parent1_iter_max;
			
			if (break_mutrun_index == this_mutrun_index)
			{
				const MutationIndex *parent2_iter		= parent_haplosome_2->mutruns_[this_mutrun_index]->begin_pointer_const();
				const MutationIndex *parent2_iter_max	= parent_haplosome_2->mutruns_[this_mutrun_index]->end_pointer_const();
				
				if (mutation_mutrun_index == this_mutrun_index)
				{
					//
					// =====  this_mutrun_index has both breakpoint(s) and new mutation(s); this is the really nasty case
					//
					
					while (true)
					{
						// while there are still old mutations in the parent before the current breakpoint...
						while (parent_iter != parent_iter_max)
						{
							MutationIndex current_mutation = *parent_iter;
							slim_position_t current_mutation_pos = (mut_block_ptr + current_mutation)->position_;
							
							if (current_mutation_pos >= breakpoint)
								break;
							
							// add any new mutations that occur before the parental mutation; we know the parental mutation is in this run, so these are too
							while (mutation_iter_pos < current_mutation_pos)
							{
								Mutation *new_mut = mut_block_ptr + mutation_iter_mutation_index;
								MutationType *new_mut_type = new_mut->mutation_type_ptr_;
								
								if (child_mutrun->enforce_stack_policy_for_addition(new_mut->position_, new_mut_type))
								{
									// The mutation was passed by the stacking policy, so we can add it to the child haplosome and the registry
									child_mutrun->emplace_back(mutation_iter_mutation_index);
									
									if (new_mut->state_ != MutationState::kInRegistry)
									{
										#pragma omp critical (MutationRegistryAdd)
										{
											MutationRegistryAdd(new_mut);
										}
									}
									
									// TREE SEQUENCE RECORDING
									if (recording_tree_sequence_mutations)
									{
#pragma omp critical (TreeSeqNewDerivedState)
										{
											species_.RecordNewDerivedState(&p_child_haplosome, new_mut->position_, *child_mutrun->derived_mutation_ids_at_position(new_mut->position_));
										}
									}
								}
								else if (new_mut->state_ == MutationState::kNewMutation)	// new and needs to be disposed of
								{
									// The mutation was rejected by the stacking policy, so we have to release it
#pragma omp critical (MutationAlloc)
									{
										new_mut->Release_PARALLEL();
									}
								}
								
								if (++mutation_iter != mutation_iter_max) {
									mutation_iter_mutation_index = *mutation_iter;
									mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
								} else {
									mutation_iter_mutation_index = -1;
									mutation_iter_pos = SLIM_INF_BASE_POSITION;
								}
								
								mutation_mutrun_index = (slim_mutrun_index_t)(mutation_iter_pos / mutrun_length);
							}
							
							// add the old mutation; no need to check for a duplicate here since the parental haplosome is already duplicate-free
							child_mutrun->emplace_back(current_mutation);
							
							parent_iter++;
						}
						
						// add any new mutations that occur before the breakpoint; for these we have to check that they fall within this mutation run
						while ((mutation_iter_pos < breakpoint) && (mutation_mutrun_index == this_mutrun_index))
						{
							Mutation *new_mut = mut_block_ptr + mutation_iter_mutation_index;
							MutationType *new_mut_type = new_mut->mutation_type_ptr_;
							
							if (child_mutrun->enforce_stack_policy_for_addition(new_mut->position_, new_mut_type))
							{
								// The mutation was passed by the stacking policy, so we can add it to the child haplosome and the registry
								child_mutrun->emplace_back(mutation_iter_mutation_index);
								
								if (new_mut->state_ != MutationState::kInRegistry)
								{
									#pragma omp critical (MutationRegistryAdd)
									{
										MutationRegistryAdd(new_mut);
									}
								}
								
								// TREE SEQUENCE RECORDING
								if (recording_tree_sequence_mutations)
								{
#pragma omp critical (TreeSeqNewDerivedState)
									{
										species_.RecordNewDerivedState(&p_child_haplosome, new_mut->position_, *child_mutrun->derived_mutation_ids_at_position(new_mut->position_));
									}
								}
							}
							else if (new_mut->state_ == MutationState::kNewMutation)	// new and needs to be disposed of
							{
								// The mutation was rejected by the stacking policy, so we have to release it
#pragma omp critical (MutationAlloc)
								{
									new_mut->Release_PARALLEL();
								}
							}
							
							if (++mutation_iter != mutation_iter_max) {
								mutation_iter_mutation_index = *mutation_iter;
								mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
							} else {
								mutation_iter_mutation_index = -1;
								mutation_iter_pos = SLIM_INF_BASE_POSITION;
							}
							
							mutation_mutrun_index = (slim_mutrun_index_t)(mutation_iter_pos / mutrun_length);
						}
						
						// we have finished the parental mutation run; if the breakpoint we are now working toward lies beyond the end of the
						// current mutation run, then we have completed this run and can exit to the outer loop which will handle the rest
						if (break_mutrun_index > this_mutrun_index)
							break;		// the outer loop will want to handle this breakpoint again at the mutation-run level
						
						// we have reached the breakpoint, so swap parents; we want the "current strand" variables to change, so no std::swap()
						parent1_iter = parent2_iter;	parent1_iter_max = parent2_iter_max;	parent_haplosome_1 = parent_haplosome_2;
						parent2_iter = parent_iter;		parent2_iter_max = parent_iter_max;		parent_haplosome_2 = parent_haplosome;
						parent_iter = parent1_iter;		parent_iter_max = parent1_iter_max;		parent_haplosome = parent_haplosome_1;
						
						// skip over anything in the new parent that occurs prior to the breakpoint; it was not the active strand
						while (parent_iter != parent_iter_max && (mut_block_ptr + *parent_iter)->position_ < breakpoint)
							parent_iter++;
						
						// we have now handled the current breakpoint, so move on; if we just handled the last breakpoint, then we are done
						if (++break_index == breakpoints_count)
							break;
						
						// otherwise, figure out the new breakpoint, and continue looping on the current mutation run, which needs to be finished
						breakpoint = breakpoints_ptr[break_index];
						break_mutrun_index = (slim_mutrun_index_t)(breakpoint / mutrun_length);
					}
					
					// if we just handled the last breakpoint, which is guaranteed to be at or beyond lastPosition+1, then we are done
					if (break_index == breakpoints_count)
						break;
					
					// We have completed this run
					++first_uncompleted_mutrun;
				}
				else
				{
					//
					// =====  this_mutrun_index has only breakpoint(s), no new mutations
					//
					
					while (true)
					{
						// while there are still old mutations in the parent before the current breakpoint...
						while (parent_iter != parent_iter_max)
						{
							MutationIndex current_mutation = *parent_iter;
							
							if ((mut_block_ptr + current_mutation)->position_ >= breakpoint)
								break;
							
							// add the old mutation; no need to check for a duplicate here since the parental haplosome is already duplicate-free
							child_mutrun->emplace_back(current_mutation);
							
							parent_iter++;
						}
						
						// we have reached the breakpoint, so swap parents; we want the "current strand" variables to change, so no std::swap()
						parent1_iter = parent2_iter;	parent1_iter_max = parent2_iter_max;	parent_haplosome_1 = parent_haplosome_2;
						parent2_iter = parent_iter;		parent2_iter_max = parent_iter_max;		parent_haplosome_2 = parent_haplosome;
						parent_iter = parent1_iter;		parent_iter_max = parent1_iter_max;		parent_haplosome = parent_haplosome_1;
						
						// skip over anything in the new parent that occurs prior to the breakpoint; it was not the active strand
						while (parent_iter != parent_iter_max && (mut_block_ptr + *parent_iter)->position_ < breakpoint)
							parent_iter++;
						
						// we have now handled the current breakpoint, so move on; if we just handled the last breakpoint, then we are done
						if (++break_index == breakpoints_count)
							break;
						
						// otherwise, figure out the new breakpoint, and continue looping on the current mutation run, which needs to be finished
						breakpoint = breakpoints_ptr[break_index];
						break_mutrun_index = (slim_mutrun_index_t)(breakpoint / mutrun_length);
						
						// if the next breakpoint is outside this mutation run, then finish the run and break out
						if (break_mutrun_index > this_mutrun_index)
						{
							while (parent_iter != parent_iter_max)
								child_mutrun->emplace_back(*(parent_iter++));
							
							break;	// the outer loop will want to handle this breakpoint again at the mutation-run level
						}
					}
					
					// if we just handled the last breakpoint, which is guaranteed to be at or beyond lastPosition+1, then we are done
					if (break_index == breakpoints_count)
						break;
					
					// We have completed this run
					++first_uncompleted_mutrun;
				}
			}
			else if (mutation_mutrun_index == this_mutrun_index)
			{
				//
				// =====  this_mutrun_index has only new mutation(s), no breakpoints
				//
				
				// add any additional new mutations that occur before the end of the mutation run; there is at least one
				do
				{
					// add any parental mutations that occur before or at the next new mutation's position
					while (parent_iter != parent_iter_max)
					{
						MutationIndex current_mutation = *parent_iter;
						slim_position_t current_mutation_pos = (mut_block_ptr + current_mutation)->position_;
						
						if (current_mutation_pos > mutation_iter_pos)
							break;
						
						child_mutrun->emplace_back(current_mutation);
						parent_iter++;
					}
					
					// add the new mutation, which might overlap with the last added old mutation
					Mutation *new_mut = mut_block_ptr + mutation_iter_mutation_index;
					MutationType *new_mut_type = new_mut->mutation_type_ptr_;
					
					if (child_mutrun->enforce_stack_policy_for_addition(new_mut->position_, new_mut_type))
					{
						// The mutation was passed by the stacking policy, so we can add it to the child haplosome and the registry
						child_mutrun->emplace_back(mutation_iter_mutation_index);
						
						if (new_mut->state_ != MutationState::kInRegistry)
						{
							#pragma omp critical (MutationRegistryAdd)
							{
								MutationRegistryAdd(new_mut);
							}
						}
						
						// TREE SEQUENCE RECORDING
						if (recording_tree_sequence_mutations)
						{
#pragma omp critical (TreeSeqNewDerivedState)
							{
								species_.RecordNewDerivedState(&p_child_haplosome, new_mut->position_, *child_mutrun->derived_mutation_ids_at_position(new_mut->position_));
							}
						}
					}
					else if (new_mut->state_ == MutationState::kNewMutation)	// new and needs to be disposed of
					{
						// The mutation was rejected by the stacking policy, so we have to release it
#pragma omp critical (MutationAlloc)
						{
							new_mut->Release_PARALLEL();
						}
					}
					
					if (++mutation_iter != mutation_iter_max) {
						mutation_iter_mutation_index = *mutation_iter;
						mutation_iter_pos = (mut_block_ptr + mutation_iter_mutation_index)->position_;
					} else {
						mutation_iter_mutation_index = -1;
						mutation_iter_pos = SLIM_INF_BASE_POSITION;
					}
					
					mutation_mutrun_index = (slim_mutrun_index_t)(mutation_iter_pos / mutrun_length);
				}
				while (mutation_mutrun_index == this_mutrun_index);
				
				// finish up any parental mutations that come after the last new mutation in the mutation run
				while (parent_iter != parent_iter_max)
					child_mutrun->emplace_back(*(parent_iter++));
				
				// We have completed this run
				++first_uncompleted_mutrun;
			}
			else
			{
				EIDOS_TERMINATION << "ERROR (Population::HaplosomeRecombined): (internal error) logic fail." << EidosTerminate();
			}
		}
	}
	
	// debugging check
#if 0
	for (int i = 0; i < child_haplosome.mutrun_count_; ++i)
		if (child_haplosome.mutruns_[i].get() == nullptr)
			EIDOS_TERMINATION << "ERROR (Population::HaplosomeRecombined): (internal error) null mutation run left at end of recombination-mutation." << EidosTerminate();
#endif
}

template void Population::HaplosomeRecombined<false, false>(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<slim_position_t> &p_breakpoints, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
template void Population::HaplosomeRecombined<false, true>(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<slim_position_t> &p_breakpoints, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
template void Population::HaplosomeRecombined<true, false>(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<slim_position_t> &p_breakpoints, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);
template void Population::HaplosomeRecombined<true, true>(Chromosome &p_chromosome, Haplosome &p_child_haplosome, Haplosome *parent_haplosome_1, Haplosome *parent_haplosome_2, std::vector<slim_position_t> &p_breakpoints, std::vector<SLiMEidosBlock*> *p_mutation_callbacks);

void Population::DoHeteroduplexRepair(std::vector<slim_position_t> &p_heteroduplex, slim_position_t *p_breakpoints, int p_breakpoints_count, Haplosome *p_parent_haplosome_1, Haplosome *p_parent_haplosome_2, Haplosome *p_child_haplosome)
{
#if DEBUG
	if (!p_child_haplosome->individual_)
		EIDOS_TERMINATION << "ERROR (Population::DoHeteroduplexRepair): (internal error) The child haplosome must have an owning individual." << EidosTerminate();
	if ((p_parent_haplosome_1->chromosome_index_ != p_parent_haplosome_2->chromosome_index_) ||
		(p_parent_haplosome_1->chromosome_index_ != p_child_haplosome->chromosome_index_))
		EIDOS_TERMINATION << "ERROR (Population::DoHeteroduplexRepair): (internal error) The child haplosome and parent haplosomes must all have the same associated chromosome." << EidosTerminate();
#endif
	
	// Heteroduplex mismatch repair handling: heteroduplex contains a set of start/end position
	// pairs, representing stretches of the offspring haplosome that result from "complex" gene
	// conversion tracts where the two homologous parental strands ended up paired, even though
	// their sequences do not necessarily match.  The code above designated one parental strand
	// as the ancestral strand, for purposes of generating the offspring haplosome and recording
	// ancestry with tree-sequence recording.  We now need to handle mismatch repair.  For each
	// heteroduplex stretch, we want to (1) determine which parental strand was considered to be
	// ancestral, (2) walk through the offspring haplosome and the non-ancestral strand looking for
	// mismatches (mutations in one that are not in the other), and (3) repair the mismatch
	// with equal probability of choosing either strand, unless we're in a nucleotide model and
	// biased gene conversion is enabled, in which case the choice will be influenced by the
	// GC bias.  In all cases, if the mismatch involves a newly introduced mutation we will
	// treat it identically to other mutations; mutations happen before heteroduplex repair,
	// although after gene conversion tracts get copied from the non-copy strand, and so they
	// can be reversed by heteroduplex repair.  This seems like the only clear/consistent policy
	// since new mutations could be stacked with other pre-existing mutations that should be
	// subject to heteroduplex repair.  Anyway this is such an edge case that our chosen policy
	// on it shouldn't matter for practical purposes.
	Chromosome *chromosome = p_child_haplosome->AssociatedChromosome();
	double gBGC_coeff_scaled = (chromosome->mismatch_repair_bias_ + 1.0) / 2.0;
	bool repairs_biased = (species_.IsNucleotideBased() && (gBGC_coeff_scaled != 0.5));
	NucleotideArray *ancestral_sequence = (repairs_biased ? chromosome->AncestralSequence() : nullptr);
	int heteroduplex_tract_count = (int)(p_heteroduplex.size() / 2);
	
	if (heteroduplex_tract_count * 2 != (int)p_heteroduplex.size())
		EIDOS_TERMINATION << "ERROR (Population::DoHeteroduplexRepair): (internal error) The heteroduplex tract vector has an odd length." << EidosTerminate();
	
	// We accumulate vectors of all mutations to add to and to remove from the offspring haplosome,
	// and do all addition/removal in a single pass at the end of the process
	std::vector<slim_position_t> repair_removals;
	std::vector<Mutation*> repair_additions;
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	Eidos_RNG_State *rng_state = EIDOS_STATE_RNG(omp_get_thread_num());
	
	for (int heteroduplex_tract_index = 0; heteroduplex_tract_index < heteroduplex_tract_count; ++heteroduplex_tract_index)
	{
		slim_position_t tract_start = p_heteroduplex[(size_t)heteroduplex_tract_index * 2];
		slim_position_t tract_end = p_heteroduplex[(size_t)heteroduplex_tract_index * 2 + 1];
		
		// Determine which parental strand was the non-copy strand in this region, by scanning
		// the breakpoints vector; it must remain the non-copy strand throughout.
		bool copy_strand_is_1 = true;
		
		for (int break_index = 0; break_index < p_breakpoints_count; ++break_index)
		{
			slim_position_t breakpoint = p_breakpoints[break_index];
			
			if (breakpoint <= tract_start)
				copy_strand_is_1 = !copy_strand_is_1;
			else if (breakpoint > tract_end)
				break;
			else
				EIDOS_TERMINATION << "ERROR (Population::DoHeteroduplexRepair): (internal error) The heteroduplex tract does not have a consistent copy strand." << EidosTerminate();
		}
		
		Haplosome *noncopy_haplosome = (copy_strand_is_1 ? p_parent_haplosome_2 : p_parent_haplosome_1);
		
		// Make haplosome walkers for the non-copy strand and the offspring strand, and move them
		// to the start of the heteroduplex tract region; we use SLIM_INF_BASE_POSITION to mean
		// "past the end of the heteroduplex tract" here
		HaplosomeWalker noncopy_walker(noncopy_haplosome);
		HaplosomeWalker offspring_walker(p_child_haplosome);
		slim_position_t noncopy_pos, offspring_pos;
		
		noncopy_walker.MoveToPosition(tract_start);
		offspring_walker.MoveToPosition(tract_start);
		
		if (noncopy_walker.Finished()) noncopy_pos = SLIM_INF_BASE_POSITION;
		else {
			noncopy_pos = noncopy_walker.Position();
			if (noncopy_pos > tract_end)
				noncopy_pos = SLIM_INF_BASE_POSITION;
		}
		if (offspring_walker.Finished()) offspring_pos = SLIM_INF_BASE_POSITION;
		else {
			offspring_pos = offspring_walker.Position();
			if (offspring_pos > tract_end)
				offspring_pos = SLIM_INF_BASE_POSITION;
		}
		
		// Move the walkers forward in sync, looking for mismatches until both strands are done
		while ((offspring_pos != SLIM_INF_BASE_POSITION) || (noncopy_pos != SLIM_INF_BASE_POSITION))
		{
			bool repair_toward_noncopy, advance_noncopy, advance_offspring;
			slim_position_t repair_pos;
			
			if (noncopy_pos < offspring_pos)
			{
				// A mutation exists on the non-copy strand where the offspring strand is empty;
				// this is a mismatch that needs to be repaired one way or the other
				if (repairs_biased)
				{
					int noncopy_nuc = noncopy_walker.NucleotideAtCurrentPosition();		// NOLINT(*-signed-char-misuse) : intentional
					
					// The offspring nucleotide is ancestral; if the noncopy nuc is too, GC bias is irrelevant
					if (noncopy_nuc != -1)
					{
						int offspring_nuc = ancestral_sequence->NucleotideAtIndex(noncopy_pos);
						bool noncopy_nuc_AT = ((noncopy_nuc == 0) || (noncopy_nuc == 3));
						bool offspring_nuc_AT = ((offspring_nuc == 0) || (offspring_nuc == 3));
						
						if (noncopy_nuc_AT != offspring_nuc_AT)
						{
							// One nucleotide is A/T, the other is G/C, so GC bias is relevant here;
							// make a determination based on the assumption that the noncopy nucleotide is G/C
							repair_toward_noncopy = (Eidos_rng_uniform(rng) <= gBGC_coeff_scaled);	// 1.0 means always repair toward GC
							
							// If the noncopy nucleotide is the A/T one, then our determination needs to be flipped
							if (noncopy_nuc_AT)
								repair_toward_noncopy = !repair_toward_noncopy;
							
							goto biasedRepair1;
						}
					}
				}
				
				// The default is unbiased repair; if we are biased we goto over this default
				repair_toward_noncopy = Eidos_RandomBool(rng_state);
				
			biasedRepair1:
				advance_noncopy = true;
				advance_offspring = false;
				repair_pos = noncopy_pos;
			}
			else if (offspring_pos < noncopy_pos)
			{
				// A mutation exists on the offspring strand where the non-copy strand is empty;
				// this is a mismatch that needs to be repaired one way or the other
				if (repairs_biased)
				{
					int offspring_nuc = offspring_walker.NucleotideAtCurrentPosition();		// NOLINT(*-signed-char-misuse) : intentional
					
					// The noncopy nucleotide is ancestral; if the offspring nuc is too, GC bias is irrelevant
					if (offspring_nuc != -1)
					{
						int noncopy_nuc = ancestral_sequence->NucleotideAtIndex(offspring_pos);
						bool noncopy_nuc_AT = ((noncopy_nuc == 0) || (noncopy_nuc == 3));
						bool offspring_nuc_AT = ((offspring_nuc == 0) || (offspring_nuc == 3));
						
						if (noncopy_nuc_AT != offspring_nuc_AT)
						{
							// One nucleotide is A/T, the other is G/C, so GC bias is relevant here;
							// make a determination based on the assumption that the noncopy nucleotide is G/C
							repair_toward_noncopy = (Eidos_rng_uniform(rng) <= gBGC_coeff_scaled);	// 1.0 means always repair toward GC
							
							// If the noncopy nucleotide is the A/T one, then our determination needs to be flipped
							if (noncopy_nuc_AT)
								repair_toward_noncopy = !repair_toward_noncopy;
							
							goto biasedRepair2;
						}
					}
				}
				
				// The default is unbiased repair; if we are biased we goto over this default
				repair_toward_noncopy = Eidos_RandomBool(rng_state);
				
			biasedRepair2:
				advance_noncopy = false;
				advance_offspring = true;
				repair_pos = offspring_pos;
			}
			else if (offspring_walker.IdenticalAtCurrentPositionTo(noncopy_walker))
			{
				// The two walkers have identical state at this position, so there is no mismatch.
				// We consider identical stacks of mutations that are re-ordered with respect to
				// each other to be a mismatch, for simplicity; such re-ordered stacks should not
				// occur anyway unless the user is doing something really weird.
				repair_toward_noncopy = false;
				advance_noncopy = true;
				advance_offspring = true;
				repair_pos = offspring_pos;
			}
			else
			{
				// The walkers are at the same position, and there is a mismatch.
				if (repairs_biased)
				{
					int noncopy_nuc = noncopy_walker.NucleotideAtCurrentPosition();			// NOLINT(*-signed-char-misuse) : intentional
					int offspring_nuc = offspring_walker.NucleotideAtCurrentPosition();		// NOLINT(*-signed-char-misuse) : intentional
					
					// If both nucleotides are ancestral, GC bias is irrelevant
					if ((noncopy_nuc != -1) || (offspring_nuc != -1))
					{
						if (noncopy_nuc == -1) noncopy_nuc = ancestral_sequence->NucleotideAtIndex(offspring_pos);
						if (offspring_nuc == -1) offspring_nuc = ancestral_sequence->NucleotideAtIndex(offspring_pos);
						
						bool noncopy_nuc_AT = ((noncopy_nuc == 0) || (noncopy_nuc == 3));
						bool offspring_nuc_AT = ((offspring_nuc == 0) || (offspring_nuc == 3));
						
						if (noncopy_nuc_AT != offspring_nuc_AT)
						{
							// One nucleotide is A/T, the other is G/C, so GC bias is relevant here;
							// make a determination based on the assumption that the noncopy nucleotide is G/C
							repair_toward_noncopy = (Eidos_rng_uniform(rng) <= gBGC_coeff_scaled);	// 1.0 means always repair toward GC
							
							// If the noncopy nucleotide is the A/T one, then our determination needs to be flipped
							if (noncopy_nuc_AT)
								repair_toward_noncopy = !repair_toward_noncopy;
							
							goto biasedRepair3;
						}
					}
				}
				
				// The default is unbiased repair; if we are biased we goto over this default
				repair_toward_noncopy = Eidos_RandomBool(rng_state);
				
			biasedRepair3:
				advance_noncopy = true;
				advance_offspring = true;
				repair_pos = offspring_pos;
			}
			
			// Move past the mismatch, marking mutations for copying if repair is toward the noncopy strand
			if (advance_noncopy)
			{
				while (true)
				{
					if (repair_toward_noncopy)
						repair_additions.emplace_back(noncopy_walker.CurrentMutation());
					
					noncopy_walker.NextMutation();
					if (noncopy_walker.Finished())
					{
						noncopy_pos = SLIM_INF_BASE_POSITION;
						break;
					}
					noncopy_pos = noncopy_walker.Position();
					if (noncopy_pos > repair_pos)
					{
						if (noncopy_pos > tract_end)
							noncopy_pos = SLIM_INF_BASE_POSITION;
						break;
					}
				}
			}
			if (advance_offspring)
			{
				if (repair_toward_noncopy)
					repair_removals.emplace_back(repair_pos);
				
				while (true)
				{
					offspring_walker.NextMutation();
					if (offspring_walker.Finished())
					{
						offspring_pos = SLIM_INF_BASE_POSITION;
						break;
					}
					offspring_pos = offspring_walker.Position();
					if (offspring_pos > repair_pos)
					{
						if (offspring_pos > tract_end)
							offspring_pos = SLIM_INF_BASE_POSITION;
						break;
					}
				}
			}
		}
	}
	
	// We are done scanning; now we do all of the planned repairs.  Tree-sequence recording
	// needs to be kept apprised of all the changes made.  Note that in some cases a mutation
	// might have been newly added at a position, and then removed again by mismatch repair;
	// we will need to make sure that the recorded state is correct when that occurs.
	
	if ((repair_removals.size() > 0) || (repair_additions.size() > 0))
	{
		// We loop through the mutation runs in p_child_haplosome, and for each one, if there are
		// mutations to be added or removed we make a new mutation run and effect the changes
		// as we copy mutations over.  Mutruns without changes are left untouched.
		Mutation *mut_block_ptr = gSLiM_Mutation_Block;
		slim_position_t mutrun_length = p_child_haplosome->mutrun_length_;
		slim_position_t mutrun_count = p_child_haplosome->mutrun_count_;
		std::size_t removal_index = 0, addition_index = 0;
		slim_position_t next_removal_pos = (removal_index < repair_removals.size()) ? repair_removals[removal_index] : SLIM_INF_BASE_POSITION;
		slim_position_t next_addition_pos = (addition_index < repair_additions.size()) ? repair_additions[addition_index]->position_ : SLIM_INF_BASE_POSITION;
		slim_mutrun_index_t next_removal_mutrun_index = (slim_mutrun_index_t)(next_removal_pos / mutrun_length);
		slim_mutrun_index_t next_addition_mutrun_index = (slim_mutrun_index_t)(next_addition_pos / mutrun_length);
		slim_mutrun_index_t run_index = std::min(next_removal_mutrun_index, next_addition_mutrun_index);
		
		while (run_index < mutrun_count)
		{
			// Now we will process *all* additions and removals for run_index
			MutationRunContext &mutrun_context_LOCKED = chromosome->ChromosomeMutationRunContextForMutationRunIndex(run_index);
			MutationRun *new_run = MutationRun::NewMutationRun_LOCKED(mutrun_context_LOCKED);
			const MutationRun *old_run = p_child_haplosome->mutruns_[run_index];
			const MutationIndex *old_run_iter		= old_run->begin_pointer_const();
			const MutationIndex *old_run_iter_max	= old_run->end_pointer_const();
			
			for ( ; old_run_iter != old_run_iter_max; ++old_run_iter)
			{
				MutationIndex old_run_mut_index = *old_run_iter;
				Mutation *old_run_mut = mut_block_ptr + old_run_mut_index;
				slim_position_t old_run_mut_pos = old_run_mut->position_;
				
				// if we are past the current removal position, advance to the next, which should be at or after our position
				if (old_run_mut_pos > next_removal_pos)
				{
					removal_index++;
					next_removal_pos = (removal_index < repair_removals.size()) ? repair_removals[removal_index] : SLIM_INF_BASE_POSITION;
				}
				
				// if the current position is being removed, skip over this mutation; we may not be done with this removal position, however
				if (old_run_mut_pos == next_removal_pos)
					continue;
				
				// we will add the current mutation, but first we must add any addition mutations that come before it
				while (next_addition_pos < old_run_mut_pos)
				{
					Mutation *addition_mut = repair_additions[addition_index];
					MutationIndex addition_mut_index = (MutationIndex)(addition_mut - mut_block_ptr); // BlockIndex()
					
					new_run->emplace_back(addition_mut_index);
					
					addition_index++;
					next_addition_pos = (addition_index < repair_additions.size()) ? repair_additions[addition_index]->position_ : SLIM_INF_BASE_POSITION;
				}
				
				// add the current mutation, since it is not being removed
				new_run->emplace_back(old_run_mut_index);
			}
			
			// update the mutrun indexes; we don't do this above to avoid lots of redundant division
			next_removal_mutrun_index = (slim_mutrun_index_t)(next_removal_pos / mutrun_length);
			next_addition_mutrun_index = (slim_mutrun_index_t)(next_addition_pos / mutrun_length);
			
			// if there are any removal positions left in this mutrun, they have been handled above
			while (next_removal_mutrun_index == run_index)
			{
				removal_index++;
				next_removal_pos = (removal_index < repair_removals.size()) ? repair_removals[removal_index] : SLIM_INF_BASE_POSITION;
				next_removal_mutrun_index = (slim_mutrun_index_t)(next_removal_pos / mutrun_length);
			}
			
			// if there are addition mutations left in this mutrun, they must go after the end of the old mutrun's mutations
			while (next_addition_mutrun_index == run_index)
			{
				Mutation *addition_mut = repair_additions[addition_index];
				MutationIndex addition_mut_index = (MutationIndex)(addition_mut - mut_block_ptr); // BlockIndex()
				
				new_run->emplace_back(addition_mut_index);
				
				addition_index++;
				next_addition_pos = (addition_index < repair_additions.size()) ? repair_additions[addition_index]->position_ : SLIM_INF_BASE_POSITION;
				next_addition_mutrun_index = (slim_mutrun_index_t)(next_addition_pos / mutrun_length);
			}
			
			// replace the mutation run at run_index with the newly constructed run that has all additions and removals
			p_child_haplosome->mutruns_[run_index] = new_run;
			
			// go to the next run index that has changes
			run_index = std::min(next_removal_mutrun_index, next_addition_mutrun_index);
		}
	}
	
	// TREE SEQUENCE RECORDING
	if (species_.RecordingTreeSequenceMutations())
	{
		// We repurpose repair_removals here as a vector of all positions that changed due to heteroduplex repair.
		// We therefore add in the positions for each entry in repair_additions, then sort and unique.
		for (Mutation *added_mut : repair_additions)
			repair_removals.emplace_back(added_mut->position_);
		
		std::sort(repair_removals.begin(), repair_removals.end());
		repair_removals.erase(unique(repair_removals.begin(), repair_removals.end()), repair_removals.end());
		
		// Then we record the new derived state at every position that changed
		for (slim_position_t changed_pos : repair_removals)
		{
#pragma omp critical (TreeSeqNewDerivedState)
			{
				species_.RecordNewDerivedState(p_child_haplosome, changed_pos, *p_child_haplosome->derived_mutation_ids_at_position(changed_pos));
			}
		}
	}
}

#ifdef SLIMGUI
void Population::RecordFitness(slim_tick_t p_history_index, slim_objectid_t p_subpop_id, double p_fitness_value)
{
	FitnessHistory *history_rec_ptr = nullptr;
	
	// Find the existing history record, if it exists
	auto history_iter = fitness_histories_.find(p_subpop_id);
	
	if (history_iter != fitness_histories_.end())
		history_rec_ptr = &(history_iter->second);
	
	// If not, create a new history record and add it to our vector
	if (!history_rec_ptr)
	{
		FitnessHistory history_record;
		
		history_record.history_ = nullptr;
		history_record.history_length_ = 0;
		
		auto emplace_rec = fitness_histories_.emplace(p_subpop_id, history_record);
		
		if (emplace_rec.second)
			history_rec_ptr = &(emplace_rec.first->second);
	}
	
	// Assuming we now have a record, resize it as needed and insert the new value
	if (history_rec_ptr)
	{
		double *history = history_rec_ptr->history_;
		slim_tick_t history_length = history_rec_ptr->history_length_;
		
		if (p_history_index >= history_length)
		{
			slim_tick_t oldHistoryLength = history_length;
			
			history_length = p_history_index + 1000;			// give some elbow room for expansion
			history = (double *)realloc(history, history_length * sizeof(double));
			
			for (slim_tick_t i = oldHistoryLength; i < history_length; ++i)
				history[i] = NAN;
			
			// Copy the new values back into the history record
			history_rec_ptr->history_ = history;
			history_rec_ptr->history_length_ = history_length;
		}
		
		history[p_history_index] = p_fitness_value;
	}
}

void Population::RecordSubpopSize(slim_tick_t p_history_index, slim_objectid_t p_subpop_id, slim_popsize_t p_subpop_size)
{
    SubpopSizeHistory *history_rec_ptr = nullptr;
	
	// Find the existing history record, if it exists
	auto history_iter = subpop_size_histories_.find(p_subpop_id);
	
	if (history_iter != subpop_size_histories_.end())
		history_rec_ptr = &(history_iter->second);
	
	// If not, create a new history record and add it to our vector
	if (!history_rec_ptr)
	{
		SubpopSizeHistory history_record;
		
		history_record.history_ = nullptr;
		history_record.history_length_ = 0;
		
		auto emplace_rec = subpop_size_histories_.emplace(p_subpop_id, history_record);
		
		if (emplace_rec.second)
			history_rec_ptr = &(emplace_rec.first->second);
	}
	
	// Assuming we now have a record, resize it as needed and insert the new value
	if (history_rec_ptr)
	{
		slim_popsize_t *history = history_rec_ptr->history_;
		slim_tick_t history_length = history_rec_ptr->history_length_;
		
		if (p_history_index >= history_length)
		{
			slim_tick_t oldHistoryLength = history_length;
			
			history_length = p_history_index + 1000;			// give some elbow room for expansion
			history = (slim_popsize_t *)realloc(history, history_length * sizeof(slim_popsize_t));
			
			for (slim_tick_t i = oldHistoryLength; i < history_length; ++i)
				history[i] = 0;
			
			// Copy the new values back into the history record
			history_rec_ptr->history_ = history;
			history_rec_ptr->history_length_ = history_length;
		}
		
		history[p_history_index] = p_subpop_size;
	}
}

// This method is used to record population statistics that are kept per tick for SLiMgui
#if defined(__clang__)
__attribute__((no_sanitize("float-divide-by-zero")))
__attribute__((no_sanitize("integer-divide-by-zero")))
#endif
void Population::SurveyPopulation(void)
{
	// Calculate mean fitness for this tick
	double totalUnscaledFitness = 0.0;
	slim_popsize_t totalPopSize = 0;
	slim_tick_t historyIndex = community_.Tick() - 1;	// zero-base: the first tick we put something in is tick 1, and we put it at index 0
	
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
	{ 
		Subpopulation *subpop = subpop_pair.second;
		slim_popsize_t subpop_size = subpop->parent_subpop_size_;
		
		// calculate the total fitness across the subpopulation; in nonWF models the population composition can change
		// late in the cycle, due to mortality and migration, so we postpone this assessment to the end of the tick
		// we use the fitness without subpop fitnessScaling, to present individual fitness without density effects
		double subpop_unscaled_total = 0;
		
		for (Individual *individual : subpop->parent_individuals_)
			subpop_unscaled_total += individual->cached_unscaled_fitness_;
		
		totalUnscaledFitness += subpop_unscaled_total;
		totalPopSize += subpop_size;
		
		double meanUnscaledFitness = subpop_unscaled_total / subpop_size;
		
		// Record for SLiMgui display
		subpop->parental_mean_unscaled_fitness_ = meanUnscaledFitness;
		RecordFitness(historyIndex, subpop_pair.first, meanUnscaledFitness);
		RecordSubpopSize(historyIndex, subpop_pair.first, subpop_size);
	}
	
	RecordFitness(historyIndex, -1, totalUnscaledFitness / totalPopSize);
	RecordSubpopSize(historyIndex, -1, totalPopSize);
}
#endif

#ifdef SLIMGUI
// This method is used to tally up histogram metrics that are kept per mutation type for SLiMgui
void Population::AddTallyForMutationTypeAndBinNumber(int p_mutation_type_index, int p_mutation_type_count, slim_tick_t p_bin_number, slim_tick_t **p_buffer, uint32_t *p_bufferBins)
{
	slim_tick_t *buffer = *p_buffer;
	uint32_t bufferBins = *p_bufferBins;
	
	// A negative bin number can occur if the user is using the origin tick of mutations for their own purposes, as a tag field.  To protect against
	// crashing, we therefore clamp the bin number into [0, 1000000].  The upper bound is somewhat arbitrary, but we don't really want to allocate a larger
	// buffer than that anyway, and having values that large get clamped is not the end of the world, since these tallies are just for graphing.
	if (p_bin_number < 0)
		p_bin_number = 0;
	if (p_bin_number > 1000000)
		p_bin_number = 1000000;
	
	if (p_bin_number >= (int64_t)bufferBins)
	{
		int oldEntryCount = bufferBins * p_mutation_type_count;
		
		bufferBins = static_cast<uint32_t>(ceil((p_bin_number + 1) / 128.0) * 128.0);			// give ourselves some headroom so we're not reallocating too often
		int newEntryCount = bufferBins * p_mutation_type_count;
		
		buffer = (slim_tick_t *)realloc(buffer, newEntryCount * sizeof(slim_tick_t));
		
		// Zero out the new entries; the compiler should be smart enough to optimize this...
		for (int i = oldEntryCount; i < newEntryCount; ++i)
			buffer[i] = 0;
		
		// Since we reallocated the buffer, we need to set it back through our pointer parameters
		*p_buffer = buffer;
		*p_bufferBins = bufferBins;
	}
	
	// Add a tally to the appropriate bin
	(buffer[p_mutation_type_index + p_bin_number * p_mutation_type_count])++;
}
#endif

void Population::ValidateMutationFitnessCaches(void)
{
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	int registry_size;
	const MutationIndex *registry_iter = MutationRegistry(&registry_size);
	const MutationIndex *registry_iter_end = registry_iter + registry_size;
	
	while (registry_iter != registry_iter_end)
	{
		MutationIndex mut_index = (*registry_iter++);
		Mutation *mut = mut_block_ptr + mut_index;
		slim_selcoeff_t sel_coeff = mut->selection_coeff_;
		slim_selcoeff_t dom_coeff = mut->mutation_type_ptr_->dominance_coeff_;
		slim_selcoeff_t hemizygous_dom_coeff = mut->mutation_type_ptr_->hemizygous_dominance_coeff_;
		
		mut->cached_one_plus_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + sel_coeff);
		mut->cached_one_plus_dom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + dom_coeff * sel_coeff);
		mut->cached_one_plus_hemizygousdom_sel_ = (slim_selcoeff_t)std::max(0.0, 1.0 + hemizygous_dom_coeff * sel_coeff);
	}
}

void Population::RecalculateFitness(slim_tick_t p_tick)
{
	// calculate the fitnesses of the parents and make lookup tables; the main thing we do here is manage the mutationEffect() callbacks
	// as per the SLiM design spec, we get the list of callbacks once, and use that list throughout this stage, but we construct
	// subsets of it for each subpopulation, so that UpdateFitness() can just use the callback list as given to it
	std::vector<SLiMEidosBlock*> mutationEffect_callbacks = species_.CallbackBlocksMatching(p_tick, SLiMEidosBlockType::SLiMEidosMutationEffectCallback, -1, -1, -1, -1);
	std::vector<SLiMEidosBlock*> fitnessEffect_callbacks = species_.CallbackBlocksMatching(p_tick, SLiMEidosBlockType::SLiMEidosFitnessEffectCallback, -1, -1, -1, -1);
	bool no_active_callbacks = true;
	
	for (SLiMEidosBlock *callback : mutationEffect_callbacks)
		if (callback->block_active_)
		{
			no_active_callbacks = false;
			break;
		}
	if (no_active_callbacks)
		for (SLiMEidosBlock *callback : fitnessEffect_callbacks)
			if (callback->block_active_)
			{
				no_active_callbacks = false;
				break;
			}
	
	// Figure out how we are going to handle MutationRun nonneutral mutation caches; see mutation_run.h.  We need to assess
	// the state of callbacks and decide which of the three "regimes" we are in, and then depending upon that and what
	// regime we were in in the previous generation, invalidate nonneutral caches or allow them to persist.
	const std::map<slim_objectid_t,MutationType*> &mut_types = species_.MutationTypes();
	int32_t last_regime = species_.last_nonneutral_regime_;
	int32_t current_regime;
	
	if (no_active_callbacks)
	{
		current_regime = 1;
	}
	else
	{
		// First, we want to save off the old values of our flags that govern nonneutral caching
		for (auto muttype_iter : mut_types)
		{
			MutationType *muttype = muttype_iter.second;
			
			muttype->previous_set_neutral_by_global_active_callback_ = muttype->set_neutral_by_global_active_callback_;
			muttype->previous_subject_to_mutationEffect_callback_ = muttype->subject_to_mutationEffect_callback_;
		}
		
		// Then we assess which muttypes are being made globally neutral by a constant-value mutationEffect() callback
		bool all_active_callbacks_are_global_neutral_effects = true;
		
		for (auto muttype_iter : mut_types)
			(muttype_iter.second)->set_neutral_by_global_active_callback_ = false;
		
		for (SLiMEidosBlock *mutationEffect_callback : mutationEffect_callbacks)
		{
			if (mutationEffect_callback->block_active_)
			{
				if (mutationEffect_callback->subpopulation_id_ == -1)
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
								
								if (mutation_type_id != -1)
								{
                                    MutationType *found_muttype = species_.MutationTypeWithID(mutation_type_id);
									
									if (found_muttype)
										found_muttype->set_neutral_by_global_active_callback_ = true;
								}
								
								// This is a constant neutral effect, so avoid dropping through to the flag set below
								continue;
							}
						}
					}
				}
				
				// if we reach this point, we have an active callback that is not a
				// global constant neutral effect, so set our flag and break out
				all_active_callbacks_are_global_neutral_effects = false;
				break;
			}
		}
		
		if (all_active_callbacks_are_global_neutral_effects)
		{
			// The only active callbacks are global (i.e. not subpop-specific) constant-effect neutral callbacks,
			// so we will use the set_neutral_by_global_active_callback flag in the muttypes that we set up above.
			// When that flag is true, the mut is neutral; when it is false, consult the selection coefficient.
			current_regime = 2;
		}
		else
		{
			// We have at least one active callback that is not a global constant-effect callback, so all
			// bets are off; any mutation of a muttype influenced by a callback must be considered non-neutral,
			// as governed by the flag set up below
			current_regime = 3;
			
			for (auto muttype_iter : mut_types)
				(muttype_iter.second)->subject_to_mutationEffect_callback_ = false;
			
			for (SLiMEidosBlock *mutationEffect_callback : mutationEffect_callbacks)
			{
				slim_objectid_t mutation_type_id = mutationEffect_callback->mutation_type_id_;
				
				if (mutation_type_id != -1)
				{
                    MutationType *found_muttype = species_.MutationTypeWithID(mutation_type_id);
					
					if (found_muttype)
						found_muttype->subject_to_mutationEffect_callback_ = true;
				}
			}
		}
	}
	
	// trigger a recache of nonneutral mutation lists for some regime transitions; see mutation_run.h
	if (last_regime == 0)									// NOLINTNEXTLINE(*-branch-clone) : intentional branch clones
		species_.nonneutral_change_counter_++;
	else if ((current_regime == 1) && ((last_regime == 2) || (last_regime == 3)))
		species_.nonneutral_change_counter_++;
	else if (current_regime == 2)
	{
		if (last_regime != 2)
			species_.nonneutral_change_counter_++;
		else
		{
			// If we are in regime 2 this cycle and were last cycle as well, then if the way that
			// mutationEffect() callbacks are influencing mutation types is the same this cycle as it was last
			// cycle, we can actually carry over our nonneutral buffers.
			bool callback_state_identical = true;
			
			for (auto muttype_iter : mut_types)
			{
				MutationType *muttype = muttype_iter.second;
				
				if (muttype->set_neutral_by_global_active_callback_ != muttype->previous_set_neutral_by_global_active_callback_)
					callback_state_identical = false;
			}
			
			if (!callback_state_identical)
				species_.nonneutral_change_counter_++;
		}
	}
	else if (current_regime == 3)
	{
		if (last_regime != 3)
			species_.nonneutral_change_counter_++;
		else
		{
			// If we are in regime 3 this cycle and were last cycle as well, then if the way that
			// mutationEffect() callbacks are influencing mutation types is the same this cycle as it was last
			// cycle, we can actually carry over our nonneutral buffers.
			bool callback_state_identical = true;
			
			for (auto muttype_iter : mut_types)
			{
				MutationType *muttype = muttype_iter.second;
				
				if (muttype->subject_to_mutationEffect_callback_ != muttype->previous_subject_to_mutationEffect_callback_)
					callback_state_identical = false;
			}
			
			if (!callback_state_identical)
				species_.nonneutral_change_counter_++;
		}
	}
	
	// move forward to the regime we just chose; UpdateFitness() can consult this to get the current regime
	species_.last_nonneutral_regime_ = current_regime;
	
	SLiMEidosBlockType old_executing_block_type = community_.executing_block_type_;
	community_.executing_block_type_ = SLiMEidosBlockType::SLiMEidosMutationEffectCallback;	// used for both mutationEffect() and fitnessEffect() for simplicity
																							// FIXME this will get cleaned up when multiple phenotypes is done
	
	if (no_active_callbacks)
	{
		std::vector<SLiMEidosBlock*> no_callbacks;
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
			subpop_pair.second->UpdateFitness(no_callbacks, no_callbacks);
	}
	else
	{
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
		{
			slim_objectid_t subpop_id = subpop_pair.first;
			Subpopulation *subpop = subpop_pair.second;
			std::vector<SLiMEidosBlock*> subpop_mutationEffect_callbacks;
			std::vector<SLiMEidosBlock*> subpop_fitnessEffect_callbacks;
			
			// Get mutationEffect() and fitnessEffect() callbacks that apply to this subpopulation
			for (SLiMEidosBlock *callback : mutationEffect_callbacks)
			{
				slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
				
				if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
					subpop_mutationEffect_callbacks.emplace_back(callback);
			}
			for (SLiMEidosBlock *callback : fitnessEffect_callbacks)
			{
				slim_objectid_t callback_subpop_id = callback->subpopulation_id_;
				
				if ((callback_subpop_id == -1) || (callback_subpop_id == subpop_id))
					subpop_fitnessEffect_callbacks.emplace_back(callback);
			}
			
			// Update fitness values, using the callbacks
			subpop->UpdateFitness(subpop_mutationEffect_callbacks, subpop_fitnessEffect_callbacks);
		}
	}
	
	community_.executing_block_type_ = old_executing_block_type;
	
	// reset fitness_scaling_ to 1.0 on subpops and individuals
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
	{
		Subpopulation *subpop = subpop_pair.second;
		
		subpop->subpop_fitness_scaling_ = 1.0;
		
		// Reset fitness_scaling_ on individuals only if it has ever been changed
		if (Individual::s_any_individual_fitness_scaling_set_)
		{
			std::vector<Individual *> &individuals = subpop->parent_individuals_;
			
			for (Individual *individual : individuals)
				individual->fitness_scaling_ = 1.0;
		}
	}
}

#if SLIM_CLEAR_HAPLOSOMES
// WF only:
// Clear all parental haplosomes to use nullptr for their mutation runs, so they are ready to reuse in the next tick
// BCH 10/15/2024: This is now only enabled as a debugging setting; clearing haplosomes is no longer necessary.
void Population::ClearParentalHaplosomes(void)
{
	if (species_.HasGenetics())
	{
		int haplosome_count_per_individual = species_.HaplosomeCountPerIndividual();
		
		EIDOS_BENCHMARK_START(EidosBenchmarkType::k_PARENTS_CLEAR);
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_PARENTS_CLEAR);
#pragma omp parallel default(none) num_threads(thread_count)
		{
			for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
			{
				Subpopulation *subpop = subpop_pair.second;
				
#pragma omp for schedule(static)
				for (Individual *ind : subpop->parent_individuals_)
				{
					Haplosome **haplosomes = ind->haplosomes_;
					
					for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
					{
						Haplosome *haplosome = haplosomes[haplosome_index];
						
						haplosome->clear_to_nullptr();
					}
				}
			}
			
			// We have to clear out removed subpops, too, for as long as they stick around
			for (Subpopulation *subpop : removed_subpops_)
			{
#pragma omp for schedule(static)
				for (Individual *ind : subpop->parent_individuals_)
				{
					Haplosome **haplosomes = ind->haplosomes_;
					
					for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
					{
						Haplosome *haplosome = haplosomes[haplosome_index];
						
						haplosome->clear_to_nullptr();
					}
				}
				
#pragma omp for schedule(static)
				for (Individual *ind : subpop->child_individuals_)
				{
					Haplosome **haplosomes = ind->haplosomes_;
					
					for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
					{
						Haplosome *haplosome = haplosomes[haplosome_index];
						
						haplosome->clear_to_nullptr();
					}
				}
			}
		}
		EIDOS_BENCHMARK_END(EidosBenchmarkType::k_PARENTS_CLEAR);
	}
}
#endif

// Scan through all mutation runs in the simulation and unique them
void Population::UniqueMutationRuns(void)
{
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::UniqueMutationRuns): (internal error) called with child generation active!" << EidosTerminate();
	
#if SLIM_DEBUG_MUTATION_RUNS
	std::clock_t begin = std::clock();
#endif
	int64_t total_mutruns = 0, total_hash_collisions = 0, total_identical = 0, total_uniqued_away = 0, total_preexisting = 0, total_final = 0;
	int64_t operation_id = MutationRun::GetNextOperationID();
	const std::vector<Chromosome *> &chromosomes = species_.Chromosomes();
	size_t chromosome_count = chromosomes.size();
	
	EIDOS_BENCHMARK_START(EidosBenchmarkType::k_UNIQUE_MUTRUNS);
	
	// BCH 22 Oct. 2024: we want the top-level loop to be over mutation runs; we want to do the uniquing work
	// on a per-mutation-run basis.  However, mutation runs live inside haplosomes, which correspond to
	// chromosomes, and that correspondence is important; the mutation runs of two haplosomes that represent
	// the same chromosome need to be uniqued against each other, not independently.  So the new top-level
	// loop is over chromosomes, and then over mutruns and haplosomes that correspond to each chromosome.
	// FIXME: TO BE PARALLELIZED
	for (size_t chromosome_index = 0; chromosome_index < chromosome_count; ++chromosome_index)
	{
		Chromosome *chromosome = chromosomes[chromosome_index];
		int first_haplosome_index = species_.FirstHaplosomeIndices()[chromosome_index];
		int last_haplosome_index = species_.LastHaplosomeIndices()[chromosome_index];
		
		int64_t count_mutruns = 0, count_hash_collisions = 0, count_identical = 0, count_uniqued_away = 0, count_preexisting = 0, count_final = 0;
		int mutrun_count_multiplier = chromosome->mutrun_count_multiplier_;
		int mutrun_context_count = chromosome->ChromosomeMutationRunContextCount();
		int mutrun_count = chromosome->mutrun_count_;
		
		if (mutrun_count_multiplier * mutrun_context_count != mutrun_count)
			EIDOS_TERMINATION << "ERROR (Population::UniqueMutationRuns): (internal error) mutation run subdivision is incorrect." << EidosTerminate();
		
		// Each mutation run index is now uniqued individually, because mutation runs cannot be used at more than one position.
		// This prevents empty mutation runs, in particular, from getting shared across positions, a necessary restriction.
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_UNIQUE_MUTRUNS);
#pragma omp parallel for schedule(dynamic) default(none) shared(mutrun_count) firstprivate(operation_id) reduction(+: count_mutruns) reduction(+: count_hash_collisions) reduction(+: count_identical) reduction(+: count_uniqued_away) reduction(+: count_preexisting) reduction(+: count_final) num_threads(thread_count)
		for (int mutrun_index = 0; mutrun_index < mutrun_count; ++mutrun_index)
		{
			std::unordered_multimap<int64_t, const MutationRun *> runmap;	// BCH 4/30/2023: switched to unordered, it is faster
			
			for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
			{
				Subpopulation *subpop = subpop_pair.second;
				
				for (Individual *ind : subpop->parent_individuals_)
				{
					Haplosome **haplosomes = ind->haplosomes_;
					
					for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
					{
						Haplosome *haplosome = haplosomes[haplosome_index];
						
						if (haplosome->IsNull())
							continue;
						
						const MutationRun *mut_run = haplosome->mutruns_[mutrun_index];
						
						if (mut_run)
						{
							bool first_sight_of_this_mutrun = false;
							
							count_mutruns++;
							
							if (mut_run->operation_id_ != operation_id)
							{
								// Mark each new run we encounter with the operation ID, to count the preexisting number of runs
								count_preexisting++;
								mut_run->operation_id_ = operation_id;
								first_sight_of_this_mutrun = true;
							}
							
							// Calculate a hash for this mutrun.  Note that we could store computed hashes into the runs above, so that
							// we only hash each pre-existing run once; but that would require an int64_t more storage per mutrun, and
							// the memory overhead doesn't presently seem worth the very slight performance gain it would usually provide
							int64_t hash = mut_run->Hash();
							
							// See if we have any mutruns already defined with this hash.  Note that we actually want to do this search
							// even when first_sight_of_this_mutrun = true, because we want to find hash collisions, which may be other
							// runs that are identical to us despite being separate objects.  That is, in fact, kind of the point.
							auto range = runmap.equal_range(hash);		// pair<Iter, Iter>
							
							if (range.first == range.second)
							{
								// No previous mutrun found with this hash, so add this mutrun to the multimap
								runmap.emplace(hash, mut_run);
								count_final++;
							}
							else
							{
								// There is at least one hit; first cycle through the hits and see if any of them are pointer-identical
								for (auto hash_iter = range.first; hash_iter != range.second; ++hash_iter)
								{
									if (mut_run == hash_iter->second)
									{
										count_identical++;
										goto is_identical;
									}
								}
								
								// OK, we have no pointer-identical matches; check for a duplicate using Identical()
								for (auto hash_iter = range.first; hash_iter != range.second; ++hash_iter)
								{
									const MutationRun *hash_run = hash_iter->second;
									
									if (mut_run->Identical(*hash_run))
									{
										haplosome->mutruns_[mutrun_index] = hash_run;
										count_identical++;
										
										// We will unique away all references to this mutrun, but we only want to count it once
										if (first_sight_of_this_mutrun)
											count_uniqued_away++;
										goto is_identical;
									}
								}
								
								// If there was no identical match, then we have a hash collision; put it in the multimap
								runmap.emplace(hash, mut_run);
								count_hash_collisions++;
								count_final++;
								
							is_identical:
								;
							}
						}
					}
				}
			}
		}
		
		total_mutruns += count_mutruns;
		total_hash_collisions += count_hash_collisions;
		total_identical += count_identical;
		total_uniqued_away += count_uniqued_away;
		total_preexisting += count_preexisting;
		total_final += count_final;
	}
	
	EIDOS_BENCHMARK_END(EidosBenchmarkType::k_UNIQUE_MUTRUNS);
	
#if SLIM_DEBUG_MUTATION_RUNS
	std::clock_t end = std::clock();
	double time_spent = static_cast<double>(end - begin) / CLOCKS_PER_SEC;
	
	std::cout << "UniqueMutationRuns(), tick " << community_.Tick() << ": \n   " << total_mutruns << " run pointers analyzed\n   " << total_preexisting << " runs pre-existing\n   " << total_uniqued_away << " duplicate runs discovered and uniqued away\n   " << (total_mutruns - total_identical) << " final uniqued mutation runs\n   " << total_hash_collisions << " hash collisions\n   " << time_spent << " seconds elapsed" << std::endl;
#else
    // get rid of unused variable warnings
    (void)total_hash_collisions;
    (void)total_mutruns;
    (void)total_preexisting;
    (void)total_uniqued_away;
    (void)total_identical;
#endif
	
	if (total_final != total_mutruns - total_identical)
		EIDOS_TERMINATION << "ERROR (Population::UniqueMutationRuns): (internal error) bookkeeping error in mutation run uniquing." << EidosTerminate();
}

#ifndef __clang_analyzer__
void Population::SplitMutationRunsForChromosome(int32_t p_new_mutrun_count, Chromosome *p_chromosome)
{
	// Note this method assumes that mutation run refcounts are correct; we enforce that here
	TallyMutationRunReferencesForPopulationForChromosome(p_chromosome);
	
	int first_haplosome_index = species_.FirstHaplosomeIndices()[p_chromosome->Index()];
	int last_haplosome_index = species_.LastHaplosomeIndices()[p_chromosome->Index()];
	
	if (model_type_ == SLiMModelType::kModelTypeWF)
	{
		// fix the child haplosomes for the chromosome since they also need to be resized
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			
			for (Individual *ind : subpop->child_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
					{
						int32_t old_mutrun_count = haplosome->mutrun_count_;
						slim_position_t old_mutrun_length = haplosome->mutrun_length_;
						int32_t new_mutrun_count = old_mutrun_count << 1;
						slim_position_t new_mutrun_length = old_mutrun_length >> 1;
						
						if (haplosome->mutruns_ != haplosome->run_buffer_)
							free(haplosome->mutruns_);
						haplosome->mutruns_ = nullptr;
						
						haplosome->mutrun_count_ = new_mutrun_count;
						haplosome->mutrun_length_ = new_mutrun_length;
						
						if (new_mutrun_count <= SLIM_HAPLOSOME_MUTRUN_BUFSIZE)
						{
							haplosome->mutruns_ = haplosome->run_buffer_;
#if SLIM_CLEAR_HAPLOSOMES
							EIDOS_BZERO(haplosome->run_buffer_, SLIM_HAPLOSOME_MUTRUN_BUFSIZE * sizeof(const MutationRun *));
#endif
						}
						else
						{
#if SLIM_CLEAR_HAPLOSOMES
							haplosome->mutruns_ = (const MutationRun **)calloc(new_mutrun_count, sizeof(const MutationRun *));
#else
							haplosome->mutruns_ = (const MutationRun **)malloc(new_mutrun_count * sizeof(const MutationRun *));
#endif
						}
						
						// we leave the haplosome cleared to nullptr, as expected by the WF code
					}
				}
			}
		}
	}
	
	// make a map to keep track of which mutation runs split into which new runs
#if EIDOS_ROBIN_HOOD_HASHING
	robin_hood::unordered_flat_map<const MutationRun *, std::pair<const MutationRun *, const MutationRun *>> split_map;
    //typedef robin_hood::pair<const MutationRun *, std::pair<const MutationRun *, const MutationRun *>> SLiM_SPLIT_PAIR;
#elif STD_UNORDERED_MAP_HASHING
	std::unordered_map<const MutationRun *, std::pair<const MutationRun *, const MutationRun *>> split_map;
    //typedef std::pair<const MutationRun *, std::pair<const MutationRun *, const MutationRun *>> SLiM_SPLIT_PAIR;
#endif
	
	int mutruns_buf_index;
	const MutationRun **mutruns_buf = (const MutationRun **)calloc(p_new_mutrun_count, sizeof(const MutationRun *));
	
	if (!mutruns_buf)
		EIDOS_TERMINATION << "ERROR (Population::SplitMutationRuns): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	try {
		// for every subpop
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
					{
						int32_t old_mutrun_count = haplosome->mutrun_count_;
						slim_position_t old_mutrun_length = haplosome->mutrun_length_;
						int32_t new_mutrun_count = old_mutrun_count << 1;
						slim_position_t new_mutrun_length = old_mutrun_length >> 1;
						
						// for every mutation run, fill up mutrun_buf with entries
						mutruns_buf_index = 0;
						
						for (int run_index = 0; run_index < old_mutrun_count; ++run_index)
						{
							const MutationRun *mutrun = haplosome->mutruns_[run_index];
							MutationRunContext &mutrun_context = p_chromosome->ChromosomeMutationRunContextForMutationRunIndex(run_index);
							
							if (mutrun->use_count() == 1)
							{
								// this mutrun is only referenced once, so we can just replace it without using the map
								// checking use_count() this way is only safe because we run directly after tallying!
								MutationRun *first_half, *second_half;
								
								mutrun->split_run(&first_half, &second_half, new_mutrun_length * (mutruns_buf_index + 1), mutrun_context);
								
								mutruns_buf[mutruns_buf_index++] = first_half;
								mutruns_buf[mutruns_buf_index++] = second_half;
							}
							else
							{
								// this mutrun is referenced more than once, so we want to use our map
								auto found_entry = split_map.find(mutrun);
								
								if (found_entry != split_map.end())
								{
									// it was in the map already, so just use the values from the map
									std::pair<const MutationRun *, const MutationRun *> &map_value = found_entry->second;
									const MutationRun *first_half = map_value.first;
									const MutationRun *second_half = map_value.second;
									
									mutruns_buf[mutruns_buf_index++] = first_half;
									mutruns_buf[mutruns_buf_index++] = second_half;
								}
								else
								{
									// it was not in the map, so make the new runs, and insert them into the map
									MutationRun *first_half, *second_half;
									
									mutrun->split_run(&first_half, &second_half, new_mutrun_length * (mutruns_buf_index + 1), mutrun_context);
									
									mutruns_buf[mutruns_buf_index++] = first_half;
									mutruns_buf[mutruns_buf_index++] = second_half;
									
									split_map.emplace(mutrun, std::pair<MutationRun *, MutationRun *>(first_half, second_half));
								}
							}
						}
						
						// now replace the runs in the haplosome with those in mutrun_buf
						if (haplosome->mutruns_ != haplosome->run_buffer_)
							free(haplosome->mutruns_);
						haplosome->mutruns_ = nullptr;
						
						haplosome->mutrun_count_ = new_mutrun_count;
						haplosome->mutrun_length_ = new_mutrun_length;
						
						if (new_mutrun_count <= SLIM_HAPLOSOME_MUTRUN_BUFSIZE)
							haplosome->mutruns_ = haplosome->run_buffer_;
						else
							haplosome->mutruns_ = (const MutationRun **)malloc(new_mutrun_count * sizeof(const MutationRun *));	// not calloc() because overwritten below
						
						for (int run_index = 0; run_index < new_mutrun_count; ++run_index)
							haplosome->mutruns_[run_index] = mutruns_buf[run_index];
					}
				}
			}
		}
	} catch (...) {
		// I think the emplace() call is the only thing above that is likely to raise, due e.g. to using a bad hash function...
		EIDOS_TERMINATION << "ERROR (Population::SplitMutationRuns): (internal error) SLiM encountered a raise from an internal hash table; please report this." << EidosTerminate(nullptr);
	}
	
	if (mutruns_buf)
		free(mutruns_buf);
}
#else
// the static analyzer has a lot of trouble understanding this method
void Population::SplitMutationRunsForChromosome(int32_t p_new_mutrun_count, Chromosome *p_chromosome)
{
}
#endif

// define a hash function for std::pair<MutationRun *, MutationRun *> so we can use it in our hash table below
// see https://stackoverflow.com/questions/32685540/c-unordered-map-with-pair-as-key-not-compiling
struct slim_pair_hash {
	template <class T1, class T2>
	std::size_t operator () (const std::pair<T1,T2> &p) const {
#if EIDOS_ROBIN_HOOD_HASHING
		auto h1 = robin_hood::hash<T1>{}(p.first);
		auto h2 = robin_hood::hash<T2>{}(p.second);
#elif STD_UNORDERED_MAP_HASHING
		auto h1 = std::hash<T1>{}(p.first);
		auto h2 = std::hash<T2>{}(p.second);
#endif
		
		// This is not a great hash function, but for our purposes it should actually be fine.
		// We don't expect identical pairs <A, A>, and if we have a pair <A, B> we don't
		// expect to get the reversed pair <B, A>, so this should not produce too many collisions.
		// If we do get collisions we could switch to MutationRun::Hash() instead, but it is
		// much slower, so this is probably better.
		//return h1 ^ h2;
		
		// BCH 8/12/2021: Actually, we do see identical pairs <A, A> in some cases, so the
		// above hash function is really not great – it produces 0 for all such cases.  Let's
		// try being just a little bit smarter, so that <A, A> produces a variety of values.
		// We lost the top bits of h1, but often they will be 0 anyway, or always the same,
		// since these are pointers and the high bits are the overall region of memory we're in.
		return (h1 << 8) ^ h2;
	}
};

#ifndef __clang_analyzer__
void Population::JoinMutationRunsForChromosome(int32_t p_new_mutrun_count, Chromosome *p_chromosome)
{
	// Note this method assumes that mutation run refcounts are correct; we enforce that here
	TallyMutationRunReferencesForPopulationForChromosome(p_chromosome);
	
	int first_haplosome_index = species_.FirstHaplosomeIndices()[p_chromosome->Index()];
	int last_haplosome_index = species_.LastHaplosomeIndices()[p_chromosome->Index()];
	
	if (model_type_ == SLiMModelType::kModelTypeWF)
	{
		// fix the child haplosomes for the chromosome since they also need to be resized
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			
			for (Individual *ind : subpop->child_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
					{
						int32_t old_mutrun_count = haplosome->mutrun_count_;
						slim_position_t old_mutrun_length = haplosome->mutrun_length_;
						int32_t new_mutrun_count = old_mutrun_count >> 1;
						slim_position_t new_mutrun_length = old_mutrun_length << 1;
						
						if (haplosome->mutruns_ != haplosome->run_buffer_)
							free(haplosome->mutruns_);
						haplosome->mutruns_ = nullptr;
						
						haplosome->mutrun_count_ = new_mutrun_count;
						haplosome->mutrun_length_ = new_mutrun_length;
						
						if (new_mutrun_count <= SLIM_HAPLOSOME_MUTRUN_BUFSIZE)
						{
							haplosome->mutruns_ = haplosome->run_buffer_;
#if SLIM_CLEAR_HAPLOSOMES
							EIDOS_BZERO(haplosome->run_buffer_, SLIM_HAPLOSOME_MUTRUN_BUFSIZE * sizeof(const MutationRun *));
#endif
						}
						else
						{
#if SLIM_CLEAR_HAPLOSOMES
							haplosome->mutruns_ = (const MutationRun **)calloc(new_mutrun_count, sizeof(const MutationRun *));
#else
							haplosome->mutruns_ = (const MutationRun **)malloc(new_mutrun_count * sizeof(const MutationRun *));
#endif
						}
						
						// we leave the haplosome cleared to nullptr, as expected by the WF code
					}
				}
			}
		}
	}
	
	// make a map to keep track of which mutation runs join into which new runs
#if EIDOS_ROBIN_HOOD_HASHING
	robin_hood::unordered_flat_map<std::pair<const MutationRun *, const MutationRun *>, const MutationRun *, slim_pair_hash> join_map;
    //typedef robin_hood::pair<std::pair<const MutationRun *, const MutationRun *>, const MutationRun *> SLiM_JOIN_PAIR;
#elif STD_UNORDERED_MAP_HASHING
	std::unordered_map<std::pair<const MutationRun *, const MutationRun *>, const MutationRun *, slim_pair_hash> join_map;
    //typedef std::pair<std::pair<const MutationRun *, const MutationRun *>, const MutationRun *> SLiM_JOIN_PAIR;
#endif
	int mutruns_buf_index;
	const MutationRun **mutruns_buf = (const MutationRun **)calloc(p_new_mutrun_count, sizeof(const MutationRun *));
	
	if (!mutruns_buf)
		EIDOS_TERMINATION << "ERROR (Population::JoinMutationRuns): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	try {
		// for every subpop
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
					{
						int32_t old_mutrun_count = haplosome->mutrun_count_;
						slim_position_t old_mutrun_length = haplosome->mutrun_length_;
						int32_t new_mutrun_count = old_mutrun_count >> 1;
						slim_position_t new_mutrun_length = old_mutrun_length << 1;
						
						// for every mutation run, fill up mutrun_buf with entries
						mutruns_buf_index = 0;
						
						for (int run_index = 0; run_index < old_mutrun_count; run_index += 2)
						{
							const MutationRun *mutrun1 = haplosome->mutruns_[run_index];
							const MutationRun *mutrun2 = haplosome->mutruns_[run_index + 1];
							MutationRunContext &mutrun_context = p_chromosome->ChromosomeMutationRunContextForMutationRunIndex(run_index);
							
							if ((mutrun1->use_count() == 1) || (mutrun2->use_count() == 1))
							{
								// one of these mutruns is only referenced once, so we can just replace them without using the map
								// checking use_count() this way is only safe because we run directly after tallying!
								MutationRun *joined_run = MutationRun::NewMutationRun(mutrun_context);	// take from shared pool of used objects
								
								joined_run->copy_from_run(*mutrun1);
								joined_run->emplace_back_bulk(mutrun2->begin_pointer_const(), mutrun2->size());
								
								mutruns_buf[mutruns_buf_index++] = joined_run;
							}
							else
							{
								// this mutrun is referenced more than once, so we want to use our map
								auto found_entry = join_map.find(std::pair<const MutationRun *, const MutationRun *>(mutrun1, mutrun2));
								
								if (found_entry != join_map.end())
								{
									// it was in the map already, so just use the values from the map
									const MutationRun *map_value = found_entry->second;
									
									mutruns_buf[mutruns_buf_index++] = map_value;
								}
								else
								{
									// it was not in the map, so make the new runs, and insert them into the map
									MutationRun *joined_run = MutationRun::NewMutationRun(mutrun_context);	// take from shared pool of used objects
									
									joined_run->copy_from_run(*mutrun1);
									joined_run->emplace_back_bulk(mutrun2->begin_pointer_const(), mutrun2->size());
									
									mutruns_buf[mutruns_buf_index++] = joined_run;
									
									join_map.emplace(std::pair<const MutationRun *, const MutationRun *>(mutrun1, mutrun2), joined_run);
								}
							}
						}
						
						// now replace the runs in the haplosome with those in mutrun_buf
						if (haplosome->mutruns_ != haplosome->run_buffer_)
							free(haplosome->mutruns_);
						haplosome->mutruns_ = nullptr;
						
						haplosome->mutrun_count_ = new_mutrun_count;
						haplosome->mutrun_length_ = new_mutrun_length;
						
						if (new_mutrun_count <= SLIM_HAPLOSOME_MUTRUN_BUFSIZE)
							haplosome->mutruns_ = haplosome->run_buffer_;
						else
							haplosome->mutruns_ = (const MutationRun **)malloc(new_mutrun_count * sizeof(const MutationRun *));	// not calloc() because overwritten below
						
						for (int run_index = 0; run_index < new_mutrun_count; ++run_index)
							haplosome->mutruns_[run_index] = mutruns_buf[run_index];
					}
				}
			}
		}
	} catch (...) {
		// I think the emplace() call is the only thing above that is likely to raise, due e.g. to using a bad hash function...
		EIDOS_TERMINATION << "ERROR (Population::JoinMutationRuns): (internal error) SLiM encountered a raise from an internal hash table; please report this." << EidosTerminate(nullptr);
	}

	if (mutruns_buf)
		free(mutruns_buf);
}
#else
// the static analyzer has a lot of trouble understanding this method
void Population::JoinMutationRunsForChromosome(int32_t p_new_mutrun_count, Chromosome *p_chromosome)
{
}
#endif

// Tally mutations and remove fixed/lost mutations
void Population::MaintainMutationRegistry(void)
{
	if ((model_type_ == SLiMModelType::kModelTypeWF) && child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::MaintainMutationRegistry): (internal error) MaintainMutationRegistry() may only be called from the parent generation in WF models." << EidosTerminate();
	
	// go through all haplosomes and increment mutation reference counts; this updates total_haplosome_count_
	// this will call TallyMutationRunReferencesForPopulation() as a side effect unless it hits its cache
	{
		InvalidateMutationReferencesCache();	// force a retally
		
		EIDOS_BENCHMARK_START(EidosBenchmarkType::k_MUT_TALLY);
		TallyMutationReferencesAcrossPopulation(/* p_clock_for_mutrun_experiments */ true);
		EIDOS_BENCHMARK_END(EidosBenchmarkType::k_MUT_TALLY);
	}
	
	// free unused mutation runs, relying upon the tally done above
	{
		EIDOS_BENCHMARK_START(EidosBenchmarkType::k_MUTRUN_FREE);
		FreeUnusedMutationRuns();
		EIDOS_BENCHMARK_END(EidosBenchmarkType::k_MUTRUN_FREE);
	}
	
	// remove any mutations that have been eliminated or have fixed
	{
		EIDOS_BENCHMARK_START(EidosBenchmarkType::k_MUT_FREE);
		RemoveAllFixedMutations();
		EIDOS_BENCHMARK_END(EidosBenchmarkType::k_MUT_FREE);
	}
	
	// check that the mutation registry does not have any "zombies" – mutations that have been removed and should no longer be there
	// also check for any mutations that are in the registry but do not have the state MutationState::kInRegistry
#if DEBUG
	CheckMutationRegistry(true);	// full check
	registry_needs_consistency_check_ = false;
#else
	if (registry_needs_consistency_check_)
	{
		CheckMutationRegistry(false);	// check registry but not haplosomes
		registry_needs_consistency_check_ = false;
	}
#endif
	
	// debug output: assess mutation run usage patterns
#if SLIM_DEBUG_MUTATION_RUNS
	AssessMutationRuns();
#endif
}

// assess usage patterns of mutation runs across the simulation
void Population::AssessMutationRuns(void)
{
	// Note this method assumes that mutation run use counts are correct; it should be called immediately after tallying
	
	if ((model_type_ == SLiMModelType::kModelTypeWF) && child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::AssessMutationRuns): (internal error) AssessMutationRuns() may only be called from the parent generation in WF models." << EidosTerminate();
	
	slim_tick_t tick = community_.Tick();
	
	if (tick % 1000 == 0)
	{
		std::cout << "***** AssessMutationRuns(), tick " << tick << ":" << std::endl;
		std::cout << "   Mutation count: " << mutation_registry_.size() << std::endl;
		
		for (Chromosome *chromosome : species_.Chromosomes())
		{
			slim_chromosome_index_t chromosome_index = chromosome->Index();
			int registry_size = 0, registry_count_in_chromosome = 0;
			Mutation *mut_block_ptr = gSLiM_Mutation_Block;
			const MutationIndex *registry = MutationRegistry(&registry_size);
			
			for (int registry_index = 0; registry_index < registry_size; ++registry_index)
			{
				Mutation *mut = mut_block_ptr + registry[registry_index];
				
				if (mut->chromosome_index_ == chromosome_index)
					registry_count_in_chromosome++;
			}
			
			int first_haplosome_index = species_.FirstHaplosomeIndices()[chromosome_index];
			int last_haplosome_index = species_.LastHaplosomeIndices()[chromosome_index];
			slim_refcount_t total_haplosome_count = 0, total_mutrun_count = 0, total_shared_mutrun_count = 0;
			int mutrun_count = 0, use_count_total = 0;
			slim_position_t mutrun_length = 0;
			int64_t mutation_total = 0;
			
			int64_t operation_id = MutationRun::GetNextOperationID();
			
			for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
			{
				Subpopulation *subpop = subpop_pair.second;
				
				for (Individual *ind : subpop->parent_individuals_)
				{
					Haplosome **haplosomes = ind->haplosomes_;
					
					for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
					{
						Haplosome *haplosome = haplosomes[haplosome_index];
						
						if (!haplosome->IsNull())
						{
							mutrun_count = haplosome->mutrun_count_;
							mutrun_length = haplosome->mutrun_length_;
							
							for (int run_index = 0; run_index < mutrun_count; ++run_index)
							{
								const MutationRun *mutrun = haplosome->mutruns_[run_index];
								int mutrun_size = mutrun->size();
								
								total_mutrun_count++;
								mutation_total += mutrun_size;
								
								if (mutrun->operation_id_ != operation_id)
								{
									slim_refcount_t use_count = (slim_refcount_t)mutrun->use_count();
									
									total_shared_mutrun_count++;
									use_count_total += use_count;
									
									mutrun->operation_id_ = operation_id;
								}
							}
							
							total_haplosome_count++;
						}
					}
				}
			}
			
			std::cout << "   ========== Chromosome index " << (unsigned int)(chromosome->Index()) << ", id " << chromosome->ID() << ", symbol " << chromosome->Symbol() << " (length " << (chromosome->last_position_ + 1) << ")" << std::endl;
			std::cout << "   Mutation count in chromosome: " << registry_count_in_chromosome << std::endl;
			std::cout << "   Haplosome count: " << total_haplosome_count << " (divided into " << mutrun_count << " mutation runs of length " << mutrun_length << ")" << std::endl;
			
			std::cout << "   Mutation run unshared: " << total_mutrun_count;
			if (total_mutrun_count) std::cout << " (containing " << (mutation_total / (double)total_mutrun_count) << " mutations on average)";
			std::cout << std::endl;
			
			std::cout << "   Mutation run actual: " << total_shared_mutrun_count;
			if (total_shared_mutrun_count) std::cout << " (mean use count " << (use_count_total / (double)total_shared_mutrun_count) << ")";
			std::cout << std::endl;
		}
	}
}

// WF only:
// step forward to the next generation: make the children become the parents
void Population::SwapGenerations(void)
{
	// record lifetime reproductive outputs for all parents before swapping, including in subpops being removed
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
		subpop_pair.second->TallyLifetimeReproductiveOutput();
	for (Subpopulation *subpop : removed_subpops_)
		subpop->TallyLifetimeReproductiveOutput();
	
	// dispose of any freed subpops
	PurgeRemovedSubpopulations();
	
	// make children the new parents; each subpop flips its child_generation_valid_ flag at the end of this call
	for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
		subpop_pair.second->SwapChildAndParentHaplosomes();
	
	// flip our flag to indicate that the good haplosomes are now in the parental generation, and the next child generation is ready to be produced
	child_generation_valid_ = false;
}

void Population::TallyMutationRunReferencesForPopulationForChromosome(Chromosome *p_chromosome)
{
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::TallyMutationRunReferencesForPopulationForChromosome): (internal error) called with child generation active!" << EidosTerminate();
	
	slim_refcount_t tallied_haplosome_count = 0;
	int first_haplosome_index = species_.FirstHaplosomeIndices()[p_chromosome->Index()];
	int last_haplosome_index = species_.LastHaplosomeIndices()[p_chromosome->Index()];
	int mutrun_count_multiplier = p_chromosome->mutrun_count_multiplier_;
	int mutrun_context_count = p_chromosome->ChromosomeMutationRunContextCount();
	
	if (mutrun_count_multiplier * mutrun_context_count != p_chromosome->mutrun_count_)
		EIDOS_TERMINATION << "ERROR (Population::TallyMutationRunReferencesForPopulationForChromosome): (internal error) mutation run subdivision is incorrect." << EidosTerminate();
	
	// THIS PARALLEL REGION CANNOT HAVE AN IF()!  IT MUST ALWAYS EXECUTE PARALLEL!
	// the reduction() is a bit odd - every thread will generate the same value, and we just want that value,
	// but lastprivate() is not legal for parallel regions, for some reason, so we use reduction(max)
#pragma omp parallel default(none) shared(mutrun_count_multiplier, mutrun_context_count, std::cerr) reduction(max: total_haplosome_count) num_threads(mutrun_context_count)
	{
		// this is initialized by OpenMP to the most negative number (reduction type max); we want zero instead!
		tallied_haplosome_count = 0;
		
#ifdef _OPENMP
		// it is imperative that we run with the requested number of threads
		if (omp_get_num_threads() != mutrun_context_count)
		{
			std::cerr << "requested  " << mutrun_context_count << " threads but got " << omp_get_num_threads() << std::endl;
			THREAD_SAFETY_IN_ANY_PARALLEL("Population::TallyMutationRunReferencesForPopulationForChromosome(): incorrect thread count!");
		}
#endif
		
		// first, zero all use counts across all in-use MutationRun objects
		// each thread does its own zeroing, for its own MutationRunContext
		{
			MutationRunContext &mutrun_context = p_chromosome->ChromosomeMutationRunContextForThread(omp_get_thread_num());
			
			for (const MutationRun *mutrun : mutrun_context.in_use_pool_)
				mutrun->zero_use_count();
		}
		
		// second, loop through all haplosomes in all subpops and tally the usage of their MutationRun objects
		// each thread handles only the range of mutation run indices that it is responsible for
		int first_mutrun_index = omp_get_thread_num() * mutrun_count_multiplier;
		int last_mutrun_index = first_mutrun_index + mutrun_count_multiplier - 1;
		
		// note this is NOT an OpenMP parallel for loop!  each encountering thread runs every iteration!
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			
			if (subpop->CouldContainNullHaplosomes())
			{
				for (Individual *ind : subpop->parent_individuals_)
				{
					Haplosome **haplosomes = ind->haplosomes_;
					
					for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
					{
						Haplosome *haplosome = haplosomes[haplosome_index];
						
						if (!haplosome->IsNull())
						{
							for (int run_index = first_mutrun_index; run_index <= last_mutrun_index; ++run_index)
								haplosome->mutruns_[run_index]->increment_use_count();
							
							tallied_haplosome_count++;
						}
					}
				}
			}
			else
			{
				// optimized case when null haplosomes do not exist in this subpop
				
				if (last_haplosome_index == first_haplosome_index + 1)
				{
					// optimize the simple diploid single-chromosome case
					if (first_haplosome_index == 0)
					{
						// optimize the first-chromosome case
						if ((first_mutrun_index == last_mutrun_index) && (first_mutrun_index == 0))
						{
							// optimize the one-mutrun case (given first-chromosome as well)
							// this is the hotspot for simple one-chromosome diploid models; note that it
							// runs about twice as slowly as in 4.3, because we no longer have subpop_genomes
							// to loop through directly, so we have to gather haplosomes from individuals;
							// I don't see a way to recover that performance loss easily
							for (Individual *ind : subpop->parent_individuals_)
							{
								ind->haplosomes_[0]->mutruns_[0]->increment_use_count();
								ind->haplosomes_[1]->mutruns_[0]->increment_use_count();
							}
						}
						else
						{
							for (Individual *ind : subpop->parent_individuals_)
							{
								Haplosome *haplosome0 = ind->haplosomes_[0];
								Haplosome *haplosome1 = ind->haplosomes_[1];
								
								for (int run_index = first_mutrun_index; run_index <= last_mutrun_index; ++run_index)
								{
									haplosome0->mutruns_[run_index]->increment_use_count();
									haplosome1->mutruns_[run_index]->increment_use_count();
								}
							}
						}
					}
					else
					{
						if (first_mutrun_index == last_mutrun_index)
						{
							// optimize the one-mutrun case
							for (Individual *ind : subpop->parent_individuals_)
							{
								ind->haplosomes_[first_haplosome_index]->mutruns_[first_mutrun_index]->increment_use_count();
								ind->haplosomes_[first_haplosome_index+1]->mutruns_[first_mutrun_index]->increment_use_count();
							}
						}
						else
						{
							for (Individual *ind : subpop->parent_individuals_)
							{
								Haplosome *haplosome0 = ind->haplosomes_[first_haplosome_index];
								Haplosome *haplosome1 = ind->haplosomes_[first_haplosome_index+1];
								
								for (int run_index = first_mutrun_index; run_index <= last_mutrun_index; ++run_index)
								{
									haplosome0->mutruns_[run_index]->increment_use_count();
									haplosome1->mutruns_[run_index]->increment_use_count();
								}
							}
						}
					}
				}
				else
				{
					for (Individual *ind : subpop->parent_individuals_)
					{
						Haplosome **haplosomes = ind->haplosomes_;
						
						for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
						{
							Haplosome *haplosome = haplosomes[haplosome_index];
							
							for (int run_index = first_mutrun_index; run_index <= last_mutrun_index; ++run_index)
								haplosome->mutruns_[run_index]->increment_use_count();
						}
					}
				}
				
				tallied_haplosome_count += (subpop->parent_individuals_.size() * (last_haplosome_index - first_haplosome_index + 1));
			}
		}
	}
	
#if DEBUG
	// In debug builds we do a complete re-tally single-threaded, into a side counter, for a check-back
	{
		slim_refcount_t tallied_haplosome_count_CHECK = 0;
		
		for (int threadnum = 0; threadnum < p_chromosome->ChromosomeMutationRunContextCount(); ++threadnum)
		{
			MutationRunContext &mutrun_context = p_chromosome->ChromosomeMutationRunContextForThread(threadnum);
			MutationRunPool &inuse_pool = mutrun_context.in_use_pool_;
			size_t inuse_pool_count = inuse_pool.size();
			
			for (size_t pool_index = 0; pool_index < inuse_pool_count; ++pool_index)
				inuse_pool[pool_index]->use_count_CHECK_ = 0;
		}
		
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
					{
						int mutrun_count = haplosome->mutrun_count_;
						
						for (int run_index = 0; run_index < mutrun_count; ++run_index)
							haplosome->mutruns_[run_index]->use_count_CHECK_++;
						
						tallied_haplosome_count_CHECK++;
					}
				}
			}
		}
		
		if (tallied_haplosome_count_CHECK != tallied_haplosome_count)
			EIDOS_TERMINATION << "ERROR (Population::TallyMutationRunReferencesForPopulationForChromosome): (internal error) tallied_haplosome_count_CHECK != tallied_haplosome_count (" << tallied_haplosome_count_CHECK << " != " << tallied_haplosome_count << ")." << EidosTerminate();
		
		for (int threadnum = 0; threadnum < p_chromosome->ChromosomeMutationRunContextCount(); ++threadnum)
		{
			MutationRunContext &mutrun_context = p_chromosome->ChromosomeMutationRunContextForThread(threadnum);
			MutationRunPool &inuse_pool = mutrun_context.in_use_pool_;
			size_t inuse_pool_count = inuse_pool.size();
			
			for (size_t pool_index = 0; pool_index < inuse_pool_count; ++pool_index)
			{
				const MutationRun *mutrun = inuse_pool[pool_index];
				
				if (mutrun->use_count_CHECK_ != mutrun->use_count())
					EIDOS_TERMINATION << "ERROR (Population::TallyMutationRunReferencesForPopulationForChromosome): (internal error) use_count_CHECK_ " << mutrun->use_count_CHECK_ << " != mutrun->use_count() " << mutrun->use_count() << "." << EidosTerminate();
			}
		}
	}
#endif
	
	// if you want to then free the mutation runs that are unused, call FreeUnusedMutationRuns()
	
	p_chromosome->tallied_haplosome_count_ = tallied_haplosome_count;
}

void Population::TallyMutationRunReferencesForPopulation(bool p_clock_for_mutrun_experiments)
{
	// Each chromosome is tallied separately, in the present design; this allows parallelization to work differently for each
	if (p_clock_for_mutrun_experiments)
	{
		for (Chromosome *chromosome : species_.Chromosomes())
		{
			chromosome->StartMutationRunExperimentClock();
			
			TallyMutationRunReferencesForPopulationForChromosome(chromosome);
			
			chromosome->StopMutationRunExperimentClock("TallyMutationRunReferencesForPopulation()");
		}
	}
	else
	{
		for (Chromosome *chromosome : species_.Chromosomes())
			TallyMutationRunReferencesForPopulationForChromosome(chromosome);
	}
}

void Population::TallyMutationRunReferencesForSubpopsForChromosome(std::vector<Subpopulation*> *p_subpops_to_tally, Chromosome *p_chromosome)
{
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::TallyMutationRunReferencesForSubpops): (internal error) called with child generation active!" << EidosTerminate();
	
	slim_refcount_t tallied_haplosome_count = 0;
	int first_haplosome_index = species_.FirstHaplosomeIndices()[p_chromosome->Index()];
	int last_haplosome_index = species_.LastHaplosomeIndices()[p_chromosome->Index()];
	int mutrun_count_multiplier = p_chromosome->mutrun_count_multiplier_;
	int mutrun_context_count = p_chromosome->ChromosomeMutationRunContextCount();
	
	if (mutrun_count_multiplier * mutrun_context_count != p_chromosome->mutrun_count_)
		EIDOS_TERMINATION << "ERROR (Population::TallyMutationRunReferencesForSubpops): (internal error) mutation run subdivision is incorrect." << EidosTerminate();
	
	// THIS PARALLEL REGION CANNOT HAVE AN IF()!  IT MUST ALWAYS EXECUTE PARALLEL!
	// the reduction() is a bit odd - every thread will generate the same value, and we just want that value,
	// but lastprivate() is not legal for parallel regions, for some reason, so we use reduction(max)
#pragma omp parallel default(none) shared(mutrun_count_multiplier, mutrun_context_count, std::cerr, p_subpops_to_tally) reduction(max: total_haplosome_count) num_threads(mutrun_context_count)
	{
		// this is initialized by OpenMP to the most negative number (reduction type max); we want zero instead!
		tallied_haplosome_count = 0;
		
#ifdef _OPENMP
		// it is imperative that we run with the requested number of threads
		if (omp_get_num_threads() != mutrun_context_count)
		{
			std::cerr << "requested  " << mutrun_context_count << " threads but got " << omp_get_num_threads() << std::endl;
			THREAD_SAFETY_IN_ANY_PARALLEL("Population::TallyMutationRunReferencesForSubpops(): incorrect thread count!");
		}
#endif
		
		// first, zero all use counts across all in-use MutationRun objects
		// each thread does its own zeroing, for its own MutationRunContext
		{
			MutationRunContext &mutrun_context = p_chromosome->ChromosomeMutationRunContextForThread(omp_get_thread_num());
			
			for (const MutationRun *mutrun : mutrun_context.in_use_pool_)
				mutrun->zero_use_count();
		}
		
		// second, loop through all haplosomes in all subpops and tally the usage of their MutationRun objects
		// each thread handles only the range of mutation run indices that it is responsible for
		int first_mutrun_index = omp_get_thread_num() * mutrun_count_multiplier;
		int last_mutrun_index = first_mutrun_index + mutrun_count_multiplier - 1;
		
		// note this is NOT an OpenMP parallel for loop!  each encountering thread runs every iteration!
		for (Subpopulation *subpop : *p_subpops_to_tally)
		{
			if (subpop->CouldContainNullHaplosomes())
			{
				for (Individual *ind : subpop->parent_individuals_)
				{
					Haplosome **haplosomes = ind->haplosomes_;
					
					for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
					{
						Haplosome *haplosome = haplosomes[haplosome_index];
						
						if (!haplosome->IsNull())
						{
							for (int run_index = first_mutrun_index; run_index <= last_mutrun_index; ++run_index)
								haplosome->mutruns_[run_index]->increment_use_count();
							
							tallied_haplosome_count++;
						}
					}
				}
			}
			else
			{
				// optimized case when null haplosomes do not exist in this subpop
				for (Individual *ind : subpop->parent_individuals_)
				{
					Haplosome **haplosomes = ind->haplosomes_;
					
					for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
					{
						Haplosome *haplosome = haplosomes[haplosome_index];
						
						for (int run_index = first_mutrun_index; run_index <= last_mutrun_index; ++run_index)
							haplosome->mutruns_[run_index]->increment_use_count();
					}
				}
				
				tallied_haplosome_count += subpop->parent_individuals_.size() * (last_haplosome_index - first_haplosome_index + 1);
			}
		}
	}
	
#if DEBUG
	// In debug builds we do a complete re-tally single-threaded, into a side counter, for a check-back
	// The code for this is very similar to the code above, but it at least checks that optimizations above are correct...
	{
		slim_refcount_t tallied_haplosome_count_CHECK = 0;
		
		for (int threadnum = 0; threadnum < p_chromosome->ChromosomeMutationRunContextCount(); ++threadnum)
		{
			MutationRunContext &mutrun_context = p_chromosome->ChromosomeMutationRunContextForThread(threadnum);
			MutationRunPool &inuse_pool = mutrun_context.in_use_pool_;
			size_t inuse_pool_count = inuse_pool.size();
			
			for (size_t pool_index = 0; pool_index < inuse_pool_count; ++pool_index)
				inuse_pool[pool_index]->use_count_CHECK_ = 0;
		}
		
		for (const Subpopulation *subpop : *p_subpops_to_tally)
		{
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
					{
						int mutrun_count = haplosome->mutrun_count_;
						
						for (int run_index = 0; run_index < mutrun_count; ++run_index)
							haplosome->mutruns_[run_index]->use_count_CHECK_++;
						
						tallied_haplosome_count_CHECK++;
					}
				}
			}
		}
		
		if (tallied_haplosome_count_CHECK != tallied_haplosome_count)
			EIDOS_TERMINATION << "ERROR (Population::TallyMutationRunReferencesForSubpopsForChromosome): (internal error) tallied_haplosome_count_CHECK != tallied_haplosome_count (" << tallied_haplosome_count_CHECK << " != " << tallied_haplosome_count << ")." << EidosTerminate();
		
		for (int threadnum = 0; threadnum < p_chromosome->ChromosomeMutationRunContextCount(); ++threadnum)
		{
			MutationRunContext &mutrun_context = p_chromosome->ChromosomeMutationRunContextForThread(threadnum);
			MutationRunPool &inuse_pool = mutrun_context.in_use_pool_;
			size_t inuse_pool_count = inuse_pool.size();
			
			for (size_t pool_index = 0; pool_index < inuse_pool_count; ++pool_index)
			{
				const MutationRun *mutrun = inuse_pool[pool_index];
				
				if (mutrun->use_count_CHECK_ != mutrun->use_count())
					EIDOS_TERMINATION << "ERROR (Population::TallyMutationRunReferencesForSubpopsForChromosome): (internal error) use_count_CHECK_ " << mutrun->use_count_CHECK_ << " != mutrun->use_count() " << mutrun->use_count() << "." << EidosTerminate();
			}
		}
	}
#endif
	
	p_chromosome->tallied_haplosome_count_ = tallied_haplosome_count;
}

void Population::TallyMutationRunReferencesForSubpops(std::vector<Subpopulation*> *p_subpops_to_tally)
{
	// Each chromosome is tallied separately, in the present design; this allows parallelization to work differently for each
	for (Chromosome *chromosome : species_.Chromosomes())
		TallyMutationRunReferencesForSubpopsForChromosome(p_subpops_to_tally, chromosome);
}

void Population::TallyMutationRunReferencesForHaplosomes(const Haplosome * const *haplosomes_ptr, slim_popsize_t haplosomes_count)
{
	// FIXME parallelize this; unclear how, since the haplosomes might be scattered across chromosomes
	// could perhaps check for the haplosomes all belonging to one chromosome, and parallelize that
	// specific case, which is obvious, at least; probably that's the common case.
		
	// first, zero all chromosome tallies and all use counts across all in-use MutationRun objects
	for (Chromosome *chromosome : species_.Chromosomes())
	{
		chromosome->tallied_haplosome_count_ = 0;
		
		int mutrun_context_count = chromosome->ChromosomeMutationRunContextCount();
		
		for (int mutrun_context_index = 0; mutrun_context_index < mutrun_context_count; ++mutrun_context_index)
		{
			MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(mutrun_context_index);
			
			for (const MutationRun *mutrun : mutrun_context.in_use_pool_)
				mutrun->zero_use_count();
		}
	}
	
	// second, loop through all haplosomes in all subpops and tally the usage of their MutationRun objects
	for (slim_popsize_t haplosome_index = 0; haplosome_index < haplosomes_count; ++haplosome_index)
	{
		const Haplosome *haplosome = haplosomes_ptr[haplosome_index];
		
		if (!haplosome->IsNull())
		{
			for (int run_index = 0; run_index < haplosome->mutrun_count_; ++run_index)
				haplosome->mutruns_[run_index]->increment_use_count();
			
			Chromosome *chromosome = species_.Chromosomes()[haplosome->chromosome_index_];
			chromosome->tallied_haplosome_count_++;
		}
	}
}

void Population::FreeUnusedMutationRuns(void)
{
	// It is assumed by this method that mutation run tallies are up to date!
	// The caller must ensure that by calling TallyMutationRunReferencesForPopulation()!
	
#if DEBUG
	// Check for usage of each mutation run we intend to free, to catch bugs in that area.
	// This is useful for debugging problems with mutation run freeing, such as the error
	// message "a mutation run was used at more than one position", which is a symptom
	// of a mutation being freed while it is still in use.
	
	// first set the use_count_CHECK_ of all mutation runs to zero
	for (Chromosome *chromosome : species_.Chromosomes())
	{
		int mutrun_context_count = chromosome->ChromosomeMutationRunContextCount();
		
		for (int mutrun_context_index = 0; mutrun_context_index < mutrun_context_count; ++mutrun_context_index)
		{
			MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(mutrun_context_index);
			
			for (const MutationRun *mutrun : mutrun_context.in_use_pool_)
				mutrun->use_count_CHECK_ = 0;
		}
	}
	
	// then go through all haplosomes of all individuals of all subpops, and increment their use count
	int haplosome_count_per_individual = species_.HaplosomeCountPerIndividual();
	
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
	{
		Subpopulation *subpop = subpop_pair.second;
		{
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
					{
						int mutrun_count = haplosome->mutrun_count_;
						
						for (int run_index = 0; run_index < mutrun_count; ++run_index)
							haplosome->mutruns_[run_index]->use_count_CHECK_++;
					}
				}
			}
		}
	}
	
	// then check for mutruns with a use count of 0 (slated to be freed) but a non-zero check count (still in use)
	for (Chromosome *chromosome : species_.Chromosomes())
	{
		int mutrun_context_count = chromosome->ChromosomeMutationRunContextCount();
		
		for (int mutrun_context_index = 0; mutrun_context_index < mutrun_context_count; ++mutrun_context_index)
		{
			MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(mutrun_context_index);
			
			for (const MutationRun *mutrun : mutrun_context.in_use_pool_)
				if ((mutrun->use_count() == 0) && (mutrun->use_count_CHECK_ != 0))
					EIDOS_TERMINATION << "ERROR (Population::FreeUnusedMutationRuns): (internal error) use_count() is zero for mutrun with actual usage count " << mutrun->use_count_CHECK_ << "!" << EidosTerminate();
		}
	}
#endif
	
	for (Chromosome *chromosome : species_.Chromosomes())
	{
		chromosome->StartMutationRunExperimentClock();
		
		// free all in-use MutationRun objects that are not actually in use (use count == 0)
		// each thread does its own checking and freeing, for its own MutationRunContext
#ifdef _OPENMP
		int mutrun_context_count = chromosome->ChromosomeMutationRunContextCount();
#endif
		
#pragma omp parallel default(none) num_threads(mutrun_context_count)
		{
			MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(omp_get_thread_num());
			MutationRunPool &inuse_pool = mutrun_context.in_use_pool_;
			size_t pool_count = inuse_pool.size();
			
			for (size_t pool_index = 0; pool_index < pool_count; )
			{
				const MutationRun *mutrun = inuse_pool[pool_index];
				
				if (mutrun->use_count() == 0)
				{
					// First we remove the mutation run from the inuse pool by backfilling
					inuse_pool[pool_index] = inuse_pool.back();
					inuse_pool.pop_back();
					
					// Because we backfilled, we want to stay at this index, but the pool is one smaller
					// This is why we remove the run ourselves, instead of FreeMutationRun() doing it
					--pool_count;
					
					// Then we give the mutation run back to the free pool
					MutationRun::FreeMutationRun(mutrun, mutrun_context);
				}
				else
				{
					++pool_index;
				}
			}
		}
		
		chromosome->StopMutationRunExperimentClock("FreeUnusedMutationRuns()");
	}
}

// count the number of non-null haplosomes in the population
slim_refcount_t Population::_CountNonNullHaplosomesForChromosome(Chromosome *p_chromosome)
{
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::_CountNonNullHaplosomesForChromosome): (internal error) called with child generation active!" << EidosTerminate();
	
	slim_chromosome_index_t chromosome_index = p_chromosome->Index();
	int first_haplosome_index = species_.FirstHaplosomeIndices()[chromosome_index];
	int last_haplosome_index = species_.LastHaplosomeIndices()[chromosome_index];
	slim_refcount_t total_haplosome_count = 0;
	
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
	{
		Subpopulation *subpop = subpop_pair.second;
		
		if (subpop->CouldContainNullHaplosomes())
		{
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
						total_haplosome_count++;
				}
			}
		}
		else
		{
			// optimized case when null haplosomes do not exist in this subpop
			total_haplosome_count += subpop->parent_individuals_.size() * (last_haplosome_index - first_haplosome_index + 1);
		}
	}
	
	return total_haplosome_count;
}

void Population::InvalidateMutationReferencesCache(void)
{
	last_tallied_subpops_.resize(0);
	cached_tallies_valid_ = false;
}

// count the total number of times that each Mutation in the registry is referenced by the whole population
void Population::TallyMutationReferencesAcrossPopulation(bool p_clock_for_mutrun_experiments)
{
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::TallyMutationReferencesAcrossPopulation): (internal error) called with child generation active!" << EidosTerminate();
	
	// Figure out whether the last tally was of the same thing, such that we can skip the work
	// For this code path (across population), last_tallied_subpops_ must be zero to hit the cache
	if (cached_tallies_valid_ && (last_tallied_subpops_.size() == 0))
	{
		// we hit the cache; we just return so the previously computed result is reused
		
#if DEBUG
#if DEBUG_LESS_INTENSIVE
		// These tests are extremely intensive, so sometimes it's useful to dial them down...
		if ((community_.Tick() % 97) != 5)
			return;
#endif
		
		// check that the cached haplosome count is correct; note that it includes only non-null haplosomes
		for (Chromosome *chromosome : species_.Chromosomes())
		{
			slim_refcount_t tallied_haplosome_count = _CountNonNullHaplosomesForChromosome(chromosome);
			
			if (tallied_haplosome_count != chromosome->tallied_haplosome_count_)
				EIDOS_TERMINATION << "ERROR (Population::TallyMutationReferencesAcrossPopulation): (internal error) cached case hit incorrectly; tallied_haplosome_count_ is not correct." << EidosTerminate();
		}
		
		// in DEBUG mode, do the complete check below as well; it should match, if the cache is valid
		goto doDebugCheck;
#endif
		
		return;
	}
	
	// Tally mutation run usage first, and then leverage that to tally mutations
	// Note this sets up tallied_haplosome_count_ for all chromosomes
	TallyMutationRunReferencesForPopulation(p_clock_for_mutrun_experiments);
	
	// Give the core work to our fast worker method; this zeroes and then tallies
	_TallyMutationReferences_FAST_FromMutationRunUsage(p_clock_for_mutrun_experiments);
	
#if DEBUG
doDebugCheck:
	{
		int haplosome_count_per_individual = species_.HaplosomeCountPerIndividual();
		std::vector<Haplosome *> haplosomes;
		
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
		{
			Subpopulation *subpop = subpop_pair.second;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **ind_haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				{
					Haplosome *haplosome = ind_haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
						haplosomes.push_back(haplosome);
				}
			}
		}
		
		_CheckMutationTallyAcrossHaplosomes(haplosomes.data(), (slim_popsize_t)haplosomes.size(), "Population::TallyMutationReferencesAcrossPopulation()");
	}
#endif
	
	// set up the cache info
	last_tallied_subpops_.resize(0);
	cached_tallies_valid_ = true;
	
	// When tallying the full population, we update total_haplosome_count_ as well, since we did the work
	for (Chromosome *chromosome : species_.Chromosomes())
		chromosome->total_haplosome_count_ = chromosome->tallied_haplosome_count_;
}

#ifdef SLIMGUI
// this tallies separately for SLiMgui, into private counters, across the selected subpopulations only
// we pay a performance price for this in SLiMgui in the cases when the main tally is up to date and
// would be accurate, but there have been too many bugs with trying to do both at the same time; if
// there is going to be any smart caching behavior for SLiMgui, it needs to be in this code path only
void Population::TallyMutationReferencesAcrossPopulation_SLiMgui(void)
{
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::TallyMutationReferencesAcrossPopulation_SLiMgui): (internal error) called with child generation active!" << EidosTerminate();
	
	// We're in SLiMgui, so we need to figure out how we're going to handle its refcounts, which are
	// separate from slim's since the user can select just a subset of subpopulations.
	bool slimgui_subpop_all_selected = true;
	
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
		if (!subpop_pair.second->gui_selected_)
			slimgui_subpop_all_selected = false;
	
	if (slimgui_subpop_all_selected)
	{
		// All subpopulations are selected in SLiMgui, so we can tally using MutationRun across
		// the whole population.  A tally from TallyMutationReferencesAcrossPopulation() will 
		// thus be valid, and might even hit its cache.  We can subcontract to it to do the work.
		TallyMutationReferencesAcrossPopulation(/* p_clock_for_mutrun_experiments */ false);
	}
	else
	{
		// A subset of subpops are selected in SLiMgui, so we can tally using MutationRun across
		// those subpops with TallyMutationReferencesAcrossSubpopulations().
		std::vector<Subpopulation *> subpops_to_tally;
		
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
			if (subpop_pair.second->gui_selected_)
				subpops_to_tally.push_back(subpop_pair.second);
		
		TallyMutationReferencesAcrossSubpopulations(&subpops_to_tally);
	}
	
	// Then copy the tallied refcounts into our private refcounts
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
	int registry_size;
	const MutationIndex *registry_iter = MutationRegistry(&registry_size);
	const MutationIndex *registry_iter_end = registry_iter + registry_size;
	
	while (registry_iter != registry_iter_end)
	{
		MutationIndex mut_index = *registry_iter;
		slim_refcount_t *refcount_ptr = refcount_block_ptr + mut_index;
		const Mutation *mutation = mut_block_ptr + mut_index;
		
		mutation->gui_reference_count_ = *refcount_ptr;
		registry_iter++;
	}
	
	// And update the SLiMgui total haplosome counts from the tally as well
	for (Chromosome *chromosome : species_.Chromosomes())
		chromosome->gui_total_haplosome_count_ = chromosome->tallied_haplosome_count_;
	
	// There seems to be no need for a separate DEBUG check here, because the calls above to
	// TallyMutationReferencesAcrossPopulation() / TallyMutationReferencesAcrossSubpopulations()
	// already have their own DEBUG check code; we just copy the work they did for us.
}
#endif

void Population::TallyMutationReferencesAcrossSubpopulations(std::vector<Subpopulation*> *p_subpops_to_tally)
{
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::TallyMutationReferencesAcrossSubpopulations): (internal error) called with child generation active!" << EidosTerminate();
	
	// When tallying just a subset of the subpops, we don't update total_haplosome_count_,
	// which applies only to population-wide tallies; but we do set tallied_haplosome_count_
	
	int haplosome_count_per_individual = species_.HaplosomeCountPerIndividual();
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
	
	// We have two ways of tallying; here we decide which way to use.  We only loop through haplosomes
	// if we are tallying for a single subpopulation and it is small; otherwise, looping through
	// mutation runs is expected to be faster.  Tallying with mutruns would still work, just slower.
	bool tally_using_mutruns = true;

	if (p_subpops_to_tally->size() == 0)			// NOLINTNEXTLINE(*-branch-clone) : intentional branch clones
		tally_using_mutruns = false;
	else if ((p_subpops_to_tally->size() == 1) && ((*p_subpops_to_tally)[0]->parent_individuals_.size() <= 5))
		tally_using_mutruns = false;
	
	// Figure out whether the last tally was of the same thing, such that we can skip the work
	if (cached_tallies_valid_ && last_tallied_subpops_.size() && (last_tallied_subpops_ == *p_subpops_to_tally))
	{
		// we hit the cache; we just return so the previously computed result is reused
		
#if DEBUG
		// in DEBUG mode, do the complete check below as well; it should match, if the cache is valid
		goto doDebugCheck;
#endif
		
		return;
	}
	
	if (tally_using_mutruns)
	{
		// FAST PATH: Tally mutation run usage first, and then leverage that to tally mutations
		// Note that this call sets up tallied_haplosome_count_ for all chromosomes
		TallyMutationRunReferencesForSubpops(p_subpops_to_tally);
		
		// Give the core work to our fast worker method; this zeroes and then tallies
		_TallyMutationReferences_FAST_FromMutationRunUsage(/* p_clock_for_mutrun_experiments */ false);
	}
	else
	{
		// SLOW PATH: Increment the refcounts through all pointers to Mutation in all haplosomes
		SLiM_ZeroRefcountBlock(mutation_registry_, /* p_registry_only */ community_.AllSpecies().size() > 1);
		
		for (Chromosome *chromosome : species_.Chromosomes())
			chromosome->tallied_haplosome_count_ = 0;
		
		for (Subpopulation *subpop : *p_subpops_to_tally)
		{
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **ind_haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				{
					Haplosome *haplosome = ind_haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
					{
						Chromosome *chromosome = species_.ChromosomesForHaplosomeIndices()[haplosome_index];
						int mutrun_count = haplosome->mutrun_count_;
						
						for (int run_index = 0; run_index < mutrun_count; ++run_index)
						{
							const MutationRun *mutrun = haplosome->mutruns_[run_index];
							const MutationIndex *haplosome_iter = mutrun->begin_pointer_const();
							const MutationIndex *haplosome_end_iter = mutrun->end_pointer_const();
							
							for (; haplosome_iter != haplosome_end_iter; ++haplosome_iter)
								++(*(refcount_block_ptr + *haplosome_iter));
						}
						
						chromosome->tallied_haplosome_count_++;	// count only non-null haplosomes to determine fixation
					}
				}
			}
		}
	}
	
#if DEBUG
doDebugCheck:
	{
		std::vector<Haplosome *> haplosomes;
		
		for (Subpopulation *subpop : *p_subpops_to_tally)
		{
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **ind_haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				{
					Haplosome *haplosome = ind_haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
						haplosomes.push_back(haplosome);
				}
			}
		}
		
		_CheckMutationTallyAcrossHaplosomes(haplosomes.data(), (slim_popsize_t)haplosomes.size(), "Population::TallyMutationReferencesAcrossSubpopulations()");
	}
#endif
	
	// set up the cache info
	last_tallied_subpops_ = *p_subpops_to_tally;
	cached_tallies_valid_ = true;
}

void Population::TallyMutationReferencesAcrossHaplosomes(const Haplosome * const *haplosomes_ptr, slim_popsize_t haplosomes_count)
{
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
	
	// We have two ways of tallying; here we decide which way to use.  We tally directly by
	// looping through haplosomes below a certain problem threshold, because there is some
	// overhead to tallying the mutation runs; it's not worth it for small problems.  The
	// threshold is a complete guess; in reality it will depend upon how much mutation run
	// sharing is present, how many mutations per run there are, and many other factors.
	// I put the threshold pretty low because if you do mutruns and you're wrong, you just
	// pay a small fixed overhead, but if you do haplosomes and you're wrong, it can hurt a lot.
	bool can_tally_using_mutruns = true;
	
	if (haplosomes_count <= 10)
		can_tally_using_mutruns = false;
	
	if (can_tally_using_mutruns)
	{
		// FAST PATH: Tally mutation run usage first, and then leverage that to tally mutations
		// Note that this call sets up tallied_haplosome_count_ for all chromosomes
		TallyMutationRunReferencesForHaplosomes(haplosomes_ptr, haplosomes_count);
		
		// Give the core work to our fast worker method; this zeroes and then tallies
		_TallyMutationReferences_FAST_FromMutationRunUsage(/* p_clock_for_mutrun_experiments */ false);
	}
	else
	{
		// SLOW PATH: Increment the refcounts through all pointers to Mutation in all haplosomes
		SLiM_ZeroRefcountBlock(mutation_registry_, /* p_registry_only */ community_.AllSpecies().size() > 1);
		
		for (Chromosome *chromosome : species_.Chromosomes())
			chromosome->tallied_haplosome_count_ = 0;
		
		for (slim_popsize_t i = 0; i < haplosomes_count; i++)
		{
			const Haplosome *haplosome = haplosomes_ptr[i];
			
			if (!haplosome->IsNull())
			{
				Chromosome *chromosome = species_.Chromosomes()[haplosome->chromosome_index_];
				int mutrun_count = haplosome->mutrun_count_;
				
				for (int run_index = 0; run_index < mutrun_count; ++run_index)
				{
					const MutationRun *mutrun = haplosome->mutruns_[run_index];
					const MutationIndex *haplosome_iter = mutrun->begin_pointer_const();
					const MutationIndex *haplosome_end_iter = mutrun->end_pointer_const();
					
					for (; haplosome_iter != haplosome_end_iter; ++haplosome_iter)
						++(*(refcount_block_ptr + *haplosome_iter));
				}
				
				chromosome->tallied_haplosome_count_++;	// count only non-null haplosomes to determine fixation
			}
		}
	}
	
#if DEBUG
	_CheckMutationTallyAcrossHaplosomes(haplosomes_ptr, haplosomes_count, "Population::TallyMutationReferencesAcrossHaplosomes()");
#endif
	
	// We have messed up any cached tallies, so mark the cache as invalid
	InvalidateMutationReferencesCache();
}

// This internal method tallies for all mutations across all mutation runs.  It does not do
// the mutation run tallying itself, however; instead, the caller can tally mutation runs
// across whatever set of subpops/haplosomes they wish, and then this method will provide
// mutation tallies given that choice.
void Population::_TallyMutationReferences_FAST_FromMutationRunUsage(bool p_clock_for_mutrun_experiments)
{
	// first zero out the refcounts in all registered Mutation objects
	SLiM_ZeroRefcountBlock(mutation_registry_, /* p_registry_only */ community_.AllSpecies().size() > 1);
	
	for (Chromosome *chromosome : species_.Chromosomes())
	{
		if (p_clock_for_mutrun_experiments)
			chromosome->StartMutationRunExperimentClock();
		
		// each thread does its own tallying, for its own MutationRunContext
#ifdef _OPENMP
		int mutrun_context_count = chromosome->ChromosomeMutationRunContextCount();
#endif
		
#pragma omp parallel default(none) shared(gSLiM_Mutation_Refcounts) num_threads(mutrun_context_count)
		{
			MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(omp_get_thread_num());
			MutationRunPool &inuse_pool = mutrun_context.in_use_pool_;
			size_t inuse_pool_count = inuse_pool.size();
			slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
			
			for (size_t pool_index = 0; pool_index < inuse_pool_count; ++pool_index)
			{
				const MutationRun *mutrun = inuse_pool[pool_index];
				const slim_refcount_t use_count = (slim_refcount_t)mutrun->use_count();
				
				// Note that no locking or atomicity is needed here at all!  This is because
				// each thread is responsible for particular positions along the haplosome;
				// no other thread will be accessing this tally at the same time as us!
				// FIXME this is probably rife with false sharing, however; it would be useful
				// to put the refcounts for different mutations into different memory blocks
				// according to the thread that manages each mutation.
				
				const MutationIndex *mutrun_iter = mutrun->begin_pointer_const();
				const MutationIndex *mutrun_end_iter = mutrun->end_pointer_const();
				
				// I've gone back and forth on unrolling this loop.  This ought to be done
				// by the compiler, and the best unrolling strategy depends on the platform.
				// But the compiler doesn't seem to do it, for my macOS system at least, or
				// doesn't do it well; this increases speed by ~5% here.  I'm not sure if
				// clang is being dumb, or what, but it seems worthwhile.
				while (mutrun_iter + 16 < mutrun_end_iter)
				{
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
				}
				
				while (mutrun_iter != mutrun_end_iter)
					*(refcount_block_ptr + (*mutrun_iter++)) += use_count;
			}
		}
		
		if (p_clock_for_mutrun_experiments)
			chromosome->StopMutationRunExperimentClock("_TallyMutationReferences_FAST_FromMutationRunUsage()");
	}
}

#if DEBUG
void Population::_CheckMutationTallyAcrossHaplosomes(const Haplosome * const *haplosomes_ptr, slim_popsize_t haplosomes_count, std::string caller_name)
{
	// This does a DEBUG check on the results of mutation reference tallying, done in several spots.
	// It should be called immediately after tallying, and passed a vector of the haplosomes tallied across.
	int registry_count;
	const MutationIndex *registry_iter = MutationRegistry(&registry_count);
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	// zero out all check refcounts
	for (int registry_index = 0; registry_index < registry_count; ++registry_index)
	{
		const Mutation *mut = mut_block_ptr + registry_iter[registry_index];
		mut->refcount_CHECK_ = 0;
	}
	
	// simply loop through all mutruns of all haplosomes given, and increment check refcounts
	for (slim_popsize_t haplosome_index = 0; haplosome_index < haplosomes_count; ++haplosome_index)
	{
		const Haplosome *haplosome = haplosomes_ptr[haplosome_index];
		int mutrun_count = haplosome->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			const MutationRun *mutrun = haplosome->mutruns_[run_index];
			const MutationIndex *mutrun_iter = mutrun->begin_pointer_const();
			const MutationIndex *mutrun_end_iter = mutrun->end_pointer_const();
			
			for (; mutrun_iter != mutrun_end_iter; ++mutrun_iter)
				(mut_block_ptr + *mutrun_iter)->refcount_CHECK_++;
		}
	}
	
	// then loop through the registry and check that all check refcounts match tallied refcounts
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
	
	for (int registry_index = 0; registry_index < registry_count; ++registry_index)
	{
		MutationIndex mut_blockindex = registry_iter[registry_index];
		const Mutation *mut = mut_block_ptr + mut_blockindex;
		
		if (mut->state_ == MutationState::kInRegistry)
		{
			slim_refcount_t refcount_standard = *(refcount_block_ptr + mut_blockindex);
			slim_refcount_t refcount_checkback = mut->refcount_CHECK_;
			
			if (refcount_standard != refcount_checkback)
				EIDOS_TERMINATION << "ERROR (Population::_CheckMutationTallyAcrossHaplosomes): (internal error) mutation refcount " << refcount_standard << " != checkback " << refcount_checkback << " in " << caller_name << "." << EidosTerminate();
		}
	}
}
#endif

EidosValue_SP Population::Eidos_FrequenciesForTalliedMutations(EidosValue *mutations_value)
{
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
	EidosValue_SP result_SP;
	
	// Fetch tallied haplosome counts for all chromosomes up front; these will be set up beforehand
	const std::vector<Chromosome *> &chromosomes = species_.Chromosomes();
	static std::vector<double> tallied_haplosome_counts;		// static to avoid alloc/dealloc
	
	tallied_haplosome_counts.resize(0);
	tallied_haplosome_counts.reserve(chromosomes.size());
	
	for (Chromosome *chromosome : chromosomes)
		tallied_haplosome_counts.push_back(chromosome->tallied_haplosome_count_);
	
	// BCH 10/3/2020: Note that we now have to worry about being asked for the frequency of mutations that are
	// not in the registry, and might be fixed or lost.  We handle this in the first major case below, where
	// a vector of mutations was given.  It could be a marginal issue in the second major case, where NULL was
	// passed for the mutation vector, because the registry can temporarily contain mutations in the state
	// MutationState::kRemovedWithSubstitution, immediately after removeMutations(substitute=T); if that is
	// a potential issue, registry_needs_consistency_check_ will be true, and we treat it specially.
	
	if (mutations_value->Type() != EidosValueType::kValueNULL)
	{
		// a vector of mutations was given, so loop through them and take their tallies
		int mutations_count = mutations_value->Count();
		EidosObject * const *mutations_data = mutations_value->ObjectData();
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(mutations_count);
		result_SP = EidosValue_SP(float_result);
		
		for (int value_index = 0; value_index < mutations_count; ++value_index)
		{
			Mutation *mut = (Mutation *)mutations_data[value_index];
			int8_t mut_state = mut->state_;
			double freq;
			
			if (mut_state == MutationState::kInRegistry)			freq = *(refcount_block_ptr + mut->BlockIndex()) / tallied_haplosome_counts[mut->chromosome_index_];
			else if (mut_state == MutationState::kLostAndRemoved)	freq = 0.0;
			else													freq = 1.0;
			
			float_result->set_float_no_check(freq, value_index);
		}
	}
	else if (MutationRegistryNeedsCheck())
	{
		// no mutation vector was given, so return all frequencies from the registry
		// this is the same as the case below, except MutationState::kRemovedWithSubstitution is possible
		int registry_size;
		const MutationIndex *registry = MutationRegistry(&registry_size);
		Mutation *mutation_block_ptr = gSLiM_Mutation_Block;
		
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(registry_size);
		result_SP = EidosValue_SP(float_result);
		
		for (int registry_index = 0; registry_index < registry_size; registry_index++)
		{
			MutationIndex mut_index = registry[registry_index];
			const Mutation *mut = mutation_block_ptr + mut_index;
			double freq;
			
			if (mut->state_ == MutationState::kInRegistry)			freq = *(refcount_block_ptr + mut_index) / tallied_haplosome_counts[mut->chromosome_index_];
			else /* MutationState::kRemovedWithSubstitution */		freq = 1.0;
			
			float_result->set_float_no_check(freq, registry_index);
		}
	}
	else
	{
		// no mutation vector was given, so return all frequencies from the registry
		int registry_size;
		const MutationIndex *registry = MutationRegistry(&registry_size);
		Mutation *mutation_block_ptr = gSLiM_Mutation_Block;
		
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->resize_no_initialize(registry_size);
		result_SP = EidosValue_SP(float_result);
		
		for (int registry_index = 0; registry_index < registry_size; registry_index++)
		{
			MutationIndex mut_index = registry[registry_index];
			const Mutation *mut = mutation_block_ptr + mut_index;
			float_result->set_float_no_check(*(refcount_block_ptr + registry[registry_index]) / tallied_haplosome_counts[mut->chromosome_index_], registry_index);
		}
	}
	
	return result_SP;
}

EidosValue_SP Population::Eidos_CountsForTalliedMutations(EidosValue *mutations_value)
{
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
	EidosValue_SP result_SP;
	
	// Fetch total haplosome counts for all chromosomes up front; these will be set up beforehand
	const std::vector<Chromosome *> &chromosomes = species_.Chromosomes();
	static std::vector<slim_refcount_t> tallied_haplosome_counts;		// static to avoid alloc/dealloc
	
	tallied_haplosome_counts.resize(0);
	tallied_haplosome_counts.reserve(chromosomes.size());
	
	for (Chromosome *chromosome : chromosomes)
		tallied_haplosome_counts.push_back(chromosome->tallied_haplosome_count_);
	
	// BCH 10/3/2020: Note that we now have to worry about being asked for the frequency of mutations that are
	// not in the registry, and might be fixed or lost.  We handle this in the first major case below, where
	// a vector of mutations was given.  It could be a marginal issue in the second major case, where NULL was
	// passed for the mutation vector, because the registry can temporarily contain mutations in the state
	// MutationState::kRemovedWithSubstitution, immediately after removeMutations(substitute=T); if that is
	// a potential issue, registry_needs_consistency_check_ will be true, and we treat it specially.
	
	if (mutations_value->Type() != EidosValueType::kValueNULL)
	{
		// a vector of mutations was given, so loop through them and take their tallies
		int mutations_count = mutations_value->Count();
		EidosObject * const *mutations_data = mutations_value->ObjectData();
		EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(mutations_count);
		result_SP = EidosValue_SP(int_result);
		
		for (int value_index = 0; value_index < mutations_count; ++value_index)
		{
			Mutation *mut = (Mutation *)mutations_data[value_index];
			int8_t mut_state = mut->state_;
			slim_refcount_t count;
			
			if (mut_state == MutationState::kInRegistry)			count = *(refcount_block_ptr + mut->BlockIndex());
			else if (mut_state == MutationState::kLostAndRemoved)	count = 0;
			else													count = tallied_haplosome_counts[mut->chromosome_index_];
			
			int_result->set_int_no_check(count, value_index);
		}
	}
	else if (MutationRegistryNeedsCheck())
	{
		// no mutation vector was given, so return all frequencies from the registry
		// this is the same as the case below, except MutationState::kRemovedWithSubstitution is possible
		int registry_size;
		const MutationIndex *registry = MutationRegistry(&registry_size);
		Mutation *mutation_block_ptr = gSLiM_Mutation_Block;
		
		EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(registry_size);
		result_SP = EidosValue_SP(int_result);
		
		for (int registry_index = 0; registry_index < registry_size; registry_index++)
		{
			MutationIndex mut_index = registry[registry_index];
			const Mutation *mut = mutation_block_ptr + mut_index;
			slim_refcount_t count;
			
			if (mut->state_ == MutationState::kInRegistry)		count = *(refcount_block_ptr + mut_index);
			else /* MutationState::kRemovedWithSubstitution */	count = tallied_haplosome_counts[mut->chromosome_index_];
			
			int_result->set_int_no_check(count, registry_index);
		}
	}
	else
	{
		// no mutation vector was given, so return all frequencies from the registry
		int registry_size;
		const MutationIndex *registry = MutationRegistry(&registry_size);
		
		EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(registry_size);
		result_SP = EidosValue_SP(int_result);
		
		for (int registry_index = 0; registry_index < registry_size; registry_index++)
			int_result->set_int_no_check(*(refcount_block_ptr + registry[registry_index]), registry_index);
	}
	
	return result_SP;
}

// handle negative fixation (remove from the registry) and positive fixation (convert to Substitution), using existing mutation reference counts
// TallyMutationReferencesAcrossPopulation() must have cached tallies across the whole population before this is called, or it will malfunction!
void Population::RemoveAllFixedMutations(void)
{
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::RemoveAllFixedMutations): (internal error) called with child generation active!" << EidosTerminate();
	
	// We use a stack-local MutationRun object so it gets disposed of properly via RAII; non-optimal
	// from a performance perspective, since it will do reallocs to reach its needed size, but
	// since this method is only called once per cycle it shouldn't matter.
	MutationRun removed_mutation_accumulator;
	
#ifdef SLIMGUI
	int mutation_type_count = static_cast<int>(species_.mutation_types_.size());
#endif
	
	// Fetch total haplosome counts for all chromosomes up front; these will be set up beforehand
	const std::vector<Chromosome *> &chromosomes = species_.Chromosomes();
	static std::vector<slim_refcount_t> total_haplosome_counts;		// static to avoid alloc/dealloc
	
	total_haplosome_counts.resize(0);
	total_haplosome_counts.reserve(chromosomes.size());
	
	for (Chromosome *chromosome : chromosomes)
		total_haplosome_counts.push_back(chromosome->total_haplosome_count_);
	
	// remove Mutation objects that are no longer referenced, freeing them; avoid using an iterator since it would be invalidated
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	{
		int registry_size;
		const MutationIndex *registry = MutationRegistry(&registry_size);
		
		for (int registry_index = 0; registry_index < registry_size; ++registry_index)
		{
			MutationIndex mutation_index = registry[registry_index];
			slim_refcount_t reference_count = *(refcount_block_ptr + mutation_index);
			Mutation *mutation = mut_block_ptr + mutation_index;
			bool remove_mutation = false;
			
			if (reference_count == 0)
			{
				if (mutation->state_ == MutationState::kRemovedWithSubstitution)
				{
					// a substitution object was already created by removeMutations() at the user's request;
					// the refcount is zero because the mutation was removed in script, but it was fixed/substituted
					// this code path is similar to the fixation code path below, but does not create a Substitution
#if DEBUG_MUTATIONS
					std::cout << "Mutation fixed by script, already substituted: " << mutation << std::endl;
#endif
					
#ifdef SLIMGUI
					// If we're running under SLiMgui, make a note of the fixation time of the mutation
					slim_tick_t fixation_time = community_.Tick() - mutation->origin_tick_;
					int mutation_type_index = mutation->mutation_type_ptr_->mutation_type_index_;
					
					AddTallyForMutationTypeAndBinNumber(mutation_type_index, mutation_type_count, fixation_time / 10, &mutation_fixation_times_, &mutation_fixation_tick_slots_);
#endif
					
					// fix the refcount of record; we want user-substituted mutations to have a full refcount, not 0
					// actually, this doesn't work, because the denominator – what total_haplosome_count_ is – depends
					// on what the user asks; so now Species::ExecuteMethod_mutationFreqsCounts() handles this
					// the same issue would have bitten SLiM-substituted mutations if the population size changed
					//*(refcount_block_ptr + mutation_index) = total_haplosome_count_;
					
					mutation->state_ = MutationState::kFixedAndSubstituted;			// marked in anticipation of removal below
					remove_mutation = true;
				}
				else
				{
#if DEBUG_MUTATIONS
					std::cout << "Mutation unreferenced, will remove: " << mutation << std::endl;
#endif
					
#ifdef SLIMGUI
					// If we're running under SLiMgui, make a note of the lifetime of the mutation
					slim_tick_t loss_time = community_.Tick() - mutation->origin_tick_;
					int mutation_type_index = mutation->mutation_type_ptr_->mutation_type_index_;
					
					AddTallyForMutationTypeAndBinNumber(mutation_type_index, mutation_type_count, loss_time / 10, &mutation_loss_times_, &mutation_loss_tick_slots_);
#endif
					
					mutation->state_ = MutationState::kLostAndRemoved;			// marked in anticipation of removal below
					remove_mutation = true;
				}
			}
			else if (reference_count == total_haplosome_counts[mutation->chromosome_index_])
			{
				if (mutation->mutation_type_ptr_->convert_to_substitution_)
				{
#if DEBUG_MUTATIONS
					std::cout << "Mutation fixed, will substitute: " << mutation << std::endl;
#endif
					
#ifdef SLIMGUI
					// If we're running under SLiMgui, make a note of the fixation time of the mutation
					slim_tick_t fixation_time = community_.Tick() - mutation->origin_tick_;
					int mutation_type_index = mutation->mutation_type_ptr_->mutation_type_index_;
					
					AddTallyForMutationTypeAndBinNumber(mutation_type_index, mutation_type_count, fixation_time / 10, &mutation_fixation_times_, &mutation_fixation_tick_slots_);
#endif
					
					// add the fixed mutation to a per-chromosome vector, to be converted to a Substitution object below
					chromosomes[mutation->chromosome_index_]->fixed_mutation_accumulator_.push_back(mutation_index);
					
					mutation->state_ = MutationState::kFixedAndSubstituted;			// marked in anticipation of removal below
					remove_mutation = true;
				}
			}
			
			if (remove_mutation)
			{
				// We have an unreferenced mutation object, so we want to remove it quickly
				if (registry_index == registry_size - 1)
				{
					mutation_registry_.pop_back();
					
					--registry_size;
				}
				else
				{
					MutationIndex last_mutation = mutation_registry_[registry_size - 1];
					mutation_registry_[registry_index] = last_mutation;
					mutation_registry_.pop_back();
					
					--registry_size;
					--registry_index;	// revisit this index
				}
				
				// We can't delete the mutation yet, because we might need to make a Substitution object from it, so add it to a vector for deletion below
				removed_mutation_accumulator.emplace_back(mutation_index);
			}
		}
	}
	
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
	// remove fixed mutations from MutationType registries as well, if we've got any; this is simpler since
	// the main registry is in charge of all bookkeeping, substitution, removal, etc.
	if (keeping_muttype_registries_ && removed_mutation_accumulator.size())
	{
		for (auto muttype_iter : species_.MutationTypes())
		{
			MutationType *muttype = muttype_iter.second;
			
			if (muttype->keeping_muttype_registry_)
			{
				MutationRun &registry = muttype->muttype_registry_;
				int registry_length = registry.size();
				
				for (int i = 0; i < registry_length; ++i)
				{
					MutationIndex mutation_index = registry[i];
					Mutation *mutation = mut_block_ptr + mutation_index;
					
					if ((mutation->state_ == MutationState::kFixedAndSubstituted) || (mutation->state_ == MutationState::kLostAndRemoved))
					{
						// We have an unreferenced mutation object, so we want to remove it quickly
						if (i == registry_length - 1)
						{
							registry.pop_back();
							
							--registry_length;
						}
						else
						{
							MutationIndex last_mutation = registry[registry_length - 1];
							registry[i] = last_mutation;
							registry.pop_back();
							
							--registry_length;
							--i;	// revisit this index
						}
					}
				}
			}
		}
	}
#endif
	
	// replace fixed mutations with Substitution objects, one chromosome at a time
	for (Chromosome *chromosome : chromosomes)
	{
		std::vector<MutationIndex> &fixed_mutation_accumulator = chromosome->fixed_mutation_accumulator_;
		int fixed_mutation_accumulator_size = (int)fixed_mutation_accumulator.size();
		
		if (fixed_mutation_accumulator_size == 0)
			continue;
		
		slim_chromosome_index_t chromosome_index = chromosome->Index();
		
		//std::cout << "Chromosome " << chromosome_index << ": removing " << fixed_mutation_accumulator.size() << " fixed mutations..." << std::endl;
		
		// We remove fixed mutations from each MutationRun just once; this is the operation ID we use for that
		int first_haplosome_index = species_.FirstHaplosomeIndices()[chromosome_index];
		int last_haplosome_index = species_.LastHaplosomeIndices()[chromosome_index];
		int64_t operation_id = MutationRun::GetNextOperationID();
		
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)		// subpopulations
		{
			Subpopulation *subpop = subpop_pair.second;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					if (!haplosome->IsNull())
					{
						// Loop over the mutations to remove, and take advantage of our mutation runs by scanning
						// for removal only within the runs that contain a mutation to be removed.  If there is
						// more than one mutation to be removed within the same run, the second time around the
						// runs will no-op the scan using operation_id.  The whole rest of the haplosomes can be skipped.
						slim_position_t mutrun_length = haplosome->mutrun_length_;
						
						for (int mut_index = 0; mut_index < fixed_mutation_accumulator_size; mut_index++)
						{
							MutationIndex mut_to_remove = fixed_mutation_accumulator[mut_index];
							Mutation *mutation = (mut_block_ptr + mut_to_remove);
							slim_position_t mut_position = mutation->position_;
							slim_mutrun_index_t mutrun_index = (slim_mutrun_index_t)(mut_position / mutrun_length);
								
							haplosome->RemoveFixedMutations(operation_id, mutrun_index);
						}
					}
				}
			}
		}
		
		slim_tick_t tick = community_.Tick();
		
		// TREE SEQUENCE RECORDING
		if (species_.RecordingTreeSequence())
		{
			// When doing tree recording, we additionally keep all fixed mutations (their ids) in a multimap indexed by their position
			// This allows us to find all the fixed mutations at a given position quickly and easily, for calculating derived states
			for (int i = 0; i < fixed_mutation_accumulator_size; i++)
			{
				Mutation *mut_to_remove = mut_block_ptr + fixed_mutation_accumulator[i];
				Substitution *sub = new Substitution(*mut_to_remove, tick);
				
				treeseq_substitutions_map_.emplace(mut_to_remove->position_, sub);
				substitutions_.emplace_back(sub);
			}
		}
		else
		{
			// When not doing tree recording, we just create substitutions and keep them in a vector
			for (int i = 0; i < fixed_mutation_accumulator_size; i++)
			{
				Mutation *mut_to_remove = mut_block_ptr + fixed_mutation_accumulator[i];
				Substitution *sub = new Substitution(*mut_to_remove, tick);
				
				substitutions_.emplace_back(sub);
			}
		}
		
		// Nucleotide-based models also need to modify the ancestral sequence when a mutation fixes
		if (species_.IsNucleotideBased())
		{
			NucleotideArray *ancestral_seq = chromosome->ancestral_seq_buffer_;
			
			for (int i = 0; i < fixed_mutation_accumulator_size; i++)
			{
				Mutation *mut_to_remove = mut_block_ptr + fixed_mutation_accumulator[i];
				
				if (mut_to_remove->mutation_type_ptr_->nucleotide_based_)
					ancestral_seq->SetNucleotideAtIndex(mut_to_remove->position_, mut_to_remove->nucleotide_);
			}
		}
		
		// Clear the accumulator for reuse next tick
		fixed_mutation_accumulator.resize(0);
	}
	
	// now we can delete (or zombify) removed mutation objects
	if (removed_mutation_accumulator.size() > 0)
	{
		for (int i = 0; i < removed_mutation_accumulator.size(); i++)
		{
			MutationIndex mutation = removed_mutation_accumulator[i];
			
#if DEBUG_MUTATION_ZOMBIES
			// Note that this violates SLiM guarantees, as of SLiM 3.5, because mutations are allowed to be retained
			// long-term, so they are not necessarily invalid objects just because they're removed from the registry.
			// But this might be useful for catching tricky bugs, so I'm leaving it in.  BCH 10/2/2020
			(mut_block_ptr + mutation)->mutation_type_ptr_ = nullptr;	// render lethal
			(mut_block_ptr + mutation)->reference_count_ = -1;			// zombie
#else
			// We no longer delete mutation objects; instead, we release them
			(mut_block_ptr + mutation)->Release();
#endif
		}
	}
}

void Population::CheckMutationRegistry(bool p_check_haplosomes)
{
	if ((model_type_ == SLiMModelType::kModelTypeWF) && child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::CheckMutationRegistry): (internal error) CheckMutationRegistry() may only be called from the parent generation in WF models." << EidosTerminate();
	
	Mutation *mutation_block_ptr = gSLiM_Mutation_Block;
#if DEBUG_MUTATION_ZOMBIES
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
#endif
	int registry_size;
	const MutationIndex *registry_iter = MutationRegistry(&registry_size);
	const MutationIndex *registry_iter_end = registry_iter + registry_size;
	
	// First check that we don't have any zombies in our registry.  BCH 10/2/2020 as of SLiM 3.5 we now also check
	// for registered/segregating mutations that do not have state_ == MutationState::kInRegistry, and we now get
	// called in DEBUG mode all the time, and in non-DEBUG mode when removeMutations(substitute=T) has been used.
	for (; registry_iter != registry_iter_end; ++registry_iter)
	{
		MutationIndex mut_index = *registry_iter;
		
#if DEBUG_MUTATION_ZOMBIES
		if (*(refcount_block_ptr + mut_index) == -1)
			EIDOS_TERMINATION << "ERROR (Population::CheckMutationRegistry): (internal error) zombie mutation found in registry with address " << (*registry_iter) << EidosTerminate();
#endif
		
		int8_t mut_state = (mutation_block_ptr + mut_index)->state_;
		
		if (mut_state != MutationState::kInRegistry)
			EIDOS_TERMINATION << "ERROR (Population::CheckMutationRegistry): A mutation was found in the mutation registry with a state other than MutationState::kInRegistry (" << (int)mut_state << ").  This may be the result of calling removeMutations(substitute=T) without actually removing the mutation from all haplosomes." << EidosTerminate();
	}
	
#if DEBUG_LESS_INTENSIVE
	// These tests are extremely intensive, so sometimes it's useful to dial them down...
	if ((community_.Tick() % 10) != 5)
		return;
#endif
	
	if (p_check_haplosomes)
	{
		// then check that we don't have any zombies in any haplosomes
		int haplosome_count_per_individual = species_.HaplosomeCountPerIndividual();
		
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)		// subpopulations
		{
			Subpopulation *subpop = subpop_pair.second;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					int mutrun_count = haplosome->mutrun_count_;
					
					for (int run_index = 0; run_index < mutrun_count; ++run_index)
					{
						const MutationRun *mutrun = haplosome->mutruns_[run_index];
						const MutationIndex *haplosome_iter = mutrun->begin_pointer_const();
						const MutationIndex *haplosome_end_iter = mutrun->end_pointer_const();
						
						for (; haplosome_iter != haplosome_end_iter; ++haplosome_iter)
						{
							MutationIndex mut_index = *haplosome_iter;
							
#if DEBUG_MUTATION_ZOMBIES
							if (*(refcount_block_ptr + mut_index) == -1)
								EIDOS_TERMINATION << "ERROR (Population::CheckMutationRegistry): (internal error) zombie mutation found in haplosome with address " << (*haplosome_iter) << EidosTerminate();
#endif
							
							int8_t mut_state = (mutation_block_ptr + mut_index)->state_;
							
							if (mut_state != MutationState::kInRegistry)
								EIDOS_TERMINATION << "ERROR (Population::CheckMutationRegistry): A mutation was found in a haplosome with a state other than MutationState::kInRegistry (" << (int)mut_state << ").  This may be the result of calling removeMutations(substitute=T) without actually removing the mutation from all haplosomes." << EidosTerminate();
						}
					}
				}
			}
		}
	}
}

// print all mutations and all haplosomes to a stream in binary, for maximum reading speed
// this is a binary version of Individual::PrintIndividuals_SLiM(), which is quite parallel
void Population::PrintAllBinary(std::ostream &p_out, bool p_output_spatial_positions, bool p_output_ages, bool p_output_ancestral_nucs, bool p_output_pedigree_ids, bool p_output_object_tags, bool p_output_substitutions) const
{
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::PrintAllBinary): (internal error) called with child generation active!." << EidosTerminate();
	
	// Figure out spatial position output.  If it was not requested, then we don't do it, and that's fine.  If it
	// was requested, then we output the number of spatial dimensions we're configured for (which might be zero).
	int32_t spatial_output_count = (int32_t)(p_output_spatial_positions ? species_.SpatialDimensionality() : 0);
	
	// Figure out age output.  If it was not requested, don't do it; if it was requested, do it if we use a nonWF model.
	int age_output_count = (p_output_ages && (model_type_ == SLiMModelType::kModelTypeNonWF)) ? 1 : 0;
	
	// Figure out pedigree ID output
	int pedigree_output_count = (p_output_pedigree_ids ? 1 : 0);
	
	// We will output nucleotides for all mutations, and an ancestral sequence at the end, if we are nucleotide-based.
	bool has_nucleotides = species_.IsNucleotideBased();
	bool output_ancestral_nucs = has_nucleotides && p_output_ancestral_nucs;
	
	int32_t section_end_tag = 0xFFFF0000;
	
	// Header section
	{
		// Write a 32-bit endianness tag
		int32_t endianness_tag = 0x12345678;
		
		p_out.write(reinterpret_cast<char *>(&endianness_tag), sizeof endianness_tag);
		
		// Write a format version tag
		int32_t version_tag = 8;		// version 2 started with SLiM 2.1
										// version 3 started with SLiM 2.3
										// version 4 started with SLiM 3.0, only when individual age is output
										// version 5 started with SLiM 3.3, adding a "flags" field and nucleotide support
										// version 6 started with SLiM 3.5, adding optional pedigree ID output with a new flag
										// version 7 started with SLiM 4.0, changing generation to ticks and adding cycle
										// version 8 started with SLiM 5.0, adding multiple chromosomes
		p_out.write(reinterpret_cast<char *>(&version_tag), sizeof version_tag);
		
		// Write the size of a double
		int32_t double_size = sizeof(double);
		
		p_out.write(reinterpret_cast<char *>(&double_size), sizeof double_size);
		
		// Write a test double, to ensure the same format is used on the reading machine
		double double_test = 1234567890.0987654321;
		
		p_out.write(reinterpret_cast<char *>(&double_test), sizeof double_test);
		
		// Write a "flags" field, new in SLiM 3.3; the bit values here are all changed/new in version 8
		{
			int64_t flags = 0;
			
			if (spatial_output_count)		flags |= spatial_output_count;	// takes 0x0001 and 0x0002
			if (age_output_count)			flags |= 0x0004;
			if (pedigree_output_count)		flags |= 0x0008;
			if (has_nucleotides)			flags |= 0x0010;
			if (output_ancestral_nucs)		flags |= 0x0020;
			if (p_output_object_tags)		flags |= 0x0040;
			if (p_output_substitutions)		flags |= 0x0080;
			
			p_out.write(reinterpret_cast<char *>(&flags), sizeof flags);
		}
		
		// Write the sizes of the various SLiM types
		int32_t slim_tick_t_size = sizeof(slim_tick_t);
		int32_t slim_position_t_size = sizeof(slim_position_t);
		int32_t slim_objectid_t_size = sizeof(slim_objectid_t);
		int32_t slim_popsize_t_size = sizeof(slim_popsize_t);
		int32_t slim_refcount_t_size = sizeof(slim_refcount_t);
		int32_t slim_selcoeff_t_size = sizeof(slim_selcoeff_t);
		int32_t slim_mutationid_t_size = sizeof(slim_mutationid_t);													// Added in version 2
		int32_t slim_polymorphismid_t_size = sizeof(slim_polymorphismid_t);											// Added in version 2
		int32_t slim_age_t_size = sizeof(slim_age_t);																// Added in version 6
		int32_t slim_pedigreeid_t_size = sizeof(slim_pedigreeid_t);													// Added in version 6
		int32_t slim_haplosomeid_t_size = sizeof(slim_haplosomeid_t);												// Added in version 6
		int32_t slim_usertag_t_size = sizeof(slim_usertag_t);														// Added in version 8
		
		p_out.write(reinterpret_cast<char *>(&slim_tick_t_size), sizeof slim_tick_t_size);
		p_out.write(reinterpret_cast<char *>(&slim_position_t_size), sizeof slim_position_t_size);
		p_out.write(reinterpret_cast<char *>(&slim_objectid_t_size), sizeof slim_objectid_t_size);
		p_out.write(reinterpret_cast<char *>(&slim_popsize_t_size), sizeof slim_popsize_t_size);
		p_out.write(reinterpret_cast<char *>(&slim_refcount_t_size), sizeof slim_refcount_t_size);
		p_out.write(reinterpret_cast<char *>(&slim_selcoeff_t_size), sizeof slim_selcoeff_t_size);
		p_out.write(reinterpret_cast<char *>(&slim_mutationid_t_size), sizeof slim_mutationid_t_size);				// Added in version 2
		p_out.write(reinterpret_cast<char *>(&slim_polymorphismid_t_size), sizeof slim_polymorphismid_t_size);		// Added in version 2
		p_out.write(reinterpret_cast<char *>(&slim_age_t_size), sizeof slim_age_t_size);							// Added in version 6
		p_out.write(reinterpret_cast<char *>(&slim_pedigreeid_t_size), sizeof slim_pedigreeid_t_size);				// Added in version 6
		p_out.write(reinterpret_cast<char *>(&slim_haplosomeid_t_size), sizeof slim_haplosomeid_t_size);			// Added in version 6
		p_out.write(reinterpret_cast<char *>(&slim_usertag_t_size), sizeof slim_usertag_t_size);					// Added in version 8
		
		// Write the tick and cycle
		slim_tick_t tick = community_.Tick();																		// Changed from generation to tick in version 7
		slim_tick_t cycle = species_.Cycle();																		// Added in version 7
		
		p_out.write(reinterpret_cast<char *>(&tick), sizeof tick);
		p_out.write(reinterpret_cast<char *>(&cycle), sizeof cycle);
	}
	
	// Write a tag indicating the section has ended
	p_out.write(reinterpret_cast<char *>(&section_end_tag), sizeof section_end_tag);
	
	// Populations section
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)
	{
		const Subpopulation *subpop = subpop_pair.second;
		slim_objectid_t subpop_id = subpop_pair.first;
		slim_popsize_t subpop_size = subpop->parent_subpop_size_;
		double subpop_sex_ratio;
		
		if (model_type_ == SLiMModelType::kModelTypeWF)
		{
			subpop_sex_ratio = subpop->parent_sex_ratio_;
		}
		else
		{
			if (subpop->parent_subpop_size_ == 0)
				subpop_sex_ratio = 0.0;
			else
				subpop_sex_ratio = 1.0 - (subpop->parent_first_male_index_ / (double)subpop->parent_subpop_size_);
		}
		
		// Write a tag indicating we are starting a new subpopulation
		int32_t subpop_start_tag = 0xFFFF0001;
		
		p_out.write(reinterpret_cast<char *>(&subpop_start_tag), sizeof subpop_start_tag);
		
		// Write the subpop identifier
		p_out.write(reinterpret_cast<char *>(&subpop_id), sizeof subpop_id);
		
		// Write the subpop size
		p_out.write(reinterpret_cast<char *>(&subpop_size), sizeof subpop_size);
		
		// Write a flag indicating whether this population has sexual or hermaphroditic
		int32_t sex_flag = (subpop->sex_enabled_ ? 1 : 0);
		
		p_out.write(reinterpret_cast<char *>(&sex_flag), sizeof sex_flag);
		
		// Write the sex ratio; if we are not sexual, this will be garbage, but that is fine, we want a constant-length record
		p_out.write(reinterpret_cast<char *>(&subpop_sex_ratio), sizeof subpop_sex_ratio);
		
		// Write the tag if requested
		if (p_output_object_tags)
			p_out.write(reinterpret_cast<const char *>(&subpop->tag_value_), sizeof subpop->tag_value_);
		
		// now will come either a subpopulation start tag, or a section end tag
	}
	
	// Write a tag indicating the section has ended
	p_out.write(reinterpret_cast<char *>(&section_end_tag), sizeof section_end_tag);
	
	// Individuals section; this contains optional metadata about the individuals
	// This section is new with version 8; its information used to be embedded in the Haplosomes section, in binary
	for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)			// go through all subpopulations
	{
		Subpopulation *subpop = subpop_pair.second;
		slim_popsize_t subpop_size = subpop->parent_subpop_size_;
		
		for (slim_popsize_t individual_index = 0; individual_index < subpop_size; individual_index++)	// go through all children
		{
			Individual &individual = *(subpop->parent_individuals_[individual_index]);
			
			// Output individual sex
			p_out.write(reinterpret_cast<char *>(&individual.sex_), sizeof individual.sex_);
			
			// Output individual pedigree ID information.  Added in version 5.
			if (pedigree_output_count)
			{
				slim_pedigreeid_t pedigree_id = individual.PedigreeID();
				
				p_out.write(reinterpret_cast<char *>(&pedigree_id), sizeof pedigree_id);
			}
			
			// Output individual spatial position information.  Added in version 3.
			if (spatial_output_count)
			{
				if (spatial_output_count >= 1)
					p_out.write(reinterpret_cast<char *>(&individual.spatial_x_), sizeof individual.spatial_x_);
				if (spatial_output_count >= 2)
					p_out.write(reinterpret_cast<char *>(&individual.spatial_y_), sizeof individual.spatial_y_);
				if (spatial_output_count >= 3)
					p_out.write(reinterpret_cast<char *>(&individual.spatial_z_), sizeof individual.spatial_z_);
			}
			
			// Output individual age information before the mutation list.  Added in version 4.
			if (age_output_count)
			{
				p_out.write(reinterpret_cast<char *>(&individual.age_), sizeof individual.age_);
			}
			
			// output object tags if requested
			if (p_output_object_tags)
			{
				char T_value = 1, F_value = 0, UNDEF_value = 2;
				
				// for these two, we just write out undefined-tag values directly; they will read back in
				p_out.write(reinterpret_cast<char *>(&individual.tag_value_), sizeof individual.tag_value_);
				p_out.write(reinterpret_cast<char *>(&individual.tagF_value_), sizeof individual.tagF_value_);
				
				// for the logical tags, we write out an undefined-tag value of 2
				if (individual.tagL0_set_)
					p_out.write(individual.tagL0_value_ ? &T_value : &F_value, sizeof T_value);
				else
					p_out.write(&UNDEF_value, sizeof UNDEF_value);
				
				if (individual.tagL1_set_)
					p_out.write(individual.tagL1_value_ ? &T_value : &F_value, sizeof T_value);
				else
					p_out.write(&UNDEF_value, sizeof UNDEF_value);
				
				if (individual.tagL2_set_)
					p_out.write(individual.tagL2_value_ ? &T_value : &F_value, sizeof T_value);
				else
					p_out.write(&UNDEF_value, sizeof UNDEF_value);
				
				if (individual.tagL3_set_)
					p_out.write(individual.tagL3_value_ ? &T_value : &F_value, sizeof T_value);
				else
					p_out.write(&UNDEF_value, sizeof UNDEF_value);
				
				if (individual.tagL4_set_)
					p_out.write(individual.tagL4_value_ ? &T_value : &F_value, sizeof T_value);
				else
					p_out.write(&UNDEF_value, sizeof UNDEF_value);
			}
		}
	}
	
	// Write a tag indicating the section has ended
	p_out.write(reinterpret_cast<char *>(&section_end_tag), sizeof section_end_tag);
	
	// BCH 2/5/2025: Now we write genetic data for each chromosome.  Each chromosome will get a section end tag.
	// Here we write out the chromosome count, so the reading code knows how many chromosome sections to expect.
	const std::vector<Chromosome *> &chromosomes = species_.Chromosomes();
	int32_t chromosome_count = (int)chromosomes.size();
	
	p_out.write(reinterpret_cast<char *>(&chromosome_count), sizeof chromosome_count);
	
	for (const Chromosome *chromosome : chromosomes)
	{
		// write information about the chromosome; we don't write the symbol, since strings are annoying,
		// so the chromosome symbol will not be validated on read, but I think that's fine
		int32_t chromosome_index = chromosome->Index();
		int32_t chromosome_type = (int32_t)chromosome->Type();
		int64_t chromosome_id = chromosome->ID();
		slim_position_t chromosome_lastpos = chromosome->last_position_;
		
		p_out.write(reinterpret_cast<char *>(&chromosome_index), sizeof chromosome_index);
		p_out.write(reinterpret_cast<char *>(&chromosome_type), sizeof chromosome_type);
		p_out.write(reinterpret_cast<char *>(&chromosome_id), sizeof chromosome_id);
		p_out.write(reinterpret_cast<char *>(&chromosome_lastpos), sizeof chromosome_lastpos);
		
		if (p_output_object_tags)
			p_out.write(reinterpret_cast<const char *>(&chromosome->tag_value_), sizeof chromosome->tag_value_);
		
		// Find all polymorphisms
		int first_haplosome_index = species_.FirstHaplosomeIndices()[chromosome_index];
		int last_haplosome_index = species_.LastHaplosomeIndices()[chromosome_index];
		PolymorphismMap polymorphisms;
		Mutation *mut_block_ptr = gSLiM_Mutation_Block;
		
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)			// go through all subpopulations
		{
			Subpopulation *subpop = subpop_pair.second;
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
				{
					Haplosome *haplosome = haplosomes[haplosome_index];
					
					int mutrun_count = haplosome->mutrun_count_;
					
					for (int run_index = 0; run_index < mutrun_count; ++run_index)
					{
						const MutationRun *mutrun = haplosome->mutruns_[run_index];
						int mut_count = mutrun->size();
						const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
						
						for (int mut_index = 0; mut_index < mut_count; ++mut_index)
							AddMutationToPolymorphismMap(&polymorphisms, mut_block_ptr + mut_ptr[mut_index]);
					}
				}
			}
		}
		
		// Write out the size of the mutation map, so we can allocate a vector rather than utilizing std::map when reading
		int32_t mutation_map_size = (int32_t)polymorphisms.size();
		
		p_out.write(reinterpret_cast<char *>(&mutation_map_size), sizeof mutation_map_size);
		
		// Mutations section
		for (const PolymorphismPair &polymorphism_pair : polymorphisms)
		{
			const Polymorphism &polymorphism = polymorphism_pair.second;
			const Mutation *mutation_ptr = polymorphism.mutation_ptr_;
			const MutationType *mutation_type_ptr = mutation_ptr->mutation_type_ptr_;
			
			slim_polymorphismid_t polymorphism_id = polymorphism.polymorphism_id_;
			int64_t mutation_id = mutation_ptr->mutation_id_;													// Added in version 2
			slim_objectid_t mutation_type_id = mutation_type_ptr->mutation_type_id_;
			slim_position_t position = mutation_ptr->position_;
			slim_selcoeff_t selection_coeff = mutation_ptr->selection_coeff_;
			slim_selcoeff_t dominance_coeff = mutation_type_ptr->dominance_coeff_;
			// BCH 9/22/2021: Note that mutation_type_ptr->hemizygous_dominance_coeff_ is not saved; too edge to be bothered...
			slim_objectid_t subpop_index = mutation_ptr->subpop_index_;
			slim_tick_t origin_tick = mutation_ptr->origin_tick_;
			slim_refcount_t prevalence = polymorphism.prevalence_;
			int8_t nucleotide = mutation_ptr->nucleotide_;
			
			// Write a tag indicating we are starting a new mutation
			int32_t mutation_start_tag = 0xFFFF0002;
			
			p_out.write(reinterpret_cast<char *>(&mutation_start_tag), sizeof mutation_start_tag);
			
			// Write the mutation data
			p_out.write(reinterpret_cast<char *>(&polymorphism_id), sizeof polymorphism_id);
			p_out.write(reinterpret_cast<char *>(&mutation_id), sizeof mutation_id);							// Added in version 2
			p_out.write(reinterpret_cast<char *>(&mutation_type_id), sizeof mutation_type_id);
			p_out.write(reinterpret_cast<char *>(&position), sizeof position);
			p_out.write(reinterpret_cast<char *>(&selection_coeff), sizeof selection_coeff);
			p_out.write(reinterpret_cast<char *>(&dominance_coeff), sizeof dominance_coeff);
			p_out.write(reinterpret_cast<char *>(&subpop_index), sizeof subpop_index);
			p_out.write(reinterpret_cast<char *>(&origin_tick), sizeof origin_tick);
			p_out.write(reinterpret_cast<char *>(&prevalence), sizeof prevalence);
			
			if (has_nucleotides)
				p_out.write(reinterpret_cast<char *>(&nucleotide), sizeof nucleotide);							// added in version 5
			
			if (p_output_object_tags)
				p_out.write(reinterpret_cast<const char *>(&mutation_ptr->tag_value_), sizeof mutation_ptr->tag_value_);
			
			// now will come either a mutation start tag, or a section end tag
		}
		
		// Write a tag indicating the section has ended
		p_out.write(reinterpret_cast<char *>(&section_end_tag), sizeof section_end_tag);
		
		// Haplosomes section
		bool use_16_bit = (mutation_map_size <= UINT16_MAX - 1);	// 0xFFFF is reserved as the start of our various tags
		
		for (const std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : subpops_)			// go through all subpopulations
		{
			Subpopulation *subpop = subpop_pair.second;
			slim_objectid_t subpop_id = subpop_pair.first + 1;	// + 1 so it doesn't ever collide with the section end tag
			
			for (Individual *ind : subpop->parent_individuals_)
			{
				Haplosome **haplosomes = ind->haplosomes_;
				
				for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; haplosome_index++)
				{
					const Haplosome *haplosome = haplosomes[haplosome_index];
					
					// Write out the haplosome header; start with the subpop id + 1 to guarantee that the first 32 bits are != section_end_tag
					p_out.write(reinterpret_cast<char *>(&subpop_id), sizeof subpop_id);	// + 1
					
					if (p_output_object_tags)
						p_out.write(reinterpret_cast<const char *>(&haplosome->tag_value_), sizeof haplosome->tag_value_);
					
					// Write out the mutation list
					if (haplosome->IsNull())
					{
						// null haplosomes get a 32-bit flag value written instead of a mutation count
						int32_t null_haplosome_tag = 0xFFFF1000;
						
						p_out.write(reinterpret_cast<char *>(&null_haplosome_tag), sizeof null_haplosome_tag);
					}
					else
					{
						// write a 32-bit mutation count
						{
							int32_t total_mutations = haplosome->mutation_count();
							
							p_out.write(reinterpret_cast<char *>(&total_mutations), sizeof total_mutations);
						}
						
						if (use_16_bit)
						{
							// Write out 16-bit mutation tags
							int mutrun_count = haplosome->mutrun_count_;
							
							for (int run_index = 0; run_index < mutrun_count; ++run_index)
							{
								const MutationRun *mutrun = haplosome->mutruns_[run_index];
								int mut_count = mutrun->size();
								const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
								
								for (int mut_index = 0; mut_index < mut_count; ++mut_index)
								{
									slim_polymorphismid_t polymorphism_id = FindMutationInPolymorphismMap(polymorphisms, mut_block_ptr + mut_ptr[mut_index]);
									
									if (polymorphism_id == -1)
										EIDOS_TERMINATION << "ERROR (Population::PrintAllBinary): (internal error) polymorphism not found." << EidosTerminate();
									
									if (polymorphism_id <= UINT16_MAX - 1)
									{
										uint16_t id_16 = (uint16_t)polymorphism_id;
										
										p_out.write(reinterpret_cast<char *>(&id_16), sizeof id_16);
									}
									else
									{
										EIDOS_TERMINATION << "ERROR (Population::PrintAllBinary): (internal error) mutation id out of 16-bit bounds." << EidosTerminate();
									}
								}
							}
						}
						else
						{
							// Write out 32-bit mutation tags
							int mutrun_count = haplosome->mutrun_count_;
							
							for (int run_index = 0; run_index < mutrun_count; ++run_index)
							{
								const MutationRun *mutrun = haplosome->mutruns_[run_index];
								int mut_count = mutrun->size();
								const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
								
								for (int mut_index = 0; mut_index < mut_count; ++mut_index)
								{
									slim_polymorphismid_t polymorphism_id = FindMutationInPolymorphismMap(polymorphisms, mut_block_ptr + mut_ptr[mut_index]);
									
									if (polymorphism_id == -1)
										EIDOS_TERMINATION << "ERROR (Population::PrintAllBinary): (internal error) polymorphism not found." << EidosTerminate();
									
									p_out.write(reinterpret_cast<char *>(&polymorphism_id), sizeof polymorphism_id);
								}
							}
						}
						
						// now will come either an individual index, or a section end tag
					}
				}
			}
		}
		
		// Write a tag indicating the haplosomes section has ended
		p_out.write(reinterpret_cast<char *>(&section_end_tag), sizeof section_end_tag);
		
		// Ancestral sequence section, for nucleotide-based models, when requested
		if (output_ancestral_nucs)
		{
			chromosome->AncestralSequence()->WriteCompressedNucleotides(p_out);
			
			// Write a tag indicating the ancestral sequence section has ended
			p_out.write(reinterpret_cast<char *>(&section_end_tag), sizeof section_end_tag);
		}
	}
	
	// New in SLiM 5, output substitutions if requested
	if (p_output_substitutions)
	{
		for (const Substitution *substitution_ptr : substitutions_)
		{
			const MutationType *mutation_type_ptr = substitution_ptr->mutation_type_ptr_;
			int64_t mutation_id = substitution_ptr->mutation_id_;
			slim_objectid_t mutation_type_id = mutation_type_ptr->mutation_type_id_;
			slim_position_t position = substitution_ptr->position_;
			slim_selcoeff_t selection_coeff = substitution_ptr->selection_coeff_;
			slim_selcoeff_t dominance_coeff = mutation_type_ptr->dominance_coeff_;
			slim_objectid_t subpop_index = substitution_ptr->subpop_index_;
			slim_tick_t origin_tick = substitution_ptr->origin_tick_;
			slim_tick_t fixation_tick = substitution_ptr->fixation_tick_;
			slim_chromosome_index_t chromosome_index = substitution_ptr->chromosome_index_;
			int8_t nucleotide = substitution_ptr->nucleotide_;
			
			// Write a tag indicating we are starting a new substitution
			int32_t substitution_start_tag = 0xFFFF0003;
			
			p_out.write(reinterpret_cast<char *>(&substitution_start_tag), sizeof substitution_start_tag);
			
			// Write the mutation data
			p_out.write(reinterpret_cast<char *>(&mutation_id), sizeof mutation_id);
			p_out.write(reinterpret_cast<char *>(&mutation_type_id), sizeof mutation_type_id);
			p_out.write(reinterpret_cast<char *>(&position), sizeof position);
			p_out.write(reinterpret_cast<char *>(&selection_coeff), sizeof selection_coeff);
			p_out.write(reinterpret_cast<char *>(&dominance_coeff), sizeof dominance_coeff);
			p_out.write(reinterpret_cast<char *>(&subpop_index), sizeof subpop_index);
			p_out.write(reinterpret_cast<char *>(&origin_tick), sizeof origin_tick);
			p_out.write(reinterpret_cast<char *>(&fixation_tick), sizeof fixation_tick);
			p_out.write(reinterpret_cast<char *>(&chromosome_index), sizeof chromosome_index);
			
			if (has_nucleotides)
				p_out.write(reinterpret_cast<char *>(&nucleotide), sizeof nucleotide);
			if (p_output_object_tags)
				p_out.write(reinterpret_cast<const char *>(&substitution_ptr->tag_value_), sizeof substitution_ptr->tag_value_);
			
			// now will come either a mutation start tag, or a section end tag
		}
		
		// Write a tag indicating the section has ended
		p_out.write(reinterpret_cast<char *>(&section_end_tag), sizeof section_end_tag);
	}
}

// print sample of p_sample_size haplosomes from subpopulation p_subpop_id
void Population::PrintSample_SLiM(std::ostream &p_out, Subpopulation &p_subpop, slim_popsize_t p_sample_size, bool p_replace, IndividualSex p_requested_sex, const Chromosome &p_chromosome) const
{
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::PrintSample_SLiM): (internal error) called with child generation active!" << EidosTerminate();
	
	std::vector<Individual *> &subpop_individuals = p_subpop.parent_individuals_;
	std::vector<Haplosome *> candidates;
	
	int first_haplosome_index = species_.FirstHaplosomeIndices()[p_chromosome.Index()];
	int last_haplosome_index = species_.LastHaplosomeIndices()[p_chromosome.Index()];
	
	for (Individual *ind : subpop_individuals)
	{
		if (p_subpop.sex_enabled_ && (p_requested_sex != IndividualSex::kUnspecified) && (ind->sex_ != p_requested_sex))
			continue;
		
		for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; ++haplosome_index)
		{
			Haplosome *haplosome = ind->haplosomes_[haplosome_index];
			
			if (!haplosome->IsNull())
				candidates.push_back(haplosome);
		}
	}
	
	if (p_replace && (candidates.size() == 0))
		EIDOS_TERMINATION << "ERROR (Population::PrintSample_SLiM): no eligible haplosomes for sampling with replacement." << EidosTerminate();
	if (!p_replace && ((slim_popsize_t)candidates.size() < p_sample_size))
		EIDOS_TERMINATION << "ERROR (Population::PrintSample_SLiM): not enough eligible haplosomes for sampling without replacement." << EidosTerminate();
	
	// assemble a sample (with or without replacement)
	std::vector<Haplosome *> sample; 
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	for (slim_popsize_t s = 0; s < p_sample_size; s++)
	{
		// select a random haplosome (not a random individual) by selecting a random candidate entry
		int candidate_index = static_cast<slim_popsize_t>(Eidos_rng_uniform_int(rng, (uint32_t)candidates.size()));
		
		sample.emplace_back(candidates[candidate_index]);
		
		// If we're sampling without replacement, remove the index we have just taken; either we will use it or it is invalid
		if (!p_replace)
		{
			candidates[candidate_index] = candidates.back();
			candidates.pop_back();
		}
	}
	
	// print the sample using Haplosome's static member function
	Haplosome::PrintHaplosomes_SLiM(p_out, sample, /* p_output_object_tags */ false);
}

// print sample of p_sample_size haplosomes from subpopulation p_subpop_id, using "ms" format
void Population::PrintSample_MS(std::ostream &p_out, Subpopulation &p_subpop, slim_popsize_t p_sample_size, bool p_replace, IndividualSex p_requested_sex, const Chromosome &p_chromosome, bool p_filter_monomorphic) const
{
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::PrintSample_MS): (internal error) called with child generation active!." << EidosTerminate();
	
	std::vector<Individual *> &subpop_individuals = p_subpop.parent_individuals_;
	std::vector<Haplosome *> candidates;
	
	int first_haplosome_index = species_.FirstHaplosomeIndices()[p_chromosome.Index()];
	int last_haplosome_index = species_.LastHaplosomeIndices()[p_chromosome.Index()];
	
	for (Individual *ind : subpop_individuals)
	{
		if (p_subpop.sex_enabled_ && (p_requested_sex != IndividualSex::kUnspecified) && (ind->sex_ != p_requested_sex))
			continue;
		
		for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; ++haplosome_index)
		{
			Haplosome *haplosome = ind->haplosomes_[haplosome_index];
			
			if (!haplosome->IsNull())
				candidates.push_back(haplosome);
		}
	}
	
	if (p_replace && (candidates.size() == 0))
		EIDOS_TERMINATION << "ERROR (Population::PrintSample_MS): no eligible haplosomes for sampling with replacement." << EidosTerminate();
	if (!p_replace && ((slim_popsize_t)candidates.size() < p_sample_size))
		EIDOS_TERMINATION << "ERROR (Population::PrintSample_MS): not enough eligible haplosomes for sampling without replacement." << EidosTerminate();
	
	// assemble a sample (with or without replacement)
	std::vector<Haplosome *> sample; 
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	for (slim_popsize_t s = 0; s < p_sample_size; s++)
	{
		// select a random haplosome (not a random individual) by selecting a random candidate entry
		int candidate_index = static_cast<slim_popsize_t>(Eidos_rng_uniform_int(rng, (uint32_t)candidates.size()));
		
		sample.emplace_back(candidates[candidate_index]);
		
		// If we're sampling without replacement, remove the index we have just taken; either we will use it or it is invalid
		if (!p_replace)
		{
			candidates[candidate_index] = candidates.back();
			candidates.pop_back();
		}
	}
	
	// print the sample using Haplosome's static member function
	Haplosome::PrintHaplosomes_MS(p_out, sample, p_chromosome, p_filter_monomorphic);
}

// print sample of p_sample_size *individuals* (NOT haplosomes or genomes) from subpopulation p_subpop_id
void Population::PrintSample_VCF(std::ostream &p_out, Subpopulation &p_subpop, slim_popsize_t p_sample_size, bool p_replace, IndividualSex p_requested_sex, const Chromosome &p_chromosome, bool p_output_multiallelics, bool p_simplify_nucs, bool p_output_nonnucs, bool p_group_as_individuals) const
{
	if (child_generation_valid_)
		EIDOS_TERMINATION << "ERROR (Population::PrintSample_VCF): (internal error) called with child generation active!." << EidosTerminate();
	
	std::vector<Individual *> &subpop_individuals = p_subpop.parent_individuals_;
	std::vector<Individual *> candidates;
	
	for (Individual *ind : subpop_individuals)
	{
		if (p_subpop.sex_enabled_ && (p_requested_sex != IndividualSex::kUnspecified) && (ind->sex_ != p_requested_sex))
			continue;
		
		candidates.push_back(ind);
	}
	
	if (p_replace && (candidates.size() == 0))
		EIDOS_TERMINATION << "ERROR (Population::PrintSample_VCF): no eligible individuals for sampling with replacement." << EidosTerminate();
	if (!p_replace && ((slim_popsize_t)candidates.size() < p_sample_size))
		EIDOS_TERMINATION << "ERROR (Population::PrintSample_VCF): not enough eligible individuals for sampling without replacement." << EidosTerminate();
	
	// assemble a sample (with or without replacement)
	std::vector<Haplosome *> sample; 
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	int first_haplosome_index = species_.FirstHaplosomeIndices()[p_chromosome.Index()];
	int last_haplosome_index = species_.LastHaplosomeIndices()[p_chromosome.Index()];
	
	for (slim_popsize_t s = 0; s < p_sample_size; s++)
	{
		// select a random individual (not a random haplosome) by selecting a random candidate entry
		int candidate_index = static_cast<slim_popsize_t>(Eidos_rng_uniform_int(rng, (uint32_t)candidates.size()));
		Individual *ind = candidates[candidate_index];
		
		// take all of its haplosomes for the chosen chromosome, including null haplosomes (needed as placeholders)
		for (int haplosome_index = first_haplosome_index; haplosome_index <= last_haplosome_index; ++haplosome_index)
			sample.emplace_back(ind->haplosomes_[haplosome_index]);
		
		// If we're sampling without replacement, remove the index we have just taken; either we will use it or it is invalid
		if (!p_replace)
		{
			candidates[candidate_index] = candidates.back();
			candidates.pop_back();
		}
	}
	
	// print the sample using Haplosome's static member function
	Haplosome::PrintHaplosomes_VCF(p_out, sample, p_chromosome, p_group_as_individuals, p_output_multiallelics, p_simplify_nucs, p_output_nonnucs);
}






































































