//
//  genome.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2019 Philipp Messer.  All rights reserved.
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


#include "genome.h"
#include "slim_global.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "slim_sim.h"
#include "polymorphism.h"
#include "subpopulation.h"

#include <algorithm>
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <utility>


#pragma mark -
#pragma mark Genome
#pragma mark -

// Static class variables in support of Genome's bulk operation optimization; see Genome::WillModifyRunForBulkOperation()
int64_t Genome::s_bulk_operation_id_ = 0;
slim_mutrun_index_t Genome::s_bulk_operation_mutrun_index_ = -1;
std::unordered_map<MutationRun*, MutationRun*> Genome::s_bulk_operation_runs_;


Genome::Genome(Subpopulation *p_subpop, int p_mutrun_count, slim_position_t p_mutrun_length, enum GenomeType p_genome_type_, bool p_is_null) : genome_type_(p_genome_type_), subpop_(p_subpop), individual_(nullptr), genome_id_(-1)
{
	// null genomes are now signalled with a mutrun_count_ of 0, rather than a separate flag
	if (p_is_null)
	{
		mutrun_count_ = 0;
		mutrun_length_ = 0;
		mutruns_ = nullptr;
	}
	else
	{
		mutrun_count_ = p_mutrun_count;
		mutrun_length_ = p_mutrun_length;
		
		if (mutrun_count_ <= SLIM_GENOME_MUTRUN_BUFSIZE)
			mutruns_ = run_buffer_;
		else
			mutruns_ = new MutationRun_SP[mutrun_count_];
	}
}

Genome::~Genome(void)
{
	for (int run_index = 0; run_index < mutrun_count_; ++run_index)
		mutruns_[run_index].reset();
	
	if (mutruns_ != run_buffer_)
		delete[] mutruns_;
	mutruns_ = nullptr;
	
	mutrun_count_ = 0;
}

// prints an error message, a stacktrace, and exits; called only for DEBUG
void Genome::NullGenomeAccessError(void) const
{
	EIDOS_TERMINATION << "ERROR (Genome::NullGenomeAccessError): (internal error) a null genome was accessed." << EidosTerminate();
}

void Genome::WillModifyRun(slim_mutrun_index_t p_run_index)
{
#ifdef DEBUG
	if (p_run_index >= mutrun_count_)
		EIDOS_TERMINATION << "ERROR (Genome::WillModifyRun): (internal error) attempt to modify an out-of-index run." << EidosTerminate();
#endif
	
	MutationRun *original_run = mutruns_[p_run_index].get();
	
	if (original_run->UseCount() > 1)
	{
		MutationRun *new_run = MutationRun::NewMutationRun();	// take from shared pool of used objects
		
		new_run->copy_from_run(*original_run);
		mutruns_[p_run_index] = MutationRun_SP(new_run);
	}
	else
	{
		original_run->will_modify_run();	// in-place modification of runs requires notification, for cache invalidation
	}
}

void Genome::BulkOperationStart(int64_t p_operation_id, slim_mutrun_index_t p_mutrun_index)
{
	if (s_bulk_operation_id_ != 0)
	{
		//EIDOS_TERMINATION << "ERROR (Genome::BulkOperationStart): (internal error) unmatched bulk operation start." << EidosTerminate();
		
		// It would be nice to be able to throw an exception here, but in the present design, the
		// bulk operation info can get messed up if the bulk operation throws an exception that
		// blows through the call to Genome::BulkOperationEnd().
		// Note this warning is not suppressed by gEidosSuppressWarnings; that is deliberate
		std::cout << "WARNING (Genome::BulkOperationStart): (internal error) unmatched bulk operation start." << std::endl;
		
		// For now, we assume that the end call got blown through, and we close out the old operation.
		Genome::BulkOperationEnd(s_bulk_operation_id_, s_bulk_operation_mutrun_index_);
	}
	
	s_bulk_operation_id_ = p_operation_id;
	s_bulk_operation_mutrun_index_ = p_mutrun_index;
}

bool Genome::WillModifyRunForBulkOperation(int64_t p_operation_id, slim_mutrun_index_t p_mutrun_index)
{
	if (p_mutrun_index != s_bulk_operation_mutrun_index_)
		EIDOS_TERMINATION << "ERROR (Genome::WillModifyRunForBulkOperation): (internal error) incorrect run index during bulk operation." << EidosTerminate();
	if (p_mutrun_index >= mutrun_count_)
		EIDOS_TERMINATION << "ERROR (Genome::WillModifyRunForBulkOperation): (internal error) attempt to modify an out-of-index run." << EidosTerminate();
	
#if 0
#warning Genome::WillModifyRunForBulkOperation disabled...
	// The trivial version of this function just calls WillModifyRun() and returns T,
	// requesting that the caller perform the operation
	WillModifyRun(p_run_index);
	return true;
#else
	// The interesting version remembers the operation in progress, using the ID, and
	// tracks original/final MutationRun pointers, returning F if an original is matched.
	MutationRun *original_run = mutruns_[p_mutrun_index].get();
	
	if (p_operation_id != s_bulk_operation_id_)
			EIDOS_TERMINATION << "ERROR (Genome::WillModifyRunForBulkOperation): (internal error) missing bulk operation start." << EidosTerminate();
	
	auto found_run_pair = s_bulk_operation_runs_.find(original_run);
	
	if (found_run_pair == s_bulk_operation_runs_.end())
	{
		// This MutationRun is not in the map, so we need to set up a new entry
		MutationRun *product_run = MutationRun::NewMutationRun();
		
		product_run->copy_from_run(*original_run);
		mutruns_[p_mutrun_index] = MutationRun_SP(product_run);
		
		s_bulk_operation_runs_.insert(std::pair<MutationRun*, MutationRun*>(original_run, product_run));
		
		//std::cout << "WillModifyRunForBulkOperation() created product for " << original_run << std::endl;
		
		return true;
	}
	else
	{
		// This MutationRun is in the map, so we can just return it
		mutruns_[p_mutrun_index] = MutationRun_SP(found_run_pair->second);
		
		//std::cout << "   WillModifyRunForBulkOperation() substituted known product for " << original_run << std::endl;
		
		return false;
	}
#endif
}

void Genome::BulkOperationEnd(int64_t p_operation_id, slim_mutrun_index_t p_mutrun_index)
{
	if ((p_operation_id == s_bulk_operation_id_) && (p_mutrun_index == s_bulk_operation_mutrun_index_))
	{
		s_bulk_operation_runs_.clear();
		s_bulk_operation_id_ = 0;
		s_bulk_operation_mutrun_index_ = -1;
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Genome::BulkOperationEnd): (internal error) unmatched bulk operation end." << EidosTerminate();
	}
}

// Remove all mutations in p_genome that have a refcount of p_fixed_count, indicating that they have fixed
// This must be called with mutation counts set up correctly as all-population counts, or it will malfunction!
void Genome::RemoveFixedMutations(int64_t p_operation_id, slim_mutrun_index_t p_mutrun_index)
{
#ifdef DEBUG
	if (mutrun_count_ == 0)
		NullGenomeAccessError();
#endif
	// This used to call RemoveFixedMutations() on each mutation run; now it removes only within a given
	// mutation run index, allowing all the parts of the genome that don't contain fixed mutations to be skipped
	mutruns_[p_mutrun_index]->RemoveFixedMutations(p_operation_id);
}

void Genome::TallyGenomeReferences(slim_refcount_t *p_mutrun_ref_tally, slim_refcount_t *p_mutrun_tally, int64_t p_operation_id)
{
#ifdef DEBUG
	if (mutrun_count_ == 0)
		NullGenomeAccessError();
#endif
	for (int run_index = 0; run_index < mutrun_count_; ++run_index)
	{
		if (mutruns_[run_index]->operation_id_ != p_operation_id)
		{
			(*p_mutrun_ref_tally) += mutruns_[run_index]->UseCount();
			(*p_mutrun_tally)++;
			mutruns_[run_index]->operation_id_ = p_operation_id;
		}
	}
}

void Genome::TallyGenomeMutationReferences(int64_t p_operation_id)
{
#ifdef DEBUG
	if (mutrun_count_ == 0)
		NullGenomeAccessError();
#endif
	slim_refcount_t *refcount_block_ptr = gSLiM_Mutation_Refcounts;
	
	for (int run_index = 0; run_index < mutrun_count_; ++run_index)
	{
		MutationRun *mutrun = mutruns_[run_index].get();
		
		if (mutrun->operation_id_ != p_operation_id)
		{
			slim_refcount_t use_count = (slim_refcount_t)mutrun->UseCount();
			
			const MutationIndex *genome_iter = mutrun->begin_pointer_const();
			const MutationIndex *genome_end_iter = mutrun->end_pointer_const();
			
			// Do 16 reps
			while (genome_iter + 16 <= genome_end_iter)
			{
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
			}
			
			// Do 4 reps
			while (genome_iter + 4 <= genome_end_iter)
			{
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
			}
			
			// Finish off
			while (genome_iter != genome_end_iter)
				*(refcount_block_ptr + (*genome_iter++)) += use_count;
			
			mutrun->operation_id_ = p_operation_id;
		}
	}
}

void Genome::MakeNull(void)
{
	if (mutrun_count_)
	{
		for (int run_index = 0; run_index < mutrun_count_; ++run_index)
			mutruns_[run_index].reset();
		
		if (mutruns_ != run_buffer_)
			delete[] mutruns_;
		mutruns_ = nullptr;
		
		mutrun_count_ = 0;
		mutrun_length_ = 0;
	}
}

void Genome::ReinitializeGenomeToMutrun(GenomeType p_genome_type, int32_t p_mutrun_count, slim_position_t p_mutrun_length, MutationRun *p_run)
{
	genome_type_ = p_genome_type;
	
	if (p_mutrun_count)
	{
		if (mutrun_count_ == 0)
		{
			// was a null genome, needs to become not null
			mutrun_count_ = p_mutrun_count;
			mutrun_length_ = p_mutrun_length;
			
			if (mutrun_count_ <= SLIM_GENOME_MUTRUN_BUFSIZE)
				mutruns_ = run_buffer_;
			else
				mutruns_ = new MutationRun_SP[mutrun_count_];
		}
		else if (mutrun_count_ != p_mutrun_count)
		{
			// the number of mutruns has changed; need to reallocate
			for (int run_index = 0; run_index < mutrun_count_; ++run_index)
				mutruns_[run_index].reset();
			
			if (mutruns_ != run_buffer_)
				delete[] mutruns_;
			
			mutrun_count_ = p_mutrun_count;
			mutrun_length_ = p_mutrun_length;
			
			if (mutrun_count_ <= SLIM_GENOME_MUTRUN_BUFSIZE)
				mutruns_ = run_buffer_;
			else
				mutruns_ = new MutationRun_SP[mutrun_count_];
		}
		
		for (int run_index = 0; run_index < mutrun_count_; ++run_index)
			mutruns_[run_index].reset(p_run);
	}
	else // if (!p_mutrun_count)
	{
		if (mutrun_count_)
		{
			// was a non-null genome, needs to become null
			for (int run_index = 0; run_index < mutrun_count_; ++run_index)
				mutruns_[run_index].reset();
			
			if (mutruns_ != run_buffer_)
				delete[] mutruns_;
			mutruns_ = nullptr;
			
			mutrun_count_ = 0;
			mutrun_length_ = 0;
		}
	}
}

void Genome::ReinitializeGenomeNullptr(GenomeType p_genome_type, int32_t p_mutrun_count, slim_position_t p_mutrun_length)
{
	genome_type_ = p_genome_type;
	
#if DEBUG
	// we are guaranteed by the caller that all existing mutrun pointers are null; check that, in DEBUG mode
	for (int run_index = 0; run_index < mutrun_count_; ++run_index)
		if (mutruns_[run_index])
			EIDOS_TERMINATION << "ERROR (Genome::ReinitializeGenomeNullptr): (internal error) nonnull mutrun pointer in ReinitializeGenomeNullptr." << EidosTerminate();
#endif
	
	if (p_mutrun_count)
	{
		if (mutrun_count_ == 0)
		{
			// was a null genome, needs to become not null
			mutrun_count_ = p_mutrun_count;
			mutrun_length_ = p_mutrun_length;
			
			if (mutrun_count_ <= SLIM_GENOME_MUTRUN_BUFSIZE)
				mutruns_ = run_buffer_;
			else
				mutruns_ = new MutationRun_SP[mutrun_count_];
		}
		else if (mutrun_count_ != p_mutrun_count)
		{
			// the number of mutruns has changed; need to reallocate
			if (mutruns_ != run_buffer_)
				delete[] mutruns_;
			
			mutrun_count_ = p_mutrun_count;
			mutrun_length_ = p_mutrun_length;
			
			if (mutrun_count_ <= SLIM_GENOME_MUTRUN_BUFSIZE)
				mutruns_ = run_buffer_;
			else
				mutruns_ = new MutationRun_SP[mutrun_count_];
		}
		
		// we leave the new mutruns_ buffer filled with nullptr
	}
	else // if (!p_mutrun_count)
	{
		if (mutrun_count_)
		{
			// was a non-null genome, needs to become null
			if (mutruns_ != run_buffer_)
				delete[] mutruns_;
			mutruns_ = nullptr;
			
			mutrun_count_ = 0;
			mutrun_length_ = 0;
		}
	}
}

void Genome::record_derived_states(SLiMSim *p_sim) const
{
	// This is called by SLiMSim::RecordAllDerivedStatesFromSLiM() to record all the derived states present
	// in a given genome that was just created by readFromPopulationFile() or some similar situation.  It should
	// make calls to record the derived state at each position in the genome that has any mutation.
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	static std::vector<Mutation *> record_vec;
	
	for (int run_index = 0; run_index < mutrun_count_; ++run_index)
	{
		MutationRun *mutrun = mutruns_[run_index].get();
		int mutrun_size = mutrun->size();
		slim_position_t last_pos = -1;
		
		//record_vec.clear();	// should always be left clear by the code below
		
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
					p_sim->RecordNewDerivedState(this, last_pos, record_vec);
					record_vec.clear();
				}
				
				// ...and start a new derived-state block
				last_pos = mutation_pos;
			}
			
			record_vec.push_back(mutation);
		}
		
		// record the last derived block, if any
		if (last_pos != -1)
		{
			p_sim->RecordNewDerivedState(this, last_pos, record_vec);
			record_vec.clear();
		}
	}
}

/*
void Genome::assert_identical_to_runs(MutationRun_SP *p_mutruns, int32_t p_mutrun_count)
{
	// This checks that the mutations carried by a genome are identical after a mutation run transformation.
	// It is run only in DEBUG mode, and does not need to be fast.
	std::vector <MutationIndex> genome_muts;
	std::vector <MutationIndex> param_muts;
	
	for (int run_index = 0; run_index < mutrun_count_; ++run_index)
	{
		MutationRun *mutrun = mutruns_[run_index].get();
		int mutrun_size = mutrun->size();
		
		for (int mut_index = 0; mut_index < mutrun_size; ++mut_index)
			genome_muts.push_back((*mutrun)[mut_index]);
	}
	
	for (int run_index = 0; run_index < p_mutrun_count; ++run_index)
	{
		MutationRun *mutrun = p_mutruns[run_index].get();
		int mutrun_size = mutrun->size();
		
		for (int mut_index = 0; mut_index < mutrun_size; ++mut_index)
			param_muts.push_back((*mutrun)[mut_index]);
	}
	
	if (genome_muts.size() != param_muts.size())
		EIDOS_TERMINATION << "ERROR (Genome::assert_identical_to_runs): (internal error) genome unequal in size after transformation." << EidosTerminate();
	if (genome_muts != param_muts)
		EIDOS_TERMINATION << "ERROR (Genome::assert_identical_to_runs): (internal error) genome unequal in contents after transformation." << EidosTerminate();
	
	// Check that mutations are also placed into the correct mutation run based on their position
	for (int run_index = 0; run_index < mutrun_count_; ++run_index)
	{
		MutationRun *mutrun = mutruns_[run_index].get();
		int mutrun_size = mutrun->size();
		
		for (int mut_index = 0; mut_index < mutrun_size; ++mut_index)
		{
			MutationIndex mutation_index = (*mutrun)[mut_index];
			Mutation *mutation = gSLiM_Mutation_Block + mutation_index;
			slim_position_t position = mutation->position_;
			
			if (position / mutrun_length_ != run_index)
				EIDOS_TERMINATION << "ERROR (Genome::assert_identical_to_runs): (internal error) genome has mutation at bad position." << EidosTerminate();
		}
	}
}
*/


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

void Genome::GenerateCachedEidosValue(void)
{
	// Note that this cache cannot be invalidated as long as a symbol table might exist that this value has been placed into
	// The false parameter selects a constructor variant that prevents this self-cache from having its address patched;
	// our self-pointer never changes.  See EidosValue_Object::EidosValue_Object() for comments on this disgusting hack.
	self_value_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Genome_Class, false));
}

const EidosObjectClass *Genome::Class(void) const
{
	return gSLiM_Genome_Class;
}

