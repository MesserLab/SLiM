//
//  interaction_type.cpp
//  SLiM
//
//  Created by Ben Haller on 2/25/17.
//  Copyright (c) 2017-2023 Philipp Messer.  All rights reserved.
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


#pragma mark -
#pragma mark InteractionType
#pragma mark -

// The SparseVector pool structure depends on whether we are built single-threaded or multi-threaded; see interaction_type.h
#ifdef _OPENMP

std::vector<std::vector<SparseVector *>> InteractionType::s_freed_sparse_vectors_PERTHREAD;
#if DEBUG
std::vector<int> InteractionType::s_sparse_vector_count_PERTHREAD;
#endif

#else

std::vector<SparseVector *> InteractionType::s_freed_sparse_vectors_SINGLE;
#if DEBUG
int InteractionType::s_sparse_vector_count_SINGLE = 0;
#endif

#endif

void InteractionType::_WarmUp(void)
{
	// Called by InteractionType_Class::InteractionType_Class() when it is created during warmup
	static bool beenHere = false;
	
	if (!beenHere) {
		THREAD_SAFETY_IN_ANY_PARALLEL("InteractionType::_WarmUp(): not warmed up");
		
#ifdef _OPENMP
		// set up per-thread sparse vector pools to avoid lock contention
		s_freed_sparse_vectors_PERTHREAD.resize(gEidosMaxThreads);
		#if DEBUG
		s_sparse_vector_count_PERTHREAD.resize(gEidosMaxThreads, 0);
		#endif
#endif
		
		beenHere = true;
	}
}

