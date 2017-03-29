//
//  interaction_type.cpp
//  SLiM
//
//  Created by Ben Haller on 2/25/17.
//  Copyright (c) 2017 Philipp Messer.  All rights reserved.
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


#include "interaction_type.h"
#include "eidos_call_signature.h"
#include "eidos_property_signature.h"
#include "slim_eidos_block.h"
#include "subpopulation.h"
#include "slim_sim.h"

#include <utility>
#include <algorithm>


// stream output for enumerations
std::ostream& operator<<(std::ostream& p_out, IFType p_if_type)
{
	switch (p_if_type)
	{
		case IFType::kFixed:			p_out << gStr_f;		break;
		case IFType::kLinear:			p_out << gStr_l;		break;
		case IFType::kExponential:		p_out << gStr_e;		break;
		case IFType::kNormal:			p_out << gEidosStr_n;	break;
	}
	
	return p_out;
}


InteractionType::InteractionType(slim_objectid_t p_interaction_type_id, std::string p_spatiality_string, bool p_reciprocal, double p_max_distance, IndividualSex p_receiver_sex, IndividualSex p_exerter_sex) :
	interaction_type_id_(p_interaction_type_id), spatiality_string_(p_spatiality_string), reciprocal_(p_reciprocal), max_distance_(p_max_distance), max_distance_sq_(p_max_distance * p_max_distance), receiver_sex_(p_receiver_sex), exerter_sex_(p_exerter_sex), if_type_(IFType::kFixed), if_param1_(1.0), if_param2_(0.0),
	self_symbol_(EidosGlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('i', p_interaction_type_id)),
				 EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_InteractionType_Class)))
{
	// Figure out our spatiality, which is the number of spatial dimensions we actively use for distances
	if (spatiality_string_ == "")
		spatiality_ = 0;
	else if ((spatiality_string_ == "x") || (spatiality_string_ == "y") || (spatiality_string_ == "z"))
		spatiality_ = 1;
	else if ((spatiality_string_ == "xy") || (spatiality_string_ == "xz") || (spatiality_string_ == "yz"))
		spatiality_ = 2;
	else if (spatiality_string_ == "xyz")
		spatiality_ = 3;
	else
		EIDOS_TERMINATION << "ERROR (InteractionType::InteractionType): illegal spatiality string value" << eidos_terminate();
	
	// Correct our reciprocality for sex-segregation.  Basically, male-male, female-female, and *-* interactions
	// can be reciprocal because the receivers are the same set of individuals as the exerters; others cannot be,
	// although some subset of the full interaction may indeed be reciprocal (we're not smart enough to use that).
	if (receiver_sex_ != exerter_sex_)
		reciprocal_ = false;
}

InteractionType::~InteractionType(void)
{
}

void InteractionType::EvaluateSubpopulation(Subpopulation *p_subpop, bool p_immediate)
{
	slim_objectid_t subpop_id = p_subpop->subpopulation_id_;
	slim_popsize_t subpop_size = p_subpop->parent_subpop_size_;
	Individual *subpop_individuals = p_subpop->parent_individuals_.data();
	
	auto data_iter = data_.find(subpop_id);
	InteractionsData *subpop_data;
	
	if (data_iter == data_.end())
	{
		// No entry in our map table for this subpop_id, so we need to make a new entry
		subpop_data = &(data_.insert(std::pair<slim_objectid_t, InteractionsData>(subpop_id, InteractionsData(p_subpop->parent_subpop_size_, p_subpop->parent_first_male_index_))).first->second);
	}
	else
	{
		// There is an existing entry, so we need to rehabilitate that entry by recycling its elements safely
		subpop_data = &(data_iter->second);
		
		if (subpop_data->individual_count_ != subpop_size)
		{
			// The population has changed size, so we will realloc buffers as needed.  If buffers have not yet been
			// allocated, we don't need to allocate them now; we will continue to defer until they are needed.
			int matrix_size = subpop_size * subpop_size;
			
			if (subpop_data->distances_)
				subpop_data->distances_ = (double *)realloc(subpop_data->distances_, matrix_size * sizeof(double));
			
			if (subpop_data->strengths_)
				subpop_data->strengths_ = (double *)realloc(subpop_data->strengths_, matrix_size * sizeof(double));
			
			subpop_data->individual_count_ = subpop_size;
		}
		
		subpop_data->first_male_index_ = p_subpop->parent_first_male_index_;
		
		// Ensure that other parts of the subpop data block are correctly reset to the same state that Invalidate()
		// uses; normally this has already been done by Initialize(), but not necessarily.
		if (subpop_data->positions_)
		{
			free(subpop_data->positions_);
			subpop_data->positions_ = nullptr;
		}
		
		if (subpop_data->kd_nodes_)
		{
			free(subpop_data->kd_nodes_);
			subpop_data->kd_nodes_ = nullptr;
		}
		
		subpop_data->kd_root_ = nullptr;
		
		subpop_data->evaluation_interaction_callbacks_.clear();
	}
	
	// At this point, positions_ is guaranteed to be nullptr; distances_ and strengths_ are either (1) nullptr,
	// or (2) allocated, but containing garbage.  Now we mark ourselves evaluated and fill in the buffers as needed.
	subpop_data->evaluated_ = true;
	
	// At a minimum, fetch positional data from the subpopulation; this is guaranteed to be present (for spatiality > 0)
	if (spatiality_ > 0)
	{
		double *positions = (double *)malloc(subpop_size * SLIM_MAX_DIMENSIONALITY * sizeof(double));
		
		subpop_data->positions_ = positions;
		
		int ind_index = 0;
		Individual *individual = subpop_individuals;
		double *ind_positions = positions;
		
		// IMPORTANT: This is the only place in InteractionType's code where the spatial position of the individuals is
		// accessed.  We cache all positions here, and then use the cache everywhere else.  This means that except for
		// here, we can treat "x", "y", and "z" identically as 1D spatiality, "xy", "xz", and "yz" identically as 2D
		// spatiality, and of course "xyz" as 3D spatiality.  We do that by storing the cached position information in
		// the same slots regardless of which original coordinates it represents.  This also means that this is the
		// only place in the code where spatiality_string_ should be used (apart from the property accessor); everywhere
		// else, spatiality_ should suffice.  Be careful to keep following this convention, and the different spatiality
		// values will just automatically work.
		if (spatiality_string_ == "x")
		{
			while (ind_index < subpop_size)
			{
				ind_positions[0] = individual->spatial_x_;
				++ind_index, ++individual, ind_positions += SLIM_MAX_DIMENSIONALITY;
			}
		}
		else if (spatiality_string_ == "y")
		{
			while (ind_index < subpop_size)
			{
				ind_positions[0] = individual->spatial_y_;
				++ind_index, ++individual, ind_positions += SLIM_MAX_DIMENSIONALITY;
			}
		}
		else if (spatiality_string_ == "z")
		{
			while (ind_index < subpop_size)
			{
				ind_positions[0] = individual->spatial_z_;
				++ind_index, ++individual, ind_positions += SLIM_MAX_DIMENSIONALITY;
			}
		}
		else if (spatiality_string_ == "xy")
		{
			while (ind_index < subpop_size)
			{
				ind_positions[0] = individual->spatial_x_;
				ind_positions[1] = individual->spatial_y_;
				++ind_index, ++individual, ind_positions += SLIM_MAX_DIMENSIONALITY;
			}
		}
		else if (spatiality_string_ == "xz")
		{
			while (ind_index < subpop_size)
			{
				ind_positions[0] = individual->spatial_x_;
				ind_positions[1] = individual->spatial_z_;
				++ind_index, ++individual, ind_positions += SLIM_MAX_DIMENSIONALITY;
			}
		}
		else if (spatiality_string_ == "yz")
		{
			while (ind_index < subpop_size)
			{
				ind_positions[0] = individual->spatial_y_;
				ind_positions[1] = individual->spatial_z_;
				++ind_index, ++individual, ind_positions += SLIM_MAX_DIMENSIONALITY;
			}
		}
		else if (spatiality_string_ == "xyz")
		{
			while (ind_index < subpop_size)
			{
				ind_positions[0] = individual->spatial_x_;
				ind_positions[1] = individual->spatial_y_;
				ind_positions[2] = individual->spatial_z_;
				++ind_index, ++individual, ind_positions += SLIM_MAX_DIMENSIONALITY;
			}
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (InteractionType::EvaluateSubpopulation): (internal error) illegal spatiality string value" << eidos_terminate();
		}
	}
	
	// Cache the interaction() callbacks applicable at this moment, for this subpopulation and this interaction type
	SLiMSim &sim = p_subpop->population_.sim_;
	slim_generation_t generation = sim.Generation();
	
	subpop_data->evaluation_interaction_callbacks_ = sim.ScriptBlocksMatching(generation, SLiMEidosBlockType::SLiMEidosInteractionCallback, -1, interaction_type_id_, subpop_id);
	
	// If we're supposed to evaluate it immediately, do so
	if (p_immediate)
	{
		// We do not set up the kd-tree here, because we don't know whether or not we'll use it, and we have all the
		// information we need to set it up later (since it doesn't depend on interactions or even distances).
		// Note that if distances_ and strengths_ are allocated, they contain garbage; CalculateAllInteractions()
		// expects that, so we don't fix it in this code path, to avoid writing values to memory twice.
		CalculateAllInteractions(p_subpop);
	}
	else
	{
		// We don't know whether we will be queried about distances or strengths at all; the user may only be interested in
		// using the kd-tree facility to do nearest-neighbor searches.  So we do not allocate distances_ or strengths_ here.
		// If they are already allocated, however, they presently contain garbage, so we have to clean them up.
		if (subpop_data->distances_)
			InitializeDistances(*subpop_data);
		
		if (subpop_data->strengths_)
			InitializeStrengths(*subpop_data);
	}
}

bool InteractionType::AnyEvaluated(void)
{
	for (auto &data_iter : data_)
	{
		InteractionsData &data = data_iter.second;
		
		if (data.evaluated_)
			return true;
	}
	
	return false;
}

void InteractionType::Invalidate(void)
{
	// Called by SLiM when the old generation goes away; should invalidate all evaluation.  We avoid actually freeing the
	// big blocks if possible, though, since that can incur large overhead from madvise() – see header comments.  We do free
	// the positional data and the k-d tree, though, in an attempt to make fatal errors occur if somebody doesn't manage
	// the buffers and evaluated state correctly.  They should be smaller, and thus not trigger madvise(), anyway.
	for (auto &data_iter : data_)
	{
		InteractionsData &data = data_iter.second;
		data.evaluated_ = false;
		
		if (data.positions_)
		{
			free(data.positions_);
			data.positions_ = nullptr;
		}
		
		if (data.kd_nodes_)
		{
			free(data.kd_nodes_);
			data.kd_nodes_ = nullptr;
		}
		
		data.kd_root_ = nullptr;
		
		data.evaluation_interaction_callbacks_.clear();
	}
}