void Genome::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType() << "<";
	
	switch (genome_type_)
	{
		case GenomeType::kAutosome:		p_ostream << gStr_A; break;
		case GenomeType::kXChromosome:	p_ostream << gStr_X; break;
		case GenomeType::kYChromosome:	p_ostream << gStr_Y; break;
	}
	
	if (mutrun_count_ == 0)
		p_ostream << ":null>";
	else
		p_ostream << ":" << mutation_count() << ">";
}

EidosValue_SP Genome::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_genomePedigreeID:		// ACCELERATED
		{
			if (!subpop_->population_.sim_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Genome::GetProperty): property genomePedigreeID is not available because pedigree recording has not been enabled." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(genome_id_));
		}
		case gID_genomeType:
		{
			switch (genome_type_)
			{
				case GenomeType::kAutosome:		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_A));
				case GenomeType::kXChromosome:	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_X));
				case GenomeType::kYChromosome:	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(gStr_Y));
			}
		}
		case gID_isNullGenome:		// ACCELERATED
			return ((mutrun_count_ == 0) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_mutations:
		{
			Mutation *mut_block_ptr = gSLiM_Mutation_Block;
			int mut_count = mutation_count();
			EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class))->resize_no_initialize(mut_count);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			int set_index = 0;
			
			for (int run_index = 0; run_index < mutrun_count_; ++run_index)
			{
				MutationRun *mutrun = mutruns_[run_index].get();
				const MutationIndex *mut_start_ptr = mutrun->begin_pointer_const();
				const MutationIndex *mut_end_ptr = mutrun->end_pointer_const();
				
				for (const MutationIndex *mut_ptr = mut_start_ptr; mut_ptr < mut_end_ptr; ++mut_ptr)
					vec->set_object_element_no_check(mut_block_ptr + *mut_ptr, set_index++);
			}
			
			return result_SP;
		}
			
			// variables
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Genome::GetProperty): property tag accessed on genome before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

EidosValue *Genome::GetProperty_Accelerated_genomePedigreeID(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	size_t value_index = 0;
	
	// check that pedigrees are enabled, once
	if (value_index < p_values_size)
	{
		Genome *value = (Genome *)(p_values[value_index]);
		
		if (!value->subpop_->population_.sim_.PedigreesEnabledByUser())
			EIDOS_TERMINATION << "ERROR (Genome::GetProperty): property genomePedigreeID is not available because pedigree recording has not been enabled." << EidosTerminate();
		
		int_result->set_int_no_check(value->genome_id_, value_index);
		++value_index;
	}
	
	for ( ; value_index < p_values_size; ++value_index)
	{
		Genome *value = (Genome *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->genome_id_, value_index);
	}
	
	return int_result;
}

EidosValue *Genome::GetProperty_Accelerated_isNullGenome(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Genome *value = (Genome *)(p_values[value_index]);
		
		logical_result->set_logical_no_check(value->mutrun_count_ == 0, value_index);
	}
	
	return logical_result;
}

EidosValue *Genome::GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Genome *value = (Genome *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (Genome::GetProperty): property tag accessed on genome before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

void Genome::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	switch (p_property_id)
	{
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			Individual::s_any_individual_or_genome_tag_set_ = true;
			return;
		}
			
		default:
		{
			return EidosObjectElement::SetProperty(p_property_id, p_value);
		}
	}
}

void Genome::SetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	Individual::s_any_individual_or_genome_tag_set_ = true;
	
	// SLiMCastToUsertagTypeOrRaise() is a no-op at present
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Genome *)(p_values[value_index]))->tag_value_ = source_value;
	}
	else
	{
		const int64_t *source_data = p_source.IntVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Genome *)(p_values[value_index]))->tag_value_ = source_data[value_index];
	}
}

EidosValue_SP Genome::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_containsMarkerMutation:		return ExecuteMethod_containsMarkerMutation(p_method_id, p_arguments, p_argument_count, p_interpreter);
		//case gID_containsMutations:			return ExecuteMethod_Accelerated_containsMutations(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_countOfMutationsOfType:		return ExecuteMethod_countOfMutationsOfType(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_mutationsOfType:				return ExecuteMethod_mutationsOfType(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_nucleotides:					return ExecuteMethod_nucleotides(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_positionsOfMutationsOfType:	return ExecuteMethod_positionsOfMutationsOfType(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_sumOfMutationsOfType:			return ExecuteMethod_sumOfMutationsOfType(p_method_id, p_arguments, p_argument_count, p_interpreter);
		default:								return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}

//	*********************	- (logical$)containsMarkerMutation(io<MutationType>$ mutType, integer$ position, [returnMutation$ = F])
//
EidosValue_SP Genome::ExecuteMethod_containsMarkerMutation(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	EidosValue *position_value = p_arguments[1].get();
	EidosValue *returnMutation_value = p_arguments[2].get();
	
	if (IsNull())
		EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_containsMarkerMutation): containsMarkerMutation() cannot be called on a null genome." << EidosTerminate();
	
	SLiMSim &sim = subpop_->population_.sim_;
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, sim, "containsMarkerMutation()");
	slim_position_t marker_position = SLiMCastToPositionTypeOrRaise(position_value->IntAtIndex(0, nullptr));
	slim_position_t last_position = sim.TheChromosome().last_position_;
	
	if (marker_position > last_position)
		EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_containsMarkerMutation): containsMarkerMutation() position " << marker_position << " is past the end of the chromosome." << EidosTerminate();
	
	Mutation *mut = mutation_with_type_and_position(mutation_type_ptr, marker_position, last_position);
	
	eidos_logical_t returnMutation = returnMutation_value->LogicalAtIndex(0, nullptr);
	
	if (returnMutation == false)
		return (mut ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
	else
		return (mut ? EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(mut, gSLiM_Mutation_Class)) : (EidosValue_SP)gStaticEidosValueNULL);
}

//	*********************	- (logical)containsMutations(object<Mutation> mutations)
//
EidosValue_SP Genome::ExecuteMethod_Accelerated_containsMutations(EidosObjectElement **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	if (p_elements_size)
	{
		EidosValue *mutations_value = p_arguments[0].get();
		int mutations_count = mutations_value->Count();
		
		if (mutations_count == 1)
		{
			Mutation *mut = (Mutation *)(mutations_value->ObjectElementAtIndex(0, nullptr));
			MutationIndex mut_block_index = mut->BlockIndex();
			slim_position_t mutrun_length = ((Genome *)(p_elements[0]))->mutrun_length_;		// assume all Genome objects have the same mutrun_length_; better be true...
			slim_position_t mutrun_index = mut->position_ / mutrun_length;
			
			if (p_elements_size == 1)
			{
				// We want to be smart enough to return gStaticEidosValue_LogicalT or gStaticEidosValue_LogicalF in the singleton/singleton case
				Genome *element = (Genome *)(p_elements[0]);
				
				if (element->IsNull())
					EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_Accelerated_containsMutations): containsMutations() cannot be called on a null genome." << EidosTerminate();
				
				bool contained = element->mutruns_[mutrun_index]->contains_mutation(mut_block_index);
				
				return (contained ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
			}
			else
			{
				EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_elements_size);
				EidosValue_SP result(logical_result);
				
				for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
				{
					Genome *element = (Genome *)(p_elements[element_index]);
					
					if (element->IsNull())
						EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_Accelerated_containsMutations): containsMutations() cannot be called on a null genome." << EidosTerminate();
					
					bool contained = element->mutruns_[mutrun_index]->contains_mutation(mut_block_index);
					
					logical_result->set_logical_no_check(contained, element_index);
				}
				
				return result;
			}
		}
		else
		{
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_elements_size * mutations_count);
			EidosValue_SP result(logical_result);
			int64_t result_index = 0;
			
			EidosObjectElement * const *mutations_data = mutations_value->ObjectElementVector()->data();
			
			for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
			{
				Genome *element = (Genome *)(p_elements[element_index]);
				
				if (element->IsNull())
					EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_Accelerated_containsMutations): containsMutations() cannot be called on a null genome." << EidosTerminate();
				
				for (int value_index = 0; value_index < mutations_count; ++value_index)
				{
					Mutation *mut = (Mutation *)mutations_data[value_index];
					MutationIndex mut_block_index = mut->BlockIndex();
					bool contained = element->contains_mutation(mut_block_index);
					
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

//	*********************	- (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Genome::ExecuteMethod_countOfMutationsOfType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	
	if (IsNull())
		EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_countOfMutationsOfType): countOfMutationsOfType() cannot be called on a null genome." << EidosTerminate();
	
	SLiMSim &sim = subpop_->population_.sim_;
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, sim, "countOfMutationsOfType()");
	
	// Count the number of mutations of the given type
	int match_count = 0;
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	for (int run_index = 0; run_index < mutrun_count_; ++run_index)
	{
		MutationRun *mutrun = mutruns_[run_index].get();
		int mut_count = mutrun->size();
		const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
		
		for (int mut_index = 0; mut_index < mut_count; ++mut_index)
			if ((mut_block_ptr + mut_ptr[mut_index])->mutation_type_ptr_ == mutation_type_ptr)
				++match_count;
	}
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(match_count));
}

//	*********************	- (object<Mutation>)mutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Genome::ExecuteMethod_mutationsOfType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	
	if (IsNull())
		EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_mutationsOfType): mutationsOfType() cannot be called on a null genome." << EidosTerminate();
	
	SLiMSim &sim = subpop_->population_.sim_;
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, sim, "mutationsOfType()");
	
	// We want to return a singleton if we can, but we also want to avoid scanning through all our mutations twice.
	// We do this by not creating a vector until we see the second match; with one match, we make a singleton.
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	Mutation *first_match = nullptr;
	EidosValue_Object_vector *vec = nullptr;
	EidosValue_SP result_SP;
	int run_index;
	
	for (run_index = 0; run_index < mutrun_count_; ++run_index)
	{
		MutationRun *mutrun = mutruns_[run_index].get();
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
						vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
						result_SP = EidosValue_SP(vec);
						
						vec->push_object_element(first_match);
						vec->push_object_element(mut);
						first_match = nullptr;
					}
				}
				else
				{
					vec->push_object_element(mut);
				}
			}
		}
	}
	
	// Now return the appropriate return value
	if (first_match)
	{
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(first_match, gSLiM_Mutation_Class));
	}
	else
	{
		if (!vec)
		{
			vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
			result_SP = EidosValue_SP(vec);
		}
		
		return result_SP;
		
	}
}

//	*********************	– (is)nucleotides([Ni$ start = NULL], [Ni$ end = NULL], [s$ format = "string"])
//
EidosValue_SP Genome::ExecuteMethod_nucleotides(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	SLiMSim *sim = (SLiMSim *)p_interpreter.Context();
	Chromosome &chromosome = sim->TheChromosome();
	slim_position_t last_position = chromosome.last_position_;
	
	if (!sim->IsNucleotideBased())
		EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_nucleotides): nucleotides() may only be called in nucleotide-based models." << EidosTerminate();
	
	NucleotideArray *sequence = sim->TheChromosome().AncestralSequence();
	EidosValue *start_value = p_arguments[0].get();
	EidosValue *end_value = p_arguments[1].get();
	
	int64_t start = (start_value->Type() == EidosValueType::kValueNULL) ? 0 : start_value->IntAtIndex(0, nullptr);
	int64_t end = (end_value->Type() == EidosValueType::kValueNULL) ? last_position : end_value->IntAtIndex(0, nullptr);
	
	if ((start < 0) || (end < 0) || (start > last_position) || (end > last_position) || (start > end))
		EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_nucleotides): start and end must be within the chromosome's extent, and start must be <= end." << EidosTerminate();
	if (((std::size_t)start >= sequence->size()) || ((std::size_t)end >= sequence->size()))
		EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_nucleotides): (internal error) start and end must be within the ancestral sequence's length." << EidosTerminate();
	
	int64_t length = end - start + 1;
	
	if (length > INT_MAX)
		EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_nucleotides): the returned vector would exceed the maximum vector length in Eidos." << EidosTerminate();
	
	static const char nuc_chars[4] = {'A', 'C', 'G', 'T'};
	EidosValue *format_value = p_arguments[2].get();
	std::string format = format_value->StringAtIndex(0, nullptr);
	
	if (format == "codon")
	{
		EidosValue_SP codon_value = sequence->NucleotidesAsCodonVector(start, end, /* p_force_vector */ true);
		
		// patch the sequence with nucleotide mutations
		// no singleton case; we force a vector return from NucleotidesAsCodonVector() for simplicity
		int64_t *int_vec = ((EidosValue_Int_vector *)(codon_value.get()))->data();
		GenomeWalker walker(this);
		
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
			GenomeWalker walker(this);
			
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
			std::string &string_string = ((EidosValue_String_singleton *)(string_value.get()))->StringValue_Mutable();
			char *string_ptr = &string_string[0];	// data() returns a const pointer, but this is safe in C++11 and later
			GenomeWalker walker(this);
			
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
					string_ptr[pos - start] = nuc_chars[nuc];
				
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
			GenomeWalker walker(this);
			
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
			int64_t *int_vec = ((EidosValue_Int_vector *)(integer_value.get()))->data();
			GenomeWalker walker(this);
			
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
					int_vec[pos-start] = nuc;
				
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
			GenomeWalker walker(this);
			
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
			std::vector<std::string> *char_vec = ((EidosValue_String_vector *)(char_value.get()))->StringVector_Mutable();
			GenomeWalker walker(this);
			
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
					(*char_vec)[pos - start] = nuc_chars[nuc];
				
				walker.NextMutation();
			}
		}
		
		return char_value;
	}
	
	EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_nucleotides): parameter format must be either 'string', 'char', 'integer', or 'codon'." << EidosTerminate();
}

//	*********************	- (integer)positionsOfMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Genome::ExecuteMethod_positionsOfMutationsOfType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	
	if (IsNull())
		EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_positionsOfMutationsOfType): positionsOfMutationsOfType() cannot be called on a null genome." << EidosTerminate();
	
	SLiMSim &sim = subpop_->population_.sim_;
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, sim, "positionsOfMutationsOfType()");
	
	// Return the positions of mutations of the given type
	EidosValue_Int_vector *int_result = new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector();
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	for (int run_index = 0; run_index < mutrun_count_; ++run_index)
	{
		MutationRun *mutrun = mutruns_[run_index].get();
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

//	*********************	- (integer$)sumOfMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Genome::ExecuteMethod_sumOfMutationsOfType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	
	if (IsNull())
		EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_sumOfMutationsOfType): sumOfMutationsOfType() cannot be called on a null genome." << EidosTerminate();
	
	SLiMSim &sim = subpop_->population_.sim_;
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, sim, "sumOfMutationsOfType()");
	
	// Count the number of mutations of the given type
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	double selcoeff_sum = 0.0;
	int mutrun_count = mutrun_count_;
	
	for (int run_index = 0; run_index < mutrun_count; ++run_index)
	{
		MutationRun *mutrun = mutruns_[run_index].get();
		int genome1_count = mutrun->size();
		const MutationIndex *genome_ptr = mutrun->begin_pointer_const();
		
		for (int mut_index = 0; mut_index < genome1_count; ++mut_index)
		{
			Mutation *mut_ptr = mut_block_ptr + genome_ptr[mut_index];
			
			if (mut_ptr->mutation_type_ptr_ == mutation_type_ptr)
				selcoeff_sum += mut_ptr->selection_coeff_;
		}
	}
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(selcoeff_sum));
}