InteractionType::InteractionType(Community &p_community, slim_objectid_t p_interaction_type_id, std::string p_spatiality_string, bool p_reciprocal, double p_max_distance, IndividualSex p_receiver_sex, IndividualSex p_exerter_sex) :
	self_symbol_(EidosStringRegistry::GlobalStringIDForString(SLiMEidosScript::IDStringWithPrefix('i', p_interaction_type_id)),
			 EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(this, gSLiM_InteractionType_Class))),
	spatiality_string_(std::move(p_spatiality_string)), reciprocal_(p_reciprocal), max_distance_(p_max_distance), max_distance_sq_(p_max_distance * p_max_distance), if_type_(SpatialKernelType::kFixed), if_param1_(1.0), if_param2_(0.0),
	community_(p_community), interaction_type_id_(p_interaction_type_id)
{
	// Figure out our spatiality, which is the number of spatial dimensions we actively use for distances
	if (spatiality_string_ == "")			{ spatiality_ = 0; required_dimensionality_ = 0; }
	else if (spatiality_string_ == "x")		{ spatiality_ = 1; required_dimensionality_ = 1; }
	else if (spatiality_string_ == "y")		{ spatiality_ = 1; required_dimensionality_ = 2; }
	else if (spatiality_string_ == "z")		{ spatiality_ = 1; required_dimensionality_ = 3; }
	else if (spatiality_string_ == "xy")	{ spatiality_ = 2; required_dimensionality_ = 2; }
	else if (spatiality_string_ == "xz")	{ spatiality_ = 2; required_dimensionality_ = 3; }		// NOLINT(*-branch-clone) : intentional branch clone
	else if (spatiality_string_ == "yz")	{ spatiality_ = 2; required_dimensionality_ = 3; }
	else if (spatiality_string_ == "xyz")	{ spatiality_ = 3; required_dimensionality_ = 3; }
	else
		EIDOS_TERMINATION << "ERROR (InteractionType::InteractionType): initializeInteractionType() spatiality '" << spatiality_string_ << "' must be '', 'x', 'y', 'z', 'xy', 'xz', 'yz', or 'xyz'." << EidosTerminate();
	
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
	
	// sex-segregation can be configured here, for historical reasons; see setConstraints() for all other constraint setting
	if ((p_receiver_sex != IndividualSex::kUnspecified) || (p_exerter_sex != IndividualSex::kUnspecified))
	{
		if (single_species && !single_species->SexEnabled())
			EIDOS_TERMINATION << "ERROR (InteractionType::InteractionType): initializeInteractionType() sexSegregation value other than '**' are unsupported in non-sexual simulations." << EidosTerminate();
		
		if (p_receiver_sex != IndividualSex::kUnspecified)
		{
			receiver_constraints_.sex_ = p_receiver_sex;
			receiver_constraints_.has_constraints_ = true;
		}
		
		if (p_exerter_sex != IndividualSex::kUnspecified)
		{
			exerter_constraints_.sex_ = p_exerter_sex;
			exerter_constraints_.has_constraints_ = true;
		}
	}
	
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
	// interaction() callbacks) is frozen at the same time.  Evaluate is necessary because the k-d trees are built
	// once and used to serve many queries, typically, and so they must be built based upon a fixed state snapshot.
	Species &species = p_subpop->species_;
	slim_objectid_t subpop_id = p_subpop->subpopulation_id_;
	slim_popsize_t subpop_size = p_subpop->parent_subpop_size_;
	Individual **subpop_individuals = p_subpop->parent_individuals_.data();
	
	// Check that the subpopulation is compatible with the configuration of this interaction type
	// At this stage, we don't know whether it will be used as a receiver, exerter, or both
	CheckSpeciesCompatibility_Generic(p_subpop->species_);
	
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
		
		// Ensure that other parts of the subpop data block are correctly reset to the same state that Invalidate()
		// uses; normally this has already been done by Initialize(), but not necessarily.
		// FIXME we could keep the positions array allocated; we would then need a flag indicating whether it's valid.
		if (subpop_data->positions_)
		{
			free(subpop_data->positions_);
			subpop_data->positions_ = nullptr;
		}
		
		// Free both k-d trees, keeping in mind that the two might share their memory.  FIXME we could keep the
		// k-d tree buffers around and reuse them; we would then need a flag indicating whether they're valid.
		if (subpop_data->kd_nodes_ALL_ == subpop_data->kd_nodes_EXERTERS_)
			subpop_data->kd_nodes_EXERTERS_ = nullptr;
		
		if (subpop_data->kd_nodes_ALL_)
		{
			free(subpop_data->kd_nodes_ALL_);
			subpop_data->kd_nodes_ALL_ = nullptr;
		}
		
		if (subpop_data->kd_nodes_EXERTERS_)
		{
			free(subpop_data->kd_nodes_EXERTERS_);
			subpop_data->kd_nodes_EXERTERS_ = nullptr;
		}
		
		subpop_data->kd_root_ALL_ = nullptr;
		subpop_data->kd_node_count_ALL_ = 0;
		
		subpop_data->kd_root_EXERTERS_ = nullptr;
		subpop_data->kd_node_count_EXERTERS_ = 0;
		
		// Free the interaction() callbacks that were cached
		subpop_data->evaluation_interaction_callbacks_.clear();
	}
	
	// At this point, positions_ is guaranteed to be nullptr, as are the k-d tree buffers.
	// Now we mark ourselves evaluated and fill in buffers as needed.
	subpop_data->evaluated_ = true;
	
	// At a minimum, fetch positional data from the subpopulation; this is guaranteed to be present (for spatiality > 0)
	if (spatiality_ > 0)
	{
		double *positions = (double *)malloc((size_t)subpop_size * SLIM_MAX_DIMENSIONALITY * sizeof(double));
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
	
	// BCH 10/1/2023: For SLiM 4.1 this policy is now altered slightly.  If non-sex exerter constraints are set, we need to cache
	// the EXERTER k-d tree nodes here, because those constraints need to be applied to the state of individuals at snapshot
	// time.  We do not build the tree, just cache its nodes so it knows which individuals it contains.  This is potentially
	// a little bit wasteful, if a subpopulation that is evaluated is used only for receivers, not for exerters; that is
	// a fairly uncommon usage pattern, and the overhead of caching the nodes is pretty minimal -- O(N) to cache, versus
	// O(N log N) to build the tree, and the constant factor for both operations is small.
	if ((spatiality_ > 0) && (exerter_constraints_.has_nonsex_constraints_))
	{
		// There is one little hitch.  CacheKDTreeNodes() will call CheckIndividualNonSexConstraints(), and that
		// method will raise if an exerter constraint exists for a tag/tagL value but a candidate individual
		// doesn't have that tag/tagL value defined.  If the k-d tree is actually going to be used to find exerters,
		// then that raise is appropriate; an exerter has an unset tag/tagL and so the constraint cannot be applied.
		// BUT if the k-d tree is only going to be used to find receivers, or perhaps not at all, then the raise is
		// not appropriate and needs to be suppressed.  SO, here we pre-test for it, and set a flag remembering that
		// "this subpop_data cannot be used to find exerters, because their state is non-compliant with the exerter
		// constraints".  We check that flag in EnsureKDTreePresent_EXERTERS() and raise there, when we are certain
		// that the tree is actually being used.
		for (int i = 0; i < subpop_size; ++i)
		{
			Individual *ind = subpop_individuals[i];
			
			if (!_PrecheckIndividualNonSexConstraints(ind, exerter_constraints_))
			{
				// The k-d tree for this subpopulation will not get cached, because of an unset tag/tagL; if the
				// user tries to use this subpop as an exerter subpop, EnsureKDTreePresent_EXERTERS() will raise,
				// but if the user does not try to do that, there is no problem.
				subpop_data->kd_constraints_raise_EXERTERS_ = true;
				return;
			}
		}
		
		// OK, it's safe to proceed with caching the exerter k-d tree; nobody will raise.
		CacheKDTreeNodes(p_subpop, *subpop_data, /* p_apply_exerter_constraints */ true, &subpop_data->kd_nodes_EXERTERS_, &subpop_data->kd_root_EXERTERS_, &subpop_data->kd_node_count_EXERTERS_);
	}
	
	// Note that receiver constraints are evaluated at query time, not here.  This means that they are applied to the
	// state of the receiver at query time, whereas exerter constraints are applied to the state of the exerter at
	// evaluate() time.  This discrepancy is intentional and documented.  The alternative would be to go through the
	// subpop here, at evaluate() time, and cache receiver eligibility for the whole subpopulation.  That would be
	// made more complex by the fact that receivers might have undefined tag values that are needed to apply the
	// constraints, but - as above for exerters - the raise from that condition must be suppressed until the individual
	// is actually used as a receiver in a query.  Doing that would be even more complex than for exerters, and the
	// performance penalty would be much larger than for exerters.
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
	
	// keep in mind that the two k-d trees may share their memory
	if (data.kd_nodes_ALL_ == data.kd_nodes_EXERTERS_)
		data.kd_nodes_EXERTERS_ = nullptr;
	
	if (data.kd_nodes_ALL_)
	{
		free(data.kd_nodes_ALL_);
		data.kd_nodes_ALL_ = nullptr;
	}
	
	if (data.kd_nodes_EXERTERS_)
	{
		free(data.kd_nodes_EXERTERS_);
		data.kd_nodes_EXERTERS_ = nullptr;
	}
	
	data.kd_root_ALL_ = nullptr;
	data.kd_node_count_ALL_ = 0;
	
	data.kd_root_EXERTERS_ = nullptr;
	data.kd_node_count_EXERTERS_ = 0;
	
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

void InteractionType::CheckSpeciesCompatibility_Generic(Species &species)
{
	// This checks that a given subpop (unknown whether receiver or exerter) is compatible with this interaction type
	if (required_dimensionality_ > species.SpatialDimensionality())
		EIDOS_TERMINATION << "ERROR (InteractionType::CheckSpeciesCompatibility_Generic): the exerter or receiver species has insufficient dimensionality to be used with this interaction type." << EidosTerminate();
	
	// For this "generic" case we do not check sex constraints at all.  This is useful partly when we don't know
	// whether the species will act as receiver or exerter, and partly when we specifically don't want to check
	// sex constraints, for queries like nearestNeighbors() that do not use constraints.
}

void InteractionType::CheckSpeciesCompatibility_Receiver(Species &species)
{
	// This checks that a given receiver subpop is compatible with this interaction type
	if (required_dimensionality_ > species.SpatialDimensionality())
		EIDOS_TERMINATION << "ERROR (InteractionType::CheckSpeciesCompatibility_Receiver): the receiver species has insufficient dimensionality to be used with this interaction type." << EidosTerminate();
	
	// If there is a sex constraint for receivers, then the receiver species must be sexual
	if ((receiver_constraints_.sex_ != IndividualSex::kUnspecified) && !species.SexEnabled())
		EIDOS_TERMINATION << "ERROR (InteractionType::CheckSpeciesCompatibility_Receiver): a sex constraint exists for receivers, but the receiver species is non-sexual." << EidosTerminate();
}

void InteractionType::CheckSpeciesCompatibility_Exerter(Species &species)
{
	// This checks that a given exerter subpop is compatible with this interaction type
	if (required_dimensionality_ > species.SpatialDimensionality())
		EIDOS_TERMINATION << "ERROR (InteractionType::CheckSpeciesCompatibility_Exerter): the exerter species has insufficient dimensionality to be used with this interaction type." << EidosTerminate();
	
	// If there is a sex constraint for receivers, then the receiver species must be sexual
	if ((exerter_constraints_.sex_ != IndividualSex::kUnspecified) && !species.SexEnabled())
		EIDOS_TERMINATION << "ERROR (InteractionType::CheckSpeciesCompatibility_Exerter): a sex constraint exists for exerters, but the exerter species is non-sexual." << EidosTerminate();
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
	// SEE ALSO: Kernel::DensityForDistance(), which is parallel to this
	switch (if_type_)
	{
		case SpatialKernelType::kFixed:
			return (if_param1_);																		// fmax
		case SpatialKernelType::kLinear:
			return (if_param1_ * (1.0 - p_distance / max_distance_));									// fmax * (1 − d/dmax)
		case SpatialKernelType::kExponential:
			return (if_param1_ * exp(-if_param2_ * p_distance));										// fmax * exp(−λd)
		case SpatialKernelType::kNormal:
			return (if_param1_ * exp(-(p_distance * p_distance) / n_2param2sq_));						// fmax * exp(−d^2/2σ^2)
		case SpatialKernelType::kCauchy:
		{
			double temp = p_distance / if_param2_;
			return (if_param1_ / (1.0 + temp * temp));													// fmax / (1+(d/λ)^2)
		}
		case SpatialKernelType::kStudentsT:
			return SpatialKernel::tdist(p_distance, if_param1_, if_param2_, if_param3_);				// fmax / (1+(d/t)^2/n)^(−(ν+1)/2)
	}
	EIDOS_TERMINATION << "ERROR (InteractionType::CalculateStrengthNoCallbacks): (internal error) unexpected SpatialKernelType." << EidosTerminate();
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
	// This uses THREAD_SAFETY_IN_ACTIVE_PARALLEL() instead of THREAD_SAFETY_IN_ANY_PARALLEL() because it does get
	// called from inactive (i.e., 1-thread) parallel regions, so that we don't have to special-case code paths
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("InteractionType::ApplyInteractionCallbacks(): running Eidos callback");
	
#if (SLIMPROFILING == 1)
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
	
#if (SLIMPROFILING == 1)
	// PROFILING
	SLIM_PROFILE_BLOCK_END(community_.profile_callback_totals_[(int)(SLiMEidosBlockType::SLiMEidosInteractionCallback)]);
#endif
	
	return p_strength;
}

size_t InteractionType::MemoryUsageForKDTrees(void)
{
	size_t usage = 0;
	
	// this may be an underestimate, since we overallocate in some cases (exerter constraints)
	for (auto &iter : data_)
	{
		const InteractionsData &data = iter.second;
		usage += sizeof(SLiM_kdNode) * data.kd_node_count_ALL_;
		usage += sizeof(SLiM_kdNode) * data.kd_node_count_EXERTERS_;
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
	THREAD_SAFETY_IN_ACTIVE_PARALLEL("InteractionType::MemoryUsageForSparseVectorPool(): s_freed_sparse_vectors_");
	
	size_t usage = 0;
	
#ifdef _OPENMP
	// When running multithreaded, count all pools
	for (auto &pool : s_freed_sparse_vectors_PERTHREAD)
	{
		usage += sizeof(std::vector<SparseVector *>);
		usage += pool.size() * sizeof(SparseVector);
		
		for (SparseVector *free_sv : pool)
			usage += free_sv->MemoryUsage();
	}
#else
	usage = s_freed_sparse_vectors_SINGLE.size() * sizeof(SparseVector);
	
	for (SparseVector *free_sv : s_freed_sparse_vectors_SINGLE)
		usage += free_sv->MemoryUsage();
#endif
	
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

inline __attribute__((always_inline)) void swap(SLiM_kdNode *p_x, SLiM_kdNode *p_y) noexcept
{
	std::swap(p_x->x, p_y->x);
	std::swap(p_x->individual_index_, p_y->individual_index_);
}

// find median for phase 0 (x)
SLiM_kdNode *InteractionType::FindMedian_p0(SLiM_kdNode *start, SLiM_kdNode *end)
{
	// BCH 12/11/2022: This used to use Quickselect, but we encounterested issues with this hitting
	// its O(n^2) worst case.  Now we use the STL std::nth_element(), which seems to do better.
	SLiM_kdNode *mid = start + (end - start) / 2;
	std::nth_element(start, mid, end, [](const SLiM_kdNode &i1, const SLiM_kdNode &i2) { return i1.x[0] < i2.x[0]; });
	return mid;
}

// find median for phase 1 (y)
SLiM_kdNode *InteractionType::FindMedian_p1(SLiM_kdNode *start, SLiM_kdNode *end)
{
	// BCH 12/11/2022: This used to use Quickselect, but we encounterested issues with this hitting
	// its O(n^2) worst case.  Now we use the STL std::nth_element(), which seems to do better.
	SLiM_kdNode *mid = start + (end - start) / 2;
	std::nth_element(start, mid, end, [](const SLiM_kdNode &i1, const SLiM_kdNode &i2) { return i1.x[1] < i2.x[1]; });
	return mid;
}

// find median for phase 2 (z)
SLiM_kdNode *InteractionType::FindMedian_p2(SLiM_kdNode *start, SLiM_kdNode *end)
{
	// BCH 12/11/2022: This used to use Quickselect, but we encounterested issues with this hitting
	// its O(n^2) worst case.  Now we use the STL std::nth_element(), which seems to do better.
	SLiM_kdNode *mid = start + (end - start) / 2;
	std::nth_element(start, mid, end, [](const SLiM_kdNode &i1, const SLiM_kdNode &i2) { return i1.x[2] < i2.x[2]; });
	return mid;
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

void InteractionType::CacheKDTreeNodes(Subpopulation *subpop, InteractionsData &p_subpop_data, bool p_apply_exerter_constraints, SLiM_kdNode **kd_nodes_ptr, SLiM_kdNode **kd_root_ptr, slim_popsize_t *kd_node_count_ptr)
{
	Individual **subpop_individuals = subpop->parent_individuals_.data();
	int individual_count = p_subpop_data.individual_count_;
	int first_individual_index, last_individual_index;
	
	// Calculate modified indices into the population, based on exerter sex-specificity.  This lets us skip over
	// individuals that are disqualified by the exerter sex-specificity constraints without even looking at them.
	if (p_apply_exerter_constraints && (exerter_constraints_.sex_ == IndividualSex::kMale))
	{
		first_individual_index = p_subpop_data.first_male_index_;
		last_individual_index = individual_count - 1;
	}
	else if (p_apply_exerter_constraints && (exerter_constraints_.sex_ == IndividualSex::kFemale))
	{
		first_individual_index = 0;
		last_individual_index = p_subpop_data.first_male_index_ - 1;
	}
	else
	{
		first_individual_index = 0;
		last_individual_index = individual_count - 1;
	}
	
	// Allocate the chosen number of nodes
	int max_node_count = last_individual_index - first_individual_index + 1;
	SLiM_kdNode *nodes = (SLiM_kdNode *)calloc(max_node_count, sizeof(SLiM_kdNode));
	if (!nodes)
		EIDOS_TERMINATION << "ERROR (InteractionType::CacheKDTreeNodes): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
	
	// Fill the nodes with their initial data; start assuming the non-periodic base case, split into spatiality cases for speed
	int actual_node_count = 0;
	
	switch (spatiality_)
	{
		case 1:
			if (p_apply_exerter_constraints && exerter_constraints_.has_nonsex_constraints_)
			{
				for (int i = first_individual_index; i <= last_individual_index; ++i)
				{
					Individual *ind = subpop_individuals[i];
					
					if (CheckIndividualNonSexConstraints(ind, exerter_constraints_))		// potentially raises
					{
						SLiM_kdNode *node = nodes + actual_node_count;
						double *position_data = p_subpop_data.positions_ + (size_t)i * SLIM_MAX_DIMENSIONALITY;
						
						node->x[0] = position_data[0];
						node->individual_index_ = i;
						actual_node_count++;
					}
				}
			}
			else
			{
				for (int i = first_individual_index; i <= last_individual_index; ++i)
				{
					SLiM_kdNode *node = nodes + actual_node_count;
					double *position_data = p_subpop_data.positions_ + (size_t)i * SLIM_MAX_DIMENSIONALITY;
					
					node->x[0] = position_data[0];
					node->individual_index_ = i;
					actual_node_count++;
				}
			}
			break;
		case 2:
			if (p_apply_exerter_constraints && exerter_constraints_.has_nonsex_constraints_)
			{
				for (int i = first_individual_index; i <= last_individual_index; ++i)
				{
					Individual *ind = subpop_individuals[i];
					
					if (CheckIndividualNonSexConstraints(ind, exerter_constraints_))		// potentially raises
					{
						SLiM_kdNode *node = nodes + actual_node_count;
						double *position_data = p_subpop_data.positions_ + (size_t)i * SLIM_MAX_DIMENSIONALITY;
						
						node->x[0] = position_data[0];
						node->x[1] = position_data[1];
						node->individual_index_ = i;
						actual_node_count++;
					}
				}
			}
			else
			{
				for (int i = first_individual_index; i <= last_individual_index; ++i)
				{
					SLiM_kdNode *node = nodes + actual_node_count;
					double *position_data = p_subpop_data.positions_ + (size_t)i * SLIM_MAX_DIMENSIONALITY;
					
					node->x[0] = position_data[0];
					node->x[1] = position_data[1];
					node->individual_index_ = i;
					actual_node_count++;
				}
			}
			break;
		case 3:
			if (p_apply_exerter_constraints && exerter_constraints_.has_nonsex_constraints_)
			{
				for (int i = first_individual_index; i <= last_individual_index; ++i)
				{
					Individual *ind = subpop_individuals[i];
					
					if (CheckIndividualNonSexConstraints(ind, exerter_constraints_))		// potentially raises
					{
						SLiM_kdNode *node = nodes + actual_node_count;
						double *position_data = p_subpop_data.positions_ + (size_t)i * SLIM_MAX_DIMENSIONALITY;
						
						node->x[0] = position_data[0];
						node->x[1] = position_data[1];
						node->x[2] = position_data[2];
						node->individual_index_ = i;
						actual_node_count++;
					}
				}
			}
			else
			{
				for (int i = first_individual_index; i <= last_individual_index; ++i)
				{
					SLiM_kdNode *node = nodes + actual_node_count;
					double *position_data = p_subpop_data.positions_ + (size_t)i * SLIM_MAX_DIMENSIONALITY;
					
					node->x[0] = position_data[0];
					node->x[1] = position_data[1];
					node->x[2] = position_data[2];
					node->individual_index_ = i;
					actual_node_count++;
				}
			}
			break;
		default:
			EIDOS_TERMINATION << "ERROR (InteractionType::CacheKDTreeNodes): (internal error) spatiality_ out of range." << EidosTerminate(nullptr);
	}
	
	// Note that replication of nodes for the periodic case is done in BuildKDTree(),
	// to save work when the k-d tree is not actually used for exerters
	
	// Write out the final constructed k-d tree to our parameters
	*kd_nodes_ptr = nodes;
	*kd_root_ptr = nullptr;
	*kd_node_count_ptr = actual_node_count;
}

void InteractionType::BuildKDTree(InteractionsData &p_subpop_data, SLiM_kdNode **kd_nodes_ptr, SLiM_kdNode **kd_root_ptr, slim_popsize_t *kd_node_count_ptr)
{
	// If we have any periodic dimensions, we need to replicate our nodes spatially
	// Note that exerter constraints have already been applied
	int periodicity_multiplier = (p_subpop_data.periodic_x_ ? 3 : 1) * (p_subpop_data.periodic_y_ ? 3 : 1) * (p_subpop_data.periodic_z_ ? 3 : 1);
	
	if (periodicity_multiplier > 1)
	{
		SLiM_kdNode *nodes = *kd_nodes_ptr;
		int actual_node_count = *kd_node_count_ptr;
		int max_node_count = actual_node_count * periodicity_multiplier;
		
		nodes = (SLiM_kdNode *)realloc(nodes, max_node_count * sizeof(SLiM_kdNode));	// NOLINT(*-realloc-usage) : realloc failure is a fatal error anyway
		if (!nodes)
			EIDOS_TERMINATION << "ERROR (InteractionType::BuildKDTree): allocation failed; you may need to raise the memory limit for SLiM." << EidosTerminate(nullptr);
		
		// We want periodicity_multiplier replicates; 3 or 9 or 27.  The central replicate is the base replicate, which we have
		// already created at replicate index 0 in the nodes buffer.  So we want to make the remaining replicates at the remaining
		// indices.  Each replicate gets offsets from the base position; to make that work easily, we calculate a modified index
		// that places the base replicate at the center of the buffer (even though it is really at position 0).
		int replicate_index_of_center = periodicity_multiplier / 2;		// rounds down to nearest integer; 3 -> 1, 9 -> 4, 27 -> 13
		
		for (int replicate = 1; replicate < periodicity_multiplier; ++replicate)
		{
			int replicate_quadrant_index = (replicate <= replicate_index_of_center) ? (replicate - 1) : replicate;
			SLiM_kdNode *replicate_nodes = nodes + (size_t)replicate * actual_node_count;
			double x_offset = 0, y_offset = 0, z_offset = 0;
			
			// Determine the correct offsets for this replicate of the individual position data;
			// maybe there is a smarter way to do this, but whatever
			int replication_dim_1 = (replicate_quadrant_index % 3) - 1;
			int replication_dim_2 = ((replicate_quadrant_index / 3) % 3) - 1;
			int replication_dim_3 = (replicate_quadrant_index / 9) - 1;
			
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
					for (int i = 0; i < actual_node_count; ++i)
					{
						SLiM_kdNode *original_node = nodes + i;
						SLiM_kdNode *replicate_node = replicate_nodes + i;
						
						replicate_node->x[0] = original_node->x[0] + x_offset;
						replicate_node->individual_index_ = original_node->individual_index_;
					}
					break;
				case 2:
					for (int i = 0; i < actual_node_count; ++i)
					{
						SLiM_kdNode *original_node = nodes + i;
						SLiM_kdNode *replicate_node = replicate_nodes + i;
						
						replicate_node->x[0] = original_node->x[0] + x_offset;
						replicate_node->x[1] = original_node->x[1] + y_offset;
						replicate_node->individual_index_ = original_node->individual_index_;
					}
					break;
				case 3:
					for (int i = 0; i < actual_node_count; ++i)
					{
						SLiM_kdNode *original_node = nodes + i;
						SLiM_kdNode *replicate_node = replicate_nodes + i;
						
						replicate_node->x[0] = original_node->x[0] + x_offset;
						replicate_node->x[1] = original_node->x[1] + y_offset;
						replicate_node->x[2] = original_node->x[2] + z_offset;
						replicate_node->individual_index_ = original_node->individual_index_;
					}
					break;
				default:
					EIDOS_TERMINATION << "ERROR (InteractionType::BuildKDTree): (internal error) spatiality_ out of range." << EidosTerminate(nullptr);
			}
		}
		
		actual_node_count *= periodicity_multiplier;
		
		// Write out the final constructed k-d tree to our parameters
		*kd_nodes_ptr = nodes;
		*kd_node_count_ptr = actual_node_count;
	}
	
	if (*kd_node_count_ptr == 0)
	{
		// Usually a root pointer of nullptr indicates that the tree hasn't been built, but it is
		// also used if the tree contains no nodes and thus has no root.
		*kd_root_ptr = nullptr;
	}
	else
	{
		// Now call out to recursively construct the tree
		switch (spatiality_)
		{
			case 1: *kd_root_ptr = MakeKDTree1_p0(*kd_nodes_ptr, *kd_node_count_ptr);	break;
			case 2: *kd_root_ptr = MakeKDTree2_p0(*kd_nodes_ptr, *kd_node_count_ptr);	break;
			case 3: *kd_root_ptr = MakeKDTree3_p0(*kd_nodes_ptr, *kd_node_count_ptr);	break;
			default:
				EIDOS_TERMINATION << "ERROR (InteractionType::BuildKDTree): (internal error) spatiality_ out of range." << EidosTerminate(nullptr);
		}
		
		// Check the tree for correctness; for now I will leave this enabled in the DEBUG case,
		// because a bug was found in the k-d tree code in 2.4.1 that would have been caught by this.
		// Eventually, when it is clear that this code is robust, this check can be disabled.
#if DEBUG
		int total_tree_count = 0;
		
		switch (spatiality_)
		{
			case 1: total_tree_count = CheckKDTree1_p0(*kd_root_ptr);	break;
			case 2: total_tree_count = CheckKDTree2_p0(*kd_root_ptr);	break;
			case 3: total_tree_count = CheckKDTree3_p0(*kd_root_ptr);	break;
			default:
				EIDOS_TERMINATION << "ERROR (InteractionType::BuildKDTree): (internal error) spatiality_ out of range." << EidosTerminate(nullptr);
		}
		
		if (total_tree_count != *kd_node_count_ptr)
			EIDOS_TERMINATION << "ERROR (InteractionType::BuildKDTree): (internal error) the k-d tree count " << total_tree_count << " does not match the allocated node count" << *kd_node_count_ptr << "." << EidosTerminate();
#endif
	}
}

SLiM_kdNode *InteractionType::EnsureKDTreePresent_ALL(Subpopulation *subpop, InteractionsData &p_subpop_data)
{
	if (!p_subpop_data.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::EnsureKDTreePresent_ALL): (internal error) the interaction has not been evaluated." << EidosTerminate();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::EnsureKDTreePresent_ALL): (internal error) a k-d tree cannot be constructed for non-spatial interactions." << EidosTerminate();
	
	if (!p_subpop_data.kd_nodes_ALL_)
		CacheKDTreeNodes(subpop, p_subpop_data, /* p_apply_exerter_constraints */ false, &p_subpop_data.kd_nodes_ALL_, &p_subpop_data.kd_root_ALL_, &p_subpop_data.kd_node_count_ALL_);
	
	if (!p_subpop_data.kd_root_ALL_ && (p_subpop_data.kd_node_count_ALL_ > 0))
		BuildKDTree(p_subpop_data, &p_subpop_data.kd_nodes_ALL_, &p_subpop_data.kd_root_ALL_, &p_subpop_data.kd_node_count_ALL_);
	
	return p_subpop_data.kd_root_ALL_;		// note that this will return nullptr if the k-d tree has zero entries!
}

SLiM_kdNode *InteractionType::EnsureKDTreePresent_EXERTERS(Subpopulation *subpop, InteractionsData &p_subpop_data)
{
	if (!p_subpop_data.evaluated_)
		EIDOS_TERMINATION << "ERROR (InteractionType::EnsureKDTreePresent_EXERTERS): (internal error) the interaction has not been evaluated." << EidosTerminate();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::EnsureKDTreePresent_EXERTERS): (internal error) a k-d tree cannot be constructed for non-spatial interactions." << EidosTerminate();
	
	if (!p_subpop_data.kd_nodes_EXERTERS_)
	{
		// If there are no exerter constraints, then the ALL tree should be the same as the EXERTERS tree; there's no reason to make both.
		// So at this point, if there are no exerter constraints, we first force the ALL tree to be constructed, and then we just leech on to it.
		if (!exerter_constraints_.has_constraints_)
		{
			EnsureKDTreePresent_ALL(subpop, p_subpop_data);
			
			p_subpop_data.kd_nodes_EXERTERS_ = p_subpop_data.kd_nodes_ALL_;
			p_subpop_data.kd_root_EXERTERS_ = p_subpop_data.kd_root_ALL_;
			p_subpop_data.kd_node_count_EXERTERS_ = p_subpop_data.kd_node_count_ALL_;
			
			return p_subpop_data.kd_root_EXERTERS_;
		}
		else
		{
			// If our flag is set that there was a constraint precondition violation earlier, then we cannot build an exerters
			// tree, and instead need to show a user-visible error.  See EvaluateSubpopulation() for discussion.
			if (p_subpop_data.kd_constraints_raise_EXERTERS_)
				EIDOS_TERMINATION << "ERROR (InteractionType::EnsureKDTreePresent_EXERTERS): a tag, tagL0, tagL1, tagL2, tagL3, or tagL4 constraint is set for exerters, but the corresponding property is undefined (has not been set) for a candidate exerter being queried." << EidosTerminate();
			
			// If there are non-sex exerter constraints, the k-d tree will be cached at evaluate() time.  This code path is
			// therefore only hit when there are no non-sex exerter constraints (but there is an exerter sex constraint).
			// Let's check that assertion, to make sure we don't have a logic error anywhere, since it is important for us
			// to cache the exerter k-d tree at evaluate() time if non-sex constraints are present.
			if (exerter_constraints_.has_nonsex_constraints_)
				EIDOS_TERMINATION << "ERROR (InteractionType::EnsureKDTreePresent_EXERTERS): (internal error) an internal error in the exerter k-d tree caching logic has occurred; please report this error." << EidosTerminate();
			
			CacheKDTreeNodes(subpop, p_subpop_data, /* p_apply_exerter_constraints */ true, &p_subpop_data.kd_nodes_EXERTERS_, &p_subpop_data.kd_root_EXERTERS_, &p_subpop_data.kd_node_count_EXERTERS_);
		}
	}
	
	if (!p_subpop_data.kd_root_EXERTERS_ && (p_subpop_data.kd_node_count_EXERTERS_ > 0))
		BuildKDTree(p_subpop_data, &p_subpop_data.kd_nodes_EXERTERS_, &p_subpop_data.kd_root_EXERTERS_, &p_subpop_data.kd_node_count_EXERTERS_);
	
	return p_subpop_data.kd_root_EXERTERS_;		// note that this will return nullptr if the k-d tree has zero entries!
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
void InteractionType::BuildSV_Presences_1(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector)
{
	double d = dist_sq1(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[0] - nd[0];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_sparse_vector->AddEntryPresence(root->individual_index_);
	
	if (dx > 0)
	{
		if (root->left)
			BuildSV_Presences_1(root->left, nd, p_focal_individual_index, p_sparse_vector);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSV_Presences_1(root->right, nd, p_focal_individual_index, p_sparse_vector);
	}
	else
	{
		if (root->right)
			BuildSV_Presences_1(root->right, nd, p_focal_individual_index, p_sparse_vector);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSV_Presences_1(root->left, nd, p_focal_individual_index, p_sparse_vector);
	}
}

// add neighbors to the sparse vector in 2D
void InteractionType::BuildSV_Presences_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase)
{
	double d = dist_sq2(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_sparse_vector->AddEntryPresence(root->individual_index_);
	
	if (++p_phase >= 2) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			BuildSV_Presences_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSV_Presences_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
	else
	{
		if (root->right)
			BuildSV_Presences_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSV_Presences_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
}

// add neighbors to the sparse vector in 3D
void InteractionType::BuildSV_Presences_3(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase)
{
	double d = dist_sq3(root, nd);
#ifndef __clang_analyzer__
	double dx = root->x[p_phase] - nd[p_phase];
#else
	double dx = 0.0;
#endif
	double dx2 = dx * dx;
	
	if ((d <= max_distance_sq_) && (root->individual_index_ != p_focal_individual_index))
		p_sparse_vector->AddEntryPresence(root->individual_index_);
	
	if (++p_phase >= 3) p_phase = 0;
	
	if (dx > 0)
	{
		if (root->left)
			BuildSV_Presences_3(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->right)
			BuildSV_Presences_3(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
	else
	{
		if (root->right)
			BuildSV_Presences_3(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		
		if (dx2 > max_distance_sq_) return;
		
		if (root->left)
			BuildSV_Presences_3(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
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

// add neighbor strengths of type "f" (SpatialKernelType::kFixed : fixed) to the sparse vector in 2D
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

// add neighbor strengths of type "l" (SpatialKernelType::kLinear : linear) to the sparse vector in 2D
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

// add neighbor strengths of type "e" (SpatialKernelType::kExponential : exponential) to the sparse vector in 2D
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

// add neighbor strengths of type "n" (SpatialKernelType::kNormal : normal/Gaussian) to the sparse vector in 2D
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

// add neighbor strengths of type "c" (SpatialKernelType::kCauchy : Cauchy) to the sparse vector in 2D
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

// add neighbor strengths of type "t" (SpatialKernelType::kStudentsT : Student's t) to the sparse vector in 2D
void InteractionType::BuildSV_Strengths_t_2(SLiM_kdNode *root, double *nd, slim_popsize_t p_focal_individual_index, SparseVector *p_sparse_vector, int p_phase)
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
		p_sparse_vector->AddEntryStrength(root->individual_index_, (sv_value_t)SpatialKernel::tdist(d, if_param1_, if_param2_, if_param3_));
	}
	
	if (++p_phase >= 2) p_phase = 0;
	if (dx > 0) {
		if (root->left)					BuildSV_Strengths_t_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		if (dx2 > max_distance_sq_)		return;
		if (root->right)				BuildSV_Strengths_t_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	} else {
		if (root->right)				BuildSV_Strengths_t_2(root->right, nd, p_focal_individual_index, p_sparse_vector, p_phase);
		if (dx2 > max_distance_sq_)		return;
		if (root->left)					BuildSV_Strengths_t_2(root->left, nd, p_focal_individual_index, p_sparse_vector, p_phase);
	}
}

bool InteractionType::_CheckIndividualNonSexConstraints(Individual *p_individual, InteractionConstraints &p_constraints)
{
	// we do not check p_constraints.has_nonsex_constraints_; this should only be called when a constraint exists
	// BEWARE: this checks for tag/tagL values being defined, as needed, and raises if they aren't
	
	if (p_constraints.tag_ != SLIM_TAG_UNSET_VALUE)
	{
		slim_usertag_t tag_value = p_individual->tag_value_;
		
		if (tag_value == SLIM_TAG_UNSET_VALUE)
			EIDOS_TERMINATION << "ERROR (InteractionType::_CheckIndividualNonSexConstraints): a tag constraint is set for the interaction type, but the tag property is undefined (has not been set) for an individual being queried." << EidosTerminate();
		
		if (p_constraints.tag_ != tag_value)
			return false;
	}
	if ((p_constraints.min_age_ != -1) && (p_constraints.min_age_ > p_individual->age_))
		return false;
	if ((p_constraints.max_age_ != -1) && (p_constraints.max_age_ < p_individual->age_))
		return false;
	if ((p_constraints.migrant_ != -1) && (p_constraints.migrant_ != p_individual->migrant_))
		return false;
	
	if (p_constraints.has_tagL_constraints_)
	{
		if (p_constraints.tagL0_ != -1)
		{
			if (!p_individual->tagL0_set_)
				EIDOS_TERMINATION << "ERROR (InteractionType::_CheckIndividualNonSexConstraints): a tagL0 constraint is set for the interaction type, but the tagL0 property is undefined (has not been set) for an individual being queried." << EidosTerminate();
			
			if (p_constraints.tagL0_ != p_individual->tagL0_value_)
				return false;
		}
		if (p_constraints.tagL1_ != -1)
		{
			if (!p_individual->tagL1_set_)
				EIDOS_TERMINATION << "ERROR (InteractionType::_CheckIndividualNonSexConstraints): a tagL1 constraint is set for the interaction type, but the tagL1 property is undefined (has not been set) for an individual being queried." << EidosTerminate();
			
			if (p_constraints.tagL1_ != p_individual->tagL1_value_)
				return false;
		}
		if (p_constraints.tagL2_ != -1)
		{
			if (!p_individual->tagL2_set_)
				EIDOS_TERMINATION << "ERROR (InteractionType::_CheckIndividualNonSexConstraints): a tagL2 constraint is set for the interaction type, but the tagL2 property is undefined (has not been set) for an individual being queried." << EidosTerminate();
			
			if (p_constraints.tagL2_ != p_individual->tagL2_value_)
				return false;
		}
		if (p_constraints.tagL3_ != -1)
		{
			if (!p_individual->tagL3_set_)
				EIDOS_TERMINATION << "ERROR (InteractionType::_CheckIndividualNonSexConstraints): a tagL3 constraint is set for the interaction type, but the tagL3 property is undefined (has not been set) for an individual being queried." << EidosTerminate();
			
			if (p_constraints.tagL3_ != p_individual->tagL3_value_)
				return false;
		}
		if (p_constraints.tagL4_ != -1)
		{
			if (!p_individual->tagL4_set_)
				EIDOS_TERMINATION << "ERROR (InteractionType::_CheckIndividualNonSexConstraints): a tagL4 constraint is set for the interaction type, but the tagL4 property is undefined (has not been set) for an individual being queried." << EidosTerminate();
			
			if (p_constraints.tagL4_ != p_individual->tagL4_value_)
				return false;
		}
	}
	
	return true;
}

bool InteractionType::_PrecheckIndividualNonSexConstraints(Individual *p_individual, InteractionConstraints &p_constraints)
{
	// This is similar to _CheckIndividualNonSexConstraints(), but it does not actually check the constraints.
	// Instead, it checks that the constraints *can* be checked, without raising.  If a tag/tagL value that is
	// needed to do the constraint check is missing, this method returns false; otherwise it returns true,
	// meaning "it is safe to check constraints".  See EvaluateSubpopulation() for discussion.
	if ((p_constraints.tag_ != SLIM_TAG_UNSET_VALUE) && (p_individual->tag_value_ == SLIM_TAG_UNSET_VALUE))
		return false;
	
	if (p_constraints.has_tagL_constraints_)
	{
		if ((p_constraints.tagL0_ != -1) && !p_individual->tagL0_set_)
				return false;
		if ((p_constraints.tagL1_ != -1) && !p_individual->tagL1_set_)
				return false;
		if ((p_constraints.tagL2_ != -1) && !p_individual->tagL2_set_)
				return false;
		if ((p_constraints.tagL3_ != -1) && !p_individual->tagL3_set_)
				return false;
		if ((p_constraints.tagL4_ != -1) && !p_individual->tagL4_set_)
				return false;
	}
	
	return true;
}

void InteractionType::FillSparseVectorForReceiverPresences(SparseVector *sv, Individual *receiver, double *receiver_position, Subpopulation *exerter_subpop, SLiM_kdNode *kd_root, __attribute__((__unused__)) bool constraints_active)
{
#if DEBUG
	// The caller should guarantee that the receiver and exerter species are compatible with the interaction
	if (constraints_active)
	{
		CheckSpeciesCompatibility_Receiver(receiver->subpopulation_->species_);
		CheckSpeciesCompatibility_Exerter(exerter_subpop->species_);
	}
	else
	{
		CheckSpeciesCompatibility_Generic(receiver->subpopulation_->species_);
		CheckSpeciesCompatibility_Generic(exerter_subpop->species_);
	}
	
	// SparseVector relies on the k-d tree, so this is an error for now
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverPresences): (internal error) request for k-d tree information from a non-spatial interaction." << EidosTerminate();
	
	// The caller should guarantee that the receiver and exerter subpops are compatible in spatial structure
	CheckSpatialCompatibility(receiver->subpopulation_, exerter_subpop);
	
	// The caller should ensure that this method is never called for a receiver that cannot receive interactions
	if (constraints_active && !CheckIndividualConstraints(receiver, receiver_constraints_))
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverPresences): (internal error) the receiver is disqualified by the current receiver constraints." << EidosTerminate();
	
	// The caller should be handing us a sparse vector set up for distance data
	if (sv->DataType() != SparseVectorDataType::kPresences)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverPresences): (internal error) the sparse vector is not configured for presences." << EidosTerminate();
	
	// The caller should guarantee that the receiver is not a new juvenile, because they need to have a saved position
	if (receiver->index_ < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverPresences): (internal error) the receiver is a new juvenile." << EidosTerminate();
#endif
	
	// if the root is nullptr, the tree is empty and we have no results
	if (kd_root)
	{
		// Figure out what index in the exerter subpopulation, if any, needs to be excluded so self-interaction is zero
		slim_popsize_t excluded_index = (exerter_subpop == receiver->subpopulation_) ? receiver->index_ : -1;
		
		// Without a specified exerter sex, we can add each exerter with no sex test
		if (spatiality_ == 2)		BuildSV_Presences_2(kd_root, receiver_position, excluded_index, sv, 0);
		else if (spatiality_ == 1)	BuildSV_Presences_1(kd_root, receiver_position, excluded_index, sv);
		else if (spatiality_ == 3)	BuildSV_Presences_3(kd_root, receiver_position, excluded_index, sv, 0);
	}
	
	// After building the sparse vector above, we mark it finished
	sv->Finished();
}

void InteractionType::FillSparseVectorForReceiverDistances(SparseVector *sv, Individual *receiver, double *receiver_position, Subpopulation *exerter_subpop, SLiM_kdNode *kd_root, __attribute__((__unused__)) bool constraints_active)
{
#if DEBUG
	// The caller should guarantee that the receiver and exerter species are compatible with the interaction
	if (constraints_active)
	{
		CheckSpeciesCompatibility_Receiver(receiver->subpopulation_->species_);
		CheckSpeciesCompatibility_Exerter(exerter_subpop->species_);
	}
	else
	{
		CheckSpeciesCompatibility_Generic(receiver->subpopulation_->species_);
		CheckSpeciesCompatibility_Generic(exerter_subpop->species_);
	}
	
	// Non-spatial interactions do not have a concept of distance, so this is an error
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverDistances): (internal error) request for distances from a non-spatial interaction." << EidosTerminate();
	
	// The caller should guarantee that the receiver and exerter subpops are compatible in spatial structure
	CheckSpatialCompatibility(receiver->subpopulation_, exerter_subpop);
	
	// The caller should ensure that this method is never called for a receiver that cannot receive interactions
	if (constraints_active && !CheckIndividualConstraints(receiver, receiver_constraints_))
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverDistances): (internal error) the receiver is disqualified by the current receiver constraints." << EidosTerminate();
	
	// The caller should be handing us a sparse vector set up for distance data
	if (sv->DataType() != SparseVectorDataType::kDistances)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverDistances): (internal error) the sparse vector is not configured for distances." << EidosTerminate();
	
	// The caller should guarantee that the receiver is not a new juvenile, because they need to have a saved position
	if (receiver->index_ < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverDistances): (internal error) the receiver is a new juvenile." << EidosTerminate();
#endif
	
	// if the root is nullptr, the tree is empty and we have no results
	if (kd_root)
	{
		// Figure out what index in the exerter subpopulation, if any, needs to be excluded so self-interaction is zero
		slim_popsize_t excluded_index = (exerter_subpop == receiver->subpopulation_) ? receiver->index_ : -1;
		
		if (spatiality_ == 2)		BuildSV_Distances_2(kd_root, receiver_position, excluded_index, sv, 0);
		else if (spatiality_ == 1)	BuildSV_Distances_1(kd_root, receiver_position, excluded_index, sv);
		else if (spatiality_ == 3)	BuildSV_Distances_3(kd_root, receiver_position, excluded_index, sv, 0);
	}
	
	// After building the sparse vector above, we mark it finished
	sv->Finished();
}

void InteractionType::FillSparseVectorForPointDistances(SparseVector *sv, double *position, __attribute__((__unused__)) Subpopulation *exerter_subpop, SLiM_kdNode *kd_root)
{
	// This is a special version of FillSparseVectorForReceiverDistances() used for nearestNeighborsOfPoint().
	// It searches for neighbors of a point, without using a receiver, just a point.
#if DEBUG
	// The caller should guarantee that the exerter species is compatible with the interaction
	CheckSpeciesCompatibility_Generic(exerter_subpop->species_);
	
	// Non-spatial interactions do not have a concept of distance, so this is an error
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForPointDistances): (internal error) request for distances from a non-spatial interaction." << EidosTerminate();
	
	// The caller should be handing us a sparse vector set up for distance data
	if (sv->DataType() != SparseVectorDataType::kDistances)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForPointDistances): (internal error) the sparse vector is not configured for distances." << EidosTerminate();
#endif
	
	// if the root is nullptr, the tree is empty and we have no results
	if (kd_root)
	{
		if (spatiality_ == 2)		BuildSV_Distances_2(kd_root, position, -1, sv, 0);
		else if (spatiality_ == 1)	BuildSV_Distances_1(kd_root, position, -1, sv);
		else if (spatiality_ == 3)	BuildSV_Distances_3(kd_root, position, -1, sv, 0);
	}
	
	// After building the sparse vector above, we mark it finished
	sv->Finished();
}

void InteractionType::FillSparseVectorForReceiverStrengths(SparseVector *sv, Individual *receiver, double *receiver_position, Subpopulation *exerter_subpop, SLiM_kdNode *kd_root, std::vector<SLiMEidosBlock*> &interaction_callbacks)
{
#if DEBUG
	// The caller should guarantee that the receiver and exerter species are compatible with the interaction
	CheckSpeciesCompatibility_Receiver(receiver->subpopulation_->species_);
	CheckSpeciesCompatibility_Exerter(exerter_subpop->species_);
	
	// Non-spatial interactions are not handled by this method (they must be handled separately by logic in the caller), so this is an error
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) request for strengths from a non-spatial interaction." << EidosTerminate();
	
	// The caller should guarantee that the receiver and exerter subpops are compatible in spatial structure
	CheckSpatialCompatibility(receiver->subpopulation_, exerter_subpop);
	
	// The caller should ensure that this method is never called for a receiver that cannot receive interactions
	if (!CheckIndividualConstraints(receiver, receiver_constraints_))
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) the receiver is disqualified by the current receiver constraints." << EidosTerminate();
	
	// The caller should be handing us a sparse vector set up for strength data
	if (sv->DataType() != SparseVectorDataType::kStrengths)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) the sparse vector is not configured for strengths." << EidosTerminate();
	
	// The caller should guarantee that the receiver is not a new juvenile, because they need to have a saved position
	if (receiver->index_ < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) the receiver is a new juvenile." << EidosTerminate();
