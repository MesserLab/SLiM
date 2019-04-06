//
//  individual.cpp
//  SLiM
//
//  Created by Ben Haller on 6/10/16.
//  Copyright (c) 2016-2019 Philipp Messer.  All rights reserved.
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


#include "individual.h"
#include "subpopulation.h"
#include "slim_sim.h"
#include "eidos_property_signature.h"
#include "eidos_call_signature.h"

#include <string>
#include <algorithm>
#include <vector>


#pragma mark -
#pragma mark Individual
#pragma mark -

// A global counter used to assign all Individual objects a unique ID
slim_pedigreeid_t gSLiM_next_pedigree_id = 0;

// Static member bools that track whether any individual has ever sustained a particular type of change
bool Individual::s_any_individual_color_set_ = false;
bool Individual::s_any_individual_dictionary_set_ = false;
bool Individual::s_any_individual_or_genome_tag_set_ = false;
bool Individual::s_any_individual_fitness_scaling_set_ = false;


Individual::Individual(Subpopulation &p_subpopulation, slim_popsize_t p_individual_index, slim_pedigreeid_t p_pedigree_id, Genome *p_genome1, Genome *p_genome2, IndividualSex p_sex, slim_age_t p_age, double p_fitness) : subpopulation_(p_subpopulation), index_(p_individual_index), migrant_(false), cached_fitness_UNSAFE_(p_fitness), genome1_(p_genome1), genome2_(p_genome2), sex_(p_sex),
#ifdef SLIM_NONWF_ONLY
	age_(p_age),
#endif  // SLIM_NONWF_ONLY
	pedigree_id_(p_pedigree_id), pedigree_p1_(-1), pedigree_p2_(-1), pedigree_g1_(-1), pedigree_g2_(-1), pedigree_g3_(-1), pedigree_g4_(-1)
{
#ifndef SLIM_NONWF_ONLY
#pragma unused(p_age)
#endif
#if DEBUG
	if (!p_genome1 || !p_genome2)
		EIDOS_TERMINATION << "ERROR (Individual::Individual): (internal error) nullptr passed for genome." << EidosTerminate();
#endif
	
	// Make our genomes use the correct pedigree IDs, if we're doing pedigree recording
	if (p_pedigree_id != -1)
	{
		p_genome1->genome_id_ = p_pedigree_id * 2;
		p_genome2->genome_id_ = p_pedigree_id * 2 + 1;
	}
	
	// Set up the pointers from our genomes to us
	p_genome1->individual_ = this;
	p_genome2->individual_ = this;
	
	// Initialize tag values to the "unset" value
	tag_value_ = SLIM_TAG_UNSET_VALUE;
	tagF_value_ = SLIM_TAGF_UNSET_VALUE;
	p_genome1->tag_value_ = SLIM_TAG_UNSET_VALUE;
	p_genome1->tag_value_ = SLIM_TAG_UNSET_VALUE;
}

double Individual::RelatednessToIndividual(Individual &p_ind)
{
	// If we're being asked about ourselves, return 1.0, even if pedigree tracking is off
	if (this == &p_ind)
		return 1.0;
	
	// Otherwise, if our own pedigree information is not initialized, then we have nothing to go on
	if (pedigree_id_ == -1)
		return 0.0;
	
	// Start with 0.0 and add in factors for shared ancestors
	double relatedness = 0.0;
	
	if ((pedigree_g1_ != -1) && (p_ind.pedigree_g1_ != -1))
	{
		// We have grandparental information, so use that; that will be the most accurate
		double g1 = pedigree_g1_;
		double g2 = pedigree_g2_;
		double g3 = pedigree_g3_;
		double g4 = pedigree_g4_;
		
		double ind_g1 = p_ind.pedigree_g1_;
		double ind_g2 = p_ind.pedigree_g2_;
		double ind_g3 = p_ind.pedigree_g3_;
		double ind_g4 = p_ind.pedigree_g4_;
		
		// Each shared grandparent adds 0.125, for a maximum of 0.5
		if ((g1 == ind_g1) || (g1 == ind_g2) || (g1 == ind_g3) || (g1 == ind_g4))	relatedness += 0.125;
		if ((g2 == ind_g1) || (g2 == ind_g2) || (g2 == ind_g3) || (g2 == ind_g4))	relatedness += 0.125;
		if ((g3 == ind_g1) || (g3 == ind_g2) || (g3 == ind_g3) || (g3 == ind_g4))	relatedness += 0.125;
		if ((g4 == ind_g1) || (g4 == ind_g2) || (g4 == ind_g3) || (g4 == ind_g4))	relatedness += 0.125;
	}
	else if ((pedigree_p1_ != -1) && (p_ind.pedigree_p1_ != -1))
	{
		// We have parental information; that's second-best
		double p1 = pedigree_p1_;
		double p2 = pedigree_p2_;
		
		double ind_p1 = p_ind.pedigree_p1_;
		double ind_p2 = p_ind.pedigree_p2_;
		
		// Each shared parent adds 0.25, for a maximum of 0.5
		if ((p1 == ind_p1) || (p1 == ind_p2))	relatedness += 0.25;
		if ((p2 == ind_p1) || (p2 == ind_p2))	relatedness += 0.25;
	}
	
	// With no information, we assume we are not related
	return relatedness;
}


//
// Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

void Individual::GenerateCachedEidosValue(void)
{
	// Note that this cache cannot be invalidated as long as a symbol table might exist that this value has been placed into
	// The false parameter selects a constructor variant that prevents this self-cache from having its address patched;
	// our self-pointer never changes.  See EidosValue_Object::EidosValue_Object() for comments on this disgusting hack.
	self_value_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_Individual_Class, false));
}

