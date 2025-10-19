//
//  mutation.cpp
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


#include "mutation.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "community.h"
#include "species.h"
#include "mutation_block.h"

#include <algorithm>
#include <string>
#include <vector>
#include <cstdint>
#include <csignal>


#pragma mark -
#pragma mark Mutation
#pragma mark -

// A global counter used to assign all Mutation objects a unique ID
slim_mutationid_t gSLiM_next_mutation_id = 0;

Mutation::Mutation(MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, slim_effect_t p_selection_coeff, slim_effect_t p_dominance_coeff, slim_objectid_t p_subpop_index, slim_tick_t p_tick, int8_t p_nucleotide) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(p_selection_coeff), dominance_coeff_(p_dominance_coeff), subpop_index_(p_subpop_index), origin_tick_(p_tick), chromosome_index_(p_chromosome_index), state_(MutationState::kNewMutation), nucleotide_(p_nucleotide), mutation_id_(gSLiM_next_mutation_id++)
{
#ifdef DEBUG_LOCKS_ENABLED
	mutation_block_LOCK.start_critical(2);
#endif
	
	Species &species = mutation_type_ptr_->species_;
	MutationBlock *mutation_block = species.SpeciesMutationBlock();
	
	// initialize the tag to the "unset" value
	tag_value_ = SLIM_TAG_UNSET_VALUE;
	
	// cache values used by the fitness calculation code for speed; see header
	cached_one_plus_sel_ = (slim_effect_t)std::max(0.0, 1.0 + selection_coeff_);
	cached_one_plus_dom_sel_ = (slim_effect_t)std::max(0.0, 1.0 + dominance_coeff_ * selection_coeff_);
	cached_one_plus_hemizygousdom_sel_ = (slim_effect_t)std::max(0.0, 1.0 + mutation_type_ptr_->hemizygous_dominance_coeff_ * selection_coeff_);
	
	// zero out our refcount and per-trait information, which is now kept in a separate buffer
	// FIXME MULTITRAIT: The per-trait info will soon supplant selection_coeff_ and dominance_coeff_; this initialization code needs to be fleshed out
	MutationIndex mutation_index = mutation_block->IndexInBlock(this);
	mutation_block->refcount_buffer_[mutation_index] = 0;
	
	int trait_count = mutation_block->trait_count_;
	MutationTraitInfo *traitInfoBase = mutation_block->trait_info_buffer_ + trait_count * mutation_index;
	
	for (int trait_index = 0; trait_index < trait_count; ++trait_index)
	{
		MutationTraitInfo *traitInfoRec = traitInfoBase + trait_index;
		Trait *trait = species.Traits()[trait_index];
		TraitType traitType = trait->Type();
		
		traitInfoRec->effect_size_ = selection_coeff_;
		traitInfoRec->dominance_coeff_ = dominance_coeff_;
		
		if (traitType == TraitType::kMultiplicative)
		{
			traitInfoRec->homozygous_effect_ = 0.0;
			traitInfoRec->heterozygous_effect_ = 0.0;
			traitInfoRec->hemizygous_effect_ = 0.0;
		}
		else
		{
			traitInfoRec->homozygous_effect_ = 0.0;
			traitInfoRec->heterozygous_effect_ = 0.0;
			traitInfoRec->hemizygous_effect_ = 0.0;
		}
	}
	
#if DEBUG_MUTATIONS
	std::cout << "Mutation constructed: " << this << std::endl;
#endif
	
#ifdef DEBUG_LOCKS_ENABLED
	mutation_block_LOCK.end_critical();
#endif
	
#if 0
	// Dump the memory layout of a Mutation object.  Note this code needs to be synced tightly with the header, since C++ has no real introspection.
	static bool been_here = false;
	
#pragma omp critical (Mutation_layout_dump)
	{
		if (!been_here)
		{
			char *ptr_base = (char *)this;
			char *ptr_mutation_type_ptr_ = (char *)&(this->mutation_type_ptr_);
			char *ptr_position_ = (char *)&(this->position_);
			char *ptr_selection_coeff_ = (char *)&(this->selection_coeff_);
			char *ptr_subpop_index_ = (char *)&(this->subpop_index_);
			char *ptr_origin_tick_ = (char *)&(this->origin_tick_);
			char *ptr_state_ = (char *)&(this->state_);
			char *ptr_nucleotide_ = (char *)&(this->nucleotide_);
			char *ptr_scratch_ = (char *)&(this->scratch_);
			char *ptr_mutation_id_ = (char *)&(this->mutation_id_);
			char *ptr_tag_value_ = (char *)&(this->tag_value_);
			char *ptr_cached_one_plus_sel_ = (char *)&(this->cached_one_plus_sel_);
			char *ptr_cached_one_plus_dom_sel_ = (char *)&(this->cached_one_plus_dom_sel_);
			char *ptr_cached_one_plus_haploiddom_sel_ = (char *)&(this->cached_one_plus_haploiddom_sel_);
			
			std::cout << "Class Mutation memory layout (sizeof(Mutation) == " << sizeof(Mutation) << ") :" << std::endl << std::endl;
			std::cout << "   " << (ptr_mutation_type_ptr_ - ptr_base) << " (" << sizeof(MutationType *) << " bytes): MutationType *mutation_type_ptr_" << std::endl;
			std::cout << "   " << (ptr_position_ - ptr_base) << " (" << sizeof(slim_position_t) << " bytes): const slim_position_t position_" << std::endl;
			std::cout << "   " << (ptr_selection_coeff_ - ptr_base) << " (" << sizeof(slim_effect_t) << " bytes): slim_effect_t selection_coeff_" << std::endl;
			std::cout << "   " << (ptr_subpop_index_ - ptr_base) << " (" << sizeof(slim_objectid_t) << " bytes): slim_objectid_t subpop_index_" << std::endl;
			std::cout << "   " << (ptr_origin_tick_ - ptr_base) << " (" << sizeof(slim_tick_t) << " bytes): const slim_tick_t origin_tick_" << std::endl;
			std::cout << "   " << (ptr_state_ - ptr_base) << " (" << sizeof(int8_t) << " bytes): const int8_t state_" << std::endl;
			std::cout << "   " << (ptr_nucleotide_ - ptr_base) << " (" << sizeof(int8_t) << " bytes): const int8_t nucleotide_" << std::endl;
			std::cout << "   " << (ptr_scratch_ - ptr_base) << " (" << sizeof(int8_t) << " bytes): const int8_t scratch_" << std::endl;
			std::cout << "   " << (ptr_mutation_id_ - ptr_base) << " (" << sizeof(slim_mutationid_t) << " bytes): const slim_mutationid_t mutation_id_" << std::endl;
			std::cout << "   " << (ptr_tag_value_ - ptr_base) << " (" << sizeof(slim_usertag_t) << " bytes): slim_usertag_t tag_value_" << std::endl;
			std::cout << "   " << (ptr_cached_one_plus_sel_ - ptr_base) << " (" << sizeof(slim_effect_t) << " bytes): slim_effect_t cached_one_plus_sel_" << std::endl;
			std::cout << "   " << (ptr_cached_one_plus_dom_sel_ - ptr_base) << " (" << sizeof(slim_effect_t) << " bytes): slim_effect_t cached_one_plus_dom_sel_" << std::endl;
			std::cout << "   " << (ptr_cached_one_plus_haploiddom_sel_ - ptr_base) << " (" << sizeof(slim_effect_t) << " bytes): slim_effect_t cached_one_plus_haploiddom_sel_" << std::endl;
			std::cout << std::endl;
			
			been_here = true;
		}
	}
#endif
}