#endif
	
	// if the root is nullptr, the tree is empty and we have no results
	if (kd_root)
	{
		// Figure out what index in the exerter subpopulation, if any, needs to be excluded so self-interaction is zero
		slim_popsize_t excluded_index = (exerter_subpop == receiver->subpopulation_) ? receiver->index_ : -1;
		
		// We special-case some builds directly to strength values here, for efficiency,
		// with no callbacks and spatiality "xy".
		if ((interaction_callbacks.size() == 0) && (spatiality_ == 2))
		{
			sv->SetDataType(SparseVectorDataType::kStrengths);
			
			switch (if_type_)
			{
				case SpatialKernelType::kFixed:			BuildSV_Strengths_f_2(kd_root, receiver_position, excluded_index, sv, 0); break;
				case SpatialKernelType::kLinear:		BuildSV_Strengths_l_2(kd_root, receiver_position, excluded_index, sv, 0); break;
				case SpatialKernelType::kExponential:	BuildSV_Strengths_e_2(kd_root, receiver_position, excluded_index, sv, 0); break;
				case SpatialKernelType::kNormal:		BuildSV_Strengths_n_2(kd_root, receiver_position, excluded_index, sv, 0); break;
				case SpatialKernelType::kCauchy:		BuildSV_Strengths_c_2(kd_root, receiver_position, excluded_index, sv, 0); break;
				case SpatialKernelType::kStudentsT:		BuildSV_Strengths_t_2(kd_root, receiver_position, excluded_index, sv, 0); break;
				default:
					EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) unoptimized SpatialKernelType value." << EidosTerminate();
			}
			
			sv->Finished();
			return;
		}
		
		// Set up to build distances first; this is an internal implementation detail, so we require the sparse vector set up for strengths above
		sv->SetDataType(SparseVectorDataType::kDistances);
		
		if (spatiality_ == 2)		BuildSV_Distances_2(kd_root, receiver_position, excluded_index, sv, 0);
		else if (spatiality_ == 1)	BuildSV_Distances_1(kd_root, receiver_position, excluded_index, sv);
		else if (spatiality_ == 3)	BuildSV_Distances_3(kd_root, receiver_position, excluded_index, sv, 0);
	}
	
	// After building the sparse vector above, we mark it finished
	sv->Finished();
	
	// Now we scan through the pre-existing sparse vector for interacting pairs,
	// and transform distances into interaction strength values calculated for each.
	uint32_t nnz, *columns;
	sv_value_t *values;
	
	sv->Distances(&nnz, &columns, &values);
	
	if (interaction_callbacks.size() == 0)
	{
		// No callbacks; strength calculations come from the interaction function only
		
		// CalculateStrengthNoCallbacks() is basically inlined here, moved outside the loop; see that function for comments
		switch (if_type_)
		{
			case SpatialKernelType::kFixed:
			{
				for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
					values[col_iter] = (sv_value_t)if_param1_;
				break;
			}
			case SpatialKernelType::kLinear:
			{
				for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
				{
					sv_value_t distance = values[col_iter];
					
					values[col_iter] = (sv_value_t)(if_param1_ * (1.0 - distance / max_distance_));
				}
				break;
			}
			case SpatialKernelType::kExponential:
			{
				for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
				{
					sv_value_t distance = values[col_iter];
					
					values[col_iter] = (sv_value_t)(if_param1_ * exp(-if_param2_ * distance));
				}
				break;
			}
			case SpatialKernelType::kNormal:
			{
				for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
				{
					sv_value_t distance = values[col_iter];
					
					values[col_iter] = (sv_value_t)(if_param1_ * exp(-(distance * distance) / n_2param2sq_));
				}
				break;
			}
			case SpatialKernelType::kCauchy:
			{
				for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
				{
					sv_value_t distance = values[col_iter];
					double temp = distance / if_param2_;
					
					values[col_iter] = (sv_value_t)(if_param1_ / (1.0 + temp * temp));
				}
				break;
			}
			case SpatialKernelType::kStudentsT:
			{
				for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
				{
					sv_value_t distance = values[col_iter];
					
					values[col_iter] = (sv_value_t)SpatialKernel::tdist(distance, if_param1_, if_param2_, if_param3_);
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
				
				EIDOS_TERMINATION << "ERROR (InteractionType::FillSparseVectorForReceiverStrengths): (internal error) unimplemented SpatialKernelType case." << EidosTerminate();
			}
		}
	}
	else
	{
		// Callbacks; strength calculations need to include callback effects
		// BEWARE: With callbacks, this method can raise arbitrarily, so the caller needs
		// to be prepared for that, particularly if they are running parallel!
		Individual **subpop_individuals = exerter_subpop->parent_individuals_.data();
		
		for (uint32_t col_iter = 0; col_iter < nnz; ++col_iter)
		{
			uint32_t col = columns[col_iter];
			sv_value_t distance = values[col_iter];
			
			values[col_iter] = (sv_value_t)CalculateStrengthWithCallbacks(distance, receiver, subpop_individuals[col], interaction_callbacks);
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
		p_result_vec.push_object_element_capcheck_NORR(p_individuals[root->individual_index_]);
	
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
		p_result_vec.push_object_element_capcheck_NORR(p_individuals[root->individual_index_]);
	
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
		p_result_vec.push_object_element_capcheck_NORR(p_individuals[root->individual_index_]);
	
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

// BCH 5/24/2023: Here used to reside FindNeighborsN_1(), FindNeighborsN_2(), and FindNeighborsN_3(),
// used for finding a particular number of neighbors (N), greater than 1 and less than all, in 1D / 2D / 3D.
// They were not thread-safe, and were replaced by FillSparseVectorForReceiverDistances_ALL_NEIGHBORS();
// now (11/2/2023) that has turned into FillSparseVectorForReceiverDistances() using kd_root_ALL_, below.

void InteractionType::FindNeighbors(Subpopulation *p_subpop, SLiM_kdNode *kd_root, slim_popsize_t kd_node_count, double *p_point, int p_count, EidosValue_Object_vector &p_result_vec, Individual *p_excluded_individual, bool constraints_active)
{
	// If this method is passed kd_root_ALL_, from EnsureKDTreePresent_ALL(), it finds all neighbors, regardless
	// of exerter constraints.  If it is passed kd_root_EXERTERS_, from EnsureKDTreePresent_EXERTERS(), it finds
	// only neighbors that satisfy the exerter constraints.
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::FindNeighbors): (internal error) neighbors cannot be found for non-spatial interactions." << EidosTerminate();
	
	// If zero neighbors are requested, or if the k-d tree root is nullptr (no nodes), return an empty result
	// BCH 11/2/2023: returning an empty result for !kd_root is a change in behavior; we used to throw an exception.
	if (!kd_root || (p_count == 0))
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
			case 1: FindNeighbors1_1(kd_root, p_point, focal_individual_index, &best, &best_dist);		break;
			case 2: FindNeighbors1_2(kd_root, p_point, focal_individual_index, &best, &best_dist, 0);	break;
			case 3: FindNeighbors1_3(kd_root, p_point, focal_individual_index, &best, &best_dist, 0);	break;
			default:
				EIDOS_TERMINATION << "ERROR (InteractionType::FindNeighbors): (internal error) spatiality_ out of range." << EidosTerminate(nullptr);
		}
		
		if (best && (best_dist <= max_distance_sq_))
		{
			Individual *best_individual = p_subpop->parent_individuals_[best->individual_index_];
			
			p_result_vec.push_object_element_NORR(best_individual);
		}
	}
	else if (p_count >= kd_node_count)	// can't do (kd_node_count - 1), because the focal individual might not be among the nodes in the k-d tree
	{
		// Finding all neighbors within the interaction distance is special-cased
		switch (spatiality_)
		{
			case 1: FindNeighborsA_1(kd_root, p_point, focal_individual_index, p_result_vec, p_subpop->parent_individuals_);		break;
			case 2: FindNeighborsA_2(kd_root, p_point, focal_individual_index, p_result_vec, p_subpop->parent_individuals_, 0);		break;
			case 3: FindNeighborsA_3(kd_root, p_point, focal_individual_index, p_result_vec, p_subpop->parent_individuals_, 0);		break;
			default:
				EIDOS_TERMINATION << "ERROR (InteractionType::FindNeighbors): (internal error) spatiality_ out of range." << EidosTerminate(nullptr);
		}
	}
	else
	{
		// Finding multiple neighbors is the slower general case; we use SparseVector to get all neighbors
		// (we would have to look at all of them anyway), and then sort them and return the top N
		// BCH 5/24/2023: This replaces the old algorithm using FindNeighborsN_X(), which was not thread-safe
		SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(p_subpop, SparseVectorDataType::kDistances);
		
		if (p_excluded_individual)
			FillSparseVectorForReceiverDistances(sv, p_excluded_individual, p_point, p_subpop, kd_root, constraints_active);
		else
			FillSparseVectorForPointDistances(sv, p_point, p_subpop, kd_root);
		
		uint32_t nnz;
		const uint32_t *columns;
		const sv_value_t *distances;
		
		distances = sv->Distances(&nnz, &columns);
		
		std::vector<std::pair<uint32_t, sv_value_t>> neighbors;
		
		for (uint32_t col_index = 0; col_index < nnz; ++col_index)
			neighbors.emplace_back(col_index, distances[col_index]);
		
		std::sort(neighbors.begin(), neighbors.end(), [](const std::pair<uint32_t, sv_value_t> &l, const std::pair<uint32_t, sv_value_t> &r) {
			return l.second < r.second;
		});
		
		std::vector<Individual *> &exerters = p_subpop->parent_individuals_;
		
		// the client requested p_count items, but we may have fewer
		if (p_count > (int)nnz)
			p_count = (int)nnz;
		
		for (int neighbor_index = 0; neighbor_index < p_count; ++neighbor_index)
		{
			Individual *exerter = exerters[columns[neighbors[neighbor_index].first]];
			
			p_result_vec.push_object_element_capcheck_NORR(exerter);
		}
		
		FreeSparseVector(sv);
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
			
			switch (receiver_constraints_.sex_)
			{
				case IndividualSex::kFemale:	sex_segregation_string += "F"; break;
				case IndividualSex::kMale:		sex_segregation_string += "M"; break;
				default:						sex_segregation_string += "*"; break;
			}
			
			switch (exerter_constraints_.sex_)
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
			if ((if_type_ == SpatialKernelType::kLinear) && (std::isinf(max_distance_) || (max_distance_ <= 0.0)))
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
		case gID_setConstraints:			return ExecuteMethod_setConstraints(p_method_id, p_arguments, p_interpreter);
		case gID_setInteractionFunction:	return ExecuteMethod_setInteractionFunction(p_method_id, p_arguments, p_interpreter);
		case gID_strength:					return ExecuteMethod_strength(p_method_id, p_arguments, p_interpreter);
		case gID_testConstraints:			return ExecuteMethod_testConstraints(p_method_id, p_arguments, p_interpreter);
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
	
	CheckSpeciesCompatibility_Generic(*species);
	
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
	bool saw_error1 = false, saw_error2 = false;
	
	if (spatiality_ == 1)
	{
		if (spatiality_string_ == "x")
		{
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_CLIPPEDINTEGRAL_1S);
#pragma omp parallel for schedule(static) default(none) shared(receivers_count, receiver_subpop_data) firstprivate(receivers_data, float_result, periodic_x) reduction(||: saw_error1) reduction(||: saw_error2) if(receivers_count >= EIDOS_OMPMIN_CLIPPEDINTEGRAL_1S) num_threads(thread_count)
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				const Individual *receiver = receivers_data[receiver_index];
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
				{
					saw_error1 = true;
					continue;
				}
				
				double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				Subpopulation *subpop = receiver->subpopulation_;
				double indA = receiver_position[0];
				double integral;
				
#ifdef _OPENMP
				try {
					integral = ClippedIntegral_1D(indA - subpop->bounds_x0_, subpop->bounds_x1_ - indA, periodic_x);
				} catch (...) {
					saw_error2 = true;
					continue;
				}
#else
				integral = ClippedIntegral_1D(indA - subpop->bounds_x0_, subpop->bounds_x1_ - indA, periodic_x);
#endif
				
				float_result->set_float_no_check(integral, receiver_index);
			}
		}
		else if (spatiality_string_ == "y")
		{
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_CLIPPEDINTEGRAL_1S);
#pragma omp parallel for schedule(static) default(none) shared(receivers_count, receiver_subpop_data) firstprivate(receivers_data, float_result, periodic_y) reduction(||: saw_error1) reduction(||: saw_error2) if(receivers_count >= EIDOS_OMPMIN_CLIPPEDINTEGRAL_1S) num_threads(thread_count)
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				const Individual *receiver = receivers_data[receiver_index];
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
				{
					saw_error1 = true;
					continue;
				}
				
				double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				Subpopulation *subpop = receiver->subpopulation_;
				double indA = receiver_position[0];
				double integral;
				
#ifdef _OPENMP
				try {
					integral = ClippedIntegral_1D(indA - subpop->bounds_y0_, subpop->bounds_y1_ - indA, periodic_y);
				} catch (...) {
					saw_error2 = true;
					continue;
				}
#else
				integral = ClippedIntegral_1D(indA - subpop->bounds_y0_, subpop->bounds_y1_ - indA, periodic_y);
#endif
				
				float_result->set_float_no_check(integral, receiver_index);
			}
		}
		else // (spatiality_string_ == "z")
		{
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_CLIPPEDINTEGRAL_1S);
#pragma omp parallel for schedule(static) default(none) shared(receivers_count, receiver_subpop_data) firstprivate(receivers_data, float_result, periodic_z) reduction(||: saw_error1) reduction(||: saw_error2) if(receivers_count >= EIDOS_OMPMIN_CLIPPEDINTEGRAL_1S) num_threads(thread_count)
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				const Individual *receiver = receivers_data[receiver_index];
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
				{
					saw_error1 = true;
					continue;
				}
				
				double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				Subpopulation *subpop = receiver->subpopulation_;
				double indA = receiver_position[0];
				double integral;
				
