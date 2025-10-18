//
//  haplosome.cpp
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


#include "haplosome.h"
#include "slim_globals.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "community.h"
#include "species.h"
#include "polymorphism.h"
#include "subpopulation.h"
#include "mutation_block.h"
#include "eidos_sorting.h"

#include <algorithm>
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <utility>


#pragma mark -
#pragma mark Haplosome
#pragma mark -

// Static class variables in support of Haplosome's bulk operation optimization; see Haplosome::WillModifyRunForBulkOperation()
int64_t Haplosome::s_bulk_operation_id_ = 0;
slim_mutrun_index_t Haplosome::s_bulk_operation_mutrun_index_ = -1;
SLiMBulkOperationHashTable Haplosome::s_bulk_operation_runs_;


Haplosome::~Haplosome(void)
{
	if (mutruns_ != run_buffer_)
		free(mutruns_);
	mutruns_ = nullptr;
	
	mutrun_count_ = 0;
}

Chromosome *Haplosome::AssociatedChromosome(void) const
{
	return individual_->subpopulation_->species_.Chromosomes()[chromosome_index_];
}

// prints an error message, a stacktrace, and exits; called only for DEBUG
void Haplosome::NullHaplosomeAccessError(void) const
{
	EIDOS_TERMINATION << "ERROR (Haplosome::NullHaplosomeAccessError): (internal error) a null haplosome was accessed." << EidosTerminate();
}

MutationRun *Haplosome::WillModifyRun(slim_mutrun_index_t p_run_index, MutationRunContext &p_mutrun_context)
{
#if DEBUG
	if (p_run_index >= mutrun_count_)
		EIDOS_TERMINATION << "ERROR (Haplosome::WillModifyRun): (internal error) attempt to modify an out-of-index run." << EidosTerminate();
#endif
	
	// This method used to support in-place modification for mutruns with a use count of 1,
	// saving the new mutation run allocation; this is now done only in WillModifyRun_UNSHARED().
	// See the header comment for more information.
	const MutationRun *original_run = mutruns_[p_run_index];
	MutationRun *new_run = MutationRun::NewMutationRun(p_mutrun_context);	// take from shared pool of used objects
	
	new_run->copy_from_run(*original_run);
	mutruns_[p_run_index] = new_run;
	
	// We return a non-const pointer to the caller, giving them permission to modify this new run
	return new_run;
}

MutationRun *Haplosome::WillModifyRun_UNSHARED(slim_mutrun_index_t p_run_index, MutationRunContext &p_mutrun_context)
{
#if DEBUG
	if (p_run_index >= mutrun_count_)
		EIDOS_TERMINATION << "ERROR (Haplosome::WillModifyRun_UNSHARED): (internal error) attempt to modify an out-of-index run." << EidosTerminate();
#endif
	
	// This method avoids the new mutation run allocation, unless the mutation run is empty.
	// This is based on a guarantee from the caller that the run is unshared (unless it is empty).
	// See the header comment for more information.
	const MutationRun *original_run = mutruns_[p_run_index];
	
	if (original_run->size() == 0)
	{
		MutationRun *new_run = MutationRun::NewMutationRun(p_mutrun_context);	// take from shared pool of used objects
		
		new_run->copy_from_run(*original_run);
		mutruns_[p_run_index] = new_run;
		
		// We return a non-const pointer to the caller, giving them permission to modify this new run
		return new_run;
	}
	else
	{
		// We have been guaranteed by the caller that this mutation run is unshared, so we can cast away the const
		MutationRun *unlocked_run = const_cast<MutationRun *>(original_run);
		
		unlocked_run->will_modify_run();	// in-place modification of runs requires notification, for cache invalidation
		
		// We return a non-const pointer to the caller, giving them permission to modify this run
		return unlocked_run;
	}
}

void Haplosome::BulkOperationStart(int64_t p_operation_id, slim_mutrun_index_t p_mutrun_index)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Haplosome::BulkOperationStart(): s_bulk_operation_id_");
	
	if (s_bulk_operation_id_ != 0)
	{
		//EIDOS_TERMINATION << "ERROR (Haplosome::BulkOperationStart): (internal error) unmatched bulk operation start." << EidosTerminate();
		
		// It would be nice to be able to throw an exception here, but in the present design, the
		// bulk operation info can get messed up if the bulk operation throws an exception that
		// blows through the call to Haplosome::BulkOperationEnd().
		// Note this warning is not suppressed by gEidosSuppressWarnings; that is deliberate
		std::cout << "WARNING (Haplosome::BulkOperationStart): (internal error) unmatched bulk operation start." << std::endl;
		
		// For now, we assume that the end call got blown through, and we close out the old operation.
		Haplosome::BulkOperationEnd(s_bulk_operation_id_, s_bulk_operation_mutrun_index_);
	}
	
	s_bulk_operation_id_ = p_operation_id;
	s_bulk_operation_mutrun_index_ = p_mutrun_index;
}

MutationRun *Haplosome::WillModifyRunForBulkOperation(int64_t p_operation_id, slim_mutrun_index_t p_mutrun_index, MutationRunContext &p_mutrun_context)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Haplosome::WillModifyRunForBulkOperation(): s_bulk_operation_id_");
	
	if (p_mutrun_index != s_bulk_operation_mutrun_index_)
		EIDOS_TERMINATION << "ERROR (Haplosome::WillModifyRunForBulkOperation): (internal error) incorrect run index during bulk operation." << EidosTerminate();
	if (p_mutrun_index >= mutrun_count_)
		EIDOS_TERMINATION << "ERROR (Haplosome::WillModifyRunForBulkOperation): (internal error) attempt to modify an out-of-index run." << EidosTerminate();
	
#if 0
#warning Haplosome::WillModifyRunForBulkOperation disabled...
	// The trivial version of this function just calls WillModifyRun(),
	// requesting that the caller perform the operation
	return WillModifyRun(p_run_index);
#else
	// The interesting version remembers the operation in progress, using the ID, and
	// tracks original/final MutationRun pointers, returning nullptr if an original is matched.
	const MutationRun *original_run = mutruns_[p_mutrun_index];
	
	if (p_operation_id != s_bulk_operation_id_)
			EIDOS_TERMINATION << "ERROR (Haplosome::WillModifyRunForBulkOperation): (internal error) missing bulk operation start." << EidosTerminate();
	
	auto found_run_pair = s_bulk_operation_runs_.find(original_run);
	
	if (found_run_pair == s_bulk_operation_runs_.end())
	{
		// This MutationRun is not in the map, so we need to set up a new entry
		MutationRun *product_run = MutationRun::NewMutationRun(p_mutrun_context);
		
		product_run->copy_from_run(*original_run);
		mutruns_[p_mutrun_index] = product_run;
		
		try {
			s_bulk_operation_runs_.emplace(original_run, product_run);
		} catch (...) {
			EIDOS_TERMINATION << "ERROR (Haplosome::WillModifyRunForBulkOperation): (internal error) SLiM encountered a raise from an internal hash table; please report this." << EidosTerminate(nullptr);
		}
		
		//std::cout << "WillModifyRunForBulkOperation() created product for " << original_run << std::endl;
		
		return product_run;
	}
	else
	{
		// This MutationRun is in the map, so we can just reuse it to redo the operation
		mutruns_[p_mutrun_index] = found_run_pair->second;
		
		//std::cout << "   WillModifyRunForBulkOperation() substituted known product for " << original_run << std::endl;
		
		return nullptr;
	}
#endif
}

void Haplosome::BulkOperationEnd(int64_t p_operation_id, slim_mutrun_index_t p_mutrun_index)
{
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Haplosome::BulkOperationEnd(): s_bulk_operation_id_");
	
	if ((p_operation_id == s_bulk_operation_id_) && (p_mutrun_index == s_bulk_operation_mutrun_index_))
	{
		s_bulk_operation_runs_.clear();
		s_bulk_operation_id_ = 0;
		s_bulk_operation_mutrun_index_ = -1;
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Haplosome::BulkOperationEnd): (internal error) unmatched bulk operation end." << EidosTerminate();
	}
}

void Haplosome::TallyHaplosomeReferences_Checkback(slim_refcount_t *p_mutrun_ref_tally, slim_refcount_t *p_mutrun_tally, int64_t p_operation_id)
{
#if DEBUG
	if (mutrun_count_ == 0)
		NullHaplosomeAccessError();
#endif
	for (int run_index = 0; run_index < mutrun_count_; ++run_index)
	{
		if (mutruns_[run_index]->operation_id_ != p_operation_id)
		{
			(*p_mutrun_ref_tally) += mutruns_[run_index]->use_count();
			(*p_mutrun_tally)++;
			mutruns_[run_index]->operation_id_ = p_operation_id;
		}
	}
}

void Haplosome::MakeNull(void)
{
	if (mutrun_count_)
	{
		if (mutruns_ != run_buffer_)
			free(mutruns_);
		mutruns_ = nullptr;
		
		mutrun_count_ = 0;
		mutrun_length_ = 0;
	}
}

void Haplosome::ReinitializeHaplosomeToNull(Individual *individual)
{
	// Transmogrify a haplosome (which might be null or non-null) into a null haplosome
	individual_ = individual;
	
	if (mutrun_count_)
	{
		// was a non-null haplosome, needs to become null
		if (mutruns_ != run_buffer_)
			free(mutruns_);
		mutruns_ = nullptr;
		
		// chromosome_index_ remains untouched; we still belong to the same chromosome
		mutrun_count_ = 0;
		mutrun_length_ = 0;
	}
}

void Haplosome::ReinitializeHaplosomeToNonNull(Individual *individual, Chromosome *p_chromosome)
{
	// Transmogrify a haplosome (which might be null or non-null) into a non-null haplosome with a specific configuration
	individual_ = individual;
	
#if DEBUG
	// We should always be reinitializing a haplosome that already belongs to the chromosome;
	// the only reason the chromosome is passed in is that it knows the mutrun configuration.
	if (chromosome_index_ != p_chromosome->Index())
		EIDOS_TERMINATION << "ERROR (Haplosome::ReinitializeHaplosomeToNonNull): (internal error) incorrect chromosome index." << EidosTerminate();
#endif
	
	if (mutrun_count_ == 0)
	{
		// was a null haplosome, needs to become not null
		mutrun_count_ = p_chromosome->mutrun_count_;
		mutrun_length_ = p_chromosome->mutrun_length_;
		
		if (mutrun_count_ <= SLIM_HAPLOSOME_MUTRUN_BUFSIZE)
		{
			mutruns_ = run_buffer_;
#if SLIM_CLEAR_HAPLOSOMES
			EIDOS_BZERO(run_buffer_, SLIM_HAPLOSOME_MUTRUN_BUFSIZE * sizeof(const MutationRun *));
#endif
		}
		else
		{
#if SLIM_CLEAR_HAPLOSOMES
			mutruns_ = (const MutationRun **)calloc(mutrun_count_, sizeof(const MutationRun *));
#else
			mutruns_ = (const MutationRun **)malloc(mutrun_count_ * sizeof(const MutationRun *));
#endif
		}
	}
	else if (mutrun_count_ != p_chromosome->mutrun_count_)
	{
		// the number of mutruns has changed; need to reallocate
		if (mutruns_ != run_buffer_)
			free(mutruns_);
		
		mutrun_count_ = p_chromosome->mutrun_count_;
		mutrun_length_ = p_chromosome->mutrun_length_;
		
		if (mutrun_count_ <= SLIM_HAPLOSOME_MUTRUN_BUFSIZE)
		{
			mutruns_ = run_buffer_;
#if SLIM_CLEAR_HAPLOSOMES
			EIDOS_BZERO(run_buffer_, SLIM_HAPLOSOME_MUTRUN_BUFSIZE * sizeof(const MutationRun *));
#endif
		}
		else
		{
#if SLIM_CLEAR_HAPLOSOMES
			mutruns_ = (const MutationRun **)calloc(mutrun_count_, sizeof(const MutationRun *));
#else
			mutruns_ = (const MutationRun **)malloc(mutrun_count_ * sizeof(const MutationRun *));
#endif
		}
	}
	else
	{
#if SLIM_CLEAR_HAPLOSOMES
		// the number of mutruns has not changed; need to zero out
		EIDOS_BZERO(mutruns_, mutrun_count_ * sizeof(const MutationRun *));
#endif
	}
}

void Haplosome::record_derived_states(Species *p_species) const
{
	// This is called by Species::RecordAllDerivedStatesFromSLiM() to record all the derived states present
	// in a given haplosome that was just created by readFromPopulationFile() or some similar situation.  It should
	// make calls to record the derived state at each position in the haplosome that has any mutation.
	Mutation *mut_block_ptr = p_species->SpeciesMutationBlock()->mutation_buffer_;
	
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Haplosome::record_derived_states(): usage of statics");

	static std::vector<Mutation *> record_vec;
	
	for (int run_index = 0; run_index < mutrun_count_; ++run_index)
	{
		const MutationRun *mutrun = mutruns_[run_index];
		int mutrun_size = mutrun->size();
		slim_position_t last_pos = -1;
		
		//record_vec.resize(0);	// should always be left clear by the code below
		
		for (int mut_index = 0; mut_index < mutrun_size; ++mut_index)
		{
			MutationIndex mutation_index = (*mutrun)[mut_index];
			Mutation *mutation = mut_block_ptr + mutation_index;
			slim_position_t mutation_pos = mutation->position_;
			
			if (mutation_pos != last_pos)
			{
				// New position, so we finish the previous derived-state block...
				if (last_pos != -1)
				{
					p_species->RecordNewDerivedState(this, last_pos, record_vec);
					record_vec.resize(0);
				}
				
				// ...and start a new derived-state block
				last_pos = mutation_pos;
			}
			
			record_vec.emplace_back(mutation);
		}
		
		// record the last derived block, if any
		if (last_pos != -1)
		{
			p_species->RecordNewDerivedState(this, last_pos, record_vec);
			record_vec.resize(0);
		}
	}
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

void Haplosome::GenerateCachedEidosValue(void)
{
	// Note that this cache cannot be invalidated as long as a symbol table might exist that this value has been placed into
	self_value_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(this, gSLiM_Haplosome_Class));
}

const EidosClass *Haplosome::Class(void) const
{
	return gSLiM_Haplosome_Class;
}

void Haplosome::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassNameForDisplay() << "<";
	
	p_ostream << AssociatedChromosome()->Type();
	
	if (mutrun_count_ == 0)
		p_ostream << ":null>";
	else
		p_ostream << ":" << mutation_count() << ">";
}

EidosValue_SP Haplosome::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_chromosome:
		{
			// We reach our chromosome through our individual; note this prevents standalone haplosome objects
			Chromosome *chromosome = AssociatedChromosome();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(chromosome, gSLiM_Chromosome_Class));
		}
		case gID_chromosomeSubposition:		// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(chromosome_subposition_));
		}
		case gID_haplosomePedigreeID:		// ACCELERATED
		{
			if (!individual_->subpopulation_->species_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Haplosome::GetProperty): property haplosomePedigreeID is not available because pedigree recording has not been enabled." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(haplosome_id_));
		}
		case gID_individual:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(individual_, gSLiM_Individual_Class));
		}
		case gID_isNullHaplosome:		// ACCELERATED
			return ((mutrun_count_ == 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_mutations:
		{
			if (IsDeferred())
				EIDOS_TERMINATION << "ERROR (Haplosome::GetProperty): the mutations of deferred haplosomes cannot be accessed." << EidosTerminate();
			
			Mutation *mut_block_ptr = individual_->subpopulation_->species_.SpeciesMutationBlock()->mutation_buffer_;
			int mut_count = mutation_count();
			EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class))->resize_no_initialize_RR(mut_count);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			int set_index = 0;
			
			for (int run_index = 0; run_index < mutrun_count_; ++run_index)
			{
				const MutationRun *mutrun = mutruns_[run_index];
				const MutationIndex *mut_start_ptr = mutrun->begin_pointer_const();
				const MutationIndex *mut_end_ptr = mutrun->end_pointer_const();
				
				for (const MutationIndex *mut_ptr = mut_start_ptr; mut_ptr < mut_end_ptr; ++mut_ptr)
					vec->set_object_element_no_check_no_previous_RR(mut_block_ptr + *mut_ptr, set_index++);
			}
			
			return result_SP;
		}
			
			// variables
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Haplosome::GetProperty): property tag accessed on haplosome before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(tag_value));
		}
			
			// all others, including gID_none
		default:
			return super::GetProperty(p_property_id);
	}
}

EidosValue *Haplosome::GetProperty_Accelerated_haplosomePedigreeID(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	size_t value_index = 0;
	
	// check that pedigrees are enabled, once
	if (value_index < p_values_size)
	{
		Haplosome *value = (Haplosome *)(p_values[value_index]);
		
		if (!value->individual_->subpopulation_->species_.PedigreesEnabledByUser())
			EIDOS_TERMINATION << "ERROR (Haplosome::GetProperty): property haplosomePedigreeID is not available because pedigree recording has not been enabled." << EidosTerminate();
		
		int_result->set_int_no_check(value->haplosome_id_, value_index);
		++value_index;
	}
	
	for ( ; value_index < p_values_size; ++value_index)
	{
		Haplosome *value = (Haplosome *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->haplosome_id_, value_index);
	}
	
	return int_result;
}

EidosValue *Haplosome::GetProperty_Accelerated_chromosomeSubposition(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Haplosome *value = (Haplosome *)(p_values[value_index]);
		int64_t subposition_value = (uint64_t)(value->chromosome_subposition_);
		
		int_result->set_int_no_check(subposition_value, value_index);
	}
	
	return int_result;
}

EidosValue *Haplosome::GetProperty_Accelerated_isNullHaplosome(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Haplosome *value = (Haplosome *)(p_values[value_index]);
		
		logical_result->set_logical_no_check(value->mutrun_count_ == 0, value_index);
	}
	
	return logical_result;
}

EidosValue *Haplosome::GetProperty_Accelerated_tag(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Haplosome *value = (Haplosome *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (Haplosome::GetProperty): property tag accessed on haplosome before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

void Haplosome::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	switch (p_property_id)
	{
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			tag_value_ = value;
			Individual::s_any_haplosome_tag_set_ = true;
			return;
		}
			
		default:
		{
			return super::SetProperty(p_property_id, p_value);
		}
	}
}

void Haplosome::SetProperty_Accelerated_tag(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
#pragma unused (p_property_id)
	Individual::s_any_haplosome_tag_set_ = true;
	
	// SLiMCastToUsertagTypeOrRaise() is a no-op at present
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex_NOCAST(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Haplosome *)(p_values[value_index]))->tag_value_ = source_value;
	}
	else
	{
		const int64_t *source_data = p_source.IntData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Haplosome *)(p_values[value_index]))->tag_value_ = source_data[value_index];
	}
}

EidosValue_SP Haplosome::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		//case gID_containsMarkerMutation:		return ExecuteMethod_Accelerated_containsMarkerMutation(p_method_id, p_arguments, p_interpreter);
		//case gID_containsMutations:			return ExecuteMethod_Accelerated_containsMutations(p_method_id, p_arguments, p_interpreter);
		//case gID_countOfMutationsOfType:		return ExecuteMethod_Accelerated_countOfMutationsOfType(p_method_id, p_arguments, p_interpreter);
		case gID_mutationsOfType:				return ExecuteMethod_mutationsOfType(p_method_id, p_arguments, p_interpreter);
		case gID_nucleotides:					return ExecuteMethod_nucleotides(p_method_id, p_arguments, p_interpreter);
		case gID_positionsOfMutationsOfType:	return ExecuteMethod_positionsOfMutationsOfType(p_method_id, p_arguments, p_interpreter);
		case gID_sumOfMutationsOfType:			return ExecuteMethod_sumOfMutationsOfType(p_method_id, p_arguments, p_interpreter);
		default:								return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (Nlo<Mutation>$)containsMarkerMutation(io<MutationType>$ mutType, integer$ position, [returnMutation$ = F])
//
EidosValue_SP Haplosome::ExecuteMethod_Accelerated_containsMarkerMutation(EidosObject **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	EidosValue *position_value = p_arguments[1].get();
	EidosValue *returnMutation_value = p_arguments[2].get();
	
	if (p_elements_size)
	{
		// SPECIES CONSISTENCY CHECK
		Species *haplosomes_species = Community::SpeciesForHaplosomesVector((Haplosome **)p_elements, (int)p_elements_size);
		
		if (!haplosomes_species)
			EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_Accelerated_containsMarkerMutation): containsMarkerMutation() requires that all target haplosomes belong to the same species." << EidosTerminate();
		
		haplosomes_species->population_.CheckForDeferralInHaplosomesVector((Haplosome **)p_elements, p_elements_size, "Haplosome::ExecuteMethod_Accelerated_containsMarkerMutation");
		
		Species &species = *haplosomes_species;
		MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &species.community_, &species, "containsMarkerMutation()");		// SPECIES CONSISTENCY CHECK
		slim_position_t marker_position = SLiMCastToPositionTypeOrRaise(position_value->IntAtIndex_NOCAST(0, nullptr));
		
		eidos_logical_t returnMutation = returnMutation_value->LogicalAtIndex_NOCAST(0, nullptr);
		
		if (p_elements_size == 1)
		{
			// separate singleton case to return gStaticEidosValue_LogicalT / gStaticEidosValue_LogicalF
			Haplosome *element = (Haplosome *)(p_elements[0]);
			
			if (!element->IsNull())
			{
				Chromosome *chromosome = element->AssociatedChromosome();
				slim_position_t last_position = chromosome->last_position_;
				
				if (marker_position > last_position)
					EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_Accelerated_containsMarkerMutation): containsMarkerMutation() position " << marker_position << " is past the end of the chromosome for the haplosome." << EidosTerminate();
				
				Mutation *mut = element->mutation_with_type_and_position(mutation_type_ptr, marker_position, last_position);
				
				if (returnMutation == false)
					return (mut ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
				else
					return (mut ? EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(mut, gSLiM_Mutation_Class)) : (EidosValue_SP)gStaticEidosValueNULL);
			}
		}
		else if (returnMutation == false)
		{
			// We will return a logical vector, T/F for each target haplosome; parallelized
			EidosValue_Logical *result_logical_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_elements_size);
			bool null_haplosome_seen = false;
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_CONTAINS_MARKER_MUT);
#pragma omp parallel for schedule(dynamic, 16) default(none) shared(p_elements_size) firstprivate(p_elements, mutation_type_ptr, marker_position, last_position, result_logical_vec) reduction(||: null_haplosome_seen) if(p_elements_size >= EIDOS_OMPMIN_CONTAINS_MARKER_MUT) num_threads(thread_count)
			for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
			{
				Haplosome *element = (Haplosome *)(p_elements[element_index]);
				
				if (element->IsNull())
				{
					null_haplosome_seen = true;
					continue;
				}
				
				Chromosome *chromosome = element->AssociatedChromosome();
				slim_position_t last_position = chromosome->last_position_;
				
				if (marker_position > last_position)
					EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_Accelerated_containsMarkerMutation): containsMarkerMutation() position " << marker_position << " is past the end of the chromosome for the haplosome." << EidosTerminate();
				
				Mutation *mut = element->mutation_with_type_and_position(mutation_type_ptr, marker_position, last_position);
				
				result_logical_vec->set_logical_no_check(mut != nullptr, element_index);
			}
			
			if (!null_haplosome_seen)
				return EidosValue_SP(result_logical_vec);
		}
		else // (returnMutation == true)
		{
			// We will return an object<Mutation> vector, one Mutation (or NULL) for each target haplosome; not parallelized, for now
			EidosValue_Object *result_obj_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class))->reserve(p_elements_size);
			bool null_haplosome_seen = false;
			
			for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
			{
				Haplosome *element = (Haplosome *)(p_elements[element_index]);
				
				if (element->IsNull())
				{
					null_haplosome_seen = true;
					continue;
				}
				
				Chromosome *chromosome = element->AssociatedChromosome();
				slim_position_t last_position = chromosome->last_position_;
				
				if (marker_position > last_position)
					EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_Accelerated_containsMarkerMutation): containsMarkerMutation() position " << marker_position << " is past the end of the chromosome for the haplosome." << EidosTerminate();
				
				Mutation *mut = element->mutation_with_type_and_position(mutation_type_ptr, marker_position, last_position);
				
				if (mut)
					result_obj_vec->push_object_element_RR(mut);
			}
			
			if (!null_haplosome_seen)
				return EidosValue_SP(result_obj_vec);
		}
		
		// we drop through to here when a null haplosome is encountered
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_Accelerated_containsMarkerMutation): containsMarkerMutation() cannot be called on a null haplosome." << EidosTerminate();
	}
	else
	{
		return gStaticEidosValue_Logical_ZeroVec;
	}
}

