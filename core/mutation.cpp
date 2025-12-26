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

// This constructor is used when making a new mutation with effects and dominances provided by the caller; FIXME MULTITRAIT: needs to take a whole vector of each, per trait!
Mutation::Mutation(MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, slim_effect_t p_selection_coeff, slim_effect_t p_dominance_coeff, slim_objectid_t p_subpop_index, slim_tick_t p_tick, int8_t p_nucleotide) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), subpop_index_(p_subpop_index), origin_tick_(p_tick), chromosome_index_(p_chromosome_index), state_(MutationState::kNewMutation), nucleotide_(p_nucleotide), mutation_id_(gSLiM_next_mutation_id++)
{
#ifdef DEBUG_LOCKS_ENABLED
	mutation_block_LOCK.start_critical(2);
#endif
	
	Species &species = mutation_type_ptr_->species_;
	const std::vector<Trait *> &traits = species.Traits();
	MutationBlock *mutation_block = species.SpeciesMutationBlock();
	
	// initialize the tag to the "unset" value
	tag_value_ = SLIM_TAG_UNSET_VALUE;
	
	// zero out our refcount and per-trait information, which is now kept in a separate buffer
	MutationIndex mutation_index = mutation_block->IndexInBlock(this);
	mutation_block->refcount_buffer_[mutation_index] = 0;
	
	int trait_count = mutation_block->trait_count_;
	MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForIndex(mutation_index);
	
	// Below basically does the work of calling SetEffect() and SetDominance(), more efficiently since
	// this is critical path.  See those methods for more comments on what is happening here.
	
	is_neutral_ = true;		// will be set to false below as needed
	
	for (int trait_index = 0; trait_index < trait_count; ++trait_index)
	{
		MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
		Trait *trait = traits[trait_index];
		TraitType traitType = trait->Type();
		
		// FIXME MULTITRAIT: This constructor needs to change to have a whole vector of trait information passed in, for effect and dominance
		slim_effect_t effect = trait_index ? p_selection_coeff : 0.0;
		slim_effect_t dominance = trait_index ? p_dominance_coeff : 0.5;
		slim_effect_t hemizygous_dominance = mutation_type_ptr_->DefaultHemizygousDominanceForTrait(trait_index);	// FIXME MULTITRAIT: This needs to come in from outside, probably
		
		traitInfoRec->effect_size_ = effect;
		traitInfoRec->dominance_coeff_ = dominance;
		traitInfoRec->hemizygous_dominance_coeff_ = hemizygous_dominance;
		
		if (effect != 0.0)
		{
			is_neutral_ = false;
			
			species.pure_neutral_ = false;						// let the sim know that it is no longer a pure-neutral simulation
			mutation_type_ptr_->all_pure_neutral_DES_ = false;	// let the mutation type for this mutation know that it is no longer pure neutral
			species.nonneutral_change_counter_++;				// nonneutral mutation caches need revalidation; // FIXME MULTITRAIT only the mutrun(s) this is added to should be recached!
			
			if (traitType == TraitType::kMultiplicative)
			{
				traitInfoRec->homozygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + effect);
				traitInfoRec->heterozygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + dominance * effect);
				traitInfoRec->hemizygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + hemizygous_dominance * effect);
			}
			else	// (traitType == TraitType::kAdditive)
			{
				traitInfoRec->homozygous_effect_ = (slim_effect_t)(2.0f * effect);
				traitInfoRec->heterozygous_effect_ = (slim_effect_t)(2.0f * dominance * effect);
				traitInfoRec->hemizygous_effect_ = (slim_effect_t)(2.0f * hemizygous_dominance * effect);
			}
		}
		else	// (effect == 0.0)
		{
			if (traitType == TraitType::kMultiplicative)
			{
				traitInfoRec->homozygous_effect_ = (slim_effect_t)1.0;
				traitInfoRec->heterozygous_effect_ = (slim_effect_t)1.0;
				traitInfoRec->hemizygous_effect_ = (slim_effect_t)1.0;
			}
			else	// (traitType == TraitType::kAdditive)
			{
				traitInfoRec->homozygous_effect_ = (slim_effect_t)0.0;
				traitInfoRec->heterozygous_effect_ = (slim_effect_t)0.0;
				traitInfoRec->hemizygous_effect_ = (slim_effect_t)0.0;
			}
		}
	}
	
#if DEBUG_MUTATIONS
	std::cout << "Mutation constructed: " << this << std::endl;
#endif
	
#ifdef DEBUG_LOCKS_ENABLED
	mutation_block_LOCK.end_critical();
#endif
}

