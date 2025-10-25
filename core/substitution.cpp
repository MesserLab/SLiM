//
//  substitution.cpp
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


#include "substitution.h"
#include "slim_globals.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "species.h"
#include "mutation_block.h"

#include <iostream>
#include <algorithm>
#include <string>
#include <vector>


#pragma mark -
#pragma mark Substitution
#pragma mark -

Substitution::Substitution(Mutation &p_mutation, slim_tick_t p_fixation_tick) :
	EidosDictionaryRetained(), mutation_type_ptr_(p_mutation.mutation_type_ptr_), position_(p_mutation.position_), subpop_index_(p_mutation.subpop_index_), origin_tick_(p_mutation.origin_tick_), fixation_tick_(p_fixation_tick), chromosome_index_(p_mutation.chromosome_index_), nucleotide_(p_mutation.nucleotide_), mutation_id_(p_mutation.mutation_id_), tag_value_(p_mutation.tag_value_)
	
{
	AddKeysAndValuesFrom(&p_mutation);
	// No call to ContentsChanged() here; we know we use Dictionary not DataFrame, and Mutation already vetted the dictionary
	
	// Copy per-trait information over from the mutation object
	Species &species = mutation_type_ptr_->species_;
	MutationBlock *mutation_block = species.SpeciesMutationBlock();
	MutationTraitInfo *mut_trait_info = mutation_block->TraitInfoForMutation(&p_mutation);
	int trait_count = species.TraitCount();
	
	trait_info_ = (SubstitutionTraitInfo *)malloc(trait_count * sizeof(SubstitutionTraitInfo));
	
	for (int trait_index = 0; trait_index < trait_count; trait_index++)
	{
		trait_info_[trait_index].effect_size_ = mut_trait_info[trait_index].effect_size_;
		trait_info_[trait_index].dominance_coeff_ = mut_trait_info[trait_index].dominance_coeff_;
	}
}

Substitution::Substitution(slim_mutationid_t p_mutation_id, MutationType *p_mutation_type_ptr, slim_chromosome_index_t p_chromosome_index, slim_position_t p_position, slim_effect_t p_selection_coeff, slim_effect_t p_dominance_coeff, slim_objectid_t p_subpop_index, slim_tick_t p_tick, slim_tick_t p_fixation_tick, int8_t p_nucleotide) :
mutation_type_ptr_(p_mutation_type_ptr), position_(p_position), subpop_index_(p_subpop_index), origin_tick_(p_tick), fixation_tick_(p_fixation_tick), chromosome_index_(p_chromosome_index), nucleotide_(p_nucleotide), mutation_id_(p_mutation_id), tag_value_(SLIM_TAG_UNSET_VALUE)
{
	// FIXME MULTITRAIT: This code path is hit when loading substitutions from an output file, also needs to initialize the multitrait info; this is just a
	// placeholder.  The file being read in ought to specify per-trait values, which hasn't happened yet, so there are lots of details to be worked out...
	Species &species = mutation_type_ptr_->species_;
	int trait_count = species.TraitCount();
	
	trait_info_ = (SubstitutionTraitInfo *)malloc(trait_count * sizeof(SubstitutionTraitInfo));
	
	trait_info_[0].effect_size_ = p_selection_coeff;
	trait_info_[0].dominance_coeff_ = p_dominance_coeff;
	
	for (int trait_index = 1; trait_index < trait_count; trait_index++)
	{
		trait_info_[trait_index].effect_size_ = 0.0;
		trait_info_[trait_index].dominance_coeff_ = 0.0;
	}
}

void Substitution::PrintForSLiMOutput(std::ostream &p_out) const
{ 
	p_out << mutation_id_ << " m" << mutation_type_ptr_->mutation_type_id_ << " " << position_;
	
	// BCH 2/2/2025: Note that in multi-chrom models, this method now prints the chromosome symbol after the position
	// For brevity and backward compatibility, the chromosome symbol is not printed in single-chromosome models
	Species &species = mutation_type_ptr_->species_;
	const std::vector<Chromosome *> &chromosomes = species.Chromosomes();
	
	if (chromosomes.size() > 1)
	{
		Chromosome *chromosome = chromosomes[chromosome_index_];
		
		p_out << " \"" << chromosome->Symbol() << "\"";
	}
	
	// write out per-trait information
	// FIXME MULTITRAIT: Just dumping all the traits, for now; not sure what should happen here
	int trait_count = species.TraitCount();
	
	for (int trait_index = 0; trait_index < trait_count; ++trait_index)
		p_out << " " << trait_info_[trait_index].effect_size_ << " " << trait_info_[trait_index].dominance_coeff_;
	
	// and then the remainder of the output line
	p_out << " p" << subpop_index_ << " " << origin_tick_ << " " << fixation_tick_;
	
	// output a nucleotide if available
	if (mutation_type_ptr_->nucleotide_based_)
		p_out << " " << gSLiM_Nucleotides[nucleotide_];
	
	p_out << std::endl;
}