//	*********************	- (logical)containsMutations(object<Mutation> mutations)
//
EidosValue_SP Haplosome::ExecuteMethod_Accelerated_containsMutations(EidosObject **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (p_elements_size)
	{
		// SPECIES CONSISTENCY CHECK
		Species *haplosomes_species = Community::SpeciesForHaplosomesVector((Haplosome **)p_elements, (int)p_elements_size);
		
		if (!haplosomes_species)
			EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_Accelerated_containsMutations): containsMutations() requires that all target haplosomes belong to the same species." << EidosTerminate();
		
		haplosomes_species->population_.CheckForDeferralInHaplosomesVector((Haplosome **)p_elements, p_elements_size, "Haplosome::ExecuteMethod_Accelerated_containsMutations");
		
		EidosValue *mutations_value = p_arguments[0].get();
		int mutations_count = mutations_value->Count();
		
		if (mutations_count > 0)
		{
			Species *mutations_species = Community::SpeciesForMutations(mutations_value);
			
			if (mutations_species != haplosomes_species)
				EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_Accelerated_containsMutations): containsMutations() requires that all mutations belong to the same species as the target haplosomes." << EidosTerminate();
		}
		
		if ((mutations_count == 1) && (p_elements_size == 1))
		{
			// We want to be smart enough to return gStaticEidosValue_LogicalT or gStaticEidosValue_LogicalF in the singleton/singleton case
			Mutation *mut = (Mutation *)(mutations_value->ObjectElementAtIndex_NOCAST(0, nullptr));
			Haplosome *element = (Haplosome *)(p_elements[0]);
			
			if (element->IsNull())
				EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_Accelerated_containsMutations): containsMutations() cannot be called on a null haplosome." << EidosTerminate();
			
			// BCH 11/24/2024: I've gone back and forth on whether it should be an error to ask whether a mutation
			// associated with chromosome A is in a haplosome associated with chromosome B.  For now I'm going to
			// say it is an error, and that can be relaxed later if it becomes clear that it is too strict.  It
			// does seem like it is a question that indicates a fundamental logic flaw, like asking whether a
			// mutation that belongs to species A is in a haplosome that belongs to species B.
			if (mut->chromosome_index_ != element->chromosome_index_)
			{
				//return gStaticEidosValue_LogicalF;
				EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_Accelerated_containsMutations): containsMutations() requires that all mutations are associated with the same chromosome as the target haplosomes.  (If this requirement makes life difficult, it could be relaxed if necessary; but it seems useful for catching logic errors.  Note that the containsMutations() method of Individual does not have this restriction.)" << EidosTerminate();
			}
			
			bool contained = element->contains_mutation(mut);
			
			return (contained ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		else
		{
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_elements_size * mutations_count);
			EidosValue_SP result(logical_result);
			int64_t result_index = 0;
			
			EidosObject * const *mutations_data = mutations_value->ObjectData();
			
			for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
			{
				Haplosome *element = (Haplosome *)(p_elements[element_index]);
				
				if (element->IsNull())
					EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_Accelerated_containsMutations): containsMutations() cannot be called on a null haplosome." << EidosTerminate();
				
				for (int value_index = 0; value_index < mutations_count; ++value_index)
				{
					Mutation *mut = (Mutation *)mutations_data[value_index];
					
					// BCH 11/24/2024: I've gone back and forth on whether it should be an error to ask whether a mutation
					// associated with chromosome A is in a haplosome associated with chromosome B.  For now I'm going to
					// say it is an error, and that can be relaxed later if it becomes clear that it is too strict.  It
					// does seem like it is a question that indicates a fundamental logic flaw, like asking whether a
					// mutation that belongs to species A is in a haplosome that belongs to species B.
					if (mut->chromosome_index_ != element->chromosome_index_)
					{
						//logical_result->set_logical_no_check(false, result_index++);
						//continue;
						EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_Accelerated_containsMutations): containsMutations() requires that all mutations are associated with the same chromosome as the target haplosomes.  (If this requirement makes life difficult, it could be relaxed if necessary; but it seems useful for catching logic errors.  Note that the containsMutations() method of Individual does not have this restriction.)" << EidosTerminate();
					}
					
					bool contained = element->contains_mutation(mut);
					
					logical_result->set_logical_no_check(contained, result_index++);
				}
			}
			
			return result;
		}
	}
	else
	{
		return gStaticEidosValue_Logical_ZeroVec;
	}
}

//	*********************	- (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Haplosome::ExecuteMethod_Accelerated_countOfMutationsOfType(EidosObject **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (p_elements_size == 0)
		return gStaticEidosValue_Integer_ZeroVec;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForHaplosomesVector((Haplosome **)p_elements, (int)p_elements_size);
	
	if (species == nullptr)
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_Accelerated_countOfMutationsOfType): countOfMutationsOfType() requires that mutType belongs to the same species as the target individual." << EidosTerminate();
	
	species->population_.CheckForDeferralInHaplosomesVector((Haplosome **)p_elements, p_elements_size, "Haplosome::ExecuteMethod_Accelerated_countOfMutationsOfType");
	
	EidosValue *mutType_value = p_arguments[0].get();
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &species->community_, species, "countOfMutationsOfType()");		// SPECIES CONSISTENCY CHECK
	
	// Count the number of mutations of the given type
	const int32_t mutrun_count = ((Haplosome *)(p_elements[0]))->mutrun_count_;
	Mutation *mut_block_ptr = species->SpeciesMutationBlock()->mutation_buffer_;
	EidosValue_Int *integer_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_elements_size);
	bool saw_error = false;
	
	EIDOS_THREAD_COUNT(gEidos_OMP_threads_G_COUNT_OF_MUTS_OF_TYPE);
#pragma omp parallel for schedule(dynamic, 1) default(none) shared(p_elements_size) firstprivate(p_elements, mut_block_ptr, mutation_type_ptr, integer_result, mutrun_count) reduction(||: saw_error) if(p_elements_size >= EIDOS_OMPMIN_G_COUNT_OF_MUTS_OF_TYPE) num_threads(thread_count)
	for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
	{
		Haplosome *element = (Haplosome *)(p_elements[element_index]);
		
		if (element->IsNull())
		{
			saw_error = true;
			continue;
		}
		
		int match_count = 0;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			const MutationRun *mutrun = element->mutruns_[run_index];
			int mut_count = mutrun->size();
			const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
			
			for (int mut_index = 0; mut_index < mut_count; ++mut_index)
				if ((mut_block_ptr + mut_ptr[mut_index])->mutation_type_ptr_ == mutation_type_ptr)
					++match_count;
		}
		
		integer_result->set_int_no_check(match_count, element_index);
	}
	
	if (saw_error)
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_Accelerated_countOfMutationsOfType): countOfMutationsOfType() cannot be called on a null haplosome." << EidosTerminate();
	
	return EidosValue_SP(integer_result);
}

//	*********************	- (object<Mutation>)mutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Haplosome::ExecuteMethod_mutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	
	if (IsDeferred())
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_mutationsOfType): the mutations of deferred haplosomes cannot be accessed." << EidosTerminate();
	if (IsNull())
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_mutationsOfType): mutationsOfType() cannot be called on a null haplosome." << EidosTerminate();
	
	Species &species = individual_->subpopulation_->species_;
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &species.community_, &species, "mutationsOfType()");		// SPECIES CONSISTENCY CHECK
	
	// We want to return a singleton if we can, but we also want to avoid scanning through all our mutations twice.
	// We do this by not creating a vector until we see the second match; with one match, we make a singleton.
	Mutation *mut_block_ptr = species.SpeciesMutationBlock()->mutation_buffer_;
	Mutation *first_match = nullptr;
	EidosValue_Object *vec = nullptr;
	EidosValue_SP result_SP;
	int run_index;
	
	for (run_index = 0; run_index < mutrun_count_; ++run_index)
	{
		const MutationRun *mutrun = mutruns_[run_index];
		int mut_count = mutrun->size();
		const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
		
		for (int mut_index = 0; mut_index < mut_count; ++mut_index)
		{
			Mutation *mut = (mut_block_ptr + mut_ptr[mut_index]);
			
			if (mut->mutation_type_ptr_ == mutation_type_ptr)
			{
				if (!vec)
				{
					if (!first_match)
						first_match = mut;
					else
					{
						vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class));
						result_SP = EidosValue_SP(vec);
						
						vec->push_object_element_RR(first_match);
						vec->push_object_element_RR(mut);
						first_match = nullptr;
					}
				}
				else
				{
					vec->push_object_element_RR(mut);
				}
			}
		}
	}
	
	// Now return the appropriate return value
	if (first_match)
	{
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(first_match, gSLiM_Mutation_Class));
	}
	else
	{
		if (!vec)
		{
			vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class));
			result_SP = EidosValue_SP(vec);
		}
		
		return result_SP;
		
	}
}

//	*********************	– (is)nucleotides([Ni$ start = NULL], [Ni$ end = NULL], [s$ format = "string"])
//
EidosValue_SP Haplosome::ExecuteMethod_nucleotides(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	if (IsDeferred())
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_nucleotides): the mutations of deferred haplosomes cannot be accessed." << EidosTerminate();
	
	Species *species = &individual_->subpopulation_->species_;
	Chromosome *chromosome = AssociatedChromosome();
	slim_position_t last_position = chromosome->last_position_;
	
	if (!species->IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_nucleotides): nucleotides() may only be called in nucleotide-based models." << EidosTerminate();
	
	NucleotideArray *sequence = chromosome->AncestralSequence();
	EidosValue *start_value = p_arguments[0].get();
	EidosValue *end_value = p_arguments[1].get();
	
	int64_t start = (start_value->Type() == EidosValueType::kValueNULL) ? 0 : start_value->IntAtIndex_NOCAST(0, nullptr);
	int64_t end = (end_value->Type() == EidosValueType::kValueNULL) ? last_position : end_value->IntAtIndex_NOCAST(0, nullptr);
	
	if ((start < 0) || (end < 0) || (start > last_position) || (end > last_position) || (start > end))
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_nucleotides): start and end must be within the chromosome's extent, and start must be <= end." << EidosTerminate();
	if (((std::size_t)start >= sequence->size()) || ((std::size_t)end >= sequence->size()))
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_nucleotides): (internal error) start and end must be within the ancestral sequence's length." << EidosTerminate();
	
	int64_t length = end - start + 1;
	
	if (length > INT_MAX)
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_nucleotides): the returned vector would exceed the maximum vector length in Eidos." << EidosTerminate();
	
	EidosValue_String *format_value = (EidosValue_String *)p_arguments[2].get();
	const std::string &format = format_value->StringRefAtIndex_NOCAST(0, nullptr);
	
	if (format == "codon")
	{
		EidosValue_SP codon_value = sequence->NucleotidesAsCodonVector(start, end, /* p_force_vector */ true);
		
		// patch the sequence with nucleotide mutations
		// no singleton case; we force a vector return from NucleotidesAsCodonVector() for simplicity
		int64_t *int_vec = ((EidosValue_Int *)(codon_value.get()))->data_mutable();
		HaplosomeWalker walker(this);
		
		walker.MoveToPosition(start);
		
		while (!walker.Finished())
		{
			Mutation *mut = walker.CurrentMutation();
			slim_position_t pos = mut->position_;
			
			// pos >= start is guaranteed by MoveToPosition()
			if (pos > end)
				break;
			
			int8_t nuc = mut->nucleotide_;
			
			if (nuc != -1)
			{
				// We have a nucleotide-based mutation within the sequence range.  We need to get the current codon value,
				// deconstruct it into nucleotides, replace the overlaid nucleotide, reconstruct the codon value, and put
				// it back into the codon vector.
				int64_t codon_index = (pos - start) / 3;
				int codon_offset = (pos - start) % 3;
				int codon = (int)int_vec[codon_index];
				
				if (codon_offset == 0)
				{
					codon = codon & 0x0F;
					codon |= (nuc * 16);
				}
				else if (codon_offset == 1)
				{
					codon = codon & 0x33;
					codon |= (nuc * 4);
				}
				else
				{
					codon = codon & 0x3C;
					codon |= nuc;
				}
				
				int_vec[codon_index] = codon;
			}
			
			walker.NextMutation();
		}
		
		return codon_value;
	}
	else if (format == "string")
	{
		EidosValue_SP string_value = sequence->NucleotidesAsStringSingleton(start, end);
		
		// patch the sequence with nucleotide mutations
		if (start == end)
		{
			// singleton case: replace string_value entirely
			HaplosomeWalker walker(this);
			
			walker.MoveToPosition(start);
			
			while (!walker.Finished())
			{
				Mutation *mut = walker.CurrentMutation();
				slim_position_t pos = mut->position_;
				
				// pos >= start is guaranteed by MoveToPosition()
				if (pos > end)
					break;
				
				int8_t nuc = mut->nucleotide_;
				
				if (nuc != -1)
				{
					if (nuc == 0)			string_value = gStaticEidosValue_StringA;
					else if (nuc == 1)		string_value = gStaticEidosValue_StringC;
					else if (nuc == 2)		string_value = gStaticEidosValue_StringG;
					else /* (nuc == 3) */	string_value = gStaticEidosValue_StringT;
				}
				
				walker.NextMutation();
			}
		}
		else
		{
			// vector case: replace the appropriate character in string_value
			std::string &string_string = ((EidosValue_String *)(string_value.get()))->StringData_Mutable()[0];
			char *string_ptr = &string_string[0];	// data() returns a const pointer, but this is safe in C++11 and later
			HaplosomeWalker walker(this);
			
			walker.MoveToPosition(start);
			
			while (!walker.Finished())
			{
				Mutation *mut = walker.CurrentMutation();
				slim_position_t pos = mut->position_;
				
				// pos >= start is guaranteed by MoveToPosition()
				if (pos > end)
					break;
				
				int8_t nuc = mut->nucleotide_;
				
				if (nuc != -1)
					string_ptr[pos - start] = gSLiM_Nucleotides[nuc];
				
				walker.NextMutation();
			}
		}
		
		return string_value;
	}
	else if (format == "integer")
	{
		EidosValue_SP integer_value = sequence->NucleotidesAsIntegerVector(start, end);
		
		// patch the sequence with nucleotide mutations
		if (start == end)
		{
			// singleton case: replace integer_value entirely
			HaplosomeWalker walker(this);
			
			walker.MoveToPosition(start);
			
			while (!walker.Finished())
			{
				Mutation *mut = walker.CurrentMutation();
				slim_position_t pos = mut->position_;
				
				// pos >= start is guaranteed by MoveToPosition()
				if (pos > end)
					break;
				
				int8_t nuc = mut->nucleotide_;
				
				if (nuc != -1)
				{
					if (nuc == 0)			integer_value = gStaticEidosValue_Integer0;
					else if (nuc == 1)		integer_value = gStaticEidosValue_Integer1;
					else if (nuc == 2)		integer_value = gStaticEidosValue_Integer2;
					else /* (nuc == 3) */	integer_value = gStaticEidosValue_Integer3;
				}
				
				walker.NextMutation();
			}
		}
		else
		{
			// vector case: replace the appropriate element in integer_value
			int64_t *int_vec = ((EidosValue_Int *)(integer_value.get()))->data_mutable();
			HaplosomeWalker walker(this);
			
			walker.MoveToPosition(start);
			
			while (!walker.Finished())
			{
				Mutation *mut = walker.CurrentMutation();
				slim_position_t pos = mut->position_;
				
				// pos >= start is guaranteed by MoveToPosition()
				if (pos > end)
					break;
				
				int8_t nuc = mut->nucleotide_;
				
				if (nuc != -1)
					int_vec[pos-start] = (int)nuc;
				
				walker.NextMutation();
			}
		}
		
		return integer_value;
	}
	else if (format == "char")
	{
		EidosValue_SP char_value = sequence->NucleotidesAsStringVector(start, end);
		
		// patch the sequence with nucleotide mutations
		if (start == end)
		{
			// singleton case: replace char_value entirely
			HaplosomeWalker walker(this);
			
			walker.MoveToPosition(start);
			
			while (!walker.Finished())
			{
				Mutation *mut = walker.CurrentMutation();
				slim_position_t pos = mut->position_;
				
				// pos >= start is guaranteed by MoveToPosition()
				if (pos > end)
					break;
				
				int8_t nuc = mut->nucleotide_;
				
				if (nuc != -1)
				{
					if (nuc == 0)			char_value = gStaticEidosValue_StringA;
					else if (nuc == 1)		char_value = gStaticEidosValue_StringC;
					else if (nuc == 2)		char_value = gStaticEidosValue_StringG;
					else /* (nuc == 3) */	char_value = gStaticEidosValue_StringT;
				}
				
				walker.NextMutation();
			}
		}
		else
		{
			// vector case: replace the appropriate element in char_value
			std::string *char_vec = char_value->StringData_Mutable();
			HaplosomeWalker walker(this);
			
			walker.MoveToPosition(start);
			
			while (!walker.Finished())
			{
				Mutation *mut = walker.CurrentMutation();
				slim_position_t pos = mut->position_;
				
				// pos >= start is guaranteed by MoveToPosition()
				if (pos > end)
					break;
				
				int8_t nuc = mut->nucleotide_;
				
				if (nuc != -1)
					char_vec[pos - start] = gSLiM_Nucleotides[nuc];
				
				walker.NextMutation();
			}
		}
		
		return char_value;
	}
	
	EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_nucleotides): parameter format must be either 'string', 'char', 'integer', or 'codon'." << EidosTerminate();
}

//	*********************	- (integer)positionsOfMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Haplosome::ExecuteMethod_positionsOfMutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	
	if (IsDeferred())
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_positionsOfMutationsOfType): the mutations of deferred haplosomes cannot be accessed." << EidosTerminate();
	if (IsNull())
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_positionsOfMutationsOfType): positionsOfMutationsOfType() cannot be called on a null haplosome." << EidosTerminate();
	
	Species &species = individual_->subpopulation_->species_;
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &species.community_, &species, "positionsOfMutationsOfType()");		// SPECIES CONSISTENCY CHECK
	
	// Return the positions of mutations of the given type
	EidosValue_Int *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int();
	Mutation *mut_block_ptr = species.SpeciesMutationBlock()->mutation_buffer_;
	
	for (int run_index = 0; run_index < mutrun_count_; ++run_index)
	{
		const MutationRun *mutrun = mutruns_[run_index];
		int mut_count = mutrun->size();
		const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
		
		for (int mut_index = 0; mut_index < mut_count; ++mut_index)
		{
			Mutation *mutation = mut_block_ptr + mut_ptr[mut_index];
			
			if (mutation->mutation_type_ptr_ == mutation_type_ptr)
				int_result->push_int(mutation->position_);
		}
	}
	
	return EidosValue_SP(int_result);
}

//	*********************	- (integer$)sumOfMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Haplosome::ExecuteMethod_sumOfMutationsOfType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	
	if (IsDeferred())
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_sumOfMutationsOfType): the mutations of deferred haplosomes cannot be accessed." << EidosTerminate();
	if (IsNull())
		EIDOS_TERMINATION << "ERROR (Haplosome::ExecuteMethod_sumOfMutationsOfType): sumOfMutationsOfType() cannot be called on a null haplosome." << EidosTerminate();
	
	Species &species = individual_->subpopulation_->species_;
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &species.community_, &species, "sumOfMutationsOfType()");		// SPECIES CONSISTENCY CHECK
	
	// Sum the selection coefficients of mutations of the given type
	Mutation *mut_block_ptr = species.SpeciesMutationBlock()->mutation_buffer_;
	double selcoeff_sum = 0.0;
	int mutrun_count = mutrun_count_;
	
	for (int run_index = 0; run_index < mutrun_count; ++run_index)
	{
		const MutationRun *mutrun = mutruns_[run_index];
		int haplosome1_count = mutrun->size();
		const MutationIndex *haplosome_ptr = mutrun->begin_pointer_const();
		
		for (int mut_index = 0; mut_index < haplosome1_count; ++mut_index)
		{
			Mutation *mut_ptr = mut_block_ptr + haplosome_ptr[mut_index];
			
			if (mut_ptr->mutation_type_ptr_ == mutation_type_ptr)
				selcoeff_sum += mut_ptr->selection_coeff_;
		}
	}
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(selcoeff_sum));
}

// print the sample represented by haplosomes, using SLiM's own format
void Haplosome::PrintHaplosomes_SLiM(std::ostream &p_out, Species &p_species, std::vector<Haplosome *> &p_haplosomes, bool p_output_object_tags)
{
	Mutation *mut_block_ptr = p_species.SpeciesMutationBlock()->mutation_buffer_;
	slim_popsize_t sample_size = (slim_popsize_t)p_haplosomes.size();
	
	// get the polymorphisms within the sample
	PolymorphismMap polymorphisms;
	
	for (slim_popsize_t s = 0; s < sample_size; s++)
	{
		Haplosome &haplosome = *p_haplosomes[s];
		
		if (haplosome.IsNull())
			EIDOS_TERMINATION << "ERROR (Haplosome::PrintHaplosomes_SLiM): cannot output null haplosomes." << EidosTerminate();
		
		for (int run_index = 0; run_index < haplosome.mutrun_count_; ++run_index)
		{
			const MutationRun *mutrun = haplosome.mutruns_[run_index];
			int mut_count = mutrun->size();
			const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
			
			for (int mut_index = 0; mut_index < mut_count; ++mut_index)
				AddMutationToPolymorphismMap(&polymorphisms, mut_block_ptr + mut_ptr[mut_index]);
		}
	}
	
	// print the sample's polymorphisms; NOTE the output format changed due to the addition of mutation_id_, BCH 11 June 2016
	// NOTE the output format changed due to the addition of the nucleotide, BCH 2 March 2019
	p_out << "Mutations:"  << std::endl;
	
	for (const PolymorphismPair &polymorphism_pair : polymorphisms)
	{
		if (p_output_object_tags)
			polymorphism_pair.second.Print_ID_Tag(p_out);
		else
			polymorphism_pair.second.Print_ID(p_out);
	}
	
	// print the sample's haplosomes
	p_out << "Haplosomes:" << std::endl;
	
	for (slim_popsize_t j = 0; j < sample_size; j++)														// go through all haplosomes
	{
		Haplosome &haplosome = *p_haplosomes[j];
		Individual *individual = haplosome.individual_;
		
		if (!individual)
			EIDOS_TERMINATION << "ERROR (Haplosome::PrintHaplosomes_SLiM): (internal error) missing individual for haplosome." << EidosTerminate();
		
		slim_popsize_t index = individual->index_;
		
		if (index == -1)
			EIDOS_TERMINATION << "ERROR (Haplosome::PrintHaplosomes_SLiM): haplosomes being output must be visible in a subpopulation (i.e., may not belong to new juveniles)." << EidosTerminate();
		
		Subpopulation *subpop = individual->subpopulation_;
		
		if (!subpop)
			EIDOS_TERMINATION << "ERROR (Haplosome::PrintHaplosomes_SLiM): (internal error) missing subpopulation for individual." << EidosTerminate();
		
		// BCH 2/9/2025: For SLiM 5, we now print the subpopulation id and the
		// individual index for the haplosome, telling the user where each
		// haplosome came from; probably not useful, but more useful than before
		p_out << 'p' << subpop->subpopulation_id_ << ":i" << index;
		
		if (p_output_object_tags)
		{
			if (haplosome.tag_value_ == SLIM_TAG_UNSET_VALUE)
				p_out << " ?";
			else
				p_out << ' ' << haplosome.tag_value_;
		}
		
		for (int run_index = 0; run_index < haplosome.mutrun_count_; ++run_index)
		{
			const MutationRun *mutrun = haplosome.mutruns_[run_index];
			int mut_count = mutrun->size();
			const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
			
			for (int mut_index = 0; mut_index < mut_count; ++mut_index)
			{
				slim_polymorphismid_t polymorphism_id = FindMutationInPolymorphismMap(polymorphisms, mut_block_ptr + mut_ptr[mut_index]);
				
				if (polymorphism_id == -1)
					EIDOS_TERMINATION << "ERROR (Haplosome::PrintHaplosomes_SLiM): (internal error) polymorphism not found." << EidosTerminate();
				
				p_out << " " << polymorphism_id;
			}
		}
		
		p_out << std::endl;
	}
}

