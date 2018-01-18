//
//  genome.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2017 Philipp Messer.  All rights reserved.
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
int Genome::s_bulk_operation_mutrun_index_ = -1;
std::unordered_map<MutationRun*, MutationRun*> Genome::s_bulk_operation_runs_;


Genome::Genome(Subpopulation *p_subpop, int p_mutrun_count, int p_mutrun_length, enum GenomeType p_genome_type_, bool p_is_null) : genome_type_(p_genome_type_), subpop_(p_subpop)
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

void Genome::WillModifyRun(int p_run_index)
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

void Genome::BulkOperationStart(int64_t p_operation_id, int p_mutrun_index)
{
	if (s_bulk_operation_id_ != 0)
	{
		//EIDOS_TERMINATION << "ERROR (Genome::BulkOperationStart): (internal error) unmatched bulk operation start." << EidosTerminate();
		
		// It would be nice to be able to throw an exception here, but in the present design, the
		// bulk operation info can get messed up if the bulk operation throws an exception that
		// blows through the call to Genome::BulkOperationEnd().
		std::cout << "WARNING (Genome::BulkOperationStart): (internal error) unmatched bulk operation start." << std::endl;
		
		// For now, we assume that the end call got blown through, and we close out the old operation.
		Genome::BulkOperationEnd(s_bulk_operation_id_, s_bulk_operation_mutrun_index_);
	}
	
	s_bulk_operation_id_ = p_operation_id;
	s_bulk_operation_mutrun_index_ = p_mutrun_index;
}

bool Genome::WillModifyRunForBulkOperation(int64_t p_operation_id, int p_mutrun_index)
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

void Genome::BulkOperationEnd(int64_t p_operation_id, int p_mutrun_index)
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
void Genome::RemoveFixedMutations(int64_t p_operation_id, int p_mutrun_index)
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

void Genome::ReinitializeGenomeToMutrun(GenomeType p_genome_type, int32_t p_mutrun_count, int32_t p_mutrun_length, MutationRun *p_run)
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

void Genome::ReinitializeGenomeNullptr(GenomeType p_genome_type, int32_t p_mutrun_count, int32_t p_mutrun_length)
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
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value_));
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
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
		
		int_result->set_int_no_check(value->tag_value_, value_index);
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
		case gID_positionsOfMutationsOfType:	return ExecuteMethod_positionsOfMutationsOfType(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_sumOfMutationsOfType:			return ExecuteMethod_sumOfMutationsOfType(p_method_id, p_arguments, p_argument_count, p_interpreter);
		default:								return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}