void InteractionType::CalculateAllInteractions(Subpopulation *p_subpop)
{
	slim_objectid_t subpop_id = p_subpop->subpopulation_id_;
	slim_popsize_t subpop_size = p_subpop->parent_subpop_size_;
	Individual *subpop_individuals = p_subpop->parent_individuals_.data();
	InteractionsData &subpop_data = data_[subpop_id];
	std::vector<SLiMEidosBlock*> &callbacks = subpop_data.evaluation_interaction_callbacks_;
	bool no_callbacks = (callbacks.size() == 0);
	
	// When this method is called, distances_ and strengths_ may be nullptr, or they may be allocated.  If they are allocated,
	// they contain garbage, so we have to be a bit careful.  If we have any sort of sex-segregation going on, we want to
	// force allocation and initialization of strengths_, since that will give us sex-segregation info set up to make the
	// interaction calculations more efficient.  If sex-segregation is not enabled, we want to force allocation but not
	// initialization of strengths_, since we will overwrite every entry anyway.  We always want to force allocation but not
	// initialization of distances_, because we will overwrite all values in it whether we are sex-segregated or not.
	bool is_sex_segregated = ((receiver_sex_ != IndividualSex::kUnspecified) || (exerter_sex_ != IndividualSex::kUnspecified));
	
	if (is_sex_segregated)
	{
		// Sex-segregation of some type; force allocation of both, and initialization of strengths_
		if (!subpop_data.distances_)
			subpop_data.distances_ = (double *)malloc(subpop_size * subpop_size * sizeof(double));
		if (!subpop_data.strengths_)
			subpop_data.strengths_ = (double *)malloc(subpop_size * subpop_size * sizeof(double));
		InitializeStrengths(subpop_data);
	}
	else
	{
		// No sex-segregation; force allocation but not initialization
		if (!subpop_data.distances_)
			subpop_data.distances_ = (double *)malloc(subpop_size * subpop_size * sizeof(double));
		if (!subpop_data.strengths_)
			subpop_data.strengths_ = (double *)malloc(subpop_size * subpop_size * sizeof(double));
	}
	
	if (spatiality_ == 0)
	{
		// Non-spatial interactions do not involve distances
		if (!reciprocal_)
		{
			// No reciprocality, so we don't need to mirror; just go through strengths_ sequentially
			int receiving_index = 0;
			double *receiving_strength = subpop_data.strengths_;
			Individual *receiving_individual = subpop_individuals;
			
			while (receiving_index < subpop_size)
			{
				int exerting_index = 0;
				double *exerting_strength = receiving_strength;		// follow the row of receiver data
				Individual *exerting_individual = subpop_individuals;
				
				// The inner loop depends on sex-segregation.  If it is enabled, we need to check the existing strength value
				// and fill only when necessary; if not, we fill in all values with no check because we're overwriting garbage.
				if (is_sex_segregated)
				{
					while (exerting_index < subpop_size)
					{
						if (isnan(*exerting_strength))
						{
							// Calculate the interaction strength; no need to check for self-interaction here
							if (no_callbacks)
								*exerting_strength = CalculateStrengthNoCallbacks(NAN);
							else
								*exerting_strength = CalculateStrengthWithCallbacks(NAN, receiving_individual, exerting_individual, p_subpop, callbacks);
						}
						
						exerting_index++, exerting_strength++, exerting_individual++;
					}
				}
				else
				{
					while (exerting_index < subpop_size)
					{
						if (receiving_index == exerting_index)
						{
							// Individuals exert no interaction strength on themselves
							*exerting_strength = 0;
						}
						else
						{
							// Calculate the interaction strength
							if (no_callbacks)
								*exerting_strength = CalculateStrengthNoCallbacks(NAN);
							else
								*exerting_strength = CalculateStrengthWithCallbacks(NAN, receiving_individual, exerting_individual, p_subpop, callbacks);
						}
						
						exerting_index++, exerting_strength++, exerting_individual++;
					}
				}
				
				receiving_index++, receiving_strength += subpop_size, receiving_individual++;
			}
		}
		else
		{
			// Reciprocality, so we will do the top-right half and mirror it down into the bottom-left half
			// This is essentially parallel to the code above, but with twice the pointers and half the work
			int receiving_index = 0;
			double *receiving_strength = subpop_data.strengths_;
			double *mirror_receiving_strength = subpop_data.strengths_;
			Individual *receiving_individual = subpop_individuals;
			
			while (receiving_index < subpop_size)
			{
				int exerting_index = receiving_index;
				double *exerting_strength = receiving_strength + exerting_index;
				double *mirror_exerting_strength = mirror_receiving_strength + exerting_index * subpop_size;
				Individual *exerting_individual = subpop_individuals + exerting_index;
				
				if (is_sex_segregated)
				{
					while (exerting_index < subpop_size)
					{
						if (isnan(*exerting_strength))
						{
							double strength;
							
							if (no_callbacks)
								strength = CalculateStrengthNoCallbacks(NAN);
							else
								strength = CalculateStrengthWithCallbacks(NAN, receiving_individual, exerting_individual, p_subpop, callbacks);
							
							*exerting_strength = strength;
							*mirror_exerting_strength = strength;
						}
						
						exerting_index++, exerting_strength++, exerting_individual++, mirror_exerting_strength += subpop_size;
					}
				}
				else
				{
					while (exerting_index < subpop_size)
					{
						double strength;
						
						if (receiving_index == exerting_index)
						{
							strength = 0;
						}
						else
						{
							if (no_callbacks)
								strength = CalculateStrengthNoCallbacks(NAN);
							else
								strength = CalculateStrengthWithCallbacks(NAN, receiving_individual, exerting_individual, p_subpop, callbacks);
						}
						
						*exerting_strength = strength;
						*mirror_exerting_strength = strength;
						
						exerting_index++, exerting_strength++, exerting_individual++, mirror_exerting_strength += subpop_size;
					}
				}
				
				receiving_index++, receiving_strength += subpop_size, receiving_individual++, mirror_receiving_strength++;
			}
		}
	}
	else
	{
		// Spatial interactions involve distances, so we need to calculate those, too
		if (!reciprocal_)
		{
			// No reciprocality, so we don't need to mirror; just go through strengths_ sequentially
			int receiving_index = 0;
			double *receiving_position = subpop_data.positions_;
			double *receiving_distance = subpop_data.distances_;
			double *receiving_strength = subpop_data.strengths_;
			Individual *receiving_individual = subpop_individuals;
			
			while (receiving_index < subpop_size)
			{
				int exerting_index = 0;
				double *exerting_position = subpop_data.positions_;
				double *exerting_distance = receiving_distance;		// follow the row of receiver data
				double *exerting_strength = receiving_strength;		// follow the row of receiver data
				Individual *exerting_individual = subpop_individuals;
				
				// The inner loop depends on sex-segregation.  If it is enabled, we need to check the existing strength value
				// and fill only when necessary; if not, we fill in all values with no check because we're overwriting garbage.
				if (is_sex_segregated)
				{
					while (exerting_index < subpop_size)
					{
						// First, we always calculate the distance; we've already fetched the cache lines containing the
						// positions, and we would need to at least write a NAN for the distance, so we should just do it.
						// We skip the check for self-interaction here; the distance will come out 0, and we avoid a test
						// and branch.  For large population sizes, that will matter more than doing the work one extra time.
						double distance;
						
						if (spatiality_ == 1)
						{
							distance = fabs(exerting_position[0] - receiving_position[0]);
						}
						else if (spatiality_ == 2)
						{
							double distance_x = (exerting_position[0] - receiving_position[0]);
							double distance_y = (exerting_position[1] - receiving_position[1]);
							
							distance = sqrt(distance_x * distance_x + distance_y * distance_y);
						}
						else if (spatiality_ == 3)
						{
							double distance_x = (exerting_position[0] - receiving_position[0]);
							double distance_y = (exerting_position[1] - receiving_position[1]);
							double distance_z = (exerting_position[2] - receiving_position[2]);
							
							distance = sqrt(distance_x * distance_x + distance_y * distance_y + distance_z * distance_z);
						}
						
						*exerting_distance = distance;
						
						// Then we calculate the strength, only if it is NAN; no need to check for self-interaction here
						if (isnan(*exerting_strength))
						{
							if (distance <= max_distance_)
							{
								if (no_callbacks)
									*exerting_strength = CalculateStrengthNoCallbacks(distance);
								else
									*exerting_strength = CalculateStrengthWithCallbacks(distance, receiving_individual, exerting_individual, p_subpop, callbacks);
							}
							else
								*exerting_strength = 0.0;
						}
						
						exerting_index++, exerting_position += SLIM_MAX_DIMENSIONALITY, exerting_distance++, exerting_strength++, exerting_individual++;
					}
				}
				else
				{
					while (exerting_index < subpop_size)
					{
						if (receiving_index == exerting_index)
						{
							// Individuals are at zero distance from themselves, but exert no interaction strength
							*exerting_distance = 0;
							*exerting_strength = 0;
						}
						else
						{
							// Calculate the distance and the interaction strength
							double distance;
							
							if (spatiality_ == 1)
							{
								distance = fabs(exerting_position[0] - receiving_position[0]);
							}
							else if (spatiality_ == 2)
							{
								double distance_x = (exerting_position[0] - receiving_position[0]);
								double distance_y = (exerting_position[1] - receiving_position[1]);
								
								distance = sqrt(distance_x * distance_x + distance_y * distance_y);
							}
							else if (spatiality_ == 3)
							{
								double distance_x = (exerting_position[0] - receiving_position[0]);
								double distance_y = (exerting_position[1] - receiving_position[1]);
								double distance_z = (exerting_position[2] - receiving_position[2]);
								
								distance = sqrt(distance_x * distance_x + distance_y * distance_y + distance_z * distance_z);
							}
							
							*exerting_distance = distance;
							
							if (distance <= max_distance_)
							{
								if (no_callbacks)
									*exerting_strength = CalculateStrengthNoCallbacks(distance);
								else
									*exerting_strength = CalculateStrengthWithCallbacks(distance, receiving_individual, exerting_individual, p_subpop, callbacks);
							}
							else
								*exerting_strength = 0.0;
						}
						
						exerting_index++, exerting_position += SLIM_MAX_DIMENSIONALITY, exerting_distance++, exerting_strength++, exerting_individual++;
					}
				}
				
				receiving_index++, receiving_position += SLIM_MAX_DIMENSIONALITY, receiving_distance += subpop_size, receiving_strength += subpop_size, receiving_individual++;
			}
		}
		else
		{
			// Reciprocality, so we will do the top-right half and mirror it down into the bottom-left half
			// This is essentially parallel to the code above, but with twice the pointers and half the work
			int receiving_index = 0;
			double *receiving_position = subpop_data.positions_;
			double *receiving_distance = subpop_data.distances_;
			double *mirror_receiving_distance = subpop_data.distances_;
			double *receiving_strength = subpop_data.strengths_;
			double *mirror_receiving_strength = subpop_data.strengths_;
			Individual *receiving_individual = subpop_individuals;
			
			while (receiving_index < subpop_size)
			{
				int exerting_index = receiving_index;
				double *exerting_position = subpop_data.positions_ + exerting_index * SLIM_MAX_DIMENSIONALITY;
				double *exerting_distance = receiving_distance + exerting_index;
				double *mirror_exerting_distance = mirror_receiving_distance + exerting_index * subpop_size;
				double *exerting_strength = receiving_strength + exerting_index;
				double *mirror_exerting_strength = mirror_receiving_strength + exerting_index * subpop_size;
				Individual *exerting_individual = subpop_individuals;
				
				if (is_sex_segregated)
				{
					while (exerting_index < subpop_size)
					{
						double distance;
						
						if (spatiality_ == 1)
						{
							distance = fabs(exerting_position[0] - receiving_position[0]);
						}
						else if (spatiality_ == 2)
						{
							double distance_x = (exerting_position[0] - receiving_position[0]);
							double distance_y = (exerting_position[1] - receiving_position[1]);
							
							distance = sqrt(distance_x * distance_x + distance_y * distance_y);
						}
						else if (spatiality_ == 3)
						{
							double distance_x = (exerting_position[0] - receiving_position[0]);
							double distance_y = (exerting_position[1] - receiving_position[1]);
							double distance_z = (exerting_position[2] - receiving_position[2]);
							
							distance = sqrt(distance_x * distance_x + distance_y * distance_y + distance_z * distance_z);
						}
						
						*exerting_distance = distance;
						*mirror_exerting_distance = distance;
						
						if (isnan(*exerting_strength))
						{
							double strength;
							
							if (distance <= max_distance_)
							{
								if (no_callbacks)
									strength = CalculateStrengthNoCallbacks(distance);
								else
									strength = CalculateStrengthWithCallbacks(distance, receiving_individual, exerting_individual, p_subpop, callbacks);
							}
							else
								strength = 0.0;
							
							*exerting_strength = strength;
							*mirror_exerting_strength = strength;
						}
						
						exerting_index++, exerting_position += SLIM_MAX_DIMENSIONALITY, exerting_distance++, exerting_strength++, exerting_individual++;
						mirror_exerting_distance += subpop_size, mirror_exerting_strength += subpop_size;
					}
				}
				else
				{
					while (exerting_index < subpop_size)
					{
						if (receiving_index == exerting_index)
						{
							*exerting_distance = 0;
							*exerting_strength = 0;
							// no need to set *mirror_exerting_distance or *mirror_exerting_strength, they point to the same memory
						}
						else
						{
							double distance;
							
							if (spatiality_ == 1)
							{
								distance = fabs(exerting_position[0] - receiving_position[0]);
							}
							else if (spatiality_ == 2)
							{
								double distance_x = (exerting_position[0] - receiving_position[0]);
								double distance_y = (exerting_position[1] - receiving_position[1]);
								
								distance = sqrt(distance_x * distance_x + distance_y * distance_y);
							}
							else if (spatiality_ == 3)
							{
								double distance_x = (exerting_position[0] - receiving_position[0]);
								double distance_y = (exerting_position[1] - receiving_position[1]);
								double distance_z = (exerting_position[2] - receiving_position[2]);
								
								distance = sqrt(distance_x * distance_x + distance_y * distance_y + distance_z * distance_z);
							}
							
							*exerting_distance = distance;
							*mirror_exerting_distance = distance;
							
							double strength;
							
							if (distance <= max_distance_)
							{
								if (no_callbacks)
									strength = CalculateStrengthNoCallbacks(distance);
								else
									strength = CalculateStrengthWithCallbacks(distance, receiving_individual, exerting_individual, p_subpop, callbacks);
							}
							else
								strength = 0.0;
							
							*exerting_strength = strength;
							*mirror_exerting_strength = strength;
						}
						
						exerting_index++, exerting_position += SLIM_MAX_DIMENSIONALITY, exerting_distance++, exerting_strength++, exerting_individual++;
						mirror_exerting_distance += subpop_size, mirror_exerting_strength += subpop_size;
					}
				}
				
				receiving_index++, receiving_position += SLIM_MAX_DIMENSIONALITY, receiving_distance += subpop_size, receiving_strength += subpop_size, receiving_individual++;
				mirror_receiving_distance++, mirror_receiving_strength++;
			}
		}
	}
}

double InteractionType::CalculateDistance(double *position1, double *position2)
{
	if (spatiality_ == 1)
	{
		return fabs(position1[0] - position2[0]);
	}
	else if (spatiality_ == 2)
	{
		double distance_x = (position1[0] - position2[0]);
		double distance_y = (position1[1] - position2[1]);
		
		return sqrt(distance_x * distance_x + distance_y * distance_y);
	}
	else if (spatiality_ == 3)
	{
		double distance_x = (position1[0] - position2[0]);
		double distance_y = (position1[1] - position2[1]);
		double distance_z = (position1[2] - position2[2]);
		
		return sqrt(distance_x * distance_x + distance_y * distance_y + distance_z * distance_z);
	}
	else
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): calculation of distances requires that the interaction be spatial." << eidos_terminate();
}

double InteractionType::CalculateStrengthNoCallbacks(double p_distance)
{
	// CAUTION: This method should only be called when p_distance <= max_distance_ (or is NAN).
	// It is the caller's responsibility to do that filtering, for performance reasons!
	// NOTE: The caller does *not* need to guarantee that this is not a self-interaction.
	// That is taken care of automatically by the logic in EnsureStrengthsPresent(), which
	// zeroes out all self-interactions at the outset.  This will never be called in that case.
	switch (if_type_)
	{
		case IFType::kFixed:
			return (if_param1_);																		// fmax
		case IFType::kLinear:
			return (if_param1_ * (1.0 - p_distance / max_distance_));									// fmax * (1 − d/dmax)
		case IFType::kExponential:
			return (if_param1_ * exp(-if_param2_ * p_distance));										// fmax * exp(−λd)
		case IFType::kNormal:
			return (if_param1_ * exp(-(p_distance * p_distance) / (2.0 * if_param2_ * if_param2_)));	// fmax * exp(−d^2/2σ^2)
	}
}

double InteractionType::CalculateStrengthWithCallbacks(double p_distance, Individual *receiver, Individual *exerter, Subpopulation *p_subpop, std::vector<SLiMEidosBlock*> &p_interaction_callbacks)
{
	// CAUTION: This method should only be called when p_distance <= max_distance_ (or is NAN).
	// It is the caller's responsibility to do that filtering, for performance reasons!
	// NOTE: The caller does *not* need to guarantee that this is not a self-interaction.
	// That is taken care of automatically by the logic in EnsureStrengthsPresent(), which
	// zeroes out all self-interactions at the outset.  This will never be called in that case.
	double strength = CalculateStrengthNoCallbacks(p_distance);
	
	strength = ApplyInteractionCallbacks(receiver, exerter, p_subpop, strength, p_distance, p_interaction_callbacks);
	
	return strength;
}

double InteractionType::ApplyInteractionCallbacks(Individual *p_receiver, Individual *p_exerter, Subpopulation *p_subpop, double p_strength, double p_distance, std::vector<SLiMEidosBlock*> &p_interaction_callbacks)
{
	SLiMSim &sim = p_subpop->population_.sim_;
	
	for (SLiMEidosBlock *interaction_callback : p_interaction_callbacks)
	{
		if (interaction_callback->active_)
		{
			// The callback is active and matches our interaction type id, so we need to execute it
			const EidosASTNode *compound_statement_node = interaction_callback->compound_statement_node_;
			
			if (compound_statement_node->cached_value_)
			{
				// The script is a constant expression such as "{ return 1.1; }", so we can short-circuit it completely
				EidosValue_SP result_SP = compound_statement_node->cached_value_;
				EidosValue *result = result_SP.get();
				
				if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
					EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyInteractionCallbacks): interaction() callbacks must provide a float singleton return value." << eidos_terminate(interaction_callback->identifier_token_);
				
				p_strength = result->FloatAtIndex(0, nullptr);
				
				// the cached value is owned by the tree, so we do not dispose of it
				// there is also no script output to handle
			}
			else
			{
				// local variables for the callback parameters that we might need to allocate here, and thus need to free below
				EidosValue_Float_singleton local_distance(p_distance);
				EidosValue_Float_singleton local_strength(p_strength);
				
				// We need to actually execute the script; we start a block here to manage the lifetime of the symbol table
				{
					EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &p_subpop->population_.sim_.SymbolTable());
					EidosSymbolTable client_symbols(EidosSymbolTableType::kVariablesTable, &callback_symbols);
					EidosFunctionMap *function_map = EidosInterpreter::BuiltInFunctionMap();
					EidosInterpreter interpreter(interaction_callback->compound_statement_node_, client_symbols, *function_map, &sim);
					
					if (interaction_callback->contains_self_)
						callback_symbols.InitializeConstantSymbolEntry(interaction_callback->SelfSymbolTableEntry());		// define "self"
					
					// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
					// We can use that method because we know the lifetime of the symbol table is shorter than that of
					// the value objects, and we know that the values we are setting here will not change (the objects
					// referred to by the values may change, but the values themselves will not change).
					if (interaction_callback->contains_distance_)
					{
						local_distance.stack_allocated();		// prevent Eidos_intrusive_ptr from trying to delete this
						callback_symbols.InitializeConstantSymbolEntry(gID_distance, EidosValue_SP(&local_distance));
					}
					if (interaction_callback->contains_strength_)
					{
						local_strength.stack_allocated();		// prevent Eidos_intrusive_ptr from trying to delete this
						callback_symbols.InitializeConstantSymbolEntry(gID_strength, EidosValue_SP(&local_strength));
					}
					if (interaction_callback->contains_receiver_)
						callback_symbols.InitializeConstantSymbolEntry(gID_receiver, p_receiver->CachedEidosValue());
					if (interaction_callback->contains_exerter_)
						callback_symbols.InitializeConstantSymbolEntry(gID_exerter, p_exerter->CachedEidosValue());
					if (interaction_callback->contains_subpop_)
						callback_symbols.InitializeConstantSymbolEntry(gID_subpop, p_subpop->SymbolTableEntry().second);
					
					try
					{
						// Interpret the script; the result from the interpretation must be a singleton double used as a new fitness value
						EidosValue_SP result_SP = interpreter.EvaluateInternalBlock(interaction_callback->script_);
						EidosValue *result = result_SP.get();
						
						if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
							EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyInteractionCallbacks): interaction() callbacks must provide a float singleton return value." << eidos_terminate(interaction_callback->identifier_token_);
						
						p_strength = result->FloatAtIndex(0, nullptr);
						
						if (isnan(p_strength) || isinf(p_strength) || (p_strength < 0.0))
							EIDOS_TERMINATION << "ERROR (Subpopulation::ApplyInteractionCallbacks): interaction() callbacks must return a finite value >= 0.0." << eidos_terminate(interaction_callback->identifier_token_);
						
						// Output generated by the interpreter goes to our output stream
						SLIM_OUTSTREAM << interpreter.ExecutionOutput();
					}
					catch (...)
					{
						// Emit final output even on a throw, so that stop() messages and such get printed
						SLIM_OUTSTREAM << interpreter.ExecutionOutput();
						
						throw;
					}
				}
			}
		}
	}
	
	return p_strength;
}

void InteractionType::EnsureDistancesPresent(InteractionsData &p_subpop_data)
{
	if (!p_subpop_data.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::EnsureDistancesPresent): (internal error) the interaction has not been evaluated." << eidos_terminate();
	
	if (!p_subpop_data.distances_ && spatiality_)
	{
		int subpop_size = p_subpop_data.individual_count_;
		int matrix_size = subpop_size * subpop_size;
		
		p_subpop_data.distances_ = (double *)malloc(matrix_size * sizeof(double));
		
		InitializeDistances(p_subpop_data);
	}
}

void InteractionType::InitializeDistances(InteractionsData &p_subpop_data)
{
	double *values = p_subpop_data.distances_;
	slim_popsize_t subpop_size = p_subpop_data.individual_count_;
	int matrix_size = subpop_size * subpop_size;
	
	// Fill with NAN initially, to mark that the distance values have not been calculated.
	// The compiler is smart enough to replace this with _platform_memset_pattern16...(),
	// so it ends up being as optimized as it can get, probably, on OS X at least.
	double *value_ptr = values;
	double *values_end = values + matrix_size;
	
	while (value_ptr < values_end)
		*(value_ptr++) = NAN;
	
	// Set distances between an individual and itself to zero.  By doing this here, we
	// save a little work elsewhere, but unlike the case of EnsureStrengthsPresent(), this
	// is non-essential; it's just an optimization.
	for (int ind_index = 0; ind_index < subpop_size; ++ind_index)
		values[ind_index * (subpop_size + 1)] = 0.0;
}

void InteractionType::EnsureStrengthsPresent(InteractionsData &p_subpop_data)
{
	if (!p_subpop_data.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::EnsureDistancesPresent): (internal error) the interaction has not been evaluated." << eidos_terminate();
	
	if (!p_subpop_data.distances_ && spatiality_)
		EnsureDistancesPresent(p_subpop_data);
	
	if (!p_subpop_data.strengths_)
	{
		int subpop_size = p_subpop_data.individual_count_;
		int matrix_size = subpop_size * subpop_size;
		
		p_subpop_data.strengths_ = (double *)malloc(matrix_size * sizeof(double));
		
		InitializeStrengths(p_subpop_data);
	}
}

