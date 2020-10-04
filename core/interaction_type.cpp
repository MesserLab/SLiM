//
//  interaction_type.cpp
//  SLiM
//
//  Created by Ben Haller on 2/25/17.
//  Copyright (c) 2017-2020 Philipp Messer.  All rights reserved.
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
#include <cmath>


// stream output for enumerations
std::ostream& operator<<(std::ostream& p_out, IFType p_if_type)
{
	switch (p_if_type)
	{
		case IFType::kFixed:			p_out << gStr_f;		break;
		case IFType::kLinear:			p_out << gStr_l;		break;
		case IFType::kExponential:		p_out << gStr_e;		break;
		case IFType::kNormal:			p_out << gEidosStr_n;	break;
		case IFType::kCauchy:			p_out << gEidosStr_c;	break;
	}
	
	return p_out;
}


#pragma mark -
#pragma mark InteractionType
#pragma mark -

InteractionType::InteractionType(SLiMSim &p_sim, slim_objectid_t p_interaction_type_id, std::string p_spatiality_string, bool p_reciprocal, double p_max_distance, IndividualSex p_receiver_sex, IndividualSex p_exerter_sex) :
	sim_(p_sim),
	self_symbol_(Eidos_GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('i', p_interaction_type_id)),
			 EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_InteractionType_Class))),
	spatiality_string_(p_spatiality_string), reciprocal_(p_reciprocal), max_distance_(p_max_distance), max_distance_sq_(p_max_distance * p_max_distance), receiver_sex_(p_receiver_sex), exerter_sex_(p_exerter_sex), if_type_(IFType::kFixed), if_param1_(1.0), if_param2_(0.0), interaction_type_id_(p_interaction_type_id)
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
		EIDOS_TERMINATION << "ERROR (InteractionType::InteractionType): illegal spatiality string value" << EidosTerminate();
}

InteractionType::~InteractionType(void)
{
}