// print the sample represented by genomes, using SLiM's own format
void Genome::PrintGenomes_SLiM(std::ostream &p_out, std::vector<Genome *> &p_genomes, slim_objectid_t p_source_subpop_id)
{
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	slim_popsize_t sample_size = (slim_popsize_t)p_genomes.size();
	
	// get the polymorphisms within the sample
	PolymorphismMap polymorphisms;
	
	for (slim_popsize_t s = 0; s < sample_size; s++)
	{
		Genome &genome = *p_genomes[s];
		
		if (genome.IsNull())
			EIDOS_TERMINATION << "ERROR (Genome::PrintGenomes_SLiM): cannot output null genomes." << EidosTerminate();
		
		for (int run_index = 0; run_index < genome.mutrun_count_; ++run_index)
		{
			MutationRun *mutrun = genome.mutruns_[run_index].get();
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
		polymorphism_pair.second.Print_ID(p_out);
	
	// print the sample's genomes
	p_out << "Genomes:" << std::endl;
	
	for (slim_popsize_t j = 0; j < sample_size; j++)														// go through all individuals
	{
		Genome &genome = *p_genomes[j];
		
		if (p_source_subpop_id == -1)
			p_out << "p*:" << j;
		else
			p_out << "p" << p_source_subpop_id << ":" << j;
		
		p_out << " " << genome.Type();
		
		for (int run_index = 0; run_index < genome.mutrun_count_; ++run_index)
		{
			MutationRun *mutrun = genome.mutruns_[run_index].get();
			int mut_count = mutrun->size();
			const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
			
			for (int mut_index = 0; mut_index < mut_count; ++mut_index)
			{
				slim_polymorphismid_t polymorphism_id = FindMutationInPolymorphismMap(polymorphisms, mut_block_ptr + mut_ptr[mut_index]);
				
				if (polymorphism_id == -1)
					EIDOS_TERMINATION << "ERROR (Genome::PrintGenomes_SLiM): (internal error) polymorphism not found." << EidosTerminate();
				
				p_out << " " << polymorphism_id;
			}
		}
		
		p_out << std::endl;
	}
}

// print the sample represented by genomes, using "ms" format
void Genome::PrintGenomes_MS(std::ostream &p_out, std::vector<Genome *> &p_genomes, const Chromosome &p_chromosome, bool p_filter_monomorphic)
{
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	slim_popsize_t sample_size = (slim_popsize_t)p_genomes.size();
	
	// BCH 7 Nov. 2016: sort the polymorphisms by position since that is the expected sort
	// order in MS output.  In other types of output, sorting by the mutation id seems to
	// be fine.
	std::vector<Polymorphism> sorted_polymorphisms;
	
	{
		// get the polymorphisms within the sample
		PolymorphismMap polymorphisms;
		
		for (slim_popsize_t s = 0; s < sample_size; s++)
		{
			Genome &genome = *p_genomes[s];
			
			if (genome.IsNull())
				EIDOS_TERMINATION << "ERROR (Genome::PrintGenomes_MS): cannot output null genomes." << EidosTerminate();
			
			for (int run_index = 0; run_index < genome.mutrun_count_; ++run_index)
			{
				MutationRun *mutrun = genome.mutruns_[run_index].get();
				int mut_count = mutrun->size();
				const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
				
				for (int mut_index = 0; mut_index < mut_count; ++mut_index)
					AddMutationToPolymorphismMap(&polymorphisms, mut_block_ptr + mut_ptr[mut_index]);
			}
		}
		
		for (const PolymorphismPair &polymorphism_pair : polymorphisms)
			sorted_polymorphisms.push_back(polymorphism_pair.second);
		
		std::sort(sorted_polymorphisms.begin(), sorted_polymorphisms.end());
	}
	
	// if requested, remove polymorphisms that are not polymorphic within the sample
	if (p_filter_monomorphic)
	{
		std::vector<Polymorphism> filtered_polymorphisms;
		
		for (Polymorphism &p : sorted_polymorphisms)
			if (p.prevalence_ != sample_size)
				filtered_polymorphisms.push_back(p);
		
		std::swap(sorted_polymorphisms, filtered_polymorphisms);
	}
	
	// make a hash table that looks up the genotype string position from a mutation pointer
	std::unordered_map<const Mutation*, int> genotype_string_positions;
	int genotype_string_position = 0;
	
	for (const Polymorphism &polymorphism : sorted_polymorphisms) 
		genotype_string_positions.insert(std::pair<const Mutation*, int>(polymorphism.mutation_ptr_, genotype_string_position++));
	
	// print header
	p_out << "//" << std::endl << "segsites: " << sorted_polymorphisms.size() << std::endl;
	
	// print the sample's positions
	if (sorted_polymorphisms.size() > 0)
	{
		// Save flags/precision
		std::ios_base::fmtflags oldflags = p_out.flags();
		std::streamsize oldprecision = p_out.precision();
		
		p_out << std::fixed << std::setprecision(7);
		
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
		Genome &genome = *p_genomes[j];
		std::string genotype(sorted_polymorphisms.size(), '0'); // fill with 0s
		
		for (int run_index = 0; run_index < genome.mutrun_count_; ++run_index)
		{
			MutationRun *mutrun = genome.mutruns_[run_index].get();
			int mut_count = mutrun->size();
			const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
			
			for (int mut_index = 0; mut_index < mut_count; ++mut_index)
			{
				const Mutation *mutation = mut_block_ptr + mut_ptr[mut_index];
				auto found_position = genotype_string_positions.find(mutation);
				
				// BCH 4/24/2019: when p_filter_monomorphic is true, mutations in a given genome may not exist in the position map
				if (found_position != genotype_string_positions.end())
					genotype.replace(found_position->second, 1, "1");
			}
		}
		
		p_out << genotype << std::endl;
	}
}

// print the sample represented by genomes, using "vcf" format
void Genome::PrintGenomes_VCF(std::ostream &p_out, std::vector<Genome *> &p_genomes, bool p_output_multiallelics, bool p_simplify_nucs, bool p_output_nonnucs, bool p_nucleotide_based, NucleotideArray *p_ancestral_seq)
{
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	slim_popsize_t sample_size = (slim_popsize_t)p_genomes.size();
	
	if (sample_size % 2 == 1)
		EIDOS_TERMINATION << "ERROR (Genome::PrintGenomes_VCF): Genome vector must be an even length, since genomes are paired into individuals." << EidosTerminate();
	
	sample_size /= 2;
	
	// get the polymorphisms within the sample
	PolymorphismMap polymorphisms;
	
	for (slim_popsize_t s = 0; s < sample_size; s++)
	{
		Genome &genome1 = *p_genomes[s * 2];
		Genome &genome2 = *p_genomes[s * 2 + 1];
		
		if (!genome1.IsNull())
		{
			for (int run_index = 0; run_index < genome1.mutrun_count_; ++run_index)
			{
				MutationRun *mutrun = genome1.mutruns_[run_index].get();
				int mut_count = mutrun->size();
				const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
				
				for (int mut_index = 0; mut_index < mut_count; ++mut_index)
					AddMutationToPolymorphismMap(&polymorphisms, mut_block_ptr + mut_ptr[mut_index]);
			}
		}
		
		if (!genome2.IsNull())
		{
			for (int run_index = 0; run_index < genome2.mutrun_count_; ++run_index)
			{
				MutationRun *mutrun = genome2.mutruns_[run_index].get();
				int mut_count = mutrun->size();
				const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
				
				for (int mut_index = 0; mut_index < mut_count; ++mut_index)
					AddMutationToPolymorphismMap(&polymorphisms, mut_block_ptr + mut_ptr[mut_index]);
			}
		}
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
	
	// BCH 6 March 2019: Note that all of the INFO fields that provide per-mutation information have been
	// changed from a Number of 1 to a Number of ., since in nucleotide-based models we can call more than
	// one allele in a single call line (unlike in non-nucleotide-based models).
	p_out << "##source=SLiM" << std::endl;
	p_out << "##INFO=<ID=MID,Number=.,Type=Integer,Description=\"Mutation ID in SLiM\">" << std::endl;
	p_out << "##INFO=<ID=S,Number=.,Type=Float,Description=\"Selection Coefficient\">" << std::endl;
	p_out << "##INFO=<ID=DOM,Number=.,Type=Float,Description=\"Dominance\">" << std::endl;
	p_out << "##INFO=<ID=PO,Number=.,Type=Integer,Description=\"Population of Origin\">" << std::endl;
	p_out << "##INFO=<ID=GO,Number=.,Type=Integer,Description=\"Generation of Origin\">" << std::endl;
	p_out << "##INFO=<ID=MT,Number=.,Type=Integer,Description=\"Mutation Type\">" << std::endl;
	p_out << "##INFO=<ID=AC,Number=.,Type=Integer,Description=\"Allele Count\">" << std::endl;
	p_out << "##INFO=<ID=DP,Number=1,Type=Integer,Description=\"Total Depth\">" << std::endl;
	if (p_output_multiallelics && !p_nucleotide_based)
		p_out << "##INFO=<ID=MULTIALLELIC,Number=0,Type=Flag,Description=\"Multiallelic\">" << std::endl;
	if (p_nucleotide_based)
		p_out << "##INFO=<ID=AA,Number=1,Type=String,Description=\"Ancestral Allele\">" << std::endl;
	if (p_output_nonnucs && p_nucleotide_based)
		p_out << "##INFO=<ID=NONNUC,Number=0,Type=Flag,Description=\"Non-nucleotide-based\">" << std::endl;
	p_out << "##FORMAT=<ID=GT,Number=1,Type=String,Description=\"Genotype\">" << std::endl;
	p_out << "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT";
	
	for (slim_popsize_t s = 0; s < sample_size; s++)
		p_out << "\ti" << s;
	p_out << std::endl;
	
	// We want to output polymorphisms sorted by position (starting in SLiM 3.3), to facilitate
	// calling all of the nucleotide mutations at a given position with a single call line.
	std::vector<Polymorphism> sorted_polymorphisms;
	
	for (const PolymorphismPair &polymorphism_pair : polymorphisms)
		sorted_polymorphisms.push_back(polymorphism_pair.second);
	
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
					nuc_based.push_back(&polymorphism);
				else
					nonnuc_based.push_back(&polymorphism);
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
		if (p_nucleotide_based && (nuc_based.size() > 0))
		{
			// Get the ancestral nucleotide at this position; this will be index 0
			// Indices 1..n will be used for the corresponding mutations in nonnuc_based
			// Note that this means it is 
			static const char nuc_chars[4] = {'A', 'C', 'G', 'T'};
			int ancestral_nuc_index = p_ancestral_seq->NucleotideAtIndex(mut_position);		// 0..3 for ACGT
			
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
					int derived_nuc_index = polymorphism->mutation_ptr_->nucleotide_;
					
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
					p_out << "1\t" << (mut_position + 1) << "\t.\t";			// +1 because VCF uses 1-based positions
					
					// emit REF ("A" etc.)
					p_out << nuc_chars[ancestral_nuc_index];
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
							
							p_out << nuc_chars[nuc_index];
						}
					}
					
					// emit QUAL (1000), FILTER (PASS)
					p_out << "\t1000\tPASS\t";
					
					// emit the INFO fields and the Genotype marker; note mutation-specific fields are omitted since we are aggregating
					p_out << "AC=";
					firstEmitted = true;
					for (int nuc_index=0; nuc_index < 4; ++nuc_index)
					{
						if (total_prevalence[nuc_index] > 0)
						{
							if (!firstEmitted)
								p_out << ',';
							firstEmitted = false;
							
							p_out << total_prevalence[nuc_index];
						}
					}
					p_out << ";DP=1000;";
					p_out << "AA=" << nuc_chars[ancestral_nuc_index];
					
					p_out << "\tGT";
					
					// emit the individual calls
					for (slim_popsize_t s = 0; s < sample_size; s++)
					{
						Genome &g1 = *p_genomes[s * 2];
						Genome &g2 = *p_genomes[s * 2 + 1];
						bool g1_null = g1.IsNull(), g2_null = g2.IsNull();
						
						if (g1_null && g2_null)
						{
							// Both genomes are null; we should have eliminated the possibility of this with the check above
							EIDOS_TERMINATION << "ERROR (Population::PrintGenomes_VCF): (internal error) no non-null genome to output for individual." << EidosTerminate();
						}
						
						p_out << '\t';
						
						for (int genome_index = 0; genome_index <= 1; ++genome_index)
						{
							Genome &genome = (genome_index == 0) ? g1 : g2;
							
							if (!genome.IsNull())
							{
								// Find and emit the nuc-based mut contained by this genome, if any.  If more than one nuc-based mut is contained, it is an error.
								int contained_mut_index = -1;
								
								for (int muts_index = 0; muts_index < (int)nuc_based.size(); ++muts_index)
								{
									const Mutation *mutation = nuc_based[muts_index]->mutation_ptr_;
									
									if (genome.contains_mutation(mutation->BlockIndex()))
									{
										if (contained_mut_index == -1)
											contained_mut_index = muts_index;
										else
											EIDOS_TERMINATION << "ERROR (Population::PrintGenomes_VCF): more than one nucleotide-based mutation encountered at the same position in the same genome; the nucleotide cannot be called." << EidosTerminate();
									}
								}
								
								if (contained_mut_index == -1)
									p_out << '0';
								else
									p_out << allele_index_for_nuc[nuc_based[contained_mut_index]->mutation_ptr_->nucleotide_];
							}
							
							// If both genomes are non-null, emit a separator
							if ((genome_index == 0) && !g1_null && !g2_null)
								p_out << '|';
						}
					}
				}
			}
			else
			{
				// emit CHROM ("1"), POS, ID (".")
				p_out << "1\t" << (mut_position + 1) << "\t.\t";			// +1 because VCF uses 1-based positions
				
				// emit REF ("A" etc.)
				p_out << nuc_chars[ancestral_nuc_index];
				p_out << "\t";
				
				// emit ALT ("T" etc.)
				for (Polymorphism *polymorphism : nuc_based)
				{
					if (polymorphism != nuc_based.front())
						p_out << ',';
					p_out << nuc_chars[polymorphism->mutation_ptr_->nucleotide_];
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
					p_out << polymorphism->mutation_ptr_->mutation_type_ptr_->dominance_coeff_;
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
				
				p_out << "GO=";
				for (Polymorphism *polymorphism : nuc_based)
				{
					if (polymorphism != nuc_based.front())
						p_out << ',';
					p_out << polymorphism->mutation_ptr_->origin_generation_;
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
				
				p_out << "AA=" << nuc_chars[ancestral_nuc_index];
				
				p_out << "\tGT";
				
				// emit the individual calls
				for (slim_popsize_t s = 0; s < sample_size; s++)
				{
					Genome &g1 = *p_genomes[s * 2];
					Genome &g2 = *p_genomes[s * 2 + 1];
					bool g1_null = g1.IsNull(), g2_null = g2.IsNull();
					
					if (g1_null && g2_null)
					{
						// Both genomes are null; we should have eliminated the possibility of this with the check above
						EIDOS_TERMINATION << "ERROR (Population::PrintGenomes_VCF): (internal error) no non-null genome to output for individual." << EidosTerminate();
					}
					
					p_out << '\t';
					
					for (int genome_index = 0; genome_index <= 1; ++genome_index)
					{
						Genome &genome = (genome_index == 0) ? g1 : g2;
						
						if (!genome.IsNull())
						{
							// Find and emit the nuc-based mut contained by this genome, if any.  If more than one nuc-based mut is contained, it is an error.
							int contained_mut_index = -1;
							
							for (int muts_index = 0; muts_index < (int)nuc_based.size(); ++muts_index)
							{
								const Mutation *mutation = nuc_based[muts_index]->mutation_ptr_;
								
								if (genome.contains_mutation(mutation->BlockIndex()))
								{
									if (contained_mut_index == -1)
										contained_mut_index = muts_index;
									else
										EIDOS_TERMINATION << "ERROR (Population::PrintGenomes_VCF): more than one nucleotide-based mutation encountered at the same position in the same genome; the nucleotide cannot be called." << EidosTerminate();
								}
							}
							
							if (contained_mut_index == -1)
								p_out << '0';
							else
								p_out << (contained_mut_index + 1);
						}
						
						// If both genomes are non-null, emit a separator
						if ((genome_index == 0) && !g1_null && !g2_null)
							p_out << '|';
					}
				}
			}
			
			p_out << std::endl;
		}
		
		// Emit the non-nucleotide-based mutations at this position as individual call lines, each as an A->T mutation
		// We do this if outputNonnucleotides==T, or if we are non-nucleotide-based (in which case outputNonnucleotides is ignored)
		if (p_output_nonnucs || !p_nucleotide_based)
		{
			for (Polymorphism *polymorphism : nonnuc_based)
			{
				const Mutation *mutation = polymorphism->mutation_ptr_;
				
				// Count the mutations at the given position to determine if we are multiallelic
				int allele_count = (int)nonnuc_based.size();
				
				// Output this mutation if (1) we are outputting multiallelics in a non-nuc-based model, or (2) we are a nuc-based model (regardless of allele count), or (3) it is not multiallelic
				if (p_output_multiallelics || p_nucleotide_based || (allele_count == 1))
				{
					// emit CHROM ("1"), POS, ID ("."), REF ("A"), and ALT ("T")
					p_out << "1\t" << (mut_position + 1) << "\t.\tA\tT";			// +1 because VCF uses 1-based positions
					
					// emit QUAL (1000), FILTER (PASS)
					p_out << "\t1000\tPASS\t";
					
					// emit the INFO fields and the Genotype marker
					p_out << "MID=" << mutation->mutation_id_ << ";";
					p_out << "S=" << mutation->selection_coeff_ << ";";
					p_out << "DOM=" << mutation->mutation_type_ptr_->dominance_coeff_ << ";";
					p_out << "PO=" << mutation->subpop_index_ << ";";
					p_out << "GO=" << mutation->origin_generation_ << ";";
					p_out << "MT=" << mutation->mutation_type_ptr_->mutation_type_id_ << ";";
					p_out << "AC=" << polymorphism->prevalence_ << ";";
					p_out << "DP=1000";
					
					if (!p_nucleotide_based && (allele_count > 1))	// output MULTIALLELIC flags only in non-nuc-based models
						p_out << ";MULTIALLELIC";
					if (p_nucleotide_based && p_output_nonnucs)
						p_out << ";NONNUC";
					
					p_out << "\tGT";
					
					// emit the individual calls
					for (slim_popsize_t s = 0; s < sample_size; s++)
					{
						Genome &g1 = *p_genomes[s * 2];
						Genome &g2 = *p_genomes[s * 2 + 1];
						bool g1_null = g1.IsNull(), g2_null = g2.IsNull();
						
						if (g1_null && g2_null)
						{
							// Both genomes are null; we should have eliminated the possibility of this with the check above
							EIDOS_TERMINATION << "ERROR (Population::PrintGenomes_VCF): (internal error) no non-null genome to output for individual." << EidosTerminate();
						}
						else if (g1_null)
						{
							// An unpaired X or Y; we emit this as haploid, I think that is the right call...
							p_out << (g2.contains_mutation(mutation->BlockIndex()) ? "\t1" : "\t0");
						}
						else if (g2_null)
						{
							// An unpaired X or Y; we emit this as haploid, I think that is the right call...
							p_out << (g1.contains_mutation(mutation->BlockIndex()) ? "\t1" : "\t0");
						}
						else
						{
							// Both genomes are non-null; emit an x|y pair that indicates the data is phased
							bool g1_has_mut = g1.contains_mutation(mutation->BlockIndex());
							bool g2_has_mut = g2.contains_mutation(mutation->BlockIndex());
							
							if (g1_has_mut && g2_has_mut)	p_out << "\t1|1";
							else if (g1_has_mut)			p_out << "\t1|0";
							else if (g2_has_mut)			p_out << "\t0|1";
							else							p_out << "\t0|0";
						}
					}
					
					p_out << std::endl;
				}
			}
		}
		
		// polyiter already points to the mutation at the next position, or to end(), so we don't advance it here
	}
}