//	*********************	- (logical$)containsMarkerMutation(io<MutationType>$ mutType, integer$ position)
//
EidosValue_SP Genome::ExecuteMethod_containsMarkerMutation(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	EidosValue *position_value = p_arguments[1].get();
	
	if (IsNull())
		EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_containsMarkerMutation): containsMarkerMutation() cannot be called on a null genome." << EidosTerminate();
	
	SLiMSim &sim = subpop_->population_.sim_;
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, sim, "containsMarkerMutation()");
	slim_position_t marker_position = SLiMCastToPositionTypeOrRaise(position_value->IntAtIndex(0, nullptr));
	slim_position_t last_position = sim.TheChromosome().last_position_;
	
	if (marker_position > last_position)
		EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_containsMarkerMutation): containsMarkerMutation() position " << marker_position << " is past the end of the chromosome." << EidosTerminate();
	
	bool contained = contains_mutation_with_type_and_position(mutation_type_ptr, marker_position, last_position);
	
	return (contained ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
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
			int32_t mutrun_length = ((Genome *)(p_elements[0]))->mutrun_length_;		// assume all Genome objects have the same mutrun_length_; better be true...
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
				EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_elements_size * mutations_count);
				EidosValue_SP result(logical_result);
				int64_t result_index = 0;
				
				for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
				{
					Genome *element = (Genome *)(p_elements[element_index]);
					
					if (element->IsNull())
						EIDOS_TERMINATION << "ERROR (Genome::ExecuteMethod_Accelerated_containsMutations): containsMutations() cannot be called on a null genome." << EidosTerminate();
					
					bool contained = element->mutruns_[mutrun_index]->contains_mutation(mut_block_index);
					
					logical_result->set_logical_no_check(contained, result_index++);
				}
				
				return result;
			}
		}
		else
		{
			EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_elements_size * mutations_count);
			EidosValue_SP result(logical_result);
			
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
					
					logical_result->set_logical_no_check(contained, value_index);
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
	
	// Count the number of mutations of the given type, so we can reserve the right vector size : FIXME is this pre-scan really worth it?? BCH 11/29/2017
	// To avoid having to scan the genome twice for the simplest case of a single mutation, we cache the first mutation found
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	int match_count = 0;
	Mutation *first_match = nullptr;
	
	for (int run_index = 0; run_index < mutrun_count_; ++run_index)
	{
		MutationRun *mutrun = mutruns_[run_index].get();
		int mut_count = mutrun->size();
		const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
		
		for (int mut_index = 0; mut_index < mut_count; ++mut_index)
		{
			Mutation *mut = (mut_block_ptr + mut_ptr[mut_index]);
			
			if (mut->mutation_type_ptr_ == mutation_type_ptr)
			{
				if (++match_count == 1)
					first_match = mut;
			}
		}
	}
	
	// Now allocate the result vector and assemble it
	if (match_count == 1)
	{
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(first_match, gSLiM_Mutation_Class));
	}
	else
	{
		EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class))->resize_no_initialize(match_count);
		EidosValue_SP result_SP = EidosValue_SP(vec);
		
		if (match_count != 0)
		{
			int set_index = 0;
			
			for (int run_index = 0; run_index < mutrun_count_; ++run_index)
			{
				MutationRun *mutrun = mutruns_[run_index].get();
				int mut_count = mutrun->size();
				const MutationIndex *mut_ptr = mutrun->begin_pointer_const();
				
				for (int mut_index = 0; mut_index < mut_count; ++mut_index)
				{
					Mutation *mut = (mut_block_ptr + mut_ptr[mut_index]);
					
					if (mut->mutation_type_ptr_ == mutation_type_ptr)
						vec->set_object_element_no_check(mut, set_index++);
				}
			}
		}
		
		return result_SP;
	}
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
	p_out << "Mutations:"  << std::endl;
	
	for (const PolymorphismPair &polymorphism_pair : polymorphisms) 
		polymorphism_pair.second.Print(p_out);
	
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
void Genome::PrintGenomes_MS(std::ostream &p_out, std::vector<Genome *> &p_genomes, const Chromosome &p_chromosome)
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
				int genotype_string_position = 0;
				
				for (const Polymorphism &polymorphism : sorted_polymorphisms) 
				{
					if (polymorphism.mutation_ptr_ == mutation)
					{
						// mark this polymorphism as present in the genome, and move on since this mutation can't also match any other polymorphism
						genotype.replace(genotype_string_position, 1, "1");
						break;
					}
					
					genotype_string_position++;
				}
			}
		}
		
		p_out << genotype << std::endl;
	}
}

