//
//  substitution.cpp
//  SLiM
//
//  Created by Ben Haller on 12/13/14.
//  Copyright (c) 2014-2026 Benjamin C. Haller.  All rights reserved.
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


#include "substitution.h"
#include "slim_globals.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "species.h"
#include "mutation_block.h"
#include "trait.h"

#include <iostream>
#include <algorithm>
#include <string>
#include <vector>


#pragma mark -
#pragma mark Substitution
#pragma mark -

void Substitution::_CheckHemizygousAccumulation(void)
{
	// This method must be called from all constructors to maintain the hemi_sub_accumulation_occurred_ flag
	// of Chromosome, which is used to check for a violation of the rules around substitution accumulation.
	
	// BCH 9/7/2026: OK, tricky stuff here.  With separate substitution offsets for males and females, we
	// can now handle substitution accumulation for sex chromosomes, including any hemizygous dominance
	// coefficient, because we know how sex chromosomes occur in males versus females.  But we can't handle
	// substitution accumulation for autosomes, with a hemizygous dominance coefficient != 1.0, if the
	// substitution occurs in a chromosome that is found hemizygously, because we don't know what pattern
	// of occurrence will be followed, and we have no way of representing that pattern of occurrence.  So
	// we need to detect that and raise an error.  We do that with two flags, per chromosome.  One says
	// "a substitution has occurred, with hemizygous dominance != 1.0, for this chromosome".  The other
	// says "an individual has been observed that has a null haplosome for this chromosome".  If both flags
	// are true for a given autosome, substitution accumulation cannot be used and an error results.  (Note
	// that hemi_sub_accumulation_occurred_ does not need to be per-trait, because null_haplosome_observed_
	// is not per-trait in any case, and so if hemi_sub_accumulation_occurred_ is true for *any* trait and
	// null_haplosome_observed_ is also true then the error condition has been met.)
	
	Species &species = mutation_type_ptr_->species_;
	Chromosome *associated_chromosome = species.Chromosomes()[chromosome_index_];
	ChromosomeType chromosome_type = associated_chromosome->Type();
	
	if ((chromosome_type == ChromosomeType::kA_DiploidAutosome) || (chromosome_type == ChromosomeType::kH_HaploidAutosome))
	{
		slim_trait_index_t trait_count = species.TraitCount();
		const std::vector<Trait *> &traits = species.Traits();
		
		for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
		{
			Trait *trait = traits[trait_index];
			
			if (trait->SubstitutionAccumulationEnabled())
			{
				if (trait_info_[trait_index].effect_size_ == 0.0)
					continue;
				
				if (trait_info_[trait_index].hemizygous_dominance_coeff_ == 1.0)
					continue;
				
				associated_chromosome->hemi_sub_accumulation_occurred_ = true;
				
				if (associated_chromosome->null_haplosome_observed_)
					EIDOS_TERMINATION << "ERROR (Substitution::_CheckHemizygousAccumulation): " << "substitution accumulation cannot occur for mutations that (1) are non-neutral for a given trait, and (2) are associated with a given autosome, IF (3) the given trait has a hemizygous dominance coefficient != 1.0, and (4) the given autosome is represented by a null haplosome in any individual.  Under these conditions, the effect of the substitution cannot be reliably represented by the trait's substitution offset(s).  To fix this error, you must change your model so that one of the four preconditions for this error is no longer met, OR -- most commonly -- you must turn off substitution for the mutation type(s) that trigger this problem by setting their convertToSubstitution property to F.  (Note that turning off substitution accumulation is typically NOT a valid fix, since then substitution will cause trait values to omit the trait effects of the mutations that get substituted, unless you compensate for that yourself in script.)" << EidosTerminate();
			}
		}
	}
}

Substitution::Substitution(Mutation &p_mutation, slim_tick_t p_fixation_tick) :
EidosDictionaryRetained(), mutation_type_ptr_(p_mutation.mutation_type_ptr_), position_(p_mutation.position_), subpop_index_(p_mutation.subpop_index_), origin_tick_(p_mutation.origin_tick_), fixation_tick_(p_fixation_tick), chromosome_index_(p_mutation.chromosome_index_), is_neutral_for_all_traits_(p_mutation.is_neutral_for_all_traits_), is_neutral_for_direct_fitness_traits_(p_mutation.is_neutral_for_direct_fitness_traits_), independent_dominance_for_all_traits_(p_mutation.independent_dominance_for_all_traits_), independent_dominance_for_any_traits_(p_mutation.independent_dominance_for_any_traits_), nucleotide_(p_mutation.nucleotide_), mutation_id_(p_mutation.mutation_id_), tag_value_(p_mutation.tag_value_)
	