// This constructor is used when making a new mutation with effects drawn from each trait's DES, and dominance taken from each trait's default dominance coefficient, both from the given mutation type
Mutation::Mutation(MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, slim_objectid_t p_subpop_index, slim_tick_t p_tick, int8_t p_nucleotide) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), subpop_index_(p_subpop_index), origin_tick_(p_tick), chromosome_index_(p_chromosome_index), state_(MutationState::kNewMutation), nucleotide_(p_nucleotide), mutation_id_(gSLiM_next_mutation_id++)
{
#ifdef DEBUG_LOCKS_ENABLED
	mutation_block_LOCK.start_critical(2);
#endif
	
	Species &species = mutation_type_ptr_->species_;
	const std::vector<Trait *> &traits = species.Traits();
	MutationBlock *mutation_block = species.SpeciesMutationBlock();
	
	// initialize the tag to the "unset" value
	tag_value_ = SLIM_TAG_UNSET_VALUE;
	
	// zero out our refcount and per-trait information, which is now kept in a separate buffer
	MutationIndex mutation_index = mutation_block->IndexInBlock(this);
	mutation_block->refcount_buffer_[mutation_index] = 0;
	
	int trait_count = mutation_block->trait_count_;
	MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForIndex(mutation_index);
	
	// Below basically does the work of calling SetEffect() and SetDominance(), more efficiently since
	// this is critical path.  See those methods for more comments on what is happening here.
	
	is_neutral_ = true;		// will be set to false below as needed
	
	if (mutation_type_ptr_->all_pure_neutral_DES_)
	{
		// The DES of the mutation type is pure neutral, so we don't need to do any draws; we can short-circuit
		// most of the work here and just set up neutral effects for all of the traits.
		for (int trait_index = 0; trait_index < trait_count; ++trait_index)
		{
			MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
			Trait *trait = traits[trait_index];
			TraitType traitType = trait->Type();
			
			traitInfoRec->effect_size_ = 0.0;
			traitInfoRec->dominance_coeff_ = mutation_type_ptr_->DefaultDominanceForTrait(trait_index);
			traitInfoRec->hemizygous_dominance_coeff_ = mutation_type_ptr_->DefaultHemizygousDominanceForTrait(trait_index);
			
			if (traitType == TraitType::kMultiplicative)
			{
				traitInfoRec->homozygous_effect_ = (slim_effect_t)1.0;
				traitInfoRec->heterozygous_effect_ = (slim_effect_t)1.0;
				traitInfoRec->hemizygous_effect_ = (slim_effect_t)1.0;
			}
			else	// (traitType == TraitType::kAdditive)
			{
				traitInfoRec->homozygous_effect_ = (slim_effect_t)0.0;
				traitInfoRec->heterozygous_effect_ = (slim_effect_t)0.0;
				traitInfoRec->hemizygous_effect_ = (slim_effect_t)0.0;
			}
		}
	}
	else
	{
		// The DES of the mutation type is not pure neutral.  Note that species.pure_neutral_ might still be true
		// at this point; the mutation type for this mutation might not be used by any genomic element type,
		// because we might be getting called by addNewDrawn() mutation for a type that is otherwise unused.
		for (int trait_index = 0; trait_index < trait_count; ++trait_index)
		{
			MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
			Trait *trait = traits[trait_index];
			TraitType traitType = trait->Type();
			
			slim_effect_t effect = mutation_type_ptr_->DrawEffectForTrait(trait_index);
			slim_effect_t dominance = mutation_type_ptr_->DefaultDominanceForTrait(trait_index);
			slim_effect_t hemizygous_dominance = mutation_type_ptr_->DefaultHemizygousDominanceForTrait(trait_index);
			
			traitInfoRec->effect_size_ = effect;
			traitInfoRec->dominance_coeff_ = dominance;
			traitInfoRec->hemizygous_dominance_coeff_ = hemizygous_dominance;
			
			if (effect != 0.0)
			{
				is_neutral_ = false;
				
				species.pure_neutral_ = false;						// let the sim know that it is no longer a pure-neutral simulation
				species.nonneutral_change_counter_++;				// nonneutral mutation caches need revalidation; // FIXME MULTITRAIT only the mutrun(s) this is added to should be recached!
				
				if (traitType == TraitType::kMultiplicative)
				{
					traitInfoRec->homozygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + effect);
					traitInfoRec->heterozygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + dominance * effect);
					traitInfoRec->hemizygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + hemizygous_dominance * effect);
				}
				else	// (traitType == TraitType::kAdditive)
				{
					traitInfoRec->homozygous_effect_ = (slim_effect_t)(2.0f * effect);
					traitInfoRec->heterozygous_effect_ = (slim_effect_t)(2.0f * dominance * effect);
					traitInfoRec->hemizygous_effect_ = (slim_effect_t)(2.0f * hemizygous_dominance * effect);
				}
			}
			else	// (effect == 0.0)
			{
				if (traitType == TraitType::kMultiplicative)
				{
					traitInfoRec->homozygous_effect_ = (slim_effect_t)1.0;
					traitInfoRec->heterozygous_effect_ = (slim_effect_t)1.0;
					traitInfoRec->hemizygous_effect_ = (slim_effect_t)1.0;
				}
				else	// (traitType == TraitType::kAdditive)
				{
					traitInfoRec->homozygous_effect_ = (slim_effect_t)0.0;
					traitInfoRec->heterozygous_effect_ = (slim_effect_t)0.0;
					traitInfoRec->hemizygous_effect_ = (slim_effect_t)0.0;
				}
			}
		}
	}
	