void InteractionType::InitializeStrengths(InteractionsData &p_subpop_data)
{
	double *values = p_subpop_data.strengths_;
	slim_popsize_t subpop_size = p_subpop_data.individual_count_;
	int matrix_size = subpop_size * subpop_size;
	bool is_sex_segregated = ((receiver_sex_ != IndividualSex::kUnspecified) || (exerter_sex_ != IndividualSex::kUnspecified));
	
	if (is_sex_segregated)
	{
		// If we have some sort of sex-segregation going on, we take the slow path here.  We
		// set interactions that are enabled by the sex-segregation to NAN, to indicate that
		// they need to be calculated; others are set to 0.0 to short-circuit calculation.
		// This is not just an optimization; it is how sex-segregation works at all.
		slim_popsize_t first_male_index = p_subpop_data.first_male_index_;
		int receiving_index = 0;
		double *receiving_strength = p_subpop_data.strengths_;
		
		while (receiving_index < subpop_size)
		{
			int exerting_index = 0;
			double *exerting_strength = receiving_strength;		// follow the row of receiver data
			
			if (((receiver_sex_ == IndividualSex::kMale) && (receiving_index < first_male_index)) || ((receiver_sex_ == IndividualSex::kFemale) && (receiving_index >= first_male_index)))
			{
				// The receiver is the wrong sex; the whole row can be filled with 0.0 (no interaction)
				while (exerting_index < subpop_size)
				{
					*exerting_strength = 0.0;
					exerting_index++, exerting_strength++;
				}
			}
			else
			{
				// The receiver is of the right sex; only the exerter now needs to be checked
				if (exerter_sex_ == IndividualSex::kMale)
				{
					// 0.0 (no interaction) for female exerters, NAN (uncalculated) for male exerters
					while (exerting_index < first_male_index)
					{
						*exerting_strength = 0.0;
						exerting_index++, exerting_strength++;
					}
					while (exerting_index < subpop_size)
					{
						*exerting_strength = NAN;
						exerting_index++, exerting_strength++;
					}
				}
				else if (exerter_sex_ == IndividualSex::kFemale)
				{
					// NAN (uncalculated) for female exerters, 0.0 (no interaction) for male exerters
					while (exerting_index < first_male_index)
					{
						*exerting_strength = NAN;
						exerting_index++, exerting_strength++;
					}
					while (exerting_index < subpop_size)
					{
						*exerting_strength = 0.0;
						exerting_index++, exerting_strength++;
					}
				}
				else if (exerter_sex_ == IndividualSex::kUnspecified)
				{
					// NAN (uncalculated) for all exerters
					while (exerting_index < subpop_size)
					{
						*exerting_strength = NAN;
						exerting_index++, exerting_strength++;
					}
				}
			}
			
			receiving_index++, receiving_strength += subpop_size;
		}
	}
	else
	{
		// Fill with NAN initially, to mark that the distance values have not been calculated.
		// The compiler is smart enough to replace this with _platform_memset_pattern16...(),
		// so it ends up being as optimized as it can get, probably, on OS X at least.
		double *value_ptr = values;
		double *values_end = values + matrix_size;
		
		while (value_ptr < values_end)
			*(value_ptr++) = NAN;
	}
	
	// Set interactions between an individual and itself to zero.  By doing this here, we
	// eliminate the need to check for this case elsewhere; even when a strength has not
	// been cached in general, it can be assumed that self-interactions are cached.
	for (int ind_index = 0; ind_index < subpop_size; ++ind_index)
		values[ind_index * (subpop_size + 1)] = 0.0;
}

#pragma mark -
#pragma mark k-d tree construction

// This k-d tree code is patterned after the C code at RosettaCode.org : https://rosettacode.org/wiki/K-d_tree#C
// It uses a Quickselect-style algorithm to select medians to produce a balanced tree
// Each spatiality case is coded separately, for maximum speed, but they are very parallel

// Some of the code below is separated by phase.  The k-d tree cycles through phase (x, y, z) as you descend,
// and rather than passing phase as a parameter, the code has been factored into phase-specific functions
// that are mutually recursive, for speed.  It's not a huge win, but it does help a little.

inline void swap(SLiM_kdNode *x, SLiM_kdNode *y)
{
	std::swap(x->x, y->x);
	std::swap(x->individual_index_, y->individual_index_);
}

// find median for phase 0 (x)
SLiM_kdNode *InteractionType::FindMedian_p0(SLiM_kdNode *start, SLiM_kdNode *end)
{
	SLiM_kdNode *p, *store, *md = start + (end - start) / 2;
	double pivot;
	
	while (1)
	{
		pivot = md->x[0];
		
		swap(md, end - 1);
		for (store = p = start; p < end; p++)
		{
			if (p->x[0] < pivot)
			{
				if (p != store)
					swap(p, store);
				store++;
			}
		}
		swap(store, end - 1);
		
		// median has duplicate values
		if (store->x[0] == md->x[0])
			return md;
		
		if (store > md)	end = store;
		else			start = store;
	}
}

// find median for phase 1 (y)
SLiM_kdNode *InteractionType::FindMedian_p1(SLiM_kdNode *start, SLiM_kdNode *end)
{
	SLiM_kdNode *p, *store, *md = start + (end - start) / 2;
	double pivot;
	
	while (1)
	{
		pivot = md->x[1];
		
		swap(md, end - 1);
		for (store = p = start; p < end; p++)
		{
			if (p->x[1] < pivot)
			{
				if (p != store)
					swap(p, store);
				store++;
			}
		}
		swap(store, end - 1);
		
		// median has duplicate values
		if (store->x[1] == md->x[1])
			return md;
		
		if (store > md)	end = store;
		else			start = store;
	}
}

// find median for phase 2 (z)
SLiM_kdNode *InteractionType::FindMedian_p2(SLiM_kdNode *start, SLiM_kdNode *end)
{
	SLiM_kdNode *p, *store, *md = start + (end - start) / 2;
	double pivot;
	
	while (1)
	{
		pivot = md->x[2];
		
		swap(md, end - 1);
		for (store = p = start; p < end; p++)
		{
			if (p->x[2] < pivot)
			{
				if (p != store)
					swap(p, store);
				store++;
			}
		}
		swap(store, end - 1);
		
		// median has duplicate values
		if (store->x[2] == md->x[2])
			return md;
		
		if (store > md)	end = store;
		else			start = store;
	}
}

// make k-d tree recursively for the 1D case for phase 0 (x)
SLiM_kdNode *InteractionType::MakeKDTree1_p0(SLiM_kdNode *t, int len)
{
	SLiM_kdNode *n = ((len == 1) ? t : FindMedian_p0(t, t + len));
	
	if (n)
	{
		int left_len = (int)(n - t);
		n->left  = (left_len ? MakeKDTree1_p0(t, left_len) : 0);
		
		int right_len = (int)(t + len - (n + 1));
		n->right = (right_len ? MakeKDTree1_p0(n + 1, right_len) : 0);
	}
	return n;
}

// make k-d tree recursively for the 2D case for phase 0 (x)
SLiM_kdNode *InteractionType::MakeKDTree2_p0(SLiM_kdNode *t, int len)
{
	SLiM_kdNode *n = ((len == 1) ? t : FindMedian_p0(t, t + len));
	
	if (n)
	{
		int left_len = (int)(n - t);
		n->left  = (left_len ? MakeKDTree2_p1(t, left_len) : 0);
		
		int right_len = (int)(t + len - (n + 1));
		n->right = (right_len ? MakeKDTree2_p1(n + 1, right_len) : 0);
	}
	return n;
}

// make k-d tree recursively for the 2D case for phase 1 (y)
SLiM_kdNode *InteractionType::MakeKDTree2_p1(SLiM_kdNode *t, int len)
{
	SLiM_kdNode *n = ((len == 1) ? t : FindMedian_p1(t, t + len));
	
	if (n)
	{
		int left_len = (int)(n - t);
		n->left  = (left_len ? MakeKDTree2_p0(t, left_len) : 0);
		
		int right_len = (int)(t + len - (n + 1));
		n->right = (right_len ? MakeKDTree2_p0(n + 1, right_len) : 0);
	}
	return n;
}

// make k-d tree recursively for the 3D case for phase 0 (x)
SLiM_kdNode *InteractionType::MakeKDTree3_p0(SLiM_kdNode *t, int len)
{
	SLiM_kdNode *n = ((len == 1) ? t : FindMedian_p0(t, t + len));
	
	if (n)
	{
		int left_len = (int)(n - t);
		n->left  = (left_len ? MakeKDTree3_p1(t, left_len) : 0);
		
		int right_len = (int)(t + len - (n + 1));
		n->right = (right_len ? MakeKDTree3_p1(n + 1, right_len) : 0);
	}
	return n;
}

// make k-d tree recursively for the 3D case for phase 1 (y)
SLiM_kdNode *InteractionType::MakeKDTree3_p1(SLiM_kdNode *t, int len)
{
	SLiM_kdNode *n = ((len == 1) ? t : FindMedian_p1(t, t + len));
	
	if (n)
	{
		int left_len = (int)(n - t);
		n->left  = (left_len ? MakeKDTree3_p2(t, left_len) : 0);
		
		int right_len = (int)(t + len - (n + 1));
		n->right = (right_len ? MakeKDTree3_p2(n + 1, right_len) : 0);
	}
	return n;
}

// make k-d tree recursively for the 3D case for phase 2 (z)
SLiM_kdNode *InteractionType::MakeKDTree3_p2(SLiM_kdNode *t, int len)
{
	SLiM_kdNode *n = ((len == 1) ? t : FindMedian_p2(t, t + len));
	
	if (n)
	{
		int left_len = (int)(n - t);
		n->left  = (left_len ? MakeKDTree3_p0(t, left_len) : 0);
		
		int right_len = (int)(t + len - (n + 1));
		n->right = (right_len ? MakeKDTree3_p0(n + 1, right_len) : 0);
	}
	return n;
}

void InteractionType::EnsureKDTreePresent(InteractionsData &p_subpop_data)
{
	if (!p_subpop_data.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::EnsureDistancesPresent): (internal error) the interaction has not been evaluated." << eidos_terminate();
	
	if (spatiality_ == 0)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::EnsureKDTreePresent): (internal error) k-d tree cannot be constructed for non-spatial interactions." << eidos_terminate();
	}
	else if (!p_subpop_data.kd_nodes_)
	{
		int count = p_subpop_data.individual_count_;
		SLiM_kdNode *nodes = (SLiM_kdNode *)calloc(count, sizeof(SLiM_kdNode));
		
		// Fill the nodes with their initial data
		switch (spatiality_)
		{
			case 1:
				for (int i = 0; i < count; ++i)
				{
					SLiM_kdNode *node = nodes + i;
					double *position_data = p_subpop_data.positions_ + i * SLIM_MAX_DIMENSIONALITY;
					
					node->x[0] = position_data[0];
					node->individual_index_ = i;
				}
				break;
			case 2:
				for (int i = 0; i < count; ++i)
				{
					SLiM_kdNode *node = nodes + i;
					double *position_data = p_subpop_data.positions_ + i * SLIM_MAX_DIMENSIONALITY;
					
					node->x[0] = position_data[0];
					node->x[1] = position_data[1];
					node->individual_index_ = i;
				}
				break;
			case 3:
				for (int i = 0; i < count; ++i)
				{
					SLiM_kdNode *node = nodes + i;
					double *position_data = p_subpop_data.positions_ + i * SLIM_MAX_DIMENSIONALITY;
					
					node->x[0] = position_data[0];
					node->x[1] = position_data[1];
					node->x[2] = position_data[2];
					node->individual_index_ = i;
				}
				break;
		}
		
		p_subpop_data.kd_nodes_ = nodes;
		
		if (p_subpop_data.individual_count_ == 0)
		{
			p_subpop_data.kd_root_ = 0;
		}
		else
		{
			// Now call out to recursively construct the tree
			switch (spatiality_)
			{
				case 1: p_subpop_data.kd_root_ = MakeKDTree1_p0(p_subpop_data.kd_nodes_, p_subpop_data.individual_count_);	break;
				case 2: p_subpop_data.kd_root_ = MakeKDTree2_p0(p_subpop_data.kd_nodes_, p_subpop_data.individual_count_);	break;
				case 3: p_subpop_data.kd_root_ = MakeKDTree3_p0(p_subpop_data.kd_nodes_, p_subpop_data.individual_count_);	break;
			}
		}
	}
}

#pragma mark -
#pragma mark k-d tree neighbor searches

inline double dist_sq1(SLiM_kdNode *a, double *b)
{
	double t = a->x[0] - b[0];
	
	return t * t;
}

inline double dist_sq2(SLiM_kdNode *a, double *b)
{
	double t, d;
	
	t = a->x[0] - b[0];
	d = t * t;
	
	t = a->x[1] - b[1];
	d += t * t;
	
	return d;
}

inline double dist_sq3(SLiM_kdNode *a, double *b)
{
	double t, d;
	
	t = a->x[0] - b[0];
	d = t * t;
	
	t = a->x[1] - b[1];
	d += t * t;
	
	t = a->x[2] - b[2];
	d += t * t;
	
	return d;
}

// find the one best neighbor in 1D
void InteractionType::FindNeighbors1_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SLiM_kdNode **best, double *best_dist)
{
	double d = dist_sq1(root, nd);
	double dx = root->x[0] - nd[0];
	double dx2 = dx * dx;
	
	if ((!*best || d < *best_dist) && (root->individual_index_ != p_focal_individual_index)) {
		*best_dist = d;
		*best = root;
	}
	
	if (dx > 0)
	{
		if (root->left)
			FindNeighbors1_1(root->left, nd, p_focal_individual_index, best, best_dist);
		
		if (dx2 >= *best_dist) return;
		
		if (root->right)
			FindNeighbors1_1(root->right, nd, p_focal_individual_index, best, best_dist);
	}
	else
	{
		if (root->right)
			FindNeighbors1_1(root->right, nd, p_focal_individual_index, best, best_dist);
		
		if (dx2 >= *best_dist) return;
		
		if (root->left)
			FindNeighbors1_1(root->left, nd, p_focal_individual_index, best, best_dist);
	}
}

// find the one best neighbor in 2D
void InteractionType::FindNeighbors1_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SLiM_kdNode **best, double *best_dist, int p_phase)
{
	double d = dist_sq2(root, nd);
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	
	if ((!*best || d < *best_dist) && (root->individual_index_ != p_focal_individual_index)) {
		*best_dist = d;
		*best = root;
	}
	
	if (++p_phase >= 2) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			FindNeighbors1_2(root->left, nd, p_focal_individual_index, best, best_dist, p_phase);
		
		if (dx2 >= *best_dist) return;
		
		if (root->right)
			FindNeighbors1_2(root->right, nd, p_focal_individual_index, best, best_dist, p_phase);
	}
	else
	{
		if (root->right)
			FindNeighbors1_2(root->right, nd, p_focal_individual_index, best, best_dist, p_phase);
		
		if (dx2 >= *best_dist) return;
		
		if (root->left)
			FindNeighbors1_2(root->left, nd, p_focal_individual_index, best, best_dist, p_phase);
	}
}

// find the one best neighbor in 3D
void InteractionType::FindNeighbors1_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SLiM_kdNode **best, double *best_dist, int p_phase)
{
	double d = dist_sq3(root, nd);
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	
	if ((!*best || d < *best_dist) && (root->individual_index_ != p_focal_individual_index)) {
		*best_dist = d;
		*best = root;
	}
	
	if (++p_phase >= 3) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			FindNeighbors1_3(root->left, nd, p_focal_individual_index, best, best_dist, p_phase);
		
		if (dx2 >= *best_dist) return;
		
		if (root->right)
			FindNeighbors1_3(root->right, nd, p_focal_individual_index, best, best_dist, p_phase);
	}
	else
	{
		if (root->right)
			FindNeighbors1_3(root->right, nd, p_focal_individual_index, best, best_dist, p_phase);
		
		if (dx2 >= *best_dist) return;
		
		if (root->left)
			FindNeighbors1_3(root->left, nd, p_focal_individual_index, best, best_dist, p_phase);
	}
}

// find all neighbors in 1D
void InteractionType::FindNeighborsA_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, std::vector<EidosObjectElement *> &p_result_vec, std::vector<Individual> &p_individuals)
{
	double d = dist_sq1(root, nd);
	double dx = root->x[0] - nd[0];
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_result_vec.push_back(&(p_individuals[root->individual_index_]));
	
	if (dx > 0)
	{
		if (root->left)
			FindNeighborsA_1(root->left, nd, p_focal_individual_index, p_result_vec, p_individuals);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			FindNeighborsA_1(root->right, nd, p_focal_individual_index, p_result_vec, p_individuals);
	}
	else
	{
		if (root->right)
			FindNeighborsA_1(root->right, nd, p_focal_individual_index, p_result_vec, p_individuals);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			FindNeighborsA_1(root->left, nd, p_focal_individual_index, p_result_vec, p_individuals);
	}
}