#ifdef _OPENMP
				try {
					integral = ClippedIntegral_1D(indA - subpop->bounds_z0_, subpop->bounds_z1_ - indA, periodic_z);
				} catch (...) {
					saw_error2 = true;
					continue;
				}
#else
				integral = ClippedIntegral_1D(indA - subpop->bounds_z0_, subpop->bounds_z1_ - indA, periodic_z);
#endif
				
				float_result->set_float_no_check(integral, receiver_index);
			}
		}
	}
	else if (spatiality_ == 2)
	{
		if (spatiality_string_ == "xy")
		{
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_CLIPPEDINTEGRAL_2S);
#pragma omp parallel for schedule(static) default(none) shared(receivers_count, receiver_subpop_data) firstprivate(receivers_data, float_result, periodic_x, periodic_y) reduction(||: saw_error1) reduction(||: saw_error2) if(receivers_count >= EIDOS_OMPMIN_CLIPPEDINTEGRAL_2S) num_threads(thread_count)
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				const Individual *receiver = receivers_data[receiver_index];
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
				{
					saw_error1 = true;
					continue;
				}
				
				double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				Subpopulation *subpop = receiver->subpopulation_;
				double indA = receiver_position[0];
				double indB = receiver_position[1];
				double integral;
				
#ifdef _OPENMP
				try {
					integral = ClippedIntegral_2D(indA - subpop->bounds_x0_, subpop->bounds_x1_ - indA, indB - subpop->bounds_y0_, subpop->bounds_y1_ - indB, periodic_x, periodic_y);
				} catch (...) {
					saw_error2 = true;
					continue;
				}
#else
				integral = ClippedIntegral_2D(indA - subpop->bounds_x0_, subpop->bounds_x1_ - indA, indB - subpop->bounds_y0_, subpop->bounds_y1_ - indB, periodic_x, periodic_y);
#endif
				
				float_result->set_float_no_check(integral, receiver_index);
			}
		}
		else if (spatiality_string_ == "xz")
		{
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_CLIPPEDINTEGRAL_2S);
#pragma omp parallel for schedule(static) default(none) shared(receivers_count, receiver_subpop_data) firstprivate(receivers_data, float_result, periodic_x, periodic_z) reduction(||: saw_error1) reduction(||: saw_error2) if(receivers_count >= EIDOS_OMPMIN_CLIPPEDINTEGRAL_2S) num_threads(thread_count)
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				const Individual *receiver = receivers_data[receiver_index];
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
				{
					saw_error1 = true;
					continue;
				}
				
				double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				Subpopulation *subpop = receiver->subpopulation_;
				double indA = receiver_position[0];
				double indB = receiver_position[1];
				double integral;
				
#ifdef _OPENMP
				try {
					integral = ClippedIntegral_2D(indA - subpop->bounds_x0_, subpop->bounds_x1_ - indA, indB - subpop->bounds_z0_, subpop->bounds_z1_ - indB, periodic_x, periodic_z);
				} catch (...) {
					saw_error2 = true;
					continue;
				}
#else
				integral = ClippedIntegral_2D(indA - subpop->bounds_x0_, subpop->bounds_x1_ - indA, indB - subpop->bounds_z0_, subpop->bounds_z1_ - indB, periodic_x, periodic_z);
#endif
				
				float_result->set_float_no_check(integral, receiver_index);
			}
		}
		else // (spatiality_string_ == "yz")
		{
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_CLIPPEDINTEGRAL_2S);
#pragma omp parallel for schedule(static) default(none) shared(receivers_count, receiver_subpop_data) firstprivate(receivers_data, float_result, periodic_y, periodic_z) reduction(||: saw_error1) reduction(||: saw_error2) if(receivers_count >= EIDOS_OMPMIN_CLIPPEDINTEGRAL_2S) num_threads(thread_count)
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				const Individual *receiver = receivers_data[receiver_index];
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
				{
					saw_error1 = true;
					continue;
				}
				
				double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				Subpopulation *subpop = receiver->subpopulation_;
				double indA = receiver_position[0];
				double indB = receiver_position[1];
				double integral;
				
#ifdef _OPENMP
				try {
					integral = ClippedIntegral_2D(indA - subpop->bounds_y0_, subpop->bounds_y1_ - indA, indB - subpop->bounds_z0_, subpop->bounds_z1_ - indB, periodic_y, periodic_z);
				} catch (...) {
					saw_error2 = true;
					continue;
				}
#else
				integral = ClippedIntegral_2D(indA - subpop->bounds_y0_, subpop->bounds_y1_ - indA, indB - subpop->bounds_z0_, subpop->bounds_z1_ - indB, periodic_y, periodic_z);
