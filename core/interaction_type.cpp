//
//  interaction_type.cpp
//  SLiM
//
//  Created by Ben Haller on 2/25/17.
//  Copyright (c) 2017-2022 Philipp Messer.  All rights reserved.
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
#include "community.h"
#include "species.h"
#include "slim_globals.h"
#include "sparse_vector.h"

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

std::vector<SparseVector *> InteractionType::s_freed_sparse_vectors_;
#if DEBUG
int InteractionType::s_sparse_vector_count_ = 0;
#endif

InteractionType::InteractionType(Community &p_community, slim_objectid_t p_interaction_type_id, std::string p_spatiality_string, bool p_reciprocal, double p_max_distance, IndividualSex p_receiver_sex, IndividualSex p_exerter_sex) :
	self_symbol_(EidosStringRegistry::GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('i', p_interaction_type_id)),
			 EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_InteractionType_Class))),
	spatiality_string_(p_spatiality_string), reciprocal_(p_reciprocal), max_distance_(p_max_distance), max_distance_sq_(p_max_distance * p_max_distance), receiver_sex_(p_receiver_sex), exerter_sex_(p_exerter_sex), if_type_(IFType::kFixed), if_param1_(1.0), if_param2_(0.0),
	community_(p_community), interaction_type_id_(p_interaction_type_id)
{
	// Figure out our spatiality, which is the number of spatial dimensions we actively use for distances
	if (spatiality_string_ == "")			{ spatiality_ = 0; required_dimensionality_ = 0; }
	else if (spatiality_string_ == "x")		{ spatiality_ = 1; required_dimensionality_ = 1; }
	else if (spatiality_string_ == "y")		{ spatiality_ = 1; required_dimensionality_ = 2; }
	else if (spatiality_string_ == "z")		{ spatiality_ = 1; required_dimensionality_ = 3; }
	else if (spatiality_string_ == "xy")	{ spatiality_ = 2; required_dimensionality_ = 2; }
	else if (spatiality_string_ == "xz")	{ spatiality_ = 2; required_dimensionality_ = 3; }
	else if (spatiality_string_ == "yz")	{ spatiality_ = 2; required_dimensionality_ = 3; }
	else if (spatiality_string_ == "xyz")	{ spatiality_ = 3; required_dimensionality_ = 3; }
	else
		EIDOS_TERMINATION << "ERROR (InteractionType::InteractionType): initializeInteractionType() spatiality \"" << spatiality_string_ << "\" must be \"\", \"x\", \"y\", \"z\", \"xy\", \"xz\", \"yz\", or \"xyz\"." << EidosTerminate();
	
	// In the single-species case, we want to do some checks up front for backward compatibility/reproducibility;
	// in the multispecies case these must be deferred to evaluate() time, since they are specific to one evaluated species
	Species *single_species = (community_.is_explicit_species_ ? nullptr : community_.AllSpecies()[0]);
	
	if (single_species)
		if (required_dimensionality_ > single_species->SpatialDimensionality())
			EIDOS_TERMINATION << "ERROR (InteractionType::InteractionType): initializeInteractionType() spatiality cannot utilize spatial dimensions beyond those set in initializeSLiMOptions()." << EidosTerminate();
	
	if (max_distance_ < 0.0)
		EIDOS_TERMINATION << "ERROR (InteractionType::InteractionType): initializeInteractionType() maxDistance must be >= 0.0." << EidosTerminate();
	if ((required_dimensionality_ == 0) && (!std::isinf(max_distance_) || (max_distance_ < 0.0)))
		EIDOS_TERMINATION << "ERROR (InteractionType::InteractionType): initializeInteractionType() maxDistance must be INF for non-spatial interactions." << EidosTerminate();
	
	if (single_species)
		if (((receiver_sex_ != IndividualSex::kUnspecified) || (exerter_sex_ != IndividualSex::kUnspecified)) && !single_species->SexEnabled())
			EIDOS_TERMINATION << "ERROR (InteractionType::InteractionType): initializeInteractionType() sexSegregation value other than '**' are unsupported in non-sexual simulation." << EidosTerminate();
	
	if ((required_dimensionality_ > 0) && std::isinf(max_distance_))
	{
		if (!gEidosSuppressWarnings)
		{
			if (!community_.warned_no_max_distance_)
			{
				SLIM_ERRSTREAM << "#WARNING (Community::ExecuteContextFunction_initializeInteractionType): initializeInteractionType() called to configure a spatial interaction type with no maximum distance; this may result in very poor performance." << std::endl;
				community_.warned_no_max_distance_ = true;
			}
		}
	}
}

InteractionType::~InteractionType(void)
{
	if (clipped_integral_)
	{
		free(clipped_integral_);
		clipped_integral_ = nullptr;
	}
}