const EidosObjectClass *Individual::Class(void) const
{
	return gSLiM_Individual_Class;
}

void Individual::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType() << "<p" << subpopulation_.subpopulation_id_ << ":i" << index_ << ">";
}

EidosValue_SP Individual::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_subpopulation:		// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(&subpopulation_, gSLiM_Subpopulation_Class));
		}
		case gID_index:				// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(index_));
		}
		case gID_genomes:
		{
			EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->resize_no_initialize(2);
			
			vec->set_object_element_no_check(genome1_, 0);
			vec->set_object_element_no_check(genome2_, 1);
			
			return EidosValue_SP(vec);
		}
		case gID_genome1:			// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(genome1_, gSLiM_Genome_Class));
		}
		case gID_genome2:			// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(genome2_, gSLiM_Genome_Class));
		}
		case gID_sex:
		{
			static EidosValue_SP static_sex_string_H;
			static EidosValue_SP static_sex_string_F;
			static EidosValue_SP static_sex_string_M;
			static EidosValue_SP static_sex_string_O;
			
			if (!static_sex_string_H)
			{
				static_sex_string_H = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("H"));
				static_sex_string_F = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("F"));
				static_sex_string_M = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("M"));
				static_sex_string_O = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton("?"));
			}
			
			switch (sex_)
			{
				case IndividualSex::kHermaphrodite:	return static_sex_string_H;
				case IndividualSex::kFemale:		return static_sex_string_F;
				case IndividualSex::kMale:			return static_sex_string_M;
				default:							return static_sex_string_O;
			}
		}
#ifdef SLIM_NONWF_ONLY
		case gID_age:				// ACCELERATED
		{
			if (age_ == -1)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property age is not available in WF models." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(age_));
		}