// print the sample represented by haplosomes, using "ms" format
void Haplosome::PrintHaplosomes_MS(std::ostream &p_out, Species &p_species, std::vector<Haplosome *> &p_haplosomes, const Chromosome &p_chromosome, bool p_filter_monomorphic)
{
	Mutation *mut_block_ptr = p_species.SpeciesMutationBlock()->mutation_buffer_;
	slim_popsize_t sample_size = (slim_popsize_t)p_haplosomes.size();
	
	// BCH 7 Nov. 2016: sort the polymorphisms by position since that is the expected sort
	// order in MS output.  In other types of output, sorting by the mutation id seems to
	// be fine.
	std::vector<Polymorphism> sorted_polymorphisms;
	
	{
		// get the polymorphisms within the sample
		PolymorphismMap polymorphisms;
		
		for (slim_popsize_t s = 0; s < sample_size; s++)
		{
			Haplosome &haplosome = *p_haplosomes[s];
			
			if (haplosome.IsNull())
				EIDOS_TERMINATION << "ERROR (Haplosome::PrintHaplosomes_MS): cannot output null haplosomes." << EidosTerminate();
			
			for (int run_index = 0; run_index < haplosome.mutrun_count_; ++run_index)
			{
				const MutationRun *mutrun = haplosome.mutruns_[run_index];
				int mut_count = mutrun->size();
				const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
				
				for (int mut_index = 0; mut_index < mut_count; ++mut_index)
					AddMutationToPolymorphismMap(&polymorphisms, mut_block_ptr + mut_ptr[mut_index]);
			}
		}
		
		for (const PolymorphismPair &polymorphism_pair : polymorphisms)
			sorted_polymorphisms.emplace_back(polymorphism_pair.second);
		
		std::sort(sorted_polymorphisms.begin(), sorted_polymorphisms.end());
	}
	
	// if requested, remove polymorphisms that are not polymorphic within the sample
	if (p_filter_monomorphic)
	{
		std::vector<Polymorphism> filtered_polymorphisms;
		
		for (Polymorphism &p : sorted_polymorphisms)
			if (p.prevalence_ != sample_size)
				filtered_polymorphisms.emplace_back(p);
		
		std::swap(sorted_polymorphisms, filtered_polymorphisms);
	}
	
	// make a hash table that looks up the genotype string position from a mutation pointer
#if EIDOS_ROBIN_HOOD_HASHING
	robin_hood::unordered_flat_map<const Mutation*, int> genotype_string_positions;
	//typedef robin_hood::pair<const Mutation*, int> MAP_PAIR;
#elif STD_UNORDERED_MAP_HASHING
	std::unordered_map<const Mutation*, int> genotype_string_positions;
	//typedef std::pair<const Mutation*, int> MAP_PAIR;
#endif
	int genotype_string_position = 0;
	
	try {
		for (const Polymorphism &polymorphism : sorted_polymorphisms) 
			genotype_string_positions.emplace(polymorphism.mutation_ptr_, genotype_string_position++);
	} catch (...) {
		EIDOS_TERMINATION << "ERROR (Haplosome::PrintHaplosomes_MS): (internal error) SLiM encountered a raise from an internal hash table; please report this." << EidosTerminate(nullptr);
	}
	
	// print header
	p_out << "//" << std::endl << "segsites: " << sorted_polymorphisms.size() << std::endl;
	
	// print the sample's positions
	if (sorted_polymorphisms.size() > 0)
	{
		// Save flags/precision
		std::ios_base::fmtflags oldflags = p_out.flags();
		std::streamsize oldprecision = p_out.precision();
		
		p_out << std::fixed << std::setprecision(15);	// BCH 26 Jan. 2020: increasing this from 7 to 10, so longer chromosomes work; maybe this should be a parameter?
														// BCH 23 July 2020: increasing from 10 to 15, which is the limit of double-precision floats anyway
		
		// Output positions
		p_out << "positions:";
		
		for (const Polymorphism &polymorphism : sorted_polymorphisms) 
			p_out << " " << static_cast<double>(polymorphism.mutation_ptr_->position_) / p_chromosome.last_position_;	// this prints positions as being in the interval [0,1], which Philipp decided was the best policy
		
		p_out << std::endl;
		
		// Restore flags/precision
		p_out.flags(oldflags);
		p_out.precision(oldprecision);
	}
	
	// print the sample's genotypes
	for (slim_popsize_t j = 0; j < sample_size; j++)														// go through all individuals
	{
		Haplosome &haplosome = *p_haplosomes[j];
		std::string genotype(sorted_polymorphisms.size(), '0'); // fill with 0s
		
		for (int run_index = 0; run_index < haplosome.mutrun_count_; ++run_index)
		{
			const MutationRun *mutrun = haplosome.mutruns_[run_index];
			int mut_count = mutrun->size();
			const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
			
			for (int mut_index = 0; mut_index < mut_count; ++mut_index)
			{
				const Mutation *mutation = mut_block_ptr + mut_ptr[mut_index];
				auto found_position = genotype_string_positions.find(mutation);
				
				// BCH 4/24/2019: when p_filter_monomorphic is true, mutations in a given haplosome may not exist in the position map
				if (found_position != genotype_string_positions.end())
					genotype.replace(found_position->second, 1, "1");
			}
		}
		
		p_out << genotype << std::endl;
	}
}

inline void EmitHaplosomeCall_Nuc_Simplify(std::ostream &p_out, const Haplosome &haplosome, std::vector<Polymorphism *> &nuc_based, slim_position_t mut_position, int *allele_index_for_nuc)
{
	// Find and emit the nuc-based mut contained by this haplosome, if any.  If more than one nuc-based mut is contained, it is an error.
	int contained_mut_index = -1;
	
	for (int muts_index = 0; muts_index < (int)nuc_based.size(); ++muts_index)
	{
		const Mutation *mutation = nuc_based[muts_index]->mutation_ptr_;
		
		if (haplosome.contains_mutation(mutation))
		{
			if (contained_mut_index == -1)
				contained_mut_index = muts_index;
			else
				EIDOS_TERMINATION << "ERROR (EmitHaplosomeCall_Nuc): more than one nucleotide-based mutation encountered at the same position (" << mut_position << ") in the same haplosome; the nucleotide cannot be called." << EidosTerminate();
		}
	}
	
	if (contained_mut_index == -1)
		p_out << '0';
	else
		p_out << allele_index_for_nuc[nuc_based[contained_mut_index]->mutation_ptr_->nucleotide_];
}

inline void EmitHaplosomeCall_Nuc(std::ostream &p_out, const Haplosome &haplosome, std::vector<Polymorphism *> &nuc_based, slim_position_t mut_position)
{
	// Find and emit the nuc-based mut contained by this haplosome, if any.  If more than one nuc-based mut is contained, it is an error.
	int contained_mut_index = -1;
	
	for (int muts_index = 0; muts_index < (int)nuc_based.size(); ++muts_index)
	{
		const Mutation *mutation = nuc_based[muts_index]->mutation_ptr_;
		
		if (haplosome.contains_mutation(mutation))
		{
			if (contained_mut_index == -1)
				contained_mut_index = muts_index;
			else
				EIDOS_TERMINATION << "ERROR (EmitHaplosomeCall_Nuc): more than one nucleotide-based mutation encountered at the same position (" << mut_position << ") in the same haplosome; the nucleotide cannot be called." << EidosTerminate();
		}
	}
	
	if (contained_mut_index == -1)
		p_out << '0';
	else
		p_out << (contained_mut_index + 1);
}

// print the sample represented by haplosomes, using "vcf" format
// the haplosomes will all belong to a single chromosome, p_chromosome, and may include null haplosomes
// depending on the intrinsic ploidy of p_chromosome the calls will be diploid or haploid; if diploid,
// calls where one of a pair of haplosomes is null will be emitted as a haploid call; if all haplosomes
// for a given individual are null, the call emitted will be "~".
void Haplosome::PrintHaplosomes_VCF(std::ostream &p_out, std::vector<Haplosome *> &p_haplosomes, const Chromosome &p_chromosome, bool p_groupAsIndividuals, bool p_output_multiallelics, bool p_simplify_nucs, bool p_output_nonnucs)
{
	Species &species = p_chromosome.species_;
	bool nucleotide_based = species.IsNucleotideBased();
	bool pedigrees_enabled = species.PedigreesEnabledByUser();
	slim_popsize_t haplosome_count = (slim_popsize_t)p_haplosomes.size();
	slim_popsize_t individual_count;
	
	// get information about the chromosome we're writing, which determines whether an "individual" in the VCF is one haplosome or two
	ChromosomeType chromosome_type = p_chromosome.Type();
	int intrinsic_ploidy = p_chromosome.IntrinsicPloidy();
	
	if (!p_groupAsIndividuals)
		intrinsic_ploidy = 1;	// if groupAsIndividuals is false, we just act as though the chromosome is haploid
	
	if (intrinsic_ploidy == 2)
	{
		if (haplosome_count % 2 == 1)
			EIDOS_TERMINATION << "ERROR (Haplosome::PrintHaplosomes_VCF): Haplosome vector must be an even length for chromosome type \"" << chromosome_type << "\", since haplosomes are paired into individuals." << EidosTerminate();
		
		individual_count = haplosome_count / 2;
	}
	else
	{
		individual_count = haplosome_count;
	}
	
	// print the VCF header
	p_out << "##fileformat=VCFv4.2" << std::endl;
	
	{
		time_t rawtime;
		struct tm timeinfo;
		char buffer[25];	// should never be more than 10, in fact, plus a null
		
		time(&rawtime);
		localtime_r(&rawtime, &timeinfo);
		strftime(buffer, 25, "%Y%m%d", &timeinfo);
		
		p_out << "##fileDate=" << std::string(buffer) << std::endl;
	}
	
	p_out << "##source=SLiM" << std::endl;
	
	// BCH 10 July 2019: output haplosome pedigree IDs, if available, for all of the haplosomes being output.
	// It would be nice to be able to output individual pedigree IDs, but since we are working with a
	// vector of haplosomes there is no guarantee that the pairs of haplosomes here come from the same individuals.
	if (pedigrees_enabled && (haplosome_count > 0))
	{
		p_out << "##slimHaplosomePedigreeIDs=";
		
		for (slim_popsize_t haplosome_index = 0; haplosome_index < haplosome_count; haplosome_index++)
		{
			if (haplosome_index > 0)
				p_out << ",";
			p_out << p_haplosomes[haplosome_index]->haplosome_id_;
		}
		
		p_out << std::endl;
	}
	
	// BCH 6 March 2019: Note that all of the INFO fields that provide per-mutation information have been
	// changed from a Number of 1 to a Number of ., since in nucleotide-based models we can call more than
	// one allele in a single call line (unlike in non-nucleotide-based models).
	p_out << "##INFO=<ID=MID,Number=.,Type=Integer,Description=\"Mutation ID in SLiM\">" << std::endl;
	p_out << "##INFO=<ID=S,Number=.,Type=Float,Description=\"Selection Coefficient\">" << std::endl;
	p_out << "##INFO=<ID=DOM,Number=.,Type=Float,Description=\"Dominance\">" << std::endl;
	// Note that at present we do not output the hemizygous dominance coefficient; too edge
	p_out << "##INFO=<ID=PO,Number=.,Type=Integer,Description=\"Population of Origin\">" << std::endl;
	p_out << "##INFO=<ID=TO,Number=.,Type=Integer,Description=\"Tick of Origin\">" << std::endl;			// changed to ticks for 4.0, and changed "GO" to "TO"
	p_out << "##INFO=<ID=MT,Number=.,Type=Integer,Description=\"Mutation Type\">" << std::endl;
	p_out << "##INFO=<ID=AC,Number=.,Type=Integer,Description=\"Allele Count\">" << std::endl;
	p_out << "##INFO=<ID=DP,Number=1,Type=Integer,Description=\"Total Depth\">" << std::endl;
	if (p_output_multiallelics && !nucleotide_based)
		p_out << "##INFO=<ID=MULTIALLELIC,Number=0,Type=Flag,Description=\"Multiallelic\">" << std::endl;
	if (nucleotide_based)
		p_out << "##INFO=<ID=AA,Number=1,Type=String,Description=\"Ancestral Allele\">" << std::endl;
	if (p_output_nonnucs && nucleotide_based)
		p_out << "##INFO=<ID=NONNUC,Number=0,Type=Flag,Description=\"Non-nucleotide-based\">" << std::endl;
	p_out << "##FORMAT=<ID=GT,Number=1,Type=String,Description=\"Genotype\">" << std::endl;
	p_out << "##contig=<ID=1,URL=https://github.com/MesserLab/SLiM>" << std::endl;
	p_out << "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT";
	
	for (slim_popsize_t individual_index = 0; individual_index < individual_count; individual_index++)
		p_out << "\ti" << individual_index;
	p_out << std::endl;
	
	Haplosome::_PrintVCF(p_out, (const Haplosome **)p_haplosomes.data(), haplosome_count, p_chromosome, p_groupAsIndividuals, p_simplify_nucs, p_output_nonnucs, p_output_multiallelics);
}

void Haplosome::_PrintVCF(std::ostream &p_out, const Haplosome **p_haplosomes, int64_t p_haplosomes_count, const Chromosome &p_chromosome, bool p_groupAsIndividuals, bool p_simplify_nucs, bool p_output_nonnucs, bool p_output_multiallelics)
{
	ChromosomeType chromosome_type = p_chromosome.Type();
	int intrinsic_ploidy = p_chromosome.IntrinsicPloidy();
	Species &species = p_chromosome.species_;
	bool nucleotide_based = species.IsNucleotideBased();
	NucleotideArray *ancestral_seq = p_chromosome.AncestralSequence();
	Mutation *mut_block_ptr = species.SpeciesMutationBlock()->mutation_buffer_;
	int64_t individual_count;
	
	// if groupAsIndividuals is false, we just act as though the chromosome is haploid
	// this option is not available for Individual::PrintIndividuals_VCF() since it doesn't
	// make sense for individual-based output, only single-chromosome haplosome-based output
	if (!p_groupAsIndividuals)
		intrinsic_ploidy = 1;
	
	if (intrinsic_ploidy == 2)
	{
		if (p_haplosomes_count % 2 == 1)
			EIDOS_TERMINATION << "ERROR (Haplosome::_PrintVCF): Haplosome vector must be an even length for chromosome type \"" << chromosome_type << "\", since haplosomes are paired into individuals." << EidosTerminate();
		
		individual_count = p_haplosomes_count / 2;
	}
	else
	{
		individual_count = p_haplosomes_count;
	}
	
	// get the polymorphisms within the sample
	PolymorphismMap polymorphisms;
	
	for (slim_popsize_t haplosome_index = 0; haplosome_index < p_haplosomes_count; haplosome_index++)
	{
		const Haplosome &haplosome = *p_haplosomes[haplosome_index];
		
		if (!haplosome.IsNull())
		{
			for (int run_index = 0; run_index < haplosome.mutrun_count_; ++run_index)
			{
				const MutationRun *mutrun = haplosome.mutruns_[run_index];
				int mut_count = mutrun->size();
				const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
				
				for (int mut_index = 0; mut_index < mut_count; ++mut_index)
					AddMutationToPolymorphismMap(&polymorphisms, mut_block_ptr + mut_ptr[mut_index]);
			}
		}
	}
	
	// We want to output polymorphisms sorted by position (starting in SLiM 3.3), to facilitate
	// calling all of the nucleotide mutations at a given position with a single call line.
	std::vector<Polymorphism> sorted_polymorphisms;
	
	for (const PolymorphismPair &polymorphism_pair : polymorphisms)
		sorted_polymorphisms.emplace_back(polymorphism_pair.second);
	
	std::sort(sorted_polymorphisms.begin(), sorted_polymorphisms.end());
	
	// Print a line for each mutation.  Note that we do NOT treat multiple mutations at the same position at being different alleles,
	// output on the same line.  This is because a single individual can carry more than one mutation at the same position, so it is
	// not really a question of different alleles; if there are N mutations at a given position, there are 2^N possible "alleles",
	// which is just silly to try to wedge into VCF format.  So instead, we output each mutation as a separate line, and we tag lines
	// for positions that carry more than one mutation with the MULTIALLELIC flag so they can be filtered out if they bother the user.
	// BCH 6 March 2019: The above comment remains true in non-nucleotide-based models.  In nucleotide-based models, the nucleotide-
	// based mutations at a given position are all output as a single call line, and then any non-nucleotide-based mutations are
	// emitted as separated call lines after that that are marked NONNUC (unless p_output_nonnucs is false, in which case they are
	// simply suppressed).
	for (auto polyiter = sorted_polymorphisms.begin(); polyiter != sorted_polymorphisms.end(); )
	{
		// Assemble vectors of all the nuc-based and non-nuc-based mutations at this position; we will emit them all at once
		std::vector<Polymorphism *> nuc_based, nonnuc_based;
		slim_position_t mut_position = polyiter->mutation_ptr_->position_;
		
		while (true)
		{
			// Eat polymorphism entries in sorted_polymorphisms as long as they're at the same position, until the end
			Polymorphism &polymorphism = *polyiter;
			const Mutation *mutation = polyiter->mutation_ptr_;
			
			if (mutation->position_ == mut_position)
			{
				if (mutation->mutation_type_ptr_->nucleotide_based_)
					nuc_based.emplace_back(&polymorphism);
				else
					nonnuc_based.emplace_back(&polymorphism);
			}
			else
			{
				break;
			}
			
			// Next polymorphism
			polyiter++;
			if (polyiter == sorted_polymorphisms.end())
				break;
		}
		
		// Emit the nucleotide-based mutations at this position as a single call line
		if (nucleotide_based && (nuc_based.size() > 0))
		{
			// Get the ancestral nucleotide at this position; this will be index 0
			// Indices 1..n will be used for the corresponding mutations in nonnuc_based
			// Note that this means it is 
			int ancestral_nuc_index = ancestral_seq->NucleotideAtIndex(mut_position);		// 0..3 for ACGT
			
			// Emit a single call line for all of the nucleotide-based mutations
			
			if (p_simplify_nucs)
			{
				// We are requested to simplify the nucleotide state; any mutations with the ancestral nucleotide will be considered part of the
				// ancestral state, and any mutations with matching nucleotide will be lumped together; SLiM state will not be emitted
				// We tally up the total prevalence of each nucleotide, ignoring the ancestral nucleotide.
				slim_refcount_t total_prevalence[4] = {0, 0, 0, 0};
				int allele_index_for_nuc[4] = {-1, -1 -1, -1};
				
				for (Polymorphism *polymorphism : nuc_based)
				{
					int derived_nuc_index = (unsigned char)polymorphism->mutation_ptr_->nucleotide_;
					
					if (derived_nuc_index != ancestral_nuc_index)
						total_prevalence[derived_nuc_index] += polymorphism->prevalence_;
				}
				
				// Assign genotype call indexes for the four nucleotides, based upon which ones have prevalence > 0
				allele_index_for_nuc[ancestral_nuc_index] = 0;	// emit 0 for any mutations with a back-mutation
				
				int next_allele_index = 1;	// 0 is ancestral
				for (int nuc_index = 0; nuc_index < 4; ++nuc_index)
				{
					if (total_prevalence[nuc_index] > 0)
						allele_index_for_nuc[nuc_index] = next_allele_index++;
				}
				
				// If the only segregating alleles are back-mutations, we don't need to emit this call line at all
				if (total_prevalence[0] + total_prevalence[1] + total_prevalence[2] + total_prevalence[3] != 0)
				{
					// emit CHROM ("1"), POS, ID (".")
					// BCH 2/3/2025: we now emit the chromosome's symbol in the CHROM field, introducing a minor
					// backward compatibility break; it used to be "1" by default, now it is "A" by default, but
					// this is easy to fix by calling initializeChromosome() explicitly and supplying symbol="1"
					p_out << p_chromosome.Symbol() << "\t" << (mut_position + 1) << "\t.\t";			// +1 because VCF uses 1-based positions
					
					// emit REF ("A" etc.)
					p_out << gSLiM_Nucleotides[ancestral_nuc_index];
					p_out << "\t";
					
					// emit ALT ("T" etc.)
					bool firstEmitted = true;
					for (int nuc_index=0; nuc_index < 4; ++nuc_index)
					{
						if (total_prevalence[nuc_index] > 0)
						{
							if (!firstEmitted)
								p_out << ',';
							firstEmitted = false;
							
							p_out << gSLiM_Nucleotides[nuc_index];
						}
					}
					
					// emit QUAL (1000), FILTER (PASS)
					p_out << "\t1000\tPASS\t";
					
					// emit the INFO fields and the Genotype marker; note mutation-specific fields are omitted since we are aggregating
					p_out << "AC=";
					firstEmitted = true;
					for (int nuc_prevalence : total_prevalence)
					{
						if (nuc_prevalence > 0)
						{
							if (!firstEmitted)
								p_out << ',';
							firstEmitted = false;
							
							p_out << nuc_prevalence;
						}
					}
					p_out << ";DP=1000;";
					p_out << "AA=" << gSLiM_Nucleotides[ancestral_nuc_index];
					
					p_out << "\tGT";
					
					// emit the individual calls
					if (intrinsic_ploidy == 1)
					{
						// intrinsically haploid chromosome; one haplosome per individual
						for (slim_popsize_t individual_index = 0; individual_index < individual_count; individual_index++)
						{
							const Haplosome &haplosome = *p_haplosomes[individual_index];
							
							// BCH 2/4/2025: If the haplosome is null, we now emit a "~" character
							if (haplosome.IsNull()) {
								p_out << "\t~";
								continue;
							}
							
							// haploid call
							p_out << '\t';
							
							EmitHaplosomeCall_Nuc_Simplify(p_out, haplosome, nuc_based, mut_position, allele_index_for_nuc);
						}
					}
					else
					{
						// intrinsically diploid chromosome; two haplosomes per individual
						for (slim_popsize_t individual_index = 0; individual_index < individual_count; individual_index++)
						{
							const Haplosome &haplosome1 = *p_haplosomes[(size_t)individual_index * 2];
							const Haplosome &haplosome2 = *p_haplosomes[(size_t)individual_index * 2 + 1];
							bool haplosome1_null = haplosome1.IsNull(), haplosome2_null = haplosome2.IsNull();
							
							// BCH 2/4/2025: If both haplosomes are null, we now emit a "~" character
							if (haplosome1_null && haplosome2_null) {
								p_out << "\t~";
								continue;
							}
							
							// diploid call unless hemizygous, producing a haploid call (and losing which haplosome was null)
							p_out << '\t';
							
							if (!haplosome1_null)
								EmitHaplosomeCall_Nuc_Simplify(p_out, haplosome1, nuc_based, mut_position, allele_index_for_nuc);
							
							if (!haplosome1_null && !haplosome2_null)	// emit a separator for a diploid call
								p_out << '|';
							
							if (!haplosome2_null)
								EmitHaplosomeCall_Nuc_Simplify(p_out, haplosome2, nuc_based, mut_position, allele_index_for_nuc);
						}
					}
					
					p_out << std::endl;
				}
			}
			else
			{
				// emit CHROM ("1"), POS, ID (".")
				// BCH 2/3/2025: we now emit the chromosome's symbol in the CHROM field, introducing a minor
				// backward compatibility break; it used to be "1" by default, now it is "A" by default, but
				// this is easy to fix by calling initializeChromosome() explicitly and supplying symbol="1"
				p_out << p_chromosome.Symbol() << "\t" << (mut_position + 1) << "\t.\t";			// +1 because VCF uses 1-based positions
				
				// emit REF ("A" etc.)
				p_out << gSLiM_Nucleotides[ancestral_nuc_index];
				p_out << "\t";
				
				// emit ALT ("T" etc.)
				for (Polymorphism *polymorphism : nuc_based)
				{
					if (polymorphism != nuc_based.front())
						p_out << ',';
					p_out << gSLiM_Nucleotides[polymorphism->mutation_ptr_->nucleotide_];
				}
				
				// emit QUAL (1000), FILTER (PASS)
				p_out << "\t1000\tPASS\t";
				
				// emit the INFO fields and the Genotype marker
				p_out << "MID=";
				for (Polymorphism *polymorphism : nuc_based)
				{
					if (polymorphism != nuc_based.front())
						p_out << ',';
					p_out << polymorphism->mutation_ptr_->mutation_id_;
				}
				p_out << ";";
				
				p_out << "S=";
				for (Polymorphism *polymorphism : nuc_based)
				{
					if (polymorphism != nuc_based.front())
						p_out << ',';
					p_out << polymorphism->mutation_ptr_->selection_coeff_;
				}
				p_out << ";";
				
				p_out << "DOM=";
				for (Polymorphism *polymorphism : nuc_based)
				{
					if (polymorphism != nuc_based.front())
						p_out << ',';
					p_out << polymorphism->mutation_ptr_->dominance_coeff_;
				}
				p_out << ";";
				
				p_out << "PO=";
				for (Polymorphism *polymorphism : nuc_based)
				{
					if (polymorphism != nuc_based.front())
						p_out << ',';
					p_out << polymorphism->mutation_ptr_->subpop_index_;
				}
				p_out << ";";
				
				p_out << "TO=";
				for (Polymorphism *polymorphism : nuc_based)
				{
					if (polymorphism != nuc_based.front())
						p_out << ',';
					p_out << polymorphism->mutation_ptr_->origin_tick_;
				}
				p_out << ";";
				
				p_out << "MT=";
				for (Polymorphism *polymorphism : nuc_based)
				{
					if (polymorphism != nuc_based.front())
						p_out << ',';
					p_out << polymorphism->mutation_ptr_->mutation_type_ptr_->mutation_type_id_;
				}
				p_out << ";";
				
				p_out << "AC=";
				for (Polymorphism *polymorphism : nuc_based)
				{
					if (polymorphism != nuc_based.front())
						p_out << ',';
					p_out << polymorphism->prevalence_;
				}
				p_out << ";";
				
				p_out << "DP=1000;";
				
				p_out << "AA=" << gSLiM_Nucleotides[ancestral_nuc_index];
				
				p_out << "\tGT";
				
				// emit the individual calls
				if (intrinsic_ploidy == 1)
				{
					// intrinsically haploid chromosome; one haplosome per individual
					for (slim_popsize_t individual_index = 0; individual_index < individual_count; individual_index++)
					{
						const Haplosome &haplosome = *p_haplosomes[individual_index];
						
						// BCH 2/4/2025: If the haplosome is null, we now emit a "~" character
						if (haplosome.IsNull()) {
							p_out << "\t~";
							continue;
						}
						
						// haploid call
						p_out << '\t';
						
						EmitHaplosomeCall_Nuc(p_out, haplosome, nuc_based, mut_position);
					}
				}
				else
				{
					// intrinsically diploid chromosome; two haplosomes per individual
					for (slim_popsize_t individual_index = 0; individual_index < individual_count; individual_index++)
					{
						const Haplosome &haplosome1 = *p_haplosomes[(size_t)individual_index * 2];
						const Haplosome &haplosome2 = *p_haplosomes[(size_t)individual_index * 2 + 1];
						bool haplosome1_null = haplosome1.IsNull(), haplosome2_null = haplosome2.IsNull();
						
						// BCH 2/4/2025: If both haplosomes are null, we now emit a "~" character
						if (haplosome1_null && haplosome2_null) {
							p_out << "\t~";
							continue;
						}
						
						// diploid call unless hemizygous, producing a haploid call (and losing which haplosome was null)
						p_out << '\t';
						
						if (!haplosome1.IsNull())
							EmitHaplosomeCall_Nuc(p_out, haplosome1, nuc_based, mut_position);
						
						if (!haplosome1_null && !haplosome2_null)	// emit a separator for a diploid call
							p_out << '|';
						
						if (!haplosome2.IsNull())
							EmitHaplosomeCall_Nuc(p_out, haplosome2, nuc_based, mut_position);
					}
				}
				
				p_out << std::endl;
			}
		}
		
		// Emit the non-nucleotide-based mutations at this position as individual call lines, each as an A->T mutation
		// We do this if outputNonnucleotides==T, or if we are non-nucleotide-based (in which case outputNonnucleotides is ignored)
		if (p_output_nonnucs || !nucleotide_based)
		{
			for (Polymorphism *polymorphism : nonnuc_based)
			{
				const Mutation *mutation = polymorphism->mutation_ptr_;
				
				// Count the mutations at the given position to determine if we are multiallelic
				int allele_count = (int)nonnuc_based.size();
				
				// Output this mutation if (1) we are outputting multiallelics in a non-nuc-based model, or (2) we are a nuc-based model (regardless of allele count), or (3) it is not multiallelic
				if (p_output_multiallelics || nucleotide_based || (allele_count == 1))
				{
					// emit CHROM ("1"), POS, ID ("."), REF ("A"), and ALT ("T")
					// BCH 2/3/2025: we now emit the chromosome's symbol in the CHROM field, introducing a minor
					// backward compatibility break; it used to be "1" by default, now it is "A" by default, but
					// this is easy to fix by calling initializeChromosome() explicitly and supplying symbol="1"
					p_out << p_chromosome.Symbol() << "\t" << (mut_position + 1) << "\t.\tA\tT";			// +1 because VCF uses 1-based positions
					
					// emit QUAL (1000), FILTER (PASS)
					p_out << "\t1000\tPASS\t";
					
					// emit the INFO fields and the Genotype marker
					p_out << "MID=" << mutation->mutation_id_ << ";";
					p_out << "S=" << mutation->selection_coeff_ << ";";
					p_out << "DOM=" << mutation->dominance_coeff_ << ";";
					p_out << "PO=" << mutation->subpop_index_ << ";";
					p_out << "TO=" << mutation->origin_tick_ << ";";
					p_out << "MT=" << mutation->mutation_type_ptr_->mutation_type_id_ << ";";
					p_out << "AC=" << polymorphism->prevalence_ << ";";
					p_out << "DP=1000";
					
					if (!nucleotide_based && (allele_count > 1))	// output MULTIALLELIC flags only in non-nuc-based models
						p_out << ";MULTIALLELIC";
					if (nucleotide_based && p_output_nonnucs)
						p_out << ";NONNUC";
					
					p_out << "\tGT";
					
					// emit the individual calls
					if (intrinsic_ploidy == 1)
					{
						for (slim_popsize_t individual_index = 0; individual_index < individual_count; individual_index++)
						{
							const Haplosome &haplosome = *p_haplosomes[individual_index];
							
							// BCH 2/4/2025: If the haplosome is null, we now emit a "~" character
							if (haplosome.IsNull()) {
								p_out << "\t~";
								continue;
							}
							
							// haploid call
							p_out << (haplosome.contains_mutation(mutation) ? "\t1" : "\t0");
						}
					}
					else
					{
						for (slim_popsize_t individual_index = 0; individual_index < individual_count; individual_index++)
						{
							const Haplosome &haplosome1 = *p_haplosomes[(size_t)individual_index * 2];
							const Haplosome &haplosome2 = *p_haplosomes[(size_t)individual_index * 2 + 1];
							bool haplosome1_null = haplosome1.IsNull(), haplosome2_null = haplosome2.IsNull();
							
							// BCH 2/4/2025: If both haplosomes are null, we now emit a "~" character
							if (haplosome1_null && haplosome2_null) {
								p_out << "\t~";
								continue;
							}
							else if (haplosome1_null)
							{
								// hemizygous; we emit this as haploid (losing which haplosome was null)
								p_out << (haplosome2.contains_mutation(mutation) ? "\t1" : "\t0");
							}
							else if (haplosome2_null)
							{
								// hemizygous; we emit this as haploid (losing which haplosome was null)
								p_out << (haplosome1.contains_mutation(mutation) ? "\t1" : "\t0");
							}
							else
							{
								bool haplosome1_has_mut = haplosome1.contains_mutation(mutation);
								bool haplosome2_has_mut = haplosome2.contains_mutation(mutation);
								
								if (haplosome1_has_mut && haplosome2_has_mut)	p_out << "\t1|1";
								else if (haplosome1_has_mut)					p_out << "\t1|0";
								else if (haplosome2_has_mut)					p_out << "\t0|1";
								else											p_out << "\t0|0";
							}
						}
					}
					
					p_out << std::endl;
				}
			}
		}
		
		// polyiter already points to the mutation at the next position, or to end(), so we don't advance it here
	}
}

