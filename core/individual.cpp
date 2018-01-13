//
//  individual.cpp
//  SLiM
//
//  Created by Ben Haller on 6/10/16.
//  Copyright (c) 2016-2017 Philipp Messer.  All rights reserved.
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
slim_mutationid_t gSLiM_next_pedigree_id = 0;

// A global flag used to indicate whether custom colors have ever been used by Individual, to save work in the display code
bool gSLiM_Individual_custom_colors = false;


Individual::Individual(Subpopulation &p_subpopulation, slim_popsize_t p_individual_index, slim_mutationid_t p_pedigree_id, Genome *p_genome1, Genome *p_genome2, IndividualSex p_sex, slim_generation_t p_age) : subpopulation_(p_subpopulation), index_(p_individual_index), genome1_(p_genome1), genome2_(p_genome2), sex_(p_sex),
#ifdef SLIM_NONWF_ONLY
	age_(p_age),
#endif  // SLIM_NONWF_ONLY
	pedigree_id_(p_pedigree_id), pedigree_p1_(-1), pedigree_p2_(-1), pedigree_g1_(-1), pedigree_g2_(-1), pedigree_g3_(-1), pedigree_g4_(-1)
{
}

Individual::~Individual(void)
{
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
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(pedigree_id_));
		}
		case gID_pedigreeParentIDs:
		{
			EidosValue_Int_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(2);
			
			vec->set_int_no_check(pedigree_p1_, 0);
			vec->set_int_no_check(pedigree_p2_, 1);
			
			return EidosValue_SP(vec);
		}
		case gID_pedigreeGrandparentIDs:
		{
			EidosValue_Int_vector *vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(4);
			
			vec->set_int_no_check(pedigree_g1_, 0);
			vec->set_int_no_check(pedigree_g2_, 1);
			vec->set_int_no_check(pedigree_g2_, 2);
			vec->set_int_no_check(pedigree_g2_, 3);
			
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
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value_));
		}
		case gID_tagF:				// ACCELERATED
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(tagF_value_));
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

int64_t Individual::GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_index:			return index_;
		case gID_pedigreeID:	return pedigree_id_;
		case gID_tag:			return tag_value_;
			
#ifdef SLIM_NONWF_ONLY
		case gID_age:
		{
			if (age_ == -1)
				EIDOS_TERMINATION << "ERROR (Individual::GetProperty): property age is not available in WF models." << EidosTerminate();
			
			return age_;
		}
#endif  // SLIM_NONWF_ONLY
			
		default:				return EidosObjectElement::GetProperty_Accelerated_Int(p_property_id);
	}
}

double Individual::GetProperty_Accelerated_Float(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_tagF:				return tagF_value_;
		case gID_fitnessScaling:	return fitness_scaling_;
		case gEidosID_x:			return spatial_x_;
		case gEidosID_y:			return spatial_y_;
		case gEidosID_z:			return spatial_z_;
			
		default:					return EidosObjectElement::GetProperty_Accelerated_Float(p_property_id);
	}
}

EidosObjectElement *Individual::GetProperty_Accelerated_ObjectElement(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_subpopulation:	return &subpopulation_;
			
		default:				return EidosObjectElement::GetProperty_Accelerated_ObjectElement(p_property_id);
	}
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
				gSLiM_Individual_custom_colors = true;	// notify the display code that custom colors are being used
			}
			return;
		}
		case gID_tag:				// ACCELERATED
		{
			slim_usertag_t value = SLiMCastToUsertagTypeOrRaise(p_value.IntAtIndex(0, nullptr));
			
			tag_value_ = value;
			return;
		}
		case gID_tagF:				// ACCELERATED
		{
			tagF_value_ = p_value.FloatAtIndex(0, nullptr);
			return;
		}
		case gID_fitnessScaling:	// ACCELERATED
		{
			fitness_scaling_ = p_value.FloatAtIndex(0, nullptr);
			
			if ((fitness_scaling_ < 0.0) || (!std::isfinite(fitness_scaling_)))
				EIDOS_TERMINATION << "ERROR (Individual::SetProperty): property fitnessScaling must have a finite value >= 0.0." << EidosTerminate();
			
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
			
			// all others, including gID_none
		default:
			return EidosObjectElement::SetProperty(p_property_id, p_value);
	}
}