void InteractionType::EvaluateSubpopulation(Subpopulation *p_subpop)
{
	if (p_subpop->has_been_removed_)
		EIDOS_TERMINATION << "ERROR (InteractionType::EvaluateSubpopulation): you cannot evaluate an InteractionType for a subpopulation that has been removed." << EidosTerminate();

	// We evaluate for receiver and exerter subpopulations, so that all interaction evaluation state (except for
	// interaction() callbacks) is frozen at the same time.  Evaluate is necessary because the k-d tree is built
	// once and used to serve many queries, typically, and so it must be built based upon a fixed state snapshot.
	Species &species = p_subpop->species_;
	slim_objectid_t subpop_id = p_subpop->subpopulation_id_;
	slim_popsize_t subpop_size = p_subpop->parent_subpop_size_;
	Individual **subpop_individuals = p_subpop->parent_individuals_.data();
	
	// Check that the exerter subpopulation is compatible with the configuration of this interaction type
	CheckSpeciesCompatibility(p_subpop->species_);
	
	// Find/create a data object for this exerter
	auto data_iter = data_.find(subpop_id);
	InteractionsData *subpop_data;
	
	if (data_iter == data_.end())
	{
		// No entry in our map table for this subpop_id, so we need to make a new entry
		subpop_data = &(data_.emplace(subpop_id, InteractionsData(subpop_size, p_subpop->parent_first_male_index_)).first->second);
	}
	else
	{
		// There is an existing entry, so we need to rehabilitate that entry by recycling its elements safely
		subpop_data = &(data_iter->second);
		
		subpop_data->individual_count_ = subpop_size;
		subpop_data->first_male_index_ = p_subpop->parent_first_male_index_;
		subpop_data->kd_node_count_ = 0;
		
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
	
	// At a minimum, fetch positional data from the subpopulation; this is guaranteed to be present (for spatiality > 0)
	if (spatiality_ > 0)
	{
		double *positions = (double *)malloc(subpop_size * SLIM_MAX_DIMENSIONALITY * sizeof(double));
		if (!positions)
			EIDOS_TERMINATION << "ERROR (InteractionType::EvaluateSubpopulation): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
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
			species.SpatialPeriodicity(&subpop_data->periodic_x_, nullptr, nullptr);
			subpop_data->bounds_x1_ = p_subpop->bounds_x1_;
			
			if (!subpop_data->periodic_x_)
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
			species.SpatialPeriodicity(nullptr, &subpop_data->periodic_x_, nullptr);
			subpop_data->bounds_x1_ = p_subpop->bounds_y1_;
			
			if (!subpop_data->periodic_x_)
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
			species.SpatialPeriodicity(nullptr, nullptr, &subpop_data->periodic_x_);
			subpop_data->bounds_x1_ = p_subpop->bounds_z1_;
			
			if (!subpop_data->periodic_x_)
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
			species.SpatialPeriodicity(&subpop_data->periodic_x_, &subpop_data->periodic_y_, nullptr);
			subpop_data->bounds_x1_ = p_subpop->bounds_x1_;
			subpop_data->bounds_y1_ = p_subpop->bounds_y1_;
			
			if (!subpop_data->periodic_x_ && !subpop_data->periodic_y_)
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
					
					if ((subpop_data->periodic_x_ && ((coord1 < 0.0) || (coord1 > coord1_bound))) ||
						(subpop_data->periodic_y_ && ((coord2 < 0.0) || (coord2 > coord2_bound))))
						out_of_bounds_seen = true;
					
					ind_positions[0] = coord1;
					ind_positions[1] = coord2;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
		}
		else if (spatiality_string_ == "xz")
		{
			species.SpatialPeriodicity(&subpop_data->periodic_x_, nullptr, &subpop_data->periodic_y_);
			subpop_data->bounds_x1_ = p_subpop->bounds_x1_;
			subpop_data->bounds_y1_ = p_subpop->bounds_z1_;
			
			if (!subpop_data->periodic_x_ && !subpop_data->periodic_y_)
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
					
					if ((subpop_data->periodic_x_ && ((coord1 < 0.0) || (coord1 > coord1_bound))) ||
						(subpop_data->periodic_y_ && ((coord2 < 0.0) || (coord2 > coord2_bound))))
						out_of_bounds_seen = true;
					
					ind_positions[0] = coord1;
					ind_positions[1] = coord2;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
		}
		else if (spatiality_string_ == "yz")
		{
			species.SpatialPeriodicity(nullptr, &subpop_data->periodic_x_, &subpop_data->periodic_y_);
			subpop_data->bounds_x1_ = p_subpop->bounds_y1_;
			subpop_data->bounds_y1_ = p_subpop->bounds_z1_;
			
			if (!subpop_data->periodic_x_ && !subpop_data->periodic_y_)
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
					
					if ((subpop_data->periodic_x_ && ((coord1 < 0.0) || (coord1 > coord1_bound))) ||
						(subpop_data->periodic_y_ && ((coord2 < 0.0) || (coord2 > coord2_bound))))
						out_of_bounds_seen = true;
					
					ind_positions[0] = coord1;
					ind_positions[1] = coord2;
					++ind_index; ++individual; ind_positions += SLIM_MAX_DIMENSIONALITY;
				}
			}
		}
		else if (spatiality_string_ == "xyz")
		{
			species.SpatialPeriodicity(&subpop_data->periodic_x_, &subpop_data->periodic_y_, &subpop_data->periodic_z_);
			subpop_data->bounds_x1_ = p_subpop->bounds_x1_;
			subpop_data->bounds_y1_ = p_subpop->bounds_y1_;
			subpop_data->bounds_z1_ = p_subpop->bounds_z1_;
			
			if (!subpop_data->periodic_x_ && !subpop_data->periodic_y_ && !subpop_data->periodic_z_)
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
					
					if ((subpop_data->periodic_x_ && ((coord1 < 0.0) || (coord1 > coord1_bound))) ||
						(subpop_data->periodic_y_ && ((coord2 < 0.0) || (coord2 > coord2_bound))) ||
						(subpop_data->periodic_z_ && ((coord3 < 0.0) || (coord3 > coord3_bound))))
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
	if ((subpop_data->periodic_x_ && (subpop_data->bounds_x1_ <= max_distance_ * 2.0)) ||
		(subpop_data->periodic_y_ && (subpop_data->bounds_y1_ <= max_distance_ * 2.0)) ||
		(subpop_data->periodic_z_ && (subpop_data->bounds_z1_ <= max_distance_ * 2.0)))
		EIDOS_TERMINATION << "ERROR (InteractionType::EvaluateSubpopulation): maximum interaction distance is greater than or equal to half of the spatial extent of a periodic spatial dimension, which would allow an individual to participate in more than one interaction with a single individual.  When periodic boundaries are used, the maximum interaction distance of interaction types involving periodic dimensions must be less than half of the spatial extent of those dimensions." << EidosTerminate();
	
	// Cache the interaction() callbacks applicable at this moment, for the given subpopulation and this interaction type.
	// Note that interaction() callbacks are non-species-specific, so we fetch from the Community with species nullptr.
	// Callbacks used depend upon the exerter subpopulation, so this is snapping the callbacks for subpop as exerters;
	// the subpopulation of receivers does not influence the choice of which callbacks are used.
	subpop_data->evaluation_interaction_callbacks_ = community_.ScriptBlocksMatching(community_.Tick(), SLiMEidosBlockType::SLiMEidosInteractionCallback, -1, interaction_type_id_, subpop_id, nullptr);
	
	// Note that we do not create the k-d tree here.  Non-spatial models will never have a k-d tree; spatial models may or
	// may not need one, depending upon what methods are called by the client, which may vary cycle by cycle.
	// Also, receiver subpopulations need to be evaluated too, but (if used only for receivers) will not require a k-d tree.
	// Methods that need the k-d tree must therefore call EnsureKDTreePresent() prior to using it.
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

void InteractionType::_InvalidateData(InteractionsData &data)
{
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

void InteractionType::Invalidate(void)
{
	// Called by SLiM when the old generation goes away; should invalidate all evaluation.  We avoid actually freeing the
	// big blocks if possible, though, since that can incur large overhead from madvise() – see header comments.  We do free
	// the positional data and the k-d tree, though, in an attempt to make fatal errors occur if somebody doesn't manage
	// the buffers and evaluated state correctly.  They should be smaller, and thus not trigger madvise(), anyway.
	for (auto &data_iter : data_)
		_InvalidateData(data_iter.second);
}

void InteractionType::InvalidateForSpecies(Species *p_invalid_species)
{
	// This is like Invalidate(), but invalidates only data associated with a given species
	for (auto &data_iter : data_)
	{
		slim_objectid_t subpop_id = data_iter.first;
		Subpopulation *subpop = community_.SubpopulationWithID(subpop_id);
		
		if (subpop)
		{
			Species *species = &subpop->species_;
			
			if (species == p_invalid_species)
				_InvalidateData(data_iter.second);
		}
	}
}

void InteractionType::InvalidateForSubpopulation(Subpopulation *p_invalid_subpop)
{
	// This is like Invalidate(), but invalidates only data associated with a given subpop
	for (auto &data_iter : data_)
	{
		slim_objectid_t subpop_id = data_iter.first;
		Subpopulation *subpop = community_.SubpopulationWithID(subpop_id);
		
		if (subpop == p_invalid_subpop)
			_InvalidateData(data_iter.second);
	}
}

void InteractionType::CheckSpeciesCompatibility(Species &species)
{
	// This checks that a given receiver subpop is compatible with this interaction type
	if (required_dimensionality_ > species.SpatialDimensionality())
		EIDOS_TERMINATION << "ERROR (InteractionType::CheckSpeciesCompatibility): the exerter or receiver species has insufficient dimensionality to be used with this interaction type." << EidosTerminate();
	
	if (((receiver_sex_ != IndividualSex::kUnspecified) || (exerter_sex_ != IndividualSex::kUnspecified)) && !species.SexEnabled())
		EIDOS_TERMINATION << "ERROR (InteractionType::CheckSpeciesCompatibility): sexSegregation value other than '**' are unsupported with non-sexual exerter/receiver species." << EidosTerminate();
}

void InteractionType::CheckSpatialCompatibility(Subpopulation *receiver_subpop, Subpopulation *exerter_subpop)
{
	// This checks that two subpops can legally interact with each other; it should always be guaranteed before a query is served
	if (receiver_subpop == exerter_subpop)
		return;
	
	// Check for identical dimensionality
	int dimensionality_ex = exerter_subpop->species_.SpatialDimensionality();
	int dimensionality_re = receiver_subpop->species_.SpatialDimensionality();
	
	if (dimensionality_ex != dimensionality_re)
		EIDOS_TERMINATION << "ERROR (InteractionType::CheckSpatialCompatibility): the exerter and receiver subpopulations have different dimensionalities." << EidosTerminate();
	
	// Check for identical periodicity
	bool periodic_ex_x, periodic_ex_y, periodic_ex_z;
	bool periodic_re_x, periodic_re_y, periodic_re_z;
	
	exerter_subpop->species_.SpatialPeriodicity(&periodic_ex_x, &periodic_ex_y, &periodic_ex_z);
	receiver_subpop->species_.SpatialPeriodicity(&periodic_re_x, &periodic_re_y, &periodic_re_z);
	
	if ((periodic_ex_x != periodic_re_x) || (periodic_ex_y != periodic_re_y) || (periodic_ex_z != periodic_re_z))
		EIDOS_TERMINATION << "ERROR (InteractionType::CheckSpatialCompatibility): the exerter and receiver subpopulations have different periodicities." << EidosTerminate();
	
	// Check for identical bounds, required only when the dimension in question is periodic
	// For non-periodic dimensions it is assumed that the bounds, even if they don't match, exist in the same coordinate system
	if (periodic_ex_x && ((exerter_subpop->bounds_x0_ != receiver_subpop->bounds_x0_) || (exerter_subpop->bounds_x1_ != receiver_subpop->bounds_x1_)))
		EIDOS_TERMINATION << "ERROR (InteractionType::CheckSpatialCompatibility): the exerter and receiver subpopulations have different periodic x boundaries." << EidosTerminate();
	
	if (periodic_ex_y && ((exerter_subpop->bounds_y0_ != receiver_subpop->bounds_y0_) || (exerter_subpop->bounds_y1_ != receiver_subpop->bounds_y1_)))
		EIDOS_TERMINATION << "ERROR (InteractionType::CheckSpatialCompatibility): the exerter and receiver subpopulations have different periodic y boundaries." << EidosTerminate();
	
	if (periodic_ex_z && ((exerter_subpop->bounds_z0_ != receiver_subpop->bounds_z0_) || (exerter_subpop->bounds_z1_ != receiver_subpop->bounds_z1_)))
		EIDOS_TERMINATION << "ERROR (InteractionType::CheckSpatialCompatibility): the exerter and receiver subpopulations have different periodic z boundaries." << EidosTerminate();
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
		EIDOS_TERMINATION << "ERROR (InteractionType::CalculateDistance): (internal error) calculation of distances requires that the interaction be spatial." << EidosTerminate();
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
		if (p_subpop_data.periodic_x_)
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
		
		if (p_subpop_data.periodic_x_)
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
		
		if (p_subpop_data.periodic_y_)
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
		
		if (p_subpop_data.periodic_x_)
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
		
		if (p_subpop_data.periodic_y_)
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
		
		if (p_subpop_data.periodic_z_)
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
		EIDOS_TERMINATION << "ERROR (InteractionType::CalculateDistanceWithPeriodicity): (internal error) calculation of distances requires that the interaction be spatial." << EidosTerminate();
}

double InteractionType::CalculateStrengthNoCallbacks(double p_distance)
{
	// CAUTION: This method should only be called when p_distance <= max_distance_ (or is NAN).
	// It is the caller's responsibility to do that filtering, for performance reasons!
	// The caller is also responsible for guaranteeing that this is not a self-interaction,
	// and that it is not ruled out by sex-selectivity.
	switch (if_type_)
	{
		case IFType::kFixed:
			return (if_param1_);																		// fmax
		case IFType::kLinear:
			return (if_param1_ * (1.0 - p_distance / max_distance_));									// fmax * (1 − d/dmax)
		case IFType::kExponential:
			return (if_param1_ * exp(-if_param2_ * p_distance));										// fmax * exp(−λd)
		case IFType::kNormal:
			return (if_param1_ * exp(-(p_distance * p_distance) / n_2param2sq_));						// fmax * exp(−d^2/2σ^2)
		case IFType::kCauchy:
		{
			double temp = p_distance / if_param2_;
			return (if_param1_ / (1.0 + temp * temp));													// fmax / (1+(d/λ)^2)
		}
	}
	EIDOS_TERMINATION << "ERROR (InteractionType::CalculateStrengthNoCallbacks): (internal error) unexpected if_type_ value." << EidosTerminate();
}

double InteractionType::CalculateStrengthWithCallbacks(double p_distance, Individual *p_receiver, Individual *p_exerter, std::vector<SLiMEidosBlock*> &p_interaction_callbacks)
{
	// CAUTION: This method should only be called when p_distance <= max_distance_ (or is NAN).
	// It is the caller's responsibility to do that filtering, for performance reasons!
	// The caller is also responsible for guaranteeing that this is not a self-interaction,
	// and that it is not ruled out by sex-selectivity.
	double strength = CalculateStrengthNoCallbacks(p_distance);
	
	strength = ApplyInteractionCallbacks(p_receiver, p_exerter, strength, p_distance, p_interaction_callbacks);
	
	return strength;
}

// the number of grid cells along one side of the 1D/2D/3D clipped_integral_ buffer; probably best to be a power of two
// we want to make this big enough that we don't need to interpolate; picking the closest value is within ~0.25% for 1024
// at this size, clipped_integral_ takes 8 MB, which is quite acceptable, and the temp buffer takes about the same
static const int64_t clipped_integral_size = 1024;

void InteractionType::CacheClippedIntegral_1D(void)
{
	if (clipped_integral_valid_ && clipped_integral_)
		return;
	
	if (clipped_integral_)
	{
		free(clipped_integral_);
		clipped_integral_ = nullptr;
	}
	
	if (!std::isfinite(max_distance_))
		EIDOS_TERMINATION << "ERROR (InteractionType::CacheClippedIntegral_1D): clippedIntegral() requires that the maxDistance of the interaction be finite; integrals out to infinity cannot be computed numerically." << EidosTerminate();
	
	// First, build a temporary buffer holding interaction function values for distances from a focal individual.
	// This is a 1D matrix of values, with the focal individual positioned at the very end of it, sitting on
	// the grid position at (0, 0).  Distances used here are in [0, max_distance_], except that they are the
	// distance from the edge grid position (where the focal individual is) to the *center* of each cell (each value)
	// in the grid, so in fact the distances represent a slightly narrower range of values than [0, max_distance_].
	int64_t dts_quadrant = clipped_integral_size - 1;	// -1 because this is the count of cells between grid lines
	double *distance_to_strength = (double *)calloc(dts_quadrant, sizeof(double));
	double dts_sum = 0.0;
	
	for (int64_t x = 0; x < dts_quadrant; ++x)
	{
		double cx = x + 0.5;										// center of the interval starting at x
		double distance = (cx / dts_quadrant) * max_distance_;		// x distance from the focal individual
		
		if (distance <= max_distance_)								// if not, calloc() provides 0.0
		{
			double strength = CalculateStrengthNoCallbacks(distance);
			
			distance_to_strength[x] = strength;
			dts_sum += strength;
		}
	}
	
#if 0
	// debug output of distance_to_strength
	std::cout << "distance_to_strength :" << std::endl;
	for (int64_t x = 0; x < dts_quadrant; ++x)
		printf("%.6f ", distance_to_strength[x]);
	std::cout << std::endl;
#endif
	
	// Now we build clipped_integral_ itself.  It is one larger than distance_to_strength in each dimension,
	// providing the integral for distances (dx) from the focal individual to the nearest edge in each
	// dimension.  Each value in it is a sum of strengths from a linear subset of distance_to_strength:
	// the strengths that would be inside the spatial bounds, for the given (dx).  The value at (0)
	// is exactly the sum of distance_to_strength, representing the integral for an individual that is
	// positioned at the edge of the space.  At (clipped_integral_size - 1) is the value for an individual
	// exactly (max_distance_), or further, from the nearest edge; it is 2x the sum of the entirety of
	// distance_to_strength.  Note that clipped_integral_ is dts_quadrant+1 values in length, because each
	// value of clipped_integral_ is conceptually positioned at the *grid position* between the intervals
	// of distance_to_strength; its values represent focal individual positions, which fall on the grid
	// positions of distance_to_strength.
	clipped_integral_ = (double *)calloc(clipped_integral_size, sizeof(double));
	
	// fill the first row/column so we have previously computed values to work with below
	clipped_integral_[0] = dts_sum;
	
	for (int64_t x = 1; x < clipped_integral_size; ++x)
	{
		// start with a previously computed value
		double integral = clipped_integral_[x - 1];
		
		// add the next value
		integral += distance_to_strength[x - 1];
		
		clipped_integral_[x] = integral;
	}
	
#if 0
	// debug output of clipped_integral_
	std::cout << "clipped_integral_ (point 1) :" << std::endl;
	for (int64_t x = 0; x < clipped_integral_size; ++x)
		printf("%.6f ", clipped_integral_[x]);
	std::cout << std::endl;
#endif
	
	// rescale clipped_integral_ by the size of each grid cell: of the area covered by the grid
	// (max_distance_ x max_distance_), the subarea comprised by one cell (1/dts_quadrant^2) of that
	// we do this as a post-pass mostly for debugging purposes, so that the steps above can be
	// verified to be working correctly in themselves before complicating matters by rescaling
	int64_t grid_count = clipped_integral_size;
	double normalization = (1.0 / dts_quadrant) * max_distance_;
	
	for (int64_t index = 0; index < grid_count; ++index)
		clipped_integral_[index] *= normalization;
	
#if 0
	// debug output of clipped_integral_
	std::cout << "clipped_integral_ (point 2) :" << std::endl;
	for (int64_t x = 0; x < clipped_integral_size; ++x)
		printf("%.6f ", clipped_integral_[x]);
	std::cout << std::endl;
#endif
	
	free(distance_to_strength);
	
	clipped_integral_valid_ = true;
}

void InteractionType::CacheClippedIntegral_2D(void)
{
	if (clipped_integral_valid_ && clipped_integral_)
		return;
	
	//double start_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC, end_time;
	
	if (clipped_integral_)
	{
		free(clipped_integral_);
		clipped_integral_ = nullptr;
	}
	
	if (!std::isfinite(max_distance_))
		EIDOS_TERMINATION << "ERROR (InteractionType::CacheClippedIntegral_2D): clippedIntegral() requires that the maxDistance of the interaction be finite; integrals out to infinity cannot be computed numerically." << EidosTerminate();
	
	// First, build a temporary buffer holding interaction function values for distances from a focal individual.
	// This is a 2D matrix of values, with the focal individual positioned at the very corner of it, sitting on
	// the grid lines that form the outside corner around the value at (0, 0).  Distances used here are in
	// [0, max_distance_], except that they are the distance from the corner grid intersection (where the focal
	// individual is) to the *center* of each cell (each value) in the grid, so in fact the distances represent
	// a slightly narrower range of values than [0, max_distance_].
	int64_t dts_quadrant = clipped_integral_size - 1;	// -1 because this is the count of cells between grid lines
	double *distance_to_strength = (double *)calloc(dts_quadrant * dts_quadrant, sizeof(double));
	
	//std::cout << "distance_to_strength size == " << ((dts_quadrant * dts_quadrant * sizeof(double)) / (1024.0 * 1024.0)) << "MB" << std::endl << std::endl;
	
	for (int64_t x = 0; x < dts_quadrant; ++x)
	{
		for (int64_t y = x; y < dts_quadrant; ++y)
		{
			double cx = x + 0.5, cy = y + 0.5;							// center of the grid cell (x, y)
			double dx = (cx / dts_quadrant) * max_distance_;			// x distance from the focal individual
			double dy = (cy / dts_quadrant) * max_distance_;			// y distance from the focal individual
			double distance = sqrt(dx * dx + dy * dy);					// distance from the focal individual
			
			if (distance <= max_distance_)								// if not, calloc() provides 0.0
			{
				double strength = CalculateStrengthNoCallbacks(distance);
				
				distance_to_strength[x + y * dts_quadrant] = strength;
				distance_to_strength[y + x * dts_quadrant] = strength;
			}
		}
	}
	
#if 0
	// debug output of distance_to_strength
	std::cout << "distance_to_strength :" << std::endl;
	for (int64_t y = 0; y < dts_quadrant; ++y)
	{
		for (int64_t x = 0; x < dts_quadrant; ++x)
		{
			printf("%.6f ", distance_to_strength[x + y * dts_quadrant]);
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
#endif
	
	// Now do preparatory summations to get a vector of cumulative column sums across distance_to_strength.
	// The first element of this vector is the sum of values from the first column of distance_to_strength.
	// The second element of this vector is that, *plus* the sum of the second column.  And so forth, such
	// that the last element of this vector of the sum of the entirety of distance_to_strength.  This will
	// allow us to work more efficiently below.  Since building clipped_integral_ the brute force way is
	// an O(N^4) algorithm (NxN values, each a sum of NxN values from distance_to_strength), this kind of
	// work will be important if we try to build a large clipped_integral_ buffer.
	double *dts_cumsums = (double *)malloc(dts_quadrant * sizeof(double));
	double *dts_colsums = (double *)malloc(dts_quadrant * sizeof(double));
	double total = 0.0;
	
	for (int64_t x = 0; x < dts_quadrant; ++x)
	{
		double colsum = 0.0;
		
		for (int64_t y = 0; y < dts_quadrant; ++y)
			colsum += distance_to_strength[x + y * dts_quadrant];
		
		dts_colsums[x] = colsum;
		
		total += colsum;
		dts_cumsums[x] = total;
	}
	
#if 0
	// debug output of dts_colsums
	std::cout << "dts_colsums :" << std::endl;
	for (int64_t x = 0; x < dts_quadrant; ++x)
		printf("%.6f ", dts_colsums[x]);
	std::cout << std::endl << std::endl;
	
	// debug output of dts_cumsums
	std::cout << "dts_cumsums :" << std::endl;
	for (int64_t x = 0; x < dts_quadrant; ++x)
		printf("%.6f ", dts_cumsums[x]);
	std::cout << std::endl << std::endl;
#endif
	
	// Now we build clipped_integral_ itself.  It is one larger than distance_to_strength in each dimension,
	// providing the integral for distances (dx, dy) from the focal individual to the nearest edge in each
	// dimension.  Each value in it is a sum of strengths from a rectangular subset of distance_to_strength:
	// the strengths that would be inside the spatial bounds, for the given (dx, dy).  The value at (0, 0)
	// is exactly the sum of distance_to_strength, representing the integral for an individual that is
	// positioned at the corner of the space.  At (clipped_integral_size - 1, clipped_integral_size - 1) is
	// the value for an individual exactly (max_distance_, max_distance_), or further, from the nearest
	// corner; it is 4x the sum of the entirety of distance_to_strength.  Each side of clipped_integral_ is
	// dts_quadrant+1 values in length, because each value of clipped_integral_ is conceptually positioned
	// at the *intersection of grid lines* between the cells of distance_to_strength; its values represent
	// focal individual positions, which fall on the grid lines of distance_to_strength.
	clipped_integral_ = (double *)calloc(clipped_integral_size * clipped_integral_size, sizeof(double));
	
	//std::cout << "clipped_integral_ size == " << ((clipped_integral_size * clipped_integral_size * sizeof(double)) / (1024.0 * 1024.0)) << "MB" << std::endl << std::endl;
	
	// fill the first row/column so we have previously computed values to work with below
	for (int64_t x = 0; x < clipped_integral_size; ++x)
	{
		double integral = dts_cumsums[dts_quadrant - 1];	// full quadrant
		
		if (x > 0)
			integral += dts_cumsums[x - 1];					// additional columns
		
		clipped_integral_[x + 0 * clipped_integral_size] = integral;
		clipped_integral_[0 + x * clipped_integral_size] = integral;
	}
	
	for (int64_t y = 1; y < clipped_integral_size; ++y)
	{
		// start with a previously computed value
		double integral = clipped_integral_[y + (y - 1) * clipped_integral_size];
		
		// add in previous values in the same row in this quadrant
		for (int64_t x = 1; x < y; ++x)
			integral += distance_to_strength[(x - 1) + (y - 1) * dts_quadrant];
		
		// now fill new values in this row
		for (int64_t x = y; x < clipped_integral_size; ++x)
		{
			// add in the full row in the other quadrant
			integral += dts_colsums[x - 1];
			
			// add in previous values in the same column in this quadrant; when x==y these were already in the previously computed value
			if (x > y)
			{
				for (int64_t yr = 1; yr < y; ++yr)
					integral += distance_to_strength[(x - 1) + (yr - 1) * dts_quadrant];
			}
			
			// add in the one new value for this new column in this row
			integral += distance_to_strength[(x - 1) + (y - 1) * dts_quadrant];
			
			clipped_integral_[x + y * clipped_integral_size] = integral;
			clipped_integral_[y + x * clipped_integral_size] = integral;
		}
	}
	
#if 0
	// debug output of clipped_integral_
	std::cout << "clipped_integral_ (point 1) :" << std::endl;
	for (int64_t y = 0; y < clipped_integral_size; ++y)
	{
		for (int64_t x = 0; x < clipped_integral_size; ++x)
		{
			printf("%.6f ", clipped_integral_[x + y * clipped_integral_size]);
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
#endif
	
	// rescale clipped_integral_ by the size of each grid cell: of the area covered by the grid
	// (max_distance_ x max_distance_), the subarea comprised by one cell (1/dts_quadrant^2) of that
	// we do this as a post-pass mostly for debugging purposes, so that the steps above can be
	// verified to be working correctly in themselves before complicating matters by rescaling
	int64_t grid_count = clipped_integral_size * clipped_integral_size;
	double normalization = (1.0 / (dts_quadrant * dts_quadrant)) * (max_distance_ * max_distance_);
	
	for (int64_t index = 0; index < grid_count; ++index)
		clipped_integral_[index] *= normalization;
	
#if 0
	// debug output of clipped_integral_
	std::cout << "clipped_integral_ (point 2) :" << std::endl;
	for (int64_t y = 0; y < clipped_integral_size; ++y)
	{
		for (int64_t x = 0; x < clipped_integral_size; ++x)
		{
			printf("%.6f ", clipped_integral_[x + y * clipped_integral_size]);
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
#endif
	
	free(distance_to_strength);
	free(dts_cumsums);
	free(dts_colsums);
	
	clipped_integral_valid_ = true;
	
	// for 1024x1024 this takes ~0.5 seconds, so ouch, but it generally only needs to be done once
	// note that the step commented "Now we build clipped_integral_ itself" is where 90% of the time here is spent
	//end_time = static_cast<double>(std::clock()) / CLOCKS_PER_SEC;
	//std::cout << "InteractionType::CacheClippedIntegral_2D() time == " << (end_time - start_time) << std::endl;
}

double InteractionType::ClippedIntegral_1D(double indDistanceA1, double indDistanceA2, bool periodic_x)
{
	if (periodic_x)
	{
		indDistanceA1 = max_distance_;
		indDistanceA2 = max_distance_;
	}
	
	if ((indDistanceA1 < max_distance_) && (indDistanceA2 < max_distance_))
		EIDOS_TERMINATION << "ERROR (InteractionType::ClippedIntegral_1D): clippedIntegral() requires that the maximum interaction distance be less than half of the spatial bounds extent, for non-periodic boundaries, such that the interaction function cannot be clipped on both sides." << EidosTerminate();
	
	double indDistanceA = std::min(std::min(indDistanceA1, indDistanceA2), max_distance_) / max_distance_;
	
	if (indDistanceA < 0.0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ClippedIntegral_1D): clippedIntegral() requires that receivers lie within the spatial bounds of their subpopulation." << EidosTerminate();
	
	int coordA = (int)round(indDistanceA * (clipped_integral_size - 1));
	
	//std::cout << "indDistanceA == " << indDistanceA << " : coordA == " << coordA << " : " << clipped_integral_[coordA] << std::endl;
	
	return clipped_integral_[coordA];
}

double InteractionType::ClippedIntegral_2D(double indDistanceA1, double indDistanceA2, double indDistanceB1, double indDistanceB2, bool periodic_x, bool periodic_y)
{
	if (periodic_x)
	{
		indDistanceA1 = max_distance_;
		indDistanceA2 = max_distance_;
	}
	if (periodic_y)
	{
		indDistanceB1 = max_distance_;
		indDistanceB2 = max_distance_;
	}
	
	if (((indDistanceA1 < max_distance_) && (indDistanceA2 < max_distance_)) || ((indDistanceB1 < max_distance_) && (indDistanceB2 < max_distance_)))
		EIDOS_TERMINATION << "ERROR (InteractionType::ClippedIntegral_2D): clippedIntegral() requires that the maximum interaction distance be less than half of the spatial bounds extent, for non-periodic boundaries, such that the interaction function cannot be clipped on both sides." << EidosTerminate();
	
	double indDistanceA = std::min(std::min(indDistanceA1, indDistanceA2), max_distance_) / max_distance_;
	double indDistanceB = std::min(std::min(indDistanceB1, indDistanceB2), max_distance_) / max_distance_;
	
	if ((indDistanceA < 0.0) || (indDistanceB < 0.0))
		EIDOS_TERMINATION << "ERROR (InteractionType::ClippedIntegral_2D): clippedIntegral() requires that receivers lie within the spatial bounds of their subpopulation." << EidosTerminate();
	
	int coordA = (int)round(indDistanceA * (clipped_integral_size - 1));
	int coordB = (int)round(indDistanceB * (clipped_integral_size - 1));
	
	//std::cout << "indDistanceA == " << indDistanceA << ", indDistanceB == " << indDistanceB << " : coordA == " << coordA << ", coordB == " << coordB << " : " << clipped_integral_[coordA + coordB * clipped_integral_size] << std::endl;
	
	return clipped_integral_[coordA + coordB * clipped_integral_size];
}

double InteractionType::ApplyInteractionCallbacks(Individual *p_receiver, Individual *p_exerter, double p_strength, double p_distance, std::vector<SLiMEidosBlock*> &p_interaction_callbacks)
{
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_START();
#endif
	
	SLiMEidosBlockType old_executing_block_type = community_.executing_block_type_;
	community_.executing_block_type_ = SLiMEidosBlockType::SLiMEidosInteractionCallback;
	
	for (SLiMEidosBlock *interaction_callback : p_interaction_callbacks)
	{
		if (interaction_callback->block_active_)
		{
#ifndef DEBUG_POINTS_ENABLED
#error "DEBUG_POINTS_ENABLED is not defined; include eidos_globals.h"
#endif
#if DEBUG_POINTS_ENABLED
			// SLiMgui debugging point
			EidosDebugPointIndent indenter;
			
			{
				EidosInterpreterDebugPointsSet *debug_points = community_.DebugPoints();
				EidosToken *decl_token = interaction_callback->root_node_->token_;
				
				if (debug_points && debug_points->set.size() && (decl_token->token_line_ != -1) &&
					(debug_points->set.find(decl_token->token_line_) != debug_points->set.end()))
				{
					SLIM_ERRSTREAM << EidosDebugPointIndent::Indent() << "#DEBUG interaction(i" << interaction_callback->interaction_type_id_;
					if (interaction_callback->subpopulation_id_ != -1)
						SLIM_ERRSTREAM << ", p" << interaction_callback->subpopulation_id_;
					SLIM_ERRSTREAM << ")";
					
					if (interaction_callback->block_id_ != -1)
						SLIM_ERRSTREAM << " s" << interaction_callback->block_id_;
					
					SLIM_ERRSTREAM << " (line " << (decl_token->token_line_ + 1) << community_.DebugPointInfo() << ")" << std::endl;
					indenter.indent();
				}
			}
#endif
			
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
					EidosSymbolTable callback_symbols(EidosSymbolTableType::kContextConstantsTable, &community_.SymbolTable());
					EidosSymbolTable client_symbols(EidosSymbolTableType::kLocalVariablesTable, &callback_symbols);
					EidosFunctionMap &function_map = community_.FunctionMap();
					EidosInterpreter interpreter(interaction_callback->compound_statement_node_, client_symbols, function_map, &community_, SLIM_OUTSTREAM, SLIM_ERRSTREAM);
					
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
					}
					catch (...)
					{
						throw;
					}
				}
			}
		}
	}
	
	community_.executing_block_type_ = old_executing_block_type;
	
#if defined(SLIMGUI) && (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosInteractionCallback)]);
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

size_t InteractionType::MemoryUsageForSparseVectorPool(void)
{
	size_t usage = s_freed_sparse_vectors_.size() * sizeof(SparseVector);
	
	for (SparseVector *free_sv : s_freed_sparse_vectors_)
		usage += free_sv->MemoryUsage();
	
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
		int periodic_dimensions = (p_subpop_data.periodic_x_ ? 1 : 0) + (p_subpop_data.periodic_y_ ? 1 : 0) + (p_subpop_data.periodic_z_ ? 1 : 0);
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
		if (!nodes)
			EIDOS_TERMINATION << "ERROR (InteractionType::EnsureKDTreePresent): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
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
				
				if (p_subpop_data.periodic_x_)
				{
					x_offset = p_subpop_data.bounds_x1_ * replication_dim_1;
					
					if (p_subpop_data.periodic_y_)
					{
						y_offset = p_subpop_data.bounds_y1_ * replication_dim_2;
						
						if (p_subpop_data.periodic_z_)
							z_offset = p_subpop_data.bounds_z1_ * replication_dim_3;
					}
					else if (p_subpop_data.periodic_z_)
					{
						z_offset = p_subpop_data.bounds_z1_ * replication_dim_2;
					}
				}
				else if (p_subpop_data.periodic_y_)
				{
					y_offset = p_subpop_data.bounds_y1_ * replication_dim_1;
					
					if (p_subpop_data.periodic_z_)
						z_offset = p_subpop_data.bounds_z1_ * replication_dim_2;
				}
				else if (p_subpop_data.periodic_z_)
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
#if DEBUG
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
#pragma mark sparse vector building
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

// add neighbors to the sparse vector in 1D
void InteractionType::BuildSV_Distances_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector)
{
	double d = dist_sq1(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[0] - nd[0];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_sparse_vector->AddEntryDistance(root->individual_index_, (sv_value_t)sqrt(d));
	
	if (dx > 0)
	{
		if (root->left)
			BuildSV_Distances_1(root->left, nd, p_focal_individual_index, p_sparse_vector);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSV_Distances_1(root->right, nd, p_focal_individual_index, p_sparse_vector);
	}
	else
	{
		if (root->right)
			BuildSV_Distances_1(root->right, nd, p_focal_individual_index, p_sparse_vector);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSV_Distances_1(root->left, nd, p_focal_individual_index, p_sparse_vector);
	}
}

// add neighbors to the sparse vector in 2D
void InteractionType::BuildSV_Distances_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase)
{
	double d = dist_sq2(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_sparse_vector->AddEntryDistance(root->individual_index_, (sv_value_t)sqrt(d));
	
	if (++p_phase >= 2) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			BuildSV_Distances_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSV_Distances_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
	else
	{
		if (root->right)
			BuildSV_Distances_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSV_Distances_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
}

// add neighbors to the sparse vector in 3D
void InteractionType::BuildSV_Distances_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase)
{
	double d = dist_sq3(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_sparse_vector->AddEntryDistance(root->individual_index_, (sv_value_t)sqrt(d));
	
	if (++p_phase >= 3) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			BuildSV_Distances_3(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSV_Distances_3(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
	else
	{
		if (root->right)
			BuildSV_Distances_3(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSV_Distances_3(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
}

// add neighbors to the sparse vector in 1D (exerter sex-specific)
void InteractionType::BuildSV_Distances_SS_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int start_exerter, int after_end_exerter)
{
	double d = dist_sq1(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[0] - nd[0];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index) && (root->individual_index_ >= start_exerter) && (root->individual_index_ < after_end_exerter))
		p_sparse_vector->AddEntryDistance(root->individual_index_, (sv_value_t)sqrt(d));
	
	if (dx > 0)
	{
		if (root->left)
			BuildSV_Distances_SS_1(root->left, nd, p_focal_individual_index, p_sparse_vector, start_exerter, after_end_exerter);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSV_Distances_SS_1(root->right, nd, p_focal_individual_index, p_sparse_vector, start_exerter, after_end_exerter);
	}
	else
	{
		if (root->right)
			BuildSV_Distances_SS_1(root->right, nd, p_focal_individual_index, p_sparse_vector, start_exerter, after_end_exerter);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSV_Distances_SS_1(root->left, nd, p_focal_individual_index, p_sparse_vector, start_exerter, after_end_exerter);
	}
}

// add neighbors to the sparse vector in 2D (exerter sex-specific)
void InteractionType::BuildSV_Distances_SS_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int start_exerter, int after_end_exerter, int p_phase)
{
	double d = dist_sq2(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index) && (root->individual_index_ >= start_exerter) && (root->individual_index_ < after_end_exerter))
		p_sparse_vector->AddEntryDistance(root->individual_index_, (sv_value_t)sqrt(d));
	
	if (++p_phase >= 2) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			BuildSV_Distances_SS_2(root->left, nd, p_focal_individual_index, p_sparse_vector, start_exerter, after_end_exerter, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSV_Distances_SS_2(root->right, nd, p_focal_individual_index, p_sparse_vector, start_exerter, after_end_exerter, p_phase);
	}
	else
	{
		if (root->right)
			BuildSV_Distances_SS_2(root->right, nd, p_focal_individual_index, p_sparse_vector, start_exerter, after_end_exerter, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSV_Distances_SS_2(root->left, nd, p_focal_individual_index, p_sparse_vector, start_exerter, after_end_exerter, p_phase);
	}
}

// add neighbors to the sparse vector in 3D (exerter sex-specific)
void InteractionType::BuildSV_Distances_SS_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int start_exerter, int after_end_exerter, int p_phase)
{
	double d = dist_sq3(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index) && (root->individual_index_ >= start_exerter) && (root->individual_index_ < after_end_exerter))
		p_sparse_vector->AddEntryDistance(root->individual_index_, (sv_value_t)sqrt(d));
	
	if (++p_phase >= 3) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			BuildSV_Distances_SS_3(root->left, nd, p_focal_individual_index, p_sparse_vector, start_exerter, after_end_exerter, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSV_Distances_SS_3(root->right, nd, p_focal_individual_index, p_sparse_vector, start_exerter, after_end_exerter, p_phase);
	}
	else
	{
		if (root->right)
			BuildSV_Distances_SS_3(root->right, nd, p_focal_individual_index, p_sparse_vector, start_exerter, after_end_exerter, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSV_Distances_SS_3(root->left, nd, p_focal_individual_index, p_sparse_vector, start_exerter, after_end_exerter, p_phase);
	}
}

// add neighbor strengths of type "f" (IFType::kFixed : fixed) to the sparse vector in 2D
void InteractionType::BuildSV_Strengths_f_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase)
{
	double d = dist_sq2(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
	{
		//d = sqrt(d);
		p_sparse_vector->AddEntryStrength(root->individual_index_, (sv_value_t)if_param1_);
	}
	
	if (++p_phase >= 2) p_phase = 0;
	if (dx > 0) {
		if (root->left)					BuildSV_Strengths_f_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		if (dx2 > max_distance_sq_)		return;
		if (root->right)				BuildSV_Strengths_f_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	} else {
		if (root->right)				BuildSV_Strengths_f_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		if (dx2 > max_distance_sq_)		return;
		if (root->left)					BuildSV_Strengths_f_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
}

// add neighbor strengths of type "l" (IFType::kLinear : linear) to the sparse vector in 2D
void InteractionType::BuildSV_Strengths_l_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase)
{
	double d = dist_sq2(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
	{
		d = sqrt(d);
		p_sparse_vector->AddEntryStrength(root->individual_index_, (sv_value_t)(if_param1_ * (1.0 - d / max_distance_)));
	}
	
	if (++p_phase >= 2) p_phase = 0;
	if (dx > 0) {
		if (root->left)					BuildSV_Strengths_l_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		if (dx2 > max_distance_sq_)		return;
		if (root->right)				BuildSV_Strengths_l_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	} else {
		if (root->right)				BuildSV_Strengths_l_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		if (dx2 > max_distance_sq_)		return;
		if (root->left)					BuildSV_Strengths_l_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
}

// add neighbor strengths of type "e" (IFType::kExponential : exponential) to the sparse vector in 2D
void InteractionType::BuildSV_Strengths_e_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase)
{
	double d = dist_sq2(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
	{
		d = sqrt(d);
		p_sparse_vector->AddEntryStrength(root->individual_index_, (sv_value_t)(if_param1_ * exp(-if_param2_ * d)));
	}
	
	if (++p_phase >= 2) p_phase = 0;
	if (dx > 0) {
		if (root->left)					BuildSV_Strengths_e_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		if (dx2 > max_distance_sq_)		return;
		if (root->right)				BuildSV_Strengths_e_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	} else {
		if (root->right)				BuildSV_Strengths_e_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		if (dx2 > max_distance_sq_)		return;
		if (root->left)					BuildSV_Strengths_e_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
}

// add neighbor strengths of type "n" (IFType::kNormal : normal/Gaussian) to the sparse vector in 2D
void InteractionType::BuildSV_Strengths_n_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase)
{
	double d = dist_sq2(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
	{
		//d = sqrt(d);
		p_sparse_vector->AddEntryStrength(root->individual_index_, (sv_value_t)(if_param1_ * exp(-d / n_2param2sq_)));
	}
	
	if (++p_phase >= 2) p_phase = 0;
	if (dx > 0) {
		if (root->left)					BuildSV_Strengths_n_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		if (dx2 > max_distance_sq_)		return;
		if (root->right)				BuildSV_Strengths_n_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	} else {
		if (root->right)				BuildSV_Strengths_n_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		if (dx2 > max_distance_sq_)		return;
		if (root->left)					BuildSV_Strengths_n_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
}


// add neighbor strengths of type "c" (IFType::kCauchy : Cauchy) to the sparse vector in 2D
void InteractionType::BuildSV_Strengths_c_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase)
{
	double d = dist_sq2(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
	{
		double temp = sqrt(d) / if_param2_;
		p_sparse_vector->AddEntryStrength(root->individual_index_, (sv_value_t)(if_param1_ / (1.0 + temp * temp)));
	}
	
	if (++p_phase >= 2) p_phase = 0;
	if (dx > 0) {
		if (root->left)					BuildSV_Strengths_c_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		if (dx2 > max_distance_sq_)		return;
		if (root->right)				BuildSV_Strengths_c_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	} else {
		if (root->right)				BuildSV_Strengths_c_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		if (dx2 > max_distance_sq_)		return;
		if (root->left)					BuildSV_Strengths_c_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
}

void InteractionType::FillSparseVectorForReceiverDistances(SparseVector *sv, Individual *receiver, double *receiver_position, Subpopulation *exerter_subpop, InteractionsData &exerter_subpop_data)
{
#if DEBUG
	// The caller should guarantee that the receiver and exerter species are compatible with the interaction
	CheckSpeciesCompatibility(receiver->subpopulation_->species_);
	CheckSpeciesCompatibility(exerter_subpop->species_);
	
	// The caller should guarantee that the interaction has been evaluated for the exerter subpopulation
	if (!exerter_subpop_data.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverDistances): (internal error) interaction has not yet been evaluated for the exerter subpopulation." << EidosTerminate();
	
	// Non-spatial interactions do not have a concept of distance, so this is an error
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverDistances): (internal error) request for distances from a non-spatial interaction." << EidosTerminate();
	
	// For spatial models, the caller should guarantee that the k-d tree is already present
	if (!exerter_subpop_data.kd_nodes_)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverDistances): (internal error) the k-d tree is not present for the exerter subpopulation." << EidosTerminate();
	
	// The caller should guarantee that the receiver and exerter subpops are compatible in spatial structure
	CheckSpatialCompatibility(receiver->subpopulation_, exerter_subpop);
	
	// The caller should ensure that this method is never called for a receiver that cannot receive interactions
	if ((receiver_sex_ != IndividualSex::kUnspecified) && (receiver_sex_ != receiver->sex_))
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverDistances): (internal error) the receiver is disqualified by sex-specificity." << EidosTerminate();
	
	// The caller should be handing us a sparse vector set up for distance data
	if (sv->DataType() != SparseVectorDataType::kDistances)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverDistances): (internal error) the sparse vector is not configured for distances." << EidosTerminate();
	
	// The caller should guarantee that the receiver is not a new juvenile, because they need to have a saved position
	if (receiver->index_ < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverDistances): (internal error) the receiver is a new juvenile." << EidosTerminate();
#endif
	
	// Figure out what index in the exerter subpopulation, if any, needs to be excluded so self-interaction is zero
	slim_popsize_t excluded_index = (exerter_subpop == receiver->subpopulation_) ? receiver->index_ : -1;
	
	if (exerter_sex_ == IndividualSex::kUnspecified)
	{
		// Without a specified exerter sex, we can add each exerter with no sex test
		if (spatiality_ == 2)		BuildSV_Distances_2(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, 0);
		else if (spatiality_ == 1)	BuildSV_Distances_1(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv);
		else if (spatiality_ == 3)	BuildSV_Distances_3(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, 0);
	}
	else
	{
		// With a specified exerter sex, we use a special version of BuildSV_Distances_X() that tests for that by range
		int start_exerter = 0, after_end_exerter = exerter_subpop_data.individual_count_;
		
		if (exerter_sex_ == IndividualSex::kMale)
			start_exerter = exerter_subpop_data.first_male_index_;
		else if (exerter_sex_ == IndividualSex::kFemale)
			after_end_exerter = exerter_subpop_data.first_male_index_;
		else
			EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverDistances): (internal error) unrecognized value for exerter_sex_." << EidosTerminate();
		
		if (spatiality_ == 2)		BuildSV_Distances_SS_2(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, start_exerter, after_end_exerter, 0);
		else if (spatiality_ == 1)	BuildSV_Distances_SS_1(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, start_exerter, after_end_exerter);
		else if (spatiality_ == 3)	BuildSV_Distances_SS_3(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, start_exerter, after_end_exerter, 0);
	}
	
	// After building the sparse vector above, we mark it finished
	sv->Finished();
}

void InteractionType::FillSparseVectorForReceiverStrengths(SparseVector *sv, Individual *receiver, double *receiver_position, Subpopulation *exerter_subpop, InteractionsData &exerter_subpop_data)
{
#if DEBUG
	// The caller should guarantee that the receiver and exerter species are compatible with the interaction
	CheckSpeciesCompatibility(receiver->subpopulation_->species_);
	CheckSpeciesCompatibility(exerter_subpop->species_);
	
	// The caller should guarantee that the interaction has been evaluated for the exerter subpopulation
	if (!exerter_subpop_data.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) interaction has not yet been evaluated for the exerter subpopulation." << EidosTerminate();
	
	// Non-spatial interactions are not handled by this method (they must be handled separately by logic in the caller), so this is an error
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) request for strengths from a non-spatial interaction." << EidosTerminate();
	
	// For spatial models, the caller should guarantee that the k-d tree is already present
	if (!exerter_subpop_data.kd_nodes_)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) the k-d tree is not present for the exerter subpopulation." << EidosTerminate();
	
	// The caller should guarantee that the receiver and exerter subpops are compatible in spatial structure
	CheckSpatialCompatibility(receiver->subpopulation_, exerter_subpop);
	
	// The caller should ensure that this method is never called for a receiver that cannot receive interactions
	if ((receiver_sex_ != IndividualSex::kUnspecified) && (receiver_sex_ != receiver->sex_))
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) the receiver is disqualified by sex-specificity." << EidosTerminate();
	
	// The caller should be handing us a sparse vector set up for strength data
	if (sv->DataType() != SparseVectorDataType::kStrengths)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) the sparse vector is not configured for strengths." << EidosTerminate();
	
	// The caller should guarantee that the receiver is not a new juvenile, because they need to have a saved position
	if (receiver->index_ < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) the receiver is a new juvenile." << EidosTerminate();
#endif
	
	// Figure out what index in the exerter subpopulation, if any, needs to be excluded so self-interaction is zero
	slim_popsize_t excluded_index = (exerter_subpop == receiver->subpopulation_) ? receiver->index_ : -1;
	
	// Set up to build distances first; this is an internal implementation detail, so we require the sparse vector set up for strengths above
	sv->SetDataType(SparseVectorDataType::kDistances);
	
	if (exerter_sex_ == IndividualSex::kUnspecified)
	{
		// Without a specified exerter sex, we can add each exerter with no sex test
		// We special-case some builds directly to strength values here, for efficiency, with
		// no callbacks and spatiality "xy".
		if ((exerter_subpop_data.evaluation_interaction_callbacks_.size() == 0) && (spatiality_ == 2))
		{
			sv->SetDataType(SparseVectorDataType::kStrengths);
			
			switch (if_type_)
			{
				case IFType::kFixed:		BuildSV_Strengths_f_2(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, 0); break;
				case IFType::kLinear:		BuildSV_Strengths_l_2(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, 0); break;
				case IFType::kExponential:	BuildSV_Strengths_e_2(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, 0); break;
				case IFType::kNormal:		BuildSV_Strengths_n_2(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, 0); break;
				case IFType::kCauchy:		BuildSV_Strengths_c_2(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, 0); break;
				default:
					EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) unoptimized IFType value." << EidosTerminate();
			}
			
			sv->Finished();
			return;
		}
		
		if (spatiality_ == 2)		BuildSV_Distances_2(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, 0);
		else if (spatiality_ == 1)	BuildSV_Distances_1(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv);
		else if (spatiality_ == 3)	BuildSV_Distances_3(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, 0);
	}
	else
	{
		// With a specified exerter sex, we use a special version of BuildSV_Distances_X() that tests for that by range
		int start_exerter = 0, after_end_exerter = exerter_subpop_data.individual_count_;
		
		if (exerter_sex_ == IndividualSex::kMale)
			start_exerter = exerter_subpop_data.first_male_index_;
		else if (exerter_sex_ == IndividualSex::kFemale)
			after_end_exerter = exerter_subpop_data.first_male_index_;
		else
			EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) unrecognized value for exerter_sex_." << EidosTerminate();
		
		if (spatiality_ == 2)		BuildSV_Distances_SS_2(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, start_exerter, after_end_exerter, 0);
		else if (spatiality_ == 1)	BuildSV_Distances_SS_1(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, start_exerter, after_end_exerter);
		else if (spatiality_ == 3)	BuildSV_Distances_SS_3(exerter_subpop_data.kd_root_, receiver_position, excluded_index, sv, start_exerter, after_end_exerter, 0);
	}
	
	// After building the sparse vector above, we mark it finished
	sv->Finished();
	
	// Now we scan through the pre-existing sparse vector for interacting pairs,
	// and transform distances into interaction strength values calculated for each.
	std::vector<SLiMEidosBlock*> &callbacks = exerter_subpop_data.evaluation_interaction_callbacks_;
	uint32_t nnz, *columns;
	sv_value_t *values;
	
	sv->Distances(&nnz, &columns, &values);
	
	if (callbacks.size() == 0)
	{
		// No callbacks; strength calculations come from the interaction function only
		
		// CalculateStrengthNoCallbacks() is basically inlined here, moved outside the loop; see that function for comments
		switch (if_type_)
		{
			case IFType::kFixed:
			{
				for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
					values[col_iter] = (sv_value_t)if_param1_;
				break;
			}
			case IFType::kLinear:
			{
				for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
				{
					sv_value_t distance = values[col_iter];
					
					values[col_iter] = (sv_value_t)(if_param1_ * (1.0 - distance / max_distance_));
				}
				break;
			}
			case IFType::kExponential:
			{
				for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
				{
					sv_value_t distance = values[col_iter];
					
					values[col_iter] = (sv_value_t)(if_param1_ * exp(-if_param2_ * distance));
				}
				break;
			}
			case IFType::kNormal:
			{
				for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
				{
					sv_value_t distance = values[col_iter];
					
					values[col_iter] = (sv_value_t)(if_param1_ * exp(-(distance * distance) / n_2param2sq_));
				}
				break;
			}
			case IFType::kCauchy:
			{
				for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
				{
					sv_value_t distance = values[col_iter];
					double temp = distance / if_param2_;
					
					values[col_iter] = (sv_value_t)(if_param1_ / (1.0 + temp * temp));
				}
				break;
			}
			default:
			{
				// should never be hit, but this is the base case
				for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
				{
					sv_value_t distance = values[col_iter];
					
					values[col_iter] = (sv_value_t)CalculateStrengthNoCallbacks(distance);
				}
				
				EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) unimplemented IFType case." << EidosTerminate();
			}
		}
	}
	else
	{
		// Callbacks; strength calculations need to include callback effects
		Individual **subpop_individuals = exerter_subpop->parent_individuals_.data();
		
		for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
		{
			uint32_t col = columns[col_iter];
			sv_value_t distance = values[col_iter];
			
			values[col_iter] = (sv_value_t)CalculateStrengthWithCallbacks(distance, receiver, subpop_individuals[col], callbacks);
		}
	}
	
	// We have transformed distances into strengths in the sparse vector's values_ buffer
	sv->SetDataType(SparseVectorDataType::kStrengths);
}


#pragma mark -
#pragma mark k-d tree neighbor searches
#pragma mark -

// count neighbors in 1D
int InteractionType::CountNeighbors_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index)
{
	int neighborCount = 0;
	double d = dist_sq1(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[0] - nd[0];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		neighborCount++;
	
	if (dx > 0)
	{
		if (root->left)
			neighborCount += CountNeighbors_1(root->left, nd, p_focal_individual_index);
		
		if (dx2 > max_distance_sq_) return neighborCount;
		
		if (root->right)
			neighborCount += CountNeighbors_1(root->right, nd, p_focal_individual_index);
	}
	else
	{
		if (root->right)
			neighborCount += CountNeighbors_1(root->right, nd, p_focal_individual_index);
		
		if (dx2 > max_distance_sq_) return neighborCount;
		
		if (root->left)
			neighborCount += CountNeighbors_1(root->left, nd, p_focal_individual_index);
	}
	
	return neighborCount;
}

// count neighbors in 2D
int InteractionType::CountNeighbors_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, int p_phase)
{
	int neighborCount = 0;
	double d = dist_sq2(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		neighborCount++;
	
	if (++p_phase >= 2) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			neighborCount += CountNeighbors_2(root->left, nd, p_focal_individual_index, p_phase);
		
		if (dx2 > max_distance_sq_) return neighborCount;
		
		if (root->right)
			neighborCount += CountNeighbors_2(root->right, nd, p_focal_individual_index, p_phase);
	}
	else
	{
		if (root->right)
			neighborCount += CountNeighbors_2(root->right, nd, p_focal_individual_index, p_phase);
		
		if (dx2 > max_distance_sq_) return neighborCount;
		
		if (root->left)
			neighborCount += CountNeighbors_2(root->left, nd, p_focal_individual_index, p_phase);
	}
	
	return neighborCount;
}

// count neighbors in 3D
int InteractionType::CountNeighbors_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, int p_phase)
{
	int neighborCount = 0;
	double d = dist_sq3(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		neighborCount++;
	
	if (++p_phase >= 3) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			neighborCount += CountNeighbors_3(root->left, nd, p_focal_individual_index, p_phase);
		
		if (dx2 > max_distance_sq_) return neighborCount;
		
		if (root->right)
			neighborCount += CountNeighbors_3(root->right, nd, p_focal_individual_index, p_phase);
	}
	else
	{
		if (root->right)
			neighborCount += CountNeighbors_3(root->right, nd, p_focal_individual_index, p_phase);
		
		if (dx2 > max_distance_sq_) return neighborCount;
		
		if (root->left)
			neighborCount += CountNeighbors_3(root->left, nd, p_focal_individual_index, p_phase);
	}
	
	return neighborCount;
}

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
		
		// Exclude the focal individual if and only if it is in the exerter subpopulation
		slim_popsize_t focal_individual_index;
		
		if (p_excluded_individual && (p_excluded_individual->subpopulation_ == p_subpop))
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
			
			if (!best || !best_dist)
				EIDOS_TERMINATION << "ERROR (InteractionType::FindNeighbors): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
			
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