size_t Haplosome::MemoryUsageForMutrunBuffers(void)
{
	if (mutruns_ == run_buffer_)
		return 0;
	else
		return mutrun_count_ * sizeof(MutationRun *);
}


//
//	Haplosome_Class
//
#pragma mark -
#pragma mark Haplosome_Class
#pragma mark -

EidosClass *gSLiM_Haplosome_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *Haplosome_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Haplosome_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_chromosome,		true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Chromosome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_chromosomeSubposition,	true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Haplosome::GetProperty_Accelerated_chromosomeSubposition));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_haplosomePedigreeID,true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Haplosome::GetProperty_Accelerated_haplosomePedigreeID));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_individual,		true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Individual_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_isNullHaplosome,	true,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Haplosome::GetProperty_Accelerated_isNullHaplosome));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutations,		true,	kEidosValueMaskObject, gSLiM_Mutation_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Haplosome::GetProperty_Accelerated_tag)->DeclareAcceleratedSet(Haplosome::SetProperty_Accelerated_tag));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Haplosome_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Haplosome_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_addMutations, kEidosValueMaskVOID))->AddObject("mutations", gSLiM_Mutation_Class));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_addNewDrawnMutation, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject("mutationType", gSLiM_MutationType_Class)->AddInt("position")->AddIntObject_ON("originSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddIntString_ON("nucleotide", gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_addNewMutation, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject("mutationType", gSLiM_MutationType_Class)->AddNumeric("selectionCoeff")->AddInt("position")->AddIntObject_ON("originSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddIntString_ON("nucleotide", gStaticEidosValueNULL));
		methods->emplace_back(((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_containsMarkerMutation, kEidosValueMaskLogical | kEidosValueMaskSingleton | kEidosValueMaskNULL | kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class)->AddInt_S("position")->AddLogical_OS("returnMutation", gStaticEidosValue_LogicalF))->DeclareAcceleratedImp(Haplosome::ExecuteMethod_Accelerated_containsMarkerMutation));
		methods->emplace_back(((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_containsMutations, kEidosValueMaskLogical))->AddObject("mutations", gSLiM_Mutation_Class))->DeclareAcceleratedImp(Haplosome::ExecuteMethod_Accelerated_containsMutations));
		methods->emplace_back(((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_countOfMutationsOfType, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class))->DeclareAcceleratedImp(Haplosome::ExecuteMethod_Accelerated_countOfMutationsOfType));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_positionsOfMutationsOfType, kEidosValueMaskInt))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_mutationCountsInHaplosomes, kEidosValueMaskInt))->AddObject_ON("mutations", gSLiM_Mutation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_mutationFrequenciesInHaplosomes, kEidosValueMaskFloat))->AddObject_ON("mutations", gSLiM_Mutation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationsOfType, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_nucleotides, kEidosValueMaskInt | kEidosValueMaskString))->AddInt_OSN(gEidosStr_start, gStaticEidosValueNULL)->AddInt_OSN(gEidosStr_end, gStaticEidosValueNULL)->AddString_OS("format", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String("string"))));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_readHaplosomesFromMS, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddString_S(gEidosStr_filePath)->AddIntObject_S("mutationType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_readHaplosomesFromVCF, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddString_S(gEidosStr_filePath)->AddIntObject_OSN("mutationType", gSLiM_MutationType_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_removeMutations, kEidosValueMaskVOID))->AddObject_ON("mutations", gSLiM_Mutation_Class, gStaticEidosValueNULL)->AddLogical_OS("substitute", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_outputHaplosomesToMS, kEidosValueMaskVOID))->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("filterMonomorphic", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_outputHaplosomesToVCF, kEidosValueMaskVOID))->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("outputMultiallelics", gStaticEidosValue_LogicalT)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("simplifyNucleotides", gStaticEidosValue_LogicalF)->AddLogical_OS("outputNonnucleotides", gStaticEidosValue_LogicalT)->AddLogical_OS("groupAsIndividuals", gStaticEidosValue_LogicalT));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_outputHaplosomes, kEidosValueMaskVOID))->AddString_OSN(gEidosStr_filePath, gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("objectTags", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_sumOfMutationsOfType, kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

EidosValue_SP Haplosome_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
	switch (p_method_id)
	{
		case gID_addMutations:					return ExecuteMethod_addMutations(p_method_id, p_target, p_arguments, p_interpreter);
		case gID_addNewDrawnMutation:
		case gID_addNewMutation:				return ExecuteMethod_addNewMutation(p_method_id, p_target, p_arguments, p_interpreter);
		case gID_mutationCountsInHaplosomes:
		case gID_mutationFrequenciesInHaplosomes:	return ExecuteMethod_mutationFreqsCountsInHaplosomes(p_method_id, p_target, p_arguments, p_interpreter);
		case gID_outputHaplosomes:
		case gID_outputHaplosomesToMS:
		case gID_outputHaplosomesToVCF:			return ExecuteMethod_outputX(p_method_id, p_target, p_arguments, p_interpreter);
		case gID_readHaplosomesFromMS:			return ExecuteMethod_readHaplosomesFromMS(p_method_id, p_target, p_arguments, p_interpreter);
		case gID_readHaplosomesFromVCF:			return ExecuteMethod_readHaplosomesFromVCF(p_method_id, p_target, p_arguments, p_interpreter);
		case gID_removeMutations:				return ExecuteMethod_removeMutations(p_method_id, p_target, p_arguments, p_interpreter);
		default:								return super::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_interpreter);
	}
}