// print the sample represented by genomes, using "vcf" format
void Genome::PrintGenomes_VCF(std::ostream &p_out, std::vector<Genome *> &p_genomes, bool p_output_multiallelics)
{
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	slim_popsize_t sample_size = (slim_popsize_t)p_genomes.size();
	
	if (sample_size % 2 == 1)
		EIDOS_TERMINATION << "ERROR (Genome::PrintGenomes_VCF): Genome vector must be an even, since genomes are paired into individuals." << EidosTerminate();
	
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
	
	p_out << "##source=SLiM" << std::endl;
	p_out << "##INFO=<ID=MID,Number=1,Type=Integer,Description=\"Mutation ID in SLiM\">" << std::endl;
	p_out << "##INFO=<ID=S,Number=1,Type=Float,Description=\"Selection Coefficient\">" << std::endl;
	p_out << "##INFO=<ID=DOM,Number=1,Type=Float,Description=\"Dominance\">" << std::endl;
	p_out << "##INFO=<ID=PO,Number=1,Type=Integer,Description=\"Population of Origin\">" << std::endl;
	p_out << "##INFO=<ID=GO,Number=1,Type=Integer,Description=\"Generation of Origin\">" << std::endl;
	p_out << "##INFO=<ID=MT,Number=1,Type=Integer,Description=\"Mutation Type\">" << std::endl;
	p_out << "##INFO=<ID=AC,Number=1,Type=Integer,Description=\"Allele Count\">" << std::endl;
	p_out << "##INFO=<ID=DP,Number=1,Type=Integer,Description=\"Total Depth\">" << std::endl;
	if (p_output_multiallelics)
		p_out << "##INFO=<ID=MULTIALLELIC,Number=0,Type=Flag,Description=\"Multiallelic\">" << std::endl;
	p_out << "##FORMAT=<ID=GT,Number=1,Type=String,Description=\"Genotype\">" << std::endl;
	p_out << "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT";
	
	for (slim_popsize_t s = 0; s < sample_size; s++)
		p_out << "\ti" << s;
	p_out << std::endl;
	
	// Print a line for each mutation.  Note that we do NOT treat multiple mutations at the same position at being different alleles,
	// output on the same line.  This is because a single individual can carry more than one mutation at the same position, so it is
	// not really a question of different alleles; if there are N mutations at a given position, there are 2^N possible "alleles",
	// which is just silly to try to wedge into VCF format.  So instead, we output each mutation as a separate line, and we tag lines
	// for positions that carry more than one mutation with the MULTIALLELIC flag so they can be filtered out if they bother the user.
	for (auto polymorphism_pair : polymorphisms)
	{
		Polymorphism &polymorphism = polymorphism_pair.second;
		const Mutation *mutation = polymorphism.mutation_ptr_;
		slim_position_t mut_position = mutation->position_;
		
		// Count the mutations at the given position to determine if we are multiallelic
		int allele_count = 0;
		
		for (const PolymorphismPair &allele_count_pair : polymorphisms) 
			if (allele_count_pair.second.mutation_ptr_->position_ == mut_position)
				allele_count++;
		
		if (p_output_multiallelics || (allele_count == 1))
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
			p_out << "GO=" << mutation->generation_ << ";";
			p_out << "MT=" << mutation->mutation_type_ptr_->mutation_type_id_ << ";";
			p_out << "AC=" << polymorphism.prevalence_ << ";";
			p_out << "DP=1000";
			
			if (allele_count > 1)
				p_out << ";MULTIALLELIC";
			
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
		
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_addMutations, kEidosValueMaskNULL))->AddObject("mutations", gSLiM_Mutation_Class));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_addNewDrawnMutation, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject("mutationType", gSLiM_MutationType_Class)->AddInt("position")->AddInt_ON("originGeneration", gStaticEidosValueNULL)->AddIntObject_ON("originSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_addNewMutation, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject("mutationType", gSLiM_MutationType_Class)->AddNumeric("selectionCoeff")->AddInt("position")->AddInt_ON("originGeneration", gStaticEidosValueNULL)->AddIntObject_ON("originSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_containsMarkerMutation, kEidosValueMaskLogical | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class)->AddInt_S("position"));
		methods->emplace_back(((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_containsMutations, kEidosValueMaskLogical))->AddObject("mutations", gSLiM_Mutation_Class))->DeclareAcceleratedImp(Genome::ExecuteMethod_Accelerated_containsMutations));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_countOfMutationsOfType, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_positionsOfMutationsOfType, kEidosValueMaskInt))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationsOfType, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_removeMutations, kEidosValueMaskNULL))->AddObject("mutations", gSLiM_Mutation_Class)->AddLogical_OS("substitute", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_outputMS, kEidosValueMaskNULL))->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_outputVCF, kEidosValueMaskNULL))->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("outputMultiallelics", gStaticEidosValue_LogicalT)->AddLogical_OS("append", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_output, kEidosValueMaskNULL))->AddString_OSN("filePath", gStaticEidosValueNULL)->AddLogical_OS("append", gStaticEidosValue_LogicalF));
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
		return gStaticEidosValueNULLInvisible;
	
	// Use the 0th genome in the target to find out what the mutation run length is, so we can calculate run indices
	Genome *genome_0 = (Genome *)p_target->ObjectElementAtIndex(0, nullptr);
	int mutrun_length = genome_0->mutrun_length_;
	SLiMSim &sim = genome_0->subpop_->population_.sim_;
	Population &pop = sim.ThePopulation();
	
	sim.CheckMutationStackPolicy();
	
	if (!sim.warned_early_mutation_add_)
	{
		if ((sim.ModelType() == SLiMModelType::kModelTypeWF) && (sim.GenerationStage() == SLiMGenerationStage::kWFStage1ExecuteEarlyScripts))
		{
			p_interpreter.ExecutionOutputStream() << "#WARNING (Genome_Class::ExecuteMethod_addMutations): addMutations() should probably not be called from an early() event in a WF model; the added mutation(s) will not influence fitness values during offspring generation." << std::endl;
			sim.warned_early_mutation_add_ = true;
		}
		if ((sim.ModelType() == SLiMModelType::kModelTypeNonWF) && (sim.GenerationStage() == SLiMGenerationStage::kNonWFStage6ExecuteLateScripts))
		{
			p_interpreter.ExecutionOutputStream() << "#WARNING (Genome_Class::ExecuteMethod_addMutations): addMutations() should probably not be called from a late() event in a nonWF model; the added mutation(s) will not influence fitness values until the partway through the next generation." << std::endl;
			sim.warned_early_mutation_add_ = true;
		}
	}
	
	// Construct a vector of mutations to add that is sorted by position
	int mutations_count = mutations_value->Count();
	std::vector<Mutation *> mutations_to_add;
	
	for (int value_index = 0; value_index < mutations_count; ++value_index)
		mutations_to_add.push_back((Mutation *)mutations_value->ObjectElementAtIndex(value_index, nullptr));
	
	std::sort(mutations_to_add.begin(), mutations_to_add.end(), [ ](Mutation *i1, Mutation *i2) {return i1->position_ < i2->position_;});
	
	// Now handle the mutations to add, broken into bulk operations according to the mutation run they fall into
	int last_handled_mutrun_index = -1;
	
	for (int value_index = 0; value_index < mutations_count; ++value_index)
	{
		Mutation *next_mutation = mutations_to_add[value_index];
		const slim_position_t pos = next_mutation->position_;
		int mutrun_index = pos / mutrun_length;
		
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
	
	return gStaticEidosValueNULLInvisible;
}