size_t Genome::MemoryUsageForMutrunBuffers(void)
{
	if (mutruns_ == run_buffer_)
		return 0;
	else
		return mutrun_count_ * sizeof(MutationRun_SP);
}


//
//	Genome_Class
//
#pragma mark -
#pragma mark Genome_Class
#pragma mark -

class Genome_Class : public EidosObjectClass
{
public:
	Genome_Class(const Genome_Class &p_original) = delete;	// no copy-construct
	Genome_Class& operator=(const Genome_Class&) = delete;	// no copying
	inline Genome_Class(void) { }
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_addMutations(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_addNewMutation(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_outputX(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_readFromMS(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_readFromVCF(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_removeMutations(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_Genome_Class = new Genome_Class();


const std::string &Genome_Class::ElementType(void) const
{
	return gEidosStr_Genome;		// in Eidos; see EidosValue_Object::EidosValue_Object()
}

const std::vector<const EidosPropertySignature *> *Genome_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomePedigreeID,true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Genome::GetProperty_Accelerated_genomePedigreeID));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomeType,		true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_isNullGenome,	true,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Genome::GetProperty_Accelerated_isNullGenome));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutations,		true,	kEidosValueMaskObject, gSLiM_Mutation_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Genome::GetProperty_Accelerated_tag)->DeclareAcceleratedSet(Genome::SetProperty_Accelerated_tag));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<const EidosMethodSignature *> *Genome_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_addMutations, kEidosValueMaskVOID))->AddObject("mutations", gSLiM_Mutation_Class));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_addNewDrawnMutation, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject("mutationType", gSLiM_MutationType_Class)->AddInt("position")->AddInt_ON("originGeneration", gStaticEidosValueNULL)->AddIntObject_ON("originSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddIntString_ON("nucleotide", gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_addNewMutation, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject("mutationType", gSLiM_MutationType_Class)->AddNumeric("selectionCoeff")->AddInt("position")->AddInt_ON("originGeneration", gStaticEidosValueNULL)->AddIntObject_ON("originSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddIntString_ON("nucleotide", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_containsMarkerMutation, kEidosValueMaskLogical | kEidosValueMaskSingleton | kEidosValueMaskNULL | kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class)->AddInt_S("position")->AddLogical_OS("returnMutation", gStaticEidosValue_LogicalF));
		methods->emplace_back(((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_containsMutations, kEidosValueMaskLogical))->AddObject("mutations", gSLiM_Mutation_Class))->DeclareAcceleratedImp(Genome::ExecuteMethod_Accelerated_containsMutations));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_countOfMutationsOfType, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_positionsOfMutationsOfType, kEidosValueMaskInt))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationsOfType, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_nucleotides, kEidosValueMaskInt | kEidosValueMaskString))->AddInt_OSN(gEidosStr_start, gStaticEidosValueNULL)->AddInt_OSN(gEidosStr_end, gStaticEidosValueNULL)->AddString_OS("format", EidosValue_String_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("string"))));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_readFromMS, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddString_S("filePath")->AddIntObject_S("mutationType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_readFromVCF, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddString_S("filePath")->AddIntObject_OSN("mutationType", gSLiM_MutationType_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_removeMutations, kEidosValueMaskVOID))->AddObject_ON("mutations", gSLiM_Mutation_Class, gStaticEidosValueNULL)->AddLogical_OS("substitute", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_outputMS, kEidosValueMaskVOID))->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("filterMonomorphic", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_outputVCF, kEidosValueMaskVOID))->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("outputMultiallelics", gStaticEidosValue_LogicalT)->AddLogical_OS("append", gStaticEidosValue_LogicalF)->AddLogical_OS("simplifyNucleotides", gStaticEidosValue_LogicalF)->AddLogical_OS("outputNonnucleotides", gStaticEidosValue_LogicalT));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_output, kEidosValueMaskVOID))->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_sumOfMutationsOfType, kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

EidosValue_SP Genome_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	switch (p_method_id)
	{
		case gID_addMutations:			return ExecuteMethod_addMutations(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
		case gID_addNewDrawnMutation:
		case gID_addNewMutation:		return ExecuteMethod_addNewMutation(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
		case gID_output:
		case gID_outputMS:
		case gID_outputVCF:				return ExecuteMethod_outputX(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
		case gID_readFromMS:			return ExecuteMethod_readFromMS(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
		case gID_readFromVCF:			return ExecuteMethod_readFromVCF(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
		case gID_removeMutations:		return ExecuteMethod_removeMutations(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
		default:						return EidosObjectClass::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
	}
}

//	*********************	+ (void)addMutations(object mutations)
//
EidosValue_SP Genome_Class::ExecuteMethod_addMutations(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutations_value = p_arguments[0].get();
	
	// FIXME this method should be optimized for large-scale bulk addition in the same
	// way that addNewMutation() and addNewDrawnMutation() now are.  BCH 29 Oct 2017
	
	int target_size = p_target->Count();
	
	if (target_size == 0)
		return gStaticEidosValueVOID;
	
	// Use the 0th genome in the target to find out what the mutation run length is, so we can calculate run indices
	Genome *genome_0 = (Genome *)p_target->ObjectElementAtIndex(0, nullptr);
	slim_position_t mutrun_length = genome_0->mutrun_length_;
	SLiMSim &sim = genome_0->subpop_->population_.sim_;
	Population &pop = sim.ThePopulation();
	
	sim.CheckMutationStackPolicy();
	
	if (!sim.warned_early_mutation_add_)
	{
		if (sim.GenerationStage() == SLiMGenerationStage::kWFStage1ExecuteEarlyScripts)
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (Genome_Class::ExecuteMethod_addMutations): addMutations() should probably not be called from an early() event in a WF model; the added mutation(s) will not influence fitness values during offspring generation." << std::endl;
				sim.warned_early_mutation_add_ = true;
			}
		}
		if (sim.GenerationStage() == SLiMGenerationStage::kNonWFStage6ExecuteLateScripts)
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (Genome_Class::ExecuteMethod_addMutations): addMutations() should probably not be called from a late() event in a nonWF model; the added mutation(s) will not influence fitness values until partway through the next generation." << std::endl;
				sim.warned_early_mutation_add_ = true;
			}
		}
	}
	
	if (pop.sim_.executing_block_type_ == SLiMEidosBlockType::SLiMEidosModifyChildCallback)
	{
		// Check that we're not inside a modifyChild() callback, or if we are, that the only genomes being modified belong to the new child.
		// This prevents problems with retracting the proposed child when tree-sequence recording is enabled; other extraneous changes must
		// not be backed out, and it's hard to separate, e.g., what's a child-related new mutation from an extraneously added new mutation.
		// Note that the other Genome methods that add/remove mutations perform the same check, and should be maintained in parallel.
		Individual *focal_modification_child = pop.sim_.focal_modification_child_;
		
		if (focal_modification_child)
		{
			Genome *focal_genome_1 = focal_modification_child->genome1_, *focal_genome_2 = focal_modification_child->genome2_;
			
			for (int genome_index = 0; genome_index < target_size; ++genome_index)
			{
				Genome *target_genome = (Genome *)p_target->ObjectElementAtIndex(genome_index, nullptr);
				
				if ((target_genome != focal_genome_1) && (target_genome != focal_genome_2))
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addMutations): addMutations() cannot be called from within a modifyChild() callback to modify any genomes except those of the focal child being generated." << EidosTerminate();
			}
		}
	}
	else if ((pop.sim_.executing_block_type_ == SLiMEidosBlockType::SLiMEidosRecombinationCallback) ||
			 (pop.sim_.executing_block_type_ == SLiMEidosBlockType::SLiMEidosMutationCallback))
		EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addMutations): addMutations() cannot be called from within a recombination() or mutation() callback." << EidosTerminate();
	
	// Construct a vector of mutations to add that is sorted by position
	int mutations_count = mutations_value->Count();
	std::vector<Mutation *> mutations_to_add;
	
	for (int value_index = 0; value_index < mutations_count; ++value_index)
		mutations_to_add.push_back((Mutation *)mutations_value->ObjectElementAtIndex(value_index, nullptr));
	
	std::sort(mutations_to_add.begin(), mutations_to_add.end(), [ ](Mutation *i1, Mutation *i2) {return i1->position_ < i2->position_;});
	
	// TREE SEQUENCE RECORDING
	// First, pre-plan the positions of new tree-seq derived states in anticipation of doing the addition.  We have to check
	// whether the mutation being added is already present, to avoid recording a new derived state identical to the old one state.
	// The algorithm used here, with GenomeWalker, depends upon the fact that we just sorted the mutations to add by position.
	// However, we do still have to think about multiple muts being added at the same position, and existing stacked mutations.
	bool recording_tree_sequence_mutations = sim.RecordingTreeSequenceMutations();
	std::vector<std::pair<Genome *, std::vector<slim_position_t>>> new_derived_state_positions;
	
	if (recording_tree_sequence_mutations)
	{
		for (int genome_index = 0; genome_index < target_size; ++genome_index)
		{
			Genome *target_genome = (Genome *)p_target->ObjectElementAtIndex(genome_index, nullptr);
			GenomeWalker walker(target_genome);
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
					// no pair entry in new_derived_state_positions yet, so make a new pair entry for this genome
					new_derived_state_positions.emplace_back(std::pair<Genome *, std::vector<slim_position_t>>(target_genome, std::vector<slim_position_t>(1, mut_pos)));
				}
				else
				{
					// we have an existing pair entry for this genome, so add this position to its position vector
					std::pair<Genome *, std::vector<slim_position_t>> &genome_entry = new_derived_state_positions.back();
					std::vector<slim_position_t> &genome_list = genome_entry.second;
					
					genome_list.push_back(mut_pos);
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
		int64_t operation_id = ++gSLiM_MutationRun_OperationID;
		
		Genome::BulkOperationStart(operation_id, mutrun_index);
		
		for (int genome_index = 0; genome_index < target_size; ++genome_index)
		{
			Genome *target_genome = (Genome *)p_target->ObjectElementAtIndex(genome_index, nullptr);
			
			if (target_genome->IsNull())
				EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addMutations): addMutations() cannot be called on a null genome." << EidosTerminate();
			
			// See if WillModifyRunForBulkOperation() can short-circuit the operation for us
			if (target_genome->WillModifyRunForBulkOperation(operation_id, mutrun_index))
			{
				for (int mut_index = value_index; mut_index < mutations_count; ++mut_index)
				{
					Mutation *mut_to_add = mutations_to_add[mut_index];
					const slim_position_t add_pos = mut_to_add->position_;
					
					// since we're in sorted order by position, as soon as we leave the current mutation run we're done
					if (add_pos / mutrun_length != mutrun_index)
						break;
					
					if (target_genome->enforce_stack_policy_for_addition(mut_to_add->position_, mut_to_add->mutation_type_ptr_))
					{
						target_genome->insert_sorted_mutation_if_unique(mut_to_add->BlockIndex());
						
						// No need to add the mutation to the registry; how would the user ever get a Mutation that was not already in it?
						// Similarly, no need to check and set pure_neutral_ and all_pure_neutral_DFE_; the mutation is already in the system
					}
				}
			}
		}
		
		Genome::BulkOperationEnd(operation_id, mutrun_index);
		
		// now we have handled all mutations at this index (and all previous indices)
		last_handled_mutrun_index = mutrun_index;
		
		// invalidate cached mutation refcounts; refcounts have changed
		pop.cached_tally_genome_count_ = 0;
	}
	
	// TREE SEQUENCE RECORDING
	// Now that all the bulk operations are done, record all the new derived states
	if (recording_tree_sequence_mutations)
	{
		for (std::pair<Genome *, std::vector<slim_position_t>> &genome_pair : new_derived_state_positions)
		{
			Genome *target_genome = genome_pair.first;
			std::vector<slim_position_t> &genome_positions = genome_pair.second;
			
			for (slim_position_t position : genome_positions)
				sim.RecordNewDerivedState(target_genome, position, *target_genome->derived_mutation_ids_at_position(position));
		}
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	+ (object<Mutation>)addNewDrawnMutation(io<MutationType> mutationType, integer position, [Ni originGeneration = NULL], [Nio<Subpopulation> originSubpop = NULL], [Nis nucleotide = NULL])
//	*********************	+ (object<Mutation>)addNewMutation(io<MutationType> mutationType, numeric selectionCoeff, integer position, [Ni originGeneration = NULL], [Nio<Subpopulation> originSubpop = NULL], [Nis nucleotide = NULL])
//
EidosValue_SP Genome_Class::ExecuteMethod_addNewMutation(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_argument_count, p_interpreter)
	
#ifdef __clang_analyzer__
	assert(((p_method_id == gID_addNewDrawnMutation) && (p_argument_count == 5)) || ((p_method_id == gID_addNewMutation) && (p_argument_count == 6)));
#endif
	
	EidosValue *arg_muttype = p_arguments[0].get();
	EidosValue *arg_selcoeff = (p_method_id == gID_addNewDrawnMutation ? nullptr : p_arguments[1].get());
	EidosValue *arg_position = (p_method_id == gID_addNewDrawnMutation ? p_arguments[1].get() : p_arguments[2].get());
	EidosValue *arg_origin_gen = (p_method_id == gID_addNewDrawnMutation ? p_arguments[2].get() : p_arguments[3].get());
	EidosValue *arg_origin_subpop = (p_method_id == gID_addNewDrawnMutation ? p_arguments[3].get() : p_arguments[4].get());
	EidosValue *arg_nucleotide = (p_method_id == gID_addNewDrawnMutation ? p_arguments[4].get() : p_arguments[5].get());
	
	int target_size = p_target->Count();
	
	if (target_size == 0)
		return gStaticEidosValueNULLInvisible;	// this is almost an error condition, since a mutation was expected to be added and none was
	
	// Use the 0th genome in the target to find out what the mutation run length is, so we can calculate run indices
	Genome *genome_0 = (Genome *)p_target->ObjectElementAtIndex(0, nullptr);
	int mutrun_count = genome_0->mutrun_count_;
	slim_position_t mutrun_length = genome_0->mutrun_length_;
	SLiMSim &sim = genome_0->subpop_->population_.sim_;
	Population &pop = sim.ThePopulation();
	bool nucleotide_based = sim.IsNucleotideBased();
	
	if (!nucleotide_based && (arg_nucleotide->Type() != EidosValueType::kValueNULL))
		EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " requires nucleotide to be NULL in non-nucleotide-based models." << EidosTerminate();
	
	sim.CheckMutationStackPolicy();
	
	if (!sim.warned_early_mutation_add_)
	{
		if (sim.GenerationStage() == SLiMGenerationStage::kWFStage1ExecuteEarlyScripts)
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " should probably not be called from an early() event in a WF model; the added mutation will not influence fitness values during offspring generation." << std::endl;
				sim.warned_early_mutation_add_ = true;
			}
		}
		if (sim.GenerationStage() == SLiMGenerationStage::kNonWFStage6ExecuteLateScripts)
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " should probably not be called from a late() event in a nonWF model; the added mutation will not influence fitness values until partway through the next generation." << std::endl;
				sim.warned_early_mutation_add_ = true;
			}
		}
	}
	
	if (pop.sim_.executing_block_type_ == SLiMEidosBlockType::SLiMEidosModifyChildCallback)
	{
		// Check that we're not inside a modifyChild() callback, or if we are, that the only genomes being modified belong to the new child.
		// This prevents problems with retracting the proposed child when tree-sequence recording is enabled; other extraneous changes must
		// not be backed out, and it's hard to separate, e.g., what's a child-related new mutation from an extraneously added new mutation.
		// Note that the other Genome methods that add/remove mutations perform the same check, and should be maintained in parallel.
		Individual *focal_modification_child = pop.sim_.focal_modification_child_;
		
		if (focal_modification_child)
		{
			Genome *focal_genome_1 = focal_modification_child->genome1_, *focal_genome_2 = focal_modification_child->genome2_;
			
			for (int genome_index = 0; genome_index < target_size; ++genome_index)
			{
				Genome *target_genome = (Genome *)p_target->ObjectElementAtIndex(genome_index, nullptr);
				
				if ((target_genome != focal_genome_1) && (target_genome != focal_genome_2))
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " cannot be called from within a modifyChild() callback to modify any genomes except those of the focal child being generated." << EidosTerminate();
			}
		}
	}
	else if ((pop.sim_.executing_block_type_ == SLiMEidosBlockType::SLiMEidosRecombinationCallback) ||
			 (pop.sim_.executing_block_type_ == SLiMEidosBlockType::SLiMEidosMutationCallback))
		EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " cannot be called from within a recombination() or mutation() callback." << EidosTerminate();
	
	// position, originGeneration, and originSubpop can now be either singletons or vectors of matching length or NULL; check them all
	int muttype_count = arg_muttype->Count();
	int selcoeff_count = (arg_selcoeff ? arg_selcoeff->Count() : 0);
	int position_count = arg_position->Count();
	int origin_gen_count = arg_origin_gen->Count();
	int origin_subpop_count = arg_origin_subpop->Count();
	int nucleotide_count = arg_nucleotide->Count();
	
	if (arg_origin_gen->Type() == EidosValueType::kValueNULL)
		origin_gen_count = 1;
	if (arg_origin_subpop->Type() == EidosValueType::kValueNULL)
		origin_subpop_count = 1;
	if (arg_nucleotide->Type() == EidosValueType::kValueNULL)
		nucleotide_count = 1;
	
	int count_to_add = std::max({muttype_count, selcoeff_count, position_count, origin_gen_count, origin_subpop_count, nucleotide_count});
	
	if (((muttype_count != 1) && (muttype_count != count_to_add)) ||
		(arg_selcoeff && (selcoeff_count != 1) && (selcoeff_count != count_to_add)) ||
		((position_count != 1) && (position_count != count_to_add)) ||
		((origin_gen_count != 1) && (origin_gen_count != count_to_add)) ||
		((origin_subpop_count != 1) && (origin_subpop_count != count_to_add)) ||
		((nucleotide_count != 1) && (nucleotide_count != count_to_add)))
		EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " requires that mutationType, " << ((p_method_id == gID_addNewMutation) ? "selectionCoeff, " : "") << "position, originGeneration, originSubpop, and nucleotide be either (1) singleton, or (2) equal in length to the other non-singleton argument(s), or (3) NULL, for originGeneration, originSubpop, and nucleotide." << EidosTerminate();
	
	EidosValue_Object_vector_SP retval(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
	
	if (count_to_add == 0)
		return retval;
	
	// before proceeding, let's check that all positions supplied are valid, so we don't need to worry about it below
	// would be better not to call IntAtIndex() multiple times for the same position, but that will not be the majority of our time anyway...
	slim_position_t last_position = sim.TheChromosome().last_position_;
	
	for (int position_index = 0; position_index < position_count; ++position_index)
	{
		slim_position_t position = SLiMCastToPositionTypeOrRaise(arg_position->IntAtIndex(position_index, nullptr));
		
		if (position > last_position)
			EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " position " << position << " is past the end of the chromosome." << EidosTerminate();
	}
	
	// similarly, check nucleotide values for validity
	uint8_t *nucleotide_lookup = NucleotideArray::NucleotideCharToIntLookup();
	
	if (arg_nucleotide->Type() == EidosValueType::kValueNULL)
	{
		// If nucleotide is NULL, all mutation types supplied must be non-nucleotide-based
		for (int muttype_index = 0; muttype_index < muttype_count; ++muttype_index)
		{
			MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(arg_muttype, muttype_index, sim, Eidos_StringForGlobalStringID(p_method_id).c_str());
			
			if (mutation_type_ptr->nucleotide_based_)
				EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " requires nucleotide to be non-NULL when nucleotide-based mutation types are used." << EidosTerminate();
		}
	}
	else
	{
		// If nucleotide is non-NULL, all mutation types supplied must be nucleotide-based
		for (int muttype_index = 0; muttype_index < muttype_count; ++muttype_index)
		{
			MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(arg_muttype, muttype_index, sim, Eidos_StringForGlobalStringID(p_method_id).c_str());
			
			if (!mutation_type_ptr->nucleotide_based_)
				EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " requires nucleotide to be NULL when non-nucleotide-based mutation types are used." << EidosTerminate();
		}
		
		// And then nucleotide values must also be within bounds
		if (arg_nucleotide->Type() == EidosValueType::kValueInt)
		{
			for (int nucleotide_index = 0; nucleotide_index < nucleotide_count; ++nucleotide_index)
			{
				int64_t nuc_int = arg_nucleotide->IntAtIndex(nucleotide_index, nullptr);
				
				if ((nuc_int < 0) || (nuc_int > 3))
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " requires integer nucleotide values to be in [0,3]." << EidosTerminate();
			}
		}
		else if (arg_nucleotide->Type() == EidosValueType::kValueString)
		{
			for (int nucleotide_index = 0; nucleotide_index < nucleotide_count; ++nucleotide_index)
			{
				uint8_t nuc = nucleotide_lookup[arg_nucleotide->StringAtIndex(nucleotide_index, nullptr)[0]];
				
				if (nuc > 3)
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " requires string nucleotide values to be 'A', 'C', 'G', or 'T'." << EidosTerminate();
			}
		}
	}
	
	// each bulk operation is performed on a single mutation run, so we need to figure out which runs we're influencing
	std::vector<slim_mutrun_index_t> mutrun_indexes;
	
	if (mutrun_count == 1)
	{
		// if we have just a single mutrun, we can avoid the sorting and uniquing; all valid positions are in mutrun 0
		mutrun_indexes.push_back(0);
	}
	else
	{
		for (int pos_index = 0; pos_index < position_count; ++pos_index)
		{
			slim_position_t position = SLiMCastToPositionTypeOrRaise(arg_position->IntAtIndex(pos_index, nullptr));
			mutrun_indexes.push_back((slim_mutrun_index_t)(position / mutrun_length));
		}
		
		std::sort(mutrun_indexes.begin(), mutrun_indexes.end());
		mutrun_indexes.resize(std::distance(mutrun_indexes.begin(), std::unique(mutrun_indexes.begin(), mutrun_indexes.end())));
	}
	
	// for the singleton case for each of the parameters, get all the info
	MutationType *singleton_mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(arg_muttype, 0, sim, Eidos_StringForGlobalStringID(p_method_id).c_str());
	
	double singleton_selection_coeff = (arg_selcoeff ? arg_selcoeff->FloatAtIndex(0, nullptr) : 0.0);
	
	slim_position_t singleton_position = SLiMCastToPositionTypeOrRaise(arg_position->IntAtIndex(0, nullptr));
	
	slim_generation_t singleton_origin_generation;
	
	if (arg_origin_gen->Type() == EidosValueType::kValueNULL)
		singleton_origin_generation = sim.Generation();
	else
		singleton_origin_generation = SLiMCastToGenerationTypeOrRaise(arg_origin_gen->IntAtIndex(0, nullptr));
	
	slim_objectid_t singleton_origin_subpop_id;
	
	if (arg_origin_subpop->Type() == EidosValueType::kValueNULL)
	{
		singleton_origin_subpop_id = -1;
		
		// We set the origin subpopulation based on the first genome in the target
		if (target_size >= 1)
		{
			Genome *first_target = (Genome *)p_target->ObjectElementAtIndex(0, nullptr);
			singleton_origin_subpop_id = first_target->subpop_->subpopulation_id_;
		}
	}
	else if (arg_origin_subpop->Type() == EidosValueType::kValueInt)
		singleton_origin_subpop_id = SLiMCastToObjectidTypeOrRaise(arg_origin_subpop->IntAtIndex(0, nullptr));
	else
#if DEBUG
		// Use dynamic_cast<> only in DEBUG since it is hella slow
		// The class should be guaranteed by the method signature already
		singleton_origin_subpop_id = dynamic_cast<Subpopulation *>(arg_origin_subpop->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
#else
		singleton_origin_subpop_id = ((Subpopulation *)(arg_origin_subpop->ObjectElementAtIndex(0, nullptr)))->subpopulation_id_;
#endif
	
	int64_t singleton_nucleotide;
	
	if (arg_nucleotide->Type() == EidosValueType::kValueNULL)
		singleton_nucleotide = -1;
	else if (arg_nucleotide->Type() == EidosValueType::kValueInt)
		singleton_nucleotide = arg_nucleotide->IntAtIndex(0, nullptr);
	else
		singleton_nucleotide = nucleotide_lookup[arg_nucleotide->StringAtIndex(0, nullptr)[0]];
	
	// ok, now loop to add the mutations in a single bulk operation per mutation run
	bool recording_tree_sequence_mutations = sim.RecordingTreeSequenceMutations();
	
	for (slim_mutrun_index_t mutrun_index : mutrun_indexes)
	{
		int64_t operation_id = ++gSLiM_MutationRun_OperationID;
		MutationRun &mutations_to_add = *MutationRun::NewMutationRun();		// take from shared pool of used objects;
		
		// Before starting the bulk operation for this mutation run, construct all of the mutations and add them all to the registry, etc.
		// It is possible that some mutations will not actually be added to any genome, due to stacking; they will be cleared from the
		// registry as lost mutations in the next generation.  All mutations are returned to the user, whether actually added or not.
		MutationType *mutation_type_ptr = singleton_mutation_type_ptr;
		double selection_coeff = singleton_selection_coeff;
		slim_position_t position = singleton_position;
		slim_generation_t origin_generation = singleton_origin_generation;
		slim_objectid_t origin_subpop_id = singleton_origin_subpop_id;
		int64_t nucleotide = singleton_nucleotide;
		
		for (int mut_parameter_index = 0; mut_parameter_index < count_to_add; ++mut_parameter_index)
		{
			if (position_count != 1)
				position = SLiMCastToPositionTypeOrRaise(arg_position->IntAtIndex(mut_parameter_index, nullptr));
			
			// check that this mutation will be added to this mutation run
			if (position / mutrun_length == mutrun_index)
			{
				if (muttype_count != 1)
					mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(arg_muttype, mut_parameter_index, sim, Eidos_StringForGlobalStringID(p_method_id).c_str());
				
				if (selcoeff_count != 1)
				{
					if (arg_selcoeff)
						selection_coeff = arg_selcoeff->FloatAtIndex(mut_parameter_index, nullptr);
					else
						selection_coeff = mutation_type_ptr->DrawSelectionCoefficient();
				}
				
				if (origin_gen_count != 1)
					origin_generation = SLiMCastToGenerationTypeOrRaise(arg_origin_gen->IntAtIndex(mut_parameter_index, nullptr));
				
				if (origin_subpop_count != 1)
				{
					if (arg_origin_subpop->Type() == EidosValueType::kValueInt)
						origin_subpop_id = SLiMCastToObjectidTypeOrRaise(arg_origin_subpop->IntAtIndex(mut_parameter_index, nullptr));
					else
#if DEBUG
						// Use dynamic_cast<> only in DEBUG since it is hella slow
						// The class should be guaranteed by the method signature already
						origin_subpop_id = dynamic_cast<Subpopulation *>(arg_origin_subpop->ObjectElementAtIndex(mut_parameter_index, nullptr))->subpopulation_id_;
#else
						origin_subpop_id = ((Subpopulation *)(arg_origin_subpop->ObjectElementAtIndex(mut_parameter_index, nullptr)))->subpopulation_id_;
#endif
				}
				
				if (nucleotide_count != 1)
				{
					// Already checked for validity above
					if (arg_nucleotide->Type() == EidosValueType::kValueInt)
						nucleotide = arg_nucleotide->IntAtIndex(mut_parameter_index, nullptr);
					else
						nucleotide = nucleotide_lookup[arg_nucleotide->StringAtIndex(mut_parameter_index, nullptr)[0]];
				}
				
				MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
				
				new (gSLiM_Mutation_Block + new_mut_index) Mutation(mutation_type_ptr, position, selection_coeff, origin_subpop_id, origin_generation, (int8_t)nucleotide);
				
				// This mutation type might not be used by any genomic element type (i.e. might not already be vetted), so we need to check and set pure_neutral_
				if (selection_coeff != 0.0)
				{
					sim.pure_neutral_ = false;
					
					// Fix all_pure_neutral_DFE_ if the selcoeff was not drawn from the muttype's DFE
					if (p_method_id == gID_addNewMutation)
						mutation_type_ptr->all_pure_neutral_DFE_ = false;
				}
				
				// add to the registry, return value, genome, etc.
				pop.mutation_registry_.emplace_back(new_mut_index);
				retval->push_object_element(gSLiM_Mutation_Block + new_mut_index);
				mutations_to_add.emplace_back(new_mut_index);
				
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
				if (pop.keeping_muttype_registries_ && mutation_type_ptr->keeping_muttype_registry_)
				{
					// This mutation type is also keeping its own private registry, so we need to add to that as well
					mutation_type_ptr->muttype_registry_.emplace_back(new_mut_index);
				}
#endif
			}
		}
		
		// Now start the bulk operation and add mutations_to_add to every target genome
		Genome::BulkOperationStart(operation_id, mutrun_index);
		
		for (int target_index = 0; target_index < target_size; ++target_index)
		{
			Genome *target_genome = (Genome *)p_target->ObjectElementAtIndex(target_index, nullptr);
			
			if (target_genome->IsNull())
				EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " cannot be called on a null genome." << EidosTerminate();
			
			// See if WillModifyRunForBulkOperation() can short-circuit the operation for us
			if (target_genome->WillModifyRunForBulkOperation(operation_id, mutrun_index))
			{
				MutationRun &original_mutrun = *(target_genome->mutruns_[mutrun_index]);
				MutationRun &merge_run = *MutationRun::NewMutationRun();		// take from shared pool of used objects;
				
				// We merge the original run and mutations_to_add into a new temporary mutrun, and then copy it back to the target
				merge_run.clear_set_and_merge(original_mutrun, mutations_to_add);
				original_mutrun.copy_from_run(merge_run);
				
				MutationRun::FreeMutationRun(&merge_run);
			}
			
			// TREE SEQUENCE RECORDING
			// whether WillModifyRunForBulkOperation() short-circuited the addition or not, we need to notify the tree seq code
			if (recording_tree_sequence_mutations)
			{
				MutationIndex *muts = mutations_to_add.begin_pointer();
				MutationIndex *muts_end = mutations_to_add.end_pointer();
				
				while (muts != muts_end)
				{
					Mutation *mut = gSLiM_Mutation_Block + *(muts++);
					slim_position_t pos = mut->position_;
					
					sim.RecordNewDerivedState(target_genome, pos, *target_genome->derived_mutation_ids_at_position(pos));
				}
			}
		}
		
		Genome::BulkOperationEnd(operation_id, mutrun_index);
		
		MutationRun::FreeMutationRun(&mutations_to_add);
		
		// invalidate cached mutation refcounts; refcounts have changed
		pop.cached_tally_genome_count_ = 0;
	}
	
	return retval;
}

//	*********************	+ (void)output([Ns$ filePath = NULL], [logical$ append=F])
//	*********************	+ (void)outputMS([Ns$ filePath = NULL], [logical$ append=F], [logical$ filterMonomorphic = F])
//	*********************	+ (void)outputVCF([Ns$ filePath = NULL], [logical$ outputMultiallelics = T], [logical$ append=F], [logical$ simplifyNucleotides = F], [logical$ outputNonnucleotides = T])
//
EidosValue_SP Genome_Class::ExecuteMethod_outputX(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_argument_count, p_interpreter)
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *outputMultiallelics_value = ((p_method_id == gID_outputVCF) ? p_arguments[1].get() : nullptr);
	EidosValue *append_value = ((p_method_id == gID_outputVCF) ? p_arguments[2].get() : p_arguments[1].get());
	EidosValue *filterMonomorphic_value = ((p_method_id == gID_outputMS) ? p_arguments[2].get() : nullptr);
	EidosValue *simplifyNucleotides_value = ((p_method_id == gID_outputVCF) ? p_arguments[3].get() : nullptr);
	EidosValue *outputNonnucleotides_value = ((p_method_id == gID_outputVCF) ? p_arguments[4].get() : nullptr);
	
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	Chromosome &chromosome = sim.TheChromosome();
	
	// default to outputting multiallelic positions (used by VCF output only)
	bool output_multiallelics = true;
	
	if (p_method_id == gID_outputVCF)
		output_multiallelics = outputMultiallelics_value->LogicalAtIndex(0, nullptr);
	
	bool simplify_nucs = false;
	
	if (p_method_id == gID_outputVCF)
		simplify_nucs = simplifyNucleotides_value->LogicalAtIndex(0, nullptr);
	
	bool output_nonnucs = true;
	
	if (p_method_id == gID_outputVCF)
		output_nonnucs = outputNonnucleotides_value->LogicalAtIndex(0, nullptr);
	
	// figure out if we're filtering out mutations that are monomorphic within the sample (MS output only)
	bool filter_monomorphic = false;
	
	if (p_method_id == gID_outputMS)
		filter_monomorphic = filterMonomorphic_value->LogicalAtIndex(0, nullptr);
	
	// Get all the genomes we're sampling from p_target
	int sample_size = p_target->Count();
	std::vector<Genome *> genomes;
	
	for (int index = 0; index < sample_size; ++index)
		genomes.push_back((Genome *)p_target->ObjectElementAtIndex(index, nullptr));
	
	// Now handle stream/file output and dispatch to the actual print method
	if (filePath_value->Type() == EidosValueType::kValueNULL)
	{
		// If filePath is NULL, output to our output stream
		std::ostream &output_stream = p_interpreter.ExecutionOutputStream();
		
		// For the output stream, we put out a descriptive SLiM-style header for all output types
		output_stream << "#OUT: " << sim.Generation() << " G";
		
		if (p_method_id == gID_output)
			output_stream << "S";
		else if (p_method_id == gID_outputMS)
			output_stream << "M";
		else if (p_method_id == gID_outputVCF)
			output_stream << "V";
		
		output_stream << " " << sample_size << std::endl;
		
		// Call out to print the actual sample
		if (p_method_id == gID_output)
			Genome::PrintGenomes_SLiM(output_stream, genomes, -1);	// -1 represents unknown source subpopulation
		else if (p_method_id == gID_outputMS)
			Genome::PrintGenomes_MS(output_stream, genomes, chromosome, filter_monomorphic);
		else if (p_method_id == gID_outputVCF)
			Genome::PrintGenomes_VCF(output_stream, genomes, output_multiallelics, simplify_nucs, output_nonnucs, sim.IsNucleotideBased(), sim.TheChromosome().AncestralSequence());
	}
	else
	{
		// Otherwise, output to filePath
		std::string outfile_path = Eidos_ResolvedPath(filePath_value->StringAtIndex(0, nullptr));
		bool append = append_value->LogicalAtIndex(0, nullptr);
		std::ofstream outfile;
		
		outfile.open(outfile_path.c_str(), append ? (std::ios_base::app | std::ios_base::out) : std::ios_base::out);
		
		if (outfile.is_open())
		{
			switch (p_method_id)
			{
				case gID_output:
					// For file output, we put out the descriptive SLiM-style header only for SLiM-format output
					outfile << "#OUT: " << sim.Generation() << " GS " << sample_size << " " << outfile_path << std::endl;
					Genome::PrintGenomes_SLiM(outfile, genomes, -1);	// -1 represents unknown source subpopulation
					break;
				case gID_outputMS:
					Genome::PrintGenomes_MS(outfile, genomes, chromosome, filter_monomorphic);
					break;
				case gID_outputVCF:
					Genome::PrintGenomes_VCF(outfile, genomes, output_multiallelics, simplify_nucs, output_nonnucs, sim.IsNucleotideBased(), sim.TheChromosome().AncestralSequence());
					break;
			}
			
			outfile.close(); 
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_outputX): could not open "<< outfile_path << "." << EidosTerminate();
		}
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	+ (o<Mutation>)readFromMS(s$ filePath = NULL, io<MutationType> mutationType)
//
EidosValue_SP Genome_Class::ExecuteMethod_readFromMS(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_argument_count, p_interpreter)
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *mutationType_value = p_arguments[1].get();
	
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	Population &pop = sim.ThePopulation();
	slim_position_t last_position = sim.TheChromosome().last_position_;
	bool recording_mutations = sim.RecordingTreeSequenceMutations();
	bool nucleotide_based = sim.IsNucleotideBased();
	std::string file_path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(filePath_value->StringAtIndex(0, nullptr)));
	MutationType *mutation_type_ptr = nullptr;
	
	if (mutationType_value->Type() != EidosValueType::kValueNULL)
		mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutationType_value, 0, sim, "ExecuteMethod_readFromMS()");
	
	if (!mutation_type_ptr)
		EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromMS): mutation type not found." << EidosTerminate();
	
	// Parse the whole input file and retain the information from it
	std::ifstream infile(file_path);
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
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromMS): expecting 'segsites:', found '" << sub << "'." << EidosTerminate();
				if (iss.eof())
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromMS): missing segsites value." << EidosTerminate();
				
				iss >> sub;
				int64_t segsites_long = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr);
				
				if ((segsites_long <= 0) || (segsites_long > 1000000))
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromMS): readMS() requires segsites in (0,1000000]." << EidosTerminate();
				
				segsites = (int)segsites_long;
				
				if (!iss.eof())
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromMS): malformed segsites line; additional content after segsites value." << EidosTerminate();
				
				parse_state = 1;
				break;
			}
			case 1:
			{
				// Expecting "positions: a b c..."
				std::istringstream iss(line);
				
				iss >> sub;
				if (sub != "positions:")
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromMS): expecting 'positions:', found '" << sub << "'." << EidosTerminate();
				
				for (int pos_index = 0; pos_index < segsites; ++pos_index)
				{
					if (iss.eof())
						EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromMS): missing positions value." << EidosTerminate();
					
					iss >> sub;
					double pos_double = EidosInterpreter::FloatForString(sub, nullptr);
					
					if ((pos_double < 0.0) || (pos_double > 1.0))
						EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromMS): readMS() requires positions in [0,1]." << EidosTerminate();
					
					positions.push_back((slim_position_t)round(pos_double * last_position));
				}
				
				if (!iss.eof())
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromMS): malformed positions line; additional content after last expected position." << EidosTerminate();
				
				parse_state = 2;
				break;
			}
			case 2:
			{
				// Expecting "001010011001101111010..." of length segsites
				if (line.find_first_not_of("01") != std::string::npos)
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromMS): call lines must be composed entirely of 0 and 1." << EidosTerminate();
				if ((int)line.length() != segsites)
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromMS): call lines must be equal in length to the segsites value." << EidosTerminate();
				
				calls.push_back(line);
				break;
			}
		}
	}
	
	infile.close();
	
	// Get the target genomes into a vector
	int target_size = p_target->Count();
	
	if ((int)calls.size() != target_size)
		EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromMS): target genome vector has size " << target_size << " but " << calls.size() << " call lines found." << EidosTerminate();
	
	// Instantiate the mutations; NOTE THAT THE STACKING POLICY IS NOT CHECKED HERE, AS THIS IS NOT CONSIDERED THE ADDITION OF A MUTATION!
	std::vector<MutationIndex> mutation_indices;
	
	for (int mut_index = 0; mut_index < segsites; ++mut_index)
	{
		slim_position_t position = positions[mut_index];
		double selection_coeff = mutation_type_ptr->DrawSelectionCoefficient();
		slim_objectid_t subpop_index = -1;
		slim_generation_t origin_generation = sim.Generation();
		int8_t nucleotide = -1;
		
		if (nucleotide_based && mutation_type_ptr->nucleotide_based_)
		{
			// select a nucleotide that is different from the ancestral state at this position
			int8_t ancestral = (int8_t)sim.TheChromosome().AncestralSequence()->NucleotideAtIndex(position);
			
			nucleotide = (int8_t)Eidos_rng_uniform_int(EIDOS_GSL_RNG, 3);	// 0, 1, 2
			
			if (nucleotide == ancestral)
				nucleotide++;
		}
		
		MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
		
		new (gSLiM_Mutation_Block + new_mut_index) Mutation(mutation_type_ptr, position, selection_coeff, subpop_index, origin_generation, nucleotide);
		
		// This mutation type might not be used by any genomic element type (i.e. might not already be vetted), so we need to check and set pure_neutral_
		if (selection_coeff != 0.0)
		{
			sim.pure_neutral_ = false;
			
			// Fix all_pure_neutral_DFE_ if the selcoeff was not drawn from the muttype's DFE
			if (p_method_id == gID_addNewMutation)
				mutation_type_ptr->all_pure_neutral_DFE_ = false;
		}
		
		// add it to our local map, so we can find it when making genomes, and to the population's mutation registry
		pop.mutation_registry_.emplace_back(new_mut_index);
		mutation_indices.emplace_back(new_mut_index);
		
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
		if (pop.keeping_muttype_registries_ && mutation_type_ptr->keeping_muttype_registry_)
		{
			// This mutation type is also keeping its own private registry, so we need to add to that as well
			mutation_type_ptr->muttype_registry_.emplace_back(new_mut_index);
		}
#endif
	}
	
	// Sort the mutations by position so we can add them in order, and make an "order" vector for accessing calls in the sorted order
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	std::vector<int64_t> order_vec = EidosSortIndexes(positions);
	
	std::sort(mutation_indices.begin(), mutation_indices.end(), [mut_block_ptr](MutationIndex i1, MutationIndex i2) {return (mut_block_ptr + i1)->position_ < (mut_block_ptr + i2)->position_;});
	
	// Add the mutations to the target genomes, recording a new derived state with each addition
	for (int genome_index = 0; genome_index < target_size; ++genome_index)
	{
		Genome *genome = (Genome *)p_target->ObjectElementAtIndex(genome_index, nullptr);
		bool genome_started_empty = (genome->mutation_count() == 0);
		slim_position_t mutrun_length = genome->mutrun_length_;
		slim_mutrun_index_t current_run_index = -1;
		std::string &genome_string = calls[genome_index];
		
		for (int segsite_index = 0; segsite_index < segsites; ++segsite_index)
		{
			int64_t call_index = order_vec[segsite_index];
			char call = genome_string[call_index];
			
			if (call == '1')
			{
				MutationIndex mut_index = mutation_indices[segsite_index];
				Mutation *mut = mut_block_ptr + mut_index;
				slim_position_t mut_pos = mut->position_;
				slim_mutrun_index_t mut_mutrun_index = (slim_mutrun_index_t)(mut_pos / mutrun_length);
				
				if (mut_mutrun_index != current_run_index)
					genome->WillModifyRun(mut_mutrun_index);
				
				MutationRun *mut_mutrun = genome->mutruns_[mut_mutrun_index].get();
				
				// If the genome started empty, we can add mutations to the end with emplace_back(); if it did not, then they need to be inserted
				if (genome_started_empty)
					mut_mutrun->emplace_back(mut_index);
				else
					mut_mutrun->insert_sorted_mutation(mut_index);
				
				if (recording_mutations)
					sim.RecordNewDerivedState(genome, mut_pos, *genome->derived_mutation_ids_at_position(mut_pos));
			}
		}
	}
	
	// Return the instantiated mutations
	EidosValue_Object_vector_SP retval(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
	
	for (MutationIndex mut_index : mutation_indices)
		retval->push_object_element(mut_block_ptr + mut_index);
	
	return retval;
}