{
	AddKeysAndValuesFrom(&p_mutation);
	// No call to ContentsChanged() here; we know we use Dictionary not DataFrame, and Mutation already vetted the dictionary
	
	// Copy per-trait information over from the mutation object
	Species &species = mutation_type_ptr_->species_;
	MutationBlock *mutation_block = species.SpeciesMutationBlock();
	MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(&p_mutation);
	slim_trait_index_t trait_count = species.TraitCount();
	
	trait_info_ = (SubstitutionTraitInfo *)malloc(trait_count * sizeof(SubstitutionTraitInfo));
	
	for (slim_trait_index_t trait_index = 0; trait_index < trait_count; trait_index++)
	{
		trait_info_[trait_index].effect_size_ = mut_trait_info[trait_index].effect_size_;
		trait_info_[trait_index].dominance_coeff_UNSAFE_ = mut_trait_info[trait_index].dominance_coeff_UNSAFE_;	// can be NAN
		trait_info_[trait_index].hemizygous_dominance_coeff_ = mut_trait_info[trait_index].hemizygous_dominance_coeff_;
	}
	
	_CheckHemizygousAccumulation();
	
#if DEBUG
	SelfConsistencyCheck(" in Substitution::Substitution()");
#endif
}

Substitution::Substitution(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, slim_effect_t p_selection_coeff, slim_effect_t p_dominance_coeff, slim_objectid_t p_subpop_index, slim_tick_t p_tick, slim_tick_t p_fixation_tick, int8_t p_nucleotide) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), subpop_index_(p_subpop_index), origin_tick_(p_tick), fixation_tick_(p_fixation_tick), chromosome_index_(p_chromosome_index), nucleotide_(p_nucleotide), mutation_id_(p_mutation_id), tag_value_(SLIM_TAG_UNSET_VALUE)
{
	// FIXME MULTITRAIT: This code path is hit when loading substitutions from an output file, also needs to initialize the multitrait info; this is just a
	// placeholder.  The file being read in ought to specify per-trait values, which hasn't happened yet, so there are lots of details to be worked out...
	Species &species = mutation_type_ptr_->species_;
	slim_trait_index_t trait_count = species.TraitCount();
	
	trait_info_ = (SubstitutionTraitInfo *)malloc(trait_count * sizeof(SubstitutionTraitInfo));
	
	trait_info_[0].effect_size_ = p_selection_coeff;
	trait_info_[0].dominance_coeff_UNSAFE_ = p_dominance_coeff;		// can be NAN
	trait_info_[0].hemizygous_dominance_coeff_ = mutation_type_ptr_->DefaultHemizygousDominanceForTrait(0);		// FIXME MULTITRAIT: needs to be passed in
	
	for (slim_trait_index_t trait_index = 1; trait_index < trait_count; trait_index++)
	{
		trait_info_[trait_index].effect_size_ = 0.0;					// FIXME MULTITRAIT: needs to be passed in
		trait_info_[trait_index].dominance_coeff_UNSAFE_ = 0.0;			// FIXME MULTITRAIT: needs to be passed in
		trait_info_[trait_index].hemizygous_dominance_coeff_ = 1.0;		// FIXME MULTITRAIT: needs to be passed in
	}
	
	EvaluateFlags();
	
	_CheckHemizygousAccumulation();
	
#if DEBUG
	SelfConsistencyCheck(" in Substitution::Substitution()");
#endif
}