//	*********************	+ (object<Mutation>)addNewDrawnMutation(io<MutationType> mutationType, integer position, [Ni originGeneration = NULL], [Nio<Subpopulation> originSubpop = NULL])
//	*********************	+ (object<Mutation>)addNewMutation(io<MutationType> mutationType, numeric selectionCoeff, integer position, [Ni originGeneration = NULL], [Nio<Subpopulation> originSubpop = NULL])
//
EidosValue_SP Genome_Class::ExecuteMethod_addNewMutation(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_argument_count, p_interpreter)
	
#ifdef __clang_analyzer__
	assert(((p_method_id == gID_addNewDrawnMutation) && (p_argument_count == 4)) || ((p_method_id == gID_addNewMutation) && (p_argument_count == 5)));
#endif
	
	EidosValue *arg_muttype = p_arguments[0].get();
	EidosValue *arg_selcoeff = (p_method_id == gID_addNewDrawnMutation ? nullptr : p_arguments[1].get());
	EidosValue *arg_position = (p_method_id == gID_addNewDrawnMutation ? p_arguments[1].get() : p_arguments[2].get());
	EidosValue *arg_origin_gen = (p_method_id == gID_addNewDrawnMutation ? p_arguments[2].get() : p_arguments[3].get());
	EidosValue *arg_origin_subpop = (p_method_id == gID_addNewDrawnMutation ? p_arguments[3].get() : p_arguments[4].get());
	
	int target_size = p_target->Count();
	
	if (target_size == 0)
		return gStaticEidosValueNULLInvisible;	// this is almost an error condition, since a mutation was expected to be added and none was
	
	// Use the 0th genome in the target to find out what the mutation run length is, so we can calculate run indices
	Genome *genome_0 = (Genome *)p_target->ObjectElementAtIndex(0, nullptr);
	int mutrun_count = genome_0->mutrun_count_;
	int mutrun_length = genome_0->mutrun_length_;
	SLiMSim &sim = genome_0->subpop_->population_.sim_;
	Population &pop = sim.ThePopulation();
	
	sim.CheckMutationStackPolicy();
	
	if (!sim.warned_early_mutation_add_)
	{
		if ((sim.ModelType() == SLiMModelType::kModelTypeWF) && (sim.GenerationStage() == SLiMGenerationStage::kWFStage1ExecuteEarlyScripts))
		{
			p_interpreter.ExecutionOutputStream() << "#WARNING (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " should probably not be called from an early() event in a WF model; the added mutation will not influence fitness values during offspring generation." << std::endl;
			sim.warned_early_mutation_add_ = true;
		}
		if ((sim.ModelType() == SLiMModelType::kModelTypeNonWF) && (sim.GenerationStage() == SLiMGenerationStage::kNonWFStage6ExecuteLateScripts))
		{
			p_interpreter.ExecutionOutputStream() << "#WARNING (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " should probably not be called from a late() event in a nonWF model; the added mutation will not influence fitness values until the partway through the next generation." << std::endl;
			sim.warned_early_mutation_add_ = true;
		}
	}
	
	// position, originGeneration, and originSubpop can now be either singletons or vectors of matching length or NULL; check them all
	int muttype_count = arg_muttype->Count();
	int selcoeff_count = (arg_selcoeff ? arg_selcoeff->Count() : 0);
	int position_count = arg_position->Count();
	int origin_gen_count = arg_origin_gen->Count();
	int origin_subpop_count = arg_origin_subpop->Count();
	
	if (arg_origin_gen->Type() == EidosValueType::kValueNULL)
		origin_gen_count = 1;
	if (arg_origin_subpop->Type() == EidosValueType::kValueNULL)
		origin_subpop_count = 1;
	
	int count_to_add = std::max({muttype_count, selcoeff_count, position_count, origin_gen_count, origin_subpop_count});
	
	if (((muttype_count != 1) && (muttype_count != count_to_add)) ||
		(arg_selcoeff && (selcoeff_count != 1) && (selcoeff_count != count_to_add)) ||
		((position_count != 1) && (position_count != count_to_add)) ||
		((origin_gen_count != 1) && (origin_gen_count != count_to_add)) ||
		((origin_subpop_count != 1) && (origin_subpop_count != count_to_add)))
		EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_addNewMutation): " << Eidos_StringForGlobalStringID(p_method_id) << " requires that mutationType, " << ((p_method_id == gID_addNewMutation) ? "selectionCoeff, " : "") << "position, originGeneration, and originSubpop be either (1) singleton, or (2) equal in length to the other non-singleton argument(s), or (3) for originGeneration or originSubpop, NULL." << EidosTerminate();
	
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
	
	// each bulk operation is performed on a single mutation run, so we need to figure out which runs we're influencing
	std::vector<int> mutrun_indexes;
	
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
			mutrun_indexes.push_back(position / mutrun_length);
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
		singleton_origin_subpop_id = (Subpopulation *)(arg_origin_subpop->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
#endif
	
	// ok, now loop to add the mutations in a single bulk operation per mutation run
	for (int mutrun_index : mutrun_indexes)
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
						origin_subpop_id = (Subpopulation *)(arg_origin_subpop->ObjectElementAtIndex(mut_parameter_index, nullptr))->subpopulation_id_;
#endif
				}
				
				MutationIndex new_mut_index = SLiM_NewMutationFromBlock();
				
				new (gSLiM_Mutation_Block + new_mut_index) Mutation(mutation_type_ptr, position, selection_coeff, origin_subpop_id, origin_generation);
				
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
		}
		
		Genome::BulkOperationEnd(operation_id, mutrun_index);
		
		MutationRun::FreeMutationRun(&mutations_to_add);
		
		// invalidate cached mutation refcounts; refcounts have changed
		pop.cached_tally_genome_count_ = 0;
	}
	
	return retval;
}

