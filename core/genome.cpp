//
//  genome.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2016 Philipp Messer.  All rights reserved.
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


#ifdef DEBUG
bool Genome::s_log_copy_and_assign_ = true;
#endif


// default constructor; gives a non-null genome of type GenomeType::kAutosome
Genome::Genome(void)
{
}

// a constructor for parent/child genomes, particularly in the SEX ONLY case: species type and null/non-null
Genome::Genome(enum GenomeType p_genome_type_, bool p_is_null) : genome_type_(p_genome_type_)
{
	// null genomes are now signalled with a null mutations pointer, rather than a separate flag
	if (p_is_null)
	{
		mutations_ = nullptr;
		mutation_capacity_ = 0;
	}
}

// prints an error message, a stacktrace, and exits; called only for DEBUG
void Genome::NullGenomeAccessError(void) const
{
	EIDOS_TERMINATION << "ERROR (Genome::NullGenomeAccessError): (internal error) a null genome was accessed." << eidos_terminate();
}

// Remove all mutations in p_genome that have a refcount of p_fixed_count, indicating that they have fixed
// This must be called with mutation counts set up correctly as all-population counts, or it will malfunction!
void Genome::RemoveFixedMutations(slim_refcount_t p_fixed_count)
{
#ifdef DEBUG
	if (mutations_ == nullptr)
		NullGenomeAccessError();
#endif
	
	Mutation **genome_iter = begin_pointer();
	Mutation **genome_backfill_iter = begin_pointer();
	Mutation **genome_max = end_pointer();
	
	// genome_iter advances through the mutation list; for each entry it hits, the entry is either fixed (skip it) or not fixed (copy it backward to the backfill pointer)
	while (genome_iter != genome_max)
	{
		Mutation *mutation_ptr = *genome_iter;
		
		if ((mutation_ptr->reference_count_ == p_fixed_count) && (mutation_ptr->mutation_type_ptr_->convert_to_substitution_))
		{
			// Fixed mutation; we want to omit it, so we just advance our pointer
			++genome_iter;
		}
		else
		{
			// Unfixed mutation; we want to keep it, so we copy it backward and advance our backfill pointer as well as genome_iter
			if (genome_backfill_iter != genome_iter)
				*genome_backfill_iter = mutation_ptr;
			
			++genome_backfill_iter;
			++genome_iter;
		}
	}
	
	// excess mutations at the end have been copied back already; we just adjust mutation_count_ and forget about them
	mutation_count_ -= (genome_iter - genome_backfill_iter);
}

bool Genome::_enforce_stack_policy_for_addition(slim_position_t p_position, MutationType *p_mut_type_ptr, MutationStackPolicy p_policy)
{
	Mutation **begin_ptr = begin_pointer();
	Mutation **end_ptr = end_pointer();
	
	if (p_policy == MutationStackPolicy::kKeepFirst)
	{
		// If the first mutation occurring at a site is kept, then we need to check for an existing mutation of this type
		// We scan in reverse order, because usually we're adding mutations on the end with emplace_back()
		for (Mutation **mut_ptr = end_ptr - 1; mut_ptr >= begin_ptr; --mut_ptr)
		{
			Mutation *mut = *mut_ptr;
			slim_position_t mut_position = mut->position_;
			
			if ((mut_position == p_position) && (mut->mutation_type_ptr_ == p_mut_type_ptr))
				return false;
			else if (mut_position < p_position)
				return true;
		}
		
		return true;
	}
	else if (p_policy == MutationStackPolicy::kKeepLast)
	{
		// If the last mutation occurring at a site is kept, then we need to check for existing mutations of this type
		// We scan in reverse order, because usually we're adding mutations on the end with emplace_back()
		Mutation **first_match_ptr = nullptr;
		
		for (Mutation **mut_ptr = end_ptr - 1; mut_ptr >= begin_ptr; --mut_ptr)
		{
			Mutation *mut = *mut_ptr;
			slim_position_t mut_position = mut->position_;
			
			if ((mut_position == p_position) && (mut->mutation_type_ptr_ == p_mut_type_ptr))
				first_match_ptr = mut_ptr;	// set repeatedly as we scan backwards, until we exit
			else if (mut_position < p_position)
				break;
		}
		
		// If we found any, we now scan forward and remove them, in anticipation of the new mutation being added
		if (first_match_ptr)
		{
			Mutation **replace_ptr = first_match_ptr;	// replace at the first match position
			Mutation **mut_ptr = first_match_ptr + 1;	// we know the initial position needs removal, so start at the next
			
			for ( ; mut_ptr < end_ptr; ++mut_ptr)
			{
				Mutation *mut = *mut_ptr;
				slim_position_t mut_position = mut->position_;
				
				if ((mut_position == p_position) && (mut->mutation_type_ptr_ == p_mut_type_ptr))
				{
					// The current scan position is a mutation that needs to be removed, so scan forward to skip copying it backward
					continue;
				}
				else
				{
					// The current scan position is a valid mutation, so we copy it backwards
					*(replace_ptr++) = mut;
				}
			}
			
			// excess mutations at the end have been copied back already; we just adjust mutation_count_ and forget about them
			mutation_count_ -= (mut_ptr - replace_ptr);
		}
		
		return true;
	}
	else
		EIDOS_TERMINATION << "ERROR (Genome::_enforce_stack_policy_for_addition): (internal error) invalid policy." << eidos_terminate();
}