#endif  // SLIM_NONWF_ONLY
		case gID_pedigreeID:		// ACCELERATED
		{
			if (!subpopulation_.population_.sim_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property pedigreeID is not available because pedigree recording has not been enabled." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(pedigree_id_));
		}
		case gID_pedigreeParentIDs:
		{
			if (!subpopulation_.population_.sim_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property pedigreeParentIDs is not available because pedigree recording has not been enabled." << EidosTerminate();
			
			EidosValue_Int_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(2);
			
			vec->set_int_no_check(pedigree_p1_, 0);
			vec->set_int_no_check(pedigree_p2_, 1);
			
			return EidosValue_SP(vec);
		}
		case gID_pedigreeGrandparentIDs:
		{
			if (!subpopulation_.population_.sim_.PedigreesEnabledByUser())
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property pedigreeGrandparentIDs is not available because pedigree recording has not been enabled." << EidosTerminate();
			
			EidosValue_Int_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(4);
			
			vec->set_int_no_check(pedigree_g1_, 0);
			vec->set_int_no_check(pedigree_g2_, 1);
			vec->set_int_no_check(pedigree_g3_, 2);
			vec->set_int_no_check(pedigree_g4_, 3);
			
			return EidosValue_SP(vec);
		}
		case gID_spatialPosition:
		{
			SLiMSim &sim = subpopulation_.population_.sim_;
			
			switch (sim.SpatialDimensionality())
			{
				case 0:
					EIDOS_TERMINATION << "ERROR (Individual::GetProperty): position cannot be accessed in non-spatial simulations." << EidosTerminate();
				case 1:
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(spatial_x_));
				case 2:
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{spatial_x_, spatial_y_});
				case 3:
					return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector{spatial_x_, spatial_y_, spatial_z_});
			}
		}
		case gID_uniqueMutations:
		{
			// We reserve a vector large enough to hold all the mutations from both genomes; probably usually overkill, but it does little harm
			int genome1_size = (genome1_->IsNull() ? 0 : genome1_->mutation_count());
			int genome2_size = (genome2_->IsNull() ? 0 : genome2_->mutation_count());
			EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
			EidosValue_SP result_SP = EidosValue_SP(vec);
			
			if ((genome1_size == 0) && (genome2_size == 0))
				return result_SP;
			
			vec->reserve(genome1_size + genome2_size);
			
			Mutation *mut_block_ptr = gSLiM_Mutation_Block;
			int mutrun_count = (genome1_size ? genome1_->mutrun_count_ : genome2_->mutrun_count_);
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				// We want to interleave mutations from the two genomes, keeping only the uniqued mutations.  For a given position, we take mutations
				// from g1 first, and then look at the mutations in g2 at the same position and add them if they are not in g1.
				MutationRun *mutrun1 = (genome1_size ? genome1_->mutruns_[run_index].get() : nullptr);
				MutationRun *mutrun2 = (genome2_size ? genome2_->mutruns_[run_index].get() : nullptr);
				int g1_size = (mutrun1 ? mutrun1->size() : 0);
				int g2_size = (mutrun2 ? mutrun2->size() : 0);
				int g1_index = 0, g2_index = 0;
				
				if (g1_size && g2_size)
				{
					// Get the position of the mutations at g1_index and g2_index
					MutationIndex g1_mut = (*mutrun1)[g1_index], g2_mut = (*mutrun2)[g2_index];
					slim_position_t pos1 = (mut_block_ptr + g1_mut)->position_, pos2 = (mut_block_ptr + g2_mut)->position_;
					
					// Process mutations as long as both genomes still have mutations left in them
					do
					{
						if (pos1 < pos2)
						{
							vec->push_object_element_no_check(mut_block_ptr + g1_mut);
							
							// Move to the next mutation in g1
							if (++g1_index >= g1_size)
								break;
							g1_mut = (*mutrun1)[g1_index];
							pos1 = (mut_block_ptr + g1_mut)->position_;
						}
						else if (pos1 > pos2)
						{
							vec->push_object_element_no_check(mut_block_ptr + g2_mut);
							
							// Move to the next mutation in g2
							if (++g2_index >= g2_size)
								break;
							g2_mut = (*mutrun2)[g2_index];
							pos2 = (mut_block_ptr + g2_mut)->position_;
						}
						else
						{
							// pos1 == pos2; copy mutations from g1 until we are done with this position, then handle g2
							slim_position_t focal_pos = pos1;
							int first_index = g1_index;
							bool done = false;
							
							while (pos1 == focal_pos)
							{
								vec->push_object_element_no_check(mut_block_ptr + g1_mut);
								
								// Move to the next mutation in g1
								if (++g1_index >= g1_size)
								{
									done = true;
									break;
								}
								g1_mut = (*mutrun1)[g1_index];
								pos1 = (mut_block_ptr + g1_mut)->position_;
							}
							
							// Note that we may be done with g1 here, so be careful
							int last_index_plus_one = g1_index;
							
							while (pos2 == focal_pos)
							{
								int check_index;
								
								for (check_index = first_index; check_index < last_index_plus_one; ++check_index)
									if ((*mutrun1)[check_index] == g2_mut)
										break;
								
								// If the check indicates that g2_mut is not in g1, we copy it over
								if (check_index == last_index_plus_one)
									vec->push_object_element_no_check(mut_block_ptr + g2_mut);
								
								// Move to the next mutation in g2
								if (++g2_index >= g2_size)
								{
									done = true;
									break;
								}
								g2_mut = (*mutrun2)[g2_index];
								pos2 = (mut_block_ptr + g2_mut)->position_;
							}
							
							// Note that we may be done with both g1 and/or g2 here; if so, done will be set and we will break out
							if (done)
								break;
						}
					}
					while (true);
				}
				
				// Finish off any tail ends, which must be unique and sorted already
				while (g1_index < g1_size)
					vec->push_object_element_no_check(mut_block_ptr + (*mutrun1)[g1_index++]);
				while (g2_index < g2_size)
					vec->push_object_element_no_check(mut_block_ptr + (*mutrun2)[g2_index++]);
			}
		
			return result_SP;
			/*
			 The code above for uniqueMutations can be tested with the simple SLiM script below.  Positions are tested with
			 identical() instead of the mutation vectors themselves, only because the sorted order of mutations at exactly
			 the same position may differ; identical(um1, um2) will occasionally flag these as false positives.
			 
			 initialize() {
				 initializeMutationRate(1e-5);
				 initializeMutationType("m1", 0.5, "f", 0.0);
				 initializeGenomicElementType("g1", m1, 1.0);
				 initializeGenomicElement(g1, 0, 99999);
				 initializeRecombinationRate(1e-8);
			 }
			 1 {
				sim.addSubpop("p1", 500);
			 }
			 1:20000 late() {
				 for (i in p1.individuals)
				 {
					 um1 = i.uniqueMutations;
					 um2 = sortBy(unique(i.genomes.mutations), "position");
					 
					 if (!identical(um1.position, um2.position))
					 {
						 print("Mismatch!");
						 print(um1.position);
						 print(um2.position);
					 }
				 }
			 }
			 */
		}
			
			// variables
		case gEidosID_color:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(color_));
		}
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tag accessed on individual before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
		case gID_tagF:				// ACCELERATED
		{
			double tagF_value = tagF_value_;
			
			if (tagF_value == SLIM_TAGF_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagF accessed on individual before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(tagF_value));
		}
		case gID_migrant:			// ACCELERATED
		{
			return (migrant_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_fitnessScaling:	// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(fitness_scaling_));
		}
		case gEidosID_x:			// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(spatial_x_));
		}
		case gEidosID_y:			// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(spatial_y_));
		}
		case gEidosID_z:			// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(spatial_z_));
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

EidosValue *Individual::GetProperty_Accelerated_index(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->index_, value_index);
	}
	
	return int_result;
}

EidosValue *Individual::GetProperty_Accelerated_pedigreeID(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	size_t value_index = 0;
	
	// check that pedigrees are enabled, once
	if (value_index < p_values_size)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		if (!value->subpopulation_.population_.sim_.PedigreesEnabledByUser())
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property pedigreeID is not available because pedigree recording has not been enabled." << EidosTerminate();
		
		int_result->set_int_no_check(value->pedigree_id_, value_index);
		++value_index;
	}
	
	for ( ; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->pedigree_id_, value_index);
	}
	
	return int_result;
}

EidosValue *Individual::GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tag accessed on individual before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

#ifdef SLIM_NONWF_ONLY
EidosValue *Individual::GetProperty_Accelerated_age(EidosObjectElement **p_values, size_t p_values_size)
{
	if ((p_values_size > 0) && (((Individual *)(p_values[0]))->subpopulation_.population_.sim_.ModelType() == SLiMModelType::kModelTypeWF))
		EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property age is not available in WF models." << EidosTerminate();
	
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->age_, value_index);
	}
	
	return int_result;
}
#endif  // SLIM_NONWF_ONLY