//	*********************	+ (void)output([Ns$ filePath = NULL], [logical$ append=F])
//	*********************	+ (void)outputMS([Ns$ filePath = NULL], [logical$ append=F])
//	*********************	+ (void)outputVCF([Ns$ filePath = NULL], [logical$ outputMultiallelics = T], [logical$ append=F])
//
EidosValue_SP Genome_Class::ExecuteMethod_outputX(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_argument_count, p_interpreter)
	EidosValue *filePath_value = p_arguments[0].get();
	EidosValue *outputMultiallelics_value = ((p_method_id == gID_outputVCF) ? p_arguments[1].get() : nullptr);
	EidosValue *append_value = ((p_method_id == gID_outputVCF) ? p_arguments[2].get() : p_arguments[1].get());
	
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	Chromosome &chromosome = sim.TheChromosome();
	
	// default to outputting multiallelic positions (used by VCF output only)
	bool output_multiallelics = true;
	
	if (p_method_id == gID_outputVCF)
		output_multiallelics = outputMultiallelics_value->LogicalAtIndex(0, nullptr);
	
	// Get all the genomes we're sampling from p_target
	int sample_size = p_target->Count();
	std::vector<Genome *> genomes;
	
	for (int index = 0; index < sample_size; ++index)
		genomes.push_back((Genome *)p_target->ObjectElementAtIndex(index, nullptr));
	
	// Now handle stream/file output and dispatch to the actual print method
	if (filePath_value->Type() == EidosValueType::kValueNULL)
	{
		// If filePath is NULL, output to our output stream
		std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
		
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
			Genome::PrintGenomes_MS(output_stream, genomes, chromosome);
		else if (p_method_id == gID_outputVCF)
			Genome::PrintGenomes_VCF(output_stream, genomes, output_multiallelics);
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
					Genome::PrintGenomes_MS(outfile, genomes, chromosome);
					break;
				case gID_outputVCF:
					Genome::PrintGenomes_VCF(outfile, genomes, output_multiallelics);
					break;
			}
			
			outfile.close(); 
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteMethod_outputX): could not open "<< outfile_path << "." << EidosTerminate();
		}
	}
	
	return gStaticEidosValueNULLInvisible;
}