void InteractionType::EvaluateSubpopulation(Subpopulation *p_subpop, bool p_immediate)
{
	SLiMSim &sim = p_subpop->population_.sim_;
	slim_objectid_t subpop_id = p_subpop->subpopulation_id_;
	slim_popsize_t subpop_size = p_subpop->parent_subpop_size_;
	Individual **subpop_individuals = p_subpop->parent_individuals_.data();
	
	auto data_iter = data_.find(subpop_id);
	InteractionsData *subpop_data;
	
	if (data_iter == data_.end())
	{
		// No entry in our map table for this subpop_id, so we need to make a new entry
		subpop_data = &(data_.insert(std::pair<slim_objectid_t, InteractionsData>(subpop_id, InteractionsData(subpop_size, p_subpop->parent_first_male_index_))).first->second);
	}
	else
	{
		// There is an existing entry, so we need to rehabilitate that entry by recycling its elements safely
		subpop_data = &(data_iter->second);
		
		subpop_data->individual_count_ = subpop_size;
		subpop_data->first_male_index_ = p_subpop->parent_first_male_index_;
		subpop_data->kd_node_count_ = 0;
		
		// If the sparse array has not yet been allocated, we will continue to defer until it is needed
		// It will never be allocated for non-spatial models, or for models that use only the k-d tree
		if (subpop_data->dist_str_)
			subpop_data->dist_str_->Reset();
		
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
	
	// At this point, positions_ is guaranteed to be nullptr; dist_str_ is either (1) nullptr,
	// or (2) allocated but empty.  Now we mark ourselves evaluated and fill in buffers as needed.
	subpop_data->evaluated_ = true;
	subpop_data->distances_calculated_ = false;
	subpop_data->strengths_calculated_ = false;
	
	// At a minimum, fetch positional data from the subpopulation; this is guaranteed to be present (for spatiality > 0)
	if (spatiality_ > 0)
	{
		double *positions = (double *)malloc(subpop_size * SLIM_MAX_DIMENSIONALITY * sizeof(double));
		
		subpop_data->positions_ = positions;
		
		int ind_index = 0;
		Individual **individual = subpop_individuals;
		double *ind_positions = positions;
		
		// IMPORTANT: This is the only place in InteractionType's code where the spatial position of the individuals is
		// accessed.  We cache all positions here, and then use the cache everywhere else.  This means that except for
		// here, we can treat "x", "y", and "z" identically as 1D spatiality, "xy", "xz", and "yz" identically as 2D
		// spatiality, and of course "xyz" as 3D spatiality.  We do that by storing the cached position information in
		// the same slots regardless of which original coordinates it represents.  This also means that this is the
		// only place in the code where spatiality_string_ should be used (apart from the property accessor); everywhere
		// else, spatiality_ should suffice.  Be careful to keep following this convention, and the different spatiality
		// values will just automatically work.
		bool out_of_bounds_seen = false;
		
		if (spatiality_string_ == "x")
		{
			sim.SpatialPeriodicity(&periodic_x_, nullptr, nullptr);
			subpop_data->bounds_x1_ = p_subpop->bounds_x1_;
			
			if (!periodic_x_)
			{
				// fast loop for the non-periodic case
				while (ind_index < subpop_size)
				{
					ind_positions[0] = (*individual)->spatial_x_;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
			else
			{
				// bounds-check individual coordinates when periodic
				double coord_bound = subpop_data->bounds_x1_;
				
				while (ind_index < subpop_size)
				{
					double coord = (*individual)->spatial_x_;
					
					if ((coord < 0.0) || (coord > coord_bound))
						out_of_bounds_seen = true;
					
					ind_positions[0] = coord;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
		}
		else if (spatiality_string_ == "y")
		{
			sim.SpatialPeriodicity(nullptr, &periodic_x_, nullptr);
			subpop_data->bounds_x1_ = p_subpop->bounds_y1_;
			
			if (!periodic_x_)
			{
				// fast loop for the non-periodic case
				while (ind_index < subpop_size)
				{
					ind_positions[0] = (*individual)->spatial_y_;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
			else
			{
				// bounds-check individual coordinates when periodic
				double coord_bound = subpop_data->bounds_x1_;
				
				while (ind_index < subpop_size)
				{
					double coord = (*individual)->spatial_y_;
					
					if ((coord < 0.0) || (coord > coord_bound))
						out_of_bounds_seen = true;
					
					ind_positions[0] = coord;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
		}
		else if (spatiality_string_ == "z")
		{
			sim.SpatialPeriodicity(nullptr, nullptr, &periodic_x_);
			subpop_data->bounds_x1_ = p_subpop->bounds_z1_;
			
			if (!periodic_x_)
			{
				// fast loop for the non-periodic case
				while (ind_index < subpop_size)
				{
					ind_positions[0] = (*individual)->spatial_z_;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
			else
			{
				// bounds-check individual coordinates when periodic
				double coord_bound = subpop_data->bounds_x1_;
				
				while (ind_index < subpop_size)
				{
					double coord = (*individual)->spatial_z_;
					
					if ((coord < 0.0) || (coord > coord_bound))
						out_of_bounds_seen = true;
					
					ind_positions[0] = coord;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
		}
		else if (spatiality_string_ == "xy")
		{
			sim.SpatialPeriodicity(&periodic_x_, &periodic_y_, nullptr);
			subpop_data->bounds_x1_ = p_subpop->bounds_x1_;
			subpop_data->bounds_y1_ = p_subpop->bounds_y1_;
			
			if (!periodic_x_ && !periodic_y_)
			{
				// fast loop for the non-periodic case
				while (ind_index < subpop_size)
				{
					ind_positions[0] = (*individual)->spatial_x_;
					ind_positions[1] = (*individual)->spatial_y_;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
			else
			{
				// bounds-check individual coordinates when periodic
				double coord1_bound = subpop_data->bounds_x1_;
				double coord2_bound = subpop_data->bounds_y1_;
				
				while (ind_index < subpop_size)
				{
					double coord1 = (*individual)->spatial_x_;
					double coord2 = (*individual)->spatial_y_;
					
					if ((periodic_x_ && ((coord1 < 0.0) || (coord1 > coord1_bound))) ||
						(periodic_y_ && ((coord2 < 0.0) || (coord2 > coord2_bound))))
						out_of_bounds_seen = true;
					
					ind_positions[0] = coord1;
					ind_positions[1] = coord2;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
		}
		else if (spatiality_string_ == "xz")
		{
			sim.SpatialPeriodicity(&periodic_x_, nullptr, &periodic_y_);
			subpop_data->bounds_x1_ = p_subpop->bounds_x1_;
			subpop_data->bounds_y1_ = p_subpop->bounds_z1_;
			
			if (!periodic_x_ && !periodic_y_)
			{
				// fast loop for the non-periodic case
				while (ind_index < subpop_size)
				{
					ind_positions[0] = (*individual)->spatial_x_;
					ind_positions[1] = (*individual)->spatial_z_;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
			else
			{
				// bounds-check individual coordinates when periodic
				double coord1_bound = subpop_data->bounds_x1_;
				double coord2_bound = subpop_data->bounds_y1_;
				
				while (ind_index < subpop_size)
				{
					double coord1 = (*individual)->spatial_x_;
					double coord2 = (*individual)->spatial_z_;
					
					if ((periodic_x_ && ((coord1 < 0.0) || (coord1 > coord1_bound))) ||
						(periodic_y_ && ((coord2 < 0.0) || (coord2 > coord2_bound))))
						out_of_bounds_seen = true;
					
					ind_positions[0] = coord1;
					ind_positions[1] = coord2;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
		}
		else if (spatiality_string_ == "yz")
		{
			sim.SpatialPeriodicity(nullptr, &periodic_x_, &periodic_y_);
			subpop_data->bounds_x1_ = p_subpop->bounds_y1_;
			subpop_data->bounds_y1_ = p_subpop->bounds_z1_;
			
			if (!periodic_x_ && !periodic_y_)
			{
				// fast loop for the non-periodic case
				while (ind_index < subpop_size)
				{
					ind_positions[0] = (*individual)->spatial_y_;
					ind_positions[1] = (*individual)->spatial_z_;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
			else
			{
				// bounds-check individual coordinates when periodic
				double coord1_bound = subpop_data->bounds_x1_;
				double coord2_bound = subpop_data->bounds_y1_;
				
				while (ind_index < subpop_size)
				{
					double coord1 = (*individual)->spatial_y_;
					double coord2 = (*individual)->spatial_z_;
					
					if ((periodic_x_ && ((coord1 < 0.0) || (coord1 > coord1_bound))) ||
						(periodic_y_ && ((coord2 < 0.0) || (coord2 > coord2_bound))))
						out_of_bounds_seen = true;
					
					ind_positions[0] = coord1;
					ind_positions[1] = coord2;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
		}
		else if (spatiality_string_ == "xyz")
		{
			sim.SpatialPeriodicity(&periodic_x_, &periodic_y_, &periodic_z_);
			subpop_data->bounds_x1_ = p_subpop->bounds_x1_;
			subpop_data->bounds_y1_ = p_subpop->bounds_y1_;
			subpop_data->bounds_z1_ = p_subpop->bounds_z1_;
			
			if (!periodic_x_ && !periodic_y_ && !periodic_z_)
			{
				// fast loop for the non-periodic case
				while (ind_index < subpop_size)
				{
					ind_positions[0] = (*individual)->spatial_x_;
					ind_positions[1] = (*individual)->spatial_y_;
					ind_positions[2] = (*individual)->spatial_z_;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
			else
			{
				// bounds-check individual coordinates when periodic
				double coord1_bound = subpop_data->bounds_x1_;
				double coord2_bound = subpop_data->bounds_y1_;
				double coord3_bound = subpop_data->bounds_z1_;
				
				while (ind_index < subpop_size)
				{
					double coord1 = (*individual)->spatial_x_;
					double coord2 = (*individual)->spatial_y_;
					double coord3 = (*individual)->spatial_z_;
					
					if ((periodic_x_ && ((coord1 < 0.0) || (coord1 > coord1_bound))) ||
						(periodic_y_ && ((coord2 < 0.0) || (coord2 > coord2_bound))) ||
						(periodic_z_ && ((coord3 < 0.0) || (coord3 > coord3_bound))))
						out_of_bounds_seen = true;
					
					ind_positions[0] = coord1;
					ind_positions[1] = coord2;
					ind_positions[2] = coord3;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
		}
		else
		{
			EIDOS_TERMINATION << "ERROR (InteractionType::EvaluateSubpopulation): (internal error) illegal spatiality string value" << EidosTerminate();
		}
		
		if (out_of_bounds_seen)
			EIDOS_TERMINATION << "ERROR (InteractionType::EvaluateSubpopulation): an individual position was seen that is out of bounds for a periodic spatial dimension; positions within periodic bounds are required by InteractionType since the underlying spatial engine's integrity depends upon them.  The use of pointPeriodic() is recommended to enforce periodic boundaries." << EidosTerminate();
	}
	
	// Check that our maximum interactions distance does not violate the assumptions of periodic boundaries;
	// an individual cannot interact with the same individual more than once, through wrapping around.
	if ((periodic_x_ && (subpop_data->bounds_x1_ <= max_distance_ * 2.0)) ||
		(periodic_y_ && (subpop_data->bounds_y1_ <= max_distance_ * 2.0)) ||
		(periodic_z_ && (subpop_data->bounds_z1_ <= max_distance_ * 2.0)))
		EIDOS_TERMINATION << "ERROR (InteractionType::EvaluateSubpopulation): maximum interaction distance is greater than or equal to half of the spatial extent of a periodic spatial dimension, which would allow an individual to participate in more than one interaction with a single individual.  When periodic boundaries are used, the maximum interaction distance of interaction types involving periodic dimensions must be less than half of the spatial extent of those dimensions." << EidosTerminate();
	
	// Cache the interaction() callbacks applicable at this moment, for this subpopulation and this interaction type
	slim_generation_t generation = sim.Generation();
	
	subpop_data->evaluation_interaction_callbacks_ = sim.ScriptBlocksMatching(generation, SLiMEidosBlockType::SLiMEidosInteractionCallback, -1, interaction_type_id_, subpop_id);
	
	// We have an "immediate" flag that defaults to F; interactions are evaluated lazily by default.  Since the
	// dist_str_ sparse array needs to be set up in a sequential fashion, we have to evaluate all interactions
	// as a single bulk operation.  However, the distances will not be evaluated until needed, and likewise for
	// the strengths; we can at least defer those operations until they are actually called for.  This way if a
	// model doesn't use distances or strengths at all (just asking for neighbors using the k-d tree, for example),
	// the interaction calculation overhead will be avoided.
	if (p_immediate)
	{
		// We could fill strengths in simultaneously with distances, for simple cases at least (no callbacks, simple
		// interaction function, etc.), but it probably isn't worth the additional code complexity; filling strengths is
		// generally fast anyway, so even if this eliminated all strength-fill overhead it wouldn't make much difference.
		CalculateAllDistances(p_subpop);
		CalculateAllStrengths(p_subpop);
	}
	else
	{
		// distances_calculated_ and strengths_calculated_ remain false here; the sparse array will be constructed on demand
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
		data.distances_calculated_ = false;
		data.strengths_calculated_ = false;
		
		if (data.positions_)
		{
			free(data.positions_);
			data.positions_ = nullptr;
		}
		
		if (data.dist_str_)
			data.dist_str_->Reset();
		
		if (data.kd_nodes_)
		{
			free(data.kd_nodes_);
			data.kd_nodes_ = nullptr;
		}
		
		data.kd_root_ = nullptr;
		
		data.evaluation_interaction_callbacks_.clear();
	}
}

void InteractionType::CalculateAllDistances(Subpopulation *p_subpop)
{
	slim_objectid_t subpop_id = p_subpop->subpopulation_id_;
	InteractionsData &subpop_data = data_[subpop_id];
	
	if (!subpop_data.distances_calculated_)
	{
		if (!subpop_data.evaluated_)
			EIDOS_TERMINATION << "ERROR (InteractionType::CalculateAllDistances): interaction has not yet been evaluated." << EidosTerminate();
		
		if (spatiality_ > 0)
		{
			// Here we use the k-d tree to find all interacting pairs, and calculate their distances.
			// This does not use reciprocality at all, but I don't think there's a good way to do so, so that's OK.
			EnsureKDTreePresent(subpop_data);
			
			slim_popsize_t subpop_size = p_subpop->parent_subpop_size_;
			
			if (subpop_data.dist_str_)
				subpop_data.dist_str_->Reset(subpop_size, subpop_size);
			else
				subpop_data.dist_str_ = new SparseArray(subpop_size, subpop_size);
			
			double *position_data = subpop_data.positions_;
			int start_row = 0, after_end_row = subpop_size, row;
			
			if (receiver_sex_ == IndividualSex::kUnspecified)
				;
			else if (receiver_sex_ == IndividualSex::kMale)
				start_row = subpop_data.first_male_index_;
			else if (receiver_sex_ == IndividualSex::kFemale)
				after_end_row = subpop_data.first_male_index_;
			else
				EIDOS_TERMINATION << "ERROR (InteractionType::CalculateAllDistances): (internal error) unrecognized value for receiver_sex_." << EidosTerminate();
			
			if (exerter_sex_ == IndividualSex::kUnspecified)
			{
				// Without a specified exerter sex, we can add each exerter with no sex test
				switch (spatiality_)
				{
					case 1:
						for (row = start_row; row < after_end_row; row++)
							BuildSA_1(subpop_data.kd_root_, position_data + row * SLIM_MAX_DIMENSIONALITY, row, subpop_data.dist_str_);
						break;
					case 2:
						for (row = start_row; row < after_end_row; row++)
							BuildSA_2(subpop_data.kd_root_, position_data + row * SLIM_MAX_DIMENSIONALITY, row, subpop_data.dist_str_, 0);
						break;
					case 3:
						for (row = start_row; row < after_end_row; row++)
							BuildSA_3(subpop_data.kd_root_, position_data + row * SLIM_MAX_DIMENSIONALITY, row, subpop_data.dist_str_, 0);
						break;
				}
			}
			else
			{
				// With a specified exerter sex, we use a special version of BuildSA_X() that tests for that by range
				int start_exerter = 0, after_end_exerter = subpop_size;
				
				if (exerter_sex_ == IndividualSex::kMale)
					start_exerter = subpop_data.first_male_index_;
				else if (exerter_sex_ == IndividualSex::kFemale)
					after_end_exerter = subpop_data.first_male_index_;
				else
					EIDOS_TERMINATION << "ERROR (InteractionType::CalculateAllDistances): (internal error) unrecognized value for exerter_sex_." << EidosTerminate();
				
				switch (spatiality_)
				{
					case 1:
						for (row = start_row; row < after_end_row; row++)
							BuildSA_SS_1(subpop_data.kd_root_, position_data + row * SLIM_MAX_DIMENSIONALITY, row, subpop_data.dist_str_, start_exerter, after_end_exerter);
						break;
					case 2:
						for (row = start_row; row < after_end_row; row++)
							BuildSA_SS_2(subpop_data.kd_root_, position_data + row * SLIM_MAX_DIMENSIONALITY, row, subpop_data.dist_str_, start_exerter, after_end_exerter, 0);
						break;
					case 3:
						for (row = start_row; row < after_end_row; row++)
							BuildSA_SS_3(subpop_data.kd_root_, position_data + row * SLIM_MAX_DIMENSIONALITY, row, subpop_data.dist_str_, start_exerter, after_end_exerter, 0);
						break;
				}
			}
			
			subpop_data.dist_str_->Finished();
			subpop_data.distances_calculated_ = true;
		}
		else
		{
			// Non-spatial interactions do not cache anything, since their cache would have to be NxN, which is unacceptable
		}
	}
}

void InteractionType::CalculateAllStrengths(Subpopulation *p_subpop)
{
	slim_objectid_t subpop_id = p_subpop->subpopulation_id_;
	InteractionsData &subpop_data = data_[subpop_id];
	
	if (!subpop_data.strengths_calculated_)
	{
		if (!subpop_data.evaluated_)
			EIDOS_TERMINATION << "ERROR (InteractionType::CalculateAllStrengths): interaction has not yet been evaluated." << EidosTerminate();
		
		if (spatiality_ > 0)
		{
			if (!subpop_data.distances_calculated_)
				CalculateAllDistances(p_subpop);
			
			// Here we scan through the pre-existing sparse array for interacting pairs,
			// and fill in interaction strength values calculated for each.
			slim_popsize_t subpop_size = p_subpop->parent_subpop_size_;
			Individual **subpop_individuals = p_subpop->parent_individuals_.data();
			std::vector<SLiMEidosBlock*> &callbacks = subpop_data.evaluation_interaction_callbacks_;
			SparseArray &dist_str = *subpop_data.dist_str_;
			
			if (callbacks.size() == 0)
			{
				// No callbacks; strength calculations come from the interaction function only
				// We do not use reciprocity here, as searching for the mirrored entry would probably take longer than just calculating twice
				for (uint32_t row = 0; row < (uint32_t)subpop_size; ++row)
				{
					uint32_t row_nnz, *row_columns;
					sa_distance_t *row_distances;
					sa_strength_t *row_strengths;
					
					dist_str.InteractionsForRow(row, &row_nnz, &row_columns, &row_distances, &row_strengths);
					
					// CalculateStrengthNoCallbacks() is basically inlined here, moved outside the loop; see that function for comments
					switch (if_type_)
					{
						case IFType::kFixed:
						{
							for (uint32_t col_iter = 0; col_iter < row_nnz; ++col_iter)
								row_strengths[col_iter] = (sa_strength_t)if_param1_;
							break;
						}
						case IFType::kLinear:
						{
							for (uint32_t col_iter = 0; col_iter < row_nnz; ++col_iter)
							{
								sa_distance_t distance = row_distances[col_iter];
								
								row_strengths[col_iter] = (sa_strength_t)(if_param1_ * (1.0 - distance / max_distance_));
							}
							break;
						}
						case IFType::kExponential:
						{
							for (uint32_t col_iter = 0; col_iter < row_nnz; ++col_iter)
							{
								sa_distance_t distance = row_distances[col_iter];
								
								row_strengths[col_iter] = (sa_strength_t)(if_param1_ * exp(-if_param2_ * distance));
							}
							break;
						}
						case IFType::kNormal:
						{
							for (uint32_t col_iter = 0; col_iter < row_nnz; ++col_iter)
							{
								sa_distance_t distance = row_distances[col_iter];
								
								row_strengths[col_iter] = (sa_strength_t)(if_param1_ * exp(-(distance * distance) / (2.0 * if_param2_ * if_param2_)));
							}
							break;
						}
						case IFType::kCauchy:
						{
							for (uint32_t col_iter = 0; col_iter < row_nnz; ++col_iter)
							{
								sa_distance_t distance = row_distances[col_iter];
								double temp = distance / if_param2_;
								
								row_strengths[col_iter] = (sa_strength_t)(if_param1_ / (1.0 + temp * temp));
							}
							break;
						}
						default:
						{
							// should never be hit, but this is the base case
							for (uint32_t col_iter = 0; col_iter < row_nnz; ++col_iter)
							{
								sa_distance_t distance = row_distances[col_iter];
								
								row_strengths[col_iter] = (sa_strength_t)CalculateStrengthNoCallbacks(distance);
							}
							
							EIDOS_TERMINATION << "ERROR (InteractionType::CalculateAllStrengths): (internal error) unimplemented IFType case." << EidosTerminate();
						}
					}
				}
			}
			else
			{
				// Callbacks; strength calculations need to include callback effects
				// We use reciprocality here, partly for speed and partly to guarantee that A->B == B->A even for stochastic or
				// state-dependent interaction() callbacks, where calling the callback twice might yield different results.
				// The tricky thing is that with sex-segregation some strengths may be mirrored while others are not.  For
				// example, a "MF" interaction has no reciprocal interactions (females exert upon males, exclusively), a "MM"
				// interaction has only reciprocal interactions (males exert upon males, bidirectionally), but with a "*M"
				// interaction some interactions are reciprocal (from males to males) while others are not (from males to
				// females).  So we have three cases: not reciprocal, fully reciprocal, and partially reciprocal.  Note that
				// these situations are already represented in the structure of the sparse array; if a given interaction is
				// not reciprocal, the mirror entry for it will not exist in the sparse array.  So we have two goals: to
				// avoid wasting time looking for mirror entries that aren't there, and to know when a mirror entry *is*
				// present so that we can confidently skip a calculation knowing that it will be (or has been) filled for us
				// by the mirror entry.
				if (!reciprocal_ ||
					(reciprocal_ && (receiver_sex_ == IndividualSex::kMale) && (exerter_sex_ == IndividualSex::kFemale)) ||
					(reciprocal_ && (receiver_sex_ == IndividualSex::kFemale) && (exerter_sex_ == IndividualSex::kMale)))
				{
					// Not reciprocal (although the reciprocal_ flag may be set).  Each sparse array strength gets calculated.
					for (uint32_t row = 0; row < (uint32_t)subpop_size; ++row)
					{
						uint32_t row_nnz, *row_columns;
						sa_distance_t *row_distances;
						sa_strength_t *row_strengths;
						
						dist_str.InteractionsForRow(row, &row_nnz, &row_columns, &row_distances, &row_strengths);
						
						for (uint32_t col_iter = 0; col_iter < row_nnz; ++col_iter)
						{
							uint32_t col = row_columns[col_iter];
							sa_distance_t distance = row_distances[col_iter];
							sa_strength_t strength = (sa_strength_t)CalculateStrengthWithCallbacks(distance, subpop_individuals[row], subpop_individuals[col], p_subpop, callbacks);
							
							row_strengths[col_iter] = strength;
						}
					}
				}
				else if (reciprocal_ && (receiver_sex_ == exerter_sex_))
				{
					// Fully reciprocal; exerters and receivers are the same.  We calculate when row <= col, and when
					// row < col we find the mirror entry and place the calculated value there too.  When row > col,
					// we assume that the strength will be (or has been) filled in by the mirror entry.
					for (uint32_t row = 0; row < (uint32_t)subpop_size; ++row)
					{
						uint32_t row_nnz, *row_columns;
						sa_distance_t *row_distances;
						sa_strength_t *row_strengths;
						
						dist_str.InteractionsForRow(row, &row_nnz, &row_columns, &row_distances, &row_strengths);
						
						for (uint32_t col_iter = 0; col_iter < row_nnz; ++col_iter)
						{
							uint32_t col = row_columns[col_iter];
							
							if (row <= col)
							{
								sa_distance_t distance = row_distances[col_iter];
								sa_strength_t strength = (sa_strength_t)CalculateStrengthWithCallbacks(distance, subpop_individuals[row], subpop_individuals[col], p_subpop, callbacks);
								
								row_strengths[col_iter] = strength;
								
								if (row < col)
									dist_str.PatchStrength(col, row, strength);
							}
						}
					}
				}
				else
				{
					// Partially reciprocal; there is partial overlap between exerters and receivers.  Basically, for
					// *->F, *->M, M->*, and F->* interactions there is a reciprocal portion that is either M<->M or
					// F<->F, and then there is a non-reciprocal portion that is M->F or F->M.  For each interacting
					// pair, we look at the individuals involved; if their sexes are the same, then the interaction is
					// reciprocal and we mirror it as above, whereas if their sexes are different the interaction is
					// non-reciprocal and we simply calculate it.
					for (uint32_t row = 0; row < (uint32_t)subpop_size; ++row)
					{
						uint32_t row_nnz, *row_columns;
						sa_distance_t *row_distances;
						sa_strength_t *row_strengths;
						
						dist_str.InteractionsForRow(row, &row_nnz, &row_columns, &row_distances, &row_strengths);
						
						for (uint32_t col_iter = 0; col_iter < row_nnz; ++col_iter)
						{
							uint32_t col = row_columns[col_iter];
							Individual *receiver = subpop_individuals[row];
							Individual *exerter = subpop_individuals[col];
							
							if (receiver->sex_ == exerter->sex_)
							{
								// same sex, so we have reciprocality
								if (row <= col)
								{
									sa_distance_t distance = row_distances[col_iter];
									sa_strength_t strength = (sa_strength_t)CalculateStrengthWithCallbacks(distance, receiver, exerter, p_subpop, callbacks);
									
									row_strengths[col_iter] = strength;
									
									if (row < col)
										dist_str.PatchStrength(col, row, strength);
								}
							}
							else
							{
								// different sexes, no reciprocality
								sa_distance_t distance = row_distances[col_iter];
								sa_strength_t strength = (sa_strength_t)CalculateStrengthWithCallbacks(distance, receiver, exerter, p_subpop, callbacks);
								
								row_strengths[col_iter] = strength;
							}
						}
					}
				}
			}
			
			subpop_data.strengths_calculated_ = true;
		}
		else
		{
			// Non-spatial interactions do not cache anything, since their cache would have to be NxN, which is unacceptable
		}
	}
}

double InteractionType::CalculateDistance(double *p_position1, double *p_position2)
{
#ifndef __clang_analyzer__
	if (spatiality_ == 1)
	{
		return fabs(p_position1[0] - p_position2[0]);
	}
	else if (spatiality_ == 2)
	{
		double distance_x = (p_position1[0] - p_position2[0]);
		double distance_y = (p_position1[1] - p_position2[1]);
		
		return sqrt(distance_x * distance_x + distance_y * distance_y);
	}
	else if (spatiality_ == 3)
	{
		double distance_x = (p_position1[0] - p_position2[0]);
		double distance_y = (p_position1[1] - p_position2[1]);
		double distance_z = (p_position1[2] - p_position2[2]);
		
		return sqrt(distance_x * distance_x + distance_y * distance_y + distance_z * distance_z);
	}
	else
		EIDOS_TERMINATION << "ERROR (InteractionType::CalculateDistance): calculation of distances requires that the interaction be spatial." << EidosTerminate();
#else
	return 0.0;
#endif
}

// Calculate a distance including effects of periodicity.  This can always be called instead of
// CalculateDistance(), it is just a little slower since it has to check the periodicity flags.
double InteractionType::CalculateDistanceWithPeriodicity(double *p_position1, double *p_position2, InteractionsData &p_subpop_data)
{
	if (spatiality_ == 1)
	{
		if (periodic_x_)
		{
			double x1 = p_position1[0], x2 = p_position2[0], d1, d2;
			
			if (x1 < x2)	{ d1 = x2 - x1; d2 = (x1 + p_subpop_data.bounds_x1_) - x2; }
			else			{ d1 = x1 - x2; d2 = (x2 + p_subpop_data.bounds_x1_) - x1; }
			
			return std::min(d1, d2);
		}
		else
		{
			return fabs(p_position1[0] - p_position2[0]);
		}
	}
	else if (spatiality_ == 2)
	{
		double distance_x, distance_y;
		
		if (periodic_x_)
		{
			double x1 = p_position1[0], x2 = p_position2[0], d1, d2;
			
			if (x1 < x2)	{ d1 = x2 - x1; d2 = (x1 + p_subpop_data.bounds_x1_) - x2; }
			else			{ d1 = x1 - x2; d2 = (x2 + p_subpop_data.bounds_x1_) - x1; }
			
			distance_x = std::min(d1, d2);
		}
		else
		{
			distance_x = p_position1[0] - p_position2[0];
		}
		
		if (periodic_y_)
		{
			double y1 = p_position1[1], y2 = p_position2[1], d1, d2;
			
			if (y1 < y2)	{ d1 = y2 - y1; d2 = (y1 + p_subpop_data.bounds_y1_) - y2; }
			else			{ d1 = y1 - y2; d2 = (y2 + p_subpop_data.bounds_y1_) - y1; }
			
			distance_y = std::min(d1, d2);
		}
		else
		{
			distance_y = p_position1[1] - p_position2[1];
		}
		
		return sqrt(distance_x * distance_x + distance_y * distance_y);
	}
	else if (spatiality_ == 3)
	{
		double distance_x, distance_y, distance_z;
		
		if (periodic_x_)
		{
			double x1 = p_position1[0], x2 = p_position2[0], d1, d2;
			
			if (x1 < x2)	{ d1 = x2 - x1; d2 = (x1 + p_subpop_data.bounds_x1_) - x2; }
			else			{ d1 = x1 - x2; d2 = (x2 + p_subpop_data.bounds_x1_) - x1; }
			
			distance_x = std::min(d1, d2);
		}
		else
		{
			distance_x = p_position1[0] - p_position2[0];
		}
		
		if (periodic_y_)
		{
			double y1 = p_position1[1], y2 = p_position2[1], d1, d2;
			
			if (y1 < y2)	{ d1 = y2 - y1; d2 = (y1 + p_subpop_data.bounds_y1_) - y2; }
			else			{ d1 = y1 - y2; d2 = (y2 + p_subpop_data.bounds_y1_) - y1; }
			
			distance_y = std::min(d1, d2);
		}
		else
		{
			distance_y = p_position1[1] - p_position2[1];
		}
		
		if (periodic_z_)
		{
			double z1 = p_position1[2], z2 = p_position2[2], d1, d2;
			
			if (z1 < z2)	{ d1 = z2 - z1; d2 = (z1 + p_subpop_data.bounds_z1_) - z2; }
			else			{ d1 = z1 - z2; d2 = (z2 + p_subpop_data.bounds_z1_) - z1; }
			
			distance_z = std::min(d1, d2);
		}
		else
		{
			distance_z = p_position1[2] - p_position2[2];
		}
		
		return sqrt(distance_x * distance_x + distance_y * distance_y + distance_z * distance_z);
	}
	else
		EIDOS_TERMINATION << "ERROR (InteractionType::CalculateDistanceWithPeriodicity): calculation of distances requires that the interaction be spatial." << EidosTerminate();
}

double InteractionType::CalculateStrengthNoCallbacks(double p_distance)
{
	// CAUTION: This method should only be called when p_distance <= max_distance_ (or is NAN).
	// It is the caller's responsibility to do that filtering, for performance reasons!
	// NOTE: The caller does *not* need to guarantee that this is not a self-interaction,
	// or is not ruled out by sex-selectivity; that is taken care of automatically by the
	// logic in CalculateAllDistances().  (If CalculateAllDistances() is not involved, then
	// ruling out the self-interaction case is indeed the caller's responsibility.)
	
	// MAINTAIN IN PARALLEL: InteractionType::CalculateAllStrengths()
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
		case IFType::kCauchy:
		{
			double temp = p_distance / if_param2_;
			return (if_param1_ / (1.0 + temp * temp));													// fmax / (1+(d/λ)^2)
		}
	}
	EIDOS_TERMINATION << "ERROR (InteractionType::CalculateStrengthNoCallbacks): (internal error) unexpected if_type_ value." << EidosTerminate();
}

double InteractionType::CalculateStrengthWithCallbacks(double p_distance, Individual *p_receiver, Individual *p_exerter, Subpopulation *p_subpop, std::vector<SLiMEidosBlock*> &p_interaction_callbacks)
{
	// CAUTION: This method should only be called when p_distance <= max_distance_ (or is NAN).
	// It is the caller's responsibility to do that filtering, for performance reasons!
	// NOTE: The caller does *not* need to guarantee that this is not a self-interaction,
	// or is not ruled out by sex-selectivity; that is taken care of automatically by the
	// logic in CalculateAllDistances().  (If CalculateAllDistances() is not involved, then
	// ruling out the self-interaction case is indeed the caller's responsibility.)
	double strength = CalculateStrengthNoCallbacks(p_distance);
	
	strength = ApplyInteractionCallbacks(p_receiver, p_exerter, p_subpop, strength, p_distance, p_interaction_callbacks);
	
	return strength;
}

double InteractionType::ApplyInteractionCallbacks(Individual *p_receiver, Individual *p_exerter, Subpopulation *p_subpop, double p_strength, double p_distance, std::vector<SLiMEidosBlock*> &p_interaction_callbacks)
{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	SLiMSim &sim = p_subpop->population_.sim_;

	SLiMEidosBlockType old_executing_block_type = sim.executing_block_type_;
	sim.executing_block_type_ = SLiMEidosBlockType::SLiMEidosInteractionCallback;
	
	for (SLiMEidosBlock *interaction_callback : p_interaction_callbacks)
	{
		if (interaction_callback->active_)
		{
			// The callback is active and matches our interaction type id, so we need to execute it
			const EidosASTNode *compound_statement_node = interaction_callback->compound_statement_node_;
			
			if (compound_statement_node->cached_return_value_)
			{
				// The script is a constant expression such as "{ return 1.1; }", so we can short-circuit it completely
				EidosValue_SP result_SP = compound_statement_node->cached_return_value_;
				EidosValue *result = result_SP.get();
				
				if ((result->Type() != EidosValueType::kValueFloat) || (result->Count() != 1))
					EIDOS_TERMINATION << "ERROR (InteractionType::ApplyInteractionCallbacks): interaction() callbacks must provide a float singleton return value." << EidosTerminate(interaction_callback->identifier_token_);
				
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
					EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &sim.SymbolTable());
					EidosSymbolTable client_symbols(EidosSymbolTableType::kVariablesTable, &callback_symbols);
					EidosFunctionMap &function_map = sim.FunctionMap();
					EidosInterpreter interpreter(interaction_callback->compound_statement_node_, client_symbols, function_map, &sim);
					
					if (interaction_callback->contains_self_)
						callback_symbols.InitializeConstantSymbolEntry(interaction_callback->SelfSymbolTableEntry());		// define "self"
					
					// Set all of the callback's parameters; note we use InitializeConstantSymbolEntry() for speed.
					// We can use that method because we know the lifetime of the symbol table is shorter than that of
					// the value objects, and we know that the values we are setting here will not change (the objects
					// referred to by the values may change, but the values themselves will not change).
					if (interaction_callback->contains_distance_)
					{
						local_distance.StackAllocated();		// prevent Eidos_intrusive_ptr from trying to delete this
						callback_symbols.InitializeConstantSymbolEntry(gID_distance, EidosValue_SP(&local_distance));
					}
					if (interaction_callback->contains_strength_)
					{
						local_strength.StackAllocated();		// prevent Eidos_intrusive_ptr from trying to delete this
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
							EIDOS_TERMINATION << "ERROR (InteractionType::ApplyInteractionCallbacks): interaction() callbacks must provide a float singleton return value." << EidosTerminate(interaction_callback->identifier_token_);
						
						p_strength = result->FloatAtIndex(0, nullptr);
						
						if (std::isnan(p_strength) || std::isinf(p_strength) || (p_strength < 0.0))
							EIDOS_TERMINATION << "ERROR (InteractionType::ApplyInteractionCallbacks): interaction() callbacks must return a finite value >= 0.0." << EidosTerminate(interaction_callback->identifier_token_);
						
						// Output generated by the interpreter goes to our output stream
						interpreter.FlushExecutionOutputToStream(SLIM_OUTSTREAM);
					}
					catch (...)
					{
						// Emit final output even on a throw, so that stop() messages and such get printed
						interpreter.FlushExecutionOutputToStream(SLIM_OUTSTREAM);
						
						throw;
					}
				}
			}
		}
	}
	
	sim.executing_block_type_ = old_executing_block_type;
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(sim.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosInteractionCallback)]);
#endif
	
	return p_strength;
}

size_t InteractionType::MemoryUsageForKDTrees(void)
{
	size_t usage = 0;
	
	for (auto &iter : data_)
	{
		const InteractionsData &data = iter.second;
		usage += sizeof(SLiM_kdNode) * data.individual_count_;
	}
	
	return usage;
}

size_t InteractionType::MemoryUsageForPositions(void)
{
	size_t usage = 0;
	
	for (auto &iter : data_)
	{
		const InteractionsData &data = iter.second;
		usage += sizeof(double) * data.individual_count_;
	}
	
	return usage;
}

size_t InteractionType::MemoryUsageForSparseArrays(void)
{
	size_t usage = 0;
	
	for (auto &iter : data_)
	{
		SparseArray *array = iter.second.dist_str_;
		
		if (array)
			usage += iter.second.dist_str_->MemoryUsage();
	}
	
	return usage;
}


#pragma mark -
#pragma mark k-d tree construction
#pragma mark -

// This k-d tree code is patterned after the C code at RosettaCode.org : https://rosettacode.org/wiki/K-d_tree#C
// It uses a Quickselect-style algorithm to select medians to produce a balanced tree
// Each spatiality case is coded separately, for maximum speed, but they are very parallel

// Some of the code below is separated by phase.  The k-d tree cycles through phase (x, y, z) as you descend,
// and rather than passing phase as a parameter, the code has been factored into phase-specific functions
// that are mutually recursive, for speed.  It's not a huge win, but it does help a little.

// BCH 14 August 2017: NOTE that I have found that the RosettaCode.org C example code was incorrect, which
// is disappointing.  It tried to check for duplicates of the median and terminate early, but its logic for
// doing so was flawed and resulted in a bad tree that produced incorrect results.  This code now follows
// the logic of the pseudocode at Wikipedia (https://en.wikipedia.org/wiki/Quickselect), which seems correct.
// Ironically, the incorrect logic of the RosettaCode version only produced incorrect results when there
// were duplicated values in the coordinate vector.

inline __attribute__((always_inline)) void swap(SLiM_kdNode *p_x, SLiM_kdNode *p_y)
{
	std::swap(p_x->x, p_y->x);
	std::swap(p_x->individual_index_, p_y->individual_index_);
}

// find median for phase 0 (x)
SLiM_kdNode *InteractionType::FindMedian_p0(SLiM_kdNode *start, SLiM_kdNode *end)
{
	SLiM_kdNode *p, *store, *md = start + (end - start) / 2;	// md is the location where the median will eventually be placed
	double pivot;
	
	while (1)
	{
		if (end == start + 1) return start;						// if end==start+1 we have reached the base case of the recursion, so return
		
		pivot = md->x[0];										// get a pivot value from md, which is effectively a random guess
		
		swap(md, end - 1);										// place the pivot value at the very end of our range
		for (store = p = start; p < end; p++)					// loop p over our range and partition into values < vs. >= pivot
		{
			if (p->x[0] < pivot)								// p is less than the pivot, so store it on the left side
			{
				if (p != store)
					swap(p, store);
				store++;
			}
		}
		swap(store, end - 1);									// move the pivot value, at end-1, to the end of the store
		
		if (store == md)		return md;						// pivot position == median; we happened to choose the median as pivot, so we're done!
		else if (store > md)	end = store;					// pivot position > median, so look for the median to the left of pivot
		else					start = store + 1;				// pivot position < median, so look for the median to the right of pivot
	}
}

// find median for phase 1 (y)
SLiM_kdNode *InteractionType::FindMedian_p1(SLiM_kdNode *start, SLiM_kdNode *end)
{
	SLiM_kdNode *p, *store, *md = start + (end - start) / 2;
	double pivot;
	
	while (1)
	{
		if (end == start + 1) return start;
		
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
		
		if (store == md)		return md;
		else if (store > md)	end = store;
		else					start = store + 1;
	}
}

// find median for phase 2 (z)
SLiM_kdNode *InteractionType::FindMedian_p2(SLiM_kdNode *start, SLiM_kdNode *end)
{
	SLiM_kdNode *p, *store, *md = start + (end - start) / 2;
	double pivot;
	
	while (1)
	{
		if (end == start + 1) return start;
		
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
		
		if (store == md)		return md;
		else if (store > md)	end = store;
		else					start = store + 1;
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
		EIDOS_TERMINATION << "ERROR (InteractionType::EnsureKDTreePresent): (internal error) the interaction has not been evaluated." << EidosTerminate();
	
	if (spatiality_ == 0)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::EnsureKDTreePresent): (internal error) k-d tree cannot be constructed for non-spatial interactions." << EidosTerminate();
	}
	else if (!p_subpop_data.kd_nodes_)
	{
		int individual_count = p_subpop_data.individual_count_;
		int count = individual_count;
		
		// If we have any periodic dimensions, we need to replicate our nodes spatially
		int periodic_dimensions = (periodic_x_ ? 1 : 0) + (periodic_y_ ? 1 : 0) + (periodic_z_ ? 1 : 0);
		int periodicity_multiplier = 1;
		
		if (periodic_dimensions == 1)
			periodicity_multiplier = 3;
		else if (periodic_dimensions == 2)
			periodicity_multiplier = 9;
		else if (periodic_dimensions == 3)
			periodicity_multiplier = 27;
		
		count *= periodicity_multiplier;
		p_subpop_data.kd_node_count_ = count;
		
		// Now allocate the chosen number of nodes
		SLiM_kdNode *nodes = (SLiM_kdNode *)calloc(count, sizeof(SLiM_kdNode));
		
		// Fill the nodes with their initial data
		if (periodic_dimensions)
		{
			// This is the periodic case; we replicate the individual position data and add an offset to each replicate
			for (int replicate = 0; replicate < periodicity_multiplier; ++replicate)
			{
				SLiM_kdNode *replicate_nodes = nodes + replicate * individual_count;
				double x_offset = 0, y_offset = 0, z_offset = 0;
				
				// Determine the correct offsets for this replicate of the individual position data;
				// maybe there is a smarter way to do this, but whatever
				int replication_dim_1 = (replicate % 3) - 1;
				int replication_dim_2 = ((replicate / 3) % 3) - 1;
				int replication_dim_3 = (replicate / 9) - 1;
				
				if (periodic_x_)
				{
					x_offset = p_subpop_data.bounds_x1_ * replication_dim_1;
					
					if (periodic_y_)
					{
						y_offset = p_subpop_data.bounds_y1_ * replication_dim_2;
						
						if (periodic_z_)
							z_offset = p_subpop_data.bounds_z1_ * replication_dim_3;
					}
					else if (periodic_z_)
					{
						z_offset = p_subpop_data.bounds_z1_ * replication_dim_2;
					}
				}
				else if (periodic_y_)
				{
					y_offset = p_subpop_data.bounds_y1_ * replication_dim_1;
					
					if (periodic_z_)
						z_offset = p_subpop_data.bounds_z1_ * replication_dim_2;
				}
				else if (periodic_z_)
				{
					z_offset = p_subpop_data.bounds_z1_ * replication_dim_1;
				}
				
				// Now that we have our offsets, copy the data for the replicate
				switch (spatiality_)
				{
					case 1:
						for (int i = 0; i < individual_count; ++i)
						{
							SLiM_kdNode *node = replicate_nodes + i;
							double *position_data = p_subpop_data.positions_ + i * SLIM_MAX_DIMENSIONALITY;
							
							node->x[0] = position_data[0] + x_offset;
							node->individual_index_ = i;
						}
						break;
					case 2:
						for (int i = 0; i < individual_count; ++i)
						{
							SLiM_kdNode *node = replicate_nodes + i;
							double *position_data = p_subpop_data.positions_ + i * SLIM_MAX_DIMENSIONALITY;
							
							node->x[0] = position_data[0] + x_offset;
							node->x[1] = position_data[1] + y_offset;
							node->individual_index_ = i;
						}
						break;
					case 3:
						for (int i = 0; i < individual_count; ++i)
						{
							SLiM_kdNode *node = replicate_nodes + i;
							double *position_data = p_subpop_data.positions_ + i * SLIM_MAX_DIMENSIONALITY;
							
							node->x[0] = position_data[0] + x_offset;
							node->x[1] = position_data[1] + y_offset;
							node->x[2] = position_data[2] + z_offset;
							node->individual_index_ = i;
						}
						break;
				}
			}
		}
		else
		{
			// This is the non-periodic base case, split into spatiality cases for speed
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
		}
		
		p_subpop_data.kd_nodes_ = nodes;
		
		if (p_subpop_data.kd_node_count_ == 0)
		{
			p_subpop_data.kd_root_ = 0;
		}
		else
		{
			// Now call out to recursively construct the tree
			switch (spatiality_)
			{
				case 1: p_subpop_data.kd_root_ = MakeKDTree1_p0(p_subpop_data.kd_nodes_, p_subpop_data.kd_node_count_);	break;
				case 2: p_subpop_data.kd_root_ = MakeKDTree2_p0(p_subpop_data.kd_nodes_, p_subpop_data.kd_node_count_);	break;
				case 3: p_subpop_data.kd_root_ = MakeKDTree3_p0(p_subpop_data.kd_nodes_, p_subpop_data.kd_node_count_);	break;
			}
			
			// Check the tree for correctness; for now I will leave this enabled in the DEBUG case,
			// because a bug was found in the k-d tree code in 2.4.1 that would have been caught by this.
			// Eventually, when it is clear that this code is robust, this check can be disabled.
#ifdef DEBUG
			int total_tree_count = 0;
			
			switch (spatiality_)
			{
				case 1: total_tree_count = CheckKDTree1_p0(p_subpop_data.kd_root_);	break;
				case 2: total_tree_count = CheckKDTree2_p0(p_subpop_data.kd_root_);	break;
				case 3: total_tree_count = CheckKDTree3_p0(p_subpop_data.kd_root_);	break;
			}
			
			if (total_tree_count != p_subpop_data.kd_node_count_)
				EIDOS_TERMINATION << "ERROR (InteractionType::EnsureKDTreePresent): (internal error) the k-d tree count " << total_tree_count << " does not match the allocated node count" << p_subpop_data.kd_node_count_ << "." << EidosTerminate();
#endif
		}
	}
}


#pragma mark -
#pragma mark k-d tree consistency checking
#pragma mark -

// The general strategy is: the _pX() functions check that they are indeed a median node for all of the
// nodes underneath the given node, for the coordinate of the given polarity.  They do this by calling
// the pX_r() method on their left and right subtree, with their own coordinate; it recurses over the
// subtrees.  The pX() method then makes a call on each subtree to have it check itself.  Each pX()
// method call returns the total number of nodes found in itself and its subtrees.

int InteractionType::CheckKDTree1_p0(SLiM_kdNode *t)
{
	double split = t->x[0];
	
	if (t->left) CheckKDTree1_p0_r(t->left, split, true);
	if (t->right) CheckKDTree1_p0_r(t->right, split, false);
	
	int left_count = t->left ? CheckKDTree1_p0(t->left) : 0;
	int right_count = t->right ? CheckKDTree1_p0(t->right) : 0;
	
	return left_count + right_count + 1;
}

void InteractionType::CheckKDTree1_p0_r(SLiM_kdNode *t, double split, bool isLeftSubtree)
{
	double x = t->x[0];
	
	if (isLeftSubtree) {
		if (x > split)	EIDOS_TERMINATION << "ERROR (InteractionType::CheckKDTree1_p0_r): (internal error) the k-d tree is not correctly sorted." << EidosTerminate();
	} else {
		if (x < split)	EIDOS_TERMINATION << "ERROR (InteractionType::CheckKDTree1_p0_r): (internal error) the k-d tree is not correctly sorted." << EidosTerminate();
	}
	if (t->left) CheckKDTree1_p0_r(t->left, split, isLeftSubtree);
	if (t->right) CheckKDTree1_p0_r(t->right, split, isLeftSubtree);
}

int InteractionType::CheckKDTree2_p0(SLiM_kdNode *t)
{
	double split = t->x[0];
	
	if (t->left) CheckKDTree2_p0_r(t->left, split, true);
	if (t->right) CheckKDTree2_p0_r(t->right, split, false);
	
	int left_count = t->left ? CheckKDTree2_p1(t->left) : 0;
	int right_count = t->right ? CheckKDTree2_p1(t->right) : 0;
	
	return left_count + right_count + 1;
}

void InteractionType::CheckKDTree2_p0_r(SLiM_kdNode *t, double split, bool isLeftSubtree)
{
	double x = t->x[0];
	
	if (isLeftSubtree) {
		if (x > split)	EIDOS_TERMINATION << "ERROR (InteractionType::CheckKDTree2_p0_r): (internal error) the k-d tree is not correctly sorted." << EidosTerminate();
	} else {
		if (x < split)	EIDOS_TERMINATION << "ERROR (InteractionType::CheckKDTree2_p0_r): (internal error) the k-d tree is not correctly sorted." << EidosTerminate();
	}
	if (t->left) CheckKDTree2_p0_r(t->left, split, isLeftSubtree);
	if (t->right) CheckKDTree2_p0_r(t->right, split, isLeftSubtree);
}

int InteractionType::CheckKDTree2_p1(SLiM_kdNode *t)
{
	double split = t->x[1];
	
	if (t->left) CheckKDTree2_p1_r(t->left, split, true);
	if (t->right) CheckKDTree2_p1_r(t->right, split, false);
	
	int left_count = t->left ? CheckKDTree2_p0(t->left) : 0;
	int right_count = t->right ? CheckKDTree2_p0(t->right) : 0;
	
	return left_count + right_count + 1;
}

void InteractionType::CheckKDTree2_p1_r(SLiM_kdNode *t, double split, bool isLeftSubtree)
{
	double x = t->x[1];
	
	if (isLeftSubtree) {
		if (x > split)	EIDOS_TERMINATION << "ERROR (InteractionType::CheckKDTree2_p1_r): (internal error) the k-d tree is not correctly sorted." << EidosTerminate();
	} else {
		if (x < split)	EIDOS_TERMINATION << "ERROR (InteractionType::CheckKDTree2_p1_r): (internal error) the k-d tree is not correctly sorted." << EidosTerminate();
	}
	if (t->left) CheckKDTree2_p1_r(t->left, split, isLeftSubtree);
	if (t->right) CheckKDTree2_p1_r(t->right, split, isLeftSubtree);
}

int InteractionType::CheckKDTree3_p0(SLiM_kdNode *t)
{
	double split = t->x[0];
	
	if (t->left) CheckKDTree3_p0_r(t->left, split, true);
	if (t->right) CheckKDTree3_p0_r(t->right, split, false);
	
	int left_count = t->left ? CheckKDTree3_p1(t->left) : 0;
	int right_count = t->right ? CheckKDTree3_p1(t->right) : 0;
	
	return left_count + right_count + 1;
}

void InteractionType::CheckKDTree3_p0_r(SLiM_kdNode *t, double split, bool isLeftSubtree)
{
	double x = t->x[0];
	
	if (isLeftSubtree) {
		if (x > split)	EIDOS_TERMINATION << "ERROR (InteractionType::CheckKDTree3_p0_r): (internal error) the k-d tree is not correctly sorted." << EidosTerminate();
	} else {
		if (x < split)	EIDOS_TERMINATION << "ERROR (InteractionType::CheckKDTree3_p0_r): (internal error) the k-d tree is not correctly sorted." << EidosTerminate();
	}
	if (t->left) CheckKDTree3_p0_r(t->left, split, isLeftSubtree);
	if (t->right) CheckKDTree3_p0_r(t->right, split, isLeftSubtree);
}

int InteractionType::CheckKDTree3_p1(SLiM_kdNode *t)
{
	double split = t->x[1];
	
	if (t->left) CheckKDTree3_p1_r(t->left, split, true);
	if (t->right) CheckKDTree3_p1_r(t->right, split, false);
	
	int left_count = t->left ? CheckKDTree3_p2(t->left) : 0;
	int right_count = t->right ? CheckKDTree3_p2(t->right) : 0;
	
	return left_count + right_count + 1;
}

void InteractionType::CheckKDTree3_p1_r(SLiM_kdNode *t, double split, bool isLeftSubtree)
{
	double x = t->x[1];
	
	if (isLeftSubtree) {
		if (x > split)	EIDOS_TERMINATION << "ERROR (InteractionType::CheckKDTree3_p1_r): (internal error) the k-d tree is not correctly sorted." << EidosTerminate();
	} else {
		if (x < split)	EIDOS_TERMINATION << "ERROR (InteractionType::CheckKDTree3_p1_r): (internal error) the k-d tree is not correctly sorted." << EidosTerminate();
	}
	if (t->left) CheckKDTree3_p1_r(t->left, split, isLeftSubtree);
	if (t->right) CheckKDTree3_p1_r(t->right, split, isLeftSubtree);
}

int InteractionType::CheckKDTree3_p2(SLiM_kdNode *t)
{
	double split = t->x[2];
	
	if (t->left) CheckKDTree3_p2_r(t->left, split, true);
	if (t->right) CheckKDTree3_p2_r(t->right, split, false);
	
	int left_count = t->left ? CheckKDTree3_p0(t->left) : 0;
	int right_count = t->right ? CheckKDTree3_p0(t->right) : 0;
	
	return left_count + right_count + 1;
}

void InteractionType::CheckKDTree3_p2_r(SLiM_kdNode *t, double split, bool isLeftSubtree)
{
	double x = t->x[2];
	
	if (isLeftSubtree) {
		if (x > split)	EIDOS_TERMINATION << "ERROR (InteractionType::CheckKDTree3_p2_r): (internal error) the k-d tree is not correctly sorted." << EidosTerminate();
	} else {
		if (x < split)	EIDOS_TERMINATION << "ERROR (InteractionType::CheckKDTree3_p2_r): (internal error) the k-d tree is not correctly sorted." << EidosTerminate();
	}
	if (t->left) CheckKDTree3_p2_r(t->left, split, isLeftSubtree);
	if (t->right) CheckKDTree3_p2_r(t->right, split, isLeftSubtree);
}


#pragma mark -
#pragma mark k-d tree sparse array building
#pragma mark -

inline __attribute__((always_inline)) double dist_sq1(SLiM_kdNode *a, double *b)
{
#ifndef __clang_analyzer__
	double t = a->x[0] - b[0];
	
	return t * t;
#else
	return 0.0;
#endif
}

inline __attribute__((always_inline)) double dist_sq2(SLiM_kdNode *a, double *b)
{
#ifndef __clang_analyzer__
	double t, d;
	
	t = a->x[0] - b[0];
	d = t * t;
	
	t = a->x[1] - b[1];
	d += t * t;
	
	return d;
#else
	return 0.0;
#endif
}

inline __attribute__((always_inline)) double dist_sq3(SLiM_kdNode *a, double *b)
{
#ifndef __clang_analyzer__
	double t, d;
	
	t = a->x[0] - b[0];
	d = t * t;
	
	t = a->x[1] - b[1];
	d += t * t;
	
	t = a->x[2] - b[2];
	d += t * t;
	
	return d;
#else
	return 0.0;
#endif
}

// add neighbors to the sparse array in 1D
void InteractionType::BuildSA_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseArray *p_sparse_array)
{
	double d = dist_sq1(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[0] - nd[0];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_sparse_array->AddEntryDistance(p_focal_individual_index, root->individual_index_, (sa_distance_t)sqrt(d));
	
	if (dx > 0)
	{
		if (root->left)
			BuildSA_1(root->left, nd, p_focal_individual_index, p_sparse_array);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSA_1(root->right, nd, p_focal_individual_index, p_sparse_array);
	}
	else
	{
		if (root->right)
			BuildSA_1(root->right, nd, p_focal_individual_index, p_sparse_array);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSA_1(root->left, nd, p_focal_individual_index, p_sparse_array);
	}
}

// add neighbors to the sparse array in 2D
void InteractionType::BuildSA_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseArray *p_sparse_array, int p_phase)
{
	double d = dist_sq2(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_sparse_array->AddEntryDistance(p_focal_individual_index, root->individual_index_, (sa_distance_t)sqrt(d));
	
	if (++p_phase >= 2) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			BuildSA_2(root->left, nd, p_focal_individual_index, p_sparse_array, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSA_2(root->right, nd, p_focal_individual_index, p_sparse_array, p_phase);
	}
	else
	{
		if (root->right)
			BuildSA_2(root->right, nd, p_focal_individual_index, p_sparse_array, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSA_2(root->left, nd, p_focal_individual_index, p_sparse_array, p_phase);
	}
}

// add neighbors to the sparse array in 3D
void InteractionType::BuildSA_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseArray *p_sparse_array, int p_phase)
{
	double d = dist_sq3(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_sparse_array->AddEntryDistance(p_focal_individual_index, root->individual_index_, (sa_distance_t)sqrt(d));
	
	if (++p_phase >= 3) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			BuildSA_3(root->left, nd, p_focal_individual_index, p_sparse_array, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSA_3(root->right, nd, p_focal_individual_index, p_sparse_array, p_phase);
	}
	else
	{
		if (root->right)
			BuildSA_3(root->right, nd, p_focal_individual_index, p_sparse_array, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSA_3(root->left, nd, p_focal_individual_index, p_sparse_array, p_phase);
	}
}

// add neighbors to the sparse array in 1D (exerter sex-specific)
void InteractionType::BuildSA_SS_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseArray *p_sparse_array, int start_exerter, int after_end_exerter)
{
	double d = dist_sq1(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[0] - nd[0];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index) && (root->individual_index_ >= start_exerter) && (root->individual_index_ < after_end_exerter))
		p_sparse_array->AddEntryDistance(p_focal_individual_index, root->individual_index_, (sa_distance_t)sqrt(d));
	
	if (dx > 0)
	{
		if (root->left)
			BuildSA_SS_1(root->left, nd, p_focal_individual_index, p_sparse_array, start_exerter, after_end_exerter);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSA_SS_1(root->right, nd, p_focal_individual_index, p_sparse_array, start_exerter, after_end_exerter);
	}
	else
	{
		if (root->right)
			BuildSA_SS_1(root->right, nd, p_focal_individual_index, p_sparse_array, start_exerter, after_end_exerter);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSA_SS_1(root->left, nd, p_focal_individual_index, p_sparse_array, start_exerter, after_end_exerter);
	}
}

// add neighbors to the sparse array in 2D (exerter sex-specific)
void InteractionType::BuildSA_SS_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseArray *p_sparse_array, int start_exerter, int after_end_exerter, int p_phase)
{
	double d = dist_sq2(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index) && (root->individual_index_ >= start_exerter) && (root->individual_index_ < after_end_exerter))
		p_sparse_array->AddEntryDistance(p_focal_individual_index, root->individual_index_, (sa_distance_t)sqrt(d));
	
	if (++p_phase >= 2) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			BuildSA_SS_2(root->left, nd, p_focal_individual_index, p_sparse_array, start_exerter, after_end_exerter, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSA_SS_2(root->right, nd, p_focal_individual_index, p_sparse_array, start_exerter, after_end_exerter, p_phase);
	}
	else
	{
		if (root->right)
			BuildSA_SS_2(root->right, nd, p_focal_individual_index, p_sparse_array, start_exerter, after_end_exerter, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSA_SS_2(root->left, nd, p_focal_individual_index, p_sparse_array, start_exerter, after_end_exerter, p_phase);
	}
}

// add neighbors to the sparse array in 3D (exerter sex-specific)
void InteractionType::BuildSA_SS_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseArray *p_sparse_array, int start_exerter, int after_end_exerter, int p_phase)
{
	double d = dist_sq3(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index) && (root->individual_index_ >= start_exerter) && (root->individual_index_ < after_end_exerter))
		p_sparse_array->AddEntryDistance(p_focal_individual_index, root->individual_index_, (sa_distance_t)sqrt(d));
	
	if (++p_phase >= 3) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			BuildSA_SS_3(root->left, nd, p_focal_individual_index, p_sparse_array, start_exerter, after_end_exerter, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSA_SS_3(root->right, nd, p_focal_individual_index, p_sparse_array, start_exerter, after_end_exerter, p_phase);
	}
	else
	{
		if (root->right)
			BuildSA_SS_3(root->right, nd, p_focal_individual_index, p_sparse_array, start_exerter, after_end_exerter, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSA_SS_3(root->left, nd, p_focal_individual_index, p_sparse_array, start_exerter, after_end_exerter, p_phase);
	}
}


#pragma mark -
#pragma mark k-d tree neighbor searches
#pragma mark -

// find the one best neighbor in 1D
void InteractionType::FindNeighbors1_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SLiM_kdNode **best, double *best_dist)
{
	double d = dist_sq1(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[0] - nd[0];
#else
	double dx = 0.0;
#endif
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
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
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
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
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
void InteractionType::FindNeighborsA_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, EidosValue_Object_vector &p_result_vec, std::vector<Individual *> &p_individuals)
{
	double d = dist_sq1(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[0] - nd[0];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_result_vec.push_object_element_NORR(p_individuals[root->individual_index_]);
	
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
void InteractionType::FindNeighborsA_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, EidosValue_Object_vector &p_result_vec, std::vector<Individual *> &p_individuals, int p_phase)
{
	double d = dist_sq2(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_result_vec.push_object_element_NORR(p_individuals[root->individual_index_]);
	
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
void InteractionType::FindNeighborsA_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, EidosValue_Object_vector &p_result_vec, std::vector<Individual *> &p_individuals, int p_phase)
{
	double d = dist_sq3(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_result_vec.push_object_element_NORR(p_individuals[root->individual_index_]);
	
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
#ifndef __clang_analyzer__
	double dx = root->x[0] - nd[0];
#else
	double dx = 0.0;
#endif
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
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
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
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
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

void InteractionType::FindNeighbors(Subpopulation *p_subpop, InteractionsData &p_subpop_data, double *p_point, int p_count, EidosValue_Object_vector &p_result_vec, Individual *p_excluded_individual)
{
	if (spatiality_ == 0)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::FindNeighbors): (internal error) neighbors cannot be found for non-spatial interactions." << EidosTerminate();
	}
	else if (!p_subpop_data.kd_nodes_)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::FindNeighbors): (internal error) the k-d tree has not been constructed." << EidosTerminate();
	}
	else if (!p_subpop_data.kd_root_)
	{
		EIDOS_TERMINATION << "ERROR (InteractionType::FindNeighbors): (internal error) the k-d tree is rootless." << EidosTerminate();
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
				Individual *best_individual = p_subpop->parent_individuals_[best->individual_index_];
				
				p_result_vec.push_object_element_NORR(best_individual);
			}
		}
		else if (p_count >= p_subpop_data.individual_count_ - 1)	// -1 because the focal individual is excluded
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
				
				Individual *best_individual = p_subpop->parent_individuals_[best_rec->individual_index_];
				
				p_result_vec.push_object_element_NORR(best_individual);
			}
			
			free(best);
			free(best_dist);
		}
	}
}


//
//	Eidos support
//
#pragma mark -
#pragma mark Eidos support
#pragma mark -

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
		{
			slim_usertag_t tag_value = tag_value_;
			
			if (tag_value == SLIM_TAG_UNSET_VALUE)
				EIDOS_TERMINATION << "ERROR (InteractionType::GetProperty): property tag accessed on interaction type before being set." << EidosTerminate();
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(tag_value));
		}
			
			// all others, including gID_none
		default:
			return EidosObjectElement::GetProperty(p_property_id);
	}
}

EidosValue *InteractionType::GetProperty_Accelerated_id(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		InteractionType *value = (InteractionType *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->interaction_type_id_, value_index);
	}
	
	return int_result;
}

EidosValue *InteractionType::GetProperty_Accelerated_tag(EidosObjectElement **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		InteractionType *value = (InteractionType *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (InteractionType::GetProperty): property tag accessed on interaction type before being set." << EidosTerminate();
		
		int_result->set_int_no_check(tag_value, value_index);
	}
	
	return int_result;
}

void InteractionType::SetProperty(EidosGlobalStringID p_property_id, const EidosValue &p_value)
{
	// All of our strings are in the global registry, so we can require a successful lookup
	switch (p_property_id)
	{
		case gID_maxDistance:
		{
			if (AnyEvaluated())
				EIDOS_TERMINATION << "ERROR (InteractionType::SetProperty): maxDistance cannot be changed while the interaction is being evaluated; call unevaluate() first, or set maxDistance prior to evaluation of the interaction." << EidosTerminate();
			
			max_distance_ = p_value.FloatAtIndex(0, nullptr);
			max_distance_sq_ = max_distance_ * max_distance_;
			
			if (max_distance_ < 0.0)
				EIDOS_TERMINATION << "ERROR (InteractionType::SetProperty): the maximum interaction distance must be greater than or equal to zero." << EidosTerminate();
			if ((if_type_ == IFType::kLinear) && (std::isinf(max_distance_) || (max_distance_ <= 0.0)))
				EIDOS_TERMINATION << "ERROR (InteractionType::SetProperty): the maximum interaction distance must be finite and greater than zero when interaction type 'l' has been chosen." << EidosTerminate();
			
			// tweak a flag to make SLiMgui update
			sim_.interaction_types_changed_ = true;
			
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

EidosValue_SP InteractionType::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_distance:					return ExecuteMethod_distance(p_method_id, p_arguments, p_interpreter);
		case gID_distanceToPoint:			return ExecuteMethod_distanceToPoint(p_method_id, p_arguments, p_interpreter);
		case gID_drawByStrength:			return ExecuteMethod_drawByStrength(p_method_id, p_arguments, p_interpreter);
		case gID_evaluate:					return ExecuteMethod_evaluate(p_method_id, p_arguments, p_interpreter);
		case gID_interactingNeighborCount:	return ExecuteMethod_interactingNeighborCount(p_method_id, p_arguments, p_interpreter);
		case gID_interactionDistance:		return ExecuteMethod_interactionDistance(p_method_id, p_arguments, p_interpreter);
		case gID_nearestInteractingNeighbors:	return ExecuteMethod_nearestInteractingNeighbors(p_method_id, p_arguments, p_interpreter);
		case gID_nearestNeighbors:			return ExecuteMethod_nearestNeighbors(p_method_id, p_arguments, p_interpreter);
		case gID_nearestNeighborsOfPoint:	return ExecuteMethod_nearestNeighborsOfPoint(p_method_id, p_arguments, p_interpreter);
		case gID_setInteractionFunction:	return ExecuteMethod_setInteractionFunction(p_method_id, p_arguments, p_interpreter);
		case gID_strength:					return ExecuteMethod_strength(p_method_id, p_arguments, p_interpreter);
		case gID_totalOfNeighborStrengths:	return ExecuteMethod_totalOfNeighborStrengths(p_method_id, p_arguments, p_interpreter);
		case gID_unevaluate:				return ExecuteMethod_unevaluate(p_method_id, p_arguments, p_interpreter);
		default:							return EidosDictionary::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

//
//	*********************	– (float)distance(object<Individual> individuals1, [No<Individual> individuals2 = NULL])
EidosValue_SP InteractionType::ExecuteMethod_distance(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *individuals1_value = p_arguments[0].get();
	EidosValue *individuals2_value = p_arguments[1].get();
	
	EidosValue *individuals1 = individuals1_value, *individuals2 = individuals2_value;
	int count1 = individuals1->Count(), count2 = individuals2->Count();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distance): distance() requires that the interaction be spatial." << EidosTerminate();
	
	if ((count1 != 1) && (count2 != 1))
	{
		if ((count1 == 0) && (count2 == 0))
			return gStaticEidosValue_Float_ZeroVec;
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distance): distance() requires that either individuals1 or individuals2 be singleton." << EidosTerminate();
	}
	
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
	
	if (ind1_index < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distance): distances can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
	
	auto subpop_data_iter = data_.find(subpop1_id);
	
	if ((subpop_data_iter == data_.end()) || !subpop_data_iter->second.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distance): distance() requires that the interaction has been evaluated for the subpopulation first." << EidosTerminate();
	
	InteractionsData &subpop_data = subpop_data_iter->second;
	
	double *position_data = subpop_data.positions_;
	double *ind1_position = position_data + ind1_index * SLIM_MAX_DIMENSIONALITY;
	bool periodicity_enabled = (periodic_x_ || periodic_y_ || periodic_z_);
	
	if (individuals2->Type() == EidosValueType::kValueNULL)
	{
		// NULL means return distances from individuals1 (which must be singleton) to all individuals in the subpopulation
		EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(subpop1_size);
		
		if (periodicity_enabled)
		{
			for (int ind2_index = 0; ind2_index < subpop1_size; ++ind2_index)
			{
				double distance = CalculateDistanceWithPeriodicity(ind1_position, position_data + ind2_index * SLIM_MAX_DIMENSIONALITY, subpop_data);
				
				result_vec->set_float_no_check(distance, ind2_index);
			}
		}
		else
		{
			for (int ind2_index = 0; ind2_index < subpop1_size; ++ind2_index)
			{
				double distance = CalculateDistance(ind1_position, position_data + ind2_index * SLIM_MAX_DIMENSIONALITY);
				
				result_vec->set_float_no_check(distance, ind2_index);
			}
		}
		
		return EidosValue_SP(result_vec);
	}
	else
	{
		// Otherwise, individuals1 is singleton, and individuals2 is any length, so we loop over individuals2
		EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(count2);
		
		for (int ind2_index = 0; ind2_index < count2; ++ind2_index)
		{
			Individual *ind2 = (Individual *)individuals2->ObjectElementAtIndex(ind2_index, nullptr);
			
			if (subpop1 != &(ind2->subpopulation_))
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distance): distance() requires that all individuals be in the same subpopulation." << EidosTerminate();
			
			slim_popsize_t ind2_index_in_subpop = ind2->index_;
			
			if (ind2_index_in_subpop < 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distance): distances can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
			
			double distance;
			
			if (periodicity_enabled)
				distance = CalculateDistanceWithPeriodicity(ind1_position, position_data + ind2_index_in_subpop * SLIM_MAX_DIMENSIONALITY, subpop_data);
			else
				distance = CalculateDistance(ind1_position, position_data + ind2_index_in_subpop * SLIM_MAX_DIMENSIONALITY);
			
			result_vec->set_float_no_check(distance, ind2_index);
		}
		
		return EidosValue_SP(result_vec);
	}
}

//	*********************	– (float)distanceToPoint(object<Individual> individuals1, float point)
//
EidosValue_SP InteractionType::ExecuteMethod_distanceToPoint(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *individuals1_value = p_arguments[0].get();
	EidosValue *point_value = p_arguments[1].get();
	
	EidosValue *individuals = individuals1_value, *point = point_value;
	int count = individuals->Count(), point_count = point->Count();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distanceToPoint): distanceToPoint() requires that the interaction be spatial." << EidosTerminate();
	if (point_count != spatiality_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distanceToPoint): distanceToPoint() requires that point is of length equal to the interaction spatiality." << EidosTerminate();
	
	if (count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	// Get the point's coordinates into a double[]
	double point_data[SLIM_MAX_DIMENSIONALITY];
	
	for (int point_index = 0; point_index < spatiality_; ++point_index)
		point_data[point_index] = point->FloatAtIndex(point_index, nullptr);
	
#ifdef __clang_analyzer__
	// The static analyzer does not understand some things, so we tell it here
	point_data[0] = 0;
	point_data[1] = 0;
	point_data[2] = 0;
	assert((spatiality_ >= 1) || !periodic_x_);
	assert((spatiality_ >= 2) || !periodic_y_);
	assert((spatiality_ >= 3) || !periodic_z_);
#endif
	
	// individuals is guaranteed to be of length >= 1; let's get the info on it
	Individual *ind_first = (Individual *)individuals->ObjectElementAtIndex(0, nullptr);
	Subpopulation *subpop1 = &(ind_first->subpopulation_);
	slim_objectid_t subpop1_id = subpop1->subpopulation_id_;
	auto subpop_data_iter = data_.find(subpop1_id);
	
	if ((subpop_data_iter == data_.end()) || !subpop_data_iter->second.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distanceToPoint): distanceToPoint() requires that the interaction has been evaluated for the subpopulation first." << EidosTerminate();
	
	InteractionsData &subpop_data = subpop_data_iter->second;
	double *position_data = subpop_data.positions_;
	bool periodicity_enabled = (periodic_x_ || periodic_y_ || periodic_z_);
	
	// If we're using periodic boundaries, the point supplied has to be within bounds in the periodic dimensions; points outside periodic bounds make no sense
	if (periodicity_enabled)
	{
		if ((periodic_x_ && ((point_data[0] < 0.0) || (point_data[0] > subpop_data.bounds_x1_))) ||
			(periodic_y_ && ((point_data[1] < 0.0) || (point_data[1] > subpop_data.bounds_y1_))) ||
			(periodic_z_ && ((point_data[2] < 0.0) || (point_data[2] > subpop_data.bounds_z1_))))
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distanceToPoint): distanceToPoint() requires that coordinates for periodic spatial dimensions fall inside spatial bounaries; use pointPeriodic() to ensure this if necessary." << EidosTerminate();
	}
	
	EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(count);
	
	if (periodicity_enabled)
	{
		for (int ind_index = 0; ind_index < count; ++ind_index)
		{
			Individual *ind = (Individual *)individuals->ObjectElementAtIndex(ind_index, nullptr);
			
			if (subpop1 != &(ind->subpopulation_))
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distanceToPoint): distanceToPoint() requires that all individuals be in the same subpopulation." << EidosTerminate();
			
			slim_popsize_t ind_index_in_subpop = ind->index_;
			
			if (ind_index_in_subpop < 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distance): distances can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
			
			double *ind_position = position_data + ind_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			
			result_vec->set_float_no_check(CalculateDistanceWithPeriodicity(ind_position, point_data, subpop_data), ind_index);
		}
	}
	else
	{
		for (int ind_index = 0; ind_index < count; ++ind_index)
		{
			Individual *ind = (Individual *)individuals->ObjectElementAtIndex(ind_index, nullptr);
			
			if (subpop1 != &(ind->subpopulation_))
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distanceToPoint): distanceToPoint() requires that all individuals be in the same subpopulation." << EidosTerminate();
			
			slim_popsize_t ind_index_in_subpop = ind->index_;
			
			if (ind_index_in_subpop < 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distance): distances can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
			
			double *ind_position = position_data + ind_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			
			result_vec->set_float_no_check(CalculateDistance(ind_position, point_data), ind_index);
		}
	}
	
	return EidosValue_SP(result_vec);
}

// a helper function for ExecuteMethod_drawByStrength() that does the draws using a vector of weights
static void DrawByWeights(int draw_count, const double *weights, int n_weights, double weight_total, std::vector<int> &draw_indices)
{
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
	if (weight_total > 0.0)
	{
		if (draw_count > 50)		// the empirically determined crossover point in performance
		{
			// Use gsl_ran_discrete() to do the drawing
			gsl_ran_discrete_t *gsl_lookup = gsl_ran_discrete_preproc(n_weights, weights);
			
			for (int64_t draw_index = 0; draw_index < draw_count; ++draw_index)
			{
				int hit_index = (int)gsl_ran_discrete(EIDOS_GSL_RNG, gsl_lookup);
				
				draw_indices.push_back(hit_index);
			}
			
			gsl_ran_discrete_free(gsl_lookup);
		}
		else
		{
			// Use linear search to do the drawing
			for (int64_t draw_index = 0; draw_index < draw_count; ++draw_index)
			{
				double the_rose_in_the_teeth = Eidos_rng_uniform(EIDOS_GSL_RNG) * weight_total;
				double cumulative_weight = 0.0;
				int hit_index;
				
				for (hit_index = 0; hit_index < n_weights; ++hit_index)
				{
					double weight = weights[hit_index];
					
					cumulative_weight += weight;
					
					if (the_rose_in_the_teeth <= cumulative_weight)
						break;
				}
				
				// We might overrun the end, due to roundoff error; if so, attribute it to the first non-zero weight entry
				if (hit_index >= n_weights)
				{
					for (hit_index = 0; hit_index < n_weights; ++hit_index)
						if (weights[hit_index] > 0.0)
							break;
					if (hit_index >= n_weights)
						hit_index = 0;
				}
				
				draw_indices.push_back(hit_index);
			}
		}
	}
}

//	*********************	– (object<Individual>)drawByStrength(object<Individual>$ individual, [integer$ count = 1])
//
EidosValue_SP InteractionType::ExecuteMethod_drawByStrength(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *individual_value = p_arguments[0].get();
	EidosValue *count_value = p_arguments[1].get();
	
	// Check the individual and subpop
	Individual *individual = (Individual *)individual_value->ObjectElementAtIndex(0, nullptr);
	Subpopulation *subpop = &(individual->subpopulation_);
	slim_objectid_t subpop_id = subpop->subpopulation_id_;
	slim_popsize_t subpop_size = subpop->parent_subpop_size_;
	int ind_index = individual->index_;
	
	if (ind_index < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_drawByStrength): drawByStrength() requires that the focal individual is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
	
	auto subpop_data_iter = data_.find(subpop_id);
	
	if ((subpop_data_iter == data_.end()) || !subpop_data_iter->second.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_drawByStrength): drawByStrength() requires that the interaction has been evaluated for the subpopulation first." << EidosTerminate();
	
	InteractionsData &subpop_data = subpop_data_iter->second;
	
	// Check the count
	int64_t count = count_value->IntAtIndex(0, nullptr);
	
	if (count < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_drawByStrength): drawByStrength() requires count >= 0." << EidosTerminate();
	
	if (count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	// If the individual cannot receive this interaction type, no draws can occur
	if ((receiver_sex_ != IndividualSex::kUnspecified) && (receiver_sex_ != individual->sex_))
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	if (spatiality_ == 0)
	{
		std::vector<SLiMEidosBlock*> &callbacks = subpop_data.evaluation_interaction_callbacks_;
		bool no_callbacks = (callbacks.size() == 0);
		EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		double total_interaction_strength = 0.0;
		std::vector<double> cached_strength;
		std::vector<Individual *> &individuals = subpop->parent_individuals_;
		
		for (slim_popsize_t exerter_index_in_subpop = 0; exerter_index_in_subpop < subpop_size; ++exerter_index_in_subpop)
		{
			Individual *exerter = individuals[exerter_index_in_subpop];
			double strength = 0;
			
			if (exerter_index_in_subpop != ind_index)
			{
				if ((exerter_sex_ == IndividualSex::kUnspecified) || (exerter_sex_ == exerter->sex_))
				{
					if (no_callbacks)
						strength = CalculateStrengthNoCallbacks(NAN);
					else
						strength = CalculateStrengthWithCallbacks(NAN, individual, exerter, subpop, callbacks);
				}
			}
			
			total_interaction_strength += strength;
			cached_strength.emplace_back(strength);
		}
		
		if (total_interaction_strength > 0.0)
		{
			std::vector<int> strength_indices;
			
			result_vec->resize_no_initialize(count);
			DrawByWeights((int)count, cached_strength.data(), subpop_size, total_interaction_strength, strength_indices);
			
			for (size_t result_index = 0; result_index < strength_indices.size(); ++result_index)
			{
				int strength_index = strength_indices[result_index];
				Individual *chosen_individual = individuals[strength_index];
				
				result_vec->set_object_element_no_check_NORR(chosen_individual, result_index);
			}
		}
		
		return EidosValue_SP(result_vec);
	}
	else
	{
		CalculateAllStrengths(subpop);
		
		// Get the sparse array data
		SparseArray &sa = *subpop_data.dist_str_;
		uint32_t row_nnz;
		const uint32_t *row_columns;
		const sa_strength_t *strengths;
		std::vector<double> double_strengths;	// needed by DrawByWeights() for gsl_ran_discrete_preproc()
		
		strengths = sa.StrengthsForRow(ind_index, &row_nnz, &row_columns);
		
		// Total the interaction strengths, and gather a vector of strengths as doubles
		double total_interaction_strength = 0.0;
		
		for (uint32_t col_index = 0; col_index < row_nnz; ++col_index)
		{
			sa_strength_t strength = strengths[col_index];
			
			total_interaction_strength += strength;
			double_strengths.push_back((double)strength);
		}
		
		// Draw individuals
		EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		
		if (total_interaction_strength > 0.0)
		{
			std::vector<int> strength_indices;
			std::vector<Individual *> &parents = subpop->parent_individuals_;
			
			result_vec->resize_no_initialize(count);
			DrawByWeights((int)count, double_strengths.data(), row_nnz, total_interaction_strength, strength_indices);
			
			for (size_t result_index = 0; result_index < strength_indices.size(); ++result_index)
			{
				int strength_index = strength_indices[result_index];
				Individual *chosen_individual = parents[row_columns[strength_index]];
				
				result_vec->set_object_element_no_check_NORR(chosen_individual, result_index);
			}
		}
		
		return EidosValue_SP(result_vec);
	}
}

//	*********************	- (void)evaluate([No<Subpopulation> subpops = NULL], [logical$ immediate = F])
//
EidosValue_SP InteractionType::ExecuteMethod_evaluate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *subpops_value = p_arguments[0].get();
	EidosValue *immediate_value = p_arguments[1].get();
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	
	if ((sim.GenerationStage() == SLiMGenerationStage::kWFStage2GenerateOffspring) ||
		(sim.GenerationStage() == SLiMGenerationStage::kNonWFStage1GenerateOffspring))
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_evaluate): evaluate() may not be called during offspring generation." << EidosTerminate();
	
	bool immediate = immediate_value->LogicalAtIndex(0, nullptr);
	
	if (subpops_value->Type() == EidosValueType::kValueNULL)
	{
		for (std::pair<const slim_objectid_t,Subpopulation*> &subpop_pair : sim.ThePopulation().subpops_)
			EvaluateSubpopulation(subpop_pair.second, immediate);
	}
	else
	{
		// requested subpops, so get them
		int requested_subpop_count = subpops_value->Count();
		
		for (int requested_subpop_index = 0; requested_subpop_index < requested_subpop_count; ++requested_subpop_index)
			EvaluateSubpopulation((Subpopulation *)(subpops_value->ObjectElementAtIndex(requested_subpop_index, nullptr)), immediate);
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	– (integer)interactingNeighborCount(object<Individual> individual)
//
EidosValue_SP InteractionType::ExecuteMethod_interactingNeighborCount(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *individual_value = p_arguments[0].get();
	int individual_count = individual_value->Count();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires that the interaction be spatial." << EidosTerminate();
	
	if (individual_count == 1)
	{
		// Check the individual and subpop
		Individual *individual = (Individual *)individual_value->ObjectElementAtIndex(0, nullptr);
		Subpopulation *subpop = &(individual->subpopulation_);
		slim_objectid_t subpop_id = subpop->subpopulation_id_;
		int ind_index = individual->index_;
		
		if (ind_index < 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactions can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
		
		auto subpop_data_iter = data_.find(subpop_id);
		
		if ((subpop_data_iter == data_.end()) || !subpop_data_iter->second.evaluated_)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires that the interaction has been evaluated for the subpopulation first." << EidosTerminate();
		
		// Find the neighbors
		CalculateAllDistances(subpop);
		
		InteractionsData &subpop_data = subpop_data_iter->second;
		SparseArray &sa = *subpop_data.dist_str_;
		uint32_t row_nnz;
		const uint32_t *row_columns;
		
		sa.DistancesForRow(ind_index, &row_nnz, &row_columns);
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(row_nnz));
	}
	else
	{
		EidosValue_Int_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(individual_count);
		
		// Try to do the subpop-level bookkeeping as little as possible
		Individual *individual0 = (Individual *)individual_value->ObjectElementAtIndex(0, nullptr);
		Subpopulation *subpop = &(individual0->subpopulation_);
		slim_objectid_t subpop_id = subpop->subpopulation_id_;
		auto subpop_data_iter = data_.find(subpop_id);
		
		if ((subpop_data_iter == data_.end()) || !subpop_data_iter->second.evaluated_)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires that the interaction has been evaluated for the subpopulation first." << EidosTerminate();
		
		CalculateAllDistances(subpop);
		
		SparseArray *sa = subpop_data_iter->second.dist_str_;
		
		if (individual_count > 0)
		{
			for (int focal_ind_index = 0; focal_ind_index < individual_count; ++focal_ind_index)
			{
				Individual *individual = (Individual *)individual_value->ObjectElementAtIndex(focal_ind_index, nullptr);
				int ind_index = individual->index_;
				
				if (ind_index < 0)
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactions can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
				
				Subpopulation *ind_subpop = &(individual->subpopulation_);
				
				if (ind_subpop != subpop)
				{
					// switched subpops, so make sure we're up to speed
					subpop = ind_subpop;
					subpop_id = subpop->subpopulation_id_;
					subpop_data_iter = data_.find(subpop_id);
					
					if ((subpop_data_iter == data_.end()) || !subpop_data_iter->second.evaluated_)
						EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires that the interaction has been evaluated for the subpopulation first." << EidosTerminate();
					
					CalculateAllDistances(subpop);
					sa = subpop_data_iter->second.dist_str_;
				}
				
				uint32_t row_nnz;
				const uint32_t *row_columns;
				
				sa->DistancesForRow(ind_index, &row_nnz, &row_columns);
				
				result_vec->set_int_no_check(row_nnz, focal_ind_index);
			}
		}
		
		return EidosValue_SP(result_vec);
	}
}

//
//	*********************	– (float)interactionDistance(object<Individual>$ receiver, [No<Individual> exerters = NULL])
EidosValue_SP InteractionType::ExecuteMethod_interactionDistance(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *receiver_value = p_arguments[0].get();
	EidosValue *exerters_value = p_arguments[1].get();
	
	int exerter_count = exerters_value->Count();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactionDistance): interactionDistance() requires that the interaction be spatial." << EidosTerminate();
	
	// receiver_value is guaranteed to be singleton; let's get the info on it
	Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
	Subpopulation *subpop1 = &(receiver->subpopulation_);
	slim_objectid_t subpop1_id = subpop1->subpopulation_id_;
	slim_popsize_t subpop1_size = subpop1->parent_subpop_size_;
	int receiver_index = receiver->index_;
	
	if (receiver_index < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactionDistance): interactions can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
	
	auto subpop_data_iter = data_.find(subpop1_id);
	
	if ((subpop_data_iter == data_.end()) || !subpop_data_iter->second.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactionDistance): interactionDistance() requires that the interaction has been evaluated for the subpopulation first." << EidosTerminate();
	
	CalculateAllDistances(subpop1);
	
	InteractionsData &subpop_data = subpop_data_iter->second;
	SparseArray &sa = *subpop_data.dist_str_;
	uint32_t row_nnz;
	const uint32_t *row_columns;
	const sa_distance_t *distances;
	
	distances = sa.DistancesForRow(receiver_index, &row_nnz, &row_columns);
	
	if (exerters_value->Type() == EidosValueType::kValueNULL)
	{
		// NULL means return distances from individuals1 (which must be singleton) to all individuals in the subpopulation
		// Make a vector big enough, initialize it to INFINITY, and fill in values from the sparse array
		EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(subpop1_size);
		double *result_ptr = result_vec->data();
		
		for (int exerter_index = 0; exerter_index < subpop1_size; ++exerter_index)
			*(result_ptr + exerter_index) = INFINITY;
		
		for (uint32_t col_index = 0; col_index < row_nnz; ++col_index)
			*(result_ptr + row_columns[col_index]) = distances[col_index];
		
		return EidosValue_SP(result_vec);
	}
	else
	{
		// Otherwise, individuals1 is singleton, and individuals2 is any length, so we loop over individuals2
		// Each individual in individuals2 requires a linear search through the row, unfortunately
		EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerter_count);
		
		for (int exerter_index = 0; exerter_index < exerter_count; ++exerter_index)
		{
			Individual *exerter = (Individual *)exerters_value->ObjectElementAtIndex(exerter_index, nullptr);
			
			if (subpop1 != &(exerter->subpopulation_))
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactionDistance): interactionDistance() requires that all individuals be in the same subpopulation." << EidosTerminate();
			
			slim_popsize_t exerter_index_in_subpop = exerter->index_;
			
			if (exerter_index_in_subpop < 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactionDistance): interactions can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
			
			double distance = INFINITY;
			
			for (uint32_t col_index = 0; col_index < row_nnz; ++col_index)
				if ((slim_popsize_t)row_columns[col_index] == exerter_index_in_subpop)
				{
					distance = distances[col_index];
					break;
				}
			
			result_vec->set_float_no_check(distance, exerter_index);
		}
		
		return EidosValue_SP(result_vec);
	}
}

//	*********************	– (object<Individual>)nearestInteractingNeighbors(object<Individual>$ individual, [integer$ count = 1])
//
EidosValue_SP InteractionType::ExecuteMethod_nearestInteractingNeighbors(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *individual_value = p_arguments[0].get();
	EidosValue *count_value = p_arguments[1].get();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): nearestInteractingNeighbors() requires that the interaction be spatial." << EidosTerminate();
	
	// Check the individual and subpop
	Individual *individual = (Individual *)individual_value->ObjectElementAtIndex(0, nullptr);
	Subpopulation *subpop = &(individual->subpopulation_);
	slim_objectid_t subpop_id = subpop->subpopulation_id_;
	slim_popsize_t subpop_size = subpop->parent_subpop_size_;
	int ind_index = individual->index_;
	
	if (ind_index < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): interactions can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
	
	auto subpop_data_iter = data_.find(subpop_id);
	
	if ((subpop_data_iter == data_.end()) || !subpop_data_iter->second.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): nearestInteractingNeighbors() requires that the interaction has been evaluated for the subpopulation first." << EidosTerminate();
	
	// Check the count
	int64_t count = count_value->IntAtIndex(0, nullptr);
	
	if (count < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): nearestInteractingNeighbors() requires count >= 0." << EidosTerminate();
	if (count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	if (count > subpop_size)
		count = subpop_size;
	
	// Find the neighbors
	CalculateAllDistances(subpop);
	
	std::vector<Individual *> &individuals = subpop->parent_individuals_;
	InteractionsData &subpop_data = subpop_data_iter->second;
	SparseArray &sa = *subpop_data.dist_str_;
	uint32_t row_nnz;
	const uint32_t *row_columns;
	const sa_distance_t *distances;
	
	distances = sa.DistancesForRow(ind_index, &row_nnz, &row_columns);
	
	if (count >= row_nnz)
	{
		// return all of the individuals in the row
		EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->resize_no_initialize(row_nnz);
		
		for (uint32_t col_index = 0; col_index < row_nnz; ++col_index)
			result_vec->set_object_element_no_check_NORR(individuals[row_columns[col_index]], col_index);
		
		return EidosValue_SP(result_vec);
	}
	else if (count == 1)
	{
		// return the individual in the row with the smallest distance
		uint32_t min_col_index = UINT32_MAX;
		double min_distance = INFINITY;
		
		for (uint32_t col_index = 0; col_index < row_nnz; ++col_index)
			if (distances[col_index] < min_distance)
			{
				min_distance = distances[col_index];
				min_col_index = col_index;
			}
		
		if (min_distance < INFINITY)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(individuals[row_columns[min_col_index]], gSLiM_Individual_Class));
	}
	else	// (count < nnz)
	{
		// return the <count> individuals with the smallest distances
		std::vector<std::pair<uint32_t, sa_distance_t>> neighbors;
		
		for (uint32_t col_index = 0; col_index < row_nnz; ++col_index)
			neighbors.push_back(std::pair<uint32_t, sa_distance_t>(col_index, distances[col_index]));
		
		std::sort(neighbors.begin(), neighbors.end(), [](const std::pair<uint32_t, sa_distance_t> &l, const std::pair<uint32_t, sa_distance_t> &r) {
			return l.second < r.second;
		});
		
		EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->resize_no_initialize(count);
		
		for (uint32_t neighbor_index = 0; neighbor_index < count; ++neighbor_index)
		{
			Individual *ind = individuals[row_columns[neighbors[neighbor_index].first]];
			
			result_vec->set_object_element_no_check_NORR(ind, neighbor_index);
		}
		
		return EidosValue_SP(result_vec);
	}
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
}

//	*********************	– (object<Individual>)nearestNeighbors(object<Individual>$ individual, [integer$ count = 1])
//
EidosValue_SP InteractionType::ExecuteMethod_nearestNeighbors(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *individual_value = p_arguments[0].get();
	EidosValue *count_value = p_arguments[1].get();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighbors): nearestNeighbors() requires that the interaction be spatial." << EidosTerminate();
	
	// Check the individual and subpop
	Individual *individual = (Individual *)individual_value->ObjectElementAtIndex(0, nullptr);
	Subpopulation *subpop = &(individual->subpopulation_);
	slim_objectid_t subpop_id = subpop->subpopulation_id_;
	slim_popsize_t subpop_size = subpop->parent_subpop_size_;
	int ind_index = individual->index_;
	
	if (ind_index < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighbors): interactions can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
	
	auto subpop_data_iter = data_.find(subpop_id);
	
	if ((subpop_data_iter == data_.end()) || !subpop_data_iter->second.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighbors): nearestNeighbors() requires that the interaction has been evaluated for the subpopulation first." << EidosTerminate();
	
	// Check the count
	int64_t count = count_value->IntAtIndex(0, nullptr);
	
	if (count < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighbors): nearestNeighbors() requires count >= 0." << EidosTerminate();
	if (count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	if (count > subpop_size)
		count = subpop_size;
	
	// Find the neighbors
	InteractionsData &subpop_data = subpop_data_iter->second;
	
	double *position_data = subpop_data.positions_;
	double *ind_position = position_data + ind_index * SLIM_MAX_DIMENSIONALITY;
	
	EnsureKDTreePresent(subpop_data);
	
	EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->reserve((int)count);
	
	FindNeighbors(subpop, subpop_data, ind_position, (int)count, *result_vec, individual);
	
	return EidosValue_SP(result_vec);
}

//	*********************	– (object<Individual>)nearestNeighborsOfPoint(object<Subpopulation>$ subpop, float point, [integer$ count = 1])
//
EidosValue_SP InteractionType::ExecuteMethod_nearestNeighborsOfPoint(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *subpop_value = p_arguments[0].get();
	EidosValue *point_value = p_arguments[1].get();
	EidosValue *count_value = p_arguments[2].get();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighborsOfPoint): nearestNeighborsOfPoint() requires that the interaction be spatial." << EidosTerminate();
	
	// Check the subpop
	Subpopulation *subpop = (Subpopulation *)subpop_value->ObjectElementAtIndex(0, nullptr);
	slim_objectid_t subpop_id = subpop->subpopulation_id_;
	slim_popsize_t subpop_size = subpop->parent_subpop_size_;
	auto subpop_data_iter = data_.find(subpop_id);
	
	if ((subpop_data_iter == data_.end()) || !subpop_data_iter->second.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighborsOfPoint): nearestNeighborsOfPoint() requires that the interaction has been evaluated for the subpopulation first." << EidosTerminate();
	
	// Check the point
	if (point_value->Count() < spatiality_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighborsOfPoint): nearestNeighborsOfPoint() requires a point vector with at least as many elements as the InteractionType spatiality." << EidosTerminate();
	
	double point_array[3];
	
	for (int point_index = 0; point_index < spatiality_; ++point_index)
		point_array[point_index] = point_value->FloatAtIndex(point_index, nullptr);
	
	// Check the count
	int64_t count = count_value->IntAtIndex(0, nullptr);
	
	if (count < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighborsOfPoint): nearestNeighborsOfPoint() requires count >= 0." << EidosTerminate();
	
	if (count > subpop_size)
		count = subpop_size;
	
	if (count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	// Find the neighbors
	InteractionsData &subpop_data = subpop_data_iter->second;
	
	EnsureKDTreePresent(subpop_data);
	
	EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->reserve((int)count);
	
	FindNeighbors(subpop, subpop_data, point_array, (int)count, *result_vec, nullptr);
	
	return EidosValue_SP(result_vec);
}

//	*********************	- (void)setInteractionFunction(string$ functionType, ...)
//
EidosValue_SP InteractionType::ExecuteMethod_setInteractionFunction(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *functionType_value = p_arguments[0].get();
	
	std::string if_type_string = functionType_value->StringAtIndex(0, nullptr);
	IFType if_type;
	int expected_if_param_count = 0;
	std::vector<double> if_parameters;
	std::vector<std::string> if_strings;
	
	if (AnyEvaluated())
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setInteractionFunction): setInteractionFunction() cannot be called while the interaction is being evaluated; call unevaluate() first, or call setInteractionFunction() prior to evaluation of the interaction." << EidosTerminate();
	
	if (if_type_string.compare(gStr_f) == 0)
	{
		if_type = IFType::kFixed;
		expected_if_param_count = 1;
	}
	else if (if_type_string.compare(gStr_l) == 0)
	{
		if_type = IFType::kLinear;
		expected_if_param_count = 1;
		
		if (std::isinf(max_distance_) || (max_distance_ <= 0.0))
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setInteractionFunction): interaction type 'l' cannot be set in setInteractionFunction() unless a finite maximum interaction distance greater than zero has been set." << EidosTerminate();
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
	else if (if_type_string.compare(gEidosStr_c) == 0)
	{
		if_type = IFType::kCauchy;
		expected_if_param_count = 2;
	}
	else
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setInteractionFunction): setInteractionFunction() functionType \"" << if_type_string << "\" must be \"f\", \"l\", \"e\", \"n\", or \"c\"." << EidosTerminate();
	
	if ((spatiality_ == 0) && (if_type != IFType::kFixed))
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setInteractionFunction): setInteractionFunction() requires functionType 'f' for non-spatial interactions." << EidosTerminate();
	
	if ((int)p_arguments.size() != 1 + expected_if_param_count)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setInteractionFunction): setInteractionFunction() functionType \"" << if_type << "\" requires exactly " << expected_if_param_count << " DFE parameter" << (expected_if_param_count == 1 ? "" : "s") << "." << EidosTerminate();
	
	for (int if_param_index = 0; if_param_index < expected_if_param_count; ++if_param_index)
	{
		EidosValue *if_param_value = p_arguments[1 + if_param_index].get();
		EidosValueType if_param_type = if_param_value->Type();
		
		if ((if_param_type != EidosValueType::kValueFloat) && (if_param_type != EidosValueType::kValueInt))
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setInteractionFunction): setInteractionFunction() requires that the parameters for this interaction function be of type numeric (integer or float)." << EidosTerminate();
		
		if_parameters.emplace_back(if_param_value->FloatAtIndex(0, nullptr));
	}
	
	// Bounds-check the IF parameters in the cases where there is a hard bound
	switch (if_type)
	{
		case IFType::kFixed:
			// no limits on fixed IFs; doesn't make much sense to use 0.0, but it's not illegal
			break;
		case IFType::kLinear:
			// no limits on linear IFs; doesn't make much sense to use 0.0, but it's not illegal
			break;
		case IFType::kExponential:
			// no limits on exponential IFs; a shape of 0.0 doesn't make much sense, but it's not illegal
			break;
		case IFType::kNormal:
			// no limits on the maximum strength (although 0.0 doesn't make much sense); sd must be >= 0
			if (if_parameters[1] < 0.0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setInteractionFunction): an interaction function of type \"n\" must have a standard deviation parameter >= 0." << EidosTerminate();
			break;
		case IFType::kCauchy:
			// no limits on the maximum strength (although 0.0 doesn't make much sense); scale must be > 0
			if (if_parameters[1] <= 0.0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setInteractionFunction): an interaction function of type \"c\" must have a scale parameter > 0." << EidosTerminate();
			break;
	}
	
	// Everything seems to be in order, so replace our IF info with the new info
	if_type_ = if_type;
	if_param1_ = ((if_parameters.size() >= 1) ? if_parameters[0] : 0.0);
	if_param2_ = ((if_parameters.size() >= 2) ? if_parameters[1] : 0.0);
	
	// mark that interaction types changed, so they get redisplayed in SLiMgui
	SLiMSim &sim = SLiM_GetSimFromInterpreter(p_interpreter);
	
	sim.interaction_types_changed_ = true;
	
	return gStaticEidosValueVOID;
}

//	*********************	– (float)strength(object<Individual>$ receiver, [No<Individual> exerters = NULL])
//
EidosValue_SP InteractionType::ExecuteMethod_strength(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *receiver_value = p_arguments[0].get();
	EidosValue *exerters_value = p_arguments[1].get();
	
	int exerter_count = exerters_value->Count();
	
	// receiver_value is guaranteed to be singleton; let's get the info on it
	Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
	Subpopulation *subpop1 = &(receiver->subpopulation_);
	slim_objectid_t subpop1_id = subpop1->subpopulation_id_;
	slim_popsize_t subpop1_size = subpop1->parent_subpop_size_;
	int receiver_index = receiver->index_;
	
	if (receiver_index < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_strength): interactions can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
	
	auto subpop_data_iter = data_.find(subpop1_id);
	
	if ((subpop_data_iter == data_.end()) || !subpop_data_iter->second.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_strength): strength() requires that the interaction has been evaluated for the subpopulation first." << EidosTerminate();
	
	InteractionsData &subpop_data = subpop_data_iter->second;
	
	if (spatiality_)
	{
		CalculateAllStrengths(subpop1);
		
		SparseArray &sa = *subpop_data.dist_str_;
		uint32_t row_nnz;
		const uint32_t *row_columns;
		const sa_strength_t *strengths;
		
		strengths = sa.StrengthsForRow(receiver_index, &row_nnz, &row_columns);
		
		if (exerters_value->Type() == EidosValueType::kValueNULL)
		{
			// NULL means return strengths to individuals1 (which must be singleton) from all individuals in the subpopulation
			// Make a vector big enough, initialize it to 0, and fill in values from the sparse array
			EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(subpop1_size);
			double *result_ptr = result_vec->data();
			
			EIDOS_BZERO(result_ptr, subpop1_size * sizeof(double));
			
			for (uint32_t col_index = 0; col_index < row_nnz; ++col_index)
				*(result_ptr + row_columns[col_index]) = strengths[col_index];
			
			return EidosValue_SP(result_vec);
		}
		else
		{
			// Otherwise, individuals1 is singleton, and exerters_value is any length, so we loop over exerters_value
			// Each individual in exerters_value requires a linear search through the row, unfortunately
			EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerter_count);
			
			for (int exerter_index = 0; exerter_index < exerter_count; ++exerter_index)
			{
				Individual *exerter = (Individual *)exerters_value->ObjectElementAtIndex(exerter_index, nullptr);
				
				if (subpop1 != &(exerter->subpopulation_))
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_strength): strength() requires that all individuals be in the same subpopulation." << EidosTerminate();
				
				slim_popsize_t exerter_index_in_subpop = (uint32_t)exerter->index_;
				
				if (exerter_index_in_subpop < 0)
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_strength): interactions can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
				
				double strength = 0;
				
				for (uint32_t col_index = 0; col_index < row_nnz; ++col_index)
					if ((slim_popsize_t)row_columns[col_index] == exerter_index_in_subpop)
					{
						strength = strengths[col_index];
						break;
					}
				
				result_vec->set_float_no_check(strength, exerter_index);
			}
			
			return EidosValue_SP(result_vec);
		}
	}
	else
	{
		//
		// Non-spatial case; no distances used.  We have to worry about sex-segregation; it is not handled for us.
		//
		std::vector<SLiMEidosBlock*> &callbacks = subpop_data.evaluation_interaction_callbacks_;
		bool no_callbacks = (callbacks.size() == 0);
		
		if (exerters_value->Type() == EidosValueType::kValueNULL)
		{
			// NULL means return strengths to individuals1 (which must be singleton) from all individuals in the subpopulation
			EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(subpop1_size);
			
			if ((receiver_sex_ == IndividualSex::kUnspecified) || (receiver_sex_ == receiver->sex_))
			{
				// The receiver can receive interactions, so evaluate them
				for (int exerter_index = 0; exerter_index < subpop1_size; ++exerter_index)
				{
					double strength = 0;
					
					if (exerter_index != receiver_index)
					{
						Individual *exerter = subpop1->parent_individuals_[exerter_index];
						
						if ((exerter_sex_ == IndividualSex::kUnspecified) || (exerter_sex_ == exerter->sex_))
						{
							if (no_callbacks)
								strength = CalculateStrengthNoCallbacks(NAN);
							else
								strength = CalculateStrengthWithCallbacks(NAN, receiver, exerter, subpop1, callbacks);
						}
					}
					
					result_vec->set_float_no_check(strength, exerter_index);
				}
			}
			else
			{
				// This individual can't receive this interaction type at all, so the result vector is all zero
				EIDOS_BZERO(result_vec->data(), subpop1_size * sizeof(double));
			}
			
			return EidosValue_SP(result_vec);
		}
		else
		{
			// Otherwise, individuals1 is singleton, and exerters_value is any length, so we loop over exerters_value
			EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerter_count);
			
			if ((receiver_sex_ == IndividualSex::kUnspecified) || (receiver_sex_ == receiver->sex_))
			{
				// The receiver can receive interactions, so evaluate them
				for (int exerter_index = 0; exerter_index < exerter_count; ++exerter_index)
				{
					Individual *exerter = (Individual *)exerters_value->ObjectElementAtIndex(exerter_index, nullptr);
					
					if (subpop1 != &(exerter->subpopulation_))
						EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_strength): strength() requires that all individuals be in the same subpopulation." << EidosTerminate();
					
					slim_popsize_t exerter_index_in_subpop = exerter->index_;
					
					if (exerter_index_in_subpop < 0)
						EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_strength): interactions can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
					
					double strength = 0;
					
					if (exerter_index_in_subpop != receiver_index)
					{
						if ((exerter_sex_ == IndividualSex::kUnspecified) || (exerter_sex_ == exerter->sex_))
						{
							if (no_callbacks)
								strength = CalculateStrengthNoCallbacks(NAN);
							else
								strength = CalculateStrengthWithCallbacks(NAN, receiver, exerter, subpop1, callbacks);
						}
					}
					
					result_vec->set_float_no_check(strength, exerter_index);
				}
			}
			else
			{
				// This individual can't receive this interaction type at all, so the result vector is all zero
				EIDOS_BZERO(result_vec->data(), exerter_count * sizeof(double));
			}
			
			return EidosValue_SP(result_vec);
		}
	}
}