void Substitution::PrintForSLiMOutput_Tag(std::ostream &p_out) const
{ 
	// BCH 4/7/2025: This is a copy of PrintForSLiMOutput() with output of tag_value_ added at the end
	
	p_out << mutation_id_ << " m" << mutation_type_ptr_->mutation_type_id_ << " " << position_;
	
	// BCH 2/2/2025: Note that in multi-chrom models, this method now prints the chromosome symbol after the position
	// For brevity and backward compatibility, the chromosome symbol is not printed in single-chromosome models
	Species &species = mutation_type_ptr_->species_;
	const std::vector<Chromosome *> &chromosomes = species.Chromosomes();
	
	if (chromosomes.size() > 1)
	{
		Chromosome *chromosome = chromosomes[chromosome_index_];
		
		p_out << " \"" << chromosome->Symbol() << "\"";
	}
	
	// write out per-trait information
	// FIXME MULTITRAIT: Just dumping all the traits, for now; not sure what should happen here
	int trait_count = species.TraitCount();
	
	for (int trait_index = 0; trait_index < trait_count; ++trait_index)
		p_out << " " << trait_info_[trait_index].effect_size_ << " " << trait_info_[trait_index].dominance_coeff_;
	
	// and then the remainder of the output line
	p_out << " p" << subpop_index_ << " " << origin_tick_ << " " << fixation_tick_;
	
	// output a nucleotide if available
	if (mutation_type_ptr_->nucleotide_based_)
		p_out << " " << gSLiM_Nucleotides[nucleotide_];
	
	// output the tag value, or '?' or the tag is not defined
	if (tag_value_ == SLIM_TAG_UNSET_VALUE)
		p_out << ' ' << '?';
	else
		p_out << ' ' << tag_value_;
	
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
		case gID_mutationType:			// ACCELERATED
			return mutation_type_ptr_->SymbolTableEntry().second;
		case gID_position:				// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int(position_));
		case gID_effect:
		{
			// This is not accelerated, because it's a bit tricky; each substitution could belong to a different species,
			// and thus be associated with a different number of traits.  It isn't expected that this will be a hot path.
			Species &species = mutation_type_ptr_->species_;
			const std::vector<Trait *> &traits = species.Traits();
			size_t trait_count = traits.size();
			
			if (trait_count == 1)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(trait_info_[0].effect_size_));
			else if (trait_count == 0)
				return gStaticEidosValue_Float_ZeroVec;
			else
			{
				EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_count);
				
				for (size_t trait_index = 0; trait_index < trait_count; ++trait_index)
				{
					slim_effect_t effect = trait_info_[trait_index].effect_size_;
					
					float_result->push_float_no_check(effect);
				}
				
				return EidosValue_SP(float_result);
			}
		}
		case gID_dominance:
		{
			// This is not accelerated, because it's a bit tricky; each substitution could belong to a different species,
			// and thus be associated with a different number of traits.  It isn't expected that this will be a hot path.
			Species &species = mutation_type_ptr_->species_;
			const std::vector<Trait *> &traits = species.Traits();
			size_t trait_count = traits.size();
			
			if (trait_count == 1)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(trait_info_[0].dominance_coeff_));
			else if (trait_count == 0)
				return gStaticEidosValue_Float_ZeroVec;
			else
			{
				EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_count);
				
				for (size_t trait_index = 0; trait_index < trait_count; ++trait_index)
				{
					slim_effect_t dominance = trait_info_[trait_index].dominance_coeff_;
					
					float_result->push_float_no_check(dominance);
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
			// Here we implement a special behavior: you can do mutation.<trait>Effect and mutation.<trait>Dominance to access a trait's values directly.
			Species &species = mutation_type_ptr_->species_;
			const std::string &property_string = EidosStringRegistry::StringForGlobalStringID(p_property_id);
			
			if ((property_string.length() > 6) && Eidos_string_hasSuffix(property_string, "Effect"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 6);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(trait_info_[trait->Index()].effect_size_));
			}
			else if ((property_string.length() > 9) && Eidos_string_hasSuffix(property_string, "Dominance"))
			{
				std::string trait_name = property_string.substr(0, property_string.length() - 9);
				Trait *trait = species.TraitFromName(trait_name);
				
				if (trait)
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(trait_info_[trait->Index()].dominance_coeff_));
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
		case gID_nucleotide:
		{
			const std::string &nucleotide = ((EidosValue_String &)p_value).StringRefAtIndex_NOCAST(0, nullptr);
			
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Substitution::SetProperty): property nucleotide is only defined for nucleotide-based substitutions." << EidosTerminate();
			
			if (nucleotide == gStr_A)		nucleotide_ = 0;
			else if (nucleotide == gStr_C)	nucleotide_ = 1;
			else if (nucleotide == gStr_G)	nucleotide_ = 2;
			else if (nucleotide == gStr_T)	nucleotide_ = 3;
			else EIDOS_TERMINATION << "ERROR (Substitution::SetProperty): property nucleotide may only be set to 'A', 'C', 'G', or 'T'." << EidosTerminate();
			return;
		}
		case gID_nucleotideValue:
		{
			int64_t nucleotide = p_value.IntAtIndex_NOCAST(0, nullptr);
			
			if (nucleotide_ == -1)
				EIDOS_TERMINATION << "ERROR (Substitution::SetProperty): property nucleotideValue is only defined for nucleotide-based substitutions." << EidosTerminate();
			if ((nucleotide < 0) || (nucleotide > 3))
				EIDOS_TERMINATION << "ERROR (Substitution::SetProperty): property nucleotideValue may only be set to 0 (A), 1 (C), 2 (G), or 3 (T)." << EidosTerminate();
			
			nucleotide_ = (int8_t)nucleotide;
			return;
		}
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
		case gID_effectForTrait:	return ExecuteMethod_effectForTrait(p_method_id, p_arguments, p_interpreter);
		case gID_dominanceForTrait:	return ExecuteMethod_dominanceForTrait(p_method_id, p_arguments, p_interpreter);
		default:					return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//	*********************	- (float)effectForTrait([Nio<Trait> trait = NULL])