const EidosClass *InteractionType::Class(void) const
{
	return gSLiM_InteractionType_Class;
}

void InteractionType::Print(std::ostream &p_ostream) const
{
	p_ostream << Class()->ClassName() << "<i" << interaction_type_id_ << ">";
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
			return super::GetProperty(p_property_id);
	}
}

EidosValue *InteractionType::GetProperty_Accelerated_id(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		InteractionType *value = (InteractionType *)(p_values[value_index]);
		
		int_result->set_int_no_check(value->interaction_type_id_, value_index);
	}
	
	return int_result;
}

EidosValue *InteractionType::GetProperty_Accelerated_tag(EidosObject **p_values, size_t p_values_size)
{
	EidosValue_Int_vector *int_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(p_values_size);
	
	for (size_t value_index = 0; value_index < p_values_size; ++value_index)
	{
		InteractionType *value = (InteractionType *)(p_values[value_index]);
		slim_usertag_t tag_value = value->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (InteractionType::GetProperty_Accelerated_tag): property tag accessed on interaction type before being set." << EidosTerminate();
		
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
			community_.interaction_types_changed_ = true;
			
			// changing max_distance_ invalidates the cached clipped_integral_ buffer; we don't deallocate it, just invalidate it
			clipped_integral_valid_ = false;
			
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
			return super::SetProperty(p_property_id, p_value);
		}
	}
}