Substitution::Substitution(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, MutationTableMetadataRec *p_metadata_ptr, slim_tick_t p_fixation_tick) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), subpop_index_(p_metadata_ptr->subpop_index_), origin_tick_(p_metadata_ptr->origin_tick_), fixation_tick_(p_fixation_tick), chromosome_index_(p_chromosome_index), nucleotide_(p_metadata_ptr->nucleotide_), mutation_id_(p_mutation_id), tag_value_(SLIM_TAG_UNSET_VALUE)
{
	// This constructor is called by Species::__CreateMutationsFromTabulation() to construct a mutation directly
	// from a MutationTableMetadataRec pointer into the tree sequence's mutation metadata table
	Species &species = mutation_type_ptr_->species_;
	slim_trait_index_t trait_count = species.TraitCount();
	
	trait_info_ = (SubstitutionTraitInfo *)malloc(trait_count * sizeof(SubstitutionTraitInfo));
	
	for (slim_trait_index_t trait_index = 0; trait_index < trait_count; trait_index++)
	{
		trait_info_[trait_index].effect_size_ = p_metadata_ptr->per_trait_[trait_index].effect_size_;
		trait_info_[trait_index].dominance_coeff_UNSAFE_ = p_metadata_ptr->per_trait_[trait_index].dominance_coeff_;
		trait_info_[trait_index].hemizygous_dominance_coeff_ = p_metadata_ptr->per_trait_[trait_index].hemizygous_dominance_coeff_;
	}
	
	EvaluateFlags();
	
	_CheckHemizygousAccumulation();
	
#if DEBUG
	SelfConsistencyCheck(" in Substitution::Substitution()");
#endif
}

void Substitution::EvaluateFlags(void)
{
	// This mirrors Substitution::SelfConsistencyCheck(); essentially it sets up flags as expected by that method.
	Species &species = mutation_type_ptr_->species_;
	const std::vector<Trait *> &traits = species.Traits();
	slim_trait_index_t trait_count = species.TraitCount();
	
	// keep flags in local variables until we are ready to set their final values
	bool is_neutral_for_all_traits = true;
	bool is_neutral_for_direct_fitness_traits = true;
	bool independent_dominance_for_all_traits = true;
	bool independent_dominance_for_any_traits = false;
	
	for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
	{
		SubstitutionTraitInfo &traitInfoRec = trait_info_[trait_index];
		
		if (traitInfoRec.effect_size_ != (slim_effect_t)0.0)
		{
			is_neutral_for_all_traits = false;
			
			if (traits[trait_index]->HasDirectFitnessEffect())
				is_neutral_for_direct_fitness_traits = false;
			
			if (std::isnan(traitInfoRec.dominance_coeff_UNSAFE_))
				independent_dominance_for_any_traits = true;
			else
				independent_dominance_for_all_traits = false;
		}
	}
	
	is_neutral_for_all_traits_ = is_neutral_for_all_traits;
	is_neutral_for_direct_fitness_traits_ = is_neutral_for_direct_fitness_traits;
	independent_dominance_for_all_traits_ = independent_dominance_for_all_traits;
	independent_dominance_for_any_traits_ = independent_dominance_for_any_traits;
}