// find all neighbors in 2D
void InteractionType::FindNeighborsA_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, std::vector<EidosObjectElement *> &p_result_vec, std::vector<Individual> &p_individuals, int p_phase)
{
	double d = dist_sq2(root, nd);
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_result_vec.push_back(&(p_individuals[root->individual_index_]));
	
	if (++p_phase >= 2) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			FindNeighborsA_2(root->left, nd, p_focal_individual_index, p_result_vec, p_individuals, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			FindNeighborsA_2(root->right, nd, p_focal_individual_index, p_result_vec, p_individuals, p_phase);
	}
	else
	{
		if (root->right)
			FindNeighborsA_2(root->right, nd, p_focal_individual_index, p_result_vec, p_individuals, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			FindNeighborsA_2(root->left, nd, p_focal_individual_index, p_result_vec, p_individuals, p_phase);
	}
}

// find all neighbors in 3D
void InteractionType::FindNeighborsA_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, std::vector<EidosObjectElement *> &p_result_vec, std::vector<Individual> &p_individuals, int p_phase)
{
	double d = dist_sq3(root, nd);
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_result_vec.push_back(&(p_individuals[root->individual_index_]));
	
	if (++p_phase >= 3) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			FindNeighborsA_3(root->left, nd, p_focal_individual_index, p_result_vec, p_individuals, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			FindNeighborsA_3(root->right, nd, p_focal_individual_index, p_result_vec, p_individuals, p_phase);
	}
	else
	{
		if (root->right)
			FindNeighborsA_3(root->right, nd, p_focal_individual_index, p_result_vec, p_individuals, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			FindNeighborsA_3(root->left, nd, p_focal_individual_index, p_result_vec, p_individuals, p_phase);
	}
}

// globals to decrease parameter-passing
slim_popsize_t gKDTree_found_count;
double gKDTree_worstbest;
int gKDTree_worstbest_index;

// find N neighbors in 1D
void InteractionType::FindNeighborsN_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, int p_count, SLiM_kdNode **best, double *best_dist)
{
	if (!root) return;
	
	double d = dist_sq1(root, nd);
	double dx = root->x[0] - nd[0];
	double dx2 = dx * dx;
	
	if (root->individual_index_ != p_focal_individual_index)
	{
		if (gKDTree_found_count == p_count)
		{
			// We have a full roster of candidates, so now the question is, is this one better than the worst one?
			if (d < gKDTree_worstbest)
			{
				// Replace the worst of the best
				best_dist[gKDTree_worstbest_index] = d;
				best[gKDTree_worstbest_index] = root;
				
				// Scan to find the new worst of the best
				gKDTree_worstbest = -1;
				
				for (int best_index = 0; best_index < p_count; ++best_index)
				{
					if (best_dist[best_index] > gKDTree_worstbest)
					{
						gKDTree_worstbest = best_dist[best_index];
						gKDTree_worstbest_index = best_index;
					}
				}
			}
		}
		else
		{
			// We do not yet have a full roster of candidates, so if this one is qualified, it is in
			if (d <= max_distance_sq_)
			{
				// Replace the first empty entry
				best_dist[gKDTree_found_count] = d;
				best[gKDTree_found_count] = root;
				
				// Update the worst of the best as needed
				if (d > gKDTree_worstbest)
				{
					gKDTree_worstbest = d;
					gKDTree_worstbest_index = gKDTree_found_count;
				}
				
				// Move to the next slot
				gKDTree_found_count++;
			}
		}
	}
	
	// Continue the search
	FindNeighborsN_1(dx > 0 ? root->left : root->right, nd, p_focal_individual_index, p_count, best, best_dist);
	
	if (gKDTree_found_count == p_count)
	{
		// If we now have a full roster, we are looking for better than our current worst of the best
		if (dx2 >= gKDTree_worstbest) return;
	}
	else
	{
		// If we do not have a full roster, we are looking for better than the max distance
		if (dx2 > max_distance_sq_) return;
	}
	
	FindNeighborsN_1(dx > 0 ? root->right : root->left, nd, p_focal_individual_index, p_count, best, best_dist);
}

// find N neighbors in 2D
void InteractionType::FindNeighborsN_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, int p_count, SLiM_kdNode **best, double *best_dist, int p_phase)
{
	if (!root) return;
	
	double d = dist_sq2(root, nd);
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	
	if (root->individual_index_ != p_focal_individual_index)
	{
		if (gKDTree_found_count == p_count)
		{
			// We have a full roster of candidates, so now the question is, is this one better than the worst one?
			if (d < gKDTree_worstbest)
			{
				// Replace the worst of the best
				best_dist[gKDTree_worstbest_index] = d;
				best[gKDTree_worstbest_index] = root;
				
				// Scan to find the new worst of the best
				gKDTree_worstbest = -1;
				
				for (int best_index = 0; best_index < p_count; ++best_index)
				{
					if (best_dist[best_index] > gKDTree_worstbest)
					{
						gKDTree_worstbest = best_dist[best_index];
						gKDTree_worstbest_index = best_index;
					}
				}
			}
		}
		else
		{
			// We do not yet have a full roster of candidates, so if this one is qualified, it is in
			if (d <= max_distance_sq_)
			{
				// Replace the first empty entry
				best_dist[gKDTree_found_count] = d;
				best[gKDTree_found_count] = root;
				
				// Update the worst of the best as needed
				if (d > gKDTree_worstbest)
				{
					gKDTree_worstbest = d;
					gKDTree_worstbest_index = gKDTree_found_count;
				}
				
				// Move to the next slot
				gKDTree_found_count++;
			}
		}
	}
	
	// Continue the search
	if (++p_phase >= 2) p_phase = 0;
	
	FindNeighborsN_2(dx > 0 ? root->left : root->right, nd, p_focal_individual_index, p_count, best, best_dist, p_phase);
	
	if (gKDTree_found_count == p_count)
	{
		// If we now have a full roster, we are looking for better than our current worst of the best
		if (dx2 >= gKDTree_worstbest) return;
	}
	else
	{
		// If we do not have a full roster, we are looking for better than the max distance
		if (dx2 > max_distance_sq_) return;
	}
	
	FindNeighborsN_2(dx > 0 ? root->right : root->left, nd, p_focal_individual_index, p_count, best, best_dist, p_phase);
}

// find N neighbors in 3D
void InteractionType::FindNeighborsN_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, int p_count, SLiM_kdNode **best, double *best_dist, int p_phase)
{
	if (!root) return;
	
	double d = dist_sq3(root, nd);
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	
	if (root->individual_index_ != p_focal_individual_index)
	{
		if (gKDTree_found_count == p_count)
		{
			// We have a full roster of candidates, so now the question is, is this one better than the worst one?
			if (d < gKDTree_worstbest)
			{
				// Replace the worst of the best
				best_dist[gKDTree_worstbest_index] = d;
				best[gKDTree_worstbest_index] = root;
				
				// Scan to find the new worst of the best
				gKDTree_worstbest = -1;
				
				for (int best_index = 0; best_index < p_count; ++best_index)
				{
					if (best_dist[best_index] > gKDTree_worstbest)
					{
						gKDTree_worstbest = best_dist[best_index];
						gKDTree_worstbest_index = best_index;
					}
				}
			}
		}
		else
		{
			// We do not yet have a full roster of candidates, so if this one is qualified, it is in
			if (d <= max_distance_sq_)
			{
				// Replace the first empty entry
				best_dist[gKDTree_found_count] = d;
				best[gKDTree_found_count] = root;
				
				// Update the worst of the best as needed
				if (d > gKDTree_worstbest)
				{
					gKDTree_worstbest = d;
					gKDTree_worstbest_index = gKDTree_found_count;
				}
				
				// Move to the next slot
				gKDTree_found_count++;
			}
		}
	}
	
	// Continue the search
	if (++p_phase >= 3) p_phase = 0;
	
	FindNeighborsN_3(dx > 0 ? root->left : root->right, nd, p_focal_individual_index, p_count, best, best_dist, p_phase);
	
	if (gKDTree_found_count == p_count)
	{
		// If we now have a full roster, we are looking for better than our current worst of the best
		if (dx2 >= gKDTree_worstbest) return;
	}
	else
	{
		// If we do not have a full roster, we are looking for better than the max distance
		if (dx2 > max_distance_sq_) return;
	}
	
	FindNeighborsN_3(dx > 0 ? root->right : root->left, nd, p_focal_individual_index, p_count, best, best_dist, p_phase);
}

void InteractionType::FindNeighbors(Subpopulation *p_subpop, InteractionsData &p_subpop_data, double *p_point, int p_count, std::vector<EidosObjectElement *> &p_result_vec, Individual *p_excluded_individual)
{
	if (spatiality_ == 0)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::FindNeighbors): (internal error) neighbors cannot be found for non-spatial interactions." << eidos_terminate();
	}
	else if (!p_subpop_data.kd_nodes_)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::FindNeighbors): (internal error) the k-d tree has not been constructed." << eidos_terminate();
	}
	else if (!p_subpop_data.kd_root_)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::FindNeighbors): (internal error) the k-d tree is rootless." << eidos_terminate();
	}
	else
	{
		if (p_count == 0)
			return;
		
		slim_popsize_t focal_individual_index;
		
		if (p_excluded_individual)
			focal_individual_index = p_excluded_individual->index_;
		else
			focal_individual_index = -1;
		
		if (p_count == 1)
		{
			// Finding a single nearest neighbor is special-cased, and does not enforce the max distance; we do that after
			SLiM_kdNode *best = nullptr;
			double best_dist = 0.0;
			
			switch (spatiality_)
			{
				case 1: FindNeighbors1_1(p_subpop_data.kd_root_, p_point, focal_individual_index, &best, &best_dist);		break;
				case 2: FindNeighbors1_2(p_subpop_data.kd_root_, p_point, focal_individual_index, &best, &best_dist, 0);	break;
				case 3: FindNeighbors1_3(p_subpop_data.kd_root_, p_point, focal_individual_index, &best, &best_dist, 0);	break;
			}
			
			if (best && (best_dist <= max_distance_sq_))
			{
				Individual *best_individual = &(p_subpop->parent_individuals_[best->individual_index_]);
				
				p_result_vec.emplace_back(best_individual);
			}
		}
		else if (p_count >= p_subpop_data.individual_count_ - 1)
		{
			// Finding all neighbors within the interaction distance is special-cased
			switch (spatiality_)
			{
				case 1: FindNeighborsA_1(p_subpop_data.kd_root_, p_point, focal_individual_index, p_result_vec, p_subpop->parent_individuals_);			break;
				case 2: FindNeighborsA_2(p_subpop_data.kd_root_, p_point, focal_individual_index, p_result_vec, p_subpop->parent_individuals_, 0);		break;
				case 3: FindNeighborsA_3(p_subpop_data.kd_root_, p_point, focal_individual_index, p_result_vec, p_subpop->parent_individuals_, 0);		break;
			}
		}
		else
		{
			// Finding multiple neighbors is the slower general case; we provide it with scratch space
			SLiM_kdNode **best;
			double *best_dist;
			
			best = (SLiM_kdNode **)calloc(p_count, sizeof(SLiM_kdNode *));
			best_dist = (double *)malloc(p_count * sizeof(double));
			gKDTree_found_count = 0;
			gKDTree_worstbest = -1;
			
			switch (spatiality_)
			{
				case 1: FindNeighborsN_1(p_subpop_data.kd_root_, p_point, focal_individual_index, p_count, best, best_dist);		break;
				case 2: FindNeighborsN_2(p_subpop_data.kd_root_, p_point, focal_individual_index, p_count, best, best_dist, 0);		break;
				case 3: FindNeighborsN_3(p_subpop_data.kd_root_, p_point, focal_individual_index, p_count, best, best_dist, 0);		break;
			}
			
			for (int best_index = 0; best_index < p_count; ++best_index)
			{
				SLiM_kdNode *best_rec = best[best_index];
				
				if (!best_rec)
					break;
				
				Individual *best_individual = &(p_subpop->parent_individuals_[best_rec->individual_index_]);
				
				p_result_vec.emplace_back(best_individual);
			}
			
			free(best);
			free(best_dist);
		}
	}
}

#pragma mark -
#pragma mark k-d tree total strength calculation

// the functions below can work with the globals here to execute callbacks; it would be slightly faster to replicate all the code
// and eliminate the if (!gSLiM_Recursive_callbacks), but that is only hit when we find a neighbor with an uncalculated strength
static Subpopulation *gSLiM_Recursive_subpop = nullptr;
static Individual *gSLiM_Recursive_receiver = nullptr;
static std::vector<SLiMEidosBlock*> *gSLiM_Recursive_callbacks = nullptr;

// total all neighbor strengths in 1D
double InteractionType::TotalNeighborStrengthA_1(SLiM_kdNode *root, double *nd, double *p_focal_strengths)
{
	double dx = root->x[0] - nd[0];
	double distance = fabs(dx);
	double total = 0.0;
	
	// Note we don't use p_focal_distances[] in 1D; it doesn't seem to be worth the overhead, since we need dx anyway...
	
	if (distance <= max_distance_)
	{
		slim_popsize_t root_individual_index = root->individual_index_;
		double strength = p_focal_strengths[root_individual_index];
		
		if (isnan(strength))
		{
			if (!gSLiM_Recursive_callbacks)
				strength = CalculateStrengthNoCallbacks(distance);
			else
				strength = CalculateStrengthWithCallbacks(distance, gSLiM_Recursive_receiver, &gSLiM_Recursive_subpop->parent_individuals_[root_individual_index], gSLiM_Recursive_subpop, *gSLiM_Recursive_callbacks);
			
			p_focal_strengths[root_individual_index] = strength;
		}
		
		total += strength;
	}
	
	if (dx > 0)
	{
		if (root->left)
			total += TotalNeighborStrengthA_1(root->left, nd, p_focal_strengths);
		
		if (distance > max_distance_) return total;
		
		if (root->right)
			total += TotalNeighborStrengthA_1(root->right, nd, p_focal_strengths);
	}
	else
	{
		if (root->right)
			total += TotalNeighborStrengthA_1(root->right, nd, p_focal_strengths);
		
		if (distance > max_distance_) return total;
		
		if (root->left)
			total += TotalNeighborStrengthA_1(root->left, nd, p_focal_strengths);
	}
	
	return total;
}

double InteractionType::TotalNeighborStrengthA_1_reciprocal(SLiM_kdNode *root, double *nd, double *p_focal_strengths, double *p_mirror_strengths, int subpop_size)
{
	double dx = root->x[0] - nd[0];
	double distance = fabs(dx);
	double total = 0.0;
	
	// Note we don't use p_focal_distances[] in 1D; it doesn't seem to be worth the overhead, since we need dx anyway...
	
	if (distance <= max_distance_)
	{
		slim_popsize_t root_individual_index = root->individual_index_;
		double strength = p_focal_strengths[root_individual_index];
		
		if (isnan(strength))
		{
			if (!gSLiM_Recursive_callbacks)
				strength = CalculateStrengthNoCallbacks(distance);
			else
				strength = CalculateStrengthWithCallbacks(distance, gSLiM_Recursive_receiver, &gSLiM_Recursive_subpop->parent_individuals_[root_individual_index], gSLiM_Recursive_subpop, *gSLiM_Recursive_callbacks);
			
			p_focal_strengths[root_individual_index] = strength;
			*(p_mirror_strengths + root_individual_index * subpop_size) = strength;
		}
		
		total += strength;
	}
	
	if (dx > 0)
	{
		if (root->left)
			total += TotalNeighborStrengthA_1_reciprocal(root->left, nd, p_focal_strengths, p_mirror_strengths, subpop_size);
		
		if (distance > max_distance_) return total;
		
		if (root->right)
			total += TotalNeighborStrengthA_1_reciprocal(root->right, nd, p_focal_strengths, p_mirror_strengths, subpop_size);
	}
	else
	{
		if (root->right)
			total += TotalNeighborStrengthA_1_reciprocal(root->right, nd, p_focal_strengths, p_mirror_strengths, subpop_size);
		
		if (distance > max_distance_) return total;
		
		if (root->left)
			total += TotalNeighborStrengthA_1_reciprocal(root->left, nd, p_focal_strengths, p_mirror_strengths, subpop_size);
	}
	
	return total;
}

// total all neighbor strengths in 2D
double InteractionType::TotalNeighborStrengthA_2(SLiM_kdNode *root, double *nd, double *p_focal_strengths, double *p_focal_distances, int p_phase)
{
	slim_popsize_t root_individual_index = root->individual_index_;
	double distance = p_focal_distances[root_individual_index];
	
	if (isnan(distance))
	{
		distance = sqrt(dist_sq2(root, nd));
		p_focal_distances[root_individual_index] = distance;
	}
	
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	double total = 0.0;
	
	if (distance <= max_distance_)
	{
		double strength = p_focal_strengths[root_individual_index];
		
		if (isnan(strength))
		{
			if (!gSLiM_Recursive_callbacks)
				strength = CalculateStrengthNoCallbacks(distance);
			else
				strength = CalculateStrengthWithCallbacks(distance, gSLiM_Recursive_receiver, &gSLiM_Recursive_subpop->parent_individuals_[root_individual_index], gSLiM_Recursive_subpop, *gSLiM_Recursive_callbacks);
			
			p_focal_strengths[root_individual_index] = strength;
		}
		
		total += strength;
	}
	
	if (++p_phase >= 2) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			total += TotalNeighborStrengthA_2(root->left, nd, p_focal_strengths, p_focal_distances, p_phase);
		
		if (dx2 > max_distance_sq_) return total;
		
		if (root->right)
			total += TotalNeighborStrengthA_2(root->right, nd, p_focal_strengths, p_focal_distances, p_phase);
	}
	else
	{
		if (root->right)
			total += TotalNeighborStrengthA_2(root->right, nd, p_focal_strengths, p_focal_distances, p_phase);
		
		if (dx2 > max_distance_sq_) return total;
		
		if (root->left)
			total += TotalNeighborStrengthA_2(root->left, nd, p_focal_strengths, p_focal_distances, p_phase);
	}
	
	return total;
}

double InteractionType::TotalNeighborStrengthA_2_reciprocal(SLiM_kdNode *root, double *nd, double *p_focal_strengths, double *p_mirror_strengths, double *p_focal_distances, double *p_mirror_distances, int subpop_size, int p_phase)
{
	slim_popsize_t root_individual_index = root->individual_index_;
	double distance = p_focal_distances[root_individual_index];
	
	if (isnan(distance))
	{
		distance = sqrt(dist_sq2(root, nd));
		p_focal_distances[root_individual_index] = distance;
		*(p_mirror_distances + root_individual_index * subpop_size) = distance;
	}
	
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	double total = 0.0;
	
	if (distance <= max_distance_)
	{
		double strength = p_focal_strengths[root_individual_index];
		
		if (isnan(strength))
		{
			if (!gSLiM_Recursive_callbacks)
				strength = CalculateStrengthNoCallbacks(distance);
			else
				strength = CalculateStrengthWithCallbacks(distance, gSLiM_Recursive_receiver, &gSLiM_Recursive_subpop->parent_individuals_[root_individual_index], gSLiM_Recursive_subpop, *gSLiM_Recursive_callbacks);
			
			p_focal_strengths[root_individual_index] = strength;
			*(p_mirror_strengths + root_individual_index * subpop_size) = strength;
		}
		
		total += strength;
	}
	
	if (++p_phase >= 2) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			total += TotalNeighborStrengthA_2_reciprocal(root->left, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_phase);
		
		if (dx2 > max_distance_sq_) return total;
		
		if (root->right)
			total += TotalNeighborStrengthA_2_reciprocal(root->right, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_phase);
	}
	else
	{
		if (root->right)
			total += TotalNeighborStrengthA_2_reciprocal(root->right, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_phase);
		
		if (dx2 > max_distance_sq_) return total;
		
		if (root->left)
			total += TotalNeighborStrengthA_2_reciprocal(root->left, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_phase);
	}
	
	return total;
}