void Individual::SetProperty_Accelerated_Int(EidosGlobalStringID p_property_id, int64_t p_value)
{
	switch (p_property_id)
	{
		case gID_tag:			tag_value_ = p_value; return;	// SLiMCastToUsertagTypeOrRaise() is a no-op at present
			
		default:				return EidosObjectElement::SetProperty_Accelerated_Int(p_property_id, p_value);
	}
}

void Individual::SetProperty_Accelerated_Float(EidosGlobalStringID p_property_id, double p_value)
{
	switch (p_property_id)
	{
		case gID_tagF:				tagF_value_ = p_value; return;
		case gID_fitnessScaling:
		{
			fitness_scaling_ = p_value;
			
			if ((fitness_scaling_ < 0.0) || (!std::isfinite(fitness_scaling_)))
				EIDOS_TERMINATION << "ERROR (Subpopulation::SetProperty_Accelerated_Float): property fitnessScaling must have a finite value >= 0.0." << EidosTerminate();
			
			return;
		}
		case gEidosID_x:			spatial_x_ = p_value; return;
		case gEidosID_y:			spatial_y_ = p_value; return;
		case gEidosID_z:			spatial_z_ = p_value; return;
			
		default:					return EidosObjectElement::SetProperty_Accelerated_Float(p_property_id, p_value);
	}
}

void Individual::SetProperty_Accelerated_String(EidosGlobalStringID p_property_id, const std::string &p_value)
{
	switch (p_property_id)
	{
		case gEidosID_color:
			color_ = p_value;
			if (!color_.empty())
			{
				Eidos_GetColorComponents(color_, &color_red_, &color_green_, &color_blue_);
				gSLiM_Individual_custom_colors = true;	// notify the display code that custom colors are being used
			}
			return;
			
		default:				return EidosObjectElement::SetProperty_Accelerated_String(p_property_id, p_value);
	}
}