void Substitution::SelfConsistencyCheck(const std::string &p_message_end) const
{
	if (!mutation_type_ptr_)
		EIDOS_TERMINATION << "ERROR (Substitution::SelfConsistencyCheck): (internal error) mutation_type_ptr_ is nullptr" << p_message_end << "." << EidosTerminate();
	if (!trait_info_)
		EIDOS_TERMINATION << "ERROR (Substitution::SelfConsistencyCheck): (internal error) trait_info_ is nullptr" << p_message_end << "." << EidosTerminate();
	
	Species &species = mutation_type_ptr_->species_;
	const std::vector<Trait *> &traits = species.Traits();
	slim_trait_index_t trait_count = species.TraitCount();
	bool all_neutral_effects = true;
	bool all_neutral_direct_effects = true;
	bool independent_dominance_for_all_traits = true;
	bool independent_dominance_for_any_traits = false;
	
	for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
	{
		Trait *trait = traits[trait_index];
		SubstitutionTraitInfo &traitInfoRec = trait_info_[trait_index];
		
		if (!std::isfinite(traitInfoRec.effect_size_))
			EIDOS_TERMINATION << "ERROR (Substitution::SelfConsistencyCheck): substitution effect size is non-finite" << p_message_end << "." << EidosTerminate();
		if (std::isinf(traitInfoRec.dominance_coeff_UNSAFE_))	// NAN is legal
			EIDOS_TERMINATION << "ERROR (Substitution::SelfConsistencyCheck): substitution dominance is infinite" << p_message_end << "." << EidosTerminate();
		if (!std::isfinite(traitInfoRec.hemizygous_dominance_coeff_))
			EIDOS_TERMINATION << "ERROR (Substitution::SelfConsistencyCheck): substitution hemizygous dominance is non-finite" << p_message_end << "." << EidosTerminate();
		
		if (traitInfoRec.effect_size_ != (slim_effect_t)0.0)
		{
			all_neutral_effects = false;
			
			if (trait->HasDirectFitnessEffect())
				all_neutral_direct_effects = false;
			
			if (std::isnan(traitInfoRec.dominance_coeff_UNSAFE_))
				independent_dominance_for_any_traits = true;
			else
				independent_dominance_for_all_traits = false;
		}
	}
	
	if ((is_neutral_for_all_traits_ && !all_neutral_effects) ||
		(!is_neutral_for_all_traits_ && all_neutral_effects))
		EIDOS_TERMINATION << "ERROR (Substitution::SelfConsistencyCheck): substitution neutrality state is inconsistent" << p_message_end << "." << EidosTerminate();
	
	if ((is_neutral_for_direct_fitness_traits_ && !all_neutral_direct_effects) ||
		(!is_neutral_for_direct_fitness_traits_ && all_neutral_direct_effects))
		EIDOS_TERMINATION << "ERROR (Substitution::SelfConsistencyCheck): substitution direct-effect neutrality state is inconsistent" << p_message_end << "." << EidosTerminate();
	
	if ((independent_dominance_for_all_traits && !independent_dominance_for_all_traits_) ||
		(!independent_dominance_for_all_traits && independent_dominance_for_all_traits_))
		EIDOS_TERMINATION << "ERROR (Substitution::SelfConsistencyCheck): independent_dominance_for_all_traits_ state is inconsistent" << p_message_end << "." << EidosTerminate();
	
	if ((independent_dominance_for_any_traits && !independent_dominance_for_any_traits_) ||
		(!independent_dominance_for_any_traits && independent_dominance_for_any_traits_))
		EIDOS_TERMINATION << "ERROR (Substitution::SelfConsistencyCheck): independent_dominance_for_any_traits_ state is inconsistent" << p_message_end << "." << EidosTerminate();
}

slim_effect_t Substitution::RealizedDominanceForTrait(Trait *p_trait)
{
	slim_trait_index_t trait_index = p_trait->Index();
	SubstitutionTraitInfo &traitInfoRec = trait_info_[trait_index];
	
	if (std::isnan(traitInfoRec.dominance_coeff_UNSAFE_))
	{
		// NAN indicates independent dominance and needs to be handled specially here
		if (p_trait->Type() == TraitType::kAdditive)
		{
			// for additive and logistic traits independent dominance is always 0.5
			return 0.5;
		}
		else
		{
			// for multiplicative traits the dominance is calculated as (sqrt(1+s)-1)/s, except that the effect
			// is clamped to a minimum of -1.0 to avoid a negative square root; this is correct, since it means
			// that 1+s (1 + -1 == 0) equals 2(1+hs): (2 x (1 + 1 x -1)) == (2 x 0) == 0.  If the resulting
			// dominance of 1.0 is used in 1+hs with the unclipped effect size, a negative mutational effect will
			// result, which is OK since multiplicative mutational effects are clipped at a minimum of 0.0.
			slim_effect_t effect_size = traitInfoRec.effect_size_;
			
			if (effect_size == (slim_effect_t)0.0)
				return (slim_effect_t)0.5;
			if (effect_size <= (slim_effect_t)-1.0)
				return (slim_effect_t)1.0;
			
			// do the math in double-precision float to avoid numerical error
			return (slim_effect_t)((std::sqrt(1.0 + (double)effect_size) - 1.0) / (double)effect_size);
		}
	}
	
	return traitInfoRec.dominance_coeff_UNSAFE_;
}