//	*********************	– (float)totalOfNeighborStrengths(object<Individual> individuals)
//
EidosValue_SP InteractionType::ExecuteMethod_totalOfNeighborStrengths(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *individuals_value = p_arguments[0].get();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): totalOfNeighborStrengths() requires that the interaction be spatial." << EidosTerminate();
	
	EidosValue *individuals = individuals_value;
	int count = individuals->Count();
	
	if (count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	// individuals is guaranteed to have at least one value
	Individual *first_ind = (Individual *)individuals->ObjectElementAtIndex(0, nullptr);
	Subpopulation *subpop = &(first_ind->subpopulation_);
	slim_objectid_t subpop_id = subpop->subpopulation_id_;
	auto subpop_data_iter = data_.find(subpop_id);
	
	if ((subpop_data_iter == data_.end()) || !subpop_data_iter->second.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): totalOfNeighborStrengths() requires that the interaction has been evaluated for the subpopulation first." << EidosTerminate();
	
	InteractionsData &subpop_data = subpop_data_iter->second;
	
	CalculateAllStrengths(subpop);
	
	if (count == 1)
	{
		// Just one value, so we can return a singleton and skip some work
		SparseArray &sa = *subpop_data.dist_str_;
		slim_popsize_t ind_index_in_subpop = first_ind->index_;
		
		if (ind_index_in_subpop < 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): interactions can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
		
		// Get the sparse array data
		uint32_t row_nnz;
		const uint32_t *row_columns;
		const sa_strength_t *strengths;
		
		strengths = sa.StrengthsForRow(ind_index_in_subpop, &row_nnz, &row_columns);
		
		// Total the interaction strengths
		double total_strength = 0.0;
		
		for (uint32_t col_index = 0; col_index < row_nnz; ++col_index)
			total_strength += strengths[col_index];
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(total_strength));
	}
	else
	{
		// Loop over the requested individuals and get the totals
		EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(count);
		SparseArray &sa = *subpop_data.dist_str_;
		
		for (int ind_index = 0; ind_index < count; ++ind_index)
		{
			Individual *individual = (Individual *)individuals->ObjectElementAtIndex(ind_index, nullptr);
			
			if (subpop != &(individual->subpopulation_))
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): totalOfNeighborStrengths() requires that all individuals be in the same subpopulation." << EidosTerminate();
			
			slim_popsize_t ind_index_in_subpop = individual->index_;
			
			if (ind_index_in_subpop < 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): interactions can only be calculated for individuals that are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
			
			// Get the sparse array data
			uint32_t row_nnz;
			const uint32_t *row_columns;
			const sa_strength_t *strengths;
			
			strengths = sa.StrengthsForRow(ind_index_in_subpop, &row_nnz, &row_columns);
			
			// Total the interaction strengths
			double total_strength = 0.0;
			
			for (uint32_t col_index = 0; col_index < row_nnz; ++col_index)
				total_strength += strengths[col_index];
			
			result_vec->set_float_no_check(total_strength, ind_index);
		}
		
		return EidosValue_SP(result_vec);
	}
}