//
//	Methods to enforce limited copying
//

Genome::Genome(const Genome &p_original)
{
#ifdef DEBUG
	if (s_log_copy_and_assign_)
	{
		SLIM_ERRSTREAM << "********* Genome::Genome(Genome&) called!" << std::endl;
		eidos_print_stacktrace(stderr);
		SLIM_ERRSTREAM << "************************************************" << std::endl;
	}
#endif
	
	if (p_original.mutations_ == nullptr)
	{
		// p_original is a null genome, so make ourselves null too
		if (mutations_ != mutations_buffer_)
			free(mutations_);
		
		mutations_ = nullptr;
		mutation_capacity_ = 0;
		mutation_count_ = 0;
	}
	else
	{
		int source_mutation_count = p_original.mutation_count_;
		
		// first we need to ensure that we have sufficient capacity
		if (source_mutation_count > mutation_capacity_)
		{
			mutation_capacity_ = p_original.mutation_capacity_;		// just use the same capacity as the source
			
			// mutations_buffer_ is not malloced and cannot be realloced, so forget that we were using it
			if (mutations_ == mutations_buffer_)
				mutations_ = nullptr;
			
			mutations_ = (Mutation **)realloc(mutations_, mutation_capacity_ * sizeof(Mutation*));
		}
		
		// then copy all pointers from the source to ourselves
		memcpy(mutations_, p_original.mutations_, source_mutation_count * sizeof(Mutation*));
		mutation_count_ = source_mutation_count;
	}
	
	// and copy other state
	genome_type_ = p_original.genome_type_;
}

Genome& Genome::operator= (const Genome& p_original)
{
#ifdef DEBUG
	if (s_log_copy_and_assign_)
	{
		SLIM_ERRSTREAM << "********* Genome::operator=(Genome&) called!" << std::endl;
		eidos_print_stacktrace(stderr);
		SLIM_ERRSTREAM << "************************************************" << std::endl;
	}
#endif
	
	if (this != &p_original)
	{
		if (p_original.mutations_ == nullptr)
		{
			// p_original is a null genome, so make ourselves null too
			if (mutations_ != mutations_buffer_)
				free(mutations_);
			
			mutations_ = nullptr;
			mutation_capacity_ = 0;
			mutation_count_ = 0;
		}
		else
		{
			int source_mutation_count = p_original.mutation_count_;
			
			// first we need to ensure that we have sufficient capacity
			if (source_mutation_count > mutation_capacity_)
			{
				mutation_capacity_ = p_original.mutation_capacity_;		// just use the same capacity as the source
				
				// mutations_buffer_ is not malloced and cannot be realloced, so forget that we were using it
				if (mutations_ == mutations_buffer_)
					mutations_ = nullptr;
				
				mutations_ = (Mutation **)realloc(mutations_, mutation_capacity_ * sizeof(Mutation*));
			}
			
			// then copy all pointers from the source to ourselves
			memcpy(mutations_, p_original.mutations_, source_mutation_count * sizeof(Mutation*));
			mutation_count_ = source_mutation_count;
		}
		
		// and copy other state
		genome_type_ = p_original.genome_type_;
	}
	
	return *this;
}

#ifdef DEBUG
bool Genome::LogGenomeCopyAndAssign(bool p_log)
{
	bool old_value = s_log_copy_and_assign_;
	
	s_log_copy_and_assign_ = p_log;
	
	return old_value;
}
#endif

Genome::~Genome(void)
{
	// mutations_buffer_ is not malloced and cannot be freed; free only if we have an external buffer
	if (mutations_ != mutations_buffer_)
		free(mutations_);
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support

void Genome::GenerateCachedEidosValue(void)
{
	// Note that this cache cannot be invalidated, because we are guaranteeing that this object will
	// live for at least as long as the symbol table it may be placed into!
	self_value_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Genome_Class));
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
	
	if (mutations_ == nullptr)
		p_ostream << ":null>";
	else
		p_ostream << ":" << mutation_count_ << ">";
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
			return ((mutations_ == nullptr) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_mutations:
		{
			EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class))->Reserve(mutation_count_);
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			for (int mut_index = 0; mut_index < mutation_count_; ++mut_index)
				vec->PushObjectElement(mutations_[mut_index]);
			
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

eidos_logical_t Genome::GetProperty_Accelerated_Logical(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_isNullGenome:		return (mutations_ == nullptr);
			
		default:					return EidosObjectElement::GetProperty_Accelerated_Logical(p_property_id);
	}
}

int64_t Genome::GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_tag:				return tag_value_;
			
		default:					return EidosObjectElement::GetProperty_Accelerated_Int(p_property_id);
	}
}

void Genome::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	switch (p_property_id)
	{
		case gID_tag:
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

EidosValue_SP Genome::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0].get() : nullptr);
	//EidosValue *arg1_value = ((p_argument_count >= 2) ? p_arguments[1].get() : nullptr);
	

	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
			//
			//	*********************	- (void)addMutations(object mutations)
			//