EidosValue_SP InteractionType::ExecuteInstanceMethod(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
	switch (p_method_id)
	{
		case gID_clippedIntegral:			return ExecuteMethod_clippedIntegral(p_method_id, p_arguments, p_interpreter);
		case gID_distance:					return ExecuteMethod_distance(p_method_id, p_arguments, p_interpreter);
		case gID_distanceFromPoint:			return ExecuteMethod_distanceFromPoint(p_method_id, p_arguments, p_interpreter);
		case gID_drawByStrength:			return ExecuteMethod_drawByStrength(p_method_id, p_arguments, p_interpreter);
		case gID_evaluate:					return ExecuteMethod_evaluate(p_method_id, p_arguments, p_interpreter);
		case gID_interactingNeighborCount:	return ExecuteMethod_interactingNeighborCount(p_method_id, p_arguments, p_interpreter);
		case gID_localPopulationDensity:	return ExecuteMethod_localPopulationDensity(p_method_id, p_arguments, p_interpreter);
		case gID_interactionDistance:		return ExecuteMethod_interactionDistance(p_method_id, p_arguments, p_interpreter);
		case gID_nearestInteractingNeighbors:	return ExecuteMethod_nearestInteractingNeighbors(p_method_id, p_arguments, p_interpreter);
		case gID_nearestNeighbors:			return ExecuteMethod_nearestNeighbors(p_method_id, p_arguments, p_interpreter);
		case gID_nearestNeighborsOfPoint:	return ExecuteMethod_nearestNeighborsOfPoint(p_method_id, p_arguments, p_interpreter);
		case gID_neighborCount:				return ExecuteMethod_neighborCount(p_method_id, p_arguments, p_interpreter);
		case gID_neighborCountOfPoint:		return ExecuteMethod_neighborCountOfPoint(p_method_id, p_arguments, p_interpreter);
		case gID_setInteractionFunction:	return ExecuteMethod_setInteractionFunction(p_method_id, p_arguments, p_interpreter);
		case gID_strength:					return ExecuteMethod_strength(p_method_id, p_arguments, p_interpreter);
		case gID_totalOfNeighborStrengths:	return ExecuteMethod_totalOfNeighborStrengths(p_method_id, p_arguments, p_interpreter);
		case gID_unevaluate:				return ExecuteMethod_unevaluate(p_method_id, p_arguments, p_interpreter);
		default:							return super::ExecuteInstanceMethod(p_method_id, p_arguments, p_interpreter);
	}
}