//	*********************	+ (void)removeMutations(object mutations, [logical$ substitute = F])
//
EidosValue_SP Genome_Class::ExecuteMethod_removeMutations(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_target, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutations_value = p_arguments[0].get();
	EidosValue *substitute_value = p_arguments[1].get();
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	int target_size = p_target->Count();
	
	if (target_size == 0)
		return gStaticEidosValueNULLInvisible;
	
	// Use the 0th genome in the target to find out what the mutation run length is, so we can calculate run indices
	Genome *genome_0 = (Genome *)p_target->ObjectElementAtIndex(0, nullptr);
	int mutrun_length = genome_0->mutrun_length_;
	SLiMSim &sim = genome_0->subpop_->population_.sim_;
	
	if (!sim.warned_early_mutation_remove_)
	{
		if ((sim.ModelType() == SLiMModelType::kModelTypeWF) && (sim.GenerationStage() == SLiMGenerationStage::kWFStage1ExecuteEarlyScripts))
		{
			p_interpreter.ExecutionOutputStream() << "#WARNING (Genome_Class::ExecuteMethod_removeMutations): removeMutations() should probably not be called from an early() event in a WF model; the removed mutation(s) will still influence fitness values during offspring generation." << std::endl;
			sim.warned_early_mutation_remove_ = true;
		}
		if ((sim.ModelType() == SLiMModelType::kModelTypeNonWF) && (sim.GenerationStage() == SLiMGenerationStage::kNonWFStage6ExecuteLateScripts))
		{
			p_interpreter.ExecutionOutputStream() << "#WARNING (Genome_Class::ExecuteMethod_removeMutations): removeMutations() should probably not be called from an late() event in a nonWF model; the removed mutation(s) will still influence fitness values until the partway through the next generation." << std::endl;
			sim.warned_early_mutation_remove_ = true;
		}
	}
	
	Population &pop = sim.ThePopulation();
	slim_generation_t generation = sim.Generation();
	
	// Construct a vector of mutations to remove that is sorted by position
	int mutations_count = mutations_value->Count();
	std::vector<Mutation *> mutations_to_remove;
	
	for (int value_index = 0; value_index < mutations_count; ++value_index)
		mutations_to_remove.push_back((Mutation *)mutations_value->ObjectElementAtIndex(value_index, nullptr));
	
	std::sort(mutations_to_remove.begin(), mutations_to_remove.end(), [ ](Mutation *i1, Mutation *i2) {return i1->position_ < i2->position_;});
	
	// Create substitutions for the mutations being removed, if requested.
	// Note we don't test for the mutations actually being fixed; that is the caller's responsibility to manage.
	bool create_substitutions = substitute_value->LogicalAtIndex(0, nullptr);
	
	if (create_substitutions)
	{
		for (int value_index = 0; value_index < mutations_count; ++value_index)
		{
			Mutation *mut = (Mutation *)mutations_value->ObjectElementAtIndex(value_index, nullptr);
			
			pop.substitutions_.emplace_back(new Substitution(*mut, generation));
		}
	}
	
	// Now handle the mutations to remove, broken into bulk operations according to the mutation run they fall into
	int last_handled_mutrun_index = -1;
	
	for (int value_index = 0; value_index < mutations_count; ++value_index)
	{
		Mutation *next_mutation = mutations_to_remove[value_index];
		const slim_position_t pos = next_mutation->position_;
		int mutrun_index = pos / mutrun_length;
		
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
	
	return gStaticEidosValueNULLInvisible;
}



































