#endif
				
				float_result->set_float_no_check(integral, receiver_index);
			}
		}
	}
	else // (spatiality_ == 3)
	{
		// FIXME
	}
	
	// deferred raises, for OpenMP compatibility
	if (saw_error1)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_clippedIntegral): clippedIntegral() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
	if (saw_error2)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_clippedIntegral): an exception was caught inside a parallel region." << EidosTerminate();
	
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
	
	CheckSpeciesCompatibility_Generic(receiver_species);
	
	InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
	double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
	
	// figure out the exerter subpopulation and get info on it
	bool exerters_value_NULL = (exerters_value->Type() == EidosValueType::kValueNULL);
	int exerters_count = exerters_value->Count();
	
	if ((exerters_count == 0) && !exerters_value_NULL)
		return gStaticEidosValue_Float_ZeroVec;
	
	Subpopulation *exerter_subpop = (exerters_value_NULL ? receiver_subpop : ((Individual *)exerters_value->ObjectElementAtIndex(0, nullptr))->subpopulation_);
	
	CheckSpeciesCompatibility_Generic(exerter_subpop->species_);
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
				double distance = CalculateDistanceWithPeriodicity(receiver_position, exerter_position_data + (size_t)exerter_index * SLIM_MAX_DIMENSIONALITY, exerter_subpop_data);
				
				result_vec->set_float_no_check(distance, exerter_index);
			}
		}
		else
		{
			for (int exerter_index = 0; exerter_index < exerter_subpop_size; ++exerter_index)
			{
				double distance = CalculateDistance(receiver_position, exerter_position_data + (size_t)exerter_index * SLIM_MAX_DIMENSIONALITY);
				
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
				distance = CalculateDistanceWithPeriodicity(receiver_position, exerter_position_data + (size_t)exerter_index_in_subpop * SLIM_MAX_DIMENSIONALITY, exerter_subpop_data);
			else
				distance = CalculateDistance(receiver_position, exerter_position_data + (size_t)exerter_index_in_subpop * SLIM_MAX_DIMENSIONALITY);
			
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
	
	CheckSpeciesCompatibility_Generic(exerter_species);
	
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
			
			double *ind_position = exerter_position_data + (size_t)exerter_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			
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
			
			double *ind_position = exerter_position_data + (size_t)exerter_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			
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
	gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
	
	if (weight_total > 0.0)
	{
		if (draw_count > 50)		// the empirically determined crossover point in performance
		{
			// Use gsl_ran_discrete() to do the drawing
			gsl_ran_discrete_t *gsl_lookup = gsl_ran_discrete_preproc(n_weights, weights);
			
			for (int64_t draw_index = 0; draw_index < draw_count; ++draw_index)
			{
				int hit_index = (int)gsl_ran_discrete(rng, gsl_lookup);
				
				draw_indices.emplace_back(hit_index);
			}
			
			gsl_ran_discrete_free(gsl_lookup);
		}
		else
		{
			// Use linear search to do the drawing
			for (int64_t draw_index = 0; draw_index < draw_count; ++draw_index)
			{
				double the_rose_in_the_teeth = Eidos_rng_uniform(rng) * weight_total;
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

//	*********************	– (object)drawByStrength(object<Individual> receiver, [integer$ count = 1], [No<Subpopulation>$ exerterSubpop = NULL], [logical$ returnDict = F])
//
EidosValue_SP InteractionType::ExecuteMethod_drawByStrength(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *receiver_value = p_arguments[0].get();
	EidosValue *count_value = p_arguments[1].get();
	EidosValue *exerterSubpop_value = p_arguments[2].get();
	EidosValue *returnDict_value = p_arguments[3].get();
	
	eidos_logical_t returnDict = returnDict_value->LogicalAtIndex(0, nullptr);
	Subpopulation *receiver_subpop = nullptr;
	
	if (!returnDict)
	{
		// This is the single-threaded, single-receiver case; it returns a vector of Individual objects
		if (receiver_value->Count() != 1)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_drawByStrength): drawByStrength() requires that the receiver is singleton when returnDict is F; if you want to process multiple receivers in a single call, pass returnDict=T." << EidosTerminate();
		
		Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
		
		receiver_subpop = receiver->subpopulation_;
	}
	else
	{
		// This is the multi-threaded, multi-receiver case; it returns a Dictionary object vectors of Individual objects
		if (receiver_value->Count() == 0)
		{
			// With no receivers, return an empty Dictionary
			EidosDictionaryRetained *dictionary = new EidosDictionaryRetained();
			EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(dictionary, gEidosDictionaryRetained_Class));
			
			dictionary->ContentsChanged("InteractionType::ExecuteMethod_drawByStrength()");
			
			// objectElement is now retained by result_SP, so we can release it
			dictionary->Release();
			
			return result_SP;
		}
		
		receiver_subpop = ((Individual *)receiver_value->ObjectElementAtIndex(0, nullptr))->subpopulation_;
	}
	
	// shared logic for both cases
	Species &receiver_species = receiver_subpop->species_;
	
	CheckSpeciesCompatibility_Receiver(receiver_species);
	
	// the exerter subpopulation defaults to the same subpop as the receivers
	Subpopulation *exerter_subpop = ((exerterSubpop_value->Type() == EidosValueType::kValueNULL) ? receiver_subpop : (Subpopulation *)exerterSubpop_value->ObjectElementAtIndex(0, nullptr));
	
	CheckSpeciesCompatibility_Exerter(exerter_subpop->species_);
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	slim_popsize_t exerter_subpop_size = exerter_subpop->parent_subpop_size_;
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	
	// Check the count; note that we do NOT clamp count to exerter_subpop_size, since draws are done with replacement!
	int64_t count = count_value->IntAtIndex(0, nullptr);
	
	if (count < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_drawByStrength): drawByStrength() requires count >= 0." << EidosTerminate();
	
	bool has_interaction_callbacks = (exerter_subpop_data.evaluation_interaction_callbacks_.size() != 0);
	bool optimize_fixed_interaction_strengths = (!has_interaction_callbacks && (if_type_ == SpatialKernelType::kFixed));
	
	if (!returnDict)
	{
		// This is the single-threaded, single-receiver case; it returns a vector of Individual objects
		if (count == 0)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		
		// receiver_value is guaranteed to be singleton; let's get the info on it
		Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
		slim_popsize_t receiver_index_in_subpop = receiver->index_;
		
		if (receiver_index_in_subpop < 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_drawByStrength): drawByStrength() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
		
		// Check constraints for the receiver; if the individual is disqualified, no draws can occur and the return is empty
		if (!CheckIndividualConstraints(receiver, receiver_constraints_))		// potentially raises
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		
		if (spatiality_ == 0)
		{
			// Non-spatial case; no distances used.  We have to worry about exerter constraints; they are not handled for us.
			// BCH 5/14/2023: We call ApplyInteractionCallbacks() below, so if this code ever goes parallel it
			// should stay single-threaded if/when any interaction() callbacks are present!
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
				
				if ((exerter_index_in_subpop != receiver_index) && CheckIndividualConstraints(exerter, exerter_constraints_))		// potentially raises
					strength = ApplyInteractionCallbacks(receiver, exerter, if_param1_, NAN, callbacks);	// hard-coding interaction function "f" (SpatialKernelType::kFixed), which is required
				
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
			double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			SLiM_kdNode *kd_root_EXERTERS = EnsureKDTreePresent_EXERTERS(exerter_subpop, exerter_subpop_data);
			EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
			SparseVector *sv = nullptr;
			
			// If there are no exerters satisfying constraints, short-circuit
			if (!kd_root_EXERTERS)
				return EidosValue_SP(result_vec);
			
			if (optimize_fixed_interaction_strengths)
			{
				// Optimized case: fixed interaction strength, no callbacks, so we can do uniform draws using presences only
				sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kPresences);
				
				try {
					FillSparseVectorForReceiverPresences(sv, receiver, receiver_position, exerter_subpop, kd_root_EXERTERS, /* constraints_active */ true);
					uint32_t nnz;
					const uint32_t *columns;
					
					sv->Presences(&nnz, &columns);
					
					if (nnz > 0)
					{
						std::vector<Individual *> &exerters = exerter_subpop->parent_individuals_;
						gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
						
						result_vec->resize_no_initialize(count);
						
						for (int64_t result_index = 0; result_index < count; ++result_index)
						{
							int presence_index = Eidos_rng_uniform_int(rng, nnz);	// equal probability for each exerter
							uint32_t exerter_index = columns[presence_index];
							Individual *chosen_individual = exerters[exerter_index];
							
							result_vec->set_object_element_no_check_NORR(chosen_individual, result_index);
						}
					}
				} catch (...) {
					InteractionType::FreeSparseVector(sv);
					throw;
				}
			}
			else
			{
				// General case, getting strengths and doing weighted draws
				// BCH 5/14/2023: The call to FillSparseVectorForReceiverStrengths() means we run interaction() callbacks,
				// so if this code is ever parallelized, it should stay single-threaded when callbacks are enabled.
				sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kStrengths);
				
				try {
					FillSparseVectorForReceiverStrengths(sv, receiver, receiver_position, exerter_subpop, kd_root_EXERTERS, exerter_subpop_data.evaluation_interaction_callbacks_);
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
				} catch (...) {
					InteractionType::FreeSparseVector(sv);
					throw;
				}
			}
			
			InteractionType::FreeSparseVector(sv);
			return EidosValue_SP(result_vec);
		}
	}
	else
	{
		// This is the multi-threaded, multi-receiver case; it returns a Dictionary object vectors of Individual objects
		// We start by making a Dictionary with an empty Individual vector for each receiver
		if (spatiality_ == 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_drawByStrength): drawByStrength() supports returning a Dictionary of results, with returnDict=T, only in the spatial case." << EidosTerminate();
		
		EidosDictionaryRetained *dictionary = new EidosDictionaryRetained();
		EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(dictionary, gEidosDictionaryRetained_Class));
		int receivers_count = receiver_value->Count();
		EidosValue_Object_vector **result_vectors = (EidosValue_Object_vector **)malloc(receivers_count * sizeof(EidosValue_Object_vector *));
		
		for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
		{
			EidosValue_Object_vector *empty_individuals_vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class);
			
			dictionary->SetKeyValue_IntegerKeys(receiver_index, EidosValue_SP(empty_individuals_vec));
			result_vectors[receiver_index] = empty_individuals_vec;
		}
		
		dictionary->ContentsChanged("InteractionType::ExecuteMethod_drawByStrength()");
		
		// objectElement is now retained by result_SP, so we can release it
		dictionary->Release();
		
		if ((count > 0) && (exerter_subpop_size > 0))	// BCH 5/24/2023: if the exerter subpop is empty, no individuals are drawn; short-circuit
		{
			SLiM_kdNode *kd_root_EXERTERS = EnsureKDTreePresent_EXERTERS(exerter_subpop, exerter_subpop_data);
			
			// If there are no exerters satisfying constraints, short-circuit
			if (!kd_root_EXERTERS)
			{
				free(result_vectors);
				return result_SP;
			}
			
			bool saw_error_1 = false, saw_error_2 = false, saw_error_3 = false, saw_error_4 = false;
			InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_DRAWBYSTRENGTH);
#pragma omp parallel for schedule(dynamic, 16) default(none) shared(gEidos_RNG_PERTHREAD, receivers_count, receiver_subpop, exerter_subpop, receiver_subpop_data, exerter_subpop_data, kd_root_EXERTERS, optimize_fixed_interaction_strengths) firstprivate(receiver_value, result_vectors, count, exerter_subpop_size) reduction(||: saw_error_1) reduction(||: saw_error_2) reduction(||: saw_error_3) reduction(||: saw_error_4) if(!has_interaction_callbacks && (receivers_count >= EIDOS_OMPMIN_DRAWBYSTRENGTH)) num_threads(thread_count)
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(receiver_index, nullptr);
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
				{
					saw_error_1 = true;
					continue;
				}
				
				// SPECIES CONSISTENCY CHECK
				if (receiver_subpop != receiver->subpopulation_)
				{
					saw_error_2 = true;
					continue;
				}
				
				// Check constraints for the receiver; if the individual is disqualified, there are no candidates to draw from
				try {
					if (!CheckIndividualConstraints(receiver, receiver_constraints_))		// potentially raises; protected
						continue;
				} catch (...) {
					saw_error_4 = true;
					continue;
				}
				
				double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				
				EidosValue_Object_vector *result_vec = result_vectors[receiver_index];
				SparseVector *sv = nullptr;
				
				if (optimize_fixed_interaction_strengths)
				{
					// Optimized case: fixed interaction strength, no callbacks, so we can do uniform draws using presences only
					sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kPresences);
					FillSparseVectorForReceiverPresences(sv, receiver, receiver_position, exerter_subpop, kd_root_EXERTERS, /* constraints_active */ true);
					uint32_t nnz;
					const uint32_t *columns;
					
					sv->Presences(&nnz, &columns);
					
					if (nnz > 0)
					{
						std::vector<Individual *> &exerters = exerter_subpop->parent_individuals_;
						gsl_rng *rng = EIDOS_GSL_RNG(omp_get_thread_num());
						
						result_vec->resize_no_initialize(count);
						
						for (int64_t result_index = 0; result_index < count; ++result_index)
						{
							int presence_index = Eidos_rng_uniform_int(rng, nnz);	// equal probability for each exerter
							uint32_t exerter_index = columns[presence_index];
							Individual *chosen_individual = exerters[exerter_index];
							
							result_vec->set_object_element_no_check_NORR(chosen_individual, result_index);
						}
					}
				}
				else
				{
					// General case, getting strengths and doing weighted draws
					sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kStrengths);
					
					// Under OpenMP, raises can't go past the end of the parallel region; handle things the same way when not under OpenMP for simplicity
					try {
						FillSparseVectorForReceiverStrengths(sv, receiver, receiver_position, exerter_subpop, kd_root_EXERTERS, exerter_subpop_data.evaluation_interaction_callbacks_);		// protected from running interaction() callbacks in parallel, above
					} catch (...) {
						saw_error_3 = true;
						InteractionType::FreeSparseVector(sv);
						continue;
					}
					
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
				}
				
				InteractionType::FreeSparseVector(sv);
			}
			
			// deferred raises, for OpenMP compatibility
			if (saw_error_1)
			{
				free(result_vectors);
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_drawByStrength): drawByStrength() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
			}
			if (saw_error_2)
			{
				free(result_vectors);
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_drawByStrength): drawByStrength() requires that all receivers be in the same subpopulation." << EidosTerminate();
			}
			if (saw_error_3)
			{
				free(result_vectors);
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_drawByStrength): an exception was caught inside a parallel region." << EidosTerminate();
			}
			if (saw_error_4)
			{
				free(result_vectors);
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_drawByStrength): drawByStrength() tested a tag or tagL constraint, but a receiver's value for that property was not defined (had not been set)." << EidosTerminate();
			}
		}
		
		free(result_vectors);
		return result_SP;
	}
}

//	*********************	- (void)evaluate(io<Subpopulation> subpops)
//
EidosValue_SP InteractionType::ExecuteMethod_evaluate(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *subpops_value = p_arguments[0].get();
	
	// TIMING RESTRICTION
	if ((community_.CycleStage() == SLiMCycleStage::kWFStage2GenerateOffspring) ||
		(community_.CycleStage() == SLiMCycleStage::kNonWFStage1GenerateOffspring) ||
		(community_.CycleStage() == SLiMCycleStage::kNonWFStage4SurvivalSelection))
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_evaluate): evaluate() may not be called during the offspring generation or viability/survival cycle stages." << EidosTerminate();
	
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
	// BCH 11/2/2023: Note that this method is now almost identical to ExecuteMethod_neighborCount()
	EidosValue *receivers_value = p_arguments[0].get();
	EidosValue *exerterSubpop_value = p_arguments[1].get();
	int receivers_count = receivers_value->Count();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires that the interaction be spatial." << EidosTerminate();
	
	if (receivers_count == 0)
		return gStaticEidosValue_Integer_ZeroVec;
	
	// the exerter subpopulation defaults to the same subpop as the receivers
	Subpopulation *receiver_subpop = ((Individual *)receivers_value->ObjectElementAtIndex(0, nullptr))->subpopulation_;
	Subpopulation *exerter_subpop = ((exerterSubpop_value->Type() == EidosValueType::kValueNULL) ? receiver_subpop : (Subpopulation *)exerterSubpop_value->ObjectElementAtIndex(0, nullptr));
	
	CheckSpeciesCompatibility_Receiver(receiver_subpop->species_);
	CheckSpeciesCompatibility_Exerter(exerter_subpop->species_);
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	SLiM_kdNode *kd_root_EXERTERS = EnsureKDTreePresent_EXERTERS(exerter_subpop, exerter_subpop_data);
	
	// If there are no exerters satisfying constraints, short-circuit
	if (!kd_root_EXERTERS)
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
	
	if (receivers_count == 1)
	{
		// Just one value, so we can return a singleton and skip some work
		Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(0, nullptr);
		slim_popsize_t receiver_index_in_subpop = receiver->index_;
		
		if (receiver_index_in_subpop < 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
		
		// Check constraints for the receiver; if the individual is disqualified, the count is zero
		if (!CheckIndividualConstraints(receiver, receiver_constraints_))		// potentially raises
			return gStaticEidosValue_Integer0;
		
		// Find the neighbors
		double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
		slim_popsize_t focal_individual_index = (exerter_subpop == receiver_subpop) ? receiver_index_in_subpop : -1;
		int neighborCount;
		
		switch (spatiality_)
		{
			case 1: neighborCount = CountNeighbors_1(kd_root_EXERTERS, receiver_position, focal_individual_index);			break;
			case 2: neighborCount = CountNeighbors_2(kd_root_EXERTERS, receiver_position, focal_individual_index, 0);		break;
			case 3: neighborCount = CountNeighbors_3(kd_root_EXERTERS, receiver_position, focal_individual_index, 0);		break;
			default: neighborCount = 0; break;	// unsupported value
		}
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(neighborCount));
	}
	else
	{
		EidosValue_Int_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(receivers_count);
		bool saw_error_1 = false, saw_error_2 = false, saw_error_3 = false;
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_INTNEIGHCOUNT);
#pragma omp parallel for schedule(dynamic, 16) default(none) shared(receivers_count, receiver_subpop, exerter_subpop, receiver_subpop_data, exerter_subpop_data, kd_root_EXERTERS) firstprivate(receivers_value, result_vec) reduction(||: saw_error_1) reduction(||: saw_error_2) reduction(||: saw_error_3) if(receivers_count >= EIDOS_OMPMIN_INTNEIGHCOUNT) num_threads(thread_count)
		for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
		{
			Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(receiver_index, nullptr);
			slim_popsize_t receiver_index_in_subpop = receiver->index_;
			
			if (receiver_index_in_subpop < 0)
			{
				saw_error_1 = true;
				continue;
			}
			
			// SPECIES CONSISTENCY CHECK
			if (receiver_subpop != receiver->subpopulation_)
			{
				saw_error_2 = true;
				continue;
			}
			
			// Check constraints for the receiver; if the individual is disqualified, the count is zero
			// Under OpenMP, raises can't go past the end of the parallel region; handle things the same way when not under OpenMP for simplicity
			try {
				if (!CheckIndividualConstraints(receiver, receiver_constraints_))		// potentially raises; protected
				{
					result_vec->set_int_no_check(0, receiver_index);
					continue;
				}
			} catch (...) {
				saw_error_3 = true;
				continue;
			}
			
			// Find the neighbors
			double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			slim_popsize_t focal_individual_index = (exerter_subpop == receiver_subpop) ? receiver_index_in_subpop : -1;
			int neighborCount;
			
			switch (spatiality_)
			{
				case 1: neighborCount = CountNeighbors_1(kd_root_EXERTERS, receiver_position, focal_individual_index);			break;
				case 2: neighborCount = CountNeighbors_2(kd_root_EXERTERS, receiver_position, focal_individual_index, 0);		break;
				case 3: neighborCount = CountNeighbors_3(kd_root_EXERTERS, receiver_position, focal_individual_index, 0);		break;
				default: neighborCount = 0; break;	// unsupported value
			}
			
			result_vec->set_int_no_check(neighborCount, receiver_index);
		}
		
		// deferred raises, for OpenMP compatibility
		if (saw_error_1)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
		if (saw_error_2)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() requires that all receivers be in the same subpopulation." << EidosTerminate();
		if (saw_error_3)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_interactingNeighborCount): interactingNeighborCount() tested a tag or tagL constraint, but a receiver's value for that property was not defined (had not been set)." << EidosTerminate();
		
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
	
	CheckSpeciesCompatibility_Receiver(receiver_subpop->species_);
	CheckSpeciesCompatibility_Exerter(exerter_subpop->species_);
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	if (receiver_subpop != exerter_subpop)
		if ((receiver_subpop->bounds_x0_ != exerter_subpop->bounds_x0_) || (receiver_subpop->bounds_x1_ != exerter_subpop->bounds_x1_) ||
			(receiver_subpop->bounds_y0_ != exerter_subpop->bounds_y0_) || (receiver_subpop->bounds_y1_ != exerter_subpop->bounds_y1_) ||
			(receiver_subpop->bounds_z0_ != exerter_subpop->bounds_z0_) || (receiver_subpop->bounds_z1_ != exerter_subpop->bounds_z1_))
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_localPopulationDensity): localPopulationDensity() requires that the receiver and exerter subpopulations have identical bounds." << EidosTerminate();
	
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	SLiM_kdNode *kd_root_EXERTERS = EnsureKDTreePresent_EXERTERS(exerter_subpop, exerter_subpop_data);
	
	// If there are no exerters satisfying constraints, short-circuit
	if (!kd_root_EXERTERS)
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
	
	double strength_for_zero_distance = CalculateStrengthNoCallbacks(0.0);	// probably always if_param1_, but let's not hard-code that...
	bool has_interaction_callbacks = (exerter_subpop_data.evaluation_interaction_callbacks_.size() != 0);
	
	if (has_interaction_callbacks)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_localPopulationDensity): localPopulationDensity() does not allow interaction() callbacks, since they cannot be integrated to compute density." << EidosTerminate();
	
	// Subcontract to ExecuteMethod_clippedIntegral(); this handles all the spatiality crap for us
	// note that we pass our own parameters through to clippedIntegral()!  So our APIs need to be the same!
	// Actually, we now have an extra parameter, exerterSubpop(), compared to clippedIntegral(); but we
	// do not need it to use that parameter (because we require identical bounds above), so it's OK.
	EidosValue_SP clipped_integrals_SP = ExecuteMethod_clippedIntegral(p_method_id, p_arguments, p_interpreter);
	EidosValue *clipped_integrals = clipped_integrals_SP.get();
	
	// Decide whether we can use our optimized case below
	bool optimize_fixed_interaction_strengths = (!has_interaction_callbacks && (if_type_ == SpatialKernelType::kFixed));
	
	if (receivers_count == 1)
	{
		// Just one value, so we can return a singleton and skip some work
		slim_popsize_t receiver_index_in_subpop = first_receiver->index_;
		
		if (receiver_index_in_subpop < 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_localPopulationDensity): localPopulationDensity() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
		
		// Check constraints for the receiver; if the individual is disqualified, the local density of interacters is zero
		if (!CheckIndividualConstraints(first_receiver, receiver_constraints_))		// potentially raises
			return gStaticEidosValue_Float0;
		
		double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
		double total_strength;
		SparseVector *sv;
		
		if (optimize_fixed_interaction_strengths)
		{
			// Optimized case for fixed interaction strength and no callbacks
			sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kPresences);
			FillSparseVectorForReceiverPresences(sv, first_receiver, receiver_position, exerter_subpop, kd_root_EXERTERS, /* constraints_active */ true);
			
			uint32_t nnz;
			sv->Presences(&nnz);
			
			total_strength = nnz * if_param1_;
		}
		else
		{
			// General case, totalling strengths
			sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kStrengths);
			FillSparseVectorForReceiverStrengths(sv, first_receiver, receiver_position, exerter_subpop, kd_root_EXERTERS, exerter_subpop_data.evaluation_interaction_callbacks_);		// singleton case, not parallel
			
			// Get the sparse vector data
			uint32_t nnz;
			const sv_value_t *strengths;
			
			strengths = sv->Strengths(&nnz);
			
			// Total the interaction strengths
			total_strength = 0.0;
			
			for (uint32_t col_index = 0; col_index < nnz; ++col_index)
				total_strength += strengths[col_index];
		}
		
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
		bool saw_error_1 = false, saw_error_2 = false, saw_error_3 = false;
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_LOCALPOPDENSITY);
#pragma omp parallel for schedule(dynamic, 16) default(none) shared(receivers_count, receiver_subpop, exerter_subpop, receiver_subpop_data, exerter_subpop_data, kd_root_EXERTERS, strength_for_zero_distance, clipped_integrals, optimize_fixed_interaction_strengths) firstprivate(receivers_value, result_vec) reduction(||: saw_error_1) reduction(||: saw_error_2) reduction(||: saw_error_3) if(!has_interaction_callbacks && (receivers_count >= EIDOS_OMPMIN_LOCALPOPDENSITY)) num_threads(thread_count)
		for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
		{
			Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(receiver_index, nullptr);
			slim_popsize_t receiver_index_in_subpop = receiver->index_;
			
			if (receiver_index_in_subpop < 0)
			{
				saw_error_1 = true;
				continue;
			}
			
			// SPECIES CONSISTENCY CHECK
			if (receiver_subpop != receiver->subpopulation_)
			{
				saw_error_2 = true;
				continue;
			}
			
			// Check constraints for the receiver; if the individual is disqualified, the local density of interacters is zero
			// Under OpenMP, raises can't go past the end of the parallel region; handle things the same way when not under OpenMP for simplicity
			try {
				if (!CheckIndividualConstraints(receiver, receiver_constraints_))		// potentially raises; protected
				{
					result_vec->set_float_no_check(0, receiver_index);
					continue;
				}
			} catch (...) {
				saw_error_3 = true;
				continue;
			}
			
			double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			double total_strength;
			SparseVector *sv;
			
			if (optimize_fixed_interaction_strengths)
			{
				// Optimized case for fixed interaction strength and no callbacks
				sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kPresences);
				FillSparseVectorForReceiverPresences(sv, receiver, receiver_position, exerter_subpop, kd_root_EXERTERS, /* constraints_active */ true);
				
				uint32_t nnz;
				sv->Presences(&nnz);
				total_strength = nnz * if_param1_;
			}
			else
			{
				// General case, totalling strengths
				sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kStrengths);
				FillSparseVectorForReceiverStrengths(sv, receiver, receiver_position, exerter_subpop, kd_root_EXERTERS, exerter_subpop_data.evaluation_interaction_callbacks_);		// we do not allow interaction() callbacks, so this should not raise
				
				// Get the sparse vector data
				uint32_t nnz;
				const sv_value_t *strengths;
				
				strengths = sv->Strengths(&nnz);
				
				// Total the interaction strengths
				total_strength = 0.0;
				
				for (uint32_t col_index = 0; col_index < nnz; ++col_index)
					total_strength += strengths[col_index];
			}
			
			// Add the interaction strength for the focal individual to the focal point, since it counts for density
			if (receiver_subpop == exerter_subpop)
				total_strength += strength_for_zero_distance;
			
			// Divide by the corresponding clipped integral to get density
			total_strength /= clipped_integrals->FloatAtIndex(receiver_index, nullptr);
			result_vec->set_float_no_check(total_strength, receiver_index);
			
			FreeSparseVector(sv);
		}
		
		// deferred raises, for OpenMP compatibility
		if (saw_error_1)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_localPopulationDensity): localPopulationDensity() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
		if (saw_error_2)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_localPopulationDensity): localPopulationDensity() requires that all receivers be in the same subpopulation." << EidosTerminate();
		if (saw_error_3)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_localPopulationDensity): localPopulationDensity() tested a tag or tagL constraint, but a receiver's value for that property was not defined (had not been set)." << EidosTerminate();
		
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
	
	CheckSpeciesCompatibility_Receiver(receiver_species);
	
	InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
	double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
	
	// figure out the exerter subpopulation and get info on it
	bool exerters_value_NULL = (exerters_value->Type() == EidosValueType::kValueNULL);
	int exerters_count = exerters_value->Count();
	
	if ((exerters_count == 0) && !exerters_value_NULL)
		return gStaticEidosValue_Float_ZeroVec;
	
	Subpopulation *exerter_subpop = (exerters_value_NULL ? receiver_subpop : ((Individual *)exerters_value->ObjectElementAtIndex(0, nullptr))->subpopulation_);
	
	CheckSpeciesCompatibility_Exerter(exerter_subpop->species_);
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	slim_popsize_t exerter_subpop_size = exerter_subpop->parent_subpop_size_;
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	SLiM_kdNode *kd_root_EXERTERS = EnsureKDTreePresent_EXERTERS(exerter_subpop, exerter_subpop_data);
	
	// set up our return value
	if (exerters_value_NULL)
		exerters_count = exerter_subpop_size;
	
	EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerters_count);
	
	// Check constraints the receiver; if the individual is disqualified, the distance to each exerter is zero
	// The same applies if the k-d tree has no qualifying exerters; below this, we can assume the kd root is non-nullptr
	if (!CheckIndividualConstraints(receiver, receiver_constraints_) || !kd_root_EXERTERS)		// potentially raises
	{
		double *result_ptr = result_vec->data();
		
		for (int exerter_index = 0; exerter_index < exerter_subpop_size; ++exerter_index)
			*(result_ptr + exerter_index) = INFINITY;
		
		return EidosValue_SP(result_vec);
	}
	
	SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kDistances);
	FillSparseVectorForReceiverDistances(sv, receiver, receiver_position, exerter_subpop, kd_root_EXERTERS, /* constraints_active */ true);
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
		// FIXME we could just calculate the distance to each exerter directly, without the sparse array,
		// which would allow us to avoid this O(N^2) behavior
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