static inline __attribute__((always_inline)) InteractionsData &InteractionsDataForSubpop(std::map<slim_objectid_t, InteractionsData> &data_, Subpopulation *subpop)
{
	slim_objectid_t subpop_id = subpop->subpopulation_id_;
	auto subpop_data_iter = data_.find(subpop_id);
	
	if ((subpop_data_iter == data_.end()) || !subpop_data_iter->second.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionsDataForSubpop): the interaction must be evaluated for the receiver and exerter subpopulations, by calling evaluate(), before any queries." << EidosTerminate();
	
	return subpop_data_iter->second;
}

//
//	*********************	– (float)clippedIntegral(No<Individual> receivers)
EidosValue_SP InteractionType::ExecuteMethod_clippedIntegral(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *receivers_value = p_arguments[0].get();
	int receivers_count = receivers_value->Count();
	// BEWARE: ExecuteMethod_localPopulationDensity() assumes that its API matches that of ExecuteMethod_clippedIntegral()!
	// If any arguments are added here, its code will need to change because that assumption will then be violated!
	// In fact localPopulationDensity() has an additional argument now, but it does not need that argument to be handled
	// by clippedIntegral(), so this dependency still works.
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_clippedIntegral): clippedIntegral() has no meaning for non-spatial interactions." << EidosTerminate();
	if (spatiality_ == 3)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_clippedIntegral): clippedIntegral() has not been implemented for the 'xyz' case yet.  If you need this functionality, please file a GitHub issue." << EidosTerminate();
	
	if (spatiality_ == 1)
		CacheClippedIntegral_1D();
	else if (spatiality_ == 2)
		CacheClippedIntegral_2D();
	else // (spatiality_ == 3)
	{
		;	// FIXME the big obstacle here is that a 1024x1024x1024 array of precalculated values is way too large, so interpolation is probably needed
	}
	
	// Note that clippedIntegral() ignores sex-specificity; every individual is in an "interaction field", which
	// clippedIntegral() measures, even if some individuals cannot actually feel any interactions exerted by others.
	// This policy means that clippedIntegral() never returns zero, avoiding divide-by-zero issues.
	
	// NULL means "what's the integral for a receiver that is not near any edge?"
	if (receivers_count == 0)
	{
		if (receivers_value->Type() == EidosValueType::kValueNULL)
		{
			double integral;
			
			if (spatiality_ == 1)
				integral = ClippedIntegral_1D(max_distance_, max_distance_, false);
			else if (spatiality_ == 2)
				integral = ClippedIntegral_2D(max_distance_, max_distance_, max_distance_, max_distance_, false, false);
			else // (spatiality_ == 3)
				integral = 0.0;			// FIXME
			
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(integral));
		}
		else
		{
			return gStaticEidosValue_Float_ZeroVec;
		}
	}
	
	// SPECIES CONSISTENCY CHECK
	Species *species = Community::SpeciesForIndividuals(receivers_value);
	
	if (!species)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_clippedIntegral): clippedIntegral() requires that all receivers belong to the same species." << EidosTerminate();
	
	CheckSpeciesCompatibility(*species);
	
	bool periodic_x, periodic_y, periodic_z;
	
	species->SpatialPeriodicity(&periodic_x, &periodic_y, &periodic_z);
	
	// We have a singleton or vector of receivers; we'd like to treat them both in the same way, so we set up for that here
	// We do not try to create a singleton return value when passed a singleton receiver; too complicated to optimize for that here
	const Individual * const *receivers_data;
	const Individual *receivers_singleton = nullptr;
	
	if (receivers_count == 1)
	{
		receivers_singleton = (Individual *)receivers_value->ObjectElementAtIndex(0, nullptr);
		receivers_data = &receivers_singleton;
	}
	else
	{
		receivers_data = (Individual * const *)receivers_value->ObjectElementVector()->data();
	}
	
	InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receivers_data[0]->subpopulation_);
	EidosValue_Float_vector *float_result = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(receivers_count);

	// Now treat cases according to spatiality
	if (spatiality_ == 1)
	{
		if (spatiality_string_ == "x")
		{
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				const Individual *receiver = receivers_data[receiver_index];
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
				
				double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				Subpopulation *subpop = receiver->subpopulation_;
				double indA = receiver_position[0];
				double integral = ClippedIntegral_1D(indA - subpop->bounds_x0_, subpop->bounds_x1_ - indA, periodic_x);
				
				float_result->set_float_no_check(integral, receiver_index);
			}
		}
		else if (spatiality_string_ == "y")
		{
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				const Individual *receiver = receivers_data[receiver_index];
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
				
				double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				Subpopulation *subpop = receiver->subpopulation_;
				double indA = receiver_position[0];
				double integral = ClippedIntegral_1D(indA - subpop->bounds_y0_, subpop->bounds_y1_ - indA, periodic_y);
				
				float_result->set_float_no_check(integral, receiver_index);
			}
		}
		else // (spatiality_string_ == "z")
		{
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				const Individual *receiver = receivers_data[receiver_index];
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
				
				double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				Subpopulation *subpop = receiver->subpopulation_;
				double indA = receiver_position[0];
				double integral = ClippedIntegral_1D(indA - subpop->bounds_z0_, subpop->bounds_z1_ - indA, periodic_x);
				
				float_result->set_float_no_check(integral, receiver_index);
			}
		}
	}
	else if (spatiality_ == 2)
	{
		if (spatiality_string_ == "xy")
		{
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				const Individual *receiver = receivers_data[receiver_index];
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
				
				double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				Subpopulation *subpop = receiver->subpopulation_;
				double indA = receiver_position[0];
				double indB = receiver_position[1];
				double integral = ClippedIntegral_2D(indA - subpop->bounds_x0_, subpop->bounds_x1_ - indA, indB - subpop->bounds_y0_, subpop->bounds_y1_ - indB, periodic_x, periodic_y);
				
				float_result->set_float_no_check(integral, receiver_index);
			}
		}
		else if (spatiality_string_ == "xz")
		{
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				const Individual *receiver = receivers_data[receiver_index];
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
				
				double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				Subpopulation *subpop = receiver->subpopulation_;
				double indA = receiver_position[0];
				double indB = receiver_position[1];
				double integral = ClippedIntegral_2D(indA - subpop->bounds_x0_, subpop->bounds_x1_ - indA, indB - subpop->bounds_z0_, subpop->bounds_z1_ - indB, periodic_x, periodic_z);
				
				float_result->set_float_no_check(integral, receiver_index);
			}
		}
		else // (spatiality_string_ == "yz")
		{
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				const Individual *receiver = receivers_data[receiver_index];
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
				
				double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				Subpopulation *subpop = receiver->subpopulation_;
				double indA = receiver_position[0];
				double indB = receiver_position[1];
				double integral = ClippedIntegral_2D(indA - subpop->bounds_y0_, subpop->bounds_y1_ - indA, indB - subpop->bounds_z0_, subpop->bounds_z1_ - indB, periodic_y, periodic_z);
				
				float_result->set_float_no_check(integral, receiver_index);
			}
		}
	}
	else // (spatiality_ == 3)
	{
		// FIXME
	}
	
	return EidosValue_SP(float_result);
}

//
//	*********************	– (float)distance(object<Individual>$ receiver, [No<Individual> exerters = NULL])
EidosValue_SP InteractionType::ExecuteMethod_distance(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_interpreter)
	EidosValue *receiver_value = p_arguments[0].get();
	EidosValue *exerters_value = p_arguments[1].get();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distance): distance() requires that the interaction be spatial." << EidosTerminate();
	
	// receiver_value is guaranteed to be singleton; let's get the info on it
	Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
	slim_popsize_t receiver_index_in_subpop = receiver->index_;
	
	if (receiver_index_in_subpop < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distance): distance() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
	
	Subpopulation *receiver_subpop = receiver->subpopulation_;
	Species &receiver_species = receiver_subpop->species_;
	
	CheckSpeciesCompatibility(receiver_species);
	
	InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
	double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
	
	// figure out the exerter subpopulation and get info on it
	bool exerters_value_NULL = (exerters_value->Type() == EidosValueType::kValueNULL);
	int exerters_count = exerters_value->Count();
	
	if ((exerters_count == 0) && !exerters_value_NULL)
		return gStaticEidosValue_Float_ZeroVec;
	
	Subpopulation *exerter_subpop = (exerters_value_NULL ? receiver_subpop : ((Individual *)exerters_value->ObjectElementAtIndex(0, nullptr))->subpopulation_);
	
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	slim_popsize_t exerter_subpop_size = exerter_subpop->parent_subpop_size_;
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	double *exerter_position_data = exerter_subpop_data.positions_;
	bool periodicity_enabled = (exerter_subpop_data.periodic_x_ || exerter_subpop_data.periodic_y_ || exerter_subpop_data.periodic_z_);
	
	if (exerters_value_NULL)
	{
		// NULL means return distances from receiver (which must be singleton) to all individuals in its subpopulation
		EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerter_subpop_size);
		
		if (periodicity_enabled)
		{
			for (int exerter_index = 0; exerter_index < exerter_subpop_size; ++exerter_index)
			{
				double distance = CalculateDistanceWithPeriodicity(receiver_position, exerter_position_data + exerter_index * SLIM_MAX_DIMENSIONALITY, exerter_subpop_data);
				
				result_vec->set_float_no_check(distance, exerter_index);
			}
		}
		else
		{
			for (int exerter_index = 0; exerter_index < exerter_subpop_size; ++exerter_index)
			{
				double distance = CalculateDistance(receiver_position, exerter_position_data + exerter_index * SLIM_MAX_DIMENSIONALITY);
				
				result_vec->set_float_no_check(distance, exerter_index);
			}
		}
		
		return EidosValue_SP(result_vec);
	}
	else
	{
		// Otherwise, individuals1 is singleton, and individuals2 is any length, so we loop over individuals2
		EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerters_count);
		
		for (int exerter_index = 0; exerter_index < exerters_count; ++exerter_index)
		{
			Individual *exerter = (Individual *)exerters_value->ObjectElementAtIndex(exerter_index, nullptr);
			
			// SPECIES CONSISTENCY CHECK
			if (exerter_subpop != exerter->subpopulation_)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distance): distance() requires that all exerters be in the same subpopulation." << EidosTerminate();
			
			slim_popsize_t exerter_index_in_subpop = exerter->index_;
			
			if (exerter_index_in_subpop < 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distance): distance() requires that exerters are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
			
			double distance;
			
			if (periodicity_enabled)
				distance = CalculateDistanceWithPeriodicity(receiver_position, exerter_position_data + exerter_index_in_subpop * SLIM_MAX_DIMENSIONALITY, exerter_subpop_data);
			else
				distance = CalculateDistance(receiver_position, exerter_position_data + exerter_index_in_subpop * SLIM_MAX_DIMENSIONALITY);
			
			result_vec->set_float_no_check(distance, exerter_index);
		}
		
		return EidosValue_SP(result_vec);
	}
}

//	*********************	– (float)distanceFromPoint(float point, object<Individual> exerters)
//
EidosValue_SP InteractionType::ExecuteMethod_distanceFromPoint(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *point_value = p_arguments[0].get();
	EidosValue *exerters_value = p_arguments[1].get();
	int exerters_count = exerters_value->Count();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distanceFromPoint): distanceFromPoint() requires that the interaction be spatial." << EidosTerminate();
	if (point_value->Count() != spatiality_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distanceFromPoint): distanceFromPoint() requires that point is of length equal to the interaction spatiality." << EidosTerminate();
	
	if (exerters_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	// Get the point's coordinates into a double[]
	double point_data[SLIM_MAX_DIMENSIONALITY];
	
#ifdef __clang_analyzer__
	// The static analyzer does not understand some things, so we tell it here
	point_data[0] = 0;
	point_data[1] = 0;
	point_data[2] = 0;
#endif
	
	for (int point_index = 0; point_index < spatiality_; ++point_index)
		point_data[point_index] = point_value->FloatAtIndex(point_index, nullptr);
	
	// exerters_value is guaranteed to be of length >= 1; let's get the info on it
	Individual *exerter_first = (Individual *)exerters_value->ObjectElementAtIndex(0, nullptr);
	Subpopulation *exerter_subpop = exerter_first->subpopulation_;
	Species &exerter_species = exerter_subpop->species_;
	
	CheckSpeciesCompatibility(exerter_species);
	
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	double *exerter_position_data = exerter_subpop_data.positions_;
	bool periodicity_enabled = (exerter_subpop_data.periodic_x_ || exerter_subpop_data.periodic_y_ || exerter_subpop_data.periodic_z_);
	
	// If we're using periodic boundaries, the point supplied has to be within bounds in the periodic dimensions; points outside periodic bounds make no sense
	if (periodicity_enabled)
	{
		if ((exerter_subpop_data.periodic_x_ && ((point_data[0] < 0.0) || (point_data[0] > exerter_subpop_data.bounds_x1_))) ||
			(exerter_subpop_data.periodic_y_ && ((point_data[1] < 0.0) || (point_data[1] > exerter_subpop_data.bounds_y1_))) ||
			(exerter_subpop_data.periodic_z_ && ((point_data[2] < 0.0) || (point_data[2] > exerter_subpop_data.bounds_z1_))))
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distanceFromPoint): distanceFromPoint() requires that coordinates for periodic spatial dimensions fall inside spatial bounaries; use pointPeriodic() to ensure this if necessary." << EidosTerminate();
	}
	
	EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerters_count);
	
	if (periodicity_enabled)
	{
		for (int exerter_index = 0; exerter_index < exerters_count; ++exerter_index)
		{
			Individual *exerter = (Individual *)exerters_value->ObjectElementAtIndex(exerter_index, nullptr);
			
			// SPECIES CONSISTENCY CHECK
			if (exerter_subpop != exerter->subpopulation_)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distanceFromPoint): distanceFromPoint() requires that all exerters be in the same subpopulation." << EidosTerminate();
			
			slim_popsize_t exerter_index_in_subpop = exerter->index_;
			
			if (exerter_index_in_subpop < 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distanceFromPoint): distanceFromPoint() requires that exerters are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
			
			double *ind_position = exerter_position_data + exerter_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			
			result_vec->set_float_no_check(CalculateDistanceWithPeriodicity(ind_position, point_data, exerter_subpop_data), exerter_index);
		}
	}
	else
	{
		for (int exerter_index = 0; exerter_index < exerters_count; ++exerter_index)
		{
			Individual *exerter = (Individual *)exerters_value->ObjectElementAtIndex(exerter_index, nullptr);
			
			// SPECIES CONSISTENCY CHECK
			if (exerter_subpop != exerter->subpopulation_)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distanceFromPoint): distanceFromPoint() requires that all exerters be in the same subpopulation." << EidosTerminate();
			
			slim_popsize_t exerter_index_in_subpop = exerter->index_;
			
			if (exerter_index_in_subpop < 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_distanceFromPoint): distanceFromPoint() requires that exerters are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
			
			double *ind_position = exerter_position_data + exerter_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			
			result_vec->set_float_no_check(CalculateDistance(ind_position, point_data), exerter_index);
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
				
				draw_indices.emplace_back(hit_index);
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
				
				draw_indices.emplace_back(hit_index);
			}
		}
	}
}