//	*********************	+ (o<Mutation>)readFromVCF(s$ filePath = NULL, [Nio<MutationType> mutationType = NULL])
//
EidosValue_SP Genome_Class::ExecuteMethod_readFromVCF(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_argument_count, p_interpreter)
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *mutationType_value = p_arguments[1].get();
	
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	Population &pop = sim.ThePopulation();
	slim_position_t last_position = sim.TheChromosome().last_position_;
	bool recording_mutations = sim.RecordingTreeSequenceMutations();
	bool nucleotide_based = sim.IsNucleotideBased();
	std::string file_path = Eidos_ResolvedPath(Eidos_StripTrailingSlash(filePath_value->StringAtIndex(0, nullptr)));
	MutationType *default_mutation_type_ptr = nullptr;
	
	if (mutationType_value->Type() != EidosValueType::kValueNULL)
		default_mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutationType_value, 0, sim, "readFromVCF()");
	
	// Parse the whole input file and retain the information from it
	std::ifstream infile(file_path);
	std::string line, sub;
	int parse_state = 0;
	int sample_id_count = 0;
	int target_size = p_target->Count();
	bool info_MID_defined = false, info_S_defined = false, info_DOM_defined = false, info_PO_defined = false;
	bool info_GO_defined = false, info_MT_defined = false, info_AA_defined = false, info_NONNUC_defined = false;
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
					if (line == "##INFO=<ID=MT,Number=.,Type=Integer,Description=\"Mutation Type\">")			info_MT_defined = true;
					if (line == "##INFO=<ID=AA,Number=1,Type=String,Description=\"Ancestral Allele\">")			info_AA_defined = true;		// this one is standard, so we don't require this definition
					if (line == "##INFO=<ID=NONNUC,Number=0,Type=Flag,Description=\"Non-nucleotide-based\">")	info_NONNUC_defined = true;
				}
				else if (line.compare(0, 1, "#") == 0)
				{
					static const char *(header_fields[9]) = {"CHROM", "POS", "ID", "REF", "ALT", "QUAL", "FILTER", "INFO", "FORMAT"};
					std::istringstream iss(line);
					
					iss.get();	// eat the initial #
					
					// verify that the expected standard columns are present
					for (int header_index = 0; header_index < 9; ++header_index)
					{
						if (iss.eof())
							EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): missing VCF header '" << header_fields[header_index] << "'." << EidosTerminate();
						iss >> sub;
						if (sub != header_fields[header_index])
							EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): expected VCF header '" << header_fields[header_index] << "', saw '" << sub << "'." << EidosTerminate();
					}
					
					// the remaining columns are sample IDs; we don't care what they are, we just count them
					while (!iss.eof())
					{
						iss >> sub;
						sample_id_count++;
					}
					
					// now the remainder of the file should be call lines
					parse_state = 1;
				}
				else
				{
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): unexpected line in VCF header: '" << line << "'." << EidosTerminate();
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
				std::getline(iss, sub, '\t');	// POS
				
				int64_t pos = EidosInterpreter::NonnegativeIntegerForString(sub, nullptr) - 1;		// -1 because VCF uses 1-based positions
				
				if ((pos < 0) || (pos > last_position))
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file POS value " << pos << " out of range." << EidosTerminate();
				
				call_lines.push_back(std::pair<slim_position_t, std::string>(pos, line));
				break;
			}
		}
	}
	
	infile.close();
	
	// sort the call lines by position, so that we can add them to empty genomes efficiently
	std::sort(call_lines.begin(), call_lines.end(), [ ](const std::pair<slim_position_t, std::string> &l1, const std::pair<slim_position_t, std::string> &l2) {return l1.first < l2.first;});
	
	// cache target genomes and determine whether they are initially empty, in which case we can do fast mutation addition with emplace_back()
	std::vector<Genome *> targets;
	std::vector<slim_mutrun_index_t> target_last_mutrun_modified;
	bool all_target_genomes_started_empty = true;
	
	for (int genome_index = 0; genome_index < target_size; ++genome_index)
	{
		Genome *genome = (Genome *)p_target->ObjectElementAtIndex(genome_index, nullptr);
		
		// null genomes are silently excluded from the target list, for convenience
		if (!genome->IsNull())
		{
			if (genome->mutation_count() != 0)
				all_target_genomes_started_empty = false;
			
			targets.push_back(genome);
			target_last_mutrun_modified.push_back(-1);
		}
	}
	
	target_size = (int)targets.size();	// adjust for possible exclusion of null genomes
	
	// parse all the call lines, instantiate their mutations, and add the mutations to the target genomes
	std::vector<MutationIndex> mutation_indices;
	bool has_initial_mutations = (gSLiM_next_mutation_id != 0);
	
	for (std::pair<slim_position_t, std::string> &call_line : call_lines)
	{
		slim_position_t mut_position = call_line.first;
		std::istringstream iss(call_line.second);
		std::string ref_str, alt_str, info_str;
		
		std::getline(iss, sub, '\t');		// CHROM; don't care
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
		else						EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file REF value must be A/C/G/T." << EidosTerminate();
		
		// parse/validate the ALT nucleotides
		std::vector<std::string> alt_substrs = Eidos_string_split(alt_str, ",");
		std::vector<int8_t> alt_nucs;
		
		for (std::string &alt_substr : alt_substrs)
		{
			if (alt_substr == "A")			alt_nucs.push_back(0);
			else if (alt_substr == "C")		alt_nucs.push_back(1);
			else if (alt_substr == "G")		alt_nucs.push_back(2);
			else if (alt_substr == "T")		alt_nucs.push_back(3);
			else							EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file ALT value must be A/C/G/T." << EidosTerminate();
		}
		
		std::size_t alt_allele_count = alt_nucs.size();
		
		// parse/validate the INFO fields that we recognize
		std::vector<std::string> info_substrs = Eidos_string_split(info_str, ";");
		std::vector<slim_mutationid_t> info_mutids;
		std::vector<double> info_selcoeffs;
		std::vector<double> info_domcoeffs;
		std::vector<slim_objectid_t> info_poporigin;
		std::vector<slim_generation_t> info_genorigin;
		std::vector<slim_objectid_t> info_muttype;
		int8_t info_ancestral_nuc = -1;
		bool info_is_nonnuc = false;
		
		for (std::string &info_substr : info_substrs)
		{
			if (info_MID_defined && (info_substr.compare(0, 4, "MID=") == 0))		// Mutation ID
			{
				std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(4), ",");
				
				for (std::string &value_substr : value_substrs)
					info_mutids.push_back((slim_mutationid_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
				
				if (info_mutids.size() && has_initial_mutations)
				{
					if (!gEidosSuppressWarnings)
					{
						if (!sim.warned_readFromVCF_mutIDs_unused_)
						{
							p_interpreter.ExecutionOutputStream() << "#WARNING (Genome_Class::ExecuteMethod_readFromVCF): readFromVCF(): the VCF file specifies mutation IDs with the MID field, but some mutation IDs have already been used so uniqueness cannot be guaranteed.  Use of mutation IDs is therefore disabled; mutations will not receive the mutation ID requested in the file.  To fix this warning, remove the MID field from the VCF file before reading.  To get readFromVCF() to use the specified mutation IDs, load the VCF file into a model that has never simulated a mutation, and has therefore not used any mutation iDs." << std::endl;
							sim.warned_readFromVCF_mutIDs_unused_ = true;
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
					info_selcoeffs.push_back(EidosInterpreter::FloatForString(value_substr, nullptr));
			}
			else if (info_DOM_defined && (info_substr.compare(0, 4, "DOM=") == 0))	// Dominance Coefficient
			{
				std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(4), ",");
				
				for (std::string &value_substr : value_substrs)
					info_domcoeffs.push_back(EidosInterpreter::FloatForString(value_substr, nullptr));
			}
			else if (info_PO_defined && (info_substr.compare(0, 3, "PO=") == 0))	// Population of Origin
			{
				std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(3), ",");
				
				for (std::string &value_substr : value_substrs)
					info_poporigin.push_back((slim_objectid_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
			}
			else if (info_GO_defined && (info_substr.compare(0, 3, "GO=") == 0))	// Generation of Origin
			{
				std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(3), ",");
				
				for (std::string &value_substr : value_substrs)
					info_genorigin.push_back((slim_generation_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
			}
			else if (info_MT_defined && (info_substr.compare(0, 3, "MT=") == 0))	// Mutation Type
			{
				std::vector<std::string> value_substrs = Eidos_string_split(info_substr.substr(3), ",");
				
				for (std::string &value_substr : value_substrs)
					info_muttype.push_back((slim_objectid_t)EidosInterpreter::NonnegativeIntegerForString(value_substr, nullptr));
			}
			else if (/* info_AA_defined && */ (info_substr.compare(0, 3, "AA=") == 0))	// Ancestral Allele; definition not required since it is a standard field
			{
				std::string aa_str = info_substr.substr(3);
				
				if (aa_str == "A")			info_ancestral_nuc = 0;
				else if (aa_str == "C")		info_ancestral_nuc = 1;
				else if (aa_str == "G")		info_ancestral_nuc = 2;
				else if (aa_str == "T")		info_ancestral_nuc = 3;
				else						EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file AA value must be A/C/G/T." << EidosTerminate();
			}
			else if (info_NONNUC_defined && (info_substr == "NONNUC"))				// Non-nucleotide-based
			{
				info_is_nonnuc = true;
			}
			
			if ((info_mutids.size() != 0) && (info_mutids.size() != alt_allele_count))
				EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file unexpected value count for MID field." << EidosTerminate();
			if ((info_selcoeffs.size() != 0) && (info_selcoeffs.size() != alt_allele_count))
				EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file unexpected value count for S field." << EidosTerminate();
			if ((info_domcoeffs.size() != 0) && (info_domcoeffs.size() != alt_allele_count))
				EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file unexpected value count for DOM field." << EidosTerminate();
			if ((info_poporigin.size() != 0) && (info_poporigin.size() != alt_allele_count))
				EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file unexpected value count for PO field." << EidosTerminate();
			if ((info_genorigin.size() != 0) && (info_genorigin.size() != alt_allele_count))
				EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file unexpected value count for GO field." << EidosTerminate();
			if ((info_muttype.size() != 0) && (info_muttype.size() != alt_allele_count))
				EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file unexpected value count for MT field." << EidosTerminate();
		}
		
		// read the genotype data for each sample id, which might be diploid or haploid, and might have data beyond GT
		std::vector<int> genotype_calls;
		
		for (int sample_index = 0; sample_index < sample_id_count; ++sample_index)
		{
			if (iss.eof())
				EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file call line ended unexpectly before the last sample." << EidosTerminate();
			
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
						EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file call out of range (does not correspond to a REF or ALT allele in the call line)." << EidosTerminate();
					
					genotype_calls.push_back(genotype_call1);
					genotype_calls.push_back(genotype_call2);
					call_handled = true;
				}
			}
			else if (sub.length() == 1)
			{
				// haploid, single-digit
				char sub_ch = sub[0];
				
				if ((sub_ch >= '0') && (sub_ch <= '9'))
				{
					int genotype_call = (int)(sub_ch - '0');
					
					if ((genotype_call < 0) || (genotype_call > (int)alt_allele_count))	// 0 is REF, 1..n are ALT alleles
						EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file call out of range (does not correspond to a REF or ALT allele in the call line)." << EidosTerminate();
					
					genotype_calls.push_back(genotype_call);
					call_handled = true;
				}
			}
			
			if (!call_handled)
			{
				std::vector<std::string> genotype_substrs;
				
				if (sub.find("|") != std::string::npos)
					genotype_substrs = Eidos_string_split(sub, "|");	// phased
				else if (sub.find("/") != std::string::npos)
					genotype_substrs = Eidos_string_split(sub, "/");	// unphased; we don't worry about that
				else
					genotype_substrs.push_back(sub);					// haploid, presumably
				
				if ((genotype_substrs.size() < 1) || (genotype_substrs.size() > 2))
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file genotype calls must be diploid or haploid; " << genotype_substrs.size() << " calls found in one sample." << EidosTerminate();
				
				// extract the calls' integer values, validate them, and keep them; we don't care which call was in which sample, we just preserve their order
				for (std::string &genotype_substr : genotype_substrs)
				{
					std::size_t genotype_call = EidosInterpreter::NonnegativeIntegerForString(genotype_substr, nullptr);
					
					if ((genotype_call < 0) || (genotype_call > alt_allele_count))	// 0 is REF, 1..n are ALT alleles
						EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file call out of range (does not correspond to a REF or ALT allele in the call line)." << EidosTerminate();
					
					genotype_calls.push_back((int)genotype_call);
				}
			}
		}
		
		if (!iss.eof())
			EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file call line has unexpected entries following the last sample." << EidosTerminate();
		if ((int)genotype_calls.size() != target_size)
			EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): target genome vector has size " << target_size << " but " << genotype_calls.size() << " calls were found in one call line." << EidosTerminate();
		
		// instantiate the mutations involved in this call line; the REF allele represents no mutation, ALT alleles are each separate mutations
		std::vector<MutationIndex> alt_allele_mut_indices;
		
		for (std::size_t alt_allele_index = 0; alt_allele_index < alt_allele_count; ++alt_allele_index)
		{
			// figure out the mutation type; if specified with MT, look it up, otherwise use the default supplied
			MutationType *mutation_type_ptr = default_mutation_type_ptr;
			
			if (info_muttype.size() > 0)
			{
				slim_objectid_t mutation_type_id = info_muttype[alt_allele_index];
				
				auto found_muttype_pair = sim.MutationTypes().find(mutation_type_id);
				
				if (found_muttype_pair == sim.MutationTypes().end())
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file MT field references a mutation type m" << mutation_type_id << " that is not defined." << EidosTerminate();
				
				mutation_type_ptr = found_muttype_pair->second;
			}
			
			if (!mutation_type_ptr)
				EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file MT field missing, but no default mutation type was supplied in the mutationType parameter." << EidosTerminate();
			
			// check the dominance coefficient of DOM against that of the mutation type
			if (info_domcoeffs.size() > 0)
			{
				if (abs(info_domcoeffs[alt_allele_index] - mutation_type_ptr->dominance_coeff_) > 0.0001)
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): VCF file DOM field specifies a dominance coefficient " << info_domcoeffs[alt_allele_index] << " that differs from the mutation type's dominance coefficient of " << mutation_type_ptr->dominance_coeff_ << "." << EidosTerminate();
			}
			
			// get the selection coefficient from S, or draw one
			double selection_coeff;
			
			if (info_selcoeffs.size() > 0)
				selection_coeff = info_selcoeffs[alt_allele_index];
			else
				selection_coeff = mutation_type_ptr->DrawSelectionCoefficient();
			
			// get the subpop index from PO, or set to -1; no bounds checking on this
			slim_objectid_t subpop_index = -1;
			
			if (info_poporigin.size() > 0)
				subpop_index = info_poporigin[alt_allele_index];
			
			// get the origin generation from gO, or set to the current generation; no bounds checking on this
			slim_generation_t origin_generation;
			
			if (info_genorigin.size() > 0)
				origin_generation = info_genorigin[alt_allele_index];
			else
				origin_generation = sim.Generation();
			
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
							EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): a mutation marked NONNUC cannot use a nucleotide-based mutation type." << EidosTerminate();
						
						nucleotide = -1;
					}
					else
					{
						// This call line is not marked NONNUC, so it represents nucleotide-based alleles
						if (!mutation_type_ptr->nucleotide_based_)
							EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): a nucleotide-based mutation cannot use a non-nucleotide-based mutation type." << EidosTerminate();
						if (ref_nuc != info_ancestral_nuc)
							EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): the REF nucleotide does not match the AA nucleotide." << EidosTerminate();
						
						int8_t ancestral = (int8_t)sim.TheChromosome().AncestralSequence()->NucleotideAtIndex(mut_position);
						
						if (ancestral != ref_nuc)
							EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): the REF/AA nucleotide does not match the ancestral nucleotide at the same position; a matching ancestral nucleotide sequence must be set prior to calling readFromVCF()." << EidosTerminate();
						
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
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_readFromVCF): cannot read a VCF file generated by a nucleotide-based model into a non-nucleotide-based model." << EidosTerminate();
				
				nucleotide = -1;
			}
			
			// instantiate the mutation with the values decided upon
			MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
			
			if (info_mutids.size() > 0)
			{
				// a mutation ID was supplied; we use it blindly, having checked above that we are in the case where this is legal
				slim_mutationid_t mut_mutid = info_mutids[alt_allele_index];
				
				new (gSLiM_Mutation_Block + new_mut_index) Mutation(mut_mutid, mutation_type_ptr, mut_position, selection_coeff, subpop_index, origin_generation, nucleotide);
			}
			else
			{
				// no mutation ID supplied, so use whatever is next
				new (gSLiM_Mutation_Block + new_mut_index) Mutation(mutation_type_ptr, mut_position, selection_coeff, subpop_index, origin_generation, nucleotide);
			}
			
			// This mutation type might not be used by any genomic element type (i.e. might not already be vetted), so we need to check and set pure_neutral_
			if (selection_coeff != 0.0)
			{
				sim.pure_neutral_ = false;
				
				// Fix all_pure_neutral_DFE_ if the selcoeff was not drawn from the muttype's DFE
				if (p_method_id == gID_addNewMutation)
					mutation_type_ptr->all_pure_neutral_DFE_ = false;
			}
			
			// add it to our local map, so we can find it when making genomes, and to the population's mutation registry
			pop.mutation_registry_.emplace_back(new_mut_index);
			alt_allele_mut_indices.push_back(new_mut_index);
			mutation_indices.push_back(new_mut_index);
			