//	*********************	+ (void)addMutations(object mutations)
//
EidosValue_SP Haplosome_Class::ExecuteMethod_addMutations(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_interpreter)
	EidosValue *mutations_value = p_arguments[0].get();
	
	// FIXME this method should be optimized for large-scale bulk addition in the same
	// way that addNewMutation() and addNewDrawnMutation() now are.  BCH 29 Oct 2017
	
	int target_size = p_target->Count();
	
	if (target_size == 0)
		return gStaticEidosValueVOID;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForHaplosomes(p_target);
	
	if (!species)
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addMutations): " << "addMutations() requires that all target haplosomes belong to the same species." << EidosTerminate();
	
	species->population_.CheckForDeferralInHaplosomes(p_target, "Haplosome_Class::ExecuteMethod_addMutations");
	
	Community &community = species->community_;
	MutationBlock *mutation_block = species->SpeciesMutationBlock();
	Mutation *mut_block_ptr = mutation_block->mutation_buffer_;
	
	// All haplosomes must belong to the same chromosome, and all mutations being added must belong to that chromosome too.
	// It's important that a mismatch result in an error; attempts to add mutations to chromosomes inconsistently should be flagged.
	int mutations_count = mutations_value->Count();
	Mutation * const *mutations = (Mutation * const *)mutations_value->ObjectData();
	Haplosome * const *targets = (Haplosome * const *)p_target->ObjectData();
	Haplosome *haplosome_0 = targets[0];
	slim_chromosome_index_t chromosome_index = haplosome_0->chromosome_index_;
	
	if (species->Chromosomes().size() > 1)
	{
		// We have to check for consistency if there's more than one chromosome
		for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
			if (targets[haplosome_index]->chromosome_index_ != chromosome_index)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addMutations): " << "addMutations() requires that all target haplosomes are associated with the same chromosome." << EidosTerminate();
		
		for (int value_index = 0; value_index < mutations_count; ++value_index)
			if (mutations[value_index]->chromosome_index_ != chromosome_index)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addMutations): " << "addMutations() requires that all mutations to be added are associated with the same chromosome as the target haplosomes." << EidosTerminate();
	}
	
	Chromosome *chromosome = species->Chromosomes()[chromosome_index];
	
	// use the 0th haplosome in the target to find out what the mutation run length is, so we can calculate run indices
	slim_position_t mutrun_length = haplosome_0->mutrun_length_;
	
	// check that the individuals that mutations are being added to have age == 0, in nonWF models, to prevent tree sequence inconsistencies (see issue #102)
	if ((community.ModelType() == SLiMModelType::kModelTypeNonWF) && species->RecordingTreeSequence())
	{
		for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
		{
			Haplosome *target_haplosome = targets[haplosome_index];
			Individual *target_individual = target_haplosome->OwningIndividual();
			
			if (target_individual->age_ > 0)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addMutations): " << "addMutations() cannot add mutations to individuals of age > 0 when tree-sequence recording is enabled, to prevent internal inconsistencies." << EidosTerminate();
		}
	}
	
	// check for other semantic issues
	Population &pop = species->population_;
	
	species->CheckMutationStackPolicy();
	
	// TIMING RESTRICTION
	if (!community.warned_early_mutation_add_)
	{
		if ((community.CycleStage() == SLiMCycleStage::kWFStage0ExecuteFirstScripts) ||
			(community.CycleStage() == SLiMCycleStage::kWFStage1ExecuteEarlyScripts))
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ErrorOutputStream() << "#WARNING (Haplosome_Class::ExecuteMethod_addMutations): addMutations() should probably not be called from a first() or early() event in a WF model; the added mutation(s) will not influence fitness values during offspring generation." << std::endl;
				community.warned_early_mutation_add_ = true;
			}
		}
		// Note that there is no equivalent problem in nonWF models, because fitness values are used for survival,
		// not reproduction, and there is no event stage in the tick cycle that splits fitness from survival.
	}
	
	// TIMING RESTRICTION
	if (community.executing_species_ == species)
	{
		if (community.executing_block_type_ == SLiMEidosBlockType::SLiMEidosModifyChildCallback)
		{
			// Check that we're not inside a modifyChild() callback, or if we are, that the only haplosomes being modified belong to the new child.
			// This prevents problems with retracting the proposed child when tree-sequence recording is enabled; other extraneous changes must
			// not be backed out, and it's hard to separate, e.g., what's a child-related new mutation from an extraneously added new mutation.
			// Note that the other Haplosome methods that add/remove mutations perform the same check, and should be maintained in parallel.
			Individual *focal_modification_child = community.focal_modification_child_;
			
			if (focal_modification_child)
			{
				for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
				{
					Haplosome *target_haplosome = targets[haplosome_index];
					
					if (target_haplosome->individual_ != focal_modification_child)
						EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addMutations): addMutations() cannot be called on the currently executing species from within a modifyChild() callback to modify any haplosomes except those of the focal child being generated." << EidosTerminate();
				}
			}
		}
		else if ((community.executing_block_type_ == SLiMEidosBlockType::SLiMEidosRecombinationCallback) ||
				 (community.executing_block_type_ == SLiMEidosBlockType::SLiMEidosMutationCallback))
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addMutations): addMutations() cannot be called on the currently executing species from within a recombination() or mutation() callback." << EidosTerminate();
	}
	
	// check that the same haplosome is not included more than once as a target, which we don't allow; we use patch_pointer as scratch
	for (int target_index = 0; target_index < target_size; ++target_index)
	{
		Haplosome *target_haplosome = targets[target_index];
		
		if (target_haplosome->IsNull())
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addMutations): addMutations() cannot be called on a null haplosome." << EidosTerminate();
		
		target_haplosome->scratch_ = 1;
	}
	
	for (int target_index = 0; target_index < target_size; ++target_index)
	{
		Haplosome *target_haplosome = targets[target_index];
		
		if (target_haplosome->scratch_ != 1)
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addMutations): addMutations() cannot be called on the same haplosome more than once (you must eliminate duplicates in the target vector)." << EidosTerminate();
		
		target_haplosome->scratch_ = 0;
	}
	
	// Construct a vector of mutations to add that is sorted by position
	std::vector<Mutation *> mutations_to_add;
	
	for (int value_index = 0; value_index < mutations_count; ++value_index)
	{
		Mutation *mut_to_add = mutations[value_index];
		
		if ((mut_to_add->state_ == MutationState::kFixedAndSubstituted) ||
			(mut_to_add->state_ == MutationState::kRemovedWithSubstitution))
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addMutations): addMutations() cannot add a mutation that has already been fixed/substituted." << EidosTerminate();
		
		mutations_to_add.emplace_back(mut_to_add);
	}
	
	std::sort(mutations_to_add.begin(), mutations_to_add.end(), [ ](Mutation *i1, Mutation *i2) {return i1->position_ < i2->position_;});
	
	// SPECIES CONSISTENCY CHECK
	if (mutations_count > 0)
	{
		Species *mutations_species = Community::SpeciesForMutations(mutations_value);
		
		if (mutations_species != species)
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addMutations): addMutations() requires that all mutations belong to the same species as the target haplosomes." << EidosTerminate();
	}
	
	// TREE SEQUENCE RECORDING
	// First, pre-plan the positions of new tree-seq derived states in anticipation of doing the addition.  We have to check
	// whether the mutation being added is already present, to avoid recording a new derived state identical to the old one state.
	// The algorithm used here, with HaplosomeWalker, depends upon the fact that we just sorted the mutations to add by position.
	// However, we do still have to think about multiple muts being added at the same position, and existing stacked mutations.
	bool recording_tree_sequence_mutations = species->RecordingTreeSequenceMutations();
	std::vector<std::pair<Haplosome *, std::vector<slim_position_t>>> new_derived_state_positions;
	
	if (recording_tree_sequence_mutations)
	{
		for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
		{
			Haplosome *target_haplosome = targets[haplosome_index];
			HaplosomeWalker walker(target_haplosome);
			slim_position_t last_added_pos = -1;
			
			for (Mutation *mut : mutations_to_add)
			{
				slim_position_t mut_pos = mut->position_;
				
				// We don't care about other mutations at an already recorded position; move on
				if (mut_pos == last_added_pos)
					continue;
				
				// Advance the walker until it is at or after the mutation's position
				while (!walker.Finished())
				{
					if (walker.Position() >= mut_pos)
						break;
					
					walker.NextMutation();
				}
				
				// If walker is finished, or is now after the mutation's position, drop through to add this position
				if (!walker.Finished() && (walker.Position() == mut_pos))
				{
					// If the mutation is already present, somewhere at this position, then we don't need to record it
					if (walker.MutationIsStackedAtCurrentPosition(mut))
						continue;
					
					// The mutation is not already present, so we need to record it; drop through to the addition code
				}
				
				// we have decided that the new derived state at this position will need to be recorded, so note that
				if (last_added_pos == -1)
				{
					// no pair entry in new_derived_state_positions yet, so make a new pair entry for this haplosome
					new_derived_state_positions.emplace_back(target_haplosome, std::vector<slim_position_t>(1, mut_pos));
				}
				else
				{
					// we have an existing pair entry for this haplosome, so add this position to its position vector
					std::pair<Haplosome *, std::vector<slim_position_t>> &haplosome_entry = new_derived_state_positions.back();
					std::vector<slim_position_t> &haplosome_list = haplosome_entry.second;
					
					haplosome_list.emplace_back(mut_pos);
				}
				
				last_added_pos = mut_pos;
			}
		}
	}
	
	// Now handle the mutations to add, broken into bulk operations according to the mutation run they fall into
	slim_mutrun_index_t last_handled_mutrun_index = -1;
	
	for (int value_index = 0; value_index < mutations_count; ++value_index)
	{
		Mutation *next_mutation = mutations_to_add[value_index];
		const slim_position_t pos = next_mutation->position_;
		slim_mutrun_index_t mutrun_index = (slim_mutrun_index_t)(pos / mutrun_length);
		
		if (mutrun_index <= last_handled_mutrun_index)
			continue;
		
		// We have not yet processed this mutation run; do this mutation run index as a bulk operation
		int64_t operation_id = MutationRun::GetNextOperationID();
		
		Haplosome::BulkOperationStart(operation_id, mutrun_index);
		
		MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForMutationRunIndex(mutrun_index);
		
		for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
		{
			Haplosome *target_haplosome = targets[haplosome_index];
			
			// See if WillModifyRunForBulkOperation() can short-circuit the operation for us
			MutationRun *target_run = target_haplosome->WillModifyRunForBulkOperation(operation_id, mutrun_index, mutrun_context);
			
			if (target_run)
			{
				for (int mut_index = value_index; mut_index < mutations_count; ++mut_index)
				{
					Mutation *mut_to_add = mutations_to_add[mut_index];
					const slim_position_t add_pos = mut_to_add->position_;
					
					// since we're in sorted order by position, as soon as we leave the current mutation run we're done
					if (add_pos / mutrun_length != mutrun_index)
						break;
					
					if (target_run->enforce_stack_policy_for_addition(mut_block_ptr, mut_to_add->position_, mut_to_add->mutation_type_ptr_))
					{
						target_run->insert_sorted_mutation_if_unique(mut_block_ptr, mutation_block->IndexInBlock(mut_to_add));
						
						// No need to add the mutation to the registry; how would the user ever get a Mutation that was not already in it?
						// Similarly, no need to check and set pure_neutral_ and all_pure_neutral_DFE_; the mutation is already in the system
					}
				}
			}
		}
		
		Haplosome::BulkOperationEnd(operation_id, mutrun_index);
		
		// now we have handled all mutations at this index (and all previous indices)
		last_handled_mutrun_index = mutrun_index;
		
		// invalidate cached mutation refcounts; refcounts have changed
		pop.InvalidateMutationReferencesCache();
	}
	
	// TREE SEQUENCE RECORDING
	// Now that all the bulk operations are done, record all the new derived states.  BCH 6/12/2021: Note that if a mutation to be added
	// was rejected by stacking policy 'f' above, it will still get recorded here with a new derived state, which will be identical to the
	// previous derived state.  This is maybe a bug, but nobody has complained and it looks hard to fix.  People don't use policy 'f' much.
	if (recording_tree_sequence_mutations)
	{
		for (std::pair<Haplosome *, std::vector<slim_position_t>> &haplosome_pair : new_derived_state_positions)
		{
			Haplosome *target_haplosome = haplosome_pair.first;
			std::vector<slim_position_t> &haplosome_positions = haplosome_pair.second;
			
			for (slim_position_t position : haplosome_positions)
				species->RecordNewDerivedState(target_haplosome, position, *target_haplosome->derived_mutation_ids_at_position(mut_block_ptr, position));
		}
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	+ (object<Mutation>)addNewDrawnMutation(io<MutationType> mutationType, integer position, [Nio<Subpopulation> originSubpop = NULL], [Nis nucleotide = NULL])
//	*********************	+ (object<Mutation>)addNewMutation(io<MutationType> mutationType, numeric selectionCoeff, integer position, [Nio<Subpopulation> originSubpop = NULL], [Nis nucleotide = NULL])
//
EidosValue_SP Haplosome_Class::ExecuteMethod_addNewMutation(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_interpreter)
	
#ifdef __clang_analyzer__
	assert(((p_method_id == gID_addNewDrawnMutation) && (p_arguments.size() == 5)) || ((p_method_id == gID_addNewMutation) && (p_arguments.size() == 6)));
#endif
	
	EidosValue *arg_muttype = p_arguments[0].get();
	EidosValue *arg_selcoeff = (p_method_id == gID_addNewDrawnMutation ? nullptr : p_arguments[1].get());
	EidosValue *arg_position = (p_method_id == gID_addNewDrawnMutation ? p_arguments[1].get() : p_arguments[2].get());
	EidosValue *arg_origin_subpop = (p_method_id == gID_addNewDrawnMutation ? p_arguments[2].get() : p_arguments[3].get());
	EidosValue *arg_nucleotide = (p_method_id == gID_addNewDrawnMutation ? p_arguments[3].get() : p_arguments[4].get());
	
	int target_size = p_target->Count();
	
	if (target_size == 0)
		return gStaticEidosValueNULLInvisible;	// this is almost an error condition, since a mutation was expected to be added and none was
	
	std::string method_name = EidosStringRegistry::StringForGlobalStringID(p_method_id);
	method_name.append("()");
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForHaplosomes(p_target);
	
	if (!species)
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " requires that all target haplosomes belong to the same species." << EidosTerminate();
	
	species->population_.CheckForDeferralInHaplosomes(p_target, "Haplosome_Class::ExecuteMethod_addNewMutation");
	
	Community &community = species->community_;
	MutationBlock *mutation_block = species->SpeciesMutationBlock();
	Mutation *mut_block_ptr = mutation_block->mutation_buffer_;
	
	// All haplosomes must belong to the same chromosome.  It's important that a mismatch result in an error;
	// attempts to add mutations to chromosomes inconsistently should be flagged.
	Haplosome * const *targets = (Haplosome * const *)p_target->ObjectData();
	Haplosome *haplosome_0 = targets[0];
	slim_chromosome_index_t chromosome_index = haplosome_0->chromosome_index_;
	
	if (species->Chromosomes().size() > 1)
	{
		// We have to check for consistency if there's more than one chromosome
		for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
			if (targets[haplosome_index]->chromosome_index_ != chromosome_index)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " requires that all target haplosomes are associated with the same chromosome." << EidosTerminate();
	}
	
	Chromosome *chromosome = species->Chromosomes()[chromosome_index];
	
	// get the 0th haplosome in the target to find out what the mutation run length is, so we can calculate run indices
	int mutrun_count = haplosome_0->mutrun_count_;
	slim_position_t mutrun_length = haplosome_0->mutrun_length_;
	
	// check that the individuals that mutations are being added to have age == 0, in nonWF models, to prevent tree sequence inconsistencies (see issue #102)
	if ((community.ModelType() == SLiMModelType::kModelTypeNonWF) && species->RecordingTreeSequence())
	{
		for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
		{
			Haplosome *target_haplosome = targets[haplosome_index];
			Individual *target_individual = target_haplosome->OwningIndividual();
			
			if (target_individual->age_ > 0)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " cannot add mutations to individuals of age > 0 when tree-sequence recording is enabled, to prevent internal inconsistencies." << EidosTerminate();
		}
	}
	
	// check for other semantic issues
	Population &pop = species->population_;
	bool nucleotide_based = species->IsNucleotideBased();
	
	if (!nucleotide_based && (arg_nucleotide->Type() != EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " requires nucleotide to be NULL in non-nucleotide-based models." << EidosTerminate();
	
	species->CheckMutationStackPolicy();
	
	// TIMING RESTRICTION
	if (!community.warned_early_mutation_add_)
	{
		if ((community.CycleStage() == SLiMCycleStage::kWFStage0ExecuteFirstScripts) ||
			(community.CycleStage() == SLiMCycleStage::kWFStage1ExecuteEarlyScripts))
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ErrorOutputStream() << "#WARNING (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " should probably not be called from a first() or early() event in a WF model; the added mutation will not influence fitness values during offspring generation." << std::endl;
				community.warned_early_mutation_add_ = true;
			}
		}
		// Note that there is no equivalent problem in nonWF models, because fitness values are used for survival,
		// not reproduction, and there is no event stage in the tick cycle that splits fitness from survival.
	}
	
	// TIMING RESTRICTION
	if (community.executing_species_ == species)
	{
		if (community.executing_block_type_ == SLiMEidosBlockType::SLiMEidosModifyChildCallback)
		{
			// Check that we're not inside a modifyChild() callback, or if we are, that the only haplosomes being modified belong to the new child.
			// This prevents problems with retracting the proposed child when tree-sequence recording is enabled; other extraneous changes must
			// not be backed out, and it's hard to separate, e.g., what's a child-related new mutation from an extraneously added new mutation.
			// Note that the other Haplosome methods that add/remove mutations perform the same check, and should be maintained in parallel.
			Individual *focal_modification_child = community.focal_modification_child_;
			
			if (focal_modification_child)
			{
				for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
				{
					Haplosome *target_haplosome = targets[haplosome_index];
					
					if (target_haplosome->individual_ != focal_modification_child)
						EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " cannot be called on the currently executing species from within a modifyChild() callback to modify any haplosomes except those of the focal child being generated." << EidosTerminate();
				}
			}
		}
		else if ((community.executing_block_type_ == SLiMEidosBlockType::SLiMEidosRecombinationCallback) ||
				 (community.executing_block_type_ == SLiMEidosBlockType::SLiMEidosMutationCallback))
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " cannot be called on the currently executing species from within a recombination() or mutation() callback." << EidosTerminate();
	}
	
	// position and originSubpop can now be either singletons or vectors of matching length or NULL; check them all
	int muttype_count = arg_muttype->Count();
	int selcoeff_count = (arg_selcoeff ? arg_selcoeff->Count() : 0);
	int position_count = arg_position->Count();
	int origin_subpop_count = arg_origin_subpop->Count();
	int nucleotide_count = arg_nucleotide->Count();
	
	if (arg_origin_subpop->Type() == EidosValueType::kValueNULL)
		origin_subpop_count = 1;
	if (arg_nucleotide->Type() == EidosValueType::kValueNULL)
		nucleotide_count = 1;
	
	int count_to_add = std::max({muttype_count, selcoeff_count, position_count, origin_subpop_count, nucleotide_count});
	
	if (((muttype_count != 1) && (muttype_count != count_to_add)) ||
		(arg_selcoeff && (selcoeff_count != 1) && (selcoeff_count != count_to_add)) ||
		((position_count != 1) && (position_count != count_to_add)) ||
		((origin_subpop_count != 1) && (origin_subpop_count != count_to_add)) ||
		((nucleotide_count != 1) && (nucleotide_count != count_to_add)))
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " requires that mutationType, " << ((p_method_id == gID_addNewMutation) ? "selectionCoeff, " : "") << "position, originSubpop, and nucleotide be either (1) singleton, or (2) equal in length to the other non-singleton argument(s), or (3) NULL, for originSubpop and nucleotide." << EidosTerminate();
	
	EidosValue_Object_SP retval(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class));
	
	if (count_to_add == 0)
		return retval;
	
	// before proceeding, let's check that all positions supplied are valid, so we don't need to worry about it below
	// would be better not to call IntAtIndex_NOCAST() multiple times for the same position, but that will not be the majority of our time anyway...
	slim_position_t last_position = chromosome->last_position_;
	
	for (int position_index = 0; position_index < position_count; ++position_index)
	{
		slim_position_t position = SLiMCastToPositionTypeOrRaise(arg_position->IntAtIndex_NOCAST(position_index, nullptr));
		
		if (position > last_position)
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " position " << position << " is past the end of the chromosome." << EidosTerminate();
	}
	
	// similarly, check nucleotide values for validity
	uint8_t *nucleotide_lookup = NucleotideArray::NucleotideCharToIntLookup();
	
	if (arg_nucleotide->Type() == EidosValueType::kValueNULL)
	{
		// If nucleotide is NULL, all mutation types supplied must be non-nucleotide-based
		for (int muttype_index = 0; muttype_index < muttype_count; ++muttype_index)
		{
			MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(arg_muttype, muttype_index, &community, species, method_name.c_str());		// SPECIES CONSISTENCY CHECK
			
			if (mutation_type_ptr->nucleotide_based_)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " requires nucleotide to be non-NULL when nucleotide-based mutation types are used." << EidosTerminate();
		}
	}
	else
	{
		// If nucleotide is non-NULL, all mutation types supplied must be nucleotide-based
		for (int muttype_index = 0; muttype_index < muttype_count; ++muttype_index)
		{
			MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(arg_muttype, muttype_index, &community, species, method_name.c_str());		// SPECIES CONSISTENCY CHECK
			
			if (!mutation_type_ptr->nucleotide_based_)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " requires nucleotide to be NULL when non-nucleotide-based mutation types are used." << EidosTerminate();
		}
		
		// And then nucleotide values must also be within bounds
		if (arg_nucleotide->Type() == EidosValueType::kValueInt)
		{
			for (int nucleotide_index = 0; nucleotide_index < nucleotide_count; ++nucleotide_index)
			{
				int64_t nuc_int = arg_nucleotide->IntAtIndex_NOCAST(nucleotide_index, nullptr);
				
				if ((nuc_int < 0) || (nuc_int > 3))
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " requires integer nucleotide values to be in [0,3]." << EidosTerminate();
			}
		}
		else if (arg_nucleotide->Type() == EidosValueType::kValueString)
		{
			for (int nucleotide_index = 0; nucleotide_index < nucleotide_count; ++nucleotide_index)
			{
				uint8_t nuc = nucleotide_lookup[(unsigned char)(arg_nucleotide->StringAtIndex_NOCAST(nucleotide_index, nullptr)[0])];
				
				if (nuc > 3)
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " requires string nucleotide values to be 'A', 'C', 'G', or 'T'." << EidosTerminate();
			}
		}
	}
	
	// check that the same haplosome is not included more than once as a target, which we don't allow; we use patch_pointer as scratch
	for (int target_index = 0; target_index < target_size; ++target_index)
	{
		Haplosome *target_haplosome = targets[target_index];
		
		if (target_haplosome->IsNull())
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " cannot be called on a null haplosome." << EidosTerminate();
		
		target_haplosome->scratch_ = 1;
	}
	
	for (int target_index = 0; target_index < target_size; ++target_index)
	{
		Haplosome *target_haplosome = targets[target_index];
		
		if (target_haplosome->scratch_ != 1)
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " cannot be called on the same haplosome more than once (you must eliminate duplicates in the target vector)." << EidosTerminate();
		
		target_haplosome->scratch_ = 0;
	}
	
	// each bulk operation is performed on a single mutation run, so we need to figure out which runs we're influencing
	std::vector<slim_mutrun_index_t> mutrun_indexes;
	
	if (mutrun_count == 1)
	{
		// if we have just a single mutrun, we can avoid the sorting and uniquing; all valid positions are in mutrun 0
		mutrun_indexes.emplace_back(0);
	}
	else
	{
		for (int pos_index = 0; pos_index < position_count; ++pos_index)
		{
			slim_position_t position = SLiMCastToPositionTypeOrRaise(arg_position->IntAtIndex_NOCAST(pos_index, nullptr));
			mutrun_indexes.emplace_back((slim_mutrun_index_t)(position / mutrun_length));
		}
		
		std::sort(mutrun_indexes.begin(), mutrun_indexes.end());
		mutrun_indexes.resize(std::distance(mutrun_indexes.begin(), std::unique(mutrun_indexes.begin(), mutrun_indexes.end())));
	}
	
	// for the singleton case for each of the parameters, get all the info
	MutationType *singleton_mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(arg_muttype, 0, &community, species, method_name.c_str());		// SPECIES CONSISTENCY CHECK
	
	double singleton_selection_coeff = (arg_selcoeff ? arg_selcoeff->NumericAtIndex_NOCAST(0, nullptr) : 0.0);
	
	slim_position_t singleton_position = SLiMCastToPositionTypeOrRaise(arg_position->IntAtIndex_NOCAST(0, nullptr));
	
	slim_tick_t origin_tick = community.Tick();
	
	slim_objectid_t singleton_origin_subpop_id;
	
	if (arg_origin_subpop->Type() == EidosValueType::kValueNULL)
	{
		singleton_origin_subpop_id = -1;
		
		// We set the origin subpopulation based on the first haplosome in the target
		if (target_size >= 1)
		{
			Haplosome *first_target = targets[0];
			singleton_origin_subpop_id = first_target->individual_->subpopulation_->subpopulation_id_;
		}
	}
	else if (arg_origin_subpop->Type() == EidosValueType::kValueInt)
	{
		singleton_origin_subpop_id = SLiMCastToObjectidTypeOrRaise(arg_origin_subpop->IntAtIndex_NOCAST(0, nullptr));
	}
	else
	{
		Subpopulation *origin_subpop = ((Subpopulation *)(arg_origin_subpop->ObjectElementAtIndex_NOCAST(0, nullptr)));
		
		// SPECIES CONSISTENCY CHECK
		if (&origin_subpop->species_ != species)
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_addNewMutation): " << method_name << " requires that originSubpop belong to the same species as the target haplosomes." << EidosTerminate();
		
		singleton_origin_subpop_id = origin_subpop->subpopulation_id_;
	}
	
	int64_t singleton_nucleotide;
	
	if (arg_nucleotide->Type() == EidosValueType::kValueNULL)
		singleton_nucleotide = -1;
	else if (arg_nucleotide->Type() == EidosValueType::kValueInt)
		singleton_nucleotide = arg_nucleotide->IntAtIndex_NOCAST(0, nullptr);
	else
		singleton_nucleotide = nucleotide_lookup[(unsigned char)(arg_nucleotide->StringAtIndex_NOCAST(0, nullptr)[0])];
	
	// ok, now loop to add the mutations in a single bulk operation per mutation run
	bool recording_tree_sequence_mutations = species->RecordingTreeSequenceMutations();
	
	for (slim_mutrun_index_t mutrun_index : mutrun_indexes)
	{
		int64_t operation_id = MutationRun::GetNextOperationID();
		std::vector<MutationIndex> mutations_to_add;
		
		// Before starting the bulk operation for this mutation run, construct all of the mutations and add them all to the registry, etc.
		// It is possible that some mutations will not actually be added to any haplosome, due to stacking; they will be cleared from the
		// registry as lost mutations in the next cycle.  All mutations are returned to the user, whether actually added or not.
		MutationType *mutation_type_ptr = singleton_mutation_type_ptr;
		double selection_coeff = singleton_selection_coeff;
		slim_position_t position = singleton_position;
		slim_objectid_t origin_subpop_id = singleton_origin_subpop_id;
		int64_t nucleotide = singleton_nucleotide;
		
		for (int mut_parameter_index = 0; mut_parameter_index < count_to_add; ++mut_parameter_index)
		{
			if (position_count != 1)
				position = SLiMCastToPositionTypeOrRaise(arg_position->IntAtIndex_NOCAST(mut_parameter_index, nullptr));
			
			// check that this mutation will be added to this mutation run
			if (position / mutrun_length == mutrun_index)
			{
				if (muttype_count != 1)
					mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(arg_muttype, mut_parameter_index, &community, species, method_name.c_str());		// SPECIES CONSISTENCY CHECK
				
				if (selcoeff_count != 1)
				{
					if (arg_selcoeff)
						selection_coeff = arg_selcoeff->NumericAtIndex_NOCAST(mut_parameter_index, nullptr);
					else
						selection_coeff = mutation_type_ptr->DrawEffectForTrait(0);	// FIXME MULTITRAIT
				}
				
				if (origin_subpop_count != 1)
				{
					if (arg_origin_subpop->Type() == EidosValueType::kValueInt)
						origin_subpop_id = SLiMCastToObjectidTypeOrRaise(arg_origin_subpop->IntAtIndex_NOCAST(mut_parameter_index, nullptr));
					else
						origin_subpop_id = ((Subpopulation *)(arg_origin_subpop->ObjectElementAtIndex_NOCAST(mut_parameter_index, nullptr)))->subpopulation_id_;
				}
				
				if (nucleotide_count != 1)
				{
					// Already checked for validity above
					if (arg_nucleotide->Type() == EidosValueType::kValueInt)
						nucleotide = arg_nucleotide->IntAtIndex_NOCAST(mut_parameter_index, nullptr);
					else
						nucleotide = nucleotide_lookup[(unsigned char)(arg_nucleotide->StringAtIndex_NOCAST(mut_parameter_index, nullptr)[0])];
				}
				
				MutationIndex new_mut_index = mutation_block->NewMutationFromBlock();
				
				Mutation *new_mut = new (mut_block_ptr + new_mut_index) Mutation(mutation_type_ptr, chromosome->Index(), position, static_cast<slim_effect_t>(selection_coeff), mutation_type_ptr->DefaultDominanceForTrait(0), origin_subpop_id, origin_tick, (int8_t)nucleotide);	// FIXME MULTITRAIT
				
				// This mutation type might not be used by any genomic element type (i.e. might not already be vetted), so we need to check and set pure_neutral_
				// The selection coefficient might have been supplied by the user (i.e., not be from the mutation type's DFE), so we set all_pure_neutral_DFE_ also
				if (selection_coeff != 0.0)
				{
					species->pure_neutral_ = false;
					mutation_type_ptr->all_pure_neutral_DFE_ = false;
				}
				
				// add to the registry, return value, haplosome, etc.
				if (new_mut->state_ != MutationState::kInRegistry)
					pop.MutationRegistryAdd(new_mut);
				retval->push_object_element_RR(new_mut);
				mutations_to_add.emplace_back(new_mut_index);
			}
		}
		
		// BCH 18 January 2020: If a vector of positions was provided, mutations_to_add might be out of sorted
		// order, which is expected below by clear_set_and_merge(), so we sort here
		if ((position_count != 1) && (mutations_to_add.size() > 1))
			std::sort(mutations_to_add.begin(), mutations_to_add.end(), [mut_block_ptr](MutationIndex i1, MutationIndex i2) {return (mut_block_ptr + i1)->position_ < (mut_block_ptr + i2)->position_;});
		
		// Now start the bulk operation and add mutations_to_add to every target haplosome
		Haplosome::BulkOperationStart(operation_id, mutrun_index);
		
		MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForMutationRunIndex(mutrun_index);
		
		for (int target_index = 0; target_index < target_size; ++target_index)
		{
			Haplosome *target_haplosome = targets[target_index];
			
			// See if WillModifyRunForBulkOperation() can short-circuit the operation for us
			const MutationRun *original_run = target_haplosome->mutruns_[mutrun_index];
			MutationRun *modifiable_mutrun = target_haplosome->WillModifyRunForBulkOperation(operation_id, mutrun_index, mutrun_context);
			
			if (modifiable_mutrun)
			{
				// We merge the original run (which has not yet been freed!) and mutations_to_add into modifiable_mutrun
				modifiable_mutrun->clear_set_and_merge(mut_block_ptr, *original_run, mutations_to_add);
			}
			
			// TREE SEQUENCE RECORDING
			// whether WillModifyRunForBulkOperation() short-circuited the addition or not, we need to notify the tree seq code
			// BCH 6/12/2021: We only need to record a derived state once per position, even if there were multiple adds at that position.
			// This prevents redundant derived states from being recorded; see discussion in https://github.com/MesserLab/SLiM/issues/195
			if (recording_tree_sequence_mutations)
			{
				MutationIndex *muts = mutations_to_add.data();
				MutationIndex *muts_end = muts + mutations_to_add.size();
				slim_position_t previous_position = -1;
				
				while (muts != muts_end)
				{
					Mutation *mut = mut_block_ptr + *(muts++);
					slim_position_t pos = mut->position_;
					
					if (pos != previous_position)
					{
						species->RecordNewDerivedState(target_haplosome, pos, *target_haplosome->derived_mutation_ids_at_position(mut_block_ptr, pos));
						previous_position = pos;
					}
				}
			}
		}
		
		Haplosome::BulkOperationEnd(operation_id, mutrun_index);
		
		// invalidate cached mutation refcounts; refcounts have changed
		pop.InvalidateMutationReferencesCache();
	}
	
	return retval;
}

//	*********************	+ (float)mutationFrequenciesInHaplosomes([No<Mutation> mutations = NULL])
//	*********************	+ (integer)mutationCountsInHaplosomes([No<Mutation> mutations = NULL])
//
EidosValue_SP Haplosome_Class::ExecuteMethod_mutationFreqsCountsInHaplosomes(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutations_value = p_arguments[0].get();
	
	// get our target vector, handle the zero-length case, and do a pre-check for null haplosomes so we don't have to worry about it later on
	slim_refcount_t target_size = (slim_refcount_t)p_target->Count();
	
	if (target_size == 0)
	{
		// With a zero-length target, frequencies are undefined so it is an error; to keep life simple, we make it an error for counts too
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_mutationFreqsCountsInHaplosomes): " << EidosStringRegistry::StringForGlobalStringID(p_method_id) << "() cannot calculate counts/frequencies in a zero-length Haplosome vector (divide by zero)." << EidosTerminate();
	}
	
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Haplosome_Class::ExecuteMethod_mutationFreqsCountsInHaplosomes(): usage of statics");
	
	Haplosome * const *target_data = (Haplosome * const *)p_target->ObjectData();
	
	for (int target_index = 0; target_index < target_size; ++target_index)
		if (target_data[target_index]->IsNull())
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_mutationFreqsCountsInHaplosomes): " << EidosStringRegistry::StringForGlobalStringID(p_method_id) << "() cannot be called on a null haplosome." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForHaplosomesVector(target_data, target_size);
	
	if (!species)
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_mutationFreqsCountsInHaplosomes): " << EidosStringRegistry::StringForGlobalStringID(p_method_id) << "() requires that all target haplosomes belong to a single species." << EidosTerminate();
	
	if (mutations_value->Count() >= 1)
	{
		Species *mut_species = Community::SpeciesForMutations(mutations_value);
		
		if (mut_species != species)
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_mutationFreqsCountsInHaplosomes): " << EidosStringRegistry::StringForGlobalStringID(p_method_id) << "() requires that all mutations belong to the same species as the target haplosomes." << EidosTerminate();
	}
	
	// Note that we allow the haplosomes and mutations to be associated with more than one chromosome here;
	// you can pass a mix, and for each mutation you get its frequency in the haplosomes for its chromosome.
	// If you pass NULL, all mutations are used, which can be confusing in the multi-chromosome case if you
	// passed a vector of haplosomes that are all for a specific chromosome.  In that case, you shouldn't
	// pass NULL for mutations, but rather use sim.subsetMutations() to get the mutations for your focal
	// chromosome, probably.
	
	species->population_.CheckForDeferralInHaplosomes(p_target, "Haplosome_Class::ExecuteMethod_mutationFreqsCountsInHaplosomes");
	
	Population &population = species->population_;
	
	// Have the Population tally for the target haplosomes
	population.TallyMutationReferencesAcrossHaplosomes(target_data, target_size);
	
	// Use the back-end code in Population to do the counting; TallyMutationReferencesAcrossHaplosomes()
	// should have set the total haplosome count correctly for the given haplosome sample.  Note that a
	// sample of mutations can be passed that belongs to a variety of different chromosomes; in this case,
	// each chromosome's total haplosome count should reflect the number of haplosomes in the sample
	// that belong to that chromosome, so the frequencies should be correct in that sense.
	if (p_method_id == gID_mutationFrequenciesInHaplosomes)
		return population.Eidos_FrequenciesForTalliedMutations(mutations_value);
	else
		return population.Eidos_CountsForTalliedMutations(mutations_value);
}