//	*********************	– (object<Individual>)drawByStrength(object<Individual>$ receiver, [integer$ count = 1], [No<Subpopulation>$ exerterSubpop = NULL])
//
EidosValue_SP InteractionType::ExecuteMethod_drawByStrength(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *receiver_value = p_arguments[0].get();
	EidosValue *count_value = p_arguments[1].get();
	EidosValue *exerterSubpop_value = p_arguments[2].get();
	
	// receiver_value is guaranteed to be singleton; let's get the info on it
	Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
	slim_popsize_t receiver_index_in_subpop = receiver->index_;
	
	if (receiver_index_in_subpop < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_drawByStrength): drawByStrength() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
	
	Subpopulation *receiver_subpop = receiver->subpopulation_;
	Species &receiver_species = receiver_subpop->species_;
	
	CheckSpeciesCompatibility(receiver_species);
	
	// the exerter subpopulation defaults to the same subpop as the receivers
	Subpopulation *exerter_subpop = ((exerterSubpop_value->Type() == EidosValueType::kValueNULL) ? receiver_subpop : (Subpopulation *)exerterSubpop_value->ObjectElementAtIndex(0, nullptr));
	
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	slim_popsize_t exerter_subpop_size = exerter_subpop->parent_subpop_size_;
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	
	// Check the count
	int64_t count = count_value->IntAtIndex(0, nullptr);
	
	if (count < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_drawByStrength): drawByStrength() requires count >= 0." << EidosTerminate();
	
	if (count > exerter_subpop_size)
		count = exerter_subpop_size;
	
	if (count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	// Check sex-specificity for the receiver; if the individual is disqualified, no draws can occur and the return is empty
	if ((receiver_sex_ != IndividualSex::kUnspecified) && (receiver_sex_ != receiver->sex_))
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	if (spatiality_ == 0)
	{
		// Non-spatial case; no distances used.  We have to worry about sex-segregation; it is not handled for us.
		slim_popsize_t receiver_index = ((exerter_subpop == receiver->subpopulation_) && (receiver->index_ >= 0) ? receiver->index_ : -1);
		std::vector<SLiMEidosBlock*> &callbacks = exerter_subpop_data.evaluation_interaction_callbacks_;
		
		EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		double total_interaction_strength = 0.0;
		std::vector<double> cached_strength;
		std::vector<Individual *> &exerters = exerter_subpop->parent_individuals_;
		
		for (slim_popsize_t exerter_index_in_subpop = 0; exerter_index_in_subpop < exerter_subpop_size; ++exerter_index_in_subpop)
		{
			Individual *exerter = exerters[exerter_index_in_subpop];
			double strength = 0;
			
			if (exerter_index_in_subpop != receiver_index)
				if ((exerter_sex_ == IndividualSex::kUnspecified) || (exerter_sex_ == exerter->sex_))
					strength = ApplyInteractionCallbacks(receiver, exerter, if_param1_, NAN, callbacks);	// hard-coding interaction function "f" (IFType::kFixed), which is required
			
			total_interaction_strength += strength;
			cached_strength.emplace_back(strength);
		}
		
		if (total_interaction_strength > 0.0)
		{
			std::vector<int> strength_indices;
			
			result_vec->resize_no_initialize(count);
			DrawByWeights((int)count, cached_strength.data(), exerter_subpop_size, total_interaction_strength, strength_indices);
			
			for (size_t result_index = 0; result_index < strength_indices.size(); ++result_index)
			{
				int strength_index = strength_indices[result_index];
				Individual *chosen_individual = exerters[strength_index];
				
				result_vec->set_object_element_no_check_NORR(chosen_individual, result_index);
			}
		}
		
		return EidosValue_SP(result_vec);
	}
	else
	{
		// Spatial case; we use the k-d tree to get strengths for all neighbors.
		InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
		double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
		
		EnsureKDTreePresent(exerter_subpop_data);
		
		SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kStrengths);
		FillSparseVectorForReceiverStrengths(sv, receiver, receiver_position, exerter_subpop, exerter_subpop_data);
		uint32_t nnz;
		const uint32_t *columns;
		const sv_value_t *strengths;
		std::vector<double> double_strengths;	// needed by DrawByWeights() for gsl_ran_discrete_preproc()
		
		strengths = sv->Strengths(&nnz, &columns);
		
		// Total the interaction strengths, and gather a vector of strengths as doubles
		double total_interaction_strength = 0.0;
		
		for (uint32_t col_index = 0; col_index < nnz; ++col_index)
		{
			sv_value_t strength = strengths[col_index];
			
			total_interaction_strength += strength;
			double_strengths.emplace_back((double)strength);
		}
		
		// Draw individuals
		EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		
		if (total_interaction_strength > 0.0)
		{
			std::vector<int> strength_indices;
			std::vector<Individual *> &exerters = exerter_subpop->parent_individuals_;
			
			result_vec->resize_no_initialize(count);
			DrawByWeights((int)count, double_strengths.data(), nnz, total_interaction_strength, strength_indices);
			
			for (size_t result_index = 0; result_index < strength_indices.size(); ++result_index)
			{
				int strength_index = strength_indices[result_index];
				Individual *chosen_individual = exerters[columns[strength_index]];
				
				result_vec->set_object_element_no_check_NORR(chosen_individual, result_index);
			}
		}
		
		FreeSparseVector(sv);
		return EidosValue_SP(result_vec);
	}
}

//	*********************	- (void)evaluate(io<Subpopulation> subpops)
//
EidosValue_SP InteractionType::ExecuteMethod_evaluate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *subpops_value = p_arguments[0].get();
	
	if ((community_.CycleStage() == SLiMCycleStage::kWFStage2GenerateOffspring) ||
		(community_.CycleStage() == SLiMCycleStage::kNonWFStage1GenerateOffspring) ||
		(community_.CycleStage() == SLiMCycleStage::kNonWFStage4SurvivalSelection))
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_evaluate): evaluate() may not be called during the offspring generation or selection/mortality cycle stages." << EidosTerminate();
	
	// Get the requested subpops
	int requested_subpop_count = subpops_value->Count();
		
	for (int requested_subpop_index = 0; requested_subpop_index < requested_subpop_count; ++requested_subpop_index)
		EvaluateSubpopulation(SLiM_ExtractSubpopulationFromEidosValue_io(subpops_value, requested_subpop_index, &community_, nullptr, "evaluate()"));
	
	return gStaticEidosValueVOID;
}

//	*********************	– (integer)interactingNeighborCount(object<Individual> receivers, [No<Subpopulation>$ exerterSubpop = NULL])
//
EidosValue_SP InteractionType::ExecuteMethod_interactingNeighborCount(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *receivers_value = p_arguments[0].get();
	EidosValue *exerterSubpop_value = p_arguments[1].get();
	int receivers_count = receivers_value->Count();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires that the interaction be spatial." << EidosTerminate();
	
	if (receivers_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	// the exerter subpopulation defaults to the same subpop as the receivers
	Subpopulation *receiver_subpop = ((Individual *)receivers_value->ObjectElementAtIndex(0, nullptr))->subpopulation_;
	Subpopulation *exerter_subpop = ((exerterSubpop_value->Type() == EidosValueType::kValueNULL) ? receiver_subpop : (Subpopulation *)exerterSubpop_value->ObjectElementAtIndex(0, nullptr));
	
	CheckSpeciesCompatibility(receiver_subpop->species_);
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	if (exerter_subpop->parent_subpop_size_ == 0)
	{
		// If the exerter subpop is empty then all count values for the receivers are zero
		if (receivers_count == 1)
		{
			return gStaticEidosValue_Integer0;
		}
		else
		{
			EidosValue_Int_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(receivers_count);
			
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
				result_vec->set_int_no_check(0, receiver_index);
			
			return EidosValue_SP(result_vec);
		}
	}
	
	InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	EnsureKDTreePresent(exerter_subpop_data);
	
	if (receivers_count == 1)
	{
		// Just one value, so we can return a singleton and skip some work
		Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(0, nullptr);
		slim_popsize_t receiver_index_in_subpop = receiver->index_;
		
		if (receiver_index_in_subpop < 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
		
		// Check sex-specificity for the receiver; if the individual is disqualified, the count is zero
		if ((receiver_sex_ != IndividualSex::kUnspecified) && (receiver_sex_ != receiver->sex_))
			return gStaticEidosValue_Integer0;
		
		double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
		SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kDistances);
		FillSparseVectorForReceiverDistances(sv, receiver, receiver_position, exerter_subpop, exerter_subpop_data);	// FIXME all we actually need is nnz!
		
		// Get the sparse vector data
		uint32_t nnz;
		
		sv->Distances(&nnz);
		
		InteractionType::FreeSparseVector(sv);
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(nnz));
	}
	else
	{
		EidosValue_Int_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(receivers_count);
		
		for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
		{
			Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(receiver_index, nullptr);
			slim_popsize_t receiver_index_in_subpop = receiver->index_;
			
			if (receiver_index_in_subpop < 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
			
			// SPECIES CONSISTENCY CHECK
			if (receiver_subpop != receiver->subpopulation_)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires that all receivers be in the same subpopulation." << EidosTerminate();
			
			// Check sex-specificity for the receiver; if the individual is disqualified, the count is zero
			if ((receiver_sex_ != IndividualSex::kUnspecified) && (receiver_sex_ != receiver->sex_))
			{
				result_vec->set_int_no_check(0, receiver_index);
				continue;
			}
			
			double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kDistances);
			FillSparseVectorForReceiverDistances(sv, receiver, receiver_position, exerter_subpop, exerter_subpop_data);	// FIXME all we actually need is nnz!
			
			// Get the sparse vector data
			uint32_t nnz;
			
			sv->Distances(&nnz);
			
			InteractionType::FreeSparseVector(sv);
			result_vec->set_int_no_check(nnz, receiver_index);
		}
		
		return EidosValue_SP(result_vec);
	}
	
	return gStaticEidosValueVOID;
}

//	*********************	– (float)localPopulationDensity(object<Individual> receivers, [No<Subpopulation>$ exerterSubpop = NULL])
//
EidosValue_SP InteractionType::ExecuteMethod_localPopulationDensity(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *receivers_value = p_arguments[0].get();
	EidosValue *exerterSubpop_value = p_arguments[1].get();
	int receivers_count = receivers_value->Count();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_localPopulationDensity): localPopulationDensity() requires that the interaction be spatial." << EidosTerminate();
	if (spatiality_ == 3)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_localPopulationDensity): localPopulationDensity() does not support the 'xyz' case yet.  If you need this functionality, please file a GitHub issue." << EidosTerminate();
	
	if (receivers_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	// receivers_value is guaranteed to have at least one value
	Individual *first_receiver = (Individual *)receivers_value->ObjectElementAtIndex(0, nullptr);
	Subpopulation *receiver_subpop = first_receiver->subpopulation_;
	
	// the exerter subpopulation defaults to the same subpop as the receivers
	Subpopulation *exerter_subpop = ((exerterSubpop_value->Type() == EidosValueType::kValueNULL) ? receiver_subpop : (Subpopulation *)exerterSubpop_value->ObjectElementAtIndex(0, nullptr));
	
	CheckSpeciesCompatibility(receiver_subpop->species_);
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	if (receiver_subpop != exerter_subpop)
		if ((receiver_subpop->bounds_x0_ != exerter_subpop->bounds_x0_) || (receiver_subpop->bounds_x1_ != exerter_subpop->bounds_x1_) ||
			(receiver_subpop->bounds_y0_ != exerter_subpop->bounds_y0_) || (receiver_subpop->bounds_y1_ != exerter_subpop->bounds_y1_) ||
			(receiver_subpop->bounds_z0_ != exerter_subpop->bounds_z0_) || (receiver_subpop->bounds_z1_ != exerter_subpop->bounds_z1_))
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_localPopulationDensity): localPopulationDensity() requires that the receiver and exerter subpopulations have identical bounds." << EidosTerminate();
	
	if (exerter_subpop->parent_subpop_size_ == 0)
	{
		// If the exerter subpop is empty then all density values for the receivers are zero (note that we
		// already handled the case of receivers_count == 0 above, so the receiver is not in the exerter subpop)
		if (receivers_count == 1)
		{
			return gStaticEidosValue_Float0;
		}
		else
		{
			EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(receivers_count);
			
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
				result_vec->set_float_no_check(0, receiver_index);
			
			return EidosValue_SP(result_vec);
		}
	}
	
	InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	EnsureKDTreePresent(exerter_subpop_data);
	
	double strength_for_zero_distance = CalculateStrengthNoCallbacks(0.0);	// probably always if_param1_, but let's not hard-code that...
	
	// Subcontract to ExecuteMethod_clippedIntegral(); this handles all the spatiality crap for us
	// note that we pass our own parameters through to clippedIntegral()!  So our APIs need to be the same!
	// Actually, we now have an extra parameter, exerterSubpop(), compared to clippedIntegral(); but we
	// do not need it to use that parameter (because we require identical bounds above), so it's OK.
	EidosValue_SP clipped_integrals = ExecuteMethod_clippedIntegral(p_method_id, p_arguments, p_interpreter);
	
	if (receivers_count == 1)
	{
		// Just one value, so we can return a singleton and skip some work
		slim_popsize_t receiver_index_in_subpop = first_receiver->index_;
		
		if (receiver_index_in_subpop < 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_localPopulationDensity): localPopulationDensity() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
		
		// Check sex-specificity for the receiver; if the individual is disqualified, the local density of interacters is zero
		if ((receiver_sex_ != IndividualSex::kUnspecified) && (receiver_sex_ != first_receiver->sex_))
			return gStaticEidosValue_Integer0;
		
		double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
		SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kStrengths);
		FillSparseVectorForReceiverStrengths(sv, first_receiver, receiver_position, exerter_subpop, exerter_subpop_data);
		
		// Get the sparse vector data
		uint32_t nnz;
		const sv_value_t *strengths;
		
		strengths = sv->Strengths(&nnz);
		
		// Total the interaction strengths
		double total_strength = 0.0;
		
		for (uint32_t col_index = 0; col_index < nnz; ++col_index)
			total_strength += strengths[col_index];
		
		// Add the interaction strength for the focal individual to the focal point, since it counts for density
		if (receiver_subpop == exerter_subpop)
			total_strength += strength_for_zero_distance;
		
		// Divide by the corresponding clipped integral to get density
		total_strength /= clipped_integrals->FloatAtIndex(0, nullptr);
		
		FreeSparseVector(sv);
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(total_strength));
	}
	else
	{
		// Loop over the requested individuals and get the totals
		EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(receivers_count);
		
		for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
		{
			Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(receiver_index, nullptr);
			slim_popsize_t receiver_index_in_subpop = receiver->index_;
			
			if (receiver_index_in_subpop < 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_localPopulationDensity): localPopulationDensity() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
			
			// SPECIES CONSISTENCY CHECK
			if (receiver_subpop != receiver->subpopulation_)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_localPopulationDensity): localPopulationDensity() requires that all receivers be in the same subpopulation." << EidosTerminate();
			
			// Check sex-specificity for the receiver; if the individual is disqualified, the local density of interacters is zero
			if ((receiver_sex_ != IndividualSex::kUnspecified) && (receiver_sex_ != receiver->sex_))
			{
				result_vec->set_float_no_check(0, receiver_index);
				continue;
			}
			
			double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kStrengths);
			FillSparseVectorForReceiverStrengths(sv, receiver, receiver_position, exerter_subpop, exerter_subpop_data);
			
			// Get the sparse vector data
			uint32_t nnz;
			const sv_value_t *strengths;
			
			strengths = sv->Strengths(&nnz);
			
			// Total the interaction strengths
			double total_strength = 0.0;
			
			for (uint32_t col_index = 0; col_index < nnz; ++col_index)
				total_strength += strengths[col_index];
			
			// Add the interaction strength for the focal individual to the focal point, since it counts for density
			if (receiver_subpop == exerter_subpop)
				total_strength += strength_for_zero_distance;
			
			// Divide by the corresponding clipped integral to get density
			total_strength /= clipped_integrals->FloatAtIndex(receiver_index, nullptr);
			result_vec->set_float_no_check(total_strength, receiver_index);
			
			FreeSparseVector(sv);
		}
		
		return EidosValue_SP(result_vec);
	}
}