EidosValue_SP Individual::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_containsMutations:			return ExecuteMethod_containsMutations(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_countOfMutationsOfType:	return ExecuteMethod_countOfMutationsOfType(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_relatedness:				return ExecuteMethod_relatedness(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_setSpatialPosition:		return ExecuteMethod_setSpatialPosition(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_sumOfMutationsOfType:		return ExecuteMethod_sumOfMutationsOfType(p_method_id, p_arguments, p_argument_count, p_interpreter);
		case gID_uniqueMutationsOfType:		return ExecuteMethod_uniqueMutationsOfType(p_method_id, p_arguments, p_argument_count, p_interpreter);
		default:							return SLiMEidosDictionary::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
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

//	*********************	– (void)setSpatialPosition(float position)
//
EidosValue_SP Individual::ExecuteMethod_setSpatialPosition(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *position_value = p_arguments[0].get();
	SLiMSim &sim = subpopulation_.population_.sim_;
	
	int dimensionality = sim.SpatialDimensionality();
	int value_count = position_value->Count();
	
	if (dimensionality == 0)
		EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_setSpatialPosition): setSpatialPosition() cannot be called in non-spatial simulations." << EidosTerminate();
	
	if (value_count < dimensionality)
		EIDOS_TERMINATION << "ERROR (Individual::ExecuteMethod_setSpatialPosition): setSpatialPosition() requires at least as many coordinates as the spatial dimensionality of the simulation." << EidosTerminate();
	
	switch (dimensionality)
	{
		case 1:
			spatial_x_ = position_value->FloatAtIndex(0, nullptr);
			break;
		case 2:
			spatial_x_ = position_value->FloatAtIndex(0, nullptr);
			spatial_y_ = position_value->FloatAtIndex(1, nullptr);
			break;
		case 3:
			spatial_x_ = position_value->FloatAtIndex(0, nullptr);
			spatial_y_ = position_value->FloatAtIndex(1, nullptr);
			spatial_z_ = position_value->FloatAtIndex(2, nullptr);
			break;
	}
	
	return gStaticEidosValueNULLInvisible;
}			

//	*********************	- (integer$)sumOfMutationsOfType(io<MutationType>$ mutType)
//
EidosValue_SP Individual::ExecuteMethod_sumOfMutationsOfType(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_argument_count, p_interpreter)
	EidosValue *mutType_value = p_arguments[0].get();
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	MutationType *mutation_type_ptr = SLiM_ExtractMutationTypeFromEidosValue_io(mutType_value, 0, sim, "sumOfMutationsOfType()");
	
	// Count the number of mutations of the given type
	Mutation *mut_block_ptr = gSLiM_Mutation_Block;
	double selcoeff_sum = 0.0;
	
	if (!genome1_->IsNull())
	{
		int mutrun_count = genome1_->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			MutationRun *mutrun = genome1_->mutruns_[run_index].get();
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
	if (!genome2_->IsNull())
	{
		int mutrun_count = genome2_->mutrun_count_;
		
		for (int run_index = 0; run_index < mutrun_count; ++run_index)
		{
			MutationRun *mutrun = genome2_->mutruns_[run_index].get();
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
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(selcoeff_sum));
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
	
	Individual_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_Individual_Class = new Individual_Class();


Individual_Class::Individual_Class(void)
{
}

const std::string &Individual_Class::ElementType(void) const
{
	return gEidosStr_Individual;		// in Eidos; see EidosValue_Object::EidosValue_Object()
}

const std::vector<const EidosPropertySignature *> *Individual_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->emplace_back(SignatureForPropertyOrRaise(gID_subpopulation));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_index));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_genomes));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_sex));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tag));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tagF));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_fitnessScaling));
		properties->emplace_back(SignatureForPropertyOrRaise(gEidosID_x));
		properties->emplace_back(SignatureForPropertyOrRaise(gEidosID_y));
		properties->emplace_back(SignatureForPropertyOrRaise(gEidosID_z));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_age));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_pedigreeID));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_pedigreeParentIDs));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_pedigreeGrandparentIDs));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_spatialPosition));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_uniqueMutations));
		properties->emplace_back(SignatureForPropertyOrRaise(gEidosID_color));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *Individual_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *subpopulationSig = nullptr;
	static EidosPropertySignature *indexSig = nullptr;
	static EidosPropertySignature *genomesSig = nullptr;
	static EidosPropertySignature *sexSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	static EidosPropertySignature *tagFSig = nullptr;
	static EidosPropertySignature *fitnessScalingSig = nullptr;
	static EidosPropertySignature *xSig = nullptr;
	static EidosPropertySignature *ySig = nullptr;
	static EidosPropertySignature *zSig = nullptr;
	static EidosPropertySignature *ageSig = nullptr;
	static EidosPropertySignature *pedigreeIDSig = nullptr;
	static EidosPropertySignature *pedigreeParentIDsSig = nullptr;
	static EidosPropertySignature *pedigreeGrandparentIDsSig = nullptr;
	static EidosPropertySignature *spatialPositionSig = nullptr;
	static EidosPropertySignature *uniqueMutationsSig = nullptr;
	static EidosPropertySignature *colorSig = nullptr;
	
	if (!subpopulationSig)
	{
		subpopulationSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_subpopulation,			gID_subpopulation,				true,	kEidosValueMaskObject | kEidosValueMaskSingleton, gSLiM_Subpopulation_Class))->DeclareAcceleratedGet();
		indexSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_index,					gID_index,						true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		genomesSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_genomes,					gID_genomes,					true,	kEidosValueMaskObject, gSLiM_Genome_Class));
		sexSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_sex,						gID_sex,						true,	kEidosValueMaskString | kEidosValueMaskSingleton));
		tagSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,						gID_tag,						false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet()->DeclareAcceleratedSet();
		tagFSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_tagF,					gID_tagF,						false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet()->DeclareAcceleratedSet();
		fitnessScalingSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_fitnessScaling,			gID_fitnessScaling,				false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet()->DeclareAcceleratedSet();
		xSig =							(EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_x,					gEidosID_x,						false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet()->DeclareAcceleratedSet();
		ySig =							(EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_y,					gEidosID_y,						false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet()->DeclareAcceleratedSet();
		zSig =							(EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_z,					gEidosID_z,						false,	kEidosValueMaskFloat | kEidosValueMaskSingleton))->DeclareAcceleratedGet()->DeclareAcceleratedSet();
		ageSig =						(EidosPropertySignature *)(new EidosPropertySignature(gStr_age,						gID_age,						true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		pedigreeIDSig =					(EidosPropertySignature *)(new EidosPropertySignature(gStr_pedigreeID,				gID_pedigreeID,					true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		pedigreeParentIDsSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_pedigreeParentIDs,		gID_pedigreeParentIDs,			true,	kEidosValueMaskInt));
		pedigreeGrandparentIDsSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_pedigreeGrandparentIDs,	gID_pedigreeGrandparentIDs,		true,	kEidosValueMaskInt));
		spatialPositionSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_spatialPosition,			gID_spatialPosition,			true,	kEidosValueMaskFloat));
		uniqueMutationsSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_uniqueMutations,			gID_uniqueMutations,			true,	kEidosValueMaskObject, gSLiM_Mutation_Class));
		colorSig =						(EidosPropertySignature *)(new EidosPropertySignature(gEidosStr_color,				gEidosID_color,					false,	kEidosValueMaskString | kEidosValueMaskSingleton))->DeclareAcceleratedSet();
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_subpopulation:				return subpopulationSig;
		case gID_index:						return indexSig;
		case gID_genomes:					return genomesSig;
		case gID_sex:						return sexSig;
		case gID_tag:						return tagSig;
		case gID_tagF:						return tagFSig;
		case gID_fitnessScaling:			return fitnessScalingSig;
		case gEidosID_x:					return xSig;
		case gEidosID_y:					return ySig;
		case gEidosID_z:					return zSig;
		case gID_age:						return ageSig;
		case gID_pedigreeID:				return pedigreeIDSig;
		case gID_pedigreeParentIDs:			return pedigreeParentIDsSig;
		case gID_pedigreeGrandparentIDs:	return pedigreeGrandparentIDsSig;
		case gID_spatialPosition:			return spatialPositionSig;
		case gID_uniqueMutations:			return uniqueMutationsSig;
		case gEidosID_color:				return colorSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *Individual_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*SLiMEidosDictionary_Class::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gID_containsMutations));
		methods->emplace_back(SignatureForMethodOrRaise(gID_countOfMutationsOfType));
		methods->emplace_back(SignatureForMethodOrRaise(gID_relatedness));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setSpatialPosition));
		methods->emplace_back(SignatureForMethodOrRaise(gID_sumOfMutationsOfType));
		methods->emplace_back(SignatureForMethodOrRaise(gID_uniqueMutationsOfType));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *Individual_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *containsMutationsSig = nullptr;
	static EidosInstanceMethodSignature *countOfMutationsOfTypeSig = nullptr;
	static EidosInstanceMethodSignature *relatednessSig = nullptr;
	static EidosInstanceMethodSignature *setSpatialPositionSig = nullptr;
	static EidosInstanceMethodSignature *sumOfMutationsOfTypeSig = nullptr;
	static EidosInstanceMethodSignature *uniqueMutationsOfTypeSig = nullptr;
	
	if (!containsMutationsSig)
	{
		containsMutationsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_containsMutations, kEidosValueMaskLogical))->AddObject("mutations", gSLiM_Mutation_Class);
		countOfMutationsOfTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_countOfMutationsOfType, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class);
		relatednessSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_relatedness, kEidosValueMaskFloat))->AddObject("individuals", gSLiM_Individual_Class);
		setSpatialPositionSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setSpatialPosition, kEidosValueMaskNULL))->AddFloat("position");
		sumOfMutationsOfTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_sumOfMutationsOfType, kEidosValueMaskFloat | kEidosValueMaskSingleton))->AddIntObject_S("mutType", gSLiM_MutationType_Class);
		uniqueMutationsOfTypeSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_uniqueMutationsOfType, kEidosValueMaskObject, gSLiM_Mutation_Class))->AddIntObject_S("mutType", gSLiM_MutationType_Class);
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_method_id)
	{
		case gID_containsMutations:			return containsMutationsSig;
		case gID_countOfMutationsOfType:	return countOfMutationsOfTypeSig;
		case gID_relatedness:				return relatednessSig;
		case gID_setSpatialPosition:		return setSpatialPositionSig;
		case gID_sumOfMutationsOfType:		return sumOfMutationsOfTypeSig;
		case gID_uniqueMutationsOfType:		return uniqueMutationsOfTypeSig;
			
			// all others, including gID_none
		default:
			return SLiMEidosDictionary_Class::SignatureForMethod(p_method_id);
	}
}

EidosValue_SP Individual_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
}












