//	*********************	– (void)unevaluate(void)
//
EidosValue_SP InteractionType::ExecuteMethod_unevaluate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	Invalidate();
	
	return gStaticEidosValueVOID;
}


//
//	InteractionType_Class
//
#pragma mark -
#pragma mark InteractionType_Class
#pragma mark -

class InteractionType_Class : public EidosDictionary_Class
{
public:
	InteractionType_Class(const InteractionType_Class &p_original) = delete;	// no copy-construct
	InteractionType_Class& operator=(const InteractionType_Class&) = delete;	// no copying
	inline InteractionType_Class(void) { }
	
	virtual const std::string &ElementType(void) const override;
	
	virtual const std::vector<EidosPropertySignature_CSP> *Properties(void) const override;
	virtual const std::vector<EidosMethodSignature_CSP> *Methods(void) const override;
};

EidosObjectClass *gSLiM_InteractionType_Class = new InteractionType_Class();


const std::string &InteractionType_Class::ElementType(void) const
{
	return gStr_InteractionType;
}

const std::vector<EidosPropertySignature_CSP> *InteractionType_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<EidosPropertySignature_CSP>(*EidosDictionary_Class::Properties());
		
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_id,				true,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(InteractionType::GetProperty_Accelerated_id));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_reciprocal,		true,	kEidosValueMaskLogical | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_sexSegregation,	true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_spatiality,		true,	kEidosValueMaskString | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_maxDistance,	false,	kEidosValueMaskFloat | kEidosValueMaskSingleton)));
		properties->emplace_back((EidosPropertySignature *)(new EidosPropertySignature(gStr_tag,			false,	kEidosValueMaskInt | kEidosValueMaskSingleton))->DeclareAcceleratedGet(InteractionType::GetProperty_Accelerated_tag));
		
		std::sort(properties->begin(), properties->end(), CompareEidosPropertySignatures);
	}
	
	return properties;
}