Mutation::Mutation(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, slim_effect_t p_selection_coeff, slim_effect_t p_dominance_coeff, slim_objectid_t p_subpop_index, slim_tick_t p_tick, int8_t p_nucleotide) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), selection_coeff_(p_selection_coeff), dominance_coeff_(p_dominance_coeff), subpop_index_(p_subpop_index), origin_tick_(p_tick), chromosome_index_(p_chromosome_index), state_(MutationState::kNewMutation), nucleotide_(p_nucleotide), mutation_id_(p_mutation_id)
{
	Species &species = mutation_type_ptr_->species_;
	MutationBlock *mutation_block = species.SpeciesMutationBlock();
	
	// initialize the tag to the "unset" value
	tag_value_ = SLIM_TAG_UNSET_VALUE;
	
	// cache values used by the fitness calculation code for speed; see header
	cached_one_plus_sel_ = (slim_effect_t)std::max(0.0, 1.0 + selection_coeff_);
	cached_one_plus_dom_sel_ = (slim_effect_t)std::max(0.0, 1.0 + dominance_coeff_ * selection_coeff_);
	cached_one_plus_hemizygousdom_sel_ = (slim_effect_t)std::max(0.0, 1.0 + mutation_type_ptr_->hemizygous_dominance_coeff_ * selection_coeff_);
	
	// zero out our refcount and per-trait information, which is now kept in a separate buffer
	// FIXME MULTITRAIT: The per-trait info will soon supplant selection_coeff_ and dominance_coeff_; this initialization code needs to be fleshed out
	MutationIndex mutation_index = mutation_block->IndexInBlock(this);
	mutation_block->refcount_buffer_[mutation_index] = 0;
	
	int trait_count = mutation_block->trait_count_;
	MutationTraitInfo *traitInfoBase = mutation_block->trait_info_buffer_ + trait_count * mutation_index;
	
	for (int trait_index = 0; trait_index < trait_count; ++trait_index)
	{
		MutationTraitInfo *traitInfoRec = traitInfoBase + trait_index;
		Trait *trait = species.Traits()[trait_index];
		TraitType traitType = trait->Type();
		
		traitInfoRec->effect_size_ = selection_coeff_;
		traitInfoRec->dominance_coeff_ = dominance_coeff_;
		
		if (traitType == TraitType::kMultiplicative)
		{
			traitInfoRec->homozygous_effect_ = 0.0;
			traitInfoRec->heterozygous_effect_ = 0.0;
			traitInfoRec->hemizygous_effect_ = 0.0;
		}
		else
		{
			traitInfoRec->homozygous_effect_ = 0.0;
			traitInfoRec->heterozygous_effect_ = 0.0;
			traitInfoRec->hemizygous_effect_ = 0.0;
		}
	}
	
#if DEBUG_MUTATIONS
	std::cout << "Mutation constructed: " << this << std::endl;
#endif
	
	// Since a mutation id was supplied by the caller, we need to ensure that subsequent mutation ids generated do not collide
	// This constructor (unline the other Mutation() constructor above) is presently never called multithreaded,
	// so we just enforce that here.  If that changes, it should start using the debug lock to detect races, as above.
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Mutation::Mutation(): gSLiM_next_mutation_id change");
	
	if (gSLiM_next_mutation_id <= mutation_id_)
		gSLiM_next_mutation_id = mutation_id_ + 1;
}