//
//	*********************	– (float)interactionDistance(object<Individual>$ receiver, [No<Individual> exerters = NULL])
EidosValue_SP InteractionType::ExecuteMethod_interactionDistance(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *receiver_value = p_arguments[0].get();
	EidosValue *exerters_value = p_arguments[1].get();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactionDistance): interactionDistance() requires that the interaction be spatial." << EidosTerminate();
	
	// receiver_value is guaranteed to be singleton; let's get the info on it
	Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
	slim_popsize_t receiver_index_in_subpop = receiver->index_;
	
	if (receiver_index_in_subpop < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactionDistance): interactionDistance() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
	
	Subpopulation *receiver_subpop = receiver->subpopulation_;
	Species &receiver_species = receiver_subpop->species_;
	
	CheckSpeciesCompatibility(receiver_species);
	
	InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
	double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
	
	// figure out the exerter subpopulation and get info on it
	bool exerters_value_NULL = (exerters_value->Type() == EidosValueType::kValueNULL);
	int exerters_count = exerters_value->Count();
	
	if ((exerters_count == 0) && !exerters_value_NULL)
		return gStaticEidosValue_Float_ZeroVec;
	
	Subpopulation *exerter_subpop = (exerters_value_NULL ? receiver_subpop : ((Individual *)exerters_value->ObjectElementAtIndex(0, nullptr))->subpopulation_);
	
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	slim_popsize_t exerter_subpop_size = exerter_subpop->parent_subpop_size_;
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	EnsureKDTreePresent(exerter_subpop_data);
	
	// set up our return value
	if (exerters_value_NULL)
		exerters_count = exerter_subpop_size;
	
	EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerters_count);
	
	// Check sex-specificity for the receiver; if the individual is disqualified, the distance to each exerter is zero
	if ((receiver_sex_ != IndividualSex::kUnspecified) && (receiver_sex_ != receiver->sex_))
	{
		double *result_ptr = result_vec->data();
		
		for (int exerter_index = 0; exerter_index < exerter_subpop_size; ++exerter_index)
			*(result_ptr + exerter_index) = INFINITY;
		
		return EidosValue_SP(result_vec);
	}
	
	SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kDistances);
	FillSparseVectorForReceiverDistances(sv, receiver, receiver_position, exerter_subpop, exerter_subpop_data);
	uint32_t nnz;
	const uint32_t *columns;
	const sv_value_t *distances;
	
	distances = sv->Distances(&nnz, &columns);
	
	if (exerters_value_NULL)
	{
		// NULL means return distances from individuals1 (which must be singleton) to all individuals in the subpopulation
		// We initialize the return vector to INFINITY, and fill in non-infinite values from the sparse vector
		double *result_ptr = result_vec->data();
		
		for (int exerter_index = 0; exerter_index < exerter_subpop_size; ++exerter_index)
			*(result_ptr + exerter_index) = INFINITY;
		
		for (uint32_t col_index = 0; col_index < nnz; ++col_index)
			*(result_ptr + columns[col_index]) = distances[col_index];
	}
	else
	{
		// Otherwise, receiver is singleton, and exerters is any length, so we loop over exerters
		// Each individual in exerters requires a linear search through the sparse vector, unfortunately
		for (int exerter_index = 0; exerter_index < exerters_count; ++exerter_index)
		{
			Individual *exerter = (Individual *)exerters_value->ObjectElementAtIndex(exerter_index, nullptr);
			
			// SPECIES CONSISTENCY CHECK
			if (exerter_subpop != exerter->subpopulation_)
			{
				InteractionType::FreeSparseVector(sv);
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactionDistance): interactionDistance() requires that all exerters be in the same subpopulation." << EidosTerminate();
			}
			
			slim_popsize_t exerter_index_in_subpop = exerter->index_;
			
			if (exerter_index_in_subpop < 0)
			{
				InteractionType::FreeSparseVector(sv);
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactionDistance): interactionDistance() requires that exerters are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
			}
			
			double distance = INFINITY;
			
			for (uint32_t col_index = 0; col_index < nnz; ++col_index)
				if ((slim_popsize_t)columns[col_index] == exerter_index_in_subpop)
				{
					distance = distances[col_index];
					break;
				}
			
			result_vec->set_float_no_check(distance, exerter_index);
		}
	}
	
	InteractionType::FreeSparseVector(sv);
	return EidosValue_SP(result_vec);
}

//	*********************	– (object<Individual>)nearestInteractingNeighbors(object<Individual>$ receiver, [integer$ count = 1], [No<Subpopulation>$ exerterSubpop = NULL])
//
EidosValue_SP InteractionType::ExecuteMethod_nearestInteractingNeighbors(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *receiver_value = p_arguments[0].get();
	EidosValue *count_value = p_arguments[1].get();
	EidosValue *exerterSubpop_value = p_arguments[2].get();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): nearestInteractingNeighbors() requires that the interaction be spatial." << EidosTerminate();
	
	// receiver_value is guaranteed to be singleton; let's get the info on it
	Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
	slim_popsize_t receiver_index_in_subpop = receiver->index_;
	
	if (receiver_index_in_subpop < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): nearestInteractingNeighbors() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
	
	Subpopulation *receiver_subpop = receiver->subpopulation_;
	Species &receiver_species = receiver_subpop->species_;
	
	CheckSpeciesCompatibility(receiver_species);
	
	InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
	double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
	
	// the exerter subpopulation defaults to the same subpop as the receivers
	Subpopulation *exerter_subpop = ((exerterSubpop_value->Type() == EidosValueType::kValueNULL) ? receiver_subpop : (Subpopulation *)exerterSubpop_value->ObjectElementAtIndex(0, nullptr));
	
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	slim_popsize_t exerter_subpop_size = exerter_subpop->parent_subpop_size_;
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	EnsureKDTreePresent(exerter_subpop_data);
	
	// Check the count
	int64_t count = count_value->IntAtIndex(0, nullptr);
	
	if (count < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): nearestInteractingNeighbors() requires count >= 0." << EidosTerminate();
	
	if (count > exerter_subpop_size)
		count = exerter_subpop_size;
	
	if (count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	// Check sex-specificity for the receiver; if the individual is disqualified, there are no interacting neighbors
	if ((receiver_sex_ != IndividualSex::kUnspecified) && (receiver_sex_ != receiver->sex_))
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	// Find the neighbors
	std::vector<Individual *> &exerters = exerter_subpop->parent_individuals_;
	SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kDistances);
	FillSparseVectorForReceiverDistances(sv, receiver, receiver_position, exerter_subpop, exerter_subpop_data);
	uint32_t nnz;
	const uint32_t *columns;
	const sv_value_t *distances;
	
	distances = sv->Distances(&nnz, &columns);
	
	if (count >= nnz)
	{
		// return all of the individuals in the row
		EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->resize_no_initialize(nnz);
		
		for (uint32_t col_index = 0; col_index < nnz; ++col_index)
			result_vec->set_object_element_no_check_NORR(exerters[columns[col_index]], col_index);
		
		FreeSparseVector(sv);
		return EidosValue_SP(result_vec);
	}
	else if (count == 1)
	{
		// return the individual in the row with the smallest distance
		uint32_t min_col_index = UINT32_MAX;
		double min_distance = INFINITY;
		
		for (uint32_t col_index = 0; col_index < nnz; ++col_index)
			if (distances[col_index] < min_distance)
			{
				min_distance = distances[col_index];
				min_col_index = col_index;
			}
		
		if (min_distance < INFINITY)
		{
			FreeSparseVector(sv);
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(exerters[columns[min_col_index]], gSLiM_Individual_Class));
		}
	}
	else	// (count < nnz)
	{
		// return the <count> individuals with the smallest distances
		std::vector<std::pair<uint32_t, sv_value_t>> neighbors;
		
		for (uint32_t col_index = 0; col_index < nnz; ++col_index)
			neighbors.emplace_back(col_index, distances[col_index]);
		
		std::sort(neighbors.begin(), neighbors.end(), [](const std::pair<uint32_t, sv_value_t> &l, const std::pair<uint32_t, sv_value_t> &r) {
			return l.second < r.second;
		});
		
		EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->resize_no_initialize(count);
		
		for (uint32_t neighbor_index = 0; neighbor_index < count; ++neighbor_index)
		{
			Individual *exerter = exerters[columns[neighbors[neighbor_index].first]];
			
			result_vec->set_object_element_no_check_NORR(exerter, neighbor_index);
		}
		
		FreeSparseVector(sv);
		return EidosValue_SP(result_vec);
	}
	
	FreeSparseVector(sv);
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
}

//	*********************	– (object<Individual>)nearestNeighbors(object<Individual>$ receiver, [integer$ count = 1], [No<Subpopulation>$ exerterSubpop = NULL])
//
EidosValue_SP InteractionType::ExecuteMethod_nearestNeighbors(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *receiver_value = p_arguments[0].get();
	EidosValue *count_value = p_arguments[1].get();
	EidosValue *exerterSubpop_value = p_arguments[2].get();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighbors): nearestNeighbors() requires that the interaction be spatial." << EidosTerminate();
	
	// receiver_value is guaranteed to be singleton; let's get the info on it
	Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
	slim_popsize_t receiver_index_in_subpop = receiver->index_;
	
	if (receiver_index_in_subpop < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighbors): nearestNeighbors() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
	
	Subpopulation *receiver_subpop = receiver->subpopulation_;
	Species &receiver_species = receiver_subpop->species_;
	
	CheckSpeciesCompatibility(receiver_species);
	
	InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
	double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
	
	// figure out the exerter subpopulation and get info on it
	Subpopulation *exerter_subpop = ((exerterSubpop_value->Type() == EidosValueType::kValueNULL) ? receiver_subpop : (Subpopulation *)exerterSubpop_value->ObjectElementAtIndex(0, nullptr));
	
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	// Check the count
	slim_popsize_t exerter_subpop_size = exerter_subpop->parent_subpop_size_;
	int64_t count = count_value->IntAtIndex(0, nullptr);
	
	if (count < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighbors): nearestNeighbors() requires count >= 0." << EidosTerminate();
	
	if (count > exerter_subpop_size)
		count = exerter_subpop_size;
	
	if (count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	// Find the neighbors
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	EnsureKDTreePresent(exerter_subpop_data);
	
	EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->reserve((int)count);
	
	FindNeighbors(exerter_subpop, exerter_subpop_data, receiver_position, (int)count, *result_vec, receiver);
	
	return EidosValue_SP(result_vec);
}

//	*********************	– (object<Individual>)nearestNeighborsOfPoint(float point, io<Subpopulation>$ exerterSubpop, [integer$ count = 1])
//
EidosValue_SP InteractionType::ExecuteMethod_nearestNeighborsOfPoint(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *point_value = p_arguments[0].get();
	EidosValue *exerterSubpop_value = p_arguments[1].get();
	EidosValue *count_value = p_arguments[2].get();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighborsOfPoint): nearestNeighborsOfPoint() requires that the interaction be spatial." << EidosTerminate();
	
	// Check the subpop
	Subpopulation *exerter_subpop = SLiM_ExtractSubpopulationFromEidosValue_io(exerterSubpop_value, 0, &community_, nullptr, "nearestNeighborsOfPoint()");
	Species &exerter_species = exerter_subpop->species_;
	
	CheckSpeciesCompatibility(exerter_species);
	
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	EnsureKDTreePresent(exerter_subpop_data);
	
	// Check the point
	if (point_value->Count() != spatiality_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighborsOfPoint): nearestNeighborsOfPoint() requires that point is of length equal to the interaction spatiality." << EidosTerminate();
	
	double point_array[SLIM_MAX_DIMENSIONALITY];
	
	for (int point_index = 0; point_index < spatiality_; ++point_index)
		point_array[point_index] = point_value->FloatAtIndex(point_index, nullptr);
	
	// Check the count
	slim_popsize_t exerter_subpop_size = exerter_subpop->parent_subpop_size_;
	int64_t count = count_value->IntAtIndex(0, nullptr);
	
	if (count < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighborsOfPoint): nearestNeighborsOfPoint() requires count >= 0." << EidosTerminate();
	
	if (count > exerter_subpop_size)
		count = exerter_subpop_size;
	
	if (count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	// Find the neighbors
	EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class))->reserve((int)count);
	
	FindNeighbors(exerter_subpop, exerter_subpop_data, point_array, (int)count, *result_vec, nullptr);
	
	return EidosValue_SP(result_vec);
}

//	*********************	– (integer)neighborCount(object<Individual> receivers, [No<Subpopulation>$ exerterSubpop = NULL])
//
EidosValue_SP InteractionType::ExecuteMethod_neighborCount(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *receivers_value = p_arguments[0].get();
	EidosValue *exerterSubpop_value = p_arguments[1].get();
	int receivers_count = receivers_value->Count();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCount): neighborCount() requires that the interaction be spatial." << EidosTerminate();
	
	if (receivers_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	// the exerter subpopulation defaults to the same subpop as the receivers
	Subpopulation *receiver_subpop = ((Individual *)receivers_value->ObjectElementAtIndex(0, nullptr))->subpopulation_;
	Subpopulation *exerter_subpop = ((exerterSubpop_value->Type() == EidosValueType::kValueNULL) ? receiver_subpop : (Subpopulation *)exerterSubpop_value->ObjectElementAtIndex(0, nullptr));
	
	CheckSpeciesCompatibility(receiver_subpop->species_);
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	if (exerter_subpop->parent_subpop_size_ == 0)
	{
		// If the exerter subpop is empty then all count values for the receivers are zero
		if (receivers_count == 1)
		{
			return gStaticEidosValue_Integer0;
		}
		else
		{
			EidosValue_Int_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(receivers_count);
			
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
				result_vec->set_int_no_check(0, receiver_index);
			
			return EidosValue_SP(result_vec);
		}
	}
	
	InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	EnsureKDTreePresent(exerter_subpop_data);

	if (receivers_count == 1)
	{
		// Just one value, so we can return a singleton and skip some work
		Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(0, nullptr);
		slim_popsize_t receiver_index_in_subpop = receiver->index_;
		
		if (receiver_index_in_subpop < 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCount): neighborCount() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
	
		// Find the neighbors
		double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
		slim_popsize_t focal_individual_index = (exerter_subpop == receiver_subpop) ? receiver_index_in_subpop : -1;
		int neighborCount;
		
		switch (spatiality_)
		{
			case 1: neighborCount = CountNeighbors_1(exerter_subpop_data.kd_root_, receiver_position, focal_individual_index);			break;
			case 2: neighborCount = CountNeighbors_2(exerter_subpop_data.kd_root_, receiver_position, focal_individual_index, 0);		break;
			case 3: neighborCount = CountNeighbors_3(exerter_subpop_data.kd_root_, receiver_position, focal_individual_index, 0);		break;
			default: EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCount): (internal error) unsupported spatiality" << EidosTerminate();
		}
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(neighborCount));
	}
	else
	{
		EidosValue_Int_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(receivers_count);
		
		for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
		{
			Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(receiver_index, nullptr);
			slim_popsize_t receiver_index_in_subpop = receiver->index_;
			
			if (receiver_index_in_subpop < 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCount): neighborCount() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
			
			// SPECIES CONSISTENCY CHECK
			if (receiver_subpop != receiver->subpopulation_)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCount): neighborCount() requires that all receivers be in the same subpopulation." << EidosTerminate();
			
			// Find the neighbors
			double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			slim_popsize_t focal_individual_index = (exerter_subpop == receiver_subpop) ? receiver_index_in_subpop : -1;
			int neighborCount;
			
			switch (spatiality_)
			{
				case 1: neighborCount = CountNeighbors_1(exerter_subpop_data.kd_root_, receiver_position, focal_individual_index);			break;
				case 2: neighborCount = CountNeighbors_2(exerter_subpop_data.kd_root_, receiver_position, focal_individual_index, 0);		break;
				case 3: neighborCount = CountNeighbors_3(exerter_subpop_data.kd_root_, receiver_position, focal_individual_index, 0);		break;
				default: EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCount): (internal error) unsupported spatiality" << EidosTerminate();
			}
			
			result_vec->set_int_no_check(neighborCount, receiver_index);
		}
		
		return EidosValue_SP(result_vec);
	}
}

//	*********************	– (integer$)neighborCountOfPoint(float point, io<Subpopulation>$ exerterSubpop)
//
EidosValue_SP InteractionType::ExecuteMethod_neighborCountOfPoint(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *point_value = p_arguments[0].get();
	EidosValue *exerterSubpop_value = p_arguments[1].get();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCountOfPoint): neighborCountOfPoint() requires that the interaction be spatial." << EidosTerminate();
	
	// Check the subpop
	Subpopulation *exerter_subpop = SLiM_ExtractSubpopulationFromEidosValue_io(exerterSubpop_value, 0, &community_, nullptr, "nearestNeighborsOfPoint()");
	Species &exerter_species = exerter_subpop->species_;
	
	CheckSpeciesCompatibility(exerter_species);
	
	if (exerter_subpop->parent_subpop_size_ == 0)
		return gStaticEidosValue_Integer0;
	
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	EnsureKDTreePresent(exerter_subpop_data);

	// Check the point
	if (point_value->Count() != spatiality_)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCountOfPoint): neighborCountOfPoint() requires that point is of length equal to the interaction spatiality." << EidosTerminate();
	
	double point_array[SLIM_MAX_DIMENSIONALITY];
	
	for (int point_index = 0; point_index < spatiality_; ++point_index)
		point_array[point_index] = point_value->FloatAtIndex(point_index, nullptr);
	
	// Find the neighbors
	int neighborCount;
	
	switch (spatiality_)
	{
		case 1: neighborCount = CountNeighbors_1(exerter_subpop_data.kd_root_, point_array, -1);		break;
		case 2: neighborCount = CountNeighbors_2(exerter_subpop_data.kd_root_, point_array, -1, 0);		break;
		case 3: neighborCount = CountNeighbors_3(exerter_subpop_data.kd_root_, point_array, -1, 0);		break;
		default: EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCountOfPoint): (internal error) unsupported spatiality" << EidosTerminate();
	}
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(neighborCount));
}