//	*********************	– (object)nearestInteractingNeighbors(object<Individual> receiver, [integer$ count = 1], [No<Subpopulation>$ exerterSubpop = NULL], [logical$ returnDict = F])
//
EidosValue_SP InteractionType::ExecuteMethod_nearestInteractingNeighbors(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// BCH 11/2/2023: Note that this method is now almost identical to ExecuteMethod_nearestNeighbors()
	EidosValue *receiver_value = p_arguments[0].get();
	EidosValue *count_value = p_arguments[1].get();
	EidosValue *exerterSubpop_value = p_arguments[2].get();
	EidosValue *returnDict_value = p_arguments[3].get();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): nearestInteractingNeighbors() requires that the interaction be spatial." << EidosTerminate();
	
	eidos_logical_t returnDict = returnDict_value->LogicalAtIndex(0, nullptr);
	Subpopulation *receiver_subpop = nullptr;
	
	if (!returnDict)
	{
		// This is the single-threaded, single-receiver case; it returns a vector of Individual objects
		if (receiver_value->Count() != 1)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): nearestInteractingNeighbors() requires that the receiver is singleton when returnDict is F; if you want to process multiple receivers in a single call, pass returnDict=T." << EidosTerminate();
		
		Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
		
		receiver_subpop = receiver->subpopulation_;
	}
	else
	{
		// This is the multi-threaded, multi-receiver case; it returns a Dictionary object with vectors of Individual objects
		if (receiver_value->Count() == 0)
		{
			// With no receivers, return an empty Dictionary
			EidosDictionaryRetained *dictionary = new EidosDictionaryRetained();
			EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(dictionary, gEidosDictionaryRetained_Class));
			
			dictionary->ContentsChanged("InteractionType::ExecuteMethod_nearestInteractingNeighbors()");
			
			// objectElement is now retained by result_SP, so we can release it
			dictionary->Release();
			
			return result_SP;
		}
		
		receiver_subpop = ((Individual *)receiver_value->ObjectElementAtIndex(0, nullptr))->subpopulation_;
	}
	
	// shared logic for both cases
	Species &receiver_species = receiver_subpop->species_;
	
	CheckSpeciesCompatibility_Receiver(receiver_species);
	
	InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
	
	// the exerter subpopulation defaults to the same subpop as the receivers
	Subpopulation *exerter_subpop = ((exerterSubpop_value->Type() == EidosValueType::kValueNULL) ? receiver_subpop : (Subpopulation *)exerterSubpop_value->ObjectElementAtIndex(0, nullptr));
	
	CheckSpeciesCompatibility_Exerter(exerter_subpop->species_);
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	// Check the count
	slim_popsize_t exerter_subpop_size = exerter_subpop->parent_subpop_size_;
	int64_t count = count_value->IntAtIndex(0, nullptr);
	
	if (count < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): nearestInteractingNeighbors() requires count >= 0." << EidosTerminate();
	
	if (count > exerter_subpop_size)
		count = exerter_subpop_size;
	
	if (!returnDict)
	{
		// This is the single-threaded, single-receiver case; it returns a vector of Individual objects
		if (count == 0)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
		// receiver_value is guaranteed to be singleton; let's get the info on it
		Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
		slim_popsize_t receiver_index_in_subpop = receiver->index_;
		
		if (receiver_index_in_subpop < 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): nearestInteractingNeighbors() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
		
		// Check constraints for the receiver; if the individual is disqualified, there are no interacting neighbors
		if (!CheckIndividualConstraints(receiver, receiver_constraints_))		// potentially raises
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		
		// Find the neighbors
		double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
		InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
		SLiM_kdNode *kd_root_EXERTERS = EnsureKDTreePresent_EXERTERS(exerter_subpop, exerter_subpop_data);
		
		EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		
		if (count < exerter_subpop_size)		// reserve only if we are finding fewer than every possible neighbor
			result_vec->reserve((int)count);
		
		FindNeighbors(exerter_subpop, kd_root_EXERTERS, exerter_subpop_data.kd_node_count_EXERTERS_, receiver_position, (int)count, *result_vec, receiver, /* constraints_active */ true);
		
		return EidosValue_SP(result_vec);
	}
	else
	{
		// This is the multi-threaded, multi-receiver case; it returns a Dictionary object vectors of Individual objects
		// We start by making a Dictionary with an empty Individual vector for each receiver
		EidosDictionaryRetained *dictionary = new EidosDictionaryRetained();
		EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(dictionary, gEidosDictionaryRetained_Class));
		int receivers_count = receiver_value->Count();
		EidosValue_Object_vector **result_vectors = (EidosValue_Object_vector **)malloc(receivers_count * sizeof(EidosValue_Object_vector *));
		
		for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
		{
			EidosValue_Object_vector *empty_individuals_vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class);
			
			dictionary->SetKeyValue_IntegerKeys(receiver_index, EidosValue_SP(empty_individuals_vec));
			result_vectors[receiver_index] = empty_individuals_vec;
		}
		
		dictionary->ContentsChanged("InteractionType::ExecuteMethod_nearestInteractingNeighbors()");
		
		// objectElement is now retained by result_SP, so we can release it
		dictionary->Release();
		
		if (count > 0)
		{
			bool saw_error_1 = false, saw_error_2 = false, saw_error_3 = false;
			
			InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
			SLiM_kdNode *kd_root_EXERTERS = EnsureKDTreePresent_EXERTERS(exerter_subpop, exerter_subpop_data);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_NEARESTINTNEIGH);
#pragma omp parallel for schedule(dynamic, 16) default(none) shared(receivers_count, receiver_subpop, exerter_subpop, receiver_subpop_data, exerter_subpop_data, kd_root_EXERTERS) firstprivate(receiver_value, result_vectors, count, exerter_subpop_size) reduction(||: saw_error_1) reduction(||: saw_error_2) reduction(||: saw_error_3) if(receivers_count >= EIDOS_OMPMIN_NEARESTINTNEIGH) num_threads(thread_count)
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(receiver_index, nullptr);
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
				{
					saw_error_1 = true;
					continue;
				}
				
				// SPECIES CONSISTENCY CHECK
				if (receiver_subpop != receiver->subpopulation_)
				{
					saw_error_2 = true;
					continue;
				}
				
				// Check constraints for the receiver; if the individual is disqualified, there are no interacting neighbors
				// Under OpenMP, raises can't go past the end of the parallel region; handle things the same way when not under OpenMP for simplicity
				try {
					if (!CheckIndividualConstraints(receiver, receiver_constraints_))		// potentially raises; protected
						continue;
				} catch (...) {
					saw_error_3 = true;
					continue;
				}
				
				double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				
				EidosValue_Object_vector *result_vec = result_vectors[receiver_index];
				
				if (count < exerter_subpop_size)		// reserve only if we are finding fewer than every possible neighbor
					result_vec->reserve((int)count);
				
				FindNeighbors(exerter_subpop, kd_root_EXERTERS, exerter_subpop_data.kd_node_count_EXERTERS_, receiver_position, (int)count, *result_vec, receiver, /* constraints_active */ true);
			}
			
			// deferred raises, for OpenMP compatibility
			if (saw_error_1)
			{
				free(result_vectors);
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): nearestInteractingNeighbors() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
			}
			if (saw_error_2)
			{
				free(result_vectors);
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): nearestInteractingNeighbors() requires that all receivers be in the same subpopulation." << EidosTerminate();
			}
			if (saw_error_3)
			{
				free(result_vectors);
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestInteractingNeighbors): nearestInteractingNeighbors() tested a tag or tagL constraint, but a receiver's value for that property was not defined (had not been set)." << EidosTerminate();
			}
		}
		
		free(result_vectors);
		return result_SP;
	}
}

//	*********************	– (object)nearestNeighbors(object<Individual> receiver, [integer$ count = 1], [No<Subpopulation>$ exerterSubpop = NULL], [logical$ returnDict = F])
//
EidosValue_SP InteractionType::ExecuteMethod_nearestNeighbors(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// BCH 11/2/2023: Note that this method is now almost identical to ExecuteMethod_nearestInteractingNeighbors()
	EidosValue *receiver_value = p_arguments[0].get();
	EidosValue *count_value = p_arguments[1].get();
	EidosValue *exerterSubpop_value = p_arguments[2].get();
	EidosValue *returnDict_value = p_arguments[3].get();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighbors): nearestNeighbors() requires that the interaction be spatial." << EidosTerminate();
	
	eidos_logical_t returnDict = returnDict_value->LogicalAtIndex(0, nullptr);
	Subpopulation *receiver_subpop = nullptr;
	
	if (!returnDict)
	{
		// This is the single-threaded, single-receiver case; it returns a vector of Individual objects
		if (receiver_value->Count() != 1)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighbors): nearestNeighbors() requires that the receiver is singleton when returnDict is F; if you want to process multiple receivers in a single call, pass returnDict=T." << EidosTerminate();
		
		Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
		
		receiver_subpop = receiver->subpopulation_;
	}
	else
	{
		// This is the multi-threaded, multi-receiver case; it returns a Dictionary object with vectors of Individual objects
		if (receiver_value->Count() == 0)
		{
			// With no receivers, return an empty Dictionary
			EidosDictionaryRetained *dictionary = new EidosDictionaryRetained();
			EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(dictionary, gEidosDictionaryRetained_Class));
			
			dictionary->ContentsChanged("InteractionType::ExecuteMethod_nearestNeighbors()");
			
			// objectElement is now retained by result_SP, so we can release it
			dictionary->Release();
			
			return result_SP;
		}
		
		receiver_subpop = ((Individual *)receiver_value->ObjectElementAtIndex(0, nullptr))->subpopulation_;
	}
	
	// shared logic for both cases
	Species &receiver_species = receiver_subpop->species_;
	
	CheckSpeciesCompatibility_Generic(receiver_species);
	
	InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
	
	// the exerter subpopulation defaults to the same subpop as the receivers
	Subpopulation *exerter_subpop = ((exerterSubpop_value->Type() == EidosValueType::kValueNULL) ? receiver_subpop : (Subpopulation *)exerterSubpop_value->ObjectElementAtIndex(0, nullptr));
	
	CheckSpeciesCompatibility_Generic(exerter_subpop->species_);
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	// Check the count
	slim_popsize_t exerter_subpop_size = exerter_subpop->parent_subpop_size_;
	int64_t count = count_value->IntAtIndex(0, nullptr);
	
	if (count < 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighbors): nearestNeighbors() requires count >= 0." << EidosTerminate();
	
	if (count > exerter_subpop_size)
		count = exerter_subpop_size;
	
	if (!returnDict)
	{
		// This is the single-threaded, single-receiver case; it returns a vector of Individual objects
		if (count == 0)
			return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		
		// receiver_value is guaranteed to be singleton; let's get the info on it
		Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(0, nullptr);
		slim_popsize_t receiver_index_in_subpop = receiver->index_;
		
		if (receiver_index_in_subpop < 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighbors): nearestNeighbors() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
		
		// Find the neighbors
		double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
		InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
		SLiM_kdNode *kd_root_ALL = EnsureKDTreePresent_ALL(exerter_subpop, exerter_subpop_data);
		
		EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
		
		if (count < exerter_subpop_size)		// reserve only if we are finding fewer than every possible neighbor
			result_vec->reserve((int)count);
		
		FindNeighbors(exerter_subpop, kd_root_ALL, exerter_subpop_data.kd_node_count_ALL_, receiver_position, (int)count, *result_vec, receiver, /* constraints_active */ false);
		
		return EidosValue_SP(result_vec);
	}
	else
	{
		// This is the multi-threaded, multi-receiver case; it returns a Dictionary object vectors of Individual objects
		// We start by making a Dictionary with an empty Individual vector for each receiver
		EidosDictionaryRetained *dictionary = new EidosDictionaryRetained();
		EidosValue_SP result_SP = EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_singleton(dictionary, gEidosDictionaryRetained_Class));
		int receivers_count = receiver_value->Count();
		EidosValue_Object_vector **result_vectors = (EidosValue_Object_vector **)malloc(receivers_count * sizeof(EidosValue_Object_vector *));
		
		for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
		{
			EidosValue_Object_vector *empty_individuals_vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class);
			
			dictionary->SetKeyValue_IntegerKeys(receiver_index, EidosValue_SP(empty_individuals_vec));
			result_vectors[receiver_index] = empty_individuals_vec;
		}
		
		dictionary->ContentsChanged("InteractionType::ExecuteMethod_nearestNeighbors()");
		
		// objectElement is now retained by result_SP, so we can release it
		dictionary->Release();
		
		if (count > 0)
		{
			bool saw_error_1 = false, saw_error_2 = false;
			
			InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
			SLiM_kdNode *kd_root_ALL = EnsureKDTreePresent_ALL(exerter_subpop, exerter_subpop_data);
			
			EIDOS_THREAD_COUNT(gEidos_OMP_threads_NEARESTNEIGH);
#pragma omp parallel for schedule(dynamic, 16) default(none) shared(receivers_count, receiver_subpop, exerter_subpop, receiver_subpop_data, exerter_subpop_data, kd_root_ALL) firstprivate(receiver_value, result_vectors, count, exerter_subpop_size) reduction(||: saw_error_1) reduction(||: saw_error_2) if(receivers_count >= EIDOS_OMPMIN_NEARESTNEIGH) num_threads(thread_count)
			for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
			{
				Individual *receiver = (Individual *)receiver_value->ObjectElementAtIndex(receiver_index, nullptr);
				slim_popsize_t receiver_index_in_subpop = receiver->index_;
				
				if (receiver_index_in_subpop < 0)
				{
					saw_error_1 = true;
					continue;
				}
				
				// SPECIES CONSISTENCY CHECK
				if (receiver_subpop != receiver->subpopulation_)
				{
					saw_error_2 = true;
					continue;
				}
				
				double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
				
				EidosValue_Object_vector *result_vec = result_vectors[receiver_index];
				
				if (count < exerter_subpop_size)		// reserve only if we are finding fewer than every possible neighbor
					result_vec->reserve((int)count);
				
				FindNeighbors(exerter_subpop, kd_root_ALL, exerter_subpop_data.kd_node_count_ALL_, receiver_position, (int)count, *result_vec, receiver, /* constraints_active */ false);
			}
			
			// deferred raises, for OpenMP compatibility
			if (saw_error_1)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighbors): nearestNeighbors() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
			if (saw_error_2)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_nearestNeighbors): nearestNeighbors() requires that all receivers be in the same subpopulation." << EidosTerminate();
		}
		
		free(result_vectors);
		return result_SP;
	}
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
	
	CheckSpeciesCompatibility_Generic(exerter_species);
	
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	SLiM_kdNode *kd_root_ALL = EnsureKDTreePresent_ALL(exerter_subpop, exerter_subpop_data);
	
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
	
	if (count > exerter_subpop_data.kd_node_count_ALL_)
		count = exerter_subpop_data.kd_node_count_ALL_;
	
	if (count == 0)
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	// Find the neighbors
	EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
	
	if (count < exerter_subpop_size)		// reserve only if we are finding fewer than every possible neighbor
		result_vec->reserve((int)count);
	
	FindNeighbors(exerter_subpop, kd_root_ALL, exerter_subpop_data.kd_node_count_ALL_, point_array, (int)count, *result_vec, nullptr, /* constraints_active */ false);
	
	return EidosValue_SP(result_vec);
}