void Mutation::SelfDelete(void)
{
	// This is called when our retain count reaches zero
	// We destruct ourselves and return our memory to our shared pool
	Species &species = mutation_type_ptr_->species_;
	MutationBlock *mutation_block = species.SpeciesMutationBlock();
	MutationIndex mutation_index = mutation_block->IndexInBlock(this);
	
	this->~Mutation();
	mutation_block->DisposeMutationToBlock(mutation_index);
}

// This is unused except by debugging code and in the debugger itself
std::ostream &operator<<(std::ostream &p_outstream, const Mutation &p_mutation)
{
	p_outstream << "Mutation{mutation_type_ " << p_mutation.mutation_type_ptr_->mutation_type_id_ << ", position_ " << p_mutation.position_ << ", selection_coeff_ " << p_mutation.selection_coeff_ << ", subpop_index_ " << p_mutation.subpop_index_ << ", origin_tick_ " << p_mutation.origin_tick_;
	
	return p_outstream;
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosClass *Mutation::Class(void) const
{
	return gSLiM_Mutation_Class;
}

void Mutation::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassNameForDisplay() << "<" << mutation_id_ << ":" << selection_coeff_ << ">";
}

EidosValue_SP Mutation::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_chromosome:
		{
			Species &species = mutation_type_ptr_->species_;
			const std::vector<Chromosome *> &chromosomes = species.Chromosomes();
			Chromosome *chromosome = chromosomes[chromosome_index_];
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object(chromosome, gSLiM_Chromosome_Class));
		}
		case gID_id:				// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(mutation_id_));
		case gID_isFixed:			// ACCELERATED
			return (((state_ == MutationState::kFixedAndSubstituted) || (state_ == MutationState::kRemovedWithSubstitution)) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_isSegregating:		// ACCELERATED
			return ((state_ == MutationState::kInRegistry) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_mutationType:		// ACCELERATED
			return mutation_type_ptr_->SymbolTableEntry().second;
		case gID_originTick:		// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(origin_tick_));
		case gID_position:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(position_));
		case gID_effect:
		{
			// This is not accelerated, because it's a bit tricky; each mutation could belong to a different species,
			// and thus be associated with a different number of traits.  It isn't expected that this will be a hot path.
			Species &species = mutation_type_ptr_->species_;
			MutationBlock *mutation_block = species.SpeciesMutationBlock();
			MutationIndex mut_index = mutation_block->IndexInBlock(this);
			MutationTraitInfo *trait_info = mutation_block->TraitInfoIndex(mut_index);
			const std::vector<Trait *> &traits = species.Traits();
			size_t trait_count = traits.size();
			
			if (trait_count == 1)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(selection_coeff_));	// FIXME MULTITRAIT
			else if (trait_count == 0)
				return gStaticEidosValue_Float_ZeroVec;
			else
			{
				EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_count);
				
				for (size_t trait_index = 0; trait_index < trait_count; ++trait_index)
				{
					slim_effect_t effect = trait_info[trait_index].effect_size_;
					
					float_result->push_float_no_check(effect);
				}
				
				return EidosValue_SP(float_result);
			}
		}
		case gID_dominance:
		{
			// This is not accelerated, because it's a bit tricky; each mutation could belong to a different species,
			// and thus be associated with a different number of traits.  It isn't expected that this will be a hot path.
			Species &species = mutation_type_ptr_->species_;
			MutationBlock *mutation_block = species.SpeciesMutationBlock();
			MutationIndex mut_index = mutation_block->IndexInBlock(this);
			MutationTraitInfo *trait_info = mutation_block->TraitInfoIndex(mut_index);
			const std::vector<Trait *> &traits = species.Traits();
			size_t trait_count = traits.size();
			
			if (trait_count == 1)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(dominance_coeff_));	// FIXME MULTITRAIT
			else if (trait_count == 0)
				return gStaticEidosValue_Float_ZeroVec;
			else
			{
				EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_count);
				
				for (size_t trait_index = 0; trait_index < trait_count; ++trait_index)
				{
					slim_effect_t dominance = trait_info[trait_index].dominance_coeff_;
					
					float_result->push_float_no_check(dominance);
				}
				
				return EidosValue_SP(float_result);
			}
		}
			
			// variables
		case gID_nucleotide:		// ACCELERATED
		{
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Mutation::GetProperty): property nucleotide is only defined for nucleotide-based mutations." << EidosTerminate();
			
			switch (nucleotide_)
			{
				case 0:	return gStaticEidosValue_StringA;
				case 1:	return gStaticEidosValue_StringC;
				case 2:	return gStaticEidosValue_StringG;
				case 3:	return gStaticEidosValue_StringT;
				default:
					EIDOS_TERMINATION << "ERROR (Mutation::GetProperty): (internal error) unrecognized value for nucleotide_." << EidosTerminate();
			}
		}
		case gID_nucleotideValue:	// ACCELERATED
		{
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Mutation::GetProperty): property nucleotideValue is only defined for nucleotide-based mutations." << EidosTerminate();
			
			switch (nucleotide_)
			{
				case 0:	return gStaticEidosValue_Integer0;
				case 1:	return gStaticEidosValue_Integer1;
				case 2:	return gStaticEidosValue_Integer2;
				case 3:	return gStaticEidosValue_Integer3;
				default:
					EIDOS_TERMINATION << "ERROR (Mutation::GetProperty): (internal error) unrecognized value for nucleotide_." << EidosTerminate();
			}
		}
		case gID_subpopID:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(subpop_index_));
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Mutation::GetProperty): property tag accessed on mutation before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(tag_value));
		}
			
			// all others, including gID_none
		default:
			// Here we implement a special behavior: you can do mutation.<trait>Effect and mutation.<trait>Dominance to access a trait's values directly.
			Species &species = mutation_type_ptr_->species_;
			MutationBlock *mutation_block = species.SpeciesMutationBlock();
			MutationIndex mut_index = mutation_block->IndexInBlock(this);
			MutationTraitInfo *trait_info = mutation_block->TraitInfoIndex(mut_index);
			const std::string &property_string = EidosStringRegistry::StringForGlobalStringID(p_property_id);
			
			if ((property_string.length() > 6) && Eidos_string_hasSuffix(property_string, "Effect"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 6);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(trait_info[trait->Index()].effect_size_));
			}
			else if ((property_string.length() > 9) && Eidos_string_hasSuffix(property_string, "Dominance"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 9);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(trait_info[trait->Index()].dominance_coeff_));
			}
			
			return super::GetProperty(p_property_id);
	}
}