#if DEBUG_MUTATIONS
	std::cout << "Mutation constructed: " << this << std::endl;
#endif
	
#ifdef DEBUG_LOCKS_ENABLED
	mutation_block_LOCK.end_critical();
#endif
}

// This constructor is used when making a new mutation with effects and dominances provided by the caller, *and* a mutation id provided by the caller
Mutation::Mutation(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, slim_effect_t p_selection_coeff, slim_effect_t p_dominance_coeff, slim_objectid_t p_subpop_index, slim_tick_t p_tick, int8_t p_nucleotide) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), subpop_index_(p_subpop_index), origin_tick_(p_tick), chromosome_index_(p_chromosome_index), state_(MutationState::kNewMutation), nucleotide_(p_nucleotide), mutation_id_(p_mutation_id)
{
	Species &species = mutation_type_ptr_->species_;
	MutationBlock *mutation_block = species.SpeciesMutationBlock();
	
	// initialize the tag to the "unset" value
	tag_value_ = SLIM_TAG_UNSET_VALUE;
	
	// zero out our refcount and per-trait information, which is now kept in a separate buffer
	MutationIndex mutation_index = mutation_block->IndexInBlock(this);
	mutation_block->refcount_buffer_[mutation_index] = 0;
	
	int trait_count = mutation_block->trait_count_;
	MutationTraitInfo *mut_trait_info = mutation_block->trait_info_buffer_ + trait_count * mutation_index;
	
	// Below basically does the work of calling SetEffect() and SetDominance(), more efficiently since
	// this is critical path.  See those methods for more comments on what is happening here.
	
	is_neutral_ = true;		// will be set to false by EffectChanged() as needed
	
	for (int trait_index = 0; trait_index < trait_count; ++trait_index)
	{
		MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
		Trait *trait = species.Traits()[trait_index];
		TraitType traitType = trait->Type();
		
		// FIXME MULTITRAIT: The per-trait info will soon supplant selection_coeff_ and dominance_coeff_; this initialization code needs to be fleshed out
		slim_effect_t effect = (trait_index == 0) ? p_selection_coeff : 0.0;
		slim_effect_t dominance = (trait_index == 0) ? p_dominance_coeff : 0.5;
		slim_effect_t hemizygous_dominance = mutation_type_ptr_->DefaultHemizygousDominanceForTrait(trait_index);
		
		traitInfoRec->effect_size_ = effect;
		traitInfoRec->dominance_coeff_ = dominance;
		traitInfoRec->hemizygous_dominance_coeff_ = hemizygous_dominance;
		
		if (effect != 0.0)
		{
			is_neutral_ = false;
			
			species.pure_neutral_ = false;						// let the sim know that it is no longer a pure-neutral simulation
			mutation_type_ptr_->all_pure_neutral_DES_ = false;	// let the mutation type for this mutation know that it is no longer pure neutral
			species.nonneutral_change_counter_++;				// nonneutral mutation caches need revalidation; // FIXME MULTITRAIT only the mutrun(s) this is added to should be recached!
			
			if (traitType == TraitType::kMultiplicative)
			{
				traitInfoRec->homozygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + effect);
				traitInfoRec->heterozygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + dominance * effect);
				traitInfoRec->hemizygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + hemizygous_dominance * effect);
			}
			else	// (traitType == TraitType::kAdditive)
			{
				traitInfoRec->homozygous_effect_ = (slim_effect_t)(2.0f * effect);
				traitInfoRec->heterozygous_effect_ = (slim_effect_t)(2.0f * dominance * effect);
				traitInfoRec->hemizygous_effect_ = (slim_effect_t)(2.0f * hemizygous_dominance * effect);
			}
		}
		else	// (effect == 0.0)
		{
			if (traitType == TraitType::kMultiplicative)
			{
				traitInfoRec->homozygous_effect_ = (slim_effect_t)1.0;
				traitInfoRec->heterozygous_effect_ = (slim_effect_t)1.0;
				traitInfoRec->hemizygous_effect_ = (slim_effect_t)1.0;
			}
			else	// (traitType == TraitType::kAdditive)
			{
				traitInfoRec->homozygous_effect_ = (slim_effect_t)0.0;
				traitInfoRec->heterozygous_effect_ = (slim_effect_t)0.0;
				traitInfoRec->hemizygous_effect_ = (slim_effect_t)0.0;
			}
		}
	}
	