#ifdef SLIM_KEEP_MUTTYPE_REGISTRIES
			if (pop.keeping_muttype_registries_ && mutation_type_ptr->keeping_muttype_registry_)
			{
				// This mutation type is also keeping its own private registry, so we need to add to that as well
				mutation_type_ptr->muttype_registry_.emplace_back(new_mut_index);
			}
#endif
		}
		
		// add the mutations to the appropriate genomes and record the new derived states
		for (int genome_index = 0; genome_index < target_size; ++genome_index)
		{
			int call = genotype_calls[genome_index];
			
			if (call != 0)
			{
				Genome *genome = targets[genome_index];
				slim_mutrun_index_t &genome_last_mutrun_modified = target_last_mutrun_modified[genome_index];
				slim_position_t mutrun_length = genome->mutrun_length_;
				MutationIndex mut_index = alt_allele_mut_indices[call - 1];
				slim_mutrun_index_t mut_mutrun_index = (slim_mutrun_index_t)(mut_position / mutrun_length);
				
				if (mut_mutrun_index != genome_last_mutrun_modified)
				{
					genome->WillModifyRun(mut_mutrun_index);
					genome_last_mutrun_modified = mut_mutrun_index;
				}
				
				MutationRun *mut_mutrun = genome->mutruns_[mut_mutrun_index].get();
				
				// If the genome started empty, we can add mutations to the end with emplace_back(); if it did not, then they need to be inserted
				if (all_target_genomes_started_empty)
					mut_mutrun->emplace_back(mut_index);
				else
					mut_mutrun->insert_sorted_mutation(mut_index);
				
				if (recording_mutations)
					sim.RecordNewDerivedState(genome, mut_position, *genome->derived_mutation_ids_at_position(mut_position));
			}
		}
	}
	
	// Return the instantiated mutations
	EidosValue_Object_vector_SP retval(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	
	for (MutationIndex mut_index : mutation_indices)
		retval->push_object_element(mut_block_ptr + mut_index);
	
	return retval;
}