EidosValue *Individual::GetProperty_Accelerated_tagF(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		double tagF_value = value->tagF_value_;
		
		if (tagF_value == SLIM_TAGF_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property tagF accessed on individual before being set." << EidosTerminate();
		
		float_result->set_float_no_check(tagF_value, value_index);
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_migrant(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		logical_result->set_logical_no_check(value->migrant_, value_index);
	}
	
	return logical_result;
}

EidosValue *Individual::GetProperty_Accelerated_fitnessScaling(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->fitness_scaling_, value_index);
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_x(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->spatial_x_, value_index);
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_y(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->spatial_y_, value_index);
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_z(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		float_result->set_float_no_check(value->spatial_z_, value_index);
	}
	
	return float_result;
}

EidosValue *Individual::GetProperty_Accelerated_subpopulation(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Subpopulation_Class))->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		object_result->set_object_element_no_check(&value->subpopulation_, value_index);
	}
	
	return object_result;
}

EidosValue *Individual::GetProperty_Accelerated_genome1(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		object_result->set_object_element_no_check(value->genome1_, value_index);
	}
	
	return object_result;
}

EidosValue *Individual::GetProperty_Accelerated_genome2(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Object_vector *object_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Genome_Class))->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		Individual *value = (Individual *)(p_values[value_index]);
		
		object_result->set_object_element_no_check(value->genome2_, value_index);
	}
	
	return object_result;
}

void Individual::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gEidosID_color:		// ACCELERATED
		{
			color_ = p_value.StringAtIndex(0, nullptr);
			if (!color_.empty())
			{
				Eidos_GetColorComponents(color_, &color_red_, &color_green_, &color_blue_);
				s_any_individual_color_set_ = true;		// keep track of the fact that an individual's color has been set
			}
			return;
		}
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			s_any_individual_or_genome_tag_set_ = true;
			return;
		}
		case gID_tagF:				// ACCELERATED
		{
			tagF_value_ = p_value.FloatAtIndex(0, nullptr);
			s_any_individual_or_genome_tag_set_ = true;
			return;
		}
		case gID_fitnessScaling:	// ACCELERATED
		{
			fitness_scaling_ = p_value.FloatAtIndex(0, nullptr);
			Individual::s_any_individual_fitness_scaling_set_ = true;
			
			if ((fitness_scaling_ < 0.0) || (std::isnan(fitness_scaling_)))
				EIDOS_TERMINATION << "ERROR (Individual::SetProperty): property fitnessScaling must be >= 0.0." << EidosTerminate();
			
			return;
		}
		case gEidosID_x:			// ACCELERATED
		{
			spatial_x_ = p_value.FloatAtIndex(0, nullptr);
			return;
		}
		case gEidosID_y:			// ACCELERATED
		{
			spatial_y_ = p_value.FloatAtIndex(0, nullptr);
			return;
		}
		case gEidosID_z:			// ACCELERATED
		{
			spatial_z_ = p_value.FloatAtIndex(0, nullptr);
			return;
		}
#ifdef SLIM_NONWF_ONLY
		case gID_age:				// ACCELERATED
		{
			slim_age_t value = SLiMCastToAgeTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			age_ = value;
			return;
		}
#endif  // SLIM_NONWF_ONLY
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SetProperty(p_property_id, p_value);
	}
}

void Individual::SetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	s_any_individual_or_genome_tag_set_ = true;
	
	// SLiMCastToUsertagTypeOrRaise() is a no-op at present
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->tag_value_ = source_value;
	}
	else
	{
		const int64_t *source_data = p_source.IntVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->tag_value_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_tagF(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	s_any_individual_or_genome_tag_set_ = true;
	
	// SLiMCastToUsertagTypeOrRaise() is a no-op at present
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->tagF_value_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->tagF_value_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_fitnessScaling(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	Individual::s_any_individual_fitness_scaling_set_ = true;
	
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex(0, nullptr);
		
		if ((source_value < 0.0) || (std::isnan(source_value)))
			EIDOS_TERMINATION << "ERROR (Individual::SetProperty_Accelerated_fitnessScaling): property fitnessScaling must be >= 0.0." << EidosTerminate();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->fitness_scaling_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			double source_value = source_data[value_index];
			
			if ((source_value < 0.0) || (std::isnan(source_value)))
				EIDOS_TERMINATION << "ERROR (Individual::SetProperty_Accelerated_fitnessScaling): property fitnessScaling must be >= 0.0." << EidosTerminate();
			
			((Individual *)(p_values[value_index]))->fitness_scaling_ = source_value;
		}
	}
}

void Individual::SetProperty_Accelerated_x(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_x_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_x_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_y(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_y_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_y_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_z(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		double source_value = p_source.FloatAtIndex(0, nullptr);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_z_ = source_value;
	}
	else
	{
		const double *source_data = p_source.FloatVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->spatial_z_ = source_data[value_index];
	}
}

void Individual::SetProperty_Accelerated_color(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		std::string &&source_value = p_source.StringAtIndex(0, nullptr);
		
		if (source_value.empty())
		{
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
				((Individual *)(p_values[value_index]))->color_ = source_value;
		}
		else
		{
			float color_red, color_green, color_blue;
			
			Eidos_GetColorComponents(source_value, &color_red, &color_green, &color_blue);
			
			for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			{
				Individual *individual = ((Individual *)(p_values[value_index]));
				
				individual->color_ = source_value;
				individual->color_red_ = color_red;
				individual->color_green_ = color_green;
				individual->color_blue_ = color_blue;
			}
			
			s_any_individual_color_set_ = true;		// keep track of the fact that an individual's color has been set
		}
	}
	else
	{
		const std::vector<std::string> *source_data = p_source.StringVector();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
		{
			Individual *individual = ((Individual *)(p_values[value_index]));
			const std::string &source_value = (*source_data)[value_index];
			
			individual->color_ = source_value;
			
			if (!source_value.empty())
			{
				Eidos_GetColorComponents(source_value, &individual->color_red_, &individual->color_green_, &individual->color_blue_);
				s_any_individual_color_set_ = true;		// keep track of the fact that an individual's color has been set
			}
		}
	}
}