#if DEBUG_MUTATIONS
	std::cout << "Mutation constructed: " << this << std::endl;
#endif
	
	// Since a mutation id was supplied by the caller, we need to ensure that subsequent mutation ids generated do not collide
	// This constructor (unlike the other Mutation() constructor above) is presently never called multithreaded,
	// so we just enforce that here.  If that changes, it should start using the debug lock to detect races, as above.
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("Mutation::Mutation(): gSLiM_next_mutation_id change");
	
	if (gSLiM_next_mutation_id <= mutation_id_)
		gSLiM_next_mutation_id = mutation_id_ + 1;
}

// This should be called whenever a mutation effect is changed; it handles the necessary recaching
void Mutation::SetEffect(TraitType traitType, MutationTraitInfo *traitInfoRec, slim_effect_t p_new_effect)
{
	slim_effect_t old_effect = traitInfoRec->effect_size_;
	slim_effect_t dominance = traitInfoRec->dominance_coeff_;
	slim_effect_t hemizygous_dominance = traitInfoRec->hemizygous_dominance_coeff_;
	
	traitInfoRec->effect_size_ = p_new_effect;
	
	if (p_new_effect != 0.0)
	{
		if (old_effect == 0.0)
		{
			// This mutation is no longer neutral; various observers care about that change
			is_neutral_ = false;
			
			Species &species = mutation_type_ptr_->species_;
			
			species.pure_neutral_ = false;						// let the sim know that it is no longer a pure-neutral simulation
			mutation_type_ptr_->all_pure_neutral_DES_ = false;	// let the mutation type for this mutation know that it is no longer pure neutral
			species.nonneutral_change_counter_++;				// nonneutral mutation caches need revalidation; // FIXME MULTITRAIT should have per chromosome or even narrower flags
		}
		
		// cache values used by the fitness calculation code for speed; see header
		
		if (traitType == TraitType::kMultiplicative)
		{
			// For multiplicative traits, we clamp the lower end to 0.0; you can't be more lethal than lethal, and we
			// never want to go negative and then go positive again by multiplying in another negative effect.  There
			// is admittedly a philosophical issue here; if a multiplicative trait represented simply some abstract
			// trait with no direct connection to fitness, then maybe clamping here would not make sense?  But even
			// then, negative effects don't really seem to me to make sense there, so I think this is good.
			traitInfoRec->homozygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + p_new_effect);							// 1 + s
			traitInfoRec->heterozygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + dominance * p_new_effect);			// 1 + hs
			traitInfoRec->hemizygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + hemizygous_dominance * p_new_effect);	// 1 + hs (using hemizygous h)
		}
		else	// (traitType == TraitType::kAdditive)
		{
			// For additive traits, the baseline of the trait is arbitrary and there is no cutoff.
			traitInfoRec->homozygous_effect_ = (slim_effect_t)(2.0f * p_new_effect);										// 2a
			traitInfoRec->heterozygous_effect_ = (slim_effect_t)(2.0f * dominance * p_new_effect);							// 2ha
			traitInfoRec->hemizygous_effect_ = (slim_effect_t)(2.0f * hemizygous_dominance * p_new_effect);					// 2ha (using hemizygous h)
		}
	}
	else	// p_new_effect == 0.0
	{
		if (old_effect != 0.0)
		{
			// Changing from non-neutral to neutral; various observers care about that
			// Note that we cannot set is_neutral_ and other such flags to true here, because only this trait's
			// effect has changed to neutral; other trait effects might be non-neutral, which we don't check
			Species &species = mutation_type_ptr_->species_;
			
			species.nonneutral_change_counter_++;				// nonneutral mutation caches need revalidation; // FIXME MULTITRAIT should have per chromosome or even narrower flags
			
			// cache values used by the fitness calculation code for speed; see header
			// for a neutral trait, we can set up this info very quickly
			if (traitType == TraitType::kMultiplicative)
			{
				traitInfoRec->homozygous_effect_ = (slim_effect_t)1.0;
				traitInfoRec->heterozygous_effect_ = (slim_effect_t)1.0;
				traitInfoRec->hemizygous_effect_ = (slim_effect_t)1.0;
			}
			else	// (traitType == TraitType::kAdditive)
			{
				traitInfoRec->homozygous_effect_ = (slim_effect_t)0.0;
				traitInfoRec->heterozygous_effect_ = (slim_effect_t)0.0;
				traitInfoRec->hemizygous_effect_ = (slim_effect_t)0.0;
			}
		}
	}
}