EidosValue *Mutation::GetProperty_Accelerated_id(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->mutation_id_, value_index);
	}
	
	return int_result;
}

EidosValue *Mutation::GetProperty_Accelerated_isFixed(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		logical_result->set_logical_no_check((value->state_ == MutationState::kFixedAndSubstituted) || (value->state_ == MutationState::kRemovedWithSubstitution), value_index);
	}
	
	return logical_result;
}

EidosValue *Mutation::GetProperty_Accelerated_isSegregating(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		logical_result->set_logical_no_check(value->state_ == MutationState::kInRegistry, value_index);
	}
	
	return logical_result;
}

EidosValue *Mutation::GetProperty_Accelerated_nucleotide(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String())->Reserve((int)p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		int8_t nucleotide = value->nucleotide_;
		
		if (nucleotide == -1)
			EIDOS_TERMINATION << "ERROR (Mutation::GetProperty_Accelerated_nucleotide): property nucleotide is only defined for nucleotide-based mutations." << EidosTerminate();
		
		if (nucleotide == 0)
			string_result->PushString(gStr_A);
		else if (nucleotide == 1)
			string_result->PushString(gStr_C);
		else if (nucleotide == 2)
			string_result->PushString(gStr_G);
		else if (nucleotide == 3)
			string_result->PushString(gStr_T);
	}
	
	return string_result;
}

EidosValue *Mutation::GetProperty_Accelerated_nucleotideValue(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		int8_t nucleotide = value->nucleotide_;
		
		if (nucleotide == -1)
			EIDOS_TERMINATION << "ERROR (Mutation::GetProperty_Accelerated_nucleotideValue): property nucleotideValue is only defined for nucleotide-based mutations." << EidosTerminate();
		
		int_result->set_int_no_check(nucleotide, value_index);
	}
	
	return int_result;
}

EidosValue *Mutation::GetProperty_Accelerated_originTick(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->origin_tick_, value_index);
	}
	
	return int_result;
}

EidosValue *Mutation::GetProperty_Accelerated_position(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->position_, value_index);
	}
	
	return int_result;
}

EidosValue *Mutation::GetProperty_Accelerated_subpopID(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->subpop_index_, value_index);
	}
	
	return int_result;
}

EidosValue *Mutation::GetProperty_Accelerated_tag(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (Mutation::GetProperty_Accelerated_tag): property tag accessed on mutation before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

EidosValue *Mutation::GetProperty_Accelerated_mutationType(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Object *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_MutationType_Class))->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Mutation *value = (Mutation *)(p_values[value_index]);
		
		object_result->set_object_element_no_check_NORR(value->mutation_type_ptr_, value_index);
	}
	
	return object_result;
}