#ifdef SLIM_NONWF_ONLY
void Individual::SetProperty_Accelerated_age(EidosObjectElement **p_values, size_t p_values_size, const EidosValue &p_source, size_t p_source_size)
{
	if (p_source_size == 1)
	{
		int64_t source_value = p_source.IntAtIndex(0, nullptr);
		slim_age_t source_age = SLiMCastToAgeTypeOrRaise(source_value);
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->age_ = source_age;
	}
	else
	{
		const int64_t *source_data = p_source.IntVector()->data();
		
		for (size_t value_index = 0; value_index < p_values_size; ++value_index)
			((Individual *)(p_values[value_index]))->age_ = SLiMCastToAgeTypeOrRaise(source_data[value_index]);
	}
}
#endif  // SLIM_NONWF_ONLY

EidosValue_SP Individual::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_containsMutations:			return ExecuteMethod_containsMutations(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_countOfMutationsOfType:	return ExecuteMethod_countOfMutationsOfType(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_relatedness:				return ExecuteMethod_relatedness(p_method_id, p_arguments, p_argument_count, p_interpreter);
		//case gID_sumOfMutationsOfType:	return ExecuteMethod_Accelerated_sumOfMutationsOfType(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_uniqueMutationsOfType:		return ExecuteMethod_uniqueMutationsOfType(p_method_id, p_arguments, p_argument_count, p_interpreter);
			
		default:
		{
			// In a sense, we here "subclass" SLiMEidosDictionary to override setValue(); we set a flag remembering that
			// an individual's dictionary has been modified, and then we call "super" for the usual behavior.
			if (p_method_id == gID_setValue)
				s_any_individual_dictionary_set_ = true;
			
			return SLiMEidosDictionary::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
		}
	}
}

//	*********************	- (logical)containsMutations(object<Mutation> mutations)
//
EidosValue_SP Individual::ExecuteMethod_containsMutations(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutations_value = p_arguments[0].get();
	int mutations_count = mutations_value->Count();
	
	if (mutations_count == 1)
	{
		MutationIndex mut = ((Mutation *)(mutations_value->ObjectElementAtIndex(0, nullptr)))->BlockIndex();
		
		if ((!genome1_->IsNull() && genome1_->contains_mutation(mut)) || (!genome2_->IsNull() && genome2_->contains_mutation(mut)))
			return gStaticEidosValue_LogicalT;
		else
			return gStaticEidosValue_LogicalF;
	}
	else
	{
		EidosValue_Logical *logical_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Logical())->resize_no_initialize(mutations_count);
		
		for (int value_index = 0; value_index < mutations_count; ++value_index)
		{
			MutationIndex mut = ((Mutation *)(mutations_value->ObjectElementAtIndex(value_index, nullptr)))->BlockIndex();
			bool contains_mut = ((!genome1_->IsNull() && genome1_->contains_mutation(mut)) || (!genome2_->IsNull() && genome2_->contains_mutation(mut)));
			
			logical_result->set_logical_no_check(contains_mut, value_index);
		}
		
		return EidosValue_SP(logical_result);
	}
	
	return gStaticEidosValueNULL;
}

//	*********************	- (integer$)countOfMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Individual::ExecuteMethod_countOfMutationsOfType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, sim, "countOfMutationsOfType()");
	
	// Count the number of mutations of the given type
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	int match_count = 0;
	
	if (!genome1_->IsNull())
	{
		int mutrun_count = genome1_->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			MutationRun *mutrun = genome1_->mutruns_[run_index].get();
			int genome1_count = mutrun->size();
			const MutationIndex *genome1_ptr = mutrun->begin_pointer_const();
			
			for (int mut_index = 0; mut_index < genome1_count; ++mut_index)
				if ((mut_block_ptr + genome1_ptr[mut_index])->mutation_type_ptr_ == mutation_type_ptr)
					++match_count;
		}
	}
	if (!genome2_->IsNull())
	{
		int mutrun_count = genome2_->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			MutationRun *mutrun = genome2_->mutruns_[run_index].get();
			int genome2_count = mutrun->size();
			const MutationIndex *genome2_ptr = mutrun->begin_pointer_const();
			
			for (int mut_index = 0; mut_index < genome2_count; ++mut_index)
				if ((mut_block_ptr + genome2_ptr[mut_index])->mutation_type_ptr_ == mutation_type_ptr)
					++match_count;
		}
	}
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(match_count));
}

//	*********************	- (float$)relatedness(o<Individual>$ individuals)
//
EidosValue_SP Individual::ExecuteMethod_relatedness(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *individuals_value = p_arguments[0].get();
	
	int individuals_count = individuals_value->Count();
	
	if (individuals_count == 1)
	{
		Individual *ind = (Individual *)(individuals_value->ObjectElementAtIndex(0, nullptr));
		double relatedness = RelatednessToIndividual(*ind);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(relatedness));
	}
	else
	{
		EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(individuals_count);
		
		for (int value_index = 0; value_index < individuals_count; ++value_index)
		{
			Individual *ind = (Individual *)(individuals_value->ObjectElementAtIndex(value_index, nullptr));
			double relatedness = RelatednessToIndividual(*ind);
			
			float_result->set_float_no_check(relatedness, value_index);
		}
		
		return EidosValue_SP(float_result);
	}
	
	return gStaticEidosValueNULL;
}