#pragma mark -addMutations()
			
		case gID_addMutations:
		{
			SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
			
			if (!sim)
				EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): (internal error) the sim is not registered as the context pointer." << eidos_terminate();
			
			if ((sim->GenerationStage() == SLiMGenerationStage::kStage1ExecuteEarlyScripts) && (!sim->warned_early_mutation_add_))
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (Genome::ExecuteInstanceMethod): addMutations() should probably not be called from an early() event; the added mutation(s) will not influence fitness values during offspring generation." << std::endl;
				sim->warned_early_mutation_add_ = true;
			}
			
			int arg0_count = arg0_value->Count();
			
			if (arg0_count)
			{
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					Mutation *new_mutation = (Mutation *)(arg0_value->ObjectElementAtIndex(value_index, nullptr));
					
					if (enforce_stack_policy_for_addition(new_mutation->position_, new_mutation->mutation_type_ptr_))
					{
						insert_sorted_mutation_if_unique(new_mutation);
						
						// I think this is not needed; how would the user ever get a Mutation that was not already in the registry?
						//if (!registry.contains_mutation(new_mutation))
						//	registry.emplace_back(new_mutation);
						
						// Similarly, no need to check and set pure_neutral_; the mutation is already in the system
					}
				}
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			//
			//	*********************	- (logical)containsMutations(object<Mutation> mutations)
			//
#pragma mark -containsMutations()
			
		case gID_containsMutations:
		{
			int arg0_count = arg0_value->Count();
			int mutation_count = this->size();
			
			if (arg0_count == 1)
			{
				Mutation *mut = (Mutation *)(arg0_value->ObjectElementAtIndex(0, nullptr));
				
				for (int mut_index = 0; mut_index < mutation_count; ++mut_index)
					if (mutations_[mut_index] == mut)
						return gStaticEidosValue_LogicalT;
				
				return gStaticEidosValue_LogicalF;
			}
			else
			{
				EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->Reserve(arg0_count);
				std::vector<eidos_logical_t> &logical_result_vec = *logical_result->LogicalVector_Mutable();
				
				for (int value_index = 0; value_index < arg0_count; ++value_index)
				{
					Mutation *mut = (Mutation *)(arg0_value->ObjectElementAtIndex(value_index, nullptr));
					bool contains_mut = false;
					
					for (int mut_index = 0; mut_index < mutation_count; ++mut_index)
						if (mutations_[mut_index] == mut)
							contains_mut = true;
					
					logical_result_vec.emplace_back(contains_mut);
				}
				
				return EidosValue_SP(logical_result);
			}
		}
			
			//
			//	*********************	- (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
			//
#pragma mark -countOfMutationsOfType()
			
		case gID_countOfMutationsOfType:
		{
			MutationType *mutation_type_ptr = nullptr;
			
			if (arg0_value->Type() == EidosValueType::kValueInt)
			{
				SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
				
				if (!sim)
					EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): (internal error) the sim is not registered as the context pointer." << eidos_terminate();
				
				slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
				auto found_muttype_pair = sim->MutationTypes().find(mutation_type_id);
				
				if (found_muttype_pair == sim->MutationTypes().end())
					EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): countOfMutationsOfType() mutation type m" << mutation_type_id << " not defined." << eidos_terminate();
				
				mutation_type_ptr = found_muttype_pair->second;
			}
			else
			{
				mutation_type_ptr = (MutationType *)(arg0_value->ObjectElementAtIndex(0, nullptr));
			}
			
			// Count the number of mutations of the given type
			int mutation_count = this->size();
			int match_count = 0, mut_index;
			
			for (mut_index = 0; mut_index < mutation_count; ++mut_index)
				if (mutations_[mut_index]->mutation_type_ptr_ == mutation_type_ptr)
					++match_count;
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(match_count));
		}
			
			//
			//	*********************	- (object<Mutation>)mutationsOfType(io<MutationType>$ mutType)
			//
#pragma mark -mutationsOfType()
			
		case gID_mutationsOfType:
		{
			MutationType *mutation_type_ptr = nullptr;
			
			if (arg0_value->Type() == EidosValueType::kValueInt)
			{
				SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
				
				if (!sim)
					EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): (internal error) the sim is not registered as the context pointer." << eidos_terminate();
				
				slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
				auto found_muttype_pair = sim->MutationTypes().find(mutation_type_id);
				
				if (found_muttype_pair == sim->MutationTypes().end())
					EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): mutationsOfType() mutation type m" << mutation_type_id << " not defined." << eidos_terminate();
				
				mutation_type_ptr = found_muttype_pair->second;
			}
			else
			{
				mutation_type_ptr = (MutationType *)(arg0_value->ObjectElementAtIndex(0, nullptr));
			}
			
			// Count the number of mutations of the given type, so we can reserve the right vector size
			// To avoid having to scan the genome twice for the simplest case of a single mutation, we cache the first mutation found
			int mutation_count = this->size();
			int match_count = 0, mut_index;
			Mutation *first_match = nullptr;
			
			for (mut_index = 0; mut_index < mutation_count; ++mut_index)
			{
				Mutation *mut = mutations_[mut_index];
				
				if (mut->mutation_type_ptr_ == mutation_type_ptr)
				{
					if (++match_count == 1)
						first_match = mut;
				}
			}
			
			// Now allocate the result vector and assemble it
			if (match_count == 1)
			{
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(first_match, gSLiM_Mutation_Class));
			}
			else
			{
				EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class))->Reserve(match_count);
				EidosValue_SP result_SP = EidosValue_SP(vec);
				
				if (match_count != 0)
				{
					for (mut_index = 0; mut_index < mutation_count; ++mut_index)
					{
						Mutation *mut = mutations_[mut_index];
						
						if (mut->mutation_type_ptr_ == mutation_type_ptr)
							vec->PushObjectElement(mut);
					}
				}
				
				return result_SP;
			}
		}
			
			
			//
			//	*********************	- (void)removeMutations(object mutations)
			//