void Substitution::PrintForSLiMOutput(std::ostream &p_out, bool p_print_tag) const
{ 
	p_out << mutation_id_ << " m" << mutation_type_ptr_->mutation_type_id_ << " " << position_;
	
	// BCH 2/2/2025: Note that in multi-chrom models, this method now prints the chromosome symbol after the position
	// For brevity and backward compatibility, the chromosome symbol is not printed in single-chromosome models
	
	// BCH 11/27/2026: This method does NOT print effect sizes/dominances at their full precision, for brevity.
	// This would prevent accurate round-trips of substitutuions, but at present we do not round-trip substitutions
	// in text format (and in binary format we output full precision).  See Polymorphism::Print_ID().
	
	Species &species = mutation_type_ptr_->species_;
	const std::vector<Chromosome *> &chromosomes = species.Chromosomes();
	
	if (chromosomes.size() > 1)
	{
		Chromosome *chromosome = chromosomes[chromosome_index_];
		
		p_out << " \"" << chromosome->Symbol() << "\"";
	}
	
	// write out per-trait information
	slim_trait_index_t trait_count = species.TraitCount();
	
	// effect sizes, bracketed with { } in multitrait models
	if (trait_count != 1)
		p_out << " {";
	
	for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
	{
		p_out << " " << trait_info_[trait_index].effect_size_;
	}
	
	if (trait_count != 1)
		p_out << " }";
	
	// dominances, bracketed with { } in multitrait models
	if (trait_count != 1)
		p_out << " {";
	
	for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
	{
		slim_effect_t dominance = trait_info_[trait_index].dominance_coeff_UNSAFE_;
		
		if (std::isnan(dominance))
			p_out << " NAN";
		else
			p_out << " " << dominance;
	}
	
	if (trait_count != 1)
		p_out << " }";
	
	// hemizygous dominances, bracketed with { } in multitrait models
	if (trait_count != 1)
		p_out << " {";
	
	for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
	{
		p_out << " " << trait_info_[trait_index].hemizygous_dominance_coeff_;
	}
	
	if (trait_count != 1)
		p_out << " }";
	
	// and then the remainder of the output line
	p_out << " p" << subpop_index_ << " " << origin_tick_ << " " << fixation_tick_;
	
	// output a nucleotide if available
	if (mutation_type_ptr_->nucleotide_based_)
		p_out << " " << gSLiM_Nucleotides[nucleotide_];
	
	if (p_print_tag)
	{
		// output the tag value, or '?' or the tag is not defined
		if (tag_value_ == SLIM_TAG_UNSET_VALUE)
			p_out << ' ' << '?';
		else
			p_out << ' ' << tag_value_;
	}
	
	p_out << std::endl;
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

const EidosClass *Substitution::Class(void) const
{
	return gSLiM_Substitution_Class;
}

void Substitution::Print(std::ostream &p_ostream) const
{
	// BCH 10/19/2025: Changing from selection_coeff_ to position_ here, as part of multitrait work
	p_ostream << Class()->ClassNameForDisplay() << "<" << mutation_id_ << ":" << position_ << ">";
}

EidosValue_SP Substitution::GetProperty(EidosGlobalStringID p_property_id)
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
		case gID_id:					// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(mutation_id_));
		case gID_isNeutral:				// ACCELERATED
			return (is_neutral_for_all_traits_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		case gID_mutationType:			// ACCELERATED
			return mutation_type_ptr_->SymbolTableEntry().second;
		case gID_position:				// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(position_));
		case gID_selectionCoeff:
		{
			// This property is now deprecated, and errors in all models that do not use the default trait
			Species &species = mutation_type_ptr_->species_;
			slim_trait_index_t trait_count = species.TraitCount();
			
			if ((trait_count != 1) || (species.Traits()[0]->Type() != TraitType::kMultiplicative) || !species.HasImplicitTrait())
				EIDOS_TERMINATION << "ERROR (Mutation::GetProperty): property selectionCoeff is not defined for object element type Substitution, apart from models using the default multiplicative trait (for backward compatibility)." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)trait_info_[0].effect_size_));
		}
		case gID_effectSize:
		{
			// This is not accelerated, because it's a bit tricky; each substitution could belong to a different species,
			// and thus be associated with a different number of traits.  It isn't expected that this will be a hot path.
			Species &species = mutation_type_ptr_->species_;
			slim_trait_index_t trait_count = species.TraitCount();
			
			if (trait_count == 1)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)trait_info_[0].effect_size_));
			else if (trait_count == 0)
				return gStaticEidosValue_Float_ZeroVec;
			else
			{
				EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_count);
				
				for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
				{
					slim_effect_t effect_size = trait_info_[trait_index].effect_size_;
					
					float_result->push_float_no_check((double)effect_size);
				}
				
				return EidosValue_SP(float_result);
			}
		}
		case gID_dominance:
		{
			// This is not accelerated, because it's a bit tricky; each substitution could belong to a different species,
			// and thus be associated with a different number of traits.  It isn't expected that this will be a hot path.
			// Note that we use RealizedDominanceForTrait() here so that an independent dominance of NAN gets handled.
			Species &species = mutation_type_ptr_->species_;
			const std::vector<Trait *> &traits = species.Traits();
			slim_trait_index_t trait_count = species.TraitCount();
			
			if (trait_count == 1)
			{
				slim_effect_t realized_dominance = RealizedDominanceForTrait(traits[0]);
				
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)realized_dominance));
			}
			else if (trait_count == 0)
			{
				return gStaticEidosValue_Float_ZeroVec;
			}
			else
			{
				EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_count);
				
				for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
				{
					slim_effect_t realized_dominance = RealizedDominanceForTrait(traits[trait_index]);
					
					float_result->push_float_no_check((double)realized_dominance);
				}
				
				return EidosValue_SP(float_result);
			}
		}
		case gID_hemizygousDominance:
		{
			// This is not accelerated, because it's a bit tricky; each substitution could belong to a different species,
			// and thus be associated with a different number of traits.  It isn't expected that this will be a hot path.
			Species &species = mutation_type_ptr_->species_;
			slim_trait_index_t trait_count = species.TraitCount();
			
			if (trait_count == 1)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)trait_info_[0].hemizygous_dominance_coeff_));
			else if (trait_count == 0)
				return gStaticEidosValue_Float_ZeroVec;
			else
			{
				EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_count);
				
				for (slim_trait_index_t trait_index = 0; trait_index < trait_count; ++trait_index)
				{
					slim_effect_t dominance = trait_info_[trait_index].hemizygous_dominance_coeff_;
					
					float_result->push_float_no_check((double)dominance);
				}
				
				return EidosValue_SP(float_result);
			}
		}
		case gID_originTick:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(origin_tick_));
		case gID_fixationTick:			// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(fixation_tick_));
			
			// variables
		case gID_nucleotide:			// ACCELERATED
		{
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Substitution::GetProperty): property nucleotide is only defined for nucleotide-based mutations." << EidosTerminate();
			
			switch (nucleotide_)
			{
				case 0:	return gStaticEidosValue_StringA;
				case 1:	return gStaticEidosValue_StringC;
				case 2:	return gStaticEidosValue_StringG;
				case 3:	return gStaticEidosValue_StringT;
				default:
					EIDOS_TERMINATION << "ERROR (Substitution::GetProperty): (internal error) unrecognized value for nucleotide_." << EidosTerminate();
			}
		}
		case gID_nucleotideValue:		// ACCELERATED
		{
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Substitution::GetProperty): property nucleotideValue is only defined for nucleotide-based mutations." << EidosTerminate();
			
			switch (nucleotide_)
			{
				case 0:	return gStaticEidosValue_Integer0;
				case 1:	return gStaticEidosValue_Integer1;
				case 2:	return gStaticEidosValue_Integer2;
				case 3:	return gStaticEidosValue_Integer3;
				default:
					EIDOS_TERMINATION << "ERROR (Substitution::GetProperty): (internal error) unrecognized value for nucleotide_." << EidosTerminate();
			}
		}
		case gID_subpopID:				// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(subpop_index_));
		case gID_tag:					// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Substitution::GetProperty): property tag accessed on substitution before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(tag_value));
		}
			
			// all others, including gID_none
		default:
			// Here we implement a special behavior: you can do substitution.<trait-name>EffectSize, substitution.<trait-name>Dominance,
			// and substitution.<trait-name>HemizygousDominance to access a trait's values directly.
			// NOTE: This mechanism also needs to be maintained in Species::ExecuteContextFunction_initializeTrait().
			// NOTE: This mechanism also needs to be maintained in SLiMTypeInterpreter::_TypeEvaluate_FunctionCall_Internal().
			Species &species = mutation_type_ptr_->species_;
			const std::string &property_string = EidosStringRegistry::StringForGlobalStringID(p_property_id);
			
			if ((property_string.length() > 10) && Eidos_string_hasSuffix(property_string, "EffectSize"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 10);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)trait_info_[trait->Index()].effect_size_));
			}
			else if ((property_string.length() > 19) && Eidos_string_hasSuffix(property_string, "HemizygousDominance"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 19);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)trait_info_[trait->Index()].hemizygous_dominance_coeff_));
			}
			else if ((property_string.length() > 9) && Eidos_string_hasSuffix(property_string, "Dominance"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 9);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
				{
					// Note that we use RealizedDominanceForTrait() here so that an independent dominance of NAN gets handled.
					slim_effect_t realized_dominance = RealizedDominanceForTrait(trait);
					
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)realized_dominance));
				}
			}
			
			return super::GetProperty(p_property_id);
	}
}