// This should be called whenever a mutation dominance is changed; it handles the necessary recaching
void Mutation::SetDominance(TraitType traitType, MutationTraitInfo *traitInfoRec, slim_effect_t p_new_dominance)
{
	traitInfoRec->dominance_coeff_ = p_new_dominance;
	
	// We only need to recache the heterozygous_effect_ values, since only they are affected by the change in
	// dominance coefficient.  Changing dominance has no effect on is_neutral_ or any of the other is-neutral
	// flags.  So this is very simple.
	
	if (traitType == TraitType::kMultiplicative)
	{
		traitInfoRec->heterozygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + p_new_dominance * traitInfoRec->effect_size_);
	}
	else	// (traitType == TraitType::kAdditive)
	{
		traitInfoRec->heterozygous_effect_ = (slim_effect_t)(2.0f * p_new_dominance * traitInfoRec->effect_size_);
	}
}

void Mutation::SetHemizygousDominance(TraitType traitType, MutationTraitInfo *traitInfoRec, slim_effect_t p_new_dominance)
{
	traitInfoRec->hemizygous_dominance_coeff_ = p_new_dominance;
	
	// We only need to recache the hemizygous_effect_ values, since only they are affected by the change in
	// dominance coefficient.  Changing dominance has no effect on is_neutral_ or any of the other is-neutral
	// flags.  So this is very simple.
	
	if (traitType == TraitType::kMultiplicative)
	{
		traitInfoRec->hemizygous_effect_ = (slim_effect_t)std::max(0.0f, 1.0f + p_new_dominance * traitInfoRec->effect_size_);
	}
	else	// (traitType == TraitType::kAdditive)
	{
		traitInfoRec->hemizygous_effect_ = (slim_effect_t)(2.0f * p_new_dominance * traitInfoRec->effect_size_);
	}
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
	Species &species = p_mutation.mutation_type_ptr_->species_;
	MutationBlock *mutation_block = species.SpeciesMutationBlock();
	MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(&p_mutation);
	
	p_outstream << "Mutation{mutation_type_ " << p_mutation.mutation_type_ptr_->mutation_type_id_ << ", position_ " << p_mutation.position_ << ", effect_size_ " << mut_trait_info[0].effect_size_ << ", subpop_index_ " << p_mutation.subpop_index_ << ", origin_tick_ " << p_mutation.origin_tick_;
	
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
	// BCH 10/20/2025: Changing from selection_coeff_ to position_ here, as part of multitrait work
	p_ostream << Class()->ClassNameForDisplay() << "<" << mutation_id_ << ":" << position_ << ">";
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
			MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(this);
			const std::vector<Trait *> &traits = species.Traits();
			size_t trait_count = traits.size();
			
			if (trait_count == 1)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(mut_trait_info[0].effect_size_));
			else if (trait_count == 0)
				return gStaticEidosValue_Float_ZeroVec;
			else
			{
				EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_count);
				
				for (size_t trait_index = 0; trait_index < trait_count; ++trait_index)
				{
					slim_effect_t effect = mut_trait_info[trait_index].effect_size_;
					
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
			MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(this);
			const std::vector<Trait *> &traits = species.Traits();
			size_t trait_count = traits.size();
			
			if (trait_count == 1)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(mut_trait_info[0].dominance_coeff_));
			else if (trait_count == 0)
				return gStaticEidosValue_Float_ZeroVec;
			else
			{
				EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_count);
				
				for (size_t trait_index = 0; trait_index < trait_count; ++trait_index)
				{
					slim_effect_t dominance = mut_trait_info[trait_index].dominance_coeff_;
					
					float_result->push_float_no_check(dominance);
				}
				
				return EidosValue_SP(float_result);
			}
		}
		case gID_hemizygousDominance:
		{
			// This is not accelerated, because it's a bit tricky; each mutation could belong to a different species,
			// and thus be associated with a different number of traits.  It isn't expected that this will be a hot path.
			Species &species = mutation_type_ptr_->species_;
			MutationBlock *mutation_block = species.SpeciesMutationBlock();
			MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(this);
			const std::vector<Trait *> &traits = species.Traits();
			size_t trait_count = traits.size();
			
			if (trait_count == 1)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(mut_trait_info[0].hemizygous_dominance_coeff_));
			else if (trait_count == 0)
				return gStaticEidosValue_Float_ZeroVec;
			else
			{
				EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_count);
				
				for (size_t trait_index = 0; trait_index < trait_count; ++trait_index)
				{
					slim_effect_t dominance = mut_trait_info[trait_index].hemizygous_dominance_coeff_;
					
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
			// Here we implement a special behavior: you can do mutation.<trait-name>Effect, mutation.<trait-name>Dominance,
			// and mutation.<trait-name>HemizygousDominance to access a trait's values directly.
			// NOTE: This mechanism also needs to be maintained in Species::ExecuteContextFunction_initializeTrait().
			// NOTE: This mechanism also needs to be maintained in SLiMTypeInterpreter::_TypeEvaluate_FunctionCall_Internal().
			Species &species = mutation_type_ptr_->species_;
			MutationBlock *mutation_block = species.SpeciesMutationBlock();
			MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(this);
			const std::string &property_string = EidosStringRegistry::StringForGlobalStringID(p_property_id);
			
			if ((property_string.length() > 6) && Eidos_string_hasSuffix(property_string, "Effect"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 6);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(mut_trait_info[trait->Index()].effect_size_));
			}
			else if ((property_string.length() > 19) && Eidos_string_hasSuffix(property_string, "HemizygousDominance"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 19);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(mut_trait_info[trait->Index()].hemizygous_dominance_coeff_));
			}
			else if ((property_string.length() > 9) && Eidos_string_hasSuffix(property_string, "Dominance"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 9);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(mut_trait_info[trait->Index()].dominance_coeff_));
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
			// Here we implement a special behavior: you can do mutation.<trait-name>Effect and mutation.<trait-name>Dominance to access a trait's values directly.
			// NOTE: This mechanism also needs to be maintained in Species::ExecuteContextFunction_initializeTrait().
			// NOTE: This mechanism also needs to be maintained in SLiMTypeInterpreter::_TypeEvaluate_FunctionCall_Internal().
			Species &species = mutation_type_ptr_->species_;
			MutationBlock *mutation_block = species.SpeciesMutationBlock();
			MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(this);
			const std::string &property_string = EidosStringRegistry::StringForGlobalStringID(p_property_id);
			
			if ((property_string.length() > 6) && Eidos_string_hasSuffix(property_string, "Effect"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 6);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
				{
					MutationTraitInfo *traitInfoRec = mut_trait_info + trait->Index();
					slim_effect_t new_effect = (slim_effect_t)p_value.FloatAtIndex_NOCAST(0, nullptr);
					
					SetEffect(trait->Type(), traitInfoRec, new_effect);
					return;
				}
			}
			else if ((property_string.length() > 19) && Eidos_string_hasSuffix(property_string, "HemizygousDominance"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 19);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
				{
					MutationTraitInfo *traitInfoRec = mut_trait_info + trait->Index();
					slim_effect_t new_dominance = (slim_effect_t)p_value.FloatAtIndex_NOCAST(0, nullptr);
					
					SetHemizygousDominance(trait->Type(), traitInfoRec, new_dominance);
					return;
				}
			}
			else if ((property_string.length() > 9) && Eidos_string_hasSuffix(property_string, "Dominance"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 9);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
				{
					MutationTraitInfo *traitInfoRec = mut_trait_info + trait->Index();
					slim_effect_t new_dominance = (slim_effect_t)p_value.FloatAtIndex_NOCAST(0, nullptr);
					
					SetDominance(trait->Type(), traitInfoRec, new_dominance);
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
		case gID_effectForTrait:				return ExecuteMethod_effectForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_dominanceForTrait:				return ExecuteMethod_dominanceForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_hemizygousDominanceForTrait:	return ExecuteMethod_hemizygousDominanceForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_setMutationType:				return ExecuteMethod_setMutationType(p_method_id, p_arguments, p_interpreter);
		default:								return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (float)effectForTrait([Niso<Trait> trait = NULL])
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
	MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(this);
	
	if (trait_indices.size() == 1)
	{
		int64_t trait_index = trait_indices[0];
		slim_effect_t effect = mut_trait_info[trait_index].effect_size_;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(effect));
	}
	else
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_indices.size());
		
		for (int64_t trait_index : trait_indices)
		{
			slim_effect_t effect = mut_trait_info[trait_index].effect_size_;
			
			float_result->push_float_no_check(effect);
		}
		
		return EidosValue_SP(float_result);
	}
}