void Mutation::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_nucleotide:
		{
			const std::string &nucleotide = ((EidosValue_String &)p_value).StringRefAtIndex_NOCAST(0, nullptr);
			
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Mutation::SetProperty): property nucleotide is only defined for nucleotide-based mutations." << EidosTerminate();
			
			if (nucleotide == gStr_A)		nucleotide_ = 0;
			else if (nucleotide == gStr_C)	nucleotide_ = 1;
			else if (nucleotide == gStr_G)	nucleotide_ = 2;
			else if (nucleotide == gStr_T)	nucleotide_ = 3;
			else EIDOS_TERMINATION << "ERROR (Mutation::SetProperty): property nucleotide may only be set to 'A', 'C', 'G', or 'T'." << EidosTerminate();
			return;
		}
		case gID_nucleotideValue:
		{
			int64_t nucleotide = p_value.IntAtIndex_NOCAST(0, nullptr);
			
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Mutation::SetProperty): property nucleotideValue is only defined for nucleotide-based mutations." << EidosTerminate();
			if ((nucleotide < 0) || (nucleotide > 3))
				EIDOS_TERMINATION << "ERROR (Mutation::SetProperty): property nucleotideValue may only be set to 0 (A), 1 (C), 2 (G), or 3 (T)." << EidosTerminate();
			
			nucleotide_ = (int8_t)nucleotide;
			return;
		}
		case gID_subpopID:			// ACCELERATED
		{
			slim_objectid_t value = SLiMCastToObjectidTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			subpop_index_ = value;
			return;
		}
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			tag_value_ = value;
			return;
		}
			
		default:
		{
			// Here we implement a special behavior: you can do mutation.<trait>Effect and mutation.<trait>Dominance to access a trait's values directly.
			Species &species = mutation_type_ptr_->species_;
			MutationBlock *mutation_block = species.SpeciesMutationBlock();
			MutationIndex mut_index = mutation_block->IndexInBlock(this);
			MutationTraitInfo *trait_info = mutation_block->TraitInfoIndex(mut_index);
			const std::string &property_string = EidosStringRegistry::StringForGlobalStringID(p_property_id);
			
			if ((property_string.length() > 6) && Eidos_string_hasSuffix(property_string, "Effect"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 6);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
				{
					trait_info[trait->Index()].effect_size_ = (slim_effect_t)p_value.FloatAtIndex_NOCAST(0, nullptr);
					return;
				}
			}
			else if ((property_string.length() > 9) && Eidos_string_hasSuffix(property_string, "Dominance"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 9);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
				{
					trait_info[trait->Index()].dominance_coeff_ = (slim_effect_t)p_value.FloatAtIndex_NOCAST(0, nullptr);
					return;
				}
			}
			
			return super::SetProperty(p_property_id, p_value);
		}
	}
}

void Mutation::SetProperty_Accelerated_subpopID(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
#pragma unused (p_property_id)
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex_NOCAST(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Mutation *)(p_values[value_index]))->subpop_index_ = SLiMCastToObjectidTypeOrRaise(source_value);
	}
	else
	{
		const int64_t *source_data = p_source.IntData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Mutation *)(p_values[value_index]))->subpop_index_ = SLiMCastToObjectidTypeOrRaise(source_data[value_index]);
	}
}

void Mutation::SetProperty_Accelerated_tag(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
#pragma unused (p_property_id)
	// SLiMCastToUsertagTypeOrRaise() is a no-op at present
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex_NOCAST(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Mutation *)(p_values[value_index]))->tag_value_ = source_value;
	}
	else
	{
		const int64_t *source_data = p_source.IntData();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Mutation *)(p_values[value_index]))->tag_value_ = source_data[value_index];
	}
}

EidosValue_SP Mutation::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_effectForTrait:	return ExecuteMethod_effectForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_dominanceForTrait:	return ExecuteMethod_dominanceForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_setMutationType:	return ExecuteMethod_setMutationType(p_method_id, p_arguments, p_interpreter);
		default:					return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (float)effectForTrait([Nio<Trait> trait = NULL])
//
EidosValue_SP Mutation::ExecuteMethod_effectForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	
	// get the trait indices, with bounds-checking
	Species &species = mutation_type_ptr_->species_;
	std::vector<int64_t> trait_indices;
	species.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "effectForTrait");
	
	// get the trait info for this mutation
	MutationBlock *mutation_block = species.SpeciesMutationBlock();
	MutationIndex mut_index = mutation_block->IndexInBlock(this);
	MutationTraitInfo *trait_info = mutation_block->TraitInfoIndex(mut_index);
	
	if (trait_indices.size() == 1)
	{
		int64_t trait_index = trait_indices[0];
		slim_effect_t effect = trait_info[trait_index].effect_size_;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(effect));
	}
	else
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_indices.size());
		
		for (int64_t trait_index : trait_indices)
		{
			slim_effect_t effect = trait_info[trait_index].effect_size_;
			
			float_result->push_float_no_check(effect);
		}
		
		return EidosValue_SP(float_result);
	}
}

//	*********************	- (float)dominanceForTrait([Nio<Trait> trait = NULL])
//
EidosValue_SP Mutation::ExecuteMethod_dominanceForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	
	// get the trait indices, with bounds-checking
	Species &species = mutation_type_ptr_->species_;
	std::vector<int64_t> trait_indices;
	species.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "dominanceForTrait");
	
	// get the trait info for this mutation
	MutationBlock *mutation_block = species.SpeciesMutationBlock();
	MutationIndex mut_index = mutation_block->IndexInBlock(this);
	MutationTraitInfo *trait_info = mutation_block->TraitInfoIndex(mut_index);
	
	if (trait_indices.size() == 1)
	{
		int64_t trait_index = trait_indices[0];
		slim_effect_t dominance = trait_info[trait_index].dominance_coeff_;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(dominance));
	}
	else
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_indices.size());
		
		for (int64_t trait_index : trait_indices)
		{
			slim_effect_t dominance = trait_info[trait_index].dominance_coeff_;
			
			float_result->push_float_no_check(dominance);
		}
		
		return EidosValue_SP(float_result);
	}
}