#pragma mark -removeMutations()
			
		case gID_removeMutations:
		{
			SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
			
			if (!sim)
				EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): (internal error) the sim is not registered as the context pointer." << eidos_terminate();
			
			if ((sim->GenerationStage() == SLiMGenerationStage::kStage1ExecuteEarlyScripts) && (!sim->warned_early_mutation_remove_))
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (Genome::ExecuteInstanceMethod): removeMutations() should probably not be called from an early() event; the removed mutation(s) will still influence fitness values during offspring generation." << std::endl;
				sim->warned_early_mutation_remove_ = true;
			}
			
			int arg0_count = arg0_value->Count();
			
			if (arg0_count)
			{
				if (mutations_ == nullptr)
					NullGenomeAccessError();
				
				// Remove the specified mutations; see RemoveFixedMutations for the origins of this code
				Mutation **genome_iter = begin_pointer();
				Mutation **genome_backfill_iter = begin_pointer();
				Mutation **genome_max = end_pointer();
				
				// genome_iter advances through the mutation list; for each entry it hits, the entry is either removed (skip it) or not removed (copy it backward to the backfill pointer)
				while (genome_iter != genome_max)
				{
					Mutation *candidate_mutation = *genome_iter;
					bool should_remove = false;
					
					for (int value_index = 0; value_index < arg0_count; ++value_index)
						if (arg0_value->ObjectElementAtIndex(value_index, nullptr) == candidate_mutation)
						{
							should_remove = true;
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
				mutation_count_ -= (genome_iter - genome_backfill_iter);
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			// all others, including gID_none
		default:
			return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}

// print the sample represented by genomes, using SLiM's own format
void Genome::PrintGenomes_slim(std::ostream &p_out, std::vector<Genome *> &genomes, slim_objectid_t p_source_subpop_id)
{
	slim_popsize_t sample_size = (slim_popsize_t)genomes.size();
	
	// get the polymorphisms within the sample
	PolymorphismMap polymorphisms;
	
	for (slim_popsize_t s = 0; s < sample_size; s++)
	{
		Genome &genome = *genomes[s];
		
		if (genome.IsNull())
			EIDOS_TERMINATION << "ERROR (Genome::PrintGenomes_slim): cannot output null genomes." << eidos_terminate();
		
		for (int k = 0; k < genome.size(); k++)			// go through all mutations
			AddMutationToPolymorphismMap(&polymorphisms, genome[k]);
	}
	
	// print the sample's polymorphisms; NOTE the output format changed due to the addition of mutation_id_, BCH 11 June 2016
	p_out << "Mutations:"  << std::endl;
	
	for (const PolymorphismPair &polymorphism_pair : polymorphisms) 
		polymorphism_pair.second.print(p_out);
	
	// print the sample's genomes
	p_out << "Genomes:" << std::endl;
	
	for (slim_popsize_t j = 0; j < sample_size; j++)														// go through all individuals
	{
		Genome &genome = *genomes[j];
		
		if (p_source_subpop_id == -1)
			p_out << "p*:" << j;
		else
			p_out << "p" << p_source_subpop_id << ":" << j;
		
		p_out << " " << genome.Type();
		
		for (int k = 0; k < genome.size(); k++)	// go through all mutations
		{
			slim_polymorphismid_t polymorphism_id = FindMutationInPolymorphismMap(polymorphisms, genome[k]);
			
			if (polymorphism_id == -1)
				EIDOS_TERMINATION << "ERROR (Genome::PrintGenomes_slim): (internal error) polymorphism not found." << eidos_terminate();
			
			p_out << " " << polymorphism_id;
		}
		
		p_out << std::endl;
	}
}

// print the sample represented by genomes, using "ms" format
void Genome::PrintGenomes_ms(std::ostream &p_out, std::vector<Genome *> &genomes, const Chromosome &p_chromosome)
{
	slim_popsize_t sample_size = (slim_popsize_t)genomes.size();
	
	// get the polymorphisms within the sample
	PolymorphismMap polymorphisms;
	
	for (slim_popsize_t s = 0; s < sample_size; s++)
	{
		Genome &genome = *genomes[s];
		
		if (genome.IsNull())
			EIDOS_TERMINATION << "ERROR (Genome::PrintGenomes_ms): cannot output null genomes." << eidos_terminate();
		
		for (int k = 0; k < genome.size(); k++)			// go through all mutations
			AddMutationToPolymorphismMap(&polymorphisms, genome[k]);
	}
	
	// print header
	p_out << "//" << std::endl << "segsites: " << polymorphisms.size() << std::endl;
	
	// print the sample's positions
	if (polymorphisms.size() > 0)
	{
		p_out << "positions:";
		
		for (const PolymorphismPair &polymorphism_pair : polymorphisms) 
			p_out << " " << std::fixed << std::setprecision(7) << static_cast<double>(polymorphism_pair.second.mutation_ptr_->position_) / p_chromosome.last_position_;	// this prints positions as being in the interval [0,1], which Philipp decided was the best policy
		
		p_out << std::endl;
	}
	
	// print the sample's genotypes
	for (slim_popsize_t j = 0; j < sample_size; j++)														// go through all individuals
	{
		Genome &genome = *genomes[j];
		std::string genotype(polymorphisms.size(), '0'); // fill with 0s
		
		for (int k = 0; k < genome.size(); k++)	// go through all mutations
		{
			const Mutation *mutation = genome[k];
			slim_mutationid_t mutation_id = mutation->mutation_id_;
			int genotype_string_position = 0;
			
			for (const PolymorphismPair &polymorphism_pair : polymorphisms) 
			{
				if (polymorphism_pair.first == mutation_id)
				{
					// mark this polymorphism as present in the genome, and move on since this mutation can't also match any other polymorphism
					genotype.replace(genotype_string_position, 1, "1");
					break;
				}
				
				genotype_string_position++;
			}
		}
		
		p_out << genotype << std::endl;
	}
}

// print the sample represented by genomes, using "vcf" format
void Genome::PrintGenomes_vcf(std::ostream &p_out, std::vector<Genome *> &genomes, bool p_output_multiallelics)
{
	slim_popsize_t sample_size = (slim_popsize_t)genomes.size();
	
	if (sample_size % 2 == 1)
		EIDOS_TERMINATION << "ERROR (Genome::PrintGenomes_vcf): Genome vector must be an even, since genomes are paired into individuals." << eidos_terminate();
	
	sample_size /= 2;
	
	// get the polymorphisms within the sample
	PolymorphismMap polymorphisms;
	
	for (slim_popsize_t s = 0; s < sample_size; s++)
	{
		Genome &genome1 = *genomes[s * 2];
		Genome &genome2 = *genomes[s * 2 + 1];
		
		if (!genome1.IsNull())
			for (int k = 0; k < genome1.size(); k++)
				AddMutationToPolymorphismMap(&polymorphisms, genome1[k]);
		
		if (!genome2.IsNull())
			for (int k = 0; k < genome2.size(); k++)
				AddMutationToPolymorphismMap(&polymorphisms, genome2[k]);
	}
	
	// print the VCF header
	p_out << "##fileformat=VCFv4.2" << std::endl;
	
	{
		time_t rawtime;
		struct tm *timeinfo;
		char buffer[25];	// should never be more than 10, in fact, plus a null
		
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		strftime(buffer, 25, "%Y%m%d", timeinfo);
		
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
				Genome &g1 = *genomes[s * 2];
				Genome &g2 = *genomes[s * 2 + 1];
				bool g1_null = g1.IsNull(), g2_null = g2.IsNull();
				
				if (g1_null && g2_null)
				{
					// Both genomes are null; we should have eliminated the possibility of this with the check above
					EIDOS_TERMINATION << "ERROR (Population::PrintSample_vcf): (internal error) no non-null genome to output for individual." << eidos_terminate();
				}
				else if (g1_null)
				{
					// An unpaired X or Y; we emit this as haploid, I think that is the right call...
					p_out << (g2.contains_mutation(mutation) ? "\t1" : "\t0");
				}
				else if (g2_null)
				{
					// An unpaired X or Y; we emit this as haploid, I think that is the right call...
					p_out << (g1.contains_mutation(mutation) ? "\t1" : "\t0");
				}
				else
				{
					// Both genomes are non-null; emit an x|y pair that indicates the data is phased
					bool g1_has_mut = g1.contains_mutation(mutation);
					bool g2_has_mut = g2.contains_mutation(mutation);
					
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

class Genome_Class : public EidosObjectClass
{
public:
	Genome_Class(const Genome_Class &p_original) = delete;	// no copy-construct
	Genome_Class& operator=(const Genome_Class&) = delete;	// no copying
	
	Genome_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_Genome_Class = new Genome_Class();


Genome_Class::Genome_Class(void)
{
}

const std::string &Genome_Class::ElementType(void) const
{
	return gStr_Genome;
}

const std::vector<const EidosPropertySignature *> *Genome_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->emplace_back(SignatureForPropertyOrRaise(gID_genomeType));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_isNullGenome));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_mutations));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Genome_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *genomeTypeSig = nullptr;
	static EidosPropertySignature *isNullGenomeSig = nullptr;
	static EidosPropertySignature *mutationsSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!genomeTypeSig)
	{
		genomeTypeSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_genomeType,		gID_genomeType,		true,	kEidosValueMaskString | kEidosValueMaskSingleton));
		isNullGenomeSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_isNullGenome,	gID_isNullGenome,	true,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAccelerated();
		mutationsSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_mutations,		gID_mutations,		true,	kEidosValueMaskObject, gSLiM_Mutation_Class));
		tagSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,				gID_tag,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAccelerated();
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_genomeType:	return genomeTypeSig;
		case gID_isNullGenome:	return isNullGenomeSig;
		case gID_mutations:		return mutationsSig;
		case gID_tag:			return tagSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *Genome_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gID_addMutations));
		methods->emplace_back(SignatureForMethodOrRaise(gID_addNewDrawnMutation));
		methods->emplace_back(SignatureForMethodOrRaise(gID_addNewMutation));
		methods->emplace_back(SignatureForMethodOrRaise(gID_containsMutations));
		methods->emplace_back(SignatureForMethodOrRaise(gID_countOfMutationsOfType));
		methods->emplace_back(SignatureForMethodOrRaise(gID_mutationsOfType));
		methods->emplace_back(SignatureForMethodOrRaise(gID_outputMS));
		methods->emplace_back(SignatureForMethodOrRaise(gID_outputVCF));
		methods->emplace_back(SignatureForMethodOrRaise(gID_output));
		methods->emplace_back(SignatureForMethodOrRaise(gID_removeMutations));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Genome_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *addMutationsSig = nullptr;
	static EidosClassMethodSignature *addNewDrawnMutationSig = nullptr;
	static EidosClassMethodSignature *addNewMutationSig = nullptr;
	static EidosInstanceMethodSignature *containsMutationsSig = nullptr;
	static EidosInstanceMethodSignature *countOfMutationsOfTypeSig = nullptr;
	static EidosInstanceMethodSignature *mutationsOfTypeSig = nullptr;
	static EidosInstanceMethodSignature *removeMutationsSig = nullptr;
	static EidosClassMethodSignature *outputMSSig = nullptr;
	static EidosClassMethodSignature *outputVCFSig = nullptr;
	static EidosClassMethodSignature *outputSig = nullptr;
	
	if (!addMutationsSig)
	{
		addMutationsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_addMutations, kEidosValueMaskNULL))->AddObject("mutations", gSLiM_Mutation_Class);
		addNewDrawnMutationSig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_addNewDrawnMutation, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Mutation_Class))->AddIntObject_S("mutationType", gSLiM_MutationType_Class)->AddInt_S("position")->AddInt_OSN("originGeneration")->AddIntObject_OS("originSubpop", gSLiM_Subpopulation_Class);
		addNewMutationSig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_addNewMutation, kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Mutation_Class))->AddIntObject_S("mutationType", gSLiM_MutationType_Class)->AddNumeric_S("selectionCoeff")->AddInt_S("position")->AddInt_OSN("originGeneration")->AddIntObject_OS("originSubpop", gSLiM_Subpopulation_Class);
		containsMutationsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_containsMutations, kEidosValueMaskLogical))->AddObject("mutations", gSLiM_Mutation_Class);
		countOfMutationsOfTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_countOfMutationsOfType, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class);
		mutationsOfTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_mutationsOfType, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class);
		removeMutationsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_removeMutations, kEidosValueMaskNULL))->AddObject("mutations", gSLiM_Mutation_Class);
		outputMSSig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_outputMS, kEidosValueMaskNULL))->AddString_OSN("filePath");
		outputVCFSig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_outputVCF, kEidosValueMaskNULL))->AddString_OSN("filePath")->AddLogical_OS("outputMultiallelics");
		outputSig = (EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_output, kEidosValueMaskNULL))->AddString_OSN("filePath");
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_addMutations:				return addMutationsSig;
		case gID_addNewDrawnMutation:		return addNewDrawnMutationSig;
		case gID_addNewMutation:			return addNewMutationSig;
		case gID_containsMutations:			return containsMutationsSig;
		case gID_countOfMutationsOfType:	return countOfMutationsOfTypeSig;
		case gID_mutationsOfType:			return mutationsOfTypeSig;
		case gID_removeMutations:			return removeMutationsSig;
		case gID_outputMS:					return outputMSSig;
		case gID_outputVCF:					return outputVCFSig;
		case gID_output:					return outputSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForMethod(p_method_id);
	}
}