//
EidosValue_SP Substitution::ExecuteMethod_effectForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	
	// get the trait indices, with bounds-checking
	Species &species = mutation_type_ptr_->species_;
	std::vector<int64_t> trait_indices;
	species.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "effectForTrait");
	
	if (trait_indices.size() == 1)
	{
		int64_t trait_index = trait_indices[0];
		slim_effect_t effect = trait_info_[trait_index].effect_size_;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(effect));
	}
	else
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_indices.size());
		
		for (int64_t trait_index : trait_indices)
		{
			slim_effect_t effect = trait_info_[trait_index].effect_size_;
			
			float_result->push_float_no_check(effect);
		}
		
		return EidosValue_SP(float_result);
	}
}

//	*********************	- (float)dominanceForTrait([Nio<Trait> trait = NULL])
//
EidosValue_SP Substitution::ExecuteMethod_dominanceForTrait(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *trait_value = p_arguments[0].get();
	
	// get the trait indices, with bounds-checking
	Species &species = mutation_type_ptr_->species_;
	std::vector<int64_t> trait_indices;
	species.GetTraitIndicesFromEidosValue(trait_indices, trait_value, "dominanceForTrait");
	
	if (trait_indices.size() == 1)
	{
		int64_t trait_index = trait_indices[0];
		slim_effect_t dominance = trait_info_[trait_index].dominance_coeff_;
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float(dominance));
	}
	else
	{
		EidosValue_Float *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float())->reserve(trait_indices.size());
		
		for (int64_t trait_index : trait_indices)
		{
			slim_effect_t dominance = trait_info_[trait_index].dominance_coeff_;
			
			float_result->push_float_no_check(dominance);
		}
		
		return EidosValue_SP(float_result);
	}
}


//
//	Substitution_Class
//
#pragma mark -
#pragma mark Substitution_Class
#pragma mark -

EidosClass *gSLiM_Substitution_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *Substitution_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		THREAD_SAFETY_IN_ANY_PARALLEL("Substitution_Class::Properties(): not warmed up");
		
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_chromosome,			true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Chromosome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,					true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_id));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_mutationType,		true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_MutationType_Class))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_mutationType));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_position,			true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_position));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_effect,				true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_dominance,			true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopID,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_subpopID));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotide,			false,	kEidosValueMaskString | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_nucleotide));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_nucleotideValue,	false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_nucleotideValue));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_originTick,	true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_originTick));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_fixationTick,	true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_fixationTick));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,				false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Substitution::GetProperty_Accelerated_tag));
		
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
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_effectForTrait, kEidosValueMaskFloat))->AddIntObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_dominanceForTrait, kEidosValueMaskFloat))->AddIntObject_ON("trait", gSLiM_Trait_Class, gStaticEidosValueNULL));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}







































