//	*********************	- (void)setInteractionFunction(string$ functionType, ...)
//
EidosValue_SP InteractionType::ExecuteMethod_setInteractionFunction(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue_String *functionType_value = (EidosValue_String *)p_arguments[0].get();
	
	const std::string &if_type_string = functionType_value->StringRefAtIndex(0, nullptr);
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
	
	if (if_type_ == IFType::kNormal)
		n_2param2sq_ = 2.0 * if_param2_ * if_param2_;
	
	// mark that interaction types changed, so they get redisplayed in SLiMgui
	community_.interaction_types_changed_ = true;
	
	// changing the interaction function invalidates the cached clipped_integral_ buffer; we don't deallocate it, just invalidate it
	clipped_integral_valid_ = false;
	
	return gStaticEidosValueVOID;
}

//	*********************	– (float)strength(object<Individual>$ receiver, [No<Individual> exerters = NULL])
//
EidosValue_SP InteractionType::ExecuteMethod_strength(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *receiver_value = p_arguments[0].get();
	EidosValue *exerters_value = p_arguments[1].get();
	
	// receiver_value is guaranteed to be singleton; let's get the info on it
	Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
	slim_popsize_t receiver_index_in_subpop = receiver->index_;
	
	if (receiver_index_in_subpop < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_strength): strength() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
	
	Subpopulation *receiver_subpop = receiver->subpopulation_;
	Species &receiver_species = receiver_subpop->species_;
	
	CheckSpeciesCompatibility(receiver_species);
	
	// figure out the exerter subpopulation and get info on it
	bool exerters_value_NULL = (exerters_value->Type() == EidosValueType::kValueNULL);
	int exerters_count = exerters_value->Count();
	
	if ((exerters_count == 0) && !exerters_value_NULL)
		return gStaticEidosValue_Float_ZeroVec;
	
	Subpopulation *exerter_subpop = (exerters_value_NULL ? receiver_subpop : ((Individual *)exerters_value->ObjectElementAtIndex(0, nullptr))->subpopulation_);
	
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	slim_popsize_t exerter_subpop_size = exerter_subpop->parent_subpop_size_;
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	
	// Check sex-specificity for the receiver; if the individual is disqualified, the distance to each exerter is zero
	if (exerters_value_NULL)
		exerters_count = exerter_subpop_size;
	
	if ((receiver_sex_ != IndividualSex::kUnspecified) && (receiver_sex_ != receiver->sex_))
	{
		EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerters_count);
		
		EIDOS_BZERO(result_vec->data(), exerter_subpop_size * sizeof(double));
		
		return EidosValue_SP(result_vec);
	}
	
	if (spatiality_)
	{
		// Spatial case; we use the k-d tree to get strengths for all neighbors.  For non-null exerters_value, we could
		// calculate distances and strengths with the receiver directly, to save building the sparse vector; FIXME.
		InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
		double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
		
		EnsureKDTreePresent(exerter_subpop_data);
		
		SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kStrengths);
		FillSparseVectorForReceiverStrengths(sv, receiver, receiver_position, exerter_subpop, exerter_subpop_data);
		uint32_t nnz;
		const uint32_t *columns;
		const sv_value_t *strengths;
		
		strengths = sv->Strengths(&nnz, &columns);
		
		// set up our return value
		EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerters_count);
		
		if (exerters_value_NULL)
		{
			// NULL means return distances from individuals1 (which must be singleton) to all individuals in the subpopulation
			// We initialize the return vector to 0, and fill in non-zero values from the sparse vector
			double *result_ptr = result_vec->data();
			
			EIDOS_BZERO(result_ptr, exerter_subpop_size * sizeof(double));
			
			for (uint32_t col_index = 0; col_index < nnz; ++col_index)
				*(result_ptr + columns[col_index]) = strengths[col_index];
		}
		else
		{
			// Otherwise, receiver is singleton, and exerters_value is any length, so we loop over exerters
			// Each individual in exerters_value requires a linear search through the sparse vector, unfortunately
			for (int exerter_index = 0; exerter_index < exerters_count; ++exerter_index)
			{
				Individual *exerter = (Individual *)exerters_value->ObjectElementAtIndex(exerter_index, nullptr);
				
				// SPECIES CONSISTENCY CHECK
				if (exerter_subpop != exerter->subpopulation_)
				{
					InteractionType::FreeSparseVector(sv);
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_strength): strength() requires that all exerters be in the same subpopulation." << EidosTerminate();
				}
				
				slim_popsize_t exerter_index_in_subpop = exerter->index_;
				
				if (exerter_index_in_subpop < 0)
				{
					InteractionType::FreeSparseVector(sv);
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_strength): strength() requires that exerters are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
				}
				
				double strength = 0;
				
				for (uint32_t col_index = 0; col_index < nnz; ++col_index)
					if ((slim_popsize_t)columns[col_index] == exerter_index_in_subpop)
					{
						strength = strengths[col_index];
						break;
					}
				
				result_vec->set_float_no_check(strength, exerter_index);
			}
		}
		
		InteractionType::FreeSparseVector(sv);
		return EidosValue_SP(result_vec);
	}
	else
	{
		// Non-spatial case; no distances used.  We have to worry about sex-segregation; it is not handled for us.
		slim_popsize_t receiver_index = ((exerter_subpop == receiver->subpopulation_) && (receiver->index_ >= 0) ? receiver->index_ : -1);
		std::vector<SLiMEidosBlock*> &callbacks = exerter_subpop_data.evaluation_interaction_callbacks_;
		
		if (exerters_value->Type() == EidosValueType::kValueNULL)
		{
			// NULL means return strengths to individuals1 (which must be singleton) from all individuals in the subpopulation
			EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerter_subpop_size);
			
			for (int exerter_index = 0; exerter_index < exerter_subpop_size; ++exerter_index)
			{
				double strength = 0;
				
				if (exerter_index != receiver_index)
				{
					Individual *exerter = exerter_subpop->parent_individuals_[exerter_index];
					
					if ((exerter_sex_ == IndividualSex::kUnspecified) || (exerter_sex_ == exerter->sex_))
						strength = ApplyInteractionCallbacks(receiver, exerter, if_param1_, NAN, callbacks);	// hard-coding interaction function "f" (IFType::kFixed), which is required
				}
				
				result_vec->set_float_no_check(strength, exerter_index);
			}
			
			return EidosValue_SP(result_vec);
		}
		else
		{
			// Otherwise, individuals1 is singleton, and exerters_value is any length, so we loop over exerters_value
			EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerters_count);
			
			for (int exerter_index = 0; exerter_index < exerters_count; ++exerter_index)
			{
				Individual *exerter = (Individual *)exerters_value->ObjectElementAtIndex(exerter_index, nullptr);
				
				if (exerter_subpop != exerter->subpopulation_)
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_strength): strength() requires that all individuals be in the same subpopulation." << EidosTerminate();
				
				slim_popsize_t exerter_index_in_subpop = exerter->index_;
				
				if (exerter_index_in_subpop < 0)
					EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_strength): strength() requires that exerters are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
				
				double strength = 0;
				
				if (exerter_index_in_subpop != receiver_index)
					if ((exerter_sex_ == IndividualSex::kUnspecified) || (exerter_sex_ == exerter->sex_))
						strength = ApplyInteractionCallbacks(receiver, exerter, if_param1_, NAN, callbacks);	// hard-coding interaction function "f" (IFType::kFixed), which is required
				
				result_vec->set_float_no_check(strength, exerter_index);
			}
			
			return EidosValue_SP(result_vec);
		}
	}
}

//	*********************	– (float)totalOfNeighborStrengths(object<Individual> receivers, [No<Subpopulation>$ exerterSubpop = NULL])
//
EidosValue_SP InteractionType::ExecuteMethod_totalOfNeighborStrengths(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *receivers_value = p_arguments[0].get();
	EidosValue *exerterSubpop_value = p_arguments[1].get();
	int receivers_count = receivers_value->Count();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): totalOfNeighborStrengths() requires that the interaction be spatial." << EidosTerminate();
	
	if (receivers_count == 0)
		return gStaticEidosValue_Float_ZeroVec;
	
	// the exerter subpopulation defaults to the same subpop as the receivers
	Subpopulation *receiver_subpop = ((Individual *)receivers_value->ObjectElementAtIndex(0, nullptr))->subpopulation_;
	Subpopulation *exerter_subpop = ((exerterSubpop_value->Type() == EidosValueType::kValueNULL) ? receiver_subpop : (Subpopulation *)exerterSubpop_value->ObjectElementAtIndex(0, nullptr));
	
	CheckSpeciesCompatibility(receiver_subpop->species_);
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	if (exerter_subpop->parent_subpop_size_ == 0)
	{
		// If the exerter subpop is empty then all strength totals for the receivers are zero
		if (receivers_count == 1)
		{
			return gStaticEidosValue_Float0;
		}
		else
		{
			EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(receivers_count);
			
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
				result_vec->set_float_no_check(0, receiver_index);
			
			return EidosValue_SP(result_vec);
		}
	}
	
	InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	EnsureKDTreePresent(exerter_subpop_data);
	
	if (receivers_count == 1)
	{
		// Just one value, so we can return a singleton and skip some work
		Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(0, nullptr);
		slim_popsize_t receiver_index_in_subpop = receiver->index_;
		
		if (receiver_index_in_subpop < 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): totalOfNeighborStrengths() requires that receivers are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
		
		// Check sex-specificity for the receiver; if the individual is disqualified, the total is zero
		if ((receiver_sex_ != IndividualSex::kUnspecified) && (receiver_sex_ != receiver->sex_))
			return gStaticEidosValue_Integer0;
		
		double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
		SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kStrengths);
		FillSparseVectorForReceiverStrengths(sv, receiver, receiver_position, exerter_subpop, exerter_subpop_data);
		
		// Get the sparse vector data
		uint32_t nnz;
		const sv_value_t *strengths;
		
		strengths = sv->Strengths(&nnz);
		
		// Total the interaction strengths
		double total_strength = 0.0;
		
		for (uint32_t col_index = 0; col_index < nnz; ++col_index)
			total_strength += strengths[col_index];
		
		InteractionType::FreeSparseVector(sv);
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Float_singleton(total_strength));
	}
	else
	{
		// Loop over the requested individuals and get the totals
		EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(receivers_count);
		
		for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
		{
			Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(receiver_index, nullptr);
			slim_popsize_t receiver_index_in_subpop = receiver->index_;
			
			if (receiver_index_in_subpop < 0)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): totalOfNeighborStrengths() requires that receivers are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
			
			// SPECIES CONSISTENCY CHECK
			if (receiver_subpop != receiver->subpopulation_)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): totalOfNeighborStrengths() requires that all receivers be in the same subpopulation." << EidosTerminate();
			
			// Check sex-specificity for the receiver; if the individual is disqualified, the total is zero
			if ((receiver_sex_ != IndividualSex::kUnspecified) && (receiver_sex_ != receiver->sex_))
			{
				result_vec->set_float_no_check(0, receiver_index);
				continue;
			}
			
			double *receiver_position = receiver_subpop_data.positions_ + receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kStrengths);
			FillSparseVectorForReceiverStrengths(sv, receiver, receiver_position, exerter_subpop, exerter_subpop_data);
			
			// Get the sparse vector data
			uint32_t nnz;
			const sv_value_t *strengths;
			
			strengths = sv->Strengths(&nnz);
			
			// Total the interaction strengths
			double total_strength = 0.0;
			
			for (uint32_t col_index = 0; col_index < nnz; ++col_index)
				total_strength += strengths[col_index];
			
			result_vec->set_float_no_check(total_strength, receiver_index);
			InteractionType::FreeSparseVector(sv);
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

EidosClass *gSLiM_InteractionType_Class = nullptr;


const std::vector<EidosPropertySignature_CSP> *InteractionType_Class::Properties(void) const
{
	static std::vector<EidosPropertySignature_CSP> *properties = nullptr;
	
	if (!properties)
	{
		properties = new std::vector<EidosPropertySignature_CSP>(*super::Properties());
		
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
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_clippedIntegral, kEidosValueMaskFloat))->AddObject_N("receivers", gSLiM_Individual_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_distance, kEidosValueMaskFloat))->AddObject_S("receiver", gSLiM_Individual_Class)->AddObject_ON("exerters", gSLiM_Individual_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_distanceFromPoint, kEidosValueMaskFloat))->AddFloat("point")->AddObject("exerters", gSLiM_Individual_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_drawByStrength, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("receiver", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddObject_OSN("exerterSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_evaluate, kEidosValueMaskVOID))->AddIntObject("subpops", gSLiM_Subpopulation_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_interactingNeighborCount, kEidosValueMaskInt))->AddObject("receivers", gSLiM_Individual_Class)->AddObject_OSN("exerterSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_localPopulationDensity, kEidosValueMaskFloat))->AddObject("receivers", gSLiM_Individual_Class)->AddObject_OSN("exerterSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_interactionDistance, kEidosValueMaskFloat))->AddObject_S("receiver", gSLiM_Individual_Class)->AddObject_ON("exerters", gSLiM_Individual_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_nearestInteractingNeighbors, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("receiver", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddObject_OSN("exerterSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_nearestNeighbors, kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject_S("receiver", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddObject_OSN("exerterSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_nearestNeighborsOfPoint, kEidosValueMaskObject, gSLiM_Individual_Class))->AddFloat("point")->AddIntObject_S("exerterSubpop", gSLiM_Subpopulation_Class)->AddInt_OS("count", gStaticEidosValue_Integer1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_neighborCount, kEidosValueMaskInt))->AddObject("receivers", gSLiM_Individual_Class)->AddObject_OSN("exerterSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_neighborCountOfPoint, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddFloat("point")->AddIntObject_S("exerterSubpop", gSLiM_Subpopulation_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setInteractionFunction, kEidosValueMaskVOID))->AddString_S("functionType")->AddEllipsis());
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_strength, kEidosValueMaskFloat))->AddObject_S("receiver", gSLiM_Individual_Class)->AddObject_ON("exerters", gSLiM_Individual_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_totalOfNeighborStrengths, kEidosValueMaskFloat))->AddObject("receivers", gSLiM_Individual_Class)->AddObject_OSN("exerterSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL));
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
	kd_nodes_ = p_source.kd_nodes_;
	kd_root_ = p_source.kd_root_;
	
	p_source.evaluated_ = false;
	p_source.evaluation_interaction_callbacks_.clear();
	p_source.individual_count_ = 0;
	p_source.first_male_index_ = 0;
	p_source.kd_node_count_ = 0;
	p_source.positions_ = nullptr;
	p_source.kd_nodes_ = nullptr;
	p_source.kd_root_ = nullptr;
}

_InteractionsData& _InteractionsData::operator=(_InteractionsData&& p_source)
{
	if (this != &p_source)  
	{
		if (positions_)
			free(positions_);
		if (kd_nodes_)
			free(kd_nodes_);
		
		evaluated_ = p_source.evaluated_;
		evaluation_interaction_callbacks_.swap(p_source.evaluation_interaction_callbacks_);
		individual_count_ = p_source.individual_count_;
		first_male_index_ = p_source.first_male_index_;
		kd_node_count_ = p_source.kd_node_count_;
		positions_ = p_source.positions_;
		kd_nodes_ = p_source.kd_nodes_;
		kd_root_ = p_source.kd_root_;
		
		p_source.evaluated_ = false;
		p_source.evaluation_interaction_callbacks_.clear();
		p_source.individual_count_ = 0;
		p_source.first_male_index_ = 0;
		p_source.kd_node_count_ = 0;
		p_source.positions_ = nullptr;
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
	
	if (kd_nodes_)
	{
		free(kd_nodes_);
		kd_nodes_ = nullptr;
	}
	
	kd_root_ = nullptr;
	
	// Unnecessary since it's about to be destroyed anyway
	//evaluation_interaction_callbacks_.clear();
}