// total all neighbor strengths in 3D
double InteractionType::TotalNeighborStrengthA_3(SLiM_kdNode *root, double *nd, double *p_focal_strengths, double *p_focal_distances, int p_phase)
{
	slim_popsize_t root_individual_index = root->individual_index_;
	double distance = p_focal_distances[root_individual_index];
	
	if (isnan(distance))
	{
		distance = sqrt(dist_sq3(root, nd));
		p_focal_distances[root_individual_index] = distance;
	}
	
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	double total = 0.0;
	
	if (distance <= max_distance_)
	{
		double strength = p_focal_strengths[root_individual_index];
		
		if (isnan(strength))
		{
			if (!gSLiM_Recursive_callbacks)
				strength = CalculateStrengthNoCallbacks(distance);
			else
				strength = CalculateStrengthWithCallbacks(distance, gSLiM_Recursive_receiver, &gSLiM_Recursive_subpop->parent_individuals_[root_individual_index], gSLiM_Recursive_subpop, *gSLiM_Recursive_callbacks);
			
			p_focal_strengths[root_individual_index] = strength;
		}
		
		total += strength;
	}
	
	if (++p_phase >= 3) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			total += TotalNeighborStrengthA_3(root->left, nd, p_focal_strengths, p_focal_distances, p_phase);
		
		if (dx2 > max_distance_sq_) return total;
		
		if (root->right)
			total += TotalNeighborStrengthA_3(root->right, nd, p_focal_strengths, p_focal_distances, p_phase);
	}
	else
	{
		if (root->right)
			total += TotalNeighborStrengthA_3(root->right, nd, p_focal_strengths, p_focal_distances, p_phase);
		
		if (dx2 > max_distance_sq_) return total;
		
		if (root->left)
			total += TotalNeighborStrengthA_3(root->left, nd, p_focal_strengths, p_focal_distances, p_phase);
	}
	
	return total;
}

double InteractionType::TotalNeighborStrengthA_3_reciprocal(SLiM_kdNode *root, double *nd, double *p_focal_strengths, double *p_mirror_strengths, double *p_focal_distances, double *p_mirror_distances, int subpop_size, int p_phase)
{
	slim_popsize_t root_individual_index = root->individual_index_;
	double distance = p_focal_distances[root_individual_index];
	
	if (isnan(distance))
	{
		distance = sqrt(dist_sq3(root, nd));
		p_focal_distances[root_individual_index] = distance;
		*(p_mirror_distances + root_individual_index * subpop_size) = distance;
	}
	
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	double total = 0.0;
	
	if (distance <= max_distance_)
	{
		double strength = p_focal_strengths[root_individual_index];
		
		if (isnan(strength))
		{
			if (!gSLiM_Recursive_callbacks)
				strength = CalculateStrengthNoCallbacks(distance);
			else
				strength = CalculateStrengthWithCallbacks(distance, gSLiM_Recursive_receiver, &gSLiM_Recursive_subpop->parent_individuals_[root_individual_index], gSLiM_Recursive_subpop, *gSLiM_Recursive_callbacks);
			
			p_focal_strengths[root_individual_index] = strength;
			*(p_mirror_strengths + root_individual_index * subpop_size) = strength;
		}
		
		total += strength;
	}
	
	if (++p_phase >= 3) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			total += TotalNeighborStrengthA_3_reciprocal(root->left, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_phase);
		
		if (dx2 > max_distance_sq_) return total;
		
		if (root->right)
			total += TotalNeighborStrengthA_3_reciprocal(root->right, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_phase);
	}
	else
	{
		if (root->right)
			total += TotalNeighborStrengthA_3_reciprocal(root->right, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_phase);
		
		if (dx2 > max_distance_sq_) return total;
		
		if (root->left)
			total += TotalNeighborStrengthA_3_reciprocal(root->left, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_phase);
	}
	
	return total;
}

double InteractionType::TotalNeighborStrength(Subpopulation *p_subpop, InteractionsData &p_subpop_data, double *p_point, Individual *p_excluded_individual)
{
	if (spatiality_ == 0)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::TotalNeighborStrength): (internal error) neighbors cannot be found for non-spatial interactions." << eidos_terminate();
	}
	else if (!p_subpop_data.kd_nodes_)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::TotalNeighborStrength): (internal error) the k-d tree has not been constructed." << eidos_terminate();
	}
	else if (!p_subpop_data.kd_root_)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::TotalNeighborStrength): (internal error) the k-d tree is rootless." << eidos_terminate();
	}
	else
	{
		slim_popsize_t focal_index = p_excluded_individual->index_;
		int subpop_size = (int)p_subpop_data.individual_count_;
		double *focal_strengths = p_subpop_data.strengths_ + focal_index * subpop_size;
		double *mirror_strengths = p_subpop_data.strengths_ + focal_index;				// for reciprocality
		double *focal_distances = p_subpop_data.distances_ + focal_index * subpop_size;
		double *mirror_distances = p_subpop_data.distances_ + focal_index;				// for reciprocality
		std::vector<SLiMEidosBlock*> &callbacks = p_subpop_data.evaluation_interaction_callbacks_;
		
		if (callbacks.size() == 0)
		{
			// No callbacks; we can assume that the callback-related globals are nilled out
			if (!reciprocal_)
			{
				switch (spatiality_)
				{
					case 1: return TotalNeighborStrengthA_1(p_subpop_data.kd_root_, p_point, focal_strengths);
					case 2: return TotalNeighborStrengthA_2(p_subpop_data.kd_root_, p_point, focal_strengths, focal_distances, 0);
					case 3: return TotalNeighborStrengthA_3(p_subpop_data.kd_root_, p_point, focal_strengths, focal_distances, 0);
				}
			}
			else
			{
				switch (spatiality_)
				{
					case 1: return TotalNeighborStrengthA_1_reciprocal(p_subpop_data.kd_root_, p_point, focal_strengths, mirror_strengths, subpop_size);
					case 2: return TotalNeighborStrengthA_2_reciprocal(p_subpop_data.kd_root_, p_point, focal_strengths, mirror_strengths, focal_distances, mirror_distances, subpop_size, 0);
					case 3: return TotalNeighborStrengthA_3_reciprocal(p_subpop_data.kd_root_, p_point, focal_strengths, mirror_strengths, focal_distances, mirror_distances, subpop_size, 0);
				}
			}
			
			return 0.0;
		}
		else
		{
			// We have callbacks, so populate our callback-related globals
			gSLiM_Recursive_subpop = p_subpop;
			gSLiM_Recursive_receiver = p_excluded_individual;
			gSLiM_Recursive_callbacks = &callbacks;
			
			double total;
			
			if (!reciprocal_)
			{
				switch (spatiality_)
				{
					case 1: total = TotalNeighborStrengthA_1(p_subpop_data.kd_root_, p_point, focal_strengths); break;
					case 2: total = TotalNeighborStrengthA_2(p_subpop_data.kd_root_, p_point, focal_strengths, focal_distances, 0); break;
					case 3: total = TotalNeighborStrengthA_3(p_subpop_data.kd_root_, p_point, focal_strengths, focal_distances, 0); break;
					default: total = 0.0; break;
				}
			}
			else
			{
				switch (spatiality_)
				{
					case 1: total = TotalNeighborStrengthA_1_reciprocal(p_subpop_data.kd_root_, p_point, focal_strengths, mirror_strengths, subpop_size); break;
					case 2: total = TotalNeighborStrengthA_2_reciprocal(p_subpop_data.kd_root_, p_point, focal_strengths, mirror_strengths, focal_distances, mirror_distances, subpop_size, 0); break;
					case 3: total = TotalNeighborStrengthA_3_reciprocal(p_subpop_data.kd_root_, p_point, focal_strengths, mirror_strengths, focal_distances, mirror_distances, subpop_size, 0); break;
					default: total = 0.0; break;
				}
			}
			
			gSLiM_Recursive_subpop = nullptr;
			gSLiM_Recursive_receiver = nullptr;
			gSLiM_Recursive_callbacks = nullptr;
			
			return total;
		}
	}
}


#pragma mark -
#pragma mark k-d tree neighbor strength fetching

// fetch all neighbor strengths in 1D
void InteractionType::FillNeighborStrengthsA_1(SLiM_kdNode *root, double *nd, double *p_focal_strengths, std::vector<double> &p_result_vec)
{
	double dx = root->x[0] - nd[0];
	double distance = fabs(dx);
	
	// Note we don't use p_focal_distances[] in 1D; it doesn't seem to be worth the overhead, since we need dx anyway...
	
	if (distance <= max_distance_)
	{
		slim_popsize_t root_individual_index = root->individual_index_;
		double strength = p_focal_strengths[root_individual_index];
		
		if (isnan(strength))
		{
			if (!gSLiM_Recursive_callbacks)
				strength = CalculateStrengthNoCallbacks(distance);
			else
				strength = CalculateStrengthWithCallbacks(distance, gSLiM_Recursive_receiver, &gSLiM_Recursive_subpop->parent_individuals_[root_individual_index], gSLiM_Recursive_subpop, *gSLiM_Recursive_callbacks);
			
			p_focal_strengths[root_individual_index] = strength;
		}
		
		p_result_vec[root_individual_index] = strength;
	}
	
	if (dx > 0)
	{
		if (root->left)
			FillNeighborStrengthsA_1(root->left, nd, p_focal_strengths, p_result_vec);
		
		if (distance > max_distance_) return;
		
		if (root->right)
			FillNeighborStrengthsA_1(root->right, nd, p_focal_strengths, p_result_vec);
	}
	else
	{
		if (root->right)
			FillNeighborStrengthsA_1(root->right, nd, p_focal_strengths, p_result_vec);
		
		if (distance > max_distance_) return;
		
		if (root->left)
			FillNeighborStrengthsA_1(root->left, nd, p_focal_strengths, p_result_vec);
	}
}

void InteractionType::FillNeighborStrengthsA_1_reciprocal(SLiM_kdNode *root, double *nd, double *p_focal_strengths, double *p_mirror_strengths, int subpop_size, std::vector<double> &p_result_vec)
{
	double dx = root->x[0] - nd[0];
	double distance = fabs(dx);
	
	// Note we don't use p_focal_distances[] in 1D; it doesn't seem to be worth the overhead, since we need dx anyway...
	
	if (distance <= max_distance_)
	{
		slim_popsize_t root_individual_index = root->individual_index_;
		double strength = p_focal_strengths[root_individual_index];
		
		if (isnan(strength))
		{
			if (!gSLiM_Recursive_callbacks)
				strength = CalculateStrengthNoCallbacks(distance);
			else
				strength = CalculateStrengthWithCallbacks(distance, gSLiM_Recursive_receiver, &gSLiM_Recursive_subpop->parent_individuals_[root_individual_index], gSLiM_Recursive_subpop, *gSLiM_Recursive_callbacks);
			
			p_focal_strengths[root_individual_index] = strength;
			*(p_mirror_strengths + root_individual_index * subpop_size) = strength;
		}
		
		p_result_vec[root_individual_index] = strength;
	}
	
	if (dx > 0)
	{
		if (root->left)
			FillNeighborStrengthsA_1_reciprocal(root->left, nd, p_focal_strengths, p_mirror_strengths, subpop_size, p_result_vec);
		
		if (distance > max_distance_) return;
		
		if (root->right)
			FillNeighborStrengthsA_1_reciprocal(root->right, nd, p_focal_strengths, p_mirror_strengths, subpop_size, p_result_vec);
	}
	else
	{
		if (root->right)
			FillNeighborStrengthsA_1_reciprocal(root->right, nd, p_focal_strengths, p_mirror_strengths, subpop_size, p_result_vec);
		
		if (distance > max_distance_) return;
		
		if (root->left)
			FillNeighborStrengthsA_1_reciprocal(root->left, nd, p_focal_strengths, p_mirror_strengths, subpop_size, p_result_vec);
	}
}

// fetch all neighbor strengths in 2D
void InteractionType::FillNeighborStrengthsA_2(SLiM_kdNode *root, double *nd, double *p_focal_strengths, double *p_focal_distances, std::vector<double> &p_result_vec, int p_phase)
{
	slim_popsize_t root_individual_index = root->individual_index_;
	double distance = p_focal_distances[root_individual_index];
	
	if (isnan(distance))
	{
		distance = sqrt(dist_sq2(root, nd));
		p_focal_distances[root_individual_index] = distance;
	}
	
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	
	if (distance <= max_distance_)
	{
		double strength = p_focal_strengths[root_individual_index];
		
		if (isnan(strength))
		{
			if (!gSLiM_Recursive_callbacks)
				strength = CalculateStrengthNoCallbacks(distance);
			else
				strength = CalculateStrengthWithCallbacks(distance, gSLiM_Recursive_receiver, &gSLiM_Recursive_subpop->parent_individuals_[root_individual_index], gSLiM_Recursive_subpop, *gSLiM_Recursive_callbacks);
			
			p_focal_strengths[root_individual_index] = strength;
		}
		
		p_result_vec[root_individual_index] = strength;
	}
	
	if (++p_phase >= 2) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			FillNeighborStrengthsA_2(root->left, nd, p_focal_strengths, p_focal_distances, p_result_vec, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			FillNeighborStrengthsA_2(root->right, nd, p_focal_strengths, p_focal_distances, p_result_vec, p_phase);
	}
	else
	{
		if (root->right)
			FillNeighborStrengthsA_2(root->right, nd, p_focal_strengths, p_focal_distances, p_result_vec, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			FillNeighborStrengthsA_2(root->left, nd, p_focal_strengths, p_focal_distances, p_result_vec, p_phase);
	}
}

void InteractionType::FillNeighborStrengthsA_2_reciprocal(SLiM_kdNode *root, double *nd, double *p_focal_strengths, double *p_mirror_strengths, double *p_focal_distances, double *p_mirror_distances, int subpop_size, std::vector<double> &p_result_vec, int p_phase)
{
	slim_popsize_t root_individual_index = root->individual_index_;
	double distance = p_focal_distances[root_individual_index];
	
	if (isnan(distance))
	{
		distance = sqrt(dist_sq2(root, nd));
		p_focal_distances[root_individual_index] = distance;
		*(p_mirror_distances + root_individual_index * subpop_size) = distance;
	}
	
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	
	if (distance <= max_distance_)
	{
		double strength = p_focal_strengths[root_individual_index];
		
		if (isnan(strength))
		{
			if (!gSLiM_Recursive_callbacks)
				strength = CalculateStrengthNoCallbacks(distance);
			else
				strength = CalculateStrengthWithCallbacks(distance, gSLiM_Recursive_receiver, &gSLiM_Recursive_subpop->parent_individuals_[root_individual_index], gSLiM_Recursive_subpop, *gSLiM_Recursive_callbacks);
			
			p_focal_strengths[root_individual_index] = strength;
			*(p_mirror_strengths + root_individual_index * subpop_size) = strength;
		}
		
		p_result_vec[root_individual_index] = strength;
	}
	
	if (++p_phase >= 2) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			FillNeighborStrengthsA_2_reciprocal(root->left, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_result_vec, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			FillNeighborStrengthsA_2_reciprocal(root->right, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_result_vec, p_phase);
	}
	else
	{
		if (root->right)
			FillNeighborStrengthsA_2_reciprocal(root->right, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_result_vec, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			FillNeighborStrengthsA_2_reciprocal(root->left, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_result_vec, p_phase);
	}
}

// fetch all neighbor strengths in 3D
void InteractionType::FillNeighborStrengthsA_3(SLiM_kdNode *root, double *nd, double *p_focal_strengths, double *p_focal_distances, std::vector<double> &p_result_vec, int p_phase)
{
	slim_popsize_t root_individual_index = root->individual_index_;
	double distance = p_focal_distances[root_individual_index];
	
	if (isnan(distance))
	{
		distance = sqrt(dist_sq3(root, nd));
		p_focal_distances[root_individual_index] = distance;
	}
	
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	
	if (distance <= max_distance_)
	{
		double strength = p_focal_strengths[root_individual_index];
		
		if (isnan(strength))
		{
			if (!gSLiM_Recursive_callbacks)
				strength = CalculateStrengthNoCallbacks(distance);
			else
				strength = CalculateStrengthWithCallbacks(distance, gSLiM_Recursive_receiver, &gSLiM_Recursive_subpop->parent_individuals_[root_individual_index], gSLiM_Recursive_subpop, *gSLiM_Recursive_callbacks);
			
			p_focal_strengths[root_individual_index] = strength;
		}
		
		p_result_vec[root_individual_index] = strength;
	}
	
	if (++p_phase >= 3) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			FillNeighborStrengthsA_3(root->left, nd, p_focal_strengths, p_focal_distances, p_result_vec, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			FillNeighborStrengthsA_3(root->right, nd, p_focal_strengths, p_focal_distances, p_result_vec, p_phase);
	}
	else
	{
		if (root->right)
			FillNeighborStrengthsA_3(root->right, nd, p_focal_strengths, p_focal_distances, p_result_vec, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			FillNeighborStrengthsA_3(root->left, nd, p_focal_strengths, p_focal_distances, p_result_vec, p_phase);
	}
}

void InteractionType::FillNeighborStrengthsA_3_reciprocal(SLiM_kdNode *root, double *nd, double *p_focal_strengths, double *p_mirror_strengths, double *p_focal_distances, double *p_mirror_distances, int subpop_size, std::vector<double> &p_result_vec, int p_phase)
{
	slim_popsize_t root_individual_index = root->individual_index_;
	double distance = p_focal_distances[root_individual_index];
	
	if (isnan(distance))
	{
		distance = sqrt(dist_sq3(root, nd));
		p_focal_distances[root_individual_index] = distance;
		*(p_mirror_distances + root_individual_index * subpop_size) = distance;
	}
	
	double dx = root->x[p_phase] - nd[p_phase];
	double dx2 = dx * dx;
	
	if (distance <= max_distance_)
	{
		double strength = p_focal_strengths[root_individual_index];
		
		if (isnan(strength))
		{
			if (!gSLiM_Recursive_callbacks)
				strength = CalculateStrengthNoCallbacks(distance);
			else
				strength = CalculateStrengthWithCallbacks(distance, gSLiM_Recursive_receiver, &gSLiM_Recursive_subpop->parent_individuals_[root_individual_index], gSLiM_Recursive_subpop, *gSLiM_Recursive_callbacks);
			
			p_focal_strengths[root_individual_index] = strength;
			*(p_mirror_strengths + root_individual_index * subpop_size) = strength;
		}
		
		p_result_vec[root_individual_index] = strength;
	}
	
	if (++p_phase >= 3) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			FillNeighborStrengthsA_3_reciprocal(root->left, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_result_vec, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			FillNeighborStrengthsA_3_reciprocal(root->right, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_result_vec, p_phase);
	}
	else
	{
		if (root->right)
			FillNeighborStrengthsA_3_reciprocal(root->right, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_result_vec, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			FillNeighborStrengthsA_3_reciprocal(root->left, nd, p_focal_strengths, p_mirror_strengths, p_focal_distances, p_mirror_distances, subpop_size, p_result_vec, p_phase);
	}
}

void InteractionType::FillNeighborStrengths(Subpopulation *p_subpop, InteractionsData &p_subpop_data, double *p_point, Individual *p_excluded_individual, std::vector<double> &p_result_vec)
{
	if (spatiality_ == 0)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::FillNeighborStrengths): (internal error) neighbors cannot be found for non-spatial interactions." << eidos_terminate();
	}
	else if (!p_subpop_data.kd_nodes_)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::FillNeighborStrengths): (internal error) the k-d tree has not been constructed." << eidos_terminate();
	}
	else if (!p_subpop_data.kd_root_)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::FillNeighborStrengths): (internal error) the k-d tree is rootless." << eidos_terminate();
	}
	else
	{
		slim_popsize_t focal_index = p_excluded_individual->index_;
		int subpop_size = (int)p_subpop_data.individual_count_;
		double *focal_strengths = p_subpop_data.strengths_ + focal_index * subpop_size;
		double *mirror_strengths = p_subpop_data.strengths_ + focal_index;				// for reciprocality
		double *focal_distances = p_subpop_data.distances_ + focal_index * subpop_size;
		double *mirror_distances = p_subpop_data.distances_ + focal_index;				// for reciprocality
		std::vector<SLiMEidosBlock*> &callbacks = p_subpop_data.evaluation_interaction_callbacks_;
		
		if (callbacks.size() == 0)
		{
			// No callbacks; we can assume that the callback-related globals are nilled out
			if (!reciprocal_)
			{
				switch (spatiality_)
				{
					case 1: FillNeighborStrengthsA_1(p_subpop_data.kd_root_, p_point, focal_strengths, p_result_vec); break;
					case 2: FillNeighborStrengthsA_2(p_subpop_data.kd_root_, p_point, focal_strengths, focal_distances, p_result_vec, 0); break;
					case 3: FillNeighborStrengthsA_3(p_subpop_data.kd_root_, p_point, focal_strengths, focal_distances, p_result_vec, 0); break;
				}
			}
			else
			{
				switch (spatiality_)
				{
					case 1: FillNeighborStrengthsA_1_reciprocal(p_subpop_data.kd_root_, p_point, focal_strengths, mirror_strengths, subpop_size, p_result_vec); break;
					case 2: FillNeighborStrengthsA_2_reciprocal(p_subpop_data.kd_root_, p_point, focal_strengths, mirror_strengths, focal_distances, mirror_distances, subpop_size, p_result_vec, 0); break;
					case 3: FillNeighborStrengthsA_3_reciprocal(p_subpop_data.kd_root_, p_point, focal_strengths, mirror_strengths, focal_distances, mirror_distances, subpop_size, p_result_vec, 0); break;
				}
			}
		}
		else
		{
			// We have callbacks, so populate our callback-related globals
			gSLiM_Recursive_subpop = p_subpop;
			gSLiM_Recursive_receiver = p_excluded_individual;
			gSLiM_Recursive_callbacks = &callbacks;
			
			if (!reciprocal_)
			{
				switch (spatiality_)
				{
					case 1: FillNeighborStrengthsA_1(p_subpop_data.kd_root_, p_point, focal_strengths, p_result_vec); break;
					case 2: FillNeighborStrengthsA_2(p_subpop_data.kd_root_, p_point, focal_strengths, focal_distances, p_result_vec, 0); break;
					case 3: FillNeighborStrengthsA_3(p_subpop_data.kd_root_, p_point, focal_strengths, focal_distances, p_result_vec, 0); break;
				}
			}
			else
			{
				switch (spatiality_)
				{
					case 1: FillNeighborStrengthsA_1_reciprocal(p_subpop_data.kd_root_, p_point, focal_strengths, mirror_strengths, subpop_size, p_result_vec); break;
					case 2: FillNeighborStrengthsA_2_reciprocal(p_subpop_data.kd_root_, p_point, focal_strengths, mirror_strengths, focal_distances, mirror_distances, subpop_size, p_result_vec, 0); break;
					case 3: FillNeighborStrengthsA_3_reciprocal(p_subpop_data.kd_root_, p_point, focal_strengths, mirror_strengths, focal_distances, mirror_distances, subpop_size, p_result_vec, 0); break;
				}
			}
			
			gSLiM_Recursive_subpop = nullptr;
			gSLiM_Recursive_receiver = nullptr;
			gSLiM_Recursive_callbacks = nullptr;
		}
	}
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support