//	*********************	+ (void)outputHaplosomes([Ns$ filePath = NULL], [logical$ append=F], [logical$ objectTags = F])
//	*********************	+ (void)outputHaplosomesToMS([Ns$ filePath = NULL], [logical$ append=F], [logical$ filterMonomorphic = F])
//	*********************	+ (void)outputHaplosomesToVCF([Ns$ filePath = NULL], [logical$ outputMultiallelics = T], [logical$ append=F], [logical$ simplifyNucleotides = F], [logical$ outputNonnucleotides = T], [logical$ groupAsIndividuals = T])
//
EidosValue_SP Haplosome_Class::ExecuteMethod_outputX(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_interpreter)
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *outputMultiallelics_value = ((p_method_id == gID_outputHaplosomesToVCF) ? p_arguments[1].get() : nullptr);
	EidosValue *append_value = ((p_method_id == gID_outputHaplosomesToVCF) ? p_arguments[2].get() : p_arguments[1].get());
	EidosValue *filterMonomorphic_value = ((p_method_id == gID_outputHaplosomesToMS) ? p_arguments[2].get() : nullptr);
	EidosValue *simplifyNucleotides_value = ((p_method_id == gID_outputHaplosomesToVCF) ? p_arguments[3].get() : nullptr);
	EidosValue *outputNonnucleotides_value = ((p_method_id == gID_outputHaplosomesToVCF) ? p_arguments[4].get() : nullptr);
	EidosValue *groupAsIndividuals_value = ((p_method_id == gID_outputHaplosomesToVCF) ? p_arguments[5].get() : nullptr);
	EidosValue *objectTags_value = ((p_method_id == gID_outputHaplosomes) ? p_arguments[2].get() : nullptr);
	
	// default to outputting multiallelic positions (used by VCF output only)
	bool output_multiallelics = true;
	
	if (p_method_id == gID_outputHaplosomesToVCF)
		output_multiallelics = outputMultiallelics_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	bool simplify_nucs = false;
	
	if (p_method_id == gID_outputHaplosomesToVCF)
		simplify_nucs = simplifyNucleotides_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	bool output_nonnucs = true;
	
	if (p_method_id == gID_outputHaplosomesToVCF)
		output_nonnucs = outputNonnucleotides_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	bool group_as_individuals = true;
	
	if (p_method_id == gID_outputHaplosomesToVCF)
		group_as_individuals = groupAsIndividuals_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	// figure out if we're filtering out mutations that are monomorphic within the sample (MS output only)
	bool filter_monomorphic = false;
	
	if (p_method_id == gID_outputHaplosomesToMS)
		filter_monomorphic = filterMonomorphic_value->LogicalAtIndex_NOCAST(0, nullptr);
	
	bool output_object_tags = (objectTags_value ? objectTags_value->LogicalAtIndex_NOCAST(0, nullptr) : false);
	
	// Get all the haplosomes we're sampling from p_target; they must all be in the same species, which we determine here
	// We require at least one haplosome because otherwise we can't determine the species
	int sample_size = p_target->Count();
	std::vector<Haplosome *> haplosomes;
	
	if (sample_size <= 0)
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_outputX): output of a zero-length haplosome vector is illegal; at least one haplosome is required for output." << EidosTerminate();
	
	Haplosome **target_haplosomes = (Haplosome **)p_target->ObjectData();
	Species *species = &target_haplosomes[0]->individual_->subpopulation_->species_;
	
	for (int index = 0; index < sample_size; ++index)
	{
		Haplosome *haplosome = target_haplosomes[index];
		Species *haplosome_species = &haplosome->individual_->subpopulation_->species_;
		
		if (species != haplosome_species)
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_outputX): all haplosomes for output must belong to the same species." << EidosTerminate();
		
		haplosomes.emplace_back(haplosome);
	}
	
	species->population_.CheckForDeferralInHaplosomes(p_target, "Haplosome_Class::ExecuteMethod_outputX");

	Community &community = species->community_;
	
	// We infer the chromosome from the haplosomes, and in a multi-chrom species all the haplosomes must belong to it.
	slim_chromosome_index_t chromosome_index = haplosomes[0]->chromosome_index_;
	const std::vector<Chromosome *> &chromosomes = species->Chromosomes();
	Chromosome *chromosome = chromosomes[chromosome_index];
	
	if (chromosomes.size() > 1)
	{
		for (Haplosome *haplosome : haplosomes)
			if (haplosome->chromosome_index_ != chromosome_index)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_outputX): all haplosomes for output must be associated with the same chromosome." << EidosTerminate();
	}
	
	// Now handle stream/file output and dispatch to the actual print method
	if (filePath_value->Type() == EidosValueType::kValueNULL)
	{
		// before writing anything, erase a progress line if we've got one up, to try to make a clean slate
		Eidos_EraseProgress();
		
		// If filePath is NULL, output to our output stream
		std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
		
		// For the output stream, we put out a descriptive SLiM-style header for all output types
		// BCH 2/2/2025: added the cycle count here after the tick; it was already documented as being here!
		// BCH 2/2/2025: added the chromosome symbol in the header; it is redundant for SLiM-format output,
		// but useful for MS and VCF; I decided to put it in all three for consistency across formats
		// BCH 2/7/2025: changed GS/GM/GV to HS/HM/HV, for the genome -> haplosome transition
		output_stream << "#OUT: " << community.Tick() << " " << species->Cycle() << " H";
		
		if (p_method_id == gID_outputHaplosomes)
			output_stream << "S";
		else if (p_method_id == gID_outputHaplosomesToMS)
			output_stream << "M";
		else if (p_method_id == gID_outputHaplosomesToVCF)
			output_stream << "V";
		
		output_stream << " " << sample_size;
		
		if (chromosomes.size() > 1)
		{
			output_stream << " " << chromosome->Type();						// chromosome type, with >1 chromosome
			output_stream << " \"" << chromosome->Symbol() << "\"";			// chromosome symbol, with >1 chromosome
		}
		
		output_stream << std::endl;
		
		// Call out to print the actual sample
		if (p_method_id == gID_outputHaplosomes)
			Haplosome::PrintHaplosomes_SLiM(output_stream, *species, haplosomes, output_object_tags);
		else if (p_method_id == gID_outputHaplosomesToMS)
			Haplosome::PrintHaplosomes_MS(output_stream, *species, haplosomes, *chromosome, filter_monomorphic);
		else if (p_method_id == gID_outputHaplosomesToVCF)
			Haplosome::PrintHaplosomes_VCF(output_stream, haplosomes, *chromosome, group_as_individuals, output_multiallelics, simplify_nucs, output_nonnucs);
	}
	else
	{
		// Otherwise, output to filePath
		std::string outfile_path = Eidos_ResolvedPath(filePath_value->StringAtIndex_NOCAST(0, nullptr));
		bool append = append_value->LogicalAtIndex_NOCAST(0, nullptr);
		std::ofstream outfile;
		
		outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		
		if (outfile.is_open())
		{
			switch (p_method_id)
			{
				case gID_outputHaplosomes:
					// For file output, we put out the descriptive SLiM-style header only for SLiM-format output
					// BCH 2/2/2025: added the cycle count here after the tick; it was already documented as being here!
					// BCH 2/2/2025: added the chromosome symbol in the header; it is redundant for SLiM-format output,
					// but useful for MS and VCF; I decided to put it in all three for consistency across formats
					// BCH 2/7/2025: changed GS/GM/GV to HS/HM/HV, for the genome -> haplosome transition
					outfile << "#OUT: " << community.Tick() << " " << species->Cycle() << " HS " << sample_size;
					
					if (chromosomes.size() > 1)
					{
						outfile << " " << chromosome->Type();						// chromosome type, with >1 chromosome
						outfile << " \"" << chromosome->Symbol() << "\"";			// chromosome symbol, with >1 chromosome
					}
					
					outfile << " " << outfile_path << std::endl;
					
					Haplosome::PrintHaplosomes_SLiM(outfile, *species, haplosomes, output_object_tags);
					break;
				case gID_outputHaplosomesToMS:
					Haplosome::PrintHaplosomes_MS(outfile, *species, haplosomes, *chromosome, filter_monomorphic);
					break;
				case gID_outputHaplosomesToVCF:
					Haplosome::PrintHaplosomes_VCF(outfile, haplosomes, *chromosome, group_as_individuals, output_multiallelics, simplify_nucs, output_nonnucs);
					break;
				default:
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_outputX): (internal error) unhandled case." << EidosTerminate();
			}
			
			outfile.close(); 
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_outputX): could not open " << outfile_path << "." << EidosTerminate();
		}
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	+ (o<Mutation>)readHaplosomesFromMS(s$ filePath = NULL, io<MutationType> mutationType)
//
EidosValue_SP Haplosome_Class::ExecuteMethod_readHaplosomesFromMS(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_interpreter)
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Haplosome_Class::ExecuteMethod_readHaplosomesFromMS(): SLiM global state read");
	
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *mutationType_value = p_arguments[1].get();
	
	Community &community = SLiM_GetCommunityFromInterpreter(p_interpreter);
	std::string file_path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(filePath_value->StringAtIndex_NOCAST(0, nullptr)));
	MutationType *mutation_type_ptr = nullptr;
	
	if (mutationType_value->Type() != EidosValueType::kValueNULL)
		mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutationType_value, 0, &community, nullptr, "ExecuteMethod_readHaplosomesFromMS()");	// this dictates the focal species
	
	if (!mutation_type_ptr)
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): mutation type not found." << EidosTerminate();
	
	// Get the species of interest from the mutation type; we will check that all target haplosomes belong to it below
	Species &species = mutation_type_ptr->species_;
	Population &pop = species.population_;
	bool recording_mutations = species.RecordingTreeSequenceMutations();
	bool nucleotide_based = species.IsNucleotideBased();
	int target_size = p_target->Count();
	
	if (target_size <= 0)
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): readHaplosomesFromMS() requires at least one target haplosome." << EidosTerminate();
	
	// SPECIES CONSISTENCY CHECK
	Species *target_species = Community::SpeciesForHaplosomes(p_target);
	
	if (target_species != &species)
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): readHaplosomesFromMS() requires that all target haplosomes belong to the same species." << EidosTerminate();
	
	species.population_.CheckForDeferralInHaplosomes(p_target, "Haplosome_Class::ExecuteMethod_readHaplosomesFromMS");
	
	MutationBlock *mutation_block = species.SpeciesMutationBlock();
	Mutation *mut_block_ptr = mutation_block->mutation_buffer_;
	
	// For MS input, we need to know the chromosome to calculate positions from the normalized interval [0, 1].
	// We infer it from the haplosomes, and in a multi-chromosome species all the haplosomes must belong to it.
	Haplosome * const *targets_data = (Haplosome * const *)p_target->ObjectData();
	slim_chromosome_index_t chromosome_index = targets_data[0]->chromosome_index_;
	const std::vector<Chromosome *> &chromosomes = species.Chromosomes();
	Chromosome *chromosome = chromosomes[chromosome_index];
	
	if (chromosomes.size() > 1)
	{
		for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
			if (targets_data[haplosome_index]->chromosome_index_ != chromosome_index)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): for readHaplosomesFromMS(), all target haplosomes must be associated with the same chromosome." << EidosTerminate();
	}
	
	slim_position_t last_position = chromosome->last_position_;
	
	// Parse the whole input file and retain the information from it
	std::ifstream infile(file_path);
	
	if (!infile)
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): could not read file at path " << file_path << "." << EidosTerminate();
	
	std::string line, sub;
	int parse_state = 0;
	int segsites = -1;
	std::vector<slim_position_t> positions;
	std::vector<std::string> calls;
	
	while (!infile.eof())
	{
		getline(infile, line);
		
		if ((line.length() == 0) || (line.find("//") == 0))
			continue;
		
		switch (parse_state)
		{
			case 0:
			{
				// Expecting "segsites: x"
				std::istringstream iss(line);
				
				iss >> sub;
				if (sub != "segsites:")
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): expecting 'segsites:', found '" << sub << "'." << EidosTerminate();
				
				if (!(iss >> sub))
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): missing segsites value." << EidosTerminate();
				
				int64_t segsites_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
				
				if ((segsites_long <= 0) || (segsites_long > 1000000))
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): readMS() requires segsites in (0,1000000]." << EidosTerminate();
				
				segsites = (int)segsites_long;
				
				if (iss >> sub)
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): malformed segsites line; additional content after segsites value." << EidosTerminate();
				
				parse_state = 1;
				break;
			}
			case 1:
			{
				// Expecting "positions: a b c..."
				std::istringstream iss(line);
				
				iss >> sub;
				if (sub != "positions:")
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): expecting 'positions:', found '" << sub << "'." << EidosTerminate();
				
				for (int pos_index = 0; pos_index < segsites; ++pos_index)
				{
					if (!(iss >> sub))
						EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): missing positions value." << EidosTerminate();
					
					double pos_double = EidosInterpreter::FloatForString(sub, nullptr);
					
					if ((pos_double < 0.0) || (pos_double > 1.0))
						EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): readMS() requires positions in [0,1]." << EidosTerminate();
					
					// BCH 26 Jan. 2020: There is a little subtlety here.  This equation, round(pos * L), provides
					// the exact inverse of what outputHaplosomesToMS() / outputMSSample() do, so it should exactly recover
					// positions written out by SLiM in MS format (modulo numerical error).  However, it results
					// in half as much "mutational density" at positions 0 and L as at other positions, if the
					// positions are uniformly distributed in [0,1] rather than originating in SLiM.  In that case,
					// min(floor(pos*(L+1)), L) would be better.  Maybe this choice ought to be an optional logical
					// parameter to readHaplosomesFromMS(), but nobody has complained yet, so I'm ignoring it for now; if
					// you expect to get exact discrete base positions you shouldn't be using MS format anyway...
					positions.emplace_back((slim_position_t)round(pos_double * last_position));
				}
				
				if (iss >> sub)
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): malformed positions line; additional content after last expected position." << EidosTerminate();
				
				parse_state = 2;
				break;
			}
			case 2:
			{
				// Expecting "001010011001101111010..." of length segsites
				if (line.find_first_not_of("01") != std::string::npos)
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): call lines must be composed entirely of 0 and 1." << EidosTerminate();
				if ((int)line.length() != segsites)
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): call lines must be equal in length to the segsites value." << EidosTerminate();
				
				calls.emplace_back(line);
				break;
			}
			default:
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): (internal error) unhandled case." << EidosTerminate();
		}
	}
	
	infile.close();
	
	if ((int)calls.size() != target_size)
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): target haplosome vector has size " << target_size << " but " << calls.size() << " call lines found." << EidosTerminate();
	
	// Instantiate the mutations; NOTE THAT THE STACKING POLICY IS NOT CHECKED HERE, AS THIS IS NOT CONSIDERED THE ADDITION OF A MUTATION!
	std::vector<MutationIndex> mutation_indices;
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	for (int mut_index = 0; mut_index < segsites; ++mut_index)
	{
		slim_position_t position = positions[mut_index];
		double selection_coeff = mutation_type_ptr->DrawEffectForTrait(0);	// FIXME MULTITRAIT
		slim_objectid_t subpop_index = -1;
		slim_tick_t origin_tick = community.Tick();
		int8_t nucleotide = -1;
		
		if (nucleotide_based && mutation_type_ptr->nucleotide_based_)
		{
			// select a nucleotide that is different from the ancestral state at this position
			int8_t ancestral = (int8_t)chromosome->AncestralSequence()->NucleotideAtIndex(position);
			
			nucleotide = (int8_t)Eidos_rng_uniform_int(rng, 3);	// 0, 1, 2
			
			if (nucleotide == ancestral)
				nucleotide++;
		}
		
		MutationIndex new_mut_index = mutation_block->NewMutationFromBlock();
		
		Mutation *new_mut = new (mut_block_ptr + new_mut_index) Mutation(mutation_type_ptr, chromosome->Index(), position, static_cast<slim_effect_t>(selection_coeff), mutation_type_ptr->DefaultDominanceForTrait(0), subpop_index, origin_tick, nucleotide);	// FIXME MULTITRAIT
		
		// This mutation type might not be used by any genomic element type (i.e. might not already be vetted), so we need to check and set pure_neutral_
		if (selection_coeff != 0.0)
		{
			species.pure_neutral_ = false;
			
			// the selection coefficient was drawn from the mutation type's DFE, so there is no need to set all_pure_neutral_DFE_
			//mutation_type_ptr->all_pure_neutral_DFE_ = false;
		}
		
		// add it to our local map, so we can find it when making haplosomes, and to the population's mutation registry
		pop.MutationRegistryAdd(new_mut);
		mutation_indices.emplace_back(new_mut_index);
	}
	
	// Sort the mutations by position so we can add them in order, and make an "order" vector for accessing calls in the sorted order
	std::vector<int64_t> order_vec = EidosSortIndexes(positions);
	
	std::sort(mutation_indices.begin(), mutation_indices.end(), [mut_block_ptr](MutationIndex i1, MutationIndex i2) {return (mut_block_ptr + i1)->position_ < (mut_block_ptr + i2)->position_;});
	
	// Add the mutations to the target haplosomes, recording a new derived state with each addition
#ifndef _OPENMP
	MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(omp_get_thread_num());	// when not parallel, we have only one MutationRunContext
#endif
	
	for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
	{
		Haplosome *haplosome = targets_data[haplosome_index];
		
		if (haplosome->IsNull())
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromMS): readHaplosomesFromMS() does not allow null haplosomes in the target haplosome vector." << EidosTerminate();
		
		bool haplosome_started_empty = (haplosome->mutation_count() == 0);
		slim_position_t mutrun_length = haplosome->mutrun_length_;
		slim_mutrun_index_t current_run_index = -1;
		MutationRun *current_mutrun = nullptr;
		std::string &haplosome_string = calls[haplosome_index];
		
		for (int segsite_index = 0; segsite_index < segsites; ++segsite_index)
		{
			int64_t call_index = order_vec[segsite_index];
			char call = haplosome_string[call_index];
			
			if (call == '1')
			{
				MutationIndex mut_index = mutation_indices[segsite_index];
				Mutation *mut = mut_block_ptr + mut_index;
				slim_position_t mut_pos = mut->position_;
				slim_mutrun_index_t mut_mutrun_index = (slim_mutrun_index_t)(mut_pos / mutrun_length);
				
				if (mut_mutrun_index != current_run_index)
				{
#ifdef _OPENMP
					// When parallel, the MutationRunContext depends upon the position in the haplosome
					MutationRunContext &mutrun_context = species.ChromosomeMutationRunContextForMutationRunIndex(mut_mutrun_index);
#endif
					
					current_run_index = mut_mutrun_index;
					
					// We use WillModifyRun() because these are existing haplosomes we didn't create, and their runs may be shared; we have
					// no way to tell.  We avoid making excessive mutation run copies by calling this only once per mutrun per haplosome.
					current_mutrun = haplosome->WillModifyRun(mut_mutrun_index, mutrun_context);
				}
				
				// If the haplosome started empty, we can add mutations to the end with emplace_back(); if it did not, then they need to be inserted
				if (haplosome_started_empty)
					current_mutrun->emplace_back(mut_index);
				else
					current_mutrun->insert_sorted_mutation(mut_block_ptr, mut_index);
				
				if (recording_mutations)
					species.RecordNewDerivedState(haplosome, mut_pos, *haplosome->derived_mutation_ids_at_position(mut_block_ptr, mut_pos));
			}
		}
	}
	
	// Return the instantiated mutations
	int mutation_count = (int)mutation_indices.size();
	EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class))->resize_no_initialize_RR(mutation_count);
	
	for (int mut_index = 0; mut_index < mutation_count; ++mut_index)
		vec->set_object_element_no_check_no_previous_RR(mut_block_ptr + mutation_indices[mut_index], mut_index);
	
	return EidosValue_Object_SP(vec);
}

//	*********************	+ (o<Mutation>)readHaplosomesFromVCF(s$ filePath = NULL, [Nio<MutationType> mutationType = NULL])
//
EidosValue_SP Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_interpreter)
	// BEWARE: This method shares a great deal of code with Individual_Class::ExecuteMethod_readIndividualsFromVCF().  Maintain in parallel.
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF(): SLiM global state read");
	
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *mutationType_value = p_arguments[1].get();
	
	// SPECIES CONSISTENCY CHECK
	if (p_target->Count() == 0)
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): " << "readHaplosomesFromVCF() requires a target Haplosome vector of length 1 or more, so that the species of the target can be determined." << EidosTerminate();
	
	Species *species = Community::SpeciesForHaplosomes(p_target);
	
	if (!species)
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): " << "readHaplosomesFromVCF() requires that all target haplosomes belong to the same species." << EidosTerminate();
	
	species->population_.CheckForDeferralInHaplosomes(p_target, "Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF");
	
	MutationBlock *mutation_block = species->SpeciesMutationBlock();
	Mutation *mut_block_ptr = mutation_block->mutation_buffer_;
	
	// All haplosomes must belong to the same chromosome, and in multichrom models the CHROM field must match its symbol
	const std::vector<Chromosome *> &chromosomes = species->Chromosomes();
	bool model_is_multi_chromosome = (chromosomes.size() > 1);
	Haplosome * const *targets_data = (Haplosome * const *)p_target->ObjectData();
	int target_size = p_target->Count();
	Haplosome *haplosome_0 = targets_data[0];
	slim_chromosome_index_t chromosome_index = haplosome_0->chromosome_index_;
	Chromosome *chromosome = chromosomes[chromosome_index];
	std::string chromosome_symbol = chromosome->Symbol();
	
	if (species->Chromosomes().size() > 1)
	{
		// We have to check for consistency if there's more than one chromosome
		for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
			if (targets_data[haplosome_index]->chromosome_index_ != chromosome_index)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): " << "readHaplosomesFromVCF() requires that all target haplosomes are associated with the same chromosome." << EidosTerminate();
	}
	
	Community &community = species->community_;
	Population &pop = species->population_;
	slim_position_t last_position = chromosome->last_position_;
	bool recording_mutations = species->RecordingTreeSequenceMutations();
	bool nucleotide_based = species->IsNucleotideBased();
	std::string file_path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(filePath_value->StringAtIndex_NOCAST(0, nullptr)));
	MutationType *default_mutation_type_ptr = nullptr;
	
	if (mutationType_value->Type() != EidosValueType::kValueNULL)
		default_mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutationType_value, 0, &community, species, "readHaplosomesFromVCF()");			// SPECIES CONSISTENCY CHECK
	
	// Parse the whole input file and retain the information from it
	std::ifstream infile(file_path);
	
	if (!infile)
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): could not read file at path " << file_path << "." << EidosTerminate();
	
	std::string line, sub;
	int parse_state = 0;
	int sample_id_count = 0;
	bool info_MID_defined = false, info_S_defined = false, info_DOM_defined = false, info_PO_defined = false;
	bool info_GO_defined = false, info_TO_defined = false, info_MT_defined = false, /*info_AA_defined = false,*/ info_NONNUC_defined = false;
	std::vector<std::pair<slim_position_t, std::string>> call_lines;
	
	while (!infile.eof())
	{
		getline(infile, line);
		
		switch (parse_state)
		{
			case 0:
			{
				// In header, parsing ## lines, until we get to the #CHROM line; the point of this is that we only want to interpret
				// INFO fields like MID, S, etc. as having their SLiM-specific meaning if their SLiM-specific definition is present
				if (line.compare(0, 2, "##") == 0)
				{
					if (line == "##INFO=<ID=MID,Number=.,Type=Integer,Description=\"Mutation ID in SLiM\">")	info_MID_defined = true;
					if (line == "##INFO=<ID=S,Number=.,Type=Float,Description=\"Selection Coefficient\">")		info_S_defined = true;
					if (line == "##INFO=<ID=DOM,Number=.,Type=Float,Description=\"Dominance\">")				info_DOM_defined = true;
					if (line == "##INFO=<ID=PO,Number=.,Type=Integer,Description=\"Population of Origin\">")	info_PO_defined = true;
					if (line == "##INFO=<ID=GO,Number=.,Type=Integer,Description=\"Generation of Origin\">")	info_GO_defined = true;
					if (line == "##INFO=<ID=TO,Number=.,Type=Integer,Description=\"Tick of Origin\">")			info_TO_defined = true;		// SLiM 4 emits TO (tick) instead of GO (generation)
					if (line == "##INFO=<ID=MT,Number=.,Type=Integer,Description=\"Mutation Type\">")			info_MT_defined = true;
					/*if (line == "##INFO=<ID=AA,Number=1,Type=String,Description=\"Ancestral Allele\">")			info_AA_defined = true;*/		// this one is standard, so we don't require this definition
					if (line == "##INFO=<ID=NONNUC,Number=0,Type=Flag,Description=\"Non-nucleotide-based\">")	info_NONNUC_defined = true;
				}
				else if (line.compare(0, 1, "#") == 0)
				{
					static const char *header_fields[9] = {"CHROM", "POS", "ID", "REF", "ALT", "QUAL", "FILTER", "INFO", "FORMAT"};
					std::istringstream iss(line);
					
					iss.get();	// eat the initial #
					
					// verify that the expected standard columns are present
					for (const char *header_field : header_fields)
					{
						if (!(iss >> sub))
							EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): missing VCF header '" << header_field << "'." << EidosTerminate();
						if (sub != header_field)
							EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): expected VCF header '" << header_field << "', saw '" << sub << "'." << EidosTerminate();
					}
					
					// the remaining columns are sample IDs; we don't care what they are, we just count them
					while (iss >> sub)
						sample_id_count++;
					
					// now the remainder of the file should be call lines
					parse_state = 1;
				}
				else
				{
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): unexpected line in VCF header: '" << line << "'." << EidosTerminate();
				}
				break;
			}
			case 1:
			{
				// In call lines, fields are separated by tabs, and could theoretically contain spaces; here we just read a whole line,
				// extract the position field for the mutation, and save the line indexed by its mutation's position for later handling
				if (line.length() == 0)
					break;
				
				std::istringstream iss(line);
				
				std::getline(iss, sub, '\t');	// CHROM
				
				if (model_is_multi_chromosome)
				{
					// in multi-chromosome models the CHROM value must match the associated chromosome of the haplosomes
					if (sub != chromosome_symbol)
						EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): the CHROM field's value (\"" << sub << "\") in a call line does not match the symbol (\"" << chromosome_symbol << "\") for the focal chromosome with which the target haplosomes are associated.  In multi-chromosome models, the CHROM field is required to match the chromosome symbol to prevent bugs." << EidosTerminate();
				}
				else
				{
					// in single-chromosome models the CHROM value must be consistent across the whole file, but need not match
					if (call_lines.size() == 0)
						chromosome_symbol = sub;	// first call line's CHROM symbol gets remembered
					else if (sub != chromosome_symbol)
						EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): the CHROM field's value (\"" << sub << "\") in a call line does not match the initial CHROM field's value (\"" << chromosome_symbol << "\").  In single-chromosome models, the CHROM field is required to have a single consistent value across all call lines to prevent bugs." << EidosTerminate();
				}
				
				std::getline(iss, sub, '\t');	// POS
				
				int64_t pos = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr) - 1;		// -1 because VCF uses 1-based positions
				
				if ((pos < 0) || (pos > last_position))
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file POS value " << pos << " out of range." << EidosTerminate();
				
				call_lines.emplace_back(pos, line);
				break;
			}
			default:
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): (internal error) unhandled case." << EidosTerminate();
		}
	}
	
	infile.close();
	
	// sort the call lines by position, so that we can add them to empty haplosomes efficiently
	std::sort(call_lines.begin(), call_lines.end(), [ ](const std::pair<slim_position_t, std::string> &l1, const std::pair<slim_position_t, std::string> &l2) {return l1.first < l2.first;});
	
	// cache target haplosomes and determine whether they are initially empty, in which case we can do fast mutation addition with emplace_back()
	std::vector<Haplosome *> targets;
	std::vector<slim_mutrun_index_t> target_last_mutrun_modified;
	std::vector<MutationRun *> target_last_mutrun;
	bool all_target_haplosomes_started_empty = true;
	
	for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
	{
		Haplosome *haplosome = targets_data[haplosome_index];
		
		// null haplosomes are silently excluded from the target list, for convenience
		if (!haplosome->IsNull())
		{
			if (haplosome->mutation_count() != 0)
				all_target_haplosomes_started_empty = false;
			
			targets.emplace_back(haplosome);
			target_last_mutrun_modified.emplace_back(-1);
			target_last_mutrun.emplace_back(nullptr);
		}
	}
	
	target_size = (int)targets.size();	// adjust for possible exclusion of null haplosomes
	
	// parse all the call lines, instantiate their mutations, and add the mutations to the target haplosomes