const std::vector<EidosMethodSignature_CSP> *InteractionType_Class::Methods(void) const
{
	static std::vector<EidosMethodSignature_CSP> *methods = nullptr;
	
	if (!methods)
	{
		methods = new std::vector<EidosMethodSignature_CSP>(*EidosDictionary_Class::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_distance, kEidosValueMaskFloat))->AddObject("individuals1", gSLiM_Individual_Class)->AddObject_ON("individuals2", gSLiM_Individual_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_distanceToPoint, kEidosValueMaskFloat))->AddObject("individuals1", gSLiM_Individual_Class)->AddFloat("point"));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_drawByStrength, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("individual", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_evaluate, kEidosValueMaskVOID))->AddObject_ON("subpops", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddLogical_OS("immediate", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_interactingNeighborCount, kEidosValueMaskInt))->AddObject("individuals", gSLiM_Individual_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_interactionDistance, kEidosValueMaskFloat))->AddObject_S("receiver", gSLiM_Individual_Class)->AddObject_ON("exerters", gSLiM_Individual_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_nearestInteractingNeighbors, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("individual", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_nearestNeighbors, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("individual", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_nearestNeighborsOfPoint, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("subpop", gSLiM_Subpopulation_Class)->AddFloat("point")->AddInt_OS("count", gStaticEidosValue_Integer1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setInteractionFunction, kEidosValueMaskVOID))->AddString_S("functionType")->AddEllipsis());
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_strength, kEidosValueMaskFloat))->AddObject_S("receiver", gSLiM_Individual_Class)->AddObject_ON("exerters", gSLiM_Individual_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_totalOfNeighborStrengths, kEidosValueMaskFloat))->AddObject("individuals", gSLiM_Individual_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_unevaluate, kEidosValueMaskVOID)));
		
		std::sort(methods->begin(), methods->end(), CompareEidosCallSignatures);
	}
	
	return methods;
}