EidosValue_SP Genome_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0].get() : nullptr);
	EidosValue *arg1_value = ((p_argument_count >= 2) ? p_arguments[1].get() : nullptr);
	EidosValue *arg2_value = ((p_argument_count >= 3) ? p_arguments[2].get() : nullptr);
	EidosValue *arg3_value = ((p_argument_count >= 4) ? p_arguments[3].get() : nullptr);
	EidosValue *arg4_value = ((p_argument_count >= 5) ? p_arguments[4].get() : nullptr);
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
			//
			//	*********************	+ (object<Mutation>)addNewDrawnMutation(io<MutationType>$ mutationType, integer$ position, [Ni$ originGeneration], [io<Subpopulation>$ originSubpop])
			//
#pragma mark +addNewDrawnMutation()
			
		case gID_addNewDrawnMutation:
		{
			int target_size = p_target->Count();
			SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
			
			if (!sim)
				EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): (internal error) the sim is not registered as the context pointer." << eidos_terminate();
			
			if ((sim->GenerationStage() == SLiMGenerationStage::kStage1ExecuteEarlyScripts) && (!sim->warned_early_mutation_add_))
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (Genome::ExecuteInstanceMethod): addNewDrawnMutation() should probably not be called from an early() event; the added mutation will not influence fitness values during offspring generation." << std::endl;
				sim->warned_early_mutation_add_ = true;
			}
			
			MutationType *mutation_type_ptr = nullptr;
			
			if (arg0_value->Type() == EidosValueType::kValueInt)
			{
				slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
				auto found_muttype_pair = sim->MutationTypes().find(mutation_type_id);
				
				if (found_muttype_pair != sim->MutationTypes().end())
					mutation_type_ptr = found_muttype_pair->second;
				
				if (!mutation_type_ptr)
					EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): addNewDrawnMutation() mutation type m" << mutation_type_id << " not defined." << eidos_terminate();
			}
			else
			{
				mutation_type_ptr = dynamic_cast<MutationType *>(arg0_value->ObjectElementAtIndex(0, nullptr));
			}
			
			slim_position_t position = SLiMCastToPositionTypeOrRaise(arg1_value->IntAtIndex(0, nullptr));
			
			if (position > sim->TheChromosome().last_position_)
				EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): addNewDrawnMutation() position " << position << " is past the end of the chromosome." << eidos_terminate();
			
			slim_generation_t origin_generation;
			
			if (!arg2_value || (arg2_value->Type() == EidosValueType::kValueNULL))
				origin_generation = sim->Generation();
			else
				origin_generation = SLiMCastToGenerationTypeOrRaise(arg2_value->IntAtIndex(0, nullptr));
			
			slim_objectid_t origin_subpop_id;
			
			if (!arg3_value)
			{
				Population &pop = sim->ThePopulation();
				
				origin_subpop_id = -1;
				
				// We set the origin subpopulation based on the first genome in the target
				if (target_size >= 1)
				{
					Genome *first_target = (Genome *)p_target->ObjectElementAtIndex(0, nullptr);
					
					for (auto subpop_iter = pop.begin(); subpop_iter != pop.end(); subpop_iter++)
						if (subpop_iter->second->ContainsGenome(first_target))
							origin_subpop_id = subpop_iter->second->subpopulation_id_;
					
					if (origin_subpop_id == -1)
						EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): addNewDrawnMutation() could not locate the subpopulation for the target genome." << eidos_terminate();
				}
			}
			else if (arg3_value->Type() == EidosValueType::kValueInt)
				origin_subpop_id = SLiMCastToObjectidTypeOrRaise(arg3_value->IntAtIndex(0, nullptr));
			else
				origin_subpop_id = dynamic_cast<Subpopulation *>(arg3_value->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
			
			// Generate and add the new mutation
			Mutation *mutation = nullptr;
			
			for (int index = 0; index < target_size; ++index)
			{
				Genome *target_genome = (Genome *)p_target->ObjectElementAtIndex(index, nullptr);
				
				if (target_genome->enforce_stack_policy_for_addition(position, mutation_type_ptr))
				{
					if (!mutation)
					{
						double selection_coeff = mutation_type_ptr->DrawSelectionCoefficient();
						
						mutation = new (gSLiM_Mutation_Pool->AllocateChunk()) Mutation(mutation_type_ptr, position, selection_coeff, origin_subpop_id, origin_generation);
						
						// This mutation type might not be used by any genomic element type (i.e. might not already be vetted), so we need to check and set pure_neutral_
						if (selection_coeff != 0.0)
							sim->pure_neutral_ = false;
						
					}
					
					target_genome->insert_sorted_mutation(mutation);
				}
			}
			
			if (mutation)
			{
				// Update the population's registry and cache information
				Population &pop = sim->ThePopulation();
				
				pop.mutation_registry_.emplace_back(mutation);
				pop.cached_genome_count_ = 0;
				
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(mutation, gSLiM_Mutation_Class));
			}
			else
			{
				return gStaticEidosValueNULLInvisible;
			}
		}
			
			
			//
			//	*********************	+ (object<Mutation>)addNewMutation(io<MutationType>$ mutationType, numeric$ selectionCoeff, integer$ position, [Ni$ originGeneration], [io<Subpopulation>$ originSubpop])
			//