//	*********************	- (float)dominanceForTrait([Niso<Trait> trait = NULL])
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
	MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(this);
	
	if (trait_indices.size() == 1)
	{
		int64_t trait_index = trait_indices[0];
		slim_effect_t dominance = mut_trait_info[trait_index].dominance_coeff_;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(dominance));
	}
	else
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_indices.size());
		
		for (int64_t trait_index : trait_indices)
		{
			slim_effect_t dominance = mut_trait_info[trait_index].dominance_coeff_;
			
			float_result->push_float_no_check(dominance);
		}
		
		return EidosValue_SP(float_result);
	}
}

//	*********************	- (float)hemizygousDominanceForTrait([Niso<Trait> trait = NULL])
//
EidosValue_SP Mutation::ExecuteMethod_hemizygousDominanceForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	
	// get the trait indices, with bounds-checking
	Species &species = mutation_type_ptr_->species_;
	std::vector<int64_t> trait_indices;
	species.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "hemizygousDominanceForTrait");
	
	// get the trait info for this mutation
	MutationBlock *mutation_block = species.SpeciesMutationBlock();
	MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(this);
	
	if (trait_indices.size() == 1)
	{
		int64_t trait_index = trait_indices[0];
		slim_effect_t dominance = mut_trait_info[trait_index].hemizygous_dominance_coeff_;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(dominance));
	}
	else
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_indices.size());
		
		for (int64_t trait_index : trait_indices)
		{
			slim_effect_t dominance = mut_trait_info[trait_index].hemizygous_dominance_coeff_;
			
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
	// FIXME MULTITRAIT: I think it might be useful for MutationType to keep a flag separately for each trait, whether *that* trait is all_pure_neutral_DES_ or not
	//int trait_count = species.TraitCount();
	//MutationBlock *mutation_block = species.SpeciesMutationBlock();
	//MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(this);
	
	if (!is_neutral_)
		mutation_type_ptr_->all_pure_neutral_DES_ = false;
	
	// Changing the mutation type no longer changes the dominance coefficient or the hemizygous dominance
	// coefficient, so there are no longer any side effects on trait effects / fitness to be managed here.
	
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
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hemizygousDominance,	true,	kEidosValueMaskFloat)));
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
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_effectForTrait, kEidosValueMaskFloat))->AddIntStringObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_dominanceForTrait, kEidosValueMaskFloat))->AddIntStringObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_hemizygousDominanceForTrait, kEidosValueMaskFloat))->AddIntStringObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_setEffectForTrait, kEidosValueMaskVOID))->AddIntStringObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL)->AddNumeric_ON("effect", gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_setDominanceForTrait, kEidosValueMaskVOID))->AddIntStringObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL)->AddNumeric_ON("dominance", gStaticEidosValueNULL));
		methods->emplace_back((EidosClassMethodSignature *)(new EidosClassMethodSignature(gStr_setHemizygousDominanceForTrait, kEidosValueMaskVOID))->AddIntStringObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL)->AddNumeric_ON("dominance", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setMutationType, kEidosValueMaskVOID))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