//	*********************	- (integer$)sumOfMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Individual::ExecuteMethod_Accelerated_sumOfMutationsOfType(EidosObjectElement **p_elements, size_t p_elements_size, EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, sim, "sumOfMutationsOfType()");
	
	// Count the number of mutations of the given type
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(p_elements_size);
	
	for (size_t element_index = 0; element_index < p_elements_size; ++element_index)
	{
		Individual *element = (Individual *)(p_elements[element_index]);
		Genome *genome1 = element->genome1_;
		Genome *genome2 = element->genome2_;
		double selcoeff_sum = 0.0;
		
		if (!genome1->IsNull())
		{
			int mutrun_count = genome1->mutrun_count_;
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				MutationRun *mutrun = genome1->mutruns_[run_index].get();
				int genome1_count = mutrun->size();
				const MutationIndex *genome1_ptr = mutrun->begin_pointer_const();
				
				for (int mut_index = 0; mut_index < genome1_count; ++mut_index)
				{
					Mutation *mut_ptr = mut_block_ptr + genome1_ptr[mut_index];
					
					if (mut_ptr->mutation_type_ptr_ == mutation_type_ptr)
						selcoeff_sum += mut_ptr->selection_coeff_;
				}
			}
		}
		if (!genome2->IsNull())
		{
			int mutrun_count = genome2->mutrun_count_;
			
			for (int run_index = 0; run_index < mutrun_count; ++run_index)
			{
				MutationRun *mutrun = genome2->mutruns_[run_index].get();
				int genome2_count = mutrun->size();
				const MutationIndex *genome2_ptr = mutrun->begin_pointer_const();
				
				for (int mut_index = 0; mut_index < genome2_count; ++mut_index)
				{
					Mutation *mut_ptr = mut_block_ptr + genome2_ptr[mut_index];
					
					if (mut_ptr->mutation_type_ptr_ == mutation_type_ptr)
						selcoeff_sum += mut_ptr->selection_coeff_;
				}
			}
		}
		
		float_result->set_float_no_check(selcoeff_sum, element_index);
	}
	
	return EidosValue_SP(float_result);
}