//	*********************	– (integer)neighborCount(object<Individual> receivers, [No<Subpopulation>$ exerterSubpop = NULL])
//
EidosValue_SP InteractionType::ExecuteMethod_neighborCount(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	// BCH 11/2/2023: Note that this method is now almost identical to ExecuteMethod_interactingNeighborCount()
	EidosValue *receivers_value = p_arguments[0].get();
	EidosValue *exerterSubpop_value = p_arguments[1].get();
	int receivers_count = receivers_value->Count();
	
	if (spatiality_ == 0)
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCount): neighborCount() requires that the interaction be spatial." << EidosTerminate();
	
	if (receivers_count == 0)
		return gStaticEidosValue_Integer_ZeroVec;
	
	// the exerter subpopulation defaults to the same subpop as the receivers
	Subpopulation *receiver_subpop = ((Individual *)receivers_value->ObjectElementAtIndex(0, nullptr))->subpopulation_;
	Subpopulation *exerter_subpop = ((exerterSubpop_value->Type() == EidosValueType::kValueNULL) ? receiver_subpop : (Subpopulation *)exerterSubpop_value->ObjectElementAtIndex(0, nullptr));
	
	CheckSpeciesCompatibility_Generic(receiver_subpop->species_);
	CheckSpeciesCompatibility_Generic(exerter_subpop->species_);
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	SLiM_kdNode *kd_root_ALL = EnsureKDTreePresent_ALL(exerter_subpop, exerter_subpop_data);
	
	// If there are no individuals in the tree, short-circuit
	if (!kd_root_ALL)
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

	if (receivers_count == 1)
	{
		// Just one value, so we can return a singleton and skip some work
		Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(0, nullptr);
		slim_popsize_t receiver_index_in_subpop = receiver->index_;
		
		if (receiver_index_in_subpop < 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCount): neighborCount() requires that the receiver is visible in a subpopulation (i.e., not a new juvenile)." << EidosTerminate();
	
		// Find the neighbors
		double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
		slim_popsize_t focal_individual_index = (exerter_subpop == receiver_subpop) ? receiver_index_in_subpop : -1;
		int neighborCount;
		
		switch (spatiality_)
		{
			case 1: neighborCount = CountNeighbors_1(kd_root_ALL, receiver_position, focal_individual_index);			break;
			case 2: neighborCount = CountNeighbors_2(kd_root_ALL, receiver_position, focal_individual_index, 0);		break;
			case 3: neighborCount = CountNeighbors_3(kd_root_ALL, receiver_position, focal_individual_index, 0);		break;
			default: neighborCount = 0; break;	// unsupported value
		}
		
		return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(neighborCount));
	}
	else
	{
		EidosValue_Int_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Int_vector())->resize_no_initialize(receivers_count);
		bool saw_error_1 = false, saw_error_2 = false;
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_NEIGHCOUNT);
#pragma omp parallel for schedule(dynamic, 16) default(none) shared(receivers_count, receiver_subpop, exerter_subpop, receiver_subpop_data, exerter_subpop_data, kd_root_ALL) firstprivate(receivers_value, result_vec) reduction(||: saw_error_1) reduction(||: saw_error_2) if(receivers_count >= EIDOS_OMPMIN_NEIGHCOUNT) num_threads(thread_count)
		for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
		{
			Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(receiver_index, nullptr);
			slim_popsize_t receiver_index_in_subpop = receiver->index_;
			
			if (receiver_index_in_subpop < 0)
			{
				saw_error_1 = true;
				continue;
			}
			
			// SPECIES CONSISTENCY CHECK
			if (receiver_subpop != receiver->subpopulation_)
			{
				saw_error_2 = true;
				continue;
			}
			
			// Find the neighbors
			double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			slim_popsize_t focal_individual_index = (exerter_subpop == receiver_subpop) ? receiver_index_in_subpop : -1;
			int neighborCount;
			
			switch (spatiality_)
			{
				case 1: neighborCount = CountNeighbors_1(kd_root_ALL, receiver_position, focal_individual_index);			break;
				case 2: neighborCount = CountNeighbors_2(kd_root_ALL, receiver_position, focal_individual_index, 0);		break;
				case 3: neighborCount = CountNeighbors_3(kd_root_ALL, receiver_position, focal_individual_index, 0);		break;
				default: neighborCount = 0; break;	// unsupported value
			}
			
			result_vec->set_int_no_check(neighborCount, receiver_index);
		}
		
		// deferred raises, for OpenMP compatibility
		if (saw_error_1)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCount): neighborCount() requires receivers to be visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
		if (saw_error_2)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCount): neighborCount() requires that all receivers be in the same subpopulation." << EidosTerminate();
		
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
	
	CheckSpeciesCompatibility_Generic(exerter_species);
	
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	SLiM_kdNode *kd_root_ALL = EnsureKDTreePresent_ALL(exerter_subpop, exerter_subpop_data);

	if (!kd_root_ALL)
		return gStaticEidosValue_Integer0;
	
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
		case 1: neighborCount = CountNeighbors_1(kd_root_ALL, point_array, -1);			break;
		case 2: neighborCount = CountNeighbors_2(kd_root_ALL, point_array, -1, 0);		break;
		case 3: neighborCount = CountNeighbors_3(kd_root_ALL, point_array, -1, 0);		break;
		default: EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_neighborCountOfPoint): (internal error) unsupported spatiality" << EidosTerminate();
	}
	
	return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Int_singleton(neighborCount));
}

//	*********************	- (void)setConstraints(string$ who, [Ns$ sex = NULL], [Ni$ tag = NULL], [Ni$ minAge = NULL], [Ni$ maxAge = NULL], [Nl$ migrant = NULL],
//													[Nl$ tagL0 = NULL], [Nl$ tagL1 = NULL], [Nl$ tagL2 = NULL], [Nl$ tagL3 = NULL], [Nl$ tagL4 = NULL])
//
EidosValue_SP InteractionType::ExecuteMethod_setConstraints(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	if (AnyEvaluated())
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setConstraints): setConstraints() cannot be called while the interaction is being evaluated; call unevaluate() first, or call setConstraints() prior to evaluation of the interaction." << EidosTerminate();
	
	EidosValue *who_value = p_arguments[0].get();
	std::string who = who_value->StringAtIndex(0, nullptr);
	bool set_receiver_constraints = false;
	bool set_exerter_constraints = false;
	
	if (who == "receiver")
	{
		set_receiver_constraints = true;
	}
	else if (who == "exerter")
	{
		set_exerter_constraints = true;
	}
	else if (who == "both")
	{
		set_receiver_constraints = true;
		set_exerter_constraints = true;
	}
	else
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setConstraints): setConstraints() requires the parameter who to be one of 'receiver', 'exerter', or 'both'." << EidosTerminate();
	
	// do two passes, one for receiver constraints, one for exerter constraints
	for (int i = 0; i <= 1; ++i)
	{
		InteractionConstraints *constraints = nullptr;
		
		if (i == 0)
		{
			if (!set_receiver_constraints)
				continue;
			constraints = &receiver_constraints_;
		}
		if (i == 1)
		{
			if (!set_exerter_constraints)
				continue;
			constraints = &exerter_constraints_;
		}
		
		// first turn off all constraints
		constraints->has_constraints_ = false;
		constraints->sex_ = IndividualSex::kUnspecified;
		constraints->has_nonsex_constraints_ = false;
		constraints->tag_ = SLIM_TAG_UNSET_VALUE;
		constraints->min_age_ = -1;
		constraints->max_age_ = -1;
		constraints->migrant_ = -1;
		constraints->has_tagL_constraints_ = false;
		constraints->tagL0_ = -1;
		constraints->tagL1_ = -1;
		constraints->tagL2_ = -1;
		constraints->tagL3_ = -1;
		constraints->tagL4_ = -1;
		
		// then turn on constraints as requested
		EidosValue *sex_value = p_arguments[1].get();
		if (sex_value->Type() != EidosValueType::kValueNULL)
		{
			std::string sex = sex_value->StringAtIndex(0, nullptr);
			if (sex == "M")			{ constraints->sex_ = IndividualSex::kMale; constraints->has_constraints_ = true; }
			else if (sex == "F")	{ constraints->sex_ = IndividualSex::kFemale; constraints->has_constraints_ = true; }
			else if (sex == "*")	constraints->sex_ = IndividualSex::kUnspecified;
			else
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setConstraints): setConstraints() requires the parameter sex to be 'M', 'F', or '*'." << EidosTerminate();
		}
		
		EidosValue *tag_value = p_arguments[2].get();
		if (tag_value->Type() != EidosValueType::kValueNULL)
		{
			slim_usertag_t tag = tag_value->IntAtIndex(0, nullptr);
			
			constraints->tag_ = tag;
			constraints->has_constraints_ = true;
			constraints->has_nonsex_constraints_ = true;
		}
		
		EidosValue *minAge_value = p_arguments[3].get();
		if (minAge_value->Type() != EidosValueType::kValueNULL)
		{
			if (community_.ModelType() == SLiMModelType::kModelTypeWF)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setConstraints): setConstraints() cannot set a minAge constraint in a WF model (since the WF model is of non-overlapping generations)." << EidosTerminate();
			
			slim_age_t minAge = SLiMCastToAgeTypeOrRaise(minAge_value->IntAtIndex(0, nullptr));
			
			if ((minAge <= 0) || (minAge > 100000))
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setConstraints): setConstraints() requires the parameter minAge to be >= 0 and <= 100000." << EidosTerminate();
			
			constraints->min_age_ = minAge;
			constraints->has_constraints_ = true;
			constraints->has_nonsex_constraints_ = true;
		}
		
		EidosValue *maxAge_value = p_arguments[4].get();
		if (maxAge_value->Type() != EidosValueType::kValueNULL)
		{
			if (community_.ModelType() == SLiMModelType::kModelTypeWF)
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setConstraints): setConstraints() cannot set a maxAge constraint in a WF model (since the WF model is of non-overlapping generations)." << EidosTerminate();
			
			slim_age_t maxAge = SLiMCastToAgeTypeOrRaise(maxAge_value->IntAtIndex(0, nullptr));
			
			if ((maxAge <= 0) || (maxAge > 100000))
				EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setConstraints): setConstraints() requires the parameter maxAge to be >= 0 and <= 100000." << EidosTerminate();
			
			constraints->max_age_ = maxAge;
			constraints->has_constraints_ = true;
			constraints->has_nonsex_constraints_ = true;
		}
		
		if ((constraints->min_age_ != -1) && (constraints->max_age_ != -1) && (constraints->min_age_ > constraints->max_age_))
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setConstraints): setConstraints() requires minAge <= maxAge." << EidosTerminate();
		
		EidosValue *migrant_value = p_arguments[5].get();
		if (migrant_value->Type() != EidosValueType::kValueNULL)
		{
			eidos_logical_t migrant = migrant_value->LogicalAtIndex(0, nullptr);
			
			constraints->migrant_ = migrant;
			constraints->has_constraints_ = true;
			constraints->has_nonsex_constraints_ = true;
		}
		
		EidosValue *tagL0_value = p_arguments[6].get();
		if (tagL0_value->Type() != EidosValueType::kValueNULL)
		{
			eidos_logical_t tagL0 = tagL0_value->LogicalAtIndex(0, nullptr);
			
			constraints->tagL0_ = tagL0;
			constraints->has_constraints_ = true;
			constraints->has_nonsex_constraints_ = true;
			constraints->has_tagL_constraints_ = true;
		}
		
		EidosValue *tagL1_value = p_arguments[7].get();
		if (tagL1_value->Type() != EidosValueType::kValueNULL)
		{
			eidos_logical_t tagL1 = tagL1_value->LogicalAtIndex(0, nullptr);
			
			constraints->tagL1_ = tagL1;
			constraints->has_constraints_ = true;
			constraints->has_nonsex_constraints_ = true;
			constraints->has_tagL_constraints_ = true;
		}
		
		EidosValue *tagL2_value = p_arguments[8].get();
		if (tagL2_value->Type() != EidosValueType::kValueNULL)
		{
			eidos_logical_t tagL2 = tagL2_value->LogicalAtIndex(0, nullptr);
			
			constraints->tagL2_ = tagL2;
			constraints->has_constraints_ = true;
			constraints->has_nonsex_constraints_ = true;
			constraints->has_tagL_constraints_ = true;
		}
		
		EidosValue *tagL3_value = p_arguments[9].get();
		if (tagL3_value->Type() != EidosValueType::kValueNULL)
		{
			eidos_logical_t tagL3 = tagL3_value->LogicalAtIndex(0, nullptr);
			
			constraints->tagL3_ = tagL3;
			constraints->has_constraints_ = true;
			constraints->has_nonsex_constraints_ = true;
			constraints->has_tagL_constraints_ = true;
		}
		
		EidosValue *tagL4_value = p_arguments[10].get();
		if (tagL4_value->Type() != EidosValueType::kValueNULL)
		{
			eidos_logical_t tagL4 = tagL4_value->LogicalAtIndex(0, nullptr);
			
			constraints->tagL4_ = tagL4;
			constraints->has_constraints_ = true;
			constraints->has_nonsex_constraints_ = true;
			constraints->has_tagL_constraints_ = true;
		}
	}
	
	// mark that interaction types changed, so they get redisplayed in SLiMgui
	community_.interaction_types_changed_ = true;
	
	return gStaticEidosValueVOID;
}

//	*********************	- (void)setInteractionFunction(string$ functionType, ...)
//
EidosValue_SP InteractionType::ExecuteMethod_setInteractionFunction(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	
	if (AnyEvaluated())
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_setInteractionFunction): setInteractionFunction() cannot be called while the interaction is being evaluated; call unevaluate() first, or call setInteractionFunction() prior to evaluation of the interaction." << EidosTerminate();
	
	// SpatialKernel parses and bounds-checks our arguments for us
	SpatialKernel kernel(spatiality_, max_distance_, p_arguments, 0, /* p_expect_max_density */ true);
	
	// Everything seems to be in order, so replace our IF info with the new info
	// FIXME we could consider actually keeping an internal SpatialKernel instance permanently
	if_type_ = kernel.kernel_type_;
	if_param1_ = kernel.kernel_param1_;
	if_param2_ = kernel.kernel_param2_;
	if_param3_ = kernel.kernel_param3_;
	n_2param2sq_ = kernel.n_2param2sq_;
	
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
	
	CheckSpeciesCompatibility_Receiver(receiver_species);
	
	// figure out the exerter subpopulation and get info on it
	bool exerters_value_NULL = (exerters_value->Type() == EidosValueType::kValueNULL);
	int exerters_count = exerters_value->Count();
	
	if ((exerters_count == 0) && !exerters_value_NULL)
		return gStaticEidosValue_Float_ZeroVec;
	
	Subpopulation *exerter_subpop = (exerters_value_NULL ? receiver_subpop : ((Individual *)exerters_value->ObjectElementAtIndex(0, nullptr))->subpopulation_);
	
	CheckSpeciesCompatibility_Exerter(exerter_subpop->species_);
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	slim_popsize_t exerter_subpop_size = exerter_subpop->parent_subpop_size_;
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	
	// Check constraints for the receiver; if the individual is disqualified, the strength to each exerter is zero
	// The same applies if the k-d tree has no qualifying exerters; below this, we can assume the kd root is non-nullptr
	SLiM_kdNode *kd_root_EXERTERS = (spatiality_ ? EnsureKDTreePresent_EXERTERS(exerter_subpop, exerter_subpop_data) : nullptr);
	
	if (exerters_value_NULL)
		exerters_count = exerter_subpop_size;
	
	if (!CheckIndividualConstraints(receiver, receiver_constraints_) ||		// potentially raises
		(!kd_root_EXERTERS && spatiality_))
	{
		EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerters_count);
		
		EIDOS_BZERO(result_vec->data(), exerter_subpop_size * sizeof(double));
		
		return EidosValue_SP(result_vec);
	}
	
	if (spatiality_)
	{
		// Spatial case; we use the k-d tree to get strengths for all neighbors.
		// BCH 5/14/2023: The call to FillSparseVectorForReceiverStrengths() means we run interaction() callbacks,
		// so if this code is ever parallelized, it should stay single-threaded when callbacks are enabled.
		InteractionsData &receiver_subpop_data = InteractionsDataForSubpop(data_, receiver_subpop);
		double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
		
		SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kStrengths);
		EidosValue_Float_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Float_vector())->resize_no_initialize(exerters_count);
		EidosValue_SP result_SP(result_vec);
		
		try {
			FillSparseVectorForReceiverStrengths(sv, receiver, receiver_position, exerter_subpop, kd_root_EXERTERS, exerter_subpop_data.evaluation_interaction_callbacks_);
			uint32_t nnz;
			const uint32_t *columns;
			const sv_value_t *strengths;
			
			strengths = sv->Strengths(&nnz, &columns);
			
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
				// FIXME we could just calculate the strength to each exerter directly, without the sparse array,
				// which would allow us to avoid this O(N^2) behavior
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
		} catch (...) {
			InteractionType::FreeSparseVector(sv);
			throw;
		}
		
		InteractionType::FreeSparseVector(sv);
		return result_SP;
	}
	else
	{
		// Non-spatial case; no distances used.  We have to worry about exerter constraints; they are not handled for us.
		// BCH 5/14/2023: We call ApplyInteractionCallbacks() below, so if this code ever goes parallel it
		// should stay single-threaded if/when any interaction() callbacks are present!
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
					
					if (CheckIndividualConstraints(exerter, exerter_constraints_))		// potentially raises
						strength = ApplyInteractionCallbacks(receiver, exerter, if_param1_, NAN, callbacks);	// hard-coding interaction function "f" (SpatialKernelType::kFixed), which is required
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
				
				if ((exerter_index_in_subpop != receiver_index) && CheckIndividualConstraints(exerter, exerter_constraints_))		// potentially raises
					strength = ApplyInteractionCallbacks(receiver, exerter, if_param1_, NAN, callbacks);	// hard-coding interaction function "f" (SpatialKernelType::kFixed), which is required
				
				result_vec->set_float_no_check(strength, exerter_index);
			}
			
			return EidosValue_SP(result_vec);
		}
	}
}