//
//	_InteractionsData
//
#pragma mark -
#pragma mark _InteractionsData
#pragma mark -

_InteractionsData::_InteractionsData(_InteractionsData&& p_source)
{
	evaluated_ = p_source.evaluated_;
	evaluation_interaction_callbacks_.swap(p_source.evaluation_interaction_callbacks_);
	individual_count_ = p_source.individual_count_;
	first_male_index_ = p_source.first_male_index_;
	kd_node_count_ = p_source.kd_node_count_;
	positions_ = p_source.positions_;
	dist_str_ = p_source.dist_str_;
	kd_nodes_ = p_source.kd_nodes_;
	kd_root_ = p_source.kd_root_;
	
	p_source.evaluated_ = false;
	p_source.evaluation_interaction_callbacks_.clear();
	p_source.individual_count_ = 0;
	p_source.first_male_index_ = 0;
	p_source.kd_node_count_ = 0;
	p_source.positions_ = nullptr;
	p_source.dist_str_ = nullptr;
	p_source.kd_nodes_ = nullptr;
	p_source.kd_root_ = nullptr;
}

_InteractionsData& _InteractionsData::operator=(_InteractionsData&& p_source)
{
	if (this != &p_source)  
	{
		if (positions_)
			free(positions_);
		if (dist_str_)
			delete dist_str_;
		if (kd_nodes_)
			free(kd_nodes_);
		
		evaluated_ = p_source.evaluated_;
		evaluation_interaction_callbacks_.swap(p_source.evaluation_interaction_callbacks_);
		individual_count_ = p_source.individual_count_;
		first_male_index_ = p_source.first_male_index_;
		kd_node_count_ = p_source.kd_node_count_;
		positions_ = p_source.positions_;
		dist_str_ = p_source.dist_str_;
		kd_nodes_ = p_source.kd_nodes_;
		kd_root_ = p_source.kd_root_;
		
		p_source.evaluated_ = false;
		p_source.evaluation_interaction_callbacks_.clear();
		p_source.individual_count_ = 0;
		p_source.first_male_index_ = 0;
		p_source.kd_node_count_ = 0;
		p_source.positions_ = nullptr;
		p_source.dist_str_ = nullptr;
		p_source.kd_nodes_ = nullptr;
		p_source.kd_root_ = nullptr;
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
	
	if (dist_str_)
	{
		delete dist_str_;
		dist_str_ = nullptr;
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