//	*********************	- (object<Mutation>)uniqueMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Individual::ExecuteMethod_uniqueMutationsOfType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, sim, "uniqueMutationsOfType()");
	
	// This code is adapted from uniqueMutations and follows its logic closely
	
	// We try to reserve a vector large enough to hold all the mutations; probably usually overkill, but it does little harm
	int genome1_size = (genome1_->IsNull() ? 0 : genome1_->mutation_count());
	int genome2_size = (genome2_->IsNull() ? 0 : genome2_->mutation_count());
	EidosValue_Object_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Mutation_Class));
	EidosValue_SP result_SP = EidosValue_SP(vec);
	
	if ((genome1_size == 0) && (genome2_size == 0))
		return result_SP;
	
	if (genome1_size + genome2_size < 100)			// an arbitrary limit, but we don't want to make something *too* unnecessarily big...
		vec->reserve(genome1_size + genome2_size);	// since we do not always reserve, we have to use push_object_element() below to check
	
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	int mutrun_count = (genome1_size ? genome1_->mutrun_count_ : genome2_->mutrun_count_);
	
	for (int run_index = 0; run_index < mutrun_count; ++run_index)
	{
		// We want to interleave mutations from the two genomes, keeping only the uniqued mutations.  For a given position, we take mutations
		// from g1 first, and then look at the mutations in g2 at the same position and add them if they are not in g1.
		MutationRun *mutrun1 = (genome1_size ? genome1_->mutruns_[run_index].get() : nullptr);
		MutationRun *mutrun2 = (genome2_size ? genome2_->mutruns_[run_index].get() : nullptr);
		int g1_size = (mutrun1 ? mutrun1->size() : 0);
		int g2_size = (mutrun2 ? mutrun2->size() : 0);
		int g1_index = 0, g2_index = 0;
		
		if (g1_size && g2_size)
		{
			MutationIndex g1_mut = (*mutrun1)[g1_index], g2_mut = (*mutrun2)[g2_index];
			
			// At this point, we need to loop forward in g1 and g2 until we have found mutations of the right type in both
			while ((mut_block_ptr + g1_mut)->mutation_type_ptr_ != mutation_type_ptr)
			{
				if (++g1_index >= g1_size)
					break;
				g1_mut = (*mutrun1)[g1_index];
			}
			
			while ((mut_block_ptr + g2_mut)->mutation_type_ptr_ != mutation_type_ptr)
			{
				if (++g2_index >= g2_size)
					break;
				g2_mut = (*mutrun2)[g2_index];
			}
			
			if ((g1_index < g1_size) && (g2_index < g2_size))
			{
				slim_position_t pos1 = (mut_block_ptr + g1_mut)->position_;
				slim_position_t pos2 = (mut_block_ptr + g2_mut)->position_;
				
				// Process mutations as long as both genomes still have mutations left in them
				do
				{
					// Now we have mutations of the right type, so we can start working with them by position
					if (pos1 < pos2)
					{
						vec->push_object_element(mut_block_ptr + g1_mut);
						
						// Move to the next mutation in g1
					loopback1:
						if (++g1_index >= g1_size)
							break;
						
						g1_mut = (*mutrun1)[g1_index];
						if ((mut_block_ptr + g1_mut)->mutation_type_ptr_ != mutation_type_ptr)
							goto loopback1;
						
						pos1 = (mut_block_ptr + g1_mut)->position_;
					}
					else if (pos1 > pos2)
					{
						vec->push_object_element(mut_block_ptr + g2_mut);
						
						// Move to the next mutation in g2
					loopback2:
						if (++g2_index >= g2_size)
							break;
						
						g2_mut = (*mutrun2)[g2_index];
						if ((mut_block_ptr + g2_mut)->mutation_type_ptr_ != mutation_type_ptr)
							goto loopback2;
						
						pos2 = (mut_block_ptr + g2_mut)->position_;
					}
					else
					{
						// pos1 == pos2; copy mutations from g1 until we are done with this position, then handle g2
						slim_position_t focal_pos = pos1;
						int first_index = g1_index;
						bool done = false;
						
						while (pos1 == focal_pos)
						{
							vec->push_object_element(mut_block_ptr + g1_mut);
							
							// Move to the next mutation in g1
						loopback3:
							if (++g1_index >= g1_size)
							{
								done = true;
								break;
							}
							g1_mut = (*mutrun1)[g1_index];
							if ((mut_block_ptr + g1_mut)->mutation_type_ptr_ != mutation_type_ptr)
								goto loopback3;
							
							pos1 = (mut_block_ptr + g1_mut)->position_;
						}
						
						// Note that we may be done with g1 here, so be careful
						int last_index_plus_one = g1_index;
						
						while (pos2 == focal_pos)
						{
							int check_index;
							
							for (check_index = first_index; check_index < last_index_plus_one; ++check_index)
								if ((*mutrun1)[check_index] == g2_mut)
									break;
							
							// If the check indicates that g2_mut is not in g1, we copy it over
							if (check_index == last_index_plus_one)
								vec->push_object_element(mut_block_ptr + g2_mut);
							
							// Move to the next mutation in g2
						loopback4:
							if (++g2_index >= g2_size)
							{
								done = true;
								break;
							}
							g2_mut = (*mutrun2)[g2_index];
							if ((mut_block_ptr + g2_mut)->mutation_type_ptr_ != mutation_type_ptr)
								goto loopback4;
							
							pos2 = (mut_block_ptr + g2_mut)->position_;
						}
						
						// Note that we may be done with both g1 and/or g2 here; if so, done will be set and we will break out
						if (done)
							break;
					}
				}
				while (true);
			}
		}
		
		// Finish off any tail ends, which must be unique and sorted already
		while (g1_index < g1_size)
		{
			MutationIndex mut = (*mutrun1)[g1_index++];
			
			if ((mut_block_ptr + mut)->mutation_type_ptr_ == mutation_type_ptr)
				vec->push_object_element(mut_block_ptr + mut);
		}
		while (g2_index < g2_size)
		{
			MutationIndex mut = (*mutrun2)[g2_index++];
			
			if ((mut_block_ptr + mut)->mutation_type_ptr_ == mutation_type_ptr)
				vec->push_object_element(mut_block_ptr + mut);
		}
	}
	
	return result_SP;
	/*
	 A SLiM model to test the above code:
	 
	 initialize() {
	 initializeMutationRate(1e-5);
	 initializeMutationType("m1", 0.5, "f", 0.0);
	 initializeMutationType("m2", 0.5, "f", 0.0);
	 initializeMutationType("m3", 0.5, "f", 0.0);
	 initializeGenomicElementType("g1", c(m1, m2, m3), c(1.0, 1.0, 1.0));
	 initializeGenomicElement(g1, 0, 99999);
	 initializeRecombinationRate(1e-8);
	 }
	 1 {
	 sim.addSubpop("p1", 500);
	 }
	 1:20000 late() {
	 for (i in p1.individuals)
	 {
	 // check m1
	 um1 = i.uniqueMutationsOfType(m1);
	 um2 = sortBy(unique(i.genomes.mutationsOfType(m1)), "position");
	 
	 if (!identical(um1.position, um2.position))
	 {
	 print("Mismatch for m1!");
	 print(um1.position);
	 print(um2.position);
	 }
	 }
	 }
	 */
}


//
//	Individual_Class
//
#pragma mark -
#pragma mark Individual_Class
#pragma mark -

class Individual_Class : public SLiMEidosDictionary_Class
{
public:
	Individual_Class(const Individual_Class &p_original) = delete;	// no copy-construct
	Individual_Class& operator=(const Individual_Class&) = delete;	// no copying
	inline Individual_Class(void) { }
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
	EidosValue_SP ExecuteMethod_setSpatialPosition(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_Individual_Class = new Individual_Class();


const std::string &Individual_Class::ElementType(void) const
{
	return gEidosStr_Individual;		// in Eidos; see EidosValue_Object::EidosValue_Object()
}

const std::vector<const EidosPropertySignature *> *Individual_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*SLiMEidosDictionary_Class::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopulation,			true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Subpopulation_Class))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_subpopulation));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_index,					true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_index));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genomes,				true,	kEidosValueMaskObject, gSLiM_Genome_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genome1,				true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Genome_Class))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_genome1));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_genome2,				true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Genome_Class))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_genome2));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_sex,					true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_tag)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_tag));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tagF,					false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_tagF)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_tagF));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_migrant,				true,	kEidosValueMaskLogical | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_migrant));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_fitnessScaling,			false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_fitnessScaling)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_fitnessScaling));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_x,					false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_x)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_x));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_y,					false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_y)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_y));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_z,					false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_z)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_z));
#ifdef SLIM_NONWF_ONLY
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_age,					false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_age)->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_age));
#endif  // SLIM_NONWF_ONLY
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_pedigreeID,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(Individual::GetProperty_Accelerated_pedigreeID));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_pedigreeParentIDs,		true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_pedigreeGrandparentIDs,	true,	kEidosValueMaskInt)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_spatialPosition,		true,	kEidosValueMaskFloat)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_uniqueMutations,		true,	kEidosValueMaskObject, gSLiM_Mutation_Class)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_color,				false,	kEidosValueMaskString | kEidosValueMaskSingleton))->DeclareAcceleratedSet(Individual::SetProperty_Accelerated_color));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<const EidosMethodSignature *> *Individual_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*SLiMEidosDictionary_Class::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_containsMutations, kEidosValueMaskLogical))->AddObject("mutations", gSLiM_Mutation_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_countOfMutationsOfType, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_relatedness, kEidosValueMaskFloat))->AddObject("individuals", gSLiM_Individual_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosClassMethodSignature(gStr_setSpatialPosition, kEidosValueMaskVOID))->AddFloat("position"));
		methods->emplace_back(((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_sumOfMutationsOfType, kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class))->DeclareAcceleratedImp(Individual::ExecuteMethod_Accelerated_sumOfMutationsOfType));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_uniqueMutationsOfType, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