//	*********************	– (lo<Individual>)testConstraints(object<Individual> individuals, string$ constraints, [logical$ returnIndividuals = F])
//
EidosValue_SP InteractionType::ExecuteMethod_testConstraints(EidosGlobalStringID p_method_id, const std::vector<EidosValue_SP> &p_arguments, EidosInterpreter &p_interpreter)
{
#pragma unused (p_method_id, p_arguments, p_interpreter)
	EidosValue *individuals_value = p_arguments[0].get();
	EidosValue_String *constraints_value = (EidosValue_String *)p_arguments[1].get();
	EidosValue *returnIndividuals_value = p_arguments[2].get();
	
	int individuals_count = individuals_value->Count();
	
	const std::string &constraints_str = constraints_value->StringRefAtIndex(0, nullptr);
	InteractionConstraints *constraints;
	
	if (constraints_str == "receiver")
		constraints = &receiver_constraints_;
	else if (constraints_str == "exerter")
		constraints = &exerter_constraints_;
	else
		EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_testConstraints): testConstraints() requires that parameter constraints be 'receiver' or 'exerter'." << EidosTerminate();
	
	bool returnIndividuals = returnIndividuals_value->LogicalAtIndex(0, nullptr);
	
	if (individuals_count == 1)
	{
		// singleton case
		Individual *ind = (Individual *)individuals_value->ObjectElementAtIndex(0, nullptr);
		
		if (CheckIndividualConstraints(ind, *constraints))
		{
			if (returnIndividuals)
				return p_arguments[0];	// return the individual as it was passed in
			else
				return gStaticEidosValue_LogicalT;
		}
		else
		{
			if (returnIndividuals)
				return EidosValue_SP(new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
			else
				return gStaticEidosValue_LogicalF;
		}
	}
	else
	{
		// non-singleton case
		Individual * const *inds = (Individual * const *)individuals_value->ObjectElementVector()->data();
		
		if (returnIndividuals)
		{
			EidosValue_Object_vector *result_vec = (new (gEidosValuePool->AllocateChunk()) EidosValue_Object_vector(gSLiM_Individual_Class));
			
			for (int index = 0; index < individuals_count; ++index)
			{
				Individual *ind = inds[index];
				
				if (CheckIndividualConstraints(ind, *constraints))
					result_vec->push_object_element_NORR(ind);
			}
			
			return EidosValue_SP(result_vec);
		}
		else
		{
			EidosValue_Logical *result_vec = new (gEidosValuePool->AllocateChunk()) EidosValue_Logical();
			result_vec->resize_no_initialize(individuals_count);
			
			for (int index = 0; index < individuals_count; ++index)
			{
				Individual *ind = inds[index];
				bool satisfied = CheckIndividualConstraints(ind, *constraints);
				
				result_vec->set_logical_no_check(satisfied, index);
			}
			
			return EidosValue_Logical_SP(result_vec);
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
	
	CheckSpeciesCompatibility_Receiver(receiver_subpop->species_);
	CheckSpeciesCompatibility_Exerter(exerter_subpop->species_);
	CheckSpatialCompatibility(receiver_subpop, exerter_subpop);
	
	InteractionsData &exerter_subpop_data = InteractionsDataForSubpop(data_, exerter_subpop);
	SLiM_kdNode *kd_root_EXERTERS = EnsureKDTreePresent_EXERTERS(exerter_subpop, exerter_subpop_data);
	
	// If there are no exerters satisfying constraints, short-circuit
	if (!kd_root_EXERTERS)
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
	
	if (receivers_count == 1)
	{
		// Just one value, so we can return a singleton and skip some work
		Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(0, nullptr);
		slim_popsize_t receiver_index_in_subpop = receiver->index_;
		
		if (receiver_index_in_subpop < 0)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): totalOfNeighborStrengths() requires that receivers are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
		
		// Check sex-specificity for the receiver; if the individual is disqualified, the total is zero
		if (!CheckIndividualConstraints(receiver, receiver_constraints_))		// potentially raises
			return gStaticEidosValue_Float0;
		
		double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
		SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kStrengths);
		
		try {
			FillSparseVectorForReceiverStrengths(sv, receiver, receiver_position, exerter_subpop, kd_root_EXERTERS, exerter_subpop_data.evaluation_interaction_callbacks_);		// singleton case, not parallel
		} catch (...) {
			InteractionType::FreeSparseVector(sv);
			throw;
		}
		
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
		EidosValue_SP result_SP(result_vec);
#ifdef _OPENMP
		bool has_interaction_callbacks = (exerter_subpop_data.evaluation_interaction_callbacks_.size() != 0);
#endif
		bool saw_error_1 = false, saw_error_2 = false, saw_error_3 = false, saw_error_4 = false;
		
		EIDOS_THREAD_COUNT(gEidos_OMP_threads_TOTNEIGHSTRENGTH);
#pragma omp parallel for schedule(dynamic, 16) default(none) shared(receivers_count, receiver_subpop, exerter_subpop, receiver_subpop_data, exerter_subpop_data, kd_root_EXERTERS) firstprivate(receivers_value, result_vec) reduction(||: saw_error_1) reduction(||: saw_error_2) reduction(||: saw_error_3) reduction(||: saw_error_4) if(!has_interaction_callbacks && (receivers_count >= EIDOS_OMPMIN_TOTNEIGHSTRENGTH)) num_threads(thread_count)
		for (int receiver_index = 0; receiver_index < receivers_count; ++receiver_index)
		{
			Individual *receiver = (Individual *)receivers_value->ObjectElementAtIndex(receiver_index, nullptr);
			slim_popsize_t receiver_index_in_subpop = receiver->index_;
			
			if (receiver_index_in_subpop < 0)
			{
				saw_error_1 = true;
				continue;
			}
			
			// SPECIES CONSISTENCY CHECK
			if (receiver_subpop != receiver->subpopulation_)
			{
				saw_error_2 = true;
				continue;
			}
			
			// Check constraints for the receiver; if the individual is disqualified, the total is zero
			// Under OpenMP, raises can't go past the end of the parallel region; handle things the same way when not under OpenMP for simplicity
			try {
				if (!CheckIndividualConstraints(receiver, receiver_constraints_))		// potentially raises; protected
				{
					result_vec->set_float_no_check(0, receiver_index);
					continue;
				}
			} catch (...) {
				saw_error_4 = true;
				continue;
			}
			
			double *receiver_position = receiver_subpop_data.positions_ + (size_t)receiver_index_in_subpop * SLIM_MAX_DIMENSIONALITY;
			SparseVector *sv = InteractionType::NewSparseVectorForExerterSubpop(exerter_subpop, SparseVectorDataType::kStrengths);
			
			// Under OpenMP, raises can't go past the end of the parallel region; handle things the same way when not under OpenMP for simplicity
			try {
				FillSparseVectorForReceiverStrengths(sv, receiver, receiver_position, exerter_subpop, kd_root_EXERTERS, exerter_subpop_data.evaluation_interaction_callbacks_);		// protected from running interaction() callbacks in parallel, above
			} catch (...) {
				saw_error_3 = true;
				InteractionType::FreeSparseVector(sv);
				continue;
			}
			
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
		
		// deferred raises, for OpenMP compatibility
		if (saw_error_1)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): totalOfNeighborStrengths() requires that receivers are visible in a subpopulation (i.e., not new juveniles)." << EidosTerminate();
		if (saw_error_2)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): totalOfNeighborStrengths() requires that all receivers be in the same subpopulation." << EidosTerminate();
		if (saw_error_3)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): an exception was caught inside a parallel region." << EidosTerminate();
		if (saw_error_4)
			EIDOS_TERMINATION << "ERROR (InteractionType::ExecuteMethod_totalOfNeighborStrengths): totalOfNeighborStrengths() tested a tag or tagL constraint, but a receiver's value for that property was not defined (had not been set)." << EidosTerminate();
		
		return result_SP;
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
		THREAD_SAFETY_IN_ANY_PARALLEL("InteractionType_Class::Properties(): not warmed up");
		
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
		THREAD_SAFETY_IN_ANY_PARALLEL("InteractionType_Class::Methods(): not warmed up");
		
		methods = new std::vector<EidosMethodSignature_CSP>(*super::Methods());
		
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_clippedIntegral, kEidosValueMaskFloat))->AddObject_N("receivers", gSLiM_Individual_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_distance, kEidosValueMaskFloat))->AddObject_S("receiver", gSLiM_Individual_Class)->AddObject_ON("exerters", gSLiM_Individual_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_distanceFromPoint, kEidosValueMaskFloat))->AddFloat("point")->AddObject("exerters", gSLiM_Individual_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_drawByStrength, kEidosValueMaskObject, nullptr))->AddObject("receiver", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddObject_OSN("exerterSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddLogical_OS("returnDict", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_evaluate, kEidosValueMaskVOID))->AddIntObject("subpops", gSLiM_Subpopulation_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_interactingNeighborCount, kEidosValueMaskInt))->AddObject("receivers", gSLiM_Individual_Class)->AddObject_OSN("exerterSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_localPopulationDensity, kEidosValueMaskFloat))->AddObject("receivers", gSLiM_Individual_Class)->AddObject_OSN("exerterSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_interactionDistance, kEidosValueMaskFloat))->AddObject_S("receiver", gSLiM_Individual_Class)->AddObject_ON("exerters", gSLiM_Individual_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_nearestInteractingNeighbors, kEidosValueMaskObject, nullptr))->AddObject("receiver", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddObject_OSN("exerterSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddLogical_OS("returnDict", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_nearestNeighbors, kEidosValueMaskObject, nullptr))->AddObject("receiver", gSLiM_Individual_Class)->AddInt_OS("count", gStaticEidosValue_Integer1)->AddObject_OSN("exerterSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL)->AddLogical_OS("returnDict", gStaticEidosValue_LogicalF));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_nearestNeighborsOfPoint, kEidosValueMaskObject, gSLiM_Individual_Class))->AddFloat("point")->AddIntObject_S("exerterSubpop", gSLiM_Subpopulation_Class)->AddInt_OS("count", gStaticEidosValue_Integer1));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_neighborCount, kEidosValueMaskInt))->AddObject("receivers", gSLiM_Individual_Class)->AddObject_OSN("exerterSubpop", gSLiM_Subpopulation_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_neighborCountOfPoint, kEidosValueMaskInt | kEidosValueMaskSingleton))->AddFloat("point")->AddIntObject_S("exerterSubpop", gSLiM_Subpopulation_Class));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setConstraints, kEidosValueMaskVOID))->AddString_S("who")->AddString_OSN("sex", gStaticEidosValueNULL)->AddInt_OSN("tag", gStaticEidosValueNULL)->AddInt_OSN("minAge", gStaticEidosValueNULL)->AddInt_OSN("maxAge", gStaticEidosValueNULL)->AddLogical_OSN("migrant", gStaticEidosValueNULL)->AddLogical_OSN("tagL0", gStaticEidosValueNULL)->AddLogical_OSN("tagL1", gStaticEidosValueNULL)->AddLogical_OSN("tagL2", gStaticEidosValueNULL)->AddLogical_OSN("tagL3", gStaticEidosValueNULL)->AddLogical_OSN("tagL4", gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_setInteractionFunction, kEidosValueMaskVOID))->AddString_S("functionType")->AddEllipsis());
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_strength, kEidosValueMaskFloat))->AddObject_S("receiver", gSLiM_Individual_Class)->AddObject_ON("exerters", gSLiM_Individual_Class, gStaticEidosValueNULL));
		methods->emplace_back((EidosInstanceMethodSignature *)(new EidosInstanceMethodSignature(gStr_testConstraints, kEidosValueMaskLogical | kEidosValueMaskObject, gSLiM_Individual_Class))->AddObject("individuals", gSLiM_Individual_Class)->AddString_S("constraints")->AddLogical_OS("returnIndividuals", gStaticEidosValue_LogicalF));
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

_InteractionsData::_InteractionsData(_InteractionsData&& p_source) noexcept
{
	evaluated_ = p_source.evaluated_;
	evaluation_interaction_callbacks_.swap(p_source.evaluation_interaction_callbacks_);
	individual_count_ = p_source.individual_count_;
	first_male_index_ = p_source.first_male_index_;
	periodic_x_ = p_source.periodic_x_;
	periodic_y_ = p_source.periodic_y_;
	periodic_z_ = p_source.periodic_z_;
	bounds_x1_ = p_source.bounds_x1_;
	bounds_y1_ = p_source.bounds_y1_;
	bounds_z1_ = p_source.bounds_z1_;
	positions_ = p_source.positions_;
	kd_nodes_ALL_ = p_source.kd_nodes_ALL_;
	kd_root_ALL_ = p_source.kd_root_ALL_;
	kd_node_count_ALL_ = p_source.kd_node_count_ALL_;
	kd_nodes_EXERTERS_ = p_source.kd_nodes_EXERTERS_;
	kd_root_EXERTERS_ = p_source.kd_root_EXERTERS_;
	kd_node_count_EXERTERS_ = p_source.kd_node_count_EXERTERS_;
	
	p_source.evaluated_ = false;
	p_source.evaluation_interaction_callbacks_.clear();
	p_source.individual_count_ = 0;
	p_source.first_male_index_ = 0;
	p_source.periodic_x_ = false;
	p_source.periodic_y_ = false;
	p_source.periodic_z_ = false;
	p_source.bounds_x1_ = 0.0;
	p_source.bounds_y1_ = 0.0;
	p_source.bounds_z1_ = 0.0;
	p_source.positions_ = nullptr;
	p_source.kd_nodes_ALL_ = nullptr;
	p_source.kd_root_ALL_ = nullptr;
	p_source.kd_node_count_ALL_ = 0;
	p_source.kd_nodes_EXERTERS_ = nullptr;
	p_source.kd_root_EXERTERS_ = nullptr;
	p_source.kd_node_count_EXERTERS_ = 0;
}

_InteractionsData& _InteractionsData::operator=(_InteractionsData&& p_source) noexcept
{
	if (this != &p_source)  
	{
		if (positions_)
			free(positions_);
		
		// keep in mind that the two k-d trees may share their memory
		if (kd_nodes_ALL_ == kd_nodes_EXERTERS_)
			kd_nodes_EXERTERS_ = nullptr;
		if (kd_nodes_ALL_)
			free(kd_nodes_ALL_);
		if (kd_nodes_EXERTERS_)
			free(kd_nodes_EXERTERS_);
		
		evaluated_ = p_source.evaluated_;
		evaluation_interaction_callbacks_.swap(p_source.evaluation_interaction_callbacks_);
		individual_count_ = p_source.individual_count_;
		first_male_index_ = p_source.first_male_index_;
		periodic_x_ = p_source.periodic_x_;
		periodic_y_ = p_source.periodic_y_;
		periodic_z_ = p_source.periodic_z_;
		bounds_x1_ = p_source.bounds_x1_;
		bounds_y1_ = p_source.bounds_y1_;
		bounds_z1_ = p_source.bounds_z1_;
		positions_ = p_source.positions_;
		kd_nodes_ALL_ = p_source.kd_nodes_ALL_;
		kd_root_ALL_ = p_source.kd_root_ALL_;
		kd_node_count_ALL_ = p_source.kd_node_count_ALL_;
		kd_nodes_EXERTERS_ = p_source.kd_nodes_EXERTERS_;
		kd_root_EXERTERS_ = p_source.kd_root_EXERTERS_;
		kd_node_count_EXERTERS_ = p_source.kd_node_count_EXERTERS_;
		
		p_source.evaluated_ = false;
		p_source.evaluation_interaction_callbacks_.clear();
		p_source.individual_count_ = 0;
		p_source.first_male_index_ = 0;
		p_source.periodic_x_ = false;
		p_source.periodic_y_ = false;
		p_source.periodic_z_ = false;
		p_source.bounds_x1_ = 0.0;
		p_source.bounds_y1_ = 0.0;
		p_source.bounds_z1_ = 0.0;
		p_source.positions_ = nullptr;
		p_source.kd_nodes_ALL_ = nullptr;
		p_source.kd_root_ALL_ = nullptr;
		p_source.kd_node_count_ALL_ = 0;
		p_source.kd_nodes_EXERTERS_ = nullptr;
		p_source.kd_root_EXERTERS_ = nullptr;
		p_source.kd_node_count_EXERTERS_ = 0;
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
	
	// keep in mind that the two k-d trees may share their memory
	if (kd_nodes_ALL_ == kd_nodes_EXERTERS_)
		kd_nodes_EXERTERS_ = nullptr;
	
	if (kd_nodes_ALL_)
	{
		free(kd_nodes_ALL_);
		kd_nodes_ALL_ = nullptr;
	}
	
	if (kd_nodes_EXERTERS_)
	{
		free(kd_nodes_EXERTERS_);
		kd_nodes_EXERTERS_ = nullptr;
	}
	
	kd_root_ALL_ = nullptr;
	kd_node_count_ALL_ = 0;
	
	kd_root_EXERTERS_ = nullptr;
	kd_node_count_EXERTERS_ = 0;
	
	// Unnecessary since it's about to be destroyed anyway
	//evaluation_interaction_callbacks_.clear();
}