//	*********************	- (void)setMutationType(io<MutationType>$ mutType)
//
EidosValue_SP Mutation::ExecuteMethod_setMutationType(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	Species &species = mutation_type_ptr_->species_;
	
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, &species.community_, &species, "setMutationType()");		// SPECIES CONSISTENCY CHECK
	
	if (mutation_type_ptr->nucleotide_based_ != mutation_type_ptr_->nucleotide_based_)
		EIDOS_TERMINATION << "ERROR (Mutation::ExecuteMethod_setMutationType): setMutationType() does not allow a mutation to be changed from nucleotide-based to non-nucleotide-based or vice versa." << EidosTerminate();
	
	// We take just the mutation type pointer; if the user wants a new selection coefficient, they can do that themselves
	mutation_type_ptr_ = mutation_type_ptr;
	
	// If we are non-neutral, make sure the mutation type knows it is now also non-neutral
	if (selection_coeff_ != 0.0)
		mutation_type_ptr_->all_pure_neutral_DFE_ = false;
	
	// cache values used by the fitness calculation code for speed; see header
	cached_one_plus_sel_ = (slim_effect_t)std::max(0.0, 1.0 + selection_coeff_);
	cached_one_plus_dom_sel_ = (slim_effect_t)std::max(0.0, 1.0 + dominance_coeff_ * selection_coeff_);
	cached_one_plus_hemizygousdom_sel_ = (slim_effect_t)std::max(0.0, 1.0 + mutation_type_ptr_->hemizygous_dominance_coeff_ * selection_coeff_);
	
	return gStaticEidosValueVOID;
}


//
//	Mutation_Class
//
#pragma mark -
#pragma mark Mutation_Class
#pragma mark -

EidosClass *gSLiM_Mutation_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *Mutation_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Mutation_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_chromosome,				true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Chromosome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,						true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_id));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_isFixed,				true,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_isFixed));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_isSegregating,			true,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_isSegregating));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationType,			true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_MutationType_Class))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_mutationType));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotide,				false,	kEidosValueMaskString | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_nucleotide));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotideValue,		false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_nucleotideValue));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_originTick,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_originTick));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_position,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_position));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_effect,					true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_dominance,				true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopID,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_subpopID)->DeclareAcceleratedSet(Mutation::SetProperty_Accelerated_subpopID));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_tag)->DeclareAcceleratedSet(Mutation::SetProperty_Accelerated_tag));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Mutation_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Mutation_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_effectForTrait, kEidosValueMaskFloat))->AddIntObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_dominanceForTrait, kEidosValueMaskFloat))->AddIntObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_setEffectForTrait, kEidosValueMaskVOID))->AddIntObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL)->AddNumeric_ON("effect", gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_setDominanceForTrait, kEidosValueMaskVOID))->AddIntObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL)->AddNumeric_ON("dominance", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMutationType, kEidosValueMaskVOID))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

EidosValue_SP Mutation_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
	switch (p_method_id)
	{
		case gID_setEffectForTrait:			return ExecuteMethod_setEffectForTrait(p_method_id, p_target, p_arguments, p_interpreter);
		case gID_setDominanceForTrait:		return ExecuteMethod_setDominanceForTrait(p_method_id, p_target, p_arguments, p_interpreter);
		default:
			return super::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_interpreter);
	}
}