#pragma mark +addNewMutation()
			
		case gID_addNewMutation:
		{
			int target_size = p_target->Count();
			SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
			
			if (!sim)
				EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): (internal error) the sim is not registered as the context pointer." << eidos_terminate();
			
			if ((sim->GenerationStage() == SLiMGenerationStage::kStage1ExecuteEarlyScripts) && (!sim->warned_early_mutation_add_))
			{
				p_interpreter.ExecutionOutputStream() << "#WARNING (Genome::ExecuteInstanceMethod): addNewMutation() should probably not be called from an early() event; the added mutation will not influence fitness values during offspring generation." << std::endl;
				sim->warned_early_mutation_add_ = true;
			}
			
			MutationType *mutation_type_ptr = nullptr;
			
			if (arg0_value->Type() == EidosValueType::kValueInt)
			{
				slim_objectid_t mutation_type_id = SLiMCastToObjectidTypeOrRaise(arg0_value->IntAtIndex(0, nullptr));
				auto found_muttype_pair = sim->MutationTypes().find(mutation_type_id);
				
				if (found_muttype_pair != sim->MutationTypes().end())
					mutation_type_ptr = found_muttype_pair->second;
				
				if (!mutation_type_ptr)
					EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): addNewMutation() mutation type m" << mutation_type_id << " not defined." << eidos_terminate();
			}
			else
			{
				mutation_type_ptr = dynamic_cast<MutationType *>(arg0_value->ObjectElementAtIndex(0, nullptr));
			}
			
			double selection_coeff = arg1_value->FloatAtIndex(0, nullptr);
			
			slim_position_t position = SLiMCastToPositionTypeOrRaise(arg2_value->IntAtIndex(0, nullptr));
			
			if (position > sim->TheChromosome().last_position_)
				EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): addNewMutation() position " << position << " is past the end of the chromosome." << eidos_terminate();
			
			slim_generation_t origin_generation;
			
			if (!arg3_value || (arg3_value->Type() == EidosValueType::kValueNULL))
				origin_generation = sim->Generation();
			else
				origin_generation = SLiMCastToGenerationTypeOrRaise(arg3_value->IntAtIndex(0, nullptr));
			
			slim_objectid_t origin_subpop_id;
			
			if (!arg4_value)
			{
				Population &pop = sim->ThePopulation();
				
				origin_subpop_id = -1;
				
				// We set the origin subpopulation based on the first genome in the target
				if (target_size >= 1)
				{
					Genome *first_target = (Genome *)p_target->ObjectElementAtIndex(0, nullptr);
					
					for (auto subpop_iter = pop.begin(); subpop_iter != pop.end(); subpop_iter++)
						if (subpop_iter->second->ContainsGenome(first_target))
							origin_subpop_id = subpop_iter->second->subpopulation_id_;
					
					if (origin_subpop_id == -1)
						EIDOS_TERMINATION << "ERROR (Genome::ExecuteInstanceMethod): addNewMutation() could not locate the subpopulation for the target genome." << eidos_terminate();
				}
			}
			else if (arg4_value->Type() == EidosValueType::kValueInt)
				origin_subpop_id = SLiMCastToObjectidTypeOrRaise(arg4_value->IntAtIndex(0, nullptr));
			else
				origin_subpop_id = dynamic_cast<Subpopulation *>(arg4_value->ObjectElementAtIndex(0, nullptr))->subpopulation_id_;
			
			// Generate and add the new mutation
			Mutation *mutation = nullptr;
			
			for (int index = 0; index < target_size; ++index)
			{
				Genome *target_genome = (Genome *)p_target->ObjectElementAtIndex(index, nullptr);
				
				if (target_genome->enforce_stack_policy_for_addition(position, mutation_type_ptr))
				{
					if (!mutation)
					{
						mutation = new (gSLiM_Mutation_Pool->AllocateChunk()) Mutation(mutation_type_ptr, position, selection_coeff, origin_subpop_id, origin_generation);
						
						// Since the selection coefficient was chosen by the user, we need to check and set pure_neutral_
						if (selection_coeff != 0.0)
							sim->pure_neutral_ = false;
					}
					
					target_genome->insert_sorted_mutation(mutation);
				}
			}
			
			if (mutation)
			{
				// Update the population's registry and cache information
				Population &pop = sim->ThePopulation();
				
				pop.mutation_registry_.emplace_back(mutation);
				pop.cached_genome_count_ = 0;
				
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(mutation, gSLiM_Mutation_Class));
			}
			else
			{
				return gStaticEidosValueNULLInvisible;
			}
		}
			
			//
			//	*********************	+ (void)output([Ns$ filePath])
			//	*********************	+ (void)outputMS([Ns$ filePath])
			//	*********************	+ (void)outputVCF([Ns$ filePath], [logical$ outputMultiallelics])
			//