EidosValue_SP Mutation_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
	switch (p_method_id)
	{
		case gID_setEffectForTrait:					return ExecuteMethod_setEffectForTrait(p_method_id, p_target, p_arguments, p_interpreter);
		case gID_setDominanceForTrait:
		case gID_setHemizygousDominanceForTrait:	return ExecuteMethod_setDominanceForTrait(p_method_id, p_target, p_arguments, p_interpreter);
		default:
			return super::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_interpreter);
	}
}

//	*********************	+ (void)setEffectForTrait([Niso<Trait> trait = NULL], [Nif effect = NULL])
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
	const std::vector<Trait *> &traits = species->Traits();
	
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
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
				MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
				slim_effect_t effect = (slim_effect_t)muttype->DrawEffectForTrait(trait_index);
				
				mut->SetEffect(traits[trait_index]->Type(), traitInfoRec, effect);
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
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
				MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
				
				mut->SetEffect(traits[trait_index]->Type(), traitInfoRec, effect);
			}
		}
		else
		{
			for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
			{
				Mutation *mut = mutations_buffer[mutation_index];
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
				
				for (int64_t trait_index : trait_indices)
				{
					MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
					
					mut->SetEffect(traits[trait_index]->Type(), traitInfoRec, effect);
				}
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
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
				MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
				
				mut->SetEffect(traits[trait_index]->Type(), traitInfoRec, effect);
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
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
					MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
					slim_effect_t effect = static_cast<slim_effect_t>(*(effects_int++));
					
					mut->SetEffect(traits[trait_index]->Type(), traitInfoRec, effect);
				}
			}
			else
			{
				for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
				{
					Mutation *mut = mutations_buffer[mutation_index];
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
					
					for (int64_t trait_index : trait_indices)
					{
						MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
						slim_effect_t effect = static_cast<slim_effect_t>(*(effects_int++));
						
						mut->SetEffect(traits[trait_index]->Type(), traitInfoRec, effect);
					}
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
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
					MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
					slim_effect_t effect = static_cast<slim_effect_t>(*(effects_float++));
					
					mut->SetEffect(traits[trait_index]->Type(), traitInfoRec, effect);
				}
			}
			else
			{
				for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
				{
					Mutation *mut = mutations_buffer[mutation_index];
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
					
					for (int64_t trait_index : trait_indices)
					{
						MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
						slim_effect_t effect = static_cast<slim_effect_t>(*(effects_float++));
						
						mut->SetEffect(traits[trait_index]->Type(), traitInfoRec, effect);
					}
				}
			}
		}
	}
	else
		EIDOS_TERMINATION << "ERROR (Mutation_Class::ExecuteMethod_setEffectForTrait): setEffectForTrait() requires that effect be (a) NULL, requesting an effect value drawn from the mutation's mutation type for each trait, (b) singleton, providing one effect value for all traits, (c) equal in length to the number of traits in the species, providing one effect value per trait, or (d) equal in length to the number of traits times the number of target mutations, providing one effect value per trait per mutation." << EidosTerminate();
	
	return gStaticEidosValueVOID;
}