EidosValue *Substitution::GetProperty_Accelerated_id(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->mutation_id_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_isNeutral(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		logical_result->set_logical_no_check(value->is_neutral_for_all_traits_, value_index);
	}
	
	return logical_result;
}

EidosValue *Substitution::GetProperty_Accelerated_nucleotide(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_String *string_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_String())->Reserve((int)p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		int8_t nucleotide = value->nucleotide_;
		
		if (nucleotide == -1)
			EIDOS_TERMINATION << "ERROR (Substitution::GetProperty_Accelerated_nucleotide): property nucleotide is only defined for nucleotide-based mutations." << EidosTerminate();
		
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

EidosValue *Substitution::GetProperty_Accelerated_nucleotideValue(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		int8_t nucleotide = value->nucleotide_;
		
		if (nucleotide == -1)
			EIDOS_TERMINATION << "ERROR (Substitution::GetProperty_Accelerated_nucleotideValue): property nucleotideValue is only defined for nucleotide-based mutations." << EidosTerminate();
		
		int_result->set_int_no_check(nucleotide, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_originTick(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->origin_tick_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_fixationTick(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->fixation_tick_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_position(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->position_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_subpopID(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->subpop_index_, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_tag(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Int *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (Substitution::GetProperty_Accelerated_tag): property tag accessed on substitution before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

EidosValue *Substitution::GetProperty_Accelerated_mutationType(EidosGlobalStringID p_property_id, EidosObject **p_values, size_t p_values_size)
{
#pragma unused (p_property_id)
	EidosValue_Object *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object(gSLiM_MutationType_Class))->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Substitution *value = (Substitution *)(p_values[value_index]);
		
		object_result->set_object_element_no_check_NORR(value->mutation_type_ptr_, value_index);
	}
	
	return object_result;
}

void Substitution::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_subpopID:
		{
			slim_objectid_t value = SLiMCastToObjectidTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			subpop_index_ = value;
			return;
		}
		case gID_tag:
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex_NOCAST(0, nullptr));
			
			tag_value_ = value;
			return;
		}
		
		default:
		{
			return super::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue_SP Substitution::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_effectSizeForTrait:			return ExecuteMethod_effectSizeForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_dominanceForTrait:				return ExecuteMethod_dominanceForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_hemizygousDominanceForTrait:	return ExecuteMethod_hemizygousDominanceForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_isIndependentDominanceForTrait:	return ExecuteMethod_isIndependentDominanceForTrait(p_method_id, p_arguments, p_interpreter);
		default:								return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (float)effectSizeForTrait([Niso<Trait> trait = NULL])
//
EidosValue_SP Substitution::ExecuteMethod_effectSizeForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	
	// get the trait indices, with bounds-checking
	Species &species = mutation_type_ptr_->species_;
	std::vector<slim_trait_index_t> trait_indices;
	species.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "effectSizeForTrait");
	
	if (trait_indices.size() == 1)
	{
		slim_trait_index_t trait_index = trait_indices[0];
		slim_effect_t effect_size = trait_info_[trait_index].effect_size_;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)effect_size));
	}
	else
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_indices.size());
		
		for (slim_trait_index_t trait_index : trait_indices)
		{
			slim_effect_t effect_size = trait_info_[trait_index].effect_size_;
			
			float_result->push_float_no_check((double)effect_size);
		}
		
		return EidosValue_SP(float_result);
	}
}