//	*********************	+ (void)removeMutations([No<Mutation> mutations = NULL], [logical$ substitute = F])
//
EidosValue_SP Genome_Class::ExecuteMethod_removeMutations(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutations_value = p_arguments[0].get();
	EidosValue *substitute_value = p_arguments[1].get();
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	int target_size = p_target->Count();
	
	if (target_size == 0)
		return gStaticEidosValueVOID;
	
	// Use the 0th genome in the target to find out what the mutation run length is, so we can calculate run indices
	Genome *genome_0 = (Genome *)p_target->ObjectElementAtIndex(0, nullptr);
	slim_position_t mutrun_length = genome_0->mutrun_length_;
	SLiMSim &sim = genome_0->subpop_->population_.sim_;
	Population &pop = sim.ThePopulation();
	slim_generation_t generation = sim.Generation();
	bool create_substitutions = substitute_value->LogicalAtIndex(0, nullptr);
	bool recording_tree_sequence_mutations = sim.RecordingTreeSequenceMutations();
	bool any_nonneutral_removed = false;
	
	if (pop.sim_.executing_block_type_ == SLiMEidosBlockType::SLiMEidosModifyChildCallback)
	{
		// Check that we're not inside a modifyChild() callback, or if we are, that the only genomes being modified belong to the new child.
		// This prevents problems with retracting the proposed child when tree-sequence recording is enabled; other extraneous changes must
		// not be backed out, and it's hard to separate, e.g., what's a child-related new mutation from an extraneously added new mutation.
		// Note that the other Genome methods that add/remove mutations perform the same check, and should be maintained in parallel.
		Individual *focal_modification_child = pop.sim_.focal_modification_child_;
		
		if (focal_modification_child)
		{
			Genome *focal_genome_1 = focal_modification_child->genome1_, *focal_genome_2 = focal_modification_child->genome2_;
			
			for (int genome_index = 0; genome_index < target_size; ++genome_index)
			{
				Genome *target_genome = (Genome *)p_target->ObjectElementAtIndex(genome_index, nullptr);
				
				if ((target_genome != focal_genome_1) && (target_genome != focal_genome_2))
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_removeMutations): removeMutations() cannot be called from within a modifyChild() callback to modify any genomes except those of the focal child being generated." << EidosTerminate();
			}
		}
		
		// This is actually only a problem when tree recording is on, but for consistency we outlaw it in all cases.  When a substitution
		// is created, it is added to the derived state of every genome, which is a side effect that can't be retracted if the modifyChild()
		// callback rejects the proposed child, so it has to be prohibited up front.  Anyway it would be a very strange thing to do.
		if (create_substitutions)
			EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_removeMutations): removeMutations() cannot be called from within a modifyChild() callback to create a substitution, because that would have side effects on genomes other than those of the focal child being generated." << EidosTerminate();
	}
	else if ((pop.sim_.executing_block_type_ == SLiMEidosBlockType::SLiMEidosRecombinationCallback) ||
			 (pop.sim_.executing_block_type_ == SLiMEidosBlockType::SLiMEidosMutationCallback))
		EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_removeMutations): removeMutations() cannot be called from within a recombination() or mutation() callback." << EidosTerminate();
	
	if (mutations_value->Type() == EidosValueType::kValueNULL)
	{
		// This is the "remove all mutations" case, for which we have no vector of mutations to remove.  In this case we do the tree
		// sequence recording first, since we can add new empty derived states just at the locations where mutations presently exist.
		// Then we just go through and clear out the target genomes.
		if (create_substitutions)
			EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_removeMutations): in removeMutations() substitute may not be T if mutations is NULL; an explicit vector of mutations to be substituted must be supplied." << EidosTerminate();
		
		// TREE SEQUENCE RECORDING
		if (recording_tree_sequence_mutations)
		{
			const std::vector<Mutation *> empty_mut_vector;
			
			for (int genome_index = 0; genome_index < target_size; ++genome_index)
			{
				Genome *target_genome = (Genome *)p_target->ObjectElementAtIndex(genome_index, nullptr);
				
				if (target_genome->IsNull())
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_removeMutations): removeMutations() cannot be called on a null genome." << EidosTerminate();
				
				for (GenomeWalker target_walker(target_genome); !target_walker.Finished(); target_walker.NextMutation())
				{
					Mutation *mut = target_walker.CurrentMutation();
					slim_position_t pos = mut->position_;
					
					sim.RecordNewDerivedState(target_genome, pos, empty_mut_vector);
				}
			}
		}
		
		// Now remove all mutations; we don't use bulk operations here because it is simpler to just set them all to the same empty run
		MutationRun *shared_empty_run = MutationRun::NewMutationRun();
		int mutrun_count = genome_0->mutrun_count_;
		
		for (int genome_index = 0; genome_index < target_size; ++genome_index)
		{
			Genome *target_genome = (Genome *)p_target->ObjectElementAtIndex(genome_index, nullptr);
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
					target_genome->mutruns_[run_index].reset(shared_empty_run);
		}
		
		// invalidate cached mutation refcounts; refcounts have changed
		pop.cached_tally_genome_count_ = 0;
		
		// in this code path we just assume that nonneutral mutations might have been removed
		any_nonneutral_removed = true;
	}
	else
	{
		// Construct a vector of mutations to remove that is sorted by position
		int mutations_count = mutations_value->Count();
		std::vector<Mutation *> mutations_to_remove;
		
		for (int value_index = 0; value_index < mutations_count; ++value_index)
		{
			Mutation *mut = (Mutation *)mutations_value->ObjectElementAtIndex(value_index, nullptr);
			
			mutations_to_remove.push_back(mut);
			
			if (mut->selection_coeff_ != 0.0)
				any_nonneutral_removed = true;
		}
		
		std::sort(mutations_to_remove.begin(), mutations_to_remove.end(), [ ](Mutation *i1, Mutation *i2) {return i1->position_ < i2->position_;});
		
		// TREE SEQUENCE RECORDING
		// First, pre-plan the positions of new tree-seq derived states in anticipation of doing the removal.  We have to check
		// whether the mutation being added is already absent, to avoid recording a new derived state identical to the old one state.
		// The algorithm used here, with GenomeWalker, depends upon the fact that we just sorted the mutations to add by position.
		// However, we do still have to think about multiple muts being removed from the same position, and existing stacked mutations.
		// Note that when we are not creating substitutions, the genomes that possess a given mutation are the ones that change; but
		// when we are creating substitutions, the genomes that do *not* possess a given mutation are the ones that change, because
		// they gain the substitutuon, whereas the other genomes lose the mutation and gain the substitution, and are thus unchanged.
		std::vector<std::pair<Genome *, std::vector<slim_position_t>>> new_derived_state_positions;
		
		if (recording_tree_sequence_mutations)
		{
			for (int genome_index = 0; genome_index < target_size; ++genome_index)
			{
				Genome *target_genome = (Genome *)p_target->ObjectElementAtIndex(genome_index, nullptr);
				GenomeWalker walker(target_genome);
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
						// no pair entry in new_derived_state_positions yet, so make a new pair entry for this genome
						new_derived_state_positions.emplace_back(std::pair<Genome *, std::vector<slim_position_t>>(target_genome, std::vector<slim_position_t>(1, mut_pos)));
					}
					else
					{
						// we have an existing pair entry for this genome, so add this position to its position vector
						std::pair<Genome *, std::vector<slim_position_t>> &genome_entry = new_derived_state_positions.back();
						std::vector<slim_position_t> &genome_list = genome_entry.second;
						
						genome_list.push_back(mut_pos);
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
				Mutation *mut = (Mutation *)mutations_value->ObjectElementAtIndex(value_index, nullptr);
				Substitution *sub = new Substitution(*mut, generation);
				
				// TREE SEQUENCE RECORDING
				// When doing tree recording, we additionally keep all fixed mutations (their ids) in a multimap indexed by their position
				// This allows us to find all the fixed mutations at a given position quickly and easily, for calculating derived states
				if (sim.RecordingTreeSequence())
					pop.treeseq_substitutions_map_.insert(std::pair<slim_position_t, Substitution *>(mut->position_, sub));
				
				pop.substitutions_.emplace_back(sub);
			}
			
			// TREE SEQUENCE RECORDING
			// When doing tree recording, if a given mutation is converted to a substitution by script, it may not be contained in some
			// genomes, but at the moment it fixes it is then considered to belong to the derived state of every non-null genome.  We
			// therefore need to go through all of the non-target genomes and record their new derived states at all positions
			// containing a new substitution.  If they already had a copy of the mutation, and didn't have it removed here, that is
			// pretty weird, but in practice it just means that their derived state will contain that mutation id twice – once for the
			// segregating mutation they still contain, and once for the new substitution.  We don't check for that case, but it should
			// just work automatically.
			if (recording_tree_sequence_mutations)
			{
				// Mark all non-null genomes in the simulation that are not among the target genomes; we use patch_pointer_ as scratch
				for (auto subpop_pair : sim.ThePopulation().subpops_)
					for (Genome *genome : subpop_pair.second->parent_genomes_)
						genome->patch_pointer_ = (genome->IsNull() ? nullptr : genome);
				
				for (int genome_index = 0; genome_index < target_size; ++genome_index)
					((Genome *)p_target->ObjectElementAtIndex(genome_index, nullptr))->patch_pointer_ = nullptr;
				
				// Figure out the unique chromosome positions that have changed (the uniqued set of mutation positions)
				std::vector<slim_position_t> unique_positions;
				slim_position_t last_pos = -1;
				
				for (Mutation *mut : mutations_to_remove)
				{
					slim_position_t pos = mut->position_;
					
					if (pos != last_pos)
					{
						unique_positions.push_back(pos);
						last_pos = pos;
					}
				}
				
				// Loop through those genomes and log the new derived state at each (unique) position
				for (auto subpop_pair : sim.ThePopulation().subpops_)
					for (Genome *genome : subpop_pair.second->parent_genomes_)
						if (genome->patch_pointer_)
						{
							for (slim_position_t position : unique_positions)
								sim.RecordNewDerivedState(genome, position, *genome->derived_mutation_ids_at_position(position));
							genome->patch_pointer_ = nullptr;
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
			int64_t operation_id = ++gSLiM_MutationRun_OperationID;
			
			Genome::BulkOperationStart(operation_id, mutrun_index);
			
			for (int genome_index = 0; genome_index < target_size; ++genome_index)
			{
				Genome *target_genome = (Genome *)p_target->ObjectElementAtIndex(genome_index, nullptr);
				
				if (target_genome->IsNull())
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_removeMutations): removeMutations() cannot be called on a null genome." << EidosTerminate();
				
				// See if WillModifyRunForBulkOperation() can short-circuit the operation for us
				if (target_genome->WillModifyRunForBulkOperation(operation_id, mutrun_index))
				{
					// Remove the specified mutations; see RemoveFixedMutations for the origins of this code
					MutationRun *mutrun = target_genome->mutruns_[mutrun_index].get();
					MutationIndex *genome_iter = mutrun->begin_pointer();
					MutationIndex *genome_backfill_iter = mutrun->begin_pointer();
					MutationIndex *genome_max = mutrun->end_pointer();
					
					// genome_iter advances through the mutation list; for each entry it hits, the entry is either removed (skip it) or not removed (copy it backward to the backfill pointer)
					while (genome_iter != genome_max)
					{
						MutationIndex candidate_mutation = *genome_iter;
						const slim_position_t candidate_pos = (mut_block_ptr + candidate_mutation)->position_;
						bool should_remove = false;
						
						for (int mut_index = value_index; mut_index < mutations_count; ++mut_index)
						{
							Mutation *mut_to_remove = mutations_to_remove[mut_index];
							MutationIndex mut_to_remove_index = mut_to_remove->BlockIndex();
							
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
							++genome_iter;
						}
						else
						{
							// Unremoved mutation; we want to keep it, so we copy it backward and advance our backfill pointer as well as genome_iter
							if (genome_backfill_iter != genome_iter)
								*genome_backfill_iter = *genome_iter;
							
							++genome_backfill_iter;
							++genome_iter;
						}
					}
					
					// excess mutations at the end have been copied back already; we just adjust mutation_count_ and forget about them
					mutrun->set_size(mutrun->size() - (int)(genome_iter - genome_backfill_iter));
				}
			}
			
			Genome::BulkOperationEnd(operation_id, mutrun_index);
			
			// now we have handled all mutations at this index (and all previous indices)
			last_handled_mutrun_index = mutrun_index;
			
			// invalidate cached mutation refcounts; refcounts have changed
			pop.cached_tally_genome_count_ = 0;
		}
		
		// TREE SEQUENCE RECORDING
		// Now that all the bulk operations are done, record all the new derived states
		if (recording_tree_sequence_mutations)
		{
			for (std::pair<Genome *, std::vector<slim_position_t>> &genome_pair : new_derived_state_positions)
			{
				Genome *target_genome = genome_pair.first;
				std::vector<slim_position_t> &genome_positions = genome_pair.second;
				
				for (slim_position_t position : genome_positions)
					sim.RecordNewDerivedState(target_genome, position, *target_genome->derived_mutation_ids_at_position(position));
			}
		}
	}
	
	// issue a warning if removeMutations() was called at a questionable time, but only if the mutations removed were non-neutral
	if (any_nonneutral_removed && !sim.warned_early_mutation_remove_)
	{
		if (sim.GenerationStage() == SLiMGenerationStage::kWFStage1ExecuteEarlyScripts)
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (Genome_Class::ExecuteMethod_removeMutations): removeMutations() should probably not be called from an early() event in a WF model; the removed mutation(s) will still influence fitness values during offspring generation." << std::endl;
				sim.warned_early_mutation_remove_ = true;
			}
		}
		if (sim.GenerationStage() == SLiMGenerationStage::kNonWFStage6ExecuteLateScripts)
		{
			if (!gEidosSuppressWarnings)
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (Genome_Class::ExecuteMethod_removeMutations): removeMutations() should probably not be called from an late() event in a nonWF model; the removed mutation(s) will still influence fitness values until partway through the next generation." << std::endl;
				sim.warned_early_mutation_remove_ = true;
			}
		}
	}
	
	return gStaticEidosValueVOID;
}