const EidosObjectClass *InteractionType::Class(void) const
{
	return gSLiM_InteractionType_Class;
}

void InteractionType::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ElementType() << "<i" << interaction_type_id_ << ">";
}

EidosValue_SP InteractionType::GetProperty(EidosGlobalStringID p_property_id)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
			// constants
		case gID_id:						// ACCELERATED
		{
			if (!cached_value_inttype_id_)
				cached_value_inttype_id_ = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(interaction_type_id_));
			return cached_value_inttype_id_;
		}
		case gID_reciprocal:
		{
			return (reciprocal_ ? gStaticEidosValue_LogicalT : gStaticEidosValue_LogicalF);
		}
		case gID_sexSegregation:
		{
			std::string sex_segregation_string;
			
			switch (receiver_sex_)
			{
				case IndividualSex::kFemale:	sex_segregation_string += "F"; break;
				case IndividualSex::kMale:		sex_segregation_string += "M"; break;
				default:						sex_segregation_string += "*"; break;
			}
			
			switch (exerter_sex_)
			{
				case IndividualSex::kFemale:	sex_segregation_string += "F"; break;
				case IndividualSex::kMale:		sex_segregation_string += "M"; break;
				default:						sex_segregation_string += "*"; break;
			}
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(sex_segregation_string));
		}
		case gID_spatiality:
		{
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_String_singleton(spatiality_string_));
		}
			
			// variables
		case gID_maxDistance:
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(max_distance_));
		case gID_tag:						// ACCELERATED
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value_));
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

int64_t InteractionType::GetProperty_Accelerated_Int(EidosGlobalStringID p_property_id)
{
	switch (p_property_id)
	{
		case gID_id:				return interaction_type_id_;
		case gID_tag:				return tag_value_;
			
		default:					return EidosObjectElement::GetProperty_Accelerated_Int(p_property_id);
	}
}

void InteractionType::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_maxDistance:
		{
			if (AnyEvaluated())
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): maxDistance cannot be changed while the interaction is being evaluated; call unevaluate() first, or set maxDistance prior to evaluation of the interaction." << eidos_terminate();
			
			max_distance_ = p_value.FloatAtIndex(0, nullptr);
			max_distance_sq_ = max_distance_ * max_distance_;
			
			if (max_distance_ < 0.0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): the maximum interaction distance must be greater than or equal to zero." << eidos_terminate();
			if ((if_type_ == IFType::kLinear) && (isinf(max_distance_) || (max_distance_ <= 0.0)))
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): the maximum interaction distance must be finite and greater than zero when interaction type 'l' has been chosen." << eidos_terminate();
			
			return;
		}
			
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