//	*********************	- (float)dominanceForTrait([Niso<Trait> trait = NULL])
//
EidosValue_SP Substitution::ExecuteMethod_dominanceForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	
	// get the trait indices, with bounds-checking
	Species &species = mutation_type_ptr_->species_;
	const std::vector<Trait *> &traits = species.Traits();
	std::vector<slim_trait_index_t> trait_indices;
	species.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "dominanceForTrait");
	
	if (trait_indices.size() == 1)
	{
		slim_trait_index_t trait_index = trait_indices[0];
		Trait *trait = traits[trait_index];
		slim_effect_t realized_dominance = RealizedDominanceForTrait(trait);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)realized_dominance));
	}
	else
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_indices.size());
		
		for (slim_trait_index_t trait_index : trait_indices)
		{
			Trait *trait = traits[trait_index];
			slim_effect_t realized_dominance = RealizedDominanceForTrait(trait);
			
			float_result->push_float_no_check((double)realized_dominance);
		}
		
		return EidosValue_SP(float_result);
	}
}

//	*********************	- (float)hemizygousDominanceForTrait([Niso<Trait> trait = NULL])
//
EidosValue_SP Substitution::ExecuteMethod_hemizygousDominanceForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	
	// get the trait indices, with bounds-checking
	Species &species = mutation_type_ptr_->species_;
	std::vector<slim_trait_index_t> trait_indices;
	species.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "hemizygousDominanceForTrait");
	
	if (trait_indices.size() == 1)
	{
		slim_trait_index_t trait_index = trait_indices[0];
		slim_effect_t dominance = trait_info_[trait_index].hemizygous_dominance_coeff_;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float((double)dominance));
	}
	else
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_indices.size());
		
		for (slim_trait_index_t trait_index : trait_indices)
		{
			slim_effect_t dominance = trait_info_[trait_index].hemizygous_dominance_coeff_;
			
			float_result->push_float_no_check((double)dominance);
		}
		
		return EidosValue_SP(float_result);
	}
}