#pragma mark +output()
#pragma mark +outputMS()
#pragma mark +outputVCF()
			
		case gID_output:
		case gID_outputMS:
		case gID_outputVCF:
		{
			SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
			Chromosome &chromosome = sim->TheChromosome();
			
			// default to outputting multiallelic positions (used by VCF output only)
			bool output_multiallelics = true;
			
			if ((p_method_id == gID_outputVCF) && (p_argument_count == 2))
				output_multiallelics = arg1_value->LogicalAtIndex(0, nullptr);
			
			// Get all the genomes we're sampling from p_target
			int sample_size = p_target->Count();
			std::vector<Genome *> genomes;
			
			for (int index = 0; index < sample_size; ++index)
				genomes.push_back((Genome *)p_target->ObjectElementAtIndex(index, nullptr));
			
			// Now handle stream/file output and dispatch to the actual print method
			if ((p_argument_count == 0) || (arg0_value->Type() == EidosValueType::kValueNULL))
			{
				// If filePath is unspecified or NULL, output to our output stream
				std::ostringstream &output_stream = p_interpreter.ExecutionOutputStream();
				
				// For the output stream, we put out a descriptive SLiM-style header for all output types
				output_stream << "#OUT: " << sim->Generation() << " G";
				
				if (p_method_id == gID_output)
					output_stream << "S";
				else if (p_method_id == gID_outputMS)
					output_stream << "M";
				else if (p_method_id == gID_outputVCF)
					output_stream << "V";
				
				output_stream << " " << sample_size << std::endl;
				
				// Call out to print the actual sample
				if (p_method_id == gID_output)
					Genome::PrintGenomes_slim(output_stream, genomes, -1);	// -1 represents unknown source subpopulation
				else if (p_method_id == gID_outputMS)
					Genome::PrintGenomes_ms(output_stream, genomes, chromosome);
				else if (p_method_id == gID_outputVCF)
					Genome::PrintGenomes_vcf(output_stream, genomes, output_multiallelics);
			}
			else
			{
				// Otherwise, output to filePath
				std::string outfile_path = EidosResolvedPath(arg0_value->StringAtIndex(0, nullptr));
				std::ofstream outfile;
				
				outfile.open(outfile_path.c_str());
				
				if (outfile.is_open())
				{
					switch (p_method_id)
					{
						case gID_output:
							// For file output, we put out the descriptive SLiM-style header only for SLiM-format output
							outfile << "#OUT: " << sim->Generation() << " GS " << sample_size << " " << outfile_path << std::endl;
							Genome::PrintGenomes_slim(outfile, genomes, -1);	// -1 represents unknown source subpopulation
							break;
						case gID_outputMS:
							Genome::PrintGenomes_ms(outfile, genomes, chromosome);
							break;
						case gID_outputVCF:
							Genome::PrintGenomes_vcf(outfile, genomes, output_multiallelics);
							break;
					}
					
					outfile.close(); 
				}
				else
				{
					EIDOS_TERMINATION << "ERROR (Genome_Class::ExecuteClassMethod): could not open "<< outfile_path << "." << eidos_terminate();
				}
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			// all others, including gID_none
		default:
			return EidosObjectClass::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
	}
}



































