#ifndef _OPENMP
	MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForThread(omp_get_thread_num());	// when not parallel, we have only one MutationRunContext
#endif
	std::vector<MutationIndex> mutation_indices;
	bool has_initial_mutations = (gSLiM_next_mutation_id != 0);
	
	for (std::pair<slim_position_t, std::string> &call_line : call_lines)
	{
		slim_position_t mut_position = call_line.first;
		std::istringstream iss(call_line.second);
		std::string ref_str, alt_str, info_str;
		
		std::getline(iss, sub, '\t');		// CHROM; don't care (already checked it above)
		std::getline(iss, sub, '\t');		// POS; already fetched
		std::getline(iss, sub, '\t');		// ID; don't care
		std::getline(iss, ref_str, '\t');	// REF
		std::getline(iss, alt_str, '\t');	// ALT
		std::getline(iss, sub, '\t');		// QUAL; don't care
		std::getline(iss, sub, '\t');		// FILTER; don't care
		std::getline(iss, info_str, '\t');	// INFO
		std::getline(iss, sub, '\t');		// FORMAT; don't care (GT must be first, according to the standard; we don't check)
		
		// parse/validate the REF nucleotide
		int8_t ref_nuc;
		
		if (ref_str == "A")			ref_nuc = 0;
		else if (ref_str == "C")	ref_nuc = 1;
		else if (ref_str == "G")	ref_nuc = 2;
		else if (ref_str == "T")	ref_nuc = 3;
		else						EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file REF value must be A/C/G/T." << EidosTerminate();
		
		// parse/validate the ALT nucleotides
		std::vector<std::string> alt_substrs = Eidos_string_split(alt_str, ",");
		std::vector<int8_t> alt_nucs;
		
		for (std::string &alt_substr : alt_substrs)
		{
			if (alt_substr == "A")			alt_nucs.emplace_back(0);
			else if (alt_substr == "C")		alt_nucs.emplace_back(1);
			else if (alt_substr == "G")		alt_nucs.emplace_back(2);
			else if (alt_substr == "T")		alt_nucs.emplace_back(3);
			else							EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file ALT value must be A/C/G/T." << EidosTerminate();
		}
		
		std::size_t alt_allele_count = alt_nucs.size();
		
		// parse/validate the INFO fields that we recognize
		std::vector<std::string> info_substrs = Eidos_string_split(info_str, ";");
		std::vector<slim_mutationid_t> info_mutids;
		std::vector<slim_effect_t> info_selcoeffs;
		std::vector<slim_effect_t> info_domcoeffs;
		std::vector<slim_objectid_t> info_poporigin;
		std::vector<slim_tick_t> info_tickorigin;
		std::vector<slim_objectid_t> info_muttype;
		int8_t info_ancestral_nuc = -1;
		bool info_is_nonnuc = false;
		
		for (std::string &info_substr : info_substrs)
		{
			if (info_MID_defined && (info_substr.compare(0, 4, "MID=") == 0))		// Mutation ID
			{
				std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(4), ",");
				
				for (std::string &value_substr : value_substrs)
					info_mutids.emplace_back((slim_mutationid_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
				
				if (info_mutids.size() && has_initial_mutations)
				{
					if (!gEidosSuppressWarnings)
					{
						if (!community.warned_readFromVCF_mutIDs_unused_)
						{
							p_interpreter.ErrorOutputStream() << "#WARNING (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): readHaplosomesFromVCF(): the VCF file specifies mutation IDs with the MID field, but some mutation IDs have already been used so uniqueness cannot be guaranteed.  Use of mutation IDs is therefore disabled; mutations will not receive the mutation ID requested in the file.  To fix this warning, remove the MID field from the VCF file before reading.  To get readHaplosomesFromVCF() to use the specified mutation IDs, load the VCF file into a model that has never simulated a mutation, and has therefore not used any mutation IDs." << std::endl;
							community.warned_readFromVCF_mutIDs_unused_ = true;
						}
					}
					
					// disable use of MID for this read
					info_MID_defined = false;
					info_mutids.clear();
				}
			}
			else if (info_S_defined && (info_substr.compare(0, 2, "S=") == 0))		// Selection Coefficient
			{
				std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(2), ",");
				
				for (std::string &value_substr : value_substrs)
					info_selcoeffs.emplace_back(EidosInterpreter::FloatForString(value_substr, nullptr));
			}
			else if (info_DOM_defined && (info_substr.compare(0, 4, "DOM=") == 0))	// Dominance Coefficient
			{
				std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(4), ",");
				
				for (std::string &value_substr : value_substrs)
					info_domcoeffs.emplace_back(EidosInterpreter::FloatForString(value_substr, nullptr));
			}
			else if (info_PO_defined && (info_substr.compare(0, 3, "PO=") == 0))	// Population of Origin
			{
				std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(3), ",");
				
				for (std::string &value_substr : value_substrs)
					info_poporigin.emplace_back((slim_objectid_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
			}
			else if (info_TO_defined && (info_substr.compare(0, 3, "TO=") == 0))	// Tick of Origin
			{
				std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(3), ",");
				
				for (std::string &value_substr : value_substrs)
					info_tickorigin.emplace_back((slim_tick_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
			}
			else if (info_GO_defined && (info_substr.compare(0, 3, "GO=") == 0))	// Generation of Origin - emitted by SLiM 3, treated as TO here
			{
				std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(3), ",");
				
				for (std::string &value_substr : value_substrs)
					info_tickorigin.emplace_back((slim_tick_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
			}
			else if (info_MT_defined && (info_substr.compare(0, 3, "MT=") == 0))	// Mutation Type
			{
				std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(3), ",");
				
				for (std::string &value_substr : value_substrs)
					info_muttype.emplace_back((slim_objectid_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
			}
			else if (/* info_AA_defined && */ (info_substr.compare(0, 3, "AA=") == 0))	// Ancestral Allele; definition not required since it is a standard field
			{
				std::string aa_str = info_substr.substr(3);
				
				if (aa_str == "A")			info_ancestral_nuc = 0;
				else if (aa_str == "C")		info_ancestral_nuc = 1;
				else if (aa_str == "G")		info_ancestral_nuc = 2;
				else if (aa_str == "T")		info_ancestral_nuc = 3;
				else						EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file AA value must be A/C/G/T." << EidosTerminate();
			}
			else if (info_NONNUC_defined && (info_substr == "NONNUC"))				// Non-nucleotide-based
			{
				info_is_nonnuc = true;
			}
			
			if ((info_mutids.size() != 0) && (info_mutids.size() != alt_allele_count))
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file unexpected value count for MID field." << EidosTerminate();
			if ((info_selcoeffs.size() != 0) && (info_selcoeffs.size() != alt_allele_count))
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file unexpected value count for S field." << EidosTerminate();
			if ((info_domcoeffs.size() != 0) && (info_domcoeffs.size() != alt_allele_count))
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file unexpected value count for DOM field." << EidosTerminate();
			if ((info_poporigin.size() != 0) && (info_poporigin.size() != alt_allele_count))
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file unexpected value count for PO field." << EidosTerminate();
			if ((info_tickorigin.size() != 0) && (info_tickorigin.size() != alt_allele_count))
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file unexpected value count for GO or TO field." << EidosTerminate();
			if ((info_muttype.size() != 0) && (info_muttype.size() != alt_allele_count))
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file unexpected value count for MT field." << EidosTerminate();
		}
		
		// read the genotype data for each sample id, which might be diploid or haploid, and might have data beyond GT
		std::vector<int> genotype_calls;
		
		for (int sample_index = 0; sample_index < sample_id_count; ++sample_index)
		{
			if (iss.eof())
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file call line ended unexpectedly before the last sample." << EidosTerminate();
			
			std::getline(iss, sub, '\t');
			
			// extract just the GT field if others are present
			std::size_t colon_pos = sub.find_first_of(':');
			
			if (colon_pos != std::string::npos)
				sub = sub.substr(0, colon_pos);
			
			// separate haploid calls that are joined by | or /; this is the hotspot of the whole method, so we try to be efficient here
			bool call_handled = false;
			
			if ((sub.length() == 3) && ((sub[1] == '|') || (sub[1] == '/')))
			{
				// diploid, both single-digit
				char sub_ch1 = sub[0];
				char sub_ch2 = sub[2];
				
				if ((sub_ch1 >= '0') && (sub_ch1 <= '9') && (sub_ch2 >= '0') && (sub_ch2 <= '9'))
				{
					int genotype_call1 = (int)(sub_ch1 - '0');
					int genotype_call2 = (int)(sub_ch2 - '0');
					
					if ((genotype_call1 < 0) || (genotype_call1 > (int)alt_allele_count) || (genotype_call2 < 0) || (genotype_call2 > (int)alt_allele_count))	// 0 is REF, 1..n are ALT alleles
						EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file call out of range (does not correspond to a REF or ALT allele in the call line)." << EidosTerminate();
					
					genotype_calls.emplace_back(genotype_call1);
					genotype_calls.emplace_back(genotype_call2);
					call_handled = true;
				}
			}
			else if (sub.length() == 1)
			{
				char sub_ch = sub[0];
				
				if (sub_ch == '~')
				{
					// If the call is ~, a tilde, it indicates that no genetic information is present
					// (this is the case for a female if we're reading Y-chromosome data, for example).
					// We do not add anything to genotype_calls; it is as if this call does not exist.
					// Note that this is not part of the VCF standard; it had to be invented for SLiM.
					call_handled = true;
				}
				else
				{
					// haploid, single-digit
					if ((sub_ch >= '0') && (sub_ch <= '9'))
					{
						int genotype_call = (int)(sub_ch - '0');
						
						if ((genotype_call < 0) || (genotype_call > (int)alt_allele_count))	// 0 is REF, 1..n are ALT alleles
							EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file call out of range (does not correspond to a REF or ALT allele in the call line)." << EidosTerminate();
						
						genotype_calls.emplace_back(genotype_call);
						call_handled = true;
					}
				}
			}
			
			if (!call_handled)
			{
				std::vector<std::string> genotype_substrs;
				
				if (sub.find('|') != std::string::npos)
					genotype_substrs = Eidos_string_split(sub, "|");	// phased
				else if (sub.find('/') != std::string::npos)
					genotype_substrs = Eidos_string_split(sub, "/");	// unphased; we don't worry about that
				else
					genotype_substrs.emplace_back(sub);					// haploid, presumably
				
				if ((genotype_substrs.size() < 1) || (genotype_substrs.size() > 2))
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file genotype calls must be diploid or haploid; " << genotype_substrs.size() << " calls found in one sample." << EidosTerminate();
				
				// extract the calls' integer values, validate them, and keep them; we don't care which call was in which sample, we just preserve their order
				for (std::string &genotype_substr : genotype_substrs)
				{
					std::size_t genotype_call = EidosInterpreter::NonnegativeIntegerForString(genotype_substr, nullptr);
					
					if (/*(genotype_call < 0) ||*/ (genotype_call > alt_allele_count))	// 0 is REF, 1..n are ALT alleles
						EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file call out of range (does not correspond to a REF or ALT allele in the call line)." << EidosTerminate();
					
					genotype_calls.emplace_back((int)genotype_call);
				}
			}
		}
		
		if (!iss.eof())
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file call line has unexpected entries following the last sample." << EidosTerminate();
		if ((int)genotype_calls.size() != target_size)
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): target haplosome vector has size " << target_size << " but " << genotype_calls.size() << " calls were found in one call line." << EidosTerminate();
		
		// We have one call for each non-null target haplosome, so the requirement for this function is met.
		// Note that there is no checking that a ~ matches the position of a null haplosome in the target
		// vector; we have no concept of "individuals", we just match haplosomes to calls for each line.
		// The Individual version of readHaplosomesFromVCF() can be smarter, since it understands individuals.
		
		// instantiate the mutations involved in this call line; the REF allele represents no mutation, ALT alleles are each separate mutations
		std::vector<MutationIndex> alt_allele_mut_indices;
		
		for (std::size_t alt_allele_index = 0; alt_allele_index < alt_allele_count; ++alt_allele_index)
		{
			// figure out the mutation type; if specified with MT, look it up, otherwise use the default supplied
			MutationType *mutation_type_ptr = default_mutation_type_ptr;
			
			if (info_muttype.size() > 0)
			{
				slim_objectid_t mutation_type_id = info_muttype[alt_allele_index];
                
                mutation_type_ptr = species->MutationTypeWithID(mutation_type_id);
				
				if (!mutation_type_ptr)
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file MT field references a mutation type m" << mutation_type_id << " that is not defined." << EidosTerminate();
			}
			
			if (!mutation_type_ptr)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): VCF file MT field missing, but no default mutation type was supplied in the mutationType parameter." << EidosTerminate();
			
			// get the dominance coefficient from DOM, or use the default coefficient from the mutation type
			slim_effect_t dominance_coeff;
			
			if (info_domcoeffs.size() > 0)
				dominance_coeff = info_domcoeffs[alt_allele_index];
			else
				dominance_coeff = mutation_type_ptr->DefaultDominanceForTrait(0);	// FIXME MULTITRAIT
			
			// get the selection coefficient from S, or draw one from the mutation type
			slim_effect_t selection_coeff;
			
			if (info_selcoeffs.size() > 0)
				selection_coeff = info_selcoeffs[alt_allele_index];
			else
				selection_coeff = static_cast<slim_effect_t>(mutation_type_ptr->DrawEffectForTrait(0));	// FIXME MULTITRAIT
			
			// get the subpop index from PO, or set to -1; no bounds checking on this
			slim_objectid_t subpop_index = -1;
			
			if (info_poporigin.size() > 0)
				subpop_index = info_poporigin[alt_allele_index];
			
			// get the origin tick from gO, or set to the current tick; no bounds checking on this
			slim_tick_t origin_tick;
			
			if (info_tickorigin.size() > 0)
				origin_tick = info_tickorigin[alt_allele_index];
			else
				origin_tick = community.Tick();
			
			// figure out the nucleotide and do nucleotide-related checks
			int8_t alt_allele_nuc = alt_nucs[alt_allele_index];		// must be defined, in all cases, but might be ignored
			int8_t nucleotide;
			
			if (nucleotide_based)
			{
				if (info_NONNUC_defined)
				{
					// We are reading a SLiM-generated VCF file that uses NONNUC to designate non-nucleotide-based mutations
					if (info_is_nonnuc)
					{
						// This call line is marked NONNUC, so there is no associated nucleotide; check against the mutation type
						if (mutation_type_ptr->nucleotide_based_)
							EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): a mutation marked NONNUC cannot use a nucleotide-based mutation type." << EidosTerminate();
						
						nucleotide = -1;
					}
					else
					{
						// This call line is not marked NONNUC, so it represents nucleotide-based alleles
						if (!mutation_type_ptr->nucleotide_based_)
							EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): a nucleotide-based mutation cannot use a non-nucleotide-based mutation type." << EidosTerminate();
						if (ref_nuc != info_ancestral_nuc)
							EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): the REF nucleotide does not match the AA nucleotide." << EidosTerminate();
						
						int8_t ancestral = (int8_t)chromosome->AncestralSequence()->NucleotideAtIndex(mut_position);
						
						if (ancestral != ref_nuc)
							EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): the REF/AA nucleotide does not match the ancestral nucleotide at the same position; a matching ancestral nucleotide sequence must be set prior to calling readHaplosomesFromVCF()." << EidosTerminate();
						
						nucleotide = alt_allele_nuc;
					}
				}
				else
				{
					// We are reading a generic VCF file that does not use NONNUC, so we follow the mutation type's lead; if it is nucleotide-based, we use the nucleotide specified
					if (mutation_type_ptr->nucleotide_based_)
					{
						// The mutation type is nucleotide-based, so use the nucleotide specified; in this case we ignore REF and AA, however
						nucleotide = alt_allele_nuc;
					}
					else
					{
						// The mutation type is non-nucleotide-based, so we ignore the nucleotide supplied, as well as REF/AA
						nucleotide = -1;
					}
				}
			}
			else
			{
				// We are a non-nucleotide-based model, so NONNUC should not be defined; we do not understand nucleotides and will ignore them
				if (info_NONNUC_defined)
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF): cannot read a VCF file generated by a nucleotide-based model into a non-nucleotide-based model." << EidosTerminate();
				
				nucleotide = -1;
			}
			
			// instantiate the mutation with the values decided upon
			MutationIndex new_mut_index = mutation_block->NewMutationFromBlock();
			Mutation *new_mut;
			
			if (info_mutids.size() > 0)
			{
				// a mutation ID was supplied; we use it blindly, having checked above that we are in the case where this is legal
				slim_mutationid_t mut_mutid = info_mutids[alt_allele_index];
				
				new_mut = new (mut_block_ptr + new_mut_index) Mutation(mut_mutid, mutation_type_ptr, chromosome->Index(), mut_position, selection_coeff, dominance_coeff, subpop_index, origin_tick, nucleotide);
			}
			else
			{
				// no mutation ID supplied, so use whatever is next
				new_mut = new (mut_block_ptr + new_mut_index) Mutation(mutation_type_ptr, chromosome->Index(), mut_position, selection_coeff, dominance_coeff, subpop_index, origin_tick, nucleotide);
			}
			
			// This mutation type might not be used by any genomic element type (i.e. might not already be vetted), so we need to check and set pure_neutral_
			// The selection coefficient might have been supplied by the user (i.e., not be from the mutation type's DFE), so we set all_pure_neutral_DFE_ also
			if (selection_coeff != 0.0)
			{
				species->pure_neutral_ = false;
				mutation_type_ptr->all_pure_neutral_DFE_ = false;
			}
			
			// add it to our local map, so we can find it when making haplosomes, and to the population's mutation registry
			pop.MutationRegistryAdd(new_mut);
			alt_allele_mut_indices.emplace_back(new_mut_index);
			mutation_indices.emplace_back(new_mut_index);
		}
		
		// add the mutations to the appropriate haplosomes and record the new derived states
		for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
		{
			int call = genotype_calls[haplosome_index];
			
			if (call != 0)
			{
				Haplosome *haplosome = targets[haplosome_index];
				slim_mutrun_index_t &haplosome_last_mutrun_modified = target_last_mutrun_modified[haplosome_index];
				MutationRun *&haplosome_last_mutrun = target_last_mutrun[haplosome_index];
				slim_position_t mutrun_length = haplosome->mutrun_length_;
				MutationIndex mut_index = alt_allele_mut_indices[call - 1];
				slim_mutrun_index_t mut_mutrun_index = (slim_mutrun_index_t)(mut_position / mutrun_length);
				
				if (mut_mutrun_index != haplosome_last_mutrun_modified)
				{
#ifdef _OPENMP
					// When parallel, the MutationRunContext depends upon the position in the haplosome
					MutationRunContext &mutrun_context = species->ChromosomeMutationRunContextForMutationRunIndex(mut_mutrun_index);
#endif
					
					// We use WillModifyRun() because these are existing haplosomes we didn't create, and their runs may be shared; we have
					// no way to tell.  We avoid making excessive mutation run copies by calling this only once per mutrun per haplosome.
					haplosome_last_mutrun = haplosome->WillModifyRun(mut_mutrun_index, mutrun_context);
					haplosome_last_mutrun_modified = mut_mutrun_index;
				}
				
				// If the haplosome started empty, we can add mutations to the end with emplace_back(); if it did not, then they need to be inserted
				if (all_target_haplosomes_started_empty)
					haplosome_last_mutrun->emplace_back(mut_index);
				else
					haplosome_last_mutrun->insert_sorted_mutation(mut_block_ptr, mut_index);
				
				if (recording_mutations)
					species->RecordNewDerivedState(haplosome, mut_position, *haplosome->derived_mutation_ids_at_position(mut_block_ptr, mut_position));
			}
		}
	}
	
	// Return the instantiated mutations
	int mutation_count = (int)mutation_indices.size();
	EidosValue_Object *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_Mutation_Class))->resize_no_initialize_RR(mutation_count);
	
	for (int mut_index = 0; mut_index < mutation_count; ++mut_index)
		vec->set_object_element_no_check_no_previous_RR(mut_block_ptr + mutation_indices[mut_index], mut_index);
	
	return EidosValue_Object_SP(vec);
}