//	*********************	- (logical)isIndependentDominanceForTrait([Niso<Trait> trait = NULL])
//
EidosValue_SP Substitution::ExecuteMethod_isIndependentDominanceForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	
	// get the trait indices, with bounds-checking
	Species &species = mutation_type_ptr_->species_;
	std::vector<slim_trait_index_t> trait_indices;
	species.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "isIndependentDominanceForTrait");
	
	if (trait_indices.size() == 1)
	{
		slim_trait_index_t trait_index = trait_indices[0];
		slim_effect_t dominance = trait_info_[trait_index].dominance_coeff_UNSAFE_;
		
		return (std::isnan(dominance) ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalT);
	}
	else
	{
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->reserve(trait_indices.size());
		
		for (slim_trait_index_t trait_index : trait_indices)
		{
			slim_effect_t dominance = trait_info_[trait_index].dominance_coeff_UNSAFE_;
			
			logical_result->push_logical_no_check(std::isnan(dominance));
		}
		
		return EidosValue_SP(logical_result);
	}
}


//
//	Substitution_Class
//
#pragma mark -
#pragma mark Substitution_Class
#pragma mark -

Substitution_Class *gSLiM_Substitution_Class = nullptr;


std::vector<EidosPropertySignature_CSP> *Substitution_Class::Properties_MUTABLE(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Substitution_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties_MUTABLE());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_chromosome,				true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Chromosome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,						true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_id));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_isNeutral,				true,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Mutation::GetProperty_Accelerated_isNeutral));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationType,			true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_MutationType_Class))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_mutationType));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_position,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_position));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_effectSize,				true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_selectionCoeff,			true,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->MarkDeprecated());
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_dominance,				true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_hemizygousDominance,	true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopID,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_subpopID));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotide,				true,	kEidosValueMaskString | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_nucleotide));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotideValue,		true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_nucleotideValue));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_originTick,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_originTick));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_fixationTick,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_fixationTick));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_tag));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *Substitution_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Substitution_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_effectSizeForTrait, kEidosValueMaskFloat))->AddIntStringObject_ON(gStr_trait, gSLiM_Trait_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_dominanceForTrait, kEidosValueMaskFloat))->AddIntStringObject_ON(gStr_trait, gSLiM_Trait_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_hemizygousDominanceForTrait, kEidosValueMaskFloat))->AddIntStringObject_ON(gStr_trait, gSLiM_Trait_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_isIndependentDominanceForTrait, kEidosValueMaskLogical))->AddIntStringObject_ON(gStr_trait, gSLiM_Trait_Class, gStaticEidosValueNULL));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}







































