//	*********************	+ (void)setEffectForTrait([Nio<Trait> trait = NULL], [Nif effect = NULL])
//
EidosValue_SP Mutation_Class::ExecuteMethod_setEffectForTrait(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	EidosValue *effect_value = p_arguments[1].get();
	
	int mutations_count = p_target->Count();
	int effect_count = effect_value->Count();
	
	if (mutations_count == 0)
		return gStaticEidosValueVOID;
	
	Mutation **mutations_buffer = (Mutation **)p_target->ObjectData();
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForMutations(p_target);
	
	if (!species)
		EIDOS_TERMINATION << "ERROR (Mutation_Class::ExecuteMethod_setEffectForTrait): setEffectForTrait() requires that all mutations belong to the same species." << EidosTerminate();
	
	MutationBlock *mutation_block = species->SpeciesMutationBlock();
	
	// get the trait indices, with bounds-checking
	std::vector<int64_t> trait_indices;
	species->GetTraitIndicesFromEidosValue(trait_indices, trait_value, "setEffectForTrait");
	int trait_count = (int)trait_indices.size();
	
	if (effect_value->Type() == EidosValueType::kValueNULL)
	{
		// pattern 1: drawing a default effect value for each trait in one or more mutations
		for (int64_t trait_index : trait_indices)
		{
			for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
			{
				Mutation *mut = mutations_buffer[mutation_index];
				MutationType *muttype = mut->mutation_type_ptr_;
				MutationIndex mut_index = mutation_block->IndexInBlock(mut);
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
				slim_effect_t effect = (slim_effect_t)muttype->DrawEffectForTrait(trait_index);
				
				mut_trait_info[trait_index].effect_size_ = effect;
			}
		}
	}
	else if (effect_count == 1)
	{
		// pattern 2: setting a single effect value across one or more traits in one or more mutations
		slim_effect_t effect = static_cast<slim_effect_t>(effect_value->NumericAtIndex_NOCAST(0, nullptr));
		
		if (trait_count == 1)
		{
			// optimized case for one trait
			int64_t trait_index = trait_indices[0];
			
			for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
			{
				Mutation *mut = mutations_buffer[mutation_index];
				MutationIndex mut_index = mutation_block->IndexInBlock(mut);
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
				
				mut_trait_info[trait_index].effect_size_ = effect;
			}
		}
		else
		{
			for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
			{
				Mutation *mut = mutations_buffer[mutation_index];
				MutationIndex mut_index = mutation_block->IndexInBlock(mut);
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
				
				for (int64_t trait_index : trait_indices)
					mut_trait_info[trait_index].effect_size_ = effect;
			}
		}
	}
	else if (effect_count == trait_count)
	{
		// pattern 3: setting one effect value per trait, in one or more mutations
		int effect_index = 0;
		
		for (int64_t trait_index : trait_indices)
		{
			slim_effect_t effect = static_cast<slim_effect_t>(effect_value->NumericAtIndex_NOCAST(effect_index++, nullptr));
			
			for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
			{
				Mutation *mut = mutations_buffer[mutation_index];
				MutationIndex mut_index = mutation_block->IndexInBlock(mut);
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
				
				mut_trait_info[trait_index].effect_size_ = effect;
			}
		}
	}
	else if (effect_count == trait_count * mutations_count)
	{
		// pattern 4: setting different effect values for each trait in each mutation; in this case,
		// all effects for the specified traits in a given mutation are given consecutively
		if (effect_value->Type() == EidosValueType::kValueInt)
		{
			// integer effect values
			const int64_t *effects_int = effect_value->IntData();
			
			if (trait_count == 1)
			{
				// optimized case for one trait
				int64_t trait_index = trait_indices[0];
				
				for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
				{
					Mutation *mut = mutations_buffer[mutation_index];
					MutationIndex mut_index = mutation_block->IndexInBlock(mut);
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
					
					mut_trait_info[trait_index].effect_size_ = static_cast<slim_effect_t>(*(effects_int++));
				}
			}
			else
			{
				for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
				{
					Mutation *mut = mutations_buffer[mutation_index];
					MutationIndex mut_index = mutation_block->IndexInBlock(mut);
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
					
					for (int64_t trait_index : trait_indices)
						mut_trait_info[trait_index].effect_size_ = static_cast<slim_effect_t>(*(effects_int++));
				}
			}
		}
		else
		{
			// float effect values
			const double *effects_float = effect_value->FloatData();
			
			if (trait_count == 1)
			{
				// optimized case for one trait
				int64_t trait_index = trait_indices[0];
				
				for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
				{
					Mutation *mut = mutations_buffer[mutation_index];
					MutationIndex mut_index = mutation_block->IndexInBlock(mut);
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
					
					mut_trait_info[trait_index].effect_size_ = static_cast<slim_effect_t>(*(effects_float++));
				}
			}
			else
			{
				for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
				{
					Mutation *mut = mutations_buffer[mutation_index];
					MutationIndex mut_index = mutation_block->IndexInBlock(mut);
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
					
					for (int64_t trait_index : trait_indices)
						mut_trait_info[trait_index].effect_size_ = static_cast<slim_effect_t>(*(effects_float++));
				}
			}
		}
	}
	else
		EIDOS_TERMINATION << "ERROR (Mutation_Class::ExecuteMethod_setEffectForTrait): setEffectForTrait() requires that effect be (a) NULL, requesting an effect value drawn from the mutation's mutation type for each trait, (b) singleton, providing one effect value for all traits, (c) equal in length to the number of traits in the species, providing one effect value per trait, or (d) equal in length to the number of traits times the number of target mutations, providing one effect value per trait per mutation." << EidosTerminate();
	
	return gStaticEidosValueVOID;
}