EidosValue_SP Individual_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	switch (p_method_id)
	{
		case gID_setSpatialPosition:	return ExecuteMethod_setSpatialPosition(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
		default:						return SLiMEidosDictionary_Class::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
	}
}

//	*********************	– (void)setSpatialPosition(float position)
//
EidosValue_SP Individual_Class::ExecuteMethod_setSpatialPosition(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *position_value = p_arguments[0].get();
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	int dimensionality = sim.SpatialDimensionality();
	int value_count = position_value->Count();
	int target_size = p_target->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_setSpatialPosition): setSpatialPosition() cannot be called in non-spatial simulations." << EidosTerminate();
	if ((dimensionality < 0) || (dimensionality > 3))
		EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_setSpatialPosition): (internal error) unrecognized dimensionality." << EidosTerminate();
	
	if (value_count < dimensionality)
		EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_setSpatialPosition): setSpatialPosition() requires at least as many coordinates as the spatial dimensionality of the simulation." << EidosTerminate();
	
	if (value_count == dimensionality)
	{
		// One point is being set across all targets
		if (target_size == 1)
		{
			// Handle the singleton target case separately so we can handle the vector target case faster
			Individual *target = (Individual *)p_target->ObjectElementAtIndex(0, nullptr);
			
			switch (dimensionality)
			{
				case 1:
					target->spatial_x_ = position_value->FloatAtIndex(0, nullptr);
					break;
				case 2:
					target->spatial_x_ = position_value->FloatAtIndex(0, nullptr);
					target->spatial_y_ = position_value->FloatAtIndex(1, nullptr);
					break;
				case 3:
					target->spatial_x_ = position_value->FloatAtIndex(0, nullptr);
					target->spatial_y_ = position_value->FloatAtIndex(1, nullptr);
					target->spatial_z_ = position_value->FloatAtIndex(2, nullptr);
					break;
			}
		}
		else
		{
			// Vector target case, one point
			const EidosValue_Object_vector *target_vec = p_target->ObjectElementVector();
			Individual * const *targets = (Individual * const *)(target_vec->data());
			
			switch (dimensionality)
			{
				case 1:
				{
					double x = position_value->FloatAtIndex(0, nullptr);
					for (int target_index = 0; target_index < target_size; ++target_index)
					{
						Individual *target = targets[target_index];
						target->spatial_x_ = x;
					}
					break;
				}
				case 2:
				{
					double x = position_value->FloatAtIndex(0, nullptr);
					double y = position_value->FloatAtIndex(1, nullptr);
					for (int target_index = 0; target_index < target_size; ++target_index)
					{
						Individual *target = targets[target_index];
						target->spatial_x_ = x;
						target->spatial_y_ = y;
					}
					break;
				}
				case 3:
				{
					double x = position_value->FloatAtIndex(0, nullptr);
					double y = position_value->FloatAtIndex(1, nullptr);
					double z = position_value->FloatAtIndex(2, nullptr);
					for (int target_index = 0; target_index < target_size; ++target_index)
					{
						Individual *target = targets[target_index];
						target->spatial_x_ = x;
						target->spatial_y_ = y;
						target->spatial_z_ = z;
					}
					break;
				}
			}
		}
	}
	else if (value_count == dimensionality * target_size)
	{
		// Vector target case, one point per target (so the point vector has to be non-singleton too)
		const EidosValue_Object_vector *target_vec = p_target->ObjectElementVector();
		Individual * const *targets = (Individual * const *)(target_vec->data());
		const EidosValue_Float_vector *position_vec = position_value->FloatVector();
		const double *positions = position_vec->data();
		
		switch (dimensionality)
		{
			case 1:
			{
				for (int target_index = 0; target_index < target_size; ++target_index)
				{
					Individual *target = targets[target_index];
					target->spatial_x_ = *(positions++);
				}
				break;
			}
			case 2:
			{
				for (int target_index = 0; target_index < target_size; ++target_index)
				{
					Individual *target = targets[target_index];
					target->spatial_x_ = *(positions++);
					target->spatial_y_ = *(positions++);
				}
				break;
			}
			case 3:
			{
				for (int target_index = 0; target_index < target_size; ++target_index)
				{
					Individual *target = targets[target_index];
					target->spatial_x_ = *(positions++);
					target->spatial_y_ = *(positions++);
					target->spatial_z_ = *(positions++);
				}
				break;
			}
		}
	}
	else
	{
		EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_setSpatialPosition): setSpatialPosition() requires the position parameter to contain either one point, or one point per individual (where each point has a number of coordinates equal to the spatial dimensionality of the simulation)." << EidosTerminate();
	}
	
	return gStaticEidosValueVOID;
}			












