EidosValue_SP InteractionType::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter)
{
	EidosValue *arg0_value = ((p_argument_count >= 1) ? p_arguments[0].get() : nullptr);
	EidosValue *arg1_value = ((p_argument_count >= 2) ? p_arguments[1].get() : nullptr);
	EidosValue *arg2_value = ((p_argument_count >= 3) ? p_arguments[2].get() : nullptr);
	
	
	switch (p_method_id)
	{
			//
			//	*********************	– (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
			//
			#pragma mark -distance()
			
		case gID_distance:
		{
			EidosValue *individuals1 = arg0_value, *individuals2 = arg1_value;
			int count1 = individuals1->Count(), count2 = individuals2->Count();
			
			if (spatiality_ == 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): distance() requires that the interaction be spatial." << eidos_terminate();
			if ((count1 != 1) && (count2 != 1))
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): distance() requires that either individuals1 or individuals2 be singleton." << eidos_terminate();
			
			// Rearrange so that if either vector is non-singleton, it is the second that is non-singleton (one-to-many)
			if (count1 != 1)
			{
				std::swap(individuals1, individuals2);
				std::swap(count1, count2);
			}
			
			// individuals1 is guaranteed to be singleton; let's get the info on it
			Individual *ind1 = (Individual *)individuals1->ObjectElementAtIndex(0, nullptr);
			Subpopulation *subpop1 = &(ind1->subpopulation_);
			slim_objectid_t subpop1_id = subpop1->subpopulation_id_;
			slim_popsize_t subpop1_size = subpop1->parent_subpop_size_;
			int ind1_index = ind1->index_;
			auto subpop_data_iter = data_.find(subpop1_id);
			
			if (subpop_data_iter == data_.end())
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): distance() requires that the interaction has been evaluated for the subpopulation first." << eidos_terminate();
			
			InteractionsData &subpop_data = subpop_data_iter->second;
			
			EnsureDistancesPresent(subpop_data);
			
			double *ind1_distances = subpop_data.distances_ + ind1_index * subpop1_size;
			double *mirror_ind1_distances = subpop_data.distances_ + ind1_index;			// used when reciprocality is enabled
			double *position_data = subpop_data.positions_;
			double *ind1_position = position_data + ind1_index * SLIM_MAX_DIMENSIONALITY;
			
			if (individuals2->Type() == EidosValueType::kValueNULL)
			{
				// NULL means return distances from individuals1 (which must be singleton) to all individuals in the subpopulation
				EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(subpop1_size);
				
				if (!reciprocal_)
				{
					// No reciprocality, so we don't mirror
					for (int ind2_index = 0; ind2_index < subpop1_size; ++ind2_index)
					{
						double distance = ind1_distances[ind2_index];
						
						if (isnan(distance))
						{
							distance = CalculateDistance(ind1_position, position_data + ind2_index * SLIM_MAX_DIMENSIONALITY);
							ind1_distances[ind2_index] = distance;
						}
						
						result_vec->PushFloat(distance);
					}
				}
				else
				{
					// Reciprocality; mirror all values that we calculate
					for (int ind2_index = 0; ind2_index < subpop1_size; ++ind2_index, mirror_ind1_distances += subpop1_size)
					{
						double distance = ind1_distances[ind2_index];
						
						if (isnan(distance))
						{
							distance = CalculateDistance(ind1_position, position_data + ind2_index * SLIM_MAX_DIMENSIONALITY);
							ind1_distances[ind2_index] = distance;
							
							*(mirror_ind1_distances) = distance;
						}
						
						result_vec->PushFloat(distance);
					}
				}
				
				return EidosValue_SP(result_vec);
			}
			else
			{
				// Otherwise, individuals1 is singleton, and individuals2 is any length, so we loop over individuals2
				EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(count2);
				
				for (int ind2_index = 0; ind2_index < count2; ++ind2_index)
				{
					Individual *ind2 = (Individual *)individuals2->ObjectElementAtIndex(ind2_index, nullptr);
					
					if (subpop1 != &(ind2->subpopulation_))
						EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): distance() requires that all individuals be in the same subpopulation." << eidos_terminate();
					
					slim_popsize_t ind2_index_in_subpop = ind2->index_;
					double distance = ind1_distances[ind2_index_in_subpop];
					
					if (isnan(distance))
					{
						distance = CalculateDistance(ind1_position, position_data + ind2_index_in_subpop * SLIM_MAX_DIMENSIONALITY);
						ind1_distances[ind2_index_in_subpop] = distance;
						
						// If reciprocality is enabled, push the calculated distance into the reciprocal entry.  There's
						// enough cruft in this version of the loop that it doesn't seem worth cloning the whole loop.
						if (reciprocal_)
							*(mirror_ind1_distances + ind2_index_in_subpop * subpop1_size) = distance;
					}
					
					result_vec->PushFloat(distance);
				}
				
				return EidosValue_SP(result_vec);
			}
		}
			
			//
			//	*********************	– (float)distanceToPoint(object<Individual> individuals1, float point)
			//
			#pragma mark -distanceToPoint()
			
		case gID_distanceToPoint:
		{
			EidosValue *individuals = arg0_value, *point = arg1_value;
			int count = individuals->Count(), point_count = point->Count();
			
			if (spatiality_ == 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): distanceToPoint() requires that the interaction be spatial." << eidos_terminate();
			if (point_count < spatiality_)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): distanceToPoint() requires that point is of length at least equal to the interaction spatiality." << eidos_terminate();
			
			if (count == 0)
				return gStaticEidosValue_Float_ZeroVec;
			
			// Get the point's coordinates into a double[]
			double point_data[SLIM_MAX_DIMENSIONALITY];
			
			for (int point_index = 0; point_index < spatiality_; ++point_index)
				point_data[point_index] = point->FloatAtIndex(point_index, nullptr);
			
			// individuals is guaranteed to be of length >= 1; let's get the info on it
			Individual *ind_first = (Individual *)individuals->ObjectElementAtIndex(0, nullptr);
			Subpopulation *subpop1 = &(ind_first->subpopulation_);
			slim_objectid_t subpop1_id = subpop1->subpopulation_id_;
			auto subpop_data_iter = data_.find(subpop1_id);
			
			if (subpop_data_iter == data_.end())
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): distanceToPoint() requires that the interaction has been evaluated for the subpopulation first." << eidos_terminate();
			
			InteractionsData &subpop_data = subpop_data_iter->second;
			
			double *position_data = subpop_data.positions_;
			
			EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(count);
			
			for (int ind_index = 0; ind_index < count; ++ind_index)
			{
				Individual *ind = (Individual *)individuals->ObjectElementAtIndex(ind_index, nullptr);
				
				if (subpop1 != &(ind->subpopulation_))
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): distanceToPoint() requires that all individuals be in the same subpopulation." << eidos_terminate();
				
				double *ind_position = position_data + ind->index_ * SLIM_MAX_DIMENSIONALITY;
				
				result_vec->PushFloat(CalculateDistance(ind_position, point_data));
			}
			
			return EidosValue_SP(result_vec);
		}
			
			
			//
			//	*********************	– (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
			//
			#pragma mark -drawByStrength()
			
		case gID_drawByStrength:
		{
			// Check the individual and subpop
			Individual *individual = (Individual *)arg0_value->ObjectElementAtIndex(0, nullptr);
			Subpopulation *subpop = &(individual->subpopulation_);
			slim_objectid_t subpop_id = subpop->subpopulation_id_;
			slim_popsize_t subpop_size = subpop->parent_subpop_size_;
			int ind_index = individual->index_;
			auto subpop_data_iter = data_.find(subpop_id);
			
			if (subpop_data_iter == data_.end())
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): drawByStrength() requires that the interaction has been evaluated for the subpopulation first." << eidos_terminate();
			
			// Check the count
			int64_t count = arg1_value->IntAtIndex(0, nullptr);
			
			if (count <= 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): drawByStrength() requires count > 0." << eidos_terminate();
			
			// Find the neighbors
			InteractionsData &subpop_data = subpop_data_iter->second;
			std::vector<SLiMEidosBlock*> &callbacks = subpop_data.evaluation_interaction_callbacks_;
			bool no_callbacks = (callbacks.size() == 0);
			
			EnsureKDTreePresent(subpop_data);
			EnsureStrengthsPresent(subpop_data);
			
			std::vector<EidosObjectElement *> neighbors;
			
			if (spatiality_ == 0)
			{
				// For non-spatial interactions, just use the subpop's vector of individuals
				neighbors.reserve(subpop_size);
				for (Individual &subpop_individual : subpop->parent_individuals_)
					neighbors.emplace_back(&subpop_individual);
			}
			else
			{
				// For spatial interactions, find all neighbors, up to the subpopulation size
				double *position_data = subpop_data.positions_;
				double *ind_position = position_data + ind_index * SLIM_MAX_DIMENSIONALITY;
				
				neighbors.reserve(subpop_size);
				FindNeighbors(subpop, subpop_data, ind_position, subpop_size, neighbors, individual);
			}
			
			// Total the interaction strengths with all neighbors; this has the side effect of caching all relevant strengths
			double total_interaction_strength = 0.0;
			std::vector<double> cached_strength;
			
			cached_strength.reserve((int)count);
			
			if (spatiality_ == 0)
			{
				double *ind1_strengths = subpop_data.strengths_ + ind_index * subpop_size;
				double *mirror_ind1_strengths = subpop_data.strengths_ + ind_index;			// used when reciprocality is enabled
				
				for (EidosObjectElement *neighbor : neighbors)
				{
					Individual *ind2 = (Individual *)neighbor;
					
					slim_popsize_t ind2_index_in_subpop = ind2->index_;
					double strength = ind1_strengths[ind2_index_in_subpop];
					
					if (isnan(strength))
					{
						if (no_callbacks)
							strength = CalculateStrengthNoCallbacks(NAN);
						else
							strength = CalculateStrengthWithCallbacks(NAN, individual, ind2, subpop, callbacks);
						
						ind1_strengths[ind2_index_in_subpop] = strength;
						
						if (reciprocal_)
							*(mirror_ind1_strengths + ind2_index_in_subpop * subpop_size) = strength;
					}
					
					total_interaction_strength += strength;
					cached_strength.emplace_back(strength);
				}
			}
			else
			{
				double *ind1_strengths = subpop_data.strengths_ + ind_index * subpop_size;
				double *mirror_ind1_strengths = subpop_data.strengths_ + ind_index;			// for reciprocality
				double *ind1_distances = subpop_data.distances_ + ind_index * subpop_size;
				double *mirror_ind1_distances = subpop_data.distances_ + ind_index;			// for reciprocality
				double *position_data = subpop_data.positions_;
				double *ind1_position = position_data + ind_index * SLIM_MAX_DIMENSIONALITY;
				
				for (EidosObjectElement *neighbor : neighbors)
				{
					Individual *ind2 = (Individual *)neighbor;
					
					slim_popsize_t ind2_index_in_subpop = ind2->index_;
					double strength = ind1_strengths[ind2_index_in_subpop];
					
					if (isnan(strength))
					{
						double distance = ind1_distances[ind2_index_in_subpop];
						
						if (isnan(distance))
						{
							distance = CalculateDistance(ind1_position, position_data + ind2_index_in_subpop * SLIM_MAX_DIMENSIONALITY);
							ind1_distances[ind2_index_in_subpop] = distance;
							
							if (reciprocal_)
								*(mirror_ind1_distances + ind2_index_in_subpop * subpop_size) = distance;
						}
						
						if (distance <= max_distance_)
						{
							if (no_callbacks)
								strength = CalculateStrengthNoCallbacks(distance);
							else
								strength = CalculateStrengthWithCallbacks(distance, individual, ind2, subpop, callbacks);
						}
						else
						{
							strength = 0.0;
						}
						
						ind1_strengths[ind2_index_in_subpop] = strength;
						
						if (reciprocal_)
							*(mirror_ind1_strengths + ind2_index_in_subpop * subpop_size) = strength;
					}
					
					total_interaction_strength += strength;
					cached_strength.emplace_back(strength);
				}
			}
			
			// Draw individuals; we do this using either the GSL or linear search, depending on the query size
			// This choice is somewhat problematic.  I empirically determined at what query size the GSL started
			// to pay off despite the overhead of setup with gsl_ran_discrete_preproc().  However, I did that for
			// a particular subpopulation size; and the crossover point might also depend upon the distribution
			// of strength values in the subpopulation.  I'm not going to worry about this too much, though; it
			// is not really answerable in general, and a crossover of 50 seems reasonable.  For small counts,
			// linear search won't take that long anyway, and there must be a limit >= 1 where linear is faster
			// than the GSL; and for large counts the GSL is surely a win.  Trying to figure out exactly where
			// the crossover is in all cases would be overkill; my testing indicates the performance difference
			// between the two methods is not really that large anyway.
			EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
			
			if (total_interaction_strength > 0.0)
			{
				result_vec->Reserve((int)count);
				std::vector<EidosObjectElement *> *result_direct = result_vec->ObjectElementVector_Mutable();
				
				if (count > 50)		// the empirically determined crossover point in performance
				{
					// Use gsl_ran_discrete() to do the drawing
					gsl_ran_discrete_t *gsl_lookup = gsl_ran_discrete_preproc(cached_strength.size(), cached_strength.data());
					
					for (int draw_index = 0; draw_index < count; ++draw_index)
					{
						int hit_index = (int)gsl_ran_discrete(gEidos_rng, gsl_lookup);
						
						result_direct->push_back(neighbors[hit_index]);
					}
					
					gsl_ran_discrete_free(gsl_lookup);
				}
				else
				{
					// Use linear search to do the drawing
					for (int draw_index = 0; draw_index < count; ++draw_index)
					{
						double the_rose_in_the_teeth = gsl_rng_uniform(gEidos_rng) * total_interaction_strength;
						double cumulative_strength = 0.0;
						int neighbors_size = (int)neighbors.size();
						int hit_index;
						
						for (hit_index = 0; hit_index < neighbors_size; ++hit_index)
						{
							double strength = cached_strength[hit_index];
							
							cumulative_strength += strength;
							
							if (the_rose_in_the_teeth <= cumulative_strength)
								break;
						}
						if (hit_index >= neighbors_size)
							hit_index = neighbors_size - 1;
						
						result_direct->push_back(neighbors[hit_index]);
					}
				}
			}
			
			return EidosValue_SP(result_vec);
		}
			
			
			//
			//	*********************	- (void)evaluate([No<Subpopulation> subpops = NULL], [logical$ immediate = F])
			//
			#pragma mark -evaluate()
			
		case gID_evaluate:
		{
			SLiMSim *sim = dynamic_cast<SLiMSim *>(p_interpreter.Context());
			
			if (!sim)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): (internal error) the sim is not registered as the context pointer." << eidos_terminate();
			
			if (sim->GenerationStage() == SLiMGenerationStage::kStage2GenerateOffspring)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): evaluate() may not be called during offspring generation." << eidos_terminate();
			
			bool immediate = arg1_value->LogicalAtIndex(0, nullptr);
			
			if (arg0_value->Type() == EidosValueType::kValueNULL)
			{
				for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : sim->ThePopulation())
					EvaluateSubpopulation(subpop_pair.second, immediate);
			}
			else
			{
				// requested subpops, so get them
				int requested_subpop_count = arg0_value->Count();
				std::vector<Subpopulation*> subpops_to_evaluate;
				
				if (requested_subpop_count)
				{
					for (int requested_subpop_index = 0; requested_subpop_index < requested_subpop_count; ++requested_subpop_index)
						EvaluateSubpopulation((Subpopulation *)(arg0_value->ObjectElementAtIndex(requested_subpop_index, nullptr)), immediate);
				}
			}
			
			return gStaticEidosValueNULLInvisible;
		}
			
			
			//
			//	*********************	– (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
			//
			#pragma mark -nearestNeighbors()
			
		case gID_nearestNeighbors:
		{
			if (spatiality_ == 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): nearestNeighbors() requires that the interaction be spatial." << eidos_terminate();
			
			// Check the individual and subpop
			Individual *individual = (Individual *)arg0_value->ObjectElementAtIndex(0, nullptr);
			Subpopulation *subpop = &(individual->subpopulation_);
			slim_objectid_t subpop_id = subpop->subpopulation_id_;
			slim_popsize_t subpop_size = subpop->parent_subpop_size_;
			int ind_index = individual->index_;
			auto subpop_data_iter = data_.find(subpop_id);
			
			if (subpop_data_iter == data_.end())
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): nearestNeighbors() requires that the interaction has been evaluated for the subpopulation first." << eidos_terminate();
			
			// Check the count
			int64_t count = arg1_value->IntAtIndex(0, nullptr);
			
			if (count <= 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): nearestNeighbors() requires count > 0." << eidos_terminate();
			
			if (count > subpop_size)
				count = subpop_size;
			
			// Find the neighbors
			InteractionsData &subpop_data = subpop_data_iter->second;
			
			double *position_data = subpop_data.positions_;
			double *ind_position = position_data + ind_index * SLIM_MAX_DIMENSIONALITY;
			
			EnsureKDTreePresent(subpop_data);
			
			EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->Reserve((int)count);
			std::vector<EidosObjectElement *> *result_direct = result_vec->ObjectElementVector_Mutable();
			
			FindNeighbors(subpop, subpop_data, ind_position, (int)count, *result_direct, individual);
			
			return EidosValue_SP(result_vec);
		}
			
			
			//
			//	*********************	– (object<Individual>)nearestNeighborsOfPoint(object<Subpopulation>$ subpop, float point, [integer$ count = 1])
			//
			#pragma mark -nearestNeighborsOfPoint()
			
		case gID_nearestNeighborsOfPoint:
		{
			if (spatiality_ == 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): nearestNeighborsOfPoint() requires that the interaction be spatial." << eidos_terminate();
			
			// Check the subpop
			Subpopulation *subpop = (Subpopulation *)arg0_value->ObjectElementAtIndex(0, nullptr);
			slim_objectid_t subpop_id = subpop->subpopulation_id_;
			slim_popsize_t subpop_size = subpop->parent_subpop_size_;
			auto subpop_data_iter = data_.find(subpop_id);
			
			if (subpop_data_iter == data_.end())
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): nearestNeighborsOfPoint() requires that the interaction has been evaluated for the subpopulation first." << eidos_terminate();
			
			// Check the point
			if (arg1_value->Count() < spatiality_)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): nearestNeighborsOfPoint() requires a point vector with at least as many elements as the InteractionType spatiality." << eidos_terminate();
			
			double point_array[3];
			
			for (int point_index = 0; point_index < spatiality_; ++point_index)
				point_array[point_index] = arg1_value->FloatAtIndex(point_index, nullptr);
			
			// Check the count
			int64_t count = arg2_value->IntAtIndex(0, nullptr);
			
			if (count <= 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): nearestNeighborsOfPoint() requires count > 0." << eidos_terminate();
			
			if (count > subpop_size)
				count = subpop_size;
			
			// Find the neighbors
			InteractionsData &subpop_data = subpop_data_iter->second;
			
			EnsureKDTreePresent(subpop_data);
			
			EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->Reserve((int)count);
			std::vector<EidosObjectElement *> *result_direct = result_vec->ObjectElementVector_Mutable();
			
			FindNeighbors(subpop, subpop_data, point_array, (int)count, *result_direct, nullptr);
			
			return EidosValue_SP(result_vec);
		}
			
			
			//
			//	*********************	- (void)setInteractionFunction(string$ functionType, ...)
			//
			#pragma mark -setInteractionFunction()
			
		case gID_setInteractionFunction:
		{
			std::string if_type_string = arg0_value->StringAtIndex(0, nullptr);
			IFType if_type;
			int expected_if_param_count = 0;
			std::vector<double> if_parameters;
			std::vector<std::string> if_strings;
			
			if (AnyEvaluated())
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): setInteractionFunction() cannot be called while the interaction is being evaluated; call unevaluate() first, or call setInteractionFunction() prior to evaluation of the interaction." << eidos_terminate();
			
			if (if_type_string.compare(gStr_f) == 0)
			{
				if_type = IFType::kFixed;
				expected_if_param_count = 1;
			}
			else if (if_type_string.compare(gStr_l) == 0)
			{
				if_type = IFType::kLinear;
				expected_if_param_count = 1;
				
				if (isinf(max_distance_) || (max_distance_ <= 0.0))
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): interaction type 'l' cannot be set in setInteractionFunction() unless a finite maximum interaction distance greater than zero has been set." << eidos_terminate();
			}
			else if (if_type_string.compare(gStr_e) == 0)
			{
				if_type = IFType::kExponential;
				expected_if_param_count = 2;
			}
			else if (if_type_string.compare(gEidosStr_n) == 0)
			{
				if_type = IFType::kNormal;
				expected_if_param_count = 2;
			}
			else
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): setInteractionFunction() functionType \"" << if_type_string << "\" must be \"f\", \"l\", \"e\", or \"n\"." << eidos_terminate();
			
			if ((spatiality_ == 0) && (if_type != IFType::kFixed))
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): setInteractionFunction() requires functionType 'f' for non-spatial interactions." << eidos_terminate();
			
			if (p_argument_count != 1 + expected_if_param_count)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): setInteractionFunction() functionType \"" << if_type << "\" requires exactly " << expected_if_param_count << " DFE parameter" << (expected_if_param_count == 1 ? "" : "s") << "." << eidos_terminate();
			
			for (int if_param_index = 0; if_param_index < expected_if_param_count; ++if_param_index)
			{
				EidosValue *if_param_value = p_arguments[1 + if_param_index].get();
				EidosValueType if_param_type = if_param_value->Type();
				
				if ((if_param_type != EidosValueType::kValueFloat) && (if_param_type != EidosValueType::kValueInt))
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): setInteractionFunction() requires that the parameters for this IF be of type numeric (integer or float)." << eidos_terminate();
				
				if_parameters.emplace_back(if_param_value->FloatAtIndex(0, nullptr));
				// intentionally no bounds checks for IF parameters
			}
			
			// Everything seems to be in order, so replace our IF info with the new info
			if_type_ = if_type;
			if_param1_ = ((if_parameters.size() >= 1) ? if_parameters[0] : 0.0);
			if_param2_ = ((if_parameters.size() >= 2) ? if_parameters[1] : 0.0);
			
			return gStaticEidosValueNULLInvisible;
		}

			
			//
			//	*********************	– (float)strength(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
			//
			#pragma mark -strength()
			
		case gID_strength:
		{
			EidosValue *individuals1 = arg0_value, *individuals2 = arg1_value;
			int count1 = individuals1->Count(), count2 = individuals2->Count();
			
			if ((count1 != 1) && (count2 != 1))
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): strength() requires that either individuals1 or individuals2 be singleton." << eidos_terminate();
			
			// Rearrange so that if either vector is non-singleton, it is the second that is non-singleton (one-to-many)
			if (count1 != 1)
			{
				std::swap(individuals1, individuals2);
				std::swap(count1, count2);
			}
			
			// individuals1 is guaranteed to be singleton; let's get the info on it
			Individual *ind1 = (Individual *)individuals1->ObjectElementAtIndex(0, nullptr);
			Subpopulation *subpop1 = &(ind1->subpopulation_);
			slim_objectid_t subpop1_id = subpop1->subpopulation_id_;
			slim_popsize_t subpop1_size = subpop1->parent_subpop_size_;
			int ind1_index = ind1->index_;
			auto subpop_data_iter = data_.find(subpop1_id);
			
			if (subpop_data_iter == data_.end())
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): strength() requires that the interaction has been evaluated for the subpopulation first." << eidos_terminate();
			
			InteractionsData &subpop_data = subpop_data_iter->second;
			std::vector<SLiMEidosBlock*> &callbacks = subpop_data.evaluation_interaction_callbacks_;
			bool no_callbacks = (callbacks.size() == 0);
			
			EnsureStrengthsPresent(subpop_data);
			
			if (spatiality_)
			{
				//
				// Spatial case; distances used
				//
				
				double *ind1_strengths = subpop_data.strengths_ + ind1_index * subpop1_size;
				double *mirror_ind1_strengths = subpop_data.strengths_ + ind1_index;			// for reciprocality
				double *ind1_distances = subpop_data.distances_ + ind1_index * subpop1_size;
				double *mirror_ind1_distances = subpop_data.distances_ + ind1_index;			// for reciprocality
				double *position_data = subpop_data.positions_;
				double *ind1_position = position_data + ind1_index * SLIM_MAX_DIMENSIONALITY;
				
				if (individuals2->Type() == EidosValueType::kValueNULL)
				{
					// NULL means return strengths from individuals1 (which must be singleton) to all individuals in the subpopulation
					EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(subpop1_size);
					
					if (isinf(max_distance_))
					{
						// This is the brute-force approach – loop through the subpop, calculate distances and strengths for everyone.
						// If the interaction is non-local (max_distance_ of INF) then this makes sense; all this work will need to be done.
						if (!reciprocal_)
						{
							for (int ind2_index = 0; ind2_index < subpop1_size; ++ind2_index)
							{
								double strength = ind1_strengths[ind2_index];
								
								if (isnan(strength))
								{
									double distance = ind1_distances[ind2_index];
									
									if (isnan(distance))
									{
										distance = CalculateDistance(ind1_position, position_data + ind2_index * SLIM_MAX_DIMENSIONALITY);
										ind1_distances[ind2_index] = distance;
									}
									
									if (distance <= max_distance_)
									{
										if (no_callbacks)
											strength = CalculateStrengthNoCallbacks(distance);
										else
											strength = CalculateStrengthWithCallbacks(distance, ind1, &subpop1->parent_individuals_[ind2_index], subpop1, callbacks);
									}
									else
									{
										strength = 0.0;
									}
									
									ind1_strengths[ind2_index] = strength;
								}
								
								result_vec->PushFloat(strength);
							}
						}
						else
						{
							for (int ind2_index = 0; ind2_index < subpop1_size; ++ind2_index, mirror_ind1_distances += subpop1_size, mirror_ind1_strengths += subpop1_size)
							{
								double strength = ind1_strengths[ind2_index];
								
								if (isnan(strength))
								{
									double distance = ind1_distances[ind2_index];
									
									if (isnan(distance))
									{
										distance = CalculateDistance(ind1_position, position_data + ind2_index * SLIM_MAX_DIMENSIONALITY);
										ind1_distances[ind2_index] = distance;
										*(mirror_ind1_distances) = distance;
									}
									
									if (distance <= max_distance_)
									{
										if (no_callbacks)
											strength = CalculateStrengthNoCallbacks(distance);
										else
											strength = CalculateStrengthWithCallbacks(distance, ind1, &subpop1->parent_individuals_[ind2_index], subpop1, callbacks);
									}
									else
									{
										strength = 0.0;
									}
									
									ind1_strengths[ind2_index] = strength;
									*(mirror_ind1_strengths) = strength;
								}
								
								result_vec->PushFloat(strength);
							}
						}
					}
					else
					{
						// If the interaction is local, this approach should be much more efficient: allocate a results vector,
						// fill it with zeros as the default value, and then find all neighbors and fill in their strengths.
						// Zeroing out the vector is still O(N), but the constant is much smaller, and the rest is then < O(N).
						// In point of fact, performance measurements indicate that with wide (but non-INF) interactions, this
						// k-d tree case can be somewhat (~20%) slower than the brute-force method above, because all of the
						// interactions need to be calculated anyway, and here we add the k-d tree construction and traversal
						// overhead, plus making twice as many memory writes (one for zeros, one for values).  But the difference
						// is not large, so I'm not too worried.  The big goal is to make it so that simulations involving large
						// spatial areas with large numbers of individuals and highly localized interactions perform as well as
						// possible, and this k-d tree case ensures that.
						std::vector<double> &result_cpp_vec = *result_vec->FloatVector_Mutable();
						
						result_cpp_vec.resize(subpop1_size);	// this value-initializes, so it zero-fills
						
						EnsureKDTreePresent(subpop_data);
						
						FillNeighborStrengths(subpop1, subpop_data, ind1_position, ind1, result_cpp_vec);
					}
					
					return EidosValue_SP(result_vec);
				}
				else
				{
					// Otherwise, individuals1 is singleton, and individuals2 is any length, so we loop over individuals2
					EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(count2);
					
					for (int ind2_index = 0; ind2_index < count2; ++ind2_index)
					{
						Individual *ind2 = (Individual *)individuals2->ObjectElementAtIndex(ind2_index, nullptr);
						
						if (subpop1 != &(ind2->subpopulation_))
							EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): strength() requires that all individuals be in the same subpopulation." << eidos_terminate();
						
						slim_popsize_t ind2_index_in_subpop = ind2->index_;
						double strength = ind1_strengths[ind2_index_in_subpop];
						
						if (isnan(strength))
						{
							double distance = ind1_distances[ind2_index_in_subpop];
							
							if (isnan(distance))
							{
								distance = CalculateDistance(ind1_position, position_data + ind2_index_in_subpop * SLIM_MAX_DIMENSIONALITY);
								ind1_distances[ind2_index_in_subpop] = distance;
								
								if (reciprocal_)
									*(mirror_ind1_distances + ind2_index_in_subpop * subpop1_size) = distance;
							}
							
							if (distance <= max_distance_)
							{
								if (no_callbacks)
									strength = CalculateStrengthNoCallbacks(distance);
								else
									strength = CalculateStrengthWithCallbacks(distance, ind1, ind2, subpop1, callbacks);
							}
							else
							{
								strength = 0.0;
							}
							
							ind1_strengths[ind2_index_in_subpop] = strength;
							
							if (reciprocal_)
								*(mirror_ind1_strengths + ind2_index_in_subpop * subpop1_size) = strength;
						}
						
						result_vec->PushFloat(strength);
					}
					
					return EidosValue_SP(result_vec);
				}
			}
			else
			{
				//
				// Non-spatial case; no distances used
				//
				
				double *ind1_strengths = subpop_data.strengths_ + ind1_index * subpop1_size;
				double *mirror_ind1_strengths = subpop_data.strengths_ + ind1_index;			// for reciprocality
				
				if (individuals2->Type() == EidosValueType::kValueNULL)
				{
					// NULL means return strengths from individuals1 (which must be singleton) to all individuals in the subpopulation
					EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(subpop1_size);
					
					if (!reciprocal_)
					{
						for (int ind2_index = 0; ind2_index < subpop1_size; ++ind2_index)
						{
							double strength = ind1_strengths[ind2_index];
							
							if (isnan(strength))
							{
								if (no_callbacks)
									strength = CalculateStrengthNoCallbacks(NAN);
								else
									strength = CalculateStrengthWithCallbacks(NAN, ind1, &subpop1->parent_individuals_[ind2_index], subpop1, callbacks);
								
								ind1_strengths[ind2_index] = strength;
							}
							
							result_vec->PushFloat(strength);
						}
					}
					else
					{
						for (int ind2_index = 0; ind2_index < subpop1_size; ++ind2_index, mirror_ind1_strengths += subpop1_size)
						{
							double strength = ind1_strengths[ind2_index];
							
							if (isnan(strength))
							{
								if (no_callbacks)
									strength = CalculateStrengthNoCallbacks(NAN);
								else
									strength = CalculateStrengthWithCallbacks(NAN, ind1, &subpop1->parent_individuals_[ind2_index], subpop1, callbacks);
								
								ind1_strengths[ind2_index] = strength;
								*(mirror_ind1_strengths) = strength;
							}
							
							result_vec->PushFloat(strength);
						}
					}
					
					return EidosValue_SP(result_vec);
				}
				else
				{
					// Otherwise, individuals1 is singleton, and individuals2 is any length, so we loop over individuals2
					EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(count2);
					
					for (int ind2_index = 0; ind2_index < count2; ++ind2_index)
					{
						Individual *ind2 = (Individual *)individuals2->ObjectElementAtIndex(ind2_index, nullptr);
						
						if (subpop1 != &(ind2->subpopulation_))
							EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): strength() requires that all individuals be in the same subpopulation." << eidos_terminate();
						
						slim_popsize_t ind2_index_in_subpop = ind2->index_;
						double strength = ind1_strengths[ind2_index_in_subpop];
						
						if (isnan(strength))
						{
							if (no_callbacks)
								strength = CalculateStrengthNoCallbacks(NAN);
							else
								strength = CalculateStrengthWithCallbacks(NAN, ind1, ind2, subpop1, callbacks);
							
							ind1_strengths[ind2_index_in_subpop] = strength;
							
							if (reciprocal_)
								*(mirror_ind1_strengths + ind2_index_in_subpop * subpop1_size) = strength;
						}
						
						result_vec->PushFloat(strength);
					}
					
					return EidosValue_SP(result_vec);
				}
			}
		}
			
			
			//
			//	*********************	– (float)totalOfNeighborStrengths(object<Individual> individuals)
			//
			#pragma mark -totalOfNeighborStrengths()
			
		case gID_totalOfNeighborStrengths:
		{
			if (spatiality_ == 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): totalOfNeighborStrengths() requires that the interaction be spatial." << eidos_terminate();
			
			EidosValue *individuals = arg0_value;
			int count = individuals->Count();
			
			if (count == 0)
				return gStaticEidosValue_Float_ZeroVec;
			
			// individuals is guaranteed to have at least one value
			Individual *first_ind = (Individual *)individuals->ObjectElementAtIndex(0, nullptr);
			Subpopulation *subpop = &(first_ind->subpopulation_);
			slim_objectid_t subpop_id = subpop->subpopulation_id_;
			auto subpop_data_iter = data_.find(subpop_id);
			
			if (subpop_data_iter == data_.end())
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): totalOfNeighborStrengths() requires that the interaction has been evaluated for the subpopulation first." << eidos_terminate();
			
			InteractionsData &subpop_data = subpop_data_iter->second;
			
			EnsureStrengthsPresent(subpop_data);
			EnsureKDTreePresent(subpop_data);
			
			// Loop over the requested individuals and get the totals
			EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->Reserve(count);
			
			for (int ind_index = 0; ind_index < count; ++ind_index)
			{
				Individual *individual = (Individual *)individuals->ObjectElementAtIndex(ind_index, nullptr);
				
				if (subpop != &(individual->subpopulation_))
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteInstanceMethod): totalOfNeighborStrengths() requires that all individuals be in the same subpopulation." << eidos_terminate();
				
				slim_popsize_t ind_index_in_subpop = individual->index_;
				double *position_data = subpop_data.positions_;
				double *ind_position = position_data + ind_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				
				// use the k-d tree to find neighbors, and total their strengths
				double total_strength = TotalNeighborStrength(subpop, subpop_data, ind_position, individual);
				
				result_vec->PushFloat(total_strength);
			}
			
			return EidosValue_SP(result_vec);
		}
			
			
			//
			//	*********************	– (float)unevaluate(void)
			//
			#pragma mark -unevaluate()
			
		case gID_unevaluate:
		{
			Invalidate();
			
			return gStaticEidosValueNULLInvisible;
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::ExecuteInstanceMethod(p_method_id, p_arguments, p_argument_count, p_interpreter);
	}
}