//
//	GenomeWalker
//
#pragma mark -
#pragma mark GenomeWalker
#pragma mark -

void GenomeWalker::NextMutation(void)
{
	if (++mutrun_ptr_ >= mutrun_end_)
	{
		// finished the current mutation, so move to the next until we find a mutation
		do
		{
			if (++mutrun_index_ >= genome_->mutrun_count_)
			{
				// finished all mutation runs, so we're done
				mutation_ = nullptr;
				return;
			}
			
			MutationRun *mutrun = genome_->mutruns_[mutrun_index_].get();
			mutrun_ptr_ = mutrun->begin_pointer_const();
			mutrun_end_ = mutrun->end_pointer_const();
		}
		while (mutrun_ptr_ == mutrun_end_);
	}
	
	mutation_ = gSLiM_Mutation_Block + *mutrun_ptr_;
}

void GenomeWalker::MoveToPosition(slim_position_t p_position)
{
	// Move the the first mutation that is at or after the given position.  Using this to move to a
	// given position is more efficient than iteratively advancing mutation by mutation, because
	// we can (1) go to the correct mutation run directly, and (2) do a binary search inside the
	// mutation run for the correct position.
	Genome *genome = genome_;
	
	// start at the mutrun dictated by the position we are moving to; positions < 0 start at 0
	mutrun_index_ = (int32_t)(p_position / genome->mutrun_length_);
	if (mutrun_index_ < 0)
		mutrun_index_ = 0;
	
	while (true)
	{
		// if the mutrun is past the end of the last mutrun, we're done
		if (mutrun_index_ >= genome->mutrun_count_)
		{
			mutation_ = nullptr;
			return;
		}
		
		// get the information on the mutrun
		MutationRun *mutrun = genome->mutruns_[mutrun_index_].get();
		mutrun_ptr_ = mutrun->begin_pointer_const();
		mutrun_end_ = mutrun->end_pointer_const();
		
		// if the mutrun is empty, we will need to move to the next mutrun to find a mutation
		if (mutrun_ptr_ == mutrun_end_)
			mutrun_index_++;
		else
			break;
	}
	
	// if the mutation found is at or after the requested position, we are already done
	mutation_ = gSLiM_Mutation_Block + *mutrun_ptr_;
	
	if (mutation_->position_ >= p_position)
		return;
	
	// otherwise, we are in the correct mutrun for the position, but the requested position
	// still lies ahead of us, so we have to advance until we reach or pass the position
	// FIXME should do a binary search inside the mutation run instead
	do
		NextMutation();
	while (!Finished() && (Position() < p_position));
}

bool GenomeWalker::MutationIsStackedAtCurrentPosition(Mutation *p_search_mut)
{
	// We have reached some chromosome position (presumptively the *first mutation* at that position,
	// which we do not check here), and the caller wants to know if a given mutation, which is
	// located at that position, is contained in this genome.  This requires that we look ahead
	// through all of the mutations at the current position, since they are not sorted.  We are
	// guaranteed to stay inside the current mutation run, though, so this lookahead is simple.
	if (Finished())
		EIDOS_TERMINATION << "ERROR (GenomeWalker::MutationIsStackedAtCurrentPosition): (internal error) MutationIsStackedAtCurrentPosition() called on a finished walker." << EidosTerminate();
	if (!p_search_mut)
		EIDOS_TERMINATION << "ERROR (GenomeWalker::MutationIsStackedAtCurrentPosition): (internal error) MutationIsStackedAtCurrentPosition() called with a nullptr mutation to search for." << EidosTerminate();
	
	slim_position_t pos = mutation_->position_;
	
	if (p_search_mut->position_ != pos)
		EIDOS_TERMINATION << "ERROR (GenomeWalker::MutationIsStackedAtCurrentPosition): (internal error) MutationIsStackedAtCurrentPosition() called with a mutation that is not at the current walker position." << EidosTerminate();
	
	for (const MutationIndex *search_ptr_ = mutrun_ptr_; search_ptr_ != mutrun_end_; ++search_ptr_)
	{
		MutationIndex mutindex = *search_ptr_;
		Mutation *mut = gSLiM_Mutation_Block + mutindex;
		
		if (mut == p_search_mut)
			return true;
		if (mut->position_ != pos)
			break;
	}
	
	return false;
}

bool GenomeWalker::IdenticalAtCurrentPositionTo(GenomeWalker &p_other_walker)
{
	if (Finished())
		EIDOS_TERMINATION << "ERROR (GenomeWalker::IdenticalAtCurrentPositionTo): (internal error) IdenticalAtCurrentPositionTo() called on a finished walker." << EidosTerminate();
	if (p_other_walker.Finished())
		EIDOS_TERMINATION << "ERROR (GenomeWalker::IdenticalAtCurrentPositionTo): (internal error) IdenticalAtCurrentPositionTo() called on a finished walker." << EidosTerminate();
	if (Position() != p_other_walker.Position())
		EIDOS_TERMINATION << "ERROR (GenomeWalker::IdenticalAtCurrentPositionTo): (internal error) IdenticalAtCurrentPositionTo() called with walkers at different positions." << EidosTerminate();
	
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
		Mutation *mut_1 = (search_ptr_1 != mutrun_end_) ? (gSLiM_Mutation_Block + *search_ptr_1) : nullptr;
		Mutation *mut_2 = (search_ptr_2 != p_other_walker.mutrun_end_) ? (gSLiM_Mutation_Block + *search_ptr_2) : nullptr;
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

int8_t GenomeWalker::NucleotideAtCurrentPosition(void)
{
	if (Finished())
		EIDOS_TERMINATION << "ERROR (GenomeWalker::NucleotideAtCurrentPosition): (internal error) NucleotideAtCurrentPosition() called on a finished walker." << EidosTerminate();
	
	// First check the mutation we're at, since we already have it
	int8_t nuc = mutation_->nucleotide_;
	
	if (nuc != -1)
		return nuc;
	
	// Then scan forward as long as we are still within the same position
	slim_position_t pos = mutation_->position_;
	
	for (const MutationIndex *search_ptr_ = mutrun_ptr_ + 1; search_ptr_ != mutrun_end_; ++search_ptr_)
	{
		MutationIndex mutindex = *search_ptr_;
		Mutation *mut = gSLiM_Mutation_Block + mutindex;
		
		if (mut->position_ != pos)
			return -1;
		
		nuc = mut->nucleotide_;
		
		if (nuc != -1)
			return nuc;
	}
	
	return -1;
}


































