//	*********************	+ (void)setDominanceForTrait([Nio<Trait> trait = NULL], [Nif dominance = NULL])
//
EidosValue_SP Mutation_Class::ExecuteMethod_setDominanceForTrait(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	EidosValue *dominance_value = p_arguments[1].get();
	
	int mutations_count = p_target->Count();
	int dominance_count = dominance_value->Count();
	
	if (mutations_count == 0)
		return gStaticEidosValueVOID;
	
	Mutation **mutations_buffer = (Mutation **)p_target->ObjectData();
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForMutations(p_target);
	
	if (!species)
		EIDOS_TERMINATION << "ERROR (Mutation_Class::ExecuteMethod_setDominanceForTrait): setDominanceForTrait() requires that all mutations belong to the same species." << EidosTerminate();
	
	MutationBlock *mutation_block = species->SpeciesMutationBlock();
	
	// get the trait indices, with bounds-checking
	std::vector<int64_t> trait_indices;
	species->GetTraitIndicesFromEidosValue(trait_indices, trait_value, "setDominanceForTrait");
	int trait_count = (int)trait_indices.size();
	
	// note there is intentionally no bounds check of dominance coefficients
	if (dominance_value->Type() == EidosValueType::kValueNULL)
	{
		// pattern 1: drawing a default dominance value for each trait in one or more mutations
		for (int64_t trait_index : trait_indices)
		{
			for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
			{
				Mutation *mut = mutations_buffer[mutation_index];
				MutationType *muttype = mut->mutation_type_ptr_;
				MutationIndex mut_index = mutation_block->IndexInBlock(mut);
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
				slim_effect_t dominance = (slim_effect_t)muttype->DefaultDominanceForTrait(trait_index);
				
				mut_trait_info[trait_index].dominance_coeff_ = dominance;
			}
		}
	}
	else if (dominance_count == 1)
	{
		// pattern 2: setting a single dominance value across one or more traits in one or more mutations
		slim_effect_t dominance = static_cast<slim_effect_t>(dominance_value->NumericAtIndex_NOCAST(0, nullptr));
		
		if (trait_count == 1)
		{
			// optimized case for one trait
			int64_t trait_index = trait_indices[0];
			
			for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
			{
				Mutation *mut = mutations_buffer[mutation_index];
				MutationIndex mut_index = mutation_block->IndexInBlock(mut);
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
				
				mut_trait_info[trait_index].dominance_coeff_ = dominance;
			}
		}
		else
		{
			for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
			{
				Mutation *mut = mutations_buffer[mutation_index];
				MutationIndex mut_index = mutation_block->IndexInBlock(mut);
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
				
				for (int64_t trait_index : trait_indices)
					mut_trait_info[trait_index].dominance_coeff_ = dominance;
			}
		}
	}
	else if (dominance_count == trait_count)
	{
		// pattern 3: setting one dominance value per trait, in one or more mutations
		int dominance_index = 0;
		
		for (int64_t trait_index : trait_indices)
		{
			slim_effect_t dominance = static_cast<slim_effect_t>(dominance_value->NumericAtIndex_NOCAST(dominance_index++, nullptr));
			
			for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
			{
				Mutation *mut = mutations_buffer[mutation_index];
				MutationIndex mut_index = mutation_block->IndexInBlock(mut);
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
				
				mut_trait_info[trait_index].dominance_coeff_ = dominance;
			}
		}
	}
	else if (dominance_count == trait_count * mutations_count)
	{
		// pattern 4: setting different dominance values for each trait in each mutation; in this case,
		// all dominances for the specified traits in a given mutation are given consecutively
		if (dominance_value->Type() == EidosValueType::kValueInt)
		{
			// integer dominance values
			const int64_t *dominances_int = dominance_value->IntData();
			
			if (trait_count == 1)
			{
				// optimized case for one trait
				int64_t trait_index = trait_indices[0];
				
				for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
				{
					Mutation *mut = mutations_buffer[mutation_index];
					MutationIndex mut_index = mutation_block->IndexInBlock(mut);
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
					
					mut_trait_info[trait_index].dominance_coeff_ = static_cast<slim_effect_t>(*(dominances_int++));
				}
			}
			else
			{
				for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
				{
					Mutation *mut = mutations_buffer[mutation_index];
					MutationIndex mut_index = mutation_block->IndexInBlock(mut);
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
					
					for (int64_t trait_index : trait_indices)
						mut_trait_info[trait_index].dominance_coeff_ = static_cast<slim_effect_t>(*(dominances_int++));
				}
			}
		}
		else
		{
			// float dominance values
			const double *dominances_float = dominance_value->FloatData();
			
			if (trait_count == 1)
			{
				// optimized case for one trait
				int64_t trait_index = trait_indices[0];
				
				for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
				{
					Mutation *mut = mutations_buffer[mutation_index];
					MutationIndex mut_index = mutation_block->IndexInBlock(mut);
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
					
					mut_trait_info[trait_index].dominance_coeff_ = static_cast<slim_effect_t>(*(dominances_float++));
				}
			}
			else
			{
				for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
				{
					Mutation *mut = mutations_buffer[mutation_index];
					MutationIndex mut_index = mutation_block->IndexInBlock(mut);
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoIndex(mut_index);
					
					for (int64_t trait_index : trait_indices)
						mut_trait_info[trait_index].dominance_coeff_ = static_cast<slim_effect_t>(*(dominances_float++));
				}
			}
		}
	}
	else
		EIDOS_TERMINATION << "ERROR (Mutation_Class::ExecuteMethod_setDominanceForTrait): setDominanceForTrait() requires that dominance be (a) NULL, requesting the default dominance coefficient from the mutation's mutation type for each trait, (b) singleton, providing one dominance value for all traits, (c) equal in length to the number of traits in the species, providing one dominance value per trait, or (d) equal in length to the number of traits times the number of target mutations, providing one dominance value per trait per mutation." << EidosTerminate();
	
	return gStaticEidosValueVOID;
}



































