//
//	InteractionType_Class
//
#pragma mark -
#pragma mark InteractionType_Class

class InteractionType_Class : public EidosObjectClass
{
public:
	InteractionType_Class(const InteractionType_Class &p_original) = delete;	// no copy-construct
	InteractionType_Class& operator=(const InteractionType_Class&) = delete;	// no copying
	
	InteractionType_Class(void);
	
	virtual const std::string &ElementType(void) const;
	
	virtual const std::vector<const EidosPropertySignature *> *Properties(void) const;
	virtual const EidosPropertySignature *SignatureForProperty(EidosGlobalStringID p_property_id) const;
	
	virtual const std::vector<const EidosMethodSignature *> *Methods(void) const;
	virtual const EidosMethodSignature *SignatureForMethod(EidosGlobalStringID p_method_id) const;
	virtual EidosValue_SP ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const;
};

EidosObjectClass *gSLiM_InteractionType_Class = new InteractionType_Class();


InteractionType_Class::InteractionType_Class(void)
{
}

const std::string &InteractionType_Class::ElementType(void) const
{
	return gStr_InteractionType;
}

const std::vector<const EidosPropertySignature *> *InteractionType_Class::Properties(void) const
{
	static std::vector<const EidosPropertySignature *> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<const EidosPropertySignature *>(*EidosObjectClass::Properties());
		properties->emplace_back(SignatureForPropertyOrRaise(gID_id));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_reciprocal));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_sexSegregation));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_spatiality));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_maxDistance));
		properties->emplace_back(SignatureForPropertyOrRaise(gID_tag));
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const EidosPropertySignature *InteractionType_Class::SignatureForProperty(EidosGlobalStringID p_property_id) const
{
	// Signatures are all preallocated, for speed
	static EidosPropertySignature *idSig = nullptr;
	static EidosPropertySignature *reciprocalSig = nullptr;
	static EidosPropertySignature *sexSegregationSig = nullptr;
	static EidosPropertySignature *spatialitySig = nullptr;
	static EidosPropertySignature *maxDistanceSig = nullptr;
	static EidosPropertySignature *tagSig = nullptr;
	
	if (!idSig)
	{
		idSig =				(EidosPropertySignature *)(new EidosPropertySignature(gStr_id,				gID_id,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
		reciprocalSig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_reciprocal,		gID_reciprocal,		true,	kEidosValueMaskLogical | kEidosValueMaskSingleton));
		sexSegregationSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_sexSegregation,	gID_sexSegregation,	true,	kEidosValueMaskString | kEidosValueMaskSingleton));
		spatialitySig =		(EidosPropertySignature *)(new EidosPropertySignature(gStr_spatiality,		gID_spatiality,		true,	kEidosValueMaskString | kEidosValueMaskSingleton));
		maxDistanceSig =	(EidosPropertySignature *)(new EidosPropertySignature(gStr_maxDistance,		gID_maxDistance,	false,	kEidosValueMaskFloat | kEidosValueMaskSingleton));
		tagSig =			(EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,				gID_tag,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet();
	}
	
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_id:				return idSig;
		case gID_reciprocal:		return reciprocalSig;
		case gID_sexSegregation:	return sexSegregationSig;
		case gID_spatiality:		return spatialitySig;
		case gID_maxDistance:		return maxDistanceSig;
		case gID_tag:				return tagSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForProperty(p_property_id);
	}
}

const std::vector<const EidosMethodSignature *> *InteractionType_Class::Methods(void) const
{
	static std::vector<const EidosMethodSignature *> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<const EidosMethodSignature *>(*EidosObjectClass::Methods());
		methods->emplace_back(SignatureForMethodOrRaise(gID_distance));
		methods->emplace_back(SignatureForMethodOrRaise(gID_distanceToPoint));
		methods->emplace_back(SignatureForMethodOrRaise(gID_drawByStrength));
		methods->emplace_back(SignatureForMethodOrRaise(gID_evaluate));
		methods->emplace_back(SignatureForMethodOrRaise(gID_nearestNeighbors));
		methods->emplace_back(SignatureForMethodOrRaise(gID_nearestNeighborsOfPoint));
		methods->emplace_back(SignatureForMethodOrRaise(gID_setInteractionFunction));
		methods->emplace_back(SignatureForMethodOrRaise(gID_strength));
		methods->emplace_back(SignatureForMethodOrRaise(gID_totalOfNeighborStrengths));
		methods->emplace_back(SignatureForMethodOrRaise(gID_unevaluate));
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}

const EidosMethodSignature *InteractionType_Class::SignatureForMethod(EidosGlobalStringID p_method_id) const
{
	static EidosInstanceMethodSignature *distanceSig = nullptr;
	static EidosInstanceMethodSignature *distanceToPointSig = nullptr;
	static EidosInstanceMethodSignature *drawByStrengthSig = nullptr;
	static EidosInstanceMethodSignature *evaluateSig = nullptr;
	static EidosInstanceMethodSignature *nearestNeighborsSig = nullptr;
	static EidosInstanceMethodSignature *nearestNeighborsOfPointSig = nullptr;
	static EidosInstanceMethodSignature *setInteractionFunctionSig = nullptr;
	static EidosInstanceMethodSignature *strengthSig = nullptr;
	static EidosInstanceMethodSignature *totalOfNeighborStrengthsSig = nullptr;
	static EidosInstanceMethodSignature *unevaluateSig = nullptr;
	
	if (!evaluateSig)
	{
		distanceSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_distance, kEidosValueMaskFloat))->AddObject("individuals1", gSLiM_Individual_Class)->AddObject_ON("individuals2", gSLiM_Individual_Class, gStaticEidosValueNULL);
		distanceToPointSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_distanceToPoint, kEidosValueMaskFloat))->AddObject("individuals1", gSLiM_Individual_Class)->AddFloat("point");
		drawByStrengthSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_drawByStrength, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("individual", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1);
		evaluateSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_evaluate, kEidosValueMaskNULL))->AddObject_ON("subpops", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddLogical_OS("immediate", gStaticEidosValue_LogicalF);
		nearestNeighborsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_nearestNeighbors, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("individual", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1);
		nearestNeighborsOfPointSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_nearestNeighborsOfPoint, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("subpop", gSLiM_Subpopulation_Class)->AddFloat("point")->AddInt_OS("count", gStaticEidosValue_Integer1);
		setInteractionFunctionSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setInteractionFunction, kEidosValueMaskNULL))->AddString_S("functionType")->AddEllipsis();
		strengthSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_strength, kEidosValueMaskFloat))->AddObject("individuals1", gSLiM_Individual_Class)->AddObject_ON("individuals2", gSLiM_Individual_Class, gStaticEidosValueNULL);
		totalOfNeighborStrengthsSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_totalOfNeighborStrengths, kEidosValueMaskFloat))->AddObject("individuals", gSLiM_Individual_Class);
		unevaluateSig = (EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_unevaluate, kEidosValueMaskNULL));
	}
	
	switch (p_method_id)
	{
		case gID_distance:					return distanceSig;
		case gID_distanceToPoint:			return distanceToPointSig;
		case gID_drawByStrength:			return drawByStrengthSig;
		case gID_evaluate:					return evaluateSig;
		case gID_nearestNeighbors:			return nearestNeighborsSig;
		case gID_nearestNeighborsOfPoint:	return nearestNeighborsOfPointSig;
		case gID_setInteractionFunction:	return setInteractionFunctionSig;
		case gID_strength:					return strengthSig;
		case gID_totalOfNeighborStrengths:	return totalOfNeighborStrengthsSig;
		case gID_unevaluate:				return unevaluateSig;
			
			// all others, including gID_none
		default:
			return EidosObjectClass::SignatureForMethod(p_method_id);
	}
}

EidosValue_SP InteractionType_Class::ExecuteClassMethod(EidosGlobalStringID p_method_id, EidosValue_Object *p_target, const EidosValue_SP *const p_arguments, int p_argument_count, EidosInterpreter &p_interpreter) const
{
	return EidosObjectClass::ExecuteClassMethod(p_method_id, p_target, p_arguments, p_argument_count, p_interpreter);
}


//
//	InteractionType_Class
//
#pragma mark -
#pragma mark InteractionType_Class

_InteractionsData::_InteractionsData(_InteractionsData&& source)
{
	evaluated_ = source.evaluated_;
	evaluation_interaction_callbacks_.swap(source.evaluation_interaction_callbacks_);
	individual_count_ = source.individual_count_;
	first_male_index_ = source.first_male_index_;
	positions_ = source.positions_;
	distances_ = source.distances_;
	strengths_ = source.strengths_;
	kd_nodes_ = source.kd_nodes_;
	kd_root_ = source.kd_root_;
	
	source.evaluated_ = false;
	source.evaluation_interaction_callbacks_.clear();
	source.individual_count_ = 0;
	source.first_male_index_ = 0;
	source.positions_ = nullptr;
	source.distances_ = nullptr;
	source.strengths_ = nullptr;
	source.kd_nodes_ = nullptr;
	source.kd_root_ = nullptr;
}

_InteractionsData& _InteractionsData::operator=(_InteractionsData&& source)
{
	if (this != &source)  
	{
		if (positions_)
			free(positions_);
		if (distances_)
			free(distances_);
		if (strengths_)
			free(strengths_);
		if (kd_nodes_)
			free(kd_nodes_);
		
		evaluated_ = source.evaluated_;
		evaluation_interaction_callbacks_.swap(source.evaluation_interaction_callbacks_);
		individual_count_ = source.individual_count_;
		first_male_index_ = source.first_male_index_;
		positions_ = source.positions_;
		distances_ = source.distances_;
		strengths_ = source.strengths_;
		kd_nodes_ = source.kd_nodes_;
		kd_root_ = source.kd_root_;
		
		source.evaluated_ = false;
		source.evaluation_interaction_callbacks_.clear();
		source.individual_count_ = 0;
		source.first_male_index_ = 0;
		source.positions_ = nullptr;
		source.distances_ = nullptr;
		source.strengths_ = nullptr;
		source.kd_nodes_ = nullptr;
		source.kd_root_ = nullptr;
	}
	
	return *this;
}

_InteractionsData::_InteractionsData(void)
{
}

_InteractionsData::_InteractionsData(slim_popsize_t p_individual_count, slim_popsize_t p_first_male_index) : individual_count_(p_individual_count), first_male_index_(p_first_male_index)
{
}

_InteractionsData::~_InteractionsData(void)
{
	if (positions_)
	{
		free(positions_);
		positions_ = nullptr;
	}
	
	if (distances_)
	{
		free(distances_);
		distances_ = nullptr;
	}
	
	if (strengths_)
	{
		free(strengths_);
		strengths_ = nullptr;
	}
	
	if (kd_nodes_)
	{
		free(kd_nodes_);
		kd_nodes_ = nullptr;
	}
	
	kd_root_ = nullptr;
	
	// Unnecessary since it's about to be destroyed anyway
	//evaluation_interaction_callbacks_.clear();
}