//	*********************	+ (void)setDominanceForTrait([Niso<Trait> trait = NULL], [Nif dominance = NULL])
//	*********************	+ (void)setHemizygousDominanceForTrait([Niso<Trait> trait = NULL], [Nif dominance = NULL])
//
EidosValue_SP Mutation_Class::ExecuteMethod_setDominanceForTrait(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_interpreter)
	const char *method_name = (p_method_id == gID_setDominanceForTrait) ? "setDominanceForTrait" : "setHemizygousDominanceForTrait"; 
	
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
		EIDOS_TERMINATION << "ERROR (Mutation_Class::ExecuteMethod_" << method_name << "): " << method_name << "() requires that all mutations belong to the same species." << EidosTerminate();
	
	MutationBlock *mutation_block = species->SpeciesMutationBlock();
	const std::vector<Trait *> &traits = species->Traits();
	
	// get the trait indices, with bounds-checking
	std::vector<int64_t> trait_indices;
	species->GetTraitIndicesFromEidosValue(trait_indices, trait_value, method_name);
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
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
				MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
				slim_effect_t dominance = ((p_method_id == gID_setDominanceForTrait) ? muttype->DefaultDominanceForTrait(trait_index) : muttype->DefaultHemizygousDominanceForTrait(trait_index));
				
				if (p_method_id == gID_setDominanceForTrait)
					mut->SetDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
				else
					mut->SetHemizygousDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
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
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
				MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
				
				if (p_method_id == gID_setDominanceForTrait)
					mut->SetDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
				else
					mut->SetHemizygousDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
			}
		}
		else
		{
			for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
			{
				Mutation *mut = mutations_buffer[mutation_index];
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
				
				for (int64_t trait_index : trait_indices)
				{
					MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
					
					if (p_method_id == gID_setDominanceForTrait)
						mut->SetDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
					else
						mut->SetHemizygousDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
				}
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
				MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
				MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
				
				if (p_method_id == gID_setDominanceForTrait)
					mut->SetDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
				else
					mut->SetHemizygousDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
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
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
					MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
					slim_effect_t dominance = static_cast<slim_effect_t>(*(dominances_int++));
					
					if (p_method_id == gID_setDominanceForTrait)
						mut->SetDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
					else
						mut->SetHemizygousDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
				}
			}
			else
			{
				for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
				{
					Mutation *mut = mutations_buffer[mutation_index];
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
					
					for (int64_t trait_index : trait_indices)
					{
						MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
						slim_effect_t dominance = static_cast<slim_effect_t>(*(dominances_int++));
						
						if (p_method_id == gID_setDominanceForTrait)
							mut->SetDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
						else
							mut->SetHemizygousDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
					}
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
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
					MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
					slim_effect_t dominance = static_cast<slim_effect_t>(*(dominances_float++));
					
					if (p_method_id == gID_setDominanceForTrait)
						mut->SetDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
					else
						mut->SetHemizygousDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
				}
			}
			else
			{
				for (int mutation_index = 0; mutation_index < mutations_count; ++mutation_index)
				{
					Mutation *mut = mutations_buffer[mutation_index];
					MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(mut);
					
					for (int64_t trait_index : trait_indices)
					{
						MutationTraitInfo *traitInfoRec = mut_trait_info + trait_index;
						slim_effect_t dominance = static_cast<slim_effect_t>(*(dominances_float++));
						
						if (p_method_id == gID_setDominanceForTrait)
							mut->SetDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
						else
							mut->SetHemizygousDominance(traits[trait_index]->Type(), traitInfoRec, dominance);
					}
				}
			}
		}
	}
	else
		EIDOS_TERMINATION << "ERROR (Mutation_Class::ExecuteMethod_" << method_name << "): " << method_name << "() requires that dominance be (a) NULL, requesting the default" << ((p_method_id == gID_setDominanceForTrait) ? " " : " hemizygous ") << "dominance coefficient from the mutation's mutation type for each trait, (b) singleton, providing one dominance value for all traits, (c) equal in length to the number of traits in the species, providing one dominance value per trait, or (d) equal in length to the number of traits times the number of target mutations, providing one dominance value per trait per mutation." << EidosTerminate();
	
	return gStaticEidosValueVOID;
}



































