//	*********************	+ (void)removeMutations([No<Mutation> mutations = NULL], [logical$ substitute = F])
//
EidosValue_SP Haplosome_Class::ExecuteMethod_removeMutations(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_interpreter)
	EidosValue *mutations_value = p_arguments[0].get();
	EidosValue *substitute_value = p_arguments[1].get();
	
	int target_size = p_target->Count();
	
	if (target_size == 0)
		return gStaticEidosValueVOID;
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForHaplosomes(p_target);
	
	if (!species)
		EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_removeMutations): removeMutations() requires that all target haplosomes belong to the same species." << EidosTerminate();
	
	MutationBlock *mutation_block = species->SpeciesMutationBlock();
	Mutation *mut_block_ptr = mutation_block->mutation_buffer_;
	
	// All haplosomes must belong to the same chromosome, and all mutations being added must belong to that chromosome too.
	// It's important that a mismatch result in an error; attempts to add mutations to chromosomes inconsistently should be flagged.
	int mutations_count = mutations_value->Count();
	Haplosome * const *targets_data = (Haplosome * const *)p_target->ObjectData();
	Haplosome *haplosome_0 = targets_data[0];
	slim_chromosome_index_t chromosome_index = haplosome_0->chromosome_index_;
	
	if (species->Chromosomes().size() > 1)
	{
		// We have to check for consistency if there's more than one chromosome
		for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
			if (targets_data[haplosome_index]->chromosome_index_ != chromosome_index)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_removeMutations): removeMutations() requires that all target haplosomes are associated with the same chromosome." << EidosTerminate();
		
		if (mutations_value->Type() != EidosValueType::kValueNULL)
		{
			Mutation * const *mutations = (Mutation * const *)mutations_value->ObjectData();
			
			for (int value_index = 0; value_index < mutations_count; ++value_index)
				if (mutations[value_index]->chromosome_index_ != chromosome_index)
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_removeMutations): removeMutations() requires that all mutations to be removed are associated with the same chromosome as the target haplosomes." << EidosTerminate();
		}
	}
	
	Chromosome *chromosome = species->Chromosomes()[chromosome_index];
	
	species->population_.CheckForDeferralInHaplosomes(p_target, "Haplosome_Class::ExecuteMethod_readHaplosomesFromVCF");
	
	Community &community = species->community_;
	Population &pop = species->population_;
	slim_tick_t tick = community.Tick();
	bool create_substitutions = substitute_value->LogicalAtIndex_NOCAST(0, nullptr);
	bool recording_tree_sequence_mutations = species->RecordingTreeSequenceMutations();
	bool any_nonneutral_removed = false;
	
	// Use the 0th haplosome in the target to find out what the mutation run length is, so we can calculate run indices
	slim_position_t mutrun_length = haplosome_0->mutrun_length_;
	
	// TIMING RESTRICTION
	if (community.executing_species_ == species)
	{
		if (community.executing_block_type_ == SLiMEidosBlockType::SLiMEidosModifyChildCallback)
		{
			// Check that we're not inside a modifyChild() callback, or if we are, that the only haplosomes being modified belong to the new child.
			// This prevents problems with retracting the proposed child when tree-sequence recording is enabled; other extraneous changes must
			// not be backed out, and it's hard to separate, e.g., what's a child-related new mutation from an extraneously added new mutation.
			// Note that the other Haplosome methods that add/remove mutations perform the same check, and should be maintained in parallel.
			Individual *focal_modification_child = community.focal_modification_child_;
			
			if (focal_modification_child)
			{
				for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
				{
					Haplosome *target_haplosome = targets_data[haplosome_index];
					
					if (target_haplosome->individual_ != focal_modification_child)
						EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_removeMutations): removeMutations() cannot be called on the currently executing species from within a modifyChild() callback to modify any haplosomes except those of the focal child being generated." << EidosTerminate();
				}
			}
			
			// This is actually only a problem when tree recording is on, but for consistency we outlaw it in all cases.  When a substitution
			// is created, it is added to the derived state of every haplosome, which is a side effect that can't be retracted if the modifyChild()
			// callback rejects the proposed child, so it has to be prohibited up front.  Anyway it would be a very strange thing to do.
			if (create_substitutions)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_removeMutations): removeMutations() cannot be called on the currently executing species from within a modifyChild() callback to create a substitution, because that would have side effects on haplosomes other than those of the focal child being generated." << EidosTerminate();
		}
		else if ((community.executing_block_type_ == SLiMEidosBlockType::SLiMEidosRecombinationCallback) ||
				 (community.executing_block_type_ == SLiMEidosBlockType::SLiMEidosMutationCallback))
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_removeMutations): removeMutations() cannot be called on the currently executing species from within a recombination() or mutation() callback." << EidosTerminate();
	}
	
	if (mutations_value->Type() == EidosValueType::kValueNULL)
	{
		// This is the "remove all mutations" case, for which we have no vector of mutations to remove.  In this case we do the tree
		// sequence recording first, since we can add new empty derived states just at the locations where mutations presently exist.
		// Then we just go through and clear out the target haplosomes.
		if (create_substitutions)
			EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_removeMutations): in removeMutations() substitute may not be T if mutations is NULL; an explicit vector of mutations to be substituted must be supplied." << EidosTerminate();
		
		// TREE SEQUENCE RECORDING
		if (recording_tree_sequence_mutations)
		{
			const std::vector<Mutation *> empty_mut_vector;
			
			for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
			{
				Haplosome *target_haplosome = targets_data[haplosome_index];
				
				if (target_haplosome->IsNull())
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_removeMutations): removeMutations() cannot be called on a null haplosome.  This error may be due to a break in backward compatibility in SLiM 3.7 involving addRecombinant() with haploid models; if that seems likely, please see the release notes." << EidosTerminate();
				
				for (HaplosomeWalker target_walker(target_haplosome); !target_walker.Finished(); target_walker.NextMutation())
				{
					Mutation *mut = target_walker.CurrentMutation();
					slim_position_t pos = mut->position_;
					
					species->RecordNewDerivedState(target_haplosome, pos, empty_mut_vector);
				}
			}
		}
		
		// Fetch haplosome pointers and check for null haplosomes up front
		std::vector<Haplosome *>target_haplosomes;
		
		for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
		{
			Haplosome *target_haplosome = targets_data[haplosome_index];
			
			if (target_haplosome->IsNull())
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_removeMutations): removeMutations() cannot be called on a null haplosome.  This error may be due to a break in backward compatibility in SLiM 3.7 involving addRecombinant() with haploid models; if that seems likely, please see the release notes." << EidosTerminate();
			
			target_haplosomes.push_back(target_haplosome);
		}
		
		// Now remove all mutations; we don't use bulk operations here because it is simpler to just set them all to the same empty run
		// BCH 8/12/2021: fixing this code to only reset mutation runs if they presently contain a mutation; do nothing for empty mutruns
		// This avoids a bunch of mutrun thrash in haploid models that remove all mutations from the second haplosome in modifyChild(), etc.
		int mutrun_count = haplosome_0->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			MutationRun *shared_empty_run = nullptr;	// different shared empty run for each mutrun index
			
			for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
			{
				Haplosome *target_haplosome = target_haplosomes[haplosome_index];
				const MutationRun *mutrun = target_haplosome->mutruns_[run_index];
				
				if (mutrun->size())
				{
					// Allocate the shared empty run lazily, since we might not need it (if we're removing mutations from haplosomes that are empty already)
					if (!shared_empty_run)
					{
						MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForMutationRunIndex(run_index);
						shared_empty_run = MutationRun::NewMutationRun(mutrun_context);
					}
					
					target_haplosome->mutruns_[run_index] = shared_empty_run;
				}
			}
		}
		
		// invalidate cached mutation refcounts; refcounts have changed
		pop.InvalidateMutationReferencesCache();
		
		// in this code path we just assume that nonneutral mutations might have been removed
		any_nonneutral_removed = true;
	}
	else
	{
		// If the user is creating substitutions for mutations, we now check for consistency at the end of the cycle, so that
		// we don't have a mutation still segregating while a substitution for it has also been created; see CheckMutationRegistry()
		// BCH 9/24/2021: Note that we cannot do the opposite check: checking that we only substitute a mutation when it has, in fact,
		// fixed.  We can't do that because there are models, such as the PAR (pseudo-autosomal region) recipe, that have different
		// fixation frequences for different parts of the haplosome because multiple chromosomes of different ploidy are being simulated.
		// Until we support multiple chromosomes more intrinsically, and can do that properly, substitution at less than frequency
		// 1.0 must be supported.  Note that this also means that we have to record new derived states here, because in some cases
		// (those same cases), the derived state for some haplosomes will have changed; if we do multiple chromosomes properly some day,
		// the recording of new derived states can probably be removed here, since no genetic state will have changed.
		if (create_substitutions)
			pop.SetMutationRegistryNeedsCheck();
		
		// SPECIES CONSISTENCY CHECK
		if (mutations_count)
		{
			Species *mutations_species = Community::SpeciesForMutations(mutations_value);
			
			if (mutations_species != species)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_removeMutations): removeMutations() requires that all mutations belong to the same species as the target haplosomes." << EidosTerminate();
		}
		
		// Construct a vector of mutations to remove that is sorted by position
		std::vector<Mutation *> mutations_to_remove;
		Mutation * const *mutations_data = (Mutation * const *)mutations_value->ObjectData();
		
		for (int value_index = 0; value_index < mutations_count; ++value_index)
		{
			Mutation *mut = mutations_data[value_index];
			
			if (mut->state_ != MutationState::kInRegistry)
				EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_removeMutations): removeMutations() cannot remove mutations that are not currently segregating (i.e., have either been fixed/substituted or lost)." << EidosTerminate();
			
			if (create_substitutions)
				mut->state_ = MutationState::kRemovedWithSubstitution;		// mark removed/substituted mutations with a special state so they get handled correctly later
			
			mutations_to_remove.emplace_back(mut);
			
			if (mut->selection_coeff_ != 0.0)
				any_nonneutral_removed = true;
		}
		
		std::sort(mutations_to_remove.begin(), mutations_to_remove.end(), [ ](Mutation *i1, Mutation *i2) {return i1->position_ < i2->position_;});
		
		// TREE SEQUENCE RECORDING
		// First, pre-plan the positions of new tree-seq derived states in anticipation of doing the removal.  We have to check
		// whether the mutation being added is already absent, to avoid recording a new derived state identical to the old one state.
		// The algorithm used here, with HaplosomeWalker, depends upon the fact that we just sorted the mutations to add by position.
		// However, we do still have to think about multiple muts being removed from the same position, and existing stacked mutations.
		// Note that when we are not creating substitutions, the haplosomes that possess a given mutation are the ones that change; but
		// when we are creating substitutions, the haplosomes that do *not* possess a given mutation are the ones that change, because
		// they gain the substitutuon, whereas the other haplosomes lose the mutation and gain the substitution, and are thus unchanged.
		std::vector<std::pair<Haplosome *, std::vector<slim_position_t>>> new_derived_state_positions;
		
		if (recording_tree_sequence_mutations)
		{
			for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
			{
				Haplosome *target_haplosome = targets_data[haplosome_index];
				HaplosomeWalker walker(target_haplosome);
				slim_position_t last_added_pos = -1;
				
				for (Mutation *mut : mutations_to_remove)
				{
					slim_position_t mut_pos = mut->position_;
					
					// We don't care about other mutations at an already recorded position; move on
					if (mut_pos == last_added_pos)
						continue;
					
					// Advance the walker until it is at or after the mutation's position
					while (!walker.Finished())
					{
						if (walker.Position() >= mut_pos)
							break;
						
						walker.NextMutation();
					}
					
					// If the derived state won't change for this position, then we don't need to record it; whether the
					// state will change depends upon whether the mutation is present, and whether we're substituting
					bool mutation_present = !walker.Finished() && (walker.Position() == mut_pos) && walker.MutationIsStackedAtCurrentPosition(mut);
					
					if ((create_substitutions && mutation_present) || (!create_substitutions && !mutation_present))
						continue;
					
					// we have decided that the new derived state at this position will need to be recorded, so note that
					if (last_added_pos == -1)
					{
						// no pair entry in new_derived_state_positions yet, so make a new pair entry for this haplosome
						new_derived_state_positions.emplace_back(target_haplosome, std::vector<slim_position_t>(1, mut_pos));
					}
					else
					{
						// we have an existing pair entry for this haplosome, so add this position to its position vector
						std::pair<Haplosome *, std::vector<slim_position_t>> &haplosome_entry = new_derived_state_positions.back();
						std::vector<slim_position_t> &haplosome_list = haplosome_entry.second;
						
						haplosome_list.emplace_back(mut_pos);
					}
					
					last_added_pos = mut_pos;
				}
			}
		}
		
		// Create substitutions for the mutations being removed, if requested.
		// Note we don't test for the mutations actually being fixed; that is the caller's responsibility to manage.
		if (create_substitutions)
		{
			for (int value_index = 0; value_index < mutations_count; ++value_index)
			{
				Mutation *mut = mutations_data[value_index];
				Substitution *sub = new Substitution(*mut, tick);
				
				// TREE SEQUENCE RECORDING
				// When doing tree recording, we additionally keep all fixed mutations (their ids) in a multimap indexed by their position
				// This allows us to find all the fixed mutations at a given position quickly and easily, for calculating derived states
				if (species->RecordingTreeSequence())
					pop.treeseq_substitutions_map_.emplace(mut->position_, sub);
				
				pop.substitutions_.emplace_back(sub);
			}
			
			// TREE SEQUENCE RECORDING
			// When doing tree recording, if a given mutation is converted to a substitution by script, it may not be contained in some
			// haplosomes, but at the moment it fixes it is then considered to belong to the derived state of every non-null haplosome.  We
			// therefore need to go through all of the non-target haplosomes and record their new derived states at all positions
			// containing a new substitution.  If they already had a copy of the mutation, and didn't have it removed here, that is
			// pretty weird, but in practice it just means that their derived state will contain that mutation id twice – once for the
			// segregating mutation they still contain, and once for the new substitution.  We don't check for that case, but it should
			// just work automatically.
			if (recording_tree_sequence_mutations)
			{
				int haplosome_count_per_individual = species->HaplosomeCountPerIndividual();
				
				// Mark all non-null haplosomes in the simulation that are not among the target haplosomes
				for (auto subpop_pair : species->population_.subpops_)
				{
					Subpopulation *subpop = subpop_pair.second;
					
					for (Individual *ind : subpop->parent_individuals_)
					{
						for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
						{
							Haplosome *haplosome = ind->haplosomes_[haplosome_index];
							
							haplosome->scratch_ = (haplosome->IsNull() ? 0 : 1);
						}
					}
				}
				
				for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
					targets_data[haplosome_index]->scratch_ = 0;
				
				// Figure out the unique chromosome positions that have changed (the uniqued set of mutation positions)
				std::vector<slim_position_t> unique_positions;
				slim_position_t last_pos = -1;
				
				for (Mutation *mut : mutations_to_remove)
				{
					slim_position_t pos = mut->position_;
					
					if (pos != last_pos)
					{
						unique_positions.emplace_back(pos);
						last_pos = pos;
					}
				}
				
				// Loop through those haplosomes and log the new derived state at each (unique) position
				for (auto subpop_pair : species->population_.subpops_)
				{
					Subpopulation *subpop = subpop_pair.second;
					
					for (Individual *ind : subpop->parent_individuals_)
					{
						for (int haplosome_index = 0; haplosome_index < haplosome_count_per_individual; haplosome_index++)
						{
							Haplosome *haplosome = ind->haplosomes_[haplosome_index];
							
							if (haplosome->scratch_ == 1)
							{
								for (slim_position_t position : unique_positions)
									species->RecordNewDerivedState(haplosome, position, *haplosome->derived_mutation_ids_at_position(mut_block_ptr, position));
								haplosome->scratch_ = 0;
							}
						}
					}
				}
			}
		}
		
		// Now handle the mutations to remove, broken into bulk operations according to the mutation run they fall into
		slim_mutrun_index_t last_handled_mutrun_index = -1;
		
		for (int value_index = 0; value_index < mutations_count; ++value_index)
		{
			Mutation *next_mutation = mutations_to_remove[value_index];
			const slim_position_t pos = next_mutation->position_;
			slim_mutrun_index_t mutrun_index = (slim_mutrun_index_t)(pos / mutrun_length);
			
			if (mutrun_index <= last_handled_mutrun_index)
				continue;
			
			// We have not yet processed this mutation run; do this mutation run index as a bulk operation
			int64_t operation_id = MutationRun::GetNextOperationID();
			
			Haplosome::BulkOperationStart(operation_id, mutrun_index);
			
			MutationRunContext &mutrun_context = chromosome->ChromosomeMutationRunContextForMutationRunIndex(mutrun_index);
			
			for (int haplosome_index = 0; haplosome_index < target_size; ++haplosome_index)
			{
				Haplosome *target_haplosome = targets_data[haplosome_index];
				
				if (target_haplosome->IsNull())
				{
					Haplosome::BulkOperationEnd(operation_id, mutrun_index);	// clean up for SLiMgui
					EIDOS_TERMINATION << "ERROR (Haplosome_Class::ExecuteMethod_removeMutations): removeMutations() cannot be called on a null haplosome.  This error may be due to a break in backward compatibility in SLiM 3.7 involving addRecombinant() with haploid models; if that seems likely, please see the release notes." << EidosTerminate();
				}
				
				// See if WillModifyRunForBulkOperation() can short-circuit the operation for us
				MutationRun *mutrun = target_haplosome->WillModifyRunForBulkOperation(operation_id, mutrun_index, mutrun_context);
				
				if (mutrun)
				{
					// Remove the specified mutations; see RemoveFixedMutations for the origins of this code
					MutationIndex *haplosome_iter = mutrun->begin_pointer();
					MutationIndex *haplosome_backfill_iter = mutrun->begin_pointer();
					MutationIndex *haplosome_max = mutrun->end_pointer();
					
					// haplosome_iter advances through the mutation list; for each entry it hits, the entry is either removed (skip it) or not removed (copy it backward to the backfill pointer)
					while (haplosome_iter != haplosome_max)
					{
						MutationIndex candidate_mutation = *haplosome_iter;
						const slim_position_t candidate_pos = (mut_block_ptr + candidate_mutation)->position_;
						bool should_remove = false;
						
						for (int mut_index = value_index; mut_index < mutations_count; ++mut_index)
						{
							Mutation *mut_to_remove = mutations_to_remove[mut_index];
							MutationIndex mut_to_remove_index = mutation_block->IndexInBlock(mut_to_remove);
							
							if (mut_to_remove_index == candidate_mutation)
							{
								should_remove = true;
								break;
							}
							
							// since we're in sorted order by position, as soon as we pass the candidate we're done
							if (mut_to_remove->position_ > candidate_pos)
								break;
						}
						
						if (should_remove)
						{
							// Removed mutation; we want to omit it, so we just advance our pointer
							++haplosome_iter;
						}
						else
						{
							// Unremoved mutation; we want to keep it, so we copy it backward and advance our backfill pointer as well as haplosome_iter
							if (haplosome_backfill_iter != haplosome_iter)
								*haplosome_backfill_iter = *haplosome_iter;
							
							++haplosome_backfill_iter;
							++haplosome_iter;
						}
					}
					
					// excess mutations at the end have been copied back already; we just adjust mutation_count_ and forget about them
					mutrun->set_size(mutrun->size() - (int)(haplosome_iter - haplosome_backfill_iter));
				}
			}
			
			Haplosome::BulkOperationEnd(operation_id, mutrun_index);
			
			// now we have handled all mutations at this index (and all previous indices)
			last_handled_mutrun_index = mutrun_index;
			
			// invalidate cached mutation refcounts; refcounts have changed
			pop.InvalidateMutationReferencesCache();
		}
		
		// TREE SEQUENCE RECORDING
		// Now that all the bulk operations are done, record all the new derived states
		if (recording_tree_sequence_mutations)
		{
			for (std::pair<Haplosome *, std::vector<slim_position_t>> &haplosome_pair : new_derived_state_positions)
			{
				Haplosome *target_haplosome = haplosome_pair.first;
				std::vector<slim_position_t> &haplosome_positions = haplosome_pair.second;
				
				for (slim_position_t position : haplosome_positions)
					species->RecordNewDerivedState(target_haplosome, position, *target_haplosome->derived_mutation_ids_at_position(mut_block_ptr, position));
			}
		}
	}
	
	// TIMING RESTRICTION
	// issue a warning if removeMutations() was called at a questionable time, but only if the mutations removed were non-neutral
	// BCH: added the !create_substitutions check; if a substitution is being created, then it can be assumed that the mutation is fixed
	// in the model and is thus deemed by the model to make no difference to fitness outcomes (mutations that matter to fitness outcomes
	// should not be removed when they fix anyway).  This is maybe not absolutely 100% clear, but models that handle their own fixation,
	// like haploid models and haplodiploid models, should not have to see/suppress this warning.
	if (any_nonneutral_removed && !create_substitutions && !community.warned_early_mutation_remove_)
	{
		if ((community.CycleStage() == SLiMCycleStage::kWFStage0ExecuteFirstScripts) ||
			(community.CycleStage() == SLiMCycleStage::kWFStage1ExecuteEarlyScripts))
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ErrorOutputStream() << "#WARNING (Haplosome_Class::ExecuteMethod_removeMutations): removeMutations() should probably not be called from a first() or early() event in a WF model; the removed mutation(s) will still influence fitness values during offspring generation." << std::endl;
				community.warned_early_mutation_remove_ = true;
			}
		}
		// Note that there is no equivalent problem in nonWF models, because fitness values are used for survival,
		// not reproduction, and there is no event stage in the tick cycle that splits fitness from survival.
	}
	
	return gStaticEidosValueVOID;
}


//
//	HaplosomeWalker
//
#pragma mark -
#pragma mark HaplosomeWalker
#pragma mark -

HaplosomeWalker::HaplosomeWalker(Haplosome *p_haplosome) : haplosome_(p_haplosome), mutrun_index_(-1), mutrun_ptr_(nullptr), mutrun_end_(nullptr), mutation_(nullptr), mut_block_ptr_(haplosome_->individual_->subpopulation_->species_.SpeciesMutationBlock()->mutation_buffer_)
{
	NextMutation();
};

void HaplosomeWalker::NextMutation(void)
{
	// the !mutrun_ptr_ is actually not necessary, but ASAN wants it to be here...
	if (!mutrun_ptr_ || (++mutrun_ptr_ >= mutrun_end_))
	{
		// finished the current mutation, so move to the next until we find a mutation
		do
		{
			if (++mutrun_index_ >= haplosome_->mutrun_count_)
			{
				// finished all mutation runs, so we're done
				mutation_ = nullptr;
				return;
			}
			
			const MutationRun *mutrun = haplosome_->mutruns_[mutrun_index_];
			mutrun_ptr_ = mutrun->begin_pointer_const();
			mutrun_end_ = mutrun->end_pointer_const();
		}
		while (mutrun_ptr_ == mutrun_end_);
	}
	
	mutation_ = mut_block_ptr_ + *mutrun_ptr_;
}

void HaplosomeWalker::MoveToPosition(slim_position_t p_position)
{
	// Move the the first mutation that is at or after the given position.  Using this to move to a
	// given position is more efficient than iteratively advancing mutation by mutation, because
	// we can (1) go to the correct mutation run directly, and (2) do a binary search inside the
	// mutation run for the correct position.
	Haplosome *haplosome = haplosome_;
	
	// start at the mutrun dictated by the position we are moving to; positions < 0 start at 0
	mutrun_index_ = (int32_t)(p_position / haplosome->mutrun_length_);
	if (mutrun_index_ < 0)
		mutrun_index_ = 0;
	
	while (true)
	{
		// if the mutrun is past the end of the last mutrun, we're done
		if (mutrun_index_ >= haplosome->mutrun_count_)
		{
			mutation_ = nullptr;
			return;
		}
		
		// get the information on the mutrun
		const MutationRun *mutrun = haplosome->mutruns_[mutrun_index_];
		mutrun_ptr_ = mutrun->begin_pointer_const();
		mutrun_end_ = mutrun->end_pointer_const();
		
		// if the mutrun is empty, we will need to move to the next mutrun to find a mutation
		if (mutrun_ptr_ == mutrun_end_)
			mutrun_index_++;
		else
			break;
	}
	
	// if the mutation found is at or after the requested position, we are already done
	mutation_ = mut_block_ptr_ + *mutrun_ptr_;
	
	if (mutation_->position_ >= p_position)
		return;
	
	// otherwise, we are in the correct mutrun for the position, but the requested position
	// still lies ahead of us, so we have to advance until we reach or pass the position
	// FIXME should do a binary search inside the mutation run instead
	do
		NextMutation();
	while (!Finished() && (Position() < p_position));
}

bool HaplosomeWalker::MutationIsStackedAtCurrentPosition(Mutation *p_search_mut)
{
	// We have reached some chromosome position (presumptively the *first mutation* at that position,
	// which we do not check here), and the caller wants to know if a given mutation, which is
	// located at that position, is contained in this haplosome.  This requires that we look ahead
	// through all of the mutations at the current position, since they are not sorted.  We are
	// guaranteed to stay inside the current mutation run, though, so this lookahead is simple.
	if (Finished())
		EIDOS_TERMINATION << "ERROR (HaplosomeWalker::MutationIsStackedAtCurrentPosition): (internal error) MutationIsStackedAtCurrentPosition() called on a finished walker." << EidosTerminate();
	if (!p_search_mut)
		EIDOS_TERMINATION << "ERROR (HaplosomeWalker::MutationIsStackedAtCurrentPosition): (internal error) MutationIsStackedAtCurrentPosition() called with a nullptr mutation to search for." << EidosTerminate();
	
	slim_position_t pos = mutation_->position_;
	
	if (p_search_mut->position_ != pos)
		EIDOS_TERMINATION << "ERROR (HaplosomeWalker::MutationIsStackedAtCurrentPosition): (internal error) MutationIsStackedAtCurrentPosition() called with a mutation that is not at the current walker position." << EidosTerminate();
	
	for (const MutationIndex *search_ptr_ = mutrun_ptr_; search_ptr_ != mutrun_end_; ++search_ptr_)
	{
		MutationIndex mutindex = *search_ptr_;
		Mutation *mut = mut_block_ptr_ + mutindex;
		
		if (mut == p_search_mut)
			return true;
		if (mut->position_ != pos)
			break;
	}
	
	return false;
}

bool HaplosomeWalker::IdenticalAtCurrentPositionTo(HaplosomeWalker &p_other_walker)
{
	if (Finished())
		EIDOS_TERMINATION << "ERROR (HaplosomeWalker::IdenticalAtCurrentPositionTo): (internal error) IdenticalAtCurrentPositionTo() called on a finished walker." << EidosTerminate();
	if (p_other_walker.Finished())
		EIDOS_TERMINATION << "ERROR (HaplosomeWalker::IdenticalAtCurrentPositionTo): (internal error) IdenticalAtCurrentPositionTo() called on a finished walker." << EidosTerminate();
	if (Position() != p_other_walker.Position())
		EIDOS_TERMINATION << "ERROR (HaplosomeWalker::IdenticalAtCurrentPositionTo): (internal error) IdenticalAtCurrentPositionTo() called with walkers at different positions." << EidosTerminate();
	
	// If the two walkers are using the same mutation run, they are identical by definition
	if (mutrun_ptr_ == p_other_walker.mutrun_ptr_)
		return true;
	
	// If their current mutation differs, then the positions are not identical
	if (mutation_ != p_other_walker.mutation_)
		return false;
	
	// Scan forward as long as we are still within the same position
	slim_position_t pos = mutation_->position_;
	const MutationIndex *search_ptr_1 = mutrun_ptr_ + 1;
	const MutationIndex *search_ptr_2 = p_other_walker.mutrun_ptr_ + 1;
	
	do
	{
		Mutation *mut_1 = (search_ptr_1 != mutrun_end_) ? (mut_block_ptr_ + *search_ptr_1) : nullptr;
		Mutation *mut_2 = (search_ptr_2 != p_other_walker.mutrun_end_) ? (mut_block_ptr_ + *search_ptr_2) : nullptr;
		bool has_mut_at_position_1 = (mut_1) ? (mut_1->position_ == pos) : false;
		bool has_mut_at_position_2 = (mut_2) ? (mut_2->position_ == pos) : false;
		
		// If we have left the current position for both, simultaneously, then the positions are identical
		if (!has_mut_at_position_1 && !has_mut_at_position_2)
			return true;
		
		// If we left the current position for one but not for the other, then the positions differ
		if (!has_mut_at_position_1 || !has_mut_at_position_2)
			return false;
		
		// We are still within the current position in both, so if the mutations we reached differ then the positions differ
		if (mut_1 != mut_2)
			return false;
		
		// Otherwise we saw identical mutations at the position, and we should continue scanning
		++search_ptr_1;
		++search_ptr_2;
	}
	while (true);
}

int8_t HaplosomeWalker::NucleotideAtCurrentPosition(void)
{
	if (Finished())
		EIDOS_TERMINATION << "ERROR (HaplosomeWalker::NucleotideAtCurrentPosition): (internal error) NucleotideAtCurrentPosition() called on a finished walker." << EidosTerminate();
	
	// First check the mutation we're at, since we already have it
	int8_t nuc = mutation_->nucleotide_;
	
	if (nuc != -1)
		return nuc;
	
	// Then scan forward as long as we are still within the same position
	slim_position_t pos = mutation_->position_;
	
	for (const MutationIndex *search_ptr_ = mutrun_ptr_ + 1; search_ptr_ != mutrun_end_; ++search_ptr_)
	{
		MutationIndex mutindex = *search_ptr_;
		Mutation *mut = mut_block_ptr_ + mutindex;
		
		if (mut->position_ != pos)
			return -1;
		
		nuc = mut->nucleotide_;
		
		if (nuc != -1)
			return nuc;
	}
	
	return -1;
}


































































